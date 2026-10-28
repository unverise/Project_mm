#define LOG_TAG         "CTS-drv"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sysfs.h"
#include "cts_charger_detect.h"
#include "cts_earjack_detect.h"
#include "cts_oem.h"
#include "cts_transfer.h"
#include "cts_test.h"

#ifdef CFG_CTS_DRM_NOTIFIER
#include <drm/drm_panel.h>
static struct drm_panel *active_panel;
static int check_dt(struct device_node *np);
#endif
bool cts_show_debug_log;
#ifdef CTS_MTK_GET_PANEL
static char *active_panel_name;
#endif

module_param_named(debug_log, cts_show_debug_log, bool, 0660);
MODULE_PARM_DESC(debug_log, "Show debug log control");

struct chipone_ts_data *g_cts_data;

static void cts_resume_work_func(struct work_struct *work);

int cts_suspend(struct chipone_ts_data *cts_data)
{
    int ret;

    cts_info("Suspend");
	
    //USD:modify for lcd aod by guoli.niu 20250321 start
    if (cts_data->lcd_aod_flag) {
        set_lcd_aod_mode(0);
    }
    //USD:modify for lcd aod by guoli.niu 20250321 end

    cts_lock_device(&cts_data->cts_dev);
    ret = cts_suspend_device(&cts_data->cts_dev);
    cts_unlock_device(&cts_data->cts_dev);

    if (ret)
        cts_err("Suspend device failed %d", ret);

    ret = cts_stop_device(&cts_data->cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        return ret;
    }
#ifdef CFG_CTS_GESTURE
    /* Enable IRQ wake if gesture wakeup enabled */
    if (cts_is_gesture_wakeup_enabled(&cts_data->cts_dev)) {
        ret = cts_plat_enable_irq_wake(cts_data->pdata);
        if (ret) {
            cts_err("Enable IRQ wake failed %d", ret);
            return ret;
        }
        ret = cts_plat_enable_irq(cts_data->pdata);
        if (ret) {
            cts_err("Enable IRQ failed %d", ret);
            return ret;
        }
    }
#endif /* CFG_CTS_GESTURE */

/** - To avoid waking up while not sleeping,
 *delay 20ms to ensure reliability
 */
    msleep(20);

    return 0;
}

int cts_resume(struct chipone_ts_data *cts_data)
{
    int ret;

    cts_info("Resume");
#ifdef CFG_CTS_GESTURE
    if (cts_is_gesture_wakeup_enabled(&cts_data->cts_dev)) {
        //USD:modify for lcd aod by guoli.niu 20250321 start
        if (cts_data->lcd_aod_flag) {
            set_lcd_aod_mode(0);
        }
        //USD:modify for lcd aod by guoli.niu 20250321 end
        ret = cts_plat_disable_irq_wake(cts_data->pdata);
        if (ret)
            cts_warn("Disable IRQ wake failed %d", ret);
        ret = cts_plat_disable_irq(cts_data->pdata);
        if (ret < 0)
            cts_err("Disable IRQ failed %d", ret);
    }
#endif /* CFG_CTS_GESTURE */

    cts_lock_device(&cts_data->cts_dev);
    ret = cts_resume_device(&cts_data->cts_dev);
    cts_unlock_device(&cts_data->cts_dev);
    if (ret) {
        cts_warn("Resume device failed %d", ret);
        return ret;
    }

    ret = cts_start_device(&cts_data->cts_dev);
    if (ret) {
        cts_err("Start device failed %d", ret);
        return ret;
    }

    return 0;
}

static void cts_resume_work_func(struct work_struct *work)
{
    struct chipone_ts_data *cts_data =
    container_of(work, struct chipone_ts_data, ts_resume_work);
    cts_info("%s", __func__);
    cts_resume(cts_data);
}

#ifdef CONFIG_CTS_PM_FB_NOTIFIER
#ifdef CFG_CTS_DRM_NOTIFIER
static int fb_notifier_callback(struct notifier_block *nb,
        unsigned long action, void *data)
{
    volatile int blank;
    const struct cts_platform_data *pdata =
    container_of(nb, struct cts_platform_data, fb_notifier);
    struct chipone_ts_data *cts_data =
    container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
    struct drm_panel_notifier *evdata = data;

    cts_info("FB notifier callback");
    if (!evdata || !cts_data)
        return 0;

    blank = *(int *)evdata->data;
    cts_info("action=%lu, blank=%d\n", action, blank);

    if (action == DRM_PANEL_EARLY_EVENT_BLANK) {
        if (blank == DRM_PANEL_BLANK_POWERDOWN)
            cts_suspend(cts_data);
    } else if (evdata->data) {
        blank = *(int *)evdata->data;
        if (action == DRM_PANEL_EVENT_BLANK) {
            if (blank == DRM_PANEL_BLANK_UNBLANK)
                /* cts_resume(cts_data); */
                queue_work(cts_data->workqueue,
                    &cts_data->ts_resume_work);
        }
    }

    return 0;
}
#else
static int fb_notifier_callback(struct notifier_block *nb,
        unsigned long action, void *data)
{
    volatile int blank;
    const struct cts_platform_data *pdata =
    container_of(nb, struct cts_platform_data, fb_notifier);
    struct chipone_ts_data *cts_data =
    container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
    struct fb_event *evdata = data;

    cts_info("FB notifier callback");
    if (action != FB_EVENT_BLANK)
        return 0;
    blank = *(int *)evdata->data;
    switch (blank) {
    case FB_BLANK_UNBLANK:
        /* cts_resume(cts_data); */
        queue_work(cts_data->workqueue, 
            &cts_data->ts_resume_work);
        break;
    case FB_BLANK_POWERDOWN:
        cts_suspend(cts_data);
        break;
    default:
        cts_info("FB blank event %d", blank);
        break;
    }
    return NOTIFY_OK;
}
#endif

static int cts_init_pm_fb_notifier(struct chipone_ts_data *cts_data)
{
    cts_info("Init FB notifier");

    cts_data->pdata->fb_notifier.notifier_call = fb_notifier_callback;

#ifdef CFG_CTS_DRM_NOTIFIER
    {
        int ret = -ENODEV;

        if (active_panel) {
            ret =drm_panel_notifier_register(active_panel,
                    &cts_data->pdata->fb_notifier);
            if (ret)
                cts_err("register drm_notifier failed. ret=%d\n", ret);
        }
        return ret;
    }
#else
    return fb_register_client(&cts_data->pdata->fb_notifier);
#endif
}

static int cts_deinit_pm_fb_notifier(struct chipone_ts_data *cts_data)
{
    cts_info("Deinit FB notifier");
#ifdef CFG_CTS_DRM_NOTIFIER
    {
        int ret = 0;

        if (active_panel) {
            ret = drm_panel_notifier_unregister(active_panel,
                    &cts_data->pdata->fb_notifier);
            if (ret)
                cts_err("Error occurred while unregistering drm_notifier.\n");
        }
        return ret;
    }
#else
    return fb_unregister_client(&cts_data->pdata->fb_notifier);
#endif
}
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */

#ifdef CFG_CTS_DRM_NOTIFIER
static int check_dt(struct device_node *np)
{
    int i;
    int count;
    struct device_node *node;
    struct drm_panel *panel;

    count = of_count_phandle_with_args(np, "panel", NULL);
    if (count <= 0)
        return 0;

    for (i = 0; i < count; i++) {
        node = of_parse_phandle(np, "panel", i);
        panel = of_drm_find_panel(node);
        of_node_put(node);
        if (!IS_ERR(panel)) {
            cts_info("check active_panel");
            active_panel = panel;
            return 0;
        }
    }
    if (node)
        cts_err("%s: %s not actived", __func__, node->name);
    return -ENODEV;
}

static int check_default_tp(struct device_node *dt, const char *prop)
{
    const char *active_tp;
    const char *compatible;
    char *start;
    int ret;

    ret = of_property_read_string(dt->parent, prop, &active_tp);
    if (ret) {
        cts_err("%s:fail to read %s %d", __func__, prop, ret);
        return -ENODEV;
    }

    ret = of_property_read_string(dt, "compatible", &compatible);
    if (ret < 0) {
        cts_err("%s:fail to read %s %d", __func__, "compatible", ret);
        return -ENODEV;
    }

    start = strnstr(active_tp, compatible, strlen(active_tp));
    if (start == NULL) {
        cts_err("no match compatible, %s, %s", compatible, active_tp);
        ret = -ENODEV;
    }

    return ret;
}
#endif

#ifdef CTS_MTK_GET_PANEL
char panel_name[50] = { 0 };

static int cts_get_panel(void)
{
    int ret = -1;

    cts_info("Enter cts_get_panel");
    if (saved_command_line) {
        char *sub;
        char key_prefix[] = "mipi_mot_vid_";
        char ic_prefix[] = "icnl";

        cts_info("saved_command_line is %s", saved_command_line);
        sub = strstr(saved_command_line, key_prefix);
        if (sub) {
            char *d;
            int n, len, len_max = 50;

            d = strstr(sub, " ");
            if (d)
                n = strlen(sub) - strlen(d);
            else
                n = strlen(sub);

            if (n > len_max)
                len = len_max;
            else
                len = n;

            strncpy(panel_name, sub, len);
            active_panel_name = panel_name;
            if (strstr(active_panel_name, ic_prefix))
                cts_info("active_panel_name=%s", active_panel_name);
            else {
                cts_info("Not chipone panel!");
                return ret;
            }

        } else {
            cts_info("chipone active panel not found!");
            return ret;
        }
    } else {
        cts_info("saved_command_line null!");
        return ret;
    }

    return 0;
}
#endif

static int _cts_driver_probe(struct i2c_client *i2c_client,
    struct spi_device *spi_client, enum cts_bus_type bus_type)
{
    struct chipone_ts_data *cts_data = NULL;
    int ret = 0;
	/*bsp:add for transsion core by guoxin.zhu 20230515 start*/
    struct tran_core_ops *ops = NULL;
    /*bsp:add for transsion core by guoxin.zhu 20230515 end*/

    cts_data = kzalloc(sizeof(struct chipone_ts_data), GFP_KERNEL);
    if (cts_data == NULL) {
        cts_err("Allocate chipone_ts_data failed");
        return -ENOMEM;
    }
    cts_data->pdata = kzalloc(sizeof(struct cts_platform_data), GFP_KERNEL);
    if (cts_data->pdata == NULL) {
        cts_err("Allocate cts_platform_data failed");
        ret = -ENOMEM;
        goto err_free_cts_data;
    }

    if (bus_type == CTS_BUS_SPI) {
        spi_set_drvdata(spi_client, cts_data);
        cts_data->spi_client = spi_client;
        cts_data->device = &spi_client->dev;
        cts_spi_init_platform_data(cts_data->pdata, spi_client);
    } else if (bus_type == CTS_BUS_I2C) {
        i2c_set_clientdata(i2c_client, cts_data);
        cts_data->i2c_client = i2c_client;
        cts_data->device = &i2c_client->dev;
        cts_i2c_init_platform_data(cts_data->pdata, i2c_client);
    } else {
        cts_err("unsupported bus type:%d\n", bus_type);
        goto err_free_cts_data;
    }
    cts_data->cts_dev.bus_type = bus_type;

    cts_data->cts_dev.pdata = cts_data->pdata;
    cts_data->pdata->cts_dev = &cts_data->cts_dev;

    cts_data->workqueue =
    create_singlethread_workqueue(CFG_CTS_DEVICE_NAME "-workqueue");
    if (cts_data->workqueue == NULL) {
        cts_err("Create workqueue failed");
        ret = -ENOMEM;
        goto err_deinit_platform_data;
    }
#ifdef CONFIG_CTS_ESD_PROTECTION
    cts_data->esd_workqueue =
    create_singlethread_workqueue(CFG_CTS_DEVICE_NAME "-esd_workqueue");
    if (cts_data->esd_workqueue == NULL) {
        cts_err("Create esd workqueue failed");
        ret = -ENOMEM;
        goto err_destroy_workqueue;
    }
#endif

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    cts_data->heart_workqueue =
        create_singlethread_workqueue(CFG_CTS_DEVICE_NAME "-heart_workqueue");
    if (cts_data->heart_workqueue == NULL) {
        cts_err("Create heart workqueue failed");
        ret = -ENOMEM;
        goto err_destroy_esd_workqueue;
    }
#endif

    ret = cts_plat_request_resource(cts_data->pdata);
    if (ret < 0) {
        cts_err("Request resource failed %d", ret);
        goto err_destroy_heart_workqueue;
    }

    ret = cts_probe_device(&cts_data->cts_dev);
    if (ret) {
        cts_err("Probe device failed %d", ret);
        goto err_free_resource;
    }

    /*bsp:add for gesture by guoxin.zhu 20231009 start*/ 
	 device_init_wakeup(cts_data->device, 1);	
    /*bsp:add for gesture by guoxin.zhu 20231009 end*/
	
    cts_post_init_platform_data(cts_data->pdata);

    ret = cts_plat_init_touch_device(cts_data->pdata);
    if (ret < 0) {
        cts_err("Init touch device failed %d", ret);
        goto err_deinit_hwdata_rtdata;
    }

    ret = cts_plat_init_vkey_device(cts_data->pdata);
    if (ret < 0) {
        cts_err("Init vkey device failed %d", ret);
        goto err_deinit_touch_device;
    }

    ret = cts_plat_init_gesture(cts_data->pdata);
    if (ret < 0) {
        cts_err("Init gesture failed %d", ret);
        goto err_deinit_vkey_device;
    }

    cts_init_esd_protection(cts_data);

    ret = cts_tool_init(cts_data);
    if (ret < 0)
        cts_warn("Init tool node failed %d", ret);

    if (bus_type == CTS_BUS_SPI)
        ret = cts_sysfs_add_device(&spi_client->dev);
    else if (bus_type == CTS_BUS_I2C)
        ret = cts_sysfs_add_device(&i2c_client->dev);
    if (ret < 0)
        cts_warn("Add sysfs entry for device failed %d", ret);

#ifdef CONFIG_CTS_PM_FB_NOTIFIER
    ret = cts_init_pm_fb_notifier(cts_data);
    if (ret) {
        cts_err("Init FB notifier failed %d", ret);
        goto err_deinit_sysfs;
    }
#endif

    ret = cts_plat_request_irq(cts_data->pdata);
    if (ret < 0) {
        cts_err("Request IRQ failed %d", ret);
        goto err_register_fb;
    }

#ifdef CONFIG_CTS_CHARGER_DETECT
    ret = cts_charger_detect_init(cts_data);
    if (ret)
        cts_err("Init charger detect failed %d", ret);
        /* Ignore this error */
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    ret = cts_earjack_detect_init(cts_data);
    if (ret) {
        cts_err("Init earjack detect failed %d", ret);
        // Ignore this error
    }
#endif

    ret = cts_init_test_vals(&cts_data->cts_dev, cts_data->cts_dev.test_vals);
    if (ret < 0) {
        cts_warn("Init test data faild %d", ret);
//        goto err_deinit_test_val;
    }

    ret = cts_oem_init(cts_data);
    if (ret < 0) {
        cts_warn("Init oem specific faild %d", ret);
        goto err_deinit_test_val;
    }

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    INIT_DELAYED_WORK(&cts_data->heart_work, cts_heartbeat_mechanism_work);
#endif

    /* Init firmware upgrade work and schedule */
    INIT_DELAYED_WORK(&cts_data->fw_upgrade_work, cts_firmware_upgrade_work);
	
/*USD:modify for sprd tps test by guoxin.zhu 20230919 start*/
#if defined(CONFIG_MTK_PLATFORM)
    if (TRAN_FACTORY_MODE == chipone_controller.tran_ts_data->boot_mode) {
        queue_delayed_work(cts_data->workqueue, &cts_data->fw_upgrade_work,
                           msecs_to_jiffies(3000));
    } else {
        queue_delayed_work(cts_data->workqueue, &cts_data->fw_upgrade_work,
                           msecs_to_jiffies(CTS_TOUCH_UPDATE_FW_DELAY));
    }
#else
    queue_delayed_work(cts_data->workqueue, &cts_data->fw_upgrade_work,
                       msecs_to_jiffies(CTS_TOUCH_UPDATE_FW_DELAY));
#endif
/*USD:modify for sprd tps test by guoxin.zhu 20230919 end*/

    INIT_WORK(&cts_data->ts_resume_work, cts_resume_work_func);
    /*bsp:add for transsion core by guoxin.zhu 20230515 start*/
    if(get_tpd_ops(&chipone_controller, &ops)) {
        ops->controller_init_completion(&chipone_controller);
    }
    /*bsp:add for transsion core by guoxin.zhu 20230515 end*/

    g_cts_data = cts_data;

    cts_info("Probe successfully\n");
    return 0;

err_deinit_test_val:
    cts_oem_deinit(cts_data);

#ifdef CONFIG_CTS_EARJACK_DETECT
    cts_earjack_detect_deinit(cts_data);
#endif
#ifdef CONFIG_CTS_CHARGER_DETECT
    cts_charger_detect_deinit(cts_data);
#endif
    cts_plat_free_irq(cts_data->pdata);

err_register_fb:
#ifdef CONFIG_CTS_PM_FB_NOTIFIER
    cts_deinit_pm_fb_notifier(cts_data);
err_deinit_sysfs:
#endif

    if (bus_type == CTS_BUS_SPI)
        cts_sysfs_remove_device(&spi_client->dev);
    else if (bus_type == CTS_BUS_I2C)
        cts_sysfs_remove_device(&i2c_client->dev);

#ifdef CONFIG_CTS_LEGACY_TOOL
    cts_tool_deinit(cts_data);
#endif

#ifdef CONFIG_CTS_ESD_PROTECTION
    cts_deinit_esd_protection(cts_data);
#endif

#ifdef CFG_CTS_GESTURE
    cts_plat_deinit_gesture(cts_data->pdata);
#endif

err_deinit_vkey_device:
#ifdef CONFIG_CTS_VIRTUALKEY
    cts_plat_deinit_vkey_device(cts_data->pdata);
#endif

err_deinit_touch_device:
    cts_plat_deinit_touch_device(cts_data->pdata);

err_deinit_hwdata_rtdata:
    cts_deinit_device(&cts_data->cts_dev);

err_free_resource:
    cts_plat_free_resource(cts_data->pdata);

err_destroy_heart_workqueue:
#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    destroy_workqueue(cts_data->heart_workqueue);
err_destroy_esd_workqueue:
#endif

#ifdef CONFIG_CTS_ESD_PROTECTION
    destroy_workqueue(cts_data->esd_workqueue);
err_destroy_workqueue:
#endif
    destroy_workqueue(cts_data->workqueue);
err_deinit_platform_data:
    cts_deinit_platform_data(cts_data->pdata);
    kfree(cts_data->pdata);
err_free_cts_data:
    kfree(cts_data);
    g_cts_data = NULL;

    cts_err("Probe failed %d", ret);
    return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
static int cts_i2c_driver_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
#else
static int cts_i2c_driver_probe(struct i2c_client *client)
#endif
{
    int ret = 0;

    cts_info("Probe i2c client: name='%s' addr=0x%02x, flags=0x%02x, irq=%d",
        client->name, client->addr, client->flags, client->irq);

#ifdef CTS_MTK_GET_PANEL
    ret = cts_get_panel();
    if (ret) {
        cts_info("MTK get chipone panel error");
        return ret;
    }
#endif

#ifdef CFG_CTS_DRM_NOTIFIER
    struct device_node *dp = client->dev.of_node;
    if (check_dt(dp)) {
        if (!check_default_tp(dp, "qcom,i2c-touch-active"))
            ret = -EPROBE_DEFER;
        else
            ret = -ENODEV;

        cts_err("%s: %s not actived\n", __func__, dp->name);
        return ret;
    }
#endif

    ret = _cts_driver_probe(client, NULL, CTS_BUS_I2C);

    return ret;
}

static int cts_spi_driver_probe(struct spi_device *client)
{
    int ret = 0;

    cts_info("Probe spi device, irq=%d\n", client->irq);

#ifdef CTS_MTK_GET_PANEL
    ret = cts_get_panel();
    if (ret) {
        cts_info("MTK get chipone panel error");
        return ret;
    }
#endif

    ret = _cts_driver_probe(NULL, client, CTS_BUS_SPI);

    return ret;
}

static int _cts_driver_remove(struct chipone_ts_data *cts_data,
    void *client)
{
    struct i2c_client *i2c_client = client;
    struct spi_device *spi_device = client;
    int ret;

    if (cts_data) {
        ret = cts_stop_device(&cts_data->cts_dev);
        if (ret)
            cts_warn("Stop device failed %d", ret);

#ifdef CONFIG_CTS_CHARGER_DETECT
        cts_charger_detect_deinit(cts_data);
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
        cts_earjack_detect_deinit(cts_data);
#endif

        cts_plat_free_irq(cts_data->pdata);

#ifdef CONFIG_CTS_PM_FB_NOTIFIER
        cts_deinit_pm_fb_notifier(cts_data);
#endif

		/*bsp:add for gesture by guoxin.zhu 20231009 start*/
		device_init_wakeup(cts_data->device, 0);
        /*bsp:add for gesture by guoxin.zhu 20231009 end*/

        cts_tool_deinit(cts_data);

        if (cts_data->cts_dev.bus_type == CTS_BUS_I2C)
            cts_sysfs_remove_device(&i2c_client->dev);
        else if (cts_data->cts_dev.bus_type == CTS_BUS_SPI)
            cts_sysfs_remove_device(&spi_device->dev);

        cts_deinit_esd_protection(cts_data);

        cts_plat_deinit_touch_device(cts_data->pdata);

        cts_plat_deinit_vkey_device(cts_data->pdata);

        cts_plat_deinit_gesture(cts_data->pdata);

        cts_plat_free_resource(cts_data->pdata);

        cts_oem_deinit(cts_data);

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
        if (cts_data->heart_workqueue)
            destroy_workqueue(cts_data->heart_workqueue);
#endif

#ifdef CONFIG_CTS_ESD_PROTECTION
        if (cts_data->esd_workqueue)
            destroy_workqueue(cts_data->esd_workqueue);
#endif

        if (cts_data->workqueue)
            destroy_workqueue(cts_data->workqueue);

        cts_deinit_platform_data(cts_data->pdata);

        if (cts_data->pdata)
            kfree(cts_data->pdata);
        kfree(cts_data);
    } else {
        cts_warn("Chipone driver remove while NULL chipone_ts_data");
        ret = -EINVAL;
    }

    return ret;
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
static int cts_i2c_driver_remove(struct i2c_client *client)
#else
static void cts_i2c_driver_remove(struct i2c_client *client)
#endif
{
    struct chipone_ts_data *cts_data;

    cts_info("Remove i2c driver");
    cts_data = (struct chipone_ts_data *)i2c_get_clientdata(client);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
    return _cts_driver_remove(cts_data, client);
#else
    _cts_driver_remove(cts_data, client);
#endif

}
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
static int cts_spi_driver_remove(struct spi_device *client)
#else
static void cts_spi_driver_remove(struct spi_device *client)
#endif
{
    struct chipone_ts_data *cts_data;

    cts_info("Remove spi driver");
    cts_data = (struct chipone_ts_data *)spi_get_drvdata(client);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
    return _cts_driver_remove(cts_data, client);
#else
    _cts_driver_remove(cts_data, client);
#endif
}

#ifdef CONFIG_CTS_PM_LEGACY
static int cts_driver_suspend(struct device *dev, pm_message_t state)
{
    cts_info("Suspend by legacy power management");
    return cts_suspend(dev_get_drvdata(dev));
}

static int cts_driver_resume(struct device *dev)
{
    cts_info("Resume by legacy power management");
    return cts_resume(dev_get_drvdata(dev));
}
#endif /* CONFIG_CTS_PM_LEGACY */

#ifdef CONFIG_CTS_PM_GENERIC
static int cts_driver_pm_suspend(struct device *dev)
{
    cts_info("Suspend by bus power management");
    return cts_suspend(dev_get_drvdata(dev));
}

static int cts_driver_pm_resume(struct device *dev)
{
    cts_info("Resume by bus power management");
    return cts_resume(dev_get_drvdata(dev));
}

/* bus control the suspend/resume procedure */
static const struct dev_pm_ops cts_driver_pm_ops = {
    .suspend = cts_driver_pm_suspend,
    .resume = cts_driver_pm_resume,
};
#endif /* CONFIG_CTS_PM_GENERIC */

/*bsp:modify for deel sleep by guoxin.zhu 20230808 start*/
#if CTS_PATCH_COMERR_PM
static int cts_pm_suspend(struct device *dev)
{
    struct cts_device *cts_dev = &(g_cts_data->cts_dev);

    cts_info("system enters into pm_suspend");
    cts_dev->rtdata.pm_suspend = true;
    reinit_completion(&(cts_dev->rtdata.pm_completion));
    return 0;
}

static int cts_pm_resume(struct device *dev)
{
    struct cts_device *cts_dev = &(g_cts_data->cts_dev);

    cts_info("system resumes from pm_suspend");
    cts_dev->rtdata.pm_suspend = false;
    complete(&(cts_dev->rtdata.pm_completion));
    return 0;
}

static const struct dev_pm_ops cts_dev_pm_ops = {
    .suspend = cts_pm_suspend,
    .resume = cts_pm_resume,
};
#endif
/*bsp:modify for deel sleep by guoxin.zhu 20230808 end*/

#ifdef CONFIG_CTS_SYSFS
static ssize_t reset_pin_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_HAS_RESET_PIN: %c\n",
#ifdef CFG_CTS_HAS_RESET_PIN
            'Y'
#else
            'N'
#endif
    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(reset_pin, S_IRUGO, reset_pin_show, NULL);
#else
static DRIVER_ATTR_RO(reset_pin);
#endif

static ssize_t swap_xy_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_SWAP_XY: %c\n",
#ifdef CFG_CTS_SWAP_XY
            'Y'
#else
            'N'
#endif
    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(swap_xy, S_IRUGO, swap_xy_show, NULL);
#else
static DRIVER_ATTR_RO(swap_xy);
#endif

static ssize_t wrap_x_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_WRAP_X: %c\n",
#ifdef CFG_CTS_WRAP_X
            'Y'
#else
            'N'
#endif
    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(wrap_x, S_IRUGO, wrap_x_show, NULL);
#else
static DRIVER_ATTR_RO(wrap_x);
#endif

static ssize_t wrap_y_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_WRAP_Y: %c\n",
#ifdef CFG_CTS_WRAP_Y
            'Y'
#else
            'N'
#endif
    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(wrap_y, S_IRUGO, wrap_y_show, NULL);
#else
static DRIVER_ATTR_RO(wrap_y);
#endif

static ssize_t force_update_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_HAS_RESET_PIN: %c\n",
#ifdef CFG_CTS_FIRMWARE_FORCE_UPDATE
            'Y'
#else
            'N'
#endif
    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(force_update, S_IRUGO, force_update_show, NULL);
#else
static DRIVER_ATTR_RO(force_update);
#endif

static ssize_t max_touch_num_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_MAX_TOUCH_NUM: %d\n",
            CFG_CTS_MAX_TOUCH_NUM);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(max_touch_num, S_IRUGO, max_touch_num_show, NULL);
#else
static DRIVER_ATTR_RO(max_touch_num);
#endif

static ssize_t vkey_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_VIRTUALKEY: %c\n",
#ifdef CONFIG_CTS_VIRTUALKEY
            'Y'
#else
            'N'
#endif
    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(vkey, S_IRUGO, vkey_show, NULL);
#else
static DRIVER_ATTR_RO(vkey);
#endif

static ssize_t gesture_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_GESTURE: %c\n",
#ifdef CFG_CTS_GESTURE
            'Y'
#else
            'N'
#endif
    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(gesture, S_IRUGO, gesture_show, NULL);
#else
static DRIVER_ATTR_RO(gesture);
#endif

static ssize_t esd_protection_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_ESD_PROTECTION: %c\n",
#ifdef CONFIG_CTS_ESD_PROTECTION
            'Y'
#else
            'N'
#endif
    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(esd_protection, S_IRUGO, esd_protection_show, NULL);
#else
static DRIVER_ATTR_RO(esd_protection);
#endif

static ssize_t slot_protocol_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_SLOTPROTOCOL: %c\n",
#ifdef CONFIG_CTS_SLOTPROTOCOL
            'Y'
#else
            'N'
#endif
    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(slot_protocol, S_IRUGO, slot_protocol_show, NULL);
#else
static DRIVER_ATTR_RO(slot_protocol);
#endif

static ssize_t max_xfer_size_show(struct device_driver *dev, char *buf)
{
#ifdef CONFIG_CTS_I2C_HOST
    return snprintf(buf, PAGE_SIZE, "CTS_MAX_I2C_BUF_SIZE: %d\n",
            CTS_MAX_I2C_BUF_SIZE);
#else
    return snprintf(buf, PAGE_SIZE, "CTS_MAX_SPI_BUF_SIZE: %d\n",
            CTS_MAX_SPI_BUF_SIZE);
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(max_xfer_size, S_IRUGO, max_xfer_size_show, NULL);
#else
static DRIVER_ATTR_RO(max_xfer_size);
#endif

static ssize_t driver_ver_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "Driver version: %s\n",
            CFG_CTS_DRIVER_VERSION);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(driver_ver, S_IRUGO, driver_ver_show, NULL);
#else
static DRIVER_ATTR_RO(driver_ver);
#endif

static struct attribute *cts_driver_config_attrs[] = {
    &driver_attr_reset_pin.attr,
    &driver_attr_swap_xy.attr,
    &driver_attr_wrap_x.attr,
    &driver_attr_wrap_y.attr,
    &driver_attr_force_update.attr,
    &driver_attr_max_touch_num.attr,
    &driver_attr_vkey.attr,
    &driver_attr_gesture.attr,
    &driver_attr_esd_protection.attr,
    &driver_attr_slot_protocol.attr,
    &driver_attr_max_xfer_size.attr,
    &driver_attr_driver_ver.attr,
    NULL
};

static const struct attribute_group cts_driver_config_group = {
    .name = "config",
    .attrs = cts_driver_config_attrs,
};

static const struct attribute_group *cts_driver_config_groups[] = {
    &cts_driver_config_group,
    NULL,
};
#endif /* CONFIG_CTS_SYSFS */

#ifdef CONFIG_CTS_OF
static const struct of_device_id cts_of_match_table[] = {
    {.compatible = CFG_CTS_OF_DEVICE_ID_NAME, },
    /*bsp:add for bringup by guoxin.zhu 20230628 start*/
    {.compatible = "adaptive-touchscreen",},
    /*bsp:add for bringup by guoxin.zhu 20230628 end*/
    { },
};

MODULE_DEVICE_TABLE(of, cts_of_match_table);
#endif /* CONFIG_CTS_OF */

static const struct i2c_device_id cts_i2c_device_id_table[] = {
    { CFG_CTS_DEVICE_NAME, 0 },
    { }
};

static const struct spi_device_id cts_spi_device_id_table[] = {
    { CFG_CTS_DEVICE_NAME, 0 },
    { }
};


static struct i2c_driver cts_i2c_driver = {
    .probe = cts_i2c_driver_probe,
    .remove = cts_i2c_driver_remove,
    .driver = {
        .name = CFG_CTS_DRIVER_NAME,
        .owner = THIS_MODULE,
#ifdef CONFIG_CTS_OF
        .of_match_table = of_match_ptr(cts_of_match_table),
#endif /* CONFIG_CTS_OF */
#ifdef CONFIG_CTS_SYSFS
        .groups = cts_driver_config_groups,
#endif /* CONFIG_CTS_SYSFS */
#ifdef CONFIG_CTS_PM_LEGACY
        .suspend = cts_driver_suspend,
        .resume = cts_driver_resume,
#endif /* CONFIG_CTS_PM_LEGACY */
#ifdef CONFIG_CTS_PM_GENERIC
        .pm = &cts_driver_pm_ops,
#endif /* CONFIG_CTS_PM_GENERIC */
        },
    .id_table = cts_i2c_device_id_table,
};

static struct spi_driver cts_spi_driver = {
    .probe = cts_spi_driver_probe,
    .remove = cts_spi_driver_remove,
    .driver = {
        .name = CFG_CTS_DRIVER_NAME,
        .owner = THIS_MODULE,
#ifdef CONFIG_CTS_OF
        .of_match_table = of_match_ptr(cts_of_match_table),
#endif /* CONFIG_CTS_OF */
#ifdef CONFIG_CTS_SYSFS
        .groups = cts_driver_config_groups,
#endif /* CONFIG_CTS_SYSFS */
#ifdef CONFIG_CTS_PM_LEGACY
        .suspend = cts_driver_suspend,
        .resume = cts_driver_resume,
#endif /* CONFIG_CTS_PM_LEGACY */
#ifdef CONFIG_CTS_PM_GENERIC
        .pm = &cts_driver_pm_ops,
#endif /* CONFIG_CTS_PM_GENERIC */
#if CTS_PATCH_COMERR_PM
        .pm = &cts_dev_pm_ops,
#endif
        },
    .id_table = cts_spi_device_id_table,
};

int cts_driver_init(void)
{
    cts_info("Chipone touch driver init, version: "CFG_CTS_DRIVER_VERSION);

    cts_info(" - Register spi driver");
    spi_register_driver(&cts_spi_driver);
    cts_info("register spi driver\n");
    if (g_cts_data) {
        cts_info("register spi driver success\n");
        return 0;
    }
    cts_info("spi driver offline, unregister it!\n");
    spi_unregister_driver(&cts_spi_driver);

    cts_info(" - Register i2c driver");
    i2c_add_driver(&cts_i2c_driver);
    cts_info("register i2c driver\n");
    if (g_cts_data) {
        cts_info("register i2c driver success\n");
        return 0;
    }

    cts_info("i2c driver offline, unregister it!\n");
    i2c_del_driver(&cts_i2c_driver);

    cts_err("cts driver init failed!\n");
    return -ENODEV;
}

void cts_driver_exit(void)
{
    cts_info("Exit");

    if (g_cts_data->cts_dev.bus_type == CTS_BUS_I2C)
        i2c_del_driver(&cts_i2c_driver);
    else if (g_cts_data->cts_dev.bus_type == CTS_BUS_SPI)
        spi_unregister_driver(&cts_spi_driver);
}

//module_init(cts_driver_init);
//module_exit(cts_driver_exit);

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
MODULE_DESCRIPTION("Chipone TDDI touchscreen Driver for QualComm platform");
MODULE_VERSION(CFG_CTS_DRIVER_VERSION);
MODULE_AUTHOR("Miao Defang <dfmiao@chiponeic.com>");
MODULE_LICENSE("GPL");
