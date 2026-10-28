#ifndef __SF_CTRL_API_H__
#define __SF_CTRL_API_H__

#include "linux/version.h"
#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#include <linux/miscdevice.h>
#include <linux/workqueue.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#else
#include <linux/fb.h>
#include <linux/notifier.h>
#endif

#include <linux/regulator/consumer.h>

#define SF_BEANPOD_COMPATIBLE_V1 0
/* int event. */
#define SF_INT_EVENT_NAME "SPI_STATE=finger"
/* Device node. */
#define SF_CTL_DEV_NAME   "/dev/sunwave_fp"
/* Max driver version buffer length. */
#define SF_DRV_VERSION_LEN 32

#define SF_MAX_VER_INFO_LEN 32

#define SF_INT_TRIG_HIGH 0

#include "tran_fp_common_data_def.h"

typedef struct {
    char tee_solution[SF_MAX_VER_INFO_LEN];
    char ca_version  [SF_MAX_VER_INFO_LEN * 2];
    char ta_version  [SF_MAX_VER_INFO_LEN * 2];
    char algorithm   [SF_MAX_VER_INFO_LEN];
    char algo_nav    [SF_MAX_VER_INFO_LEN];
    char driver      [SF_MAX_VER_INFO_LEN];
    char firmware    [SF_MAX_VER_INFO_LEN];
    char sunwave_id  [SF_MAX_VER_INFO_LEN];
    char vendor_id   [SF_MAX_VER_INFO_LEN];
} __attribute__((__packed__)) sf_version_info_t;

/* Magic code for IOCTL-subsystem, 's'(0x73) means 'Sunwave'. */
#define SF_IOC_MAGIC 's'

/* Allocate/Release driver resource (GPIO/SPI etc.). */
#define SF_IOC_INIT_DRIVER      _IO(SF_IOC_MAGIC, 0x00)
#define SF_IOC_DEINIT_DRIVER    _IO(SF_IOC_MAGIC, 0x01)

/* HW reset the fingerprint module. */
#define SF_IOC_RESET_DEVICE     _IO(SF_IOC_MAGIC, 0x02)

/* Low-level IRQ control. */
#define SF_IOC_ENABLE_IRQ       _IO(SF_IOC_MAGIC, 0x03)
#define SF_IOC_DISABLE_IRQ      _IO(SF_IOC_MAGIC, 0x04)
#define SF_IOC_REQUEST_IRQ      _IO(SF_IOC_MAGIC, 0x21)

/* SPI bus clock control, for power-saving purpose. */
#define SF_IOC_ENABLE_SPI_CLK   _IO(SF_IOC_MAGIC, 0x05)
#define SF_IOC_DISABLE_SPI_CLK  _IO(SF_IOC_MAGIC, 0x06)

/* Fingerprint module power control. */
#define SF_IOC_ENABLE_POWER     _IO(SF_IOC_MAGIC, 0x07)
#define SF_IOC_DISABLE_POWER    _IO(SF_IOC_MAGIC, 0x08)

/* Androind system-wide key event, for navigation purpose. */
#define SF_IOC_REPORT_KEY_EVENT _IOW(SF_IOC_MAGIC, 0x09, struct tran_key_nav *)

/* Sync 'sf_driver_config_t', the driver configuration. */
#define SF_IOC_SYNC_CONFIG      _IOWR(SF_IOC_MAGIC, 0x0a, void *)

/* SPI speed related. */
#define SF_IOC_SPI_SPEED        _IO(SF_IOC_MAGIC, 0x0b)

/* patform attribute */
#define SF_IOC_ATTRIBUTE        _IO(SF_IOC_MAGIC, 0x0d)

/* Query the driver version string. */
#define SF_IOC_GET_VERSION      _IOR(SF_IOC_MAGIC, 0x20, const char *)

/* sunwave hal lib version info set&get. */
#define SF_IOC_SET_LIB_VERSION  _IO(SF_IOC_MAGIC, 0x30)
#define SF_IOC_GET_LIB_VERSION  _IO(SF_IOC_MAGIC, 0x31)
#define SF_IOC_SET_SPI_BUF_SIZE _IO(SF_IOC_MAGIC, 0x40)
#define SF_IOC_SET_RESET_OUTPUT _IO(SF_IOC_MAGIC, 0x50)

/* REE emulate TEE*/
#define SPI_IOC_MAGIC           'k'
/* Read / Write SPI device default max speed hz */
#define SPI_IOC_RD_MAX_SPEED_HZ     _IOR(SPI_IOC_MAGIC, 4, __u32)
#define SPI_IOC_WR_MAX_SPEED_HZ     _IOW(SPI_IOC_MAGIC, 4, __u32)
#define SPI_IOC_RST                 _IO(SPI_IOC_MAGIC, 5)
#define SUNWAVE_IOC_ATTRIBUTE       _IOW(SPI_IOC_MAGIC, 17, __u32)
void sf_ctl_device_event(struct work_struct *ws);
#endif /* __SF_CTRL_API_H__ */
