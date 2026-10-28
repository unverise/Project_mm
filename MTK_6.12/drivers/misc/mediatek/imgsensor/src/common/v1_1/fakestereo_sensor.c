/**
* Copyright@ 2018 Transsion Inc
* Author quan.chang@transsion.com
* Description:common fake sensor mgr
* ITD modify by yingji.wang 20250807
*/
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>


static struct class *fakesensor_class = NULL;
static struct device *fakesensor_dev = NULL;
static DEFINE_MUTEX(main2_light_mutex);
static struct regulator *fakesensor_regulator = NULL;
static unsigned int fakesensor_output;
static const char *sensor_matecodes;
static int fakesensor_index = 0;

static int fakesensor_init(void)
{
	struct device_node *np;
	struct platform_device *pdev;
	const char *index;;
	int ret;

	np = of_find_compatible_node(NULL, NULL, "transsion,fakesensor");
	if (!np) {
		pr_err("Failed to find compatible node\n");
		return -ENODEV;
	}

	ret = of_property_read_string(np, "sensor_matecodes", &sensor_matecodes);
	if (ret) {
		pr_err("Failed to get sensor_matecodes\n");
		of_node_put(np);
		return ret;
	}
	pr_info("sensor_matecodes: %s\n", sensor_matecodes);

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_err("Failed to find platform device\n");
		of_node_put(np);
		return -ENODEV;
	}

	ret = of_property_read_string(np, "sensor_index", &index);
	if (ret) {
		pr_err("Failed to get sensor_index\n");
		of_node_put(np);
		return ret;
	}
	ret = kstrtoint(index, 10, &fakesensor_index);
	if (ret) {
		pr_err("Failed to get sensor_index 2\n");
		of_node_put(np);
		return ret;
	}
	pr_info("sensor_index: %d\n", fakesensor_index);

	fakesensor_regulator = devm_regulator_get(&pdev->dev, "fakevin");
	if (IS_ERR(fakesensor_regulator)) {
		pr_err("Failed to get vin regulator\n");
		of_node_put(np);
		return PTR_ERR(fakesensor_regulator);
	}

	fakesensor_output = of_get_named_gpio(np, "fakesensor_output", 0);
	if (fakesensor_output < 0) {
		pr_err("Failed to get named GPIO: %d\n", fakesensor_output);
		of_node_put(np);
		return fakesensor_output;
	}

	ret = gpio_request(fakesensor_output, "fakesensor_output");
	if(ret){
		pr_err( "Failed to request fakesensor_output GPIO\n");
		of_node_put(np);
		return ret;
	}
	ret = gpio_direction_input(fakesensor_output);
	pr_info("fakesensor_output: %d\n", fakesensor_output);

	of_node_put(np);
	return 0;
}

static ssize_t main2_light_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int y_average = -1;
	int pin_value = 0;
	mutex_lock(&main2_light_mutex);

	if (fakesensor_regulator != NULL && regulator_is_enabled(fakesensor_regulator))
	{
		pin_value = gpio_get_value(fakesensor_output);
		pr_info("pin_vale:%d\n",pin_value);
		if(pin_value)
		{
			y_average = 100;
		}
		else
		{
			y_average = 1;
		}
	}
	mutex_unlock(&main2_light_mutex);
	pr_info("main2_light_show:%d\n",y_average);
	return sprintf(buf,"%d\n", y_average);
}

static ssize_t main2_light_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int mode = 0;
	char err = 0;
	sscanf(buf,"%d",&mode);
	pr_info("main2_light_store modebuf = %d\n", mode);
	mutex_lock(&main2_light_mutex);
	if (fakesensor_regulator != NULL)
	{
		if (mode)
			err = regulator_enable(fakesensor_regulator);
		else
			if (regulator_is_enabled(fakesensor_regulator))
				regulator_disable(fakesensor_regulator);
	}
	else
	{
		pr_info("main2_light_store fakesensor_regulator = NULL ");
	}
	mutex_unlock(&main2_light_mutex);
	pr_info("main2_light_store\n");
	return size;
}

static DEVICE_ATTR_RW(main2_light);
extern void update_fakesensor_info(int index, const char *sensor_matecodes, int len);

int main2_light_init(void)
{
	if(fakesensor_init() == 0)
	{
		update_fakesensor_info(fakesensor_index, sensor_matecodes, strlen(sensor_matecodes));
	}
	else
	{
		pr_err("[main2_light_init] fakesensor_init failed\n");
		return 0;
	}

	fakesensor_class = class_create( "main2_light");

	if (IS_ERR(fakesensor_class) || fakesensor_class==0)
	{
		pr_err("[main2_light_init] Unable to create class, err = %d\n", (int)PTR_ERR(fakesensor_class));
		return 0;
	}

	fakesensor_dev = device_create(fakesensor_class, NULL, 0, 0,"main2_light");

	if(NULL == fakesensor_dev)
	{
		pr_err("[main2_light_init] device_create fail\n");
		return 0;
	}

	device_create_file(fakesensor_dev, &dev_attr_main2_light);

	return 0;
}

void main2_light_exit(void)
{
	if (fakesensor_dev != NULL) {
		device_remove_file(fakesensor_dev, &dev_attr_main2_light);
		device_unregister(fakesensor_dev);
	}
	if(fakesensor_class!=NULL)
		class_destroy(fakesensor_class);

	if (fakesensor_output > 0)
		gpio_free(fakesensor_output);
}

