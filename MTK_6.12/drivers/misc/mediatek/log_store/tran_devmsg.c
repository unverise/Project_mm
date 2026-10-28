/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 TRANSSION Inc.
 * Transsion device stability message manager by qiwu.chen on 20240310
 *
 */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/memblock.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/kdebug.h>
#include <linux/uaccess.h>
#include <linux/rtc.h>
#include <linux/panic_notifier.h>
//#include <asm/io.h>
#include <asm/kexec.h>
#include <trace/hooks/traps.h>
#include "log_store_kernel.h"
#include "tran_devmsg.h"

static bool enable = false;
module_param(enable, bool, 0644);
static int dev_id = 0;
module_param(dev_id, int, 0644);
static int msg_type = 0;
module_param(msg_type, int, 0644);

static char *g_tran_devmsg_buffer;
static u32 g_tran_devmsg_size;
static struct proc_dir_entry *proc_entry;
/* Gates the devmsg recording process to ensure only one succeeds in a concurrent time. */
static atomic_t tran_record_devmsg_gate = ATOMIC_INIT(0);

static int tran_devmsg_parse_chosen(void)
{
	struct device_node *node;
	const char *tran_fdt;
	int ret = -1;

	node = of_find_node_by_path("/chosen");
	if (node) {
		if (of_property_read_string(node, "tran,fdt", &tran_fdt) == 0) {
			ret = 0;
			if (strstr(tran_fdt, "tran_devmsg")) {
				enable = true;
				pr_info("%s: tran_devmsg is enable\n", __func__);
			} else {
				enable = false;
				pr_info("%s: tran_devmsg is disable\n", __func__);
			}
		} else
			pr_err("%s: Can't find tran,fdt node\n", __func__);
		of_node_put(node);
	} else
		pr_err("%s: Can't find chosen node\n", __func__);

	return ret;
}

static void tran_devmsg_time_sync(struct tran_devmsg_time *tm)
{
	struct rtc_time rt;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	struct timespec64 tv;
#else
	struct timeval tv;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	ktime_get_ts64(&tv);
#else
	do_gettimeofday(&tv);
#endif
	rtc_time64_to_tm(tv.tv_sec - sys_tz.tz_minuteswest * 60, &rt);
	tm->tm_year = rt.tm_year + 1900;
	tm->tm_mon = rt.tm_mon + 1;
	tm->tm_mday = rt.tm_mday;
	tm->tm_hour = rt.tm_hour;
	tm->tm_min = rt.tm_min;
	tm->tm_sec = rt.tm_sec;
}

bool tran_record_devmsg_api(enum TRAN_DEV_EXP_TYPE id, u32 msgtype, const char *fmt, ...)
{
	int size;
	struct tran_devmsg_header *header;
	struct tran_devmsg_desc *desc;
	va_list ap;

	if (IS_ERR_OR_NULL(g_tran_devmsg_buffer) || atomic_read(&tran_record_devmsg_gate))
		return false;

	if (id < 0 || id >= DEV_EXP_TYPE_END) {
		pr_notice("%s: invalid dev id: %d\n", __func__, id);
		return false;
	} else if (id == CPU_DEV_EXP && msgtype >= CPU_DEV_EXP_TYPE_END) {
		pr_notice("%s: invalid cpu dev msg type: %d\n", __func__, msgtype);
		return false;
	} else if (id == DRAM_DEV_EXP && msgtype >= DRAM_DEVMSG_TYPE_END) {
		pr_notice("%s: invalid dram dev msg type: %d\n", __func__, msgtype);
		return false;
	} else if (id == FLASH_DEV_EXP && msgtype >= FLASH_DEVMSG_TYPE_END) {
		pr_notice("%s: invalid flash dev msg type: %d\n", __func__, msgtype);
		return false;
	} else if (id == POWER_LOSS_EXP && msgtype >= POWER_EXP_DEVMSG_TYPE_END) {
		pr_notice("%s: invalid power dev msg type: %d\n", __func__, msgtype);
		return false;
	}

        if (!fmt){
                pr_notice("%s: invalid dev msg log = NULL\n", __func__);
                return false;
        }

	header = (struct tran_devmsg_header *)((unsigned long)g_tran_devmsg_buffer);
	if (strncmp(header->magic, TRAN_DEV_MSG_MAGIC, 4)) {
		pr_notice("%s: magic mismatch\n", __func__);
		return false;
	}

	if (header->offset + TRAN_DEVMSG_MAX_SIZE >= g_tran_devmsg_size) {
		pr_notice("%s: no space left to write devmsg\n", __func__);
		return false;
	}

	atomic_inc(&tran_record_devmsg_gate);
	desc = (struct tran_devmsg_desc *)((unsigned long)g_tran_devmsg_buffer + header->offset);
	desc->start = header->offset;
	va_start(ap, fmt);
	size = vscnprintf(&g_tran_devmsg_buffer[desc->start + sizeof(struct tran_devmsg_desc)], TRAN_DEVMSG_MAX_SIZE, fmt, ap);
	va_end(ap);
	desc->len = ALIGN(size, 8) + sizeof(struct tran_devmsg_desc);
	desc->id = id;
	desc->phase = DEVMSG_PHASE_KERNEL;
	desc->type = msgtype;
	desc->idx = header->count;
	tran_devmsg_time_sync(&desc->tm);
	header->count++;
	header->offset += desc->len;
	atomic_set(&tran_record_devmsg_gate, 0);

	return true;
}
EXPORT_SYMBOL(tran_record_devmsg_api);

static ssize_t tran_devmsg_proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	char msg[TRAN_DEVMSG_MAX_SIZE];

	memset(msg, 0, sizeof(msg));
	if ((size < 2) || (size > sizeof(msg))) {
		pr_notice("%s: count = %zx\n", __func__, size);
		return -EINVAL;
	}

	if (!buf) {
		pr_notice("%s: buf = NULL\n", __func__);
		return -EINVAL;
	}

	if (copy_from_user(msg, buf, size)) {
		pr_notice("%s: error\n", __func__);
		return -EFAULT;
	}
	tran_record_devmsg_api(dev_id, msg_type, msg);

	return size;
}

static int tran_devmsg_show(struct seq_file *m, void *v)
{
	int i, n;
	struct tran_devmsg_header *header;
	struct tran_devmsg_desc *desc;
	unsigned long offset;
	char buf[TRAN_DEVMSG_MAX_SIZE + SZ_64];

	if (IS_ERR_OR_NULL(g_tran_devmsg_buffer) || atomic_read(&tran_record_devmsg_gate))
		return 0;

	header = (struct tran_devmsg_header *)((unsigned long)g_tran_devmsg_buffer);
	if (strncmp(header->magic, TRAN_DEV_MSG_MAGIC, 4) || !header->count ||
		header->offset <= sizeof(struct tran_devmsg_header) || header->offset > g_tran_devmsg_size) {
		seq_printf(m, "[%s] invalid header: magic:%s, count:%d, offset:%d\n",
			__func__,  header->magic, header->count, header->offset);
		return 0;
	}

	seq_printf(m, "[%s] count:%d, offset:%d\n", __func__, header->count, header->offset);
	for (i = 0; i < header->count; i++) {
		if (!i)
			offset = sizeof(struct tran_devmsg_header);
		else
			offset += desc->len;
		desc = (struct tran_devmsg_desc *)((unsigned long)g_tran_devmsg_buffer + offset);

		memset(buf, 0, sizeof(buf));
		n = snprintf(buf, sizeof(buf), "<%d,%d,%d-%02d-%02d %02d:%02d:%02d>",
			desc->id, desc->type, desc->tm.tm_year, desc->tm.tm_mon, desc->tm.tm_mday,
			desc->tm.tm_hour, desc->tm.tm_min, desc->tm.tm_sec);
		strlcat(buf + n, g_tran_devmsg_buffer + offset + sizeof(struct tran_devmsg_desc),
			desc->len - sizeof(struct tran_devmsg_desc));
		seq_puts(m, buf);
	}

	return 0;
}

static int tran_devmsg_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_devmsg_show, NULL);
}

#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
static const struct file_operations tran_devmsg_proc_fops = {
	.owner = THIS_MODULE,
	.open = tran_devmsg_proc_open,
	.read = seq_read,
	.write	= tran_devmsg_proc_write,
	.release = single_release,
};
#else
static const struct proc_ops tran_devmsg_proc_fops = {
        .proc_open    = tran_devmsg_proc_open,
        .proc_read      = seq_read,
        .proc_lseek   = seq_lseek,
        .proc_release = single_release,
        .proc_write     = tran_devmsg_proc_write,
};
#endif

void tran_do_undefinstr(void *data, struct pt_regs *regs, unsigned long esr)
{
	tran_record_devmsg_api(CPU_DEV_EXP, UNDEFINED_INST, "Undefined instruction: pc=%pS, esr=0x%016lx", (void *)regs->pc, esr);
}

void tran_arm64_serror_panic(void *data, struct pt_regs *regs, unsigned long esr)
{
	tran_record_devmsg_api(CPU_DEV_EXP, SERROR, "SError Interrupt on CPU%d, code 0x%016lx", smp_processor_id(), esr);
}

static void tran_devmsg_record_regs(struct pt_regs *regs)
{
	u64 lr;
	char buffer[SZ_256] = {'\0'};

	if (compat_user_mode(regs))
		lr = regs->compat_lr;
	else
		lr = regs->regs[30];

	if (!user_mode(regs))
		snprintf(buffer, SZ_256, "pc:%pS, lr:%pS", (void *)regs->pc, (void *)ptrauth_strip_kernel_insn_pac(lr));
	else
		snprintf(buffer, SZ_256, "pc:%016llx, lr:%016llx", regs->pc, lr);

	tran_record_devmsg_api(DRAM_DEV_EXP, 0, "%s", buffer);
}

static int tran_devmsg_panic_notifier_call(struct notifier_block *this, unsigned long event,
								void *ptr)
{
	struct pt_regs regs;

	crash_setup_regs(&regs, NULL);
	tran_devmsg_record_regs(&regs);

	return 0;
}

static int tran_devmsg_die_notifier_call(struct notifier_block *self, unsigned long cmd, void *ptr)
{
	struct die_args *dargs = (struct die_args *)ptr;

	tran_devmsg_record_regs(dargs->regs);

	return 0;
}

static struct notifier_block tran_devmsg_panic_blk = {
	.notifier_call = tran_devmsg_panic_notifier_call,
	.priority = INT_MAX - 100,
};

static struct notifier_block tran_devmsg_die_blk = {
	.notifier_call = tran_devmsg_die_notifier_call,
	.priority = INT_MAX - 100,
};

int __init tran_devmsg_init(u32 paddr, u32 size)
{
	int ret;
	struct tran_devmsg_header *header;


	if (tran_devmsg_parse_chosen() < 0 || !enable)
		return 0;

	if (paddr > memblock_end_of_DRAM() || size != TRAN_DEVMSG_MBLOCK_SIZE) {
		pr_notice("%s: invalid tran_devmsg_mem [0x%x-0x%x]\n", __func__, paddr, size);
		return -EINVAL;
	}

	g_tran_devmsg_buffer = (char *)log_store_remap_lowmem(paddr, size);
	if (!g_tran_devmsg_buffer) {
		pr_notice("%s: remap_lowmem [0x%x, 0x%x] failed\n", __func__, paddr, size);
		return -ENOMEM;
	}
	g_tran_devmsg_size = size;
	pr_info("%s: tran_devmsg_mem:0x%x-0x%x (0x%lx)\n", __func__, paddr, size, (unsigned long)g_tran_devmsg_buffer);

	/* init kernel devmsg buffer header */
	memset_io(g_tran_devmsg_buffer, 0, g_tran_devmsg_size);
	header = (struct tran_devmsg_header *)((unsigned long)g_tran_devmsg_buffer);
	memcpy(header->magic, TRAN_DEV_MSG_MAGIC, 4);
	header->offset = sizeof(struct tran_devmsg_header);

	proc_entry = proc_create("tran_devmsg", S_IFREG | S_IRUGO, NULL, &tran_devmsg_proc_fops);
	if (!proc_entry) {
		pr_err("%s: proc_create tran_devmsg failed\n", __func__);
		return -ENOMEM;
	}

	ret = register_trace_android_rvh_do_el1_undef(tran_do_undefinstr, NULL);
	if (ret) {
		pr_err("register_trace_android_rvh_do_el1_undef failed, ret=%d\n", ret);
		return ret;
	}

	ret = register_trace_android_rvh_arm64_serror_panic(tran_arm64_serror_panic, NULL);
	if (ret) {
		pr_err("register_trace_android_rvh_arm64_serror_panic_failed, ret=%d\n", ret);
		return ret;
	}

	atomic_notifier_chain_register(&panic_notifier_list, &tran_devmsg_panic_blk);
	register_die_notifier(&tran_devmsg_die_blk);

	return 0;
}

void __exit tran_devmsg_exit(void)
{
	proc_remove(proc_entry);
	atomic_notifier_chain_unregister(&panic_notifier_list, &tran_devmsg_panic_blk);
	unregister_die_notifier(&tran_devmsg_die_blk);
}

