/*
 *GPL-2.0
 * Copyright (c) 2020 MediaTek Inc.
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
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
// #include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif
#include "tran_drm_panel_i2c.h"
//=============== External/Internal variables =========================
#define REGFLAG_CMD                0xFFFA
#define REGFLAG_UDELAY             0xFFFB
#define REGFLAG_DELAY              0xFFFC
#define REGFLAG_END_OF_TABLE       0xFFFD
static unsigned int tran_is_lcm_poweroff = 0; //0:lcm_power state is poweron, 1:lcm_power state is poweroff
//=============== DDIC parameters =====================================
#define LCM_DSI_CMD_MODE                0
#define FRAME_WIDTH                     (1080)
#define FRAME_HEIGHT                    (2344)
#define PHYSICAL_WIDTH                  (71863)
#define PHYSICAL_HEIGHT                 (155970)
#define MIPI_CLOCK 510
#define DATA_RATE 1020

#define VSA 10
#define VBP 28
#define HSA 8
#define HBP 24
#define HFP 185
#define MODE_144_FPS 144
#define MODE_144_VFP 60
#define MODE_144_HFP 89
//Parameter setting for mode_0(120Hz)
#define MODE_120_FPS 120
#define MODE_120_VFP 60
//Parameter setting for mode_1(90Hz)
#define MODE_90_FPS 90
#define MODE_90_VFP 863
//Parameter setting for mode_2(60Hz)
#define MODE_60_FPS 60
#define MODE_60_VFP 2500
//Parameter setting for mode_3(45Hz)
#define MODE_45_FPS 45
#define MODE_45_VFP 4130
//=============== DSC parameters ======================================
#define DSC_ENABLE                1
#define DSC_VER                   17
#define DSC_SLICE_MODE            1
#define DSC_RGB_SWAP              0
#define DSC_DSC_CFG               34
#define DSC_RCT_ON                1
#define DSC_BIT_PER_CHANNEL       8
#define DSC_DSC_LINE_BUF_DEPTH    9
#define DSC_BP_ENABLE             1
#define DSC_BIT_PER_PIXEL         128
#define DSC_PIC_HEIGHT            2344
#define DSC_PIC_WIDTH             1080
#define DSC_SLICE_HEIGHT          8
#define DSC_SLICE_WIDTH           540
#define DSC_CHUNK_SIZE            540
#define DSC_XMIT_DELAY            512
#define DSC_DEC_DELAY             616
#define DSC_SCALE_VALUE           32
#define DSC_INCREMENT_INTERVAL    187
#define DSC_DECREMENT_INTERVAL    7
#define DSC_LINE_BPG_OFFSET       12
#define DSC_NFL_BPG_OFFSET        3511
#define DSC_SLICE_BPG_OFFSET      3255
#define DSC_INITIAL_OFFSET        6144
#define DSC_FINAL_OFFSET          4336
#define DSC_FLATNESS_MINQP        3
#define DSC_FLATNESS_MAXQP        12
#define DSC_RC_MODEL_SIZE         8192
#define DSC_RC_EDGE_FACTOR        6
#define DSC_RC_QUANT_INCR_LIMIT0  11
#define DSC_RC_QUANT_INCR_LIMIT1  11
#define DSC_RC_TGT_OFFSET_HI      3
#define DSC_RC_TGT_OFFSET_LO      3

//static unsigned int rc_buf_thresh[14] = {14, 28, 42, 56, 70, 84, 98, 105, 112, 119, 
//                                         121, 123, 125, 126};
//static unsigned int range_min_qp[15] = {0, 4, 0, 4, 1, 5, 1, 6, 3, 7, 3, 7, 3, 7, 3};//8bit
//static unsigned int range_max_qp[15] = {8, 3, 9, 3, 10, 5, 11, 5, 12, 5, 13, 7, 13, 13, 15};//8bit
//=============== LCD bias driver =====================================
#define TRAN_CTP_GESTURE_FUN
#if defined(TRAN_CTP_GESTURE_FUN)
//USD: add gesture by longtao.ran 20241212 start
/*This variable is for kernel-5.10 TP new driver frame*/
int tran_new_gesture_fun_enable = 0;
extern u32 tran_call_tp_cmd_handle(u32 cmd, void *pdata);
//USD: add gesture by longtao.ran 20241212 end
#endif
static int current_fps = 120;
static unsigned int mapped_level = 0;
static bool g_dimming_enable = 0;
static char bl_dimming_enable[] = {0x53, 0x2C};
static char bl_dimming_disable[] = {0x53, 0x24};
static char bl_tb0[] = {0x51, 0x0C, 0xF3}; //normal max 51_DBV
struct OCP2131_SETTING_TABLE {
	unsigned char cmd;
	unsigned char data;
};
static struct OCP2131_SETTING_TABLE ocp2131_cmd_data[2] = {
	{ 0x00, 0x0F }, //AVDD +5.5
	{ 0x01, 0x0F }, //AVEE -5.5
};
//call this function, need set mediatek-drm-gateic.ko to ko_order_table.csv
//such as: mediatek-drm-gateic.ko,/drivers/gpu/drm/panel/mediatek-drm-gateic.ko,ramdisk,Y,Y,user/userdebug/eng
//=============== lcm driver =====================================
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight; //For OLED backlight set
	struct gpio_desc *reset_gpio;
	struct gpio_desc *lcm_1v8_en;
	struct gpio_desc *avdd_en_gpio;
	struct gpio_desc *avee_en_gpio;
	struct regulator *vio18_regulator;
	bool prepared;
	bool enabled;
	int error;
};
tran_lcm_doze_backlight g_lcm_doze_backlight = {
	.doze_backlight_num = 0,
	.doze_backlight_level1 = 0,
	.doze_backlight_level2 = 0,
	.doze_backlight_level3 = 0,
};
struct tran_panel_driver_params panel_driver_status = {0};
/*
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
#define lcm_dcs_write_seq_static(ctx, seq...)                     \
	({                                                            \
		static const u8 d[] = {seq};                              \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                     \
	})
*/
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
		dev_err(ctx->dev, "error %d reading dcs seq:(0x%x)\n", ret, cmd);
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
struct LCM_setting_table_v2 {
    unsigned char count;
    unsigned char para_list[200];
};
static struct LCM_setting_table_v2 lcm_initialization_setting_v2[] = {
	{2,{0x00,0x00}},
	{4,{0xFF,0x87,0x25,0x01}},
	{2,{0x00,0x80}},
	{3,{0xFF,0x87,0x25}},
	{4,{0x1F,0x78,0x00,0x10}},
	{2,{0x00,0xF7}},
	{2,{0xCF,0x51}},
	{2,{0x00,0xB0}},
	{4,{0xCA,0x02,0x02,0x0C}},
	{2,{0x00,0xB5}},
	{2,{0xCA,0x04}},
	{2,{0x00,0xA0}},
	{4,{0xCA,0x05,0x05,0x05}},
	{2,{0x00,0x00}},
	{4,{0xFF,0xFF,0xFF,0xFF}},
	{2,{0x35,0x00}},
	{3,{0x51,0x00,0x00}},
	{2,{0x53,0x24}},
	{2,{0x55,0x00}},
	{1,{0x11}},
	{0,{120}},
	{1,{0x29}},
	{0,{20}},
};
static struct LCM_setting_table_v2 lcm_deep_sleep_mode_in_setting_v2[] = {
	{2,{0x28,0x00}},
	{0,{20}},
	{2,{0x10,0x00}},
	{0,{100}},
	// Deep Standby
	{2,{0x00,0x00}},
	{5,{0xF7,0x5A,0xA5,0x95,0x27}},
};

static int lcm_panel_init_v2(void *dsi_drv, struct drm_panel *panel, void *handle, mtk_dsi_ddic_cmd cb,
			struct mtk_dsi_cmd_option *cmd_opt)
{

	struct lcm *ctx = panel_to_lcm(panel);
	static char fps_144_cmd[] = {0x1F,0x90,0x00,0x00};
	static char fps_60_cmd[] = {0x1F,0x78,0x00,0x10};
	static struct mipi_dsi_msg fps_cmd = {0};
	static struct mipi_dsi_msg init_cmd[ARRAY_SIZE(lcm_initialization_setting_v2)] = {0};
	static int flag;
	int i, ret = 0;
	//USD: add gesture by longtao.ran 20241212 start
	//uint8_t tp_irq_en = 0;
	void *tp_data = NULL;
	pr_info("%s Start\n", __func__);
	pr_info("%s lcm disable tp irq", __func__);
        //USD: add for LJ7H782-749 by junyu.chen2 20250422 start
	//tran_call_tp_cmd_handle(1004,&tp_irq_en);
        //USD: add for LJ7H782-749 by junyu.chen2 20250422 end

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_err("[error]%s: cannot get reset_gpio %ld\n", __func__, PTR_ERR(ctx->reset_gpio));
		return -1;
	}
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(11);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	pr_info("%s lcm call tp to download fw", __func__);
	tran_call_tp_cmd_handle(1005,tp_data);
	//USD: add gesture by longtao.ran 20241212 end
	mdelay(10);

	if (!flag) {
		flag = 1;
		for (i = 0; i < ARRAY_SIZE(init_cmd); i++) {
			init_cmd[i].tx_len = lcm_initialization_setting_v2[i].count;
			init_cmd[i].tx_buf = lcm_initialization_setting_v2[i].para_list;
		}
	}
	struct mtk_dsi_cmd_msg cmd_msg = {
		.is_package = 0,
		.cmd_num = ARRAY_SIZE(init_cmd),
		.transfer_mode = PACKET_LP_MODE,
		.cmd_msg = init_cmd,
	};
	ret = cb(dsi_drv, handle, cmd_opt, &cmd_msg);
	if (ret < 0) {
		pr_err("%s: lcm_panel_init_v2 failed, ret = %d\n", __func__, ret);
		return ret;
	}

	// esd check
	if (mapped_level != 0) {
		if (current_fps == 144)
			fps_cmd.tx_buf = fps_144_cmd;
		else
			fps_cmd.tx_buf = fps_60_cmd;
		fps_cmd.tx_len = ARRAY_SIZE(fps_144_cmd);
		struct mtk_dsi_cmd_msg fps_msg = {
			.cmd_num = 1,
			.transfer_mode = PACKET_LP_MODE,
			.cmd_msg = &fps_cmd,
		};
		cb(dsi_drv, handle, cmd_opt, &fps_msg);

		struct mipi_dsi_msg bl_tb0_cmd = {0};
		bl_tb0_cmd.tx_buf = bl_tb0;
		bl_tb0_cmd.tx_len = ARRAY_SIZE(bl_tb0);
		struct mtk_dsi_cmd_msg bl_tb0_msg = {
			.cmd_num = 1,
			.transfer_mode = PACKET_LP_MODE,
			.cmd_msg = &bl_tb0_cmd,
		};
		cb(dsi_drv, handle, cmd_opt, &bl_tb0_msg);
	}
	ctx->error = 0;
	ctx->prepared = true;
	pr_info("%s End\n", __func__);

	return 0;
}

static int lcm_panel_deinit_v2(void *dsi_drv, struct drm_panel *panel, void *handle, mtk_dsi_ddic_cmd cb,
			struct mtk_dsi_cmd_option *cmd_opt)
{
	int ret = 0;
	int i;
	static int flag = 0;
	struct lcm *ctx = panel_to_lcm(panel);
	static struct mipi_dsi_msg suspend_cmd[ARRAY_SIZE(lcm_deep_sleep_mode_in_setting_v2)] = {0};

	pr_info("%s Start\n", __func__);
	if (!ctx->prepared)
	{
		pr_info("%s: lcm_panel_deinit_v2 not prepared\n", __func__);
		return 0;
	}
	if (!flag) {
		flag = 1;
		for (i = 0; i < ARRAY_SIZE(suspend_cmd); i++) {
			suspend_cmd[i].tx_len = lcm_deep_sleep_mode_in_setting_v2[i].count;
			suspend_cmd[i].tx_buf = lcm_deep_sleep_mode_in_setting_v2[i].para_list;
		}
	}
	struct mtk_dsi_cmd_msg suspend_cmd_msg = {
		.is_package = 0,
		.cmd_num = ARRAY_SIZE(suspend_cmd),
		.transfer_mode = PACKET_LP_MODE,
		.cmd_msg = suspend_cmd,
	};
	ret = cb(dsi_drv, handle, cmd_opt, &suspend_cmd_msg);
	if (ret < 0) {
		pr_err("%s: lcm_panel_deinit_v2 failed, ret = %d\n", __func__, ret);
		return ret;
	}
	pr_info("%s End\n", __func__);
	return 0;
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
{//lcm_suspend_power
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s Start\n", __func__);
	if (!ctx->prepared)
		return 0;
//when enable TP_gesture, panel no need power off
#if defined (TRAN_CTP_GESTURE_FUN)
	//USD: add gesture by longtao.ran 20241212 start
	tran_new_gesture_fun_enable = tran_call_tp_cmd_handle(1003,NULL);
	printk("ctp tran_new_gesture_fun_enable = %d g_shutdown_flag=%d\n", tran_new_gesture_fun_enable, g_shutdown_flag);
	//USD: add gesture by longtao.ran 20241212 end
	if (!tran_new_gesture_fun_enable && !g_shutdown_flag) {
#endif
		if (g_shutdown_flag != 0) {
			ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
			if (IS_ERR(ctx->reset_gpio)) {
				pr_err("[error]%s: cannot get reset_gpio %ld\n", __func__, PTR_ERR(ctx->reset_gpio));
			}
			gpiod_set_value(ctx->reset_gpio, 0);
			devm_gpiod_put(ctx->dev, ctx->reset_gpio);
			mdelay(5);
		}

		ctx->avee_en_gpio = devm_gpiod_get(ctx->dev, "avee", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->avee_en_gpio)) {
			pr_err("[error]%s: cannot get avee_en_gpio %ld\n", __func__, PTR_ERR(ctx->avee_en_gpio));
		}
		gpiod_set_value(ctx->avee_en_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->avee_en_gpio);
		mdelay(5);

		ctx->avdd_en_gpio = devm_gpiod_get(ctx->dev, "avdd", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->avdd_en_gpio)) {
			pr_err("[error]%s: cannot get avdd_en_gpio %ld\n", __func__, PTR_ERR(ctx->avdd_en_gpio));
		}
		gpiod_set_value(ctx->avdd_en_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->avdd_en_gpio);
		tran_is_lcm_poweroff = 1; //lcm_power(AVDD/AVEE) state is poweroff
#if defined (TRAN_CTP_GESTURE_FUN)
	}
#endif

	ctx->error = 0;
	ctx->prepared = false;
	pr_info("%s End\n", __func__);
	return 0;
}
static void lcm_shutdown(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
	pr_info("%s Start\n", __func__);
	if ( !tran_is_lcm_poweroff ) {  //when shutdown/restart, if lcm has been poweroff, no need poweroff again.
		pr_info("%s  lcm poweroff = %d, ctp_gesture = %d \n", __func__, tran_is_lcm_poweroff, tran_new_gesture_fun_enable);
		//0.tp/lcm reset gpio pulldown
		g_shutdown_flag = 2;
		//USD: add gesture by longtao.ran 20241212 start
		//tran_call_tp_cmd_handle(1001, &g_shutdown_flag);// TP RST pull down
		//USD: add gesture by longtao.ran 20241212 end

		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->reset_gpio)) {
			pr_err("[error]%s: cannot get reset_gpio %ld\n", __func__, PTR_ERR(ctx->reset_gpio));
			//return PTR_ERR(ctx->reset_gpio);
		}
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		mdelay(5);
		//1.lcm avee_2 gpio pulldown
		ctx->avee_en_gpio = devm_gpiod_get(ctx->dev, "avee", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->avee_en_gpio)) {
			pr_err("[error]%s: cannot get avee_en_gpio %ld\n", __func__, PTR_ERR(ctx->avee_en_gpio));
		}
		gpiod_set_value(ctx->avee_en_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->avee_en_gpio);
		mdelay(3);
		//2.lcm avdd gpio pulldown
		ctx->avdd_en_gpio = devm_gpiod_get(ctx->dev, "avdd", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->avdd_en_gpio)) {
			pr_err("[error]%s: cannot get avdd_en_gpio %ld\n", __func__, PTR_ERR(ctx->avdd_en_gpio));
		}
		gpiod_set_value(ctx->avdd_en_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->avdd_en_gpio);
		tran_is_lcm_poweroff = 1; //lcm_power(AVDD/AVEE) state is poweroff
		g_shutdown_flag = 0;
	}
	pr_info("%s End\n", __func__);
}
static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;
	int i = 0;
	pr_info("%s Start\n", __func__);
	if (ctx->prepared)
		return 0;
	/*
	if (IS_ERR_OR_NULL(ctx->vio18_regulator)) {
		ctx->vio18_regulator = devm_regulator_get(ctx->dev, "vmc");

		if (IS_ERR_OR_NULL(ctx->vio18_regulator)) {
			ret = PTR_ERR(ctx->vio18_regulator);
			dev_err(ctx->dev, "<%s,%d> lcm vci get fail!ret[%d].\n", __func__, __LINE__, ret);
			return 0;
		}
	}*/

	if (tran_is_lcm_poweroff) {
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->reset_gpio)) {
			pr_err("[error]%s: cannot get reset_gpio %ld\n", __func__, PTR_ERR(ctx->reset_gpio));
		}
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		mdelay(5);

		ctx->avdd_en_gpio = devm_gpiod_get(ctx->dev, "avdd", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->avdd_en_gpio)) {
			pr_err("[error]%s: cannot get avdd_en_gpio %ld\n", __func__, PTR_ERR(ctx->avdd_en_gpio));
		}
		gpiod_set_value(ctx->avdd_en_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->avdd_en_gpio);
		mdelay(5);

		ctx->avee_en_gpio = devm_gpiod_get(ctx->dev, "avee", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->avee_en_gpio)) {
			pr_err("[error]%s: cannot get avee_en_gpio %ld\n", __func__, PTR_ERR(ctx->avee_en_gpio));
		}
		gpiod_set_value(ctx->avee_en_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->avee_en_gpio);
		mdelay(2);
		for (i = 0; i < ARRAY_SIZE(ocp2131_cmd_data); i++) {
			tran_panel_i2c_write_bytes(ocp2131_cmd_data[i].cmd, ocp2131_cmd_data[i].data);
		}
		mdelay(5);
		tran_is_lcm_poweroff = 0; //lcm_power(AVDD/AVEE) state is poweron
	}
	//lcm_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);
	//ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	pr_info("%s End\n", __func__);
	return ret;
}

static int lcm_setbacklight_cmdq_v2(void *dsi_drv, mtk_dsi_ddic_cmd cb,
			void *handle, unsigned int level, struct mtk_dsi_cmd_option *cmd_opt)
{
	struct mipi_dsi_msg bl_tb0_cmd = { 0 };
	struct mipi_dsi_msg bl_dim_cmd = { 0 };

	if (level > 5119)
		level = 5119;

	if (level < 0)
		level = 0;

	// close dimming
	if (level == 0) {
		bl_dim_cmd.tx_buf = bl_dimming_disable;
		bl_dim_cmd.tx_len = ARRAY_SIZE(bl_dimming_disable);
		struct mtk_dsi_cmd_msg bl_dim_msg = {
			.cmd_num = 1,
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = &bl_dim_cmd,
		};
		cb(dsi_drv, handle, cmd_opt, &bl_dim_msg);
		g_dimming_enable = 0;
	} else if (!g_dimming_enable && mapped_level != 0) {
		bl_dim_cmd.tx_buf = bl_dimming_enable;
		bl_dim_cmd.tx_len = ARRAY_SIZE(bl_dimming_enable);
		struct mtk_dsi_cmd_msg bl_dim_msg = {
			.cmd_num = 1,
			.transfer_mode = PACKET_HS_MODE,
			.cmd_msg = &bl_dim_cmd,
		};
		cb(dsi_drv, handle, cmd_opt, &bl_dim_msg);
		g_dimming_enable = 1;
	}

	if(level <= 4095)
		mapped_level = level * 3276 / 4095; //normal backlight range 80% duty
	else
		mapped_level = 3276 + ((level-4096) * (4095-3276) / 1023); //hbm backlight range

	bl_tb0[1] = ((mapped_level >> 4) & 0xff);
	bl_tb0[2] = (mapped_level & 0x0f);

	if (!cb)
		return -1;

	bl_tb0_cmd.tx_buf = bl_tb0;
	bl_tb0_cmd.tx_len = 3;
	struct mtk_dsi_cmd_msg bl_tb0_msg = {
		.cmd_num = 1,
		.transfer_mode = PACKET_HS_MODE,
		.cmd_msg = &bl_tb0_cmd,
	};
	cb(dsi_drv, handle, cmd_opt, &bl_tb0_msg);
	pr_info("[LCM] %s level = %d, mapped_level = %d\n", __func__, level, mapped_level);

	return 0;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
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
static const struct drm_display_mode switch_mode_144hz = { //fps:144Hz porch params
	.clock       = ((FRAME_WIDTH  + MODE_144_HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_144_VFP + VSA + VBP) * MODE_144_FPS / 1000),
	.hdisplay    = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH  + MODE_144_HFP,
	.hsync_end   = FRAME_WIDTH  + MODE_144_HFP + HSA,
	.htotal      = FRAME_WIDTH  + MODE_144_HFP + HSA + HBP,
	.vdisplay    = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_144_VFP,
	.vsync_end   = FRAME_HEIGHT + MODE_144_VFP + VSA,
	.vtotal      = FRAME_HEIGHT + MODE_144_VFP + VSA + VBP,
};
static const struct drm_display_mode switch_mode_120hz = { //fps:120Hz porch params
	.clock       = ((FRAME_WIDTH  + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_120_VFP + VSA + VBP) * MODE_120_FPS / 1000),
	.hdisplay    = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH  + HFP,
	.hsync_end   = FRAME_WIDTH  + HFP + HSA,
	.htotal      = FRAME_WIDTH  + HFP + HSA + HBP,
	.vdisplay    = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_120_VFP,
	.vsync_end   = FRAME_HEIGHT + MODE_120_VFP + VSA,
	.vtotal      = FRAME_HEIGHT + MODE_120_VFP + VSA + VBP,
};
static const struct drm_display_mode switch_mode_90hz = { //fps:90Hz porch params
	.clock       = ((FRAME_WIDTH  + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_90_VFP + VSA + VBP) * MODE_90_FPS / 1000),
	.hdisplay    = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH  + HFP,
	.hsync_end   = FRAME_WIDTH  + HFP + HSA,
	.htotal      = FRAME_WIDTH  + HFP + HSA + HBP,
	.vdisplay    = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_90_VFP,
	.vsync_end   = FRAME_HEIGHT + MODE_90_VFP + VSA,
	.vtotal      = FRAME_HEIGHT + MODE_90_VFP + VSA + VBP,
};
static const struct drm_display_mode switch_mode_60hz = { //fps:60Hz porch params
	.clock       = ((FRAME_WIDTH  + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_60_VFP + VSA + VBP) * MODE_60_FPS / 1000),
	.hdisplay    = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH  + HFP,
	.hsync_end   = FRAME_WIDTH  + HFP + HSA,
	.htotal      = FRAME_WIDTH  + HFP + HSA + HBP,
	.vdisplay    = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_60_VFP,
	.vsync_end   = FRAME_HEIGHT + MODE_60_VFP + VSA,
	.vtotal      = FRAME_HEIGHT + MODE_60_VFP + VSA + VBP,
};
static const struct drm_display_mode switch_mode_45hz = { //fps:45Hz porch params
	.clock       = ((FRAME_WIDTH  + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_45_VFP + VSA + VBP) * MODE_45_FPS / 1000),
	.hdisplay    = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH  + HFP,
	.hsync_end   = FRAME_WIDTH  + HFP + HSA,
	.htotal      = FRAME_WIDTH  + HFP + HSA + HBP,
	.vdisplay    = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_45_VFP,
	.vsync_end   = FRAME_HEIGHT + MODE_45_VFP + VSA,
	.vtotal      = FRAME_HEIGHT + MODE_45_VFP + VSA + VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_144hz = { //120Hz,dsi params
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
//DSC ENABLE
	.ssc_enable = 0,
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
		.pic_height            =  DSC_PIC_HEIGHT,
		.pic_width             =  DSC_PIC_WIDTH,
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
		/*.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			//.range_bpg_ofs = range_bpg_ofs,
		},*/
	},
	.data_rate = DATA_RATE,
	.vdo_keep_hs_perline = 1,
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 4, {0x1F,0x90,0x00,0x00}},
		//.dfps_cmd_table[1] = {0, 2, {0xCF, 0x90}},
		//.dfps_cmd_table[2] = {0, 2, {0x00, 0xD2}},
		//.dfps_cmd_table[3] = {0, 2, {0xC0, 0x00}},

	},
	.tran_panel_params = &panel_driver_status,
/*
	.dyn = { //For mipi hopping
		.switch_en = 1,
		.pll_clk = 501,
		.hbp = 20,
	},
*/
};
static struct mtk_panel_params ext_params_120hz = { //120Hz,dsi params
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
//DSC ENABLE
	.ssc_enable = 0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	//.vfp_low_power = VFP_45HZ,
	.wait_sof_before_dec_vfp = 1,
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
		.pic_height            =  DSC_PIC_HEIGHT,
		.pic_width             =  DSC_PIC_WIDTH,
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
		/*.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			//.range_bpg_ofs = range_bpg_ofs,
		},*/
	},
	.data_rate = DATA_RATE,
	.vdo_keep_hs_perline = 1,
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 4, {0x1F,0x78,0x00,0x10}},
		//.dfps_cmd_table[1] = {0, 2, {0xCF, 0x78}},
		//.dfps_cmd_table[2] = {0, 2, {0x00, 0xD2}},
		//.dfps_cmd_table[3] = {0, 2, {0xC0, 0x01}},
	},
	.tran_panel_params = &panel_driver_status,
/*
	.dyn = { //For mipi hopping
		.switch_en = 1,
		.pll_clk = 501,
		.hbp = 20,
	},
*/
};
static struct mtk_panel_params ext_params_90hz = { //90Hz,dsi params
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
//DSC ENABLE
	.ssc_enable = 0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	//.vfp_low_power = VFP_45HZ,
	.wait_sof_before_dec_vfp = 1,
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
		.pic_height            =  DSC_PIC_HEIGHT,
		.pic_width             =  DSC_PIC_WIDTH,
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
		/*.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			//.range_bpg_ofs = range_bpg_ofs,
		},*/
	},
	.data_rate = DATA_RATE,
	.vdo_keep_hs_perline = 1,
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 4, {0x1F,0x78,0x00,0x10}},
		//.dfps_cmd_table[1] = {0, 2, {0xCF, 0x78}},
		//.dfps_cmd_table[2] = {0, 2, {0x00, 0xD2}},
		//.dfps_cmd_table[3] = {0, 2, {0xC0, 0x01}},
	},
	.tran_panel_params = &panel_driver_status,
/*
	.dyn = { //For mipi hopping
		.switch_en = 1,
		.pll_clk = 501,
		.hbp = 20,
	},
*/
};
static struct mtk_panel_params ext_params_60hz = { //60Hz,dsi params
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
//DSC ENABLE
	.ssc_enable = 0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	//.vfp_low_power = VFP_45HZ,
	.wait_sof_before_dec_vfp = 1,
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
		.pic_height            =  DSC_PIC_HEIGHT,
		.pic_width             =  DSC_PIC_WIDTH,
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
		/*.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			//.range_bpg_ofs = range_bpg_ofs,
		},*/
	},
	.data_rate = DATA_RATE,
	.vdo_keep_hs_perline = 1,
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 4, {0x1F,0x78,0x00,0x10}},
		//.dfps_cmd_table[1] = {0, 2, {0xCF, 0x78}},
		//.dfps_cmd_table[2] = {0, 2, {0x00, 0xD2}},
		//.dfps_cmd_table[3] = {0, 2, {0xC0, 0x01}},
	},
	.tran_panel_params = &panel_driver_status,
/*
	.dyn = { //For mipi hopping
		.switch_en = 1,
		.pll_clk = 501,
		.hbp = 20,
	},
*/
};
static struct mtk_panel_params ext_params_45hz = { //45Hz,dsi params
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
//DSC ENABLE
	.ssc_enable = 0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	//.vfp_low_power = VFP_45HZ,
	.wait_sof_before_dec_vfp = 1,
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
		.pic_height            =  DSC_PIC_HEIGHT,
		.pic_width             =  DSC_PIC_WIDTH,
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
		/*.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			//.range_bpg_ofs = range_bpg_ofs,
		},*/
	},
	.data_rate = DATA_RATE,
	.vdo_keep_hs_perline = 1,
	.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 0,
		.dfps_cmd_table[0] = {0, 4, {0x1F,0x78,0x00,0x10}},
		//.dfps_cmd_table[1] = {0, 2, {0xCF, 0x78}},
		//.dfps_cmd_table[2] = {0, 2, {0x00, 0xD2}},
		//.dfps_cmd_table[3] = {0, 2, {0xC0, 0x01}},
	},
	.tran_panel_params = &panel_driver_status,
/*
	.dyn = { //For mipi hopping
		.switch_en = 1,
		.pll_clk = 501,
		.hbp = 20,
	},
*/
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
	if (drm_mode_vrefresh(m) == MODE_144_FPS) //144Hz
		ext->params = &ext_params_144hz;
	else if (drm_mode_vrefresh(m) == MODE_120_FPS) //120Hz
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == MODE_90_FPS) //90Hz
		ext->params = &ext_params_90hz;
	else if (drm_mode_vrefresh(m) == MODE_60_FPS) //60Hz
		ext->params = &ext_params_60hz;
	else if (drm_mode_vrefresh(m) == MODE_45_FPS) //45Hz
		ext->params = &ext_params_45hz;
	else
		ret = 1;
	if (!ret)
		current_fps = drm_mode_vrefresh(m);
	return ret;
}
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_err("[error]%s: cannot get reset_gpio %ld\n", __func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
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
	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);
	if (data[0] == id[0])
		return 1;
	pr_info("ATA expect read data is %x %x %x\n", id[0], id[1], id[2]);
	return 0;
}

static int panel_doze_enable(struct drm_panel *panel,
		    void *dsi, dcs_write_gce cb, void *handle)
{
//	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("[LCM] %s begin\n", __func__);

	pr_info("[LCM] %s end\n", __func__);
	return 0;
}

//USD: add for LJ7H782-749 by junyu.chen2 20250422 start
static int panel_doze_disable(struct drm_panel *panel,
                    void *dsi, dcs_write_gce cb, void *handle)
{
        //void *tp_data = NULL;
//      struct lcm *ctx = panel_to_lcm(panel);
        pr_info("[LCM] %s begin\n", __func__);
        pr_info("%s lcm call tp to download fw", __func__);
        //tran_call_tp_cmd_handle(1005,tp_data);
        pr_info("[LCM] %s end\n", __func__);
        return 0;
}
//USD: add for LJ7H782-749 by junyu.chen2 20250422 end

#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
static int panel_set_aod_light_mode_v2(void *dsi_drv, mtk_dsi_ddic_cmd cb,
		void *handle, unsigned int level, struct mtk_dsi_cmd_option *cmd_opt)
{
	unsigned int aod_mapped_level = 0;
	struct mipi_dsi_msg bl_tb0_cmd = { 0 };

	if(level <= 4095)
		aod_mapped_level = level * 3276 / 4095; //normal backlight range 80% duty
	else
		aod_mapped_level = 3276 + ((level-4096) * (4095-3276) / 1023); //hbm backlight range
	pr_info("[LCM] %s level is %u,aod_mapped_level is %d\n", __func__, level, aod_mapped_level);

	bl_tb0[1] = ((aod_mapped_level >> 4) & 0xff);
	bl_tb0[2] = (aod_mapped_level & 0x0f);
	bl_tb0_cmd.tx_buf = bl_tb0;
	bl_tb0_cmd.tx_len = ARRAY_SIZE(bl_tb0);
	struct mtk_dsi_cmd_msg bl_tb0_msg = {
		.cmd_num = 1,
		.transfer_mode = PACKET_HS_MODE,
		.cmd_msg = &bl_tb0_cmd,
	};
	cb(dsi_drv, handle, cmd_opt, &bl_tb0_msg);

	return 0;
}
#endif

extern int tran_tp_need_lcd_esdrecovery;
static int lcm_panel_esd_check(void)
{
	if (tran_tp_need_lcd_esdrecovery) {
		pr_info("[LCM][FOCALTECH ESD]TP fail tran_tp_need_lcd_esdrecovery = %d \n", tran_tp_need_lcd_esdrecovery);
		tran_tp_need_lcd_esdrecovery = 0;
		return 1;
	}
	return 0;
}
static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq_v2 = lcm_setbacklight_cmdq_v2, //For OLED backlight set
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
	#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
	.set_aod_light_mode_v2 = panel_set_aod_light_mode_v2,
	#endif
	.panel_init_v2 = lcm_panel_init_v2,
	.panel_deinit_v2 = lcm_panel_deinit_v2,
	.panel_esd_check = lcm_panel_esd_check,
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
	struct drm_display_mode *mode_144Hz;
	struct drm_display_mode *mode_120Hz;
	struct drm_display_mode *mode_90Hz;
	struct drm_display_mode *mode_60Hz;
	struct drm_display_mode *mode_45Hz;

	mode_144Hz = drm_mode_duplicate(connector->dev, &switch_mode_144hz);
	if (!mode_144Hz) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_144hz.hdisplay, switch_mode_144hz.vdisplay, drm_mode_vrefresh(&switch_mode_144hz) );
		return -ENOMEM;
	}
	drm_mode_set_name(mode_144Hz);
	mode_144Hz->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_144Hz);

	mode_120Hz = drm_mode_duplicate(connector->dev, &switch_mode_120hz);
	if (!mode_120Hz) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_120hz.hdisplay, switch_mode_120hz.vdisplay, drm_mode_vrefresh(&switch_mode_120hz) );
		return -ENOMEM;
	}
	drm_mode_set_name(mode_120Hz);
	mode_120Hz->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_120Hz);

	mode_90Hz = drm_mode_duplicate(connector->dev, &switch_mode_90hz);
	if (!mode_90Hz) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_90hz.hdisplay, switch_mode_90hz.vdisplay, drm_mode_vrefresh(&switch_mode_90hz) );
		return -ENOMEM;
	}
	drm_mode_set_name(mode_90Hz);
	mode_90Hz->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_90Hz);

	mode_60Hz = drm_mode_duplicate(connector->dev, &switch_mode_60hz);
	if (!mode_60Hz) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_60hz.hdisplay, switch_mode_60hz.vdisplay, drm_mode_vrefresh(&switch_mode_60hz) );
		return -ENOMEM;
	}
	drm_mode_set_name(mode_60Hz);
	mode_60Hz->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_60Hz);

	mode_45Hz = drm_mode_duplicate(connector->dev, &switch_mode_45hz);
	if (!mode_45Hz) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_45hz.hdisplay, switch_mode_45hz.vdisplay, drm_mode_vrefresh(&switch_mode_45hz) );
		return -ENOMEM;
	}
	drm_mode_set_name(mode_45Hz);
	mode_45Hz->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_45Hz);

	connector->display_info.width_mm = 72;
	connector->display_info.height_mm = 155;
	return 5;
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
	struct device_node *backlight;
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
	dsi->mode_flags =  MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);
		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->vio18_regulator = devm_regulator_get(ctx->dev, "vmc");

	if (IS_ERR_OR_NULL(ctx->vio18_regulator)) {
		ret = PTR_ERR(ctx->vio18_regulator);
		dev_err(ctx->dev, "<%s,%d> lcm vio18 get fail!ret[%d].\n", __func__, __LINE__, ret);
	} else {
		ret = regulator_set_voltage(ctx->vio18_regulator, 1800000, 1800000); //set 1.8V
		ret = regulator_enable(ctx->vio18_regulator);
	}

	ctx->lcm_1v8_en = devm_gpiod_get(dev, "lcm_1v8", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcm_1v8_en)) {
		pr_err("cannot get lcm_1v8_en-gpios %ld\n", PTR_ERR(ctx->lcm_1v8_en));
	} else {
		devm_gpiod_put(dev, ctx->lcm_1v8_en);
	}

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

	//RESET ENABLE GPIO
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_err("[error]%s: cannot get reset-gpios %ld\n", __func__, PTR_ERR(ctx->reset_gpio));
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
	//mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_120hz, &ext_funcs, &ctx->panel);
	if (ret < 0) {
		pr_err("%s create new panel params fail\n", __func__);
		return ret;
	}
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
	//return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{
		.compatible = "ft8725,fhdp,dsi,vdo,dsc,tm,tm,144hz,lk6",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);
static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "ft8725_fhdp_dsi_vdo_dsc_tm_tm_144hz_lk6",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
	.shutdown = lcm_shutdown,
};

module_mipi_dsi_driver(lcm_driver);
MODULE_AUTHOR("diao.lu <diao.lu@transsion.com>");
MODULE_DESCRIPTION("ft8725+tm dsi vdo DSC 144hz Panel Driver");
MODULE_LICENSE("GPL v2");
