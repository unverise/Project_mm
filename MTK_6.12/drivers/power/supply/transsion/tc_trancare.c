#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/alarmtimer.h>

#include "tc_charger.h"
#include "tc_auto_test.h"
#include "tc_charger_class.h"
#include "tc_pe5.h"
#include "tc_common_class.h"
#include "tc_algorithm_class.h"
#include "tc_misc_intf.h"
#include "tc_tcpc.h"
#include "tc_pd.h"
#include "tc_battery.h"
#include "tc_gauge.h"
#include "tc_trancare.h"
#include "tranlog.h"

#define TRANSCARE_TID 184561000002

#define TC_TRANCARE_GET_ERR -1
#define WIRED_RESERVE_PLUGIN	1
#define WIRELESS_RESERVE_PLUGIN	2
#define OTG_DEVICE_PLUGIN	3

enum pe50_curlmt_module {
	MODULE_UNKNOWN = 0,
	TA_CURLMT,
	TBAT_CURLMT,
	TDVCHG_CURLMT,
	TPCB_CURLMT,
	TPA_CURLMT,
	SYS_POWER_CURLMT,
	PRLMT_CURLMT,
	TA_IMAX_CURLMT,
	RCABLE_CURLMT,

	PE50_CURLMT_MODULE_MAX,
};

static char *pe50_curlmt_module_text[PE50_CURLMT_MODULE_MAX] = {
	[MODULE_UNKNOWN] = "unknown",
	[TA_CURLMT] = "ta",
	[TBAT_CURLMT] = "tbat",
	[TDVCHG_CURLMT] = "tdvchg",
	[TPCB_CURLMT] = "tpcb",
	[TPA_CURLMT] = "tpa",
	[SYS_POWER_CURLMT] = "sys_power",
	[PRLMT_CURLMT] = "prlmt",
	[TA_IMAX_CURLMT] = "ta_imax",
	[RCABLE_CURLMT] = "rcable",
};

static char *tran_trancare_usb_type_text[] = {
	[TRAN_USB_TYPE_UNKNOWN]	=	"Unknown",
	[TRAN_USB_TYPE_SDP]	=	"SDP",
	[TRAN_USB_TYPE_DCP]	=	"DCP",
	[TRAN_USB_TYPE_CDP]	=	"CDP",
	[TRAN_USB_TYPE_TC30]	=	"TC30",
	[TRAN_USB_TYPE_PE]	=	"PE",
	[TRAN_USB_TYPE_PE2]	=	"PE2",
	[TRAN_USB_TYPE_PE4]	=	"PE4",
	[TRAN_USB_TYPE_HVDCP]	=	"HVDCP",
	[TRAN_USB_TYPE_PDC]	=	"PD",
	[TRAN_USB_TYPE_PD_PPS]	=	"PD_PPS",
	[TRAN_USB_TYPE_RFC]	=	"RFC",
	[TRAN_USB_TYPE_NONSTAND]	=	"NONSTAND",
	[TRAN_USB_TYPE_WIRELESS]	=	"WIRELESS",
	[TRAN_USB_TYPE_WIRELESS_BPP]	=	"WIRELESS_BPP",
	[TRAN_USB_TYPE_WIRELESS_EPP]	=	"WIRELESS_EPP",
	[TRAN_USB_TYPE_WIRELESS_PE50]	=	"WIRELESS_PE50",
};

static struct tc_trancare_data tc_trancare_chg_array[] = {
	{TRAN_TRANCARE_PROP_CHG_TYPE,	"chg_type"},
	{TRAN_TRANCARE_PROP_CAPACITY,	"capacity"},
	{TRAN_TRANCARE_PROP_ENHANCE_MODE,	"enhance_mode"},
	{TRAN_TRANCARE_PROP_PCB_TEMP,   "tpcb"},
	{TRAN_TRANCARE_PROP_BAT_TEMP,	"tbat"},
	{TRAN_TRANCARE_PROP_PA_TEMP,	"tpa"},
	{TRAN_TRANCARE_PROP_TA_POWER,	"ta_power"},
	{TRAN_TRANCARE_PROP_CHGSPEED_REAL,	"chg_speed_real"},
	{TRAN_TRANCARE_PROP_CHGSPEED_OWNER,	"chg_speed_owner"},
	{TRAN_TRANCARE_PROP_WLS_TRAN_DEVICES,	"wls_tran_devices"},
	{TRAN_TRANCARE_PROP_BYPASS_EN,	"bypass_en"},
	{TRAN_TRANCARE_PROP_WATER_DET,	"water_det"},
	{TRAN_TRANCARE_PROP_HIGH_TEMP_DISCHG,	"high_temp_dischg"},
	{TRAN_TRANCARE_PROP_LOW_TEMP_DISCHG,	"low_temp_dischg"},
	{TRAN_TRANCARE_PROP_AICHG_EN,	"aichg_en"},
	{TRAN_TRANCARE_PROP_FG_BAT_CYCLE,	"bat_cycle"},
	{TRAN_TRANCARE_PROP_FG_BAT_HEALTH,	"bat_health"},
	{TRAN_TRANCARE_PROP_R_CABLE,	"rcable_level"},
	{TRAN_TRANCARE_PROP_TA_TEMP,	"ta_temp"},
	{TRAN_TRANCARE_PROP_CHG_STATUS,	"chg_status"},
	{TRAN_TRANCARE_PROP_SCREEN_ON,	"screen_on"},
	{TRAN_TRANCARE_PROP_RESERVE_CHG_TYPE,	"reserve_chg_type"},
};

static struct tc_trancare_data tc_trancare_chg_array_char[] = {
	{TRAN_TRANCARE_PROP_CHG_TYPE,	"chg_type"},
	{TRAN_TRANCARE_PROP_PE50_LMT_MODULE,	"pe50_lmt_module"},
};

static struct tc_trancare_data tc_trancare_reserve_chg_array[] = {
	{TRAN_TRANCARE_PROP_RESERVE_CHG_TYPE,	"reserve_chg_type"},
};

enum tran_chg_ta_power {
	TRAN_CHG_POWER_0W = 0,
	TRAN_CHG_POWER_2P5W = 2,
	TRAN_CHG_POWER_5W = 5,
	TRAN_CHG_POWER_6W = 6,
	TRAN_CHG_POWER_7P5W = 7,
	TRAN_CHG_POWER_10W = 10,
	TRAN_CHG_POWER_15W = 15,
	TRAN_CHG_POWER_18W = 18,
};

static struct tc_trancare_info *g_trancare_info = NULL;

static bool rfc_is_running(struct tchg_alg_device *pe5_alg)
{
	if (pe5_alg && tchg_alg_is_algo_running(pe5_alg))
		return true;

	return false;
}

static void pe50_get_curlmt_module(struct tchg_alg_device *pe5_alg, int *min_index)
{
	struct pe50_algo_info *pe5_info = tchg_alg_dev_get_drvdata(pe5_alg);
	struct pe50_algo_data *data = pe5_info->data;
	struct pe50_algo_desc *desc = pe5_info->desc;
	struct pe50_ta_auth_data *auth_data = &data->ta_auth_data[data->running_spec];
	int curlmt[PE50_CURLMT_MODULE_MAX] = {0};
	int i = 0;
	int ita_min = 0;

	if (!min_index) {
		pr_err("%s: min_index pointer is NULL!", __func__);
		return;
	}

	curlmt[0] = data->ita_lmt;
	curlmt[1] = data->ita_lmt - desc->tta_curlmt[data->tta_level];
	curlmt[2] = data->ita_lmt - data->tbat_curlmt[data->tbat_level];
	curlmt[3] = data->ita_lmt - desc->tdvchg_curlmt[data->tdvchg_level];
	curlmt[4] = data->ita_lmt - data->tpcb_curlmt[data->tpcb_level];
	curlmt[5] = data->ita_lmt - data->tpa_curlmt[data->tpa_level];
	curlmt[6] = data->ita_lmt;
	curlmt[7] = data->ita_pwr_lmt;
	curlmt[8] = auth_data->ita_max;
	curlmt[9] = data->ita_lmt_rcable;

	if (!data->screen_on && data->state == PE50_ALGO_CC_CV &&
		desc->sys_power_curlmt[data->sys_power_level] > 0) {
		curlmt[6] = desc->sys_power_curlmt[data->sys_power_level];
	}

	tchg_alg_get_prop(pe5_alg, ALG_PE50_LMT_ITA_MIN, &ita_min);
	if(ita_min == data->ita_lmt)
		return;

	for(i = 0; i < PE50_CURLMT_MODULE_MAX; i++) {
		if(ita_min == curlmt[i]) {
			min_index[i] = i;
		}
	}

	return;
}

static void tc_chg_trancare_data_get(int type, int *val)
{
	struct tchg_alg_device *pe5_alg = get_tchg_alg_by_name("pe5");
	struct tchg_alg_device *pdc_alg = get_tchg_alg_by_name("pd");
	struct tran_device *gauge_dev = tran_get_by_name("tc_gauge");
	struct tran_device *tc_chg_dev = tran_get_by_name("tc_charger");
	struct tran_device *tc_wd_dev = tran_get_by_name("water_detect");
	struct tran_device *pid_chg_dev = tran_get_by_name("pid_chg_algo");
	struct tran_device *tc_otg = tran_get_by_name("tc_otg");
	struct tran_device *tc_wireless = tran_get_by_name("tc_wireless");
	union com_propval tran_val = {0, };
	union com_propval tran_val_temp = {0, };
	union com_propval tran_val_temp02 = {0, };

	struct pe50_algo_info *pe5_info = NULL;
	int ret = 0;
	int value = -1;
	int chg_type = 0;

	if(!val) {
        pr_err("%s: val pointer is NULL!", __func__);
        return;
	}

	if (pe5_alg)
		pe5_info = tchg_alg_dev_get_drvdata(pe5_alg);

	switch (type)
	{
	case TRAN_TRANCARE_PROP_CAPACITY:
		ret =tran_dev_get_prop(gauge_dev, TRAN_PROP_CAPACITY, &tran_val);
		if (ret < 0) {
			pr_err("get ui soc failed, ret = %d\n", ret);
		}
		*val = tran_val.intval;
		break;
	case TRAN_TRANCARE_PROP_ENHANCE_MODE:
		if(pid_chg_dev)
			tran_dev_get_prop(pid_chg_dev, TRAN_PROP_ENHANCE_MODE, &tran_val);

		*val = tran_val.intval;
		break;
	case TRAN_TRANCARE_PROP_FG_BAT_CYCLE:
		if(gauge_dev)
			tran_dev_get_prop(gauge_dev, TRAN_PROP_BATTERY_CYCLE, &tran_val);

		*val = tran_val.intval;
		break;
	case TRAN_TRANCARE_PROP_FG_BAT_HEALTH:
		ret = tran_dev_get_prop(tc_chg_dev, TRAN_PROP_BAT_HEALTH_PERCENT, &tran_val);
		if (ret < 0) {
			pr_err("get bat health percent failed, ret = %d\n", ret);
		}
		*val = tran_val.intval;
		break;
	/*temperature*/
	case TRAN_TRANCARE_PROP_BAT_TEMP:
		ret = tran_dev_get_prop(gauge_dev, TRAN_PROP_TEMP, &tran_val);
		if (ret < 0) {
			pr_err("get bat temp failed, ret = %d\n", ret);
		}
		*val = tran_val.intval/10;
		break;
	case TRAN_TRANCARE_PROP_PCB_TEMP:
		*val = tc_get_tpcb_temp();
		break;
	case TRAN_TRANCARE_PROP_USB_TEMP:
		tc_get_tusb_temp(val);
		break;
	case TRAN_TRANCARE_PROP_PA_TEMP:
		*val = max(tc_get_tpa_temp_4g(),
					tc_get_tpa_temp_5g());
		break;
	case TRAN_TRANCARE_PROP_TA_TEMP:
		if (pe5_alg && rfc_is_running(pe5_alg)) {
			ret = tchg_alg_get_prop(pe5_alg, ALG_TA_TEMP, &value);
			if (ret < 0) {
				pr_err("get tta fail(%d)\n", ret);
			}
			*val = value;
		} else {
			*val = TC_TRANCARE_GET_ERR;
		}
		break;
	case TRAN_TRANCARE_PROP_CHG_TYPE:
		ret =tran_dev_get_prop(tc_chg_dev, TRAN_PROP_TC_CHG_TYPE, &tran_val);
		if (ret < 0) {
			pr_err("get ibat failed, ret = %d\n", ret);
		}
		*val = tran_val.intval;
		break;
	case TRAN_TRANCARE_PROP_RESERVE_CHG_TYPE:
		if(IS_ERR_OR_NULL(tc_wireless)) {
			*val = 0;
			pr_err("%s:can not get tc_wireless device!\n", __func__);
		} else {
			tran_dev_get_prop(tc_wireless, TRAN_PROP_WLS_RESERVE_TYPE, &tran_val);
			if(tran_val.intval == true)
				*val = WIRELESS_RESERVE_PLUGIN;
		}
		if (IS_ERR_OR_NULL(tc_otg)) {
			*val = 0;
			pr_err("%s:can not get tc_otg device!\n", __func__);
		} else {
			tran_dev_get_prop(tc_otg, TRAN_PROP_OTG_CTL_TYPE, &tran_val);
			tran_dev_get_prop(tc_otg, TRAN_PROP_OTG_PLUG_IN, &tran_val_temp);
			tran_dev_get_prop(tc_otg, TRAN_PROP_OTG_PD_CONNECT, &tran_val_temp02);
			if(tran_val.intval != 0 && tran_val_temp.intval != 0) {
				if(tran_val_temp02.intval != 0)
					*val = WIRED_RESERVE_PLUGIN;
				else
					*val = OTG_DEVICE_PLUGIN;
			}
		}
		break;
	case TRAN_TRANCARE_PROP_CHG_STATUS:
		ret =tran_dev_get_prop(gauge_dev, TRAN_PROP_BATT_STATUS, &tran_val);
		if (ret < 0) {
			pr_err("get chg status failed, ret = %d\n", ret);
		}
		*val = tran_val.intval;
		break;
	case TRAN_TRANCARE_PROP_CHGSPEED_REAL:
		if (pe5_alg && rfc_is_running(pe5_alg)) {
			tchg_alg_get_prop(pe5_alg, ALG_MULTI_CHG_SPEED_REAL, val);
		} else {
			*val = 0;
		}
		break;
	case TRAN_TRANCARE_PROP_CHGSPEED:
		if (pe5_alg && rfc_is_running(pe5_alg)) {
			tchg_alg_get_prop(pe5_alg, ALG_MULTI_CHG_SPEED, &value);
			*val = value;
		} else
			*val = 0;
		break;
	case TRAN_TRANCARE_PROP_CHGSPEED_OWNER:
		if (pe5_alg && rfc_is_running(pe5_alg)) {
			tchg_alg_get_prop(pe5_alg, ALG_MULTI_CHG_SPEED_OWNER, &value);
			*val = value;
		} else
			*val = 0;
		break;
	case TRAN_TRANCARE_PROP_BYPASS_EN:
		ret = tran_dev_get_prop(tc_chg_dev, TRAN_PROP_GET_BYPASS_EN, &tran_val);
		if (ret < 0) {
			pr_err("get bypass_en failed, ret = %d\n", ret);
		}
		*val = tran_val.intval;
		break;
	case TRAN_TRANCARE_PROP_WATER_DET:
		if(IS_ERR_OR_NULL(tc_wd_dev)) {
			*val = 0;
			pr_err("%s:can not get tc_wd_dev device!\n", __func__);
		} else {
			tran_dev_get_prop(tc_wd_dev, TRAN_PROP_WATER_DET_RESULT, &tran_val);
			*val = tran_val.intval;
		}
		break;
	case TRAN_TRANCARE_PROP_AICHG_EN:
		ret = tran_dev_get_prop(tc_chg_dev, TRAN_PROP_GET_AICHG_EN, &tran_val);
		if (ret < 0) {
			pr_err("get aichg_en failed, ret = %d\n", ret);
		}
		*val = tran_val.intval;
		break;
	case TRAN_TRANCARE_PROP_HIGH_TEMP_DISCHG:
		ret = tran_dev_get_prop(tc_chg_dev, TRAN_PROP_HIGH_TEMP_DISCHG, &tran_val);
		if (ret < 0) {
			pr_err("get high_temp_dischg failed, ret = %d\n", ret);
		}
		*val = tran_val.intval;
		break;
	case TRAN_TRANCARE_PROP_LOW_TEMP_DISCHG:
		ret = tran_dev_get_prop(tc_chg_dev, TRAN_PROP_LOW_TEMP_DISCHG, &tran_val);
		if (ret < 0) {
			pr_err("get low_temp_dischg failed, ret = %d\n", ret);
		}
		*val = tran_val.intval;
		break;
	case TRAN_TRANCARE_PROP_R_CABLE:
		if (pe5_info && pe5_info->data && rfc_is_running(pe5_alg))
			*val = pe5_info->data->r_cable;
		else
			*val = TC_TRANCARE_GET_ERR;
		break;
	case TRAN_TRANCARE_PROP_TA_POWER:
		ret =tran_dev_get_prop(tc_chg_dev, TRAN_PROP_TC_CHG_TYPE, &tran_val);
		if (ret < 0) {
			pr_err("get ibat failed, ret = %d\n", ret);
		}
		chg_type = tran_val.intval;
		if(chg_type == TRAN_USB_TYPE_SDP) {
			*val = TRAN_CHG_POWER_2P5W;
		} else if(chg_type == TRAN_USB_TYPE_CDP) {
			*val = TRAN_CHG_POWER_7P5W;
		} else if(chg_type == TRAN_USB_TYPE_NONSTAND) {
			*val = TRAN_CHG_POWER_6W;
		} else if(chg_type == TRAN_USB_TYPE_DCP) {
			*val = TRAN_CHG_POWER_10W;
		} else if(chg_type == TRAN_USB_TYPE_RFC) {
			if(pe5_alg && rfc_is_running(pe5_alg)) {
				tchg_alg_get_prop(pe5_alg, ALG_ADAPTER_CAPACITY, &value);
				*val = value;
			} else {
				*val = TC_TRANCARE_GET_ERR;
			}
		} else if(chg_type == TRAN_USB_TYPE_WIRELESS_BPP) {
			*val = TRAN_CHG_POWER_5W;
		} else if(chg_type == TRAN_USB_TYPE_WIRELESS_EPP) {
			*val = TRAN_CHG_POWER_15W;
		} else if(chg_type == TRAN_USB_TYPE_WIRELESS_PE50) {
			if(pe5_alg && rfc_is_running(pe5_alg)) {
				tchg_alg_get_prop(pe5_alg, ALG_ADAPTER_CAPACITY, &value);
				*val = value;
			} else
				*val = TC_TRANCARE_GET_ERR;
		} else if(chg_type == TRAN_USB_TYPE_PDC) {
			if(pdc_alg && tchg_alg_is_algo_running(pdc_alg)) {
				tchg_alg_get_prop(pdc_alg, ALG_ADAPTER_CAPACITY, &value);
				*val = value;
			} else
				*val = TC_TRANCARE_GET_ERR;
		} else if(chg_type == TRAN_USB_TYPE_PD_PPS){
			if(pe5_alg && rfc_is_running(pe5_alg)){
				tchg_alg_get_prop(pe5_alg, ALG_ADAPTER_CAPACITY, &value);
				*val = value;
			} else
				*val = TC_TRANCARE_GET_ERR;
		} else if((chg_type == TRAN_USB_TYPE_TC30) || (chg_type == TRAN_USB_TYPE_HVDCP)
			|| (chg_type == TRAN_USB_TYPE_PE4) || (chg_type == TRAN_USB_TYPE_PE2) || (chg_type == TRAN_USB_TYPE_PE)) {
			*val = TRAN_CHG_POWER_18W;
		} else {
			*val = TRAN_CHG_POWER_0W;
		}
		break;
	case TRAN_TRANCARE_PROP_WLS_TRAN_DEVICES:
		ret = tran_dev_get_prop(tc_wireless, TRAN_PROP_WLS_TRAN_DEVICES, &tran_val);
		if (ret < 0) {
			pr_err("get wls_tran_devices failed, ret = %d\n", ret);
		}
		*val = tran_val.intval;
		//pr_err("%s:TRAN_TRANCARE_PROP_WLS_TRAN_DEVICES:%d!\n", __func__, *val);
		break;
	case TRAN_TRANCARE_PROP_PE50_LMT_MODULE:
		if (pe5_alg && rfc_is_running(pe5_alg) && pe5_info && pe5_info->data 
			&& pe5_info->data->is_dvchg_en[PE50_DVCHG_MASTER]) {
			pe50_get_curlmt_module(pe5_alg, val);
		} else {
			*val = 0;
		}
		break;
	case TRAN_TRANCARE_PROP_SCREEN_ON:
		if (g_trancare_info)
			*val = g_trancare_info->screen_on;
		else
			*val = 0;
		break;
    default:
		*val = 0;
        break;
    }
}

static int tc_chg_trancare_send_data(void)
{
	struct athena_msg *msg = NULL;
	int value;
	int i,j;
	int curlmt_module[PE50_CURLMT_MODULE_MAX] = {0};
	bool pe50_lmt_module_flag = false;

	if (g_trancare_info == NULL)
		return -EINVAL;

	msg = alloc_msg();
	if (msg == NULL) {
		pr_err("%s:alloc_msg failed!\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&g_trancare_info->data_send_lock);

	for(i = 0; i < ARRAY_SIZE(tc_trancare_chg_array); i++) {
		tc_chg_trancare_data_get(tc_trancare_chg_array[i].prop, &value);
		msg_put_int(&msg, tc_trancare_chg_array[i].name, value);
	}

	for(i = 0; i < ARRAY_SIZE(tc_trancare_chg_array_char); i++) {
		if(tc_trancare_chg_array_char[i].prop == TRAN_TRANCARE_PROP_CHG_TYPE) {
			tc_chg_trancare_data_get(tc_trancare_chg_array_char[i].prop, &value);
			if (value >= 0 && value < ARRAY_SIZE(tran_trancare_usb_type_text)) {
				msg_put_string(&msg, tc_trancare_chg_array_char[i].name, tran_trancare_usb_type_text[value]);
			}
		} else if(tc_trancare_chg_array_char[i].prop == TRAN_TRANCARE_PROP_PE50_LMT_MODULE) {
				tc_chg_trancare_data_get(TRAN_TRANCARE_PROP_PE50_LMT_MODULE, curlmt_module);
				for(j = 0; j < PE50_CURLMT_MODULE_MAX; j++) {
					if(curlmt_module[j] != 0) {
						if (curlmt_module[j] > 0 && curlmt_module[j] < ARRAY_SIZE(pe50_curlmt_module_text)) {
							msg_put_string(&msg, tc_trancare_chg_array_char[i].name, pe50_curlmt_module_text[curlmt_module[j]]);
							pe50_lmt_module_flag = true;
						}
					}
				}
				if(!pe50_lmt_module_flag) {
					msg_put_string(&msg, tc_trancare_chg_array_char[i].name, pe50_curlmt_module_text[0]);
				}
		}
	}
	submit_msg(&msg, 1, TRANSCARE_TID, "device_chg_chg");
	free_msg(&msg);

	mutex_unlock(&g_trancare_info->data_send_lock);

	return 0;
}

static int tc_chg_trancare_reserve_send_data(void)
{
	struct athena_msg *msg = NULL;
	int value;
	int i;

	msg = alloc_msg();
	if (msg == NULL)
		return -EINVAL;

	for(i = 0; i < ARRAY_SIZE(tc_trancare_reserve_chg_array); i++) {
		tc_chg_trancare_data_get(tc_trancare_reserve_chg_array[i].prop, &value);
		msg_put_int(&msg, tc_trancare_reserve_chg_array[i].name, value);
	}
	submit_msg(&msg, 1, TRANSCARE_TID, "device_chg_chg");
	free_msg(&msg);

	return 0;
}

/*tranlog*/
static void tc_upload_battery_health_percent_msg(void)
{
	struct athena_msg *msg = NULL;
	struct tran_device *tc_chg_dev = tran_get_by_name("tc_charger");
	union com_propval tran_val = {0, };
	int bat_health_percent, ret;

	pr_info("%s: enter\n", __func__);
	msg = alloc_msg();
	if (msg == NULL || tc_chg_dev == NULL)
		return;

	ret = tran_dev_get_prop(tc_chg_dev, TRAN_PROP_BAT_HEALTH_PERCENT, &tran_val);
	if (ret < 0) {
		pr_err("get bat health percent failed, ret = %d\n", ret);
	}
	bat_health_percent = tran_val.intval;

	pr_info("%s:health percent = %d\n",__func__, bat_health_percent);
	tranlog_local("BSP_CHARGER", "{\"battery_health\":\"%d\"}", bat_health_percent);

    free_msg(&msg);
}

static int tc_upload_tranlog_vbus_check(void)
{
	int vbus;

	vbus = tc_get_vbus();
	if(vbus < 3500) {
		//pr_info("%s:vbus = %d\n",__func__, vbus);
		tranlog_local("USD_HW_CHG_CHECK", "{\"HW_CHG:VBUS HW is Fail\":\"%d\"}", vbus);
	}
	pr_info("%s:vbus = %d\n",__func__, vbus);

	return 0;
}

int tc_upload_tranlog_chgic_check(void)
{
	struct tran_device *tc_chg_dev = tran_get_by_name("tc_charger");

	if (IS_ERR_OR_NULL(tc_chg_dev)) {
		tranlog_local("USD_HW_CHG_CHECK", "{\"HW_CHG:CHG_IC:-1,HW is Fail\"}");
		return -ENODEV;
	}

	return 0;
}

static int tc_upload_tranlog_cp_check(void)
{
	struct tc_charger *info = NULL;
	struct tran_device * tc_charger_dev = NULL;

	tc_charger_dev = tran_get_by_name("tc_charger");
	if (!tc_charger_dev) {
		tchr_err("can't find tc_charger_dev ***\n");
		return -ENODEV;
	}

	info = tran_get_data(tc_charger_dev);

	if (info->desc->ftm_cp_num >= 1 && get_charger_by_name("primary_dvchg") == NULL){
		goto out;
	}
	if (info->desc->ftm_cp_num >= 2 && get_charger_by_name("secondary_dvchg") == NULL){
		goto out;
	}
	if (info->desc->ftm_cp_num >= 3 && get_charger_by_name("third_divider_chg") == NULL){
		goto out;
	}

	return 0;
out:
	tranlog_local("USD_HW_CHG_CHECK", "{\"HW_CHG:CHG_PUMP_IC:-1,HW is Fail\"}");
	return -1;
}

static int tc_upload_tranlog_typcic_check(void)
{
	struct tran_device *tc_tcpc_dev = tran_get_by_name("tc_tcpc");

	if (IS_ERR_OR_NULL(tc_tcpc_dev)) {
		tranlog_local("USD_HW_CHG_CHECK", "{\"HW_CHG:TYPEC_IC:-1,HW is Fail\"}");
		return -ENODEV;
	}

	return 0;
}

static void tc_upload_hw_check_msg(void)
{
	//pr_info("%s: enter\n", __func__);
	tc_upload_tranlog_vbus_check();
	//tc_upload_tranlog_chgic_check();
	tc_upload_tranlog_cp_check();
	tc_upload_tranlog_typcic_check();
}

static int tc_chg_tranlog_send_data(void)
{
	tc_upload_battery_health_percent_msg();
	tc_upload_hw_check_msg();

	return 0;
}

static int tc_trancare_set_property(struct tran_device *dev,
				enum tran_common_prop prop,
				const union com_propval *val)
{
	int ret;

	switch (prop) {
		case TRAN_PROP_TRANCARE_SEND_DATA:
			ret = tc_chg_trancare_send_data();
			break;
		case TRAN_PROP_TRANCARE_RESERVE_SEND_DATA:
			ret = tc_chg_trancare_reserve_send_data();
			break;
		case TRAN_PROP_TRANLOG_SEND_DATA:
			ret = tc_chg_tranlog_send_data();
			break;
		default:
			ret = -EINVAL;
	}

	return ret;
}

static int tc_trancare_get_property(struct tran_device *dev,
					   enum tran_common_prop prop,
					   union com_propval *val)
{
	//no need
	return 0;
}

static struct tran_ops tc_trancare_ops = {
	.get_prop = tc_trancare_get_property,
	.set_prop = tc_trancare_set_property,
};

static int tc_trancare_prop_init(struct tc_trancare_info *info)
{
	info->tc_trancare_props.alias_name = "tc_trancare";
	info->tc_trancare_dev = tran_device_register("tc_trancare",
						info->dev, info,
						&tc_trancare_ops,
						&info->tc_trancare_props);
	if (IS_ERR_OR_NULL(info->tc_trancare_dev))
		return -ENODEV;

	return 0;
}

static int trancare_screen_notifier_callback(struct notifier_block *nb,
                    unsigned long event, void *s)
{
	struct tc_trancare_info *info = container_of(nb,
			struct tc_trancare_info, tc_trancare_notifier);

	pr_info("%s: enter\n", __func__);
	switch (event) {
	case TRAN_DEV_NOTIFY_SCREEN_OFF:
		pr_info("%s: screen off\n", __func__);
		info->screen_on = false;
		break;
	case TRAN_DEV_NOTIFY_SCREEN_ON:
		pr_info("%s: screen on\n", __func__);
		info->screen_on = true;
		break;
	default:
		break;
	}

	return 0;
}

static int trancare_screen_notifier_init(struct tc_trancare_info *info)
{
	int ret = 0;

	if(!info) {
		pr_err("%s: tc_trancare_info is NULL\n", __func__);
		return -EINVAL;
	}

	info->tc_lcd = tran_get_by_name("tc_lcd");
	if (IS_ERR_OR_NULL(info->tc_lcd)) {
		pr_err("%s: get tc_lcd_dev fail\n", __func__);
		return -ENODEV;
	}

	info->tc_trancare_notifier.notifier_call = trancare_screen_notifier_callback;
	ret = register_tran_device_notifier(info->tc_lcd,
				&info->tc_trancare_notifier);
	if (ret != 0) {
		pr_err("%s: register lcd notify failed, ret = %d\n", __func__, ret);
	}

	return ret;
}

static int tc_trancare_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tc_trancare_info *info = NULL;

	pr_info("%s\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		ret = -ENOMEM;
        dev_err(&pdev->dev, "Failed to allocate memory\n");
		return ret;
	}

	g_trancare_info = info;

	info->dev = &pdev->dev;

	platform_set_drvdata(pdev, info);

	mutex_init(&info->data_send_lock);

	ret = tc_trancare_prop_init(info);
	if (ret < 0) {
		dev_err(info->dev, "tc trancare prop init failed!\n");
		return ret;
	}

	ret = trancare_screen_notifier_init(info);
	if (ret < 0) {
		dev_err(info->dev, "trancare screen notifier init failed!\n");
		return ret;
	}

	g_trancare_info = info;

	pr_info("%s done\n", __func__);
	return 0;
}

static int tc_trancare_prepare_suspend(struct device *dev)
{
	struct tc_trancare_info *info = dev_get_drvdata(dev);

	if (info == NULL) {
		pr_err("%s: info is null\n", __func__);
		return 0;
	}
	pr_info("%s\n", __func__);

	mutex_lock(&info->data_send_lock);

	return 0;
}

static void tc_trancare_complete_resume(struct device *dev)
{
	struct tc_trancare_info *info = dev_get_drvdata(dev);

	if (info == NULL) {
		pr_err("%s: info is null\n", __func__);
		return;
	}

	pr_info("%s\n", __func__);

	mutex_unlock(&info->data_send_lock);

}

static const struct dev_pm_ops tc_trancare_pm_ops = {
	.prepare	= tc_trancare_prepare_suspend,
	.complete       = tc_trancare_complete_resume,
};

static const struct of_device_id tc_trancare_match[] = {
	{ .compatible = "tc,trancare", },
	{ },
};

static void tc_trancare_remove(struct platform_device *pdev)
{
	return;
}

static struct platform_driver tran_trancare_driver = {
	.probe = tc_trancare_probe,
	.remove = tc_trancare_remove,
	.driver = {
		.name = "tc_trancare",
		.owner = THIS_MODULE,
		.pm = &tc_trancare_pm_ops,
		.of_match_table = tc_trancare_match,
	},
};

static s32 __init tc_trancare_det_init(void)
{
	return platform_driver_register(&tran_trancare_driver);
}

static void __exit tc_trancare_det_exit(void)
{
	platform_driver_unregister(&tran_trancare_driver);
}

late_initcall(tc_trancare_det_init);
module_exit(tc_trancare_det_exit);

MODULE_DESCRIPTION("mtk-trancare");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");
