/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 Transsion Inc.
 */
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <uapi/linux/uleds.h>
#include <linux/of.h>
#include "tran_led_class.h"
#include "tran_led_core.h"

static struct class * tran_led_class = NULL;

/********** API FOR APP & HAL  **********/
int tran_led_dev_set_i2c_retries(struct tran_led_device *dev, unsigned long arg)
{
	if (dev != NULL && dev->ops != NULL &&
	    dev->ops->set_i2c_retries)
		return dev->ops->set_i2c_retries(dev, arg);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(tran_led_dev_set_i2c_retries);

int tran_led_dev_set_i2c_timeout(struct tran_led_device *dev, unsigned long arg)
{
	if (dev != NULL && dev->ops != NULL &&
	    dev->ops->set_i2c_timeout)
		return dev->ops->set_i2c_timeout(dev, arg);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(tran_led_dev_set_i2c_timeout);

int tran_led_dev_rw_i2c_data(struct tran_led_device *dev, unsigned long arg)
{
	if (dev != NULL && dev->ops != NULL &&
	    dev->ops->rw_i2c_data)
		return dev->ops->rw_i2c_data(dev, arg);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(tran_led_dev_rw_i2c_data);

int tran_led_dev_get_chip_type(struct tran_led_device *dev, enum ChipType * p)
{
	if (dev != NULL && dev->ops != NULL &&
	    dev->ops->get_chip_type)
		return dev->ops->get_chip_type(dev, p);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(tran_led_dev_get_chip_type);

int tran_led_dev_get_cmd(struct tran_led_device *dev, int32_t * cmd_data,int size)
{
	if (dev != NULL && dev->ops != NULL &&
	    dev->ops->get_tran_led_cmd)
		return dev->ops->get_tran_led_cmd(dev, cmd_data, size);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(tran_led_dev_get_cmd);

int tran_led_dev_set_cmd(struct tran_led_device *dev, int32_t * cmd_data,int size)
{
	if (dev != NULL && dev->ops != NULL && dev->ops->set_tran_led_cmd)
    {   
		return dev->ops->set_tran_led_cmd(dev, cmd_data, size);
    } else {
        tlc_err("tran_led_dev_set_cmd failed vendor dev = null!");
    }

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(tran_led_dev_set_cmd);

/********** API FOR MIDTEST MODE OR FACTORY MODE **********/
int tran_led_dev_store_tran_led_cmd(struct tran_led_device *dev,const char *buf)
{
	if (dev != NULL && dev->ops != NULL &&
	    dev->ops->store_tran_led_cmd)
		return dev->ops->store_tran_led_cmd(dev, buf);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(tran_led_dev_store_tran_led_cmd);

int tran_led_dev_show_tran_led_cmd(struct tran_led_device *dev, char *buf)
{
	if (dev != NULL && dev->ops != NULL &&
	    dev->ops->show_tran_led_cmd)
		return dev->ops->show_tran_led_cmd(dev, buf);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(tran_led_dev_show_tran_led_cmd);

int tran_led_dev_store_tran_led_check(struct tran_led_device *dev,const char *buf)
{
	if (dev != NULL && dev->ops != NULL &&
	    dev->ops->store_tran_led_check)
		return dev->ops->store_tran_led_check(dev, buf);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(tran_led_dev_store_tran_led_check);

int tran_led_dev_show_tran_led_check(struct tran_led_device *dev, char *buf)
{
	if (dev != NULL && dev->ops != NULL &&
	    dev->ops->show_tran_led_check)
		return dev->ops->show_tran_led_check(dev, buf);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(tran_led_dev_show_tran_led_check);

struct tran_led_device *tran_led_device_register(const char *name,
		struct device *parent, void *devdata,
		const struct tran_led_ops *ops,
		const struct tran_led_properties *props)
{
	struct tran_led_device *led_dev;
	int rc;

	led_dev = kzalloc(sizeof(*led_dev), GFP_KERNEL);
	if (!led_dev)
		return ERR_PTR(-ENOMEM);

	led_dev->dev.class = tran_led_class;
	led_dev->dev.parent = parent;
	dev_set_name(&led_dev->dev, "%s", name);
	dev_set_drvdata(&led_dev->dev, devdata);

	/* Copy properties */
	if (props) {
		memcpy(&led_dev->props, props,
		       sizeof(struct tran_led_properties));
	}
	rc = device_register(&led_dev->dev);
	if (rc) {
		kfree(led_dev);
		led_dev = NULL;
		return ERR_PTR(rc);
	}
	led_dev->ops = ops;

	return led_dev;
}
EXPORT_SYMBOL(tran_led_device_register);

void tran_led_device_unregister(struct tran_led_device *led_dev)
{
	if (!led_dev)
		return;

	device_unregister(&led_dev->dev);
	kfree(led_dev);
	led_dev = NULL;
}
EXPORT_SYMBOL(tran_led_device_unregister);

static int tranled_match_device_by_name(struct device *dev,
	const void *data)
{
	const char *name = data;

	return strcmp(dev_name(dev), name) == 0;
}

struct tran_led_device *get_leddev_by_name(const char *name)
{
	struct device *dev;

	if (!name || IS_ERR_OR_NULL(tran_led_class))
		return (struct tran_led_device *)NULL;

	dev = class_find_device(tran_led_class, NULL, name,
				tranled_match_device_by_name);

	return dev ? to_led_device(dev) : NULL;

}
EXPORT_SYMBOL(get_leddev_by_name);

int tran_led_class_init(struct platform_device *pdev)
{
	struct tran_led_core *tlc = NULL;
	tlc = dev_get_drvdata(&pdev->dev);
    tlc->tran_led_class = class_create("tran_led");
    if(IS_ERR(tlc->tran_led_class)) {
          return PTR_ERR(tlc->tran_led_class);
    }

	tran_led_class = tlc->tran_led_class;

	return 0;
}
EXPORT_SYMBOL(tran_led_class_init);

int tran_led_class_destroy(struct platform_device *pdev)
{
	struct tran_led_core *tlc = NULL;
	tlc = dev_get_drvdata(&pdev->dev);

	if (IS_ERR_OR_NULL(tlc->tran_led_class))
		return -1;

	class_destroy(tlc->tran_led_class);
	tran_led_class = NULL;
	return 0;
}
EXPORT_SYMBOL(tran_led_class_destroy);

//MODULE_LICENSE("GPL");
//MODULE_DESCRIPTION("Transsion LED Class Interface");
