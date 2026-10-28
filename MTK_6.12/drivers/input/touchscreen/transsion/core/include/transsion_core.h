#ifndef __TRANSSON_CORE_H__
#define __TRANSSON_CORE_H__

#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/irq.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/pm_wakeup.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>


#define TP_DRV_MAX_COUNT     10
#define MAX_LCM_NAME_LEN     64

#define TPD_DEVICE                "mtk-tpd2"
#define TPD_SUB_DEVICE          "mtk-tpd"

#define ENUM_TO_STRING(enumVar) #enumVar
/*
 * DEBUG
*/
#define TRAN_DEBUG(fmt, args...) do { \
    printk("[tran_touch]%s %d:"fmt"\n", __func__, __LINE__, ##args); \
} while (0)

#define TRAN_INFO(fmt, args...) do { \
    printk("[tran_touch-INFO]%s %d:"fmt"\n", __func__, __LINE__, ##args); \
} while (0)

#define TRAN_ERROR(fmt, args...) do { \
    printk(KERN_ERR "[tran_touch-ERROR]%s %d:"fmt"\n", __func__, __LINE__, ##args); \
} while (0)

#define TRAN_FUNC_ENTER() do { \
    printk("[tran_touch]++++++++++%s: In(%d)++++++++++\n", __func__, __LINE__); \
} while (0)

#define TRAN_FUNC_EXIT() do { \
    printk("[tran_touch]----------%s: Exit(%d)----------\n", __func__, __LINE__); \
} while (0)


#define CTP_DEBUG_ON   0

#define CTP_INFO(fmt,arg...)    printk("<<-CTP-INFO->> "fmt"\n",##arg)
#define CTP_ERROR(fmt,arg...)    printk("<<-CTP-ERROR->> "fmt"\n",##arg)
#define CTP_DEBUG(fmt,arg...) do{ \
    if(CTP_DEBUG_ON)\
    printk("<<-CTP-DEBUG->> [%d]"fmt"\n",__LINE__, ##arg);\
}while(0)

/*
 * extern var & function
*/
extern struct tran_ts_core g_tran_ts;

/*
 * function support
*/
#define TRAN_PSENSOR_FUNCTION (1)
#define TRAN_GESTURE_FUNCTION (1 << 1)

/*TNE log control */
#define TRAN_TNE_LOG_EN     1

#define PSENSOR_IS_SUPPORT ((g_tran_ts.function_support & TRAN_PSENSOR_FUNCTION)?1:0)
#define GESTURE_IS_SUPPORT ((g_tran_ts.function_support & TRAN_GESTURE_FUNCTION)?1:0)

#define KEY_DETECT_PEN    199
#define KEY_FINGER        195
/*
* TRAN_NORMAL_MODE : Allow to load controllers
* TRAN_NOT_RUN_MODE: Not allowed to load controllers
* TRAN_FACTOAR_MODE: Allow to load controllers, flash ic don't update tp firmware
*/
enum TRAN_BOOT_MODE
{
    TRAN_NORMAL_MODE = 0,
    TRAN_NOT_RUN_MODE = 1,
    TRAN_FACTORY_MODE = 2,
};

/* This function will affect suspend or resume */
enum TS_FUNC_MODE
{
    FUNC_MODE_NONE = 0,
    FUNC_MODE_PS = 1,
    FUNC_MODE_GESTURE = 2,
};

enum TS_CONTROLLER_MODE
{
    TRAN_ACTIVE_MODE = 10,
    TRAN_SUSPEND_MODE = 11,
    TRAN_PS_SUSPEND_MODE = 12,
    TRAN_GESTURE_SUSPEND_MODE = 13,

    TRAN_MAX_MODE
};

/* how manly functions the controller supports */
enum TS_FUNC_CMD
{
    SET_NOISE_MODE = 100,
    SET_PS_MODE = 101,
    SET_GAME_MODE = 102,
    SET_EDGE_REJECT_MODE = 103,
    LOCK_POINT_SWITCH = 104,
    REPORT_RATE_SWITCH = 105,
    TRANSIENT_SAMPLING_RATE_SWITCH = 106,
    SET_EDGE_LEVER_MODE = 107,
    SET_CUSTOM_MISTOUCH_MODE = 108,
    LCD_AOD_MODE = 109,
    SET_POCKET_MODE = 110,
    LCD_AOD_MODE_EARLY =111,
    MICRO_CONTROL_ACCURACY =112,
    GET_KEYPAD_STATUS =113,
    GET_SCENE_STATUS =114,

    MAX_SET_MODE
};

/* how manly cmds the core provide to external mode */
enum TS_EXT_CMD
{
    /* set cmd */
    SET_TS_REST = 1001,
    SET_TS_DDI_RESUME = 1002,
    /* get cmd */
    GET_TS_MODE = 1003,
    SET_TS_IRQ_EN = 1004,
    CALL_TP_FWLOAD = 1005,
    MAX_TS_CMD
};

/* 0:off 1:on 2:on later */
enum TRAN_PS_MODE
{
    TRAN_PS_OFF = 2000,
    TRAN_PS_ON = 2001,
    TRAN_PS_ON_LATER = 2002,

    TRAN_PS_NEAR = 2003,
    TRAN_PS_FAR = 2004,

    MAX_PS_MODE
};

enum TRAN_TRIGER_EVENT
{
    RESUME_TRIGER = 0,
    SUSPEND_TRIGER = 1,
};

enum TRAN_CONTROLLER_TYPE
{
    TRAN_MAIN_CONTROLLER = 0,
    TRAN_SUB_CONTROLLER = 1,
    TRAN_MAX_CONTROLLER_NUM = 2
};

enum feature_type {
    FEATURE_ESPORT,
    FEATURE_FOD,
    FEATURE_GAME_AMT,
    FEATURE_POCKET_AMT,
    FEATURE_EDGE_AMT,
    FEATURE_DYNAMIC_REPORT_RATE,
    FEATURE_TP_SCENE,
    FEATURE_BASE_FUNCTION,
};

struct supplier_list {
    int supplier_num;
    int controller_type; /* 0:main controller 1:sub controller */
    u32 resolution_multiples; /* Multiples of resolution */
    u32 tx_num; /* tx num */
    u32 rx_num; /* rx num */
    char *supplier_info; /* <lcm name>,<vendor name>,<ic name> */
    char *supplier_vendor; /* Module factory such as TM TXD*/
    char *supplier_fw;
    char *supplier_testfw;
    bool is_matched;

    struct list_head next;
};

/*
 * drv layer
*/
struct vendor_gesture_map {
    unsigned char vendor_code; /* vendor's gesture id */
    unsigned char tran_code; /* transsion's gesture id */
};

struct tran_controller_ops {
    int (*tran_ts_init)(void);    /* TP drv init entry */
    void (*tran_ts_suspend)(struct device *h, int opt);    /* tp suspend api */
    void (*tran_ts_resume)(struct device *h, int opt);    /* tp resume api */

    /*
     * set tp function mode:
     * 1.high noise mode
     * 2.game mode
     * 3.ps mode
     * 4.edge reject mode
     */
    void (*set_ts_mode)(u32 cmd, u32 state);

    /* get tp firmware info */
    unsigned char (*get_firmware_version)(void);    /* current firmware which run in ic */
    unsigned char (*get_base_version)(void);    /*  firmware version which in filesystem */
    //BSP: add KJ7H895-2 for TP fw info by hongwu.zhan 20230821 start
    unsigned char (*get_config_version)(void);  /* goodix cofig.bin version */
    //BSP: add KJ7H895-2 for TP fw info by hongwu.zhan 20230821 end
    char *(*get_algo_version)(void);    /* current algo version which run in ic */
    char *(*get_algo_func)(void);

    /* self test */
    int (*tran_selftest)(void);
    int (*get_selftest_data)(char * test_result);
    int (*tran_selftest_open)(struct inode *inode, struct file *file);
    /*get lock point distance*/
    int (*get_lock_point_distance)(u8 *data);

    /* vendor */
    int (*tran_vendor_gesture_enable)(int);
    /* USD:add for CN5c underwater by qixuan.qin 20250910 start */
    void (*set_underwater_status)(int enable);
    int (*get_underwater_status)(void);
    /* USD:add for CN5c underwater by qixuan.qin 20250910 end */
};

struct tran_controller_ext_ops {
    void (*tran_tp_reset)(int val); /* tp reset */
    void (*resume_by_ddi)(void); /* tddi resume */
    void (*irq_enable_en)(uint8_t val); /* tp irq onoff */
    void (*call_tp_fwload)(void); /* call tp load fw */
};

struct tran_ts_controller {
    char *cur_ic; /* matched ic name */
    char *vendor_name; /* vendor name */
    struct completion comp;
    bool controller_init_state; /* controller init state */
    bool gesture_flag;
    u32 gesture_write_flag;
    u32 controller_state; /* no suspend. suspend. gesture_suspend. ps_suspend*/

    struct mutex dsp_event_mutex;

    struct vendor_gesture_map *gesture_map; /* gesture id map */
    char *selftest_data; /* self test result */
    char **support_ic;     /* may be support nums ic  */
    struct input_dev *input_dev; /*  which regist by drv */
    struct tran_controller_ops *ops;
    struct tran_controller_ext_ops *ext_ops;

    struct tran_ts_core *tran_ts_data; /* transsion core data */

    struct supplier_list *supplier;
};

/*
 * platform layer
*/
struct tran_platform_ops {
    int (*plat_init)(struct tran_ts_core *pdata);
    int (*plat_deinit)(struct tran_ts_core *pdata);
    int (*get_lcm_name)(char * p_name);
    int (*get_boot_mode)(void);
    int (*sub_plat_init)(struct tran_ts_core *pdata); /* midtest & dsp notify event */
    int (*sub_plat_deinit)(struct tran_ts_core *pdata);
    int (*bigdata_tp_mistouch)(void);
};

struct tran_ts_platform {
    int init_flag;
    const char *selftest_node_name; /* self test node name for midtest */
    struct tran_platform_ops *ops;
    struct tran_ts_core *tran_ts_data; /* transsion core data */
};


/*
 * core layer
*/
struct tran_core_ops {
    /* gesture */
    int (*gesture_state_get)(struct tran_ts_controller* ctr); /* gesture function state check: 0 off 1 on */
    int (*gesture_id_report)(const int gesture_id, struct tran_ts_controller* ctr);
    //BSP: add goodix for TP bringup by hai.tang5 20260302 start
    int (*gesture_switch_status)(int value);
    //BSP: add goodix for TP bringup by hai.tang5 20260302 end
    /* ps */
    int (*ps_state_get)(u32 *value); /* ps state check return 0 off 1 on 2 will be open by resume */
    int (*ps_state_report)(u32 near_far);
    /* sync notify */
    void (*controller_init_completion)(struct tran_ts_controller *ctr); /* call tp core controller init finished */
    void (*controller_uninit_completion)(struct tran_ts_controller *ctr);
    //USD: add display kick idle by jiangzhu.yang start
    void (*tran_call_disp_kick_idle)(struct tran_ts_controller* ctr); /* display kick idle */
    //USD: add display kick idle by jiangzhu.yang end
    //USD: add tne log by jiangzhu.yang start
    #if TRAN_TNE_LOG_EN
    #if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
    #if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
    void (*tran_call_tp_module_tnelog)(char *str); /* display kick idle */
    #endif
    #endif
    #endif
    //USD: add tne log by jiangzhu.yang end
};

struct tp_ps_status {
    u32 psSwitch; /* 0:off 1:on 2:on later */
    u32 psState; /* 0:near 1:far */
    u32 lastPsState; /* 0:near 1:far */
};
//BSP: add for custom mistouch by linsheng.liu 20231019 start
struct tp_mistouch_status {
    u32 LeftStartX_H;
    u32 LeftStartX_L;
    u32 LeftStartY_H;
    u32 LeftStartY_L;
    u32 RightStartX_H;
    u32 RightStartX_L;
    u32 RightStartY_H;
    u32 RightStartY_L;
};
//BSP: add for custom mistouch by linsheng.liu 20231019 end
//BSP: add KJ7H895-2 for TP bringup by hongwu.zhan 20230814 start
struct tp_fp_status {
    int g_fod_flag;
};
//BSP: add KJ7H895-2 for TP bringup by hongwu.zhan 20230814 end

struct touchscreen_feature
{
    /* double panel */
    bool fold_touch;
    /* Fingerprint */
	bool fod;

	/* rate switch */
	bool dynamic_report_rate;

    /* Anti-MisTouch */
    bool game_amt;
    bool curved_edge_amt;
    bool pocket_amt;

    /* tp scene */
    bool tp_scene;

    /* game */
	u32 esport;
};


struct tran_ts_core {

    /* core status */
    int switch_mode; /* dispaly rotation state */
    unsigned char g_tran_edge_level; /* edge level */
    unsigned char g_tran_tp_scene;
    int boot_mode;
    bool charger_in;
    bool is_ime_state;
    bool fixture_test_mode;
    uint8_t g_tran_tne_test_enable;

     /*big data*/
    unsigned int bigdata_tt;
    unsigned int bigdata_mt;

    struct tp_ps_status ps_state;
    struct tp_mistouch_status mistouch[5];
    //BSP: add KJ7H895-2 for TP bringup by hongwu.zhan 20230814 start
    struct tp_fp_status fp_status;
    //BSP: add KJ7H895-2 for TP bringup by hongwu.zhan 20230814 end
    u32 ts_core_mode; /* no suspend. suspend. gesture_suspend. ps_suspend*/
    /* tran common function */
    uint8_t g_tran_game_state;            /* game state                */
    uint8_t g_tran_transient_report_rate; /* transient sampling rate   */
    uint8_t g_tran_report_rate;           /* point report rate         */
    uint8_t g_tran_game_mode;             /* lock point                */
    //USD: add for tp feature by jiafeng.fan 20250905 start
    uint8_t g_tran_micro_control_accuracy;    /* micro control accuracy    */
    //USD: add for tp feature by jiafeng.fan 20250905 end
    uint8_t tran_pocket_state;            /* tran pocket state         */

    /* function support */
    struct touchscreen_feature features;

    struct platform_device *pdev;
    struct platform_device *sub_pdev;

    /* mutex */
    struct mutex core_data_mutex;
    struct mutex dsp_event_mutex;
    /* workqueue */
    struct workqueue_struct *tpd_init_workqueue;
    struct workqueue_struct *tpd_workqueue;
    /* work */
    struct work_struct tpd_init_work; /* core init work */
    struct work_struct tpd_dsp_work; /* suspend&resume work */
    struct work_struct tpd_sub_dsp_work; /* suspend&resume work */
    struct work_struct tpd_usb_work; /* charge in out work */
    /* notifier */
    struct notifier_block dsp_event_block;
    struct notifier_block sub_dsp_event_block;
    struct notifier_block usb_event_block;

    struct list_head supplier_head;
    struct list_head controller_head;

    struct tran_core_ops *tpd_ops;

    struct tran_ts_controller *controller[TRAN_MAX_CONTROLLER_NUM];
    struct tran_ts_platform *platform_data;
};
//USD: add display kick idle by jiangzhu.yang start
void tran_disp_kick_ilde(struct tran_ts_controller *ctr);
//USD: add display kick idle by jiangzhu.yang end
//USD: add tne log by jiangzhu.yang start
#if TRAN_TNE_LOG_EN
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
void tran_tp_module_tnelog(char *str);
#endif
#endif
#endif
//USD: add tne log by jiangzhu.yang end

/* externl function */
extern void init_tran_platform_info(void);
extern int state_change_machine(u32 triger, struct tran_ts_controller *pController);
extern int is_support_feature(enum feature_type type);
//USD: add display kick idle by jiangzhu.yang start
//extern void tran_disp_touch_kickidle(void);
//USD: add display kick idle by jiangzhu.yang end
#endif
