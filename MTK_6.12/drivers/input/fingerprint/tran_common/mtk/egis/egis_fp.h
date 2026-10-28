#ifndef __EGIS_FP_H__
#define __EGIS_FP_H__

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
#include "tran_fp_common_data_def.h"

#define ET721

#define EGIS_DEV_NAME "esfp0"
#define EGIS_SPI_DEV_NAME "esfp-spi"
#define EGIS_CHRD_DRIVER_NAME "egis_fp"

/* ------------------------- Opcode -------------------------------------*/
#define EGIS_FP_REGISTER_READ 0x01
#define EGIS_FP_REGISTER_WRITE 0x02
#define EGIS_FP_GET_ONE_IMG 0x03
#define EGIS_FP_SENSOR_RESET 0x04
#define EGIS_FP_POWER_ONOFF 0x05
#define EGIS_FP_SET_SPI_CLOCK 0x06
#define EGIS_FP_RESET_SET 0x07
#define EGIS_FP_WAKELOCK_ENABLE 0x08
#define EGIS_FP_WAKELOCK_DISABLE 0x09
#define EGIS_FP_GET_RESOURCE 0x11
#define EGIS_FP_FREE_RESOURCE 0x12

#define EGIS_GET_IO_STUS 0x21
#define EGIS_SEND_NAVI_EVENT 0x31
/* trigger signal initial routine*/
#define EGIS_INT_TRIGGER_INIT 0xa4
/* trigger signal close routine*/
#define EGIS_INT_TRIGGER_CLOSE 0xa5
/* read trigger status*/
#define EGIS_INT_TRIGGER_READ 0xa6
/* polling trigger status*/
#define EGIS_INT_TRIGGER_POLLING 0xa7
/* polling abort*/
#define EGIS_INT_TRIGGER_ABORT 0xa8

#define EGIS_FP_FREE_GPIO 0xaf
#define EGIS_FP_SPICLK_ENABLE 0xaa
#define EGIS_FP_SPICLK_DISABLE 0xab
#define EGIS_DELETE_DEVICE_NODE 0xac

#define EGIS_FP_SPIPIN_SETTING 0xad
#define EGIS_FP_SPIPIN_PULLLOW 0xae

#define EGIS_FP_POWERSETUP 0xb0
#define EGIS_FP_WAKELOCK_TIMEOUT_ENABLE 0xb1
#define EGIS_FP_WAKELOCK_TIMEOUT_DISABLE 0xb2
#define EGIS_GET_SCREEN_ONOFF 0xb3

#define DRDY_IRQ_ENABLE 1
#define DRDY_IRQ_DISABLE 0

#define EDGE_TRIGGER_FALLING 0x0
#define EDGE_TRIGGER_RISING 0x1
#define LEVEL_TRIGGER_LOW 0x2
#define LEVEL_TRIGGER_HIGH 0x3
#define WAKE_HOLD_TIME 2000 //ms

#define SPI_DEFAULT_SPEED (1000000 * 20) //20M

#define Tpwr_off_delay 3
#define Tpwr_on_delay 3
#define Rst_off_delay 3
#define Rst_on_delay 12

struct egisfp_ioctl_cmd_t
{
	uint32_t int_mode;
	uint32_t detect_period;
	uint32_t detect_threshold;
	uint32_t power_on;
};

void egisfp_interrupt_timer_call(struct timer_list *t);
int egisfp_interrupt_init(struct tran_fp_data *tran_fp_datap, int int_mode, int detect_period, int detect_threshold);
int egisfp_interrupt_free(struct tran_fp_data *tran_fp_datap);
int egis_customize_input_init(struct tran_fp_data *tran_fp_datap);
int egf_key_nav_adjust_type_value(tran_key_nav_event_t *key_nav_p, unsigned int cmd);
#endif
