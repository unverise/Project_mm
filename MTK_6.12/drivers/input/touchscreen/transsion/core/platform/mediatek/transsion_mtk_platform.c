#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include "mtk_disp_notify.h"
#include "transsion_core.h"

extern int power_supply_reg_notifier(struct notifier_block *nb);
extern int mtk_node_create(struct tran_ts_core *pdata);

struct tag_bootmode {
    u32 size;
    u32 tag;
    u32 bootmode;
    u32 boottype;
};

struct tran_videolfb {
    u64 fb_base;
    u32 islcmfound;
    u32 fps;
    u32 vram;
    char lcmname[1]; /* this is the minimum size */
};

enum mtk_boot_mode {
    MTK_NORMAL_BOOT = 0,
    MTK_META_BOOT,
    MTK_RECOVERY_BOOT,
    MTK_SW_REBOOT,
    MTK_FACTORY_BOOT,
    MTK_ADVMETA_BOOT,
    MTK_ATE_FACTORY_BOOT,
    MTK_ALARM_BOOT,
    MTK_KERNEL_POWER_OFF_CHARGING_BOOT,
    MTK_LOW_POWER_OFF_CHARGING_BOOT,
    MTK_DONGLE_BOOT,
};

static int mtk_power_misc_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
    struct power_supply *psy = v;
    union power_supply_propval val;
    union power_supply_propval prop = {0};
    static struct power_supply *chg_psy = NULL;
    int ret = 0;
    if (strcmp(psy->desc->name, "battery") == 0) {
        power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS,&val);
        if(1 == val.intval && 0 == g_tran_ts.charger_in) {
            TRAN_INFO("notif usb charger mode in");
            g_tran_ts.charger_in = 1;
            schedule_work(&g_tran_ts.tpd_usb_work);
        } else if(2 == val.intval && 1 == g_tran_ts.charger_in) {
            TRAN_INFO("notif charger mode out");
            g_tran_ts.charger_in = 0;
            schedule_work(&g_tran_ts.tpd_usb_work);
        }
    }
    if (IS_ERR_OR_NULL(chg_psy)) {
        chg_psy = power_supply_get_by_name("charger");
        if (IS_ERR_OR_NULL(chg_psy)) {
            TRAN_ERROR("Couldn't get charger psy");
            goto out;
        }
    }
    ret = power_supply_get_property(chg_psy, POWER_SUPPLY_PROP_TYPE, &prop);
    if(ret == 0 && prop.intval == POWER_SUPPLY_TYPE_WIRELESS) {
        TRAN_INFO("Wireless charging mode detected");
        g_tran_ts.charger_in = 2;
        schedule_work(&g_tran_ts.tpd_usb_work);
    }
out:
    return 0;
}

int mtk_register_charger_notify(struct tran_ts_core *pdata)
{
    g_tran_ts.usb_event_block.notifier_call = mtk_power_misc_psy_event;
    power_supply_reg_notifier(&pdata->usb_event_block);

    return 0;
};
int mtk_unregister_charger_notify(struct tran_ts_core *pdata)
{
    return 0;
};

static int nvt_disp_notifier_callback(struct notifier_block *nb, unsigned long value, void *v)
{
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
    struct tran_ts_controller *pController = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];
    int *data = (int *)v;

    if(!pController || !pController->controller_init_state) {
        TRAN_ERROR("[main]: vendor_probe_result is false!");
        return -1;
    }

    if (v) {
        if (value == MTK_DISP_EVENT_BLANK) {
            //if (*data == MTK_DISP_BLANK_UNBLANK) {//for normal
            if (*data == TRAN_DISP_BLANK_AOD_NOTIFY) {//for AOD
                TRAN_INFO("[main]: drm notify tp resume start");
                mutex_lock(&pController->dsp_event_mutex);
                /*USD: modify for deadlock by guoxin.zhu 20240624 start*/
                if (!queue_work(g_tran_ts.tpd_workqueue, &g_tran_ts.tpd_dsp_work)) {
                    TRAN_ERROR("[main] resume work queue_work failed and unlock\n");
                    mutex_unlock(&pController->dsp_event_mutex);
                }
                /*USD: modify for deadlock by guoxin.zhu 20240624 end*/
            } else if (*data == TRAN_DISP_BLANK_ENTER_LCD_AOD_NOTIFY) {
                mutex_lock(&pController->dsp_event_mutex);
                TRAN_INFO("[main]: drm notify lcd aod MTK_DISP_EVENT_BLANK start");
                pController->ops->set_ts_mode(LCD_AOD_MODE,1);
                mutex_unlock(&pController->dsp_event_mutex);
            }
        } else if (value == MTK_DISP_EARLY_EVENT_BLANK) {
            if (*data == MTK_DISP_BLANK_POWERDOWN) {
                TRAN_INFO("[main]: drm notify tp suspends start");
                mutex_lock(&pController->dsp_event_mutex);
                cancel_work_sync(&g_tran_ts.tpd_dsp_work);
                //BSP: modify XLWYEHLYEL-1 by hongwu.zhan 20221117 start
                state_change_machine(SUSPEND_TRIGER, pController);/* must befor suspend */
                pController->ops->tran_ts_suspend(NULL, 0);

                //BSP: modify XLWYEHLYEL-1 by hongwu.zhan 20221117 end
                mutex_unlock(&pController->dsp_event_mutex);
                TRAN_INFO("[main]: drm notify tp suspends end");
            } else if (*data == TRAN_DISP_BLANK_ENTER_LCD_AOD_NOTIFY) {
                mutex_lock(&pController->dsp_event_mutex);
                TRAN_INFO("[main]: drm notify lcd aod MTK_DISP_EARLY_EVENT_BLANK start");
                pController->ops->set_ts_mode(LCD_AOD_MODE_EARLY,1);
                mutex_unlock(&pController->dsp_event_mutex);
            }
        }
    } else {
        TRAN_ERROR("[main]: wrong value v check notifier !!");
        return -1;
    }
#endif

    return 0;
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
static int mtk_register_dsp_notify(struct tran_ts_core *pdata)
{
    int ret;

    pdata->dsp_event_block.notifier_call = nvt_disp_notifier_callback;
    ret = mtk_disp_notifier_register("Touch", &pdata->dsp_event_block);
    if (ret) {
        TRAN_ERROR("Failed to register disp notifier client:%d", ret);
    }
    return 0;
};
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
static int mtk_unregister_dsp_notify(struct tran_ts_core *pdata)
{
    if (mtk_disp_notifier_unregister(&pdata->dsp_event_block))
        TRAN_ERROR("Error occurred while unregistering disp_notifier.\n");

    return 0;
};
#endif

/*******************************************************
    Mobile phone startup mode list
    NORMAL_BOOT = 0,
    META_BOOT = 1,
    RECOVERY_BOOT = 2,
    SW_REBOOT = 3,
    FACTORY_BOOT = 4,
    ADVMETA_BOOT = 5,
    ATE_FACTORY_BOOT = 6,
    ALARM_BOOT = 7,
    KERNEL_POWER_OFF_CHARGING_BOOT = 8,
    LOW_POWER_OFF_CHARGING_BOOT = 9,
    FASTBOOT = 99,
    DOWNLOAD_BOOT = 100,
    UNKNOWN_BOOT

    return:boot_mode or -1
*******************************************************/
static int tran_tp_get_boot_mode(void)
{
    struct device_node *boot_node = NULL;
    struct tag_bootmode *tags = NULL;

    boot_node = of_find_node_by_path("/chosen");
    if(!boot_node) {
        TRAN_ERROR("tran_get_boot_mode error \n");
        return -1;
    } else {
        tags = (struct tag_bootmode *)of_get_property(boot_node,"atag,boot", NULL);
        if(tags) {
            TRAN_INFO("bootmode is %x",tags->bootmode);
            return tags->bootmode;
        } else {
            TRAN_ERROR("of_get_property bootmode error \n");
            return -1;
        }
    }
}

static int get_bootargs(char *current_mode, char *boot_param)
{
    struct device_node *np;
    const char *cmd_line;
    char *s = NULL;
    int ret = 0;
    int i = 0;

    if(IS_ERR_OR_NULL(current_mode))
        return -1;

    np = of_find_node_by_path("/chosen");
    if (IS_ERR_OR_NULL(np)) {
        TRAN_INFO("Can't get the /chosen");
        return -EIO;
    }
    if (!strcmp(boot_param, "lcm_name=")) {
        ret = of_property_read_string(np, "touch,lcm_name", &cmd_line);
        if (ret < 0) {
            TRAN_INFO("Can't get the touch,ret=%d", ret);
        } else {
            strcpy(current_mode, cmd_line);
            TRAN_INFO("get lcm_name: %s  len:%lu", cmd_line, strlen(cmd_line));
            return 0;
        }
    }
    ret = of_property_read_string(np, "bootargs", &cmd_line);
    if (ret < 0) {
        TRAN_INFO("Can't get the bootargs");
        return ret;
    }

    s = strstr(cmd_line, boot_param);
    if(!IS_ERR_OR_NULL(s)) {
        s += strlen(boot_param);
        while(*s != ' ' && i < 64) {
            *current_mode++ = *s++;
            i++;
        }
        *current_mode = '\0';
    } else {
	TRAN_INFO("Can't get the lcm_name=%d", ret);
        return -1;
    }

    return 0;
}

static int mtk_find_lcm_name(char * p_name)
{
    int ret = -1;
    if(!IS_ERR_OR_NULL(p_name)) {
        ret = get_bootargs(p_name,"lcm_name=");
    }

    return ret;
}

static int mtk_get_boot_mode(void)
{
    int boot_mode = -1;

    boot_mode = tran_tp_get_boot_mode();

    if (boot_mode == MTK_KERNEL_POWER_OFF_CHARGING_BOOT || boot_mode == MTK_LOW_POWER_OFF_CHARGING_BOOT
            || boot_mode == MTK_RECOVERY_BOOT ) {
        TRAN_ERROR("bootmode=%d don't load touch !", boot_mode);
        return TRAN_NOT_RUN_MODE;
    }

    if(boot_mode == MTK_META_BOOT \
            || boot_mode == MTK_FACTORY_BOOT \
            || boot_mode == MTK_ATE_FACTORY_BOOT) {
        return TRAN_FACTORY_MODE;
    }

    return TRAN_NORMAL_MODE;
}

static int mtk_plat_init(struct tran_ts_core *pdata) {
    TRAN_FUNC_ENTER();

    /* dsp register notify */
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
    mtk_register_dsp_notify(pdata);
#endif

    /* usb register notify */
    mtk_register_charger_notify(pdata);

    /* create node for mtk */
    mtk_node_create(pdata);

    TRAN_FUNC_EXIT();
    return 0;
}
static int mtk_plat_deinit(struct tran_ts_core *pdata) {

    TRAN_FUNC_ENTER();
    /* unregister dsp notify */
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
    mtk_unregister_dsp_notify(pdata);
#endif
    /* unregister usb notify */
    mtk_unregister_charger_notify(pdata);

    /* remove node */

    TRAN_FUNC_EXIT();
    return 0;
}

extern int mtk_sub_platform_init(struct tran_ts_core *pdata);
extern int mtk_sub_platform_deinit(struct tran_ts_core *pdata);
extern struct athena_msg* alloc_msg(void);
extern void free_msg(struct athena_msg **athena);
extern void submit_msg(struct athena_msg **athena, int version, long long tid, char *tag);
extern void msg_put_string(struct athena_msg **athena, char *key, char *value);
extern void msg_put_int(struct athena_msg **athena, char *key, int value);
extern void msg_put_long(struct athena_msg **athena, char *key, long value);

void get_tp_ic(char * buf)
{
    int ic_name_len = 0;

    if(!IS_ERR_OR_NULL(g_tran_ts.controller))
    {
        ic_name_len = strlen(g_tran_ts.controller[0]->cur_ic)>16?16:strlen(g_tran_ts.controller[0]->cur_ic);
        memcpy(buf, g_tran_ts.controller[0]->cur_ic, ic_name_len);
    }
}

void get_tp_module(char * buf)
{

	if (IS_ERR_OR_NULL(buf)) {
		return;
	}

	if(IS_ERR_OR_NULL(g_tran_ts.controller) || 
		IS_ERR_OR_NULL(g_tran_ts.controller[0]->supplier) ||
		IS_ERR_OR_NULL(g_tran_ts.controller[0]->supplier->supplier_vendor)){
		return;
	}
	sprintf(buf,"%s",g_tran_ts.controller[0]->supplier->supplier_vendor);
}

int bigdata_tp_mistouch(void)
{
    long long tid = 901061000012;
    struct athena_msg *msg = NULL;
    char ic_name[16] = {0};
    char tp_module[16] = {0};

    get_tp_ic(ic_name);
    get_tp_module(tp_module);
    msg = alloc_msg();
    msg_put_int(&msg, "tt", g_tran_ts.bigdata_tt);
    msg_put_int(&msg, "mt", g_tran_ts.bigdata_mt);
    msg_put_string(&msg, "ic", ic_name);
    msg_put_string(&msg, "md", tp_module);
    msg_put_string(&msg, "tt1", "");
    msg_put_string(&msg, "tt2", "");

    submit_msg(&msg, 1, tid,"tp_mistouchs");
    free_msg(&msg);
    TRAN_INFO("bigdata_tp_mistouch report tt=%d mt=%d", g_tran_ts.bigdata_tt, g_tran_ts.bigdata_mt);
    return 0;
}

static struct tran_platform_ops mtk_plat_ops = {
    .plat_init = mtk_plat_init,
    .plat_deinit = mtk_plat_deinit,
    .get_lcm_name = mtk_find_lcm_name,
    .get_boot_mode= mtk_get_boot_mode,
    .sub_plat_init = mtk_sub_platform_init,
    .sub_plat_deinit = mtk_sub_platform_deinit,
    .bigdata_tp_mistouch = bigdata_tp_mistouch,
};

static struct tran_ts_platform mtk_plat = {
    .ops = &mtk_plat_ops,
};

void init_tran_platform_info(void)
{
    g_tran_ts.platform_data = &mtk_plat;
}
