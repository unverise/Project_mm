/**
  *Copyright 2023 Transsion Inc
  *Author transsioner
  *tran_mm_collect feature suppport
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched/loadavg.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <trace/hooks/vendor_hooks.h>
#include <linux/mm.h>
#include <linux/vmstat.h>
#include <linux/mmzone.h>
#include <trace/hooks/mm.h>
#include <string.h>
#include <ctype.h>
#include <linux/string.h>

#include "tranmem_direct_reclaim.h"
#include "tranmem_emem.h"
#include "tranmem_task_struct.h"

//#define TRANMM "[tranmem_direct_reclaim]"



#ifdef CONFIG_TRAN_KERNEL_MM_FANS_SUPPORT
//#define TNE_TAG_DIRECT_RECLAIM            "0x0050100a" /////////////NEED!!!!!

#define FLAG_TNE_SYS_MEM               (1UL << 5)
#define FLAG_TNE_LOG                   (1UL << 9)
#define FLAG_TNE_DISPLAY               (1UL << 30)
#define FLAG_TNE_SYS_MEM_EXT           (1UL << 34)
#define FLAG_TNE_TOTAL                 (FLAG_TNE_SYS_MEM | FLAG_TNE_LOG | \
                                        FLAG_TNE_DISPLAY | FLAG_TNE_SYS_MEM_EXT)
#else
//#define TNE_TAG_DIRECT_RECLAIM            "0x0050100a" /////////////NEED!!!!!

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


struct vmstat_data {
	unsigned long allocstall_normal;
	unsigned long allocstall_movable;
	unsigned long pgfault;
	unsigned long pgsteal_direct;
};

#define TRAN_INTERVAL_SECOND 10 //10s
static unsigned long direct_reclaim_ratio;
static unsigned long direct_reclaim_efficiency;
static struct vmstat_data vmstat_data_curr, vmstat_data_last;
static atomic64_t direct_reclaim_time_gap;  //ns
static atomic64_t key_processes_direct_reclaim_time_total;  //ms
static atomic64_t key_processes_direct_reclaim_count_total;
static atomic64_t key_processes_alloc_pages_slowpath_time_total;  //ms
static atomic64_t key_processes_alloc_pages_slowpath_count_total;
#define MS_TO_NS 1000000
//(0-3],(3-5],(5-10],(10-15],(15-20],(20-30],(30,-]
#define NUM_INTERVALS 7
static atomic64_t key_processes_alloc_pages_slowpath_time_counts[NUM_INTERVALS] = {0};
const char* INTERVAL_STRINGS[NUM_INTERVALS] = {
    "[0ms,3ms]",
    "(3ms,5ms]",
    "(5ms,10ms]",
    "(10ms,15ms]",
    "(15ms,20ms]",
    "(20ms,30ms]",
    "(30ms,inf]",
};


//for Trancare
static atomic64_t direct_reclaim_time_total = ATOMIC64_INIT(0);	//ns
static atomic_long_t pgsteal_direct_total = ATOMIC_LONG_INIT(0);
static atomic_long_t allocstall_total = ATOMIC_LONG_INIT(0);
static atomic_long_t pgfault_total = ATOMIC_LONG_INIT(0);
atomic_long_t key_process_direct_reclaim_total = ATOMIC_LONG_INIT(0);
atomic_long_t key_process_pgfault_total = ATOMIC_LONG_INIT(0);
//limit value
static atomic_long_t direct_reclaim_efficiency_threshold = ATOMIC_LONG_INIT(0);
static atomic_long_t direct_reclaim_ratio_threshold = ATOMIC_LONG_INIT(0);
static atomic_long_t direct_reclaim_ratio_key_process_threshold = ATOMIC_LONG_INIT(0);
//over limit count
static atomic_long_t direct_reclaim_efficiency_over_limit_count = ATOMIC_LONG_INIT(0);
static atomic_long_t direct_reclaim_ratio_over_limit_count = ATOMIC_LONG_INIT(0);
static atomic_long_t direct_reclaim_ratio_key_process_over_limit_count = ATOMIC_LONG_INIT(0);

const char * const direct_reclaim_text[] = {
	"direct_reclaim_efficiency",
	"direct_reclaim_ratio",
	"key_process_direct_reclaim_ratio",
};



static void check_direct_reclaim_ratio_limit(unsigned long current_direct_reclaim_ratio)
{
	unsigned long direct_reclaim_ratio_limit = atomic_long_read(&direct_reclaim_ratio_threshold);
	pr_info(TRANMM "direct_reclaim_ratio_limit:%ld\n",direct_reclaim_ratio_limit);
	if (direct_reclaim_ratio_limit == 0 || current_direct_reclaim_ratio <= direct_reclaim_ratio_limit)
		return;

	atomic_long_inc(&direct_reclaim_ratio_over_limit_count);
	pr_err(TRANMM "over direct reclaim ratio limitation, curr:%ld.%03ld <----> limit:%lu.%03lu\n", INTEGER_PART_DIRECT_RECLAIM(current_direct_reclaim_ratio), FRACTIONAL_PART_DIRECT_RECLAIM(current_direct_reclaim_ratio), INTEGER_PART_DIRECT_RECLAIM(direct_reclaim_ratio_limit), FRACTIONAL_PART_DIRECT_RECLAIM(direct_reclaim_ratio_limit));
}


void check_direct_reclaim_ratio_key_process_limit(unsigned long current_direct_reclaim_ratio)
{
	unsigned long direct_reclaim_ratio_key_process_limit = atomic_long_read(&direct_reclaim_ratio_key_process_threshold);
	pr_info(TRANMM "direct_reclaim_ratio_limit:%ld\n",direct_reclaim_ratio_key_process_limit);
	if (direct_reclaim_ratio_key_process_limit == 0 || current_direct_reclaim_ratio <= direct_reclaim_ratio_key_process_limit)
		return;

	atomic_long_inc(&direct_reclaim_ratio_key_process_over_limit_count);
	pr_err(TRANMM "over direct reclaim ratio key process limitation, curr:%ld.%03ld <----> limit:%lu.%03lu\n", INTEGER_PART_DIRECT_RECLAIM(current_direct_reclaim_ratio), FRACTIONAL_PART_DIRECT_RECLAIM(current_direct_reclaim_ratio), INTEGER_PART_DIRECT_RECLAIM(direct_reclaim_ratio_key_process_limit), FRACTIONAL_PART_DIRECT_RECLAIM(direct_reclaim_ratio_key_process_limit));
}

static void check_direct_reclaim_efficiency_limit(unsigned long current_direct_reclaim_effciency)
{
	unsigned long direct_reclaim_efficiency_limit = atomic_long_read(&direct_reclaim_efficiency_threshold);
	pr_info(TRANMM "direct_reclaim_efficiency_limit:%ld\n",direct_reclaim_efficiency_limit);
	if (direct_reclaim_efficiency_limit == 0 || current_direct_reclaim_effciency <= direct_reclaim_efficiency_limit)
		return;

	atomic_long_inc(&direct_reclaim_efficiency_over_limit_count);
	pr_err(TRANMM "over direct reclaim efficiency limitation, curr:%ld.%03ld <----> limit:%lu.%03lu\n", INTEGER_PART_DIRECT_RECLAIM(current_direct_reclaim_effciency), FRACTIONAL_PART_DIRECT_RECLAIM(current_direct_reclaim_effciency), INTEGER_PART_DIRECT_RECLAIM(direct_reclaim_efficiency_limit), FRACTIONAL_PART_DIRECT_RECLAIM(direct_reclaim_efficiency_limit));

}


void update_direct_reclaim_data(void)
{
	static bool has_update = false;
	unsigned long ev[NR_VM_EVENT_ITEMS];
	long allocstall_normal_diff;
	long allocstall_movable_diff;
	long pgfault_diff;
	long pgsteal_direct_diff;
	u64 direct_reclaim_time = 0;

	//read_vmstat_data
	all_vm_events(ev);
	vmstat_data_curr.allocstall_normal = ev[ALLOCSTALL_NORMAL];
	vmstat_data_curr.allocstall_movable = ev[ALLOCSTALL_MOVABLE];
	vmstat_data_curr.pgfault = ev[PGFAULT];
	vmstat_data_curr.pgsteal_direct = ev[PGSTEAL_DIRECT];

	if (!has_update){
		vmstat_data_last = vmstat_data_curr;
		has_update = true;
		return;
	}
	//add direct_reclaim_ratio
	allocstall_normal_diff = vmstat_data_curr.allocstall_normal - vmstat_data_last.allocstall_normal;
	allocstall_movable_diff = vmstat_data_curr.allocstall_movable - vmstat_data_last.allocstall_movable;
	pgfault_diff = vmstat_data_curr.pgfault - vmstat_data_last.pgfault;
	if(allocstall_normal_diff < 0 || allocstall_movable_diff < 0 || pgfault_diff < 0){
		pr_err(TRANMM "Invalid direct reclaim data, allocstall_normal_diff:%ld, allocstall_movable_diff:%ld, pgfault_diff:%ld\n", allocstall_normal_diff, allocstall_movable_diff, pgfault_diff);
		direct_reclaim_ratio = 0;
	} else if (pgfault_diff == 0){
		direct_reclaim_ratio = 0;
	} else {
		direct_reclaim_ratio = ((allocstall_normal_diff + allocstall_movable_diff) * PERCENT_FACTOR * FIXED_FACTOR) / pgfault_diff;
	}
	pr_info(TRANMM" allocstall_normal + allocstall_movable_diff is %ld, pgfault_diff is %ld\n", allocstall_normal_diff + allocstall_movable_diff, pgfault_diff);
	pr_info(TRANMM" direct_reclaim_ratio is %ld.%03ld%%.\n", INTEGER_PART_DIRECT_RECLAIM(direct_reclaim_ratio),FRACTIONAL_PART_DIRECT_RECLAIM(direct_reclaim_ratio));
	check_direct_reclaim_ratio_limit(direct_reclaim_ratio);
	atomic_long_add(allocstall_normal_diff + allocstall_movable_diff, &allocstall_total);
	atomic_long_add(pgfault_diff, &pgfault_total);

	//add direct_reclaim_efficiency
	direct_reclaim_time = atomic64_xchg(&direct_reclaim_time_gap, 0);
	pgsteal_direct_diff = vmstat_data_curr.pgsteal_direct - vmstat_data_last.pgsteal_direct;
	if(direct_reclaim_time != 0){
		direct_reclaim_efficiency = pgsteal_direct_diff * NSEC_PER_MSEC * FIXED_FACTOR / direct_reclaim_time;
	} else {
		direct_reclaim_efficiency = 0;
	}
	pr_info(TRANMM" pgsteal_direct_diff is %ld, direct_reclaim_time_gap is %llu ns\n", pgsteal_direct_diff, direct_reclaim_time);
	pr_info(TRANMM" direct_reclaim_efficiency is %ld.%03ld (page/ms).\n", INTEGER_PART_DIRECT_RECLAIM(direct_reclaim_efficiency), FRACTIONAL_PART_DIRECT_RECLAIM(direct_reclaim_efficiency));
	check_direct_reclaim_efficiency_limit(direct_reclaim_efficiency);
	atomic_long_add(pgsteal_direct_diff, &pgsteal_direct_total);

	vmstat_data_last = vmstat_data_curr;
	pr_info(TRANMM"direct_reclaim:%-26s%-26s\n", "Alloc_Slowpath_Time(ms)", "Key_Pro_Alloc_Slowpath_Count");
	for(int i=0;i<NUM_INTERVALS;i++){
		pr_info(TRANMM"alloc_pages_slowpath:%-26s%-26llu\n", INTERVAL_STRINGS[i], atomic64_read(&key_processes_alloc_pages_slowpath_time_counts[i]));
	}

	//update key process direct reclaim time total
	pr_info(TRANMM" key_processes_direct_reclaim_time_total is %llu ms, key_processes_direct_reclaim_count_total is %llu\n", atomic64_read(&key_processes_direct_reclaim_time_total), atomic64_read(&key_processes_direct_reclaim_count_total));
	pr_info(TRANMM" key_processes_alloc_pages_slowpath_time_total is %llu ms, key_processes_alloc_pages_slowpath_count_total is %llu\n", atomic64_read(&key_processes_alloc_pages_slowpath_time_total), atomic64_read(&key_processes_alloc_pages_slowpath_count_total));



}

static void cal_key_processes_alloc_pages_slowpath_count_total(u64 gap_ms)
{

	if(gap_ms <= 0){
		return;
	}
	if(gap_ms <= 3){
		atomic64_inc(&key_processes_alloc_pages_slowpath_time_counts[0]);
	} else if(gap_ms <= 5){
		atomic64_inc(&key_processes_alloc_pages_slowpath_time_counts[1]);
	} else if(gap_ms <= 10){
		atomic64_inc(&key_processes_alloc_pages_slowpath_time_counts[2]);
	} else if(gap_ms <= 15){
		atomic64_inc(&key_processes_alloc_pages_slowpath_time_counts[3]);
	} else if( gap_ms <= 20){
		atomic64_inc(&key_processes_alloc_pages_slowpath_time_counts[4]);
	} else if(gap_ms <= 30){
		atomic64_inc(&key_processes_alloc_pages_slowpath_time_counts[5]);
	} else {
		atomic64_inc(&key_processes_alloc_pages_slowpath_time_counts[6]);
	}
}


static int show_direct_reclaim_info(struct seq_file *m, void *v)
{
	//direct_reclaim_efficiency_trancare
	unsigned long direct_reclaim_efficiency_trancare = 0;
	unsigned long direct_reclaim_ratio_trancare = 0;

	u64 direct_reclaim_time = atomic64_read(&direct_reclaim_time_total);
	long pgsteal_direct = atomic_long_read(&pgsteal_direct_total);
	long current_direct_reclaim_efficiency_threshold = atomic_long_read(&direct_reclaim_efficiency_threshold);
	if(direct_reclaim_time != 0){
		direct_reclaim_efficiency_trancare = pgsteal_direct * NSEC_PER_MSEC * FIXED_FACTOR / direct_reclaim_time;
	} else {
		direct_reclaim_efficiency_trancare = 0;
	}

	//direct_reclaim_ratio_trancare
	long allocstall = atomic_long_read(&allocstall_total);
	long pgfault = atomic_long_read(&pgfault_total);
	long current_direct_reclaim_ratio_threshold = atomic_long_read(&direct_reclaim_ratio_threshold);
	if(pgfault != 0){
		direct_reclaim_ratio_trancare = (allocstall * PERCENT_FACTOR * FIXED_FACTOR) / pgfault;
	} else {
 		direct_reclaim_ratio_trancare = 0;
	}

	//direct_reclaim_ratio_key_process
	long direct_reclaim_ratio_key_process = 0;
	long key_process_direct_reclaim = atomic_long_read(&key_process_direct_reclaim_total);
	long pgfault_key_process = atomic_long_read(&key_process_pgfault_total);
	long current_direct_reclaim_ratio_key_process_threshold = atomic_long_read(&direct_reclaim_ratio_key_process_threshold);
	if(pgfault_key_process != 0){
		direct_reclaim_ratio_key_process = (key_process_direct_reclaim * PERCENT_FACTOR * FIXED_FACTOR) / pgfault_key_process;
	} else {
		direct_reclaim_ratio_key_process = 0;
	}

	seq_printf(m, "%s %ld.%03ld,%ld,%ld.%03ld\n", direct_reclaim_text[0],
		INTEGER_PART_DIRECT_RECLAIM(direct_reclaim_efficiency_trancare), FRACTIONAL_PART_DIRECT_RECLAIM(direct_reclaim_efficiency_trancare),
		atomic_long_read(&direct_reclaim_efficiency_over_limit_count),
		INTEGER_PART_DIRECT_RECLAIM(current_direct_reclaim_efficiency_threshold),FRACTIONAL_PART_DIRECT_RECLAIM(current_direct_reclaim_efficiency_threshold) );
    seq_printf(m, "%s %ld.%03ld,%ld,%ld.%03ld\n", direct_reclaim_text[1], INTEGER_PART_DIRECT_RECLAIM(direct_reclaim_ratio_trancare), FRACTIONAL_PART_DIRECT_RECLAIM(direct_reclaim_ratio_trancare),
		atomic_long_read(&direct_reclaim_ratio_over_limit_count),
		INTEGER_PART_DIRECT_RECLAIM(current_direct_reclaim_ratio_threshold),FRACTIONAL_PART_DIRECT_RECLAIM(current_direct_reclaim_ratio_threshold));
	seq_printf(m, "%s %ld.%03ld,%ld,%ld.%03ld\n", direct_reclaim_text[2], INTEGER_PART_DIRECT_RECLAIM(direct_reclaim_ratio_key_process), FRACTIONAL_PART_DIRECT_RECLAIM(direct_reclaim_ratio_key_process),
		atomic_long_read(&direct_reclaim_ratio_key_process_over_limit_count),
		INTEGER_PART_DIRECT_RECLAIM(current_direct_reclaim_ratio_key_process_threshold),FRACTIONAL_PART_DIRECT_RECLAIM(current_direct_reclaim_ratio_key_process_threshold));


	return 0;
}


static int proc_direct_reclaim_info_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, show_direct_reclaim_info, inode);
}


static int parse_thousandths(char *str, unsigned long *result) {
	unsigned long integer_part = 0;
	unsigned long fractional_part = 0;
	int i;
	char *token, *ptr = str;

	//integer_part = simple_strtol(str, &next, 10);
	token = strsep(&ptr, ".");
	if (*token == '\0' || kstrtoul(token, 10, &integer_part)) {
		return -EINVAL;
	}
	if (*ptr != '\0') {
		const char *frac_str = ptr;
		//0.001
		for (i = 0; i < 3; i++) {
			fractional_part *= 10;
			if (*frac_str >= '0' && *frac_str <= '9') {
				fractional_part += *frac_str - '0';
				frac_str++;
			}
		}
	}

	*result = integer_part * 1000 + fractional_part;
	return 0;
}

//"0" to reset
static ssize_t proc_direct_reclaim_info_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *ppos)
{
	int ret = count;
	int value = 0;
	char buffer[3] = {0};
	if (count < 0 || count > 2) {
		pr_err(TRANMM" direct_reclaim_info input too long, count is %zu \n", count);
		return -EINVAL;
	}
	if (copy_from_user(buffer, ubuf, count)) {
		pr_err(TRANMM" direct reclaim info reset failed, try again.\n");
		return -EFAULT;
	}
	buffer[count] = '\0';
	if (kstrtoint(buffer, 0, &value) || value != 0) {
		pr_err(TRANMM" Invalid value for reset direct reclaim info.\n");
		return -EINVAL;
	}

	atomic64_set(&pgsteal_direct_total, 0);
	atomic64_set(&direct_reclaim_time_total, 0);
	atomic64_set(&allocstall_total, 0);
	atomic64_set(&pgfault_total, 0);
	atomic64_set(&key_process_direct_reclaim_total, 0);
	atomic64_set(&key_process_pgfault_total, 0);
	atomic64_set(&direct_reclaim_efficiency_over_limit_count, 0);
	atomic64_set(&direct_reclaim_ratio_over_limit_count, 0);
	atomic64_set(&direct_reclaim_ratio_key_process_over_limit_count, 0);
	pr_info(TRANMM" direct_reclaim_info reset success\n");
	return ret;
}

static const struct proc_ops direct_reclaim_info_fops = {
    .proc_open = proc_direct_reclaim_info_open,
    .proc_read = seq_read,
    .proc_write = proc_direct_reclaim_info_write,
    .proc_release = single_release,
};

//XXX,YYY
static ssize_t proc_direct_reclaim_threshold_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *ppos)
{
	char buffer[20] = {0};
	int ret = count;
	char param1[10] = {0};
	char param2[10] = {0};
	unsigned long value1 = 0;
	unsigned long value2 = 0;
	int param_num = 0;
	//char *saveptr = NULL;
	if (count > sizeof(buffer) - 1) {
		pr_err(TRANMM" direct_reclaim_threshold input too long\n");
		return -EINVAL;
	}
	if (copy_from_user(buffer, ubuf, count)) {
		pr_err(TRANMM" direct_reclaim_limit set failed, try again.\n");
		return -EFAULT;
	}
	buffer[count] = '\0';
	//buffer[strlen(buffer)] = '\0';
	param_num = sscanf(buffer, "%9s %9s", param1, param2);
	if (param_num != 2) {
		pr_err(TRANMM"Invalid format. Use: <num>,<num>   now is %s\n", buffer);
		return -EINVAL;
	}
	if (parse_thousandths(param1, &value1)) {
		pr_err(TRANMM"Invalid value: %s\n", param1);
		return -EINVAL;
	}
	if (parse_thousandths(param2, &value2)) {
		pr_err(TRANMM"Invalid value: %s\n", param2);
		return -EINVAL;
	}

	pr_info(TRANMM" direct_reclaim_efficiency_limit is set to %ld.%03ld.\n",
            INTEGER_PART_DIRECT_RECLAIM(value1), FRACTIONAL_PART_DIRECT_RECLAIM(value1));
	pr_info(TRANMM" direct_reclaim_ratio_limit is set to %ld.%03ld.\n", 
            INTEGER_PART_DIRECT_RECLAIM(value2), FRACTIONAL_PART_DIRECT_RECLAIM(value2));
	atomic64_set(&direct_reclaim_efficiency_threshold, value1);
	atomic64_set(&direct_reclaim_ratio_threshold, value2);
	return ret;
}

static int show_direct_reclaim_threshold(struct seq_file *m, void *v)
{
	seq_printf(m, "%ld.%03ld,%ld.%03ld\n",
		INTEGER_PART_DIRECT_RECLAIM(atomic_long_read(&direct_reclaim_efficiency_threshold)),
		FRACTIONAL_PART_DIRECT_RECLAIM(atomic_long_read(&direct_reclaim_efficiency_threshold)),
		INTEGER_PART_DIRECT_RECLAIM(atomic_long_read(&direct_reclaim_ratio_threshold)),
		FRACTIONAL_PART_DIRECT_RECLAIM(atomic_long_read(&direct_reclaim_ratio_threshold)));
	return 0;
}


static int proc_direct_reclaim_threshold_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, show_direct_reclaim_threshold, inode);
}


static const struct proc_ops direct_reclaim_threshold_fops = {
    .proc_open = proc_direct_reclaim_threshold_open,
    .proc_read = seq_read,
    .proc_write = proc_direct_reclaim_threshold_write,
    .proc_release = single_release,
};


void tranmem_direct_reclaim_proc_create(struct proc_dir_entry* parent)
{
	proc_create("direct_reclaim_info",     0666, parent, &direct_reclaim_info_fops);
	proc_create("direct_reclaim_threshold",     0666, parent, &direct_reclaim_threshold_fops);
}


static void probe_android_vh_mm_direct_reclaim_enter(void *unused, unsigned int order)
{
	if (in_interrupt()){
		return;
	}
	rcu_read_lock();
	get_task_struct(current);
	struct tranmem_task_struct *tranmem_ts = get_tranmem_task_struct(current);
	if (IS_ERR_OR_NULL(tranmem_ts)) {
		pr_err(TRANMM" trans_task_struct of %s is not inited!\n", current->comm);
		goto out;
	}
	tranmem_ts->direct_reclaim_start_time = ktime_get_ns();
out:
	put_task_struct(current);
	rcu_read_unlock();

}

static void probe_android_vh_mm_direct_reclaim_exit(void *unused, unsigned long did_some_progress, int retry_times)
{
	struct tranmem_task_struct *tranmem_ts = NULL;
	struct tranmem_task_struct *thread_tranmem_ts = NULL;
	if (in_interrupt()){
		return;
	}
	rcu_read_lock();
	get_task_struct(current);
	if (IS_ERR_OR_NULL(current->group_leader)) {
		pr_err(TRANMM"current->group_leader is NULL!\n");
		goto out;
	}
	get_task_struct(current->group_leader);
	tranmem_ts = get_tranmem_task_struct(current->group_leader);
	thread_tranmem_ts = get_tranmem_task_struct(current);
	if (IS_ERR_OR_NULL(thread_tranmem_ts)) {
		pr_err(TRANMM" trans_task_struct of %s or it's group_leader is not inited!\n", current->comm);
		put_task_struct(current->group_leader);
		goto out;
	}

	u64 gap = ktime_get_ns() - thread_tranmem_ts->direct_reclaim_start_time;
	atomic64_add(gap, &direct_reclaim_time_gap);
	atomic64_add(gap, &direct_reclaim_time_total);
	if(!IS_ERR_OR_NULL(tranmem_ts)){
		atomic64_add(gap, &tranmem_ts->direct_reclaim_gap_time);
		atomic64_inc(&tranmem_ts->direct_reclaim_count);
	}
	put_task_struct(current->group_leader);
	if (!is_critical(current)){
		goto out;
	}
	atomic64_inc(&key_process_direct_reclaim_total);
	atomic64_inc(&key_processes_direct_reclaim_count_total);
	atomic64_add(gap/NSEC_PER_MSEC, &key_processes_direct_reclaim_time_total);
out:
	put_task_struct(current);
	rcu_read_unlock();
}

static void probe_android_vh_alloc_pages_slowpath_end(void *unused, gfp_t *gfp_mask, unsigned int order, unsigned long alloc_start,u64 stime, unsigned long did_some_progress,unsigned long pages_reclaimed, int retry_loop_count)
{
	rcu_read_lock();
	get_task_struct(current);
	if (in_interrupt() || !is_critical(current)){
		goto out;
	}
	u64 gap = jiffies_to_nsecs(jiffies - alloc_start);
	atomic64_inc(&key_processes_alloc_pages_slowpath_count_total);
	if (gap > 0){
		atomic64_add(gap/NSEC_PER_MSEC, &key_processes_alloc_pages_slowpath_time_total);
		cal_key_processes_alloc_pages_slowpath_count_total(gap/NSEC_PER_MSEC);
	}
out:
	put_task_struct(current);
	rcu_read_unlock();
}


static void unregister_direct_reclaim_start_vendor_hooks(void)
{
	unregister_trace_android_vh_mm_direct_reclaim_enter(probe_android_vh_mm_direct_reclaim_enter, NULL);
}

static void unregister_direct_reclaim_end_vendor_hooks(void)
{
	unregister_trace_android_vh_mm_direct_reclaim_exit(probe_android_vh_mm_direct_reclaim_exit, NULL);
}

static int register_direct_reclaim_start_vendor_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_mm_direct_reclaim_enter(probe_android_vh_mm_direct_reclaim_enter, NULL);
	if (ret != 0) {
		pr_err(TRANMM"register direct reclaim start failed! ret=%d\n", ret);
		unregister_direct_reclaim_end_vendor_hooks();
	}

	return ret;
}

static int register_direct_reclaim_end_vendor_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_mm_direct_reclaim_exit(probe_android_vh_mm_direct_reclaim_exit, NULL);
	if (ret != 0) {
		pr_err(TRANMM"register direct reclaim end failed! ret=%d\n", ret);
		unregister_direct_reclaim_start_vendor_hooks();
	}

	return ret;
}


static void unregister_alloc_pages_slowpath_end_vendor_hooks(void)
{
	unregister_trace_android_vh_alloc_pages_slowpath_end(probe_android_vh_alloc_pages_slowpath_end, NULL);
}


static int register_alloc_pages_slowpath_end_vendor_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_alloc_pages_slowpath_end(probe_android_vh_alloc_pages_slowpath_end, NULL);
	if (ret != 0) {
		pr_err(TRANMM"register alloc pages slowpath end failed! ret=%d\n", ret);
		unregister_alloc_pages_slowpath_end_vendor_hooks();
	}

	return ret;
}

void tranmem_direct_reclaim_init(void)
{
	register_direct_reclaim_start_vendor_hooks();
	register_direct_reclaim_end_vendor_hooks();
	register_alloc_pages_slowpath_end_vendor_hooks();
}

void tranmem_direct_reclaim_exit(void)
{
	unregister_direct_reclaim_start_vendor_hooks();
	unregister_direct_reclaim_end_vendor_hooks();
	unregister_alloc_pages_slowpath_end_vendor_hooks();
}

