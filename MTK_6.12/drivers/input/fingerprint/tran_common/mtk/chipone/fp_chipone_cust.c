#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#include <net/sock.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifdef CONFIG_SPI_MT65XX_KERNEL510
#include <linux/platform_data/spi-mt65xx.h>
#endif
#endif

#include  <linux/regulator/consumer.h>
#include "fpsensor_spi_tee.h"

#ifdef CONFIG_SPI_MT65XX_KERNEL510
#include "tran_spi.h"
#endif

extern int tran_fp_spi_transfer(void *chip_conf, int cfg_len, const char *txbuf, char *rxbuf, int len, const char *txbuf1, char *rxbuf1, int len1);

static int chipone_read_sensor_id(void);
static char readbuf[16]  = {0};
static char writebuf[16] = {0};

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifndef CONFIG_SPI_MT65XX_KERNEL510
extern struct mt_chip_conf tran_chip_config;
#else
extern struct mtk_chip_config tran_chip_config;
#endif
#endif
extern struct tran_fp_data *g_tran_fp_datap;

int cf_spi_transfer(unsigned char *chip_id_str)
{
	g_tran_fp_datap->reset(g_tran_fp_datap, 2000, 5000);
	chipone_read_sensor_id();
	memcpy(chip_id_str, readbuf, 16);
	return 0;
}

static void setRcvIRQ(int val)
{
    g_tran_fp_datap->RcvIRQ = val;
}

irqreturn_t fpsensor_irq(int irq, void *handle)
{
    struct tran_fp_data *fpsensor_dev = (struct tran_fp_data *)handle;

    /* Make sure 'wakeup_enabled' is updated before using it
    ** since this is interrupt context (other thread...) */
    smp_rmb();
#ifdef CONFIG_PM_SLEEP
    __pm_wakeup_event(fpsensor_dev->tran_wakelock, msecs_to_jiffies(1000));
#else
    wake_lock_timeout(&fpsensor_dev->tran_wakelock, msecs_to_jiffies(1000));
#endif
    setRcvIRQ(1);
    wake_up_interruptible(&fpsensor_dev->wq_irq_return);

    return IRQ_HANDLED;
}

int cf_fp_irq_init(void)
{
    return 0;
}

int cf_fp_irq_deinit(void)
{
    int retval = 0;
    free_irq(g_tran_fp_datap->irq, g_tran_fp_datap);
    retval = disable_irq_wake(g_tran_fp_datap->irq);
    if (retval < 0 ) {
	TRAN_FP_ERROR(" disable_irq_wake failed , retval =%d \n", retval);
	return retval;
    }
    return retval;
}

int chipone_read_sensor_id(void)
{
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
    int ret = -1;
    int trytimes = 4;

    int cfg_len = sizeof(struct mt_chip_conf);
    uint8_t cf_7153_7222_7332_chip_data_1[6] = {0x08,  0x55};
    uint8_t cf_7153_7222_7332_chip_data_2[7] = {0x00,  0x00, 0x00};

    TRAN_FP_DEBUG("(..) enter.\n");
    do {
       //  1.detect 7153 7222 7332
        	memset(readbuf,  0, sizeof(readbuf));
        	memset(writebuf, 0, sizeof(writebuf));
		memcpy(writebuf, cf_7153_7222_7332_chip_data_1, 2);
		ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 6, NULL, NULL, 0);
		 if (ret != 0) {
            		TRAN_FP_ERROR("SPI transfer failed\n");
            		continue;
       		 }
        	memset(readbuf,  0, sizeof(readbuf));
        	memset(writebuf, 0, sizeof(writebuf));
		memcpy(writebuf, cf_7153_7222_7332_chip_data_2, 3);
		ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 7, NULL, NULL, 0);

		if (ret != 0) {
            		TRAN_FP_ERROR("SPI transfer failed\n");
            		continue;
           	}
		TRAN_FP_INFO("fpsensor FINGERPRINT CHIPID is ICNT%2x%2x , trytimes = %d\n",readbuf[1],readbuf[2], trytimes);
        	if (readbuf[1] == 0x71 || readbuf[1] == 0x72 || readbuf[1] == 0x73) {
			TRAN_FP_INFO("fpsensor get chipid success!");
            		return 0;
        	}
    }while (trytimes--);
    return -1;
#else
    return 0;
#endif
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void cf_early_suspend(struct early_suspend *handler)
{
	TRAN_FP_DEBUG("enter\n");
}

void cf_late_resume(struct early_suspend *handler)
{
	TRAN_FP_DEBUG("enter\n");
}
#else
int cf_fb_notifier_callback(struct notifier_block* self, unsigned long event, void* data)
{
    int retval = 0;
    static char screen_status[64] = { '\0' };
    struct tran_fp_data *fpsensor_dev = g_tran_fp_datap;
    int *blank = (int *)data;

    TRAN_FP_DEBUG(" enter.  event : 0x%x\n", (unsigned)event);
    if (event != MTK_DISP_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
        return 0;
    }

    TRAN_FP_DEBUG(" enter, blank=0x%x\n", *blank);

    switch (*blank) {
    case MTK_DISP_BLANK_UNBLANK:
        TRAN_FP_INFO("lcd on notify\n");
        sprintf(screen_status, "SCREEN_STATUS=%s", "ON");
        fpsensor_dev->fb_status = 1;
        if( fpsensor_dev->enable_report_blankon) {
            fpsensor_dev->RcvIRQ = 2;
            wake_up_interruptible(&fpsensor_dev->wq_irq_return);
        }
        break;

    case MTK_DISP_BLANK_POWERDOWN:
        TRAN_FP_INFO("lcd off notify\n");
        sprintf(screen_status, "SCREEN_STATUS=%s", "OFF");
        fpsensor_dev->fb_status = 0;
        break;

    default:
        TRAN_FP_INFO("other notifier, ignore\n");
        break;
    }

    TRAN_FP_DEBUG("%s leave.\n", screen_status);
    return retval;
}
#endif

void fpsensor_dev_cleanup(struct tran_fp_data *fpsensor)
{
    TRAN_FUNC_ENTRY();

    misc_deregister(&fpsensor->miscdev);

    TRAN_FUNC_EXIT();
}


static int fpsensordevinfo_proc_open(struct inode *inode, struct file *file)
{
	TRAN_FP_INFO("inode:%p,file:%p\n",inode,file);
	return 0;
}

static const struct proc_ops fpsensordevinfo_proc_fops = {
	.proc_open		= fpsensordevinfo_proc_open,
};

int cf_proc_init(struct tran_fp_data *tran_fp_dev)
{
	struct proc_dir_entry * pde;
	pde = proc_create("fpsensor", 0, NULL, &fpsensordevinfo_proc_fops);
	if (pde == NULL) {
	    TRAN_FP_ERROR("proc node create failed");
	    return -ENOENT;
	}
	tran_fp_dev->pde = pde;
	return 0;
}

int cf_proc_deinit(struct tran_fp_data *tran_fp_dev)
{
	struct proc_dir_entry * pde;
	pde = tran_fp_dev->pde;
	if (pde != NULL) {
	    proc_remove(pde);
	}
	return 0;
}
