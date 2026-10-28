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

//#include "mtk_panel_ext.h"
#include "mtk_disp_notify.h"
#include "transsion_core.h"

/* TP_INFO in midtest */
static ssize_t tp_firmware_version_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    u8 fw_ver_in_tp = 0;
    u8 fw_ver_in_fs = 0;
    u8 config_ver = 0;
    struct tran_ts_controller *pController = g_tran_ts.controller[TRAN_SUB_CONTROLLER];

    if(!pController || !pController->controller_init_state) {
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
    struct tran_ts_controller *pController = g_tran_ts.controller[TRAN_SUB_CONTROLLER];

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

//BSP: add main ctp_name node for ADSHBSE-94 by jiangzhu.yang 20221011 start
static ssize_t tp_name_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    struct tran_ts_controller *sub_pController = g_tran_ts.controller[TRAN_SUB_CONTROLLER];
    struct tran_ts_controller *pController = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];
    char main_icName[16] = {0};
    char sub_icName[16] = {0};
    int ic_name_len = 0;
    int i;

    /* get main ic name */
    if(pController && pController->controller_init_state) {
        ic_name_len = strlen(pController->cur_ic)>16?16:strlen(pController->cur_ic);
        memcpy(main_icName, pController->cur_ic, ic_name_len);

        for(i=0; i<ic_name_len; i++) {
            main_icName[i] = tran_toupper((int)main_icName[i]);
        }
    }

    /* get sub ic name */
    if(sub_pController && sub_pController->controller_init_state) {
        ic_name_len = strlen(sub_pController->cur_ic)>16?16:strlen(sub_pController->cur_ic);
        memcpy(sub_icName, sub_pController->cur_ic, ic_name_len);

        for(i=0; i<ic_name_len; i++) {
            sub_icName[i] = tran_toupper((int)sub_icName[i]);
        }
    }

    if(pController != NULL && sub_pController != NULL) {
        return sprintf(buf,"MAIN:%s_%s_%02d SUB:%s_%s_%02d", \
                       pController->supplier->supplier_vendor, main_icName, pController->supplier->supplier_num, \
                       sub_pController->supplier->supplier_vendor, sub_icName, sub_pController->supplier->supplier_num);
    } else if(pController) {
        return sprintf(buf,"MAIN:%s_%s_%02d SUB:UNKNOWN", \
                       pController->supplier->supplier_vendor, main_icName, pController->supplier->supplier_num);
    } else if(sub_pController) {
        return sprintf(buf,"MAIN:UNKNOWN SUB:%s_%s_%02d", \
                       sub_pController->supplier->supplier_vendor, sub_icName, sub_pController->supplier->supplier_num);
    } else {
        return sprintf(buf,"UNKNOWN");
    }
}

static ssize_t tp_name_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)
{
    return -EPERM;
}
//BSP: add main ctp_name node for ADSHBSE-94 by jiangzhu.yang 20221011 end

static DEVICE_ATTR(tp_vendor, 0644, tp_vendor_show, tp_vendor_store);

//BSP: add main ctp_name node for ADSHBSE-94 by jiangzhu.yang 20221011 start
/* must link mtk-tpd */
static DEVICE_ATTR(tp_name, 0644, tp_name_show, tp_name_store);
//BSP: add main ctp_name node for ADSHBSE-94 by jiangzhu.yang 20221011 end



static struct attribute *mtk_tp_info_attrs[] = {
    &dev_attr_tp_firmware_version.attr,
    &dev_attr_tp_vendor.attr,
    &dev_attr_tp_name.attr,
    NULL
};

static const struct attribute_group g_mtk_paltform_group = {
    .attrs = mtk_tp_info_attrs,
};


static int sub_disp_notifier_callback(struct notifier_block *nb, unsigned long value, void *v)
{
#if IS_ENABLED(CONFIG_TRAN_FOLD_DISPLAY)
    struct tran_ts_controller *sub_pController = g_tran_ts.controller[TRAN_SUB_CONTROLLER];
    int *data = (int *)v;

    if(!sub_pController || !sub_pController->controller_init_state) {
        TRAN_INFO("[sub]: vendor_probe_result is false!");
        return -1;
    }

    if (v) {
        if (value == MTK_DISP_EVENT_BLANK) {

            if (*data == TRAN_DISP_BLANK_AOD_NOTIFY_DSI1) {
                TRAN_INFO("[sub]: drm notify tp resume start");
                /*USD: modify for deadlock by guoxin.zhu 20240624 start*/
                mutex_lock(&sub_pController->dsp_event_mutex);
                if (!queue_work(g_tran_ts.tpd_workqueue, &g_tran_ts.tpd_sub_dsp_work)) {
                    TRAN_ERROR("[sub] resume work queue_work failed and unlock\n");
                    mutex_unlock(&sub_pController->dsp_event_mutex);
                }
                /*USD: modify for deadlock by guoxin.zhu 20240624 end*/
            }
        } else if (value == MTK_DISP_EARLY_EVENT_BLANK) {
            if (*data == TRAN_DISP_BLANK_POWERDOWN_DSI1) {
                TRAN_INFO("[sub]: drm notify tp suspends start");
                mutex_lock(&sub_pController->dsp_event_mutex);
                cancel_work_sync(&g_tran_ts.tpd_dsp_work);
                //BSP: modify XLWYEHLYEL-1 by hongwu.zhan 20221117 start
                state_change_machine(SUSPEND_TRIGER, sub_pController);/* must befor suspend */
                sub_pController->ops->tran_ts_suspend(NULL, 0);

                //BSP: modify XLWYEHLYEL-1 by hongwu.zhan 20221117 end
                mutex_unlock(&sub_pController->dsp_event_mutex);
                TRAN_INFO("[sub]: drm notify tp suspends end");
            }
        }
    } else {
        TRAN_ERROR("[sub]: wrong value v check notifier !!");
        return -1;
    }
#endif

    return 0;
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
static int mtk_register_sub_dsp_notify(struct tran_ts_core *pdata)
{
    int ret;

    pdata->sub_dsp_event_block.notifier_call = sub_disp_notifier_callback;
    ret = mtk_disp_notifier_register("Touch", &pdata->sub_dsp_event_block);
    if (ret) {
        TRAN_ERROR("[sub]: Failed to register disp notifier client:%d", ret);
    }
    return 0;
};
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
static int mtk_unregister_sub_dsp_notify(struct tran_ts_core *pdata)
{
    TRAN_FUNC_ENTER();
    if (mtk_disp_notifier_unregister(&pdata->sub_dsp_event_block))
        TRAN_ERROR("[sub]: Error occurred while unregistering disp_notifier.\n");
    TRAN_FUNC_EXIT();

    return 0;
};
#endif

static int mtk_sub_node_create(struct tran_ts_core *pdata)
{
    int ret;

    TRAN_FUNC_ENTER();

    ret = sysfs_create_group(&pdata->sub_pdev->dev.kobj, &g_mtk_paltform_group);
    if (ret) {
        sysfs_remove_group(&pdata->sub_pdev->dev.kobj, &g_mtk_paltform_group);
        return ret;
    }

    ret = sysfs_create_link(&platform_bus.kobj, &pdata->sub_pdev->dev.kobj, "mtk-tpd");

    TRAN_FUNC_EXIT();
    return ret;
}

int mtk_sub_platform_init(struct tran_ts_core *pdata)
{
    mtk_sub_node_create(pdata);
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
    mtk_register_sub_dsp_notify(pdata);
#endif
    return 0;
}

int mtk_sub_platform_deinit(struct tran_ts_core *pdata)
{
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
    mtk_unregister_sub_dsp_notify(pdata);
#endif
    return 0;
}
