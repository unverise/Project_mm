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
#define FRAME_HEIGHT                2436
#define PHYSICAL_WIDTH              69832
#define PHYSICAL_HEIGHT             157511

#define HSA                         4
#define HBP                         20
#define VSA                         4
#define VBP                         16

#define PANEL_CLOCK                 595
#define PANEL_DATA_RATE             1190

/*Parameter setting for mode 0 Start*/
#define MODE_0_FPS                  60
#define MODE_0_VFP                  2588
#define MODE_0_HFP                  173
/*Parameter setting for mode 0 End*/

/*Parameter setting for mode 2 Start*/
#define MODE_2_FPS                  120
#define MODE_2_VFP                  64
#define MODE_2_HFP                  173
/*Parameter setting for mode 2 End*/

/*Parameter setting for mode 3 Start*/
#define MODE_3_FPS                  144
#define MODE_3_VFP                  104
#define MODE_3_HFP                  56
/*Parameter setting for mode 3 End*/

/* DSC RELATED */
#define DSC_ENABLE                  1
#define DSC_VER                     17
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 40 //8bit: 34, 10bit: 40
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         10 //8bit: 8, 10bit: 10
#define DSC_DSC_LINE_BUF_DEPTH      11 //8bit: 9, 10bit: 11
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128
//define DSC_PIC_HEIGHT
//define DSC_PIC_WIDTH
#define DSC_SLICE_HEIGHT            12
#define DSC_SLICE_WIDTH             540
#define DSC_CHUNK_SIZE              540
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               526
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      287
#define DSC_DECREMENT_INTERVAL      7
#define DSC_LINE_BPG_OFFSET         12
#define DSC_NFL_BPG_OFFSET          2235
#define DSC_SLICE_BPG_OFFSET        2170
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            4336
#define DSC_FLATNESS_MINQP          7 //8bit: 3, 10bit: 10
#define DSC_FLATNESS_MAXQP          16 //8bit: 12, 10bit: 16
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    15 //8bit: 11, 10bit: 15
#define DSC_RC_QUANT_INCR_LIMIT1    15 //8bit: 11, 10bit: 15
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3
static unsigned int rc_buf_thresh[14] = {
	896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064};
unsigned int range_min_qp[15] = {0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 11, 17};
unsigned int range_max_qp[15] = {8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 15, 16, 17, 17, 19};
int range_bpg_ofs[15] = {2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12};
#endif

/* define default dimming status*/
#define DIMMING_DEFAULT_STATUS     100

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */

/* option function to read data from some panel address */
/* #define PANEL_SUPPORT_READBACK */

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vci_gpio;
	struct regulator *dvdd_regulator;
	struct gpio_desc *vddi_gpio;

	bool prepared;
	bool enabled;

	int error;

	bool hbm_en;
	bool hbm_wait;
};
struct tran_panel_driver_params panel_driver_status = {0};
static char switch_page[] = {0xFE, 0x00}; //switch ddic page,default page 0
static char bl_tb0[] = {0x51, 0x0C, 0xF3}; //normal max 51_DBV
static int g_aod_enable = 0;
static int g_hbm_enable = 0;
static int g_lcm_fresh_mode = MODE_2_FPS;
static unsigned int mapped_level = 0, input_brightness = 0, last_mapped_level = 0;
//add ddic_dim function control logic 20230911 start
static char bl_dim[] = {0x53, 0x28}; //enable ddic dim function
static unsigned int g_dim_enable = 0; //record dim state
static unsigned int g_need_dim_enable = 0; //if or not need dim function
//add ddic_dim function control logic 20230911 end
static int g_set_aod_light = 0;

static s32 cpu = 0, min_freq = 1300000;
static struct freq_qos_request qos_req;
extern int freq_qos_add_request(struct freq_constraints *qos,struct freq_qos_request *req, enum freq_qos_req_type type, s32 value);
extern int freq_qos_remove_request(struct freq_qos_request *req);
extern int freq_qos_update_request(struct freq_qos_request *req, s32 new_value);

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
	{0xFE, 1, {0x00}}, //page 0
	{0x2F, 1, {0x05}}, //support 144Hz:2F00, support 120Hz/60Hz:2F05

//opt ddic dim function && effect 20230911 start
	{0xFE,1,{0x98}},
	{0x20,1,{0x03}},
	{0xFE,1,{0x70}},
	{0x0D,1,{0x33}},
	{0xFE,1,{0x70}},
	{0x0E,1,{0x03}},
	{0xFE,1,{0x00}},
	{0x53,1,{0x20}},
//opt ddic dim function && effect 20230911 end

//apha hbm start
	{0xFE,1,{0x6D}},
	{0x90,1,{0x80}},   //rb_circle_dbv_dis=0, 參考下方 rb_rcn_dbv_lut_cXX[4:0] 調整漸層亮度
					   //rb_circle_dbv_dis=1, 參考 rb_circle_dbv[5:0] 調整漸層亮度
	{0xfe,1,{0x40}},
	{0xBB,1,{0x1F}},   //rb_FPS_area_sel = 1  (確認下原本設定值)

	{0xfe,1,{0x16}},
	{0x35,1,{0x00}},

	{0x0D,1,{0x00}},   //rcn_dbv_lut_c00;	rcn_dbv = d'0  (h'0)
	{0x0E,1,{0x05}},   //rcn_dbv_lut_c01;	rcn_dbv = d'112  (h'70)
	{0x0F,1,{0x06}},   //rcn_dbv_lut_c02;	rcn_dbv = d'176  (h'0B0)
	{0x10,1,{0x07}},   //rcn_dbv_lut_c03;	rcn_dbv = d'240  (h'0F0)
	{0x11,1,{0x08}},   //rcn_dbv_lut_c04;	rcn_dbv = d'304  (h'130)
	{0x12,1,{0x09}},   //rcn_dbv_lut_c05;	rcn_dbv = d'368  (h'170)
	{0x13,1,{0x0A}},   //rcn_dbv_lut_c06;	rcn_dbv = d'432  (h'1B0)
	{0x14,1,{0x0A}},   //rcn_dbv_lut_c07;	rcn_dbv = d'496  (h'1F0)
	{0x15,1,{0x0B}},   //rcn_dbv_lut_c08;	rcn_dbv = d'560  (h'230)
	{0x16,1,{0x0B}},   //rcn_dbv_lut_c09;	rcn_dbv = d'624  (h'270)
	{0x17,1,{0x0C}},   //rcn_dbv_lut_c10;	rcn_dbv = d'752  (h'2F0)
	{0x18,1,{0x0D}},   //rcn_dbv_lut_c11;	rcn_dbv = d'880  (h'370)
	{0x19,1,{0x0E}},   //rcn_dbv_lut_c12;	rcn_dbv = d'1008  (h'3F0)
	{0x1A,1,{0x10}},   //rcn_dbv_lut_c13;	rcn_dbv = d'1264  (h'4F0)
	{0x1B,1,{0x11}},   //rcn_dbv_lut_c14;	rcn_dbv = d'1520  (h'5F0)
	{0x1C,1,{0x12}},   //rcn_dbv_lut_c15;	rcn_dbv = d'1776  (h'6F0)
	{0x1D,1,{0x14}},   //rcn_dbv_lut_c16;	rcn_dbv = d'2032  (h'7F0)
	{0x1E,1,{0x15}},   //rcn_dbv_lut_c17;	rcn_dbv = d'2288  (h'8F0)
	{0x1F,1,{0x16}},   //rcn_dbv_lut_c18;	rcn_dbv = d'2544  (h'9F0)
	{0x20,1,{0x17}},   //rcn_dbv_lut_c19;	rcn_dbv = d'2800  (h'AF0)
	{0x21,1,{0x18}},   //rcn_dbv_lut_c20;	rcn_dbv = d'3056  (h'BF0)
	{0x22,1,{0x18}},   //rcn_dbv_lut_c21;	rcn_dbv = d'3312  (h'CF0)
	{0x23,1,{0x1B}},   //rcn_dbv_lut_c22;	rcn_dbv = d'3568  (h'DF0)
	{0x24,1,{0x1E}},   //rcn_dbv_lut_c23;	rcn_dbv = d'3824  (h'EF0)
	{0x25,1,{0x1F}},   //rcn_dbv_lut_c24;	rcn_dbv = d'4095  (h'FFF)

	{0xFE,1,{0x6D}},
	{0x96,1,{0x03}},

	{0xFE,1,{0x53}},
	{0xD9,1,{0x00}},  //GAM_D_FPS_en=1

	//Set FPS gamma data_by user
	{0xFE,1,{0x42}},
	{0xA2,1,{0x27}},	 //gam_ram_ctrl_fps_page_sel
						 //FPS區域 gam sel (from gam ram)
	{0x96,1,{0x0A}},
	{0x82,1,{0xE6}},
	{0x83,1,{0xCC}},   //FPS_en_gHBM_auto_th[12:0]= 6630_T9 (h'0CF3)
	{0x87,1,{0x60}},
	{0x1B,1,{0x10}},  //DBVdim_halt_wffps = d'0

	{0xFE,1,{0x70}},
	{0x29,1,{0x01}},//bc_alpha_mode = 1'b1
	{0x20,1,{0x62}},//FPS_BCd_full = 1'b1
	{0x1B,1,{0x03}},//BC_alpha_SW[2:0]

	{0xFE,1,{0x9C}},
	{0x00,1,{0x13}},
	{0x34,1,{0x13}},
	{0xA0,1,{0x13}},

	{0xFE,1,{0x4A}},
	{0x01,1,{0x3C}},
	{0x21,1,{0x83}},
	{0x22,1,{0x00}},
	{0x23,1,{0x01}},
	{0x24,1,{0x7B}},
	{0x25,1,{0x4A}},
	{0x26,1,{0x0A}},

	{0x27,1,{0x83}},
	{0x28,1,{0x00}},
	{0x29,1,{0x00}},
	{0x2A,1,{0x85}},
	{0x2B,1,{0x4A}},
	{0x2D,1,{0x0D}},

	{0x7B,1,{0xFE}},
	{0x7C,1,{0x9A}},
	{0x7D,1,{0x58}},
	{0x7E,1,{0x00}},
	{0x7F,1,{0xFE}},
	{0x80,1,{0x9B}},
	{0x81,1,{0x22}},
	{0x82,1,{0x41}},
	{0x83,1,{0xFE}},
	{0x84,1,{0x00}},

	{0x85,1,{0xFF}},
	{0x86,1,{0x01}},
	{0x87,1,{0x02}},
	{0x88,1,{0xFE}},
	{0x89,1,{0x9A}},
	{0x8A,1,{0x58}},
	{0x8B,1,{0x40}},
	{0x8C,1,{0xFE}},
	{0x8D,1,{0x9B}},
	{0x8E,1,{0x22}},
	{0x8F,1,{0x11}},
	{0x90,1,{0xFE}},
	{0x91,1,{0x00}},
//apha hbm end

	{0xFE,1,{0x49}},
	{0x7B,1,{0x77}},
	{0x7C,1,{0x77}},
	{0x7D,1,{0x77}},
	{0x7E,1,{0x77}},
	{0x7F,1,{0x77}},
	{0xFE,1,{0xA0}},
	{0x5D,1,{0x46}},
	{0x64,1,{0x08}},
	{0x17,1,{0x01}},
	{0x5F,1,{0x40}},
	{0xFE,1,{0x49}},
	{0x6C,1,{0x07}},
	{0x69,1,{0x07}},
	{0x6A,1,{0x07}},
	{0x6B,1,{0x07}},
	{0x73,1,{0x29}},
	{0x74,1,{0x29}},
	{0x75,1,{0x29}},
	{0x76,1,{0x1F}},
	{0xFE,1,{0x62}},
	{0x05,1,{0x0D}},
	{0x06,1,{0x49}},
	{0x07,1,{0x46}},
	{0xFE,1,{0x60}},
	{0x05,1,{0x0D}},
	{0x06,1,{0x4A}},
	{0x07,1,{0x4D}},
	{0xFE,1,{0x62}},
	{0x30,1,{0x0B}},
	{0xFE,1,{0x60}},
	{0x20,1,{0x0C}},
	{0x21,1,{0x4A}},
	{0x22,1,{0x4D}},
	{0xFE,1,{0x62}},
	{0x36,1,{0x0A}},
	{0xFE,1,{0x60}},
	{0x88,1,{0x0F}},
	{0x89,1,{0x4A}},
	{0xFE,1,{0x60}},
	{0x3F,1,{0x05}},
	{0x40,1,{0x4A}},
	{0x41,1,{0x20}},
	{0xFE,1,{0x62}},
	{0x3D,1,{0x07}},
	{0xFE,1,{0x62}},
	{0x18,1,{0x5D}},
	{0x19,1,{0x5D}},
	{0xFE,1,{0x60}},
	{0x18,1,{0x5E}},
	{0x19,1,{0x62}},
	{0xFE,1,{0x60}},
	{0x35,1,{0x5E}},
	{0x36,1,{0x61}},
	{0xFE,1,{0x60}},
	{0x9B,1,{0x5F}},
	{0xFE,1,{0x60}},
	{0x52,1,{0x5D}},
	{0x53,1,{0x28}},

	{0xFE,1,{0xA1}},
	{0x58,1,{0x67}},
	{0xFE,1,{0xA1}},
	{0xB9,1,{0x90}},
	{0xFE,1,{0x00}},
	{0xFA,1,{0x01}},//vesa on
	//{0x2f,{1,0x05}},//default te: 120hz(2F05)
	{0xFE,1,{0x00}},//page 0
	{0xC1,1,{0x03}},// By pass RAM
	{0xC2,1,{0x03}},//video mode
	{0x35,1,{0x00}},//TE on
	{0x51,2,{0x00,0x00}}, //max normal 550nit:0x7FF

//10bit code 20240329 start
#if 1
	{0xFE,1,{0xD2}}, //Page D2
	{0x97,1,{0x08}}, //ENG PPS

	{0x36,1,{0x11}},
	{0x39,1,{0xAB}},
	{0x3A,1,{0x30}},
	{0x3B,1,{0x80}},
	{0x3D,1,{0x09}},
	{0x3F,1,{0x84}},
	{0x40,1,{0x04}},
	{0x41,1,{0x38}},
	{0x42,1,{0x00}},
	{0x43,1,{0x0C}},
	{0x44,1,{0x02}},
	{0x45,1,{0x1C}},
	{0x46,1,{0x02}},
	{0x47,1,{0x1C}},
	{0x48,1,{0x02}},
	{0x49,1,{0x00}},
	{0x4A,1,{0x02}},
	{0x4B,1,{0x0E}},
	{0x4D,1,{0x20}},
	{0x4E,1,{0x01}},
	{0x4F,1,{0x1F}},

	{0x50,1,{0x00}},
	{0x51,1,{0x07}},
	{0x53,1,{0x0C}},
	{0x54,1,{0x08}},
	{0x55,1,{0xBB}},
	{0x56,1,{0x08}},
	{0x58,1,{0x7A}},
	{0x59,1,{0x18}},
	{0x5A,1,{0x00}},
	{0x5B,1,{0x10}},
	{0x5C,1,{0xF0}},
	{0x5D,1,{0x07}},
	{0x5E,1,{0x10}},
	{0x5F,1,{0x20}},
	{0x60,1,{0x00}},
	{0x61,1,{0x06}},
	{0x62,1,{0x0F}},
	{0x63,1,{0x0F}},
	{0x64,1,{0x33}},
	{0x65,1,{0x0E}},
	{0x66,1,{0x1C}},
	{0x67,1,{0x2A}},
	{0x68,1,{0x38}},
	{0x69,1,{0x46}},
	{0x6A,1,{0x54}},
	{0x6B,1,{0x62}},
	{0x6C,1,{0x69}},
	{0x6D,1,{0x70}},
	{0x6E,1,{0x77}},
	{0x6F,1,{0x79}},
	{0x70,1,{0x7B}},
	{0x71,1,{0x7D}},
	{0x72,1,{0x7E}},
	{0x73,1,{0x02}},
	{0x74,1,{0x02}},
	{0x75,1,{0x22}},
	{0x76,1,{0x00}},
	{0x77,1,{0x2A}},
	{0x78,1,{0x40}},
	{0x79,1,{0x2A}},
	{0x7A,1,{0xBE}},
	{0x7B,1,{0x3A}},

	{0x7C,1,{0xFC}},
	{0x7D,1,{0x3A}},
	{0x7E,1,{0xFA}},
	{0x7F,1,{0x3A}},
	{0x80,1,{0xF8}},
	{0x81,1,{0x3B}},
	{0x82,1,{0x38}},
	{0x83,1,{0x3B}},
	{0x84,1,{0x78}},
	{0x85,1,{0x3B}},
	{0x86,1,{0xB6}},
	{0x87,1,{0x4B}},
	{0x88,1,{0xF6}},
	{0x89,1,{0x4C}},
	{0x8A,1,{0x34}},
	{0x8B,1,{0x4C}},
	{0x8C,1,{0x74}},
	{0x8D,1,{0x5C}},
	{0x8E,1,{0x74}},
	{0x8F,1,{0x8C}},
	{0x90,1,{0xF4}},
	{0x91,1,{0x00}},
	{0x92,1,{0x00}},
	{0x93,1,{0x00}},
	{0x94,1,{0x00}},
	{0x95,1,{0x00}},
	{0x96,1,{0x00}},
#endif
//10bit code 20240329 end

	{0xFE,1,{0x00}},
	{0x11, 0, {}},	//Sleep out
	{REGFLAG_DELAY, 120, {}},
	//Display on
	{0x29, 0, {}},	//Display on
	{REGFLAG_DELAY, 20,{}},
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	// Display off sequence
	{0x28, 0, {}},
	{REGFLAG_DELAY, 10, {}},
	// Sleep Mode On
	{0x10, 0, {}},
	{REGFLAG_DELAY, 100, {}},
};
/* //AOD brightness 3level
static struct LCM_setting_table aod_mode_enter_setting[] = {
	{0xFE,1,{0x00}}, //page 0
	{0x51,2,{0x01,0x6A}}, //60nit
	//{0x51,2,{0x00,0x20}}, //5nit
};

static struct LCM_setting_table aod_mode_exit_setting[] = {
	{0xFE,1,{0x00}}, //page 0
	{0x51,2,{0x01,0x6A}}, //60nit
	//{0x51,2,{0x00,0x20}}, //5nit
	{REGFLAG_DELAY, 17, {}},
};
*/
static struct LCM_setting_table hbm_mode_enter_setting[] = {
	{0xFE,1,{0x00}}, //page 0
	{0x83,3,{0x01,0x7F,0x1F}},//Dynamically follow DBV
};

static struct LCM_setting_table hbm_mode_exit_setting[] = {
	{0xFE,1,{0x00}}, //page 0
	{0x83,1,{0x00}},
};

static unsigned int brightness_mapped_to_alpha[256][2]= {
	{0x00,0x00},{0x0A,0x1C},{0x0E,0x07},{0x11,0x0C},{0x13,0x1A},{0x15,0x12},{0x17,0x04},{0x18,0x19},{0x1A,0x09},{0x1B,0x19},{0x1D,0x0B},{0x1E,0x1B},{0x20,0x04},{0x21,0x0A},{0x22,0x10},{0x23,0x14},
	{0x24,0x16},{0x25,0x17},{0x26,0x17},{0x27,0x16},{0x28,0x16},{0x29,0x12},{0x2A,0x10},{0x2B,0x0A},{0x2C,0x00},{0x2C,0x19},{0x2D,0x10},{0x2E,0x06},{0x2E,0x1C},{0x2F,0x10},{0x30,0x0A},{0x31,0x02},
	{0x31,0x1A},{0x32,0x12},{0x33,0x08},{0x34,0x00},{0x34,0x16},{0x35,0x0F},{0x35,0x1D},{0x36,0x0B},{0x36,0x19},{0x37,0x07},{0x37,0x15},{0x38,0x04},{0x38,0x14},{0x39,0x03},{0x39,0x10},{0x3A,0x00},
	{0x3A,0x0E},{0x3A,0x1C},{0x3B,0x0C},{0x3B,0x1A},{0x3C,0x0A},{0x3C,0x16},{0x3D,0x06},{0x3D,0x14},{0x3E,0x02},{0x3E,0x10},{0x3F,0x00},{0x3F,0x0F},{0x40,0x00},{0x40,0x10},{0x41,0x00},{0x41,0x10},
	{0x42,0x00},{0x42,0x10},{0x43,0x02},{0x43,0x12},{0x44,0x02},{0x44,0x12},{0x45,0x02},{0x45,0x12},{0x46,0x02},{0x46,0x12},{0x47,0x02},{0x47,0x12},{0x48,0x05},{0x48,0x17},{0x49,0x07},{0x49,0x17},
	{0x4A,0x07},{0x4A,0x18},{0x4B,0x07},{0x4B,0x19},{0x4C,0x09},{0x4C,0x1B},{0x4D,0x0A},{0x4D,0x1B},{0x4E,0x0B},{0x4E,0x1C},{0x4F,0x0C},{0x50,0x00},{0x50,0x10},{0x51,0x03},{0x51,0x07},{0x51,0x14},
	{0x51,0x1B},{0x52,0x05},{0x52,0x10},{0x52,0x18},{0x53,0x00},{0x53,0x07},{0x53,0x10},{0x53,0x17},{0x54,0x00},{0x54,0x0A},{0x54,0x14},{0x54,0x1B},{0x55,0x02},{0x55,0x0B},{0x55,0x12},{0x55,0x1C},
	{0x56,0x04},{0x56,0x0D},{0x56,0x15},{0x56,0x1E},{0x57,0x0C},{0x57,0x15},{0x57,0x1E},{0x58,0x08},{0x58,0x10},{0x58,0x17},{0x59,0x01},{0x59,0x09},{0x59,0x11},{0x59,0x19},{0x5A,0x02},{0x5A,0x0A},
	{0x5A,0x13},{0x5A,0x1C},{0x5B,0x04},{0x5B,0x0D},{0x5B,0x14},{0x5B,0x1E},{0x5C,0x05},{0x5C,0x0F},{0x5C,0x16},{0x5D,0x00},{0x5D,0x08},{0x5D,0x11},{0x5D,0x1F},{0x5E,0x08},{0x5E,0x10},{0x5E,0x17},
	{0x5F,0x00},{0x5F,0x09},{0x5F,0x13},{0x5F,0x1A},{0x60,0x04},{0x60,0x0E},{0x60,0x16},{0x61,0x00},{0x61,0x08},{0x61,0x13},{0x61,0x1C},{0x62,0x03},{0x62,0x0E},{0x62,0x17},{0x63,0x00},{0x63,0x08},
	{0x63,0x12},{0x63,0x1A},{0x64,0x04},{0x64,0x12},{0x64,0x1B},{0x65,0x06},{0x65,0x10},{0x65,0x18},{0x66,0x01},{0x66,0x0B},{0x66,0x14},{0x66,0x1C},{0x67,0x06},{0x67,0x10},{0x67,0x18},{0x68,0x02},
	{0x68,0x0B},{0x68,0x14},{0x68,0x1E},{0x69,0x06},{0x69,0x0F},{0x69,0x18},{0x6A,0x01},{0x6A,0x0B},{0x6A,0x14},{0x6A,0x1C},{0x6B,0x0C},{0x6B,0x16},{0x6B,0x1F},{0x6C,0x08},{0x6C,0x11},{0x6C,0x19},
	{0x6D,0x02},{0x6D,0x0D},{0x6D,0x15},{0x6D,0x1F},{0x6E,0x07},{0x6E,0x10},{0x6E,0x18},{0x6F,0x02},{0x6F,0x0B},{0x6F,0x14},{0x6F,0x1E},{0x70,0x07},{0x70,0x11},{0x70,0x1A},{0x71,0x04},{0x71,0x0C},
	{0x71,0x16},{0x72,0x06},{0x72,0x10},{0x72,0x18},{0x73,0x03},{0x73,0x0C},{0x73,0x14},{0x73,0x1E},{0x74,0x08},{0x74,0x12},{0x74,0x1A},{0x75,0x03},{0x75,0x0D},{0x75,0x17},{0x76,0x00},{0x76,0x09},
	{0x76,0x12},{0x76,0x1B},{0x77,0x05},{0x77,0x0E},{0x77,0x18},{0x78,0x02},{0x78,0x0C},{0x78,0x14},{0x79,0x07},{0x79,0x11},{0x79,0x19},{0x7A,0x02},{0x7A,0x0C},{0x7A,0x15},{0x7A,0x1E},{0x7B,0x08},
	{0x7B,0x11},{0x7B,0x19},{0x7C,0x04},{0x7C,0x0E},{0x7C,0x17},{0x7D,0x01},{0x7D,0x0B},{0x7D,0x14},{0x7D,0x1F},{0x7E,0x08},{0x7E,0x11},{0x7E,0x1C},{0x7F,0x05},{0x7F,0x10},{0x7F,0x17},{0x7F,0x1F},
};

//X6861H963-757/CL8H962-4578/X6871H962-5072 opt 20240423 start
static unsigned int dim_speed_change = 0; //0: no change, 1: change
static struct LCM_setting_table dim_speed_setting[] = {
	{0xFE,1,{0x70}}, //page 70
	{0x0D,1,{0x33}}, //default 0x33
//	{0xFE,1,{0x70}}, //page 70
//	{0x0E,1,{0x03}}, //default 0x03
};
//X6861H963-757/CL8H962-4578/X6871H962-5072 opt 20240423 end

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

static void cpu_qos_min_write(s32 cpu, s32 min_freq)
{
	int ret = 0;
	static struct cpufreq_policy *policy;

	// 获取cpu的策略
	policy = cpufreq_cpu_get(cpu);
	if (!policy) {
		pr_err("[LCM] Failed to get cpufreq policy for CPU %d\n", cpu);
		return;
	}

	// 添加QoS请求，锁定最小频率
	ret = freq_qos_add_request(&policy->constraints, &qos_req, FREQ_QOS_MIN, min_freq);
	if (ret < 0) {
		 pr_err("[LCM] Failed to add freq QoS request for CPU %d: %d\n", cpu, ret);
	} else {
		pr_info("[LCM] Locked CPU %d min frequency to %d kHz\n", cpu, min_freq);
	}

	// 释放策略
	cpufreq_cpu_put(policy);
	return;
}

static void cpu_qos_update(s32 new_min_freq)
{
    int ret;

	if(!IS_ERR_OR_NULL(qos_req.qos))
	{
		// 更新QoS请求的最小频率
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
	mdelay(2);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(2);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	msleep(35); //DDIC spec need more 32ms

	switch (g_lcm_fresh_mode) {
		case MODE_0_FPS: //60Hz
			lcm_initialization_setting[1].para_list[0] = 0x06; //60Hz
			break;
		case MODE_2_FPS: //120Hz
			lcm_initialization_setting[1].para_list[0] = 0x05; //120Hz
			break;
		case MODE_3_FPS: //144Hz
			lcm_initialization_setting[1].para_list[0] = 0x00; //144Hz
			break;
	}

	push_table(ctx, lcm_initialization_setting,
		sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table));
	if (mapped_level) {
		lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
	}
	ctx->error=0;
//add ddic_dim function control logic 20230911 start
	g_dim_enable = 0;
	g_need_dim_enable = 0;
//add ddic_dim function control logic 20230911 end
	pr_info("[LCM] %s end\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

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

	push_table(ctx, lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table));

	if(g_lcm_fresh_mode == MODE_3_FPS)
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

	pr_info("[LCM] %s begin\n", __func__);
	if (ctx->prepared)
		return 0;

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	if (g_lcm_fresh_mode == MODE_3_FPS)
		cpu_qos_update(min_freq);

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

static const struct drm_display_mode switch_mode_60hz = {
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

static const struct drm_display_mode switch_mode_144hz = {
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
static struct mtk_panel_params ext_params_60hz = {
	.pll_clk = PANEL_CLOCK,
	.data_rate = PANEL_DATA_RATE,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.ssc_enable=0,
	.lcm_esd_check_table[0] = {
			.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
		},
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
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 2, {0xFE, 0x00}}, //page 0
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x06}}, //60Hz-2F06
	},
	.tran_panel_params = &panel_driver_status,
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = PANEL_CLOCK,
	.data_rate = PANEL_DATA_RATE,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.ssc_enable=0,
	.lcm_esd_check_table[0] = {
			.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
		},
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
		.pic_height               = FRAME_HEIGHT,
		.pic_width                = FRAME_WIDTH,
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
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},

	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 2, {0xFE, 0x00}}, //page 0
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x05}}, //120Hz-2F05
	},
	.tran_panel_params = &panel_driver_status,
};

static struct mtk_panel_params ext_params_144hz = {
	.pll_clk = PANEL_CLOCK,
	.data_rate = PANEL_DATA_RATE,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.ssc_enable=0,
	.lcm_esd_check_table[0] = {
			.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
		},
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
		.pic_height               = FRAME_HEIGHT,
		.pic_width                = FRAME_WIDTH,
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
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},

	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 2, {0xFE, 0x00}}, //page 0
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x00}}, //144Hz-2F00
	},
	.tran_panel_params = &panel_driver_status,
};

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	if (level > 5119)
		level = 5119;

	if (level < 0)
		level = 0;

    input_brightness = level;

	if (level > 4095)
		mapped_level =  3315 + (level - 4096) * (4095-3315) / 1023;
	else
		mapped_level = level*3315/4095;

	bl_tb0[1] = ((mapped_level >> 8) & 0x0f);
	bl_tb0[2] = (mapped_level & 0xff);

	if (!cb)
		return -1;

	switch_page[1] = 0x00;
	cb(dsi, handle, switch_page, ARRAY_SIZE(switch_page)); //need switch page 0, then send 51

//add ddic_dim function control logic 20230911 start
	if ((g_dim_enable==0)&&(g_need_dim_enable==1)) {
		bl_dim[1] = 0x28;
		cb(dsi, handle, bl_dim, ARRAY_SIZE(bl_dim));
		g_dim_enable = 1;
	}

	if(g_set_aod_light == 1) {
		g_set_aod_light = 0;
	} else {
		g_need_dim_enable = 1;
	}

	if (mapped_level == 0) {
		bl_dim[1] = 0x20;
		cb(dsi, handle, bl_dim, ARRAY_SIZE(bl_dim));
		g_dim_enable = 0;
		g_need_dim_enable = 0;
	}
//add ddic_dim function control logic 20230911 end

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

//X6861H963-757/CL8H962-4578/X6871H962-5072 opt 20240423 start
	if (mapped_level != 0) {
		last_mapped_level = mapped_level;
	}

	if (dim_speed_change) {
		dim_speed_setting[1].para_list[0] = 0x33; //recovery default dim speed 0x33
		push_table_cb(dsi, cb, handle, dim_speed_setting, sizeof(dim_speed_setting) / sizeof(struct LCM_setting_table));
		dim_speed_change = 0;
	}
//X6861H963-757/CL8H962-4578/X6871H962-5072 opt 20240423 end

	pr_info("[LCM] %s level = %d, mapped_level = %d\n", __func__, level, mapped_level);

	return 0;
}

#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
tran_lcm_doze_backlight g_lcm_doze_backlight = {
	.doze_backlight_num = 0,
	.doze_backlight_level1 = 0,
	.doze_backlight_level2 = 0,
	.doze_backlight_level3 = 0,
};
static int panel_set_aod_light_mode(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	pr_info("[LCM] %s begin\n", __func__);

	input_brightness = level;
	mapped_level = level*3315/4095;	//0x1A-5nit-2, 0xB0-30nit-13, 0x16A-60nit-27

	if ((level != g_lcm_doze_backlight.doze_backlight_level1) &&
		(level != g_lcm_doze_backlight.doze_backlight_level2) &&
		(level != g_lcm_doze_backlight.doze_backlight_level3)) {
			mapped_level = 0x16A;	//60nit
	}

	bl_tb0[1] = ((mapped_level >> 8) & 0x0f);
	bl_tb0[2] = (mapped_level & 0xff);

	switch_page[1] = 0x00;
	cb(dsi, handle, switch_page, ARRAY_SIZE(switch_page)); 	//need switch page 0, then send 51
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	pr_info("[LCM] [%s] aod level is %u,aod mapped_level is %d\n", __func__, level, mapped_level);

	g_set_aod_light =1;

	/*if (last_mapped_level > mapped_level) { //when aod_mode_exit , aod bl value, need enable 51 dim for next screen_on
		dim_speed_setting[1].para_list[0] = 0x03; //change dim speed
		push_table_cb(dsi, cb, handle, dim_speed_setting, sizeof(dim_speed_setting) / sizeof(struct LCM_setting_table));
		dim_speed_change = 1; //dim speed has changed

		g_dim_enable = 0; //current dim state close
		g_need_dim_enable = 1; //need open dim function

		//switch_page[1] = 0x00; //page 0
		//bl_dim[1] = 0x28; //enable dim function
		//cb(dsi, handle, switch_page, ARRAY_SIZE(switch_page)); //need switch page 0
		//cb(dsi, handle, bl_dim, ARRAY_SIZE(bl_dim)); //enable 51 dim function
		pr_info("[%s] last_mapped_level > aod mapped_level:[0x%X > 0x%X], need enable dim\n", __func__, last_mapped_level, mapped_level);
	}*/

	pr_info("[LCM] %s end\n", __func__);
	return 0;
}
#endif

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
	if (drm_mode_vrefresh(m) == MODE_0_FPS)
		ext->params = &ext_params_60hz;
	else if (drm_mode_vrefresh(m) == MODE_2_FPS)
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == MODE_3_FPS)
		ext->params = &ext_params_144hz;
	else
		ret = 1;
/*
	if (drm_mode_vrefresh(m) != MODE_3_FPS && g_lcm_fresh_mode == MODE_3_FPS) {
		ext->params->dyn_fps.dfps_cmd_table[1].src_fps=0;
		ext->params->dyn_fps.dfps_cmd_table[1].cmd_num=3;
		ext->params->dyn_fps.dfps_cmd_table[1].para_list[0] = bl_tb0[0];
		ext->params->dyn_fps.dfps_cmd_table[1].para_list[1] = bl_tb0[1];
		ext->params->dyn_fps.dfps_cmd_table[1].para_list[2] = bl_tb0[2];
		ext->params->send_bl_cmd = 1;
	} else if (drm_mode_vrefresh(m) != MODE_3_FPS && !ext->params->send_bl_cmd) {
		ext->params->dyn_fps.dfps_cmd_table[1].cmd_num=0;
	}
*/

	if(drm_mode_vrefresh(m) == MODE_3_FPS && g_lcm_fresh_mode!= MODE_3_FPS) //切换到144hz
	{
		cpu_qos_update(min_freq);
	}
	else if (drm_mode_vrefresh(m) != MODE_3_FPS && g_lcm_fresh_mode == MODE_3_FPS) //退144hz
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
	if (IS_ERR_OR_NULL(ctx->dvdd_regulator)) {
		ctx->dvdd_regulator = devm_regulator_get(ctx->dev, "dvdd");

		if (IS_ERR_OR_NULL(ctx->dvdd_regulator)) {
			ret = PTR_ERR(ctx->dvdd_regulator);
			dev_err(ctx->dev, "<%s,%d> lcm dvdd get fail!ret[%d].\n", __func__, __LINE__, ret);
			goto END;
		}
	}


	if (on) {
		unsigned long time_diff = 0;
		lcm_prepare_time = jiffies_to_msecs(jiffies);
		time_diff = lcm_prepare_time - lcm_unprepare_time;
		if (time_diff < 7) //rm69220 DDIC need more than 5ms(power off->power on)
			mdelay(7 - time_diff);

		//VDDI 1.8V enable
		//ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
		//gpiod_set_value(ctx->vddi_gpio, 1);
		//devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
		//mdelay(2);

		//DVDD 1.25V enable
		ret = regulator_set_voltage(ctx->dvdd_regulator, 1254000, 1254000); //0.6+0.006x = y, y=1.254, x is integer value
		if (ret < 0)
			dev_err(ctx->dev, "<%s:%d>set lcm dvdd vol error!ret[%d]\n", __func__, __LINE__, ret);
		ret = regulator_enable(ctx->dvdd_regulator);
		if (ret < 0)
			dev_err(ctx->dev, "<%s:%d>enable lcm dvdd error!ret[%d]\n", __func__, __LINE__, ret);
		mdelay(5);

		//VCI 3.0V enable
		ctx->vci_gpio = devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->vci_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->vci_gpio);
		mdelay(5); //DDIC spec need more 10ms
	} else {
		//VCI 3.0V disable
		ctx->vci_gpio = devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->vci_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->vci_gpio);
		mdelay(5);

		//DVDD 1.25V disable
		if (regulator_is_enabled(ctx->dvdd_regulator))
			ret = regulator_disable(ctx->dvdd_regulator);
		if (ret < 0)
			dev_err(ctx->dev, "<%s:%d>disable lcm dvdd error!ret[%d]\n", __func__, __LINE__, ret);
		if (regulator_is_enabled(ctx->dvdd_regulator))
		{
			ret = regulator_disable(ctx->dvdd_regulator);
			if (regulator_is_enabled(ctx->dvdd_regulator))
				dev_err(ctx->dev, "<%s>disable dvdd_regulator error!ret[%d]\n", __func__, ret);
		}
		mdelay(5);

		//VDDI 1.8V disable
		//ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
		//gpiod_set_value(ctx->vddi_gpio, 0);
		//devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
		//mdelay(2);
		lcm_unprepare_time = jiffies_to_msecs(jiffies);
	}

END:
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}

static int panel_doze_enable(struct drm_panel *panel,
		    void *dsi, dcs_write_gce cb, void *handle)
{
//	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("[LCM] %s begin\n", __func__);

	//push_table_cb(dsi, cb, handle, aod_mode_enter_setting,
	//	sizeof(aod_mode_enter_setting) / sizeof(struct LCM_setting_table));	//AOD brightness 3level

	g_aod_enable = 1;
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
		    void *dsi, dcs_write_gce cb, void *handle)
{
//	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("[LCM] %s begin\n", __func__);

	//aod_mode_exit_setting[1].para_list[0] = bl_tb0[1];	//AOD brightness 3level
	//aod_mode_exit_setting[1].para_list[1] = bl_tb0[2];

	//push_table_cb(dsi, cb, handle, aod_mode_exit_setting,
	//	sizeof(aod_mode_exit_setting) / sizeof(struct LCM_setting_table));	//AOD brightness 3level

	g_aod_enable = 0;
	mdelay(17);

	pr_info("[LCM] %s end\n", __func__);
	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x9c, 0x02, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x0a, data, 3);
	if (ret < 0) {
		pr_err("[LCM] %s error\n", __func__);
		return 0;
	}

	printk("[LCM] ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0])
		return 1;

	printk("[LCM] ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 0;
}

//HBM function for fp unlock
static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
		    dcs_write_gce cb, void *handle, bool en)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int safe_brightness;

	if (!cb)
		return -1;

	if (ctx->hbm_en == en)
		goto done;

	pr_info("[LCM] %s FPS=%dHz en=%d, mapped_level=%d, aod=%d, input_brightness=%d  \n", __func__, g_lcm_fresh_mode, en, mapped_level, g_aod_enable, input_brightness);

	switch_page[1] = 0x00;
	bl_dim[1] = 0x20;
	cb(dsi, handle, switch_page, ARRAY_SIZE(switch_page)); //switch page 0
	cb(dsi, handle, bl_dim, ARRAY_SIZE(bl_dim)); //close 51 dim
	// max  4095
	safe_brightness = (input_brightness > 4095) ? 4095 : input_brightness;
	// 4095 ----> 255
	int map_index = (safe_brightness * 255) / 4095;

	if (en) {
		if(input_brightness>4095){
			hbm_mode_enter_setting[1].para_list[1] = brightness_mapped_to_alpha[255][0];
			hbm_mode_enter_setting[1].para_list[2] = brightness_mapped_to_alpha[255][1];
		} else {
			hbm_mode_enter_setting[1].para_list[1] = brightness_mapped_to_alpha[map_index][0];
			hbm_mode_enter_setting[1].para_list[2] = brightness_mapped_to_alpha[map_index][1];
		}

		push_table_cb(dsi, cb, handle, hbm_mode_enter_setting,
			sizeof(hbm_mode_enter_setting) / sizeof(struct LCM_setting_table));
	} else {
		push_table_cb(dsi, cb, handle, hbm_mode_exit_setting,
			sizeof(hbm_mode_exit_setting) / sizeof(struct LCM_setting_table));
	}

	g_dim_enable = 0; //current dim state close
	g_need_dim_enable = 0; //NO need open dim function
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
	int safe_brightness;
	//struct lcm *ctx = panel_to_lcm(panel);
	pr_info("[LCM] %s FPS=%dHz en=%d, mapped_level=%d, aod=%d, input_brightness=%d  \n", __func__, g_lcm_fresh_mode, en, mapped_level, g_aod_enable, input_brightness);

	switch_page[1] = 0x00;
	bl_dim[1] = 0x20;
	cb(dsi, handle, switch_page, ARRAY_SIZE(switch_page)); //switch page 0
	cb(dsi, handle, bl_dim, ARRAY_SIZE(bl_dim)); //close 51 dim
	// max  4095
	safe_brightness = (input_brightness > 4095) ? 4095 : input_brightness;
	// 4095 ----> 255
	int map_index = (safe_brightness * 255) / 4095;

	if (en) {
		if(input_brightness>4095){
			hbm_mode_enter_setting[1].para_list[1] = brightness_mapped_to_alpha[255][0];
			hbm_mode_enter_setting[1].para_list[2] = brightness_mapped_to_alpha[255][1];
		} else {
			hbm_mode_enter_setting[1].para_list[1] = brightness_mapped_to_alpha[map_index][0];
			hbm_mode_enter_setting[1].para_list[2] = brightness_mapped_to_alpha[map_index][1];
		}

		push_table_cb(dsi, cb, handle, hbm_mode_enter_setting,
			sizeof(hbm_mode_enter_setting) / sizeof(struct LCM_setting_table));
	} else {
		push_table_cb(dsi, cb, handle, hbm_mode_exit_setting,
			sizeof(hbm_mode_exit_setting) / sizeof(struct LCM_setting_table));
	}

	g_dim_enable = 0; //current dim state close
	g_need_dim_enable = 1; //need open dim function

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
extern s32 cmdq_pkt_sleep(struct cmdq_pkt *pkt, u32 tick, u16 reg_gpr);
/*static int tran_ddic_cmd_set_cmdq(struct drm_panel *panel, void *dsi,
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
}*/
//for sensor calibration enable/disable dim func 20230928 end

static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
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
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
	.set_aod_light_mode = panel_set_aod_light_mode,
#endif
	/*.doze_get_mode_flags = panel_doze_get_mode_flags,
	.doze_enable_start = panel_doze_enable_start,
	.doze_area = panel_doze_area,
	.doze_post_disp_on = panel_doze_post_disp_on,*/

//for sensor calibration enable/disable dim func 20230928 start
	//.tran_ddic_cmd_set_cmdq = tran_ddic_cmd_set_cmdq,
//for sensor calibration enable/disable dim func 20230928 end
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

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode_60hz;
	struct drm_display_mode *mode_120hz;
	struct drm_display_mode *mode_144hz;

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
	connector->display_info.height_mm = 158;

	pr_info("[LCM] %s end\n", __func__);
	return 3;  //support 144Hz/120Hz/60Hz:3  support 120Hz/60Hz:2
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static struct lcm *ctx_work = NULL;
static int lcm_regulator_power_init(struct lcm *ctx_work)
{
	int ret = 0;
	pr_info("[LCM]%s start\n", __func__);

	//dvdd 1.25V by ET5904
	ctx_work->dvdd_regulator = devm_regulator_get(ctx_work->dev, "dvdd");
	if (IS_ERR_OR_NULL(ctx_work->dvdd_regulator)) {
		pr_info("<%s,%d> lcm dvdd regulator get fail!\n", __func__, __LINE__);
		return -ENODEV;
	}
	ret = regulator_set_voltage(ctx_work->dvdd_regulator, 1254000, 1254000); //0.6+0.006x = y, y=1.254, x is integer value
	if (ret < 0){
		pr_info("<%s,%d> lcm dvdd set vol fail!ret[%d]\n", __func__, __LINE__, ret);
		return ret;
	}
	ret = regulator_enable(ctx_work->dvdd_regulator);
	if (ret < 0){
		pr_info("<%s,%d> lcm dvdd enable error!ret[%d]\n", __func__, __LINE__, ret);
		return ret;
	}
	cpu_qos_min_write(cpu, 0);//lcm_probe 添加QoS请求
	if(g_lcm_fresh_mode == MODE_3_FPS)
		cpu_qos_update(min_freq);
	return 0;
}

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

	pr_info("[LCM] %s rm69220 begin\n", __func__);

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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	backlight = of_parse_phandle(dev->of_node, "backlight", 0);

	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	//vddi 1.8V
	ctx->vddi_gpio = devm_gpiod_get(dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(dev, "cannot get lcm vddi-gpios %ld\n", PTR_ERR(ctx->vddi_gpio));
		return PTR_ERR(ctx->vddi_gpio);
	}
	devm_gpiod_put(dev, ctx->vddi_gpio);

	//dvdd 1.25V
	ret = lcm_regulator_power_init(ctx_work);//call lcm_regulator_power_init func
	if(ret < 0){
		dev_err(dev, "cannot get DVDD-regulator\n");
		return ret;
	}

	ctx->vci_gpio = devm_gpiod_get(dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_gpio)) {
		dev_dbg(dev, "cannot get vci-gpios %ld\n", PTR_ERR(ctx->vci_gpio));
		return PTR_ERR(ctx->vci_gpio);
	}
	devm_gpiod_put(dev, ctx->vci_gpio);

	//lcm reset
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get lcm reset-gpios %ld\n", PTR_ERR(ctx->reset_gpio));
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
	ret = mtk_panel_ext_create(dev, &ext_params_120hz, &ext_funcs, &ctx->panel); //support 144Hz:ext_params_144hz support 120Hz/60Hz:ext_params_120hz
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

	// 释放QoS请求
	if(!IS_ERR_OR_NULL(qos_req.qos))
	{
		freq_qos_remove_request(&qos_req);
		pr_info("[LCM] Released QoS request for CPU %d\n", cpu);
	}
}

static void lcm_shutdown(struct mipi_dsi_device *dsi)
{
	int ret = 0;
	//bool cancelled;
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	pr_info("[LCM] %s begin\n", __func__);
	lcm_disable(&ctx->panel);

	if(ctx->prepared) {
		lcm_dcs_write_seq_static(ctx, 0xFE,0x00);
		lcm_dcs_write_seq_static(ctx, 0x53,0x20);
		lcm_dcs_write_seq_static(ctx, 0x51,0x00,0x00);
		push_table(ctx, lcm_suspend_setting,
			sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table));

		ctx->vci_gpio = devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->vci_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->vci_gpio);
		mdelay(5);

		if (IS_ERR_OR_NULL(ctx->dvdd_regulator)) {
			ctx->dvdd_regulator = devm_regulator_get(ctx->dev, "dvdd");

			if (IS_ERR_OR_NULL(ctx->dvdd_regulator)) {
				ret = PTR_ERR(ctx->dvdd_regulator);
				dev_err(ctx->dev, "<%s,%d> lcm dvdd get fail!ret[%d].\n", __func__, __LINE__, ret);
				return;
			}
		}
		if (regulator_is_enabled(ctx->dvdd_regulator))
			ret = regulator_disable(ctx->dvdd_regulator);
		if (regulator_is_enabled(ctx->dvdd_regulator))
		{
			ret = regulator_disable(ctx->dvdd_regulator);
			if (regulator_is_enabled(ctx->dvdd_regulator))
				dev_err(ctx->dev, "<%s>disable dvdd_regulator error!ret[%d]\n", __func__, ret);
		}
	}

	/*if (IS_ERR_OR_NULL(ctx->dvdd_regulator)) {
		ctx->dvdd_regulator = devm_regulator_get(ctx->dev, "dvdd");
	}
	if (IS_ERR_OR_NULL(ctx->dvdd_regulator)) {
		ret = PTR_ERR(ctx->dvdd_regulator);
		dev_err(ctx->dev, "<%s,%d> lcm dvdd get fail!ret[%d].\n", __func__, __LINE__, ret);
	} else {
		cancelled = cancel_delayed_work_sync(&lcm_regulator_work);
		if (cancelled){
			pr_err("[LCM] %s work is 6s stop  regulator is disabled\n", __func__);
			ret = regulator_enable(ctx->dvdd_regulator);
			mdelay(2);
			ret = regulator_disable(ctx->dvdd_regulator);
			if (regulator_is_enabled(ctx->dvdd_regulator)) {
				pr_err("[LCM] %s %d regulator is enabled,try to disable\n",__func__, __LINE__);
				ret = regulator_disable(ctx->dvdd_regulator);
			}
		} else {
			if (regulator_is_enabled(ctx->dvdd_regulator)) {
				pr_err("[LCM] %s work is run and  regulator is enabled\n", __func__);
				ret = regulator_disable(ctx->dvdd_regulator);
			}
		}
		if (regulator_is_enabled(ctx->dvdd_regulator)) {
			pr_err("[LCM] %s %d regulator is enabled,try to disable\n",__func__, __LINE__);
			ret = regulator_disable(ctx->dvdd_regulator);
		}
	}*/
	// 释放QoS请求
	if(!IS_ERR_OR_NULL(qos_req.qos))
	{
		freq_qos_remove_request(&qos_req);
		pr_info("[LCM] Released QoS request for CPU %d\n", cpu);
	}
}

static const struct of_device_id lcm_of_match[] = {
	{
		.compatible = "rm69220,fhdp,dsi,vdo,dsc,csot,csot,144hz,x6858",
	},
	{} };

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.shutdown = lcm_shutdown,
	.driver = {
			.name = "rm69220_fhdp_dsi_vdo_dsc_csot_csot_144hz_x6858",
			.owner = THIS_MODULE,
			.of_match_table = lcm_of_match,
		},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("qian.wu3 <qian.wu3 @transsion.com>");
MODULE_DESCRIPTION("RM69220 RAMLESS VDO 144HZ AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");

