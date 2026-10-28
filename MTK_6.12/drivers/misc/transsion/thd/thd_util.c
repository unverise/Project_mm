#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/semaphore.h>
#include <linux/mm.h>
#include <linux/mmap_lock.h>
#include <linux/sched/mm.h>
#include <linux/sched/debug.h>
#include <asm/uaccess.h>

#ifdef __aarch64__
/* The struct defined for AArch64 userspace stack frame */
struct stack_frame_user {
	unsigned long fp;
	unsigned long sp;
	unsigned long pc;
};


/*
 * The function of AArch64 userspace stack frame unwind method.
 * Note: If the caller want to dump another task's user backtrace,
 * call access_process_vm() to access another task' address space.
 */
static int arch_unwind_user_frame(struct task_struct *tsk, struct stack_frame_user *frame)
{
	int ret = 0;
	unsigned long fp = frame->fp;
	unsigned long low = frame->sp;
	unsigned long high = ALIGN(low, TASK_SIZE);

	if (fp < low || fp > high || fp & 0xf)
		return -EFAULT;

	frame->sp = fp + 0x10;
	/* Disable page fault to avoid break up get_user progress */
	pagefault_disable();
	if (tsk == current) {
		if (get_user(frame->fp, (unsigned long __user*)fp) ||
			get_user(frame->pc, (unsigned long __user*)(fp + 8)))
			ret = -EFAULT;
	} else {
		if (access_process_vm(tsk, fp, &frame->fp, sizeof(unsigned long), 0) != sizeof(unsigned long)
			|| access_process_vm(tsk, fp + 0x08, &frame->pc, sizeof(unsigned long), 0) != sizeof(unsigned long))
			ret = -EFAULT;
	}
	pagefault_enable();

	return ret;
}

/*
 * Print the name of a VMA.
 */
static void dump_vma_addr(char *prefix, struct task_struct *task, unsigned long ip, const char *loglvl)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;

	if (task != current)
		mm = get_task_mm(task);
	else
		mm = task->mm;

	if (!mm)
		return;
	/*
	 * we might be running from an atomic context so we cannot sleep
	 */
	if (!mmap_read_trylock(mm)) {
		mmput(mm);
		return;
	}

	vma = find_vma(mm, ip);
	if (vma && vma->vm_file) {
		struct file *f = vma->vm_file;
		char *buf = (char *)__get_free_page(GFP_NOWAIT);
		if (buf) {
			char *p;

			p = file_path(f, buf, PAGE_SIZE);
			if (IS_ERR(p))
				p = "?";
			printk("%s%s%s[%lx-%lx]\n", loglvl, prefix, p,
					vma->vm_start,
					vma->vm_end);
			free_page((unsigned long)buf);
		}
	}
	mmap_read_unlock(mm);
	if (task != current)
		mmput(mm);
}

static void dump_user_backtrace_entry(struct task_struct *tsk, unsigned long where, const char *loglvl)
{
	char prefix[64];

	snprintf(prefix, sizeof(prefix), "<0x%lx> in ", where);
	dump_vma_addr(prefix, tsk, where, loglvl);
}

void dump_user_stacktrace(struct pt_regs *regs, struct task_struct *tsk, const char *loglvl)
{
	struct stack_frame_user frame;

	if (!tsk)
		tsk = current;
	/*
	 * If tsk != current, then it can be concurrently executing.
	 * In that case, its regs are stale and the task can concurrently
	 * modify its stack. The caller must call try_get_task_stack() to
	 * safely access the stack of a non-current task that might exit.
	 */
	if (!regs || tsk != current) {
		if ((tsk != current) && (IS_ERR_OR_NULL(task_stack_page(tsk))
			|| !try_get_task_stack(tsk)))
			return;

		regs = task_pt_regs(tsk);
	}

	/* TODO: support stack unwind for compat user mode */
	if (!user_mode(regs) || compat_user_mode(regs))
		goto exit;

	frame.fp = regs->user_regs.regs[29];
	frame.sp = regs->user_regs.sp;
	frame.pc = regs->user_regs.pc;

	printk("%s[%s-%d] Dump user backtrace:\n", loglvl, tsk->comm, tsk->pid);
	while(1) {
		unsigned long where = frame.pc;

		if (!where || where & 0x3)
			break;
		dump_user_backtrace_entry(tsk, where, loglvl);
		if (arch_unwind_user_frame(tsk, &frame) < 0)
			break;
	}
exit:
	if (tsk != current){
#if IS_ENABLED(CONFIG_THREAD_INFO_IN_TASK)
		refcount_dec(&tsk->stack_refcount);
#endif
	}
}
#else
void dump_user_stacktrace(struct pt_regs *regs, struct task_struct *tsk, const char *loglvl)
{
	return;
}
#endif
