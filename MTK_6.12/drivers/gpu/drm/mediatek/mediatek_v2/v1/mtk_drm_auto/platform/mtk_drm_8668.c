// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include "../mtk_drm_drv_auto.h"
#include "mtk_drm_platform_auto.h"

//guest
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
static const enum mtk_ddp_comp_id mt8668_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_DSI0_VIRTUAL,
};
#endif

//host
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
static const enum mtk_ddp_comp_id mt8668_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_PQ0_IN_CB0,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB4,
#else
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,	 DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,	 DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,	     DDP_COMPONENT_AAL0,
	DDP_COMPONENT_GAMMA0,	 DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_DITHER0,	 DDP_COMPONENT_PQ0_OUT_CB0,
#endif
	DDP_COMPONENT_SPLIT_OUT_CB2,
	DDP_COMPONENT_COMP0_OUT_CB2,
	DDP_COMPONENT_DSI0,
	//DDP_COMPONENT_PWM0,
};
#endif

//signal android
#ifdef MTK_DRM_MEDIATEK_AUTO_AN_ONLY
static const enum mtk_ddp_comp_id mt8668_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_PQ0_IN_CB0,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB4,
#else
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,	 DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,	 DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,	     DDP_COMPONENT_AAL0,
	DDP_COMPONENT_GAMMA0,	 DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_DITHER0,	 DDP_COMPONENT_PQ0_OUT_CB0,
#endif
	DDP_COMPONENT_SPLIT_OUT_CB2,
	DDP_COMPONENT_COMP0_OUT_CB2,
	DDP_COMPONENT_DSI0,
	//DDP_COMPONENT_PWM0,
};
#endif

static const struct mtk_addon_scenario_data mt8668_addon_main[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
};

static const enum mtk_ddp_comp_id mt8668_scaling_main[] = {
	DDP_COMPONENT_RSZ0,
};


struct mtk_crtc_path_data mt8668_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt8668_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt8668_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt8668_addon_main,
	.scaling_data = mt8668_scaling_main,
	.is_path_enable = true,
};
EXPORT_SYMBOL(mt8668_mtk_main_path_data);

