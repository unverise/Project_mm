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
#include <linux/workqueue.h>  //for lcm_regulator_power_init func
#include <linux/cpufreq.h>
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
#define FRAME_WIDTH                 1080
#define FRAME_HEIGHT                2364
#define PHYSICAL_WIDTH              71574
#define PHYSICAL_HEIGHT             156657

#define HSA                         4
#define HBP                         18
#define VSA                         2
#define VBP                         40

/*Parameter setting for mode 0 Start*/
#define MODE_0_FPS                  144
#define MODE_0_VFP                  72
#define MODE_0_HFP                  58//95//28
#define MODE_0_DATA_RATE            1124//1190//1028
/*Parameter setting for mode 0 End*/
/*Parameter setting for mode 1 Start*/
#define MODE_1_FPS                  120
#define MODE_1_VFP                  72
#define MODE_1_HFP                  162//206//124
#define MODE_1_DATA_RATE            1124//1190//1028
/*Parameter setting for mode 1 End*/
/*Parameter setting for mode 2 Start*/
#define MODE_2_FPS                  90
#define MODE_2_VFP                  898
#define MODE_2_HFP                  162//206//124
#define MODE_2_DATA_RATE            1124//1190//1028
/*Parameter setting for mode 2 End*/
/*Parameter setting for mode 3 Start*/
#define MODE_3_FPS                  60
#define MODE_3_VFP                  2550
#define MODE_3_HFP                  162//206//124
#define MODE_3_DATA_RATE            1124//1190//1028
/*Parameter setting for mode 3 End*/
/*Parameter setting for mode 4 Start*/
#define MODE_4_FPS                  30
#define MODE_4_VFP                  72
#define MODE_4_HFP                  2040
#define MODE_4_DATA_RATE            1124//1190//1028
/*Parameter setting for mode 4 End*/

/* DSC RELATED */
#define DSC_ENABLE                  1
#define DSC_VER                     18
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 34  //8bit: 34, 10bit: 40
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         8  //8bit: 8, 10bit: 10
#define DSC_DSC_LINE_BUF_DEPTH      9  //8bit: 9, 10bit: 11
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128
//define DSC_PIC_HEIGHT
//define DSC_PIC_WIDTH
#define DSC_SLICE_HEIGHT            12//4
#define DSC_SLICE_WIDTH             540
#define DSC_CHUNK_SIZE              540
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               526//391
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      287//119
#define DSC_DECREMENT_INTERVAL      7
#define DSC_LINE_BPG_OFFSET         12//6
#define DSC_NFL_BPG_OFFSET          2235//4096
#define DSC_SLICE_BPG_OFFSET        2170//6509
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            4336
#define DSC_FLATNESS_MINQP          3  //8bit: 3, 10bit: 7
#define DSC_FLATNESS_MAXQP          12  //8bit: 12, 10bit: 16
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    11  //8bit: 11, 10bit: 15
#define DSC_RC_QUANT_INCR_LIMIT1    11  //8bit: 11, 10bit: 15
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3
static unsigned int vtdr6126a_vdo_144hz_dphy_buf_thresh[14] = {896, 1792, 2688, 3584, 4480,
	5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064};

static unsigned int vtdr6126a_vdo_144hz_dphy_range_min_qp[15] = {0, 0, 1, 1, 3, 3, 3, 3, 3,
	3, 5, 5, 5, 9, 12};
static unsigned int vtdr6126a_vdo_144hz_dphy_range_max_qp[15] = {4, 4, 5, 6, 7, 7, 7, 8, 9, 
	10, 10, 11, 11, 12, 13};
static int vtdr6126a_vdo_144hz_dphy_range_bpg_ofs[15] = {2, 0, 0, -2, -4, -6, -8, -8, -8, -10,
	-10, -12, -12, -12, -12};
#endif

/* define default dimming status*/
#define DIMMING_DEFAULT_STATUS     100

#ifndef UNUSED
#define UNUSED(x)    ((void)(x))
#endif

static bool lcm_longh30hz_support = false;
/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
/* option function to read data from some panel address */
/* #define PANEL_SUPPORT_READBACK */
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct regulator *vddi_regulator;
	struct regulator *vci_regulator;
	struct gpio_desc *dvdd_gpio;
	bool prepared;
	bool enabled;
	int error;
	bool hbm_en;
	bool hbm_wait;
};

#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
tran_lcm_doze_backlight g_lcm_doze_backlight = {
        .doze_backlight_num = 0,
        .doze_backlight_level1 = 0,
        .doze_backlight_level2 = 0,
        .doze_backlight_level3 = 0,
};
#endif

struct tran_panel_driver_params panel_driver_status = {0};
static char bl_tb0[] = {0x51, 0x0D, 0xBB};
static int g_aod_enable=0;
static int g_hbm_enable=0;
static int g_lcm_fresh_mode=MODE_1_FPS;
static unsigned int mapped_level = 0;
static char bl_dim[] = {0x53, 0x28}; //enable ddic dim function
static unsigned int g_dim_enable = 0; //record dim state
static unsigned int g_need_dim_enable = 0; //if or not need dim function
static unsigned int last_mapped_level = 0;
static int g_set_aod_light = 0;
static s32 cpu = 0, min_freq = 1300000;
static struct freq_qos_request qos_req;
extern int freq_qos_add_request(struct freq_constraints *qos,struct freq_qos_request *req, enum freq_qos_req_type type, s32 value);
extern int freq_qos_remove_request(struct freq_qos_request *req);
extern int freq_qos_update_request(struct freq_qos_request *req, s32 new_value);

extern s32 cmdq_pkt_sleep(struct cmdq_pkt *pkt, u32 tick, u16 reg_gpr);

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
struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[120];
};
static struct LCM_setting_table lcm_initialization_setting[] = {
	{0x2F,1,{0x01}},//frame select, 0x00:144Hz, 0x01:120Hz, 0x02:90Hz, 0x03:60Hz
	{0x8F,1,{0x01}},//Scale Enable
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},//CMD2 Page0
	{0x6F,1,{0x01}},
	{0xC0,1,{0x00}},//sync_mc_on=0
	{0x6F,1,{0x06}},
	{0xC0,1,{0x00}},//sync_mc_off=0
	{0x3B,4,{0x00,0x2A,0x00,0x50}},//VBP_EXT_FEST0=42 VFP_EXT_FEST0=80
	{0x6F,1,{0x04}},
	{0x3B,4,{0x00,0x2A,0x00,0x50}},//VBP_EXT_FEST1=42 VFP_EXT_FEST1=80
	{0x6F,1,{0x08}},
	{0x3B,4,{0x00,0x2A,0x00,0x50}},//VBP_EXT_FEST2=42  VFP_EXT_FEST2=2852
	{0x6F,1,{0x0C}},
	{0x3B,4,{0x00,0x2A,0x20,0xCC}},//VBP_EXT_FEST3=42 VFP_EXT_FEST3=8396
	{0x6F,1,{0x10}},
	{0x3B,4,{0x00,0x2A,0x00,0x50}},//VBP_EXT_FEST1=42 VFP_EXT_FEST1=80
	{0x6F,1,{0x18}},
	{0x3B,1,{0x80}},//en_manu_vbp_bf_scaler=1
	{0x6F,1,{0x19}},
	{0x3B,1,{0x30}},//MANU_VBP_BF_SCALER=48
	{0x8F,1,{0x01}},//Scale Enable
	{0x6F,1,{0x01}},
	{0x8F,2,{0x72,0x70}},//Scale_step_x=29296
	{0x6F,1,{0x03}},
	{0x8F,2,{0x72,0x72}},//Scale_step_x=29298
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x03}},//CMD2 Page3
	{0x6F,1,{0x1A}},
	{0xBA,5,{0x63,0x63,0x63,0x63,0x63}},//int_hsync_rem_period_fset=99
	{0x6F,1,{0x21}},
	{0xBA,5,{0x05,0x2E,0x2E,0x2E,0x2E}},//int_hsync_rem_num_fset=5,46, 46,46,5
	{0x6F,1,{0x2A}},
	{0xBA,10,{0x00,0x4A,0x00,0x59,0x00,0x59,0x00,0x59,0x00,0x59}},//t1a_mipi_fset & t1b_mipi_idle=74,89,89,89,89
	{0x6F,1,{0x05}},
	{0x8F,2,{0x09,0x3C}},//v_res_b4_scale=2364
	{0x6F,1,{0x0D}},
	{0xBA,1,{0x01}},//en_ext_hsync_calib
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x01}},
	{0x6F,1,{0x04}},
	{0xC3,1,{0xFF}},
	{0x6F,1,{0x09}},
	{0xC3,1,{0xFF}},
	{0xEA,1,{0xC0}},
	{0x6F,1,{0x11}},
	{0xEA,1,{0xC0}},
	{0x6F,1,{0x07}},
	{0xEA,10,{0x01,0x2E,0x01,0x6A,0x01,0x6A,0x01,0x6A,0x05,0xA7}},
	{0x6F,1,{0x18}},
	{0xEA,10,{0x01,0xA6,0x01,0xFA,0x01,0xFA,0x01,0xFA,0x07,0xEA}},

	{0xF0,5,{0x55,0xAA,0x52,0x08,0x04}},
	{0x6F,1,{0x01}},
	{0xC7,1,{0x11}},
	{0x6F,1,{0x04}},
	{0xC7,4,{0x1C,0x02,0x1C,0x02}},
	{0xFF,4,{0xAA,0x55,0xA5,0x80}},//CMD3_Page0
	{0x6F,1,{0x47}},
	{0xF2,1,{0x21}},//SEL_IOSC
	{0xFF,4,{0xAA,0x55,0xA5,0x80}},//CMD3_Page0
	{0x6F,1,{0x1F}},
	{0xF4,1,{0x0B}},
	{0x6F,1,{0x20}},
	{0xF4,1,{0x3F}},
	{0x6F,1,{0x08}},
	{0xFC,1,{0x03}},
	{0xFF,4,{0xAA,0x55,0xA5,0x80}},//CMD3_Page0
	//VDD_OSC/VDD_OSC2 cap
	{0x6F,1,{0x24}},
	{0xF8,1,{0xFF}},
	//Power off Timing (For Vinit)
	{0x6F,1,{0x08}},
	{0xF6,1,{0x44}},
	//Power off Timing (For VGXP)
	{0x6F,1,{0x0A}},
	{0xF6,6,{0x60,0x60,0x60,0x60,0x60,0x30}},
	{0xFF,4,{0xAA,0x55,0xA5,0x84}},//CMD3_Page4
	//dpc_opt = 0x02
	{0x6F,1,{0x10}},
	{0xF8,1,{0x02}},
	{0xFF,4,{0xAA,0x55,0xA5,0x84}},//CMD3_Page4
	{0x81,2,{0x03,0x28}},
	{0xF2,1,{0x15}},//BIST mode round shift
	{0xFF,4,{0xAA,0x55,0xA5,0x84}},//CMD3_Page4
	//VDC setting
	{0x6F,1,{0x27}},
	{0xF7,1,{0x00}},
	{0xFF,4,{0xAA,0x55,0xA5,0x84}},//CMD3_Page4
	//LTCT setting
	{0x6F,1,{0xA9}},
	{0xF4,1,{0xF3}},
	{0x17,1,{0x03}},
	//GB 1.2 10_8 3.75x VESASPEC_1080x2364_slic e2_12 for Scaling up
	{0x2A,4,{0x00,0x00,0x04,0x37}},
	{0x2B,4,{0x00,0x00,0x09,0x3B}},
	{0x90,2,{0x03,0x43}},
	{0x91,18,{0x89,0xA8,0x00,0x0C,0xC2,0x00,0x02,0x0E,0x01,0x1F,
		0x00,0x07,0x08,0xBB,0x08,0x7A,0x10,0xF0}},
	{0x53,1,{0x20}},
	{0x88,1,{0x01}},
	{0x2F,1,{0x01}},
	{0x35,1,{0x00}},
	{0x5F,2,{0x00,0x00}},
	{0x51,2,{0x00,0x00}},
	{0xA2,1,{0x01}},

	{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},
	{0xB2,1,{0x99}},
	{0x6F,1,{0x02}},
	{0xB2,2,{0x20,0x20}},

	{0xF0,5,{0x55,0xAA,0x52,0x08,0x01}},
	{0x6F,1,{0x03}},
	{0xC7,1,{0xC7}},

	{0xF0,5,{0x55,0xAA,0x52,0x08,0x05}},
	{0xC9,1,{0x11}},
	{0xCA,1,{0x11}},
	{0xCB,5,{0x00,0x00,0x00,0x00,0x00}},
	{0xD8,1,{0x11}},
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x01}},
	{0x6F,1,{0x03}},
	{0xC7,6,{0xC7,0x00,0x27,0x08,0x02,0x05}},

	{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},
	{0xC0,1,{0x40}},
	{0xB2,1,{0x99}},

	{0xF0,5,{0x55,0xAA,0x52,0x08,0x01}},
	{0x6F,1,{0x01}},
	{0xD2,1,{0x01}},

	{0x11,0,{}},
	{REGFLAG_DELAY, 120, {}},
	{0x29,0,{}},
	{REGFLAG_DELAY, 20,{}},
};
static struct LCM_setting_table lcm_suspend_setting[] = {
	// Display off sequence
	{0x28, 0, {}},
	{0x10, 0, {}},
	{REGFLAG_DELAY, 120, {}},
};
static struct LCM_setting_table aodmode_hbm_mode_enter_setting[] = {
	{0xA9,25,{0x02,0x09,0xB0,0x00,0x00,0x11,0x01,0x00,0x8B,0x01,
		0x01,0x01,0x01,0x00,0x87,0x00,0x00,0x25,0x02,0x00,
		0xDF,0x00,0x01,0x00,0x14}},
};

static struct LCM_setting_table hbm_mode_enter_setting[] = {
	{0xA9,18,{0x02,0x09,0xB0,0x00,0x00,0x11,0x01,0x00,0x8B,0x01,
		0x01,0x01,0x01,0x00,0x87,0x00,0x00,0x25}},
};

static struct LCM_setting_table hbm_mode_exit_setting[] = {
	{0xA9,18,{0x02,0x09,0xB0,0x00,0x00,0x00,0x01,0x00,0x87,0x00,
		0x00,0x00,0x01,0x00,0x8B,0x01,0x01,0x00}},
};


static struct LCM_setting_table dimming_modify_setting[] = {
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},
	{0xB2,1,{0x99}},
	{0x6F,1,{0x02}},
	{0xB2,2,{0x05,0x05}},
};

#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
static struct LCM_setting_table lcm_pcd_on_setting[] = {
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x01}},
	{0xD0,2,{0x00,0x80}},
	{0x11,0,{}},
};

static struct LCM_setting_table lcm_pcd_on2_setting[] = {
	{0x29,0,{}},
};

static struct LCM_setting_table lcm_pcd_on3_setting[] = {
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x01}},
};

static struct LCM_setting_table lcm_pcd_off_setting[] = {
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x01}},
	{0xD0,2,{0x00,0x00}},
};
#endif

static struct LCM_setting_table aod_mode_esd_recovery_setting[] = {
	{0x39,0,{}},
};

static struct LCM_setting_table aod_mode_enter_setting[] = {
	{0x51,2,{0x0F, 0xFF}},
	{0x6F,1,{0x04}},
	{0x51,2,{0x0F, 0xFE}},
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
static void push_table_cb(void *dsi, dcs_write_gce cb, void *handle, struct LCM_setting_table *table, unsigned int table_count)
{
	unsigned int i, j;
	unsigned char temp[255] = {0};
	unsigned int cmd;
	for (i = 0; i < table_count; i++) {
		cmd = table[i].cmd;
		memset(temp, 0, sizeof(temp));
		switch (cmd) {
			case REGFLAG_DELAY:
				msleep(table[i].count);
				break;
			case REGFLAG_UDELAY:
				udelay(table[i].count);
				break;
			case REGFLAG_END_OF_TABLE:
				break;
			default:
				temp[0] = cmd;
				for (j = 0; j < table[i].count; j++)
					temp[j+1] = table[i].para_list[j];
				cb(dsi, handle, temp, table[i].count+1);
			}
	}
}

struct lcm_scale_ratio {
	unsigned int numerator;
	unsigned int denominator;
};

static struct lcm_scale_ratio g_lcm_scale_ratio = {
	.numerator = 1,
	.denominator = 1,
};

/* B0~B2 coefficients, fixed-point x100000, 12 pairs per register */
static const unsigned int g_red_coeff_x100000[36] = {
	0,      86710,  86691,  87480,  88307,  88908,
	90065,  91978,  93616,  94881,  96609,  97500,
	98225,  98604,  98952,  99381,  99651,  99794,
	99950,  99929, 100159, 100130, 100186, 100185,
	100123, 100123, 100123, 100123, 100123, 100123,
	100123, 100123, 100123, 100123, 100123, 100123,
};

/* B3~B5 coefficients, fixed-point x100000, 12 pairs per register */
static const unsigned int g_green_coeff_x100000[36] = {
	0,      88185,  88186,  88678,  89376,  89797,
	90484,  91544,  92557,  93713,  95467,  96674,
	97597,  98217,  98595,  99217,  99571,  99754,
	99947, 100051, 100217, 100253, 100286, 100262,
	100302, 100302, 100302, 100302, 100302, 100302,
	100302, 100302, 100302, 100302, 100302, 100302,
};

/* B6~B8 coefficients, fixed-point x100000, 12 pairs per register */
static const unsigned int g_blue_coeff_x100000[36] = {
	0,      96569,  96579,  96554,  96672,  96753,
	97146,  97665,  98037,  98509,  98983,  99375,
	99620,  99792,  99925, 100048, 100092, 100132,
	100149, 100165, 100195, 100148, 100178, 100159,
	100210, 100210, 100210, 100210, 100210, 100210,
	100210, 100210, 100210, 100210, 100210, 100210,
};

static u8 g_read_gamma_buf[9][24] = {0};
static const u8 g_gamma_reg_list[9] = {
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8
};
static const u8 g_gamma_reg_len[9] = {
	24, 24, 24, 24, 24, 24, 24, 24, 24,
};
static bool g_bf_cached_done = false;
static u8 g_bf_cached_val = 0xFF;
static bool g_gamma_scaled_done = false;

static int lcm_dcs_read_len(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return ctx->error;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		pr_err("[LCM] %s: read cmd=0x%02x len=%zu ret=%zd\n",
			__func__, cmd, len, ret);
		ctx->error = (int)ret;
		return (int)ret;
	}
	return (int)ret;
}

static int lcm_read_bf_id(struct lcm *ctx, u8 *bf_val)
{
	int ret;
	u8 page1_cmd[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01};
	u8 val = 0;

	if (!ctx || !bf_val)
		return -EINVAL;

	lcm_dcs_write(ctx, page1_cmd, ARRAY_SIZE(page1_cmd));
	ret = lcm_dcs_read_len(ctx, 0xBF, &val, 1);
	if (ret < 0)
		return ret;
	if (ret == 0)
		return -EIO;

	*bf_val = val;
	pr_info("[LCM] %s: page1 BF=0x%02x\n", __func__, *bf_val);
	return 0;
}

static int lcm_read_multi_by_offset(struct lcm *ctx, u8 cmd, u8 *buffer, u32 length)
{
	u32 pos = 0;
	u32 i;
	int ret;
	u8 tmp[10] = {0};
	u8 page_cmd[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x02};
	u8 page_cmd_2[] = {0xBF, 0x02, 0x05};
	u8 off_cmd[2] = {0x6F, 0x00};
	u32 chunk;

	if (!buffer || !length)
		return -EINVAL;

	pr_info("[LCM] %s: start cmd=0x%02x len=%u\n", __func__, cmd, length);
	lcm_dcs_write(ctx, page_cmd, ARRAY_SIZE(page_cmd));
	lcm_dcs_write(ctx, page_cmd_2, ARRAY_SIZE(page_cmd_2));

	while (pos < length) {
		memset(tmp, 0, sizeof(tmp));
		off_cmd[1] = (u8)pos;
		lcm_dcs_write(ctx, off_cmd, ARRAY_SIZE(off_cmd));
		pr_info("[LCM] %s: pos=%u offset=0x%02x\n", __func__, pos, off_cmd[1]);
		chunk = length - pos;
		if (chunk > sizeof(tmp))
			chunk = sizeof(tmp);
		ret = lcm_dcs_read_len(ctx, cmd, tmp, chunk);
		if (ret <= 0) {
			pr_err("[LCM] %s: read failed pos=%u ret=%d\n", __func__, pos, ret);
			return (ret < 0) ? ret : -EIO;
		}
		memcpy(buffer + pos, tmp, chunk);
		pos += chunk;
	}
	for (i = 0; i < length; i++)
		pr_info("[LCM] %s: buffer[%u]=0x%02x\n", __func__, i, buffer[i]);
	return 0;
}

static int lcm_write_multi_by_offset(struct lcm *ctx, u8 cmd, const u8 *buffer, u32 length)
{
	u8 off_cmd[] = {0x6F, 0x00};
	u8 tx_buf[65] = {0};

	if (!ctx || !buffer || !length || length > 64)
		return -EINVAL;

	tx_buf[0] = cmd;
	memcpy(&tx_buf[1], buffer, length);
	lcm_dcs_write(ctx, off_cmd, ARRAY_SIZE(off_cmd));
	lcm_dcs_write(ctx, tx_buf, length + 1);
	pr_info("[LCM] %s: write cmd=0x%02x len=%u done(one-shot)\n",
		__func__, cmd, length);
	return 0;
}

static void lcm_apply_scale_on_pairs(u8 *buf, u32 len, struct lcm_scale_ratio ratio)
{
	u32 i, raw, scaled;
	u64 product;

	if (!buf || len < 2 || (len & 0x1) || ratio.denominator == 0)
		return;

	for (i = 0; i < len; i += 2) {
		raw = ((u32)buf[i] << 8) | (u32)buf[i + 1];
		product = (u64)raw * (u64)ratio.numerator;
		scaled = (u32)(product / ratio.denominator);
		if (scaled > 0xFFFF)
			scaled = 0xFFFF;
		buf[i] = (u8)((scaled >> 8) & 0xFF);
		buf[i + 1] = (u8)(scaled & 0xFF);
	}
}

static void lcm_apply_scale_on_pairs_coeff(u8 *buf, u32 len,
					   const unsigned int *coeff_x100000,
					   u32 coeff_cnt)
{
	u32 i, raw, scaled, pair_cnt;
	u64 product;

	if (!buf || !coeff_x100000 || len < 2 || (len & 0x1))
		return;

	pair_cnt = len / 2;
	if (coeff_cnt < pair_cnt)
		pair_cnt = coeff_cnt;

	for (i = 0; i < pair_cnt; i++) {
		raw = ((u32)buf[i * 2] << 8) | (u32)buf[i * 2 + 1];
		product = (u64)raw * (u64)coeff_x100000[i];
		scaled = (u32)(product / 100000ULL);
		if (scaled > 0xFFFF)
			scaled = 0xFFFF;
		buf[i * 2] = (u8)((scaled >> 8) & 0xFF);
		buf[i * 2 + 1] = (u8)(scaled & 0xFF);
	}
}

static void lcm_handle_gamma_restore(struct lcm *ctx)
{
	int ret = 0;
	int reg_idx;
	u8 wr_page_cmd[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x02};
	u8 wr_page_cmd_2[] = {0xBF, 0x02, 0x05};
	u8 post_cmd[] = {0xCC, 0x10};
	u8 post_cmd0[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
	u8 post_cmd1[] = {0x6F, 0x34};
	u8 post_cmd2[] = {0xB4, 0x07, 0xBA, 0x07, 0xBA, 0x07, 0xBA, 0x07,
		0xBA, 0x07, 0x58, 0x05, 0x50, 0x00, 0xC8, 0x00, 0x70, 0x00,
		0x70, 0x00, 0x70, 0x00, 0x70, 0x00, 0x70, 0x00, 0x70};

	if (!ctx)
		return;

	if (!g_bf_cached_done) {
		ret = lcm_read_bf_id(ctx, &g_bf_cached_val);
		if (ret < 0) {
			pr_err("[LCM] %s: read page1 BF failed ret=%d\n", __func__, ret);
			return;
		}
		g_bf_cached_done = true;
		pr_info("[LCM] %s: cache page1 BF=0x%02x\n", __func__, g_bf_cached_val);
	} else {
		pr_info("[LCM] %s: reuse cached page1 BF=0x%02x\n", __func__, g_bf_cached_val);
	}

	if (g_bf_cached_val != 0x00) {
		pr_info("[LCM] %s: skip restore gamma, page1 BF=0x%02x\n",
			__func__, g_bf_cached_val);
		return;
	}

	if (!g_gamma_scaled_done) {
		for (reg_idx = 0; reg_idx < ARRAY_SIZE(g_gamma_reg_list); reg_idx++) {
			ret = lcm_read_multi_by_offset(ctx, g_gamma_reg_list[reg_idx],
				g_read_gamma_buf[reg_idx], g_gamma_reg_len[reg_idx]);
			if (ret < 0) {
				pr_err("[LCM] %s: read reg 0x%02x failed ret=%d\n",
					__func__, g_gamma_reg_list[reg_idx], ret);
				return;
			}
		}

		for (reg_idx = 0; reg_idx < ARRAY_SIZE(g_gamma_reg_list); reg_idx++) {
			if (reg_idx < 3)
				lcm_apply_scale_on_pairs_coeff(g_read_gamma_buf[reg_idx],
					g_gamma_reg_len[reg_idx],
					&g_red_coeff_x100000[reg_idx * 12], 12);
			else if (reg_idx < 6)
				lcm_apply_scale_on_pairs_coeff(g_read_gamma_buf[reg_idx],
					g_gamma_reg_len[reg_idx],
					&g_green_coeff_x100000[(reg_idx - 3) * 12], 12);
			else if (reg_idx < 9)
				lcm_apply_scale_on_pairs_coeff(g_read_gamma_buf[reg_idx],
					g_gamma_reg_len[reg_idx],
					&g_blue_coeff_x100000[(reg_idx - 6) * 12], 12);
			else
				lcm_apply_scale_on_pairs(g_read_gamma_buf[reg_idx],
					g_gamma_reg_len[reg_idx], g_lcm_scale_ratio);
		}

		g_gamma_scaled_done = true;
		pr_info("[LCM] %s: read/scale done once\n", __func__);
	}

	lcm_dcs_write(ctx, wr_page_cmd, ARRAY_SIZE(wr_page_cmd));
	lcm_dcs_write(ctx, wr_page_cmd_2, ARRAY_SIZE(wr_page_cmd_2));
	if (!g_gamma_scaled_done) {
		pr_err("[LCM] %s: gamma not scaled, skip write\n", __func__);
		return;
	}

	for (reg_idx = 0; reg_idx < ARRAY_SIZE(g_gamma_reg_list); reg_idx++) {
		ret = lcm_write_multi_by_offset(ctx, g_gamma_reg_list[reg_idx],
			g_read_gamma_buf[reg_idx], g_gamma_reg_len[reg_idx]);
		if (ret < 0) {
			pr_err("[LCM] %s: write reg 0x%02x failed ret=%d\n",
				__func__, g_gamma_reg_list[reg_idx], ret);
			return;
		}
	}

	lcm_dcs_write(ctx, post_cmd, ARRAY_SIZE(post_cmd));
	lcm_dcs_write(ctx, post_cmd0, ARRAY_SIZE(post_cmd0));
	lcm_dcs_write(ctx, post_cmd1, ARRAY_SIZE(post_cmd1));
	lcm_dcs_write(ctx, post_cmd2, ARRAY_SIZE(post_cmd2));
}

static void cpu_qos_min_write(s32 cpu, s32 min_freq)
{
	int ret = 0;
	static struct cpufreq_policy *policy;

	policy = cpufreq_cpu_get(cpu);
	if (!policy) {
		pr_err("[LCM] Failed to get cpufreq policy for CPU %d\n", cpu);
		return;
	}

	ret = freq_qos_add_request(&policy->constraints, &qos_req, FREQ_QOS_MIN, min_freq);
	if (ret < 0) {
		 pr_err("[LCM] Failed to add freq QoS request for CPU %d: %d\n", cpu, ret);
	} else {
		pr_info("[LCM] Locked CPU %d min frequency to %d kHz\n", cpu, min_freq);
	}


	cpufreq_cpu_put(policy);
	return;
}

static void cpu_qos_update(s32 new_min_freq)
{
    int ret;

	if(!IS_ERR_OR_NULL(qos_req.qos))
	{

		ret = freq_qos_update_request(&qos_req, new_min_freq);
		if (ret < 0) {
			pr_err("[LCM] Failed to update freq QoS request: %d\n", ret);
		} else {
			pr_info("[LCM] Updated min frequency to %d kHz\n", new_min_freq);
		}
	} else {
		pr_err("[LCM] update freq QoS request IS_ERR_OR_NULL\n");
	}
	return;
}
static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("[LCM] %s begin\n", __func__);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(12); ////DDIC spec need more 10ms
	lcm_handle_gamma_restore(ctx);
	switch (g_lcm_fresh_mode) {
		case MODE_0_FPS: //144Hz
			lcm_initialization_setting[0].para_list[0] = 0x00;
			break;
		case MODE_1_FPS: //120Hz
			lcm_initialization_setting[0].para_list[0] = 0x01;
			break;
		case MODE_2_FPS: //90Hz
			lcm_initialization_setting[0].para_list[0] = 0x02;
			break;
		case MODE_3_FPS: //60Hz
			lcm_initialization_setting[0].para_list[0] = 0x03;
			break;
	}
	push_table(ctx, lcm_initialization_setting,
		sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table));

	//ESD
	if (mapped_level) {
		lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
		if (lcm_longh30hz_support) {
			if(g_aod_enable) {
				push_table(ctx, aod_mode_esd_recovery_setting,
					sizeof(aod_mode_esd_recovery_setting) / sizeof(struct LCM_setting_table));
				push_table(ctx, aod_mode_enter_setting,
					sizeof(aod_mode_enter_setting) / sizeof(struct LCM_setting_table));
			}
		}
	}

	ctx->error=0;
	g_dim_enable = 0;
	g_need_dim_enable = 0;
	pr_info("[LCM] %s end\n", __func__);
}
static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled) {
		pr_info("[LCM] %s ctx->enabled already false\n", __func__);
		return 0;
	}
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}
	ctx->enabled = false;
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}
static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared) {
		pr_info("[LCM] %s ctx->prepared already false\n", __func__);
		return 0;
	}
	pr_info("[LCM] %s begin\n", __func__);
	g_aod_enable=0;
	push_table(ctx, lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table));
	if(g_lcm_fresh_mode == MODE_0_FPS)
		cpu_qos_update(0);
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
	if (ctx->prepared) {
		pr_info("[LCM] %s ctx->prepared already true\n", __func__);
		return 0;
	}
	pr_info("[LCM] %s begin\n", __func__);
	lcm_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);
	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	if (g_lcm_fresh_mode == MODE_0_FPS)
		cpu_qos_update(min_freq);
	pr_info("[LCM] %s end\n", __func__);
	return ret;
}
static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	if (ctx->enabled) {
		pr_info("[LCM] %s ctx->enabled already true\n", __func__);
		return 0;
	}
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}
	ctx->enabled = true;
	return 0;
}
static const struct drm_display_mode switch_mode_144hz = {
	.clock = (int)((FRAME_WIDTH + MODE_0_HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_0_VFP + VSA + VBP) * MODE_0_FPS / 1000),
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
};
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
static struct mtk_panel_params ext_params_144hz = {
	.pll_clk = MODE_0_DATA_RATE/2,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.ssc_enable=0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
	.lcm_pcd_check_table[0] = {
		.cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xBC,
	},
	.set_pcd_params_with_gec3 = 1,
	#endif
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.lp_perline_en = 1,
	//.tran_need_notify_tp_fps = MODE_3_FPS,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = vtdr6126a_vdo_144hz_dphy_buf_thresh,
			.range_min_qp = vtdr6126a_vdo_144hz_dphy_range_min_qp,
			.range_max_qp = vtdr6126a_vdo_144hz_dphy_range_max_qp,
			.range_bpg_ofs = vtdr6126a_vdo_144hz_dphy_range_bpg_ofs,
		},
	},
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 38, {0xA9,0x02,0x09,0xB0,0x00,0x00,0x00,0x01,0x00,0x2F,
					      0x00,0x00,0x00,0x01,0x00,0xA2,0x00,0x13,0x03,0x02,
					      0x03,0xB3,0x7F,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,
					      0x00,0x00,0x02,0x08,0xBF,0x01,0x01,0x89}},
		.dfps_cmd_table[1] = {0, 1, {0x38}},
	},
	.data_rate = MODE_0_DATA_RATE,
	.tran_panel_params = &panel_driver_status,
};
static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = MODE_1_DATA_RATE/2,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.ssc_enable=0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
	.lcm_pcd_check_table[0] = {
		.cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xBC,
	},
	.set_pcd_params_with_gec3 = 1,
	#endif
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.lp_perline_en = 1,
	//.tran_need_notify_tp_fps = MODE_3_FPS,
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
			.rc_buf_thresh = vtdr6126a_vdo_144hz_dphy_buf_thresh,
			.range_min_qp = vtdr6126a_vdo_144hz_dphy_range_min_qp,
			.range_max_qp = vtdr6126a_vdo_144hz_dphy_range_max_qp,
			.range_bpg_ofs = vtdr6126a_vdo_144hz_dphy_range_bpg_ofs,
		},
	},
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 45, {0xA9,0x02,0x09,0xB0,0x00,0x00,0x00,0x01,0x00,0x2F,
						0x00,0x00,0x01,0x01,0x02,0x00,0xDF,0x00,0x01,0x00,
						0x54,0x00,0xA2,0x00,0x13,0x03,0x02,0x03,0xB3,0x7F,
						0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,
						0x08,0xBF,0x01,0x01,0x88}},
		.dfps_cmd_table[1] = {0, 1, {0x38}},
	},
	.data_rate = MODE_1_DATA_RATE,
	.tran_panel_params = &panel_driver_status,
};
static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = MODE_2_DATA_RATE/2,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.ssc_enable=0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
	.lcm_pcd_check_table[0] = {
		.cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xBC,
	},
	.set_pcd_params_with_gec3 = 1,
	#endif
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.lp_perline_en = 1,
	//.tran_need_notify_tp_fps = MODE_3_FPS,
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
			.rc_buf_thresh = vtdr6126a_vdo_144hz_dphy_buf_thresh,
			.range_min_qp = vtdr6126a_vdo_144hz_dphy_range_min_qp,
			.range_max_qp = vtdr6126a_vdo_144hz_dphy_range_max_qp,
			.range_bpg_ofs = vtdr6126a_vdo_144hz_dphy_range_bpg_ofs,
		}, 
	},
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 38, {0xA9,0x02,0x09,0xB0,0x00,0x00,0x01,0x01,0x00,0x2F,
					      0x00,0x00,0x02,0x01,0x00,0xA2,0x00,0x13,0x03,0x02,
					      0x03,0xB3,0x7F,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,
					      0x00,0x00,0x02,0x08,0xBF,0x01,0x01,0x88}},
		.dfps_cmd_table[1] = {0, 1, {0x38}},
	},
	.data_rate = MODE_2_DATA_RATE,
	.tran_panel_params = &panel_driver_status,
};
static struct mtk_panel_params ext_params_60hz = {
	.pll_clk = MODE_3_DATA_RATE/2,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.ssc_enable=0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
	.lcm_pcd_check_table[0] = {
		.cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xBC,
	},
	.set_pcd_params_with_gec3 = 1,
	#endif
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.lp_perline_en = 1,
	//.tran_need_notify_tp_fps = MODE_3_FPS,
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
			.rc_buf_thresh = vtdr6126a_vdo_144hz_dphy_buf_thresh,
			.range_min_qp = vtdr6126a_vdo_144hz_dphy_range_min_qp,
			.range_max_qp = vtdr6126a_vdo_144hz_dphy_range_max_qp,
			.range_bpg_ofs = vtdr6126a_vdo_144hz_dphy_range_bpg_ofs,
		}, 
	},
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 38, {0xA9,0x02,0x09,0xB0,0x00,0x00,0x00,0x01,0x00,0x2F,
					      0x00,0x00,0x03,0x01,0x00,0xA2,0x00,0x13,0x03,0x02,
					      0x03,0xB3,0x7F,0x7F,0x80,0x00,0x00,0x00,0x00,0x00,
					      0x00,0x00,0x02,0x08,0xBF,0x01,0x01,0x88}},
		.dfps_cmd_table[1] = {0, 1, {0x38}},
	},
	.data_rate = MODE_3_DATA_RATE,
	.tran_panel_params = &panel_driver_status,
};

static struct mtk_panel_params ext_params_30hz = {
	.pll_clk = MODE_4_DATA_RATE/2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.ssc_enable=0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.lp_perline_en = 1,
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
			.rc_buf_thresh = vtdr6126a_vdo_144hz_dphy_buf_thresh,
			.range_min_qp = vtdr6126a_vdo_144hz_dphy_range_min_qp,
			.range_max_qp = vtdr6126a_vdo_144hz_dphy_range_max_qp,
			.range_bpg_ofs = vtdr6126a_vdo_144hz_dphy_range_bpg_ofs,
		},
	},
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 1, {0x39}},
	},
	.data_rate = MODE_4_DATA_RATE,
	.tran_panel_params = &panel_driver_status,
};
#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
static int lcm_set_pcd_cmdq(void *dsi, dcs_write_gce cb, void *handle, bool state)
{
	if (!cb) {
		pr_info("[LCM] %s cb is null\n", __func__);
		return -1;
	}

	if (state == true) {
		push_table_cb(dsi, cb, handle, lcm_pcd_on_setting, sizeof(lcm_pcd_on_setting) / sizeof(struct LCM_setting_table));
		mdelay(120);
		push_table_cb(dsi, cb, handle, lcm_pcd_on2_setting, sizeof(lcm_pcd_on2_setting) / sizeof(struct LCM_setting_table));
		mdelay(90);
		push_table_cb(dsi, cb, handle, lcm_pcd_on3_setting, sizeof(lcm_pcd_on3_setting) / sizeof(struct LCM_setting_table));
	} else {
		mdelay(30);
		push_table_cb(dsi, cb, handle, lcm_pcd_off_setting, sizeof(lcm_pcd_off_setting) / sizeof(struct LCM_setting_table));
	}
	pr_info("[LCM] %s state=%d\n", __func__, state);

	return 0;
}
#endif

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	//unsigned int mapped_level = level;

	if (level > 5119)
		level = 5119;

	if(level<=4095) {
		mapped_level = level * 3515 / 4095;
	} else if (4095 < level && level <= 5119) {
		mapped_level = 3515 + (level - 4095) * (4095-3515) / 1024;
	}

	bl_tb0[1] = ((mapped_level >> 8) & 0x0f);
	bl_tb0[2] = (mapped_level & 0xff);

	if (!cb) {
		pr_err("[LCM] %s cb is invalid\n", __func__);
		return -1;
	}

	//USD mofidy dimming setting start
	if (panel_driver_status.dimming_status ==2) {
		dimming_modify_setting[3].para_list[0] = 0x05;
		dimming_modify_setting[3].para_list[1] = 0x05;
        push_table_cb(dsi, cb, handle, dimming_modify_setting,
            sizeof(dimming_modify_setting) / sizeof(struct LCM_setting_table));
		pr_info("[LCM] %s bl_dim dimming_status =%d\n", __func__, panel_driver_status.dimming_status);
		panel_driver_status.dimming_status = DIMMING_DEFAULT_STATUS;
		g_dim_enable = 0;
	}

	if (panel_driver_status.dimming_status ==1) {
		dimming_modify_setting[3].para_list[0] = 0x20;
		dimming_modify_setting[3].para_list[1] = 0x20;
        push_table_cb(dsi, cb, handle, dimming_modify_setting,
            sizeof(dimming_modify_setting) / sizeof(struct LCM_setting_table));
		pr_info("[LCM] %s bl_dim dimming_status =%d\n", __func__, panel_driver_status.dimming_status);
		panel_driver_status.dimming_status = DIMMING_DEFAULT_STATUS;
		g_dim_enable = 0;
	}
	//USD mofidy dimming setting end
	if ((g_dim_enable==0)&&(g_need_dim_enable==1)) {
		bl_dim[1] = 0x28;
		cb(dsi, handle, bl_dim, ARRAY_SIZE(bl_dim));
		g_dim_enable =  1;
	}

	if(g_set_aod_light == 1) {
		g_set_aod_light = 0;
	} else {
		g_need_dim_enable = 1;
	}

	if ((mapped_level == 0) || (panel_driver_status.dimming_status ==0)) {
		bl_dim[1] = 0x20;
		cb(dsi, handle, bl_dim, ARRAY_SIZE(bl_dim));
		g_dim_enable = 0;
		g_need_dim_enable = 0;
	}
	if (mapped_level != 0) {
		last_mapped_level = mapped_level;
	}

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	pr_info("[LCM] %s level = %d, mapped_level = %d\n", __func__, level, mapped_level);

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
	pr_info("[LCM] %s FPS form %d to %d begin\n", __func__, g_lcm_fresh_mode, drm_mode_vrefresh(m));
	if (lcm_longh30hz_support) {
		if (drm_mode_vrefresh(m) == MODE_4_FPS) {
			ext->params = &ext_params_30hz;
			g_lcm_fresh_mode = drm_mode_vrefresh(m);
			return ret;
		}
	}
	if (drm_mode_vrefresh(m) == MODE_0_FPS)
		ext->params = &ext_params_144hz;
	else if (drm_mode_vrefresh(m) == MODE_1_FPS)
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == MODE_2_FPS)
		ext->params = &ext_params_90hz;
	else if (drm_mode_vrefresh(m) == MODE_3_FPS)
		ext->params = &ext_params_60hz;
	else
		ret = 1;
	if(drm_mode_vrefresh(m) == MODE_0_FPS && g_lcm_fresh_mode!= MODE_0_FPS) //select 144hz
	{
		cpu_qos_update(min_freq);
	}
	else if (drm_mode_vrefresh(m) != MODE_0_FPS && g_lcm_fresh_mode == MODE_0_FPS) //out 144hz
	{
		cpu_qos_update(0);
	}
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
	int ret = 0;
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("[LCM] %s status=%d begin\n", __func__, on);
	if (IS_ERR_OR_NULL(ctx->vddi_regulator)) {
		ctx->vddi_regulator = devm_regulator_get(ctx->dev, "vddi");
		if (IS_ERR_OR_NULL(ctx->vddi_regulator)) {
			ret = PTR_ERR(ctx->vddi_regulator);
			dev_err(ctx->dev, "<%s,%d> lcm vddi get fail!ret[%d].\n", __func__, __LINE__, ret);
			return ret;
		}
	}
	if (IS_ERR_OR_NULL(ctx->vci_regulator)) {
		ctx->vci_regulator = devm_regulator_get(ctx->dev, "vci");
		if (IS_ERR_OR_NULL(ctx->vci_regulator)) {
			ret = PTR_ERR(ctx->vci_regulator);
			dev_err(ctx->dev, "<%s,%d> lcm dvdd get fail!ret[%d].\n", __func__, __LINE__, ret);
			return ret;
		}
	}
	if (on) {
		unsigned long time_diff = 0;
		lcm_prepare_time = jiffies_to_msecs(jiffies);
		time_diff = lcm_prepare_time - lcm_unprepare_time;
		if (time_diff < 120)
			mdelay(120 - time_diff);

		//VDDI 1.8V enable
		ret = regulator_set_voltage(ctx->vddi_regulator, 1800000, 1800000);
		if (ret < 0)
			dev_err(ctx->dev, "<%s:%d>set lcm vddi vol error!ret[%d]\n", __func__, __LINE__, ret);
		ret = regulator_enable(ctx->vddi_regulator);
		if (ret < 0)
			dev_err(ctx->dev, "<%s:%d>enable lcm vddi error!ret[%d]\n", __func__, __LINE__, ret);
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

		mdelay(12); //DDIC spec need more 10ms
	} else {
		//lcm reset pull low after mipipoweroff
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		mdelay(2);
		//VCI 3.0V disable
		if (regulator_is_enabled(ctx->vci_regulator)) {
			ret = regulator_disable(ctx->vci_regulator);
			if (ret < 0)
				dev_err(ctx->dev, "<%s:%d>disable lcm vci error!ret[%d]\n", __func__, __LINE__, ret);
		}
		if (regulator_is_enabled(ctx->vci_regulator)) {
			ret = regulator_disable(ctx->vci_regulator);
			if (ret < 0)
				dev_err(ctx->dev, "<%s:%d>disable lcm vci error!ret[%d]\n", __func__, __LINE__, ret);
		}
		mdelay(2);
		//DVDD 1.25V disable
		ctx->dvdd_gpio = devm_gpiod_get(ctx->dev, "dvdd", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->dvdd_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
		mdelay(150);
		//VDDI 1.8V disable
		if (regulator_is_enabled(ctx->vddi_regulator)) {
			ret = regulator_disable(ctx->vddi_regulator);
			if (ret < 0)
				dev_err(ctx->dev, "<%s:%d>disable lcm vci error!ret[%d]\n", __func__, __LINE__, ret);
		}
		if (regulator_is_enabled(ctx->vddi_regulator)) {
			ret = regulator_disable(ctx->vddi_regulator);
			if (ret < 0)
				dev_err(ctx->dev, "<%s:%d>disable lcm vci error!ret[%d]\n", __func__, __LINE__, ret);
		}
		//ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
		//gpiod_set_value(ctx->vddi_gpio, 0);
		//devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
		mdelay(2);
		lcm_unprepare_time = jiffies_to_msecs(jiffies);
	}
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}
static int panel_doze_enable(struct drm_panel *panel,
		    void *dsi, dcs_write_gce cb, void *handle)
{
//	struct lcm *ctx = panel_to_lcm(panel);
	UNUSED(panel);
	UNUSED(dsi);
	UNUSED(cb);
	UNUSED(handle);
	pr_info("[LCM] %s begin\n", __func__);
//	push_table_cb(dsi, cb, handle, aod_mode_enter_setting,
//		sizeof(aod_mode_enter_setting) / sizeof(struct LCM_setting_table));
	g_aod_enable = 1;
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}
static int panel_doze_disable(struct drm_panel *panel,
		    void *dsi, dcs_write_gce cb, void *handle)
{
//	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("[LCM] %s begin\n", __func__);
	/*aod_mode_exit_setting[0].para_list[0] = bl_tb0[1];
	aod_mode_exit_setting[0].para_list[1] = bl_tb0[2];
	push_table_cb(dsi, cb, handle, aod_mode_exit_setting,
		sizeof(aod_mode_exit_setting) / sizeof(struct LCM_setting_table));
	g_need_dim_enable = 1;*/
	UNUSED(panel);
	UNUSED(dsi);
	UNUSED(cb);
	UNUSED(handle);
	g_aod_enable = 0;
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
static int panel_set_aod_light_mode(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	unsigned int aod_mapped_level = 0;
	if (0 < level && level < 100) {
		aod_mode_enter_setting[2].para_list[0] = 0x00;
		aod_mode_enter_setting[2].para_list[1] = 0x05;
		aod_mapped_level = 0x29;  //10nit
	} else if (level >= 100 && level < 300) {
		aod_mode_enter_setting[2].para_list[0] = 0x07;
		aod_mode_enter_setting[2].para_list[1] = 0x48;
		aod_mapped_level = 0x44;  //15nit
	} else if (level >= 300 && level < 600) {
		aod_mode_enter_setting[2].para_list[0] = 0x0F;
		aod_mode_enter_setting[2].para_list[1] = 0xFE;
		aod_mapped_level = 0x13D;  //60nit
	} else {
		aod_mode_enter_setting[2].para_list[0] = 0x0F;
		aod_mode_enter_setting[2].para_list[1] = 0xFE;
		aod_mapped_level = 0x13D;  //60nit
	}
	aod_mode_enter_setting[0].para_list[0] = ((aod_mapped_level >> 8) & 0x0f);
	aod_mode_enter_setting[0].para_list[1] = (aod_mapped_level & 0xff);
	push_table_cb(dsi, cb, handle, aod_mode_enter_setting,
		sizeof(aod_mode_enter_setting) / sizeof(struct LCM_setting_table));
	pr_info("[LCM] %s level is %u, CMD 0x51's value is 0x%x 0x%x\n", __func__,
		level, aod_mode_enter_setting[2].para_list[0],
		aod_mode_enter_setting[2].para_list[1]);
	g_set_aod_light = 1;
	mapped_level = aod_mapped_level;

	return 0;
}
#endif
#if 0
static unsigned long panel_doze_get_mode_flags(struct drm_panel *panel,
	int doze_en)
{
	unsigned long mode_flags;
	pr_info("%s dbegain doze_en=%d\n", __func__, doze_en);
	if (doze_en) {
		mode_flags = MIPI_DSI_MODE_LPM
		       | MIPI_DSI_MODE_EOT_PACKET
		       | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	} else {
		mode_flags =  MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
		       MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
		       | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	}
	pr_info("[LCM] %s end\n", __func__);
	return mode_flags;
}
static int panel_doze_enable_start(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	pr_err("%s begain\n", __func__);
	return 0;
}
static int panel_doze_area(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	pr_info("%s begain\n", __func__);
	return 0;
}
static int panel_doze_post_disp_on(struct drm_panel *panel,
		void *dsi, dcs_write_gce cb, void *handle)
{
	//struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s begain\n", __func__);
	//lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	//lcm_dcs_write_seq_static(ctx, 0x39);
#ifdef VENDOR_EDIT
/* Hujie@PSW.MM.DisplayDriver.AOD, 2019/12/10, add for keylog*/
	pr_info("debug for lcm %s\n", __func__);
#endif
	return 0;
}
static int panel_set_aod_light_mode(void *dsi,
	dcs_write_gce cb, void *handle, unsigned int mode)
{
	pr_info("debug for lcm %s\n", __func__);
	//memset(send_cmd, 0, RAMLESS_AOD_PAYLOAD_SIZE);
	return 0;
}
#endif

//#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
#if 0
static int panel_lcm_pcd_check_by_read_lcm(void *dsi, tran_lcm_read read, dcs_write_gce cb, void *handle)
{
	u8 i = 0, j = 0, ret = 0;
	u8 ret_dlen = 0;
	u8 tx[2] = {0xD0};
	u8 read_num = 1;
	static struct LCM_setting_table lcm_pcd_enable_lcm_setting[] = {
		{0xF0,2,{0xAA,0x12}},
		{0xD0,2,{0x08,0x02}}, // BIST Enable
		{REGFLAG_DELAY, 100, {}},
		{0x11,0,{}},
		{0xF0,2,{0xAA,0x12}},
		{0x65,1,{0x04}},
	};
	static struct LCM_setting_table lcm_pcd_disable_lcm_setting[] = {
		{0xF0,2,{0xAA,0x12}},
		{0xD0,2,{0x08,0x00}}, //Disable
	};
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (!cmd_msg)
		return 0;
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
	cmd_msg->rx_buf[0] = kmalloc(4 * sizeof(unsigned char),
		GFP_ATOMIC);
	memset(cmd_msg->rx_buf[0], 0, 4);
	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;
	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_len[0] = read_num;
	push_table_cb(dsi, cb, handle, lcm_pcd_enable_lcm_setting,
		sizeof(lcm_pcd_enable_lcm_setting) / sizeof(struct LCM_setting_table));
	ret = read(cmd_msg);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
	}
	for (i = 0; i < cmd_msg->rx_cmd_num; i++) {
		ret_dlen = cmd_msg->rx_len[i];
		pr_info("%s:0x%x--dlen:%d--cmd_idx:%d\n",__func__,
			*(char *)(cmd_msg->tx_buf[i]), ret_dlen, i);
		for (j = 0; j < ret_dlen; j++) {
			pr_info("%s:0x%x--byte:%02d,val:0x%x\n",__func__,
				*(char *)(cmd_msg->tx_buf[i]), j,
				*(char *)(cmd_msg->rx_buf[i] + j));
		}
	}
	push_table_cb(dsi, cb, handle, lcm_pcd_disable_lcm_setting,
		sizeof(lcm_pcd_disable_lcm_setting) / sizeof(struct LCM_setting_table));
	if (((*(char *)(cmd_msg->rx_buf[0]))  >= 0x81) && ((*(char *)(cmd_msg->rx_buf[0]))  <= 0xD3)) {
		for (i = 0; i < cmd_msg->rx_cmd_num; i++)
			kfree(cmd_msg->rx_buf[i]);
		vfree(cmd_msg);
		return 1;
	}
	pr_info("%s read data is 0x%x\n", __func__,
			*(char *)(cmd_msg->rx_buf[0]));
	for (i = 0; i < cmd_msg->rx_cmd_num; i++)
		kfree(cmd_msg->rx_buf[i]);
	vfree(cmd_msg);
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
//HBM function for fp unlock
static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
		    dcs_write_gce cb, void *handle, bool en)
{
	struct lcm *ctx = panel_to_lcm(panel);
	if (!cb)
		return -1;
	if (ctx->hbm_en == en)
		goto done;
	pr_info("[LCM] %s FPS=%dHz en=%d, mapped_level=%d, aod=%d \n", __func__, g_lcm_fresh_mode, en, mapped_level, g_aod_enable);

	if(en) {
		if (g_lcm_fresh_mode == MODE_4_FPS)
			push_table_cb(dsi, cb, handle, aodmode_hbm_mode_enter_setting,
				sizeof(aodmode_hbm_mode_enter_setting) / sizeof(struct LCM_setting_table));
		else
			push_table_cb(dsi, cb, handle, hbm_mode_enter_setting,
				sizeof(hbm_mode_enter_setting) / sizeof(struct LCM_setting_table));
	}else {
		push_table_cb(dsi, cb, handle, hbm_mode_exit_setting,
			sizeof(hbm_mode_exit_setting) / sizeof(struct LCM_setting_table));
		if (lcm_longh30hz_support) {
			if (g_aod_enable) {
				push_table_cb(dsi, cb, handle, aod_mode_enter_setting,
					sizeof(aod_mode_enter_setting) / sizeof(struct LCM_setting_table));
			}
		}
	}
	ctx->hbm_en = en;
	ctx->hbm_wait = true;
	g_hbm_enable = ctx->hbm_en;
done:
	return 0;
}
//HBM node function
static int panel_hbm_set_cmdq_switch(struct drm_panel *panel, void *dsi,
		    dcs_write_gce cb, void *handle, bool en)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	pr_info("[LCM] %s FPS=%dHz en=%d, mapped_level=%d, aod=%d \n", __func__, g_lcm_fresh_mode, en, mapped_level, g_aod_enable);

	if(en) {
		if (g_lcm_fresh_mode == MODE_4_FPS)
			push_table_cb(dsi, cb, handle, aodmode_hbm_mode_enter_setting,
				sizeof(aodmode_hbm_mode_enter_setting) / sizeof(struct LCM_setting_table));
		else
			push_table_cb(dsi, cb, handle, hbm_mode_enter_setting,
				sizeof(hbm_mode_enter_setting) / sizeof(struct LCM_setting_table));
	}else {
		push_table_cb(dsi, cb, handle, hbm_mode_exit_setting,
			sizeof(hbm_mode_exit_setting) / sizeof(struct LCM_setting_table));
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
//for sensor calibration enable/disable dim func 20230928 start
#if 0
static int tran_ddic_cmd_set_cmdq(struct drm_panel *panel, void *dsi,
		    dcs_write_gce cb, void *handle, unsigned long index)
{
	switch (index) {
		case 1024:  //disable 0x51 dim func for sensor calibration
			bl_dim[1] = 0x20; //disable ddic dim func
			cb(dsi, handle, bl_dim, ARRAY_SIZE(bl_dim));
			cmdq_pkt_sleep(handle, 26000*17, 0x03); //delay one frame, make sure dim cmd is effective
			g_dim_enable = 0;
			g_need_dim_enable = 0;
			pr_info("%s index=%ld disable ddic dim, fps=%d, aod=%d\n",__func__, index, g_lcm_fresh_mode, g_aod_enable);
			break;
		case 2048:  //enable 0x51 dim func for sensor calibration
			bl_dim[1] = 0x28; //enable ddic dim func
			cb(dsi, handle, bl_dim, ARRAY_SIZE(bl_dim));
			cmdq_pkt_sleep(handle, 26000*17, 0x03); //delay one frame, make sure dim cmd is effective
			pr_info("%s index=%ld enable ddic dim, fps=%d, aod=%d\n",__func__, index, g_lcm_fresh_mode, g_aod_enable);
			break;
		default:
			pr_info("%s index=%ld undefined value, fps=%d, aod=%d\n",__func__, index, g_lcm_fresh_mode, g_aod_enable);
			break;
	}
	return 0;
}
#endif
//for sensor calibration enable/disable dim func 20230928 end
static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
	.set_pcd_cmdq = lcm_set_pcd_cmdq,
#endif
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	//.mode_switch = mode_switch,
	.reset = panel_ext_reset,
	.lcm_power_set = panel_ext_lcm_power_set,
	/* add for ramless HBM */
	.hbm_set_cmdq = panel_hbm_set_cmdq,
	.hbm_set_cmdq_switch = panel_hbm_set_cmdq_switch,
	.hbm_get_state = panel_hbm_get_state,
	.hbm_get_wait_state = panel_hbm_get_wait_state,
	.hbm_set_wait_state = panel_hbm_set_wait_state,
	/* add for ramless AOD */
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
	/*.doze_get_mode_flags = panel_doze_get_mode_flags,
	.doze_enable_start = panel_doze_enable_start,
	.doze_area = panel_doze_area,
	.doze_post_disp_on = panel_doze_post_disp_on,*/
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
	.set_aod_light_mode = panel_set_aod_light_mode,
#endif
//for sensor calibration enable/disable dim func 20230928 start
	//.tran_ddic_cmd_set_cmdq = tran_ddic_cmd_set_cmdq,
//for sensor calibration enable/disable dim func 20230928 end
	//#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
	#if 0
	.lcm_pcd_check_by_read_lcm = panel_lcm_pcd_check_by_read_lcm,
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
	struct drm_display_mode *mode_144hz;
	struct drm_display_mode *mode_30hz;
	UNUSED(panel);
	pr_info("[LCM] %s begin\n", __func__);
	mode_144hz = drm_mode_duplicate(connector->dev, &switch_mode_144hz);
	if (!mode_144hz) {
		dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_144hz.hdisplay,
			switch_mode_144hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_144hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_144hz);
	mode_144hz->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_144hz);
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
	mode_60hz->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(connector, mode_60hz);
	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 157;

	if (lcm_longh30hz_support) {
		mode_30hz = drm_mode_duplicate(connector->dev, &switch_mode_30hz);
		if (!mode_30hz) {
			dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
				switch_mode_30hz.hdisplay,
				switch_mode_30hz.vdisplay,
				drm_mode_vrefresh(&switch_mode_30hz));
			return -ENOMEM;
		}
		drm_mode_set_name(mode_30hz);
		mode_30hz->type = DRM_MODE_TYPE_DRIVER ;
		drm_mode_probed_add(connector, mode_30hz);
		pr_info("[LCM] %s end add support longh30hz\n", __func__);
		return 5;
	}
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
static struct lcm *ctx_work = NULL;
static void lcm_regulator_power_init(struct work_struct *lcm_work)
{
	int ret = 0;
	pr_info("[LCM]%s start\n", __func__);
	UNUSED(lcm_work);
	//vci 3.0v by mt6369
	ctx_work->vci_regulator = devm_regulator_get(ctx_work->dev, "vci");
	if (IS_ERR_OR_NULL(ctx_work->vci_regulator)) {
		pr_info("<%s,%d> lcm vci regulator get fail!\n", __func__, __LINE__);
		return ;
	}
	ret = regulator_set_voltage(ctx_work->vci_regulator, 3000000, 3000000);
	if (ret < 0)
		pr_info("<%s,%d> lcm vci set vol fail!ret[%d]\n", __func__, __LINE__, ret);
	ret = regulator_enable(ctx_work->vci_regulator);
	if (ret < 0)
		pr_info("<%s,%d> lcm vci enable error!ret[%d]\n", __func__, __LINE__, ret);
	//dvdd 1.25V by ET5904
	ctx_work->vddi_regulator = devm_regulator_get(ctx_work->dev, "vddi");
	if (IS_ERR_OR_NULL(ctx_work->vddi_regulator)) {
		pr_info("<%s,%d> lcm vddi regulator get fail!\n", __func__, __LINE__);
		return ;
	}
	ret = regulator_set_voltage(ctx_work->vddi_regulator, 1800000, 1800000); //0.6+0.006x = y, y=1.254, x is integer value
	if (ret < 0)
		pr_info("<%s,%d> lcm vddi set vol fail!ret[%d]\n", __func__, __LINE__, ret);
	ret = regulator_enable(ctx_work->vddi_regulator);
	if (ret < 0)
		pr_info("<%s,%d> lcm vddi enable error!ret[%d]\n", __func__, __LINE__, ret);

	cpu_qos_min_write(cpu, 0);//lcm_probe
	if(g_lcm_fresh_mode == MODE_0_FPS)
		cpu_qos_update(min_freq);
}
static DECLARE_DELAYED_WORK(lcm_regulator_work, lcm_regulator_power_init);
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
	pr_info("[LCM] %s lcm_doze_backlight:%d;%d;%d;%d;\n", __func__, g_lcm_doze_backlight.doze_backlight_num, g_lcm_doze_backlight.doze_backlight_level1, g_lcm_doze_backlight.doze_backlight_level2, g_lcm_doze_backlight.doze_backlight_level3);
	panel_driver_status.lcm_doze_backlight = &g_lcm_doze_backlight;
#endif
	panel_driver_status.dimming_status = DIMMING_DEFAULT_STATUS;	// default dimming status
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mipi_dsi_set_drvdata(dsi, ctx);
	ctx_work = ctx;
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_LPM;
			/*MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET*/
			//MIPI_DSI_CLOCK_NON_CONTINUOUS;
	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);
		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}
	ctx->dvdd_gpio = devm_gpiod_get(dev, "dvdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_dbg(dev, "cannot get dvdd-gpios %ld\n", PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	devm_gpiod_put(dev, ctx->dvdd_gpio);
	schedule_delayed_work(&lcm_regulator_work, msecs_to_jiffies(10000));  //after 10s, call lcm_regulator_power_init func
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_dbg(dev, "cannot get reset-gpios %ld\n", PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	if (of_property_read_bool(dev->of_node, "lcm-longh30hz-support")) {
		lcm_longh30hz_support = true;
		pr_info("panel support longh 30hz\n");
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

	if(!IS_ERR_OR_NULL(qos_req.qos))
	{
		freq_qos_remove_request(&qos_req);
		pr_info("[LCM] Released QoS request for CPU %d\n", cpu);
	}
}
static void lcm_shutdown(struct mipi_dsi_device *dsi)
{
	int ret = 0;
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
	pr_info("[LCM] %s begin\n", __func__);
	lcm_disable(&ctx->panel);
	if(ctx->prepared) {
		lcm_dcs_write_seq_static(ctx, 0x28);
		lcm_dcs_write_seq_static(ctx, 0x10);
		mdelay(120);
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		mdelay(2);
		if (IS_ERR_OR_NULL(ctx->vci_regulator)) {
			ctx->vci_regulator = devm_regulator_get(ctx->dev, "vci");
			if (IS_ERR_OR_NULL(ctx->vci_regulator)) {
				ret = PTR_ERR(ctx->vci_regulator);
				dev_err(ctx->dev, "<%s,%d> lcm vci get fail!ret[%d].\n", __func__, __LINE__, ret);
				return;
			}
		}
		if (IS_ERR_OR_NULL(ctx->vddi_regulator)) {
			ctx->vddi_regulator = devm_regulator_get(ctx->dev, "vddi");
			if (IS_ERR_OR_NULL(ctx->vddi_regulator)) {
				ret = PTR_ERR(ctx->vddi_regulator);
				dev_err(ctx->dev, "<%s,%d> lcm vddi get fail!ret[%d].\n", __func__, __LINE__, ret);
				return;
			}
		}
		if (regulator_is_enabled(ctx->vci_regulator))
		{
			ret = regulator_disable(ctx->vci_regulator);
			if (regulator_is_enabled(ctx->vci_regulator))
				dev_err(ctx->dev, "<%s>disable vci_regulator error!ret[%d]\n", __func__, ret);
 		}
		mdelay(2);
		ctx->dvdd_gpio = devm_gpiod_get(ctx->dev, "dvdd", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->dvdd_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
		mdelay(150);
		if (regulator_is_enabled(ctx->vddi_regulator)) {
			ret = regulator_disable(ctx->vddi_regulator);
			if (ret < 0)
				dev_err(ctx->dev, "<%s:%d>disable lcm vci error!ret[%d]\n", __func__, __LINE__, ret);
		}
		mdelay(2);
	}

	if(!IS_ERR_OR_NULL(qos_req.qos))
	{
		freq_qos_remove_request(&qos_req);
		pr_info("[LCM] Released QoS request for CPU %d\n", cpu);
	}
}
static const struct of_device_id lcm_of_match[] = {
	{
		.compatible = "nt37706a,fhdp,dsi,vdo,dsc,boe,144hz,cn5c",
	},
	{} };
MODULE_DEVICE_TABLE(of, lcm_of_match);
static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.shutdown = lcm_shutdown,
	.driver = {
			.name =  "nt37706a_fhdp_dsi_vdo_dsc_boe_boe_144hz_cn5c",
			.owner = THIS_MODULE,
			.of_match_table = lcm_of_match,
		},
};
module_mipi_dsi_driver(lcm_driver);
MODULE_AUTHOR("bing.he<bing.he@transsion.com>");
MODULE_DESCRIPTION("VTDR6126A VDO 144hz AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
