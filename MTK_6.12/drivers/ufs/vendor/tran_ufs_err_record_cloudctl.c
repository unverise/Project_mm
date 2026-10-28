#include <linux/module.h>
#include "tranlog.h"
static char* regID = "1008000016";
static const char disable_uer_key[] = "cloud_uer_disable";
static bool is_cloud_engine_notifier_registed = false;
bool uer_cloudctl_statu = true;
static int cloude_change(struct notifier_block *nb, unsigned long action, void* data)
{
	struct cloud_engine_config* cec;
	char* id;
	char* disable_uer_str;
	if(NULL == data)
		return -1;
	cec = (struct cloud_engine_config*)data;
	id = cec->id;
	printk("cloud control,cloude_change,id:%s,config:%s \n",id,cec->config);
	if(strcmp(regID, id) == 0)
	{
		cloudengine_feed_back(regID, 1);
		disable_uer_str = cloudengine_getStringByID(regID,disable_uer_key);
		if(!IS_ERR_OR_NULL(disable_uer_str)&&(strcmp("true",disable_uer_str) == 0)){
			uer_cloudctl_statu = false;
			printk("cloud control, uer disable \n");
		} else {
			uer_cloudctl_statu = true;
			printk("cloud control, uer enable \n");
		}
	}
	return 0;
}

struct notifier_block cloude_notifier_block = {
    .notifier_call = cloude_change,
};
int cloud_engine_init(void)
{
	int ret = -1;
	if(!is_cloud_engine_notifier_registed){
		ret = register_cloudengine_notifier(regID, "v1.0", &cloude_notifier_block);
		if(ret == 0){
			is_cloud_engine_notifier_registed = true;
			printk("cloud control,cloud_engine_init successfull");
		}
	}
	return ret;
}
void cloud_engine_exit(void)
{
	int ret = -1;
	if(is_cloud_engine_notifier_registed){
		ret = unregister_cloudengine_notifier(regID, &cloude_notifier_block);
		if(ret == 0){
			is_cloud_engine_notifier_registed = false;
			printk("cloud control,cloud_engine_exit successfull");
		}
	}
}
