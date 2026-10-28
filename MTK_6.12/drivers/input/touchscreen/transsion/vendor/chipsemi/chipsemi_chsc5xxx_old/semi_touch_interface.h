#ifndef __SEMI_TOUCH_INTERFACE_H__
#define __SEMI_TOUCH_INTERFACE_H__
#include "head_def.h"
#include "fw_code_bin.h"
#include "semi_touch_thp_dev.h"
/**************************************************************************************************/
/*basic util interface*/
/**************************************************************************************************/
unsigned short caculate_checksum_u16(unsigned short * buf, unsigned int length);
unsigned short caculate_checksum_u816(unsigned char * buf, unsigned int length);
unsigned int caculate_check_sum_u32(unsigned int * buf, unsigned int length);
unsigned int caculate_checksum_ex(unsigned char * buf, unsigned int length);


/**************************************************************************************************/
/*i2c_communication interface*/
/**************************************************************************************************/
int hall_write_bytes(struct hal_io_packet* ppacket);
int hall_read_bytes(struct hal_io_packet* ppacket);
int semi_touch_hall_init(void* hal);
int semi_touch_hall_init_buff(struct sm_touch_dev *st_dev);
int semi_touch_hall_exit(void);

/**************************************************************************************************/
/*semi_touch_function interface*/
/**************************************************************************************************/
#define semi_touch_watch_dog_feed(watch_dog_feed)            (watch_dog_feed = true)
#define semi_touch_check_watch_dog_feed(watch_dog_feed)      (watch_dog_feed == true)
#define semi_touch_reset_watch_dog(watch_dog_feed)           (watch_dog_feed = false)
extern unsigned char H_V_CODE;

struct _frame_cnt
{
    unsigned short frame_cnt;
    unsigned short frame_cnt_check;
};

extern unsigned short G_GESTURE_MARK;
extern unsigned short G_GESTURE_BACK;
extern unsigned int G_GAME_BACK;
int semi_touch_reset(enum reset_action action);
int semi_touch_device_prob(void);
int semi_touch_reset_and_detect(void);
int semi_touch_start_up_check(unsigned char* checkOK, unsigned char opt);
int semi_touch_heart_beat(void);
int semi_touch_write_bytes(unsigned int reg, const unsigned char* buffer, unsigned short len);
int semi_touch_read_bytes(unsigned int reg, unsigned char* buffer, unsigned short len);
int cmd_send_to_tp(struct m_ctp_cmd_std_t *ptr_cmd, struct m_ctp_rsp_std_t *ptr_rsp, const int delay);
int cmd_send_to_tp_step_100us(struct m_ctp_cmd_std_t *ptr_cmd, struct m_ctp_rsp_std_t *ptr_rsp, const int delay_us);
int cmd_send_to_tp_no_check(struct m_ctp_cmd_std_t *ptr_cmd);
int read_and_report_touch_points(unsigned char *readbuffer, unsigned short len);
int semi_touch_mode_init(struct sm_touch_dev *st_dev);
int semi_touch_suspend_ctrl(unsigned char en);
int semi_touch_glove_switch(unsigned char en);
int semi_touch_guesture_switch(unsigned short mark, unsigned char en);
int semi_touch_proximity_switch(unsigned char en);
int semi_touch_orientation_switch(unsigned char en);
int semi_touch_wet_finger_switch(unsigned char en);
int semi_touch_h_v_switch(unsigned char code, unsigned char en);
int semi_touch_charger_switch(unsigned char en);
int semi_touch_game_mode_switch(unsigned int mark, unsigned char en);
int semi_touch_palm_mode_switch(unsigned char en);
int semi_touch_queue_asyn_work(enum work_queue_t queue_type, work_func_t work_func, int ms);
int semi_touch_create_work_queue(enum work_queue_t queue_type, const char* queue_name);
int semi_touch_destroy_work_queue(void);
s64 semi_touch_get_rtime(void);
int semi_touch_send_password_rtime(void);
int semi_touch_send_rtime(void);
int semi_touch_send_value(unsigned short num_index);
int semi_touch_send_ioctrl(unsigned char index);
int semi_touch_send_cmd_status(unsigned short cmd, unsigned short index,
                               unsigned short num, unsigned short checksum, s64 time);

/**************************************************************************************************/
/*semi_touch_raw interface   "/proc/sm_touch_cmd" */
/**************************************************************************************************/
#define SEMI_TOUCH_PROC_NAME_RAW            "sm_thp_cmd"


/**************************************************************************************************/
/*semi_touch_raw interface "/dev/sm_touch"  */
/**************************************************************************************************/
#define DEV_NAME                            "sm_thp_raw"
/**************************************************************************************************/
/*semi_touch_apk interface*/
/**************************************************************************************************/
#define SEMI_TOUCH_PROC_NAME              "semi_touch_debug"
int semi_touch_create_apk_proc(struct sm_touch_dev* st_dev);
int semi_touch_remove_apk_proc(struct sm_touch_dev* st_dev);


/**************************************************************************************************/
/*semi_touch_upgrade interface*/
/**************************************************************************************************/
int semi_touch_get_backup_pid(unsigned long long *id);
int semi_touch_run_ram_code(unsigned char code);
int semi_touch_memory_write(struct apk_complex_data* apk_comlex_addr);
int semi_touch_memory_read(struct apk_complex_data* apk_comlex_addr);
int semi_touch_bootup_update_check(void);
int semi_touch_online_update_check(char* file_path);
int semi_touch_fw_update_check(struct sm_touch_dev *st_dev);


/**************************************************************************************************/
/*semi_touch_device interface*/
/**************************************************************************************************/
int semi_touch_init(void *hal);
int semi_touch_deinit(void *hal);
irqreturn_t semi_touch_clear_report(void);


/**************************************************************************************************/
/*semi_touch_custom interface*/
/**************************************************************************************************/
#define MAKE_NODE_UNDER_PROC                  0
#define MAKE_NDDE_UNDER_SYS                   1
#define SEMI_TOUCH_PROC_DIR                   "touchscreen"
#define SEMI_TOUCH_MAKE_NODES_DIR             MAKE_NODE_UNDER_PROC
int semi_touch_custom_work(struct sm_touch_dev *st_dev);
int semi_touch_custom_clean_up(void);
int semi_touch_wake_lock(void);
bool semi_touch_gesture_report(unsigned char gesture_id);

/**************************************************************************************************/
/*factory test interface*/
/**************************************************************************************************/
int semi_touch_start_factory_test(char*filebuff, int*lenth);

#endif//__SEMI_TOUCH_INTERFACE_H__
