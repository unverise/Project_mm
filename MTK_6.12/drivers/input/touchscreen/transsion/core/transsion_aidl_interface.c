#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/device.h>

#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/cdev.h>

#include "transsion_core.h"
#include "transsion_gesture.h"

/*
 * ------------------------------
 * Constants
 * ------------------------------
 */
#define POLL_DEVICE_NAME    "tppolldrv"
#define POLL_CLASS_NAME     "tppollclass"
#define MAX_DATA_LENGTH      256

/*
 * ------------------------------
 * Struct Definitions
 * ------------------------------
 */
struct IoctlData {
        int length;  // 数据长度
        char data[256]; // 数据缓冲区
};

struct ioctlHandler {
    unsigned int ioctlFlag;
    bool(*ioctlHandle)(struct IoctlData *, unsigned int);
//    struct work_struct work;
};

struct selftest_info {
    int result; // 0:success, 1:fail
    int work_execution_status; // 0:not run, 1:runing, 2:finished
    struct mutex status_mutex;
    #define SELFTEST_RESULT_FAILED 0
    #define SELFTEST_RESULT_SUCCESS 1
    #define SELFTEST_WORK_IDLE 2
    #define SELFTEST_WORK_RUNING 3
    #define SELFTEST_WORK_FINISHED 4
};

/*
 * ------------------------------
 * IOCTL Commands
 * ------------------------------
 */
/* 工模测试 */
#define  IOCTL_GET_VENDOR               _IOR('t', 1, struct IoctlData)

#define  IOCTL_GET_FW_VERSION           _IOR('t', 2, struct IoctlData)

#define  IOCTL_SET_SELFTEST_START       _IOW('t', 3, struct IoctlData)
#define  IOCTL_GET_SELFTEST_STATUS      _IOR('t', 3, struct IoctlData)

/* 手势 */
#define  IOCTL_GET_GESTURE_STATUS       _IOR('t', 4, struct IoctlData)
#define  IOCTL_GET_GESTURE_CODE         _IOR('t', 5, struct IoctlData)
#define  IOCTL_SET_GESTURE_STATUS       _IOW('t', 5, struct IoctlData)

/* 横屏抑制 */
#define  IOCTL_SET_ROTAION_STATUS       _IOW('t', 10, struct IoctlData)
#define  IOCTL_GET_ROTAION_STATUS       _IOR('t', 10, struct IoctlData)

/* 报点率 */
#define  IOCTL_SET_REPORT_RATE          _IOW('t', 11, struct IoctlData)
#define  IOCTL_GET_REPORT_RATE          _IOR('t', 11, struct IoctlData)

/* 大面积防误触 */
#define  IOCTL_SET_LARGE_MISTOUCH       _IOW('t', 12, struct IoctlData)
#define  IOCTL_GET_LARGE_MISTOUCH       _IOR('t', 12, struct IoctlData)

/* 曲面屏边缘防误触 */
#define  IOCTL_SET_EDGE_LEVEL           _IOW('t', 13, struct IoctlData)
#define  IOCTL_GET_EDGE_LEVEL           _IOR('t', 13, struct IoctlData)

/* 关机标志 */
#define  IOCTL_SET_SHUTDOWN_FLAG        _IOW('t', 16, struct IoctlData)
#define  IOCTL_GET_SHUTDOWN_FLAG        _IOR('t', 16, struct IoctlData)

/* 游戏区域防误触 */
#define  IOCTL_SET_MISTOUCH_MODE        _IOW('t', 17, struct IoctlData)
#define  IOCTL_GET_MISTOUCH_MODE        _IOR('t', 17, struct IoctlData)

/* 锁点距离(电竞引擎1.0) */
#define  IOCTL_SET_GAME_MODE            _IOW('t', 18, struct IoctlData)
#define  IOCTL_GET_GAME_MODE            _IOR('t', 18, struct IoctlData)

/* 游戏模式(电竞引擎阉割版) */
#define  IOCTL_SET_GAME_STATE           _IOW('t', 19, struct IoctlData)
#define  IOCTL_GET_GAME_STATE           _IOR('t', 19, struct IoctlData)

/* 瞬时采样率 */
#define  IOCTL_SET_TRANSIENT_RATE       _IOW('t', 20, struct IoctlData)
#define  IOCTL_GET_TRANSIENT_RATE       _IOR('t', 20, struct IoctlData)

/* 键盘弹出状态 */
#define  IOCTL_SET_IME_STATE            _IOW('t', 21, struct IoctlData)

/* USD:add for CNx underwater by qixuan.qin 20250910 start */
#define  IOCTL_SET_UNDERWATER_STATE     _IOW('t', 32, struct IoctlData)
#define  IOCTL_GET_UNDERWATER_STATE     _IOR('t', 32, struct IoctlData)
/* USD:add for CNx underwater by qixuan.qin 20250910 end */

/*
 * ------------------------------
 * Global Variables
 * ------------------------------
 */
static int major_number;
static bool data_available = false;
static char message[256] = {0};
static short message_size;

static wait_queue_head_t poll_queue;

static struct class* poll_class = NULL;
static struct cdev poll_cdev;
static struct device* poll_device = NULL;
static struct work_struct app_write_work;
static struct work_struct tp_selftest_work;
static struct workqueue_struct *ioctl_workqueue;
static struct selftest_info selftest_status;
//临界资源锁

static inline int tran_toupper(int c)
{
    if ((c >= 'a') && (c <= 'z'))
        return c + ('A' - 'a');
    return c;
}

int is_support_feature(enum feature_type type)
{
    struct touchscreen_feature *features = &g_tran_ts.features;

    switch(type) {
        case FEATURE_ESPORT:
            return features->esport;
        case FEATURE_FOD:
            return features->fod;
        case FEATURE_GAME_AMT:
            return features->game_amt;
        case FEATURE_POCKET_AMT:
            return features->pocket_amt;
        case FEATURE_EDGE_AMT:
            return features->curved_edge_amt;
        case FEATURE_DYNAMIC_REPORT_RATE:
            return features->dynamic_report_rate;
        case FEATURE_TP_SCENE:
            return features->tp_scene;
        case FEATURE_BASE_FUNCTION:
            return 1;
        default:
            return 0;
    }
}
/*
 * ------------------------------
 * IOCTL Handler works
 * ------------------------------
 */
static void app_write_call(struct work_struct *work)
{
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];
    if(message[0] == '1'){
        ctr->ops->set_ts_mode(GET_KEYPAD_STATUS, 1);
        TRAN_INFO("ime state set to true");
        g_tran_ts.is_ime_state = true;
    } else if(message[0] == '0'){
        ctr->ops->set_ts_mode(GET_KEYPAD_STATUS, 0);
        TRAN_INFO("ime state set to false");
        if(g_tran_ts.is_ime_state){
            if(g_tran_ts.bigdata_mt){
                if(g_tran_ts.platform_data->ops->bigdata_tp_mistouch){
                    g_tran_ts.platform_data->ops->bigdata_tp_mistouch();
                }
                g_tran_ts.bigdata_mt = 0;
                g_tran_ts.bigdata_tt = 0;
                g_tran_ts.is_ime_state = false;
            }
        }
    }
}

static void tp_self_test(struct work_struct *work)
{
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];

    mutex_lock(&selftest_status.status_mutex);
    selftest_status.work_execution_status = SELFTEST_WORK_RUNING;
    mutex_unlock(&selftest_status.status_mutex);

    selftest_status.result = ctr->ops->tran_selftest();
    TRAN_INFO("selftest_result:%d.", selftest_status.result);

    mutex_lock(&selftest_status.status_mutex);
    selftest_status.work_execution_status = SELFTEST_WORK_FINISHED;
    mutex_unlock(&selftest_status.status_mutex);
}
/*
 * ------------------------------
 * IOCTL Handler Start
 * ------------------------------
 */

/**
 * function: ioctl_vendor_cmd_handle
 * description: get the vendor name
 *
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_vendor_cmd_handle(struct IoctlData *pdata, unsigned int cmd) {
    int i;
    char ic_name[16] = {0};
    char *buf = (char *)pdata->data;
    int ic_name_len = 0;
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];

    TRAN_FUNC_ENTER();

    if(g_tran_ts.controller[TRAN_MAIN_CONTROLLER]) {
        ic_name_len = strlen(g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->cur_ic)>16?16:strlen(g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->cur_ic);
        memcpy(ic_name, g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->cur_ic, ic_name_len);

        for(i=0; i<ic_name_len; i++) {
            ic_name[i] = tran_toupper((int)ic_name[i]);
        }

        if(ctr->supplier->supplier_vendor != NULL) {
            pdata->length = snprintf(buf, MAX_DATA_LENGTH, "%s_%s_%02d", ctr->supplier->supplier_vendor, ic_name, ctr->supplier->supplier_num);
            if(pdata->length < MAX_DATA_LENGTH) {
                pdata->length += 1;
            }
            TRAN_INFO("vendor_name:%s len=%d", buf, pdata->length);
        }
    } else {
        pdata->length = snprintf(buf, MAX_DATA_LENGTH, "UNKNOWN");
        pdata->length += 1;
        TRAN_INFO("controller is NULL");
    }

    TRAN_FUNC_EXIT();
    return true;
}

/**
 * function: ioctl_fw_version_handle
 * description: get fw version number from tp
 *
 * Boundary Resources:
 * - bus(spi or i2c)
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_fw_version_handle(struct IoctlData *pdata, unsigned int cmd) {
    u8 fw_ver_in_tp = 0;
    u8 fw_ver_in_fs = 0;
    u8 config_ver = 0;
    char *buf = (char *)pdata->data;

    TRAN_FUNC_ENTER();

    if(g_tran_ts.controller[TRAN_MAIN_CONTROLLER]) {
        if(g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->ops->get_firmware_version && g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->ops->get_base_version) {
            fw_ver_in_tp = g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->ops->get_firmware_version();
            fw_ver_in_fs = g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->ops->get_base_version();
            if(g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->ops->get_config_version) {
                config_ver = g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->ops->get_config_version();
            }
            TRAN_INFO("fw:0x%02x bv:0x%02x", fw_ver_in_tp, fw_ver_in_fs);
        }

        if(strcmp("goodix",g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->vendor_name) == 0) {
            pdata->length = snprintf(buf, MAX_DATA_LENGTH, "0x%02x_0x%02x,bv:0x%02x_0x%02x\n", fw_ver_in_tp, config_ver, fw_ver_in_tp, config_ver);
            if(pdata->length < MAX_DATA_LENGTH) {
                pdata->length += 1;
            }
        } else {
            pdata->length = snprintf(buf, MAX_DATA_LENGTH, "0x%02x,bv:0x%02x\n", fw_ver_in_tp, fw_ver_in_fs);
            if(pdata->length < MAX_DATA_LENGTH) {
                pdata->length += 1;
            }
        }

    } else {
        pdata->length = snprintf(buf, MAX_DATA_LENGTH, "UNKNOWN");

        TRAN_INFO("controller is NULL");
    }

    TRAN_FUNC_EXIT();
    return true;
}

/**
 * function: ioctl_selftest_handle
 * description: deal with Capacitance factory test
 *
 * Boundary Resources:
 * - bus(spi or i2c)
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_selftest_handle(struct IoctlData *pdata, unsigned int cmd) {
    bool result = false;
    char s_test_pass[] = "Short Test PASS";
    char o_test_pass[] = "Open Test PASS";
    char s_test_fail[] = "Short Test FAIL";
    char o_test_fail[] = "Open Test FAIL";

    TRAN_FUNC_ENTER();
    switch(cmd) {
        case IOCTL_SET_SELFTEST_START:
            if(g_tran_ts.controller[TRAN_MAIN_CONTROLLER]) {
                if(!ioctl_workqueue) {
                    return result;
                }

                if (!queue_work(ioctl_workqueue, &tp_selftest_work)) {
                    TRAN_ERROR("Work is already on the queue\n");
                    return result;
                }
            } else {
                TRAN_ERROR("Controller is NULL\n");
                return result;
            }
            result = true;
            break;

        case IOCTL_GET_SELFTEST_STATUS:
            mutex_lock(&selftest_status.status_mutex);
            if(SELFTEST_WORK_FINISHED == selftest_status.work_execution_status) {
                if (SELFTEST_RESULT_SUCCESS == selftest_status.result) {
                    pdata->length += sprintf(pdata->data, "%s\n%s\n", s_test_pass, o_test_pass) + 1;
                } else {
                    pdata->length += sprintf(pdata->data, "%s\n%s\n", s_test_fail, o_test_fail) + 1;
                }

                selftest_status.work_execution_status = SELFTEST_WORK_IDLE;
                result = true;
            } else {
                TRAN_ERROR("Selftest work not finished, status: %d\n", selftest_status.work_execution_status);
                result = false;
            }
            mutex_unlock(&selftest_status.status_mutex);
            break;
        default:
            break;
    }
    TRAN_FUNC_EXIT();

    return result;
}

/**
 * function: ioctl_gesture_handle
 * description: deal with gesture commands
 *
 * Boundary Resources:
 * - gesture_maps
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_gesture_handle(struct IoctlData *pdata, unsigned int cmd) {
    bool result = false;

    TRAN_FUNC_ENTER();
    switch(cmd) {
        case IOCTL_GET_GESTURE_STATUS:
            pdata->length = get_gesture_status(pdata->data);
            result = true;
            break;

        case IOCTL_GET_GESTURE_CODE:
            pdata->length = get_gesture_code(pdata->data);
            result = true;
            break;

        case IOCTL_SET_GESTURE_STATUS:   
            if(0 != set_gesture_status(pdata->data, pdata->length)) {
                result = false;
            } else {
                result = true;
            }

            break;
        default:
            break;
    }
    TRAN_FUNC_EXIT();

    return result;
}

/**
 * function: ioctl_display_rotation_handle
 * description: deal with display rotation status
 *
 * Boundary Resources:
 * - bus(spi or i2c)
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_display_rotation_handle(struct IoctlData *pdata, unsigned int cmd) {
    int32_t ret;
    int32_t tmp;
    uint8_t edge_reject_switch;
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];
    bool result = false;

    TRAN_FUNC_ENTER();
    if (ctr == NULL) {
        TRAN_ERROR("controller is null");
        return result;
    }

    switch (cmd)    
    {
        case IOCTL_SET_ROTAION_STATUS:
            ret = sscanf(pdata->data, "%d", &tmp);
            if (ret != 1 || tmp > 4) {
                TRAN_ERROR("Invalid value! ret = %d, cmd = %d", ret, tmp);
                return result;
            }

            edge_reject_switch = (uint8_t)tmp;
            g_tran_ts.switch_mode = edge_reject_switch;
            if(ctr->controller_state == TRAN_SUSPEND_MODE) {
                TRAN_INFO("tp in sleep mode cancel this opration");
                return result;
            }
            ctr->ops->set_ts_mode(SET_EDGE_REJECT_MODE, edge_reject_switch);

            result = true;
            break;
        case IOCTL_GET_ROTAION_STATUS:
            pdata->length = sprintf(pdata->data, "%d\n", g_tran_ts.switch_mode) + 1;
            result = true;
            break;
        default:
            break;
    }
    TRAN_FUNC_EXIT();

    return result;
}

/**
 * function: ioctl_report_rate_handle
 * description: deal with report rate commands
 *
 * Boundary Resources:
 * - bus(spi or i2c)
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_report_rate_handle(struct IoctlData *pdata, unsigned int cmd) {
    bool result = false;
    TRAN_FUNC_ENTER();
    if (!is_support_feature(FEATURE_DYNAMIC_REPORT_RATE)) {
        return result;
    }

    /* not support */
    result = true;
    TRAN_FUNC_EXIT();

    return result;
}


/**
 * function: ioctl_large_mistouch_handle
 * description: deal with pocket anti accidental touch
 *
 * Boundary Resources:
 * - bus(spi or i2c)
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_large_mistouch_handle(struct IoctlData *pdata, unsigned int cmd) {
    int32_t ret;
    int32_t tmp;
    uint8_t tran_pocket_state;
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];
    bool result = false;

    TRAN_FUNC_ENTER();
    if (!is_support_feature(FEATURE_POCKET_AMT)) {
        return result;
    }

    if (ctr == NULL) {
        TRAN_ERROR("controller is null");
        return result;
    }

    switch (cmd) {
        case IOCTL_SET_LARGE_MISTOUCH:
            ret = sscanf(pdata->data, "%d", &tmp);
            if (ret != 1) {
                TRAN_ERROR("Invalid value! ret = %d", ret);
                return result;
            }
            if (tmp != 0 && tmp != 1) {
                TRAN_ERROR("Invalid value! ret = %d, tran_pocket_state val = %d", ret, tmp);
                return result;
            }

            tran_pocket_state = tmp;
            TRAN_INFO("tran_pocket_state = %d", tran_pocket_state);

            g_tran_ts.tran_pocket_state = tran_pocket_state;
            if (TRAN_SUSPEND_MODE == ctr->controller_state) {
                TRAN_ERROR("TP already suspended do not tran_pocket_state");
            } else {
                ctr->ops->set_ts_mode(SET_POCKET_MODE, tran_pocket_state);
            }

            result = true;
            break;

        case IOCTL_GET_LARGE_MISTOUCH:
            pdata->length = sprintf(pdata->data, "%d\n", g_tran_ts.tran_pocket_state) + 1;
            result = true;
            break;
        default:
            break;
    }
    TRAN_FUNC_EXIT();

    return result;
}


/**
 * function: ioctl_edge_level_handle
 * description: deal with edge level commands
 *
 * Boundary Resources:
 * - bus(spi or i2c)
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_edge_level_handle(struct IoctlData *pdata, unsigned int cmd) {
    int32_t ret;
    int32_t tmp;
    uint8_t tran_edge_level;
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];
    bool result = false;

    TRAN_FUNC_ENTER();
    if (!is_support_feature(FEATURE_EDGE_AMT)) {
        return result;
    }

    switch(cmd){
        case IOCTL_SET_EDGE_LEVEL:
            ret = sscanf(pdata->data, "%d", &tmp);
            if (tmp != 0 && tmp != 2 && tmp != 3 && tmp != 4) {
                TRAN_ERROR("Invalid value! ret = %d, tran_edge_level_write val = %d", ret, tmp);
                return result;
            }
            tran_edge_level = tmp;
            TRAN_INFO("tran_edge_level = %d", tran_edge_level);

            g_tran_ts.g_tran_edge_level = tran_edge_level;
            if (ctr->controller_state != TRAN_ACTIVE_MODE) {
                TRAN_ERROR("TP already suspended do not set edge level");
            } else {
                ctr->ops->set_ts_mode(SET_EDGE_LEVER_MODE, tran_edge_level);
            }
            result = true;
            break;
        case IOCTL_GET_EDGE_LEVEL:
            pdata->length = sprintf(pdata->data, "%d\n", g_tran_ts.g_tran_edge_level) + 1;
            result = true;
            break;
        default:
            break;
    }
    TRAN_FUNC_EXIT();

    return result;
}

/**
 * function: ioctl_shutdown_flag_handle
 * description: deal with x x commands
 *
 * Boundary Resources:
 * - bus(spi or i2c)
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_shutdown_flag_handle(struct IoctlData *pdata, unsigned int cmd) {
    bool result = false;

    TRAN_FUNC_ENTER();
    /* not support */
    TRAN_FUNC_EXIT();

    return result;
}

/**
 * function: ioctl_mistouch_mode_handle
 * description: deal with x x commands
 *
 * Boundary Resources:
 * - bus(spi or i2c)
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_mistouch_mode_handle(struct IoctlData *pdata, unsigned int cmd) {
    bool result = false;

    TRAN_FUNC_ENTER();
    if (!is_support_feature(FEATURE_GAME_AMT)) {
        return result;
    }
    switch(cmd) {
        case IOCTL_SET_MISTOUCH_MODE:
            result = true;
            break;
        case IOCTL_GET_MISTOUCH_MODE:
            result = true;
            break;
        default:
            break;
    }
    TRAN_FUNC_EXIT();

    return result;
}

/**
 * function: ioctl_game_mode_handle
 * description: deal with distance lock point commands,esport1.0
 *
 * Boundary Resources:
 * - bus(spi or i2c)
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_game_mode_handle(struct IoctlData *pdata, unsigned int cmd) {
    int32_t ret;
    int32_t tmp;
    uint8_t tran_game_mode;
    u8 lock_point_distance = 0;
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];
    bool result = false;

    TRAN_FUNC_ENTER();
    if (!is_support_feature(FEATURE_ESPORT)) {
        return result;
    }

    if (ctr == NULL) {
        TRAN_ERROR("controller is null");
        return result;
    }
    switch(cmd) {
        case IOCTL_SET_GAME_MODE:
            ret = sscanf(pdata->data, "%d", &tmp);
            if (ret != 1) {
                TRAN_ERROR("Invalid value! ret = %d", ret);
                return result;
            }
            if (tmp != 0 && tmp != 1 && tmp != 2 && tmp != 3 && tmp != 4 && tmp != 5) {
                TRAN_ERROR("Invalid value! ret = %d, tran_report_rate val = %d", ret, tmp);
                return result;
            }

            tran_game_mode = tmp;
            TRAN_INFO("tran_report_rate = %d", tran_game_mode);

            g_tran_ts.g_tran_game_mode = tran_game_mode;
            if (TRAN_SUSPEND_MODE == ctr->controller_state) {
                TRAN_ERROR("TP already suspended do not set game_mode");
            } else {
                ctr->ops->set_ts_mode(LOCK_POINT_SWITCH, tran_game_mode);
            }
            result = true;
            break;
        case IOCTL_GET_GAME_MODE:
            if(ctr->ops->get_lock_point_distance) {
                ctr->ops->get_lock_point_distance(&lock_point_distance);
            }
            pdata->length = sprintf(pdata->data, "Gear position:%d\nLock point distance:%d\n", g_tran_ts.g_tran_game_mode, lock_point_distance) + 1;
            result = true;
            break;
        
        default:
            break;
    }
    TRAN_FUNC_EXIT();

    return result;
}

/**
 * function: ioctl_game_state_handle
 * description: deal with simple esport1.0
 *
 * Boundary Resources:
 * - bus(spi or i2c)
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_game_state_handle(struct IoctlData *pdata, unsigned int cmd) {
    int32_t ret;
    int32_t tmp;
    uint8_t g_tran_game_state = 0;
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];
    bool result = false;

    TRAN_FUNC_ENTER();
    if (ctr == NULL) {
        TRAN_ERROR("controller is null");
        return result;
    }
    switch(cmd){
        case IOCTL_SET_GAME_STATE:
            ret = sscanf(pdata->data, "%d", &tmp);
            if (ret != 1) {
                TRAN_ERROR("Invalid value! ret = %d", ret);
                return result;
            }

            if ( tmp > 1) {
                TRAN_ERROR("Invalid value! tmp = %d", tmp);
                return result;
            }

            g_tran_game_state = tmp;
            g_tran_ts.g_tran_game_state = g_tran_game_state;
            TRAN_INFO("tp_game_state_write val = %d", g_tran_game_state);

            ctr->ops->set_ts_mode(SET_GAME_MODE, g_tran_game_state);
            if(0 == g_tran_game_state){//report big data when exit game mode
                //g_tran_ts.platform_data->ops->bigdata_tp_lock_vel();
            }
            result = true;
            break;

        case IOCTL_GET_GAME_STATE:
            pdata->length = sprintf(pdata->data, "Game state:%d\n", g_tran_ts.g_tran_game_state) + 1;
            result = true;
            break;
        
        default:
            break;
    }
    TRAN_FUNC_EXIT();

    return result;
}

/**
 * function: ioctl_transient_rate_handle
 * description: deal with esports transient report rate
 *
 * Boundary Resources:
 * - bus(spi or i2c)
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_transient_rate_handle(struct IoctlData *pdata, unsigned int cmd) {
    int32_t ret;
    int32_t tmp;
    uint8_t tran_transient_report_rate;
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];
    bool result = false;

    TRAN_FUNC_ENTER();
    if (!is_support_feature(FEATURE_ESPORT)) {
        return result;
    }

    if (ctr == NULL) {
        TRAN_ERROR("controller is null");
        return result;
    }

    switch(cmd) {
        case IOCTL_SET_TRANSIENT_RATE:
            ret = sscanf(pdata->data, "%d", &tmp);
            if (ret != 1) {
                TRAN_ERROR("Invalid value! ret = %d", ret);
                return result;
            }
            if (tmp != 0 && tmp != 1) {
                TRAN_ERROR("Invalid value! ret = %d, tran_transient_report_rate val = %d", ret, tmp);
                return result; 
            }

            tran_transient_report_rate = tmp;
            TRAN_INFO("tran_transient_report_rate = %d", tran_transient_report_rate);

            g_tran_ts.g_tran_transient_report_rate = tran_transient_report_rate;
            if (TRAN_SUSPEND_MODE == ctr->controller_state) {
                TRAN_ERROR("TP already suspended do not tran_transient_report_rate");
            } else {
                ctr->ops->set_ts_mode(TRANSIENT_SAMPLING_RATE_SWITCH, tran_transient_report_rate);
            }
            result = true;
            break;

        case IOCTL_GET_TRANSIENT_RATE:
            pdata->length = sprintf(pdata->data, "%d\n", g_tran_ts.g_tran_transient_report_rate);
            result = true;
            break;

        default:
            break;
    }
    TRAN_FUNC_EXIT();
    return result;
}

/**
 * function: ioctl_x_x_handle
 * description: deal with x x commands
 *
 * Boundary Resources:
 * - bus(spi or i2c)
 * 
 * Change Logs:
 * - 2024-12-04: USD add for aidl interface by guoxin.zhu
 */
static bool ioctl_ime_state_handle(struct IoctlData *pdata, unsigned int cmd) {
    bool result = false;

    TRAN_FUNC_ENTER();
    switch(cmd){
        case IOCTL_SET_IME_STATE:
            if (g_tran_ts.controller[TRAN_MAIN_CONTROLLER] != NULL) {
            } else {
                TRAN_ERROR("controller is null");
            }
            result = true;
            break;
        
        default:
            break;
    }
    TRAN_FUNC_EXIT();

    return result;
}

#if 1
/**
 * function: ioctl_underwater_state_handle
 * description: deal with switch underwater enable status and get underwater status info
 *
 * Boundary Resources:
 * - spi
 *
 * Change Logs:
 * - 2025-09-10: USD add for aidl interface by qixuan.qin
 */
static bool ioctl_underwater_state_handle(struct IoctlData *pdata, unsigned int cmd) {
    bool result = false;
    int32_t tmp;
    int ret = 0;
    int status = 0;

    TRAN_FUNC_ENTER();
    switch(cmd) {
        case IOCTL_SET_UNDERWATER_STATE:
            ret = sscanf(pdata->data, "%d", &tmp);
            if (ret != 1) {
                TRAN_ERROR("Invalid value! ret = %d", ret);
                return result;
            }
            status = tmp;
            if(g_tran_ts.controller[TRAN_MAIN_CONTROLLER]) {
                if(g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->ops->set_underwater_status) {
                    g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->ops->set_underwater_status(status);
                    result = true;
                } else {
                    TRAN_ERROR("This IC does not support set_underwater_status!!!");
                }
            }
            break;
        case IOCTL_GET_UNDERWATER_STATE:
            if(g_tran_ts.controller[TRAN_MAIN_CONTROLLER]) {
                if(g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->ops->get_underwater_status) {
                    status = g_tran_ts.controller[TRAN_MAIN_CONTROLLER]->ops->get_underwater_status();
                    pdata->length = sprintf(pdata->data, "%d", status);
                    result = true;
                } else {
                    TRAN_ERROR("This IC does not support get_underwater_status!!!");
                }
            }
            break;
        default:
            break;
    }
    TRAN_FUNC_EXIT();

    return result;
}
#endif

struct ioctlHandler ioctl_handler[] = {
    {IOCTL_GET_VENDOR,              ioctl_vendor_cmd_handle},
    {IOCTL_GET_FW_VERSION,          ioctl_fw_version_handle},
    {IOCTL_SET_SELFTEST_START,      ioctl_selftest_handle},
    {IOCTL_GET_SELFTEST_STATUS,     ioctl_selftest_handle},
    {IOCTL_GET_GESTURE_STATUS,      ioctl_gesture_handle},
    {IOCTL_GET_GESTURE_CODE,        ioctl_gesture_handle},
    {IOCTL_SET_GESTURE_STATUS,      ioctl_gesture_handle},
    {IOCTL_SET_ROTAION_STATUS,      ioctl_display_rotation_handle},
    {IOCTL_GET_ROTAION_STATUS,      ioctl_display_rotation_handle},
    {IOCTL_SET_REPORT_RATE,         ioctl_report_rate_handle}, //not support
    {IOCTL_GET_REPORT_RATE,         ioctl_report_rate_handle}, //not support
    {IOCTL_SET_LARGE_MISTOUCH,      ioctl_large_mistouch_handle},
    {IOCTL_GET_LARGE_MISTOUCH,      ioctl_large_mistouch_handle},
    {IOCTL_SET_EDGE_LEVEL,          ioctl_edge_level_handle},
    {IOCTL_GET_EDGE_LEVEL,          ioctl_edge_level_handle},
    {IOCTL_SET_SHUTDOWN_FLAG,       ioctl_shutdown_flag_handle},//not support
    {IOCTL_GET_SHUTDOWN_FLAG,       ioctl_shutdown_flag_handle},//not support
    {IOCTL_SET_MISTOUCH_MODE,       ioctl_mistouch_mode_handle},//not support
    {IOCTL_GET_MISTOUCH_MODE,       ioctl_mistouch_mode_handle},//not support
    {IOCTL_SET_GAME_MODE,           ioctl_game_mode_handle},
    {IOCTL_GET_GAME_MODE,           ioctl_game_mode_handle},
    {IOCTL_SET_GAME_STATE,          ioctl_game_state_handle},
    {IOCTL_GET_GAME_STATE,          ioctl_game_state_handle},
    {IOCTL_SET_TRANSIENT_RATE,      ioctl_transient_rate_handle},
    {IOCTL_GET_TRANSIENT_RATE,      ioctl_transient_rate_handle},
    {IOCTL_SET_IME_STATE,           ioctl_ime_state_handle},//not support
/* USD:add for CNx underwater by qixuan.qin 20250910 start */
    {IOCTL_SET_UNDERWATER_STATE,    ioctl_underwater_state_handle},
    {IOCTL_GET_UNDERWATER_STATE,    ioctl_underwater_state_handle},
/* USD:add for CNx underwater by qixuan.qin 20250910 end */
};

static struct ioctlHandler *get_handler(unsigned int ioctlFlag)
{
    int i;
    for (i = 0; i < sizeof(ioctl_handler) / sizeof(struct ioctlHandler); i++) {
        if (ioctlFlag == ioctl_handler[i].ioctlFlag) {
            TRAN_INFO("match CMD success!");
            return &ioctl_handler[i];
        }
    }

    TRAN_ERROR("match CMD fail!");
    return NULL;
};
/*
 * ------------------------------
 * IOCTL Handler end
 * ------------------------------
 */

static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
    struct IoctlData data;
    struct ioctlHandler *pioctl;

    TRAN_FUNC_ENTER();

    TRAN_INFO("IOCTL CMD = 0x%x", cmd);
    switch(cmd) {
        // GET 命令
        case IOCTL_GET_VENDOR:
        case IOCTL_GET_FW_VERSION:
        case IOCTL_GET_SELFTEST_STATUS:
        case IOCTL_GET_GESTURE_STATUS:
        case IOCTL_GET_GESTURE_CODE:
        case IOCTL_GET_ROTAION_STATUS:
        case IOCTL_GET_REPORT_RATE:
        case IOCTL_GET_LARGE_MISTOUCH:
        case IOCTL_GET_EDGE_LEVEL:
        case IOCTL_GET_SHUTDOWN_FLAG:
        case IOCTL_GET_MISTOUCH_MODE:
        case IOCTL_GET_GAME_MODE:
        case IOCTL_GET_GAME_STATE:
        case IOCTL_GET_TRANSIENT_RATE:
        case IOCTL_GET_UNDERWATER_STATE:
            // 执行命令，获取信息
            pioctl  = get_handler(cmd);
            if (pioctl == NULL) {
                TRAN_ERROR("pioctl = Null");
                return -EINVAL;
            }

            if(!pioctl->ioctlHandle(&data, cmd)) {
                TRAN_ERROR("ioctlHandle null");
                return -EFAULT;
            }

            // 拷贝到用户空间
            if (copy_to_user((void __user *)arg, &data, sizeof(struct IoctlData)) != 0) {
                return -EFAULT;
            }
            break;

        // SET 命令
        case IOCTL_SET_SELFTEST_START:
        case IOCTL_SET_GESTURE_STATUS:
        case IOCTL_SET_ROTAION_STATUS:
        case IOCTL_SET_REPORT_RATE:
        case IOCTL_SET_LARGE_MISTOUCH:
        case IOCTL_SET_EDGE_LEVEL:
        case IOCTL_SET_SHUTDOWN_FLAG:
        case IOCTL_SET_MISTOUCH_MODE:
        case IOCTL_SET_GAME_MODE:
        case IOCTL_SET_GAME_STATE:
        case IOCTL_SET_TRANSIENT_RATE:
        case IOCTL_SET_IME_STATE:
        case IOCTL_SET_UNDERWATER_STATE:
            // 获取参数信息
            pioctl  = get_handler(cmd);
            if (pioctl == NULL) {
                return -EINVAL;
            }

            if (copy_from_user(&data, (void __user *)arg, sizeof(struct IoctlData)) != 0) {
                return -EFAULT;
            }

            // 执行命令
            if(!pioctl->ioctlHandle(&data, cmd)) {
                return -EFAULT;
            }
            break;

        default:
            return -ENOTTY;
    }
    TRAN_ERROR("no support cmd");//debug
    return 0;
}

static int dev_open(struct inode *inodep, struct file *filep) {
    TRAN_INFO("dev_open");
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    TRAN_INFO("dev_release");
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int error_count = 0;
    TRAN_INFO("dev_read in");
    if (!data_available) {
        TRAN_INFO("data is not available");
        return 0;
    }

    error_count = copy_to_user(buffer, message, message_size);

    if (error_count == 0) {
        TRAN_INFO("Sent size=%d characters to the user\n", message_size);
        data_available = false; // clear the flag
        return message_size;
    } else {
        TRAN_INFO("Failed to send %d characters to the user\n", error_count);
        return -EFAULT;
    }
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    size_t to_copy = min(len, sizeof(message) - 1);

    TRAN_INFO("dev_write in write len = %lu", len);
    if (copy_from_user(message, buffer, to_copy) != 0) {
        TRAN_INFO("copy failed  len = %lu", len);
        return -EFAULT;
    }
    message[to_copy] = '\0';
    message_size = to_copy;
    data_available = true;
    schedule_work(&app_write_work);
#ifdef ENABLE_CALL_BACK
    wake_up_interruptible(&poll_queue);
#endif
    return to_copy;
}

static unsigned int dev_poll(struct file *filep, poll_table *wait) {
    unsigned int mask = 0;

    TRAN_INFO("dev_poll in");
    poll_wait(filep, &poll_queue, wait);
    if (data_available) {
        mask |= POLLIN | POLLRDNORM; // Readable
    }

    return mask;
}

static struct file_operations aidl_interface_fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
    .poll = dev_poll,
    .unlocked_ioctl = dev_ioctl,
};

/*
 * ------------------------------
 * Initialization Function
 * ------------------------------
 */
int tp_poll_drv_init(void) {
    TRAN_INFO("tp_poll_drv_init in");
    major_number = register_chrdev(0, POLL_DEVICE_NAME, &aidl_interface_fops);
    if (major_number < 0) {
        TRAN_INFO("Failed to register a major number\n");
        return major_number;
    }

    poll_class = class_create(POLL_CLASS_NAME);
    if (IS_ERR(poll_class)) {
        unregister_chrdev(major_number, POLL_DEVICE_NAME);
        TRAN_INFO("Failed to register device class\n");
        return PTR_ERR(poll_class);
    }

    poll_device = device_create(poll_class, NULL, MKDEV(major_number, 0), NULL, POLL_DEVICE_NAME);
    if (IS_ERR(poll_device)) {
        class_destroy(poll_class);
        unregister_chrdev(major_number, POLL_DEVICE_NAME);
        TRAN_INFO("Failed to create the device\n");
        return PTR_ERR(poll_device);
    }

    cdev_init(&poll_cdev, &aidl_interface_fops);
    if (cdev_add(&poll_cdev, MKDEV(major_number, 0), 1) < 0) {
        device_destroy(poll_class, MKDEV(major_number, 0));
        class_unregister(poll_class);
        class_destroy(poll_class);
        unregister_chrdev(major_number, POLL_DEVICE_NAME);
        TRAN_INFO("Failed to add cdev\n");
        return -1;
    }

    ioctl_workqueue = create_singlethread_workqueue("ioctl_workqueue");
    if (!ioctl_workqueue) {
        // 处理错误
        TRAN_ERROR("Failed to create ioctl_workqueue");
    }
    init_waitqueue_head(&poll_queue);
    INIT_WORK(&app_write_work, app_write_call);
    INIT_WORK(&tp_selftest_work, tp_self_test);

    TRAN_INFO("Poll driver initialized with device: /dev/%s\n", POLL_DEVICE_NAME);
    return 0;
}
