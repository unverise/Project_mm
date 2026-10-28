/*****************************************************************************
*
* File Name: focaltech_tran_thp.c
*
* Author: transsion TP Driver Team
*
* Created: 2025-07-30
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "focaltech_core.h"
//#include "transsion_thp_interface.h"

/*****************************************************************************
* 1.Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_REG_WATER_MODE       0x92
#define FTS_ENTER_WATER_MODE     0x01
#define FTS_EXIT_WATER_MODE      0x00

extern void tran_ts_trace_touchscreen_report(int x, int y, int touch_id, int flag);
/*****************************************************************************
* 2.Public variable definitions
*****************************************************************************/
//static struct tran_thp_init_info g_tran_thp_init_info;

struct tran_thp_scene g_thp_scene = {
    .is_awake = true,
    .orientation = 0,
    .game_mode = false,
    .pocket_mode = 0,
    .typing_mode = 0,
    .camera_mode = 0,
    .charging_status = 0,
    .video_mode = 0,
    .res_mode = 0,
};

/*****************************************************************************
* 3.Static function prototypes
*****************************************************************************/

static long tran_thp_input_report_touch(struct fts_ts_data *ts_data, unsigned long arg)
{
    int i = 0;
    int touch_num = 0;
    struct input_dev *input_dev;
    struct tran_thp_alg_info ioctl_touch_event;
    struct tran_thp_touch_event *events;
    int touch_down_point_cur = 0;
    int touch_point_pre = 0;
    bool touch_event_coordinate = false;
    u32 max_touch_num;
    int water_mode = 0;
    u8 res_mul = 1;
    static int water_mode_pre = -1;
    int ret = 0;

    if (!ts_data || !ts_data->thp_data) {
        FTS_ERROR("ts_data/thp_data is null");
        return -EINVAL;
    }

    if (copy_from_user(&ioctl_touch_event, (void *)arg, sizeof(struct tran_thp_alg_info))) {
        FTS_ERROR("copy input touch report data from userspace fail");
        return -EFAULT;
    }
    //mutex_lock(&ts_data->report_mutex);
    res_mul = ts_data->res_mul;
    touch_num = ioctl_touch_event.ts_events.touch_num;
    max_touch_num = THP_MAX_POINTS_SUPPORT;
    touch_point_pre = ts_data->touch_points;
    events = &ioctl_touch_event.ts_events.events[0];
    input_dev = ts_data->input_dev;
    water_mode = ioctl_touch_event.ts_events.water_mode;

    if (touch_num > max_touch_num) {
        FTS_ERROR("touch_num:%d > max_touch_num:%d, invalid touch number", touch_num, max_touch_num);
        //mutex_unlock(&ts_data->report_mutex);
        return -EINVAL;
    }

    // notify IC water mode change, water_mode:0 is no water, non-0 do not enter idle mode
    if (water_mode != water_mode_pre) {
        ret = fts_write_reg(FTS_REG_WATER_MODE, water_mode ? FTS_ENTER_WATER_MODE : FTS_EXIT_WATER_MODE);
        if (ret < 0) {
            FTS_ERROR("set TP to water mode:%d fail, ret=%d", water_mode, ret);
        } else {
            FTS_INFO("set TP to water mode:%d success, pre water_mode:%d", water_mode, water_mode_pre);
            water_mode_pre = water_mode;
        }
    }

    for (i = 0; i < max_touch_num; i++) {
        struct tran_thp_touch_event *event = &events[i];

        if (!event->valid) {
            continue;
        }

        touch_event_coordinate = true;
        if (THP_EVENT_DOWN(event->flag)) {
            input_mt_slot(input_dev, event->id);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, event->major);
            input_report_abs(input_dev, ABS_MT_TOUCH_MINOR, event->minor);
            input_report_abs(input_dev, ABS_MT_POSITION_X, event->x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, event->y);

            touch_down_point_cur |= (1 << event->id);
            touch_point_pre |= (1 << event->id);

            if (THP_INPUT_DOWN == event->flag) {
                FTS_INFO("[THP][res:%d] P%d(%d, %d)[p:%d,tm:%d] DOWN!", res_mul, event->id, event->x / res_mul,
                          event->y / res_mul, event->p, event->major);
            }
            //trace_touchscreen_report(event->x, event->y, event->id, event->flag);
        } else {
            input_mt_slot(input_dev, event->id);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
            touch_point_pre &= ~(1 << event->id);

            if (THP_INPUT_UP == event->flag) {
                FTS_INFO("[THP][res:%d] P%d(%d, %d) UP!", res_mul, event->id, event->x / res_mul, event->y / res_mul);
            }
            //trace_touchscreen_report(event->x, event->y, event->id, event->flag);
        }
        tran_ts_trace_touchscreen_report(event->x, event->y, event->id, event->flag);
    }

    if (unlikely(touch_point_pre ^ touch_down_point_cur)) {
        int unmatched_points = touch_point_pre ^ touch_down_point_cur;
        for (i = 0; i < max_touch_num; i++) {
            if ((1 << i) & (unmatched_points)) {
                if (ts_data->log_level >= 1) {
                    FTS_DEBUG("[THP]P%d UP! pre:%d, cur:%d", i, touch_point_pre, touch_down_point_cur);
                }
                input_mt_slot(input_dev, i);
                input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
            }
        }
    }

    if (touch_down_point_cur) {
        input_report_key(input_dev, BTN_TOUCH, 1);
    } else if (touch_event_coordinate || ts_data->touch_points) {
        input_report_key(input_dev, BTN_TOUCH, 0);
    }

    ts_data->touch_points = touch_down_point_cur;
    input_sync(input_dev);

    //mutex_unlock(&ts_data->report_mutex);
    return 0;
}

static int tran_thp_ioctl_set_irq(struct fts_ts_data *ts_data, unsigned long arg)
{
    if (!ts_data) {
        FTS_ERROR("ts data is null");
        return -EINVAL;
    }

    FTS_INFO("set irq to %s", (int)arg ? "enable" : "disable");
    if (arg)
        fts_irq_enable();
    else
        fts_irq_disable();
    return 0;
}

static int tran_thp_ioctl_get_queue_param(struct fts_ts_data *ts_data, unsigned long arg)
{
    if (!ts_data || !ts_data->thp_data) {
        FTS_ERROR("ts_data/thp_data is null");
        return -EINVAL;
    }
    struct thp_queue_params param = ts_data->thp_data->q_params;

    if (copy_to_user((void *)arg, &param, sizeof(struct thp_queue_params))) {
        FTS_ERROR("copy thp queue params to userspace fail");
        return -EFAULT;
    }
    return 0;
}
static int tran_toupper(int c)
{
    if ((c >= 'a') && (c <= 'z'))
        return c + ('A' - 'a');
    return c;
}

static void tran_thp_get_chip_info(struct tran_thp_init_info *init_info)
{
    char ic_name[17] = {0};
    char chip_info[64] = {0};
    ssize_t length;
    struct tran_ts_controller *ctl = &ft3683g_controller;

    if (IS_ERR_OR_NULL(ctl) || IS_ERR_OR_NULL(ctl->cur_ic)) {
        snprintf(chip_info, sizeof(chip_info), "UNKNOWN");
    } else {
        int ic_name_len = strnlen(ctl->cur_ic, sizeof(ic_name) - 1);
        memcpy(ic_name, ctl->cur_ic, ic_name_len);
        ic_name[ic_name_len] = '\0';

        for (int i = 0; i < ic_name_len; i++) {
            ic_name[i] = tran_toupper((int)ic_name[i]);
        }

        if (!IS_ERR_OR_NULL(ctl->supplier) && !IS_ERR_OR_NULL(ctl->supplier->supplier_vendor)) {

            length = snprintf(chip_info, sizeof(chip_info), "%s_%s_%02d",
                              ctl->supplier->supplier_vendor,
                              ic_name,
                              ctl->supplier->supplier_num);

            if (length >= sizeof(chip_info)) {
                TRAN_ERROR("chip_info was truncated, length:%zu size:%zu", length, sizeof(chip_info));
                chip_info[sizeof(chip_info) - 1] = '\0';
            }
        } else {
            snprintf(chip_info, sizeof(chip_info), "UNKNOWN");
        }
    }
    strncpy(init_info->chip_info, chip_info, sizeof(init_info->chip_info) - 1);
    init_info->chip_info[sizeof(init_info->chip_info) - 1] = '\0';
}

static int tran_thp_ioctl_get_init_param(struct fts_ts_data *ts_data, unsigned long arg)
{
    if (!ts_data || !ts_data->thp_data) {
        FTS_ERROR("ts_data/thp_data is null");
        return -EINVAL;
    }

    struct tran_thp_init_info info = {0};
    //memset(&info, 0, sizeof(info));

    tran_thp_get_chip_info(&info);
    info.param = ts_data->thp_data->q_params;
    info.scene = g_thp_scene;
    //memcpy(&info.scene, &g_thp_scene, sizeof(info.scene));
    FTS_INFO("get thp init param, is_awake:%d", info.scene.is_awake);

    if (copy_to_user((void __user *)arg, &info, sizeof(struct tran_thp_init_info))) {
        FTS_ERROR("Failed to copy thp init info to userspace");
        return -EFAULT;
    }
    return 0;
}

static long tran_thp_get_fw_status(struct fts_ts_data *ts_data, unsigned long arg)
{
    uint8_t status = 0;

    if (!ts_data) {
        FTS_ERROR("ts_data is null");
        return -EINVAL;
    }

    /* status 1:done, 0:not done, others:reserved */
    status = ts_data->fw_loading ? 0 : 1;
    FTS_INFO("Firmware update status: %s", status ? "done" : "in progress");
    if (copy_to_user((void __user *)arg, &status, sizeof(uint8_t))) {
        FTS_ERROR("Failed to copy firmware status to userspace");
        return -EFAULT;
    }

    return 0;
}

static long tran_thp_get_alg_status(struct fts_ts_data *ts_data, unsigned long arg)
{
    struct tran_thp_alg_debug alg_status;

    if (!ts_data) {
        FTS_ERROR("ts_data is null");
        return -EINVAL;
    }
    if (copy_from_user(&alg_status, (void __user *)arg, sizeof(struct tran_thp_alg_debug))) {
        FTS_ERROR("Failed to copy thp alg status from userspace");
        return -EFAULT;
    }
    ts_data->thp_palm = (bool)alg_status.reserved[0];
    FTS_INFO("[THP]get thp alg status, palm:%d", alg_status.reserved[0]);
    return 0;
}

static int tran_thp_set_version_info(struct fts_ts_data *ts_data, long arg)
{
    struct thp_version_info ver_info;
    if (!ts_data || !ts_data->thp_data) {
        FTS_ERROR("ts_data/thp_data is null");
        return -EINVAL;
    }
    FTS_INFO("set thp version info");
    if (copy_from_user(&ver_info, (void __user *)arg, sizeof(struct thp_version_info))) {
        FTS_ERROR("copy thp version info from userspace fail");
        return -EFAULT;
    }
    memcpy(&ts_data->thp_data->ver_info, &ver_info, sizeof(struct thp_version_info));
    FTS_INFO("thp init version info: %s", ts_data->thp_data->ver_info.ini_version);
    FTS_INFO("thp hal version info: %s", ts_data->thp_data->ver_info.hal_version);
    FTS_INFO("thp algo version info: %s", ts_data->thp_data->ver_info.algo_version);
    FTS_INFO("thp edge version info: %s", ts_data->thp_data->ver_info.edge_version);
    return 0;
}
static int tran_thp_ioctl_set_thp_mode(struct fts_ts_data *ts_data, long arg)
{
    bool thp_mode = true;
    if (!ts_data || !ts_data->thp_data) {
        FTS_ERROR("ts_data/thp_data is null");
        return -EINVAL;
    }
    if (copy_from_user(&thp_mode, (void __user *)arg, sizeof(bool))) {
        FTS_ERROR("copy thp mode from userspace fail");
        return -EFAULT;
    }
    //false:mcu，true:thp
    FTS_INFO("set mode to :%s", thp_mode ? "thp" : "mcu");
    ts_data->thp_data->thp_mode_debug = thp_mode;
    ts_data->thp_data->thp_mode = thp_mode;
    return 0;
}

static int tran_thp_input_open(struct inode *inode, struct file *file)
{
    FTS_INFO("open thp input device");
    return 0;
}

static int tran_thp_input_close(struct inode *inode, struct file *file)
{
    FTS_INFO("close thp input device");
    return 0;
}
static long tran_thp_input_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct fts_ts_data *ts_data = container_of(file->private_data, struct fts_ts_data, misc_dev);

    if (!ts_data) {
        FTS_ERROR("ts_data is null");
        return -ENODEV;
    }
    //FTS_INFO("ioctl THP_IOCTL_INPUT_REPORTER_TOUCH:0x%lx", THP_IOCTL_INPUT_REPORTER_TOUCH);
    switch (cmd) {
        case THP_IOCTL_INPUT_REPORTER_TOUCH:
            mutex_lock(&ts_data->report_mutex);
            ret = tran_thp_input_report_touch(ts_data, arg);
            mutex_unlock(&ts_data->report_mutex);
            break;
        case THP_IOCTL_INPUT_RELEASE_ALL:
            fts_release_all_finger();
            break;
        case THP_IOCTL_INPUT_SET_IRQ:
            ret = tran_thp_ioctl_set_irq(ts_data, arg);
            break;
        case THP_IOCTL_INPUT_RESET:
            fts_reset_proc(100);
            break;
        case THP_IOCTL_INPUT_GET_QUEUE_PARAM:
            ret = tran_thp_ioctl_get_queue_param(ts_data, arg);
            break;
        case THP_IOCTL_GET_INIT_INFO:
            ret = tran_thp_ioctl_get_init_param(ts_data, arg);
            break;
        case THP_IOCTL_INPUT_GET_FW_STATUS:
            ret = tran_thp_get_fw_status(ts_data, arg);
            break;
        case THP_IOCTL_INPUT_SET_VERSION_INFO:
            ret = tran_thp_set_version_info(ts_data, arg);
            break;
        case THP_IOCTL_INPUT_SET_THP_MODE:
            ret = tran_thp_ioctl_set_thp_mode(ts_data, arg);
            break;
        case THP_IOCTL_INPUT_GET_ALG_STATUS:
            ret = tran_thp_get_alg_status(ts_data, arg);
            break;
        default:
            FTS_ERROR("unknown ioctl cmd:0x%x,type:%x,nr:%d", cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
            ret = -ENOTTY;
            break;
    }

    return ret;
}
static unsigned int tran_thp_input_poll(struct file *file, struct poll_table_struct *wait)
{
    FTS_INFO("poll thp input device");
    return 0;
}
static struct file_operations tran_thp_input_fops = {
    .owner = THIS_MODULE,
    .open = tran_thp_input_open,
    .release = tran_thp_input_close,
    .unlocked_ioctl = tran_thp_input_ioctl,
    .poll = tran_thp_input_poll,
};

static int tran_thp_miscdev_init(struct fts_ts_data *ts_data)
{
    int ret = 0;

    ts_data->misc_dev.minor = MISC_DYNAMIC_MINOR;
    ts_data->misc_dev.name = THP_INPUT_MISC_NAME;
    ts_data->misc_dev.fops = &tran_thp_input_fops;
    ret = misc_register(&ts_data->misc_dev);
    if (ret) {
        FTS_ERROR("misc_register(/dev/%s) failed", ts_data->misc_dev.name);
        return ret;
    }
    FTS_INFO("misc_register(/dev/%s) success", ts_data->misc_dev.name);
    return 0;
}

static void tran_thp_init_param(struct fts_ts_data *ts_data)
{
    uint32_t total_frame_len;

    if (!ts_data || !ts_data->thp_data || !ts_data->pdata) {
        FTS_ERROR("Invalid parameters");
        return;
    }

    const struct tran_thp_custom_param *param = &ts_data->pdata->thp_custom_param;

    if (param->thp_frame_len == 0 || param->thp_crc_len == 0) {
        FTS_ERROR("Invalid frame_len(%u) or crc_len(%u)", 
                 param->thp_frame_len, param->thp_crc_len);
        return;
    }

    total_frame_len = param->thp_frame_len + sizeof(struct tran_thp_frame);

    ts_data->thp_data->q_params = (struct thp_queue_params){
        .queue_size = TRAN_FRAME_BUF,
        .queue_count = TRAN_MAX_QUEUE_COUNT,
        .frame_len = total_frame_len,
        .crc_len = param->thp_crc_len,
        .thp_tx_num = param->thp_tx_num,
        .thp_rx_num = param->thp_rx_num
    };

    /* Initialize individual parameters */
    ts_data->thp_data->thp_custom_param = *param;

    /* Log successful initialization with detailed information */
    FTS_INFO("THP parameters initialized successfully: "
             "frame_len=%u(+%zu)=%u, crc_len=%u, tx=%u, rx=%u, queues=%u",
             param->thp_frame_len,
             sizeof(struct tran_thp_frame),
             total_frame_len,
             param->thp_crc_len,
             param->thp_tx_num,
             param->thp_rx_num,
             (unsigned int)TRAN_MAX_QUEUE_COUNT);
}
#if 0
#define CRC32_POLYNOMIAL 0xE89061DB
static uint32_t crc32_table[256];
static void init_crc32_table(void)
{
    uint32_t i, j, crc;
    for (i = 0; i < 256; ++i) {
        crc = i << 24;
        for (j = 0; j < 8; ++j) {
            if (crc & 0x80000000)
                crc = (crc << 1) ^ CRC32_POLYNOMIAL;
            else
                crc <<= 1;
        }
        crc32_table[i] = crc;
    }
}
static uint32_t tran_thp_crc32_check(int32_t s32_message[], int s32_len) {
    int i, j;
    uint32_t s32_remainder = 0UL;
    unsigned char u8_byteData;

    for (i = 0; i < s32_len; i++) {
        for (j = 0; j < 4; j++) {
            u8_byteData = (s32_message[i] >> (j * 8)) & 0xFF;
            s32_remainder = crc32_table[(s32_remainder >> 24) ^ u8_byteData] ^ (s32_remainder << 8);
        }
    }
    return s32_remainder;
}
#endif

static s32 tran_thp_raw_checksum(u8 *pBuf, u16 lenth)
{
    u16 i;
    s32 checksum;
    checksum = 0;
    for (i = 0; i < lenth; i++)
    {
        checksum += pBuf[i];
    }
    checksum = ~checksum;
    checksum += 1;
    return checksum;
}

int tran_thp_init(struct fts_ts_data *ts_data)
{
    int ret = 0;

    if (!ts_data || !ts_data->pdata) {
        FTS_ERROR("Invalid ts_data or ts_data->pdata");
        return -EINVAL;
    }

    if (!ts_data->pdata->tran_thp_support) {
        FTS_INFO("tran_thp_support is false");
        ts_data->thp_data_flag = false;
        ts_data->edge_mode = false;
        return 0;
    }

    ts_data->thp_data = tran_thp_device_init();
    if (!ts_data->thp_data || !ts_data->thp_data->q_ops) {
        ts_data->thp_data_flag = false;
        FTS_ERROR("Failed to initialize thp_data or q_ops");
        return -EINVAL;
    } else {
        ts_data->thp_data_flag = true;
        ts_data->thp_data->big_endian = true;
        //init_crc32_table();
        ts_data->thp_data->q_ops->crc32_check = tran_thp_raw_checksum;
        tran_thp_init_param(ts_data);
    }
    ret = tran_thp_miscdev_init(ts_data);
    if (ret) {
        FTS_ERROR("Failed to register thp misc device");
        goto err_misc_init_fail;
    }
    // if use tran edge algo, enable edge mode
    ts_data->edge_mode = true;

    FTS_INFO("Successfully init thp");
    return 0;

err_misc_init_fail:
    if (ts_data->thp_data) {
        ts_data->thp_data_flag = false;
        tran_thp_device_exit(ts_data->thp_data);
        ts_data->thp_data = NULL;
    }
    return ret;
}

void tran_thp_exit(struct fts_ts_data *ts_data)
{
    if (ts_data && ts_data->thp_data_flag) {
        misc_deregister(&ts_data->misc_dev);
        if (ts_data->thp_data) {
            ts_data->thp_data_flag = false;
            tran_thp_device_exit(ts_data->thp_data);
            ts_data->thp_data = NULL;
        }
    }
}
