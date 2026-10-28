/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*****************************************************************************
*
* File Name: focaltech_hp.c
*
* Author: Focaltech Driver Team
*
* Created: 2023-12-12
*
* Abstract: Host Processing
*
* Version: V1.0
*
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "focaltech_core.h"
#include <linux/jiffies.h>

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_FHP_NAME                        "fhp_ft"
#define FTS_FHPI_DEV_NAME                   "fhp_input"

#define DEFAULT_IOCTL_SPI_BUFSIZE           256
#define MAX_QUEUE_BUFSIZE                   65536
#define MAX_QUEUE_COUNT                     16
#define MAX_CHIP_FRAME                      16384
#define MAX_DRV_STATUS                      32

#define FHP_CMD_GET_FRAME                   0x01

#define MIN_COMPENSATION_INTERVAL (HZ / 20)  // 50ms

struct fhp_spi_sync_data {
    char *tx;
    char *rx;
    unsigned int size;
};

struct fhp_safe_frame_data {
    unsigned int max_size;
    char *buff;
    unsigned int *read_size;
};

struct fhp_cfg {
    u32 fhp_mode        : 1;
    u32 irq_status      : 1;
    u32 unused          : 30;
    int speed;
    int open_cnt;
    int timeout;
    int frame_waitq_flag;
    int touch_size;
    int queue_count;
    int queue_elem_size;
    int queue_buf_size;
    int r[7];
};

struct fhp_input_report_data {
    char *buffer;
    unsigned int size;
};

struct fhp_input_cfg {
    int version;
    int x_max;
    int y_max;
    int max_touch_num;
    int vnd_num;
    u32 en_pen                  : 1;
    u32 en_touch_hires          : 1;
    u32 en_pen_hires            : 1;
    u32 en_report_pressure      : 1;
    u32 en_r                    : 28;
    int touch_hires_x;
    int touch_hires_max;
    int pen_hires_x;
    int pen_hires_max;
    int r[23];
};

struct fhp_input_ioctl_touch_data {
    struct ts_event events[FTS_MAX_POINTS_SUPPORT];
    int touch_num;
    int event_num;
};

struct fhp_input_ioctl_pen_data {
    struct pen_event events;
    int touch_num;
    int event_num;
};

#define FHP_IOCTL_DEVICE                    0xC5
#define FHP_IOCTL_DEVICE_VER                0x0002
#define FHP_IOCTL_RESET                     _IOW(FHP_IOCTL_DEVICE, 0x01, long)
#define FHP_IOCTL_SPI_SYNC                  _IOWR(FHP_IOCTL_DEVICE, 0x02, struct fhp_spi_sync_data)
#define FHP_IOCTL_SET_IRQ                   _IOW(FHP_IOCTL_DEVICE, 0x03, long)
#define FHP_IOCTL_SET_RAW_SIZE              _IOW(FHP_IOCTL_DEVICE, 0x04, long)
#define FHP_IOCTL_SET_SPI_SPEED             _IOW(FHP_IOCTL_DEVICE, 0x05, long)
#define FHP_IOCTL_GET_CHIP_INIT_DONE        _IOR(FHP_IOCTL_DEVICE, 0x06, long)
#define FHP_IOCTL_GET_FRAME                 _IOR(FHP_IOCTL_DEVICE, 0x07, void *)
#define FHP_IOCTL_CLEAR_FRAME               _IOW(FHP_IOCTL_DEVICE, 0x08, long)
#define FHP_IOCTL_SET_TIMEOUT               _IOW(FHP_IOCTL_DEVICE, 0x09, long)
#define FHP_IOCTL_REPORT                    _IOW(FHP_IOCTL_DEVICE, 0x0A, struct fhp_input_report_data)
#define FHP_IOCTL_GET_VER                   _IOR(FHP_IOCTL_DEVICE, 0x0B, long)
#define FHP_IOCTL_GET_FRAME_SAFE            _IOR(FHP_IOCTL_DEVICE, 0x0C, struct fhp_safe_frame_data)
#define FHP_IOCTL_GET_CFG                   _IOR(FHP_IOCTL_DEVICE, 0x0D, struct fhp_cfg)

#define FHP_INPUT_IOCTL_DEVICE              0xC6
#define FHP_INPUT_IOCTL_DEVICE_VER          0x0003
#define FHP_INPUT_IOCTL_REPORT              _IOW(FHP_INPUT_IOCTL_DEVICE, 0x01, struct fhp_input_report_data)
#define FHP_INPUT_IOCTL_GET_CFG             _IOR(FHP_INPUT_IOCTL_DEVICE, 0x02, struct fhp_input_cfg)
#define FHP_INPUT_IOCTL_REPORT_TOUCH        _IOW(FHP_INPUT_IOCTL_DEVICE, 0x03, struct fhp_input_ioctl_touch_data)
#define FHP_INPUT_IOCTL_REPORT_PEN          _IOW(FHP_INPUT_IOCTL_DEVICE, 0x04, struct fhp_input_ioctl_pen_data)
#define FHP_INPUT_IOCTL_RELEASE_ALL         _IO(FHP_INPUT_IOCTL_DEVICE, 0x05)
#define FHP_INPUT_IOCTL_GET_FIRMWARE_STATE  _IOR(FHP_INPUT_IOCTL_DEVICE, 0x06, int)
#define FHP_INPUT_IOCTL_REPORT_FOD          _IOW(FHP_INPUT_IOCTL_DEVICE, 0x07, int)
#define FHP_INPUT_IOCTL_REPORT_PALM         _IOW(FHP_INPUT_IOCTL_DEVICE, 0x08, int)
#define FHP_INPUT_IOCTL_SET_ALGO_VERSION    _IOW(FHP_INPUT_IOCTL_DEVICE, 0x09, char*)

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
typedef enum {
    FHP_FRAME_WAITQ_DEFAULT,
    FHP_FRAME_WAITQ_WAIT,
    FHP_FRAME_WAITQ_WAIT_POLL,
    FHP_FRAME_WAITQ_WAKEUP,
} FHP_FRAME_WAITQ_FLAG;

typedef enum {
    FHP_STATUS_WAITQ_DEFAULT,
    FHP_STATUS_WAITQ_WAIT,
    FHP_STATUS_WAITQ_WAIT_POLL,
    FHP_STATUS_WAITQ_WAKEUP,
} FHP_STATUS_WAITQ_FLAG;


/*USD:add for status notify by guoxin.zhu 20250307 start*/
#pragma pack(1)
struct fhp_status {
    u8 fhp_suspend_status;
    u8 fhp_fod_status;
    u8 fhp_rotation_status;
    u8 fhp_keypad_status;
    u8 fhp_scene_mode;
    u8 fhp_pocket_status;
};
#pragma pack()
/*USD:add for status notify by guoxin.zhu 20250307 end*/
struct fhp_frame {
    u64 tv;
    unsigned char value[0];
};

struct fhp_queue {
    int head;
    int tail;
    int count;
    int max_count;
    int elem_size;
    int buffer_size;
    u8 *buffer;
    struct mutex mutexq;
};

struct fhp_core_data {
    struct fts_ts_data *ts_data;
    struct miscdevice fhp_dev;
    struct miscdevice fhpi_dev;
    wait_queue_head_t fhpi_waitq;
    wait_queue_head_t frame_waitq;
    struct fhp_queue q;
    /*USD:add for status notify by guoxin.zhu 20250307 start*/
    struct mutex fhpi_mutex;
    struct fhp_status *drv_state;
    struct fhp_status *hal_state;
    int fhpi_state_rescend;
    unsigned long last_compensation_jiffies;
    /*USD:add for status notify by guoxin.zhu 20250307 end*/
    int fhp_dev_open_cnt;
    int timeout; /*0:no wait, <0 wait infinitely, >0 wait timeout*/
    int frame_waitq_flag;
    int fhpi_waitq_flag;
    u32 fhp_touch_size;
    u8 *fhp_touch_buf;
};

struct status_map {
    int class;
};

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
//static struct fhp_core_data *fts_fhp_data; by guoxin.zhu
struct fhp_core_data *fts_fhp_data;

/*USD:add for status notify by guoxin.zhu 20250307 start*/
static struct fhp_status g_fhp_drv_state;
static struct fhp_status g_fhp_hal_state;
static int g_status_map[] = {
    FHP_SUSPEND_TYPE,//FHP_RESUME_MODE = 0, /* thp mode */
    FHP_SUSPEND_TYPE,//FHP_GESTURE_MODE = 1, /* mcu mode */
    FHP_FOD_TYPE,//FHP_FOD_DOWN = 2,
    FHP_FOD_TYPE,//FHP_FOD_UP = 3,
    FHP_SUSPEND_TYPE,//FHP_SLEEP_MODE = 4, /* sleep mode */

    FHP_ROTATION_TYPE,//FHP_ROTAION_DOWN = 5,
    FHP_ROTATION_TYPE,//FHP_ROTAION_RIGHT = 6,
    FHP_ROTATION_TYPE,//FHP_ROTAION_LEFT = 7,

    FHP_SCENE_TYPE,//FHP_SCENE_NORMAL = 8,
    FHP_SCENE_TYPE,//FHP_SCENE_SV = 9,
    FHP_SCENE_TYPE,//FHP_SCENE_GAME = 10,
    FHP_KEYPAD_TYPE,//FHP_KEYPAD_ON = 11,
    FHP_KEYPAD_TYPE,//FHP_KEYPAD_OFF = 12,
    FHP_SCENE_TYPE,//FHP_SCENE_CAMERA = 13,
    FHP_POCKET_TYPE,//FHP_SCENE_POCKET_ON = 14,
    FHP_POCKET_TYPE,//FHP_SCENE_POCKET_OFF = 15,
};
/*USD:add for status notify by guoxin.zhu 20250307 end*/
static long fhp_input_report(struct fhp_core_data *fhp_data, unsigned long arg);
static long fhp_input_report_fod(struct fhp_core_data *fhp_data, unsigned long arg);

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static u64 fhp_get_timestamp(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
    ktime_t tv;
    tv = ktime_get_real() / 1000;
    return (u64)tv;
#else
    struct timeval tv;
    do_gettimeofday(&tv);
    return (u64)(((u64)tv.tv_sec * 1000000) + tv.tv_usec);
#endif
}

static int fhp_spi_sync(struct fhp_core_data *fhp_data, u8 *tx_buf, u8 *rx_buf, u32 len)
{
    int ret = 0;
    u8 *tbuf = NULL;
    u8 *rbuf = NULL;
    struct spi_message msg;
    struct spi_transfer xfer = { 0 };

    if (!fhp_data || !fhp_data->ts_data || !fhp_data->ts_data->spi) {
        FTS_ERROR("fhp/ts data/spi is null");
        return -EINVAL;
    }

    if (!fhp_data->ts_data->bus_tx_buf || !fhp_data->ts_data->bus_rx_buf || !tx_buf || !rx_buf || !len) {
        FTS_ERROR("tx_buf/rx_buf is null/len(%d) invalid", len);
        return -EINVAL;
    }

    mutex_lock(&fhp_data->ts_data->bus_lock);
    if (len > FTS_MAX_BUS_BUF) {
        tbuf = kzalloc(len, GFP_KERNEL);
        if (NULL == tbuf) {
            FTS_ERROR("txbuf malloc fail");
            ret = -ENOMEM;
            goto err_spi_sync;
        }

        rbuf = kzalloc(len, GFP_KERNEL);
        if (NULL == rbuf) {
            FTS_ERROR("rxbuf malloc fail");
            ret = -ENOMEM;
            goto err_spi_sync;
        }
    } else {
        tbuf = fhp_data->ts_data->bus_tx_buf;
        rbuf = fhp_data->ts_data->bus_rx_buf;
        memset(tbuf, 0x0, FTS_MAX_BUS_BUF);
        memset(rbuf, 0x0, FTS_MAX_BUS_BUF);
    }

    memcpy(tbuf, tx_buf, len);
    xfer.tx_buf = tbuf;
    xfer.rx_buf = rbuf;
    xfer.len = len;
    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);
    ret = spi_sync(fhp_data->ts_data->spi, &msg);
    memcpy(rx_buf, rbuf, len);
    if (ret) {
        FTS_ERROR("spi_sync fail,addr:%x,status:%x,ret:%d", tbuf[0], rbuf[3], ret);
        goto err_spi_sync;
    }

    ret = 0;
err_spi_sync:
    if (len > FTS_MAX_BUS_BUF) {
        if (tbuf) {
            kfree(tbuf);
            tbuf = NULL;
        }

        if (rbuf) {
            kfree(rbuf);
            rbuf = NULL;
        }
    }

    mutex_unlock(&fhp_data->ts_data->bus_lock);
    return ret;
}

static int fhpq_open(struct fhp_queue *q)
{
    int ret = 0;
    if (!q) {
        FTS_ERROR("q is null");
        return -EINVAL;
    }

    FTS_FUNC_ENTER();
    mutex_lock(&(q->mutexq));
    q->head = q->tail = q->count = 0;
    q->buffer_size = MAX_QUEUE_BUFSIZE;
    /*USD:modify for KE by guoxin.zhu 20250611 start*/
    if (q->buffer) {
        memset(q->buffer, 0, q->buffer_size);
        FTS_INFO("q->buffer already allocated, skipping allocation");
    } else {
        q->buffer = vmalloc(q->buffer_size);
        if (!q->buffer) {
            FTS_ERROR("malloc queue buffer failed");
            q->buffer_size = 0;
            ret = -ENOMEM;
        } else {
            memset(q->buffer, 0, q->buffer_size);
            FTS_INFO("malloc queue buffer succeeded,size=%d", q->buffer_size);
        }
    }
    /*USD:modify for KE by guoxin.zhu 20250611 end*/
    mutex_unlock(&(q->mutexq));
    FTS_FUNC_EXIT();
    return ret;
}

static int fhpq_set_element_size(struct fhp_queue *q, int elem_size)
{
    int ret = 0;
    if (!q || !elem_size || (elem_size > MAX_CHIP_FRAME)) {
        FTS_ERROR("q is null/elem_size:%d > max(%d)", elem_size, MAX_CHIP_FRAME);
        return -EINVAL;
    }

    FTS_FUNC_ENTER();
    mutex_lock(&(q->mutexq));
    q->elem_size = elem_size;
    q->max_count = MAX_QUEUE_COUNT;
    FTS_INFO("fhp_queue,elem_size=%d,max_count=%d,buffer size=%d", q->elem_size, q->max_count, q->buffer_size);
    if ((q->elem_size * q->max_count) > q->buffer_size) {
        if (q->buffer) {
            vfree(q->buffer);
            q->buffer = NULL;
        }
        q->buffer_size = q->elem_size * q->max_count;
        q->buffer = vmalloc(q->buffer_size);
        if (!q->buffer) {
            FTS_ERROR("malloc queue buffer failed");
            q->buffer_size = 0;
            ret = -ENOMEM;
        } else {
            FTS_INFO("malloc queue buffer succeeded,size=%d", q->buffer_size);
        }
    }
    mutex_unlock(&(q->mutexq));
    FTS_FUNC_EXIT();
    return ret;
}

static bool fhpq_full(struct fhp_queue *q)
{
    return q->count == q->max_count;
}

static bool fhpq_empty(struct fhp_queue *q)
{
    return q->count == 0;
}

static void fhpq_clear(struct fhp_queue *q)
{
    mutex_lock(&(q->mutexq));
    q->head = q->tail = q->count = 0;
    mutex_unlock(&(q->mutexq));
}

static int fhpq_enqueue(struct fhp_queue *q, u8 *val, u64 timestamp)
{
    struct fhp_frame *tail_elem = NULL;

    if (!q || !val || !q->buffer || !q->buffer_size || !q->elem_size || !q->max_count) {
        FTS_ERROR("q/val/buffer is null, or buffer_size/elem_size/max_count is 0");
        return -EINVAL;
    }

    mutex_lock(&(q->mutexq));
    tail_elem = (struct fhp_frame *)&q->buffer[q->tail * q->elem_size];
    tail_elem->tv = timestamp;
    /* copy frame data */
    memcpy(tail_elem->value, val, q->elem_size);
    q->tail = (q->tail + 1) % q->max_count;
    if (fhpq_full(q)) {
        q->head = (q->head + 1) % q->max_count;
    } else {
        q->count++;
    }
    mutex_unlock(&(q->mutexq));

    return 0;
}

static int fhpq_dequeue_userspace(struct fhp_queue *q, void __user *buff, unsigned int max_size)
{
    if (!q || !buff || !q->buffer) {
        FTS_ERROR("q/buff/buffer is null");
        return -EINVAL;
    }

    if ((max_size > 0) && (max_size < q->elem_size)) {
        FTS_ERROR("user buff size, max(%d) < elem(%d)", max_size, q->elem_size);
        return -EINVAL;
    }

    mutex_lock(&(q->mutexq));
    if (fhpq_empty(q)) {
        mutex_unlock(&(q->mutexq));
        return -ENODATA;
    }
    if (copy_to_user(buff, &q->buffer[q->head * q->elem_size], q->elem_size)) {
        FTS_ERROR("copy frame data to user failed");
        mutex_unlock(&(q->mutexq));
        return -EFAULT;
    }
    q->head = (q->head + 1) % q->max_count;
    q->count--;
    mutex_unlock(&(q->mutexq));
    return 0;
}

/*USD:modify for power off ke by guoxin.zhu start*/
static int fhpq_release(struct fhp_queue *q)
{
    FTS_FUNC_ENTER();
    if (q) {
        mutex_lock(&(q->mutexq));
        q->head = q->tail = q->count = 0;
        q->max_count = q->elem_size = 0;
        //if (q->buffer) {
        //    vfree(q->buffer);
        //    q->buffer = NULL;
        //}
        q->buffer_size = 0;
        mutex_unlock(&(q->mutexq));
    }
    FTS_FUNC_EXIT();
    return 0;
}
/*USD:modify for power off ke by guoxin.zhu end*/


static int fhp_ioctl_reset(struct fhp_core_data *fhp_data, unsigned long high)
{
    if (!fhp_data || !fhp_data->ts_data) {
        FTS_ERROR("fhp/ts data is null");
        return -EINVAL;
    }
    return fts_set_reset(fhp_data->ts_data, (int)high);
}

static int fhp_ioctl_set_irq(struct fhp_core_data *fhp_data, unsigned long enable)
{
    if (!fhp_data || !fhp_data->ts_data) {
        FTS_ERROR("fhp/ts data is null");
        return -EINVAL;
    }

    FTS_INFO("set irq to %s", enable ? "enable" : "disable");
    if (enable)
        fts_irq_enable();
    else
        fts_irq_disable();
    return 0;
}

static int fhp_ioctl_spi_sync(struct fhp_core_data *fhp_data, unsigned long arg)
{
    int ret = 0;
    struct fhp_spi_sync_data spi_data;
    u8 txbuf_temp[DEFAULT_IOCTL_SPI_BUFSIZE] = { 0 };
    u8 rxbuf_temp[DEFAULT_IOCTL_SPI_BUFSIZE] = { 0 };
    u8 *txbuf = txbuf_temp;
    u8 *rxbuf = rxbuf_temp;

    if (!fhp_data || !fhp_data->ts_data) {
        FTS_ERROR("fhp/ts data is null");
        return -EINVAL;
    }

    if (copy_from_user(&spi_data, (void *)arg, sizeof(struct fhp_spi_sync_data))) {
        FTS_ERROR("copy spi_sync data from userspace fail");
        return -EFAULT;
    }

    if (!spi_data.tx || !spi_data.rx || !spi_data.size) {
        FTS_ERROR("tx/rx/size(%d) from userspace are invalid", spi_data.size);
        return -EINVAL;
    }

    if (spi_data.size > DEFAULT_IOCTL_SPI_BUFSIZE) {
        txbuf = kmalloc(spi_data.size, GFP_KERNEL);
        rxbuf = kmalloc(spi_data.size, GFP_KERNEL);
        if (!txbuf || !rxbuf) {
            FTS_ERROR("kzalloc memory(size:%d) for spi tx/rx buffer fail", spi_data.size);
            ret = -ENOMEM;
            goto spi_sync_err;
        }
    }
    memset(txbuf, 0, spi_data.size);
    memset(rxbuf, 0, spi_data.size);

    if (copy_from_user(txbuf, spi_data.tx, spi_data.size)) {
        FTS_ERROR("copy spi tx data from userspace fail");
        ret = -EFAULT;
        goto spi_sync_err;
    }

    ret = fhp_spi_sync(fhp_data, txbuf, rxbuf, spi_data.size);
    if (ret) {
        FTS_ERROR("spi sync fail");
        ret = -EIO;
        goto spi_sync_err;
    }

    if (copy_to_user(spi_data.rx, rxbuf, spi_data.size)) {
        FTS_ERROR("copy spi rx data to userspace fail");
        ret = -EFAULT;
        goto spi_sync_err;
    }

    return 0;
spi_sync_err:
    if (spi_data.size > DEFAULT_IOCTL_SPI_BUFSIZE) {
        kfree(txbuf);
        kfree(rxbuf);
    }
    return ret;
}

static int fhp_ioctl_set_frame_size(struct fhp_core_data *fhp_data, unsigned long size)
{
    int ret = 0;
    int chip_frame_size = (int)size;
    int frame_size = 0;
    int max_size = MAX_CHIP_FRAME - sizeof(struct fhp_frame);

    if (!fhp_data || !fhp_data->ts_data || (chip_frame_size <= 0) || (chip_frame_size > max_size)) {
        FTS_ERROR("fhp/ts data is null/size(%d) is invalid", chip_frame_size);
        return -EINVAL;
    }

    FTS_INFO("set frame size to %d", chip_frame_size);
    frame_size = chip_frame_size + sizeof(struct fhp_frame);
    ret = fhpq_set_element_size(&fhp_data->q, frame_size);
    if (ret) {
        FTS_ERROR("set queue element size failed,ret=%d", ret);
        return ret;
    }
    fhp_data->fhp_touch_size = chip_frame_size;
    return 0;
}

static int fhp_ioctl_set_spi_speed(struct fhp_core_data *fhp_data, unsigned long speed)
{
    int ret = 0;
    if (!fhp_data || !fhp_data->ts_data || !fhp_data->ts_data->spi) {
        FTS_ERROR("fhp/ts data/spi is null");
        return -EINVAL;
    }

    if (fhp_data->ts_data->spi->max_speed_hz == speed) {
        FTS_INFO("spi speed == max_speed_hz");
        return 0;
    }

    FTS_INFO("set spi speed to %ld", speed);
    mutex_lock(&fhp_data->ts_data->bus_lock);
    fhp_data->ts_data->spi->max_speed_hz = speed;
    ret = spi_setup(fhp_data->ts_data->spi);
    if (ret < 0) {
        FTS_ERROR("spi setup fail,ret:%d", ret);
    }
    mutex_unlock(&fhp_data->ts_data->bus_lock);
    return ret;
}

static int fhp_ioctl_get_chip_init_done(struct fhp_core_data *fhp_data, unsigned long arg)
{
    int ret = 0;
    u8 status = 0;
    if (!fhp_data || !fhp_data->ts_data) {
        FTS_ERROR("fhp/ts data is null");
        return -EINVAL;
    }

    /* 1:init done, 0:not done, others:reserved */
    status = fhp_data->ts_data->fw_loading ? 0 : 1;
    FTS_INFO("chip init flag:%d", status);
    if (copy_to_user((void *)arg, (u8 *)&status, sizeof(u8))) {
        FTS_ERROR("copy chip_init_done flag data to userspace fail");
        ret = -EFAULT;
        return ret;
    }

    return 0;
}

static int fhp_ioctl_get_frame(struct fhp_core_data *fhp_data, unsigned long arg)
{
    int ret = 0;
    if (!fhp_data || !fhp_data->ts_data) {
        FTS_ERROR("fhp/ts data is null");
        return -EINVAL;
    }

    if ((fhp_data->ts_data->suspended) || (fhp_data->ts_data->fw_loading) || (fhp_data->ts_data->fw_in_esd_reset)) {
        return -EPERM;
    }

    if (fhpq_empty(&fhp_data->q) && (fhp_data->frame_waitq_flag != FHP_FRAME_WAITQ_WAIT_POLL)) {
        fhp_data->frame_waitq_flag = FHP_FRAME_WAITQ_WAIT;
        if (fhp_data->timeout == 0) {
            FTS_INFO("No frame data, and not wait");
            return -ENODATA;
        } else if (fhp_data->timeout < 0) {
            wait_event_interruptible(fhp_data->frame_waitq,
                                     fhp_data->frame_waitq_flag == FHP_FRAME_WAITQ_WAKEUP);
        } else if (fhp_data->timeout > 0) {
            ret = wait_event_interruptible_timeout(fhp_data->frame_waitq,
                                                   fhp_data->frame_waitq_flag == FHP_FRAME_WAITQ_WAKEUP,
                                                   msecs_to_jiffies(fhp_data->timeout));
            if (ret == 0) return -ETIMEDOUT;
        }
    }

    ret = fhpq_dequeue_userspace(&fhp_data->q, (void *)arg, 0);
    if (ret < 0) {
        FTS_ERROR("dequeue frame failed");
        return ret;
    }

    return 0;
}

static int fhp_ioctl_get_frame_safe(struct fhp_core_data *fhp_data, unsigned long arg)
{
    int ret = 0;
    struct fhp_safe_frame_data k_safe_frame_data;
    if (!fhp_data || !fhp_data->ts_data) {
        FTS_ERROR("fhp/ts data is null");
        return -EINVAL;
    }

    if ((fhp_data->ts_data->suspended) || (fhp_data->ts_data->fw_loading) || (fhp_data->ts_data->fw_in_esd_reset)) {
        return -EPERM;
    }

    if (copy_from_user(&k_safe_frame_data, (void *)arg, sizeof(struct fhp_safe_frame_data))) {
        FTS_ERROR("copy spi_sync data from userspace fail");
        return -EFAULT;
    }

    if (!k_safe_frame_data.buff || !k_safe_frame_data.read_size) {
        FTS_ERROR("safe_frame data, buff/read_size is null");
        return -EINVAL;
    }

    if (k_safe_frame_data.max_size < fhp_data->q.elem_size) {
        FTS_ERROR("safe_frame data, max size(%d) < elem(%d)", k_safe_frame_data.max_size, fhp_data->q.elem_size);
        return -EINVAL;
    }

    if (fhpq_empty(&fhp_data->q) && (fhp_data->frame_waitq_flag != FHP_FRAME_WAITQ_WAIT_POLL)) {
        fhp_data->frame_waitq_flag = FHP_FRAME_WAITQ_WAIT;
        if (fhp_data->timeout == 0) {
            FTS_INFO("No frame data, and not wait");
            return -ENODATA;
        } else if (fhp_data->timeout < 0) {
            wait_event_interruptible(fhp_data->frame_waitq,
                                     fhp_data->frame_waitq_flag == FHP_FRAME_WAITQ_WAKEUP);
        } else if (fhp_data->timeout > 0) {
            ret = wait_event_interruptible_timeout(fhp_data->frame_waitq,
                                                   fhp_data->frame_waitq_flag == FHP_FRAME_WAITQ_WAKEUP,
                                                   msecs_to_jiffies(fhp_data->timeout));
            if (ret == 0) return -ETIMEDOUT;
        }
    }

    ret = fhpq_dequeue_userspace(&fhp_data->q, (void *)(k_safe_frame_data.buff), k_safe_frame_data.max_size);
    if (ret < 0) {
        FTS_ERROR("dequeue frame failed");
        return ret;
    }

    if (copy_to_user(k_safe_frame_data.read_size, &(fhp_data->q.elem_size), sizeof(unsigned int))) {
        FTS_ERROR("copy frame_size data to user failed");
        return -EFAULT;
    }

    return 0;
}

static int fhp_ioctl_clear_frame(struct fhp_core_data *fhp_data, unsigned long arg)
{
    if (fhp_data) {
        FTS_INFO("clear queue frame");
        fhpq_clear(&fhp_data->q);
    }
    return 0;
}

static int fhp_ioctl_set_timeout(struct fhp_core_data *fhp_data, unsigned long arg)
{
    int timeout = (int)arg;
    if (!fhp_data) {
        FTS_ERROR("fhp data is null");
        return -EINVAL;
    }

    if (fhp_data->timeout != timeout) {
        FTS_INFO("set timeout:%d->%d", fhp_data->timeout, timeout);
        fhp_data->timeout = timeout;
    }
    return 0;
}

static long fhp_ioctl_get_cfg(struct fhp_core_data *fhp_data, unsigned long arg)
{
    struct fhp_cfg cfg;
    if (!fhp_data || !fhp_data->ts_data) {
        FTS_ERROR("fhp_data/ts_data is null");
        return -EINVAL;
    }

    memset(&cfg, 0, sizeof(struct fhp_cfg));
    cfg.fhp_mode = fhp_data->ts_data->fhp_mode;
    cfg.irq_status = !fhp_data->ts_data->irq_disabled;
    cfg.speed = fhp_data->ts_data->spi_speed;
    cfg.open_cnt = fhp_data->fhp_dev_open_cnt;
    cfg.timeout = fhp_data->timeout;
    cfg.frame_waitq_flag = fhp_data->frame_waitq_flag;
    cfg.touch_size = fhp_data->fhp_touch_size;
    cfg.queue_count = fhp_data->q.count;
    cfg.queue_elem_size = fhp_data->q.elem_size;
    cfg.queue_buf_size = fhp_data->q.buffer_size;
    if (copy_to_user((void *)arg, (void *)(&cfg), sizeof(struct fhp_cfg))) {
        FTS_ERROR("copy fhp config to userspace failed");
        return -EFAULT;
    }

    return 0;
}

static int fhp_open(struct inode *inode, struct file *file)
{
    int ret = 0;
    struct fhp_core_data *fhp_data = container_of(file->private_data, struct fhp_core_data, fhp_dev);
    struct device *dev = NULL;
    FTS_FUNC_ENTER();
    if (!fhp_data && !fhp_data->ts_data) {
        FTS_ERROR("fhp_data/ts_data is null");
        return -ENODATA;
    }

    if (fhp_data->fhp_dev_open_cnt >= 1) {
        FTS_ERROR("open fail(cnt:%d)", fhp_data->fhp_dev_open_cnt);
        return -EBUSY;
    }

    dev = fhp_data->ts_data->dev;
    /*USD:modif for fix power off KE by guoxin.zhu 0611 start*/
    if (!(fhp_data->fhp_touch_buf)) {
        FTS_INFO("malloc for fhp touch buffer,size=%d", MAX_CHIP_FRAME);
        fhp_data->fhp_touch_buf = devm_kzalloc(dev, MAX_CHIP_FRAME, GFP_KERNEL);
        if (!(fhp_data->fhp_touch_buf)) {
            FTS_ERROR("allocate memory for fhp_touch_buf failed");
            return -ENOMEM;
        }
    } else {
        memset(fhp_data->fhp_touch_buf, 0, MAX_CHIP_FRAME);
        FTS_INFO("fhp touch buffer already allocated, skipping allocation");
    }
    /*USD:modif for fix power off KE by guoxin.zhu 0611 end*/

    fts_irq_disable();
    ret = fhpq_open(&fhp_data->q);
    if (ret) {
        FTS_ERROR("queue open failed");
        return ret;
    }

    fhp_data->frame_waitq_flag = FHP_FRAME_WAITQ_DEFAULT;
    fhp_data->fhp_touch_size = 0;
    fhp_data->timeout = 0;
    fhp_data->fhp_dev_open_cnt++;
    fhp_data->ts_data->fhp_mode = ENABLE;
    if (!fhp_data->ts_data->suspended) {
        fhp_data->drv_state->fhp_suspend_status = FHP_RESUME_MODE;
        FTS_INFO("fhp suspend status is 0");
    }
    FTS_INFO("open successfully(cnt:%d)", fhp_data->fhp_dev_open_cnt);
    FTS_FUNC_EXIT();
    return 0;
}

/*USD:modify for power off KE by guoxin.zhu 20260611 start*/
static int fhp_close(struct inode *inode, struct file *file)
{
    struct fhp_core_data *fhp_data = container_of(file->private_data, struct fhp_core_data, fhp_dev);

    FTS_FUNC_ENTER();
    if (!fhp_data || !fhp_data->ts_data) {
        FTS_ERROR("fhp_data/ts_data is null");
        return -ENODATA;
    }

    fts_irq_disable();
    fhp_data->ts_data->fhp_mode = DISABLE;
    //recover mcu mode
    fhp_status_switch_trigger(1);
    fhpq_release(&fhp_data->q);
    fhp_data->fhp_dev_open_cnt--;
    fhp_data->fhp_touch_size = 0;
    //if (fhp_data->fhp_touch_buf) {
    //    kfree(fhp_data->fhp_touch_buf);
    //    fhp_data->fhp_touch_buf = NULL;
    //}

    fts_irq_enable();
    FTS_FUNC_EXIT();
    return 0;
}
/*USD:modify for power off KE by guoxin.zhu 20260611 end*/

static long fhp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct fhp_core_data *fhp_data = container_of(filp->private_data, struct fhp_core_data, fhp_dev);

    if (!fhp_data) {
        FTS_ERROR("fhp_data is null");
        return -ENODATA;
    }

    //FTS_DEBUG("ioctl cmd:%x,arg:%lx", cmd, arg);
    switch (cmd) {
    case FHP_IOCTL_GET_FRAME_SAFE:
        ret = fhp_ioctl_get_frame_safe(fhp_data, arg);
        break;
    case FHP_IOCTL_GET_FRAME:
        ret = fhp_ioctl_get_frame(fhp_data, arg);
        break;
    case FHP_IOCTL_SPI_SYNC:
        ret = fhp_ioctl_spi_sync(fhp_data, arg);
        break;
    case FHP_IOCTL_RESET:
        ret = fhp_ioctl_reset(fhp_data, arg);
        break;
    case FHP_IOCTL_SET_IRQ:
        ret = fhp_ioctl_set_irq(fhp_data, arg);
        break;
    case FHP_IOCTL_SET_RAW_SIZE:
        fhp_ioctl_set_frame_size(fhp_data, arg);
        break;
    case FHP_IOCTL_SET_SPI_SPEED:
        ret = fhp_ioctl_set_spi_speed(fhp_data, arg);
        break;
    case FHP_IOCTL_GET_CHIP_INIT_DONE:
        ret = fhp_ioctl_get_chip_init_done(fhp_data, arg);
        break;
    case FHP_IOCTL_CLEAR_FRAME:
        ret = fhp_ioctl_clear_frame(fhp_data, arg);
        break;
    case FHP_IOCTL_SET_TIMEOUT:
        ret = fhp_ioctl_set_timeout(fhp_data, arg);
        break;
    case FHP_IOCTL_REPORT:
        ret = fhp_input_report(fhp_data, arg);
        break;
    case FHP_IOCTL_GET_VER:
        FTS_DEBUG("get fhp ver:0x%04x", FHP_IOCTL_DEVICE_VER);
        ret = put_user(FHP_IOCTL_DEVICE_VER, (long __user *)arg);
        break;
    case FHP_IOCTL_GET_CFG:
        ret = fhp_ioctl_get_cfg(fhp_data, arg);
        break;
    default:
        FTS_ERROR("unkown ioctl cmd:0x%x,type:%x,nr:%d", (int)cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
        ret = -EINVAL;
        break;
    }

    return ret;
}

static unsigned int fhp_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    struct fhp_core_data *fhp_data = container_of(filp->private_data, struct fhp_core_data, fhp_dev);

    if (!fhp_data) {
        FTS_ERROR("fhp_data is null");
        return -ENODATA;
    }

    poll_wait(filp, &fhp_data->frame_waitq, wait);
    if (fhp_data->frame_waitq_flag == FHP_FRAME_WAITQ_WAKEUP)
        mask |= POLLIN | POLLRDNORM;

    fhp_data->frame_waitq_flag = FHP_FRAME_WAITQ_WAIT_POLL;
    return mask;
}

static struct file_operations fhp_fops = {
    .open = fhp_open,
    .release = fhp_close,
    .unlocked_ioctl = fhp_ioctl,
    .poll = fhp_poll,
};

static int fhp_miscdev_init(struct fhp_core_data *fhp_data)
{
    int ret = 0;

    fhp_data->fhp_dev.minor = MISC_DYNAMIC_MINOR;
    fhp_data->fhp_dev.name = FTS_FHP_NAME;
    fhp_data->fhp_dev.fops = &fhp_fops;
    ret = misc_register(&fhp_data->fhp_dev);
    if (ret) {
        FTS_ERROR("misc_register(fhp_dev) fail");
        return ret;
    }

    FTS_INFO("misc_register(fhp_dev) success");
    return 0;
}

static long fhp_input_report(struct fhp_core_data *fhp_data, unsigned long arg)
{
    int ret = 0;
    u8 report_buf[FTS_TOUCH_DATA_LEN_V2] = { 0 };
    struct fhp_input_report_data report;

    if (!fhp_data || !fhp_data->ts_data) {
        FTS_ERROR("fhp_data/ts_data is null");
        return -EINVAL;
    }

    if (copy_from_user(&report, (void *)arg, sizeof(struct fhp_input_report_data))) {
        FTS_ERROR("copy input touch report data from userspace fail");
        return -EFAULT;
    }

    if ((report.size <= 0) || (report.size > FTS_TOUCH_DATA_LEN_V2)) {
        FTS_ERROR("report size(%d) is invalid", report.size);
        return -EINVAL;
    }

    memset(report_buf, 0xFF, FTS_TOUCH_DATA_LEN_V2);
    if (copy_from_user(report_buf, report.buffer , report.size)) {
        FTS_ERROR("copy input touch report buffer from userspace fail");
        return -EFAULT;
    }

    if ((report_buf[1] == 0xFF) && (report_buf[2] == 0xFF) && (report_buf[3] == 0xFF) && (report_buf[4] == 0xFF)) {
        fts_release_all_finger();
        fts_tp_state_recovery(fhp_data->ts_data);
        return 0;
    }

    ret = fts_input_report_buffer(fhp_data->ts_data, report_buf);
    if (ret < 0) {
        FTS_ERROR("report buffer failed");
        return ret;
    }
    return 0;
}

static long fhp_input_report_fod(struct fhp_core_data *fhp_data, unsigned long arg)
{
    int ret = 0;
    int fod_down = 0;
    struct fts_ts_data *ts_data = fhp_data->ts_data;

    if (!fhp_data || !ts_data) {
        FTS_ERROR("fhp_data/ts_data is null");
        return -EINVAL;
    }

    if (copy_from_user(&fod_down, (void *)arg, sizeof(int))) {
        FTS_ERROR("copy input fod status from userspace fail");
        return -EFAULT;
    }

    if (tran_fingerprint_status && fod_down) {
        /* FOD down, need report to tell host */
        input_report_key(ts_data->input_dev, KEY_FINGER, 1);
        input_sync(ts_data->input_dev);
        ts_data->fod_fp_down = true;
    } else {
        /* FOD up, need report to tell host */
        fhp_status_switch_trigger(3);
        input_report_key(ts_data->input_dev, KEY_FINGER, 0);
        input_sync(ts_data->input_dev);
        ts_data->fod_fp_down = false;
    }

    return ret;
}
static void tran_pocket_state_report(struct fts_ts_data *ts_data, int palm)
{
    //int ret = 0;
    //u8 palm = 0;
    uint8_t tran_pocket_state;

    tran_pocket_state = ft3683g_controller.tran_ts_data->tran_pocket_state;
    if (ts_data->pdata->pocket_func_support && tran_pocket_state) {
        if(!ts_data->suspended && palm==1 && !ts_data->palmflag){
            input_report_key(ts_data->input_dev, KEY_LARGE_AREA, 1);
            input_sync(ts_data->input_dev);
            ts_data->palmflag = true;
            FTS_INFO("fhp:Large area touch down");
        }else if((palm == 0) && (ts_data->palmflag)){
            input_report_key(ts_data->input_dev, KEY_LARGE_AREA, 0);
            input_sync(ts_data->input_dev);
            FTS_INFO("fhp:Large area touch up");
            ts_data->palmflag = false;
        }
        return;
    }
    //Palm in, and fingerprint unlocking to enter the system
    ts_data->palmflag = false;
}
static long fhp_input_report_palm(struct fhp_core_data *fhp_data, unsigned long arg)
{
    int ret = 0;
    int palm_flag = 0;
    struct fts_ts_data *ts_data = fhp_data->ts_data;

    if (!fhp_data || !ts_data) {
        FTS_ERROR("fhp_data/ts_data is null");
        return -EINVAL;
    }

    if (copy_from_user(&palm_flag, (void *)arg, sizeof(int))) {
        FTS_ERROR("copy input palm status from userspace fail");
        return -EFAULT;
    }
    if (palm_flag == 2){
        FTS_INFO("fhp:palm flag is 2,support tran palm");
        ts_data->tran_palm_support = true;
        return ret;
    }
    tran_pocket_state_report(ts_data, palm_flag);

    return ret;
}
static long fhp_input_get_cfg(struct fhp_core_data *fhp_data, unsigned long arg)
{
    struct fhp_input_cfg cfg;
    struct supplier_list *tmpList = ft3683g_controller.supplier;
    if (!fhp_data || !fhp_data->ts_data || !fhp_data->ts_data->pdata || !tmpList) {
        FTS_ERROR("fhp_data/ts_data/pdata is null");
        return -EINVAL;
    }

    memset(&cfg, 0, sizeof(struct fhp_input_cfg));
    cfg.version = FHP_INPUT_IOCTL_DEVICE_VER;
    cfg.x_max = fhp_data->ts_data->pdata->x_max;
    cfg.y_max = fhp_data->ts_data->pdata->y_max;
    cfg.vnd_num = tmpList->supplier_num;
    cfg.max_touch_num = fhp_data->ts_data->pdata->max_touch_number;
    cfg.en_pen = FTS_PEN_EN;
    cfg.en_touch_hires = FTS_TOUCH_HIRES_EN;
    cfg.en_pen_hires = FTS_PEN_HIRES_EN;
    cfg.en_report_pressure = FTS_REPORT_PRESSURE_EN;
    cfg.touch_hires_x = FTS_TOUCH_HIRES_X;
    cfg.touch_hires_max = FTS_HI_RES_X_MAX;
    cfg.pen_hires_x = FTS_PEN_HIRES_X;
    cfg.pen_hires_max = FTS_HI_RES_X_MAX;
    if (copy_to_user((void *)arg, (void *)(&cfg), sizeof(struct fhp_input_cfg))) {
        FTS_ERROR("copy input config to userspace failed");
        return -EFAULT;
    }

    return 0;
}

static long fhp_input_set_algo_version(struct fhp_core_data *fhp_data, unsigned long arg)
{
    char algo_version[32] = {0};
    if (!fhp_data || !fhp_data->ts_data) {
        FTS_ERROR("fhp_data/ts_data is null");
        return -EINVAL;
    }

    if (copy_from_user(algo_version, (void *)arg, sizeof(algo_version) - 1)) {
        FTS_ERROR("copy input algo version from userspace fail");
        return -EFAULT;
    }
    strncpy(fhp_data->ts_data->algo_version, algo_version, sizeof(fhp_data->ts_data->algo_version) - 1);
    fhp_data->ts_data->algo_version[sizeof(fhp_data->ts_data->algo_version) - 1] = '\0';
    FTS_INFO("get algo version: %s",fhp_data->ts_data->algo_version);
    return 0;
}
/*USD: add for transfer drv status by guoxin.zhu 20250307 start*/
static long fhp_input_get_firmware_mode(struct fhp_core_data *fhp_data, unsigned long arg)
{

    struct fhp_status status;
    if (!fhp_data || !fhp_data->ts_data || !fhp_data->ts_data->pdata) {
        FTS_ERROR("fhp_data/ts_data/pdata is null");
        return -EINVAL;
    }

    mutex_lock(&fhp_data->fhpi_mutex);
    FTS_INFO("fhpi wait falg: %d",fhp_data->fhpi_waitq_flag);
    status = g_fhp_drv_state;
    g_fhp_hal_state = g_fhp_drv_state;
    mutex_unlock(&fhp_data->fhpi_mutex);
    if (copy_to_user((void *)arg, (void *)(&status), sizeof(struct fhp_status))) {
        FTS_ERROR("copy input config to userspace failed");
        return -EFAULT;
    }

    return 0;
}
/*USD: add for transfer drv status by guoxin.zhu 20250307 end*/

static long fhp_input_report_touch(struct fhp_core_data *fhp_data, unsigned long arg)
{
    int i = 0;
    int max_touch_num = 0;
    struct input_dev *input_dev;
    struct fhp_input_ioctl_touch_data ioctl_touch_event;
    struct ts_event *events = &ioctl_touch_event.events[0];

    if (!fhp_data || !fhp_data->ts_data || !fhp_data->ts_data->pdata) {
        FTS_ERROR("fhp_data/ts_data/pdata is null");
        return -EINVAL;
    }

    if (copy_from_user(&ioctl_touch_event, (void *)arg, sizeof(struct fhp_input_ioctl_touch_data))) {
        FTS_ERROR("copy input touch report data from userspace fail");
        return -EFAULT;
    }

    max_touch_num = fhp_data->ts_data->pdata->max_touch_number;
    input_dev = fhp_data->ts_data->input_dev;
    mutex_lock(&fhp_data->ts_data->report_mutex);
    for (i = 0; i < max_touch_num; i++) {
        if (events[i].valid && events[i].down) {
            input_mt_slot(input_dev, i);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, events[i].major);
            input_report_abs(input_dev, ABS_MT_TOUCH_MINOR, events[i].minor);
            input_report_abs(input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, events[i].y);
#if FTS_REPORT_PRESSURE_EN
            input_report_abs(input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
            if ((fhp_data->ts_data->log_level >= 2) ||
                ((1 == fhp_data->ts_data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
                FTS_DEBUG("[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!", i, events[i].x,
                          events[i].y, events[i].p, events[i].major);
                    if(!IS_ERR_OR_NULL(ft3683g_controller.tran_ts_data)){
                        if(ft3683g_controller.tran_ts_data->is_ime_state){//count touch times in ime state
                            ft3683g_controller.tran_ts_data->bigdata_tt++;
                            if(i > 1){// count mistouch times when three fingers on
                                ft3683g_controller.tran_ts_data->bigdata_mt++;
                                FTS_DEBUG("tran mis touch cause");
                            }
                        }
                    }
            }
        } else {
            input_mt_slot(input_dev, i);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
            if ((fhp_data->ts_data->log_level >= 1) && events[i].valid)
                FTS_DEBUG("[B]P%d UP!", i);
        }
    }

    if (ioctl_touch_event.touch_num) {
        input_report_key(input_dev, BTN_TOUCH, 1);
    } else {
        if (fhp_data->ts_data->log_level >= 1) FTS_DEBUG("[B]Points All Up!");
        input_report_key(input_dev, BTN_TOUCH, 0);
    }

    input_sync(input_dev);
    mutex_unlock(&fhp_data->ts_data->report_mutex);
    return 0;
}

#if FTS_PEN_EN
static long fhp_input_report_pen(struct fhp_core_data *fhp_data, unsigned long arg)
{
    struct fhp_input_ioctl_pen_data ioctl_pen_event;
    struct pen_event *pevt = &ioctl_pen_event.events;

    if (!fhp_data || !fhp_data->ts_data) {
        FTS_ERROR("fhp_data/ts_data is null");
        return -EINVAL;
    }

    if (copy_from_user(&ioctl_pen_event, (void *)arg, sizeof(struct fhp_input_ioctl_pen_data))) {
        FTS_ERROR("copy input pen report data from userspace fail");
        return -EFAULT;
    }

    fts_input_report_pen(fhp_data->ts_data, pevt);
    return 0;
}
#endif

static int fhp_input_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int fhp_input_close(struct inode *inode, struct file *file)
{
    return 0;
}

static unsigned int fhp_input_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    struct fhp_core_data *fhp_data = container_of(filp->private_data, struct fhp_core_data, fhpi_dev);

    if (!fhp_data) {
        FTS_ERROR("fhp_data is null");
        return -ENODATA;
    }

    poll_wait(filp, &fhp_data->fhpi_waitq, wait);
    FTS_INFO("fhpi wait flag:%d", fhp_data->fhpi_waitq_flag);
    mutex_lock(&fhp_data->fhpi_mutex);
    if (fhp_data->fhpi_waitq_flag == FHP_STATUS_WAITQ_WAKEUP) {
        mask |= POLLIN | POLLRDNORM;
    }
    fhp_data->fhpi_waitq_flag = FHP_STATUS_WAITQ_WAIT_POLL;
    mutex_unlock(&fhp_data->fhpi_mutex);

    return mask;
}

static long fhp_input_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct fhp_core_data *fhp_data = container_of(filp->private_data, struct fhp_core_data, fhpi_dev);

    if (!fhp_data) {
        FTS_ERROR("fhp_data is null");
        return -ENODATA;
    }

    switch (cmd) {
    case FHP_INPUT_IOCTL_REPORT:
        ret = fhp_input_report(fhp_data, arg);
        break;
    case FHP_INPUT_IOCTL_GET_CFG:
        ret = fhp_input_get_cfg(fhp_data, arg);
        break;
    case FHP_INPUT_IOCTL_REPORT_TOUCH:
        ret = fhp_input_report_touch(fhp_data, arg);
        break;
    case FHP_INPUT_IOCTL_REPORT_FOD:
        ret = fhp_input_report_fod(fhp_data, arg);
        break;
    case FHP_INPUT_IOCTL_REPORT_PALM:
        ret = fhp_input_report_palm(fhp_data, arg);
        break;
    case FHP_INPUT_IOCTL_SET_ALGO_VERSION:
        ret = fhp_input_set_algo_version(fhp_data, arg);
        break;
    case FHP_INPUT_IOCTL_REPORT_PEN:
#if FTS_PEN_EN
        ret = fhp_input_report_pen(fhp_data, arg);
#endif
        break;
    case FHP_INPUT_IOCTL_RELEASE_ALL:
        fts_release_all_finger();
        break;
    case FHP_INPUT_IOCTL_GET_FIRMWARE_STATE:
        fhp_input_get_firmware_mode(fhp_data, arg);
        break;
    default:
        FTS_ERROR("unkown ioctl cmd:0x%x,type:%x,nr:%d", (int)cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
        ret = -EINVAL;
        break;
    }

    return ret;
}

/*USD: add for transfer drv status by guoxin.zhu 20250307 start*/
void fhp_status_switch_trigger(u32 switch_trigger)
{
    struct fhp_status *status;

    if (switch_trigger >= FHP_STATUS_MAX || fts_fhp_data == NULL || fts_fhp_data->drv_state == NULL) {
        FTS_ERROR("Invalid value, return!");
        return;
    }

    mutex_lock(&fts_fhp_data->fhpi_mutex);

    status = fts_fhp_data->drv_state;

    switch (g_status_map[switch_trigger]) {
        case FHP_SUSPEND_TYPE:
            if (FHP_RESUME_MODE == switch_trigger) {
                if (status->fhp_suspend_status != FHP_RESUME_MODE) {
                    fts_write_reg(0x9e, 0x41);
                }
                fts_fhp_data->fhpi_state_rescend = 0;
                fts_fhp_data->last_compensation_jiffies = 0;
                FTS_INFO("Switch to THP mode");
            } else if (FHP_GESTURE_MODE) {
                fts_write_reg(0x9e, 0x00);
                FTS_INFO("Switch to MCU mode");
            } else if (FHP_SLEEP_MODE) {
                FTS_INFO("Switch to SLEEP mode");
            }
            status->fhp_suspend_status = switch_trigger;
            break;

        case FHP_FOD_TYPE:
            status->fhp_fod_status = switch_trigger;
            break;

        case FHP_ROTATION_TYPE:
            status->fhp_rotation_status = switch_trigger;
            break;

        case FHP_SCENE_TYPE:
            status->fhp_scene_mode = switch_trigger;
            break;

        case FHP_POCKET_TYPE:
            status->fhp_pocket_status = switch_trigger;
            break;

        case FHP_KEYPAD_TYPE:
            status->fhp_keypad_status = switch_trigger;
            break;

        default:
            FTS_ERROR("Unknown type:%d, trigger:%d", g_status_map[switch_trigger], switch_trigger);
            mutex_unlock(&fts_fhp_data->fhpi_mutex);
            return;
    }

    FTS_INFO("trigger %d, fhpi waitq flag %d", switch_trigger, fts_fhp_data->fhpi_waitq_flag);
    if (fts_fhp_data->fhpi_waitq_flag != FHP_STATUS_WAITQ_WAKEUP) {
        fts_fhp_data->fhpi_waitq_flag = FHP_STATUS_WAITQ_WAKEUP;
        mutex_unlock(&fts_fhp_data->fhpi_mutex);
        wake_up_interruptible(&fts_fhp_data->fhpi_waitq);
    } else {
        mutex_unlock(&fts_fhp_data->fhpi_mutex);
    }
}
/*USD: add for transfer drv status by guoxin.zhu 20250307 end*/

/*
 * On certain platforms, it is necessary to initialize the open function in order to initialize the private_data of
 * the current device. Otherwise, there may be a situation where using container_of to retrieve the device fails.
 */
static struct file_operations fhp_input_fops = {
    .open = fhp_input_open,
    .release = fhp_input_close,
    .unlocked_ioctl = fhp_input_ioctl,
    .poll = fhp_input_poll,
};

static int fhp_input_miscdev_init(struct fhp_core_data *fhp_data)
{
    int ret = 0;

    fhp_data->fhpi_dev.minor = MISC_DYNAMIC_MINOR;
    fhp_data->fhpi_dev.name = FTS_FHPI_DEV_NAME;
    fhp_data->fhpi_dev.fops = &fhp_input_fops;
    ret = misc_register(&fhp_data->fhpi_dev);
    if (ret) {
        FTS_ERROR("misc_register(input misc device) fail");
        return ret;
    }

    FTS_INFO("misc_register(input misc device) success");
    return 0;
}

static int fhp_get_frame(struct fhp_core_data *fhp_data)
{
    int ret = 0;
    u8 cmd = FHP_CMD_GET_FRAME;

    if (!fhp_data || !fhp_data->fhp_touch_buf) {
        FTS_ERROR("fhp_data/fhp_touch_buf is null");
        return -EINVAL;
    }

    if ((fhp_data->fhp_touch_size == 0) || (fhp_data->fhp_touch_size > MAX_CHIP_FRAME)) {
        FTS_ERROR("fhp_touch_size(%d) is invalid", fhp_data->fhp_touch_size);
        return -EINVAL;
    }

    if (fhp_data->q.elem_size != fhp_data->fhp_touch_size + sizeof(struct fhp_frame)) {
        FTS_ERROR("fhp touch size(%u,%d,%lu) is invalid", fhp_data->fhp_touch_size, fhp_data->q.elem_size,
                  sizeof(struct fhp_frame));
        return -EINVAL;
    }

    ret = fts_read(&cmd, 1, fhp_data->fhp_touch_buf, fhp_data->fhp_touch_size);
    if (ret < 0) {
        FTS_ERROR("read fhp frame data failed,ret=%d", ret);
        return ret;
    }

    ret = fhpq_enqueue(&fhp_data->q, fhp_data->fhp_touch_buf, fhp_get_timestamp());
    if (ret == 0 && g_fhp_drv_state.fhp_suspend_status < g_fhp_hal_state.fhp_suspend_status \
            && fhp_data->fhpi_state_rescend < 3) {

        if (time_after(jiffies, fhp_data->last_compensation_jiffies + MIN_COMPENSATION_INTERVAL)) {
 
            mutex_lock(&fhp_data->fhpi_mutex);
            FTS_INFO("State mismatch detected! Kernel: %d, HAL: %d.",
                g_fhp_drv_state.fhp_suspend_status,
                g_fhp_hal_state.fhp_suspend_status);

            // 唤醒等待队列并更新状态
            fts_fhp_data->fhpi_waitq_flag = FHP_STATUS_WAITQ_WAKEUP;
            wake_up_interruptible(&fts_fhp_data->fhpi_waitq);
            fhp_data->fhpi_state_rescend++;
            fhp_data->last_compensation_jiffies = jiffies;
            mutex_unlock(&fhp_data->fhpi_mutex);
        }
    }

    if ((fhp_data->frame_waitq_flag == FHP_FRAME_WAITQ_WAIT)
        || (fhp_data->frame_waitq_flag == FHP_FRAME_WAITQ_WAIT_POLL)) {
        fhp_data->frame_waitq_flag = FHP_FRAME_WAITQ_WAKEUP;
        wake_up_interruptible(&fhp_data->frame_waitq);
    }

    return 0;
}

int fts_fhp_irq_handler(struct fts_ts_data *ts_data)
{
    int ret = 0;
    struct fhp_core_data *fhp_data = fts_fhp_data;

    if (!ts_data || !fhp_data) {
        FTS_ERROR("ts_data/fhp_data is null");
        return -EINVAL;
    }

    /* TODO:Other function  FOD,GESTURE */
#if FTS_FOD_EN
    /* in mcu mode, read fod */
    if (ts_data->suspended && ts_data->fod_mode) {
        if (fts_fod_readdata(ts_data) == FTS_RETVAL_IGNORE_TOUCHES) {
            FTS_ERROR("FTS_RETVAL_IGNORE_TOUCHES");
            return 0;
        }
    }
#endif

    if (ts_data->suspended && ts_data->gesture_support) {
        fts_gesture_readdata(ts_data, NULL);
        return 0;
    }

    ret = fhp_get_frame(fhp_data);
    if (ret < 0) {
        FTS_ERROR("get frame failed");
        return ret;
    }

    return 0;
}

int fts_fhp_init(struct fts_ts_data *ts_data)
{
    int ret = 0;
    struct fhp_core_data *fhp_data = NULL;

    FTS_FUNC_ENTER();
    fhp_data = kzalloc(sizeof(struct fhp_core_data), GFP_KERNEL);
    if (!fhp_data) {
        FTS_ERROR("allocate memory for fhp_data failed");
        return -ENOMEM;
    }

    fts_fhp_data = fhp_data;
    fhp_data->ts_data = ts_data;
    /*USD:modify for poweroff ke by guoxin.zhu 20260611 start*/
    fhp_data->q.buffer = NULL;
    fhp_data->fhp_touch_buf = NULL;
    fhp_data->drv_state = &g_fhp_drv_state;
    fhp_data->hal_state = &g_fhp_hal_state;
    /*USD:modify for poweroff ke by guoxin.zhu 20260611 end*/
    fhp_data->fhp_dev_open_cnt = 0;
    fhp_data->frame_waitq_flag = FHP_FRAME_WAITQ_DEFAULT;
    fhp_data->fhpi_waitq_flag = FHP_STATUS_WAITQ_DEFAULT;
    init_waitqueue_head(&fhp_data->frame_waitq);
    init_waitqueue_head(&fhp_data->fhpi_waitq);
    mutex_init(&fhp_data->q.mutexq);
    mutex_init(&fhp_data->fhpi_mutex);
    memset(&g_fhp_hal_state, 0xff, sizeof(struct fhp_status));

    /*register /dev/fhp_ft*/
    ret = fhp_miscdev_init(fhp_data);
    if (ret) {
        FTS_ERROR("fhp_dev miscdev register fail");
        goto err_miscdev_init;
    }

    ret = fhp_input_miscdev_init(fhp_data);
    if (ret) {
        FTS_ERROR("input misc device register fail");
        goto err_input_miscdev_init;
    }

    FTS_FUNC_EXIT();
    return 0;

err_input_miscdev_init:
    misc_deregister(&fhp_data->fhp_dev);
err_miscdev_init:
    kfree(fhp_data);
    fhp_data = NULL;
    return ret;
}

void fts_fhp_exit(struct fts_ts_data *ts_data)
{
    struct fhp_core_data *fhp_data = fts_fhp_data;
    FTS_FUNC_ENTER();
    if (fhp_data) {
        misc_deregister(&fhp_data->fhpi_dev);
        misc_deregister(&fhp_data->fhp_dev);
        if (fhp_data->q.buffer) {
            vfree(fhp_data->q.buffer);
            fhp_data->q.buffer = NULL;
        }

        /*USD:modify for devm_kmalloc auto free by guoxin.zhu 20260611 start*/
        //if (fhp_data->fhp_touch_buf) {
        //    kfree(fhp_data->fhp_touch_buf);
        //    fhp_data->fhp_touch_buf = NULL;
        //}
        /*USD:modify for devm_kmalloc auto free by guoxin.zhu 20260611 end*/
        kfree(fhp_data);
        fhp_data = NULL;

    }
    FTS_FUNC_EXIT();
}
