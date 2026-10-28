#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
#include <linux/module.h>
#include <linux/string.h>
#include "tranlog.h"

static char* regID = "1008000016";
int sensorhub_tne_flag = 1;

extern int register_cloudengine_notifier(const char* id, const char* ver, struct notifier_block *nb);
extern int unregister_cloudengine_notifier(const char* id, struct notifier_block *nb);
extern void cloudengine_feed_back(const char* id, int state);
extern char* cloudengine_getConfig(const char* id);
extern char* cloudengine_getStringByContent(char* content, const char* key);
extern int cloudengine_getIntByContent(char* content, const char* key);
extern char* cloudengine_getStringByID(const char* id, const char* key);
extern int cloudengine_getIntByID(const char* id, const char* key);

static void sensorhub_cloud_engine_updata_switch(void)
{
    static int ret = -1;
    ret = cloudengine_getIntByID(regID, "sensorhub_tne_switch");
    //ret = 1--->close
    if (ret == 1) {
        sensorhub_tne_flag = 0;
    } else {
        sensorhub_tne_flag = 1;
    }
}

//do not modify,func must return 0
static int sensorhub_cloud_engine_callback_func(struct notifier_block *nb, unsigned long action, void* data)
{
    struct cloud_engine_config* cec;
    char* id;
    char* config;
    if (NULL == data) {
        return -1;
    }
    cec = (struct cloud_engine_config*)data;
    id = cec->id;
    if (strcmp(regID, id) == 0) {
        config = cec->config;
        //1--->close
        sensorhub_cloud_engine_updata_switch();
    }
    return 0;
}

struct notifier_block sensorhub_cloud_notifier_block = {
    .notifier_call = sensorhub_cloud_engine_callback_func,
};

int sensorhub_cloud_engine_init(void)
{
    register_cloudengine_notifier(regID, "v1.0", &sensorhub_cloud_notifier_block);
    sensorhub_cloud_engine_updata_switch();
    return 0;
}

void sensorhub_cloud_engine_exit(void)
{
    unregister_cloudengine_notifier(regID, &sensorhub_cloud_notifier_block);
}
#endif