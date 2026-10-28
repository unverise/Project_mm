/*
 * Copyright (C) 2013-2016, Shenzhen Huiding Technology Co., Ltd.
 * All Rights Reserved.
 */
#ifndef __GF_SPI_TEE_H
#define __GF_SPI_TEE_H

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

#define GF_LINUX_VERSION "V1.01.04"
#define GF_NETLINK_ROUTE 30
#define MAX_NL_MSG_LEN 16
/**********************IO Magic**********************/
#define GF_IOC_MAGIC	'g'

/* define for gf_spi_cfg_t->speed_hz */
#define GF_SPI_SPEED_LOW 1
#define GF_SPI_SPEED_MEDIUM 6
#define GF_SPI_SPEED_HIGH 9

#include "tran_fp_common_data_def.h"

struct gf_ioc_transfer_raw {
    u32 len;
    u8 *read_buf;
    u8 *write_buf;
    uint32_t high_time;
    uint32_t low_time;
};

enum gf_spi_cpol {
    GF_SPI_CPOL_0,
    GF_SPI_CPOL_1
};

enum gf_spi_cpha {
    GF_SPI_CPHA_0,
    GF_SPI_CPHA_1
};

typedef struct {
    unsigned int cs_setuptime;
    unsigned int cs_holdtime;
    unsigned int cs_idletime;
    unsigned int speed_hz; //spi clock rate
    unsigned int duty_cycle; //The time ratio of active level in a period. Default value is 50. that means high time and low time is same.
    enum gf_spi_cpol cpol;
    enum gf_spi_cpol cpha;
} gf_spi_cfg_t;
/* define commands */
#define GF_IOC_INIT			_IOR(GF_IOC_MAGIC, 0, uint8_t)
#define GF_IOC_EXIT			_IO(GF_IOC_MAGIC, 1)
#define GF_IOC_RESET			_IO(GF_IOC_MAGIC, 2)

#define GF_IOC_ENABLE_IRQ		_IO(GF_IOC_MAGIC, 3)
#define GF_IOC_DISABLE_IRQ		_IO(GF_IOC_MAGIC, 4)

#define GF_IOC_ENABLE_SPI_CLK           _IOW(GF_IOC_MAGIC, 5, uint32_t)
#define GF_IOC_DISABLE_SPI_CLK		_IO(GF_IOC_MAGIC, 6)

#define GF_IOC_ENABLE_POWER		_IO(GF_IOC_MAGIC, 7)
#define GF_IOC_DISABLE_POWER		_IO(GF_IOC_MAGIC, 8)

#define GF_IOC_INPUT_KEY_EVENT		_IOW(GF_IOC_MAGIC, 9, struct tran_key_nav)

/* fp sensor has change to sleep mode while screen off */
#define GF_IOC_ENTER_SLEEP_MODE		_IO(GF_IOC_MAGIC, 10)
#define GF_IOC_GET_FW_INFO		_IOR(GF_IOC_MAGIC, 11, uint8_t)
#define GF_IOC_REMOVE		_IO(GF_IOC_MAGIC, 12)
#define GF_IOC_CHIP_INFO	_IOW(GF_IOC_MAGIC, 13, struct tran_ioc_chip_info)

#define GF_IOC_NAV_EVENT	_IOW(GF_IOC_MAGIC, 14, tran_key_nav_event_t)

/* for SPI REE transfer */
#define GF_IOC_TRANSFER_CMD		_IOWR(GF_IOC_MAGIC, 15, struct gf_ioc_transfer)
#define GF_IOC_TRANSFER_RAW_CMD	_IOWR(GF_IOC_MAGIC, 16, struct gf_ioc_transfer_raw)
#define GF_IOC_SPI_INIT_CFG_CMD	_IOW(GF_IOC_MAGIC, 17, gf_spi_cfg_t)

#define  GF_IOC_MAXNR    18  /* THIS MACRO IS NOT USED NOW... */
 
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
irqreturn_t gf_irq(int irq, void *handle);
int gf_fb_notifier_callback(struct notifier_block *self,unsigned long event, void *data);
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);
extern int tran_init_eint(struct tran_fp_data*tran_fp_dev);
extern void tran_fp_reset_gpio_high(struct tran_fp_data *tran_fp_dev);

#endif

#endif	/* __GF_SPI_TEE_H */
