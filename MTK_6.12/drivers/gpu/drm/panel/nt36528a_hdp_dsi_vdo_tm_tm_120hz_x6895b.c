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
//BSP: add tp gesture by guoli.niu 20240620 start
#define TRAN_CTP_GESTURE_FUN
#if defined(TRAN_CTP_GESTURE_FUN)
/*This variable is for kernel-5.10 TP new driver frame*/
 int tran_new_gesture_fun_enable = 0;
#endif
extern u32 tran_call_tp_cmd_handle(u32 cmd, void *pdata);
//BSP: add tp gesture by guoli.niu 20240620 end

//BSP:add for lcd_dimmig by shiyu.cai 20251210 start
static char bl_dimming_enable[] = {0x53, 0x2C};
static char bl_dimming_disable[] = {0x53, 0x24};
static unsigned int g_dim_enable = 0;
static unsigned int g_need_dim_enable = 0;
static unsigned int mapped_level = 0;

#define REGFLAG_DELAY               0xFFFC
#define REGFLAG_UDELAY              0xFFFB
#define REGFLAG_END_OF_TABLE        0xFFFD
//BSP:add for lcd_dimmig by shiyu.cai 20251210 end
#define FRAME_WIDTH		720
#define FRAME_HEIGHT		1576
#define PHYSICAL_WIDTH		71561
#define PHYSICAL_HEIGHT		156639
#define HSA			12
#define HBP			10
#define HFP			24
#define VSA			4
#define VBP			24
#define DATA_RATE		1196
/*Parameter setting for mode 120 Start*/
#define MODE_120_FPS		120
#define MODE_120_VFP		309
/*Parameter setting for mode 120 End*/
/*Parameter setting for mode 90 Start*/
#define MODE_90_FPS		90
#define MODE_90_VFP		946
/*Parameter setting for mode 90 End*/
/*Parameter setting for mode 60 Start*/
#define MODE_60_FPS		60
#define MODE_60_VFP		2222
/*Parameter setting for mode 60 End*/
/*Parameter setting for mode 45 Start*/
#define MODE_45_FPS		45
#define MODE_45_VFP		3498
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
	unsigned char para_list[120];
};
static struct LCM_setting_table lcm_initialization_setting[] = {
	{0xFF,1,{0x10}},
	{0xFB,1,{0x01}},
	{0x35,1,{0x00}},
	{0xBA,1,{0x03}},
	{0xBB,1,{0x13}},
	{0xFF,1,{0x10}},
	{0xFB,1,{0x01}},
	{0x51,2,{0x00, 0x00}},
	{0x53,1,{0x24}},
	{0x55,1,{0x00}},
	
	//ddic pwm freq 16kHz 20251230 start
	{0xFF,1,{0x23}},
	{0xFB,1,{0x01}},
	{0x07,1,{0x00}},
	{0x08,1,{0x01}},
	{0x09,1,{0x0D}},
	//ddic pwm freq 16kHz 20251230 end

	//Optimization of dim speed 20260113 start
	{0xFF,1,{0x23}},
	{0xFB,1,{0x01}},
	{0x04,1,{0x04}},
	{0x05,1,{0x20}},
	{0x06,1,{0x00}},
	//Optimization of dim speed 20260113 end

	//esd 20260108 start 
	{0xFF,1,{0x24}},
	{0xFB,1,{0x01}},
	{0xC2,1,{0xC6}},
	{0xFF,1,{0x25}},
	{0xFB,1,{0x01}},
	{0x02,2,{0x10,0x40}},
	{0x19,1,{0x0E}},
	{0xFF,1,{0xD0}},
	{0xFB,1,{0x01}},
	{0xC9,1,{0x1C}},
	//esd 20260108 end
		
	//Optimization of low brightness effect 20251230 start
	{0xFF,1,{0x23}},
	{0xFB,1,{0x01}},
	{0x8D,1,{0x01}},
	{0xFF,1,{0x10}},
	{0xFB,1,{0x01}},
	{0x68,2,{0x03,0x01}},
	//Optimization of low brightness effect 20251230 end
	
	{0x11,0,{}},
	{REGFLAG_DELAY, 120, {}},
	{0x29,0,{}},
	{REGFLAG_DELAY, 40,{}},
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
	int ret = 0;
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
	pr_info("%s lcm call tp to download fw", __func__);
	ret = tran_call_tp_cmd_handle(1005,tp_data);
  	if (ret < 0) {
  		pr_info("%s call tran_call_tp_cmd_handle failed to download fw, ret=%d\n", __func__, ret);
  	}
	mdelay(10);
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
		ctx->avee_en_gpio = devm_gpiod_get(ctx->dev, "avee", GPIOD_OUT_HIGH);
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
	.dyn = {
		.switch_en = 1,
		.data_rate = 1228,
		.hbp = 28,
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
	.dyn = {
		.switch_en = 1,
		.data_rate = 1228,
		.hbp = 28,
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
	.dyn = {
		.switch_en = 1,
		.data_rate = 1228,
		.hbp = 28,
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
	.dyn = {
		.switch_en = 1,
		.data_rate = 1228,
		.hbp = 28,
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

	bl_tb0[1] = ((mapped_level >> 8) & 0x0f);
	bl_tb0[2] = (mapped_level & 0xff);

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
			aod_mapped_level = 70;		//doze_backlight_level1 10nit
		}else if(level >= 120 && level < 180){
			aod_mapped_level = 96;		//doze_backlight_level1 15nit
		}else{
			aod_mapped_level = 353;		//doze_backlight_level1 60nit
		}
		pr_info("[%s]lcm AOD backlight level=%d, aod_mapped_level=%d\n", __func__, level, aod_mapped_level);
		bl_aod_level[1] = ((aod_mapped_level & 0xF00) >> 8) & 0xF;
		bl_aod_level[2] = aod_mapped_level & 0xFF;
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

	//add 45hz mode
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
	{.compatible = "nt36528a,hdp,dsi,vdo,tm,tm,120hz,x6895b",},
	{}
};
MODULE_DEVICE_TABLE(of, lcm_of_match);
static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "nt36528a_hdp_dsi_vdo_tm_tm_120hz_x6895b",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};
module_mipi_dsi_driver(lcm_driver);
MODULE_AUTHOR("shiyu.cai <shiyu.cai@transsion.com>");
MODULE_DESCRIPTION("nt36528a VDO 120HZ LCD Panel Driver");
MODULE_LICENSE("GPL v2");
