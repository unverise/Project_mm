#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#ifdef CONFIG_IDE
#include <linux/ide.h>
#endif
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/version.h>
#include <linux/regulator/consumer.h>
#include "semi_touch_interface.h"
#include "semi_touch_test_5xxx.h"
/*bsp:add for transsion core by guoxin.zhu 20230515 start*/
#include "chipsemi_transsion.h"
/*bsp:add for transsion core by guoxin.zhu 20230515 end*/

#define semi_io_free(pin)                   do{ if(gpio_is_valid(pin)) gpio_free(pin); }while(0)

static const struct of_device_id sm_of_match[] =
{
    {.compatible = "chipsemi,chsc_cap_touch", },
    {}
};

static const struct hal_device_id sm_ts_id[] =
{
    {CHSC_DEVICE_NAME, 0},
    {}
};

int semi_touch_get_int(void)
{
    int int_gpio_no = 0;
    struct device_node *of_node = NULL;
    //of_node = of_find_node_by_name(NULL, "smtouch");
    //check_return_if_zero(of_node, NULL);
    of_node = of_find_matching_node(NULL, sm_of_match);
    check_return_if_zero(of_node, NULL);

    int_gpio_no = of_get_named_gpio(of_node, "chipsemi,int-gpio", 0);
    check_return_if_fail(int_gpio_no, NULL);

    gpio_request(int_gpio_no, "chsc_int_pin");

    return int_gpio_no;

    //return of_get_named_gpio(of_node, "chipsemi,int-gpio", 0);
}

int semi_touch_get_rst(void)
{
    int rst_gpio_no = 0;
    struct device_node *of_node = NULL;
    //of_node = of_find_node_by_name(NULL, "smtouch");
    //check_return_if_zero(of_node, NULL);
    of_node = of_find_matching_node(NULL, sm_of_match);
    check_return_if_zero(of_node, NULL);

    rst_gpio_no = of_get_named_gpio(of_node, "chipsemi,rst-gpio", 0);
    check_return_if_fail(rst_gpio_no, NULL);

    gpio_request(rst_gpio_no, "chsc_rst_pin");

    return rst_gpio_no;
}

int semi_touch_get_vdd(void)
{
    int vdd_gpio_no = 0;
    struct device_node *of_node = NULL;
    //of_node = of_find_node_by_name(NULL, "smtouch");
    //check_return_if_zero(of_node, NULL);
    of_node = of_find_matching_node(NULL, sm_of_match);
    check_return_if_zero(of_node, NULL);

    vdd_gpio_no = of_get_named_gpio(of_node, "chipsemi,vdd-gpio", 0);
    check_return_if_fail(vdd_gpio_no, NULL);

    gpio_request(vdd_gpio_no, "chsc_vdd_pin");

    return vdd_gpio_no;

    //return of_get_named_gpio(of_node, "chipsemi,vdd-gpio", 0);
}

int semi_touch_get_vddio(void)
{
    int vddio_gpio_no = 0;
    struct device_node *of_node = NULL;
    //of_node = of_find_node_by_name(NULL, "smtouch");
    //check_return_if_zero(of_node, NULL);
    of_node = of_find_matching_node(NULL, sm_of_match);
    check_return_if_zero(of_node, NULL);

    vddio_gpio_no = of_get_named_gpio(of_node, "chipsemi,vddio-gpio", 0);
    //check_return_if_fail(vddio_gpio_no, NULL);
    if (vddio_gpio_no < 0) {
        kernel_log_e("get vddio-gpio fail%d", vddio_gpio_no);
        vddio_gpio_no = -1;
    }
    if (vddio_gpio_no > 0) {
        gpio_request(vddio_gpio_no, "chsc_vddio_pin");
    }
    return vddio_gpio_no;

    //return of_get_named_gpio(of_node, "chipsemi,vddio-gpio", 0);
}

int semi_touch_get_irq(int rst_pin)
{
    int irq_no = 0;
    struct gpio_desc *desc;

    // 有效性校验
    if (!gpio_is_valid(rst_pin)) {
        kernel_log_e("Invalid GPIO: %d\n", rst_pin);
        return -EINVAL;
    }

    // 转换到描述符
    desc = gpio_to_desc(rst_pin);
    if (!desc) {
        kernel_log_e("Failed to convert GPIO %d to descriptor\n", rst_pin);
        return -ENODEV;
    }

    gpiod_set_debounce(desc, 50);

    irq_no = gpio_to_irq(rst_pin);

    return irq_no;
}

#if SEMI_TOUCH_FACTORY_TEST_EN
void convert_u32_to_short(short *short_array, unsigned int *u32_array, unsigned int size)
{
    int i;

    for (i = 0; i < size; i++)
    {
        short_array[i] = (short)u32_array[i];
    }
}
#endif

int semi_touch_get_info(void)
{
    int ret = SEMI_DRV_ERR_OK;
#if SEMI_TOUCH_FACTORY_TEST_EN
    unsigned int count;
    struct rawdata_th *p_raw = NULL;
#endif
    struct device_node *of_node = NULL;
    of_node = of_find_matching_node(NULL, sm_of_match);
    check_return_if_zero(of_node, NULL);

#if SEMI_TOUCH_FACTORY_TEST_EN
    ret = of_property_read_u32(of_node, "chipsemi,rawdata_limits_row", &st_dev.rawdata_row);
    if (ret)
    {
        kernel_log_e("get rawdata_limits_row faild! \r\n");
        return ret;
    }

    ret = of_property_read_u32(of_node, "chipsemi,rawdata_limits_col", &st_dev.rawdata_col);
    if (ret)
    {
        kernel_log_e("get rawdata_limits_col faild! \r\n");
        return ret;
    }

    ret = of_property_read_u32(of_node, "chipsemi,short_limits", &st_dev.short_limits);
    if (ret)
    {
        kernel_log_e("get short_limits faild! \r\n");
        return ret;
    }


    ret = of_property_read_u32_array(of_node, "invalid_row_col", st_dev.invalid_row_col, 8);
    if (ret != 0)
    {
        kernel_log_e("get invalid_row_col faild! \r\n");
        return ret;
    }

    count = st_dev.rawdata_row * st_dev.rawdata_col;

    p_raw = kzalloc(sizeof(struct rawdata_th), GFP_KERNEL);
    if (!p_raw)
    {
        kernel_log_e("alloc p_raw buff failed, size:%ld",sizeof(struct rawdata_th));
        return -ENOMEM;
    }

    ret = of_property_read_u32_array(of_node, "rawdata_lo_limits", p_raw -> rawdata_min, count);
    if (ret != 0)
    {
        kernel_log_e("get rawdata_lo_limits faild! \r\n");
        goto exit;
    }

    ret = of_property_read_u32_array(of_node, "rawdata_hi_limits", p_raw -> rawdata_max, count);
    if (ret != 0)
    {
        kernel_log_e("get rawdata_hi_limits faild! \r\n");
        goto exit;
    }

    convert_u32_to_short(st_dev.rawdata_min, p_raw -> rawdata_min, count);
    convert_u32_to_short(st_dev.rawdata_max, p_raw -> rawdata_max, count);

exit:

    if (p_raw)
    {
        kfree(p_raw);
    }
#endif

    return ret;
}

struct regulator *reg_vdd = NULL;
struct regulator *reg_vio = NULL;

int semi_touch_power_ctrl(unsigned char level)
{
    int ret = SEMI_DRV_ERR_OK;
    static unsigned char power_status = 0; //off

    if (1 == level && 0 == power_status)
    {
        power_status = 1;
        if (!IS_ERR_OR_NULL(reg_vdd))
        {
            ret = regulator_enable(reg_vdd);
            check_return_if_fail(ret, NULL);
        }
        if (!IS_ERR_OR_NULL(reg_vio))
        {
            ret = regulator_enable(reg_vio);
            check_return_if_fail(ret, NULL);
        }
        if (st_dev.rst_pin > 0)
        {
            semi_io_direction_out(st_dev.rst_pin, 1);
        }

        kernel_log_d("vdd power up...\n");
    }
    else if (0 == level && 1 == power_status)
    {
        power_status = 0;
        if (!IS_ERR_OR_NULL(reg_vdd))
        {
            ret = regulator_disable(reg_vdd);
            check_return_if_fail(ret, NULL);
        }
        if (!IS_ERR_OR_NULL(reg_vio))
        {
            ret = regulator_disable(reg_vio);
            check_return_if_fail(ret, NULL);
        }
        if (st_dev.rst_pin > 0)
        {
            semi_io_direction_out(st_dev.rst_pin, 0);
        }

        kernel_log_d("vdd power down...\n");
        enter_suspend_gate(st_dev.stc.ctp_run_status);
    }
    else
    {
        //don't care
    }

    return ret;
}

int semi_touch_power_exit(void)
{
    int ret = SEMI_DRV_ERR_OK;

    semi_touch_power_ctrl(0);

    if (!IS_ERR_OR_NULL(reg_vdd))
    {
        ret = regulator_set_voltage(reg_vdd, 0, 0);
        check_return_if_fail(ret, NULL);
        regulator_put(reg_vdd);
    }
    if (!IS_ERR_OR_NULL(reg_vio))
    {
        ret = regulator_set_voltage(reg_vio, 0, 0);
        check_return_if_fail(ret, NULL);
        regulator_put(reg_vio);
    }

    return ret;
}
/*bsp:modify for transsion core by longtao.ran 20240321 start*/
#if HAL_INTERFACE_I2C == HAL_INTERFACE_TYPE
int semi_touch_power_init(struct i2c_client *hal)
#else
int semi_touch_power_init(struct hal_device *hal)
#endif
/*bsp:modify for transsion core by longtao.ran 20240321 end*/
{
    int ret = SEMI_DRV_ERR_OK;
    reg_vdd = regulator_get(&hal->dev, "vdd");
    if (IS_ERR_OR_NULL(reg_vdd))
    {
        kernel_log_d("vdd regulator dts not match\n");
    }

    reg_vio = regulator_get(&hal->dev, "vio");
    if (IS_ERR_OR_NULL(reg_vio))
    {
        kernel_log_d("vio regulator dts not match\n");
    }

    return ret;
}

/********************************************************************************************************************************/
/*virtual key*/
#if 0 == SEMI_TOUCH_VKEY_MAPPING
struct kobject *sm_properties_kobj = NULL;
static ssize_t virtual_keys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int index = 0, iter = 0;
    char *vkey_buf = buf;
    for (index = 0; (index < st_dev.stc.vkey_num) && (index < MAX_VKEY_NUMBER); index++)
    {
        iter += sprintf(vkey_buf + iter, "%s:%d:%d:%d:%d:%d%s",
                        __stringify(EV_KEY), st_dev.stc.vkey_evt_arr[index],
                        st_dev.stc.vkey_dim_map[index][0], st_dev.stc.vkey_dim_map[index][1], 50, 50,
                        (index == st_dev.stc.vkey_num - 1) ? "\n" : ":");
    }

    return iter;
}
static struct kobj_attribute virtual_keys_attr =
{
    .attr =
    {
        .name = "virtualkeys.chsc_cap_touch",
        .mode = S_IRUGO,
    },
    .show = &virtual_keys_show,
};
static struct attribute *properties_attrs[] =
{
    &virtual_keys_attr.attr,
    NULL
};
static struct attribute_group properties_attr_group =
{
    .attrs = properties_attrs,
};

int semi_touch_vkey_initialize(void)
{
    int ret = 0;
    sm_properties_kobj = kobject_create_and_add("board_properties", NULL);
    check_return_if_zero(sm_properties_kobj, NULL);

    ret = sysfs_create_group(sm_properties_kobj, &properties_attr_group);
    check_return_if_fail(ret, NULL);

    return ret;
}
#else
#define semi_touch_vkey_initialize()    0
#endif  //SEMI_TOUCH_VKEY_MAPPING
/********************************************************************************************************************************/
/*proximity support*/
#if SEMI_TOUCH_PROXIMITY_OPEN
#include <linux/input.h>
#define PROXIMITY_CLASS_NAME            "chsc_tpd"
#define PROXIMITY_DEVICE_NAME           "device"

/* default cmd interface(refer to sensor HAL):"/sys/class/chsc-tpd/device/proximity" */

struct chsc_proximity
{
    struct class *proximity_cls;
    struct device *proximity_dev;
    struct input_dev *proximity_input;
};

static struct chsc_proximity proximity_obj;

int semi_touch_proximity_init(void)
{
    int ret = 0;

    proximity_obj.proximity_cls = class_create(THIS_MODULE, PROXIMITY_CLASS_NAME);
    check_return_if_fail(proximity_obj.proximity_cls, NULL);

    proximity_obj.proximity_dev = device_create(proximity_obj.proximity_cls, NULL, 0, NULL, PROXIMITY_DEVICE_NAME);
    check_return_if_fail(proximity_obj.proximity_cls, NULL);

    proximity_obj.proximity_input = input_allocate_device();
    check_return_if_zero(proximity_obj.proximity_input, NULL);

    proximity_obj.proximity_input->name = "proximity_tp";
    set_bit(EV_ABS, proximity_obj.proximity_input->evbit);
    input_set_capability(proximity_obj.proximity_input, EV_ABS, ABS_DISTANCE);
    input_set_abs_params(proximity_obj.proximity_input, ABS_DISTANCE, 0, 1, 0, 0);
    ret = input_register_device(proximity_obj.proximity_input);
    check_return_if_fail(ret, NULL);

    open_proximity_function(st_dev.stc.custom_function_en);

    return ret;
}

bool semi_touch_proximity_report(unsigned char proximity)
{
    kernel_log_d("proximity = %d\n", proximity);
    if (is_proximity_function_en(st_dev.stc.custom_function_en))
    {
        input_report_abs(proximity_obj.proximity_input, ABS_DISTANCE, proximity);
        input_mt_sync(proximity_obj.proximity_input);
        input_sync(proximity_obj.proximity_input);
    }

    return true;
}

int semi_touch_proximity_stop(void)
{
    if (proximity_obj.proximity_input)
    {
        input_unregister_device(proximity_obj.proximity_input);
        input_free_device(proximity_obj.proximity_input);
    }
    if (proximity_obj.proximity_dev)
    {
        device_destroy(proximity_obj.proximity_cls, 0);
    }
    if (proximity_obj.proximity_cls)
    {
        class_destroy(proximity_obj.proximity_cls);
    }

    return 0;
}
#endif

int semi_touch_platform_variety(void)
{
    semi_touch_power_exit();

    if (st_dev.int_pin)
    {
        semi_io_free(st_dev.int_pin);
    }

    if (st_dev.rst_pin)
    {
        semi_io_free(st_dev.rst_pin);
    }

#if 0 == SEMI_TOUCH_VKEY_MAPPING
    if (NULL != sm_properties_kobj)
    {
        sysfs_remove_group(sm_properties_kobj, &properties_attr_group);
        kobject_put(sm_properties_kobj);
    }
#endif

    return 0;
}
/********************************************************************************************************************************/
#if(!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND) && !defined(CONFIG_DRM))
static const struct dev_pm_ops semi_touch_dev_pm_ops =
{
    .suspend = semi_touch_suspend_entry,
    .resume = semi_touch_resume_entry,
};
#else
static const struct dev_pm_ops semi_touch_dev_pm_ops =
{

};
#endif
/*bsp:modify for transsion core by longtao.ran 20240321 start*/
#if 0
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
struct notifier_block sm_fb_notify;
static int semi_touch_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
    int *blank;
    struct fb_event *evdata = data;
    if (evdata && evdata->data && FB_EVENT_BLANK == event && st_dev.client)
    {
        blank = evdata->data;
        if (FB_BLANK_UNBLANK == *blank)
            semi_touch_resume_entry(&st_dev.client->dev);
        else if (FB_BLANK_POWERDOWN == *blank)
            semi_touch_suspend_entry(&st_dev.client->dev);
    }

    return 0;
}
int semi_touch_work_done(void)
{
    int ret = 0;
    ret = semi_touch_vkey_initialize();
    check_return_if_fail(ret, NULL);

    sm_fb_notify.notifier_call = semi_touch_fb_notifier_callback;
    ret = fb_register_client(&sm_fb_notify);
    check_return_if_fail(ret, NULL);

    return ret;
}
int semi_touch_resource_release(void)
{
    fb_unregister_client(&sm_fb_notify);
    return semi_touch_platform_variety();
}
#elif defined(CONFIG_DRM)
#include <linux/notifier.h>
#include <drm/drm_panel.h>
struct drm_panel *active_panel = NULL;
struct notifier_block sm_fb_notify;
static int semi_touch_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
    int *blank;
    struct drm_panel_notifier *evdata = (struct drm_panel_notifier *)data;

    if (evdata && evdata->data && DRM_PANEL_EVENT_BLANK == event && st_dev.client)
    {
        blank = evdata->data;
        if (DRM_PANEL_BLANK_UNBLANK == *blank)
            semi_touch_resume_entry(&st_dev.client->dev);
        else if (DRM_PANEL_BLANK_POWERDOWN == *blank)
            semi_touch_suspend_entry(&st_dev.client->dev);

        //kernel_log_d("drm event = %lu, blank = %d\n", event, *blank);
    }

    return 0;
}
static int semi_touch_drm_get_panel(struct device_node *np)
{
    int index, count;
    struct device_node *node = NULL;
    struct drm_panel *panel = NULL;

    count = of_count_phandle_with_args(np, "panel", NULL);
    if (count <= 0) return -SEMI_DRV_INVALID_PARAM;

    for (index = 0; index < count; index++)
    {
        node = of_parse_phandle(np, "panel", index);
        panel = of_drm_find_panel(node);
        of_node_put(node);
        if (!IS_ERR(panel))
        {
            active_panel = panel;
            return SEMI_DRV_ERR_OK;
        }
    }

    return -SEMI_DRV_ERR_NOT_MATCH;
}
int semi_touch_work_done(void)
{
    int ret = 0;

    ret = semi_touch_vkey_initialize();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_drm_get_panel(st_dev.client->dev.of_node);
    check_return_if_fail(ret, NULL);

    kernel_log_d("register drm notify, active = %x\n", active_panel);

    sm_fb_notify.notifier_call = semi_touch_drm_notifier_callback;
    ret = drm_panel_notifier_register(active_panel, &sm_fb_notify);
    check_return_if_fail(ret, NULL);

    return ret;
}
int semi_touch_resource_release(void)
{
    if (NULL != active_panel)
    {
        drm_panel_notifier_unregister(active_panel, &sm_fb_notify);
    }
    return semi_touch_platform_variety();
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
struct early_suspend esp;
static void semi_touch_early_suspend(struct early_suspend *h)
{
    if (NULL == h) return;

    semi_touch_suspend_entry(&st_dev.client->dev);
}
static void semi_touch_late_resume(struct early_suspend *h)
{
    if (NULL == h) return;

    semi_touch_resume_entry(&st_dev.client->dev);
}
int semi_touch_work_done(void)
{
    ret = semi_touch_vkey_initialize();
    check_return_if_fail(ret, NULL);

    esp.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    esp.suspend = semi_touch_early_suspend;
    esp.resume = semi_touch_late_resume;
    register_early_suspend(&esp);

    return 0;
}
int semi_touch_resource_release(void)
{
    unregister_early_suspend(&esp);
    return semi_touch_platform_variety();
}
#else
int semi_touch_work_done(void)
{
    return 0;
}
int semi_touch_resource_release(void)
{
    return semi_touch_platform_variety();
}
#endif
#endif

#if HAL_INTERFACE_I2C == HAL_INTERFACE_TYPE
static int semi_touch_probe(struct hal_device *hal, const struct hal_device_id *id)
#else
static int semi_touch_probe(struct hal_device *hal)
#endif
{

    int ret = 0;
    /*bsp:add for transsion core by longtao.ran 20240321 start*/
    struct tran_core_ops *ops = NULL;
    /*bsp:add for transsion core by longtao.ran 20240321 end*/

    semi_touch_power_init(hal);
    semi_touch_power_ctrl( 1 );

    ret = semi_touch_init(hal);
    if (-SEMI_DRV_ERR_HAL_IO == ret)
    {
        semi_touch_deinit(hal);
        check_return_if_fail(ret, NULL);
    }
    /*bsp:add for transsion core by longtao.ran 20240321 start*/
    chipsemi_controller.input_dev = st_dev.input;
    if(get_tpd_ops(&chipsemi_controller, &ops)) {
        ops->controller_init_completion(&chipsemi_controller);
    }
    /*bsp:add for transsion core by longtao.ran 20240321 end*/

    kernel_log_d(
        "probe finished(result:%d) driver ver(%s) PROJECT_NAME(%d)\r\n",
        ret,
        CHSC_DRIVER_VERSION,
        PROJECT_NAME);

    return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0))
static void semi_touch_remove(struct hal_device *hal)
{
    semi_touch_deinit(hal);
}
#else
static int semi_touch_remove(struct hal_device *hal)
{
    int ret = 0;

    ret = semi_touch_deinit(hal);

    return ret;
}
#endif

void semi_touch_mode_recover (int mark)
{
    unsigned char bootCheckOk = 0;

    semi_touch_start_up_check(&bootCheckOk, only_sp_check);

    if (bootCheckOk)
    {
        if (mark & 0x01) semi_touch_charger_switch(1);
        if ((mark >> 1) & 0x01) semi_touch_game_mode_switch(1);
        if ((mark >> 2) & 0x01) semi_touch_palm_mode_switch(1);
        if ((mark >> 3) & 0x01) semi_touch_game_op_mode_switch(G_GAME_BACK, 1);
        if ((mark >> 4) & 0x01) semi_touch_high_sr_mode_switch(1);
        if ((mark >> 5) & 0x01) semi_touch_game_inhibit_switch(1);
        if ((mark >> 6) & 0x01) semi_touch_eaa_touch_switch(G_EAAT_BACK, 1);
        if ((mark >> 7) & 0x01) semi_touch_region_ctrl_switch(1);
    }
}

int semi_touch_suspend_entry(struct device *dev)
{
    //struct i2c_client *client = st_dev.client;
    if (is_proximity_function_en(st_dev.stc.custom_function_en))
    {
        if (is_proximity_activate(st_dev.stc.ctp_run_status))
        {
            kernel_log_d("proximity is active, so fake suspend...");
            return SEMI_DRV_ERR_OK;
        }
    }

    if (is_guesture_function_en(st_dev.stc.custom_function_en))
    {
        semi_touch_guesture_switch(G_GESTURE_MARK, 1);//Control the value of G_GESTURE_MARK to switch the required mode
        st_dev.gest_flag = 1;
        enable_irq_wake(st_dev.client->irq);
    }
    else
    {
#if SEMI_TOUCH_SUSPEND_BY_TPCMD
        semi_touch_suspend_ctrl(1);
#else
        semi_touch_power_ctrl(0);
#endif
        semi_touch_clear_report();
        //disable_irq(client->irq);
        kernel_log_d("tpd real suspend...\n");
    }

    return SEMI_DRV_ERR_OK;
}

int semi_touch_resume_entry(struct device *dev)
{
    unsigned char bootCheckOk = 0;
    int mode_mark = 0;
    unsigned char glove_activity = is_glove_activate(st_dev.stc.ctp_run_status);
    unsigned char wet_finger_activity = is_wet_finger_activate(st_dev.stc.ctp_run_status);
    mode_mark |= (is_charger_activate(st_dev.stc.ctp_run_status)) << 0;
    mode_mark |= (is_game_mode_activate(st_dev.stc.ctp_run_status)) << 1;
    mode_mark |= (is_palm_mode_activate(st_dev.stc.ctp_run_status)) << 2;
    mode_mark |= (is_game_op_mode_activate(st_dev.stc.ctp_run_status)) << 3;
    mode_mark |= (is_high_sr_mode_activate(st_dev.stc.ctp_run_status)) << 4;
    mode_mark |= (is_game_touch_inhibit_activate(st_dev.stc.ctp_run_status)) << 5;
    mode_mark |= (is_eaa_touch_activate(st_dev.stc.ctp_run_status)) << 6;
    mode_mark |= (is_region_ctrl_activate(st_dev.stc.ctp_run_status)) << 7;

    if (is_proximity_function_en(st_dev.stc.custom_function_en))
    {
        if (is_proximity_activate(st_dev.stc.ctp_run_status))
        {
            kernel_log_d("proximity is active, so fake resume...");
            return SEMI_DRV_ERR_OK;
        }
    }
    if (is_guesture_function_en(st_dev.stc.custom_function_en))
    {
        disable_irq_wake(st_dev.client->irq);
    }

#if 0 == SEMI_TOUCH_SUSPEND_BY_TPCMD
    semi_touch_power_ctrl(1);
#endif

    st_dev.gest_flag = 0;

    //reset tp + iic detected
    disable_irq(st_dev.client->irq);
    semi_touch_reset_and_detect();
    //set_status_pointing(st_dev.stc.ctp_run_status);
    enable_irq(st_dev.client->irq);

    semi_touch_clear_report();
    //enable_irq(client->irq);
    //USD:Add for fod by yun.luo start
    if (st_dev.fod_func_support) {
        if (chipsemi_controller.tran_ts_data->fp_status.g_fod_flag == 1) {
            input_report_key(st_dev.input, KEY_FINGER, 0);
            input_sync(st_dev.input);
            chipsemi_controller.tran_ts_data->fp_status.g_fod_flag = 0;
            kernel_log_d("resume gesture FOD key finger up");
        }
    }
    //USD:Add for fod by yun.luo end
    if((glove_activity) && (!wet_finger_activity))
    {
        semi_touch_start_up_check(&bootCheckOk, only_sp_check);
        if (bootCheckOk)
        {
            semi_touch_glove_switch(1);
        }
    }

    if ((wet_finger_activity) && (!glove_activity))
    {
        semi_touch_start_up_check(&bootCheckOk, only_sp_check);
        if (bootCheckOk)
        {
            semi_touch_wet_finger_switch(1);
        }
    }

    semi_touch_mode_recover (mode_mark);

    semi_touch_send_rtime();

    kernel_log_d("tpd_resume...\r\n");

    return SEMI_DRV_ERR_OK;
}

static struct hal_driver sm_touch_driver =
{
    .driver =
    {
        .owner = THIS_MODULE,
        //BSP: modify cit bring up by xiaobo.fan 20220413 start
        .name  = "mtk-tpd",
        //BSP: modify cit bring up by xiaobo.fan 20220413 end
        .of_match_table = of_match_ptr(sm_of_match),
#if CONFIG_PM
        .pm = &semi_touch_dev_pm_ops,
#endif
    },
    .id_table = sm_ts_id,
    .probe = semi_touch_probe,
    .remove = semi_touch_remove,
};
/*bsp:modify for transsion core by longtao.ran 20240321 start*/
int hal_device_init(void)
{
    int ret = 0;

    ret = hal_register_driver(&sm_touch_driver);
    check_return_if_fail(ret, NULL);

    return ret;
}

void hal_device_exit(void)
{
    hal_unregister_driver(&sm_touch_driver);
}

//module_init(hal_device_init);
//module_exit(hal_device_exit);
/*bsp:modify for transsion core by longtao.ran 20240321 end*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("wasim");
