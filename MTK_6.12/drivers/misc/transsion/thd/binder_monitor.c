/*Transsion Top Secret*/
/*
 * Copyright (C) 2025 Transsion Inc.
 *
 * @author chonglin.yang
 */
#include <uapi/linux/android/binder.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <trace/hooks/binder.h>
#include <drivers/android/binder_internal.h>
#include "thd_util.h"
#include "aegisk.h"


#define MODULE_NAME "binder_monitor"
#define MAX_TRANSACTIONS 256
#define SYSTEM_SERVER_NAME "system_server"

/* 监控阈值 */
#define SYNC_MEM_USAGE_THRESHOLD 95  /* % */
#define ASYNC_MEM_AVAIL_THRESHOLD 5  /* % */
#define THREAD_ACTIVITY_THRESHOLD 100  /* 31/31 */
#define THRESHOLD_DURATION_MS 3000
#define MAX_THRESHOLD_TRIGGER_COUNT 12

/* 事务记录结构体 */
struct transaction_record {
    pid_t sender_pid;
    pid_t sender_tid;
    pid_t target_pid;
    pid_t target_tid;
    char sender_comm[TASK_COMM_LEN];       /* 发送端进程名字 */
    char sender_thread_comm[TASK_COMM_LEN]; /* 发送端线程名字 */
    char target_thread_comm[TASK_COMM_LEN]; /* 目标线程名字 */
    size_t mem_size;
    uint32_t code;
    bool is_oneway;
    uint64_t debug_id;                    /* debug id */
};

/* 循环buffer结构体 */
struct transaction_buffer {
    struct transaction_record *records;
    size_t head;
    size_t tail;
    size_t count;
    size_t max_size;
    spinlock_t lock;
};

/* 监控状态结构体 */
struct monitor_state {
    pid_t system_server_pid;
    struct binder_proc *system_server_proc;
    struct transaction_buffer trans_buffer;

    /* 内存状态 */
    size_t sync_total_space;
    size_t sync_used_space;
    size_t async_total_space;
    size_t async_free_space;

    /* 阈值触发状态 */
    ktime_t sync_mem_threshold_start;
    bool sync_mem_threshold_triggered;
    int sync_mem_threshold_trigger_count;

    ktime_t async_mem_threshold_start;
    bool async_mem_threshold_triggered;
    int async_mem_threshold_trigger_count;

    ktime_t thread_activity_threshold_start;
    bool thread_activity_threshold_triggered;
    int thread_activity_threshold_trigger_count;

    /* 保护system_server_pid的锁 */
    spinlock_t proc_lock;

    /* 保护内存和线程状态的锁 */
    spinlock_t state_lock;

    /* 1秒轮询定时器 */
    struct timer_list check_timer;
    struct work_struct check_work;
};

/* 异步工作类型 */
enum monitor_work_type {
    WORK_ADD_TRANSACTION,
    WORK_REMOVE_TRANSACTION,
    WORK_RESET_SYSTEM_SERVER
};

/* 异步工作结构体 */
struct monitor_work {
    struct work_struct work;
    enum monitor_work_type type;
    union {
        struct {
            struct transaction_record rec;
            size_t sync_total_space;
            size_t async_total_space;
            size_t free_async_space;
            size_t data_size;
            size_t offsets_size;
            size_t extra_buffers_size;
        } add_trans;

        struct {
            uint64_t debug_id; /* debug id */
        } remove_trans;

        struct {
            struct binder_proc *proc;
        } reset_system_server;
    } data;
};

static struct monitor_state *g_monitor = NULL;
static struct workqueue_struct *monitor_wq = NULL;

#define binder_inner_proc_lock(proc) _binder_inner_proc_lock(proc, __LINE__)
static void
_binder_inner_proc_lock(struct binder_proc *proc, int line)
	__acquires(&proc->inner_lock)
{
	spin_lock(&proc->inner_lock);
}

#define binder_inner_proc_unlock(proc) _binder_inner_proc_unlock(proc, __LINE__)
static void
_binder_inner_proc_unlock(struct binder_proc *proc, int line)
	__releases(&proc->inner_lock)
{
	spin_unlock(&proc->inner_lock);
}

/* 初始化循环buffer */
int init_transaction_buffer(struct transaction_buffer *buffer, size_t max_size)
{
    buffer->records = kmalloc_array(max_size, sizeof(struct transaction_record), GFP_KERNEL);
    if (!buffer->records) {
        return -ENOMEM;
    }

    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;
    buffer->max_size = max_size;
    spin_lock_init(&buffer->lock);

    return 0;
}

/* 销毁循环buffer */
void destroy_transaction_buffer(struct transaction_buffer *buffer)
{
    kfree(buffer->records);
    buffer->records = NULL;
}

/* 添加事务记录到循环buffer */
void add_transaction_record(struct transaction_buffer *buffer, struct transaction_record *record)
{
    unsigned long flags;

    spin_lock_irqsave(&buffer->lock, flags);

    /* 复制记录到buffer */
    memcpy(&buffer->records[buffer->head], record, sizeof(struct transaction_record));

    /* 更新buffer指针 */
    buffer->head = (buffer->head + 1) % buffer->max_size;

    if (buffer->count < buffer->max_size) {
        buffer->count++;
    } else {
        /* buffer已满，移动tail指针 */
        buffer->tail = (buffer->tail + 1) % buffer->max_size;
        //pr_info("%s: Transaction buffer full, discarding oldest record.\n", MODULE_NAME);
    }

    spin_unlock_irqrestore(&buffer->lock, flags);
}

/* 从循环buffer中移除事务记录 */
bool remove_transaction_record(struct transaction_buffer *buffer, uint64_t debug_id,struct transaction_record *record_out)
{
    unsigned long flags;
    size_t i, idx;
    bool found = false;

    spin_lock_irqsave(&buffer->lock, flags);

    for (i = 0; i < buffer->count; i++) {
        idx = (buffer->tail + i) % buffer->max_size;
        if (buffer->records[idx].debug_id == debug_id) {
            /* 找到匹配的记录*/
            if(record_out) {
                memcpy(record_out,&buffer->records[idx], sizeof(struct transaction_record));
            }

            /* 优化：将最后一个元素复制到当前位置，避免逐个移动 */
            size_t last_idx = (buffer->head + buffer->max_size - 1) % buffer->max_size;
            if (i != buffer->count - 1) {  /* 如果不是最后一个元素才需要复制 */
                memcpy(&buffer->records[idx], &buffer->records[last_idx], sizeof(struct transaction_record));
            }

            /* 清除最后一个元素内容(提高安全性)*/
            memset(&buffer->records[last_idx], 0, sizeof(struct transaction_record));

            /* 更新head指针和count */
            buffer->count--;
            if (buffer->count == 0) {
                /* 缓冲区为空，重置头尾指针 */
                buffer->head = 0;
                buffer->tail = 0;
            } else {
                /* 更新head指针指向新的最后一个元素 */
                buffer->head = (buffer->head + buffer->max_size - 1) % buffer->max_size;
            }

            found = true;
            break;
        }
    }

    spin_unlock_irqrestore(&buffer->lock, flags);

    return found;
}

void post_binder_loading_to_aegis(struct transaction_record *rec){
	char buffer[256] = {'\0'};
    if(!rec) {
        return;
    }
	snprintf(buffer, sizeof(buffer), "from %s,%s to %s ,%d:%d code 0x:%x to %d:%d ,debug_id:0x%llx,mem_size:%zu,oneway:%d\n",
							rec->sender_comm,rec->sender_thread_comm,rec->target_thread_comm,rec->sender_pid, rec->sender_tid, rec->code,
                            rec->target_pid,rec->target_tid,rec->debug_id,rec->mem_size,rec->is_oneway);

    //pr_err("%s,post_binder_loading_to_aegis: %s", MODULE_NAME, buffer);

	uploadBinderInfo2Aegis("append_binder_loading", buffer, "");
}
void clear_aegis_binder_loading(void){
	uploadBinderInfo2Aegis("clear_binder_loading", "", "");
}

/* 打印所有事务记录 */
void upload_transaction_records(struct transaction_buffer *buffer)
{
    unsigned long flags;
    size_t i, idx;

    spin_lock_irqsave(&buffer->lock, flags);

    pr_err("%s: Printing %zu pending transactions:\n", MODULE_NAME, buffer->count);
    if(buffer->count > 0) {
        clear_aegis_binder_loading();
    }
    for (i = 0; i < buffer->count; i++) {
        idx = (buffer->tail + i) % buffer->max_size;
        struct transaction_record *rec = &buffer->records[idx];
        post_binder_loading_to_aegis(rec);
    }

    spin_unlock_irqrestore(&buffer->lock, flags);
}

/* 检查是否为system_server进程 */
bool is_system_server(struct task_struct *task)
{
    if (task) {
        return strcmp(task->comm, SYSTEM_SERVER_NAME) == 0;
    }
    return false;
}

/* 更新system_server进程信息 - 同步执行，因为其他逻辑依赖于此 */
void update_system_server_info(struct binder_proc *proc)
{
    unsigned long flags;

    if (!proc || !proc->tsk) {
        return;
    }

    if (is_system_server(proc->tsk)) {
        spin_lock_irqsave(&g_monitor->proc_lock, flags);

        /* 只有当proc与当前system_server_pid不同时，才更新并重置数据 */
        if (proc->pid != g_monitor->system_server_pid) {
            spin_unlock_irqrestore(&g_monitor->proc_lock, flags);

            /* 重置事务缓冲区 */
            spin_lock(&g_monitor->trans_buffer.lock);
            g_monitor->trans_buffer.head = 0;
            g_monitor->trans_buffer.tail = 0;
            g_monitor->trans_buffer.count = 0;
            spin_unlock(&g_monitor->trans_buffer.lock);

            /* 重置内存状态 */
            spin_lock_irqsave(&g_monitor->state_lock, flags);
            g_monitor->sync_total_space = 0;
            g_monitor->sync_used_space = 0;
            g_monitor->async_total_space = 0;
            g_monitor->async_free_space = 0;

            /* 重置阈值触发状态 */
            g_monitor->sync_mem_threshold_start = 0;
            g_monitor->sync_mem_threshold_trigger_count = 0;
            g_monitor->sync_mem_threshold_triggered = false;

            g_monitor->async_mem_threshold_start = 0;
            g_monitor->async_mem_threshold_trigger_count = 0;
            g_monitor->async_mem_threshold_triggered = false;

            g_monitor->thread_activity_threshold_start = 0;
            g_monitor->thread_activity_threshold_trigger_count = 0;
            g_monitor->thread_activity_threshold_triggered = false;
            spin_unlock_irqrestore(&g_monitor->state_lock, flags);

            spin_lock_irqsave(&g_monitor->proc_lock, flags);
            /* 更新system_server信息 */
            g_monitor->system_server_pid = task_pid_nr(proc->tsk);
            g_monitor->system_server_proc = proc;
            spin_unlock_irqrestore(&g_monitor->proc_lock, flags);

            pr_info("%s: Updated system_server PID to %d, reset all monitor data\n",
                   MODULE_NAME, g_monitor->system_server_pid);
        } else {
            /* 无需更新，直接解锁 */
            spin_unlock_irqrestore(&g_monitor->proc_lock, flags);
        }
    }
}

int getCountTimes(int count){
    int times = 1;
    if(count > 0){
        times *= (1 << (count-1));
    }
    return times;
}
/* 检查监控阈值 - 同步内存 */
bool check_sync_mem_threshold(void)
{
    bool triggered = false;
    ktime_t now = ktime_get();
    unsigned long flags;

    /* 计算同步空间内存使用率 */
    size_t sync_mem_threshold = 0;
    spin_lock_irqsave(&g_monitor->state_lock, flags);

    if (g_monitor->sync_total_space > 0) {
        sync_mem_threshold = (g_monitor->sync_total_space*SYNC_MEM_USAGE_THRESHOLD)/100;

        /*pr_err("%s: sync memory usage threshold:%zu, sync_total_space:%zu, sync_used_space:%zu\n",
            MODULE_NAME,sync_mem_threshold,g_monitor->sync_total_space,g_monitor->sync_used_space);*/

        /* 检查同步内存使用率阈值 */
        if (g_monitor->sync_used_space >= sync_mem_threshold) {
            if (!g_monitor->sync_mem_threshold_triggered) {
                g_monitor->sync_mem_threshold_start = now;
                g_monitor->sync_mem_threshold_triggered = true;
                g_monitor->sync_mem_threshold_trigger_count++;
            } else {
                s64 duration = ktime_to_ms(ktime_sub(now, g_monitor->sync_mem_threshold_start));
                if (duration >= (THRESHOLD_DURATION_MS*getCountTimes(g_monitor->sync_mem_threshold_trigger_count))) {
                    pr_err("%s: sync memory usage threshold exceeded:%zu for %lldms\n",
                        MODULE_NAME, g_monitor->sync_used_space,duration);
                    triggered = true;
                    g_monitor->sync_mem_threshold_start = now;
                    g_monitor->sync_mem_threshold_trigger_count++;
                    if(g_monitor->sync_mem_threshold_trigger_count > MAX_THRESHOLD_TRIGGER_COUNT){
                        g_monitor->sync_mem_threshold_triggered = false;
                        g_monitor->sync_mem_threshold_trigger_count = 0;
                    }
                }
            }
        } else {
            g_monitor->sync_mem_threshold_triggered = false;
            g_monitor->sync_mem_threshold_trigger_count = 0;
        }
    }

    spin_unlock_irqrestore(&g_monitor->state_lock, flags);

    return triggered;
}

/* 检查监控阈值 - 异步内存 */
bool check_async_mem_threshold(void)
{
    bool triggered = false;
    ktime_t now = ktime_get();
    unsigned long flags;

    /* 计算异步空间内存可用率 */
    size_t async_mem_avail_threshold = 0;
    spin_lock_irqsave(&g_monitor->state_lock, flags);

    if (g_monitor->async_total_space > 0) {
        async_mem_avail_threshold = (g_monitor->async_total_space*ASYNC_MEM_AVAIL_THRESHOLD)/100;

        /*pr_err("%s: async memory available threshold:%zu, async_total_space:%zu, async_free_space:%zu\n",
            MODULE_NAME,async_mem_avail_threshold,g_monitor->async_total_space,g_monitor->async_free_space);*/

        /* 检查异步内存可用率阈值 */
        if (g_monitor->async_free_space <= async_mem_avail_threshold) {
            if (!g_monitor->async_mem_threshold_triggered) {
                g_monitor->async_mem_threshold_start = now;
                g_monitor->async_mem_threshold_triggered = true;
                g_monitor->async_mem_threshold_trigger_count++;
            } else {
                s64 duration = ktime_to_ms(ktime_sub(now, g_monitor->async_mem_threshold_start));
                if (duration >= (THRESHOLD_DURATION_MS*getCountTimes(g_monitor->async_mem_threshold_trigger_count))) {
                    pr_err("%s: async memory available threshold exceeded: %zu for %lldms\n",
                        MODULE_NAME, g_monitor->async_free_space,duration);
                    triggered = true;
                    g_monitor->async_mem_threshold_start = now;
                    g_monitor->async_mem_threshold_trigger_count++;
                    if(g_monitor->async_mem_threshold_trigger_count > MAX_THRESHOLD_TRIGGER_COUNT){
                        g_monitor->async_mem_threshold_triggered = false;
                        g_monitor->async_mem_threshold_trigger_count = 0;
                    }

                }
            }
        } else {
            g_monitor->async_mem_threshold_triggered = false;
            g_monitor->async_mem_threshold_trigger_count = 0;
        }
    }
    spin_unlock_irqrestore(&g_monitor->state_lock, flags);

    return triggered;
}

/* 检查监控阈值 - 线程活跃度 */
bool check_thread_activity_threshold(void)
{
    bool triggered = false;
    ktime_t now = ktime_get();
    unsigned long flags;
    int max_threads = 0;
    int running_count = 0;
    int thread_activity_threshold = 0;
    struct rb_node *n;

    spin_lock_irqsave(&g_monitor->proc_lock, flags);
    if (!g_monitor->system_server_pid || g_monitor->system_server_proc == NULL) {
        spin_unlock_irqrestore(&g_monitor->proc_lock, flags);
        return false;
    }
    binder_inner_proc_lock(g_monitor->system_server_proc);
    max_threads = g_monitor->system_server_proc->max_threads;
    for (n = rb_first(&g_monitor->system_server_proc->threads); n != NULL; n = rb_next(n)) {
		struct binder_thread *thread = rb_entry(n, struct binder_thread, rb_node);
        if (READ_ONCE(thread->task->__state) == TASK_RUNNING) {
            running_count++;
        }
	}
    binder_inner_proc_unlock(g_monitor->system_server_proc);
    spin_unlock_irqrestore(&g_monitor->proc_lock, flags);
    if (max_threads <= 0) {
        return false;
    }

    /* 计算线程活跃度阈值  */
    thread_activity_threshold = (max_threads*THREAD_ACTIVITY_THRESHOLD)/100;

    /*pr_err("%s: thread activity threshold:%d, max_threads:%d, running_count:%d\n",
        MODULE_NAME,thread_activity_threshold,max_threads,running_count);*/

    /* 检查线程活跃度阈值 */
    if (running_count >= thread_activity_threshold) {
        if (!g_monitor->thread_activity_threshold_triggered) {
            g_monitor->thread_activity_threshold_start = now;
            g_monitor->thread_activity_threshold_triggered = true;
            g_monitor->thread_activity_threshold_trigger_count++;
        } else {
            s64 duration = ktime_to_ms(ktime_sub(now, g_monitor->thread_activity_threshold_start));
            if (duration >= (THRESHOLD_DURATION_MS*getCountTimes(g_monitor->thread_activity_threshold_trigger_count))) {
                pr_err("%s: thread activity threshold exceeded: %d for %lldms\n",
                       MODULE_NAME, running_count, duration);
                triggered = true;
                g_monitor->thread_activity_threshold_start = now;
                g_monitor->thread_activity_threshold_trigger_count++;
                if(g_monitor->thread_activity_threshold_trigger_count > MAX_THRESHOLD_TRIGGER_COUNT){
                    g_monitor->thread_activity_threshold_triggered = false;
                    g_monitor->thread_activity_threshold_trigger_count = 0;
                }

            }
        }
    } else {
        g_monitor->thread_activity_threshold_triggered = false;
        g_monitor->thread_activity_threshold_trigger_count = 0;
    }

    return triggered;
}

/* 检查监控阈值并打印日志 - 可选择检查类型 */
void check_monitor_thresholds(void)
{
    bool sync_mem_triggered = false;
    bool async_mem_triggered = false;
    bool thread_activity_triggered = false;
    unsigned long flags;
    char buffer[64] = {'\0'};
    char pids[64] = {'\0'};
    int trigger_type = 0;

    spin_lock_irqsave(&g_monitor->proc_lock, flags);
    if (!g_monitor->system_server_pid) {
        spin_unlock_irqrestore(&g_monitor->proc_lock, flags);
        return;
    }
    snprintf(pids, sizeof(pids), "%d\n",g_monitor->system_server_pid);
    spin_unlock_irqrestore(&g_monitor->proc_lock, flags);

     // 检查同步内存
     sync_mem_triggered |= check_sync_mem_threshold();
     if(sync_mem_triggered){
        trigger_type |= 1<<3;
     }

    // 检查异步内存
    async_mem_triggered |= check_async_mem_threshold();
    if(async_mem_triggered){
        trigger_type |= 1<<2;
    }

    // 检查线程活跃度
    thread_activity_triggered |= check_thread_activity_threshold();
    if(thread_activity_triggered){
        trigger_type |= 1<<1;
    }

    if (sync_mem_triggered || async_mem_triggered || thread_activity_triggered) {
        upload_transaction_records(&g_monitor->trans_buffer);
        snprintf(buffer, sizeof(buffer), "trigger_type:%d\n",trigger_type);
        startAegisK("BINDER_LOADING_EXCEEDED", pids, buffer);
    }
}

/* 轮询检查工作队列处理函数 */
void check_thresholds_work_func(struct work_struct *work)
{
    /* 检查所有阈值 */
    check_monitor_thresholds();
}

/* 定时器回调函数 */
void check_timer_callback(struct timer_list *t)
{
    struct monitor_state *monitor = container_of(t, struct monitor_state, check_timer);

    /* 只有当system_server_pid已初始化时才执行检查 */
    if (monitor->system_server_pid) {
        /* 将检查任务提交到工作队列 */
        schedule_work(&monitor->check_work);
    }

    /* 重新设置定时器，1秒后再次触发 */
    mod_timer(&monitor->check_timer, jiffies + msecs_to_jiffies(1000));
}

/* Calculate the sanitized total size, returns 0 for invalid request */
static inline size_t sanitized_size(size_t data_size,
				    size_t offsets_size,
				    size_t extra_buffers_size)
{
	size_t total, tmp;

	/* Align to pointer size and check for overflows */
	tmp = ALIGN(data_size, sizeof(void *)) +
		ALIGN(offsets_size, sizeof(void *));
	if (tmp < data_size || tmp < offsets_size)
		return 0;
	total = tmp + ALIGN(extra_buffers_size, sizeof(void *));
	if (total < tmp || total < extra_buffers_size)
		return 0;

	/* Pad 0-sized buffers so they get a unique address */
	total = max(total, sizeof(void *));

	return total;
}

/* 异步工作处理函数 */
void monitor_work_handler(struct work_struct *work)
{
    struct monitor_work *monitor_work = container_of(work, struct monitor_work, work);

    switch (monitor_work->type) {
        case WORK_ADD_TRANSACTION: {
            unsigned long flags;
            struct transaction_record *rec = &monitor_work->data.add_trans.rec;

            size_t total_size = sanitized_size(monitor_work->data.add_trans.data_size,
                    monitor_work->data.add_trans.offsets_size, monitor_work->data.add_trans.extra_buffers_size);
            if (total_size == 0) {
                pr_err("%s: invalid transaction size, data_size:%zu, offsets_size:%zu, extra_buffers_size:%zu\n",
                       MODULE_NAME, monitor_work->data.add_trans.data_size, monitor_work->data.add_trans.offsets_size,
                        monitor_work->data.add_trans.extra_buffers_size);
                break;
            }

            rec->mem_size = total_size;

            spin_lock_irqsave(&g_monitor->state_lock, flags);
            if(monitor_work->data.add_trans.async_total_space != g_monitor->async_total_space
                        || monitor_work->data.add_trans.sync_total_space != g_monitor->sync_total_space) {

                g_monitor->async_total_space = monitor_work->data.add_trans.async_total_space;
                g_monitor->sync_total_space = monitor_work->data.add_trans.sync_total_space;
                g_monitor->async_free_space = monitor_work->data.add_trans.free_async_space;
            }
            if(rec->is_oneway){
                g_monitor->async_free_space = monitor_work->data.add_trans.free_async_space;
            }else{
                g_monitor->sync_used_space += total_size;

            }
            spin_unlock_irqrestore(&g_monitor->state_lock, flags);

                //添加到循环buffer
            add_transaction_record(&g_monitor->trans_buffer, rec);
            break;
        }

        case WORK_REMOVE_TRANSACTION: {
            uint64_t debug_id = monitor_work->data.remove_trans.debug_id;
            struct transaction_record record;

            /* 移除已接收的事务记录 */
            bool found = remove_transaction_record(&g_monitor->trans_buffer, debug_id,&record);
            if(!found){
                //pr_err("%s: transaction record not found with DebugID 0x%llx.\n", MODULE_NAME, debug_id);
                break;
            }
            /* 事务完成时更新内存使用情况 */
            unsigned long flags;
            spin_lock_irqsave(&g_monitor->state_lock, flags);
            if(record.is_oneway){
                g_monitor->async_free_space += record.mem_size;
            }else if (g_monitor->sync_used_space > 0){
                g_monitor->sync_used_space -= record.mem_size;
            }
            spin_unlock_irqrestore(&g_monitor->state_lock, flags);

            break;
        }

        case WORK_RESET_SYSTEM_SERVER: {
            unsigned long flags;

            spin_lock_irqsave(&g_monitor->proc_lock, flags);

            pr_info("%s: system_server proc freed, resetting state\n", MODULE_NAME);
            g_monitor->system_server_pid = 0;
            g_monitor->system_server_proc = NULL;

            spin_unlock_irqrestore(&g_monitor->proc_lock, flags);
            break;
        }
    }

    kfree(monitor_work);
}

/* 分配并初始化异步工作 */
struct monitor_work *alloc_monitor_work(enum monitor_work_type type)
{
    struct monitor_work *work = kmalloc(sizeof(*work), GFP_ATOMIC);
    if (work) {
        INIT_WORK(&work->work, monitor_work_handler);
        work->type = type;
        memset(&work->data, 0, sizeof(work->data));
    }
    return work;
}


/* trace_android_vh_binder_transaction_received 钩子 */
void binder_transaction_received(void *ignore, struct binder_transaction *t,
                                        struct binder_proc *proc, struct binder_thread *thread,
                                        uint32_t cmd)
{
    if (!g_monitor || !proc || !t) {
        return;
    }

    /* 先判断是否是system_server的事务，减少不必要的work创建 */
    unsigned long flags;
    bool is_system_server_pid = false;
    pid_t system_server_pid = 0;

    spin_lock_irqsave(&g_monitor->proc_lock, flags);
    system_server_pid = g_monitor->system_server_pid;
    spin_unlock_irqrestore(&g_monitor->proc_lock, flags);

    is_system_server_pid = (proc->pid == system_server_pid);
    if (is_system_server_pid) {
        /* 异步处理：移除事务记录 */
        struct monitor_work *work = alloc_monitor_work(WORK_REMOVE_TRANSACTION);
        if (work) {
            work->data.remove_trans.debug_id = t->debug_id;
            queue_work(monitor_wq, &work->work);
        }
    }
}

/* trace_android_vh_binder_transaction_record 钩子 */
void binder_transaction_record(void *ignore, struct binder_transaction_data *tr,
                                           struct binder_transaction *t,
                                           struct binder_transaction *in_reply_to)
{
    if (!g_monitor || !t->to_proc || !t->from || !t->from->proc
                || t->to_proc->is_dead ||(!(t->flags & TF_ONE_WAY)&&t->to_proc->is_frozen)
                || (t->to_thread && t->to_thread->is_dead)) {
        return;
    }

    /* 先判断是否是system_server的事务，减少不必要的work创建 */
    unsigned long flags;
    bool is_target_system_server = false;
    pid_t system_server_pid = 0;

    spin_lock_irqsave(&g_monitor->proc_lock, flags);
    system_server_pid = g_monitor->system_server_pid;
    spin_unlock_irqrestore(&g_monitor->proc_lock, flags);

    is_target_system_server = (t->to_proc->pid == system_server_pid);
    if (system_server_pid > 0 && is_target_system_server) {
        size_t sync_total_space;
        size_t async_total_space;
        size_t free_async_space;

        spin_lock(&t->to_proc->alloc.lock);
        /*pr_info("%s: binder_transaction_record, size_t:%zu,free_async_space:%zu \n",
                MODULE_NAME,t->to_proc->alloc.buffer_size, t->to_proc->alloc.free_async_space);*/
        async_total_space = t->to_proc->alloc.buffer_size/2;
        sync_total_space = t->to_proc->alloc.buffer_size - async_total_space;
        free_async_space = t->to_proc->alloc.free_async_space;
        spin_unlock(&t->to_proc->alloc.lock);

        /* 异步处理：添加事务记录 */
        struct monitor_work *work = alloc_monitor_work(WORK_ADD_TRANSACTION);
        if (work) {
            work->data.add_trans.rec.sender_pid = t->from_pid;
            work->data.add_trans.rec.sender_tid = t->from_tid;
            work->data.add_trans.rec.code = t->code;
            work->data.add_trans.rec.target_pid = t->to_proc->pid;
            work->data.add_trans.rec.target_tid = t->to_thread ? t->to_thread->pid : -1;
            work->data.add_trans.rec.debug_id = t->debug_id;
            work->data.add_trans.rec.is_oneway = ((in_reply_to == NULL)&&!!(t->flags & TF_ONE_WAY));

            work->data.add_trans.sync_total_space = sync_total_space;
            work->data.add_trans.async_total_space = async_total_space;
            work->data.add_trans.free_async_space = free_async_space;
            work->data.add_trans.data_size = t->buffer->data_size;
            work->data.add_trans.offsets_size = t->buffer->offsets_size;
            work->data.add_trans.extra_buffers_size = t->buffer->extra_buffers_size;

            // 初始化字符串字段
            memset(work->data.add_trans.rec.sender_comm, 0, TASK_COMM_LEN);
            memset(work->data.add_trans.rec.sender_thread_comm, 0, TASK_COMM_LEN);
            memset(work->data.add_trans.rec.target_thread_comm, 0, TASK_COMM_LEN);

            strncpy(work->data.add_trans.rec.sender_comm,t->from?t->from->proc->tsk->comm:"",TASK_COMM_LEN);
            strncpy(work->data.add_trans.rec.sender_thread_comm,t->from? t->from->task->comm: "",TASK_COMM_LEN);
            strncpy(work->data.add_trans.rec.target_thread_comm,t->to_thread? t->to_thread->task->comm: "",TASK_COMM_LEN);

            queue_work(monitor_wq, &work->work);
        }
    }
}

/* trace_android_vh_binder_preset 钩子 */
void binder_preset(void *ignore, struct hlist_head *hhead, struct mutex *lock,
                          struct binder_proc *proc)
{
    if (!g_monitor || !proc) {
        return;
    }

    /* 更新system_server信息 - 同步执行，因为这是proc创建的早期阶段 */
    update_system_server_info(proc);
}

/* trace_android_vh_binder_free_proc 钩子 */
void binder_free_proc(void *ignore, struct binder_proc *proc)
{
    if (!g_monitor || !proc) {
        return;
    }

    /* 先判断是否是system_server进程，减少不必要的work创建 */
    unsigned long flags;
    bool is_system_server_pid = false;
    pid_t system_server_pid = 0;

    spin_lock_irqsave(&g_monitor->proc_lock, flags);
    system_server_pid = g_monitor->system_server_pid;
    spin_unlock_irqrestore(&g_monitor->proc_lock, flags);

    is_system_server_pid = (proc->pid == system_server_pid);
    if (is_system_server_pid) {
        pr_info("%s: System server process is being freed\n", MODULE_NAME);
        /* 异步处理：重置system_server状态 */
        struct monitor_work *work = alloc_monitor_work(WORK_RESET_SYSTEM_SERVER);
        if (work) {
            work->data.reset_system_server.proc = proc;
            queue_work(monitor_wq, &work->work);
        }
    }
}

/* 注册trace点钩子 */
int register_trace_hooks(void)
{
    int ret = 0;

    ret = register_trace_android_vh_binder_transaction_received(binder_transaction_received, NULL);
    if (ret) {
        pr_err("%s: Failed to register binder_transaction_received hook\n", MODULE_NAME);
        return ret;
    }

    ret = register_trace_android_vh_binder_transaction_record(binder_transaction_record, NULL);
    if (ret) {
        pr_err("%s: Failed to register binder_transaction_record hook\n", MODULE_NAME);
        goto unreg_transaction_received;
    }

    ret = register_trace_android_vh_binder_preset(binder_preset, NULL);
    if (ret) {
        pr_err("%s: Failed to register binder_preset hook\n", MODULE_NAME);
        goto unreg_transaction_record;
    }

    ret = register_trace_android_vh_binder_free_proc(binder_free_proc, NULL);
    if (ret) {
        pr_err("%s: Failed to register binder_free_proc hook\n", MODULE_NAME);
        goto unreg_binder_preset;
    }

    return 0;


unreg_binder_preset:
    unregister_trace_android_vh_binder_preset(binder_preset, NULL);
unreg_transaction_record:
    unregister_trace_android_vh_binder_transaction_record(binder_transaction_record, NULL);
unreg_transaction_received:
    unregister_trace_android_vh_binder_transaction_received(binder_transaction_received, NULL);

    return ret;
}

void unregister_trace_hooks(void)
{
    unregister_trace_android_vh_binder_free_proc(binder_free_proc, NULL);
    unregister_trace_android_vh_binder_preset(binder_preset, NULL);
    unregister_trace_android_vh_binder_transaction_record(binder_transaction_record, NULL);
    unregister_trace_android_vh_binder_transaction_received(binder_transaction_received, NULL);
}

bool binder_monitor_init(void)
{
     int ret = 0;

    pr_info("%s: Initializing binder monitor\n", MODULE_NAME);

    /* 创建工作队列 */
    monitor_wq = create_singlethread_workqueue("binder_monitor_wq");
    if (!monitor_wq) {
        pr_err("%s: Failed to create workqueue\n", MODULE_NAME);
        return -ENOMEM;
    }

    /* 分配监控状态结构体 */
    g_monitor = kzalloc(sizeof(struct monitor_state), GFP_KERNEL);
    if (!g_monitor) {
        pr_err("%s: Failed to allocate monitor state\n", MODULE_NAME);
        destroy_workqueue(monitor_wq);
        monitor_wq = NULL;
        return -ENOMEM;
    }

    /* 初始化循环buffer */
    ret = init_transaction_buffer(&g_monitor->trans_buffer, MAX_TRANSACTIONS);
    if (ret) {
        pr_err("%s: Failed to initialize transaction buffer\n", MODULE_NAME);
        destroy_workqueue(monitor_wq);
        monitor_wq = NULL;
        kfree(g_monitor);
        g_monitor = NULL;
        return ret;
    }

    /* 初始化锁 */
    spin_lock_init(&g_monitor->proc_lock);
    spin_lock_init(&g_monitor->state_lock);

    /* 初始化轮询检查工作 */
    INIT_WORK(&g_monitor->check_work, check_thresholds_work_func);

    /* 初始化1秒定时器 */
    timer_setup(&g_monitor->check_timer, check_timer_callback, 0);

    /* 启动定时器，1秒后首次触发 */
    mod_timer(&g_monitor->check_timer, jiffies + msecs_to_jiffies(1000));

    /* 注册trace点钩子 */
    ret = register_trace_hooks();
    if (ret) {
        pr_err("%s: Failed to register trace hooks\n", MODULE_NAME);
        destroy_transaction_buffer(&g_monitor->trans_buffer);
        destroy_workqueue(monitor_wq);
        del_timer(&g_monitor->check_timer);
        cancel_work_sync(&g_monitor->check_work);
        monitor_wq = NULL;
        kfree(g_monitor);
        g_monitor = NULL;
        return ret;
    }

    pr_info("%s: Binder monitor initialized successfully\n", MODULE_NAME);
    return 0;
}
void binder_monitor_exit(void)
{
    pr_info("%s exit.\n", MODULE_NAME);
    if (!g_monitor) {
        goto cleanup_wq;
    }

    pr_info("%s: Exiting binder monitor\n", MODULE_NAME);

    /* 注销trace点钩子 */
    unregister_trace_hooks();

    /* 停止定时器 */
    del_timer(&g_monitor->check_timer);

    /* 取消未完成的工作 */
    cancel_work_sync(&g_monitor->check_work);

    /* 销毁循环buffer */
    destroy_transaction_buffer(&g_monitor->trans_buffer);

    /* 释放监控状态结构体 */
    kfree(g_monitor);
    g_monitor = NULL;

cleanup_wq:
    if (monitor_wq) {
        destroy_workqueue(monitor_wq);
        monitor_wq = NULL;
    }

    pr_info("%s: Binder monitor exited successfully\n", MODULE_NAME);

}
