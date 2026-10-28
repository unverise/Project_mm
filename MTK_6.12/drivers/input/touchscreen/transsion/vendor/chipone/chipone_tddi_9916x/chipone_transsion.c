#include "cts_config.h"
#include "cts_core.h"
#include "cts_tcs.h"
#include "cts_oem.h"
#include "cts_platform.h"
#include "chipone_transsion.h"

extern int register_ts_controller(struct tran_ts_controller *controller);
extern int unregister_ts_controller(struct tran_ts_controller *controller);
extern int cts_driver_init(void);
extern void cts_driver_exit(void);
extern int cts_suspend(struct chipone_ts_data *cts_data);
extern int cts_resume(struct chipone_ts_data *cts_data);

extern struct chipone_ts_data *g_cts_data;

struct tran_ts_controller chipone_controller;

static int tran_ts_init(void)
{
    int ret = 0;
    TRAN_FUNC_ENTER();
    cts_driver_init();
    TRAN_FUNC_EXIT();
    return ret;
}

static void suspend(struct device *h, int opt)
{
    if(g_cts_data != NULL) {
        /* ps suspend */
        if(chipone_controller.controller_state == TRAN_PS_SUSPEND_MODE) {
            cts_info("ps suspend, do nothing");
            return;
        }
        /* gesture suspend */
        if(chipone_controller.controller_state == TRAN_GESTURE_SUSPEND_MODE) {
            g_cts_data->cts_dev.rtdata.gesture_wakeup_enabled = 1;
        }
        /* exec suspend */
        cts_suspend(g_cts_data);
    }
}

static void resume(struct device *h, int opt)
{
    if(g_cts_data != NULL) {
        /* ps resume */
        if(chipone_controller.controller_state == TRAN_PS_SUSPEND_MODE) {
            cts_info("ps resume, do nothing");
            return;
        }
        /* exec resume */
        cts_resume(g_cts_data);
        /* turn off gesture state after resume */
        if(chipone_controller.controller_state == TRAN_GESTURE_SUSPEND_MODE)
            g_cts_data->cts_dev.rtdata.gesture_wakeup_enabled = 0;
    }
}

static inline void set_edge_reject_mode(uint8_t edge_reject_switch)
{
    struct chipone_ts_data *pdata = g_cts_data;
    u8 direction;
    int ret;

    if(pdata == NULL)
        return ;
    switch (edge_reject_switch) {
    case 0:
        direction = 0;
        break;//normal
    case 1:
        direction = 1;
        break;//usb right
    case 3:
        direction = 2;
        break;//usb left
    default:
        cts_err("Invalid arg for mode");
        return ;
    }
    cts_info("direction = %d", direction);

    cts_lock_device(&pdata->cts_dev);
    ret = cts_tcs_set_panel_direction(&pdata->cts_dev, direction);
    cts_unlock_device(&pdata->cts_dev);

    if (ret) {
        cts_err("Set edge restain failed");
    }
}

static inline void set_noise_mode(u32 state)
{
    struct chipone_ts_data *pdata = g_cts_data;

    if(pdata == NULL)
        return;

    if(state) {
        /* usb plug in */
        cts_lock_device(&pdata->cts_dev);
        cts_set_dev_charger_attached_for_transsion(&pdata->cts_dev, true);
        cts_unlock_device(&pdata->cts_dev);
    } else {
        /* usb plug out */
        cts_lock_device(&pdata->cts_dev);
        cts_set_dev_charger_attached_for_transsion(&pdata->cts_dev, false);
        cts_unlock_device(&pdata->cts_dev);
    }
}

static inline void set_ps_mode(u32 state)
{
    struct chipone_ts_data *pdata = g_cts_data;

    if (pdata == NULL)
        return;

    if(state) {
        /* usb plug in */
        cts_lock_device(&pdata->cts_dev);
        cts_tcs_set_proximity_mode(&pdata->cts_dev, 1);
        cts_unlock_device(&pdata->cts_dev);
    } else {
        /* usb plug out */
        cts_lock_device(&pdata->cts_dev);
        cts_tcs_set_proximity_mode(&pdata->cts_dev, 0);
        cts_unlock_device(&pdata->cts_dev);
    }
}

static void set_game_mode(u32 state)
{
    int ret;
    const struct cts_device *cts_dev = &g_cts_data->cts_dev;

    ret = cts_tcs_game_mode_select(cts_dev, state);
    if (ret < 0) {
        cts_err("game state write error %d", ret);
    }
}

//USD:modify for lcd aod by guoli.niu 20250321 start
void set_lcd_aod_mode(u32 state)
{
    int ret = 0;
    struct chipone_ts_data *pdata = g_cts_data;

    if(pdata == NULL)
        return;

    ret = cts_plat_disable_irq_wake(pdata->pdata);
    if (ret) {
        cts_warn("Disable IRQ wake failed %d", ret);
    }
    ret = cts_plat_disable_irq(pdata->pdata);
    if (ret < 0) {
        cts_err("Disable IRQ failed %d", ret);
    }

    ret = cts_tcs_set_aod_pwr_mode(&pdata->cts_dev, state);
    if (!ret) {
        pdata->lcd_aod_flag = state;
        TRAN_INFO("%s aod mode ",state?"enable":"disable");
    }

    ret = cts_plat_enable_irq_wake(pdata->pdata);
    if (ret) {
        cts_err("Enable IRQ wake failed %d", ret);
        return;
    }
    ret = cts_plat_enable_irq(pdata->pdata);
    if (ret) {
        cts_err("Enable IRQ failed %d", ret);
        return;
    }
}
//USD:modify for lcd aod by guoli.niu 20250321 end

//BSP:add for pocket mode by xihao.ding 20251129 start
static inline void set_pocket_mode_switch(u32 state)
{
    int ret = 0;
    struct chipone_ts_data *pdata = g_cts_data;
    if(pdata == NULL)
        return;
    if(state == 1){
        pdata->cts_dev.is_pocket_in = true;
    } else {
        pdata->cts_dev.is_pocket_in = false;
    }
    cts_info("is_pocket_in is %d",pdata->cts_dev.is_pocket_in);
    cts_info("enter tp large");
    if(pdata->pdata->pocket_function_support && pdata->cts_dev.is_pocket_in && !pdata->cts_dev.rtdata.suspended) {
        ret = cts_tcs_set_pocket_mode(&pdata->cts_dev, 01);
        if (ret) {
            cts_err("Enter pocket status failed %d", ret);
        }
    } else {
        ret = cts_tcs_set_pocket_mode(&pdata->cts_dev, 00);
        if (ret) {
            cts_err("close pocket status failed %d", ret);
        }
    }
}
//BSP:add for pocket mode by xihao.ding 20251129 end

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
        set_game_mode(state);
        break;

    case SET_EDGE_REJECT_MODE:
        set_edge_reject_mode(state);
        break;
    //USD:modify for lcd aod by yun.luo 20240527 start
    case LCD_AOD_MODE:
        set_lcd_aod_mode(state);
        break;
    //USD:modify for lcd aod by yun.luo 20240527 end

    //BSP:add for pocket mode by xihao.ding 20251129 start
    case SET_POCKET_MODE:
        set_pocket_mode_switch(state);
        break;
    //BSP:add for pocket mode by xihao.ding 20251129 end
    default:
        TRAN_INFO("can't find cmd=0x%x",cmd);
        break;
    }
}

static unsigned char get_firmware_version(void)
{
    struct chipone_ts_data *pdata = g_cts_data;
    u8 fw_ver = 0xff;

    if(pdata != NULL) {
        fw_ver = (pdata->cts_dev.fwdata.version)&0xff;
    }

    return fw_ver;
}

static unsigned char get_base_version(void)
{
    struct chipone_ts_data *pdata = g_cts_data;

    if(pdata != NULL)
        return pdata->bv;
    else
        return 0;

}

static int tp_selftest(void)
{
    int ret = 0;
    TRAN_FUNC_ENTER();
    ret = tran_get_SelftestResult(); 
    TRAN_FUNC_EXIT();
    return ret;
}

static int get_selftest_data(char * data)
{
    TRAN_FUNC_ENTER();

    return 0;
}

static int tran_selftest_open(struct inode *inode, struct file *file)
{
    TRAN_FUNC_ENTER();
    selftest_proc_open(inode, file); 
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
    if (controller == NULL) {
        TRAN_INFO("controller Invalid parameters,get ops fail !");
        return 0;
    }

    if(controller->tran_ts_data != NULL) {
        *ops = controller->tran_ts_data->tpd_ops;
        if(*ops != NULL)
            return 1;
    }

    TRAN_INFO("get ops fail !");
    return 0;
}
//USD: add one click gesture for lcd aod by yun.luo start
static struct vendor_gesture_map  gestures_maps[] = {
    {CTS_GESTURE_LEFT,         GESTURE_LF},
    {CTS_GESTURE_UP,           GESTURE_up},
    {CTS_GESTURE_RIGHT,        GESTURE_RT},
    {CTS_GESTURE_DOWN,         GESTURE_down},
    {CTS_GESTURE_D_TAP,        GESTURE_DC},
    {CTS_GESTURE_O_TAP,        GESTURE_OC},
    {CTS_GESTURE_W,            GESTURE_w},
    {CTS_GESTURE_O,            GESTURE_o},
    {CTS_GESTURE_M,            GESTURE_m},
    {CTS_GESTURE_E,            GESTURE_e},
    {CTS_GESTURE_C,            GESTURE_c},
    {CTS_GESTURE_Z,            GESTURE_z},
    {CTS_GESTURE_S,            GESTURE_s},
    {CTS_GESTURE_V,            GESTURE_v},
    {0,0},

};
//USD: add one click gesture for lcd aod by yun.luo end

static char *support_list[] = {
    "icnl9916x",
    "icnl9922cac01",
    "icnl9916c",
    "icnl9911s",
    "icnl9911c",
    "icnl9922c",
    "icnl9922",
    "icnl9951",
    "icnl9916",
    "icnl9911",
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

struct tran_ts_controller chipone_controller = {
    .cur_ic = NULL,
    .tran_ts_data = NULL,
    .selftest_data = NULL,
    .controller_state = TRAN_ACTIVE_MODE,
    .vendor_name = "chipone",
    .gesture_flag = true,
    .gesture_map = gestures_maps,
    .support_ic = support_list,
    .ops = &controller_ops,
    .ext_ops = &controller_ext_ops,
};

static int __init controller_module_init(void)
{
    init_completion(&chipone_controller.comp);
    register_ts_controller(&chipone_controller);
    return 0;
}

static void __exit controller_module_exit(void)
{
    /* 清除transsion core的资源 */
    unregister_ts_controller(&chipone_controller);

    /* 清除当前驱动的资源 */
    cts_driver_exit();

}

module_init(controller_module_init);
module_exit(controller_module_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
