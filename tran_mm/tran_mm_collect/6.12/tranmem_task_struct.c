/**
  *Copyright 2025 Transsion Inc
  *Author transsioner
  *tran_mm_collect feature suppport
*/
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/sched.h>
#include <linux/sched/cputime.h>
#include <linux/sched/task.h>

#include <linux/slab.h>
#include <kernel/sched/sched.h>

#include <linux/slab.h>
#include <linux/version.h>
#include <trace/hooks/sched.h>

#include "tranmem_emem.h"
#include "tranmem_task_struct.h"

static struct tranmem_task_struct_data g_tranmem_task_struct_data = {
    .cache = NULL,
    .name = "tranmem_task_struct",
};

u64 tranmem_get_fail = 0;
struct tranmem_task_struct *get_tranmem_task_struct(struct task_struct *task)
{
    struct tranmem_task_struct *tranmem_ts = NULL;

    // task->ANDROID_OEM_DATA_ARRAY(1, xx);
    if (unlikely(IS_ERR_OR_NULL(task))) {
        pr_err(TRANMM "get_tranmem_task_struct task IS_ERR_OR_NULL\n");
    } else {
        tranmem_ts = (struct tranmem_task_struct *) READ_ONCE(task->android_oem_data1[TRANMEM_TASK_STRUCT_OFFSET]);
        if (IS_ERR_OR_NULL(tranmem_ts))
            tranmem_get_fail++;
    }

    return tranmem_ts;
}

struct task_struct *tran_find_get_task_by_vpid(pid_t nr)
{
	struct task_struct *task;

	rcu_read_lock();
	task = find_task_by_vpid(nr);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();

	return task;
}
EXPORT_SYMBOL_GPL(tran_find_get_task_by_vpid);

int tranmem_task_struct_alloc(struct task_struct *task)
{
    struct tranmem_task_struct *tranmem_ts = NULL;

    if (!task) {
        return -EFAULT;
    }

    tranmem_ts = kmem_cache_alloc(g_tranmem_task_struct_data.cache, GFP_ATOMIC);
    if (!tranmem_ts) {
        pr_err("tranmem_task_struct_alloc tranmem_task_struct alloc failed:%s/%d/%d, tts=0x%llx\n", task->comm, task->pid, task->tgid, (u64)tranmem_ts);
        return -ENOMEM;
    }

    WRITE_ONCE(task->android_oem_data1[TRANMEM_TASK_STRUCT_OFFSET], (u64)tranmem_ts);
    pr_debug("tranmem_task_struct alloc: %s/%d/%d, tranmem_ts=%p\n", task->comm, task->pid, task->tgid, tranmem_ts);

    return 0;
}

void android_vh_dup_task_struct_hook(void *unused,
        struct task_struct *tsk, struct task_struct *orig)
{
    struct tranmem_task_struct *tts = NULL;

    if (!tsk || !orig)
        return;

    /* The required space has been allocated */
    if (!IS_ERR_OR_NULL((void *)READ_ONCE(tsk->android_oem_data1[TRANMEM_TASK_STRUCT_OFFSET])))
        return;

    tts = kmem_cache_alloc(g_tranmem_task_struct_data.cache, GFP_ATOMIC);
    if (IS_ERR_OR_NULL(tts)) {
        pr_err("dup_task tranmem_task_struct alloc failed:%s/%d/%d, tts=0x%llx\n", tsk->comm, tsk->pid, tsk->tgid, (u64)tts);
        return;
    }

    memset(tts, 0, sizeof(struct tranmem_task_struct));
    tts->task = tsk;

    smp_mb();

    WRITE_ONCE(tsk->android_oem_data1[TRANMEM_TASK_STRUCT_OFFSET], (u64) tts);
}

void android_vh_free_task_hook(void *data, struct task_struct *task)
{
    struct tranmem_task_struct *tranmem_ts = NULL;

    if (!task)
        return;

    tranmem_ts = get_tranmem_task_struct(task);
    if (IS_ERR_OR_NULL(tranmem_ts))
        return;

    WRITE_ONCE(task->android_oem_data1[TRANMEM_TASK_STRUCT_OFFSET], 0);

    memset(tranmem_ts, 0, sizeof(struct tranmem_task_struct));

    smp_mb();

    kmem_cache_free(g_tranmem_task_struct_data.cache, tranmem_ts);
}

static void alloc_tts_for_all_threads(void)
{
    struct task_struct *p, *g;
    u32 iter_cpu;

    read_lock(&tasklist_lock);
    for_each_process_thread(g, p) {
        struct tranmem_task_struct *tts = NULL;

        tts = (struct tranmem_task_struct *) READ_ONCE(p->android_oem_data1[TRANMEM_TASK_STRUCT_OFFSET]);
        if (IS_ERR_OR_NULL(tts)) {
            tts = kmem_cache_alloc(g_tranmem_task_struct_data.cache, GFP_ATOMIC);

            if (!IS_ERR_OR_NULL(tts)) {
                tts->task = p;
                smp_mb();

                WRITE_ONCE(p->android_oem_data1[TRANMEM_TASK_STRUCT_OFFSET], (u64) tts);
            } else {
                pr_err(TRANMM "alloc_tts_for_all_threads: tranmem_task_struct alloc failed for %s/%d/%d, tts=0x%llx\n",
                    p->comm, p->pid, p->tgid, (u64)tts);
            }
        }
    }
    for_each_possible_cpu(iter_cpu) {
        struct tranmem_task_struct *tts = NULL;

        p = cpu_rq(iter_cpu)->idle;
        tts = (struct tranmem_task_struct *) READ_ONCE(p->android_oem_data1[TRANMEM_TASK_STRUCT_OFFSET]);
        if (IS_ERR_OR_NULL(tts)) {
            tts = kmem_cache_alloc(g_tranmem_task_struct_data.cache, GFP_ATOMIC);
            if (!IS_ERR_OR_NULL(tts)) {
                tts->task = p;
                smp_mb();

                WRITE_ONCE(p->android_oem_data1[TRANMEM_TASK_STRUCT_OFFSET], (u64) tts);
            } else {
                pr_err(TRANMM "alloc_tts_for_all_threads: tranmem_task_struct alloc failed for %s/%d/%d, tts=0x%llx\n",
                    p->comm, p->pid, p->tgid, (u64)tts);
            }
        }
    }
    read_unlock(&tasklist_lock);
}

static void init_tranmem_task_struct(void *ptr)
{
    struct tranmem_task_struct *tts = ptr;

    memset(tts, 0, sizeof(struct tranmem_task_struct));
}

int tranmem_task_struct_init(void)
{
    int ret = 0;

    pr_err(TRANMM" sizeof(struct tranmem_task_struct) is %d\n", (int) sizeof(struct tranmem_task_struct));
    g_tranmem_task_struct_data.cache = kmem_cache_create(g_tranmem_task_struct_data.name,
                        sizeof(struct tranmem_task_struct), 0, \
                        SLAB_PANIC|SLAB_ACCOUNT, init_tranmem_task_struct);
    if (!g_tranmem_task_struct_data.cache) {
        return -ENOMEM;
    }

    ret = register_trace_android_vh_dup_task_struct(android_vh_dup_task_struct_hook, NULL);
    if (ret) {
        pr_err(TRANMM"register android_vh_dup_task_struct_handler failed!\n");
		return ret;
    }

    ret = register_trace_android_vh_free_task(android_vh_free_task_hook, NULL);
    if (ret) {
        pr_err(TRANMM"register android_vh_free_task_hook failed!\n");
		unregister_trace_android_vh_dup_task_struct(android_vh_dup_task_struct_hook, NULL);
		return ret;
    }

    /*
     * alloc tranmem_task_struct for all tasks.
     */
    alloc_tts_for_all_threads();

    return ret;
}

void tranmem_task_struct_deinit(void)
{
    unregister_trace_android_vh_dup_task_struct(android_vh_dup_task_struct_hook, NULL);
    unregister_trace_android_vh_free_task(android_vh_free_task_hook, NULL);

    kmem_cache_destroy(g_tranmem_task_struct_data.cache);
}
