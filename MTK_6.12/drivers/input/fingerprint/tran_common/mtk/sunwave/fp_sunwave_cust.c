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

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifdef CONFIG_SPI_MT65XX_KERNEL510
#include <linux/platform_data/spi-mt65xx.h>
#endif
#endif

#include "sf_ctl.h"

#ifdef CONFIG_SPI_MT65XX_KERNEL510
#include "tran_spi.h"
#endif

#if SF_BEANPOD_COMPATIBLE_V1
#include "nt_smc_call.h"
#endif

#define SF_INT_NAME "sf-irq"
#define SF_DRV_VERSION "v2.2.26-2018-06-19"

sf_version_info_t sf_hw_ver;
extern int tran_fp_spi_transfer(void *chip_conf, int cfg_len, const char *txbuf, char *rxbuf, int len, const char *txbuf1, char *rxbuf1, int len1);
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifndef CONFIG_SPI_MT65XX_KERNEL510
extern struct mt_chip_conf tran_chip_config;
#else
extern struct mtk_chip_config tran_chip_config;
#endif
#endif

#define TRAN_IOC_REPORT_KEY_EVENT SF_IOC_REPORT_KEY_EVENT
extern struct tran_fp_data	* g_tran_fp_datap;
extern int g_tran_fp_req_irq_times;

static char readbuf[16]  = {0};
static char writebuf[16] = {0};

static ssize_t sunwave_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int len = 0;
    len += sprintf(buf, "%s\n", sf_hw_ver.driver);
    return len;
}

static DEVICE_ATTR(version, S_IRUGO | S_IWUSR, sunwave_version_show, NULL);

static ssize_t sunwave_chip_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int len = 0;
    len += sprintf((char *)buf, "chip   : %s %s\nid     : 0x0 lib:%s\nvendor : fw:%s\nmore   : fingerprint\n",
                   sf_hw_ver.sunwave_id, sf_hw_ver.ca_version,
                   sf_hw_ver.algorithm,
                   sf_hw_ver.firmware);
    return len;
}

static DEVICE_ATTR(chip_info, S_IRUGO | S_IWUSR, sunwave_chip_info_show, NULL);

static ssize_t sf_show_version(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ret = 0;
    ret += sprintf(buf + ret, "solution:%s\n", sf_hw_ver.tee_solution);
    ret += sprintf(buf + ret, "ca      :%s\n", sf_hw_ver.ca_version);
    ret += sprintf(buf + ret, "ta      :%s\n", sf_hw_ver.ta_version);
    ret += sprintf(buf + ret, "alg     :%s\n", sf_hw_ver.algorithm);
    ret += sprintf(buf + ret, "nav     :%s\n", sf_hw_ver.algo_nav);
    ret += sprintf(buf + ret, "driver  :%s\n", sf_hw_ver.driver);
    ret += sprintf(buf + ret, "firmware:%s\n", sf_hw_ver.firmware);
    ret += sprintf(buf + ret, "sensor  :%s\n", sf_hw_ver.sunwave_id);
    ret += sprintf(buf + ret, "vendor  :%s\n", sf_hw_ver.vendor_id);
    return ret;
}

static DEVICE_ATTR(tee_version, S_IWUSR | S_IRUGO, sf_show_version, NULL);

static ssize_t sf_store_set_fun(struct device *d, struct device_attribute *attr, const char *buf, size_t count)
{
    int blank;
    sscanf(buf, "%d", &blank);
    TRAN_FP_INFO("sf_store_set_fun (..) blank = %d.", blank);
    //fb_blank(NULL, blank);
    TRAN_FP_DEBUG("sf_store_set_fun (..) end.");
    return count;
}

static DEVICE_ATTR(set_fun, S_IWUSR | S_IRUGO, NULL, sf_store_set_fun);

struct attribute *sf_sysfs_entries[] = {
    &dev_attr_tee_version.attr,
    &dev_attr_chip_info.attr,
    &dev_attr_version.attr,
    &dev_attr_set_fun.attr,
    NULL
};

#if SF_INT_TRIG_HIGH
static int sf_ctl_set_irq_type(unsigned long type)
{
    return irq_set_irq_type(g_tran_fp_datap->irq, type | IRQF_NO_SUSPEND | IRQF_ONESHOT);
}
#endif

static int sf_read_sensor_id(void)
{
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
	uint8_t sf_8241_8271_chip_data[6] = {0xA0, (uint8_t)~0xA0};
	uint8_t fsf_3151_chip_data[8] = {0x65, (uint8_t)~0x65, 0x00, 0x02, 0x20, 0x01, 0x41};
	uint8_t sf_9006_chip_data[4] = {0x08, 0xf7, 0x00, 0xe4};
	uint8_t sf_9006_chip_data_1[6] = {0x60, 0x9f, 0x02, 0x00, 0x00, 0x00};
	uint8_t sf_9022_chip_data[7] = {0x65, (uint8_t)(~0x65), 0x00, 0x04, 0x20, 0x04, 0x86};
//now the chips which sunwave sold to us mainly are SW9008/SW9006, so only this kind of way about reading chipid is enough
/*
	uint8_t sf_8202_8231_chip_data[7] = {0x60,  (uint8_t)~0x60, 0x28, 0x02, 0x00};
	uint8_t sf_8221_chip_data[6] = {0x60,  0x28, 0x02, 0x00};
	uint8_t sf_8201_8211_chip_data_1[3] = {0x1c,  0x1c, 0x1c};
	uint8_t sf_8201_8211_chip_data_2[10] = {0x96,  0x69, 0x00, 0x00, 0x1e, 0x00, 0x02, 0x00};
*/
	
	char readbuf_tmp[16]  = {0};
	int ret = -1;
        int trytimes = 1;
	int sw9008_flg = 0;
	int cfg_len = sizeof(struct mt_chip_conf);
/*
    g_tran_fp_datap->tran_fp_dev->max_speed_hz = 1 * 1000 * 1000;
    g_tran_fp_datap->tran_fp_dev->bits_per_word = 8;
    g_tran_fp_datap->tran_fp_dev->mode = SPI_MODE_0;
*/
    TRAN_FP_DEBUG("(..) enter.\n");
    mdelay(4);

    do {
        /* 1.detect 8205, 8231, 8241 or 8271 */
        memset(readbuf,  0, sizeof(readbuf));
        memset(writebuf, 0, sizeof(writebuf));
	memcpy(writebuf, sf_8241_8271_chip_data, 6);
        ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 6, NULL, NULL, 0);

        if (ret != 0) {
            TRAN_FP_ERROR("sw90xx SPI transfer failed\n");
            continue;
        } else   if ((0x53 == readbuf[2]) && (0x75 == readbuf[3]) && (0x6e == readbuf[4])
            && (0x57 == readbuf[5])) {
            TRAN_FP_INFO("sw90xx read chip is ok\n");
	    sw9008_flg = 1;
	    memcpy(readbuf_tmp, readbuf, 16);
            //return 0;
        }

        //TRAN_FP_INFO("continue to read sw9006 or sw9022");

    	mdelay(1);
        /* 1.detect 9006 */
        memset(readbuf,  0, sizeof(readbuf));
        memset(writebuf, 0, sizeof(writebuf));
	memcpy(writebuf, sf_9006_chip_data, 4);
        ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 4, NULL, NULL, 0);

        if (ret != 0) {
            TRAN_FP_ERROR("sw9006 SPI transfer failed\n");
            continue;
        }
        memset(readbuf,  0, sizeof(readbuf));
        memset(writebuf, 0, sizeof(writebuf));
	memcpy(writebuf, sf_9006_chip_data_1, 6);
        ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 6, NULL, NULL, 0);

        if (ret != 0) {
            TRAN_FP_ERROR("8205 sw9006 SPI transfer failed\n");
            continue;
        }
		if ((((readbuf[4]<<8)&0xFF00)|(readbuf[5]&0xFF)) == 0x8233) {
            TRAN_FP_INFO("chip is sw9006");
            return 0;
        } else {
	    if (sw9008_flg == 1) {
	    	memcpy(readbuf, readbuf_tmp, 16);
            TRAN_FP_INFO("chip is sw9008");
            	return 0;
	    }
	}
        /* 2. detect 8393 */
	g_tran_fp_datap->reset(g_tran_fp_datap, 2000, 10000);
        memset(readbuf,  0, sizeof(readbuf));
        memset(writebuf, 0, sizeof(writebuf));
	memcpy(writebuf, sf_9022_chip_data, 7);
        ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 11, NULL, NULL, 0);
        if (ret != 0) {
            TRAN_FP_ERROR("sw9022 SPI transfer failed\n");
            continue;
        }
        if ((0x53 == readbuf[7]) && (0x75 == readbuf[8]) && (0x6e == readbuf[9])
            && (0x57 == readbuf[10])) {
            TRAN_FP_INFO("sw9022 or fs9722 read chip is ok");
            //return 0;
        }

    	mdelay(1);

        memset(readbuf,  0, sizeof(readbuf));
        memset(writebuf, 0, sizeof(writebuf));
	memcpy(writebuf, fsf_3151_chip_data, 7);
        ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 12, NULL, NULL, 0);
        if (ret != 0) {
            TRAN_FP_ERROR("SPI transfer failed\n");
            continue;
        } /*else   if ((0x46 == readbuf[7]) && (0x74 == readbuf[8]) && (0x53 == readbuf[9])
            && (0x73 == readbuf[10]) && (0x8e == readbuf[11])) {*/
            else if(0x95 == readbuf[7]){
            TRAN_FP_INFO("chip is fs9722\n");
            return 0;
        }
            else if(0x93 == readbuf[7]){
            TRAN_FP_INFO("chip is sw9022\n");
            return 0;
        }
            else if(0x97 == readbuf[7]){
            TRAN_FP_INFO("chip is fs9722\n");
            return 0;
        }
#if 0
        /* 2.detect 8202, 8205 or 8231 */
        memset(readbuf,  0, sizeof(readbuf));
        memset(writebuf, 0, sizeof(writebuf));
	memcpy(writebuf, sf_8202_8231_chip_data, 7);
        ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 7, NULL, NULL, 0);

        if (ret != 0) {
            TRAN_FP_ERROR("SPI transfer failed\n");
            continue;
        } else  if (readbuf[5] == 0x82) {
            TRAN_FP_INFO("read chip is ok\n");
            return 0;
        }

        /* 3.detect 8221 */
        memset(readbuf,  0, sizeof(readbuf));
        memset(writebuf, 0, sizeof(writebuf));
	memcpy(writebuf, sf_8221_chip_data, 6);
        ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 6, NULL, NULL, 0);

        if (ret != 0) {
            TRAN_FP_ERROR("SPI transfer failed\n");
            continue;
        } else  if (readbuf[4] == 0x82) {
            TRAN_FP_INFO("read chip is ok\n");
            return 0;
        }

        /* trustkernel bug, can not read more than 8 bytes */
        /* 4.detect 8201 or 8211 */
            mdelay(200);
            memset(readbuf,  0, sizeof(readbuf));
            memset(writebuf, 0, sizeof(writebuf));
	    memcpy(writebuf, sf_8201_8211_chip_data_1, 3);
            ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 3, NULL, NULL, 0);

            if (ret != 0) {
                TRAN_FP_ERROR("SPI transfer failed\n");
                continue;
            }

            mdelay(5);
            memset(readbuf,  0, sizeof(readbuf));
            memset(writebuf, 0, sizeof(writebuf));
	    memcpy(writebuf, sf_8201_8211_chip_data_2, 10);
            ret =  tran_fp_spi_transfer(&tran_chip_config, cfg_len, writebuf, readbuf, 10, NULL, NULL, 0);

            if (ret != 0) {
                TRAN_FP_ERROR("SPI transfer failed\n");
                continue;
            } else  if ((readbuf[8] == 0xfa) || (readbuf[9] == 0xfa)) {
                TRAN_FP_INFO("read chip is ok\n");
                return 0;
            }
#endif
    }while (trytimes--);
    return -1;
#else
    return 0;

#endif
}

int sf_spi_transfer(unsigned char *chip_id_str)
{
	g_tran_fp_datap->reset(g_tran_fp_datap, 2000, 6000);
	sf_read_sensor_id();
	memcpy(chip_id_str, readbuf, 16);
	return 0;
}

void sf_ctl_device_event(struct work_struct *ws)
{
    char *uevent_env[2] = { SF_INT_EVENT_NAME, NULL };
#ifdef FINGERPRINT_INTERRUPT_LOG
    TRAN_FP_DEBUG("(..) enter.\n");
#endif
    kobject_uevent_env(&g_tran_fp_datap->miscdev.this_device->kobj,
                       KOBJ_CHANGE, uevent_env);
}

static irqreturn_t sf_ctl_device_irq(int irq, void *dev_id)
{
    disable_irq_nosync(irq);
#ifdef FINGERPRINT_INTERRUPT_LOG
    TRAN_FP_DEBUG("(irq = %d, ..) toggled.\n", irq);
#endif
    schedule_work(&g_tran_fp_datap->work_queue);
#ifdef CONFIG_PM_SLEEP
    __pm_wakeup_event(g_tran_fp_datap->tran_wakelock, msecs_to_jiffies(5000));
#else
    wake_lock_timeout(&g_tran_fp_datap->tran_wakelock, msecs_to_jiffies(5000));
#endif
#if SF_INT_TRIG_HIGH 
    sf_ctl_set_irq_type(IRQF_TRIGGER_RISING);
#endif
    enable_irq(irq);
    return IRQ_HANDLED;
}

int sf_fp_irq_init(void)
{
	int ret = 0;
	unsigned long irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND;
    TRAN_FP_INFO("g_tran_fp_datap->irq = %d \n", g_tran_fp_datap->irq);
	if ((ret = request_irq(g_tran_fp_datap->irq,sf_ctl_device_irq,irq_flags,SF_INT_NAME,g_tran_fp_datap)) < 0) {
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


#ifdef CONFIG_HAS_EARLYSUSPEND
static void sf_early_suspend(struct early_suspend *handler)
{
    char *screen[2] = { "SCREEN_STATUS=OFF", NULL };
    kobject_uevent_env(&g_tran_fp_datap->miscdev.this_device->kobj, KOBJ_CHANGE, screen);
#if SF_INT_TRIG_HIGH 
    sf_ctl_set_irq_type(IRQF_TRIGGER_RISING);
#endif
}

static void sf_late_resume(struct early_suspend *handler)
{
    char *screen[2] = { "SCREEN_STATUS=ON", NULL };
    kobject_uevent_env(&g_tran_fp_datap->miscdev.this_device->kobj, KOBJ_CHANGE, screen);
#if SF_INT_TRIG_HIGH 
    sf_ctl_set_irq_type(IRQF_TRIGGER_RISING);
#endif
}

#else

int sf_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
    static char screen_status[64] = {'\0'};
    char *screen_env[2] = { screen_status, NULL };
    //struct fb_event *evdata = data;
    int retval = 0;
    int *blank = (int *)data;

    if (event != MTK_DISP_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
        return 0;
    }

    switch (*blank) {
        case MTK_DISP_BLANK_UNBLANK:
            sprintf(screen_status, "SCREEN_STATUS=%s", "ON");
            kobject_uevent_env(&g_tran_fp_datap->miscdev.this_device->kobj, KOBJ_CHANGE, screen_env);
#if SF_INT_TRIG_HIGH 
            sf_ctl_set_irq_type(IRQF_TRIGGER_RISING);
#endif
            break;

        case MTK_DISP_BLANK_POWERDOWN:
            sprintf(screen_status, "SCREEN_STATUS=%s", "OFF");
            kobject_uevent_env(&g_tran_fp_datap->miscdev.this_device->kobj, KOBJ_CHANGE, screen_env);
#if SF_INT_TRIG_HIGH 
            sf_ctl_set_irq_type(IRQF_TRIGGER_RISING);
#endif
            break;

        default:
            break;
    }

    return retval;
}
#endif //SF_CFG_HAS_EARLYSUSPEND


const char *sf_ctl_get_version(void)
{
    static char version[SF_DRV_VERSION_LEN] = {'\0', };
    strncpy(version, SF_DRV_VERSION, SF_DRV_VERSION_LEN);
    version[SF_DRV_VERSION_LEN - 1] = '\0';
    return (const char *)version;
}

int sf_proc_init(struct tran_fp_data *tran_fp_dev)
{
	return 0;
}

int sf_proc_deinit(struct tran_fp_data *tran_fp_dev)
{
	return 0;
}

int sf_key_nav_adjust_type_value(tran_key_nav_event_t *key_nav_p, unsigned int cmd)
{
	switch(cmd)
	{
	    case TRAN_IOC_REPORT_KEY_EVENT:
		switch(*key_nav_p)
		{
		case TRAN_KEY_NAV_POWER:
			*key_nav_p = TRAN_KEY_NAV_MENU;
			break;
		case TRAN_KEY_NAV_MENU:
			*key_nav_p = TRAN_KEY_NAV_BACK;
			break;
		case TRAN_KEY_NAV_BACK:
			*key_nav_p = TRAN_KEY_NAV_F28;
			break;
		case TRAN_KEY_NAV_CAMERA:
			*key_nav_p = TRAN_KEY_NAV_ENTER;
			break;
		case TRAN_KEY_NAV_FINGER_UP:
			*key_nav_p = TRAN_KEY_NAV_UP;
			break;
		case TRAN_KEY_NAV_FINGER_DOWN:
			*key_nav_p = TRAN_KEY_NAV_LEFT;
			break;
		case TRAN_KEY_NAV_UP:
			*key_nav_p = TRAN_KEY_NAV_RIGHT;
			break;
		case TRAN_KEY_NAV_LEFT:
			*key_nav_p = TRAN_KEY_NAV_WAKEUP;
			break;
		case TRAN_KEY_NAV_RIGHT:
		case TRAN_KEY_NAV_CLICK:
		case TRAN_KEY_NAV_HEAVY:
		case TRAN_KEY_NAV_LONG_PRESS:
		case TRAN_KEY_NAV_DOUBLE_CLICK:
		case TRAN_KEY_NAV_F28:
		case TRAN_KEY_NAV_ENTER:
		case TRAN_KEY_NAV_WAKEUP:
			*key_nav_p = TRAN_KEY_NAV_NONE;
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
