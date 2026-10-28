// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 Transsion. All rights reserved.
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/sched/mm.h>
#include <linux/module.h>
#include <linux/kernel_stat.h>
#include <linux/pagemap.h>
#include <linux/vmpressure.h>
#include <linux/vmstat.h>
#include <linux/file.h>
#include <linux/writeback.h>
#include <linux/mm_inline.h>
#include <linux/rmap.h>
#include <linux/topology.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/rwsem.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/memcontrol.h>
#include <linux/delayacct.h>
#include <linux/sysctl.h>
#include <linux/oom.h>
#include <linux/ksm.h>
#include <linux/prefetch.h>
#include <linux/printk.h>
#include <linux/psi.h>
#include <linux/cpufreq.h>
#include <trace/hooks/vmscan.h>
#include <linux/proc_fs.h>
#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/notifier.h>
#include <linux/mmu_notifier.h>
#include <linux/sched/mm.h>
#include <asm/tlb.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/dtask.h>
#include <linux/mm_types.h>
#include <trace/hooks/vendor_hooks.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/mmu_context.h>
#include <linux/pagewalk.h>
#include <linux/pgtable.h>
#include <linux/swap.h>
#include <linux/kprobes.h>
#include <linux/shmem_fs.h>
#include <tranlog.h>

#include "../mm/internal.h"

wait_queue_head_t kill_enhance_wait;
static struct task_struct *kill_enhance_tsk = NULL;
atomic_t kill_enhance_strategy_runnable = ATOMIC_INIT(0);
static DEFINE_MUTEX(kill_enhance_strategy_mutex);

static char *kill_enhance_ID = "1003000057";
static char *kill_enhance_enable_key = "kill_enhances";

extern int register_cloudengine_notifier(const char* id, const char* ver, struct notifier_block *nb);
extern int unregister_cloudengine_notifier(const char* id, struct notifier_block *nb);
extern void cloudengine_feed_back(const char* id, int state);
extern char* cloudengine_getStringByID(const char* id, const char* key);


typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
static kallsyms_lookup_name_t kallsyms_lookup_name_dup;
struct kprobe kallsyms_lookup_name_kp = {
	.symbol_name = "kallsyms_lookup_name"
};
static struct address_space_operations *shmem_aops_dup = NULL;

typedef bool (*folio_isolate_lru_t)(struct folio *folio);
static folio_isolate_lru_t folio_isolate_lru_dup;
struct kprobe folio_isolate_lru_kp = {
	.symbol_name = "folio_isolate_lru"
};

/*
 * The KILL ENHANCE strategy contains multiple features and is subject to the limitations of cloud control.
 * 0 Turn OFF the functions
 * 1 Turn ON the  function
 */
enum kill_enhance {
	KILL_ENHANCE_OFF = 0,
	KILL_ENHANCE_ON,
};

static atomic_t kill_enhance_setup = ATOMIC_INIT(0);

struct reclaim_mm {
	struct mm_struct	*mm;
};

#define MAX_BUF_LEN 16
static struct reclaim_mm *reclaim = NULL;

static unsigned long nr_kill_enhance_filter_pages = 0;
static unsigned long nr_kill_enhance_isolate_pages = 0;
static unsigned long nr_kill_enhance_reclaim_pages = 0;

void select_kill_enhance_strategy_cpus(void)
{
	struct cpumask mask;
	struct cpumask *cpumask = &mask;
	pg_data_t *pgdat = NODE_DATA(0);
	unsigned int cpu = 0, cpufreq_max_tmp = 0;
	struct cpufreq_policy *policy_max;
	static bool set_cpus_success = false;

	if (likely(set_cpus_success))
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
		if (NULL == kill_enhance_tsk)
			return;
		set_cpus_allowed_ptr(kill_enhance_tsk, cpumask);
		set_cpus_success = true;
	}
}

void exit_mmgrab(struct mm_struct *mm)
{
	atomic_inc(&mm->mm_count);
}

static int kill_enhance_pte_walk_reclaimer(pmd_t *pmd, unsigned long addr,
				unsigned long end, struct mm_walk *walk)
{
	int gen;
	struct vm_area_struct *vma = walk->vma;
	pte_t *orig_pte, *pte, ptent;
	spinlock_t *ptl;
	struct page *page;
	struct folio *folio;
	LIST_HEAD(folio_list);
	struct lruvec *lruvec;

	orig_pte = pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	arch_enter_lazy_mmu_mode();

	for (; addr < end; pte++, addr += PAGE_SIZE) {
		ptent = *pte;

		if (pte_none(ptent))
			continue;

		if (!pte_present(ptent))
			continue;

		page = vm_normal_page(vma, addr, ptent);
		if (!page || is_zone_device_page(page))
			continue;

		folio = page_folio(page);

		if (folio_test_swapbacked(folio))
			continue;

		if (folio_test_unevictable(folio))
			continue;

		if(folio_test_active(folio) || folio_test_workingset(folio))
			continue;

#ifdef CONFIG_LRU_GEN_ENABLED
		lruvec = folio_lruvec(folio);
		gen = folio_lru_gen(folio);
		if (lru_gen_is_active(lruvec, gen))
			continue;
#endif

		if (folio_mapcount(folio) != 1)
			continue;

		/* copy drop_caches cannot reclaim Dirty、Writeback、Mapped pages */
		if (folio_test_dirty(folio) || folio_test_writeback(folio))
			continue;

		/*
		 * tig show e41c81d0d30e1a6ebf408feaf561f80cac4457dc
		 * folio_mapped() is expensive because it has to check each page's mapcount
		 * field.  A cheaper check is whether there are any extra references to
	     * the page, other than the one we own, one from the page private data and
	     * the ones held by the page cache.
		 */
		if (folio_ref_count(folio) >
							folio_nr_pages(folio) + folio_has_private(folio) + 1)
			continue;

		nr_kill_enhance_filter_pages++;

		if (!folio_isolate_lru_dup(folio))
			continue;

		nr_kill_enhance_isolate_pages++;

		list_add(&folio->lru, &folio_list);
	}

	arch_leave_lazy_mmu_mode();
	pte_unmap_unlock(orig_pte, ptl);

	nr_kill_enhance_reclaim_pages += reclaim_pages(&folio_list);

	cond_resched();
	return 0;
}

struct mm_walk_ops reclaimer_walk = {
	.pmd_entry = kill_enhance_pte_walk_reclaimer,
};

/*
 * Determine whether it is shmem; if so, such pages are categorized as anonymous pages to avoid unnecessary scanning.
 * true: shmem;
 * false：not shmem;
 */
bool shmem_mapping_dup(struct address_space *mapping)
{
	if (!mapping)
		return true;

	if (!shmem_aops_dup)
		return true;

	return mapping->a_ops == shmem_aops_dup;
}

bool tran_can_do_file_pageout(struct vm_area_struct *vma)
{
	struct address_space *mapping;

	if (vma_is_anonymous(vma))
		return false;

	if (vma->vm_file && vma->vm_file->f_mapping) {
		mapping = vma->vm_file->f_mapping;
		if (shmem_mapping_dup(mapping)) {
			return false;
		}
	}

	return true;
}

bool tran_can_madv_lru_vma(struct vm_area_struct *vma)
{
	return !(vma->vm_flags & (VM_LOCKED|VM_PFNMAP|VM_HUGETLB));
}

static void kill_enhance_strategy_task_mm(struct mm_struct *mm)
{
	struct vm_area_struct *vma = NULL;

	if (!mmap_read_trylock(mm))
		return;

	if (test_bit(MMF_OOM_SKIP, &mm->flags)) {
		mmap_read_unlock(mm);
		return;
	}

	VMA_ITERATOR(vmi, mm, 0);

	for_each_vma(vmi, vma) {
		if (is_vm_hugetlb_page(vma))
			continue;

		if (!tran_can_madv_lru_vma(vma))
			continue;

		/*
		 * If the process is in an exit,drop the clean and unmapped file pages of the exited process.
		 * Rapid memory release to alleviate system memory pressure.
		 */
		if (tran_can_do_file_pageout(vma)) {
			walk_page_range(vma->vm_mm, vma->vm_start, vma->vm_end, &reclaimer_walk, NULL);
		}

		/* Double-check */
		if (test_bit(MMF_OOM_SKIP, &mm->flags))
			break;
	}

	mmap_read_unlock(mm);
}

static int kill_enhance_func(void *p)
{
	struct mm_struct *mm = NULL;

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

	atomic_set(&kill_enhance_strategy_runnable, 0);

	while (!kthread_should_stop()) {
		wait_event_freezable(kill_enhance_wait,
					(atomic_read(&kill_enhance_strategy_runnable) == 1));
		
		select_kill_enhance_strategy_cpus();

		mutex_lock(&kill_enhance_strategy_mutex);
		if (reclaim == NULL) {
			atomic_set(&kill_enhance_strategy_runnable, 0);
			mutex_unlock(&kill_enhance_strategy_mutex);
			continue;
		}

		mm = reclaim->mm;
		if (mm == NULL) {
			atomic_set(&kill_enhance_strategy_runnable, 0);
			mutex_unlock(&kill_enhance_strategy_mutex);
			continue;
		}
		mutex_unlock(&kill_enhance_strategy_mutex);

		kill_enhance_strategy_task_mm(mm);

		if(unlikely(atomic_dec_and_test(&mm->mm_count)))
			__mmdrop(mm);

		atomic_set(&kill_enhance_strategy_runnable, 0);

	}
	current->flags &= ~(PF_MEMALLOC | PF_KSWAPD);
	current->reclaim_state = NULL;

	return 0;
}

#if 0
static bool system_memory_status(void)
{
	unsigned long nr_file_pages = 0;
	unsigned long nr_file_mapped_pages = 0;

	nr_file_pages = global_node_page_state(NR_ACTIVE_FILE) +
					global_node_page_state(NR_INACTIVE_FILE);
	nr_file_mapped_pages = global_node_page_state(NR_FILE_MAPPED);

	if ((nr_file_pages >> 1) >=  nr_file_mapped_pages)
		return true;

	return false;
}
#endif

static bool kill_enhance_can_quick_reclaimed(struct mm_struct *mm)
{
	if (mm == NULL)
		return false;

	if (test_bit(MMF_OOM_SKIP, &mm->flags))
		return false;

	//if (!system_memory_status())
	//	return false;

	return true;
}

static void kill_enhance_strategy(void *data, struct task_struct *exit_task)
{
	struct task_struct *task_check;
	struct mm_struct *mm;

	if (unlikely(atomic_read(&kill_enhance_setup) == KILL_ENHANCE_OFF))
		return;

	if (exit_task == NULL)
		return;

	if (!mutex_trylock(&kill_enhance_strategy_mutex))
		return;

	/* Avoid repeated thread wake-ups. */
	if (atomic_read(&kill_enhance_strategy_runnable) == 1) {
		mutex_unlock(&kill_enhance_strategy_mutex);
		return;
	}

	task_check = exit_task;
	get_task_struct(task_check);

	if (!(task_check->flags & PF_EXITING) || (task_check->pid != task_check->tgid) 
		|| (task_check->group_leader != task_check)) {
		put_task_struct(task_check);
		mutex_unlock(&kill_enhance_strategy_mutex);
		return;
	}

	mm = task_check->mm;
	if (!kill_enhance_can_quick_reclaimed(mm)) {
		put_task_struct(task_check);
		mutex_unlock(&kill_enhance_strategy_mutex);
		return;
	}

	exit_mmgrab(mm);
	reclaim->mm = mm;

	atomic_set(&kill_enhance_strategy_runnable, 1);
	put_task_struct(task_check);
	mutex_unlock(&kill_enhance_strategy_mutex);

	wake_up_interruptible(&kill_enhance_wait);
}

static int register_kill_enhance_strategy_vendor_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_exit_check(kill_enhance_strategy, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_exit_check failed! ret=%d\n", ret);
		goto out;
	}

	return ret;

out:
	return ret;
}

static void unregister_kill_enhance_strategy_vendor_hooks(void)
{
	unregister_trace_android_vh_exit_check(kill_enhance_strategy, NULL);

	return;
}

static int kill_enhance_debug_show(struct seq_file *m, void *arg)
{
	seq_printf(m,
			   "kill_enhance debug distribution:\n"
			   "nr_kill_enhance_filter_pages:             %ld\n"
			   "nr_kill_enhance_isolate_pages:            %ld\n"
			   "nr_kill_enhance_reclaim_pages:            %ld\n",
			   nr_kill_enhance_filter_pages,
			   nr_kill_enhance_isolate_pages,
			   nr_kill_enhance_reclaim_pages);
	seq_putc(m, '\n');

	return 0;
}

static int kill_enhance_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, kill_enhance_debug_show,
			   inode->i_private);
}

static const struct proc_ops kill_enhance_ops = {
	.proc_open = kill_enhance_debug_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int kill_enhance_state_show(struct seq_file *m, void *arg)
{
	seq_printf(m, "kill_enhance_setup:     %d\n", atomic_read(&kill_enhance_setup));
	seq_putc(m, '\n');

	return 0;
}

static int kill_enhance_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, kill_enhance_state_show, inode->i_private);
}

static ssize_t kill_enhance_state_write(struct file *file,
										const char __user *buff, size_t len, loff_t *ppos)
{
	int ret;
	char kbuf[MAX_BUF_LEN] = {'\0'};
	char *str;
	int value;

	if (len > MAX_BUF_LEN - 1) {
		return -EINVAL;
	}

	if (copy_from_user(&kbuf, buff, len))
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

	if (value < 0 || value > INT_MAX){
		return -EINVAL;
	}

	printk("page_filter_state_write is %d\n", value);
	atomic_set(&kill_enhance_setup, value);

	return len;
}

static const struct proc_ops kill_enhance_state_ops = {
	.proc_open = kill_enhance_state_open,
	.proc_write = kill_enhance_state_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int register_kprobe_resource_function(void)
{
	int ret = 0;
	/* get some symbols address using kprobe */
	ret = register_kprobe(&kallsyms_lookup_name_kp);
	if (ret < 0) {
		pr_err("get kallsyms_lookup_name addr from kprobe failed!\n");
		return ret;
	}

	kallsyms_lookup_name_dup = (kallsyms_lookup_name_t)kallsyms_lookup_name_kp.addr;
	if (!kallsyms_lookup_name_dup) {
		unregister_kprobe(&kallsyms_lookup_name_kp);
		pr_err("get kallsyms_lookup_name_dup addr from kprobe failed!\n");
		return -1;;
	}

	pr_info("suceesfully get kallsyms_lookup_name addr:0x%px\n", kallsyms_lookup_name_dup);
	unregister_kprobe(&kallsyms_lookup_name_kp);

	shmem_aops_dup = (struct address_space_operations *)kallsyms_lookup_name_dup("shmem_aops");
	if (!shmem_aops_dup) {
		pr_err("get shmem_aops addr from kprobe failed!\n");
		return -1;;
	}
	pr_info("suceesfully get shmem_aops addr:0x%px\n", shmem_aops_dup);

	/* get folio_isolate_lru symbols address using kprobe */
	ret = register_kprobe(&folio_isolate_lru_kp);
	if (ret < 0) {
		pr_err("get folio_isolate_lru addr from kprobe failed!\n");
		return ret;
	}

	folio_isolate_lru_dup = (folio_isolate_lru_t)folio_isolate_lru_kp.addr;
	if (!folio_isolate_lru_dup) {
		unregister_kprobe(&folio_isolate_lru_kp);
		pr_err("get folio_isolate_lru_dup addr from kprobe failed!\n");
		return -1;;
	}

	pr_info("suceesfully get folio_isolate_lru addr:0x%px\n", folio_isolate_lru_dup);
	unregister_kprobe(&folio_isolate_lru_kp);

	return ret;
}

static int kill_enhance_status_change(struct notifier_block *nb, unsigned long action, void* data)
{
	struct cloud_engine_config* cec;
	char* id;
	char* str_kill_enhance;

	if(NULL == data){
		pr_err("%s data == NULL, default enable kill_enhance function\n", __FUNCTION__);
		return -1;
	}

	cec = (struct cloud_engine_config*)data;
	id = cec->id;
	if(strcmp(kill_enhance_ID, id) != 0) {
		return 0;
	}

	cloudengine_feed_back(kill_enhance_ID, 1);

	str_kill_enhance = cloudengine_getStringByID(kill_enhance_ID, kill_enhance_enable_key);
	if (NULL == str_kill_enhance) {
		pr_err("%s get %s value failed, default enable kill_enhance function\n", __FUNCTION__, kill_enhance_enable_key);
		return 0;
	}else {
		if (strcmp("false", str_kill_enhance) == 0) {
			atomic_set(&kill_enhance_setup, 0);
		} else {
			atomic_set(&kill_enhance_setup, 1);
		}
		pr_info("%s cloud control settings kill_enhance status = %s\n", __FUNCTION__, str_kill_enhance);
	}

    return 0;
}

struct notifier_block kill_enhance_notifier_block = {
    .notifier_call = kill_enhance_status_change,
};

static int __init kill_enhance_strategy_init(void)
{
	int ret = 0;
	struct proc_dir_entry *kill_enhance_dir = NULL;

	reclaim = kmalloc(sizeof(*reclaim), GFP_KERNEL);
	if (!reclaim) {
		pr_err("Kill enhance strategy Kmalloc memory failed\n");
		return 0;
	}

	ret = register_kprobe_resource_function();
	if (ret < 0) {
		pr_err("kprobe resource function register failed\n");
		kfree(reclaim);
		return ret;
	}

	ret = register_kill_enhance_strategy_vendor_hooks();
	if (ret != 0) {
		pr_err("Kill enhance strategy hooks register failed\n");
		kfree(reclaim);
		return ret;
	}

	init_waitqueue_head(&kill_enhance_wait);

	kill_enhance_tsk = kthread_run(kill_enhance_func, NULL, "exit_enhanced");
	if (IS_ERR_OR_NULL(kill_enhance_tsk)) {
		pr_err("Failed to start kill_enhanced on node 0\n");
		ret = PTR_ERR(kill_enhance_tsk);
		kill_enhance_tsk = NULL;
		kfree(reclaim);
		return ret;
	}

	kill_enhance_dir = proc_mkdir("kill_enhance", NULL);
	if (kill_enhance_dir == NULL) {
		pr_err("proc create kill_enhance_dir failed\n");
		kfree(reclaim);
		ret = -1;
		return ret;
	}

	proc_create("kill_enhance_debug", 0666, kill_enhance_dir, &kill_enhance_ops);
	proc_create("kill_enhance_state", 0666, kill_enhance_dir, &kill_enhance_state_ops);

	atomic_set(&kill_enhance_setup, 1);
	register_cloudengine_notifier(kill_enhance_ID, "v1.x", &kill_enhance_notifier_block);

	pr_info("kill_enhance_strategy_init succeed!\n");

	return 0;
}

static void __exit kill_enhance_strategy_exit(void)
{
	unregister_kill_enhance_strategy_vendor_hooks();

	unregister_cloudengine_notifier(kill_enhance_ID, &kill_enhance_notifier_block);

	kfree(reclaim);
	reclaim = NULL;

	pr_info("kill_enhance_strategy exit succeed!\n");
	return;
}

module_init(kill_enhance_strategy_init);
module_exit(kill_enhance_strategy_exit);

MODULE_LICENSE("GPL v2");
