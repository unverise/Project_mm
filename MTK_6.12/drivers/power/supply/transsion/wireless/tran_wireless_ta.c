/*
 * Copyright (C) 2021 Transsion Technology Corp.
 *
 * Transsion wireless Fast Charge TA for Proprietary Charging Algorithm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "tran_wireless_ta: " fmt
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include "tc_ta_class.h"
#include "tran_wireless_ta.h"

#define WIRELESS_TA_VERSION	"1.0.2"

static struct wireless_dt_args wireless_desc_defval = {
	.support_cc 		= false,
	.vta_step 			= 20,
	.ita_step 			= 30,
	.ita_gap_per_vstep 	= 20,
	.pwr_lmt 			= false,
	.support_meas_cap 	= false,
	.support_status 	= false,
	.ita_gap_cv_mode 	= 100,
	.hall_close_pdp_th  = {50, 50, 999},
	.hall_close_pdp     = {30, 30, 30},
};

static int wireless_enable_ta_wdt(struct tadapter_device *dev, bool en)
{
	return 0;
}

static int wireless_is_cc(struct tadapter_device *dev, bool *cc)
{
	return 0;
}

static int wireless_sync_volt(struct tadapter_device *dev, u32 mV)
{
	struct wireless_ta *ta = tadapter_dev_get_drvdata(dev);
	wireless_ta_volt_sync(ta->chg_dev, mV);
	return 0;
}

static int wireless_set_ta_wdt(struct tadapter_device *dev, u32 wdt)
{
	return 0;
}

static int wireless_send_hardreset(struct tadapter_device *dev)
{
	return 0;
}

static int wireless_set_cap(struct tadapter_device *dev, enum tadapter_cap_type type,
		int mV, int mA)
{
	struct wireless_ta *ta = tadapter_dev_get_drvdata(dev);

	wireless_set_tx_vol(ta->chg_dev, mV);
	return 0;
}

static int wireless_get_measure_cap(struct tadapter_device *dev, int *mV, int *mA)
{
	int ret = 0;
	struct wireless_ta *ta = tadapter_dev_get_drvdata(dev);
	
	ret = wireless_ta_get_vbus(ta->chg_dev, mV);
	if (ret)
		return ret;
	
	ret = wireless_ta_get_ibus(ta->chg_dev, mA);
	if (ret)
		return ret;

	pr_info("%s: %dmV, %dmA\n", __func__, *mV, *mA);
	return 0;
}

static int wireless_tx_rx_site(struct tadapter_device *dev, struct tadapter_status *ta_status)
{
	return 0;
}

#if IS_ENABLED(CONFIG_MTK_LINE_HALL)
extern int hall_get_state(void);
#endif

bool tc_get_hall_state(void)
{
	int hall_state = 1;
#if IS_ENABLED(CONFIG_MTK_LINE_HALL)
	hall_state = hall_get_state();
#endif
	if(hall_state)
		return false;
	else
		return true;
}

static int wireless_authenticate_ta(struct tadapter_device *dev, struct tadapter_auth_data *data)
{
	bool auth, pg, hall;
	int ret = 0,i;
	struct wls_hw_info hw_info;
	struct wireless_ta *ta = tadapter_dev_get_drvdata(dev);

	pr_info("%s start\n", __func__);

	ret = wireless_ta_get_pg_state(ta->chg_dev, &pg);
	if (ret < 0 || !pg) {
		pr_err("wireless charger pg off\n");
		return -EINVAL;
	}

	wireless_ta_authenticate(ta->chg_dev, &auth);
	if (!auth) {
		pr_err("wireless authenticate tran private protocol fail!\n");
		return -EINVAL;
	} 

	ret = wireless_get_ta_tx_cap(ta->chg_dev, &hw_info);
	if (ret) {
		pr_err("wireless get tx capacity fail(%d)!\n", ret);
		return ret;
	}

	pr_info("ta_pmax:%d, ta_vmax:%d, ta_imax:%d\n",
			hw_info.ta_pmax, hw_info.ta_vmax, hw_info.ta_imax);
	pr_info("tx_pmax:%d, tx_vmax:%d, tx_imax:%d\n",
			hw_info.tx_pmax, hw_info.tx_vmax, hw_info.tx_imax);

	data->vta_max = min(hw_info.ta_vmax, hw_info.tx_vmax);
	if(data->vta_max < data->vcap_max){
		pr_err("wireless authenticate vta_max too low!\n");
		return -EINVAL;
	}

	data->vta_min = 5500;
	data->ita_max = hw_info.tx_imax;
	data->pwr_lmt = ta->desc->pwr_lmt;
	data->support_cc = ta->desc->support_cc;
	data->support_meas_cap = ta->desc->support_meas_cap;
	data->support_status = ta->desc->support_status;
	data->vta_step = ta->desc->vta_step;
	data->ita_step = ta->desc->ita_step;
	data->ita_gap_per_vstep = ta->desc->ita_gap_per_vstep;
	data->ita_gap_cv_mode = ta->desc->ita_gap_cv_mode;
	
	data->pdp = min(hw_info.tx_pmax, hw_info.ta_pmax) / 10;
	hall = tc_get_hall_state();
	if(hall) {
		for(i = 0; i < HALL_PDP_MAX; i++) {
			if(data->pdp < ta->desc->hall_close_pdp_th[i]) {
				pr_err("hall close pdp =%d\n",data->pdp);
				break;
			} else {
				data->pdp = ta->desc->hall_close_pdp[i];
			}
		}
	}	
	return ret;
}

static int wireless_get_epp(struct tadapter_device *dev)
{
	int power = 0;
	struct wireless_ta *ta = tadapter_dev_get_drvdata(dev);

	wireless_get_tx_power(ta->chg_dev, &power);
	return power;
}

static int wireless_get_ce(struct tadapter_device *dev)
{
	struct wireless_ta *ta = tadapter_dev_get_drvdata(dev);
	return wireless_ta_get_ce(ta->chg_dev);
}

static struct tadapter_ops wireless_ops = {
	.set_cap	= wireless_set_cap,
	.get_output	= wireless_get_measure_cap,
	.get_status	= wireless_tx_rx_site,
	.is_cc		= wireless_is_cc,
	.send_hardreset	= wireless_send_hardreset,
	.authentication	= wireless_authenticate_ta,
	.sync_volt	= wireless_sync_volt,
	.set_wdt	= wireless_set_ta_wdt,
	.enable_wdt	= wireless_enable_ta_wdt,
	.get_epp	= wireless_get_epp,
	.get_ce 	= wireless_get_ce,
};

static int wireless_parse_dt(struct wireless_ta *ta)
{
	struct device_node *np = ta->dev->of_node;
	struct wireless_dt_args *desc;

	desc = devm_kzalloc(ta->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	ta->desc = desc;
	memcpy(desc, &wireless_desc_defval, sizeof(*desc));
	desc->support_cc = of_property_read_bool(np, "support_cc");

	desc->pwr_lmt = of_property_read_bool(np, "pwr_lmt");
	desc->support_meas_cap = of_property_read_bool(np, "support_meas_cap");
	desc->support_status = of_property_read_bool(np, "support_status");

	of_property_read_u32(np,"vta_step", &desc->vta_step);
	of_property_read_u32(np,"ita_step", &desc->ita_step);
	of_property_read_u32(np,"ita_gap_per_vstep", &desc->ita_gap_per_vstep);
	of_property_read_u32(np,"ita_gap_cv_mode", &desc->ita_gap_cv_mode);
	of_property_read_u32_array(np, "hall_close_pdp_th", desc->hall_close_pdp_th, HALL_PDP_MAX);
	of_property_read_u32_array(np, "hall_close_pdp", desc->hall_close_pdp, HALL_PDP_MAX);

	pr_info("%s:%d %d %d %d %d %d %d %d\n", __func__, desc->support_cc,
			desc->vta_step,desc->ita_step,desc->ita_gap_per_vstep, desc->pwr_lmt,
			desc->support_meas_cap, desc->support_status,desc->ita_gap_cv_mode);

	return 0;
}

static int wireless_ta_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct wireless_ta *ta;

	pr_info("%s(%s)\n", __func__, WIRELESS_TA_VERSION);

	ta = devm_kzalloc(&pdev->dev, sizeof(*ta), GFP_KERNEL);
	if (!ta)
		return -ENOMEM;

	ta->dev = &pdev->dev;
	platform_set_drvdata(pdev, ta);

	ta->chg_dev = get_charger_by_name("wireless_manager");
	if (!ta->chg_dev) {
		pr_err("get wireless charger manager device fail\n");
		return -EPROBE_DEFER;
	}

	ta->apd = tadapter_device_register("wireless_adapter",
		&pdev->dev, ta, &wireless_ops, NULL);
	if (IS_ERR_OR_NULL(ta->apd)) {
		pr_err("register wireless adapter fail\n");
		return -EINVAL;
	}

	tadapter_dev_set_drvdata(ta->apd, ta);
	ret = wireless_parse_dt(ta);
	if (ret < 0) {
		pr_err("%s parse dt fail\n", __func__);
		goto exit;
	}

	return 0;
exit:
	tadapter_device_unregister(ta->apd);
	return -EINVAL;
}

static void wireless_ta_remove(struct platform_device *pdev)
{
	struct wireless_ta *ta = platform_get_drvdata(pdev);

	pr_info("%s\n", __func__);
	tadapter_device_unregister(ta->apd);
	kfree(ta->apd);
	devm_kfree(&pdev->dev, ta->desc);
	devm_kfree(&pdev->dev, ta);
	return;
}

static const struct of_device_id wireless_of_match[] = {
	{.compatible = "wireless_ta",},
	{},
};

MODULE_DEVICE_TABLE(of, wireless_of_match);

static struct platform_driver wireless_ta_driver = {
	.probe = wireless_ta_probe,
	.remove = wireless_ta_remove,
	.driver = {
		.name = "wireless_ta",
		.owner = THIS_MODULE,
		.of_match_table = wireless_of_match,
	},
};

module_platform_driver(wireless_ta_driver);

MODULE_DESCRIPTION("Transsion wireless Fast Charge TA For PCA");
MODULE_AUTHOR("yongqing.dou<yongqing.dou@transsion.com>");
MODULE_VERSION(WIRELESS_TA_VERSION);
MODULE_LICENSE("GPL");

/*
 * 1.0.0_G
 * Initial release
 */
