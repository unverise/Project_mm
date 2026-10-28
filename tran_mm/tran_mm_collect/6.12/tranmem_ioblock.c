/**
  *Copyright 2023 Transsion Inc
  *Author transsioner
  *tran_mm_collect feature suppport
*/

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mmzone.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "tranmem_ioblock.h"
#include "tranmem_emem.h"

#define CREATE_TRACE_POINTS
#include "tranmem_event.h"

#define MAX_SYMBOL_LEN	64

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0))
#define TRANMM_SYMBOL_NAME_BLOCK    "folio_wait_bit_common"
#define TRANMM_USE_FOLIO 1
#else
#define TRANMM_SYMBOL_NAME_BLOCK    "lock_page_maybe_drop_mmap"
#endif

#define NR_KPROBES      3

static char kp_symbol[NR_KPROBES][MAX_SYMBOL_LEN] = {
	"workingset_refault",
	"__mod_lruvec_state",
	TRANMM_SYMBOL_NAME_BLOCK
};

/* For each probe you need to allocate a kprobe structure */
static struct kprobe kp[NR_KPROBES];

static kprobe_pre_handler_t pre_handlers[NR_KPROBES];

enum tranmem_ioblock_state {
	STATE_ENABLED = 1,
	STATE_DISABLED
};
int tranmem_ioblock_enable = STATE_DISABLED;

static int __kprobes workingset_refault_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	u64 folio = 0;

	if (!tracepoint_enabled(tranmem_workingset_refault))
		return 0;

	folio = regs->regs[0]; //workingset_refault(struct folio *folio, ...)

	trace_tranmem_workingset_refault(folio);
	return 0;
}

static int __kprobes lruvec_state_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	int folio_state = 0;
	if (!tracepoint_enabled(tranmem_lruvec_state))
		return 0;

	folio_state = (int)regs->regs[1];    //__mod_lruvec_state(struct lruvec *lruvec, enum node_stat_item idx,...)
	if (folio_state <= WORKINGSET_REFAULT_BASE)
		return 0;

	trace_tranmem_lruvec_state(folio_state);
	return 0;
}

static int __kprobes ioblock_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	u64 folio = 0;

	if (!tracepoint_enabled(tranmem_ioblock))
		return 0;

	//lock_page_maybe_drop_mmap(struct vm_fault *vmf, struct page *page, ...)
	//folio_wait_bit_common(struct folio *folio, ...)
#ifdef TRANMM_USE_FOLIO
	folio = regs->regs[0];
#else
	folio = regs->regs[1];
#endif

	trace_tranmem_ioblock(folio);
	return 0;
}

static int register_tranmem_ioblock_kprobes(void)
{
	int i = 0;
	int error_id = 0;
	int ret = 0;

	pre_handlers[0] = workingset_refault_handler_pre;
	pre_handlers[1] = lruvec_state_handler_pre;
	pre_handlers[2] = ioblock_handler_pre;

	for (; i < NR_KPROBES; ++i) {
		memset(&kp[i], 0, sizeof(struct kprobe));

		kp[i].symbol_name = kp_symbol[i];
		kp[i].pre_handler = pre_handlers[i];

		ret = register_kprobe(&kp[i]);
		if (ret < 0) {
			error_id = i;
			pr_err(TRANMM"register_kprobe failed, returned %d\n", ret);
			goto reg_out;
		}

		pr_info(TRANMM"Planted kprobe at %s\n", kp[i].symbol_name);
	}

	return 0;

reg_out:
	for (i = 0; i < error_id; ++i) {
		unregister_kprobe(&kp[i]);
	}

	return ret;
}

static void unregister_tranmem_ioblock_kprobes(void)
{
	int i = 0;

	for (; i < NR_KPROBES; ++i) {
		unregister_kprobe(&kp[i]);
		pr_info(TRANMM"kprobe at %s unregistered\n", kp[i].symbol_name);
	}
}

void tranmem_ioblock_cloud_changed(int new_state)
{
	/*
	 * We can't determine artificially set values, so we need to
	 * make appropriate corrections.
	 */
	if (new_state != STATE_ENABLED)
		new_state = STATE_DISABLED;

	if (tranmem_ioblock_enable == new_state)
		return;

	if (new_state == STATE_ENABLED)
		register_tranmem_ioblock_kprobes();
	else
		unregister_tranmem_ioblock_kprobes();

	tranmem_ioblock_enable = new_state;
}

