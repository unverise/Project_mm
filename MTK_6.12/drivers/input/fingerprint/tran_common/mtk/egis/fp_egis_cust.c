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

#include "egis_fp.h"

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

#define TRAN_EGIS_SEND_NAVI_EVENT EGIS_SEND_NAVI_EVENT

static DECLARE_WAIT_QUEUE_HEAD(waiter);

extern int tran_fp_spi_transfer(void *chip_conf, int cfg_len, const char *txbuf, char *rxbuf, int len, const char *txbuf1, char *rxbuf1, int len1);
extern struct tran_fp_data *g_tran_fp_datap;
extern int g_tran_fp_req_irq_times;

int egf_spi_transfer(unsigned char *chip_id_str)
{
    uint8_t tx[] = {0x20, 0x00, 0x00, 0x00};
    int cfg_len;
    cfg_len = sizeof(struct mt_chip_conf);
    g_tran_fp_datap->reset(g_tran_fp_datap, 3000, 12000);

    tran_fp_spi_transfer(&tran_chip_config, cfg_len, tx, tx, 4, NULL, NULL, 0);
    TRAN_FP_INFO(" rx_test fp id:%#2x %#2x %#2x %#2x \n", tx[0],tx[1],tx[2],tx[3]);
    memcpy(chip_id_str, tx + 2, 2);
    return 0;
}

irqreturn_t fp_eint_func(int irq, void *dev_id)
{
	struct tran_fp_data *tran_fp_datap = dev_id;
	if (!tran_fp_datap->fps_ints.int_count)
	    mod_timer(&tran_fp_datap->fps_ints.timer, jiffies + msecs_to_jiffies(tran_fp_datap->fps_ints.detect_period));
	tran_fp_datap->fps_ints.int_count++;
	TRAN_FP_INFO(" fps_ints.int_count=%d ", tran_fp_datap->fps_ints.int_count);
	pm_wakeup_event(&tran_fp_datap->tran_fp_dev->dev, WAKE_HOLD_TIME);
	return IRQ_HANDLED;
}

irqreturn_t fp_eint_func_ll(int irq, void *dev_id)
{
	struct tran_fp_data *tran_fp_datap = dev_id;
	unsigned long irqflags;
	TRAN_FP_INFO(" enter ");
	spin_lock_irqsave(&tran_fp_datap->irq_lock, irqflags);
	if (tran_fp_datap->fps_ints.drdy_irq_flag == DRDY_IRQ_ENABLE)
	{
	    tran_fp_datap->fps_ints.finger_on = 1;
	    disable_irq_nosync(tran_fp_datap->irq);
	    TRAN_FP_INFO(" DRDY_IRQ_DISABLE ");
	    tran_fp_datap->fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;
	}
	spin_unlock_irqrestore(&tran_fp_datap->irq_lock, irqflags);
	wake_up_interruptible(&tran_fp_datap->wq_irq_return);
	pm_wakeup_event(&tran_fp_datap->tran_fp_dev->dev, WAKE_HOLD_TIME);
	return IRQ_RETVAL(IRQ_HANDLED);
}

void egisfp_interrupt_timer_call(struct timer_list *t)
{
	struct tran_fp_interrupt_desc_t *fps_int = container_of(t, struct tran_fp_interrupt_desc_t, timer);
	struct tran_fp_data *tran_fp_datap;

	tran_fp_datap = container_of(fps_int, struct tran_fp_data, fps_ints);
	TRAN_FP_INFO(" interrupt count = %d \n", tran_fp_datap->fps_ints.int_count);

	if (tran_fp_datap->fps_ints.int_count >= tran_fp_datap->fps_ints.detect_threshold)
	{
		tran_fp_datap->fps_ints.finger_on = 1;
		TRAN_FP_INFO(" interrupt triggered ");
	}
	else
	{
		TRAN_FP_INFO(" interrupt not triggered ");
	}
	tran_fp_datap->fps_ints.int_count = 0;
	wake_up_interruptible(&tran_fp_datap->wq_irq_return);
}

int egf_fp_irq_init(void)
{
    return 0;
}

int egisfp_interrupt_init(struct tran_fp_data *tran_fp_datap, int int_mode, int detect_period, int detect_threshold)
{

	int err = 0;
	unsigned long irqflags;

	TRAN_FP_INFO(" mode = %d period = %d threshold = %d \n", int_mode, detect_period, detect_threshold);
	TRAN_FP_INFO(" irq = %d  pin = %d \n", tran_fp_datap->irq, tran_fp_datap->irq_gpio);

	tran_fp_datap->fps_ints.detect_period = detect_period;
	tran_fp_datap->fps_ints.detect_threshold = detect_threshold;
	tran_fp_datap->fps_ints.int_count = 0;
	tran_fp_datap->fps_ints.finger_on = 0;
	tran_fp_datap->fps_ints.drdy_irq_abort = 0;

	if (g_tran_fp_req_irq_times == 0)
	{
		TRAN_FP_INFO("current flag : %d disable: %d enable: %d \n", tran_fp_datap->fps_ints.drdy_irq_flag, DRDY_IRQ_DISABLE, DRDY_IRQ_ENABLE);

		if (int_mode == EDGE_TRIGGER_RISING)
		{
			TRAN_FP_INFO("EDGE_TRIGGER_RISING \n");
			err = request_irq(tran_fp_datap->irq, fp_eint_func, IRQ_TYPE_EDGE_RISING, "fp_detect-eint", tran_fp_datap);
		}
		else if (int_mode == EDGE_TRIGGER_FALLING)
		{
			TRAN_FP_INFO("EDGE_TRIGGER_FALLING \n");
			err = request_irq(tran_fp_datap->irq, fp_eint_func, IRQ_TYPE_EDGE_FALLING, "fp_detect-eint", tran_fp_datap);
		}
		else if (int_mode == LEVEL_TRIGGER_LOW)
		{
			TRAN_FP_INFO("LEVEL_TRIGGER_LOW \n");
			err = request_irq(tran_fp_datap->irq, fp_eint_func_ll, IRQ_TYPE_LEVEL_LOW, "fp_detect-eint", tran_fp_datap);
		}
		else if (int_mode == LEVEL_TRIGGER_HIGH)
		{
			TRAN_FP_INFO("LEVEL_TRIGGER_HIGH \n");
			err = request_irq(tran_fp_datap->irq, fp_eint_func_ll, IRQ_TYPE_LEVEL_HIGH, "fp_detect-eint", tran_fp_datap);
		}

		if (err)
		{
			TRAN_FP_ERROR("request_irq failed return %d \n", err);
			return err;
		}

		g_tran_fp_req_irq_times = 1;
		disable_irq_nosync(tran_fp_datap->irq);
		TRAN_FP_INFO("request_irq return %d \n", err);
	}
	TRAN_FP_DEBUG("current flag : %d disable: %d enable: %d \n", tran_fp_datap->fps_ints.drdy_irq_flag, DRDY_IRQ_DISABLE, DRDY_IRQ_ENABLE);
	spin_lock_irqsave(&tran_fp_datap->irq_lock, irqflags);
	if (tran_fp_datap->fps_ints.drdy_irq_flag == DRDY_IRQ_DISABLE)
	{
		tran_fp_datap->fps_ints.drdy_irq_flag = DRDY_IRQ_ENABLE;
		TRAN_FP_INFO("enable irq with mode = %d \n", int_mode);
		enable_irq_wake(tran_fp_datap->irq);
		enable_irq(tran_fp_datap->irq);
	}
	spin_unlock_irqrestore(&tran_fp_datap->irq_lock, irqflags);

	return err;
}

int egisfp_interrupt_free(struct tran_fp_data *tran_fp_datap)
{
	unsigned long irqflags;
	TRAN_FP_DEBUG(" enter ");
	spin_lock_irqsave(&tran_fp_datap->irq_lock, irqflags);
	tran_fp_datap->fps_ints.finger_on = 0;
	if (tran_fp_datap->fps_ints.drdy_irq_flag == DRDY_IRQ_ENABLE)
	{
		TRAN_FP_DEBUG(" irq disable ");
		disable_irq_nosync(tran_fp_datap->irq);
		del_timer_sync(&tran_fp_datap->fps_ints.timer);
		tran_fp_datap->fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;
	}
	spin_unlock_irqrestore(&tran_fp_datap->irq_lock, irqflags);
	return 0;
}

int egf_key_nav_adjust_type_value(tran_key_nav_event_t *key_nav_p, unsigned int cmd)
{
    switch(cmd)
    {
	case TRAN_EGIS_SEND_NAVI_EVENT:
	    switch(*key_nav_p)
	    {
	    case TRAN_KEY_NAV_NONE:
		    *key_nav_p = 254;
		    break;
	    case TRAN_KEY_NAV_HOME:
		    *key_nav_p = TRAN_KEY_NAV_MENU;
		    break;
	    case TRAN_KEY_NAV_POWER:
		    *key_nav_p = TRAN_KEY_NAV_UP;
		    break;
	    case TRAN_KEY_NAV_MENU:
		    *key_nav_p = TRAN_KEY_NAV_DOWN;
		    break;
	    case TRAN_KEY_NAV_BACK:
		    *key_nav_p = TRAN_KEY_NAV_LEFT;
		    break;
	    case TRAN_KEY_NAV_CAMERA:
		    *key_nav_p = TRAN_KEY_NAV_RIGHT;
		    break;
	    case TRAN_KEY_NAV_FINGER_UP:
		    *key_nav_p = TRAN_KEY_NAV_PAGEUP;
		    break;
	    case TRAN_KEY_NAV_FINGER_DOWN:
		    *key_nav_p = TRAN_KEY_NAV_PAGEDOWN;
		    break;
	    case TRAN_KEY_NAV_UP:
		    *key_nav_p = TRAN_KEY_NAV_HOME;
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

int egis_customize_input_init(struct tran_fp_data *tran_fp_datap)
{
    __set_bit(KEY_PAGEUP, tran_fp_datap->input->keybit);
    __set_bit(KEY_PAGEDOWN, tran_fp_datap->input->keybit);
    __set_bit(254, tran_fp_datap->input->keybit);
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void egf_early_suspend(struct early_suspend *es)
{
    TRAN_FP_DEBUG(" enter\n");
}

void egf_late_resume(struct early_suspend *es)
{
    TRAN_FP_DEBUG(" enter\n");
}
#else
int egf_fb_notifier_callback(struct notifier_block *notif, unsigned long event, void *data)
{
    //unsigned int blank;
    int retval = 0;
    struct tran_fp_data *tran_fp_datap = container_of(notif, struct tran_fp_data, notifier);
    //struct fb_event* evdata = data;
    int *blank = (int *)data;
    if (event != MTK_DISP_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
        return 0;
    }
    //blank = *(unsigned int*)evdata->data;
    switch (*blank) {
        case MTK_DISP_BLANK_UNBLANK:
            TRAN_FP_INFO("==FB_BLANK_UNBLANK==\n");
            mutex_lock(&tran_fp_datap->buf_lock);
            tran_fp_datap->screen_status = 1;
            mutex_unlock(&tran_fp_datap->buf_lock);
            break;

        case MTK_DISP_BLANK_POWERDOWN:
            TRAN_FP_INFO("==FB_BLANK_POWERDOWN==\n");
            mutex_lock(&tran_fp_datap->buf_lock);
            tran_fp_datap->screen_status = 0;
            mutex_unlock(&tran_fp_datap->buf_lock);
            break;
        default:
            break;
    }

    return retval;
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

int egf_proc_init(struct tran_fp_data *tran_fp_dev)
{
    return 0;

}

void egf_proc_deinit(struct tran_fp_data *tran_fp_dev)
{
    return;
}
