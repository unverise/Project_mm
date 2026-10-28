#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/spi/spi.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>

#include "cdfinger_tee.h"

#ifdef CONFIG_SPI_MT65XX_KERNEL510
#include "tran_spi.h"
#endif

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifdef CONFIG_SPI_MT65XX_KERNEL510
#include <linux/platform_data/spi-mt65xx.h>
extern struct mtk_chip_config tran_chip_config;
#else
extern struct mt_chip_conf tran_chip_config;
#endif
#endif

#define TRAN_CDFINGER_REPORT_KEY CDFINGER_REPORT_KEY

static DECLARE_WAIT_QUEUE_HEAD(waiter);

extern int tran_fp_spi_transfer(void *chip_conf, int cfg_len, const char *txbuf, char *rxbuf, int len, const char *txbuf1, char *rxbuf1, int len1);
extern struct tran_fp_data *g_tran_fp_datap;
extern int g_tran_fp_req_irq_times;

int cdf_spi_transfer(unsigned char *chip_id_str)
{
    uint8_t tx[] = {0x74, 0x66, 0x66, 0x66};
    uint8_t work_reg = 0x18;
    uint8_t reg_ret = 0;
    int cfg_len;
    cfg_len = sizeof(struct mt_chip_conf);
    g_tran_fp_datap->reset(g_tran_fp_datap, 1000, 2000);

    tran_fp_spi_transfer(&tran_chip_config, cfg_len, &work_reg, &reg_ret, 1, NULL, NULL, 0);
    mdelay(1);
    TRAN_FP_INFO(" work mode(%#2x)\n",reg_ret);
    tran_fp_spi_transfer(&tran_chip_config, cfg_len, tx, tx, 4, NULL, NULL, 0);
    TRAN_FP_INFO(" rx_test fp id:%#2x %#2x %#2x %#2x \n", tx[0],tx[1],tx[2],tx[3]);
    if (reg_ret == 1)
    	memcpy(chip_id_str, tx, 4);
    return 0;
}

static void cdfinger_async_Report(void)
{
    struct tran_fp_data *tran_fp_datap = g_tran_fp_datap;
    TRAN_FP_DEBUG("enter\n");
    kill_fasync(&tran_fp_datap->async_queue, SIGIO, POLL_IN);
    TRAN_FP_DEBUG("exit\n");
}

void cdfinger_release_wakelock(struct tran_fp_data *tran_fp_datap)
{
    TRAN_FP_DEBUG("enter\n");
#ifdef CONFIG_PM_SLEEP
    __pm_relax(tran_fp_datap->tran_wakelock);
#else
    wake_unlock(&tran_fp_datap->tran_wakelock);
#endif
    TRAN_FP_DEBUG("exit\n");
}

int cdfinger_mode_init(struct tran_fp_data *tran_fp_datap, uint8_t arg, enum work_mode mode)
{
    TRAN_FP_INFO("mode=0x%x\n", mode);
    tran_fp_datap->process_interrupt = 1;
    tran_fp_datap->device_mode = mode;
    tran_fp_datap->key_report = 0;

    return 0;
}

void cdfinger_wake_lock(struct tran_fp_data *tran_fp_datap, int arg)
{
    TRAN_FP_DEBUG("cdfinger_wake_lock enter----------\n");
    if(arg)
    {
#ifdef CONFIG_PM_SLEEP
        __pm_stay_awake(tran_fp_datap->tran_wakelock);
#else
        wake_lock(&tran_fp_datap->tran_wakelock);
#endif
    }
    else
    {
#ifdef CONFIG_PM_SLEEP
        __pm_relax(tran_fp_datap->tran_wakelock);
        //__pm_wakeup_event(tran_fp_datap->tran_wakelock, jiffies_to_msecs(3*HZ));
#else
        wake_unlock(&tran_fp_datap->tran_wakelock);
        //wake_lock_timeout(&tran_fp_datap->tran_wakelock, 3*HZ);
#endif
    }
}

int cdf_key_nav_adjust_type_value(tran_key_nav_event_t *key_nav_p, unsigned int cmd)
{
    if ((unsigned int)(*key_nav_p) == KEY_UP)
	    *key_nav_p = TRAN_KEY_NAV_UP;
    else if ((unsigned int)(*key_nav_p) == KEY_DOWN)
	    *key_nav_p = TRAN_KEY_NAV_DOWN;
    else if ((unsigned int)(*key_nav_p) == KEY_RIGHT)
	    *key_nav_p = TRAN_KEY_NAV_RIGHT;
    else if ((unsigned int)(*key_nav_p) == KEY_LEFT)
	    *key_nav_p = TRAN_KEY_NAV_LEFT;
	else if ((unsigned int)(*key_nav_p) == KEY_F28) {
	    	*key_nav_p = TRAN_KEY_NAV_F28;
	}

	switch(cmd)
	{
	    case TRAN_CDFINGER_REPORT_KEY:
		switch(*key_nav_p)
		{
		case TRAN_KEY_NAV_UP:
			*key_nav_p = TRAN_KEY_NAV_NONE;
			break;
		case TRAN_KEY_NAV_DOWN:
			*key_nav_p = TRAN_KEY_NAV_NONE;
			break;
		case TRAN_KEY_NAV_RIGHT:
			*key_nav_p = TRAN_KEY_NAV_PAGEUP;
			break;
		case TRAN_KEY_NAV_LEFT:
			*key_nav_p = TRAN_KEY_NAV_PAGEDOWN;
			break;
		default:
			break;
		}
		break;
	    default:
		break;
	}
	return 0;
}

int cdfinger_report_key_legacy(struct tran_fp_data *tran_fp_datap, uint8_t arg)
{
    input_report_key(tran_fp_datap->input, KEY_INTERRUPT, !!arg);
    input_sync(tran_fp_datap->input);
    return 0;
}

int cdfinger_customize_input_init(struct tran_fp_data *tran_fp_datap)
{
    __set_bit(KEY_INTERRUPT, tran_fp_datap->input->keybit);
    __set_bit(KEY_F1, tran_fp_datap->input->keybit);
    __set_bit(KEY_F2, tran_fp_datap->input->keybit);
    __set_bit(KEY_F3, tran_fp_datap->input->keybit);
    __set_bit(KEY_F4, tran_fp_datap->input->keybit);
    __set_bit(KEY_PAGEUP, tran_fp_datap->input->keybit);
    __set_bit(KEY_PAGEDOWN, tran_fp_datap->input->keybit);

    tran_fp_datap->input->id.bustype = BUS_HOST;
    return 0;
}

//void int_timer_handle(unsigned long arg) //kernel-4.14 use this api, not fits for kernel-4.19
void int_timer_handle(struct timer_list *tlp)
{
    struct tran_fp_data *tran_fp_datap = g_tran_fp_datap;

    TRAN_FP_DEBUG("enter\n");
    if ((tran_fp_datap->device_mode == FINGER_KEY_MODE) && (tran_fp_datap->key_report == 1)) {
        input_report_key(tran_fp_datap->input, KEY_INTERRUPT, 0);
        input_sync(tran_fp_datap->input);
        tran_fp_datap->key_report = 0;
    }

    if (tran_fp_datap->device_mode == FINGER_FINGER_UP_MODE){
        tran_fp_datap->process_interrupt = 0;
        cdfinger_async_Report();
    }
    TRAN_FP_DEBUG("exit\n");
}

//static irqreturn_t cdfinger_interrupt_handler(unsigned irq, void *arg)//kernel-4.14 use this api, not fits for kernel-4.19
static irqreturn_t cdfinger_interrupt_handler(int irq, void *arg)
{
    struct tran_fp_data *tran_fp_datap = (struct tran_fp_data *)arg;

    if (tran_fp_datap->process_interrupt == 1)
    {
        mod_timer(&tran_fp_datap->int_timer, jiffies + HZ / 10);
        tran_fp_datap->thread_wakeup = 1;
        wake_up_interruptible(&waiter);
    }

    return IRQ_HANDLED;
}

int cdf_fp_irq_init(void)
{
	int ret = 0;
	unsigned long irq_flags = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
    TRAN_FP_INFO("g_tran_fp_datap->irq = %d \n", g_tran_fp_datap->irq);
	if ((ret = request_threaded_irq(g_tran_fp_datap->irq, cdfinger_interrupt_handler, NULL, irq_flags, "cdfinger-irq", g_tran_fp_datap)) != 0) {
    TRAN_FP_ERROR("request_irq failed ret = %d \n", ret);
    goto request_irq_fail;
   } else {
	g_tran_fp_req_irq_times = 1;
   }
	ret = enable_irq_wake(g_tran_fp_datap->irq);
        if (ret < 0 ) {
            TRAN_FP_ERROR(" enable_irq_wake failed , ret = %d \n", ret);
	    if (g_tran_fp_datap->irq && g_tran_fp_req_irq_times == 1) {
	    	free_irq(g_tran_fp_datap->irq, g_tran_fp_datap);
		g_tran_fp_req_irq_times = 0;
	    }
        }
request_irq_fail:
	return ret;
}

int cdfinger_thread_func(void *arg)
{
    struct tran_fp_data *tran_fp_datap = (struct tran_fp_data *)arg;

    do {
        wait_event_interruptible(waiter, tran_fp_datap->thread_wakeup != 0);
        TRAN_FP_INFO("cdfinger:thread wakeup\n");
        tran_fp_datap->thread_wakeup = 0;
#ifdef CONFIG_PM_SLEEP
        __pm_wakeup_event(tran_fp_datap->tran_wakelock, jiffies_to_msecs(3*HZ));
#else
        wake_lock_timeout(&tran_fp_datap->tran_wakelock, 3*HZ);
#endif
        if (tran_fp_datap->device_mode == FINGER_INTERRUPT_MODE) {
            tran_fp_datap->process_interrupt = 0;
            tran_fp_datap->sign_sync = 1;
            wake_up_interruptible(&tran_fp_datap->wq_irq_return);
            cdfinger_async_Report();
            del_timer_sync(&tran_fp_datap->int_timer);
            continue;
        } else if ((tran_fp_datap->device_mode == FINGER_KEY_MODE) && (tran_fp_datap->key_report == 0)) {
            input_report_key(tran_fp_datap->input, KEY_INTERRUPT, 1);
            input_sync(tran_fp_datap->input);
            tran_fp_datap->key_report = 1;
        }

    }while(!kthread_should_stop());

    TRAN_FP_DEBUG("thread exit\n");
    return -1;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
void cdf_early_suspend(struct early_suspend *es)
{
    TRAN_FP_DEBUG(" enter\n");
}

void cdf_late_resume(struct early_suspend *es)
{
    TRAN_FP_DEBUG(" enter\n");
}
#else
int cdf_fb_notifier_callback(struct notifier_block *notif, unsigned long event, void *data)
{
    int *blank = (int *)data;
    int retval = 0;
    struct tran_fp_data *tran_fp_datap = container_of(notif, struct tran_fp_data, notifier);
    //struct fb_event* evdata = data;

    if (event != MTK_DISP_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
        return 0;
    }
    //blank = (int*)evdata->data;
    switch (*blank) {
        case MTK_DISP_BLANK_UNBLANK:
            TRAN_FP_INFO("==FB_BLANK_UNBLANK==\n");
            mutex_lock(&tran_fp_datap->buf_lock);
            tran_fp_datap->screen_status = 1;
            if (tran_fp_datap->isInKeyMode == 0) {
                tran_fp_datap->sign_sync = 1;
                wake_up_interruptible(&tran_fp_datap->wq_irq_return);
                cdfinger_async_Report();
            }
            mutex_unlock(&tran_fp_datap->buf_lock);
            break;

        case MTK_DISP_BLANK_POWERDOWN:
            TRAN_FP_INFO("==FB_BLANK_POWERDOWN==\n");
            mutex_lock(&tran_fp_datap->buf_lock);
            tran_fp_datap->screen_status = 0;
            if (tran_fp_datap->isInKeyMode == 0) {
                tran_fp_datap->sign_sync = 1;
                wake_up_interruptible(&tran_fp_datap->wq_irq_return);
                cdfinger_async_Report();
            }
            mutex_unlock(&tran_fp_datap->buf_lock);
            break;
        default:
            break;
    }

    return retval;
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

int cdf_proc_init(struct tran_fp_data *tran_fp_dev)
{
    return 0;

}

void cdf_proc_deinit(struct tran_fp_data *tran_fp_dev)
{
    return;
}
