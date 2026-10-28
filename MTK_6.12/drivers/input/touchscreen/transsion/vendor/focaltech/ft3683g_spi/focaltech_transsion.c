#include "focaltech_core.h"
#include "./focaltech_test/focaltech_test.h"
#include "focaltech_transsion.h"

#define CHECK_DATA_PTR(ptr) \
    do { \
        if (unlikely(!(ptr))) { \
            TRAN_ERROR("Invalid pointer: %s", #ptr); \
            return; \
        } \
    } while(0)
extern int register_ts_controller(struct tran_ts_controller *controller);
extern int unregister_ts_controller(struct tran_ts_controller *controller);
static void tran_thp_ops_send_msg(const int msg);
struct tran_ts_controller ft3683g_controller;
static int g_keypad_status = 0;
static int g_scene_status = -1;
static int g_rotation = -1;
static int g_pocket_status = 0;

static int fts_ts_spi_init(void)
{
    int ret = 0;
    TRAN_FUNC_ENTER();
    ft3681_init();
    FTS_FUNC_EXIT();
    return ret;
}

static void suspend(struct device *h, int opt)
{
    struct fts_ts_data *ts_data = fts_data;

    if(opt == 1 && ts_data != NULL) {
        /* force suspend */
        ts_data->suspended = false;
    }
    fts_ts_suspend(NULL);
    tran_thp_ops_send_msg(TRAN_NL_SCREEN_OFF);
}

static void resume(struct device *h, int opt)
{
    tran_thp_ops_send_msg(TRAN_NL_SCREEN_ON);
    fts_ts_resume(NULL);
}

/*USD: add for pocket anti accidental touch by longtao.ran 20240605 start*/
static int fts_pocket_mode_switch(u8 state)
{
    int ret = 0;

    if (fts_data->pdata->pocket_func_support) {
        if(state == 1){
            ret = fts_write_reg(FTS_POCKET_SWITCH, FTS_POCKET_OPEN);
        } else {
            ret = fts_write_reg(FTS_POCKET_SWITCH, FTS_POCKET_CLOSE);
        }
        if (ret < 0) {
            FTS_ERROR("write FTS_POCKET_SWITCH fail, ret:%d", ret);
            return ret;
        }
        return 1;
    }

    return 0;
}
/*USD: add for pocket anti accidental touch by longtao.ran 20240605 start*/

/*BSP: add for ESport1.0 X6850H895-5 by longtao.ran 20231018 start*/
//Esports ops
/*USD: modify report rate 240hz by linsheng.liu 20241205 start*/
static void fts_esport_feature_switch(u32 cmd, u8 state)
{
    u8 tran_report_rate = 0;
    u8 writebuf[6] = { 0 };
    int ret;

    switch (cmd) {
    case LOCK_POINT_SWITCH:
        fts_data->tran_lock_point_value = state;
        break;

    case REPORT_RATE_SWITCH:
        tran_report_rate = state;
        if (tran_report_rate == 0 || tran_report_rate == 1 || tran_report_rate == 2) {
            fts_data->tran_report_rate_value = 0x00;
        } else if (tran_report_rate == 3) {
            fts_data->tran_report_rate_value = 0x01;
        } else {
            fts_data->tran_report_rate_value = 0x02;
        }
        break;

    case TRANSIENT_SAMPLING_RATE_SWITCH:
        fts_data->tran_transient_report_rate_value = state;
        break;
    case MICRO_CONTROL_ACCURACY:
        fts_data->tran_micro_control_accuracy = state;
        break;

    default:
        FTS_ERROR("It's not support cmd=0x%x", cmd);
        break;
    }
    fts_data->tran_esprots_switch = fts_data->tran_transient_report_rate_value
                                    || fts_data->tran_report_rate_value
				    || fts_data->tran_micro_control_accuracy
                                    || fts_data->tran_lock_point_value;
    writebuf[0] = FTS_REG_TRAN_ESPROTS;
    writebuf[1] = fts_data->tran_esprots_switch;
    writebuf[2] = fts_data->tran_transient_report_rate_value;
    writebuf[3] = 0;
    writebuf[4] = fts_data->tran_lock_point_value;
    //USD: add for esports1.1 feature by junyu.chen2 20250311 start
    writebuf[5] = fts_data->tran_micro_control_accuracy;
    ret = fts_write(writebuf, 6);
    //USD: add for esports1.1 feature by junyu.chen2 20250311 end
    if (ret < 0) {
        FTS_ERROR("write esprots reg fail");
    }
}
/*USD: modify report rate 240hz by linsheng.liu 20241205 end*/
/*BSP: add for ESport1.0 X6850H895-5 by longtao.ran 20231018 end*/
//BSP: add for custom mistouch by linsheng.liu 20231019 start
static void fts_custom_mistouch_switch(void)
{
    u8 writebuf[41] = { 0 };
    int ret;
    struct tran_ts_core *ctr = ft3683g_controller.tran_ts_data;
    if (fts_data->pdata->custom_mistouch_support) {
        writebuf[0] = 0xc3;
        for (ret = 0; ret < 5; ret++) {
            writebuf[1 + ret * 8] = ctr->mistouch[ret].LeftStartX_H;
            writebuf[2 + ret * 8] = ctr->mistouch[ret].LeftStartX_L;
            writebuf[3 + ret * 8] = ctr->mistouch[ret].LeftStartY_H;
            writebuf[4 + ret * 8] = ctr->mistouch[ret].LeftStartY_L;
            writebuf[5 + ret * 8] = ctr->mistouch[ret].RightStartX_H;
            writebuf[6 + ret * 8] = ctr->mistouch[ret].RightStartX_L;
            writebuf[7 + ret * 8] = ctr->mistouch[ret].RightStartY_H;
            writebuf[8 + ret * 8] = ctr->mistouch[ret].RightStartY_L;
        }
        if (fts_data->log_level >= 4) {
            FTS_DEBUG("custom mistouch data+++++++++++\n");
        for (ret = 0; ret < 41; ret++) {
                FTS_DEBUG("0x%x ", writebuf[ret]);
        }
            FTS_DEBUG("custom mistouch data--------------\n");
        }
        ret = fts_write(writebuf, 41);
        if (ret < 0) {
            FTS_ERROR("write custom mistouch reg fail");
        }
    }
}
//BSP: add for custom mistouch by linsheng.liu 20231019 end

//USD: Add for tiktok edg by junyu.chen2 20250313 start
static void fts_set_edg_level(u32 state)
{
    int ret = 0;

    if (state == 4) {
        ret = fts_write_reg(0x9c, 1);//enter tiktok
        ret = fts_write_reg(0x8d, 3);
    } else {
        ret = fts_write_reg(0x8d, state);
        ret = fts_write_reg(0x9c, 0);
    }

    if (ret < 0) {
        FTS_ERROR("write edge level fail");
    }else{
        FTS_INFO("write edge level %d", state);
    }
}
//USD: Add for tiktok edg by junyu.chen2 20250313 end
static void tran_thp_ops_send_msg(const int msg)
{
    struct fts_ts_data *ts_data = fts_data;

    CHECK_DATA_PTR(ts_data);
    if (!ts_data->thp_data_flag) {
        return;
    }
    switch (msg) {
        case TRAN_NL_WIRELESS_CHARGE:
            g_thp_scene.charging_status = 2;
            break;
        case TRAN_NL_TOUCH_CHARGIN:
            g_thp_scene.charging_status = 1;
            break;
        case TRAN_NL_TOUCH_CHARGOUT:
            g_thp_scene.charging_status = 0;
            break;
        case TRAN_NL_ROTATION_DOWN:
            g_thp_scene.orientation = 0;
            break;
        case TRAN_NL_ROTATION_RIGHT:
            g_thp_scene.orientation = 1;
            break;
        case TRAN_NL_ROTATION_LEFT:
            g_thp_scene.orientation = 3;
            break;
        case TRAN_NL_SCREEN_LOCK:
            g_thp_scene.pocket_mode = 1;
            break;
        case TRAN_NL_SCREEN_UNLOCK:
            g_thp_scene.pocket_mode = 0;
            break;
        case TRAN_NL_SCENE_NORMAL:
            g_thp_scene.game_mode = 0;
            g_thp_scene.video_mode = 0;
            g_thp_scene.camera_mode = 0;
            g_thp_scene.res_mode &= 0xfffffffe;
            break;
        case TRAN_NL_SCENE_VIDEO:
            g_thp_scene.video_mode = 1;
            break;
        case TRAN_NL_SCENE_GAME:
            g_thp_scene.game_mode = 1;
            break;
        case TRAN_NL_SCENE_CAMERA:
            g_thp_scene.camera_mode = 1;
            break;
        case TRAN_NL_SCENE_DIAL_PAD:
            g_thp_scene.res_mode |= 0x01;
            break;
        case TRAN_NL_KEYPAD_ON:
            g_thp_scene.typing_mode = 1;
            break;
        case TRAN_NL_KEYPAD_OFF:
            g_thp_scene.typing_mode = 0;
            break;
        case TRAN_NL_SCREEN_OFF:
        case TRAN_NL_SCREEN_ON:
            g_thp_scene.is_awake = (msg - TRAN_NL_SCREEN_OFF);
            break;
        default:
            FTS_ERROR("Unknown message: %d", msg);
            break;
    }
    ts_data->thp_data->q_ops->send_msg(msg);
}

static void handle_charger_mode(u32 state)
{
    if (state == 2) {
        tran_thp_ops_send_msg(TRAN_NL_WIRELESS_CHARGE);
    } else if (state == 1) {
        tran_thp_ops_send_msg(TRAN_NL_TOUCH_CHARGIN);
    } else if (state == 0) {
        tran_thp_ops_send_msg(TRAN_NL_TOUCH_CHARGOUT);
    }
}
static void handle_rotation_mode(u32 state)
{
    if (state == 0 && g_rotation != 0) {
        tran_thp_ops_send_msg(TRAN_NL_ROTATION_DOWN);
        g_rotation = 0;
    } else if (state == 1 && g_rotation != 1) {
        tran_thp_ops_send_msg(TRAN_NL_ROTATION_RIGHT);
        g_rotation = 1;
    } else if (state == 3 && g_rotation != 2) {
        tran_thp_ops_send_msg(TRAN_NL_ROTATION_LEFT);
        g_rotation = 2;
    }
}

static void handle_pocket_mode(u32 state)
{
    if (state == 1 && g_pocket_status == 0) {
        tran_thp_ops_send_msg(TRAN_NL_SCREEN_LOCK);
        g_pocket_status = 1;
    } else if (state == 0 && g_pocket_status == 1) {
        tran_thp_ops_send_msg(TRAN_NL_SCREEN_UNLOCK);
        g_pocket_status = 0;
    }
}

static void handle_scene_status(u32 state)
{
    int i = 0;
    static const struct {
        u32 state;
        u32 netlink_msg;
    } scene_map[] = {
        {0, TRAN_NL_SCENE_NORMAL},
        {1, TRAN_NL_SCENE_VIDEO},
        {2, TRAN_NL_SCENE_GAME},
        {3, TRAN_NL_SCENE_CAMERA},
        {4, TRAN_NL_SCENE_DIAL_PAD},
    };

    for (i = 0; i < ARRAY_SIZE(scene_map); i++) {
        if (state == scene_map[i].state && g_scene_status != scene_map[i].state) {
            tran_thp_ops_send_msg(scene_map[i].netlink_msg);
            g_scene_status = scene_map[i].state;
            break;
        }
    }
}

static void handle_keypad_status(u32 state)
{
    if (state == 1 && g_keypad_status == 0) {
        tran_thp_ops_send_msg(TRAN_NL_KEYPAD_ON);
        g_keypad_status = 1;
    } else if (state == 0 && g_keypad_status == 1) {
        tran_thp_ops_send_msg(TRAN_NL_KEYPAD_OFF);
        g_keypad_status = 0;
    }
}
static void set_ts_mode(u32 cmd, u32 state)
{
    CHECK_DATA_PTR(fts_data);
    switch (cmd) {

    case SET_NOISE_MODE:
        handle_charger_mode(state);
        fts_write_reg(FTS_REG_CHARGER_MODE_EN, state ? ENABLE : DISABLE);
        break;

    case SET_PS_MODE:
        break;

    case SET_GAME_MODE:
        break;

    case SET_EDGE_REJECT_MODE:
        if (fts_data->edge_mode) {
            fts_write_reg(0x8c, 0xaa);
            handle_rotation_mode(state);
        } else {
            handle_rotation_mode(state);
        mutex_lock(&fts_data->input_dev->mutex);
        fts_set_edge_reject_switch(state);
        mutex_unlock(&fts_data->input_dev->mutex);
        }
        break;

    /* For esport feature start*/
    case LOCK_POINT_SWITCH:
    case REPORT_RATE_SWITCH:
    case TRANSIENT_SAMPLING_RATE_SWITCH:
    case MICRO_CONTROL_ACCURACY:
        fts_esport_feature_switch(cmd, state);
        break;
    /* For esport feature end*/

    case SET_EDGE_LEVER_MODE:
        fts_set_edg_level(state);
        break;
    case SET_CUSTOM_MISTOUCH_MODE:
        fts_custom_mistouch_switch();
        break;
    /* For pocket mode start*/

    case SET_POCKET_MODE:
        handle_pocket_mode(state);
        fts_pocket_mode_switch(state);
        break;

    /* For pocket mode end*/
    case GET_KEYPAD_STATUS:
        handle_keypad_status(state);
        break;

    case GET_SCENE_STATUS:
        handle_scene_status(state);
        break;

    default:
        TRAN_INFO("can't find cmd=0x%x", cmd);
        break;
    }
}

static int fts_fwupg_get_ver_in_tp(u8 *ver)
{
    int ret = 0;

    if (NULL == ver) {
        FTS_ERROR("ver is NULL");
        return -EINVAL;
    }

    ret = fts_read_reg(FTS_REG_FW_VER, ver);
    if (ret < 0) {
        FTS_ERROR("read fw ver from tp fail");
        return ret;
    }

    return 0;
}

u8 fts_fwupg_get_ver_in_tp_call_transsion(void)
{
    int ret = 0;
    u8 fw_ver_in_tp = 0;

    ret = fts_fwupg_get_ver_in_tp(&fw_ver_in_tp);
    if (ret < 0) {
        FTS_ERROR("get fw ver in tp fail");
        return false;
    }

    FTS_INFO("fw version in tp:%x", fw_ver_in_tp);

    return fw_ver_in_tp;
}

static unsigned char get_firmware_version(void)
{
    u8 ret = 0;

    TRAN_FUNC_ENTER();
    ret = fts_fwupg_get_ver_in_tp_call_transsion();

    return ret;
}

static unsigned char get_base_version(void)
{
    u8 ret = 0;

    TRAN_FUNC_ENTER();
    ret = fts_data->bv;
    return ret;
}
static char *get_algo_version(void)
{
    struct fts_ts_data *ts_data = fts_data;

    if (ts_data == NULL || !ts_data->thp_data_flag || ts_data->thp_data == NULL) {
        FTS_ERROR("ts_data is NULL or Not support THP");
        return NULL;
    }
    if (false == ts_data->thp_data->thp_mode) {
        FTS_INFO("In mcu mode");
        return NULL;
    }
    return ts_data->thp_data->ver_info.algo_version;
}

static char func_name[64] = {0};
static char *get_algo_func(void)
{
    struct fts_ts_data *ts_data = fts_data;
    const char *features[] = {"Palm", "Rain_touch", "NTT", "Gloves", "UWT", "Film"};
    int feature_flags[sizeof(features) / sizeof(features[0])] = {0}; // 动态确定数组大小
    int len = 0;

    if (ts_data == NULL || !ts_data->thp_data_flag || ts_data->thp_data == NULL) {
        FTS_ERROR("ts_data is NULL or thp_data_flag is false or thp_data is NULL");
        return NULL;
    }

    // set feature flags based on ts_data->support_* flags
    //feature_flags[0] = 1;//ts_data->support_palm;
    feature_flags[1] = ts_data->support_rain_touch;
    //feature_flags[2] = ts_data->support_ntt;
    //feature_flags[3] = ts_data->support_gloves;

    int has_feature = 0;
    for (int i = 0; i < sizeof(feature_flags) / sizeof(feature_flags[0]); i++) {
        if (feature_flags[i]) {
            has_feature = 1;
            break;
        }
    }
    if (!has_feature) {
        FTS_INFO("no THP feature enabled");
        return NULL;
    }

    for (int i = 0; i < sizeof(features) / sizeof(features[0]); i++) {
        if (feature_flags[i]) {
            if (len > 0) {
                len += snprintf(func_name + len, sizeof(func_name) - len, ",");
            }
            len += snprintf(func_name + len, sizeof(func_name) - len, "%s", features[i]);
        }
    }

    FTS_INFO("used size: %d, max size: 64, func_name: %s", len, func_name);
    return func_name;
}

static int tp_selftest(void)
{
    int ret = 0;

    TRAN_FUNC_ENTER();
    ret = tran_fts_selftest();
    TRAN_INFO("fts_test_result:%d.", ret);
    TRAN_FUNC_EXIT();
    if (ret == 2) {
        return 1;
    } else {
        return 0;
    }
}

static int get_selftest_data(char *data)
{
    TRAN_FUNC_ENTER();

    return tran_fts_selftest();
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

static void tp_reset_level(int val)
{
    /* do noting */
}

static void tran_set_irq_status(uint8_t en)
{
    if (!IS_ERR_OR_NULL(fts_data)) {
        if (en) {
            fts_irq_enable();
        } else {
            fts_irq_disable();
        }
    }
}
/* 1: success 0: fail */
int get_tpd_ops(struct tran_ts_controller *controller, struct tran_core_ops **ops)
{
    if (controller->tran_ts_data != NULL) {
        *ops = controller->tran_ts_data->tpd_ops;
        if (!IS_ERR_OR_NULL(*ops))
            return 1;
    }

    TRAN_INFO("get ops fail !");
    return 0;
}

/*BSP: add for ESport1.0 X6850H895-5 by longtao.ran 20231220 start*/
int get_lock_point_distance(u8 *data)
{
    int ret = 0;

    if (NULL == data) {
        FTS_ERROR("data is NULL");
        return -EINVAL;
    }
    ret = fts_read_reg(0xB5, data);
    if (ret < 0) {
        FTS_ERROR("read fw lock point distance from tp fail");
        return ret;
    }
    return 0;
}
/*BSP: add for ESport1.0 X6850H895-5 by longtao.ran 20231220 end*/

static struct vendor_gesture_map  fts_gestures_maps[] = {
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
    {GESTURE_ONECLICK,    GESTURE_OC},
    {0, 0},
};

static char *support_list[] = {
    "ft3683g",
    "ft3682g",
    NULL /* not modify */
};

static struct tran_controller_ops controller_ops = {
    .tran_ts_init = fts_ts_spi_init,
    .tran_ts_suspend = suspend,
    .tran_ts_resume = resume,
    .set_ts_mode = set_ts_mode,
    .get_firmware_version = get_firmware_version,
    .get_base_version = get_base_version,
    .get_algo_version = get_algo_version,
    .get_algo_func = get_algo_func,
    .tran_selftest = tp_selftest,
    .get_selftest_data = get_selftest_data,
    .tran_selftest_open = tran_selftest_open,
    .get_lock_point_distance = get_lock_point_distance,
/* USD:add for CN5c underwater by qixuan.qin 20250910 start */
    .set_underwater_status = set_underwater_status,
    .get_underwater_status = get_underwater_status,
/* USD:add for CN5c underwater by qixuan.qin 20250910 end */
};

static struct tran_controller_ext_ops controller_ext_ops = {
    .tran_tp_reset = tp_reset_level,
    .resume_by_ddi = resume_by_ddi,
    .irq_enable_en = tran_set_irq_status,
};

struct tran_ts_controller ft3683g_controller = {
    .cur_ic = NULL,
    .tran_ts_data = NULL,
    .selftest_data = NULL,
    .gesture_flag = true,
    .controller_state = TRAN_ACTIVE_MODE,
    .vendor_name = "focaltech",
    .gesture_map = fts_gestures_maps,
    .support_ic = support_list,
    .ops = &controller_ops,
    .ext_ops = &controller_ext_ops,
    .dsp_event_mutex = __MUTEX_INITIALIZER(ft3683g_controller.dsp_event_mutex),
};


static int __init fts_device_init(void)
{
    init_completion(&ft3683g_controller.comp);
    register_ts_controller(&ft3683g_controller);
    return 0;
}

static void __exit fts_device_exit(void)
{
    /* 清除当前驱动的资源 */
    ft3681_exit();
    /* 清除transsion core的资源 */
    unregister_ts_controller(&ft3683g_controller);
}

module_init(fts_device_init);
module_exit(fts_device_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");

