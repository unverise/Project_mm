#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_wakeup.h>

#define DEVICE_NAME "gpio_sar_monitor"

struct gpio_sar_monitor_data {
    struct gpio_desc *gpio_sar;
    struct input_dev *dev;
    int irq;
};

static void report_gpio_state(struct gpio_sar_monitor_data *gpio_sar_data)
{
    int gpio_state = gpiod_get_value(gpio_sar_data->gpio_sar);
    printk("report_gpio_state: gpio_state=%d\n", gpio_state);
    input_report_key(gpio_sar_data->dev, KEY_VENDOR, 1);
    input_sync(gpio_sar_data->dev);
    input_report_key(gpio_sar_data->dev, KEY_VENDOR, 0);
    input_sync(gpio_sar_data->dev);
}

static irqreturn_t gpio_interrupt_handler(int irq, void *dev_id)
{
    struct gpio_sar_monitor_data *gpio_sar_data = dev_id;
    if (!gpio_sar_data)
        return IRQ_NONE;
    pm_wakeup_event(gpio_sar_data->dev->dev.parent, 500);
    report_gpio_state(gpio_sar_data);
    return IRQ_HANDLED;
}

static ssize_t gpio_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct gpio_sar_monitor_data *gpio_sar_data = dev_get_drvdata(dev);
    if (!gpio_sar_data)
        return -EINVAL;
    int gpio_state = gpiod_get_value(gpio_sar_data->gpio_sar);
    return sprintf(buf, "%d\n", gpio_state);
}

static DEVICE_ATTR_RO(gpio_state);

static struct attribute *gpio_sar_monitor_attrs[] = {
    &dev_attr_gpio_state.attr,
    NULL,
};

static const struct attribute_group gpio_sar_monitor_attr_group = {
    .attrs = gpio_sar_monitor_attrs,
};

static int gpio_sar_monitor_probe(struct platform_device *pdev)
{
    int ret;
    struct device *dev = &pdev->dev;
    struct gpio_sar_monitor_data *gpio_sar_data;

    gpio_sar_data = devm_kzalloc(dev, sizeof(*gpio_sar_data), GFP_KERNEL);
    if (!gpio_sar_data) {
        printk("GPIO SAR devm_kzalloc error\n");
        return -ENOMEM;
    }

    gpio_sar_data->gpio_sar = devm_gpiod_get(dev, "sar_monitor", GPIOD_IN);
    if (IS_ERR_OR_NULL(gpio_sar_data->gpio_sar)) {
        printk("GPIO SAR devm_gpiod_get error\n");
        return -EINVAL;
    }

    gpio_sar_data->dev = devm_input_allocate_device(dev);
    if (!gpio_sar_data->dev) {
        printk("GPIO SAR Failed to allocate input device\n");
        return -ENOMEM;
    }

    gpio_sar_data->dev->name = DEVICE_NAME;
    gpio_sar_data->dev->dev.parent = dev;

    input_set_capability(gpio_sar_data->dev, EV_KEY, KEY_VENDOR);

    ret = input_register_device(gpio_sar_data->dev);
    if (ret) {
        printk("GPIO SAR Failed to register input device\n");
        return ret;
    }

    gpio_sar_data->irq = gpiod_to_irq(gpio_sar_data->gpio_sar);
    if (gpio_sar_data->irq < 0) {
        printk("GPIO SAR Failed to convert GPIO to IRQ\n");
        return -EINVAL;
    }

    ret = devm_request_threaded_irq(dev, gpio_sar_data->irq, NULL, gpio_interrupt_handler,
                                    IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                    "gpio_sar_monitor", gpio_sar_data);
    if (ret) {
        printk("GPIO SAR Failed to request IRQ\n");
        return ret;
    }

    platform_set_drvdata(pdev, gpio_sar_data);

    device_init_wakeup(dev, true);

    ret = devm_device_add_group(dev, &gpio_sar_monitor_attr_group);
    if (ret) {
        dev_err(&pdev->dev, "GPIO SAR Failed to create sysfs group\n");
        return ret;
    }

    report_gpio_state(gpio_sar_data);
    printk("GPIO SAR first report done\n");

    printk("GPIO SAR state monitor initialized done\n");

    return 0;
}

static void gpio_sar_monitor_remove(struct platform_device *pdev)
{
    device_init_wakeup(&pdev->dev, false);
}

static const struct of_device_id gpio_sar_monitor_of_match[] = {
    { .compatible = "gpio_sar_monitor" },
    { },
};
MODULE_DEVICE_TABLE(of, gpio_sar_monitor_of_match);

static int __maybe_unused gpio_sar_monitor_suspend(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct gpio_sar_monitor_data *gpio_sar_data = platform_get_drvdata(pdev);

    if (device_may_wakeup(dev))
        enable_irq_wake(gpio_sar_data->irq);

    return 0;
}

static int __maybe_unused gpio_sar_monitor_resume(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct gpio_sar_monitor_data *gpio_sar_data = platform_get_drvdata(pdev);

    if (device_may_wakeup(dev))
        disable_irq_wake(gpio_sar_data->irq);

    return 0;
}

static const struct dev_pm_ops gpio_sar_monitor_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(gpio_sar_monitor_suspend, gpio_sar_monitor_resume)
};

static struct platform_driver gpio_sar_monitor_driver = {
    .probe = gpio_sar_monitor_probe,
    .remove = gpio_sar_monitor_remove,
    .driver = {
        .name = DEVICE_NAME,
        .of_match_table = gpio_sar_monitor_of_match,
        .pm = &gpio_sar_monitor_pm_ops,
    },
};

module_platform_driver(gpio_sar_monitor_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ningninggao");
MODULE_DESCRIPTION("GPIO SAR Monitor Module");
