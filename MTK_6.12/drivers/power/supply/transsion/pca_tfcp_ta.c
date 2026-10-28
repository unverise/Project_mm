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

#define PCA_TFCP_TA_VERSION	"1.0.0_G"
#define TFCP_STAT_MODE	BIT(0)
#define TFCP_STAT_PSRDY	BIT(3)
#define TFCP_STAT_OVP	BIT(16)
#define TFCP_STAT_OTP1	BIT(17)
#define TFCP_STAT_OTP2	BIT(18)
#define TFCP_STAT_UVP	BIT(19)
#define TFCP_STAT_OCP	BIT(20)

struct pca_tfcp_info {
	struct device *dev;
	struct pca_tfcp_dt_args *desc;
	struct tadapter_device *pca;
	struct tc_ta_classdev *tfcp;
	const char *adapter_dev_name;
	struct tran_device *usb_control_dev;
	struct tran_device *adapter_ctrl_dev;
	struct tran_device *port_burn_dev;
	struct tran_device *tc_charger_dev;
	bool is_unknow_ta;
	bool is_rfc_adapter;
	bool tfcp_ta_det_done;
	int retry_cnt;
};

struct pca_tfcp_dt_args {
	bool support_cc;
	/* bool support_s65w; */
	u32 vta_step;
	u32 ita_step;
	u32 ita_gap_per_vstep;
	bool pwr_lmt;
	bool support_meas_cap;
	bool support_status;
	u32 ita_gap_cv_mode;
	int retry_cnt_max;
};

static struct pca_tfcp_dt_args pca_tfcp_desc_defval = {
	.support_cc = true,
	/* .support_s65w = false, */
	.vta_step = 20,
	.ita_step = 30,
	.ita_gap_per_vstep = 20,
	.pwr_lmt=false,
	.support_meas_cap=true,
	.support_status=true,
	.ita_gap_cv_mode = 50,
};

static bool is_IP33_tadapter(u8 *code)
{
	int i =0;
	char mfinfo[8] = {'A', 'B', 'C', 'D', 'I', 'P', '3', '3'};
	if (!code)
		return false;
	for(i =0; i < 8; i++) {
		if(mfinfo[i] == code[i])
			continue;
		break;
	}
	if (8 == i)
		return true;
	return false;
}


static int pca_tfcp_get_ta_fw(struct tadapter_device *dev,u8 *code,
			      u32 length)
{
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);
	int ret=0;

	if (IS_ERR_OR_NULL(info)){
		return -EINVAL;
	}

	ret = tc_ta_device_get_fwcode(info->tfcp,code,length);
	if (ret < 0) {
		pr_info("get ta fw fail(%d)\n", ret);
		return ret;
	}

	return 0;
}

#if 0
static int pca_tfcp_get_ta_mfinfo(struct tadapter_device *dev,u8 *code,
			      u32 length)
{
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);
	int ret=0;

	ret=tc_ta_device_get_mfinfo(info->tfcp,code,length);
	if (ret < 0) {
		pr_info("get ta mfinfo fail(%d)\n", ret);
		return ret;
	}

	return 0;
}
#endif

static int pca_tfcp_enable_charging(struct tadapter_device *dev, bool en,
				   u32 mV, u32 mA)
{
	int ret;
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);

	ret = tc_ta_device_set_voltage_current(info->tfcp, mV, mA);
	if (ret < 0) {
		pr_info("set voltage current fail(%d)\n", ret);
		return ret;
	}
	pr_info("%s: en = %d, %dmV, %dmA,ret = %d\n", __func__,en, mV, mA, ret);
	return 0;
}

static int pca_tfcp_set_cap(struct tadapter_device *dev, enum tadapter_cap_type type,
		int mV, int mA)
{
	int ret;
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);
	if (IS_ERR_OR_NULL(info)){
		return -EINVAL;
	}

	ret = tc_ta_device_set_voltage_current(info->tfcp, mV, mA);
	if (ret < 0) {
		pr_info("set voltage current fail(%d)\n", ret);
		return ret;
	}
	pr_info("%s: %dmV, %dmA,ret=%d\n", __func__, mV, mA, ret);
	return 0;
}

static int pca_tfcp_get_measure_cap(struct tadapter_device *dev, int *mV, int *mA)
{
	int ret;
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(info)){
		return -EINVAL;
	}

	ret = tc_ta_device_get_output_voltage_current(info->tfcp, mV, mA);
	if (ret < 0) {
		pr_info("get output voltage current fail(%d)\n", ret);
		return ret;
	}
	pr_info("%s: %dmV, %dmA\n", __func__, *mV, *mA);
	return 0;
}

static int pca_tfcp_get_temperature(struct tadapter_device *dev, int *temp)
{
	int ret;
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);

	ret = tc_ta_device_get_temp1(info->tfcp, temp);
	if (ret < 0) {
		pr_info("get temp fail(%d)\n", ret);
		return ret;
	}
	pr_info("%s: %d degree\n", __func__, *temp);
	return 0;
}

static int pca_tfcp_get_status(struct tadapter_device *dev,struct tadapter_status *ta_status)
{
	int ret;
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);
	u32 status;

	if (IS_ERR_OR_NULL(info)){
		return -EINVAL;
	}

	ret = tc_ta_device_get_status(info->tfcp, &status);
	if (ret < 0) {
		pr_info("get status fail(%d)\n", ret);
		return ret;
	}
	ret = pca_tfcp_get_temperature(dev, &ta_status->temperature);
	if (ret < 0) {
		pr_info("get temperature fail(%d)\n", ret);
		return ret;
	}

	ta_status->ocp = (status & TFCP_STAT_OCP) ? true : false;
	ta_status->otp = (status & (TFCP_STAT_OTP1 | TFCP_STAT_OTP2)) ? true
								    : false;
	ta_status->ovp = (status & TFCP_STAT_OVP) ? true : false;
	ta_status->cc_mode = (status & TFCP_STAT_MODE) ? true : false;
	ta_status->cable_capability = (status & 0x300) >>8;
	pr_info("%s:0x%x 0x%x\n", __func__, status,ta_status->cable_capability);

	return 0;
}

static int pca_tfcp_is_cc(struct tadapter_device *dev, bool *cc)
{
	int ret;
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);
	u32 status;

	if (IS_ERR_OR_NULL(info)){
		return -EINVAL;
	}

	ret = tc_ta_device_get_status(info->tfcp, &status);
	if (ret < 0) {
		pr_info("get status fail(%d)\n", ret);
		return ret;
	}

	*cc = (status & TFCP_STAT_MODE) ? true : false;
	return ret > 0 ? -ret : ret;
}

static int pca_tfcp_send_hardreset(struct tadapter_device *dev)
{
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(info)){
		return -EINVAL;
	}

	pr_info("%s: ++\n", __func__);
	tc_ta_device_enable_wdt(info->tfcp, false);
	return tc_ta_device_set_voltage_current(info->tfcp, 5300, 3000);
}

static int pca_tfcp_exit_rfc_mode(struct pca_tfcp_info *info)
{
	union com_propval tran_val = {0, };

	pr_info("%s\n", __func__);
	tran_val.intval = RFC_NONE;
	tran_dev_set_prop(info->usb_control_dev,
			TRAN_PROP_USB_CTRL_RFC, &tran_val);

	return 0;
}

static void pca_tfcp_inform_related_module(struct pca_tfcp_info *info)
{
	union com_propval tran_val = {0, };

	pr_info("%s\n", __func__);

	if (IS_ERR_OR_NULL(info->adapter_ctrl_dev))
		info->adapter_ctrl_dev = tran_get_by_name("adapter_control");
	if (IS_ERR_OR_NULL(info->port_burn_dev))
		info->port_burn_dev = tran_get_by_name("port_burn");


	tran_val.intval = info->is_rfc_adapter;
	tran_dev_set_prop(info->adapter_ctrl_dev, TRAN_PROP_IS_RFC_TA, &tran_val);
	tran_dev_set_prop(info->port_burn_dev, TRAN_PROP_IS_RFC_TA, &tran_val);
}

static int pca_tfcp_is_rfc_ta(struct pca_tfcp_info *info)
{
	int alias_type = TC_UNKNOWN;
	union com_propval tran_val = {0, };
	union com_propval bc12_done = {1, };
	int ret;
	int pd_type;
	int i;

	if (info->is_rfc_adapter) {
		pr_info("already check successfully, ignore\n");
		goto out;
	}

	alias_type = tc_get_alias_type();

	if (IS_ERR_OR_NULL(info->tc_charger_dev)) {
		info->tc_charger_dev = tran_get_by_name("tc_charger");
		if (IS_ERR_OR_NULL(info->tc_charger_dev)) {
			pr_info("get tc_charger dev fail\n");
			goto out;
		}
	}

	tran_dev_get_prop(info->tc_charger_dev, TRAN_PROP_BC12_DONE, &bc12_done);
	if (alias_type == TC_NON_STD) {
		pr_info("is TC_NON_STD adaptor, ignore check\n");
		info->is_unknow_ta = true;
		info->is_rfc_adapter = false;
		info->retry_cnt = 0;
		goto out;
	}

	info->is_unknow_ta = false;

	if (alias_type != TC_DCP) {
		pr_info("other adapter type, ignore check\n");
		info->is_rfc_adapter = false;
		goto out;
	}

	pd_type = tc_get_pd_type();
	if (pd_type == TC_PD_CONNECT_PE_READY_SNK ||
		pd_type == TC_PD_CONNECT_PE_READY_SNK_PD30 ||
		pd_type == TC_PD_CONNECT_PE_READY_SNK_APDO) {
		pr_info("is PD adapter, ignore check, pd state = %d\n", pd_type);
		info->is_rfc_adapter = false;
		goto out;
	}

	if (info->retry_cnt > info->desc->retry_cnt_max) {
		pr_info("trigger retry_cnt_max, ignore check,retry_cnt:%d\n",
			info->retry_cnt);
		info->is_rfc_adapter = false;
		goto out;
	}

	for (i = 0; i <= info->desc->retry_cnt_max; i++) {
		tran_val.intval = RFC_HANDSHAKE;
		ret = tran_dev_set_prop(info->usb_control_dev,
				TRAN_PROP_USB_CTRL_RFC, &tran_val);
		if (!ret) {
			info->is_rfc_adapter = !ret;
			break;
		}
		pca_tfcp_exit_rfc_mode(info);
		info->retry_cnt++;
	}


	if (!info->is_rfc_adapter) {
		pr_info("not rfc adapter, exit rfc mode\n");
		pca_tfcp_exit_rfc_mode(info);
	} else {
		pr_info("is rfc adapter, retry_cnt:%d\n", info->retry_cnt);
		pca_tfcp_inform_related_module(info);
	}
out:
	info->tfcp_ta_det_done = true;
	if (alias_type == TC_NON_STD && !(bc12_done.intval))
		info->tfcp_ta_det_done = false;
	return 0;
}

static int pca_tfcp_authenticate_ta(struct tadapter_device *dev,
			     struct tadapter_auth_data *data)
{
	int ret;
	int alias_type = 0;
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);
	u32 vmin, vmax, imax, power, time;
	u8 code[8] = {0};
	u8 vnd_info[5] = {0};

	pr_info("%s: ++\n", __func__);

	alias_type = tc_get_alias_type();
	if (alias_type == TC_WIRELESS){
		pr_info("%s: alias_type is wireless,skip tfcp authen\n", __func__);
		ret = -ENODEV;
		return ret;
	}

	if (IS_ERR_OR_NULL(info)){
		 return -EINVAL;
	}

	pca_tfcp_is_rfc_ta(info);

	if (!info->is_rfc_adapter) {
		ret = -ENODEV;
		return ret;
	}

	if (!info->tfcp) {
		info->tfcp = tc_ta_device_get_by_name("tfcp_ta");
		if (!info->tfcp) {
			pr_info("get tfcp ta fail\n");
			ret = -ENODEV;
			goto out;
		}
	}

	tc_ta_device_get_mfinfo(info->tfcp, code, 8);
	pr_info("is_ip33_tadapter = %d\n", is_IP33_tadapter(code));
	if (!is_IP33_tadapter(code)) {
		ret = tc_ta_device_authentication(info->tfcp);
		if (ret < 0) {
			pr_info("authentication fail(%d)\n", ret);
			ret = -101;
			goto out;
		}
	} else {
		ret = pca_tfcp_enable_charging(dev, true, 5500, 2200);
		if (ret < 0) {
			pr_info("enable_charging fail(%d)\n", ret);
			ret = -101;
			goto out;
		}
	}

	ret = tc_ta_device_get_fwcode(info->tfcp, vnd_info, 4);
	if (ret < 0) {
		pr_err("get vendor info fail(%d)\n", ret);
		goto out;
	}
	vnd_info[4] = '\0';
	pr_info("TA vnd_info = %s\n", vnd_info);

	ret = tc_ta_device_get_min_voltage(info->tfcp, &vmin);
	if (ret < 0) {
		pr_info("get vmin fail(%d)\n", ret);
		goto out;
	}

	ret = tc_ta_device_get_max_voltage(info->tfcp, &vmax);
	if (ret < 0) {
		pr_info("get vmax fail(%d)\n", ret);
		goto out;
	}

	ret = tc_ta_device_get_max_current(info->tfcp, &imax);
	if (ret < 0) {
		pr_info("get imax fail(%d)\n", ret);
		goto out;
	}

	ret=tc_ta_device_get_power_limit(info->tfcp, &power);
	if (ret < 0) {
		pr_info("get power limit fail(%d)\n", ret);
		goto out;
	}

	ret = tc_ta_device_get_max_power_duration(info->tfcp, &time);
	if (ret < 0) {
		pr_info("get max power duration fail(%d)\n", ret);
		goto out;
	}

	pr_info("tfcp boundary,%dW %d mv ~ %d mv, %d ma %d %d %d, max_power_duration %d\n",
		power, vmin, vmax, imax,data->vcap_min,data->vcap_max,data->icap_min, time);

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
		data->pdp = power;
		data->full_power_run_time = time;
	} else {
		pr_info("%s:tfcp cap cannot support pca algo\n", __func__);
		ret = -EINVAL;
	}
out:

	return ret;
}

static int pca_tfcp_enable_wdt(struct tadapter_device *dev, bool en)
{
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);
	if (IS_ERR_OR_NULL(info)){
		return -EINVAL;
	}

	pr_info("%s\n", __func__);
	return tc_ta_device_enable_wdt(info->tfcp, en);
}

static int pca_tfcp_set_wdt(struct tadapter_device *dev, u32 wdt)
{
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(info)){
		return -EINVAL;
	}

	pr_info("%s\n", __func__);
	return tc_ta_device_set_wdt(info->tfcp, wdt);
}

static int pca_tfcp_plug_out_reset(struct tadapter_device *dev)
{
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(info)){
		return 0;
	}

	pr_info("%s\n", __func__);
	pca_tfcp_exit_rfc_mode(info);

	info->is_rfc_adapter = false;
	info->tfcp_ta_det_done = false;
	info->retry_cnt = 0;

	return 0;
}

static int pca_tfcp_get_det_done(struct tadapter_device *dev, bool *det_done)
{
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(info)){
		return 0;
	}

	pr_info("%s: tfcp_ta_det_done:%d\n", __func__, info->tfcp_ta_det_done);
	*det_done = info->tfcp_ta_det_done;

	return 0;
}

static int pca_tfcp_exit_protocol(struct tadapter_device *dev)
{
	struct pca_tfcp_info *info = tadapter_dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(info)){
		return 0;
	}

	pr_info("%s\n", __func__);
	pca_tfcp_exit_rfc_mode(info);

	return 0;
}

static struct tadapter_ops tfcp_tadapter_ops = {
	.set_cap = pca_tfcp_set_cap,
	.get_output = pca_tfcp_get_measure_cap,
	.get_status = pca_tfcp_get_status,
	.is_cc = pca_tfcp_is_cc,
	.send_hardreset = pca_tfcp_send_hardreset,
	.authentication = pca_tfcp_authenticate_ta,
	.enable_wdt = pca_tfcp_enable_wdt,
	.set_wdt = pca_tfcp_set_wdt,
	.get_ta_fw = pca_tfcp_get_ta_fw,
	//.get_ta_mfinfo = pca_tfcp_get_ta_mfinfo,
	.plug_out_reset = pca_tfcp_plug_out_reset,
	.get_det_done = pca_tfcp_get_det_done,
	.exit_protocol = pca_tfcp_exit_protocol,
};

static int pca_tfcp_parse_dt(struct pca_tfcp_info *info)
{
	struct device_node *np = info->dev->of_node;
	struct pca_tfcp_dt_args *desc;

	desc = devm_kzalloc(info->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	info->desc = desc;
	memcpy(desc, &pca_tfcp_desc_defval, sizeof(*desc));

	if (of_property_read_string(np, "adapter_name",&info->adapter_dev_name) < 0){
		pr_info("%s: no tadapter name\n", __func__);
		info->adapter_dev_name= "tfcp_adapter";
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
		dev_err(info->dev, "Get ita_gap_cv_mode failed\n");
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

static int pca_tfcp_probe(struct platform_device *pdev)
{
	int ret;
	struct pca_tfcp_info *info;

	dev_info(&pdev->dev, "%s(%s)\n", __func__, PCA_TFCP_TA_VERSION);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->dev = &pdev->dev;
	platform_set_drvdata(pdev, info);

	info->tfcp = tc_ta_device_get_by_name("tfcp_ta");
	if (!info->tfcp) {
		dev_err(info->dev, "%s get tfcp dev fail\n", __func__);
		return -ENODEV;
	}

	info->usb_control_dev = tran_get_by_name("usb_control");
	if (IS_ERR_OR_NULL(info->usb_control_dev)) {
		dev_err(info->dev, "%s: get usb_control_dev failed\n", __func__);
		return -ENODEV;
	}
	
	info->adapter_ctrl_dev = tran_get_by_name("adapter_control");
	if (IS_ERR_OR_NULL(info->adapter_ctrl_dev)) {
		pr_err("can't find adapter control dev\n");
	}

	info->tc_charger_dev = tran_get_by_name("tc_charger");
	if (IS_ERR_OR_NULL(info->tc_charger_dev)) {
		pr_err("can't find tc_charger dev\n");
	}

	ret = pca_tfcp_parse_dt(info);
	if (ret < 0) {
		dev_notice(info->dev, "%s parse dt fail\n", __func__);
		return ret;
	}

	info->pca = tadapter_device_register(info->adapter_dev_name,
		&pdev->dev, info, &tfcp_tadapter_ops, NULL);
	if (IS_ERR_OR_NULL(info->pca)) {
		ret = PTR_ERR(info->pca);
		dev_err(&pdev->dev, "register pca_tfcp_ta fail\n");
		return -EINVAL;
	}
	tadapter_dev_set_drvdata(info->pca, info);

	dev_info(info->dev, "%s successfully\n", __func__);

	return 0;
}

static void pca_tfcp_remove(struct platform_device *pdev)
{
	return;
}

static const struct of_device_id pca_tfcp_of_match[] = {
	{.compatible = "tc,pca_tfcp_ta",},
	{},
};
MODULE_DEVICE_TABLE(of, pca_tfcp_of_match);

static struct platform_driver pca_tfcp_platdrv = {
	.probe = pca_tfcp_probe,
	.remove = pca_tfcp_remove,
	.driver = {
		.name = "pca_tfcp",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = pca_tfcp_of_match,
#endif
	},
};

static int __init pca_tfcp_init(void)
{
	return platform_driver_register(&pca_tfcp_platdrv);
}
device_initcall_sync(pca_tfcp_init);

static void __exit pca_tfcp_exit(void)
{
	platform_driver_unregister(&pca_tfcp_platdrv);
}
module_exit(pca_tfcp_exit);

MODULE_VERSION(PCA_TFCP_TA_VERSION);
MODULE_LICENSE("GPL");

/*
 * 1.0.1_G
 * (1) Modify authentication data, provide information of power limit,
 *     step of voltage and current
 *
 * 1.0.0_G
 * Initial release
 */
