/*
 * Copyright (c) 2025 MediaTek Inc.
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
#define VBP                         46
#define HSA                         4
#define HBP                         18
/*Parameter setting for 120Hz Start*/
#define MODE_1_FPS                  120
#define MODE_1_VFP                  80
#define MODE_1_HFP                  124
#define MODE_1_DATA_RATE            1096
/*Parameter setting for 120Hz End*/
/*Parameter setting for 90Hz Start*/
#define MODE_2_FPS                  90
#define MODE_2_VFP                  1004
#define MODE_2_HFP                  124
#define MODE_2_DATA_RATE            1096
/*Parameter setting for 90Hz End*/
/*Parameter setting for 60Hz Start*/
#define MODE_3_FPS                  60
#define MODE_3_VFP                  2852
#define MODE_3_HFP                  124
#define MODE_3_DATA_RATE            1096
/*Parameter setting for 60Hz End*/
/*Parameter setting for 30Hz Start*/
#define MODE_4_FPS                  30
#define MODE_4_VFP                  80
#define MODE_4_HFP                  1768
#define MODE_4_DATA_RATE            1096
/*Parameter setting for 30Hz End*/
/* DSC RELATED */
#define DSC_ENABLE					1
#define DSC_VER						18
#define DSC_SLICE_MODE				1
#define DSC_RGB_SWAP				0
#define DSC_DSC_CFG					40
#define DSC_RCT_ON					1
#define DSC_BIT_PER_CHANNEL         10  //8bit: 8, 10bit: 10
#define DSC_DSC_LINE_BUF_DEPTH      11 //8bit: 9, 10bit: 11
#define DSC_BP_ENABLE				1
#define DSC_BIT_PER_PIXEL			128
#define DSC_SLICE_HEIGHT			4
#define DSC_SLICE_WIDTH				604
#define DSC_CHUNK_SIZE              604
#define DSC_XMIT_DELAY				512
#define DSC_DEC_DELAY               559
#define DSC_SCALE_VALUE				32
#define DSC_INCREMENT_INTERVAL      90
#define DSC_DECREMENT_INTERVAL      8
#define DSC_LINE_BPG_OFFSET			12
#define DSC_NFL_BPG_OFFSET          8192
#define DSC_SLICE_BPG_OFFSET        5759
#define DSC_INITIAL_OFFSET			6144
#define DSC_FINAL_OFFSET			4320
#define DSC_FLATNESS_MINQP			7
#define DSC_FLATNESS_MAXQP			16
#define DSC_RC_MODEL_SIZE			8192
#define DSC_RC_EDGE_FACTOR			6
#define DSC_RC_QUANT_INCR_LIMIT0	15
#define DSC_RC_QUANT_INCR_LIMIT1	15
#define DSC_RC_TGT_OFFSET_HI		3
#define DSC_RC_TGT_OFFSET_LO		3

static unsigned int rc_buf_thresh[14] = {896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064};
static unsigned int range_min_qp[15] = {0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16};
static unsigned int range_max_qp[15] = {8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 14, 15, 15, 16, 17};
static int range_bpg_ofs[15] = {2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12};
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
	struct regulator *vci_regulator;
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
	{2,{0x2F,0x01}},	//120hz
	{2,{0x8F,0x00}},
	{5,{0x3B,0x00,0x30,0x00,0x50}},
	{2,{0x6F,0x04}},
	{5,{0x3B,0x00,0x30,0x00,0x50}},
	{2,{0x6F,0x08}},
	{5,{0x3B,0x00,0x30,0x0B,0x24}},
	{2,{0x6F,0x0C}},
	{5,{0x3B,0x00,0x30,0x20,0xCC}},
	{2,{0x6F,0x10}},
	{5,{0x3B,0x00,0x30,0x00,0x50}},

	{6,{0xF0,0x55,0xAA,0x52,0x08,0x01}},
	{2,{0x6F,0x04}},
	{2,{0xC3,0xFF}},
	{2,{0x6F,0x09}},
	{2,{0xC3,0xFF}},
	{2,{0xEA,0xC0}},
	{2,{0x6F,0x11}},
	{2,{0xEA,0xC0}},
	{2,{0x6F,0x07}},
	{11,{0xEA,0x01,0x0E,0x01,0x43,0x01,0x43,0x01,0x43,0x05,0x0E}},
	{2,{0x6F,0x18}},
	{11,{0xEA,0x01,0x79,0x01,0xC5,0x01,0xC5,0x01,0xC5,0x07,0x13}},
	//flash lock
	{6,{0xF0,0x55,0xAA,0x52,0x08,0x04}},
	{2,{0x6F,0x01}},
	{2,{0xC7,0x11}},
	{2,{0x6F,0x04}},
	{5,{0xC7,0x1C,0x02,0x1C,0x02}},

	{5,{0xFF,0xAA,0x55,0xA5,0x80}},
	{2,{0x6F,0x47}},
	{2,{0xF2,0x21}},
	{5,{0xFF,0xAA,0x55,0xA5,0x80}},
	{2,{0x6F,0x1F}},
	{2,{0xF4,0x0B}},
	{2,{0x6F,0x20}},
	{2,{0xF4,0x3F}},
	{2,{0x6F,0x08}},
	{2,{0xFC,0x03}},
	{5,{0xFF,0xAA,0x55,0xA5,0x80}},
	{2,{0x6F,0x24}},
	{2,{0xF8,0xFF}},
	{2,{0x6F,0x08}},
	{2,{0xF6,0x44}},
	{2,{0x6F,0x0A}},
	{7,{0xF6,0x60,0x60,0x60,0x60,0x60,0x30}},
	{5,{0xFF,0xAA,0x55,0xA5,0x84}},
	{2,{0x6F,0x10}},
	{2,{0xF8,0x02}},
	{3,{0x81,0x03,0x28}},	//90Hz Gamma offset
	{5,{0xFF,0xAA,0x55,0xA5,0x84}},
	{2,{0xF2,0x15}},
	{5,{0xFF,0xAA,0x55,0xA5,0x84}},
	{2,{0x6F,0x27}},
	{2,{0xF7,0x00}},
	{5,{0xFF,0xAA,0x55,0xA5,0x84}},
	{2,{0x6F,0xA9}},
	{2,{0xF4,0xF3}},
	{2,{0x17,0x03}},
	//1208*2644 vesa1.2 10bit scaling off
	{5,{0x2A,0x00,0x00,0x04,0xB7}},
	{5,{0x2B,0x00,0x00,0x0A,0x53}},
	{3,{0x90,0x03,0x43}},
	{19,{0x91,0xAB,0xA8,0x00,0x04,0xC2,0x00,0x02,0x2F,0x00,0x5A,0x00,0x08,0x20,0x00,0x16,0x7F,0x10,0xE0}},
	{2,{0x53,0x20}},
	{2,{0x88,0x01}},
	{2,{0x35,0x00}},
	{3,{0x5F,0x00,0x00}},
	{3,{0x51,0x00,0x00}},
	{2,{0x6F,0x04}},
	{3,{0x51,0x00,0x00}},
	{2,{0xA2,0x01}},	//A2EN
	//bl dimming
	{6,{0xF0,0x55,0xAA,0x52,0x08,0x00}},
	{2,{0xB2,0x99}},
	{2,{0x6F,0x02}},
	{3,{0xB2,0x20, 0x20}},
	//LVD on
	{6,{0xF0,0x55,0xAA,0x52,0x08,0x01}},
	{2,{0x6F,0x03}},
	{2,{0xC7,0xC7}},
	//aod flush black --251121
	{6,{0xF0,0x55,0xAA,0x52,0x08,0x00}},
	{2,{0xC0,0x40}},
	{2,{0xB2,0x99}},

	//强制重启闪绿 --251202
	{6,{0xF0,0x55,0xAA,0x52,0x08,0x05}},
	{2,{0xC9,0x11}},
	{2,{0xCA,0x11}},
	{6,{0xCB,0x00,0x00,0x00,0x00,0x00}},
	{2,{0xD8,0x11}},
	{6,{0xF0,0x55,0xAA,0x52,0x08,0x01}},
	{2,{0x6F,0x03}},
	{7,{0xC7,0xC7,0x00,0x27,0x08,0x02,0x05}},

	{1,{0x11}},
	{0,{120}},
	{1,{0x29}},
	{0,{20}},
};
static struct LCM_setting_table_v2 lcm_suspend_setting_v2[] = {
	// Display off sequence
	{1,{0x28}},
	//{0,{20}},	//remove 20251107
	// Sleep Mode On
	{1,{0x10}},
	{0,{100}},
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
	mdelay(2);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(2);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(12); ////DDIC spec need more 10ms

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
			.transfer_mode = PACKET_LP_MODE,
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
		.transfer_mode = PACKET_LP_MODE,
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
static const struct drm_display_mode switch_mode_30hz = {
	.clock = (int)((FRAME_WIDTH + MODE_4_HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_4_VFP + VSA + VBP) * MODE_4_FPS / 1000),
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_4_HFP,
	.hsync_end = FRAME_WIDTH + MODE_4_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_4_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_4_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_4_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_4_VFP + VSA + VBP,
};
#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_120hz = {
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.ssc_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
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
		.dfps_cmd_table[0] = {0, 1, {0x38}},
		.dfps_cmd_table[1] = {0, 26, {0xA9,0x02,0x09,0xB0,0x00,0x00,0x00,0x01,0x00,0x2F,0x00,0x00,0x01,0x02,0x08,0xBF,0x01,0x01,0x88,0x02,0x00,0xDF,0x00,0x01,0x00,0x54}}, //120Hz
	},
	.data_rate = MODE_1_DATA_RATE,
	.lcm_pcd_check_table[0] = {
		.cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xBC,
	},
	.tran_panel_params = &panel_driver_status,
};
static struct mtk_panel_params ext_params_90hz = {
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.ssc_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
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
		.dfps_cmd_table[0] = {0, 1, {0x38}},
		.dfps_cmd_table[1] = {0, 19, {0xA9,0x02,0x09,0xB0,0x00,0x00,0x01,0x01,0x00,0x2F,0x00,0x00,0x02,0x02,0x08,0xBF,0x01,0x01,0x88}}, //90Hz

	},
	.data_rate = MODE_2_DATA_RATE,
	.lcm_pcd_check_table[0] = {
		.cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xBC,
	},
	.tran_panel_params = &panel_driver_status,
};
static struct mtk_panel_params ext_params_60hz = {
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.ssc_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
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
		.dfps_cmd_table[0] = {0, 1, {0x38}},
		.dfps_cmd_table[1] = {0, 19, {0xA9,0x02,0x09,0xB0,0x00,0x00,0x01,0x01,0x00,0x2F,0x00,0x00,0x03,0x02,0x08,0xBF,0x01,0x01,0x88}}, //60Hz
	},
	.data_rate = MODE_3_DATA_RATE,
	.lcm_pcd_check_table[0] = {
		.cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xBC,
	},
	.tran_panel_params = &panel_driver_status,
};
static struct mtk_panel_params ext_params_30hz = {
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.ssc_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
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
		.dfps_cmd_table[0] = {0, 1, {0x39}}, //30Hz
	},
	.data_rate = MODE_4_DATA_RATE,
	.lcm_pcd_check_table[0] = {
		.cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xBC,
	},
	.tran_panel_params = &panel_driver_status,
};
static int lcm_setbacklight_cmdq_v2(void *dsi, mtk_dsi_ddic_cmd cb,
	void *handle, unsigned int level, struct mtk_dsi_cmd_option *cmd_opt)
{
	struct mipi_dsi_msg bl_tb0_cmd = { 0 };

	if (level > 5119)
		level = 5119;
	if(level <= 4095) {
		mapped_level = level * 3515 / 4095;
	} else if (4095 < level && level <= 5119) {
		mapped_level = 3515 + (level - 4095) * (4095-3515) / 1024;
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
	if (g_aod_setbacklight == 1) {
		g_aod_setbacklight = 0;
	} else {
		g_need_dim_enable = 1;
	}
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
	else if (drm_mode_vrefresh(m) == MODE_4_FPS)
		ext->params = &ext_params_30hz;
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
	if (on) {
		unsigned long time_diff = 0;
		lcm_prepare_time = jiffies_to_msecs(jiffies);
		time_diff = lcm_prepare_time - lcm_unprepare_time;
		if (time_diff < 120)
			mdelay(120 - time_diff);
		//VDDI 1.8V enable
		ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->vddi_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
		mdelay(2);
		//DVDD 1.25V enable
		ctx->dvdd_gpio = devm_gpiod_get(ctx->dev, "dvdd", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->dvdd_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
		mdelay(2);
		//VCI 3.0V enable
		ret = regulator_set_voltage(ctx->vci_regulator, 3000000, 3000000);
		if (ret < 0)
			dev_err(ctx->dev, "<%s:%d>set lcm vci vol error!ret[%d]\n", __func__, __LINE__, ret);
		ret = regulator_enable(ctx->vci_regulator);
		if (ret < 0)
			dev_err(ctx->dev, "<%s:%d>enable lcm vci error!ret[%d]\n", __func__, __LINE__, ret);
		mdelay(10);
	} else {
		//lcm reset pull low after mipipoweroff
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		mdelay(2);

		//VCI 3.0V disable
		ret = regulator_disable(ctx->vci_regulator);
		if (ret < 0)
			dev_err(ctx->dev, "<%s:%d>disable lcm vci error!ret[%d]\n", __func__, __LINE__, ret);
		if (regulator_is_enabled(ctx->vci_regulator))
			dev_err(ctx->dev, "<%s:%d>after disable, lcm vci still enable!ret[%d]\n", __func__, __LINE__, ret);
		mdelay(2);
		//DVDD 1.25V disable
		ctx->dvdd_gpio = devm_gpiod_get(ctx->dev, "dvdd", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->dvdd_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
		msleep(150);
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
	// push_table_cb(dsi, cb, handle, aod_mode_enter_setting,
	// 	sizeof(aod_mode_enter_setting) / sizeof(struct LCM_setting_table));
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
static struct LCM_setting_table_v2 aod_bl_v2[] = {
	{3,{0x51,0x01,0x3D}},
	{2,{0x6F,0x04}},
	{3,{0x51,0x0F,0xFE}},
};
static int panel_set_aod_light_mode_v2(void *dsi_drv, mtk_dsi_ddic_cmd cb,
		void *handle, unsigned int mode, struct mtk_dsi_cmd_option *cmd_opt)
{
	struct mipi_dsi_msg aod_bl_cmd[ARRAY_SIZE(aod_bl_v2)] = { 0 };
	int i;
	switch (mode) {
		case 56:
			aod_bl_v2[0].para_list[1] = 0x00;  //10nit
			aod_bl_v2[0].para_list[2] = 0x29;  //10nit
			aod_bl_v2[2].para_list[1] = 0x00;  //10nit
			aod_bl_v2[2].para_list[2] = 0x05;  //10nit
			break;
		case 88:
			aod_bl_v2[0].para_list[1] = 0x00;  //15nit
			aod_bl_v2[0].para_list[2] = 0x44;  //15nit
			aod_bl_v2[2].para_list[1] = 0x07;  //15nit
			aod_bl_v2[2].para_list[2] = 0x48;  //15nit
			break;
		case 378:
			aod_bl_v2[0].para_list[1] = 0x01;  //60nit
			aod_bl_v2[0].para_list[2] = 0x3D;  //60nit
			aod_bl_v2[2].para_list[1] = 0x0F;  //60nit
			aod_bl_v2[2].para_list[2] = 0xFE;  //60nit
			break;
		default:
			aod_bl_v2[0].para_list[1] = 0x01;  //60nit
			aod_bl_v2[0].para_list[2] = 0x3D;  //60nit
			aod_bl_v2[2].para_list[1] = 0x0F;  //60nit
			aod_bl_v2[2].para_list[2] = 0xFE;  //60nit
			break;
	}

	for (i = 0; i < ARRAY_SIZE(aod_bl_v2); i++) {
		aod_bl_cmd[i].tx_len = aod_bl_v2[i].count;
		aod_bl_cmd[i].tx_buf = aod_bl_v2[i].para_list;
	}
	struct mtk_dsi_cmd_msg aod_bl_msg = {
		.cmd_num = ARRAY_SIZE(aod_bl_v2),
		.transfer_mode = PACKET_HS_MODE,
		.cmd_msg = aod_bl_cmd,
	};
	pr_info("[LCM] %s level is %u,FPS is %d\n", __func__, mode, g_lcm_fresh_mode);
	cb(dsi_drv, handle, cmd_opt, &aod_bl_msg);

	g_aod_setbacklight = 1;
	mapped_level = mode;
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
		{6,{0xF0,0x55,0xAA,0x52,0x08,0x01}},
		{3,{0xD0,0x00,0x80}},
		{1,{0x11}},
};
static struct LCM_setting_table_v2 lcm_pcd_on2_setting_v2[] = {
		{1,{0x29}},
};
static struct LCM_setting_table_v2 lcm_pcd_on3_setting_v2[] = {
		{6,{0xF0,0x55,0xAA,0x52,0x08,0x01}},
};
static struct LCM_setting_table_v2 lcm_pcd_off_setting_v2[] = {
		{6,{0xF0,0x55,0xAA,0x52,0x08,0x01}},
		{3,{0xD0,0x00,0x00}},
};
static int lcm_set_pcd_cmdq_v2(void *dsi_drv, mtk_dsi_ddic_cmd cb, void *handle, bool state, struct mtk_dsi_cmd_option *cmd_opt)
{
	int i;
	struct mipi_dsi_msg pcd_on_setting[ARRAY_SIZE(lcm_pcd_on_setting_v2)] = { 0 };
	struct mipi_dsi_msg pcd_on2_setting[ARRAY_SIZE(lcm_pcd_on2_setting_v2)] = { 0 };
	struct mipi_dsi_msg pcd_on3_setting[ARRAY_SIZE(lcm_pcd_on3_setting_v2)] = { 0 };
	struct mipi_dsi_msg pcd_off_setting[ARRAY_SIZE(lcm_pcd_off_setting_v2)] = { 0 };

	for (i = 0; i < ARRAY_SIZE(lcm_pcd_on_setting_v2); i++) {
		pcd_on_setting[i].tx_len= lcm_pcd_on_setting_v2[i].count;
		pcd_on_setting[i].tx_buf = lcm_pcd_on_setting_v2[i].para_list;
	}
	for (i = 0; i < ARRAY_SIZE(lcm_pcd_on2_setting_v2); i++) {
		pcd_on2_setting[i].tx_len= lcm_pcd_on2_setting_v2[i].count;
		pcd_on2_setting[i].tx_buf = lcm_pcd_on2_setting_v2[i].para_list;
	}
	for (i = 0; i < ARRAY_SIZE(lcm_pcd_on3_setting_v2); i++) {
		pcd_on3_setting[i].tx_len= lcm_pcd_on3_setting_v2[i].count;
		pcd_on3_setting[i].tx_buf = lcm_pcd_on3_setting_v2[i].para_list;
	}
	for (i = 0; i < ARRAY_SIZE(lcm_pcd_off_setting_v2); i++) {
		pcd_off_setting[i].tx_len= lcm_pcd_off_setting_v2[i].count;
		pcd_off_setting[i].tx_buf = lcm_pcd_off_setting_v2[i].para_list;
	}

	struct mtk_dsi_cmd_msg pcd_on_setting_msg = {
		.cmd_num = ARRAY_SIZE(lcm_pcd_on_setting_v2),
		.transfer_mode = PACKET_LP_MODE,
		.cmd_msg = pcd_on_setting,
	};
	struct mtk_dsi_cmd_msg pcd_on2_setting_msg = {
		.cmd_num = ARRAY_SIZE(lcm_pcd_on2_setting_v2),
		.transfer_mode = PACKET_LP_MODE,
		.cmd_msg = pcd_on2_setting,
	};
	struct mtk_dsi_cmd_msg pcd_on3_setting_msg = {
		.cmd_num = ARRAY_SIZE(lcm_pcd_on3_setting_v2),
		.transfer_mode = PACKET_LP_MODE,
		.cmd_msg = pcd_on3_setting,
	};
	struct mtk_dsi_cmd_msg pcd_off_setting_msg = {
		.cmd_num = ARRAY_SIZE(lcm_pcd_off_setting_v2),
		.transfer_mode = PACKET_LP_MODE,
		.cmd_msg = pcd_off_setting,
	};

	if (state == true) {
		cb(dsi_drv, handle, cmd_opt, &pcd_on_setting_msg);
		cmdq_pkt_sleep(handle, 26000*120, 0x03);
		cb(dsi_drv, handle, cmd_opt, &pcd_on2_setting_msg);
		cmdq_pkt_sleep(handle, 26000*90, 0x03);
		cb(dsi_drv, handle, cmd_opt, &pcd_on3_setting_msg);
	} else {
		cmdq_pkt_sleep(handle, 26000*32, 0x03);
		cb(dsi_drv, handle, cmd_opt, &pcd_off_setting_msg);
	}
	pr_info("[LCM] %s state=%d\n", __func__, state);

	return 0;
}
static struct LCM_setting_table_v2 hbm_mode_enter_setting_v2[] = {
	{19,{0xA9,0x02,0x09,0xB0,0x00,0x00,0x11,0x01,0x00,0x8B,0x01,0x01,0x01,0x01,0x00,0x87,0x00,0x00,0x25}},
};
static struct LCM_setting_table_v2 hbm_mode_enter_setting_new_v2[] = {
	{26,{0xA9,0x02,0x09,0xB0,0x00,0x00,0x11,0x01,0x00,0x8B,0x01,0x01,0x01,0x01,0x00,0x87,0x00,0x00,0x25,0x02,0x00,0xDF,0x00,0x01,0x00,0x14}},
};
static struct LCM_setting_table_v2 hbm_mode_exit_setting_new_v2[] = {
	{19,{0xA9,0x02,0x09,0xB0,0x00,0x00,0x00,0x01,0x00,0x87,0x00,0x00,0x00,0x01,0x00,0x8B,0x01,0x01,0x00}},
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
		if(g_lcm_fresh_mode == MODE_4_FPS && g_aod_enable == 1)
		{
			struct mipi_dsi_msg enter_setting_v2[ARRAY_SIZE(hbm_mode_enter_setting_new_v2)] = { 0 };
			for (i = 0; i < ARRAY_SIZE(hbm_mode_enter_setting_new_v2); i++) {
				enter_setting_v2[i].tx_len= hbm_mode_enter_setting_new_v2[i].count;
				enter_setting_v2[i].tx_buf = hbm_mode_enter_setting_new_v2[i].para_list;
			}
			struct mtk_dsi_cmd_msg enter_setting_msg = {
				.cmd_num = ARRAY_SIZE(enter_setting_v2),
				.transfer_mode = PACKET_HS_MODE,
				.cmd_msg = enter_setting_v2,
			};
			cb(dsi_drv, handle, cmd_opt, &enter_setting_msg);
		} else {
			struct mipi_dsi_msg enter_setting_v2[ARRAY_SIZE(hbm_mode_enter_setting_v2)] = { 0 };
			for (i = 0; i < ARRAY_SIZE(hbm_mode_enter_setting_v2); i++) {
				enter_setting_v2[i].tx_len= hbm_mode_enter_setting_v2[i].count;
				enter_setting_v2[i].tx_buf = hbm_mode_enter_setting_v2[i].para_list;
			}
			struct mtk_dsi_cmd_msg enter_setting_msg = {
				.cmd_num = ARRAY_SIZE(enter_setting_v2),
				.transfer_mode = PACKET_HS_MODE,
				.cmd_msg = enter_setting_v2,
			};
			cb(dsi_drv, handle, cmd_opt, &enter_setting_msg);
		}
	}else {
		struct mipi_dsi_msg exit_setting_v2[ARRAY_SIZE(hbm_mode_exit_setting_new_v2)] = { 0 };
		for (i = 0; i < ARRAY_SIZE(hbm_mode_exit_setting_new_v2); i++) {
			exit_setting_v2[i].tx_len= hbm_mode_exit_setting_new_v2[i].count;
			exit_setting_v2[i].tx_buf = hbm_mode_exit_setting_new_v2[i].para_list;
		}
		struct mtk_dsi_cmd_msg exit_setting_msg = {
			.cmd_num = ARRAY_SIZE(exit_setting_v2),
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = exit_setting_v2,
		};
		cb(dsi_drv, handle, cmd_opt, &exit_setting_msg);
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
		if(g_lcm_fresh_mode == MODE_4_FPS && g_aod_enable == 1)
		{
			struct mipi_dsi_msg enter_setting_v2[ARRAY_SIZE(hbm_mode_enter_setting_new_v2)] = { 0 };
			for (i = 0; i < ARRAY_SIZE(hbm_mode_enter_setting_new_v2); i++) {
				enter_setting_v2[i].tx_len= hbm_mode_enter_setting_new_v2[i].count;
				enter_setting_v2[i].tx_buf = hbm_mode_enter_setting_new_v2[i].para_list;
			}
			struct mtk_dsi_cmd_msg enter_setting_msg = {
				.cmd_num = ARRAY_SIZE(enter_setting_v2),
				.transfer_mode = PACKET_HS_MODE,
				.cmd_msg = enter_setting_v2,
			};
			cb(dsi_drv, handle, cmd_opt, &enter_setting_msg);
		} else {
			struct mipi_dsi_msg enter_setting_v2[ARRAY_SIZE(hbm_mode_enter_setting_v2)] = { 0 };
			for (i = 0; i < ARRAY_SIZE(hbm_mode_enter_setting_v2); i++) {
				enter_setting_v2[i].tx_len= hbm_mode_enter_setting_v2[i].count;
				enter_setting_v2[i].tx_buf = hbm_mode_enter_setting_v2[i].para_list;
			}
			struct mtk_dsi_cmd_msg enter_setting_msg = {
				.cmd_num = ARRAY_SIZE(enter_setting_v2),
				.transfer_mode = PACKET_HS_MODE,
				.cmd_msg = enter_setting_v2,
			};
			cb(dsi_drv, handle, cmd_opt, &enter_setting_msg);
		}
	}else {
		struct mipi_dsi_msg exit_setting_v2[ARRAY_SIZE(hbm_mode_exit_setting_new_v2)] = { 0 };
		for (i = 0; i < ARRAY_SIZE(hbm_mode_exit_setting_new_v2); i++) {
			exit_setting_v2[i].tx_len= hbm_mode_exit_setting_new_v2[i].count;
			exit_setting_v2[i].tx_buf = hbm_mode_exit_setting_new_v2[i].para_list;
		}
		struct mtk_dsi_cmd_msg exit_setting_msg = {
			.cmd_num = ARRAY_SIZE(exit_setting_v2),
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = exit_setting_v2,
		};
		cb(dsi_drv, handle, cmd_opt, &exit_setting_msg);
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
	struct drm_display_mode *mode_30hz;
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
	mode_30hz = drm_mode_duplicate(connector->dev, &switch_mode_30hz);
	if (!mode_30hz) {
		dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_30hz.hdisplay,
			switch_mode_30hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_30hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_30hz);
	mode_30hz->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_30hz);
	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 158;
	pr_info("[LCM] %s end\n", __func__);
	return 4;
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
	pr_info("[LCM]nt37706a %s begin\n", __func__);
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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_NO_EOT_PACKET | MIPI_DSI_MODE_LPM;
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

	ctx->dvdd_gpio = devm_gpiod_get(dev, "dvdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(dev, "cannot get dvdd-gpios %ld\n", PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	devm_gpiod_put(dev, ctx->dvdd_gpio);

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

static void lcm_shutdown(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
	pr_info("[LCM] %s ctx->prepared=%d, begin\n", __func__, ctx->prepared);
	if (ctx->prepared) {
		lcm_dcs_write_seq_static(ctx, 0x53,0x20);
		msleep(18);
		lcm_dcs_write_seq_static(ctx, 0x51,0x00,0x00);
		msleep(18);
	}
}
static const struct of_device_id lcm_of_match[] = {
	{
		.compatible = "nt37706a,fhdp,dsi,vdo,dsc,boe,boe,120hz,x6887,old",
	},
	{} };
MODULE_DEVICE_TABLE(of, lcm_of_match);
static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.shutdown = lcm_shutdown,
	.driver = {
			.name = "nt37706a_fhdp_dsi_vdo_dsc_boe_boe_120hz_x6887_old",
			.owner = THIS_MODULE,
			.of_match_table = lcm_of_match,
		},
};
module_mipi_dsi_driver(lcm_driver);
MODULE_AUTHOR("you.tan <you.tan@transsion.com>");
MODULE_DESCRIPTION("VTDR6126A VDO 1.5K+120HZ Ramless AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
