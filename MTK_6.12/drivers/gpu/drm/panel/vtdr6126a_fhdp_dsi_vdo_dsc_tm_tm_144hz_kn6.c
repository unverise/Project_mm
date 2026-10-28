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
#define HBP                         16
#define VSA                         2
#define VBP                         18

/*Parameter setting for mode 0 Start*/
#define MODE_0_FPS                  144
#define MODE_0_VFP                  64
#define MODE_0_HFP                  65//95//28
#define MODE_0_DATA_RATE            1124//1190//1028
/*Parameter setting for mode 0 End*/

/*Parameter setting for mode 1 Start*/
#define MODE_1_FPS                  120
#define MODE_1_VFP                  64
#define MODE_1_HFP                  172//206//124
#define MODE_1_DATA_RATE            1124//1190//1028
/*Parameter setting for mode 1 End*/
/*Parameter setting for mode 2 Start*/
#define MODE_2_FPS                  90
#define MODE_2_VFP                  880
#define MODE_2_HFP                  172//206//124
#define MODE_2_DATA_RATE            1124//1190//1028
/*Parameter setting for mode 2 End*/
/*Parameter setting for mode 3 Start*/
#define MODE_3_FPS                  60
#define MODE_3_VFP                  2512
#define MODE_3_HFP                  172//206//124
#define MODE_3_DATA_RATE            1124//1190//1028
/*Parameter setting for mode 3 End*/
/*Parameter setting for mode 4 Start*/
#define MODE_4_FPS                  30
#define MODE_4_VFP                  64
#define MODE_4_HFP                  2082//206//124
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

#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
tran_lcm_doze_backlight g_lcm_doze_backlight = {
        .doze_backlight_num = 0,
        .doze_backlight_level1 = 0,
        .doze_backlight_level2 = 0,
        .doze_backlight_level3 = 0,
};
#endif

static bool lcm_longh30hz_support = false;
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
    //frame select, 0x00:144Hz, 0x01:120Hz, 0x02:90Hz, 0x03:60Hz
    {0x6C,1,{0x01}},
    {0x03,1,{0x01}},
    {0x35,1,{0x00}},
    {0x53,1,{0x20}},
    {0x51,2,{0x00,0x00}},
    {0x59,1,{0x09}},//Demura ON=09 OFF=00
    {0x5E,1,{0x00}},//0= Hybrid mode ,1= DC mode
    {0x6D,1,{0x00}},
    {0x6F,1,{0x01}},
    {0x72,1,{0x00}},//00 SGM3837s  01 SGM3838
    {0xF0,2,{0xAA,0x18}},
    {0xB0,1,{0x80}},
    {0xB2,1,{0x00}},
    //vesa 8bit slice height 12
    {0x70,94,{0x12,0x00,0x00,0x89,0x30,0x80,0x09,0x3C,0x04,
              0x38,0x00,0x0C,0x02,0x1C,0x02,0x1C,0x02,0x00,0x02,
              0x0E,0x00,0x20,0x01,0x1F,0x00,0x07,0x00,0x0C,0x08,
              0xBB,0x08,0x7A,0x18,0x00,0x10,0xF0,0x03,0x0C,0x20,
              0x00,0x06,0x0B,0x0B,0x33,0x0E,0x1C,0x2A,0x38,0x46,
              0x54,0x62,0x69,0x70,0x77,0x79,0x7B,0x7D,0x7E,0x01,
              0x02,0x01,0x00,0x09,0x40,0x09,0xBE,0x19,0xFC,0x19,
              0xFA,0x19,0xF8,0x1A,0x38,0x1A,0x78,0x1A,0xB6,0x2A,
              0xB6,0x2A,0xF4,0x2A,0xF4,0x4B,0x34,0x63,0x74,0x00,
              0x00,0x00,0x00,0x00,0x00}},

    //==============CM3===================//
    //AOD MIPI time out
    {0xFF,2,{0x5A,0x80}},
    {0xF9,1,{0x10}},
    {0x65,1,{0x0A}},
    {0xF9,1,{0x1E}},
    //By V清error
    {0xFF,2,{0x5A,0x80}},
    {0x65,1,{0x25}},
    {0xFD,1,{0x01}},
    //SOP
    {0xFF,2,{0x5A,0x81}},
    {0x65,1,{0x03}},
    {0xF3,1,{0x61}},
    {0x65,1,{0x0B}},
    {0xF3,1,{0x78}},
    //By V清error
    {0xFF,2,{0x5A,0x83}},
    {0x65,1,{0x0B}},
    {0xF7,1,{0x03}},
    //关闭NEQ
    {0xF0,2,{0xAA,0x12}},
    {0x65,1,{0x0B}},
    {0xE9,1,{0x00}},
    //crc off
    {0xFF,2,{0x5A,0x83}},
    {0x65,1,{0x09}},
    {0xF7,2,{0x10,0x10}},
    //RUF OFF
    {0xF0,2,{0xAA,0x18}},
    {0xB0,1,{0x80}},
    {0xF0,2,{0xAA,0x16}},
    //MTE_VRR_BASE=1
    {0xD1,16,{0x00,0x00,0x00,0x01,0x01,0x00,0x00,0x00,0x00,
              0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    //CMD3 VGLR power sequence
    {0xFF,2,{0x5A,0x81}},
    {0x65,1,{0x02}},
    {0xFB,2,{0x73,0x73}},

    //Peaklum IP开启阈值5%
    {0xF0,2,{0xaa,0x1C}},
    {0xB0,11,{0x01,0x00,0x0F,0xFF,0x0C,0x11,0x11,0x2C,0x2D,0x2E,0x2F}},

    //dummy=0,for,AP
    {0xF0,2,{0xAA,0x10}},
    //NC&NL,for,AP
    {0xB0,9,{0x05,0x2A,0x01,0x2E,0x01,0x04,0x38,0x04,0x9E}},
    //SCL
    {0xF0,2,{0xAA,0x10}},
    {0xB0,9,{0x05,0x2A,0x01,0x2E,0x01,0x04,0x38,0x04,0x9E}},
    {0xBA,9,{0x07,0x0F,0x1F,0x0B,0x31,0x0E,0x4E,0x0E,0x4F}},
    //T1A
    {0xF0,2,{0xAA,0x10}},
    {0xB1,44,{0x01,0x61,0x00,0x18,0x00,0x44,0x00,0x01,0xA9,0x00,
              0x18,0x00,0x44,0x00,0x01,0xA9,0x00,0x18,0x03,0xD4,
              0x00,0x01,0xA9,0x00,0x18,0x0A,0xF4,0x00,0x00,0x14,
              0x00,0x40,0x00,0x14,0x00,0x40,0x00,0x14,0x03,0x70,
              0x00,0x14,0x09,0xD0}},
    {0xB2,33,{0x01,0xA9,0x00,0x18,0x00,0x44,0x03,0x01,0xA9,0x00,
              0x18,0x00,0x44,0x03,0x01,0xA9,0x00,0x18,0x00,0x44,
              0x03,0x00,0x14,0x00,0x40,0x00,0x14,0x00,0x40,0x00,
              0x14,0x00,0x40}},
    {0xF0,2,{0xAA,0x14}},
    {0xCC,28,{0x00,0x03,0x00,0x02,0x00,0x03,0x00,0x01,0x00,0x03,
              0x00,0x01,0x00,0x03,0x00,0x01,0x00,0x03,0x00,0x01,
              0x00,0x03,0x00,0x01,0x00,0x03,0x00,0x01}},
    //OTK4
    {0xF0,2,{0xAA,0x14}},
    {0xC1,4,{0x04,0x02,0x21,0x12}},
    {0xCA,4,{0x11,0x01,0x05,0x05}},
    {0x65,1,{0x04}},
    {0XCA,16,{0x03,0x78,0x04,0x1A,0x04,0x1A,0x04,0x1A,0x02,0xB0,
              0x03,0x52,0x03,0x52,0x03,0x52}},
    {0x65,1,{0x34}},
    {0XCA,12,{0x04,0x1A,0x04,0x1A,0x04,0x1A,0x03,0x52,0x03,0x52,0x03,0x52}},
    {0x65,1,{0x66}},
    {0XCA,16,{0x03,0x64,0x04,0x06,0x04,0x06,0x04,0x06,0x02,0xC4,
              0x03,0x66,0x03,0x66,0x03,0x66}},
    {0x65,1,{0x76}},
    {0XCA,12,{0x04,0x06,0x04,0x06,0x04,0x06,0x03,0x66,0x03,0x66,0x03,0x66}},
    {0x65,1,{0x14}},
    {0XCA,16,{0x03,0x1C,0x03,0xBE,0x03,0xBE,0x03,0xBE,0x03,0x0C,
        0x03,0xAE,0x03,0xAE,0x03,0xAE}},
    {0x65,1,{0x40}},
    {0XCA,12,{0x03,0xBE,0x03,0xBE,0x03,0xBE,0x03,0xAE,0x03,0xAE,0x03,0xAE}},
    {0x65,1,{0x24}},
    {0XCA,16,{0x00,0xC6,0x00,0xEE,0x00,0xEE,0x00,0xEE,0x00,0xC4,
              0x00,0xEC,0x00,0xEC,0x00,0xEC}},
    {0x65,1,{0x4C}},
    {0XCA,12,{0x00,0xEE,0x00,0xEE,0x00,0xEE,0x00,0xEC,0x00,0xEC,0x00,0xEC}},
    {0x65,1,{0x58}},
    {0XCA,14,{0x01,0x8A,0x01,0xDB,0x01,0xDB,0x01,0xDB,0x01,0xDB,0x01,0xDB,0x01,0xDB}},

    {0xF0,2,{0xAA,0x15}},
    {0x65,1,{0x0A}},
    {0xB1,1,{0x02}},

    {0xF0,2,{0xAA,0x10}},
    {0xCE,1,{0x20}},
    //LHBM
    {0xF0,2,{0xAA,0x18}},
    {0xB6,13,{0x65,0x00,0x1D,0x08,0xBD,0x01,0x00,0x47,0x57,0x57,0x00,0x00,0x00}},
    {0xB7,8,{0x01,0x01,0xF7,0x08,0xFA,0x00,0xCA,0xCA}},
    {0x65,1,{0x0C}},
    {0xB0,1,{0x04}},
    {0xC0,15,{0xE0,0xAB,0x96,0x49,0x63,0xC9,0xEC,0xBA,0x7E,0x20,0x10,0x48,0x44,0x32,0x20}},
    {0xC1,15,{0x22,0x89,0x88,0xC4,0x72,0x38,0xA0,0x90,0x29,0x14,0x92,0x49,0x24,0x91,0x50}},
    {0xC2,15,{0x15,0x0A,0x85,0x42,0xA1,0x50,0xA8,0x54,0x2A,0x15,0x12,0x49,0x22,0x91,0x48}},
    {0xC3,15,{0x14,0x8A,0x45,0x02,0x81,0x41,0x1C,0x4E,0x27,0x13,0x09,0x84,0xC2,0x51,0x28}},
    {0xC4,15,{0x12,0x09,0x04,0x62,0x31,0x10,0x88,0x42,0x21,0x10,0x08,0x07,0xE3,0xE1,0xF0}},
    {0xC5,15,{0x1E,0x8F,0x47,0x83,0xB1,0xD8,0xE8,0x72,0x39,0x1C,0x0D,0xC6,0xC3,0x61,0xA8}},
    {0xC6,15,{0x1A,0x0C,0xC6,0x43,0x21,0x88,0xC0,0x5E,0x2E,0x17,0x0B,0x45,0x82,0xB1,0x50}},
    {0xC7,15,{0x14,0x8A,0x04,0xE2,0x61,0x28,0x90,0x46,0x22,0x10,0x80,0x40,0x00,0x00,0x00}},
    {0xE0,15,{0x94,0xA3,0x06,0xA8,0xE3,0xBC,0x98,0x6C,0xC5,0x41,0x9A,0x75,0x02,0x51,0x18}},
    {0xE1,15,{0x5E,0x99,0xA5,0xE1,0x15,0x24,0x7A,0xAB,0x0D,0xB8,0x1B,0x6C,0x15,0x4B,0x43}},
    {0xE2,15,{0x54,0x71,0xF3,0x32,0x36,0xD8,0x0A,0x96,0x42,0x75,0xC6,0xB7,0xDF,0x7F,0xE0}},
    {0xE3,15,{0x00,0x00,0x00,0x7F,0x60,0x00,0x00,0x00,0x03,0x60,0x00,0x00,0x00,0x00,0x00}},
    {0xf0,2,{0xaa,0x1c}},
    {0xc0,6,{0x02,0x26,0x01,0xF4,0x02,0x92}},
    {0xc1,20,{0x01,0x91,0x03,0xeb,0x06,0x5f,0x08,0xd3,0x0b,0x47,0x0d,0xbb,0x0e,0x7c,0x0f,0x3d,0x0f,0xfe,0x0f,0xff}},
    {0xc4,2,{0x01,0xff}},
    {0x65,1,{0x00}},
    {0xc2,3,{0x3E,0x36,0x2A}},
    {0x65,1,{0x02}},
    {0xc4,3,{0x03,0x04,0x04}},
    {0x65,1,{0x03}},
    {0xc2,3,{0x3D,0x36,0x28}},
    {0x65,1,{0x05}},
    {0xc4,3,{0x04,0x04,0x04}},
    {0x65,1,{0x06}},
    {0xc2,3,{0x3C,0x36,0x27}},
    {0x65,1,{0x08}},
    {0xc4,3,{0x06,0x06,0x07}},
    {0x65,1,{0x09}},
    {0xc2,3,{0x3B,0x36,0x26}},
    {0x65,1,{0x0b}},
    {0xc4,3,{0x08,0x08,0x0a}},
    {0x65,1,{0x0c}},
    {0xc2,3,{0x3C,0x36,0x26}},
    {0x65,1,{0x0e}},
    {0xc4,3,{0x09,0x0a,0x0c}},
    {0x65,1,{0x0f}},
    {0xc2,3,{0x3B,0x34,0x24}},
    {0x65,1,{0x11}},
    {0xc4,3,{0x0d,0x0e,0x11}},
    {0x65,1,{0x12}},
    {0xc2,3,{0x29,0x24,0x0F}},
    {0x65,1,{0x14}},
    {0xc4,3,{0x13,0x12,0x15}},
    {0x65,1,{0x15}},
    {0xc2,3,{0x19,0x16,0x83}},
    {0x65,1,{0x17}},
    {0xc4,3,{0x17,0x18,0x1b}},
    {0x65,1,{0x18}},
    {0xc2,3,{0x09,0x08,0x95}},
    {0x65,1,{0x1a}},
    {0xc4,3,{0x1f,0x1e,0x22}},
    {0x65,1,{0x1b}},
    {0xc2,3,{0x0F,0x0E,0x91}},
    {0x65,1,{0x1d}},
    {0xc4,3,{0x1d,0x1c,0x1f}},

    //ECK mask
    {0xF0,2,{0xAA,0x14}},
    {0xBE,1,{0x90}},
    {0xBF,1,{0x90}},

    //Swire Dimming
    {0xF0,2,{0xAA,0x10}},
    {0x65,1,{0x01}},
    {0xD0,1,{0xE8}},
    //Dimming Frame
    {0xF0,2,{0xAA,0x13}},
    {0xC8,1,{0x18}},
    {0xD0,1,{0x18}},
    {0xE0,1,{0x18}},
    {0xE8,1,{0x18}},

    {0xF0,2,{0xAA,0x12}},
    {0xC5,1,{0x12}},
    {0x65,1,{0x0A}},
    {0xC5,1,{0x12}},
    {0x65,1,{0x13}},
    {0xC5,1,{0x12}},

    //Leave idle no black frame
    {0xF0,2,{0xAA,0x12}},
    {0xD1,3,{0x06,0x01,0x01}},
    {0xD6,2,{0x01,0x00}},

    {0xF0,2,{0xAA,0x13}},
    {0xD8,24,{0x10,0x04,0x04,0x04,0x00,0x00,0x10,0x04,0x04,0x04,
              0x00,0x00,0x10,0x04,0x04,0x04,0x00,0x00,0x10,0x04,
              0x04,0x04,0x00,0x00}},

    {0xF0,2,{0xAA,0x10}},
    {0xBE,2,{0x3F,0x70}},

    {0xF0,2,{0xAA,0x15}},
    {0xBD,4,{0x40,0x40,0x40,0x40}},
    {0xBE,6,{0x22,0x11,0x11,0x11,0x11,0x44}},

    {0xF0,2,{0xAA,0x12}},
    {0x65,1,{0x07}},//DIS
    {0xB9,7,{0x60,0x60,0x60,0x60,0xF0,0xF0,0xF0}},
    {0x65,1,{0x2E}},//DIS
    {0xB9,4,{0x22,0x22,0x00,0x00}},

    //disable cmd2 cmd3
    {0xF0,2,{0xAA,0x00}},
    {0xFF,2,{0x5A,0x00}},

    {0x11,0,{}},
    {REGFLAG_DELAY, 100, {}},
    {0x29,0,{}},
    {REGFLAG_DELAY, 20,{}},
};
static struct LCM_setting_table lcm_suspend_setting[] = {
    {0x6c,1,{0x02}}, //step 1, select 90Hz
    {REGFLAG_DELAY, 10, {}},
    // Display off sequence
    {0x28, 0, {}}, //step 2, sleep off
    // Sleep Mode On
    {REGFLAG_DELAY, 11, {}},
    {0x10, 0, {}},
    {REGFLAG_DELAY, 80, {}},
};

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

static struct LCM_setting_table hbm_mode_enter_setting[] = {
//Alpha mode control
    {0xF0,2,{0xAA,0x13}},
    {0xC6,1,{0x01}},
    {0x63,4,{0x10,0x00,0x0D,0xBB}}, //51-DBV <= 0x190: 1st&&2nd, need from alpha_gain table; 51-DBV > 0x190: 3th&&4th, need current backlight value.
    {0x62,1,{0x03}},
};

static struct LCM_setting_table hbm_mode_exit_setting[] = {
//Alpha mode control
    {0x62,1,{0x00}},
};

static struct LCM_setting_table aod_mode_esd_recovery_setting[] = {
    {0x39,0,{}},
};

static struct LCM_setting_table aod_mode_enter_setting[] = {
    {0x51,2,{0x0F,0xFF}},
    {0x6D,1,{0x02}},
};

#if IS_ENABLED(CONFIG_TRAN_DDIC_OSC_TRACKING)
static struct LCM_setting_table lcm_osc_tracking_setting[] = {
    {0xA4,1,{0x01}},
    {0xF0,2,{0xAA,0x14}},
    {0xC1,2,{0x04,0x47}},
    {0xA4,1,{0x00}},
    {0xA6,1,{0x01}},
    {0xF0,2,{0xAA,0x00}},
    {0xFF,2,{0x5A,0x00}},
};
#endif

#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
static struct LCM_setting_table lcm_pcd_on_setting[] = {
    {0xF0,2,{0xAA,0x12}},
    {0xD0,2,{0x08,0x02}}, // BIST Enable
};

static struct LCM_setting_table lcm_pcd_on2_setting[] = {
    {0x11,0,{}},
    {0xF0,2,{0xAA,0x12}},
    {0x65,1,{0x04}},
};

static struct LCM_setting_table lcm_pcd_off_setting[] = {
    {0xF0,2,{0xAA,0x12}},
    {0xD0,2,{0x08,0x00}}, //Disable
};
#endif

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
    struct cpufreq_policy *policy = NULL;

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

    if (IS_ERR_OR_NULL(qos_req.qos))
        cpu_qos_min_write(cpu, 0);

    if (!IS_ERR_OR_NULL(qos_req.qos))
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
    if (IS_ERR_OR_NULL(ctx->reset_gpio)) {
        dev_err(ctx->dev, "cannot get reset_gpio %ld\n", PTR_ERR(ctx->reset_gpio));
    } else {
        gpiod_set_value(ctx->reset_gpio, 1);
        mdelay(5);
        gpiod_set_value(ctx->reset_gpio, 0);
        mdelay(5);
        gpiod_set_value(ctx->reset_gpio, 1);
        devm_gpiod_put(ctx->dev, ctx->reset_gpio);
        msleep(25); ////DDIC spec need more 10ms
    }
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
            if (g_aod_enable) {
                push_table(ctx, aod_mode_enter_setting,
                    sizeof(aod_mode_enter_setting) / sizeof(struct LCM_setting_table));
                push_table(ctx, aod_mode_esd_recovery_setting,
                    sizeof(aod_mode_esd_recovery_setting) / sizeof(struct LCM_setting_table));
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
    .lcm_esd_check_table[1] = {
        .cmd = 0x66, .count = 2, .para_list[0] = 0x00, .para_list[1] = 0x00,
    },
    #if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
    .lcm_pcd_check_table[0] = {
        .cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xD4,
    },
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
        .dfps_cmd_table[0] = {0, 2, {0x6C, 0x00}}, //144Hz
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
    .lcm_esd_check_table[1] = {
        .cmd = 0x66, .count = 2, .para_list[0] = 0x00, .para_list[1] = 0x00,
    },
    #if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
    .lcm_pcd_check_table[0] = {
        .cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xD4,
    },
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
        .dfps_cmd_table[0] = {0, 2, {0x6C, 0x01}}, //120Hz
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
    .lcm_esd_check_table[1] = {
        .cmd = 0x66, .count = 2, .para_list[0] = 0x00, .para_list[1] = 0x00,
    },
    #if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
    .lcm_pcd_check_table[0] = {
        .cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xD4,
    },
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
        .dfps_cmd_table[0] = {0, 2, {0x6C, 0x02}}, //90Hz
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
    .lcm_esd_check_table[1] = {
        .cmd = 0x66, .count = 2, .para_list[0] = 0x00, .para_list[1] = 0x00,
    },
    #if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
    .lcm_pcd_check_table[0] = {
        .cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xD4,
    },
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
        .dfps_cmd_table[0] = {0, 2, {0x6C, 0x03}}, //60Hz
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
    #if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
    .lcm_pcd_check_table[0] = {
        .cmd = 0xD0, .count = 1, .para_list[0] = 0x81, .para_list[1] = 0xD4,
    },
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
        .dfps_cmd_table[0] = {0, 1, {0x39}}, //30Hz
    },
    .data_rate = MODE_4_DATA_RATE,
    .tran_panel_params = &panel_driver_status,
};
//Ajust dim speed 20241212 start
//extern unsigned int g_dimming_status;
static unsigned int ghbm_state = 0;
static unsigned int dim_speed_change = 0;
static struct LCM_setting_table dim_frame_speed[] = {
     //Diming by Frame, set 0x08 frame time for HBM DIMING
    {0xF0,2,{0xAA,0x13}},
    {0xC8,1,{0x08}},
    {0xD0,1,{0x08}},
    {0xE0,1,{0x08}},
    {0xE8,1,{0x08}},
    {0xF0,2,{0xAA,0x10}},
    {0x65,1,{0x01}},
    {0xD0,1,{0x68}},
};
//Ajust dim speed 20241212 end
//resolve screen flickering anomalies when 0x51 is 0xFFE/0xFFF for suspend test case
static unsigned int vrefn2_en = 0;
static struct LCM_setting_table vrefn2_set[] = {
    {0xF0,2,{0xAA,0x12}},
    {0xBE,2,{0x01,0x05}},
};
#if IS_ENABLED(CONFIG_TRAN_DDIC_OSC_TRACKING)
static int lcm_set_osc_tracking_cmdq(void *dsi, dcs_write_gce cb, void *handle)
{
    if (!cb) {
        pr_info("[LCM] %s cb is null\n", __func__);
        return -1;
    }

    push_table_cb(dsi, cb, handle, lcm_osc_tracking_setting, sizeof(lcm_osc_tracking_setting) / sizeof(struct LCM_setting_table));

    pr_info("[LCM] %s \n", __func__);

    return 0;
}
#endif

#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
static int lcm_set_pcd_cmdq(void *dsi, dcs_write_gce cb, void *handle, bool state)
{
    if (!cb) {
        pr_info("[LCM] %s cb is null\n", __func__);
        return -1;
    }

    if (state == true) {
        push_table_cb(dsi, cb, handle, lcm_pcd_on_setting, sizeof(lcm_pcd_on_setting) / sizeof(struct LCM_setting_table));
        cmdq_pkt_sleep(handle, 26000*100, 0x03); //exit aod enter LHBM, need delay 30ms
        push_table_cb(dsi, cb, handle, lcm_pcd_on2_setting, sizeof(lcm_pcd_on2_setting) / sizeof(struct LCM_setting_table));
    } else {
        push_table_cb(dsi, cb, handle, lcm_pcd_off_setting, sizeof(lcm_pcd_off_setting) / sizeof(struct LCM_setting_table));
    }
    pr_info("[LCM] %s state=%d\n", __func__, state);

    return 0;
}
#endif

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
    void *handle, unsigned int level)
{
    if (!cb) {
        pr_info("[LCM] %s cb is null\n", __func__);
        return -1;
    }

    if (level > 5119)
        level = 5119;
    if(level<=4095) {
        mapped_level = level * 3515 / 4095;
        if (ghbm_state) {
            ghbm_state = ghbm_state - 1;  /* after levea hbm, recovery dim speed(0x18) for second normal backlight */
        }
    } else if (4095 < level && level <= 5119) {
        mapped_level = 3515 + (level - 4095) * (4095-3515) / 1024;
        ghbm_state = 2;
    }
    if ((mapped_level < 10) && (mapped_level > 0)) {
        mapped_level = 10;
    }

    //USD mofidy dimming setting for XDR start
    if (panel_driver_status.dimming_status ==2) {
        if(dim_speed_change != 1) {
            dim_frame_speed[1].para_list[0] = 0x06;
            dim_frame_speed[2].para_list[0] = 0x06;
            dim_frame_speed[3].para_list[0] = 0x06;
            dim_frame_speed[4].para_list[0] = 0x06;
            push_table_cb(dsi, cb, handle, dim_frame_speed,
                sizeof(dim_frame_speed) / sizeof(struct LCM_setting_table));
            pr_info("[LCM] %s bl_dim dimming_status =%d\n", __func__, panel_driver_status.dimming_status);
            //panel_driver_status.dimming_status = DIMMING_DEFAULT_STATUS;
            //g_dim_enable = 0;
        }
        ghbm_state = 3; //dimming status for XDR
        dim_speed_change = 1;
    }

    //USD mofidy dimming setting end

    if ((ghbm_state == 2) && (dim_speed_change == 0)) { /* need ajust dim speed(0x08 frame) for hbm diming */
        pr_info("%s ghbm_state=%d, dim_speed_change=%d set 0x08.\n", __func__, ghbm_state, dim_speed_change);
        dim_frame_speed[1].para_list[0] = 0x08;
        dim_frame_speed[2].para_list[0] = 0x08;
        dim_frame_speed[3].para_list[0] = 0x08;
        dim_frame_speed[4].para_list[0] = 0x08;
        dim_frame_speed[7].para_list[0] = 0x68;
        push_table_cb(dsi, cb, handle, dim_frame_speed, sizeof(dim_frame_speed) / sizeof(struct LCM_setting_table));
        dim_speed_change = 1;
    }

    if (((ghbm_state == 0) && (dim_speed_change == 1)) || (panel_driver_status.dimming_status ==1)) { /* after levea hbm, recovery dim speed(0x18) for second normal backlight */
        pr_info("%s ghbm_state=%d, dim_speed_change=%d set 0x18.\n", __func__, ghbm_state, dim_speed_change);
        dim_frame_speed[1].para_list[0] = 0x18;
        dim_frame_speed[2].para_list[0] = 0x18;
        dim_frame_speed[3].para_list[0] = 0x18;
        dim_frame_speed[4].para_list[0] = 0x18;
        dim_frame_speed[7].para_list[0] = 0xE8;
        push_table_cb(dsi, cb, handle, dim_frame_speed, sizeof(dim_frame_speed) / sizeof(struct LCM_setting_table));
        dim_speed_change = 0;
        if (panel_driver_status.dimming_status ==1) {
            panel_driver_status.dimming_status = DIMMING_DEFAULT_STATUS;
        }
    }
    //Ajust dim speed 20241212 end

    bl_tb0[1] = ((mapped_level >> 8) & 0x0f);
    bl_tb0[2] = (mapped_level & 0xff);
    if (!cb) {
        pr_err("[LCM] %s cb is invalid\n", __func__);
        return -1;
    }
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

    if ((mapped_level == 0) || /*(g_dimming_status == 0) ||*/ (panel_driver_status.dimming_status ==0)) {
        bl_dim[1] = 0x20;
        cb(dsi, handle, bl_dim, ARRAY_SIZE(bl_dim));
        g_dim_enable = 0;
        g_need_dim_enable = 0;
    }
    if (mapped_level != 0) {
        last_mapped_level = mapped_level;
    }
    //resolve screen flickering anomalies when 0x51 is 0xFFE/0xFFF for suspend test case
    switch (mapped_level) {
        case 0:
            if (vrefn2_en) {
                pr_info("[LCM] %s vrefn2_en=%d mapped_level=%d \n", __func__, vrefn2_en, mapped_level);
                push_table_cb(dsi, cb, handle, vrefn2_set, sizeof(vrefn2_set) / sizeof(struct LCM_setting_table));
            }
            break;
        case 0xFFE:
        case 0xFFF:
            vrefn2_en = 1;
            break;
        default:
            vrefn2_en = 0;
            break;
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

    switch (drm_mode_vrefresh(m)) {
        case MODE_0_FPS:
            ext->params = &ext_params_144hz;
            break;
        case MODE_1_FPS:
            ext->params = &ext_params_120hz;
            break;
        case MODE_2_FPS:
            ext->params = &ext_params_90hz;
            break;
        case MODE_3_FPS:
            ext->params = &ext_params_60hz;
            break;
        case MODE_4_FPS:
            if (lcm_longh30hz_support) {
                ext->params = &ext_params_30hz;
            } else {
                ret = 1;
            }
            break;
        default:
            ret = 1;
            break;
    }

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
    if (IS_ERR_OR_NULL(ctx->reset_gpio)) {
     dev_err(ctx->dev, "cannot get reset_gpio %ld\n", PTR_ERR(ctx->reset_gpio));
    } else {
        gpiod_set_value(ctx->reset_gpio, on);
        devm_gpiod_put(ctx->dev, ctx->reset_gpio);
    }
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

    if (IS_ERR_OR_NULL(ctx->vci_regulator))
        ctx->vci_regulator = devm_regulator_get(ctx->dev, "vci");

    if (on) {
        unsigned long time_diff = 0;
        lcm_prepare_time = jiffies_to_msecs(jiffies);
        time_diff = lcm_prepare_time - lcm_unprepare_time;
        if (time_diff < 120)
            mdelay(120 - time_diff);

        //VDDI 1.8V enable
        ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
        if (IS_ERR_OR_NULL(ctx->vddi_gpio)) {
            dev_err(ctx->dev, "cannot get vddi_gpio %ld\n", PTR_ERR(ctx->vddi_gpio));
        } else {
            gpiod_set_value(ctx->vddi_gpio, 1);
            devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
        }
        mdelay(2);

        //DVDD 1.25V enable
        ctx->dvdd_gpio = devm_gpiod_get(ctx->dev, "dvdd", GPIOD_OUT_HIGH);
        if (IS_ERR_OR_NULL(ctx->dvdd_gpio)) {
            dev_err(ctx->dev, "cannot get dvdd_gpio %ld\n", PTR_ERR(ctx->dvdd_gpio));
        } else {
            gpiod_set_value(ctx->dvdd_gpio, 1);
            devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
        }
        mdelay(2);

        //VCI 3.0V enable
        if (IS_ERR_OR_NULL(ctx->vci_regulator)) {
            dev_err(ctx->dev, "<%s,%d> lcm vci_regulator get fail! ret[%ld].\n", __func__, __LINE__, PTR_ERR(ctx->vci_regulator));
        } else {
            ret = regulator_set_voltage(ctx->vci_regulator, 3000000, 3000000);
            if (ret < 0)
                dev_err(ctx->dev, "<%s:%d>set lcm vci_regulator vol error!ret[%d]\n", __func__, __LINE__, ret);
            ret = regulator_enable(ctx->vci_regulator);
            if (ret < 0)
                dev_err(ctx->dev, "<%s:%d>enable lcm vci_regulator error!ret[%d]\n", __func__, __LINE__, ret);
        }
        mdelay(12); //DDIC spec need more 10ms
    } else {
        //lcm reset pull low after mipipoweroff
        ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
        if (IS_ERR_OR_NULL(ctx->reset_gpio)) {
            dev_err(ctx->dev, "cannot get reset_gpio %ld\n", PTR_ERR(ctx->reset_gpio));
        } else {
            gpiod_set_value(ctx->reset_gpio, 0);
            devm_gpiod_put(ctx->dev, ctx->reset_gpio);
        }
        mdelay(2);

        //VCI 3.0V disable
        if (IS_ERR_OR_NULL(ctx->vci_regulator)) {
            dev_err(ctx->dev, "<%s,%d> lcm vci_regulator get fail! ret[%ld].\n", __func__, __LINE__, PTR_ERR(ctx->vci_regulator));
        } else {
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
        }
        mdelay(2);

        //DVDD 1.25V disable
        ctx->dvdd_gpio = devm_gpiod_get(ctx->dev, "dvdd", GPIOD_OUT_HIGH);
        if (IS_ERR_OR_NULL(ctx->dvdd_gpio)) {
            dev_err(ctx->dev, "cannot get dvdd_gpio %ld\n", PTR_ERR(ctx->dvdd_gpio));
        } else {
            gpiod_set_value(ctx->dvdd_gpio, 0);
            devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
        }
        mdelay(40);

        //VDDI 1.8V disable
        ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
        if (IS_ERR_OR_NULL(ctx->vddi_gpio)) {
            dev_err(ctx->dev, "cannot get vddi_gpio %ld\n", PTR_ERR(ctx->vddi_gpio));
        } else {
            gpiod_set_value(ctx->vddi_gpio, 0);
            devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
        }
        mdelay(2);

        lcm_unprepare_time = jiffies_to_msecs(jiffies);
    }
    pr_info("[LCM] %s end\n", __func__);
    return 0;
}
static int panel_doze_enable(struct drm_panel *panel,
            void *dsi, dcs_write_gce cb, void *handle)
{
//    struct lcm *ctx = panel_to_lcm(panel);
    UNUSED(panel);
    UNUSED(dsi);
    UNUSED(cb);
    UNUSED(handle);
    pr_info("[LCM] %s begin\n", __func__);
//    push_table_cb(dsi, cb, handle, aod_mode_enter_setting,
//        sizeof(aod_mode_enter_setting) / sizeof(struct LCM_setting_table));
    g_aod_enable = 1;
    pr_info("[LCM] %s end\n", __func__);
    return 0;
}
static int panel_doze_disable(struct drm_panel *panel,
            void *dsi, dcs_write_gce cb, void *handle)
{
//    struct lcm *ctx = panel_to_lcm(panel);
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
        aod_mode_enter_setting[1].para_list[0] = 0x00;
        aod_mapped_level = 0x2E;  //10nit
    } else if (level >= 100 && level < 300) {
        aod_mode_enter_setting[1].para_list[0] = 0x01;
        aod_mapped_level = 0x4B;  //15nit
    } else if (level >= 300 && level < 600) {
        aod_mode_enter_setting[1].para_list[0] = 0x02;
        aod_mapped_level = 0x129;  //60nit
    } else {
        aod_mode_enter_setting[1].para_list[0] = 0x02;
        aod_mapped_level = 0x129;  //60nit
    }
    aod_mode_enter_setting[0].para_list[0] = ((aod_mapped_level >> 8) & 0x0f);
    aod_mode_enter_setting[0].para_list[1] = (aod_mapped_level & 0xff);
    push_table_cb(dsi, cb, handle, aod_mode_enter_setting,
        sizeof(aod_mode_enter_setting) / sizeof(struct LCM_setting_table));
    pr_info("[LCM] %s level is %u, CMD 0x6D's value is 0x%x\n", __func__,
            level, aod_mode_enter_setting[0].para_list[0]);
    g_set_aod_light =1;
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
        if (mapped_level <= 0x190) {
            hbm_mode_enter_setting[2].para_list[0] = alpha_gain[mapped_level][0]; //dynamic selection from alpha_gain table
            hbm_mode_enter_setting[2].para_list[1] = alpha_gain[mapped_level][1]; //dynamic selection from alpha_gain table
            hbm_mode_enter_setting[2].para_list[2] = 0x01; //default value:0x01
            hbm_mode_enter_setting[2].para_list[3] = 0x91; //default value:0x91
        } else {
            hbm_mode_enter_setting[2].para_list[0] = 0x10; //default value:0x10, no need change
            hbm_mode_enter_setting[2].para_list[1] = 0x00; //default value:0x00, no need change
            hbm_mode_enter_setting[2].para_list[2] = ((mapped_level >> 8) & 0x0f); //dynamic selection from current backlight value
            hbm_mode_enter_setting[2].para_list[3] = (mapped_level & 0xff); //dynamic selection from current backlight value
        }
        push_table_cb(dsi, cb, handle, hbm_mode_enter_setting,
            sizeof(hbm_mode_enter_setting) / sizeof(struct LCM_setting_table));

        pr_info("[LCM] %s HBM_CMD=0x%X, HBM_PARA=0x%X,0x%X,0x%X,0x%X\n", __func__, hbm_mode_enter_setting[2].cmd,
                hbm_mode_enter_setting[2].para_list[0], hbm_mode_enter_setting[2].para_list[1],
                hbm_mode_enter_setting[2].para_list[2], hbm_mode_enter_setting[2].para_list[3]);
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
        if (mapped_level <= 0x190) {
            hbm_mode_enter_setting[2].para_list[0] = alpha_gain[mapped_level][0]; //dynamic selection from alpha_gain table
            hbm_mode_enter_setting[2].para_list[1] = alpha_gain[mapped_level][1]; //dynamic selection from alpha_gain table
            hbm_mode_enter_setting[2].para_list[2] = 0x01; //default value:0x01, no need change
            hbm_mode_enter_setting[2].para_list[3] = 0x91; //default value:0x91, no need change
        } else {
            hbm_mode_enter_setting[2].para_list[0] = 0x10; //default value:0x10, no need change
            hbm_mode_enter_setting[2].para_list[1] = 0x00; //default value:0x00, no need change
            hbm_mode_enter_setting[2].para_list[2] = ((mapped_level >> 8) & 0x0f); //dynamic selection from current backlight value
            hbm_mode_enter_setting[2].para_list[3] = (mapped_level & 0xff); //dynamic selection from current backlight value
        }
        push_table_cb(dsi, cb, handle, hbm_mode_enter_setting,
            sizeof(hbm_mode_enter_setting) / sizeof(struct LCM_setting_table));
    }else {
        push_table_cb(dsi, cb, handle, hbm_mode_exit_setting,
            sizeof(hbm_mode_exit_setting) / sizeof(struct LCM_setting_table));
    }

    pr_info("[LCM] %s HBM_CMD=0x%x, HBM_PARA=0x%x,0x%x,0x%x,0x%x\n", __func__,
            hbm_mode_enter_setting[2].cmd,
            hbm_mode_enter_setting[2].para_list[0],
            hbm_mode_enter_setting[2].para_list[1],
            hbm_mode_enter_setting[2].para_list[2],
            hbm_mode_enter_setting[2].para_list[3]);

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
    .set_backlight_cmdq = lcm_setbacklight_cmdq,
#if IS_ENABLED(CONFIG_TRAN_DDIC_OSC_TRACKING)
    .set_osc_tracking_cmdq = lcm_set_osc_tracking_cmdq,
#endif
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
    struct drm_display_mode *mode_144hz;
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
    panel_driver_status.dimming_status = DIMMING_DEFAULT_STATUS;    // default dimming status
    ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;
    mipi_dsi_set_drvdata(dsi, ctx);
    ctx->dev = dev;
    dsi->lanes = 4;
    dsi->format = MIPI_DSI_FMT_RGB888;
    dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
            /*MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET*/
            //MIPI_DSI_CLOCK_NON_CONTINUOUS;
    backlight = of_parse_phandle(dev->of_node, "backlight", 0);
    if (backlight) {
        ctx->backlight = of_find_backlight_by_node(backlight);
        of_node_put(backlight);
        if (!ctx->backlight)
            return -EPROBE_DEFER;
    }

    //VDDI
    ctx->vddi_gpio = devm_gpiod_get(dev, "vddi", GPIOD_OUT_HIGH);
    if (IS_ERR_OR_NULL(ctx->vddi_gpio)) {
        dev_err(ctx->dev, "cannot get vddi_gpio %ld\n", PTR_ERR(ctx->vddi_gpio));
    } else {
        devm_gpiod_put(dev, ctx->vddi_gpio);
    }

    //dvdd
    ctx->dvdd_gpio = devm_gpiod_get(dev, "dvdd", GPIOD_OUT_HIGH);
    if (IS_ERR_OR_NULL(ctx->dvdd_gpio)) {
        dev_err(ctx->dev, "cannot get dvdd_gpio %ld\n", PTR_ERR(ctx->dvdd_gpio));
    } else {
        devm_gpiod_put(dev, ctx->dvdd_gpio);
    }

    //vci_regulator
    ctx->vci_regulator = devm_regulator_get(ctx->dev, "vci");
    if (IS_ERR_OR_NULL(ctx->vci_regulator)) {
        dev_err(ctx->dev, "<%s,%d> lcm vci_regulator get fail! ret[%ld].\n", __func__, __LINE__, PTR_ERR(ctx->vci_regulator));
    } else {
        ret = regulator_set_voltage(ctx->vci_regulator, 3000000, 3000000);
        if (ret < 0)
            dev_err(ctx->dev, "<%s:%d>set lcm vci_regulator vol error!ret[%d]\n", __func__, __LINE__, ret);
        ret = regulator_enable(ctx->vci_regulator);
        if (ret < 0)
            dev_err(ctx->dev, "<%s:%d>enable lcm vci_regulator error!ret[%d]\n", __func__, __LINE__, ret);
    }

    //reset
    ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
   if (IS_ERR_OR_NULL(ctx->dvdd_gpio)) {
        dev_err(ctx->dev, "cannot get reset_gpio %ld\n", PTR_ERR(ctx->reset_gpio));
    } else {
        devm_gpiod_put(dev, ctx->reset_gpio);
    }

    if (of_property_read_bool(dev->of_node, "lcm-longh30hz-support")) {
        lcm_longh30hz_support = true;
        pr_info("panel support longh 30hz\n");
    }
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

    if(g_lcm_fresh_mode == MODE_0_FPS)
        cpu_qos_update(min_freq);

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
        lcm_dcs_write_seq_static(ctx, 0x6c,0x02);
        mdelay(10);
        lcm_dcs_write_seq_static(ctx, 0x28);
        mdelay(10);
        lcm_dcs_write_seq_static(ctx, 0xF0,0xAA,0x10);
        lcm_dcs_write_seq_static(ctx, 0xD0,0xA4);
        lcm_dcs_write_seq_static(ctx, 0xD1,0x3E);
        mdelay(11);
        lcm_dcs_write_seq_static(ctx, 0x10);
        mdelay(80);

        //lcm reset pull low after mipipoweroff
        ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
        if (IS_ERR_OR_NULL(ctx->reset_gpio)) {
            dev_err(ctx->dev, "cannot get reset_gpio %ld\n", PTR_ERR(ctx->reset_gpio));
        } else {
            gpiod_set_value(ctx->reset_gpio, 0);
            devm_gpiod_put(ctx->dev, ctx->reset_gpio);
        }
        mdelay(2);

        //VCI 3.0V disable
        if (IS_ERR_OR_NULL(ctx->vci_regulator))
            ctx->vci_regulator = devm_regulator_get(ctx->dev, "vci");
        if (IS_ERR_OR_NULL(ctx->vci_regulator)) {
            dev_err(ctx->dev, "<%s,%d> lcm vci_regulator get fail! ret[%ld].\n", __func__, __LINE__, PTR_ERR(ctx->vci_regulator));
        } else {
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
        }
        mdelay(2);

        //DVDD 1.25V disable
        ctx->dvdd_gpio = devm_gpiod_get(ctx->dev, "dvdd", GPIOD_OUT_HIGH);
        if (IS_ERR_OR_NULL(ctx->dvdd_gpio)) {
            dev_err(ctx->dev, "cannot get dvdd_gpio %ld\n", PTR_ERR(ctx->dvdd_gpio));
        } else {
            gpiod_set_value(ctx->dvdd_gpio, 0);
            devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
        }
        mdelay(40);

        //VDDI 1.8V disable
        ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
        if (IS_ERR_OR_NULL(ctx->vddi_gpio)) {
            dev_err(ctx->dev, "cannot get vddi_gpio %ld\n", PTR_ERR(ctx->vddi_gpio));
        } else {
            gpiod_set_value(ctx->vddi_gpio, 0);
            devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
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
        .compatible = "vtdr6126a,fhdp,dsi,vdo,dsc,tm,144hz,kn6",
    },
    {} };
MODULE_DEVICE_TABLE(of, lcm_of_match);
static struct mipi_dsi_driver lcm_driver = {
    .probe = lcm_probe,
    .remove = lcm_remove,
    .shutdown = lcm_shutdown,
    .driver = {
            .name =  "vtdr6126a_fhdp_dsi_vdo_dsc_tm_tm_144hz_kn6",
            .owner = THIS_MODULE,
            .of_match_table = lcm_of_match,
        },
};
module_mipi_dsi_driver(lcm_driver);
MODULE_AUTHOR("long.chen<long.chen@transsion.com>");
MODULE_DESCRIPTION("VTDR6126A VDO 120hz AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
