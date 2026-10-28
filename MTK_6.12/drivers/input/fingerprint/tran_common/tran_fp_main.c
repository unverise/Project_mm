/**
 * Copyright (C) 2015 Transsion Inc
 * Author fan.peng
 * This file is used to implement the main or fundamental functions for fingerprint device driver
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/compat.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/seq_file.h>
#include <linux/of_irq.h>
#include <linux/kobject.h>
#include <linux/debugfs.h>
#include <../kernel/power/power.h>
#include <linux/jiffies.h>
#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/cpumask.h>
#include <linux/kernel_stat.h>
#include <linux/bug.h>
#include <linux/param.h>
#include <linux/ioport.h>
#include <linux/irqreturn.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/cpu.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <uapi/linux/sched/types.h>

#define CONFIG_SPI_MT65XX_KERNEL510 y

#if !defined(CONFIG_SPI_MT65XX_KERNEL510)
#include <mt_spi.h>
#include <mt_spi_hal.h>
#endif

#include "tran_fp_main.h"
#include "mc_linux_api.h"

#include <linux/timex.h>
#include <linux/timer.h>
#include "tranlog.h"
//#define TRAN_INIT_INTER_CHIP_RESET_DELAY 2000
//#define TRAN_INIT_POST_CHIP_RESET_DELAY 3000
#define N_SPI_MINORS 32
#define TRAN_FP_STATIC_MINOR 228

char android_version = 0;
struct tran_fp_data	* g_tran_fp_datap;
int g_tran_fp_req_irq_times = 0;
ff_driver_config_t driver_config;
ff_driver_config_t *ff_g_config = NULL;
ff_log_level_t g_log_level = __FF_EARLY_LOG_LEVEL;

extern struct attribute_group tran_fp_attribute_group;
extern u8 g_fp_vendor_id;
extern sf_version_info_t sf_hw_ver;
extern struct fp_dev_init_t silfp_dev_init_d;

//extern void app_get_fingerprint_name(char *name);
extern int tran_proc_init(struct tran_fp_data *tran_fp_datap);
extern int tran_proc_deinit(struct tran_fp_data *tran_fp_dev);
extern const char *sf_ctl_get_version(void);
extern struct fp_dev_init_t silfp_dev_init_d;
extern fp_debug_level_t sil_debug_level;
extern void silfp_pwdn(struct tran_fp_data *tran_fp_datap, u8 flag_avdd);
extern irqreturn_t silfp_irq_handler(int irq, void *dev_id);
extern void tran_netlink_send(struct tran_fp_data *tran_fp_datap, const int cmd);
extern char vendor_name[PROC_VND_ID_LEN];
extern g_fp_name_e g_fp_name_flg;

//extern void set_tee_worker_threads_on_big_core(bool big_core , int afinity_mask);

//extern cpumask_t tee_set_affinity(void);
static unsigned int bufsiz = 64; //(25 * 1024);
static struct spi_board_info spi_desc = {
	.modalias        = "spidev",
	.bus_num         = 0,
	.chip_select     = 0,
	.mode            = SPI_MODE_0,
};

static DECLARE_BITMAP(minors, N_SPI_MINORS);
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
static DECLARE_WAIT_QUEUE_HEAD(tran_fp_poll_waitq);

#ifdef TEE_VERSION_510
extern void set_tee_worker_threads_Schedue(int policy);
static int POLICY_HIGH = SCHED_FIFO ;
static int POLICY_LOW = SCHED_NORMAL ;
#endif


//BSP: add change proc fp to sys fp for HAL compatibility XLJOEI-309 by fan.peng 20200806 start
static struct kobject *fp_sys_kobj;
//BSP: add change proc fp to sys fp for HAL compatibility XLJOEI-309 by fan.peng 20200806 end
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

static long tran_fp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int	err = 0;
	int	retval = 0;
	struct tran_fp_data	*tran_fp_datap = NULL;
	struct TRAN_DEV_SEL_device *tran_fp_dev = NULL;
	PWR_VAL_U power_val;
	struct tran_ioc_chip_info info;
	tran_key_nav_event_t key_nav_event = TRAN_KEY_NAV_NONE;
	tran_key_nav_t tran_key_nav = {TRAN_KEY_NAV_NONE, 0};
	u8 netlink_route = GF_NETLINK_ROUTE;
	struct fp_dev_key_t key;
	u8 need_update = 0;
	unsigned int val = 0;
	unsigned char delay;
	struct spi_device *spi_dev = NULL;
	struct spi_controller *spi_ctl = NULL;
	ff_key_event_t kevent;
	struct egisfp_ioctl_cmd_t data = {0};
	uint32_t power_onoff;
	//    uint32_t g_cmd_sn = 0;
	int i;
	//BSP:modify for reducing the unnecessary log output and logic judgement NFRFP-195 by fan.peng 20210409 start
	/*TRAN_FP_INFO(" (_IO(type,nr) nr= 0x%08x, ..)\n", _IOC_NR(cmd));
	  switch (cmd)
	  {
	  case TRAN_IOC_INIT:
	  case TRAN_IOC_CHIP_INFO:
	  case TRAN_IOC_EXIT:
	  case TRAN_IOC_RESET:
	  case TRAN_IOC_ENABLE_IRQ_1:
	  case TRAN_IOC_DISABLE_IRQ_1:
	  case TRAN_IOC_ENABLE_SPI_CLK_1:
	  case TRAN_IOC_DISABLE_SPI_CLK_1:
	  case TRAN_IOC_ENABLE_POWER_1:
	  case TRAN_IOC_DISABLE_POWER_1:
	  case TRAN_IOC_INPUT_KEY_EVENT:
	  case TRAN_IOC_NAV_EVENT:
	  case TRAN_IOC_ENTER_SLEEP_MODE:
	  case TRAN_IOC_GET_FW_INFO:
	  case TRAN_IOC_REMOVE:
	  if (_IOC_TYPE(cmd) != GF_IOC_MAGIC)
	  {
	  TRAN_FP_INFO(" Goodix ioctl cmd failed!  ======\n");
	  return -ENOTTY;
	  }
	  break;

	  case TRAN_IOC_INIT_DRIVER:
	  case TRAN_IOC_DEINIT_DRIVER:
	  case TRAN_IOC_RESET_DEVICE:
	  case TRAN_IOC_ENABLE_IRQ_2:
	  case TRAN_IOC_DISABLE_IRQ_2:
	  case TRAN_IOC_REQUEST_IRQ:
	  case TRAN_IOC_ENABLE_SPI_CLK_2:
	  case TRAN_IOC_DISABLE_SPI_CLK_2:
	  case TRAN_IOC_ENABLE_POWER_2:
	  case TRAN_IOC_DISABLE_POWER_2:
	  case TRAN_IOC_REPORT_KEY_EVENT:
	  case TRAN_IOC_SYNC_CONFIG:
	  case TRAN_IOC_SPI_SPEED:
	  case TRAN_IOC_ATTRIBUTE_2:
	  case TRAN_IOC_GET_VERSION:
	  case TRAN_IOC_SET_LIB_VERSION:
	  case TRAN_IOC_GET_LIB_VERSION:
	  if (_IOC_TYPE(cmd) != SF_IOC_MAGIC)
	  {
	  TRAN_FP_INFO(" Sunwave ioctl cmd 1 failed!  ======\n");
	  return -ENOTTY;
	  }
	  break;
	  case TRAN_SPI_IOC_RD_MAX_SPEED_HZ:
	  case TRAN_SPI_IOC_WR_MAX_SPEED_HZ:
	  case TRAN_SPI_IOC_RST:
	  case TRAN_IOC_ATTRIBUTE_1:
	  if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
	  {
	  TRAN_FP_INFO(" Sunwave ioctl cmd 2 failed!  ======\n");
	  return -ENOTTY;
	  }
	  break;
	  case TRAN_FPSENSOR_IOC_INIT:
	  case TRAN_FPSENSOR_IOC_EXIT:
	  case TRAN_FPSENSOR_IOC_RESET:
	  case TRAN_FPSENSOR_IOC_ENABLE_IRQ:
	  case TRAN_FPSENSOR_IOC_DISABLE_IRQ:
	  case TRAN_FPSENSOR_IOC_GET_INT_VAL:
	  case TRAN_FPSENSOR_IOC_ENABLE_SPI_CLK:
	  case TRAN_FPSENSOR_IOC_DISABLE_SPI_CLK:
	  case TRAN_FPSENSOR_IOC_ENABLE_POWER:
	  case TRAN_FPSENSOR_IOC_DISABLE_POWER:
	  case TRAN_FPSENSOR_IOC_REMOVE:
	  case TRAN_FPSENSOR_IOC_CANCEL_WAIT:
	  case TRAN_FPSENSOR_IOC_GET_FP_STATUS :
	case TRAN_FPSENSOR_IOC_ENABLE_REPORT_BLANKON:
	case TRAN_FPSENSOR_IOC_UPDATE_DRIVER_SN:
	if (_IOC_TYPE(cmd) != FPSENSOR_IOC_MAGIC)
	{
		TRAN_FP_INFO(" Chipone ioctl cmd failed!  ======\n");
		return -ENOTTY;
	}
	break;
	case TRAN_OXIFP_IOC_RST_SENSOR:
	case TRAN_OXIFP_IOC_CS_HIGH:
	case TRAN_OXIFP_IOC_CS_LOW:
	case TRAN_OXIFP_IOC_POWER_DOWN:
	case TRAN_OXIFP_IOC_POWER_ON:
	case TRAN_OXIFP_IOC_IS_FINGER_ON:
	case TRAN_OXIFP_KEY_REPORT:
	case TRAN_OXIFP_WAKEUP_SYSTEM:
	case TRAN_OXIFP_IOC_ATTRIBUTE:
	case TRAN_OXIFP_SET_VERSION_INFO:
	if (_IOC_TYPE(cmd) != OXIFP_IOC_MAGIC)
	{
		TRAN_FP_INFO(" Oxi ioctl cmd failed!  %s ======\n", __func__);
		return -ENOTTY;
	}
	break;
	case TRAN_SIFP_IOC_RESET:
	case TRAN_SIFP_IOC_ENABLE_IRQ:
	case TRAN_SIFP_IOC_DISABLE_IRQ:
	case TRAN_SIFP_IOC_WAIT_IRQ:
	case TRAN_SIFP_IOC_CLR_IRQ:
	case TRAN_SIFP_IOC_KEY_EVENT:
	case TRAN_SIFP_IOC_INIT:
	case TRAN_SIFP_IOC_DEINIT:
	case TRAN_SIFP_IOC_IRQ_STATUS:
	case TRAN_SIFP_IOC_DEBUG:
	case TRAN_SIFP_IOC_SCR_STATUS:
	case TRAN_SIFP_IOC_GET_VER:
	case TRAN_SIFP_IOC_SET_KMAP:
	case TRAN_SIFP_IOC_ACQ_SPI:
	case TRAN_SIFP_IOC_RLS_SPI:
	case TRAN_SIFP_IOC_PKG_SIZE:
	case TRAN_SIFP_IOC_DBG_LEVEL:
	case TRAN_SIFP_IOC_WAKELOCK:
	case TRAN_SIFP_IOC_PWDN:
	case TRAN_SIFP_IOC_PROC_NODE:
	case TRAN_SIFP_IOC_SET_FEATURE:
	if (_IOC_TYPE(cmd) != SIFP_IOC_MAGIC)
	{
		TRAN_FP_INFO(" Silead ioctl cmd failed!  ======\n");
		return -ENOTTY;
	}
	break;
	case TRAN_CDFINGER_INIT:
	case TRAN_CDFINGER_GETIMAGE:
	case TRAN_CDFINGER_INITERRUPT_MODE:
	case TRAN_CDFINGER_INITERRUPT_KEYMODE:
	case TRAN_CDFINGER_INITERRUPT_FINGERUPMODE:
	case TRAN_CDFINGER_RELEASE_WAKELOCK:
	case TRAN_CDFINGER_CHECK_INTERRUPT:
	case TRAN_CDFINGER_SET_SPI_SPEED:
	case TRAN_CDFINGER_REPORT_KEY_LEGACY:
	case TRAN_CDFINGER_REPORT_KEY:
	case TRAN_CDFINGER_POWERDOWN:
	case TRAN_CDFINGER_ENABLE_IRQ:
	case TRAN_CDFINGER_DISABLE_IRQ:
	case TRAN_CDFINGER_HW_RESET:
	case TRAN_CDFINGER_GET_STATUS:
	case TRAN_CDFINGER_SPI_CLK:
	case TRAN_CDFINGER_WAKE_LOCK:
	case TRAN_CDFINGER_ENABLE_CLK:
	case TRAN_CDFINGER_POLL_TRIGGER:
	case TRAN_CDFINGER_NEW_KEYMODE:
	if (_IOC_TYPE(cmd) != CDFINGER_IOCTL_MAGIC_NO)
	{
		TRAN_FP_INFO(" Cdfinger ioctl cmd failed!  ======\n");
		return -ENOTTY;
	}
	break;
	case TRAN_FF_IOC_INIT_DRIVER:
	case TRAN_FF_IOC_FREE_DRIVER:
	case TRAN_FF_IOC_RESET_DEVICE:
	case TRAN_FF_IOC_ENABLE_IRQ:
	case TRAN_FF_IOC_DISABLE_IRQ:
	case TRAN_FF_IOC_ENABLE_SPI_CLK:
	case TRAN_FF_IOC_DISABLE_SPI_CLK:
	case TRAN_FF_IOC_ENABLE_POWER:
	case TRAN_FF_IOC_DISABLE_POWER:
	case TRAN_FF_IOC_REPORT_KEY_EVENT:
	case TRAN_FF_IOC_SYNC_CONFIG:
	case TRAN_FF_IOC_GET_VERSION:
	if (_IOC_TYPE(cmd) != FF_IOC_MAGIC)
	{
		TRAN_FP_INFO(" Focaltech ioctl cmd failed!  ======\n");
		return -ENOTTY;
	}
	break;
	default:
	TRAN_FP_INFO(" implicit or extra cmd found!  ======\n");
	break;
}
*/
//BSP:modify for reducing the unnecessary log output and logic judgement NFRFP-195 by fan.peng 20210409 end
if (_IOC_DIR(cmd) & _IOC_READ)
	err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
	err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
if (err){
		TRAN_FP_ERROR(" cannot access read! ======\n");
		return -EFAULT;
}
tran_fp_datap = filp->private_data;
if (tran_fp_datap == NULL) {
	TRAN_FP_ERROR(" tran_fp_datap is NULL ! ======\n");
	return -EINVAL;
}
tran_fp_datap->cancel = 0;
spin_lock_irq(&tran_fp_datap->spi_lock);
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
tran_fp_dev = spi_dev_get(tran_fp_datap->tran_fp_dev);
#else
tran_fp_dev = (tran_fp_datap->tran_fp_dev && get_device(&tran_fp_datap->tran_fp_dev->dev)) ? tran_fp_datap->tran_fp_dev : NULL;
#endif
spin_unlock_irq(&tran_fp_datap->spi_lock);
if (tran_fp_dev == NULL) {
	TRAN_FP_ERROR(" no tran_fp_dev! ======\n");
	return -ESHUTDOWN;
}
mutex_lock(&tran_fp_datap->buf_lock);
switch (cmd) {
	case TRAN_IOC_INIT:
		TRAN_FP_INFO(" GF_IOC_INIT gf init======\n");
		TRAN_FP_DEBUG(" Linux Version %s\n", GF_LINUX_VERSION);
		if (copy_to_user((void __user *)arg, (void *)&netlink_route, sizeof(u8))) {
			retval = -EFAULT;
			break;
		}

		if (tran_fp_datap->system_status) {
			TRAN_FP_INFO(" system re-started======\n");
			break;
		}
		retval = request_threaded_irq(tran_fp_datap->irq, NULL, gf_irq,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT, "goodix_fp_irq", tran_fp_datap);
		if (!retval) {
			TRAN_FP_INFO(" irq thread request success!\n");
			g_tran_fp_req_irq_times = 1;
		}
		else {
			TRAN_FP_ERROR(" irq thread request failed, retval=%d\n", retval);
			break;
		}
		retval = enable_irq_wake(tran_fp_datap->irq);
		if (retval < 0 ) {
			TRAN_FP_ERROR(" enable_irq_wake failed , retval = %d \n", retval);
			free_irq(tran_fp_datap->irq, tran_fp_datap);
			break;
		} else {
			TRAN_FP_INFO(" enable_irq_wake successful======\n");
			tran_fp_datap->irq_count = 1;
		}
		tran_fp_datap->disable_irq(tran_fp_datap);

		if (tran_fp_datap->lcm_onoff_registered == false) {
			TRAN_FP_INFO(" register lcm callback");
#if defined(CONFIG_HAS_EARLYSUSPEND)
			register_early_suspend(&tran_fp_datap->early_suspend);
#else
			//BSP: modify  by qing.li 20220706 for kernel-5.10 start
			// fb_register_client(&tran_fp_datap->notifier);
			retval=mtk_disp_notifier_register("FINGERPRINT", &tran_fp_datap->notifier);
			if (retval) {
				TRAN_FP_INFO("mtk_disp_notifier_register failed:%d\n",retval);
			}
			//BSP: modify  by qing.li 20220706 for kernel-5.10 end
#endif
			tran_fp_datap->lcm_onoff_registered = true;
		}

		tran_fp_datap->sig_count = 0;
		tran_fp_datap->system_status = 1;

		TRAN_FP_INFO(" gf init finished======\n");
		break;

	case TRAN_IOC_CHIP_INFO:
		TRAN_FP_INFO(" TRAN_IOC_CHIP_INFO ======\n");
		if (copy_from_user(&info, (struct tran_ioc_chip_info *)arg, sizeof(struct tran_ioc_chip_info))) {
			retval = -EFAULT;
			break;
		}
		g_fp_vendor_id = info.vendor_id;

		TRAN_FP_INFO(" vendor_id 0x%x\n", g_fp_vendor_id);
		TRAN_FP_INFO(" mode 0x%x\n", info.mode);
		TRAN_FP_INFO(" operation 0x%x\n", info.operation);
		break;

	case TRAN_IOC_EXIT:
		TRAN_FP_INFO(" GF_IOC_EXIT ======\n");
		tran_fp_datap->disable_irq(tran_fp_datap);
		if (tran_fp_datap->irq && g_tran_fp_req_irq_times == 1) {
			free_irq(tran_fp_datap->irq, tran_fp_datap);
			g_tran_fp_req_irq_times = 0;
			tran_fp_datap->irq_count = 0;
		}

		if (tran_fp_datap->lcm_onoff_registered == true) {
			TRAN_FP_INFO(" unregister lcm callback");
#ifdef CONFIG_HAS_EARLYSUSPEND
			if (tran_fp_datap->early_suspend.suspend)
				unregister_early_suspend(&tran_fp_datap->early_suspend);
#else
			fb_unregister_client(&tran_fp_datap->notifier);
#endif
			tran_fp_datap->lcm_onoff_registered = false;
		}
		tran_fp_datap->system_status = 0;
		TRAN_FP_INFO(" gf exit finished ======\n");
		break;

	case TRAN_IOC_RESET:
		TRAN_FP_INFO(" gf chip reset command\n");
		tran_fp_datap->reset(tran_fp_datap, tran_fp_datap->inter_delay, tran_fp_datap->post_delay);
		break;

	case TRAN_IOC_ENABLE_IRQ_1:
		TRAN_FP_INFO(" GF_IOC_ENABLE_IRQ ======\n");
		tran_fp_datap->enable_irq(tran_fp_datap);
		break;

	case TRAN_IOC_DISABLE_IRQ_1:
		TRAN_FP_INFO(" GF_IOC_DISABLE_IRQ ======\n");
		tran_fp_datap->disable_irq(tran_fp_datap);
		break;

	case TRAN_IOC_ENABLE_SPI_CLK_1:
		TRAN_FP_INFO(" GF_IOC_ENABLE_SPI_CLK ======\n");
		tran_fp_datap->spi_clk_on(tran_fp_datap);
		break;

	case TRAN_IOC_DISABLE_SPI_CLK_1:
		TRAN_FP_INFO(" GF_IOC_DISABLE_SPI_CLK ======\n");
		tran_fp_datap->spi_clk_off(tran_fp_datap);
		break;

	case TRAN_IOC_ENABLE_POWER_1:
		TRAN_FP_INFO(" GF_IOC_ENABLE_POWER ======\n");
		tran_fp_datap->power_init(tran_fp_datap, true);
		tran_fp_datap->power_onoff(tran_fp_datap, true);
		tran_fp_get_power_val(tran_fp_datap, &power_val);
		tran_fp_datap->reset(tran_fp_datap, tran_fp_datap->inter_delay, tran_fp_datap->post_delay);
		break;

	case TRAN_IOC_DISABLE_POWER_1:
		TRAN_FP_INFO(" GF_IOC_DISABLE_POWER ======\n");
		tran_fp_datap->power_onoff(tran_fp_datap, false);
		tran_fp_get_power_val(tran_fp_datap, &power_val);
		tran_fp_datap->power_init(tran_fp_datap, false);
		break;

	case TRAN_IOC_INPUT_KEY_EVENT:
		TRAN_FP_INFO(" TRAN_IOC_INPUT_KEY_EVENT ======\n");
		if (copy_from_user(&tran_key_nav, (tran_key_nav_t *)arg, sizeof(tran_key_nav_t))) {
			TRAN_FP_ERROR("Failed to copy input key event from user to kernel\n");
			retval = -EFAULT;
			break;
		}
		if ((TRAN_KEY_NAV_POWER == tran_key_nav.key || TRAN_KEY_NAV_CAMERA == tran_key_nav.key) && (tran_key_nav.value == 1)) {
			tran_fp_report_key_nav_event(tran_fp_datap->input, &tran_key_nav);
			tran_key_nav.value = 0;
			tran_fp_report_key_nav_event(tran_fp_datap->input, &tran_key_nav);
			tran_key_nav.value = 1;
		}
		if (TRAN_KEY_NAV_HOME == tran_key_nav.key) {
			gf_key_nav_adjust_type_value(&(tran_key_nav.key), TRAN_IOC_INPUT_KEY_EVENT);
			tran_fp_report_key_nav_event(tran_fp_datap->input, &tran_key_nav);
		}
		break;

	case TRAN_IOC_NAV_EVENT:
		TRAN_FP_INFO(" TRAN_IOC_NAV_EVENT ======\n");
		if (copy_from_user(&key_nav_event, (tran_key_nav_event_t *)arg, sizeof(tran_key_nav_event_t))) {
			TRAN_FP_ERROR("Failed to copy nav event from user to kernel\n");
			retval = -EFAULT;
			break;
		}
		gf_key_nav_adjust_type_value(&key_nav_event, TRAN_IOC_NAV_EVENT);
		tran_key_nav.key = key_nav_event;
		tran_key_nav.value = 1;
		tran_fp_report_key_nav_event(tran_fp_datap->input, &tran_key_nav);
		tran_key_nav.value = 0;
		tran_fp_report_key_nav_event(tran_fp_datap->input, &tran_key_nav);
		break;

	case TRAN_IOC_ENTER_SLEEP_MODE:
		TRAN_FP_INFO(" GF_IOC_ENTER_SLEEP_MODE ======\n");
		break;

	case TRAN_IOC_GET_FW_INFO:
		TRAN_FP_INFO(" GF_IOC_GET_FW_INFO ======\n");
		need_update = tran_fp_datap->need_update;
		TRAN_FP_DEBUG(" firmware info  0x%x\n", need_update);
		if (copy_to_user((void __user *)arg, (void *)&need_update, sizeof(u8))) {
			TRAN_FP_ERROR("Failed to copy data to user\n");
			retval = -EFAULT;
		}
		break;
	case TRAN_IOC_REMOVE:
		TRAN_FP_INFO(" GF_IOC_REMOVE ======\n");
		tran_netlink_destroy(tran_fp_datap);
		mutex_lock(&tran_fp_datap->release_lock);
		if (tran_fp_datap->input == NULL) {
			mutex_unlock(&tran_fp_datap->release_lock);
			break;
		}
		input_unregister_device(tran_fp_datap->input);
		tran_fp_datap->input = NULL;
		mutex_unlock(&tran_fp_datap->release_lock);
		sysfs_remove_group(&tran_fp_datap->tran_fp_dev->dev.kobj, &tran_fp_attribute_group);
		misc_deregister(&tran_fp_datap->miscdev);
		list_del(&tran_fp_datap->device_entry);
		tran_fp_datap->power_onoff(tran_fp_datap, false);
		tran_fp_get_power_val(tran_fp_datap, &power_val);
		tran_fp_datap->power_init(tran_fp_datap, false);
		tran_fp_datap->spi_clk_off(tran_fp_datap);
		mutex_lock(&tran_fp_datap->release_lock);
		if (tran_fp_datap->tran_buffer != NULL) {
			kfree(tran_fp_datap->tran_buffer);
			tran_fp_datap->tran_buffer = NULL;
		}
		mutex_unlock(&tran_fp_datap->release_lock);
		TRAN_DEV_SEL_set_drvdata(tran_fp_datap->tran_fp_dev, NULL);
		tran_fp_datap->tran_fp_dev = NULL;
		mutex_destroy(&tran_fp_datap->buf_lock);
		mutex_destroy(&tran_fp_datap->release_lock);
		TRAN_FP_INFO(" GF_IOC_REMOVE leave======\n");
		break;

	case TRAN_IOC_INIT_DRIVER: {
								   TRAN_FP_INFO(" SF_IOC_INIT ======\n");
								   //tran_fp_datap->gpio_init(tran_fp_datap);
								   break;
							   }

	case TRAN_IOC_DEINIT_DRIVER: {
									 TRAN_FP_INFO(" SF_IOC_DEINIT ======\n");
									 //tran_fp_datap->free_gpio(tran_fp_datap);
									 break;
								 }

	case TRAN_SPI_IOC_RST:
	case TRAN_IOC_RESET_DEVICE: {
									TRAN_FP_INFO(" SF_IOC_RESET ======\n");
									tran_fp_datap->reset(tran_fp_datap, tran_fp_datap->inter_delay, tran_fp_datap->post_delay);
									break;
								}

	case TRAN_IOC_ENABLE_IRQ_2: {
									TRAN_FP_INFO(" SF_IOC_ENABLE_IRQ ======\n");
									// TODO:
									break;
								}

	case TRAN_IOC_DISABLE_IRQ_2: {
									 TRAN_FP_INFO(" SF_IOC_DISABLE_IRQ ======\n");
									 // TODO:
									 break;
								 }

	case TRAN_IOC_REQUEST_IRQ: {
								   TRAN_FP_INFO(" SF_IOC_REQUEST_IRQ ======\n");
								   //tran_fp_datap->irq_init();
								   break;
							   }

	case TRAN_IOC_ENABLE_SPI_CLK_2: {
										TRAN_FP_INFO(" SF_IOC_ENABLE_SPI_CLK ======\n");
										tran_fp_datap->spi_clk_on(tran_fp_datap);
										break;
									}

	case TRAN_IOC_DISABLE_SPI_CLK_2: {
										 TRAN_FP_INFO(" SF_IOC_DISABLE_SPI_CLK ======\n");
										 tran_fp_datap->spi_clk_off(tran_fp_datap);
										 break;
									 }

	case TRAN_IOC_ENABLE_POWER_2: {
									  tran_fp_datap->power_init(tran_fp_datap, true);
									  tran_fp_datap->power_onoff(tran_fp_datap, true);
									  break;
								  }

	case TRAN_IOC_DISABLE_POWER_2: {
									   tran_fp_datap->power_onoff(tran_fp_datap, false);
									   tran_fp_datap->power_init(tran_fp_datap, false);
									   break;
								   }

	case TRAN_IOC_REPORT_KEY_EVENT: {
										TRAN_FP_INFO(" SF_IOC_REPORT_KEY_EVENT ======\n");
										if (copy_from_user(&tran_key_nav, (tran_key_nav_t *)arg, sizeof(tran_key_nav_t))) {
											TRAN_FP_ERROR("copy_from_user(..) failed.\n");
											retval = (-EFAULT);
											break;
										}

										sf_key_nav_adjust_type_value(&(tran_key_nav.key), TRAN_IOC_REPORT_KEY_EVENT);
										retval = tran_fp_report_key_nav_event(tran_fp_datap->input, &tran_key_nav);
										break;
									}

	case TRAN_IOC_SYNC_CONFIG: {
								   // TODO:
								   break;
							   }

	case TRAN_SPI_IOC_WR_MAX_SPEED_HZ:
	case TRAN_IOC_SPI_SPEED: {
								 TRAN_FP_INFO(" SF_IOC_CTRL_SPI_SPEED ======\n");
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifndef CONFIG_SPI_MT65XX_KERNEL510
								 tran_ctl_spi_speed(tran_fp_datap, arg, FIFO_TRANSFER);
#endif
#endif
								 break;
							 }

	case TRAN_SPI_IOC_RD_MAX_SPEED_HZ: {
										   // TODO:
										   break;
									   }

	case TRAN_IOC_ATTRIBUTE_1:
	case TRAN_IOC_ATTRIBUTE_2: {
								   TRAN_FP_INFO(" SF_IOC_ATTRIBUTE ======\n");
								   retval = __put_user(tran_fp_datap->attribute, (__u32 __user *)arg);
								   break;
							   }

	case TRAN_IOC_GET_VERSION: {
								   TRAN_FP_INFO(" SF_IOC_GET_VERSION ======\n");
								   if (copy_to_user((void *)arg, sf_ctl_get_version(), TRAN_DRV_VERSION_LEN)) {
									   TRAN_FP_ERROR("copy_to_user(..) failed.\n");
									   retval = (-EFAULT);
									   break;
								   }

								   break;
							   }

	case TRAN_IOC_SET_LIB_VERSION: {
									   TRAN_FP_INFO(" SF_IOC_SET_LIB_VERSION ======\n");
									   if (copy_from_user((void *)&sf_hw_ver, (void *)arg, sizeof(sf_version_info_t))) {
										   TRAN_FP_ERROR("sf_hw_info_t copy_from_user(..) failed.\n");
										   retval = (-EFAULT);
										   break;
									   }
									   break;
								   }

	case TRAN_IOC_GET_LIB_VERSION: {
									   TRAN_FP_INFO(" SF_IOC_GET_LIB_VERSION ======\n");
									   if (copy_to_user((void *)arg, (void *)&sf_hw_ver, sizeof(sf_version_info_t))) {
										   TRAN_FP_ERROR("sf_hw_info_t copy_to_user(..) failed.\n");
										   retval = (-EFAULT);
										   break;
									   }
									   break;
								   }

	case TRAN_IOC_SET_SPI_BUF_SIZE: {
										TRAN_FP_INFO(" SF_IOC_SET_SPI_BUF_SIZE ======\n");
										tran_fp_datap->spi_buf_size = arg;
										break;
									}

	case TRAN_IOC_SET_RESET_OUTPUT: {
										TRAN_FP_INFO(" SF_IOC_SET_RESET_OUTPUT ======\n");
										if (arg) {
											tran_fp_reset_gpio_high(tran_fp_datap);
										}
										else {
											tran_fp_datap->shutdown(tran_fp_datap);
										}

										break;
									}

	case TRAN_FPSENSOR_IOC_INIT:
									TRAN_FP_INFO(" fpsensor init started======\n");
#ifdef TRAN_FP_ANDROID_T
									android_version = 'T';
#endif
#ifdef TRAN_FP_ANDROID_U
									android_version = 'U';
#endif
									retval = request_threaded_irq(tran_fp_datap->irq, fpsensor_irq, NULL, IRQF_TRIGGER_RISING | IRQF_ONESHOT, FPSENSOR_DEV_NAME, tran_fp_datap);
									if (retval == 0) {
										TRAN_FP_INFO(" irq thread request success!\n");
										g_tran_fp_req_irq_times = 1;
									} else {
										TRAN_FP_ERROR(" irq thread request failed , retval =%d \n", retval);
										break;
									}
									retval = enable_irq_wake(tran_fp_datap->irq);
									if (retval < 0 ) {
										TRAN_FP_ERROR(" enable_irq_wake failed , retval = %d \n", retval);
										free_irq(tran_fp_datap->irq, tran_fp_datap);
										break;
									} else {
										TRAN_FP_INFO(" enable_irq_wake successful======\n");
										tran_fp_datap->irq_count = 1;
									}
									tran_fp_datap->device_available = 1;
									tran_fp_datap->spi_clk_on(tran_fp_datap);
									tran_fp_datap->disable_irq(tran_fp_datap);
									if (copy_to_user((void __user *)arg, (void *)&android_version, sizeof(char))) {
										TRAN_FP_ERROR("Failed to copy data to user\n");
										retval = -EFAULT;
										break;
									}
									TRAN_FP_INFO(" fpsensor init finished======\n");
									break;

	case TRAN_FPSENSOR_IOC_EXIT:
									TRAN_FP_INFO(" fpsensor exit started======\n");
									tran_fp_datap->disable_irq(tran_fp_datap);
									tran_fp_datap->spi_clk_off(tran_fp_datap);
									if (tran_fp_datap->irq && g_tran_fp_req_irq_times == 1) {
										free_irq(tran_fp_datap->irq, tran_fp_datap);
										tran_fp_datap->irq_count = 0;
										g_tran_fp_req_irq_times = 0;
									}
									tran_fp_datap->device_available = 0;
									TRAN_FP_INFO(" fpsensor exit finished======\n");
									break;

	case TRAN_FPSENSOR_IOC_RESET:
									TRAN_FP_INFO(" fpsensor chip reset command\n");
									tran_fp_datap->reset(tran_fp_datap, tran_fp_datap->inter_delay, tran_fp_datap->post_delay);
									break;

	case TRAN_FPSENSOR_IOC_ENABLE_IRQ:
									TRAN_FP_INFO(" fpsensor chip ENable IRQ command\n");
									tran_fp_datap->enable_irq(tran_fp_datap);
									break;

	case TRAN_FPSENSOR_IOC_DISABLE_IRQ:
									TRAN_FP_INFO(" fpsensor chip disable IRQ command\n");
									tran_fp_datap->disable_irq(tran_fp_datap);
									break;
	case TRAN_FPSENSOR_IOC_GET_INT_VAL:
									TRAN_FP_INFO(" fpsensor get int val command\n");
									val = gpio_get_value(tran_fp_datap->irq_gpio);
									if (copy_to_user((void __user *)arg, (void *)&val, sizeof(unsigned int))) {
										TRAN_FP_ERROR("Failed to copy data to user\n");
										retval = -EFAULT;
										break;
									}
									retval = 0;
									break;
	case TRAN_FPSENSOR_IOC_ENABLE_SPI_CLK:
									TRAN_FP_INFO(" fpsensor ENABLE_SPI_CLK ======\n");
									tran_fp_datap->spi_clk_on(tran_fp_datap);
									break;
	case TRAN_FPSENSOR_IOC_DISABLE_SPI_CLK:
									TRAN_FP_INFO(" fpsensor DISABLE_SPI_CLK ======\n");
									tran_fp_datap->spi_clk_off(tran_fp_datap);
									break;
	case TRAN_FPSENSOR_IOC_ENABLE_POWER:
									TRAN_FP_INFO(" FPSENSOR_IOC_ENABLE_POWER ======\n");
									break;
	case TRAN_FPSENSOR_IOC_DISABLE_POWER:
									TRAN_FP_INFO(" FPSENSOR_IOC_DISABLE_POWER ======\n");
									break;
	case TRAN_FPSENSOR_IOC_REMOVE:
									TRAN_FP_INFO(" FPSENSOR_IOC_REMOVE ======\n");
									tran_fp_datap->disable_irq(tran_fp_datap);
									if (tran_fp_datap->irq && g_tran_fp_req_irq_times == 1) {
										free_irq(tran_fp_datap->irq, tran_fp_datap);
										tran_fp_datap->irq_count = 0;
									}
									tran_fp_datap->device_available = 0;
									fpsensor_dev_cleanup(tran_fp_datap);
									fb_unregister_client(&tran_fp_datap->notifier);
									tran_fp_datap->free_flag = 1;
									TRAN_FP_INFO("fpsensor remove finished\n");
									break;
	case TRAN_FPSENSOR_IOC_CANCEL_WAIT:
									TRAN_FP_INFO(" FPSENSOR CANCEL WAIT\n");
									tran_fp_datap->cancel = 1;
									wake_up_interruptible(&tran_fp_datap->wq_irq_return);
									break;
	case TRAN_FPSENSOR_IOC_GET_FP_STATUS :
									val = tran_fp_datap->fb_status;
									TRAN_FP_INFO(" FPSENSOR_IOC_GET_FP_STATUS  %d \n", tran_fp_datap->fb_status);
									if (copy_to_user((void __user *)arg, (void *)&val, sizeof(unsigned int))) {
										TRAN_FP_ERROR("Failed to copy data to user\n");
										retval = -EFAULT;
										break;
									}
									retval = 0;
									break;
	case TRAN_FPSENSOR_IOC_ENABLE_REPORT_BLANKON:
									TRAN_FP_INFO(" FPSENSOR_IOC_ENABLE_REPORT_BLANKON \n");
									if (copy_from_user(&val, (void __user *)arg, sizeof(uint32_t))) {
										retval = -EFAULT;
										break;
									}
									tran_fp_datap->enable_report_blankon = val;
									TRAN_FP_DEBUG("FPSENSOR_IOC_ENABLE_REPORT_BLANKON: %d\n", val);
									break;
	case TRAN_FPSENSOR_IOC_UPDATE_DRIVER_SN:
									/*
									   if (copy_from_user(&g_cmd_sn, (void __user *)arg, sizeof(uint32_t))) {
									   TRAN_FP_ERROR("Failed to copy g_cmd_sn from user to kernel");
									   retval = -EFAULT;
									   break;
									   }
									   TRAN_FP_INFO("FPSENSOR_IOC_UPDATE_DRIVER_SN: %d", g_cmd_sn);
									   */
									break;
	case TRAN_OXIFP_IOC_RST_SENSOR:
									TRAN_FP_INFO(" oxi chip reset command\n");
									tran_fp_datap->reset(tran_fp_datap, tran_fp_datap->inter_delay, tran_fp_datap->post_delay);
									break;
	case TRAN_OXIFP_IOC_CS_HIGH:
									TRAN_FP_INFO(" oxi chip reset-pin high command\n");
									tran_fp_reset_gpio_high(tran_fp_datap);
									break;
	case TRAN_OXIFP_IOC_CS_LOW:
									TRAN_FP_INFO(" oxi chip reset-pin low command\n");
									tran_fp_datap->shutdown(tran_fp_datap);
									break;
	case TRAN_OXIFP_WAKEUP_SYSTEM:
									TRAN_FP_INFO(" oxi wakeup system command\n");
									break;
									//Silead ioctl cmd start
	case TRAN_SIFP_IOC_INIT:
									TRAN_FP_INFO(" silead chip init command\n");
									//BSP: delete fix silead 2nd cit test fail XLLLHLWS-309 by fan.peng 20201118 start
									//g_tran_fp_datap = tran_fp_datap;
									//BSP: delete fix silead 2nd cit test fail XLLLHLWS-309 by fan.peng 20201118 end
									//qing.li start
									atomic_set(&tran_fp_datap->spi_clk_onoff_cnt, 0);
									//qing.li end
									if (atomic_read(&tran_fp_datap->init)) {
										atomic_inc(&tran_fp_datap->init);
										TRAN_FP_DEBUG(" dev already init(%d).\n", atomic_read(&tran_fp_datap->init));
										if (copy_to_user((void __user *)arg, (void *)&silfp_dev_init_d, sizeof(struct fp_dev_init_t))){
											retval = -EFAULT;
										}
										break;
									}
									tran_fp_datap->irq_no_use = 0;

									if (tran_fp_datap->power_is_on == 0) {
										tran_fp_datap->power_init(tran_fp_datap, true);
										tran_fp_datap->power_onoff(tran_fp_datap, true);
										tran_fp_datap->power_is_on = 1;
									}
									retval = request_irq(tran_fp_datap->irq,
											silfp_irq_handler,
											IRQ_TYPE_EDGE_RISING, //IRQ_TYPE_LEVEL_HIGH, //irq_table[ts->int_trigger_type],
											"silfp",
											tran_fp_datap);
									if ( retval < 0 ) {
										TRAN_FP_ERROR(" Failed to request_irq (%d), retval = %d", tran_fp_datap->irq, retval);
										retval = -ENODEV;
										break;
									} else {
										g_tran_fp_req_irq_times = 1;
										TRAN_FP_INFO(" Enable_irq_wake.\n");
										retval = enable_irq_wake(tran_fp_datap->irq);
										if (retval < 0 ) {
											TRAN_FP_ERROR(" enable_irq_wake failed , retval = %d \n", retval);
											free_irq(tran_fp_datap->irq, tran_fp_datap);
											break;
										} else {
											TRAN_FP_INFO(" enable_irq_wake successful======\n");
											tran_fp_datap->irq_count = 1;
										}
										tran_fp_datap->disable_irq(tran_fp_datap);
									}

									if (tran_fp_datap->input == NULL) {
										retval = tran_fp_input_init();
										if (retval) {
											TRAN_FP_ERROR(" Filed to init input, retval = %d", retval);
											break;
										}
									}
									atomic_set(&tran_fp_datap->init, 1);
									silfp_dev_init_d.reserve = PKG_SIZE;
									silfp_dev_init_d.reserve <<= 12;

									if (tran_fp_datap->spi_id) {
										silfp_dev_init_d.dev_id = (uint8_t)tran_fp_datap->spi_id;
										silfp_dev_init_d.reserve |= tran_fp_datap->spi_irq & 0x0FFF;
										silfp_dev_init_d.reg = tran_fp_datap->spi_reg;
									}
									if (copy_to_user((void __user *)arg, (void *)&silfp_dev_init_d, sizeof(struct fp_dev_init_t))) {
										retval = -EFAULT;
									}
									TRAN_FP_INFO(" silead chip init success\n");
									break;
	case TRAN_SIFP_IOC_DEINIT:
									TRAN_FP_INFO(" silead chip deinit command\n");
									//BSP: delete fix silead 2nd cit test fail XLLLHLWS-309 by fan.peng 20201118 start
									//g_tran_fp_datap = NULL;
									//BSP: delete fix silead 2nd cit test fail XLLLHLWS-309 by fan.peng 20201118 end
									if (atomic_read(&tran_fp_datap->init)) {
										atomic_dec(&tran_fp_datap->init);

										if (!atomic_read(&tran_fp_datap->init)) {
											TRAN_FP_DEBUG(" no more users, free GPIOs\n");
											if (!tran_fp_datap->irq_no_use) {
												tran_fp_datap->disable_irq(tran_fp_datap);
												free_irq(tran_fp_datap->irq, tran_fp_datap);
												tran_fp_datap->irq_no_use = 1;
											}

											gpio_direction_input(tran_fp_datap->irq_gpio);
											tran_fp_datap->irq_gpio = 0;
											tran_fp_datap->reset_gpio = 0;
											if (tran_fp_datap->input) {
												input_unregister_device(tran_fp_datap->input);
												tran_fp_datap->input = NULL;
											}
											if (tran_fp_datap->power_is_on == 1) {
												tran_fp_datap->power_onoff(tran_fp_datap, false);
												tran_fp_datap->power_init(tran_fp_datap, false);
												tran_fp_datap->power_is_on = 0;
											}
											if (tran_fp_datap->proc_entry) {
												remove_proc_entry(PROC_NODE, NULL);
												tran_fp_datap->proc_entry = NULL;
											}
										}
										tran_netlink_send(tran_fp_datap, SIFP_NETLINK_DISCONNECT);
									}
									TRAN_FP_INFO(" silead chip deinit success\n");
									break;

	case TRAN_SIFP_IOC_RESET:
									delay = RESET_TIME;
									TRAN_FP_INFO(" silead chip reset, delay = %d on start\n", delay);
									if (arg) {
										if (copy_from_user(&delay, (void __user *)arg, sizeof(char))) {
											retval = -EFAULT;
											break;
										}
									}

									TRAN_FP_INFO(" delay = %d after confluence of user space\n", delay);
									if (tran_fp_datap->power_is_on == 0) {
										tran_fp_datap->power_init(tran_fp_datap, true);
										tran_fp_datap->power_onoff(tran_fp_datap, true);
										tran_fp_datap->power_is_on = 1;
									}
									tran_fp_datap->irq_ignore = 1;
									if (!tran_fp_datap->irq_count && !tran_fp_datap->irq_no_use) {
										tran_fp_datap->enable_irq(tran_fp_datap);
										tran_fp_datap->reset(tran_fp_datap, delay ? delay * 1000 : 5000, delay ? delay * 1000 : 3000);
										tran_fp_datap->disable_irq(tran_fp_datap);
									} else {
										tran_fp_datap->reset(tran_fp_datap, delay ? delay * 1000 : 5000, delay ? delay * 1000 : 3000);
									}
									tran_fp_datap->irq_ignore = 0;
									break;

	case TRAN_SIFP_IOC_ENABLE_IRQ:
									TRAN_FP_INFO(" silead enable irq\n");
									tran_fp_datap->enable_irq(tran_fp_datap);
									break;

	case TRAN_SIFP_IOC_DISABLE_IRQ:
									TRAN_FP_INFO(" silead disable irq\n");
									tran_fp_datap->disable_irq(tran_fp_datap);
									break;

	case TRAN_SIFP_IOC_CLR_IRQ:
									TRAN_FP_INFO(" silead clear irq\n");
									break;

	case TRAN_SIFP_IOC_WAIT_IRQ:
									TRAN_FP_INFO(" silead wait irq\n");
									wait_for_completion(&tran_fp_datap->done);
									retval = __put_user(1, (__u8 __user *)arg);
									break;

	case TRAN_SIFP_IOC_IRQ_STATUS:
									TRAN_FP_INFO(" silead irq status\n");
									if (tran_fp_datap->irq_gpio)
										val = gpio_get_value(tran_fp_datap->irq_gpio);
									retval = __put_user((char)val, (__u8 __user *)arg);
									break;

	case TRAN_SIFP_IOC_KEY_EVENT:
									TRAN_FP_INFO(" silead key event\n");
									if (copy_from_user(&key, (struct fp_dev_key_t *)arg, sizeof(struct fp_dev_key_t))) {
										TRAN_FP_ERROR(" silead copy key fail?\n");
										retval = -EFAULT;
									} else {
										retval = silfp_keyevent(tran_fp_datap, &key);
									}
									break;

	case TRAN_SIFP_IOC_SCR_STATUS:
									TRAN_FP_INFO(" silead screen status\n");
									if (arg) {
										TRAN_FP_INFO(" silead [IOC_SCR_STATUS] put_user !scr_off = %d\n", (u8)(!tran_fp_datap->scr_off));
										retval = __put_user((u8)(!tran_fp_datap->scr_off), (__u8 __user *)arg);
									} else {
										TRAN_FP_INFO(" silead [IOC_SCR_STATUS] netlink_send !scr_off = %d\n", (u8)(!tran_fp_datap->scr_off));
										tran_netlink_send(tran_fp_datap, tran_fp_datap->scr_off ? SIFP_NETLINK_SCR_OFF:SIFP_NETLINK_SCR_ON);
									}
									break;

	case TRAN_SIFP_IOC_GET_VER:
									TRAN_FP_INFO(" silead get version\n");
									if ( copy_to_user((void __user *)arg, (void *)SIL_FP_DEV_VERSION, sizeof(char)*7)) {
										TRAN_FP_ERROR(" silead [IOC_GET_VER] copy_to fail\n");
										retval = -EFAULT;
									}
									break;

	case TRAN_SIFP_IOC_SET_KMAP:
									TRAN_FP_INFO(" silead set kmap\n");
									if (!arg) {
										retval = -EFAULT;
										break;
									}
									if (copy_from_user(tran_fp_datap->k, (void __user *)arg, sizeof(uint16_t)*(NAV_KEY_MAX-NAV_KEY_START))) {
										TRAN_FP_ERROR(" silead [IOC_SET_KMAP] copy_from fail\n");
										retval = -EFAULT;
										break;
									}

									if (tran_fp_datap->input) {
										input_unregister_device(tran_fp_datap->input);
									}
									for ( i = 0; i < ARRAY_SIZE(tran_fp_datap->k); i++ ) {
										if (tran_fp_datap->k[i] && (tran_fp_datap->k[i] != KEY_RESERVED)) {
											__set_bit(tran_fp_datap->k[i], tran_fp_datap->input->keybit);
										}
									}
									err = input_register_device(tran_fp_datap->input);

									if (err) {
										TRAN_FP_ERROR(" silead input_register_device(..) = %d.\n", err);
										input_free_device(tran_fp_datap->input);
										tran_fp_datap->input = NULL;
										return (-ENODEV);
									}
									break;

	case TRAN_SIFP_IOC_ACQ_SPI:
									TRAN_FP_INFO(" silead enable clk\n");
									retval = tran_fp_datap->tran_fp_set_clock(tran_fp_datap,1);
									break;

	case TRAN_SIFP_IOC_RLS_SPI:
									TRAN_FP_INFO(" silead disable clk\n");
									retval = tran_fp_datap->tran_fp_set_clock(tran_fp_datap,0);
									break;

	case TRAN_SIFP_IOC_PKG_SIZE:
									TRAN_FP_INFO(" silead package size\n");
									if (arg) {
										TRAN_FP_DEBUG(" silead [IOC_PKG_SIZE] %d\n", (u8)(PKG_SIZE));
										retval = __put_user((u8)(PKG_SIZE), (__u8 __user *)arg);
									}
									break;

	case TRAN_SIFP_IOC_DBG_LEVEL:
									TRAN_FP_INFO(" silead debug level\n");
									if (arg) {
										unsigned char level = 0;
										if (copy_from_user(&level, (void __user *)arg, sizeof(char)) || level > (uint8_t)ALL_LOG ) {
											retval = -EFAULT;
											break;
										}
										TRAN_FP_DEBUG(" silead debug level %d-->%d\n", sil_debug_level, level);
										retval = __put_user((u8)(sil_debug_level), (__u8 __user *)arg);
										sil_debug_level = (fp_debug_level_t)level;
									}
									break;

	case TRAN_SIFP_IOC_WAKELOCK:
									//TRAN_FP_INFO(" silead wakelock\n");
									if (arg) {
										unsigned char lock = 0;
										if (copy_from_user(&lock, (void __user *)arg, sizeof(char))) {
											TRAN_FP_ERROR(" silead [SIFP_IOC_WAKELOCK] copy_from fail\n");
											retval = -EFAULT;
											break;
										}
										silfp_wakelock_ctl(tran_fp_datap, lock);
									}
									break;

	case TRAN_SIFP_IOC_PWDN:
									TRAN_FP_INFO(" silead power down\n");
									if (arg) {
										unsigned char flag_avdd = 0;
										if (copy_from_user(&flag_avdd, (void __user *)arg, sizeof(char))) {
											TRAN_FP_ERROR(" silead [SIFP_IOC_PWDN] copy_from fail\n");
											retval = -EFAULT;
											break;
										}
										silfp_pwdn(tran_fp_datap, flag_avdd);
									} else {
										silfp_pwdn(tran_fp_datap, SIFP_PWDN_NONE);
									}

									break;

	case TRAN_SIFP_IOC_PROC_NODE:
									TRAN_FP_INFO(" silead proc node\n");
									if (arg) {
										if (copy_from_user(vendor_name, (void __user *)arg, PROC_VND_ID_LEN)) {
											TRAN_FP_ERROR(" silead [SIFP_IOC_PROC_NODE] copy_from fail\n");
											retval = -EFAULT;
											break;
										}
										retval = silfp_proc_create_node(tran_fp_datap);
										if (retval) {
											TRAN_FP_ERROR(" silfp_proc_create_node failed! retval = %d\n", retval);
										}
									}
									break;

	case TRAN_SIFP_IOC_SET_FEATURE:
									TRAN_FP_INFO(" silead set feature\n");
									if (arg) {
										unsigned char feature = 0;
										if (copy_from_user(&feature, (void __user *)arg, sizeof(char))) {
											TRAN_FP_ERROR(" silead [SIFP_IOC_SET_FEATURE] copy_from fail\n");
											retval = -EFAULT;
											break;
										}
										if(feature == FEATURE_FLASH_CS){
											TRAN_FP_INFO(" silead set feature flash cs\n");
											tran_fp_datap->disable_irq(tran_fp_datap);
											free_irq(tran_fp_datap->irq, tran_fp_datap);
											pinctrl_select_state(tran_fp_datap->tran_pinctrl, tran_fp_datap->pins_reset_low);
											tran_fp_datap->irq_no_use = 1;
										}
									}
									break;
									//Cdfinger ioctl cmd start
	case TRAN_CDFINGER_INIT:
									TRAN_FP_INFO(" ======CDFINGER_INIT======");
									break;
	case TRAN_CDFINGER_GETIMAGE:
									TRAN_FP_INFO(" ======CDFINGER_GETIMAGE======");
									break;
	case TRAN_CDFINGER_INITERRUPT_MODE:
									TRAN_FP_INFO(" ======CDFINGER_INITERRUPT_MODE======");
									tran_fp_datap->sign_sync = 0;
									tran_fp_datap->isInKeyMode = 1;  // not key mode
									tran_fp_datap->reset(tran_fp_datap, 2000, 2000);
									retval = cdfinger_mode_init(tran_fp_datap, arg, FINGER_INTERRUPT_MODE);
									break;
	case TRAN_CDFINGER_NEW_KEYMODE:
									TRAN_FP_INFO(" ======CDFINGER_NEW_KEYMODE======");
									tran_fp_datap->isInKeyMode = 0;
									retval = cdfinger_mode_init(tran_fp_datap, arg, FINGER_INTERRUPT_MODE);
									break;
	case TRAN_CDFINGER_INITERRUPT_FINGERUPMODE:
									TRAN_FP_INFO(" ======CDFINGER_INITERRUPT_FINGERUPMODE======");
									retval = cdfinger_mode_init(tran_fp_datap, arg, FINGER_FINGER_UP_MODE);
									break;
	case TRAN_CDFINGER_RELEASE_WAKELOCK:
									TRAN_FP_INFO(" ======CDFINGER_RELEASE_WAKELOCK======");
									cdfinger_release_wakelock(tran_fp_datap);
									break;
	case TRAN_CDFINGER_INITERRUPT_KEYMODE:
									TRAN_FP_INFO(" ======CDFINGER_INITERRUPT_KEYMODE======");
									retval = cdfinger_mode_init(tran_fp_datap, arg, FINGER_KEY_MODE);
									break;
	case TRAN_CDFINGER_CHECK_INTERRUPT:
									TRAN_FP_INFO(" ======CDFINGER_CHECK_INTERRUPT======");
									break;
	case TRAN_CDFINGER_SET_SPI_SPEED:
									TRAN_FP_INFO(" ======CDFINGER_SET_SPI_SPEED======");
									break;
	case TRAN_CDFINGER_WAKE_LOCK:
									TRAN_FP_INFO(" ======CDFINGER_WAKE_LOCK======");
									cdfinger_wake_lock(tran_fp_datap, arg);
									break;
	case TRAN_CDFINGER_REPORT_KEY:
									TRAN_FP_INFO(" ======CDFINGER_REPORT_KEY======");
									if (copy_from_user(&tran_key_nav, (tran_key_nav_t *)arg, sizeof(tran_key_nav_t))) {
										TRAN_FP_ERROR("copy_from_user(..) failed.\n");
										retval = (-EFAULT);
										break;
									}

									cdf_key_nav_adjust_type_value(&(tran_key_nav.key), TRAN_CDFINGER_REPORT_KEY);
									retval = tran_fp_report_key_nav_event(tran_fp_datap->input, &tran_key_nav);
									break;
	case TRAN_CDFINGER_REPORT_KEY_LEGACY:
									TRAN_FP_INFO(" ======CDFINGER_REPORT_KEY_LEGACY======");
									retval = cdfinger_report_key_legacy(tran_fp_datap, arg);
									break;
	case TRAN_CDFINGER_POWERDOWN:
									TRAN_FP_INFO(" ======CDFINGER_POWERDOWN======");
									break;
	case TRAN_CDFINGER_ENABLE_IRQ:
									TRAN_FP_INFO(" ======CDFINGER_ENABLE_IRQ======");
									tran_fp_datap->enable_irq(tran_fp_datap);
									break;
	case TRAN_CDFINGER_DISABLE_IRQ:
									TRAN_FP_INFO(" ======CDFINGER_DISABLE_IRQ======");
									tran_fp_datap->disable_irq(tran_fp_datap);
									break;
	case TRAN_CDFINGER_ENABLE_CLK:
	case TRAN_CDFINGER_SPI_CLK:
									if (arg == 1) {
										TRAN_FP_INFO(" ======CDFINGER_ENABLE_CLK======");
										tran_fp_datap->spi_clk_on(tran_fp_datap);
									} else if (arg == 0) {
										TRAN_FP_INFO(" ======CDFINGER_DISABLE_CLK======");
										tran_fp_datap->spi_clk_off(tran_fp_datap);
									}
									break;
	case TRAN_CDFINGER_HW_RESET:
									TRAN_FP_INFO(" ======CCDFINGER_HW_RESET======");
									tran_fp_datap->reset(tran_fp_datap, arg * 1000, arg * 1000);
									break;
	case TRAN_CDFINGER_GET_STATUS:
									TRAN_FP_INFO(" ======CDFINGER_GET_STATUS======");
									retval = tran_fp_datap->screen_status;
									break;
	case TRAN_CDFINGER_POLL_TRIGGER:
									TRAN_FP_INFO(" ======CDFINGER_POLL_TRIGGER======");
									tran_fp_datap->sign_sync = 2;
									wake_up_interruptible(&tran_fp_datap->wq_irq_return);
									retval = 0;
									break;
									//Focaltech ioctl cmd start
	case TRAN_FF_IOC_INIT_DRIVER:
									TRAN_FP_INFO(" ======FF_IOC_INIT_DRIVER======");
									if (tran_fp_datap->b_driver_inited) {
										TRAN_FP_INFO(" ======driver inited already======");
									} else {
										retval = request_irq(tran_fp_datap->irq, ff_ctl_device_irq,
												IRQF_TRIGGER_RISING | IRQF_ONESHOT, "ff_irq", (void*)tran_fp_datap);
										if (retval) {
											TRAN_FP_ERROR("request_irq(..) = %d.", retval);
											break;
										}
										g_tran_fp_req_irq_times = 1;
										retval = enable_irq_wake(tran_fp_datap->irq);
										if (retval) {
											TRAN_FP_ERROR("enable_irq_wake(%d) = %d.", tran_fp_datap->irq, retval);
											free_irq(tran_fp_datap->irq, tran_fp_datap);
											break;
										} else {
											TRAN_FP_INFO(" enable_irq_wake successful======\n");
											tran_fp_datap->irq_count = 1;
										}
										if (ff_g_config && ff_g_config->enable_spidev) {
											spi_desc.bus_num = ff_g_config->spidev_bus;
											spi_desc.chip_select = ff_g_config->spidev_c_s;
											//spi_ctl = spi_busnum_to_master(spi_desc.bus_num);
											if (!spi_ctl) {
												TRAN_FP_ERROR("there is no spi master controller on bus #%d.", spi_desc.bus_num);
												retval = disable_irq_wake(tran_fp_datap->irq);
												if (retval) {
													TRAN_FP_ERROR("disable_irq_wake(%d) = %d.", tran_fp_datap->irq, retval);
												}
												free_irq(tran_fp_datap->irq, tran_fp_datap);
												retval = -ENODEV;
												break;
											}
											//spi_dev = spi_new_device(spi_ctl, &spi_desc);
											if (!spi_dev) {
												TRAN_FP_ERROR("failed to register spidev device.");
												spi_controller_put(spi_ctl);
												spi_ctl = NULL;
												retval = disable_irq_wake(tran_fp_datap->irq);
												if (retval) {
													TRAN_FP_ERROR("disable_irq_wake(%d) = %d.", tran_fp_datap->irq, retval);
												}
												free_irq(tran_fp_datap->irq, tran_fp_datap);
												retval = -EBUSY;
												break;
											}
											TRAN_FP_INFO("spidev device has been registered.");
										}
										if (!tran_fp_datap->input) {
											retval = focaltech_customize_input_init(tran_fp_datap);
											if (retval) {
												TRAN_FP_ERROR("focaltech_customize_input_init() = %d.", retval);
											}
										}
										retval = tran_fp_datap->spi_clk_on(tran_fp_datap);
										if (tran_fp_datap->lcm_onoff_registered == false) {
#ifndef CONFIG_HAS_EARLYSUSPEND
											//retval = fb_register_client(&tran_fp_datap->notifier);
											retval = mtk_disp_notifier_register("FINGERPRINT", &tran_fp_datap->notifier);
											if (retval) {
												TRAN_FP_ERROR("mtk_disp_notifier_register failed, retval = %d.", retval);
												break;
											}
#else
											register_early_suspend(&tran_fp_datap->early_suspend);

#endif
											tran_fp_datap->lcm_onoff_registered = true;
										}
										tran_fp_datap->b_driver_inited = true;
									}
									break;
	case TRAN_FF_IOC_FREE_DRIVER:
									TRAN_FP_INFO(" ======FF_IOC_FREE_DRIVER======");
									if (tran_fp_datap->b_driver_inited) {
										if (tran_fp_datap->lcm_onoff_registered == true) {
#ifndef CONFIG_HAS_EARLYSUSPEND
											retval = fb_unregister_client(&tran_fp_datap->notifier);
											if (retval) {
												TRAN_FP_ERROR("fb_unregister_client failed, retval = %d.", retval);
												break;
											}
#else
											unregister_early_suspend(&tran_fp_datap->early_suspend);
#endif
											tran_fp_datap->lcm_onoff_registered = false;
										}
										tran_fp_datap->spi_clk_off(tran_fp_datap);
										if (spi_dev) {
											spi_controller_put(spi_dev->controller);
											spi_unregister_device(spi_dev);
											spi_dev = NULL;
											TRAN_FP_INFO("spidev device has been unregistered.");
										}
										if (spi_ctl) {
											spi_controller_put(spi_ctl);
											spi_ctl = NULL;
										}
										if (tran_fp_datap->input) {
											input_unregister_device(tran_fp_datap->input);
											tran_fp_datap->input = NULL;
										}
										if (tran_fp_datap->tran_pinctrl != NULL ) {
											devm_pinctrl_put(tran_fp_datap->tran_pinctrl);
											tran_fp_datap->tran_pinctrl = NULL;
										}
										if (tran_fp_datap->irq > 0) {
											retval = disable_irq_wake(tran_fp_datap->irq);
											if (retval) {
												TRAN_FP_ERROR("disable_irq_wake(%d) = %d.", tran_fp_datap->irq, retval);
											}
											free_irq(tran_fp_datap->irq, (void*)tran_fp_datap);
											tran_fp_datap->irq = -1;
										}
									}
									tran_fp_datap->b_driver_inited = false;
									break;
	case TRAN_FF_IOC_RESET_DEVICE:
									TRAN_FP_INFO(" ======FF_IOC_RESET_DEVICE======");
									tran_fp_datap->reset(tran_fp_datap, tran_fp_datap->inter_delay, tran_fp_datap->post_delay);
									break;
	case TRAN_FF_IOC_RESET_DEVICE_HL:
									TRAN_FP_INFO(" ======FF_IOC_RESET_DEVICE_HL======");
									if(tran_fp_datap->tran_pinctrl) {
										if (!!arg) {
											pinctrl_select_state(tran_fp_datap->tran_pinctrl, tran_fp_datap->pins_reset_high);
										} else {
											pinctrl_select_state(tran_fp_datap->tran_pinctrl, tran_fp_datap->pins_reset_low);
										}
									} else {
										TRAN_FP_ERROR("tran_pinctrl null,FF_IOC_RESET_DEVICE_HL fail");
									}
									break;
	case TRAN_FF_IOC_ENABLE_IRQ:
									TRAN_FP_INFO(" ======FF_IOC_ENABLE_IRQ======");
									tran_fp_datap->enable_irq(tran_fp_datap);
									break;
	case TRAN_FF_IOC_DISABLE_IRQ:
									TRAN_FP_INFO(" ======FF_IOC_DISABLE_IRQ======");
									tran_fp_datap->disable_irq(tran_fp_datap);
									break;
	case TRAN_FF_IOC_ENABLE_SPI_CLK:
									TRAN_FP_INFO(" ======FF_IOC_ENABLE_SPI_CLK======");
									tran_fp_datap->spi_clk_on(tran_fp_datap);
									break;
	case TRAN_FF_IOC_DISABLE_SPI_CLK:
									TRAN_FP_INFO(" ======FF_IOC_DISABLE_SPI_CLK======");
									tran_fp_datap->spi_clk_off(tran_fp_datap);
									break;
	case TRAN_FF_IOC_ENABLE_POWER:
									TRAN_FP_INFO(" ======FF_IOC_ENABLE_POWER======");
									if (tran_fp_datap->power_is_on == 0) {
										tran_fp_datap->power_init(tran_fp_datap, true);
										tran_fp_datap->power_onoff(tran_fp_datap, true);
										tran_fp_datap->power_is_on = 1;
									}
									break;
	case TRAN_FF_IOC_DISABLE_POWER:
									TRAN_FP_INFO(" ======FF_IOC_DISABLE_POWER======");
									if (tran_fp_datap->power_is_on == 1) {
										tran_fp_datap->power_onoff(tran_fp_datap, false);
										tran_fp_datap->power_init(tran_fp_datap, false);
										tran_fp_datap->power_is_on = 0;
									}
									break;
	case TRAN_FF_IOC_REPORT_KEY_EVENT:
									TRAN_FP_INFO(" ======FF_IOC_REPORT_KEY_EVENT======");
									if (copy_from_user(&kevent, (ff_key_event_t *)arg, sizeof(ff_key_event_t))) {
										TRAN_FP_ERROR("copy_from_user(..) failed.");
										err = -EFAULT;
										break;
									}
									if (tran_fp_datap->input) {
										input_report_key(tran_fp_datap->input, kevent.code, kevent.value);
										input_sync(tran_fp_datap->input);
									}
									break;
	case TRAN_FF_IOC_SYNC_CONFIG:
									TRAN_FP_INFO(" ======FF_IOC_SYNC_CONFIG======");
									if (copy_from_user(&driver_config, (ff_driver_config_t *)arg, sizeof(ff_driver_config_t))) {
										TRAN_FP_ERROR("copy_from_user(..) failed.");
										err = -EFAULT;
										break;
									}
									ff_g_config = &driver_config;
									g_log_level = ff_g_config->log_level;
									break;
	case TRAN_FF_IOC_GET_VERSION:
									TRAN_FP_INFO(" ======FF_IOC_GET_VERSION======");
									if (copy_to_user((void *)arg, ff_ctl_get_version(), FF_DRV_VERSION_LEN)) {
										TRAN_FP_ERROR("copy_to_user(..) failed.");
										retval = -EFAULT;
										break;
									}
									break;
									//EGIS ioctl cmd start
	case TRAN_EGIS_FP_GET_RESOURCE:
									TRAN_FP_INFO(" ======EGIS_FP_GET_RESOURCE======");
									if (tran_fp_datap->b_driver_inited == 0) {
										if (tran_fp_datap->power_is_on == 1) {
											tran_fp_datap->power_onoff(tran_fp_datap, false);
											tran_fp_datap->power_init(tran_fp_datap, false);
											tran_fp_datap->power_is_on = 0;
										}
										tran_fp_datap->shutdown(tran_fp_datap);
										if (tran_fp_datap->tran_pinctrl == NULL) {
											tran_fp_datap->tran_pinctrl = devm_pinctrl_get(&tran_fp_datap->tran_fp_dev->dev);
											if(IS_ERR(tran_fp_datap->tran_pinctrl)){
												TRAN_FP_ERROR("devm_pinctrl_get failed!");
												retval = PTR_ERR(tran_fp_datap->tran_pinctrl);
											}
										}
										retval = pinctrl_select_state(tran_fp_datap->tran_pinctrl, tran_fp_datap->pins_spi_default);
										if (retval)
											TRAN_FP_ERROR(" pinctrl select spi default mode failed!");
										else
											TRAN_FP_INFO("[tran] set spi default state success!");
										tran_fp_datap->b_driver_inited = 1;
									}
									break;
	case TRAN_EGIS_FP_FREE_RESOURCE:
									TRAN_FP_INFO(" ======EGIS_FP_FREE_RESOURCE======");
									if (tran_fp_datap->b_driver_inited == 1) {
										if (g_tran_fp_req_irq_times == 1) {
											if (tran_fp_datap->fps_ints.drdy_irq_flag == DRDY_IRQ_ENABLE)
											{
												disable_irq_nosync(tran_fp_datap->irq);
												tran_fp_datap->fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;
											}
											free_irq(tran_fp_datap->irq, tran_fp_datap);
											g_tran_fp_req_irq_times = 0;
										}
										tran_fp_datap->spi_clk_off(tran_fp_datap);
										retval = pinctrl_select_state(tran_fp_datap->tran_pinctrl, tran_fp_datap->pins_spi_gpio);
										if (retval)
											TRAN_FP_ERROR(" pinctrl select spi gpio mode failed!");
										tran_fp_datap->shutdown(tran_fp_datap);
										mdelay(1);
										if (tran_fp_datap->power_is_on == 1) {
											tran_fp_datap->power_onoff(tran_fp_datap, false);
											tran_fp_datap->power_init(tran_fp_datap, false);
											tran_fp_datap->power_is_on = 0;
										}
										mdelay(3);
										if (tran_fp_datap->tran_pinctrl) {
											devm_pinctrl_put(tran_fp_datap->tran_pinctrl);
											tran_fp_datap->tran_pinctrl = NULL;
										}
										tran_fp_datap->b_driver_inited = 0;
									}
									break;
	case TRAN_EGIS_FP_SET_SPI_CLOCK:
									TRAN_FP_INFO(" ======EGIS_FP_SET_SPI_CLOCK======");
									if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
										TRAN_FP_ERROR("Failed to copy from user to kernel\n");
										retval = -EFAULT;
										break;
									}
									tran_fp_datap->clk_speed = data.int_mode * 1000000;
									TRAN_FP_INFO("set clk speed %d", tran_fp_datap->clk_speed);
									break;
	case TRAN_EGIS_INT_TRIGGER_INIT:
									TRAN_FP_INFO(" ======EGIS_INT_TRIGGER_INIT======");
									if (copy_from_user(&data, (int __user *)arg, sizeof(data)))
									{
										TRAN_FP_ERROR("Failed to copy from user to kernel\n");
										retval = -EFAULT;
										break;
									}
									retval = egisfp_interrupt_init(tran_fp_datap, data.int_mode, data.detect_period, data.detect_threshold);
									break;
	case TRAN_EGIS_FP_SENSOR_RESET:
									TRAN_FP_INFO(" ======EGIS_FP_SENSOR_RESET======");
									tran_fp_datap->reset(tran_fp_datap, tran_fp_datap->inter_delay, tran_fp_datap->post_delay);
									break;
	case TRAN_EGIS_FP_POWER_ONOFF:
									TRAN_FP_INFO(" ======EGIS_FP_POWER_ONOFF======");
									if (copy_from_user(&data, (int __user *)arg, sizeof(data)))
									{
										TRAN_FP_ERROR("Failed to copy from user to kernel\n");
										retval = -EFAULT;
										break;
									}
									power_onoff = data.power_on;
									if (power_onoff)
									{
										if (tran_fp_datap->power_is_on == 0) {
											tran_fp_datap->power_init(tran_fp_datap, true);
											tran_fp_datap->power_onoff(tran_fp_datap, true);
											tran_fp_get_power_val(tran_fp_datap, &power_val);
											mdelay(Tpwr_on_delay);
											tran_fp_datap->power_is_on = 1;
										}
									}
									else
									{
										if (tran_fp_datap->power_is_on == 1) {
											tran_fp_datap->power_onoff(tran_fp_datap, false);
											tran_fp_datap->power_init(tran_fp_datap, false);
											tran_fp_get_power_val(tran_fp_datap, &power_val);
											mdelay(Tpwr_off_delay);
											tran_fp_datap->power_is_on = 0;
										}
									}
									break;
	case TRAN_EGIS_FP_RESET_SET:
									TRAN_FP_INFO(" ======EGIS_FP_RESET_SET======");
									if (copy_from_user(&data, (int __user *)arg, sizeof(data)))
									{
										TRAN_FP_ERROR("Failed to copy from user to kernel\n");
										retval = -EFAULT;
										break;
									}
									if(tran_fp_datap->tran_pinctrl) {
										if (data.int_mode) {
											retval = pinctrl_select_state(tran_fp_datap->tran_pinctrl, tran_fp_datap->pins_reset_high);
											mdelay(Rst_on_delay);
										} else {
											retval = pinctrl_select_state(tran_fp_datap->tran_pinctrl, tran_fp_datap->pins_reset_low);
											mdelay(Rst_off_delay);
										}
									}
									break;
	case TRAN_EGIS_FP_WAKELOCK_ENABLE:
									TRAN_FP_INFO(" ======EGIS_FP_WAKELOCK_ENABLE======");
									pm_stay_awake(&tran_fp_datap->tran_fp_dev->dev);
									break;
	case TRAN_EGIS_FP_WAKELOCK_DISABLE:
									TRAN_FP_INFO(" ======EGIS_FP_WAKELOCK_DISABLE======");
									pm_relax(&tran_fp_datap->tran_fp_dev->dev);
									break;
	case TRAN_EGIS_GET_IO_STUS:
									TRAN_FP_INFO(" ======EGIS_GET_IO_STUS======");
									tran_fp_get_power_val(tran_fp_datap, &power_val);
									break;
	case TRAN_EGIS_SEND_NAVI_EVENT:
									TRAN_FP_INFO(" ======EGIS_SEND_NAVI_EVENT======");
									if (copy_from_user(&data, (int __user *)arg, sizeof(data)))
									{
										TRAN_FP_ERROR("Failed to copy from user to kernel\n");
										retval = -EFAULT;
										break;
									}
									tran_key_nav.key = (tran_key_nav_event_t)(data.int_mode);
									egf_key_nav_adjust_type_value(&(tran_key_nav.key), TRAN_EGIS_SEND_NAVI_EVENT);
									tran_key_nav.value = 1;
									tran_fp_report_key_nav_event(tran_fp_datap->input, &tran_key_nav);
									tran_key_nav.value = 0;
									tran_fp_report_key_nav_event(tran_fp_datap->input, &tran_key_nav);
									break;
	case TRAN_EGIS_FP_SPIPIN_SETTING:
									TRAN_FP_INFO(" ======EGIS_FP_SPIPIN_SETTING======");
									if (tran_fp_datap->tran_pinctrl) { 
										retval = pinctrl_select_state(tran_fp_datap->tran_pinctrl, tran_fp_datap->pins_spi_default);
									}
									break;
	case TRAN_EGIS_FP_SPIPIN_PULLLOW:
									TRAN_FP_INFO(" ======EGIS_FP_SPIPIN_PULLLOW======");
									if (tran_fp_datap->tran_pinctrl) { 
										retval = pinctrl_select_state(tran_fp_datap->tran_pinctrl, tran_fp_datap->pins_spi_gpio);
									}
									break;
	case TRAN_EGIS_INT_TRIGGER_CLOSE:
									TRAN_FP_INFO(" ======EGIS_INT_TRIGGER_CLOSE======");
									egisfp_interrupt_free(tran_fp_datap);
									break;
	case TRAN_EGIS_INT_TRIGGER_ABORT:
									TRAN_FP_INFO(" ======EGIS_INT_TRIGGER_ABORT======");
									tran_fp_datap->fps_ints.finger_on = 0;
									tran_fp_datap->fps_ints.drdy_irq_abort = 1;
									wake_up_interruptible(&tran_fp_datap->wq_irq_return);
									break;
	case TRAN_EGIS_FP_FREE_GPIO:
									TRAN_FP_INFO(" ======EGIS_FP_FREE_GPIO======");
									break;
	case TRAN_EGIS_FP_WAKELOCK_TIMEOUT_ENABLE:
									TRAN_FP_INFO(" ======EGIS_FP_WAKELOCK_TIMEOUT_ENABLE======");
									if (copy_from_user(&data, (int __user *)arg, sizeof(data)))
									{
										data.int_mode = WAKE_HOLD_TIME;
									}
									if (data.int_mode == 0)
										data.int_mode = WAKE_HOLD_TIME;
									TRAN_FP_INFO(" data.int_mode = %d", data.int_mode);
									pm_wakeup_event(&tran_fp_datap->tran_fp_dev->dev, data.int_mode);
									break;
	case TRAN_EGIS_FP_WAKELOCK_TIMEOUT_DISABLE:
									TRAN_FP_INFO(" ======EGIS_FP_WAKELOCK_TIMEOUT_DISABLE======");
									break;
	case TRAN_EGIS_FP_SPICLK_ENABLE:
									TRAN_FP_INFO(" ======EGIS_FP_SPICLK_ENABLE======");
									tran_fp_datap->spi_clk_on(tran_fp_datap);
									break;
	case TRAN_EGIS_FP_SPICLK_DISABLE:
									TRAN_FP_INFO(" ======EGIS_FP_SPICLK_DISABLE======");
									tran_fp_datap->spi_clk_off(tran_fp_datap);
									break;
	case TRAN_EGIS_DELETE_DEVICE_NODE:
									TRAN_FP_INFO(" ======EGIS_DELETE_DEVICE_NODE======");
									break;
	case TRAN_EGIS_GET_SCREEN_ONOFF:
									TRAN_FP_INFO(" ======EGIS_GET_SCREEN_ONOFF======");
									data.int_mode = tran_fp_datap->screen_status;
									if (copy_to_user((int __user *)arg, &data, sizeof(data)))
									{
										TRAN_FP_ERROR("Failed to copy from kernel to user \n");
										retval = -EFAULT;
										break;
									}
									break;
									//jiiov ioctl cmd start		

	case TRAN_ANC_IOC_RESET: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_RESET======");
									tran_fp_datap->reset(tran_fp_datap, tran_fp_datap->inter_delay, tran_fp_datap->post_delay);
									mdelay(10);
									break;	

	case TRAN_ANC_IOC_REQUEST_RESOURCE: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_REQUEST_RESOURCE start ======"); 

									if (tran_fp_datap->resource_requested) {
										TRAN_FP_INFO(" resource requested======\n");
										break;
									}
									if (tran_fp_datap->lcm_onoff_registered == false) {
										TRAN_FP_INFO(" register lcm callback");
#if defined(CONFIG_HAS_EARLYSUSPEND)
										register_early_suspend(&tran_fp_datap->early_suspend);
#else
										fb_register_client(&tran_fp_datap->notifier);
#endif
										tran_fp_datap->lcm_onoff_registered = true;
									}
									tran_fp_datap->resource_requested = true;
									TRAN_FP_INFO(" anc TRAN_ANC_IOC_REQUEST_RESOURCE finished======\n");			

									break;		

	case TRAN_ANC_IOC_RELEASE_RESOURCE: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_RELEASE_RESOURCE start======");
									if (tran_fp_datap->irq && g_tran_fp_req_irq_times == 1) {
										tran_fp_datap->disable_irq(tran_fp_datap);
										free_irq(tran_fp_datap->irq, tran_fp_datap);
										g_tran_fp_req_irq_times = 0;
										tran_fp_datap->irq_count = 0;
									}
									if (tran_fp_datap->lcm_onoff_registered == true) {
										TRAN_FP_INFO(" unregister lcm callback");
#ifdef CONFIG_HAS_EARLYSUSPEND
										if (tran_fp_datap->early_suspend.suspend)
											unregister_early_suspend(&tran_fp_datap->early_suspend);
#else
										fb_unregister_client(&tran_fp_datap->notifier);
#endif
										tran_fp_datap->lcm_onoff_registered = false;
									}
									if (tran_fp_datap->power_is_on == 1) {
										tran_fp_datap->power_onoff(tran_fp_datap, false);
										tran_fp_get_power_val(tran_fp_datap, &power_val);
										tran_fp_datap->power_init(tran_fp_datap, false);
										tran_fp_datap->power_is_on = 0;
									}

									tran_fp_datap->resource_requested = false;

									TRAN_FP_INFO(" TRAN_ANC_IOC_RELEASE_RESOURCE leave======\n");

									break;			
	case TRAN_ANC_IOC_ENABLE_IRQ: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_ENABLE_IRQ======"); 
									tran_fp_datap->enable_irq(tran_fp_datap);			
									break;	
	case TRAN_ANC_IOC_DISABLE_IRQ: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_DISABLE_IRQ======");
									tran_fp_datap->disable_irq(tran_fp_datap);			
									break;	
	case TRAN_ANC_IOC_INIT_IRQ: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_INIT_IRQ======"); 
									if (g_tran_fp_req_irq_times == 1) {
										TRAN_FP_INFO(" irq requested======\n");
										break;
									}

									retval = request_threaded_irq(tran_fp_datap->irq, NULL, anc_irq,
											IRQF_TRIGGER_RISING | IRQF_ONESHOT, "anc_fp_irq", tran_fp_datap);
									if (!retval) {
										TRAN_FP_INFO(" irq thread request success!\n");
										g_tran_fp_req_irq_times = 1;
									}
									else {
										TRAN_FP_ERROR(" irq thread request failed, retval=%d\n", retval);
										break;
									}
									retval = enable_irq_wake(tran_fp_datap->irq);
									if (retval < 0 ) {
										TRAN_FP_ERROR(" enable_irq_wake failed , retval = %d \n", retval);
										free_irq(tran_fp_datap->irq, tran_fp_datap);
										break;
									} else {
										TRAN_FP_INFO(" enable_irq_wake successful======\n");
										tran_fp_datap->irq_count = 1;
									}
									tran_fp_datap->disable_irq(tran_fp_datap);
									break;			
	case TRAN_ANC_IOC_DEINIT_IRQ: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_DEINIT_IRQ======");
									if (tran_fp_datap->irq && g_tran_fp_req_irq_times == 1) {
										tran_fp_datap->disable_irq(tran_fp_datap);
										free_irq(tran_fp_datap->irq, tran_fp_datap);
										g_tran_fp_req_irq_times = 0;
										tran_fp_datap->irq_count = 0;
									}
									break;			
	case TRAN_ANC_IOC_SET_IRQ_FLAG_MASK: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_SET_IRQ_FLAG_MASK======");
									break;			
	case TRAN_ANC_IOC_CLEAR_IRQ_FLAG_MASK: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_CLEAR_IRQ_FLAG_MASK======");
									break;			
	case TRAN_ANC_IOC_ENABLE_SPI_CLK: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_ENABLE_SPI_CLK======");
									tran_fp_datap->spi_clk_on(tran_fp_datap);
									break;			
	case TRAN_ANC_IOC_DISABLE_SPI_CLK: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_DISABLE_SPI_CLK======");
									tran_fp_datap->spi_clk_off(tran_fp_datap);
									break;			
	case TRAN_ANC_IOC_ENABLE_POWER: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_ENABLE_POWER======");
									tran_fp_datap->power_init(tran_fp_datap, true);
									tran_fp_datap->power_onoff(tran_fp_datap, true);
									tran_fp_get_power_val(tran_fp_datap, &power_val);
									break;	        
	case TRAN_ANC_IOC_DISABLE_POWER: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_DISABLE_POWER======");
									tran_fp_datap->power_onoff(tran_fp_datap, false);
									tran_fp_get_power_val(tran_fp_datap, &power_val);
									tran_fp_datap->power_init(tran_fp_datap, false);
									break;			
	case TRAN_ANC_IOC_SPI_SPEED: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_SPI_SPEED======");
									break;			
	case TRAN_ANC_IOC_CLEAR_FLAG: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_CLEAR_FLAG======");
									break;			          
	case TRAN_ANC_IOC_WAKE_LOCK: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_WAKE_LOCK======");
									pm_stay_awake(&tran_fp_datap->tran_fp_dev->dev);
									break;			           
	case TRAN_ANC_IOC_WAKE_UNLOCK: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_WAKE_UNLOCK======");
									pm_relax(&tran_fp_datap->tran_fp_dev->dev);
									break;			         
	case TRAN_ANC_IOC_REPORT_KEY_EVENT: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_REPORT_KEY_EVENT======");
									break;			    
	case TRAN_ANC_IOC_SET_PRODUCT_ID: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_SET_PRODUCT_ID======");
									break;			      
	case TRAN_ANC_IOC_GET_IRQ_PIN_LEVEL: 
									TRAN_FP_INFO(" ======TRAN_ANC_IOC_GET_IRQ_PIN_LEVEL======");
									break;			

									//BSP: add change for tee big core XLBELHQQS-9088 by xiaohong.huang 2022.08.17 start
	case TRAN_FP_TEE_AFINITY:
//									if (copy_from_user(&val, (int __user *)arg, sizeof(val)))
//										TRAN_FP_INFO(" TRAN_FP_TEE_AFINITY val = %x \n",val);
//									if(val) {
//										set_tee_worker_threads_on_big_core(true,tran_fp_datap->tran_tee_afinity);
//										tee_set_affinity();
//										TRAN_FP_ERROR("set_tee_worker_threads_on_big_core true \n");
//#ifdef TEE_VERSION_510
//										if(tran_fp_datap->tran_fp_sched == 1){
//											set_tee_worker_threads_Schedue(POLICY_HIGH);
//											TRAN_FP_INFO(" set_tee_worker_threads_Schedue POLICY_HIGH\n");
//										} else {
//											TRAN_FP_ERROR(" set_tee_worker_threads_Schedue disable\n");
//										}
//#endif
//									} else {
//										set_tee_worker_threads_on_big_core(false,0x00);
//										tee_set_affinity();
//										TRAN_FP_ERROR("set_tee_worker_threads_on_big_core false \n");
//#ifdef TEE_VERSION_510
//										if(tran_fp_datap->tran_fp_sched == 1)
//										{
//											set_tee_worker_threads_Schedue(POLICY_LOW);
//											TRAN_FP_INFO(" set_tee_worker_threads_Schedue POLICY_LOW\n");
//										}
//										else {
//											TRAN_FP_ERROR(" set_tee_worker_threads_Schedue disable\n");
//										}
//#endif
//									}
									break;
									//BSP: add change for tee big core XLBELHQQS-9088 by xiaohong.huang 2022.08.17 end
									//BSP: add catch log for fans XLBELHQQS-9088 by zheyi.bai 2023.02.14 start
	case TRAN_FP_OPENHAL_STATUS:
									if (copy_from_user(&val, (int __user *)arg, sizeof(val)))
										TRAN_FP_INFO(" TRAN_FP_OPENHAL_FLAG = %d \n",val);
									if(val)
									{
										tran_fp_datap->tran_openHal_flag = 1;
										TRAN_FP_INFO("Write openHal_flag\n");
									} else {
										if (copy_to_user((int __user *)arg, (int *)&tran_fp_datap->tran_openHal_flag, sizeof(int))) {
											TRAN_FP_ERROR("Failed to copy data to user\n");
											retval = -EFAULT;
											break;
										}
										TRAN_FP_INFO("Read  openHal_flag\n");
									}
									break;
									//BSP: add catch log for fans XLBELHQQS-9088 by zheyi.bai 2023.03.01 end
									//BSP: add read_fp_id for ota by zheyi.bai 20231123 start
	case TRAN_FP_READ_FP_ID:
									if(!tran_fp_datap->tran_opt_fp_support)
									{
										if((tran_read_fp_chip_id(tran_fp_datap)) < 0){
											tran_netlink_send(tran_fp_datap, TRAN_NETLINK_FP_ID_FAILED);
											TRAN_FP_ERROR(" tran_read_fp_chip_id failed");
										}
										else{
											tran_netlink_send(tran_fp_datap, TRAN_NETLINK_FP_ID_PASS);
											TRAN_FP_INFO(" tran_read_fp_chip_id pass");
										}
										break;
									}
									else
										TRAN_FP_INFO("This is opt fp");
									break;
	case TRAN_FP_OPT_SUPPORT:
									tran_fp_datap->tran_opt_fp_support = 1;
									TRAN_FP_INFO(" BZY opt fingerprint support");
									break;
									//BSP: add read_fp_id for ota by zheyi.bai 20231123 end
	default:
									TRAN_FP_ERROR("tran doesn't support this command(%#08x), _IOC_TYPE(cmd) = %c, _IOC_NR(cmd) = %#08x", cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
									break;
}
mutex_unlock(&tran_fp_datap->buf_lock);
TRAN_DEV_SEL_dev_put(tran_fp_dev);
return retval;
}

#ifdef CONFIG_COMPAT
static long tran_fp_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return tran_fp_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define tran_fp_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static unsigned int tran_fp_poll( struct file *filp, struct poll_table_struct *wait)
{
	unsigned int ret = 0;
	poll_wait(filp, &g_tran_fp_datap->wq_irq_return, wait);
	if (g_tran_fp_datap->cancel == 1) {
		TRAN_FP_INFO(" chipone cancle\n");
		ret =  POLLERR;
		g_tran_fp_datap->cancel = 0;
		return ret;
	} else if ( g_tran_fp_datap->RcvIRQ) {
		if (g_tran_fp_datap->RcvIRQ == 2) {
			TRAN_FP_INFO("chipone get fp on notify\n");
			ret |= POLLIN;
			ret |= POLLHUP;
		} else {
			TRAN_FP_INFO("chipone get irq\n");
			ret |= POLLIN;
			ret |= POLLRDNORM;
		}
	} else if ( g_tran_fp_datap->sign_sync) {
		if (g_tran_fp_datap->sign_sync == 1) {
			TRAN_FP_INFO(" cdfinger sign_sync 1\n");
			ret |= POLLIN|POLLPRI;
		} else if (g_tran_fp_datap->sign_sync == 2) {
			TRAN_FP_INFO(" cdfinger sign_sync 2\n");
			ret |= POLLOUT;
		}
		g_tran_fp_datap->sign_sync = 0;
	} else if ( g_tran_fp_datap->fps_ints.finger_on) {
		TRAN_FP_INFO("egis get irq\n");
		ret |= POLLIN;
		ret |= POLLRDNORM;
	} else if (g_tran_fp_datap->fps_ints.drdy_irq_abort == 1) {
		ret |= POLLFREE;
		g_tran_fp_datap->fps_ints.drdy_irq_abort = 0;
	} else {
		ret = 0;
	}
	return ret;
}

void tran_fasync_for_lcm_full_HBM(void)
{
	struct tran_fp_data *tran_fp_datap = g_tran_fp_datap;
	char* env_ext[2] = {"TRAN_FULL_HBM_EVENT=FULL_HBM_SET", NULL};
	if(!IS_ERR_OR_NULL(tran_fp_datap) && !IS_ERR_OR_NULL(tran_fp_datap->tran_fp_dev)){
		TRAN_FP_INFO("lcm start setting HBM and prepare to notify fp HAL by uevent");
		kobject_uevent_env(&tran_fp_datap->tran_fp_dev->dev.kobj, KOBJ_CHANGE, env_ext);
	}else{
		TRAN_FP_ERROR("tran_fp_datap->tran_fp_dev is null");
	}
	//kill_fasync(&tran_fp_datap->async_queue, sig, band);
}
EXPORT_SYMBOL(tran_fasync_for_lcm_full_HBM);
static int tran_fp_fasync(int fd, struct file *filp, int mode)
{
	struct tran_fp_data *tran_fp_datap = g_tran_fp_datap;

	TRAN_FP_INFO("enter, mode = %#08x", mode);
	return fasync_helper(fd, filp, mode, &tran_fp_datap->async_queue);
}
//BSP: add change proc fp to sys fp for HAL compatibility XLJOEI-309 by fan.peng 20200806 start
static ssize_t vendor_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t vendor_name_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t n);
static ssize_t ic_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t ic_name_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t n);
static ssize_t fod_location_xy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t fod_location_xy_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t n);
#define tran_attr(_name) \
	static struct kobj_attribute _name##_attr = {	\
		.attr	= {				\
			.name = __stringify(_name),	\
			.mode = 0664,			\
		},					\
		.show	= _name##_show,			\
		.store	= _name##_store,		\
	}

tran_attr(vendor_name);
tran_attr(ic_name);
tran_attr(fod_location_xy);

static struct attribute * g[] = {
	&vendor_name_attr.attr,
	&ic_name_attr.attr,
	&fod_location_xy_attr.attr,
	NULL,
};

static struct attribute_group vendor_attr_group = {
	.attrs = g,
};

static DEFINE_MUTEX(vendor_name_lock);
static DEFINE_MUTEX(ic_name_lock);

static ssize_t vendor_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *str = buf;

	mutex_lock(&vendor_name_lock);
	str += sprintf(str, "%s\n", g_tran_fp_datap->modalias);
	//    *str++ = '\n';
	mutex_unlock(&vendor_name_lock);
	return (str - buf);
}

static ssize_t vendor_name_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t n)
{
	mutex_lock(&vendor_name_lock);
	TRAN_FP_INFO("[%s]has not been implemented.\n", __func__);
	mutex_unlock(&vendor_name_lock);
	return n;
}
//BSP: add change proc fp to sys fp for HAL compatibility XLJOEI-309 by fan.peng 20200806 end
static ssize_t ic_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *str = buf;
	mutex_lock(&ic_name_lock);
	str += sprintf(str, "%s\n", g_tran_fp_datap->ic_name);
	//    *str++ = '\n';
	mutex_unlock(&ic_name_lock);
	return (str - buf);
}
static ssize_t ic_name_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t n)
{
	mutex_lock(&ic_name_lock);
	TRAN_FP_INFO("[%s]has not been implemented.\n", __func__);
	mutex_unlock(&ic_name_lock);
	return n;
}
static ssize_t fod_location_xy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *str = buf;
	mutex_lock(&ic_name_lock);
	str += sprintf(str, "%s", g_tran_fp_datap->fod_location_xy);
	mutex_unlock(&ic_name_lock);
	return (str - buf);
}
static ssize_t fod_location_xy_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t n)
{
	mutex_lock(&ic_name_lock);
	TRAN_FP_INFO("[%s]has not been implemented.\n", __func__);
	mutex_unlock(&ic_name_lock);
	return n;
}


static int tran_fp_open(struct inode *inode, struct file *filp)
{
	struct tran_fp_data	*tran_fp_datap;
	int	status = -ENXIO;
	TRAN_FP_INFO("entered!");

	mutex_lock(&device_list_lock);
	list_for_each_entry(tran_fp_datap, &device_list, device_entry)
	{
		if (tran_fp_datap->devno == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	if (status == 0) {
		if (!tran_fp_datap->tran_buffer) {
			tran_fp_datap->tran_buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (!tran_fp_datap->tran_buffer) {
				TRAN_FP_ERROR("open no memory\n");
				status = -ENOMEM;
			}
		}
		if (status == 0) {
			tran_fp_datap->users++;
			tran_fp_datap->device_available = 1;
			filp->private_data = tran_fp_datap;
			nonseekable_open(inode, filp);
		}

	} else
		TRAN_FP_INFO("tran_fp_datap: nothing for minor %d\n", iminor(inode));
	mutex_unlock(&device_list_lock);
	return status;
}

static int tran_fp_release(struct inode *inode, struct file *filp)
{
	struct tran_fp_data	*tran_fp_datap;
	int			status = 0;

	TRAN_FP_INFO("entered!");
	mutex_lock(&device_list_lock);
	tran_fp_datap = filp->private_data;
	filp->private_data = NULL;
	//tran_fp_datap->spi_clk_off(tran_fp_datap);

	//BSP:modify fix resource release bug XLLLLWSHW-56 by fan.peng start
	/* last close? */
	tran_fp_datap->users--;
	if (tran_fp_datap->users <= 0) {
		tran_fp_datap->disable_irq(tran_fp_datap);
		if (tran_fp_datap->tran_buffer) {
			kfree(tran_fp_datap->tran_buffer);
			tran_fp_datap->tran_buffer = NULL;
		}
		/* ... after we unbound from the underlying device? */
	}
	tran_fp_datap->device_available = 0;
	//BSP:modify fix resource release bug XLLLLWSHW-56 by fan.peng end
	mutex_unlock(&device_list_lock);
	if (g_fp_name_flg == FOCALTECH)
		status = tran_fp_fasync(-1, filp, 0);
	return status;
}

static const struct file_operations tran_fp_fops = {
	.owner =	THIS_MODULE,
	.unlocked_ioctl = tran_fp_ioctl,
	.compat_ioctl = tran_fp_compat_ioctl,
	.open =		tran_fp_open,
	.release =	tran_fp_release,
	.poll = tran_fp_poll,
	.fasync = tran_fp_fasync,
};


#ifdef CONFIG_OF
static const struct of_device_id tran_of_plat_match[] = {
	{.compatible = "tran_fp_legacy",},
	{},
};
MODULE_DEVICE_TABLE(of, tran_of_plat_match);
#endif

static int  tran_fp_probe(struct TRAN_DEV_SEL_device *tran_fp_dev)
{
	struct tran_fp_data	*tran_fp_datap;
	int	status = 0;
	unsigned char *chip_id_str = NULL;
	PWR_VAL_U power_val;


	TRAN_FP_INFO("enter");
	tran_fp_datap = kzalloc(sizeof(*tran_fp_datap), GFP_KERNEL);
	if (!tran_fp_datap) {
		TRAN_FP_ERROR("no enough mem for tran_fp_datap!");
		status = -ENOMEM;
		goto tran_kzalloc_err;
	}

	g_tran_fp_datap = tran_fp_datap;
	tran_fp_datap->tran_fp_dev = tran_fp_dev;
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
	tran_fp_datap->tran_fp_dev->mode = SPI_MODE_0;
	tran_fp_datap->tran_fp_dev->bits_per_word = 8;
	tran_fp_datap->tran_fp_dev->max_speed_hz = 4 * 1000 * 1000;
	//BSP: delete for fix timing sequence issue XLLLHLWS-309 by fan.peng 20201106 start
	/*
	   if(spi_setup(tran_fp_datap->tran_fp_dev) != 0)
	   {
	   TRAN_FP_ERROR(" spi setup failed!\n");
	   goto spi_setup_fail;
	   }
	   */
	//BSP: delete for fix timing sequence issue XLLLHLWS-309 by fan.peng 20201106 end
#endif
	tran_fp_datap->miscdev.minor = TRAN_FP_STATIC_MINOR;
	tran_fp_datap->miscdev.fops = &tran_fp_fops;

	if (tran_fp_datap->tran_buffer == NULL) {
		tran_fp_datap->tran_buffer = kzalloc(bufsiz, GFP_KERNEL);
		if (tran_fp_datap->tran_buffer == NULL) {
			status = -ENOMEM;
			goto err_buf;
		}
	}
	status = tran_fp_parse_dts_file(tran_fp_datap);
	if (status < 0) {
		TRAN_FP_ERROR(" tran_fp_parse_dts_file failed");
		goto dts_parse_err;
	}

	tran_register_base_ops(tran_fp_datap);
	status = tran_fp_datap->resource_init(tran_fp_datap);
	if (status < 0) { 
		TRAN_FP_ERROR("resource_init failed! status = %d", status);
		goto resource_init_err;
	}
	if(tran_fp_datap->fp_regulator_support) {
		status = tran_fp_datap->power_init(tran_fp_datap, true);
		if (status < 0) { 
			TRAN_FP_ERROR("power init failed! err=%d", status);
			goto tran_pwr_init_err;
		}

		status = tran_fp_datap->power_onoff(tran_fp_datap, true);
		if (status < 0) {
			TRAN_FP_ERROR("power on failed! err=%d\n", status);
			goto tran_pwr_onoff_err;
		}
		mdelay(10);
		status = tran_fp_get_power_val(tran_fp_datap, &power_val);
	}
	if(IS_ERR(tran_fp_datap->pins_spi_default)){
		TRAN_FP_ERROR("pinctrl_lookup_state spi_default failed!");
	} else {
		//BSP: add change fp for CKBNHJSY-5 by xiaohong.huang 2023.11.16 start
		status = pinctrl_select_state(tran_fp_datap->tran_pinctrl, tran_fp_datap->pins_spi_default);
		if(status) {
			TRAN_FP_ERROR("[tran]cannot set spi default state, ret = %d \n", status);
		} else
			TRAN_FP_INFO("[tran] set spi default state success!");
	}
	//BSP: add change fp for CKBNHJSY-5 by xiaohong.huang 2023.11.16 end
	tran_fp_datap->spi_clk_on(tran_fp_datap);

	//tran_fp_datap->reset(tran_fp_datap, TRAN_INIT_INTER_CHIP_RESET_DELAY, TRAN_INIT_POST_CHIP_RESET_DELAY);

	status = tran_get_fp_sensor_chip_id(tran_fp_datap, chip_id_str);
	if (status < 0) {
		TRAN_FP_ERROR(" tran_get_fp_sensor_chip_id failed");
		tranlog_local("USD_FP_CHECK","HW_FP Check ID Fail In Init");
		goto get_chip_id_err;
	}
	tranlog_local("USD_FP_CHECK","HW_FP Check ID Pass In Init");

	//BSP: add change proc fp to sys fp for HAL compatibility XLJOEI-309 by fan.peng 20200806 start
	fp_sys_kobj = kobject_create_and_add("tran_fp", kernel_kobj);
	if (!fp_sys_kobj) {
		TRAN_FP_ERROR("%s, Failed to create /sys/kernel/tran_fp dir\n", __func__);
		goto sys_dir_tran_fp_create_err;
	}
	status = sysfs_create_group(fp_sys_kobj, &vendor_attr_group);
	if (status) {
		TRAN_FP_ERROR("%s, Failed to create /sys/kernel/tran_fp/vendor_name node\n", __func__);
		goto sys_fp_node_create_err;
	}
	//BSP: add change proc fp to sys fp for HAL compatibility XLJOEI-309 by fan.peng 20200806 end
	status = misc_register(&tran_fp_datap->miscdev);
	if (status < 0) {
		TRAN_FP_ERROR(" misc_register failed");
		goto misc_register_err;
	}
	if (status == 0) {
		mutex_lock(&device_list_lock);
		tran_fp_datap->devno = MKDEV(MISC_MAJOR, tran_fp_datap->miscdev.minor);
		list_add(&tran_fp_datap->device_entry, &device_list);
		mutex_unlock(&device_list_lock);
	}

	if (tran_fp_attribute_group.attrs != NULL) {
		if (g_fp_name_flg == SUNWAVE) {
			status = sysfs_create_group(&tran_fp_datap->miscdev.this_device->kobj, &tran_fp_attribute_group);
		} else if (g_fp_name_flg == GOODIX) {
			status = sysfs_create_group(&tran_fp_datap->tran_fp_dev->dev.kobj, &tran_fp_attribute_group);
		} else if (g_fp_name_flg == FPC) {
			status = sysfs_create_group(&tran_fp_datap->tran_fp_plat_dev->dev.kobj, &tran_fp_attribute_group);
		}
		if (status < 0) {
			TRAN_FP_ERROR(" sysfs_create_group failed");
			goto sysfs_group_create_err;
		}
	}

	status = tran_proc_init(tran_fp_datap);
	if (status < 0) {
		TRAN_FP_ERROR(" tran_proc_init failed");
		goto proc_init_err;
	}

	status = tran_fp_input_init();
	if (status < 0) {
		TRAN_FP_ERROR(" tran_fp_input_init failed");
		goto input_init_err;
	}

	status = tran_fp_irq_init();
	if (status < 0) {
		TRAN_FP_ERROR(" tran_fp_irq_init failed");
		goto irq_init_err;
	}

	status = tran_netlink_init(tran_fp_datap);
	if (status < 0) {
		TRAN_FP_ERROR(" tran_netlink_init failed");
		goto netlink_init_err;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&tran_fp_datap->early_suspend);
#else
	//fb_register_client(&tran_fp_datap->notifier);
	mtk_disp_notifier_register("FINGERPRINT", &tran_fp_datap->notifier);
#endif
	//app_get_fingerprint_name(tran_fp_datap->modalias);
	tran_fp_datap->lcm_onoff_registered = true;
	tran_fp_datap->probe_finish = 1;
	tran_fp_datap->is_sleep_mode = 0;
	tran_fp_datap->device_available = 1;
	tran_fp_datap->wakeup_enabled = false;
	tran_fp_datap->fpc_fp_exist = true;
	atomic_set(&tran_fp_datap->init,0);

	if (status == 0) {
		TRAN_FP_INFO("%s success! \n", __func__);
		TRAN_DEV_SEL_set_drvdata(tran_fp_dev, tran_fp_datap);
		tran_fp_datap->spi_clk_off(tran_fp_datap);
	} else {
		//BSP:modify [fix resource release bug] XLLLLWSHW-56 by fan.peng 20191109 start
		tran_fp_datap->shutdown(tran_fp_datap);
netlink_init_err:
		if (tran_fp_datap->irq && g_tran_fp_req_irq_times == 1) {
			free_irq(tran_fp_datap->irq,tran_fp_datap);
			tran_fp_datap->irq_count = 0;
			g_tran_fp_req_irq_times = 0;
		}
irq_init_err:
		input_unregister_device(tran_fp_datap->input);
		tran_fp_datap->input = NULL;
input_init_err:
		tran_proc_deinit(tran_fp_datap);
proc_init_err:
		if (g_fp_name_flg == SUNWAVE) {
			sysfs_remove_group(&tran_fp_datap->miscdev.this_device->kobj, &tran_fp_attribute_group);
		} else if (g_fp_name_flg == GOODIX) {
			sysfs_remove_group(&tran_fp_datap->tran_fp_dev->dev.kobj, &tran_fp_attribute_group);

		} else if (g_fp_name_flg == FPC) {
			sysfs_remove_group(&tran_fp_datap->tran_fp_plat_dev->dev.kobj, &tran_fp_attribute_group);
		}
sysfs_group_create_err:
		mutex_lock(&device_list_lock);
		list_del(&tran_fp_datap->device_entry);
		mutex_unlock(&device_list_lock);
		misc_deregister(&tran_fp_datap->miscdev);

misc_register_err:
		sysfs_remove_group(fp_sys_kobj, &vendor_attr_group);
sys_fp_node_create_err:
		kobject_del(fp_sys_kobj);
sys_dir_tran_fp_create_err:
		if (g_fp_name_flg == CDFINGER) {
			kthread_stop(tran_fp_datap->finger_thread);
			del_timer(&tran_fp_datap->int_timer);
		} else if (g_fp_name_flg == FPC) {

			//platform_device_unregister(tran_fp_datap->tran_fp_plat_dev);
		}
get_chip_id_err:
		tran_fp_datap->spi_clk_off(tran_fp_datap);
dts_parse_err:
		if (tran_fp_datap->tran_pinctrl) {
			devm_pinctrl_put(tran_fp_datap->tran_pinctrl);
			tran_fp_datap->tran_pinctrl = NULL;
		}
#ifndef CONFIG_TRAN_FP_DRV_MODE_SPI
		devm_clk_put(&tran_fp_datap->tran_fp_dev->dev, tran_fp_datap->clk);
#endif
		tran_fp_datap->power_onoff(tran_fp_datap, false);
tran_pwr_onoff_err:
		tran_fp_datap->power_init(tran_fp_datap, false);
		destroy_workqueue(tran_fp_datap->wqueue);
#ifdef CONFIG_PM_SLEEP
		wakeup_source_unregister(tran_fp_datap->tran_wakelock);
#else
		wake_lock_destroy(&tran_fp_datap->tran_wakelock);
#endif
		mutex_destroy(&tran_fp_datap->buf_lock);
		mutex_destroy(&tran_fp_datap->release_lock);

		//BSP:modify [fix resource release bug] XLLLLWSHW-56 by fan.peng 20191109 end
tran_pwr_init_err:
resource_init_err:
		kfree(tran_fp_datap->tran_buffer);
		tran_fp_datap->tran_buffer = NULL;
err_buf:
		//BSP: delete for fix timing sequence issue XLLLHLWS-309 by fan.peng 20201106 start
		//#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
		//spi_setup_fail:
		//#endif
		//BSP: delete for fix timing sequence issue XLLLHLWS-309 by fan.peng 20201106 end
		kfree(tran_fp_datap);
		tran_fp_datap = NULL;
		g_tran_fp_datap = NULL;
	}
tran_kzalloc_err:
	return status;

}

static void  tran_fp_remove(struct TRAN_DEV_SEL_device * spi)
{
	struct tran_fp_data  *tran_fp_datap = TRAN_DEV_SEL_get_drvdata(spi);
	PWR_VAL_U power_val;
	int status = 0;
	TRAN_FP_INFO(" entered!\n");
	/* make sure ops on existing fds can abort cleanly */
	tran_fp_datap->shutdown(tran_fp_datap);
	tran_netlink_destroy(tran_fp_datap);
	if (tran_fp_datap->irq && g_tran_fp_req_irq_times == 1) {
		free_irq(tran_fp_datap->irq,tran_fp_datap);
		tran_fp_datap->irq_count = 0;
		g_tran_fp_req_irq_times = 0;
	}
	input_unregister_device(tran_fp_datap->input);
	tran_fp_datap->input = NULL;
	tran_proc_deinit(tran_fp_datap);
	sysfs_remove_group(&tran_fp_datap->miscdev.this_device->kobj, &tran_fp_attribute_group);
	misc_deregister(&tran_fp_datap->miscdev);
	sysfs_remove_group(fp_sys_kobj, &vendor_attr_group);
	kobject_del(fp_sys_kobj);
	devm_pinctrl_put(tran_fp_datap->tran_pinctrl);
	tran_fp_datap->tran_pinctrl = NULL;
#ifndef CONFIG_TRAN_FP_DRV_MODE_SPI
	devm_clk_put(&tran_fp_datap->tran_fp_dev->dev, tran_fp_datap->clk);
#endif
	spin_lock_irq(&tran_fp_datap->spi_lock);
	tran_fp_datap->tran_fp_dev = NULL;
	TRAN_DEV_SEL_set_drvdata(spi, NULL);
	spin_unlock_irq(&tran_fp_datap->spi_lock);
	status = tran_fp_datap->power_onoff(tran_fp_datap, false);
	if (status < 0) 
		TRAN_FP_ERROR("power off failed! err=%d\n", status);
	status = tran_fp_get_power_val(tran_fp_datap, &power_val);
	status = tran_fp_datap->power_init(tran_fp_datap, false);
	if (status < 0) 
		TRAN_FP_ERROR("power deinit failed! err=%d", status);
	/* prevent new opens */
	if (tran_fp_datap->wqueue) {
		destroy_workqueue(tran_fp_datap->wqueue);
		tran_fp_datap->wqueue = NULL;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	if (tran_fp_datap->early_suspend.suspend) {
		unregister_early_suspend(&tran_fp_datap->early_suspend);
	}
#else
	fb_unregister_client(&tran_fp_datap->notifier);
#endif /* CONFIG_HAS_EARLYSUSPEND */
#ifdef CONFIG_PM_SLEEP
	wakeup_source_unregister(tran_fp_datap->tran_wakelock);
	wakeup_source_unregister(tran_fp_datap->wakelock_hal);
#else
	wake_lock_destroy(&tran_fp_datap->tran_wakelock);
	wake_lock_destroy(&tran_fp_datap->wakelock_hal);
#endif
	mutex_destroy(&tran_fp_datap->buf_lock);
	mutex_destroy(&tran_fp_datap->release_lock);
	mutex_lock(&device_list_lock);
	list_del(&tran_fp_datap->device_entry);
	clear_bit(MINOR(tran_fp_datap->devno), minors);
	mutex_unlock(&device_list_lock);
	//tran_fp_free_gpios(tran_fp_datap);
	if (g_fp_name_flg == EGIS) {
		del_timer_sync(&tran_fp_datap->fps_ints.timer);
	}
	kfree(tran_fp_datap->tran_buffer);
	tran_fp_datap->tran_buffer = NULL;
	kfree(tran_fp_datap);
	tran_fp_datap = NULL;
}

#ifdef CONFIG_PM_SLEEP
static int tran_fp_suspend(struct device *spi)
{
	//struct tran_fp_data *tran_fp_datap = dev_get_drvdata(spi);
	//enable_irq_wake(tran_fp_datap->irq);
	TRAN_FP_INFO(" entering ");
	return 0;
}

static int tran_fp_resume(struct device *spi)
{
	//struct tran_fp_data  *tran_fp_datap = dev_get_drvdata(spi);
	//disable_irq_wake(tran_fp_datap->irq);
	TRAN_FP_INFO(" entering ");
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(tran_fp_pm_ops, tran_fp_suspend, tran_fp_resume);

#ifdef CONFIG_OF
static const struct of_device_id tran_of_match[] = {
	{.compatible = "tran_fp_legacy",},
	{},
};
MODULE_DEVICE_TABLE(of, tran_of_match);
#endif

static const struct TRAN_DEV_SEL_device_id tran_id_table[] = {
	{.name = "tran_fp_legacy",},
	{},
};

static struct TRAN_DEV_SEL_driver tran_fp_driver = {
	.driver = {
		.name =		"tran_fp_legacy",
		.owner =	THIS_MODULE,
		.pm 	= &tran_fp_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = tran_of_match,
#endif
	},
	.probe =	tran_fp_probe,
	.remove =	tran_fp_remove,
	.id_table =     tran_id_table,
};

static int __init tran_fp_init(void)
{
	int status = 0;
	TRAN_FP_INFO("entered!");
	tran_register_board_info();
	status = TRAN_DEV_SEL_register_driver(&tran_fp_driver);
	if (status < 0) {
		TRAN_FP_ERROR("tran platform/spi driver register failed!");
		return status;
	}

	return status;
}

static void __exit tran_fp_exit(void)
{
	TRAN_FP_INFO("entered!");
	TRAN_DEV_SEL_unregister_driver(&tran_fp_driver);
}

late_initcall(tran_fp_init);
module_exit(tran_fp_exit);

MODULE_DESCRIPTION("User mode SPI device interface");
MODULE_LICENSE("GPL");
