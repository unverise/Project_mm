// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 Transsion Inc.
 */
#include <linux/ctype.h>
#include <linux/device.h>
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
#include <linux/platform_device.h>
#include <linux/of.h>
#include "tran_led_core.h"


struct ioctl_frame_data {
    uint32_t length;
    int *data;
};


#define LED_IOCTL_MAGIC 0x12
#define TRAN_LED_GET_CHIP_TYPE 			_IOR(LED_IOCTL_MAGIC, 0, uint8_t)
#define TRAN_LED_GET_CMD 				_IOR(LED_IOCTL_MAGIC, 1, int32_t[TRAN_LED_CMD_LENGTH])
#define TRAN_LED_SET_CMD 				_IOW(LED_IOCTL_MAGIC, 2, int32_t[TRAN_LED_CMD_LENGTH])
#define TRAN_LED_I2C_RDWR 				_IOW(LED_IOCTL_MAGIC, 3, int)
#define TRAN_LED_I2C_RETRIES 			_IOW(LED_IOCTL_MAGIC, 4, int)
#define TRAN_LED_I2C_TIMEOUT 			_IOW(LED_IOCTL_MAGIC, 5, int)
#define TRAN_LED_GET_FRAME_SIZE 		_IOW(LED_IOCTL_MAGIC, 6, int)
#define TRAN_LED_SEND_FRAME				_IOW(LED_IOCTL_MAGIC, 8, struct ioctl_frame_data)




#define TRAN_LED_FRAME_TYPE 0x03


struct tran_led_core *ptlc = NULL;
static int tran_led_dev_check(struct tran_led_core * tlc);


int tlc_get_debug_level(void)
{
	if (IS_ERR_OR_NULL(ptlc))
		return LEDLOG_DEBUG_LEVEL; 

	return ptlc->log_level;
}
EXPORT_SYMBOL(tlc_get_debug_level);

static ssize_t store_tran_led_log_level(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct tran_led_core *tlc = dev->driver_data;
	int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < 0) {
			tlc_err("%s: val is invalid: %d\n", __func__, temp);
			temp = 0;
		}
		tlc->log_level = temp;
		tlc_err("%s: log_level=%d\n", __func__, tlc->log_level);

	} else {
		tlc_err("%s: format error!\n", __func__);
	}
	return size;
}

static ssize_t show_tran_led_log_level(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tran_led_core *tlc = dev->driver_data;
	tlc_debug("%s: %d\n", __func__, tlc->log_level);
	return sprintf(buf, "%d\n", tlc->log_level);
}
static DEVICE_ATTR(tran_led_log_level, 0664, show_tran_led_log_level, store_tran_led_log_level);

/********** API FOR MIDTEST MODE OR FACTORY MODE **********/
static ssize_t store_tran_led_cmd(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct tran_led_core *tlc = dev->driver_data;
	if (!size)
		return 0;
	if(!tlc->leddev || tlc->leddev1){
		tran_led_dev_check(tlc);
	}
	if(tlc->leddev)
		tran_led_dev_store_tran_led_cmd(tlc->leddev, buf);
	if(tlc->leddev1)
		tran_led_dev_store_tran_led_cmd(tlc->leddev1, buf);
	return size;
}

static ssize_t show_tran_led_cmd(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tran_led_core *tlc = dev->driver_data;
	char pBuf;

	tran_led_dev_show_tran_led_cmd(tlc->leddev, &pBuf);
	return sprintf(buf, "%c\n", pBuf);
}
static DEVICE_ATTR(tran_led_cmd, 0664, show_tran_led_cmd, store_tran_led_cmd);

static ssize_t store_tran_led_check(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct tran_led_core *tlc = dev->driver_data;
	if (!size)
		return 0;
    tran_led_dev_store_tran_led_check(tlc->leddev, buf);
    return size;
}

static ssize_t show_tran_led_check(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tran_led_core *tlc = dev->driver_data;
	return tran_led_dev_show_tran_led_check(tlc->leddev, buf);
}
static DEVICE_ATTR(tran_led_check, 0664, show_tran_led_check, store_tran_led_check);

static ssize_t store_tran_pdlc_cmd(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct tran_led_core *tlc = dev->driver_data;
	if (!size)
		return 0;
    tran_led_dev_store_tran_led_cmd(tlc->leddev1, buf);
    return size;
}

static ssize_t show_tran_pdlc_cmd(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tran_led_core *tlc = dev->driver_data;
	char pBuf;
	tran_led_dev_show_tran_led_cmd(tlc->leddev1, &pBuf);
	return sprintf(buf, "%c\n", pBuf);
}
static DEVICE_ATTR(tran_pdlc_cmd, 0664, show_tran_pdlc_cmd, store_tran_pdlc_cmd);

static struct attribute *tran_led_core_class_attrs[] = {
	&dev_attr_tran_led_log_level.attr,
	&dev_attr_tran_led_cmd.attr,
	&dev_attr_tran_led_check.attr,
	&dev_attr_tran_pdlc_cmd.attr,
	NULL,
};

static const struct attribute_group led_group = {
	.attrs = tran_led_core_class_attrs,
};

static int tlc_setup_files(struct tran_led_core *tlc)
{
	int ret = 0;
	struct platform_device *pdev = tlc->pdev;
	struct kobject *kobj;

	ret = sysfs_create_group(&pdev->dev.kobj, &led_group);
	if(ret < 0){
		tlc_err("%s : sysfs_create_group failed ret=%d\n", __func__,ret);
		goto _out;
	}

	kobj = kobject_create_and_add("led", NULL);
	if (!kobj) {
		tlc_err("%s:sysfs_create_group fail\n",__func__);
		goto _out;
	}

	ret = sysfs_create_link(kobj,&pdev->dev.kobj,"led");
	if(ret < 0){
		tlc_err("%s : sysfs_create_link failed ret=%d\n", __func__,ret);
		goto _out;
	}

	if(tlc->support_pdlc){
		kobj = kobject_create_and_add("pdlc", NULL);
		if (!kobj) {
			tlc_err("%s:sysfs_create_group fail\n",__func__);
			goto _out;
		}

		ret = sysfs_create_link(kobj,&pdev->dev.kobj,"pdlc");
		if(ret < 0){
			tlc_err("%s : sysfs_create_link failed ret=%d\n", __func__,ret);
			goto _out;
		}
	}
_out:
	return ret;
}

static int tran_led_dev_check(struct tran_led_core * tlc)
{
	if(!IS_ERR_OR_NULL(tlc->leddev))
		return 0;

	tlc->leddev = get_leddev_by_name("primary_led");
	if (IS_ERR_OR_NULL(tlc->leddev)){
		tlc_err("%s: leddev is invalid\n", __func__);
		return -ENODEV;
	}

	tlc->leddev1 = get_leddev_by_name("second_led");
	if (IS_ERR_OR_NULL(tlc->leddev1)){
		tlc_err("%s: leddev1 is invalid\n", __func__);
	}

	return 0;
}

static int tran_led_open(struct inode *inode, struct file *file)
{
    struct tran_led_core * tlc = container_of(inode->i_cdev, struct tran_led_core, cdev);

  	if (IS_ERR_OR_NULL(tlc)){
		tlc_err("%s: tlc is invalid\n", __func__);
  		return -ENODEV;
	}

	file->private_data = tlc;
	return 0;
}

static int tran_led_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}


static long tran_led_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tran_led_core * tlc = file->private_data;
	int32_t cmd_data[TRAN_LED_CMD_LENGTH] = {0};
	enum ChipType chip_type = DRV_OTHER;
	int ret = 0;
	int32_t cmd_type,dev_num;
	struct ioctl_frame_data data;
	uint32_t *kernel_buffer;
	int32_t frame_size;

	if(tran_led_dev_check(tlc) < 0)
		return -ENODEV;

	tlc_debug("ioctl, cmd=0x%02x, arg=0x%02lx\n",cmd, arg);

	memset(cmd_data, 0, sizeof(cmd_data));

	mutex_lock(&tlc->tran_led_mutex);

	switch (cmd) {
	case TRAN_LED_GET_CHIP_TYPE:
		tran_led_dev_get_chip_type(tlc->leddev, &chip_type);
		tlc->chip_type = chip_type;
		if(copy_to_user((unsigned char __user *)arg, &chip_type, sizeof(chip_type))) {
			ret = -EFAULT;
			goto out;
		}
		break;
	case TRAN_LED_GET_CMD:
		if(tlc->chip_type >= HAL_SO_SOLUTION_CHIP){
            ret = -EINVAL;
            goto out;
		}

		tran_led_dev_get_cmd(tlc->leddev, cmd_data, sizeof(cmd_data));
		if(copy_to_user((int32_t __user *)arg, cmd_data, sizeof(cmd_data))) {
            ret = -EFAULT;
            goto out;
		}
		break;
	case TRAN_LED_SET_CMD:
		if (copy_from_user(cmd_data, (int32_t __user *)arg, sizeof(cmd_data))) {
			tlc_err("Fail to copy data from user\n");
            ret = -EFAULT;
            goto out;
		}
		tlc_debug("Received data: %d %d %d %d %d %d %d %d\n",
			   cmd_data[0], cmd_data[1], cmd_data[2], cmd_data[3], cmd_data[4], cmd_data[5], cmd_data[6], cmd_data[7]);

		cmd_type = cmd_data[0];
		dev_num = cmd_data[1];
		if(cmd_type < HAL_SO_SOLUTION){
			switch(dev_num){
				case 0xff:
					tran_led_dev_set_cmd(tlc->leddev, cmd_data, sizeof(cmd_data));
					tran_led_dev_set_cmd(tlc->leddev1, cmd_data, sizeof(cmd_data));
					break;
				case 0x01:
					tran_led_dev_set_cmd(tlc->leddev, cmd_data, sizeof(cmd_data));
					break;
				case 0x02:
					tran_led_dev_set_cmd(tlc->leddev1, cmd_data, sizeof(cmd_data));
					break;
				default:
					tlc_err("Not support:dev_num\n");
					break;
			}
		}
		break;
	case TRAN_LED_I2C_RDWR: 
		if(tlc->chip_type < HAL_SO_SOLUTION_CHIP){
            ret = -EINVAL;
            goto out;
		}

		tran_led_dev_rw_i2c_data(tlc->leddev, arg);
		break;
	case TRAN_LED_I2C_RETRIES:
		if (arg > INT_MAX || (tlc->chip_type < HAL_SO_SOLUTION_CHIP)){
            ret = -EINVAL;
            goto out;
		}
		tran_led_dev_set_i2c_retries(tlc->leddev, arg);
		break;
	case TRAN_LED_I2C_TIMEOUT:
		if (arg > INT_MAX || (tlc->chip_type < HAL_SO_SOLUTION_CHIP)){
			ret = -EINVAL;
			goto out;
		}
		tran_led_dev_set_i2c_timeout(tlc->leddev, arg);
		break;
	case TRAN_LED_GET_FRAME_SIZE:
		if(tlc->leddev && tlc->leddev->ops && tlc->leddev->ops->get_frame_size){
			frame_size = tlc->leddev->ops->get_frame_size(tlc->leddev);
		}

		if(tlc->leddev1 && tlc->leddev1->ops && tlc->leddev1->ops->get_frame_size){
			if(tlc->leddev1->ops->get_frame_size(tlc->leddev1) > frame_size){
				frame_size = tlc->leddev1->ops->get_frame_size(tlc->leddev1);
			}
		}

		if(copy_to_user((unsigned char __user *)arg, &frame_size, sizeof(frame_size))) {
			ret = -EFAULT;
			goto out;
		}
		break;
	case TRAN_LED_SEND_FRAME:
		if (copy_from_user(&data, (void  __user *)arg, sizeof(data))) {
			tlc_err("Fail to copy data from user\n");
			ret = -EFAULT;
			goto out;
		}
		kernel_buffer = kmalloc(data.length * sizeof(uint32_t), GFP_KERNEL);
		if (!kernel_buffer) {
			tlc_err("kmalloc buffer failed !!");
			ret = -ENOMEM;
			goto out;

		}
		if (copy_from_user(kernel_buffer, data.data, data.length * sizeof(uint32_t))) {
			kfree(kernel_buffer);
			tlc_err("copy_from_user failed  lenth=%d!! ", data.length);
			ret = -EFAULT;
			goto out;
		}

		if(kernel_buffer[0] == 3 && kernel_buffer[1] == 1){
			tran_led_dev_set_cmd(tlc->leddev, kernel_buffer, data.length);
		}
		if(kernel_buffer[0] == 3 && kernel_buffer[1] == 2){
			tran_led_dev_set_cmd(tlc->leddev1, kernel_buffer, data.length);
		}

		kfree(kernel_buffer);
		kernel_buffer = NULL;
		break;
	default:
		tlc_err("Not support:cmd\n");
		ret = -ENOTTY;
		break;
	}

out:
	if(ret)
		tlc_debug("ioctl fail,ret = %d\n",ret);
	mutex_unlock(&tlc->tran_led_mutex);
	return ret;
}

/********** API FOR APP & HAL  **********/
static struct file_operations tran_led_ops = {
    .owner	 = THIS_MODULE,
    .read    = NULL,
    .write   = NULL,
    .open    = tran_led_open,
    .release = tran_led_release,
    .unlocked_ioctl = tran_led_ioctl,
};
static int tlc_cdev_init(struct tran_led_core *tlc)
{
	int ret;

	ret = alloc_chrdev_region(&tlc->dev_num, 0, 1, "tran_led");
	if (ret < 0) {
		tlc_err("Failed to allocate char device region\n");
		return ret;
	}

	cdev_init(&tlc->cdev, &tran_led_ops);
	tlc->cdev.owner = THIS_MODULE;

	ret = cdev_add(&tlc->cdev, tlc->dev_num, 1);
	if (ret) {
		tlc_err("Failed to add cdev\n");
		goto unreg_chrdev;
	}

    ret = tran_led_class_init(tlc->pdev);
    if(ret < 0) {
		tlc_err("Failed to add led class\n");
		goto del;
    }
    device_create(tlc->tran_led_class, NULL, tlc->dev_num,NULL, "tran_led");

	return 0;
del:
	cdev_del(&tlc->cdev);
unreg_chrdev:
	unregister_chrdev_region(tlc->dev_num, 1);
    return -1;
}

static void tlc_parse_dt(struct tran_led_core *tlc)
{
	struct device *dev = &tlc->pdev->dev;
	struct device_node *np = dev->of_node;

	tlc->support_pdlc = of_property_read_bool(np, "support_pdlc");
}

static int tran_led_core_probe(struct platform_device *pdev)
{
	struct tran_led_core *tlc = NULL;

	tlc_debug("%s: starts\n", __func__);

	tlc = devm_kzalloc(&pdev->dev, sizeof(*tlc), GFP_KERNEL);
	if (!tlc)
		return -ENOMEM;

	tlc->log_level = LEDLOG_DEBUG_LEVEL;
	mutex_init(&tlc->tran_led_mutex);
	platform_set_drvdata(pdev, tlc);
	tlc->pdev = pdev;
	tlc->support_pdlc = false;

	tlc_parse_dt(tlc);

	tlc_setup_files(tlc);

	tlc_cdev_init(tlc);

	ptlc = tlc;
	tlc_debug("%s: done\n", __func__);
	return 0;
}

static void tran_led_core_remove(struct platform_device *dev)
{
	struct tran_led_core *tlc =	platform_get_drvdata(dev);

	mutex_destroy(&tlc->tran_led_mutex);
	cdev_del(&tlc->cdev);
	unregister_chrdev_region(tlc->dev_num, 1);
	tran_led_class_destroy(tlc->pdev);
	return;
}

static void tran_led_core_shutdown(struct platform_device *dev)
{
	struct tran_led_core *tlc =	platform_get_drvdata(dev);

	mutex_destroy(&tlc->tran_led_mutex);
	cdev_del(&tlc->cdev);
	unregister_chrdev_region(tlc->dev_num, 1);
	tran_led_class_destroy(tlc->pdev);
	return;
}

static const struct of_device_id tran_led_core_of_match[] = {
	{.compatible = "tran,led_core",},
	{},
};

MODULE_DEVICE_TABLE(of, tran_led_core_of_match);

static struct platform_driver tran_led_core_driver = {
	.probe = tran_led_core_probe,
	.remove = tran_led_core_remove,
	.shutdown = tran_led_core_shutdown,
	.driver = {
		   .name = "tran_led_core",
		   .of_match_table = tran_led_core_of_match,
	},
};

static int __init tran_led_core_init(void)
{
	return platform_driver_register(&tran_led_core_driver);
}

late_initcall(tran_led_core_init);

static void __exit tran_led_core_exit(void)
{
	platform_driver_unregister(&tran_led_core_driver);
}
module_exit(tran_led_core_exit);

MODULE_DESCRIPTION("Tran Led Core Driver");
MODULE_LICENSE("GPL");
