#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/compat.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/seq_file.h>
#include <linux/of_irq.h>
//#define SPI_DRV_COMPATIBLE
#define sileadDBG
#include <linux/jiffies.h>
#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include "slspi.h"
#include <mtk_spi.h>
#include <mtk_spi_hal.h>


#define MTK_6757_GPIO_OFFSET 318
#define MTK_SPEED_BASE 1000000000

extern void mt_spi_enable_master_clk(struct spi_device *spidev);
#ifndef SLPT
static struct mt_chip_conf chip_config = {
		.setuptime =10,//6, //10,//15,//10 ,//3,
		.holdtime =10, //6,//10,//15,//10,//3,
		.high_time = 30,//4,//6,//8,//12, //25,//8,      //10--6m   15--4m   20--3m  30--2m  [ 60--1m 120--0.5m  300--0.2m]
		.low_time = 30,//4,//6,//8,//12,//25,//8,
		.cs_idletime = 2,//30,// 60,//100,//12,
		.ulthgh_thrsh = 0,

		.rx_mlsb = SPI_MSB, 
		.tx_mlsb = SPI_MSB,		 
		.tx_endian = 0,
		.rx_endian = 0,

		.cpol = SPI_CPOL_0,
		.cpha = SPI_CPHA_0,
		
		.com_mod = DMA_TRANSFER,
		.pause = 0,
		.finish_intr = 1,
};
#endif

int gsl_spi_open_clock_set(struct GSL_DEV_SEL_device *spi)
{
#ifndef SLPT
	mt_spi_enable_master_clk(spi);
#else
    struct spidev_data	*spidev;
	spidev = platform_get_drvdata(spi);
	clk_enable(spidev->clk);
    SL_KLOGD("%s thomas entered \n",__func__);
#endif
	return 0;
}

int gsl_spi_close_clock_set(struct GSL_DEV_SEL_device *spi)
{
#ifndef SLPT
	mt_spi_disable_master_clk(spi);
#else
    struct spidev_data	*spidev;
	spidev = platform_get_drvdata(spi);
	clk_disable(spidev->clk);
    SL_KLOGD("%s thomas entered \n",__func__);
#endif
	return 0;
}
	
int gsl_spi_speed_set(struct GSL_DEV_SEL_device *spi)
{
#ifndef SLPT
            chip_config.high_time = MTK_SPEED_BASE/((spi->max_speed_hz)* 2 * 10);
            chip_config.low_time = MTK_SPEED_BASE/((spi->max_speed_hz)* 2 * 10);
            SL_KLOGD("high_time = %d low_time = %d\n", chip_config.high_time, chip_config.low_time);
#endif
            return 0;
}

int gsl_spidev_set_before_rd_chipid(struct GSL_DEV_SEL_device *spi)
{
	int rv = 0;
#ifndef SLPT
	spi->max_speed_hz = SPI_SPEED;
	spi->bits_per_word = SPI_BITS;
	spi->mode = SPI_MODE_0;
	spi->controller_data = &chip_config;
#ifdef SPI_DRV_COMPATIBLE
	chip_config.com_mod = FIFO_TRANSFER;
#endif
	rv = spi_setup(spi);
	if(rv < 0) {
		SL_KLOGD("spi master set mode failed");
		return rv;
	}
#endif
	return rv;
}

int gsl_spidev_set_after_rd_chipid(struct GSL_DEV_SEL_device *spi)
{
	int rv = 0;
#ifndef SLPT
#ifdef SPI_DRV_COMPATIBLE
	chip_config.com_mod = DMA_TRANSFER;
	rv = spi_setup(spi);
	if(rv < 0) {
		SL_KLOGD("spi master set mode failed");
		return rv;
	}
#endif
#endif
	return rv;
}
	
long gsl_fp_pinctrl_init(struct spidev_data * spidev)
{
	long ret = 0;
#ifdef SLPT
	spidev->clk = devm_clk_get(&spidev->spi->dev, "main");
	if (IS_ERR(spidev->clk)) {
		SL_KLOGD("cannot get spi main clock or dma clock. main clk err : %ld .\n",PTR_ERR(spidev->clk));
		return PTR_ERR(spidev->clk);
	}
#endif
	spidev->fp_pinctrl=devm_pinctrl_get(&spidev->spi->dev);
	if(IS_ERR(spidev->fp_pinctrl)){
		SL_KLOGD("devm_pinctrl_get failed!");
		ret=PTR_ERR(spidev->fp_pinctrl);
		goto pinctrl_get_fail;
	}
	spidev->pinctrl_state_active=pinctrl_lookup_state(spidev->fp_pinctrl,GSL_FP_RESET_ACTIVE);
	if(IS_ERR(spidev->pinctrl_state_active)){
		SL_KLOGD("pinctrl_lookup_state reset_active failed!");
		ret=PTR_ERR(spidev->pinctrl_state_active);
		goto lookup_state_fail;
	}
	spidev->pinctrl_state_suspend=pinctrl_lookup_state(spidev->fp_pinctrl,GSL_FP_RESET_DEACTIVE );
	if(IS_ERR(spidev->pinctrl_state_suspend)){
		SL_KLOGD("pinctrl_lookup_state reset_deactive failed!");
		ret=PTR_ERR(spidev->pinctrl_state_suspend);
		goto lookup_state_fail;
	}
	spidev->pinctrl_state_interrupt=pinctrl_lookup_state(spidev->fp_pinctrl,GSL_FP_IRQ_ACTIVE);
	if(IS_ERR(spidev->pinctrl_state_interrupt)){
		SL_KLOGD("pinctrl_lookup_state irq_active failed!");
		ret=PTR_ERR(spidev->pinctrl_state_interrupt);
		goto lookup_state_fail;
	}
	return 0;
lookup_state_fail:
	devm_pinctrl_put(spidev->fp_pinctrl);
pinctrl_get_fail:
	spidev->fp_pinctrl=NULL;
	return ret;
}

int spidev_reset_hw(struct spidev_data *spidev)
{
	int rc = 0;
#ifdef sileadDBG
	int value = 2;
#endif
        if(spidev->fp_pinctrl)
        {	
            rc = pinctrl_select_state(spidev->fp_pinctrl, spidev->pinctrl_state_suspend);
            if(rc)
                dev_err(&spidev->spi->dev, "[silead]cannot get suspend pinctrl state\n");
        }
    	mdelay(1);
#ifdef sileadDBG
	value = gpio_get_value_cansleep(spidev->hw_reset_gpio);
	SL_KLOGD("[%s] GPIO %d state is %d\n", __func__,spidev->hw_reset_gpio,value);
#endif
        if(spidev->fp_pinctrl)
        {
            rc = pinctrl_select_state(spidev->fp_pinctrl, spidev->pinctrl_state_active);
            if(rc)
                dev_err(&spidev->spi->dev, "[silead]cannot get active pinctrl state\n");
        }
#ifdef sileadDBG
	value = gpio_get_value_cansleep(spidev->hw_reset_gpio);
	SL_KLOGD("[%s] GPIO %d state is %d\n", __func__,spidev->hw_reset_gpio,value);
#endif
     	return rc; 
}

int spidev_shutdown_hw(struct spidev_data *spidev)
{
	int rc = 0;
#ifdef sileadDBG
	int value = 2;
#endif
        if(spidev->fp_pinctrl)
        {	
            rc = pinctrl_select_state(spidev->fp_pinctrl, spidev->pinctrl_state_suspend);
            if(rc)
                dev_err(&spidev->spi->dev, "[silead]cannot get suspend pinctrl state\n");
        }
#ifdef sileadDBG
	value = gpio_get_value_cansleep(spidev->hw_reset_gpio);
	SL_KLOGD("[%s] GPIO %d state is %d\n", __func__,spidev->hw_reset_gpio,value);
#endif
	 /* 20150817 silead thomas end */
    return rc;
}

int silead_init_eint(struct spidev_data*spidev)	
{
	int ret,irq,debounce=0;
	unsigned gpio_pin;
	int ints[2];
	struct device_node * dn= of_find_compatible_node(NULL,NULL,"mediatek,silead_fp_irq");
	if(IS_ERR(dn)){
		ret=PTR_ERR(dn);
		return ret;
	}
	spidev->irq_node = dn;
	if(dn){
		irq= irq_of_parse_and_map(dn,0);
        	SL_KLOGD("parsed irq = %d\n",irq);
		if(irq<0)
			return -EINVAL;
		of_property_read_u32_array(dn,"debounce",ints,ARRAY_SIZE(ints));
		gpio_pin=ints[0];
		debounce=ints[1];
       		SL_KLOGD("int gpio = %d,debounce=%d\n",gpio_pin,debounce);
		gpio_set_debounce(ints[0],ints[1]);
	}else{
        	SL_KLOGD("irq node not found !\n");
		return -EINVAL;
	}
	spidev->irq=irq;
	return 0;
}

int silead_fp_parse_reset_and_int_gpios(struct spidev_data *spidev)
{
    spidev->hw_reset_gpio = of_get_named_gpio(spidev->spi->dev.of_node,"silead,gpio_reset",0);
    spidev->hw_reset_gpio -= MTK_6757_GPIO_OFFSET;
    SL_KLOGD("spidev->hw_reset_gpio= %d",spidev->hw_reset_gpio);
    spidev->hw_int_gpio= of_get_named_gpio(spidev->spi->dev.of_node,"silead,gpio_irq",0);
    spidev->hw_int_gpio -= MTK_6757_GPIO_OFFSET;
    SL_KLOGD("spidev->hw_int_gpio= %d",spidev->hw_int_gpio);
    return 0;
}

int silead_fp_init_gpio_states(struct spidev_data *spidev)
{
	int status = 0;
	spidev_reset_hw(spidev);
	SL_KLOGD("reset gpio successfull!");
	status = pinctrl_select_state(spidev->fp_pinctrl,spidev->pinctrl_state_interrupt);	
	if(status){
			SL_KLOGD("pinctrl_select_state pinctrl_state_interrupt failed!");
			return status;
	}
	SL_KLOGD("int gpio select status successfull!");
	return status;
}

void silead_register_board_info(void)
{
	return ;
}
