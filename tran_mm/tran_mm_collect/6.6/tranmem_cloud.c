/**
  *Copyright 2023 Transsion Inc
  *Author transsioner
  *tran_mm_collect feature suppport
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>

#include <tranlog.h>

#include "tranmem_cloud.h"
//#include "tranmem_ioblock.h"
#include "tranmem_emem.h"

static char *tranmem_cloud_config_ID        =  "1003000026";

static char *cloud_key_psi_limit_interval   =  "psi_limit_interval";

#ifdef CONFIG_TASK_DELAY_ACCT
//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 start
static char *tranmem_delayacct_enable_key = "delayacct_enable";
//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 end

//1, enable delayacct; 2, disable delayacct;
extern int delayacct_enable;
#endif

//static char *cloud_key_ioblock_enable       =  "ioblock_enable";

extern int tranmem_psi_limit_interval;
//extern int tranmem_ioblock_enable;

static void tranmem_cloud_config_parsed(void)
{
	int state = 0;

//	state = cloudengine_getIntByID(tranmem_cloud_config_ID, cloud_key_ioblock_enable);
//	if (0 == state) {
//		pr_err(TRANMM"%s get %s value failed\n", __func__, cloud_key_ioblock_enable);
//	} else {
//		pr_info(TRANMM"%s, %s changes from %d to %d.\n", __func__, cloud_key_ioblock_enable, tranmem_ioblock_enable, state);
//		tranmem_ioblock_cloud_changed(state);
//	}

	state = cloudengine_getIntByID(tranmem_cloud_config_ID, cloud_key_psi_limit_interval);
	if (0 == state) {
		pr_err(TRANMM"%s get %s value failed\n", __func__, cloud_key_psi_limit_interval);
	} else {
		pr_info(TRANMM"%s, %s changes from %d to %d.\n", __func__, cloud_key_psi_limit_interval, tranmem_psi_limit_interval, state);
		tranmem_psi_limit_interval = state;
	}

#ifdef CONFIG_TASK_DELAY_ACCT
//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 start
	state = cloudengine_getIntByID(tranmem_cloud_config_ID, tranmem_delayacct_enable_key);
	if (0 == state) {
		pr_err(TRANMM"%s get %s value failed\n", __func__, tranmem_delayacct_enable_key);
	} else {
		pr_info(TRANMM"%s, delayacct configuration changes from %d to %d.\n", __func__, delayacct_enable, state);
		delayacct_enable = state;
	}
//SPD: add XLBYYHBWQ-3103 for delayacct by bing.han at 20240724 end
#endif
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

	cloudengine_feed_back(tranmem_cloud_config_ID, 1);

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

