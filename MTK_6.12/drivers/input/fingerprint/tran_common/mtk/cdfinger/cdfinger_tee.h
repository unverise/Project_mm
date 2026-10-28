#ifndef __CDFINGER_TEE_H__
#define __CDFINGER_TEE_H__

#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#else
#include <linux/fb.h>
#include <linux/notifier.h>
#endif

#include "tran_fp_common_data_def.h"

#define KEY_INTERRUPT KEY_F28

#define CDFINGER_IOCTL_MAGIC_NO          0xFB
#define CDFINGER_INIT                    _IOW(CDFINGER_IOCTL_MAGIC_NO, 0, uint8_t)
#define CDFINGER_GETIMAGE                _IOW(CDFINGER_IOCTL_MAGIC_NO, 1, uint8_t)
#define CDFINGER_INITERRUPT_MODE         _IOW(CDFINGER_IOCTL_MAGIC_NO, 2, uint8_t)
#define CDFINGER_INITERRUPT_KEYMODE      _IOW(CDFINGER_IOCTL_MAGIC_NO, 3, uint8_t)
#define CDFINGER_INITERRUPT_FINGERUPMODE _IOW(CDFINGER_IOCTL_MAGIC_NO, 4, uint8_t)
#define CDFINGER_RELEASE_WAKELOCK        _IO(CDFINGER_IOCTL_MAGIC_NO, 5)
#define CDFINGER_CHECK_INTERRUPT         _IO(CDFINGER_IOCTL_MAGIC_NO, 6)
#define CDFINGER_SET_SPI_SPEED           _IOW(CDFINGER_IOCTL_MAGIC_NO, 7, uint8_t)
#define CDFINGER_REPORT_KEY_LEGACY       _IOW(CDFINGER_IOCTL_MAGIC_NO, 10, uint8_t)
#define CDFINGER_REPORT_KEY              _IOW(CDFINGER_IOCTL_MAGIC_NO, 19, tran_key_nav_t)
#define CDFINGER_POWERDOWN               _IO(CDFINGER_IOCTL_MAGIC_NO, 11)
#define CDFINGER_ENABLE_IRQ              _IO(CDFINGER_IOCTL_MAGIC_NO, 12)
#define CDFINGER_DISABLE_IRQ             _IO(CDFINGER_IOCTL_MAGIC_NO, 13)
#define CDFINGER_HW_RESET                _IOW(CDFINGER_IOCTL_MAGIC_NO, 14, uint8_t)
#define CDFINGER_GET_STATUS              _IO(CDFINGER_IOCTL_MAGIC_NO, 15)
#define CDFINGER_SPI_CLK                 _IOW(CDFINGER_IOCTL_MAGIC_NO, 16, uint8_t)
#define CDFINGER_WAKE_LOCK               _IOW(CDFINGER_IOCTL_MAGIC_NO,26,uint8_t)
#define CDFINGER_ENABLE_CLK              _IOW(CDFINGER_IOCTL_MAGIC_NO, 30, uint8_t)
#define CDFINGER_POLL_TRIGGER            _IO(CDFINGER_IOCTL_MAGIC_NO,31)
#define CDFINGER_NEW_KEYMODE             _IOW(CDFINGER_IOCTL_MAGIC_NO, 37, uint8_t)

int cdfinger_mode_init(struct tran_fp_data *tran_fp_datap, uint8_t arg, enum work_mode mode);
void cdfinger_release_wakelock(struct tran_fp_data *tran_fp_datap);
void cdfinger_wake_lock(struct tran_fp_data *tran_fp_datap, int arg);
int cdfinger_report_key_legacy(struct tran_fp_data *tran_fp_datap, uint8_t arg);
int cdf_key_nav_adjust_type_value(tran_key_nav_event_t *key_nav_p, unsigned int cmd);
int cdfinger_customize_input_init(struct tran_fp_data *tran_fp_datap);
int cdfinger_thread_func(void *arg);
//void int_timer_handle(unsigned long arg); //kernel-4.14 use this api, not fits for kernel-4.19
void int_timer_handle(struct timer_list *tlp);
#endif    /* __CDFINGER_TEE_H__ */
