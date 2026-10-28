#include <linux/power_supply.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>

#include "transsion_core.h"
#include "transsion_node.h"
#include "transsion_gesture.h"

extern struct tran_ts_core g_tran_ts;
extern struct tran_core_ops g_tpd_ops;

static void sub_tran_shutdown(struct platform_device * pdev)
{
    /* do nothing */
}

static void sub_tpd_remove(struct platform_device *pdev)
{
    /* do nothing */
}

const struct dev_pm_ops sub_tpd_pm_ops = {
    .suspend = NULL,
    .resume = NULL,
};

const struct of_device_id sub_touch_of_match[] = {
    { .compatible = "mediatek,touch-panel", },
    {},
};

static int tran_parse_sub_dt(struct device *dev, struct tran_ts_core *pdata)
{
    int i,len,ret;
    int supplier_num = 0;
    int controller_type = 0;
    /* Multiples of resolution */
    u32 resolution_multiples = 1;

    const char *supplier_info = NULL;
    const char *supplier_vendor= NULL;
    struct supplier_list *tmpList = NULL;
    const char *supplier_fw = NULL;
    const char *supplier_testfw = NULL;
    char supplierx[32] = {0};

    struct device_node *np = dev->of_node;

    TRAN_FUNC_ENTER();

    /* get tp supplier map */
    ret = of_property_read_u32(np, "supplier-num", &supplier_num);
    if(0 > ret) {
        TRAN_ERROR("[sub]: supplier-num: can not find!");
        return ret;
    }
    TRAN_INFO("[sub]: supplier-num: %d",supplier_num);

    /* get controller type */
    ret = of_property_read_u32(np, "controller_type", &controller_type);
    if(0 > ret) {
        TRAN_ERROR("[sub]: sub controller_type: can not find!");
        controller_type = 1;
    }
    TRAN_INFO("[sub]: controller_type: %d",controller_type);

    /* get tp supplier info */
    for(i=0; i<supplier_num; i++) {
        /* clear data */
        tmpList = NULL;
        supplier_info = NULL;
        supplier_vendor= NULL;
        memset(supplierx,0,32);

        /* get supplier info */
        snprintf(supplierx, sizeof("supplier-x"), "supplier-%d", i);
        supplier_info = of_get_property(np, (const char*)supplierx, &len);
        if(supplier_info == NULL) {
            TRAN_ERROR("[sub]: supplier-%d: can not find!", i);
            return ret;
        } else {
            TRAN_INFO("[sub]: supplier-%d: %s", i, supplier_info);
        }

        /* get fw info */
        snprintf(supplierx, sizeof("supplier-vendor-fwname-x"), "supplier-vendor-fwname-%d", i);
        supplier_fw = of_get_property(np, (const char*)supplierx, &len);
        if(IS_ERR_OR_NULL(supplier_fw)) {
           TRAN_ERROR("supplier-vendor-fwname-%d: can not find!", i);
           supplier_fw = NULL;
        } else {
            TRAN_INFO("supplier-vendor-fwname-%d: %s", i, supplier_fw);
         }
        /* get ini info */
        snprintf(supplierx, sizeof("supplier-vendor-testname-x"), "supplier-vendor-testname-%d", i);
        supplier_testfw = of_get_property(np, (const char*)supplierx, &len);
        if(IS_ERR_OR_NULL(supplier_testfw)) {
            TRAN_ERROR("supplier-vendor-testname-%d: can not find!", i);
            supplier_testfw = NULL;
         } else {
            TRAN_INFO("supplier-vendor-testname-%d: %s", i, supplier_testfw);
         }

        /* get Module factory */
        snprintf(supplierx, sizeof("supplier-vendor-x"), "supplier-vendor-%d", i);
        supplier_vendor = of_get_property(np, (const char*)supplierx, &len);
        if(supplier_vendor == NULL) {
            TRAN_INFO("[sub]: supplier-vendor-%d: can not find!", i);
        } else {
            TRAN_INFO("[sub]: supplier-vendor-%d: %s", i, supplier_vendor);
        }

        /* get Multiples of resolution */
        snprintf(supplierx, sizeof("supplier-resolution-x"), "supplier-resolution-%d", i);
        ret = of_property_read_u32(np, (const char*)supplierx, &resolution_multiples);
        if(0 > ret) {
            TRAN_ERROR("[sub]:supplier-resolution-%d: can not find!", i);
            resolution_multiples = 1;
        }
        TRAN_INFO("[sub]:supplier-resolution-%d: %d", i, resolution_multiples);

        /* request mem */
        tmpList = (struct supplier_list *)vmalloc(sizeof(struct supplier_list));
        if(tmpList == NULL) {
            TRAN_ERROR("[sub]:request supplier_list mem failed!");
            return ret;
        }

        /* init & add to list */
        tmpList->supplier_num = i;
        tmpList->supplier_info = (char *)supplier_info;
        tmpList->supplier_vendor = (char *)supplier_vendor;
        tmpList->resolution_multiples = resolution_multiples;
        tmpList->controller_type = controller_type;
        tmpList->is_matched = false;
        mutex_lock(&g_tran_ts.core_data_mutex);
        list_add_tail(&tmpList->next, &pdata->supplier_head);
        mutex_unlock(&g_tran_ts.core_data_mutex);
    }

    TRAN_FUNC_EXIT();

    return 0;
}

static int sub_tpd_probe(struct platform_device *pdev)
{
    struct tran_ts_platform *tran_pdata = g_tran_ts.platform_data;

    TRAN_FUNC_ENTER();
    TRAN_INFO("sub ctp probe start");

    if(tran_pdata == NULL) {
        TRAN_ERROR("[sub]: tran platform is NULL!");
        return 0;
    }

    /* parse dts */
    tran_parse_sub_dt(&pdev->dev, &g_tran_ts);

    g_tran_ts.sub_pdev = pdev;

    //fts_device_init(); 

    TRAN_FUNC_EXIT();
    return 0;
}

struct platform_driver sub_tpd_driver = {
    .remove = sub_tpd_remove,
    .shutdown = sub_tran_shutdown,
    .probe = sub_tpd_probe,
    .driver = {
        .name = TPD_SUB_DEVICE,
        .owner = THIS_MODULE,
        .of_match_table = sub_touch_of_match,
    },
};