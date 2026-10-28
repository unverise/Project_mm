#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/printk.h>
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
#include <linux/bug.h>
#include <linux/param.h>
#include <linux/ioport.h>
#include <linux/irqreturn.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/cpu.h>
#include <linux/of_device.h>
#include <linux/string.h>

#include "focaltech_tee.h"

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

extern int tran_fp_spi_transfer(void *chip_conf, int cfg_len, const char *txbuf, char *rxbuf, int len, const char *txbuf1, char *rxbuf1, int len1);
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);

extern struct tran_fp_data *g_tran_fp_datap;
extern int g_tran_fp_req_irq_times;
extern ff_driver_config_t *ff_g_config;
extern struct work_struct ff_core_work;

static char readbuf[16]  = {0};
static char writebuf[16] = {0};

static int ftf_read_sensor_id(void)
{
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
	uint8_t ftf_chip_data_1_0[4] = {0x09, 0xf6, 0xfd, 0x0a};
    uint8_t ftf_chip_data_1_1[4] = {0x09, 0xf6, 0xfe, 0x7f};
    uint8_t ftf_chip_data_1_2[5] = {0x08, 0xf7, 0xfe, 0x00, 0x00};
    
    uint8_t ftf_chip_data_1_3[4] = {0x09, 0xf6, 0xc6, 0x01};
    uint8_t ftf_chip_data_1_4[5] = {0x08, 0xf7, 0xc6, 0x00, 0x00};

    uint8_t ftf_chip_data_1_5[8] = {0x05, 0xfa, 0x9a, 0x84, 0x00, 0x00, 0xff, 0xff};
    uint8_t ftf_chip_data_1_6[10] = {0x04, 0xfb, 0x9a, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	int ret = -1;
    int trytimes = 3;
    uint8_t set_iov_tries = 5;
    uint8_t set_spimode_tries = 3;
    int i;
	int cfg_len = sizeof(struct mt_chip_conf);
	mdelay(2);

	do {
        g_tran_fp_datap->shutdown(g_tran_fp_datap);
        msleep(5);
        g_tran_fp_datap->power_onoff(g_tran_fp_datap, 1);
        msleep(10);
        g_tran_fp_datap->reset(g_tran_fp_datap, 1000, 10000);

        for (i = 0; i < set_iov_tries; i++) {
            memset(readbuf,  0, sizeof(readbuf));
	        memset(writebuf, 0, sizeof(writebuf));
	        memcpy(writebuf, ftf_chip_data_1_0, 4);
	        ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 4, NULL, NULL, 0);
            if (ret != 0) {
		        TRAN_FP_ERROR("SPI transfer failed\n");
		        continue;
	        }

            memset(readbuf,  0, sizeof(readbuf));
	        memset(writebuf, 0, sizeof(writebuf));
	        memcpy(writebuf, ftf_chip_data_1_1, 4);
	        ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 4, NULL, NULL, 0);
            if (ret != 0) {
		        TRAN_FP_ERROR("SPI transfer failed\n");
		        continue;
	        }

            msleep(1);
            memset(readbuf,  0, sizeof(readbuf));
	        memset(writebuf, 0, sizeof(writebuf));
	        memcpy(writebuf, ftf_chip_data_1_2, 5);
	        ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 5, NULL, NULL, 0);
            if (ret != 0) {
		        TRAN_FP_ERROR("SPI transfer failed\n");
		        continue;
	        }
            if (0x7F != readbuf[4]) {
                TRAN_FP_ERROR("set io voltage abnormal,read:%d,retry:%d", readbuf[4], i);
                continue;
            }
            TRAN_FP_INFO("set io voltage pass");
            break;
        }
        if (i >= set_iov_tries) {
            TRAN_FP_ERROR("set io voltage fail");
        }

        for (i = 0; i < set_spimode_tries; i++) {
            memset(readbuf,  0, sizeof(readbuf));
	        memset(writebuf, 0, sizeof(writebuf));
	        memcpy(writebuf, ftf_chip_data_1_3, 4);
	        ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 4, NULL, NULL, 0);
            if (ret != 0) {
		        TRAN_FP_ERROR("SPI transfer failed\n");
		        continue;
	        }
            msleep(4);
            memset(readbuf,  0, sizeof(readbuf));
	        memset(writebuf, 0, sizeof(writebuf));
	        memcpy(writebuf, ftf_chip_data_1_4, 5);
	        ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 5, NULL, NULL, 0);
            if (ret != 0) {
		        TRAN_FP_ERROR("SPI transfer failed\n");
		        continue;
	        }
            if (0x01 != readbuf[4]) {
                TRAN_FP_ERROR("set spi mode abnormal,read:%d,retry:%d", readbuf[4], i);
                continue;
            }
            TRAN_FP_INFO("set spi mode pass");
            break;
        }

        if (i >= set_spimode_tries) {
            TRAN_FP_ERROR("set spi mode fail");
        }

        memset(readbuf,  0, sizeof(readbuf));
	    memset(writebuf, 0, sizeof(writebuf));
	    memcpy(writebuf, ftf_chip_data_1_5, 8);
	    ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 8, NULL, NULL, 0);

	    if (ret != 0) {
		TRAN_FP_ERROR("SPI transfer failed\n");
		continue;
	    }
		memset(readbuf,  0, sizeof(readbuf));
		memset(writebuf, 0, sizeof(writebuf));
		memcpy(writebuf, ftf_chip_data_1_6, 10);
		ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 10, NULL, NULL, 0);
	    if (ret != 0) {
		TRAN_FP_ERROR("SPI transfer failed\n");
		continue;
	    }
		TRAN_FP_INFO(" readbuf[6] = %#2x, readbuf[7] = %#2x", readbuf[6], readbuf[7]);

	    if ((((readbuf[6] << 8) | readbuf[7]) == 0x9362) || (((readbuf[6] << 8) | readbuf[7]) == 0x9371) || (((readbuf[6] << 8) | readbuf[7]) == 0x9372) || (((readbuf[6] << 8) | readbuf[7]) == 0x9391) || (((readbuf[6] << 8) | readbuf[7]) == 0x9395) || (((readbuf[6] << 8) | readbuf[7]) == 0x9396) || (((readbuf[6] << 8) | readbuf[7]) == 0x9399)) {
    	    	TRAN_FP_INFO(" focaltech read chip id success!");
		return 0;
	    }
        g_tran_fp_datap->shutdown(g_tran_fp_datap);
        msleep(5);
        g_tran_fp_datap->power_onoff(g_tran_fp_datap, 0);
        msleep(100);
	}while (trytimes--);
        g_tran_fp_datap->shutdown(g_tran_fp_datap);
        g_tran_fp_datap->power_onoff(g_tran_fp_datap, 1);
	return -1;
#else
	return 0;

#endif
}

int ftf_spi_transfer(unsigned char *chip_id_str)
{
	g_tran_fp_datap->reset(g_tran_fp_datap, 5000, 3000);
	ftf_read_sensor_id();
	memcpy(chip_id_str, readbuf, 16);
	return 0;
}

void ff_ctl_device_event(struct work_struct *ws)
{
    struct tran_fp_data *tran_fp_datap = container_of(ws, struct tran_fp_data, work_queue);
    char *uevent_env[2] = {"FF_INTERRUPT", NULL};
#ifdef FINGERPRINT_INTERRUPT_LOG
    TRAN_FP_DEBUG(" enter.");
#endif

#ifdef CONFIG_PM_SLEEP
    __pm_wakeup_event(tran_fp_datap->tran_wakelock, jiffies_to_msecs(2*HZ));
#else
    wake_lock_timeout(&tran_fp_datap->tran_wakelock, 2*HZ);
#endif
    kobject_uevent_env(&tran_fp_datap->miscdev.this_device->kobj, KOBJ_CHANGE, uevent_env);

#ifdef FINGERPRINT_INTERRUPT_LOG
    TRAN_FP_DEBUG(" leave.");
#endif
}

irqreturn_t ff_ctl_device_irq(int irq, void *dev_id)
{
    struct tran_fp_data *tran_fp_datap = (struct tran_fp_data *)dev_id;
    disable_irq_nosync(irq);
    queue_work(tran_fp_datap->wqueue, &ff_core_work);

    if (likely(irq == tran_fp_datap->irq)) {
        if (ff_g_config->enable_fasync && tran_fp_datap->async_queue) {
            kill_fasync(&tran_fp_datap->async_queue, SIGIO, POLL_IN);
        } else {
            schedule_work(&tran_fp_datap->work_queue);
        }
    }
    enable_irq(irq);
    return IRQ_HANDLED;
}

int ftf_fp_irq_init(void)
{
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void ftf_early_suspend(struct early_suspend *es)
{
    TRAN_FP_DEBUG(" enter\n");
}

void ftf_late_resume(struct early_suspend *es)
{
    TRAN_FP_DEBUG(" enter\n");
}
#else
int ftf_fb_notifier_callback(struct notifier_block *nb, unsigned long action, void *data)
{
    //struct fb_event *event;
    char *uevent_env[2];
    int *blank = (int *)data;
    if (action != MTK_DISP_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
        return NOTIFY_DONE;
    }

    TRAN_FP_DEBUG("enter");

    //event = (struct fb_event *)data;
    //blank = *(int *)event->data;
    switch (*blank) {
    case MTK_DISP_BLANK_UNBLANK:
        uevent_env[0] = "FF_SCREEN_ON";
        break;
    case MTK_DISP_BLANK_POWERDOWN:
        uevent_env[0] = "FF_SCREEN_OFF";
        break;
    default:
        uevent_env[0] = "FF_SCREEN_??";
        break;
    }
    uevent_env[1] = NULL;
    kobject_uevent_env(&g_tran_fp_datap->miscdev.this_device->kobj, KOBJ_CHANGE, uevent_env);

    TRAN_FP_DEBUG("leave");
    return NOTIFY_OK;
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

int ftf_proc_init(struct tran_fp_data *tran_fp_dev)
{
    return 0;

}

void ftf_proc_deinit(struct tran_fp_data *tran_fp_dev)
{
    return;
}

int focaltech_customize_input_init(struct tran_fp_data *tran_fp_datap)
{
    int err = 0;
    TRAN_FP_DEBUG(" enter.\n");
    if (ff_g_config) {
        input_set_capability(tran_fp_datap->input, EV_KEY, ff_g_config->keycode_nav_left);
        input_set_capability(tran_fp_datap->input, EV_KEY, ff_g_config->keycode_nav_right);
        input_set_capability(tran_fp_datap->input, EV_KEY, ff_g_config->keycode_nav_up);
        input_set_capability(tran_fp_datap->input, EV_KEY, ff_g_config->keycode_nav_down);
        input_set_capability(tran_fp_datap->input, EV_KEY, ff_g_config->keycode_double_click);
        input_set_capability(tran_fp_datap->input, EV_KEY, ff_g_config->keycode_click);
        input_set_capability(tran_fp_datap->input, EV_KEY, ff_g_config->keycode_long_press);
        input_set_capability(tran_fp_datap->input, EV_KEY, ff_g_config->keycode_simulation);
    }
    err = input_register_device(tran_fp_datap->input);

    if (err) {
        TRAN_FP_ERROR("input_register_device(..) = %d.\n", err);
        input_free_device(tran_fp_datap->input);
        tran_fp_datap->input = NULL;
        return (-ENODEV);
    }
    TRAN_FP_DEBUG(" leave.\n");
    return 0;
}

const char *ff_ctl_get_version(void)
{
    static char version[FF_DRV_VERSION_LEN] = {'\0', };
    version[0] = '\0';
    strcat(version, FF_DRV_VERSION);
#ifdef __FF_SVN_REV
    if (strlen(__FF_SVN_REV) > 0) {
        //sprintf(version, "%s-r%s", version, __FF_SVN_REV);
    }
#endif
#ifdef __TRAN_BUILD_DATE
    strcat(version, "-"__TRAN_BUILD_DATE);
#endif
    //sprintf(version, "%s-%s", version, ff_ctl_arch_str());
    TRAN_FP_INFO("version: '%s'.", version);
    return (const char *)version;
}


void ff_work_handler(struct work_struct* data)
{
    int cpu;
    TRAN_FP_DEBUG("enter ff_work_handler");

    for (cpu = 1 ; cpu < NR_CPUS; cpu++) {
        if (!cpu_online(cpu)) {
        }
    }
}

int ff_log_printf(ff_log_level_t level, const char *tag, const char *fmt, ...)
{
    va_list ap;
    static char uevent_env_buf[128];
    char *ptr = uevent_env_buf;
    int n, available = sizeof(uevent_env_buf);
    available -= sprintf(uevent_env_buf, "FF_LOG=%1d", level);
    ptr += strlen(uevent_env_buf);
    va_start(ap, fmt);
    vsnprintf(ptr, available, fmt, ap);
    va_end(ap);
    /* Send to ff_device. */
    if (likely(g_tran_fp_datap) && likely(ff_g_config) && unlikely(ff_g_config->logcat_driver)) {
        char *uevent_env[2] = {uevent_env_buf, NULL};
        kobject_uevent_env(&g_tran_fp_datap->miscdev.this_device->kobj, KOBJ_CHANGE, uevent_env);
    }
    /* Native output. */
    switch (level) {
    case FF_LOG_LEVEL_ERR:
        n = printk(KERN_ERR FF_DRV_NAME": %s\n", ptr);
        break;
    case FF_LOG_LEVEL_WRN:
        n = printk(KERN_WARNING FF_DRV_NAME": %s\n", ptr);
        break;
    case FF_LOG_LEVEL_INF:
        n = printk(KERN_INFO FF_DRV_NAME": %s\n", ptr);
        break;
    case FF_LOG_LEVEL_DBG:
    case FF_LOG_LEVEL_VBS:
    default:
        n = printk(KERN_DEBUG FF_DRV_NAME": %s\n", ptr);
        break;
    }
    return n;
}
/*
const char *ff_ctl_arch_str(void)
{
    return CONFIG_MTK_PLATFORM;
}
*/
