#ifndef CTS_CORE_H
#define CTS_CORE_H

//#include "cts_config.h"
/*bsp:add for transsion core by guoxin.zhu 20230515 start*/
#include "chipone_transsion.h"
/*bsp:add for transsion core by guoxin.zhu 20230515 end*/
/*bsp:add for deep sleep by guoxin.zhu 20230908 start*/
/*
 * For commnication error in PM(deep sleep) state
 */
#ifdef CFG_CTS_GESTURE
#define CTS_PATCH_COMERR_PM                 1
#define CTS_TIMEOUT_COMERR_PM               700
#else 
#define CTS_PATCH_COMERR_PM                 0
#endif
/*bsp:add for deep sleep by guoxin.zhu 20230908 end*/

enum cts_dev_hw_reg {
    CTS_DEV_HW_REG_HARDWARE_ID     = 0x00000u,
    CTS_DEV_HW_REG_CLOCK_GATING    = 0x00004u,
    CTS_DEV_HW_REG_RESET_CONFIG    = 0x00008u,
    CTS_DEV_HW_REG_BOOT_MODE       = 0x00010u,
    CTS_DEV_HW_REG_CURRENT_MODE    = 0x00011u,
    CTS_DEV_HW_REG_DDI_REG_CTRL    = 0x0002Cu,
    CTS_DEV_HW_REG_CLK_DIV_CFG     = 0x00033u,
    CTS_DEV_HW_REG_HPPROG          = 0x00074u,
    CTS_DEV_HW_REG_SET_RESET       = 0x000F8u,
    CTS_DEV_HW_REG_CASC_STATE      = 0x00060u,
};

#define CTS_RST_BIT      ~BIT(3)
#define CTS_CHIP_TP_RST_BIT     ~BIT(1)
#define CTS_RST_BIT_MASK        0xFF
#define CTS_CASC_EN_MASK    BIT(1)
#define CTS_CASC_MASTER_MASK    BIT(0)

enum cts_boot_mode {
    CTS_BOOT_MODE_SRAM      = 0,
    CTS_BOOT_MODE_PROGRAM   = 1,
    CTS_BOOT_MODE_UNKOWN    = 2,
};

enum cts_dev_boot_mode {
    CTS_DEV_BOOT_MODE_IDLE          = 0,
    CTS_DEV_BOOT_MODE_FLASH         = 1,
    CTS_DEV_BOOT_MODE_TCH_PROG      = 2,
    CTS_DEV_BOOT_MODE_SRAM          = 3,
    CTS_DEV_BOOT_MODE_DDI_PROG       = 4,
    CTS_DEV_BOOT_MODE_SPI_PRG_9911  = 5,
    CTS_DEV_BOOT_MODE_UNKOWN        = 127,
};

#define CTS_REGDEF_BASE0    0x30000
#define CTS_REGDEF_BASE1    0x70000

#define CTS_DEV_BOOT_MODE_I2C_PRG_9911  CTS_DEV_BOOT_MODE_TCH_PROG
#define CTS_DEV_BOOT_MODE_MASK  0x07


/** Hardware IDs, read from hardware id register */
enum cts_dev_hwid {
    CTS_DEV_HWID_ICNL9911  = 0x990100u,
    CTS_DEV_HWID_ICNL9911S = 0x990100u,
    CTS_DEV_HWID_ICNL9911C = 0x991100u,
    CTS_DEV_HWID_ICNL9911D = 0x99D100u,
    CTS_DEV_HWID_ICNL9916  = 0x990100u,
    CTS_DEV_HWID_ICNL9916C = 0x99C100u,
    CTS_DEV_HWID_ICNL9916X = 0x996100u,
    CTS_DEV_HWID_ICNL9922  = 0x990200u,
    CTS_DEV_HWID_ICNL9922C = 0x99C200u,
    CTS_DEV_HWID_ICNL9951  = 0x990500u,
    CTS_DEV_HWID_ICNL9951R = 0x991500u,
    CTS_DEV_HWID_ICNL9951C = 0x99c500u,
    CTS_DEV_HWID_ICNL9905  = 0x99000u,

    CTS_DEV_HWID_INVALID   = 0xFFFFFFFFu,
};

/* Firmware IDs, read from firmware register @ref CTS_DEV_FW_REG_CHIP_TYPE
 * under normal mode
 */
enum cts_dev_fwid {
    CTS_DEV_FWID_ICNL9911  = 0x9911u,
    CTS_DEV_FWID_ICNL9911S = 0x9964u,
    CTS_DEV_FWID_ICNL9911C = 0x9954u,
    CTS_DEV_FWID_ICNL9911D = 0x9954u,  // TODO: to be confirmed
    CTS_DEV_FWID_ICNL9916  = 0x9916u,
    CTS_DEV_FWID_ICNL9916C = 0x9959u,
    CTS_DEV_FWID_ICNL9916X = 0x996eu,
    CTS_DEV_FWID_ICNL9922  = 0x9922u,
    CTS_DEV_FWID_ICNL9922C = 0x9965u,
    CTS_DEV_FWID_ICNL9951  = 0x9951u,
    CTS_DEV_FWID_ICNL9951R = 0x99a3u,
    CTS_DEV_FWID_ICNL9951C = 0x9994u,
    CTS_DEV_FWID_ICNL9905  = 0x9905u,

    CTS_DEV_FWID_ANY       = 0u,
    CTS_DEV_FWID_INVALID   = 0xFFFFu
};

#ifdef CONFIG_CTS_TP_PROXIMITY
enum cts_proximity_state {
    CTS_CMD_PROXIMITY_STATUS = 0x80,
    /* For vivo code, make a distinction between palm func and proximity func */
    CTS_CMD_PROXIMITY_NEAR_VIVO = 0x30,
    CTS_CMD_PROXIMITY_FAR_VIVO = 0x31,
};
#endif

#pragma pack(1)

struct cts_firmware_status {
    u16 charger:1;
    u16 proximity:1;
    u16 earjack:1;
    u16 knuckle:1;
    u16 glove:1;
    u16 pocket:1;
    u16 game:1;
    u16 reserved:1;
    u16 high_temperature:1;
    u16 low_temperature:1;
    u16 bend_mode:1;
    u16 palm_status:1;
    u16 noise_mode:1;
    u16 base_trace:1;
    u16 water_mode:1;
    u16 ground:1;
};
/** Touch debug messages read back from chip [40 bytes] */
// TODO:: outdated structure
struct cts_touch_debug_msg {
    u8 protocol_version;
    u8 fw_version;
    u32 reset_flag;
    u16 frame_index;
    u16 firmware_status;

    u8 proximity;
    u8 work_mode;
    u8 power_mode;
    u8 curr_freq;
    u8 esd_0a_status;
    u8 fw_esd_status;
    u8 landscape_mode;
    u16 curr_freq_noise;
    u16 max_diff;
    u8 max_diff_row;
    u8 max_diff_col;
    u16 max_neg_diff;
    u8 max_neg_diff_row;
    u8 max_neg_diff_col;
    u8 data_method;
    u8 data_type;
    u8 reserved[11];
};

#define CTS_GESTURE_UP                  (0x11)
#define CTS_GESTURE_C                   (0x12)
#define CTS_GESTURE_O                   (0x13)
#define CTS_GESTURE_M                   (0x14)
#define CTS_GESTURE_W                   (0x15)
#define CTS_GESTURE_E                   (0x16)
#define CTS_GESTURE_S                   (0x17)
#define CTS_GESTURE_B                   (0x18)
#define CTS_GESTURE_T                   (0x19)
#define CTS_GESTURE_H                   (0x1A)
#define CTS_GESTURE_F                   (0x1B)
#define CTS_GESTURE_X                   (0x1C)
#define CTS_GESTURE_Z                   (0x1D)
#define CTS_GESTURE_V                   (0x1E)
#define CTS_GESTURE_D_TAP               (0x50)
#define CTS_GESTURE_TAP                 (0x7F)
//USD:modify for lcd aod by guoli.niu 20250321 start
#define CTS_GESTURE_O_TAP               (0x7F)
//USD:modify for lcd aod by guoli.niu 20250321 end
#define CTS_GESTURE_DOWN                (0x22)
#define CTS_GESTURE_LEFT                (0x23)
#define CTS_GESTURE_RIGHT               (0x24)
//BSP:add for pocket mode by xihao.ding 20251129 start
#define CTS_CMD_PALM_DETECTED           (0xC0)
//BSP:add for pocket mode by xihao.ding 20251129 end

enum cts_tp_protocols {
    CTS_TP_LEGACY = 0,
    CTS_TP_FINGER_ONLY,
    CTS_TP_STYLUS_ONLY,
    CTS_TP_FINGER_STYLUS,
    CTS_TP_PROTOCOL_MAX,
};

struct cts_touch_info_header {
    union {
        u8 vkey_state;
        u8 gesture_id;
        struct {
            u8 gesture_id:7;
            u8 has_ps_info:1;
        }gesture_info;
    };

    struct {
        u8 finger_num:4;
        u8 :1;
        u8 has_heartbeat:1;
        u8 protocol:2;
    }proto_info;
};

#define CTS_TOUCH_EVENT_NONE         (0)
#define CTS_TOUCH_EVENT_DOWN         (1)
#define CTS_TOUCH_EVENT_MOVE         (2)
#define CTS_TOUCH_EVENT_STAY         (3)
#define CTS_TOUCH_EVENT_UP           (4)
/** Touch message read back from chip */
struct cts_touch_msg_finger {
    struct {
        u8 event:4;
        u8 id:4;
    }id_info;
    struct {
        u8 x_h:4;
        u8 y_h:4;
    }xy_h;
    u8 x_l;
    u8 y_l;
    u8 pressure;
};

struct cts_touch_msg_stylus {
    struct {
        u8 event:4;
        u8 id:4;
    }id_info;
    struct {
        u8 x_h:4;
        u8 y_h:4;
    }xy_h;
    u8 x_l;
    u8 y_l;
    struct {
        u8 x_h:4;
        u8 y_h:4;
    }ring_xy_h;
    u8 ring_x_l;
    u8 ring_y_l;
    u8 battery;
    u8 tilt;
    u8 pressure_l;
    struct {
        u8 pressure_h:5;
        u8 key0:1;
        u8 key1:1;
        u8 key2:1;
    }key_info;
    u8 tilt_x;
    u8 tilt_y;
};

struct cts_ps_info{
    struct {
        u8 ps_val:1;
        u8 :4;
        u8 ps_level:4;
    }ps_info;
    struct {
        u8 palm_val:1;
        u8 palmswipe_major:7;
        u8 packet_val:1;
        u8 palmswipe_minor:7;
        u8 :7;
        u8 palmswipe_val:1;
    }palmswipe_info;
};

struct cts_touchinfo_finger {
    struct cts_touch_info_header header;
    u8 reserved;
    struct cts_touch_msg_finger msg[CFG_CTS_MAX_TOUCH_NUM];
    struct cts_ps_info ps_info;
    u8 hart_beat_info[CFG_CTS_HEART_BEAT_INFO_LEN];
};

struct cts_touchinfo_stylus {
    struct cts_touch_info_header header;
    u8 reserved;
    struct {
        u8 num:4;
        u8 ver:4;
    }stylus_info;
    struct cts_touch_msg_stylus msg[CFG_CTS_MAX_STYLUS_NUM];
    u8 reserved1;
    u8 hart_beat_info[CFG_CTS_HEART_BEAT_INFO_LEN];
};

struct cts_touchinfo_finger_stylus {
    struct cts_touch_info_header header;
    u8 reserved;
    struct cts_touch_msg_finger finger_msg[CFG_CTS_MAX_TOUCH_NUM];
    struct cts_ps_info ps_info;
    struct {
        u8 num:4;
        u8 ver:4;
    }stylus_info;
    struct cts_touch_msg_stylus stylus_msg[CFG_CTS_MAX_STYLUS_NUM];
    u16 reserved1;
};

/** Touch message read back from chip */
struct cts_touch_msg_legacy {
    union {
        struct {
            u8 ps_level:2;
            u8 palm_val:1;
            u8 packet_val:1;
            u8 id:4;
        }id_info;
        u8 id;
    };
    u16 x;
    u16 y;
    u8 pressure;
    u8 event;
};

/** Touch information read back from chip */
struct cts_touch_info_legacy {
    struct cts_touch_info_header header;
    struct cts_touch_msg_legacy msgs[CFG_CTS_MAX_TOUCH_NUM];
};

#pragma pack()

struct cts_device;

enum cts_crc_type {
    CTS_CRC16 = 1,
    CTS_CRC32 = 2,
};

enum cts_bus_type {
    CTS_BUS_SPI     = 0,
    CTS_BUS_I2C,
    CTS_BUS_I3C,
    CTS_BUS_MAX,
};

enum spi_protocol_type {
    CTS_SPI_DRW     = 0,
    CTS_SPI_PRW,
    CTS_SPI_PROTOCOL_MAX,
};

enum i2c_protocol_type {
    CTS_I2C_DRW     = 0,
    CTS_I2C_PRW,
    CTS_I2C_PROTOCOL_MAX,
};

struct cts_device_hwdata {
    char *name;
    u32 hwid;
    u16 fwid;
    u8 num_row;
    u8 num_col;
    bool has_flash;
    bool support_cascade;

    u32 regdef_base;
    u32 sfctl_base;
    u32 i2ccfg_base;
    u32 spi_base;
    u32 gpio_base;
    u32 krang_base;

    u32 flash_ddi_para_addr;
    u32 flash_ddi_init_en_addr;

    u8 i2c_protocol;
    u8 spi_protocol;
    u8 i2c_addr_norm;
    u8 i2c_addr_prog;
    u8 spi_addr_norm;
    u8 spi_addr_prog;

    struct cts_sfctrl *sfctrl;
    int (*enable_access_ddi_reg)(struct cts_device *cts_dev, bool enable);
};

enum power_mode {
    PWR_MODE_ACTIVE = 0,
    PWR_MODE_MNT,
    PWR_MODE_HIBERNATE,
    PWR_MODE_GSTR_WAKEUP,
    PWR_MODE_MNT_DBG,
    PWR_MODE_GSTR_DBG,
    PWR_MODE_HW_MONITOR,
    PWR_MODE_AOD,
    PWR_MODE_AOD_DBG
};

enum int_data_type {
    INT_DATA_TYPE_NONE = 0,
    INT_DATA_TYPE_RAWDATA = BIT(0),
    INT_DATA_TYPE_MANUAL_DIFF = BIT(1),
    INT_DATA_TYPE_REAL_DIFF = BIT(2),
    INT_DATA_TYPE_NOISE_DIFF = BIT(3),
    INT_DATA_TYPE_BASEDATA = BIT(4),
    INT_DATA_TYPE_CNEGDATA = BIT(5),
    INT_DATA_TYPE_MASK = 0x3FF,
};

enum int_data_method {
    INT_DATA_METHOD_NONE = 0,
    INT_DATA_METHOD_HOST = 1,
    INT_DATA_METHOD_POLLING = 2,
    INT_DATA_METHOD_DEBUG = 3,
    INT_DATA_METHOD_CNT = 4,
};

struct cts_touch_xy_info {
    u16 res_x;
    u16 res_y;
    u8 rows;
    u8 cols;
    u8 lp_rows;
    u8 lp_cols;
};

struct cts_ic_channel_info {
    u8 ic_fs_cols;
    u8 ic_fs_rows;
    u8 ic_gs_cols;
    u8 ic_gs_rows;
    u8 ic_pc_cols;
    u8 ic_pc_rows;
    u8 ic_pr_cols;
    u8 ic_pr_rows;
};

struct cts_panel_info {
    u8 panel_type;
    u8 panel_scal;
    u8 panel_size;
    u16 res_x;
    u16 res_y;
    u8 fs_cols;
    u8 fs_rows;
    u8 gs_cols;
    u8 gs_rows;
    u8 pc_cols;
    u8 pc_rows;
    u8 pr_cols;
    u8 pr_rows;
};

/** Chip firmware data */
struct cts_device_fwdata {
    u16 version;
    u32 module_id;
    struct cts_touch_xy_info xy_info;
    struct cts_ic_channel_info channel_info;
    struct cts_panel_info panel_info;
    bool flip_x;
    bool flip_y;
    bool swap_axes;
    u8 ddi_version;
    u8 int_mode;
    u8 esd_method;
    u16 lib_version;
    u16 int_keep_time;
    u16 rawdata_target;
    bool has_int_data;
    u8 int_data_method;
    u16 int_data_types;
    size_t int_data_size;
    struct cts_firmware *cts_fw;
};

/** Chip runtime data */
struct cts_device_rtdata {
    u8 boot_mode;

    bool suspended;
    bool updating;
    bool testing;
    bool update_work_run;

    bool gesture_wakeup_enabled;
    bool charger_exist;
    bool fw_log_redirect_enabled;
    bool glove_mode_enabled;
    bool is_casc_master;

    u8 esd_count;
    u16 firmware_status;

    void *touch_info;

#ifdef CONFIG_CTS_TP_PROXIMITY
    bool proximity_status;
    int proximity_num;
#endif

    u16 tcscmd[32];
    int tcscmd_len;
    u16 curr_cmd;
    int curr_len;
/*bsp:add for deep sleep by guoxin.zhu 20230908 start*/
#if CTS_PATCH_COMERR_PM
    struct completion pm_completion;
    bool pm_suspend;
#endif
/*bsp:add for deep sleep by guoxin.zhu 20230908 end*/
};

struct cts_device {
    struct cts_platform_data *pdata;
    const struct cts_test_val *test_vals;
    const struct cts_device_hwdata *hwdata;
    struct cts_device_fwdata fwdata;
    struct cts_device_rtdata rtdata;
    const struct cts_flash *flash;
    enum cts_bus_type bus_type;
    bool enabled;
    //BSP:add for pocket mode by xihao.ding 20251129 start
    bool is_pocket_in;
    //BSP:add for pocket mode by xihao.ding 20251129 end
};

struct chipone_ts_data {
    struct i2c_client *i2c_client;
    struct spi_device *spi_client;
    struct device *device;
    struct cts_device cts_dev;
    struct cts_platform_data *pdata;
    struct workqueue_struct *workqueue;
    struct delayed_work fw_upgrade_work;
    struct work_struct ts_resume_work;
#ifdef CONFIG_CTS_CHARGER_DETECT
    void *charger_detect_data;
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    void *earjack_detect_data;
#endif

#ifdef CONFIG_CTS_ESD_PROTECTION
    struct workqueue_struct *esd_workqueue;
    struct delayed_work esd_work;
    bool esd_enabled;
    int esd_check_fail_cnt;
#endif

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    struct workqueue_struct *heart_workqueue;
    struct delayed_work heart_work;
#endif

#ifdef CONFIG_CTS_LEGACY_TOOL
    struct proc_dir_entry *procfs_entry;
#endif

    void *oem_data;

    bool force_reflash;
	//USD:modify for lcd aod by yun.luo 20240527 start
    u8 bv;
    bool lcd_aod_flag;
    //USD:modify for lcd aod by yun.luo 20240527 end
    struct kobject *suspend_kobj;
};

#define wrap(max, x)        ((max) - 1 - (x))

extern u16 cts_crc16(const u8 *data, size_t len);
extern u32 cts_crc32(const u8 *data, size_t len);

extern void cts_lock_device(const struct cts_device *cts_dev);
extern void cts_unlock_device(const struct cts_device *cts_dev);

extern int cts_irq_handler(struct cts_device *cts_dev);
extern void cts_firmware_upgrade_work(struct work_struct *work);

extern bool cts_is_device_suspended(const struct cts_device *cts_dev);
extern int cts_suspend_device(struct cts_device *cts_dev);
extern int cts_resume_device(struct cts_device *cts_dev);

int cts_is_casc_enabled(const struct cts_device *cts_dev);
int cts_plat_is_casc_master(const struct cts_device *cts_dev);
bool is_sram_mode(const struct cts_device *cts_dev, u8 dev_boot_mode);
bool is_dev_bootmode_program_mode(const struct cts_device *cts_dev, u8 boot_mode);
extern bool cts_is_program_mode(const struct cts_device *cts_dev);
extern bool cts_is_sram_mode(const struct cts_device *cts_dev);
extern int cts_enter_program_mode(struct cts_device *cts_dev);
extern int cts_enter_normal_mode(struct cts_device *cts_dev);

extern int cts_probe_device(struct cts_device *cts_dev);
extern void cts_deinit_device(struct cts_device *cts_dev);

extern int cts_get_hwid(struct cts_device *cts_dev, u32 *hwid);

#ifdef CFG_CTS_GESTURE
extern void cts_enable_gesture_wakeup(struct cts_device *cts_dev);
extern void cts_disable_gesture_wakeup(struct cts_device *cts_dev);
extern bool cts_is_gesture_wakeup_enabled(const struct cts_device *cts_dev);
extern int cts_get_gesture_info(struct cts_device *cts_dev, void *gesture_info);
#endif /* CFG_CTS_GESTURE */

extern int cts_set_int_data_types(struct cts_device *cts_dev, u16 types);
extern int cts_set_int_data_method(struct cts_device *cts_dev, u8 method);

#ifdef CONFIG_CTS_ESD_PROTECTION
extern void cts_init_esd_protection(struct chipone_ts_data *cts_data);
extern void cts_enable_esd_protection(struct chipone_ts_data *cts_data);
extern void cts_disable_esd_protection(struct chipone_ts_data *cts_data);
extern void cts_deinit_esd_protection(struct chipone_ts_data *cts_data);
#else /* CONFIG_CTS_ESD_PROTECTION */
static inline void cts_init_esd_protection(struct chipone_ts_data *cts_data)
{
}

static inline void cts_enable_esd_protection(struct chipone_ts_data *cts_data)
{
}

static inline void cts_disable_esd_protection(struct chipone_ts_data *cts_data)
{
}

static inline void cts_deinit_esd_protection(struct chipone_ts_data *cts_data)
{
}
#endif /* CONFIG_CTS_ESD_PROTECTION  */

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
void cts_show_touch_debug_msg(void *debug);
void cts_heartbeat_mechanism_work(struct work_struct *work);
void cts_enable_heartbeat_mechanism(struct chipone_ts_data *cts_data);
void cts_disable_heartbeat_mechanism(struct chipone_ts_data *cts_data);
#endif

#ifdef CONFIG_CTS_GLOVE
extern int cts_enter_glove_mode(struct cts_device *cts_dev);
extern int cts_exit_glove_mode(struct cts_device *cts_dev);
int cts_is_glove_enabled(const struct cts_device *cts_dev);
#else
static inline int cts_enter_glove_mode(struct cts_device *cts_dev)
{
    return 0;
}

static inline int cts_exit_glove_mode(struct cts_device *cts_dev)
{
    return 0;
}

static inline int cts_is_glove_enabled(const struct cts_device *cts_dev)
{
    return 0;
}
#endif

#ifdef CONFIG_CTS_CHARGER_DETECT 
extern bool cts_is_charger_exist(struct cts_device *cts_dev);
extern int cts_set_dev_charger_attached(struct cts_device *cts_dev,
        bool attached);
#else /* CONFIG_CTS_CHARGER_DETECT */
static inline bool cts_is_charger_exist(struct cts_device *cts_dev)
{
    return false;
}

static inline int cts_set_dev_charger_attached(struct cts_device *cts_dev,
        bool attached)
{
    return 0;
}
#endif /* CONFIG_CTS_CHARGER_DETECT */

#ifdef CONFIG_CTS_CHARGER_DETECT_FOR_TRANSSION
extern int cts_set_dev_charger_attached_for_transsion(struct cts_device *cts_dev,
        bool attached);
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
extern bool cts_is_earjack_exist(struct cts_device *cts_dev);
extern int cts_set_dev_earjack_attached(struct cts_device *cts_dev, bool attached);
#else /* CONFIG_CTS_EARJACK_DETECT */
static inline bool cts_is_earjack_exist(struct cts_device *cts_dev)
{
    return false;
}
static inline int cts_set_dev_earjack_attached(struct cts_device *cts_dev, bool attached)
{
    return 0;
}
#endif /* CONFIG_CTS_EARJACK_DETECT */

#ifdef CONFIG_CTS_LEGACY_TOOL
extern int cts_tool_init(struct chipone_ts_data *cts_data);
extern void cts_tool_deinit(struct chipone_ts_data *data);
#else /* CONFIG_CTS_LEGACY_TOOL */
static inline int cts_tool_init(struct chipone_ts_data *cts_data)
{
    return 0;
}

static inline void cts_tool_deinit(struct chipone_ts_data *data)
{
}
#endif /* CONFIG_CTS_LEGACY_TOOL */

extern bool cts_is_device_enabled(const struct cts_device *cts_dev);
extern int cts_start_device(struct cts_device *cts_dev);
extern int cts_stop_device(struct cts_device *cts_dev);

bool is_valid_boot_mode(u8 boot_mode);
extern char *cts_dev_boot_mode2str(const struct cts_device *cts_dev, u8 boot_mode);
extern bool cts_is_fwid_valid(u16 fwid);

extern int cts_reset_device(struct cts_device *cts_dev);

extern int kstrtobool(const char *s, bool *res);

extern struct chipone_ts_data *g_cts_data;

//USD:modify for lcd aod by guoli.niu 20250321 start
extern void set_lcd_aod_mode(u32 state);
//USD:modify for lcd aod by guoli.niu 20250321 end
#endif /* CTS_CORE_H */
