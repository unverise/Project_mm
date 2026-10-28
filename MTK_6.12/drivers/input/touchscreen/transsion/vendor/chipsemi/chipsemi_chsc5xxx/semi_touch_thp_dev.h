/**
********************************************************************************
* Copyright (C) 2021-2031,   All Rights Reserved.
*
* @file    : semi_touch_thp_dev.h
*
* @brief   :
*
* @author  : Touch Team
*
* @version : 0.1
*
* @data    : 2024-08-02
*
* @par     :
*
* History  :
*
*  1.  Date: 2024-08-02
*
*    Author:
*
*    Modification:
*
*  2. ...
*
********************************************************************************
*/

#ifndef __THP_DEBUG_DEV_H__
#define __THP_DEBUG_DEV_H__


/**
********************************************************************************
*.Included files
********************************************************************************
*/
#include <linux/miscdevice.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/delay.h>


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include<linux/device.h>


#if SEMI_TOUCH_THP_DRIVER_EN

/**
********************************************************************************
* Global constant and macro definitions using #define
********************************************************************************
*/
#ifdef ATK_3568_EN
#define TEST_BOARD_EN   1
#else
#define TEST_BOARD_EN   0
#endif

#define PROJECT_NAME_CN5  3
#define PROJECT_NAME   PROJECT_NAME_CN5
#define  FRAME_RX                       40
#define  FRAME_TX                       18

/**
********************************************************************************
*
********************************************************************************
*/
#define RAW_SIZE                 (PAGE_SIZE * 30) // PAGE_SIZE 4096
#define DRIVE_BUF_NUM            14 // < 15   max =RAW_SIZE/PAGE_SIZE/2

/**
********************************************************************************
*
********************************************************************************
*/
#if TEST_BOARD_EN == 1
#undef  SEMI_TOUCH_FW_UPDATE_EN
#define SEMI_TOUCH_FW_UPDATE_EN  0

#undef  SEMI_TOUCH_DMA_TRANSFER
#define SEMI_TOUCH_DMA_TRANSFER  0

#define FORCE_UPDATE_EN
#endif


#ifdef  FORCE_UPDATE_EN
#define FORCE_UPDATE_RUN_RAM_CODE() {\
    if (ret != 0)\
    {\
        ret = semi_touch_run_ram_code(RAM_CODE_BURN_MCAP_SHARE);\
    }\
}
#else
#define FORCE_UPDATE_RUN_RAM_CODE()
#endif


/**
********************************************************************************
* THP_EN    0 reports coordinates
            1 thp raw data packet,
            2 Automatic recognition, default reading of firmware coordinate
            report points, if THP algorithm is loaded. Change to THP
            reporting point
********************************************************************************
*/
#define THP_EN                     2
#define FILE_SET_SPI_SPEED_EN      0
#define BIN_CMD_FORCE_UPDATE_EN    1
#define MCAP_THP_FRAM_HEAD        64

#define SPI_MAX_SPEED_HZ    semi_touch_file_set_spi_speed(SPI_DEFAULT_SPEED,&st_dev)

/**
********************************************************************************
*
********************************************************************************
*/
#define ____study                /*...study         For marking */
#define ____need_improve         /*...need_improve  For marking*/
#define ____need_improve_begin   /*...need_improve  For marking*/
#define ____need_improve_end     /*...need_improve  For marking*/
#define ____need_improve_123
/**
********************************************************************************
* Index of module-related data flows or usage examples for easy understanding
********************************************************************************
*/
#define THP_CMD_TO_IC_PATH(...) // Command path to interact with IC


/**
********************************************************************************
*
********************************************************************************
*/

#define LOG_LEVEL_I              KERN_EMERG // KERN_INFO
#define LOG_LEVEL_ERR            KERN_EMERG //KERN_ERR

#define kernel_log_i_t(fmt, ...)   printk(LOG_LEVEL_I HEAD fmt, MODULE_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define kernel_log_a0( ...)
#define kernel_log_e0( ...)
#define kernel_log_d0( ...)
#define kernel_log_i0( ...)
#define kernel_log_w0( ...)
#define kernel_log_r0( ...)


#define ALG_C_LOG(level, fmt, arg...)   do { \
        if (level <= thp_drive_log_level) { \
            kernel_log_i_t(fmt, ##arg); \
        } \
} while(0)


#define ALG_C_LOG1(level, fmt, arg...)   do { \
        if (level == thp_drive_log_level) { \
            kernel_log_i_t(fmt, ##arg); \
        } \
} while(0)



#define kernel_log_a(fmt, arg...)  ALG_C_LOG(LG_ALWAYS, "%s: " fmt, "LG_ALWAYS", ##arg)
#define kernel_log_r(fmt, arg...)  ALG_C_LOG(LG_ERROR, "%s: " fmt,  "LG_ERROR", ##arg)
#define kernel_log_w(fmt, arg...)  ALG_C_LOG(LG_WARNING, "%s: " fmt,"LG_WARNING", ##arg)
#define kernel_log_i(fmt, arg...)  ALG_C_LOG(LG_INFO, "%s: " fmt,   "LG_INFO", ##arg)


#define kernel_log_i_n(level,fmt, arg...)  ALG_C_LOG1(level, "%s: " fmt,   "-LG_INFO", ##arg)
#define kernel_log_i_8(fmt, arg...)  ALG_C_LOG1(-8, "%s: " fmt,   "-LG_INFO", ##arg)
#define kernel_log_i_9(fmt, arg...)  ALG_C_LOG1(-9, "%s: " fmt,   "-LG_INFO", ##arg)
#define kernel_log_i_10(fmt, arg...)  ALG_C_LOG1(-10, "%s: " fmt,   "-LG_INFO", ##arg)
#define kernel_log_i_11(fmt, arg...)  ALG_C_LOG1(-11, "%s: " fmt,   "-LG_INFO", ##arg)


#define boot_update_log(fmt, arg...)  {boot_update_step++; kernel_log_a("step:%3d " fmt,boot_update_step,##arg);}

/**
********************************************************************************
* Only points below this value are reported to the system
*
********************************************************************************
*/
#define NO_THP_POINT_TO_INPUT_LIMIT  10


#define FRAME_MC_SC_LEN         (((FRAME_RX+FRAME_TX)<<2)+((FRAME_RX*FRAME_TX)<<1))


#define CUR_FRAME_LEN          (64+255+FRAME_MC_SC_LEN) // THP Default length of data packet
#define MCAP_NODE_MAX          (1024*6)    // 6K



#define INPUT_STYLE             88  // Coordinate data packet
#define REPORT_FLG              99  // Coordinate data packet


#define CMD_STYLE               77 // cmd data packet
#define CMD_FLG                 88 // cmd data packet


/*CUR,DEFAULT,MIN,MAX*/
#define VALUE_TYPE_SIZE 6
#define VALUE_GRIP_SIZE 9
#define MAX_BUF_SIZE 256
#define BTN_INFO 0x152
#define MAX_TOUCH_ID 10
#define RAW_BUF_NUM 4


#define CMD_DATA_BUF_SIZE        50
#define THP_CMD_BASE             1000
#define THP_CMD_TOCH_RAW_BASE    2000
#define THP_IC_CMD_BASE          3000
#define OPEN_TRANSPORT_MODE      3045
#define TOUCH_MOTION_BASE        200
#define Touch_GAMETURBOTOOL_BASE 10000

#define LAST_TOUCH_EVENTS_MAX 512

/**
********************************************************************************
*time
********************************************************************************
*/
#define UMECS_PER_SEC 1000000


/**
********************************************************************************
* Global structures, unions and enumerations using typedef
********************************************************************************
*/
typedef enum
{
    LG_ALWAYS = 0,
    LG_ERROR = 1,
    LG_WARNING = 2,
    LG_INFO = 3,
    LG_DEBUG = 4,
    LG_INOUT = 5,
} thp_log_level_t;

typedef struct semi_data_struct
{
    u8 irq_read_en[2];
    u8 write_cmd[8];
    u32 write_len;
    u32 read_len;
    u16 save_frame;
} semi_com;


/*Data sending structure*/
typedef struct semi_touch_rawdata_struct
{
    s16 rawdata[2048];
} semi_data;



typedef struct semi_touch_spi_send_struct
{
    u8 frame_index;
    u8 frame_cnt;
    semi_data data_buffer[50];
} semi_touch_spi_send;


/**
********************************************************************************
*
********************************************************************************
*/
typedef s16               raw_data_t;

enum suspend_state
{
    SEMI_TOUCH_RESUME = 0,
    SEMI_TOUCH_SUSPEND,
    SEMI_TOUCH_LP1,
    SEMI_TOUCH_LP2,
};

enum common_data_cmd
{
    SET_CUR_VALUE = 0,
    GET_CUR_VALUE,
    GET_DEF_VALUE,
    GET_MIN_VALUE,
    GET_MAX_VALUE,
    GET_MODE_VALUE,
    RESET_MODE,
    SET_LONG_VALUE,

    SET_THP_IC_CUR_VALUE,
    GET_THP_IC_CUR_VALUE,

    SET_CMD_FOR_THP,
    SET_CMD_FOR_DRIVER,
};


enum common_data_mode
{
    Touch_Game_Mode              = 0,
    Touch_Active_MODE            = 1,

    Touch_Suspend                = 27,

    Touch_Boost_EN               = TOUCH_MOTION_BASE + 0,
    Touch_Empty_Int              = TOUCH_MOTION_BASE + 1,
    Touch_Super_Report           = TOUCH_MOTION_BASE + 2,
    Touch_Idle_Scan_Rate         = TOUCH_MOTION_BASE + 3,
    Touch_Idle_THD               = TOUCH_MOTION_BASE + 4,
    Touch_Resolution             = TOUCH_MOTION_BASE + 5,
    Touch_Move_Jitter            = TOUCH_MOTION_BASE + 6,



    THP_LOCK_SCAN_MODE           = THP_CMD_BASE + 0,
    THP_FOD_DOWNUP_CTL           = THP_CMD_BASE + 1,
    THP_HAL_INIT_READY           = THP_CMD_BASE + 4,
    THP_HAL_BUILD_VERSION        = THP_CMD_BASE + 5, //
    THP_HAL_REPORT_RATE          = THP_CMD_BASE + 11,//
    THP_CALIBRATION_EN           = THP_CMD_BASE + 16,
    THP_PRINT_RUN_INFOR          = THP_CMD_BASE + 18,

    Touch_Palm_Sensor            = 26,
    THP_ACTIVE_FLG               = THP_CMD_BASE + 28,

    THP_TX_NUM                   = THP_CMD_BASE + 39,
    THP_RX_NUM                   = THP_CMD_BASE + 40,

    THP_REPORT_CLOSE_EN          = THP_CMD_BASE + 65,

    THP_IDLE_BASALINE_UPDATE     = THP_CMD_BASE + 71,
    Touch_THP_Dump               = THP_CMD_BASE + 76,
    THP_IC_FRAME_LOG_LEVE        = THP_CMD_BASE + 77,

    THP_IDLE_THD                 = THP_CMD_BASE + 84,

    THP_BASE_STATUS               = THP_CMD_BASE + 91,
    THP_GLOVE_STATUS              = THP_CMD_BASE + 92,
    THP_BETA_USER_FLAG            = THP_CMD_BASE + 93,
    THP_TEMPERATURE_STATUS        = THP_CMD_BASE + 94,

    THP_ALG_CORE_LOG             = THP_CMD_BASE + 200,

    THP_ALG_LOG_LEVEL            = THP_CMD_BASE + 201,

    THP_ALG_REPORT_EN             = THP_CMD_BASE + 202,
    THP_DRIVE_GET_FRAME_EN        = THP_CMD_BASE + 203,
    THP_DRIVE_LOG_LEVERL          = THP_CMD_BASE + 204,
    THP_DRIVE_CUR_FRAME_LEN       = THP_CMD_BASE + 205,
    THP_DRIVE_FAST_DEBUG          = THP_CMD_BASE + 206,

    
    THP_DRIVE_CUR_FRAME_ADD       = THP_CMD_BASE + 303,
    THP_DRIVE_READ_MODE           = THP_CMD_BASE + 304,   


};


enum THP_IC_MODE_TYPE
{
    SET_IDLE_THD                        = THP_IC_CMD_BASE + 1,
    SET_IDLE_RATE                       = THP_IC_CMD_BASE + 2,
    SET_ENTER_SLEEP_MODE                = THP_IC_CMD_BASE + 3,
    SET_VSYNC_EN                        = THP_IC_CMD_BASE + 4,
    SET_HSYNC_EN                        = THP_IC_CMD_BASE + 5,
    SET_FOD_EN                          = THP_IC_CMD_BASE + 6,
    SET_REPORT_RATE                     = THP_IC_CMD_BASE + 7,
    SET_SCAN_FREQ                       = THP_IC_CMD_BASE + 8,
    SET_SCAN_FREQ_HOPPING_EN            = THP_IC_CMD_BASE + 9,
    SET_AFE_EN                          = THP_IC_CMD_BASE + 10,
    SET_MC_SCAN_EN                      = THP_IC_CMD_BASE + 11,
    SET_SC_SCAN_EN                      = THP_IC_CMD_BASE + 12,
    SET_MC_CALIBRATION_EN               = THP_IC_CMD_BASE + 13,
    SET_SC_CALIBRATION_EN               = THP_IC_CMD_BASE + 14,
    SET_LINE_SHIRT_EN                   = THP_IC_CMD_BASE + 15,
    SET_INT_STATE                       = THP_IC_CMD_BASE + 16,
    SET_BASE_REFRESH_EN                 = THP_IC_CMD_BASE + 17,
    SET_FRAME_DATA_TYPE                 = THP_IC_CMD_BASE + 18,
    SET_GAME_MODE_EN                    = THP_IC_CMD_BASE + 19,
    SET_CHARGING_STATUS_EN              = THP_IC_CMD_BASE + 20,
    SET_TOUCH_IC_RESET_EN               = THP_IC_CMD_BASE + 21,
    SET_GESTURE_EN                      = THP_IC_CMD_BASE + 22,
    SET_CHLICK_GESTURE_EN               = THP_IC_CMD_BASE + 23,
    SET_DOUBLE_CHLICK_EN                = THP_IC_CMD_BASE + 24,
    SET_FLAG_BUF                        = THP_IC_CMD_BASE + 25,
    SET_ACTIVE_STYLUS_PROTOCOL          = THP_IC_CMD_BASE + 26,
    ACTIVE_STYLUS_EN                    = THP_IC_CMD_BASE + 27,
    ACTIVE_STYLUS_ONLY_EN               = THP_IC_CMD_BASE + 28,
    ACTIVE_STYLUS_TOUCH_SIMULTANEOUSLY  = THP_IC_CMD_BASE + 29,
    ACTIVE_STYLUS_SYNC_SUCESS           = THP_IC_CMD_BASE + 30,
    ACTIVE_STYLUS_GESTURE_MODE          = THP_IC_CMD_BASE + 31,
    SET_IC_RUN_STEP                     = THP_IC_CMD_BASE + 32,
    SET_NULL_MODE                       = THP_IC_CMD_BASE + 33,
    SET_IC_LOG_LEVEL                    = THP_IC_CMD_BASE + 34,
    SET_IC_CALIBRATEION                 = THP_IC_CMD_BASE + 35,
    SET_IC_SELF_TEST                    = THP_IC_CMD_BASE + 36,
    SET_IC_SOFT_RETEST                  = THP_IC_CMD_BASE + 37,
    SET_SCAN_SLOPE                      = THP_IC_CMD_BASE + 38,
    SET_SCAN_VOLTAGE                    = THP_IC_CMD_BASE + 39,
    SET_SCAN_NUM                        = THP_IC_CMD_BASE + 40,
    SET_SCAN_FREQ_NUM                   = THP_IC_CMD_BASE + 41,
    SET_IC_WORK_MODE                    = THP_IC_CMD_BASE + 42,
    SET_FILTER_LEVEL                    = THP_IC_CMD_BASE + 43,
    SET_RAW_TYPE                        = THP_IC_CMD_BASE + 44,

    SET_OPEN_TRANSPORT_MODE             = THP_IC_CMD_BASE + 45,
    SET_CRC_EN                          = THP_IC_CMD_BASE + 46,
    GET_OPEN_TRANSPORT_MODE             = THP_IC_CMD_BASE + 47,
    SET_TOUCH_IC_INFO                   = THP_IC_CMD_BASE + 50,
};


typedef struct common_data
{
    s8 touch_id;
    u8 cmd;
    u16 mode;
    u16 data_len;
    s32 data_buf[CMD_DATA_BUF_SIZE];
} common_data_t;


struct semi_touch_interface
{
    int thp_cmd_buf[MAX_BUF_SIZE];
    int thp_cmd_size;

    int (*setModeValue)(int Mode, int value);
    int (*setModeLongValue)(int Mode, int value_len, int *value);
    int (*getModeValue)(int Mode, int value_type);
    int (*getModeAll)(int Mode, int *modevalue);
    int (*resetMode)(int Mode);
    int (*prox_sensor_read)(void);
    int (*prox_sensor_write)(int on);
    int (*palm_sensor_read)(void);
    int (*palm_sensor_write)(int on);
    int (*get_touch_rx_num)(void);
    int (*get_touch_tx_num)(void);
    int (*get_touch_x_resolution)(void);
    int (*get_touch_y_resolution)(void);
    int (*enable_touch_raw)(bool en);
    int (*enable_clicktouch_raw)(int count);
    int (*enable_touch_delta)(bool en);
    u8(*panel_vendor_read)(void);
    u8(*panel_color_read)(void);
    u8(*panel_display_read)(void);
    char (*touch_vendor_read)(void);


    int long_mode_len;
    int long_mode_value[MAX_BUF_SIZE];


    bool is_enable_touchraw;
    int thp_downthreshold;
    int thp_upthreshold;
    int thp_movethreshold;
    int thp_noisefilter;
    int thp_islandthreshold;
    int thp_smooth;
    int thp_dump_raw;

    bool is_enable_touchdelta;

};

struct semi_touch
{

    struct miscdevice   misc_dev;

    struct device *dev;

    struct class *class;

    struct attribute_group attrs;

    struct mutex  mutex;
    struct mutex  palm_mutex;
    struct mutex  prox_mutex;

    wait_queue_head_t   wait_queue;
};



enum touch_state
{
    EVENT_INIT,
    EVENT_DOWN,
    EVENT_UP,
};

struct touch_event
{

    u32 slot;
    enum touch_state state;

    struct timespec64 touch_time;
};


struct last_touch_event
{
    int head;
    struct touch_event touch_event_buf[LAST_TOUCH_EVENTS_MAX];
};

struct semi_touch_pdata
{

    struct semi_touch *device;

    struct semi_touch_interface *touch_data[2];


    dma_addr_t phy_base;

    int raw_head;
    int raw_tail;

    int raw_len;
    unsigned int *raw_buf[RAW_BUF_NUM];

    unsigned int *raw_data; // mmap

    spinlock_t raw_lock;

    spinlock_t cmd_lock;

    spinlock_t input_lock;


    struct mutex  ioctl_mutex;
    struct mutex  write_mutex;


    int palm_value;
    bool palm_changed;

    int prox_value;
    bool prox_changed;
    int suspend_state;


    const char *name;


    struct proc_dir_entry  *last_touch_events_proc;

    struct last_touch_event *last_touch_events;
};

/**
********************************************************************************
*.thp frame  :drive infor +tp ic infor
********************************************************************************
*/
#pragma pack(1)
typedef struct
{
    u16 frame_no;
    u16 frame_data[2000];
} RepotDbgBufThp;

typedef  struct
{
    s64 time_ns;
    u64 frame_cnt;

    u8  wake_up_cnt;
    u8  ev_raw__cnt;
    s16 fod_pressed;

    u8  bottom;
    u8  top;

    s16 fod_trackingId; // struct tp_frame END

    char thp_frame_buf[4096 /* PAGE_SIZE */];

    s32 dump_type;
    u16 thp_dbg_buf[sizeof(RepotDbgBufThp)/2];

} tp_frame_drive;


typedef  struct
{
    u8  bottom;
    u8  top;
    s16 buf_len;
    s16 buf_len_r;

    u32 reserve[5];

    tp_frame_drive buf[DRIVE_BUF_NUM];
} tp_frame_drive_nbuf;


typedef struct
{
    s64 time_ns;
    u64 frame_cnt;
    s32 fod_pressed;
    s32 fod_trackingId;
} tp_frame;


typedef struct
{
    u8 protocol_type;
    u8 protocol_v;

    u16 ic_head_cnt;

    s32 crc;
    s16 crc_len;
    s16 crc_r_len;
    s32 crc_r;

    s16 code_v;
    s16 cur_frame_len_r;

    s8 data_state;
    s8 data_type;

    u8 event_info: 4;
    u8 base_flg:  3;
    u8 water_flg: 1;

    s8 noise_level;
    s8 scan_mode;
    s8 scan_rate_index;
    u16 scan_freq;
    u16 ic_frame_cnt;
    u16 drop_frame_no;
    u16 noise_r0;
    u16 noise_r1;
    u16 noise_r2;
    u16 noise_r3;
    u16 noise_r4;
    u16 noise_r5;

    u16 cur_frame_len;
    u16 next_frame_len;
    s8 col_num;// long
    s8 row_num;// short

    u16 ic_ms_time;

    u16 scan_rate;
    s8  scan_freq_index;
    s8  write_cmd_cnt;

    s8  frame_data_type;
    u16 flg_buf;
    u8  debug_buf_size;

    u16 reserved_big_buf_size;
    u16 write_cmd;


//    s16 *mc_raw;
//    s16 *sc_raw_tx; //short
//    s16 *sc_raw_rx; //long
//
//    s16 *sc_raw2_tx; //short
//    s16 *sc_raw2_rx; // long
//
//    char *debug_buf;

    s16 frame_data[MCAP_NODE_MAX]; // frame + debug_buf  frame_data


} tp_raw;


typedef struct
{
    s8 input_style;

    s8 report_size;

    u8 readbuffer[248];

    u8 reportflg;

    s32 irq_cnt;

} report_piont_t_semi;

#pragma pack()


/**
********************************************************************************
* THP_EN    0 reports coordinates
            1 thp raw data packet,
            2 Automatic recognition, default reading of firmware coordinate
            report points, if THP algorithm is loaded. Change to THP
            reporting point
********************************************************************************
*/

typedef struct
{
    u16 thp_en;
    u16 debug_flg;
    u32 spi_speed;

    u16 cur_frame_len_bak;
    u16 cur_frame_len_bak_en;

    u16 cur_frame_len;

    u16 frame_data_en;

    int read_frame_err;

    u16 thp_get_frame_en;

    u8 bottom_top_dif_max ;
    u32 loss_frame_cnt ;
    u8 read_mode;

    u32 frame_log_cnt_limit;

    tp_raw frame_data;

    char version[256];


} semi_thp;


/**
********************************************************************************
*
********************************************************************************
*/



/**
********************************************************************************
* Global variable extern declarations
********************************************************************************
*/
extern semi_thp semi_thp_dev;
extern  struct sm_touch_dev st_dev;
extern s8 thp_drive_log_level;
extern  int boot_update_step;


/**
********************************************************************************
* Global function prototypes
********************************************************************************
*/
void semi_thp_driver_exit(void);

int  semi_thp_driver_init(struct sm_touch_dev *st_dev);

int semi_thp_irq_handler(int irq_cnt);

int semi_thp_get_thp_ver(char *ver);

u32 semi_touch_file_set_spi_speed(u32 speed_hz, struct sm_touch_dev *st_dev);

void input_lock(void);

void input_unlock(void);

/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
#else

#define FORCE_UPDATE_RUN_RAM_CODE()
#define TEST_BOARD_EN                 0

#endif



/**
********************************************************************************
*
********************************************************************************
*/
#endif
/*********************** (C) COPYRIGHT  ***********END OF FILE ****************/
