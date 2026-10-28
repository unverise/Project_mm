#include "omnivision_tcm_core.h"
#include "omnivision_transsion.h"

extern struct ovt_tcm_hcd *g_tcm_hcd;

extern int register_ts_controller(struct tran_ts_controller *controller);
extern int unregister_ts_controller(struct tran_ts_controller *controller);

/*bsp:modify for bring up by guoxin.zhu 20230625 start*/
extern int ovt_tcm_suspend(struct device *dev);
extern int ovt_tcm_resume(struct device *dev);
extern int ovt_tcm_module_init(void);
extern void ovt_tcm_module_exit(void);
/*bsp:modify for bring up by guoxin.zhu 20230625 end*/

struct tran_ts_controller ovt_controller;

static int tran_ts_init(void)
{
    int ret = 0;
    TRAN_FUNC_ENTER();
    ovt_tcm_module_init();
    TRAN_FUNC_EXIT();
    return ret;
}

static void suspend(struct device *h ,int opt)
{
    struct ovt_tcm_hcd *tcm_hcd;
    tcm_hcd = g_tcm_hcd;
    if (tcm_hcd) {
        /* ps suspend */
        if(ovt_controller.controller_state == TRAN_PS_SUSPEND_MODE) {
            TRAN_INFO("ps suspend, do nothing");
            return;
        }

        if(ovt_controller.controller_state == TRAN_GESTURE_SUSPEND_MODE)
        {
            tcm_hcd->wakeup_gesture_enabled = 1;
        }
        ovt_tcm_suspend(&tcm_hcd->pdev->dev);
    }
}

static void resume(struct device *h , int opt)
{
    struct ovt_tcm_hcd *tcm_hcd = g_tcm_hcd;

    if (tcm_hcd) {
        /* ps resume */
        if(ovt_controller.controller_state == TRAN_PS_SUSPEND_MODE) {
            TRAN_INFO("ps suspend, do nothing");
            return;
        }

        ovt_tcm_resume(&tcm_hcd->pdev->dev);
        if(ovt_controller.controller_state == TRAN_GESTURE_SUSPEND_MODE)
            tcm_hcd->wakeup_gesture_enabled = 0;
    }

}
extern int ovt_tcm_set_func_face_detect_en_state(bool en);
static inline void  set_ps_mode(u32 state)
{
    if(state) {
        ovt_tcm_set_func_face_detect_en_state(1);
    } else {
        ovt_tcm_set_func_face_detect_en_state(0);
    }
}

//BSP: add to tp touch up report by linsheng.liu20230705 start
static int32_t set_edge_reject_switch(uint8_t edge_reject_switch)
{
    unsigned short buf = 0;
    int32_t ret = 0;
    struct ovt_tcm_hcd *tcm_hcd = g_tcm_hcd;
    if (IS_ERR_OR_NULL(tcm_hcd))
    {
        return 0;
    }
    LOGI(tcm_hcd->pdev->dev.parent,"++\n");
    LOGI(tcm_hcd->pdev->dev.parent, "set edge reject switch: %d\n", edge_reject_switch);

    //BSP: add KGLKGHLEYB-415 by tao.lan 20210820 start
    switch(edge_reject_switch) {
    case 0:
        buf = 1;//Vertical screen
        break;
    case 1:
        buf = 3;//Horizontal left of receiver
        break;
    case 3:
        buf = 4;//Horizontal right of receiver
        break;
    default:
        buf = 1;//Vertical screen
        break;
    }
    //BSP: add KGLKGHLEYB-415 by tao.lan 20210820 end
    //USD: modify for KL8H353-78 by yun.luo start
    if (tcm_hcd->in_suspend == false) {
        ret = tcm_hcd->set_dynamic_config(tcm_hcd, DC_ENABLE_ROATE_HORIZONTAL_LEVEL, buf);
        if (ret < 0) {
            LOGE(tcm_hcd->pdev->dev.parent, "Write edge reject switch command fail!\n");
        }
    } else {
        tcm_hcd->func_roate_horizontal_level_en = true;
        tcm_hcd->func_roate_status = buf;
        printk("[omnivision]set edge_reject_switch[%d] in suspend.....\n",buf);
    }
    //USD: modify for KL8H353-78 by yun.luo end
    LOGI(tcm_hcd->pdev->dev.parent,"--\n");
    return ret;
}

static int32_t charger_connect_detect(uint8_t charger_connect)
{
    int32_t ret = 0;
    struct ovt_tcm_hcd *tcm_hcd = g_tcm_hcd;
    if (IS_ERR_OR_NULL(tcm_hcd))
    {
        return 0;
    }
    //USD: modify for KL8H353-78 by yun.luo start
    if (tcm_hcd->in_suspend == false) {
        ret = tcm_hcd->set_dynamic_config(tcm_hcd, DC_CHARGER_CONNECTED, charger_connect);
        if (ret < 0) {
            LOGE(tcm_hcd->pdev->dev.parent, "Write charger_connect %d command fail!\n",charger_connect);
        }
    } else {
        tcm_hcd->func_charger_connected_en = true;
        tcm_hcd->func_charger_status = charger_connect;
        printk("[omnivision]set charger[%d] in suspend.....\n",charger_connect);
    }
    //USD: modify for KL8H353-78 by yun.luo end
    return ret;
}

//USD:modify for lcd aod by yun.luo 20240715 start
static void set_lcd_aod_status(u32 state)
{
    struct ovt_tcm_hcd *tcm_hcd = g_tcm_hcd;
    int ret = 0;

    if ((IS_ERR_OR_NULL(tcm_hcd)) || (tcm_hcd->ovt_tcm_driver_removing)) {
        TRAN_INFO("tcm shutdown, do nothing");
        return;
    }

    if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) || atomic_read(&tcm_hcd->host_downloading) || !atomic_read(&tcm_hcd->hdl_done)) {
        TRAN_INFO("not ready , set lcd_aod_state = true");
        tcm_hcd->lcd_aod_state = true;
    } else {
        ret = tcm_hcd->set_dynamic_config(tcm_hcd, DC_IN_WAKEUP_GESTURE_MODE, state);
        if (0 == ret) {
            tcm_hcd->lcd_aod_state = true;
            //USD:modify for mode 2->1->2 tp exit mode 1 fail by tuo.wang5 20250327 start
            tcm_hcd->in_suspend = true;
            //USD:modify for mode 2->1->2 tp exit mode 1 fail by tuo.wang5 20250327 end
            TRAN_INFO("Success to recovery gesture in aod status.");
        } else {
            TRAN_INFO("Failed to recovery gesture in aod status:%d", ret);
        }
    }
}
//USD:modify for lcd aod by yun.luo 20240715 end

//BSP:add for pocket mode by xihao.ding 20251129 start
static int32_t set_pocket_mode_switch(uint8_t state)
{
    int32_t ret = 0;
    struct ovt_tcm_hcd *tcm_hcd = g_tcm_hcd;
    if (IS_ERR_OR_NULL(tcm_hcd))
    {
        return 0;
    }
    if(state == 1){
        tcm_hcd->is_pocket_in = true;
    } else {
        tcm_hcd->is_pocket_in = false;
    }
    printk("[omnivision]set_pocket_mode_switch = [%d]\n", tcm_hcd->is_pocket_in);
    if (tcm_hcd->in_suspend == false && tcm_hcd->hw_if->bdata->pocket_function_support) {
        printk("[omnivision]set_pocket_mode_switch cmd= [%d]\n",state);
        ret = tcm_hcd->set_dynamic_config(tcm_hcd, 0x11, state);//0x11 reg is for plam  1: enable & 0: disbale
        if (ret < 0) {
            LOGI(tcm_hcd->pdev->dev.parent, "Write set_pocket_mode_switch %d command fail!\n",state);
        }
    }
    return ret;
}
//BSP:add for pocket mode by xihao.ding 20251129 end

static void set_ts_mode(u32 cmd, u32 state)
{
    switch(cmd) {
    case SET_NOISE_MODE:
        //USD: modify for KL8H353-78 by yun.luo start
        charger_connect_detect(state);
        //USD: modify for KL8H353-78 by yun.luo start
        break;

    case SET_PS_MODE:
        set_ps_mode(state);
        break;

    case SET_GAME_MODE:
        break;

    case SET_EDGE_REJECT_MODE:
        set_edge_reject_switch(state);
        break;
    //USD:modify for lcd aod by yun.luo 20240701 start
    case LCD_AOD_MODE:
        set_lcd_aod_status(state);
        break;
    //USD:modify for lcd aod by yun.luo 20240701 end

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
//BSP: add to tp touch up report by linsheng.liu20230705 end
u8 fts_fwupg_get_ver_in_tp_call_transsion(void)
{
    u8 fw_ver_in_tp = 0;

    return fw_ver_in_tp;
}

static unsigned char get_firmware_version(void)
{
    u8 ret = 0;

    if (g_tcm_hcd) {
        ret = g_tcm_hcd->app_info.customer_config_id[15];
    }

    return ret;
}

static unsigned char get_base_version(void)
{
    u8 ret = 0;

    if (g_tcm_hcd) {
        ret = g_tcm_hcd->bv;
    }

    return ret;
}

//BSP: modify LGQNAEEEX-1 by linsheng.liu 20230630 start
static int tp_selftest(void)
{
    int ret = 0;
    int omn_test_result = 0;
    TRAN_FUNC_ENTER();
    omn_test_result = lct_ovt_tp_selftest();
    if(omn_test_result==2)
    {
        ret = 1;
    }
    TRAN_INFO("test_result:%d.", ret);
    TRAN_FUNC_EXIT();
    return ret;

}
//BSP: modify LGQNAEEEX-1 by linsheng.liu 20230630 end
static int get_selftest_data(char * data)
{
    TRAN_FUNC_ENTER();

    tp_selftest();

    return 0;
}

static int tran_selftest_open(struct inode *inode, struct file *file)
{
    TRAN_FUNC_ENTER();
    return 0;
}

static void resume_by_ddi(void)
{
    /* do nothing */
}

//BSP: modify tp reset for lcd call by yun.luo 20231227 start
static void tp_reset_level(int val)
{
    if (!IS_ERR_OR_NULL(g_tcm_hcd)) {
        const struct ovt_tcm_board_data *bdata = g_tcm_hcd->hw_if->bdata;

        if (gpio_is_valid(bdata->reset_gpio)) {
            gpio_set_value(bdata->reset_gpio, val);
        } else {
            TRAN_ERROR("Set tp reset val %d fail , gpio is invalid!!!",val);
            return;
        }

        TRAN_INFO("Set tp reset val = %d",val);
    }
}
//BSP: modify tp reset for lcd call by yun.luo 20231227 end

/* 1: success 0: fail */
int get_tpd_ops(struct tran_ts_controller *controller, struct tran_core_ops **ops)
{
    if(!IS_ERR_OR_NULL(controller->tran_ts_data)) {
        *ops = controller->tran_ts_data->tpd_ops;
        if(!IS_ERR_OR_NULL(*ops))
            return 1;
    }

    TRAN_INFO("get ops fail !");
    return 0;
}

#define OVT_GESTURE_C               99
#define OVT_GESTURE_W               119
#define OVT_GESTURE_V               5
#define OVT_GESTURE_DOUBLECLICK     1
#define OVT_GESTURE_Z               122
#define OVT_GESTURE_M               109
#define OVT_GESTURE_O               16
#define OVT_GESTURE_E               101
#define OVT_GESTURE_S               115
#define OVT_GESTURE_UP              72
#define OVT_GESTURE_DOWN            68
#define OVT_GESTURE_LEFT            66
#define OVT_GESTURE_RIGHT           65
//USD:modify for lcd aod by yun.luo 20240701 start
#define OVT_GESTURE_ONECLICK        30

static struct vendor_gesture_map  gestures_maps[] = {
    {OVT_GESTURE_LEFT,        GESTURE_LF},
    {OVT_GESTURE_UP,          GESTURE_up},
    {OVT_GESTURE_RIGHT,       GESTURE_RT},
    {OVT_GESTURE_DOWN,        GESTURE_down},
    {OVT_GESTURE_DOUBLECLICK, GESTURE_DC},
    {OVT_GESTURE_W,           GESTURE_w},
    {OVT_GESTURE_O,           GESTURE_o},
    {OVT_GESTURE_M,           GESTURE_m},
    {OVT_GESTURE_E,           GESTURE_e},
    {OVT_GESTURE_C,           GESTURE_c},
    {OVT_GESTURE_Z,           GESTURE_z},
    {OVT_GESTURE_S,           GESTURE_s},
    {OVT_GESTURE_V,           GESTURE_v},
    {OVT_GESTURE_ONECLICK,    GESTURE_OC},
    {0,0},
};
//USD:modify for lcd aod by yun.luo 20240701 end
static char *support_list[] = {
    "td4160b",
    "td4160",
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

struct tran_ts_controller ovt_controller = {
    .cur_ic = NULL,
    .tran_ts_data = NULL,
    .selftest_data = NULL,
    .controller_state = TRAN_ACTIVE_MODE,
    .vendor_name = "omnivision",
    .gesture_flag = true,
    .gesture_map = gestures_maps,
    .support_ic = support_list,
    .ops = &controller_ops,
    .ext_ops = &controller_ext_ops,
    .dsp_event_mutex = __MUTEX_INITIALIZER(ovt_controller.dsp_event_mutex),
};

static int __init controller_module_init(void)
{
	init_completion(&ovt_controller.comp);
    register_ts_controller(&ovt_controller);
    return 0;
}

static void __exit controller_module_exit(void)
{
    /* 清除transsion core的资源 */
    unregister_ts_controller(&ovt_controller);

    /* 清除当前驱动的资源 */
    ovt_tcm_module_exit();
}

module_init(controller_module_init);
module_exit(controller_module_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
