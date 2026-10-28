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

#include <drm/drm_panel.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_connector.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <drm/drm_device.h>
#include <drm/drm_modes.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#endif

extern int mtk_panel_i2c_write_bytes(unsigned char addr, unsigned char value);
#define FRAME_WIDTH		    1600
#define FRAME_HEIGHT		2560
#define PHYSICAL_WIDTH		69552
#define PHYSICAL_HEIGHT		154560

#define HSA		10
#define HBP		50
#define HFP		50
#define VSA		20
#define VBP		174
#define DATA_RATE	1092

#define REGFLAG_DELAY           0xFFFC
#define REGFLAG_UDELAY          0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW       0xFFFE
#define REGFLAG_RESET_HIGH      0xFFFF
/*Parameter setting for mode 120 Start*/
//#define MODE_120_FPS	120
//#define MODE_120_VFP	260
/*Parameter setting for mode 120 End*/

/*Parameter setting for mode 90 Start*/
#define MODE_90_FPS	90
#define MODE_90_VFP	30
/*Parameter setting for mode 90 End*/

/*Parameter setting for mode 60 Start*/
//#define MODE_60_FPS	60
//#define MODE_60_VFP	1422
/*Parameter setting for mode 60 End*/
#if 0
struct LCM_BIAS_SETTING_TABLE {
	unsigned char cmd;
	unsigned char data;
};
static struct LCM_BIAS_SETTING_TABLE ocp2131_resume_data[2] = {
	{0x00, 0x14},
	{0x01, 0x14},
};
#endif
static unsigned int nt36523n_vdo_90hz_dphy_buf_thresh[14] = {896, 1792, 2688, 3584, 4480,
	5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064};

static unsigned int nt36523n_vdo_90hz_dphy_range_min_qp[15] = {0, 0, 1, 1, 3, 3, 3, 3, 3,
	3, 5, 5, 5, 9, 12};
static unsigned int nt36523n_vdo_90hz_dphy_range_max_qp[15] = {4, 4, 5, 6, 7, 7, 7, 8, 9, 
	10, 10, 11, 11, 12, 13};
static int nt36523n_vdo_90hz_dphy_range_bpg_ofs[15] = {2, 0, 0, -2, -4, -6, -8, -8, -8, -10,
	-10, -12, -12, -12, -12};
	
static char bl_tb0[] = {0x51, 0x0F, 0xFF};
static char bl_dim[] = {0x53, 0x2C};
static unsigned int g_dim_enable = 0;
static unsigned int g_need_dim_enable = 0;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *avdd_en_gpio;
	struct gpio_desc *avee_en_gpio;
	bool prepared;
	bool enabled;
	int error;
};

struct tran_panel_driver_params panel_driver_status = {0};
tran_lcm_doze_backlight g_lcm_doze_backlight = {
	.doze_backlight_num = 0,
	.doze_backlight_level1 = 0,
	.doze_backlight_level2 = 0,
	.doze_backlight_level3 = 0,
};
static unsigned int g_aod_enable = 0;

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
		pr_info("error %d reading dcs seq:(%#x)\n", ret, cmd);
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
		pr_info("return %d data(0x%08x) to dsi engine\n",
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
		pr_info("error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}
struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[256];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
	{0xFF, 1, {0xF0} },
	{0xFB, 1, {0x01} },
	{0x24, 1, {0x02} },
	{0xFF, 1, {0xE0} },
	{0xFB, 1, {0x01} },
	{0x14, 1, {0x60} },
	{0x16, 1, {0xC0} },
	{0xFF, 1, {0x20} },
	{0x18, 1, {0x04} },
	{0x19, 1, {0x02} },
	{0xFF, 1, {0x20} },
	{0xFB, 1, {0x01} },
	{0x05, 1, {0xB1} },
	{0x06, 1, {0xC0} },
	{0x07, 1, {0x7D} },
	{0x08, 1, {0x5F} },
	{0x09, 1, {0x99} },
	{0x0A, 1, {0x88} },
	{0x0B, 1, {0x10} },
	{0x0C, 1, {0x00} },
	{0x0D, 1, {0x63} },
	{0x0E, 1, {0x96} },
	{0x0F, 1, {0x78} },
	{0x30, 1, {0x50} },
	{0x31, 1, {0x02} },
	{0x32, 1, {0x42} },
	{0x58, 1, {0x43} },
	{0x69, 1, {0xDD} },
	{0x65, 1, {0x55} },
	{0x6D, 1, {0xAA} },
	{0x6E, 1, {0x00} },
	{0x75, 1, {0xC4} },
	{0x7C, 1, {0x22} },
	{0x7D, 1, {0x99} },
	{0x88, 1, {0x00} },
	{0x94, 1, {0xC0} },
	{0x95, 1, {0x09} },
	{0x96, 1, {0x09} },
	{0xE5, 12, {0x55,0x55,0x55,0x55,0x99,0x99,0x99,0x99,0xAA,0xAA,0xAA,0xAA} },
	{0xFF, 1, {0x24} },
	{0xFB, 1, {0x01} },
	{0x00, 1, {0x03} },
	{0x01, 1, {0x08} },
	{0x02, 1, {0x1C} },
	{0x03, 1, {0x1D} },
	{0x04, 1, {0x03} },
	{0x05, 1, {0x03} },
	{0x06, 1, {0x03} },
	{0x07, 1, {0x03} },
	{0x08, 1, {0x03} },
	{0x09, 1, {0x26} },
	{0x0A, 1, {0x0F} },
	{0x0B, 1, {0x0E} },
	{0x0C, 1, {0x0D} },
	{0x0D, 1, {0x0C} },
	{0x0E, 1, {0x04} },
	{0x0F, 1, {0x26} },
	{0x10, 1, {0x03} },
	{0x11, 1, {0x03} },
	{0x12, 1, {0x03} },
	{0x13, 1, {0x03} },
	{0x14, 1, {0x03} },
	{0x15, 1, {0x03} },
	{0x16, 1, {0x03} },
	{0x17, 1, {0x08} },
	{0x18, 1, {0x1C} },
	{0x19, 1, {0x1D} },
	{0x1A, 1, {0x03} },
	{0x1B, 1, {0x03} },
	{0x1C, 1, {0x03} },
	{0x1D, 1, {0x03} },
	{0x1E, 1, {0x03} },
	{0x1F, 1, {0x26} },
	{0x20, 1, {0x0F} },
	{0x21, 1, {0x0E} },
	{0x22, 1, {0x0D} },
	{0x23, 1, {0x0C} },
	{0x24, 1, {0x04} },
	{0x25, 1, {0x26} },
	{0x26, 1, {0x03} },
	{0x27, 1, {0x03} },
	{0x28, 1, {0x03} },
	{0x29, 1, {0x03} },
	{0x2A, 1, {0x03} },
	{0x2B, 1, {0x03} },
	{0x2D, 1, {0x00} },
	{0x2F, 1, {0x05} },
	{0x30, 1, {0x00} },
	{0x33, 1, {0x05} },
	{0x34, 1, {0x00} },
	{0x37, 1, {0x66} },
	{0x39, 1, {0x00} },
	{0x3A, 1, {0x25} },
	{0x3B, 1, {0x59} },
	{0x3D, 1, {0x91} },
	{0xAB, 1, {0x55} },
	{0x3F, 1, {0x09} },
	{0x43, 1, {0x09} },
	{0x47, 1, {0x40} },
	{0x49, 1, {0x00} },
	{0x4A, 1, {0x62} },
	{0x4B, 1, {0x42} },
	{0x4C, 1, {0x01} },
	{0x4D, 1, {0x21} },
	{0x4E, 1, {0x43} },
	{0x4F, 1, {0x00} },
	{0x50, 1, {0x00} },
	{0x51, 1, {0x00} },
	{0x52, 1, {0x00} },
	{0x53, 1, {0x00} },
	{0x54, 1, {0x00} },
	{0x55, 2, {0x83,0x03} },
	{0x56, 1, {0x04} },
	{0x58, 1, {0x21} },
	{0x59, 1, {0x40} },
	{0x5A, 1, {0x59} },
	{0x5B, 1, {0x4C} },
	{0x5C, 1, {0x8F} },
	{0x5D, 1, {0x00} },
	{0x5E, 2, {0x00,0x08} },
	{0x60, 1, {0xC8} },
	{0x61, 1, {0x00} },
	{0x63, 1, {0xA0} },
	{0x7A, 1, {0x00} },
	{0x7B, 1, {0x44} },
	{0x7E, 1, {0x20} },
	{0x7F, 1, {0x64} },
	{0x80, 1, {0x00} },
	{0x81, 1, {0x00} },
	{0x82, 1, {0x08} },
	{0x83, 1, {0x1B} },
	{0x97, 1, {0xC3} },
	{0x9F, 1, {0x00} },
	{0xA0, 1, {0x00} },
	{0x91, 1, {0x40} },
	{0x92, 1, {0x62} },
	{0x93, 2, {0x1E,0x00} },
	{0x94, 2, {0xC2,0x00} },
	{0x95, 1, {0x81} },
	{0x96, 1, {0x00} },
	{0x98, 1, {0x00} },
	{0x99, 1, {0x20} },
	{0xAA, 1, {0x62} },
	{0xB6, 12, {0x05,0x00,0x05,0x00,0x00,0x00,0x05,0x05,0x05,0x05,0x05,0x05} },
	{0xBB, 1, {0x80} },
	{0xBC, 5, {0x00,0x00,0x40,0x20,0x00} },
	{0xC2, 3, {0xCC,0x06,0x02} },
	{0xDB, 1, {0x01} },
	{0xDC, 1, {0x63} },
	{0xDF, 1, {0x01} },
	{0xE0, 1, {0x63} },
	{0xE1, 1, {0x01} },
	{0xE2, 1, {0x63} },
	{0xE3, 1, {0x01} },
	{0xE4, 1, {0x63} },
	{0xE5, 1, {0x01} },
	{0xE6, 1, {0x63} },
	{0xEF, 1, {0x01} },
	{0xF0, 1, {0x63} },
	{0xFF, 1, {0x25} },
	{0xFB, 1, {0x01} },
	{0x05, 1, {0x00} },
	{0x13, 1, {0x02} },
	{0x14, 1, {0x3F} },
	{0xDB, 1, {0x01} },
	{0xDC, 1, {0xC5} },
	{0x19, 1, {0x1F} },
	{0x1B, 1, {0x55} },
	{0x1C, 1, {0x50} },
	{0x1E, 1, {0x00} },
	{0x1F, 1, {0x59} },
	{0x20, 1, {0x4C} },
	{0x25, 1, {0x00} },
	{0x26, 1, {0x59} },
	{0x27, 1, {0x4C} },
	{0x3F, 1, {0x00} },
	{0x40, 1, {0x00} },
	{0x43, 1, {0x00} },
	{0x44, 1, {0x25} },
	{0x45, 1, {0x59} },
	{0x48, 1, {0x59} },
	{0x49, 1, {0x4C} },
	{0x5B, 1, {0x80} },
	{0x5E, 1, {0x60} },
	{0x61, 1, {0x59} },
	{0x62, 1, {0x4C} },
	{0x67, 1, {0x00} },
	{0x68, 1, {0x04} },
	{0xC2, 1, {0xC0} },
	{0xC5, 1, {0x17} },
	{0xC6, 1, {0x10} },
	{0xC8, 1, {0x04} },
	{0xFF, 1, {0x26} },
	{0xFB, 1, {0x01} },
	{0x00, 1, {0x81} },
	{0x06, 1, {0x32} },
	{0x04, 1, {0x50} },
	{0x0A, 1, {0xF2} },
	{0x0C, 1, {0x0E} },
	{0x0D, 1, {0x08} },
	{0x0F, 1, {0x00} },
	{0x11, 1, {0x00} },
	{0x12, 1, {0x50} },
	{0x13, 1, {0xA4} },
	{0x14, 1, {0xAE} },
	{0x16, 1, {0x10} },
	{0x19, 1, {0x1A} },
	{0x1A, 1, {0xB8} },
	{0x1B, 1, {0x19} },
	{0x1C, 1, {0x50} },
	{0x1D, 1, {0x00} },
	{0x1E, 1, {0x62} },
	{0x1F, 1, {0x62} },
	{0x20, 1, {0x00} },
	{0x21, 1, {0x07} },
	{0x24, 1, {0x02} },
	{0x25, 1, {0x62} },
	{0x26, 1, {0x26} },
	{0x2A, 1, {0x1A} },
	{0x2B, 1, {0xB8} },
	{0x2F, 1, {0x06} },
	{0x30, 1, {0x62} },
	{0x31, 1, {0x00} },
	{0x32, 1, {0x62} },
	{0x39, 1, {0x08} },
	{0x3A, 1, {0x62} },
	{0x33, 1, {0x66} },
	{0x34, 1, {0x66} },
	{0x35, 1, {0x66} },
	{0x36, 1, {0x66} },
	{0x37, 1, {0x66} },
	{0x38, 1, {0x06} },
	{0x43, 1, {0x02} },
	{0xC9, 1, {0x8E} },
	{0xA7, 1, {0x00} },
	{0xA9, 1, {0xA3} },
	{0xAA, 1, {0xA2} },
	{0xAB, 1, {0xA1} },
	{0xAC, 1, {0xA0} },
	{0xAD, 1, {0x9F} },
	{0xAE, 1, {0x9E} },
	{0xAF, 1, {0x9D} },
	{0xB0, 1, {0x9C} },
	{0xB1, 1, {0x9B} },
	{0xB2, 1, {0x9A} },
	{0xB3, 1, {0x99} },
	{0xB4, 1, {0x98} },
	{0xB5, 1, {0x97} },
	{0xB6, 1, {0x96} },
	{0xC7, 1, {0x00} },
	{0xC8, 1, {0x00} },
	{0xCD, 2, {0x38,0x80} },
	{0xCE, 2, {0x37,0x80} },
	{0xCF, 3, {0x04,0x00,0x20} },
	{0xD0, 2, {0x00,0x00} },
	{0xD1, 2, {0x00,0x00} },
	{0xD2, 1, {0x3F} },
	{0xFF, 1, {0x27} },
	{0xFB, 1, {0x01} },
	{0x13, 1, {0x06} },
	{0x58, 1, {0x80} },
	{0x59, 1, {0x00} },
	{0x5A, 1, {0x00} },
	{0x5B, 1, {0xB8} },
	{0x5C, 1, {0x00} },
	{0x5D, 1, {0x00} },
	{0x5E, 1, {0x00} },
	{0x5F, 1, {0x00} },
	{0x60, 1, {0x00} },
	{0x61, 1, {0x00} },
	{0x62, 1, {0x00} },
	{0x63, 1, {0x00} },
	{0x64, 1, {0x00} },
	{0x65, 1, {0x00} },
	{0x66, 1, {0x00} },
	{0x67, 1, {0x00} },
	{0x68, 1, {0x00} },
	{0x75, 1, {0xA8} },
	{0x76, 1, {0x00} },
	{0x77, 1, {0xA0} },
	{0xC0, 1, {0x00} },
	{0xC2, 1, {0x03} },
	{0xD0, 1, {0x13} },
	{0xD1, 1, {0x54} },
	{0xDF, 1, {0x02} },
	{0xD2, 1, {0x38} },
	{0xDE, 1, {0x43} },
	{0xFF, 1, {0x2A} },
	{0xFB, 1, {0x01} },
	{0x22, 1, {0x0F} },
	{0x23, 1, {0x10} },
	{0x24, 1, {0x00} },
	{0x25, 1, {0x62} },
	{0x27, 1, {0x05} },
	{0x28, 1, {0x8E} },
	{0x29, 1, {0x05} },
	{0x2A, 1, {0x8E} },
	{0x2B, 1, {0x05} },
	{0x2D, 1, {0x8E} },
	{0x64, 1, {0x93} },
	{0x67, 1, {0x93} },
	{0x68, 1, {0x32} },
	{0x6A, 1, {0x93} },
	{0x70, 1, {0x93} },
	{0x7F, 1, {0x93} },
	{0x97, 1, {0x3C} },
	{0x98, 1, {0x02} },
	{0x99, 1, {0x95} },
	{0x9A, 1, {0x03} },
	{0x9B, 1, {0x00} },
	{0x9C, 1, {0x0B} },
	{0x9D, 1, {0x0A} },
	{0x9E, 1, {0x90} },
	{0xF2, 1, {0x3C} },
	{0xF3, 1, {0x02} },
	{0xF4, 1, {0x95} },
	{0xF5, 1, {0x03} },
	{0xF6, 1, {0x00} },
	{0xF7, 1, {0x0B} },
	{0xF8, 1, {0x0A} },
	{0xF9, 1, {0x90} },
	{0xA2, 1, {0x3F} },
	{0xA4, 1, {0xC3} },
	{0xB6, 7, {0x00,0xFF,0xFF,0x00,0x00,0x77,0x77} },
	{0xB9, 9, {0x28,0x00,0xC7,0x00,0x80,0xB3,0x40,0x5F,0x41} },
	{0xBA, 6, {0xAA,0xAA,0xA5,0xA5,0xE1,0xE1} },
	{0xBB, 12, {0xAA,0xAA,0xAA,0xAA,0xAA,0x55,0xAA,0x55,0x38,0xC7,0x38,0xC7} },
	{0xBC, 2, {0x66,0x06} },
	{0xE8, 1, {0x00} },
	{0xFF, 1, {0x23} },
	{0xFB, 1, {0x01} },
	{0x00, 1, {0x80} },
	{0x07, 1, {0x00} },
	{0x08, 1, {0x01} },
	{0x09, 1, {0xC2} },
	{0x11, 1, {0x02} },
	{0x12, 1, {0x80} },
	{0x15, 1, {0x83} },
	{0x16, 1, {0x15} },
	{0xFF, 1, {0x20} },
	{0xFB, 1, {0x01} },
	{0xB0, 16, {0x00 ,0x08 ,0x00 ,0x2A ,0x00 ,0x5D ,0x00 ,0x84 ,0x00 ,0xA4 ,0x00 ,0xC0 ,0x00 ,0xD7 ,0x00 ,0xEC} },
	{0xB1, 16, {0x00 ,0xFE ,0x01 ,0x3C ,0x01 ,0x66 ,0x01 ,0xAA ,0x01 ,0xD9 ,0x02 ,0x24 ,0x02 ,0x5D ,0x02 ,0x5E} },
	{0xB2, 16, {0x02 ,0x97 ,0x02 ,0xD4 ,0x02 ,0xFB ,0x03 ,0x2C ,0x03 ,0x4C ,0x03 ,0x78 ,0x03 ,0x85 ,0x03 ,0x94} },
	{0xB3, 12, {0x03 ,0xA4 ,0x03 ,0xB2 ,0x03 ,0xD1 ,0x03 ,0xEF ,0x03 ,0xFD ,0x03 ,0xFF} },
	{0xB4, 16, {0x00 ,0x08 ,0x00 ,0x2A ,0x00 ,0x5D ,0x00 ,0x84 ,0x00 ,0xA4 ,0x00 ,0xC0 ,0x00 ,0xD7 ,0x00 ,0xEC} },
	{0xB5, 16, {0x00 ,0xFE ,0x01 ,0x3C ,0x01 ,0x66 ,0x01 ,0xAA ,0x01 ,0xD9 ,0x02 ,0x24 ,0x02 ,0x5D ,0x02 ,0x5E} },
	{0xB6, 16, {0x02 ,0x97 ,0x02 ,0xD4 ,0x02 ,0xFB ,0x03 ,0x2C ,0x03 ,0x4C ,0x03 ,0x78 ,0x03 ,0x85 ,0x03 ,0x94} },
	{0xB7, 12, {0x03 ,0xA4 ,0x03 ,0xB2 ,0x03 ,0xD1 ,0x03 ,0xEF ,0x03 ,0xFD ,0x03 ,0xFF} },
	{0xB8, 16, {0x00 ,0x08 ,0x00 ,0x2A ,0x00 ,0x5D ,0x00 ,0x84 ,0x00 ,0xA4 ,0x00 ,0xC0 ,0x00 ,0xD7 ,0x00 ,0xEC} },
	{0xB9, 16, {0x00 ,0xFE ,0x01 ,0x3C ,0x01 ,0x66 ,0x01 ,0xAA ,0x01 ,0xD9 ,0x02 ,0x24 ,0x02 ,0x5D ,0x02 ,0x5E} },
	{0xBA, 16, {0x02 ,0x97 ,0x02 ,0xD4 ,0x02 ,0xFB ,0x03 ,0x2C ,0x03 ,0x4C ,0x03 ,0x78 ,0x03 ,0x85 ,0x03 ,0x94} },
	{0xBB, 12, {0x03 ,0xA4 ,0x03 ,0xB2 ,0x03 ,0xD1 ,0x03 ,0xEF ,0x03 ,0xFD ,0x03 ,0xFF} },
	{0xC6, 1, {0x72} },
	{0xC7, 1, {0x30} },
	{0xC8, 1, {0x31} },
	{0xC9, 1, {0x20} },
	{0xCA, 1, {0x10} },
	{0xCB, 1, {0x72} },
	{0xCC, 1, {0x30} },
	{0xCD, 1, {0x31} },
	{0xCE, 1, {0x20} },
	{0xCF, 1, {0x10} },
	{0xD0, 1, {0x72} },
	{0xD1, 1, {0x30} },
	{0xD2, 1, {0x31} },
	{0xD3, 1, {0x20} },
	{0xD4, 1, {0x10} },
	{0xD5, 1, {0x72} },
	{0xD6, 1, {0x30} },
	{0xD7, 1, {0x31} },
	{0xD8, 1, {0x20} },
	{0xD9, 1, {0x10} },
	{0xDA, 1, {0x72} },
	{0xDB, 1, {0x30} },
	{0xDC, 1, {0x31} },
	{0xDD, 1, {0x20} },
	{0xDE, 1, {0x10} },
	{0xDF, 1, {0x72} },
	{0xE0, 1, {0x30} },
	{0xE1, 1, {0x31} },
	{0xE2, 1, {0x20} },
	{0xE3, 1, {0x10} },
	{0xFF, 1, {0x21} },
	{0xFB, 1, {0x01} },
	{0xB0, 16, {0x00 ,0x00 ,0x00 ,0x22 ,0x00 ,0x55 ,0x00 ,0x7C ,0x00 ,0x9C ,0x00 ,0xB8 ,0x00 ,0xCF ,0x00 ,0xE4} },
	{0xB1, 16, {0x00 ,0xF6 ,0x01 ,0x34 ,0x01 ,0x5E ,0x01 ,0xA2 ,0x01 ,0xD1 ,0x02 ,0x1C ,0x02 ,0x55 ,0x02 ,0x56} },
	{0xB2, 16, {0x02 ,0x8F ,0x02 ,0xCC ,0x02 ,0xF3 ,0x03 ,0x24 ,0x03 ,0x44 ,0x03 ,0x70 ,0x03 ,0x7D ,0x03 ,0x8C} },
	{0xB3, 12, {0x03 ,0x9C ,0x03 ,0xAA ,0x03 ,0xC9 ,0x03 ,0xE7 ,0x03 ,0xF5 ,0x03 ,0xF7} },
	{0xB4, 16, {0x00 ,0x00 ,0x00 ,0x22 ,0x00 ,0x55 ,0x00 ,0x7C ,0x00 ,0x9C ,0x00 ,0xB8 ,0x00 ,0xCF ,0x00 ,0xE4} },
	{0xB5, 16, {0x00 ,0xF6 ,0x01 ,0x34 ,0x01 ,0x5E ,0x01 ,0xA2 ,0x01 ,0xD1 ,0x02 ,0x1C ,0x02 ,0x55 ,0x02 ,0x56} },
	{0xB6, 16, {0x02 ,0x8F ,0x02 ,0xCC ,0x02 ,0xF3 ,0x03 ,0x24 ,0x03 ,0x44 ,0x03 ,0x70 ,0x03 ,0x7D ,0x03 ,0x8C} },
	{0xB7, 12, {0x03 ,0x9C ,0x03 ,0xAA ,0x03 ,0xC9 ,0x03 ,0xE7 ,0x03 ,0xF5 ,0x03 ,0xF7} },
	{0xB8, 16, {0x00 ,0x00 ,0x00 ,0x22 ,0x00 ,0x55 ,0x00 ,0x7C ,0x00 ,0x9C ,0x00 ,0xB8 ,0x00 ,0xCF ,0x00 ,0xE4} },
	{0xB9, 16, {0x00 ,0xF6 ,0x01 ,0x34 ,0x01 ,0x5E ,0x01 ,0xA2 ,0x01 ,0xD1 ,0x02 ,0x1C ,0x02 ,0x55 ,0x02 ,0x56} },
	{0xBA, 16, {0x02 ,0x8F ,0x02 ,0xCC ,0x02 ,0xF3 ,0x03 ,0x24 ,0x03 ,0x44 ,0x03 ,0x70 ,0x03 ,0x7D ,0x03 ,0x8C} },
	{0xBB, 12, {0x03 ,0x9C ,0x03 ,0xAA ,0x03 ,0xC9 ,0x03 ,0xE7 ,0x03 ,0xF5 ,0x03 ,0xF7} },
	{0xFF, 1, {0x26} },
	{0xFB, 1, {0x01} },
	{0xD2, 1, {0x2F} },
	{0xC9, 1, {0x00} },
	{0xFF, 1, {0x10} },
	{0xFB, 1, {0x01} },
	{0x35, 1, {0x00} },
	{0x51, 2, {0x0F,0xFF} },
	{0x53, 1, {0x2C} },
	{0x55, 1, {0x00} },
	{0x90, 1, {0x03} },
	{0x91, 16, {0x89, 0xA8, 0x00, 0x14, 0xD2, 0x00, 0x02, 0xB2, 0x02, 0x2B, 0x00, 0x0B, 0x05, 0x7A, 0x03, 0x68} },
	{0x92, 2, {0x10, 0xE0} },
	{0xB2, 1, {0x91} },
	{0xB3, 1, {0x00} },
	{0xBB, 1, {0x13} },
	{0x3B, 6, {0x03,0xC2,0x1E,0x04,0x04,0x00} },
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 1, {0x00} },
    {REGFLAG_DELAY, 20, {} },
};

static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	unsigned int i, j;
	unsigned char temp[255] = {0};
	unsigned int cmd;
	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		memset(temp, 0, sizeof(temp));
		switch (cmd) {
		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				msleep(table[i].count);
			else
				msleep(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			temp[0] = cmd;
			for (j = 0; j < table[i].count; j++)
				temp[j+1] = table[i].para_list[j];
			lcm_dcs_write(ctx, temp, table[i].count+1);
		}
	}
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;
	pr_info("[LCM] %s begin\n", __func__);

	lcm_dcs_write_seq_static(ctx, 0x28);
	mdelay(20);
	lcm_dcs_write_seq_static(ctx, 0x10);
	mdelay(120);
/* Low power mode: deep sleep by keke.wang 20260109 start
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_err("cannot get reset-gpios %ld\n", PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	mdelay(5);
	Low power mode: deep sleep by keke.wang 20260109 end
*/
	ctx->avee_en_gpio = devm_gpiod_get(ctx->dev, "avee",GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avee_en_gpio)) {
		pr_err("cannot get avee-gpios %ld\n", PTR_ERR(ctx->avee_en_gpio));
		return PTR_ERR(ctx->avee_en_gpio);
	}
	gpiod_set_value(ctx->avee_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->avee_en_gpio);
	mdelay(5);

	ctx->avdd_en_gpio = devm_gpiod_get(ctx->dev, "avdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avdd_en_gpio)) {
		pr_err("cannot get avdd-gpios %ld\n", PTR_ERR(ctx->avdd_en_gpio));
		return PTR_ERR(ctx->avdd_en_gpio);
	}
	gpiod_set_value(ctx->avdd_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->avdd_en_gpio);
	mdelay(5);

	ctx->error = 0;
	ctx->prepared = false;

	pr_info("[LCM] %s end\n", __func__);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;
//	int i = 0;

	pr_info("[LCM] %s begin\n", __func__);
	if (ctx->prepared)
		return 0;

	ctx->avdd_en_gpio = devm_gpiod_get(ctx->dev, "avdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avdd_en_gpio)) {
		pr_err("cannot get avdd-gpios %ld\n", PTR_ERR(ctx->avdd_en_gpio));
		return PTR_ERR(ctx->avdd_en_gpio);
	}
	gpiod_set_value(ctx->avdd_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->avdd_en_gpio);
	mdelay(5);

	ctx->avee_en_gpio = devm_gpiod_get(ctx->dev, "avee", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avee_en_gpio)) {
		pr_err("cannot get avee-gpios %ld\n", PTR_ERR(ctx->avee_en_gpio));
		return PTR_ERR(ctx->avee_en_gpio);
	}
	gpiod_set_value(ctx->avee_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->avee_en_gpio);
#if 0
	for (i = 0; i < ARRAY_SIZE(ocp2131_resume_data); i++) {
		mtk_panel_i2c_write_bytes(ocp2131_resume_data[i].cmd, ocp2131_resume_data[i].data);
		mdelay(1);
	}
#endif
	mdelay(10);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_err("cannot get reset-gpios %ld\n", PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	//Low power mode: deep sleep by keke.wang 20260109 start
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(15);
	//Low power mode: deep sleep by keke.wang 20260109 end
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(15);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	push_table(ctx, lcm_initialization_setting,
		sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table));

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	pr_info("[LCM] %s end\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	ctx->enabled = true;

	return 0;
}
/*
static const struct drm_display_mode switch_mode_120hz = {
	.clock = (int)((FRAME_WIDTH + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_120_VFP + VSA + VBP) * MODE_120_FPS / 1000),
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_120_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_120_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_120_VFP + VSA + VBP,
};
*/
static const struct drm_display_mode switch_mode_90hz = {
	.clock = (int)((FRAME_WIDTH + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_90_VFP + VSA + VBP) * MODE_90_FPS / 1000),
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_90_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_90_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_90_VFP + VSA + VBP,
};
/*
static const struct drm_display_mode switch_mode_60hz = {
	.clock = (int)((FRAME_WIDTH + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_60_VFP + VSA + VBP) * MODE_60_FPS / 1000),
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_60_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_60_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_60_VFP + VSA + VBP,
};
*/

#if defined(CONFIG_MTK_PANEL_EXT)
/*
static struct mtk_panel_params ext_params_120hz = {
	.data_rate = DATA_RATE,
	.ssc_enable=0,
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.tran_panel_params = &panel_driver_status,
};
*/
static struct mtk_panel_params ext_params_90hz = {
	.data_rate = DATA_RATE,
	.ssc_enable=0,
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.rotate = MTK_PANEL_ROTATE_180,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.lp_perline_en = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
        .enable = 1,	//enable or disable DSC                    
        .ver = 18,	//DSC version, ex) DSC 1.1 = 17 (0x11)
        .slice_mode = 1,	//1-slice mode(0) 2-slice mode(1) 注意 1-slice mode 对应的值并不是1而是0
        .rgb_swap = 0,	//select RGB swap for input. RGB(0), BGR(1)
        .dsc_cfg = 34,	//flatness detection config 请参考平台默认同类型lcm driver的配置 (10bit是40, 8bit是34)
        .rct_on = 1,	//Converts RGB to YCoCg. input YCoCg(0), input RGB(1)
        .bit_per_channel = 8,	//bits per R,G,B channel(10bit就是10, 8bit就是8)
        .dsc_line_buf_depth = 9,	//Indicates the number of bits of precision within the decoder line buffer (This value should align with LCM vendor)
        .bp_enable = 1,	//enable Block Prediction(This value should align with LCM vendor)
        .bit_per_pixel = 128,	//bpp * 16, (default bpp = 8)
        .pic_height = 2560,	//equal frame height
        .pic_width = 1600,	//equal frame width
        .slice_height = 20,	//This value should align with LCM vendor
        .slice_width = 800,	//frame width / slice mode
        .chunk_size = 800,	//(slice_width * bpp / 8) Bytes
        .xmit_delay = 512,
        .dec_delay = 690,
        .scale_value = 32,
        .increment_interval = 555,
        .decrement_interval = 11,
        .line_bpg_offset = 13,
        .nfl_bpg_offset = 1402,
        .slice_bpg_offset = 872,
        .initial_offset = 6144,
        .final_offset = 4320,
        .flatness_minqp = 3,
        .flatness_maxqp = 12,
        .rc_model_size = 8192,
        .rc_edge_factor = 6,
        .rc_quant_incr_limit0 = 11,
        .rc_quant_incr_limit1 = 11,
        .rc_tgt_offset_hi = 3,
        .rc_tgt_offset_lo = 3,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = nt36523n_vdo_90hz_dphy_buf_thresh,
			.range_min_qp = nt36523n_vdo_90hz_dphy_range_min_qp,
			.range_max_qp = nt36523n_vdo_90hz_dphy_range_max_qp,
			.range_bpg_ofs = nt36523n_vdo_90hz_dphy_range_bpg_ofs,
		},
	},
	.tran_panel_params = &panel_driver_status,
};
/*
static struct mtk_panel_params ext_params_60hz = {
	.data_rate = DATA_RATE,
	.ssc_enable=0,
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
	    .enable = 1,	//enable or disable DSC                    
        .ver = 18,	//DSC version, ex) DSC 1.1 = 17 (0x11)
        .slice_mode = 1,	//1-slice mode(0) 2-slice mode(1) 注意 1-slice mode 对应的值并不是1而是0
        .rgb_swap = 0,	//select RGB swap for input. RGB(0), BGR(1)
        .dsc_cfg = 34,	//flatness detection config 请参考平台默认同类型lcm driver的配置 (10bit是40, 8bit是34)
        .rct_on = 1,	//Converts RGB to YCoCg. input YCoCg(0), input RGB(1)
        .bit_per_channel = 8,	//bits per R,G,B channel(10bit就是10, 8bit就是8)
        .dsc_line_buf_depth = 9,	//Indicates the number of bits of precision within the decoder line buffer (This value should align with LCM vendor)
        .bp_enable = 1,	//enable Block Prediction(This value should align with LCM vendor)
        .bit_per_pixel = 128,	//bpp * 16, (default bpp = 8)
        .pic_height = 2560,	//equal frame height
        .pic_width = 1600,	//equal frame width
        .slice_height = 20,	//This value should align with LCM vendor
        .slice_width = 800,	//frame width / slice mode
        .chunk_size = 800,	//(slice_width * bpp / 8) Bytes
        .xmit_delay = 512,
        .dec_delay = 690,
        .scale_value = 32,
        .increment_interval = 555,
        .decrement_interval = 11,
        .line_bpg_offset = 13,
        .nfl_bpg_offset = 1402,
        .slice_bpg_offset = 872,
        .initial_offset = 6144,
        .final_offset = 4320,
        .flatness_minqp = 3,
        .flatness_maxqp = 12,
        .rc_model_size = 8192,
        .rc_edge_factor = 6,
        .rc_quant_incr_limit0 = 11,
        .rc_quant_incr_limit1 = 11,
        .rc_tgt_offset_hi = 3,
        .rc_tgt_offset_lo = 3,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = nt36523n_vdo_90hz_dphy_buf_thresh,
			.range_min_qp = nt36523n_vdo_90hz_dphy_range_min_qp,
			.range_max_qp = nt36523n_vdo_90hz_dphy_range_max_qp,
			.range_bpg_ofs = nt36523n_vdo_90hz_dphy_range_bpg_ofs,
		},
	},
	.tran_panel_params = &panel_driver_status,
};
*/
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	unsigned int mapped_level;

	if(level <= 4095)
		mapped_level = level * 3276 / 4095;
	else
		mapped_level = 3276 + ((level-4096) * (4095-3276) / 1023);

	bl_tb0[1] = ((mapped_level >> 8) & 0xf);
	bl_tb0[2] = (mapped_level & 0xff);

	if (!cb)
		return -1;

	if ((g_dim_enable == 0) && (g_need_dim_enable == 1)) {
		bl_dim[1] = 0x2C;
		cb(dsi, handle, bl_dim, ARRAY_SIZE(bl_dim));
		g_dim_enable = 1;
	}

	g_need_dim_enable = 1;

	if (mapped_level == 0) {
		bl_dim[1] = 0x24;
		cb(dsi, handle, bl_dim, ARRAY_SIZE(bl_dim));
		g_dim_enable = 0;
		g_need_dim_enable = 0;
	}
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	pr_info("[LCM] %s level = %d, mapped_level = %d\n", __func__, level, mapped_level);

	return 0;
}

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
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (drm_mode_vrefresh(m) == MODE_90_FPS)
		ext->params = &ext_params_90hz;
	else
		ret = 1;
	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("[LCM] %s begin\n", __func__);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}

static int panel_doze_enable(struct drm_panel *panel,
		    void *dsi, dcs_write_gce cb, void *handle)
{
	pr_info("[LCM] %s begin\n", __func__);
	g_aod_enable=1;
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
		    void *dsi, dcs_write_gce cb, void *handle)
{
	pr_info("[LCM] %s begin\n", __func__);
	g_aod_enable=0;
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}

#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
static int panel_set_aod_light_mode(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	unsigned int aod_mapped_level = 0;
	switch (level) {
	case 20://doze_backlight_level1
		aod_mapped_level = 0x14;  //5nit
		break;
	case 126://doze_backlight_level2
		aod_mapped_level = 0xC8;  //30nit
		break;
	case 247://doze_backlight_level3
		aod_mapped_level = 0x190;  //60nit
		break;
	default:
		aod_mapped_level = 0x190;  //60nit
		break;
	}
	pr_info("[LCM] %s level is %u,aod_mapped_level is %d\n", __func__, level, aod_mapped_level);

	bl_tb0[1] = ((aod_mapped_level >> 8) & 0x0f);
	bl_tb0[2] = (aod_mapped_level & 0xff);
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}
#endif

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x9c, 0x02, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x0a, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	printk("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0])
		return 1;

	printk("ATA expect read data is %x %x %x\n", id[0], id[1], id[2]);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
	.set_aod_light_mode = panel_set_aod_light_mode,
#endif
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
	//struct drm_display_mode *mode_60hz;
	struct drm_display_mode *mode_90hz;
	//struct drm_display_mode *mode_120hz;
	pr_info("[LCM] %s begin\n", __func__);
	/*mode_120hz = drm_mode_duplicate(connector->dev, &switch_mode_120hz);
	if (!mode_120hz) {
		dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_120hz.hdisplay, switch_mode_120hz.vdisplay, drm_mode_vrefresh(&switch_mode_120hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_120hz);
	mode_120hz->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_120hz);
*/
	mode_90hz = drm_mode_duplicate(connector->dev, &switch_mode_90hz);
	if (!mode_90hz) {
		dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_90hz.hdisplay, switch_mode_90hz.vdisplay, drm_mode_vrefresh(&switch_mode_90hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_90hz);
	mode_90hz->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(connector, mode_90hz);

	/*mode_60hz = drm_mode_duplicate(connector->dev, &switch_mode_60hz);
	if (!mode_60hz) {
		dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_60hz.hdisplay, switch_mode_60hz.vdisplay, drm_mode_vrefresh(&switch_mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_60hz);
	mode_60hz->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(connector, mode_60hz);
*/
	connector->display_info.width_mm = 69;
	connector->display_info.height_mm = 154;

	pr_info("[LCM] %s end\n", __func__);
	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
	unsigned int doze_backlight[] = {0, 0, 0, 0};
#endif
	pr_info("%s+\n", __func__);

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

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_err("cannot get reset-gpios %ld\n", PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->avdd_en_gpio = devm_gpiod_get(dev, "avdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avdd_en_gpio)) {
		pr_err("cannot get avdd-gpios %ld\n", PTR_ERR(ctx->avdd_en_gpio));
		return PTR_ERR(ctx->avdd_en_gpio);
	}
	devm_gpiod_put(dev, ctx->avdd_en_gpio);

	ctx->avee_en_gpio = devm_gpiod_get(dev, "avee", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avee_en_gpio)) {
		pr_err("cannot get avee-gpios %ld\n", PTR_ERR(ctx->avee_en_gpio));
		return PTR_ERR(ctx->avee_en_gpio);
	}
	devm_gpiod_put(dev, ctx->avee_en_gpio);

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
	ret = mtk_panel_ext_create(dev, &ext_params_90hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	pr_info("%s-\n", __func__);

	return ret;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif
	pr_info("[LCM] %s begin\n", __func__);
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
	pr_info("[LCM] %s end\n", __func__);
}

static const struct of_device_id lcm_of_match[] = {
	{.compatible = "nt36523n,wqxga,dsi,vdo,dsc,hjr,90hz,x1103",},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "nt36523n_wqxga_dsi_vdo_dsc_hjr_90hz_x1103",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("long.chen <long.chen@transsion.com>");
MODULE_DESCRIPTION("nt36523n VDO 90HZ LCD Panel Driver");
MODULE_LICENSE("GPL v2");
