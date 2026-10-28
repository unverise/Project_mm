#include <linux/init.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/uio.h>
#include <linux/rtc.h>
#include <linux/kfifo.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>

#include "tranlog.h"
#include "trancare_config.h"
#include "cloud_engine.h"
#include "arraylist.h"


#define logd(x...) pr_err("[CloudEngine_kernel]" x);
#define logw(x...) pr_warn("[CloudEngine_kernel]" x);
#define loge(x...) pr_err("[CloudEngine_kernel]" x);


struct cloudengine_ctl
{
    struct miscdevice       miscdev;
    struct mutex            mutex;
};

struct cloudengine_reg_info
{
    char* id;
    char* ver;
    int state;
};

struct cloudengine_ctl *g_cloudengine_ctl = NULL;
static arraylist* g_list = NULL;
static int g_native_boot = 0;


//cloud engine
static BLOCKING_NOTIFIER_HEAD(cloudengine_notify_chain);

/**
**register cloud engine notifier
**/
int register_cloudengine_notifier(const char* id, const char* ver, struct notifier_block *nb)
{
    return blocking_notifier_chain_register(&cloudengine_notify_chain, nb);

}
EXPORT_SYMBOL_GPL(register_cloudengine_notifier);

/**
**unregister cloud engine notifier
**/
int unregister_cloudengine_notifier(const char* id, struct notifier_block *nb)
{
    return blocking_notifier_chain_unregister(&cloudengine_notify_chain, nb);
}
EXPORT_SYMBOL_GPL(unregister_cloudengine_notifier);

/**
**feed back config state
**/
void cloudengine_feed_back(const char* id, int state)
{
   //do nothings
}
EXPORT_SYMBOL_GPL(cloudengine_feed_back);

/**
**get string by content    (key+value;key+value)
**/
char* cloudengine_getStringByContent(char* content, const char* key)
{
    char* p;
    char* tag;
    char* temp;
    char* ret = NULL;
    char* org_temp;
    int length;

    if(NULL == content || NULL == key)
        return NULL;

    length = strlen(content);
    temp = (char*)kzalloc(length + 1, GFP_KERNEL);
    memcpy(temp, content, length);
    temp[length] = '\0';
    org_temp = temp;

    while((p = strsep(&temp, ";")) != NULL)
    {
        if(strstr(p, key) != NULL)
        {
            while((tag = strsep(&p, "+")) != NULL)
            {
                if(strstr(tag, key) != NULL)
                {
                    continue;
                }
                ret = (char*)kzalloc(strlen(tag) + 1, GFP_KERNEL);
                memcpy(ret, tag, strlen(tag));
                ret[strlen(tag)] = '\0';
                goto END;
            }
        }
    }
END:
    temp = org_temp;
    if(temp) {
        kfree(temp);
        temp = NULL;
        org_temp = NULL;
    }
    return ret;
}
EXPORT_SYMBOL_GPL(cloudengine_getStringByContent);

/**
**get int by content   (key+value;key+value)
**/
int cloudengine_getIntByContent(char* content, const char* key)
{
    char* p;
    char* tag;
    char* temp;
    int ret = 0;
    char* org_temp;
    int length;

    if(NULL == content || NULL == key)
        return 0;

    length = strlen(content);
    temp = (char*)kzalloc(length + 1, GFP_KERNEL);
    memcpy(temp, content, length);
    temp[length] = '\0';
    org_temp = temp;

    while((p = strsep(&temp, ";")) != NULL)
    {
        if(strstr(p, key) != NULL)
        {
            while((tag = strsep(&p, "+")) != NULL)
            {
                if(strstr(tag, key) != NULL)
                {
                    continue;
                }
                ret = simple_strtol(tag, NULL, 10);
                goto END;
            }
        }
    }
END:
    temp = org_temp;
    if(temp) {
        kfree(temp);
        temp = NULL;
        org_temp = NULL;
    }
    return ret;
}
EXPORT_SYMBOL_GPL(cloudengine_getIntByContent);

/**
**get string by ID   (key+value;key+value)
**/
char* cloudengine_getStringByID(const char* id, const char* key)
{
    char* config;
    char* ret;

    if(NULL == id || NULL == key)
        return NULL;

    config = cloudengine_getConfig(id);

    if(NULL == config)
        return NULL;

    ret = cloudengine_getStringByContent(config, key);
    kfree(config);

    return ret;
}
EXPORT_SYMBOL_GPL(cloudengine_getStringByID);

/**
**get int by ID  (key+value;key+value)
**/
int cloudengine_getIntByID(const char* id, const char* key)
{
    char* config;
    int ret;

    if(NULL == id || NULL == key)
        return 0;

    config = cloudengine_getConfig(id);

    if(NULL == config)
        return 0;

    ret = cloudengine_getIntByContent(config, key);
    kfree(config);

    return ret;
}
EXPORT_SYMBOL_GPL(cloudengine_getIntByID);

char* cloudengine_getConfig(const char* id)
{
    int i;
    void* v;
    char* ret = NULL;
    int length;
    struct cloud_engine_config* value;

    if(g_list == NULL || g_list->size == 0)
    {
        return NULL;
    }

    arraylist_iterate(g_list, i, v) {
        value = (struct cloud_engine_config*)v;
        if(strcmp(value->id, id) == 0) {
            length = strlen(value->config);
            ret = (char*) kzalloc(length+1, GFP_KERNEL);
            if(ret)
            {
                memcpy(ret, value->config, length);
                ret[length] = '\0';
            }
            return ret;
        }
    }

    return NULL;
}
EXPORT_SYMBOL_GPL(cloudengine_getConfig);

static void addConfig(struct cloud_engine_config* cec)
{
    int i;
    void* v;
    struct cloud_engine_config* value;

    if(g_list == NULL || cec == NULL)
    {
        return;
    }

    arraylist_iterate(g_list, i, v) {
        value = (struct cloud_engine_config*)v;
        if(strcmp(value->id, cec->id) == 0) {
            arraylist_set(g_list,i,cec);
            return;
        }
    }
    arraylist_add(g_list, cec);
}

static void notify_cloudengine_changed(struct cloud_engine_config* cec)
{
    blocking_notifier_call_chain(&cloudengine_notify_chain, 0, cec);
}

static int cloud_engine_ctl_open(struct inode *inode, struct file *file)
{
    struct cloudengine_ctl *ctl = g_cloudengine_ctl;
    if(ctl == NULL)
    {
        return -1;
    }

    if(file->private_data == NULL)
    {
        file->private_data = ctl;
    }

    return 0;
}

static int cloud_engine_ctl_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t cloud_engine_ctl_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    return count;
}

static long cloud_engine_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct cloud_engine_config* cec;
    struct kernel_cloud_config kernelConfig;
    struct cloudengine_ctl *ctl = g_cloudengine_ctl;
    ctl = file->private_data;
    if (!ctl) {
        loge("[cloud engine kernel] No control data\n");
        return -ENODEV;
    }

    if(_IOC_TYPE(cmd) != CLOUD_ENGINE_IOC_MAGIC) {
        loge("[cloud engine kernel] Invalid command type\n");
        return -EINVAL;
    }

    loge("%s cmd = %d\n",__func__, cmd);
    switch(cmd)
    {
        // bufLen(int) + id length(int) + id(char*) + config length(int) + config(char*)
        case CLOUD_ENGINE_UPDATE_CONFIG:
            mutex_lock(&ctl->mutex);

            if (copy_from_user(&kernelConfig, (struct kernel_cloud_config*)arg, sizeof(struct kernel_cloud_config))) {
                loge("[cloud engine kernel] copy_from_user error!!!!\n");
                mutex_unlock(&ctl->mutex);
                return -EFAULT;
            }

            cec = (struct cloud_engine_config*)kzalloc(sizeof(struct cloud_engine_config), GFP_KERNEL);
            if(cec == NULL)
            {
                loge("[cloud engine kernel] malloc cec failed\n");
                mutex_unlock(&ctl->mutex);
                return -EFAULT;
            }

            cec->id = (char*) kzalloc(kernelConfig.id_len + 1, GFP_KERNEL);
            if(cec->id == NULL)
            {
                loge("[cloud engine kernel] malloc cec->id failed\n");
                kfree(cec);
                mutex_unlock(&ctl->mutex);
                return -EFAULT;
            }

            cec->config = (char*) kzalloc(kernelConfig.config_len + 1, GFP_KERNEL);
            if(cec->config == NULL)
            {
                loge("[cloud engine kernel] malloc cec->config failed\n");
                kfree(cec->id);
                kfree(cec);
                mutex_unlock(&ctl->mutex);
                return -EFAULT;
            }

            if (copy_from_user(cec->id, kernelConfig.id, kernelConfig.id_len) ||
                copy_from_user(cec->config, kernelConfig.config, kernelConfig.config_len)) {
                loge("[cloud engine kernel] copy cec->id or cec->config failed!!!\n");
                kfree(cec->id);
                kfree(cec->config);
                kfree(cec);
                mutex_unlock(&ctl->mutex);
                return -EFAULT;
            }

            cec->id[kernelConfig.id_len] = '\0';
            cec->config[kernelConfig.config_len] = '\0';

            loge("[cloud engine kernel] id=%s, config = %s\n", cec->id, cec->config);

            addConfig(cec);

            mutex_unlock(&ctl->mutex);

            notify_cloudengine_changed(cec);
            break;

        case CLOUD_ENGINE_NOTIDY_REG:
            g_native_boot = 1;
            break;
    }

    return 0;
}

static const struct file_operations cloud_engine_ctl_fops =
{
    .owner =        THIS_MODULE,
    .open =         cloud_engine_ctl_open,
    .release =      cloud_engine_ctl_release,
    .write =        cloud_engine_ctl_write,
    .unlocked_ioctl = cloud_engine_ioctl,
};


static struct cloudengine_ctl* create_device_ctl(int minor_number)
{
    struct cloudengine_ctl *ctl = NULL;
    int ret = 0;

    ctl = kzalloc(sizeof(struct cloudengine_ctl), GFP_KERNEL);
    if(!ctl) {
        loge("%s: failed\n", __func__);
        ret = -ENOMEM;
        goto error;
    }


    ctl->miscdev.fops = &cloud_engine_ctl_fops;
    ctl->miscdev.minor = minor_number;
    ctl->miscdev.name = kstrdup("tranlog_cloudctl", GFP_KERNEL);;
    if(ctl->miscdev.name == NULL) {
        ret = -ENOMEM;
        goto error;
    }

    ret = misc_register(&ctl->miscdev);
    if(ret != 0) {
        loge("%s:register misc device failed", __func__);
        goto error;
    }
    mutex_init(&ctl->mutex);

    return ctl;

error:
    if(ctl) {
        if(ctl->miscdev.name) {
            kfree(ctl->miscdev.name);
        }
        kfree(ctl);
    }
    return NULL;
}

#define MINOR_NUMBER_CLOUD_CTL       249

void cloudengine_ctl_init(void)
{
    g_cloudengine_ctl = create_device_ctl(MINOR_NUMBER_CLOUD_CTL);
    if(g_cloudengine_ctl == NULL)
    {
        g_cloudengine_ctl = create_device_ctl(MISC_DYNAMIC_MINOR);
    }
    g_list = arraylist_create();
}

void cloudengine_ctl_exit(void)
{
    int i;
    void* v;
    struct cloud_engine_config* cec;

    if(g_cloudengine_ctl)
    {
        misc_deregister(&g_cloudengine_ctl->miscdev);
        kfree(g_cloudengine_ctl);
        g_cloudengine_ctl = NULL;
    }

    if(g_list)
    {
        if(g_list->size > 0)
        {
            arraylist_iterate(g_list, i, v) {
                cec = (struct cloud_engine_config*)v;
                kfree(cec->id);
                kfree(cec->config);
                kfree(cec);
            }
        }
        arraylist_destroy(g_list);
    }
}


