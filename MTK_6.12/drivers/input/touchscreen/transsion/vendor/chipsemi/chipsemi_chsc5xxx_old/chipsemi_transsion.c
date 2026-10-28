#include "head_def.h"
#include "platform.h"
#include "chipsemi_transsion.h"
#include "semi_touch_interface.h"
#include "semi_touch_test_5xxx.h"

extern int register_ts_controller(struct tran_ts_controller *controller);
extern int unregister_ts_controller(struct tran_ts_controller *controller);
extern int hal_device_init(void);
extern void hal_device_exit(void);

extern struct sm_touch_dev st_dev;
struct sm_touch_dev *g_semi_data = &st_dev;

struct tran_ts_controller chipsemi_controller;

static int tran_ts_init(void)
{
    int ret = 0;
    TRAN_FUNC_ENTER();
    hal_device_init();
    TRAN_FUNC_EXIT();
    return ret;
}

static void suspend(struct device *h, int opt)
{
    if(g_semi_data != NULL) {
        /* ps suspend */
        if(chipsemi_controller.controller_state == TRAN_PS_SUSPEND_MODE) {
            kernel_log_d("ps suspend, do nothing");
            return;
        }
        /* gesture suspend */
        if(chipsemi_controller.controller_state == TRAN_GESTURE_SUSPEND_MODE) {
            open_guesture_function(g_semi_data->stc.custom_function_en);
            kernel_log_d("gesture enable = %d",st_dev.stc.custom_function_en);
        }
        /* exec suspend */
        semi_touch_suspend_entry(h);
    }
}

static void resume(struct device *h, int opt)
{
    if(g_semi_data != NULL) {
        /* ps resume */
        if(chipsemi_controller.controller_state == TRAN_PS_SUSPEND_MODE) {
            kernel_log_d("ps resume, do nothing");
            return;
        }
        /* exec resume */
        semi_touch_resume_entry(h);
        /* turn off gesture state after resume */
        if(chipsemi_controller.controller_state == TRAN_GESTURE_SUSPEND_MODE)
            close_guesture_function(g_semi_data->stc.custom_function_en);
    }
}

static void set_edge_reject_mode(uint8_t edge_reject_switch)
{
    u8 direction;
    int ret;

    switch (edge_reject_switch) {
    case 0:
        direction = 0;
        ret = semi_touch_h_v_switch(0x13, 1);
        break;//normal
    case 1:
        direction = 1;
        ret = semi_touch_h_v_switch(0x14, 1);
        break;//usb right
    case 3:
        direction = 2;
        ret = semi_touch_h_v_switch(0x15, 1);
        break;//usb left
    default:
        kernel_log_e("Invalid arg for mode");
        return ;
    }
    kernel_log_d("direction = %d", direction);

    if (ret) {
        kernel_log_e("Set edge restain failed:ret= %d",ret);
    }
}

static void set_noise_mode(u32 state)
{
    if(!is_status_pointing(st_dev.stc.custom_function_en)){
        kernel_log_e("host downloading, not ready , do nothing");
        return;
    }
    if(state) {
        /* usb plug in */
        open_charger_function(st_dev.stc.custom_function_en);
        semi_touch_charger_switch(1);
    } else {
        /* usb plug out */
        semi_touch_charger_switch(0);
        close_charger_function(st_dev.stc.custom_function_en);
    }
}

static void set_ps_mode(u32 state)
{
#if 0
    struct chipsemi_ts_data *pdata = g_semi_data;

    if(pdata == NULL)
        return;

    if(state) {
        /* usb plug in */
        semi_lock_device(&pdata->semi_dev);
        semi_tcs_set_proximity_mode(&pdata->semi_dev, 1);
        semi_unlock_device(&pdata->semi_dev);
    } else {
        /* usb plug out */
        semi_lock_device(&pdata->semi_dev);
        semi_tcs_set_proximity_mode(&pdata->semi_dev, 0);
        semi_unlock_device(&pdata->semi_dev);
    }
#endif
}

static void tran_semi_touch_palm_mode_switch(u32 state)
{

    if (state) {
        open_palm_mode_function(st_dev.stc.custom_function_en);
        semi_touch_palm_mode_switch(1);
    } else {
        semi_touch_palm_mode_switch(0);
        close_palm_mode_function(st_dev.stc.custom_function_en);
    }

}

static void set_ts_mode(u32 cmd, u32 state)
{
    switch(cmd) {
    case SET_NOISE_MODE:
        set_noise_mode(state);
        break;

    case SET_PS_MODE:
        set_ps_mode(state);
        break;

    case SET_GAME_MODE:
        break;

    case SET_EDGE_REJECT_MODE:
        set_edge_reject_mode(state);
        break;
    case SET_POCKET_MODE:
        tran_semi_touch_palm_mode_switch(state);
        break;
    default:
        TRAN_INFO("can't find cmd=0x%x",cmd);
        break;
    }
}

static unsigned char get_firmware_version(void)
{
    struct sm_touch_dev *pdata = g_semi_data;
    u8 fw_ver = 0xff;

    if(pdata != NULL) {
        fw_ver = (pdata->vid_pid)&0xff;
    }

    return fw_ver;
}

static unsigned char get_base_version(void)
{
    struct sm_touch_dev *pdata = g_semi_data;
    u8 ret = 0;

    if(pdata != NULL)
        return pdata->upd_vid_pid;
    else
        return 0;

    return ret;
}

static int tp_selftest(void)
{
    int ret = 0;

    TRAN_FUNC_ENTER();
    ret = semi_tran_tp_selftest();
    if (ret == 0)
        return 1;
    TRAN_FUNC_EXIT();
    return 0;
}

static int get_selftest_data(char * data)
{
    TRAN_FUNC_ENTER();

    return 0;
}

static int tran_selftest_open(struct inode *inode, struct file *file)
{
    TRAN_FUNC_ENTER();
    //selftest_proc_open();
    return 0;
}

static void resume_by_ddi(void)
{
    /* do nothing */
}

static void tp_reset_level(int val)
{
    /* do noting */
}

/* 1: success 0: fail */
int get_tpd_ops(struct tran_ts_controller *controller, struct tran_core_ops **ops)
{
    if(controller->tran_ts_data != NULL) {
        *ops = controller->tran_ts_data->tpd_ops;
        if(*ops != NULL)
            return 1;
    }

    TRAN_INFO("get ops fail !");
    return 0;
}

static struct vendor_gesture_map  gestures_maps[] = {
    {GESTURE_LEFT,         GESTURE_LF},
    {GESTURE_UP,           GESTURE_up},
    {GESTURE_RIGHT,        GESTURE_RT},
    {GESTURE_DOWN,         GESTURE_down},
    {GESTURE_DOUBLECLICK,  GESTURE_DC},
    {GESTURE_SINGLECLICK,  GESTURE_OC},
    {GESTURE_W,            GESTURE_w},
    {GESTURE_O,            GESTURE_o},
    {GESTURE_M,            GESTURE_m},
    {GESTURE_E,            GESTURE_e},
    {GESTURE_C,            GESTURE_c},
    {GESTURE_Z,            GESTURE_z},
    {GESTURE_S,            GESTURE_s},
    {GESTURE_V,            GESTURE_v},
    {GESTURE_FPDWN,        GESTURE_FOD_down},
    {GESTURE_FPUP,         GESTURE_FOD_up},
    {0,0},

};

static char *support_list[] = {
    "chct5560",
    "chct5562",
    NULL /* not modify */

};

static struct tran_controller_ops controller_ops = {
    .tran_ts_init = tran_ts_init,
    .tran_ts_suspend = suspend,
    .tran_ts_resume = resume,
    .set_ts_mode= set_ts_mode,
    .get_firmware_version = get_firmware_version,
    .get_base_version = get_base_version,
    .tran_selftest = tp_selftest,
    .get_selftest_data = get_selftest_data,
    .tran_selftest_open = tran_selftest_open,
};

static struct tran_controller_ext_ops controller_ext_ops = {
    .tran_tp_reset = tp_reset_level,
    .resume_by_ddi = resume_by_ddi,
};

struct tran_ts_controller chipsemi_controller = {
	.cur_ic = NULL,
	.tran_ts_data = NULL,
	.selftest_data = NULL,
	.gesture_flag = true,
	.controller_state = TRAN_ACTIVE_MODE,
	.vendor_name = "chipsemi",
	.gesture_map = gestures_maps,
	.support_ic = support_list,
	.ops = &controller_ops,
	.ext_ops = &controller_ext_ops,
	.dsp_event_mutex = __MUTEX_INITIALIZER(chipsemi_controller.dsp_event_mutex),

};

static int __init controller_module_init(void)
{
	init_completion(&chipsemi_controller.comp);
    register_ts_controller(&chipsemi_controller);
    return 0;
}

static void __exit controller_module_exit(void)
{
    /* 清除transsion core的资源 */
    unregister_ts_controller(&chipsemi_controller);

    /* 清除当前驱动的资源 */
    hal_device_exit();

}

module_init(controller_module_init);
module_exit(controller_module_exit);

MODULE_AUTHOR("Transsion TP Driver Team");
MODULE_DESCRIPTION("Transsion Touchscreen Driver");
MODULE_LICENSE("GPL v2");
