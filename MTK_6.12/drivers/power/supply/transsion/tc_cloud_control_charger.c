#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
#include <linux/module.h>
#include <linux/string.h>
#include "tc_charger.h"
#include "tranlog.h"
#include "tc_common_class.h"

static char* enhance_charger_id = "1003000061";
#if IS_ENABLED(CONFIG_TTRAN_THIRD_PARTY_TA_PPS_KERNEL_FLAG)
static char* enable_third_party_ta_pps_id = "1003000070";
#endif

extern int register_cloudengine_notifier(const char* id, const char* ver, struct notifier_block *nb);
extern int unregister_cloudengine_notifier(const char* id, struct notifier_block *nb);
extern void cloudengine_feed_back(const char* id, int state);
extern char* cloudengine_getConfig(const char* id);
extern char* cloudengine_getStringByContent(char* content, const char* key);
extern int cloudengine_getIntByContent(char* content, const char* key);
extern char* cloudengine_getStringByID(const char* id, const char* key);
extern int cloudengine_getIntByID(const char* id, const char* key);


static void enhance_charger_cloud_engine_update_switch(void)
{
    int ret = -1;
    struct tran_device *tc_charger_dev;
    struct tc_charger *info;
    
    tc_charger_dev = tran_get_by_name("tc_charger");
    if (!tc_charger_dev) {
        tchr_err("charger_cloud_engine: can't find tc_charger_dev\n");
        return;
    }
    
    info = tran_get_data(tc_charger_dev);
    if (!info) {
        tchr_err("charger_cloud_engine: info pointer is NULL\n");
        return;
    }
    
    ret = cloudengine_getIntByID(enhance_charger_id, "enable_enhance_mode");
	tchr_err("enable_enhance_mode is %d\n", ret);
    //ret = 0--->close
    if (ret == 0) {
        info->desc->support_enhance_mode = 0;
    } else {
        info->desc->support_enhance_mode = 1;
    }
}

#if IS_ENABLED(CONFIG_TTRAN_THIRD_PARTY_TA_PPS_KERNEL_FLAG)
static void third_party_ta_pps_cloud_engine_update_switch(void)
{
    int ret = -1;
    struct tran_device *tc_charger_dev;
    struct tc_charger *info;
    
    tc_charger_dev = tran_get_by_name("tc_charger");
    if (!tc_charger_dev) {
        tchr_err("charger_cloud_engine: can't find tc_charger_dev\n");
        return;
    }
    
    info = tran_get_data(tc_charger_dev);
    if (!info) {
        tchr_err("charger_cloud_engine: info pointer is NULL\n");
        return;
    }
    

    ret = cloudengine_getIntByID(enable_third_party_ta_pps_id, "enable_third_party_ta_pps");
    tchr_err("enable_third_party_ta_pps is %d\n", ret);
    //ret = 0--->close
    if (ret == 0) {
        info->desc->support_third_party_ta_pps = 0;
    } else {
        info->desc->support_third_party_ta_pps = 1;
    }
}
#endif

//do not modify,func must return 0
static int charger_cloud_engine_callback_func(struct notifier_block *nb, unsigned long action, void* data)
{
    struct cloud_engine_config* cec;
    char* id;

    if (NULL == data) {
        tchr_err("charger_cloud_engine: callback data is NULL\n");
        return -1;
    }
    cec = (struct cloud_engine_config*)data;
    id = cec->id;
    
    tchr_err("charger_cloud_engine: callback received data, id=%s, action=%lu\n", id, action);
    
    if (strcmp(enhance_charger_id, id) == 0) {
        tchr_err("charger_cloud_engine: enhance_charger_id matched, updating switch\n");
        enhance_charger_cloud_engine_update_switch();
    }
#if IS_ENABLED(CONFIG_TTRAN_THIRD_PARTY_TA_PPS_KERNEL_FLAG)
    else if (strcmp(enable_third_party_ta_pps_id, id) == 0) {
        tchr_err("charger_cloud_engine: enable_third_party_ta_pps_id matched, updating switch\n");
        third_party_ta_pps_cloud_engine_update_switch();
    }
#endif

    return 0;
}

struct notifier_block charger_cloud_notifier_block = {
    .notifier_call = charger_cloud_engine_callback_func,
};

int charger_cloud_engine_init(void)
{
    // charger_cloud_notifier_block，Only need to register once, there is no need to register again for adding IDs in the future
    int ret = register_cloudengine_notifier(enhance_charger_id, "v1.0", &charger_cloud_notifier_block);
    if (ret == 0) {
        tchr_err("charger_cloud_engine: register cloudengine notifier success, id=%s\n", enhance_charger_id);
    } else {
        tchr_err("charger_cloud_engine: register cloudengine notifier failed, ret=%d\n", ret);
    }

    return ret;
}


void charger_cloud_engine_exit(void)
{
    unregister_cloudengine_notifier(enhance_charger_id, &charger_cloud_notifier_block);
}
#endif
