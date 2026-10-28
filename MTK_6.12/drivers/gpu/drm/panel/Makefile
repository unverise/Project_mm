# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat

ifneq ($(filter y m, $(CONFIG_MTK_MME_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mme
endif

obj-$(CONFIG_NT36523N_WQXGA_DSI_VDO_DSC_HJR_90HZ_X1103) += nt36523n_wqxga_dsi_vdo_dsc_hjr_90hz_x1103.o
obj-$(CONFIG_TRAN_DRM_PANEL_I2C) += tran_drm_panel_i2c.o
#BSP: add ko5 lcm_drv by minlong.gao 20260129 start
obj-$(CONFIG_ICNL9916X_HDPLUS1576_DSI_CSOT_HKC_120HZ_KO5) += icnl9916x_hdplus1576_dsi_vdo_csot_120hz_ko5.o
obj-$(CONFIG_TD4160_HDPLUS1576_DSI_VDO_DPT_HKC_120HZ_KO5) += td4160_hdplus1576_dsi_vdo_dpt_hkc_120hz_ko5.o
#BSP: add ko5 lcm_drv by minlong.gao 20260129 end
obj-$(CONFIG_DRM_PANEL_NT37801_MSYNC2_RTE) += panel-nt37801-msync2-rte.o
obj-$(CONFIG_DRM_PANEL_BOE_TV106C9MLL0) += ocp2138_i2c.o
obj-$(CONFIG_DRM_PANEL_MAXIAM_MAX96851) += panel-maxiam-max96851.o
#USD:add x6887+25ms lcm_drv by long.chen 20250704 start
obj-$(CONFIG_NT37706A_FHDP_DSI_VDO_DSC_BOE_BOE_144HZ_X6887) += nt37706a_fhdp_dsi_vdo_dsc_boe_boe_144hz_x6887.o
#USD:add x6887+25ms lcm_drv by long.chen 20250704 end
#USD:add lk7k lcm_drv by bing.he 20250811 start
obj-$(CONFIG_NT37706A_FHDP_DSI_VDO_DSC_BOE_BOE_144HZ_LK7K) += nt37706a_fhdp_dsi_vdo_dsc_boe_boe_144hz_lk7k.o
obj-$(CONFIG_NT37706A_FHDP_DSI_VDO_DSC_BOE_BOE_144HZ_LK7K_OP72) += nt37706a_fhdp_dsi_vdo_dsc_boe_boe_144hz_lk7k_old.o
#USD:add lk7k lcm_drv by bing.he 20250811 end
#USD:add x6898 lcm_drv by bing.he 20251016 start
obj-$(CONFIG_NT37706A_FHDP_DSI_VDO_DSC_BOE_BOE_144HZ_X6898) += nt37706a_fhdp_dsi_vdo_dsc_boe_boe_144hz_x6898.o
obj-$(VTDR6126A_FHDP_DSI_VDO_DSC_TM_TM_120HZ_X6898) += vtdr6126a_fhdp_dsi_vdo_dsc_tm_tm_120hz_x6898.o
#USD:add x6898 lcm_drv by bing.he 20251016 end
#USD:add x6895b lcm_drv by shiyu.cai 20251104 start
obj-$(CONFIG_NT36528A_HDP_DSI_VDO_TM_TM_120HZ_X6895B) += nt36528a_hdp_dsi_vdo_tm_tm_120hz_x6895b.o
obj-$(CONFIG_FT5087M_HDP_DSI_VDO_TXD_BOE_120HZ_X6895B) += ft8057m_hdp_dsi_vdo_txd_boe_120hz_x6895b.o
obj-$(CONFIG_NT36528A_HDP_DSI_VDO_TM_TM_120HZ_X6895) += nt36528a_hdp_dsi_vdo_tm_tm_120hz_x6895.o
obj-$(CONFIG_FT5087M_HDP_DSI_VDO_TXD_BOE_120HZ_X6895) += ft8057m_hdp_dsi_vdo_txd_boe_120hz_x6895.o
#USD:add x6895b lcm_drv by shiyu.cai 20251104 end
#USD:add lk6 lcm_drv by weihong.zu 20251120 start
obj-$(CONFIG_FT8725_FHDP_DSI_VDO_DSC_TM_TM_144HZ_LK6) += ft8725_fhdp_dsi_vdo_dsc_tm_tm_144hz_lk6.o
#USD:add lk6 lcm_drv by weihong.zu 20251120 end
obj-$(CONFIG_DRM_PANEL_VTDR6126A_VDO_144HZ) += panel-vtdr6126a-vdo-144hz.o

ccflags-$(CONFIG_DRM_MEDIATEK_PANEL) += -I$(DEVICE_MODULES_PATH)/drivers/leds/
#BSP: add t1103 lcm_drv by shude.wan 20251229 start
obj-$(CONFIG_NT36523N_FHDP_DSI_VDO_DSC_HJR_CSOT_90HZ_T1103) += nt36523n_fhdp_dsi_vdo_dsc_hjr_csot_90hz_t1103.o
#BSP: add t1103 lcm_drv by shude.wan 20251229 end
#USD:add x6896 lcm_drv by yishi.xun 20260115 start
obj-$(CONFIG_FT8725_FHDP_DSI_VDO_DSC_TM_TM_144HZ_X6896) += ft8725_fhdp_dsi_vdo_dsc_tm_tm_144hz_x6896.o
#USD:add x6896 lcm_drv by yishi.xun 20260115 end
#USD: add X6855/X6858 lcm drv by qian.wu3 20260126 start
obj-$(CONFIG_NT37703A_FHDP_DSI_VDO_DSC_TM_TM_144HZ_X6855) += nt37703a_fhdp_dsi_vdo_dsc_tm_tm_144hz_x6855.o
obj-$(CONFIG_NT37703A_FHDP_DSI_VDO_DSC_TM_TM_144HZ_X6858) += nt37703a_fhdp_dsi_vdo_dsc_tm_tm_144hz_x6858.o
obj-$(CONFIG_RM69220_FHDP_DSI_VDO_DSC_CSOT_CSOT_144HZ_X6858) += rm69220_fhdp_dsi_vdo_dsc_csot_csot_144hz_x6858.o
#USD: add X6855/X6858 lcm drv by qian.wu3 20260126 end
