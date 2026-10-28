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
// #include "mtk_drm_gateic.h"
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
// #include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif
#include "tran_drm_panel_i2c.h"
//USD: add tp gesture by longtao.ran 20240620 start
#define TRAN_CTP_GESTURE_FUN
#if defined(TRAN_CTP_GESTURE_FUN)
/*This variable is for kernel-5.10 TP new driver frame*/
int tran_new_gesture_fun_enable = 0;
#endif
extern u32 tran_call_tp_cmd_handle(u32 cmd, void *pdata);
//USD: add tp gesture by longtao.ran 20240620 end
//#USD:add for gesture Decoupling by junyu.chen2 20251216 start
//#if IS_ENABLED(CONFIG_FTS_MOVE_UPDATEFW_TO_LCD)
//extern void fts_fwresume_work(void);
//#endif
//#USD:add for gesture Decoupling by junyu.che2 20251216 end
//BSP:add for lcd_dimmig by qian.wu3 20251018 start
static char bl_dimming_enable[] = {0x53, 0x2C};
static char bl_dimming_disable[] = {0x53, 0x24};
static unsigned int g_dim_enable = 0;
static unsigned int g_need_dim_enable = 0;
static unsigned int mapped_level = 0;
//BSP:add for lcd_dimmig by qian.wu3 20251018 end
#define REGFLAG_DELAY               0xFFFC
#define REGFLAG_UDELAY              0xFFFB
#define REGFLAG_END_OF_TABLE        0xFFFD

#define FRAME_WIDTH		720
#define FRAME_HEIGHT		1576
#define PHYSICAL_WIDTH		71561
#define PHYSICAL_HEIGHT		156639
#define HSA			8
#define HBP			16
#define HFP			28
#define VSA			6
#define VBP			30
#define DATA_RATE		1196
/*Parameter setting for mode 120 Start*/
#define MODE_120_FPS        120
#define MODE_120_VFP        292 
/*Parameter setting for mode 120 End*/
/*Parameter setting for mode 90 Start*/
#define MODE_90_FPS     90
#define MODE_90_VFP     928
/*Parameter setting for mode 90 End*/
/*Parameter setting for mode 60 Start*/
#define MODE_60_FPS     60
#define MODE_60_VFP     2198
/*Parameter setting for mode 60 End*/
/*Parameter setting for mode 45 Start*/
#define MODE_45_FPS     45
#define MODE_45_VFP     3466
/*Parameter setting for mode 45 End*/

static char bl_tb0[] = {0x51, 0x0D, 0xBB};
struct OCP2131_SETTING_TABLE {
	unsigned char cmd;
	unsigned char data;
};
static struct OCP2131_SETTING_TABLE ocp2131_cmd_data[2] = {
	{0x00, 0x14},    //AVDD +6.0
	{0x01, 0x14},    //AVEE -6.0
};
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	//struct backlight_device *backlight;
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
	{0x00,1,{0x00}},
	{0xFF,3,{0x80,0x57,0x01}},
	{0x00,1,{0x80}},
	{0xFF,2,{0x80,0x57}},
	{0x00,1,{0x00}},
	{0x2A,4,{0x00,0x00,0x02,0xCF}}, //720
	{0x00,1,{0x00}},
	{0x2B,4,{0x00,0x00,0x06,0x27}}, //1576
	{0x00,1,{0xA3}},  //Y=1576
	{0xB3,4,{0x06,0x28,0x00,0x18}},
	//--------------Voltage set--------------//
	{0x00,1,{0x93}},//VGH_N 15V
	{0xC5,1,{0x57}},
	{0x00,1,{0x97}},//VGH_I 15V
	{0xC5,1,{0x57}},
	{0x00,1,{0x9A}},//VGL_N -15V //3X(2*AVEE-AVDD)
	{0xC5,1,{0xDF}},
	{0x00,1,{0x9C}},//VGL_I -15V //3X(2*AVEE-AVDD)
	{0xC5,1,{0xDF}},
	{0x00,1,{0xB6}}, //VGHO1_N_I=14V
	{0xC5,2,{0x43,0x43}},
	{0x00,1,{0xB8}}, //VGLO1_N_I -14V
	{0xC5,2,{0x55,0x55}},
	{0x00,1,{0x00}},
	{0xD8,2,{0x35,0x39}},  //GVDDP/N  5.6V/-5.6V
	{0x00,1,{0x82}},
	{0xC5,1,{0x55}},  //LVD
	{0x00,1,{0x83}},
	{0xC5,1,{0x07}},  //LVD Enable
	{0x00,1,{0x96}},
	{0xf5,1,{0x19}},
	{0x00,1,{0x86}},
	{0xf5,1,{0x19}},
	{0x00,1,{0x94}},
	{0xC5,1,{0x13}},
	{0x00,1,{0x9B}},
	{0xC5,1,{0x31}},
	{0x00,1,{0xA3}},  //GVDD_EN
	{0xA5,1,{0x04}},
	{0x00,1,{0x99}},
	{0xCF,1,{0x56}},
	{0x00,1,{0x86}},
	{0xB7,1,{0x80}}, //I2C EN
	{0x00,1,{0xA5}},
	{0xB0,1,{0x0D}}, //
	{0x00,1,{0x90}}, //MIPI 4lane
	{0xFF,1,{0x00}},
	{0x00,1,{0x80}},
	{0xCE,1,{0x00}},
	// VB term EN
	{0x00,1,{0xD0}},
	{0xCE,1,{0x01}},
	//RTN  Compensation off
	{0x00,1,{0xE0}},
	{0xCE,1,{0x00}},
	//follow MIPI video VS/HS
	{0x00,1,{0xA1}},
	{0xC1,1,{0xCC}},
	//fifo mode Vsync shift
	{0x00,1,{0xA5}},
	{0xC1,2,{0x00 ,0x10}},
	//VFP=345
	{0x00,1,{0x70}},
	{0xC0,6,{0x00,0x65,0x01,0x23,0x00,0x24}},
	//normal mode VB term2： 1.4ms
	{0x00,1,{0xD1}},
	{0xCE,7,{0x00,0x0C,0x01,0x01,0x01,0x12,0x01}},
	//LPF/FIFO mode VB term2： 1.4ms
	{0x00,1,{0xE8}},
	{0xCE,4,{0x01,0x12,0x01,0x12}},
	{0x00,1,{0xB0}},
	{0xC0,5,{0x00,0x65,0x01,0x24,0x24}},
	{0x00,1,{0x80}},
	{0xC1,2,{0x22 ,0x22 }},
	{0x00,1,{0x90}},
	{0xC1,1,{0x02}},
	{0x00,1,{0xF1}},
	{0xCF,1,{0x78}},
	{0x00,1,{0xF5}},
	{0xCF,1,{0x00}},
	{0x00,1,{0xF6}},
	{0xCF,1,{0x78}},
	//Mode switch by CMD2
	{0x00,1,{0xF7}},
	{0xCF,1,{0x11}},
	// fifo mode Vstart&Vmid int
	{0x00,1,{0x70}},
	{0xCF,8,{0x06 ,0x06 ,0x50 ,0x54 ,0x02 ,0x02 ,0x3E ,0x42}},
	//fifo mode Vend int
	{0x00,1,{0xC0}},
	{0xCF,4,{0x04 ,0x04 ,0x0A ,0x0E}},
	// fifo mode Vblank int
	{0x00,1,{0xC5}},
	{0xCF,4,{0x06,0x06,0x2E,0x32}},
	//--------------Mapping--------------//
	//panel mapping setting
	//u2d_L
	{0x00,1,{0x80}},
	{0xCC,16,{0x00,0x26,0x1D,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x1C,0x02,0x04,0x00,0x00}},
	{0x00,1,{0x90}},
	{0xCC,8,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//u2d_R
	{0x00,1,{0x80}},
	{0xCD,16,{0x00,0x26,0x1D,0x07,0x09,0x0B,0x0D,0x0F,0x11,0x13,0x15,0x1C,0x03,0x05,0x00,0x00}},
	{0x00,1,{0x90}},
	{0xCD,8,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//power off enmode setting
	{0x00,1,{0x80}},
	{0xCB,16,{0xCD,0xCD,0xCD,0xCD,0xCD,0xCD,0xCD,0xCD,0xCD,0xCD,0xCD,0xCD,0xCD,0xCD,0xCD,0xC1}},
	{0x00,1,{0xED}},
	{0xCB,1,{0xCD}},
	//power on enmode setting
	{0x00,1,{0x90}},
	{0xCB,16,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00,0x00,0x00,0x00}},
	{0x00,1,{0xEE}},
	{0xCB,1,{0x00}},
	{0x00,1,{0x90}},
	{0xC3,1,{0x00}},
	//skip & powr on1 enmode setting
	{0x00,1,{0xA0}},
	{0xCB,8,{0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00}},
	//power off blank enmode setting
	{0x00,1,{0xB0}},
	{0xCB,4,{0x55,0x55,0x55,0x55}},
	//power on blank enmode setting
	{0x00,1,{0xC0}},
	{0xCB,4,{0x55,0x55,0x55,0x55}},
	//power on blank enmode setting
	{0x00,1,{0xD2}},
	{0xCB,11,{0x81,0x00,0x81,0x81,0x00,0x81,0x81,0x00,0x81,0x81,0x00}},
	{0x00,1,{0xE0}},
	{0xCB,13,{0x81,0x81,0x00,0x81,0x81,0x00,0x81,0x81,0x00,0x81,0x81,0x00,0x81}},
	{0x00,1,{0xFA}},
	{0xCB,2,{0x81,0x00}},
	{0x00,1,{0xEF}},
	{0xCB,1,{0x05}},
	//--------------GIP Timming--------------//
	//STV1 
	{0x00,1,{0x68}},
	{0xC2,4,{0x92,0x07,0x00,0x00}},
	//STV2
	{0x00,1,{0x6C}},
	{0xC2,4,{0x91,0x07,0x00,0x00}},
	//STV3
	{0x00,1,{0x70}},
	{0xC2,4,{0x8E,0x07,0x00,0x00}},
	//STV4
	{0x00,1,{0x74}},
	{0xC2,4,{0x8D,0x07,0x00,0x00}},
	//CKV1
	{0x00,1,{0x8C}},
	{0xC2,5,{0x84,0x00,0x05,0x00,0x90}},
	//CKV2
	{0x00,1,{0x91}},
	{0xC2,5,{0x83,0x00,0x05,0x00,0x90}},
	//CKV3
	{0x00,1,{0x96}},
	{0xC2,5,{0x82,0x00,0x05,0x00,0x90}},
	//CKV4
	{0x00,1,{0x9B}},
	{0xC2,5,{0x81,0x00,0x05,0x00,0x90}},
	//CKV5
	{0x00,1,{0xA0}},
	{0xC2,5,{0x80,0x00,0x05,0x00,0x90}},
	//CKV6
	{0x00,1,{0xA5}},
	{0xC2,5,{0x01,0x00,0x05,0x00,0x90}},
	//CKV7
	{0x00,1,{0xAA}},
	{0xC2,5,{0x02,0x00,0x05,0x00,0x90}},
	//CKV8
	{0x00,1,{0xAF}},
	{0xC2,5,{0x03,0x01,0x05,0x00,0x90}},
	//CKV9
	{0x00,1,{0xB4}},
	{0xC2,5,{0x8C,0x02,0x05,0x00,0x90}},
	//CKV10
	{0x00,1,{0xB9}},
	{0xC2,5,{0x8B,0x03,0x05,0x00,0x90}},
	//CKV11
	{0x00,1,{0xBE}},
	{0xC2,5,{0x8A,0x04,0x05,0x00,0x90}},
	//CKV12
	{0x00,1,{0xC3}},
	{0xC2,5,{0x89,0x05,0x05,0x00,0x90}},
	//CKV13
	{0x00,1,{0xC8}},
	{0xC2,5,{0x88,0x06,0x05,0x00,0x90}},
	//CKV14
	{0x00,1,{0xCD}},
	{0xC2,5,{0x87,0x07,0x05,0x00,0x90}},
	//CKV15
	{0x00,1,{0xD2}},
	{0xC2,5,{0x86,0x08,0x05,0x00,0x90}},
	//CKV16
	{0x00,1,{0xD7}},
	{0xC2,5,{0x85,0x09,0x05,0x00,0x90}},
	//CKV width setting
	{0x00,1,{0xDC}},
	{0xC2,8,{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}},
	{0x00,1,{0xFC}},        //GCL=STV0,GCH=GCH
	{0xCB,2,{0x08,0x40}},   //CBFD=40,GCL INV
	
	{0x00,1,{0xFB}},        //GCH/GCL start&end point
	{0xC3,4,{0x95,0x09,0x95,0x09}},
	//--------------Source setting--------------//
	{0x00,1,{0x98}},
	{0xC4,1,{0x08}},
	{0x00,1,{0x90}},
	{0xE9,5,{0x10,0xFF,0xFF,0xFF,0x00}},
	{0x00,1,{0x85}},//Posdmy pch
	{0xC4,1,{0x80}},
	{0x00,1,{0x81}},
	{0xA4,1,{0x73}},
	{0x00,1,{0x86}},//4VB pch
	{0xA4,1,{0xB6}},
	{0x00,1,{0x95}},//VB pch data
	{0xC4,1,{0xFF}},
	{0x00,1,{0x93}},
	{0xC4,1,{0x8F}},
	{0x00,1,{0x80}},
	{0xB3,1,{0x17}},
	{0x00,1,{0xA5}},
	{0xB0,1,{0x0D}}, //RC delay 95ns
	{0x00,1,{0xCA}},//Power on 3
	{0xC0,2,{0x90,0x11}},
	{0x00,1,{0xB7}},//sd_en_sdpl_sel
	{0xF5,1,{0x1D}},
	{0x00,1,{0x90}},
	{0xC3,1,{0x00}},
	{0x00,1,{0xB1}},
	{0xF5,1,{0x11}},
	{0x00,1,{0xB0}},
	{0xC5,1,{0x00}},
	{0x00,1,{0xB3}},
	{0xC5,1,{0x00}},
	{0x00,1,{0xB2}},
	{0xC5,1,{0x0D}},
	{0x00,1,{0xB5}},
	{0xC5,1,{0x02}},
	{0x00,1,{0xC2}},
	{0xF5,1,{0x42}},
	{0x00,1,{0xBE}}, //bist mode边缘显示margin
	{0xC0,1,{0x06}},
	{0x00,1,{0xAD}},
	{0xC1,3,{0xFF,0xFF,0xFF}},
	{0x00,1,{0xB0}},
	{0xCA,3,{0x00,0x00,0x0C}},
	{0x00,1,{0xB5}},
	{0xCA,1,{0x08}},
	{0x00,1,{0x00}},
	{0xFF,3,{0x00,0x00,0x00}},
	{0x00,1,{0x80}},
	{0xFF,2,{0x00,0x00}},
	{0x51,2,{0x00,0x00}},
	{0x53,1,{0x24}},
	{0x55,1,{0x00}},
	{0x11,1,{0x00}},
	{REGFLAG_DELAY,120,{}},
	{0x29,1,{0x00}},
	{0x35,1,{0x00}},
	{REGFLAG_DELAY,20,{}}
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
static void lcm_panel_init(struct lcm *ctx)
{
	void *tp_data = NULL;
	int ret;
	pr_info("[LCM] %s begin\n", __func__);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(15);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	//#USD:add for gesture Decoupling by junyu.chen2 20251216 start
//#if IS_ENABLED(CONFIG_FTS_MOVE_UPDATEFW_TO_LCD)
	pr_info("%s lcm call tp to download fw", __func__);
	//fts_fwresume_work();
	ret = tran_call_tp_cmd_handle(1005,tp_data);
  	if (ret < 0) {
  		pr_info("%s call tran_call_tp_cmd_handle failed to download fw, ret=%d\n", __func__, ret);
  	}
	mdelay(10);
//#endif
//#USD:add for gesture Decoupling by junyu.chen2 20251216 end

	push_table(ctx, lcm_initialization_setting,
		sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table));
	//ESD
	if (mapped_level) {
		pr_info("[LCM] %s esd recovery, need set curr_bl=%d \n", __func__, mapped_level);
		lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
	}
	ctx->error=0;
	g_dim_enable = 0;
	g_need_dim_enable = 0;
	pr_info("[LCM] %s end\n", __func__);
}

static struct LCM_setting_table lcm_suspend_setting[] = {
	// Display off sequence
	{0x28, 0, {}},
	{REGFLAG_DELAY, 20, {}},
	// Sleep Mode On
	{0x10, 0, {}},
	{REGFLAG_DELAY, 120, {}},
};
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
	push_table(ctx, lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table));
#if defined (TRAN_CTP_GESTURE_FUN)
 	tran_new_gesture_fun_enable = tran_call_tp_cmd_handle(1003,NULL);
	printk("ctp tran_new_gesture_fun_enable = %d\n", tran_new_gesture_fun_enable);
	if (!tran_new_gesture_fun_enable) {
#endif
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->reset_gpio)) {
			pr_err("cannot get reset-gpios %ld\n", PTR_ERR(ctx->reset_gpio));
			return PTR_ERR(ctx->reset_gpio);
		}
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		mdelay(5);
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
#if defined (TRAN_CTP_GESTURE_FUN)
	}
#endif
	ctx->error = 0;
	ctx->prepared = false;
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}
static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;
	int i = 0;
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
	for (i = 0; i < ARRAY_SIZE(ocp2131_cmd_data); i++) {
		tran_panel_i2c_write_bytes(ocp2131_cmd_data[i].cmd, ocp2131_cmd_data[i].data);
		mdelay(1);
	}
	mdelay(5);
	lcm_panel_init(ctx);
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
static const struct drm_display_mode switch_mode_45hz = {
	.clock = (int)((FRAME_WIDTH + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_45_VFP + VSA + VBP) * MODE_45_FPS / 1000),
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_45_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_45_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_45_VFP + VSA + VBP,
};


#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_120hz = {
	.data_rate = DATA_RATE,
	.ssc_enable=0,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0D, .count = 1, .para_list[0] = 0x00,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0x0B, .count = 1, .para_list[0] = 0x00,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = 1228,
		.hbp = 36,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.tran_panel_params = &panel_driver_status,
};
static struct mtk_panel_params ext_params_90hz = {
	.data_rate = DATA_RATE,
	.ssc_enable=0,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0D, .count = 1, .para_list[0] = 0x00,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0x0B, .count = 1, .para_list[0] = 0x00,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = 1228,
		.hbp = 36,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.tran_panel_params = &panel_driver_status,
};
static struct mtk_panel_params ext_params_60hz = {
	.data_rate = DATA_RATE,
	.ssc_enable=0,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0D, .count = 1, .para_list[0] = 0x00,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0x0B, .count = 1, .para_list[0] = 0x00,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = 1228,
		.hbp = 36,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.tran_panel_params = &panel_driver_status,
};
static struct mtk_panel_params ext_params_45hz = {
	.data_rate = DATA_RATE,
	.ssc_enable=0,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0D, .count = 1, .para_list[0] = 0x00,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0x0B, .count = 1, .para_list[0] = 0x00,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = 1228,
		.hbp = 36,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.tran_panel_params = &panel_driver_status,
};

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
	if (drm_mode_vrefresh(m) == MODE_120_FPS)
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == MODE_90_FPS)
		ext->params = &ext_params_90hz;
	else  if (drm_mode_vrefresh(m) == MODE_60_FPS)
		ext->params = &ext_params_60hz;
	else  if (drm_mode_vrefresh(m) == MODE_45_FPS)
		ext->params = &ext_params_45hz;
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
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	if(level > 5119){
		mapped_level = 4095;
	}else if(level <= 0){
		mapped_level = 0;
	}else if(level > 0 && level <= 8){
		mapped_level = 18;
	}else if(level > 8 && level <= 4095){
		mapped_level = 18 + ((level-8) * (3276-18) / (4095-8));
	}else{
		mapped_level = 3276 + ((level-4095) * (4095-3276) / 1024);
	}
	bl_tb0[1] = (mapped_level >> 4) & 0xFF;  // 高8位 DBV[11:4]
	bl_tb0[2] = mapped_level & 0x0F;         // 低4位 DBV[3:0]
	
	if ((g_dim_enable == 0) && (g_need_dim_enable == 1)) {	//enable dimming
		cb(dsi, handle, bl_dimming_enable, ARRAY_SIZE(bl_dimming_enable));
		g_dim_enable =  1;
	}

	g_need_dim_enable = 1;

	if (mapped_level == 0) {	//close dimming
		cb(dsi, handle, bl_dimming_disable, ARRAY_SIZE(bl_dimming_disable));
		g_dim_enable = 0;
		g_need_dim_enable = 0;
	}
	if (!cb)
		return -1;
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	pr_info("[LCM] %s level = %d, mapped_level = %d, dim_state = %d\n", __func__, level, mapped_level, g_dim_enable);
	return 0;
}

static int panel_set_aod_light_mode(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_aod_level[] = {0x51,0x0F,0xFF};
	unsigned int aod_mapped_level = 0;
	if (level != 0){
		if(level > 0 && level < 120){
			aod_mapped_level = 44;		//doze_backlight_level1 10nit
		}else if(level >= 120 && level < 180){
			aod_mapped_level = 92;		//doze_backlight_level1 15nit
		}else{
			aod_mapped_level = 353;		//doze_backlight_level1 60nit
		}
		pr_info("[%s]lcm AOD backlight level=%d, aod_mapped_level=%d\n", __func__, level, aod_mapped_level);
		bl_aod_level[1] = (aod_mapped_level >> 4) & 0xFF;
		bl_aod_level[2] = aod_mapped_level & 0x0F;
		cb(dsi, handle, bl_aod_level, ARRAY_SIZE(bl_aod_level));
	}
	mapped_level = aod_mapped_level;
	return 0;
}

#if 0
static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	pr_info("[LCM] %s begin\n", __func__);
	push_table_cb(dsi, cb, handle, aod_mode_enter_setting,
		sizeof(aod_mode_enter_setting) / sizeof(struct LCM_setting_table));
	g_aod_enable = 1;
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}
static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	pr_info("[LCM] %s begin\n", __func__);
	aod_mode_exit_setting[0].para_list[0] = bl_tb0[1];
	aod_mode_exit_setting[0].para_list[1] = bl_tb0[2];
	push_table_cb(dsi, cb, handle, aod_mode_exit_setting,
		sizeof(aod_mode_exit_setting) / sizeof(struct LCM_setting_table));
	g_aod_enable=0;
	g_dim_enable=0;
	g_need_dim_enable=1;
	pr_info("[LCM] %s end\n", __func__);
	return 0;
}
#endif
static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	.set_aod_light_mode = panel_set_aod_light_mode,
#if 0
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
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
	struct drm_display_mode *mode_45hz;
	struct drm_display_mode *mode_60hz;
	struct drm_display_mode *mode_90hz;
	struct drm_display_mode *mode_120hz;
	pr_info("[LCM] %s begin\n", __func__);
	mode_120hz = drm_mode_duplicate(connector->dev, &switch_mode_120hz);
	if (!mode_120hz) {
		dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_120hz.hdisplay, switch_mode_120hz.vdisplay, drm_mode_vrefresh(&switch_mode_120hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_120hz);
	mode_120hz->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_120hz);
	mode_90hz = drm_mode_duplicate(connector->dev, &switch_mode_90hz);
	if (!mode_90hz) {
		dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_90hz.hdisplay, switch_mode_90hz.vdisplay, drm_mode_vrefresh(&switch_mode_90hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_90hz);
	mode_90hz->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(connector, mode_90hz);
	mode_60hz = drm_mode_duplicate(connector->dev, &switch_mode_60hz);
	if (!mode_60hz) {
		dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_60hz.hdisplay, switch_mode_60hz.vdisplay, drm_mode_vrefresh(&switch_mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_60hz);
	mode_60hz->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(connector, mode_60hz);

	//mode_45hz
	mode_45hz = drm_mode_duplicate(connector->dev, &switch_mode_45hz);
	if (!mode_45hz) {
		dev_dbg(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_45hz.hdisplay, switch_mode_45hz.vdisplay, drm_mode_vrefresh(&switch_mode_45hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_45hz);
	mode_45hz->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(connector, mode_45hz);

	connector->display_info.width_mm = 69;
	connector->display_info.height_mm = 154;
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
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	unsigned int doze_backlight[] = {0, 0, 0, 0};
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
	of_property_read_u32_array(remote_node, "doze_backlight", doze_backlight, ARRAY_SIZE(doze_backlight));
	g_lcm_doze_backlight.doze_backlight_num = doze_backlight[0];
	g_lcm_doze_backlight.doze_backlight_level1 = doze_backlight[1];
	g_lcm_doze_backlight.doze_backlight_level2 = doze_backlight[2];
	g_lcm_doze_backlight.doze_backlight_level3 = doze_backlight[3];
	pr_info("[LCM] %s lcm_doze_backlight:%d;%d;%d;%d;\n", __func__, g_lcm_doze_backlight.doze_backlight_num,
	   g_lcm_doze_backlight.doze_backlight_level1, g_lcm_doze_backlight.doze_backlight_level2, g_lcm_doze_backlight.doze_backlight_level3);
	panel_driver_status.lcm_doze_backlight = &g_lcm_doze_backlight;
	//USD: added for X6895AEE-5207 by shiyu.cai 20260422 start
	panel_driver_status.tran_lcd_bl_opt_support = 1;
	//USD: added for X6895AEE-5207 by shiyu.cai 20260422 end
	pr_info("[LCM] %s begin\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE|MIPI_DSI_MODE_LPM;
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
	ret = mtk_panel_ext_create(dev, &ext_params_120hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
#if defined(CONFIG_TINNO_DEVINFO)
	FULL_PRODUCT_DEVICE_INFO(ID_LCD, dev->driver->name);
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
	{.compatible = "ft8057m,hdp,dsi,vdo,txd,boe,120hz,x6895b",},
	{}
};
MODULE_DEVICE_TABLE(of, lcm_of_match);
static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "ft8057m_hdp_dsi_vdo_txd_boe_120hz_x6895b",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};
module_mipi_dsi_driver(lcm_driver);
MODULE_AUTHOR("shiyu.cai <shiyu.cai@transsion.com>");
MODULE_DESCRIPTION("ft8057m VDO 120HZ LCD Panel Driver");
MODULE_LICENSE("GPL v2");
