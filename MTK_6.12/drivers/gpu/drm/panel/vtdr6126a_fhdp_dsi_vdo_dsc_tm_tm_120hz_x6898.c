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
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
//#include "../mediatek/mediatek_v2/mtk_log.h"
#define REGFLAG_DELAY               0xFFFC
#define REGFLAG_UDELAY              0xFFFB
#define REGFLAG_END_OF_TABLE        0xFFFD
#define REGFLAG_RESET_LOW           0xFFFE
#define REGFLAG_RESET_HIGH          0xFFFF
#define FRAME_WIDTH                 1208
#define FRAME_HEIGHT                2644
#define PHYSICAL_WIDTH              70670
#define PHYSICAL_HEIGHT             157050
#define VSA                         2
#define VBP                         22
#define HSA                         4
#define HBP                         16
/*Parameter setting for 120Hz Start*/
#define MODE_1_FPS                  120
#define MODE_1_VFP                  68
#define MODE_1_HFP                  132
#define MODE_1_DATA_RATE            1096
/*Parameter setting for 120Hz End*/
/*Parameter setting for 90Hz Start*/
#define MODE_2_FPS                  90
#define MODE_2_VFP                  980
#define MODE_2_HFP                  132
#define MODE_2_DATA_RATE            1096
/*Parameter setting for 90Hz End*/
/*Parameter setting for 60Hz Start*/
#define MODE_3_FPS                  60
#define MODE_3_VFP                  2804
#define MODE_3_HFP                  132
#define MODE_3_DATA_RATE            1096
/*Parameter setting for 60Hz End*/
/* DSC RELATED */
#define DSC_ENABLE		1
#define DSC_VER			18
#define DSC_SLICE_MODE		1
#define DSC_RGB_SWAP		0
#define DSC_DSC_CFG		40
#define DSC_RCT_ON		1
#define DSC_BIT_PER_CHANNEL	10
#define DSC_DSC_LINE_BUF_DEPTH	11
#define DSC_BP_ENABLE		1
#define DSC_BIT_PER_PIXEL	128
#define DSC_SLICE_HEIGHT	8
#define DSC_SLICE_WIDTH		604
#define DSC_CHUNK_SIZE		604
#define DSC_XMIT_DELAY		512
#define DSC_DEC_DELAY	282
#define DSC_SCALE_VALUE		32
#define DSC_INCREMENT_INTERVAL	346
#define DSC_DECREMENT_INTERVAL	8
#define DSC_LINE_BPG_OFFSET	1
#define DSC_NFL_BPG_OFFSET	293
#define DSC_SLICE_BPG_OFFSET	2920
#define DSC_INITIAL_OFFSET	6144
#define DSC_FINAL_OFFSET	4352
#define DSC_FLATNESS_MINQP	7
#define DSC_FLATNESS_MAXQP	16
#define DSC_RC_MODEL_SIZE	8192
#define DSC_RC_EDGE_FACTOR	6
#define DSC_RC_QUANT_INCR_LIMIT0	15
#define DSC_RC_QUANT_INCR_LIMIT1	15
#define DSC_RC_TGT_OFFSET_HI		3
#define DSC_RC_TGT_OFFSET_LO		3
static unsigned int rc_buf_thresh[14] = {896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064};
unsigned int range_min_qp[15] = {0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16};
unsigned int range_max_qp[15] = {8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 14, 15, 15, 16, 17};
int range_bpg_ofs[15] = {2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12};
#endif
/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
/* option function to read data from some panel address */
/* #define PANEL_SUPPORT_READBACK */
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vddi_gpio; 
	struct regulator *vci_regulator; //pmic vmch chanel
	struct gpio_desc *dvdd_gpio; 
	bool prepared;
	bool enabled;
	int error;
	bool hbm_en;
	bool hbm_wait;
};

tran_lcm_doze_backlight g_lcm_doze_backlight = {
	.doze_backlight_num = 0,
	.doze_backlight_level1 = 0,
	.doze_backlight_level2 = 0,
	.doze_backlight_level3 = 0,
};

struct tran_panel_driver_params panel_driver_status = {0};
static char bl_tb0[] = {0x51, 0x0D, 0xBB};
static unsigned int last_mapped_level = 0;
static int g_aod_enable = 0;
static int g_hbm_enable = 0;
static int g_lcm_fresh_mode = MODE_1_FPS; //default 120Hz
static unsigned int mapped_level = 0;
static char bl_dim[] = {0x53, 0x28}; //enable ddic dim function
static unsigned int g_dim_state = 0; //record dim state
static unsigned int g_need_dim_enable = 0; //if or not need dim function
static unsigned int g_aod_setbacklight = 0; //record aod_setbacklight state
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
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
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
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}
struct LCM_setting_table_v2 {
	unsigned char count;
	unsigned char para_list[200];
};
static struct LCM_setting_table_v2 lcm_initialization_setting_v2[] = {
    //==============CM1===================// 
    {2,{0x6C,0x01}},// 00 144Hz ；01 120Hz；02 90Hz；03 60}},
    {2,{0x03,0x01}},
    {2,{0x35,0x00}},
    {3,{0x51,0x00,0x00}},
    {2,{0x53,0x20}},
    {2,{0x59,0x09}},//Demura ON=09 OFF=00
    {2,{0x5E,0x00}},//0= Hybrid mode ,1= DC mo}},
    {2,{0x6D,0x00}},
    {2,{0x6F,0x01}},
    {2,{0x72,0x00}},//00 SGM3837s  01 SGM38}},
    //Vesa 1.2 10bit decoder=2 slice height=8
    {95,{0x70,0x12,0x00,0x00,0xAB,0x30,0x80,0x0A,0x54,0x04,0xB8,0x00,0x08,0x02,0x5C,0x02,0x5C,0x02,0x00,0x01,0x1A,0x00,0x20,0x01,0x5A,0x00,0x08,0x00,0x01,0x01,0x25,0x0B,0x68,0x18,0x00,0x11,0x00,0x07,0x10,0x20,0x00,0x06,0x0F,0x0F,0x33,0x0E,0x1C,0x2A,0x38,0x46,0x54,0x62,0x69,0x70,0x77,0x79,0x7B,0x7D,0x7E,0x02,0x02,0x22,0x00,0x2A,0x40,0x2A,0xBE,0x3A,0xFC,0x3A,0xFA,0x3A,0xF8,0x3B,0x38,0x3B,0x78,0x3B,0xB6,0x4B,0xB6,0x4B,0xF4,0x4B,0xF4,0x6C,0x34,0x84,0x74,0x00,0x00,0x00,0x00,0x00,0x00}},
    {3,{0xF0,0xAA,0x10}},
    {6,{0xB0,0x05,0x2A,0x01,0x2E,0x01}},
    //==============CM3===================//
    //AOD MIPI time out
    {3,{0xFF,0x5A,0x80}},
    {2,{0x65,0x0A}},
    {2,{0xF9,0x1E}},
    //By V清error
    {3,{0xFF,0x5A,0x80}},
    {2,{0x65,0x25}},
    {2,{0xfd,0x01}},
    //SOP
    {3,{0xFF,0x5A,0x81}},
    {2,{0x65,0x03}},
    {2,{0xF3,0x61}},
    {2,{0x65,0x0B}},
    {2,{0xF3,0x78}},
    //By V清error
    {3,{0xFF,0x5A,0x83}},
    {2,{0x65,0x0B}},
    {2,{0xF7,0x03}},
    //crc off
    {3,{0xFF,0x5A,0x83}},
    {2,{0x65,0x09}},
    {3,{0xF7,0x10,0x10}},
    //RUF OFF
    {3,{0xF0,0xAA,0x18}},
    {2,{0xB0,0x80}},
    {3,{0xF0,0xAA,0x16}},
    //MTE_VRR_BASE=1
    {17,{0xD1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    //CMD3 VGLR power sequence
    {3,{0xFF,0x5A,0x81}},
    {2,{0x65,0x02}},
    {3,{0xFB,0x73,0x73}},
    {3,{0xF0,0xAA,0x13}},
    {14,{0xC8,0x10,0xC0,0x13,0x01,0x90,0x0F,0xFF,0x0F,0xFF,0x0F,0x01,0x01,0x01}},
    {14,{0xD0,0x10,0x80,0x00,0x01,0x90,0x0F,0xFF,0x0F,0xFF,0x11,0x01,0x01,0x01}},
    {14,{0xE0,0x10,0x80,0x00,0x01,0x90,0x0F,0xFF,0x0F,0xFF,0x17,0x01,0x01,0x01}},
    {14,{0xE8,0x10,0x80,0x00,0x01,0x90,0x0F,0xFF,0x0F,0xFF,0x23,0x01,0x01,0x01}},
    //LHBM
    {3,{0xF0,0xAA,0x18}},
    {14,{0xB6,0x65,0x00,0x1D,0x08,0xBD,0x01,0x00,0x47,0x57,0x57,0x00,0x00,0x00}},
    {9,{0xB7,0x01,0x01,0xF7,0x08,0xFA,0x00,0xCA,0xCA}},
    {2,{0x65,0x0C}},
    {2,{0xB0,0x04}},
    {16,{0xC0,0xE0,0xAB,0x96,0x49,0x63,0xC9,0xEC,0xBA,0x7E,0x20,0x10,0x48,0x44,0x32,0x20}},
    {16,{0xC1,0x22,0x89,0x88,0xC4,0x72,0x38,0xA0,0x90,0x29,0x14,0x92,0x49,0x24,0x91,0x50}},
    {16,{0xC2,0x15,0x0A,0x85,0x42,0xA1,0x50,0xA8,0x54,0x2A,0x15,0x12,0x49,0x22,0x91,0x48}},
    {16,{0xC3,0x14,0x8A,0x45,0x02,0x81,0x41,0x1C,0x4E,0x27,0x13,0x09,0x84,0xC2,0x51,0x28}},
    {16,{0xC4,0x12,0x09,0x04,0x62,0x31,0x10,0x88,0x42,0x21,0x10,0x08,0x07,0xE3,0xE1,0xF0}},
    {16,{0xC5,0x1E,0x8F,0x47,0x83,0xB1,0xD8,0xE8,0x72,0x39,0x1C,0x0D,0xC6,0xC3,0x61,0xA8}},
    {16,{0xC6,0x1A,0x0C,0xC6,0x43,0x21,0x88,0xC0,0x5E,0x2E,0x17,0x0B,0x45,0x82,0xB1,0x50}},
    {16,{0xC7,0x14,0x8A,0x04,0xE2,0x61,0x28,0x90,0x46,0x22,0x10,0x80,0x40,0x00,0x00,0x00}},
    {16,{0xE0,0x94,0xA3,0x06,0xA8,0xE3,0xBC,0x98,0x6C,0xC5,0x41,0x9A,0x75,0x02,0x51,0x18}},
    {16,{0xE1,0x5E,0x99,0xA5,0xE1,0x15,0x24,0x7A,0xAB,0x0D,0xB8,0x1B,0x6C,0x15,0x4B,0x43}},
    {16,{0xE2,0x54,0x71,0xF3,0x32,0x36,0xD8,0x0A,0x96,0x42,0x75,0xC6,0xB7,0xDF,0x7F,0xE0}},
    {16,{0xE3,0x00,0x00,0x00,0x7F,0x60,0x00,0x00,0x00,0x03,0x60,0x00,0x00,0x00,0x00,0x00}},
    {3,{0xF0,0xAA,0x1C}},
    {7,{0xC0,0x02,0x26,0x01,0xF4,0x02,0x92}},
    {21,{0xc1,0x01,0x91,0x03,0xeb,0x06,0x5f,0x08,0xd3,0x0b,0x47,0x0d,0xbb,0x0e,0x7c,0x0f,0x3d,0x0f,0xfe,0x0f,0xff}},
    {3,{0xc4,0x01,0xff}},
    {2,{0x65,0x00}},
    {4,{0xc2,0x3E,0x36,0x2A}},
    {2,{0x65,0x02}},
    {4,{0xc4,0x03,0x04,0x04}},
    {2,{0x65,0x03}},
    {4,{0xc2,0x3D,0x36,0x28}},
    {2,{0x65,0x05}},
    {4,{0xc4,0x04,0x04,0x04}},
    {2,{0x65,0x06}},
    {4,{0xc2,0x3C,0x36,0x27}},
    {2,{0x65,0x08}},
    {4,{0xc4,0x06,0x06,0x07}},
    {2,{0x65,0x09}},
    {4,{0xc2,0x3B,0x36,0x26}},
    {2,{0x65,0x0b}},
    {4,{0xc4,0x08,0x08,0x0a}},
    {2,{0x65,0x0c}},
    {4,{0xc2,0x3C,0x36,0x26}},
    {2,{0x65,0x0e}},
    {4,{0xc4,0x09,0x0a,0x0c}},
    {2,{0x65,0x0f}},
    {4,{0xc2,0x3B,0x34,0x24}},
    {2,{0x65,0x11}},
    {4,{0xc4,0x0d,0x0e,0x11}},
    {2,{0x65,0x12}},
    {4,{0xc2,0x29,0x24,0x0F}},
    {2,{0x65,0x14}},
    {4,{0xc4,0x13,0x12,0x15}},
    {2,{0x65,0x15}},
    {4,{0xc2,0x19,0x16,0x83}},
    {2,{0x65,0x17}},
    {4,{0xc4,0x17,0x18,0x1b}},
    {2,{0x65,0x18}},
    {4,{0xc2,0x09,0x08,0x95}},
    {2,{0x65,0x1a}},
    {4,{0xc4,0x1f,0x1e,0x22}},
    {2,{0x65,0x1b}},
    {4,{0xc2,0x0F,0x0E,0x91}},
    {2,{0x65,0x1d}},
    {4,{0xc4,0x1d,0x1c,0x1f}},
    //Swire Dimming
    {3,{0xF0,0xAA,0x10}},
    {2,{0x65,0x01}},
    {2,{0xD0,0xE8}},
    //Dimming 24 Frame
    {3,{0xF0,0xAA,0x13}},
    {2,{0xC8,0x20}},
    {2,{0xD0,0x20}},
    {2,{0xE0,0x20}},
    {2,{0xE8,0x20}},
    //Peaklum IP开启阈值7%
    {3,{0xF0,0xAA,0x1C}},
    {2,{0x65,0x05}},
    {3,{0xB0,0x53,0x67}},
	//降低DVDD_baseline 251015
	{3,{0xF0,0xAA,0x12}},
	{2,{0xC5,0x12}},
	{2,{0x65,0x0A}},
	{2,{0xC5,0x12}},
	{2,{0x65,0x13}},
	{2,{0xC5,0x12}},
    //disable cmd2&cmd3
    {3,{0xF0,0xaa,0x00}},
    {3,{0xFF,0x5A,0x00}},
    //Sleep out, display on
    {1, {0x11}},
    {0, {120}},
    {1, {0x29}},
    {0, {20}},
};
static struct LCM_setting_table_v2 lcm_suspend_setting_v2[] = {
	// Display off sequence
	{1, {0x28}},
	{0, {20}},
	// Sleep Mode On
	{1, {0x10}},
	{0, {100}},
};
static int lcm_panel_init_v2(void *dsi_drv, struct drm_panel *panel, void *handle, mtk_dsi_ddic_cmd cb,
			struct mtk_dsi_cmd_option *cmd_opt)
{
	int i;
	struct lcm *ctx = panel_to_lcm(panel);
	static struct mipi_dsi_msg init_cmd[ARRAY_SIZE(lcm_initialization_setting_v2)] = { 0 };
	static int flag;
	int ret = 0;

	pr_info("[LCM] %s begin\n", __func__);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(25);

	if (!flag) {
		flag = 1;
		for (i = 0; i < ARRAY_SIZE(lcm_initialization_setting_v2); i++) {
			init_cmd[i].tx_len= lcm_initialization_setting_v2[i].count;
			init_cmd[i].tx_buf = lcm_initialization_setting_v2[i].para_list;
		}
	}
	struct mtk_dsi_cmd_msg cmd_msg = {
		.is_package = 0,
		.cmd_num = ARRAY_SIZE(lcm_initialization_setting_v2),
		.transfer_mode = PACKET_LP_MODE,
		.cmd_msg = init_cmd,
	};

	switch (g_lcm_fresh_mode) {
		case MODE_1_FPS:
			lcm_initialization_setting_v2[0].para_list[1] = 0x01;
			break;
		case MODE_2_FPS:
			lcm_initialization_setting_v2[0].para_list[1] = 0x02;
			break;
		case MODE_3_FPS:
			lcm_initialization_setting_v2[0].para_list[1] = 0x03;
			break;
	}
	ret = cb(dsi_drv, handle, cmd_opt, &cmd_msg);

	if (ret < 0) {
		pr_info("[LCM] %s error\n", __func__);
		goto end;
	}

	if (mapped_level) {
		pr_info("[LCM] %s esd recovery, need set curr_bl=%d \n", __func__, mapped_level);

		struct mipi_dsi_msg bl_tb0_cmd = { 0 };
		bl_tb0_cmd.tx_buf = bl_tb0;
		bl_tb0_cmd.tx_len = 3;
		struct mtk_dsi_cmd_msg bl_tb0_msg = {
			.cmd_num = 1,
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = &bl_tb0_cmd,
		};
		ret = cb(dsi_drv, handle, cmd_opt, &bl_tb0_msg);
		if (ret < 0) {
			pr_info("[LCM] %s set blerror\n", __func__);
			goto end;
		}
	}
	ctx->error=0;
	g_dim_state = 0;
	g_need_dim_enable = 0;
	ctx->prepared = true;
	pr_info("[LCM] %s end\n", __func__);
end:
	return ret;
}
static int lcm_panel_deinit_v2(void *dsi_drv, struct drm_panel *panel, void *handle, mtk_dsi_ddic_cmd cb,
			struct mtk_dsi_cmd_option *cmd_opt)
{
	int i;
	int ret = 0;
	static int flag;
	struct lcm *ctx = panel_to_lcm(panel);
	static struct mipi_dsi_msg suspend_code[ARRAY_SIZE(lcm_suspend_setting_v2)] = { 0 };

	pr_info("%s ++\n", __func__);

	if (!ctx->prepared) {
		pr_info("%s skip\n", __func__);
		return 0;
	}

	if (!flag) {
		flag = 1;
		for (i = 0; i < ARRAY_SIZE(lcm_suspend_setting_v2); i++) {
			suspend_code[i].tx_len= lcm_suspend_setting_v2[i].count;
			suspend_code[i].tx_buf = lcm_suspend_setting_v2[i].para_list;
		}
	}

	struct mtk_dsi_cmd_msg suspend_cmd = {
		.cmd_num = ARRAY_SIZE(lcm_suspend_setting_v2),
		.transfer_mode = PACKET_HS_MODE,
		.cmd_msg = suspend_code,
	};

	ret = cb(dsi_drv, handle, cmd_opt, &suspend_cmd);

	if (ret < 0) {
		pr_info("%s error\n", __func__);
		goto end;
	}

	pr_info("%s --\n", __func__);

end:
	return ret;
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("[LCM] %s begin\n", __func__);
	if (!ctx->enabled)
		return 0;
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}
	ctx->enabled = false;
	return 0;
}
static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);


	if (!ctx->prepared)
		return 0;
	pr_info("[LCM] %s begin\n", __func__);
	g_aod_enable=0;

	ctx->error = 0;
	ctx->prepared = false;
	g_hbm_enable = 0;
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}
static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("[LCM] %s begin\n", __func__);
	if (ctx->prepared)
		return 0;
	//lcm_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);
	//ctx->prepared = true;
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
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}
	ctx->enabled = true;
	return 0;
}
static const struct drm_display_mode switch_mode_120hz = {
	.clock = (int)((FRAME_WIDTH + MODE_1_HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_1_VFP + VSA + VBP) * MODE_1_FPS / 1000),
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_1_HFP,
	.hsync_end = FRAME_WIDTH + MODE_1_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_1_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
};
static const struct drm_display_mode switch_mode_90hz = {
	.clock = (int)((FRAME_WIDTH + MODE_2_HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_2_VFP + VSA + VBP) * MODE_2_FPS / 1000),
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_2_HFP,
	.hsync_end = FRAME_WIDTH + MODE_2_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_2_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,
};
static const struct drm_display_mode switch_mode_60hz = {
	.clock = (int)((FRAME_WIDTH + MODE_3_HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_3_VFP + VSA + VBP) * MODE_3_FPS / 1000),
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_3_HFP,
	.hsync_end = FRAME_WIDTH + MODE_3_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_3_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_3_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_3_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_3_VFP + VSA + VBP,
};
#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_120hz = {
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.ssc_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x66, .count = 1, .para_list[0] = 0x00,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0x03, .count = 1, .para_list[0] = 0x01,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.vdo_keep_hs_perline = 1,
	//.lp_perline_en = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                   = DSC_ENABLE,
		.ver                      = DSC_VER,
		.slice_mode               = DSC_SLICE_MODE,
		.rgb_swap                 = DSC_RGB_SWAP,
		.dsc_cfg                  = DSC_DSC_CFG,
		.rct_on                   = DSC_RCT_ON,
		.bit_per_channel          = DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth       = DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable                = DSC_BP_ENABLE,
		.bit_per_pixel            = DSC_BIT_PER_PIXEL,
		.pic_height               = FRAME_HEIGHT,
		.pic_width                = FRAME_WIDTH,
		.slice_height             = DSC_SLICE_HEIGHT,
		.slice_width              = DSC_SLICE_WIDTH,
		.chunk_size               = DSC_CHUNK_SIZE,
		.xmit_delay               = DSC_XMIT_DELAY,
		.dec_delay                = DSC_DEC_DELAY,
		.scale_value              = DSC_SCALE_VALUE,
		.increment_interval       = DSC_INCREMENT_INTERVAL,
		.decrement_interval       = DSC_DECREMENT_INTERVAL,
		.line_bpg_offset          = DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset           = DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset         = DSC_SLICE_BPG_OFFSET,
		.initial_offset           = DSC_INITIAL_OFFSET,
		.final_offset             = DSC_FINAL_OFFSET,
		.flatness_minqp           = DSC_FLATNESS_MINQP,
		.flatness_maxqp           = DSC_FLATNESS_MAXQP,
		.rc_model_size            = DSC_RC_MODEL_SIZE,
		.rc_edge_factor           = DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0     = DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1     = DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi         = DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo         = DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 2, {0x6C, 0x01}}, //120Hz
	},
	.data_rate = MODE_1_DATA_RATE,
	.lcm_pcd_check_table[0] = {
		.cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xD4,
	},
	.tran_panel_params = &panel_driver_status,
};
static struct mtk_panel_params ext_params_90hz = {
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.ssc_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x66, .count = 1, .para_list[0] = 0x00,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0x03, .count = 1, .para_list[0] = 0x01,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.vdo_keep_hs_perline = 1,
	//.lp_perline_en = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                   = DSC_ENABLE,
		.ver                      = DSC_VER,
		.slice_mode               = DSC_SLICE_MODE,
		.rgb_swap                 = DSC_RGB_SWAP,
		.dsc_cfg                  = DSC_DSC_CFG,
		.rct_on                   = DSC_RCT_ON,
		.bit_per_channel          = DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth       = DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable                = DSC_BP_ENABLE,
		.bit_per_pixel            = DSC_BIT_PER_PIXEL,
		.pic_height               = FRAME_HEIGHT,
		.pic_width                = FRAME_WIDTH,
		.slice_height             = DSC_SLICE_HEIGHT,
		.slice_width              = DSC_SLICE_WIDTH,
		.chunk_size               = DSC_CHUNK_SIZE,
		.xmit_delay               = DSC_XMIT_DELAY,
		.dec_delay                = DSC_DEC_DELAY,
		.scale_value              = DSC_SCALE_VALUE,
		.increment_interval       = DSC_INCREMENT_INTERVAL,
		.decrement_interval       = DSC_DECREMENT_INTERVAL,
		.line_bpg_offset          = DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset           = DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset         = DSC_SLICE_BPG_OFFSET,
		.initial_offset           = DSC_INITIAL_OFFSET,
		.final_offset             = DSC_FINAL_OFFSET,
		.flatness_minqp           = DSC_FLATNESS_MINQP,
		.flatness_maxqp           = DSC_FLATNESS_MAXQP,
		.rc_model_size            = DSC_RC_MODEL_SIZE,
		.rc_edge_factor           = DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0     = DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1     = DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi         = DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo         = DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 2, {0x6C, 0x02}}, //90Hz
	},
	.data_rate = MODE_2_DATA_RATE,
	.lcm_pcd_check_table[0] = {
		.cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xD4,
	},
	.tran_panel_params = &panel_driver_status,
};
static struct mtk_panel_params ext_params_60hz = {
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.ssc_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x66, .count = 1, .para_list[0] = 0x00,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0x03, .count = 1, .para_list[0] = 0x01,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.vdo_keep_hs_perline = 1,
	//.lp_perline_en = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                   = DSC_ENABLE,
		.ver                      = DSC_VER,
		.slice_mode               = DSC_SLICE_MODE,
		.rgb_swap                 = DSC_RGB_SWAP,
		.dsc_cfg                  = DSC_DSC_CFG,
		.rct_on                   = DSC_RCT_ON,
		.bit_per_channel          = DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth       = DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable                = DSC_BP_ENABLE,
		.bit_per_pixel            = DSC_BIT_PER_PIXEL,
		.pic_height               = FRAME_HEIGHT,
		.pic_width                = FRAME_WIDTH,
		.slice_height             = DSC_SLICE_HEIGHT,
		.slice_width              = DSC_SLICE_WIDTH,
		.chunk_size               = DSC_CHUNK_SIZE,
		.xmit_delay               = DSC_XMIT_DELAY,
		.dec_delay                = DSC_DEC_DELAY,
		.scale_value              = DSC_SCALE_VALUE,
		.increment_interval       = DSC_INCREMENT_INTERVAL,
		.decrement_interval       = DSC_DECREMENT_INTERVAL,
		.line_bpg_offset          = DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset           = DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset         = DSC_SLICE_BPG_OFFSET,
		.initial_offset           = DSC_INITIAL_OFFSET,
		.final_offset             = DSC_FINAL_OFFSET,
		.flatness_minqp           = DSC_FLATNESS_MINQP,
		.flatness_maxqp           = DSC_FLATNESS_MAXQP,
		.rc_model_size            = DSC_RC_MODEL_SIZE,
		.rc_edge_factor           = DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0     = DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1     = DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi         = DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo         = DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
			},
	},
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 2, {0x6C, 0x03}}, //60Hz
	},
	.data_rate = MODE_3_DATA_RATE,
	.lcm_pcd_check_table[0] = {
		.cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xD4,
	},
	.tran_panel_params = &panel_driver_status,
};
//Ajust dim speed 20241212 start
static unsigned int ghbm_state = 0;
static unsigned int dim_speed_change = 0;
static struct LCM_setting_table_v2 dim_frame_speed_v2[] = {
	 //Diming by Frame, set 0x08 frame time for HBM DIMING
	{0x3, {0xF0, 0xAA,0x13}},
	{0x2, {0xC8, 0x08}},
	{0x2, {0xD0, 0x08}},
	{0x2, {0xE0, 0x08}},
	{0x2, {0xE8, 0x08}},
};
static int lcm_setbacklight_cmdq_v2(void *dsi, mtk_dsi_ddic_cmd cb,
	void *handle, unsigned int level, struct mtk_dsi_cmd_option *cmd_opt)
{
	int i;
	struct mipi_dsi_msg bl_tb0_cmd = { 0 };

	level = (level > 5119) ? 5119 : (level < 0) ? 0 : level;

	if (level == 0) {
		mapped_level = 0;
	} else if (level > 0 && level <= 8) {
		mapped_level = 10;
	} else if (level > 8 && level <= 4095) {
		mapped_level = 10 + ((level - 8) * (3515 - 10) / (4095 - 8));
	} else if (level > 4095 && level <= 5119) {
		mapped_level = 3515 + ((level - 4095) * (4095 - 3515) / 1024);
	}

	if (level <= 4095 && ghbm_state) {
		ghbm_state = ghbm_state - 1;
	} else if (4095 < level && level <= 5119) {
		ghbm_state = 2;
	}
	/* need adjust dim speed(0x08 frame) for hbm diming */
	if ((ghbm_state == 2) && (dim_speed_change == 0)) {
		struct mipi_dsi_msg frame_speed_v2_tmp[ARRAY_SIZE(dim_frame_speed_v2)] = { 0 };
		pr_info("%s ghbm_state=%d, dim_speed_change=%d set 0x08.\n", __func__, ghbm_state, dim_speed_change);
		dim_frame_speed_v2[1].para_list[1] = 0x08;
		dim_frame_speed_v2[2].para_list[1] = 0x08;
		dim_frame_speed_v2[3].para_list[1] = 0x08;
		dim_frame_speed_v2[4].para_list[1] = 0x08;

		for (i = 0; i < ARRAY_SIZE(dim_frame_speed_v2); i++) {
			frame_speed_v2_tmp[i].tx_len= dim_frame_speed_v2[i].count;
			frame_speed_v2_tmp[i].tx_buf = dim_frame_speed_v2[i].para_list;
		}
		struct mtk_dsi_cmd_msg frame_speed_v2_msg = {
			.cmd_num = ARRAY_SIZE(dim_frame_speed_v2),
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = frame_speed_v2_tmp,
		};
		cb(dsi, handle, cmd_opt, &frame_speed_v2_msg);
		dim_speed_change = 1;
	}
	/* after levea hbm, recovery dim speed(0x18) for second normal backlight */
	if ((ghbm_state == 0) && (dim_speed_change == 1)) {
		struct mipi_dsi_msg frame_speed_v2_tmp[ARRAY_SIZE(dim_frame_speed_v2)] = { 0 };
		pr_info("%s ghbm_state=%d, dim_speed_change=%d set 0x18.\n", __func__, ghbm_state, dim_speed_change);
		dim_frame_speed_v2[1].para_list[1] = 0x20;
		dim_frame_speed_v2[2].para_list[1] = 0x20;
		dim_frame_speed_v2[3].para_list[1] = 0x20;
		dim_frame_speed_v2[4].para_list[1] = 0x20;

		for (i = 0; i < ARRAY_SIZE(dim_frame_speed_v2); i++) {
			frame_speed_v2_tmp[i].tx_len= dim_frame_speed_v2[i].count;
			frame_speed_v2_tmp[i].tx_buf = dim_frame_speed_v2[i].para_list;
		}
		struct mtk_dsi_cmd_msg frame_speed_v2_msg = {
			.cmd_num = ARRAY_SIZE(dim_frame_speed_v2),
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = frame_speed_v2_tmp,
		};
		cb(dsi, handle, cmd_opt, &frame_speed_v2_msg);
		dim_speed_change = 0;
	}
	bl_tb0[1] = ((mapped_level >> 8) & 0x0f);
	bl_tb0[2] = (mapped_level & 0xff);
	if ((g_dim_state == 0) && (g_need_dim_enable == 1)) {
		struct mipi_dsi_msg bl_dim_cmd = { 0 };
		bl_dim[1] = 0x28;
		bl_dim_cmd.tx_buf = bl_dim;
		bl_dim_cmd.tx_len = 2;
		struct mtk_dsi_cmd_msg bl_dim_msg = {
			.cmd_num = 1,
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = &bl_dim_cmd,
		};
		cb(dsi, handle, cmd_opt, &bl_dim_msg);
		g_dim_state =  1;
	}
	if (g_aod_setbacklight == 1)
		g_aod_setbacklight = 0;
	else
		g_need_dim_enable = 1;

	if (mapped_level == 0 || (panel_driver_status.dimming_status == 0)) {
		struct mipi_dsi_msg bl_dim_cmd = { 0 };
		bl_dim[1] = 0x20;
		bl_dim_cmd.tx_buf = bl_dim;
		bl_dim_cmd.tx_len = 2;
		struct mtk_dsi_cmd_msg bl_dim_msg = {
			.cmd_num = 1,
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = &bl_dim_cmd,
		};
		cb(dsi, handle, cmd_opt, &bl_dim_msg);
		g_dim_state = 0;
		g_need_dim_enable = 0;
	} else {
		last_mapped_level = mapped_level;
	}
	bl_tb0_cmd.tx_buf = bl_tb0;
	bl_tb0_cmd.tx_len = 3;
	struct mtk_dsi_cmd_msg bl_tb0_msg = {
		.cmd_num = 1,
		.transfer_mode = PACKET_HS_MODE,
		.cmd_msg = &bl_tb0_cmd,
	};

	cb(dsi, handle, cmd_opt, &bl_tb0_msg);
	pr_info("[LCM] %s level = %d, mapped_level = %d, dim_state = %d\n", __func__, level, mapped_level, g_dim_state);
	return 0;
}
struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m = NULL;
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
	//struct lcm *ctx = panel_to_lcm(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);
	pr_info("[LCM] %s FPS form %d to %d\n", __func__, g_lcm_fresh_mode, drm_mode_vrefresh(m));
	if (drm_mode_vrefresh(m) == MODE_1_FPS)
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == MODE_2_FPS)
		ext->params = &ext_params_90hz;
	else if (drm_mode_vrefresh(m) == MODE_3_FPS)
		ext->params = &ext_params_60hz;
	else
		ret = 1;
	g_lcm_fresh_mode = drm_mode_vrefresh(m);
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
extern unsigned int jiffies_to_msecs(const unsigned long j);
static unsigned long lcm_unprepare_time = 0;
static unsigned long lcm_prepare_time = 0;
static int panel_ext_lcm_power_set(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret = 0;
	pr_info("[LCM] %s status=%d begin\n", __func__, on);
	if (IS_ERR_OR_NULL(ctx->vci_regulator)) {
		ctx->vci_regulator = devm_regulator_get(ctx->dev, "vci");
		if (IS_ERR_OR_NULL(ctx->vci_regulator)) {
			ret = PTR_ERR(ctx->vci_regulator);
			dev_err(ctx->dev, "<%s,%d> lcm vci get fail!ret[%d].\n", __func__, __LINE__, ret);
			return 0;
		}
	}
	if (on) { //vddi 1.8V --> vci 3V --> dvdd 1.25V
		unsigned long time_diff = 0;
		lcm_prepare_time = jiffies_to_msecs(jiffies);
		time_diff = lcm_prepare_time - lcm_unprepare_time;
		if (time_diff < 120) //more than 120ms(power off->power on)
			mdelay(120 - time_diff);
		//VDDI 1.8V enable
		ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->vddi_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
		mdelay(2);
		//VCI 3.0V enable
		ret = regulator_set_voltage(ctx->vci_regulator, 3000000, 3000000); //set 3.0V
		if (ret < 0)
			dev_err(ctx->dev, "<%s:%d>set lcm vci vol error!ret[%d]\n", __func__, __LINE__, ret);
		ret = regulator_enable(ctx->vci_regulator);
		if (ret < 0)
			dev_err(ctx->dev, "<%s:%d>enable lcm vci error!ret[%d]\n", __func__, __LINE__, ret);
		mdelay(2);
		//DVDD 1.25V enable
		ctx->dvdd_gpio = devm_gpiod_get(ctx->dev, "dvdd", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->dvdd_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
		mdelay(15);
	} else { //dvdd 1.25V --> vci 3V --> vddi 1.8V
		//lcm reset pull low after mipipoweroff
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		mdelay(2);
		//DVDD 1.25V disable
		ctx->dvdd_gpio = devm_gpiod_get(ctx->dev, "dvdd", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->dvdd_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
		mdelay(2);
		//VCI 3.0V disable
		ret = regulator_disable(ctx->vci_regulator);
		if (ret < 0)
			dev_err(ctx->dev, "<%s:%d>disable lcm vci error!ret[%d]\n", __func__, __LINE__, ret);
		if (regulator_is_enabled(ctx->vci_regulator))
			dev_err(ctx->dev, "<%s:%d>after disable, lcm vci still enable!ret[%d]\n", __func__, __LINE__, ret);
		mdelay(2);
		//VDDI 1.8V disable
		ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->vddi_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
		mdelay(2);
		lcm_unprepare_time = jiffies_to_msecs(jiffies);
	}
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}
static int panel_doze_enable(struct drm_panel *panel,
			void *dsi, dcs_write_gce cb, void *handle)
{
	pr_info("[LCM] %s begin\n", __func__);
	g_aod_enable = 1;
	g_aod_setbacklight = 1;
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}
static int panel_doze_disable(struct drm_panel *panel,
			void *dsi, dcs_write_gce cb, void *handle)
{
	pr_info("[LCM] %s begin\n", __func__);
	g_aod_enable = 0;
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
static int panel_set_aod_light_mode_v2(void *dsi_drv, mtk_dsi_ddic_cmd cb,
		void *handle, unsigned int mode, struct mtk_dsi_cmd_option *cmd_opt)
{
	unsigned int aod_mapped_level = 0;
	struct mipi_dsi_msg bl_tb0_cmd = { 0 };

	switch (mode) {
		case 72:
			aod_mapped_level = 0x36;  //10nit
			break;
		case 88:
			aod_mapped_level = 0x44;  //15nit
			break;
		case 378:
			aod_mapped_level = 0x13D;  //60nit
			break;
		default:
			aod_mapped_level = 0x13D;  //60nit
			break;
	}
	pr_info("[LCM] %s level is %u,aod_mapped_level is %d\n", __func__, mode, aod_mapped_level);
	bl_tb0[1] = ((aod_mapped_level >> 8) & 0x0f);
	bl_tb0[2] = (aod_mapped_level & 0xff);
	bl_tb0_cmd.tx_buf = bl_tb0;
	bl_tb0_cmd.tx_len = 3;
	struct mtk_dsi_cmd_msg bl_tb0_msg = {
		.cmd_num = 1,
		.transfer_mode = PACKET_HS_MODE,
		.cmd_msg = &bl_tb0_cmd,
	};
	cb(dsi_drv, handle, cmd_opt, &bl_tb0_msg);
	g_aod_setbacklight = 1;
	mapped_level = aod_mapped_level;
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
	printk("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);
	return 0;
}
static struct LCM_setting_table_v2 lcm_pcd_on_setting_v2[] = {
	{3,{0xF0,0xAA,0x12}},
	{3,{0xD0,0x08,0x02}}, // BIST Enable
};
static struct LCM_setting_table_v2 lcm_pcd_on2_setting_v2[] = {
	{1,{0x11}},
	{3,{0xF0,0xAA,0x12}},
	{2,{0x65,0x04}},
};
static struct LCM_setting_table_v2 lcm_pcd_off_setting_v2[] = {
	{3,{0xF0,0xAA,0x12}},
	{3,{0xD0,0x08,0x00}}, //Disable
};
static int lcm_set_pcd_cmdq_v2(void *dsi_drv, mtk_dsi_ddic_cmd cb, void *handle, bool state, struct mtk_dsi_cmd_option *cmd_opt)
{
	int i;
	struct mipi_dsi_msg pcd_on_setting[ARRAY_SIZE(lcm_pcd_on_setting_v2)] = { 0 };
	struct mipi_dsi_msg pcd_on2_setting[ARRAY_SIZE(lcm_pcd_on2_setting_v2)] = { 0 };
	struct mipi_dsi_msg pcd_off_setting[ARRAY_SIZE(lcm_pcd_off_setting_v2)] = { 0 };

	for (i = 0; i < ARRAY_SIZE(lcm_pcd_on_setting_v2); i++) {
		pcd_on_setting[i].tx_len= lcm_pcd_on_setting_v2[i].count;
		pcd_on_setting[i].tx_buf = lcm_pcd_on_setting_v2[i].para_list;
	}
	for (i = 0; i < ARRAY_SIZE(lcm_pcd_on2_setting_v2); i++) {
		pcd_on2_setting[i].tx_len= lcm_pcd_on2_setting_v2[i].count;
		pcd_on2_setting[i].tx_buf = lcm_pcd_on2_setting_v2[i].para_list;
	}
	for (i = 0; i < ARRAY_SIZE(lcm_pcd_off_setting_v2); i++) {
		pcd_off_setting[i].tx_len= lcm_pcd_off_setting_v2[i].count;
		pcd_off_setting[i].tx_buf = lcm_pcd_off_setting_v2[i].para_list;
	}
	struct mtk_dsi_cmd_msg pcd_on_setting_msg = {
		.cmd_num = ARRAY_SIZE(lcm_pcd_on_setting_v2),
		.transfer_mode = PACKET_HS_MODE,
		.cmd_msg = pcd_on_setting,
	};
	struct mtk_dsi_cmd_msg pcd_on2_setting_msg = {
		.cmd_num = ARRAY_SIZE(lcm_pcd_on2_setting_v2),
		.transfer_mode = PACKET_HS_MODE,
		.cmd_msg = pcd_on2_setting,
	};
	struct mtk_dsi_cmd_msg pcd_off_setting_msg = {
		.cmd_num = ARRAY_SIZE(lcm_pcd_off_setting_v2),
		.transfer_mode = PACKET_HS_MODE,
		.cmd_msg = pcd_off_setting,
	};

	if (state == true) {
		cb(dsi_drv, handle, cmd_opt, &pcd_on_setting_msg);
		cmdq_pkt_sleep(handle, 26000*100, 0x03);
		cb(dsi_drv, handle, cmd_opt, &pcd_on2_setting_msg);
	} else {
		cb(dsi_drv, handle, cmd_opt, &pcd_off_setting_msg);
	}
	pr_info("[LCM] %s state=%d\n", __func__, state);
	return 0;
}
static unsigned int alpha_gain[401][2] = {
	{0x00,0x08},{0x01,0x64},{0x02,0x24},{0x02,0x50},{0x02,0x78},{0x02,0xA4},{0x02,0xC4},{0x02,0xE4},{0x03,0x00},{0x03,0x1C},
	{0x03,0x30},{0x03,0x48},{0x03,0x5C},{0x03,0x74},{0x03,0x90},{0x03,0xA8},{0x03,0xC0},{0x03,0xDC},{0x03,0xF8},{0x04,0x14},
	{0x04,0x38},{0x04,0x54},{0x04,0x74},{0x04,0x84},{0x04,0x98},{0x04,0xA8},{0x04,0xB0},{0x04,0xBC},{0x04,0xD0},{0x04,0xDC},
	{0x04,0xE4},{0x04,0xF8},{0x05,0x04},{0x05,0x14},{0x05,0x2C},{0x05,0x38},{0x05,0x44},{0x05,0x5C},{0x05,0x68},{0x05,0x74},
	{0x05,0x88},{0x05,0x98},{0x05,0xA4},{0x05,0xBC},{0x05,0xC8},{0x05,0xD8},{0x05,0xE8},{0x05,0xF8},{0x06,0x0C},{0x06,0x1C},
	{0x06,0x28},{0x06,0x44},{0x06,0x50},{0x06,0x64},{0x06,0x78},{0x06,0x84},{0x06,0x94},{0x06,0xA8},{0x06,0xBC},{0x06,0xC0},
	{0x06,0xCC},{0x06,0xD4},{0x06,0xDC},{0x06,0xEC},{0x06,0xF0},{0x06,0xF8},{0x07,0x08},{0x07,0x14},{0x07,0x1C},{0x07,0x30},
	{0x07,0x34},{0x07,0x40},{0x07,0x4C},{0x07,0x54},{0x07,0x5C},{0x07,0x68},{0x07,0x74},{0x07,0x80},{0x07,0x88},{0x07,0x90},
	{0x07,0xA0},{0x07,0xA8},{0x07,0xB0},{0x07,0xC0},{0x07,0xC4},{0x07,0xD0},{0x07,0xD8},{0x07,0xE4},{0x07,0xEC},{0x07,0xF4},
	{0x07,0xFC},{0x08,0x08},{0x08,0x14},{0x08,0x1C},{0x08,0x24},{0x08,0x30},{0x08,0x34},{0x08,0x3C},{0x08,0x48},{0x08,0x50},
	{0x08,0x58},{0x08,0x68},{0x08,0x6C},{0x08,0x74},{0x08,0x84},{0x08,0x84},{0x08,0x94},{0x08,0xA0},{0x08,0xA4},{0x08,0xB0},
	{0x08,0xBC},{0x08,0xC4},{0x08,0xD0},{0x08,0xDC},{0x08,0xE0},{0x08,0xEC},{0x08,0xF4},{0x08,0xF8},{0x09,0x08},{0x09,0x14},
	{0x09,0x18},{0x09,0x28},{0x09,0x30},{0x09,0x34},{0x09,0x48},{0x09,0x50},{0x09,0x54},{0x09,0x68},{0x09,0x70},{0x09,0x78},
	{0x09,0x88},{0x09,0x94},{0x09,0x98},{0x09,0xA4},{0x09,0xB0},{0x09,0xB4},{0x09,0xC4},{0x09,0xD0},{0x09,0xD8},{0x09,0xE4},
	{0x09,0xEC},{0x09,0xF0},{0x0A,0x00},{0x0A,0x04},{0x0A,0x10},{0x0A,0x18},{0x0A,0x1C},{0x0A,0x28},{0x0A,0x28},{0x0A,0x30},
	{0x0A,0x38},{0x0A,0x44},{0x0A,0x48},{0x0A,0x54},{0x0A,0x54},{0x0A,0x58},{0x0A,0x64},{0x0A,0x6C},{0x0A,0x74},{0x0A,0x7C},
	{0x0A,0x80},{0x0A,0x84},{0x0A,0x90},{0x0A,0x90},{0x0A,0x98},{0x0A,0xA4},{0x0A,0xA8},{0x0A,0xAC},{0x0A,0xB4},{0x0A,0xB8},
	{0x0A,0xC0},{0x0A,0xC8},{0x0A,0xD0},{0x0A,0xD4},{0x0A,0xDC},{0x0A,0xDC},{0x0A,0xE4},{0x0A,0xE8},{0x0A,0xF0},{0x0A,0xF4},
	{0x0B,0x00},{0x0B,0x04},{0x0B,0x0C},{0x0B,0x10},{0x0B,0x14},{0x0B,0x20},{0x0B,0x24},{0x0B,0x2C},{0x0B,0x34},{0x0B,0x38},
	{0x0B,0x3C},{0x0B,0x44},{0x0B,0x4C},{0x0B,0x54},{0x0B,0x5C},{0x0B,0x60},{0x0B,0x68},{0x0B,0x70},{0x0B,0x78},{0x0B,0x80},
	{0x0B,0x84},{0x0B,0x88},{0x0B,0x94},{0x0B,0x94},{0x0B,0x9C},{0x0B,0xA8},{0x0B,0xAC},{0x0B,0xAC},{0x0B,0xB8},{0x0B,0xBC},
	{0x0B,0xC0},{0x0B,0xC8},{0x0B,0xD0},{0x0B,0xD4},{0x0B,0xDC},{0x0B,0xE0},{0x0B,0xE8},{0x0B,0xEC},{0x0B,0xEC},{0x0B,0xFC},
	{0x0C,0x00},{0x0C,0x04},{0x0C,0x0C},{0x0C,0x10},{0x0C,0x14},{0x0C,0x20},{0x0C,0x24},{0x0C,0x28},{0x0C,0x30},{0x0C,0x34},
	{0x0C,0x3C},{0x0C,0x44},{0x0C,0x4C},{0x0C,0x50},{0x0C,0x54},{0x0C,0x58},{0x0C,0x5C},{0x0C,0x68},{0x0C,0x6C},{0x0C,0x78},
	{0x0C,0x7C},{0x0C,0x80},{0x0C,0x88},{0x0C,0x8C},{0x0C,0x90},{0x0C,0x98},{0x0C,0xA0},{0x0C,0xA4},{0x0C,0xA8},{0x0C,0xAC},
	{0x0C,0xB0},{0x0C,0xBC},{0x0C,0xC0},{0x0C,0xC4},{0x0C,0xCC},{0x0C,0xCC},{0x0C,0xD0},{0x0C,0xD8},{0x0C,0xDC},{0x0C,0xE4},
	{0x0C,0xEC},{0x0C,0xF0},{0x0C,0xF8},{0x0C,0xFC},{0x0D,0x04},{0x0D,0x0C},{0x0D,0x14},{0x0D,0x18},{0x0D,0x1C},{0x0D,0x20},
	{0x0D,0x24},{0x0D,0x2C},{0x0D,0x38},{0x0D,0x38},{0x0D,0x40},{0x0D,0x44},{0x0D,0x48},{0x0D,0x54},{0x0D,0x54},{0x0D,0x60},
	{0x0D,0x68},{0x0D,0x6C},{0x0D,0x6C},{0x0D,0x74},{0x0D,0x80},{0x0D,0x84},{0x0D,0x8C},{0x0D,0x94},{0x0D,0x98},{0x0D,0x9C},
	{0x0D,0x9C},{0x0D,0xA8},{0x0D,0xA8},{0x0D,0xB4},{0x0D,0xB8},{0x0D,0xBC},{0x0D,0xC0},{0x0D,0xCC},{0x0D,0xCC},{0x0D,0xD8},
	{0x0D,0xDC},{0x0D,0xE0},{0x0D,0xE0},{0x0D,0xE8},{0x0D,0xE8},{0x0D,0xEC},{0x0D,0xF4},{0x0D,0xFC},{0x0E,0x08},{0x0E,0x08},
	{0x0E,0x10},{0x0E,0x14},{0x0E,0x14},{0x0E,0x20},{0x0E,0x28},{0x0E,0x2C},{0x0E,0x30},{0x0E,0x34},{0x0E,0x38},{0x0E,0x3C},
	{0x0E,0x48},{0x0E,0x50},{0x0E,0x54},{0x0E,0x58},{0x0E,0x5C},{0x0E,0x60},{0x0E,0x68},{0x0E,0x6C},{0x0E,0x70},{0x0E,0x78},
	{0x0E,0x80},{0x0E,0x88},{0x0E,0x88},{0x0E,0x90},{0x0E,0x98},{0x0E,0x9C},{0x0E,0xA0},{0x0E,0xA8},{0x0E,0xAC},{0x0E,0xAC},
	{0x0E,0xB8},{0x0E,0xB8},{0x0E,0xBC},{0x0E,0xC4},{0x0E,0xC8},{0x0E,0xCC},{0x0E,0xD4},{0x0E,0xDC},{0x0E,0xDC},{0x0E,0xE4},
	{0x0E,0xE4},{0x0E,0xE8},{0x0E,0xF0},{0x0E,0xF4},{0x0E,0xFC},{0x0F,0x00},{0x0F,0x04},{0x0F,0x0C},{0x0F,0x10},{0x0F,0x10},
	{0x0F,0x20},{0x0F,0x24},{0x0F,0x28},{0x0F,0x2C},{0x0F,0x30},{0x0F,0x34},{0x0F,0x38},{0x0F,0x44},{0x0F,0x48},{0x0F,0x4C},
	{0x0F,0x4C},{0x0F,0x50},{0x0F,0x5C},{0x0F,0x5C},{0x0F,0x68},{0x0F,0x70},{0x0F,0x70},{0x0F,0x74},{0x0F,0x78},{0x0F,0x7C},
	{0x0F,0x8C},{0x0F,0x8C},{0x0F,0x90},{0x0F,0x94},{0x0F,0x9C},{0x0F,0xA0},{0x0F,0xA8},{0x0F,0xB0},{0x0F,0xB4},{0x0F,0xC0},
	{0x0F,0xC4},{0x0F,0xC4},{0x0F,0xC8},{0x0F,0xD4},{0x0F,0xD8},{0x0F,0xE0},{0x0F,0xE4},{0x0F,0xE8},{0x0F,0xF0},{0x0F,0xF4},
	{0x10,0x00},
};
static struct LCM_setting_table_v2 hbm_mode_enter_setting_v2[] = {
	{3,{0xF0,0xAA,0x13}},
	{2,{0xC6,0x01}},
	{5,{0x63,0x10,0x00,0x0D,0xBB}},
	{2,{0x62,0x03}},
};
static struct LCM_setting_table_v2 hbm_mode_exit_setting_v2[] = {
//Alpha mode control
	{2,{0x62,0x00}},
};
static int panel_hbm_set_cmdq_v2(struct drm_panel *panel, void *dsi_drv,
			    mtk_dsi_ddic_cmd cb, void *handle, bool en,
			    struct mtk_dsi_cmd_option *cmd_opt)
{
	int i;
	struct lcm *ctx = panel_to_lcm(panel);
	if (!cb)
		return -1;
	if (ctx->hbm_en == en)
		goto done;
	pr_info("[LCM] %s FPS=%dHz en=%d, mapped_level=%d, aod=%d \n", __func__, g_lcm_fresh_mode, en, mapped_level, g_aod_enable);
    if(en) {
		struct mipi_dsi_msg enter_setting_v2[ARRAY_SIZE(hbm_mode_enter_setting_v2)] = { 0 };
		if (mapped_level <= 0x190) {
			hbm_mode_enter_setting_v2[2].para_list[1] = alpha_gain[mapped_level][0];
			//dynamic selection from alpha_gain table
			hbm_mode_enter_setting_v2[2].para_list[2] = alpha_gain[mapped_level][1];
			hbm_mode_enter_setting_v2[2].para_list[3] = 0x01;
			hbm_mode_enter_setting_v2[2].para_list[4] = 0x91;
		} else {
			hbm_mode_enter_setting_v2[2].para_list[1] = 0x10;
			hbm_mode_enter_setting_v2[2].para_list[2] = 0x00;
			hbm_mode_enter_setting_v2[2].para_list[3] = ((mapped_level >> 8) & 0x0f);
			hbm_mode_enter_setting_v2[2].para_list[4] = (mapped_level & 0xff);
		}
		for (i = 0; i < ARRAY_SIZE(hbm_mode_enter_setting_v2); i++) {
			enter_setting_v2[i].tx_len= hbm_mode_enter_setting_v2[i].count;
			enter_setting_v2[i].tx_buf = hbm_mode_enter_setting_v2[i].para_list;
		}
		struct mtk_dsi_cmd_msg enter_setting_v2_msg = {
			.cmd_num = ARRAY_SIZE(hbm_mode_enter_setting_v2),
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = enter_setting_v2,
		};
		cb(dsi_drv, handle, cmd_opt, &enter_setting_v2_msg);
	}else {
		struct mipi_dsi_msg exit_setting_v2[ARRAY_SIZE(hbm_mode_exit_setting_v2)] = { 0 };
		for (i = 0; i < ARRAY_SIZE(hbm_mode_exit_setting_v2); i++) {
			exit_setting_v2[i].tx_len= hbm_mode_exit_setting_v2[i].count;
			exit_setting_v2[i].tx_buf = hbm_mode_exit_setting_v2[i].para_list;
		}
		struct mtk_dsi_cmd_msg exit_setting_v2_msg = {
			.cmd_num = ARRAY_SIZE(hbm_mode_exit_setting_v2),
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = exit_setting_v2,
		};
		cb(dsi_drv, handle, cmd_opt, &exit_setting_v2_msg);
    }
	ctx->hbm_en = en;
	ctx->hbm_wait = true;
	g_hbm_enable = ctx->hbm_en;
done:
	return 0;
}
static int panel_hbm_set_cmdq_switch_v2(struct drm_panel *panel, void *dsi_drv,
			    mtk_dsi_ddic_cmd cb, void *handle, bool en,
			    struct mtk_dsi_cmd_option *cmd_opt)
{
	int i;
	if (!cb)
		return -1;
	pr_info("[LCM] %s FPS=%dHz en=%d, mapped_level=%d, aod=%d \n", __func__, g_lcm_fresh_mode, en, mapped_level, g_aod_enable);
    if(en) {
		struct mipi_dsi_msg enter_setting_v2[ARRAY_SIZE(hbm_mode_enter_setting_v2)] = { 0 };
		if (mapped_level <= 0x190) {
			hbm_mode_enter_setting_v2[2].para_list[1] = alpha_gain[mapped_level][0];
			//dynamic selection from alpha_gain table
			hbm_mode_enter_setting_v2[2].para_list[2] = alpha_gain[mapped_level][1];
			hbm_mode_enter_setting_v2[2].para_list[3] = 0x01;
			hbm_mode_enter_setting_v2[2].para_list[4] = 0x91;
		} else {
			hbm_mode_enter_setting_v2[2].para_list[1] = 0x10;
			hbm_mode_enter_setting_v2[2].para_list[2] = 0x00;
			hbm_mode_enter_setting_v2[2].para_list[3] = ((mapped_level >> 8) & 0x0f);
			hbm_mode_enter_setting_v2[2].para_list[4] = (mapped_level & 0xff);
		}
		for (i = 0; i < ARRAY_SIZE(hbm_mode_enter_setting_v2); i++) {
			enter_setting_v2[i].tx_len= hbm_mode_enter_setting_v2[i].count;
			enter_setting_v2[i].tx_buf = hbm_mode_enter_setting_v2[i].para_list;
		}
		struct mtk_dsi_cmd_msg enter_setting_v2_msg = {
			.cmd_num = ARRAY_SIZE(hbm_mode_enter_setting_v2),
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = enter_setting_v2,
		};
		cb(dsi_drv, handle, cmd_opt, &enter_setting_v2_msg);
	}else {
		struct mipi_dsi_msg exit_setting_v2[ARRAY_SIZE(hbm_mode_exit_setting_v2)] = { 0 };
		for (i = 0; i < ARRAY_SIZE(hbm_mode_exit_setting_v2); i++) {
			exit_setting_v2[i].tx_len= hbm_mode_exit_setting_v2[i].count;
			exit_setting_v2[i].tx_buf = hbm_mode_exit_setting_v2[i].para_list;
		}
		struct mtk_dsi_cmd_msg exit_setting_v2_msg = {
			.cmd_num = ARRAY_SIZE(hbm_mode_exit_setting_v2),
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = exit_setting_v2,
		};
		cb(dsi_drv, handle, cmd_opt, &exit_setting_v2_msg);
    }
	g_hbm_enable = en;
	return 0;
}
static void panel_hbm_get_state(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);
	*state = ctx->hbm_en;
}
static void panel_hbm_get_wait_state(struct drm_panel *panel, bool *wait)
{
	struct lcm *ctx = panel_to_lcm(panel);
	*wait = ctx->hbm_wait;
}
static bool panel_hbm_set_wait_state(struct drm_panel *panel, bool wait)
{
	struct lcm *ctx = panel_to_lcm(panel);
	bool old = ctx->hbm_wait;
	ctx->hbm_wait = wait;
	return old;
}
static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq_v2 = lcm_setbacklight_cmdq_v2,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	.set_pcd_cmdq_v2 = lcm_set_pcd_cmdq_v2,
	//.mode_switch = mode_switch,
	.reset = panel_ext_reset,
	.lcm_power_set = panel_ext_lcm_power_set,
	/* add for ramless HBM */
	.hbm_set_cmdq_v2 = panel_hbm_set_cmdq_v2,
	.hbm_set_cmdq_switch_v2 = panel_hbm_set_cmdq_switch_v2,
	.hbm_get_state = panel_hbm_get_state,
	.hbm_get_wait_state = panel_hbm_get_wait_state,
	.hbm_set_wait_state = panel_hbm_set_wait_state,
	/* add for ramless AOD */
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
	.set_aod_light_mode_v2 = panel_set_aod_light_mode_v2,
#endif
	.panel_init_v2 = lcm_panel_init_v2,
	.panel_deinit_v2 = lcm_panel_deinit_v2,
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
	struct drm_display_mode *mode_60hz;
	struct drm_display_mode *mode_90hz;
	struct drm_display_mode *mode_120hz;
	pr_info("[LCM] %s begin\n", __func__);
	mode_120hz = drm_mode_duplicate(connector->dev, &switch_mode_120hz);
	if (!mode_120hz) {
		dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_120hz.hdisplay,
			switch_mode_120hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_120hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_120hz);
	mode_120hz->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_120hz);
	mode_90hz = drm_mode_duplicate(connector->dev, &switch_mode_90hz);
	if (!mode_90hz) {
		dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_90hz.hdisplay,
			switch_mode_90hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_90hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_90hz);
	mode_90hz->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_90hz);
	mode_60hz = drm_mode_duplicate(connector->dev, &switch_mode_60hz);
	if (!mode_60hz) {
		dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_60hz.hdisplay,
			switch_mode_60hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_60hz);
	mode_60hz->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_60hz);
	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 158;
	pr_info("[LCM] %s end\n", __func__);
	return 3;
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
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
	unsigned int doze_backlight[] = {0, 0, 0, 0};
	#endif
	pr_info("[LCM] %s begin\n", __func__);
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
	ret = of_property_read_u32_array(remote_node, "doze_backlight", doze_backlight, ARRAY_SIZE(doze_backlight));
	if(!ret)
	{
		g_lcm_doze_backlight.doze_backlight_num = doze_backlight[0];
		g_lcm_doze_backlight.doze_backlight_level1 = doze_backlight[1];
		g_lcm_doze_backlight.doze_backlight_level2 = doze_backlight[2];
		g_lcm_doze_backlight.doze_backlight_level3 = doze_backlight[3];
	}
	panel_driver_status.lcm_doze_backlight = &g_lcm_doze_backlight;
	pr_info("[LCM] %s lcm_doze_backlight:%d;%d;%d;%d;\n", __func__, g_lcm_doze_backlight.doze_backlight_num, g_lcm_doze_backlight.doze_backlight_level1, g_lcm_doze_backlight.doze_backlight_level2, g_lcm_doze_backlight.doze_backlight_level3);
	#endif
	panel_driver_status.dimming_status = 1;  // default dimming status on
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			MIPI_DSI_MODE_NO_EOT_PACKET;
			//| MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS;  //continue clk, Keep HS
	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);
		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}
	ctx->vddi_gpio = devm_gpiod_get(dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(dev, "cannot get vddi-gpios %ld\n", PTR_ERR(ctx->vddi_gpio));
		return PTR_ERR(ctx->vddi_gpio);
	}
	devm_gpiod_put(dev, ctx->vddi_gpio);
	ctx->vci_regulator = devm_regulator_get(ctx->dev, "vci");
	if (IS_ERR(ctx->vci_regulator)) {
		ret = PTR_ERR(ctx->vci_regulator);
		dev_err(ctx->dev, "cannot get vci regulator ret[%d]\n",ret);
		return ret;
	}
	ret = regulator_set_voltage(ctx->vci_regulator, 3000000, 3000000); //set 3.0V
	if (ret < 0)
		dev_err(ctx->dev, "<%s:%d>set lcm vci vol error!ret[%d]\n", __func__, __LINE__, ret);
	ret = regulator_enable(ctx->vci_regulator);
	if (ret < 0)
		dev_err(ctx->dev, "<%s:%d>enable vci vol error!ret[%d]\n", __func__, __LINE__, ret);
	ctx->dvdd_gpio = devm_gpiod_get(dev, "dvdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(dev, "cannot get dvdd-gpios %ld\n", PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	devm_gpiod_put(dev, ctx->dvdd_gpio);
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n", PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
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
	ret = mtk_panel_ext_create(dev, &ext_params_120hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	pr_info("[LCM] %s end\n", __func__);
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
	return ;
}
//Fix not repeated under 28 10 and after reset pull low,then pull high's abnormal hehavior 20241215 start
static void lcm_shutdown(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
	pr_info("[LCM] %s ctx->prepared=%d, begin\n", __func__, ctx->prepared);
	if (ctx->prepared) {
		lcm_dcs_write_seq_static(ctx, 0xF0,0xAA,0x10); //add by 20250218
		lcm_dcs_write_seq_static(ctx, 0x65,0x01);
		lcm_dcs_write_seq_static(ctx, 0xD0,0x68); //Ramless AOD: 0x68 close swire dimming; DDIC AOD: 0x66 close swire dimming
		lcm_dcs_write_seq_static(ctx, 0x53,0x20);
		mdelay(18);
		lcm_dcs_write_seq_static(ctx, 0x51,0x00,0x00);
		mdelay(18);
	}
}
static const struct of_device_id lcm_of_match[] = {
	{
		.compatible = "vtdr6126a,fhdp,dsi,vdo,dsc,tm,tm,120hz,x6898",
	},
	{} };
MODULE_DEVICE_TABLE(of, lcm_of_match);
static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.shutdown = lcm_shutdown,
	.driver = {
			.name = "vtdr6126a_fhdp_dsi_vdo_dsc_tm_tm_120hz_x6898",
			.owner = THIS_MODULE,
			.of_match_table = lcm_of_match,
		},
};
module_mipi_dsi_driver(lcm_driver);
MODULE_AUTHOR("bing.he <bing.he@transsion.com>");
MODULE_DESCRIPTION("VTDR6126A VDO 1.5K+120HZ Ramless AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
