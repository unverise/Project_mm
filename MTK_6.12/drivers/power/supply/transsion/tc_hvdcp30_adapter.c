/*
 * Copyright (C) 2019 Richtek Technology Corp.
 *
 * Fast Charge TA for Proprietary Charging Algorithm
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/of.h>

#include "tc_adapter_class.h"
#include "tc_ta_class.h"
#include "tc_common_class.h"
#include "tc_misc_intf.h"
#include "tc_tcpc.h"
#include "tc_voter.h"

#define TC_HVDCP30_TA_VERSION	"1.0.0_G"

struct tc_hvdcp30_info {
	struct device *dev;
	struct tc_hvdcp30_dt_args *desc;
	struct tadapter_device *hvdcp30;
	const char *adapter_dev_name;
	struct tran_device *usb_control_dev;
	int is_hvdcp30_adapter;
	int retry_cnt;
	int last_request_volt;
};

struct tc_hvdcp30_dt_args {
	bool support_cc;
	u32 vta_step;
	u32 ita_step;
	u32 ita_gap_per_vstep;
	bool pwr_lmt;
	bool support_meas_cap;
	bool support_status;
	u32 ita_gap_cv_mode;
	int retry_cnt_max;
	int request_min_volt;
};

static struct tc_hvdcp30_dt_args tc_hvdcp30_desc_defval = {
	.support_cc = false,
	.vta_step = 200,
	.ita_step = 180,
	.ita_gap_per_vstep = 200,
	.pwr_lmt = false,
	.support_meas_cap = false,
	.support_status = false,
	.ita_gap_cv_mode = 200,
};

enum {
	TUNE_VBUS_UP = 0,
	TUNE_VBUS_DOWN,
	TUNE_VBUS_RESET,
};

static int tc_hvdcp30_vbus_tune(struct tc_hvdcp30_info *info, int tune_dir, int pulse_cnt)
{
	union com_propval tran_val = {0, };
	int i;
	int vbus;

	pr_info("%s: tune_dir:%d, pulse_cnt:%d\n",
		__func__, tune_dir, pulse_cnt);

	if (tune_dir == TUNE_VBUS_RESET) {
		// 可以考虑改写成红米那种直接掉压到5V的模式
		for (i = 0; i <= 100; i++) {
			tran_val.intval = HVDCP30_DM_PULSE;
			tran_dev_set_prop(info->usb_control_dev, TRAN_PROP_USB_CTRL_HVDCP30, &tran_val);
			vbus = tc_get_vbus();
			if (vbus <= 5500)
				break;
		}
		vbus = tc_get_vbus();
		pr_info("%s: reset vbus done, %dmV\n", __func__, vbus);
		return 0;
	}

	for (i = 0; i < pulse_cnt; i++) {
		if (tune_dir == TUNE_VBUS_UP) {
			tran_val.intval = HVDCP30_DP_PULSE;
			tran_dev_set_prop(info->usb_control_dev, TRAN_PROP_USB_CTRL_HVDCP30, &tran_val);
		} else if (tune_dir == TUNE_VBUS_DOWN) {
			tran_val.intval = HVDCP30_DM_PULSE;
			tran_dev_set_prop(info->usb_control_dev, TRAN_PROP_USB_CTRL_HVDCP30, &tran_val);
		}
	}

	return 0;
}

static int tc_hvdcp30_set_cap(struct tadapter_device *dev, enum tadapter_cap_type type,
		int mV, int mA)
{
	int ret = 0;
	struct tc_hvdcp30_info *info = tadapter_dev_get_drvdata(dev);
	int vbus = 0;
	int request_volt = mV;
	int step_cnt = 0;

	vbus = tc_get_vbus();

	pr_info("%s: vbus_now:%dmV, request:%dmV, %dmA, last_request:%d\n",
		__func__, vbus, mV, mA, info->last_request_volt);

	if (request_volt <= info->desc->request_min_volt) {
		tc_hvdcp30_vbus_tune(info, TUNE_VBUS_RESET, 0);
		info->last_request_volt = 0;
		goto out;
	}

	if (request_volt == info->last_request_volt)
		goto out;

	if (info->last_request_volt == 0) {
		step_cnt = abs(vbus - request_volt) / info->desc->vta_step;
		tc_hvdcp30_vbus_tune(info, TUNE_VBUS_UP, step_cnt);
		goto out;
	}

	step_cnt = abs(info->last_request_volt - request_volt) / info->desc->vta_step;
	if (request_volt < info->last_request_volt) {
		tc_hvdcp30_vbus_tune(info, TUNE_VBUS_DOWN, step_cnt);
	} else {
		tc_hvdcp30_vbus_tune(info, TUNE_VBUS_UP, step_cnt);
	}

out:
	info->last_request_volt = request_volt;
	return ret;
}

static int tc_hvdcp30_get_measure_cap(struct tadapter_device *dev, int *mV, int *mA)
{
	return TC_ADAPTER_OK;
}

static int tc_hvdcp30_send_hardreset(struct tadapter_device *dev)
{
	return TC_ADAPTER_OK;
}

static int tc_hvdcp30_exit_hvdcp30(struct tc_hvdcp30_info *info)
{
	union com_propval tran_val = {0, };

	pr_info("%s\n", __func__);
	tran_val.intval = HVDCP30_NONE;
	tran_dev_set_prop(info->usb_control_dev,
			TRAN_PROP_USB_CTRL_HVDCP30, &tran_val);

	return 0;
}

static int tc_hvdcp30_mode_check(struct tc_hvdcp30_info *info)
{
	int alias_type = TC_UNKNOWN;
	union com_propval tran_val = {0, };
	int ret;
	int pd_type;
	int i;

	if (info->is_hvdcp30_adapter == IS_HVDCP30_A ||
		info->is_hvdcp30_adapter == IS_HVDCP30_B) {
		pr_info("already check successfully, ignore\n");
		goto out;
	}

	alias_type = tc_get_alias_type();

	if (alias_type == TC_NON_STD) {
		pr_info("is TC_NON_STD adaptor, ignore check\n");
		info->is_hvdcp30_adapter = NOT_HVDCP30;
		info->retry_cnt = 0;
		goto out;
	}

	if (alias_type != TC_DCP) {
		pr_info("other adapter type, ignore check\n");
		info->is_hvdcp30_adapter = NOT_HVDCP30;
		goto out;
	}

	pd_type = tc_get_pd_type();
	if (pd_type == TC_PD_CONNECT_PE_READY_SNK ||
		pd_type == TC_PD_CONNECT_PE_READY_SNK_PD30 ||
		pd_type == TC_PD_CONNECT_PE_READY_SNK_APDO) {
		pr_info("is PD adapter, ignore check, pd state = %d\n", pd_type);
		info->is_hvdcp30_adapter = NOT_HVDCP30;
		goto out;
	}

	if (info->retry_cnt > info->desc->retry_cnt_max) {
		pr_info("trigger retry_cnt_max, ignore check,retry_cnt:%d\n",
			info->retry_cnt);
		info->is_hvdcp30_adapter = NOT_HVDCP30;
		goto out;
	}

	for (i = 0; i <= info->desc->retry_cnt_max; i++) {
		tran_val.intval = HVDCP30_HANDSHAKE;
		ret = tran_dev_set_prop(info->usb_control_dev,
				TRAN_PROP_USB_CTRL_HVDCP30, &tran_val);
		if (!ret) {
			info->is_hvdcp30_adapter = ret;
			break;
		}

		tc_hvdcp30_exit_hvdcp30(info);
		info->retry_cnt++;
	}

	if (info->is_hvdcp30_adapter == NOT_HVDCP30) {
		pr_info("not hvdcp30 adapter, exit rfc mode\n");
		tc_hvdcp30_exit_hvdcp30(info);
	} else {
		pr_info("is hvdcp30 adapter\n");
	}
out:
	return 0;
}

static int tc_hvdcp30_authenticate_ta(struct tadapter_device *dev,
			     struct tadapter_auth_data *data)
{
	int ret = TC_ADAPTER_OK;
	struct tc_hvdcp30_info *info = tadapter_dev_get_drvdata(dev);
	u32 vmin, vmax, imax;
	/* int vbus; */
	/* struct votable *total_aicr_vote = NULL; */

	pr_info("%s: ++\n", __func__);

	tc_hvdcp30_mode_check(info);

	if (info->is_hvdcp30_adapter == NOT_HVDCP30) {
		return -EINVAL;
	}

	/* total_aicr_vote = find_votable("total_aicr"); */
	/* if (total_aicr_vote == NULL) { */
	/*         pr_info("can't find total_aicr_vote\n"); */
	/*         return -EINVAL; */
	/* } */

	/* vote(total_aicr_vote, HVDCP30_VOTER, true, 500000); */

	/* check adapter max capacity */
	/* tc_hvdcp30_vbus_tune(info, TUNE_VBUS_UP, 32); */

	/* vbus = tc_get_vbus(); */
	/* if (vbus > 12000) { */
	/*         vmin = 4500; */
	/*         vmax = 20000; */
	/*         imax = 3000; */
	/* } else { */
	/*         vmin = 4500; */
	/*         vmax = 12000; */
	/*         imax = 3000; */
	/* } */

	/* reset vbus to 5V */
	/* tc_hvdcp30_vbus_tune(info, TUNE_VBUS_RESET, 0); */
	
	/* vote(total_aicr_vote, HVDCP30_VOTER, false, 0); */

	if (info->is_hvdcp30_adapter == IS_HVDCP30_B) {
		vmin = 4500;
		vmax = 20000;
		imax = 3000;
	} else {
		vmin = 4500;
		vmax = 12000;
		imax = 3000;
	}

	pr_info("hvdcp30 boundary,%d mv ~ %d mv, %d ma %d %d %d\n",
		vmin, vmax, imax,data->vcap_min,data->vcap_max,data->icap_min);

	if (vmin <= data->vcap_min && vmax >= data->vcap_max &&
	    imax >= data->icap_min) {
		data->vta_max = vmax;
		data->vta_min = vmin;
		data->ita_max = imax;
		data->pwr_lmt = info->desc->pwr_lmt; /* 0 stands for no power limit */
		data->support_cc = info->desc->support_cc;
		data->support_meas_cap = info->desc->support_meas_cap;
		data->support_status = info->desc->support_status;
		data->vta_step = info->desc->vta_step;
		data->ita_step = info->desc->ita_step;
		data->ita_gap_per_vstep = info->desc->ita_gap_per_vstep;
		data->ita_gap_cv_mode = info->desc->ita_gap_cv_mode;
	} else {
		pr_info("hvdcp30 cap cannot support hvdcp30 algo\n");
		ret = -EINVAL;
	}

	/* if (ret != TC_ADAPTER_OK) { */
	/*         pr_info("auth failed, exit hvdcp30 mode, ret:%d\n", ret); */
	/*         tc_hvdcp30_exit_hvdcp30(info); */
	/* } */

	return ret;
}

static int tc_hvdcp30_plug_out_reset(struct tadapter_device *dev)
{
	struct tc_hvdcp30_info *info = tadapter_dev_get_drvdata(dev);

	pr_info("%s\n", __func__);
	tc_hvdcp30_exit_hvdcp30(info);

	info->is_hvdcp30_adapter = NOT_HVDCP30;
	info->retry_cnt = 0;
	info->last_request_volt = 0;

	return 0;
}

static int tc_hvdcp30_set_wdt(struct tadapter_device *dev, u32 wdt)
{
	return TC_ADAPTER_OK;
}

static int tc_hvdcp30_en_wdt(struct tadapter_device *dev, bool en)
{
	return TC_ADAPTER_OK;
}

static int tc_hvdcp30_exit_protocol(struct tadapter_device *dev)
{
	struct tc_hvdcp30_info *info = tadapter_dev_get_drvdata(dev);

	pr_info("%s\n", __func__);
	tc_hvdcp30_exit_hvdcp30(info);

	return 0;	
}

static struct tadapter_ops hvdcp30_tadapter_ops = {
	.set_cap = tc_hvdcp30_set_cap,
	.get_output = tc_hvdcp30_get_measure_cap,
	.send_hardreset = tc_hvdcp30_send_hardreset,
	.authentication = tc_hvdcp30_authenticate_ta,
	.plug_out_reset = tc_hvdcp30_plug_out_reset,
	.set_wdt = tc_hvdcp30_set_wdt,
	.enable_wdt = tc_hvdcp30_en_wdt,
	.exit_protocol = tc_hvdcp30_exit_protocol,
};

static int tc_hvdcp30_parse_dt(struct tc_hvdcp30_info *info)
{
	struct device_node *np = info->dev->of_node;
	struct tc_hvdcp30_dt_args *desc;

	desc = devm_kzalloc(info->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	info->desc = desc;
	memcpy(desc, &tc_hvdcp30_desc_defval, sizeof(*desc));

	if (of_property_read_string(np, "adapter_name",&info->adapter_dev_name) < 0){
		pr_info("%s: no tadapter name\n", __func__);
		info->adapter_dev_name= "hvdcp30_adapter";
	}
	
	desc->support_cc = of_property_read_bool(np, "support_cc");
	dev_info(info->dev, "support_cc = %d\n", desc->support_cc);

	/* desc->support_s65w = of_property_read_bool(np, "support_s65w"); */
	desc->pwr_lmt = of_property_read_bool(np, "pwr_lmt");
	desc->support_meas_cap = of_property_read_bool(np, "support_meas_cap");
	desc->support_status = of_property_read_bool(np, "support_status");

	if (of_property_read_u32(np,"vta_step", &desc->vta_step)<0) {
		dev_err(info->dev, "Get vta_step failed\n");
	}

	if (of_property_read_u32(np,"ita_step", &desc->ita_step)<0) {
		dev_err(info->dev, "Get ita_step failed\n");
	}

	if (of_property_read_u32(np,"ita_gap_per_vstep", &desc->ita_gap_per_vstep)<0) {
		dev_err(info->dev, "Get ita_gap_per_vstep failed\n");
	}

	//of_property_read_u32(np, "ita_min", &desc->ita_min);
	if (of_property_read_u32(np,"ita_gap_cv_mode", &desc->ita_gap_cv_mode)<0) {
		dev_err(info->dev, "Get ita_gap_cv_mode failed\n");
	}
	
	if (of_property_read_u32(np,"retry_cnt_max", &desc->retry_cnt_max) < 0) {
		desc->retry_cnt_max = 2;
		dev_err(info->dev, "Get retry cnt max failed\n");
	}

	if (of_property_read_u32(np, "request_min_volt", &desc->request_min_volt) < 0) {
		dev_err(info->dev, "request_min_volt failed, use default value\n");
		desc->request_min_volt = 6000; // 6V
	}
	/* dev_err(info->dev, "%s:%d %d %d %d %d %d %d %d\n", */
	/*                 __func__,desc->support_cc,desc->support_s65w, */
	/*                 desc->vta_step,desc->ita_step,desc->ita_gap_per_vstep, */
	/*                 desc->pwr_lmt,desc->support_meas_cap,desc->support_status); */
	dev_err(info->dev, "%s:%d %d %d %d %d %d %d\n", __func__,
		desc->support_cc, desc->vta_step, desc->ita_step,
		desc->ita_gap_per_vstep, desc->pwr_lmt, 
		desc->support_meas_cap, desc->support_status);
	return 0;
}

static int tc_hvdcp30_probe(struct platform_device *pdev)
{
	int ret;
	struct tc_hvdcp30_info *info;

	dev_info(&pdev->dev, "%s(%s)\n", __func__, TC_HVDCP30_TA_VERSION);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->dev = &pdev->dev;
	platform_set_drvdata(pdev, info);

	info->usb_control_dev = tran_get_by_name("usb_control");
	if (IS_ERR_OR_NULL(info->usb_control_dev)) {
		dev_err(info->dev, "%s: get usb_control_dev failed\n", __func__);
		return -ENODEV;
	}

	ret = tc_hvdcp30_parse_dt(info);
	if (ret < 0) {
		dev_notice(info->dev, "%s parse dt fail\n", __func__);
		return ret;
	}

	info->hvdcp30 = tadapter_device_register(info->adapter_dev_name,
		&pdev->dev, info, &hvdcp30_tadapter_ops, NULL);
	if (IS_ERR_OR_NULL(info->hvdcp30)) {
		ret = PTR_ERR(info->hvdcp30);
		dev_err(&pdev->dev, "register tc_hvdcp30_ta fail\n");
		return -EINVAL;
	}
	tadapter_dev_set_drvdata(info->hvdcp30, info);

	dev_info(info->dev, "%s successfully\n", __func__);

	return 0;
}

static void tc_hvdcp30_remove(struct platform_device *pdev)
{
	return;
}

static const struct of_device_id tc_hvdcp30_of_match[] = {
	{.compatible = "tc,tc_hvdcp30_ta",},
	{},
};
MODULE_DEVICE_TABLE(of, tc_hvdcp30_of_match);

static struct platform_driver tc_hvdcp30_platdrv = {
	.probe = tc_hvdcp30_probe,
	.remove = tc_hvdcp30_remove,
	.driver = {
		.name = "tc_hvdcp30",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = tc_hvdcp30_of_match,
#endif
	},
};

static int __init tc_hvdcp30_init(void)
{
	return platform_driver_register(&tc_hvdcp30_platdrv);
}
device_initcall_sync(tc_hvdcp30_init);

static void __exit tc_hvdcp30_exit(void)
{
	platform_driver_unregister(&tc_hvdcp30_platdrv);
}
module_exit(tc_hvdcp30_exit);

MODULE_VERSION(TC_HVDCP30_TA_VERSION);
MODULE_LICENSE("GPL");

/*
 * 1.0.1_G
 * (1) Modify authentication data, provide information of power limit,
 *     step of voltage and current
 *
 * 1.0.0_G
 * Initial release
 */
