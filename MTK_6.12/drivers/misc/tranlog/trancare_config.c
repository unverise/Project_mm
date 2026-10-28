#include <linux/init.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/uio.h>
#include <linux/rtc.h>
//#include <linux/kfifo.h>
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
#include "tran_kfifo.h"


#define logd(x...) pr_err("[CloudEngine_kernel]" x);
#define logw(x...) pr_warn("[CloudEngine_kernel]" x);
#define loge(x...) pr_err("[CloudEngine_kernel]" x);


struct cloudengine_config {
    struct miscdevice       miscdev;
    struct mutex            mutex;
};
struct cloudengine_config *g_cloudengine_config = NULL;

char *g_cloudengine_id = NULL;

//trancare config data change
static int cloudengine_config_open(struct inode *inode, struct file *file)
{
    struct cloudengine_config *ctl = g_cloudengine_config;

    if(ctl == NULL) {
        return -1;
    }

    if(file->private_data == NULL) {
        file->private_data = ctl;
    }
    return 0;
}

static int cloudengine_config_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t cloudengine_config_write(struct kiocb *iocb, struct iov_iter *from)
{
    struct cloudengine_config *tranlog = iocb->ki_filp->private_data;
    size_t len = iov_iter_count(from);

    if (!tranlog)
    {
        return -ENODEV;
    }

    mutex_lock(&tranlog->mutex);
    if(g_cloudengine_id != NULL)
    {
        kfree(g_cloudengine_id);
        g_cloudengine_id = NULL;
    }

    g_cloudengine_id = kzalloc(len + 1, GFP_KERNEL);
    if(g_cloudengine_id == NULL)
    {
        mutex_unlock(&tranlog->mutex);
        return -ENOMEM;
    }

    if(copy_from_iter(g_cloudengine_id, len, from) != len) {
        kfree(g_cloudengine_id);
        g_cloudengine_id = NULL;
        mutex_unlock(&tranlog->mutex);
        return -EFAULT;
    }

    g_cloudengine_id[len] = '\0';



    mutex_unlock(&tranlog->mutex);

    return len;
}

static ssize_t cloudengine_config_read(struct file *file, char __user *buffer, size_t count,
              loff_t *ppos)
{
    struct cloudengine_config *tranlog = NULL;
    size_t ret = 0;
    size_t len = 0;
    tranlog = file->private_data;
    if (!tranlog) {
        return -ENODEV;
    }

    mutex_lock(&tranlog->mutex);
    if(g_cloudengine_id != NULL)
    {
        len = strlen(g_cloudengine_id) + 1;
        if (count < len)
        {
            loge("reader buffer is too small, size:%zu, count:%zu\n", len, count);
            mutex_unlock(&tranlog->mutex);
            goto out;
        }

        ret = copy_to_user(buffer, g_cloudengine_id, len);
    }
    mutex_unlock(&tranlog->mutex);

out:
    if(ret)
    {
        return -EFAULT;
    } else {
        return len;
    }
}

static const struct file_operations cloudengine_config_fops = {
    .owner =        THIS_MODULE,
    .open =         cloudengine_config_open,
    .release =      cloudengine_config_release,
    .write_iter =   cloudengine_config_write,
    .read =         cloudengine_config_read,
};

static struct cloudengine_config* create_device_config(int minor_number) {
    struct cloudengine_config *ctl = NULL;
    int ret = 0;

    ctl = kzalloc(sizeof(struct cloudengine_config), GFP_KERNEL);
    if(!ctl) {
        loge("%s: failed\n", __func__);
        ret = -ENOMEM;
        goto error;
    }

    ctl->miscdev.fops = &cloudengine_config_fops;
    ctl->miscdev.minor = minor_number;
    ctl->miscdev.name = kstrdup("tranlog_config", GFP_KERNEL);;
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
    if(ctl->miscdev.name) {
        kfree(ctl->miscdev.name);
    }

    if(ctl) {
        kfree(ctl);
    }
    return NULL;
}

#define MINOR_NUMBER_CONFIG     251

void cloudengine_config_init(void)
{
    g_cloudengine_config = create_device_config(MINOR_NUMBER_CONFIG);
    if(g_cloudengine_config == NULL)
    {
        g_cloudengine_config = create_device_config(MISC_DYNAMIC_MINOR);
    }
}

void cloudengine_config_exit(void)
{
    if(g_cloudengine_config) {
        misc_deregister(&g_cloudengine_config->miscdev);
        kfree(g_cloudengine_config);
        g_cloudengine_config = NULL;
    }
}
