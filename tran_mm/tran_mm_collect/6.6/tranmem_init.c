/**
  *Copyright 2023 Transsion Inc
  *Author transsioner
  *tran_mm_collect feature suppport
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/sched/stat.h>
#include <linux/sched/types.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/cputime.h>
#include <linux/sched/loadavg.h>
#include <linux/sched/cputime.h>
#include <linux/sched/signal.h>
#include <linux/sched/types.h>
#include <uapi/linux/sched/types.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/sched/stat.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/cputime.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/time_namespace.h>
#include <linux/irqnr.h>
#include <linux/tick.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/signal.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sched/signal.h>
#include <linux/proc_fs.h>
#include <linux/printk.h>
#include <linux/kprobes.h>
#include <linux/sched/task.h>
#include <linux/sched/clock.h>

#include <kernel/sched/sched.h>
//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 start
#ifdef CONFIG_TASK_DELAY_ACCT
#include <linux/taskstats.h>
#include <linux/delayacct.h>
#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/mm.h>
#endif
//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 end

#include "tranmem_cloud.h"
#include "tranmem_emem.h"
#include "tranmem_psi.h"
#include "tranmem_task_struct.h"
#include "tranmem_direct_reclaim.h"

#define TRAN_MM_COLLECT "tran_mm_collect"
#define MAX_ARRAY_LENGTH 256

static struct task_struct *mmcollect = NULL;

// only when the time consumed by a single page fault is greater than this threshold(default value is 1000 us\ 1 ms), the page fault will be treated as an exception
static unsigned long long pg_exception_threshold = 1000000;
module_param_named(pg_exception_threshold, pg_exception_threshold, ullong, 0640);

//The execution cycle of kernel thread(tran_mm_collect), the default value is 10 seconds
#define TOPNUM 10

struct task_top {
	bool critical; // true if the process is critical, false otherwise
	short oom_score_adj;
	char comm[TASK_COMM_LEN];
	u64 min_flt;
	u64 maj_flt;
	pid_t pid;
	struct list_head list;
};

LIST_HEAD(g_task_top_list);
DEFINE_MUTEX(task_top_list_lock);

LIST_HEAD(tmp_task_top_list);
static int g_kernel_log_enable = 1;

static int g_thread_tgid = -1;
static bool thread_task_top_list_enable = false;
LIST_HEAD(thread_task_top_list);
static DEFINE_MUTEX(thread_mutex);

/* Whether to output the duration information of tran_mm_collect.*/
static bool tran_mm_collect_enable = true;

/* Whether to record the page fault information of all processes.*/
bool page_fault_acct_enable = true;
// Control switch for page fault time statistics. See enum emem_page_fault_time_type.
int page_fault_time_enable = PAGE_FAULT_TIME_DISABLE;

/* Whether to record the alloc page time of each order.*/
//static bool alloc_page_time_enable = false;

//The time of last page fault info calculation.
static ktime_t calculation_time;

//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 start
#ifdef CONFIG_TASK_DELAY_ACCT
struct tran_fg_delay_info{
	u64 blkio_delay;	/* wait for sync block io completion */
	u64 swapin_delay;	/* wait for swapin block io completion */
	u64 blkio_count;	/* total count of the number of sync block */
						/* io operations performed */
	u64 swapin_count;	/* total count of the number of swapin block */
						/* io operations performed */
	u64 freepages_delay;	/* wait for memory reclaim */
	u64 thrashing_delay;	/* wait for thrashing page */
	u64 freepages_count;	/* total count of memory reclaim */
	u64 thrashing_count;	/* total count of thrash waits */
};


/* Fixed me for concurrency*/
static struct tran_fg_delay_info g_fg_total;
static struct tran_fg_delay_info g_fg_start;

//record main proc id of top-app
static int g_fg_tgid = -1;
static u64 fg_start_timestamp = 0;
static DEFINE_MUTEX(fg_mutex);
//1, enable delayacct; 2, disable delayacct;
int delayacct_enable = 1;
#endif
//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 end

static struct proc_dir_entry *tran_proc_parent = NULL;

//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 start
#ifdef CONFIG_TASK_DELAY_ACCT
void foreground_delayacct_init(void)
{
	memset(&g_fg_total, 0, sizeof(g_fg_total));
	memset(&g_fg_start, 0, sizeof(g_fg_start));
}

int accumulate_child_threads_delayacct(pid_t tgid, u64 starttime, u64 *timestamp, struct tran_fg_delay_info *delayinfo)
{
	struct task_struct *tsk, *first;
	unsigned long flags;
	struct tran_fg_delay_info tmp;
	int ret = 0;

	memset(&tmp, 0, sizeof(tmp));

	/*
	 * Add additional stats from live tasks except zombie thread group
	 * leaders who are already counted with the dead tasks
	 */
	rcu_read_lock();
	first = find_task_by_vpid(tgid);

	//|| !lock_task_sighand(first, &flags)
	if ((!first ) ||(((0 < starttime) && (first->start_time > starttime)))) {
		ret = -1;
		goto out;
	}

	if (timestamp)
		*timestamp = ktime_get_ns();

	tsk = first;

	do {
		if (tsk->exit_state)
			continue;

		get_task_struct(tsk);
		/*
		 * Accounting subsystem can call its functions here to
		 * fill in relevant parts of struct taskstsats as follows
		 *
		 *	per-task-foo(stats, tsk);
		 */
		if (tsk->delays) {
			raw_spin_lock_irqsave(&tsk->delays->lock, flags);
			tmp.blkio_delay += tsk->delays->blkio_delay;
			tmp.swapin_delay += tsk->delays->swapin_delay;
			tmp.blkio_count += (u64)(tsk->delays->blkio_count);
			tmp.swapin_count += (u64)(tsk->delays->swapin_count);
			tmp.freepages_delay += tsk->delays->freepages_delay;
			tmp.thrashing_delay += tsk->delays->thrashing_delay;
			tmp.freepages_count += (u64)(tsk->delays->freepages_count);
			tmp.thrashing_count += (u64)(tsk->delays->thrashing_count);
			raw_spin_unlock_irqrestore(&tsk->delays->lock, flags);
		}
		put_task_struct(tsk);
	} while_each_thread(first, tsk);

	//unlock_task_sighand(first, &flags);
	memcpy(delayinfo, &tmp, sizeof(*delayinfo));
out:
	rcu_read_unlock();

	return ret;
}

int calculate_process_delayacct(pid_t tgid, u64 start, struct tran_fg_delay_info *start_delay)
{
	struct tran_fg_delay_info end_delay;
	u64 end_time;
	int ret = 0;

	ret = accumulate_child_threads_delayacct(tgid, start, &end_time, &end_delay);

	if (likely(0 == ret)) {
		g_fg_total.blkio_delay += end_delay.blkio_delay - start_delay->blkio_delay;
		g_fg_total.swapin_delay += end_delay.swapin_delay - start_delay->swapin_delay;
		g_fg_total.blkio_count += end_delay.blkio_count - start_delay->blkio_count;
		g_fg_total.swapin_count += end_delay.swapin_count - start_delay->swapin_count;
		g_fg_total.freepages_delay += end_delay.freepages_delay - start_delay->freepages_delay;
		g_fg_total.thrashing_delay += end_delay.thrashing_delay - start_delay->thrashing_delay;
		g_fg_total.freepages_count += end_delay.freepages_count - start_delay->freepages_count;
		g_fg_total.thrashing_count += end_delay.thrashing_count - start_delay->thrashing_count;
	}

	return ret;
}

static void probe_android_vh_exit_mm (void *ignore,struct mm_struct *mm)
{
	unsigned long flags;
	struct task_struct *group_leader = NULL;
	u64 blkio_delay_tmp = 0;
	u64 swapin_delay_tmp = 0;
	u64 freepages_delay_tmp = 0;
	u64 thrashing_delay_tmp = 0;
	u32 blkio_count_tmp = 0;
	u32 swapin_count_tmp = 0;
	u32 freepages_count_tmp = 0;
	u32 thrashing_count_tmp = 0;
	struct task_struct *tsk = current;

	if (1 != delayacct_enable)
		return;

	tran_memory_info_kernel_trace_start(TRANMM"delayacct_exit_mm");
	rcu_read_lock();

	if (unlikely(NULL == tsk)) {
		pr_info(TRANMM"Current Process is NULL\n");
	} else if (unlikely((!thread_task_top_list_enable) && (tsk->pid == tsk->tgid) && (tsk->pid == g_thread_tgid))) {
		pr_info(TRANMM"The process that was collecting page fault information for all child threads has exited:%d\n", g_thread_tgid);
		g_thread_tgid = -1;
	} else if (tsk->pid != tsk->tgid) {
		get_task_struct(tsk);

		if (likely(NULL != tsk->delays)) {
			raw_spin_lock_irqsave(&tsk->delays->lock, flags);
			blkio_delay_tmp = tsk->delays->blkio_delay;
			swapin_delay_tmp = tsk->delays->swapin_delay;
			freepages_delay_tmp = tsk->delays->blkio_count;
			thrashing_delay_tmp = tsk->delays->swapin_count;
			blkio_count_tmp = tsk->delays->freepages_delay;
			swapin_count_tmp = tsk->delays->thrashing_delay;
			freepages_count_tmp = tsk->delays->freepages_count;
			thrashing_count_tmp = tsk->delays->thrashing_count;
			raw_spin_unlock_irqrestore(&tsk->delays->lock, flags);
		}
		put_task_struct(tsk);

		group_leader = tsk->group_leader;
		if (likely(NULL != group_leader)) {
			get_task_struct(group_leader);
			if (NULL != group_leader->delays) {
				raw_spin_lock_irqsave(&group_leader->delays->lock, flags);
				group_leader->delays->blkio_delay += blkio_delay_tmp;
				group_leader->delays->swapin_delay += swapin_delay_tmp;
				group_leader->delays->blkio_count += blkio_count_tmp;
				group_leader->delays->swapin_count += swapin_count_tmp;
				group_leader->delays->freepages_delay += freepages_delay_tmp;
				group_leader->delays->thrashing_delay += thrashing_delay_tmp;
				group_leader->delays->freepages_count += freepages_count_tmp;
				group_leader->delays->thrashing_count += thrashing_count_tmp;
				raw_spin_unlock_irqrestore(&group_leader->delays->lock, flags);
			}
			put_task_struct(group_leader);
		}
	}

	rcu_read_unlock();
	tran_memory_info_kernel_trace_end(TRANMM"delayacct_exit_mm");
}

/*
 *	it may be inaccurate due to ignoring the CONFIG_NO_HZ_COMMON config
 */
static int fg_tgid_show(struct seq_file *m, void *v)
{
	struct tran_fg_delay_info tmp;

	if (1 != delayacct_enable) {
		seq_printf(m, "[1] delayacct is closed :%d, so skip reading foreground process delayacct information\n", delayacct_enable);
		return 0;
	}
	tran_memory_info_kernel_trace_start(TRANMM"delayacct_show");

	memset(&tmp, 0, sizeof(tmp));
	mutex_lock(&fg_mutex);
	accumulate_child_threads_delayacct(g_fg_tgid, fg_start_timestamp, NULL, &tmp);
	seq_printf(m, "current_foreground_pid:  %d\n", g_fg_tgid);
	seq_printf(m, "current_blkio_delay:     %llu\n", tmp.blkio_delay);
	seq_printf(m, "current_swapin_delay:    %llu\n", tmp.swapin_delay);
	seq_printf(m, "current_blkio_count:     %llu\n", tmp.blkio_count);
	seq_printf(m, "current_swapin_count:    %llu\n", tmp.swapin_count);
	seq_printf(m, "current_freepages_delay: %llu\n", tmp.freepages_delay);
	seq_printf(m, "current_thrashing_delay: %llu\n", tmp.thrashing_delay);
	seq_printf(m, "current_freepages_count: %llu\n", tmp.freepages_count);
	seq_printf(m, "current_thrashing_count: %llu\n", tmp.thrashing_count);

	seq_printf(m, "total_blkio_delay:       %llu\n", g_fg_total.blkio_delay);
	seq_printf(m, "total_swapin_delay:      %llu\n", g_fg_total.swapin_delay);
	seq_printf(m, "total_blkio_count:       %llu\n", g_fg_total.blkio_count);
	seq_printf(m, "total_swapin_count:      %llu\n", g_fg_total.swapin_count);
	seq_printf(m, "total_freepages_delay:   %llu\n", g_fg_total.freepages_delay);
	seq_printf(m, "total_thrashing_delay:   %llu\n", g_fg_total.thrashing_delay);
	seq_printf(m, "total_freepages_count:   %llu\n", g_fg_total.freepages_count);
	seq_printf(m, "total_thrashing_count:   %llu\n", g_fg_total.thrashing_count);
	mutex_unlock(&fg_mutex);

	tran_memory_info_kernel_trace_end(TRANMM"delayacct_show");
	return 0;
}

static ssize_t fg_tgid_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[10]={'\0'};
	int fg_pid = 0;
	int ret = 0;

	if (1 != delayacct_enable) {
		pr_info(TRANMM"delayacct is closed, so skip writing foreground tgid!\n");
		return count;
	}

	if (count >= 10) {
		pr_err(TRANMM"foreground process pid set failed for count exceeds limit(10)!\n");
		return count;
	}

	mutex_lock(&fg_mutex);

	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err(TRANMM"foreground process pid set failed, please try again!\n");
			return -EFAULT;
		}

		tran_memory_info_kernel_trace_start(TRANMM"delayacct_fg_tgid_write");

		sscanf(buf, "%d", &fg_pid);
		/* Fixed me for concurrency */
		if (0 < fg_pid) {
			if (fg_pid != g_fg_tgid) {
				/*old foreground process*/
				if ((0 < g_fg_tgid) && unlikely(0 > calculate_process_delayacct(g_fg_tgid, fg_start_timestamp, &g_fg_start))) {
					g_fg_tgid = -1;
					fg_start_timestamp = 0;
					memset(&g_fg_start, 0, sizeof(g_fg_start));
				}

				/*new foreground process*/
				ret = accumulate_child_threads_delayacct(fg_pid, fg_start_timestamp, &fg_start_timestamp, &g_fg_start);
				if (ret < 0) {
					g_fg_tgid = -1;
					fg_start_timestamp = 0;
					memset(&g_fg_start, 0, sizeof(g_fg_start));
				} else {
					g_fg_tgid = fg_pid;
				}
			} else {
				pr_err(TRANMM"Repeatedly setting the foreground process:%d\n", fg_pid);
			}
		} else {
			pr_err(TRANMM"foreground process pid invalid(%d)!\n", fg_pid);
		}

		tran_memory_info_kernel_trace_end(TRANMM"delayacct_fg_tgid_write");
	}
	mutex_unlock(&fg_mutex);
	pr_info(TRANMM"foreground process pid write succeed:%d\n", fg_pid);

	return count;
}

static int fg_tgid_open(struct inode *inode, struct file *file)
{
	return single_open(file, fg_tgid_show, NULL);
}

static const struct proc_ops fg_tgid_fops = {
    .proc_open = fg_tgid_open,
    .proc_read = seq_read,
    .proc_write = fg_tgid_write,
    .proc_release = single_release,
};
#endif
//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 end

static int destroy_task_top_list(struct list_head *task_top_list)
{
	struct task_top *node, *tmp;
	//free task_top_list
	list_for_each_entry_safe(node, tmp, task_top_list, list) {
		if (!list_empty(&node->list))
			list_del_init(&node->list);
		kfree(node);
	}

	return 0;
}

static int init_task_top_list(struct list_head *task_top_list)
{
	int i;
	int ret = 0;
	struct task_top *p_g = NULL;
	struct task_top *node, *tmp;

	for (i = 0; i < TOPNUM; i++) {
		//g_list for clound data sampling
		p_g = (struct task_top *)kzalloc(sizeof(struct task_top), GFP_KERNEL);
		if (!p_g) {
			pr_err(TRANMM" kmalloc p_g failed !!![%d]\n", i);
			ret = -ENOMEM;
			goto err;
		}

		INIT_LIST_HEAD(&p_g->list);
		if (!(task_top_list->next)) {
			pr_err(TRANMM" task_top_list->next  = NULL \n");
		}

		if (!(task_top_list->prev)) {
			pr_err(TRANMM" task_top_list->prev  = NULL \n");
		}
		list_add_tail(&p_g->list, task_top_list);
	}

	return ret;

err:
	//free task_top_list
	list_for_each_entry_safe(node, tmp, task_top_list, list) {
		if (!list_empty(&node->list))
			list_del_init(&node->list);
		kfree(node);
	}

	return ret;

}

static int task_top_sort(struct task_top *new_data, struct list_head *task_top_list)
{
	u64 new_pgfault = 0;
	struct task_top *p = NULL;
	struct task_top *tail = NULL;
	struct task_top *tmp = NULL;

	if (new_data == NULL) {
		pr_err(TRANMM" new_data == NULL \n");
		return -EFAULT;
	}

	new_pgfault = new_data->maj_flt + new_data->min_flt;
	if (new_pgfault <= 0) //the process does not do pgfault in recent 10s.
		return 0;

	tail = list_last_entry(task_top_list, struct task_top, list);

	list_for_each_entry_safe(p, tmp, task_top_list, list)
	{
		if (new_pgfault > (p->maj_flt + p->min_flt))
		{
			if (list_is_last(&p->list, task_top_list))
			{
				tail->maj_flt = new_data->maj_flt;
				tail->min_flt = new_data->min_flt;
				tail->pid = new_data->pid;
				tail->oom_score_adj = new_data->oom_score_adj;
				tail->critical = new_data->critical;
				strncpy(tail->comm, new_data->comm, TASK_COMM_LEN);
			} else {
				list_del(&tail->list);
				tail->maj_flt = new_data->maj_flt;
				tail->min_flt = new_data->min_flt;
				tail->pid = new_data->pid;
				tail->oom_score_adj = new_data->oom_score_adj;
				tail->critical = new_data->critical;
				strncpy(tail->comm, new_data->comm, TASK_COMM_LEN);

				list_add_tail(&tail->list, &p->list); //add tail(new_data) before node:p
			}
			break;
		}
	}

	return 0;
}

/*
 *	Get the page fault information of all processes.
 */
static int page_fault_info_show(struct seq_file *m, void *v)
{
	struct tranmem_task_struct *tranmem_ts;
	struct task_struct *task;
	s64 interval = TRAN_INTERVAL_SECOND;

#ifdef CONFIG_ANDROID_VENDOR_OEM_DATA
	if (!page_fault_acct_enable) {
		pr_debug(TRANMM"The machanism of recording page fault information of processes is not enabled.\n");
		seq_printf(m,"The machanism of recording page fault information of processes is not enabled.\n");
		return 0;
	}

	seq_printf(m,"ProcessName\t\tPid\t\tMin_Fault(total)\tMaj_Fault(total)\tMin_Fault(%lld)\tMaj_Fault(%lld)\n", interval, interval);
	seq_printf(m,"time:%llums ~ %llums\n", ktime_to_ms(calculation_time)-interval*1000, ktime_to_ms(calculation_time));

	rcu_read_lock();
	for_each_process(task) {
		if (!task || is_global_init(task) || (task->flags & PF_KTHREAD) || (task->pid != task->tgid))
			continue;
		get_task_struct(task);

		tranmem_ts = get_tranmem_task_struct(task);
		if (IS_ERR_OR_NULL(tranmem_ts)) {
			pr_err(TRANMM" tranmem_task_struct of %s is not inited!\n", task->comm);
			put_task_struct(task);
			continue;
		}
		seq_printf(m,"%-24s%-16llu%-16llu%-16llu%-16llu%-16llu\n",
		    task->comm, (u64)task->pid, tranmem_ts->min_flt_old, tranmem_ts->maj_flt_old, tranmem_ts->min_flt_delta, tranmem_ts->maj_flt_delta);
		put_task_struct(task);
	}
	rcu_read_unlock();
#else
	seq_printf(m,"CONFIG_ANDROID_VENDOR_OEM_DATA is not set, so the feature don't supprot.\n");
#endif
	return 0;
}

static int page_fault_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, page_fault_info_show, NULL);
}

static ssize_t page_fault_info_write(struct file *file, const char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	int val = -1;
	char buf[10]={'\0'};

	if (count >= 10)
		return count;

	if(count > 0) {
		if(copy_from_user(buf, ubuf, count)) {
			pr_err(TRANMM"get page fault information para failed, please try again!\n");
			return -EFAULT;
		}
		sscanf(buf, "%d", &val);
		switch (val) {
			case 0:
				page_fault_acct_enable = false;
				pr_info(TRANMM"Stop to record the page fault information of all processes\n");
				break;
			case 1:
				page_fault_acct_enable = true;
				pr_info(TRANMM"Start to record the page fault information of all processes\n");
				break;
			default:
				pr_err(TRANMM"get page fault information para illegal, please try again!\n");
				break;
		}
	}

	return count;
}

static const struct proc_ops page_fault_info_fops = {
    .proc_open = page_fault_info_open,
    .proc_read = seq_read,
    .proc_write = page_fault_info_write,
    .proc_release = single_release,
};

static int tran_mm_collect_enable_show(struct seq_file *m, void *v)
{
	seq_printf(m,"tran_mm_collect_enable is %s.\n", tran_mm_collect_enable?"true":"false");
	return 0;
}

static int tran_mm_collect_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_mm_collect_enable_show, NULL);
}

static ssize_t tran_mm_collect_enable_write(struct file *file, const char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	int val = -1;
	char buf[10]={'\0'};

	if (count >= 10)
		return count;

	if(count > 0) {
		if(copy_from_user(buf, ubuf, count)) {
			pr_err(TRANMM"get tran_mm_collect_enable information para failed, please try again!\n");
			return -EFAULT;
		}
		sscanf(buf, "%d", &val);
		switch (val) {
			case 0:
				tran_mm_collect_enable = false;
				pr_info(TRANMM"Stop to output the duration information of tran_mm_collect\n");
				break;
			case 1:
				tran_mm_collect_enable = true;
				pr_info(TRANMM"Start to output the duration information of tran_mm_collect\n");
				break;
			default:
				pr_err(TRANMM"get tran_mm_collect_enable para illegal, please try again!\n");
				break;
		}
	}

	return count;
}

static const struct proc_ops tran_mm_collect_enable_fops = {
    .proc_open = tran_mm_collect_enable_open,
    .proc_read = seq_read,
    .proc_write = tran_mm_collect_enable_write,
    .proc_release = single_release,
};

static int page_fault_info_top10_show(struct seq_file *m, void *v)
{
	struct task_top *p = NULL;
	struct task_top *tmp = NULL;
	int i = 0;
	s64 interval = TRAN_INTERVAL_SECOND;
#ifdef CONFIG_ANDROID_VENDOR_OEM_DATA
	if (!page_fault_acct_enable) {
		pr_debug(TRANMM"The machanism of recording page fault information of processes is not enabled.\n");
		seq_printf(m,"The machanism of recording page fault information of processes is not enabled.\n");
		return 0;
	}

	seq_printf(m,"ProcessName\t\tMin_Fault(%lld)\tMaj_Fault(%lld)\n", interval, interval);
	seq_printf(m,"time:%llums ~ %llums\n", ktime_to_ms(calculation_time)-TRAN_INTERVAL_SECOND*1000, ktime_to_ms(calculation_time));

	mutex_lock(&task_top_list_lock);
	tran_memory_info_kernel_trace_start(TRANMM"top10show");
	list_for_each_entry_safe(p, tmp, &g_task_top_list, list) {
		seq_printf(m,"%-24s %-16llu %-16llu [%d][%p]\n", p->comm, p->min_flt, p->maj_flt, i, &p->list);
		i++;
	}
	tran_memory_info_kernel_trace_end(TRANMM"top10show");
	mutex_unlock(&task_top_list_lock);

#else
	seq_printf(m,"CONFIG_ANDROID_VENDOR_OEM_DATA is not set, so the feature don't supprot.\n");
#endif
	return 0;
}

static int page_fault_info_top10_open(struct inode *inode, struct file *file)
{
	return single_open(file, page_fault_info_top10_show, NULL);
}

static const struct proc_ops page_fault_info_top10_fops = {
    .proc_open = page_fault_info_top10_open,
    .proc_read = seq_read,
    //.proc_write = page_fault_info_top10_write,
    .proc_release = single_release,
};

static int thread_top_page_fault_update(void)
{
	struct tranmem_task_struct *tranmem_ts = NULL;
	struct task_struct *tsk, *first;
	struct task_top task_top_data;
	int ret = -1;

	memset(&task_top_data, 0, sizeof(struct task_top));

	if (!page_fault_acct_enable || !thread_task_top_list_enable) {
		pr_debug(TRANMM"The machanism of recording page fault information of processes is not enabled.\n");
		return ret;
	}

	if (g_thread_tgid <= 0) {
		pr_debug(TRANMM"The pid value is invalid.\n");
		return ret;
	}

	/*
	 * Add additional stats from live tasks except zombie thread group
	 * leaders who are already counted with the dead tasks
	 */
	rcu_read_lock();
	first = find_task_by_vpid(g_thread_tgid);
	if (!first) {
		pr_debug(TRANMM"Can not find the process according to the set pid(%d) .\n", g_thread_tgid);
		goto out;
	}
	get_task_struct(first);
	tsk = first;

	do {
		if ((tsk == NULL) || (tsk->exit_state) || (tsk->pid == tsk->tgid))
			continue;
		get_task_struct(tsk);

		tranmem_ts = get_tranmem_task_struct(tsk);
		if (IS_ERR_OR_NULL(tranmem_ts)) {
			pr_err(TRANMM" thread_top_page_fault_update: tranmem_task_struct of %s is not inited!\n", tsk->comm);
			put_task_struct(tsk);
			continue;
		}
		task_top_data.maj_flt = tranmem_ts->maj_flt_delta;
		task_top_data.min_flt = tranmem_ts->min_flt_delta;
		task_top_data.pid = tsk->pid;
		// Mark critical process
		task_top_data.critical = is_critical(tsk);
		strncpy(task_top_data.comm, tsk->comm, TASK_COMM_LEN);
		task_top_sort(&task_top_data, &thread_task_top_list);
		put_task_struct(tsk);
	} while_each_thread(first, tsk);

	put_task_struct(first);
	ret = 0;

out:
	rcu_read_unlock();
	return ret;
}

static int thread_page_fault_info_show(struct seq_file *m, void *v)
{
	struct task_top *tmp = NULL;
	struct task_top *p = NULL;
	int i = 0;
	int ret = -1;
	s64 interval = TRAN_INTERVAL_SECOND;

#ifdef CONFIG_ANDROID_VENDOR_OEM_DATA
	mutex_lock(&thread_mutex);
	if (thread_task_top_list_enable) {
		list_for_each_entry_safe(p, tmp, &thread_task_top_list, list) {
			memset(p->comm, 0, TASK_COMM_LEN);
			p->pid = 0;
			p->oom_score_adj = -1;
			p->min_flt = 0;
			p->maj_flt = 0;
		}
	}
	ret = thread_top_page_fault_update();
	if (ret == 0) {
		tran_memory_info_kernel_trace_start(TRANMM"thread_page_fault_info_show");
		seq_printf(m, "%-18s %-10s %-10s %-10s %-10s\n", "ProcessName", "Pid", "Alloc_MB", "Min_Fault", "Maj_Fault ");
		seq_printf(m,"time:%llums ~ %llums\n", ktime_to_ms(calculation_time)-interval*1000, ktime_to_ms(calculation_time));
		list_for_each_entry_safe(p, tmp, &thread_task_top_list, list) {
			seq_printf(m, "%-18s %-10d %-10llu %-10llu %-10llu [%d]\n",
					p->comm, p->pid, (p->min_flt+p->maj_flt)>>8, p->min_flt, p->maj_flt, i++);
		}
		tran_memory_info_kernel_trace_end(TRANMM"thread_page_fault_info_show");
	}
	mutex_unlock(&thread_mutex);
#else
	ret = 0;
	seq_printf(m,"CONFIG_ANDROID_VENDOR_OEM_DATA is not set, so the feature don't supprot.\n");
#endif

	return ret;
}

static ssize_t thread_page_fault_info_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	struct task_struct *tsk;
	char buf[10]={'\0'};
	int thread_pid = 0;
	int ret = -EFAULT;

	if (!page_fault_acct_enable) {
		pr_debug(TRANMM"The machanism of recording page fault information of processes is not enabled.\n");
		return ret;
	}

	if (count >= 10) {
		pr_err(TRANMM"Process pid set failed for count exceeds limit(10)!\n");
		return ret;
	}


	mutex_lock(&thread_mutex);
	if(count > 0)
	{
		tran_memory_info_kernel_trace_start(TRANMM"thread_page_fault_info_write");
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err(TRANMM"Process pid set failed, please try again!\n");
			goto out;
		}
		sscanf(buf, "%d", &thread_pid);

		/* Fixed me for concurrency */
		if (0 < thread_pid) {
			if (!thread_task_top_list_enable) {
				if (init_task_top_list(&thread_task_top_list)) {
					pr_err(TRANMM" thread_task_top_list init failed. \n");
					goto out;
				}
				thread_task_top_list_enable = true;
			}
			if (thread_pid != g_thread_tgid) {
				rcu_read_lock();
				tsk = find_task_by_vpid(thread_pid);
				if (!tsk) {
					pr_err(TRANMM"Can not find the process according to the set pid(%d) .\n", thread_pid);
					rcu_read_unlock();
					goto out;
				}
				get_task_struct(tsk);
				if (tsk->pid == tsk->tgid) {
					g_thread_tgid = thread_pid;
					ret = count;
					pr_info(TRANMM"Process pid set succeed:%d\n", thread_pid);
				} else {
					pr_err(TRANMM" The set PID(%d) is not the main thread's PID.. \n", thread_pid);
				}
				put_task_struct(tsk);
				rcu_read_unlock();
			} else {
				pr_err(TRANMM"Repeatedly setting process:%d\n", thread_pid);
			}
		} else {
			if (thread_task_top_list_enable) {
				destroy_task_top_list(&thread_task_top_list);
				thread_task_top_list_enable = false;
				g_thread_tgid = -1;
				ret = count;
				pr_info(TRANMM"disable the thread page fault !\n");
			} else {
				pr_err(TRANMM"Repeatedly disable the thread page fault!\n");
			}
		}
out:
		tran_memory_info_kernel_trace_end(TRANMM"thread_page_fault_info_write");
	}

	mutex_unlock(&thread_mutex);
	return ret;
}

static int thread_page_fault_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, thread_page_fault_info_show, NULL);
}

static const struct proc_ops thread_page_fault_info_fops = {
    .proc_open = thread_page_fault_info_open,
    .proc_read = seq_read,
    .proc_write = thread_page_fault_info_write,
    .proc_release = single_release,
};

static int page_fault_sampling_show(struct seq_file *m, void *v)
{
	struct task_top *p = NULL;
	struct task_top *tmp = NULL;

	mutex_lock(&task_top_list_lock);
	list_for_each_entry_safe(p, tmp, &g_task_top_list, list) {
		seq_printf(m,"%d %llu %d %s\n", p->pid, (p->min_flt+p->maj_flt), p->oom_score_adj, p->comm);
	}
	mutex_unlock(&task_top_list_lock);
	pr_debug("[sampling] time:%llums(kernel_boot_time)\n", ktime_to_ms(ktime_get_boottime()));
	return 0;
}

static int page_fault_sampling_open(struct inode *inode, struct file *file)
{
	return single_open(file, page_fault_sampling_show, NULL);
}

static const struct proc_ops page_fault_sampling_fops = {
    .proc_open = page_fault_sampling_open,
    .proc_read = seq_read,
    //.proc_write = page_fault_sampling_write,
    .proc_release = single_release,
};

static int kernel_log_enable_show(struct seq_file *m, void *v)
{

	seq_printf(m, "kernel_log_enable: %d\n", g_kernel_log_enable);

	return 0;
}

static int kernel_log_enable_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, kernel_log_enable_show, inode);
}

static ssize_t kernel_log_enable_write(struct file *file, const char __user *buf,
							size_t count, loff_t *ppos)
{
	char buffer[64];
	int err = 0;

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user((void *)buffer, buf, count)) {
		err = -EFAULT;
		goto out;
	}

	sscanf(buffer, "%d", &g_kernel_log_enable);

out:
	return err < 0 ? err : count;
}

bool kernel_log_enable(void)
{
	return g_kernel_log_enable? true : false;
}

static const struct proc_ops kernel_log_enable_fops = {
    .proc_open = kernel_log_enable_open,
    .proc_read = seq_read,
    .proc_write = kernel_log_enable_write,
    .proc_release = single_release,
};

static int dram_size_show(struct seq_file *m, void *v)
{
	unsigned long physpages = get_num_physpages();
	seq_printf(m, "DramSize: %lu kB\n", physpages << (PAGE_SHIFT - 10));
	return 0;
}

static int dram_size_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dram_size_show, inode);
}

static const struct proc_ops dram_size_fops = {
	.proc_open = dram_size_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static atomic64_t critical_thread_pgfault_execution_time_total = ATOMIC64_INIT(0);  //ns
static atomic64_t critical_thread_pgfault_consume_time_total = ATOMIC64_INIT(0);  //ns
#define TRANMEM_PG_NUM_INTERVALS 15
static atomic64_t critical_thread_pgfault_execution_time_counts[TRANMEM_PG_NUM_INTERVALS];
static atomic64_t critical_thread_pgfault_consume_time_counts[TRANMEM_PG_NUM_INTERVALS];
static const char* PG_INTERVAL_STRINGS[TRANMEM_PG_NUM_INTERVALS] = {
    "[0ns,1ns]",
    "(1ns,10ns]",
    "(10ns,100ns]",
    "(100ns,1us]",
    "(1us,10us]",
    "(10us,100us]",
    "(100us,1ms]",
    "(1ms,2ms]",
    "(2ms,3ms]",
    "(3ms,5ms]",
    "(5ms,10ms]",
    "(10ms,15ms]",
    "(15ms,20ms]",
    "(20ms,30ms]",
    "(30ms,inf]",
};

static atomic64_t critical_thread_execution_exception_count = ATOMIC64_INIT(0);
static atomic64_t critical_thread_consume_exception_count = ATOMIC64_INIT(0);
static atomic64_t critical_thread_pgfault_count = ATOMIC64_INIT(0);
static atomic64_t normal_thread_execution_exception_count = ATOMIC64_INIT(0);
static atomic64_t normal_thread_consume_exception_count = ATOMIC64_INIT(0);
static atomic64_t normal_thread_pgfault_count = ATOMIC64_INIT(0);
static atomic64_t tranmem_task_struct_exception_count = ATOMIC64_INIT(0);

struct mmf_time_data {
	bool output;
    unsigned long long enter_exception_time;
	ktime_t enter_consume_time;
};

extern void update_critical_thread_time_distribution(atomic64_t *distribution, atomic64_t *total, s64 time_interval_ns);

static int pg_handler_pre(struct kretprobe_instance *ri, struct pt_regs *regs) {
	struct mmf_time_data *kprobe_data = NULL;

	if (IS_ERR_OR_NULL(ri) || IS_ERR_OR_NULL(ri->data)) {
		pr_err(TRANMM"pg_handler_pre: kretprobe_instance is IS_ERR_OR_NULL\n");
		return 0;
	}

	kprobe_data = (struct mmf_time_data *)ri->data;
	kprobe_data->enter_exception_time = task_sched_runtime(current);
	kprobe_data->enter_consume_time = ktime_get();

	if ((page_fault_time_enable >= PAGE_FAULT_TIME_INVALID) ||(page_fault_time_enable <= PAGE_FAULT_TIME_DISABLE) ||
		((page_fault_time_enable == PAGE_FAULT_TIME_ONLY_CRITICAL) && !is_critical(current)))
		kprobe_data->output = false;
	else
		kprobe_data->output = true;

	return 0;
}

static int pg_handler_post(struct kretprobe_instance *ri, struct pt_regs *regs) {
	unsigned long long end_exception_time;
	ktime_t end_consume_time;
	unsigned long long execution_ns;
	s64 consume_ns;
	struct mmf_time_data *kprobe_data = NULL;
	struct tranmem_task_struct *tranmem_ts = NULL;
	struct tranmem_task_struct *tgid_tranmem_ts = NULL;
    bool got_current = false;
    bool got_group_leader = false;

	if (IS_ERR_OR_NULL(ri) || IS_ERR_OR_NULL(ri->data)) {
		pr_err(TRANMM"pg_handler_post: kretprobe_instance is IS_ERR_OR_NULL\n");
		return 0;
	}

	kprobe_data = (struct mmf_time_data *)ri->data;
	if ((kprobe_data->output == false) && ((page_fault_time_enable >= PAGE_FAULT_TIME_INVALID) ||(page_fault_time_enable <= PAGE_FAULT_TIME_DISABLE) ||
		((page_fault_time_enable == PAGE_FAULT_TIME_ONLY_CRITICAL) && !is_critical(current))))
		return 0;

	end_exception_time = task_sched_runtime(current);
	end_consume_time = ktime_get();
	consume_ns = ktime_to_ns(ktime_sub(end_consume_time, kprobe_data->enter_consume_time));
	if ((consume_ns < 0) || (end_exception_time < kprobe_data->enter_exception_time)) {
		pr_err(TRANMM" pg_handler_post: The end time of page fault is earlier than the start time, ignore this page fault execution time!\n");
		return 0;
	}
	execution_ns = end_exception_time - kprobe_data->enter_exception_time;

	rcu_read_lock();
	get_task_struct(current);
	got_current = true;

	tranmem_ts = get_tranmem_task_struct(current);
	if (unlikely(IS_ERR_OR_NULL(tranmem_ts))) {
		pr_err(TRANMM" pg_handler_post: tranmem_task_struct of %s is not inited!\n", current->comm);
		atomic64_inc(&tranmem_task_struct_exception_count);
		tranmem_ts = NULL;
		goto out;
	}

	if ((current->pid != current->tgid) && (current->group_leader != NULL)) {
		get_task_struct(current->group_leader);
		got_group_leader = true;
		tgid_tranmem_ts = get_tranmem_task_struct(current->group_leader);
		if (unlikely(IS_ERR_OR_NULL(tgid_tranmem_ts))) {
			put_task_struct(current->group_leader);
			tgid_tranmem_ts = NULL;
			atomic64_inc(&tranmem_task_struct_exception_count);
			goto out;
		}
	}

	tranmem_ts->flt_execution_time += execution_ns;
	tranmem_ts->flt_consume_time += consume_ns;
	if (tgid_tranmem_ts) {
		tgid_tranmem_ts->flt_execution_time += execution_ns;
		tgid_tranmem_ts->flt_consume_time += consume_ns;
	}

	if (is_critical(current)) {
		atomic64_inc(&critical_thread_pgfault_count);
		update_critical_thread_time_distribution(critical_thread_pgfault_execution_time_counts,
				&critical_thread_pgfault_execution_time_total, execution_ns);
		update_critical_thread_time_distribution(critical_thread_pgfault_consume_time_counts,
				&critical_thread_pgfault_consume_time_total, consume_ns);
	} else {
		atomic64_inc(&normal_thread_pgfault_count);
	}

	if (execution_ns >= pg_exception_threshold) {
		tranmem_ts->flt_excep_count++;
		if (tgid_tranmem_ts)
			tgid_tranmem_ts->flt_excep_count++;

		if (is_critical(current))
			atomic64_inc(&critical_thread_execution_exception_count);
		else
			atomic64_inc(&normal_thread_execution_exception_count);
	}
	if (consume_ns >= pg_exception_threshold) {
		tranmem_ts->flt_concep_count++;
		if (tgid_tranmem_ts)
			tgid_tranmem_ts->flt_concep_count++;

		if (is_critical(current))
			atomic64_inc(&critical_thread_consume_exception_count);
		else
			atomic64_inc(&normal_thread_consume_exception_count);
	}

out:
	if (got_group_leader) {
		put_task_struct(current->group_leader);
		got_group_leader = false;
	}
	if (got_current) {
		put_task_struct(current);
		got_current = false;
	}
	rcu_read_unlock();

	return 0;
}

static struct kretprobe pagefault_trace_kp = {
    .kp.symbol_name = "handle_mm_fault",
    .entry_handler = pg_handler_pre,
    .handler = pg_handler_post,
    .data_size = sizeof(struct mmf_time_data),
    .maxactive = 128,
};

static int page_fault_time_show(struct seq_file *m, void *v)
{
	struct tranmem_task_struct *tranmem_ts;
	struct task_struct *task;

	seq_printf(m, "page_fault_time_enable: %d, valid value range is (%d, %d)\n", page_fault_time_enable, PAGE_FAULT_TIME_DISABLE, PAGE_FAULT_TIME_INVALID);

	if (page_fault_time_enable == PAGE_FAULT_TIME_DISABLE) {
		pr_debug(TRANMM"The machanism of page_fault_time is not enabled.\n");
		seq_printf(m,"The machanism of page_fault_time is not enabled.\n");
		return 0;
	}

	seq_printf(m,"%-10s%-16s%-6s%-16s%-16s%-16s%-16s%-16s\n", "Critical?", "ProcessName", "Pid", "PageFault(MB)", "ExeTotalTime(us)", "ConTotalTime(us)", "ExeExcTimes", "ConExcTimes");

	rcu_read_lock();
	for_each_process(task) {
		if (!task || is_global_init(task) || (task->flags & PF_KTHREAD) || (task->pid != task->tgid))
			continue;
		get_task_struct(task);

		tranmem_ts = get_tranmem_task_struct(task);
		if (IS_ERR_OR_NULL(tranmem_ts)) {
			pr_err(TRANMM" tranmem_task_struct of %s is not inited!\n", task->comm);
			put_task_struct(task);
			continue;
		}
		if ((page_fault_time_enable == PAGE_FAULT_TIME_ALL_PROCESS) ||
			((page_fault_time_enable == PAGE_FAULT_TIME_ONLY_CRITICAL) && is_critical(task))) {
			seq_printf(m,"%-10s%-16s%-6d%-16lu%-16llu%-16llu%-16u%-16u\n", is_critical(task)?"Critical":"Normal", task->comm, task->pid,
				PAGE_MB(tranmem_ts->min_flt_old + tranmem_ts->maj_flt_old), tranmem_ts->flt_execution_time/NSEC_PER_USEC,
				tranmem_ts->flt_consume_time/NSEC_PER_USEC, tranmem_ts->flt_excep_count, tranmem_ts->flt_concep_count);
		}
		put_task_struct(task);
	}
	rcu_read_unlock();

	return 0;
}

static int page_fault_time_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, page_fault_time_show, inode);
}

extern int register_alloc_page(void);
extern void unregister_alloc_page(void);
static DEFINE_MUTEX(page_fault_time_lock);
static ssize_t page_fault_time_write(struct file *file, const char __user *buf,
							size_t count, loff_t *ppos)
{
	char buffer[64];
	int err = -EFAULT;
	int value = -1;
	int i = 0;

	if (!page_fault_acct_enable) {
		pr_err(TRANMM" page_fault_time_enable enable fail, because this function depends on page_fault_info is enabled.\n");
		return err;
	}

	mutex_lock(&page_fault_time_lock);
	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user((void *)buffer, buf, count)) {
		pr_err(TRANMM" page_fault_time write fail, because copy_from_user.\n");
		goto out;
	}

	sscanf(buffer, "%d", &value);
	if ((value < PAGE_FAULT_TIME_INVALID) && (value > PAGE_FAULT_TIME_DISABLE)) {
		if (PAGE_FAULT_TIME_DISABLE == page_fault_time_enable) {
			err = register_kretprobe(&pagefault_trace_kp);
			if (err < 0) {
				pr_err(TRANMM"Failed to register kretprobe(do_page_fault): %d\n", err);
				goto out;
			}
			for (i = 0; i < TRANMEM_PG_NUM_INTERVALS; i++) {
				atomic64_set(&critical_thread_pgfault_execution_time_counts[i], 0);
				atomic64_set(&critical_thread_pgfault_consume_time_counts[i], 0);
			}
			err = register_alloc_page();
			if (err) {
				pr_err(TRANMM"Failed to register kretprobe(alloc_page): %d\n", err);
				// Release registered kretprobe resource when alloc_page register failed.
				unregister_kretprobe(&pagefault_trace_kp);
				goto out;
			}
			err = 0;
			pr_info(TRANMM"Enable page_fault_time_enable succeed.\n");
		} else if (page_fault_time_enable == value) {
			pr_err(TRANMM"Repeatly enable page_fault_time_enable.\n");
		}
		page_fault_time_enable = value;
	} else if (value == PAGE_FAULT_TIME_DISABLE) {
		if (page_fault_time_enable != PAGE_FAULT_TIME_DISABLE)  {
			unregister_kretprobe(&pagefault_trace_kp);
			unregister_alloc_page();
			pr_info(TRANMM"Disable page_fault_time_enable succeed.\n");
			err = 0;
			page_fault_time_enable = PAGE_FAULT_TIME_DISABLE;
		} else {
			pr_err(TRANMM"Repeatly disable page_fault_time_enable.\n");
		}
	} else {
		pr_err(TRANMM"Invalid parameter: %d.\n", value);
	}

out:
	mutex_unlock(&page_fault_time_lock);
	return err;
}

static const struct proc_ops page_fault_time_fops = {
    .proc_open =  page_fault_time_open,
    .proc_read = seq_read,
    .proc_write =  page_fault_time_write,
    .proc_release = single_release,
};

void tran_proc_init(void){
	tran_proc_parent = proc_mkdir(TRAN_MM_COLLECT, NULL);
	if (NULL == tran_proc_parent){
		pr_err(TRANMM"create tran_mm_collect proc file failed\n");
		return;
	}

	proc_create("tran_mm_collect_enable", S_IALLUGO, tran_proc_parent, &tran_mm_collect_enable_fops);
	proc_create("page_fault_info", S_IALLUGO, tran_proc_parent, &page_fault_info_fops);
	proc_create("page_fault_info_top10", S_IALLUGO, tran_proc_parent, &page_fault_info_top10_fops);
	proc_create("thread_page_fault_info", S_IALLUGO, tran_proc_parent, &thread_page_fault_info_fops);
	proc_create("page_fault_sampling", S_IALLUGO, tran_proc_parent, &page_fault_sampling_fops);
	proc_create("kernel_log_enable", S_IALLUGO, tran_proc_parent, &kernel_log_enable_fops);
	proc_create("page_fault_time", S_IALLUGO, tran_proc_parent, &page_fault_time_fops);
//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 start
#ifdef CONFIG_TASK_DELAY_ACCT
	proc_create("fg_tgid", S_IALLUGO, tran_proc_parent, &fg_tgid_fops);
#endif
//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 end
	proc_create("dram_size", S_IALLUGO, tran_proc_parent, &dram_size_fops);

	//Add for psi collector
	tranmem_psi_proc_create(tran_proc_parent);
	tranmem_direct_reclaim_proc_create(tran_proc_parent);
	return;
}

void tran_proc_exit(void){
	remove_proc_entry(TRAN_MM_COLLECT, NULL);
	return;
}

static struct sighand_struct *__lock_task_sighand_tranmm(struct task_struct *tsk,
					   unsigned long *flags)
{
	struct sighand_struct *sighand;

	rcu_read_lock();
	for (;;) {
		sighand = rcu_dereference(tsk->sighand);
		if (unlikely(sighand == NULL))
			break;

		/*
		 * This sighand can be already freed and even reused, but
		 * we rely on SLAB_TYPESAFE_BY_RCU and sighand_ctor() which
		 * initializes ->siglock: this slab can't go away, it has
		 * the same object type, ->siglock can't be reinitialized.
		 *
		 * We need to ensure that tsk->sighand is still the same
		 * after we take the lock, we can race with de_thread() or
		 * __exit_signal(). In the latter case the next iteration
		 * must see ->sighand == NULL.
		 */
		spin_lock_irqsave(&sighand->siglock, *flags);
		if (likely(sighand == rcu_access_pointer(tsk->sighand)))
			break;
		spin_unlock_irqrestore(&sighand->siglock, *flags);
	}
	rcu_read_unlock();

	return sighand;
}

struct sighand_struct *tranmm_lock_task_sighand(struct task_struct *task,
						       unsigned long *flags)
{
	struct sighand_struct *ret;

	ret = __lock_task_sighand_tranmm(task, flags);
	(void)__cond_lock(&task->sighand->siglock, ret);
	return ret;
}

void update_page_fault_time_and_show(void) {
	struct tranmem_task_struct *tranmem_ts;
	struct task_struct *task;
	struct task_struct *t;
	struct signal_struct *sig;
	struct tranmem_task_struct *tranmem_ts_thread;

	int i = 0;
	int j = 0;
	u64 min_flt_temp = 0, maj_flt_temp = 0;
	u64 min_flt_old = 0, maj_flt_old = 0;
	u64 calculate_status = 0;
	u64 kthread_exec_time = 0;
	u64 pgfault_delta = 0;
	u64 direct_reclaim_gap_time = 0;
	u64 direct_reclaim_count_gap = 0;
	u64 direct_reclaim_ratio = 0;
	short tsk_adj = -1;
	unsigned long flags;
	unsigned long long work_time = 0;
	unsigned long sum_pgfault = 0;
	struct task_top task_top_data, *p, *tmp, *g_entry;
	ktime_t start_time;
	start_time = ktime_get_boottime();
	tran_memory_info_kernel_trace_start(TRANMM"update_pgfault");
	kthread_exec_time = current->se.sum_exec_runtime;
	memset(&task_top_data, 0, sizeof(struct task_top));

	mutex_lock(&task_top_list_lock);
	tran_memory_info_kernel_trace_start(TRANMM"init");
	list_for_each_entry_safe(p, tmp, &g_task_top_list, list) {
		memset(p->comm, 0, TASK_COMM_LEN);
		p->pid = 0;
		p->oom_score_adj = -1;
		p->min_flt = 0;
		p->maj_flt = 0;
		p->critical = false;
	}

	list_for_each_entry_safe(p, tmp, &tmp_task_top_list, list) {
		memset(p->comm, 0, TASK_COMM_LEN);
		p->pid = 0;
		p->oom_score_adj = -1;
		p->min_flt = 0;
		p->maj_flt = 0;
		p->critical = false;
	}
	tran_memory_info_kernel_trace_end(TRANMM"init");
	mutex_unlock(&task_top_list_lock);

	if ((page_fault_time_enable > PAGE_FAULT_TIME_DISABLE) && (page_fault_time_enable < PAGE_FAULT_TIME_INVALID)) {
		pr_info(TRANMM"pg_time:%-16s%-16s%-16s\n", "TimeRange", "CriProPFExeTime", "CriProPFConsTime");
		for(j = 0; j < TRANMEM_PG_NUM_INTERVALS; j++){
			pr_info(TRANMM"pg_time:%-16s%-16llu%-16llu\n", PG_INTERVAL_STRINGS[j],
				(u64)atomic64_read(&critical_thread_pgfault_execution_time_counts[j]),
				(u64)atomic64_read(&critical_thread_pgfault_consume_time_counts[j]));
		}
		pr_info(TRANMM"pg_time:CriProcPFExeTimeTotal:%-16llu ms;CriProcPFConsTimeTotal:%-16llu ms\n",
			(u64)atomic64_read(&critical_thread_pgfault_execution_time_total)/USEC_PER_MSEC,
			(u64)atomic64_read(&critical_thread_pgfault_consume_time_total)/USEC_PER_MSEC);
		pr_info(TRANMM"pg_time:CriProcPFExeExcCount:%llu;CriProcPFConsExcCount:%llu;CriProcPFCount:%llu;NorProcPFExeExcCount:%llu;NorProcPFConsExcCount:%llu;NorProcPFCount:%llu;TranmemExcCount:%llu;\n",
			(u64)atomic64_read(&critical_thread_execution_exception_count), (u64)atomic64_read(&critical_thread_consume_exception_count), (u64)atomic64_read(&critical_thread_pgfault_count),
			(u64)atomic64_read(&normal_thread_execution_exception_count), (u64)atomic64_read(&normal_thread_consume_exception_count), (u64)atomic64_read(&normal_thread_pgfault_count),
			(u64)atomic64_read(&tranmem_task_struct_exception_count));
		pr_info(TRANMM"pg_time:%-10s%-16s%-6s%-6s%-10s%-10s%-8s%-8s%-6s%-10s%-10s\n",
			"Crit?", "ProcessName", "Pid", "PF(MB)", "ExeTime(us)", "ConsTime(us)",
			"EETimes","CETimes" ,"Adj","DRTime(ns)","DRRatio(%)");
	}

	rcu_read_lock();

	for_each_process(task) {
		if (!task || is_global_init(task) || (task->flags & PF_KTHREAD) || (task->pid != task->tgid))
			continue;

		get_task_struct(task);
		direct_reclaim_gap_time = 0;
		direct_reclaim_count_gap = 0;
		direct_reclaim_ratio = 0;
		min_flt_temp = 0;
		maj_flt_temp = 0;
		tsk_adj = -1;
		t = task;

		do {
			min_flt_temp += t->min_flt;
			maj_flt_temp += t->maj_flt;
			if (unlikely(thread_task_top_list_enable && (g_thread_tgid > 0) && (task->tgid == g_thread_tgid) && (t->pid != t->tgid))) {
				get_task_struct(t);
				tranmem_ts_thread = get_tranmem_task_struct(t);
				if (IS_ERR_OR_NULL(tranmem_ts_thread)) {
					pr_err(TRANMM" tranmem_task_struct of thread(%s) is not inited!\n", t->comm);
					put_task_struct(t);
					continue;
				}
				min_flt_old = tranmem_ts_thread->min_flt_old;
				maj_flt_old = tranmem_ts_thread->maj_flt_old;
				calculate_status = tranmem_ts_thread->calc_status;
				tranmem_ts_thread->min_flt_old = t->min_flt;
				tranmem_ts_thread->maj_flt_old = t->maj_flt;
				if (!calculate_status) {
					tranmem_ts_thread->calc_status = true;
				} else {
					tranmem_ts_thread->min_flt_delta = (u64) (t->min_flt - min_flt_old);
					tranmem_ts_thread->maj_flt_delta = (u64) (t->maj_flt - maj_flt_old);
				}
				put_task_struct(t);
			}
		} while_each_thread(task, t);
		//tran_memory_info_kernel_trace_change();
		if (tranmm_lock_task_sighand(task, &flags)) {
			sig = task->signal;

			if (likely(virt_addr_valid(sig))) {
				min_flt_temp += sig->min_flt;
				maj_flt_temp += sig->maj_flt;
			}
			tsk_adj = sig->oom_score_adj;
			unlock_task_sighand(task, &flags);
		}
		tranmem_ts = get_tranmem_task_struct(task);
		if (IS_ERR_OR_NULL(tranmem_ts)) {
			pr_err(TRANMM" tranmem_task_struct of %s is not inited!\n", task->comm);
			put_task_struct(task);
			continue;
		}
		direct_reclaim_gap_time = atomic64_xchg(&tranmem_ts->direct_reclaim_gap_time, 0);
		direct_reclaim_count_gap = atomic64_xchg(&tranmem_ts->direct_reclaim_count, 0);
		min_flt_old = tranmem_ts->min_flt_old;
		maj_flt_old = tranmem_ts->maj_flt_old;
		calculate_status = tranmem_ts->calc_status;
		tranmem_ts->min_flt_old = min_flt_temp;
		tranmem_ts->maj_flt_old = maj_flt_temp;
		if (!calculate_status) {
			tranmem_ts->calc_status = true;
		} else {
			tranmem_ts->min_flt_delta = (u64) (min_flt_temp - min_flt_old);
			tranmem_ts->maj_flt_delta = (u64) (maj_flt_temp - maj_flt_old);
		}

		task_top_data.min_flt = tranmem_ts->min_flt_delta;
		task_top_data.maj_flt = tranmem_ts->maj_flt_delta;
		task_top_data.pid = task->pid;
		task_top_data.oom_score_adj = tsk_adj;
		task_top_data.critical = is_critical(task);
		strncpy(task_top_data.comm, task->comm, TASK_COMM_LEN);

		if (((page_fault_time_enable == PAGE_FAULT_TIME_ALL_PROCESS) ||
			((page_fault_time_enable == PAGE_FAULT_TIME_ONLY_CRITICAL) && is_critical(task))) &&
			(tranmem_ts->flt_time >= tranmem_ts->flt_time_old)) {
			pgfault_delta = tranmem_ts->min_flt_delta + tranmem_ts->maj_flt_delta;
			if (is_critical(task)){
				atomic_long_add(pgfault_delta, &key_process_pgfault_total);
			}
			if (pgfault_delta != 0) {
				direct_reclaim_ratio = direct_reclaim_count_gap * FIXED_FACTOR * PERCENT_FACTOR/ pgfault_delta;
			}
			pr_info(TRANMM"pg_time:%-10s%-16s%-6d%-6lu%-10llu%-10llu%-8u%-8u%-6d%-10llu%llu.%03llu\n",
				is_critical(task)?"Critical":"Normal", task->comm, task->pid,
				PAGE_MB(tranmem_ts->min_flt_delta + tranmem_ts->maj_flt_delta),
				(tranmem_ts->flt_execution_time - tranmem_ts->flt_time_execution_old)/NSEC_PER_USEC,
				(tranmem_ts->flt_consume_time - tranmem_ts->flt_time_consume_old)/NSEC_PER_USEC,
				tranmem_ts->flt_excep_count, tranmem_ts->flt_concep_count, (int)tsk_adj, direct_reclaim_gap_time,
				INTEGER_PART_DIRECT_RECLAIM(direct_reclaim_ratio), FRACTIONAL_PART_DIRECT_RECLAIM(direct_reclaim_ratio));
			tranmem_ts->flt_time_execution_old = tranmem_ts->flt_execution_time;
			tranmem_ts->flt_time_consume_old = tranmem_ts->flt_consume_time;
		}else if (is_critical(task) && direct_reclaim_gap_time > 0 ) {
			pr_info(TRANMM"pg_time:%-10s%-16s%-6d%-6s%-10s%-10s%-8s%-8s%-6d%-10llu%-10s\n",
				is_critical(task)?"Critical":"Normal", task->comm, task->pid, "-", "-", "-",
				"-", "-" , (int)tsk_adj, direct_reclaim_gap_time, "-");
		}
		/*
		else if (direct_reclaim_gap_time > 0){
			pr_info(TRANMM"%-10s%-16s%-6d%-16s%-16s%-26s%-16s%-16d%-26llu%-16s\n", is_critical(task)?"Critical":"Normal", task->comm, task->pid,"-", "-", "-", "-" , (int)tsk_adj, direct_reclaim_gap_time, "-");
		}*/
		put_task_struct(task);

		sum_pgfault = sum_pgfault + task_top_data.min_flt + task_top_data.min_flt;

		task_top_sort(&task_top_data, &tmp_task_top_list);
	}

	rcu_read_unlock();

	if (g_kernel_log_enable && sum_pgfault > 3000) //~12 MB
	{
		pr_info(TRANMM"pg_number:%-10s%-18s %-10s %-10s %-10s %-10s %-10s\n", "Critical?", "ProcessName", "Pid", "Alloc_MB", "Min_Fault", "Maj_Fault ", "OOM_ADJ");
		mutex_lock(&task_top_list_lock);
		tran_memory_info_kernel_trace_start(TRANMM"log");
		//tmp_task_top_list copy
		g_entry = list_first_entry(&g_task_top_list, struct task_top, list);
		list_for_each_entry_safe(p, tmp, &tmp_task_top_list, list) {
			//pr_info(TRANMM"%-18s %-10d %-10llu %-10llu %-10llu [%d][%d]\n",
			//	p->comm, p->pid, (p->min_flt+p->maj_flt)>>8, p->min_flt, p->maj_flt, p->oom_score_adj, i++);
			g_entry->maj_flt = p->maj_flt;
			g_entry->min_flt = p->min_flt;
			g_entry->pid = p->pid;
			g_entry->oom_score_adj = p->oom_score_adj;
			g_entry->critical = p->critical;
			strncpy(g_entry->comm, p->comm, TASK_COMM_LEN);
			g_entry = list_next_entry(g_entry, list);
		}
		//copy end
		list_for_each_entry_safe(p, tmp, &g_task_top_list, list) {
			pr_info(TRANMM"pg_number:%-10s%-18s %-10d %-10llu %-10llu %-10llu [%d][%d]\n",
				p->critical?"Critical":"Normal", p->comm, p->pid, (p->min_flt+p->maj_flt)>>8, p->min_flt, p->maj_flt, p->oom_score_adj, i++);
		}
		tran_memory_info_kernel_trace_end(TRANMM"log");
		mutex_unlock(&task_top_list_lock);
	}
	calculation_time = ktime_get_boottime();
	work_time = ktime_ms_delta(calculation_time, start_time);
	if (g_kernel_log_enable)
		pr_info(TRANMM"notice work_time consumes[%llu]ms, sum_exec_time[%llu]\n", work_time, (current->se.sum_exec_runtime-kthread_exec_time)/NSEC_PER_MSEC);

	tran_memory_info_kernel_trace_end(TRANMM"update_pgfault");
}

void show_direct_reclaim_data(void)
{
	struct task_struct *task;
	struct tranmem_task_struct *tranmem_ts;
	u64 direct_reclaim_gap_time = 0;

	pr_info(TRANMM"%-16s%-6s%-26s\n", "ProcessName","Pid","Direct_Reclaim_Time(ns)");
	rcu_read_lock();

	for_each_process(task) {
		get_task_struct(task);
		if (!task || is_global_init(task) || (task->flags & PF_KTHREAD) || (task->pid != task->tgid)) {
			put_task_struct(task);
			continue;
		}
		direct_reclaim_gap_time = 0;

		tranmem_ts = get_tranmem_task_struct(task);
		if (IS_ERR_OR_NULL(tranmem_ts)) {
			pr_err(TRANMM" tranmem_task_struct of %s is not inited!\n", task->comm);
			put_task_struct(task);
			continue;
		}
		direct_reclaim_gap_time = atomic64_xchg(&tranmem_ts->direct_reclaim_gap_time, 0);
		/*
		pr_info(TRANMM"task is %d, task->group_leader is %d, time is %llu\n", task->pid, task->group_leader->pid, direct_reclaim_gap_time);
		pr_info(TRANMM"%-16s%-6d%-26llu\n", task->comm, task->pid, direct_reclaim_gap_time);
		if (direct_reclaim_gap_time > 0 ) {
			pr_info(TRANMM"%-16s%-6d%-26llu\n", task->comm, task->pid, direct_reclaim_gap_time);
		}*/
		if (is_critical(task) && direct_reclaim_gap_time > 0 ) {
			pr_info(TRANMM"%-16s%-6d%-26llu\n", task->comm, task->pid, direct_reclaim_gap_time);
		}

		put_task_struct(task);
	}
	rcu_read_unlock();

}

static int tran_mm_collect(void *p) {

	while(!kthread_should_stop()){
		msleep((TRAN_INTERVAL_SECOND*1000));

		if (!tran_mm_collect_enable)
			continue;

		if (tranmem_psi_enabled())
			update_psi_mem_data();

		update_direct_reclaim_data();

		if (page_fault_acct_enable)
			update_page_fault_time_and_show();
		else
			show_direct_reclaim_data();

		if (is_exception())
			emem_trigger(1, __func__, DURATION);
		else
			emem_trigger(300, __func__, DURATION);
	}

	return 0;
}

int tran_mmthread_init(void){
	int ret = 0;
	struct cpumask cls_cpus = {0x7}; //default bind cpu:0-2
	struct sched_param param = {  .sched_priority = SCHED_IDLE };

	mmcollect = kthread_run(tran_mm_collect, NULL, "tran_mm_collect");
	if (IS_ERR(mmcollect)) {
		ret = PTR_ERR(mmcollect);
		pr_err(TRANMM "Failed to create mmthread kernel thread\n");
		return ret;
	}
	// cls_num = arch_get_nr_clusters();
	// if (cls_num > 1)
	// {
	// 	arch_get_cluster_cpus(&cls_cpus, 0);
	// 	set_cpus_allowed_ptr(mmcollect, &cls_cpus);	//cpu allowed: little cluster
	// } else
	{
		set_cpus_allowed_ptr(mmcollect, &cls_cpus);
	}
	pr_info(TRANMM "set cpus = %*pbl\n", cpumask_pr_args(&cls_cpus));
	sched_setscheduler(mmcollect, SCHED_IDLE, &param);
	return ret;
}

static int __init tranmem_init(void)
{
	int ret = 0;

	ret = tranmem_task_struct_init();
	if (ret) {
		pr_err(TRANMM" tranmem_task_struct init failed!\n");
		return ret;
	}

	ret = init_task_top_list(&g_task_top_list);
	if (ret) {
		tranmem_task_struct_deinit();
		pr_err(TRANMM" g_task_top_list init failed(%d) \n", ret);
		return ret;
	}
	ret = init_task_top_list(&tmp_task_top_list);
	if (ret) {
		destroy_task_top_list(&g_task_top_list);
		tranmem_task_struct_deinit();
		pr_err(TRANMM" tmp_task_top_list init failed(%d) \n", ret);
		return ret;
	}
	if (ret == 0)
	{
		tranmem_cloudengine_init();
		tran_proc_init();
		tranmem_psi_init();
		tranmem_direct_reclaim_init();
#ifdef CONFIG_TASK_DELAY_ACCT
		foreground_delayacct_init();
#endif

		emem_init(tran_proc_parent);
		tran_mmthread_init();
//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 start
#ifdef CONFIG_TASK_DELAY_ACCT
		ret = register_trace_android_vh_exit_mm(probe_android_vh_exit_mm, NULL);
		if (ret) {
			delayacct_enable = 2;
			pr_err(TRANMM" vh_exit_mm register failed, the delayacct of process will not be correct, because the delayacct information of sub-thread will be lost!\n");
		}
#endif
//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 end
		if ((page_fault_time_enable > PAGE_FAULT_TIME_DISABLE) && (page_fault_time_enable < PAGE_FAULT_TIME_INVALID)) {
			ret = register_kretprobe(&pagefault_trace_kp);
			if (ret < 0) {
				pr_err(TRANMM"Failed to register kretprobe(do_page_fault): %d\n", ret);
			} else {
				ret = register_alloc_page();
				if (ret) {
					pr_err(TRANMM"Failed to register kretprobe(alloc_page): %d\n", ret);
					// Release registered kretprobe resource when alloc_page register failed.
					unregister_kretprobe(&pagefault_trace_kp);
				}
			}
			pr_info(TRANMM"Enable page_fault_time_enable succeed.\n");
		}
		pr_info(TRANMM" tran_mm_collect ko initialized!\n");
	} else {
		destroy_task_top_list(&tmp_task_top_list);
		destroy_task_top_list(&g_task_top_list);
		tranmem_task_struct_deinit();
		page_fault_acct_enable = false;
		pr_info(TRANMM" tran_mm_collect ko initialized failed !!!!\n");
	}

	return ret;
}

static void __exit tranmem_exit(void)
{
	if(mmcollect)
		kthread_stop(mmcollect);

	tranmem_direct_reclaim_exit();
	if (page_fault_acct_enable)
	{
		emem_exit();
		tranmem_psi_exit();
		tran_proc_exit();
		tranmem_cloudengine_exit();
		if ((page_fault_time_enable > PAGE_FAULT_TIME_DISABLE) && (page_fault_time_enable < PAGE_FAULT_TIME_INVALID))  {
			unregister_kretprobe(&pagefault_trace_kp);
			unregister_alloc_page();
			page_fault_time_enable = PAGE_FAULT_TIME_DISABLE;
		}
	}

	destroy_task_top_list(&tmp_task_top_list);
	mutex_lock(&task_top_list_lock);
	destroy_task_top_list(&g_task_top_list);
	mutex_unlock(&task_top_list_lock);

	tranmem_task_struct_deinit();
	return;
}

module_init(tranmem_init);
module_exit(tranmem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Transsion Inc");
MODULE_DESCRIPTION("tran mm Feature: data collection function!");
