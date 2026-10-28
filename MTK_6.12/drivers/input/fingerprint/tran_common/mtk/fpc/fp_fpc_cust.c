#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
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
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>

#include "fpc_tee.h"

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

#define FPC_TTW_HOLD_TIME 2000
#define FPC_CHIP 0x1000
#define FPC_CHIP_MASK_SENSOR_TYPE 0xff00

extern int tran_fp_spi_transfer(void *chip_conf, int cfg_len, const char *txbuf, char *rxbuf, int len, const char *txbuf1, char *rxbuf1, int len1);
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);

extern struct tran_fp_data *g_tran_fp_datap;
extern int g_tran_fp_req_irq_times;

static int ff_spi_read_bytes_ree(struct tran_fp_data *tran_fp_datap, u16 addr, u32 data_len, u8 *rx_buf)
{
	 int cfg_len;
	 uint8_t tmp_buf[10] = { 0 };
	 tmp_buf[0] = addr;
 	 cfg_len = sizeof(struct mt_chip_conf);
	 tran_fp_spi_transfer(&tran_chip_config, cfg_len, &tmp_buf[0], NULL, 1, tmp_buf + 2, tmp_buf + 4, 2);
	 memcpy(rx_buf, tmp_buf + 4, data_len);
	 return 0;
}

int ff_spi_transfer(unsigned char *chip_id_str)
{
	int status = -EFAULT;
	(void) status;
	unsigned char rx_test[2] = {0};
	g_tran_fp_datap->reset(g_tran_fp_datap, 5000, 2000);
	status = ff_spi_read_bytes_ree(g_tran_fp_datap, 0xFC, 2, rx_test);
    	TRAN_FP_INFO(" rx_test fp id:%#2x %#2x ",rx_test[0],rx_test[1]);
	memcpy(chip_id_str, rx_test, 2);
	return 0;
}

static ssize_t hw_reset_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	struct tran_fp_data *tran_fp_datap = g_tran_fp_datap;

	if (!strncmp(buf, "reset", strlen("reset")))
		ret = tran_fp_datap->reset(tran_fp_datap, tran_fp_datap->inter_delay, tran_fp_datap->post_delay);
	else
		return -EINVAL;
	return ret ? ret : count;
}

static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, hw_reset_set);

static ssize_t wakeup_enable_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tran_fp_data *tran_fp_datap = g_tran_fp_datap;

	if (!strncmp(buf, "enable", strlen("enable"))) {
		tran_fp_datap->wakeup_enabled = true;
		smp_wmb();
	} else if (!strncmp(buf, "disable", strlen("disable"))) {
		tran_fp_datap->wakeup_enabled = false;
		smp_wmb();
	} else
		return -EINVAL;

	return count;
}

static DEVICE_ATTR(wakeup_enable, S_IWUSR, NULL, wakeup_enable_set);

static ssize_t do_wakeup_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tran_fp_data *tran_fp_datap = g_tran_fp_datap;

	if (count > 0) {
		/* Sending power key event creates a toggling
		   effect that may be desired. It could be
		   replaced by another event such as KEY_WAKEUP. */
		input_report_key(tran_fp_datap->input, KEY_POWER, 1);
		input_report_key(tran_fp_datap->input, KEY_POWER, 0);
		input_sync(tran_fp_datap->input);
	} else {
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR(do_wakeup, S_IWUSR, NULL, do_wakeup_set);

static ssize_t clk_enable_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tran_fp_data *tran_fp_datap = g_tran_fp_datap;
	TRAN_FP_DEBUG(" buff is %d ", *buf);

	if (tran_fp_datap->tran_fp_dev) {
		//update spi clk
		if (*buf == 49)
			mt_spi_enable_master_clk(tran_fp_datap->tran_fp_dev);
		TRAN_FP_INFO(" enable spi clk ");

		if (*buf == 48)
			mt_spi_disable_master_clk(tran_fp_datap->tran_fp_dev);
		TRAN_FP_INFO(" disable spi clk ");

		return 1;	//set_clks(tran_fp_datap, (*buf == '1')) ? : count;
	} else {
		TRAN_FP_ERROR(" spi clk NULL");
		return 0;
	}

}

static DEVICE_ATTR(clk_enable, S_IWUSR, NULL, clk_enable_set);

static ssize_t irq_get(struct device *device, struct device_attribute *attribute, char *buffer)
{
	struct tran_fp_data *tran_fp_datap = g_tran_fp_datap;
	int irq = gpio_get_value(tran_fp_datap->irq_gpio);
	return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}
static ssize_t irq_ack(struct device *device, struct device_attribute *attribute, const char *buffer, size_t count)
{
	TRAN_FP_INFO("---");
	return count;
}

static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);

static ssize_t fpc_ic_is_exist(struct device *device, struct device_attribute *attribute, char *buffer)
{
	int fpc_exist = 0;
	struct tran_fp_data *tran_fp_datap = g_tran_fp_datap;
	if (tran_fp_datap->fpc_fp_exist) {
		fpc_exist = 1;
	} else {
		fpc_exist = 0;
	}
	return scnprintf(buffer, PAGE_SIZE, "%i\n", fpc_exist);
}

static DEVICE_ATTR(fpid_get, S_IRUSR | S_IWUSR, fpc_ic_is_exist, NULL);

struct attribute *ff_sysfs_entries[] = {
	&dev_attr_hw_reset.attr,
	&dev_attr_wakeup_enable.attr,
	&dev_attr_do_wakeup.attr,
	&dev_attr_clk_enable.attr,
	&dev_attr_irq.attr,
	&dev_attr_fpid_get.attr,
	NULL
};

void fpc1022_workqueue_notify(struct work_struct *work)
{
        sysfs_notify(&g_tran_fp_datap->tran_fp_plat_dev->dev.kobj, NULL, dev_attr_irq.attr.name);
}

static irqreturn_t fpc1022_irq_handler(int irq, void *handle)
{
	struct tran_fp_data *tran_fp_datap = handle;
	smp_rmb();
#ifdef CONFIG_PM_SLEEP
	__pm_wakeup_event(tran_fp_datap->tran_wakelock, msecs_to_jiffies(FPC_TTW_HOLD_TIME));
#else
        wake_lock_timeout(&tran_fp_datap->tran_wakelock, 2*HZ);
#endif

	//sysfs_notify(&tran_fp_datap->tran_fp_plat_dev->dev.kobj, NULL, dev_attr_irq.attr.name);

	schedule_work(&g_tran_fp_datap->work_queue);

	return IRQ_HANDLED;
}

int ff_fp_irq_init(void)
{
	int ret = 0;
	unsigned long irq_flags = IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND;
    TRAN_FP_INFO("g_tran_fp_datap->irq = %d \n", g_tran_fp_datap->irq);
	if ((ret = devm_request_threaded_irq(&g_tran_fp_datap->tran_fp_plat_dev->dev, g_tran_fp_datap->irq, fpc1022_irq_handler, NULL, irq_flags, "fpc_irq", g_tran_fp_datap)) != 0) {
    TRAN_FP_ERROR("request_irq failed ret = %d \n", ret);
    goto request_irq_fail;
   } else {
	g_tran_fp_req_irq_times = 1;
   }
	ret = enable_irq_wake(g_tran_fp_datap->irq);
        if (ret < 0 ) {
            TRAN_FP_ERROR(" enable_irq_wake failed , ret = %d \n", ret);
	    if (g_tran_fp_datap->irq && g_tran_fp_req_irq_times == 1) {
	    	devm_free_irq(&g_tran_fp_datap->tran_fp_plat_dev->dev, g_tran_fp_datap->irq, g_tran_fp_datap);
		g_tran_fp_req_irq_times = 0;
	    }
        }
request_irq_fail:
	return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void ff_early_suspend(struct early_suspend *es)
{
    TRAN_FP_DEBUG(" enter\n");
}

void ff_late_resume(struct early_suspend *es)
{
    TRAN_FP_DEBUG(" enter\n");
}
#else
int ff_fb_notifier_callback(struct notifier_block *notif, unsigned long event, void *data)
{
    TRAN_FP_DEBUG(" enter\n");
    return 0;
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

int ff_proc_init(struct tran_fp_data *tran_fp_dev)
{
    return 0;

}

void ff_proc_deinit(struct tran_fp_data *tran_fp_dev)
{
    return;
}
