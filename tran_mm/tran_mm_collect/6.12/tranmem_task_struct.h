#ifndef __TRANMEM_TASK_STRUCT_H
#define __TRANMEM_TASK_STRUCT_H
/**
  *Copyright 2023 Transsion Inc
  *Author transsioner
  *tran_mm_collect feature suppport
*/
#include <linux/printk.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/sched/cputime.h>
#include <kernel/sched/sched.h>
#include "tranmem_emem.h"

#define TRANMEM_TASK_STRUCT_OFFSET (1)
#define TRANMEM_MANAGEMENT_OFFSET (2)

#define TRANMEM_TASK_STRUCT_COMM_LEN (20)

#define TRANMEM_ASSERT(tranmem_task_struct, tranmem_task_struct_offset, kernel_struct)  \
    BUILD_BUG_ON((sizeof(tranmem_task_struct) + tranmem_task_struct_offset) > (sizeof(u64) *  \
       ARRAY_SIZE(((kernel_struct *)0)->android_oem_data1)))

enum emem_page_fault_time_type {
	PAGE_FAULT_TIME_DISABLE = 0,        // disable page fault time monitoring
	PAGE_FAULT_TIME_ONLY_CRITICAL,      // only record the time of critical process's page fault
	PAGE_FAULT_TIME_ALL_PROCESS,        // record the time of all process's page fault
	PAGE_FAULT_TIME_INVALID,            // invalid value
};

struct task_priority_struct {
	bool high_priority;
};

struct tranmem_task_struct_data {
    struct kmem_cache *cache;
    char name[TRANMEM_TASK_STRUCT_COMM_LEN];
};

struct tranmem_task_struct {
	// Whether this thread has been monitored of page fault information
	bool calc_status;
	// The total number of page fault exception (single execution time is more than 1ms)
	u32 flt_excep_count;
	// The total number of page fault exception (single execution time is more than 1ms)
	u32 flt_concep_count;
	// The task of this thread
	struct task_struct *task;
	// The total page fault excution until last update(update each 10-TRAN_INTERVAL_SECOND seconds)
	u64 flt_time_old;
	// The total page fault excution until now
	u64 flt_time;
	// The total page fault execution until last update(update each 10-TRAN_INTERVAL_SECOND seconds)
	u64 flt_time_execution_old;
	// The total page fault execution until last update(update each 10-TRAN_INTERVAL_SECOND seconds)
	u64 flt_time_consume_old;
	// The total page fault execution time until now
	u64 flt_execution_time;
	// The total page fault consumed time until now
	u64 flt_consume_time;
	// The total minor page fault times until last update(update each 10-TRAN_INTERVAL_SECOND seconds)
	u64 min_flt_old;
	// The total major page fault times until last update(update each 10-TRAN_INTERVAL_SECOND seconds)
	u64 maj_flt_old;
	// The total minor page fault times durning the last update period(update each 10-TRAN_INTERVAL_SECOND seconds)
	u64 min_flt_delta;
	// The total major page fault times durning the last update period(update each 10-TRAN_INTERVAL_SECOND seconds)
	u64 maj_flt_delta;
	// The direct reclaim count durning the last update period(update each 10-TRAN_INTERVAL_SECOND seconds)
	atomic64_t direct_reclaim_count;
	// The direct reclaim start time
	u64 direct_reclaim_start_time;
	// The direct reclaim time durning the last update period(update each 10-TRAN_INTERVAL_SECOND seconds)
	atomic64_t direct_reclaim_gap_time;
} ____cacheline_aligned;

extern struct task_struct *tran_find_get_task_by_vpid(pid_t nr);

struct tranmem_task_struct *get_tranmem_task_struct(struct task_struct *task);

static inline struct task_priority_struct *get_task_priority_struct(struct task_struct *task)
{
    if (!task)
        return NULL;

    // task->ANDROID_OEM_DATA_ARRAY(1, xx);
    return (struct task_priority_struct *)&(task->android_oem_data1[TRANMEM_MANAGEMENT_OFFSET]);
}

static inline bool is_critical(struct task_struct *task)
{
    struct task_priority_struct *task_priority = get_task_priority_struct(task);
    if (IS_ERR_OR_NULL(task_priority)) {
        return false;
    }

    return task_priority->high_priority;
}

int tranmem_task_struct_init(void);
void tranmem_task_struct_deinit(void);

#endif   //__TRANMEM_TASK_STRUCT_H
