/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of_graph.h>
#include <linux/power_supply.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
//#include "../mediatek/mediatek_v2/mtk_log.h"
#endif
#include "tran_drm_panel_i2c.h"
#include <linux/kernel.h>
//#include "./tran_drm_i2c_drv/tran_drm_panel_i2c.h"
//extern unsigned int get_lcm_status;
//BSP: modify  LGQNAEEEX-1 by hongwu.zhan 20220509 start
#define TRAN_CTP_GESTURE_FUN
//BSP: modify  LGQNAEEEX-1 by hongwu.zhan 20220509 end
//SR:add for esd recovery skip deep standby mode by shude.wan 2024-06-04 start
//extern bool is_esd_recovery;
//SR:add for esd recovery skip deep standby mode by shude.wan 2024-06-04 end
#if defined(TRAN_CTP_GESTURE_FUN)
/*This variable is for kernel-5.10 TP new driver frame*/
static int tran_new_gesture_fun_enable = 0;
#endif
extern u32 tran_call_tp_cmd_handle(u32 cmd, void *pdata);
static unsigned int tran_is_lcm_poweroff = 0; //0:lcm_power state is poweron, 1:lcm_power state is poweroff
static unsigned int tran_dim_state = 3;
static char bl_dimming_enable[] = {0x53, 0x2C};
static char bl_dimming_disable[] = {0x53, 0x24};
//BSP: modify X1102M1201-865 low power set maxbacklight by qihui.zhang 20250710 start
static int g_lcm_uisoc = 75;
static struct notifier_block lcm_psy_nb;
//BSP: modify X1102M1201-865 low power set maxbacklight by qihui.zhang 20250710 end
#define REGFLAG_DELAY           0xFFFC
#define REGFLAG_UDELAY          0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW       0xFFFE
#define REGFLAG_RESET_HIGH      0xFFFF
#define PANEL_CLOCK             414831

#define FRAME_WIDTH                 1600
#define FRAME_HEIGHT                2560

#define PHYSICAL_WIDTH              147410
#define PHYSICAL_HEIGHT             235850

#define DATA_RATE                   1010
/*BSP:add DATA_RATE_KHZ for the refresh rate require of pen by shude.wan 2026-01-06 start*/
#define DATA_RATE_KHZ		    1010280
/*BSP:add DATA_RATE_KHZ for the refresh rate require of pen by shude.wan 2026-01-06 end*/
#define VSA                         18
#define VBP                         176
#define HSA                         10
#define HBP                         20
#define HFP                         30

//Parameter setting for mode_0(60Hz)
#define MODE_0_FPS                  60
#define MODE_0_VFP                  1422
//Parameter setting for mode_1(90Hz)
#define MODE_1_FPS                  90
#define MODE_1_VFP                  30

#define DSC_RC_BUF_THRESH_0            896
#define DSC_RC_BUF_THRESH_1            1792
#define DSC_RC_BUF_THRESH_2            2688
#define DSC_RC_BUF_THRESH_3            3584
#define DSC_RC_BUF_THRESH_4            4480
#define DSC_RC_BUF_THRESH_5            5376
#define DSC_RC_BUF_THRESH_6            6272
#define DSC_RC_BUF_THRESH_7            6720
#define DSC_RC_BUF_THRESH_8            7168
#define DSC_RC_BUF_THRESH_9            7616
#define DSC_RC_BUF_THRESH_10           7744
#define DSC_RC_BUF_THRESH_11           7872
#define DSC_RC_BUF_THRESH_12           8000
#define DSC_RC_BUF_THRESH_13           8064
#define DSC_RANGE_MIN_QP_0              0
#define DSC_RANGE_MIN_QP_1              0
#define DSC_RANGE_MIN_QP_2              1
#define DSC_RANGE_MIN_QP_3              1
#define DSC_RANGE_MIN_QP_4              3
#define DSC_RANGE_MIN_QP_5              3
#define DSC_RANGE_MIN_QP_6              3
#define DSC_RANGE_MIN_QP_7              3
#define DSC_RANGE_MIN_QP_8              3
#define DSC_RANGE_MIN_QP_9              3
#define DSC_RANGE_MIN_QP_10             5
#define DSC_RANGE_MIN_QP_11             5
#define DSC_RANGE_MIN_QP_12             5
#define DSC_RANGE_MIN_QP_13             9
#define DSC_RANGE_MIN_QP_14             12
#define DSC_RANGE_MAX_QP_0              4
#define DSC_RANGE_MAX_QP_1              4
#define DSC_RANGE_MAX_QP_2              5
#define DSC_RANGE_MAX_QP_3              6
#define DSC_RANGE_MAX_QP_4              7
#define DSC_RANGE_MAX_QP_5              7
#define DSC_RANGE_MAX_QP_6              7
#define DSC_RANGE_MAX_QP_7              8
#define DSC_RANGE_MAX_QP_8              9
#define DSC_RANGE_MAX_QP_9             10
#define DSC_RANGE_MAX_QP_10            10
#define DSC_RANGE_MAX_QP_11            11
#define DSC_RANGE_MAX_QP_12            11
#define DSC_RANGE_MAX_QP_13            12
#define DSC_RANGE_MAX_QP_14            13
#define DSC_RANGE_BPG_OFFSET_0          2
#define DSC_RANGE_BPG_OFFSET_1          0
#define DSC_RANGE_BPG_OFFSET_2          0
#define DSC_RANGE_BPG_OFFSET_3         -2
#define DSC_RANGE_BPG_OFFSET_4         -4
#define DSC_RANGE_BPG_OFFSET_5         -6
#define DSC_RANGE_BPG_OFFSET_6         -8
#define DSC_RANGE_BPG_OFFSET_7         -8
#define DSC_RANGE_BPG_OFFSET_8         -8
#define DSC_RANGE_BPG_OFFSET_9        -10
#define DSC_RANGE_BPG_OFFSET_10       -10
#define DSC_RANGE_BPG_OFFSET_11       -12
#define DSC_RANGE_BPG_OFFSET_12       -12
#define DSC_RANGE_BPG_OFFSET_13       -12
#define DSC_RANGE_BPG_OFFSET_14       -12


static unsigned int dsc_range_min_qp[15] = {
    DSC_RANGE_MIN_QP_0,  DSC_RANGE_MIN_QP_1,  DSC_RANGE_MIN_QP_2,
    DSC_RANGE_MIN_QP_3,  DSC_RANGE_MIN_QP_4,  DSC_RANGE_MIN_QP_5,
    DSC_RANGE_MIN_QP_6,  DSC_RANGE_MIN_QP_7,  DSC_RANGE_MIN_QP_8,
    DSC_RANGE_MIN_QP_9,  DSC_RANGE_MIN_QP_10, DSC_RANGE_MIN_QP_11,
    DSC_RANGE_MIN_QP_12, DSC_RANGE_MIN_QP_13, DSC_RANGE_MIN_QP_14
};

static unsigned int dsc_range_max_qp[15] = {
    DSC_RANGE_MAX_QP_0,  DSC_RANGE_MAX_QP_1,  DSC_RANGE_MAX_QP_2,
    DSC_RANGE_MAX_QP_3,  DSC_RANGE_MAX_QP_4,  DSC_RANGE_MAX_QP_5,
    DSC_RANGE_MAX_QP_6,  DSC_RANGE_MAX_QP_7,  DSC_RANGE_MAX_QP_8,
    DSC_RANGE_MAX_QP_9,  DSC_RANGE_MAX_QP_10, DSC_RANGE_MAX_QP_11,
    DSC_RANGE_MAX_QP_12, DSC_RANGE_MAX_QP_13, DSC_RANGE_MAX_QP_14
};

static int dsc_range_bpg_offset[15] = {
    DSC_RANGE_BPG_OFFSET_0,  DSC_RANGE_BPG_OFFSET_1,
    DSC_RANGE_BPG_OFFSET_2,  DSC_RANGE_BPG_OFFSET_3,
    DSC_RANGE_BPG_OFFSET_4,  DSC_RANGE_BPG_OFFSET_5,
    DSC_RANGE_BPG_OFFSET_6,  DSC_RANGE_BPG_OFFSET_7,
    DSC_RANGE_BPG_OFFSET_8,  DSC_RANGE_BPG_OFFSET_9,
    DSC_RANGE_BPG_OFFSET_10, DSC_RANGE_BPG_OFFSET_11,
    DSC_RANGE_BPG_OFFSET_12, DSC_RANGE_BPG_OFFSET_13,
    DSC_RANGE_BPG_OFFSET_14
};
static unsigned int dsc_rc_buf_thresh[14] ={
	DSC_RC_BUF_THRESH_0,  DSC_RC_BUF_THRESH_1,
    DSC_RC_BUF_THRESH_2,  DSC_RC_BUF_THRESH_3,
	DSC_RC_BUF_THRESH_4,  DSC_RC_BUF_THRESH_5,
	DSC_RC_BUF_THRESH_6,  DSC_RC_BUF_THRESH_7,
	DSC_RC_BUF_THRESH_8,  DSC_RC_BUF_THRESH_9,
	DSC_RC_BUF_THRESH_10, DSC_RC_BUF_THRESH_11,
	DSC_RC_BUF_THRESH_12, DSC_RC_BUF_THRESH_13
};
// ---------------------------------------------------------------------------
//  LCM power is provided by I2C
// ---------------------------------------------------------------------------
//extern int mtk_panel_i2c_write_bytes(unsigned char addr, unsigned char value);
extern int tran_panel_i2c_write_bytes(unsigned char addr, unsigned char value);
static char bl_tb0[] = {0x51, 0x0F, 0xFF};

//SR:add for esd recovery by shude.wan 20231128 start
static unsigned int bl_51_dbv;
//SR:add for esd recovery by shude.wan 20231128 end

struct OCP2131_SETTING_TABLE {
	unsigned char cmd;
	unsigned char data;
};

static struct OCP2131_SETTING_TABLE ocp2131_cmd_data[3] = {
	{ 0x00, 0x14 }, //AVDD +6.0
	{ 0x01, 0x14 }, //AVEE -6.0
	{ 0x03, 0x43 }, //AVEE 150mA 0x03->0x43
};
//USD:add for aod by shude.wan 20260109 start
static struct tran_panel_driver_params panel_driver_status = {0};
static tran_lcm_doze_backlight g_lcm_doze_backlight = {
	.doze_backlight_num = 0,
	.doze_backlight_level1 = 0,
	.doze_backlight_level2 = 0,
	.doze_backlight_level3 = 0,
};
//USD:add for aod by shude.wan 20260109 end


struct lcm {
	struct device *dev;
	struct drm_panel panel;
//	struct backlight_device *backlight; //For OLED backlight set
	struct gpio_desc *reset_gpio;
	struct gpio_desc *tp_reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;

	bool prepared;
	bool enabled;
	int error;
};

#define lcm_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_dbg(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}
	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;
	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;
	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_dbg(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static void lcm_panel_init(struct lcm *ctx)
{
    //-------------  Display Initial Code Setting  -------------------------
	lcm_dcs_write_seq_static(ctx,0xFF,0xF0);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0x24,0x02);
	lcm_dcs_write_seq_static(ctx,0xFF,0xE0);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0x14,0x60);
	lcm_dcs_write_seq_static(ctx,0x16,0xC0);
	lcm_dcs_write_seq_static(ctx,0xFF,0x20);
	lcm_dcs_write_seq_static(ctx,0x18,0x04);
	lcm_dcs_write_seq_static(ctx,0x19,0x02);

	lcm_dcs_write_seq_static(ctx,0xFF,0x20);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0x05,0xB1);
	lcm_dcs_write_seq_static(ctx,0x06,0xC0);
	lcm_dcs_write_seq_static(ctx,0x07,0x7D);
	lcm_dcs_write_seq_static(ctx,0x08,0x5F);
	lcm_dcs_write_seq_static(ctx,0x09,0x99);
	lcm_dcs_write_seq_static(ctx,0x0A,0x88);
	lcm_dcs_write_seq_static(ctx,0x0B,0x10);
	lcm_dcs_write_seq_static(ctx,0x0C,0x00);
	lcm_dcs_write_seq_static(ctx,0x0D,0x63);
	lcm_dcs_write_seq_static(ctx,0x0E,0x96);
	lcm_dcs_write_seq_static(ctx,0x0F,0x78);
	lcm_dcs_write_seq_static(ctx,0x30,0x50);
	lcm_dcs_write_seq_static(ctx,0x31,0x02);
	lcm_dcs_write_seq_static(ctx,0x32,0x42);
	lcm_dcs_write_seq_static(ctx,0x58,0x43);
	lcm_dcs_write_seq_static(ctx,0x69,0xDD);
	lcm_dcs_write_seq_static(ctx,0x65,0x55);
	lcm_dcs_write_seq_static(ctx,0x6D,0xAA);
	lcm_dcs_write_seq_static(ctx,0x6E,0x00);
	lcm_dcs_write_seq_static(ctx,0x75,0xC4);
	lcm_dcs_write_seq_static(ctx,0x7C,0x22);
	lcm_dcs_write_seq_static(ctx,0x7D,0x99);
	lcm_dcs_write_seq_static(ctx,0x88,0x00);
	lcm_dcs_write_seq_static(ctx,0x94,0xC0);
	lcm_dcs_write_seq_static(ctx,0x95,0x09);
	lcm_dcs_write_seq_static(ctx,0x96,0x09);
	lcm_dcs_write_seq_static(ctx,0xE5,0x55,0x55,0x55,0x55,0x99,0x99,0x99,0x99,0xAA,0xAA,0xAA,0xAA);

	lcm_dcs_write_seq_static(ctx,0xFF,0x24);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0x00,0x03);
	lcm_dcs_write_seq_static(ctx,0x01,0x08);
	lcm_dcs_write_seq_static(ctx,0x02,0x1C);
	lcm_dcs_write_seq_static(ctx,0x03,0x1D);
	lcm_dcs_write_seq_static(ctx,0x04,0x03);
	lcm_dcs_write_seq_static(ctx,0x05,0x03);
	lcm_dcs_write_seq_static(ctx,0x06,0x03);
	lcm_dcs_write_seq_static(ctx,0x07,0x03);
	lcm_dcs_write_seq_static(ctx,0x08,0x03);
	lcm_dcs_write_seq_static(ctx,0x09,0x26);
	lcm_dcs_write_seq_static(ctx,0x0A,0x0F);
	lcm_dcs_write_seq_static(ctx,0x0B,0x0E);
	lcm_dcs_write_seq_static(ctx,0x0C,0x0D);
	lcm_dcs_write_seq_static(ctx,0x0D,0x0C);
	lcm_dcs_write_seq_static(ctx,0x0E,0x04);
	lcm_dcs_write_seq_static(ctx,0x0F,0x26);
	lcm_dcs_write_seq_static(ctx,0x10,0x03);
	lcm_dcs_write_seq_static(ctx,0x11,0x03);
	lcm_dcs_write_seq_static(ctx,0x12,0x03);
	lcm_dcs_write_seq_static(ctx,0x13,0x03);
	lcm_dcs_write_seq_static(ctx,0x14,0x03);
	lcm_dcs_write_seq_static(ctx,0x15,0x03);
	lcm_dcs_write_seq_static(ctx,0x16,0x03);
	lcm_dcs_write_seq_static(ctx,0x17,0x08);
	lcm_dcs_write_seq_static(ctx,0x18,0x1C);
	lcm_dcs_write_seq_static(ctx,0x19,0x1D);
	lcm_dcs_write_seq_static(ctx,0x1A,0x03);
	lcm_dcs_write_seq_static(ctx,0x1B,0x03);
	lcm_dcs_write_seq_static(ctx,0x1C,0x03);
	lcm_dcs_write_seq_static(ctx,0x1D,0x03);
	lcm_dcs_write_seq_static(ctx,0x1E,0x03);
	lcm_dcs_write_seq_static(ctx,0x1F,0x26);
	lcm_dcs_write_seq_static(ctx,0x20,0x0F);
	lcm_dcs_write_seq_static(ctx,0x21,0x0E);
	lcm_dcs_write_seq_static(ctx,0x22,0x0D);
	lcm_dcs_write_seq_static(ctx,0x23,0x0C);
	lcm_dcs_write_seq_static(ctx,0x24,0x04);
	lcm_dcs_write_seq_static(ctx,0x25,0x26);
	lcm_dcs_write_seq_static(ctx,0x26,0x03);
	lcm_dcs_write_seq_static(ctx,0x27,0x03);
	lcm_dcs_write_seq_static(ctx,0x28,0x03);
	lcm_dcs_write_seq_static(ctx,0x29,0x03);
	lcm_dcs_write_seq_static(ctx,0x2A,0x03);
	lcm_dcs_write_seq_static(ctx,0x2B,0x03);
	//STV
	lcm_dcs_write_seq_static(ctx,0x2D,0x00);
	lcm_dcs_write_seq_static(ctx,0x2F,0x05);
	lcm_dcs_write_seq_static(ctx,0x30,0x00);
	lcm_dcs_write_seq_static(ctx,0x33,0x05);
	lcm_dcs_write_seq_static(ctx,0x34,0x00);
	lcm_dcs_write_seq_static(ctx,0x37,0x66);
	lcm_dcs_write_seq_static(ctx,0x39,0x00);
	lcm_dcs_write_seq_static(ctx,0x3A,0x25);
	lcm_dcs_write_seq_static(ctx,0x3B,0x59);
	lcm_dcs_write_seq_static(ctx,0x3D,0x91);
	lcm_dcs_write_seq_static(ctx,0xAB,0x55);
	//RESET
	lcm_dcs_write_seq_static(ctx,0x3F,0x09);
	lcm_dcs_write_seq_static(ctx,0x43,0x09);
	lcm_dcs_write_seq_static(ctx,0x47,0x40);
	lcm_dcs_write_seq_static(ctx,0x49,0x00);
	lcm_dcs_write_seq_static(ctx,0x4A,0x62);
	lcm_dcs_write_seq_static(ctx,0x4B,0x42);
	lcm_dcs_write_seq_static(ctx,0x4C,0x01);		  
	//CK  
	lcm_dcs_write_seq_static(ctx,0x4D,0x21);
	lcm_dcs_write_seq_static(ctx,0x4E,0x43);
	lcm_dcs_write_seq_static(ctx,0x4F,0x00);
	lcm_dcs_write_seq_static(ctx,0x50,0x00);
	lcm_dcs_write_seq_static(ctx,0x51,0x00);
	lcm_dcs_write_seq_static(ctx,0x52,0x00);
	lcm_dcs_write_seq_static(ctx,0x53,0x00);
	lcm_dcs_write_seq_static(ctx,0x54,0x00);
	lcm_dcs_write_seq_static(ctx,0x55,0x83,0x03);
	lcm_dcs_write_seq_static(ctx,0x56,0x04);
	lcm_dcs_write_seq_static(ctx,0x58,0x21);
	lcm_dcs_write_seq_static(ctx,0x59,0x40);
	lcm_dcs_write_seq_static(ctx,0x5A,0x59);
	lcm_dcs_write_seq_static(ctx,0x5B,0x4C);
	lcm_dcs_write_seq_static(ctx,0x5C,0x8F);
	lcm_dcs_write_seq_static(ctx,0x5D,0x00);
	lcm_dcs_write_seq_static(ctx,0x5E,0x00,0x08);
	lcm_dcs_write_seq_static(ctx,0x60,0xC8);
	lcm_dcs_write_seq_static(ctx,0x61,0x00);
	lcm_dcs_write_seq_static(ctx,0x63,0xA0);
	lcm_dcs_write_seq_static(ctx,0x7A,0x00);
	lcm_dcs_write_seq_static(ctx,0x7B,0x44);
	lcm_dcs_write_seq_static(ctx,0x7E,0x20);
	lcm_dcs_write_seq_static(ctx,0x7F,0x64);
	lcm_dcs_write_seq_static(ctx,0x80,0x00);
	lcm_dcs_write_seq_static(ctx,0x81,0x00);
	lcm_dcs_write_seq_static(ctx,0x82,0x08);
	lcm_dcs_write_seq_static(ctx,0x83,0x1B);
	lcm_dcs_write_seq_static(ctx,0x97,0xC3);
	lcm_dcs_write_seq_static(ctx,0x9F,0x00);
	lcm_dcs_write_seq_static(ctx,0xA0,0x00);
	lcm_dcs_write_seq_static(ctx,0x91,0x40);
	lcm_dcs_write_seq_static(ctx,0x92,0x62);
	lcm_dcs_write_seq_static(ctx,0x93,0x1E,0x00);
	lcm_dcs_write_seq_static(ctx,0x94,0xC2,0x00);
	lcm_dcs_write_seq_static(ctx,0x95,0x81);
	lcm_dcs_write_seq_static(ctx,0x96,0x00);
	lcm_dcs_write_seq_static(ctx,0x98,0x00);
	lcm_dcs_write_seq_static(ctx,0x99,0x20);
	lcm_dcs_write_seq_static(ctx,0xAA,0x62);
	lcm_dcs_write_seq_static(ctx,0xB6,0x05,0x00,0x05,0x00,0x00,0x00,0x05,0x05,0x05,0x05,0x05,0x05);
	lcm_dcs_write_seq_static(ctx,0xBB,0x80);
	lcm_dcs_write_seq_static(ctx,0xBC,0x00,0x00,0x40,0x20,0x00);
	lcm_dcs_write_seq_static(ctx,0xC2,0xCC,0x06,0x02);
	lcm_dcs_write_seq_static(ctx,0xDB,0x01);
	lcm_dcs_write_seq_static(ctx,0xDC,0x63);
	lcm_dcs_write_seq_static(ctx,0xDF,0x01);
	lcm_dcs_write_seq_static(ctx,0xE0,0x63);
	lcm_dcs_write_seq_static(ctx,0xE1,0x01);
	lcm_dcs_write_seq_static(ctx,0xE2,0x63);
	lcm_dcs_write_seq_static(ctx,0xE3,0x01);
	lcm_dcs_write_seq_static(ctx,0xE4,0x63);
	lcm_dcs_write_seq_static(ctx,0xE5,0x01);
	lcm_dcs_write_seq_static(ctx,0xE6,0x63);
	lcm_dcs_write_seq_static(ctx,0xEF,0x01);
	lcm_dcs_write_seq_static(ctx,0xF0,0x63);

	lcm_dcs_write_seq_static(ctx,0xFF,0x25);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0x05,0x00);
	lcm_dcs_write_seq_static(ctx,0x13,0x02);
	lcm_dcs_write_seq_static(ctx,0x14,0x3F);
	lcm_dcs_write_seq_static(ctx,0xDB,0x01);
	lcm_dcs_write_seq_static(ctx,0xDC,0xC5);
	lcm_dcs_write_seq_static(ctx,0x19,0x1F);
	lcm_dcs_write_seq_static(ctx,0x1B,0x55);
	lcm_dcs_write_seq_static(ctx,0x1C,0x50);
	lcm_dcs_write_seq_static(ctx,0x1E,0x00);
	lcm_dcs_write_seq_static(ctx,0x1F,0x59);
	lcm_dcs_write_seq_static(ctx,0x20,0x4C);
	lcm_dcs_write_seq_static(ctx,0x25,0x00);
	lcm_dcs_write_seq_static(ctx,0x26,0x59);
	lcm_dcs_write_seq_static(ctx,0x27,0x4C);
	lcm_dcs_write_seq_static(ctx,0x3F,0x00);
	lcm_dcs_write_seq_static(ctx,0x40,0x00);
	lcm_dcs_write_seq_static(ctx,0x43,0x00);
	lcm_dcs_write_seq_static(ctx,0x44,0x25);
	lcm_dcs_write_seq_static(ctx,0x45,0x59);
	lcm_dcs_write_seq_static(ctx,0x48,0x59);
	lcm_dcs_write_seq_static(ctx,0x49,0x4C);
	lcm_dcs_write_seq_static(ctx,0x5B,0x80);
	lcm_dcs_write_seq_static(ctx,0x5E,0x60);
	lcm_dcs_write_seq_static(ctx,0x61,0x59);
	lcm_dcs_write_seq_static(ctx,0x62,0x4C);
	lcm_dcs_write_seq_static(ctx,0x67,0x00);
	lcm_dcs_write_seq_static(ctx,0x68,0x04);
	lcm_dcs_write_seq_static(ctx,0xC2,0xC0);
	lcm_dcs_write_seq_static(ctx,0xC5,0x17);
	lcm_dcs_write_seq_static(ctx,0xC6,0x10);
	lcm_dcs_write_seq_static(ctx,0xC8,0x04);

	lcm_dcs_write_seq_static(ctx,0xFF,0x26);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0x00,0x81);
	lcm_dcs_write_seq_static(ctx,0x06,0x32);
	lcm_dcs_write_seq_static(ctx,0x04,0x50);
	lcm_dcs_write_seq_static(ctx,0x0A,0xF2);
	lcm_dcs_write_seq_static(ctx,0x0C,0x0E);
	lcm_dcs_write_seq_static(ctx,0x0D,0x08);
	lcm_dcs_write_seq_static(ctx,0x0F,0x00);
	lcm_dcs_write_seq_static(ctx,0x11,0x00);
	lcm_dcs_write_seq_static(ctx,0x12,0x50);
	lcm_dcs_write_seq_static(ctx,0x13,0xA4);
	lcm_dcs_write_seq_static(ctx,0x14,0xAE);
	lcm_dcs_write_seq_static(ctx,0x16,0x10);
	lcm_dcs_write_seq_static(ctx,0x19,0x1A);
	lcm_dcs_write_seq_static(ctx,0x1A,0xB8);
	lcm_dcs_write_seq_static(ctx,0x1B,0x19);
	lcm_dcs_write_seq_static(ctx,0x1C,0x50);
	lcm_dcs_write_seq_static(ctx,0x1D,0x00);
	lcm_dcs_write_seq_static(ctx,0x1E,0x62);
	lcm_dcs_write_seq_static(ctx,0x1F,0x62);
	lcm_dcs_write_seq_static(ctx,0x20,0x00);
	lcm_dcs_write_seq_static(ctx,0x21,0x07);
	lcm_dcs_write_seq_static(ctx,0x24,0x02);
	lcm_dcs_write_seq_static(ctx,0x25,0x62);
	lcm_dcs_write_seq_static(ctx,0x26,0x26);
	lcm_dcs_write_seq_static(ctx,0x2A,0x1A);
	lcm_dcs_write_seq_static(ctx,0x2B,0xB8);
	lcm_dcs_write_seq_static(ctx,0x2F,0x06);
	lcm_dcs_write_seq_static(ctx,0x30,0x62);
	lcm_dcs_write_seq_static(ctx,0x31,0x00);
	lcm_dcs_write_seq_static(ctx,0x32,0x62);
	lcm_dcs_write_seq_static(ctx,0x39,0x08);
	lcm_dcs_write_seq_static(ctx,0x3A,0x62);
	lcm_dcs_write_seq_static(ctx,0x33,0x66);
	lcm_dcs_write_seq_static(ctx,0x34,0x66);
	lcm_dcs_write_seq_static(ctx,0x35,0x66);
	lcm_dcs_write_seq_static(ctx,0x36,0x66);
	lcm_dcs_write_seq_static(ctx,0x37,0x66);
	lcm_dcs_write_seq_static(ctx,0x38,0x06);
	lcm_dcs_write_seq_static(ctx,0x43,0x02);
	lcm_dcs_write_seq_static(ctx,0xC9,0x8E);
	lcm_dcs_write_seq_static(ctx,0xA7,0x00);
	lcm_dcs_write_seq_static(ctx,0xA9,0xA3);
	lcm_dcs_write_seq_static(ctx,0xAA,0xA2);
	lcm_dcs_write_seq_static(ctx,0xAB,0xA1);
	lcm_dcs_write_seq_static(ctx,0xAC,0xA0);
	lcm_dcs_write_seq_static(ctx,0xAD,0x9F);
	lcm_dcs_write_seq_static(ctx,0xAE,0x9E);
	lcm_dcs_write_seq_static(ctx,0xAF,0x9D);
	lcm_dcs_write_seq_static(ctx,0xB0,0x9C);
	lcm_dcs_write_seq_static(ctx,0xB1,0x9B);
	lcm_dcs_write_seq_static(ctx,0xB2,0x9A);
	lcm_dcs_write_seq_static(ctx,0xB3,0x99);
	lcm_dcs_write_seq_static(ctx,0xB4,0x98);
	lcm_dcs_write_seq_static(ctx,0xB5,0x97);
	lcm_dcs_write_seq_static(ctx,0xB6,0x96);
	lcm_dcs_write_seq_static(ctx,0xC7,0x00);
	lcm_dcs_write_seq_static(ctx,0xC8,0x00);
	lcm_dcs_write_seq_static(ctx,0xCD,0x38,0x80);
	lcm_dcs_write_seq_static(ctx,0xCE,0x37,0x80);
	lcm_dcs_write_seq_static(ctx,0xCF,0x04,0x00,0x20);
	lcm_dcs_write_seq_static(ctx,0xD0,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xD1,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xD2,0x3F);

	lcm_dcs_write_seq_static(ctx,0xFF,0x27);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0x13,0x06);
	lcm_dcs_write_seq_static(ctx,0x58,0x80);
	lcm_dcs_write_seq_static(ctx,0x59,0x00);
	lcm_dcs_write_seq_static(ctx,0x5A,0x00);
	lcm_dcs_write_seq_static(ctx,0x5B,0xB8);
	lcm_dcs_write_seq_static(ctx,0x5C,0x00);
	lcm_dcs_write_seq_static(ctx,0x5D,0x00);
	lcm_dcs_write_seq_static(ctx,0x5E,0x00);
	lcm_dcs_write_seq_static(ctx,0x5F,0x00);
	lcm_dcs_write_seq_static(ctx,0x60,0x00);
	lcm_dcs_write_seq_static(ctx,0x61,0x00);
	lcm_dcs_write_seq_static(ctx,0x62,0x00);
	lcm_dcs_write_seq_static(ctx,0x63,0x00);
	lcm_dcs_write_seq_static(ctx,0x64,0x00);
	lcm_dcs_write_seq_static(ctx,0x65,0x00);
	lcm_dcs_write_seq_static(ctx,0x66,0x00);
	lcm_dcs_write_seq_static(ctx,0x67,0x00);
	lcm_dcs_write_seq_static(ctx,0x68,0x00);
	lcm_dcs_write_seq_static(ctx,0x75,0xA8);
	lcm_dcs_write_seq_static(ctx,0x76,0x00);
	lcm_dcs_write_seq_static(ctx,0x77,0xA0);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00);
	lcm_dcs_write_seq_static(ctx,0xC2,0x03);
	lcm_dcs_write_seq_static(ctx,0xD0,0x13);
	lcm_dcs_write_seq_static(ctx,0xD1,0x54);
	lcm_dcs_write_seq_static(ctx,0xDF,0x02);
	lcm_dcs_write_seq_static(ctx,0xD2,0x38);
	lcm_dcs_write_seq_static(ctx,0xDE,0x43);

	lcm_dcs_write_seq_static(ctx,0xFF,0x2A);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0x22,0x0F);
	lcm_dcs_write_seq_static(ctx,0x23,0x10);
	lcm_dcs_write_seq_static(ctx,0x24,0x00);
	lcm_dcs_write_seq_static(ctx,0x25,0x62);
	lcm_dcs_write_seq_static(ctx,0x27,0x05);
	lcm_dcs_write_seq_static(ctx,0x28,0x8E);
	lcm_dcs_write_seq_static(ctx,0x29,0x05);
	lcm_dcs_write_seq_static(ctx,0x2A,0x8E);
	lcm_dcs_write_seq_static(ctx,0x2B,0x05);
	lcm_dcs_write_seq_static(ctx,0x2D,0x8E);
	lcm_dcs_write_seq_static(ctx,0x64,0x93);
	lcm_dcs_write_seq_static(ctx,0x67,0x93);
	lcm_dcs_write_seq_static(ctx,0x68,0x32);
	lcm_dcs_write_seq_static(ctx,0x6A,0x93);
	lcm_dcs_write_seq_static(ctx,0x70,0x93);
	lcm_dcs_write_seq_static(ctx,0x7F,0x93);
	lcm_dcs_write_seq_static(ctx,0x97,0x3C);
	lcm_dcs_write_seq_static(ctx,0x98,0x02);
	lcm_dcs_write_seq_static(ctx,0x99,0x95);
	lcm_dcs_write_seq_static(ctx,0x9A,0x03);
	lcm_dcs_write_seq_static(ctx,0x9B,0x00);
	lcm_dcs_write_seq_static(ctx,0x9C,0x0B);
	lcm_dcs_write_seq_static(ctx,0x9D,0x0A);
	lcm_dcs_write_seq_static(ctx,0x9E,0x90);
	lcm_dcs_write_seq_static(ctx,0xF2,0x3C);
	lcm_dcs_write_seq_static(ctx,0xF3,0x02);
	lcm_dcs_write_seq_static(ctx,0xF4,0x95);
	lcm_dcs_write_seq_static(ctx,0xF5,0x03);
	lcm_dcs_write_seq_static(ctx,0xF6,0x00);
	lcm_dcs_write_seq_static(ctx,0xF7,0x0B);
	lcm_dcs_write_seq_static(ctx,0xF8,0x0A);
	lcm_dcs_write_seq_static(ctx,0xF9,0x90);
	lcm_dcs_write_seq_static(ctx,0xA2,0x3F);
	lcm_dcs_write_seq_static(ctx,0xA4,0xC3);
	lcm_dcs_write_seq_static(ctx,0xB6,0x00,0xFF,0xFF,0x00,0x00,0x77,0x77);
	lcm_dcs_write_seq_static(ctx,0xB9,0x28,0x00,0xC7,0x00,0x80,0xB3,0x40,0x5F,0x41);
	lcm_dcs_write_seq_static(ctx,0xBA,0xAA,0xAA,0xA5,0xA5,0xE1,0xE1);
	lcm_dcs_write_seq_static(ctx,0xBB,0xAA,0xAA,0xAA,0xAA,0xAA,0x55,0xAA,0x55,0x38,0xC7,0x38,0xC7);
	lcm_dcs_write_seq_static(ctx,0xBC,0x66,0x06);
	lcm_dcs_write_seq_static(ctx,0xE8,0x00);

	lcm_dcs_write_seq_static(ctx,0xFF,0x23);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0x07,0x00);
	lcm_dcs_write_seq_static(ctx,0x08,0x01);
	lcm_dcs_write_seq_static(ctx,0x09,0xC2);
	lcm_dcs_write_seq_static(ctx,0x11,0x02);
	lcm_dcs_write_seq_static(ctx,0x12,0x80);
	lcm_dcs_write_seq_static(ctx,0x15,0x83);
	lcm_dcs_write_seq_static(ctx,0x16,0x15);

	lcm_dcs_write_seq_static(ctx,0xFF,0x20);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0xB0,0x00,0x08,0x00,0x2A,0x00,0x5D,0x00,0x84,0x00,0xA4,0x00,0xC0,0x00,0xD7,0x00,0xEC);
	lcm_dcs_write_seq_static(ctx,0xB1,0x00,0xFE,0x01,0x3C,0x01,0x66,0x01,0xAA,0x01,0xD9,0x02,0x24,0x02,0x5D,0x02,0x5E);
	lcm_dcs_write_seq_static(ctx,0xB2,0x02,0x97,0x02,0xD4,0x02,0xFB,0x03,0x2C,0x03,0x4C,0x03,0x78,0x03,0x85,0x03,0x94);
	lcm_dcs_write_seq_static(ctx,0xB3,0x03,0xA4,0x03,0xB2,0x03,0xD1,0x03,0xEF,0x03,0xFD,0x03,0xFF);
	lcm_dcs_write_seq_static(ctx,0xB4,0x00,0x08,0x00,0x2A,0x00,0x5D,0x00,0x84,0x00,0xA4,0x00,0xC0,0x00,0xD7,0x00,0xEC);
	lcm_dcs_write_seq_static(ctx,0xB5,0x00,0xFE,0x01,0x3C,0x01,0x66,0x01,0xAA,0x01,0xD9,0x02,0x24,0x02,0x5D,0x02,0x5E);
	lcm_dcs_write_seq_static(ctx,0xB6,0x02,0x97,0x02,0xD4,0x02,0xFB,0x03,0x2C,0x03,0x4C,0x03,0x78,0x03,0x85,0x03,0x94);
	lcm_dcs_write_seq_static(ctx,0xB7,0x03,0xA4,0x03,0xB2,0x03,0xD1,0x03,0xEF,0x03,0xFD,0x03,0xFF);
	lcm_dcs_write_seq_static(ctx,0xB8,0x00,0x08,0x00,0x2A,0x00,0x5D,0x00,0x84,0x00,0xA4,0x00,0xC0,0x00,0xD7,0x00,0xEC);
	lcm_dcs_write_seq_static(ctx,0xB9,0x00,0xFE,0x01,0x3C,0x01,0x66,0x01,0xAA,0x01,0xD9,0x02,0x24,0x02,0x5D,0x02,0x5E);
	lcm_dcs_write_seq_static(ctx,0xBA,0x02,0x97,0x02,0xD4,0x02,0xFB,0x03,0x2C,0x03,0x4C,0x03,0x78,0x03,0x85,0x03,0x94);
	lcm_dcs_write_seq_static(ctx,0xBB,0x03,0xA4,0x03,0xB2,0x03,0xD1,0x03,0xEF,0x03,0xFD,0x03,0xFF);
	lcm_dcs_write_seq_static(ctx,0xC6,0x72);
	lcm_dcs_write_seq_static(ctx,0xC7,0x30);
	lcm_dcs_write_seq_static(ctx,0xC8,0x31);
	lcm_dcs_write_seq_static(ctx,0xC9,0x20);
	lcm_dcs_write_seq_static(ctx,0xCA,0x10);
	lcm_dcs_write_seq_static(ctx,0xCB,0x72);
	lcm_dcs_write_seq_static(ctx,0xCC,0x30);
	lcm_dcs_write_seq_static(ctx,0xCD,0x31);
	lcm_dcs_write_seq_static(ctx,0xCE,0x20);
	lcm_dcs_write_seq_static(ctx,0xCF,0x10);
	lcm_dcs_write_seq_static(ctx,0xD0,0x72);
	lcm_dcs_write_seq_static(ctx,0xD1,0x30);
	lcm_dcs_write_seq_static(ctx,0xD2,0x31);
	lcm_dcs_write_seq_static(ctx,0xD3,0x20);
	lcm_dcs_write_seq_static(ctx,0xD4,0x10);
	lcm_dcs_write_seq_static(ctx,0xD5,0x72);
	lcm_dcs_write_seq_static(ctx,0xD6,0x30);
	lcm_dcs_write_seq_static(ctx,0xD7,0x31);
	lcm_dcs_write_seq_static(ctx,0xD8,0x20);
	lcm_dcs_write_seq_static(ctx,0xD9,0x10);
	lcm_dcs_write_seq_static(ctx,0xDA,0x72);
	lcm_dcs_write_seq_static(ctx,0xDB,0x30);
	lcm_dcs_write_seq_static(ctx,0xDC,0x31);
	lcm_dcs_write_seq_static(ctx,0xDD,0x20);
	lcm_dcs_write_seq_static(ctx,0xDE,0x10);
	lcm_dcs_write_seq_static(ctx,0xDF,0x72);
	lcm_dcs_write_seq_static(ctx,0xE0,0x30);
	lcm_dcs_write_seq_static(ctx,0xE1,0x31);
	lcm_dcs_write_seq_static(ctx,0xE2,0x20);
	lcm_dcs_write_seq_static(ctx,0xE3,0x10);

	lcm_dcs_write_seq_static(ctx,0xFF,0x21);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0xB0,0x00,0x00,0x00,0x22,0x00,0x55,0x00,0x7C,0x00,0x9C,0x00,0xB8,0x00,0xCF,0x00,0xE4);
	lcm_dcs_write_seq_static(ctx,0xB1,0x00,0xF6,0x01,0x34,0x01,0x5E,0x01,0xA2,0x01,0xD1,0x02,0x1C,0x02,0x55,0x02,0x56);
	lcm_dcs_write_seq_static(ctx,0xB2,0x02,0x8F,0x02,0xCC,0x02,0xF3,0x03,0x24,0x03,0x44,0x03,0x70,0x03,0x7D,0x03,0x8C);
	lcm_dcs_write_seq_static(ctx,0xB3,0x03,0x9C,0x03,0xAA,0x03,0xC9,0x03,0xE7,0x03,0xF5,0x03,0xF7);
	lcm_dcs_write_seq_static(ctx,0xB4,0x00,0x00,0x00,0x22,0x00,0x55,0x00,0x7C,0x00,0x9C,0x00,0xB8,0x00,0xCF,0x00,0xE4);
	lcm_dcs_write_seq_static(ctx,0xB5,0x00,0xF6,0x01,0x34,0x01,0x5E,0x01,0xA2,0x01,0xD1,0x02,0x1C,0x02,0x55,0x02,0x56);
	lcm_dcs_write_seq_static(ctx,0xB6,0x02,0x8F,0x02,0xCC,0x02,0xF3,0x03,0x24,0x03,0x44,0x03,0x70,0x03,0x7D,0x03,0x8C);
	lcm_dcs_write_seq_static(ctx,0xB7,0x03,0x9C,0x03,0xAA,0x03,0xC9,0x03,0xE7,0x03,0xF5,0x03,0xF7);
	lcm_dcs_write_seq_static(ctx,0xB8,0x00,0x00,0x00,0x22,0x00,0x55,0x00,0x7C,0x00,0x9C,0x00,0xB8,0x00,0xCF,0x00,0xE4);
	lcm_dcs_write_seq_static(ctx,0xB9,0x00,0xF6,0x01,0x34,0x01,0x5E,0x01,0xA2,0x01,0xD1,0x02,0x1C,0x02,0x55,0x02,0x56);
	lcm_dcs_write_seq_static(ctx,0xBA,0x02,0x8F,0x02,0xCC,0x02,0xF3,0x03,0x24,0x03,0x44,0x03,0x70,0x03,0x7D,0x03,0x8C);
	lcm_dcs_write_seq_static(ctx,0xBB,0x03,0x9C,0x03,0xAA,0x03,0xC9,0x03,0xE7,0x03,0xF5,0x03,0xF7);

	lcm_dcs_write_seq_static(ctx,0xFF,0x26);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0xD2,0x2F);
	lcm_dcs_write_seq_static(ctx,0xC9,0x00);

	lcm_dcs_write_seq_static(ctx,0xFF,0x10);
	lcm_dcs_write_seq_static(ctx,0xFB,0x01);
	lcm_dcs_write_seq_static(ctx,0x35,0x00);
	lcm_dcs_write_seq_static(ctx,0x51,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x53,0x2C);
	lcm_dcs_write_seq_static(ctx,0x55,0x00);
	lcm_dcs_write_seq_static(ctx,0x90,0x03);
	lcm_dcs_write_seq_static(ctx,0x91,0x89,0xA8,0x00,0x14,0xD2,0x00,0x02,0xB2,0x02,0x2B,0x00,0x0B,0x05,0x7A,0x03,0x68);
	lcm_dcs_write_seq_static(ctx,0x92,0x10,0xE0);
	lcm_dcs_write_seq_static(ctx,0xB2,0x91);
	lcm_dcs_write_seq_static(ctx,0xB3,0x00);
	lcm_dcs_write_seq_static(ctx,0xBB,0x13);
	lcm_dcs_write_seq_static(ctx,0x3B,0x03,0xC2,0x1E,0x04,0x04,0x00);
	lcm_dcs_write_seq_static(ctx,0x11,0x00);
	mdelay(100);
	lcm_dcs_write_seq_static(ctx,0x29,0x00);
	mdelay(40);
	ctx->error=0;
//SR:add for esd recovery by shude.wan 20231128 start
	if(bl_51_dbv){
		lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
	}
//SR:add for esd recovery by shude.wan 20231128 end
	pr_info("%s test version End\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;
/* For OLED backlight set
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}
*/
	ctx->enabled = false;
	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s Start\n", __func__);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx,0x28);
	mdelay(20);
	lcm_dcs_write_seq_static(ctx,0x10);
	mdelay(60);
//when enable TP_gesture, panel no need power off
#if defined (TRAN_CTP_GESTURE_FUN)
	tran_new_gesture_fun_enable = tran_call_tp_cmd_handle(1003,NULL);
	pr_info("ctp gesture enable = %d\n", tran_new_gesture_fun_enable);
	/*We need to make sure the vsp/vsn/reset pull low while doing esd recovery */
	if (!tran_new_gesture_fun_enable) {//is_esd_recovery || 
#endif
		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
		udelay(5*1000);
		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
		mdelay(5);
		tran_is_lcm_poweroff = 1; //lcm_power(AVDD/AVEE) state is poweroff
#if defined (TRAN_CTP_GESTURE_FUN)
	}
#endif
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	ctx->error = 0;
	ctx->prepared = false;
	pr_info("%s End\n", __func__);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s Start tran_is_lcm_poweroff:%d\n", __func__, tran_is_lcm_poweroff);
	if (ctx->prepared)
		return 0;

	if (tran_is_lcm_poweroff) {
		ctx->bias_pos =
		    devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
		mdelay(5);
		ctx->bias_neg =
		    devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
		mdelay(10);
		tran_panel_i2c_write_bytes(ocp2131_cmd_data[0].cmd, ocp2131_cmd_data[0].data);
		udelay(1*1000);
		tran_panel_i2c_write_bytes(ocp2131_cmd_data[1].cmd, ocp2131_cmd_data[1].data);
		udelay(1*1000);
		tran_panel_i2c_write_bytes(ocp2131_cmd_data[2].cmd, ocp2131_cmd_data[2].data);
		mdelay(8);
		ctx->reset_gpio =
			devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		mdelay(11);
		gpiod_set_value(ctx->reset_gpio, 1);
		mdelay(5);
		gpiod_set_value(ctx->reset_gpio, 0);
		mdelay(1);
		gpiod_set_value(ctx->reset_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		mdelay(11);
		tran_is_lcm_poweroff = 0; //lcm_power(AVDD/AVEE) state is poweron
	}else{
		ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		mdelay(11);
		gpiod_set_value(ctx->reset_gpio, 1);
		mdelay(5);
		gpiod_set_value(ctx->reset_gpio, 0);
		mdelay(1);
		gpiod_set_value(ctx->reset_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		mdelay(11);
	}
	lcm_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	pr_info("%s test End\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s start\n", __func__);
	if (ctx->enabled)
		return 0;
/* For OLED backlight set
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}
*/
	ctx->enabled = true;
	return 0;
}

static const struct drm_display_mode performance_mode_60hz = { //fps:60Hz porch params
	.clock       = ((FRAME_WIDTH  + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_0_VFP + VSA + VBP) * MODE_0_FPS / 1000),
	.hdisplay    = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH  + HFP,
	.hsync_end   = FRAME_WIDTH  + HFP + HSA,
	.htotal      = FRAME_WIDTH  + HFP + HSA + HBP,
	.vdisplay    = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end   = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal      = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
};

static const struct drm_display_mode performance_mode_90hz = { //fps:90Hz porch params
	.clock       = ((FRAME_WIDTH  + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_1_VFP + VSA + VBP) * MODE_1_FPS / 1000),
	.hdisplay    = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH  + HFP,
	.hsync_end   = FRAME_WIDTH  + HFP + HSA,
	.htotal      = FRAME_WIDTH  + HFP + HSA + HBP,
	.vdisplay    = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end   = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal      = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
};


#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_mode_60Hz = {
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	//.vdo_per_frame_lp_enable = 1,
	.lcm_esd_check_table[0] = {
			.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable=0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.data_rate = DATA_RATE,
	.data_rate_khz = DATA_RATE_KHZ,
	.rotate = MTK_PANEL_ROTATE_180,
	.phy_timcon = {
		.lpx = 0x6,
	},
	.dyn_fps = {
		.switch_en = 0,
	},
	.dyn = {
		.switch_en = 0,
		.hfp = 39,
		.data_rate = 638,
	},
	.tran_panel_params = &panel_driver_status,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_param_load_mode = 2,
	.dsc_params = {
		.enable                = 1,
		.ver                   = 18,
		.slice_mode            = 1,
		.rgb_swap              = 0,
		.dsc_cfg               = 34,
		.rct_on                = 1,
		.bit_per_channel       = 8,
		.dsc_line_buf_depth    = 9,
		.bp_enable             = 1,
		.bit_per_pixel         = 128,
		.pic_height            = 2560,
		.pic_width             = 1600,
		.slice_height          = 20,
		.slice_width           = 800,
		.chunk_size            = 800,
		.xmit_delay            = 512,
		.dec_delay             = 690,
		.scale_value           = 32,
		.increment_interval    = 555,
		.decrement_interval    = 11,
		.line_bpg_offset       = 13,
		.nfl_bpg_offset        = 1402,
		.slice_bpg_offset      = 872,
		.initial_offset        = 6144,
		.final_offset          = 4320,
		.flatness_minqp        = 3,
		.flatness_maxqp        = 12,
		.rc_model_size         = 8192,
		.rc_edge_factor        = 6,
		.rc_quant_incr_limit0  = 11,
		.rc_quant_incr_limit1  = 11,
		.rc_tgt_offset_hi      = 3,
		.rc_tgt_offset_lo      = 3,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = dsc_rc_buf_thresh,
			.range_min_qp  = dsc_range_min_qp,
			.range_max_qp  = dsc_range_max_qp,
			.range_bpg_ofs = dsc_range_bpg_offset,
			},
		},

};

static struct mtk_panel_params ext_params_mode_90Hz = {
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	//.vdo_per_frame_lp_enable = 1,
	.lcm_esd_check_table[0] = {
			.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable=0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.data_rate = DATA_RATE,
	.data_rate_khz = DATA_RATE_KHZ,
	.rotate = MTK_PANEL_ROTATE_180,
	.phy_timcon = {
		.lpx = 0x6,
	},
	.dyn_fps = {
		.switch_en = 0,
	},
	.dyn = {
		.switch_en = 0,
		.hfp = 39,
		.data_rate = 638,
	},
	.tran_panel_params = &panel_driver_status,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                = 1,
		.ver                   = 18,
		.slice_mode            = 1,
		.rgb_swap              = 0,
		.dsc_cfg               = 34,
		.rct_on                = 1,
		.bit_per_channel       = 8,
		.dsc_line_buf_depth    = 9,
		.bp_enable             = 1,
		.bit_per_pixel         = 128,
		.pic_height            = 2560,
		.pic_width             = 1600,
		.slice_height          = 20,
		.slice_width           = 800,
		.chunk_size            = 800,
		.xmit_delay            = 512,
		.dec_delay             = 690,
		.scale_value           = 32,
		.increment_interval    = 555,
		.decrement_interval    = 11,
		.line_bpg_offset       = 13,
		.nfl_bpg_offset        = 1402,
		.slice_bpg_offset      = 872,
		.initial_offset        = 6144,
		.final_offset          = 4320,
		.flatness_minqp        = 3,
		.flatness_maxqp        = 12,
		.rc_model_size         = 8192,
		.rc_edge_factor        = 6,
		.rc_quant_incr_limit0  = 11,
		.rc_quant_incr_limit1  = 11,
		.rc_tgt_offset_hi      = 3,
		.rc_tgt_offset_lo      = 3,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = dsc_rc_buf_thresh,
			.range_min_qp  = dsc_range_min_qp,
			.range_max_qp  = dsc_range_max_qp,
			.range_bpg_ofs = dsc_range_bpg_offset,
			},
		},
		
};


//SR:add for backlight S666LNMA6-246 by yunkang.dai 20231113 start
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{

/*add for the minimum brighness is too low by shixian.yu 20250709 start*/
	if (level > 0 && level <= 10)
		level = 10;
/*add for the minimum brighness is too low by shixian.yu 20250709 end*/
//BSP: modify X1102M1201-865 low power set maxbacklight by qihui.zhang 20250710 start
	if((g_lcm_uisoc <= 10) && (level > 3552)){		
		level = 3552;	
	}
//BSP: modify X1102M1201-865 low power set maxbacklight by qihui.zhang 20250710 end	
	bl_tb0[1] = ((level >> 8) & 0x0f);
	bl_tb0[2] = (level & 0xff);

	if (!cb)
		return -1;
	if(level == 0){
		cb(dsi, handle, bl_dimming_disable, ARRAY_SIZE(bl_dimming_disable));
		tran_dim_state = 3;
	}
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	pr_info("%s [LCM] brightness level = %d, bl_tb0[1]:%x, bl_tb0[2]:%x\n", __func__, level, bl_tb0[1],bl_tb0[2]);
	bl_51_dbv = level;
	if(tran_dim_state > 1){
		tran_dim_state --;
	}else if(tran_dim_state == 1){
		cb(dsi, handle, bl_dimming_enable, ARRAY_SIZE(bl_dimming_enable));
		tran_dim_state = 0;
	}

	return 0;
}
//SR:add for backlight S666LNMA6-246 by yunkang.dai 20231113 end
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
static int panel_set_aod_light_mode(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_aod_level[] = {0x51,0x00,0x00};
	unsigned int aod_mapped_level = 0;
	if (level  != 0){
		if(level  > 0 && level  < 80){
			aod_mapped_level = 32;		//doze_backlight_level1 5nit
		}else if(level  >= 80 && level  < 300){
			aod_mapped_level = 240;		//doze_backlight_level1 30nit
		}else{
			aod_mapped_level = 480;		//doze_backlight_level1 60nit
		}
		pr_info("[%s]lcm AOD backlight level=%d, aod_mapped_level=%d\n", __func__, level, aod_mapped_level);
		bl_aod_level[1] = ((aod_mapped_level & 0xF00) >> 8) & 0xF;
		bl_aod_level[2] = aod_mapped_level & 0xFF;
		cb(dsi, handle, bl_aod_level, ARRAY_SIZE(bl_aod_level));
	}
	return 0;
}
#endif

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{//BSP:X1102M1201-981 add touch type pen or finger check by qihui.zhang 20250711 start
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0,touch_type_pen=0,vrefresh;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);
	
	touch_type_pen = tran_call_tp_cmd_handle(1006,NULL);
	vrefresh=drm_mode_vrefresh(m);
	pr_info("%s  touch_type: %d,  vrefresh %d\n", __func__,touch_type_pen, vrefresh);

	if (touch_type_pen && vrefresh != MODE_0_FPS)
		return 1;
	
	if (vrefresh == MODE_0_FPS) //60Hz
		ext->params = &ext_params_mode_60Hz;
	else if (vrefresh == MODE_1_FPS) //90Hz
		ext->params = &ext_params_mode_90Hz;
	else
		ret = 1;
	pr_info("%s  end\n",__func__);
//BSP:X1102M1201-981 add touch type pen or finger check by qihui.zhang 20250711 end
	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_err("[error]%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	/*struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x1c, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x0a, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	printk("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0])
		return 1;

	printk("ATA expect read data is %x %x %x\n",id[0], id[1], id[2]);

	return 1;*/
	struct mtk_panel_ext *ext = find_panel_ext(panel);

	pr_info("ATA read data %x\n", ext->is_connected);

	return ext->is_connected;
}

static struct mtk_panel_funcs ext_funcs = {
//SR:add for backlight S666LNMA6-246 by yunkang.dai 20231113 start
	.set_backlight_cmdq = lcm_setbacklight_cmdq,//For OLED backlight set
//SR:add for backlight S666LNMA6-246 by yunkang.dai 20231113 end
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	.reset = panel_ext_reset,
//BSP:add for aod by shude.wan 20260109 start
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
	.set_aod_light_mode = panel_set_aod_light_mode,
#endif
//BSP:add for aod by shude.wan 20260109 end

/* add for ramless AOD */
/*	.doze_get_mode_flags = panel_doze_get_mode_flags,
	.doze_enable = panel_doze_enable,
	.doze_enable_start = panel_doze_enable_start,
	.doze_area = panel_doze_area,
	.doze_disable = panel_doze_disable,
	.doze_post_disp_on = panel_doze_post_disp_on,
	.set_aod_light_mode = panel_set_aod_light_mode,
*/
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *           become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *          display the first valid frame after starting to receive
	 *          video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *           turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *             to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode_60Hz;
	struct drm_display_mode *mode_90Hz;


	mode_60Hz = drm_mode_duplicate(connector->dev, &performance_mode_60hz);
	if (!mode_60Hz) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_60hz.hdisplay,
			performance_mode_60hz.vdisplay,
			drm_mode_vrefresh(&performance_mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_60Hz);
	mode_60Hz->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_60Hz);


	mode_90Hz = drm_mode_duplicate(connector->dev, &performance_mode_90hz);
	if (!mode_90Hz) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_90hz.hdisplay,
			performance_mode_90hz.vdisplay,
			drm_mode_vrefresh(&performance_mode_90hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_90Hz);
	mode_90Hz->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_90Hz);


	connector->display_info.width_mm = 147;
	connector->display_info.height_mm = 235;

	return 2;
}
//BSP: modify X1102M1201-865 low power set maxbacklight by qihui.zhang 20250710 start
static int lcm_get_uisoc(void)
{	

	struct power_supply *psy;	
	union power_supply_propval prop;	
	int ret;	

	psy = power_supply_get_by_name("battery");	
	if (psy == NULL) {		
		pr_info("%s psy is not rdy\n", __func__);		
		return -ENODEV;	
	}	

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &prop);	
	if (ret || prop.intval < 0) {		
		return -EINVAL;	
	}	

	pr_info("%s uisoc=%d\n", __func__, prop.intval);	
	return prop.intval;
}

static int mtk_power_lcm_psy_event(
	struct notifier_block *nb, unsigned long event, void *v)
{
	struct power_supply *psy = v;

	if (strcmp(psy->desc->name, "battery") == 0) {
		
		g_lcm_uisoc = lcm_get_uisoc();
		pr_info("%s uisoc=%d\n", __func__,g_lcm_uisoc);	
		
	}
	return NOTIFY_DONE;
}
//BSP: modify X1102M1201-865 low power set maxbacklight by qihui.zhang 20250710 end
static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int nt36523n_lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
//	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	//USD:add for aod by shude.wan 20260109 start
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
	unsigned int doze_backlight[] = {0, 0, 0, 0};
#endif
	//USD:add for aod by shude.wan 20260109 end

	pr_info("nt36523n %s+\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}
	//USD:add for aod by shude.wan 20260109 start
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
	of_property_read_u32_array(remote_node, "doze_backlight", doze_backlight, ARRAY_SIZE(doze_backlight));
	g_lcm_doze_backlight.doze_backlight_num = doze_backlight[0];
	g_lcm_doze_backlight.doze_backlight_level1 = doze_backlight[1];
	g_lcm_doze_backlight.doze_backlight_level2 = doze_backlight[2];
	g_lcm_doze_backlight.doze_backlight_level3 = doze_backlight[3];
	pr_info("[LCM] %s lcm_doze_backlight:%d;%d;%d;%d;\n", __func__, g_lcm_doze_backlight.doze_backlight_num,
	   g_lcm_doze_backlight.doze_backlight_level1, g_lcm_doze_backlight.doze_backlight_level2, g_lcm_doze_backlight.doze_backlight_level3);
	panel_driver_status.lcm_doze_backlight = &g_lcm_doze_backlight;
#endif
	//USD:add for aod by shude.wan 20260109 end

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO|MIPI_DSI_MODE_VIDEO_BURST|MIPI_DSI_CLOCK_NON_CONTINUOUS;

/* For OLED backlight set
	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}
*/
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_err("[error]cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		pr_err("[error]%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		pr_err("[error]%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &ext_params_mode_60Hz, &ext_funcs, &ctx->panel);
	if (ret < 0) {
		pr_err("%s create new panel params fail\n", __func__);
		return ret;
	}
#endif
//BSP: modify X1102M1201-865 low power set maxbacklight by qihui.zhang 20250710 start
	lcm_psy_nb.notifier_call = mtk_power_lcm_psy_event;
	power_supply_reg_notifier(&lcm_psy_nb);
//BSP: modify X1102M1201-865 low power set maxbacklight by qihui.zhang 20250710 end
	pr_info("%s-\n", __func__);

	return ret;
}

static void nt36523n_lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined (CONFIG_MTK_PANEL_EXT)
		struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif
	printk("%s() begin \n",__func__);
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined (CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
	return;
}

static void nt36523n_lcm_shutdown(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
	pr_info("%s enter\n",__func__);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->reset_gpio)) {
			pr_err("[error]%s: cannot get reset_gpio %ld\n", __func__, PTR_ERR(ctx->reset_gpio));
			//return PTR_ERR(ctx->reset_gpio);
		}
	gpiod_set_value(ctx->reset_gpio, 0);
    devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	mdelay(5);
	if ( !tran_is_lcm_poweroff ) {  //when shutdown/restart, if lcm has been poweroff, no need poweroff again.
		//3.lcm avee_2 gpio pulldown
		ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			pr_err("[error]%s: cannot get bias_neg %ld\n", __func__, PTR_ERR(ctx->bias_neg));
			//return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		udelay(5*1000);

		//4.lcm avdd gpio pulldown
		ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			pr_err("[error]%s: cannot get bias_pos %ld\n", __func__, PTR_ERR(ctx->bias_pos));
			//return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
		tran_is_lcm_poweroff = 1; //lcm_power(AVDD/AVEE) state is poweroff
	}
	printk("%s() end\n",__func__);
}
static const struct of_device_id lcm_of_match[] = {
	{
		.compatible = "nt36523n,fhdp,dsi,vdo,dsc,hjr,csot,90hz,t1103",
	},
	{} };

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = nt36523n_lcm_probe,
	.remove = nt36523n_lcm_remove,
	.driver = {

			.name = "nt36523n_fhdp_dsi_vdo_dsc_hjr_csot_90hz_t1103",
			.owner = THIS_MODULE,
			.of_match_table = lcm_of_match,
		},
	.shutdown = nt36523n_lcm_shutdown,
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("shude.wan <shude.wan@transsion.com>");
MODULE_DESCRIPTION("nt36523n vdo t1103 90Hz LCD Panel Driver");
MODULE_LICENSE("GPL v2");
