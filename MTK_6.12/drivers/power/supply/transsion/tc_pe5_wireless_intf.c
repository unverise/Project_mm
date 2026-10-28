#include "tc_pe5.h"


void pe50_init_wireless_algo_data(struct pe50_algo_info *info)
{
	struct pe50_algo_data *data = info->data;
	struct pe50_algo_desc *desc = info->desc;
	struct pe50_ta_auth_data *auth_data = &data->ta_auth_data[data->running_spec];
	u32 *rcable_level = desc->rcable_level;
	u32 *ita_level = desc->ita_level;

	data->ita_lmt = min(desc->wireless_curlmt[data->running_spec],
			    (u32)auth_data->ita_max);
	data->idvchg_ss_init = max(data->idvchg_ss_init,
				   (u32)auth_data->ita_min);
	data->idvchg_ss_init = min(data->idvchg_ss_init, data->ita_lmt);
	data->ita_pwr_lmt = 0;
	data->idvchg_cc = ita_level[PE50_RCABLE_NORMAL];
	data->idvchg_term = desc->idvchg_term;
	data->err_retry_cnt = 0;
	data->is_swchg_en = false;
	data->is_dvchg_en[PE50_DVCHG_MASTER] = false;
	data->is_dvchg_en[PE50_DVCHG_SLAVE] = false;
	data->suspect_ta_cc = false;
	data->aicr_setting = 0;
	data->ichg_setting = 0;
	data->pe50_vta_init = max_t(u32, desc->pe50_wireless_vta_init, auth_data->vta_min);
	data->pe50_ita_init = min_t(u32, desc->pe50_wireless_ita_init, auth_data->ita_max);
	data->vta_setting = data->pe50_vta_init;
	data->ita_setting = data->pe50_ita_init;
	data->full_power_flag = false;
	data->ita_gap_per_vstep = 0;
	data->ita_gap_window_idx = 0;
	memset(data->ita_gaps, 0, sizeof(data->ita_gaps));
	data->is_vbat_over_cv = false;
	data->ignore_ibusucpf = false;
	data->force_ta_cv = false;
	data->vbat_cv = desc->vbat_cv;
	data->vbat_cv_no_ircmp = desc->vbat_cv;
	data->cv_lower_bound = desc->vbat_cv - PE50_CV_LOWER_BOUND_GAP;
	data->vta_comp = 0;
	data->zcv = 0;
	data->r_bat = desc->ircmp_rbat;
	data->r_sw = desc->rsw_min;
	data->r_cable = rcable_level[PE50_RCABLE_NORMAL];
	data->r_cable_by_swchg = rcable_level[PE50_RCABLE_NORMAL];
	data->chg_time_max = desc->wireless_chg_time_max;
	data->tbat_level = PE50_THERMAL_NORMAL;
	data->tta_level = PE50_THERMAL_NORMAL;
	data->tdvchg_level = PE50_THERMAL_NORMAL;
	data->tswchg_level = PE50_THERMAL_NORMAL;
	data->tpcb_level = PE50_THERMAL_NORMAL;
	data->tpa_level = PE50_THERMAL_NORMAL;
	data->sys_power_level = PE50_THERMAL_NORMAL;
	if(auth_data->pdp >= desc->pe50_wireless_pdp_limit && desc->support_wireless_pdp_limit){
	memcpy(data->tbat_level_def, desc->wireless_tbat_level_def_h,
			sizeof(desc->wireless_tbat_level_def_h));
	memcpy(data->tpcb_level_def, desc->wireless_tpcb_level_def_h,
			sizeof(desc->wireless_tpcb_level_def_h));
	memcpy(data->tpa_level_def, desc->wireless_tpa_level_def_h,
			sizeof(desc->wireless_tpa_level_def_h));
	memcpy(data->tbat_curlmt, desc->wireless_tbat_curlmt_h,
			sizeof(desc->wireless_tbat_curlmt_h));
	memcpy(data->tpcb_curlmt, desc->wireless_tpcb_curlmt_h,
			sizeof(desc->wireless_tpcb_curlmt_h));
	memcpy(data->tpa_curlmt, desc->wireless_tpa_curlmt_h,
			sizeof(desc->wireless_tpa_curlmt_h));
		PE50_INFO("pdp limit = %d\n",auth_data->pdp);
	}else{
	memcpy(data->tbat_level_def, desc->wireless_tbat_level_def,
			sizeof(desc->wireless_tbat_level_def));
	memcpy(data->tpcb_level_def, desc->wireless_tpcb_level_def,
			sizeof(desc->wireless_tpcb_level_def));
	memcpy(data->tpa_level_def, desc->wireless_tpa_level_def,
			sizeof(desc->wireless_tpa_level_def));
	memcpy(data->tbat_curlmt, desc->wireless_tbat_curlmt,
			sizeof(desc->wireless_tbat_curlmt));
	memcpy(data->tpcb_curlmt, desc->wireless_tpcb_curlmt,
			sizeof(desc->wireless_tpcb_curlmt));
	memcpy(data->tpa_curlmt, desc->wireless_tpa_curlmt,
			sizeof(desc->wireless_tpa_curlmt));
	}
	switch (data->running_spec) {
	case SUPPORT_SPEC_6_2:
		data->conversion_ratio = 3;
		break;
	case SUPPORT_SPEC_4_1:
		data->conversion_ratio = 4;
		break;
	case SUPPORT_SPEC_4_2:
	case SUPPORT_SPEC_2_1:
		data->conversion_ratio = 2;
		break;
	case SUPPORT_SPEC_1_1:
		data->conversion_ratio = 1;
		break;
	default:
		data->conversion_ratio = 2;
		break;
	}

	data->vbat_step_cc = 0;
	data->vbat_step_cv = 0;
	data->step_chg_index = PE50_VBAT_FFC_LOW;
	data->bat_ffc_cv = 0;
	data->bat_ffc_eoc = 0;
	data->step_ffc_level = -1;

	data->vbat_master_step_cv = 0;
	data->vbat_master_step_cc = 0;
	data->step_master_chg_index = PE50_VBAT_FFC_LOW;
	data->bat_master_ffc_cv = 0;
	data->bat_master_ffc_eoc = 0;
	data->step_master_ffc_level = -1;

	data->vbat_slave_step_cv = 0;
	data->vbat_slave_step_cc = 0;
	data->step_slave_chg_index = PE50_VBAT_FFC_LOW;
	data->bat_slave_ffc_cv = 0;
	data->bat_slave_ffc_eoc = 0;
	data->step_slave_ffc_level = -1;

	data->dynamic_sys_power = 1;
	data->pwr_ratio = 100;
	data->pe50_over_power = false;
	data->battery_cycle = tc_get_battery_cycle();

	data->run_once = true;
	mutex_lock(&data->notify_lock);
	data->notify = 0;
	mutex_unlock(&data->notify_lock);
	data->stime = ktime_get_boottime();
	memcpy(data->run_cp_cur, desc->run_wls_cp_cur, sizeof(data->run_cp_cur));
	memcpy(data->run_cp_recovery_gap, desc->run_wls_cp_cur, sizeof(data->run_cp_recovery_gap));
}

//zhushaoan 加一个setting和measure的值的最大diff值
int pe50_wireless_get_ita_lmt(struct pe50_algo_info *info)
{
	struct pe50_algo_data *data = info->data;
	struct pe50_algo_desc *desc = info->desc;
	struct pe50_ta_auth_data *auth_data = &data->ta_auth_data[data->running_spec];
	u32 ita = data->ita_lmt;
	int bypass_level=0;
	int ita_min;
	int dvchg_num;

	mutex_lock(&data->ext_lock);

	/* get dvchg online number */
	dvchg_num = pe50_dvchg_online_cnt(info);

	/* select idvchg current lmt by dvchg_num */
	ita = min(ita, desc->idvchg_level_multi[dvchg_num - 1]);

	/* compare with auth ita_max */
	ita = min_t(u32, ita, auth_data->ita_max);

	if (data->ita_pwr_lmt > 0)
		ita = min(ita, data->ita_pwr_lmt);

	if (data->vbat_step_cc > 0)     
		ita = min_t(u32, ita, data->vbat_step_cc / data->conversion_ratio);

	ita = min(ita, data->ita_lmt - desc->tta_curlmt[data->tta_level]);

	ita = min(ita, data->ita_lmt - desc->tdvchg_curlmt[data->tdvchg_level]);
	/* compare with battery & pcb &pa temp current limit */
	ita = min(ita, data->ita_lmt - data->tbat_curlmt[data->tbat_level]);
	ita = min(ita, data->ita_lmt - data->tpcb_curlmt[data->tpcb_level]);
	ita = min(ita, data->ita_lmt - data->tpa_curlmt[data->tpa_level]);

	/* sys power limit*/
	if (data->screen_on && data->state == PE50_ALGO_CC_CV &&
		desc->sys_power_curlmt[data->sys_power_level] > 0)
		ita = min_t(u32, ita, desc->sys_power_curlmt[data->sys_power_level]);

	if (data->state == PE50_ALGO_CC_CV) {
		bypass_level = pe5_hal_get_bypass_energy(info);
		ita = min_t(u32, ita, percent(data->ita_lmt, bypass_level));
	}

	ita_min = data->idvchg_term + percent(data->ita_lmt, 5) + desc->ita_lmt_gap;
	ita = max_t(u32, ita, ita_min);

	/* bat self repair current limit */
	if (data->ibat_limt > 0)
		ita = min_t(u32, ita, data->ibat_limt);

	PE50_INFO("ita(org,tta,tbat,tdvchg,tpcb,tpa,syspower,prlmt,step_cc,ita_min,idvchg_num,ta_imax,ibat_limt)="
		"%d(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)\n",
		 ita, data->ita_lmt, desc->tta_curlmt[data->tta_level],
		 data->tbat_curlmt[data->tbat_level],
		 desc->tdvchg_curlmt[data->tdvchg_level],
		 data->tpcb_curlmt[data->tpcb_level],
		 data->tpa_curlmt[data->tpa_level],
		 desc->sys_power_curlmt[data->sys_power_level],
		 data->ita_pwr_lmt, data->vbat_step_cc / data->conversion_ratio, ita_min,
		 desc->idvchg_level_multi[dvchg_num - 1], auth_data->ita_max,
		 data->ibat_limt);

	mutex_unlock(&data->ext_lock);
	return ita;
}


static inline int pe50_wireless_get_idvchg_lmt(struct pe50_algo_info *info)
{
	u32 ita_lmt, idvchg_lmt;
	struct pe50_algo_data *data = info->data;

	ita_lmt = pe50_wireless_get_ita_lmt(info);
	idvchg_lmt = min(data->idvchg_cc, ita_lmt);
	PE50_INFO("idvchg_lmt(ita_lmt, idvchg_cc)=%d(%d,%d)\n",
		idvchg_lmt, ita_lmt, data->idvchg_cc);
	return idvchg_lmt;
}

int pe50_is_wireless_online(struct pe50_algo_info *info)
{
	int ret=0;
	struct power_supply *psy;
	union power_supply_propval prop;
	psy = power_supply_get_by_name("wireless");
	if (!psy) {
		PE50_ERR("%s: get wireless power supply failed\n", __func__);
		return  -ENODEV;
	}

	ret = power_supply_get_property(psy,POWER_SUPPLY_PROP_ONLINE, &prop);
	if (ret < 0) {
		PE50_ERR("get wireless online fail(%d)\n", ret);
		return  -EINVAL;
	}

	if (!prop.intval) {
		PE50_ERR("wireless already plug out\n");
	}

	return prop.intval;
}

static int wireless_enable_master_dvchg(struct pe50_algo_info *info)
{
	struct pe50_algo_data *data = info->data;
	bool wireless_online;
	int repeat_cnt = 50;
	int ret = 0;
	int i;

	ret = pe50_hal_set_ibusucp_enable(info->alg, DVCHG1, false);
	if (ret < 0) {
		PE50_ERR("wireless disable ibus ucp failed, ret:%d\n", ret);
		goto out;
	}

	ret = pe50_set_dvchg_charging(info, true);
	if (ret < 0) {
		PE50_ERR("en dvchg fail\n");
		goto out;
	}

	for (i = 0; i <= repeat_cnt; i++) {
		if (atomic_read(&data->stop_algo)) {
			ret = -EINVAL;
			PE50_INFO("stop algo\n");
			goto out;
		}

		wireless_online = pe50_is_wireless_online(info);
		if (!wireless_online) {
			ret = -EINVAL;
			PE50_ERR("wireless already plug out\n");
			goto out;
		}

		ret = pe50_get_ta_cap_by_supportive(info,
			&data->vta_measure, &data->ita_measure);
		if (ret < 0) {
			PE50_ERR("read CP ibus failed\n");
			goto out;
		}

		if (data->ita_measure >= 600)
			 break;

		msleep(20);
	}

	ret = pe50_hal_set_ibusucp_enable(info->alg, DVCHG1, true);
	if (ret < 0) {
		PE50_ERR("wireless enable ibus ucp failed");
		goto out;
	}

	return ret;
out:
	pe50_hal_set_ibusucp_enable(info->alg, DVCHG1, true);
	return ret;
}

int pe50_wireless_algo_init(struct pe50_algo_info *info)
{
	u32 vta;
	int ret, i, vbus, vbat, vout;
	int vbat_avg = 0;
	int wireless_cnt = 50;
	int wireless_online = 0;
	const int avg_times = 10;
	struct pe50_algo_data *data = info->data;
	struct pe50_algo_desc *desc = info->desc;
	struct pe50_ta_auth_data *auth_data = &data->ta_auth_data[data->running_spec];
	struct pe50_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PE50_DBG("++\n");

	/* Set DVCHG running mode */
	for (i = PE50_DVCHG_MASTER; i < PE50_DVCHG_MAX; i++) {
		if (!data->is_dvchg_exist[i])
			continue;
		ret = pe50_hal_set_run_spec(info->alg, to_chgidx(i), data->running_spec);
		if (ret < 0) {
			PE50_ERR("(%s) set run spec fail(%d)\n",
				pe50_dvchg_role_name[i], ret);
			return ret;
		}
	}

	ret = pe50_enable_ta_charging(info, true, data->pe50_vta_init, data->pe50_ita_init);
	if (ret < 0) {
		PE50_ERR("enable ta charge fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}

	for (i = 0; i < avg_times; i++) {
		ret = pe50_get_adc(info, PE50_ADCCHAN_VBAT, &vbat);
		if (ret < 0) {
			PE50_ERR("get vbus fail(%d)\n", ret);
			goto err;
		}
		vbat_avg += vbat;
	}
	vbat_avg = precise_div(vbat_avg, avg_times);
	data->zcv = vbat_avg;
	PE50_INFO("avg(vbat):(%d)\n", vbat_avg);

	if (vbat_avg >= desc->start_vbat_max) {
		PE50_INFO("finish PE5.0, vbat(%d) > %d\n", vbat_avg,
			  desc->start_vbat_max);
		goto out;
	}

	if (desc->wireless_en_dual) {
		ret=pe50_enable_special_function(info->alg,HVCHG,HVCHG_CP_MODE_INIT);
		if (ret < 0) {
			PE50_ERR("set hvchg function fail(%d)\n", ret);
		}
	}

	if(data->running_spec <= SUPPORT_SPEC_4_2){
		for (i = PE50_DVCHG_MASTER; i < PE50_DVCHG_MAX; i++) {
		if (!data->is_dvchg_exist[i])
			continue;
		ret = pe50_enable_special_function(info->alg,to_chgidx(i),HVCHG_CP_WIRELESS_MODE_INIT);
		if (ret < 0) {
			PE50_ERR("(%s) set cp special fail(%d)\n",
				pe50_dvchg_role_name[i], ret);
			return ret;
		}
		}
	}
/*
	ret = pe50_hal_set_aicr(info->alg, true, 300);
	if (ret < 0) {
		PE50_ERR("set aicr fail(%d)\n", ret);
		return ret;
	}

	ret = pe50_hal_set_ichg(info->alg, true, 800);
	if (ret < 0) {
		PE50_ERR("set ichg fail(%d)\n", ret);
		return ret;
	}

	ret = pe50_hal_enable_charging(info->alg, CHG1, true);
	if (ret < 0) {
		PE50_ERR("enable charger fail\n");
		return ret;
	}
*/
	ret = pe50_hal_enable_hz(info->alg,true);
	if(ret < 0){
		PE50_ERR("set hiz fail(%d)\n", ret);
		return ret;
	}

	ret = pe50_set_ta_cap_cv(info, data->pe50_vta_init, 1000);
	if (ret < 0) {
		PE50_ERR("set ta cap fail(%d)\n", ret);
		return ret;
	}

	if (desc->wireless_en_dual) {
		ret = pe50_set_ta_cap_cv(info, PE50_WIRELESS_INIT_VTA_HV, 1000);
		if (ret < 0) {
			PE50_ERR("set ta cap fail(%d)\n", ret);
			return ret;
		}

		ret = pe50_hal_sync_ta_volt(info->alg, PE50_WIRELESS_INIT_VTA_HV + 500);
		if (ret < 0) {
			PE50_ERR("set ta cap fail(%d)\n", ret);
		}
	} else {
		ret = pe50_set_ta_cap_cv(info, PE50_WIRELESS_INIT_VTA, 1000);
		if (ret < 0) {
		        PE50_ERR("set ta cap fail(%d)\n", ret);
		        return ret;
		}
	}

	msleep(500); /* Wait current stable */

	ret = pe50_get_adc(info, PE50_ADCCHAN_VBUS, &vbus);
	if (ret < 0) {
		PE50_ERR("get vbus fail(%d)\n", ret);
		goto err;
	}

	ret = pe50_hal_sync_ta_volt(info->alg, vbus);
	if (ret < 0 && ret != -EOPNOTSUPP) {
		PE50_ERR("sync ta setting fail(%d)\n", ret);
		goto err;
	}

	ret = pe50_get_adc(info, PE50_ADCCHAN_VOUT, &vout);
	if (ret < 0) {
		PE50_ERR("get vout fail(%d)\n", ret);
		goto err;
	}

	pe50_enable_special_function(info->alg, HVCHG, HVCHG_EN_CP_MODE);

	vta = pe50_vout2vbus(info, vout);
	ret = pe50_set_ta_cap_cv(info, vta, data->idvchg_ss_init);
	if (ret < 0) {
		PE50_ERR("set ta cap fail(%d)\n", ret);
		goto err;
	}

	while (true) {
		if (atomic_read(&data->stop_algo)) {
			PE50_INFO("stop algo\n");
			goto out;
		}
		wireless_online = pe50_is_wireless_online(info);
		if (!wireless_online) {
			PE50_ERR("wireless already plug out\n");
			goto out;
		}

		msleep(200);
		ret = pe50_get_ta_cap(info);
		if (ret < 0) {
			PE50_ERR("get ta cap fail(%d)\n", ret);
			goto out;
		}

		PE50_ERR("vout = %d vta = %d vbus = %d wireless_cnt = %d\n",
			vout, vta, data->vta_measure, wireless_cnt);

		if (abs(vta - data->vta_measure) < 600 || wireless_cnt == 0)
			break;

		if (data->vta_measure < 2500) {
			PE50_ERR("vbus is too low\n");
			goto out;
		}
		wireless_cnt--;
	}

	ret = wireless_enable_master_dvchg(info);
	if (ret < 0) {
		PE50_ERR("en dvchg fail\n");
		goto out;
	}

	/* Get ita measure after enable dvchg */
	ret = pe50_get_ta_cap_by_supportive(info, &data->vta_measure,
					    &data->ita_measure);
	if (ret < 0) {
		PE50_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}
	data->err_retry_cnt = 0;
	data->state = PE50_ALGO_MEASURE_R;
	return 0;
err:
	if (data->err_retry_cnt < PE50_INIT_RETRY_MAX) {
		data->err_retry_cnt++;
		return 0;
	}
out:
	return pe50_stop(info, &sinfo);
}

int pe50_wireless_algo_measure_r(struct pe50_algo_info *info)
{
	struct pe50_algo_data *data = info->data;

	PE50_DBG("++\n");

	/*
	 * Ignore measuring r,
	 * treat as normal cable if meas_cap is not supported
	 */

	data->err_retry_cnt = 0;
	data->state = PE50_ALGO_SS_DVCHG;
	return 0;
}

int pe50_wireless_algo_ss_dvchg(struct pe50_algo_info *info)
{
	int wireless_online = 0;
	bool dvchg_en;
	int ret, vbat;
	int repeat_cnt = 200;
	ktime_t start_time, end_time;
	struct pe50_algo_data *data = info->data;
	struct pe50_algo_desc *desc = info->desc;
	struct pe50_ta_auth_data *auth_data = &data->ta_auth_data[data->running_spec];
	u32 idvchg_lmt, vta, ita, delta_time;
	u32 ita_gap_per_vstep = data->ita_gap_per_vstep > 0 ?
				data->ita_gap_per_vstep :
				auth_data->ita_gap_per_vstep;
	struct pe50_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

repeat:
	PE50_DBG("++\n");
	vta = data->vta_setting;
	start_time = ktime_get();

	ret = pe50_hal_is_chip_enabled(info->alg, DVCHG1, &dvchg_en);
	if (ret < 0 || !dvchg_en) {
		PE50_ERR("get dvchg status fail or dvchg not en(%d, %d)\n",
			ret, dvchg_en);
		goto out;
	}

	wireless_online = pe50_is_wireless_online(info);
	if (!wireless_online) {
		PE50_ERR("wireless already plug out\n");
		goto out;
	}

	ret = pe50_select_vbat_cv(info);
	if (ret < 0) {
		PE50_ERR("select vbat cv fail(%d)\n", ret);
		goto out;
	}

	ret = pe50_get_adc(info, PE50_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE50_ERR("get vbat fail(%d)\n", ret);
		goto out;
	}

	ret = pe50_get_ta_cap_by_supportive(info, &data->vta_measure,
					    &data->ita_measure);
	if (ret < 0) {
		PE50_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}

	if (abs(vta - data->vta_measure) > PE50_WIRELESS_VTA_MEASURE_GAP) {
		PE50_INFO("low_power vbus  vta = %d vta_measure = %d\n",
			vta, data->vta_measure);
		goto out;
	}

	ret = pe50_algo_multi_dvchg_update(info);
	if (ret < 0) {
		ret = pe50_earily_restart(info);
		if (ret < 0) {
			PE50_ERR("earily restart fail(%d)\n", ret);
			goto out;
		}
		return 0;
	}

	idvchg_lmt = pe50_wireless_get_idvchg_lmt(info);
	if (idvchg_lmt < data->idvchg_term && data->ibat_limt <= 0) {
		PE50_INFO("idvchg_lmt(%d) < idvchg_term(%d)\n", idvchg_lmt,
			 data->idvchg_term);
		goto out;
	}
	ita = idvchg_lmt;

	/* VBAT reaches CV level */
	if (vbat >= data->vbat_cv) {
		if (data->ita_measure < data->idvchg_term) {
			if ((data->ita_measure <= PE50_IBUSUCPF_RECHECK ||
				data->pe50_auto_test_ibusucp) &&
			       	(data->notify & BIT(EVT_IBUSUCP_FALL))) {
				pe50_ab_ibusucp_stop_algo(info,PE50_IBUS_UCP);
			} else {
				PE50_INFO("finish PE5.0 charging, vbat(%d), ita(%d)\n",
					  vbat, data->ita_measure);
			}
			goto out;
		}
		vta -= auth_data->vta_step;
		data->state = PE50_ALGO_CC_CV;
		goto out_set_cap;
	}

	if (desc->support_dual_battery) {
		if (data->fg_a_vbat >= data->vbat_master_step_cv ||
		    data->fg_b_vbat >= data->vbat_slave_step_cv) {
			PE50_INFO("--vbat(%d, %d) >= vbat_cv(%d, %d)\n",
				data->fg_a_vbat, data->fg_b_vbat,
				data->vbat_master_step_cv, data->vbat_slave_step_cv);
			vta -= auth_data->vta_step;
			data->state = PE50_ALGO_CC_CV;
			goto out_set_cap;
		}

		if (data->fg_a_ibat >= data->vbat_master_step_cc ||
		    data->fg_b_ibat >= data->vbat_slave_step_cc) {
			PE50_INFO("--ibat(%d, %d) >= ibat_cc(%d, %d)\n",
				data->fg_a_ibat, data->fg_b_ibat,
				data->vbat_master_step_cc, data->vbat_slave_step_cc);
			vta -= auth_data->vta_step;
			data->state = PE50_ALGO_CC_CV;
			goto out_set_cap;
		}
	}

	/* IBUS reaches CC level */
	if (data->ita_measure + ita_gap_per_vstep + auth_data->ita_gap_cv_mode > idvchg_lmt ||
	    vta == auth_data->vcap_max || repeat_cnt-- <= 0) {
		data->state = PE50_ALGO_CC_CV;
		ret = pe50_hal_enable_hz(info->alg, true);
		if (ret < 0) {
			PE50_ERR("set swchg hz fail(%d)\n", ret);
			goto out;
		}
	} else {
		vta += auth_data->vta_step;
		vta = min(vta, (u32)auth_data->vcap_max);
	}

out_set_cap:
	ret = pe50_set_ta_cap_cv(info, vta, ita);
	if (ret < 0) {
		PE50_ERR("set ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto out;
	}
	if (data->state == PE50_ALGO_SS_DVCHG) {
		end_time = ktime_get();
		delta_time = ktime_ms_delta(end_time, start_time);
		PE50_DBG("delta time %dms\n", delta_time);
		if (delta_time < desc->ta_cv_ss_repeat_tmin)
			msleep(desc->ta_cv_ss_repeat_tmin - delta_time);
		goto repeat;
	}
	return 0;
out:
	return pe50_stop(info, &sinfo);
}

int pe50_wireless_algo_cc_cv(struct pe50_algo_info *info)
{
	int ret, vbat;
	bool dual_batt_rise = false;
	struct pe50_algo_data *data = info->data;
	struct pe50_algo_desc *desc = info->desc;
	struct pe50_ta_auth_data *auth_data = &data->ta_auth_data[data->running_spec];
	u32 idvchg_lmt, vta = data->vta_setting, ita = data->ita_setting;
	u32 ita_gap_per_vstep = data->ita_gap_per_vstep > 0 ?
				data->ita_gap_per_vstep :
				auth_data->ita_gap_per_vstep;
	u32 vta_measure, ita_measure, suspect_ta_cc = false;
	struct pe50_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PE50_DBG("++\n");

	ret = pe50_get_adc(info, PE50_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE50_ERR("get vbat fail(%d)\n", ret);
		goto out;
	}

	ret = pe50_get_ta_cap_by_supportive(info, &data->vta_measure,
					    &data->ita_measure);
	if (ret < 0) {
		PE50_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}

	if (abs(vta - data->vta_measure) > PE50_WIRELESS_VTA_MEASURE_GAP) {
		PE50_ERR("low_power vta = %d vta_measure = %d\n",
			vta, data->vta_measure);
		goto out;
	}

	pe50_algo_multi_dvchg_update(info);

	if (data->ita_measure <= data->idvchg_term && data->ibat_limt <= 0) {
		if ((data->ita_measure <= PE50_IBUSUCPF_RECHECK ||
			data->pe50_auto_test_ibusucp) &&
		       	(data->notify & BIT(EVT_IBUSUCP_FALL))) {

			pe50_ab_ibusucp_stop_algo(info,PE50_IBUS_UCP);
		} else {
			if (vbat < data->cv_lower_bound)
				goto cc_cv;

			PE50_INFO("finish PE5.0 charging measure = %d term = %d\n",
				data->ita_measure,data->idvchg_term);
			data->pe50_taper_done = true;
		}
		goto out;
	}

cc_cv:
	idvchg_lmt = pe50_wireless_get_idvchg_lmt(info);
	if (idvchg_lmt < data->idvchg_term && data->ibat_limt <= 0) {
		PE50_INFO("idvchg_lmt(%d) < idvchg_term(%d)\n", idvchg_lmt,
			  data->idvchg_term);
		goto out;
	}

	if (desc->support_dual_battery) {
		if (data->fg_a_vbat >= data->vbat_master_step_cv ||
		    data->fg_b_vbat >= data->vbat_slave_step_cv) {
			PE50_INFO("--vbat(%d, %d) >= vbat_cv(%d, %d)\n",
				data->fg_a_vbat, data->fg_b_vbat,
				data->vbat_master_step_cv, data->vbat_slave_step_cv);
			vta -= auth_data->vta_step;
			goto out_set_cap;
		}

		if (data->fg_a_ibat >= data->vbat_master_step_cc ||
		    data->fg_b_ibat >= data->vbat_slave_step_cc) {
			PE50_INFO("--ibat(%d, %d) >= ibat_cc(%d, %d)\n",
				data->fg_a_ibat, data->fg_b_ibat,
				data->vbat_master_step_cc, data->vbat_slave_step_cc);
			vta -= auth_data->vta_step;
			goto out_set_cap;
		}

		if (data->fg_a_ibat <= data->vbat_master_step_cc -
		    desc->step_cc_gap &&
		    data->fg_b_ibat <= data->vbat_slave_step_cc -
		    desc->step_cc_gap) {
			dual_batt_rise = true;
		}
	} else {
		dual_batt_rise = true;
	}

	if (vbat >= data->vbat_cv) {
		PE50_INFO("--vbat >= vbat_cv, %d > %d\n", vbat, data->vbat_cv);
		vta -= min(auth_data->vta_step,desc->pe50_default_vta_step);
		ita -= ita_gap_per_vstep;
		data->is_vbat_over_cv = true;
		goto out_set_cap;
	} else if ((data->ita_measure > (idvchg_lmt + ita_gap_per_vstep + auth_data->ita_gap_cv_mode))) {
		vta -= auth_data->vta_step;
		ita -= ita_gap_per_vstep;
		ita = max(ita, idvchg_lmt);
		PE50_INFO("--vta, ita(meas,lmt)=(%d,%d)\n", data->ita_measure,
			  idvchg_lmt);
		goto out_set_cap;
	} else if (!data->is_vbat_over_cv && vbat <= data->cv_lower_bound &&
		   data->ita_measure <= (idvchg_lmt - ita_gap_per_vstep - auth_data->ita_gap_cv_mode) &&
		   vta < auth_data->vcap_max && !data->suspect_ta_cc && dual_batt_rise) {
		vta += auth_data->vta_step;
		vta = min_t(u32, vta, auth_data->vcap_max);
		ita = idvchg_lmt;
		if (ita == data->ita_setting)
			suspect_ta_cc = true;
		PE50_INFO("++vta, ita(meas,lmt)=(%d,%d)\n", data->ita_measure,
			  idvchg_lmt);
		goto out_set_cap;
	} else if (data->is_vbat_over_cv) {
		data->is_vbat_over_cv = false;
	}

out_set_cap:
	ret = pe50_set_ta_cap_cv(info, vta, ita);
	if (ret < 0) {
		PE50_ERR("set_ta_cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto out;
	}

	ret = pe50_get_ta_cap_by_supportive(info, &vta_measure, &ita_measure);
	if (ret < 0) {
		PE50_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}
	data->suspect_ta_cc = (suspect_ta_cc &&
			       data->ita_measure == ita_measure);
	return 0;
out:
	return pe50_stop(info, &sinfo);
}
