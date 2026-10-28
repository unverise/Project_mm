/* SPDX-License-Identifier: GPL-2.0*/
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_PLATFORM_8668_GUEST_H
#define MTK_DRM_PLATFORM_8668_GUEST_H

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif
#include "mtk_drm_ddp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_disp_ovl.h"
#include "mtk_disp_wdma.h"
#include "mtk_mipi_tx.h"
#include "mtk_disp_dsc.h"
#include "mtk_disp_postmask.h"
#include "mtk_disp_rdma.h"
#include "mtk_disp_rsz.h"
#include "mtk_disp_aal.h"
#include "mtk_disp_ccorr.h"
#include "mtk_disp_color.h"
#include "mtk_disp_dither.h"
#include "mtk_dsi.h"

#define OVL_MAX_LAYER_NR	2
extern struct mtk_crtc_path_data  mt8668_mtk_main_path_data;



#endif

