#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/i2c.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/reboot.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>
#include <linux/phy/phy.h>

#include "charger_class.h"
#include "mtk_charger.h"
#include <linux/linear_range.h>
#include <linux/regmap.h>

#include "sc8950.h"

#define GPIO_CHG_ENABLE     0
#define GPIO_CHG_DISABLE    1

/*MD_CHG:add for charger pump by yongqing 20220413 start*/
enum dpdm_ctrl_status {
	DPDM_CTRL_HZ,
	DPDM_CTRL_0V,
	DPDM_CTRL_0_6V,
	DPDM_CTRL_3_3V,
	DPDM_CTRL_DP_HZ_DM_HZ,
	DPDM_CTRL_DP_0_6_DM_HZ,
	DPDM_CTRL_DP_3_3_DM_0_6,
};

static const char * const dpdm_ctrl_name[] = {
	[DPDM_CTRL_HZ]	 	  = "DPDM_CTRL_HZ",
	[DPDM_CTRL_0V]	 	  = "DPDM_CTRL_0V",
	[DPDM_CTRL_0_6V] 	  = "DPDM_CTRL_0_6V",
	[DPDM_CTRL_3_3V] 	  = "DPDM_CTRL_3_3V",
	[DPDM_CTRL_DP_HZ_DM_HZ]   = "DPDM_CTRL_DP_HZ_DM_HZ",
	[DPDM_CTRL_DP_0_6_DM_HZ]  = "DPDM_CTRL_DP_0_6_DM_HZ",
	[DPDM_CTRL_DP_3_3_DM_0_6] = "DPDM_CTRL_DP_3_3_DM_0_6",
};
/*MD_CHG:add for charger pump by yongqing 20220413 end*/

#if IS_ENABLED(CONFIG_MTK_TC30_SUPPORT)
static int  sc8950_tc30_gpio_enable(struct charger_device *chg_dev, bool en);
#endif
//static void sc8950_start_bc12_timer(struct sc8950_info *info);
#ifndef PAGE_SIZE
# define PAGE_SIZE 4096
#endif
#define SC8950_MANUFACTURER	"Southchip"

//add for can not recognize docking station by xiewei 20241021 start
#if IS_ENABLED(CONFIG_CHARGER_TYPE_MT6357)
extern void mt6357_hw_bc11_init(void);
extern void mt6357_hw_bc11_done(void);
#endif
//add for can not recognize docking station by xiewei 20241021 end

enum sc8950_usbsw {
        SC8950_USBSW_CHG = 0,
        SC8950_USBSW_USB,
};

#define SC8950_PHY_MODE_BC11_SET 1
#define SC8950_PHY_MODE_BC11_CLR 2
#define SC8950_BAT_COMP_DEFAULT 0
#define SC8950_VCLAMP_DEFAULT 0

enum sc8950_pmu_chg_type {
        SC8950_CHG_TYPE_NOVBUS = 0,
        SC8950_CHG_TYPE_SDP,
        SC8950_CHG_TYPE_CDP,
        SC8950_CHG_TYPE_DCP,
        SC8950_CHG_TYPE_UNKNOW=5,
        SC8950_CHG_TYPE_SDPNSTD,
        SC8950_CHG_TYPE_OTG,
        SC8950_CHG_TYPE_MAX,
};

enum sc8950_ic_stat {
	SC8950_ICSTAT_DISABLE = 0,
	SC8950_ICSTAT_PRECHG,
	SC8950_ICSTAT_FASTCHG,
	SC8950_ICSTAT_CHGDONE,
	SC8950_ICSTAT_MAX,
};

enum attach_type {
        ATTACH_TYPE_NONE,
        ATTACH_TYPE_PWR_RDY,
        ATTACH_TYPE_TYPEC,
        ATTACH_TYPE_PD,
        ATTACH_TYPE_PD_SDP,
        ATTACH_TYPE_PD_DCP,
        ATTACH_TYPE_PD_NONSTD,
};

const unsigned int sc8950_otg_oc_threshold[] = {
	500000, 750000,1200000,1400000,1650000,1875000,2150000,2450000
};

static inline u8 sc8950_val_toreg(u32 min, u32 max, u32 step, u32 target,
				  bool ru)
{
	if (target <= min)
		return 0;

	if (target >= max)
		return (max - min) / step;

	if (ru)
		return (target - min + step) / step;
	return (target - min) / step;
}

static u8 sc8950_vboost_toreg(u32 uV)
{
	return sc8950_val_toreg(SC8950_VBOOST_MIN, SC8950_VBOOST_MAX, SC8950_VBOOST_STEP, uV, true);
}

/***************** I2C Operation *****************/
unsigned int __sc8950_read_byte(struct sc8950_info *info, unsigned char cmd, 
	unsigned char *returnData)
{
	unsigned char xfers = 2;
	int ret, retries = 3;
	unsigned char s_buf[1];

	s_buf[0] = cmd;
	
	do {
		struct i2c_msg msgs[2] = {
			{
				.addr = info->client->addr,
				.flags = 0,
				.len = 1,
				.buf = s_buf,
			},
			{

				.addr = info->client->addr,
				.flags = I2C_M_RD,
				.len = 1,
				.buf = returnData,
			}
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(info->client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			dev_err(info->dev, "skipping non-existent adapter %s\n",
				info->client->adapter->name);
			break;
		}

		if (ret != xfers)
			mdelay(10);
	} while (ret != xfers && --retries);

	return ret == xfers ? 0 : -1;
}

unsigned int sc8950_read_byte(struct sc8950_info *info, unsigned char cmd, 
	unsigned char *returnData)
{
	unsigned int ret = 0;
	
	mutex_lock(&info->sc8950_i2c_access);
	ret = __sc8950_read_byte(info, cmd, returnData);
	mutex_unlock(&info->sc8950_i2c_access);

	return ret;
}

unsigned int __sc8950_write_byte(struct sc8950_info *info, unsigned char cmd,
	unsigned char writeData)
{
	unsigned char xfers = 1;
	int ret, retries = 3;
	unsigned char buf[2];

	buf[0] = cmd;
	memcpy(&buf[1], &writeData, 1);

	do {
		struct i2c_msg msgs[1] = {
			{
				.addr = info->client->addr,
				.flags = 0,
				.len = 1 + 1,
				.buf = buf,
			},
		};
		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(info->client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			dev_err(info->dev, "skipping non-existent adapter %s\n",
				info->client->adapter->name);
			break;
		}

		if (ret != xfers)
			mdelay(10);
	} while (ret != xfers && --retries);

	return ret == xfers ? 1 : -1;
}

unsigned int sc8950_write_byte(struct sc8950_info *info, unsigned char cmd,
	unsigned char writeData)
{
	int ret = 0;

	mutex_lock(&info->sc8950_i2c_access);
	ret = __sc8950_write_byte(info, cmd, writeData);
	mutex_unlock(&info->sc8950_i2c_access);

	return ret;
}

unsigned int sc8950_update_bits(struct sc8950_info *info, unsigned char RegNum,
	unsigned char val, unsigned char mask,unsigned char shift)
{
	unsigned char sc8950_reg = 0, sc8950_reg_old = 0, sc8950_reg_new = 0;
	unsigned int ret = 0;
	
	mutex_lock(&info->sc8950_i2c_access);
	
	ret = __sc8950_read_byte(info, RegNum, &sc8950_reg);
	if (ret < 0) {
		dev_err(info->dev, "%s read Reg[0x%x] failed\n", __func__, RegNum);
		goto err;
	}

	sc8950_reg_old = sc8950_reg;
	
	sc8950_reg &= ~(mask << shift);
	sc8950_reg |= (val << shift);

	ret = __sc8950_write_byte(info, RegNum, sc8950_reg);
	if (ret < 0) {
		dev_err(info->dev, "%s write Reg[0x%x] failed\n", __func__, RegNum);
		goto err;
	}

	ret = __sc8950_read_byte(info, RegNum, &sc8950_reg_new);
	if (ret < 0) {
		dev_err(info->dev, "%s read Reg[0x%x] again failed\n", __func__, RegNum);
		goto err;
	}

err:
	mutex_unlock(&info->sc8950_i2c_access);
	dev_dbg(info->dev, "[%s] write Reg[%x]=0x%x from 0x%x new:0x%x\n", __func__,
		RegNum, sc8950_reg, sc8950_reg_old, sc8950_reg_new);

	return ret;
}

unsigned int sc8950_read_interface(struct sc8950_info *info, unsigned char RegNum,
				    unsigned char *val, unsigned char MASK,unsigned char SHIFT)
{
	unsigned char sc8950_reg = 0;
	unsigned int ret = 0;

	mutex_lock(&info->sc8950_i2c_access);

	ret = __sc8950_read_byte(info, RegNum, &sc8950_reg);
	if (ret < 0) {
		dev_err(info->dev, "[%s] read Reg[%x] failed ret = %d\n", __func__, RegNum, ret);
		goto err;
	}

	dev_dbg(info->dev, "[%s] Reg[%x]=0x%x ", __func__, RegNum, sc8950_reg);

	sc8950_reg &= (MASK << SHIFT);
	*val = (sc8950_reg >> SHIFT);

err:
	dev_dbg(info->dev, " val=0x%x\n", *val);
	mutex_unlock(&info->sc8950_i2c_access);

	return ret;
}
/***************** I2C Operation *****************/

/***************** SC8950 IC Reg Operation *****************/
static int sc8950_set_hiz(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s hiz_en:0x%x\n", __func__, value);
	if (info->boost_mode && value)
		return 0;
	return sc8950_update_bits(info, SC8950_REG_INPUT_CONTROL,
		value, SC8950_REG_INPUT_CONTROL_HIZ_MASK, SC8950_REG_INPUT_CONTROL_HIZ_SHIFT);
}

static int sc8950_get_hiz(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_INPUT_CONTROL,
		value, SC8950_REG_INPUT_CONTROL_HIZ_MASK, SC8950_REG_INPUT_CONTROL_HIZ_SHIFT);
}

static int sc8950_set_ibus(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s ibus:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_INPUT_CONTROL,
		value, SC8950_REG_INPUT_CONTROL_INDPM_MASK, SC8950_REG_INPUT_CONTROL_INDPM_SHIFT);
}

static int sc8950_get_ibus(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_INPUT_CONTROL,
		value, SC8950_REG_INPUT_CONTROL_INDPM_MASK, SC8950_REG_INPUT_CONTROL_INDPM_SHIFT);
}

static int __maybe_unused sc8950_set_dp(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s dp:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_DPDM_CONTROL1,
		value, SC8950_REG_DPDM_CONTROL1_DP_DAC_MASK, SC8950_REG_DPDM_CONTROL1_DP_DAC_SHIFT);
}

static int __maybe_unused sc8950_set_dm(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s dm:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_DPDM_CONTROL1,
		value, SC8950_REG_DPDM_CONTROL1_DM_DAC_MASK, SC8950_REG_DPDM_CONTROL1_DM_DAC_SHIFT);
}

static int sc8950_set_ico(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s ico_en:0x%x\n", __func__, value);
	
	return sc8950_update_bits(info, SC8950_REG_DPDM_CONTROL2,
		value, SC8950_REG_DPDM_CONTROL2_ICO_EN_MASK, SC8950_REG_DPDM_CONTROL2_ICO_EN_SHIFT);
}

static int sc8950_set_hvdcp(struct sc8950_info *info, unsigned char value)
{
	dev_info(info->dev, "%s hvdcp_en:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_DPDM_CONTROL2,
		value, SC8950_REG_DPDM_CONTROL2_HVDCP_EN_MASK, SC8950_REG_DPDM_CONTROL2_HVDCP_EN_SHIFT);
}

static int __maybe_unused sc8950_set_test_mode_7f(struct sc8950_info *info, bool en);

static int sc8950_set_force_dpdm(struct sc8950_info *info, unsigned char value)
{
	int ret = 0;
	dev_info(info->dev, "%s force_dpdm_en:0x%x\n", __func__, value);
	if (info->first_plugin) {
		info->first_plugin = false;
		info->force_dpdm = true;
		sc8950_set_test_mode_7f(info, true);
		info->cnt = 0;
		sc8950_write_byte(info, 0xD3, 0x85);
		sc8950_write_byte(info, 0xD4, 0x6a);
		sc8950_write_byte(info, 0xD3, 0x95);
		mdelay(10);
		if (info->cnt < 20) {
			sc8950_write_byte(info, 0xD3, 0x05);
			sc8950_write_byte(info, 0xD3, 0x2d);
			dev_info(info->dev, "adapter is not unknown\n");
		} else {
			sc8950_write_byte(info, 0xD3, 0x1d);
			dev_info(info->dev, "adapter is unknown\n");
		}
		sc8950_write_byte(info, 0xD3, 0x00);
		sc8950_set_test_mode_7f(info, false);
		info->force_dpdm = false;
		
	} else {
		ret = sc8950_update_bits(info, SC8950_REG_DPDM_CONTROL2,
		value, SC8950_REG_DPDM_CONTROL2_FORCE_DPDM_MASK, SC8950_REG_DPDM_CONTROL2_FORCE_DPDM_SHIFT);
	}
		
	return ret;
}

static int sc8950_set_autodpdm(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s autodpdm_en:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_DPDM_CONTROL2,
		value, SC8950_REG_DPDM_CONTROL2_AUTO_DPDPM_EN_MASK, SC8950_REG_DPDM_CONTROL2_AUTO_DPDPM_EN_SHIFT);
}

static int sc8950_set_wdt_rst(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s wdt_rst:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL1,
		value, SC8950_REG_SYSTEM_CONTROL1_WD_RST_MASK, SC8950_REG_SYSTEM_CONTROL1_WD_RST_SHIFT);
}

static int sc8950_set_en_otg(struct sc8950_info *info, unsigned char value)
{
	dev_info(info->dev, "%s otg en:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL1,
		value, SC8950_REG_SYSTEM_CONTROL1_OTG_CFG_MASK, SC8950_REG_SYSTEM_CONTROL1_OTG_CFG_SHIFT);
}

int sc8950_get_en_otg(struct charger_device *chg_dev, unsigned char *value)
{
  	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);
  	
	return sc8950_read_interface(info, SC8950_REG_SYSTEM_CONTROL1,
		value, SC8950_REG_SYSTEM_CONTROL1_OTG_CFG_MASK, SC8950_REG_SYSTEM_CONTROL1_OTG_CFG_SHIFT);
}

static int sc8950_set_en_charge(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s charge en:0x%x,boost_mode:%d\n", __func__, value, info->boost_mode);
	/* in boost mode,if en charge will cut off reverse charge */
	if (info->boost_mode && value)
		return 0;
	else
		return sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL1,
			value, SC8950_REG_SYSTEM_CONTROL1_CHG_CFG_MASK, SC8950_REG_SYSTEM_CONTROL1_CHG_CFG_SHIFT);
}

static int sc8950_get_en_charge(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_SYSTEM_CONTROL1,
		value, SC8950_REG_SYSTEM_CONTROL1_CHG_CFG_MASK, SC8950_REG_SYSTEM_CONTROL1_CHG_CFG_SHIFT);
}

static int sc8950_set_ircmp_resistor(struct sc8950_info *info, unsigned int value)
{
	if(value >= SC8950_BAT_COMP_MAX) {
		value = SC8950_BAT_COMP_MAX;
	}

	dev_err(info->dev, "%s set ircmp bat_comp:%d\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_TREG,
		value/SC8950_BAT_COMP_LSB, SC8950_REG_TREG_BAT_COMP_MASK, SC8950_REG_TREG_BAT_COMP_SHIFT);
}

static int sc8950_set_ircmp_vclamp(struct sc8950_info *info, unsigned int value)
{
	if(value >= SC8950_VCLAMP_MAX) {
		value = SC8950_VCLAMP_MAX;
	}

	dev_dbg(info->dev, "%s set ircmp vclamp:%d\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_TREG,
		value/SC8950_VCLAMP_LSB, SC8950_REG_TREG_VCLAMP_MASK, SC8950_REG_TREG_VCLAMP_SHIFT);
}

static int sc8950_set_vsys_min(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s vsys_min:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL1,
		value, SC8950_REG_SYSTEM_CONTROL1_VSYS_MIN_MASK, SC8950_REG_SYSTEM_CONTROL1_VSYS_MIN_SHIFT);
}

static int sc8950_set_vbatlow_otg(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s vbatlow_otg:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL1,
		value, SC8950_REG_SYSTEM_CONTROL1_VBATLOW_OTG_MASK, SC8950_REG_SYSTEM_CONTROL1_VBATLOW_OTG_SHIFT);
}

static int sc8950_set_icc(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s icc:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_ICC,
		value, SC8950_REG_ICC_ICC_MASK, SC8950_REG_ICC_ICC_SHIFT);
}

static int sc8950_get_icc(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_ICC,
		value, SC8950_REG_ICC_ICC_MASK, SC8950_REG_ICC_ICC_SHIFT);
}

static int sc8950_set_iprechg(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s iprechg:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_ITC_ITERM,
		value, SC8950_REG_ITC_ITERM_ITC_MASK, SC8950_REG_ITC_ITERM_ITC_SHIFT);
}

static int sc8950_set_iterm(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s iterm:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_ITC_ITERM,
		value, SC8950_REG_ITC_ITERM_ITERM_MASK, SC8950_REG_ITC_ITERM_ITERM_SHIFT);
}

static int sc8950_get_iterm(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_ITC_ITERM,
		value, SC8950_REG_ITC_ITERM_ITERM_MASK, SC8950_REG_ITC_ITERM_ITERM_SHIFT);
}

static int sc8950_set_vreg(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s vreg:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_VBAT,
		value, SC8950_REG_VBAT_VBAT_MASK, SC8950_REG_VBAT_VBAT_SHIFT);
}

static int sc8950_get_vreg(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_VBAT,
		value, SC8950_REG_VBAT_VBAT_MASK, SC8950_REG_VBAT_VBAT_SHIFT);
}

static int sc8950_set_vrechg(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s vrechg:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_VBAT,
		value, SC8950_REG_VBAT_VRECHG_MASK, SC8950_REG_VBAT_VRECHG_SHIFT);
}

static int sc8950_set_en_term(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s term en:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL2,
		value, SC8950_REG_SYSTEM_CONTROL2_EN_TERM_MASK, SC8950_REG_SYSTEM_CONTROL2_EN_TERM_SHIFT);
}

static int sc8950_set_statdis(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s STAT pin function en:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL2,
		value, SC8950_REG_SYSTEM_CONTROL2_STAT_DIS_MASK, SC8950_REG_SYSTEM_CONTROL2_STAT_DIS_SHIFT);
}

static int sc8950_set_watchdog_cfg(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s wtd_cfg:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL2,
		value, SC8950_REG_SYSTEM_CONTROL2_TWD_MASK, SC8950_REG_SYSTEM_CONTROL2_TWD_SHIFT);
}

static int sc8950_set_en_timer(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s safety timer en:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL2,
		value, SC8950_REG_SYSTEM_CONTROL2_EN_TIMER_MASK, SC8950_REG_SYSTEM_CONTROL2_EN_TIMER_SHIFT);
}

static int sc8950_get_en_timer(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_SYSTEM_CONTROL2,
		value, SC8950_REG_SYSTEM_CONTROL2_EN_TIMER_MASK, SC8950_REG_SYSTEM_CONTROL2_EN_TIMER_SHIFT);
}

static int sc8950_set_tchg(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s charge time:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL2,
		value, SC8950_REG_SYSTEM_CONTROL2_TCHG_MASK, SC8950_REG_SYSTEM_CONTROL2_TCHG_SHIFT);
}
#if IS_ENABLED(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT) || IS_ENABLED(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT)
static int sc8950_set_chg_thermal(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s chg_thermal:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_TREG,
		value, SC8950_REG_TREG_TREG_MASK, SC8950_REG_TREG_TREG_SHIFT);
}

static int sc8950_set_ovp(struct sc8950_info *info, unsigned char val)
{
	int ret;
	unsigned char sc8950_reg = 0;

	ret = sc8950_read_byte(info, SC8950_REG_83, &sc8950_reg);
	if (ret < 0) {
		__sc8950_set_key(info);
	}

	sc8950_update_bits(info, SC8950_REG_83, val, 
		SC8950_REG_83_VBUS_OVP_MASK, SC8950_REG_83_VBUS_OVP_SHIFT);

	return __sc8950_set_key(info);
}
#endif
static int sc8950_set_batfet_reset_en(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s batfet reset en:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL3,
		value, SC8950_REG_SYSTEM_CONTROL3_BATFET_RST_EN_MASK, SC8950_REG_SYSTEM_CONTROL3_BATFET_RST_EN_SHIFT);
}

static int sc8950_get_boostv(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_BOOST_CONTROL,
		value, SC8950_REG_BOOST_CONTROL_VBOOST_MASK, SC8950_REG_BOOST_CONTROL_VBOOST_SHIFT);
}

static int sc8950_set_boostv(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s boostv:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_BOOST_CONTROL,
		value, SC8950_REG_BOOST_CONTROL_VBOOST_MASK, SC8950_REG_BOOST_CONTROL_VBOOST_SHIFT);
}

static int sc8950_get_iboost(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_BOOST_CONTROL,
		value, SC8950_REG_BOOST_CONTROL_IBOOST_MASK, SC8950_REG_BOOST_CONTROL_IBOOST_SHIFT);
}

static int sc8950_set_iboost(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s iboost:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_BOOST_CONTROL,
		value, SC8950_REG_BOOST_CONTROL_IBOOST_MASK, SC8950_REG_BOOST_CONTROL_IBOOST_SHIFT);
}

static int sc8950_get_vbus_stat(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_STAT1,
		value, SC8950_REG_STAT1_VBUS_STAT_MASK, SC8950_REG_STAT1_VBUS_STAT_SHIFT);
}

static int sc8950_get_chg_stat(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_STAT1,
		value, SC8950_REG_STAT1_CHRG_STAT_MASK, SC8950_REG_STAT1_CHRG_STAT_SHIFT);
}

static int sc8950_get_vg_stat(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_STAT6,
		value, SC8950_REG_STAT6_VBUS_GD_MASK, SC8950_REG_STAT6_VBUS_GD_SHIFT);
}

static int sc8950_set_force_vindpm(struct sc8950_info *info, unsigned char value)
{
	unsigned int ret = 0, cyc_cnt = 3;
	unsigned char retval;

	dev_dbg(info->dev, "%s force vindpm:0x%x\n", __func__, value);

	sc8950_read_interface(info, SC8950_REG_VINDPM,
		&retval, SC8950_REG_VINDPM_FORCE_MASK, SC8950_REG_VINDPM_FORCE_SHIFT);
	
	while(retval != 0x1 && cyc_cnt > 0){
		/* 0x1: Run Absolute VINDPM Threshold */
		sc8950_update_bits(info, SC8950_REG_VINDPM,
			0x1, SC8950_REG_VINDPM_FORCE_MASK, SC8950_REG_VINDPM_FORCE_SHIFT);

		msleep(5);
		cyc_cnt--;
		sc8950_read_interface(info, SC8950_REG_VINDPM,
			&retval, SC8950_REG_VINDPM_FORCE_MASK, SC8950_REG_VINDPM_FORCE_SHIFT);
	}
	
	ret = sc8950_update_bits(info, SC8950_REG_VINDPM,
		value, SC8950_REG_VINDPM_VALUE_MASK, SC8950_REG_VINDPM_VALUE_SHIFT);

	return ret;
}

static int sc8950_get_force_vindpm(struct sc8950_info *info, unsigned char *value)
{
	unsigned int ret = 0;
	unsigned char retval;
	
	sc8950_read_interface(info, SC8950_REG_VINDPM,
		&retval, SC8950_REG_VINDPM_FORCE_MASK, SC8950_REG_VINDPM_FORCE_SHIFT);

	if(retval != 0x1) {
		dev_info(info->dev, "%s its not force vindpm,return!\n", __func__);
	} else {
		dev_info(info->dev, "%s its force vindpm!\n", __func__);
		ret = sc8950_read_interface(info, SC8950_REG_VINDPM,
			value, SC8950_REG_VINDPM_VALUE_MASK, SC8950_REG_VINDPM_VALUE_SHIFT);
	}
	return ret;
}

static int sc8950_get_vindpm_stat(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_STAT8,
		value, SC8950_REG_STAT8_VINDPM_STAT_MASK, SC8950_REG_STAT8_VINDPM_STAT_SHIFT);
}

static int sc8950_get_iindpm_stat(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_STAT8,
		value, SC8950_REG_STAT8_IINDPM_STAT_MASK, SC8950_REG_STAT8_IINDPM_STAT_SHIFT);
}

static int sc8950_get_pd(struct sc8950_info *info, unsigned char *value)
{
	return sc8950_read_interface(info, SC8950_REG_PN,
		value, SC8950_REG_PN_PN_MASK, SC8950_REG_PN_PN_SHIFT);
}

static int sc8950_set_rst(struct sc8950_info *info, unsigned char value)
{
	dev_dbg(info->dev, "%s rst:0x%x\n", __func__, value);
	return sc8950_update_bits(info, SC8950_REG_PN,
		value, SC8950_REG_PN_REG_RST_MASK, SC8950_REG_PN_REG_RST_SHIFT);
}

static int __sc8950_set_key(struct sc8950_info *info)
{
	sc8950_write_byte(info, SC8950_REG_7D, SC8950_KEY1);
	sc8950_write_byte(info, SC8950_REG_7D, SC8950_KEY2);
	sc8950_write_byte(info, SC8950_REG_7D, SC8950_KEY3);
	return sc8950_write_byte(info, SC8950_REG_7D, SC8950_KEY4);
}

static int __maybe_unused sc8950_set_key_7f(struct sc8950_info *info) 
{
	sc8950_write_byte(info, SC8950_REG7F, SC8950_KEY2);
	sc8950_write_byte(info, SC8950_REG7F, SC8950_7F_KEY2);
	sc8950_write_byte(info, SC8950_REG7F, SC8950_KEY3);
	return sc8950_write_byte(info, SC8950_REG7F, SC8950_KEY2);
}
static int __maybe_unused sc8950_set_test_mode_7f(struct sc8950_info *info, bool en)
{
	int ret = 0;
	unsigned char reg_val = 0;
	int cnt = 0;
	do {
		ret = sc8950_read_byte(info, SC8950_REG7F, &reg_val);
		dev_info(info->dev, "%s: %x\n", __func__, reg_val);
		if(reg_val == 0x02 && !en) {
			dev_info(info->dev, "not in test mode\n");
			break;
		}
		
		if(reg_val == 0x03 && en) {
			dev_info(info->dev, "in test mode\n");
			break;
		}
		
		sc8950_write_byte(info, SC8950_REG7F, SC8950_KEY2);
		sc8950_write_byte(info, SC8950_REG7F, SC8950_7F_KEY2);
		sc8950_write_byte(info, SC8950_REG7F, SC8950_KEY3);
		sc8950_write_byte(info, SC8950_REG7F, SC8950_KEY2);
		cnt ++;
	}while(cnt < 20);
	return 0;
}

static int __maybe_unused sc8950_set_private(struct sc8950_info *info)
{
	int ret;
	unsigned char val;

	ret = sc8950_read_byte(info, SC8950_REG_DPDM3, &val);
	if (ret < 0) {
		__sc8950_set_key(info);
	}

	sc8950_write_byte(info, SC8950_REG_DPDM3, SC8950_PRIVATE);

	return __sc8950_set_key(info);
}

static int __maybe_unused sc8950_set_transsin_protocol(struct sc8950_info *info, bool en)
{
	int ret;
	unsigned char sc8950_reg = 0;

	ret = sc8950_read_byte(info, SC8950_REG_83, &sc8950_reg);
	if (ret < 0) {
		__sc8950_set_key(info);
	}

	sc8950_update_bits(info, SC8950_REG_83, (!!en), 
		SC8950_REG_83_TC30_DPDM_SET_MASK, SC8950_REG_83_TC30_DPDM_SET_SHIFT);

	return __sc8950_set_key(info);
}

static int sc8950_get_reg83(struct sc8950_info *info, unsigned char *val)
{
	int ret;

	ret = sc8950_read_byte(info, SC8950_REG_83, val);
	if (ret < 0) {
		__sc8950_set_key(info);
	}

	sc8950_read_byte(info, SC8950_REG_83, val);

	dev_dbg(info->dev, "[%s] Reg[%x]=0x%x\n", __func__, SC8950_REG_83, *val);

	return __sc8950_set_key(info);
}


/***************** SC8950 IC Reg Operation *****************/

/************** SC8950 Charger Ops Operation ***************/
static int sc8950_set_ichg(struct charger_device *chg_dev, u32 uA)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	if(uA >= SC8950_ICC_MAX) {
		uA = SC8950_ICC_MAX;
	}
	dev_info(info->dev, "%s set %d uA\n", __func__, uA);

	return sc8950_set_icc(info, uA/SC8950_ICC_LSB);
}

static int sc8950_set_aicr(struct charger_device *chg_dev, u32 uA)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);
	u32 tmp = uA;
	unsigned char reg_val = 0;

	if(uA >= SC8950_AICR_MAX) {
		tmp = SC8950_AICR_MAX;
	}else if (uA <= SC8950_AICR_OFFSET) {
		tmp = SC8950_AICR_OFFSET;
	}
	reg_val = (tmp - SC8950_AICR_OFFSET)/ SC8950_AICR_LSB;
	
	dev_info(info->dev, "%s uA:%d reg_val:%d\n", __func__, uA, reg_val);
	
	return sc8950_set_ibus(info, reg_val);
}

static int __maybe_unused sc8950_check_bc12_done(struct sc8950_info *info, unsigned char *state)
{
	int ret = 0;
	unsigned char val = 0;

	ret = sc8950_read_byte(info, SC8950_REG_STAT1, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s failed!\n", __func__);
		return ret;
	}

	*state = ((val & 0x04) >> 2);

	return 0;
}

static int sc8950_plug_in(struct charger_device *chg_dev)
{
	int ret = 0;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);
	dev_info(info->dev, "%s\n", __func__);
	ret = sc8950_set_en_charge(info, 0x1);
	if (ret < 0) {
		dev_err(info->dev, "%s dis charge fail(%d)\n", __func__, ret);
		return ret;
	}

	/* enable wdt */
	ret = sc8950_set_watchdog_cfg(info, SC8950_WDT_TIME_DIS);
	if(ret < 0) {
		dev_err(info->dev, "%s set wdt failed ret:%d\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int sc8950_plug_out(struct charger_device *chg_dev)
{
	int ret = 0;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info->dev, "%s\n", __func__);

#if IS_ENABLED(CONFIG_MTK_TC30_SUPPORT)
	sc8950_tc30_gpio_enable(chg_dev, false);
#endif

	/* Disable charging */
	ret = sc8950_set_en_charge(info, 0x0);
	if (ret < 0) {
		dev_err(info->dev, "%s dis charge fail(%d)\n", __func__, ret);
		return ret;
	}

	/* Disable WDT */
	ret = sc8950_set_watchdog_cfg(info, SC8950_WDT_TIME_DIS);
	if (ret < 0) {
		dev_err(info->dev, "%s set wdt fail(%d)\n", __func__, ret);
	}

	info->vg_skip = false;
	cancel_delayed_work(&info->vindpm_force_work);
	sc8950_set_force_vindpm(info, (12000000 - SC8950_VINDPM_BASE) / SC8950_VINDPM_LSB);
	info->vindpm_force = true;
	info->vbus_stat_pre = false;
	atomic_set(&info->pd_set_type, false);

	return ret;
}

static int sc8950_enable_charging(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info ->dev, "%s en:%d\n", __func__, en);

	ret = sc8950_set_en_charge(info, en);
	if (ret < 0) {
		dev_err(info->dev, "%s enable charge fail(%d)\n", __func__, ret);
		return ret;
	}

	if (info->chg_en_by_gpio) {
		if (gpio_is_valid(info->en_gpio)) {
			if (en)
				gpio_direction_output(info->en_gpio, GPIO_CHG_ENABLE);
			else
				gpio_direction_output(info->en_gpio, GPIO_CHG_DISABLE);
		} else {
			dev_err(info->dev, "%s enable sc8950 charge gpio fail\n", __func__);
			return -EFAULT;
		}
	}

	return ret;
}

static int sc8950_is_charging_enabled(struct charger_device *chg_dev, bool *en)
{
	int ret;
	unsigned char val = 0;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = sc8950_get_en_charge(info, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s get charge stat fail(%d)\n", __func__, ret);
		return ret;
	}

	*en = val;

	if (info->chg_en_by_gpio) {
		if (gpio_is_valid(info->en_gpio)) {
			val = gpio_get_value(info->en_gpio);
			if (val == GPIO_CHG_ENABLE && *en)
				*en = 1;
			else
				*en = 0;
			pr_debug("%s: sc8950 gpio is %d, %s\n", __func__, val, (*en == 1)?"enabled":"disabled");
			return 0;
		}
	}

	dev_dbg(info ->dev, "%s en:%d\n", __func__, *en);

	return ret;
}

static int sc8950_get_ichg(struct charger_device *chg_dev, u32 *uA)
{
	int ret = 0;
	unsigned char val;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = sc8950_get_icc(info, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s get icc fail(%d)\n", __func__, ret);
		return ret;
	}

	*uA = val*SC8950_ICC_LSB;

	dev_dbg(info ->dev, "%s uA:%d\n", __func__, *uA);

	return ret;
}

static int sc8950_get_min_ichg(struct charger_device *chg_dev, u32 *uA)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);
	
	*uA = SC8950_ICC_MIN;
	
	dev_dbg(info->dev, "%s %d uA\n", __func__, *uA);
	
	return 0;
}

static int sc8950_get_cv(struct charger_device *chg_dev, u32 *uV)
{
	int ret = 0;
	unsigned char val;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = sc8950_get_vreg(info, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s get cv fail(%d)\n", __func__, ret);
		return ret;
	}

	*uV = SC8950_CV_BASE + val * SC8950_CV_LSB;
	dev_dbg(info ->dev, "%s uV:%d\n", __func__, *uV);

	return ret;
}

static int sc8950_set_cv(struct charger_device *chg_dev, u32 uV)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	if(uV >= SC8950_CV_MAX) {
		uV = SC8950_CV_MAX;
	}
	dev_dbg(info->dev, "%s set %d uV\n", __func__, uV);

	return sc8950_set_vreg(info, (uV - SC8950_CV_BASE) / SC8950_CV_LSB);
}

static int sc8950_get_aicr(struct charger_device *chg_dev, u32 *uA)
{
	int ret = 0;
	unsigned char val;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = sc8950_get_ibus(info, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s get ibus fail(%d)\n", __func__, ret);
		return ret;
	}

	*uA = val * SC8950_AICR_LSB + SC8950_AICR_OFFSET;
	dev_dbg(info ->dev, "%s uA:%d\n", __func__, *uA);

	return ret;
}

static int sc8950_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = SC8950_AICR_MIN;

	return 0;
}

static int sc8950_get_ieoc(struct charger_device *chg_dev, u32 *uA)
{
	int ret = 0;
	unsigned char val;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = sc8950_get_iterm(info, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s get ieoc fail(%d)\n", __func__, ret);
		return ret;
	}

	*uA = SC8950_ITERM_BASE + val * SC8950_ITERM_LSB;
	dev_dbg(info ->dev, "%s uA:%d\n", __func__, *uA);

	return ret;
}

static int sc8950_set_ieoc(struct charger_device *chg_dev, u32 uA)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);
	unsigned char ieoc = 0;

	if(uA >= SC8950_ITERM_MAX) {
		uA = SC8950_ITERM_MAX;
	}

	ieoc = (uA - SC8950_ITERM_BASE) / SC8950_ITERM_LSB;

	dev_dbg(info->dev, "%s set %d uA reg:0x%x\n", __func__, uA, ieoc);

	return sc8950_set_iterm(info, ieoc);
}

static int sc8950_kick_wdt(struct charger_device *chg_dev)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	return sc8950_set_wdt_rst(info, 1);
}

static int sc8950_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info->dev, "%s event = %d\n", __func__, event);
	power_supply_changed(info->psy);

	return 0;
}

static int sc8950_set_mivr(struct charger_device *chg_dev, u32 uV)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	if(uV >= SC8950_VINDPM_MAX) {
		uV = SC8950_VINDPM_MAX;
	}
	dev_info(info->dev, "%s set %d uV,vindpm_force is %d\n", __func__, uV,info->vindpm_force);

	if (info->vindpm_force) {
		return 0;
	}
	return sc8950_set_force_vindpm(info, (uV - SC8950_VINDPM_BASE) / SC8950_VINDPM_LSB);
}

static void sc8950_vindpm_force(struct work_struct *work)
{
	struct sc8950_info *info = container_of(work,
			struct sc8950_info,
			vindpm_force_work.work);
	info->vindpm_force = false;
  	dev_info(info->dev,"%s\n",__func__);
	sc8950_set_force_vindpm(info, (4600000 - SC8950_VINDPM_BASE) / SC8950_VINDPM_LSB);
}

int sc8950_get_mivr(struct charger_device *chg_dev, u32 *uV)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);
	unsigned char val = 0;
	int ret = 0;

	ret = sc8950_get_force_vindpm(info, &val);

	*uV = val * SC8950_VINDPM_LSB + SC8950_VINDPM_BASE;
	if(*uV <= SC8950_VINDPM_MIN)
		*uV = SC8950_VINDPM_MIN;

	dev_dbg(info->dev, "%s get mivr: %d uV\n", __func__, *uV);

	return ret;
}

static int sc8950_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
	int ret;
	unsigned char val;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = sc8950_get_vindpm_stat(info, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s get mivr_state failed(%d)\n", __func__, ret);
		return ret;
	}

	*in_loop = val;
	dev_dbg(info ->dev, "%s mivr_state:0x%x\n", __func__, *in_loop);

	return ret;
}

static int __maybe_unused sc8950_get_aicr_state(struct charger_device *chg_dev, bool *in_loop)
{
	int ret;
	unsigned char val;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = sc8950_get_iindpm_stat(info, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s get aicr_state failed(%d)\n", __func__, ret);
		return ret;
	}

	*in_loop = val;
	dev_dbg(info ->dev, "%s aicr_state:0x%x\n", __func__, *in_loop);

	return ret;
}


static int sc8950_en_hz(struct charger_device *chg_dev, bool en)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info ->dev, "%s powerpath en:0x%x\n", __func__, en);

	return sc8950_set_hiz(info, en);
}

static int sc8950_enable_powerpath(struct charger_device *chg_dev, bool en)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info ->dev, "%s powerpath en:0x%x\n", __func__, en);

	return sc8950_set_hiz(info, !en);
}

static int sc8950_is_powerpath_enabled(struct charger_device *chg_dev, bool *en)
{
	int ret;
	unsigned char val;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = sc8950_get_hiz(info, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s get powerpath_enabled state failed(%d)\n", __func__, ret);
		return ret;
	}

	*en = !val;
	dev_dbg(info ->dev, "%s powerpath_enabled:0x%x\n", __func__, *en);

	return 0;
}

static int sc8950_enable_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	return sc8950_set_en_timer(info, en);
}

static int sc8950_is_safety_timer_enabled(struct charger_device *chg_dev,
                    bool *en)
{
	int ret;
	unsigned char val;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = sc8950_get_en_timer(info, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s get safety_timer_enabled state failed(%d)\n", __func__, ret);
		return ret;
	}

	*en = val;
	dev_dbg(info ->dev, "%s safety_timer_enabled:0x%x\n", __func__, *en);

	return ret;
}

static int sc8950_enable_te(struct charger_device *chg_dev, bool en)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);
	return sc8950_set_en_term(info, en);
}

static int sc8950_enable_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	info->boost_mode = en;
	if (en) {
		ret = sc8950_set_watchdog_cfg(info, SC8950_WDT_TIME_DIS);
		if (ret < 0) {
			dev_err(info->dev, "%s set wdt fail(%d)\n", __func__, ret);
			return ret;
		}

		ret=sc8950_set_hiz(info,!en);
		if (ret < 0) {
			dev_err(info->dev, "%s set wdt fail(%d)\n", __func__, ret);
			return ret;
		}
	}

	ret = sc8950_set_en_charge(info, !en);
	if (ret < 0) {
		dev_err(info->dev, "%s set charge fail(%d)\n", __func__, ret);
		return ret;
	}
	ret = sc8950_set_en_otg(info, en);
	if (ret < 0) {
		dev_err(info->dev, "%s set otg fail(%d)\n", __func__, ret);
		return ret;
	}

	if (!en) {
		ret = sc8950_set_watchdog_cfg(info, SC8950_WDT_TIME_DIS);
		if (ret < 0) {
			dev_err(info->dev, "%s set wdt fail(%d)\n", __func__, ret);
		}
	}

	return ret;
}

static int __maybe_unused sc8950_enable_dcp(struct charger_device *chg_dev, bool en)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	dev_dbg(info->dev, "%s, en :%d\n", __func__, en);

	if (en) {
		ret = sc8950_set_dp(info, SC8950_USB_0_6V);
		ret = sc8950_set_dm(info, SC8950_USB_0_6V);
	} else {
		ret = sc8950_set_dp(info, SC8950_USB_HIZ);
		ret = sc8950_set_dm(info, SC8950_USB_HIZ);
	}
	return ret;
}

static int __maybe_unused sc8950_get_boost_status(struct charger_device *chg_dev)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);
	unsigned char val = 0;

	sc8950_get_vbus_stat(info, &val);
	val = (val == SC8950_VBUSSTAT_OTG);
	dev_dbg(info->dev, "%s:%d\n", __func__, val);
	return val;
}

static int __maybe_unused sc8950_set_boost_current_limit(struct charger_device *chg_dev,
                    u32 uA)
{
	unsigned char val;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	if (uA <= 500000) {
		val = 0x0;
	} else if (uA <= 750000) {
		val = 0x1;
	} else if (uA <= 1200000) {
		val = 0x2;
	} else if (uA <= 1400000) {
		val = 0x3;
	} else if (uA <= 1650000) {
		val = 0x4;
	} else if (uA <= 1875000) {
		val = 0x5;
	} else if (uA <= 2150000) {
		val = 0x6;
	} else {
		val = 0x7;
	}

	dev_dbg(info->dev, "%s uA:%d reg_val:0x%x\n", __func__, uA, val);

	return sc8950_set_iboost(info, val);
}

static int __maybe_unused sc8950_set_boost_voltage_limit(struct charger_device *chg_dev,
					u32 uV)
{
	unsigned char regval=0;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	regval=sc8950_vboost_toreg(uV);
	dev_dbg(info->dev, "%s: uV=%d regval=%d\n",__func__,uV,regval);
	return sc8950_set_boostv(info,regval);
}

static int __sc8950_dump_registers(struct sc8950_info *info)
{
	int ret;
	int i;
	unsigned char val;
	unsigned char reg83_val = 0;

	for (i = 0; i < SC8950_REG_MAX; i++) {
		ret = sc8950_read_byte(info, i, &val);
		if (ret < 0) {
			return ret;
		}
		dev_info(info->dev, "%s reg[0x%02x] = 0x%02x\n", __func__, i, val);
	}

	sc8950_get_reg83(info, &reg83_val);
	dev_info(info->dev, "%s reg[0x%02x] = 0x%02x\n", __func__, SC8950_REG_83, reg83_val);

	return 0;
}

__maybe_unused static int sc8950_dump_registers(struct charger_device *chg_dev)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	return __sc8950_dump_registers(info);
}

#if IS_ENABLED(CONFIG_MTK_TC30_SUPPORT)
static int  sc8950_tc30_gpio_enable(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info->dev, "%s en:%d\n", __func__, en);

	ret = sc8950_set_transsin_protocol(info, en);

	if (!en) {
		sc8950_set_dp(info, SC8950_USB_HIZ);
		sc8950_set_dm(info, SC8950_USB_HIZ);
	}
	return ret;
}
#endif

// MD_CHG:add for charger by xin.he start Fri 09 Sep 2022 04:01:05 PM CST
static int __maybe_unused sc8950_set_dp_function(struct charger_device *chg_dev, enum dpdm_ctrl_status status)
{
	int ret = 0;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info->dev, "sc8950 set dp %s\n", dpdm_ctrl_name[status]);
	switch (status) {
		case DPDM_CTRL_HZ:
			ret = sc8950_set_dp(info, SC8950_USB_HIZ);
			break;
		case DPDM_CTRL_0V:
			ret = sc8950_set_dp(info, SC8950_USB_0V);
			//dev_info(chip->dev, "rt9471 not support dp 0V\n");
			break;
		case DPDM_CTRL_0_6V:
			ret = sc8950_set_dp(info, SC8950_USB_0_6V);
			break;
		case DPDM_CTRL_3_3V:
			ret = sc8950_set_dp(info, SC8950_USB_3_3V);
			//dev_info(chip->dev, "rt9471 not support dp 3.3V\n");
			break;
		default:
			break;
	}

	return ret;
}

static int __maybe_unused sc8950_set_dm_function(struct charger_device *chg_dev, enum dpdm_ctrl_status status)
{
	int ret = 0;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info->dev, "sc8950 set dm %s\n", dpdm_ctrl_name[status]);
	switch (status) {
		case DPDM_CTRL_HZ:
			ret = sc8950_set_dm(info, SC8950_USB_HIZ);
			break;
		case DPDM_CTRL_0V:
			ret = sc8950_set_dm(info, SC8950_USB_0V);
			break;
		case DPDM_CTRL_0_6V:
			ret = sc8950_set_dm(info, SC8950_USB_0_6V);
			break;
		case DPDM_CTRL_3_3V:
			ret = sc8950_set_dm(info, SC8950_USB_3_3V);
			break;
		default:
			break;
	}

	return ret;
}

#if !IS_ENABLED(CONFIG_TRAN_USB_CONTROL)
#if IS_ENABLED(CONFIG_MTK_HVDCP20_SUPPORT)
static int sc8950_hvdcp20_set_dp_0_6_v(struct charger_device *chg_dev)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info->dev, "%s\n", __func__);

	sc8950_set_dp(info, SC8950_USB_0V);
	sc8950_set_dm(info, SC8950_USB_0V);

	msleep(500);
	sc8950_set_dp(info, SC8950_USB_0_6V);//D+ set out 0.6V

	msleep(1800);

	return 0;
}

static int sc8950_hvdcp20_set_dp_3_0_v(struct charger_device *chg_dev,bool stat)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info->dev, "%s stat=%d\n", __func__, stat);

	if(stat) {
		sc8950_set_dp(info, SC8950_USB_3_3V);
	} else {
		sc8950_set_dp(info, SC8950_USB_0V);
	}

	return 0;
}

static int sc8950_hvdcp20_set_dm_0_6_v(struct charger_device *chg_dev)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info->dev, "%s\n", __func__);

	sc8950_set_dm(info, SC8950_USB_0_6V);//D- set out 0.6V

	return 0;
}

static int sc8950_hvdcp20_set_dp_dm_recover(struct charger_device *chg_dev)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info->dev, "%s!\n", __func__);

	sc8950_set_dp(info, SC8950_USB_HIZ);
	sc8950_set_dm(info, SC8950_USB_HIZ);

	return 0;
}
#endif
#endif

static int sc8950_set_pep20_efficiency_table(struct charger_device *chg_dev)
{
	return 0;
}

struct timespec64 ptime[13];
static int cptime[13][2];
#define PEOFFTIME 40
#define PEONTIME 90

static int dtime(int i)
{
    struct timespec64 time;

    time = timespec64_sub(ptime[i], ptime[i-1]);
    return time.tv_nsec/1000000;
}

static int sc8950_set_pep20_current_pattern(struct charger_device *chg_dev, u32 uV)
{
	int value;
	int i, j = 0;
	int flag;
	int errcnt = 0;
	ktime_t ktime_now;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);
	
	dev_dbg(info->dev, "%s!\n", __func__);

	sc8950_set_ichg(chg_dev, 2000000);

	mdelay(20);
	value = (uV - 5500000) / 500000;

	sc8950_set_aicr(chg_dev, 0);
	mdelay(150);

	ktime_now = ktime_get_boottime();
	ptime[j++] = ktime_to_timespec64(ktime_now);
	for (i = 4; i >= 0; i--) {
		flag = value & (1 << i);
		if (flag == 0) {
			sc8950_set_aicr(chg_dev, 800000);
			mdelay(PEOFFTIME);
			ktime_now = ktime_get_boottime();
			ptime[j] = ktime_to_timespec64(ktime_now);
			cptime[j][0] = PEOFFTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 30 || cptime[j][1] > 65) {
				errcnt = 1;
				return -1;
			}
			j++;
			
			sc8950_set_aicr(chg_dev, 0);
			mdelay(PEONTIME);
			ktime_now = ktime_get_boottime();
			ptime[j] = ktime_to_timespec64(ktime_now);
			cptime[j][0] = PEONTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 90 || cptime[j][1] > 115) {
				errcnt = 1;
				return -1;
			}
			
			j++;
		} else {
			sc8950_set_aicr(chg_dev, 800000);
			mdelay(PEONTIME);
			ktime_now = ktime_get_boottime();
			ptime[j] = ktime_to_timespec64(ktime_now);
			cptime[j][0] = PEONTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 90 || cptime[j][1] > 115) {
				errcnt = 1;
				return -1;
			}
			j++;
			
			sc8950_set_aicr(chg_dev,0);
			mdelay(PEOFFTIME);
			ktime_now = ktime_get_boottime();
			ptime[j] = ktime_to_timespec64(ktime_now);
			cptime[j][0] = PEOFFTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 30 || cptime[j][1] > 65) {
				errcnt = 1;
				return -1;
			}
			j++;
		}
	}

	sc8950_set_aicr(chg_dev, 800000);
	mdelay(200);
	ktime_now = ktime_get_boottime();
	ptime[j] = ktime_to_timespec64(ktime_now);
	cptime[j][0] = 200;
	cptime[j][1] = dtime(j);
	if (cptime[j][1] < 180 || cptime[j][1] > 240) {
		errcnt = 1;
		return -1;
	}
	
	j++;

	sc8950_set_aicr(chg_dev, 0);
	mdelay(150);
	sc8950_set_aicr(chg_dev, 800000);

	//sc8950_set_aicr(chg_dev, 2000000);

	mdelay(300);

	if (errcnt == 0) {
		return 0;
	}
	
	return -1;
}

static int sc8950_reset_ta(struct charger_device *chg_dev)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);
	union power_supply_propval val = {.intval = 0};

	dev_dbg(info->dev, "%s \n", __func__);

	/* 800mA */
	val.intval = 800000;
	power_supply_set_property(info->psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);

	/* 50mA */
	val.intval = 50000;
	power_supply_set_property(info->psy,
			POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);

	msleep(250);//250ms

	/* 2A */
	val.intval = 2000000;
	power_supply_set_property(info->psy,
			POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
	
	return 0;
}

//USD:add for X6861H963-494 by lei.shi5 20240425 start
static int __maybe_unused sc8950_set_ffc_sw_eoc(struct charger_device *chg_dev, bool chg_done)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_dbg(info->dev, "%s: sw eoc:%d\n", __func__, chg_done);

	info->ffc_sw_eoc = chg_done;

	return 0;
}
//USD:add for X6861H963-494 by lei.shi5 20240425 end

static int sc8950_check_fault_state(struct sc8950_info *info, unsigned char *state)
{
	int ret = 0;

	ret = sc8950_read_byte(info, SC8950_REG_STAT2, state);
	if (ret < 0) {
		dev_err(info->dev, "%s read reg[0x%x] failed!\n", __func__, SC8950_REG_STAT2);
		return ret;
	}

	return 0;
}

static void sc8950_get_ic_stat(struct sc8950_info *info, u32 *val)
{
	unsigned int ret = 0;
	unsigned char stat = 0, stat1 = 0;
	bool chg_en = false;
	unsigned char charg_fault = 0;

	//USD:add for X6861H963-494 by lei.shi5 20240425 start
	if (info->ffc_sw_eoc) {
		dev_info(info->dev, "sw chg done\n");
		*val = POWER_SUPPLY_STATUS_FULL;
		return;
	}
	//USD:add for X6861H963-494 by lei.shi5 20240425 end
	sc8950_get_chg_stat(info,&stat);
	if (stat == 0) {
		sc8950_get_vg_stat(info, &stat1);
		stat = (stat1?SC8950_ICSTAT_FASTCHG:SC8950_ICSTAT_DISABLE);
	}
	ret = atomic_read(&info->attach);
	sc8950_check_fault_state(info, &charg_fault);
	sc8950_is_charging_enabled(info->chg_dev,&chg_en);
	dev_dbg(info->dev, " %s stat =%d,online =%d,chg_en = %d\n", __func__,stat,ret,chg_en);
	
	switch (stat) {
		case SC8950_ICSTAT_DISABLE:
		case SC8950_ICSTAT_PRECHG:
		case SC8950_ICSTAT_FASTCHG:
			*val = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case SC8950_ICSTAT_CHGDONE:
			*val = POWER_SUPPLY_STATUS_FULL;
			break;
		default:
			if (charg_fault & SC8950_BAT_FAULT_MASK) {
				*val = POWER_SUPPLY_STATUS_FULL;
				dev_err(info->dev, "%s bat ovp!reg[c]=0x%x\n", __func__, charg_fault);
			} else {
				*val = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
			break;
	}
}

static int sc8950_chg_set_usbsw(struct sc8950_info *info,
                                enum sc8950_usbsw usbsw)
{
        struct phy *phy;
        int ret, mode = (usbsw == SC8950_USBSW_CHG) ? SC8950_PHY_MODE_BC11_SET :
                                               SC8950_PHY_MODE_BC11_CLR;

        dev_dbg(info->dev, "%s:usbsw=%d\n",__func__,usbsw);
        phy = phy_get(info->dev, "usb2-phy");
        if (IS_ERR_OR_NULL(phy)) {
                dev_err(info->dev, "%s:failed to get usb2-phy\n",__func__);
                return -ENODEV;
        }    
        ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
        if (ret)
                dev_err(info->dev, "%s:failed to set phy ext mode\n",__func__);
        phy_put(info->dev, phy);
        return ret; 
}

static bool is_usb_rdy(struct device *dev)
{
	bool ready = true;
	struct device_node *node = NULL;

	node = of_parse_phandle(dev->of_node, "usb", 0);
	if (node) {
		ready = !of_property_read_bool(node, "cdp-block");
		dev_info(dev, "usb ready = %d\n", ready);
	} else
		dev_notice(dev, "usb node missing or invalid\n");
	return ready;
}

static int sc8950_chg_enable_bc12(struct sc8950_info *info, bool en)
{
        int i, ret;
        static const int max_wait_cnt = 250;

        dev_info(info->dev, "%s:en=%d\n",__func__,en);
        if (en) {
                /* CDP port specific process */
                dev_info(info->dev, "check CDP block\n");
                for (i = 0; i < max_wait_cnt; i++) {
                        if (is_usb_rdy(info->dev))
                                break;
                        if (!atomic_read(&info->vbus_gd) || !atomic_read(&info->attach)) {
							dev_notice(info->dev, "%s:Plug out or shutdown!\n", __func__);
							return 0;
                        }
                        msleep(100);
                }
                if (i == max_wait_cnt)
                        dev_notice(info->dev, "%s:CDP timeout\n", __func__);
                else
                        dev_info(info->dev, "%s:CDP free\n", __func__);
        }
        ret = sc8950_chg_set_usbsw(info, en ? SC8950_USBSW_CHG : SC8950_USBSW_USB);

	return ret;
}

#define BQ_DET_COUNT_MAX 50
#define VG_DET_COUNT_MAX 20
static void sc8950_bc12_detect(struct work_struct *work)
{
	unsigned char usb_status = 0;
	unsigned char stat = 0;
	unsigned char pg_state = 0;
	bool attach;
	int i=0;
	struct sc8950_info *info = container_of(work,struct sc8950_info,bc12_work);

	//BSP_CHG:add for X6853H895-3 by lei.shi5 20230914 start
#if IS_ENABLED(CONFIG_WIRELESS_TA) || IS_ENABLED(CONFIG_TRAN_WIRELESS_TA)
	int ret = 0;
	struct power_supply *wireless_psy;
	union power_supply_propval prop;
	wireless_psy = power_supply_get_by_name("wireless");
	if (!wireless_psy) {
		dev_info(info->dev, "%s: get wireless power supply failed\n", __func__);
	}
#endif

	mutex_lock(&info->attach_lock);

	attach = atomic_read(&info->vbus_gd);
	if ((atomic_read(&info->attach) == attach) && !(info->psy_desc.type == POWER_SUPPLY_TYPE_USB && 
		info->psy_usb_type == POWER_SUPPLY_USB_TYPE_DCP) && !(info->psy_desc.type == POWER_SUPPLY_TYPE_USB && 
		info->psy_usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN)) {
		dev_info(info->dev, "%s attach(%d) is the same\n",
				    __func__, attach);
		goto out;
	}
	atomic_set(&info->attach, attach);

	pr_info("%s: attach= 0x%X\n", __func__, attach);

#if IS_ENABLED(CONFIG_WIRELESS_TA) || IS_ENABLED(CONFIG_TRAN_WIRELESS_TA)
	if (!IS_ERR_OR_NULL(wireless_psy)) {
		ret = power_supply_get_property(wireless_psy,POWER_SUPPLY_PROP_ONLINE, &prop);
		if(prop.intval){
			dev_info(info->dev, "%s attach(%d) wireless charger\n",__func__, attach);
			mutex_unlock(&info->attach_lock);
			return;
		}
	}
#endif
	//BSP_CHG:add for X6853H895-3 by lei.shi5 20230914 end
	if(!attach || info->boost_mode){
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		atomic_set(&info->pd_set_type, false);
		goto out;
	}

	sc8950_enable_powerpath(info->chg_dev,true);
//add for can not recognize docking station by xiewei 20241021 start
#if IS_ENABLED(CONFIG_CHARGER_TYPE_MT6357)
	mt6357_hw_bc11_init();
#endif
//add for can not recognize docking station by xiewei 20241021 end

	for(i = 0; i < VG_DET_COUNT_MAX && atomic_read(&info->vbus_gd); i++) {
		msleep(20);
		sc8950_get_vg_stat(info, &stat);
		pr_info("%s vg_stat:%d\n", __func__, stat);
		if (stat)
			break;
	}

	msleep(80);
	sc8950_chg_enable_bc12(info, true);

	sc8950_set_force_dpdm(info, 1);

	//__sc8950_dump_registers(info);

	for(i = 0; i < BQ_DET_COUNT_MAX && atomic_read(&info->vbus_gd); i++){
		msleep(30);
		sc8950_check_bc12_done(info, &pg_state);
		sc8950_get_vg_stat(info, &stat);
		pr_info("%s: pg_state=%d  count=%d\n",__func__, pg_state,i);
		if(!stat) {
			pr_info("%s vbus(%d) not good, exit that get charger type!\n", __func__, stat);
			break;
		} else if(pg_state) {
			break;
		}
	}

	sc8950_get_vbus_stat(info,&usb_status);
	pr_debug("%s: usb_stats= 0x%X\n", __func__, usb_status);
//add for can not recognize docking station by xiewei 20241021 start
#if IS_ENABLED(CONFIG_CHARGER_TYPE_MT6357)
	mt6357_hw_bc11_done();
#endif
//add for can not recognize docking station by xiewei 20241021 end
	if (atomic_read(&info->pd_set_type)) {
		pr_info("%s, PD already set type\n", __func__);
		goto out;
	}

	switch (usb_status) {
		case SC8950_CHG_TYPE_SDP:
			info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			info->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			break;
		case SC8950_CHG_TYPE_UNKNOW:
			info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			info->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			break;
		case SC8950_CHG_TYPE_CDP:
			info->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
			info->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
			break;
		case SC8950_CHG_TYPE_DCP:
			info->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			info->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			break;
		case SC8950_CHG_TYPE_SDPNSTD:
		case SC8950_CHG_TYPE_NOVBUS:	////VBUS maybe is not ready,retry to do bc1.2
		default:
			info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			info->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			break;
	}
out:
	mutex_unlock(&info->attach_lock);

	pr_info("%s: type=%d usb_type=%d\n", __func__, info->psy_desc.type,info->psy_usb_type);

	if (info->psy_desc.type != POWER_SUPPLY_TYPE_USB_DCP)
		sc8950_chg_enable_bc12(info, false);

	if (info->psy)
		power_supply_changed(info->psy);

}

static int sc8950_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	if(info == NULL) {
		ret = -EINVAL;
		goto out;
	}

	if (info->boost_mode && en) {
		dev_notice(info->dev, "in boost mode!\n");
		goto out;
	}

	if (atomic_read(&info->pd_set_type) && en) {
		goto out;
	}

	atomic_set(&info->vbus_gd, en);
	mutex_lock(&info->attach_lock);
	schedule_work(&info->bc12_work);
	mutex_unlock(&info->attach_lock);
out:
	return ret;
}

static int sc8950_chg_attach_process(struct sc8950_info *info,
                                      int attach)
 {
	switch (attach) {
	case ATTACH_TYPE_PD_SDP:
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case ATTACH_TYPE_PD_DCP:
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case ATTACH_TYPE_PD_NONSTD:
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	default:
		dev_info(info->dev,"%s: using tradtional bc12 flow!\n", __func__);
		return sc8950_enable_chg_type_det(info->chg_dev, attach);
	}
	atomic_set(&info->pd_set_type, true);
	atomic_set(&info->vbus_gd, true);
	atomic_set(&info->attach, !!attach);
	power_supply_changed(info->psy);
	dev_dbg(info->dev, "%s: attach = %d,type = %d\n", __func__, attach,info->psy_desc.type);
		return 0;
 }

static int sc8950_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	int ret;
	unsigned char val;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = sc8950_get_chg_stat(info, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s get chg stat fail(%d)\n", __func__, ret);
		return ret;
	}

	*done = (val == 3 ? true : false);
	dev_dbg(info->dev, "%s done:%d\n", __func__, *done);

	return ret;
}

//BSP:add shipmode interface by peihuang 20240401 start
static int __maybe_unused sc8950_set_ship_mode(struct charger_device *chg_dev, bool on)
{
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);
	dev_info(info->dev, "%s on:%d\n", __func__, on);
	sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL3, on, SC8950_REG_SYSTEM_CONTROL3_BATFET_DLY_MASK, SC8950_REG_SYSTEM_CONTROL3_BATFET_DLY_SHIFT);
	mdelay(10);
	sc8950_update_bits(info, SC8950_REG_SYSTEM_CONTROL3, on, SC8950_REG_SYSTEM_CONTROL3_BATFET_DIS_MASK, SC8950_REG_SYSTEM_CONTROL3_BATFET_DIS_SHIFT);
	info->shipmode_en = on;

	return 0;
}

static int __maybe_unused sc8950_get_shipmode_status(struct charger_device *chg_dev)
{
	unsigned char value = 0;
	struct sc8950_info *info = dev_get_drvdata(&chg_dev->dev);

	sc8950_read_interface(info, SC8950_REG_SYSTEM_CONTROL3,
		&value, SC8950_REG_SYSTEM_CONTROL3_BATFET_DIS_MASK, SC8950_REG_SYSTEM_CONTROL3_BATFET_DIS_SHIFT);

	return (value || info->shipmode_en);
}
//BSP:add shipmode interface by peihuang 20240401 end

static bool sc8950_check_devinfo(struct sc8950_info *info)
{
	unsigned char devinfo = 0;
	int ret = 0;

	ret = sc8950_get_pd(info, &devinfo);
	if (ret < 0) {
		dev_notice(info->dev, "%s get_pd fail(%d)\n", __func__, ret);
		return false;
	}

	switch(devinfo) {
		case SC8950_VENDOR_ID:
			info->vendor_id = SC8950_VENDOR_ID;
			break;
		default:
			info->vendor_id = SC8950_VENDOR_ID_UNKNOW;
			return false;
	};

	dev_dbg(info->dev, "%s chip id is 0x%x\n", __func__, devinfo);

	return true;
}
/************** SC8950 Charger Ops Operation ***************/

static int sc8950_parse_dt(struct sc8950_info *info,  struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0;
	int val = 0;

	dev_dbg(info->dev, "%s\n", __func__);
	if (!np) {
		dev_info(info->dev, "%s: no of node\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_string(np, "chg_name", &info->chg_name);
	if (ret < 0) {
		info->chg_name="primary_chg";
		dev_err(info->dev, "%s no chg_name(%d)\n", __func__, ret);
	}

	ret = of_property_read_string(np, "chg_alias_name", &(info->chg_props.alias_name));
	if (ret < 0) {
               info->chg_props.alias_name = "sc8950";
               pr_info("%s: no alias name\n", __func__);
       }

	info->intr_gpio = of_get_named_gpio(np, "sc8950,intr_gpio", 0);
	if(info->intr_gpio < 0){
		info->intr_gpio = U32_MAX;
		pr_info("%s irq_gpio=%d get fail\n", __func__, info->intr_gpio);
	}

	if (of_property_read_u32(np, "ircmp_resistor", &val) >= 0)
		info->ircmp_resistor = val;
	else {
		chr_err("use default ircmp_resistor:%d\n", SC8950_BAT_COMP_DEFAULT);
		info->ircmp_resistor = SC8950_BAT_COMP_DEFAULT;
	}

	if (of_property_read_u32(np, "ircmp_vclamp", &val) >= 0)
		info->ircmp_vclamp = val;
	else {
		chr_err("use default ircmp_vclamp:%d\n", SC8950_VCLAMP_DEFAULT);
		info->ircmp_vclamp = SC8950_VCLAMP_DEFAULT;
	}

	info->chg_en_by_gpio = of_property_read_bool(np, "chg_en_by_gpio");
	if (info->chg_en_by_gpio) {
		info->en_gpio = of_get_named_gpio(np, "sc8950,en_gpio", 0);
		if (info->en_gpio < 0){
			info->en_gpio = U32_MAX;
			pr_info("%s en_gpio=%d get fail\n", __func__, info->en_gpio);
		} else {
			ret = gpio_request(info->en_gpio, "sc8950 chg en pin");
			if (ret) {
				pr_err("%s: en_gpio request failed\n", __func__);
			} else {
				gpio_direction_output(info->en_gpio, GPIO_CHG_DISABLE);
			}
		}
	}
//abandon sc8950 irq register for sc8950 as slave charger by xu.liu 20250317 start
	info->slave_chg_abandon_irq = of_property_read_bool(np, "slave_chg_abandon_irq");
//abandon sc8950 irq register for sc8950 as slave charger by xu.liu 20250317 end
	info->dual_chg_as_slave = of_property_read_bool(np, "dual_chg_as_slave");

	return 0;
}

static int sc8950_hw_init(struct sc8950_info *info)
{
	unsigned char regval = 0;
	dev_info(info->dev, "%s", __func__);

	sc8950_set_rst(info, 0x1);				/* reset chip register */
	sc8950_set_ico(info, 0x0);				/* disable ico */
	sc8950_set_hvdcp(info, 0x0);				/* disable hvdcp handshake */
	sc8950_set_autodpdm(info, 0x0);			/* disable auto dp/dm detection */
	sc8950_set_hiz(info, 0x0);				/* disable hiz */
	sc8950_set_force_vindpm(info, 0x14);		/* VIN DPM check 4.6V */
	sc8950_set_wdt_rst(info, 0x1);			/* Kick watchdog */
	sc8950_set_vsys_min(info, 0x5);			/* Minimum system voltage 3.5V */
	sc8950_set_iprechg(info, 0x7);			/* Precharge current 480mA */
	sc8950_set_iterm(info, 0x3);				/* Termination current 180mA */

	if (info->dual_chg_as_slave)
		sc8950_set_vreg(info, 0x2B);				/* VREG 4.528V for 18W fast charge when sc8950 is slave */
	else
		sc8950_set_vreg(info, 0x23);				/* VREG 4.4V */

	sc8950_set_statdis(info, 0x0);				/* enable statdis */
	sc8950_set_vbatlow_otg(info, 0x0);		/* minimum otg vbat 2.8V */
	sc8950_set_vrechg(info, 0x0);				/* VRECHG 0.1V */
	sc8950_set_en_term(info, 0x1);			/* Enable termination */
	sc8950_set_watchdog_cfg(info, 0x0);		/* disable WDT */
	sc8950_set_en_timer(info, 0x1);			/* Enable charge timer */
	sc8950_set_tchg(info, 0x3);				/* charge time 20 hours */
	sc8950_set_batfet_reset_en(info, 0x0);	/* disable batfet rst */
#if IS_ENABLED(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT) || IS_ENABLED(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT)
	sc8950_set_ovp(info, 0x3);				/* set ovp to 14.5v */
	sc8950_set_chg_thermal(info, 0x3);		/* set thermal to 120 */
#endif
	sc8950_set_boostv(info,0xe);				/* set vboost 5.3v*/
	sc8950_set_ircmp_resistor(info, info->ircmp_resistor); /*set bat_comp*/
	sc8950_set_ircmp_vclamp(info, info->ircmp_vclamp);	/*set vclamp*/
	sc8950_set_en_charge(info, 0x0);

	//sc8950_set_private(info);				/* Loop optimization */
	sc8950_set_test_mode_7f(info, true);
	sc8950_read_byte(info, 0x88, &regval);
	regval &= ~(0x01 << 7);
	sc8950_write_byte(info , 0x88, regval);
	sc8950_write_byte(info , 0xD9, 0x08);
	sc8950_set_test_mode_7f(info, false);
	info->boost_mode = false;
	info->vg_skip = false;
	return 0;
}

static struct charger_ops sc8950_chg_ops = {
	/* cable plug in/out */
	.plug_in = sc8950_plug_in,
	.plug_out = sc8950_plug_out,

	/* enable/disable charger */
	.enable = sc8950_enable_charging,
	.is_enabled = sc8950_is_charging_enabled,

	/* get/set charging current*/
	.get_charging_current = sc8950_get_ichg,
	.set_charging_current = sc8950_set_ichg,
	.get_min_charging_current = sc8950_get_min_ichg,

	/* set cv */
	.get_constant_voltage = sc8950_get_cv,
	.set_constant_voltage = sc8950_set_cv,

	/* set input_current */
	.get_input_current = sc8950_get_aicr,
	.set_input_current = sc8950_set_aicr,
	.get_min_input_current = sc8950_get_min_aicr,

	/* set termination current */
	.get_eoc_current = sc8950_get_ieoc,
	.set_eoc_current = sc8950_set_ieoc,

	/* kick wdt */
	.kick_wdt = sc8950_kick_wdt,

	.event = sc8950_event,

	.set_mivr = sc8950_set_mivr,
	.get_mivr = sc8950_get_mivr,
	.get_mivr_state = sc8950_get_mivr_state,
	//.get_aicr_state = sc8950_get_aicr_state,

	/* enable/disable chip */
	.enable_chip = sc8950_enable_powerpath,
	.is_chip_enabled = sc8950_is_powerpath_enabled,

	/* enable/disable powerpath */
	.enable_powerpath = sc8950_enable_powerpath,
	.enable_hz = sc8950_en_hz,
	.is_powerpath_enabled = sc8950_is_powerpath_enabled,

	/* enable/disable charging safety timer */
	.enable_safety_timer = sc8950_enable_safety_timer,
	.is_safety_timer_enabled = sc8950_is_safety_timer_enabled,

	/* enable term */
	.enable_termination = sc8950_enable_te,

	/* OTG */
	.enable_otg = sc8950_enable_otg,
  	//.get_enable_otg = sc8950_get_en_otg,
	.enable_discharge = NULL,
	.set_boost_current_limit = sc8950_set_boost_current_limit,
	//.set_boost_voltage = sc8950_set_boost_voltage_limit,
	//.enable_dcp = sc8950_enable_dcp,
	//.get_boost_status = sc8950_get_boost_status,

	/* charger type detection */
	.enable_chg_type_det = sc8950_enable_chg_type_det,

	/* reset EOC state */
	.reset_eoc_state = NULL,

	.is_charging_done = sc8950_is_charging_done,

	.dump_registers = sc8950_dump_registers,

	.reset_ta = sc8950_reset_ta,

	//USD:add for X6861H963-494 by lei.shi5 20240425 end
	/* PE+20/HVDCP */
	.set_pe20_efficiency_table = sc8950_set_pep20_efficiency_table,
	.send_ta20_current_pattern = sc8950_set_pep20_current_pattern,
};

static int sc8950_init_chg(struct sc8950_info *info)
{
	info->chg_dev = charger_device_register(info->chg_name,
			info->dev, info, &sc8950_chg_ops, &info->chg_props);
	if (!info->chg_dev->dev.class || IS_ERR_OR_NULL(info->chg_dev))
		return -EPROBE_DEFER;

	return 0;
}

static int sc8950_eoc_irq_handler(struct sc8950_info *info)
{
	bool is_done = false;

	sc8950_is_charging_done(info->chg_dev, &is_done);
	if(is_done) {
		dev_err(info->dev, "%s happen!\n", __func__);
		charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_EOC);
	}

	return 0;
}

static int sc8950_vg_irq_handler(struct sc8950_info *info)
{
	unsigned char vg_stat = 0;
	int ret;
	bool prev_vbus_gd;
	unsigned char true_power_good = 0;
	ret = sc8950_get_vg_stat(info, &vg_stat);
	ret = sc8950_check_bc12_done(info, &true_power_good);
	if (ret) {
		pr_info("%s read dail ret:%d\n", __func__, ret);
		return IRQ_HANDLED;
	}
	pr_info("%s vg_stat:%d true_power_good:%d\n", __func__, vg_stat,true_power_good);
	prev_vbus_gd = info->vbus_good;
	info->vbus_good = ! !vg_stat;

	if(info->vg_skip){
		pr_info("%s vg_skip is true\n", __func__);
	}

	if(!prev_vbus_gd && info->vbus_good && !info->vg_skip){
		info->vg_skip = true;
		info->first_plugin = true;
		cancel_delayed_work(&info->vindpm_force_work);
		flush_delayed_work(&info->vindpm_force_work);
		schedule_delayed_work(&info->vindpm_force_work, 1500);
	} else if (prev_vbus_gd && !info->vbus_good) {
		info->first_plugin = false;
	}

	return 0;
}

static int sc8950_fault_irq_handler(struct sc8950_info *info)
{
	unsigned char charg_fault = 0, tmp = 0;
	struct chgdev_notify *noti = &(info->chg_dev->noti);
	unsigned char is_otg_en = 0;

	sc8950_check_fault_state(info, &charg_fault);

	if (charg_fault & SC8950_WATCHDOG_FAULT_MASK) {
		dev_err(info->dev, "%s Watchdog timer expiration!reg[c]=0x%x\n", __func__, charg_fault);
	}

	if (charg_fault & SC8950_BOOST_FAULT_MASK) {
		sc8950_get_en_otg(info->chg_dev, &is_otg_en);
		if (is_otg_en == 0) {
			dev_err(info->dev, "%s vbus ovp!reg[c]=0x%x\n", __func__, charg_fault);
			info->vbus_stat_pre = true;
			noti->vbusov_stat = SC8950_BOOST_FAULT_VBUS_OVP;
			charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
			//charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_OTG_FAULT);
		} else {
			dev_err(info->dev, "%s vbus ocp!reg[c]=0x%x\n", __func__, charg_fault);
		}
	} else if (info->vbus_stat_pre == true) {
		info->vbus_stat_pre = false;
		noti->vbusov_stat = SC8950_BOOST_FAULT_NORMAL;
		charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
	}

	if (charg_fault & SC8950_CHRG_FAULT_MASK) {
		tmp = ((charg_fault & SC8950_CHRG_FAULT_MASK) >> SC8950_CHRG_FAULT_SHIFT);
		if (tmp == SC8950_CHRG_FAULT_INPUT_FAULT) {
			dev_err(info->dev, "%s vac ovp!reg[c]=0x%x\n", __func__, charg_fault);
			info->vbus_stat_pre = true;
			noti->vbusov_stat = SC8950_BOOST_FAULT_VBUS_OVP;
			charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
		} else if (tmp == SC8950_CHRG_FAULT_THERMAL_SHUTDOWN) {
			dev_err(info->dev, "%s thermal shutdown!reg[c]=0x%x\n", __func__, charg_fault);
		} else if (tmp == SC8950_CHRG_FAULT_SAFETY_TIMER_EXPIRATION) {
			dev_err(info->dev, "%s safety timer expiration!reg[c]=0x%x\n", __func__, charg_fault);
		}
	} else {
		tmp = ((charg_fault & SC8950_CHRG_FAULT_MASK) >> SC8950_CHRG_FAULT_SHIFT);
		if (tmp == SC8950_CHRG_FAULT_NORMAL && info->vbus_stat_pre) {
			info->vbus_stat_pre = false;
			noti->vbusov_stat = SC8950_BOOST_FAULT_NORMAL;
			charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
		}
	}

	if (charg_fault & SC8950_BAT_FAULT_MASK) {
		dev_err(info->dev, "%s bat ovp!reg[c]=0x%x\n", __func__, charg_fault);
	}

	if (charg_fault & SC8950_NTC_FAULT_MASK) {
		tmp = ((charg_fault & SC8950_NTC_FAULT_MASK) >> SC8950_NTC_FAULT_SHIFT);
		if (tmp == SC8950_NTC_FAULT_WARM) {
			dev_err(info->dev, "%s ntc warm!reg[c]=0x%x\n", __func__, charg_fault);
		} else if (tmp == SC8950_NTC_FAULT_COOL) {
			dev_err(info->dev, "%s ntc cool !reg[c]=0x%x\n", __func__, charg_fault);
		} else if (tmp == SC8950_NTC_FAULT_COLD) {
			dev_err(info->dev, "%s ntc cold !reg[c]=0x%x\n", __func__, charg_fault);
		} else if (tmp == SC8950_NTC_FAULT_HOT) {
			dev_err(info->dev, "%s ntc hot !reg[c]=0x%x\n", __func__, charg_fault);
		}
	}

	return 0;
}

static const struct irq_mapping_tbl sc8950_irq_mapping_tbl[] = {
	SC8950_IRQ_MAPPING(vg),
	SC8950_IRQ_MAPPING(eoc),
	SC8950_IRQ_MAPPING(fault),
};

static irqreturn_t __maybe_unused sc8950_irq_pre_handler(int irq, void *data)
{
        struct sc8950_info *info = (struct sc8950_info *)data;
	if(! info->force_dpdm) {
		return IRQ_WAKE_THREAD;
	}
	info->cnt++;
        return IRQ_HANDLED;
}

void sc8950_work_handler(struct work_struct *work)
{
	int i = 0;
	struct sc8950_info *info = container_of(work, struct sc8950_info, sc8950_work);

	disable_irq(info->irq);

	dev_dbg(info->dev, "%s\n", __func__);
	
	for (i = 0; i < ARRAY_SIZE(sc8950_irq_mapping_tbl); i++){
		sc8950_irq_mapping_tbl[i].hdlr(info);
	}

	enable_irq(info->irq);
}

static irqreturn_t __maybe_unused sc8950_irq_handler(int irq, void *data)
{
        struct sc8950_info *info = (struct sc8950_info *)data;

        dev_dbg(info->dev, "%s\n", __func__);
        queue_work(info->sc8950_wq, &info->sc8950_work);

        return 0;
}

/* ======================= */
/* sc8950 Power Supply Ops */
/* ======================= */

static int sc8950_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct sc8950_info *info = power_supply_get_drvdata(psy);
	//BSP_CHG:add for X6853H895-3 by lei.shi5 20230914 start
#if IS_ENABLED(CONFIG_WIRELESS_TA) || IS_ENABLED(CONFIG_TRAN_WIRELESS_TA)
	struct power_supply *wireless_psy;
	union power_supply_propval prop;
#endif
	int ret=0;
	dev_dbg(info->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = SC8950_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
#if IS_ENABLED(CONFIG_WIRELESS_TA) || IS_ENABLED(CONFIG_TRAN_WIRELESS_TA)
		wireless_psy = power_supply_get_by_name("wireless");
		if (IS_ERR_OR_NULL(wireless_psy)) {
			dev_err(info->dev,"%s: get wireless power supply failed\n", __func__);
			return -EINVAL;
		}
		ret = power_supply_get_property(wireless_psy,POWER_SUPPLY_PROP_ONLINE, &prop);
		if(prop.intval)
			val->intval=prop.intval;
		else
#endif
		//BSP_CHG:add for X6853H895-3 by lei.shi5 20230914 end
		val->intval = atomic_read(&info->attach);
		//USD:del note by jie.hu 20240715 start
		dev_dbg(info->dev, "%s: online = %d\n", __func__,val->intval);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = info->psy_desc.type;
		dev_dbg(info->dev, "%s: psy desc type = %d\n", __func__,val->intval);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = info->psy_usb_type;
		dev_dbg(info->dev, "%s: psy usb type = %d\n", __func__,val->intval);
		//USD:del note by jie.hu 20240715 start
		break;
	case POWER_SUPPLY_PROP_STATUS:
		sc8950_get_ic_stat(info,&(val->intval));
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (info->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (info->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sc8950_get_ichg(info->chg_dev, &(val->intval));
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sc8950_get_cv(info->chg_dev, &(val->intval));
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sc8950_get_aicr(info->chg_dev, &(val->intval));
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = sc8950_get_ieoc(info->chg_dev, &(val->intval));
		break;
	default:
		ret = -ENODATA;
		break;
	}
	return ret;
}

static int sc8950_charger_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	struct sc8950_info *info = power_supply_get_drvdata(psy);
	int ret;
//USD:del note by jie.hu 20240715 start
	dev_dbg(info->dev, "%s: prop = %d\n", __func__, psp);
//USD:del note by jie.hu 20240715 end
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = sc8950_chg_attach_process(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sc8950_set_ichg(info->chg_dev, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sc8950_set_cv(info->chg_dev, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sc8950_set_aicr(info->chg_dev, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sc8950_set_mivr(info->chg_dev, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = sc8950_set_ieoc(info->chg_dev, val->intval);
		break;
	//USD:del note by jie.hu 20240715 start
#if IS_ENABLED(CONFIG_WIRELESS_TA) || IS_ENABLED(CONFIG_TRAN_WIRELESS_TA)
	case POWER_SUPPLY_PROP_TYPE:
		info->psy_desc.type=val->intval;
		dev_dbg(info->dev,"%s:type=%d\n",__func__,info->psy_desc.type);
		ret=0;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		info->psy_usb_type=val->intval;
		dev_dbg(info->dev,"%s:psy_usb_type=%d\n",__func__,info->psy_usb_type);
		ret=0;
		break;
#endif
	//USD:del note by jie.hu 20240715 start
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int sc8950_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property sc8950_charger_properties[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static const struct power_supply_desc sc8950_charger_desc = {
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= sc8950_charger_properties,
	.num_properties		= ARRAY_SIZE(sc8950_charger_properties),
	.get_property		= sc8950_charger_get_property,
	.set_property		= sc8950_charger_set_property,
	.property_is_writeable	= sc8950_charger_property_is_writeable,
	.usb_types =  BIT(POWER_SUPPLY_USB_TYPE_UNKNOWN) |
				  BIT(POWER_SUPPLY_USB_TYPE_SDP) |
				  BIT(POWER_SUPPLY_USB_TYPE_CDP) |
				  BIT(POWER_SUPPLY_USB_TYPE_DCP),
};

static char *sc8950_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};

/*regulator otg ops*/
int __maybe_unused sc8950_enable_regulator_otg(struct regulator_dev *rdev)
{
	struct sc8950_info *info = rdev_get_drvdata(rdev);

	dev_dbg(info->dev, "%s\n", __func__);
	/* otg current to 1.2A */
	return sc8950_enable_otg(info->chg_dev, true);
}

int  __maybe_unused sc8950_disable_regulator_otg(struct regulator_dev *rdev)
{
	struct sc8950_info *info = rdev_get_drvdata(rdev);

	dev_dbg(info->dev, "%s\n", __func__);

	return sc8950_enable_otg(info->chg_dev, false);
}

static int sc8950_boost_set_voltage_sel(struct regulator_dev *rdev,
					unsigned int sel)
{
	unsigned char regval=0;
	struct sc8950_info *info = rdev_get_drvdata(rdev);

	regval=sc8950_vboost_toreg(sel);
	dev_dbg(info->dev, "%s: sel=%d regval=%d\n",__func__,sel,regval);
	return sc8950_set_boostv(info,regval);
}

static int sc8950_boost_get_voltage_sel(struct regulator_dev *rdev)
{
	struct sc8950_info *info = rdev_get_drvdata(rdev);
	unsigned char val=0;

	sc8950_get_boostv(info,&val);
	dev_dbg(info->dev, "%s: val=%d\n",__func__,val);
	return val;
}

static int sc8950_boost_set_current_limit(struct regulator_dev *rdev,
						int min_uA, int max_uA)
{
	struct sc8950_info *info = rdev_get_drvdata(rdev);
	int i;

	for (i = 0; i < ARRAY_SIZE(sc8950_otg_oc_threshold); i++) {
		if (min_uA <= sc8950_otg_oc_threshold[i])
			break;
	}
	if (i == ARRAY_SIZE(sc8950_otg_oc_threshold) ||
		sc8950_otg_oc_threshold[i] > max_uA) {
		dev_notice(info->dev,"%s: out of current range\n", __func__);
		return -EINVAL;
	}
	dev_dbg(info->dev, "%s: select otg_oc = %d\n",__func__, sc8950_otg_oc_threshold[i]);
	return sc8950_set_iboost(info,i);
}

static int sc8950_boost_get_current_limit(struct regulator_dev *rdev)
{
	int ret = 0;
	struct sc8950_info *info = rdev_get_drvdata(rdev);
	unsigned char regval = 0;

	ret = sc8950_get_iboost(info,&regval);
	if (ret < 0)
		return ret;

	if (regval >= ARRAY_SIZE(sc8950_otg_oc_threshold))
		return -EINVAL;

	dev_dbg(info->dev, "%s: regval=%d get otg_oc = %d\n",__func__,regval,sc8950_otg_oc_threshold[regval]);
	return sc8950_otg_oc_threshold[regval];
}

static ssize_t sc8950_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sc8950_info *info = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8950");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = sc8950_read_byte(info, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
					"Reg[%.2X] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t sc8950_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sc8950_info *info = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2) {
		sc8950_write_byte(info, (unsigned char)reg, (unsigned char)val);
	}

	return count;
}

static DEVICE_ATTR(sc8950_registers_debug, 0664, sc8950_show_registers, sc8950_store_register);

static int __maybe_unused sc8950_otg_is_enabled(struct regulator_dev *rdev)
{
	struct sc8950_info *info = rdev_get_drvdata(rdev);
	dev_dbg(info->dev, "%s  is_en:%d\n", __func__, info->boost_mode);
	return info->boost_mode;
}

static const struct regulator_ops sc8950_chg_otg_ops = {
	.list_voltage = regulator_list_voltage_linear,
/*
	.enable = sc8950_enable_regulator_otg,
	.disable = sc8950_disable_regulator_otg,
	.is_enabled = sc8950_otg_is_enabled,
*/
	.set_voltage_sel = sc8950_boost_set_voltage_sel,
	.get_voltage_sel = sc8950_boost_get_voltage_sel,
	.set_current_limit = sc8950_boost_set_current_limit,
	.get_current_limit = sc8950_boost_get_current_limit,
};

static const struct regulator_desc sc8950_otg_rdesc = {
	.of_match = "usb-otg-vbus",
	.name = "usb-otg-vbus",
	.ops = &sc8950_chg_otg_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.min_uV = 3900000,
	.uV_step = 100000, /* step  100mV */
	.n_voltages = 15, /* 3900mV to 5400mV */
	.linear_min_sel = 0,
};

static const struct regulator_init_data sc8950_vbus_init_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS|REGULATOR_CHANGE_CURRENT,
		.min_uA = 500000,
		.max_uA = 2450000,
	},
};

static int sc8950_init_regulator(struct sc8950_info *info)
{
	struct regulator_config config = { };
	int ret=0;
	dev_dbg(info->dev, "%s\n", __func__);

	config.dev = info->dev;
	config.driver_data = info;
	config.init_data = &sc8950_vbus_init_data;
	info->otg_rdev = devm_regulator_register(info->dev, &sc8950_otg_rdesc,&config);

	if (IS_ERR(info->otg_rdev)) {
		ret = PTR_ERR(info->otg_rdev);
		dev_info(info->dev, "%s fail (%d)!\n", __func__,ret);
		return ret;
	}
	dev_dbg(info->dev, "%s succesfully!\n", __func__);
	return 0;
}

static int sc8950_init_psy(struct sc8950_info *info)
{
	struct power_supply_config charger_cfg = {};

	dev_dbg(info->dev, "%s\n", __func__);

	info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	/* power supply register */
	memcpy(&info->psy_desc,&sc8950_charger_desc, sizeof(info->psy_desc));
	info->psy_desc.name = "primary_chg";

	charger_cfg.drv_data = info;
	charger_cfg.of_node = info->dev->of_node;
	charger_cfg.supplied_to = sc8950_charger_supplied_to;
	charger_cfg.num_supplicants = ARRAY_SIZE(sc8950_charger_supplied_to);
	info->psy = devm_power_supply_register(info->dev,
					&info->psy_desc, &charger_cfg);
	return IS_ERR_OR_NULL(info->psy);
}

static int sc8950_probe(struct i2c_client *client)
{
	struct sc8950_info *info = NULL;
	int ret = 0;
	
	dev_dbg(&client->dev, "%s\n", __func__);

	info = devm_kzalloc(&client->dev, sizeof(struct sc8950_info), GFP_KERNEL);
	if (!info) {
		dev_info(&client->dev, "%s alloc sc8950_chip failed!\n", __func__);
		return -ENOMEM;
	}

	info->client = client;
	info->dev = &client->dev;
	i2c_set_clientdata(client, info);

	mutex_init(&info->sc8950_i2c_access);
	mutex_init(&info->attach_lock);
	atomic_set(&info->attach, 0);
	atomic_set(&info->vbus_gd, false);
	atomic_set(&info->pd_set_type, false);
	//USD:add for X6861H963-494 by lei.shi5 20240425 start
	info->ffc_sw_eoc = false;
	//USD:add for X6861H963-494 by lei.shi5 20240425 end
	info->force_dpdm = false;
	if(sc8950_parse_dt(info, &client->dev) < 0){
		dev_err(&client->dev, "%s parse dt failed!\n", __func__);
		goto err_parse_dt;
	}

	if(!sc8950_check_devinfo(info)) {
		dev_err(&client->dev, "%s sc8950 ic is not exist!\n", __func__);
		ret = -ENODEV;
		goto err_parse_dt;
	}

	sc8950_hw_init(info);

	/* Register charger device */
	ret = sc8950_init_chg(info);
	if (ret) {
		ret = -EPROBE_DEFER;
		pr_info("%s: register charger device  failed\n", __func__);
		goto err_register_chg_dev;
	}

	INIT_WORK(&info->bc12_work, sc8950_bc12_detect);
	INIT_DELAYED_WORK(&info->vindpm_force_work, sc8950_vindpm_force);

	ret = sc8950_init_regulator(info);
	if (ret<0) {
		ret = PTR_ERR(info->otg_rdev);
		dev_notice(info->dev, "%s regulator register fail\n", __func__);
		goto err_register_irq;
	}
	ret = sc8950_init_psy(info);
	if (ret < 0) {
		ret = PTR_ERR(info->psy);
		dev_err(info->dev,"Fail to register power supply dev, is NULL = %d\n",(info->psy == NULL));
		goto err_register_irq;
	}

	device_create_file(info->dev, &dev_attr_sc8950_registers_debug);

	__sc8950_dump_registers(info);

	dev_err(info->dev, "sc8950 probe successfully!\n");

	return 0;

err_register_irq:
	charger_device_unregister(info->chg_dev);
err_register_chg_dev:
err_parse_dt:
	mutex_destroy(&info->sc8950_i2c_access);
	mutex_destroy(&info->attach_lock);
	devm_kfree(info->dev, info);
	return ret;
}

static void sc8950_remove(struct i2c_client *client)
{
	struct sc8950_info *info = i2c_get_clientdata(client);

	if(!info)
		return;

	if (info->psy)
		power_supply_put(info->psy);

	charger_device_unregister(info->chg_dev);
	mutex_destroy(&info->sc8950_i2c_access);
	mutex_destroy(&info->attach_lock);
	dev_dbg(info->dev, "%s end\n", __func__);
	return;
}

static void sc8950_shutdown(struct i2c_client *client)
{
	struct sc8950_info *info = i2c_get_clientdata(client);

	if(!info)
		return;

	sc8950_set_rst(info, 0x1);
	atomic_set(&info->attach, 0);
	i2c_set_clientdata(client, NULL);
	dev_dbg(info->dev, "%s end\n", __func__);
	return;
}

static int sc8950_suspend(struct device *dev)
{
	return 0;
}

static int sc8950_resume(struct device *dev)
{
	return 0;
}
static SIMPLE_DEV_PM_OPS(sc8950_pm_ops, sc8950_suspend, sc8950_resume);

static const struct of_device_id sc8950_of_device_id[] = {
	{ .compatible = "southchip,sc8950", },
	{ },
};
MODULE_DEVICE_TABLE(of, sc8950_of_device_id);

static struct i2c_driver sc8950_i2c_driver = {
	.driver = {
		.name = "sc8950",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sc8950_of_device_id),
		.pm = &sc8950_pm_ops,
	},
	.probe = sc8950_probe,
	.remove = sc8950_remove,
	.shutdown = sc8950_shutdown,
};
module_i2c_driver(sc8950_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SC8950 Charger Driver");

