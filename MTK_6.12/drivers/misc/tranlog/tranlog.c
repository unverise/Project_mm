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
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/version.h>
#include <linux/time.h>
#include <linux/ktime.h>

#include "tranlog.h"
#include "athena.h"
//SPD:add by chongyang.zhang for trancare config 2020.03.30 start
#include "trancare_config.h"
#include "cloud_engine.h"
//SPD:add by chongyang.zhang for trancare config 2020.03.30 end
#include "tran_kfifo.h"

#define TRANLOG_TO_LOCAL           1
#define TRANLOG_TO_SERVER          2
#define TRANLOG_TO_BOTH            3
//SPD:add by chongyang.zhang for dump data 2020.07.10 start
#define TRANLOG_TO_DUMP            4
#define TRANLOG_TO_DUMP_TRIGGER    5
//SPD:add by chongyang.zhang for dump data 2020.07.10 end

#define TRANLOG_TYPE_LOG      1
#define TRANLOG_TYPE_RAW      2
#define TRANLOG_TYPE_LOG_KEEP 3
#define TRANLOG_TYPE_NV       4

#define TRANLOG_FLAG_APPEND   1
#define TRANLOG_FLAG_REPLACE  2

#define TYPE_INT    1
#define TYPE_STRING 2
#define TYPE_BOOL   3
#define TYPE_LONG   4
#define TYPE_S64    5
#define TYPE_FLOAT  6

#define logd(x...) pr_err("[tranlog]" x);
//#define logd(x...) pr_debug("[tranlog]" x);
#define logw(x...) pr_warn("[tranlog]" x);
#define loge(x...) pr_err("[tranlog]" x);

struct tranlog {
    struct miscdevice       miscdev;
    struct tran_kfifo_rec_ptr_2  buffer;
    wait_queue_head_t       wq;
    struct mutex            mutex;
    struct list_head        list;
    int                     index;
    int                     max_pkg_size;
};

static LIST_HEAD(all_devices);
struct tranlog *g_tranlog = NULL;

//TODO, only s32/s64 type support now; string:TBD
#define LEN_NV_KEY  32
struct watchlist {
    char key[LEN_NV_KEY];
    union {
        int32_t s32;
        int64_t s64;
    };
    int type;
    struct list_head    list;
};
static LIST_HEAD(nv_watchlist);

#if 0
static void dump_watchlist(void)
{
    struct watchlist *cur = NULL;

    list_for_each_entry(cur, &nv_watchlist, list) {
        if(cur->type == TYPE_INT) {
            logd("dump: %s:%d\n", cur->key, cur->s32);
        } else if(cur->type == TYPE_S64) {
            logd("dump: %s:%lld\n", cur->key, cur->s64);
        }
    }
}
#endif

static struct watchlist *find_watchlist(const char *key)
{
    struct watchlist *cur = NULL;

    list_for_each_entry(cur, &nv_watchlist, list) {
        if(!strcmp(cur->key, key)) {
            return cur;
        }
    }

    return NULL;
}

int set_nv_watchlist(const char *key, int type)
{
    struct watchlist *wl;
    struct tranlog *tranlog = g_tranlog;

    if(tranlog == NULL)
        return -1;

    wl = find_watchlist(key);
    if(!wl) {
        wl = (struct watchlist *) kzalloc(sizeof(struct watchlist), GFP_KERNEL);
        if(!wl) {
            loge("malloc watchlist error\n");
            return -1;
        }
    } else {
        return 0;
    }

    mutex_lock(&tranlog->mutex);
    strncpy(wl->key, key, LEN_NV_KEY - 1);
    if(type == TYPE_INT) {
        wl->type = TYPE_INT;
    } else if(type == TYPE_S64) {
        wl->type = TYPE_S64;
    }
    INIT_LIST_HEAD(&wl->list);
    list_add_tail(&wl->list, &nv_watchlist);
    mutex_unlock(&tranlog->mutex);

    return 0;
}
EXPORT_SYMBOL(set_nv_watchlist);

int32_t tranlog_getnv_s32(const char *key, int32_t def)
{
    struct watchlist *wl;
    struct tranlog *tranlog = g_tranlog;
    int32_t ret = def;

    if(tranlog == NULL)
        return ret;

    mutex_lock(&tranlog->mutex);
    wl = find_watchlist(key);
    if(wl) {
        ret = wl->s32;
    }
    mutex_unlock(&tranlog->mutex);

    return ret;
}
EXPORT_SYMBOL(tranlog_getnv_s32);

int64_t tranlog_getnv_s64(const char *key, int64_t def)
{
    struct watchlist *wl;
    struct tranlog *tranlog = g_tranlog;
    int32_t ret = def;

    if(tranlog == NULL)
        return ret;

    mutex_lock(&tranlog->mutex);
    wl = find_watchlist(key);
    if(wl) {
        ret = wl->s64;
    }
    mutex_unlock(&tranlog->mutex);

    return ret;
}
EXPORT_SYMBOL(tranlog_getnv_s64);

static void get_local_time(char *buf, int len)
{
    struct rtc_time tm;
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
        struct timespec64 tv;
    #else
        struct timeval tv;
    #endif

    if(len < 20)
        return;

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
        ktime_get_real_ts64(&tv);
    #else
        do_gettimeofday(&tv);
    #endif
    rtc_time64_to_tm(tv.tv_sec - (sys_tz.tz_minuteswest * 60), &tm);

    snprintf(buf, len-1, "%d-%02d-%02d %02d:%02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
}

static ssize_t __tranlog_local_locked(const char *from, int type, int dest, char *msg, int msg_len)
{
/* msg layout
struct tranlog_pkg_header {
    char tag[32];
    char time[24];
    int type;
    int dest;
    int msg_len;
};
*/
    char ts[24];
    char tag[32];
    int len = 0;
    int offset = 0;
    char *p = NULL;
    struct tranlog *tranlog = g_tranlog;

    if(tranlog == NULL)
        return -1;

    memset(ts, 0, sizeof(ts));
    memset(tag, 0, sizeof(tag));
    get_local_time(ts, sizeof(ts));
    snprintf(tag, sizeof(tag)-1, "%s", from);

    len = sizeof(ts) + sizeof(tag) + sizeof(type) + sizeof(dest) + sizeof(msg_len) + msg_len;
    p = (char *)kzalloc(len, GFP_KERNEL);
    if(p == NULL)
        return -1;

    memcpy(p + offset, tag, sizeof(tag));
    offset += sizeof(tag);
    memcpy(p + offset, ts, sizeof(ts));
    offset += sizeof(ts);
    memcpy(p + offset, &type, sizeof(type));
    offset += sizeof(type);
    memcpy(p + offset, &dest, sizeof(dest));
    offset += sizeof(dest);
    memcpy(p + offset, &msg_len, sizeof(msg_len));
    offset += sizeof(msg_len);
    memcpy(p + offset, msg, msg_len);
    
    //SPD:modify for tranlog pkg size by chongyang.zhang 2020/07/17 start
    if(tranlog->max_pkg_size < len) {
        tranlog->max_pkg_size = len+1;
    }
    //SPD:modify for tranlog pkg size by chongyang.zhang 2020/07/17 end

    tran_kfifo_in(&tranlog->buffer, p, len);

    kfree(p);
    wake_up_interruptible(&tranlog->wq);

    return 0;
}

ssize_t tranlog_local(const char *tag, const char* fmt, ...)
{
    va_list ap;
    char buf[1024];
    struct tranlog *tranlog = g_tranlog;

    if(tranlog == NULL)
        return -1;

    mutex_lock(&tranlog->mutex);
    memset(buf, 0, sizeof(buf));
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf)-1, fmt, ap);
    va_end(ap);

    __tranlog_local_locked(tag, TRANLOG_TYPE_LOG, TRANLOG_TO_LOCAL, buf, strlen(buf));
    mutex_unlock(&tranlog->mutex);

    return 0;
}
EXPORT_SYMBOL(tranlog_local);

//SPD:add by chongyang.zhang for dump data 2020.07.10 start
ssize_t tranlog_dump(const char *tag, const char* fmt, ...)
{
    va_list ap;
    char buf[300];
    struct tranlog *tranlog = g_tranlog;

    if(tranlog == NULL)
        return -1;
    if(strncmp("all", tag, 3) == 0) {
		loge("[tranlog_dump] tag must not set all !!!!!\n");
		return -1;
    }
    mutex_lock(&tranlog->mutex);
    memset(buf, 0, sizeof(buf));
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf)-1, fmt, ap);
    va_end(ap);

    __tranlog_local_locked(tag, TRANLOG_TYPE_LOG, TRANLOG_TO_DUMP, buf, strlen(buf));
    mutex_unlock(&tranlog->mutex);

    return 0;
}
EXPORT_SYMBOL(tranlog_dump);

ssize_t tranlog_dump_trigger(const char *tag)
{
    char buf[20] = "trigger";
    struct tranlog *tranlog = g_tranlog;

    if(tranlog == NULL)
        return -1;

    mutex_lock(&tranlog->mutex);
    __tranlog_local_locked(tag, TRANLOG_TYPE_LOG, TRANLOG_TO_DUMP_TRIGGER, buf, strlen(buf));
    mutex_unlock(&tranlog->mutex);

    return 0;
}
EXPORT_SYMBOL(tranlog_dump_trigger);


//SPD:add by chongyang.zhang for dump data 2020.07.10 end


void tranlog_local_log_nodel(const char *tag, const char* fmt, ...)
{
    va_list ap;
    char buf[1024];
    struct tranlog *tranlog = g_tranlog;

    if(tranlog == NULL)
        return;

    mutex_lock(&tranlog->mutex);
    memset(buf, 0, sizeof(buf));
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf)-1, fmt, ap);
    va_end(ap);

    __tranlog_local_locked(tag, (TRANLOG_TYPE_LOG_KEEP|(TRANLOG_FLAG_APPEND<<8)), TRANLOG_TO_LOCAL, buf, strlen(buf));
    mutex_unlock(&tranlog->mutex);
}
EXPORT_SYMBOL(tranlog_local_log_nodel);

void tranlog_setnv_s32(const char *key, int value, int op)
{
    char msg[32];
    struct tranlog *tranlog = g_tranlog;

    if(tranlog == NULL)
        return;

    mutex_lock(&tranlog->mutex);
    memset(msg, 0, sizeof(msg));
    snprintf(msg, sizeof(msg)-1, "%d", value);
    __tranlog_local_locked(key, (TRANLOG_TYPE_NV|(op << 8)|(TYPE_INT << 16)), TRANLOG_TO_LOCAL, msg, strlen(msg));
    mutex_unlock(&tranlog->mutex);
}
EXPORT_SYMBOL(tranlog_setnv_s32);

void tranlog_setnv_s64(const char *key, long long value, int op)
{
    char msg[32];
    struct tranlog *tranlog = g_tranlog;

    if(tranlog == NULL)
        return;

    mutex_lock(&tranlog->mutex);
    memset(msg, 0, sizeof(msg));
    snprintf(msg, sizeof(msg)-1, "%lld", value);
    __tranlog_local_locked(key, (TRANLOG_TYPE_NV|(op << 8)|(TYPE_S64 << 16)), TRANLOG_TO_LOCAL, msg, strlen(msg));
    mutex_unlock(&tranlog->mutex);
}
EXPORT_SYMBOL(tranlog_setnv_s64);

/*
void tranlog_setnv_float(const char *key, float value, int op)
{
    char msg[32];
    struct tranlog *tranlog = g_tranlog;

    if(tranlog == NULL)
        return;

    mutex_lock(&tranlog->mutex);
    memset(msg, 0, sizeof(msg));
    snprintf(msg, sizeof(msg)-1, "%f", value);
    __tranlog_local_locked(key, (TRANLOG_TYPE_NV|(op << 8)|(TYPE_FLOAT << 16)), TRANLOG_TO_LOCAL,  msg, strlen(msg));
    mutex_unlock(&tranlog->mutex);

}
*/

void tranlog_setnv_string(const char *key, const char* fmt, ...)
{
    va_list ap;
    char buf[1024];
    struct tranlog *tranlog = g_tranlog;

    if(tranlog == NULL)
        return;

    mutex_lock(&tranlog->mutex);
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf)-1, fmt, ap);
    va_end(ap);

    __tranlog_local_locked(key, (TRANLOG_TYPE_NV|(TYPE_STRING << 16)), TRANLOG_TO_LOCAL, buf, strlen(buf));
    mutex_unlock(&tranlog->mutex);
}
EXPORT_SYMBOL(tranlog_setnv_string);

//server log
void msg_put_int(struct athena_msg **athena, char *key, int value)
{
    struct athena_msg *p = *athena;
    struct athena_msg *msg = NULL;

    if(p == NULL)
        return;

    msg = (struct athena_msg *)kzalloc(sizeof(struct athena_msg), GFP_KERNEL);
    if(msg == NULL) {
        return;
    } else {
        msg->key = kzalloc(strlen(key)+1, GFP_KERNEL);
        if(msg->key == NULL) {
            kfree(msg);
            return;
        }

        msg->value = kzalloc(32, GFP_KERNEL);
        if(msg->value == NULL) {
            kfree(msg->key);
            kfree(msg);
            return;
        }

        memcpy(msg->key, key, strlen(key));
        snprintf(msg->value, 31, "%d", value);
        msg->type = TYPE_INT;
        msg->next = NULL;
    }

    while(p->next != NULL) {
        p = p->next;
    }

    p->next = msg;
}
EXPORT_SYMBOL(msg_put_int);

void msg_put_long(struct athena_msg **athena, char *key, long value)
{
    struct athena_msg *p = *athena;
    struct athena_msg *msg = NULL;

    if(p == NULL)
        return;

    msg = (struct athena_msg *)kzalloc(sizeof(struct athena_msg), GFP_KERNEL);
    if(msg == NULL) {
        return;
    } else {
        msg->key = kzalloc(strlen(key)+1, GFP_KERNEL);
        if(msg->key == NULL) {
            kfree(msg);
            return;
        }

        msg->value = kzalloc(32, GFP_KERNEL);
        if(msg->value == NULL) {
            kfree(msg->key);
            kfree(msg);
            return;
        }

        memcpy(msg->key, key, strlen(key));
        snprintf(msg->value, 31, "%ld", value);
        msg->type = TYPE_LONG;
        msg->next = NULL;
    }

    while(p->next != NULL) {
        p = p->next;
    }

    p->next = msg;

}
EXPORT_SYMBOL(msg_put_long);

void msg_put_ll(struct athena_msg **athena, char *key, long long value)
{
    struct athena_msg *p = *athena;
    struct athena_msg *msg = NULL;

    if(p == NULL)
        return;

    msg = (struct athena_msg *)kzalloc(sizeof(struct athena_msg), GFP_KERNEL);
    if(msg == NULL) {
        return;
    } else {
        msg->key = kzalloc(strlen(key)+1, GFP_KERNEL);
        if(msg->key == NULL) {
            kfree(msg);
            return;
        }

        msg->value = kzalloc(32, GFP_KERNEL);
        if(msg->value == NULL) {
            kfree(msg->key);
            kfree(msg);
            return;
        }

        memcpy(msg->key, key, strlen(key));
        snprintf(msg->value, 31, "%lld", value);
        msg->type = TYPE_S64;
        msg->next = NULL;
    }

    while(p->next != NULL) {
        p = p->next;
    }

    p->next = msg;

}
EXPORT_SYMBOL(msg_put_ll);

void msg_put_string(struct athena_msg **athena, char *key, char *value)
{
    struct athena_msg *p = *athena;
    struct athena_msg *msg = NULL;

    if(p == NULL)
        return;

    msg = (struct athena_msg *)kzalloc(sizeof(struct athena_msg), GFP_KERNEL);
    if(msg == NULL) {
        return;
    } else {
        msg->key = kzalloc(strlen(key)+1, GFP_KERNEL);
        if(msg->key == NULL) {
            kfree(msg);
            return;
        }

        msg->value = kzalloc(strlen(value)+1, GFP_KERNEL);
        if(msg->value == NULL) {
            kfree(msg->key);
            kfree(msg);
            return;
        }

        memcpy(msg->key, key, strlen(key));
        memcpy(msg->value, value, strlen(value));
        msg->type = TYPE_STRING;
        msg->next = NULL;
    }

    while(p->next != NULL) {
        p = p->next;
    }

    p->next = msg;

}
EXPORT_SYMBOL(msg_put_string);

struct athena_msg* alloc_msg(void)
{
    struct athena_msg *msg = NULL;

    msg = (struct athena_msg *)kzalloc(sizeof(struct athena_msg), GFP_KERNEL);
    return msg;
}
EXPORT_SYMBOL(alloc_msg);

void free_msg(struct athena_msg **athena)
{
    struct athena_msg *p = *athena;
    struct athena_msg *curr = NULL;

    if(p == NULL)
        return;

    while(p != NULL) {
        switch(p->type) {
            case TYPE_STRING:
            case TYPE_INT:
            case TYPE_LONG:
            case TYPE_S64:
                if(p->key)
                    kfree(p->key);
                if(p->value)
                    kfree(p->value);
                break;
            default:
                break;
        }
        curr = p;
        p = p->next;
        kfree(curr);
    }
}
EXPORT_SYMBOL(free_msg);

static void submit_msg_wrapper(struct athena_msg **athena, int version, long long tid, char *tag)
{
    struct athena_msg *p = *athena;
    char *buffer = NULL;
    int len = 0;
    int offset = 0;
    int count = 0;
    int v_int = 0;
    long v_long = 0;
    long long v_ll = 0;
    ssize_t ret = 0;
    struct tranlog *tranlog = g_tranlog;

    if((tag == NULL) || (strlen(tag) < 1)) {
        return;
    }

    if(p == NULL)
        return;

    if(tranlog == NULL)
        return;

    mutex_lock(&tranlog->mutex);
    buffer = (char *)kzalloc(4096, GFP_KERNEL);

    //layout: count|tid|version|{type|key len|value len|key|value} * N
    offset += sizeof(count);

    //tid
    memcpy(buffer+offset, &tid, sizeof(tid));
    offset += sizeof(tid);

    //version
    memcpy(buffer+offset, &version, sizeof(version));
    offset += sizeof(version);

    while(p != NULL) {
        if((p->key == NULL) || (p->value == NULL)) {
            p = p->next;
            continue;
        }

        //type
        memcpy(buffer+offset, &p->type, sizeof(p->type));
        offset += sizeof(p->type);

        //key len
        len = strlen(p->key) + 1;
        memcpy(buffer+offset, &len, sizeof(len));
        offset += sizeof(len);

        //value len;
        switch(p->type) {
            case TYPE_STRING:
                len = strlen(p->value) + 1;
                memcpy(buffer+offset, &len, sizeof(int));
                offset += sizeof(int);
                count++;
                break;
            case TYPE_INT:
                len = sizeof(int);
                memcpy(buffer+offset, &len, sizeof(int));
                offset += sizeof(int);
                count++;
                break;
            case TYPE_LONG:
                len = sizeof(long);
                memcpy(buffer+offset, &len, sizeof(int));
                offset += sizeof(int);
                count++;
                break;
            case TYPE_S64:
                len = sizeof(long long);
                memcpy(buffer+offset, &len, sizeof(int));
                offset += sizeof(int);
                count++;
                break;
            case TYPE_BOOL:
                count++;
                break;
            default:
                break;
        }

        //key
        memcpy(buffer+offset, p->key, strlen(p->key));
        offset += (strlen(p->key) + 1); // fill '\0'

        //value
        switch(p->type) {
            case TYPE_STRING:
                memcpy(buffer+offset, p->value, strlen(p->value));
                offset += (strlen(p->value) + 1); // fill '\0'
                //ALOGD("tranlog len:%d, %s: %s", offset, p->key, (char *)p->value);
                break;
            case TYPE_INT:
                //v_int = atoi(p->value);
                ret = kstrtos32(p->value, 10, &v_int);
                if(ret) {
                    v_int = 0;
                }
                memcpy(buffer+offset, &v_int, sizeof(int));
                offset += sizeof(int);
                break;
            case TYPE_LONG:
                //v_long = atol(p->value);
                v_long = simple_strtol(p->value, NULL, 10);
                memcpy(buffer+offset, &v_long, sizeof(long));
                offset += sizeof(long);
                break;
            case TYPE_S64:
                ret = kstrtos64(p->value, 10, &v_ll);
                if(ret) {
                    v_ll = 0;
                }
                memcpy(buffer+offset, &v_ll, sizeof(long long));
                offset += sizeof(long long);
                break;
            case TYPE_BOOL:
                break;
            default:
                break;
        }

        p = p->next;
    }

    memcpy(buffer, &count, sizeof(int));
    __tranlog_local_locked(tag, TRANLOG_TYPE_LOG, TRANLOG_TO_SERVER, buffer, offset);
    kfree(buffer);
    mutex_unlock(&tranlog->mutex);
}

void submit_msg(struct athena_msg **athena, int version, long long tid, char *tag)
{
    submit_msg_wrapper(athena, version, tid, tag);
}
EXPORT_SYMBOL(submit_msg);



static struct tranlog *get_tranlog_from_minor(int minor)
{
    struct tranlog *tranlog;

    list_for_each_entry(tranlog, &all_devices, list) {
        if (tranlog->miscdev.minor == minor) {
//          logd("name:%s", tranlog->miscdev.name);
            return tranlog;
        }
    }

    return NULL;
}


static int tranlog_open(struct inode *inode, struct file *file)
{
    struct tranlog *tranlog = NULL;

    tranlog = get_tranlog_from_minor(MINOR(inode->i_rdev));
    if (!tranlog)
        return -ENODEV;

    file->private_data = tranlog;

    return 0;
}

static int tranlog_release(struct inode *inode, struct file *file)
{
    struct tranlog *tranlog = NULL;

    tranlog = get_tranlog_from_minor(MINOR(inode->i_rdev));
    if (!tranlog)
        return -ENODEV;

    return 0;
}

static bool is_data_ready(struct tranlog *tranlog)
{
    if(!tran_kfifo_is_empty(&tranlog->buffer))
        return true;
    else
        return false;
}

static unsigned int tranlog_poll(struct file *file, poll_table *wait)
{
    struct tranlog *tranlog = NULL;
    unsigned int ret = 0;

    tranlog = file->private_data;
    if (!tranlog)
        ret = POLLERR;

    poll_wait(file, &tranlog->wq, wait);

    if(is_data_ready(tranlog)) {
        //ret |= POLLIN | POLLRDNORM;
        ret |= POLLIN;
    }

    //logd("%s %s fifo_len:%d, ret:0x%0x -\n", __func__, tranlog->miscdev.name, tran_kfifo_len(&tranlog->buffer), ret);

    return ret;
}

static ssize_t tranlog_read(struct file *file, char __user *buffer, size_t count,
              loff_t *ppos)
{
    struct tranlog *tranlog = NULL;
    int out = 0;
    char *buf = NULL;
    size_t ret = 0;

    tranlog = file->private_data;
    if (!tranlog) {
        return -ENODEV;
    }

    buf = kzalloc(tranlog->max_pkg_size, GFP_KERNEL);
    if(!buf) {
        return -ENOMEM;
    }

retry:
    if(is_data_ready(tranlog)) {
        memset(buf, 0, sizeof(tranlog->max_pkg_size));
        mutex_lock(&tranlog->mutex);
        out = tran_kfifo_out(&tranlog->buffer, buf, tranlog->max_pkg_size);
        if(count < out) {
            logd("reader buffer is too small, kfifo:%d, count:%zu\n", out, count);
            mutex_unlock(&tranlog->mutex);
            ret = -EFAULT;
            goto out;
        }
        ret = copy_to_user(buffer, buf, out);
        mutex_unlock(&tranlog->mutex);
        goto out;
    }

    ret = wait_event_interruptible(tranlog->wq, is_data_ready(tranlog));
    if(ret == 0)
        goto retry;

    memset(buf, 0, tranlog->max_pkg_size);
    mutex_lock(&tranlog->mutex);
    out = tran_kfifo_out(&tranlog->buffer, buf, tranlog->max_pkg_size);
    if(count < out) {
        logd("reader buffer is too small, kfifo:%d, count:%zu\n", out, count);
        mutex_unlock(&tranlog->mutex);
        ret = -EFAULT;
        goto out;
    }

    ret = copy_to_user(buffer, buf, out);
    mutex_unlock(&tranlog->mutex);

out:
    if(buf) {
        kfree(buf);
    }

    if(ret) {
        return ret;
    } else {
        return out;
    }
}

static ssize_t tranlog_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
    struct tranlog *tranlog = iocb->ki_filp->private_data;
    size_t len = iov_iter_count(from);
    char *buf = NULL;

    if (!tranlog)
        return -ENODEV;

    if(tranlog->max_pkg_size < len)
        tranlog->max_pkg_size = len+1;

    //TODO malloc once?
    buf = kzalloc(len, GFP_KERNEL);
    if(buf == NULL)
        return -ENOMEM;

    if(copy_from_iter(buf, len, from) != len) {
        kfree(buf);
        return -EFAULT;
    }

    mutex_lock(&tranlog->mutex);
    tran_kfifo_in(&tranlog->buffer, buf, len);
    mutex_unlock(&tranlog->mutex);
    //logd("%s, len=%zu, fifo_len=%d\n", __func__, len, tran_kfifo_len(&tranlog->buffer));

    kfree(buf);
    wake_up_interruptible(&tranlog->wq);

    return len;
}

static const struct file_operations tranlog_fops = {
    .owner =        THIS_MODULE,
    .open =         tranlog_open,
    .release =      tranlog_release,
    .poll =         tranlog_poll,
    .read =         tranlog_read,
    .write_iter =   tranlog_write_iter
};

static struct tranlog* create_tranlog_device(char *name, size_t size, int minor_number)
{
    struct tranlog *tranlog = NULL;
    int ret = 0;
    static int count = 0;

    tranlog = kzalloc(sizeof(struct tranlog), GFP_KERNEL);
    if(!tranlog) {
        loge("%s:%s failed\n", __func__, name);
        ret = -ENOMEM;
        goto error;
    }

    ret = tran_kfifo_alloc(&tranlog->buffer, size, GFP_KERNEL);
    if (ret) {
        loge("error kfifo_alloc:%s\n", name);
        goto error;
    }


    tranlog->index = count++;
    tranlog->max_pkg_size = 256;

    tranlog->miscdev.fops = &tranlog_fops;
    tranlog->miscdev.minor = minor_number;
    tranlog->miscdev.name = kstrdup(name, GFP_KERNEL);;
    if(tranlog->miscdev.name == NULL) {
        ret = -ENOMEM;
        goto error_kfifo;
    }

    ret = misc_register(&tranlog->miscdev);
    if(ret != 0) {
        loge("%s:register %s misc device failed", __func__, name);
        goto error_kfifo;
    }

    init_waitqueue_head(&tranlog->wq);
    mutex_init(&tranlog->mutex);
    INIT_LIST_HEAD(&tranlog->list);
    list_add_tail(&tranlog->list, &all_devices);

    return tranlog;

error_kfifo:
    tran_kfifo_free(&tranlog->buffer);

error:
    if((tranlog != NULL) && tranlog->miscdev.name)
        kfree(tranlog->miscdev.name);

    if(tranlog)
        kfree(tranlog);

    return NULL;
}

static ssize_t nvlist_store(struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t count)
{
    char *key = NULL;
    char *value = NULL;
    int ret = 0;
    struct watchlist *wl;
    struct tranlog *tranlog = g_tranlog;
    int len = -1;

    if(tranlog == NULL)
        return -1;

    len = strlen(buf);
    if(len < 3)
        return -1;

    key = (char *)kzalloc(len + 1, GFP_KERNEL);
    if(!key) {
        return -1;
    }

    value = (char *)kzalloc(len + 1, GFP_KERNEL);
    if(!value) {
        goto out;
    }

    ret = sscanf(buf, "%s %s", key, value);
    if(ret != 2) {
        goto out;
    }

    //debug
    //set_nv_watchlist(key, TYPE_INT);

    wl = find_watchlist(key);
    mutex_lock(&tranlog->mutex);
    if(wl) {
        if(wl->type == TYPE_INT) {
            int v_int = 0;
            ret = kstrtos32(value, 10, &v_int);
            if(ret) {
                v_int = 0;
            }
            wl->s32 = v_int;
        } else if(wl->type == TYPE_LONG) {
            long long v_long = 0;
            v_long = simple_strtol(value, NULL, 10);
            wl->s64 = v_long;
        }

        //dump_watchlist();
    }
    mutex_unlock(&tranlog->mutex);

out:
    if(key) {
        kfree(key);
    }
    if(value) {
        kfree(value);
    }

    return count;
}

static struct kobj_attribute nvlist = __ATTR_WO(nvlist);

static struct attribute *attrs[] = {
	&nvlist.attr,
	NULL,
};
static struct attribute_group attr_group = {
	.attrs = attrs,
};

static int create_sys_watchlist(void)
{
    struct kobject *kobj = NULL;
    int ret = -1;

	kobj = kobject_create_and_add("tranv", kernel_kobj);
	if (!kobj) {
        logd("create nv sysfs kobj failed");
		return -1;
	}

	ret = sysfs_create_group(kobj, &attr_group);
	if(ret != 0) {
		kobject_put(kobj);
        logd("create nv sysfs group failed");
        return -1;
	}

    return 0;
}

static int create_sys_node(void)
{
    create_sys_watchlist();
    return 0;
}

//WTF:register static minor first
#define MINOR_NUMBER_USER       254
#define MINOR_NUMBER_KERNEL     253
static int tranlog_init(void)
{
    if(create_tranlog_device("tranlog_user", 256*1024, MINOR_NUMBER_USER) == NULL) {
        create_tranlog_device("tranlog_user", 256*1024, MISC_DYNAMIC_MINOR);    
    }

    g_tranlog = create_tranlog_device("tranlog_kernel", 32*1024, MINOR_NUMBER_KERNEL);
    if(g_tranlog == NULL) {
        g_tranlog = create_tranlog_device("tranlog_kernel", 32*1024, MISC_DYNAMIC_MINOR);
    }

    create_sys_node();

    athena_ctl_init();
    //SPD:add by chongyang.zhang for  trancare config 2020.03.30 start
    cloudengine_config_init();
	cloudengine_ctl_init();
    //SPD:add by chongyang.zhang for  trancare config 2020.03.30 end
    return 0;
}

static void tranlog_exit(void)
{
    struct tranlog *_current = NULL;
    struct tranlog *_next = NULL;

    g_tranlog = NULL;

    list_for_each_entry_safe(_current, _next, &all_devices, list) {
        logd("index=%d", _current->index);
        kfree(_current->miscdev.name);
        misc_deregister(&_current->miscdev);
        tran_kfifo_free(&_current->buffer);
        list_del(&_current->list);
        kfree(_current);
    }

    athena_ctl_exit();
    //SPD:add by chongyang.zhang for trancare config 2020.03.30 start
    cloudengine_config_exit();
	cloudengine_ctl_exit();
    //SPD:add by chongyang.zhang for  trancare config 2020.03.30 end
}

module_init(tranlog_init);
module_exit(tranlog_exit);

MODULE_LICENSE("GPL");
