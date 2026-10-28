// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (C) 2020-2022 Transsion. All rights reserved.
*/
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/vmpressure.h>
#include <linux/vmstat.h>
#include <linux/file.h>
#include <linux/writeback.h>
#include <linux/blkdev.h>
#include <linux/mm_inline.h>
#include <linux/backing-dev.h>
#include <linux/rmap.h>
#include <linux/topology.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/compaction.h>
#include <linux/notifier.h>
#include <linux/rwsem.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/memcontrol.h>
#include <linux/delayacct.h>
#include <linux/sysctl.h>
#include <linux/oom.h>
#include <linux/prefetch.h>
#include <linux/printk.h>
#include <linux/dax.h>
#include <linux/psi.h>
#include <linux/swapops.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <trace/hooks/vmscan.h>
#include <linux/swap.h>
#include <linux/proc_fs.h>
#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/cpufreq.h>
#include <linux/freezer.h>
#include <linux/wait.h>


#define MAX_BUF_LEN 8
#define SHRINK_PRIO_MAX 12
#define SHRINK_DELAY_MAX 2000
extern unsigned long shrink_slab(gfp_t gfp_mask, int nid,
				 struct mem_cgroup *memcg,
				 int priority);

static bool async_shrink_slabd_setup = false;
static int g_shrink_prio = 5;
static int g_shrink_slabd_interval = 200;
wait_queue_head_t shrink_slabd_wait;
struct async_slabd_parameter {
	gfp_t shrink_slabd_gfp_mask;
	int shrink_slabd_nid;
	struct mem_cgroup *shrink_slabd_memcg;
	int shrink_slabd_priority;
	atomic_t shrink_slabd_runnable;
	struct reclaim_state *reclaim_state;
	unsigned long expires;
} asp;

static struct task_struct *shrink_slabd_tsk = NULL;
static int kshrink_slabd_pid;

static struct reclaim_state async_reclaim_state = {
	.reclaimed = 0,
};

static DEFINE_MUTEX(async_kshrink_slabd_mutex);

/* direct copy from vmscan.c  */
static void set_task_reclaim_state(struct task_struct *task,
	struct reclaim_state *rs)
{
	/* Check for an overwrite */
	WARN_ON_ONCE(rs && task->reclaim_state);

	/* Check for the nulling of an already-nulled member */
	WARN_ON_ONCE(!rs && !task->reclaim_state);

	task->reclaim_state = rs;
}

static int shrink_prio_show(struct seq_file *m, void *v)
{

	seq_printf(m, "shrink_prio: %d\n", g_shrink_prio);

	return 0;
}

static int shrink_prio_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, shrink_prio_show, inode);
}

static ssize_t shrink_prio_write(struct file *file, const char __user *buf,
							size_t len, loff_t *ppos)
{
	int ret;
	char kbuf[MAX_BUF_LEN] = {'\0'};
	char *str;
	int value;

	if (len > MAX_BUF_LEN - 1) {
		return -EINVAL;
	}

	if (copy_from_user(&kbuf, buf, len))
		return -EFAULT;
	kbuf[len] = '\0';

	str = strstrip(kbuf);
	if (!str) {
		return -EINVAL;
	}

	ret = kstrtoint(str, 10, &value);
	if (ret) {
		return -EINVAL;
	}

	if (value < 0 || value > SHRINK_PRIO_MAX) {
		return -EINVAL;
	}

	printk("shrink_prio is %d\n", g_shrink_prio);
	g_shrink_prio = value;

	return len;
}

static const struct proc_ops shrink_prio_fops = {
	.proc_open = shrink_prio_open,
	.proc_read = seq_read,
	.proc_write = shrink_prio_write,
	.proc_release = single_release,
};

static int shrink_interval_show(struct seq_file *m, void *v)
{

	seq_printf(m, "shrink_interval: %d\n", g_shrink_slabd_interval);

	return 0;
}

static int shrink_interval_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, shrink_interval_show, inode);
}

static ssize_t shrink_interval_write(struct file *file, const char __user *buf,
							size_t len, loff_t *ppos)
{
	int ret;
	char kbuf[MAX_BUF_LEN] = {'\0'};
	char *str;
	int value;

	if (len > MAX_BUF_LEN - 1) {
		return -EINVAL;
	}

	if (copy_from_user(&kbuf, buf, len))
		return -EFAULT;
	kbuf[len] = '\0';

	str = strstrip(kbuf);
	if (!str) {
		return -EINVAL;
	}

	ret = kstrtoint(str, 10, &value);
	if (ret) {
		return -EINVAL;
	}

	if (value < 0 || value > SHRINK_DELAY_MAX) {
		return -EINVAL;
	}

	printk("shrink_interval is %d ms\n", value);
	g_shrink_slabd_interval = value;

	return len;
}

static const struct proc_ops shrink_interval_fops = {
	.proc_open = shrink_interval_open,
	.proc_read = seq_read,
	.proc_write = shrink_interval_write,
	.proc_release = single_release,
};


bool wakeup_shrink_slabd(gfp_t gfp_mask, int nid,
				 struct mem_cgroup *memcg,
				 int priority)
{
	if (unlikely(!async_shrink_slabd_setup))
		return false;

	if (!mutex_trylock(&async_kshrink_slabd_mutex))
		return true;

	if (atomic_read(&(asp.shrink_slabd_runnable)) == 1 ||
		(time_before(jiffies, asp.expires) && priority > g_shrink_prio)){
		mutex_unlock(&async_kshrink_slabd_mutex);
		return true;
	}

	async_reclaim_state.reclaimed = 0;
	asp.reclaim_state = &async_reclaim_state;
	asp.shrink_slabd_gfp_mask = gfp_mask;
	asp.expires = jiffies + msecs_to_jiffies(g_shrink_slabd_interval);
	asp.shrink_slabd_nid = nid;
	asp.shrink_slabd_memcg = memcg;
	asp.shrink_slabd_priority = priority;

	atomic_set(&(asp.shrink_slabd_runnable), 1);

	css_get(&memcg->css);
	mutex_unlock(&async_kshrink_slabd_mutex);

	wake_up_interruptible(&shrink_slabd_wait);

	return true;
}

/*
 * Choose an idle CPU or the CPU with the highest frequency
 * Need more validation analysis
*/
void set_async_slabd_cpus(void)
{
	struct cpumask mask;
	struct cpumask *cpumask = &mask;
	pg_data_t *pgdat = NODE_DATA(0);
	unsigned int cpu = 0, cpufreq_max_tmp = 0;
	struct cpufreq_policy *policy_max;
	static bool set_slabd_cpus_success = false;

	if (unlikely(!async_shrink_slabd_setup))
		return;

	if (likely(set_slabd_cpus_success))
		return;
	for_each_possible_cpu(cpu) {
		struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);

		if (policy == NULL)
			continue;

		if (policy->cpuinfo.max_freq >= cpufreq_max_tmp) {
			cpufreq_max_tmp = policy->cpuinfo.max_freq;
			policy_max = policy;
		}
	}

	cpumask_copy(cpumask, cpumask_of_node(pgdat->node_id));
	cpumask_andnot(cpumask, cpumask, policy_max->related_cpus);

	if (!cpumask_empty(cpumask)) {
		set_cpus_allowed_ptr(shrink_slabd_tsk, cpumask);
		set_slabd_cpus_success = true;
	}
}

static int kshrink_slabd_func(void *p)
{
	int nid, priority;
	struct mem_cgroup *memcg;
	gfp_t gfp_mask;
	/*
	 * Tell the memory management that we're a "memory allocator",
	 * and that if we need more memory we should get access to it
	 * regardless (see "__alloc_pages()"). "kswapd" should
	 * never get caught in the normal page freeing logic.
	 *
	 * (Kswapd normally doesn't need memory anyway, but sometimes
	 * you need a small amount of memory in order to be able to
	 * page out something else, and this flag essentially protects
	 * us from recursively trying to free more memory as we're
	 * trying to free the first piece of memory in the first place).
	 */
    /* copy from kswapd
     * Set the status of the current process and add a flag: 1 Memory allocation thread, 2. Allow write back to swap, 3 Is a kswapd thread
     *
     * There may be memory allocation during the memory reclamation process.
     * If memory allocation occurs, all restrictions will be ignored and memory allocation will be attempted as much as possible;
     * The current process may also write swap pages to the swap partition; The current process is memory reclamation
     */
	current->flags |= PF_MEMALLOC | PF_KSWAPD;
	set_freezable();

	asp.reclaim_state = NULL;
	asp.shrink_slabd_gfp_mask = 0;
	asp.shrink_slabd_nid = 0;
	asp.shrink_slabd_memcg = NULL;
	asp.shrink_slabd_priority = 0;
    atomic_set(&(asp.shrink_slabd_runnable), 0);

	while (!kthread_should_stop()) {
		wait_event_freezable(shrink_slabd_wait,
					(atomic_read(&(asp.shrink_slabd_runnable)) == 1));

		set_async_slabd_cpus();
		
		mutex_lock(&async_kshrink_slabd_mutex);
		set_task_reclaim_state(current, asp.reclaim_state);
		gfp_mask = asp.shrink_slabd_gfp_mask;
		nid = asp.shrink_slabd_nid;
		priority = asp.shrink_slabd_priority;
		memcg = asp.shrink_slabd_memcg;
		atomic_set(&(asp.shrink_slabd_runnable), 0);
		mutex_unlock(&async_kshrink_slabd_mutex);

		shrink_slab(gfp_mask, nid, memcg, priority);

		css_put(&memcg->css);
		set_task_reclaim_state(current, NULL);
	}
	current->flags &= ~(PF_MEMALLOC | PF_KSWAPD);
	current->reclaim_state = NULL;

	return 0;
}

static bool judge_shrink_slabd_task_pid(struct task_struct *tsk)
{
	return tsk->pid == kshrink_slabd_pid;
}

/*
 * Avoid recursive loop calls
 */
static void should_shrink_async(void *data, gfp_t gfp_mask, int nid,
			struct mem_cgroup *memcg, int priority, bool *bypass)
{
	if (unlikely(!async_shrink_slabd_setup)) {
		*bypass = false;
		return;
	}

	if (judge_shrink_slabd_task_pid(current)) {
		*bypass = false;
	} else {
		*bypass = wakeup_shrink_slabd(gfp_mask, nid, memcg, priority);
	}
}

static int register_shrink_async_vendor_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_shrink_slab_bypass(should_shrink_async, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_shrink_slab_bypass failed! ret=%d\n", ret);
		goto out;
	}
out:
	return ret;
}

static void unregister_shrink_async_vendor_hooks(void)
{
	unregister_trace_android_vh_shrink_slab_bypass(should_shrink_async, NULL);

	return;
}

static int __init async_shrink_slabd_init(void)
{
	int ret = 0;
	struct proc_dir_entry * shrink_slabd_dir = NULL;

	ret = register_shrink_async_vendor_hooks();
	if (ret != 0)
		return ret;

	init_waitqueue_head(&shrink_slabd_wait);

	shrink_slabd_dir = proc_mkdir("shrink_slabd", NULL);
	if (shrink_slabd_dir == NULL) {
		pr_err("proc create shrink_slabd failed\n");
		return -1;
	}
	proc_create("shrink_interval", 0666, shrink_slabd_dir, &shrink_interval_fops);
	proc_create("shrink_prio", 0666, shrink_slabd_dir, &shrink_prio_fops);

	shrink_slabd_tsk = kthread_run(kshrink_slabd_func, NULL, "kshrink_slabd");
	if (IS_ERR_OR_NULL(shrink_slabd_tsk)) {
		pr_err("Failed to start shrink_slabd on node 0\n");
		ret = PTR_ERR(shrink_slabd_tsk);
		shrink_slabd_tsk = NULL;
		remove_proc_subtree("shrink_slabd", NULL);
		return ret;
	}

	kshrink_slabd_pid = shrink_slabd_tsk->pid;
	async_shrink_slabd_setup = true;

	return 0;
}

static void __exit async_shrink_slabd_exit(void)
{
	unregister_shrink_async_vendor_hooks();
	remove_proc_subtree("shrink_slabd", NULL);

	return;
}

module_init(async_shrink_slabd_init);
module_exit(async_shrink_slabd_exit);

MODULE_LICENSE("GPL v2");

