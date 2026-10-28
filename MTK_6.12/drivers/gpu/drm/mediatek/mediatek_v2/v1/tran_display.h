/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __TRAN_DISPLAY_H__
#define __TRAN_DISPLAY_H__
#include "mtk_dsi.h"

#define DDP_DSI_COUNT 2

enum DDP_DSI_NUM {
	DDP_DSI0 = 0,
	DDP_DSI1,
	DDP_DSI_MAX
};

struct lcm_node {
	int tran_backlight;
	enum TRAN_LCM_BACKLIGHT_TYPE tran_backlight_type;
};

void tran_dsi_init(struct mtk_dsi* dsi);
struct mtk_dsi* tran_get_dsi(enum DDP_DSI_NUM index);
int mtkfb_register_kernelfs(struct device *dev);
int mtkfb_register_sub_kernelfs(struct device *dev);
enum TRAN_LCM_BACKLIGHT_TYPE tran_lcm_backlight_get_type(void);
//USD: added for X6895AEE-5207 by shiyu.cai 20260422 start
bool tran_get_bl_opt_support(void);
//USD: added for X6895AEE-5207 by shiyu.cai 20260422 end
#endif
