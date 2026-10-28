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

#include "tc_tc30.h"
#include "tc_algorithm_class.h"
#include "tc_common_class.h"
#include "tc_misc_intf.h"


static char *tc30_state_to_str(int state)
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
	tc30_err("%s unknown state:%d\n", __func__, state);

	return "TC30_UNKNOWN";
}

static int tc30_reset_ta_none_check_volt(struct tchg_alg_device *alg);
static int tc30_info_level = TC30_DEBUG_LEVEL;

int tc30_get_debug_level(void)
{
	return tc30_info_level;
}

static void tc30_report_charing_animation(struct tchg_alg_device *alg)
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

static int tc30_plugout_reset(struct tchg_alg_device *alg)
{
	int ret = 0, cnt = 0;
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);
	u32 org_chr_volt_mivr = tc30->chr_volt_mivr[TC30_NONE];

	tc30_info("%s\n", __func__);
	/* set flag to end tc30 thread asap */
	mutex_lock(&tc30->cable_out_lock);
	tc30->is_cable_out_occur = true;
	mutex_unlock(&tc30->cable_out_lock);

	/* wait thread end of run */
	while (mutex_trylock(&tc30->access_lock) == 0 && cnt < 20) {
		tc30_err("%s:tc30 is running state: %s cnt:%d\n",
			__func__, tc30_state_to_str(tc30->state), cnt);
		cnt++;
		msleep(100);
	}
	/* reset cable out */
	mutex_lock(&tc30->cable_out_lock);
	tc30->is_cable_out_occur = false;
	mutex_unlock(&tc30->cable_out_lock);

	/* Reset ta control */
	tc30_reset_ta_none_check_volt(alg);

	/* Enable OVP */
	ret = tc30_hal_enable_vbus_ovp(alg, true);
	if (ret < 0)
		tc30_err("%s:enable vbus ovp fail, ret:%d\n",
			__func__, ret);

	/* Disable tc30 mivr */
	ret = tc30_hal_set_mivr(alg, false, org_chr_volt_mivr);
	if (ret < 0)
		tc30_err("%s:set mivr fail, ret:%d\n",
			__func__, ret);

	tc30->state = ALG_READY;
	tc30->is_fast_chr = false;

	tc30_info("%s: OK\n", __func__);

	mutex_unlock(&tc30->access_lock);

	return 0;
}

int tc30_reset_ta_vchr(struct tchg_alg_device *alg)
{
	int ret, chr_volt = 0, ret_value = -1;
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);
	u32 org_chr_volt = tc30->chr_volt[TC30_NONE];
	u32 org_chr_volt_mivr = tc30->chr_volt_mivr[TC30_NONE];

	tc30_info("%s: starts\n", __func__);

	/* Reset TA's charging voltage */
	ret = ta_dev_set_usb_ctrl_tc30(alg, TC30_NONE);
	if (ret < 0)
		tc30_err("%s:reset TA fail, ret:%d\n",
			__func__, ret);

	/* Check charger's voltage */
	chr_volt = tc30_hal_get_vbus(alg);
	tc30_info("%s: ori_vbus:%d vbus:%d\n", __func__,
		org_chr_volt, chr_volt);

	if (abs(chr_volt - org_chr_volt) <= 1000000) {
		ret_value = 0;
	}

	/* Reset MIVR */
	tc30_hal_set_mivr(alg, false, org_chr_volt_mivr);

	tc30_info("%s: OK\n", __func__);

	return ret;
}

static int tc30_reset_ta_none_check_volt(struct tchg_alg_device *alg)
{
	int ret;
	/* struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev); */

	tc30_info("%s: starts\n", __func__);

	/* Reset TA's charging voltage */
	ret = ta_dev_set_usb_ctrl_tc30(alg, TC30_NONE);
	if (ret < 0)
		tc30_err("%s:reset TA fail, ret:%d\n",
			__func__, ret);

	tc30_info("%s: OK\n", __func__);

	return ret;
}

static int _tc30_set_ta_vchr(struct tchg_alg_device *alg, int target_volt)
{
	int ret = 0, ret_value = 0;
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);

	tc30_info("%s: starts\n", __func__);

	/* Not to set chr volt if cable is plugged out */
	if (tc30->is_cable_out_occur && target_volt != TC30_NONE) {
		tc30_err("%s: failed, cable out\n", __func__);
		return -ECABLEOUT;
	}

	tc30->ta_vchr_target = target_volt;
	ret = ta_dev_set_usb_ctrl_tc30(alg, target_volt);
	if (ret < 0) {
		tc30_err("%s: tc30 set target_volt:%d failed, ret = %d\n",
			__func__, target_volt, ret);
		return -EHAL;
	}

	return ret_value;
}

static int tc30_set_ta_vchr(struct tchg_alg_device *alg, int target_volt)
{
	int ret = 0, ret_value = 0;
	int vchr_before, vchr_after, vchr_delta;
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);
	u32 chr_volt = tc30->chr_volt[target_volt];
	u32 chr_volt_mivr = tc30->chr_volt_mivr[target_volt];
	u32 org_chr_volt_mivr = tc30->chr_volt_mivr[TC30_NONE];

	tc30_info("%s: starts, target:%d\n", __func__, chr_volt / 1000);

	tc30_hal_set_input_current(alg, true, 1000000);
	tc30_hal_set_mivr(alg, false, org_chr_volt_mivr);

	vchr_before = tc30_hal_get_vbus(alg);
	ret = _tc30_set_ta_vchr(alg, target_volt);
	vchr_after = tc30_hal_get_vbus(alg);

	vchr_delta = abs(vchr_after - chr_volt);
	tc30_err("ret:%d delta:%d %d %d\n",
		 ret, vchr_delta, vchr_after, chr_volt);
	/*
	 * It is successful if VBUS difference to target is
	 * less than 1V.
	 */
	if (vchr_delta < 1000000 && ret == 0) {
		tc30_info("%s: OK, vchr = (%d, %d), vchr_target = %dmV\n", __func__,
			vchr_before / 1000, vchr_after / 1000, chr_volt / 1000);
		/* Set MIVR */
		tc30_hal_set_mivr(alg, true, chr_volt_mivr);
		goto out;
	}


	tc30_info("%s: vchr = (%d, %d), vchr_target = %dmV\n",
		__func__, vchr_before / 1000,
		vchr_after / 1000, chr_volt / 1000);


	if (tc30->is_cable_out_occur)
		ret_value = -ECABLEOUT;
	else
		ret_value = -EHAL;

	tc30_info("%s: failed, vchr_after = %dmV, target_vchr = %dmV, ret_value:%d\n",
		__func__, vchr_after / 1000, chr_volt / 1000, ret_value);

out:
	tc30_hal_set_input_current(alg, false, 1000000);
	return ret_value;
}

static void tc30_inform_related_module(struct tc_tc30 *tc30, bool enable)
{
	union com_propval tran_val = {0, };

	if (!tc30->port_burn_dev)
		tc30->port_burn_dev = tran_get_by_name("port_burn");

	tran_val.intval = enable;
	if (enable) {
		tran_dev_set_prop(tc30->port_burn_dev, TRAN_PROP_IS_TC30_TA, &tran_val);
	}
}

static int tc30_detect_ta(struct tchg_alg_device *alg)
{
	int ret = 0;
	/* struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev); */

	tc30_info("%s: starts\n", __func__);

	/* Disable Normal OVP */
	ret = tc30_hal_enable_vbus_ovp(alg, false);
	if (ret < 0)
		goto err;

	ret = tc30_set_ta_vchr(alg, TC30_7V5);
	if (ret < 0) {
		tc30_info("%s: set ta vchr failed, ret:%d\n",
			__func__, ret);
		goto err;
	}

	tc30_info("%s: ret:%d\n", __func__, ret);

	return ret;
err:
	/* Enable Normal OVP */
	tc30_hal_enable_vbus_ovp(alg, true);
	tc30_err("%s: failed, ret = %d\n", __func__, ret);
	return ret;
}

static int __tc30_check_charger(struct tchg_alg_device *alg)
{
	int ret = 0, ret_value = 0;
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);

	tc30_info("%s\n", __func__);

	if (tc30->is_cable_out_occur)
		goto out;

	ret = tc30_reset_ta_none_check_volt(alg);
	if (ret != 0)
		goto out;

	if (tc30->is_cable_out_occur)
		goto out;

	ret = tc30_detect_ta(alg);
	if (ret < 0)
		goto out;

	tc30->is_fast_chr = true;
	tc30_report_charing_animation(alg);
	tc30_inform_related_module(tc30, true);

	tc30_info("%s: OK, state = %d\n",
		__func__, tc30->state);

	return ret;
out:

	if (ret_value == 0)
		ret_value = ALG_TA_NOT_SUPPORT;

	/* Recover Normal state */
	tc30_reset_ta_none_check_volt(alg);

	tc30_info("%s: state:%s, ret:%d, plugout:%d\n", __func__,
		tc30_state_to_str(tc30->state), ret, tc30->is_cable_out_occur);

	return ret_value;
}

static int tc30_leave(struct tchg_alg_device *alg)
{
	int ret = 0;
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);
	u32 org_chr_volt_mivr = 0;

	tc30_info("%s: starts\n", __func__);

	org_chr_volt_mivr = tc30->chr_volt_mivr[TC30_NONE];

	ret = tc30_set_ta_vchr(alg, TC30_NONE);
	if (ret != 0) {
		tc30_err("%s: failed, state = %d, ret = %d\n",
			__func__, tc30->state, ret);
	}

	ret = tc30_hal_enable_vbus_ovp(alg, true);
	if (ret != 0) {
		tc30_err("%s: enable vbus ovp fai,ret:%d\n",
			__func__, ret);
	}

	tc30_hal_set_mivr(alg, false, org_chr_volt_mivr);
	if (ret != 0) {
		tc30_err("%s:set mivr fail,ret:%d\n",
			__func__, ret);
	}

	tc30_info("%s: OK\n", __func__);
	return ret;
}

static int _tc30_init_algo(struct tchg_alg_device *alg)
{
	int log_level;
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);

	mutex_lock(&tc30->access_lock);
	if (tc30_hal_init_hardware(alg) != 0) {
		tc30->state = ALG_INIT_FAIL;
		tc30_err("%s:init hw fail\n", __func__);
	} else
		tc30->state = ALG_READY;

	log_level = tc30_hal_get_log_level(alg);
	tc30_info("%s: log_level=%d", __func__, log_level);
	if (log_level > 0)
		tc30_info_level = log_level;
	tc30->ta_vchr_target = TC30_NONE;
	tc30->is_fast_chr = false;

	mutex_unlock(&tc30->access_lock);
	tc30_info("%s config:%d\n", __func__, alg->config);

	return 0;
}

static int _tc30_is_algo_ready(struct tchg_alg_device *alg)
{
	int ret_value, uisoc;
	int alias_type = TC_UNKNOWN;
	int Vbat;
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);

	tc30 = dev_get_drvdata(&alg->dev);

	mutex_lock(&tc30->access_lock);
	__pm_stay_awake(tc30->suspend_lock);

	alias_type = tc30_hal_get_charger_type(alg);
	if (alias_type == TC_UNKNOWN)
		goto out;

	if (tc30->state != ALG_READY)
		goto out;

	Vbat = tc30_hal_get_vbat(alg);
	uisoc = tc30_hal_get_uisoc(alg);
	if (alias_type != TC_DCP) {
		tc30->state = ALG_TA_NOT_SUPPORT;
	} else if (uisoc >= tc30->ta_stop_battery_soc ||
		Vbat > tc30->vbat_threshold) {
		tc30->state = ALG_NOT_READY;
	}
out:
	tc30_info("%s state:%s\n", __func__,
		tc30_state_to_str(tc30->state));

	ret_value = tc30->state;
	__pm_relax(tc30->suspend_lock);
	mutex_unlock(&tc30->access_lock);

	return ret_value;
}

static int __tc30_run(struct tchg_alg_device *alg)
{
	int vchr, uisoc, ichg;
	int ret = 0, ret_value = 0;
	u32 org_chr_volt = 0;
	u32 charger_current = 0;
	int ta_vchr_target = 0;
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);

	ta_vchr_target = tc30->ta_vchr_target;

	org_chr_volt = tc30->chr_volt[ta_vchr_target];

	if (tc30->is_cable_out_occur) {
		ret_value = ALG_TA_NOT_SUPPORT;
		goto out;
	}

	/* TC30 leaves unexpectedly */
	vchr = tc30_hal_get_vbus(alg);
	if (abs(vchr - org_chr_volt) > 1000000) {
		tc30_err("%s: TC30 leave unexpectedly, recheck TA %d %d\n",
			__func__, vchr, org_chr_volt);
		ret = tc30_leave(alg);
		ret_value = ALG_TA_NOT_SUPPORT;
		goto out;
	}

	/* Check SOC & Ichg */
	ichg = tc30_hal_get_ibat(alg);
	uisoc = tc30_hal_get_uisoc(alg);
	if (uisoc > tc30->ta_stop_battery_soc &&
		ichg > 0 && ichg < tc30->ta_ichg_level_threshold) {
		ret = tc30_leave(alg);
		tc30_err("%s: OK, SOC = (%d,%d), ichg:(%d, %d), stop TC30\n", __func__,
			uisoc, tc30->ta_stop_battery_soc,
			ichg, tc30->ta_ichg_level_threshold);
		ret_value = ALG_DONE;
		goto out;
	}

	/* Switch Vchr gears by charger current limit */
	charger_current = tc30_hal_get_charger_current_limit(alg);
	if (charger_current <= tc30->chr_volt_chg_cur[TC30_SWITCH_5V] - tc30->recover_7v5_gap)
		ta_vchr_target = TC30_SWITCH_5V;
	else if ((charger_current >= tc30->chr_volt_chg_cur[TC30_SWITCH_5V]) &&
		tc30->ta_vchr_target == TC30_SWITCH_5V)
		ta_vchr_target = TC30_SWITCH_7V5;

	if (ta_vchr_target != tc30->ta_vchr_target) {
		tc30_info("%s: curr_info:%d(%d,%d), switch voltage to ta_vchr_target:%d\n",
			__func__, charger_current, tc30->chr_volt_chg_cur[TC30_SWITCH_5V],
			tc30->recover_7v5_gap, ta_vchr_target);
		ret = tc30_set_ta_vchr(alg, ta_vchr_target);
		if (ret != 0) {
			tc30_err("%s: switch voltage failed, ret:%d\n",
				__func__, ret);
			tc30_leave(alg);
			ret_value = ALG_TA_NOT_SUPPORT;
			goto out;
		}
	}
out:

	tc30_info("%s: SOC:%d, Ibat = %d, plugout = %d\n", __func__,
		tc30_hal_get_uisoc(alg), tc30_hal_get_ibat(alg), tc30->is_cable_out_occur);

	tc30_info("%s: ret = %d:%d\n",
		__func__, ret, ret_value);

	return ret_value;
}

static int _tc30_start_algo(struct tchg_alg_device *alg)
{
	int ret = 0, ret_value = 0;
	bool again = false;
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);

	mutex_lock(&tc30->access_lock);
	__pm_stay_awake(tc30->suspend_lock);

	do {
		tc30_info("%s state:%d %s %d\n", __func__,
			tc30->state,
			tc30_state_to_str(tc30->state),
			again);

		again = false;
		switch (tc30->state) {
		case ALG_READY:
			ret = __tc30_check_charger(alg);
			if (ret == 0) {
				tc30->state = ALG_RUNNING;
				again = true;
			} else if (ret == ALG_TA_CHECKING) {
				tc30->state = ret;
			} else if (ret == ALG_TA_NOT_SUPPORT) {
				tc30->state = ret;
			} else {
				tc30->state = ALG_TA_NOT_SUPPORT;
			}
			break;
		case ALG_RUNNING:
			ret = __tc30_run(alg);
			if (ret == ALG_TA_NOT_SUPPORT) {
				tc30->state = ret;
			} else if (ret == ALG_DONE) {
				tc30->state = ret;
			} /* ignore else. default running */
			break;
		default:
			tc30_err("tc30 unknown state:%d\n", tc30->state);
			tc30->state = ALG_TA_NOT_SUPPORT;
			break;
		}
	} while (again == true);

	ret_value = tc30->state;
	__pm_relax(tc30->suspend_lock);
	mutex_unlock(&tc30->access_lock);

	return ret_value;
}


static bool _tc30_is_algo_running(struct tchg_alg_device *alg)
{
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);

	if (tc30->state == ALG_RUNNING)
		return true;

	return false;
}

static int _tc30_stop_algo(struct tchg_alg_device *alg)
{
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);

	mutex_lock(&tc30->access_lock);

	tc30_info("%s %d\n", __func__, tc30->state);
	if (tc30->state == ALG_RUNNING) {
		tc30_set_ta_vchr(alg, TC30_NONE);
		tc30->state = ALG_READY;
	}
	tc30->is_fast_chr = false;
	mutex_unlock(&tc30->access_lock);

	return 0;
}

static int tc30_full_event(struct tchg_alg_device *alg)
{
	int ret_value = 0;
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);

	if (tc30->state == ALG_RUNNING) {
		tc30_err("%s evt full\n",  __func__);
		tc30_leave(alg);
		tc30->state = ALG_DONE;
	}

	return ret_value;
}

static int _tc30_notifier_call(struct tchg_alg_device *alg,
			 struct tchg_alg_notify *notify)
{
	int ret_value = 0;
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);

	tc30_err("%s evt:%d state:%s\n", __func__, notify->evt,
		tc30_state_to_str(tc30->state));

	switch (notify->evt) {
	case EVT_FULL:
		ret_value = tc30_full_event(alg);
		break;
	default:
		ret_value = -EINVAL;
	}
	return ret_value;
}

static int tc_tc30_parse_dt(struct tc_tc30 *tc30,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val;
	int length, i;

	/* TC30 */
	if (of_property_read_u32(np, "ta_ichg_level_threshold", &val) >= 0)
		tc30->ta_ichg_level_threshold = val;
	else {
		tc30_info("use default TA_ICHG_LEAVE_THRESHOLD:%d\n",
			TA_ICHG_LEAVE_THRESHOLD);
		tc30->ta_ichg_level_threshold = TA_ICHG_LEAVE_THRESHOLD;
	}

	if (of_property_read_u32(np, "ta_stop_battery_soc", &val) >= 0)
		tc30->ta_stop_battery_soc = val;
	else {
		tc30_info("use default TA_STOP_BATTERY_SOC:%d\n",
			TA_STOP_BATTERY_SOC);
		tc30->ta_stop_battery_soc = TA_STOP_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "recover_7v5_gap", &val) >= 0)
		tc30->recover_7v5_gap = val;
	else {
		tc30_info("use default RECOVER_7V5_GAP:%d\n",
			RECOVER_7V5_GAP);
		tc30->recover_7v5_gap = RECOVER_7V5_GAP;
	}

	if (of_property_read_u32(np, "vbat_threshold", &val) >= 0)
		tc30->vbat_threshold = val;
	else {
		tc30_info("turn off vbat_threshold checking:%d\n",
			DISABLE_VBAT_THRESHOLD);
		tc30->vbat_threshold = DISABLE_VBAT_THRESHOLD;
	}

	length = of_property_count_elems_of_size(np, "chr_volt", sizeof(u32));
	if (length <= 0) {
		tc30_err("%s: chr_volt dts undefined, check dtsi config\n", __func__);
		return -EINVAL;
	}

	tc30->chr_volt = devm_kzalloc(dev, length * sizeof(u32), GFP_KERNEL);
	tc30->chr_volt_mivr = devm_kzalloc(dev, length * sizeof(u32), GFP_KERNEL);
	tc30->chr_volt_input_cur = devm_kzalloc(dev, length * sizeof(u32), GFP_KERNEL);
	tc30->chr_volt_chg_cur = devm_kzalloc(dev, length * sizeof(u32), GFP_KERNEL);
	if (IS_ERR_OR_NULL(tc30->chr_volt) ||
		IS_ERR_OR_NULL(tc30->chr_volt_mivr) ||
		IS_ERR_OR_NULL(tc30->chr_volt_input_cur) ||
		IS_ERR_OR_NULL(tc30->chr_volt_chg_cur)) {
		tc30_err("%s: cann't malloc, exit....\n", __func__);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(np, "chr_volt", tc30->chr_volt, length) < 0) {
		tc30_err("%s: failed parse chr_volt\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32_array(np, "chr_volt_mivr", tc30->chr_volt_mivr, length)) {
		tc30_err("%s: failed parse chr_volt_mivr\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32_array(np, "chr_volt_input_cur", tc30->chr_volt_input_cur, length)) {
		tc30_err("%s: failed parse chr_volt_input_cur\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32_array(np, "chr_volt_chg_cur", tc30->chr_volt_chg_cur, length)) {
		tc30_err("%s: failed parse chr_volt_chg_cur\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < length; i++) {
		tc30_info("%s:chr_volt[%d] = %d, "
			"chr_volt_mivr[%d] = %d, "
			"chr_volt_input_cur[%d] = %d, "
			"chr_volt_chg_cur[%d] = %d\n", __func__,
			i, tc30->chr_volt[i],
			i, tc30->chr_volt_mivr[i],
			i, tc30->chr_volt_input_cur[i],
			i, tc30->chr_volt_chg_cur[i]);
	}

	return 0;
}

int _tc30_get_prop(struct tchg_alg_device *alg, enum tchg_alg_props s, int *value)
{
	struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev);

	tc30_info("%s: %d\n", __func__, s);

	switch (s) {
	case ALG_INPUT_CURRENT: //uA
		*value = tc30->chr_volt_input_cur[tc30->ta_vchr_target];
		break;
	case ALG_CHG_CURRENT: //uA
		*value = tc30->chr_volt_chg_cur[tc30->ta_vchr_target];
		break;
	case ALG_MAX_VBUS: //uV
		*value = tc30->chr_volt[tc30->ta_vchr_target];
		break;
	case ALG_IS_FAST_CHR:
		*value = tc30->is_fast_chr;
		break;
	case ALG_ADAPTER_CAPACITY:
		*value = 18;
		break;
	default:
		break;
	}

	return 0;
}

int _tc30_set_prop(struct tchg_alg_device *alg,
		enum tchg_alg_props s, int value)
{
	/* struct tc_tc30 *tc30 = dev_get_drvdata(&alg->dev); */

	tc30_info("%s %d %d\n", __func__, s, value);

	switch (s) {
	case ALG_LOG_LEVEL:
		tc30_info_level = value;
		break;
	default:
		break;
	}

	return 0;
}

static int _tc30_plugout_reset(struct tchg_alg_device *alg)
{
	tc30_info("%s\n", __func__);
	return tc30_plugout_reset(alg);
}


static struct tchg_alg_ops tc30_alg_ops = {
	.init_algo = _tc30_init_algo,
	.is_algo_ready = _tc30_is_algo_ready,
	.start_algo = _tc30_start_algo,
	.is_algo_running = _tc30_is_algo_running,
	.stop_algo = _tc30_stop_algo,
	.notifier_call = _tc30_notifier_call,
	.get_prop = _tc30_get_prop,
	.set_prop = _tc30_set_prop,
	.plugout_reset = _tc30_plugout_reset,
};

static int tc_tc30_probe(struct platform_device *pdev)
{
	struct tc_tc30 *tc30 = NULL;
	int ret = 0;

	pr_err("%s: starts\n", __func__);

	tc30 = devm_kzalloc(&pdev->dev, sizeof(*tc30), GFP_KERNEL);
	if (!tc30)
		return -ENOMEM;
	platform_set_drvdata(pdev, tc30);
	tc30->pdev = pdev;

	tc30->suspend_lock = wakeup_source_register(NULL, "TC30 suspend wakelock");

	mutex_init(&tc30->access_lock);
	mutex_init(&tc30->cable_out_lock);

	tc30->state = ALG_NOT_READY;
	ret = tc_tc30_parse_dt(tc30, &pdev->dev);
	if (ret < 0) {
		pr_err("%s: parse dt failed, ret:%d\n", __func__, ret);
		return ret;
	}

	tc30->alg = tchg_alg_device_register("tc30", &pdev->dev,
					tc30, &tc30_alg_ops, NULL);

	pr_err("%s: done\n", __func__);

	return 0;
}

static void tc_tc30_remove(struct platform_device *dev)
{
	return;
}

static void tc_tc30_shutdown(struct platform_device *dev)
{

}

static const struct of_device_id tc30_of_match[] = {
	{.compatible = "transsion,tc30_charger",},
	{},
};

MODULE_DEVICE_TABLE(of, tc30_of_match);

static struct platform_driver tc30_driver = {
	.probe = tc_tc30_probe,
	.remove = tc_tc30_remove,
	.shutdown = tc_tc30_shutdown,
	.driver = {
		   .name = "tc30",
		   .of_match_table = tc30_of_match,
	},
};
module_platform_driver(tc30_driver);

MODULE_DESCRIPTION("TRANSSION TC30 algorithm Driver");
MODULE_LICENSE("GPL");
