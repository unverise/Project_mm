#ifndef __FOCALTECH_TEE_H__
#define __FOCALTECH_TEE_H__

#include <linux/stddef.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/ioctl.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/string.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#else
#include <linux/fb.h>
#include <linux/notifier.h>
#endif

#include "tran_fp_common_data_def.h"

#define FF_DRV_VERSION_LEN 32
#define FF_DRV_VERSION "v2.1.1"
#define FF_DRV_NAME "focaltech_fp"

typedef struct {
    unsigned int code;
    int value;
} ff_key_event_t;

typedef struct {
    /* Using asynchronous notification mechanism instead of NETLINK. */
    bool enable_fasync;

    /* Gesture(Key emulation & Navigation) key codes. */
    int32_t keycode_nav_left;
    int32_t keycode_nav_right;
    int32_t keycode_nav_up;
    int32_t keycode_nav_down;
    int32_t keycode_double_click;
    int32_t keycode_click;
    int32_t keycode_long_press;
    int32_t keycode_simulation;

    /* For '/dev/spidevB.C' of REE-Emulation. */
    bool enable_spidev;
    int32_t spidev_bus;
    int32_t spidev_c_s;

    /* For obsolete driver that doesn't support device tree. */
    int32_t gpio_mosi_pin;
    int32_t gpio_miso_pin;
    int32_t gpio_ck_pin;
    int32_t gpio_cs_pin;
    int32_t gpio_rst_pin;
    int32_t gpio_int_pin;
    int32_t gpio_power_pin;
    int32_t gpio_iovcc_pin;

    /* Logging driver to logcat through uevent mechanism. */
    int32_t log_level;
    bool logcat_driver;
} ff_driver_config_t;
/* Magic code for IOCTL-subsystem, 'f'(0x66) means '[F]ocalTech'. */
#define FF_IOC_MAGIC 'f'

/* Allocate/Release driver resource (GPIO/SPI etc.). */
#define FF_IOC_INIT_DRIVER      _IO(FF_IOC_MAGIC, 0x00)
#define FF_IOC_FREE_DRIVER      _IO(FF_IOC_MAGIC, 0x01)

/* HW reset the fingerprint module. */
#define FF_IOC_RESET_DEVICE     _IO(FF_IOC_MAGIC, 0x02)
#define FF_IOC_RESET_DEVICE_HL  _IOW(FF_IOC_MAGIC, 0x02, uint32_t)

/* Low-level IRQ control. */
#define FF_IOC_ENABLE_IRQ       _IO(FF_IOC_MAGIC, 0x03)
#define FF_IOC_DISABLE_IRQ      _IO(FF_IOC_MAGIC, 0x04)

/* SPI bus clock control, for power-saving purpose. */
#define FF_IOC_ENABLE_SPI_CLK   _IO(FF_IOC_MAGIC, 0x05)
#define FF_IOC_DISABLE_SPI_CLK  _IO(FF_IOC_MAGIC, 0x06)

/* Fingerprint module power control. */
#define FF_IOC_ENABLE_POWER     _IO(FF_IOC_MAGIC, 0x07)
#define FF_IOC_DISABLE_POWER    _IO(FF_IOC_MAGIC, 0x08)

/* Androind system-wide key event, for navigation purpose. */
#define FF_IOC_REPORT_KEY_EVENT _IOW(FF_IOC_MAGIC, 0x09, ff_key_event_t)

/* Sync 'ff_driver_config_t', the driver configuration. */
#define FF_IOC_SYNC_CONFIG     _IOWR(FF_IOC_MAGIC, 0x0a, ff_driver_config_t)

/* Query the driver version string. */
#define FF_IOC_GET_VERSION      _IOR(FF_IOC_MAGIC, 0x0b, const char)

const char *ff_ctl_get_version(void);
void ff_work_handler(struct work_struct* data);
void ff_ctl_device_event(struct work_struct *ws);
irqreturn_t ff_ctl_device_irq(int irq, void *dev_id);
int focaltech_customize_input_init(struct tran_fp_data *tran_fp_datap);
#ifndef CONFIG_HAS_EARLYSUSPEND
int ftf_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#endif

#define LOG_TAG_FTF "focaltech"
typedef enum {
    FF_LOG_LEVEL_ALL = 0,
    FF_LOG_LEVEL_VBS = 1, /* Verbose */
    FF_LOG_LEVEL_DBG = 2, /* Debug   */
    FF_LOG_LEVEL_INF = 3, /* Info    */
    FF_LOG_LEVEL_WRN = 4, /* Warning */
    FF_LOG_LEVEL_ERR = 5, /* Error   */
} ff_log_level_t;
#ifndef __FF_EARLY_LOG_LEVEL
#define __FF_EARLY_LOG_LEVEL FF_LOG_LEVEL_ALL
#endif

#define FF_LOGV(...)                                               \
    do {                                                           \
        if (g_log_level <= FF_LOG_LEVEL_VBS) {                     \
            ff_log_printf(FF_LOG_LEVEL_VBS, LOG_TAG_FTF, __VA_ARGS__); \
        };                                                         \
    } while (0)

#define FF_LOGD(...)                                               \
    do {                                                           \
        if (g_log_level <= FF_LOG_LEVEL_DBG) {                     \
            ff_log_printf(FF_LOG_LEVEL_DBG, LOG_TAG_FTF, __VA_ARGS__); \
        };                                                         \
    } while (0)

#define FF_LOGI(...)                                               \
    do {                                                           \
        if (g_log_level <= FF_LOG_LEVEL_INF) {                     \
            ff_log_printf(FF_LOG_LEVEL_INF, LOG_TAG_FTF, __VA_ARGS__); \
        };                                                         \
    } while (0)

#define FF_LOGW(...)                                               \
    do {                                                           \
        if (g_log_level <= FF_LOG_LEVEL_WRN) {                     \
            ff_log_printf(FF_LOG_LEVEL_WRN, LOG_TAG_FTF, __VA_ARGS__); \
        };                                                         \
    } while (0)

#define FF_LOGE(format, ...)                                       \
    do {                                                           \
        if (g_log_level <= FF_LOG_LEVEL_ERR) {                     \
            const char* const __fname__ = strrchr(__FILE__, '/') ? \
                    strrchr(__FILE__, '/') + 1 : __FILE__;         \
            ff_log_printf(FF_LOG_LEVEL_ERR, LOG_TAG_FTF,               \
                    "error at %s[%s:%d]: " format, __FUNCTION__,   \
                    __fname__, __LINE__, ##__VA_ARGS__);           \
        };                                                         \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif
int ff_log_printf(ff_log_level_t level, const char *tag, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif    /* __FOCALTECH_TEE_H__*/
