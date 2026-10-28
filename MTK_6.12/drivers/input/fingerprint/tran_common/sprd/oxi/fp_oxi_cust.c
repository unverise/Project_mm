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

/* MTK header */
#ifndef CONFIG_SPI_MT65XX_KERNEL510
#include "mt_spi.h"
#include "mt_spi_hal.h"
#endif

/* there is no this file on standardized GPIO platform */
#ifdef CONFIG_MTK_GPIO
#include "mtk_gpio.h"
#include "mach/gpio_const.h"
#endif

#include <mt-plat/sync_write.h>
#include <linux/of_address.h>

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifdef CONFIG_SPI_MT65XX_KERNEL510
#include <linux/platform_data/spi-mt65xx.h>
#endif
#endif

#include  <linux/regulator/consumer.h>

#define CONFIG_TRAN_FP_POWER_CTRL_REGULATOR

#include "oxifp_drv.h"
#include "tran_spi.h"


int of_spi_transfer(unsigned char *chip_id_str);
extern struct tran_fp_data * g_tran_fp_datap;

extern int tran_fp_spi_transfer(void *chip_conf, int cfg_len, const char *txbuf, char *rxbuf, int len);

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifndef CONFIG_SPI_MT65XX_KERNEL510
extern struct mt_chip_conf tran_chip_config; 
#else
extern struct mtk_chip_config tran_chip_config;
#endif
#endif

static int of_spi_read_bytes_ree(struct tran_fp_data *of_dev, u16 addr, u32 data_len, u8 *rx_buf);

int of_spi_transfer(unsigned char *chip_id_str)
{
	int status = -EFAULT;
	unsigned char rx_test[10] = {0};
	status = of_spi_read_bytes_ree(g_tran_fp_datap, 0x3EFF, 2, rx_test);
    TRAN_FP_INFO("%s rx_test fp id:0x%x 0x%x \n", __func__,rx_test[0],rx_test[1]);

	memcpy(chip_id_str, rx_test, 2);
	return 0;
}

static int of_spi_read_bytes_ree(struct tran_fp_data *of_dev, u16 addr, u32 data_len, u8 *rx_buf)
{
	 u8 *tmp_buf = NULL;
	 u32 package, reminder, retry;
	 u8 gf_chip_data_head[] = {(addr >> 8) & 0xFF, addr & 0xFF};
	 int cfg_len;
 	 cfg_len = sizeof(struct mt_chip_conf);
	 tmp_buf = of_dev->tran_buffer;
	 package = (data_len + 2) / 1024;
	 reminder = (data_len + 2) % 1024;
 	 memcpy(tmp_buf, gf_chip_data_head, 2);
	 
#ifndef CONFIG_SPI_MT65XX_KERNEL510
	 /* switch to DMA mode if transfer length larger than 32 bytes */
	 if ((data_len + 1) > 32) {
		 of_dev->spi_mcc.com_mod = DMA_TRANSFER;
		 spi_setup(of_dev->tran_fp_dev);
	 }
#endif
	 
	 if ((package > 0) && (reminder != 0)) { 
/*
		 retry = 1;
		 tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf, tmp_buf, 3);
		 tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf + 4, tmp_buf + 4, package * 1024);
		 memcpy(rx_buf, (tmp_buf + 5), (package * 1024 - 1));

		 addr = addr + package * 1024 - 2;
		 memset(tmp_buf, 0, package * 1024 - 1);
		 gf_chip_data_head[1] = (addr >> 8) & 0xFF;
		 gf_chip_data_head[2] = addr & 0xFF;
		 memcpy(tmp_buf, gf_chip_data_head, 5);
		 tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf, tmp_buf, 3);
		 tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf + 4, tmp_buf + 4, reminder + 1);
		 memcpy((rx_buf + package * 1024 - 1), tmp_buf + 6, reminder - 1);
*/
	 } else {
		 retry = 0;
		 tran_fp_spi_transfer(&tran_chip_config, cfg_len, tmp_buf, tmp_buf, 2);
		 memcpy(rx_buf, tmp_buf, data_len);
	 }

#ifndef CONFIG_SPI_MT65XX_KERNEL510 
	 /* restore to FIFO mode if has used DMA */
	 if ((data_len + 1) > 32) {
		 of_dev->spi_mcc.com_mod = FIFO_TRANSFER;
		 spi_setup(of_dev->tran_fp_dev);
	 }
#endif
	 return 0;
}
