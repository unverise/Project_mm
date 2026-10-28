/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2025-2027 Transsion. All rights reserved.
 * NOTICE: Directly copy the open-source Linux code.
 */
#ifndef __PRIORITY_ALLOCATE_H__
#define __PRIORITY_ALLOCATE_H__

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

//#define VIP_PAGE_POOL_DEBUG 0

/*
* static int fallbacks[MIGRATE_TYPES][3] = {
*  	[MIGRATE_UNMOVABLE]   = { MIGRATE_RECLAIMABLE, MIGRATE_MOVABLE,   MIGRATE_TYPES },
*  	[MIGRATE_MOVABLE]     = { MIGRATE_RECLAIMABLE, MIGRATE_UNMOVABLE, MIGRATE_TYPES },
*  	[MIGRATE_RECLAIMABLE] = { MIGRATE_UNMOVABLE,   MIGRATE_MOVABLE,   MIGRATE_TYPES },
* };
*/
enum POOL_MIGRATETYPE {
	POOL_MIGRATETYPE_UNMOVABLE,
	POOL_MIGRATETYPE_MOVABLE,
	POOL_MIGRATETYPE_TYPES_SIZE
};

/*
 * low: The memory pool will wake up the process to replenish the memory pool when it falls below this threshold.
 * high: The memory pool max space
 * count: The current size of the memory pool.
 * total_used: The total number of successful allocations from the VIP memory pool.
 * pcp_used: The number of successful bulk path allocations from the VIP memory pool.
 * allocate_fail: The number of failures to allocate from the VIP memory pool.
 * items: The memory pool maintains a linked list.
 * lock: Lock to ensure struct consistency.
 * order: Memory allocation order.
 * gfp_mask: Memory allocation GFP,determine memory allocation behavior.
 */
struct page_pool {
	int low[POOL_MIGRATETYPE_TYPES_SIZE];
	int high[POOL_MIGRATETYPE_TYPES_SIZE];
	int count[POOL_MIGRATETYPE_TYPES_SIZE];
	unsigned long long total_used[POOL_MIGRATETYPE_TYPES_SIZE];
	unsigned long long pcp_used[POOL_MIGRATETYPE_TYPES_SIZE];
	unsigned long long allocate_fail[POOL_MIGRATETYPE_TYPES_SIZE];
	struct list_head items[POOL_MIGRATETYPE_TYPES_SIZE];
	spinlock_t lock;
	unsigned int order;
	gfp_t gfp_mask[POOL_MIGRATETYPE_TYPES_SIZE];
};

/*
 * The VIP Allocates strategy contains multiple features and is subject to the limitations of cloud control.
 * 0 Turn OFF the functions
 * 1 Turn ON the  function
 */
enum vip_page_allocates {
	VIP_ALLOCATE_OFF = 0,
	VIP_ALLOCATE_ON,
};

/*
 * The VIP Allocates strategy contains multiple features and is subject to the limitations of cloud control.
 * 0 Turn OFF the functions
 * 1 Turn ON the  function
 */
enum pool_fillthread {
	POOL_FILLTHREAD_OFF = 0,
	POOL_FILLTHREAD_ON,
};

extern bool check_priority_allocate_feature(void);

#endif /* __PRIORITY_ALLOCATE_H__ */
 
