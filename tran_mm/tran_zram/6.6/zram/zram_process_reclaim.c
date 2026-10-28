// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025-2027 Transsion. All rights reserved.
 */
#define pr_fmt(fmt) "zram: " fmt

#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched/task.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/rmap.h>
#include <linux/cred.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/mm_inline.h>
#include <linux/swap.h>
#include <linux/hugetlb.h>
#include <linux/huge_mm.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>
#include <linux/proc_fs.h>
#include <uapi/linux/uio.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include "zram_process_reclaim.h"
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT

/*
 * check current need cancel reclaim or not, please check task not NULL first.
 * If the reclaimed task has goto foreground, cancel reclaim immediately
 */
enum reclaim_type {
	RECLAIM_FILE,
	RECLAIM_ANON,
	RECLAIM_ALL,
};

struct process_reclaim_control {
	unsigned long compress_pages; //unit is PAGE_SIZE
	unsigned long compress_time; //unit is us
	unsigned long compress_ratio; //compress data / total process VmRss
};

#define MF_COMPACT_STAT_CMD _IOWR('a', 1, struct process_reclaim_stat *)
#define MF_COMPACT_PID_STAT_CMD _IOWR('b', 1, struct pid_reclaim_stat *)

/*
 * Please ensure that struct process_reclaim_cmd is consistent with the definition in
 * services/core/jni/com_android_server_am_CachedAppOptimizer.cpp
 */
struct process_reclaim_cmd {
	union {
		struct iovec *vec;/* OUT: vma address table*/
		char *cmd;/* IN: cmd string*/
		long compress_size;/* OUT: compress size in KB*/
		long decompress_size; /* OUT: decompress size in KB*/
	};
	unsigned int cnt;/*IN: cnt means cmd string len  OUT: nr. of struct iovec*/
	unsigned int max_cnt;/* IN: means the max nums of struct iovec*/
	unsigned long last_addr; /* INOUT: means compress max addr last time*/
	unsigned long compress_data_limit; /* IN: unit is KB */
	unsigned long compress_time_limit; /* IN: unit is ms */
	int compress_prop_limit; /* IN: compression proportion */
	int readahead; /* IN: 0 compress,  1 readahead */
	int abort_reason; /* OUT: compress abort reason */
	int log_enable;/* OUT: 0 disalbe 1 enable log*/
	unsigned long rss_anon; /*same as /proc/pid/status RssAnon*/
	unsigned long vm_swap; /*same as /proc/pid/status VmSwap*/
	unsigned long vm_rss; /*same as /proc/pid/status VmRss*/
	unsigned long vm_lock; /*same as /proc/pid/status VmLck*/
	unsigned long free_swap; /*same as /proc/meminfo SwapFree*/
	int64_t readback_total; /*The pages read back after compression.*/
	int64_t readback_1; /*The pages read back within 0-10 seconds after compression.*/
	int64_t readback_2; /*The pages read back within 10-30 seconds after compression.*/
	int64_t readback_3; /*The pages read back within 30-60 seconds after compression.*/
	int64_t readback_4; /*The pages read back within 60-300 seconds after compression.*/
	int frozen; /* target process frozen state */
	int frozen_chg; /* no. of changes in the frozen state of the process during the entire compression process. */
	int get_vma_seq;  /* get vma seq*/
	int reserved1;
	int reserved2;
	int reserved3;
};

#define VECT_LEN_LIMIT (512 << 20)

#define PROCESS_RECLAIM_CMD_LEN 64
#define PROCESS_RECLAIM_PROP_MAX 100

#define MF_COMPRESS_MAX_TIME 30*1000UL
#define MF_COMPRESS_MAX_SIZE 12*1024*1024UL

__read_mostly static int zram_process_reclaim_prop = 70;
module_param(zram_process_reclaim_prop, int, 0644);
/* unit is ms, limit reclaim time 30s*/
__read_mostly static int zram_process_reclaim_time = MF_COMPRESS_MAX_TIME;
module_param(zram_process_reclaim_time, int, 0644);
/* unit is KB, limit reclaim data 2GB */
__read_mostly static int zram_process_reclaim_limit = MF_COMPRESS_MAX_SIZE;
module_param(zram_process_reclaim_limit, int, 0644);
__read_mostly static int zram_vma_debug_enable = 0;
module_param(zram_vma_debug_enable, int, 0644);

/* unit is pages*/
__read_mostly int zram_min_rss_anon = 512;
module_param(zram_min_rss_anon, int, 0644);

__read_mostly int zram_min_cprs_prop = 90;
module_param(zram_min_cprs_prop, int, 0644);

/* Records compression times since zram setup. */
atomic64_t fwk_compress_freq_since = ATOMIC_INIT(0);
/* Records the number of pages compressed by the compression thread. */
atomic64_t fwk_compress_pages_since = ATOMIC_INIT(0);
/* Record the number of pages from which the data was read. These pages was compressed by the compression thread. */
atomic64_t fwk_decompress_pages_since = ATOMIC_INIT(0);
/* Records compression runtime on cpu since zram setup. */
atomic64_t fwk_compress_runtime_since = ATOMIC_INIT(0);

/* Record compressed abort reason. */
int zram_last_compressed_abort = ZRAM_CANCEL_BY_NONE;
/* Record compressed thread task_struct. single thread model*/
struct task_struct *zram_compressed_task = NULL;

static struct zram_compress_single zram_compress_params = {0};

static DEFINE_MUTEX(fwk_reclaim_mutex);
static struct proc_dir_entry *zram_process_reclaim_stats = NULL;
static struct proc_dir_entry *zram_process_reclaim = NULL;
static struct proc_dir_entry *zram_process_reclaim_enable = NULL;

static long get_process_stat_mm(struct task_struct* tsk, struct pid_reclaim_stat *cmd_entry);

/* now, only one compress thread, so we don't care about multi-threads protection.*/
void init_compress_stat(const struct process_reclaim_cmd* cmd_entry, struct task_struct* task)
{
	struct zram_compress_single *params = &zram_compress_params;

	memset(params, 0, sizeof(struct zram_compress_single));
	if (!cmd_entry->readahead)
		params->compressed_snapshot = atomic64_read(&fwk_compress_pages_since);
	else
		params->compressed_snapshot = atomic64_read(&fwk_decompress_pages_since);
	params->start_time = local_clock();

	if (!cmd_entry->compress_data_limit)
		params->cprs_data_limit  = zram_process_reclaim_limit >> (PAGE_SHIFT - 10);
	else
		params->cprs_data_limit = cmd_entry->compress_data_limit >> (PAGE_SHIFT - 10);

	if (!cmd_entry->compress_time_limit)
		params->cprs_time_limit = zram_process_reclaim_time*1000*1000;
	else
		params->cprs_time_limit = cmd_entry->compress_time_limit*1000*1000;

	if (!cmd_entry->compress_prop_limit)
		params->cprs_prop_limit = zram_process_reclaim_prop;
	else
		params->cprs_prop_limit = cmd_entry->compress_prop_limit;

	params->target_task = task;
	params->target_pid = task->pid;

	params->frozen = READ_ONCE(task->__state) & TASK_FROZEN;
	params->frozen_chg = 0U;

	params->sum_exec_runtime = current->se.sum_exec_runtime;
	atomic64_inc(&fwk_compress_freq_since);
	zram_last_compressed_abort = ZRAM_CANCEL_BY_NONE;
	if (READ_ONCE(zram_compressed_task) != current) {
		pr_err("compress thread change frome %px to %px\n", zram_compressed_task, current);
		WRITE_ONCE(zram_compressed_task, current);
	}

	if (is_log_enable(ZRAM_COMPRESS_LOG))
		pr_err("cmd limit: data:%llu pages time: %llu ns prop:%u runtime:%llu size:%lld\n",
			params->cprs_data_limit,
			params->cprs_time_limit,
			params->cprs_prop_limit,
			params->sum_exec_runtime,
			params->compressed_snapshot <<(PAGE_SHIFT-10));
}

static void update_compress_stat(void* arg, struct process_reclaim_cmd *cmd_entry){
	struct pid_reclaim_stat stat;
	struct sysinfo sysinfo;
	s64 delta_compress_time = 0LL;
	struct zram_compress_single *params = &zram_compress_params;
	struct task_struct *target_task;
	int ret;

	if (current->se.sum_exec_runtime > params->sum_exec_runtime) {
		delta_compress_time = current->se.sum_exec_runtime - params->sum_exec_runtime;
		atomic64_add(delta_compress_time, &fwk_compress_runtime_since);
	}

	if (!cmd_entry->readahead) {
		cmd_entry->compress_size = (atomic64_read(&fwk_compress_pages_since) -
							params->compressed_snapshot) << (PAGE_SHIFT - 10);
	} else {
		cmd_entry->decompress_size = (atomic64_read(&fwk_decompress_pages_since) -
							params->compressed_snapshot) << (PAGE_SHIFT - 10);
	}
	cmd_entry->abort_reason = zram_last_compressed_abort;
	cmd_entry->frozen = params->frozen;
	cmd_entry->frozen_chg = params->frozen_chg;

	si_swapinfo(&sysinfo);
	cmd_entry->free_swap = sysinfo.freeswap << (PAGE_SHIFT - 10);

	if (is_log_enable(ZRAM_COMPRESS_LOG)) {
		rcu_read_lock();
		target_task = find_task_by_vpid(params->target_pid);
		if (!target_task) {
			rcu_read_unlock();
			pr_err("cprs update stat. pid:%d died\n", params->target_pid);
			return;
		}

		get_task_struct(target_task);
		rcu_read_unlock();

		memset(&stat, 0, sizeof(stat));
		ret = get_process_stat_mm(target_task, &stat);

		cmd_entry->rss_anon = stat.rss_anon;
		cmd_entry->vm_swap = stat.vm_swap;
		cmd_entry->vm_rss = stat.vm_rss;
		cmd_entry->vm_lock = stat.vm_lock;

		pr_err("cprs [%s] pid:%d tgid:%d rss_anon:%lu swap:%lu rss:%lu lock:%lu freeswap:%lu read %lld %lld %lld %lld ret:%d\n",
			target_task->comm, target_task->pid, target_task->tgid,
			stat.rss_anon, stat.vm_swap, stat.vm_rss, stat.vm_lock, cmd_entry->free_swap,
			cmd_entry->readback_1, cmd_entry->readback_2, cmd_entry->readback_3, cmd_entry->readback_4, ret);

		put_task_struct(target_task);

		pr_err("compress %ld KB time: %llu ns speed: %lld KB/s times:%lld runtime:%lld us fz:%x fz-chg:%d\n",
			cmd_entry->compress_size, delta_compress_time, cmd_entry->compress_size*1000*1000*1000/delta_compress_time,
			atomic64_read(&fwk_compress_freq_since), atomic64_read(&fwk_compress_runtime_since)/1000, params->frozen, params->frozen_chg);
	}

	return;
}

struct zram_compress_single* get_compress_params(void) {
	return &zram_compress_params;
}

static inline bool can_madv_lru_vma(struct vm_area_struct *vma)
{
	return !(vma->vm_flags & (VM_LOCKED|VM_HUGETLB|VM_PFNMAP));
}
#if 0
static inline bool can_do_pageout(struct vm_area_struct *vma, enum reclaim_type type)
{
	switch (type) {
	case RECLAIM_ANON:
		return vma_is_anonymous(vma);
	case RECLAIM_FILE:
		if (vma_is_anonymous(vma))
			return false;
		if (!vma->vm_file)
			return false;
		return true;
	case RECLAIM_ALL:
		if (vma_is_anonymous(vma))
			return true;
		if (!vma->vm_file)
			return false;
	default:
		return false;
	}

	return false;
}
#endif

/* Determine whether further compression is possible. */
static noinline int zram_should_cancel_reclaim(struct task_struct *task,
		struct mm_struct *mm, struct process_reclaim_cmd* cmd_entry)
{
	unsigned long total_rss, rss_anon, rss_file, rss_shmem, swap;

	if (cmd_entry->get_vma_seq > 1)
		return 0;

	rss_anon = get_mm_counter(mm, MM_ANONPAGES);
	swap = get_mm_counter(mm, MM_SWAPENTS);

	if ((rss_anon < zram_min_rss_anon) && (swap * 100 > (rss_anon + swap)*zram_min_cprs_prop)) {
		if (is_log_enable(ZRAM_COMPRESS_LOG)) {
			rss_file = get_mm_counter(mm, MM_FILEPAGES);
			rss_shmem = get_mm_counter(mm, MM_SHMEMPAGES);
			total_rss = rss_anon + rss_file + rss_shmem;
			cmd_entry->rss_anon = rss_anon<<(PAGE_SHIFT-10);
			cmd_entry->vm_swap = swap<<(PAGE_SHIFT-10);
			cmd_entry->vm_rss = total_rss<<(PAGE_SHIFT-10);
			cmd_entry->vm_lock = mm->locked_vm<<(PAGE_SHIFT-10);
		}
		return -ZRAM_REASON_CANNOT_CPRS;
	}

	return 0;
}

static noinline ssize_t zram_reclaim_task(struct task_struct *task, char *buffer,
		struct iovec *uvector, unsigned long *last_addr, struct process_reclaim_cmd* cmd_entry)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	enum reclaim_type type;
	enum reclaim_type vma_type;
	char *type_buf;
	unsigned long start, end;
	int ret = 0, i;
	struct vma_iterator vmi;

	if (task == current->group_leader)
		goto out_err;

	type_buf = strstrip(buffer);
	if (!strcmp(type_buf, "file"))
		type = RECLAIM_FILE;
	else if (!strcmp(type_buf, "anon"))
		type = RECLAIM_ANON;
	else if (!strcmp(type_buf, "all"))
		type = RECLAIM_ALL;
	else {
		ret = -EINVAL;
		goto out_err;
	}

	mm = get_task_mm(task);
	if (!mm)
		return -EINVAL;

	ret = zram_should_cancel_reclaim(task, mm, cmd_entry);
	if (ret < 0) {
		mmput(mm);
		goto out_err;
	}

	init_compress_stat(cmd_entry, task);

	start = 0;
	end = 0;
	i = 0;
	down_read(&mm->mmap_lock);

	vma_iter_init(&vmi, mm, 0);
	for_each_vma(vmi, vma) {
		if (is_vm_hugetlb_page(vma))
			continue;

		if (!cmd_entry->readahead) {
			if (vma->vm_end <= *last_addr)
				continue;
			if (!can_madv_lru_vma(vma))
				continue;
		}

		if (type == RECLAIM_ALL)
			vma_type = RECLAIM_ALL;
		else {
			//vma_type 0 mean file  1 mean anon
			vma_type = vma_is_anonymous(vma);
		}

		if (type == vma_type) {
			if (start == 0) {
				start = vma->vm_start;
				end = vma->vm_end;
			} else if (zram_vma_debug_enable && (vma->vm_start == end)) {
				end = vma->vm_end;
				if (end - start >= VECT_LEN_LIMIT) {
					uvector[i].iov_base = (void*)start;
					uvector[i].iov_len = (size_t)(end - start);
					*last_addr = end;
					i++;
					start = 0;
				}
			} else {
				uvector[i].iov_base = (void*)start;
				uvector[i].iov_len = (size_t)(end - start);
				*last_addr = end;
				i++;
				start = vma->vm_start;
				end = vma->vm_end;
			}
		} else {
			if (start == 0)
				continue;
			uvector[i].iov_base = (void*)start;
			uvector[i].iov_len = (size_t)(end - start);
			*last_addr = end;
			i++;
			start = 0;
		}

		if (!cmd_entry->readahead && (mm != task->mm)) {
			ret = -ESRCH;
		}

		if (i >= cmd_entry->max_cnt)
			break;
	}

	if (ret < 0) {
		up_read(&mm->mmap_lock);
		mmput(mm);
		goto out_err;
	}

	if ((i < cmd_entry->max_cnt) && (type == vma_type) && (start != 0)) {
		uvector[i].iov_base = (void*)start;
		uvector[i].iov_len = (size_t)(end - start);
		*last_addr = end;
		i++;
	}

	ret = i;
	up_read(&mm->mmap_lock);
	mmput(mm);

out_err:
	return ret;
}

static unsigned int collect_process_vmas(char *kbuf, struct iovec *uvector,
					struct process_reclaim_cmd* cmd_entry,  unsigned long *last_addr)
{
	const unsigned int params_num = 2;
	pid_t tsk_pid;
	struct task_struct* tsk;
	ssize_t ret = 0;
	char type[12] = {'\0'};

	kbuf = strstrip(kbuf);
	if (sscanf(kbuf, "%d %s", &tsk_pid, (char*)&type) != params_num) {
		pr_err("process_reclaim kbuf is invalid: %s\n", kbuf);
		ret = -EINVAL;
		goto done;
	}

	rcu_read_lock();
	tsk = find_task_by_vpid(tsk_pid);
	if (!tsk) {
		rcu_read_unlock();
		pr_err("process_reclaim can't find pid:%d\n", tsk_pid);
		ret = -ESRCH;
		goto done;
	}

	if (tsk != tsk->group_leader)
		tsk = tsk->group_leader;
	get_task_struct(tsk);
	rcu_read_unlock();
	ret = zram_reclaim_task(tsk, type, uvector, last_addr, cmd_entry);
	put_task_struct(tsk);

done:

	return ret;
}

long process_reclaim_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct process_reclaim_cmd cmd_entry;
	unsigned int iovec_size;
	char cmdline[PROCESS_RECLAIM_CMD_LEN];
	struct iovec *uvectors;
	unsigned long last_addr;
	int ret = 0;

	if (!g_process_reclaim_enable) {
		pr_err("process_reclaim is disabled!\n");
		return -EPERM;
	}

	ret = mutex_trylock(&fwk_reclaim_mutex);
	if (!ret) {
		pr_err("process_reclaim trylock failed\n");
		ret = -EAGAIN;
		goto release_lock;
	}

	if (!access_ok((void *)arg, sizeof(struct process_reclaim_cmd))) {
		pr_err("process_reclaim params are invalid\n");
		ret = -EINVAL;
		goto release_lock;
	}

	if (copy_from_user(&cmd_entry, (void *)arg,
				sizeof(struct process_reclaim_cmd))) {
		pr_err("process_reclaim copy params failed");
		ret = -ENOMEM;
		goto release_lock;
	}

	if (cmd_entry.cnt == 0 || cmd_entry.max_cnt == 0 ||
			cmd_entry.cmd == NULL ||
			cmd_entry.cnt >= PROCESS_RECLAIM_CMD_LEN ||
			cmd_entry.max_cnt > UIO_MAXIOV * 2 ||
			cmd_entry.compress_prop_limit > PROCESS_RECLAIM_PROP_MAX || cmd_entry.compress_prop_limit < 0) {
		pr_err("process_reclaim %s invalid params, cnt %u max_cnt %u cmd %px prop %d\n",
				current->comm, cmd_entry.cnt, cmd_entry.max_cnt, cmd_entry.cmd, cmd_entry.compress_prop_limit);
		ret = -EINVAL;
		goto release_lock;
	}

	if (is_log_enable(ZRAM_COMPRESS_LOG)) {
		pr_err("process_reclaim cnt:%d max_cnt:%d last_addr:%lx readahead:%d data:%lu time:%lu prop:%d\n",
				 cmd_entry.cnt, cmd_entry.max_cnt, cmd_entry.last_addr, cmd_entry.readahead,
				 cmd_entry.compress_data_limit, cmd_entry.compress_time_limit, cmd_entry.compress_prop_limit);
	}

	iovec_size = cmd_entry.max_cnt * sizeof(struct iovec);
	if (!access_ok((void *)cmd_entry.cmd, iovec_size)) {
		pr_err("process_reclaim cmd is invalid\n");
		ret = -EINVAL;
		goto release_lock;
	}

	memset(cmdline, 0, PROCESS_RECLAIM_CMD_LEN);
	if (copy_from_user(cmdline, (void *)cmd_entry.cmd, cmd_entry.cnt)) {
		pr_err("process_reclaim copy cmd failed");
		ret = -ENOMEM;
		goto release_lock;
	}

	/*finish compress and update compression stat*/
	if (!strncmp(cmdline, "update", PROCESS_RECLAIM_CMD_LEN)) {
		update_compress_stat((void *)arg, &cmd_entry);
		(void) copy_to_user((void *)arg, (void*)&cmd_entry,
				sizeof(struct process_reclaim_cmd));	
		ret = 0;
		goto release_lock;
	}

	uvectors = vmalloc(iovec_size);
	if (!uvectors) {
		pr_err("process_reclaim vmalloc uvectors failed.\n");
		goto release_lock;
	}

	last_addr = cmd_entry.last_addr;
	ret = collect_process_vmas(cmdline, uvectors, &cmd_entry, &last_addr);
	cmd_entry.cnt = 0;
	cmd_entry.log_enable = is_log_enable(ZRAM_VMA_DEBUG_LOG);

	if (ret > 0) {
		cmd_entry.last_addr = last_addr;
		(void)copy_to_user(cmd_entry.vec, uvectors,
				ret * sizeof(struct iovec));
		cmd_entry.cnt = ret;
	}
	(void) copy_to_user((void *)arg, (void*)&cmd_entry,
				sizeof(struct process_reclaim_cmd));
	vfree(uvectors);

release_lock:

	mutex_unlock(&fwk_reclaim_mutex);
	return ret;
}

long get_process_reclaim_stat(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct process_reclaim_stat cmd_entry;
	int ret = 0;

	if (!g_process_reclaim_enable) {
		pr_err("process_reclaim stat is disabled!\n");
		return -EPERM;
	}

	if (!access_ok((void *)arg, sizeof(struct process_reclaim_stat))) {
		pr_err("process_reclaim stat params are invalid\n");
		return -EINVAL;
	}

	if (copy_from_user(&cmd_entry, (void *)arg,
				sizeof(struct process_reclaim_stat))) {
		pr_err("process_reclaim stat copy params failed");
		return -ENOMEM;
	}

	if (cmd_entry.cmd == 0 || cmd_entry.check_size != sizeof(cmd_entry)) {
		pr_err("process_reclaim stat %s invalid params, cmd: %d checksize: %d\n",
				current->comm, cmd_entry.cmd, cmd_entry.check_size);
		return -EINVAL;
	}

	ret = zram_process_reclaim_stat(&cmd_entry);
	if (ret < 0) {
		pr_err("process_reclaim stat zram not ready\n");
		return -EBUSY;
	}
	cmd_entry.check_size = (int)sizeof(cmd_entry);
	(void) copy_to_user((void *)arg, (void*)&cmd_entry, sizeof(cmd_entry));

	return ret;
}

static long get_process_stat_mm(struct task_struct* tsk, struct pid_reclaim_stat *cmd_entry)
{
	struct mm_struct *mm;
	unsigned long total_rss, rss_anon, rss_file, rss_shmem, swap;

	mm = get_task_mm(tsk);
	if (!mm) {
		return -EINVAL;
	}

	rss_anon = get_mm_counter(mm, MM_ANONPAGES);
	rss_file = get_mm_counter(mm, MM_FILEPAGES);
	rss_shmem = get_mm_counter(mm, MM_SHMEMPAGES);
	swap = get_mm_counter(mm, MM_SWAPENTS);

	total_rss = rss_anon + rss_file + rss_shmem;
	cmd_entry->rss_anon = rss_anon<<(PAGE_SHIFT-10);
	cmd_entry->vm_swap = swap<<(PAGE_SHIFT-10);
	cmd_entry->vm_rss = total_rss<<(PAGE_SHIFT-10);
	cmd_entry->vm_lock = mm->locked_vm<<(PAGE_SHIFT-10);
	mmput(mm);

	return 0;
}

long get_pid_compress_stat_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct task_struct *tsk;
	struct pid_reclaim_stat cmd_entry;
	int ret = 0;
	pid_t tsk_pid;

	if (!g_process_reclaim_enable) {
		pr_err("pid_compress_stat is disabled!\n");
		return -EPERM;
	}

	if (!access_ok((void *)arg, sizeof(cmd_entry))) {
		pr_err("pid_compress_stat params are invalid\n");
		return -EINVAL;
	}

	if (copy_from_user(&cmd_entry, (void *)arg, sizeof(cmd_entry))) {
		pr_err("pid_compress_stat copy params failed");
		return -ENOMEM;
	}

	tsk_pid = cmd_entry.pid;
	rcu_read_lock();
	tsk = find_task_by_vpid(tsk_pid);
	if (!tsk) {
		rcu_read_unlock();
		pr_err("pid_compress_stat can't find pid:%d\n", tsk_pid);
		ret = -ESRCH;
		goto OUT;
	}

	if (tsk != tsk->group_leader)
		tsk = tsk->group_leader;

	get_task_struct(tsk);
	rcu_read_unlock();
	ret = get_process_stat_mm(tsk, &cmd_entry);
	put_task_struct(tsk);

	cmd_entry.log_enable = is_log_enable(ZRAM_COMPRESS_LOG);
	if (ret >= 0)
		(void) copy_to_user((void *)arg, (void*)&cmd_entry, sizeof(cmd_entry));
OUT:
	return ret;
}

long process_reclaim_stat_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret;

	switch (cmd) {
		case MF_COMPACT_STAT_CMD:
		   ret = get_process_reclaim_stat(file, cmd, arg);
		   break;

		case MF_COMPACT_PID_STAT_CMD:
		   ret = get_pid_compress_stat_ioctl(file, cmd, arg);
		   break;

		default:
		   ret = -ENOTTY;
		   break;
	}

	return ret;
}

static int process_reclaim_enable_show(struct seq_file *m, void *v) {
	seq_printf(m, "compress: %s\n", g_process_reclaim_enable?"on":"off");
	return 0;
}

static int process_reclaim_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, process_reclaim_enable_show, NULL);
}

static ssize_t process_reclaim_enable_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[32]={'\0'};
	int temp = 0;

	if (count >= 32) {
		return count;
	}

	if (count > 0) {
		if (copy_from_user(buf, ubuf, count)) {
		    return -EFAULT;
		}
		sscanf(buf, "%d", &temp);
		g_process_reclaim_enable = (temp>0?true:false);
		pr_err("app compact status: %s\n", g_process_reclaim_enable?"on":"off");
	}

	return count;
}

const struct proc_ops zram_process_reclaim_enable_fops = {
	.proc_write = process_reclaim_enable_write,
	.proc_open = process_reclaim_enable_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops zram_process_reclaim_stat_ops = {
	.proc_ioctl			= process_reclaim_stat_ioctl,
	.proc_compat_ioctl	= process_reclaim_stat_ioctl,
	.proc_lseek		= default_llseek,
};

static const struct proc_ops zram_process_reclaim_fops = {
	.proc_ioctl			= process_reclaim_ioctl,
	.proc_compat_ioctl	= process_reclaim_ioctl,
	.proc_lseek			= default_llseek,
};

int zram_process_reclaim_init(struct proc_dir_entry *memfusion_dir_entry)
{
	zram_process_reclaim_enable = proc_create("compress_enable", S_IALLUGO, memfusion_dir_entry, &zram_process_reclaim_enable_fops);
	if (!zram_process_reclaim_enable) {
		pr_err("process_reclaim failed to create compress_enable\n");
		return -ENOMEM;
	}

	zram_process_reclaim = proc_create("compress", S_IALLUGO, memfusion_dir_entry, &zram_process_reclaim_fops);
	if (!zram_process_reclaim) {
		proc_remove(zram_process_reclaim_enable);
		zram_process_reclaim_enable = NULL;
		pr_err("process_reclaim failed to create compress\n");
		return -ENOMEM;
	}

	zram_process_reclaim_stats = proc_create("compress_stat", S_IALLUGO, memfusion_dir_entry, &zram_process_reclaim_stat_ops);
	if (!zram_process_reclaim_stats) {
		proc_remove(zram_process_reclaim_enable);
		zram_process_reclaim_enable = NULL;
		proc_remove(zram_process_reclaim);
		zram_process_reclaim = NULL;
		pr_err("process_reclaim failed to create compress_stat\n");
		return -ENOMEM;
	}

	return 0;
}

void zram_process_reclaim_exit(void)
{
	proc_remove(zram_process_reclaim_enable);
	proc_remove(zram_process_reclaim_stats);
	proc_remove(zram_process_reclaim);
}

#endif

