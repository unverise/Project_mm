#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/slab.h>

#include <tc_common_class.h>

#define CLASS_NAME "tran_thermal"
#define DEVICE_NAME "intf"

struct tran_intf_data {
    struct tran_device *tc_chg_dev;
    struct class *class;
    struct device *dev;
};

static struct tran_intf_data g_data;

static ssize_t charger_type_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
    int ret;
    union com_propval val = {0};

    if (IS_ERR_OR_NULL(g_data.tc_chg_dev))
        return sysfs_emit(buf, "unknown\n");

    ret = tran_dev_get_prop(g_data.tc_chg_dev,
            TRAN_PROP_TC_CHG_TYPE, &val);

    if (ret < 0)
        return sysfs_emit(buf, "unknown\n");

    return sysfs_emit(buf, "%d\n", val.intval);
}

static ssize_t charger_type_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count)
{
    return count;
}

static DEVICE_ATTR_RW(charger_type);

static struct attribute *thermal_attrs[] = {
    &dev_attr_charger_type.attr,
    NULL,
};

static const struct attribute_group thermal_attr_group = {
    .attrs = thermal_attrs,
};

static const struct attribute_group *thermal_attr_groups[] = {
    &thermal_attr_group,
    NULL,
};

static int __init thermal_interface_init(void)
{
    struct class *thermal_class;
    struct device *thermal_dev;

    thermal_class = class_create(CLASS_NAME);
    if (IS_ERR(thermal_class))
        return PTR_ERR(thermal_class);

    thermal_dev = device_create_with_groups(
            thermal_class,
            NULL,
            MKDEV(0, 0),
            NULL,
            thermal_attr_groups,
            DEVICE_NAME);

    if (IS_ERR(thermal_dev)) {
        class_destroy(thermal_class);
        return PTR_ERR(thermal_dev);
    }
    g_data.class = thermal_class;
    g_data.dev = thermal_dev;
    g_data.tc_chg_dev = tran_get_by_name("tc_charger");
    pr_info("thermal_interface: init success\n");
    return 0;
}

static void __exit thermal_interface_exit(void)
{
    device_destroy(g_data.class, MKDEV(0, 0));
    class_destroy(g_data.class);

    pr_info("thermal_interface: exit\n");
}

module_init(thermal_interface_init);
module_exit(thermal_interface_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("transsion low power group");
MODULE_DESCRIPTION("Thermal Interface using attribute groups");
