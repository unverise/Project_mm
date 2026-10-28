#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include "transsion_core.h"

/* TP_INFO in midtest */
static ssize_t tp_firmware_version_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    u8 fw_ver_in_tp = 0;
    u8 fw_ver_in_fs = 0;
    u8 config_ver = 0;
    char *algo_version = NULL;
    char *algo_func = NULL;
    struct tran_ts_controller *pController = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];

    if(!pController || !pController->controller_init_state){
        TRAN_ERROR("main tp init failed do nothing");
        return 0;
    }

    if(pController->ops->get_firmware_version && pController->ops->get_base_version) {
        fw_ver_in_tp = pController->ops->get_firmware_version();
        fw_ver_in_fs = pController->ops->get_base_version();
        if(pController->ops->get_config_version) {
            config_ver = pController->ops->get_config_version();
        }
        TRAN_INFO("fw:0x%02x bv:0x%02x", fw_ver_in_tp, fw_ver_in_fs);
    }
    if (pController->ops->get_algo_version && g_tran_ts.boot_mode == TRAN_NORMAL_MODE) {
        int len = 0;
        algo_version = pController->ops->get_algo_version();
        if (algo_version) {
            TRAN_INFO("algo version: %s", algo_version);
            if(strcmp("goodix",pController->vendor_name) == 0) {
                len = snprintf(buf,128,"0x%02x_0x%02x,bv:0x%02x_0x%02x  Algo:%s\n", 
                    fw_ver_in_tp, config_ver, fw_ver_in_tp, config_ver, algo_version);
            } else {
                len = snprintf(buf,128,"0x%02x,bv:0x%02x  Algo:%s\n", 
                    fw_ver_in_tp, fw_ver_in_fs, algo_version);
            }
        } else {
            goto exit;
        }
        if (pController->ops->get_algo_func) {
            algo_func = pController->ops->get_algo_func();
            if (algo_func) {
                len += snprintf(buf + len, 128 - len, "Func:%s\n", algo_func);
            }
        }
        TRAN_INFO("used size: %d, max size: 128", len);
        return len;
    }
exit:
    //BSP: add KJ7H895-2 for TP fw info by hongwu.zhan 20230821 start
    if(strcmp("goodix",pController->vendor_name) == 0) {
        return snprintf(buf,32,"0x%02x_0x%02x,bv:0x%02x_0x%02x\n", fw_ver_in_tp, config_ver, fw_ver_in_tp, config_ver);
    } else {
        return snprintf(buf,32,"0x%02x,bv:0x%02x\n", fw_ver_in_tp, fw_ver_in_fs);
    }
    //BSP: add KJ7H895-2 for TP fw info by hongwu.zhan 20230821 end
}

static ssize_t tp_firmware_version_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
    TRAN_INFO("tp_firmware_version_store");
    return count;
}

static DEVICE_ATTR(tp_firmware_version, 0644, tp_firmware_version_show, tp_firmware_version_store);

/* TP_INFO in midtest */
static inline int tran_toupper(int c) {
    if ((c >= 'a') && (c <= 'z'))
        return c + ('A' - 'a');
    return c;
}

static ssize_t tp_vendor_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    char ic_name[16] = {0};
    int ic_name_len = 0;
    int i;
    struct tran_ts_controller *pController = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];

    if(pController && pController->controller_init_state) {
        ic_name_len = strlen(pController->cur_ic)>16?16:strlen(pController->cur_ic);
        memcpy(ic_name, pController->cur_ic, ic_name_len);

        for(i=0; i<ic_name_len; i++) {
            ic_name[i] = tran_toupper((int)ic_name[i]);
        }

        if(pController->supplier != NULL) {
            TRAN_INFO("vendor_name:%s_%s_%02d", pController->supplier->supplier_vendor, \
                      ic_name, pController->supplier->supplier_num);
            return sprintf(buf,"%s_%s_%02d",pController->supplier->supplier_vendor, ic_name, pController->supplier->supplier_num);
        }
    }
    return sprintf(buf,"UNKNOWN");
}

static ssize_t tp_vendor_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)
{
    TRAN_FUNC_ENTER();
    return count;
}

static DEVICE_ATTR(tp_vendor, 0644, tp_vendor_show, tp_vendor_store);

static struct attribute *mtk_tp_info_attrs[] = {
    &dev_attr_tp_firmware_version.attr,
    &dev_attr_tp_vendor.attr,
    NULL
};

static const struct attribute_group g_mtk_paltform_group = {
    .attrs = mtk_tp_info_attrs,
};

int mtk_node_create(struct tran_ts_core *pdata)
{
    int ret;

    TRAN_FUNC_ENTER();

    ret = sysfs_create_group(&pdata->pdev->dev.kobj, &g_mtk_paltform_group);
    if (ret) {
        sysfs_remove_group(&pdata->pdev->dev.kobj, &g_mtk_paltform_group);
        return ret;
    }

    ret = sysfs_create_link(&platform_bus.kobj, &pdata->pdev->dev.kobj, "mtk-tpd");

    TRAN_FUNC_EXIT();
    return ret;
}
