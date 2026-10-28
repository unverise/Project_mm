/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*****************************************************************************
*
* File Name: focaltech_core.h

* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

#ifndef __LINUX_FOCALTECH_CORE_H__
#define __LINUX_FOCALTECH_CORE_H__
/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>
#include "focaltech_common.h"

/**********transsion***********/
#include "../../../core/include/transsion_core.h"
#include "../../../core/include/transsion_node.h"
#include "../../../core/include/transsion_gesture.h"
/************transsion********/

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_NAME                     "mtk-tpd"

/*BSP:add for third provide tp bring up by longtao.ran 20231106 start */
#define FTS_INI_NAME_PREX_WITH_REQUEST      "Conf_MultipleTest"
/*BSP:add for third provide tp bring up by longtao.ran 20231106 end */

#define FTS_MAX_POINTS_SUPPORT              10 /* constant value, can't be changed */
#define FTS_MAX_KEYS                        4
#define FTS_KEY_DIM                         10
#define FTS_COORDS_ARR_SIZE                 4
#define FTS_ONE_TCH_LEN                     6
#define FTS_ONE_TCH_LEN_V2                  8
#define FTS_TOUCH_DATA_LEN_V2  (FTS_MAX_POINTS_SUPPORT * FTS_ONE_TCH_LEN_V2 + 4)


#define FTS_TOUCH_DATA_LEN  (FTS_MAX_POINTS_SUPPORT * FTS_ONE_TCH_LEN + 2)

#define FTS_GESTURE_POINTS_MAX              6
#define FTS_GESTURE_DATA_LEN               (FTS_GESTURE_POINTS_MAX * 4 + 4)

#define FTS_SIZE_PEN                        15
#define FTS_SIZE_DEFAULT                    15
#define FTS_SIZE_DEFAULT_V2                 21


#define FTS_MAX_ID                          0x0A
#define FTS_TOUCH_OFF_E_XH                  0
#define FTS_TOUCH_OFF_XL                    1
#define FTS_TOUCH_OFF_ID_YH                 2
#define FTS_TOUCH_OFF_YL                    3
#define FTS_TOUCH_OFF_PRE                   4
#define FTS_TOUCH_OFF_AREA                  5
#define FTS_TOUCH_OFF_MINOR                 6

#define FTS_TOUCH_E_NUM                     1
#define FTS_X_MIN_DISPLAY_DEFAULT           0
#define FTS_Y_MIN_DISPLAY_DEFAULT           0
#define FTS_X_MAX_DISPLAY_DEFAULT           (720 - 1)
#define FTS_Y_MAX_DISPLAY_DEFAULT           (1280 - 1)

#define FTS_TOUCH_DOWN                      0
#define FTS_TOUCH_UP                        1
#define FTS_TOUCH_CONTACT                   2
#define EVENT_DOWN(flag)                    ((FTS_TOUCH_DOWN == flag) || (FTS_TOUCH_CONTACT == flag))
#define EVENT_UP(flag)                      (FTS_TOUCH_UP == flag)

#define FTS_MAX_COMPATIBLE_TYPE             8
#define FTS_MAX_COMMMAND_LENGTH             16

#define FTS_MAX_TOUCH_BUF                   4096
#define FTS_MAX_BUS_BUF                     4096



/*****************************************************************************
*  Alternative mode (When something goes wrong, the modules may be able to solve the problem.)
*****************************************************************************/
/*
 * For commnication error in PM(deep sleep) state
 */
//BSP: add CONFIG_TRAN_FTS_PATCH_COMERR_PM config by tuo.wang 20240416 start
#ifdef CONFIG_TRAN_FTS_PATCH_COMERR_PM
#define FTS_PATCH_COMERR_PM                 1
#else
#define FTS_PATCH_COMERR_PM                 1
#endif
//BSP: add CONFIG_TRAN_FTS_PATCH_COMERR_PM config by tuo.wang 20240416 end
#define FTS_TIMEOUT_COMERR_PM               700

/*
 * For high resolution
 * Set FTS_TOUCH_HIRES_EN to 1 to support high resolution reporting of touch finger.
 * Set FTS_PEN_HIRES_EN to 1 to support high resolution reporting of stylus pen.
 *
 * FTS_XXX_HIRES_X, a multiple relative to the original resolution
 * FTS_HI_RES_X_MAX, const value, can't be modified
 */
//BSP: add eightfold resolution by longtao.ran 20230920 start
#define FTS_TOUCH_HIRES_EN                  1
#define FTS_TOUCH_HIRES_X                   8
//BSP: add eightfold resolution by longtao.ran 20230920 end

#define FTS_PEN_HIRES_EN                    1
#define FTS_PEN_HIRES_X                     10


#define FTS_HI_RES_X_MAX                    16


/*USD: add for pocket anti accidental touch by kunrui.li 20251010 start*/
#ifndef KEY_LARGE_AREA
#define KEY_LARGE_AREA      251
#endif
#define FTS_POCKET_SWITCH                                          0x9A
#define FTS_POCKET_MODE                                            0x9B
#define FTS_POCKET_OPEN                                            0x05
#define FTS_POCKET_CLOSE                                           0x00
/*USD: add for pocket anti accidental touch by kunrui.li 20251010 end*/


/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
struct ftxxxx_proc {
    struct proc_dir_entry *proc_entry;
    u8 opmode;
    u8 cmd_len;
    u8 cmd[FTS_MAX_COMMMAND_LENGTH];
};

struct fts_ts_platform_data {
    u32 irq_gpio;
    u32 irq_gpio_flags;
    u32 reset_gpio;
    u32 reset_gpio_flags;
    bool have_key;
    bool fw_three_in_one;
    bool lcd_call_fw;//for lcd call tp fw update
    bool is_aod_mode;//for lcd aod function
    bool pocket_func_support;
    bool insert_rate_support;//add for tp report rate
    u32 key_number;
    u32 keys[FTS_MAX_KEYS];
    u32 key_y_coords[FTS_MAX_KEYS];
    u32 key_x_coords[FTS_MAX_KEYS];
    u32 x_max;
    u32 y_max;
    u32 x_min;
    u32 y_min;
    u32 max_touch_number;
};

struct ts_event {
    int x;      /*x coordinate */
    int y;      /*y coordinate */
    int p;      /* pressure */
    int flag;   /* touch event flag: 0 -- down; 1-- up; 2 -- contact */
    int id;     /*touch ID */
    int area;
    int minor;
};

struct pen_event {
    int down;
    int inrange;
    int tip;
    int x;      /*x coordinate */
    int y;      /*y coordinate */
    int p;      /* pressure */
    int flag;   /* touch event flag: 0 -- down; 1-- up; 2 -- contact */
    int id;     /*touch ID */
    int tilt_x;
    int tilt_y;
    int azimuth;
    int tool_type;
};

struct fts_ts_data {
    struct i2c_client *client;
    struct spi_device *spi;
    struct device *dev;
    struct input_dev *input_dev;
    struct input_dev *pen_dev;
    struct fts_ts_platform_data *pdata;
    struct ts_ic_info ic_info;
    struct workqueue_struct *ts_workqueue;
    struct work_struct fwres_work;//luoyun
    struct completion fw_comp;
    struct work_struct fwupg_work;
    //BSP: modify LHQNHB-2423 by hongwu.zhan 20230321 start
    struct work_struct ini_work;
    //BSP: modify LHQNHB-2423 by hongwu.zhan 20230321 end
    struct delayed_work esdcheck_work;
    //USD:add patch for insert func by junyu.chen2 20251225 start
    struct work_struct recovery_work;
    //USD:add patch for insert func by junyu.chen2 20251225 end
    struct delayed_work prc_work;
    struct work_struct resume_work;
    wait_queue_head_t ts_waitqueue;
    struct ftxxxx_proc proc;
    struct ftxxxx_proc proc_ta;
    spinlock_t irq_lock;
    struct mutex report_mutex;
    struct mutex bus_lock;
    unsigned long intr_jiffies;
    int irq;
    int log_level;
    int fw_is_running;      /* confirm fw is running when using spi:default 0 */
    int dummy_byte;
#if IS_ENABLED(CONFIG_PM) && FTS_PATCH_COMERR_PM
    struct completion pm_completion;
    bool pm_suspend;
#endif
    bool suspended;
    bool fw_loading;
    bool irq_disabled;
    bool power_disabled;
    bool glove_mode;
    bool cover_mode;
    bool charger_mode;
    bool touch_analysis_support;
    bool prc_support;
    bool prc_mode;
    bool esd_support;

    bool gesture_support;   /* gesture enable or disable, default: disable */
    u8 gesture_bmode;       /*gesture buffer mode*/

    u8 pen_etype;
    struct pen_event pevent;
    struct ts_event events[FTS_MAX_POINTS_SUPPORT];    /* multi-touch */
    u8 touch_addr;
    u32 touch_size;
    u8 *touch_buf;
    int touch_event_num;
    int touch_points;
    int key_state;
    int ta_flag;
    u32 ta_size;
    u8 *ta_buf;
    bool palmflag;
    
    u8 *bus_tx_buf;
    u8 *bus_rx_buf;
    int bus_type;
    int bus_ver;
    struct regulator *vdd;
    struct regulator *vcc_i2c;
#if FTS_PINCTRL_EN
    struct pinctrl *pinctrl;
    struct pinctrl_state *pins_active;
    struct pinctrl_state *pins_suspend;
    struct pinctrl_state *pins_release;
#endif
    struct notifier_block fb_notif;
    //BSP: add by hongwu.zhan 20230823 start
    u8 bv;
    unsigned char g_tran_switch;
    const struct firmware *fw;
    u8 shutdown_flag;
    //BSP: add by hongwu.zhan 20230823 end
    //USD: add for ESport1.1 by yanjun.wang 20260210 start
    u8 tran_esprots_switch;
    u8 tran_report_rate_value;
    u8 tran_lock_point_value;
    u8 tran_transient_report_rate_value;
    u8 tran_micro_control_accuracy;
    //USD: add for ESport1.1 by yanjun.wang 20260210 end
    //USD:add patch for insert func by junyu.chen2 20251225 start
    struct hrtimer timer;
    int int_cnt[10];
    int report_rate;
    bool insert_flag;
    spinlock_t insert_spin_lock;
    //USD:add patch for insert func by junyu.chen2 20251225 start
};


enum _FTS_BUS_TYPE {
    BUS_TYPE_NONE,
    BUS_TYPE_I2C,
    BUS_TYPE_SPI,
};

enum _FTS_BUS_VER {
    BUS_VER_DEFAULT = 1,
    BUS_VER_V2,
};

enum _FTS_TOUCH_ETYPE {
    TOUCH_DEFAULT = 0x00,
    TOUCH_PROTOCOL_v2 = 0x02,
    TOUCH_EXTRA_MSG = 0x08,
    TOUCH_PEN = 0x0B,
    TOUCH_GESTURE = 0x80,
    TOUCH_FW_INIT = 0x81,
    TOUCH_DEFAULT_HI_RES = 0x82,
    TOUCH_IGNORE = 0xFE,
    TOUCH_ERROR = 0xFF,
};

enum _FTS_STYLUS_ETYPE {
    STYLUS_DEFAULT,
    STYLUS_HOVER,
};

enum _FTS_GESTURE_BMODE {
    GESTURE_BM_REG,
    GESTURE_BM_TOUCH,
};

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
extern struct fts_ts_data *fts_data;

/* communication interface */
int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen);
int fts_read_reg(u8 addr, u8 *value);
int fts_write(u8 *writebuf, u32 writelen);
int fts_write_reg(u8 addr, u8 value);
int fts_bus_configure(struct fts_ts_data *ts_data, u8 *buf, u32 size);
int fts_bus_transfer_direct(u8 *writebuf, u32 writelen, u8 *readbuf, u32 readlen);
void fts_hid2std(void);
int fts_ts_probe_entry(struct fts_ts_data *ts_data);
int fts_ts_remove_entry(struct fts_ts_data *ts_data);

/* Gesture functions */
int fts_gesture_init(struct fts_ts_data *ts_data);
int fts_gesture_exit(struct fts_ts_data *ts_data);
void fts_gesture_recovery(struct fts_ts_data *ts_data);
int fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *data);
int fts_gesture_suspend(struct fts_ts_data *ts_data);
int fts_gesture_resume(struct fts_ts_data *ts_data);

/* Apk and functions */
int fts_create_apk_debug_channel(struct fts_ts_data *);
void fts_release_apk_debug_channel(struct fts_ts_data *);

/* ADB functions */
int fts_create_sysfs(struct fts_ts_data *ts_data);
int fts_remove_sysfs(struct fts_ts_data *ts_data);

/* ESD */
int fts_esdcheck_init(struct fts_ts_data *ts_data);
int fts_esdcheck_exit(struct fts_ts_data *ts_data);
void fts_esdcheck_switch(struct fts_ts_data *ts_data, bool enable);
void fts_esdcheck_proc_busy(struct fts_ts_data *ts_data, bool proc_debug);
void fts_esdcheck_suspend(struct fts_ts_data *ts_data);
void fts_esdcheck_resume(struct fts_ts_data *ts_data);
bool fts_esd_is_disable(void);


/* Host test */
int fts_test_init(struct fts_ts_data *ts_data);
int fts_test_exit(struct fts_ts_data *ts_data);

/* Point Report Check*/
int fts_point_report_check_init(struct fts_ts_data *ts_data);
int fts_point_report_check_exit(struct fts_ts_data *ts_data);
void fts_prc_queue_work(struct fts_ts_data *ts_data);

/* FW upgrade */
int fts_fwupg_init(struct fts_ts_data *ts_data);
int fts_fwupg_exit(struct fts_ts_data *ts_data);
int fts_fw_recovery(void);
int fts_upgrade_bin(char *fw_name, bool force);
int fts_enter_test_environment(bool test_state);
int fts_enter_gesture_fw(void);
int fts_enter_normal_fw(void);


/* Other */
int fts_reset_proc(int hdelayms);
int fts_check_cid(struct fts_ts_data *ts_data, u8 id_h);
int fts_wait_tp_to_valid(void);
void fts_release_all_finger(void);
void fts_tp_state_recovery(struct fts_ts_data *ts_data);
int fts_ex_mode_init(struct fts_ts_data *ts_data);
int fts_ex_mode_exit(struct fts_ts_data *ts_data);
int fts_ex_mode_recovery(struct fts_ts_data *ts_data);

void fts_irq_disable(void);
void fts_irq_enable(void);

#if FTS_PSENSOR_EN
int fts_proximity_init(void);
int fts_proximity_exit(void);
int fts_proximity_readdata(struct fts_ts_data *ts_data);
int fts_proximity_suspend(void);
int fts_proximity_resume(void);
int fts_proximity_recovery(struct fts_ts_data *ts_data);
#endif

/***********************transsion*********************/
#define GESTURE_LEFT                            0x20
#define GESTURE_RIGHT                           0x21
#define GESTURE_UP                              0x22
#define GESTURE_DOWN                            0x23
#define GESTURE_DOUBLECLICK                     0x24
#define GESTURE_O                               0x30
#define GESTURE_W                               0x31
#define GESTURE_M                               0x32
#define GESTURE_E                               0x33
#define GESTURE_L                               0x44
#define GESTURE_S                               0x46
#define GESTURE_V                               0x54
#define GESTURE_Z                               0x65
#define GESTURE_C                               0x34
//USD:modify for lcd aod by yun.luo 20240627 start
#define GESTURE_ONECLICK                        0x25
//USD:modify for lcd aod by yun.luo 20240627 end
int get_tpd_ops(struct tran_ts_controller *controller, struct tran_core_ops **ops);
extern struct tran_ts_controller fts_noflash_controller;

extern int32_t fts_set_edge_reject_switch(u8 edge_reject_switch);
extern int fts_ts_suspend(struct device *dev);
extern int fts_ts_resume(struct device *dev);
extern int tran_fts_get_SelftestResult(void);

/***********************transsion*********************/

#endif /* __LINUX_FOCALTECH_CORE_H__ */
