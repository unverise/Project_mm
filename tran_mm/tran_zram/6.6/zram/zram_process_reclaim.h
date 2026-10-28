/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2025-2027 Transsion. All rights reserved.
 */

#ifndef __ZRAM_PROCESS_RECLAIM_H__
#define __ZRAM_PROCESS_RECLAIM_H__

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/pagewalk.h>
#include <linux/types.h>
#include <asm/atomic.h>

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT

#define ZRAM_REASON_CANNOT_CPRS (99)
enum zram_cancel_madvise {
	ZRAM_CANCEL_BY_NONE = 0,
	ZRAM_CANCEL_BY_SIGNAL,
	ZRAM_CANCEL_BY_TIME,
	ZRAM_CANCEL_BY_SIZE,
	ZRAM_CANCEL_BY_PROP,
	ZRAM_CANCEL_BY_MMAP_LOCK,
	ZRAM_CANCEL_BY_RUNNING,
	ZRAM_CANCEL_MAX
};

/*
 * Please ensure that struct process_reclaim_stat is consistent with the definition in
 * services/core/jni/com_android_server_am_CachedAppOptimizer.cpp
 */
struct pid_reclaim_stat {
	int pid;
	int log_enable;
	unsigned long rss_anon; /*same as /proc/pid/status RssAnon*/
	unsigned long vm_swap; /*same as /proc/pid/status VmSwap*/
	unsigned long vm_rss; /*same as /proc/pid/status VmRss*/
	unsigned long vm_lock; /*same as /proc/pid/status VmLck*/
};

struct process_reclaim_stat {
	int cmd;
	int check_size;
	int log_enable; /*0 disalbe 1 enable log*/
	unsigned int thrash1; /*recent 1s to now , in pages/s */
	unsigned int thrash2; /*recent 2s to recent 1s , in pages/s */
	unsigned int thrash3; /*recent 3s to recent 2s , in pages/s */

	unsigned long totalswap;/*in KB*/
	unsigned long freeswap;/*in KB*/

	unsigned long sharedram;/*in KB*/
	unsigned long zram_phy_used;/*in KB*/

	unsigned long anon_active;/*in KB*/
	unsigned long anon_inactive;/*in KB*/
	unsigned long file_active;/*in KB*/
	unsigned long file_inactive;/*in KB*/
	unsigned long unevictable;/*in KB*/

	int64_t fwk_compress_pages_since; /* no. of pages since zram set up */
	int64_t fwk_compress_freq_since; /* no. of compressions since zram set up */
	int64_t fwk_compress_runtime_since; /* compressson runtime on cpu since zram set up, unit is us */
	int64_t fwk_decompress_pages_since; /* no. of decompressions since zram set up */
};

struct zram_compress_single{
	s64 compressed_snapshot;/* record compressed data size when compression beginning. */
	u64 start_time;/* compression beginning timestamp , unit is ns*/
	u64 sum_exec_runtime; /* compression thread running timestamp , unit is ns*/
	u64 cprs_data_limit;/* compression data limit, unit is pages*/
	u64 cprs_time_limit;/* compression time limit, unit is ns*/
	u64 cprs_count;
	int cprs_prop_limit; /* proportion of process memory compression */
	struct task_struct * target_task; /* Compressed target process */
	int frozen; /* target process frozen state */
	int frozen_chg; /* no. of changes in the frozen state of the process during the entire compression process. */
	pid_t target_pid; /* Compressed target pid */
};

#define ZRAM_WB_LOG 0
#define ZRAM_READ_LOG 1
#define ZRAM_CGROUP_LOG 2

#define ZRAM_COMPRESS_LOG 4
#define ZRAM_VMA_DEBUG_LOG 5
#define ZRAM_FREE_LOG 6

extern __read_mostly int zram_wbd_debug;
#define is_log_enable(flag) unlikely(zram_wbd_debug & BIT(flag))
#define clear_log_enable(flag) unlikely(zram_wbd_debug & (~BIT(flag)))

extern __read_mostly bool zram_reclaim_shared_page_enable;
extern __read_mostly bool g_process_reclaim_enable;
extern atomic64_t fwk_compress_freq_since;
extern atomic64_t fwk_compress_pages_since;
extern atomic64_t fwk_decompress_pages_since;
extern atomic64_t fwk_compress_runtime_since;
extern int zram_last_compressed_abort;
extern struct task_struct *zram_compressed_task;
extern int zram_process_reclaim_init(struct proc_dir_entry *memfusion_dir_entry);
extern void zram_process_reclaim_exit(void);
extern struct zram_compress_single * get_compress_params(void);
int zram_process_reclaim_stat(struct process_reclaim_stat *stat);

#endif

#endif /* __ZRAM_PROCESS_RECLAIM_H__ */
