// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Transsion Inc.
 */
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include "tc_hvdcp20.h"
#include "tc_charger_class.h"
#include "tc_charger.h"

struct hvdcp20_hal {
	struct charger_device *chg1_dev;
	struct charger_device *chg2_dev;
	struct tran_device *usb_control_dev;
};

int hvdcp20_get_chg_dev_info(struct tchg_alg_device *alg,
		enum tran_common_prop prop, int *value)
{
	int ret = 0;
	union com_propval val = {0, };
	struct tran_device *tc_chg_dev = NULL;

	tc_chg_dev = tran_get_by_name("tc_charger");
	ret = tran_dev_get_prop(tc_chg_dev, prop, &val);
	if (ret < 0) {
		hvdcp20_err("%s: get prop(%d) failed(%d)\n", __func__, prop, ret);
		return ret;
	}
	*value = val.intval;
	return ret;

}

int hvdcp20_hal_init_hardware(struct tchg_alg_device *alg)
{
	struct tc_hvdcp20 *hvdcp20;
	struct hvdcp20_hal *hal;

	hvdcp20_dbg("%s\n", __func__);
	if (alg == NULL) {
		hvdcp20_err("%s: alg is null\n", __func__);
		return -EINVAL;
	}

	hvdcp20 = dev_get_drvdata(&alg->dev);
	hal = tchg_alg_dev_get_drv_hal_data(alg);
	if (hal == NULL) {
		hal = devm_kzalloc(&hvdcp20->pdev->dev, sizeof(*hal), GFP_KERNEL);
		if (!hal)
			return -ENOMEM;
		tchg_alg_dev_set_drv_hal_data(alg, hal);
	}

	hal->chg1_dev = get_charger_by_name("primary_chg");
	if (hal->chg1_dev)
		hvdcp20_dbg("%s: Found primary charger\n", __func__);
	else {
		hvdcp20_err("%s: Error : can't find primary charger\n",
			__func__);
		return -ENODEV;
	}

	hal->chg2_dev = get_charger_by_name("secondary_chg");
	if (hal->chg2_dev)
		hvdcp20_dbg("%s: Found secondary charger\n", __func__);
	else
		hvdcp20_err("%s: Error : can't find secondary charger\n",
			__func__);

	return 0;
}

int hvdcp20_hal_get_uisoc(struct tchg_alg_device *alg)
{
	return tc_get_uisoc();
}

int hvdcp20_hal_get_charger_type(struct tchg_alg_device *alg)
{
	return tc_get_alias_type();
}

int hvdcp20_hal_set_mivr(struct tchg_alg_device *alg, bool enable, int uV)
{
	int ret = 0;
	struct votable *chg1_mivr_vote = NULL;

	chg1_mivr_vote = find_votable("chg1_mivr");
	if (chg1_mivr_vote == NULL)
		return -ENODEV;
	
	ret = vote(chg1_mivr_vote, HVDCP20_VOTER, enable, uV);
	if (ret < 0) {
		hvdcp20_err("hvdcp20 set mivr failed, ret = %d\n", ret);
	}

	return ret;
}

int hvdcp20_hal_get_charger_cnt(struct tchg_alg_device *alg)
{
	struct hvdcp20_hal *hal;
	int cnt = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = tchg_alg_dev_get_drv_hal_data(alg);
	if (hal->chg1_dev != NULL)
		cnt++;
	if (hal->chg2_dev != NULL)
		cnt++;

	return cnt;
}

int hvdcp20_hal_get_vbus(struct tchg_alg_device *alg)
{
	return tc_get_vbus() * 1000;
}

int hvdcp20_hal_get_vbat(struct tchg_alg_device *alg)
{
	return tc_get_battery_voltage() * 1000;
}

int hvdcp20_hal_get_ibat(struct tchg_alg_device *alg)
{
	return tc_get_battery_current() * 1000;
}

int hvdcp20_hal_get_mivr_state(struct tchg_alg_device *alg,
	enum tchg_idx chgidx, bool *in_loop)
{
	struct hvdcp20_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = tchg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		charger_dev_get_mivr_state(hal->chg1_dev, in_loop);
	else if (chgidx == CHG2 && hal->chg2_dev != NULL)
		charger_dev_get_mivr_state(hal->chg2_dev, in_loop);

	return 0;
}

int hvdcp20_hal_get_charger_current_limit(struct tchg_alg_device *alg)
{
	int value = DEFAULT_ICHG;
	struct votable *chg1_ichg_vote = NULL;
	struct votable *chg2_ichg_vote = NULL;

	chg1_ichg_vote = find_votable("chg1_ichg");
	if (chg1_ichg_vote == NULL)
		return -ENODEV;
	
	chg2_ichg_vote = find_votable("chg2_ichg");
	if (chg2_ichg_vote == NULL)
		return -ENODEV;
	
	value = get_effective_result(chg1_ichg_vote) +
		get_effective_result(chg2_ichg_vote);

	return value;
}

int hvdcp20_hal_set_input_current(struct tchg_alg_device *alg, bool enable, u32 uA)
{
	int ret = 0;
	struct votable *total_aicr_vote = NULL;

	total_aicr_vote = find_votable("total_aicr");
	if (total_aicr_vote == NULL)
		return -ENODEV;
	
	ret = vote(total_aicr_vote, HVDCP20_VOTER, enable, uA);
	if (ret < 0) {
		hvdcp20_err("hvdcp20 set aicr failed, ret = %d\n", ret);
	}

	return ret;
}

int hvdcp20_hal_get_input_current(struct tchg_alg_device *alg,
	enum tchg_idx chgidx, u32 *uA)
{
	struct hvdcp20_hal *hal;
	int ret;

	if (alg == NULL)
		return -EINVAL;

	hal = tchg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		ret = charger_dev_get_input_current(hal->chg1_dev, uA);
	if (chgidx == CHG2 && hal->chg2_dev != NULL)
		ret = charger_dev_get_input_current(hal->chg2_dev, uA);
	pr_notice("%s idx:%d %d\n", __func__, chgidx, *uA);
	return 0;
}

int hvdcp20_hal_enable_vbus_ovp(struct tchg_alg_device *alg, bool enable)
{
	enum charger_voltage_max idx;

	idx = CHARGER_VOLTAGE_SWITCH_FC;

	tc_chg_switch_vbus_ovp(idx, HVDCP20_VOTER, !enable);

	return 0;
}

int hvdcp20_hal_get_log_level(struct tchg_alg_device *alg)
{
	int value = HVDCP20_DEBUG_LEVEL;

	hvdcp20_get_chg_dev_info(alg, TRAN_PROP_LOG_LEVEL, &value);

	return value;
}

int ta_dev_set_usb_ctrl_hvdcp20(struct tchg_alg_device *alg, int val)
{
        struct hvdcp20_hal *hal;
        union com_propval tran_val = {0, };

        if (alg == NULL)
                return -EINVAL;

        hal = tchg_alg_dev_get_drv_hal_data(alg);

        if (!hal->usb_control_dev)
                hal->usb_control_dev = tran_get_by_name("usb_control");

        if (IS_ERR_OR_NULL(hal->usb_control_dev))
		hvdcp20_err("usb_control device not found\n");

        tran_val.intval = val;

        return tran_dev_set_prop(hal->usb_control_dev,
                TRAN_PROP_USB_CTRL_HVDCP20, &tran_val);
}

