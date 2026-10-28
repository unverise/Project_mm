#include "tc_pe5.h"

static void pe50_init_vol_diff(struct pe50_algo_info *info)
{
	int i;
	struct pe50_algo_data *data = info->data;
	struct pe50_algo_desc *desc = info->desc;
	struct pe50_ta_auth_data *auth_data = &data->ta_auth_data[data->running_spec];

	for (i = PE50_CABLE_CAPABILTY_MAX - 1; i >= 0; i--) {
		if (auth_data->cable_capability >= desc->cable_capability_level[i]) {
			data->vol_diff_max = desc->vol_diff_level[i];
			break;
		}

		if (i == 0) {
			data->vol_diff_max = PE50_DEFAULT_VOL_DIFF;
		}
	}

	if (data->vol_diff_max <= 0) {
		data->vol_diff_max = PE50_DEFAULT_VOL_DIFF;
	}

	PE50_INFO("Update vol_diff_max (%d)\n", data->vol_diff_max);

	return;
}

void pe50_init_rfc_algo_data(struct pe50_algo_info *info)
{
	struct pe50_algo_data *data = info->data;
	struct pe50_algo_desc *desc = info->desc;
	struct pe50_ta_auth_data *auth_data = &data->ta_auth_data[data->running_spec];
	u32 *rcable_level = desc->rcable_level;
	u32 *ita_level = desc->ita_level;
	int chg_speed_temp = data->chg_speed;

	data->ita_lmt = ita_level[PE50_RCABLE_NORMAL];
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
	data->pe50_vta_init = max_t(u32, desc->pe50_vta_init, auth_data->vta_min);
	data->pe50_ita_init = min_t(u32, desc->pe50_ita_init, auth_data->ita_max);
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
	data->chg_time_max = desc->chg_time_max;
	data->tbat_level = PE50_THERMAL_NORMAL;
	data->tta_level = PE50_THERMAL_NORMAL;
	data->tdvchg_level = PE50_THERMAL_NORMAL;
	data->tswchg_level = PE50_THERMAL_NORMAL;
	data->tpcb_level = PE50_THERMAL_NORMAL;
	data->tpa_level = PE50_THERMAL_NORMAL;
	data->sys_power_level = PE50_THERMAL_NORMAL;
	if(!desc->supprot_multi_level_charging) {
		memcpy(data->tbat_level_def, desc->tbat_level_def,
				sizeof(desc->tbat_level_def));
		memcpy(data->tpcb_level_def, desc->tpcb_level_def,
				sizeof(desc->tpcb_level_def));
		memcpy(data->tpa_level_def, desc->tpa_level_def,
				sizeof(desc->tpa_level_def));
		memcpy(data->tbat_curlmt, desc->tbat_curlmt,
				sizeof(desc->tbat_curlmt));
		memcpy(data->tpcb_curlmt, desc->tpcb_curlmt,
				sizeof(desc->tpcb_curlmt));
		memcpy(data->tpa_curlmt, desc->tpa_curlmt,
				sizeof(desc->tpa_curlmt));
	} else {
		if(auth_data->pdp < desc->project_power) {
			PE50_ERR("%s:adapter_capacity < project_power,chg_speed use default:TRAN_MULTI_SPEED_MID\n", __func__);
			chg_speed_temp = TRAN_MULTI_SPEED_MID;
		}

		mutex_lock(&data->chgspeed_lock);
		memcpy(data->tbat_level_def, desc->tc_pe50_multi_level[chg_speed_temp-1].tbat_level_def,
				sizeof(data->tbat_level_def));
		memcpy(data->tpcb_level_def, desc->tc_pe50_multi_level[chg_speed_temp-1].tpcb_level_def,
				sizeof(data->tpcb_level_def));
		memcpy(data->tpa_level_def, desc->tc_pe50_multi_level[chg_speed_temp-1].tpa_level_def,
				sizeof(data->tpa_level_def));
		memcpy(data->tbat_curlmt, desc->tc_pe50_multi_level[chg_speed_temp-1].tbat_curlmt,
				sizeof(data->tbat_curlmt));
		memcpy(data->tpcb_curlmt, desc->tc_pe50_multi_level[chg_speed_temp-1].tpcb_curlmt,
				sizeof(data->tpcb_curlmt));
		memcpy(data->tpa_curlmt, desc->tc_pe50_multi_level[chg_speed_temp-1].tpa_curlmt,
			sizeof(data->tpa_curlmt));
		mutex_unlock(&data->chgspeed_lock);
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

	data->battery_cycle = tc_get_battery_cycle();

	data->dynamic_sys_power = 1;
	data->vta_up_ita_stable_cnt = 0;
	data->ita_meas_lmt = 0;
	data->ita_stable_cnt = desc->rfc_ita_stable_cnt;
	data->pwr_ratio = 100;
	data->pe50_over_power = (auth_data->pdp <= desc->project_power) ? true : false;
	PE50_INFO("rfc idvchg_term=%d idvchg_pps_term=%d, "
		"pe50_over_power:%d, auth_data->vta_max:%d, auth_data->ita_max:%d, desc->project_power:%d\n",
		data->idvchg_term, desc->idvchg_pps_term,
		data->pe50_over_power, auth_data->vta_max, auth_data->ita_max, desc->project_power);

	data->run_once = true;
	mutex_lock(&data->notify_lock);
	data->notify = 0;
	mutex_unlock(&data->notify_lock);
	data->stime = ktime_get_boottime();
	memcpy(data->run_cp_cur, desc->run_cp_cur, sizeof(data->run_cp_cur));
	memcpy(data->run_cp_recovery_gap, desc->run_cp_recovery_gap, sizeof(data->run_cp_recovery_gap));
	pe50_init_vol_diff(info);
}

static int pe50_select_ita_lmt_by_r(struct pe50_algo_info *info, bool dual)
{
	struct pe50_algo_data *data = info->data;
	struct pe50_algo_desc *desc = info->desc;
	struct pe50_ta_auth_data *auth_data = &data->ta_auth_data[data->running_spec];
	u32 ita_lmt_by_r;
	u32 *rcable_level = desc->rcable_level;
	u32 *ita_level = desc->ita_level;

	if (data->ignore_measure_r) {
		ita_lmt_by_r = auth_data->cable_capability;
		goto out;
	}

	if (data->r_cable <= rcable_level[PE50_RCABLE_NORMAL])
		ita_lmt_by_r = ita_level[PE50_RCABLE_NORMAL];
	else if (data->r_cable <= rcable_level[PE50_RCABLE_BAD1])
		ita_lmt_by_r = ita_level[PE50_RCABLE_BAD1];
	else if (data->r_cable <= rcable_level[PE50_RCABLE_BAD2])
		ita_lmt_by_r = ita_level[PE50_RCABLE_BAD2];
	else if (data->r_cable <= rcable_level[PE50_RCABLE_BAD3])
		ita_lmt_by_r = ita_level[PE50_RCABLE_BAD3];
	else {
		PE50_ERR("r_cable(%d) too worse\n", data->r_cable);
		return -EINVAL;
	}

	PE50_ERR("r_cable: %d\n", data->r_cable);
out:
	PE50_INFO("ita limited by r = %d\n", ita_lmt_by_r);
	data->ita_lmt_rcable = min_t(u32, ita_lmt_by_r, data->ita_lmt);
	return 0;
}

int pe50_rfc_get_ita_lmt(struct pe50_algo_info *info)
{
	struct pe50_algo_data *data = info->data;
	struct pe50_algo_desc *desc = info->desc;
	struct pe50_ta_auth_data *auth_data = &data->ta_auth_data[data->running_spec];
	u32 ita = data->ita_lmt;
	int ita_min;
	int ita_min_cnt = 5;
	int bypass_level;
	int dvchg_num;

	mutex_lock(&data->ext_lock);

	/* get dvchg online number */
	dvchg_num = pe50_dvchg_online_cnt(info);

	/* select idvchg current lmt by dvchg_num */
	ita = min(ita, desc->idvchg_level_multi[dvchg_num - 1]);

	/* compare with ita_lmt_rcable */
	ita = min(ita, data->ita_lmt_rcable);

	/* compare with auth ita_max */
	ita = min_t(u32, ita, auth_data->ita_max);

	/* compare with ita_pwr_lmt */
	if (data->ita_pwr_lmt > 0)
		ita = min(ita, data->ita_pwr_lmt);

	/* up v but not increase i */
	if (data->ita_meas_lmt != 0)
		ita = min_t(u32, ita, data->ita_meas_lmt);

	/* compare with dvchg temp current limit */
	ita = min(ita, data->ita_lmt - desc->tdvchg_curlmt[data->tdvchg_level]);

	/* compare with ta temp current limit */
	ita = min(ita, data->ita_lmt - desc->tta_curlmt[data->tta_level]);

	/* compare with battery & pcb &pa temp current limit */
	ita = min(ita, data->ita_lmt - data->tbat_curlmt[data->tbat_level]);
	ita = min(ita, data->ita_lmt - data->tpcb_curlmt[data->tpcb_level]);
	ita = min(ita, data->ita_lmt - data->tpa_curlmt[data->tpa_level]);

	/* sys power limit*/
	if (data->screen_on && data->state == PE50_ALGO_CC_CV &&
		desc->sys_power_curlmt[data->sys_power_level] > 0)
		ita = min_t(u32, ita, desc->sys_power_curlmt[data->sys_power_level]);

	/* compare with vbat FFC param */
	if (data->vbat_step_cc > 0)
		ita = min(ita, data->vbat_step_cc / data->conversion_ratio);

	/* compare with bypass mode current limit */
	if (data->state == PE50_ALGO_CC_CV) {
		bypass_level = pe5_hal_get_bypass_energy(info);
		ita = min_t(u32, ita, percent(data->ita_lmt, bypass_level));
	}

	/* compare with min current limit */
	if(data->running_spec == SUPPORT_SPEC_4_1)
		ita_min_cnt = percent(ita_min_cnt,50);

	ita_min = data->idvchg_term + percent(data->ita_lmt, ita_min_cnt) + desc->ita_lmt_gap;
	ita = max_t(u32, ita, ita_min);

	/* bat self repair current limit */
	if (data->ibat_limt > 0)
		ita = min_t(u32, ita, data->ibat_limt);

	PE50_INFO("ita(org,tta,tbat,tdvchg,tpcb,tpa,sys_power,ita_meas,prlmt,step_cc,ita_min,idvchg_num,ta_imax,rcable,ibat_limt)="
		"%d(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)\n",
		 ita, data->ita_lmt, desc->tta_curlmt[data->tta_level],
		 data->tbat_curlmt[data->tbat_level],
		 desc->tdvchg_curlmt[data->tdvchg_level],
		 data->tpcb_curlmt[data->tpcb_level],
		 data->tpa_curlmt[data->tpa_level],
		 desc->sys_power_curlmt[data->sys_power_level],
		 data->ita_meas_lmt,
		 data->ita_pwr_lmt, data->vbat_step_cc / data->conversion_ratio, ita_min,
		 desc->idvchg_level_multi[dvchg_num - 1], auth_data->ita_max,
		 data->ita_lmt_rcable,
		 data->ibat_limt);

	mutex_unlock(&data->ext_lock);

	return ita;
}

static inline int pe50_rfc_get_idvchg_lmt(struct pe50_algo_info *info)
{
	u32 ita_lmt, idvchg_lmt;
	struct pe50_algo_data *data = info->data;

	ita_lmt = pe50_rfc_get_ita_lmt(info);
	idvchg_lmt = min(data->idvchg_cc, ita_lmt);
	PE50_INFO("idvchg_lmt(ita_lmt, idvchg_cc)=%d(%d,%d)\n",
		idvchg_lmt, ita_lmt, data->idvchg_cc);
	return idvchg_lmt;
}

int pe50_rfc_algo_init(struct pe50_algo_info *info)
{
	u32 vta;
	int cnt =0;
	int ret, i, vbus, vbat, vout;
	int ita_avg = 0, vta_avg = 0, vbus_avg = 0, vbat_avg = 0;
	bool err;
	const int avg_times = 10;
	bool unlock_retry = false;
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

	ret = pe50_hal_enable_hz(info->alg, true);
	if (ret < 0) {
		PE50_ERR("set swchg hz fail(%d)\n", ret);
		goto err;
	}
	msleep(500); /* Wait current stable */


	ret = pe50_get_adc(info, PE50_ADCCHAN_VBUS, &vbus);
	if (ret < 0) {
		PE50_ERR("get vbus fail(%d)\n", ret);
		goto err;
	}
	ret = pe50_get_adc(info, PE50_ADCCHAN_VOUT, &vout);
	if (ret < 0) {
		PE50_ERR("get vout fail(%d)\n", ret);
		goto err;
	}

unlock_err_retry:
	/* Adjust VBUS to make sure DVCHG can be turned on */
	vta = pe50_vout2vbus(info, vout);
	ret = pe50_set_ta_cap_cv(info, vta, data->idvchg_ss_init);
	if (ret < 0) {
		PE50_ERR("set ta cap fail(%d)\n", ret);
		goto err;
	}

	msleep(50); //wait TA boost
	ret = pe50_get_ta_cap_by_supportive(info, &data->vta_measure,
					    &data->ita_measure);
	if (ret < 0) {
		PE50_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}

	if (data->vta_setting > data->vta_measure + 1000) {
		PE50_ERR("RFC TA request voltage failed, set:%d, measure:%d, unlock_retry:%d\n",
			data->vta_setting, data->vta_measure, unlock_retry);
		if (!unlock_retry) {
			ret = pe50_set_ta_cap_cv(info, 5500, 2200);
			if (ret < 0) {
				PE50_ERR("set ta cap fail(%d)\n", ret);
				goto err;
			}
			unlock_retry = true;
			goto unlock_err_retry;
		}
		goto err;
	}

	while (cnt++ <= 40) {
		ret = pe50_hal_is_vbuslowerr(info->alg, DVCHG1, &err);
		if (ret < 0) {
			PE50_ERR("get vbuslowerr fail(%d)\n", ret);
			goto err;
		}
		if (!err)
			break;
		vta = data->vta_setting + auth_data->vta_step;
		ret = pe50_set_ta_cap_cv(info, vta, data->idvchg_ss_init);
		if (ret < 0) {
			PE50_ERR("set ta cap fail(%d)\n", ret);
			goto err;
		}

	}

	for (i = 0; i < avg_times; i++) {
		if (auth_data->support_meas_cap) {
			ret = pe50_get_ta_cap(info);
			if (ret < 0) {
				PE50_ERR("get ta cap fail(%d)\n", ret);
				sinfo.hardreset_ta = true;
				goto err;
			}
			ita_avg += data->ita_measure;
			vta_avg += data->vta_measure;
			ret = pe50_get_adc(info, PE50_ADCCHAN_VBUS, &vbus);
			if (ret < 0) {
				PE50_ERR("get vbus fail(%d)\n", ret);
				goto err;
			}
			vbus_avg += vbus;
		}
	}

	if (auth_data->support_meas_cap) {
		ita_avg = precise_div(ita_avg, avg_times);
		vta_avg = precise_div(vta_avg, avg_times);
		vbus_avg = precise_div(vbus_avg, avg_times);
	}

	if (auth_data->support_meas_cap) {
		/* vbus calibration: voltage difference between TA & device */
		data->vbus_cali = vta_avg - vbus_avg;
		PE50_INFO("avg(ita,vta,vbus):(%d,%d,%d), vbus_cali:%d\n",
			  ita_avg, vta_avg, vbus_avg, data->vbus_cali);
		if (abs(data->vbus_cali) > PE50_VBUS_CALI_THRESHOLD) {
			PE50_ERR("vbus cali (%d) > (%d)\n", data->vbus_cali,
				 PE50_VBUS_CALI_THRESHOLD);
			goto err;
		}
		if (ita_avg > desc->ifod_threshold) {
			PE50_ERR("foreign object detected, ita(%d) > (%d)\n",
				 ita_avg, desc->ifod_threshold);
			goto err;
		}
	}
	ret = pe50_set_dvchg_charging(info, true);
	if (ret < 0) {
		PE50_ERR("en dvchg fail\n");
		goto err;
	}

	cnt = 0;
	while (cnt++ <= 20) {
	    	ret = pe50_get_ta_cap_by_supportive(info,
	    		&data->vta_measure, &data->ita_measure);
	    	if (ret < 0) {
	    		PE50_ERR("get ta cap fail(%d)\n", ret);
	    		goto err;
	    	}
	
	    	PE50_ERR("vat(%d) ita_measure(%d) vta_measure(%d) cnt(%d)\n",
	    		vta, data->ita_measure, data->vta_measure, cnt);
	
	    	if (data->ita_measure > 800)
	    		break;
	
	    	vta = data->vta_setting + auth_data->vta_step;
	
	    	ret = pe50_set_ta_cap_cv(info, vta, data->idvchg_ss_init);
	    	if (ret < 0) {
	    		PE50_ERR("set ta cap fail(%d)\n", ret);
	    		goto err;
	    	}
	    	msleep(50);
	    	cnt++;
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

int pe50_rfc_algo_measure_r(struct pe50_algo_info *info)
{
	int ret;
	struct pe50_algo_data *data = info->data;
	struct pe50_algo_desc *desc = info->desc;
	struct pe50_ta_auth_data *auth_data = &data->ta_auth_data[data->running_spec];
	u32 rcable_retry_level = desc->rcable_level[PE50_RCABLE_NORMAL];
	struct pe50_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PE50_DBG("++\n");

	/*
	 * Ignore measuring r,
	 * treat as normal cable if meas_cap is not supported
	 */

	if (!auth_data->support_meas_cap) {
		PE50_INFO("ignore measuring resistance\n");
		goto select_ita;
	}

	ret = pe50_algo_cal_r_info_with_ta_cap(info, &sinfo);
	if (ret < 0) {
		PE50_ERR("get r info fail(%d)\n", ret);
		goto err;
	}
	if (data->r_cable > rcable_retry_level &&
	    data->err_retry_cnt < PE50_MEASURE_R_RETRY_MAX) {
		PE50_INFO("rcable(%d) is worse than normal\n", data->r_cable);
		goto err;
	}
	PE50_ERR("avg_r(sw,bat,cable):(%d,%d,%d), r_total:%d\n",
		 data->r_sw, data->r_bat, data->r_cable, data->r_total);
select_ita:
	ret = pe50_select_ita_lmt_by_r(info, false);
	if (ret < 0) {
		PE50_ERR("select dvchg ita lmt fail(%d)\n", ret);
		goto out;
	}
	data->err_retry_cnt = 0;
	data->state = PE50_ALGO_SS_DVCHG;
	return 0;
err:
	if (data->err_retry_cnt < PE50_MEASURE_R_RETRY_MAX) {
		data->err_retry_cnt++;
		return 0;
	}
out:
	return pe50_stop(info, &sinfo);
}

int pe50_rfc_algo_ss_dvchg(struct pe50_algo_info *info)
{
	bool dvchg_en;
	int ret, vbat;
	int repeat_cnt = 200;
	int vbus, vol_diff;
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
		PE50_ERR("get dvchg status fail(%d, %d)\n", ret, dvchg_en);
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

	ret = pe50_get_adc(info, PE50_ADCCHAN_VBUS, &vbus);
	if (ret < 0) {
		PE50_ERR("get vbus fail(%d)\n", ret);
		goto out;
	}

	ret = pe50_get_ta_cap_by_supportive(info, &data->vta_measure,
					    &data->ita_measure);
	if (ret < 0) {
		PE50_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}

	vol_diff = data->vta_measure - vbus;

	ret = pe50_algo_multi_dvchg_update(info);
	if (ret < 0) {
		ret = pe50_earily_restart(info);
		if (ret < 0) {
			PE50_ERR("earily restart fail(%d)\n", ret);
			goto out;
		}
		return 0;
	}

	idvchg_lmt = pe50_rfc_get_idvchg_lmt(info);
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
		PE50_INFO("--vta, ita, vbat over cv(%d, %d)\n",
			vbat, data->vbat_cv);
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

	if (vol_diff >= data->vol_diff_max) {
		vta -= auth_data->vta_step;
		data->state = PE50_ALGO_CC_CV;
		PE50_INFO("--vta, ita, vol_diff(meas,lmt) = (%d, %d)\n",
			vol_diff, data->vol_diff_max);
		goto out_set_cap;
	}

	/* IBUS reaches CC level */
	if (data->ita_measure + ita_gap_per_vstep + auth_data->ita_gap_cv_mode > idvchg_lmt ||
	    vta == auth_data->vcap_max || repeat_cnt-- <= 0){
		data->state = PE50_ALGO_CC_CV;
		PE50_INFO("Reach ita max or cnt max, try CC_CV\n");
	} else {
		vta += auth_data->vta_step;
		vta = min(vta, (u32)auth_data->vcap_max);
	}

	if (desc->buck_pump_chg_multipled) {
		if ((!data->is_swchg_en)&&(data->ita_measure + ita_gap_per_vstep + auth_data->ita_gap_cv_mode > PE50_INITIAT_BUCKCHG_LEVEL)) {
			PE50_INFO("swtich to enable\n");
			ret = pe50_hal_set_ichg(info->alg, CHG1, PE50_BUCK_ICHG_MAX);
			if (ret < 0) {
				PE50_ERR("set ichg fail(%d)\n", ret);
				goto out;
			}
			ret = pe50_hal_set_aicr(info->alg, CHG1, PE50_BUCK_IINDPM_MAX);
			if (ret < 0) {
				PE50_ERR("set aicr fail(%d)\n", ret);
				goto out;
			}
			pe50_enable_swchg_charging(info,true);
		}
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

int pe50_rfc_algo_cc_cv(struct pe50_algo_info *info)
{
	int ret, vbat;
	int vbus, vol_diff;
	int buck_difference, dvchg_num;
	bool dual_batt_rise = false;
	struct pe50_algo_desc *desc = info->desc;
	struct pe50_algo_data *data = info->data;
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

	ret = pe50_get_adc(info, PE50_ADCCHAN_VBUS, &vbus);
	if (ret < 0) {
		PE50_ERR("get vbus fail(%d)\n", ret);
		goto out;
	}

	ret = pe50_get_ta_cap_by_supportive(info, &data->vta_measure,
					    &data->ita_measure);
	if (ret < 0) {
		PE50_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}

	vol_diff = data->vta_measure - vbus;

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
	idvchg_lmt = pe50_rfc_get_idvchg_lmt(info);
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
		vta -= min(auth_data->vta_step, desc->pe50_default_vta_step);
		ita = max(ita, idvchg_lmt);
		data->is_vbat_over_cv = true;
		goto out_set_cap;
	} else if ((data->ita_measure > (idvchg_lmt + ita_gap_per_vstep +
			auth_data->ita_gap_cv_mode))) {
		vta -= auth_data->vta_step;
		ita = max(ita, idvchg_lmt);
		PE50_INFO("--vta, ita(meas,lmt)=(%d,%d)\n", data->ita_measure,
			  idvchg_lmt);
		goto out_set_cap;
	} else if (vol_diff >= data->vol_diff_max) {
		vta -= auth_data->vta_step;
		PE50_INFO("--vta, ita, vol_diff(meas,lmt) = (%d, %d)\n",
			vol_diff, data->vol_diff_max);
	} else if (!data->is_vbat_over_cv && vbat <= data->cv_lower_bound &&
		   data->ita_measure <= (idvchg_lmt - ita_gap_per_vstep -
		   auth_data->ita_gap_cv_mode) && dual_batt_rise &&
		   vta < auth_data->vcap_max && !data->suspect_ta_cc &&
		   vol_diff < (data->vol_diff_max - desc->vol_diff_gap)) {
		vta += auth_data->vta_step;
		vta = min_t(u32, vta, auth_data->vcap_max);
		ita = max(ita, idvchg_lmt);
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

	if (desc->buck_pump_chg_multipled) {
		dvchg_num = pe50_dvchg_online_cnt(info);
		if (dvchg_num <= 0 || dvchg_num > TRAN_MULTI_SPEED_MAX - 1) {
			PE50_ERR("invaid dvchg_num: %d\n", dvchg_num);
			goto out;
		}
		buck_difference = desc->idvchg_level_multi[dvchg_num - 1] - data->ita_measure;
		if (data->is_swchg_en) {
			ret = pe50_hal_set_ichg(info->alg, CHG1, (PE50_BUCK_ICHG_MAX - 2 * buck_difference));
			if (ret < 0) {
				PE50_ERR("set ichg fail(%d)\n", ret);
				goto out;
			}
			ret = pe50_hal_set_aicr(info->alg, CHG1, (PE50_BUCK_IINDPM_MAX - buck_difference));
			if (ret < 0) {
				PE50_ERR("set aicr fail(%d)\n", ret);
				goto out;
			}
			if (buck_difference > desc->idvchg_level_multi[dvchg_num - 1] - PE50_INITIAT_BUCKCHG_LEVEL) {
				ret = pe50_hal_set_ichg(info->alg, CHG1, PE50_BUCK_IINDPM_MIN);
				if (ret < 0) {
					PE50_ERR("set ichg fail(%d)\n", ret);
					goto out;
				}
				ret = pe50_hal_set_aicr(info->alg, CHG1, PE50_BUCK_IINDPM_MIN);
				if (ret < 0) {
					PE50_ERR("set aicr fail(%d)\n", ret);
					goto out;
				}
				pe50_enable_swchg_charging(info, false);
			}
		}
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
