/**
  *Copyright 2023 Transsion Inc
  *Author transsioner
  *tran_mm_collect feature suppport
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>

#include <tranlog.h>

#include "tranmem_cloud.h"

#define TRANMM  "[tranmem_pf_cloud]"

static char *tranmem_cloud_config_ID        =  "1003000026";

/*
 * For performance and TMPS operation convenience, 'Pages Protection Enabled' is simplified to
 * ppt_enable, with "ppt" as the prefix.
 *
 *     ppt_enable:  >0 enabled, <0 disabled;
 * ppt_swappiness:  [0, TRAN_MAX_SWAPPINESS], TRAN_MAX_SWAPPINESS defined in main file.
 *  ppt_threshold:  pages, < MAX_GUARANTEED_FILE_SIZE, MAX_GUARANTEED_FILE_SIZE defined in main file.
 */
static char *cloud_key_pages_protect_enabled         =  "ppt_enable";
static char *cloud_key_pages_protect_swappiness      =  "ppt_swappiness";
static char *cloud_key_pages_protect_threshold       =  "ppt_threshold";

/*
 * For performance and TMPS operation convenience, 'Special Memcg Enabled' is simplified to
 * smemcg_enable, with "smemcg" as the prefix.
 *
 *      smemcg_enable:  >0 enabled, <0 disabled;
 *  memcg_thd_percent:  (0, 100], percentage of the file threshold for special memcg.
 */
static char *cloud_key_special_memcg_enabled         =  "smemcg_enable";
static char *cloud_key_memcg_file_thd_percent        =  "memcg_thd_percent";

/*
 * For performance and TMPS operation convenience, 'Inodes Protection Enabled' is simplified to
 * ip_enable, with "ip" as the prefix.
 *
 *      ip_enable:  >0 enabled, <0 disabled;
 */
static char *cloud_key_inodes_protect_enabled        =  "ip_enable";

/*
 * For performance and TMPS operation convenience, 'Kswapd Desperate Enabled' is simplified to
 * kd_enable, with "kd" as the prefix.
 *
 *      kd_enable:  >0 enabled, <0 disabled;
 */
static char *cloud_key_kswapd_desperate_enabled        =  "kd_enable";

/*
 * For performance and TMPS operation convenience, 'MGLRU Aging Bypass Enabled' is simplified to
 * mab_enable, with "mab" as the prefix.
 *
 *      mab_enable:  >0 enabled, <0 disabled;
 */
static char *cloud_key_mglru_aging_bypass_enabled        =  "mab_enable";

extern bool update_dynamic_tune_enabled(bool);
extern int  update_dynamic_tune_swappiness(int);
extern int  update_dynamic_pp_threshold(int);
extern bool update_special_memcg_enabled(bool enabled);
extern int  update_memcg_file_thd_percentage(int percentage);
extern bool update_inodes_protect_enabled(bool enabled);
extern bool update_kswapd_desperate_enabled(bool enabled);
extern bool update_mglru_aging_bypass_enabled(bool enabled);

static void tranmem_cloud_config_parsed(void)
{
	int state = 0;

	state = cloudengine_getIntByID(tranmem_cloud_config_ID, cloud_key_pages_protect_enabled);
	if (0 == state) {
		pr_err(TRANMM"%s get %s value failed\n", __func__, cloud_key_pages_protect_enabled);
	} else {
		pr_info(TRANMM"%s, %s changes from %d to %d.\n", __func__, cloud_key_pages_protect_enabled,
				update_dynamic_tune_enabled(state > 0), state);
	}

	state = cloudengine_getIntByID(tranmem_cloud_config_ID, cloud_key_pages_protect_swappiness);
	if (0 == state) {
		pr_err(TRANMM"%s get %s value failed\n", __func__, cloud_key_pages_protect_swappiness);
	} else {
		pr_info(TRANMM"%s, %s changes from %d to %d.\n", __func__, cloud_key_pages_protect_swappiness,
				update_dynamic_tune_swappiness(state), state);
	}

	state = cloudengine_getIntByID(tranmem_cloud_config_ID, cloud_key_pages_protect_threshold);
	if (0 == state) {
		pr_err(TRANMM"%s get %s value failed\n", __func__, cloud_key_pages_protect_threshold);
	} else {
		pr_info(TRANMM"%s, %s changes from %d to %d.\n", __func__, cloud_key_pages_protect_threshold,
				update_dynamic_pp_threshold(state), state);
	}

	state = cloudengine_getIntByID(tranmem_cloud_config_ID, cloud_key_special_memcg_enabled);
	if (0 == state) {
		pr_err(TRANMM"%s get %s value failed\n", __func__, cloud_key_special_memcg_enabled);
	} else {
		pr_info(TRANMM"%s, %s changes from %d to %d.\n", __func__, cloud_key_special_memcg_enabled,
				update_special_memcg_enabled(state > 0), state);
	}

	state = cloudengine_getIntByID(tranmem_cloud_config_ID, cloud_key_memcg_file_thd_percent);
	if (0 == state) {
		pr_err(TRANMM"%s get %s value failed\n", __func__, cloud_key_memcg_file_thd_percent);
	} else {
		pr_info(TRANMM"%s, %s changes from %d to %d.\n", __func__, cloud_key_memcg_file_thd_percent,
				update_memcg_file_thd_percentage(state), state);
	}

	state = cloudengine_getIntByID(tranmem_cloud_config_ID, cloud_key_inodes_protect_enabled);
	if (0 == state) {
		pr_err(TRANMM"%s get %s value failed\n", __func__, cloud_key_inodes_protect_enabled);
	} else {
		pr_info(TRANMM"%s, %s changes from %d to %d.\n", __func__, cloud_key_inodes_protect_enabled,
				update_inodes_protect_enabled(state > 0), state);
	}

	state = cloudengine_getIntByID(tranmem_cloud_config_ID, cloud_key_kswapd_desperate_enabled);
	if (0 == state) {
		pr_err(TRANMM"%s get %s value failed\n", __func__, cloud_key_kswapd_desperate_enabled);
	} else {
		pr_info(TRANMM"%s, %s changes from %d to %d.\n", __func__, cloud_key_kswapd_desperate_enabled,
				update_kswapd_desperate_enabled(state > 0), state);
	}

	state = cloudengine_getIntByID(tranmem_cloud_config_ID, cloud_key_mglru_aging_bypass_enabled);
	if (0 == state) {
		pr_err(TRANMM"%s get %s value failed\n", __func__, cloud_key_mglru_aging_bypass_enabled);
	} else {
		pr_info(TRANMM"%s, %s changes from %d to %d.\n", __func__, cloud_key_mglru_aging_bypass_enabled,
				update_mglru_aging_bypass_enabled(state > 0), state);
	}
}

static int tranmem_cloud_config_changed(struct notifier_block *nb, unsigned long action, void* data)
{
	struct cloud_engine_config* cec;
	char* id;

	if(NULL == data){
		pr_err(TRANMM"%s data == NULL, use the default tranmem configuration.\n", __func__);
		return -1;
	}

	cec = (struct cloud_engine_config*)data;
	id = cec->id;
	if(strcmp(tranmem_cloud_config_ID, id) != 0) {
		return 0;
	}

	//cloudengine_feed_back(tranmem_cloud_config_ID, 1);

	tranmem_cloud_config_parsed();

	return 0;
}

struct notifier_block tranmem_cloud_notifier_block = {
	.notifier_call = tranmem_cloud_config_changed,
};

void tranmem_cloudengine_init(void)
{
	register_cloudengine_notifier(tranmem_cloud_config_ID, "v1.x", &tranmem_cloud_notifier_block);
}

void tranmem_cloudengine_exit(void)
{
	unregister_cloudengine_notifier(tranmem_cloud_config_ID, &tranmem_cloud_notifier_block);
}

