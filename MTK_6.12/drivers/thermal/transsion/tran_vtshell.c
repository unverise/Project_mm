#include <linux/bits.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/timer.h>
#include "thermal_core.h"

#include "tran_vtshell.h"

struct vtshell_data *plat_vtshell_info;
EXPORT_SYMBOL(plat_vtshell_info);

static int __vtshell_get_temp(struct thermal_zone_device *tz, int *temp)
{
    struct vtshell_temp_tz *shell_tz = (struct vtshell_temp_tz *)tz->devdata;   //todo
    struct vtshell_data *shell_data = shell_tz->shell_data;
    struct vtshell_tz_param *shell_param = shell_data->params;
    struct thermal_zone_device *tzd;
    long long vtshell = 0, coef;
    int tz_temp, i, ret;
    char *sensor_name;

    if(shell_param[shell_tz->id].ref_num == 0) {
        *temp = THERMAL_TEMP_INVALID;
        return 0;
    }

    for(i = 0; i < shell_param[shell_tz->id].ref_num; i++) {
        sensor_name = shell_param[shell_tz->id].vtshell_ref[i].sensor_name;
        if(!sensor_name) {
            dev_err(shell_data->dev, "get sensor name fail %d\n", i);
            *temp = THERMAL_TEMP_INVALID;
            return -EINVAL;
        }

        //tzd = thermal_zone_get_zone_by_name(sensor_name);
        tzd = shell_param[shell_tz->id].tzd[i];
        if(IS_ERR_OR_NULL(tzd) || !tzd->ops.get_temp) {
            dev_err(shell_data->dev, "get %s temp fail\n", sensor_name);
            *temp = THERMAL_TEMP_INVALID;
            return -EINVAL;
        }

        if(strncmp(tzd->type, "vtshell", strlen("vtshell")) == 0)     //todo
            ret = __vtshell_get_temp(tzd, &tz_temp);
        else
            ret = tzd->ops.get_temp(tzd, &tz_temp);
        
        if(ret < 0) {
            dev_err(shell_data->dev, "%s get_temp fail %d\n", sensor_name, ret);
            *temp = THERMAL_TEMP_INVALID;
            return -EINVAL;
        }

        if(shell_param[shell_tz->id].operation == OPER_MAX) {
            if(i == 0)
                *temp = THERMAL_TEMP_INVALID;
            
            if(tz_temp > *temp)
                *temp = tz_temp;
            
        }else if(shell_param[shell_tz->id].operation == OPER_COEF) {
            coef = shell_param[shell_tz->id].vtshell_ref[i].sensor_coef;
            vtshell += tz_temp * coef;
            if(i == shell_param[shell_tz->id].ref_num - 1)
                *temp = (int)(vtshell / 100000000);
        }
    }

    return 0;
}

static int vtshell_get_temp(struct thermal_zone_device *tz, int *temp)
{
    struct vtshell_temp_tz *shell_tz = (struct vtshell_temp_tz *)tz->devdata;
    struct vtshell_data *shell_data = shell_tz->shell_data;
    int ret;

    mutex_lock(&shell_data->lock);
    ret = __vtshell_get_temp(tz, temp);
    mutex_unlock(&shell_data->lock);
    
    return ret;
}

static const struct thermal_zone_device_ops vtshell_ops = {
    .get_temp = vtshell_get_temp,
};

static ssize_t vtshell_info_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int len = 0, i, j;
    struct vtshell_tz_param *param;

    if(!plat_vtshell_info) {
        len += snprintf(buf + len, PAGE_SIZE - len, "vtshell info NULL\n");
        return len;
    }

    len += snprintf(buf + len, PAGE_SIZE - len, "vtshell sensor total num=%d\n", plat_vtshell_info->num_sensor);

    mutex_lock(&plat_vtshell_info->lock);     //todo
    for(i = 0; i < plat_vtshell_info->num_sensor; i++) {
        param = &plat_vtshell_info->params[i];
        len += snprintf(buf + len, PAGE_SIZE - len,
            "   vtshell id=%d, name=%s, ref_num=%d op=%d\n",
            i, param->tz_name, param->ref_num,param->operation);
        
        for(j = 0; j < param->ref_num; j++) {
            len += snprintf(buf + len, PAGE_SIZE - len,
                "      %20s, %10lld\n", param->vtshell_ref[j].sensor_name,
                param->vtshell_ref[j].sensor_coef);
        }
        len += snprintf(buf + len, PAGE_SIZE - len, "\n");
    }
    mutex_unlock(&plat_vtshell_info->lock);

    return len;
}

static ssize_t vtshell_info_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int len = 0, ref_num = 0, op;
    char shell_name[THERMAL_NAME_LENGTH + 1], ref_name[THERMAL_NAME_LENGTH + 1];
    long long ref_coef;
    unsigned int shell_id, i;
    struct thermal_zone_device *tzd[MAX_VTSHELL_REF_NUM];
    struct vtshell_tz_param *param;
    struct vtshell_coef coef[MAX_VTSHELL_REF_NUM];

    if(!plat_vtshell_info) {
		pr_info("plat_vtshell_info is NULL\n");
		return -ENODATA;
    }

	if (sscanf(buf, "%20s %d %d%n", shell_name, &op, &ref_num, &len) != 3) {
		pr_info("wrong shell_name, op, or ref_num %s\n", buf);
		return -EINVAL;
	}
	buf += len;

    for(shell_id = 0; shell_id < plat_vtshell_info->num_sensor; shell_id++) {
        if(!strcmp(shell_name, plat_vtshell_info->params[shell_id].tz_name))
            break;
    }

	if (shell_id >= plat_vtshell_info->num_sensor) {
		pr_info("shell name:%s not match any actual vtshell name\n", shell_name);
		return -EINVAL;
	}

	if (op < 0 || op >= OPER_NUM) {
		pr_info("wrong operation %d\n", op);
		return -EINVAL;
	}

	if (ref_num < 0 || ref_num > MAX_VTSHELL_REF_NUM) {
		pr_info("wrong ref number %d\n", ref_num);
		return -EINVAL;
	}

	memset(&coef[0], 0, sizeof(struct vtshell_coef) * MAX_VTSHELL_REF_NUM);
	memset(tzd, 0, sizeof(tzd));

	for (i = 0; i < ref_num; i++) {
		if (sscanf(buf, "%20s %lld%n", ref_name, &ref_coef, &len) != 2) {
			pr_info("wrong scan vtshell ref sensor:%s\n", buf);
			return -EINVAL;
		}
		buf += len;

		tzd[i] = thermal_zone_get_zone_by_name(ref_name);
		if (IS_ERR_OR_NULL(tzd[i])) {
			pr_info("get %s for thermal zone fail\n", ref_name);
			return -EINVAL;
		}

		strncpy(coef[i].sensor_name, ref_name, strlen(ref_name));
		coef[i].sensor_coef = ref_coef;
	}

	mutex_lock(&plat_vtshell_info->lock);
	param = &plat_vtshell_info->params[shell_id];
	param->operation = op;
	param->ref_num = (unsigned int)ref_num;
	memcpy(&param->vtshell_ref[0], &coef[0], sizeof(struct vtshell_coef) * MAX_VTSHELL_REF_NUM);
	for (i = 0; i < MAX_VTSHELL_REF_NUM; i++)
		param->tzd[i] = tzd[i];
	mutex_unlock(&plat_vtshell_info->lock);

	return count;
}

static struct kobj_attribute vtshell_info_attr = __ATTR_RW(vtshell_info);

static struct attribute *vtshell_attrs[] = {
    &vtshell_info_attr.attr,
    NULL
};

static struct attribute_group vtshell_attr_group = {
	.name	= "vtshell",
	.attrs	= vtshell_attrs,
};

static int vtshell_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct vtshell_temp_tz *shell_tz;
    struct vtshell_data *shell_data;
    struct thermal_zone_device *tzdev;
    int i, ret;

    if(!pdev->dev.of_node){
        dev_err(dev, "Only DT based supported\n");
        return -ENODEV;
    }

    shell_data = (struct vtshell_data *)of_device_get_match_data(dev);
    if(!shell_data) {
        dev_err(dev, "Error: Failed to get lvts platform data\n");
        return -ENODATA;
    }

    shell_data->dev = dev;
    platform_set_drvdata(pdev, shell_data);

    mutex_init(&shell_data->lock);

    for(i = 0; i < shell_data->num_sensor; i++) {
        shell_tz = devm_kzalloc(dev, sizeof(*shell_tz), GFP_KERNEL);
        if(!shell_tz)
            return -ENOMEM;
        
        shell_tz->id = i;
        shell_tz->shell_data = shell_data;

        tzdev = devm_thermal_of_zone_register(dev, shell_tz->id, shell_tz, &vtshell_ops);

        if(IS_ERR(tzdev)) {
            ret = PTR_ERR(tzdev);
            dev_err(dev, "Error: Failed to register shell_tz.id %d, ret = %d\n", shell_tz->id, ret);
            return ret;
        }

        ret = snprintf(shell_data->params[i].tz_name, THERMAL_NAME_LENGTH, tzdev->type);
        if(ret < 0)
            dev_notice(dev, "copy tz_name fail %s\n", tzdev->type);
    }

    plat_vtshell_info = shell_data;

    ret = sysfs_create_group(kernel_kobj, &vtshell_attr_group);
    if(ret) {
		dev_err(dev, "failed to create vtshell sysfs, ret=%d!\n", ret);
		return ret;
    }

    //pr_info("vtshell driver probe success\n");

    return 0;
}

static void vtshell_remove(struct platform_device *pdev)
{
    sysfs_remove_group(kernel_kobj, &vtshell_attr_group);
}

enum common_vtshell_sensor_enum {
	COMMON_VTSHELL_MAX,
	COMMON_VTSHELL_FRONT,
	COMMON_VTSHELL_BACK,
	COMMON_VTSHELL_NUM,
};

struct vtshell_tz_param common_vtshell_params[] = {
	[COMMON_VTSHELL_MAX] = {
		.ref_num = 0,
		.operation = OPER_MAX,
	},
	[COMMON_VTSHELL_FRONT] = {
		.ref_num = 0,
		.operation = OPER_COEF,
	},
	[COMMON_VTSHELL_BACK] = {
		.ref_num = 0,
		.operation = OPER_COEF,
	}
};

static struct vtshell_data common_vtshell_data = {
	.num_sensor = COMMON_VTSHELL_NUM,
	.params = common_vtshell_params,		//数组指针
};

static const struct of_device_id vtshell_of_match[] = {
	{
		.compatible = "mediatek,mt6789-virtual-tshell",
		.data = (void *)&common_vtshell_data,
	},
	{
		.compatible = "mediatek,mt6858-virtual-tshell",
		.data = (void *)&common_vtshell_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, vtshell_of_match);

static struct platform_driver vtshell_driver = {
	.probe = vtshell_probe,
    .remove = vtshell_remove,
	.driver = {
		.name = "tran-virtual-tshell",
		.of_match_table = vtshell_of_match,
	},
};

module_platform_driver(vtshell_driver);

MODULE_AUTHOR("Jun Zhou <jun.zhou@transsion.com>");
MODULE_DESCRIPTION("Transsion on virtual tshell driver");
MODULE_LICENSE("GPL v2");
