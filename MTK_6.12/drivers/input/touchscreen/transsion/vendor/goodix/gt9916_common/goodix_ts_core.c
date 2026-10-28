/*
 * Goodix Touchscreen Driver
 * Copyright (C) 2020 - 2021 Goodix, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif
//BSP:add for charger mode by linsheng.liu 20220810 start
#include <linux/power_supply.h>
//BSP:add for charger mode by linsheng.liu 20220810 end
#include "goodix_ts_core.h"
#define GOODIX_DEFAULT_CFG_NAME 	"goodix_cfg_group.cfg"
#define GOOIDX_INPUT_PHYS			"goodix_ts/input0"

#ifndef KEY_F26
#define KEY_F26 198
#endif

struct goodix_module goodix_modules;
int core_module_prob_sate = CORE_MODULE_UNPROBED;
/*BSP: add ACBHBWS-14 by  linsheng.liu 2022.0208 start*/
DEFINE_MUTEX(suppression_lock);
//static struct proc_dir_entry *brl_proc_edge_reject_switch_entry;
uint8_t g_tran_switch = 0;
/*BSP: add ACBHBWS-14 by  linsheng.liu 2022.0208 end*/
/*BSP: add for tp finger by linsheng.liu 20220125 start*/
static DEFINE_MUTEX(tran_fingerprint_status_lock);
static struct proc_dir_entry *fp_status_proc_entry;
int tran_fingerprint_status = 0;
/*BSP: add for tp finger by linsheng.liu 20220125 end*/
/* BSP: add ACBHBHW-2 for mp test by linsheng.liu 20220111 start */
static struct proc_dir_entry *NVT_proc_selftest_entry = NULL;
/* BSP: add ACBHBHW-2 for mp test by linsheng.liu 20220111 end */

/*BSP: add ACBHBWS-14 by linsheng.liu 20220125 start*/
static struct proc_dir_entry *brl_proc_report_rate_switch_entry;
uint8_t report_rate_switch = 0;
/*BSP: add ACBHBWS-14 by linsheng.liu 20220125 end*/
/* BSP: add ACBHBHW-2 for tp_vendor/bv show test by yun.luo 20220426 start */
u32 bin_config_id = 0;
/* BSP: add ACBHBHW-2 for tp_vendor/bv show test by yun.luo 20220426 end */
static int goodix_send_ic_config(struct goodix_ts_core *cd, int type);
/**
 * __do_register_ext_module - register external module
 * to register into touch core modules structure
 * return 0 on success, otherwise return < 0
 */
//BSP:add for charger mode by linsheng.liu 20220810 start
static int firmware_updata_flag = 0;
int brl_charger_detect(struct goodix_ts_core *cd, int type);
static int charger_in = 0;
struct notifier_block charger_nof;
extern int power_supply_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val);
static void goodix_charger_work(struct work_struct *work)
{
    struct goodix_ts_core *core_data = container_of(work, struct goodix_ts_core,
                                       charger_work);

    brl_charger_detect(core_data, charger_in);

}

static int mtk_power_misc_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
    int ret = 0;
    struct power_supply *psy = v;
    union power_supply_propval val;
    struct goodix_ts_core *core_data = goodix_modules.core_data;

    if (strcmp(psy->desc->name, "battery") == 0) {
        ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
        ts_debug("charger notifier: online=%d ret=%d", val.intval, ret);
        if (!ret) {
            if (1 == val.intval && 1 != charger_in) {
                ts_info("charger mode in");
                charger_in = 1;
                if (firmware_updata_flag && !atomic_read(&core_data->suspended))
                    schedule_work(&core_data->charger_work);
            } else if (2 == val.intval && 0 != charger_in) {
                ts_info("charger mode out");
                charger_in = 0;
                if (firmware_updata_flag && !atomic_read(&core_data->suspended))
                    schedule_work(&core_data->charger_work);
            }
        }
    }
    return 0;
}

extern int power_supply_reg_notifier(struct notifier_block *nb);
//BSP:add for charger mode by linsheng.liu 20220810 end
static int __do_register_ext_module(struct goodix_ext_module *module)
{
    struct goodix_ext_module *ext_module, *next;
    struct list_head *insert_point = &goodix_modules.head;

    /* prority level *must* be set */
    if (module->priority == EXTMOD_PRIO_RESERVED) {
        ts_err("Priority of module [%s] needs to be set",
               module->name);
        return -EINVAL;
    }
    mutex_lock(&goodix_modules.mutex);
    /* find insert point for the specified priority */
    if (!list_empty(&goodix_modules.head)) {
        list_for_each_entry_safe(ext_module, next,
                                 &goodix_modules.head, list) {
            if (ext_module == module) {
                ts_info("Module [%s] already exists",
                        module->name);
                mutex_unlock(&goodix_modules.mutex);
                return 0;
            }
        }

        /* smaller priority value with higher priority level */
        list_for_each_entry_safe(ext_module, next,
                                 &goodix_modules.head, list) {
            if (ext_module->priority >= module->priority) {
                insert_point = &ext_module->list;
                break;
            }
        }
    }

    if (module->funcs && module->funcs->init) {
        if (module->funcs->init(goodix_modules.core_data,
                                module) < 0) {
            ts_err("Module [%s] init error",
                   module->name ? module->name : " ");
            mutex_unlock(&goodix_modules.mutex);
            return -EFAULT;
        }
    }

    list_add(&module->list, insert_point->prev);
    mutex_unlock(&goodix_modules.mutex);

    ts_info("Module [%s] registered,priority:%u", module->name,
            module->priority);
    return 0;
}

static void goodix_register_ext_module_work(struct work_struct *work)
{
    struct goodix_ext_module *module =
        container_of(work, struct goodix_ext_module, work);

    ts_info("module register work IN");

    /* driver probe failed */
    if (core_module_prob_sate != CORE_MODULE_PROB_SUCCESS) {
        ts_err("Can't register ext_module core error");
        return;
    }

    if (__do_register_ext_module(module))
        ts_err("failed register module: %s", module->name);
    else
        ts_info("success register module: %s", module->name);
}

static void goodix_core_module_init(void)
{
    if (goodix_modules.initilized)
        return;
    goodix_modules.initilized = true;
    INIT_LIST_HEAD(&goodix_modules.head);
    mutex_init(&goodix_modules.mutex);
}

/**
 * goodix_register_ext_module - interface for register external module
 * to the core. This will create a workqueue to finish the real register
 * work and return immediately. The user need to check the final result
 * to make sure registe is success or fail.
 *
 * @module: pointer to external module to be register
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module(struct goodix_ext_module *module)
{
    if (IS_ERR_OR_NULL(module))
        return -EINVAL;

    ts_info("goodix_register_ext_module IN");

    goodix_core_module_init();
    INIT_WORK(&module->work, goodix_register_ext_module_work);
    schedule_work(&module->work);

    ts_info("goodix_register_ext_module OUT");
    return 0;
}

/**
 * goodix_register_ext_module_no_wait
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module_no_wait(struct goodix_ext_module *module)
{
    if (IS_ERR_OR_NULL(module))
        return -EINVAL;
    ts_info("goodix_register_ext_module_no_wait IN");
    goodix_core_module_init();
    /* driver probe failed */
    if (core_module_prob_sate != CORE_MODULE_PROB_SUCCESS) {
        ts_err("Can't register ext_module core error");
        return -EINVAL;
    }
    return __do_register_ext_module(module);
}

/**
 * goodix_unregister_ext_module - interface for external module
 * to unregister external modules
 *
 * @module: pointer to external module
 * return: 0 ok, <0 failed
 */
int goodix_unregister_ext_module(struct goodix_ext_module *module)
{
    struct goodix_ext_module *ext_module, *next;
    bool found = false;

    if (IS_ERR_OR_NULL(module))
        return -EINVAL;

    if (!goodix_modules.initilized)
        return -EINVAL;

    if (IS_ERR_OR_NULL(goodix_modules.core_data))
        return -ENODEV;

    mutex_lock(&goodix_modules.mutex);
    if (!list_empty(&goodix_modules.head)) {
        list_for_each_entry_safe(ext_module, next,
                                 &goodix_modules.head, list) {
            if (ext_module == module) {
                found = true;
                break;
            }
        }
    } else {
        mutex_unlock(&goodix_modules.mutex);
        return 0;
    }

    if (!found) {
        ts_debug("Module [%s] never registed",
                 module->name);
        mutex_unlock(&goodix_modules.mutex);
        return 0;
    }

    list_del(&module->list);
    mutex_unlock(&goodix_modules.mutex);

    if (module->funcs && module->funcs->exit)
        module->funcs->exit(goodix_modules.core_data, module);

    ts_info("Moudle [%s] unregistered",
            module->name ? module->name : " ");
    return 0;
}

static void goodix_ext_sysfs_release(struct kobject *kobj)
{
    ts_info("Kobject released!");
}

#define to_ext_module(kobj)	container_of(kobj,\
				struct goodix_ext_module, kobj)
#define to_ext_attr(attr)	container_of(attr,\
				struct goodix_ext_attribute, attr)

static ssize_t goodix_ext_sysfs_show(struct kobject *kobj,
                                     struct attribute *attr, char *buf)
{
    struct goodix_ext_module *module = to_ext_module(kobj);
    struct goodix_ext_attribute *ext_attr = to_ext_attr(attr);

    if (ext_attr->show)
        return ext_attr->show(module, buf);

    return -EIO;
}

static ssize_t goodix_ext_sysfs_store(struct kobject *kobj,
                                      struct attribute *attr, const char *buf, size_t count)
{
    struct goodix_ext_module *module = to_ext_module(kobj);
    struct goodix_ext_attribute *ext_attr = to_ext_attr(attr);

    if (ext_attr->store)
        return ext_attr->store(module, buf, count);

    return -EIO;
}

static const struct sysfs_ops goodix_ext_ops = {
    .show = goodix_ext_sysfs_show,
    .store = goodix_ext_sysfs_store
};

static struct kobj_type goodix_ext_ktype = {
    .release = goodix_ext_sysfs_release,
    .sysfs_ops = &goodix_ext_ops,
};

struct kobj_type *goodix_get_default_ktype(void)
{
    return &goodix_ext_ktype;
}

struct kobject *goodix_get_default_kobj(void)
{
    struct kobject *kobj = NULL;

    if (goodix_modules.core_data &&
        goodix_modules.core_data->pdev)
        kobj = &goodix_modules.core_data->pdev->dev.kobj;
    return kobj;
}

/* show driver infomation */
static ssize_t goodix_ts_driver_info_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "DriverVersion:%s\n",
                    GOODIX_DRIVER_VERSION);
}

/* show chip infoamtion */
static ssize_t goodix_ts_chip_info_show(struct device  *dev,
                                        struct device_attribute *attr, char *buf)
{
    struct goodix_ts_core *core_data = dev_get_drvdata(dev);
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
    struct goodix_fw_version chip_ver;
    u8 temp_pid[8] = {0};
    int ret;
    int cnt = -EINVAL;

    if (hw_ops->read_version) {
        ret = hw_ops->read_version(core_data, &chip_ver);
        if (!ret) {
            memcpy(temp_pid, chip_ver.rom_pid, sizeof(chip_ver.rom_pid));
            cnt = snprintf(&buf[0], PAGE_SIZE,
                           "rom_pid:%s\nrom_vid:%02x%02x%02x\n",
                           temp_pid, chip_ver.rom_vid[0],
                           chip_ver.rom_vid[1], chip_ver.rom_vid[2]);
            cnt += snprintf(&buf[cnt], PAGE_SIZE,
                            "patch_pid:%s\npatch_vid:%02x%02x%02x%02x\n",
                            chip_ver.patch_pid, chip_ver.patch_vid[0],
                            chip_ver.patch_vid[1], chip_ver.patch_vid[2],
                            chip_ver.patch_vid[3]);
            cnt += snprintf(&buf[cnt], PAGE_SIZE,
                            "sensorid:%d\n", chip_ver.sensor_id);
        }
    }

    if (hw_ops->get_ic_info) {
        ret = hw_ops->get_ic_info(core_data, &core_data->ic_info);
        if (!ret) {
            cnt += snprintf(&buf[cnt], PAGE_SIZE,
                            "config_id:%x\n", core_data->ic_info.version.config_id);
            cnt += snprintf(&buf[cnt], PAGE_SIZE,
                            "config_version:%x\n", core_data->ic_info.version.config_version);
        }
    }

    return cnt;
}

/* reset chip */
static ssize_t goodix_ts_reset_store(struct device *dev,
                                     struct device_attribute *attr,
                                     const char *buf,
                                     size_t count)
{
    struct goodix_ts_core *core_data = dev_get_drvdata(dev);
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;

    if (IS_ERR_OR_NULL(buf) || count <= 0)
        return -EINVAL;
    if (buf[0] != '0')
        hw_ops->reset(core_data, GOODIX_NORMAL_RESET_DELAY_MS);
    return count;
}

/* read config */
static ssize_t goodix_ts_read_cfg_show(struct device *dev,
                                       struct device_attribute *attr,
                                       char *buf)
{
    struct goodix_ts_core *core_data = dev_get_drvdata(dev);
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
    int ret;
    int i;
    int offset;
    char *cfg_buf = NULL;

    cfg_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
    if (IS_ERR_OR_NULL(cfg_buf))
        return -ENOMEM;

    if (hw_ops->read_config)
        ret = hw_ops->read_config(core_data, cfg_buf, PAGE_SIZE);
    else
        ret = -EINVAL;

    if (ret > 0) {
        offset = 0;
        for (i = 0; i < 200; i++) { // only print 200 bytes
            offset += snprintf(&buf[offset], PAGE_SIZE - offset,
                               "%02x,", cfg_buf[i]);
            if ((i + 1) % 20 == 0)
                buf[offset++] = '\n';
        }
    }

    kfree(cfg_buf);
    if (ret <= 0)
        return ret;

    return offset;
}

static u8 ascii2hex(u8 a)
{
    s8 value = 0;

    if (a >= '0' && a <= '9')
        value = a - '0';
    else if (a >= 'A' && a <= 'F')
        value = a - 'A' + 0x0A;
    else if (a >= 'a' && a <= 'f')
        value = a - 'a' + 0x0A;
    else
        value = 0xff;

    return value;
}

static int goodix_ts_convert_0x_data(const u8 *buf, int buf_size,
                                     u8 *out_buf, int *out_buf_len)
{
    int i, m_size = 0;
    int temp_index = 0;
    u8 high, low;

    for (i = 0; i < buf_size; i++) {
        if (buf[i] == 'x' || buf[i] == 'X')
            m_size++;
    }

    if (m_size <= 1) {
        ts_err("cfg file ERROR, valid data count:%d", m_size);
        return -EINVAL;
    }
    *out_buf_len = m_size;

    for (i = 0; i < buf_size; i++) {
        if (buf[i] != 'x' && buf[i] != 'X')
            continue;

        if (temp_index >= m_size) {
            ts_err("exchange cfg data error, overflow,"
                   "temp_index:%d,m_size:%d",
                   temp_index, m_size);
            return -EINVAL;
        }
        high = ascii2hex(buf[i + 1]);
        low = ascii2hex(buf[i + 2]);
        if (high == 0xff || low == 0xff) {
            ts_err("failed convert: 0x%x, 0x%x",
                   buf[i + 1], buf[i + 2]);
            return -EINVAL;
        }
        out_buf[temp_index++] = (high << 4) + low;
    }
    return 0;
}

/* send config */
static ssize_t goodix_ts_send_cfg_store(struct device *dev,
                                        struct device_attribute *attr,
                                        const char *buf, size_t count)
{
    struct goodix_ts_core *core_data = dev_get_drvdata(dev);
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
    struct goodix_ic_config *config = NULL;
    const struct firmware *cfg_img = NULL;
    int en;
    int ret;

    if (sscanf(buf, "%d", &en) != 1)
        return -EINVAL;

    if (en != 1)
        return -EINVAL;

    hw_ops->irq_enable(core_data, false);

    ret = request_firmware(&cfg_img, GOODIX_DEFAULT_CFG_NAME, dev);
    if (ret < 0) {
        ts_err("cfg file [%s] not available,errno:%d",
               GOODIX_DEFAULT_CFG_NAME, ret);
        goto exit;
    } else {
        ts_info("cfg file [%s] is ready", GOODIX_DEFAULT_CFG_NAME);
    }

    config = kzalloc(sizeof(*config), GFP_KERNEL);
    if (IS_ERR_OR_NULL(config))
        goto exit;

    if (goodix_ts_convert_0x_data(cfg_img->data, cfg_img->size,
                                  config->data, &config->len)) {
        ts_err("convert config data FAILED");
        goto exit;
    }

    if (hw_ops->send_config) {
        ret = hw_ops->send_config(core_data, config->data, config->len);
        if (ret < 0)
            ts_err("send config failed");
    }

exit:
    hw_ops->irq_enable(core_data, true);
    kfree(config);
    if (cfg_img)
        release_firmware(cfg_img);

    return count;
}

/* reg read/write */
static u32 rw_addr;
static u32 rw_len;
static u8 rw_flag;
static u8 store_buf[32];
static u8 show_buf[PAGE_SIZE];
static ssize_t goodix_ts_reg_rw_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    struct goodix_ts_core *core_data = dev_get_drvdata(dev);
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
    int ret;

    if (!rw_addr || !rw_len) {
        ts_err("address(0x%x) and length(%d) can't be null",
               rw_addr, rw_len);
        return -EINVAL;
    }

    if (rw_flag != 1) {
        ts_err("invalid rw flag %d, only support [1/2]", rw_flag);
        return -EINVAL;
    }

    ret = hw_ops->read(core_data, rw_addr, show_buf, rw_len);
    if (ret < 0) {
        ts_err("failed read addr(%x) length(%d)", rw_addr, rw_len);
        return snprintf(buf, PAGE_SIZE, "failed read addr(%x), len(%d)\n",
                        rw_addr, rw_len);
    }

    return snprintf(buf, PAGE_SIZE, "0x%x,%d {%*ph}\n",
                    rw_addr, rw_len, rw_len, show_buf);
}

static ssize_t goodix_ts_reg_rw_store(struct device *dev,
                                      struct device_attribute *attr,
                                      const char *buf, size_t count)
{
    struct goodix_ts_core *core_data = dev_get_drvdata(dev);
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
    char *pos = NULL;
    char *token = NULL;
    long result = 0;
    int ret;
    int i;

    if (IS_ERR_OR_NULL(buf) || !count) {
        ts_err("invalid parame");
        goto err_out;
    }

    if (buf[0] == 'r') {
        rw_flag = 1;
    } else if (buf[0] == 'w') {
        rw_flag = 2;
    } else {
        ts_err("string must start with 'r/w'");
        goto err_out;
    }

    /* get addr */
    pos = (char *)buf;
    pos += 2;
    token = strsep(&pos, ":");
    if (IS_ERR_OR_NULL(token)) {
        ts_err("invalid address info");
        goto err_out;
    } else {
        if (kstrtol(token, 16, &result)) {
            ts_err("failed get addr info");
            goto err_out;
        }
        rw_addr = (u32)result;
        ts_info("rw addr is 0x%x", rw_addr);
    }

    /* get length */
    token = strsep(&pos, ":");
    if (IS_ERR_OR_NULL(token)) {
        ts_err("invalid length info");
        goto err_out;
    } else {
        if (kstrtol(token, 0, &result)) {
            ts_err("failed get length info");
            goto err_out;
        }
        rw_len = (u32)result;
        ts_info("rw length info is %d", rw_len);
        if (rw_len > sizeof(store_buf)) {
            ts_err("data len > %lu", sizeof(store_buf));
            goto err_out;
        }
    }

    if (rw_flag == 1)
        return count;

    for (i = 0; i < rw_len; i++) {
        token = strsep(&pos, ":");
        if (IS_ERR_OR_NULL(token)) {
            ts_err("invalid data info");
            goto err_out;
        } else {
            if (kstrtol(token, 16, &result)) {
                ts_err("failed get data[%d] info", i);
                goto err_out;
            }
            store_buf[i] = (u8)result;
            ts_info("get data[%d]=0x%x", i, store_buf[i]);
        }
    }
    ret = hw_ops->write(core_data, rw_addr, store_buf, rw_len);
    if (ret < 0) {
        ts_err("failed write addr(%x) data %*ph", rw_addr,
               rw_len, store_buf);
        goto err_out;
    }

    ts_info("%s write to addr (%x) with data %*ph",
            "success", rw_addr, rw_len, store_buf);

    return count;
err_out:
    snprintf(show_buf, PAGE_SIZE, "%s\n",
             "invalid params, format{r/w:4100:length:[41:21:31]}");
    return -EINVAL;

}

/* show irq infomation */
static ssize_t goodix_ts_irq_info_show(struct device *dev,
                                       struct device_attribute *attr,
                                       char *buf)
{
    struct goodix_ts_core *core_data = dev_get_drvdata(dev);
    struct irq_desc *desc;
    size_t offset = 0;
    int r;

    r = snprintf(&buf[offset], PAGE_SIZE, "irq:%u\n", core_data->irq);
    if (r < 0)
        return -EINVAL;

    offset += r;
    r = snprintf(&buf[offset], PAGE_SIZE - offset, "state:%s\n",
                 atomic_read(&core_data->irq_enabled) ?
                 "enabled" : "disabled");
    if (r < 0)
        return -EINVAL;

    desc = irq_to_desc(core_data->irq);
    offset += r;
    r = snprintf(&buf[offset], PAGE_SIZE - offset, "disable-depth:%d\n",
                 desc->depth);
    if (r < 0)
        return -EINVAL;

    offset += r;
    r = snprintf(&buf[offset], PAGE_SIZE - offset, "trigger-count:%zu\n",
                 core_data->irq_trig_cnt);
    if (r < 0)
        return -EINVAL;

    offset += r;
    r = snprintf(&buf[offset], PAGE_SIZE - offset,
                 "echo 0/1 > irq_info to disable/enable irq\n");
    if (r < 0)
        return -EINVAL;

    offset += r;
    return offset;
}

/* enable/disable irq */
static ssize_t goodix_ts_irq_info_store(struct device *dev,
                                        struct device_attribute *attr,
                                        const char *buf, size_t count)
{
    struct goodix_ts_core *core_data = dev_get_drvdata(dev);
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;

    if (IS_ERR_OR_NULL(buf) || count <= 0)
        return -EINVAL;

    if (buf[0] != '0')
        hw_ops->irq_enable(core_data, true);
    else
        hw_ops->irq_enable(core_data, false);
    return count;
}

/* show esd status */
static ssize_t goodix_ts_esd_info_show(struct device *dev,
                                       struct device_attribute *attr,
                                       char *buf)
{
    struct goodix_ts_core *core_data = dev_get_drvdata(dev);
    struct goodix_ts_esd *ts_esd = &core_data->ts_esd;
    int r = 0;

    r = snprintf(buf, PAGE_SIZE, "state:%s\n",
                 atomic_read(&ts_esd->esd_on) ?
                 "enabled" : "disabled");

    return r;
}

/* enable/disable esd */
static ssize_t goodix_ts_esd_info_store(struct device *dev,
                                        struct device_attribute *attr,
                                        const char *buf, size_t count)
{
    if (IS_ERR_OR_NULL(buf) || count <= 0)
        return -EINVAL;

    if (buf[0] != '0')
        goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
    else
        goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
    return count;
}

/* debug level show */
static ssize_t goodix_ts_debug_log_show(struct device *dev,
                                        struct device_attribute *attr,
                                        char *buf)
{
    int r = 0;

    r = snprintf(buf, PAGE_SIZE, "state:%s\n",
                 debug_log_flag ?
                 "enabled" : "disabled");

    return r;
}

/* debug level store */
static ssize_t goodix_ts_debug_log_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    if (IS_ERR_OR_NULL(buf) || count <= 0)
        return -EINVAL;

    if (buf[0] != '0')
        debug_log_flag = true;
    else
        debug_log_flag = false;
    return count;
}

static DEVICE_ATTR(driver_info, 0444, goodix_ts_driver_info_show, NULL);
static DEVICE_ATTR(chip_info, 0444, goodix_ts_chip_info_show, NULL);
static DEVICE_ATTR(reset, 0220, NULL, goodix_ts_reset_store);
static DEVICE_ATTR(send_cfg, 0220, NULL, goodix_ts_send_cfg_store);
static DEVICE_ATTR(read_cfg, 0444, goodix_ts_read_cfg_show, NULL);
static DEVICE_ATTR(reg_rw, 0664, goodix_ts_reg_rw_show, goodix_ts_reg_rw_store);
static DEVICE_ATTR(irq_info, 0664, goodix_ts_irq_info_show, goodix_ts_irq_info_store);
static DEVICE_ATTR(esd_info, 0664, goodix_ts_esd_info_show, goodix_ts_esd_info_store);
static DEVICE_ATTR(debug_log, 0664, goodix_ts_debug_log_show, goodix_ts_debug_log_store);

static struct attribute *sysfs_attrs[] = {
    &dev_attr_driver_info.attr,
    &dev_attr_chip_info.attr,
    &dev_attr_reset.attr,
    &dev_attr_send_cfg.attr,
    &dev_attr_read_cfg.attr,
    &dev_attr_reg_rw.attr,
    &dev_attr_irq_info.attr,
    &dev_attr_esd_info.attr,
    &dev_attr_debug_log.attr,
    NULL,
};

static const struct attribute_group sysfs_group = {
    .attrs = sysfs_attrs,
};

static int goodix_ts_sysfs_init(struct goodix_ts_core *core_data)
{
    int ret;

    ret = sysfs_create_group(&core_data->pdev->dev.kobj, &sysfs_group);
    if (ret) {
        ts_err("failed create core sysfs group");
        return ret;
    }

    return ret;
}

static void goodix_ts_sysfs_exit(struct goodix_ts_core *core_data)
{
    sysfs_remove_group(&core_data->pdev->dev.kobj, &sysfs_group);
}

/* prosfs create */
static int rawdata_proc_show(struct seq_file *m, void *v)
{
    struct ts_rawdata_info *info;
    struct goodix_ts_core *cd = m->private;
    int tx;
    int rx;
    int ret;
    int i;
    int index;

    if (IS_ERR_OR_NULL(m) || IS_ERR_OR_NULL(v) || IS_ERR_OR_NULL(cd))
        return -EIO;

    info = kzalloc(sizeof(*info), GFP_KERNEL);
    if (IS_ERR_OR_NULL(info))
        return -ENOMEM;

    ret = cd->hw_ops->get_capacitance_data(cd, info);
    if (ret < 0) {
        ts_err("failed to get_capacitance_data, exit!");
        goto exit;
    }

    rx = info->buff[0];
    tx = info->buff[1];
    seq_printf(m, "TX:%d  RX:%d\n", tx, rx);
    seq_puts(m, "mutual_rawdata:\n");
    index = 2;
    for (i = 0; i < tx * rx; i++) {
        seq_printf(m, "%5d,", info->buff[index + i]);
        if ((i + 1) % tx == 0)
            seq_puts(m, "\n");
    }
    seq_puts(m, "mutual_diffdata:\n");
    index += tx * rx;
    for (i = 0; i < tx * rx; i++) {
        seq_printf(m, "%3d,", info->buff[index + i]);
        if ((i + 1) % tx == 0)
            seq_puts(m, "\n");
    }

exit:
    kfree(info);
    return ret;
}

static int rawdata_proc_open(struct inode *inode, struct file *file)
{
    return single_open_size(file, rawdata_proc_show, pde_data(inode), PAGE_SIZE * 10);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations rawdata_proc_fops = {
    .open = rawdata_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#else
static const struct proc_ops rawdata_proc_fops = {
    .proc_open = rawdata_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#endif

/*USD: add for pocket anti accidental touch by longtao.ran 20240605 start*/
int gts_pocket_mode_switch(int type)
{
    int ret = 0;
    u8 mode_open[7] = {0x00,0x00,0x05,0x70,0x01,0x76,0x00};
    u8 mode_close[7] = {0x00,0x00,0x05,0x70,0x00,0x75,0x00};
    struct goodix_ts_core *core_data = goodix_modules.core_data;

    if(IS_ERR_OR_NULL(core_data) || IS_ERR_OR_NULL(core_data->hw_ops) || IS_ERR_OR_NULL(core_data->hw_ops->write)){
         ts_err("pointer is null or error");
         return -EINVAL;
    }
    if(type==1)
    {
        ts_info("pocket mode open");
        ret = core_data->hw_ops->write(core_data, 0x10174, mode_open, 7);
    }else {
        ts_info("pocket mode close");
        ret = core_data->hw_ops->write(core_data, 0x10174, mode_close, 7);
    }

    if(ret < 0){
        ts_err("write pocket mode switch fail");
    }
    return ret;
}

int gts_pocket_state_report(bool palm)
{
    uint8_t tran_pocket_state;
    struct goodix_ts_core *core_data = goodix_modules.core_data;
    struct input_dev *input_dev;

    if(IS_ERR_OR_NULL(core_data)){
         ts_err("pointer is null or error");
         return -EINVAL;
    }
    input_dev = core_data->input_dev;
    tran_pocket_state = goodix_controller.tran_ts_data->tran_pocket_state;
    if (core_data->board_data.pocket_func_support && tran_pocket_state) {
         if(!atomic_read(&core_data->suspended) && palm && core_data->palmflag){
             input_report_key(input_dev, KEY_LARGE_AREA, 1);
             input_sync(input_dev);
            core_data->palmflag = false;
             ts_info("Large area touch dowm");
          }else if((palm == 0) && (!core_data->palmflag)){
           input_report_key(input_dev, KEY_LARGE_AREA, 0);
             input_sync(input_dev);
            ts_info("Large area touch up");
             core_data->palmflag = true;
          }
        return 1;
    }
     //Palm in, and fingerprint unlocking to enter the system
    core_data->palmflag = true;

    return 0;
}
/*USD: add for pocket anti accidental touch by longtao.ran 20240605 end*/

//BSP:add for charger mode by linsheng.liu 20220810 start
#define GOODIX_CHARGER_DETECT_CMD 	0xAF
int brl_charger_detect(struct goodix_ts_core *cd, int type)
{
    struct goodix_ts_cmd charger_cmd;

    charger_cmd.cmd = GOODIX_CHARGER_DETECT_CMD;
    charger_cmd.len = 5;
    charger_cmd.data[0] = type;
    if (type) {
        charger_cmd.data[1] = 0xB5;
        charger_cmd.data[2] = 0x00;
    } else {
        charger_cmd.data[1] = 0xB4;
        charger_cmd.data[2] = 0x00;
    }
    if (cd->hw_ops->send_cmd(cd, &charger_cmd))
        ts_err("failed send charger_cmd");

    return 0;
}
//BSP:add for charger mode by linsheng.liu 20220810 end
/*BSP: add for tp finger by linsheng.liu 20220125 start*/
#define GOODIX_AREA_SWITCH_CMD 	0x30
int brl_area_switch(struct goodix_ts_core *cd, int type)
{
    struct goodix_ts_cmd edge_cmd;

    edge_cmd.cmd = GOODIX_AREA_SWITCH_CMD;
    edge_cmd.len = 5;
    edge_cmd.data[0] = type;
    if (type) {
        edge_cmd.data[1] = 0x36;
        edge_cmd.data[2] = 0x00;
    } else {
        edge_cmd.data[1] = 0x35;
        edge_cmd.data[2] = 0x00;
    }
    if (cd->hw_ops->send_cmd(cd, &edge_cmd))
        ts_err("failed send edge_restrain cmd");

    return 0;
}
static ssize_t fingerprint_status_read(struct file *filp, char *buf, size_t count, loff_t *off)
{
    return 0;
}

static ssize_t fingerprint_status_write(struct file *filp, const char *buf, size_t count, loff_t *off)
{
    char buff[2] = {'0', '\0'};
    int ret = -1;
    struct goodix_ts_core *core_data = goodix_modules.core_data;

    ret = copy_from_user(buff, buf, 1);
    if (ret) {
        ts_err("copy_from_user failed");
        return -EPERM;
    }

    mutex_lock(&tran_fingerprint_status_lock);

    /* 2 means enroll
     * 1 means unlock
     * 0 means close
     */
    if (buff[0] == '2') {
        brl_area_switch(core_data, 1);
        tran_fingerprint_status = 2;
        ts_info("tran_fingerprint_status = %d", tran_fingerprint_status);
    } else if (buff[0] == '1') {
        brl_area_switch(core_data, 0);
        tran_fingerprint_status = 1;
        ts_info("tran_fingerprint_status = %d", tran_fingerprint_status);
    } else if (buff[0] == '0') {
        tran_fingerprint_status = 0;
        ts_info("tran_fingerprint_status = %d", tran_fingerprint_status);
    } else {
        ts_err("fingerprint status error input");
    }
    mutex_unlock(&tran_fingerprint_status_lock);

    return count;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
struct file_operations fingerprint_status_fops = {
    .read = fingerprint_status_read,
    .write = fingerprint_status_write,
};
#else
struct proc_ops fingerprint_status_fops = {
    //.owner = THIS_MODULE,
    .proc_read = fingerprint_status_read,
    .proc_write = fingerprint_status_write,
};
#endif

/*BSP: add for tp finger by linsheng.liu 20220125 end*/
#if 1
/* BSP: add ACBHBHW-2 for mp test by linsheng.liu 20220111 start */
static ssize_t gtp_selftest_read(struct file *filp, char *buf, size_t count, loff_t *off)
{
    int ret = 0;
    char s_test_pass[] = "Short Test PASS";
    char o_test_pass[] = "Open Test PASS";
    char s_test_fail[] = "Short Test FAIL";
    char o_test_fail[] = "Open Test FAIL";
    int len = -1;
    char *page = NULL;
    char *ptr = NULL;
    static int finished = 0;

    struct goodix_ts_core *core_data = goodix_modules.core_data;
    struct ts_rawdata_info *info = NULL;

    info = kzalloc(sizeof(*info), GFP_KERNEL);
    if (IS_ERR_OR_NULL(info)) {
        ts_err("Failed to alloc rawdata info memory");
        return -ENOMEM;
    }

    if (finished) {
        ts_info("read END");
        finished = 0;
        kfree(info);
        return 0;
    }
    finished = 1;

    if (goodix_do_inspect(core_data, info)) {
        ts_err("Factory_test FAIL");
        goto exit;
    }

    page = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (IS_ERR_OR_NULL(page)) {
        kfree(info);
        return -ENOMEM;
    }
    ptr = page;

    //ret = snprintf(buf, PAGE_SIZE, "resultInfo: %s", info->result);
    if (strstr(info->result, "1P") && strstr(info->result, "5P")) {
        ptr += sprintf(ptr, "%s\n%s\n", s_test_pass, o_test_pass);
    } else {
        ptr += sprintf(ptr, "%s\n%s\n", s_test_fail, o_test_fail);
    }

    len = ptr - page;
    if (*off >= len) {
        kfree(info);
        kfree(page);
        return 0;
    }
    ret = copy_to_user(buf, (char *)page, len);
    *off += len;
    if (ret) {
        kfree(info);
        kfree(page);
        return ret;
    }
    kfree(info);
    kfree(page);
    return len;

exit:
    kfree(info);
    return ret;
}

static ssize_t gtp_selftest_write(struct file *filp, const char *buf, size_t count, loff_t *off)
{
    return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
struct file_operations gtp_selftest_fops = {
    //.owner = THIS_MODULE,
    .read = gtp_selftest_read,
    .write = gtp_selftest_write,
};
#else
struct proc_ops gtp_selftest_fops = {
    //.owner = THIS_MODULE,
    .proc_read = gtp_selftest_read,
    .proc_write = gtp_selftest_write,
};
#endif

/* BSP: add ACBHBHW-2 for mp test by linsheng.liu 20220111 end */
#endif

//BSP:midify for test data by linsheng.liu 20220526 start
char *selftest_data = NULL;
bool g_selftest_data_need = false;
int g_csv_length = 0;

static int selftest_proc_show(struct seq_file *seq, void *v)
{
    int data_size = 0;
    data_size = strlen(selftest_data);
    ts_info("cp total size=%d", data_size);
    seq_printf(seq, "%s\n", selftest_data);
    if (seq->size >= data_size) {
        ts_info("cp fac test data end free test buf");
        vfree(selftest_data);
        selftest_data = NULL;
        g_csv_length = 0;
        g_selftest_data_need = false;
    }
    return 0;
}

#define CSV_BUFFER_LEN (1024*80*5)
static int selftest_proc_open(struct inode *inode, struct file *file)
{
    struct ts_rawdata_info *info;
    struct goodix_ts_core *cd = goodix_modules.core_data;
    selftest_data = vmalloc(CSV_BUFFER_LEN);
    if (IS_ERR_OR_NULL(selftest_data)) {
        ts_info("csv_buffer malloc fail\n");
        return 0;
    }

    info = kzalloc(sizeof(*info), GFP_KERNEL);
    g_selftest_data_need = true;
    goodix_do_inspect(cd, info);

    kfree(info);
    return  single_open(file, &selftest_proc_show, NULL);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
struct file_operations tran_selftest_fops = {
    .open = selftest_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#else
struct proc_ops tran_selftest_fops = {
    .proc_open = selftest_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#endif

//BSP:midify for test data by linsheng.liu 20220526 end

/*BSP: add ACBHBWS-14 by linsheng.liu 20220125 start*/
static ssize_t brl_report_rate_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;

    page = kmalloc(PAGE_SIZE, GFP_KERNEL);
    ptr = page;
    ptr += sprintf(page, "%d\n", report_rate_switch);
    len = ptr - page;
    if (*f_pos >= len) {
        kfree(page);
        return 0;
    }
    if (copy_to_user(buf, (char *)page, len)) {
        kfree(page);
        return len;
    }
    *f_pos += len;
    kfree(page);
    return len;
}
/*BSP: add for tp 360hz/240hz rata switch linsheng.liu 20220125 start*/
#define GOODIX_360HZ_SWITCH_CMD 	0xC0
int brl_360hz_switch(struct goodix_ts_core *cd, int type)
{
    struct goodix_ts_cmd rate_cmd;

    rate_cmd.cmd = GOODIX_360HZ_SWITCH_CMD;
    rate_cmd.len = 5;
    rate_cmd.data[0] = type;
    if (type) {
        rate_cmd.data[1] = 0xC6;
        rate_cmd.data[2] = 0x00;
    } else {
        rate_cmd.data[1] = 0xC5;
        rate_cmd.data[2] = 0x00;
    }
    if (cd->hw_ops->send_cmd(cd, &rate_cmd))
        ts_err("failed send 360hz rate_cmd cmd");

    return 0;
}

#define GOODIX_240HZ_SWITCH_CMD 	0x9D
int brl_240hz_switch(struct goodix_ts_core *cd)
{
    struct goodix_ts_cmd rate_cmd;

    rate_cmd.cmd = GOODIX_240HZ_SWITCH_CMD;
    rate_cmd.len = 5;
    rate_cmd.data[0] = 0x02;
    rate_cmd.data[1] = 0xA4;
    rate_cmd.data[2] = 0x00;
    if (cd->hw_ops->send_cmd(cd, &rate_cmd))
        ts_err("failed send 240hz rate_cmd cmd");

    return 0;
}
static ssize_t brl_report_rate_switch_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    char buff[2] = {'0', '\0'};
    struct goodix_ts_core *core_data = goodix_modules.core_data;
    if (count == 0 || count > 2) {
        ts_err("Invalid value! count = %zu", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff, buf, 1);
    if (ret) {
        ts_err("copy_from_user fail.");
        return -EPERM;
    }
    ts_debug("transsion report_rate_switch buff: %c", buff[0]);
    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        ts_err("Invalid value! ret = %d", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp > 1) {
        ts_err("Invalid value! tmp = %d", tmp);
        ret = -EINVAL;
        goto out;
    }

    report_rate_switch = (uint8_t)tmp;
    ts_info("transsion report_rate_switch %d", report_rate_switch);
    if (report_rate_switch == 1) {
        brl_360hz_switch(core_data, 1);
    } else {
        brl_360hz_switch(core_data, 0);
        brl_240hz_switch(core_data);
    }

    ret = count;
out:
    return ret;
}
/*BSP: add for tp 360hz/240hz rata switch linsheng.liu 20220125 end*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations brl_report_rate_switch_fops = {
    //.owner = THIS_MODULE,
    .read = brl_report_rate_switch_proc_read,
    .write = brl_report_rate_switch_proc_write,
};
#else
static const struct proc_ops brl_report_rate_switch_fops = {
    //.owner = THIS_MODULE,
    .proc_read = brl_report_rate_switch_proc_read,
    .proc_write = brl_report_rate_switch_proc_write,
};
#endif

/*BSP: add ACBHBWS-14 by linsheng.liu 20220125 end*/
/*BSP: add ACBHBWS-14 by  linsheng.liu 2022.0208 start*/
#define GOODIX_EDGE_RESTRAIN_IN_CMD 	0x17
#define GOODIX_EDGE_RESTRAIN_OUT_CMD	0x17
int brl_edge_restrain_in(struct goodix_ts_core *cd, int type)
{
    struct goodix_ts_cmd edge_cmd;

    edge_cmd.cmd = GOODIX_EDGE_RESTRAIN_IN_CMD;
    edge_cmd.len = 6;
    switch (type) {
    case 1:
        edge_cmd.data[0] = 0x40;
        edge_cmd.data[1] = 0x80;
        edge_cmd.data[2] = 0xDD;
        edge_cmd.data[3] = 0x00;
        break;
    case 2:
        edge_cmd.data[0] = 0x80;
        edge_cmd.data[1] = 0x80;
        edge_cmd.data[2] = 0x1D;
        edge_cmd.data[3] = 0x01;
        break;
    default:
        break;
    }
    if (cd->hw_ops->send_cmd(cd, &edge_cmd))
        ts_err("failed send edge_restrain cmd");

    return 0;
}

int brl_edge_restrain_out(struct goodix_ts_core *cd)
{
    struct goodix_ts_cmd edge_cmd;

    edge_cmd.cmd = GOODIX_EDGE_RESTRAIN_OUT_CMD;
    edge_cmd.len = 6;
    edge_cmd.data[0] = 0x00;
    edge_cmd.data[1] = 0x00;
    edge_cmd.data[2] = 0x1D;
    edge_cmd.data[3] = 0x00;
    if (cd->hw_ops->send_cmd(cd, &edge_cmd))
        ts_err("failed send edge_restrain cmd");

    return 0;
}

#if 0
static ssize_t brl_edge_reject_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    static int finished = 0;
    int32_t len = 0;
    //uint8_t edge_reject_switch;
    char *page = NULL;
    char *ptr = NULL;

    ts_info("++");

    /*
    * We return 0 to indicate end of file, that we have
    * no more information. Otherwise, processes will
    * continue to read from us in an endless loop.
    */
    if (finished) {
        ts_info("read END");
        finished = 0;
        return 0;
    }
    finished = 1;

    page = kmalloc(PAGE_SIZE, GFP_KERNEL);
    ptr = page;
    ptr += sprintf(page, "%d\n", g_tran_switch);
    len = ptr - page;
    if (*f_pos >= len) {
        kfree(page);
        return 0;
    }
    if (copy_to_user(buf, (char *)page, len)) {
        kfree(page);
        return len;
    }
    *f_pos += len;
    kfree(page);
    ts_info("--");
    return len;
}

static ssize_t brl_edge_reject_switch_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    uint8_t edge_reject_switch;
    char buff[2] = {'0', '\0'};
    struct goodix_ts_core *core_data = goodix_modules.core_data;

    ts_info("++");

    if (count == 0 || count > 2) {
        ts_err("Invalid value! count = %zu\n", count);
        ret = -EINVAL;
        goto out;
    }

    ret = copy_from_user(buff, buf, 1);
    if (ret) {
        ts_err("copy_from_user fail.");
        return -EPERM;
    }

    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        ts_err("Invalid value! ret = %d", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp > 4) {
        ts_err("Invalid value! tmp = %d", tmp);
        ret = -EINVAL;
        goto out;
    }

    edge_reject_switch = (uint8_t)tmp;
    ts_info("edge_reject_switch = %d", edge_reject_switch);

    if (mutex_lock_interruptible(&suppression_lock)) {
        return -ERESTARTSYS;
    }

    g_tran_switch = edge_reject_switch;

    if (g_tran_switch == 1) {
        brl_edge_restrain_in(core_data, 1);
    } else if (g_tran_switch == 3) {
        brl_edge_restrain_in(core_data, 2);
    } else {
        brl_edge_restrain_out(core_data);
    }

    mutex_unlock(&suppression_lock);

    ret = count;
out:
    ts_info("--");
    return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations brl_edge_reject_switch_fops = {
    //.owner = THIS_MODULE,
    .read = brl_edge_reject_switch_proc_read,
    .write = brl_edge_reject_switch_proc_write,
};
#else
static const struct proc_ops brl_edge_reject_switch_fops = {
    //.owner = THIS_MODULE,
    .proc_read = brl_edge_reject_switch_proc_read,
    .proc_write = brl_edge_reject_switch_proc_write,
};
#endif

/*BSP: add ACBHBWS-14 by  linsheng.liu 2022.0208 end*/
#endif
static void goodix_ts_procfs_init(struct goodix_ts_core *core_data)
{
    if (!proc_mkdir("goodix_ts", NULL))
        return;
    proc_create_data("goodix_ts/tp_capacitance_data",
                     0666, NULL, &rawdata_proc_fops, core_data);
    //BSP:midify for test data by linsheng.liu 20220526 start
    proc_create_data("goodix_data_selftest",
                     0666, NULL, &tran_selftest_fops, core_data);
    //BSP:midify for test data by linsheng.liu 20220526 end
    /* BSP: add ACBHBHW-2 for mp test by linsheng.liu 20220111 start */
    /* BSP: add ACBHBHW-2 for mp test by linsheng.liu 20220111 start */
    NVT_proc_selftest_entry = proc_create("goodix_selftest", 0444, NULL, &gtp_selftest_fops);
    if (NVT_proc_selftest_entry == NULL) {
        ts_err("create /proc/goodix_selftest Failed!");
        return;
    }
    /* BSP: add ACBHBHW-2 for mp test by linsheng.liu 20220111 end */

    /*BSP: add ACBHBWS-14 by linsheng.liu 20220125 start*/
    brl_proc_report_rate_switch_entry = proc_create("report_rate_switch", 0666, NULL, &brl_report_rate_switch_fops);
    if (brl_proc_report_rate_switch_entry == NULL) {
        ts_err("create proc/%s Failed!", "report_rate_switch");
    } else {
        ts_info("create proc/%s Succeeded!", "report_rate_switch");
    }
    /*BSP: add ACBHBWS-14 by linsheng.liu 20220125 end*/
    /*BSP: add for tp finger by linsheng.liu 20220125 start*/
    fp_status_proc_entry = proc_create("fingerprint_status", 0666, NULL, &fingerprint_status_fops);
    if (fp_status_proc_entry == NULL) {
        ts_err("create proc/fingerprint_status Failed!");
    } else {
        ts_info("create proc/fingerprint_status Succeeded!");
    }
    /*BSP: add for tp finger by linsheng.liu 20220125 end*/
    /*BSP: add ACBHBWS-14 by  linsheng.liu 2022.0208 start*/
    /*brl_proc_edge_reject_switch_entry = proc_create("border_suppression", 0666, NULL,&brl_edge_reject_switch_fops);
    if (brl_proc_edge_reject_switch_entry == NULL) {
    	ts_err("create proc/%s Failed!", "border_suppression");
    } else {
    	ts_info("create proc/%s Succeeded!", "border_suppression");
    }*/
    /*BSP: add ACBHBWS-14 by  linsheng.liu 2022.0208 end*/
}

static void goodix_ts_procfs_exit(struct goodix_ts_core *core_data)
{
    remove_proc_entry("goodix_ts/tp_capacitance_data", NULL);
    remove_proc_entry("goodix_ts", NULL);
}

/* event notifier */
static BLOCKING_NOTIFIER_HEAD(ts_notifier_list);
/**
 * goodix_ts_register_client - register a client notifier
 * @nb: notifier block to callback on events
 *  see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_register_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_register(&ts_notifier_list, nb);
}

/**
 * goodix_ts_unregister_client - unregister a client notifier
 * @nb: notifier block to callback on events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_unregister_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_unregister(&ts_notifier_list, nb);
}

/**
 * fb_notifier_call_chain - notify clients of fb_events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_blocking_notify(enum ts_notify_event evt, void *v)
{
    int ret;

    ret = blocking_notifier_call_chain(&ts_notifier_list,
                                       (unsigned long)evt, v);
    return ret;
}

#ifdef CONFIG_OF
/**
 * goodix_parse_dt_resolution - parse resolution from dt
 * @node: devicetree node
 * @board_data: pointer to board data structure
 * return: 0 - no error, <0 error
 */
static int goodix_parse_dt_resolution(struct device_node *node,
                                      struct goodix_ts_board_data *board_data)
{
    int ret;

    ret = of_property_read_u32(node, "goodix,panel-max-x",
                               &board_data->panel_max_x);
    if (ret) {
        ts_err("failed get panel-max-x");
        return ret;
    }

    ret = of_property_read_u32(node, "goodix,panel-max-y",
                               &board_data->panel_max_y);
    if (ret) {
        ts_err("failed get panel-max-y");
        return ret;
    }

    ret = of_property_read_u32(node, "goodix,panel-max-w",
                               &board_data->panel_max_w);
    if (ret) {
        ts_err("failed get panel-max-w");
        return ret;
    }

    ret = of_property_read_u32(node, "goodix,panel-max-p",
                               &board_data->panel_max_p);
    if (ret) {
        ts_err("failed get panel-max-p, use default");
        board_data->panel_max_p = GOODIX_PEN_MAX_PRESSURE;
    }

    return 0;
}

/**
 * goodix_parse_dt - parse board data from dt
 * @dev: pointer to device
 * @board_data: pointer to board data structure
 * return: 0 - no error, <0 error
 */
static int goodix_parse_dt(struct device_node *node,
                           struct goodix_ts_board_data *board_data)
{
    const char *name_tmp;
    int r;

    if (IS_ERR_OR_NULL(board_data)) {
        ts_err("invalid board data");
        return -EINVAL;
    }

    r = of_get_named_gpio(node, "goodix,avdd-gpio", 0);
    if (r < 0) {
        ts_info("can't find avdd-gpio, use other power supply");
        board_data->avdd_gpio = 0;
    } else {
        ts_info("get avdd-gpio[%d] from dt", r);
        board_data->avdd_gpio = r;
    }

    r = of_get_named_gpio(node, "goodix,iovdd-gpio", 0);
    if (r < 0) {
        ts_info("can't find iovdd-gpio, use other power supply");
        board_data->iovdd_gpio = 0;
    } else {
        ts_info("get iovdd-gpio[%d] from dt", r);
        board_data->iovdd_gpio = r;
    }

    r = of_get_named_gpio(node, "goodix,reset-gpio", 0);
    if (r < 0) {
        ts_err("invalid reset-gpio in dt: %d", r);
        return -EINVAL;
    }
    ts_info("get reset-gpio[%d] from dt", r);
    board_data->reset_gpio = r;

    r = of_get_named_gpio(node, "goodix,irq-gpio", 0);
    if (r < 0) {
        ts_err("invalid irq-gpio in dt: %d", r);
        return -EINVAL;
    }
    ts_info("get irq-gpio[%d] from dt", r);
    board_data->irq_gpio = r;

    r = of_property_read_u32(node, "goodix,irq-flags",
                             &board_data->irq_flags);
    if (r) {
        ts_err("invalid irq-flags");
        return -EINVAL;
    }

    memset(board_data->avdd_name, 0, sizeof(board_data->avdd_name));
    r = of_property_read_string(node, "goodix,avdd-name", &name_tmp);
    if (!r) {
        ts_info("avdd name from dt: %s", name_tmp);
        if (strlen(name_tmp) < sizeof(board_data->avdd_name))
            strncpy(board_data->avdd_name,
                    name_tmp, sizeof(board_data->avdd_name));
        else
            ts_info("invalied avdd name length: %ld > %ld",
                    strlen(name_tmp),
                    sizeof(board_data->avdd_name));
    }

    memset(board_data->iovdd_name, 0, sizeof(board_data->iovdd_name));
    r = of_property_read_string(node, "goodix,iovdd-name", &name_tmp);
    if (!r) {
        ts_info("iovdd name from dt: %s", name_tmp);
        if (strlen(name_tmp) < sizeof(board_data->iovdd_name))
            strncpy(board_data->iovdd_name,
                    name_tmp, sizeof(board_data->iovdd_name));
        else
            ts_info("invalied iovdd name length: %ld > %ld",
                    strlen(name_tmp),
                    sizeof(board_data->iovdd_name));
    }

    /* get firmware file name */
    r = of_property_read_string(node, "goodix,firmware-name", &name_tmp);
    if (!r) {
        ts_info("firmware name from dt: %s", name_tmp);
        strncpy(board_data->fw_name, name_tmp, sizeof(board_data->fw_name));
    } else {
        ts_info("can't find firmware name, use default: %s", TS_DEFAULT_FIRMWARE);
        strncpy(board_data->fw_name, TS_DEFAULT_FIRMWARE, sizeof(board_data->fw_name));
    }
    /*USD: add for pocket anti accidental touch by longtao.ran 20240605 start*/
    board_data->pocket_func_support = of_property_read_bool(node, "goodix,pocket-function-support");
    if (board_data->pocket_func_support){
        board_data->pocket_func_support = true;
        ts_info("pocket_func_support enabled");
    }else{
        board_data->pocket_func_support = false;
    }
    /*USD: add for pocket anti accidental touch by longtao.ran 20240605 end*/
    /* get config file name */
    r = of_property_read_string(node, "goodix,config-name", &name_tmp);
    if (!r) {
        ts_info("config name from dt: %s", name_tmp);
        strncpy(board_data->cfg_bin_name, name_tmp, sizeof(board_data->cfg_bin_name));
    } else {
        ts_info("can't find config name, use default: %s", TS_DEFAULT_CFG_BIN);
        strncpy(board_data->cfg_bin_name, TS_DEFAULT_CFG_BIN, sizeof(board_data->cfg_bin_name));
    }

    /* get xyz resolutions */
    r = goodix_parse_dt_resolution(node, board_data);
    if (r) {
        ts_err("Failed to parse resolutions:%d", r);
        return r;
    }

    /*get pen-enable switch and pen keys, must after "key map"*/
    board_data->pen_enable = of_property_read_bool(node,
                             "goodix,pen-enable");
    if (board_data->pen_enable)
        ts_info("goodix pen enabled");
    //BSP: add for custom mistouch by linsheng.liu 20231019 start
    board_data->custom_mistouch_support = of_property_read_bool(node, "goodix,custom-mistouch-support");
    if (board_data->custom_mistouch_support) {
        ts_info("custom_mistouch_support enabled");
    }
    //BSP: add for custom mistouch by linsheng.liu 20231019 end
    ts_debug("[DT]x:%d, y:%d, w:%d, p:%d", board_data->panel_max_x,
             board_data->panel_max_y, board_data->panel_max_w,
             board_data->panel_max_p);
    return 0;
}
#endif

static void goodix_ts_report_pen(struct input_dev *dev,
                                 struct goodix_pen_data *pen_data)
{
    int i;

    mutex_lock(&dev->mutex);

    if (pen_data->coords.status == TS_TOUCH) {
        input_report_key(dev, BTN_TOUCH, 1);
        input_report_key(dev, pen_data->coords.tool_type, 1);
        input_report_abs(dev, ABS_X, pen_data->coords.x);
        input_report_abs(dev, ABS_Y, pen_data->coords.y);
        input_report_abs(dev, ABS_PRESSURE, pen_data->coords.p);
        input_report_abs(dev, ABS_TILT_X, pen_data->coords.tilt_x);
        input_report_abs(dev, ABS_TILT_Y, pen_data->coords.tilt_y);
        ts_debug("pen_data:x %d, y %d, p %d, tilt_x %d tilt_y %d key[%d %d]",
                 pen_data->coords.x, pen_data->coords.y,
                 pen_data->coords.p, pen_data->coords.tilt_x,
                 pen_data->coords.tilt_y, pen_data->keys[0].status == TS_TOUCH ? 1 : 0,
                 pen_data->keys[1].status == TS_TOUCH ? 1 : 0);
    } else {
        input_report_key(dev, BTN_TOUCH, 0);
        input_report_key(dev, pen_data->coords.tool_type, 0);
    }
    /* report pen button */
    for (i = 0; i < GOODIX_MAX_PEN_KEY; i++) {
        if (pen_data->keys[i].status == TS_TOUCH)
            input_report_key(dev, pen_data->keys[i].code, 1);
        else
            input_report_key(dev, pen_data->keys[i].code, 0);
    }

    input_sync(dev);
    mutex_unlock(&dev->mutex);
}

static void goodix_ts_report_finger(struct input_dev *dev,
                                    struct goodix_touch_data *touch_data)
{
    unsigned int touch_num = touch_data->touch_num;
    int i;
    //BSP: add touch down info by linsheng.liu 20220218 start
    static int touch_down = 0;
    static int x = 0;
    static int y = 0;
    //BSP: add touch down info by linsheng.liu 20220218 end
    //BSP: add for factory mode by hongwu.zhan 20230913 start
    int boot_mode = 0;
    int resolution_multiples = goodix_controller.supplier->resolution_multiples;
    boot_mode = goodix_controller.tran_ts_data->boot_mode;
    //BSP: add for factory mode by hongwu.zhan 20230913 end
    mutex_lock(&dev->mutex);

    for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
        if (touch_data->coords[i].status == TS_TOUCH) {

            x = touch_data->coords[i].x;
            y = touch_data->coords[i].y;
            //BSP: add for factory mode by hongwu.zhan 20230913 start
            if (boot_mode == TRAN_FACTORY_MODE) {
                touch_data->coords[i].x = touch_data->coords[i].x / resolution_multiples;
                touch_data->coords[i].y = touch_data->coords[i].y / resolution_multiples;
                x = touch_data->coords[i].x;
                y = touch_data->coords[i].y;
            } else {
                x = x / resolution_multiples;
                y = y / resolution_multiples;
            }
            if (!touch_down) {
                ts_info("[res:%d] touch down (input_x,input_y)=(%d, %d)",
                        resolution_multiples, x, y);
                touch_down++;
            }
            //BSP: add for factory mode by hongwu.zhan 20230913 end
            ts_debug("report: id %d, x %d, y %d, w %d", i,
                     touch_data->coords[i].x, touch_data->coords[i].y,
                     touch_data->coords[i].w);
            input_mt_slot(dev, i);
            input_mt_report_slot_state(dev, MT_TOOL_FINGER, true);
            input_report_abs(dev, ABS_MT_POSITION_X,
                             touch_data->coords[i].x);
            input_report_abs(dev, ABS_MT_POSITION_Y,
                             touch_data->coords[i].y);
            input_report_abs(dev, ABS_MT_TOUCH_MAJOR,
                             touch_data->coords[i].w);
        } else {
            input_mt_slot(dev, i);
            input_mt_report_slot_state(dev, MT_TOOL_FINGER, false);
        }
    }
    //BSP: add touch up info by linsheng.liu 20220218 start
    if (touch_num == 0) {
        if (touch_down)
            ts_info("[res:%d] touch up (input_x,input_y)=(%d, %d)", resolution_multiples, x, y);
        touch_down = 0;
    }
    //BSP: add touch up info by linsheng.liu 20220218 end
    input_report_key(dev, BTN_TOUCH, touch_num > 0 ? 1 : 0);
    input_sync(dev);

    mutex_unlock(&dev->mutex);
}

static int goodix_ts_request_handle(struct goodix_ts_core *cd,
                                    struct goodix_ts_event *ts_event)
{
    struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
    int ret = -1;

    if (ts_event->request_code == REQUEST_TYPE_CONFIG)
        ret = goodix_send_ic_config(cd, CONFIG_TYPE_NORMAL);
    else if (ts_event->request_code == REQUEST_TYPE_RESET)
        ret = hw_ops->reset(cd, GOODIX_NORMAL_RESET_DELAY_MS);
    else
        ts_info("can not handle request type 0x%x",
                ts_event->request_code);
    if (ret)
        ts_err("failed handle request 0x%x",
               ts_event->request_code);
    else
        ts_info("success handle ic request 0x%x",
                ts_event->request_code);
    return ret;
}

/**
 * goodix_ts_threadirq_func - Bottom half of interrupt
 * This functions is excuted in thread context,
 * sleep in this function is permit.
 *
 * @data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static irqreturn_t goodix_ts_threadirq_func(int irq, void *data)
{
    struct goodix_ts_core *core_data = data;
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
    struct goodix_ext_module *ext_module, *next;
    struct goodix_ts_event *ts_event = &core_data->ts_event;
    struct goodix_ts_esd *ts_esd = &core_data->ts_esd;
    int ret;

    ts_esd->irq_status = true;
    core_data->irq_trig_cnt++;
    //BSP: add ADJHBSY-592 for wakeup lock by yun.luo 20220527 start
    if (atomic_read(&core_data->suspended)) {
        pm_wakeup_event(&core_data->input_dev->dev, 2000);
    }
    //BSP: add ADJHBSY-592 for wakeup lock by yun.luo 20220527 end
    /* inform external module */
    mutex_lock(&goodix_modules.mutex);
    list_for_each_entry_safe(ext_module, next,
                             &goodix_modules.head, list) {
        if (IS_ERR_OR_NULL(ext_module->funcs->irq_event))
            continue;
        ret = ext_module->funcs->irq_event(core_data, ext_module);
        if (ret == EVT_CANCEL_IRQEVT) {
            mutex_unlock(&goodix_modules.mutex);
            return IRQ_HANDLED;
        }
    }
    mutex_unlock(&goodix_modules.mutex);

    /* read touch data from touch device */
    ret = hw_ops->event_handler(core_data, ts_event);
    if (likely(!ret)) {
        if (ts_event->event_type == EVENT_TOUCH) {
            /* report touch */
            goodix_ts_report_finger(core_data->input_dev,
                                    &ts_event->touch_data);
        }
        if (core_data->board_data.pen_enable &&
            ts_event->event_type == EVENT_PEN) {
            goodix_ts_report_pen(core_data->pen_dev,
                                 &ts_event->pen_data);
        }
        if (ts_event->event_type == EVENT_REQUEST) {
            goodix_ts_request_handle(core_data, ts_event);
        }
    }

    if (!core_data->tools_ctrl_sync && !ts_event->retry)
        hw_ops->after_event_handler(core_data);
    ts_event->retry = 0;

    return IRQ_HANDLED;
}

/**
 * goodix_ts_init_irq - Requset interrput line from system
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_irq_setup(struct goodix_ts_core *core_data)
{
    const struct goodix_ts_board_data *ts_bdata = board_data(core_data);
    int ret;

    /* if ts_bdata-> irq is invalid */
    core_data->irq = gpio_to_irq(ts_bdata->irq_gpio);
    if (core_data->irq < 0) {
        ts_err("failed get irq num %d", core_data->irq);
        return -EINVAL;
    }

    ts_info("IRQ:%u,flags:%d", core_data->irq, (int)ts_bdata->irq_flags);
    ret = devm_request_threaded_irq(&core_data->pdev->dev,
                                    core_data->irq, NULL,
                                    goodix_ts_threadirq_func,
                                    ts_bdata->irq_flags | IRQF_ONESHOT,
                                    GOODIX_CORE_DRIVER_NAME,
                                    core_data);
    if (ret < 0)
        ts_err("Failed to requeset threaded irq:%d", ret);
    else
        atomic_set(&core_data->irq_enabled, 1);

    return ret;
}

/**
 * goodix_ts_power_init - Get regulator for touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_power_init(struct goodix_ts_core *core_data)
{
    struct goodix_ts_board_data *ts_bdata = board_data(core_data);
    struct device *dev = core_data->bus->dev;
    int ret = 0;

    ts_info("Power init");
    if (strlen(ts_bdata->avdd_name)) {
        core_data->avdd = devm_regulator_get(dev,
                                             ts_bdata->avdd_name);
        if (IS_ERR_OR_NULL(core_data->avdd)) {
            ret = PTR_ERR(core_data->avdd);
            ts_err("Failed to get regulator avdd:%d", ret);
            core_data->avdd = NULL;
            return ret;
        }
        /* BSP:add AD9 tp_drv ADJHBSY-1 by yun.luo 20220415 start */
        ret = regulator_set_voltage(core_data->avdd, 3300000, 3300000);
        if (ret != 0) {
            ts_err("[POWER]Failed to set voltage of regulator,ret=%d!", ret);
        }
        /* BSP:add AD9 tp_drv ADJHBSY-1 by yun.luo 20220415 end */
    } else {
        ts_info("Avdd name is NULL");
    }

    if (strlen(ts_bdata->iovdd_name)) {
        core_data->iovdd = devm_regulator_get(dev,
                                              ts_bdata->iovdd_name);
        if (IS_ERR_OR_NULL(core_data->iovdd)) {
            ret = PTR_ERR(core_data->iovdd);
            ts_err("Failed to get regulator iovdd:%d", ret);
            core_data->iovdd = NULL;
        }
    } else {
        ts_info("iovdd name is NULL");
    }

    return ret;
}

/**
 * goodix_ts_power_on - Turn on power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_on(struct goodix_ts_core *cd)
{
    int ret = 0;

    ts_info("Device power on");
    if (cd->power_on)
        return 0;

    ret = cd->hw_ops->power_on(cd, true);
    if (!ret)
        cd->power_on = 1;
    else
        ts_err("failed power on, %d", ret);
    return ret;
}

/**
 * goodix_ts_power_off - Turn off power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_off(struct goodix_ts_core *cd)
{
    int ret;

    ts_info("Device power off");
    if (!cd->power_on)
        return 0;

    ret = cd->hw_ops->power_on(cd, false);
    if (!ret)
        cd->power_on = 0;
    else
        ts_err("failed power off, %d", ret);

    return ret;
}

/**
 * goodix_ts_gpio_setup - Request gpio resources from GPIO subsysten
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_gpio_setup(struct goodix_ts_core *core_data)
{
    struct goodix_ts_board_data *ts_bdata = board_data(core_data);
    int r = 0;

    ts_info("GPIO setup,reset-gpio:%d, irq-gpio:%d",
            ts_bdata->reset_gpio, ts_bdata->irq_gpio);
    /*
     * after kenerl3.13, gpio_ api is deprecated, new
     * driver should use gpiod_ api.
     */
    r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->reset_gpio,
                              GPIOF_OUT_INIT_LOW, "ts_reset_gpio");
    if (r < 0) {
        ts_err("Failed to request reset gpio, r:%d", r);
        return r;
    }

    r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->irq_gpio,
                              GPIOF_IN, "ts_irq_gpio");
    if (r < 0) {
        ts_err("Failed to request irq gpio, r:%d", r);
        return r;
    }

    if (ts_bdata->avdd_gpio > 0) {
        r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->avdd_gpio,
                                  GPIOF_OUT_INIT_LOW, "ts_avdd_gpio");
        if (r < 0) {
            ts_err("Failed to request avdd-gpio, r:%d", r);
            return r;
        }
    }

    if (ts_bdata->iovdd_gpio > 0) {
        r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->iovdd_gpio,
                                  GPIOF_OUT_INIT_LOW, "ts_iovdd_gpio");
        if (r < 0) {
            ts_err("Failed to request iovdd-gpio, r:%d", r);
            return r;
        }
    }

    return 0;
}

/**
 * goodix_ts_input_dev_config - Requset and config a input device
 *  then register it to input sybsystem.
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_input_dev_config(struct goodix_ts_core *core_data)
{
    struct goodix_ts_board_data *ts_bdata = board_data(core_data);
    struct input_dev *input_dev = NULL;
    int r;

    input_dev = input_allocate_device();
    if (IS_ERR_OR_NULL(input_dev)) {
        ts_err("Failed to allocated input device");
        return -ENOMEM;
    }

    core_data->input_dev = input_dev;
    input_set_drvdata(input_dev, core_data);

    input_dev->name = "mtk-tpd";
    input_dev->phys = GOOIDX_INPUT_PHYS;
    input_dev->id.product = 0xDEAD;
    input_dev->id.vendor = 0xBEEF;
    input_dev->id.version = 10427;

    __set_bit(EV_SYN, input_dev->evbit);
    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(EV_ABS, input_dev->evbit);
    __set_bit(BTN_TOUCH, input_dev->keybit);
    __set_bit(BTN_TOOL_FINGER, input_dev->keybit);

#ifdef INPUT_PROP_DIRECT
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
#endif

    /* set input parameters */
    input_set_abs_params(input_dev, ABS_MT_POSITION_X,
                         0, ts_bdata->panel_max_x, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
                         0, ts_bdata->panel_max_y, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
                         0, ts_bdata->panel_max_w, 0, 0);
#ifdef INPUT_TYPE_B_PROTOCOL
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 7, 0)
    input_mt_init_slots(input_dev, GOODIX_MAX_TOUCH,
                        INPUT_MT_DIRECT);
#else
    input_mt_init_slots(input_dev, GOODIX_MAX_TOUCH);
#endif
#endif

    input_set_capability(input_dev, EV_KEY, KEY_POWER);
    //BSP: add gesture function for ad9 by yun.luo 20220418 start
    input_set_capability(input_dev, EV_KEY, KEY_F26);
    //BSP: add gesture function for ad9 by yun.luo 20220418 end
    input_set_capability(input_dev, EV_KEY, KEY_LARGE_AREA);
    /*BSP: add for tp finger by linsheng.liu 20220125 start*/
    input_set_capability(input_dev, EV_KEY, KEY_F20);
    input_set_capability(input_dev, EV_KEY, KEY_FINGER);
    /*BSP: add for tp finger by linsheng.liu 20220125 end*/
    r = input_register_device(input_dev);
    if (r < 0) {
        ts_err("Unable to register input device");
        input_free_device(input_dev);
        return r;
    }
    //BSP: add by hongwu.zhan for gesture 20230831 start
    goodix_controller.input_dev = input_dev;
    //BSP:add by hongwu.zhan for gesture 20230831 end

    return 0;
}

static int goodix_ts_pen_dev_config(struct goodix_ts_core *core_data)
{
    struct goodix_ts_board_data *ts_bdata = board_data(core_data);
    struct input_dev *pen_dev = NULL;
    int r;

    pen_dev = input_allocate_device();
    if (IS_ERR_OR_NULL(pen_dev)) {
        ts_err("Failed to allocated pen device");
        return -ENOMEM;
    }

    core_data->pen_dev = pen_dev;
    input_set_drvdata(pen_dev, core_data);

    pen_dev->name = GOODIX_PEN_DRIVER_NAME;
    pen_dev->id.product = 0xDEAD;
    pen_dev->id.vendor = 0xBEEF;
    pen_dev->id.version = 10427;

    pen_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    __set_bit(ABS_X, pen_dev->absbit);
    __set_bit(ABS_Y, pen_dev->absbit);
    __set_bit(ABS_TILT_X, pen_dev->absbit);
    __set_bit(ABS_TILT_Y, pen_dev->absbit);
    __set_bit(BTN_STYLUS, pen_dev->keybit);
    __set_bit(BTN_STYLUS2, pen_dev->keybit);
    __set_bit(BTN_TOUCH, pen_dev->keybit);
    __set_bit(BTN_TOOL_PEN, pen_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, pen_dev->propbit);
    input_set_abs_params(pen_dev, ABS_X, 0, ts_bdata->panel_max_x, 0, 0);
    input_set_abs_params(pen_dev, ABS_Y, 0, ts_bdata->panel_max_y, 0, 0);
    input_set_abs_params(pen_dev, ABS_PRESSURE, 0,
                         ts_bdata->panel_max_p, 0, 0);
    input_set_abs_params(pen_dev, ABS_TILT_X,
                         -GOODIX_PEN_MAX_TILT, GOODIX_PEN_MAX_TILT, 0, 0);
    input_set_abs_params(pen_dev, ABS_TILT_Y,
                         -GOODIX_PEN_MAX_TILT, GOODIX_PEN_MAX_TILT, 0, 0);

    r = input_register_device(pen_dev);
    if (r < 0) {
        ts_err("Unable to register pen device");
        input_free_device(pen_dev);
        return r;
    }

    return 0;
}

void goodix_ts_input_dev_remove(struct goodix_ts_core *core_data)
{
    if (IS_ERR_OR_NULL(core_data->input_dev))
        return;
    input_unregister_device(core_data->input_dev);
    input_free_device(core_data->input_dev);
    core_data->input_dev = NULL;
}

void goodix_ts_pen_dev_remove(struct goodix_ts_core *core_data)
{
    if (IS_ERR_OR_NULL(core_data->pen_dev))
        return;
    input_unregister_device(core_data->pen_dev);
    input_free_device(core_data->pen_dev);
    core_data->pen_dev = NULL;
}

/**
 * goodix_ts_esd_work - check hardware status and recovery
 *  the hardware if needed.
 */
static void goodix_ts_esd_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct goodix_ts_esd *ts_esd = container_of(dwork,
                                   struct goodix_ts_esd, esd_work);
    struct goodix_ts_core *cd = container_of(ts_esd,
                                struct goodix_ts_core, ts_esd);
    const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
    int ret = 0;

    if (ts_esd->irq_status)
        goto exit;

    if (!atomic_read(&ts_esd->esd_on))
        return;

    if (IS_ERR_OR_NULL(hw_ops->esd_check))
        return;

    ret = hw_ops->esd_check(cd);
    if (ret) {
        ts_err("esd check failed");
        goodix_ts_power_off(cd);
        usleep_range(5000, 5100);
        goodix_ts_power_on(cd);
    }

exit:
    ts_esd->irq_status = false;
    if (atomic_read(&ts_esd->esd_on))
        schedule_delayed_work(&ts_esd->esd_work, 2 * HZ);
}

/**
 * goodix_ts_esd_on - turn on esd protection
 */
static void goodix_ts_esd_on(struct goodix_ts_core *cd)
{
    struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
    struct goodix_ts_esd *ts_esd = &cd->ts_esd;

    if (!misc->esd_addr)
        return;

    if (atomic_read(&ts_esd->esd_on))
        return;

    atomic_set(&ts_esd->esd_on, 1);
    if (!schedule_delayed_work(&ts_esd->esd_work, 2 * HZ)) {
        ts_info("esd work already in workqueue");
    }
    ts_info("esd on");
}

/**
 * goodix_ts_esd_off - turn off esd protection
 */
static void goodix_ts_esd_off(struct goodix_ts_core *cd)
{
    struct goodix_ts_esd *ts_esd = &cd->ts_esd;
    int ret;

    if (!atomic_read(&ts_esd->esd_on))
        return;

    atomic_set(&ts_esd->esd_on, 0);
    ret = cancel_delayed_work_sync(&ts_esd->esd_work);
    ts_info("Esd off, esd work state %d", ret);
}

/**
 * goodix_esd_notifier_callback - notification callback
 *  under certain condition, we need to turn off/on the esd
 *  protector, we use kernel notify call chain to achieve this.
 *
 *  for example: before firmware update we need to turn off the
 *  esd protector and after firmware update finished, we should
 *  turn on the esd protector.
 */
static int goodix_esd_notifier_callback(struct notifier_block *nb,
                                        unsigned long action, void *data)
{
    struct goodix_ts_esd *ts_esd = container_of(nb,
                                   struct goodix_ts_esd, esd_notifier);

    switch (action) {
    case NOTIFY_FWUPDATE_START:
    case NOTIFY_SUSPEND:
    case NOTIFY_ESD_OFF:
        goodix_ts_esd_off(ts_esd->ts_core);
        break;
    case NOTIFY_FWUPDATE_FAILED:
    case NOTIFY_FWUPDATE_SUCCESS:
    case NOTIFY_RESUME:
    case NOTIFY_ESD_ON:
        goodix_ts_esd_on(ts_esd->ts_core);
        break;
    default:
        break;
    }

    return 0;
}

/**
 * goodix_ts_esd_init - initialize esd protection
 */
int goodix_ts_esd_init(struct goodix_ts_core *cd)
{
    struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
    struct goodix_ts_esd *ts_esd = &cd->ts_esd;

    if (IS_ERR_OR_NULL(cd->hw_ops->esd_check) || !misc->esd_addr) {
        ts_info("missing key info for esd check");
        return 0;
    }

    INIT_DELAYED_WORK(&ts_esd->esd_work, goodix_ts_esd_work);
    ts_esd->ts_core = cd;
    atomic_set(&ts_esd->esd_on, 0);
    ts_esd->esd_notifier.notifier_call = goodix_esd_notifier_callback;
    goodix_ts_register_notifier(&ts_esd->esd_notifier);
    goodix_ts_esd_on(cd);

    return 0;
}

static void goodix_ts_release_connects(struct goodix_ts_core *core_data)
{
    struct input_dev *input_dev = core_data->input_dev;
    int i;
    int ret = 0;
    /*BSP: add patch for Release coordinates  by linsheng.liu 20220520 start*/
    u32 sync_addr = core_data->ic_info.misc.touch_data_addr;
    u8 buf[8] = {0};
    struct tran_core_ops *ops = NULL;
    /*BSP: add patch for Release coordinates  by linsheng.liu 20220520 end*/
    mutex_lock(&input_dev->mutex);

    for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
        input_mt_slot(input_dev, i);
        input_mt_report_slot_state(input_dev,
                                   MT_TOOL_FINGER,
                                   false);
    }
    input_report_key(input_dev, BTN_TOUCH, 0);
    input_mt_sync_frame(input_dev);
    input_sync(input_dev);
    gts_pocket_state_report(0);

    mutex_unlock(&input_dev->mutex);
    /*BSP: add patch for Release coordinates  by linsheng.liu 20220520 start*/
    //if(Cts_SetDozeStatus())
    if (get_tpd_ops(&goodix_controller, &ops)) {
        ret = ops->gesture_state_get(&goodix_controller);
        if (ret) {
            core_data->hw_ops->write(core_data, sync_addr, buf, sizeof(buf));
        }
    }
    /*BSP: add patch for Release coordinates  by linsheng.liu 20220520 end*/
}

/**
 * goodix_ts_suspend - Touchscreen suspend function
 * Called by PM/FB/EARLYSUSPEN module to put the device to  sleep
 */
static int goodix_ts_suspend(struct goodix_ts_core *core_data)
{
    struct goodix_ext_module *ext_module, *next;
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
    int ret;

    if (core_data->init_stage < CORE_INIT_STAGE2 ||
        atomic_read(&core_data->suspended))
        return 0;

    ts_info("Suspend start");
    atomic_set(&core_data->suspended, 1);
    /* disable irq */
    hw_ops->irq_enable(core_data, false);

    /*
     * notify suspend event, inform the esd protector
     * and charger detector to turn off the work
     */
    goodix_ts_blocking_notify(NOTIFY_SUSPEND, NULL);

    /* inform external module */
    mutex_lock(&goodix_modules.mutex);
    if (!list_empty(&goodix_modules.head)) {
        list_for_each_entry_safe(ext_module, next,
                                 &goodix_modules.head, list) {
            if (IS_ERR_OR_NULL(ext_module->funcs->before_suspend))
                continue;

            ret = ext_module->funcs->before_suspend(core_data,
                                                    ext_module);
            if (ret == EVT_CANCEL_SUSPEND) {
                mutex_unlock(&goodix_modules.mutex);
                ts_info("Canceled by module:%s",
                        ext_module->name);
                goto out;
            }
        }
    }
    mutex_unlock(&goodix_modules.mutex);

    /* enter sleep mode or power off */
    if (!IS_ERR_OR_NULL(hw_ops->suspend))
        hw_ops->suspend(core_data);

    /* inform exteranl modules */
    mutex_lock(&goodix_modules.mutex);
    if (!list_empty(&goodix_modules.head)) {
        list_for_each_entry_safe(ext_module, next,
                                 &goodix_modules.head, list) {
            if (IS_ERR_OR_NULL(ext_module->funcs->after_suspend))
                continue;

            ret = ext_module->funcs->after_suspend(core_data,
                                                   ext_module);
            if (ret == EVT_CANCEL_SUSPEND) {
                mutex_unlock(&goodix_modules.mutex);
                ts_info("Canceled by module:%s",
                        ext_module->name);
                goto out;
            }
        }
    }
    mutex_unlock(&goodix_modules.mutex);

out:
    goodix_ts_release_connects(core_data);
    ts_info("Suspend end");
    return 0;
}

/*BSP: add for tp finger by linsheng.liu 20220125 start*/
extern int g_fp_flag;
//int g_aod_flag;
/*BSP: add for tp finger by linsheng.liu 20220125 end*/
/**
 * goodix_ts_resume - Touchscreen resume function
 * Called by PM/FB/EARLYSUSPEN module to wakeup device
 */
static int goodix_ts_resume(struct goodix_ts_core *core_data)
{
    struct goodix_ext_module *ext_module, *next;
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
    int ret;
    //BSP: add KJ7H895-2 for TP bringup by hongwu.zhan 20230814 start
    struct tran_ts_core *ctr = goodix_controller.tran_ts_data;
    //BSP: add KJ7H895-2 for TP bringup by hongwu.zhan 20230814 end

    if (core_data->init_stage < CORE_INIT_STAGE2 ||
        !atomic_read(&core_data->suspended))
        return 0;

    ts_info("Resume start");
    atomic_set(&core_data->suspended, 0);
    /*BSP: solve tp Deadlocks by linsheng.liu 20220328 start*/
    hw_ops->irq_enable(core_data, false);
    /*BSP: solve tp Deadlocks by by linsheng.liu 20220328 end*/
    /*BSP: add for tp finger by linsheng.liu 20220125 start*/
    if (g_fp_flag == 1 || ctr->fp_status.g_fod_flag == 1) {
        ts_info("resume key finger up");
        input_report_key(core_data->input_dev, KEY_FINGER, 0);
        input_sync(core_data->input_dev);
        g_fp_flag = 0;
        ctr->fp_status.g_fod_flag = 0;
    }
    /*BSP: add for tp finger by linsheng.liu 20220125 end*/
    mutex_lock(&goodix_modules.mutex);
    if (!list_empty(&goodix_modules.head)) {
        list_for_each_entry_safe(ext_module, next,
                                 &goodix_modules.head, list) {
            if (IS_ERR_OR_NULL(ext_module->funcs->before_resume))
                continue;

            ret = ext_module->funcs->before_resume(core_data,
                                                   ext_module);
            if (ret == EVT_CANCEL_RESUME) {
                mutex_unlock(&goodix_modules.mutex);
                ts_info("Canceled by module:%s",
                        ext_module->name);
                goto out;
            }
        }
    }
    mutex_unlock(&goodix_modules.mutex);

    /* reset device or power on*/
    if (hw_ops->resume)
        hw_ops->resume(core_data);

    mutex_lock(&goodix_modules.mutex);
    if (!list_empty(&goodix_modules.head)) {
        list_for_each_entry_safe(ext_module, next,
                                 &goodix_modules.head, list) {
            if (IS_ERR_OR_NULL(ext_module->funcs->after_resume))
                continue;

            ret = ext_module->funcs->after_resume(core_data,
                                                  ext_module);
            if (ret == EVT_CANCEL_RESUME) {
                mutex_unlock(&goodix_modules.mutex);
                ts_info("Canceled by module:%s",
                        ext_module->name);
                goto out;
            }
        }
    }
    mutex_unlock(&goodix_modules.mutex);

out:
    /* enable irq */
    hw_ops->irq_enable(core_data, true);
    /* open esd */
    goodix_ts_blocking_notify(NOTIFY_RESUME, NULL);
    //BSP: add resume set 360hz by linsheng.liu 20220222 start
    if (report_rate_switch == 1) {
        brl_360hz_switch(core_data, 1);
    }
    //BSP: add resume set 360hz by linsheng.liu 20220222 end
    ts_info("tran_fingerprint_status=%d g_fp_flag=%d", tran_fingerprint_status, g_fp_flag);
    //BSP:add for charger mode by linsheng.liu 20220810 start
    if (1 == charger_in) {
        ts_info("resume: charger mode in");
        brl_charger_detect(core_data, 1);
    }
    //BSP:add for charger mode by linsheng.liu 20220810 end
    ts_info("Resume end");
    return 0;
}

#ifdef CONFIG_FB

/**
 * goodix_ts_fb_notifier_callback - Framebuffer notifier callback
 * Called by kernel during framebuffer blanck/unblank phrase
 */
int goodix_ts_fb_notifier_callback(struct notifier_block *self,
                                   unsigned long event, void *data)
{
    struct goodix_ts_core *core_data =
        container_of(self, struct goodix_ts_core, fb_notifier);
    struct fb_event *fb_event = data;

    if (fb_event && fb_event->data && core_data) {
        if (event == FB_EARLY_EVENT_BLANK) {
            /* before fb blank */
        } else if (event == FB_EVENT_BLANK) {
            int *blank = fb_event->data;
            if (*blank == FB_BLANK_UNBLANK)
                goodix_ts_resume(core_data);
            else if (*blank == FB_BLANK_POWERDOWN)
                goodix_ts_suspend(core_data);
        }
    }

    return 0;
}
#endif
#if 0
//BSP: add mutex lock for resume/suspend by yun.luo 20220523 start
static DEFINE_MUTEX(tran_suspend_resume);
/*BSP: add thread to handle resume by yun.luo 20220616 start*/
static void goodix_resume_work(struct work_struct *work)
{
    struct goodix_ts_core *core_data = container_of(work, struct goodix_ts_core,
                                       resume_work);

    goodix_ts_resume(core_data);
    mutex_unlock(&tran_suspend_resume);
}
/*BSP: add thread to handle resume by yun.luo 20220616 end*/
static int drm_notifier_callback(struct notifier_block *nb,
                                 unsigned long value, void *v)
{
    struct goodix_ts_core *core_data =
        container_of(nb, struct goodix_ts_core, fb_notifier);
    int *data = (int *)v;

    ts_debug("Enter  %s  value = %ul,data = %d", __func__, value, *data);
    if (v) {
        if (value == MTK_DISP_EVENT_BLANK) {
            //resume: touch power on is after display to avoid display disturb
            /*BSP: modify ADJHBSY-1122 for avoid to wake up by AOD by yun.luo 20220606 start*/
            //if (*data == MTK_DISP_BLANK_UNBLANK) { //select it if not surport AOD function
            if (*data == TRAN_DISP_BLANK_AOD_NOTIFY) {//it will not be called if it's in AOD status
                /*BSP: modify ADJHBSY-1122 for avoid to wake up by AOD by yun.luo 20220606 end*/
                ts_info("%s resume ", __func__);
                /*BSP: add thread to handle resume by yun.luo 20220616 start*/
                mutex_lock(&tran_suspend_resume);
                //goodix_ts_resume(core_data);
                queue_work(core_data->ts_workqueue, &core_data->resume_work);
                //mutex_unlock(&tran_suspend_resume);
                /*BSP: add thread to handle resume by yun.luo 20220616 end*/
            }
        } else if (value == MTK_DISP_EARLY_EVENT_BLANK) {
            //suspend: touch power off is before display to avoid touch report event
            //after screen is off
            if (*data == MTK_DISP_BLANK_POWERDOWN) {
                /*BSP: add thread to handle resume by yun.luo 20220616 start*/
                ts_info("%s suspend ", __func__);
                mutex_lock(&tran_suspend_resume);
                cancel_work_sync(&core_data->resume_work);
                goodix_ts_suspend(core_data);
                mutex_unlock(&tran_suspend_resume);
                /*BSP: add thread to handle resume by yun.luo 20220616 end*/
            }
        }
    } else {
        ts_info("gt9916 touch IC can not suspend or resume");
        return -1;
    }
    ts_debug("Leave  %s", __func__);
    return 0;
}
//BSP: add mutex lock for resume/suspend by yun.luo 20220523 end
//BSP: add gesture function for ad9 by yun.luo 20220418 end

//BSP: add ADJHBSY-592 for disable PM by yun.luo 20220527 start
#endif

#if defined(CONFIG_PM) && GOODIX_IS_ENABLE_PM
//BSP: add ADJHBSY-592 for disable PM by yun.luo 20220527 end
#if !defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND)
/**
 * goodix_ts_pm_suspend - PM suspend function
 * Called by kernel during system suspend phrase
 */
static int goodix_ts_pm_suspend(struct device *dev)
{
    struct goodix_ts_core *core_data =
        dev_get_drvdata(dev);

    return goodix_ts_suspend(core_data);
}
/**
 * goodix_ts_pm_resume - PM resume function
 * Called by kernel during system wakeup
 */
static int goodix_ts_pm_resume(struct device *dev)
{
    struct goodix_ts_core *core_data =
        dev_get_drvdata(dev);

    return goodix_ts_resume(core_data);
}
#endif
#endif

/**
 * goodix_generic_noti_callback - generic notifier callback
 *  for goodix touch notification event.
 */
static int goodix_generic_noti_callback(struct notifier_block *self,
                                        unsigned long action, void *data)
{
    struct goodix_ts_core *cd = container_of(self,
                                struct goodix_ts_core, ts_notifier);
    const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

    if (cd->init_stage < CORE_INIT_STAGE2)
        return 0;

    ts_info("notify event type 0x%x", (unsigned int)action);
    switch (action) {
    case NOTIFY_FWUPDATE_START:
        hw_ops->irq_enable(cd, 0);
        break;
    case NOTIFY_FWUPDATE_SUCCESS:
    case NOTIFY_FWUPDATE_FAILED:
        if (hw_ops->read_version(cd, &cd->fw_version))
            ts_info("failed read fw version info[ignore]");
        hw_ops->irq_enable(cd, 1);
        break;
    default:
        break;
    }
    return 0;
}

int goodix_ts_stage2_init(struct goodix_ts_core *cd)
{
    int ret;

    /* alloc/config/register input device */
    ret = goodix_ts_input_dev_config(cd);
    if (ret < 0) {
        ts_err("failed set input device");
        return ret;
    }
    //BSP: add ADJHBSY-592 for wakeup lock by yun.luo 20220527 start
    device_init_wakeup(&cd->input_dev->dev, 1);
    //BSP: add ADJHBSY-592 for wakeup lock by yun.luo 20220527 end
    if (cd->board_data.pen_enable) {
        ret = goodix_ts_pen_dev_config(cd);
        if (ret < 0) {
            ts_err("failed set pen device");
            goto err_finger;
        }
    }
    /* request irq line */
    ret = goodix_ts_irq_setup(cd);
    if (ret < 0) {
        ts_info("failed set irq");
        goto exit;
    }
    ts_info("success register irq");

#ifdef CONFIG_FB
    cd->fb_notifier.notifier_call = goodix_ts_fb_notifier_callback;
    if (fb_register_client(&cd->fb_notifier))
        ts_err("Failed to register fb notifier client:%d", ret);
#endif
    /*	cd->fb_notifier.notifier_call = drm_notifier_callback;
    if (mtk_disp_notifier_register("goodix-ts",&cd->fb_notifier))
    	ts_err("Failed to register fb notifier client:%d", ret);*/
    //BSP: add gesture function for ad9 by yun.luo 20220418 end
    /* create sysfs files */
    goodix_ts_sysfs_init(cd);

    /* create procfs files */
    goodix_ts_procfs_init(cd);

    /* esd protector */
    goodix_ts_esd_init(cd);

    /* gesture init */
    gesture_module_init();

    /* inspect init */
    /* BSP: add ACBHBHW-2 for tp midtest test by yun.luo 20220426 start */
    inspect_module_init(cd);
    /* BSP: add ACBHBHW-2 for tp midtest test by yun.luo 20220426 end */

    return 0;
exit:
    goodix_ts_pen_dev_remove(cd);
    //BSP: add ADJHBSY-592 for wakeup lock by yun.luo 20220527 start
    device_init_wakeup(&cd->input_dev->dev, 0);
    //BSP: add ADJHBSY-592 for wakeup lock by yun.luo 20220527 end
err_finger:
    goodix_ts_input_dev_remove(cd);
    return ret;
}

/* try send the config specified with type */
static int goodix_send_ic_config(struct goodix_ts_core *cd, int type)
{
    u32 config_id;
    struct goodix_ic_config *cfg;

    if (type >= GOODIX_MAX_CONFIG_GROUP) {
        ts_err("unsupproted config type %d", type);
        return -EINVAL;
    }

    cfg = cd->ic_configs[type];
    if (IS_ERR_OR_NULL(cfg) || cfg->len <= 0) {
        ts_info("no valid normal config found");
        return -EINVAL;
    }

    config_id = goodix_get_file_config_id(cfg->data);
    //BSP: show bv version ACBHBWS-367 by linsheng.liu 20220110 start
    bin_config_id = config_id;
    //BSP: show bv version ACBHBWS-367 by linsheng.liu 20220110 end
    if (cd->ic_info.version.config_id == config_id) {
        ts_info("config id is equal 0x%x, skiped", config_id);
        return 0;
    }

    ts_info("try send config, id=0x%x", config_id);
    return cd->hw_ops->send_config(cd, cfg->data, cfg->len);
}

/**
 * goodix_later_init_thread - init IC fw and config
 * @data: point to goodix_ts_core
 *
 * This function respond for get fw version and try upgrade fw and config.
 * Note: when init encounter error, need release all resource allocated here.
 */
static int goodix_later_init_thread(void *data)
{
    int ret, i;
    int update_flag = UPDATE_MODE_BLOCK | UPDATE_MODE_SRC_REQUEST;
    struct goodix_ts_core *cd = data;
    struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

    /* step 1: read version */
    ret = cd->hw_ops->read_version(cd, &cd->fw_version);
    if (ret < 0) {
        ts_err("failed to get version info, try to upgrade");
        update_flag |= UPDATE_MODE_FORCE;
        goto upgrade;
    }

    /* step 2: get config data from config bin */
    ret = goodix_get_config_proc(cd);
    if (ret)
        ts_info("no valid ic config found");
    else
        ts_info("success get valid ic config");

upgrade:
    /* step 3: init fw struct add try do fw upgrade */
    ret = goodix_fw_update_init(cd);
    if (ret) {
        ts_err("failed init fw update module");
        goto err_out;
    }

    ts_info("update flag: 0x%X", update_flag);
    ret = goodix_do_fw_update(cd->ic_configs[CONFIG_TYPE_NORMAL], update_flag);
    /*BSP: add XLQEHBY-961 by jiangzhu.yang 20220105 start*/
    if (ret) {
        ts_err("failed do fw update");
        goto err_out;
    }
    /*BSP: add XLQEHBY-961 by jiangzhu.yang 20220105 end*/

    /* step 4: get fw version and ic_info
     * at this step we believe that the ic is in normal mode,
     * if the version info is invalid there must have some
     * problem we cann't cover so exit init directly.
     */
    ret = hw_ops->read_version(cd, &cd->fw_version);
    if (ret) {
        ts_err("invalid fw version, abort");
        goto uninit_fw;
    }
    ret = hw_ops->get_ic_info(cd, &cd->ic_info);
    if (ret) {
        ts_err("invalid ic info, abort");
        goto uninit_fw;
    }

    /* the recomend way to update ic config is throuth ISP,
     * if not we will send config with interactive mode
     */
    goodix_send_ic_config(cd, CONFIG_TYPE_NORMAL);

    /* init other resources */
    ret = goodix_ts_stage2_init(cd);
    if (ret) {
        ts_err("stage2 init failed");
        goto uninit_fw;
    }
    cd->init_stage = CORE_INIT_STAGE2;
    //BSP:add for charger mode by linsheng.liu 20220810 start
    firmware_updata_flag = 1;
    if (charger_in == 1)
        brl_charger_detect(cd, 1);
    //BSP:add for charger mode by linsheng.liu 20220810 end
    return 0;

uninit_fw:
    goodix_fw_update_uninit();
err_out:
    ts_err("stage2 init failed");
    cd->init_stage = CORE_INIT_FAIL;
    for (i = 0; i < GOODIX_MAX_CONFIG_GROUP; i++) {
        if (cd->ic_configs[i])
            kfree(cd->ic_configs[i]);
        cd->ic_configs[i] = NULL;
    }
    return ret;
}

static int goodix_start_later_init(struct goodix_ts_core *ts_core)
{
    /*BSP: add XLQEHBY-961 by jiangzhu.yang 20220105 start*/
    //struct task_struct *init_thrd;
    /* create and run update thread */
    ts_core->init_thrd = kthread_run(goodix_later_init_thread,
                                     ts_core, "goodix_init_thread");
    if (IS_ERR_OR_NULL(ts_core->init_thrd)) {
        ts_err("Failed to create update thread:%ld",
               PTR_ERR(ts_core->init_thrd));
        return -EFAULT;
    }
    /*BSP: add XLQEHBY-961 by jiangzhu.yang 20220105 end*/
    return 0;
}

/**
 * goodix_ts_probe - called by kernel when Goodix touch
 *  platform driver is added.
 */
static int goodix_ts_probe(struct platform_device *pdev)
{
    struct goodix_ts_core *core_data = NULL;
    struct goodix_bus_interface *bus_interface;
    int ret;
    //BSP: add KJ7H895-2 for TP bringup by hongwu.zhan 20230814 start
    struct tran_core_ops *ops = NULL;
    //BSP: add KJ7H895-2 for TP bringup by hongwu.zhan 20230814 end

    ts_info("goodix_ts_probe IN");

    bus_interface = pdev->dev.platform_data;
    if (IS_ERR_OR_NULL(bus_interface)) {
        ts_err("Invalid touch device");
        core_module_prob_sate = CORE_MODULE_PROB_FAILED;
        return -ENODEV;
    }

    core_data = devm_kzalloc(&pdev->dev,
                             sizeof(struct goodix_ts_core), GFP_KERNEL);
    if (IS_ERR_OR_NULL(core_data)) {
        ts_err("Failed to allocate memory for core data");
        core_module_prob_sate = CORE_MODULE_PROB_FAILED;
        return -ENOMEM;
    }

    if (IS_ENABLED(CONFIG_OF) && bus_interface->dev->of_node) {
        /* parse devicetree property */
        ret = goodix_parse_dt(bus_interface->dev->of_node,
                              &core_data->board_data);
        if (ret) {
            ts_err("failed parse device info form dts, %d", ret);
            return -EINVAL;
        }
    } else {
        ts_err("no valid device tree node found");
        return -ENODEV;
    }

    core_data->hw_ops = goodix_get_hw_ops();
    if (IS_ERR_OR_NULL(core_data->hw_ops)) {
        ts_err("hw ops is NULL");
        core_module_prob_sate = CORE_MODULE_PROB_FAILED;
        return -EINVAL;
    }
    goodix_core_module_init();
    /* touch core layer is a platform driver */
    core_data->pdev = pdev;
    core_data->bus = bus_interface;
    platform_set_drvdata(pdev, core_data);

    /* get GPIO resource */
    ret = goodix_ts_gpio_setup(core_data);
    if (ret) {
        ts_err("failed init gpio");
        goto err_out;
    }

    ret = goodix_ts_power_init(core_data);
    if (ret) {
        ts_err("failed init power");
        goto err_out;
    }

    ret = goodix_ts_power_on(core_data);
    if (ret) {
        ts_err("failed power on");
        goto err_out;
    }

    /* generic notifier callback */
    core_data->ts_notifier.notifier_call = goodix_generic_noti_callback;
    goodix_ts_register_notifier(&core_data->ts_notifier);
    /*BSP: add thread to handle resume by yun.luo 20220616 start*/
    /*core_data->ts_workqueue = create_singlethread_workqueue("goodix_wq");
    if (IS_ERR_OR_NULL(core_data->ts_workqueue)) {
    	ts_err("create resume workqueue fail");
    }else{
    	INIT_WORK(&core_data->resume_work, goodix_resume_work);
    }*/
    /*BSP: add thread to handle resume by yun.luo 20220616 end*/
    /* debug node init */
    goodix_tools_init();

    core_data->init_stage = CORE_INIT_STAGE1;
    goodix_modules.core_data = core_data;
    core_module_prob_sate = CORE_MODULE_PROB_SUCCESS;

    /* Try start a thread to get config-bin info */
    /*BSP: add XLQEHBY-961 by jiangzhu.yang 20220105 start*/
    ret = goodix_start_later_init(core_data);
    if (ret < 0) {
        ts_err("start later init failed");
        goto err_out;
    }
    /*BSP: add XLQEHBY-961 by jiangzhu.yang 20220105 end*/
    //BSP:add for charger mode by linsheng.liu 20220810 start
    INIT_WORK(&core_data->charger_work, goodix_charger_work);
    charger_nof.notifier_call = mtk_power_misc_psy_event;
    power_supply_reg_notifier(&charger_nof);
    //BSP:add for charger mode by linsheng.liu 20220810 end
    //BSP: add KJ7H895-2 for TP bringup by hongwu.zhan 20230814 start
    if (get_tpd_ops(&goodix_controller, &ops)) {
        ops->controller_init_completion(&goodix_controller);
    }
    //BSP: add KJ7H895-2 for TP bringup by hongwu.zhan 20230814 end

    ts_info("goodix_ts_core probe success");
    return 0;

err_out:
    core_data->init_stage = CORE_INIT_FAIL;
    core_module_prob_sate = CORE_MODULE_PROB_FAILED;
    /*BSP: add thread to handle resume by yun.luo 20220616 start*/
    if (core_data->ts_workqueue) {
        destroy_workqueue(core_data->ts_workqueue);
    }
    /*BSP: add thread to handle resume by yun.luo 20220616 end*/
    /*BSP: add XLQEHBY-961 by jiangzhu.yang 20220105 start*/
    if (!IS_ERR_OR_NULL(core_data->init_thrd)) {
        kthread_stop(core_data->init_thrd);
        core_data->init_thrd = NULL;
        ts_err("stop init thread");
    }
    /*BSP: add XLQEHBY-961 by jiangzhu.yang 20220105 end*/
    ts_err("goodix_ts_core failed, ret:%d", ret);
    return ret;
}

static void goodix_ts_remove(struct platform_device *pdev)
{
    struct goodix_ts_core *core_data = platform_get_drvdata(pdev);
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
    struct goodix_ts_esd *ts_esd = &core_data->ts_esd;

    goodix_ts_unregister_notifier(&core_data->ts_notifier);
    goodix_tools_exit();

    if (core_data->init_stage >= CORE_INIT_STAGE2) {
        gesture_module_exit();
        inspect_module_exit();
        hw_ops->irq_enable(core_data, false);
#ifdef CONFIG_FB
        fb_unregister_client(&core_data->fb_notifier);
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
        mtk_disp_notifier_unregister(&core_data->fb_notifier);
#endif
        core_module_prob_sate = CORE_MODULE_REMOVED;
        if (atomic_read(&core_data->ts_esd.esd_on))
            goodix_ts_esd_off(core_data);
        goodix_ts_unregister_notifier(&ts_esd->esd_notifier);
        //BSP: add ADJHBSY-592 for wakeup lock by yun.luo 20220527 start
        device_init_wakeup(&core_data->input_dev->dev, 0);
        //BSP: add ADJHBSY-592 for wakeup lock by yun.luo 20220527 end

        goodix_fw_update_uninit();
        goodix_ts_input_dev_remove(core_data);
        goodix_ts_pen_dev_remove(core_data);
        goodix_ts_sysfs_exit(core_data);
        goodix_ts_procfs_exit(core_data);
        goodix_ts_power_off(core_data);
    }
}

//BSP: add ADJHBSY-592 for disable PM by yun.luo 20220527 start
#if defined(CONFIG_PM) && GOODIX_IS_ENABLE_PM
//BSP: add ADJHBSY-592 for disable PM by yun.luo 20220527 end
static const struct dev_pm_ops dev_pm_ops = {
#if !defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND)
    .suspend = goodix_ts_pm_suspend,
    .resume = goodix_ts_pm_resume,
#endif
};
#endif

static const struct platform_device_id ts_core_ids[] = {
    {.name = GOODIX_CORE_DRIVER_NAME},
    {}
};
MODULE_DEVICE_TABLE(platform, ts_core_ids);

static struct platform_driver goodix_ts_driver = {
    .driver = {
        .name = GOODIX_CORE_DRIVER_NAME,
        .owner = THIS_MODULE,
//BSP: add ADJHBSY-592 for disable PM by yun.luo 20220527 start
#if defined(CONFIG_PM) && GOODIX_IS_ENABLE_PM
//BSP: add ADJHBSY-592 for disable PM by yun.luo 20220527 end
        .pm = &dev_pm_ops,
#endif
    },
    .probe = goodix_ts_probe,
    .remove = goodix_ts_remove,
    .id_table = ts_core_ids,
};

static int goodix_ts_core_init(void)
{
    int ret;

#ifdef CONFIG_TOUCHSCREEN_GOODIX_BRL_SPI
    ret = goodix_spi_bus_init();
    ts_info("goodix_spi_bus_init end\n");
#else
   ret = goodix_i2c_bus_init();
#endif
    if (ret) {
        ts_err("failed add bus driver");
        return ret;
    }
    return platform_driver_register(&goodix_ts_driver);
}

static void __exit goodix_ts_core_exit(void)
{
    ts_info("Core layer exit");

    platform_driver_unregister(&goodix_ts_driver);

#ifdef CONFIG_TOUCHSCREEN_GOODIX_BRL_SPI
    goodix_spi_bus_exit();
#else
    goodix_i2c_bus_exit();
#endif
}

/*************************transsion************************/
extern u8 bin_fw_version;
extern int register_ts_controller(struct tran_ts_controller *controller);
extern int unregister_ts_controller(struct tran_ts_controller *controller);

struct tran_ts_controller goodix_controller;

static int goodix_ts_init(void)
{
    int ret = 0;

    TRAN_FUNC_ENTER();
    goodix_ts_core_init();
    TRAN_FUNC_EXIT();

    return ret;
}

static void suspend(struct device *h, int opt)
{
    struct goodix_ts_core *core_data = goodix_modules.core_data;

    if (goodix_controller.controller_state == TRAN_GESTURE_SUSPEND_MODE) {
        goodix_gesture_enable(1);
    } else if (goodix_controller.controller_state == TRAN_SUSPEND_MODE) {
        goodix_gesture_enable(0);
    }
    goodix_ts_suspend(core_data);
}

static void resume(struct device *h, int opt)
{
    struct goodix_ts_core *core_data = goodix_modules.core_data;

    goodix_ts_resume(core_data);

}

#define GOODIX_GAME_STATE_CMD	0x33
void goodix_game_state_switch(struct goodix_ts_core *cd, u8 type)
{
    struct goodix_ts_cmd game_cmd;

    game_cmd.cmd = GOODIX_GAME_STATE_CMD;
    game_cmd.len = 5;
    game_cmd.data[0] = type;
    if (type) {
        game_cmd.data[1] = 0x05;
        game_cmd.data[2] = 0x3D;
    } else {
        game_cmd.data[1] = 0x00;
        game_cmd.data[2] = 0x38;
    }
    if (cd->hw_ops->send_cmd(cd, &game_cmd)) {
        ts_err("failed send game state cmd");
    } else {
        ts_info("%s game state successful!", type ? "Enter" : "Exit");
    }

}

#define GOODIX_SAMPLING_RATE_CMD	0xC2
void goodix_sampling_rate_switch(struct goodix_ts_core *cd, u8 type)
{
    struct goodix_ts_cmd sampling_cmd;

    sampling_cmd.cmd = GOODIX_SAMPLING_RATE_CMD;
    sampling_cmd.len = 5;
    sampling_cmd.data[0] = type;
    if (type) {
        sampling_cmd.data[1] = 0xC8;
        sampling_cmd.data[2] = 0x00;
    } else {
        sampling_cmd.data[1] = 0xC7;
        sampling_cmd.data[2] = 0x00;
    }
    if (cd->hw_ops->send_cmd(cd, &sampling_cmd)) {
        ts_err("failed send game state cmd");
    } else {
        ts_info("%s super idle successful!", type ? "Enter" : "Exit");
    }

}

//BSP: add for esport 1.0 by hongwu.zhan 20231007 start
#define GOODIX_GAME_LOCK_POINT_CMD	0x33
void goodix_lock_point_switch(struct goodix_ts_core *cd, u32 type)
{
    struct goodix_ts_cmd lock_point_cmd;

    lock_point_cmd.cmd = GOODIX_GAME_LOCK_POINT_CMD;
    lock_point_cmd.len = 5;
    lock_point_cmd.data[2] = 0x00;
    switch (type) {
    /*Attenuation of default*/
    case 0:
        lock_point_cmd.data[0] = 0x00;
        lock_point_cmd.data[1] = 0x38;
        break;
    /*Attenuation of 5%*/
    case 1:
        lock_point_cmd.data[0] = 0x01;
        lock_point_cmd.data[1] = 0x39;
        break;
    /*Attenuation of 8%*/
    case 2:
        lock_point_cmd.data[0] = 0x02;
        lock_point_cmd.data[1] = 0x3A;
        break;
    /*Attenuation of 10%*/
    case 3:
        lock_point_cmd.data[0] = 0x03;
        lock_point_cmd.data[1] = 0x3B;
        break;
    /*Attenuation of 15%*/
    case 4:
        lock_point_cmd.data[0] = 0x04;
        lock_point_cmd.data[1] = 0x3C;
        break;
    /*Attenuation of 30%*/
    case 5:
        lock_point_cmd.data[0] = 0x05;
        lock_point_cmd.data[1] = 0x3D;
        break;
    }
    if (cd->hw_ops->send_cmd(cd, &lock_point_cmd)) {
        ts_err("failed send game lock point cmd");
    } else {
        ts_info("Enter %d game lock point level successful!", type);
    }

}

#define GOODIX_REPORT_RATE_CMD	0x27
void goodix_report_rate_switch(struct goodix_ts_core *cd, u32 type)
{
    struct goodix_ts_cmd report_rate_cmd;

    report_rate_cmd.cmd = GOODIX_REPORT_RATE_CMD;
    report_rate_cmd.len = 5;
    report_rate_cmd.data[2] = 0x00;
    switch (type) {
    /*Reduce sensitivity by 2 levels*/
    case 1:
        report_rate_cmd.data[0] = 0x01;
        report_rate_cmd.data[1] = 0x2D;
        break;
    /*Reduce sensitivity by 1 level*/
    case 2:
        report_rate_cmd.data[0] = 0x02;
        report_rate_cmd.data[1] = 0x2E;
        break;
    /*default*/
    case 0:/*exit game mode, recovery default*/
    case 3:
        report_rate_cmd.data[0] = 0x03;
        report_rate_cmd.data[1] = 0x2F;
        break;
    /*Increased sensitivity by 1 level*/
    case 4:
        report_rate_cmd.data[0] = 0x04;
        report_rate_cmd.data[1] = 0x30;
        break;
    /*Increased sensitivity by 2 levels*/
    case 5:
        report_rate_cmd.data[0] = 0x05;
        report_rate_cmd.data[1] = 0x31;
        break;
    }
    if (cd->hw_ops->send_cmd(cd, &report_rate_cmd)) {
        ts_err("failed send game report rate cmd");
    } else {
        ts_info("Enter %d game report rate successful!", type);
    }

}

//BSP: add for edge level by hongwu.zhan 20231013 end
#define GOODIX_EDGE_LEVEL_CMD	0x36
void goodix_edge_level_switch(struct goodix_ts_core *cd, u32 type)
{
    struct goodix_ts_cmd edge_level_cmd;

    edge_level_cmd.cmd = GOODIX_EDGE_LEVEL_CMD;
    edge_level_cmd.len = 5;
    edge_level_cmd.data[2] = 0x00;
    switch (type) {
    /* default */
    case 0:
        edge_level_cmd.data[0] = 0x00;
        edge_level_cmd.data[1] = 0x3B;
        break;
    /* Weak */
    case 2:
        edge_level_cmd.data[0] = 0x01;
        edge_level_cmd.data[1] = 0x3C;
        break;
    /* Strong */
    case 3:
        edge_level_cmd.data[0] = 0x02;
        edge_level_cmd.data[1] = 0x3D;
        break;
    }
    if (cd->hw_ops->send_cmd(cd, &edge_level_cmd)) {
        ts_err("failed send TP edge level cmd");
    } else {
        ts_info("Enter %d TP edge_level successful!", type);
    }
}
//BSP: add for edge level by hongwu.zhan 20231013 end
//BSP: add for custom mistouch by linsheng.liu 20231019 start
static void gtp_custom_mistouch_switch(u32 state)
{
    struct goodix_ts_core *core_data = goodix_modules.core_data;
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
    u8 writebuf[40] = { 0 };
    u8 mistouchenter[7] = {0x00, 0x00, 0x05, 0x34, 0x01, 0x3A, 0x00};
    u8 mistouchexit[7] = {0x00, 0x00, 0x05, 0x34, 0x00, 0x39, 0x00};
    int ret;
    struct tran_ts_core *ctr = goodix_controller.tran_ts_data;

    if (core_data->board_data.custom_mistouch_support == false) {
        return;
    }
    for (ret = 0; ret < 5; ret++) {
        writebuf[0 + ret * 8] = ctr->mistouch[ret].LeftStartX_H;
        writebuf[1 + ret * 8] = ctr->mistouch[ret].LeftStartX_L;
        writebuf[2 + ret * 8] = ctr->mistouch[ret].LeftStartY_H;
        writebuf[3 + ret * 8] = ctr->mistouch[ret].LeftStartY_L;
        writebuf[4 + ret * 8] = ctr->mistouch[ret].RightStartX_H;
        writebuf[5 + ret * 8] = ctr->mistouch[ret].RightStartX_L;
        writebuf[6 + ret * 8] = ctr->mistouch[ret].RightStartY_H;
        writebuf[7 + ret * 8] = ctr->mistouch[ret].RightStartY_L;
    }
    TRAN_INFO("custom mistouch data+++++++++++");
    for (ret = 0; ret < 40; ret++) {
        TRAN_INFO("0x%x ", writebuf[ret]);
    }
    TRAN_INFO("custom mistouch data--------------");

    if (state == 1) {
        ret = hw_ops->write(core_data, 0x10220, writebuf, 40);
        if (ret < 0) {
            TRAN_ERROR("write custom mistouch reg fail");
        }
        ret = hw_ops->write(core_data, 0x10174, mistouchenter, 7);
        if (ret < 0) {
            TRAN_ERROR("write custom mistouch enable fail");
        }
    } else {
        ret = hw_ops->write(core_data, 0x10174, mistouchexit, 7);
        if (ret < 0) {
            TRAN_ERROR("write custom mistouch disable fail");
        }
    }
}
//BSP: add for custom mistouch by linsheng.liu 20231019 end
static void set_ts_mode(u32 cmd, u32 state)
{
    struct goodix_ts_core *core_data = goodix_modules.core_data;

    if (NULL == core_data) {
        TRAN_ERROR("core_data is null");
        return;
    }
    switch (cmd) {
    case SET_NOISE_MODE:
        break;

    case SET_PS_MODE:
        break;

    case SET_GAME_MODE:
        goodix_game_state_switch(core_data, state);
        break;

    case SET_EDGE_REJECT_MODE:
        if (state == 1) {
            brl_edge_restrain_in(core_data, 1);
            TRAN_INFO("usb forward right: set edge reject switch: %d", state);
        } else if (state == 3) {
            brl_edge_restrain_in(core_data, 2);
            TRAN_INFO("usb forward left: set edge reject switch: %d", state);
        } else {
            brl_edge_restrain_out(core_data);
            TRAN_INFO("set edge reject switch: 0");
        }
        break;
    /* For esport feature start*/
    case LOCK_POINT_SWITCH:
        goodix_lock_point_switch(core_data, state);
        break;
    case REPORT_RATE_SWITCH:
        goodix_report_rate_switch(core_data, state);
        break;
    case TRANSIENT_SAMPLING_RATE_SWITCH:
        goodix_sampling_rate_switch(core_data, state);
        break;
    /* For esport feature end*/

    case SET_EDGE_LEVER_MODE:
        TRAN_INFO("write edge level %d", state);
        goodix_edge_level_switch(core_data, state);
        break;
    //BSP: add for custom mistouch by linsheng.liu 20231019 start
    case SET_CUSTOM_MISTOUCH_MODE:
        gtp_custom_mistouch_switch(state);
        break;
    //BSP: add for custom mistouch by linsheng.liu 20231019 end
    default:
        TRAN_INFO("can't find cmd=0x%x", cmd);
        break;
    }
}

//BSP: add KJ7H895-2 for TP fw info by hongwu.zhan 20230821 start
static unsigned char get_goodix_firmware_version(void)
{
    struct goodix_ts_core *core_data = goodix_modules.core_data;
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
    struct goodix_fw_version chip_ver;

    TRAN_FUNC_ENTER();

    hw_ops->read_version(core_data, &chip_ver);
    hw_ops->get_ic_info(core_data, &core_data->ic_info);

    ts_info("fw_version = 0x%x, config_id = 0x%x", chip_ver.patch_vid[3], core_data->ic_info.version.config_id);
    ts_info("bin_fw_version = 0x%x, bin_config_id = 0x%x", bin_fw_version, bin_config_id);

    if (chip_ver.patch_vid[3] == bin_fw_version) {
        return chip_ver.patch_vid[3];
    } else {
        return 0;
    }
}

static unsigned char get_goodix_base_version(void)
{

    TRAN_FUNC_ENTER();

    return 0;
}

static unsigned char get_goodix_config_version(void)
{
    struct goodix_ts_core *core_data = goodix_modules.core_data;
    struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
    struct goodix_fw_version chip_ver;

    TRAN_FUNC_ENTER();

    hw_ops->read_version(core_data, &chip_ver);
    hw_ops->get_ic_info(core_data, &core_data->ic_info);

    ts_info("fw_version = 0x%x, config_id = 0x%x", chip_ver.patch_vid[3], core_data->ic_info.version.config_id);
    ts_info("bin_fw_version = 0x%x, bin_config_id = 0x%x", bin_fw_version, bin_config_id);

    return core_data->ic_info.version.config_version;
}
//BSP: add KJ7H895-2 for TP fw info by hongwu.zhan 20230821 end

static int tp_selftest(void)
{
    int ret = 0;
    struct ts_rawdata_info *info = NULL;
    struct goodix_ts_core *core_data = goodix_modules.core_data;

    TRAN_FUNC_ENTER();
    info = kzalloc(sizeof(*info), GFP_KERNEL);
    if (IS_ERR_OR_NULL(info)) {
        ts_err("Failed to alloc rawdata info memory");
        return -ENOMEM;
    }

    if (goodix_do_inspect(core_data, info)) {
        ts_err("Factory_test FAIL");
    }
    //USD:modify tp test by linsheng.liu 20240508 start
    if (strstr(info->result, "1P") && strstr(info->result, "2P") && strstr(info->result, "3P") && strstr(info->result, "5P")) {
        //ptr += sprintf(ptr, "%s\n%s\n", s_test_pass, o_test_pass);
        ret = 1;
    } else {
        //ptr += sprintf(ptr, "%s\n%s\n", s_test_fail, o_test_fail);
        ret = 0;
    }
    //USD:modify tp test by linsheng.liu 20240508 end
    kfree(info);
    TRAN_FUNC_EXIT();
    return ret;

}

static int get_selftest_data(char *data)
{
    TRAN_FUNC_ENTER();

    return 0;
}

static int tran_selftest_open(struct inode *inode, struct file *file)
{
    TRAN_FUNC_ENTER();

    return 0;
}

static void tp_reset_level(int val)
{
    TRAN_FUNC_ENTER();
}

/* 1: success 0: fail */
int get_tpd_ops(struct tran_ts_controller *controller, struct tran_core_ops **ops)
{
    if (controller->tran_ts_data != NULL) {
        *ops = controller->tran_ts_data->tpd_ops;
        if (!IS_ERR_OR_NULL(*ops))
            return 1;
    }

    TRAN_ERROR("get ops fail !");
    return 0;
}

/*BSP: add for ESport1.0 X6850H895-5 by longtao.ran 20231220 start*/
int get_lock_point_distance(u8 *data)
{
    struct goodix_ts_core *core_data = goodix_modules.core_data;

    if (NULL == data) {
        ts_err("data is NULL");
        return -EINVAL;
    }
    if (core_data->hw_ops->read(core_data, 0x10270, data, 2)) {
        ts_err("read fw lock point distance from tp fail");
    }
    *data = *data / 8;
    return 0;
}
/*BSP: add for ESport1.0 X6850H895-5 by longtao.ran 20231220 end*/

/***vendor & transsion ***/
static struct vendor_gesture_map  goodix_gestures_maps[] = {
    {GESTURE_LEFT,        GESTURE_LF},
    {GESTURE_UP,          GESTURE_up},
    {GESTURE_RIGHT,       GESTURE_RT},
    {GESTURE_DOWN,        GESTURE_down},
    {GESTURE_DOUBLECLICK, GESTURE_DC},
    {GESTURE_W,           GESTURE_w},
    {GESTURE_O,           GESTURE_o},
    {GESTURE_M,           GESTURE_m},
    {GESTURE_E,           GESTURE_e},
    {GESTURE_C,           GESTURE_c},
    {GESTURE_Z,           GESTURE_z},
    {GESTURE_S,           GESTURE_s},
    {GESTURE_V,           GESTURE_v},
    {GESTURE_SINGLECLICK, GESTURE_OC},
    {GESTURE_FOD_DOWN,    GESTURE_FOD_down},
    {GESTURE_FOD_UP,      GESTURE_FOD_up},

    {0, 0},
};

static char *support_list[] = {
    "gt9916",
    NULL /* not modify */
};

static struct tran_controller_ops controller_ops = {
    .tran_ts_init = goodix_ts_init,
    .tran_ts_suspend = suspend,
    .tran_ts_resume = resume,
    .set_ts_mode = set_ts_mode,
    .get_firmware_version = get_goodix_firmware_version,
    .get_base_version = get_goodix_base_version,
    .get_config_version = get_goodix_config_version,
    .tran_selftest = tp_selftest,
    .get_selftest_data = get_selftest_data,
    .tran_selftest_open = tran_selftest_open,
    .tran_vendor_gesture_enable = goodix_gesture_enable,
    .get_lock_point_distance = get_lock_point_distance,
};

static struct tran_controller_ext_ops controller_ext_ops = {
    .tran_tp_reset = tp_reset_level,
};

struct tran_ts_controller goodix_controller = {
    .cur_ic = NULL,
    .tran_ts_data = NULL,
    .selftest_data = NULL,
    .gesture_flag = true,
    .controller_state = TRAN_ACTIVE_MODE,
    .vendor_name = "goodix",
    .gesture_map = goodix_gestures_maps,
    .support_ic = support_list,
    .ops = &controller_ops,
    .ext_ops = &controller_ext_ops,
    .dsp_event_mutex = __MUTEX_INITIALIZER(goodix_controller.dsp_event_mutex),
};

static int __init goodix_device_init(void)
{
    ts_info("in");
    init_completion(&goodix_controller.comp);
    register_ts_controller(&goodix_controller);
    ts_info("out");
    return 0;
}

static void __exit goodix_device_exit(void)
{
    ts_info("in");
    /* 清除transsion core的资源 */
    unregister_ts_controller(&goodix_controller);
    goodix_ts_core_exit();
    ts_info("out");

    /* 清除当前驱动的资源 */
}

module_init(goodix_device_init);
module_exit(goodix_device_exit);

/*************************transsion************************/

/*
late_initcall(goodix_ts_core_init);
module_exit(goodix_ts_core_exit); */

MODULE_DESCRIPTION("Goodix Touchscreen Core Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
