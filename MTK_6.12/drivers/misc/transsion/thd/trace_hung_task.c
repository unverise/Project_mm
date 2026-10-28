/*Transsion Top Secret*/
/*
 * Copyright (C) 2023 Transsion Inc.
 *
 * @author qiwu.chen
 */
#define pr_fmt(fmt) "[hung_task]" fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/nmi.h>
#include <linux/printk.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/pid.h>
#include <linux/freezer.h>
#include <linux/sched/debug.h>
#include <trace/hooks/hung_task.h>
#ifdef CONFIG_ARM
#include <linux/cred.h>
#endif
#include "thd_util.h"
#include "aegisk.h"

/* because of KMI, define local val by module param */
static unsigned int hung_task_maxiowait_count = 5;
module_param(hung_task_maxiowait_count, uint, 0644);
MODULE_PARM_DESC(hung_task_maxiowait_count, "max iowait task count, default 5");

static unsigned int hung_task_maxdeath_count = 5;
module_param(hung_task_maxdeath_count, uint, 0644);
MODULE_PARM_DESC(hung_task_maxdeath_count, "max death task count, default 5");

static unsigned int hung_task_warnings = 10;
module_param(hung_task_warnings, uint, 0644);
MODULE_PARM_DESC(hung_task_warnings, "max warning log count, default 10");

/* Count the number of iowait and death tasks in one scan cycle */
static unsigned int death_count = 0, iowait_count = 0;

//add by ju.wei for cloudengine 202301122 start
//add cloud hung_taskstatic
static const char regID[] = "1003000013";
static const char enable_key[] = "cloud_hung_task_enable";
static bool is_cloud_engine_notifier_registed = false;
static bool hung_task_feature_enable = true;

static int hung_task_cloud_change(struct notifier_block *nb, unsigned long action, void* data)
{
	struct cloud_engine_config* cec;
	char* id;
	char* config;
	char* enable_str;
	static char* enable_str_old = "true";

	if(NULL == data)
		return -1;
	cec = (struct cloud_engine_config*)data;
	id = cec->id;
	config = cec->config;
	pr_info(" cloudengine recv id:%s, config:%s\n", id, config);

	if (strcmp(regID, id) == 0)
	{
		enable_str = cloudengine_getStringByID(regID,enable_key);
		if(enable_str == NULL ){
			hung_task_feature_enable = true;
			pr_info("enable_str is null\n");
			return 0;
		}else{
			if(enable_str_old != enable_str){
				enable_str_old = enable_str;
				pr_info("enable_str %s\n",enable_str);
				if(strcmp("false",enable_str) == 0){
					hung_task_feature_enable = false;
				}else{
					hung_task_feature_enable = true;
				}
			}
		}
		cloudengine_feed_back(regID, 1);
	}
	return 0;
}

struct notifier_block my_notifier_block = {
	.notifier_call = hung_task_cloud_change,
};

static int cloud_engine_hung_init(void)
{
	if(!is_cloud_engine_notifier_registed){
		char ver[]="v1.0";
		int ret = -1;
		ret = register_cloudengine_notifier(regID, ver, &my_notifier_block);
		if(ret == 0){
			is_cloud_engine_notifier_registed = true;
			pr_info(" cloud_hung_task_enable cloud control,cloud_engine_hung_init successfull");
		}
	}
	return 0;
}
//add by ju.wei for cloudengine 202301122 end

static void cloud_engine_hung_exit(void)
{
	int ret = -1;
	if(is_cloud_engine_notifier_registed){
		ret = unregister_cloudengine_notifier(regID,&my_notifier_block);
		if(ret == 0){
			pr_info(" cloud control,cloud_engine_hung_exit successfull");
			is_cloud_engine_notifier_registed = false;
		}
	}
}

static const char *const ignored_process[] = {
	"aee", /* Index must be the first */
	"wdtk",
	"resource_monito",
	"hang_detect"
};

static bool is_ignore_process(struct task_struct *t)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ignored_process); i++) {
		if (!strncmp(t->comm, ignored_process[i], strlen(ignored_process[i]))) {
			return true;
		}
	}
	return false;
}

static bool is_usersapce_key_process(struct task_struct *t)
{
	const struct cred *tcred = __task_cred(t);

	//only monitor user main thread
	if ((t->tgid == t->pid) && (!strncmp(t->comm, "system_server", TASK_COMM_LEN)
			|| !strncmp(t->comm, "surfaceflinger", TASK_COMM_LEN)
			|| !strncmp(t->comm, "SWWatchDog", TASK_COMM_LEN)
			|| !strncmp(t->comm, "vdc", TASK_COMM_LEN)
			|| !strncmp(t->comm, "composer@", strlen("composer@"))
			|| !strncmp(t->comm, "ndroid.systemui", TASK_COMM_LEN)
			|| is_global_init(t)))
		return true;

	//zygote
	if (!strcmp(t->comm, "main") && (tcred->uid.val == 0) && (t->parent && !strcmp(t->parent->comm, "init")))
		return true;

	//vold, vdc, keystore
	if (!strncmp(t->comm, "Binder:", 7) && (t->tgid == t->pid) && (tcred->uid.val == 0)
			&& (t->parent && !strcmp(t->parent->comm, "init")))
		return true;

	//system_server main thread
	if (!strncmp(t->comm, "Binder:", 7) && (t->tgid == t->pid) && (tcred->uid.val == 1000)
			&& (t->parent && !strcmp(t->parent->comm, "main")))
		return true;

	return false;
}

static bool check_hung_task_basic(struct task_struct *t, unsigned long timeout)
{
	unsigned long switch_count = t->nvcsw + t->nivcsw;

	/*
	 * Ensure the task is not frozen.
	 * Also, skip vfork and any other user process that freezer should skip.
	 */
	if (unlikely(READ_ONCE(t->__state) & TASK_FROZEN))
		return false;

	/*
	 * When a freshly created task is scheduled once, changes its state to
	 * TASK_UNINTERRUPTIBLE without having ever been switched out once, it
	 * musn't be checked.
	 */
	if (unlikely(!switch_count))
		return false;

	if (switch_count != t->last_switch_count) {
		t->last_switch_count = switch_count;
		t->last_switch_time = jiffies;
		return false;
	}

	if (time_is_after_jiffies(t->last_switch_time + timeout * HZ))
		return false;

	return true;
}

static void dump_hung_task_info(struct task_struct *t, unsigned iowait)
{
	/*
	 * Ok, the task did not get scheduled for more than 2 minutes,
	 * dump some useable task info and upload to tne service.
	 */
	if (hung_task_warnings) {
		if (hung_task_warnings > 0)
			hung_task_warnings--;
		dump_user_stacktrace(NULL, t, KERN_DEFAULT);
		sched_show_task(t);
		debug_show_held_locks(t);
#if IS_ENABLED(CONFIG_TRAN_FREEZE_AEGIS_SUPPORT)
		if (iowait){
			char pids[256] = {'\0'};
			snprintf(pids, sizeof(pids), "%d", t->pid);
			startAegisK("HUNG_TASK_IO_WAIT", pids, "FLAG_TNE_HUNG_TASK_IO_WAIT");
		}
		else{
			char pids[256] = {'\0'};
			snprintf(pids, sizeof(pids), "%d", t->pid);
			startAegisK("HUNG_TASK_NO_IO_WAIT", pids, "FLAG_TNE_HUNG_TASK_NON_IO_WAIT");
		}
#endif
	}
}

static void tran_check_hung_task(void *data, struct task_struct *t, unsigned long timeout, bool *need_check)
{
	unsigned long btime;
	unsigned int state;

	if (!is_usersapce_key_process(t))
		return;

	if (is_ignore_process(t) || !check_hung_task_basic(t, timeout)) {
		*need_check = false;
		return;
	}

	state = READ_ONCE(t->__state);
	/* check the hung task in state D/T/t */
	if (state == TASK_UNINTERRUPTIBLE || state == TASK_STOPPED || state == TASK_TRACED) {
		btime = (jiffies - t->last_switch_time) / HZ;
		if (t->in_iowait) {
			pr_info("task %s:%d blocked IO for more than %ld seconds, iowait_count:%d\n",
					t->comm, t->pid, btime, iowait_count);
			iowait_count++;
		} else {
			pr_info("task %s:%d blocked for more than %ld seconds in state:%c (flags:0x%x), death_count:%d\n",
				   t->comm, t->pid, btime, task_state_to_char(t), t->flags, death_count + 1);
			death_count++;
		}
		*need_check = false;
		dump_hung_task_info(t, t->in_iowait);
	}
}

static void tran_check_tasks_done(void *ignore, void *extra)
{
	// BSP add by ju.wei for cloudengine 202301122 start
	if(!hung_task_feature_enable){
		pr_info("hung_cloud value is false");
		return;
	}
	// BSP add by ju.wei for cloudengine 202301122 end

	/* Foreground background optimization, change max iowait count */
	if (iowait_count >= hung_task_maxiowait_count) {
		//trigger_all_cpu_backtrace();
		debug_show_all_locks();
		panic("hung_task: IO blocked tasks more than %u", iowait_count);
	}
	iowait_count = 0;

	/* Foreground background optimization, change max death task count */
	if (death_count >= hung_task_maxdeath_count) {
		//trigger_all_cpu_backtrace();
		debug_show_all_locks();
		panic("hung_task: blocked key tasks more than %u", death_count);
	}
	death_count = 0;
}

int __init trace_hung_task_init(void)
{
	int ret;

	pr_info("%s\n", __func__);
	ret = register_trace_android_vh_check_uninterruptible_tasks(tran_check_hung_task, NULL);
	if (ret) {
		pr_err("register_trace_android_vh_check_uninterruptible_tasks failed\n");
		return ret;
	}

	ret = register_trace_android_vh_check_uninterruptible_tasks_dn(tran_check_tasks_done, NULL);
	if (ret) {
		pr_err("register_trace_android_vh_check_uninterruptible_tasks_dn failed\n");
		unregister_trace_android_vh_check_uninterruptible_tasks(tran_check_hung_task, NULL);
		return ret;
	}

	// BSP add by ju.wei for cloudengine 202301122 start
	cloud_engine_hung_init();
	// BSP add by ju.wei for cloudengine 202301122 end

	return ret;
}

int __exit trace_hung_task_exit(void)
{
	pr_info("%s\n", __func__);
	unregister_trace_android_vh_check_uninterruptible_tasks(tran_check_hung_task, NULL);
	unregister_trace_android_vh_check_uninterruptible_tasks_dn(tran_check_tasks_done, NULL);
	cloud_engine_hung_exit();
	return 0;
}
