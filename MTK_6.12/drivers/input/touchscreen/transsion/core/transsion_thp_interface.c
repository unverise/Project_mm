#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/byteorder/generic.h>
#include <linux/ktime.h>

#include "transsion_core.h"
#include "transsion_gesture.h"
#include "transsion_thp_interface.h"


/*
 * ------------------------------
 * Static Variables Declaration
 * ------------------------------
 */

//static volatile bool data_available = false;
static uint32_t cache_frame_cnt_limit = 7;
static struct tran_thp_data *g_tran_thp_data = NULL;
static int g_pid = 0;

static struct tran_thp_netlink_msg nl_msg[] = {
    {TRAN_NL_SCREEN_OFF, "screen off"},
    {TRAN_NL_SCREEN_ON, "screen on"},
    {TRAN_NL_TOUCH_CHARGOUT, "charge out"},
    {TRAN_NL_TOUCH_CHARGIN, "charge in"},
    {TRAN_NL_WIRELESS_CHARGE, "wireless charge"},
    {TRAN_NL_SCREEN_LOCK, "screen lock"},
    {TRAN_NL_SCREEN_UNLOCK, "screen unlock"},
    {TRAN_NL_KEYPAD_ON, "keypad on"},
    {TRAN_NL_KEYPAD_OFF, "keypad off"},
    {TRAN_NL_ROTATION_DOWN, "rotation down"},
    {TRAN_NL_ROTATION_LEFT, "rotation left"},
    {TRAN_NL_ROTATION_RIGHT, "rotation right"},
    {TRAN_NL_SCENE_NORMAL, "scene normal"},
    {TRAN_NL_SCENE_VIDEO, "scene video"},
    {TRAN_NL_SCENE_GAME, "scene game"},
    {TRAN_NL_SCENE_CAMERA, "scene camera"},
    {TRAN_NL_SCENE_DIAL_PAD, "scene dial pad"},
    {TRAN_NL_MAX_CMD, "unknow cmd"},
};
/*
 * ------------------------------
 * Global functions declaration
 * ------------------------------
 */

extern void tran_ts_trace_queue_event_cnt(uint8_t hh, uint8_t tt,
    uint64_t drive_frame_cnt, uint32_t loss_frame_cnt, uint32_t cache_frame_cnt);

/*
 * ------------------------------
 * Static functions declaration
 * ------------------------------
 */
static int tran_thp_netlink_send_msg(const int msg);


/*
 * ------------------------------
 * Macros
 * ------------------------------
 */
#define CHECK_DATA_PTR(ptr) \
    do { \
        if (unlikely(!(ptr))) { \
            TRAN_ERROR("Invalid pointer: %s", #ptr); \
            return -EINVAL; \
        } \
    } while(0)

#define DISABLE_IRQ                    (0)
#define ENABLE_IRQ                     (1)
#define TRAN_NL_HAL_LOG_OFFSET         (TRAN_NL_HAL_LOG_LEVEL_ERROR - TRAN_NL_DEBUG_BASE - 1)
#define TRAN_THP_DRV_VER               "THP_DRV-0116_01"


/*
 * ------------------------------
 * Sysfs Attributes
 * ------------------------------
 */

static ssize_t lost_frames_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct tran_thp_data *data = g_tran_thp_data;
    uint32_t count = 0;
    uint32_t frame_count = 0;

    CHECK_DATA_PTR(data);
    if (data && data->thp_q) {
        mutex_lock(&data->mutexq);
        count = data->thp_q->loss_frame_cnt;
        frame_count = data->thp_q->drive_frame_cnt;
        mutex_unlock(&data->mutexq);
    }
    return sprintf(buf, "lost:%u, total:%u, poll_cnt:%d\n", \
        count, frame_count, (int)atomic_read(&data->poll_cnt));
}

static ssize_t log_level_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct tran_thp_data *data = g_tran_thp_data;
    CHECK_DATA_PTR(data);
    return sprintf(buf, "%d\n", data->log_level);
}

static ssize_t log_level_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    struct tran_thp_data *data = g_tran_thp_data;
    int val;

    CHECK_DATA_PTR(data);
    if (kstrtoint(buf, 10, &val))
        return -EINVAL;
    data->log_level = val;
    return count;
}

static ssize_t thp_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct tran_thp_data *data = g_tran_thp_data;
    CHECK_DATA_PTR(data);
    return sprintf(buf, "%d\n", data->thp_mode_debug);
}

static ssize_t thp_mode_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    struct tran_thp_data *data = g_tran_thp_data;
    int val;

    CHECK_DATA_PTR(data);
    if (kstrtoint(buf, 10, &val))
        return -EINVAL;
    //data->thp_mode_debug = (bool)val;
    if (val == 2) {
        tran_thp_netlink_send_msg(TRAN_NL_DEBUG_THP_MODE_WDT_TEST);
    } else {
        data->thp_mode_debug = (bool)val;
        tran_thp_netlink_send_msg(val == 1 ? TRAN_NL_DEBUG_THP_MODE_ON : TRAN_NL_DEBUG_THP_MODE_OFF);
    }
    return count;
}

static ssize_t thp_crc_valid_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct tran_thp_data *data = g_tran_thp_data;
    CHECK_DATA_PTR(data);
    return sprintf(buf, "%d\n", data->thp_crc_valid);
}

static ssize_t thp_crc_valid_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    struct tran_thp_data *data = g_tran_thp_data;
    int val;

    CHECK_DATA_PTR(data);
    if (kstrtoint(buf, 10, &val))
        return -EINVAL;
    data->thp_crc_valid = val;
    return count;
}

static ssize_t thp_ver_info_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct tran_thp_data *data = g_tran_thp_data;
    CHECK_DATA_PTR(data);
    return sprintf(buf, "ini:%s, hal:%s, algo:%s, edge:%s, drv:%s\n", \
        data->ver_info.ini_version, \
        data->ver_info.hal_version, \
        data->ver_info.algo_version, \
        data->ver_info.edge_version, \
        TRAN_THP_DRV_VER);
}

static ssize_t hal_log_level_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct tran_thp_data *data = g_tran_thp_data;
    CHECK_DATA_PTR(data);
    return sprintf(buf, "%d\n", data->hal_log_level);
}
static ssize_t hal_log_level_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    struct tran_thp_data *data = g_tran_thp_data;
    int val;

    CHECK_DATA_PTR(data);
    if (kstrtoint(buf, 10, &val))
        return -EINVAL;
    data->hal_log_level = (uint8_t)val;
    /**val:
        LOG_VERBOSE_LEVEL      4
        LOG_DEBUG_LEVEL        3
        LOG_INFO_LEVEL         2
        LOG_ERROR_LEVEL        1
    *******************************/
    if (val < 5) {
        tran_thp_netlink_send_msg(val + TRAN_NL_DEBUG_BASE + TRAN_NL_HAL_LOG_OFFSET);
    } else {
        TRAN_ERROR("Invalid hal_log_level: %d", val);
    }
    return count;
}
static ssize_t cache_frame_cnt_limit_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n", cache_frame_cnt_limit);
}

static ssize_t cache_frame_cnt_limit_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int val;

    if (kstrtoint(buf, 10, &val))
        return -EINVAL;
    cache_frame_cnt_limit = (uint32_t)val;
    if (cache_frame_cnt_limit > TRAN_MAX_QUEUE_COUNT || cache_frame_cnt_limit < 3) {
        cache_frame_cnt_limit = 7;
    }
    return count;
}



static struct kobj_attribute lost_frames_attr = __ATTR_RO(lost_frames);
static struct kobj_attribute log_level_attr = __ATTR_RW(log_level);
static struct kobj_attribute thp_mode_attr = __ATTR_RW(thp_mode);
static struct kobj_attribute thp_crc_valid_attr = __ATTR_RW(thp_crc_valid);
static struct kobj_attribute thp_ver_info_attr = __ATTR_RO(thp_ver_info);
static struct kobj_attribute hal_log_level_attr = __ATTR_RW(hal_log_level);
static struct kobj_attribute cache_frame_cnt_limit_attr = __ATTR_RW(cache_frame_cnt_limit);

static struct attribute *thp_debug_attrs[] = {
    &lost_frames_attr.attr,
    &log_level_attr.attr,
    &thp_mode_attr.attr,
    &thp_crc_valid_attr.attr,
    &thp_ver_info_attr.attr,
    &hal_log_level_attr.attr,
    &cache_frame_cnt_limit_attr.attr,
    NULL,
};

static struct attribute_group thp_attr_group = {
    .attrs = thp_debug_attrs,
};

static int tran_thp_netlink_send_msg(const int msg)
{
    struct sk_buff *skb = NULL;
    struct nlmsghdr *nlh = NULL;
    int res = 0;
    struct tran_thp_data *thp_data = g_tran_thp_data;
    const size_t msg_size = sizeof(msg);
    int nl_msg_size = ARRAY_SIZE(nl_msg);

    if (!thp_data || !thp_data->nl_sk || g_pid <= 0) {
        TRAN_ERROR("Invalid input: nl_sk=%p, g_pid=%d",
                thp_data ? thp_data->nl_sk : NULL, g_pid);
        return -EINVAL;
    }
    if (thp_data->thp_dev_open_cnt < 1) {
        TRAN_ERROR("THP mode is disabled");
        return 0;
    }
    skb = nlmsg_new(msg_size, GFP_KERNEL);
    if (!skb) {
        TRAN_ERROR("Failed to allocate skb (size=%zu)", msg_size);
        return -ENOMEM;
    }

    nlh = nlmsg_put(skb, 0, 0, TRAN_THP_NETLINK_MSG_TYPE, msg_size, 0);
    if (!nlh) {
        TRAN_ERROR("Failed to put nlmsg (size=%zu)", msg_size);
        res = -EMSGSIZE;
        goto free_skb;
    }

    NETLINK_CB(skb).portid = 0;
    NETLINK_CB(skb).dst_group = 0;

    memcpy(NLMSG_DATA(nlh), &msg, msg_size);

    res = netlink_unicast(thp_data->nl_sk, skb, g_pid, MSG_DONTWAIT);
    if (res < 0) {
        TRAN_ERROR("Failed to send msg %d to pid %d: %d", msg, g_pid, res);
        return res;
    }
    if (msg >= 0 && msg < nl_msg_size) {
        TRAN_INFO("Successfully send msg:%s %d to pid %d, len=%d", 
                 nl_msg[msg].msg, nl_msg[msg].cmd, g_pid, res);
    } else {
        TRAN_INFO("Successfully send msg id:%d to pid %d, len=%d", msg, g_pid, res);
    }
    //TRAN_INFO("Successfully send msg:%s %d to pid %d, len=%d", nl_msg[msg].msg, nl_msg[msg].cmd, g_pid, res);
    return 0;

free_skb:
    kfree_skb(skb);
    return res;
}

static void tran_thp_netlink_recv(struct sk_buff *__skb)
{
    struct sk_buff *skb = NULL;
    struct nlmsghdr *nlh = NULL;
    char str[128] = {0};
    //int g_pid = 0;

    skb = skb_get(__skb);
    if (!skb) {
        TRAN_ERROR("skb is NULL");
        return;
    }

    if (skb->len < NLMSG_SPACE(0)) {
        TRAN_ERROR("Invalid skb length: %d", skb->len);
        goto cleanup;
    }

    nlh = nlmsg_hdr(skb);
    if (!nlh) {
        TRAN_ERROR("nlmsg_hdr is NULL");
        goto cleanup;
    }

    if (nlh->nlmsg_len < NLMSG_LENGTH(0) || nlh->nlmsg_len > skb->len) {
        TRAN_ERROR("Invalid nlmsg_len: %d", nlh->nlmsg_len);
        goto cleanup;
    }

    if (strscpy(str, NLMSG_DATA(nlh), sizeof(str)) < 0) {
        TRAN_ERROR("Failed to copy message data");
        goto cleanup;
    }
    if (strncmp(str, "TRAN_THP", 8) == 0) {
        g_pid = nlh->nlmsg_pid;
        TRAN_INFO("THP msg: pid = %d, msg = %s, type = %d", g_pid, str, nlh->nlmsg_type);
    } else {
        TRAN_INFO("Other msg: pid = %d, msg = %s, type = %d", nlh->nlmsg_pid, str, nlh->nlmsg_type);
    }

cleanup:
    if (skb)
        kfree_skb(skb);
}
static int tran_thp_netlink_init(struct tran_thp_data *thp_data)
{
    struct netlink_kernel_cfg cfg;
    memset(&cfg, 0, sizeof(struct netlink_kernel_cfg));
    cfg.input = tran_thp_netlink_recv;

    thp_data->nl_sk = netlink_kernel_create(&init_net, TRAN_THP_NETLINK_ROUTE, &cfg);
    if (thp_data->nl_sk == NULL) {
        return -ENOMEM;
    }

    TRAN_INFO("netlink create success,msg type:%u", (unsigned int)TRAN_THP_NETLINK_MSG_TYPE);
    return 0;
}
static void tran_thp_netlink_release(struct tran_thp_data *thp_data)
{
    if (thp_data->nl_sk != NULL) {
        netlink_kernel_release(thp_data->nl_sk);
        thp_data->nl_sk = NULL;
        TRAN_INFO("Release netlink socket done.");
    }
}
static void tran_set_irq(uint8_t en)
{
    struct tran_ts_controller *ctl = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];

    if (ctl && ctl->ext_ops && ctl->ext_ops->irq_enable_en) {
        ctl->ext_ops->irq_enable_en(en);
    }
}

static int tran_thp_q_init(struct tran_thp_data *thp_data)
{
    int ret = 0;
    struct tran_thp_queue *q = thp_data->thp_q;

    mutex_lock(&(thp_data->mutexq));
    if (!q) {
        //q = kzalloc(TRAN_MAX_QUEUE_BUFSIZE, GFP_KERNEL);
        q = (struct tran_thp_queue *)__get_free_pages(TRAN_THP_GFP_FLAGS, TRAN_THP_ALLOC_ORDER);
        if (!q) {
            TRAN_ERROR("allocate memory for thp_q failed");
            mutex_unlock(&(thp_data->mutexq));
            return -ENOMEM;
        } else {
            thp_data->thp_q = q;
            q->head = q->tail = 0;
            q->loss_frame_cnt = 0;
            q->cache_frame_cnt = 0;
            q->buffer_size = TRAN_MAX_QUEUE_BUFSIZE - sizeof(struct tran_thp_queue);
            q->elem_size = TRAN_FRAME_BUF;
            q->max_count = TRAN_MAX_QUEUE_COUNT;
            TRAN_INFO("allocate memory for thp_q size:(%d) successful,tran_thp_queue size:%zu Byte", \
                        q->buffer_size, sizeof(struct tran_thp_queue));
        }
    } else {
        q->head = q->tail = 0;
        memset(q->buffer, 0, q->buffer_size);
        TRAN_INFO("memset 0 for q buffer_size:%d successful", q->buffer_size);
    }
    mutex_unlock(&(thp_data->mutexq));

    return ret;
}
static int tran_thp_dev_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    struct tran_thp_data *thp_data = container_of(filp->private_data, struct tran_thp_data, thp_misc_dev);

    TRAN_FUNC_ENTER();
    if (!thp_data) {
        TRAN_ERROR("thp_data is null");
        return -ENODATA;
    }

    if (thp_data->thp_dev_open_cnt >= 1) {
        TRAN_ERROR("open fail(cnt:%d)", thp_data->thp_dev_open_cnt);
        return -EBUSY;
    }
    tran_set_irq(DISABLE_IRQ);
    ret = tran_thp_q_init(thp_data);
    if (ret) {
        TRAN_ERROR("tran_thp_q_init fail");
        tran_set_irq(ENABLE_IRQ);
        return ret;
    }
    atomic_set(&thp_data->refcount, 1);
    thp_data->thp_dev_open_cnt++;
    thp_data->thp_mode = true;
    tran_set_irq(ENABLE_IRQ);
    TRAN_INFO("open successfully(cnt:%d)", thp_data->thp_dev_open_cnt);
    return 0;
}

static int tran_thp_dev_close(struct inode *inode, struct file *filp)
{
    struct tran_thp_data *thp_data = container_of(filp->private_data, struct tran_thp_data, thp_misc_dev);

    TRAN_FUNC_ENTER();
    if (!thp_data) {
        TRAN_ERROR("thp_data is null");
        return -ENODATA;
    }
    tran_set_irq(DISABLE_IRQ);
    mutex_lock(&(thp_data->mutexq));
    if (thp_data->thp_dev_open_cnt >= 1) {
        if (thp_data->thp_q && atomic_dec_and_test(&thp_data->refcount)) {
            //kfree(thp_data->thp_q);
            //free_pages((unsigned long)thp_data->thp_q, TRAN_THP_ALLOC_ORDER);
            //thp_data->thp_q = NULL;
            //TRAN_INFO("free thp_q");
        }
        TRAN_INFO("refcount:%d,thp_dev_open_cnt:%d", atomic_read(&thp_data->refcount), thp_data->thp_dev_open_cnt);
        thp_data->thp_dev_open_cnt--;
        thp_data->thp_mode = false;
    } else {
        TRAN_ERROR("Not open dev or open fail(cnt:%d)", thp_data->thp_dev_open_cnt);
    }
    mutex_unlock(&(thp_data->mutexq));
    tran_set_irq(ENABLE_IRQ);
    TRAN_FUNC_EXIT();

    return 0;
}

static int tran_thp_dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long size;
    struct tran_thp_data *thp_data;
    unsigned long pfn;

    TRAN_FUNC_ENTER();

    if (!filp || !vma) {
        TRAN_ERROR("Invalid input: filp or vma is null");
        return -EINVAL;
    }

    thp_data = container_of(filp->private_data, struct tran_thp_data, thp_misc_dev);
    if (!thp_data || !thp_data->thp_q) {
        TRAN_ERROR("thp_data or thp_q is null");
        return -ENXIO;
    }

    size = vma->vm_end - vma->vm_start;
    if (size > TRAN_MAX_QUEUE_BUFSIZE) {
        TRAN_ERROR("Mapping size (%lu) exceeds buffer size (%lu)", size, TRAN_MAX_QUEUE_BUFSIZE);
        return -EINVAL;
    }

    // ensure thp_q is page-aligned (for remap_pfn_range)
    if (!IS_ALIGNED((unsigned long)thp_data->thp_q, PAGE_SIZE)) {
        TRAN_ERROR("Buffer address is not page-aligned: addr=0x%lx", (unsigned long)thp_data->thp_q);
        return -EINVAL;
    }

    pfn = virt_to_phys((void *)(thp_data->thp_q)) >> PAGE_SHIFT;

    if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot)) {
        TRAN_ERROR("Failed to remap memory: pfn=0x%lx, size=%lu, vma_start=0x%lx", \
                   pfn, size, vma->vm_start);
        return -EAGAIN;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,18,0)
    vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
#else
    vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
#endif

    TRAN_INFO("Memory mapped successfully: pfn=0x%lx, size=%lu", pfn, size);

    return 0;
}

static unsigned int tran_thp_dev_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    int pending;
    struct tran_thp_data *thp_data = container_of(filp->private_data, struct tran_thp_data, thp_misc_dev);

    if (!thp_data || !thp_data->thp_q) {
        TRAN_ERROR("thp_data/thp_q is null");
        return mask;
    }

    //trace_drv_poll_wait_start(0);
    poll_wait(filp, &thp_data->frame_waitq, wait);
    pending = atomic_xchg(&thp_data->data_valid, 0);
    //trace_drv_poll_wait_end(pending);

    if (pending > 0) {
        mask |= POLLIN | POLLRDNORM;
        atomic_add(pending, &thp_data->poll_cnt);
    }

    return mask;
}

static const struct file_operations thp_interface_fops = {
    .owner = THIS_MODULE,
    .open = tran_thp_dev_open,
    .release = tran_thp_dev_close,
    .poll = tran_thp_dev_poll,
    .mmap = tran_thp_dev_mmap,
};

static void print_frame_data(const char *frame_data, size_t frame_length, uint8_t num) {
    char log_buffer[1024];
    int buffer_index = 0;

    for (size_t i = 0; i < frame_length; i++) {

        buffer_index += snprintf(log_buffer + buffer_index, sizeof(log_buffer) - buffer_index, "%02X,", frame_data[i]);

        if ((i + 1) % num == 0 || i == frame_length - 1) {
            log_buffer[buffer_index] = '\0';
            TRAN_INFO("Frame data: %s", log_buffer);
            buffer_index = 0;
        }
    }
}

static void validate_crc(uint8_t *val, struct tran_thp_data *thp_data, bool debug_flag)
{
    s32 crc_val = 0;
    s32 crc_data = 0;
    u8 crc_offset = (u8)thp_data->thp_custom_param.thp_crc_offset;
    u16 crc_len = (u16)thp_data->thp_custom_param.thp_crc_len;

    if (thp_data->big_endian) {
        crc_data = val[3] << 24 | val[4] << 16 | val[5] << 8 | val[6];
    } else {
        crc_data = val[6] << 24 | val[5] << 16 | val[4] << 8 | val[3];
    }
    if (!thp_data->q_ops->crc32_check) {
        TRAN_ERROR("crc check failed, crc32_check not implemented");
        return;
    }
    crc_val = thp_data->q_ops->crc32_check((val + crc_offset), crc_len);
    //crc_val = tran_thp_crc32_check((uint32_t *)(val + crc_offset), crc_len/4);
    if (crc_val != crc_data) {
        TRAN_ERROR("crc check failed,crc_cal:0x%x crc_data:0x%x(0x%02x 0x%02x 0x%02x 0x%02x)", \
                crc_val, crc_data, val[3], val[4], val[5], val[6]);
        TRAN_ERROR("crc check failed,crc_len:(0x%02x 0x%02x)", val[9], val[10]);
    } else {
        if (debug_flag)
            TRAN_INFO("[crc check passed]head:%d tail:%d crc_cal:0x%x  crc data:0x%02x 0x%02x 0x%02x 0x%02x crc_len:%d", \
                thp_data->thp_q->head, thp_data->thp_q->tail, crc_val, val[3], val[4], val[5], val[6], crc_len);
    }
}

#define TOUCH_EVENT_NUM 12
#define TOUCH_EVENT_SIZE 21
static void parse_touch_events(const u8 *val, struct tran_thp_data *thp_data, bool debug_flag)
{
    int i, j;
    struct tran_thp_touch_event te[TOUCH_EVENT_NUM];
    val += thp_data->thp_custom_param.thp_touch_event_offset;
    for (i = 0; i < TOUCH_EVENT_NUM; i++) {
        const u8 *p = val + i * TOUCH_EVENT_SIZE;

        te[i].valid = p[0];
        te[i].x = be32_to_cpu(*(const u32 *)(p+1));
        te[i].y = be32_to_cpu(*(const u32 *)(p+5));
        te[i].p = be16_to_cpu(*(const u16 *)(p+9));
        te[i].flag = p[11];
        te[i].id = p[12];
        te[i].major = be16_to_cpu(*(const u16 *)(p+13));
        te[i].minor = be16_to_cpu(*(const u16 *)(p+15));
        te[i].value = be16_to_cpu(*(const u16 *)(p+17));
        te[i].rx = p[19];
        te[i].tx = p[20];

        if (debug_flag) {
            for (j = 0; j < TOUCH_EVENT_SIZE; j++) {
                TRAN_INFO("Touch Event Data: buf[%d][%d] = %d ",i, j, val[j]);
            }
            TRAN_INFO("Touch Event: valid=%u, x=%u, y=%u, p=%u, flag=%u, id=%u, major=%u, minor=%u, value=%u, rx=%u, tx=%u\n",
                    te[i].valid, te[i].x, te[i].y, te[i].p, te[i].flag, te[i].id, te[i].major, te[i].minor, te[i].value, te[i].rx, te[i].tx);
        }

        if (te[i].valid == 1 && te[i].flag == 1) {
            TRAN_INFO("Touch Down: x=%u, y=%u, id=%u, tx=%u, rx=%u\n", te[i].x, te[i].y, te[i].id, te[i].tx, te[i].rx);
        }

    }
}
static void tran_ktime_delta(ktime_t start)
{
    ktime_t end = ktime_get();
    s64 duration = ktime_to_ns(ktime_sub(end, start));
    TRAN_INFO("dequeue time: %llu ms %llu us", duration/1000000, (duration%1000000)/1000);
}

static void tran_thp_queue_debug(struct tran_thp_data *thp_data, u8 *val, u32 data_size, struct tran_thp_frame *tail_elem, ktime_t start)
{
    struct tran_thp_queue *q = thp_data->thp_q;

    if (thp_data->log_level == 2) {
        parse_touch_events(val, thp_data, false);
    }
    if (thp_data->log_level == 3) {
        parse_touch_events(val, thp_data, true);
    }
    if (thp_data->log_level == 4) {
        TRAN_INFO("timestamp: %lld ns", tail_elem->tv);
        validate_crc(val, thp_data, true);
    }
    if (thp_data->log_level == 5) {
        print_frame_data((const char *)tail_elem->value, data_size, thp_data->thp_custom_param.thp_rx_num);
    }
    if (thp_data->log_level == 6) {
        tran_ktime_delta(start);
    }
    if (thp_data->log_level == 7) {
        TRAN_INFO("[debug info]head:%d tail:%d driver_cnt:%llu",
                q->head, q->tail, q->drive_frame_cnt);
    }
}
static int tran_thp_enqueue(u8 *val, u32 data_size, s64 timestamp)
{
    struct tran_thp_frame *tail_elem = NULL;
    int next_tail;
    size_t offset;

    struct tran_thp_data *thp_data = g_tran_thp_data;
    if (!thp_data) {
        TRAN_ERROR("thp_data is NULL");
        return -EINVAL;
    }

    if (!atomic_inc_not_zero(&thp_data->refcount)) {
        TRAN_ERROR("thp_data is being freed");
        return -ENODEV;
    }

    struct tran_thp_queue *q = thp_data->thp_q;
    if (!val || !q || q->buffer_size == 0 || q->elem_size == 0 || q->max_count == 0) {
        atomic_dec(&thp_data->refcount);
        TRAN_ERROR("Invalid input parameters");
        return -EINVAL;
    }

    ktime_t start = ktime_get();

    mutex_lock(&thp_data->mutexq);
    offset = q->tail * q->elem_size;
    if (offset + q->elem_size > q->buffer_size) {
        TRAN_ERROR("Buffer overflow! tail=%d elem_size=%d buffer_size=%d",
                  q->tail, q->elem_size, q->buffer_size);
        goto error_unlock;
    }

    tail_elem = (struct tran_thp_frame *)&q->buffer[offset];

    //memset(tail_elem, 0, q->elem_size);
    tail_elem->tv = timestamp;
    tail_elem->driver_cnt = q->drive_frame_cnt;
    memcpy(tail_elem->value, val, data_size);

    next_tail = (q->tail + 1) % q->max_count;
    if (next_tail == q->head) {
        q->loss_frame_cnt++;
        TRAN_INFO("[Warning:Queue full]head:%d tail:%d driver_cnt:%llu loss_cnt:%u cache_cnt:%u",
            q->head, q->tail, q->drive_frame_cnt, q->loss_frame_cnt, q->cache_frame_cnt);
    } else {
        q->tail = next_tail;
    }
    q->drive_frame_cnt++;
    q->cache_frame_cnt = (q->tail >= q->head) ? 
        (q->tail - q->head) : 
        (q->tail + q->max_count - q->head);
    if (q->cache_frame_cnt >= cache_frame_cnt_limit) {
        TRAN_INFO("[Warning:almost full]head:%d tail:%d driver_cnt:%llu loss_cnt:%u cache_cnt:%u",
            q->head, q->tail, q->drive_frame_cnt, q->loss_frame_cnt, q->cache_frame_cnt);
        tran_thp_netlink_send_msg(TRAN_NL_PERFORMANCE_POLL_SHAREMEM);
    }
    mutex_unlock(&thp_data->mutexq);

    atomic_inc(&thp_data->data_valid);
    wake_up_interruptible(&thp_data->frame_waitq);
    tran_ts_trace_queue_event_cnt(q->head, q->tail, q->drive_frame_cnt,
            q->loss_frame_cnt, q->cache_frame_cnt);

    if (thp_data->thp_crc_valid) {
        validate_crc(val, thp_data, false);
    }
    if (thp_data->log_level > 1) {
        tran_thp_queue_debug(thp_data, val, data_size, tail_elem, start);
    }

    atomic_dec(&thp_data->refcount);

    return 0;

error_unlock:
    mutex_unlock(&thp_data->mutexq);
    atomic_dec(&thp_data->refcount);
    return -EFAULT;
}

static struct tran_thp_ops thp_ops = {
    .thp_enqueue = tran_thp_enqueue,
    .send_msg = tran_thp_netlink_send_msg,
    .crc32_check = NULL,
};

struct tran_thp_data * tran_thp_device_init(void)
{
    int ret = 0;
    struct tran_thp_data *tran_thp_data = NULL;
    //TRAN_FUNC_ENTER();
    TRAN_INFO("Init thp driver version: %s", TRAN_THP_DRV_VER);

    tran_thp_data = kzalloc(sizeof(struct tran_thp_data), GFP_KERNEL);
    if (!tran_thp_data) {
        TRAN_ERROR("Failed to alloc tran_thp_data");
        return NULL;
    } else {
        g_tran_thp_data = tran_thp_data;
        TRAN_INFO("alloc %zu success", sizeof(struct tran_thp_data));
    }

    tran_thp_data->thp_misc_dev.minor = MISC_DYNAMIC_MINOR;
    tran_thp_data->thp_misc_dev.name = THP_DEVICE_NAME;
    tran_thp_data->thp_misc_dev.fops = &thp_interface_fops;
    ret = misc_register(&tran_thp_data->thp_misc_dev);
    if (ret) {
        TRAN_ERROR("Failed to register dev/%s", tran_thp_data->thp_misc_dev.name);
        goto misc_register_err;
    }
    ret = tran_thp_netlink_init(tran_thp_data);
    if (ret) {
        TRAN_ERROR("Failed to initialize thp netlink:%d", ret);
        goto err_netlink_init_fail;
    }
    tran_thp_data->q_ops = &thp_ops;
    tran_thp_data->log_level = 1;
    tran_thp_data->thp_crc_valid = false;
    tran_thp_data->thp_mode_debug = true;
    tran_thp_data->thp_kobj = kobject_create_and_add("thp_debug", kernel_kobj);
    if (tran_thp_data->thp_kobj) {
        if (sysfs_create_group(tran_thp_data->thp_kobj, &thp_attr_group)) {
            TRAN_ERROR("Failed to create sysfs group");
            goto sysfs_create_err;
        } else {
            TRAN_INFO("create sysfs group success");
        }
    } else {
        TRAN_ERROR("Failed to create kobject");
    }

    init_waitqueue_head(&tran_thp_data->frame_waitq);
    spin_lock_init(&tran_thp_data->data_lock);
    mutex_init(&tran_thp_data->mutexq);
    atomic_set(&tran_thp_data->refcount, 1);
    atomic_set(&tran_thp_data->data_valid, 0);
    atomic_set(&tran_thp_data->poll_cnt, 0);

    TRAN_INFO("Register /dev/%s success...", THP_DEVICE_NAME);
    return tran_thp_data;

sysfs_create_err:
    kobject_put(tran_thp_data->thp_kobj);
    tran_thp_data->thp_kobj = NULL;
    tran_thp_netlink_release(tran_thp_data);
err_netlink_init_fail:
    misc_deregister(&tran_thp_data->thp_misc_dev);
misc_register_err:
    kfree(tran_thp_data);
    tran_thp_data = NULL;
    g_tran_thp_data = NULL;
    TRAN_ERROR("Register /dev/%s fail(%d)", THP_DEVICE_NAME, ret);
    return NULL;
}
EXPORT_SYMBOL(tran_thp_device_init);

void tran_thp_device_exit(struct tran_thp_data *thp_data)
{
    TRAN_FUNC_ENTER();

    if (thp_data) {
        if (thp_data->thp_kobj) {
            sysfs_remove_group(thp_data->thp_kobj, &thp_attr_group);
            kobject_put(thp_data->thp_kobj);
            thp_data->thp_kobj = NULL;
        }
        misc_deregister(&thp_data->thp_misc_dev);
        if (thp_data->thp_q) {
            //kfree(thp_data->thp_q);
            free_pages((unsigned long)thp_data->thp_q, TRAN_THP_ALLOC_ORDER);
            thp_data->thp_q = NULL;
        }
        kfree(thp_data);
        g_tran_thp_data = thp_data = NULL;
        TRAN_INFO("Exit tran_thp_device_exit");
    }
}
EXPORT_SYMBOL(tran_thp_device_exit);
