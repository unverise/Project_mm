/*Transsion Top Secret*/
/*
 * Copyright (C) 2023 Transsion Inc.
 *
 * @author chonglin.yang
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>

#include "trace_hung_binder.h"
#include "trace_hung_cpu.h"
#include "trace_hung_task.h"
#include "system_monitor.h"
#include "binder_monitor.h"
#include "aegisk.h"
#include "tranlog.h"

static char* regID = "1008000016";
static const char disable_binder_key[] = "cloud_hung_binder_disable";
static const char disable_cpu_key[] = "cloud_hung_cpu_disable";
static const char disable_monitor_key[] = "csmd";//"cloud_system_monitor_disable";
static bool is_cloud_engine_notifier_registed = false;
static bool is_binder_hung_inited = false;
static bool is_cpu_hung_inited = false;
static bool is_system_monitor_inited = false;
static char* disable_binder_str = NULL;
static char* disable_cpu_str = NULL;
static char* disable_monitor_str = NULL;

static void update_cloud_value(void)
{
	disable_binder_str = cloudengine_getStringByID(regID,disable_binder_key);
	disable_cpu_str = cloudengine_getStringByID(regID,disable_cpu_key);
	disable_monitor_str = cloudengine_getStringByID(regID,disable_monitor_key);
	printk("cloud control, update_cloud_value,disable_binder_str:%s ,disable_cpu_str:%s,disable_monitor_str:%s\n",disable_binder_str,disable_cpu_str,disable_monitor_str);
}

static int cloude_change(struct notifier_block *nb, unsigned long action, void* data)
{
	struct cloud_engine_config* cec;
	char* id;
	if(NULL == data)
		return -1;
	cec = (struct cloud_engine_config*)data;
	id = cec->id;
	//printk("cloud control,cloude_change,id:%s,config:%s \n",id,cec->config);
	if(strcmp(regID, id) == 0)
	{
		cloudengine_feed_back(regID, 1);
		update_cloud_value();
		if(is_binder_hung_inited && !IS_ERR_OR_NULL(disable_binder_str) && (strcmp("true",disable_binder_str) == 0)){
			trace_hung_binder_exit();
			is_binder_hung_inited = false;
			printk("cloud control, trace_hung_binder_exit \n");
		} else if(!is_binder_hung_inited && (IS_ERR_OR_NULL(disable_binder_str) || strcmp("true",disable_binder_str) != 0)){
			is_binder_hung_inited = trace_hung_binder_init();
			printk("cloud control, trace_hung_binder_init,is_binder_hung_inited:%d \n",is_binder_hung_inited);
		}
		if(is_cpu_hung_inited && !IS_ERR_OR_NULL(disable_cpu_str) && (strcmp("true",disable_cpu_str) == 0)){
			trace_hung_cpu_exit();
			is_cpu_hung_inited = false;
			printk("cloud control, trace_hung_cpu_exit \n");
		} else if(!is_cpu_hung_inited && (IS_ERR_OR_NULL(disable_cpu_str) || strcmp("true",disable_cpu_str) != 0)){
			is_cpu_hung_inited = trace_hung_cpu_init();
			printk("cloud control, trace_hung_cpu_init,is_cpu_hung_inited:%d \n",is_cpu_hung_inited);
		}
		if(is_system_monitor_inited && !IS_ERR_OR_NULL(disable_monitor_str) && (strcmp("true",disable_monitor_str) == 0)){
			system_monitor_exit();
			is_system_monitor_inited = false;
			printk("cloud control, system_monitor_exit \n");
		} else if(!is_system_monitor_inited && (IS_ERR_OR_NULL(disable_monitor_str) || strcmp("true",disable_monitor_str) != 0)){
			is_system_monitor_inited = system_monitor_init();
			printk("cloud control, system_monitor_init,is_system_monitor_inited:%d \n",is_system_monitor_inited);
		}
	}
	return 0;
}

struct notifier_block cloude_notifier_block = {
    .notifier_call = cloude_change,
};

 void cloud_engine_init(void)
{
	int ret = -1;
	if(!is_cloud_engine_notifier_registed){
		ret = register_cloudengine_notifier(regID, "v1.0", &cloude_notifier_block);
		if(ret == 0){
			is_cloud_engine_notifier_registed = true;
			printk("cloud control,cloud_engine_init successfull");
		}
	}
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


static int __init hung_dectect_module_init(void)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
	update_cloud_value();
	cloud_engine_init();
	aegisk_init();
	if(IS_ERR_OR_NULL(disable_binder_str) || (strcmp("true",disable_binder_str) != 0))
	{
		is_binder_hung_inited = trace_hung_binder_init();
	}
	if(IS_ERR_OR_NULL(disable_cpu_str) || (strcmp("true",disable_cpu_str) != 0))
	{
		is_cpu_hung_inited = trace_hung_cpu_init();
	}
	trace_hung_task_init();
	if(IS_ERR_OR_NULL(disable_monitor_str) || (strcmp("true",disable_monitor_str) != 0))
	{
		is_system_monitor_inited = system_monitor_init();
	}
#if IS_ENABLED(CONFIG_TTRAN_AEGIS_KERNEL_FLAG)
	binder_monitor_init();
#endif
#endif
	return ret;
}

static void __exit hung_dectect_module_exit(void)
{
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
	cloud_engine_exit();
	trace_hung_binder_exit();
	trace_hung_cpu_exit();
	trace_hung_task_exit();
	system_monitor_exit();
	is_binder_hung_inited = false;
	is_cpu_hung_inited = false;
	is_system_monitor_inited = false;
	aegisk_exit();
#if IS_ENABLED(CONFIG_TTRAN_AEGIS_KERNEL_FLAG)
	binder_monitor_exit();
#endif
#endif
}

module_init(hung_dectect_module_init);
module_exit(hung_dectect_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Transsion Hung Detect");
MODULE_AUTHOR("Transsion Inc.");
