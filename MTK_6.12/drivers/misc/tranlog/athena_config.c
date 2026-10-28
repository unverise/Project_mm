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

#include "tranlog.h"
#include "athena.h"

#define logd(x...) pr_err("[athena]" x);
//#define logd(x...) pr_debug("[athena]" x);
#define logw(x...) pr_warn("[athena]" x);
#define loge(x...) pr_err("[athena]" x);

struct tid_state {
    struct hlist_node   hash;
    long long tid;
    int enable;
};

#define TID_HASH_BITS (9)
static DEFINE_HASHTABLE(tid_hash_table, TID_HASH_BITS);

static BLOCKING_NOTIFIER_HEAD(tid_notify_chain);

struct athena_ctl {
    struct miscdevice   miscdev;
    wait_queue_head_t   wq;
    struct mutex        mutex;
    int user_exp;
};

struct athena_ctl *g_ctl = NULL;


int register_athena_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_register(&tid_notify_chain, nb);
}
EXPORT_SYMBOL_GPL(register_athena_notifier);

int unregister_athena_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_unregister(&tid_notify_chain, nb);
}
EXPORT_SYMBOL_GPL(unregister_athena_notifier);

static void notify_tid_changed(void)
{
    blocking_notifier_call_chain(&tid_notify_chain, 0, 0);
}


static int athena_ctl_open(struct inode *inode, struct file *file)
{
    struct athena_ctl *ctl = g_ctl;

    if(ctl == NULL) {
        return -1;
    }

    if(file->private_data == NULL) {
        file->private_data = ctl;
    }

    return 0;
}

static int athena_ctl_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long long tid_hash(long long tid)
{
    return tid;
}

static struct tid_state* find_tid_info(long long tid)
{
    struct hlist_node *tmp;
    struct tid_state *tid_state;

    hash_for_each_possible_safe(tid_hash_table, tid_state, tmp, hash, tid_hash(tid)) {
        if(tid_state && (tid_state->tid == tid)) {
            return tid_state;
        }
    }

    return NULL;
}


static void set_tid(long long tid, int state)
{
    struct tid_state *tid_state;

    if(tid <= 0) {
        return;
    }

//  logd("%s: %d -> %d\n", __func__, tid, state);

    tid_state = find_tid_info(tid);
    if(tid_state == NULL) {
        tid_state = kzalloc(sizeof(struct tid_state), GFP_KERNEL);
        if(tid_state == NULL) {
            loge("alloc tid_state error\n");
            return;
        }
        tid_state->tid = tid;
        tid_state->enable = state;
        hash_add(tid_hash_table, &tid_state->hash, tid_hash(tid));
    } else {
        tid_state->enable = state;
    }
}

static void dump(void)
{
    int bkt = 0;
    struct hlist_node *tmp;
    struct tid_state *tid_state = NULL;

    logd("\ndump start\n");
    hash_for_each_safe(tid_hash_table, bkt, tmp, tid_state, hash) {
        if(tid_state != NULL) {
            logd("tid:%lld -> %d\n", tid_state->tid, tid_state->enable);
        }
    }
    logd("dump end\n");
}

static void clear_tid_state(void)
{
    int bkt = 0;
    struct hlist_node *tmp;
    struct tid_state *tid_state = NULL;

    logd("%s\n", __func__);

    hash_for_each_safe(tid_hash_table, bkt, tmp, tid_state, hash) {
        if(tid_state != NULL) {
            tid_state->enable = 0;
        }
    }
}

int is_tid_enable(long long tid)
{
    int bkt = 0;
    struct hlist_node *tmp;
    struct tid_state *tid_state = NULL;
    struct athena_ctl *ctl = g_ctl;
    int ret = 0;

    if(ctl == NULL) {
        return 0;
    }

    mutex_lock(&ctl->mutex);
    hash_for_each_safe(tid_hash_table, bkt, tmp, tid_state, hash) {
        if(tid_state != NULL) {
            if(tid_state->tid == tid) {
                ret = tid_state->enable && ctl->user_exp;
                goto out;
            }
        }
    }
out:
    mutex_unlock(&ctl->mutex);
    return ret;
}
EXPORT_SYMBOL(is_tid_enable);

static long athena_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct ioc_tid tid_ctl;
    struct athena_ctl *ctl = g_ctl;
    int count = 0;
    long long *tids = NULL; 
    int i = 0;

    ctl = file->private_data;
    if (!ctl) {
        return -ENODEV;
    }

    if(_IOC_TYPE(cmd) != ATHENA_IOC_MAGIC)
        return -EINVAL;

    switch(cmd) {
        case ATHENA_SET_TID:
            mutex_lock(&ctl->mutex);
            memset(&tid_ctl, 0, sizeof(struct ioc_tid));

            if(copy_from_user(&tid_ctl, (void *)arg, sizeof(struct ioc_tid))) {
                loge("%s:copy from user failed\n", __func__);
                mutex_unlock(&ctl->mutex);
                return -EFAULT;
            }

            set_tid(tid_ctl.tid, tid_ctl.state);
            mutex_unlock(&ctl->mutex);
            notify_tid_changed();
            break;
        case ATHENA_SET_TIDS:
            mutex_lock(&ctl->mutex);
            if(get_user(count, (long long *)arg)) {
                logd("get the size error\n");
                mutex_unlock(&ctl->mutex);
                return -EFAULT;
            }
            mutex_unlock(&ctl->mutex);

            if(count == 0) {
                mutex_lock(&ctl->mutex);
                clear_tid_state();
                mutex_unlock(&ctl->mutex);
                return 0;
            }

            count += 1;
            tids = (long long *)kzalloc(count * sizeof(long long), GFP_KERNEL);
            if(tids == NULL) {
                logd("alloc tids error\n");
                return -EFAULT;
            }

            if(copy_from_user(tids, (void *)arg, sizeof(long long)*count)) {
                logd("copy_from_user tids error\n");
                kfree(tids);
                return -EFAULT;
            }

            mutex_lock(&ctl->mutex);
            clear_tid_state();
            for(i=1; i<count; i++) {
                set_tid(tids[i], 1);
            }
            mutex_unlock(&ctl->mutex);
            kfree(tids);

            notify_tid_changed();
            break;
        case ATHENA_CLR_TID:
            mutex_lock(&ctl->mutex);
            clear_tid_state();
            mutex_unlock(&ctl->mutex);
            notify_tid_changed();
            break;
        case ATHENA_GET_TID:
            mutex_lock(&ctl->mutex);
            memset(&tid_ctl, 0, sizeof(struct ioc_tid));
            if(copy_from_user(&tid_ctl, (void *)arg, sizeof(struct ioc_tid))) {
                loge("%s:copy from user failed\n", __func__);
                mutex_unlock(&ctl->mutex);
                return -EFAULT;
            }
            mutex_unlock(&ctl->mutex);

            logd("get tid:%lld\n", tid_ctl.tid);
            tid_ctl.state = is_tid_enable(tid_ctl.tid);
    
            if(copy_to_user((void *)arg, &tid_ctl, sizeof(struct ioc_tid))) {
                loge("%s:copy to user failed\n", __func__);
                return -EFAULT;
            }

            break;
        case ATHENA_NFY_TID:
            logd("tid notify\n");
            notify_tid_changed();
            dump();
            break;
        case ATHENA_SET_EXP:
            mutex_lock(&ctl->mutex);
            if(get_user(ctl->user_exp, (int *)arg)) {
                logd("get the user exp error\n");
                mutex_unlock(&ctl->mutex);
                return -EFAULT;
            }
            mutex_unlock(&ctl->mutex);
            logd("set user exp:%d\n", ctl->user_exp); 
            notify_tid_changed();
            break;
        default:
            break; 
    }

    return 0;
}

static ssize_t athena_ctl_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    return count;
}

static const struct file_operations athena_ctl_fops = {
    .owner =        THIS_MODULE,
    .open =         athena_ctl_open,
    .release =      athena_ctl_release,
    .write =        athena_ctl_write,
    .unlocked_ioctl = athena_ioctl,
};

static struct athena_ctl* create_device(void)
{
    struct athena_ctl *ctl = NULL;
    int ret = 0;

    ctl = kzalloc(sizeof(struct athena_ctl), GFP_KERNEL);
    if(!ctl) {
        loge("%s: failed\n", __func__);
        ret = -ENOMEM;
        goto error;
    }

    ctl->miscdev.fops = &athena_ctl_fops;
    ctl->miscdev.minor = 252;
    ctl->miscdev.name = kstrdup("tranlog_ctl", GFP_KERNEL);;
    if(ctl->miscdev.name == NULL) {
        ret = -ENOMEM;
        goto error;
    }

    ret = misc_register(&ctl->miscdev);
    if(ret != 0) {
        loge("%s:register misc device failed", __func__);
        goto error;
    }

    init_waitqueue_head(&ctl->wq);
    mutex_init(&ctl->mutex);

    return ctl;

error:
    if((ctl != NULL) && ctl->miscdev.name) {
        kfree(ctl->miscdev.name);
    }

    if(ctl) {
        kfree(ctl);
    }

    return NULL;
}

void athena_ctl_init(void)
{
    g_ctl = create_device();
}

void athena_ctl_exit(void)
{
    if(g_ctl) {
        misc_deregister(&g_ctl->miscdev);
        g_ctl = NULL;
    }
}
