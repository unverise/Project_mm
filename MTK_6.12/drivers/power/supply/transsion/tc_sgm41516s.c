// SPDX-License-Identifier: GPL-2.0
// SGM41516s driver
// Copyright (C) 2024 SGMICRO Incorporated - http://www.sg-micro.com

#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/phy/phy.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <generated/uapi/linux/version.h>
#include <linux/acpi.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include "tc_sgm41516s.h"

// #define CONFIG_MTK_CHARGER_V4P19
#define CONFIG_MTK_CHARGER_V5P10
#include "tc_charger_class.h"
#include "tc_charger.h"
#ifdef CONFIG_MTK_CHARGER_V4P19
#include "charger_type.h"
#include "mtk_charger_intf.h"
#endif /*CONFIG_MTK_CHARGER_V4P19*/



#define SGM_DRV_VERSION		"V1.2"

#define SGM_KERNEL_DEBUG
#ifdef SGM_KERNEL_DEBUG
#define SGM_INFO(fmt, args...) pr_info("[SGMICRO] %s() called: line=%d." fmt, __func__, __LINE__, ##args)
#define SGM_ERR(fmt, args...)  pr_err("[SGMICRO] %s() called: line=%d." fmt, __func__, __LINE__, ##args)
#else
#define SGM_INFO(fmt, args...)
#define SGM_ERR(fmt, args...)
#endif


#ifdef CONFIG_MTK_CHARGER_V5P10
#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2

enum sgm41516s_usbsw {
	USBSW_CHG,
	USBSW_USB,
};

#endif


struct sgm41516s_device {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *charger;
	struct mutex lock;
	struct mutex i2c_rw_lock;
	struct regmap *regmap;
	struct wakeup_source *charger_wakelock;
	char model_name[I2C_NAME_SIZE];
	struct sgm41516s_init_data init_data;
	struct sgm41516s_state state;
	struct regulator_dev *otg_rdev;
	struct notifier_block pm_nb;
	struct charger_properties chg_props;

	bool sgm41516s_suspend_flag;
	bool watchdog_enable;
	int device_id;
	bool chg_en;
	bool otg_en;
	bool charge_done;
	bool safetytimer_en;
	int usb_type;
	int chg_type;
	struct workqueue_struct *sgm_monitor_wq;
	//struct delayed_work sgm_delay_work;
	//struct delayed_work  monitor_dwork;
	//struct delayed_work charge_detect_delayed_work;
	//struct delayed_work hvdcp_work;
	//struct delayed_work force_detect_dwork;
	struct charger_device *chg_dev;

	int set_vbus;

	int force_detect_count;
	int qc2_en;
	int qc3_en;

	u32 intr_gpio;

};

static struct sgm_vendor_info s_chg_vendor[MAX_PN_ID] = {
	[SGM41542S_PN_ID]  = { "sgm41542s", (BIT(6) | BIT(4))},
	[SGM41516S_PN_ID]  = { "sgm41516s", (BIT(6) | BIT(5))},

};

/* SGM41516S REG06 BOOST_LIM[5:4], uV */
static const unsigned int SYS_MIN[] = {
	2600000, 2800000, 3000000, 3200000,3400000,3500000,3600000,3700000
};

static const unsigned int BOOST_CURRENT_LIMIT[] = {
	500000, 1000000, 1200000, 1500000, 2000000, 2500000, 3000000, 3200000,
};

static const unsigned int WATCHDOG_STABLE[] = {
	0, 40, 80, 160,
};

static const unsigned int BOOSTV_STABLE[] = {
	4850000, 5000000, 5150000, 5300000, 5800000, 6400000, 6900000, 7500000,
};

static const unsigned int IBATOCP_STABLE[] = {
	6000000, 8000000, 10000000, 12000000
};

static const unsigned int RECHRG_STABLE[] = {
	100, 200, 300, 600
};

enum SGM41516S_VINDPM_OS {
	VINDPM_OS_3900mV,
	VINDPM_OS_5900mV,
	VINDPM_OS_7500mV,
	VINDPM_OS_10500mV,
};

enum sgm41516s_pmu_chg_type {
	SGM41516S_CHG_TYPE_NOVBUS = 0,
	SGM41516S_CHG_TYPE_SDP,
	SGM41516S_CHG_TYPE_CDP,
	SGM41516S_CHG_TYPE_DCP,
	SGM41516S_CHG_TYPE_UNKNOW=5,
	SGM41516S_CHG_TYPE_SDPNSTD,
	SGM41516S_CHG_TYPE_OTG,
	SGM41516S_CHG_TYPE_MAX,
};
static int sgm41516xx_enable_dppu_enhance(struct sgm41516s_device *sgm);
static int sgm41516xx_enable_dmpu_enhance(struct sgm41516s_device *sgm);
static void sgm41516s_set_en_hiz(struct sgm41516s_device *sgm, bool val);
static int sgm41516xx_write_reg(struct sgm41516s_device *sgm , u8 reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sgm->client, reg, val);
	if (ret < 0) {
	pr_err("%s i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		__func__, val, reg, ret);
	return ret;
	}

	return 0;
}

static int sgm41516xx_read_reg(struct sgm41516s_device *sgm, u8 reg, int *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sgm->client, reg);
	if (ret < 0) {
	pr_err("%s i2c read fail: can't read from reg 0x%02X\n", __func__, reg);
	return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int sgm41516xx_update_bits(struct sgm41516s_device *sgm, u8 reg, u8 mask, u8 data)
{
	int ret;
	int tmp = 0;
	int original, modified, actual;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = sgm41516xx_read_reg(sgm, reg, &tmp);
	if (ret) {
	pr_err("%s Failed: read reg=0x%02X, ret=%d\n", __func__, reg, ret);
	goto out;
	}
	original = tmp;

	tmp &= ~mask;
	tmp |= (data & mask);
	modified = tmp;

	pr_info("%s: reg=0x%02X, mask=0x%02X, data(in)=0x%02X | val: 0x%02X -> 0x%02X\n", 
		__func__, reg, mask, data, original, modified);

	ret = sgm41516xx_write_reg(sgm, reg, modified);
	if (ret) {
	pr_err("%s Failed: write reg=0x%02X, ret=%d\n", __func__, reg, ret);
	}
	ret = sgm41516xx_read_reg(sgm, reg, &actual);
	if (ret == 0) {
	pr_info("%s: reg=0x%02X | orig:0x%02X -> want:0x%02X -> actual:0x%02X\n", 
		__func__, reg, original, modified, actual);
			
	if ((modified & mask) != (actual & mask)) {
		 pr_warn("%s: Warning! Write verification failed!\n", __func__);
	}
	}

out:
	mutex_unlock(&sgm->i2c_rw_lock);
	return ret;
}

#ifdef CONFIG_MTK_CHARGER_V5P10
/* Switch D+D- to AP*/
static int sgm41516s_set_usbsw(struct sgm41516s_device *sgm,
		enum sgm41516s_usbsw usbsw)
{
	struct phy *phy = NULL;
	int ret = 0;
	int mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET :
				PHY_MODE_BC11_CLR;

	pr_err("usbsw=%d\n", usbsw);
	phy = phy_get(sgm->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
	pr_err("failed to get usb2-phy\n");
	return -ENODEV;
	}
	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
	if (ret){
	pr_err("failed to set phy ext mode\n");
		phy_put(sgm->dev, phy);
		return -1;
	}
	phy_put(sgm->dev, phy);

	return ret;
}
#endif/*CONFIG_MTK_CHARGER_V5P10*/

__maybe_unused static int sgm41516s_jeita_en(struct sgm41516s_device *sgm, bool en)
{
	int ret;
	if(sgm->device_id){//sgm41516s
		if (en) {
			ret = sgm41516xx_update_bits(sgm,
					 SGM41516S_CHRG_CTRL_6,
					 SGM41516S_JEITA_SET,
					 0);
		} else {
			ret = sgm41516xx_update_bits(sgm,
					 SGM41516S_CHRG_CTRL_6,
					 SGM41516S_JEITA_SET,
					 1);
		}
	} else {//sgm41542s
		if (en) {
			ret = sgm41516xx_update_bits(sgm,
					 SGM41516S_CHRG_CTRL_6,
					 SGM41516S_JEITA_SET,
					 1);
		} else {
			ret = sgm41516xx_update_bits(sgm,
					 SGM41516S_CHRG_CTRL_6,
					 SGM41516S_JEITA_SET,
					 0);
		}

	}

	if (ret)
	dev_err(sgm->dev, "SGM41516S_JEITA_SET failed!\n");
	return ret;
}

__maybe_unused static int sgm41516s_bc12_auto_en(struct sgm41516s_device *sgm, bool en)
{
	int ret;
	if (en) {
	ret = sgm41516xx_update_bits(sgm,
		 SGM41516S_CHRG_CTRL_10,
		 SGM41516S_BC12,
		 SGM41516S_BC12);
	} else {
	ret = sgm41516xx_update_bits(sgm,
		 SGM41516S_CHRG_CTRL_10,
		 SGM41516S_BC12,
		 0);
	}

	if (ret)
	dev_err(sgm->dev, "SGM41516S_BC12 failed!\n");
	return ret;
}

static int sgm41516s_get_vindpm_offset_os(struct sgm41516s_device *sgm)
{
	int ret;
	u32 reg_val;

	ret = sgm41516xx_read_reg(sgm, SGM41516S_CHRG_CTRL_7, &reg_val);
	if (ret)
		return ret;

	reg_val = reg_val & SGM41516S_VINDPM_OS_MASK;

	return reg_val;
}

static int sgm41516s_set_vindpm_offset_os(struct sgm41516s_device *sgm,
					 enum SGM41516S_VINDPM_OS offset_os)
{
	int ret;

	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_CTRL_7,
				 SGM41516S_VINDPM_OS_MASK,
				 offset_os);

	if (ret)
		dev_err(sgm->dev, "set vindpm offset os error!\n");

	pr_err("%s set vindpm :ret%d\n", __func__, ret);

	return ret;
}

__maybe_unused static int sgm41516s_watchdog_timer_reset(struct sgm41516s_device *sgm)
{
	int ret;

	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_CTRL_1,
				 SGM41516S_WD_RST,
				 SGM41516S_WD_RST);

	if (ret)
		SGM_ERR("set watchdog timer reset error\n");

	return ret;
}

static int sgm41516s_set_watchdog_timer(struct sgm41516s_device *sgm, int time)
{
	u8 reg_val;
	int ret;

	for (reg_val = 1; reg_val < 4 && time >= WATCHDOG_STABLE[reg_val]; reg_val++) {
		pr_debug("%d\n", reg_val);
	}
		reg_val--;

	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_CTRL_2,
				 SGM41516S_WDT_TIMER_MASK,
				 reg_val);

	if (ret) {
		dev_err(sgm->dev, "set watchdog timsgm41516s_set_watchdog_timerer error i2c:%02x!\n", sgm->client->addr);
	}

	ret =  sgm41516xx_update_bits(sgm, SGM41516S_CHRG_CTRL_2, SGM41516S_WDT_TIMER_MASK, reg_val);
	if (ret) {
		dev_err(sgm->dev, "set watchdog timsgm41516s_set_watchdog_timerer error! i2c addr :%02X\n", sgm->client->addr);
	}
	if (time) {
		sgm->watchdog_enable = true;
	} else {
		sgm->watchdog_enable = false;
		sgm41516s_watchdog_timer_reset(sgm);
	}

	return ret;
}

static int sgm41516s_enable_charger(struct sgm41516s_device *sgm)
{
	int ret;

	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_CTRL_1,
				 SGM41516S_CHRG_EN,
				 SGM41516S_CHRG_EN);
	pr_err("%s ret=%d\n", __func__, ret);
	if (ret)
		dev_err(sgm->dev, "enable charger error!\n");

	return ret;
}

static int sgm41516s_disable_charger(struct sgm41516s_device *sgm)
{
	int ret;

	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_CTRL_1,
				 SGM41516S_CHRG_EN,
				 0);
	if (ret)
		dev_err(sgm->dev, "disable charger error!\n");

	return ret;
}


static int sgm41516s_set_vac_ovp(struct sgm41516s_device *sgm)
{
	int reg_val;
	int ret;

	reg_val = 0xFF & SGM41516S_VAC_OVP_MASK;

	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_CTRL_8,
				 SGM41516S_VAC_OVP_MASK,
				 reg_val);

	pr_err("%s ret=%d\n", __func__, ret);
	if (ret)
		dev_err(sgm->dev, "set vac ovp error!\n");

	return ret;
}

static __maybe_unused int sgm41516s_set_recharge_volt(struct sgm41516s_device *sgm, u32 recharge_volt)
{
	int reg_val;
	int ret;

	for (reg_val = 1; reg_val < 4 && recharge_volt >= RECHRG_STABLE[reg_val]; reg_val++) {
		pr_debug("%d\n", reg_val);
	}
		reg_val--;

	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_CTRL_9,
				 SGM41516S_VRECHARGE_MASK,
				 reg_val);

	if (ret)
		dev_err(sgm->dev, "set recharger error!\n");

	return ret;
}

static int sgm41516s_dump_register(struct sgm41516s_device *sgm)
{

	unsigned char i = 0;
	unsigned int ret = 0;
	unsigned int sgm41516s_reg[SGM41516S_DPDM_CTRL_2+1] = { 0 };

	for (i = 0; i <= SGM41516S_DPDM_CTRL_2; i++) {
		ret = sgm41516xx_read_reg(sgm,i, &sgm41516s_reg[i]);
		if (ret != 0) {
			pr_info("[sgm41516s] i2c transfor error\n");
			return 1;
		}
		pr_info("%s,[0x%x]=0x%x ",__func__, i, sgm41516s_reg[i]);
	}
	return 0;
}

static int sgm41516s_set_otg_voltage(struct sgm41516s_device *sgm, u32 uv)
{
	int ret = 0;
	int reg_val;

	for (reg_val = 1; reg_val < 8 && uv >= BOOSTV_STABLE[reg_val]; reg_val++) {
		pr_debug("%s\n",__func__ );
	}
	reg_val--;

	reg_val = reg_val << SGM41516S_BOOSTV_SHIFT;
	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_CTRL_3,
				 SGM41516S_BOOSTV,
				 reg_val);
	if (ret) {
		dev_err(sgm->dev, "set otg voltage error!\n");
		return ret;
	}

	return ret;
}

static int sgm41516s_set_otg_current(struct sgm41516s_device *sgm, u32 ua)
{
	int ret = 0;
	int reg_val = 0;
	SGM_INFO("set boost current limit :%d\n", ua);
	for (reg_val = 1; reg_val < 9 && ua >= BOOST_CURRENT_LIMIT[reg_val]; reg_val++) {
		pr_debug("%s\n",__func__ );
	}
	reg_val--;

	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_CTRL_8,
				 SGM41516S_BOOST_LIM,
				 reg_val);
	if (ret) {
		dev_err(sgm->dev, "set boost current limit error!\n");
		return ret;
	}
	return ret;
}

static int sgm41516s_set_otg_enable(struct sgm41516s_device *sgm,bool en)
{
	int ret = 0;

	if (en) {
		sgm41516s_set_en_hiz(sgm, false);
		ret = sgm41516xx_update_bits(sgm, SGM41516S_CHRG_CTRL_1,
						SGM41516S_OTG_EN, SGM41516S_OTG_EN);
		if (ret) {
			dev_err(sgm->dev, "Enable OTG failed\n");
			return ret;
		}
		sgm->otg_en = true;
		dev_info(sgm->dev, "OTG enabled\n");

	} else {
		ret = sgm41516xx_update_bits(sgm, SGM41516S_CHRG_CTRL_1,
						SGM41516S_OTG_EN, 0);
		if (ret) {
			dev_err(sgm->dev, "Disable OTG failed\n");
			return ret;
		}
		sgm->otg_en = false;
		sgm41516s_set_en_hiz(sgm, false);
		dev_info(sgm->dev, "OTG disabled\n");
	}
	return 0;
}

static int sgm41516s_set_input_volt_lim(struct sgm41516s_device *sgm,
						u32 vindpm)
{
	enum SGM41516S_VINDPM_OS os_val;
	unsigned int offset;
	u8 reg_val;
	int ret;

	if (vindpm < SGM41516S_VINDPM_V_MIN_uV ||
		vindpm > SGM41516S_VINDPM_V_MAX_uV)
		return -EINVAL;

	if (vindpm < 5900000) {
		os_val = VINDPM_OS_3900mV;
		offset = 3900000;
	} else if (vindpm >= 5900000 && vindpm < 7500000) {
		os_val = VINDPM_OS_5900mV;
		offset = 5900000;
	} else if (vindpm >= 7500000 && vindpm < 10500000) {
		os_val = VINDPM_OS_7500mV;
		offset = 7500000;
	} else {
		os_val = VINDPM_OS_10500mV;
		offset = 10500000;
	}

	ret = sgm41516s_set_vindpm_offset_os(sgm, os_val);
	if (ret) {
		pr_err("set vin dpm error!\n");
		return ret;
	}

	reg_val = (vindpm - offset) / SGM41516S_VINDPM_STEP_uV;

	ret = sgm41516xx_update_bits(sgm, SGM41516S_CHRG_CTRL_3,
				 SGM41516S_VINDPM_V_MASK, reg_val);
	if (ret) {
		pr_err("input voltage error!\n");
		return ret;
	}
	sgm->init_data.vlim = vindpm;

	pr_err("%s set :ret%d\n", __func__, ret);
	return 0;
}


static int sgm41516s_get_input_volt_lim(struct sgm41516s_device *sgm, u32 *uV)
{
	int ret;
	int offset;
	u32 vlim;
	int temp;

	ret = sgm41516xx_read_reg(sgm, SGM41516S_CHRG_CTRL_3, &vlim);
	if (ret)
		return ret;

	temp = sgm41516s_get_vindpm_offset_os(sgm);
	if (0 == temp)
		offset = 3900000;//uv
	else if (1 == temp)
		offset = 5900000;
	else if (2 == temp)
		offset = 7500000;
	else if (3 == temp)
		offset = 10500000;
	else
		return temp;

	*uV = offset + (vlim & 0x0F) * SGM41516S_VINDPM_STEP_uV;

	return 0;
}

static int sgm41516s_set_input_curr_lim(struct sgm41516s_device *sgm, u32 iindpm)
{
	int reg_val;
	int ret;

	if (iindpm < SGM41516S_IINDPM_I_MIN_uA)
		iindpm =  SGM41516S_IINDPM_I_MIN_uA;
	else if (iindpm > SGM41516S_IINDPM_I_MAX_uA)
		iindpm =  SGM41516S_IINDPM_I_MAX_uA;

	reg_val = (iindpm-SGM41516S_IINDPM_I_MIN_uA) / SGM41516S_IINDPM_STEP_uA;

	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_INPUT_CURR_LIMIT,
				 SGM41516S_IINDPM_I_MASK,
				 reg_val);
	if (ret){
		pr_err("set input current limit error!\n");
		return ret;
	}

	pr_err("%s set :ret%d\n", __func__, ret);
	return 0;
}

static int sgm41516s_get_input_curr_lim(struct sgm41516s_device *sgm, u32 *iindpm)
{
	int ret;
	int val;

	ret = sgm41516xx_read_reg(sgm, SGM41516S_INPUT_CURR_LIMIT, &val);
	if (ret) {
		dev_err(sgm->dev, "get input current limit error!\n");
		return ret;
	}
	*iindpm = (val & SGM41516S_IINDPM_I_MASK) * SGM41516S_IINDPM_STEP_uA + SGM41516S_IINDPM_I_MIN_uA;

	return ret;
}

static int sgm41516s_set_term_curr(struct sgm41516s_device *sgm, u32 uA)
{
	int reg_val;
	int ret;

	if (uA < SGM41516S_TERMCHRG_I_MIN_uA)
		uA = SGM41516S_TERMCHRG_I_MIN_uA;
	else if (uA > SGM41516S_TERMCHRG_I_MAX_uA)
		uA = SGM41516S_TERMCHRG_I_MAX_uA;

	reg_val = (uA - SGM41516S_TERMCHRG_I_MIN_uA) / SGM41516S_TERMCHRG_CURR_STEP_uA;

	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_PRE_TERM,
				 SGM41516S_TERMCHRG_CUR_MASK,
				 reg_val);
	if (ret){
		dev_err(sgm->dev, "set term current error!\n");
		return ret;
	}

	pr_err("%s ret:%d\n", __func__, ret);

	return 0;
}

static int sgm41516s_set_prechrg_curr(struct sgm41516s_device *sgm, u32 uA)
{
	int reg_val;
	int ret;

	if (uA < SGM41516S_PRECHRG_I_MIN_uA)
		uA = SGM41516S_PRECHRG_I_MIN_uA;
	else if (uA > SGM41516S_PRECHRG_I_MAX_uA)
		uA = SGM41516S_PRECHRG_I_MAX_uA;

	reg_val = (uA - SGM41516S_PRECHRG_I_MIN_uA) / SGM41516S_PRECHRG_CURR_STEP_uA;

	reg_val = reg_val << 4;

	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_PRE_TERM,
				 SGM41516S_PRECHRG_CUR_MASK,
				 reg_val);
	if (ret)
		dev_err(sgm->dev, "set precharge current error!\n");

	pr_err("%s set ipre success ret:%d \n", __func__, ret);

	return ret;
}

static int sgm41516s_get_ichrg_curr(struct sgm41516s_device *sgm, u32 *uA)
{
	int ret;
	int reg_val;
	ret = sgm41516xx_read_reg(sgm, SGM41516S_CHRG_CURR_LIMIT, &reg_val);
	if (ret) {
		dev_err(sgm->dev, "sgm41516s_get_ichrg_curr!\n");
		return ret;
	}
	*uA = (reg_val & SGM41516S_ICHRG_CUR_MASK) * SGM41516S_ICHRG_I_STEP_uA + SGM41516S_ICHRG_I_MIN_uA;

	if(*uA > SGM41516S_ICHRG_I_MAX_uA)
		*uA =SGM41516S_ICHRG_I_MAX_uA;

	return 0;
}

static int sgm41516s_get_chrg_volt(struct sgm41516s_device *sgm, u32 *chrg_volt)
{
	int val;
	int ret;

	ret = sgm41516xx_read_reg(sgm, SGM41516S_BAT_VOLT_LIMIT, &val);
	if (ret) {
		dev_err(sgm->dev, "sgm41516s_get_chrg_volt!\n");
		return ret;
	}
	*chrg_volt = ((val & SGM41516S_VREG_V_MASK) >> SGM41516S_VREG_V_SHIFT) * SGM41516S_VREG_V_STEP_uV
				+ SGM41516S_VREG_V_MIN_uV;

	return 0;
}

static int sgm41516s_set_chrg_volt(struct sgm41516s_device *sgm, u32 chrg_volt)
{
	int reg_val;
	int ret;

	if (chrg_volt < SGM41516S_VREG_V_MIN_uV)
		chrg_volt = SGM41516S_VREG_V_MIN_uV;
	else if ( chrg_volt > SGM41516S_VREG_V_MAX_uV)
		chrg_volt = SGM41516S_VREG_V_MAX_uV;

	reg_val = (chrg_volt - SGM41516S_VREG_V_MIN_uV) / SGM41516S_VREG_V_STEP_uV;

	reg_val = reg_val << SGM41516S_VREG_V_SHIFT;
	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_BAT_VOLT_LIMIT,
				 SGM41516S_VREG_V_MASK,
				 reg_val);
	if (ret)
	{
		dev_err(sgm->dev, "set charge voltage error!\n");
		return ret;
	}

	pr_err("%s set chg v %d\n", __func__ , chrg_volt);
	sgm->init_data.vreg = chrg_volt;
	return 0;
}

static int sgm41516s_set_ichrg_curr(struct sgm41516s_device *sgm, u32 uA)
{
	int reg_val;
	int ret;

	if (uA < SGM41516S_ICHRG_I_MIN_uA)
		uA = SGM41516S_ICHRG_I_MIN_uA;
	else if ( uA > SGM41516S_ICHRG_I_MAX_uA)
		uA = SGM41516S_ICHRG_I_MAX_uA;

	reg_val = uA / SGM41516S_ICHRG_I_STEP_uA;

	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_CURR_LIMIT,
				 SGM41516S_ICHRG_CUR_MASK,
				 reg_val);
	if (ret){
		pr_err("set icharge current error! ret=%d\n", ret);
		return ret;
	}
	sgm->init_data.ichg = uA;

	pr_err("%s set ipre success ichg:%d ret :%d\n", __func__, uA, ret);
	return 0;
}

__maybe_unused static int sgm41516s_force_dpdm(struct sgm41516s_device *sgm)
{
	int ret;

	ret = sgm41516xx_update_bits(sgm, SGM41516S_CHRG_CTRL_4, SGM41516S_IINDET_EN,SGM41516S_IINDET_EN);
	if (ret < 0){
		SGM_ERR("update bit fail, ret = %d\n", ret);
		return ret;
	}

	SGM_INFO("successful\n");

	return ret;
}

static int sgm41516s_adc_enable(struct sgm41516s_device *sgm, bool en)
{
	int ret;

	if (en) {
		ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_CTRL_9,
				 SGM41516S_CONV_START,
				 SGM41516S_CONV_START);

		ret += sgm41516xx_update_bits(sgm,
				SGM41516S_CHRG_CTRL_9,
				SGM41516S_CONV_RATE,
				SGM41516S_CONV_RATE);
	if (ret)
		dev_err(sgm->dev, "disable charger error!\n");
	} else {
			ret = sgm41516xx_update_bits(sgm,
				SGM41516S_CHRG_CTRL_9,
				SGM41516S_CONV_START,
				0);
			ret += sgm41516xx_update_bits(sgm,
				SGM41516S_CHRG_CTRL_9,
				SGM41516S_CONV_RATE,
				0);

	if (ret)
		dev_err(sgm->dev, "disable charger error!\n");
	}
	sgm->chg_en = en;
	return 0;
}

 static int sgm41516s_check_chg_enabled(struct sgm41516s_device *sgm,bool *enable)
{
	int ret,reg_val;

	ret = sgm41516xx_read_reg(sgm,SGM41516S_CHRG_CTRL_1,&reg_val);
	if (ret) {
		SGM_ERR("read charge enable fail\n");
		return ret;
	}
	*enable = (reg_val & SGM41516S_CHRG_EN) ? 1 : 0;
	SGM_INFO("chg enable:%d\n", *enable);

	return ret;
}


__maybe_unused static bool sgm41516s_dpdm_detect_is_done(struct sgm41516s_device *sgm)
{
	int chrg_flag = 0, ret;

	ret = sgm41516xx_read_reg(sgm, SGM41516S_CHRG_FLAG, &chrg_flag);
	if(ret) {
		dev_err(sgm->dev, "Check DPDM detecte error\n");
	}

	return (chrg_flag & SGM41516S_INPUT_DET_DONE) ? true:false;
}

__maybe_unused static int get_charger_type(struct sgm41516s_device * sgm)
{
		int curr_in_limit = 0;

		switch(sgm->state.chrg_type) {
		case SGM41516S_USB_SDP:
			pr_err("SGM4154x charger type: SDP\n");
			curr_in_limit = 500000;
			sgm->usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			sgm->chg_type = POWER_SUPPLY_TYPE_USB;
			#ifdef CONFIG_MTK_CHARGER_V5P10
			sgm41516s_set_usbsw(sgm, USBSW_USB);
			#endif /*CONFIG_MTK_CHARGER_V5P10*/
			break;

		case SGM41516S_USB_CDP:
			pr_err("SGM4154x charger type: CDP\n");
			curr_in_limit = 1000000;
			sgm->usb_type = POWER_SUPPLY_USB_TYPE_CDP;
			sgm->chg_type = POWER_SUPPLY_TYPE_USB;
			#ifdef CONFIG_MTK_CHARGER_V5P10
			sgm41516s_set_usbsw(sgm, USBSW_USB);
			#endif /*CONFIG_MTK_CHARGER_V5P10*/
			break;

		case SGM41516S_USB_DCP:
			pr_err("SGM4154x charger type: DCP\n");
			curr_in_limit = 2000000;
		sgm->usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		sgm->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
			sgm41516s_set_ichrg_curr(sgm, 1500000);
			break;

		case SGM41516S_UNKNOWN:
			pr_err("SGM4154x charger type: UNKNOWN\n");
			sgm->usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		sgm->chg_type = POWER_SUPPLY_TYPE_USB;
			curr_in_limit = 500000;
			if(sgm->force_detect_count < 2){ // retry detect
			#ifdef CONFIG_MTK_CHARGER_V5P10
			sgm41516s_set_usbsw(sgm, USBSW_USB);
			#endif /*CONFIG_MTK_CHARGER_V5P10*/
				SGM_ERR("unkonwn type, force detect again\n");
				//schedule_delayed_work(&sgm->force_detect_dwork, msecs_to_jiffies(100));// start force dpdm after 100ms
			}
			break;

		case SGM41516S_NON_STANDARD:
			pr_err("SGM4154x charger type: NON_STANDARD\n");
			sgm->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
			sgm->usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			curr_in_limit = 900000;
			break;
		case SGM41516S_OTG_MODE:
			pr_err("SGM4154x charger type: OTG\n");
			sgm->usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		sgm->chg_type = POWER_SUPPLY_TYPE_USB;
			curr_in_limit = 0;
			//sgm41516s_set_usbsw(sgm, USBSW_USB);
			break;

		default:
			pr_err("SGM4154x charger type: default\n");
			sgm->usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		sgm->chg_type = POWER_SUPPLY_TYPE_USB;
			curr_in_limit = 500000;
			break;
	}

	sgm41516s_set_input_curr_lim(sgm, curr_in_limit);
	return sgm->usb_type;
}

/********************** QC Interface Start ***********************/

static int32_t sgm41516s_set_dpdm(
	struct sgm41516s_device *sgm, uint8_t dp_val, uint8_t dm_val)
{
	uint8_t reg_val_dp = 0;
	uint8_t reg_val_dm = 0;

	int ret;

	reg_val_dp = (dp_val << SGM41516S_DP_VOLT_SHIFT) & SGM41516S_DP_VSEL_MASK;
	reg_val_dm = (dm_val << SGM41516S_DM_VOLT_SHIFT) & SGM41516S_DM_VSEL_MASK;

	ret = sgm41516xx_update_bits(sgm, SGM41516S_DPDM_CTRL_1,
					 SGM41516S_DP_VSEL_MASK,reg_val_dp);
	ret |= sgm41516xx_update_bits(sgm, SGM41516S_DPDM_CTRL_1,
					 SGM41516S_DM_VSEL_MASK,reg_val_dm);


	return ret;

}

__maybe_unused static int sgm41516s_charging_set_hvdcp20(struct sgm41516s_device *sgm,
							uint32_t vbus_target)
{
	int32_t ret = 0;

	switch (vbus_target) {
	case QC_20_5000mV:
		SGM_INFO("Set vbus target 5v\n");
		ret = sgm41516s_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_0P0);
		sgm41516s_set_input_volt_lim(sgm,4500000);
		break;
	case QC_20_9000mV:
		SGM_INFO("Set vbus target 9v\n");
		ret = sgm41516s_set_dpdm(sgm, SGM_DP_DM_VOL_3P3, SGM_DP_DM_VOL_0P6);
		sgm41516s_set_input_volt_lim(sgm, 8000000);
		break;
	case QC_20_12000mV:
		SGM_INFO("Set vbus target 12v\n");
		ret = sgm41516s_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_0P6);
		sgm41516s_set_input_volt_lim(sgm,11000000);
		break;
	default:
		ret = sgm41516s_set_dpdm(sgm, SGM_DP_DM_VOL_HIZ, SGM_DP_DM_VOL_HIZ);
		sgm41516s_set_input_volt_lim(sgm,4500000);
		break;
	}

	return (ret < 0) ? ret : 0;
}

int32_t sgm41516s_charging_enable_hvdcp30(struct sgm41516s_device *sgm, bool enable)
{
	SGM_INFO("enable %d\n", enable);
	//enter continuous mode DP 0.6, DM 3.3
	if (enable)
		return sgm41516s_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_3P3);
	else
		return sgm41516s_set_dpdm(sgm, SGM_DP_DM_VOL_0P0, SGM_DP_DM_VOL_0P0);
}

int32_t sgm41516s_charging_set_hvdcp30(struct sgm41516s_device *sgm, bool increase)
{
	int32_t ret = 0;

	SGM_INFO("increase %d\n", increase);
	if (increase) {
		//DP 3.3, DM 3.3
		ret = sgm41516s_set_dpdm(sgm, SGM_DP_DM_VOL_3P3, SGM_DP_DM_VOL_3P3);
		if (ret < 0)
			return ret;
		udelay(200);
		//DP 0.6, DM 3.3
		ret = sgm41516s_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_3P3);
		if (ret < 0)
			return ret;
		udelay(200);
	} else {
		//DP 0.6, DM 3.3
		ret = sgm41516s_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_0P6);
		if (ret < 0)
			return ret;
		udelay(200);
		//DP 0.6, DM 3.3
		ret = sgm41516s_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_3P3);
		if (ret < 0)
			return ret;
		udelay(200);
	}
	return 0;
}

/********************* end ************************/


/********************* PE START ************************/

static int sgm41516s_send_ta_current_pattern(struct sgm41516s_device *sgm, bool is_increase)
{
	int val;
	int ret;

	if (is_increase)
		val = SGM41516S_EN_PUMPX | SGM41516S_PUMPX_UP;
	else
		val = SGM41516S_EN_PUMPX | SGM41516S_PUMPX_DN;

	ret = sgm41516xx_write_reg(sgm,
				 SGM41516S_CHRG_CTRL_6,
				 val);
	if (ret) {
		pr_err("increase or reduce voltage fail\n");
		return ret;
	}
	return 0;
}

#if 0
static struct timespec64 ptime[13];
#endif
static int cptime[13][2];

static int dtime(int i)
{
#if 0
	struct timespec64 time;

	time = timespec64_sub(ptime[i], ptime[i - 1]);
	return time.tv_nsec / 1000000;
#else
	return 0;
#endif
}

#define PEOFFTIME 40
#define PEONTIME 90
static int sgm41516s_send_ta20_current_pattern(struct sgm41516s_device *sgm, u32 chr_vol)
{

	int value;
	int i, j = 0;
	int flag;

		sgm41516s_set_input_curr_lim(sgm, 4200000);
		sgm41516s_set_ichrg_curr(sgm, 600000);

	usleep_range(1000, 1200);

	value = (chr_vol - 5500000) / 500000;
	sgm41516s_set_input_curr_lim(sgm, 100000);// set ibus 100mA
	mdelay(70);

	//getboottime64(&ptime[j++]);
	j++;
	for (i = 4; i >= 0; i--) {
		flag = value & (1 << i);
		if (flag == 0) {
			sgm41516s_set_input_curr_lim(sgm, 700000);// ibus = 700mA

			mdelay(PEOFFTIME);

			//getboottime64(&ptime[j]);
			cptime[j][0] = PEOFFTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 30 || cptime[j][1] > 65) {
				pr_info("charging_set_ta20_current_pattern fail1: idx:%d target:%d actual:%d\n",
					i,
					PEOFFTIME,
					cptime[j][1]);
			}
			j++;
			sgm41516s_set_input_curr_lim(sgm, 0);
			mdelay(PEONTIME);
			//getboottime64(&ptime[j]);
			cptime[j][0] = PEONTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 90 || cptime[j][1] > 115) {
				pr_info("charging_set_ta20_current_pattern fail2: idx:%d target:%d actual:%d\n",
					i,
					PEOFFTIME,
					cptime[j][1]);
			}
			j++;
		} else {
			sgm41516s_set_input_curr_lim(sgm, 700000);// ibus = 700mA
			mdelay(PEONTIME);
			//getboottime64(&ptime[j]);
			cptime[j][0] = PEONTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 90 || cptime[j][1] > 115) {
				pr_info("charging_set_ta20_current_pattern fail3: idx:%d target:%d actual:%d\n",
					i,
					PEOFFTIME,
					cptime[j][1]);
			}
			j++;
			sgm41516s_set_input_curr_lim(sgm, 0);
			mdelay(PEOFFTIME);
			//getboottime64(&ptime[j]);
			cptime[j][0] = PEOFFTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 30 || cptime[j][1] > 65) {
				pr_info("charging_set_ta20_current_pattern fail4: idx:%d target:%d actual:%d\n",
					i,
					PEOFFTIME,
					cptime[j][1]);
			} 
			j++;
		}
	}

	sgm41516s_set_input_curr_lim(sgm, 700000);// ibus = 700mA
	mdelay(160);
	//getboottime64(&ptime[j]);
	cptime[j][0] = 160;
	cptime[j][1] = dtime(j);
	if (cptime[j][1] < 150 || cptime[j][1] > 240) {
		pr_info("charging_set_ta20_current_pattern fail5: idx:%d target:%d actual:%d\n",
			i,
			PEOFFTIME,
			cptime[j][1]);
	}
	j++;
	sgm41516s_set_input_curr_lim(sgm, 0);
	mdelay(30);
	sgm41516s_set_input_curr_lim(sgm, 700000);// ibus = 700mA

	pr_info("%d %d: %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d!!\n",
		chr_vol, value, cptime[1][0], cptime[2][0], cptime[3][0],
		cptime[4][0], cptime[5][0], cptime[6][0], cptime[7][0],
		cptime[8][0], cptime[9][0], cptime[10][0], cptime[11][0]);
	pr_info("%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d!!\n",
		cptime[1][1], cptime[2][1], cptime[3][1], cptime[4][1],
		cptime[5][1], cptime[6][1], cptime[7][1], cptime[8][1],
		cptime[9][1], cptime[10][1], cptime[11][1]);
	mdelay(100);
	return 0;
}

__maybe_unused static int sgm41516s_set_ta20_reset(struct sgm41516s_device *sgm) {

	int curr;

	sgm41516s_get_input_curr_lim(sgm, &curr);

	sgm41516s_set_input_curr_lim(sgm, 100000);
	msleep(300);
	return sgm41516s_set_input_curr_lim(sgm, curr);
}

/********************* PE END ************************/

static irqreturn_t sgm41516s_irq_handler_thread(int irq, void *private)
{
	//struct sgm41516s_device *sgm = private;

#ifdef CONFIG_MTK_CHARGER_V5P10
	//sgm41516s_set_usbsw(sgm, USBSW_CHG);
#endif
	pr_err("%s sgm irq trig \n", __func__);

	//schedule_delayed_work(&sgm->charge_detect_delayed_work, 400);

	/*if(!sgm41516s_dpdm_detect_is_done(sgm)) {
		dev_err(sgm->dev, "DPDM detecte not done, disable charge\n");
		goto err;
	}*/
	return IRQ_HANDLED;
}
static bool sgm41516s_is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SGM41516S_INPUT_CURR_LIMIT ... SGM41516S_DPDM_CTRL_2:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config sgm41516s_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = SGM41516S_DPDM_CTRL_2,
	//.cache_type = REGCACHE_RBTREE,
	.volatile_reg = sgm41516s_is_volatile_reg,
};
static int sgm41516s_hw_init(struct sgm41516s_device *sgm)
{
	struct power_supply_battery_info bat_info = { };
	int chrg_stat, ret = 0;
	sgm->otg_en = false;
	sgm41516s_set_en_hiz(sgm, false);
	if (!bat_info.constant_charge_current_max_ua)
		bat_info.constant_charge_current_max_ua = 500000;
	if (!bat_info.constant_charge_voltage_max_uv)
		bat_info.constant_charge_voltage_max_uv = SGM41516S_VREG_V_DEF_uV;
	if (!bat_info.precharge_current_ua)
		bat_info.precharge_current_ua = SGM41516S_PRECHRG_I_DEF_uA;
	if (!bat_info.charge_term_current_ua)
		bat_info.charge_term_current_ua = SGM41516S_TERMCHRG_I_DEF_uA;

	ret = sgm41516s_set_watchdog_timer(sgm, 0);
	if (ret)
		goto err_out;

	ret = sgm41516s_set_prechrg_curr(sgm, bat_info.precharge_current_ua);
	if (ret)
		goto err_out;

	ret = sgm41516s_set_chrg_volt(sgm,bat_info.constant_charge_voltage_max_uv);
	if (ret)
		goto err_out;

	ret = sgm41516s_set_term_curr(sgm,bat_info.charge_term_current_ua);
	if (ret)
		goto err_out;

	ret = sgm41516s_set_input_volt_lim(sgm, sgm->init_data.vlim);
	if (ret)
		goto err_out;

	ret = sgm41516xx_read_reg(sgm, SGM41516S_CHRG_STAT_1, &chrg_stat);
	if (ret) {
		pr_err("read SGM41516S_CHRG_STAT fail\n");
		goto err_out;
	}

	if (!(chrg_stat & SGM41516S_PG_STAT)) {
		ret = sgm41516s_set_input_curr_lim(sgm, DEFAULT_INPUT_CURRENT);
		if (ret)
			goto err_out;
		ret = sgm41516s_set_ichrg_curr(sgm,bat_info.constant_charge_current_max_ua);
		if (ret)
			goto err_out;
	}
	if(sgm->device_id)
		sgm41516s_bc12_auto_en(sgm,true);

	sgm41516s_adc_enable(sgm, true);
	sgm41516s_enable_charger(sgm);
	ret = sgm41516s_set_vac_ovp(sgm);
	if (ret)
		goto err_out;

	sgm41516xx_update_bits(sgm,
				SGM41516S_CHRG_STAT_2,
				SGM41516S_IINDPM_INT_MASK | SGM41516S_VINDPM_INT_MASK,
				SGM41516S_VINDPM_INT_MASK | SGM41516S_IINDPM_INT_MASK);

	pr_err("ichrg_curr:%d\n"
		"prechrg_curr:%d\n"
		"chrg_vol:%d\n"
		"term_curr:%d\n"
		"input_curr_lim:%d\n",
		bat_info.constant_charge_current_max_ua,
		bat_info.precharge_current_ua,
		bat_info.constant_charge_voltage_max_uv,
		bat_info.charge_term_current_ua,
		sgm->init_data.ilim);

	pr_err("hw_init succuess\n");
	return 0;

err_out:
	pr_err("read SGM41516S_CHRG_reg fail%d\n", ret);
	return ret;
}

static int sgm41516s_parse_dt(struct sgm41516s_device *sgm)
{
	int ret;

	ret = of_property_read_string(sgm->dev->of_node, "chg_alias_name", &(sgm->chg_props.alias_name));
	if (ret < 0) {
			sgm->chg_props.alias_name = "sgm41516s";
			pr_info("%s: no alias name\n", __func__);
		}
	ret = device_property_read_u32(sgm->dev,
						"input-voltage-limit-microvolt",
						&sgm->init_data.vlim);
	if (ret)
		sgm->init_data.vlim = SGM41516S_VINDPM_DEF_uV;

	if (sgm->init_data.vlim > SGM41516S_VINDPM_V_MAX_uV ||
		sgm->init_data.vlim < SGM41516S_VINDPM_V_MIN_uV)
		return -EINVAL;

	ret = device_property_read_u32(sgm->dev,
						"input-current-limit-microamp",
						&sgm->init_data.ilim);
	if (ret)
		sgm->init_data.ilim = SGM41516S_IINDPM_DEF_uA;

	if (sgm->init_data.ilim > SGM41516S_IINDPM_I_MAX_uA ||
		sgm->init_data.ilim < SGM41516S_IINDPM_I_MIN_uA)
		return -EINVAL;

	sgm->intr_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,intr_gpio", 0);
	if (sgm->intr_gpio < 0) {
	pr_err("no up,intr_gpio, ret:%d\n", sgm->intr_gpio);
	}
	pr_err("intr_gpio = %d\n", sgm->intr_gpio);

	return 0;
}

/******************************** MTK OPS **************************************/
//enable DPPU enhance mode
static int sgm41516xx_enable_dppu_enhance(struct sgm41516s_device *sgm)
{
	int ret;
	ret = sgm41516xx_update_bits(sgm, SGM41516S_DPDM_CTRL_2,
				 SGM41516S_DPPU_ENHANCE, 
				 SGM41516S_DPPU_ENHANCE);
	if (ret)
	pr_err("%s: Failed to enable DPPU enhance, ret=%d\n", __func__, ret);
	return ret;
}

//enable DMPU enhance mode
static int sgm41516xx_enable_dmpu_enhance(struct sgm41516s_device *sgm)
{
	int ret;

	ret = sgm41516xx_update_bits(sgm, SGM41516S_DPDM_CTRL_2, 
				 SGM41516S_DMPU_ENHANCE, 
				 SGM41516S_DMPU_ENHANCE);
	if (ret)
	pr_err("%s: Failed to enable DMPU enhance, ret=%d\n", __func__, ret);

	return ret;
}
static int sgm41516s_ensure_manual_mode(struct sgm41516s_device *sgm)
{
	int ret;
	int val = 0;

	ret = sgm41516xx_read_reg(sgm, SGM41516S_DPDM_CTRL_2, &val);
	if (ret < 0)
		return ret;

	if (!(val & SGM41516S_DPDM_EN)) {
		ret = sgm41516xx_update_bits(sgm, SGM41516S_DPDM_CTRL_2, SGM41516S_DPDM_EN, SGM41516S_DPDM_EN);
		if (ret < 0)
			return ret;
	}
	ret = sgm41516xx_enable_dppu_enhance(sgm);
	if (ret < 0) {
		SGM_INFO("%s: enable dppu enhance failed, ret=%d\n", __func__, ret);
	}
	ret = sgm41516xx_enable_dmpu_enhance(sgm);
	if (ret < 0) {
		SGM_INFO("%s: enable dmpu enhance failed, ret=%d\n", __func__, ret);
	}

		return 0;
}
static int sgm41516s_set_dp_reg(struct sgm41516s_device *sgm, u32 val)
{
	int ret;

	SGM_INFO("%s: val=%d\n", __func__, val);

	ret = sgm41516s_ensure_manual_mode(sgm);
	if (ret < 0) {
		SGM_INFO("%s: Failed to ensure manual mode, ret=%d\n", __func__, ret);
		return ret;
	}

	ret = sgm41516xx_update_bits(sgm, SGM41516S_DPDM_CTRL_1,
				 	SGM41516S_DP_VSEL_MASK,
				 	(val << SGM41516S_DP_VOLT_SHIFT) & SGM41516S_DP_VSEL_MASK);
	if (ret < 0) {
		SGM_INFO("%s: update bits failed, ret=%d\n", __func__, ret);
		return ret;
	}

		return 0;
}

static int sgm41516s_set_dm_reg(struct sgm41516s_device *sgm, u32 val)
{
	int ret;

	SGM_INFO("%s: val=%d\n", __func__, val);

	ret = sgm41516s_ensure_manual_mode(sgm);
	if (ret < 0) {
		SGM_INFO("%s: Failed to ensure manual mode, ret=%d\n", __func__, ret);
		return ret;
		}

	ret = sgm41516xx_update_bits(sgm, SGM41516S_DPDM_CTRL_1,
				 	SGM41516S_DM_VSEL_MASK,
					(val << SGM41516S_DM_VOLT_SHIFT) & SGM41516S_DM_VSEL_MASK);
	if (ret < 0) {
		SGM_INFO("%s: update bits failed, ret=%d\n", __func__, ret);
		return ret;
	}


	return 0;
}
static int sgm41516s_set_dp_function(struct charger_device *chg_dev, enum dpdm_ctrl_status status)
{
	struct sgm41516s_device *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;

	SGM_INFO("%s: status = %d\n", __func__, status);

	switch (status) {
	case DPDM_CTRL_HZ:
		ret = sgm41516s_set_dp_reg(sgm, SGM_DP_DM_VOL_HIZ);
		break;
	case DPDM_CTRL_0V:
		ret = sgm41516s_set_dp_reg(sgm, SGM_DP_DM_VOL_0P0);
		break;
	case DPDM_CTRL_0_6V:
		ret = sgm41516s_set_dp_reg(sgm, SGM_DP_DM_VOL_0P6);
		break;
	case DPDM_CTRL_3_3V:
		ret = sgm41516s_set_dp_reg(sgm, SGM_DP_DM_VOL_3P3);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int sgm41516s_set_dm_function(struct charger_device *chg_dev, enum dpdm_ctrl_status status)
{
	struct sgm41516s_device *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;

	SGM_INFO("%s: status = %d\n", __func__, status);

	switch (status) {
	case DPDM_CTRL_HZ:
		ret = sgm41516s_set_dm_reg(sgm, SGM_DP_DM_VOL_HIZ);
		break;
	case DPDM_CTRL_0V:
		ret = sgm41516s_set_dm_reg(sgm, SGM_DP_DM_VOL_0P0);
		break;
	case DPDM_CTRL_0_6V:
		ret = sgm41516s_set_dm_reg(sgm, SGM_DP_DM_VOL_0P6);
		break;
	case DPDM_CTRL_3_3V:
		ret = sgm41516s_set_dm_reg(sgm, SGM_DP_DM_VOL_3P3);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
static int sgm41516s_tc30_oneshot(struct charger_device *chg_dev, int val)
{
	SGM_INFO("%s: val = %d\n", __func__, val);

	switch (val) {
	case TC30_SWITCH_5V:
		/* Reset: Hi-Z */
		sgm41516s_set_dp_function(chg_dev, DPDM_CTRL_HZ);
		sgm41516s_set_dm_function(chg_dev, DPDM_CTRL_HZ);
		SGM_INFO("reset dpdm successfully!\n");
		break;
	case TC30_SWITCH_7V5:
		/* Special Mode: DP=3.3V, DM=0V */
		sgm41516s_set_dp_function(chg_dev, DPDM_CTRL_3_3V);
		sgm41516s_set_dm_function(chg_dev, DPDM_CTRL_0V);
		SGM_INFO("%s successfully!\n", __func__);
		break;
	default:
		SGM_INFO("%s:unknown ctrl:%d\n", __func__, val);
		break;
	}
	return 0;
}
static int sgm41516s_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	struct sgm41516s_device *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;

	SGM_INFO("%s: en = %d\n", __func__, en);

	if (en) {
		/* Force detection: Set IINDET_EN bit */
		ret = sgm41516xx_update_bits(sgm, SGM41516S_CHRG_CTRL_4,
						 SGM41516S_IINDET_EN, SGM41516S_IINDET_EN);
	} else {
		ret = sgm41516xx_update_bits(sgm, SGM41516S_CHRG_CTRL_4,
						 SGM41516S_IINDET_EN, 0);
	}

	return ret;
}

static int sgm41516s_get_port_stat(struct charger_device *chg_dev, int *port_stat)
{
	struct sgm41516s_device *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret, val;
	int type;

	ret = sgm41516xx_read_reg(sgm, SGM41516S_CHRG_STAT_1, &val);
	if (ret < 0) {
		*port_stat = 0;
		return ret;
	}
	type = (val & SGM41516S_VBUS_STAT_MASK) >> SGM41516S_VBUS_STAT_SHIFT;

	SGM_INFO("%s: reg08=0x%x, type=%d\n", __func__, val, type);

	switch (type) {
	case SGM41516S_CHG_TYPE_SDP: /* USB SDP */
		*port_stat = TC_PORT_STAT_SDP;/* TC_PORT_STAT_SDP */
		break;
	case SGM41516S_CHG_TYPE_CDP: /* USB CDP */
		*port_stat = TC_PORT_STAT_CDP;/* TC_PORT_STAT_CDP */
		break;
	case SGM41516S_CHG_TYPE_UNKNOW:
		*port_stat = TC_PORT_STAT_NOINFO;
		break;
	case SGM41516S_CHG_TYPE_SDPNSTD: /* USB DCP */
	case SGM41516S_CHG_TYPE_DCP:
		*port_stat = TC_PORT_STAT_DCP;/* TC_PORT_STAT_DCP */
		break;
	default:
		*port_stat = TC_PORT_STAT_UNKNOWN_TA;/* Unknown / No Info */
		break;
	}

	return 0;
}
static int sgm41516s_get_vbus_gd_stat(struct charger_device *chg_dev, bool *vbus_gd)
{
	struct sgm41516s_device *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret, val;
	SGM_INFO("%s\n", __func__);
	ret = sgm41516xx_read_reg(sgm, SGM41516S_CHRG_STAT_2, &val);
	if (ret < 0) {
		*vbus_gd = false;
		return ret;
	}

	*vbus_gd = !!(val & SGM41516S_VBUS_GD);

	SGM_INFO("%s: vbus_gd = %d (reg0a=0x%x)\n", __func__, *vbus_gd, val);

	return 0;
}
static int sgm41516s_get_aicr_state(struct charger_device *chg_dev, bool *state)
{
	struct sgm41516s_device *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret, val;
	SGM_INFO("%s\n", __func__);
	ret = sgm41516xx_read_reg(sgm, SGM41516S_CHRG_STAT_2, &val);
	if (ret < 0) {
		*state = false;
		return ret;
	}

	*state = !!(val & SGM41516S_IINDPM_STAT);

	SGM_INFO("%s: aicr state = %d\n", __func__, *state);
	return 0;
}
static int sgm41516s_get_shipmode_status(struct charger_device *chg_dev)
{
	struct sgm41516s_device *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret, val;

	ret = sgm41516xx_read_reg(sgm, SGM41516S_CHRG_CTRL_4, &val);
	if (ret < 0) {
		SGM_ERR("%s: get failed!\n", __func__);
		return 0;
	}
	val = !!(val & SGM41516S_BATFET_DIS);

	SGM_INFO("%s : %d\n", __func__, val);
	return val;
}
static int sgm41516s_enable_shipmode(struct charger_device *chg_dev, bool on)
{
	struct sgm41516s_device *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret;

	SGM_INFO("%s enter! on=%d\n", __func__, on);

	ret = sgm41516xx_update_bits(sgm, SGM41516S_CHRG_CTRL_4,
					 SGM41516S_BATFET_DLY, 
					 on ? SGM41516S_BATFET_DLY : 0);
	mdelay(10);

	ret = sgm41516xx_update_bits(sgm, SGM41516S_CHRG_CTRL_4,
					 SGM41516S_BATFET_DIS, 
					 on ? SGM41516S_BATFET_DIS : 0);

	return 0;
}
__maybe_unused static int mtk_sgm41516s_dump_register(struct charger_device *chg_dev)
{

	unsigned int ret = 0;

	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	ret = sgm41516s_dump_register(sgm);

	return ret;
}
static void sgm41516s_set_en_hiz(struct sgm41516s_device *sgm, bool val)
{
	if (sgm->otg_en && val)
		return;

	sgm41516xx_update_bits(sgm, SGM41516S_CHRG_CTRL_10,
					SGM41516S_EN_HIZ,
					val ? SGM41516S_EN_HIZ : 0);
}
static int sgm41516s_enable_hiz_mode(struct charger_device *chg_dev, bool enable)
{
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);

	SGM_INFO("%s: enter! enable=%d\n", __func__, enable);

	sgm41516s_set_en_hiz(sgm, enable);

	return 0;
}
__maybe_unused static int mtk_sgm41516s_set_otg_voltage(struct charger_device *chg_dev, u32 uv)
{
	int ret = 0;

	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	ret = sgm41516s_set_otg_voltage(sgm, uv);
	return ret;
}


__maybe_unused static int mtk_sgm41516s_set_otg_current(struct charger_device *chg_dev, u32 ua)
{
	int ret = 0;

	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	ret = sgm41516s_set_otg_current(sgm,ua);

	return ret;
}

__maybe_unused static int mtk_sgm41516s_set_otg_enable(struct charger_device *chg_dev,bool en)
{
	int ret = 0;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	ret = sgm41516s_set_otg_enable(sgm,en);

	return ret;
}


__maybe_unused static int mtk_sgm41516s_set_input_volt_lim(struct charger_device *chg_dev,
						u32 vindpm)
{
	int ret;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);

	ret = sgm41516s_set_input_volt_lim(sgm,vindpm);
	return ret;
}


__maybe_unused static int mtk_sgm41516s_get_input_volt_lim(struct charger_device *chg_dev, u32 *uV)
{
	int ret;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);

	ret = sgm41516s_get_input_volt_lim(sgm,uV);
	return ret;
}



__maybe_unused static int mtk_sgm41516s_set_input_curr_lim(struct charger_device *chg_dev, u32 iindpm)
{
	int ret;

	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	ret = sgm41516s_set_input_curr_lim(sgm,iindpm);

	return ret;
}
static int sgm41516s_get_mivr_state(struct charger_device *chg_dev, bool *state)
{
	struct sgm41516s_device *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret, val;

	ret = sgm41516xx_read_reg(sgm, SGM41516S_CHRG_STAT_2, &val);
	if (ret < 0) {
		*state = false;
		return ret;
	}

	*state = !!(val & SGM41516S_VINDPM_STAT);

	SGM_INFO("%s: mivr state = %d\n", __func__, *state);
	return 0;
}
__maybe_unused static int mtk_sgm41516s_get_input_mincurr_lim(struct charger_device *chg_dev, u32 *iindpm)
{
	*iindpm = SGM41516S_IINDPM_I_MIN_uA;
	return 0;
}


__maybe_unused static int mtk_sgm41516s_get_input_curr_lim(struct charger_device *chg_dev, u32 *iindpm)
{
	int ret;

	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	ret = sgm41516s_get_input_curr_lim(sgm,iindpm);
	return ret;
}

#define SGM41516S_VINDPM_MAX  12000000
__maybe_unused static int mtk_sgm41516s_enable_power_path(struct charger_device *chg_dev, bool en)
{
	int ret =0;
	u32 mivr ;

	struct sgm41516s_device *sgm = charger_get_data(chg_dev);

	if (en)
	{
		mivr = 	sgm->init_data.vlim;
	} else {
		mivr = 	SGM41516S_VINDPM_MAX;
	}

	ret = sgm41516s_set_input_volt_lim(sgm,mivr);
	if (ret) {
		dev_err(sgm->dev, "%s: power path set failed\n", __func__);
		return ret;
	}

	return 0;
}

__maybe_unused static int mtk_sgm41516s_is_powerpath_enabled(struct charger_device *chg_dev, bool *en)
{
	int ret;
	int val = 0;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);

	ret = sgm41516s_get_input_volt_lim(sgm,&val);

	if (!ret) {

		*en = (val == SGM41516S_VINDPM_MAX) ? false : true;
	}
	return ret;
}


__maybe_unused static int mtk_sgm41516s_watchdog_timer_reset(struct charger_device *chg_dev)
{
	int ret;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	ret = sgm41516s_watchdog_timer_reset(sgm);
	return ret;
}

__maybe_unused static int mtk_sgm41516s_set_term_curr(struct charger_device *chg_dev, u32 uA)
{
	int ret;

	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	ret = sgm41516s_set_term_curr(sgm,uA);
	return 0;
}


__maybe_unused static int mtk_sgm41516s_set_prechrg_curr(struct charger_device *chg_dev, u32 uA)
{
	int ret;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	ret = sgm41516s_set_prechrg_curr(sgm,uA);
	return ret;
}



__maybe_unused static int mtk_sgm41516s_get_ichrg_curr(struct charger_device *chg_dev, u32 *uA)
{
	int ret;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);

	ret = sgm41516s_get_ichrg_curr(sgm, uA);

	return ret;
}


static int mtk_sgm41516s_get_minichg_curr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = SGM41516S_ICHRG_I_MIN_uA;
	return 0;
}


__maybe_unused static int mtk_sgm41516s_set_ichrg_curr(struct charger_device *chg_dev, u32 uA)
{
	int ret;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	ret = sgm41516s_set_ichrg_curr(sgm,uA);

	return 0;
}


__maybe_unused static int mtk_sgm41516s_get_chrg_volt(struct charger_device *chg_dev, u32 *chrg_volt)
{
	int ret;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);

	ret = sgm41516s_get_chrg_volt(sgm, chrg_volt);

	return ret;
}


__maybe_unused static int mtk_sgm41516s_set_chrg_volt(struct charger_device *chg_dev, u32 chrg_volt)
{
	int ret;

	struct sgm41516s_device *sgm = charger_get_data(chg_dev);

	ret = sgm41516s_set_chrg_volt(sgm,chrg_volt);

	return ret;
}

static int sgm41516s_plug_in(struct charger_device *chg_dev)
{
	int ret = 0;

	struct sgm41516s_device *sgm = dev_get_drvdata(&chg_dev->dev);

	SGM_INFO("%s\n", __func__);

	ret = sgm41516s_set_watchdog_timer(sgm, 0);
	if (ret < 0)
		dev_err(sgm->dev, "enable watchdog failed\n");

	ret = sgm41516s_enable_charger(sgm);
	if (ret) {
		SGM_INFO("%s: enable charger failed\n", __func__);
		return ret;
	}

	return 0;
}

static int sgm41516s_plug_out(struct charger_device *chg_dev)
{
	int ret = 0;
	struct sgm41516s_device *sgm = dev_get_drvdata(&chg_dev->dev);

	SGM_INFO("%s\n", __func__);

	/* 1. Disable Charger */
	ret = sgm41516s_disable_charger(sgm);
	if (ret) {
		SGM_INFO("%s: disable charger failed\n", __func__);
		return ret;
	}
	/* 2. Disable HIZ mode */
	ret = sgm41516s_enable_hiz_mode(chg_dev, false);
	if (ret < 0)
		SGM_INFO("%s: disable hiz failed\n", __func__);

	/* 3. Disable WDT */
	ret = sgm41516s_set_watchdog_timer(sgm, 0);
	if (ret < 0)
		dev_err(sgm->dev, "disable watchdog failed\n");

#if IS_ENABLED(CONFIG_MTK_HVDCP20_SUPPORT) || IS_ENABLED(CONFIG_MTK_TC30_SUPPORT)
	sgm41516s_set_dp_dm_recover(chg_dev);
#endif

	if (sgm)
		sgm->chg_en = false;

	return 0;
}

static int mtk_sgm41516s_charging_enable(struct charger_device *chg_dev, bool en)
{
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);

	if (en)
		sgm41516s_enable_charger(sgm);
	else
		sgm41516s_disable_charger(sgm);
	sgm->chg_en = en;
	return 0;
}

static int mtk_sgm41516s_is_charging(struct charger_device *chg_dev, bool *en)
{
	int ret;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	ret = sgm41516s_check_chg_enabled(sgm,en);
	return 0;
}

static int mtk_sgm41516s_get_charging_status(struct charger_device *chg_dev, bool *done)
{
	int ret;
	int val;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	if(IS_ERR_OR_NULL(sgm))
		return -ENOMEM;
	ret = sgm41516xx_read_reg(sgm, SGM41516S_CHRG_STAT_1, &val);
	if(ret < 0)
		SGM_ERR("get SGM41516S_CHRG_STAT_1 failed.\n");
	val = val& SGM41516S_CHG_STAT_MASK;
	*done = (val == SGM41516S_TERM_CHRG) ? 1 : 0;
	return 0;
}

static int mtk_sgm41516s_enable_safetytimer(struct charger_device *chg_dev, bool en)
{
	int ret;
	int val;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	if(IS_ERR_OR_NULL(sgm))
		return -ENOMEM;
	if (en)
		val = SGM41516S_EN_TIMER;
	else
		val = 0;
	ret = sgm41516xx_update_bits(sgm,
				 SGM41516S_CHRG_CTRL_2,
				 SGM41516S_EN_TIMER,
				 val);
	if (ret)
	{
		dev_err(sgm->dev, "enable safetytimer error!\n");
		return ret;
	}

	sgm->safetytimer_en = en;
	return 0;
}

static int mtk_sgm41516s_get_is_safetytimer_enable(struct charger_device *chg_dev, bool *en)
{
	int ret;
	int val;
	struct sgm41516s_device *sgm = charger_get_data(chg_dev);
	if(IS_ERR_OR_NULL(sgm))
		return -ENOMEM;

	ret = sgm41516xx_read_reg(sgm, SGM41516S_CHRG_CTRL_2, &val);

	*en = (val & SGM41516S_EN_TIMER) ? 1 : 0;
	return 0;
}

__maybe_unused static int mtk_sgm41516s_send_ta_current_pattern(struct charger_device *chg_dev, bool is_increase)
{
	int ret;

	struct sgm41516s_device *sgm = charger_get_data(chg_dev);

	ret = sgm41516s_send_ta_current_pattern(sgm,is_increase);

	return 0;
}

static int mtk_sgm41516s_send_ta20_current_pattern(struct charger_device *chg_dev, u32 chr_vol)
{

	int ret;

	struct sgm41516s_device *sgm = charger_get_data(chg_dev);

	ret = sgm41516s_send_ta20_current_pattern(sgm,chr_vol);

	return 0;
}

static int mtk_sgm41516s_set_ta20_reset(struct charger_device *chg_dev)
{

	int ret;

	struct sgm41516s_device *sgm = charger_get_data(chg_dev);

	ret = sgm41516s_set_ta20_reset(sgm);

	return 0;
}

static const struct charger_properties sgm41516s_chg_props = {
		.alias_name = "sgm41516S",};

static struct charger_ops sgm41516s_chg_ops = {
	.dump_registers = mtk_sgm41516s_dump_register,
	/* cable plug in/out */
	.plug_in = sgm41516s_plug_in,
	.plug_out = sgm41516s_plug_out,
	/* enable */
	.enable = mtk_sgm41516s_charging_enable,
	.is_enabled = mtk_sgm41516s_is_charging,
	/* charging current */
	.set_charging_current = mtk_sgm41516s_set_ichrg_curr,
	.get_charging_current = mtk_sgm41516s_get_ichrg_curr,
	.get_min_charging_current = mtk_sgm41516s_get_minichg_curr,
	/* charging voltage */
	.set_constant_voltage = mtk_sgm41516s_set_chrg_volt,
	.get_constant_voltage = mtk_sgm41516s_get_chrg_volt,
	/* input current limit */
	.set_input_current = mtk_sgm41516s_set_input_curr_lim,
	.get_input_current = mtk_sgm41516s_get_input_curr_lim,
	.get_min_input_current = mtk_sgm41516s_get_input_mincurr_lim,
	/* MIVR */
	.set_mivr = mtk_sgm41516s_set_input_volt_lim,
	.get_mivr = mtk_sgm41516s_get_input_volt_lim,
	.get_mivr_state = sgm41516s_get_mivr_state,
	.get_aicr_state = sgm41516s_get_aicr_state,
	.set_eoc_current = mtk_sgm41516s_set_term_curr,

	.enable_shipmode = sgm41516s_enable_shipmode,
	.get_shipmode_status = sgm41516s_get_shipmode_status,

	.is_charging_done = mtk_sgm41516s_get_charging_status,
	/* power path */
	.enable_powerpath = mtk_sgm41516s_enable_power_path,
	.is_powerpath_enabled = mtk_sgm41516s_is_powerpath_enabled,
	/* timer */
	.enable_safety_timer = mtk_sgm41516s_enable_safetytimer,
	.is_safety_timer_enabled = mtk_sgm41516s_get_is_safetytimer_enable,
	.kick_wdt = mtk_sgm41516s_watchdog_timer_reset,
	/* AICL */
	/* PE+/PE+20 */
	.send_ta_current_pattern = mtk_sgm41516s_send_ta_current_pattern,
	.send_ta20_current_pattern = mtk_sgm41516s_send_ta20_current_pattern,
	.reset_ta = mtk_sgm41516s_set_ta20_reset,

	.set_dp = sgm41516s_set_dp_function,
	.set_dm = sgm41516s_set_dm_function,

	.set_tc30_oneshot = sgm41516s_tc30_oneshot,

	.enable_hz = sgm41516s_enable_hiz_mode,
	/* OTG */
	.enable_otg = mtk_sgm41516s_set_otg_enable,
	.set_boost_current_limit = mtk_sgm41516s_set_otg_current,
	.set_boost_voltage = mtk_sgm41516s_set_otg_voltage,

	.enable_chg_type_det = sgm41516s_enable_chg_type_det,
	.get_port_stat = sgm41516s_get_port_stat,
	.get_vbus_gd = sgm41516s_get_vbus_gd_stat,
};

static int sgm41516s_hw_chipid_detect(struct sgm41516s_device *sgm)
{
	int ret = 0;
	int val = 0;
	int index = 0;

	ret = sgm41516xx_read_reg(sgm, SGM41516S_PART_INFO, &val);
	if (ret)
	{
		dev_err(sgm->dev,"[%s] read fail\n", __func__);
		return false;
	}

	pr_info("[%s] Reg[0x0B]=0x%x\n", __func__,val);
	val = val & SGM41516S_PN_MASK;
	for (index = 0; index < MAX_PN_ID; index++) {
		if ((s_chg_vendor[index].part_id == val) && (strstr(s_chg_vendor[index].name,sgm->client->name)))
		{
			sgm->device_id = index;
			pr_info("[%s] device name %s\n", __func__,s_chg_vendor[index].name);
			return true;
		}
	}

	pr_info("[%s] don't found device ,Reg[0x0B]=0x%x\n", __func__,val);
	return true;
}

/********************creat devices note start*************************************************/
__maybe_unused static ssize_t otgen_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sgm41516s_device *sgm = dev_get_drvdata(dev);
	unsigned int en;
	int ret;

	ret = sscanf(buf, "%d", &en);
	if (ret == 1 && en == 1) {
		sgm41516s_disable_charger(sgm);
		/* OTG setting 5V/1.2A */
		ret = sgm41516s_set_otg_voltage(sgm, 5000000);
		if (ret) {
			SGM_ERR("set OTG voltage error!\n");
			return ret;
		}

		ret = sgm41516s_set_otg_current(sgm, 1200000);
		if (ret) {
			SGM_ERR("set OTG current error!\n");
			return ret;
		}

		ret = sgm41516s_set_otg_enable(sgm,1);
		if (ret) {
			SGM_ERR("set OTG enable error!\n");
			return ret;
		}
	}
	else if (ret == 1 && en == 0){
		ret = sgm41516s_set_otg_enable(sgm,0);
		if (ret) {
			SGM_ERR("set OTG disable error!\n");
			return ret;
		}
	}
	return count;
}
static ssize_t sgm41516s_show_registers(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sgm41516s_device *sgm = i2c_get_clientdata(to_i2c_client(dev));
	int i, ret, val;
	int len = 0;

	for (i = 0; i < SGM41516_REG_NUM; i++) {
		ret = sgm41516xx_read_reg(sgm, i, &val);
		if (!ret)
			len += sysfs_emit_at(buf, len, "Reg[0x%02x] = 0x%02x\n", i, val);
	}
	return len;
}

static ssize_t sgm41516s_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sgm41516s_device *sgm = i2c_get_clientdata(to_i2c_client(dev));
	unsigned int reg, val;
	int ret;

	/* Expect input format: "0x00 0x55" or "0 85" */
	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret != 2) {
		dev_err(dev, "Invalid format. Usage: echo 'reg val' > debug_node\n");
		return -EINVAL;
	}

	if (reg >= SGM41516_REG_NUM) {
		dev_err(dev, "Register address out of range\n");
		return -EINVAL;
	}

	/* Use update_bits with mask 0xFF to write the full byte */
	ret = sgm41516xx_update_bits(sgm, (u8)reg, 0xFF, (u8)val);
	if (ret < 0)
		return ret;

	dev_info(dev, "Write Reg[0x%02x] = 0x%02x success\n", reg, val);
	return count;
}
static DEVICE_ATTR(sgm_registers_debug, 0664, sgm41516s_show_registers, sgm41516s_store_register);

/********************creat devices note end*************************************************/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 1))

int sgm41516s_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sgm41516s_device *sgm;
	int ret;

	dev_err(dev, "start to  init sgm41516s_probe\n");
	sgm = devm_kzalloc(dev, sizeof(*sgm), GFP_KERNEL);
	if (!sgm)
		return -ENOMEM;
	dev_err(dev, "i2c addr%x:\n", client->addr);

	//client->addr = 0x6b;
	sgm->client = client;
	sgm->dev = dev;

	mutex_init(&sgm->lock);
	sgm->charger_wakelock = wakeup_source_register(sgm->dev, "sgm_chg_wakelock");

	i2c_set_clientdata(client, sgm);

	mutex_init(&sgm->i2c_rw_lock);
	device_create_file(&client->dev, &dev_attr_sgm_registers_debug);
	ret = sgm41516s_parse_dt(sgm);
	if (ret) {
		dev_err(dev, "Failed to read device tree properties%d\n", ret);
		return ret;
	}

	ret = sgm41516s_hw_chipid_detect(sgm);
	if (!ret){
		dev_err(dev, "failed to detect chipid\n");
		return ret;
	}
	device_init_wakeup(dev, 1);
	sgm->chg_dev = charger_device_register("primary_chg",
						&client->dev, sgm,
						&sgm41516s_chg_ops,
						&sgm41516s_chg_props);
	if (IS_ERR_OR_NULL(sgm->chg_dev)) {
		pr_info("%s: register charger device  failed\n", __func__);
		ret = PTR_ERR(sgm->chg_dev);
		goto err_register_chg_dev;
		return ret;
	}
	SGM_INFO("sgm41516s register chg devices success!\n");

	client->irq = gpio_to_irq(sgm->intr_gpio);

	if (client->irq) {
		ret = devm_request_threaded_irq(dev, client->irq, NULL,
						sgm41516s_irq_handler_thread,
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						"sgm41516s-irq", sgm);
		if (ret) {
			dev_err(dev, "failed to requset irq\n");
			goto error_out;
		}
		enable_irq_wake(client->irq);
	}

	ret = sgm41516s_hw_init(sgm);
	if (ret) {
		dev_err(dev, "Cannot initialize the chip.\n");
		goto error_out;
	}

	/* OTG setting 5V/1.2A */
	ret = sgm41516s_set_otg_voltage(sgm, 5000000);
	if (ret) {
		dev_err(sgm->dev, "set OTG voltage error!\n");
		return ret;
	}

	ret = sgm41516s_set_otg_current(sgm, 1200000);
	if (ret) {
		dev_err(sgm->dev, "set OTG current error!\n");
		return ret;
	}

	sgm->sgm_monitor_wq = alloc_ordered_workqueue("%s",
			WQ_MEM_RECLAIM | WQ_FREEZABLE, "sgm-monitor-wq");

	pr_err("%s success!!!\n",__func__);
	return ret;
err_register_chg_dev:
	devm_kfree(&client->dev, sgm);
error_out:
	pr_err("failed to  init sgm41516s_probe\n");
	return ret;
}


static void sgm41516s_charger_remove(struct i2c_client *client)
{
	struct sgm41516s_device *sgm = i2c_get_clientdata(client);

	charger_device_unregister(sgm->chg_dev);
	power_supply_unregister(sgm->charger);
	device_remove_file(&client->dev, &dev_attr_sgm_registers_debug);
	mutex_destroy(&sgm->lock);

}

#else
int sgm41516s_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sgm41516s_device *sgm;
	int ret;

	dev_err(dev, "start to  init sgm41516s_probe\n");
	sgm = devm_kzalloc(dev, sizeof(*sgm), GFP_KERNEL);
	if (!sgm)
		return -ENOMEM;
	dev_err(dev, "i2c addr%x:\n", client->addr);

	sgm->client = client;
	sgm->dev = dev;

	mutex_init(&sgm->lock);
	sgm->charger_wakelock = wakeup_source_register(sgm->dev, "sgm_chg_wakelock");

	i2c_set_clientdata(client, sgm);

	mutex_init(&sgm->i2c_rw_lock);
	device_create_file(&client->dev, &dev_attr_sgm_registers_debug);
	ret = sgm41516s_parse_dt(sgm);
	if (ret) {
		dev_err(dev, "Failed to read device tree properties%d\n", ret);
		return ret;
	}

	ret = sgm41516s_hw_chipid_detect(sgm);
	if (!ret){
		dev_err(dev, "failed to detect chipid\n");
		return ret;
	}
	device_init_wakeup(dev, 1);
	sgm->chg_dev = charger_device_register("primary_chg",
						&client->dev, sgm,
						&sgm41516s_chg_ops,
						&sgm41516s_chg_props);
	if (IS_ERR_OR_NULL(sgm->chg_dev)) {
		pr_info("%s: register charger device  failed\n", __func__);
		ret = PTR_ERR(sgm->chg_dev);
		goto err_register_chg_dev;
		return ret;
	}
	SGM_INFO("sgm41516s register chg devices success!\n");
	/* otg regulator */
	client->irq = gpio_to_irq(sgm->intr_gpio);

	if (client->irq) {
		ret = devm_request_threaded_irq(dev, client->irq, NULL,
						sgm41516s_irq_handler_thread,
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						"sgm41516s-irq", sgm);
		if (ret) {
			dev_err(dev, "failed to requset irq\n");
			goto error_out;
		}
		enable_irq_wake(client->irq);
	}

	ret = sgm41516s_hw_init(sgm);
	if (ret) {
		dev_err(dev, "Cannot initialize the chip.\n");
		goto error_out;
	}

	/* OTG setting 5V/1.2A */
	ret = sgm41516s_set_otg_voltage(sgm, 5000000);
	if (ret) {
		dev_err(sgm->dev, "set OTG voltage error!\n");
		return ret;
	}

	ret = sgm41516s_set_otg_current(sgm, 1200000);
	if (ret) {
		dev_err(sgm->dev, "set OTG current error!\n");
		return ret;
	}

	sgm->sgm_monitor_wq = alloc_ordered_workqueue("%s",
			WQ_MEM_RECLAIM | WQ_FREEZABLE, "sgm-monitor-wq");

	SGM_INFO("%s success!!!\n",__func__);
	return ret;
err_register_chg_dev:
	devm_kfree(&client->dev, sgm);
error_out:
	pr_err("failed to  init sgm41516s_probe\n");
	return ret;
}

static int sgm41516s_charger_remove(struct i2c_client *client)
{
	struct sgm41516s_device *sgm = i2c_get_clientdata(client);
	charger_device_unregister(sgm->chg_dev);
	power_supply_unregister(sgm->charger);
	device_remove_file(&client->dev, &dev_attr_sgm_registers_debug);
	mutex_destroy(&sgm->lock);

	return 0;
}
#endif
static void sgm41516s_charger_shutdown(struct i2c_client *client)
{
	int ret = 0;

	struct sgm41516s_device *sgm = i2c_get_clientdata(client);
	ret = sgm41516s_disable_charger(sgm);
	if (ret) {
	pr_err("Failed to disable charger, ret = %d\n", ret);
	}
	pr_info("sgm41516s_charger_shutdown\n");
}

static const struct i2c_device_id sgm41516s_i2c_ids[] = {
	{ "sgm41516s", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sgm41516s_i2c_ids);

static const struct of_device_id sgm41516s_of_match[] = {
	{ .compatible = "sgm41516s", },
	{ },
};
MODULE_DEVICE_TABLE(of, sgm41516s_of_match);

static struct i2c_driver sgm41516s_driver = {
	.driver = {
		.name = "sgm41516s-charger",
		.of_match_table = sgm41516s_of_match,
	},
	.probe = sgm41516s_probe,
	.remove = sgm41516s_charger_remove,
	.shutdown = sgm41516s_charger_shutdown,
	.id_table = sgm41516s_i2c_ids,
};
module_i2c_driver(sgm41516s_driver);

MODULE_AUTHOR("<hongqiang_qin@sg-micro.com>");
MODULE_DESCRIPTION("sgm41516s charger driver");
MODULE_LICENSE("GPL v2");

