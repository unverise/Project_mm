// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Transsion Inc.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/reboot.h>

#include "tc_hvdcp20.h"
#include "tc_algorithm_class.h"
#include "tc_common_class.h"
#include "tc_misc_intf.h"


static char *hvdcp20_state_to_str(int state)
{
	switch (state) {
	case ALG_INIT_FAIL:
		return "ALG_INIT_FAIL";
	case ALG_TA_CHECKING:
		return "ALG_TA_CHECKING";
	case ALG_TA_NOT_SUPPORT:
		return "ALG_TA_NOT_SUPPORT";
	case ALG_NOT_READY:
		return "ALG_NOT_READY";
	case ALG_READY:
		return "ALG_READY";
	case ALG_RUNNING:
		return "ALG_RUNNING";
	case ALG_DONE:
		return "ALG_DONE";
	default:
		break;
	}
	hvdcp20_err("%s unknown state:%d\n", __func__, state);

	return "HVDCP20_UNKNOWN";
}

static int hvdcp20_reset_ta_none_check_volt(struct tchg_alg_device *alg);
static int hvdcp20_info_level = HVDCP20_DEBUG_LEVEL;

int hvdcp20_get_debug_level(void)
{
	return hvdcp20_info_level;
}

static void hvdcp20_report_charing_animation(struct tchg_alg_device *alg)
{
	struct tran_device *dev = NULL;
	union com_propval prop = {.intval = 0};

	dev = tran_get_by_name("tc_charger");
	if (IS_ERR_OR_NULL(dev)) {
		pr_err("get tc_charger dev fail\n");
		return;
	}

	tran_dev_set_prop(dev, TRAN_PROP_CHARGING_ANIMATION, &prop);

}

static int hvdcp20_plugout_reset(struct tchg_alg_device *alg)
{
	int ret = 0, cnt = 0;
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);
	u32 org_chr_volt_mivr = hvdcp20->chr_volt_mivr[HVDCP20_NONE];

	hvdcp20_info("%s\n", __func__);
	/* set flag to end hvdcp20 thread asap */
	mutex_lock(&hvdcp20->cable_out_lock);
	hvdcp20->is_cable_out_occur = true;
	mutex_unlock(&hvdcp20->cable_out_lock);

	/* wait thread end of run */
	while (mutex_trylock(&hvdcp20->access_lock) == 0 && cnt < 20) {
		hvdcp20_err("%s:hvdcp20 is running state: %s cnt:%d\n",
			__func__, hvdcp20_state_to_str(hvdcp20->state), cnt);
		cnt++;
		msleep(100);
	}
	/* reset cable out */
	mutex_lock(&hvdcp20->cable_out_lock);
	hvdcp20->is_cable_out_occur = false;
	mutex_unlock(&hvdcp20->cable_out_lock);

	/* Reset ta control */
	hvdcp20_reset_ta_none_check_volt(alg);

	/* Enable OVP */
	ret = hvdcp20_hal_enable_vbus_ovp(alg, true);
	if (ret < 0)
		hvdcp20_err("%s:enable vbus ovp fail, ret:%d\n",
			__func__, ret);

	/* Disable hvdcp20 mivr */
	ret = hvdcp20_hal_set_mivr(alg, false, org_chr_volt_mivr);
	if (ret < 0)
		hvdcp20_err("%s:set mivr fail, ret:%d\n",
			__func__, ret);

	hvdcp20->state = ALG_READY;
	hvdcp20->is_fast_chr = false;

	hvdcp20_info("%s: OK\n", __func__);

	mutex_unlock(&hvdcp20->access_lock);

	return 0;
}

int hvdcp20_reset_ta_vchr(struct tchg_alg_device *alg)
{
	int ret, chr_volt = 0, ret_value = -1;
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);
	u32 org_chr_volt = hvdcp20->chr_volt[HVDCP20_NONE];
	u32 org_chr_volt_mivr = hvdcp20->chr_volt_mivr[HVDCP20_NONE];

	hvdcp20_info("%s: starts\n", __func__);

	/* Reset TA's charging voltage */
	ret = ta_dev_set_usb_ctrl_hvdcp20(alg, HVDCP20_NONE);
	if (ret < 0)
		hvdcp20_err("%s:reset TA fail, ret:%d\n",
			__func__, ret);

	/* Check charger's voltage */
	chr_volt = hvdcp20_hal_get_vbus(alg);
	hvdcp20_info("%s: ori_vbus:%d vbus:%d\n", __func__,
		org_chr_volt, chr_volt);

	if (abs(chr_volt - org_chr_volt) <= 1000000) {
		ret_value = 0;
	}

	/* Reset MIVR */
	hvdcp20_hal_set_mivr(alg, false, org_chr_volt_mivr);

	hvdcp20_info("%s: OK\n", __func__);

	return ret;
}

static int hvdcp20_reset_ta_none_check_volt(struct tchg_alg_device *alg)
{
	int ret;
	/* struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev); */

	hvdcp20_info("%s: starts\n", __func__);

	/* Reset TA's charging voltage */
	ret = ta_dev_set_usb_ctrl_hvdcp20(alg, HVDCP20_NONE);
	if (ret < 0)
		hvdcp20_err("%s:reset TA fail, ret:%d\n",
			__func__, ret);

	hvdcp20_info("%s: OK\n", __func__);

	return ret;
}

static int _hvdcp20_set_ta_vchr(struct tchg_alg_device *alg, int target_volt)
{
	int ret = 0, ret_value = 0;
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);

	hvdcp20_info("%s: starts\n", __func__);

	/* Not to set chr volt if cable is plugged out */
	if (hvdcp20->is_cable_out_occur && target_volt != HVDCP20_NONE) {
		hvdcp20_err("%s: failed, cable out\n", __func__);
		return -ECABLEOUT;
	}

	hvdcp20->ta_vchr_target = target_volt;
	ret = ta_dev_set_usb_ctrl_hvdcp20(alg, target_volt);
	if (ret < 0) {
		hvdcp20_err("%s: hvdcp20 set target_volt:%d failed, ret = %d\n",
			__func__, target_volt, ret);
		return -EHAL;
	}

	return ret_value;
}

static int hvdcp20_set_ta_vchr(struct tchg_alg_device *alg, int target_volt)
{
	int ret = 0, ret_value = 0;
	int vchr_before, vchr_after, vchr_delta;
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);
	u32 chr_volt = hvdcp20->chr_volt[target_volt];
	u32 chr_volt_mivr = hvdcp20->chr_volt_mivr[target_volt];
	u32 org_chr_volt_mivr = hvdcp20->chr_volt_mivr[HVDCP20_NONE];

	hvdcp20_info("%s: starts, target:%d\n", __func__, chr_volt / 1000);

	hvdcp20_hal_set_input_current(alg, true, 1000000);
	hvdcp20_hal_set_mivr(alg, false, org_chr_volt_mivr);

	vchr_before = hvdcp20_hal_get_vbus(alg);
	ret = _hvdcp20_set_ta_vchr(alg, target_volt);
	vchr_after = hvdcp20_hal_get_vbus(alg);

	vchr_delta = abs(vchr_after - chr_volt);
	hvdcp20_err("ret:%d delta:%d %d %d\n",
		 ret, vchr_delta, vchr_after, chr_volt);
	/*
	 * It is successful if VBUS difference to target is
	 * less than 1V.
	 */
	if (vchr_delta < 1000000 && ret == 0) {
		hvdcp20_info("%s: OK, vchr = (%d, %d), vchr_target = %dmV\n", __func__,
			vchr_before / 1000, vchr_after / 1000, chr_volt / 1000);
		/* Set MIVR */
		hvdcp20_hal_set_mivr(alg, true, chr_volt_mivr);
		goto out;
	}

	hvdcp20_info("%s: vchr = (%d, %d), vchr_target = %dmV\n",
		__func__, vchr_before / 1000,
		vchr_after / 1000, chr_volt / 1000);


	if (hvdcp20->is_cable_out_occur)
		ret_value = -ECABLEOUT;
	else
		ret_value = -EHAL;

	hvdcp20_info("%s: failed, vchr_after = %dmV, target_vchr = %dmV, ret_value:%d\n",
		__func__, vchr_after / 1000, chr_volt / 1000, ret_value);

out:
	hvdcp20_hal_set_input_current(alg, false, 1000000);
	return ret_value;
}


static int hvdcp20_detect_ta(struct tchg_alg_device *alg)
{
	int ret = 0;
	/* struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev); */

	hvdcp20_info("%s: starts\n", __func__);

	/* Disable Normal OVP */
	ret = hvdcp20_hal_enable_vbus_ovp(alg, false);
	if (ret < 0)
		goto err;

	ret = hvdcp20_set_ta_vchr(alg, HVDCP20_9V);
	if (ret < 0) {
		hvdcp20_info("%s: set ta vchr failed, ret:%d\n",
			__func__, ret);
		goto err;
	}

	hvdcp20_info("%s: ret:%d\n", __func__, ret);

	return ret;
err:
	/* Enable Normal OVP */
	hvdcp20_hal_enable_vbus_ovp(alg, true);
	hvdcp20_err("%s: failed, ret = %d\n", __func__, ret);
	return ret;
}

static int __hvdcp20_check_charger(struct tchg_alg_device *alg)
{
	int ret = 0, ret_value = 0;
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);

	hvdcp20_info("%s\n", __func__);

	if (hvdcp20->is_cable_out_occur)
		goto out;

	ret = hvdcp20_reset_ta_none_check_volt(alg);
	if (ret != 0)
		goto out;

	if (hvdcp20->is_cable_out_occur)
		goto out;

	ret = hvdcp20_detect_ta(alg);
	if (ret < 0)
		goto out;

	hvdcp20->is_fast_chr = true;
	hvdcp20_report_charing_animation(alg);

	hvdcp20_info("%s: OK, state = %d\n",
		__func__, hvdcp20->state);

	return ret;
out:

	if (ret_value == 0)
		ret_value = ALG_TA_NOT_SUPPORT;

	/* Recover Normal state */
	hvdcp20_reset_ta_none_check_volt(alg);

	hvdcp20_info("%s: state:%s, ret:%d, plugout:%d\n", __func__,
		hvdcp20_state_to_str(hvdcp20->state), ret, hvdcp20->is_cable_out_occur);

	return ret_value;
}

static int hvdcp20_leave(struct tchg_alg_device *alg)
{
	int ret = 0;
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);
	u32 org_chr_volt_mivr = 0;

	hvdcp20_info("%s: starts\n", __func__);

	org_chr_volt_mivr = hvdcp20->chr_volt_mivr[HVDCP20_NONE];

	ret = hvdcp20_set_ta_vchr(alg, HVDCP20_NONE);
	if (ret != 0) {
		hvdcp20_err("%s: failed, state = %d, ret = %d\n",
			__func__, hvdcp20->state, ret);
	}

	ret = hvdcp20_hal_enable_vbus_ovp(alg, true);
	if (ret != 0) {
		hvdcp20_err("%s: enable vbus ovp fai,ret:%d\n",
			__func__, ret);
	}

	hvdcp20_hal_set_mivr(alg, false, org_chr_volt_mivr);
	if (ret != 0) {
		hvdcp20_err("%s:set mivr fail,ret:%d\n",
			__func__, ret);
	}

	hvdcp20_info("%s: OK\n", __func__);
	return ret;
}

static int _hvdcp20_init_algo(struct tchg_alg_device *alg)
{
	int log_level;
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);

	mutex_lock(&hvdcp20->access_lock);
	if (hvdcp20_hal_init_hardware(alg) != 0) {
		hvdcp20->state = ALG_INIT_FAIL;
		hvdcp20_err("%s:init hw fail\n", __func__);
	} else
		hvdcp20->state = ALG_READY;

	log_level = hvdcp20_hal_get_log_level(alg);
	hvdcp20_info("%s: log_level=%d", __func__, log_level);
	if (log_level > 0)
		hvdcp20_info_level = log_level;
	hvdcp20->ta_vchr_target = HVDCP20_NONE;
	hvdcp20->is_fast_chr = false;

	mutex_unlock(&hvdcp20->access_lock);
	hvdcp20_info("%s config:%d\n", __func__, alg->config);

	return 0;
}

static int _hvdcp20_is_algo_ready(struct tchg_alg_device *alg)
{
	int ret_value, uisoc;
	int alias_type = TC_UNKNOWN;
	int Vbat;
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);

	hvdcp20 = dev_get_drvdata(&alg->dev);

	mutex_lock(&hvdcp20->access_lock);
	__pm_stay_awake(hvdcp20->suspend_lock);

	alias_type = hvdcp20_hal_get_charger_type(alg);
	if (alias_type == TC_UNKNOWN)
		goto out;

	if (hvdcp20->state != ALG_READY)
		goto out;
		
	Vbat = hvdcp20_hal_get_vbat(alg);
	uisoc = hvdcp20_hal_get_uisoc(alg);
	if (alias_type != TC_DCP) {
		hvdcp20->state = ALG_TA_NOT_SUPPORT;
	} else if (uisoc >= hvdcp20->ta_stop_battery_soc ||
		Vbat > hvdcp20->vbat_threshold) {
		hvdcp20->state = ALG_NOT_READY;
	}
out:
	hvdcp20_info("%s state:%s\n", __func__,
		hvdcp20_state_to_str(hvdcp20->state));

	ret_value = hvdcp20->state;
	__pm_relax(hvdcp20->suspend_lock);
	mutex_unlock(&hvdcp20->access_lock);

	return ret_value;
}

static int __hvdcp20_run(struct tchg_alg_device *alg)
{
	int vchr, uisoc, ichg;
	int ret = 0, ret_value = 0;
	u32 org_chr_volt = 0;
	u32 charger_current = 0;
	int ta_vchr_target = 0;
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);

	ta_vchr_target = hvdcp20->ta_vchr_target;

	org_chr_volt = hvdcp20->chr_volt[ta_vchr_target];

	if (hvdcp20->is_cable_out_occur) {
		ret_value = ALG_TA_NOT_SUPPORT;
		goto out;
	}

	/* HVDCP20 leaves unexpectedly */
	vchr = hvdcp20_hal_get_vbus(alg);
	if (abs(vchr - org_chr_volt) > 1500000) {
		hvdcp20_err("%s: HVDCP20 leave unexpectedly, recheck TA %d %d\n",
			__func__, vchr, org_chr_volt);
		ret = hvdcp20_leave(alg);
		ret_value = ALG_TA_NOT_SUPPORT;
		goto out;
	}

	/* Check SOC & Ichg */
	ichg = hvdcp20_hal_get_ibat(alg);
	uisoc = hvdcp20_hal_get_uisoc(alg);
	if (uisoc > hvdcp20->ta_stop_battery_soc &&
		ichg > 0 && ichg < hvdcp20->ta_ichg_level_threshold) {
		ret = hvdcp20_leave(alg);
		hvdcp20_err("%s: OK, SOC = (%d,%d), ichg:(%d, %d), stop HVDCP20\n", __func__,
			uisoc, hvdcp20->ta_stop_battery_soc,
			ichg, hvdcp20->ta_ichg_level_threshold);
		ret_value = ALG_DONE;
		goto out;
	}

	/* Switch Vchr gears by charger current limit */
	charger_current = hvdcp20_hal_get_charger_current_limit(alg);
	if (charger_current <= hvdcp20->chr_volt_chg_cur[HVDCP20_SWITCH_5V] -
			hvdcp20->recover_9v_gap)
		ta_vchr_target = HVDCP20_SWITCH_5V;
	else if ((charger_current >= hvdcp20->chr_volt_chg_cur[HVDCP20_SWITCH_5V]) &&
		hvdcp20->ta_vchr_target == HVDCP20_SWITCH_5V)
		ta_vchr_target = HVDCP20_SWITCH_9V;

	if (ta_vchr_target != hvdcp20->ta_vchr_target) {
		hvdcp20_info("%s: curr_info:%d(%d,%d), switch voltage to ta_vchr_target:%d\n",
			__func__, charger_current, hvdcp20->chr_volt_chg_cur[HVDCP20_SWITCH_5V],
			hvdcp20->recover_9v_gap, ta_vchr_target);
		ret = hvdcp20_set_ta_vchr(alg, ta_vchr_target);
		if (ret != 0) {
			hvdcp20_err("%s: switch voltage failed, ret:%d\n",
				__func__, ret);
			hvdcp20_leave(alg);
			ret_value = ALG_TA_NOT_SUPPORT;
			goto out;
		}
	}
out:

	hvdcp20_info("%s: SOC:%d, Ibat = %d, plugout = %d\n", __func__,
		hvdcp20_hal_get_uisoc(alg), hvdcp20_hal_get_ibat(alg), hvdcp20->is_cable_out_occur);

	hvdcp20_info("%s: ret = %d:%d\n",
		__func__, ret, ret_value);

	return ret_value;
}

static int _hvdcp20_start_algo(struct tchg_alg_device *alg)
{
	int ret = 0, ret_value = 0;
	bool again = false;
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);

	mutex_lock(&hvdcp20->access_lock);
	__pm_stay_awake(hvdcp20->suspend_lock);

	do {
		hvdcp20_info("%s state:%d %s %d\n", __func__,
			hvdcp20->state,
			hvdcp20_state_to_str(hvdcp20->state),
			again);

		again = false;
		switch (hvdcp20->state) {
		case ALG_READY:
			ret = __hvdcp20_check_charger(alg);
			if (ret == 0) {
				hvdcp20->state = ALG_RUNNING;
				again = true;
			} else if (ret == ALG_TA_CHECKING) {
				hvdcp20->state = ret;
			} else if (ret == ALG_TA_NOT_SUPPORT) {
				hvdcp20->state = ret;
			} else {
				hvdcp20->state = ALG_TA_NOT_SUPPORT;
			}
			break;
		case ALG_RUNNING:
			ret = __hvdcp20_run(alg);
			if (ret == ALG_TA_NOT_SUPPORT) {
				hvdcp20->state = ret;
			} else if (ret == ALG_DONE) {
				hvdcp20->state = ret;
			} /* ignore else. default running */
			break;
		default:
			hvdcp20_err("hvdcp20 unknown state:%d\n", hvdcp20->state);
			hvdcp20->state = ALG_TA_NOT_SUPPORT;
			break;
		}
	} while (again == true);

	ret_value = hvdcp20->state;
	__pm_relax(hvdcp20->suspend_lock);
	mutex_unlock(&hvdcp20->access_lock);

	return ret_value;
}


static bool _hvdcp20_is_algo_running(struct tchg_alg_device *alg)
{
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);

	if (hvdcp20->state == ALG_RUNNING)
		return true;

	return false;
}

static int _hvdcp20_stop_algo(struct tchg_alg_device *alg)
{
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);

	mutex_lock(&hvdcp20->access_lock);

	hvdcp20_info("%s %d\n", __func__, hvdcp20->state);
	if (hvdcp20->state == ALG_RUNNING) {
		hvdcp20_leave(alg);
		hvdcp20->state = ALG_READY;
	}
	hvdcp20->is_fast_chr = false;
	mutex_unlock(&hvdcp20->access_lock);

	return 0;
}

static int hvdcp20_full_event(struct tchg_alg_device *alg)
{
	int ret_value = 0;
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);

	if (hvdcp20->state == ALG_RUNNING) {
		hvdcp20_err("%s evt full\n",  __func__);
		hvdcp20_leave(alg);
		hvdcp20->state = ALG_DONE;
	}

	return ret_value;
}

static int _hvdcp20_notifier_call(struct tchg_alg_device *alg,
			 struct tchg_alg_notify *notify)
{
	int ret_value = 0;
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);

	hvdcp20_err("%s evt:%d state:%s\n", __func__, notify->evt,
		hvdcp20_state_to_str(hvdcp20->state));

	switch (notify->evt) {
	case EVT_FULL:
		ret_value = hvdcp20_full_event(alg);
		break;
	default:
		ret_value = -EINVAL;
	}
	return ret_value;
}

static int tc_hvdcp20_parse_dt(struct tc_hvdcp20 *hvdcp20,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val;
	int length, i;

	/* HVDCP20 */
	if (of_property_read_u32(np, "ta_ichg_level_threshold", &val) >= 0)
		hvdcp20->ta_ichg_level_threshold = val;
	else {
		hvdcp20_info("use default TA_ICHG_LEAVE_THRESHOLD:%d\n",
			TA_ICHG_LEAVE_THRESHOLD);
		hvdcp20->ta_ichg_level_threshold = TA_ICHG_LEAVE_THRESHOLD;
	}

	if (of_property_read_u32(np, "ta_stop_battery_soc", &val) >= 0)
		hvdcp20->ta_stop_battery_soc = val;
	else {
		hvdcp20_info("use default TA_STOP_BATTERY_SOC:%d\n",
			TA_STOP_BATTERY_SOC);
		hvdcp20->ta_stop_battery_soc = TA_STOP_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "recover_9v_gap", &val) >= 0)
		hvdcp20->recover_9v_gap = val;
	else {
		hvdcp20_info("use default RECOVER_9V_GAP:%d\n",
			RECOVER_9V_GAP);
		hvdcp20->recover_9v_gap = RECOVER_9V_GAP;
	}

	if (of_property_read_u32(np, "vbat_threshold", &val) >= 0)
		hvdcp20->vbat_threshold = val;
	else {
		hvdcp20_info("turn off vbat_threshold checking:%d\n",
			DISABLE_VBAT_THRESHOLD);
		hvdcp20->vbat_threshold = DISABLE_VBAT_THRESHOLD;
	}

	length = of_property_count_elems_of_size(np, "chr_volt", sizeof(u32));
	if (length <= 0) {
		hvdcp20_err("%s: chr_volt dts undefined, check dtsi config\n", __func__);
		return -EINVAL;
	}

	hvdcp20->chr_volt = devm_kzalloc(dev, length * sizeof(u32), GFP_KERNEL);
	hvdcp20->chr_volt_mivr = devm_kzalloc(dev, length * sizeof(u32), GFP_KERNEL);
	hvdcp20->chr_volt_input_cur = devm_kzalloc(dev, length * sizeof(u32), GFP_KERNEL);
	hvdcp20->chr_volt_chg_cur = devm_kzalloc(dev, length * sizeof(u32), GFP_KERNEL);
	if (IS_ERR_OR_NULL(hvdcp20->chr_volt) ||
		IS_ERR_OR_NULL(hvdcp20->chr_volt_mivr) ||
		IS_ERR_OR_NULL(hvdcp20->chr_volt_input_cur) ||
		IS_ERR_OR_NULL(hvdcp20->chr_volt_chg_cur)) {
		hvdcp20_err("%s: cann't malloc, exit....\n", __func__);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(np, "chr_volt", hvdcp20->chr_volt, length) < 0) {
		hvdcp20_err("%s: failed parse chr_volt\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32_array(np, "chr_volt_mivr", hvdcp20->chr_volt_mivr, length)) {
		hvdcp20_err("%s: failed parse chr_volt_mivr\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32_array(np, "chr_volt_input_cur", hvdcp20->chr_volt_input_cur, length)) {
		hvdcp20_err("%s: failed parse chr_volt_input_cur\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32_array(np, "chr_volt_chg_cur", hvdcp20->chr_volt_chg_cur, length)) {
		hvdcp20_err("%s: failed parse chr_volt_chg_cur\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < length; i++) {
		hvdcp20_info("%s:chr_volt[%d] = %d, "
			"chr_volt_mivr[%d] = %d, "
			"chr_volt_input_cur[%d] = %d, "
			"chr_volt_chg_cur[%d] = %d\n", __func__,
			i, hvdcp20->chr_volt[i],
			i, hvdcp20->chr_volt_mivr[i],
			i, hvdcp20->chr_volt_input_cur[i],
			i, hvdcp20->chr_volt_chg_cur[i]);
	}

	return 0;
}

int _hvdcp20_get_prop(struct tchg_alg_device *alg, enum tchg_alg_props s, int *value)
{
	struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev);

	hvdcp20_info("%s: %d\n", __func__, s);

	switch (s) {
	case ALG_INPUT_CURRENT: //uA
		*value = hvdcp20->chr_volt_input_cur[hvdcp20->ta_vchr_target];
		break;
	case ALG_CHG_CURRENT: //uA
		*value = hvdcp20->chr_volt_chg_cur[hvdcp20->ta_vchr_target];
		break;
	case ALG_MAX_VBUS: //uV
		*value = hvdcp20->chr_volt[hvdcp20->ta_vchr_target];
		break;
	case ALG_IS_FAST_CHR:
		*value = hvdcp20->is_fast_chr;
		break;
	case ALG_ADAPTER_CAPACITY:
		*value = 18;
		break;
	case ALG_RUNNING_VOL:
		*value = hvdcp20->ta_vchr_target;
		break;
	default:
		break;
	}

	return 0;
}

int _hvdcp20_set_prop(struct tchg_alg_device *alg,
		enum tchg_alg_props s, int value)
{
	/* struct tc_hvdcp20 *hvdcp20 = dev_get_drvdata(&alg->dev); */

	hvdcp20_info("%s %d %d\n", __func__, s, value);

	switch (s) {
	case ALG_LOG_LEVEL:
		hvdcp20_info_level = value;
		break;
	default:
		break;
	}

	return 0;
}

static int _hvdcp20_plugout_reset(struct tchg_alg_device *alg)
{
	hvdcp20_info("%s\n", __func__);
	return hvdcp20_plugout_reset(alg);
}


static struct tchg_alg_ops hvdcp20_alg_ops = {
	.init_algo = _hvdcp20_init_algo,
	.is_algo_ready = _hvdcp20_is_algo_ready,
	.start_algo = _hvdcp20_start_algo,
	.is_algo_running = _hvdcp20_is_algo_running,
	.stop_algo = _hvdcp20_stop_algo,
	.notifier_call = _hvdcp20_notifier_call,
	.get_prop = _hvdcp20_get_prop,
	.set_prop = _hvdcp20_set_prop,
	.plugout_reset = _hvdcp20_plugout_reset,
};

static int tc_hvdcp20_probe(struct platform_device *pdev)
{
	struct tc_hvdcp20 *hvdcp20 = NULL;
	int ret = 0;

	pr_err("%s: starts\n", __func__);

	hvdcp20 = devm_kzalloc(&pdev->dev, sizeof(*hvdcp20), GFP_KERNEL);
	if (!hvdcp20)
		return -ENOMEM;
	platform_set_drvdata(pdev, hvdcp20);
	hvdcp20->pdev = pdev;

	hvdcp20->suspend_lock = wakeup_source_register(NULL, "HVDCP20 suspend wakelock");

	mutex_init(&hvdcp20->access_lock);
	mutex_init(&hvdcp20->cable_out_lock);

	hvdcp20->state = ALG_NOT_READY;
	ret = tc_hvdcp20_parse_dt(hvdcp20, &pdev->dev);
	if (ret < 0) {
		pr_err("%s: parse dt failed, ret:%d\n", __func__, ret);
		return ret;
	}

	hvdcp20->alg = tchg_alg_device_register("hvdcp20", &pdev->dev,
					hvdcp20, &hvdcp20_alg_ops, NULL);

	pr_err("%s: done\n", __func__);

	return 0;
}

static void tc_hvdcp20_remove(struct platform_device *dev)
{
	return;
}

static void tc_hvdcp20_shutdown(struct platform_device *dev)
{

}

static const struct of_device_id hvdcp20_of_match[] = {
	{.compatible = "transsion,hvdcp20_charger",},
	{},
};

MODULE_DEVICE_TABLE(of, hvdcp20_of_match);

static struct platform_driver hvdcp20_driver = {
	.probe = tc_hvdcp20_probe,
	.remove = tc_hvdcp20_remove,
	.shutdown = tc_hvdcp20_shutdown,
	.driver = {
		   .name = "hvdcp20",
		   .of_match_table = hvdcp20_of_match,
	},
};
module_platform_driver(hvdcp20_driver);

MODULE_DESCRIPTION("TRANSSION HVDCP20 algorithm Driver");
MODULE_LICENSE("GPL");
