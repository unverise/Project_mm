/* drivers/char/trangpio.c
 *
 * TRANGPIO driver (single /dev/trangpio controlling 5 GPIOs via pinctrl states)
 *
 * Compatible string: "tran,trangpio"
 *
 * Copyright ...
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/compiler_attributes.h>

#define TRANGPIO_DTNAME "tran,trangpio"
#define TRANGPIO_NAME "trangpio"
#define GPIO_COUNT 2
#define MAX_BUF 128

/* Module parameter for debug control */
static bool debug_enabled = false;
module_param(debug_enabled, bool, 0644);
MODULE_PARM_DESC(debug_enabled, "Enable debug logging (default: false)");

/* Debug logging macros */
#define trangpio_dbg(dev, fmt, ...) \
    do { \
        if (debug_enabled) \
            dev_info(dev, "[trangpio] " fmt, ##__VA_ARGS__); \
    } while (0)

#define trangpio_dbg_pr(fmt, ...) \
    do { \
        if (debug_enabled) \
            pr_info("[trangpio] " fmt, ##__VA_ARGS__); \
    } while (0)

/* Timing configuration structure */
struct trangpio_timing {
    unsigned int gpio_switch_delay_min;  /* microseconds */
    unsigned int gpio_switch_delay_max;  /* microseconds */
    unsigned int stabilization_delay;    /* milliseconds */
};
struct gpio_state_map {
    const char *name_high;
    const char *name_low;
    struct pinctrl_state *high;
    struct pinctrl_state *low;
};
struct trans_trangpio_data {
    struct pinctrl *pctrl;
    struct gpio_state_map gpios[GPIO_COUNT];
    int gpio_status; /* bit i corresponds to gpios[i] (i=0..GPIO_COUNT-1) */
    struct cdev cdev;
    dev_t devt;
    struct device *device;
    struct device *dev; /* parent platform device */
    struct mutex lock;
    struct trangpio_timing timing; /* configurable timing parameters */
};
static struct class *trangpio_class;
/* file ops */
static ssize_t trangpio_read(struct file *file, char __user *buf,
                  size_t count, loff_t *ppos);
static ssize_t trangpio_write(struct file *file, const char __user *buf,
                   size_t count, loff_t *ppos);
static int trangpio_open(struct inode *inode, struct file *file);
static int trangpio_release(struct inode *inode, struct file *file);
static const struct file_operations trangpio_fops = {
    .owner = THIS_MODULE,
    .open = trangpio_open,
    .release = trangpio_release,
    .read = trangpio_read,
    .write = trangpio_write,
};
/* pinctrl state names must match DTS */
static const char *gpio_state_names[GPIO_COUNT][2] = {
    { "gpio140_high", "gpio140_low" },
    { "gpio141_high", "gpio141_low" },
};

/* GPIO number to index mapping */
static const int gpio_numbers[GPIO_COUNT] = {140, 141};

/* Find GPIO index by GPIO number */
static int find_gpio_index(int gpio_num) {
    int i;
    for (i = 0; i < GPIO_COUNT; i++) {
        if (gpio_numbers[i] == gpio_num) {
            return i;
        }
    }
    return -1;
}

/* Get timing configuration from device tree */
static void trans_trangpio_get_timing_info(struct device *dev,
                        struct trans_trangpio_data *data)
{
    struct device_node *np = dev->of_node;

    /* Set default values */
    data->timing.gpio_switch_delay_min = 1000;  /* 1ms */
    data->timing.gpio_switch_delay_max = 2000;  /* 2ms */
    data->timing.stabilization_delay = 20;      /* 20ms */

    if (!np) {
        trangpio_dbg(dev, "No device tree node, using default timing\n");
        return;
    }

    /* Read timing parameters from device tree */
    of_property_read_u32(np, "gpio-switch-delay-min",
             &data->timing.gpio_switch_delay_min);
    of_property_read_u32(np, "gpio-switch-delay-max",
             &data->timing.gpio_switch_delay_max);
    of_property_read_u32(np, "stabilization-delay",
             &data->timing.stabilization_delay);

    trangpio_dbg(dev, "Timing config: switch_delay=%u-%uus, stabilization=%ums\n",
         data->timing.gpio_switch_delay_min,
         data->timing.gpio_switch_delay_max,
         data->timing.stabilization_delay);
}

static int trans_trangpio_get_dts_info(struct device *dev,
                    struct trans_trangpio_data *data)
{
    int i;
    struct pinctrl *pctrl;

    trangpio_dbg(dev, "Starting DTS info retrieval...\n");

    /* Get timing configuration first */
    trans_trangpio_get_timing_info(dev, data);

    pctrl = devm_pinctrl_get(dev);
    if (IS_ERR(pctrl)) {
        dev_err(dev, "Failed to get pinctrl: %ld\n", PTR_ERR(pctrl));
        return PTR_ERR(pctrl);
    }
    data->pctrl = pctrl;
    trangpio_dbg(dev, "pinctrl handle obtained successfully\n");

    for (i = 0; i < GPIO_COUNT; i++) {
        data->gpios[i].name_high = gpio_state_names[i][0];
        data->gpios[i].name_low  = gpio_state_names[i][1];

        trangpio_dbg(dev, "GPIO %d (number %d): configuring states '%s'/'%s'\n",
             i, gpio_numbers[i], data->gpios[i].name_high, data->gpios[i].name_low);

        data->gpios[i].high = pinctrl_lookup_state(data->pctrl,
                           data->gpios[i].name_high);
        if (IS_ERR(data->gpios[i].high)) {
            dev_err(dev, "Failed to lookup state '%s': %ld\n",
            data->gpios[i].name_high,
            PTR_ERR(data->gpios[i].high));
            return PTR_ERR(data->gpios[i].high);
        }

        data->gpios[i].low = pinctrl_lookup_state(data->pctrl,
                          data->gpios[i].name_low);
        if (IS_ERR(data->gpios[i].low)) {
            dev_err(dev, "Failed to lookup state '%s': %ld\n",
            data->gpios[i].name_low,
            PTR_ERR(data->gpios[i].low));
            return PTR_ERR(data->gpios[i].low);
        }

        trangpio_dbg(dev, "GPIO %d configured successfully\n", i);
    }
    trangpio_dbg(dev, "All pinctrl states loaded for %d gpios\n", GPIO_COUNT);
    return 0;
}
static int trans_trangpio_single_gpio_ctl(struct trans_trangpio_data *data,
                       int gpio_idx, int onoff)
{
    struct pinctrl_state *state;
    const char *state_name;
    int ret;

    if (!data || !data->pctrl || gpio_idx < 0 || gpio_idx >= GPIO_COUNT) {
        pr_err("trangpio: invalid args to single_gpio_ctl (idx=%d, onoff=%d)\n",
           gpio_idx, onoff);
        return -EINVAL;
    }

    state = onoff ? data->gpios[gpio_idx].high : data->gpios[gpio_idx].low;
    state_name = onoff ? data->gpios[gpio_idx].name_high :
             data->gpios[gpio_idx].name_low;

    if (IS_ERR_OR_NULL(state)) {
        dev_err(data->dev, "state NULL/ERR for GPIO %d (number %d), state=%s\n",
        gpio_idx, gpio_numbers[gpio_idx], state_name);
        return -EINVAL;
    }

    trangpio_dbg(data->dev, "Setting GPIO %d (number %d) to %s (state: %s)\n",
         gpio_idx, gpio_numbers[gpio_idx], onoff ? "HIGH" : "LOW",
         state_name);

    ret = pinctrl_select_state(data->pctrl, state);
    if (ret) {
        dev_err(data->dev, "Failed to set GPIO %d (number %d) to %s: %d\n",
        gpio_idx, gpio_numbers[gpio_idx], onoff ? "HIGH" : "LOW",
        ret);
        return ret;
    }

    trangpio_dbg(data->dev, "GPIO %d (number %d) state applied successfully\n",
                 gpio_idx, gpio_numbers[gpio_idx]);
    return 0;
}
static int trans_trangpio_multi_gpio_ctl(struct trans_trangpio_data *data,
                      int gpio_status)
{
    int i, ret;
    bool any_high = false;

    if (!data) {
        pr_err("trans_trangpio_multi_gpio_ctl: data is NULL\n");
        return -ENODEV;
    }

    trangpio_dbg(data->dev, "Multi GPIO control: status=0x%x (binary: %s%s)\n",
                 gpio_status,
                 (gpio_status & 0x02) ? "1" : "0",
                 (gpio_status & 0x01) ? "1" : "0");

    for (i = 0; i < GPIO_COUNT; i++) {
        int bit = (gpio_status >> i) & 1;
        trangpio_dbg(data->dev, "Processing GPIO %d (number %d): bit=%d\n",
                     i, gpio_numbers[i], bit);

        ret = trans_trangpio_single_gpio_ctl(data, i, bit);
        if (ret) {
            dev_err(data->dev, "Failed to control GPIO %d: %d\n",
            gpio_numbers[i], ret);
            return ret;
        }

        if (bit)
            any_high = true;

        if (i < GPIO_COUNT - 1) {
            trangpio_dbg(data->dev, "Delay between GPIO operations (%u-%uμs)\n",
                         data->timing.gpio_switch_delay_min,
             data->timing.gpio_switch_delay_max);
            usleep_range(data->timing.gpio_switch_delay_min,
             data->timing.gpio_switch_delay_max);
        }
    }

    if (any_high) {
        trangpio_dbg(data->dev, "At least one GPIO is high, applying %ums stabilization delay\n",
             data->timing.stabilization_delay);
        msleep(data->timing.stabilization_delay);
    }

    trangpio_dbg(data->dev, "Multi GPIO control completed\n");
    return 0;
}
/* file ops implementation */
static int trangpio_open(struct inode *inode, struct file *file)
{
    struct trans_trangpio_data *data = container_of(inode->i_cdev,
                             struct trans_trangpio_data,
                             cdev);
    if (!data) {
        pr_err("trangpio_open: data is NULL\n");
        return -ENODEV;
    }
    file->private_data = data;
    trangpio_dbg(data->dev, "trangpio device opened (current GPIO status: 0x%x)\n",
         data->gpio_status);
    return 0;
}

static int trangpio_release(struct inode *inode, struct file *file)
{
    struct trans_trangpio_data *data = file->private_data;
    if (data) {
        trangpio_dbg(data->dev, "trangpio device released (final GPIO status: 0x%x)\n",
         data->gpio_status);
    } else {
        pr_err("trangpio_release: data is NULL\n");
    }
    return 0;
}
static ssize_t trangpio_read(struct file *file, char __user *buf,
                  size_t count, loff_t *ppos)
{
    struct trans_trangpio_data *data = file->private_data;
    char kbuf[MAX_BUF];
    size_t len = 0;
    int i;
    size_t to_copy, remaining;

    if (!data) {
        pr_err("trangpio_read: data is NULL\n");
        return -ENODEV;
    }

    trangpio_dbg(data->dev, "READ: count=%zu, ppos=%lld, GPIO status=0x%x\n",
         count, *ppos, data->gpio_status);

    if (mutex_lock_interruptible(&data->lock)) {
        dev_warn(data->dev, "Failed to acquire mutex (interrupted)\n");
        return -ERESTARTSYS;
    }

    /* build format: "141:0,140:0" */
    for (i = GPIO_COUNT - 1; i >= 0; i--) {
        int gpio_state = (data->gpio_status >> i) & 1;

        trangpio_dbg(data->dev, "GPIO[%d]: num=%d, state=%d (bit %d)\n",
             i, gpio_numbers[i], gpio_state, i);

        /* Check remaining space before each snprintf */
        size_t remaining = sizeof(kbuf) - len;
        size_t needed = snprintf(NULL, 0, "%s%d:%d",
                                (i == GPIO_COUNT - 1) ? "" : ",",
                                gpio_numbers[i], gpio_state) + 1;
        if (remaining < needed) {
            dev_warn(data->dev, "Output buffer full, truncating at %zu bytes\n", len);
            break;
        }

        len += snprintf(kbuf + len, sizeof(kbuf) - len, "%s%d:%d",
                       (i == GPIO_COUNT - 1) ? "" : ",",
                       gpio_numbers[i], gpio_state);
    }

    /* Ensure we have space for '\n' and '\0' */
    if (len < sizeof(kbuf) - 1) {
        kbuf[len++] = '\n';
    }
    if (len < sizeof(kbuf)) {
        kbuf[len] = '\0';
    } else {
        kbuf[sizeof(kbuf) - 1] = '\0';  /* Force null termination */
    }

    trangpio_dbg(data->dev, "Output string (%zu bytes): '%s'\n",
         len, kbuf);

    if (*ppos >= (loff_t)len) {
        trangpio_dbg(data->dev, "EOF reached (ppos=%lld >= len=%zu)\n",
             *ppos, len);
        mutex_unlock(&data->lock);
        return 0;
    }

    /* Safe calculation to avoid integer underflow */
    remaining = len - (size_t)*ppos;
    to_copy = min(remaining, count);

    if (copy_to_user(buf, kbuf + *ppos, to_copy)) {
        dev_err(data->dev, "Failed to copy data to user space\n");
        mutex_unlock(&data->lock);
        return -EFAULT;
    }

    *ppos += to_copy;
    mutex_unlock(&data->lock);

    trangpio_dbg(data->dev, "READ completed: copied %zu bytes, new ppos=%lld\n",
         to_copy, *ppos);
    return to_copy;
}
static ssize_t trangpio_write(struct file *file, const char __user *buf,
                   size_t count, loff_t *ppos)
{
    struct trans_trangpio_data *data = file->private_data;
    char kbuf[MAX_BUF];
    char *token, *gpio_config, *colon_pos;
    size_t input_len;
    int gpio_num, gpio_state, gpio_index;
    int new_gpio_status;
    int ret;
    int __maybe_unused gpio_count = 0;

    if (!data) {
        pr_err("trangpio_write: data is NULL\n");
        return -ENODEV;
    }

    trangpio_dbg(data->dev, "WRITE: count=%zu bytes\n", count);

    if (count == 0) {
        trangpio_dbg(data->dev, "Empty write request\n");
        return 0;
    }
    if (count >= sizeof(kbuf)) {
        dev_err(data->dev, "Write buffer too large: %zu >= %zu\n",
        count, sizeof(kbuf));
        return -EINVAL;
    }
    if (copy_from_user(kbuf, buf, count)) {
        dev_err(data->dev, "Failed to copy data from user space\n");
        return -EFAULT;
    }
    kbuf[min(count, (size_t)(sizeof(kbuf) - 1))] = '\0';
    trangpio_dbg(data->dev, "Raw input string (%zu bytes): '%s'\n",
         count, kbuf);

    /* trim trailing whitespace */
    input_len = count;
    while (input_len > 0 &&
           (kbuf[input_len - 1] == '\n' || kbuf[input_len - 1] == '\r' ||
            kbuf[input_len - 1] == ' '  || kbuf[input_len - 1] == '\t')) {
        kbuf[input_len - 1] = '\0';
        input_len--;
    }
    if (input_len == 0) {
        dev_err(data->dev, "Input string is empty after trimming\n");
        return -EINVAL;
    }
    trangpio_dbg(data->dev, "Trimmed input string (%zu bytes): '%s'\n",
         input_len, kbuf);

    /* Parse format: "141:0,140:1" */
    /* Start with current GPIO status for partial updates */
    new_gpio_status = data->gpio_status;
    trangpio_dbg(data->dev, "Current GPIO status: 0x%x\n", data->gpio_status);

    gpio_config = kbuf;
    while ((token = strsep(&gpio_config, ",")) != NULL) {
        /* Skip empty tokens */
        if (strlen(token) == 0) {
            trangpio_dbg(data->dev, "Skipping empty token\n");
            continue;
        }

        gpio_count++;
        trangpio_dbg(data->dev, "Processing GPIO pair #%d: '%s'\n",
             gpio_count, token);

        /* Find colon separator */
        colon_pos = strchr(token, ':');
        if (!colon_pos) {
            dev_err(data->dev, "Invalid format: missing ':' in '%s'\n", token);
            return -EINVAL;
        }
        *colon_pos = '\0';

        /* Parse GPIO number */
        ret = kstrtoint(token, 10, &gpio_num);
        if (ret) {
            dev_err(data->dev, "Invalid GPIO number: '%s' (error: %d)\n",
            token, ret);
            return -EINVAL;
        }

        /* Parse GPIO state */
        ret = kstrtoint(colon_pos + 1, 10, &gpio_state);
        if (ret) {
            dev_err(data->dev, "Invalid GPIO state: '%s' (error: %d)\n",
            colon_pos + 1, ret);
            return -EINVAL;
        }

        /* Validate GPIO state */
        if (gpio_state != 0 && gpio_state != 1) {
            dev_err(data->dev, "Invalid GPIO state %d, must be 0 or 1\n",
            gpio_state);
            return -EINVAL;
        }

        /* Find GPIO index */
        gpio_index = find_gpio_index(gpio_num);
        if (gpio_index == -1) {
            dev_err(data->dev, "GPIO %d not found in mapping table\n",
            gpio_num);
            return -EINVAL;
        }

        trangpio_dbg(data->dev, "GPIO %d -> index %d, setting to %s\n",
                     gpio_num, gpio_index, gpio_state ? "HIGH" : "LOW");

        /* Update corresponding bit */
        if (gpio_state) {
            new_gpio_status |= (1 << gpio_index);
        } else {
            new_gpio_status &= ~(1 << gpio_index);
        }
        trangpio_dbg(data->dev, "Updated GPIO %d (index %d) to state %d, new status: 0x%x\n",
             gpio_num, gpio_index, gpio_state, new_gpio_status);
    }

    trangpio_dbg(data->dev, "Processed %d GPIO pairs\n", gpio_count);
    trangpio_dbg(data->dev, "Final GPIO status: 0x%x (was 0x%x)\n",
         new_gpio_status, data->gpio_status);

    if (mutex_lock_interruptible(&data->lock)) {
        dev_warn(data->dev, "Failed to acquire mutex (interrupted)\n");
        return -ERESTARTSYS;
    }

    if (!data->pctrl) {
        dev_err(data->dev, "pinctrl handle is NULL\n");
        mutex_unlock(&data->lock);
        return -ENODEV;
    }

    if (new_gpio_status != data->gpio_status) {
        trangpio_dbg(data->dev, "GPIO status changed from 0x%x to 0x%x, applying new configuration...\n",
             data->gpio_status, new_gpio_status);
        ret = trans_trangpio_multi_gpio_ctl(data, new_gpio_status);
        if (ret) {
            dev_err(data->dev, "Failed to apply GPIO configuration: %d\n", ret);
            mutex_unlock(&data->lock);
            return ret;
        }
        data->gpio_status = new_gpio_status;
        trangpio_dbg(data->dev, "GPIO configuration applied successfully\n");
    } else {
        trangpio_dbg(data->dev, "GPIO status unchanged, no hardware update needed\n");
    }

    mutex_unlock(&data->lock);
    trangpio_dbg(data->dev, "WRITE completed: %zu bytes processed\n", count);
    return count;
}
/* platform driver */
static int trans_trangpio_probe(struct platform_device *pdev)
{
    struct trans_trangpio_data *data;
    int ret;

    trangpio_dbg_pr("PROBE: device %s, compatible %s\n",
             dev_name(&pdev->dev), TRANGPIO_DTNAME);

    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data) {
        dev_err(&pdev->dev, "Failed to allocate memory for driver data\n");
        return -ENOMEM;
    }

    data->dev = &pdev->dev;
    mutex_init(&data->lock);

    ret = trans_trangpio_get_dts_info(&pdev->dev, data);
    if (ret) {
        dev_err(&pdev->dev, "Failed to get DTS/pinctrl info: %d\n", ret);
        return ret;
    }

    ret = alloc_chrdev_region(&data->devt, 0, 1, TRANGPIO_NAME);
    if (ret) {
        dev_err(&pdev->dev, "Failed alloc_chrdev_region: %d\n", ret);
        return ret;
    }

    cdev_init(&data->cdev, &trangpio_fops);
    data->cdev.owner = THIS_MODULE;

    ret = cdev_add(&data->cdev, data->devt, 1);
    if (ret) {
        dev_err(&pdev->dev, "Failed cdev_add: %d\n", ret);
        unregister_chrdev_region(data->devt, 1);
        return ret;
    }

    data->device = device_create(trangpio_class, &pdev->dev, data->devt, data,
                 TRANGPIO_NAME);
    if (IS_ERR(data->device)) {
        dev_err(&pdev->dev, "Failed device_create: %ld\n",
        PTR_ERR(data->device));
        cdev_del(&data->cdev);
        unregister_chrdev_region(data->devt, 1);
        return PTR_ERR(data->device);
    }

    platform_set_drvdata(pdev, data);

    data->gpio_status = 0;
    ret = trans_trangpio_multi_gpio_ctl(data, 0);
    if (ret) {
        dev_err(&pdev->dev, "Failed to initialize GPIO states: %d\n", ret);
        device_destroy(trangpio_class, data->devt);
        cdev_del(&data->cdev);
        unregister_chrdev_region(data->devt, 1);
        return ret;
    }

    trangpio_dbg_pr("PROBE completed: /dev/%s (major=%d, minor=%d), GPIO=0x%x\n",
             TRANGPIO_NAME, MAJOR(data->devt), MINOR(data->devt), data->gpio_status);

    dev_info(&pdev->dev, "TRANGPIO driver probe completed successfully\n");
    return 0;
}
static void trans_trangpio_remove(struct platform_device *pdev)
{
    struct trans_trangpio_data *data = platform_get_drvdata(pdev);

    trangpio_dbg_pr("REMOVE: cleaning up device %s\n", dev_name(&pdev->dev));

    if (!data) {
        dev_warn(&pdev->dev, "No driver data found during remove\n");
        return;
    }

    mutex_lock(&data->lock);

    trangpio_dbg_pr("GPIO cleanup: status=0x%x -> 0x0\n", data->gpio_status);
    if (data->pctrl) {
        trans_trangpio_multi_gpio_ctl(data, 0);
    } else {
        dev_warn(&pdev->dev, "pinctrl handle is NULL, skipping GPIO cleanup\n");
    }

    mutex_unlock(&data->lock);

    device_destroy(trangpio_class, data->devt);
    cdev_del(&data->cdev);
    unregister_chrdev_region(data->devt, 1);

    trangpio_dbg_pr("REMOVE completed\n");
}
static const struct of_device_id trans_trangpio_of_ids[] = {
    { .compatible = TRANGPIO_DTNAME, },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, trans_trangpio_of_ids);
static struct platform_driver trangpio_driver = {
    .probe = trans_trangpio_probe,
    .remove = trans_trangpio_remove,
    .driver = {
        .name = TRANGPIO_NAME,
        .owner = THIS_MODULE,
        .of_match_table = trans_trangpio_of_ids,
    },
};
static int __init trangpio_driver_init(void)
{
    int ret;

    trangpio_dbg_pr("MODULE INIT: %s v1.0, compatible: %s\n",
             TRANGPIO_NAME, TRANGPIO_DTNAME);

    trangpio_class = class_create(TRANGPIO_NAME);
    if (IS_ERR(trangpio_class)) {
        pr_err("Failed to create class: %ld\n", PTR_ERR(trangpio_class));
        return PTR_ERR(trangpio_class);
    }

    ret = platform_driver_register(&trangpio_driver);
    if (ret) {
        pr_err("Failed to register platform driver: %d\n", ret);
        class_destroy(trangpio_class);
        return ret;
    }

    trangpio_dbg_pr("MODULE INIT completed successfully\n");
    return 0;
}

static void __exit trangpio_driver_exit(void)
{
    trangpio_dbg_pr("MODULE EXIT: cleaning up %s\n", TRANGPIO_NAME);

    platform_driver_unregister(&trangpio_driver);
    class_destroy(trangpio_class);

    trangpio_dbg_pr("MODULE EXIT completed\n");
}
module_init(trangpio_driver_init);
module_exit(trangpio_driver_exit);
MODULE_DESCRIPTION("Refactored Transsion TRANGPIO Driver");
MODULE_LICENSE("GPL v2");
