#ifndef __TRANSSION_THP_INTERFACE_H__
#define __TRANSSION_THP_INTERFACE_H__

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/atomic.h>
#include <net/sock.h>
#include <linux/spinlock.h>
#include <linux/gfp.h>
//#include "transsion_core.h"

/*
 * ------------------------------
 * IOCTL Commands
 * ------------------------------
 */
#define THP_DEVICE_NAME                   "tran_thp"
#define THP_INPUT_MISC_NAME               "thp_input"

#define THP_INPUT_IOCTL_TYPE              0x11
#define THP_IOCTL_INPUT_REPORTER_TOUCH    _IOW(THP_INPUT_IOCTL_TYPE, 0x01, struct tran_thp_alg_info)
#define THP_IOCTL_INPUT_RELEASE_ALL       _IO(THP_INPUT_IOCTL_TYPE, 0x02)
#define THP_IOCTL_INPUT_SET_IRQ           _IOW(THP_INPUT_IOCTL_TYPE, 0x03, long)
#define THP_IOCTL_INPUT_RESET             _IO(THP_INPUT_IOCTL_TYPE, 0x04)
#define THP_IOCTL_INPUT_GET_QUEUE_PARAM   _IOR(THP_INPUT_IOCTL_TYPE, 0x05, struct thp_queue_params)
#define THP_IOCTL_GET_INIT_INFO           _IOR(THP_INPUT_IOCTL_TYPE, 0x06, struct tran_thp_init_info)
#define THP_IOCTL_INPUT_GET_FW_STATUS     _IOW(THP_INPUT_IOCTL_TYPE, 0x07, long)
#define THP_IOCTL_INPUT_SET_VERSION_INFO  _IOW(THP_INPUT_IOCTL_TYPE, 0x08, struct thp_version_info)
#define THP_IOCTL_INPUT_SET_THP_MODE      _IOW(THP_INPUT_IOCTL_TYPE, 0x09, bool)
#define THP_IOCTL_INPUT_GET_ALG_STATUS    _IOW(THP_INPUT_IOCTL_TYPE, 0x0A, struct tran_thp_alg_debug)
/*
 * ------------------------------
 * Macros
 * ------------------------------
 */
#define TRAN_THP_ALLOC_ORDER            4
#define TRAN_THP_ALLOC_PAGE_NUM         (1 << TRAN_THP_ALLOC_ORDER)
#define TRAN_FRAME_BUF                  PAGE_SIZE
#define TRAN_MAX_QUEUE_BUFSIZE          (TRAN_THP_ALLOC_PAGE_NUM * PAGE_SIZE)//PAGE_SIZE:4096
#define TRAN_MAX_QUEUE_COUNT            (TRAN_MAX_QUEUE_BUFSIZE / TRAN_FRAME_BUF - 1)
#define TRAN_THP_NETLINK_MSG_TYPE       (NLMSG_MIN_TYPE + TRAN_THP_NETLINK_ROUTE)
#define TRAN_THP_NETLINK_ROUTE          23//DO NOT > MAX_LINKS:32
#define THP_MAX_POINTS_SUPPORT          12
#define TRAN_THP_VERSION_LEN            (64)
#define TRAN_THP_GFP_FLAGS              (GFP_KERNEL | __GFP_ZERO)

#define TRAN_THP_RESERVED_MAX_LEN       32
#define TRAN_THP_ALG_DEBUG_RESERVED_LEN 8
/*
 * ------------------------------
 * Data Structures
 * ------------------------------
 */


struct thp_version_info {
    char ini_version[TRAN_THP_VERSION_LEN];
    char hal_version[TRAN_THP_VERSION_LEN];
    char algo_version[TRAN_THP_VERSION_LEN];
    char edge_version[TRAN_THP_VERSION_LEN];
};

#pragma pack(1)
struct tran_thp_frame {
    uint64_t hal_cnt;
    int64_t tv;
    uint64_t driver_cnt;
    uint8_t value[];
};
#pragma pack()

struct tran_thp_queue {
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint8_t max_count;
    uint64_t drive_frame_cnt;
    uint32_t elem_size;
    uint32_t buffer_size;
    uint32_t loss_frame_cnt;
    uint32_t cache_frame_cnt;
    uint8_t buffer[];
};

#pragma pack(1)
struct tran_thp_alg_debug {
    int water_mode_debug;
    int water_sc_filter_level;
    int water_jitter_level;
    int rain_water_mode;
    uint32_t framecnt_after_resume;
    bool palm_status;
    uint8_t adaptive_lgm_level;
    bool weak_water_proc_sts;
    uint8_t reserved[TRAN_THP_ALG_DEBUG_RESERVED_LEN];
};

struct tran_thp_touch_event {
    uint8_t valid;
    uint32_t x;      /*x coordinate */
    uint32_t y;      /*y coordinate */
    uint16_t p;      /* pressure */
    uint8_t flag;   /* touch event flag: 0 -- up; 1-- down; 2 -- contact */
    uint8_t id;     /*touch ID */
    uint16_t major;
    uint16_t minor;
    uint16_t value;
    uint8_t rx;
    uint8_t tx;
};
struct thp_input_ioctl_touch_data {
    uint8_t touch_num;
    struct tran_thp_touch_event events[THP_MAX_POINTS_SUPPORT];
    uint8_t last_touch_num;
    uint8_t touch_mode;      //0: default  1:IC  2:thp
    int32_t water_mode;      //0: Not water_mode other:water_mode
    int32_t last_water_mode; //0: Not water_mode other:water_mode
};
struct tran_thp_alg_info{
    struct thp_input_ioctl_touch_data ts_events;
    struct tran_thp_alg_debug alg_debug;
};
#pragma pack()

//netlink cmd enum
enum tran_thp_netlink_cmd {
    TRAN_NL_SCREEN_OFF,       //0
    TRAN_NL_SCREEN_ON,        //1
    TRAN_NL_TOUCH_CHARGOUT,   //2
    TRAN_NL_TOUCH_CHARGIN,    //3
    TRAN_NL_WIRELESS_CHARGE,  //4
    TRAN_NL_SCREEN_LOCK,      //5
    TRAN_NL_SCREEN_UNLOCK,    //6
    TRAN_NL_KEYPAD_ON,        //7
    TRAN_NL_KEYPAD_OFF,       //8
    TRAN_NL_ROTATION_DOWN,    //9
    TRAN_NL_ROTATION_LEFT,    //10
    TRAN_NL_ROTATION_RIGHT,   //11
    TRAN_NL_SCENE_NORMAL,     //12
    TRAN_NL_SCENE_VIDEO,      //13
    TRAN_NL_SCENE_GAME,       //14
    TRAN_NL_SCENE_CAMERA,     //15
    TRAN_NL_SCENE_DIAL_PAD,   //16

    TRAN_NL_DEBUG_BASE = 100,
    TRAN_NL_DEBUG_THP_MODE_ON = 101,
    TRAN_NL_DEBUG_THP_MODE_OFF = 102,
    TRAN_NL_HAL_LOG_LEVEL_ERROR = 103,
    TRAN_NL_HAL_LOG_LEVEL_INFO = 104,
    TRAN_NL_HAL_LOG_LEVEL_DEBUG = 105,
    TRAN_NL_HAL_LOG_LEVEL_VERBOSE = 106,
    TRAN_NL_DEBUG_THP_MODE_WDT_TEST = 107,

    TRAN_NL_PERFORMANCE_BASE = 200, //normal func msg
    TRAN_NL_PERFORMANCE_POLL_SHAREMEM = 201,
    TRAN_NL_MAX_CMD
};

struct tran_thp_netlink_msg {
    enum tran_thp_netlink_cmd cmd;
    char *msg;
};

struct thp_queue_params {
    uint32_t frame_len;
    uint32_t crc_len;
    uint32_t queue_size;
    uint32_t queue_count;
    uint8_t thp_tx_num;
    uint8_t thp_rx_num;
    uint8_t endian_mode;
    uint8_t reserved[TRAN_THP_RESERVED_MAX_LEN];
};

struct tran_thp_scene {
    bool is_awake;
    int orientation;
    bool game_mode;
    int pocket_mode;
    int typing_mode;
    int camera_mode;
    int charging_status;
    int video_mode;
    int res_mode;
};

struct tran_thp_init_info {
    struct thp_queue_params param;
    char chip_info[64];
    struct tran_thp_scene scene;
    uint8_t reserved[TRAN_THP_RESERVED_MAX_LEN];
};

struct tran_thp_ops {
    int (*thp_enqueue)(u8 *, u32, s64);
    int (*send_msg)(const int msg);
    s32 (*crc32_check)(u8 *, u16);
};

struct tran_thp_custom_param {
    uint32_t thp_data_head_size;
    uint32_t thp_tx_num;
    uint32_t thp_rx_num;
    uint32_t thp_crc_offset;
    uint32_t thp_touch_event_offset;
    uint32_t thp_crc_len;
    uint32_t thp_frame_len;
    uint32_t thp_touch_offset;
    uint32_t thp_debug_buf_size;
    uint32_t thp_report_buf_size;
    uint32_t thp_reserves_big_buf_size;
};

struct tran_thp_data {
    struct miscdevice thp_misc_dev;
    struct tran_thp_ops *q_ops;
    struct tran_thp_queue *thp_q;
    struct thp_version_info ver_info;
    struct thp_queue_params q_params;
    struct tran_thp_custom_param thp_custom_param;
    struct mutex mutexq;
    spinlock_t data_lock;
    struct kobject *thp_kobj;
    struct sock *nl_sk;
    wait_queue_head_t frame_waitq;
    atomic_t refcount;
    atomic_t data_valid;
    atomic_t poll_cnt;
    uint8_t thp_dev_open_cnt;
    uint8_t log_level;
    uint8_t hal_log_level;
    bool thp_mode_debug;
    bool thp_mode;
    bool thp_crc_valid;
    bool big_endian;
};

extern struct tran_thp_data *tran_thp_device_init(void);
extern void tran_thp_device_exit(struct tran_thp_data *thp_data);

#endif