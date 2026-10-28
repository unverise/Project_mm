#ifndef TRANSCEIVER_FEATURES_H
#define TRANSCEIVER_FEATURES_H

#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/leds-mtk.h>
#include <drm/drm_modes.h>
#include "hf_sensor_type.h"
//modify for trancare SR-202507-002286 by hongliang.zhang 20250818 start
#include <linux/types.h>
#include <linux/notifier.h>
#include <linux/module.h>
#include <linux/string.h>
#include "tranlog.h"
extern struct athena_msg* alloc_msg(void);
extern void free_msg(struct athena_msg **athena);
extern void submit_msg(struct athena_msg **athena, int version, long long tid, char *tag);
extern void msg_put_string(struct athena_msg **athena, char *key, char *value);
extern void msg_put_int(struct athena_msg **athena, char *key, int value);
//modify for trancare SR-202507-002286 by hongliang.zhang 20250818 end
//modify for SR-202507-002286 by hongliang.zhang 20250818 start
#include "share_memory.h"
#include "hf_sensor_io.h"
#include <linux/mutex.h>
#define TRAN_HW_SENSOR_TYPE_MAX 6
//modify for SR-202507-002286 by hongliang.zhang 20250818 end
//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 start
#include <linux/of.h>
#include <linux/of_fdt.h>
#include "mtk_disp_notify.h"
//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 end
#define CUST_LCM_INFO__ELEM_CNT 4

struct tran_sensor_cust_lcm_info {
    u32 support_state;
    u32 offset_x;
    u32 offset_y;
    u32 clip_w;
    u32 clip_h;
    u8 r_avg;
    u8 g_avg;
    u8 b_avg;
    u8 lcm_id;
    u8 cwb_state;
    u32 vendor;
    u32 algo;
    u32 ver;
    u8 enable_state;
    u32 Lsensor_x;
    u32 Lsensor_y;
    u32 Lsensor_w;
    u32 Lsensor_h;
    u32 ratio_in;
    u32 ratio_out;
};

struct tran_sensor_cwb_info {
	uint32_t r_avg;
	uint32_t g_avg;
	uint32_t b_avg;
};

//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 start
struct tran_sensor_cust_param  {
    int32_t data_type;
    int8_t data[8];
} __packed __aligned(4);

enum Tran_Sensor_Data_Type {
    TRAN_LCM_POWERONOFF_INFO = 0,
    TRAN_LCM_SUPPLIER_INFO = 1,
    TRAN_LCM_INFO_MAX,
};

enum Tran_Lcm_Info {
    TRAN_SUPPLIER_LCM_0 = 0,
    TRAN_SUPPLIER_LCM_1 = 1,
    TRAN_SUPPLIER_LCM_2 = 2,
    TRAN_SUPPLIER_LCM_3 = 3,
    TRAN_SUPPLIER_LCM_MAX,
};
//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 end

enum Tran_Under_Screen_Sensor {
    TRAN_ML = 0,        //SENSOR_TYPE_LIGHT
    TRAN_MSL = 1,       //SENSOR_TYPE_VICE_LIGHT
    TRAN_SL = 2,        //SENSOR_TYPE_PADALS
    TRAN_SSL = 3,       //SENSOR_TYPE_VICE_PADALS
    TRAN_UNDER_SENSOR_MAX,
};

enum Tran_ml_nv_info {
    VENDORTYPE = 0,
    ALGOTYPE = 1,
    VERSION = 2,
    TRAN_ML_NV_INFO_MAX,
};

enum VENDORTYPE {
    SensorTek = 1,
    VENDORTYPE_MAX,
};

enum ALGOTYPE {
    CutOut = 1,
    ALGOTYPE_MAX,
};

enum VERSION {
    V1_0_0 = 1,
    VERSION_MAX,
};

enum Tran_CWB_UPDATE_STATE {
    CWB_INFO_ERROR = 0,         //cwb info error
    CWB_INFO_NOT_UPDATE = 1,    //cwb info do not update
    CWB_INFO_UPDATE = 2,        //cwb info update
};

#include <transsion_cwb.h>
extern void transsion_cwb_init(void);
extern int transsion_cwb_enable(bool en, u8 lcm_id);
extern int transsion_cwb_registor(u8 lcm_id, u8 sensor_id, unsigned int  offset_x, unsigned int offset_y, unsigned int clip_w, unsigned int clip_h, unsigned int  Lsensor_x, unsigned int  Lsensor_y, unsigned int  Lsensor_w, unsigned int  Lsensor_h, unsigned int  ratio_in, unsigned int  ratio_out);
extern void transsion_get_cwb_rgb_info(Transsion_CWB_Sensor_Info *rgb_buf);
//USD: add LK7KOS16-2 by yi.peng for HWC 20251010 start
extern void transceiver_transfer_hwc_cwb_data(int sensor_type, uint8_t *rgb_data);
//USD: add LK7KOS16-2 by yi.peng for HWC 20251010 end
#define TRAN_ERROR(fmt, args...) do { \
    printk(KERN_ERR "[tran_sensor_debug]%s %d:"fmt"\n", __func__, __LINE__, ##args); \
} while (0)

void tran_sensor_cwb_init(void);
int tran_sensor_cwb_enable(uint8_t sensor_type, bool en);
void tran_sensor_cwb_register(void);
void tran_sensor_cwb_rgb_get(void);

typedef struct {
	u8 sensor_id;
	u8 r_avg;
	u8 g_avg;
	u8 b_avg;
} Transsion_CWB_Sensor_Info_MOCK;

#include <linux/notifier.h>

extern struct tran_sensor_cust_lcm_info cust_sensor_info[TRAN_UNDER_SENSOR_MAX];
extern struct tran_sensor_cwb_info als_cwb_info[TRAN_UNDER_SENSOR_MAX];
extern bool tran_sensor_un_als_support;
extern bool tran_sensor_ml_nv_state_get;
extern bool tran_sensor_sl_nv_state_get;
extern int32_t g_tran_debug_mode;
extern uint32_t nv_restore_times;
extern uint64_t tran_als_nv_time;
extern uint64_t tran_padals_nv_time;
extern uint64_t tran_ps_nv_time;
extern struct sensor_param tran_lcm_info;

#define ALS_NV_TRANSFER_MAX_BYTE 9*4
#define PS_NV_TRANSFER_MAX_BYTE 2*4
#define PS_NV_TRANSFER_TIME 2
#include <linux/fortify-string.h>
struct tran_als_nvcfg {
    uint8_t length;
    struct_group(data,
        uint8_t field[];
    ) __aligned(4);
};

struct tran_ps_nvcfg {
    uint8_t length;
    struct_group(data,
        uint8_t field[];
    ) __aligned(4);
};

extern struct tran_als_nvcfg *nvcfg;
extern struct tran_ps_nvcfg *ps_nvcfg;
#include <linux/power_supply.h>
#define CHARGER_POLLING_DELAY 200
extern uint32_t tran_charger_val;
extern uint32_t tran_last_charger_val;
extern int tran_charger_sensor_type;
//int tran_send_charger_to_hub(void *data, uint8_t length);

#define ACCDET_NOTIFIER_IN 0x98
#define ACCDET_NOTIFIER_OUT 0x99

extern int g_tran_brightness_sensor_type;

#include <drm/drm_modes.h>
#define VREFRESH_POLLING_DELAY 20
extern int g_tran_get_vrefresh;
extern int g_tran_get_vrefresh_main;
static int g_tran_last_get_vrefresh = 0;
static int g_tran_last_get_vrefresh_main = 0;
//int g_tran_vrefresh_sensor_type = 0;
#define HALL_POLLING_DELAY 90
extern int hall_get_state(void);
static int g_tran_last_hall_level = -1;
static int g_tran_hall_level = -1;
extern uint8_t tran_cwb_flip_lightals_enable;
extern uint8_t tran_cwb_flip_subals_enable;
static uint8_t g_tran_orientation_enable = false;

//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 start
enum {
    TRAN_INTERNAL_SCREEN_POWER_ON,//Internal screen power on
    TRAN_EXTERNAL_SCREEN_POWER_ON,//External screen power on
    TRAN_INTERNAL_SCREEN_POWER_OFF,//Internal screen power off
    TRAN_EXTERNAL_SCREEN_POWER_OFF,//External screen power off
};

extern char tran_supplier_lcm0[64];
extern char tran_supplier_lcm1[64];
extern char tran_supplier_lcm2[64];
extern char tran_supplier_lcm3[64];
extern int tran_supplier_lcm_num;
//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 end

extern int tran_inter_display_st;
extern int tran_exter_display_st;
extern int tran_display_st;
extern int kernel_hinge;


// 定义存储设备树属性的结构体
struct tran_sensor_properties {
    u32 screen_power_state;
    u32 charger_poll;
    u32 als_support;
    u32 brightness_level_support;
    u32 vrefresh_support;
    u32 hall_sensor;
    u32 cloud_tracking_sensor_data;
    u32 sar_debug;
    u32 sensor_debug;
    u32 cwb_support;
    u32 tnek_support;
    u32 disp_notif;
    u32 lcm_supplier_info;
};

// 声明全局变量
extern struct tran_sensor_properties sensor_props;

// ---------------- CONFIG_TRAN_GET_BRIGHTNESS_LEVEL_SUPPORT ----------------

extern uint64_t tran_get_lcm_info_timer;
extern int g_tran_brightness_level;


// ---------------- CONFIG_TRAN_CWB_SUPPORT ----------------

#define CWB_POLLING_DELAY 20
extern int sensor_location;
extern struct tran_cwb_ctrl cwb_ctrl;


// ---------------- CONFIG_TRAN_GET_VREFRESH_SUPPORT ----------------

#define VREFRESH_POLLING_DELAY 20
extern int g_tran_get_vrefresh;
extern int g_tran_last_get_vrefresh;
extern int g_tran_get_vrefresh_main;
extern int g_tran_last_get_vrefresh_main;

#define HALL_POLLING_DELAY 90
extern int g_tran_last_hall_level;
extern int g_tran_hall_level;

extern struct sensor_param tran_lcm_info;

struct transceiver_feature {
    struct workqueue_struct *charger_wq;
    struct work_struct work_charger;
    struct hrtimer charger_timer;
    ktime_t charger_poll_delay;
    struct workqueue_struct *brightness_wq;
    struct work_struct work_brightness;
    struct hrtimer brightness_timer;
    ktime_t brightness_poll_delay;
    struct workqueue_struct *vrefresh_wq;
    struct work_struct work_vrefresh;
    struct hrtimer vrefresh_timer;
    ktime_t vrefresh_poll_delay;
    struct workqueue_struct *hall_wq;
    struct work_struct work_hall;
    struct hrtimer hall_timer;
    ktime_t hall_poll_delay;
    struct workqueue_struct *cwb_wq;
    struct work_struct work_cwb;
    struct hrtimer cwb_timer;
    ktime_t cwb_poll_delay;
//USD:modify by hongliang.zhang for [sensorhub tne] 20250520 start
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
    struct delayed_work tran_sensor_aee_delayed_work;
#endif
//USD:modify by hongliang.zhang for [sensorhub tne] 20250520 end
//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 start
    struct notifier_block sensor_disp_notif;
    struct workqueue_struct *disp_wq;
    struct work_struct work_disp;
//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 end
};


extern struct transceiver_feature *feature_dev;
int transceiver_features_init(void);
void transceiver_features_exit(void);
void transceiver_alloc_nv_configs(void);
void transceiver_handle_sensor_config(int sensor_type, void *data);
void transceiver_handle_cwb_disable(int sensor_type, int en);
struct transceiver_feature *get_transceiver_feature(void);
void transceiver_handle_als_enable(struct transceiver_feature *feature, int sensor_type, int en);
void transceiver_handle_als_disable(struct transceiver_feature *feature, int sensor_type, int en);
void transceiver_handle_sensor_enable(struct transceiver_feature *feature, int sensor_type, int en);
int transceiver_restore_config(uint8_t sensor_type, void *config_data, size_t config_length);
int als_set_brightness(void *data, uint32_t count);
extern int __init tran_sensor_init(void);

/*
int transsion_cwb_registor(u8 lcm_id, u8 sensor_id, unsigned int offset_x, unsigned int offset_y, unsigned int clip_w, unsigned int clip_h,
  							unsigned int Lsensor_x, unsigned int Lsensor_y, unsigned int Lsensor_w, unsigned int Lsensor_h,
  							unsigned int ratio_in, unsigned int ratio_out);


int transsion_cwb_enable(bool en, u8 lcm_id);
void transsion_cwb_init(void);
void transsion_get_cwb_rgb_info(Transsion_CWB_Sensor_Info *rgb_buf);
*/
typedef int (*transceiver_comm_with_t)(int sensor_type, int cmd, void *data, uint8_t length);
extern transceiver_comm_with_t transceiver_comm_with_ptr;
void init_transceiver_comm_with(void);

//modify for SR-202507-002286 by hongliang.zhang 20250818 start
struct tran_sensor_type_info {
    u8 acc;
    u8 mag;
    u8 gyro;
    u8 als;
    u8 ps;
    u8 sar;
};

enum dev_hw_state_enable {
    ENABLE_SUCCESS = 1,
    ENABLE_FAIL,
    MAX_ENABLE_STATUS,
};

enum dev_hw_state_cali {
    CALI_SUCCESS = 1,
    CALI_FAIL,
    MAX_CALI_STATUS,
};

enum status {
    TRAN_SENSOR_OFF = 0,
    TRAN_SENSOR_ON,
};

typedef enum {
    SENSOR_STATE_NOT_SUPPORTED = 0,
    SENSOR_STATE_LOAD_FAILED,
    SENSOR_STATE_LOAD_SUCCESS
} sensor_state_t;

typedef struct {
    uint8_t expected_type;
    sensor_state_t state;
    const char *name;
} sensor_status_t;

struct last_arr_data {
    int32_t acc[3];
    int32_t acc_cali_flag;
    int32_t gyro_cali_flag;
    int32_t mag_accuracy;
    int32_t light_k;
    int32_t light_cali_flag;
    int32_t prox_noise;
    int32_t prox_cali_flag;
};

extern struct tran_sensor_type_info dts_types;
void check_sensor_status(const struct tran_sensor_type_info *dts_types, struct sensor_info *loaded_types, int loaded_count);
void sensor_translate_arr_log(const struct share_mem_data *src);
//modify for SR-202507-002286 by hongliang.zhang 20250818 start

#endif // TRANSCEIVER_FEATURES_H
