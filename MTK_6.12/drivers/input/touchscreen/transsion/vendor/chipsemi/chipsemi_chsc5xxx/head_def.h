#ifndef __HEAD_DEFINE__
#define __HEAD_DEFINE__
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include "platform.h"

#define LOG_LEVEL_E              KERN_EMERG
#define LOG_LEVEL_D              KERN_EMERG /*KERN_DEBUG*/
#define MODULE_NAME              "CHSC"
#define CHSC_DRIVER_VERSION      "v4.3.4"

#define MAX_CORE_WRITE_LEN       256

#if SEMI_TOUCH_APK_NODE_EN
#define MAX_IO_BUFFER_LEN        1016
#define MAX_TX_RX_BUFF_LEN       (3 * 1024)
#else
#define MAX_IO_BUFFER_LEN        1016
#define MAX_TX_RX_BUFF_LEN       4
#endif

#define READ_BUF_MAX_SIZE        (1024 * 6)
#define CMD_BUFF_MAX             32
#define CMD_TXRX_BUFF_MAX        (1024 * 8)
#define ARRAY_SIZE_MAX           (20 * 42)
#define INHIBIT_BUFF_MAX         40

#define REGION_COUNTS            9
#define REGION_BUFF_MAX          (REGION_COUNTS * 12)

#define MIN_FW_SIZE              1024

#define FP_KEY_VALUE             0x00c3 //Fingerprint key value
#define BIT4                     0x10
extern struct sm_touch_dev st_dev;

#define HEAD "[%s] function = %-30s, line = %-4d: "
#define kernel_log_e(fmt, ...)   printk(LOG_LEVEL_E HEAD fmt, MODULE_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define kernel_log_d(fmt, ...)   printk(LOG_LEVEL_D HEAD fmt, MODULE_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define check_return_if_fail(x, complete)  do{ if(IS_ERR((void*)(long)x)) { kernel_log_e("err code = %ld\r\n", (long)x); if(complete > 0) ((de_init_fun)complete)(); return PTR_ERR((void*)(long)x); }}while(0)
#define check_return_if_zero(x, complete)  do{ if(NULL == (void*)(long)x) { kernel_log_e("err code = %d\r\n", -ENOMEM);  if(complete > 0) ((de_init_fun)complete)(); return -ENOMEM; }}while(0)
#define check_break_if_fail(x, complete)   { if(IS_ERR((void*)(long)x)) { kernel_log_e("err code = %ld\r\n", PTR_ERR((void*)(long)x));  if(complete > 0) ((de_init_fun)complete)(); break; }}

typedef void (*de_init_fun)(void);

#define TP_CMD_BUFF_ADDR             0x20000000
#define TP_RSP_BUFF_ADDR             0x20000000
#define TP_WR_BUFF_ADDR              0x20010000
#define TP_RD_BUFF_ADDR              0x20010400
#define TP_HOLD_MCU_ADDR             0x40008000
#define TP_AUTO_FEED_ADDR            0x4000801c
#define TP_REMAP_MCU_ADDR            0x40008000
#define TP_RELEASE_MCU_ADDR          0x40008000
#define TP_HOLD_MCU_VAL              0x00eb7fdc
#define TP_AUTO_FEED_VAL             0x07ffc25a
#define TP_REMAP_MCU_VAL             0x10eb7fdc
#define TP_RELEASE_MCU_VAL           0x30eb7fdc
#define CFG_ROM_ADDRESS              (200 * 1024)
#define BOOT_ROM_ADDRESS             0X2000
#define VID_PID_BACKUP_ADDR          (248 * 1024 + 0X10)
#define ADC_NUM_MAX                  62

#define BOOT_LOAD_MARK               0x6274


/*ctp work staus*/
#define CTP_POINTING_WORK            0x00000000
#define CTP_READY_UPGRADE            (1 << 1)
#define CPT_UPGRAD_RUNING            (1 << 2)
#define CTP_SUSPEND_GATE             (1 << 16)
#define CTP_GUESTURE_GATE            (1 << 17)
#define CTP_PROXIMITY_GATE           (1 << 18)
#define CTP_GLOVE_GATE               (1 << 19)
#define CTP_ORIENTATION_GATE         (1 << 20)
#define CTP_WET_FINGER_GATE          (1 << 21)
#define CTP_H_V_SW_GATE              (1 << 22)
#define CTP_CHARGER_GATE             (1 << 23)
#define CTP_GAME_MODE_GATE           (1 << 24)
#define CTP_PALM_MODE_GATE           (1 << 25)
#define CTP_GAME_OP_MODE_GATE        (1 << 26)
#define CTP_HIGH_SR_MODE_GATE        (1 << 27)
#define CTP_GAME_TOUCH_INHIBIT_GATE  (1 << 28)
#define CTP_EAA_TOUCH_GATE           (1 << 29)
#define CTP_REGION_CTRL_GATE         (1 << 30)


#define ack_pointing_action(x)       (0 == (x & 0x1ffff))
#define set_status_pointing(x)       do{((x) = CTP_POINTING_WORK); kernel_log_d("set status pointing...\n");}while(0)
#define set_status_ready_upgrade(x)  do{((x) = CTP_READY_UPGRADE); kernel_log_d("set status before reset tp...\n");}while(0)
#define set_status_upgrade_run(x)    do{((x) = CPT_UPGRAD_RUNING); kernel_log_d("set status upgrade running...\n");}while(0)
#define is_status_pointing(x)        (0 == ((x) & 0x07))

#define is_proximity_activate(x)     (((x) & CTP_PROXIMITY_GATE) > 0)
#define enter_proximity_gate(x)      do{((x) |= CTP_PROXIMITY_GATE); kernel_log_d("enter proximity gate...\n");}while(0)
#define leave_proximity_gate(x)      do{((x) &= (~CTP_PROXIMITY_GATE)); kernel_log_d("leave proximity gate...\n");}while(0)

#define is_suspend_activate(x)       (((x) & CTP_SUSPEND_GATE) > 0)
#define enter_suspend_gate(x)        do{((x) |= CTP_SUSPEND_GATE); kernel_log_d("enter suspend gate...\n");}while(0)
#define leave_suspend_gate(x)        do{((x) &= (~CTP_SUSPEND_GATE)); kernel_log_d("leave suspend gate...\n");}while(0)

#define is_guesture_activate(x)      (((x) & CTP_GUESTURE_GATE) > 0)
#define enter_guesture_gate(x)       do{((x) |= CTP_GUESTURE_GATE); kernel_log_d("enter guesture gate...\n");}while(0)
#define leave_guesture_gate(x)       do{((x) &= (~CTP_GUESTURE_GATE)); kernel_log_d("leave guesture gate...\n");}while(0)

#define is_glove_activate(x)         (((x) & CTP_GLOVE_GATE) > 0)
#define enter_glove_gate(x)          do{((x) |= CTP_GLOVE_GATE); kernel_log_d("enter glove gate...\n");}while(0)
#define leave_glove_gate(x)          do{((x) &= (~CTP_GLOVE_GATE)); kernel_log_d("leave glove gate...\n");}while(0)

#define is_orientation_activate(x)   (((x) & CTP_ORIENTATION_GATE) > 0)
#define enter_orientation_gate(x)    do{((x) |= CTP_ORIENTATION_GATE); kernel_log_d("orientation horizontal...\n");}while(0)
#define leave_orientation_gate(x)    do{((x) &= (~CTP_ORIENTATION_GATE)); kernel_log_d("orientation vertical...\n");}while(0)

#define is_wet_finger_activate(x)    (((x) & CTP_WET_FINGER_GATE) > 0)
#define enter_wet_finger_gate(x)     do{((x) |= CTP_WET_FINGER_GATE); kernel_log_d("enter wet finger gate...\n");}while(0)
#define leave_wet_finger_gate(x)     do{((x) &= (~CTP_WET_FINGER_GATE)); kernel_log_d("leave wet finger gate...\n");}while(0)

#define is_h_v_sw_activate(x)        (((x) & CTP_H_V_SW_GATE) > 0)
#define enter_h_v_sw_gate(x)         do{((x) |= CTP_H_V_SW_GATE); kernel_log_d("enter h v sw gate...\n");}while(0)
#define leave_h_v_sw_gate(x)         do{((x) &= (~CTP_H_V_SW_GATE)); kernel_log_d("leave h v sw gate...\n");}while(0)

#define is_charger_activate(x)       (((x) & CTP_CHARGER_GATE) > 0)
#define enter_charger_gate(x)        do{((x) |= CTP_CHARGER_GATE); kernel_log_d("enter charger gate...\n");}while(0)
#define leave_charger_gate(x)        do{((x) &= (~CTP_CHARGER_GATE)); kernel_log_d("leave charger gate...\n");}while(0)

#define is_game_mode_activate(x)     (((x) & CTP_GAME_MODE_GATE) > 0)
#define enter_game_mode_gate(x)      do{((x) |= CTP_GAME_MODE_GATE); kernel_log_d("enter game mode gate...\n");}while(0)
#define leave_game_mode_gate(x)      do{((x) &= (~CTP_GAME_MODE_GATE)); kernel_log_d("leave game mode gate...\n");}while(0)

#define is_palm_mode_activate(x)     (((x) & CTP_PALM_MODE_GATE) > 0)
#define enter_palm_mode_gate(x)      do{((x) |= CTP_PALM_MODE_GATE); kernel_log_d("enter palm mode gate...\n");}while(0)
#define leave_palm_mode_gate(x)      do{((x) &= (~CTP_PALM_MODE_GATE)); kernel_log_d("leave palm mode gate...\n");}while(0)

#define is_game_op_mode_activate(x)  (((x) & CTP_GAME_OP_MODE_GATE) > 0)
#define enter_game_op_mode_gate(x)   do{((x) |= CTP_GAME_OP_MODE_GATE); kernel_log_d("enter game op mode gate...\n");}while(0)
#define leave_game_op_mode_gate(x)   do{((x) &= (~CTP_GAME_OP_MODE_GATE)); kernel_log_d("leave game op mode gate...\n");}while(0)

#define is_high_sr_mode_activate(x)  (((x) & CTP_HIGH_SR_MODE_GATE) > 0)
#define enter_high_sr_mode_gate(x)   do{((x) |= CTP_HIGH_SR_MODE_GATE); kernel_log_d("enter high sr mode gate...\n");}while(0)
#define leave_high_sr_mode_gate(x)   do{((x) &= (~CTP_HIGH_SR_MODE_GATE)); kernel_log_d("leave high sr op mode gate...\n");}while(0)

#define is_game_touch_inhibit_activate(x)    (((x) & CTP_GAME_TOUCH_INHIBIT_GATE) > 0)
#define enter_game_touch_inhibit_gate(x)    do{((x) |= CTP_GAME_TOUCH_INHIBIT_GATE); kernel_log_d("enter game touch inhibit gate...\n");}while(0)
#define leave_game_touch_inhibit_gate(x)    do{((x) &= (~CTP_GAME_TOUCH_INHIBIT_GATE)); kernel_log_d("leave game touch inhibit gate...\n");}while(0)

#define is_eaa_touch_activate(x)     (((x) & CTP_EAA_TOUCH_GATE) > 0)
#define enter_eaa_touch_gate(x)      do{((x) |= CTP_EAA_TOUCH_GATE); kernel_log_d("enter edge anti-accidental touch gate...\n");}while(0)
#define leave_eaa_touch_gate(x)      do{((x) &= (~CTP_EAA_TOUCH_GATE)); kernel_log_d("leave edge anti-accidental touch gate...\n");}while(0)

#define is_region_ctrl_activate(x)    (((x) & CTP_REGION_CTRL_GATE) > 0)
#define enter_region_ctrl_gate(x)     do{((x) |= CTP_REGION_CTRL_GATE); kernel_log_d("enter region ctrl gate...\n");}while(0)
#define leave_region_ctrl_gate(x)     do{((x) &= (~CTP_REGION_CTRL_GATE)); kernel_log_d("leave region ctrl gate...\n");}while(0)

/*ctp function switch*/
//#define SUSPEND_FUNCTION_EN            (1 << 0)
#define GUESTURE_FUNCTION_EN             (1 << 1)
#define PROXIMITY_FUNCTION_EN            (1 << 2)
#define GLOVE_FUNCTION_EN                (1 << 3)
#define ESD_FUNCTION_EN                  (1 << 4)
#define WET_FINGER_FUNCTION_EN           (1 << 5)
#define H_V_SW_FUNCTION_EN               (1 << 6)
#define CHARGER_FUNCTION_EN              (1 << 7)
#define GAME_MODE_FUNCTION_EN            (1 << 8)
#define PALM_MODE_FUNCTION_EN            (1 << 9)
#define GAME_OP_MODE_FUNCTION_EN         (1 << 10)
#define HIGH_SR_FUNCTION_EN              (1 << 11)
#define GAME_TOUCH_INHIBIT_FUNCTION_EN   (1 << 12)
#define EAA_TOUCH_FUNCTION_EN            (1 << 13)
#define REGION_CTRL_FUNCTION_EN          (1 << 14)


#define open_proximity_function(x)   do{((x) |= PROXIMITY_FUNCTION_EN); kernel_log_d("open proximity function...\n");}while(0)
#define close_proximity_function(x)  do{((x) &= (~PROXIMITY_FUNCTION_EN)); kernel_log_d("close proximity function...\n");}while(0)
#define is_proximity_function_en(x)  (((x) & PROXIMITY_FUNCTION_EN) > 0)
#define open_guesture_function(x)    do{((x) |= GUESTURE_FUNCTION_EN); kernel_log_d("open guesture function...\n");}while(0)
#define close_guesture_function(x)   do{((x) &= (~GUESTURE_FUNCTION_EN)); kernel_log_d("close guesture function...\n");}while(0)
#define is_guesture_function_en(x)   (((x) & GUESTURE_FUNCTION_EN) > 0)
#define open_glove_function(x)       do{((x) |= GLOVE_FUNCTION_EN); kernel_log_d("open glove function...\n");}while(0)
#define close_glove_function(x)      do{((x) &= (~GLOVE_FUNCTION_EN)); kernel_log_d("close glove function...\n");}while(0)
#define is_glove_function_en(x)      (((x) & GLOVE_FUNCTION_EN) > 0)
#define open_esd_function(x)         do{((x) |= ESD_FUNCTION_EN); kernel_log_d("open esd function...\n");}while(0)
#define close_esd_function(x)        do{((x) &= (~ESD_FUNCTION_EN)); kernel_log_d("close esd function...\n");}while(0)
#define is_esd_function_en(x)        (((x) & ESD_FUNCTION_EN) > 0)
#define open_wet_finger_function(x)  do{((x) |= WET_FINGER_FUNCTION_EN); kernel_log_d("open wet finger function...\n");}while(0)
#define close_wet_finger_function(x) do{((x) &= (~WET_FINGER_FUNCTION_EN)); kernel_log_d("close wet finger function...\n");}while(0)
#define is_wet_finger_function_en(x) (((x) & WET_FINGER_FUNCTION_EN) > 0)
#define open_h_v_sw_function(x)      do{((x) |= H_V_SW_FUNCTION_EN); kernel_log_d("open h v sw function...\n");}while(0)
#define close_h_v_sw_function(x)     do{((x) &= (~H_V_SW_FUNCTION_EN)); kernel_log_d("close h v sw function...\n");}while(0)
#define is_h_v_sw_function_en(x)     (((x) &  H_V_SW_FUNCTION_EN) > 0)
#define open_charger_function(x)     do{((x) |= CHARGER_FUNCTION_EN); kernel_log_d("open charger function...\n");}while(0)
#define close_charger_function(x)    do{((x) &= (~CHARGER_FUNCTION_EN)); kernel_log_d("close charger function...\n");}while(0)
#define is_charger_function_en(x)    (((x) & CHARGER_FUNCTION_EN) > 0)
#define open_game_mode_function(x)   do{((x) |= GAME_MODE_FUNCTION_EN); kernel_log_d("open game mode function...\n");}while(0)
#define close_game_mode_function(x)  do{((x) &= (~GAME_MODE_FUNCTION_EN)); kernel_log_d("close game mode function...\n");}while(0)
#define is_game_mode_function_en(x)  (((x) & GAME_MODE_FUNCTION_EN) > 0)
#define open_palm_mode_function(x)   do{((x) |= PALM_MODE_FUNCTION_EN); kernel_log_d("open palm mode function...\n");}while(0)
#define close_palm_mode_function(x)  do{((x) &= (~PALM_MODE_FUNCTION_EN)); kernel_log_d("close palm mode function...\n");}while(0)
#define is_palm_mode_function_en(x)  (((x) & PALM_MODE_FUNCTION_EN) > 0)
#define open_game_op_mode_function(x)   do{((x) |= GAME_OP_MODE_FUNCTION_EN); kernel_log_d("open game op mode function...\n");}while(0)
#define close_game_op_mode_function(x)  do{((x) &= (~GAME_OP_MODE_FUNCTION_EN)); kernel_log_d("close game op mode function...\n");}while(0)
#define is_game_op_mode_function_en(x)  (((x) & GAME_OP_MODE_FUNCTION_EN) > 0)
#define open_high_sr_mode_function(x)   do{((x) |= HIGH_SR_FUNCTION_EN); kernel_log_d("open high sr mode function...\n");}while(0)
#define close_high_sr_mode_function(x)  do{((x) &= (~HIGH_SR_FUNCTION_EN)); kernel_log_d("close high sr mode function...\n");}while(0)
#define is_high_sr_mode_function_en(x)  (((x) & HIGH_SR_FUNCTION_EN) > 0)
#define open_game_touch_inhibit_function(x)   do{((x) |= GAME_TOUCH_INHIBIT_FUNCTION_EN); kernel_log_d("open game touch inhibit function...\n");}while(0)
#define close_game_touch_inhibit_function(x)  do{((x) &= (~GAME_TOUCH_INHIBIT_FUNCTION_EN)); kernel_log_d("close game touch inhibit function...\n");}while(0)
#define is_game_touch_inhibit_function_en(x)  (((x) & GAME_TOUCH_INHIBIT_FUNCTION_EN) > 0)
#define open_eaa_touch_function(x)   do{((x) |= EAA_TOUCH_FUNCTION_EN); kernel_log_d("open eaa touch function...\n");}while(0)
#define close_eaa_touch_function(x)  do{((x) &= (~EAA_TOUCH_FUNCTION_EN)); kernel_log_d("close eaa touch function...\n");}while(0)
#define is_eaa_touch_function_en(x)  (((x) & EAA_TOUCH_FUNCTION_EN) > 0)
#define open_region_ctrl_function(x)   do{((x) |= REGION_CTRL_FUNCTION_EN); kernel_log_d("open region ctrl function...\n");}while(0)
#define close_region_ctrl_function(x)  do{((x) &= (~REGION_CTRL_FUNCTION_EN)); kernel_log_d("close region ctrl function...\n");}while(0)
#define is_region_ctrl_function_en(x)  (((x) & REGION_CTRL_FUNCTION_EN) > 0)

enum reset_action { no_report_after_reset = 0, do_report_after_reset = 1 };
enum startup_action { only_sp_check = 0, check_backup_if_fail = 1 };

enum SEMI_DRV_ERR
{
    SEMI_DRV_ERR_OK = 0,
    SEMI_DRV_ERR_HAL_IO,
    SEMI_DRV_ERR_NO_INIT,
    SEMI_DRV_ERR_TIMEOUT,
    SEMI_DRV_ERR_CHECKSUM,
    SEMI_DRV_ERR_RESPONSE,
    SEMI_DRV_INVALID_CMD,
    SEMI_DRV_INVALID_PARAM,
    SEMI_DRV_ERR_NOT_MATCH,
};

enum CMD_TYPE_ID
{
    CMD_NA              = 0x0f,
    CMD_IDENTITY        = 0x01,
    CMD_CTP_SSCAN       = 0x02,
    CMD_CTP_IOCTL       = 0x03,
    CMD_CTP_CMD         = 0x04,
    CMD_CTP_RST         = 0x10,
    CMD_SHORT_TST       = 0x21,
    CMD_CHK_CFG         = 0x26,
    CMD_DATA_SYNC       = 0x28,
    CMD_MEM_WR          = 0x30,
    CMD_MEM_RD          = 0x31,
    CMD_BSPR_RW         = 0x37,
    CMD_IRQ_IOCTL       = 0xE0,
};

struct hal_io_packet
{
    unsigned int   io_register;
    unsigned char  io_buffer[MAX_IO_BUFFER_LEN];
    unsigned short io_length;
    void*          hal_adapter;
};

struct semi_touch_init_d
{
    unsigned int rawdata_addr;
    unsigned int differ_addr;
    unsigned int base_addr;
    unsigned int touch_addr;
    unsigned int thp_addr;
    bool initialize_ok;
    bool dog_feed_flag;
    unsigned int ctp_run_status;
    unsigned int custom_function_en;
    int vkey_num;
    int vkey_evt_arr[MAX_VKEY_NUMBER];
    int vkey_dim_map[MAX_VKEY_NUMBER][4];
};

struct chsc_updfile_header
{
    unsigned int sig;
    unsigned int extend;
    unsigned int n_cfg;
    unsigned int match;
    unsigned int len_cfg;
    unsigned int len_boot;
    unsigned long long vlist[1];
};

#define VID_PID_ONE_SPACE_LEN   sizeof(unsigned long long)
typedef int (*hal_write_bytes)(struct hal_io_packet *ppacket);
typedef int (*hal_read_bytes)(struct hal_io_packet *ppacket);
struct hal_io_function
{
    hal_write_bytes hal_write_fun;
    hal_read_bytes  hal_read_fun;
    struct mutex    bus_lock;
    void* hal_param;
};

#define ASYN_WORK_MAX      3
#define typename(x)        #x
enum work_queue_t { work_queue_interrupt, work_queue_custom_work, work_queue_max };

struct work_wraper
{
    unsigned long uid;
    struct delayed_work  work;
};

struct work_quene_wraper
{
    int new_work_idx;
    struct workqueue_struct *work_queue[work_queue_max];
    struct work_wraper works_list[ASYN_WORK_MAX];
};

#if HAL_INTERFACE_I2C == HAL_INTERFACE_TYPE
#define  hal_device i2c_client
#define  hal_driver i2c_driver
#define  BUS_HAL  BUS_I2C
#define  hal_device_id i2c_device_id
#define  hal_register_driver i2c_add_driver
#define  hal_unregister_driver i2c_del_driver
#define  HAL_RD_ADDR_TAG   0X00
#else
#define  hal_device spi_device
#define  hal_driver spi_driver
#define  BUS_HAL BUS_SPI
#define  hal_device_id spi_device_id
#define  hal_register_driver spi_register_driver
#define  hal_unregister_driver spi_unregister_driver
#define  HAL_RD_ADDR_TAG   0X02
#endif

typedef struct sm_touch_dev
{
    struct device_node *nd;
    struct input_dev *input;
    //struct i2c_client *client;
    struct hal_device *client;
    struct semi_touch_init_d stc;
    struct hal_io_function hal;
    void   *chsc_nodes_dir;
    struct proc_dir_entry *proc_entry;
    struct work_quene_wraper asyn_work;
    struct mutex custom_lock;
    int int_pin;
    int rst_pin;
    int vdd_pin;
    int vddio_pin;
    int cmd_flag;
    int gest_flag;
    unsigned int max_x;
    unsigned int max_y;
    /*USD:modify for transsion core by yun.luo 20241225 start*/
    u32 solution_x;
    u32 solution_y;
    u32 multiple_solution;
    bool fod_func_support;
    bool switch_report_support;
    bool pocket_func_support;
    //USD: add for tp feature by jiafeng.fan 20250905 start
    bool custom_mistouch_support;
    //USD: add for tp feature by jiafeng.fan 20250905 end
    int tran_fingerprint_status;
    unsigned char report_rate_switch;
    /*USD:modify for transsion core by yun.luo 20241225 end*/
#if SEMI_TOUCH_FACTORY_TEST_EN
    unsigned int rawdata_row;
    unsigned int rawdata_col;
    unsigned int short_limits;
    unsigned int invalid_row_col[8];
    short rawdata_min[ARRAY_SIZE_MAX];
    short rawdata_max[ARRAY_SIZE_MAX];
#endif
    unsigned char tran_inhibit_data[INHIBIT_BUFF_MAX];
    unsigned char region_ctrl_data[REGION_BUFF_MAX];
    unsigned short cmd_buff[CMD_BUFF_MAX];
    unsigned short cmd_txrx_buff[CMD_TXRX_BUFF_MAX];
    unsigned short fw_ver;
    /*BSP: add for firmware version by longtao.ran 20240416 start*/
    unsigned short upd_boot_ver;
    unsigned long long upd_vid_pid;
    /*BSP: add for firmware version by longtao.ran 20240416 end*/
    unsigned long long vid_pid;    //0xVID_PID_CFGVER
    //int irq_no;
    void *pv;
} sm_touch_dev, *psm_touch_dev;

//cammand struct for mcap
struct m_ctp_cmd_std_t
{
    unsigned short chk; // 16 bit checksum
    unsigned short d0;  //data 0
    unsigned short d1;  //data 1
    unsigned short d2;  //data 2
    unsigned short d3;  //data 3
    unsigned short d4;  //data 4
    unsigned short d5;  //data 5

    unsigned char  id;   //offset 15
    unsigned char  tag;  //offset 16
};

//response struct for mcap
struct m_ctp_rsp_std_t
{
    unsigned short chk; // 16 bit checksum
    unsigned short d0;  //data 0
    unsigned short d1;  //data 1
    unsigned short d2;  //data 2
    unsigned short d3;  //data 3
    unsigned short d4;  //data 4
    unsigned short d5;  //data 5

    unsigned char  cc;  //offset 15
    unsigned char  id;  //offset 16
};

struct sync_context
{
    atomic_t atomic_sync_flag;

    unsigned int sync_addr;
    unsigned short sync_size;
};

struct apk_complex_data
{
    unsigned char stm_cmd_buffer[16];
    unsigned char stm_rsp_buffer[16];
    unsigned char stm_rdy_buffer[4];
    unsigned char stm_ctp_buffer[4];
    unsigned char stm_txrx_buffer[MAX_TX_RX_BUFF_LEN];
    unsigned short op_type;
    unsigned long  op_args;
    struct sync_context sync;
};

union rpt_point_t
{
    struct
    {
        unsigned char x_l8;
        unsigned char x_h8;
        unsigned char y_l8;
        unsigned char y_h8;
        unsigned char z;
        unsigned char id: 4;
        unsigned char event: 4;
    } rp;
    unsigned char data[6];
};

typedef struct _rpt_flash_t
{
    unsigned char act;
    unsigned char num;
    unsigned char cmd;
    unsigned char cmdNeg;
    unsigned short d0;
    unsigned short d1;
    unsigned short d2;
    unsigned short d3;
    unsigned short d4;
    unsigned short d5;
} rpt_flash_t;

typedef struct _rpt_content_t
{
    unsigned char act;
    unsigned char num;
    union rpt_point_t points[15];
} rpt_content_t;

typedef struct _rpt_content_sub
{
    unsigned short flag;
    unsigned short fingerprintArea;
    unsigned short reserve2;
    unsigned short checksum2;
} rpt_content_sub;

typedef struct _img_header_t
{
    unsigned short fw_ver;
    unsigned short resv;
    unsigned int sig;
    // unsigned int vid_pid;
    unsigned int resv2;
    unsigned short raw_offet;
    unsigned short dif_offet;
} img_header_t;

struct bin_code_chain
{
    const unsigned char *bin_code_addr;
    unsigned int bin_code_len;
    struct bin_code_chain *next;
};

/********************************************/
typedef enum
{
    eTYPE_HOTZONE_OFF = 0,
    eTYPE_NONHOTZONE_PARAMS,
    eTYPE_MINI_MAP_AREA,
    eTYPE_DIRECTION_PAD_AREA,
    eTYPE_SKILL_RELEASE_AREA,
    eTYPE_QUICK_TAP_AREA,
    eTYPE_FOCUS_VIEW_AREA
} eAREA_TYPE_CONFIG;

typedef struct _ScreenRegionConfig
{
    unsigned char area_type;
    unsigned char sensitivity;
    unsigned char responsiveness;
    unsigned char lockingThreshold;
    unsigned short x_start;
    unsigned short y_start;
    unsigned short x_end;
    unsigned short y_end;
} filter_game30_smooth;

typedef struct _tran_region_core
{
    filter_game30_smooth region_s[REGION_COUNTS];
}tran_region_core;

typedef struct TP_Region_data
{
    unsigned short flag;
    unsigned short areaEn;
    filter_game30_smooth func[REGION_COUNTS];
    unsigned short checksum;
}filter_game30_func;

/********************************************/
#endif //__HEAD_DEFINE__
