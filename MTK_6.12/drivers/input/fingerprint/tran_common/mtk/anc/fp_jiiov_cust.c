/*
 * Copyright (C) 2013-2016, JIIOV Technology Co., Ltd.
 * All Rights Reserved.
 */
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
//#include <teei_fp.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#endif
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#ifdef CONFIG_MTK_CLKMGR
#include "mach/mt_clkmgr.h"
#else
#include <linux/clk.h>
#endif
#include <net/sock.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#define CONFIG_SPI_MT65XX_KERNEL510 y
/* MTK header */
#ifndef CONFIG_SPI_MT65XX_KERNEL510
#include "mt_spi.h"
#include "mt_spi_hal.h"
#else
#include "tran_spi.h"
#endif
/* there is no this file on standardized GPIO platform */
#ifdef CONFIG_MTK_GPIO
#include "mtk_gpio.h"
#include "mach/gpio_const.h"
#endif
//#include <mt-plat/sync_write.h>
#include <linux/of_address.h>
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifdef CONFIG_SPI_MT65XX_KERNEL510
#include <linux/platform_data/spi-mt65xx.h>
#endif
#endif
#include  <linux/regulator/consumer.h>
#include "anc_spi_tee.h"
extern struct tran_fp_data * g_tran_fp_datap;
extern int tran_fp_spi_transfer(void *chip_conf, int cfg_len, const char *txbuf, char *rxbuf, int len, const char *txbuf1, char *rxbuf1, int len1);
extern void tran_netlink_send(struct tran_fp_data *tran_fp_datap, const int cmd);
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifndef CONFIG_SPI_MT65XX_KERNEL510
extern struct mt_chip_conf tran_chip_config;
#else
extern struct mtk_chip_config tran_chip_config;
#endif
#endif
#define TRAN_IOC_NAV_EVENT ANC_IOC_NAV_EVENT
#define TRAN_IOC_INPUT_KEY_EVENT ANC_IOC_INPUT_KEY_EVENT
u8 anc_fp_vendor_id = 0;
static int anc_spi_read_bytes_ree(struct tran_fp_data *anc_dev, u16 addr, u32 data_len, u8 *rx_buf);
//static int anc_spi_write_bytes_ree(struct tran_fp_data *anc_dev, u16 addr, u32 data_len, u8 *tx_buf);
static ssize_t anc_debug_show(struct device *dev,struct device_attribute *attr, char *buf);
static ssize_t anc_debug_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count);
static DEVICE_ATTR(debug, S_IRUGO | S_IWUSR, anc_debug_show, anc_debug_store);
struct attribute *anc_sysfs_entries[] = {
	&dev_attr_debug.attr,
	NULL
};
static ssize_t anc_debug_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "vendor id 0x%x\n", anc_fp_vendor_id);
}
static ssize_t anc_debug_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
	struct tran_fp_data *anc_dev =  dev_get_drvdata(dev);
	int retval = 0;
	unsigned char rx_test[10] = {0};
	struct mt_spi_t *ms = NULL;
	(void) ms;
	ms = spi_controller_get_devdata(anc_dev->tran_fp_dev->controller);
	if (!strncmp(buf, "-8", 2)) {
		TRAN_FP_DEBUG(" parameter is -8, enable spi clock test===============\n");
		mt_spi_enable_master_clk(anc_dev->tran_fp_dev);
	} else if (!strncmp(buf, "-9", 2)) {
		TRAN_FP_DEBUG(" parameter is -9, disable spi clock test===============\n");
		mt_spi_disable_master_clk(anc_dev->tran_fp_dev);
	} else if (!strncmp(buf, "-10", 3)) {
		TRAN_FP_DEBUG(" parameter is -10, gf init start===============\n");
		tran_init_eint(anc_dev);
		retval = request_threaded_irq(anc_dev->irq, NULL, anc_irq,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev_name(&(anc_dev->tran_fp_dev->dev)), anc_dev);
		if (!retval)
			TRAN_FP_INFO("irq thread request success!\n");
		else
			TRAN_FP_ERROR("irq thread request failed, retval=%d\n", retval);
		anc_dev->irq_count = 1;
		anc_dev->disable_irq(anc_dev);
#if defined(CONFIG_HAS_EARLYSUSPEND)
		TRAN_FP_DEBUG(" register_early_suspend\n");
		anc_dev->early_suspend.level = (EARLY_SUSPEND_LEVEL_DISABLE_FB - 1);
		anc_dev->early_suspend.suspend = ancf_early_suspend,
		anc_dev->early_suspend.resume = ancf_late_resume,
		register_early_suspend(&anc_dev->early_suspend);
#else
		/* register screen on/off callback */
		anc_dev->notifier.notifier_call = ancf_fb_notifier_callback;
		fb_register_client(&anc_dev->notifier);
#endif
		anc_dev->sig_count = 0;
		TRAN_FP_INFO(" gf init finished======\n");
	} else if (!strncmp(buf, "-11", 3)) {
		TRAN_FP_DEBUG(" parameter is -11, enable irq===============\n");
		anc_dev->enable_irq(anc_dev);
	} else if (!strncmp(buf, "-12", 3)) {
		TRAN_FP_DEBUG(" parameter is -12, GPIO test===============\n");
		tran_fp_reset_gpio_high(anc_dev);
	} else if (!strncmp(buf, "-13", 3)) {
		TRAN_FP_DEBUG(" parameter is -13, Vendor ID test --> 0x%x\n", anc_fp_vendor_id);
		anc_spi_read_bytes_ree(anc_dev, 0x0000, 4, rx_test);
	TRAN_FP_INFO(" rx_test chip id:0x%x 0x%x 0x%x 0x%x \n",rx_test[0],rx_test[1],rx_test[2],rx_test[3]);
	} else {
		TRAN_FP_ERROR(" wrong parameter!===============\n");
	}
	return count;
#else
	return 0;
#endif
}
int ancf_spi_transfer(unsigned char *chip_id_str)
{
	int status = -EFAULT;
	(void) status;
	//unsigned char rx_test[10] = {0};
	unsigned char rx_test_1[10] = {0};
	g_tran_fp_datap->reset(g_tran_fp_datap, 10000, 10000);
	status = anc_spi_read_bytes_ree(g_tran_fp_datap, 0x0000, 4, rx_test_1);
    TRAN_FP_INFO("%s rx_test_1 fp id:0x%x 0x%x 0x%x 0x%x \n", __func__,rx_test_1[0],rx_test_1[1],rx_test_1[2],rx_test_1[3]);
	//status = anc_spi_read_bytes_ree(g_tran_fp_datap, 0x4304, 4, rx_test);
    //TRAN_FP_INFO("%s rx_test mcu id:0x%x 0x%x 0x%x 0x%x \n", __func__,rx_test[0],rx_test[1],rx_test[2],rx_test[3]);
	memcpy(chip_id_str, rx_test_1, 4);
	return 0;
}
int ancf_fp_irq_init(void)
{
	return 0;
}
irqreturn_t anc_irq(int irq, void *handle)
{
	struct tran_fp_data *anc_dev = (struct tran_fp_data *)handle;
#ifdef FINGERPRINT_INTERRUPT_LOG
	TRAN_FUNC_ENTRY();
#endif
#ifdef CONFIG_PM_SLEEP
    __pm_wakeup_event(g_tran_fp_datap->tran_wakelock, msecs_to_jiffies(1500));
#else
    wake_lock_timeout(&g_tran_fp_datap->tran_wakelock, msecs_to_jiffies(1500));
#endif
	tran_netlink_send(anc_dev, TRAN_NETLINK_IRQ);
	anc_dev->sig_count++;
#ifdef FINGERPRINT_INTERRUPT_LOG
	TRAN_FUNC_EXIT();
#endif
	return IRQ_HANDLED;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
 void ancf_early_suspend(struct early_suspend *handler)
{
	struct tran_fp_data *anc_dev = NULL;
	anc_dev = container_of(handler, struct tran_fp_data, early_suspend);
	TRAN_FP_DEBUG("enter\n");
	tran_netlink_send(anc_dev, TRAN_NETLINK_SCREEN_OFF);
}
 void ancf_late_resume(struct early_suspend *handler)
{
	struct tran_fp_data *anc_dev = NULL;
	anc_dev = container_of(handler, struct tran_fp_data, early_suspend);
	TRAN_FP_DEBUG("enter\n");
	tran_netlink_send(anc_dev, TRAN_NETLINK_SCREEN_ON);
}
#else
#if IS_ENABLED(CONFIG_TRAN_HBM_NOTIFY_FINGERPRINT) || IS_ENABLED(CONFIG_TRAN_LHBM_NOTIFY_FINGERPRINT)
static void tran_fasync_for_lcm_full_HBM(void)
{
        struct tran_fp_data *tran_fp_datap = g_tran_fp_datap;
        char* env_ext[2] = {"TRAN_FULL_HBM_EVENT=FULL_HBM_SET", NULL};
        if(tran_fp_datap->tran_fp_dev!=NULL){
                TRAN_FP_INFO("lcm start setting HBM and prepare to notify fp HAL by uevent");
                kobject_uevent_env(&tran_fp_datap->tran_fp_dev->dev.kobj, KOBJ_CHANGE, env_ext);
        }else{
                TRAN_FP_ERROR("tran_fp_datap->tran_fp_dev is null");
        }
        //kill_fasync(&tran_fp_datap->async_queue, sig, band);
}
#endif

int ancf_fb_notifier_callback(struct notifier_block *self,unsigned long event, void *data)
{
	struct tran_fp_data *anc_dev = NULL;
	//struct fb_event *evdata = data;
	//unsigned int blank;
	int retval = 0;
	int *blank=(int *)data;
	TRAN_FUNC_ENTRY();
	/* If we aren't interested in this event, skip it immediately ... */
	if (event != MTK_DISP_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */)
		return 0;
	anc_dev = container_of(self, struct tran_fp_data, notifier);
	//blank = *(int *)evdata->data;
	TRAN_FP_DEBUG(" enter, blank=0x%x\n", *blank);
	switch (*blank) {
	case TRAN_DISP_BLANK_AOD_NOTIFY:
		TRAN_FP_INFO(" lcd on notify\n");
		tran_netlink_send(anc_dev, TRAN_NETLINK_SCREEN_ON);
		break;
	case MTK_DISP_BLANK_POWERDOWN:
		TRAN_FP_INFO(" lcd off notify\n");
		tran_netlink_send(anc_dev, TRAN_NETLINK_SCREEN_OFF);
		break;
//BSP:add lcd notify status ADSHBSE-7808 
#if IS_ENABLED(CONFIG_TRAN_FOLD_DISPLAY)
	case TRAN_DISP_BLANK_UNBLANK_DSI1:
		TRAN_FP_INFO("lcd_1 on notify\n");
		tran_netlink_send(anc_dev, TRAN_NETLINK_SCREEN_ON);
		break;
	case TRAN_DISP_BLANK_POWERDOWN_DSI1:
		TRAN_FP_INFO("lcd_1 off notify\n");
		tran_netlink_send(anc_dev, TRAN_NETLINK_SCREEN_OFF);
		break;
#endif
//BSP:add lcd notify status ADSHBSE-7808
#if IS_ENABLED(CONFIG_TRAN_HBM_NOTIFY_FINGERPRINT) || IS_ENABLED(CONFIG_TRAN_LHBM_NOTIFY_FINGERPRINT)
       case TRAN_DISP_BLANK_NOTIFY_FINGERPRINT:
                TRAN_FP_INFO(" TRAN_DISP_BLANK_NOTIFY_FINGERPRINT\n");
                tran_fasync_for_lcm_full_HBM();
                break;
#endif
	default:
		TRAN_FP_INFO(" other notifier, ignore\n");
		break;
	}
	TRAN_FUNC_EXIT();
	return retval;
}
#endif //CONFIG_HAS_EARLYSUSPEND
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
	//status = anc_spi_read_bytes_ree(g_tran_fp_datap, 0x0000, 4, rx_test_1);
static int anc_spi_read_bytes_ree(struct tran_fp_data *anc_dev, u16 addr, u32 data_len, u8 *rx_buf)
{
#if 0
	 u8 *tmp_buf = NULL;
	 u32 package, reminder, retry;
	 u8 anc_chip_data_head[] = {0xF0,  (addr >> 8) & 0xFF, addr & 0xFF, 0x00, 0xF1};
	 int cfg_len;
 	 cfg_len = sizeof(struct mt_chip_conf);
	 tmp_buf = anc_dev->tran_buffer;
	 package = (data_len + 2) / 1024;
	 reminder = (data_len + 2) % 1024;
 	 memcpy(tmp_buf, anc_chip_data_head, 5);
#ifndef CONFIG_SPI_MT65XX_KERNEL510
	 /* switch to DMA mode if transfer length larger than 32 bytes */
	 if ((data_len + 1) > 32) {
		 anc_dev->spi_mcc.com_mod = DMA_TRANSFER;
		 spi_setup(anc_dev->tran_fp_dev);
	 }
#endif
	 if ((package > 0) && (reminder != 0)) {
		 retry = 1;
		 tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf, tmp_buf, 3, NULL, NULL, 0);
		 tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf + 4, tmp_buf + 4, package * 1024, NULL, NULL, 0);
		 memcpy(rx_buf, (tmp_buf + 5), (package * 1024 - 1));
		 addr = addr + package * 1024 - 2;
		 memset(tmp_buf, 0, package * 1024 - 1);
		 anc_chip_data_head[1] = (addr >> 8) & 0xFF;
		 anc_chip_data_head[2] = addr & 0xFF;
		 memcpy(tmp_buf, anc_chip_data_head, 5);
		 tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf, tmp_buf, 3, NULL, NULL, 0);
		 tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf + 4, tmp_buf + 4, reminder + 1, NULL, NULL, 0);
		 memcpy((rx_buf + package * 1024 - 1), tmp_buf + 6, reminder - 1);
	 } else {
		 retry = 0;
		 tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf, tmp_buf, 3, NULL, NULL, 0);
		 tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf + 4, tmp_buf + 4, data_len + 1, NULL, NULL, 0);
		 memcpy(rx_buf, (tmp_buf + 5), data_len);
	 }
#ifndef CONFIG_SPI_MT65XX_KERNEL510
	 /* restore to FIFO mode if has used DMA */
	 if ((data_len + 1) > 32) {
		 anc_dev->spi_mcc.com_mod = FIFO_TRANSFER;
		 spi_setup(anc_dev->tran_fp_dev);
	 }
#endif

#else
     int err = 0;
	 u8 *tmp_buf = NULL;


    char anc_tx_buf[5] = {0};
    char anc_rx_buf[5] = {0};
    int sensor_chip_id = 0x00;
    int read_retry = 0;
	int cfg_len;
 	cfg_len = sizeof(struct mt_chip_conf);
	tmp_buf = anc_dev->tran_buffer;

#ifndef CONFIG_SPI_MT65XX_KERNEL510
	 /* switch to DMA mode if transfer length larger than 32 bytes */
	 if ((data_len + 1) > 32) {
		 anc_dev->spi_mcc.com_mod = DMA_TRANSFER;
		 spi_setup(anc_dev->tran_fp_dev);
	 }
#endif
    TRAN_FP_INFO("[FP_ANC] jv0307 call");
   // if (NULL == chip_info) {
    //    TRAN_FP_ERROR("[FP_ANC] jv0307 call, invalid chip_info:%p", chip_info);
    //    return -1;
   // }

    //tee_memset(anc_tx_buf, 0, sizeof(anc_tx_buf));
    anc_tx_buf[0] = 0x08;
	
	//int tran_fp_spi_transfer(void *chip_conf, int cfg_len, const char *txbuf, char *rxbuf, int len, const char *txbuf1, char *rxbuf1, int len1)

    tran_fp_spi_transfer(&tran_chip_config, cfg_len, anc_tx_buf, anc_rx_buf,4, NULL, NULL, 0);
    //if (FP_TA_ERR_SPI_OK != (err = tee_spi_write(anc_tx_buf, anc_rx_buf, 4))) {
    //    TRAN_FP_ERROR("[FP_ANC] jv0307 call, fail to write 0x08, err: %d", err);
    //     return err;
    //}

    //TRAN_FP_INFO("[FP_ANC] jv0307 call, wait 3ms start1");
    //tee_wait_time_ms(3);
    //TRAN_FP_INFO("[FP_ANC] jv0307 call, wait 3ms end1");

    //tee_memset(anc_tx_buf, 0, sizeof(anc_tx_buf));
	memset(anc_tx_buf, 0, sizeof(anc_tx_buf));
    anc_tx_buf[0] = 0x13;
    anc_tx_buf[1] = 0xfd;
    anc_tx_buf[2] = 0x00;
    anc_tx_buf[3] = 0xc9;
	
	tran_fp_spi_transfer(&tran_chip_config, cfg_len, anc_tx_buf, anc_rx_buf,5, NULL, NULL, 0);
   /// if (FP_TA_ERR_SPI_OK != (err = tee_spi_write(anc_tx_buf, anc_rx_buf, 5))) {
   // TRAN_FP_ERROR("[FP_ANC] jv0307 call, fail to write 0x13 0xfd, err: %d", err);
   //  return err;
   //}

    memset(anc_tx_buf, 0, sizeof(anc_tx_buf));
    anc_tx_buf[0] = 0x13;
    anc_tx_buf[1] = 0x20;
    anc_tx_buf[2] = 0x00;
    anc_tx_buf[3] = 0x9a;
	
	tran_fp_spi_transfer(&tran_chip_config, cfg_len, anc_tx_buf, anc_rx_buf,5, NULL, NULL, 0);
    //if (FP_TA_ERR_SPI_OK != (err = tee_spi_write(anc_tx_buf, anc_rx_buf, 5))) {
    //    TRAN_FP_ERROR("[FP_ANC] jv0307 call, fail to write 0x13 0x20, err: %d", err);
     //   return err;
    //}

    TRAN_FP_INFO("[FP_ANC] jv0307 call, wait 3ms start2");
    //tee_wait_time_ms(3);
    mdelay(3);
    TRAN_FP_INFO("[FP_ANC] jv0307 call, wait 3ms end2");

    memset(anc_tx_buf, 0, sizeof(anc_tx_buf));
    anc_tx_buf[0] = 0x13;
    anc_tx_buf[1] = 0x6f;
    anc_tx_buf[2] = 0x10;
    anc_tx_buf[3] = 0x72;
	tran_fp_spi_transfer(&tran_chip_config, cfg_len, anc_tx_buf, anc_rx_buf,5, NULL, NULL, 0);	
   // if (FP_TA_ERR_SPI_OK != (err = tee_spi_write(anc_tx_buf, anc_rx_buf, 5))) {
   //     TRAN_FP_ERROR("[FP_ANC] jv0307 call, fail to write 0x13 0x6f 0x10, err: %d", err);
    //    return err;
   // }

    for (read_retry = 0; read_retry < 3; read_retry++) {
        memset(anc_tx_buf, 0, sizeof(anc_tx_buf));
        anc_tx_buf[0] = 0x15;
        anc_tx_buf[1] = 0x02;
        anc_tx_buf[2] = 0xcf;
        memset(anc_rx_buf, 0, sizeof(anc_rx_buf));
	    tran_fp_spi_transfer(&tran_chip_config, cfg_len, anc_tx_buf, anc_rx_buf,5, NULL, NULL, 0);			
        //if (FP_TA_ERR_SPI_OK != (err = tee_spi_write_read(anc_tx_buf, 5, anc_rx_buf, 5))) {
        //    TRAN_FP_ERROR("[FP_ANC] jv0307 call, fail to write/read 0x15 0x02 0x00, err: %d", err);
         //   return err;
        //}
        sensor_chip_id = ((anc_rx_buf[3] << 24) & 0xFF000000);
        memcpy(tmp_buf, anc_rx_buf+3, 1);
		
        memset(anc_tx_buf, 0, sizeof(anc_tx_buf));
        anc_tx_buf[0] = 0x15;
        anc_tx_buf[1] = 0x03;
        anc_tx_buf[2] = 0xc8;
        memset(anc_rx_buf, 0, sizeof(anc_rx_buf));
	    tran_fp_spi_transfer(&tran_chip_config, cfg_len, anc_tx_buf, anc_rx_buf,5, NULL, NULL, 0);	
       // if (FP_TA_ERR_SPI_OK != (err = tee_spi_write_read(anc_tx_buf, 5, anc_rx_buf, 5))) {
       //     TRAN_FP_ERROR("[FP_ANC] jv0307 call, fail to write/read 0x15 0x03 0x00, err: %d", err);
       //     return err;
       // }
        sensor_chip_id |= ((anc_rx_buf[3] << 16) & 0x00FF0000);
		memcpy(tmp_buf+1, anc_rx_buf+3, 1);

        memset(anc_tx_buf, 0, sizeof(anc_tx_buf));
        anc_tx_buf[0] = 0x15;
        anc_tx_buf[1] = 0x04;
        anc_tx_buf[2] = 0xdd;
        memset(anc_rx_buf, 0, sizeof(anc_rx_buf));
		tran_fp_spi_transfer(&tran_chip_config, cfg_len, anc_tx_buf, anc_rx_buf,5, NULL, NULL, 0);	
       // if (FP_TA_ERR_SPI_OK != (err = tee_spi_write_read(anc_tx_buf, 5, anc_rx_buf, 5))) {
        //    TRAN_FP_ERROR("[FP_ANC] jv0307 call, fail to write/read 0x15 0x04 0x00, err: %d", err);
       //     return err;
       // }
        sensor_chip_id |= ((anc_rx_buf[3] << 8) & 0x0000FF00);
        memcpy(tmp_buf+2, anc_rx_buf+3, 1);
		
        memset(anc_tx_buf, 0, sizeof(anc_tx_buf));
        anc_tx_buf[0] = 0x15;
        anc_tx_buf[1] = 0x05;
        anc_tx_buf[2] = 0xda;
        memset(anc_rx_buf, 0, sizeof(anc_rx_buf));
	   	tran_fp_spi_transfer(&tran_chip_config, cfg_len, anc_tx_buf, anc_rx_buf,5, NULL, NULL, 0);	
       // if (FP_TA_ERR_SPI_OK != (err = tee_spi_write_read(anc_tx_buf, 5, anc_rx_buf, 5))) {
        //    TRAN_FP_ERROR("[FP_ANC] jv0307 call, fail to write/read 0x15 0x05 0x00, err: %d", err);
        //    return err;
        //}
        sensor_chip_id |= (anc_rx_buf[3] & 0x000000FF);
		memcpy(tmp_buf+3, anc_rx_buf+3, 1);

        if (sensor_chip_id == 0x46544301) {
            //chip_info->chip_id = sensor_chip_id;
			//memcpy(tmp_buf, (char)sensor_chip_id, 5);
			memcpy(rx_buf, tmp_buf, 5);
            TRAN_FP_INFO("[FP_ANC] jv0307 call, sensor_chip_id:%x, chip_id:%s",
                    sensor_chip_id,
                    tmp_buf);
            err = 0;
            break;
        } else {
            TRAN_FP_ERROR("[FP_ANC] jv0307 call, sensor_chip_id:%x, is not 0x46544301", sensor_chip_id);
            err = -1;
			
        }
        //tee_wait_time_ms(1);
        mdelay(3);
    }


return err;
#endif
	
}
#if 0
static int anc_spi_write_bytes_ree(struct tran_fp_data *anc_dev, u16 addr, u32 data_len, u8 *tx_buf)
{
	u8 *tmp_buf = NULL;
	u32 package, reminder, retry;
	u8 anc_chip_data_head[] = {0xF0,  (addr >> 8) & 0xFF, addr & 0xFF};
	int cfg_len = sizeof(struct mt_chip_conf);
	tmp_buf = anc_dev->tran_buffer;
	
	package = (data_len + 3) / 1024;
	reminder = (data_len + 3) % 1024;
	memcpy(tmp_buf, anc_chip_data_head, 3);
#ifndef CONFIG_SPI_MT65XX_KERNEL510
	if ((data_len + 3) > 32) {
		anc_dev->spi_mcc.com_mod = DMA_TRANSFER;
		spi_setup(anc_dev->tran_fp_dev);
	}
#endif
	if ((package > 0) && (reminder != 0)) {
		retry = 1;
		memcpy(tmp_buf + 3, tx_buf, (package * 1024 - 3));
		tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf, tmp_buf, package * 1024, NULL, NULL, 0);
		addr = addr + package * 1024 - 3;
		memset(tmp_buf, 0, package * 1024);
		anc_chip_data_head[1] = (addr >> 8) & 0xFF;
		anc_chip_data_head[2] = addr & 0xFF;
		memcpy(tmp_buf, anc_chip_data_head, 3);
		memcpy(tmp_buf + 3, (tx_buf + package * 1024 - 3), reminder);
		tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf, tmp_buf, reminder + 3, NULL, NULL, 0);
	} else {
		retry = 0;
		memcpy(tmp_buf + 3, tx_buf, data_len);
		tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf, tmp_buf, data_len + 3, NULL, NULL, 0);
	}
	/* restore to FIFO mode if has used DMA */
#ifndef CONFIG_SPI_MT65XX_KERNEL510
	if ((data_len + 3) > 32) {
		anc_dev->spi_mcc.com_mod = FIFO_TRANSFER;
		spi_setup(anc_dev->tran_fp_dev);
	}
#endif
	return 0;
}
#endif
#endif
int ancf_proc_init(struct tran_fp_data *tran_fp_dev)
{
	return 0;
}
int ancf_proc_deinit(struct tran_fp_data *tran_fp_dev)
{
	return 0;
}
# if 0
int anc_key_nav_adjust_type_value(tran_key_nav_event_t *key_nav_p, unsigned int cmd)
{
	switch(cmd)
	{
	    case TRAN_IOC_NAV_EVENT:
		switch(*key_nav_p)
		{
		case TRAN_KEY_NAV_HOME:
			*key_nav_p = TRAN_KEY_NAV_FINGER_UP;
			break;
		case TRAN_KEY_NAV_POWER:
			*key_nav_p = TRAN_KEY_NAV_FINGER_DOWN;
			break;
		case TRAN_KEY_NAV_MENU:
			*key_nav_p = TRAN_KEY_NAV_UP;
			break;
		case TRAN_KEY_NAV_BACK:
			*key_nav_p = TRAN_KEY_NAV_DOWN;
			break;
		case TRAN_KEY_NAV_CAMERA:
			*key_nav_p = TRAN_KEY_NAV_LEFT;
			break;
		case TRAN_KEY_NAV_FINGER_UP:
			*key_nav_p = TRAN_KEY_NAV_RIGHT;
			break;
		case TRAN_KEY_NAV_FINGER_DOWN:
			*key_nav_p = TRAN_KEY_NAV_CLICK;
			break;
		case TRAN_KEY_NAV_UP:
			*key_nav_p = TRAN_KEY_NAV_HEAVY;
			break;
		case TRAN_KEY_NAV_DOWN:
			*key_nav_p = TRAN_KEY_NAV_LONG_PRESS;
			break;
		case TRAN_KEY_NAV_LEFT:
			*key_nav_p = TRAN_KEY_NAV_DOUBLE_CLICK;
			break;
		default:
			break;
		}
		break;
	    case TRAN_IOC_INPUT_KEY_EVENT:
		switch(*key_nav_p)
		{
		case TRAN_KEY_NAV_HOME:
			*key_nav_p = TRAN_KEY_NAV_F28;
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
#endif
