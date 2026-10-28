
/**
 * Copyright (C) 2015 Transsion Inc
 * Author fan.peng
 * This file is used to implement the extended functions(such as power enabling/disabling, spi clk enabling/disabling, etc.) for fingerprint device driver
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
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
#include <linux/cdev.h>
#include <linux/jiffies.h>
#include <linux/types.h>
#include <linux/spi/spi.h>
#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#include <linux/proc_fs.h>
#include <linux/of_irq.h>
#include <linux/sched.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>
#include <linux/of_platform.h>

#define CONFIG_SPI_MT65XX_KERNEL510 y

#if !defined(CONFIG_SPI_MT65XX_KERNEL510)
#include <mt_spi.h>
#include <mt_spi_hal.h>
#else
#include "tran_spi.h"
#endif

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifdef CONFIG_SPI_MT65XX_KERNEL510
#include <linux/platform_data/spi-mt65xx.h>
#endif
#endif

#include <internal.h>
#include "tran_fp_ext.h"
#include <linux/of.h>

#define MTK_SPEED_BASE 1000000000
#define CONFIG_TRAN_PROC_FS
#define TRAN_FP_VENDOR_NUMS 9

#ifdef CONFIG_SPI_MT65XX_KERNEL510
u32 tran_spi_speed = 1*1000000;
#endif

#define SPI_SYNC_RTY_CNT 5
#define READ_FP_CHIP_ID_RTY_CNT 3

typedef int (*tran_spi_transfer)(unsigned char *chip_id_str);

struct attribute_group tran_fp_attribute_group = {NULL};
struct work_struct ff_core_work;

extern struct tran_fp_data	* g_tran_fp_datap;
extern struct attribute *sf_sysfs_entries[];
extern struct attribute *ff_sysfs_entries[];
extern struct attribute *gf_sysfs_entries[];
extern struct platform_driver tran_fp_platform_driver;
extern void mt_spi_enable_master_clk(struct spi_device *spidev);

static int open_clk_count = 0, close_clk_count = 1;

g_fp_name_e g_fp_name_flg;

static int pid;
static int tran_fp_reset_sensor(struct tran_fp_data *tran_fp_dev, u32 inter_delay, u32 post_delay);
static int tran_fp_open_clock(struct tran_fp_data *tran_fp_datap);
static int tran_fp_close_clock(struct tran_fp_data *tran_fp_datap);
static long tran_fp_pinctrl_init(struct tran_fp_data * tran_fp_dev);

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifndef CONFIG_SPI_MT65XX_KERNEL510
struct mt_chip_conf tran_chip_config = {
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
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#else
struct mtk_chip_config tran_chip_config = {
	.sample_sel = 0,
	.cs_setuptime = 0,
	.cs_holdtime = 0,
	.cs_idletime = 0,
	.tick_delay = 0,
};
#endif
//BSP: modify [fix section mismatch issue during build time] XLLLHLWS-309 by fan.peng 20191112 start
/*
   static struct spi_board_info tran_spi_board_devs[] = {
   [0] = {
   .modalias = "tran_fp",
   .bus_num = 0,
   .chip_select = 0,
   .mode = SPI_MODE_0,
   },
   };
   */
#else
static struct platform_device_info tran_plat_board_devs[] = {
	[0] = {
		.name = "tran_fp",
	},
};
#endif
//BSP: modify [fix section mismatch issue during build time] XLLLHLWS-309 by fan.peng 20191112 end

static tran_spi_transfer tran_spi_transfer_p_arr[TRAN_FP_VENDOR_NUMS] = {NULL};

void yxy_test_func(void)
{
	return;
}
EXPORT_SYMBOL(yxy_test_func);


/**
 * fingerprint pwoer supply have three ways now.
 * 
 *only regulator power supply ,tran_fp_dev->fp_regulator_support = 1，Need to set voltage.
 *
 *only GPIO power supply，tran_fp_dev->fp_regulator_support = 2，only set gpio 0 or 1.
 *
 *GPIO & regulator power supply,tran_fp_dev->fp_regulator_support = 3，only set gpio 0 or 1.
 */


static int tran_fp_power_init(struct tran_fp_data *tran_fp_dev, bool on)
{
	int rc = 0;
	if(tran_fp_dev->fp_regulator_support == 1) {
		const char buf[FP_NAME_SIZE];
		tran_fp_dev->always_on = buf;
		if (!on) {
			if (tran_fp_dev->power_is_init == 1) {
				if (regulator_count_voltages(tran_fp_dev->vdd) > 0)
					regulator_set_voltage(tran_fp_dev->vdd, tran_fp_dev->min_uV, tran_fp_dev->max_uV);
				regulator_put(tran_fp_dev->vdd);
				tran_fp_dev->power_is_init = 0;
			}
		} else {
			if (tran_fp_dev->power_is_init == 0) {
				tran_fp_dev->vdd = regulator_get(&tran_fp_dev->tran_fp_dev->dev, "vmch");
				if (IS_ERR(tran_fp_dev->vdd)) {
					rc = PTR_ERR(tran_fp_dev->vdd);
					TRAN_FP_ERROR("Regulator get failed vdd rc=%d\n", rc);
					return rc;
				} else {
					if ((rc = of_property_read_u32_array(tran_fp_dev->vdd->rdev->dev.of_node, "regulator-min-microvolt", &(tran_fp_dev->min_uV),1))) {
						TRAN_FP_ERROR("get regulator-min-microvolt failed, rc=%d\n", rc);
						return rc;
					}
					if ((rc = of_property_read_u32_array(tran_fp_dev->vdd->rdev->dev.of_node, "regulator-max-microvolt", &(tran_fp_dev->max_uV),1))) {
						TRAN_FP_ERROR("get regulator-max-microvolt failed, rc=%d\n", rc);
						return rc;
					}
					TRAN_FP_INFO("regulator-min-microvolt = %d uv, regulator-max-microvolt = %d uv", tran_fp_dev->min_uV, tran_fp_dev->max_uV);
					if (of_property_read_string(tran_fp_dev->vdd->rdev->dev.of_node, "regulator-always-on", &(tran_fp_dev->always_on)) == 0)
						TRAN_FP_INFO("regulator-always-on = %s", tran_fp_dev->always_on);
					else
						TRAN_FP_ERROR("regulator-always-on is not set or its property length is too long");
					tran_fp_dev->always_on = NULL;
					if (regulator_count_voltages(tran_fp_dev->vdd) > 0) {
						rc = regulator_set_voltage(tran_fp_dev->vdd, tran_fp_dev->max_uV, tran_fp_dev->max_uV);
						if (rc) {
							TRAN_FP_ERROR("Regulator set failed vdd rc=%d\n",rc);
							goto reg_vdd_put;
						}
					}
					tran_fp_dev->power_is_init = 1;
				}
			}
		}
		return rc;

reg_vdd_put:
		regulator_put(tran_fp_dev->vdd);
	}
	else {
		if (on) {
			if (tran_fp_dev->power_is_init == 0) {
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
				TRAN_FP_INFO("prase tran,gpio_pwr-std \n");
				tran_fp_dev->hw_pwr_gpio = of_get_named_gpio(tran_fp_dev->tran_fp_dev->dev.of_node,"tran,gpio_pwr-std",0);

#else
				TRAN_FP_INFO("prase tran,gpio_pwr ");
				if ((rc = of_property_read_u32_array(tran_fp_dev->tran_fp_dev->dev.of_node, "tran,gpio_pwr", &(tran_fp_dev->hw_pwr_gpio),1))) {
					TRAN_FP_ERROR("get tran,gpio_pwr failed, rc=%d\n", rc);
					return rc;
				}
#endif
				if(tran_fp_dev->fp_regulator_support == 3)//when fingerprint vdd is GPIO and used vmc power,fp_regulator_support == 3
				{
					if (tran_fp_dev->power_is_init == 0){
						tran_fp_dev->vdd = regulator_get(&tran_fp_dev->tran_fp_dev->dev, "vqmmc");
						if (IS_ERR(tran_fp_dev->vdd)) {
							rc = PTR_ERR(tran_fp_dev->vdd);
							TRAN_FP_ERROR("vqmcc get failed vdd rc=%d\n", rc);
							return rc;
						}
						if ((rc = of_property_read_u32_array(tran_fp_dev->vdd->rdev->dev.of_node, "regulator-min-microvolt", &(tran_fp_dev->min_uV),1))) {
							TRAN_FP_ERROR("get regulator-min-microvolt failed, rc=%d\n", rc);
							return rc;
						}
						if ((rc = of_property_read_u32_array(tran_fp_dev->vdd->rdev->dev.of_node, "regulator-max-microvolt", &(tran_fp_dev->max_uV),1))) {
							TRAN_FP_ERROR("get regulator-max-microvolt failed, rc=%d\n", rc);
							return rc;
						}
						TRAN_FP_INFO("vqmmc-regulator-min-microvolt = %d uv, vqmmc-regulator-max-microvolt = %d uv", tran_fp_dev->min_uV, tran_fp_dev->max_uV);
						if (regulator_count_voltages(tran_fp_dev->vdd) > 0) {
							rc = regulator_set_voltage(tran_fp_dev->vdd, tran_fp_dev->min_uV, tran_fp_dev->max_uV);//Not setting a specific voltage, only setting a range, because VMC power supply may be used for other modules
							if (rc) {
								TRAN_FP_ERROR("Regulator set failed vdd rc=%d\n",rc);
								goto reg_vdd_put;
							}
						}
					}
				}
				TRAN_FP_INFO("tran_fp_dev->hw_pwr_gpio= %d",tran_fp_dev->hw_pwr_gpio);
				if (gpio_is_valid(tran_fp_dev->hw_pwr_gpio)) {
					rc = devm_gpio_request(&tran_fp_dev->tran_fp_dev->dev,tran_fp_dev->hw_pwr_gpio,"tran_pwr_gpio");
					if (rc) {
						TRAN_FP_ERROR("devm_gpio_request pwr_gpio failed! rc = %d",rc);
						return -EFAULT;
					} else {
						tran_fp_dev->power_is_init = 1;
						TRAN_FP_INFO("devm_gpio_request pwr_gpio success!\n");
					}
				} else {
					TRAN_FP_ERROR("the state of pwr_gpio is invalid");
					return -EINVAL;
				}
			}
		} else {
			if (tran_fp_dev->power_is_init == 1) {
				if (gpio_is_valid(tran_fp_dev->hw_pwr_gpio)) {
					gpio_free(tran_fp_dev->hw_pwr_gpio);
					tran_fp_dev->power_is_init = 0;
					TRAN_FP_INFO("devm_gpio_free pwr_gpio success!");
				} else {
					TRAN_FP_ERROR("the state of pwr_gpio is invalid!");
					return -EINVAL;
				}
			}
		}
	}
	return rc;
}

static int tran_fp_power_onoff(struct tran_fp_data *tran_fp_datap,bool onoff)
{
	int rc = 0;
	if (tran_fp_datap->power_is_init == 1) {
		if(tran_fp_datap->fp_regulator_support == 1) {
			if (onoff) {
				if (tran_fp_datap->power_is_on == 0) {
					rc = regulator_enable(tran_fp_datap->vdd);
					if (rc) {
						TRAN_FP_ERROR("Regulator vdd enable failed rc=%d\n", rc);
						goto fp_power_done;
					}
					tran_fp_datap->power_is_on = 1;
				}
			} else {
				if (tran_fp_datap->power_is_on == 1) {
					rc = regulator_disable(tran_fp_datap->vdd);
					if (rc) {
						TRAN_FP_ERROR("Regulator vdd disable failed rc=%d\n", rc);
						goto fp_power_done;
					}
					tran_fp_datap->power_is_on = 0;
				}
			}

		} else {
			if(tran_fp_datap->fp_regulator_support == 3){
				if (onoff) {
					if (tran_fp_datap->power_is_on == 0) {
						rc = regulator_enable(tran_fp_datap->vdd);
						if (rc) {
							TRAN_FP_ERROR("Regulator vqmmc vdd enable failed rc=%d\n", rc);
							goto fp_power_done;
						}
						//tran_fp_datap->power_is_on = 1;//power_is_on in here don't modify,will be  GPIO modify  later.
					}
				} else {
					if (tran_fp_datap->power_is_on == 1) {
						rc = regulator_disable(tran_fp_datap->vdd);
						if (rc) {
							TRAN_FP_ERROR("Regulator vqmmc vdd disable failed rc=%d\n", rc);
							goto fp_power_done;
						}
						//tran_fp_datap->power_is_on = 0;//power_is_on in here don't modify,will be  GPIO modify  later.
					}
				}	
			}
			if (onoff) {
				if (tran_fp_datap->power_is_on == 0) {
					gpio_direction_output(tran_fp_datap->hw_pwr_gpio,1);
					mdelay(1);
					tran_fp_datap->power_is_on = 1;
				}
			} else {
				if (tran_fp_datap->power_is_on == 1) {
					gpio_direction_output(tran_fp_datap->hw_pwr_gpio,0);
					mdelay(1);
					tran_fp_datap->power_is_on = 0;
				}
			}
		}
	}
fp_power_done:
	return rc;
}

int tran_fp_get_power_val(struct tran_fp_data *tran_fp_dev,PWR_VAL_U * cur_val)
{
	int rc = 0;
	if(tran_fp_dev->fp_regulator_support == 1) {
		if (IS_ERR(tran_fp_dev->vdd)) {
			rc = PTR_ERR(tran_fp_dev->vdd);
			TRAN_FP_ERROR("Regulator vdd is invalid,rc=%d\n", rc);
			return rc;
		} else {
			(*cur_val).rgltor_val.vdd_val = regulator_get_voltage(tran_fp_dev->vdd);
			//	(*cur_val).rgltor_val.vio_val = regulator_get_voltage(tran_fp_dev->vio);
			TRAN_FP_INFO("cur vdd_val = %d  uv ",(*cur_val).rgltor_val.vdd_val);
		}
	}
	else {
		if (gpio_is_valid(tran_fp_dev->hw_pwr_gpio)) {
			(*cur_val).gpio_val = gpio_get_value_cansleep(tran_fp_dev->hw_pwr_gpio);
			TRAN_FP_INFO("cur pwr_gpio_val = %d ", (*cur_val).gpio_val);
		} else {
			TRAN_FP_ERROR("the state of pwr_gpio is invalid!");
			return -EINVAL;
		}
	}
	return rc;
}

int tran_fp_parse_dts_file(struct tran_fp_data * tran_fp_dev)
{
#ifdef CONFIG_OF
	int value, ret;
	unsigned char tran_tee_afinity_tmp = 0 , i = 0;
	struct device_node *node = NULL, *np = NULL, *child = NULL;
	const char *str = NULL;
	TRAN_FP_INFO(" from dts pinctrl\n");

	node = of_find_compatible_node(NULL, NULL, "tran_fp_legacy");
	//tran_fp_dev->tran_fp_dev.dev->tran_fp_dev = node;
	if (node) {
		of_property_read_u32(node, "netlink-event", &value);
		TRAN_FP_INFO(" get netlink event[%d] from dts\n", value);
		ret = tran_fp_pinctrl_init(tran_fp_dev);
		if (ret) {
			TRAN_FP_ERROR(" tran_fp_pinctrl_init failed");
			return -EFAULT;
		} else
			TRAN_FP_INFO(" tran_fp_pinctrl_init success!\n");
	} else {
		TRAN_FP_ERROR(" device node is null\n");
		return -EINVAL;
	}
	if(of_property_read_string(node,"fod_location_xy",&str)) {
		strcpy(tran_fp_dev->fod_location_xy,"null");
		TRAN_FP_ERROR("fod_location_xy is null\n");
	} else {
		strcpy(tran_fp_dev->fod_location_xy,str);
		TRAN_FP_INFO("fod_location_xy = %s\n",tran_fp_dev->fod_location_xy);
	}
	if(of_property_read_bool(node,"tran_fp_sched")) {
		tran_fp_dev->tran_fp_sched = 1;
		TRAN_FP_INFO("tran_fp_sched = %d\n",tran_fp_dev->tran_fp_sched);
	} else {
		tran_fp_dev->tran_fp_sched = 0;
		TRAN_FP_ERROR("tran_fp_sched is disable\n");
	}
	if(of_property_read_bool(node,"vmch-supply")) 
		tran_fp_dev->fp_regulator_support = 1;
	else if(of_property_read_bool(node,"tran,gpio_pwr") || of_property_read_bool(node,"tran,gpio_pwr-std")) 
	{
		if(of_property_read_bool(node,"vqmmc-supply"))
			tran_fp_dev->fp_regulator_support = 3;
		else
			tran_fp_dev->fp_regulator_support = 2;
	}
	else tran_fp_dev->fp_regulator_support = 0;
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	tran_fp_dev->reset_gpio = of_get_named_gpio(tran_fp_dev->tran_fp_dev->dev.of_node,"reset-gpio-std",0);
	if (tran_fp_dev->reset_gpio < 0) {
		TRAN_FP_ERROR("%s: get fp RST GPIO failed (%d)", __func__, tran_fp_dev->reset_gpio);
		return -EINVAL;
	}

	tran_fp_dev->irq_gpio = of_get_named_gpio(tran_fp_dev->tran_fp_dev->dev.of_node,"irq-gpio-std",0);
	if (tran_fp_dev->irq_gpio < 0) {
		TRAN_FP_ERROR("%s: get fp IRQ GPIO failed (%d)", __func__, tran_fp_dev->irq_gpio);
		return -EINVAL;
	}
#else
	if ((ret = of_property_read_u32_array(tran_fp_dev->tran_fp_dev->dev.of_node, "reset-gpio", &(tran_fp_dev->reset_gpio),1))) {
		TRAN_FP_ERROR("get reset-gpio failed, ret=%d\n", ret);
		return ret;
	}
	if ((ret = of_property_read_u32_array(tran_fp_dev->tran_fp_dev->dev.of_node, "irq-gpio", &(tran_fp_dev->irq_gpio),1))) {
		TRAN_FP_ERROR("get irq-gpio failed, ret=%d\n", ret);
		return ret;
	}
#endif
	//BSP: add change for tee big core XLBELHQQS-9088 by xiaohong.huang 2022.08.17 start
	np = of_find_node_by_path("/cpus/cpu-map/cluster0");
	if(np) {
		for_each_child_of_node(np,child) {
			if (of_property_read_bool(child,"cpu"))
				tran_tee_afinity_tmp = ( 1 << i++ );
			tran_fp_dev->tran_tee_afinity |= tran_tee_afinity_tmp;
		}
		tran_fp_dev->tran_tee_afinity = ~tran_fp_dev->tran_tee_afinity;
		TRAN_FP_INFO(" tran_fp_dev->tran_tee_afinity = %x\n",tran_fp_dev->tran_tee_afinity);	
	} else TRAN_FP_ERROR(" /cpus/cpu-map/cluster0 node is null\n");
	of_node_put(np);
	//BSP: add change for tee big core XLBELHQQS-9088 by xiaohong.huang 2022.08.17 end

	TRAN_FP_INFO(" parse dts success!\n");
#endif
	return 0;

}

int tran_init_eint(struct tran_fp_data*tran_fp_dev)
{
	int ret = -EFAULT, irq = 0, debounce = 0;
	unsigned gpio_pin;
	int ints[2];
	struct device_node * dn= of_find_compatible_node(NULL,NULL,"tran_fp_legacy");
	if(IS_ERR(dn)){
		ret=PTR_ERR(dn);
		return ret;
	}
	tran_fp_dev->irq_node = dn;
	if(dn){
		irq = irq_of_parse_and_map(dn,0);
		TRAN_FP_INFO("parsed irq = %d\n",irq);
		if (irq < 0) {
			TRAN_FP_ERROR("irq number get error!\n");
			return -EINVAL;
		}
		if ((ret = of_property_read_u32_array(dn,"debounce",ints,ARRAY_SIZE(ints)))) {
			TRAN_FP_ERROR("get debounce failed, ret=%d\n", ret);
			return ret;
		}
		gpio_pin=ints[0];
		debounce=ints[1];
		TRAN_FP_INFO("int gpio = %d,debounce=%d\n",gpio_pin,debounce);
		gpiod_set_debounce(gpio_to_desc(gpio_pin),debounce);
	}else{
		TRAN_FP_ERROR("irq node not found !\n");
		return -EINVAL;
	}
	tran_fp_dev->irq = irq;
	return 0;
}

static int tran_fp_reset_sensor(struct tran_fp_data *tran_fp_dev, u32 inter_delay, u32 post_delay)
{
	int rc = 0;
	int value = 2;
	(void)value;
	if(tran_fp_dev->tran_pinctrl)
	{
		rc = pinctrl_select_state(tran_fp_dev->tran_pinctrl, tran_fp_dev->pins_reset_low);
		if(rc) {
			TRAN_FP_ERROR("[tran]cannot set suspend pinctrl state\n");
			goto reset_stat_select_fail;
		}
	}
	udelay(inter_delay);
	value = gpio_get_value_cansleep(tran_fp_dev->reset_gpio);
	TRAN_FP_INFO("GPIO %d state is %d\n",tran_fp_dev->reset_gpio,value);
	if(tran_fp_dev->tran_pinctrl)
	{
		rc = pinctrl_select_state(tran_fp_dev->tran_pinctrl, tran_fp_dev->pins_reset_high);
		if(rc) {
			TRAN_FP_ERROR("[tran]cannot set active pinctrl state\n");
			goto reset_stat_select_fail;
		}
	}
	if (post_delay) {
		udelay(post_delay);
	}
	value = gpio_get_value_cansleep(tran_fp_dev->reset_gpio);
	TRAN_FP_INFO("GPIO %d state is %d\n",tran_fp_dev->reset_gpio,value);
reset_stat_select_fail:
	return rc;
}

static int tran_fp_shutdown_sensor(struct tran_fp_data *tran_fp_dev)
{
	int rc = 0;
	int value;
	(void)value;
	if(tran_fp_dev->tran_pinctrl)
	{
		rc = pinctrl_select_state(tran_fp_dev->tran_pinctrl, tran_fp_dev->pins_reset_low);
		if(rc)
			TRAN_FP_ERROR("[tran]cannot set suspend pinctrl state\n");
	}
	udelay(2000);
	value = gpio_get_value_cansleep(tran_fp_dev->reset_gpio);
	TRAN_FP_INFO("GPIO %d state is %d\n",tran_fp_dev->reset_gpio,value);
	return rc;
}

void tran_fp_reset_gpio_high(struct tran_fp_data *tran_fp_dev)
{
	int value;
	(void)value;
#ifdef CONFIG_OF
	pinctrl_select_state(tran_fp_dev->tran_pinctrl, tran_fp_dev->pins_reset_high);
#endif
	udelay(2000);
	value = gpio_get_value_cansleep(tran_fp_dev->reset_gpio);
	TRAN_FP_INFO("GPIO %d state is %d\n",tran_fp_dev->reset_gpio,value);
}

static void tran_fp_enable_irq(struct tran_fp_data *tran_fp_dev)
{
	tran_fp_dev->RcvIRQ = 0;
	if (1 == tran_fp_dev->irq_count) {
		TRAN_FP_INFO(" irq already enabled\n");
	} else {
		enable_irq(tran_fp_dev->irq);
		tran_fp_dev->irq_count = 1;
		TRAN_FP_DEBUG(" enable interrupt!\n");
	}
}

static void tran_fp_disable_irq(struct tran_fp_data *tran_fp_dev)
{
	if (0 == tran_fp_dev->irq_count) {
		TRAN_FP_INFO(" irq already disabled\n");
	} else {
		disable_irq_nosync(tran_fp_dev->irq);
		tran_fp_dev->irq_count = 0;
		TRAN_FP_DEBUG(" disable interrupt!\n");
	}
	tran_fp_dev->RcvIRQ = 0;
}

static long tran_fp_pinctrl_init(struct tran_fp_data * tran_fp_dev)
{
	long ret = 0;
#ifndef CONFIG_TRAN_FP_DRV_MODE_SPI
	tran_fp_dev->clk = devm_clk_get(&tran_fp_dev->tran_fp_dev->dev, "main");
	if (IS_ERR(tran_fp_dev->clk)) {
		TRAN_FP_ERROR("cannot get spi main clock or dma clock. main clk err : %ld .\n",PTR_ERR(tran_fp_dev->clk));
		return PTR_ERR(tran_fp_dev->clk);
	}
#endif
	tran_fp_dev->tran_pinctrl = devm_pinctrl_get(&tran_fp_dev->tran_fp_dev->dev);
	if(IS_ERR(tran_fp_dev->tran_pinctrl)){
		TRAN_FP_ERROR("devm_pinctrl_get failed!");
		ret=PTR_ERR(tran_fp_dev->tran_pinctrl);
		goto pinctrl_get_fail;
	}
	tran_fp_dev->pins_reset_high = pinctrl_lookup_state(tran_fp_dev->tran_pinctrl,TRAN_FP_RESET_HIGH);
	if(IS_ERR(tran_fp_dev->pins_reset_high)){
		TRAN_FP_ERROR("pinctrl_lookup_state reset_active failed!");
		ret=PTR_ERR(tran_fp_dev->pins_reset_high);
		goto lookup_state_fail;
	}
	tran_fp_dev->pins_reset_low = pinctrl_lookup_state(tran_fp_dev->tran_pinctrl,TRAN_FP_RESET_LOW);
	if(IS_ERR(tran_fp_dev->pins_reset_low)){
		TRAN_FP_ERROR("pinctrl_lookup_state reset_deactive failed!");
		ret=PTR_ERR(tran_fp_dev->pins_reset_low);
		goto lookup_state_fail;
	}
	tran_fp_dev->pins_irq = pinctrl_lookup_state(tran_fp_dev->tran_pinctrl,TRAN_FP_IRQ);
	if(IS_ERR(tran_fp_dev->pins_irq)){
		TRAN_FP_ERROR("pinctrl_lookup_state irq_active failed!");
		ret=PTR_ERR(tran_fp_dev->pins_irq);
		goto lookup_state_fail;
	}
	tran_fp_dev->pins_spi_default = pinctrl_lookup_state(tran_fp_dev->tran_pinctrl,TRAN_FP_SPI_DEFAULT);
	if(IS_ERR(tran_fp_dev->pins_spi_default)){
		TRAN_FP_ERROR("pinctrl_lookup_state spi_default failed!");
		ret=PTR_ERR(tran_fp_dev->pins_spi_default);
		goto no_spi_dts_default_config_no_influence;
	}
no_spi_dts_default_config_no_influence:
	tran_fp_dev->pins_spi_gpio = pinctrl_lookup_state(tran_fp_dev->tran_pinctrl,TRAN_FP_SPI_GPIO);
	if(IS_ERR(tran_fp_dev->pins_spi_gpio)){
		ret = PTR_ERR(tran_fp_dev->pins_spi_gpio);
		TRAN_FP_ERROR("pinctrl_lookup_state spi_gpio failed! ret = %ld", ret);
	}
	return 0;
lookup_state_fail:
pinctrl_get_fail:
	tran_fp_dev->tran_pinctrl=NULL;
	return ret;
}

static int tran_fp_resource_init(struct tran_fp_data *tran_fp_dev)
{
	int ret = 0;
	ret = tran_init_eint(tran_fp_dev);
	if (ret < 0) {
		TRAN_FP_ERROR("tran_init_eint error!\n");
		return -EINVAL;
	}
	spin_lock_init(&tran_fp_dev->spi_lock);
	mutex_init(&tran_fp_dev->buf_lock);
	mutex_init(&tran_fp_dev->release_lock);
	INIT_LIST_HEAD(&tran_fp_dev->device_entry);
#ifdef CONFIG_PM_SLEEP
	tran_fp_dev->tran_wakelock = wakeup_source_register(NULL, "tran_wakelock");
#else
	wake_lock_init(&tran_fp_dev->tran_wakelock, WAKE_LOCK_SUSPEND, "tran_wakelock");
#endif
	init_waitqueue_head(&tran_fp_dev->wq_irq_return);
	tran_fp_dev->wqueue = create_singlethread_workqueue("tran_fp_wq");
	return ret;
}
//qing.li start
static int tran_fp_set_clock(struct tran_fp_data *tran_fp_datap, bool enable)
{
	int ret = 0;

	if (enable) {
		if (!atomic_read(&tran_fp_datap->spi_clk_onoff_cnt)) {
			TRAN_FP_INFO("[%s] enable clk\n", __func__);
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
			mt_spi_enable_master_clk(tran_fp_datap->tran_fp_dev);
#else
			clk_enable(tran_fp_datap->clk);
#endif
		}
		atomic_inc(&tran_fp_datap->spi_clk_onoff_cnt);
	} else if (atomic_read(&tran_fp_datap->spi_clk_onoff_cnt)) {
		atomic_dec(&tran_fp_datap->spi_clk_onoff_cnt);
		if (!atomic_read(&tran_fp_datap->spi_clk_onoff_cnt)) {
			TRAN_FP_INFO("[%s] disable clk\n", __func__);
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
			mt_spi_disable_master_clk(tran_fp_datap->tran_fp_dev);
#else
			clk_disable(tran_fp_datap->clk);
#endif
		}
	} else {
		TRAN_FP_INFO("[%s] unpaired enable/disable (%d)\n", __func__, enable);
	}
	TRAN_FP_INFO("[%s] enable: %d, tran_fp_datap->spi_clk_onoff_cnt: %d, done (%d)\n", __func__, enable, atomic_read(&tran_fp_datap->spi_clk_onoff_cnt), ret);
	return ret;
}
//qing.li end

static int tran_fp_open_clock(struct tran_fp_data *tran_fp_datap)
{
	if (open_clk_count == 0) 
	{
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
		mt_spi_enable_master_clk(tran_fp_datap->tran_fp_dev);
#else
		clk_enable(tran_fp_datap->clk);
#endif
		TRAN_FP_DEBUG("entered \n");
		open_clk_count = 1;
		close_clk_count = 0;
	}
	return 0;
}

static int tran_fp_close_clock(struct tran_fp_data *tran_fp_datap)
{
	if (close_clk_count == 0) {
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
		mt_spi_disable_master_clk(tran_fp_datap->tran_fp_dev);
#else
		clk_disable(tran_fp_datap->clk);
#endif
		TRAN_FP_DEBUG("entered \n");
		close_clk_count = 1;
		open_clk_count = 0;
	}	
	return 0;
}

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifndef CONFIG_SPI_MT65XX_KERNEL510
int tran_ctl_spi_speed(struct tran_fp_data *spi, unsigned int speed, enum spi_transfer_mode mode)
{
#define SPI_MODULE_CLOCK   (100*1000*1000)
#define SPI_SPEED (1*1000*1000)
#define SPI_BITS 8
	struct mt_chip_conf *config;
	unsigned int    time = SPI_MODULE_CLOCK / speed;
	config = &spi->spi_mcc;
	config->low_time = time / 2;
	config->high_time = time / 2;

	if (time % 2) {
		config->high_time = config->high_time + 1;
	}
	if ((mode == DMA_TRANSFER) || (mode == FIFO_TRANSFER)) {
		config->com_mod = mode;
	} else {
		/* default set to FIFO mode */
		config->com_mod = FIFO_TRANSFER;
	}

	if (spi_setup(spi->tran_fp_dev))
		TRAN_FP_ERROR(" failed to setup spi conf\n");
	return 0;
}

int tran_spidev_set_before_rd_chipid(struct TRAN_DEV_SEL_device *spi)
{
	int rv = 0;
	spi->max_speed_hz = SPI_SPEED;
	spi->bits_per_word = SPI_BITS;
	spi->mode = SPI_MODE_0;
	spi->controller_data = &tran_chip_config;
	tran_chip_config.com_mod = FIFO_TRANSFER;
	rv = spi_setup(spi);
	if(rv < 0) {
		TRAN_FP_ERROR("spi master set mode failed");
		return rv;
	}
	return rv;
}

int tran_spidev_set_after_rd_chipid(struct TRAN_DEV_SEL_device *spi)
{
	int rv = 0;
	tran_chip_config.com_mod = DMA_TRANSFER;
	rv = spi_setup(spi);
	if(rv < 0) {
		TRAN_FP_ERROR("spi master set mode failed");
		return rv;
	}
	return rv;
}
#endif
#endif

int tran_get_fp_sensor_chip_id(struct tran_fp_data *tran_fp_datap, unsigned char *chip_id)
{
#define CHIP_ID_LEN 16
	unsigned char vendor_name_str[CHIP_ID_LEN] = {0x00};
	unsigned char chip_name_str[CHIP_ID_LEN] = {0x00};
	char *vendor_name_p = vendor_name_str;
	char *chip_name_p = tran_fp_datap->ic_name;
	struct TEEC_UUID vendor_uuid;
	uint8_t GOODIX_clockSeqAndNode[] = {0x9b, 0x01, 0xc9, 0x92, 0xd5, 0x6d, 0x71, 0xbf};
	//uint8_t SUNWAVE_clockSeqAndNode[] = {0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x4b};
	uint8_t FORTSENSE_clockSeqAndNode[] = {0x93, 0xb7, 0x57, 0x55, 0xc4, 0x8f, 0x45, 0x04};
	uint8_t CHIPONE_clockSeqAndNode[] = {0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0xc0};
	uint8_t SILEAD_clockSeqAndNode[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t CDFINGER_clockSeqAndNode[] = {0x05, 0xd4, 0xc6, 0x7c, 0x7e, 0x8a, 0xfb, 0x1f};
	//uint8_t FPC_clockSeqAndNode[] = {0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x4b};
	uint8_t FOCALTECH_clockSeqAndNode[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	//uint8_t OXI_clockSeqAndNode[] = {0x9b, 0x01, 0xc9, 0x92, 0xd5, 0x6d, 0x71, 0xbf};
	//uint8_t EGIS_clockSeqAndNode[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t JIIOV_clockSeqAndNode[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	int ret = 0;
	int i = 0;
	u32 delayms = 0;
	for (i = 0; i < READ_FP_CHIP_ID_RTY_CNT; i++) {
		ret = tran_read_fp_chip_id(tran_fp_datap);
		if (ret < 0) {
			TRAN_FP_ERROR("%s,read sensor id failed,rty = %d!", __func__,i);
			delayms = 12 * (i+1);
			mdelay(delayms);
			continue;
		} else {
			TRAN_FP_ERROR("%s,read sensor id success,rty = %d!", __func__,i);
			break;
		}
	}
	if(ret < 0)
	{
		TRAN_FP_ERROR("read sensor id failed!");
		return -1;
	}
	else
	{
		TRAN_FP_INFO("tran_fp_datap->ic_name = %s", tran_fp_datap->ic_name);
		vendor_name_p = strsep(&chip_name_p, "_");//这里如果chip_name_p为空，strsep没有找到_ ,chip_name_p会出现空指针。
		if(chip_name_p == NULL)
		{
			TRAN_FP_ERROR("tran_fp_datap->ic_name is null,chip_name_p is null");
			return -1;
		}
		strcpy(vendor_name_str,vendor_name_p);		
		strcpy(chip_name_str,chip_name_p);
		memset(tran_fp_datap->ic_name,0,sizeof(tran_fp_datap->ic_name));
		strcpy(tran_fp_datap->ic_name,chip_name_str);
		chip_name_p = chip_name_str;
		if(strcmp(vendor_name_str,"GOODIX") == 0)
		{
			//USD:modify for fp gf3986 bringup XLLJFL-4 by yan.wen 20240913 start
			if(strcmp(chip_name_p,"GF3976") == 0 || strcmp(chip_name_p,"GF3956") == 0 || strcmp(chip_name_p, "GF3986") == 0)
				//#USD:modify for fp gf3986 bringup XLLJFL-4 by yan.wen 20240913 end
			{
				TRAN_FP_INFO("this is side type capacitance finger chip of Goodix");
				FP_VENDOR_INIT_AFTER_READ_CHIP_ID(GOODIX, "goodix_fp", 10000, 10000, "goodix_tee", 0xfc32e4da, 0x17d3, 0x4244, g);
			}
			else if(strcmp(chip_name_p,"GW9578") == 0 ||strcmp(chip_name_p,"GW9598") == 0)
			{
				TRAN_FP_INFO("this is optical finger chip without mcu or capacitance finger chip of Goodix");
				FP_VENDOR_INIT_AFTER_READ_CHIP_ID(GOODIX, "goodix_fp", 10000, 10000, "goodix_opt_tee", 0xfc32e4da, 0x17d3, 0x4244, g);
			}
			tran_fp_attribute_group.attrs = gf_sysfs_entries;
#ifdef CONFIG_HAS_EARLYSUSPEND
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_1(g);
#else
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_2(g);
#endif
			return 0;
		}
		else if(strcmp(vendor_name_str,"JIIOV") == 0)
		{
			if(strcmp(chip_name_p,"jv0307") == 0)
			{
				TRAN_FP_INFO("this is jv0307 type optical finger chip  capacitance finger chip of ANC");
			}
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID(JIIOV, "jiiov_fp", 10000, 10000, "anc_opt_tee", 0xfc32e4da, 0x17d3, 0x4244, anc);
#ifdef CONFIG_HAS_EARLYSUSPEND
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_1(anc);
#else
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_2(anc);
#endif

			return 0;
		}
		else if(strcmp(vendor_name_str,"CHIPONE") == 0)
		{
			if(strcmp(chip_name_p,"ICNF7312BL") == 0 ||strcmp(chip_name_p,"ICNF7332AL") == 0 || strcmp(chip_name_p,"ICNF7319AL") == 0 ||strcmp(chip_name_p,"ICNF7316EL") == 0)
			{
				TRAN_FP_INFO("this is side type capacitance finger chip of chipone");
			}
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID(CHIPONE, "fpsensor", 1000, 2000, "chipone_tee", 0x7778c03f, 0xc30c, 0x4dd0, c);
#ifdef CONFIG_HAS_EARLYSUSPEND
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_1(c);
#else
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_2(c);
#endif
			return 0;
		}
		else if(strcmp(vendor_name_str,"FORTSENSE") == 0)
		{
			if(strcmp(chip_name_p,"FS9722") == 0)
			{
				TRAN_FP_INFO("this is side type capacitance finger chip of fortsense");
			}
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID(FORTSENSE, "fortsense_fp", 2000, 6000, "fortsense_tee", 0x07721f0c, 0xd217, 0x4f35, s);
			tran_fp_attribute_group.attrs = sf_sysfs_entries;
			INIT_WORK(&tran_fp_datap->work_queue,sf_ctl_device_event);
#ifdef CONFIG_HAS_EARLYSUSPEND
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_1(s);
#else
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_2(s);
#endif
			return 0;
		}
		else if(strcmp(vendor_name_str,"FOCALTECH") == 0)
		{
			if(strcmp(chip_name_p,"FT9371") == 0 || strcmp(chip_name_p,"FT9362") == 0 || strcmp(chip_name_p,"FT9391") == 0 || strcmp(chip_name_p,"FT9396") == 0 || strcmp(chip_name_p,"FT9399") == 0)
			{
				TRAN_FP_INFO("this is side type capacitance finger chip of focaltech");
			}
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID(FOCALTECH, "focaltech_fp", 5000, 3000, "focaltech_tee", 0x04190000, 0x0000, 0x0000, ft);
#ifdef CONFIG_HAS_EARLYSUSPEND
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_1(ft);
#else
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_2(ft);
#endif
			INIT_WORK(&tran_fp_datap->work_queue, ff_ctl_device_event);
			return 0;
		}
		else if(strcmp(vendor_name_str,"SILEAD") == 0)
		{
			if(strcmp(chip_name_p,"GSL6157") == 0 || strcmp(chip_name_p,"GSL6159") == 0 || strcmp(chip_name_p,"GSL6193") == 0)
			{
				FP_VENDOR_INIT_AFTER_READ_CHIP_ID(SILEAD, "silead_fp", 5000, 3000, "silead_tee", 0x511ead0a, 0x0000, 0x0000, si);
				TRAN_FP_INFO("this is capacity finger chip of SILEAD");

			}
			else if(strcmp(chip_name_p,"GSL7002") == 0)
			{
				FP_VENDOR_INIT_AFTER_READ_CHIP_ID(SILEAD, "silead_fp", 5000, 3000, "silead_opt_tee", 0x511ead0a, 0x0000, 0x0000, si);
				TRAN_FP_INFO("this is optic finger chip of SILEAD");
			}
			TRAN_FP_INFO("silead IC shut down to avoid RF interference");
			slf_write_reg(&tran_chip_config,0xE4, 0xB5);
#ifdef CONFIG_PM_SLEEP
			tran_fp_datap->wakelock_hal = wakeup_source_register(NULL, "silfp_wakelock_hal");
#else
			wake_lock_init(&tran_fp_datap->wakelock_hal,WAKE_LOCK_SUSPEND,"silfp_wakelock_hal");
#endif
			atomic_set(&tran_fp_datap->init, 0);
			INIT_WORK(&tran_fp_datap->work_queue, silfp_work_func);
			init_completion(&tran_fp_datap->done);
#ifdef CONFIG_HAS_EARLYSUSPEND
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_1(si);
#else
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_2(si);
#endif
			return 0;
		}
		else if(strcmp(vendor_name_str,"CDFINGER") == 0)
		{
			if (strcmp(chip_name_p,"FPS980SM") == 0)
			{
				TRAN_FP_INFO("this is capacity finger chip of CDFINGER");
			}
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID(CDFINGER, "fpsdev0", 1000, 2000, "cdfinger_tee", 0x06bf3e2d, 0x6be3, 0xa45b, cd);
			init_timer_key(&tran_fp_datap->int_timer, int_timer_handle, 0, "cdfinger_fp_timer", NULL);
			add_timer(&tran_fp_datap->int_timer);
			tran_fp_datap->input_init = cdfinger_customize_input_init;
			tran_fp_datap->finger_thread = kthread_run(cdfinger_thread_func, tran_fp_datap, "finger_thread");
			if (IS_ERR(tran_fp_datap->finger_thread)) {
				TRAN_FP_ERROR("kthread_run is failed\n");
				return -1;
			}
#ifdef CONFIG_HAS_EARLYSUSPEND
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_1(cd);
#else
			FP_VENDOR_INIT_AFTER_READ_CHIP_ID_2(cd);
#endif
			return 0;
		}
		TRAN_FP_INFO("the finger chip is mismatch");
		return -1;
	}
}

int tran_read_fp_chip_id(struct tran_fp_data *tran_fp_datap)
{
#define CHIP_ID_LEN 16
	unsigned char chip_id_str[CHIP_ID_LEN] = {0x00};
	unsigned char *chip_id_p = chip_id_str;
	int i = 0;
	int ret = 0;
	int round_times = 0;
	int non_null_transfer_num = 0;
	struct device_node *node = NULL;
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifndef CONFIG_SPI_MT65XX_KERNEL510
	int status = 0;
	status = tran_spidev_set_before_rd_chipid(tran_fp_datap->tran_fp_dev);
	if (status < 0)
		TRAN_FP_ERROR("tran_spidev_set_before_rd_chipid failed! status=%d\n", status);
#endif
#endif
	node = of_find_compatible_node(NULL, NULL, "tran_fp_legacy");
	if(of_property_read_bool(node,"sunwave"))
		tran_spi_transfer_p_arr[i++] = sf_spi_transfer;
	if(of_property_read_bool(node,"chipone"))
		tran_spi_transfer_p_arr[i++] = cf_spi_transfer;
	if(of_property_read_bool(node,"cdfinger"))
		tran_spi_transfer_p_arr[i++] = cdf_spi_transfer;
	if(of_property_read_bool(node,"silead"))
		tran_spi_transfer_p_arr[i++] = sif_spi_transfer;
	if(of_property_read_bool(node,"focaltech"))
		tran_spi_transfer_p_arr[i++] = ftf_spi_transfer;
	if(of_property_read_bool(node,"fpc"))
		tran_spi_transfer_p_arr[i++] = ff_spi_transfer;
	if(of_property_read_bool(node,"goodix"))
		tran_spi_transfer_p_arr[i++] = gf_spi_transfer;
	if(of_property_read_bool(node,"oxi"))
		tran_spi_transfer_p_arr[i++] = of_spi_transfer;
	if(of_property_read_bool(node,"egis"))
		tran_spi_transfer_p_arr[i++] = egf_spi_transfer;
	if(of_property_read_bool(node,"anc"))
		tran_spi_transfer_p_arr[i++] = ancf_spi_transfer;

	for (i = 0; i < ARRAY_SIZE(tran_spi_transfer_p_arr); i++) {
		if (tran_spi_transfer_p_arr[i])
			non_null_transfer_num++;
	}
	for (i = 0; i < ARRAY_SIZE(tran_spi_transfer_p_arr) && tran_spi_transfer_p_arr[i]; i++) {

		(*(tran_spi_transfer_p_arr[i]))(chip_id_p);
		memset(tran_fp_datap->ic_name,0,sizeof(tran_fp_datap->ic_name));
		//USD:modify for fp gf3986 bringup XLLJFL-4 by yan.wen 20240913 start
		if ((chip_id_p[0] == 0x12 && chip_id_p[1] == 0x12) || (chip_id_p[0] == 0x12 && chip_id_p[1] == 0x61) || (chip_id_p[0] == 0x12 && chip_id_p[1] == 0x31) || (chip_id_p[2] == 0x00 && chip_id_p[3] == 0x26) || (chip_id_p[0] == 0x06 && chip_id_p[3] == 0x25) || (chip_id_p[0] == 0x05 && chip_id_p[1] == 0x01 && chip_id_p[2] == 0x02 && chip_id_p[3] == 0x01) || (chip_id_p[0] == 0x05 && chip_id_p[1] == 0x02 && chip_id_p[2] == 0x02 && chip_id_p[3] == 0x01) || (chip_id_p[0] == 0x05 && chip_id_p[1] == 0x03 && chip_id_p[2] == 0x02 && chip_id_p[3] == 0x01) || (chip_id_p[0] == 0x06 && chip_id_p[1] == 0x01 && chip_id_p[2] == 0x02 && chip_id_p[3] == 0x01))
			//USD:modify for fp gf3986 bringup XLLJFL-4 by yan.wen 20240913 end
		{
			if (chip_id_p[0] == 0x12 && chip_id_p[1] == 0x12) {
				TRAN_FP_INFO("this is G6 type optical finger chip without mcu or capacitance finger chip of Goodix");
			}
			else if (chip_id_p[0] == 0x12 && chip_id_p[1] == 0x31) {
				TRAN_FP_INFO("this is GW9578 type optical finger chip without mcu or capacitance finger chip of Goodix");
				strcpy(tran_fp_datap->ic_name, "GOODIX_GW9578" );
			}
			else if (chip_id_p[0] == 0x12 && chip_id_p[1] == 0x61) {
				TRAN_FP_INFO("this is GW9598 type optical finger chip without mcu or capacitance finger chip of Goodix");
				strcpy(tran_fp_datap->ic_name, "GOODIX_GW9598" );
			}
			else if (chip_id_p[2] == 0x00 && chip_id_p[3] == 0x26) {
				TRAN_FP_INFO("this is G6X type optical finger chip without mcu or capacitance finger chip of Goodix");
			}
			else if (chip_id_p[0] == 0x06 && chip_id_p[3] == 0x25) {
				TRAN_FP_INFO("this is GF3636 side type capacitance finger chip of Goodix");
			}
			else if (chip_id_p[0] == 0x05 && chip_id_p[1] == 0x02 && chip_id_p[2] == 0x02 && chip_id_p[3] == 0x01) {
				TRAN_FP_INFO("this is GF3956 side type capacitance finger chip of Goodix");
				strcpy(tran_fp_datap->ic_name, "GOODIX_GF3956" );
			}
			else if (chip_id_p[0] == 0x05 && chip_id_p[1] == 0x01 && chip_id_p[2] == 0x02 && chip_id_p[3] == 0x01) {
				TRAN_FP_INFO("this is GF3988 back type capacitance finger chip of Goodix");
			}
			else if (chip_id_p[0] == 0x05 && chip_id_p[1] == 0x03 && chip_id_p[2] == 0x02 && chip_id_p[3] == 0x01) {
				TRAN_FP_INFO("this is GF3976 side type capacitance finger chip of Goodix");
				strcpy(tran_fp_datap->ic_name, "GOODIX_GF3976" );
			}
			//USD:add for fp gf3986 bringup XLLJFL-4 by yan.wen 20240913 start
			else if (chip_id_p[0] == 0x06 && chip_id_p[1] == 0x01 && chip_id_p[2] == 0x02 && chip_id_p[3] == 0x01) {
				TRAN_FP_INFO("this is GF3986 side type capacitance finger chip of Goodix");
				strcpy(tran_fp_datap->ic_name, "GOODIX_GF3986" );
			}
			//USD:add for fp gf3986 bringup XLLJFL-4 by yan.wen 20240913 end
			break;
		} else if(chip_id_p[0] == 0x46 && chip_id_p[1] == 0x54 && chip_id_p[2] == 0x43 && chip_id_p[3] == 0x01) 
		{
			TRAN_FP_INFO("this is jv0307 type optical finger chip  capacitance finger chip of ANC");
			strcpy(tran_fp_datap->ic_name, "JIIOV_JV0307" );
		} else if ((((chip_id_p[6] << 8) | chip_id_p[7]) == 0x95a8) || (((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9362) || (((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9371) || (((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9372) || (((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9391) || (((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9395) || (((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9396) || (((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9399))
		{
			if (((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9362) {
				TRAN_FP_INFO("ic name is FT9362");
				strcpy(tran_fp_datap->ic_name, "FOCALTECH_FT9362" );
			} else if ((((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9371) || (((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9372)){
				TRAN_FP_INFO("ic name is FT9371");
				strcpy(tran_fp_datap->ic_name, "FOCALTECH_FT9371" );
			} else if ((((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9391) || (((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9395)) {
				TRAN_FP_INFO("ic name is FT9391");
				strcpy(tran_fp_datap->ic_name, "FOCALTECH_FT9391" );
			} else if (((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9396) {
				TRAN_FP_INFO("ic name is FT9396");
				strcpy(tran_fp_datap->ic_name, "FOCALTECH_FT9396" );
			} else if (((chip_id_p[6] << 8) | chip_id_p[7]) == 0x9399) {
				TRAN_FP_INFO("ic name is FT9399");
				strcpy(tran_fp_datap->ic_name, "FOCALTECH_FT9399" );
			}
			break;
		} else if (chip_id_p[1] == 0x71 || chip_id_p[1] == 0x72 || chip_id_p[1] == 0x73)
		{
			if(chip_id_p[1] == 0x73 && chip_id_p[2] == 0x12){
				TRAN_FP_INFO("ic name is ICNF7312BL");
				strcpy(tran_fp_datap->ic_name, "CHIPONE_ICNF7312BL" );
			}
			else if(chip_id_p[1] == 0x73 && chip_id_p[2] == 0x32){
				TRAN_FP_INFO("ic name is ICNF7332AL");
				strcpy(tran_fp_datap->ic_name, "CHIPONE_ICNF7332AL" );
			}
			else if(chip_id_p[1] == 0x73 && chip_id_p[2] == 0x19){
				TRAN_FP_INFO("ic name is ICNF7319AL");
				strcpy(tran_fp_datap->ic_name, "CHIPONE_ICNF7319AL" );
			}
			else if(chip_id_p[1] == 0x73 && chip_id_p[2] == 0x16){
				TRAN_FP_INFO("ic name is ICNF7316EL");
				strcpy(tran_fp_datap->ic_name, "CHIPONE_ICNF7316EL" );
			}
			break;
		} else if (chip_id_p[0] == 0x18 || chip_id_p[0] == 0x19 || chip_id_p[0] == 0x1c)
		{
			TRAN_FP_INFO("ic name is FPC");
			break;
		} else if (((chip_id_p[7] == 0x95) || (chip_id_p[7] == 0x93) || (chip_id_p[7] == 0x97) || (chip_id_p[2] == 0x53 && chip_id_p[3] == 0x75 && chip_id_p[4] == 0x6e && chip_id_p[5] == 0x57) || (chip_id_p[4] == 0x82 && chip_id_p[5] == 0x33) || (chip_id_p[7] == 0x53 && chip_id_p[8] == 0x75 && chip_id_p[9] == 0x6e && chip_id_p[10] == 0x57) || (chip_id_p[7] == 0x46 && chip_id_p[8] == 0x74 && chip_id_p[9] == 0x53 && chip_id_p[10] == 0x73 && chip_id_p[11] == 0x8e)))
		{
			if ((chip_id_p[7] == 0x95) || (chip_id_p[7] == 0x97) || (chip_id_p[7] == 0x46 && chip_id_p[8] == 0x74 && chip_id_p[9] == 0x53 && chip_id_p[10] == 0x73 && chip_id_p[11] == 0x8e)) {
				TRAN_FP_INFO("ic name is FS9722");
				strcpy(tran_fp_datap->ic_name, "FORTSENSE_FS9722" );
			}
			else if ((chip_id_p[7] == 0x93) || (chip_id_p[2] == 0x53 && chip_id_p[3] == 0x75 && chip_id_p[4] == 0x6e && chip_id_p[5] == 0x57) || (chip_id_p[4] == 0x82 && chip_id_p[5] == 0x33) || (chip_id_p[7] == 0x53 && chip_id_p[8] == 0x75 && chip_id_p[9] == 0x6e && chip_id_p[10] == 0x57)) {
				TRAN_FP_INFO("this is normal capacity finger chip of Sunwave");
			}
			break;
		} else if (((chip_id_p[3] << 24) | (chip_id_p[2] << 16) | (chip_id_p[1] << 8) | chip_id_p[0]) == 0x5e)
		{
			TRAN_FP_INFO("ic name is OXI");
			break;
		} else if (chip_id_p[3] == 0x61 || chip_id_p[3] == 0x72 || chip_id_p[3] == 0x70)
		{
			if (chip_id_p[3] == 0x61 && chip_id_p[2] == 0x57){
				TRAN_FP_INFO("this is capacity FP of GSL6157");
				strcpy(tran_fp_datap->ic_name, "SILEAD_GSL6157" );
			}
			else if (chip_id_p[3] == 0x61 && chip_id_p[2] == 0x59){
				TRAN_FP_INFO("this is capacity FP of GSL6159");
				strcpy(tran_fp_datap->ic_name, "SILEAD_GSL6159" );
			}
			else if (chip_id_p[3] == 0x61 && chip_id_p[2] == 0x93){
				TRAN_FP_INFO("this is capacity FP of GSL6193");
				strcpy(tran_fp_datap->ic_name, "SILEAD_GSL6193" );
			}
			else if (chip_id_p[3] == 0x70 && chip_id_p[2] == 0x15){
				TRAN_FP_INFO("this is optical FP of 7015(7002)");
				strcpy(tran_fp_datap->ic_name, "SILEAD_GSL7002" );
			}
			break;
		} else if (chip_id_p[3] == 0x80 || chip_id_p[3] == 0x98 || chip_id_p[3] == 0x56 || chip_id_p[3] == 0x88  || chip_id_p[3] == 0x86 || chip_id_p[3] == 0x64 || chip_id_p[3] == 0x35 || chip_id_p[3] == 0x16)
		{
			if(chip_id_p[3] == 0x88  || chip_id_p[3] == 0x86 || chip_id_p[3] == 0x64){
				TRAN_FP_INFO("this is finger chip is FPS980SM");
			}
			break;
		} else if (chip_id_p[0] == 0xAA || chip_id_p[1] == 0xAA)
		{
			TRAN_FP_INFO("ic name is EGIS");
			break;
		} else
		{
			round_times++;
			TRAN_FP_INFO("round_times = %d, ready for the next round!", round_times);
		}
	}
	if (round_times >= non_null_transfer_num) {
		TRAN_FP_ERROR("the finger chip type has error!");
		ret = -1;
	}

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifndef CONFIG_SPI_MT65XX_KERNEL510
	status = tran_spidev_set_after_rd_chipid(tran_fp_datap->tran_fp_dev);
	if (status < 0)
		TRAN_FP_ERROR("tran_spidev_set_after_rd_chipid failed! status=%d\n", status);
#endif
#endif
	return ret;
}

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
int tran_fp_spi_transfer(void *chip_conf, int cfg_len, const char *txbuf, char *rxbuf, int len, const char *txbuf1, char *rxbuf1, int len1)
{
	struct spi_transfer t,t1;
	struct spi_message m;
	int i = 0;
	int ret = 0;
	memset(&t, 0, sizeof(t));
	memset(&t1, 0, sizeof(t1));

	g_tran_fp_datap->tran_fp_dev->controller_data = chip_conf;
	spi_message_init(&m);

	t.tx_buf = txbuf;
	t.rx_buf = rxbuf;
	t.bits_per_word = 8;
	t.delay.unit = SPI_DELAY_UNIT_USECS;
	t.delay.value = 5;
#ifdef CONFIG_SPI_MT65XX_KERNEL510
	t.speed_hz = tran_spi_speed;
	//TRAN_FP_INFO(" now spi-clock:%d\n", t.speed_hz);
#endif
	t.len = len;
	spi_message_add_tail(&t, &m);

	if (txbuf1 || rxbuf1) {
		t1.tx_buf = txbuf1;
		t1.rx_buf = rxbuf1;
		t1.bits_per_word = 8;
		t.delay.unit = SPI_DELAY_UNIT_USECS;
		t.delay.value = 5;
#ifdef CONFIG_SPI_MT65XX_KERNEL510
		t1.speed_hz = tran_spi_speed;
		//TRAN_FP_INFO(" now spi-clock:%d\n", t.speed_hz);
#endif
		t1.len = len1;
		spi_message_add_tail(&t1, &m);
	}
	for (i = 0; i < SPI_SYNC_RTY_CNT; i++) {
		ret = spi_sync(g_tran_fp_datap->tran_fp_dev, &m);
		if (0 == ret) {
			TRAN_FP_ERROR("%s, spi_sync pass, retry=%d,result=%d\n", __func__, i, ret);
			break;
		} else {
			TRAN_FP_ERROR("%s, spi_sync fail, retry=%d,result=%d\n", __func__, i, ret);
			continue;
		}
	}
	return ret;
}
#endif

int tran_fp_input_init(void)
{
	int err = 0;
	TRAN_FP_INFO(" enter.\n");
	g_tran_fp_datap->input = input_allocate_device();

	if (!g_tran_fp_datap->input) {
		TRAN_FP_ERROR("input_allocate_device(..) failed.\n");
		return (-ENOMEM);
	}

	g_tran_fp_datap->input->name = "tran-keys";
	__set_bit(EV_KEY,   g_tran_fp_datap->input->evbit);
	__set_bit(KEY_HOME,   g_tran_fp_datap->input->keybit);
	//__set_bit(KEY_HOMEPAGE,   g_tran_fp_datap->input->keybit);   //maybe has some conflict with some game app
	__set_bit(KEY_MENU,   g_tran_fp_datap->input->keybit);
	__set_bit(KEY_BACK,   g_tran_fp_datap->input->keybit);
	__set_bit(KEY_POWER,  g_tran_fp_datap->input->keybit);
	//BSP: modify [avoid key conflicts and fit with android R] XLLLHLWS-1751 by fan.peng 20191109 start
	__set_bit(KEY_F28,    g_tran_fp_datap->input->keybit);
	//BSP: modify [avoid key conflicts and fit with android R] XLLLHLWS-1751 by fan.peng 20191109 end
	__set_bit(KEY_ENTER,  g_tran_fp_datap->input->keybit);
	__set_bit(KEY_UP,     g_tran_fp_datap->input->keybit);
	__set_bit(KEY_LEFT,   g_tran_fp_datap->input->keybit);
	__set_bit(KEY_RIGHT,  g_tran_fp_datap->input->keybit);
	__set_bit(KEY_DOWN,   g_tran_fp_datap->input->keybit);
	__set_bit(KEY_WAKEUP, g_tran_fp_datap->input->keybit);
	__set_bit(KEY_CAMERA, g_tran_fp_datap->input->keybit);

	if (g_tran_fp_datap->input_init)
		g_tran_fp_datap->input_init(g_tran_fp_datap);
	//BSP: modify [avoid key conflicts with some game apps] XLLLHLWS-1751 by fan.peng 20191109 start
	//__set_bit(KEY_VOLUMEDOWN, g_tran_fp_datap->input->keybit);
	//__set_bit(KEY_VOLUMEUP, g_tran_fp_datap->input->keybit);
	//__set_bit(KEY_SEARCH, g_tran_fp_datap->input->keybit);
	//__set_bit(KEY_CHAT, g_tran_fp_datap->input->keybit);
	//BSP: modify [avoid key conflicts with some game apps] XLLLHLWS-1751 by fan.peng 20191109 end

	err = input_register_device(g_tran_fp_datap->input);

	if (err) {
		TRAN_FP_ERROR("input_register_device(..) = %d.\n", err);
		input_free_device(g_tran_fp_datap->input);
		g_tran_fp_datap->input = NULL;
		return (-ENODEV);
	}

	TRAN_FP_INFO(" leave.\n");
	return err;
}

int tran_fp_irq_init(void)
{
	int rc = 0;
	if(g_tran_fp_datap->tran_pinctrl)
	{
		rc = pinctrl_select_state(g_tran_fp_datap->tran_pinctrl, g_tran_fp_datap->pins_irq);
		if(rc)
			TRAN_FP_ERROR("[tran]cannot set irq pinctrl state, rc = %d \n", rc);
	}
	return g_tran_fp_datap->irq_init();
}

int tran_netlink_init(struct tran_fp_data *tran_fp_datap)
{
	struct netlink_kernel_cfg cfg;
	struct netlink_kernel_cfg cfg_bio;

	memset(&cfg, 0, sizeof(struct netlink_kernel_cfg));
	memset(&cfg_bio, 0, sizeof(struct netlink_kernel_cfg));
	cfg.input = tran_netlink_recv;
	cfg_bio.input = tran_netlink_recv;

	tran_fp_datap->nl_sk = netlink_kernel_create(&init_net, TRAN_NETLINK_ROUTE, &cfg);
	if (tran_fp_datap->nl_sk == NULL) {
		TRAN_FP_ERROR(" netlink create failed\n");
		return -1;
	}
	tran_fp_datap->nl_sk_bio = netlink_kernel_create(&init_net, TRAN_BIO_NETLINK_ROUTE, &cfg_bio);
	if (tran_fp_datap->nl_sk_bio == NULL) {
		TRAN_FP_ERROR(" netlink_bio create failed\n");
		return -1;
	}

	TRAN_FP_INFO(" netlink create success\n");
	return 0;
}

int tran_netlink_destroy(struct tran_fp_data *tran_fp_datap)
{
	if (tran_fp_datap->nl_sk != NULL) {
		netlink_kernel_release(tran_fp_datap->nl_sk);
		tran_fp_datap->nl_sk = NULL;
		return 0;
	}
	TRAN_FP_ERROR(" no netlink socket yet\n");
	return -1;
}

void tran_netlink_send(struct tran_fp_data *tran_fp_datap, const int cmd)
{
	struct nlmsghdr *nlh = NULL;
	struct sk_buff *skb = NULL;
	int ret;

#ifdef FINGERPRINT_INTERRUPT_LOG
	TRAN_FP_INFO("[%s] send cmd %d\n", __func__, cmd);
#endif
	if (!tran_fp_datap || !tran_fp_datap->nl_sk) {
		TRAN_FP_ERROR("[%s] invalid socket\n", __func__);
		return;
	}

	if (! pid) {
		TRAN_FP_ERROR("[%s] invalid PID\n", __func__);
		return;
	}

	/*alloc data buffer for sending to native*/
	/*malloc data space at least 1500 bytes, which is ethernet data length*/
	skb = alloc_skb(TRAN_NL_MSG_LEN, GFP_ATOMIC);
	if (skb == NULL) {
		return;
	}

	nlh = nlmsg_put(skb, 0, 0, 0, TRAN_NL_MSG_LEN, 0);
	if (!nlh) {
		TRAN_FP_ERROR("[%s] nlmsg_put() failed\n", __func__);
		kfree_skb(skb);
		return;
	}

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = 0;

	*(char *)NLMSG_DATA(nlh) = cmd;
	if(cmd == TRAN_NETLINK_FP_ID_FAILED || cmd == TRAN_NETLINK_FP_ID_PASS)
	{
		if(tran_fp_datap->nl_sk_bio != NULL)
		{
			ret = netlink_unicast(tran_fp_datap->nl_sk_bio, skb, pid, MSG_DONTWAIT);
			if (ret == 0) {
				TRAN_FP_ERROR("[%s] send failed\n", __func__);
				kfree_skb(skb);
				return;
			}
		}
		else
			TRAN_FP_ERROR("tran_fp_datap->nl_sk_bio is NULL!\n");
	}
	else{
		ret = netlink_unicast(tran_fp_datap->nl_sk, skb, pid, MSG_DONTWAIT);
		if (ret == 0) {
			TRAN_FP_ERROR("[%s] send failed\n", __func__);
			kfree_skb(skb);
			return;
		}
	}
#ifdef FINGERPRINT_INTERRUPT_LOG
	TRAN_FP_DEBUG("[%s] sent, len=%d\n", __func__, ret);
#endif
}

void tran_netlink_recv(struct sk_buff *__skb)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	char str[128];

	skb = skb_get(__skb);
	if (!skb ) {
		TRAN_FP_ERROR("[%s] skb NULL\n", __func__);
		return;
	}

	/* presume there is 5byte payload at leaset */
	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);
		memcpy(str, NLMSG_DATA(nlh), sizeof(str));
		/*
		   if (g_fp_name_flg == SILEAD) {
		   if ((strlen(str) == strlen("FP")) && !strncmp(str,"FP", strlen(str)))
		   pid = nlh->nlmsg_pid;
		   } else if (g_fp_name_flg == GOODIX) {
		   pid = nlh->nlmsg_pid;
		   }
		   TRAN_FP_DEBUG("[%s]nlh->nlmsg_pid = %d,  pid = %d, msg = %s\n", __func__, nlh->nlmsg_pid, pid, str);
		   } else {
		   */
		//  TRAN_FP_ERROR("[%s] data len incorrect\n", __func__);
		pid = nlh->nlmsg_pid;
		TRAN_FP_DEBUG("[%s]nlh->nlmsg_pid = %d,  pid = %d, msg = %s\n", __func__, nlh->nlmsg_pid, pid, str);
	}
	kfree_skb(skb);
}

int tran_fp_report_key_nav_event(struct input_dev *input, tran_key_nav_t *kevent)
{
	int err = 0;
	unsigned int key_code = KEY_UNKNOWN;
	TRAN_FP_DEBUG("(..) enter.\n");

	switch (kevent->key) {
		case TRAN_KEY_NAV_NONE:
			key_code = KEY_UNKNOWN;
			TRAN_FP_INFO("key none");
			break;

		case TRAN_KEY_NAV_HOME:
			key_code = KEY_HOME;
			TRAN_FP_INFO("key home");
			break;

		case TRAN_KEY_NAV_POWER:
			key_code = KEY_POWER;
			TRAN_FP_INFO("key power");
			break;

		case TRAN_KEY_NAV_MENU:
			key_code = KEY_MENU;
			TRAN_FP_INFO("key menu");
			break;

		case TRAN_KEY_NAV_BACK:
			key_code = KEY_BACK;
			TRAN_FP_INFO("key back");
			break;

		case TRAN_KEY_NAV_CAMERA:
			key_code = KEY_CAMERA;
			TRAN_FP_INFO("key camera");
			break;

		case TRAN_KEY_NAV_FINGER_UP:
			TRAN_FP_INFO("nav finger up");
			break;

		case TRAN_KEY_NAV_FINGER_DOWN:
			TRAN_FP_INFO("nav finger down");
			break;

		case TRAN_KEY_NAV_UP:
			key_code = KEY_UP;
			TRAN_FP_INFO("nav up");
			break;

		case TRAN_KEY_NAV_DOWN:
			key_code = KEY_DOWN;
			TRAN_FP_INFO("nav down");
			break;

		case TRAN_KEY_NAV_LEFT:
			key_code = KEY_LEFT;
			TRAN_FP_INFO("nav left");
			break;

		case TRAN_KEY_NAV_RIGHT:
			key_code = KEY_RIGHT;
			TRAN_FP_INFO("nav right");
			break;

		case TRAN_KEY_NAV_CLICK:
			key_code = KEY_VOLUMEDOWN;
			TRAN_FP_INFO("nav volume down");
			break;

		case TRAN_KEY_NAV_HEAVY:
			key_code = KEY_CHAT;
			TRAN_FP_INFO("nav chat");
			break;

		case TRAN_KEY_NAV_LONG_PRESS:
			key_code = KEY_SEARCH;
			TRAN_FP_INFO("nav search");
			break;

		case TRAN_KEY_NAV_DOUBLE_CLICK:
			key_code = KEY_VOLUMEUP;
			TRAN_FP_INFO("nav volume up");
			break;

		case TRAN_KEY_NAV_F28:
			key_code = KEY_F28;
			TRAN_FP_INFO("nav F28");
			break;

		case TRAN_KEY_NAV_ENTER:
			key_code = KEY_ENTER;
			TRAN_FP_INFO("nav enter");
			break;

		case TRAN_KEY_NAV_WAKEUP:
			key_code = KEY_WAKEUP;
			TRAN_FP_INFO("nav wakeup");
			break;

		case TRAN_KEY_NAV_PAGEUP:
			key_code = KEY_PAGEUP;
			TRAN_FP_INFO("nav pageup");
			break;

		case TRAN_KEY_NAV_PAGEDOWN:
			key_code = KEY_PAGEDOWN;
			TRAN_FP_INFO("nav pagedown");
			break;
		default:
			TRAN_FP_INFO("key_nav none or bad parameter, kevent->key = %d ", kevent->key);
			break;
	}

	TRAN_FP_INFO(" key_code[%d], key=%d, value=%d\n", key_code, kevent->key, kevent->value);
	if ((kevent->key != TRAN_KEY_NAV_FINGER_DOWN) && (kevent->key != TRAN_KEY_NAV_FINGER_UP)) {
		input_report_key(input, key_code, kevent->value);
		input_sync(input);
	}
	TRAN_FP_DEBUG("(..) leave.\n");
	return err;
}

int tran_proc_init(struct tran_fp_data *tran_fp_dev)
{
	int ret = 0;
#ifdef CONFIG_TRAN_PROC_FS
	ret = tran_fp_dev->proc_init(tran_fp_dev);
#endif
	return ret;
}

int tran_proc_deinit(struct tran_fp_data *tran_fp_dev)
{
	int ret = 0;
#ifdef CONFIG_TRAN_PROC_FS
	ret = tran_fp_dev->proc_deinit(tran_fp_dev);
#endif
	return ret;
}
//BSP: delete g90 bringup XLLLHLWS-309 by fan.peng 20200118 start
void tran_register_board_info(void)
{
#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
	/*
	   spi_register_board_info(tran_spi_board_devs,ARRAY_SIZE(tran_spi_board_devs));
	   */
#else
	platform_device_register_full(tran_plat_board_devs);
#endif
	return;
}
//BSP: delete g90 bringup XLLLHLWS-309 by fan.peng 20200118 end

int tran_register_base_ops(struct tran_fp_data *tran_fp_datap)
{
	tran_fp_datap->resource_init  = tran_fp_resource_init;
	tran_fp_datap->power_init = tran_fp_power_init;
	tran_fp_datap->power_onoff = tran_fp_power_onoff;
	tran_fp_datap->spi_clk_on = tran_fp_open_clock;
	tran_fp_datap->spi_clk_off = tran_fp_close_clock;
	tran_fp_datap->reset      = tran_fp_reset_sensor;
	tran_fp_datap->shutdown      = tran_fp_shutdown_sensor;
	tran_fp_datap->enable_irq = tran_fp_enable_irq;
	tran_fp_datap->disable_irq = tran_fp_disable_irq;
	tran_fp_datap->tran_fp_set_clock = tran_fp_set_clock;
	return 0;
}
