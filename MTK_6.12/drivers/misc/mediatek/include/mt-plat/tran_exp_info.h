/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 Transsion Inc.
 */

#if !defined(__TRAN_EXP_INFO_H__)
#define __TRAN_EXP_INFO_H__

#include <linux/types.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/stacktrace.h>

//USD: keep it consistent with tran_pdb by ningning.gao 20260310 start
#define TRAN_EXP_TYPE_LEN	16
#define TRAN_EXP_KEYINFO_LEN	1008
#define TRAN_EXP_DETAIL_LEN	3072
//USD: keep it consistent with tran_pdb by ningning.gao 20260310 end

/* The size of tran_exp_info is 4K */
struct tran_exp_info {
	char exp_type[TRAN_EXP_TYPE_LEN];
	char exp_keyinfo[TRAN_EXP_KEYINFO_LEN];
	char exp_detail[TRAN_EXP_DETAIL_LEN];
};
extern struct tran_exp_info *tran_expinfo;

static inline void tran_exp_type_save(const char *type)
{
	if (tran_expinfo)
		strscpy(tran_expinfo->exp_type, type, TRAN_EXP_TYPE_LEN);
}

static inline void tran_exp_detail_save(const char *fmt, ...)
{
	va_list args;
	static int sz = 0;

	if (!tran_expinfo || sz >= TRAN_EXP_DETAIL_LEN)
		return;

	va_start(args, fmt);
	sz += vscnprintf(tran_expinfo->exp_detail + sz, TRAN_EXP_DETAIL_LEN - sz, fmt, args);
	va_end(args);
}

static inline void tran_exp_keyinfo_save(const char *fmt, ...)
{
	va_list args;
	static int sz = 0;

	if (!tran_expinfo || sz >= TRAN_EXP_KEYINFO_LEN)
		return;

	va_start(args, fmt);
	sz += vscnprintf(tran_expinfo->exp_keyinfo + sz, TRAN_EXP_KEYINFO_LEN - sz, fmt, args);
	va_end(args);
}

static inline void tran_save_tsk_kernel_bt(struct task_struct *tsk, unsigned int skipnr)
{
	unsigned long stacks[32];
	int nr_entries, i;

	nr_entries = stack_trace_save_tsk(tsk, stacks, ARRAY_SIZE(stacks), skipnr);
	for (i = 0; i < nr_entries; i++)
		tran_exp_detail_save("%pS\n", (void *)stacks[i]);
	tran_exp_detail_save("%s", "\n");
}

#endif/* __TRAN_EXP_INFO_H__ */
