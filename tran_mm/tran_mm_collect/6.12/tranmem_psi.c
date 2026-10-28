/**
  *Copyright 2023 Transsion Inc
  *Author transsioner
  *tran_mm_collect feature suppport
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/psi_types.h>
#include <linux/psi.h>
#include <linux/sched/loadavg.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/psi.h>

//#include <tnek.h>

#include "tranmem_psi.h"
#include "tranmem_emem.h"

/*10s one shot, 6 time one min, 24h get (24*360) = 8640 datas*/
#define MAX_COLLECT_LENGTH     8640

/*Used to get psi_system's data*/
static struct psi_group* tranmem_psi_group = NULL;

DEFINE_MUTEX(tran_mm_psi_lock);
DEFINE_MUTEX(tran_mm_psi_limit_lock);

#ifdef CONFIG_TRAN_KERNEL_MM_FANS_SUPPORT
#define TNE_TAG_PSI        "0x0050100a"

#define FLAG_TNE_SYS_MEM               (1UL << 5)
#define FLAG_TNE_LOG                   (1UL << 9)
#define FLAG_TNE_DISPLAY               (1UL << 30)
#define FLAG_TNE_SYS_MEM_EXT           (1UL << 34)
#define FLAG_TNE_TOTAL                 (FLAG_TNE_SYS_MEM | FLAG_TNE_LOG | \
                                        FLAG_TNE_DISPLAY | FLAG_TNE_SYS_MEM_EXT)
#else
#define TNE_TAG_PSI        "0xffffff4b"

#define FLAG_TNE_USER_LOG_MAIN_SYS     (1UL << 1)
#define FLAG_TNE_USER_LOG_EVENT        (1UL << 2)
#define FLAG_TNE_USER_LOG_CRASH        (1UL << 4)
#define FLAG_TNE_USER_LOG_KERNEL       (1UL << 5)
#define FLAG_TNE_USER_PROC_MEMINFO     (1UL << 13)
#define FLAG_TNE_USER_ZONE_INFO        (1UL << 14)
#define FLAG_TNE_USER_BLOCKIO          (1UL << 15)
#define FLAG_TNE_USER_MMSTAT           (1UL << 16)
#define FLAG_TNE_USER_PROPERTY         (1UL << 18)
#define FLAG_TNE_USER_WINDOW           (1UL << 22)
#define FLAG_TNE_USER_SYS_MEM_EXTT     (1UL << 29)
#define FLAG_TNE_TOTAL                 (FLAG_TNE_USER_LOG_MAIN_SYS | FLAG_TNE_USER_LOG_EVENT | \
                                        FLAG_TNE_USER_LOG_CRASH | FLAG_TNE_USER_LOG_KERNEL | \
                                        FLAG_TNE_USER_PROC_MEMINFO | FLAG_TNE_USER_ZONE_INFO | \
                                        FLAG_TNE_USER_BLOCKIO | FLAG_TNE_USER_MMSTAT | \
                                        FLAG_TNE_USER_PROPERTY | FLAG_TNE_USER_WINDOW | \
                                        FLAG_TNE_USER_SYS_MEM_EXTT)
#endif



int tranmem_psi_limit_interval = 1800;    //default 30 mins

struct psi_data {
	short data[MAX_COLLECT_LENGTH];
	int curr;
};

typedef enum tranmem_psi_mem_type {
	TANRMEM_PSI_MEM_SOME,
	TRANMEM_PSI_MEM_FULL,
	NR_TRANMEM_PSI_MEM,
} tranmem_psi_mem_t;

static struct psi_data psi_mem_data[NR_TRANMEM_PSI_MEM];

static int psi_mem_limit_v;

//Flag indicates the hook has registed and probed successfully.
static bool psi_probed;

static void unregister_psi_monitor_vendor_hooks(void);
static int register_psi_monitor_vendor_hooks(void);

bool tranmem_psi_enabled(void)
{
	return tranmem_psi_group ? true : false;
}

static bool check_limit_available(void)
{
	struct timespec64 time;
	static u64 last_time = 0;

	ktime_get_real_ts64(&time);
	if (last_time != 0 && time.tv_sec - last_time < tranmem_psi_limit_interval) {
		return false;
	}

	last_time = time.tv_sec;

	return true;
}

static void check_psi_limit(const short current_psi)
{
	if (psi_mem_limit_v <= 0 || current_psi <= (psi_mem_limit_v * 100))
		return;

	if (!check_limit_available())
		return;

	pr_err(TRANMM "Exception for PSI limitation, curr:%d  <----> limit:%d\n", current_psi, psi_mem_limit_v);

	//trigger emem
	emem_trigger(1, __func__, EXCEPTION);

	//start TNE
//	tnek_caller_api(TNE_TAG_PSI, FLAG_TNE_TOTAL, 0, "");
}

static short add_psi_data(const tranmem_psi_mem_t type)
{
	unsigned long avg = 0;
	unsigned long load_avg = 0;
	struct psi_data* psi_mem = &psi_mem_data[type];

	if (type == TANRMEM_PSI_MEM_SOME) {
		avg = tranmem_psi_group->avg[PSI_MEM_SOME][0];
	} else {
		avg = tranmem_psi_group->avg[PSI_MEM_FULL][0];
	}

	load_avg = LOAD_INT(avg) * 100 + LOAD_FRAC(avg);
	psi_mem->data[psi_mem->curr] = (short)load_avg;

	psi_mem->curr++;
	if(psi_mem->curr >= MAX_COLLECT_LENGTH) {
		psi_mem->curr = 0;
	}

	return (short)load_avg;
}

static int add_psi_mem_data(void)
{
	short curr_psi_some = 0;

	mutex_lock(&tran_mm_psi_lock);

	tran_memory_info_kernel_trace_start(TRANMM"add_psi_mem_data");

	curr_psi_some = add_psi_data(TANRMEM_PSI_MEM_SOME);
	add_psi_data(TRANMEM_PSI_MEM_FULL);

	tran_memory_info_kernel_trace_end("");

	mutex_unlock(&tran_mm_psi_lock);

	check_psi_limit(curr_psi_some);

	return 0;
}

void update_psi_mem_data(void)
{
	if(!tranmem_psi_group) {
		pr_warn(TRANMM "tranmem_psi_group is null, wait hook.");
		return;
	}

	if(psi_probed) {
		unregister_psi_monitor_vendor_hooks();
		pr_err(TRANMM "unregister_psi_monitor_vendor_hooks");
		psi_probed = false;
	}

	add_psi_mem_data();
}

static void read_psi_data(struct seq_file *m, const tranmem_psi_mem_t type)
{
	int i = 0;
	struct psi_data* psi_mem = &psi_mem_data[type];

	if (psi_mem->data[MAX_COLLECT_LENGTH - 1] != -1) {      //has loop
		psi_mem->curr = MAX_COLLECT_LENGTH - 1;
	} else {
		psi_mem->curr -= 1;     //don't show the last -1
	}

	seq_printf(m, "%s", type == TANRMEM_PSI_MEM_SOME ? "some: " : "full: ");

	for(i = 0; i <= psi_mem->curr; i++) {
		seq_printf(m, "%d", psi_mem->data[i]);

		if(i != psi_mem->curr) {
			seq_printf(m, ",");
		}
	}
	seq_printf(m, "\n");

	memset(psi_mem, -1, sizeof(struct psi_data));
	psi_mem->curr = 0;
}

static int read_psi_mem_data(struct seq_file *m)
{
	if(!m || !tranmem_psi_group) {
		return -1;
	}

	mutex_lock(&tran_mm_psi_lock);

	tran_memory_info_kernel_trace_start(TRANMM"read_psi_mem_data");

	read_psi_data(m, TANRMEM_PSI_MEM_SOME);
	read_psi_data(m, TRANMEM_PSI_MEM_FULL);

	tran_memory_info_kernel_trace_end("");

	mutex_unlock(&tran_mm_psi_lock);

	return 0;
}

static int psi_mem_show(struct seq_file *m, void *v)
{
	read_psi_mem_data(m);
	return 0;
}

static int psi_mem_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, psi_mem_show, inode);
}

static const struct proc_ops psi_mem_fops = {
	.proc_open = psi_mem_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int psi_mem_limit_show(struct seq_file *m, void *v)
{
	mutex_lock(&tran_mm_psi_limit_lock);

	seq_printf(m, "%d", psi_mem_limit_v);

	mutex_unlock(&tran_mm_psi_limit_lock);
	return 0;
}

static int psi_mem_limit_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, psi_mem_limit_show, inode);
}

static ssize_t psi_mem_limit_write(struct file *file, const char __user *ubuf, size_t count, loff_t *pos)
{
	char buffer[4] = {0};
	int old_limit = 0;
	int ret = count;

	if (count < 0 || count >= 4) {
		pr_err(TRANMM" Length(%zu) of limit value for psi memory must be less than 4.\n", count);
		return -EINVAL;
	}

	mutex_lock(&tran_mm_psi_limit_lock);
	if (copy_from_user(buffer, ubuf, count)) {
		pr_err(TRANMM" psi_mem_limit set failed, try again.\n");
		ret = -EFAULT;
		goto out;
	}

	old_limit = psi_mem_limit_v;

	if (kstrtoint(buffer, 0, &psi_mem_limit_v) || psi_mem_limit_v >= 100) {
		pr_err(TRANMM" Invalid value for psi_mem_limit.\n");
		ret = -EINVAL;
		goto out;
	}

	pr_info(TRANMM" psi_mem_limit is set from %d to %d.\n", old_limit, psi_mem_limit_v);
out:
	mutex_unlock(&tran_mm_psi_limit_lock);
	return ret;
}

static const struct proc_ops psi_mem_limit_fops = {
    .proc_open = psi_mem_limit_open,
    .proc_read = seq_read,
    .proc_write = psi_mem_limit_write,
    .proc_release = single_release,
};

static void probe_android_vh_psi_group(void *unused, struct psi_group *group)
{
	if(tranmem_psi_group)
		return;

	tranmem_psi_group = group;
	psi_probed  = true;
}

static int register_psi_monitor_vendor_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_psi_group(probe_android_vh_psi_group, NULL);
	if (ret != 0) {
		pr_err(TRANMM"register psi_group vendor_hooks failed! ret=%d\n", ret);
	}

	return ret;
}

static void unregister_psi_monitor_vendor_hooks(void)
{
	unregister_trace_android_vh_psi_group(probe_android_vh_psi_group, NULL);
}

void tranmem_psi_proc_create(struct proc_dir_entry* parent)
{
	proc_create("psi_mem_info",     0444, parent, &psi_mem_fops);
	proc_create("psi_mem_limit",    0666, parent, &psi_mem_limit_fops);
}

void tranmem_psi_init(void)
{
	register_psi_monitor_vendor_hooks();

	memset(psi_mem_data, -1, sizeof(psi_mem_data));
	psi_mem_data[TANRMEM_PSI_MEM_SOME].curr = 0;
	psi_mem_data[TRANMEM_PSI_MEM_FULL].curr = 0;
}

void tranmem_psi_exit(void)
{
	unregister_psi_monitor_vendor_hooks();
}

