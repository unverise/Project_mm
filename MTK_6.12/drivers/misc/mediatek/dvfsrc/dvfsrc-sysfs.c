// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/interconnect.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include "dvfsrc-common.h"
#include "dvfsrc-helper.h"
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/sysfs.h>

static DEFINE_MUTEX(bw_lock);

//MD:modify by tao.chen start 2025.07.25
#if IS_ENABLED(CONFIG_TRAN_DDR_RATE_STATICS)
#include <linux/jiffies.h>
#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif

#define TRAN_STATISTICS_VAL_LEN 24

struct dvfsrc_dvfs_opps {
	unsigned int vcore_uv;
	unsigned long long active_time;
	unsigned int  dram_kbps;
	unsigned long long bw;
};

struct dvfsrc_vcore_dram_opps {
	union {
		unsigned int vcore_uv;
		unsigned int dram_kbps;
	};
	unsigned long long active_time;
	unsigned long long bw;
};

struct dvfsrc_rate_statistics {
	struct dvfsrc_dvfs_opps *dvfs_opps;
	struct dvfsrc_vcore_dram_opps *dram_opps;
	struct dvfsrc_vcore_dram_opps *vcore_opps;
	unsigned long long total_active_time;
	int num_dvfs_opp;
	int num_dram_opp;
	int num_vcore_opp;
};

static struct dvfsrc_rate_statistics rate_statistics;
static struct timer_list statistics_timer;
static u32 timer_ms = 1000;
static u64 pre_jiffies;
static int rate_dump_enable = 0;
static struct mtk_dvfsrc *dvfsrc = NULL;
DEFINE_SPINLOCK(timer_cb_spinlock);

static char * tran_get_div_float_str(unsigned long long num, unsigned long long denom, char *str, int len)
{
	if (denom == 0) {
		snprintf(str, len, "0.000");
	} else {
		snprintf(str, len, "%llu.%03llu", num/denom, ((num % denom) * 1000) / denom);
	}
	return str;
}

static void tran_free_statistics(struct dvfsrc_rate_statistics *prate_statistics , bool need_locked)
{
	unsigned long spin_flags;
	struct dvfsrc_vcore_dram_opps *tmp_prate_dram_opps;
	struct dvfsrc_vcore_dram_opps *tmp_prate_vcore_opps;
	struct dvfsrc_dvfs_opps *tmp_prate_dvfs_opps;

	if (need_locked == true) {
		spin_lock_irqsave(&timer_cb_spinlock, spin_flags);
	}
	tmp_prate_dram_opps = prate_statistics->dram_opps ;
	tmp_prate_vcore_opps = prate_statistics->vcore_opps ;
	tmp_prate_dvfs_opps = prate_statistics->dvfs_opps;
	prate_statistics->num_dram_opp = 0;
	prate_statistics->num_vcore_opp = 0;
	prate_statistics->num_dvfs_opp = 0;
	prate_statistics->total_active_time = 0;
	prate_statistics->dram_opps = NULL;
	prate_statistics->vcore_opps = NULL;
	prate_statistics->dvfs_opps = NULL;

	if (need_locked == true) {
		spin_unlock_irqrestore(&timer_cb_spinlock, spin_flags);
	}

	if (tmp_prate_dram_opps != NULL) {
		kfree(tmp_prate_dram_opps);
	}

	if (tmp_prate_vcore_opps != NULL) {
		kfree(tmp_prate_vcore_opps);
	}

	if (tmp_prate_dvfs_opps != NULL) {
		kfree(tmp_prate_dvfs_opps);
	}

	return;
}

static int tran_create_statistics(struct dvfsrc_rate_statistics *prate_statistics)
{
	if (prate_statistics == NULL) {
		return -EINVAL;
	}

	if (!dvfsrc || !dvfsrc->opp_desc) {
		return -EINVAL;
	}

	prate_statistics->num_dram_opp = dvfsrc->opp_desc->num_dram_opp;
	prate_statistics->num_vcore_opp = dvfsrc->opp_desc->num_vcore_opp;
	prate_statistics->num_dvfs_opp = dvfsrc->opp_desc->num_opp;
	prate_statistics->total_active_time = 0;
	if (prate_statistics->num_dram_opp <= 0 || prate_statistics->num_vcore_opp <= 0 || prate_statistics->num_dvfs_opp <= 0) {
		return -EINVAL;
	}

	prate_statistics->dram_opps = kcalloc(prate_statistics->num_dram_opp, sizeof(struct dvfsrc_vcore_dram_opps), GFP_KERNEL);
	prate_statistics->vcore_opps = kcalloc(prate_statistics->num_vcore_opp, sizeof(struct dvfsrc_vcore_dram_opps), GFP_KERNEL);
	prate_statistics->dvfs_opps = kcalloc(prate_statistics->num_dvfs_opp, sizeof(struct dvfsrc_dvfs_opps), GFP_KERNEL);

	if (prate_statistics->dram_opps == NULL || prate_statistics->vcore_opps == NULL || prate_statistics->dvfs_opps == NULL) {
		tran_free_statistics(prate_statistics, false);
		return -ENOMEM;
	}

	return 0;
}

static void tran_copy_statistics(struct dvfsrc_rate_statistics *pdest, struct dvfsrc_rate_statistics *psrc)
{
	unsigned long spin_flags;
	if (pdest == NULL || psrc == NULL) {
		return;
	}

	spin_lock_irqsave(&timer_cb_spinlock, spin_flags);
	pdest->num_dram_opp = psrc->num_dram_opp;
	pdest->num_vcore_opp = psrc->num_vcore_opp;
	pdest->num_dvfs_opp = psrc->num_dvfs_opp;
	pdest->total_active_time = psrc->total_active_time;

	if (pdest->dram_opps && psrc->dram_opps) {
		memcpy(pdest->dram_opps, psrc->dram_opps, sizeof(struct dvfsrc_vcore_dram_opps) * psrc->num_dram_opp);
	}
	if (pdest->vcore_opps && psrc->vcore_opps) {
		memcpy(pdest->vcore_opps, psrc->vcore_opps, sizeof(struct dvfsrc_vcore_dram_opps) * psrc->num_vcore_opp);
	}
	if (pdest->dvfs_opps && psrc->dvfs_opps) {
		memcpy(pdest->dvfs_opps, psrc->dvfs_opps, sizeof(struct dvfsrc_dvfs_opps) * psrc->num_dvfs_opp);
	}
	spin_unlock_irqrestore(&timer_cb_spinlock, spin_flags);

	return;
}

static void tran_opps_timer_cb(struct timer_list *t)
{
	int dram_opp = 0;
	int dvfs_opp = 0;
	int vcore_opp = 0;
	u64 curr_jiffies = 0;
	unsigned long long active_time = 0;
	unsigned long long bw = 0;
	unsigned long spin_flags;

	if (dvfsrc ==  NULL) {
		return;
	}

	dram_opp = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_OPP);
	dvfs_opp  = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DVFS_OPP);
	vcore_opp  = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_OPP);

	curr_jiffies = get_jiffies_64();
	active_time = jiffies64_to_msecs(curr_jiffies - pre_jiffies);
	pre_jiffies = curr_jiffies;
	bw = 0;

	if (dvfsrc->dvd && dvfsrc->dvd->config && dvfsrc->dvd->config->tran_get_bw_reg) {
		bw = dvfsrc->dvd->config->tran_get_bw_reg(dvfsrc) * active_time / 10; // tran_get_bw_reg(dvfsrc) * 100 * active_time / 1000
	}

	spin_lock_irqsave(&timer_cb_spinlock, spin_flags);
	if (rate_statistics.dvfs_opps != NULL && dvfs_opp < rate_statistics.num_dvfs_opp && dvfs_opp >= 0) {
		rate_statistics.dvfs_opps[dvfs_opp].active_time += active_time;
		rate_statistics.dvfs_opps[dvfs_opp].bw += bw;
	}

	if (rate_statistics.dram_opps != NULL && dram_opp < rate_statistics.num_dram_opp && dram_opp >= 0 ) {
		rate_statistics.dram_opps[dram_opp].active_time += active_time;
		rate_statistics.dram_opps[dram_opp].bw += bw;
	}

	if (rate_statistics.vcore_opps != NULL && vcore_opp < rate_statistics.num_vcore_opp && vcore_opp >= 0) {
		rate_statistics.vcore_opps[vcore_opp].active_time += active_time;
		rate_statistics.vcore_opps[vcore_opp].bw += bw;
	}
	rate_statistics.total_active_time += active_time;
	spin_unlock_irqrestore(&timer_cb_spinlock, spin_flags);

	mod_timer(&statistics_timer, jiffies + msecs_to_jiffies(timer_ms));
}

static ssize_t tran_dvfsrc_rate_dvfs_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	char tmp[TRAN_STATISTICS_VAL_LEN] = {0};
	char tmp2[TRAN_STATISTICS_VAL_LEN] = {0};
	int i = 0;
	struct dvfsrc_rate_statistics tmp_rate_statistics = {};

	mutex_lock(&bw_lock);
	if (dvfsrc == NULL) {
		dvfsrc = dev_get_drvdata(dev);
	}

	if (0 != tran_create_statistics(&tmp_rate_statistics)) {
		p += snprintf(p, buff_end - p, "Please try again, init error.\n");
		mutex_unlock(&bw_lock);
		return p - buf;
	}

	tran_copy_statistics(&tmp_rate_statistics, &rate_statistics);

	if ( tmp_rate_statistics.dvfs_opps == NULL ) {
		p += snprintf(p, buff_end - p, "please enable rate dump.\n");
	} else if (tmp_rate_statistics.total_active_time == 0) {
		p += snprintf(p, buff_end - p, "Please wait a few moments during startup.\n");
	} else {
		p += snprintf(p, buff_end - p, "%-10s	%-16s	%-16s	%-16s	%-16s	%-16s\n","DVFS_OPP", "vcore(uv)", "DRAM(kbps)", "active_time(s)", "percent(%)", "BW(MB)");
		for (i=0; i<tmp_rate_statistics.num_dvfs_opp; i++)
		{
			p += snprintf(p, buff_end - p, "%-10u	%-16u	%-16u	%-16s	%-16s	%-16llu\n",
				i, tmp_rate_statistics.dvfs_opps[i].vcore_uv, tmp_rate_statistics.dvfs_opps[i].dram_kbps,
				 tran_get_div_float_str(tmp_rate_statistics.dvfs_opps[i].active_time, 1000, tmp, TRAN_STATISTICS_VAL_LEN),
				 tran_get_div_float_str(tmp_rate_statistics.dvfs_opps[i].active_time * 100, tmp_rate_statistics.total_active_time, tmp2, TRAN_STATISTICS_VAL_LEN),
				 tmp_rate_statistics.dvfs_opps[i].bw);
		}

		p += snprintf(p, buff_end - p, "Total active time is %ss, current sampling time is %ums \n", tran_get_div_float_str(tmp_rate_statistics.total_active_time, 1000, tmp, TRAN_STATISTICS_VAL_LEN), timer_ms) ;
	}
	mutex_unlock(&bw_lock);
	tran_free_statistics(&tmp_rate_statistics, false);
	return p - buf;
}
static DEVICE_ATTR_RO(tran_dvfsrc_rate_dvfs_dump);

static ssize_t tran_dvfsrc_rate_vcore_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	char tmp[TRAN_STATISTICS_VAL_LEN] = {0};
	char tmp2[TRAN_STATISTICS_VAL_LEN] = {0};
	int i = 0;
	struct dvfsrc_rate_statistics tmp_rate_statistics = {};

	mutex_lock(&bw_lock);

	if (dvfsrc == NULL) {
		dvfsrc = dev_get_drvdata(dev);
	}

	if (0 != tran_create_statistics(&tmp_rate_statistics)) {
		p += snprintf(p, buff_end - p, "Please try again, init error.\n");
		mutex_unlock(&bw_lock);
		return p - buf;
	}

	tran_copy_statistics(&tmp_rate_statistics, &rate_statistics);

	if ( tmp_rate_statistics.dvfs_opps == NULL ) {
		p += snprintf(p, buff_end - p, "please enable rate dump.\n");
	} else if (tmp_rate_statistics.total_active_time == 0)
	{
		p += snprintf(p, buff_end - p, "Please wait a few moments during startup.\n");
	} else {
		p += snprintf(p, buff_end - p, "%-10s	%-16s	%-16s	%-16s	%-16s\n","\nVCORE_OPP", "vcore(uv)", "active_time(s)", "percent(%)", "BW(MB)");
		for (i=0; i<tmp_rate_statistics.num_vcore_opp; i++) {
			p += snprintf(p, buff_end - p, "%-10u	%-16u	%-16s	%-16s	%-16llu\n", 
				i, tmp_rate_statistics.vcore_opps[i].vcore_uv,
				tran_get_div_float_str(tmp_rate_statistics.vcore_opps[i].active_time, 1000, tmp, TRAN_STATISTICS_VAL_LEN),
				tran_get_div_float_str(tmp_rate_statistics.vcore_opps[i].active_time * 100, tmp_rate_statistics.total_active_time, tmp2, TRAN_STATISTICS_VAL_LEN),
				tmp_rate_statistics.vcore_opps[i].bw);
		}
		p += snprintf(p, buff_end - p, "Total active time is %ss, current sampling time is %ums \n", tran_get_div_float_str(tmp_rate_statistics.total_active_time, 1000, tmp, TRAN_STATISTICS_VAL_LEN), timer_ms) ;
	}

	mutex_unlock(&bw_lock);
	tran_free_statistics(&tmp_rate_statistics, false);
	return p - buf;
}

static DEVICE_ATTR_RO(tran_dvfsrc_rate_vcore_dump);

static ssize_t tran_dvfsrc_rate_dram_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	char tmp[TRAN_STATISTICS_VAL_LEN] = {0};
	char tmp2[TRAN_STATISTICS_VAL_LEN] = {0};
	int i = 0;
	struct dvfsrc_rate_statistics tmp_rate_statistics = {};

	mutex_lock(&bw_lock);

	if (dvfsrc == NULL) {
		dvfsrc = dev_get_drvdata(dev);
	}

	if (0 != tran_create_statistics(&tmp_rate_statistics)) {
		p += snprintf(p, buff_end - p, "Please try again, init error.\n");
		mutex_unlock(&bw_lock);
		return p - buf;
	}

	tran_copy_statistics(&tmp_rate_statistics, &rate_statistics);

	if ( tmp_rate_statistics.dvfs_opps == NULL ) {
		p += snprintf(p, buff_end - p, "please enable rate dump.\n");
	} else if (tmp_rate_statistics.total_active_time == 0)
	{
		p += snprintf(p, buff_end - p, "Please wait a few moments during startup.\n");
	} else {
		p += snprintf(p, buff_end - p, "%-10s	%-16s	%-16s	%-16s	%-16s\n","\nDRAM_OPP", "DRAM(kbps)", "active_time(s)", "percent(%)", "BW(MB)");
		for (i=0; i<tmp_rate_statistics.num_dram_opp; i++)
		{
			p += snprintf(p, buff_end - p, "%-10u	%-16u	%-16s	%-16s	%-16llu\n", 
				i, tmp_rate_statistics.dram_opps[i].dram_kbps,
				tran_get_div_float_str(tmp_rate_statistics.dram_opps[i].active_time, 1000,  tmp, TRAN_STATISTICS_VAL_LEN),
				tran_get_div_float_str(tmp_rate_statistics.dram_opps[i].active_time * 100, tmp_rate_statistics.total_active_time, tmp2, TRAN_STATISTICS_VAL_LEN),
				tmp_rate_statistics.dram_opps[i].bw);
		}

		p += snprintf(p, buff_end - p, "Total active time is %ss, current sampling time is %ums \n", tran_get_div_float_str(tmp_rate_statistics.total_active_time, 1000, tmp, TRAN_STATISTICS_VAL_LEN), timer_ms) ;
	}
	mutex_unlock(&bw_lock);
	tran_free_statistics(&tmp_rate_statistics, false);

	return p - buf;
}
static DEVICE_ATTR_RO(tran_dvfsrc_rate_dram_dump);

static void tran_destroy_statistics(void)
{
	if (timer_pending(&statistics_timer)) {
		del_timer_sync(&statistics_timer);
	}

	tran_free_statistics(&rate_statistics, true);
	return ;
}

static int tran_init_statistics(void)
{
	int i = 0;
	int j = 0;
	int dram_idx = 0;
	int vcore_idx = 0;

	struct dvfsrc_rate_statistics tmp_rate_statistics = {};

	if (!dvfsrc || !dvfsrc->opp_desc) {
		return -EINVAL;
	}

	if (0 != tran_create_statistics(&tmp_rate_statistics)) {
		pr_info("dvfsrc_create_statistics  error\n");
		return -ENOMEM;
	}

	if (0 != tran_create_statistics(&rate_statistics)) {
		pr_info("dvfsrc_create_statistics  error\n");
		tran_free_statistics(&tmp_rate_statistics, false);
		return -ENOMEM;
	}

	for (i = 0; i< tmp_rate_statistics.num_dvfs_opp; i++)
	{
		j = tmp_rate_statistics.num_dvfs_opp - (i + 1);

		if (j < 0 || j >= dvfsrc->opp_desc->num_opp) {
			tran_free_statistics(&tmp_rate_statistics, false);
			tran_free_statistics(&rate_statistics, true);
			return -EINVAL;
		}

		tmp_rate_statistics.dvfs_opps[i].vcore_uv = dvfsrc->opp_desc->opps[j].vcore_uv;
		tmp_rate_statistics.dvfs_opps[i].dram_kbps = dvfsrc->opp_desc->opps[j].dram_kbps;
		tmp_rate_statistics.dvfs_opps[i].active_time = 0;

		dram_idx = tmp_rate_statistics.num_dram_opp - (dvfsrc->opp_desc->opps[j].dram_opp + 1);
		vcore_idx = tmp_rate_statistics.num_vcore_opp - (dvfsrc->opp_desc->opps[j].vcore_opp + 1);

		if (dram_idx >= 0 && dram_idx < tmp_rate_statistics.num_dram_opp) {
		    tmp_rate_statistics.dram_opps[dram_idx].dram_kbps = dvfsrc->opp_desc->opps[j].dram_kbps;
		}
		if (vcore_idx >= 0 && vcore_idx < tmp_rate_statistics.num_vcore_opp) {
		    tmp_rate_statistics.vcore_opps[vcore_idx].vcore_uv = dvfsrc->opp_desc->opps[j].vcore_uv;
		}
	}

	tran_copy_statistics(&rate_statistics, &tmp_rate_statistics);
	tran_free_statistics(&tmp_rate_statistics, false);
	timer_setup(&statistics_timer, tran_opps_timer_cb, 0);
	pre_jiffies = get_jiffies_64();
	mod_timer(&statistics_timer, jiffies + msecs_to_jiffies(timer_ms));
	return 0;
}

static ssize_t tran_dvfsrc_rate_dump_enable_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	char *p = buf;
    char *buff_end = p + PAGE_SIZE;

	mutex_lock(&bw_lock);
	if (rate_dump_enable == 0 ) {
		if (dvfsrc == NULL) {
			dvfsrc = dev_get_drvdata(dev);
		}
		timer_ms = 1000;
		if (0 != tran_init_statistics()) {
			tran_destroy_statistics();
			p += snprintf(p, buff_end - p, "Failed to alloc mem.");
		} else {
			p += snprintf(p, buff_end - p, "Succeed to enable rate dump");
			rate_dump_enable = !rate_dump_enable;
		}
	} else if ( rate_dump_enable == 1 ) {
		tran_destroy_statistics();
		dvfsrc = NULL;
		p += snprintf(p, buff_end - p, "Succeed to disable rate dump");
		rate_dump_enable = !rate_dump_enable;
	}
	mutex_unlock(&bw_lock);
	return p - buf;
}

static DEVICE_ATTR_RO(tran_dvfsrc_rate_dump_enable);

static ssize_t tran_dvfsrc_rate_dump_samping_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;

	if (kstrtou32(buf, 10, &val)) {
		return -EINVAL;
	}

	if (val == 0) {
		return -EINVAL;
	}

	mutex_lock(&bw_lock);
	if (dvfsrc == NULL) {
		dvfsrc = dev_get_drvdata(dev);
	}

	tran_destroy_statistics();
	timer_ms = val;
	if (0 != tran_init_statistics()) {
		mutex_unlock(&bw_lock);
		return -ENOMEM;
	}
	mutex_unlock(&bw_lock);

	return count;
}

static DEVICE_ATTR_WO(tran_dvfsrc_rate_dump_samping);

#endif

//MD:modify by tao.chen end 2025.07.25 end

static ssize_t dvfsrc_req_bw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (!dvfsrc->bw_path)
		return -EINVAL;

	icc_set_bw(dvfsrc->bw_path, MBps_to_icc(val), 0);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_bw);

static ssize_t dvfsrc_req_hrtbw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (!dvfsrc->hrt_path)
		return -EINVAL;

	icc_set_bw(dvfsrc->hrt_path, MBps_to_icc(val), 0);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_hrtbw);

static ssize_t dvfsrc_req_ddr_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (!dvfsrc->perf_path)
		return -EINVAL;

	if (val < dvfsrc->num_perf)
		icc_set_bw(dvfsrc->perf_path, 0, dvfsrc->perfs_peak_bw[val]);
	else
		icc_set_bw(dvfsrc->perf_path, 0, 0);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_ddr_opp);

static ssize_t dvfsrc_req_vcore_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	u32 max_opp;
	u32 gear;
	int vcore_uV = 0;

	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	max_opp = dvfsrc->opp_desc->num_vcore_opp - 1;
	gear = (val <= max_opp) ? max_opp - val : 0;

	if (dvfsrc->dvfsrc_vcore_power) {
		vcore_uV = regulator_list_voltage(dvfsrc->dvfsrc_vcore_power, gear);
		if (vcore_uV > 0)
			regulator_set_voltage(dvfsrc->dvfsrc_vcore_power, vcore_uV, INT_MAX);
	}

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_vcore_opp);

static ssize_t dvfsrc_force_vcore_dvfs_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (dvfsrc->force_opp)
		dvfsrc->force_opp(dvfsrc, val);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_force_vcore_dvfs_opp);

static ssize_t dvfsrc_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	ssize_t dump_size = PAGE_SIZE - 1;
	const struct dvfsrc_config *config;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	config = dvfsrc->dvd->config;
	p = dvfsrc->dump_info(dvfsrc, p, dump_size);

	if (config->dump_vmode_info)
		p = config->dump_vmode_info(dvfsrc, p, dump_size - (p - buf));

	p = config->dump_reg(dvfsrc, p, dump_size - (p - buf));
	p = config->dump_record(dvfsrc, p, dump_size - (p - buf));

	if (config->dump_spm_info && dvfsrc->spm_regs)
		p = config->dump_spm_info(dvfsrc, p, dump_size - (p - buf));

	if (config->dump_hvs_info && dvfsrc->hvs_regs)
		p = config->dump_hvs_info(dvfsrc, p, dump_size - (p - buf));

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_dump);

static ssize_t spm_cmd_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	ssize_t dump_size = PAGE_SIZE - 1;
	const struct dvfsrc_config *config;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	config = dvfsrc->dvd->config;
	if (config->dump_spm_cmd && dvfsrc->spm_regs)
		p = config->dump_spm_cmd(dvfsrc, p, dump_size - (p - buf));

	return p - buf;
}
static DEVICE_ATTR_RO(spm_cmd_dump);

static ssize_t spm_timer_latch_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	ssize_t dump_size = PAGE_SIZE - 1;
	const struct dvfsrc_config *config;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	config = dvfsrc->dvd->config;
	if (config->dump_spm_timer_latch && dvfsrc->spm_regs)
		p = config->dump_spm_timer_latch(dvfsrc, p, dump_size - (p - buf));

	return p - buf;
}
static DEVICE_ATTR_RO(spm_timer_latch_dump);

static ssize_t dvfsrc_opp_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j;
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	p += snprintf(p, buff_end - p,
		"NUM_VCORE_OPP : %d\n",
		dvfsrc->opp_desc->num_vcore_opp);
	p += snprintf(p, buff_end - p,
		"NUM_DDR_OPP : %d\n",
		dvfsrc->opp_desc->num_dram_opp);
	p += snprintf(p, buff_end - p,
		"NUM_DVFSRC_OPP : %d\n\n",
		dvfsrc->opp_desc->num_opp);

	for (i = 0; i < dvfsrc->opp_desc->num_opp; i++) {
		j = dvfsrc->opp_desc->num_opp - (i + 1);
		if (dvfsrc->dvd->dump_flag & DVFSRC_EMI_DUMP_FLAG) {
			p += snprintf(p, buff_end - p,
				"[%-3u]:%-6u, %-5u Mbps [%u]\n",
				i,
				(dvfsrc->opp_desc->opps[j].vcore_uv),
				(dvfsrc->opp_desc->opps[j].dram_kbps / 1000),
				dvfsrc->opp_desc->opps[j].emi_mbps);
		} else {
			p += snprintf(p, buff_end - p,
				"[OPP%-2d]: %-8u uv %-8u kbps\n",
				i,
				dvfsrc->opp_desc->opps[j].vcore_uv,
				dvfsrc->opp_desc->opps[j].dram_kbps);
		}
	}

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_opp_table);

static ssize_t dvfsrc_ddr_opp_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	int level = 0;
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (dvfsrc->opp_desc->num_opp > 0)
		level = dvfsrc->opp_desc->opps[0].dram_opp;

	for (i = 0; i < dvfsrc->opp_desc->num_opp; i++) {
		if (dvfsrc->opp_desc->opps[i].dram_opp == level) {
			p += snprintf(p, buff_end - p,
			"%lu ",
			(unsigned long)(dvfsrc->opp_desc->opps[i].dram_kbps)*1000);
			level = level + 1;
		}
	}
	p += snprintf(p, buff_end - p, "\n");

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_ddr_opp_table);

static ssize_t dvfsrc_num_opps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", dvfsrc->opp_desc->num_opp);
}
static DEVICE_ATTR_RO(dvfsrc_num_opps);

static ssize_t dvfsrc_get_dvfs_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u64 dvfs_time_us;

	const struct dvfsrc_config *config;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	config = dvfsrc->dvd->config;

	if (dvfsrc->force_opp_idx >= dvfsrc->opp_desc->num_opp)
		return snprintf(buf, PAGE_SIZE, "Not in force mode\n");

	if (config->query_dvfs_time)
		dvfs_time_us = config->query_dvfs_time(dvfsrc);
	else
		return snprintf(buf, PAGE_SIZE, "Not Suuport query\n");

	return snprintf(buf, PAGE_SIZE, "dvfs_time = %llu us\n", dvfs_time_us);
}
DEVICE_ATTR_RO(dvfsrc_get_dvfs_time);

static inline ssize_t dvfsrc_qos_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", dvfsrc->qos_mode);
}

static inline ssize_t dvfsrc_qos_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int mode = 0;
	struct arm_smccc_res ares;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtouint(buf, 0, &mode) != 0)
		return -EINVAL;

	if (!dvfsrc->afl_fuzzer_en) {
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_QOS_MODE,
			mode, 0, 0, 0, 0, 0,
			&ares);

		if (!ares.a0)
			dvfsrc->qos_mode = mode;
	} else {
		pr_notice("%s:afl_fuzzer enable\n", __func__);
	}

	return count;
}
DEVICE_ATTR_RW(dvfsrc_qos_mode);

static inline ssize_t dvfsrc_qosmm_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%x\n", dvfsrc->qos_mm_mode);
}

static inline ssize_t dvfsrc_qosmm_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int mode = 0;
	struct arm_smccc_res ares;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 16, &mode))
		return -EINVAL;

	if (!dvfsrc->afl_fuzzer_en) {
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_QOS_MODE,
			mode, 0, 0, 0, 0, 0,
			&ares);

		if (!ares.a0)
			dvfsrc->qos_mm_mode = mode;

	} else {
		pr_notice("%s:afl_fuzzer enable\n", __func__);
	}

	return count;
}
DEVICE_ATTR_RW(dvfsrc_qosmm_mode);

static inline ssize_t dvfsrc_qosmd_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%x\n", dvfsrc->qos_mm_mode);
}

static inline ssize_t dvfsrc_qosmd_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int mode = 0;
	struct arm_smccc_res ares;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 16, &mode))
		return -EINVAL;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_QOS_MODE,
		mode, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		dvfsrc->qos_mm_mode = mode;

	return count;
}
DEVICE_ATTR_RW(dvfsrc_qosmd_mode);

static ssize_t dvfsrc_md_floor_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	ssize_t dump_size = PAGE_SIZE - 1;
	const struct dvfsrc_config *config;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	config = dvfsrc->dvd->config;
	if (config->dump_md_floor_table)
		p = config->dump_md_floor_table(dvfsrc, p, dump_size - (p - buf));

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_md_floor_table);

static inline ssize_t dvfsrc_ceiling_opp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	int i;

	for (i = 0; i < CEILING_ITEM_MAX; i++) {
		p += snprintf(p, buff_end - p, "[%d] : %d\n",
			i, dvfsrc->ceil_ddr_opp[i]);
	}

	return p - buf;
}

static inline ssize_t dvfsrc_ceiling_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int magic = 0, ceil_ddr_opp = 0;

	if (sscanf(buf, "%x %d", &magic, &ceil_ddr_opp) != 2)
		return -EINVAL;

	if (magic != 0xDEBC)
		return -EINVAL;

	mtk_dvfsrc_set_ceiling_freq(CEILING_SYSFS, ceil_ddr_opp);

	return count;
}
DEVICE_ATTR_RW(dvfsrc_ceiling_opp);

static struct ratelimit_state dvfsrc_force_ddr =
	RATELIMIT_STATE_INIT_FLAGS("dvfsrc_force_ddr", HZ, 2, RATELIMIT_MSG_ON_RELEASE);
static inline ssize_t dvfsrc_force_ddr_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (!dvfsrc->ceil_ddr_support)
		return -EINVAL;

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (val == 9595) {
		dvfsrc->force_ddr_en = true;
		return count;
	}

	if (dvfsrc->force_ddr_en) {
		if (__ratelimit(&dvfsrc_force_ddr))
			pr_info("dvfsrc_force ddr_opp\n");
		mtk_dvfsrc_set_ceiling_freq(CEILING_FORCE_MODE, val);
	}

	return count;
}
DEVICE_ATTR_WO(dvfsrc_force_ddr_opp);

static ssize_t dvfsrc_req_emi_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int magic = 0, emi_opp = 0, val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (sscanf(buf, "%x %d", &magic, &emi_opp) != 2)
		return -EINVAL;

	if (magic != 0xEEEE)
		return -EINVAL;

	if (emi_opp < dvfsrc->opp_desc->num_emi_opp)
		val = dvfsrc->opp_desc->num_emi_opp - emi_opp - 1;

	mtk_dvfsrc_send_request(dvfsrc->dev->parent,
		MTK_DVFSRC_CMD_EMICLK_REQUEST,
		val);

	return count;

}
static DEVICE_ATTR_WO(dvfsrc_req_emi_opp);

static ssize_t vcore_avs_zone_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	ssize_t dump_size = PAGE_SIZE - 1;
	const struct dvfsrc_config *config;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	config = dvfsrc->dvd->config;
	if (config->dump_vcore_avs_zone)
		p = config->dump_vcore_avs_zone(dvfsrc, p, dump_size - (p - buf));

	return p - buf;
}
static DEVICE_ATTR_RO(vcore_avs_zone_dump);

static ssize_t dvfsrc_therm_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	ssize_t dump_size = PAGE_SIZE - 1;
	const struct dvfsrc_config *config;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	config = dvfsrc->dvd->config;

	if (config->dump_therm_info)
		p = config->dump_therm_info(dvfsrc, p, dump_size - (p - buf));

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_therm_info);

static struct attribute *dvfsrc_sysfs_attrs[] = {
	&dev_attr_dvfsrc_req_bw.attr,
	&dev_attr_dvfsrc_req_hrtbw.attr,
	&dev_attr_dvfsrc_req_vcore_opp.attr,
	&dev_attr_dvfsrc_req_ddr_opp.attr,
	&dev_attr_dvfsrc_dump.attr,
	&dev_attr_dvfsrc_force_vcore_dvfs_opp.attr,
	&dev_attr_dvfsrc_opp_table.attr,
	&dev_attr_dvfsrc_ddr_opp_table.attr,
	&dev_attr_dvfsrc_num_opps.attr,
	&dev_attr_dvfsrc_get_dvfs_time.attr,
	&dev_attr_spm_cmd_dump.attr,
	&dev_attr_spm_timer_latch_dump.attr,
	&dev_attr_dvfsrc_qos_mode.attr,
	&dev_attr_dvfsrc_md_floor_table.attr,
	&dev_attr_vcore_avs_zone_dump.attr,
//MD:modify by tao.chen start 2025.07.25
#if IS_ENABLED(CONFIG_TRAN_DDR_RATE_STATICS)
	&dev_attr_tran_dvfsrc_rate_dvfs_dump.attr,
	&dev_attr_tran_dvfsrc_rate_vcore_dump.attr,
	&dev_attr_tran_dvfsrc_rate_dram_dump.attr,
	&dev_attr_tran_dvfsrc_rate_dump_enable.attr,
    &dev_attr_tran_dvfsrc_rate_dump_samping.attr,
#endif
//MD:modify by tao.chen end 2025.07.25
	NULL,
};

static struct attribute_group dvfsrc_sysfs_attr_group = {
	.attrs = dvfsrc_sysfs_attrs,
};

int dvfsrc_register_sysfs(struct device *dev)
{
	int ret, err;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	ret = sysfs_create_group(&dev->kobj, &dvfsrc_sysfs_attr_group);
	if (ret)
		return ret;

	if (dvfsrc->ceil_ddr_support) {
		err = sysfs_add_file_to_group(&dev->kobj,
			      &dev_attr_dvfsrc_ceiling_opp.attr, NULL);
		if (err)
			dev_info(dvfsrc->dev, "can't create ceiling_opp sysfs file\n");

		err = sysfs_add_file_to_group(&dev->kobj,
			      &dev_attr_dvfsrc_force_ddr_opp.attr, NULL);
		if (err)
			dev_info(dvfsrc->dev, "can't create force_ddr_opp sysfs file\n");
	}

	if (dvfsrc->dvd->qos_mm_mode_en) {
		err = sysfs_add_file_to_group(&dev->kobj,
			      &dev_attr_dvfsrc_qosmm_mode.attr, NULL);
		if (err)
			dev_info(dvfsrc->dev, "can't create mmqos sysfs file\n");
	}

	if (dvfsrc->dvd->qos_md_mode_en) {
		err = sysfs_add_file_to_group(&dev->kobj,
			      &dev_attr_dvfsrc_qosmd_mode.attr, NULL);
		if (err)
			dev_info(dvfsrc->dev, "can't create mdqos sysfs file\n");
	}

	if (dvfsrc->dvd->emi_opp_req_enmode) {
		err = sysfs_add_file_to_group(&dev->kobj,
			      &dev_attr_dvfsrc_req_emi_opp.attr, NULL);
		if (err)
			dev_info(dvfsrc->dev, "can't create emi_req sysfs file\n");
	}

	if (dvfsrc->dvd->therm_info_en) {
		err = sysfs_add_file_to_group(&dev->kobj,
			      &dev_attr_dvfsrc_therm_info.attr, NULL);
		if (err)
			dev_info(dvfsrc->dev, "can't create therm_info sysfs file\n");
	}

	ret = sysfs_create_link(&dev->parent->kobj, &dev->kobj,
		"helio-dvfsrc");
	if (ret)
		return ret;

	ret = sysfs_create_link(kernel_kobj, &dev->kobj,
		"helio-dvfsrc");
	return ret;
}

void dvfsrc_unregister_sysfs(struct device *dev)
{
//MD:modify by tao.chen start 2025.07.25
	mutex_lock(&bw_lock);
	tran_destroy_statistics();
	mutex_unlock(&bw_lock);
//MD:modify by tao.chen end 2025.07.25
	sysfs_remove_link(&dev->parent->kobj, "helio-dvfsrc");
	sysfs_remove_group(&dev->kobj, &dvfsrc_sysfs_attr_group);
}
