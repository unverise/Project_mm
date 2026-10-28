/*
 * Copyright (C) 2013-2016, JIIOV Technology Co., Ltd.
 * All Rights Reserved.
 */
#ifndef __ANC_SPI_TEE_H
#define __ANC_SPI_TEE_H
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/cdev.h>
#include <linux/input.h>
#ifndef CONFIG_SPI_MT65XX_KERNEL510
#include "mtk_spi.h"
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#else
#include <linux/notifier.h>
#endif
#define ANC_LINUX_VERSION "V1.01.04"
#define ANC_NETLINK_ROUTE 30
#define MAX_NL_MSG_LEN 16
/**********************IO Magic**********************/
#define ANC_IOC_MAGIC	'a'
/* define for anc_spi_cfg_t->speed_hz */
#define ANC_SPI_SPEED_LOW 1
#define ANC_SPI_SPEED_MEDIUM 6
#define ANC_SPI_SPEED_HIGH 9
#include "tran_fp_common_data_def.h"
struct anc_ioc_transfer_raw {
    u32 len;
    u8 *read_buf;
    u8 *write_buf;
    uint32_t high_time;
    uint32_t low_time;
};
enum anc_spi_cpol {
    ANC_SPI_CPOL_0,
    ANC_SPI_CPOL_1
};
enum anc_spi_cpha {
    ANC_SPI_CPHA_0,
    ANC_SPI_CPHA_1
};
typedef struct {
    unsigned int cs_setuptime;
    unsigned int cs_holdtime;
    unsigned int cs_idletime;
    unsigned int speed_hz; //spi clock rate
    unsigned int duty_cycle; //The time ratio of active level in a period. Default value is 50. that means high time and low time is same.
    enum anc_spi_cpol cpol;
    enum anc_spi_cpol cpha;
} anc_spi_cfg_t;

typedef enum {
        ANC_KEY_NONE = 0,
        ANC_KEY_HOME,
        ANC_KEY_MENU,
        ANC_KEY_BACK,
        ANC_KEY_UP,
        ANC_KEY_LEFT,
        ANC_KEY_RIGHT,
        ANC_KEY_DOWN,
        ANC_KEY_POWER,
        ANC_KEY_WAKEUP,
        ANC_KEY_CAMERA,
} ANC_KEY_TYPE;

typedef struct {
        ANC_KEY_TYPE key;
        int value; /* 0 = key up, 1 = key down */
} ANC_KEY_EVENT;

typedef struct {
        char product_id[16];
} __attribute__((packed)) ANC_SENSOR_PRODUCT_INFO;

/* define commands */

#define ANC_IOC_RESET _IO(ANC_IOC_MAGIC, 0)
#define ANC_IOC_REQUEST_RESOURCE _IO(ANC_IOC_MAGIC, 1)
#define ANC_IOC_RELEASE_RESOURCE _IO(ANC_IOC_MAGIC, 2)
#define ANC_IOC_ENABLE_IRQ _IO(ANC_IOC_MAGIC, 3)
#define ANC_IOC_DISABLE_IRQ _IO(ANC_IOC_MAGIC, 4)
#define ANC_IOC_INIT_IRQ _IO(ANC_IOC_MAGIC, 5)
#define ANC_IOC_DEINIT_IRQ _IO(ANC_IOC_MAGIC, 6)
#define ANC_IOC_SET_IRQ_FLAG_MASK _IO(ANC_IOC_MAGIC, 7)
#define ANC_IOC_CLEAR_IRQ_FLAG_MASK _IO(ANC_IOC_MAGIC, 8)
#define ANC_IOC_ENABLE_SPI_CLK _IO(ANC_IOC_MAGIC, 9)
#define ANC_IOC_DISABLE_SPI_CLK _IO(ANC_IOC_MAGIC, 10)
#define ANC_IOC_ENABLE_POWER _IO(ANC_IOC_MAGIC, 11)
#define ANC_IOC_DISABLE_POWER _IO(ANC_IOC_MAGIC, 12)
#define ANC_IOC_SPI_SPEED _IOW(ANC_IOC_MAGIC, 13, uint32_t)
#define ANC_IOC_CLEAR_FLAG _IO(ANC_IOC_MAGIC, 14)
#define ANC_IOC_WAKE_LOCK _IO(ANC_IOC_MAGIC, 15)
#define ANC_IOC_WAKE_UNLOCK _IO(ANC_IOC_MAGIC, 16)
#define ANC_IOC_REPORT_KEY_EVENT _IOW(ANC_IOC_MAGIC, 17, ANC_KEY_EVENT)
#define ANC_IOC_SET_PRODUCT_ID _IOW(ANC_IOC_MAGIC, 18, ANC_SENSOR_PRODUCT_INFO)
#define ANC_IOC_GET_IRQ_PIN_LEVEL _IOR(ANC_IOC_MAGIC, 19, uint8_t)

/**************************REE SPI******************************/
#ifndef SUPPORT_REE_SPI
#define SUPPORT_REE_SPI	
#endif
#ifdef SUPPORT_REE_SPI
#define HIGH_SPEED 6
#define LOW_SPEED  1
#define ERR_NO_SENSOR    111
#define ERR_FW_DESTROY   112
#define ERR_PREPARE_FAIL 113
/**********************function defination**********************/
irqreturn_t anc_irq(int irq, void *handle);
int ancf_fb_notifier_callback(struct notifier_block *self,unsigned long event, void *data);
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);
extern int tran_init_eint(struct tran_fp_data*tran_fp_dev);
extern void tran_fp_reset_gpio_high(struct tran_fp_data *tran_fp_dev);
#endif
#endif	/* __ANC_SPI_TEE_H */
