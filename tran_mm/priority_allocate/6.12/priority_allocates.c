// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 Transsion. All rights reserved.
 */

#if IS_ENABLED(CONFIG_TRANS_SCHED_FEATURE)
#include <trans_sched_interface.h>
#endif


#include <linux/module.h>
#include <trace/hooks/vmscan.h>
#include <trace/hooks/mm.h>
#include <linux/swap.h>
#include <linux/proc_fs.h>
#include <linux/gfp.h>
#include <linux/spinlock.h>
#include <uapi/linux/sched/types.h>
#include <linux/cpufreq.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/pagemap.h>
#include <linux/page-flags.h>
#include <linux/pageblock-flags.h>
#include <linux/debugfs.h>
#include <linux/memcontrol.h>
#include <linux/mm_types.h>
#include <linux/kasan.h>
#include <linux/page_owner.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/cgroup.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/vmstat.h>
#include <linux/oom.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/page_ref.h>
#include <linux/mmzone.h>
#include <linux/kprobes.h>
#include <linux/delay.h>

#include "../mm/internal.h"
#include "priority_allocate.h"

static const unsigned int orders = 0;
/* Different memory configurations choose memory pools of different sizes. */
static const unsigned int page_pool_nr_pages = (SZ_32M >> PAGE_SHIFT);
static struct page_pool *pools = NULL;
static struct per_cpu_pages __percpu *dma32_pcp_base = NULL;

#define VIP_PAGE_POOL_NAME "page_pool_fillthread"
static struct task_struct *vip_page_pool_tsk = NULL;
static wait_queue_head_t kworkthread_waitq;

static atomic_t kworkthread_wait_flag = ATOMIC_INIT(0);
static atomic_t vip_page_pool_setup = ATOMIC_INIT(0);
static atomic_t vip_page_fillthread_setup = ATOMIC_INIT(0);
static atomic_long_t nr_ux_binder_task_allocate = ATOMIC_INIT(0);
static atomic_long_t nr_skip_avoid_d = ATOMIC_INIT(0);

#define VIP_MEM_POOL_MAX_PAGES 2
#define PARA_BUF_LEN 512
#define MAX_VIP_POOL_ALLOC_RETRIES 3

static pid_t allocator_service_pid = -1;

/* prep_new_page Initialize the new allocated page. */
typedef void (*post_alloc_hook_t)(struct page *page, unsigned int order, gfp_t gfp_flags);
static post_alloc_hook_t post_alloc_hook_dup = NULL;

static int page_pool_fill(struct page_pool *pool, int migratetype);
extern int vip_task_priority_init(struct proc_dir_entry *priority_allocate_dir);
extern bool task_is_high_priority(struct task_struct *task);

bool check_priority_allocate_feature(void)
{
	if (atomic_read(&vip_page_pool_setup) == VIP_ALLOCATE_OFF)
		return false;

	return true;
}

static void init_dma32_pcp(void)
{
	int i, nid;
	struct zone *zone;

	for_each_online_node(nid) {
		pg_data_t *pgdat = NODE_DATA(nid);

		for (i = 0; i <= ZONE_NORMAL; i++) {
			zone = &pgdat->node_zones[i];

			if (populated_zone(zone) && !strcmp(zone->name, "DMA32")) {
				dma32_pcp_base = zone->per_cpu_pageset;
				pr_info("DMA32 zone detected\n");
			}
		}
	}
}


static bool in_dma32_zone(struct per_cpu_pages *pcp)
{
	if (dma32_pcp_base)
		return pcp == this_cpu_ptr(dma32_pcp_base);
	return false;
}

static void page_pool_wakeup_process(struct page_pool *pool)
{
	if (unlikely(atomic_read(&vip_page_pool_setup) == VIP_ALLOCATE_OFF))
		return;

		/* Avoid repeated thread wake-ups. */
	if (atomic_read(&kworkthread_wait_flag) == 1)
		return;

	if (pool == NULL) {
		pr_err("%s: boost_pool is NULL!\n", __func__);
		return;
	}

	if (likely(atomic_read(&vip_page_fillthread_setup) == POOL_FILLTHREAD_ON)) {
		atomic_set(&kworkthread_wait_flag, 1);
		wake_up_interruptible(&kworkthread_waitq);
	}
}

void __maybe_unused set_vip_page_pool_fillthread_cpus(void)
{
	struct cpumask mask;
	struct cpumask *cpumask = &mask;
	pg_data_t *pgdat = NODE_DATA(0);
	unsigned int cpu = 0, cpufreq_max_tmp = 0;
	struct cpufreq_policy *policy_max = NULL;

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
	if (policy_max)
		cpumask_andnot(cpumask, cpumask, policy_max->related_cpus);

	if (!cpumask_empty(cpumask))
		set_cpus_allowed_ptr(current, cpumask);
}

static int vip_page_pool_fillthread(void *p)
{
	int i;
	int ret;

#ifdef VIP_PAGE_POOL_DEBUG
	unsigned long begin;
#endif

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(kworkthread_waitq,
									   (atomic_read(&kworkthread_wait_flag) == 1));
		if (ret < 0)
			continue;

		set_vip_page_pool_fillthread_cpus();

		for (i = 0; i < POOL_MIGRATETYPE_TYPES_SIZE; i++) {

#ifdef VIP_PAGE_POOL_DEBUG
			begin = jiffies;

			pr_info("fill start >>>>> order:%d migratetype:%d low: %d high: %d count:%d gfp_mask %#x.\n",
					pools->order, i, pools->low[i], pools->high[i], pools->count[i], pools->gfp_mask);
#endif
			while (pools->count[i] < pools->high[i]) {
				if (page_pool_fill(pools, i) < 0) {
					/* sleep for 20ms if alloc fail */
					msleep_interruptible(20);
				}
			}

#ifdef VIP_PAGE_POOL_DEBUG
			pr_info("fill end >>> order:%d migratetype:%d low: %d high: %d count:%d  gfp_mask %#x use %dms\n",
					pools->order, i, pools->low[i], pools->high[i], pools->count[i],  pools->gfp_mask, jiffies_to_msecs(jiffies - begin));
#endif
		}

		atomic_set(&kworkthread_wait_flag, 0);
	}

	return 0;
}

struct page_pool *vip_page_pool_create(gfp_t gfp_mask, unsigned int order, unsigned int nr_pages)
{
	struct page_pool *pool;
	int i;

	pool = kmalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return ERR_PTR(-ENOMEM);

	/* init vip memory pools infomation */
	pool->order = order;
	for (i = 0; i < POOL_MIGRATETYPE_TYPES_SIZE; i++) {
		pool->count[i] = 0;
		/* MIGRATETYPE: UNMOVABLE & MOVABLE */
		if (i == POOL_MIGRATETYPE_UNMOVABLE) {
			pool->gfp_mask[i] = gfp_mask;
			pool->high[i] = nr_pages / (POOL_MIGRATETYPE_TYPES_SIZE*2);
		} else {
			pool->gfp_mask[i] = gfp_mask | __GFP_MOVABLE;
			pool->high[i] = nr_pages*3 / (POOL_MIGRATETYPE_TYPES_SIZE*2);
		}
		/* wakeup kthread on count < low*/
		pool->low[i] = pool->high[i] / 2;
		pool->total_used[i] = 0;
		pool->pcp_used[i] = 0;
		pool->allocate_fail[i] = 0;
		INIT_LIST_HEAD(&pool->items[i]);

		pr_info("%s order:%d migratetype:%d low: %d high: %d count:%d.\n",
				__func__, pool->order, i, pool->low[i], pool->high[i], pool->count[i]);
	}

	spin_lock_init(&pool->lock);
	return pool;
}

static void page_pool_add(struct page_pool *pool, struct page *page, int migratetype)
{
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	list_add_tail(&page->lru, &pool->items[migratetype]);
	pool->count[migratetype]++;
	spin_unlock_irqrestore(&pool->lock, flags);
}

static struct page *page_pool_remove(struct page_pool *pool, int migratetype, bool pcp_used)
{
	struct page *page;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	
	page = list_first_entry_or_null(&pool->items[migratetype], struct page, lru);
	if (page) {
		pool->count[migratetype]--;
		pool->total_used[migratetype]++;
		if (pcp_used)
			pool->pcp_used[migratetype]++;
		list_del(&page->lru);
	}
	spin_unlock_irqrestore(&pool->lock, flags);

	/* wakeup kthread on count < low*/
	if (pool->count[migratetype] < pool->low[migratetype])
		page_pool_wakeup_process(pool);

	return page;
}

static int page_pool_fill(struct page_pool *pool, int migratetype)
{
	struct page *page;

	if (pool == NULL || migratetype >= POOL_MIGRATETYPE_TYPES_SIZE) {
		pr_err("%s: pool is NULL or migratetype is error!\n", __func__);
		return -1;
	}

	gfp_t gfp_refill = pool->gfp_mask[migratetype];

	page = alloc_pages(gfp_refill, pool->order);
	if (page == NULL)
		return -1;

	page_pool_add(pool, page, migratetype);

	return 1;
}

/* fast path */
struct page *vip_page_pool_alloc_pages(unsigned int order, int migratetype, bool pcp_used)
{
	struct page *page = NULL;
	int retries = 0;

	if ((migratetype > MIGRATE_MOVABLE) || (order != orders))
		return NULL;

	if (pools == NULL)
		return NULL;

retry:
	/* Fast-path: Get a page from cache */
	page = page_pool_remove(pools, migratetype, pcp_used);
	if (!page && retries < MAX_VIP_POOL_ALLOC_RETRIES) {
		retries++;
		goto retry;
	}

	if (!page)
		pools->allocate_fail[migratetype]++;

	return page;
}

static void vip_page_pool_destory(void)
{
	int i;
	unsigned long flags;
	struct page *page, *tmp;

	atomic_set(&vip_page_fillthread_setup, POOL_FILLTHREAD_OFF);
	atomic_set(&vip_page_pool_setup, VIP_ALLOCATE_OFF);

	if (pools == NULL)
		return;

	spin_lock_irqsave(&pools->lock, flags);
	for (i = 0; i < POOL_MIGRATETYPE_TYPES_SIZE; i++) {
		list_for_each_entry_safe(page, tmp, &pools->items[i], lru)
			__free_pages(page, pools->order);
	}
	spin_unlock_irqrestore(&pools->lock, flags);
	kfree(pools);
}

int vip_page_pool_refill(struct page *page, unsigned int order, int migratetype)
{
	if (pools == NULL || (order != orders))
		return false;
	/* vip memory pool full, so release page to buddy system */
	if (pools->count[migratetype] >= pools->high[migratetype])
		return false;

	/* set_page_count(page, 1); */
	page_pool_add(pools, page, migratetype);

	return true;
}

static ssize_t vip_page_pool_write(struct file *file,
								  const char __user *buff, size_t len, loff_t *ppos)
{
	char kbuf[PARA_BUF_LEN] = {'\0'};
	char *str;
	int high;
	int i, ret;
	unsigned long flags;

	if (len > PARA_BUF_LEN - 1) {
		pr_err("len %ld is too long\n", len);
		return -EINVAL;
	}

	if (copy_from_user(&kbuf, buff, len))
		return -EFAULT;

	kbuf[len] = '\0';

	str = strstrip(kbuf);
	if (!str) {
		pr_err("buff %s is invalid\n", kbuf);
		return -EINVAL;
	}

	if (pools == NULL)
		return -EINVAL;

	ret = sscanf(str, "%d", &high);

	if (ret == 1) {
		for (i = 0; i < POOL_MIGRATETYPE_TYPES_SIZE; i++) {
			spin_lock_irqsave(&pools->lock, flags);
			/* MIGRATETYPE: UNMOVABLE & MOVABLE */
			if (i == POOL_MIGRATETYPE_UNMOVABLE) {
				pools->high[i] = high / (POOL_MIGRATETYPE_TYPES_SIZE * 2);
			} else {
				pools->high[i] = high*3 / (POOL_MIGRATETYPE_TYPES_SIZE*2);
			}
			pools->low[i] = pools->high[i] / 2;
			spin_unlock_irqrestore(&pools->lock, flags);
			pr_info("%s order:%d migratetype:%d low: %d high: %d count:%d.\n",
					__func__, pools->order, i,
					pools->low[i], pools->high[i], pools->count[i]);
		}
		return len;
	}

	if (strstr(str, "pause")) {
		atomic_set(&vip_page_fillthread_setup, POOL_FILLTHREAD_OFF);
		return len;
	}

	if (strstr(str, "resume")) {
		atomic_set(&vip_page_fillthread_setup, POOL_FILLTHREAD_ON);
		return len;
	}

	return -EINVAL;
}

static ssize_t vip_page_pool_read(struct file *file,
								 char __user *buffer, size_t count, loff_t *off)
{
	char kbuf[PARA_BUF_LEN] = {'\0'};
	int len = 0;
	int i;

	if (pools == NULL)
		return -EINVAL;

	for (i = 0; i < POOL_MIGRATETYPE_TYPES_SIZE; i++) {
		len += snprintf(kbuf + len, PARA_BUF_LEN - len,
						"VIP POOL INFO: Order:%d migratetype:%d low: %d high: %d count:%d total_used:%lld pcp_used:%lld allocate_fail:%lld.\n",
						pools->order, i,
						pools->low[i], pools->high[i], pools->count[i],
						pools->total_used[i], pools->pcp_used[i], pools->allocate_fail[i]);
	}

	if (len == PARA_BUF_LEN)
		kbuf[len - 1] = '\0';

	if (len > *off)
		len -= *off;
	else
		return 0;

	if (copy_to_user(buffer, kbuf + *off, (len < count ? len : count)))
		return -EFAULT;

	*off += (len < count ? len : count);
	return (len < count ? len : count);
}

static const struct proc_ops vip_page_pool_fops = {
	.proc_write = vip_page_pool_write,
	.proc_read = vip_page_pool_read,
};

static int vip_page_pool_state_show(struct seq_file *m, void *arg)
{
	seq_printf(m,
		"vip page pool allocate state debug distribution:\n"
		"vip_page_pool_setup:                  %d\n"
		"vip_page_fillthread_setup:            %d\n"
		"nr_ux_binder_task_allocate:           %lu\n"
		"allocator_service_pid:                %d\n"
		"nr_skip_avoid_d:                      %lu\n",
		atomic_read(&vip_page_pool_setup),
		atomic_read(&vip_page_fillthread_setup),
		atomic_long_read(&nr_ux_binder_task_allocate),
		allocator_service_pid,
		atomic_long_read(&nr_skip_avoid_d)
	);
	seq_putc(m, '\n');

	return 0;
}

static int vip_page_pool_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, vip_page_pool_state_show, inode->i_private);
}

static ssize_t vip_page_pool_state_write(struct file *file,
										const char __user *buff, size_t len, loff_t *ppos)
{
	int ret;
	char kbuf[PARA_BUF_LEN] = {'\0'};
	char *str;
	int value;

	if (len > PARA_BUF_LEN - 1) {
		return -EINVAL;
	}

	if (copy_from_user(&kbuf, buff, len))
		return -EFAULT;

	kbuf[len] = '\0';

	str = strstrip(kbuf);
	if (!str){
		return -EINVAL;
	}

	ret = kstrtoint(str, 10, &value);
	if (ret){
		return -EINVAL;
	}

	if (value < 0 || value > INT_MAX) {
		return -EINVAL;
	}

	printk("vip_page_pool_setup is %d\n", value);
	atomic_set(&vip_page_pool_setup, value);

	return len;
}

static const struct proc_ops vip_page_pool_state_ops = {
	.proc_open = vip_page_pool_state_open,
	.proc_write = vip_page_pool_state_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int vip_page_pool_init(void)
{
	int ret = 0;
	struct proc_dir_entry *priority_allocate_dir = NULL;

	struct kprobe post_alloc_hook_kp = {
		.symbol_name = "post_alloc_hook"};

	pools = vip_page_pool_create((GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN |
									__GFP_NORETRY) & ~__GFP_RECLAIM,
								   orders, page_pool_nr_pages);

	init_waitqueue_head(&kworkthread_waitq);
	vip_page_pool_tsk = kthread_run(vip_page_pool_fillthread, NULL, VIP_PAGE_POOL_NAME);
	if (IS_ERR_OR_NULL(vip_page_pool_tsk)) {
		pr_err("%s:run vip_page_pool_fillthread failed!\n", __func__);
		kfree(pools);
		return -1;
	}

	/* Cloud Control enable the function */
	atomic_set(&vip_page_pool_setup, VIP_ALLOCATE_ON);

	/* Allow VIP memory pool threads to perform filling. */
	atomic_set(&vip_page_fillthread_setup, POOL_FILLTHREAD_ON);

	page_pool_wakeup_process(pools);

	priority_allocate_dir = proc_mkdir("priority_allocate", NULL);
	if (priority_allocate_dir == NULL) {
		pr_err("proc create priority_allocate_dir failed\n");
		vip_page_pool_destory();
		kthread_stop(vip_page_pool_tsk);
		vip_page_pool_tsk = NULL;
		return -1;
	}
	proc_create("vip_page_pool", 0666, priority_allocate_dir, &vip_page_pool_fops);
	proc_create("vip_page_pool_state", 0666, priority_allocate_dir, &vip_page_pool_state_ops);

	ret = vip_task_priority_init(priority_allocate_dir);
	if (ret < 0) {
		pr_err("priority setting node create failed!\n");
		vip_page_pool_destory();
		kthread_stop(vip_page_pool_tsk);
		vip_page_pool_tsk = NULL;
		return ret;
	}
	
	/* get post_alloc_hook addr init new pages */
	ret = register_kprobe(&post_alloc_hook_kp);
	if (ret < 0) {
		pr_err("get post_alloc_hook addr from kprobe failed!\n");
		vip_page_pool_destory();
		kthread_stop(vip_page_pool_tsk);
		vip_page_pool_tsk = NULL;
		return ret;
	}
	/* Obtain the function through the kprobe mechanism to enable calling the function. */
	post_alloc_hook_dup = (post_alloc_hook_t)post_alloc_hook_kp.addr;
	pr_info("suceesfully get post_alloc_hook addr:0x%px\n", post_alloc_hook_dup);

	unregister_kprobe(&post_alloc_hook_kp);

	/* record dma32_zone pageset if exist */
	init_dma32_pcp();

	return 0;
}

#if IS_ENABLED(CONFIG_TRANS_SCHED_FEATURE)
static inline bool current_is_allocator_service_task(struct task_struct *task)
{
	bool ret = false;
	struct mm_struct *mm;
	struct task_struct *group_leader;
	struct file *exe_file;

	if (!task || !task->mm)
		return ret;

	group_leader = task->group_leader;
	if (!group_leader || !group_leader->mm)
		return ret;

	//Reduce CPU consumption by avoiding repeated thread queries through group_leader PID determination.
	if (allocator_service_pid == group_leader->pid && allocator_service_pid != -1){
		ret = true;
		return ret;
	}

	mm = group_leader->mm;
	if (!mm)
		return ret;

	//direct copy get_mm_exe_file
	rcu_read_lock();
	exe_file = rcu_dereference(mm->exe_file);
	if (!exe_file) {
		rcu_read_unlock();
		return ret;
	}
	rcu_read_unlock();

	if (strstr(exe_file->f_path.dentry->d_name.name, "allocator-V2-service")){
		allocator_service_pid = group_leader->pid;
		ret = true;
	}

	return ret;
}

static inline bool current_is_ux_binder_task(struct task_struct *task)
{
	bool ret = false;

	if (!task || !g_trans_sched_interface.check_ux_task)
		return ret;

	if (strstr(task->comm, "binder") && g_trans_sched_interface.check_ux_task(task)) {
		if (!current_is_allocator_service_task(task))
			return ret;
		atomic_long_add(1, &nr_ux_binder_task_allocate);
		ret = true;
	}

	return ret;
}
#else
static inline bool current_is_ux_binder_task(struct task_struct *task)
{
	return false;
}
#endif

static inline bool current_is_vip_task(void)
{
	return task_is_high_priority(current) || current_is_ux_binder_task(current);
}

static void __nocfi get_page_from_vip_pages_pool(void *data, gfp_t gfp_mask, int order, int alloc_flags,
											int migratetype, struct page **p_page)
{
	struct page *page = NULL;
	int i;

	if (unlikely(atomic_read(&vip_page_pool_setup) == VIP_ALLOCATE_OFF))
		return;

	if (current_is_vip_task() && !(gfp_mask & __GFP_DMA32)) {
#ifdef VIP_PAGE_POOL_DEBUG
		pr_info("VIP Name %s Order %d migratetype %d\n", current->comm, order, migratetype);
#endif
		page = vip_page_pool_alloc_pages(order, migratetype, false);
		if (page) {
			/* prep new page begin */
			if (!page_count(page)) {
				if (post_alloc_hook_dup) {
					post_alloc_hook_dup(page, order, gfp_mask & ~(__GFP_DIRECT_RECLAIM));
				}else {
					set_page_private(page, 0);
					set_page_refcounted(page);

					for (i = 0; i < (1 << order); i++)
						clear_highpage(page + i);
				}
			}

			if (alloc_flags & ALLOC_NO_WATERMARKS)
				set_page_pfmemalloc(page);
			else
				clear_page_pfmemalloc(page);

			/* prep new page end */
			*p_page = page;
		}
	}
}

static void vip_pages_pool_refill(void *data, struct page *page, int order,
							 int migratetype, bool *bypass)
{
	if (unlikely(atomic_read(&vip_page_pool_setup) == VIP_ALLOCATE_OFF))
		return;

	struct zone *zone = page_zone(page);
	unsigned long mark = zone->_watermark[WMARK_LOW];
	long free_pages = zone_page_state(zone, NR_FREE_PAGES);

	free_pages -= zone->nr_reserved_highatomic;

	if ((migratetype <= MIGRATE_MOVABLE) && (zone_idx(zone) == ZONE_NORMAL) && free_pages > mark) {
		if (vip_page_pool_refill(page, order, migratetype))
			*bypass = true;
	}
}

static void kvmalloc_check_use_vmalloc(void *data, size_t size,
									   gfp_t *kmalloc_flags, bool *use_vmalloc)
{
	if (unlikely(atomic_read(&vip_page_pool_setup) == VIP_ALLOCATE_OFF))
		return;

	if (current_is_vip_task() && (size > VIP_MEM_POOL_MAX_PAGES * PAGE_SIZE)) {
#ifdef VIP_PAGE_POOL_DEBUG
		pr_info("VIP Name %s\n", current->comm);
#endif
		*kmalloc_flags |= __GFP_KSWAPD_RECLAIM;
		*use_vmalloc = false;
	}
}

static void should_alloc_pages_retry(void *data, gfp_t gfp_mask,
									 int order, int *alloc_flags, int migratetype,
									 struct zone *preferred_zone, struct page **p_page, bool *should_alloc_retry)
{
	if (unlikely(atomic_read(&vip_page_pool_setup) == VIP_ALLOCATE_OFF))
		return;

	if (unlikely(current_is_vip_task()) && !in_interrupt() &&
		(preferred_zone->nr_reserved_highatomic >= (SZ_8M >> PAGE_SHIFT)) &&
		!(*alloc_flags & (ALLOC_NON_BLOCK | ALLOC_OOM)) && 
		!(gfp_mask & __GFP_NORETRY) && !(gfp_mask & __GFP_NOMEMALLOC)) {
#ifdef VIP_PAGE_POOL_DEBUG
		pr_info("VIP Name %s Order %d migratetype %d\n", current->comm, order, migratetype);
#endif
		*alloc_flags |= ALLOC_NON_BLOCK;
		*should_alloc_retry = true;
	} else {
		*should_alloc_retry = false;
	}
}

static void fill_pcplist_from_vip_pages_pool(void *data, unsigned int order,
										struct per_cpu_pages *pcp, int migratetype, struct list_head *list)
{
	if (unlikely(atomic_read(&vip_page_pool_setup) == VIP_ALLOCATE_OFF))
		return;

	struct page *page = NULL;

	if (current_is_vip_task() && !in_dma32_zone(pcp)) {
#ifdef VIP_PAGE_POOL_DEBUG
		pr_info("VIP Name %s Order %d migratetype %d\n", current->comm, order, migratetype);
#endif
		page = vip_page_pool_alloc_pages(order,
				migratetype == get_cma_migrate_type() ? MIGRATE_MOVABLE : migratetype, true);
		if (page) {
			if (page_count(page))
				set_page_count(page, 0);
			list_add_tail(&page->lru, list);
			pcp->count += 1 << order;
		}
	}
}

static unsigned long zone_reclaimable_pages_local(struct zone *zone)
{
	unsigned long nr;

	nr = zone_page_state_snapshot(zone, NR_ZONE_INACTIVE_FILE) +
		zone_page_state_snapshot(zone, NR_ZONE_ACTIVE_FILE);
	nr += zone_page_state_snapshot(zone, NR_ZONE_INACTIVE_ANON) +
		zone_page_state_snapshot(zone, NR_ZONE_ACTIVE_ANON);
	/*
	 * If there are no reclaimable file-backed or anonymous pages,
	 * ensure zones with sufficient free pages are not skipped.
	 * This prevents zones like DMA32 from being ignored in reclaim
	 * scenarios where they can still help alleviate memory pressure.
	 */
	if (nr == 0)
		nr = zone_page_state_snapshot(zone, NR_FREE_PAGES);
	return nr;
}

bool memory_is_watermark_min_half(void)
{
	int i, nid;
	struct zone *zone;
	unsigned long pfmemalloc_reserve = 0;
	unsigned long free_pages = 0;
	bool wmark_ok;

	for_each_online_node(nid) {

		pg_data_t *pgdat = NODE_DATA(nid);

		for (i = 0; i <= ZONE_NORMAL; i++) {
			zone = &pgdat->node_zones[i];
			if (!managed_zone(zone))
				continue;

			if (!zone_reclaimable_pages_local(zone))
				continue;

			pfmemalloc_reserve += min_wmark_pages(zone);
			free_pages += zone_page_state_snapshot(zone, NR_FREE_PAGES);
		}
	}

	//Force system protection to prevent OOM
    if (free_pages <= pfmemalloc_reserve / 4) {
        wmark_ok = true;
    } else {
        wmark_ok = free_pages > pfmemalloc_reserve / 2;
    }

	return wmark_ok;
}

static void skip_waiting_pages(void *data, bool *bypass)
{
	if (unlikely(atomic_read(&vip_page_pool_setup) == VIP_ALLOCATE_OFF))
		return;

	if (current_is_vip_task() || rt_task(current)) {
		//min/4 < free <= min/2 skip
		if (unlikely(!memory_is_watermark_min_half())) {
			atomic_long_add(1, &nr_skip_avoid_d);
			*bypass = true;
		}
	}
}

static int register_vip_pages_pool_vendor_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_alloc_pages_reclaim_bypass(get_page_from_vip_pages_pool, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_alloc_pages_reclaim_bypass failed! ret=%d\n",
			   ret);
		goto out;
	}

	ret = register_trace_android_vh_free_unref_page_bypass(vip_pages_pool_refill, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_free_page failed! ret=%d\n", ret);
		goto out1;
	}

	ret = register_trace_android_vh_kvmalloc_node_use_vmalloc(kvmalloc_check_use_vmalloc, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_kvmalloc_node_use_vmalloc failed! ret=%d\n",
			   ret);
		goto out2;
	}

	ret = register_trace_android_vh_should_alloc_pages_retry(should_alloc_pages_retry, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_should_alloc_pages_retry failed! ret=%d\n", ret);
		goto out3;
	}

	ret = register_trace_android_vh_rmqueue_bulk_bypass(fill_pcplist_from_vip_pages_pool, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_rmqueue_bulk_bypass failed! ret=%d\n", ret);
		goto out4;
	}

	ret = register_trace_android_vh_throttle_direct_reclaim_bypass(skip_waiting_pages, NULL);
	if (ret != 0)
	{
		pr_err("register_trace_android_vh_throttle_direct_reclaim_bypass failed! ret=%d\n", ret);
		goto out5;
	}

	return ret;

out5:
	unregister_trace_android_vh_rmqueue_bulk_bypass(fill_pcplist_from_vip_pages_pool, NULL);

out4:
	unregister_trace_android_vh_should_alloc_pages_retry(should_alloc_pages_retry, NULL);
out3:
	unregister_trace_android_vh_kvmalloc_node_use_vmalloc(kvmalloc_check_use_vmalloc, NULL);
out2:
	unregister_trace_android_vh_free_unref_page_bypass(vip_pages_pool_refill, NULL);
out1:
	unregister_trace_android_vh_alloc_pages_reclaim_bypass(get_page_from_vip_pages_pool, NULL);

out:
	return ret;
}

static void unregister_vip_pages_pool_vendor_hooks(void)
{
	unregister_trace_android_vh_throttle_direct_reclaim_bypass(skip_waiting_pages, NULL);

	unregister_trace_android_vh_rmqueue_bulk_bypass(fill_pcplist_from_vip_pages_pool, NULL);

	unregister_trace_android_vh_should_alloc_pages_retry(should_alloc_pages_retry, NULL);

	unregister_trace_android_vh_kvmalloc_node_use_vmalloc(kvmalloc_check_use_vmalloc, NULL);

	unregister_trace_android_vh_free_unref_page_bypass(vip_pages_pool_refill, NULL);

	unregister_trace_android_vh_alloc_pages_reclaim_bypass(get_page_from_vip_pages_pool, NULL);
}

static int __init vip_pages_pool_init(void)
{
	int ret = 0;

	ret = vip_page_pool_init();
	if (ret != 0) {
		pr_err("vip_page_pool_init failed!\n");
		return ret;
	}

	ret = register_vip_pages_pool_vendor_hooks();
	if (ret != 0) {
		vip_page_pool_destory();
		return ret;
	}

	pr_info("vip_pages_pool_init succeed!\n");

	return 0;
}

static void __exit vip_pages_pool_exit(void)
{
	unregister_vip_pages_pool_vendor_hooks();
	vip_page_pool_destory();

	pr_info("vip_pages_pool_exit succeed!\n");
}

module_init(vip_pages_pool_init);
module_exit(vip_pages_pool_exit);

MODULE_LICENSE("GPL v2");
