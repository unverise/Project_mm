/*
 * @Author: xihang.jin@transsion.com
 * @Date: 2025-05-08
 * @LastEditors: xihang.jin@transsion.com
 * @LastEditTime: 2025-05-08
 * @Description: Transsion Sensor Kernel Features
 */


#include <linux/module.h>
#include <linux/slab.h>
#include "transceiver_feature.h"
#include "sensor_comm.h"
#include <linux/proc_fs.h>

//USD:modify by hongliang.zhang for [sensorhub tne] 20250520 start
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
#include <tran_sensor_tne.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
extern int sensorhub_tne_flag;
extern int tran_aee_register_notifier(struct notifier_block *nb);
extern int tran_aee_unregister_notifier(struct notifier_block *nb);
#endif
//USD:modify by hongliang.zhang for [sensorhub tne] 20250520 end

//USD:modify by weidong.liang for [trancare tne] 20251111 start
static const long long TRANCARE_TNE_SENSOR_ID = 486361000008;
static void trancare_tne_sensor_sendData(void);
//USD:modify by weidong.liang for [trancare tne] 20251111 end

struct transceiver_feature *feature_dev;
struct tran_als_nvcfg *nvcfg = NULL;
struct tran_ps_nvcfg *ps_nvcfg = NULL;
uint64_t tran_als_nv_time = 1;
uint64_t tran_padals_nv_time = 1;
uint64_t tran_ps_nv_time = 1;
struct sensor_param tran_lcm_info;
int g_tran_brightness_sensor_type = 0;
static int g_tran_last_brightness_level = 0;
uint32_t tran_charger_val = 0;
uint32_t tran_last_charger_val = 0;
int tran_charger_sensor_type = 0;
int kernel_hinge = 0;
uint8_t tran_cwb_flip_lightals_enable = false;
uint8_t tran_cwb_flip_subals_enable = false;
int tran_inter_display_st = TRAN_INTERNAL_SCREEN_POWER_ON;
int tran_exter_display_st = TRAN_EXTERNAL_SCREEN_POWER_ON;
int tran_display_st = TRAN_INTERNAL_SCREEN_POWER_ON;
uint64_t tran_get_lcm_info_timer = 25;

Transsion_CWB_Sensor_Info cust_sensor_rgb_info[TRAN_UNDER_SENSOR_MAX];
Transsion_CWB_Sensor_Info cwb_rgb_to_mid[TRAN_UNDER_SENSOR_MAX];

bool tran_sensor_un_als_support = 0;
bool tran_sensor_ml_nv_state_get;
bool tran_sensor_sl_nv_state_get;
int32_t g_tran_debug_mode = 0;
int g_tran_hall_sensor_type = 0;
int sensor_location = 0;
uint32_t nv_restore_times;
struct tran_sensor_cwb_info als_cwb_info[TRAN_UNDER_SENSOR_MAX];
struct tran_sensor_cwb_info last_als_cwb_info[TRAN_UNDER_SENSOR_MAX];
struct tran_sensor_cust_lcm_info cust_sensor_info[TRAN_UNDER_SENSOR_MAX];
static RAW_NOTIFIER_HEAD(accdet_notifier_chain);
#define CWB_POLLING_DELAY 20
#define CWB_EN_RETRY_CNT 50
struct tran_cwb_ctrl {
    int sensor_type;
    int timer_count;
    int qw_flag;
    int statue;
    bool als_on;
};
struct tran_cwb_ctrl cwb_ctrl;
struct tran_sensor_properties sensor_props;

enum Tran_Cwb_En_Lock_Status {
    CWB_EN_UNLOCK = 0,
    CWB_EN_LOCK = 1,
};
enum Tran_Cwb_En_State {
    CWB_EN_FAIL = 0,
    CWB_EN_SUCESS = 1,
};

//modify for SR-202507-002286 by hongliang.zhang 20250818 start
extern ssize_t tranlog_local(const char *tag, const char* fmt, ...);
struct tran_sensor_type_info dts_types;
struct last_arr_data last_arr_data = {0};
#define ARR_TAG "sensor_arr"
//midify for SR-202507-002286 by hongliang.zhang 20250818 end

//modify for trancare SR-202507-002286 by hongliang.zhang 20250818 start
static long long g_tid = 184561000008;
static int tran_compass_status = TRAN_SENSOR_OFF;
//modify for trancare SR-202507-002286 by hongliang.zhang 20250818 end

struct transceiver_feature *get_transceiver_feature(void) {
    return feature_dev;
}

int als_get_vrefresh(void *data, uint32_t count)
{
    return transceiver_comm_with_ptr(SENSOR_TYPE_LIGHT, SENS_COMM_CTRL_DEBUG_CMD, (uint32_t *)data, sizeof(count));
}
EXPORT_SYMBOL_GPL(als_get_vrefresh);


int kernel_get_hinge(void)
{
    pr_err("kernel_get_hinge: %d \n", kernel_hinge);
    return kernel_hinge;
}
EXPORT_SYMBOL_GPL(kernel_get_hinge);


int call_accdet_notifiers(unsigned long val, void *v)
{
    return raw_notifier_call_chain(&accdet_notifier_chain, val, v);
}
EXPORT_SYMBOL(call_accdet_notifiers);

int register_accdet_notifier(struct notifier_block *nb)
{
    int err;
    err = raw_notifier_chain_register(&accdet_notifier_chain, nb);
    if(err)
        goto out;
out:
    return err;
}
EXPORT_SYMBOL(register_accdet_notifier);

static int aw_sar_accdet_inout_event(struct notifier_block *nb, unsigned long event, void *v)
{
    switch(event){
    case ACCDET_NOTIFIER_IN:
        printk("accdet_notifier event in");
        transceiver_comm_with_ptr(SENSOR_TYPE_SAR, SENS_COMM_CTRL_CALI_CMD, NULL, 0);
        break;

    case ACCDET_NOTIFIER_OUT:
        printk("accdet_notifier event out");
        transceiver_comm_with_ptr(SENSOR_TYPE_SAR, SENS_COMM_CTRL_CALI_CMD, NULL, 0);
        break;

    default:
        break;
    }
    return 0;
}

static struct notifier_block accdet_init_notifier = {
    .notifier_call = aw_sar_accdet_inout_event,
};

static enum hrtimer_restart vrefresh_timer_func(struct hrtimer *timer) {
    queue_work(feature_dev->vrefresh_wq, &feature_dev->work_vrefresh);
    hrtimer_forward_now(&feature_dev->vrefresh_timer, feature_dev->vrefresh_poll_delay);
    return HRTIMER_RESTART;
}

static void vrefresh_work_func(struct work_struct *work) {
    pr_info("[vrefresh] Vrefresh work function executed.");
}

static enum hrtimer_restart hall_timer_func(struct hrtimer *timer) {
    queue_work(feature_dev->hall_wq, &feature_dev->work_hall);
    hrtimer_forward_now(&feature_dev->hall_timer, feature_dev->hall_poll_delay);
    return HRTIMER_RESTART;
}

static void hall_work_func(struct work_struct *work) {
    pr_info("[hall] Hall sensor work function executed.");
}

static enum hrtimer_restart cwb_timer_func(struct hrtimer *timer)
{
    int retry_cnt = CWB_EN_RETRY_CNT;

    if((cwb_ctrl.statue == CWB_EN_FAIL) && (cwb_ctrl.timer_count < retry_cnt) && (cwb_ctrl.qw_flag == CWB_EN_UNLOCK)) {
        queue_work(feature_dev->cwb_wq, &feature_dev->work_cwb);
        pr_err("%s [tran_sensor]retry queue_work start for cwb enable, retry count:%d, cwb_ctrl.statue = %d, cwb_ctrl.qw_flag = %d\n", __func__, cwb_ctrl.timer_count, cwb_ctrl.statue, cwb_ctrl.qw_flag);
    }

    if (cwb_ctrl.timer_count < retry_cnt) {
        cwb_ctrl.timer_count++;
    }

    if((cwb_ctrl.statue == CWB_EN_SUCESS) && (cwb_ctrl.timer_count < retry_cnt) && (cwb_ctrl.qw_flag == CWB_EN_UNLOCK)) {
        pr_err("%s [tran_sensor] The cwb has been enabled\n", __func__);
        return HRTIMER_NORESTART;
    }

    if(cwb_ctrl.timer_count == retry_cnt) {
        pr_err("%s [tran_sensor]retry queue_work end cwb_ctrl.timer_count >= %d, cwb_ctrl.statue = %d, cwb_ctrl.qw_flag = %d\n", __func__, retry_cnt, cwb_ctrl.statue, cwb_ctrl.qw_flag);
        return HRTIMER_NORESTART;
    }

    hrtimer_forward_now(&feature_dev->cwb_timer, feature_dev->cwb_poll_delay);

    return HRTIMER_RESTART;
}

static void cwb_work_func(struct work_struct *work)
{
    int ret = 0;
    if (cwb_ctrl.als_on) {
        cwb_ctrl.qw_flag = CWB_EN_LOCK;
        //pr_err("%s [tran_sensor]cwb enable start, cwb_ctrl.qw_flag = %d\n",__func__, cwb_ctrl.qw_flag);
        ret = tran_sensor_cwb_enable(cwb_ctrl.sensor_type, true);
        cwb_ctrl.qw_flag = CWB_EN_UNLOCK;
        if (ret < 0) {
            cwb_ctrl.statue = CWB_EN_FAIL;
            pr_err("%s [tran_sensor]cwb enable end [fail], cwb_ctrl.statue = %d, cwb_ctrl.qw_flag = %d\n",__func__, cwb_ctrl.statue, cwb_ctrl.qw_flag);
        } else {
            cwb_ctrl.statue = CWB_EN_SUCESS;
            pr_err("%s [tran_sensor]cwb enable end [sucess], cwb_ctrl.statue = %d, cwb_ctrl.qw_flag = %d\n",__func__, cwb_ctrl.statue, cwb_ctrl.qw_flag);
        }
    } else {
        pr_err("%s [tran_sensor]cwb enable Junmp, cwb_ctrl.statue = %d, cwb_ctrl.qw_flag = %d, als_on = %d\n",__func__, cwb_ctrl.statue, cwb_ctrl.qw_flag, cwb_ctrl.als_on);
    }
}


int als_set_brightness(void *data, uint32_t count)
{
    int tran_sensor_type = 0;
    if(sensor_props.cwb_support){
        tran_sensor_type = cwb_ctrl.sensor_type;
    }else{
        tran_sensor_type = SENSOR_TYPE_LIGHT;
    }
    return transceiver_comm_with_ptr(tran_sensor_type, SENS_COMM_CTRL_DEBUG_CMD, (uint32_t *)data, sizeof(count));
}
EXPORT_SYMBOL_GPL(als_set_brightness);

static enum hrtimer_restart brightness_timer_func(struct hrtimer *timer) {
    queue_work(feature_dev->brightness_wq, &feature_dev->work_brightness);
    hrtimer_forward_now(&feature_dev->brightness_timer, feature_dev->brightness_poll_delay);
    return HRTIMER_RESTART;
}

static void brightness_work_func(struct work_struct *work)
{
    int ret = 0;
    if (g_tran_debug_mode == 2) {
        printk("[get_brightness] %s, g_tran_brightness_level= %d.\n ",__func__, g_tran_brightness_level);
    }
    if(g_tran_last_brightness_level != g_tran_brightness_level)
    {
        g_tran_last_brightness_level = g_tran_brightness_level;
        ret = als_set_brightness(&g_tran_last_brightness_level, sizeof(g_tran_last_brightness_level));
        //printk("[get_brightness] %s: als_set_brightness done! g_tran_brightness_level = %d\n", __func__, g_tran_brightness_level);
    }
    //printk("[tran_sensor] [will do tran_sensor_cwb_rgb_get]%s\n ",__func__);

    if(sensor_props.cwb_support){
        uint8_t i = 0;
        if ((tran_sensor_un_als_support)&&(cwb_ctrl.statue == CWB_EN_SUCESS)) {
            memset(&tran_lcm_info, 0, sizeof(struct sensor_param));
            tran_sensor_cwb_rgb_get();
            for (i = 0; i < TRAN_UNDER_SENSOR_MAX; i++) {
            if ((cust_sensor_info[i].support_state) && (cust_sensor_info[i].cwb_state == CWB_INFO_UPDATE)) {
                tran_lcm_info.cust_param[0] = als_cwb_info[i].r_avg;
                tran_lcm_info.cust_param[1] = als_cwb_info[i].g_avg;
                tran_lcm_info.cust_param[2] = als_cwb_info[i].b_avg;
                tran_lcm_info.cust_param[3] = cwb_ctrl.sensor_type;
                //printk("[tran_sensor] transceiver_comm_with_ptr %s\n ",__func__);
                transceiver_comm_with_ptr(cwb_ctrl.sensor_type, SENS_COMM_CTRL_CUST_PARAM_CMD, &tran_lcm_info, sizeof(struct sensor_param));
                cust_sensor_info[i].cwb_state = CWB_INFO_NOT_UPDATE;
                }
            }
        }
    }
}

//modify for trancare SR-202507-002286 by hongliang.zhang 20251009 start
static int trancare_sensor_sendData(long long tid, char *event_id, char *name_key_1, int value_1,char *name_key_2, int value_2)
{
    if (!event_id || !name_key_1 || !name_key_2) {
        pr_err("Invalid parameters\n");
        return -EINVAL;
    }

    int state = is_tid_enable(tid);
    pr_err("tid_enable_state, state = %d\n", state);
    if (!state) {
        return -ENODEV;
    }
    if (state) {
        struct athena_msg *msg = NULL;
        msg = alloc_msg();
        if (!msg) {
            pr_err("Failed to allocate message\n");
            return -ENOMEM;
        }
        msg_put_int(&msg, name_key_1, value_1);
        msg_put_int(&msg, name_key_2, value_2);
        submit_msg(&msg, 2, tid, event_id);
        free_msg(&msg);
    }
    return 0;
}
//modify for trancare SR-202507-002286 by hongliang.zhang 20251009 end

static int tran_update_charger_state(void)
{
    int ret = 0;
    struct power_supply *psy = NULL;
    union power_supply_propval val;
    int tran_battery_current = 0;
    int tran_battery_status = 0;

    psy = power_supply_get_by_name("battery");
    if (!psy) {
        pr_err("%s: get battery supply failed!\n", __func__);
        return -1;
    }else{
        ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val); //get charger current /sys/class/power_supply/battery/current_now
        if (ret){
            pr_err("Cannot get current!\n");
            return -1;
        }
        tran_battery_current = val.intval;

        ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &val); //get battery status /sys/class/power_supply/battery/status
        if (ret){
            pr_err("Cannot get battery status!\n");
            return -1;
        }
        tran_battery_status = val.intval;
        //modify for trancare SR-202507-002286 by hongliang.zhang 20251009 start
        if ((tran_battery_current > 0) && (tran_compass_status == TRAN_SENSOR_ON)) {
            trancare_sensor_sendData(g_tid, "device_compass_charge", "compass_charge_on", 1, "compass_on", 0);
            tran_compass_status = TRAN_SENSOR_OFF;
        } else if ((tran_battery_current <= 0) && (tran_compass_status == TRAN_SENSOR_ON)) {
            trancare_sensor_sendData(g_tid, "device_compass_charge", "compass_charge_on", 0, "compass_on", 1);
            tran_compass_status = TRAN_SENSOR_OFF;
        }
        //modify for trancare SR-202507-002286 by hongliang.zhang 20251009 end
    }

    pr_err("[setchg] %s tran_current: %d, tran_status: %d\n", __func__,
            tran_battery_current, tran_battery_status);

    tran_charger_val = tran_battery_current;
    return 0;
}

static enum hrtimer_restart charger_timer_func(struct hrtimer *timer) {
    queue_work(feature_dev->charger_wq, &feature_dev->work_charger);
    hrtimer_forward_now(&feature_dev->charger_timer, feature_dev->charger_poll_delay);
    return HRTIMER_RESTART;
}

static void charger_work_func(struct work_struct *work)
{
    if (tran_update_charger_state()) {
        pr_err("[setchg] %s: update charger state failed!\n", __func__);
        return;
    }
    pr_err("[setchg] %s: start tran_charger_val = %d\n", __func__, tran_charger_val);

    if(tran_last_charger_val != tran_charger_val)
    {
        tran_last_charger_val = tran_charger_val;
        transceiver_comm_with_ptr(SENSOR_TYPE_MAGNETIC_FIELD, SENS_COMM_CTRL_DEBUG_CMD, &tran_last_charger_val, sizeof(tran_last_charger_val));
        pr_err("[setchg] %s: tran_send_charger_to_hub done! tran_charger_val = %d\n", __func__, tran_charger_val);
    }
}

//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 start
static int get_bootargs(char *current_mode, char *boot_param)
{
    struct device_node *np;
    const char *cmd_line;
    char *s = NULL;
    int ret = 0;
    int i = 0;

    if(current_mode == NULL)
        return -1;

    np = of_find_node_by_path("/chosen");
    if (!np) {
        pr_err("Can't get the /chosen");
        return -EIO;
    }
    if (!strcmp(boot_param, "lcm_name=")) {
        ret = of_property_read_string(np, "touch,lcm_name", &cmd_line);
        if (ret < 0) {
            pr_err("Can't get the touch,ret=%d", ret);
        } else {
            strcpy(current_mode, cmd_line);
            pr_err("get lcm_name: %s  len:%zu", cmd_line, strlen(cmd_line));
            return 0;
        }
    }
    ret = of_property_read_string(np, "bootargs", &cmd_line);
    if (ret < 0) {
        pr_err("Can't get the bootargs");
        return ret;
    }

    s = strstr(cmd_line, boot_param);
    if(s != NULL) {
        s += strlen(boot_param);
        while(*s != ' ' && i < 64) {
            *current_mode++ = *s++;
            i++;
        }
        *current_mode = '\0';
    } else {
        return -1;
    }

    return 0;
}

static int mtk_find_lcm_name(char * p_name)
{
    if(p_name != NULL)
        get_bootargs(p_name,"lcm_name=");

    return 1;
}

static int tran_sensor_lcm_info_comparison(void)
{
    char lcm_name[64]= {0};
    /*get current lcm name*/
    mtk_find_lcm_name(lcm_name);
    pr_info("tran_disp: get_lcm_name %s",lcm_name);

    /*Screen information comparison*/
    if(strcmp(lcm_name,tran_supplier_lcm0) == 0) {
        return TRAN_SUPPLIER_LCM_0;
    } else if(strcmp(lcm_name,tran_supplier_lcm1) == 0) {
        return TRAN_SUPPLIER_LCM_1;
    } else if(strcmp(lcm_name,tran_supplier_lcm2) == 0) {
        return TRAN_SUPPLIER_LCM_2;
    } else if(strcmp(lcm_name,tran_supplier_lcm3) == 0) {
        return TRAN_SUPPLIER_LCM_3;
    } else {
        return TRAN_SUPPLIER_LCM_MAX;
    }
}

static void tran_sensor_send_lcm_supplier_info(void)
{
    struct tran_sensor_cust_param tran_lcm_info;
    memset(&tran_lcm_info, 0, sizeof(struct tran_sensor_cust_param));

    /* init tran_lcm_info */
    tran_lcm_info.data_type = TRAN_LCM_SUPPLIER_INFO;
    tran_lcm_info.data[0] = TRAN_SUPPLIER_LCM_MAX;

    /* get lcm info & comparison*/
    tran_lcm_info.data[0] = tran_sensor_lcm_info_comparison();
    pr_info("tran_disp: lcm_supplier_info:%d",tran_lcm_info.data[0]);

    /* send lcm info */
    if (tran_lcm_info.data[0] != TRAN_SUPPLIER_LCM_MAX) {
        transceiver_comm_with_ptr(SENSOR_TYPE_LIGHT, SENS_COMM_CTRL_ONCHANGE_CMD, &tran_lcm_info, sizeof(struct tran_sensor_cust_param));
    } else {
        pr_err("tran_disp: lcm_supplier_info is max, check dts file");
    }
}

static void tran_disp_work_func(struct work_struct* work)
{
    //struct transceiver_device *device = &transceiver_dev;
    pr_info("[tran_disp:%s]get the data form Internal display tran_inter_display_st = %d , tran_display_st=%d", __func__, tran_inter_display_st , tran_display_st);

    //transceiver_comm_with(SENSOR_TYPE_LIGHT, SENS_COMM_CTRL_ONCHANGE_CMD, &tran_inter_display_st, sizeof(tran_inter_display_st));

    if (tran_inter_display_st == TRAN_INTERNAL_SCREEN_POWER_ON) {
        if(sensor_props.lcm_supplier_info){
            tran_sensor_send_lcm_supplier_info();
        }
        //hrtimer_start(&device->vrefresh_timer, ns_to_ktime(VREFRESH_POLLING_DELAY * NSEC_PER_MSEC), HRTIMER_MODE_REL);
        //pr_info("[get_vrefresh] %s is hrtimer_start poll!!!\n", __func__);
    } else if(tran_inter_display_st == TRAN_INTERNAL_SCREEN_POWER_OFF) {
        //hrtimer_cancel(&device->vrefresh_timer);
        //pr_info("[get_vrefresh] %s is hrtimer_end poll!!!\n", __func__);
        //cancel_work_sync(&device->work_vrefresh);
        //g_tran_get_vrefresh = 0;
    }
}
static void tran_a_g_cvt_map(void)
{
    if ((tran_inter_display_st == TRAN_INTERNAL_SCREEN_POWER_ON) && (tran_exter_display_st == TRAN_EXTERNAL_SCREEN_POWER_OFF)) {
        tran_display_st = TRAN_INTERNAL_SCREEN_POWER_ON;
    } else if ((tran_inter_display_st == TRAN_INTERNAL_SCREEN_POWER_OFF) && (tran_exter_display_st == TRAN_EXTERNAL_SCREEN_POWER_ON)){
        tran_display_st = TRAN_EXTERNAL_SCREEN_POWER_ON;
    } else if ((tran_inter_display_st == TRAN_INTERNAL_SCREEN_POWER_ON) && (tran_exter_display_st == TRAN_EXTERNAL_SCREEN_POWER_ON)){
        tran_display_st = TRAN_INTERNAL_SCREEN_POWER_ON;
    }
    pr_err("[tran_disp:%s]get the data tran_display_st = %d", __func__, tran_display_st);
}

static int sensor_disp_notifier_callback(struct notifier_block *nb,
    unsigned long value, void *v)
{
    int *data = (int *)v;
    if (v) {
        /*dsi on off power MTK_DISP_EVENT_BLANK: 0x01*/
        if (value == MTK_DISP_EVENT_BLANK) {
            if (*data == MTK_DISP_BLANK_UNBLANK) {
                /*Internal display on MTK_DISP_BLANK_UNBLANK: 0x00*/
                pr_err("[tran_disp:%s]get the data form Internal display ON: value = 0x%lx , data = 0x%x", __func__, value , *data);
                tran_inter_display_st = TRAN_INTERNAL_SCREEN_POWER_ON;
                tran_a_g_cvt_map();
                /* Check if disp_wq and work_disp are not NULL before queuing work */
                if (feature_dev->disp_wq) {
                    queue_work(feature_dev->disp_wq, &feature_dev->work_disp);
                } else {
                    pr_err("[tran_disp:%s] Work queue is NULL", __func__);
                }
            // } else if (*data == TRAN_DISP_BLANK_UNBLANK_DSI1) {
            //     /*External display on MTK_DISP_BLANK_UNBLANK: 0x03*/
            //     tran_exter_display_st = TRAN_EXTERNAL_SCREEN_POWER_ON;
            //     pr_err("[tran_sensor_cvt]get the data form External display ON: value = 0x%lx , data = 0x%x",value , *data);
            //     tran_a_g_cvt_map();
            } else if (*data == MTK_DISP_BLANK_POWERDOWN) {
                /*Internal display off MTK_DISP_BLANK_UNBLANK: 0x01*/
                pr_err("[tran_disp:%s]get the data form Internal display OFF: value = 0x%lx , data = 0x%x", __func__, value , *data);
                tran_inter_display_st = TRAN_INTERNAL_SCREEN_POWER_OFF;
                /* Check if disp_wq and work_disp are not NULL before queuing work */
                if (feature_dev->disp_wq) {
                    queue_work(feature_dev->disp_wq, &feature_dev->work_disp);
                }else {
                    pr_err("[tran_disp:%s] Work queue is NULL", __func__);
                }
            // } else if (*data == TRAN_DISP_BLANK_POWERDOWN_DSI1) {
            //     /*External display off MTK_DISP_BLANK_UNBLANK: 0x04*/
            //     pr_err("[tran_sensor_cvt]get the data form External display OFF: value = 0x%lx , data = 0x%x",value , *data);
            //     tran_exter_display_st = TRAN_EXTERNAL_SCREEN_POWER_OFF;
            }
        }
    } else {
        pr_err("[tran_disp:%s] can't get the data from display", __func__);
        return 0;
    }
    return 0;
}
//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 end

void dump_cwb_coordinate(int location)
{
    pr_err("%s [tran_sensor] dts info offset_x=%d, offset_y=%d, clip_w=%d, clip_h=%d\n", \
                __func__, \
                cust_sensor_info[location].offset_x, \
                cust_sensor_info[location].offset_y, \
                cust_sensor_info[location].clip_w, \
                cust_sensor_info[location].clip_h);
}

static ssize_t cwb_status_write(struct file *filp, const char __user * buf, size_t count, loff_t * off)
{
    int32_t ret;
    int32_t tmp;
    uint8_t tran_debug_mode;
    char buff[2]={'0','\0'};

    if (count == 0 || count > 2) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff, buf, 1);
    if (ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        TRAN_ERROR("Invalid value! ret = %d", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp != 0 && tmp != 1 && tmp != 2 && tmp != 3 && tmp != 4 && tmp != 5) {
        TRAN_ERROR("Invalid value! ret = %d, tran_debug_mode val = %d", ret, tmp);
        return -EINVAL;
        goto out;
    }

    tran_debug_mode = tmp;
    TRAN_ERROR("tran_debug_mode = %d", tran_debug_mode);

    g_tran_debug_mode = tran_debug_mode;

    ret = count;

out:
    return ret;
}

static ssize_t cwb_status_read(struct file *file, char * buffer, size_t size, loff_t * ppos)
{
    int len,ret = -1;
    char *page = NULL;
    char *ptr = NULL;
    uint8_t i = 0;

    Transsion_CWB_Sensor_Info adb_rgb_map[TRAN_UNDER_SENSOR_MAX];
    memset(cwb_rgb_to_mid, 0, sizeof(Transsion_CWB_Sensor_Info) * TRAN_UNDER_SENSOR_MAX);
    memset(adb_rgb_map, 0, sizeof(Transsion_CWB_Sensor_Info) * TRAN_UNDER_SENSOR_MAX);

    transsion_get_cwb_rgb_info(cwb_rgb_to_mid);
    page = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!page)
    {
        kfree(page);
        return -ENOMEM;
    }
    ptr = page;

    for (i = 0; i < TRAN_UNDER_SENSOR_MAX; i++) {
        if (cwb_rgb_to_mid[i].sensor_id == TRAN_ML) {
            if (cust_sensor_info[TRAN_ML].support_state) {
                adb_rgb_map[TRAN_ML].sensor_id = SENSOR_TYPE_LIGHT;
            } else {
                adb_rgb_map[TRAN_ML].sensor_id = cwb_rgb_to_mid[i].sensor_id;
            }
            adb_rgb_map[TRAN_ML].r_avg = cwb_rgb_to_mid[i].r_avg;
            adb_rgb_map[TRAN_ML].g_avg = cwb_rgb_to_mid[i].g_avg;
            adb_rgb_map[TRAN_ML].b_avg = cwb_rgb_to_mid[i].b_avg;
        }else if (cwb_rgb_to_mid[i].sensor_id == TRAN_MSL) {
            adb_rgb_map[TRAN_MSL].sensor_id = cwb_rgb_to_mid[i].sensor_id;
            adb_rgb_map[TRAN_MSL].r_avg = cwb_rgb_to_mid[i].r_avg;
            adb_rgb_map[TRAN_MSL].g_avg = cwb_rgb_to_mid[i].g_avg;
            adb_rgb_map[TRAN_MSL].b_avg = cwb_rgb_to_mid[i].b_avg;
        }else if (cwb_rgb_to_mid[i].sensor_id == TRAN_SL) {
            if (cust_sensor_info[TRAN_SL].support_state) {
                adb_rgb_map[TRAN_SL].sensor_id = SENSOR_TYPE_PADALS;
            } else {
                adb_rgb_map[TRAN_SL].sensor_id = cwb_rgb_to_mid[i].sensor_id;
            }
            adb_rgb_map[TRAN_SL].r_avg = cwb_rgb_to_mid[i].r_avg;
            adb_rgb_map[TRAN_SL].g_avg = cwb_rgb_to_mid[i].g_avg;
            adb_rgb_map[TRAN_SL].b_avg = cwb_rgb_to_mid[i].b_avg;
        }else if (cwb_rgb_to_mid[i].sensor_id == TRAN_SSL) {
            adb_rgb_map[TRAN_SSL].sensor_id = cwb_rgb_to_mid[i].sensor_id;
            adb_rgb_map[TRAN_SSL].r_avg = cwb_rgb_to_mid[i].r_avg;
            adb_rgb_map[TRAN_SSL].g_avg = cwb_rgb_to_mid[i].g_avg;
            adb_rgb_map[TRAN_SSL].b_avg = cwb_rgb_to_mid[i].b_avg;
        }
    }

    if ((cust_sensor_info[TRAN_ML].support_state == 1) && (cust_sensor_info[TRAN_SL].support_state == 0)) {
        ptr += sprintf(ptr,"%d:%d:%d:%d\n%d:%d:%d:%d\n%d:%d:%d:%d\n%d:%d:%d:%d\n",adb_rgb_map[TRAN_ML].sensor_id,adb_rgb_map[TRAN_ML].r_avg,adb_rgb_map[TRAN_ML].g_avg,adb_rgb_map[TRAN_ML].b_avg,
                    adb_rgb_map[TRAN_MSL].sensor_id,adb_rgb_map[TRAN_MSL].r_avg,adb_rgb_map[TRAN_MSL].g_avg,adb_rgb_map[TRAN_MSL].b_avg,
                    adb_rgb_map[TRAN_SL].sensor_id,adb_rgb_map[TRAN_SL].r_avg,adb_rgb_map[TRAN_SL].g_avg,adb_rgb_map[TRAN_SL].b_avg,
                    adb_rgb_map[TRAN_SSL].sensor_id,adb_rgb_map[TRAN_SSL].r_avg,adb_rgb_map[TRAN_SSL].g_avg,adb_rgb_map[TRAN_SSL].b_avg);
    } else if ((cust_sensor_info[TRAN_ML].support_state == 0) && (cust_sensor_info[TRAN_SL].support_state == 1)) {
        ptr += sprintf(ptr,"%d:%d:%d:%d\n%d:%d:%d:%d\n%d:%d:%d:%d\n%d:%d:%d:%d\n",adb_rgb_map[TRAN_SL].sensor_id,adb_rgb_map[TRAN_SL].r_avg,adb_rgb_map[TRAN_SL].g_avg,adb_rgb_map[TRAN_SL].b_avg,
                    adb_rgb_map[TRAN_ML].sensor_id,adb_rgb_map[TRAN_ML].r_avg,adb_rgb_map[TRAN_ML].g_avg,adb_rgb_map[TRAN_ML].b_avg,
                    adb_rgb_map[TRAN_MSL].sensor_id,adb_rgb_map[TRAN_MSL].r_avg,adb_rgb_map[TRAN_MSL].g_avg,adb_rgb_map[TRAN_MSL].b_avg,
                    adb_rgb_map[TRAN_SSL].sensor_id,adb_rgb_map[TRAN_SSL].r_avg,adb_rgb_map[TRAN_SSL].g_avg,adb_rgb_map[TRAN_SSL].b_avg);
    }
    len = ptr - page;
    if(*ppos >= len)
    {
        kfree(page);
        return 0;
    }
    ret = copy_to_user(buffer,(char *)page,len);
    *ppos += len;
    if(ret)
    {
        kfree(page);
        return ret;
    }
    kfree(page);
    return len;

}

static const struct proc_ops cwb_fops = {
    .proc_read = cwb_status_read,
    .proc_write = cwb_status_write,
};

void tran_sensor_dump_get_cwb_info(void)
{
    uint8_t i = 0;
    if (g_tran_debug_mode) {
        for (i = 0; i < TRAN_UNDER_SENSOR_MAX; i++) {
            if (cust_sensor_info[i].support_state) {
                pr_info("[%s] cust_sensor_rgb_info[%d], sensor_id[%d], r_avg=%d, g_avg=%d, b_avg=%d\n", \
                        __func__, i, \
                        cust_sensor_rgb_info[i].sensor_id, \
                        cust_sensor_rgb_info[i].r_avg, \
                        cust_sensor_rgb_info[i].g_avg, \
                        cust_sensor_rgb_info[i].b_avg);
                pr_info("[%s] als_cwb_info[%d], r_avg=%d, g_avg=%d, b_avg=%d\n", \
                        __func__, i, \
                        als_cwb_info[i].r_avg, \
                        als_cwb_info[i].g_avg, \
                        als_cwb_info[i].b_avg);
            }
        }
    }
}

void tran_sensor_cwb_register(void)
{
    uint8_t i = 0;
    uint8_t ret = 0;
    for (i = 0; i < TRAN_UNDER_SENSOR_MAX; i++) {

        /*Settings lcm id*/
        if ((i == TRAN_ML) || (i == TRAN_MSL)) {
            cust_sensor_info[i].lcm_id = TRANSSION_DISP_CWB_MAIN_LCM;
        } else if ((i == TRAN_SL) || (i == TRAN_SSL)) {
            cust_sensor_info[i].lcm_id = TRANSSION_DISP_CWB_SUB_LCM;
        }

        /*Dynamic registration according to dts*/
        if (cust_sensor_info[i].support_state) {
            ret = transsion_cwb_registor(cust_sensor_info[i].lcm_id, i, \
                                            cust_sensor_info[i].offset_x, cust_sensor_info[i].offset_y, \
                                            cust_sensor_info[i].clip_w, cust_sensor_info[i].clip_h, \
                                            cust_sensor_info[i].Lsensor_x, cust_sensor_info[i].Lsensor_y,\
                                            cust_sensor_info[i].Lsensor_w, cust_sensor_info[i].Lsensor_h, \
                                            cust_sensor_info[i].ratio_in, cust_sensor_info[i].ratio_out);
            if (ret == 0) {
                pr_info("[%s][Sucess]:lcm_id[%d],cust_sensor_info[%d].support_state = %u\n", __func__, cust_sensor_info[i].lcm_id, \
                        i, cust_sensor_info[i].support_state);
            } else {
                pr_err("[%s][fail]:lcm_id[%d],cust_sensor_info[%d].support_state = %u\n", __func__, cust_sensor_info[i].lcm_id, \
                        i, cust_sensor_info[i].support_state);
            }
            
        } else {
            pr_info("[%s][fail because it does not support cwb]:lcm_id[%d],cust_sensor_info[%d].support_state = %u\n", __func__, cust_sensor_info[i].lcm_id, \
                        i, cust_sensor_info[i].support_state);
        }
    }
}

int tran_sensor_cwb_enable(uint8_t sensor_type, bool en)
{
    uint8_t sensor_id = 0;
    int ret  = 0;
    uint8_t i = 0;

    /*reset cwb info buffer*/

    memset(cust_sensor_rgb_info, 0, sizeof(Transsion_CWB_Sensor_Info) * TRAN_UNDER_SENSOR_MAX);
    /*eset cwb info buffer for scp*/
    //memset(als_cwb_info, 0, sizeof(struct tran_sensor_cwb_info) * TRAN_UNDER_SENSOR_MAX);
    //memset(last_als_cwb_info, 0, sizeof(struct tran_sensor_cwb_info) * TRAN_UNDER_SENSOR_MAX);
    for (i = 0; i < TRAN_UNDER_SENSOR_MAX; i++) {
        als_cwb_info[i].r_avg = 300;
        als_cwb_info[i].g_avg = 300;
        als_cwb_info[i].b_avg = 300;
        last_als_cwb_info[i].r_avg = 300;
        last_als_cwb_info[i].g_avg = 300;
        last_als_cwb_info[i].b_avg = 300;
    }

    /*enable main als cwb*/
    if (sensor_type == SENSOR_TYPE_LIGHT) {
        sensor_id = TRAN_ML;
        ret = transsion_cwb_enable(en, cust_sensor_info[sensor_id].lcm_id);
        pr_info("[%s][done]:sensor_type[%d], en[%d]\n", __func__, sensor_type, en);
    }
    /*enable main sub als cwb*/
    //TDB

    /*enable sub als cwb*/
    if (sensor_type == SENSOR_TYPE_PADALS) {
        sensor_id = TRAN_SL;
        ret = transsion_cwb_enable(en, cust_sensor_info[sensor_id].lcm_id);
        pr_info("[%s][done]:sensor_type[%d], en[%d]\n", __func__, sensor_type, en);
    }

    /*enable sub sub als cwb*/
    //TDB

    return ret;
}

void tran_sensor_cwb_init(void)
{
    struct proc_dir_entry *tran_cwb_msg = NULL;
    tran_cwb_msg = proc_create("rgb_screeninfo", 0666, NULL, &cwb_fops);
    if (tran_cwb_msg == NULL) {
        pr_err("[Sensor CWB PROC]Couldn't create proc entry[tran_cwb_msg]!");
    } else {
        pr_info("[Sensor CWB PROC]Create proc entry[tran_cwb_msg] success!");
    }

    memset(cust_sensor_rgb_info, 0, sizeof(Transsion_CWB_Sensor_Info) * TRAN_UNDER_SENSOR_MAX);

    tran_sensor_dump_get_cwb_info();
    transsion_cwb_init();

    pr_info("[%s][done]\n", __func__);
}

void tran_sensor_cwb_rgb_get(void)
{
    uint8_t i = 0;
    Transsion_CWB_Sensor_Info rgb_map[TRAN_UNDER_SENSOR_MAX];

    memset(rgb_map, 0, sizeof(Transsion_CWB_Sensor_Info) * TRAN_UNDER_SENSOR_MAX);
    //get cwb info for 4 position
    transsion_get_cwb_rgb_info(cust_sensor_rgb_info);


    for (i = 0; i < TRAN_UNDER_SENSOR_MAX; i++) {
        if (cust_sensor_rgb_info[i].sensor_id == TRAN_ML) {
            rgb_map[TRAN_ML].r_avg = cust_sensor_rgb_info[i].r_avg;
            rgb_map[TRAN_ML].g_avg = cust_sensor_rgb_info[i].g_avg;
            rgb_map[TRAN_ML].b_avg = cust_sensor_rgb_info[i].b_avg;
        }else if (cust_sensor_rgb_info[i].sensor_id == TRAN_MSL) {
            rgb_map[TRAN_MSL].r_avg = cust_sensor_rgb_info[i].r_avg;
            rgb_map[TRAN_MSL].g_avg = cust_sensor_rgb_info[i].g_avg;
            rgb_map[TRAN_MSL].b_avg = cust_sensor_rgb_info[i].b_avg;
        }else if (cust_sensor_rgb_info[i].sensor_id == TRAN_SL) {
            rgb_map[TRAN_SL].r_avg = cust_sensor_rgb_info[i].r_avg;
            rgb_map[TRAN_SL].g_avg = cust_sensor_rgb_info[i].g_avg;
            rgb_map[TRAN_SL].b_avg = cust_sensor_rgb_info[i].b_avg;
        }else if (cust_sensor_rgb_info[i].sensor_id == TRAN_SSL) {
            rgb_map[TRAN_SSL].r_avg = cust_sensor_rgb_info[i].r_avg;
            rgb_map[TRAN_SSL].g_avg = cust_sensor_rgb_info[i].g_avg;
            rgb_map[TRAN_SSL].b_avg = cust_sensor_rgb_info[i].b_avg;
        }
    }

    for (i = 0; i < TRAN_UNDER_SENSOR_MAX; i++) {
        als_cwb_info[i].r_avg = rgb_map[i].r_avg;
        als_cwb_info[i].g_avg = rgb_map[i].g_avg;
        als_cwb_info[i].b_avg = rgb_map[i].b_avg;

        if (cust_sensor_info[i].support_state) {
            tran_sensor_dump_get_cwb_info();
            if ((als_cwb_info[i].r_avg != last_als_cwb_info[i].r_avg) ||
                (als_cwb_info[i].g_avg != last_als_cwb_info[i].g_avg) ||
                (als_cwb_info[i].b_avg != last_als_cwb_info[i].b_avg)) {
                    cust_sensor_info[i].cwb_state = CWB_INFO_UPDATE;
                    //tran_sensor_dump_get_cwb_info();
                }
        }

        last_als_cwb_info[i].r_avg = als_cwb_info[i].r_avg;
        last_als_cwb_info[i].g_avg = als_cwb_info[i].g_avg;
        last_als_cwb_info[i].b_avg = als_cwb_info[i].b_avg;
    }
}

void transceiver_handle_cwb_disable(int sensor_type, int en) {
if(sensor_props.cwb_support){
    if (tran_sensor_un_als_support) {
        if ((cust_sensor_info[TRAN_ML].support_state) && (sensor_type == SENSOR_TYPE_LIGHT)) {
            int cwb_disable_ret = tran_sensor_cwb_enable(sensor_type, false);
            pr_err("%s-AlsDisable-[tran_sensor] sensor_type[%d], en[%d] cwb disable done, cwb_disable_ret:%d\n",
                   __func__, sensor_type, cwb_disable_ret, en);
        }
        if ((cust_sensor_info[TRAN_SL].support_state) && (sensor_type == SENSOR_TYPE_PADALS)) {
            int cwb_disable_ret = tran_sensor_cwb_enable(sensor_type, false);
            pr_err("%s-AlsDisable-[tran_sensor] sensor_type[%d], en[%d] cwb disable done, cwb_disable_ret:%d\n",
                   __func__, sensor_type, cwb_disable_ret, en);
        }
    }
}
}

void transceiver_handle_sensor_enable(struct transceiver_feature *feature, int sensor_type, int en) {
    if(sensor_props.charger_poll){
        if (sensor_type == SENSOR_TYPE_ORIENTATION) {
            pr_err("[setchg] %s sensor_type = %d\n", __func__, sensor_type);
            tran_charger_sensor_type = sensor_type;
            if (en) {
                queue_work(feature->charger_wq, &feature->work_charger);
                hrtimer_start(&feature->charger_timer, ns_to_ktime(CHARGER_POLLING_DELAY * NSEC_PER_MSEC), HRTIMER_MODE_REL);
                pr_err("[setchg] %s is hrtimer_start poll!!!\n", __func__);
                //modify for trancare SR-202507-002286 by hongliang.zhang 20250818 start
                tran_compass_status = TRAN_SENSOR_ON;
                //modify for trancare SR-202507-002286 by hongliang.zhang 20250818 end
            } else {
                hrtimer_cancel(&feature->charger_timer);
                pr_err("[setchg] %s is hrtimer_end poll!!!\n", __func__);
                cancel_work_sync(&feature->work_charger);
                tran_charger_val = 0;
                //modify for trancare SR-202507-002286 by hongliang.zhang 20250818 start
                tran_compass_status = TRAN_SENSOR_OFF;
                //modify for trancare SR-202507-002286 by hongliang.zhang 20250818 end
            }
        }
    }

//USD: add LK7KOS16-2 by yi.peng for HWC 20251010 start
    if(sensor_props.brightness_level_support || sensor_props.cwb_support){
            if (sensor_type == SENSOR_TYPE_LIGHT) {
                g_tran_brightness_sensor_type = sensor_type;
                if (en) {
                hrtimer_start(&feature->brightness_timer, ns_to_ktime(tran_get_lcm_info_timer * NSEC_PER_MSEC), HRTIMER_MODE_REL);
                } else {
                hrtimer_cancel(&feature->brightness_timer);
                cancel_work_sync(&feature->work_brightness);
                //g_tran_brightness_level = 0;
                }
            }
    }
//USD: add LK7KOS16-2 by yi.peng for HWC 20251010 end

    if(sensor_props.hall_sensor){
        if (sensor_type == SENSOR_TYPE_HINGE) {
            g_tran_hall_sensor_type = sensor_type;
            if (en) {
                hrtimer_start(&feature->hall_timer, ns_to_ktime(HALL_POLLING_DELAY * NSEC_PER_MSEC), HRTIMER_MODE_REL);
            } else {
                hrtimer_cancel(&feature->hall_timer);
                cancel_work_sync(&feature->work_hall);
                g_tran_last_hall_level = 0;
            }
        }
    }
}

void transceiver_handle_als_enable(struct transceiver_feature *feature, int sensor_type, int en) {

    if(sensor_props.cwb_support){
        if ((cust_sensor_info[TRAN_ML].support_state) && (sensor_type == SENSOR_TYPE_LIGHT)) {
            sensor_location = TRAN_ML;
            dump_cwb_coordinate(sensor_location);
            cust_sensor_info[TRAN_ML].enable_state = en;
            cwb_ctrl.sensor_type = SENSOR_TYPE_LIGHT;
            cwb_ctrl.als_on = true;
            g_tran_last_brightness_level = 1000;
            if (cwb_ctrl.qw_flag == CWB_EN_UNLOCK) {
                queue_work(feature->cwb_wq, &feature->work_cwb);
            }
            hrtimer_start(&feature->brightness_timer, ns_to_ktime(tran_get_lcm_info_timer * NSEC_PER_MSEC), HRTIMER_MODE_REL);
            hrtimer_start(&feature->cwb_timer, ns_to_ktime(CWB_POLLING_DELAY * NSEC_PER_MSEC), HRTIMER_MODE_REL);
            //pr_err("%s-AlsEnable-[tran_sensor] sensor_type[%d], en[%d] do queue_work and hrtimer_start !!!\n", __func__, sensor_type, en);
        }

        if ((cust_sensor_info[TRAN_SL].support_state) && (sensor_type == SENSOR_TYPE_PADALS)) {
            sensor_location = TRAN_SL;
            dump_cwb_coordinate(sensor_location);
            cust_sensor_info[TRAN_SL].enable_state = en;
            cwb_ctrl.sensor_type = SENSOR_TYPE_PADALS;
            cwb_ctrl.als_on = true;
            g_tran_last_brightness_level = 1000;

            if (cwb_ctrl.qw_flag == CWB_EN_UNLOCK) {
                queue_work(feature->cwb_wq, &feature->work_cwb);
            }
            hrtimer_start(&feature->brightness_timer, ns_to_ktime(tran_get_lcm_info_timer * NSEC_PER_MSEC), HRTIMER_MODE_REL);
            hrtimer_start(&feature->cwb_timer, ns_to_ktime(CWB_POLLING_DELAY * NSEC_PER_MSEC), HRTIMER_MODE_REL);
            //pr_err("%s-AlsEnable-[tran_sensor] sensor_type[%d], en[%d] do queue_work and hrtimer_start !!!\n", __func__, sensor_type, en);
        }
    }
}

void transceiver_handle_als_disable(struct transceiver_feature *feature, int sensor_type, int en) {

    if(sensor_props.cwb_support){
        if ((cust_sensor_info[TRAN_ML].support_state) && (sensor_type == SENSOR_TYPE_LIGHT)) {
            cust_sensor_info[TRAN_ML].enable_state = en;
            cwb_ctrl.als_on = false;
            hrtimer_cancel(&feature->brightness_timer);
            hrtimer_cancel(&feature->cwb_timer);
            cwb_ctrl.timer_count = 0;
            cancel_work_sync(&feature->work_cwb);
            //pr_err("%s-AlsDisable-[tran_sensor] sensor_type[%d], en[%d] do cancel_work_sync and hrtimer_cancel !!!\n", __func__, sensor_type, en);
        }
        if ((cust_sensor_info[TRAN_SL].support_state) && (sensor_type == SENSOR_TYPE_PADALS)) {
            cust_sensor_info[TRAN_SL].enable_state = en;
            cwb_ctrl.als_on = false;
            hrtimer_cancel(&feature->brightness_timer);
            hrtimer_cancel(&feature->cwb_timer);
            cwb_ctrl.timer_count = 0;
            cancel_work_sync(&feature->work_cwb);
            //pr_err("%s-AlsDisable-[tran_sensor] sensor_type[%d], en[%d] do cancel_work_sync and hrtimer_cancel !!!\n", __func__, sensor_type, en);
        }
    }
}

void transceiver_alloc_nv_configs(void) {
    if (!nvcfg) {
        nvcfg = kzalloc(sizeof(struct tran_als_nvcfg) + (ALS_NV_TRANSFER_MAX_BYTE * nv_restore_times), GFP_KERNEL);
        if (nvcfg) {
            pr_err("nvcfg created successfully\n");
        } else {
            pr_err("Failed to allocate nvcfg\n");
        }
    }

    if (!ps_nvcfg) {
        ps_nvcfg = kzalloc(sizeof(struct tran_ps_nvcfg) + (PS_NV_TRANSFER_MAX_BYTE * PS_NV_TRANSFER_TIME), GFP_KERNEL);
        if (ps_nvcfg) {
            pr_err("ps_nvcfg created successfully\n");
        } else {
            pr_err("Failed to allocate ps_nvcfg\n");
        }
    }
}

void transceiver_handle_sensor_config(int sensor_type, void *data) {
    if ((cust_sensor_info[TRAN_ML].support_state) && (tran_sensor_ml_nv_state_get)) {
        if (sensor_type == SENSOR_TYPE_LIGHT) {
            if (tran_als_nv_time % nv_restore_times == 1) {
                memcpy(&nvcfg->data.field[0], data, ALS_NV_TRANSFER_MAX_BYTE);
                tran_als_nv_time++;
            } else if (tran_als_nv_time % nv_restore_times == 2) {
                memcpy(&nvcfg->data.field[ALS_NV_TRANSFER_MAX_BYTE], data, ALS_NV_TRANSFER_MAX_BYTE);
                tran_als_nv_time++;
            } else if (tran_als_nv_time % nv_restore_times == 0) {
                memcpy(&nvcfg->data.field[2 * ALS_NV_TRANSFER_MAX_BYTE], data, ALS_NV_TRANSFER_MAX_BYTE);
                tran_als_nv_time++;
            }
        }
    }

    if ((cust_sensor_info[TRAN_SL].support_state) && (tran_sensor_sl_nv_state_get)) {
        if (sensor_type == SENSOR_TYPE_PADALS) {
            if (tran_padals_nv_time % nv_restore_times == 1) {
                memcpy(&nvcfg->data.field[0], data, ALS_NV_TRANSFER_MAX_BYTE);
                tran_padals_nv_time++;
            } else if (tran_padals_nv_time % nv_restore_times == 2) {
                memcpy(&nvcfg->data.field[ALS_NV_TRANSFER_MAX_BYTE], data, ALS_NV_TRANSFER_MAX_BYTE);
                tran_padals_nv_time++;
            } else if (tran_padals_nv_time % nv_restore_times == 0) {
                memcpy(&nvcfg->data.field[2 * ALS_NV_TRANSFER_MAX_BYTE], data, ALS_NV_TRANSFER_MAX_BYTE);
                tran_padals_nv_time++;
            }
        }
    }

if (sensor_type == SENSOR_TYPE_PROXIMITY) {
    if (tran_ps_nv_time % PS_NV_TRANSFER_TIME == 1) {
        memcpy(&ps_nvcfg->data.field[0], data, PS_NV_TRANSFER_MAX_BYTE);
        tran_ps_nv_time++;
    } else if (tran_ps_nv_time % PS_NV_TRANSFER_TIME == 0) {
        memcpy(&ps_nvcfg->data.field[PS_NV_TRANSFER_MAX_BYTE], data, PS_NV_TRANSFER_MAX_BYTE);
        tran_ps_nv_time++;
    }
}
}

int transceiver_restore_config(uint8_t sensor_type, void *config_data, size_t config_length) {
    int ret = 0;

    // 恢复 SENSOR_TYPE_LIGHT 的配置
    if ((sensor_type == SENSOR_TYPE_LIGHT) && (tran_sensor_ml_nv_state_get)) {
        if (cust_sensor_info[TRAN_ML].vendor == SensorTek) { // SensorTek
            if (cust_sensor_info[TRAN_ML].algo == CutOut) { // CutOut
                if (cust_sensor_info[TRAN_ML].ver == V1_0_0) { // V1_0_0
                    ret = transceiver_comm_with_ptr(sensor_type, SENS_COMM_CTRL_CONFIG_CMD, &nvcfg->data.field[0], ALS_NV_TRANSFER_MAX_BYTE);
                    ret = transceiver_comm_with_ptr(sensor_type, SENS_COMM_CTRL_CONFIG_CMD, &nvcfg->data.field[ALS_NV_TRANSFER_MAX_BYTE], ALS_NV_TRANSFER_MAX_BYTE);
                    ret = transceiver_comm_with_ptr(sensor_type, SENS_COMM_CTRL_CONFIG_CMD, &nvcfg->data.field[2 * ALS_NV_TRANSFER_MAX_BYTE], ALS_NV_TRANSFER_MAX_BYTE);
                }
            }
        }
    }

    // 恢复 SENSOR_TYPE_PADALS 的配置
    if ((sensor_type == SENSOR_TYPE_PADALS) && (tran_sensor_sl_nv_state_get)) {
        if (cust_sensor_info[TRAN_SL].vendor == SensorTek) { // SensorTek
            if (cust_sensor_info[TRAN_SL].algo == CutOut) { // CutOut
                if (cust_sensor_info[TRAN_SL].ver == V1_0_0) { // V1_0_0
                    ret = transceiver_comm_with_ptr(sensor_type, SENS_COMM_CTRL_CONFIG_CMD, &nvcfg->data.field[0], ALS_NV_TRANSFER_MAX_BYTE);
                    ret = transceiver_comm_with_ptr(sensor_type, SENS_COMM_CTRL_CONFIG_CMD, &nvcfg->data.field[ALS_NV_TRANSFER_MAX_BYTE], ALS_NV_TRANSFER_MAX_BYTE);
                    ret = transceiver_comm_with_ptr(sensor_type, SENS_COMM_CTRL_CONFIG_CMD, &nvcfg->data.field[2 * ALS_NV_TRANSFER_MAX_BYTE], ALS_NV_TRANSFER_MAX_BYTE);
                }
            }
        }
    }

    // 恢复 SENSOR_TYPE_PROXIMITY 的配置
    if (sensor_type == SENSOR_TYPE_PROXIMITY) {
        ret = transceiver_comm_with_ptr(sensor_type, SENS_COMM_CTRL_CONFIG_CMD, &ps_nvcfg->data.field[0], PS_NV_TRANSFER_MAX_BYTE);
        if (ret < 0) {
            pr_err("restore config %u fail %d\n", sensor_type, ret);
        }

        ret = transceiver_comm_with_ptr(sensor_type, SENS_COMM_CTRL_CONFIG_CMD, &ps_nvcfg->data.field[PS_NV_TRANSFER_MAX_BYTE], PS_NV_TRANSFER_MAX_BYTE);
        if (ret < 0) {
            pr_err("restore config %u fail %d\n", sensor_type, ret);
        }
    } else {
        ret = transceiver_comm_with_ptr(sensor_type, SENS_COMM_CTRL_CONFIG_CMD, config_data, config_length);
    }

    return ret;
}

//USD:modify by hongliang.zhang for [sensorhub tne] 20250520 start
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
static void tran_sensor_aee_work_handler(struct work_struct *work)
{
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
    tnek_caller_api("0xffffff42", FLAG_TNE_LOG|FLAG_TNE_USER_PROPERTY, 0, "/data/vendor/aee_exp/db.fatal.08.EE/db.fatal.08.EE.dbg");
//USD:modify by weidong.liang for [trancare tne] 20251111 start
    trancare_tne_sensor_sendData();
    pr_err("trancare_tne work_handler done\n");
//USD:modify by weidong.liang for [trancare tne] 20251111 end
#endif
}
static int tran_aee_notifier_call(struct notifier_block *nb,
			unsigned long action, void *data)
{
    pr_err("tran_aee recv action %lu data = %p,sensorhub_tne_flag:%d\n",action,data,sensorhub_tne_flag);
    if(sensorhub_tne_flag == 1) {
        schedule_delayed_work(&feature_dev->tran_sensor_aee_delayed_work, 20000); //20s
    }
    return 0;
}
static struct notifier_block tran_aee_notifier = {
    .notifier_call = tran_aee_notifier_call,
};
#endif
//USD:modify by hongliang.zhang for [sensorhub tne] 20250520 end

//USD:modify by weidong.liang for [trancare tne] 20251111 start
static void trancare_tne_sensor_sendData(void)
{
    int trancare_sensor_tne_state = 0;
    trancare_sensor_tne_state = is_tid_enable(TRANCARE_TNE_SENSOR_ID);
    pr_err("tran_aee trancare state:%d\n",trancare_sensor_tne_state);
    if (trancare_sensor_tne_state) {
        struct athena_msg *msg = NULL;
        msg = alloc_msg();
        if (!msg) {
            pr_err("tran_aee failed to allocate message\n");
            return;
        }
        msg_put_string(&msg, "type", "subsystemcrash");
        msg_put_string(&msg, "reason", "sensorhub_ee");
        msg_put_int(&msg, "count", 1);
        submit_msg(&msg, 1, TRANCARE_TNE_SENSOR_ID, "subsystemcrash");
        pr_err("tran_aee trancare data success\n");
        free_msg(&msg);
    }
}
//USD:modify by weidong.liang for [trancare tne] 20251111 end

//modify for SR-202507-002286 by hongliang.zhang 20251009 start
void check_sensor_status(const struct tran_sensor_type_info *dts_types,
                        struct sensor_info *loaded_types, int loaded_count)
{
    //init sensor status
    sensor_status_t sensors[TRAN_HW_SENSOR_TYPE_MAX] = {
        {dts_types->acc,  SENSOR_STATE_NOT_SUPPORTED, "ACC"},
        {dts_types->mag,  SENSOR_STATE_NOT_SUPPORTED, "MAG"},
        {dts_types->gyro, SENSOR_STATE_NOT_SUPPORTED, "GYRO"},
        {dts_types->als,  SENSOR_STATE_NOT_SUPPORTED, "LIGHT"},
        {dts_types->ps,   SENSOR_STATE_NOT_SUPPORTED, "PROXIMITY"},
        {dts_types->sar,  SENSOR_STATE_NOT_SUPPORTED, "SAR"}
    };

    // state set to loaded_failed if not supported
    for (int i = 0; i < TRAN_HW_SENSOR_TYPE_MAX; i++) {
        if (sensors[i].expected_type != 0) {
            sensors[i].state = SENSOR_STATE_LOAD_FAILED;
        }
    }

    // update state to loaded_success if supported
    for (int i = 0; i < loaded_count; i++) {
        for (int j = 0; j < TRAN_HW_SENSOR_TYPE_MAX; j++) {
            if (loaded_types[i].sensor_type == sensors[j].expected_type) {
                sensors[j].state = SENSOR_STATE_LOAD_SUCCESS;
                break;
            }
        }
    }

    pr_info("===== Sensor Status Report =====\n");
    for (int i = 0; i < TRAN_HW_SENSOR_TYPE_MAX; i++) {
        switch (sensors[i].state) {
            case SENSOR_STATE_NOT_SUPPORTED:
                pr_info("[NOT SUPPORTED] %s\n", sensors[i].name);
                break;
            case SENSOR_STATE_LOAD_FAILED:
                pr_err("[FAILED] %s (Type:%d) configured but not loaded!\n",
                      sensors[i].name, sensors[i].expected_type);
                tranlog_local("USD_SENSOR_CHECK", "HW_%s Check_ID Fail:HW\n",
                      sensors[i].name, sensors[i].expected_type);
                break;
            case SENSOR_STATE_LOAD_SUCCESS:
                pr_info("[OK] %s (Type:%d) loaded successfully\n",
                       sensors[i].name, sensors[i].expected_type);
                tranlog_local("USD_SENSOR_CHECK", "HW_%s Check_ID SUCCESS:HW\n",
                                    sensors[i].name, sensors[i].expected_type);
                break;
        }
    }
    pr_info("================================\n");
}

//USD: add LK7KOS16-2 by yi.peng for HWC 20251010 start
void transceiver_transfer_hwc_cwb_data(int sensor_type, uint8_t *rgb_data)
{
    memset(&tran_lcm_info, 0, sizeof(struct sensor_param));
    tran_lcm_info.cust_param[0] = rgb_data[0];
    tran_lcm_info.cust_param[1] = rgb_data[1];
    tran_lcm_info.cust_param[2] = rgb_data[2];
    tran_lcm_info.cust_param[3] = sensor_type;
    //printk("[tran_sensor] transceiver_transfer_hwc_cwb_data %d %d %d %d\n",
    //    tran_lcm_info.cust_param[0], tran_lcm_info.cust_param[1], tran_lcm_info.cust_param[2], tran_lcm_info.cust_param[3]);
    transceiver_comm_with_ptr(sensor_type, SENS_COMM_CTRL_CUST_PARAM_CMD, &tran_lcm_info, sizeof(struct sensor_param));

}
//USD: add LK7KOS16-2 by yi.peng for HWC 20251010 end

void sensor_translate_arr_log(const struct share_mem_data *src)
{
    if (!src) {
        pr_err("%s: invalid input parameters\n", ARR_TAG);
        return;
    }
    switch (src->sensor_type) {
        case SENSOR_TYPE_ACCELEROMETER:
            if (src->value[0] == ENABLE_FAIL) {
                pr_info("%s:acc enable fail\n",ARR_TAG);
                tranlog_local("USD_SENSOR_CHECK","HW_ACC Check_ID Fail:HW");
            } else if (src->value[0] == ENABLE_SUCCESS) {
                pr_info("%s:acc enable success\n",ARR_TAG);
                if (src->value[1] == CALI_FAIL) {
                    if (last_arr_data.acc_cali_flag != src->value[1]) {
                        pr_info("%s:acc cali fail\n",ARR_TAG);
                        tranlog_local("USD_SENSOR_CHECK","HW_ACC No_Cali:FACTORY");
                        last_arr_data.acc_cali_flag = src->value[1];
                    }
                } else if (src->value[1] == CALI_SUCCESS && last_arr_data.acc_cali_flag != CALI_SUCCESS) {
                    pr_info("%s:acc cali success\n",ARR_TAG);
                    tranlog_local("USD_SENSOR_CHECK","HW_ACC Cali_SUCCESS:FACTORY");
                    last_arr_data.acc_cali_flag = src->value[1];
                } else if (src->value[1] == CALI_SUCCESS  && last_arr_data.acc_cali_flag == CALI_SUCCESS) {
                    if ((last_arr_data.acc[0] == src->value[2] && last_arr_data.acc[1] == src->value[3]) ||
                        (last_arr_data.acc[0] == src->value[2] && last_arr_data.acc[2] == src->value[4]) ||
                        (last_arr_data.acc[1] == src->value[3] && last_arr_data.acc[2] == src->value[4])) {
                        pr_info("%s:ACC DATA:x:%d,y:%d,z:%d\n",
                            ARR_TAG,src->value[2],src->value[3],src->value[4]);
                        tranlog_local("ACC","DATA:x:%d,y:%d,z:%d",
                            (int)src->value[2],(int)src->value[3],(int)src->value[4]);
                        last_arr_data.acc_cali_flag = src->value[1];
                        last_arr_data.acc[0] = src->value[2];
                        last_arr_data.acc[1] = src->value[3];
                        last_arr_data.acc[2] = src->value[4];
                    }
                }
            }
            break;
        case SENSOR_TYPE_MAGNETIC_FIELD:
            if (src->value[0] == ENABLE_FAIL) {
                pr_info("%s:mag enable fail\n",ARR_TAG);
                tranlog_local("USD_SENSOR_CHECK","HW_MAG Check_ID Fail:HW");
            } else if (src->value[0] == ENABLE_SUCCESS) {
                pr_info("%s:mag enable success\n",ARR_TAG);
                if (last_arr_data.mag_accuracy != src->value[1]) {
                    pr_info("%s:MAG ACCURACY:%d\n",ARR_TAG,src->value[1]);
                    tranlog_local("MAG", "ACCURACY:%d",(int)src->value[1]);
                }
                last_arr_data.mag_accuracy = src->value[1];
            }
            break;
        case SENSOR_TYPE_GYROSCOPE:
            if (src->value[0] == ENABLE_FAIL) {
                pr_info("%s:gyro enable fail\n",ARR_TAG);
                tranlog_local("USD_SENSOR_CHECK","HW_GYRO Check_ID Fail:HW");
            } else if (src->value[0] == ENABLE_SUCCESS) {
                pr_info("%s:gyro enable success\n",ARR_TAG);
                if (src->value[1] == CALI_FAIL) {
                    if(last_arr_data.gyro_cali_flag != src->value[1]) {
                        pr_info("%s:gyro cali fail\n",ARR_TAG);
                        tranlog_local("USD_SENSOR_CHECK","HW_GYRO No_Cali:FACTORY");
                    }
                } else if (src->value[1] == CALI_SUCCESS) {
                    if (last_arr_data.gyro_cali_flag != src->value[1]) {
                        pr_info("%s:gyro cali success\n",ARR_TAG);
                        tranlog_local("USD_SENSOR_CHECK","HW_GYRO Cali_SUCCESS:FACTORY");
                    }
                }
                last_arr_data.gyro_cali_flag = src->value[1];
            }
            break;
        case SENSOR_TYPE_LIGHT:
            if (src->value[0] == ENABLE_FAIL) {
                pr_info("%s:light enable fail\n",ARR_TAG);
                tranlog_local("USD_SENSOR_CHECK","HW_LIGHT Check_ID Fail:HW");
            } else if (src->value[0] == ENABLE_SUCCESS) {
                pr_info("%s:light enable success\n",ARR_TAG);
                if (src->value[1] == CALI_FAIL) {
                    if (last_arr_data.light_cali_flag != src->value[1]) {
                        pr_info("%s:light cali fail\n",ARR_TAG);
                        tranlog_local("USD_SENSOR_CHECK","HW_LIGHT No_Cali:FACTORY");
                    }
                } else if (src->value[1] == CALI_SUCCESS) {
                    if (last_arr_data.light_k != src->value[2]) {
                        pr_info("%s:light cali success\n",ARR_TAG);
                        tranlog_local("USD_SENSOR_CHECK","HW_LIGHT Cali_SUCCESS:FACTORY");
                        pr_info("%s:LIGHT K:%d\n",ARR_TAG,src->value[2]);
                        tranlog_local("LIGHT", "K:%d",(int)src->value[2]);
                    }
                }
                last_arr_data.light_cali_flag = src->value[1];
                last_arr_data.light_k = src->value[2];
            }
            break;
        case SENSOR_TYPE_PROXIMITY:
            if (src->value[0] == ENABLE_FAIL) {
                pr_info("%s:prox enable fail\n",ARR_TAG);
                tranlog_local("USD_SENSOR_CHECK","HW_PROXIMITY Check_ID Fail:HW");
            } else if (src->value[0] == ENABLE_SUCCESS) {
                pr_info("%s:prox enable success\n",ARR_TAG);
                if (src->value[1] == CALI_FAIL) {
                    if (last_arr_data.prox_cali_flag != src->value[1]) {
                        pr_info("%s:prox cali fail\n",ARR_TAG);
                        tranlog_local("USD_SENSOR_CHECK","HW_PROXIMITY No_Cali:FACTORY");
                        last_arr_data.prox_cali_flag = src->value[1];
                    }
                } else if (src->value[1] == CALI_SUCCESS && last_arr_data.prox_cali_flag != CALI_SUCCESS){
                    pr_info("%s:prox cali success\n",ARR_TAG);
                    tranlog_local("USD_SENSOR_CHECK","HW_PROXIMITY Cali_SUCCESS:FACTORY");
                    last_arr_data.prox_cali_flag = src->value[1];
                } else if (src->value[1] == CALI_SUCCESS && last_arr_data.prox_cali_flag == CALI_SUCCESS) {
                    if (last_arr_data.prox_noise != src->value[2]) {
                        pr_info("%s:prox noise:%d,psi:%d,th:%d,tl:%d\n",
                            ARR_TAG,(int)src->value[2],(int)src->value[3],(int)src->value[4],(int)src->value[5]);
                        tranlog_local("prox", "noise:%d,psi:%d,th:%d,tl:%d",
                            (int)src->value[2],(int)src->value[3],(int)src->value[4],(int)src->value[5]);
                        last_arr_data.prox_cali_flag = src->value[1];
                        last_arr_data.prox_noise = src->value[2];
                    }
                }
            }
            break;
        case SENSOR_TYPE_SAR:
            if (src->value[0] == ENABLE_FAIL) {
                pr_info("%s:sar enable fail\n",ARR_TAG);
                tranlog_local("USD_SENSOR_CHECK","HW_SAR Check_ID Fail:HW");
            } else if (src->value[0] == ENABLE_SUCCESS) {
                pr_info("%s:sar enable success\n",ARR_TAG);
            }
            break;
        default:
            pr_info("%s: no use sensor type %d\n", ARR_TAG, src->sensor_type);
            break;
    }
}
//modify for SR-202507-002286 by hongliang.zhang 20251009 end

int transceiver_features_init(void) {
    int ret = 0;

    // 分配功能模块的数据结构
    feature_dev = kzalloc(sizeof(struct transceiver_feature), GFP_KERNEL);
    if (!feature_dev) {
        pr_err("[transceiver_features] Failed to allocate memory for feature_dev\n");
        return -ENOMEM;
    }
    init_transceiver_comm_with();

    if(sensor_props.charger_poll){
        // 初始化 Charger 功能
        feature_dev->charger_poll_delay = ns_to_ktime(CHARGER_POLLING_DELAY * NSEC_PER_MSEC);
        hrtimer_init(&feature_dev->charger_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        feature_dev->charger_timer.function = charger_timer_func;
    
        feature_dev->charger_wq = create_singlethread_workqueue("charger_wq");
        if (!feature_dev->charger_wq) {
            pr_err("[setchg] Failed to create charger workqueue.");
            ret = -ENOMEM;
            goto err_free_feature_dev;
        }
        INIT_WORK(&feature_dev->work_charger, charger_work_func);
    }

    if(sensor_props.brightness_level_support){
        // 初始化 Brightness 功能
        feature_dev->brightness_poll_delay = ns_to_ktime(tran_get_lcm_info_timer * NSEC_PER_MSEC);
        hrtimer_init(&feature_dev->brightness_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        feature_dev->brightness_timer.function = brightness_timer_func;
    
        feature_dev->brightness_wq = create_singlethread_workqueue("brightness_wq");
        if (!feature_dev->brightness_wq) {
            pr_err("[get_brightness] Failed to create brightness workqueue.");
            ret = -ENOMEM;
            if(sensor_props.charger_poll){
                destroy_workqueue(feature_dev->charger_wq);
                }
            goto err_free_feature_dev;
        }
        INIT_WORK(&feature_dev->work_brightness, brightness_work_func);
    }
    
    if(sensor_props.vrefresh_support){
        // 初始化 Vrefresh 功能
        feature_dev->vrefresh_poll_delay = ns_to_ktime(VREFRESH_POLLING_DELAY * NSEC_PER_MSEC);
        hrtimer_init(&feature_dev->vrefresh_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        feature_dev->vrefresh_timer.function = vrefresh_timer_func;
    
        feature_dev->vrefresh_wq = create_singlethread_workqueue("vrefresh_wq");
        if (!feature_dev->vrefresh_wq) {
            pr_err("[vrefresh] Failed to create vrefresh workqueue.");
            ret = -ENOMEM;
            if(sensor_props.brightness_level_support){
                destroy_workqueue(feature_dev->brightness_wq);
            }
                if(sensor_props.charger_poll){
                    destroy_workqueue(feature_dev->charger_wq);
                }
                goto err_free_feature_dev;
            }
        INIT_WORK(&feature_dev->work_vrefresh, vrefresh_work_func);
    }
    
    if(sensor_props.hall_sensor){
        // 初始化 Hall 功能
        feature_dev->hall_poll_delay = ns_to_ktime(HALL_POLLING_DELAY * NSEC_PER_MSEC);
        hrtimer_init(&feature_dev->hall_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        feature_dev->hall_timer.function = hall_timer_func;
    
        feature_dev->hall_wq = create_singlethread_workqueue("hall_wq");
        if (!feature_dev->hall_wq) {
            pr_err("[hall] Failed to create hall workqueue.");
            ret = -ENOMEM;
            if(sensor_props.vrefresh_support){
                destroy_workqueue(feature_dev->vrefresh_wq);
                }
            if(sensor_props.brightness_level_support){
                destroy_workqueue(feature_dev->brightness_wq);
                }
            if(sensor_props.charger_poll){
                destroy_workqueue(feature_dev->charger_wq);
                }
            goto err_free_feature_dev;
        }
        INIT_WORK(&feature_dev->work_hall, hall_work_func);
    }
    
    if(sensor_props.cwb_support){
        // 初始化 CWB 功能
        feature_dev->cwb_poll_delay = ns_to_ktime(CWB_POLLING_DELAY * NSEC_PER_MSEC);
        hrtimer_init(&feature_dev->cwb_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        feature_dev->cwb_timer.function = cwb_timer_func;
    
        feature_dev->cwb_wq = create_singlethread_workqueue("cwb_wq");
        if (!feature_dev->cwb_wq) {
            pr_err("[cwb] Failed to create cwb workqueue.");
            ret = -ENOMEM;
        if(sensor_props.hall_sensor){
                destroy_workqueue(feature_dev->hall_wq);
            }
        if(sensor_props.vrefresh_support){
                destroy_workqueue(feature_dev->vrefresh_wq);
            }
        if(sensor_props.brightness_level_support){
                destroy_workqueue(feature_dev->brightness_wq);
            }
        if(sensor_props.charger_poll){
                destroy_workqueue(feature_dev->charger_wq);
            }
            goto err_free_feature_dev;
        }
        INIT_WORK(&feature_dev->work_cwb, cwb_work_func);
    }
//USD:modify by hongliang.zhang for [sensorhub tne] 20250520 start
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
    sensorhub_cloud_engine_init();
    pr_info("[tran_aee] sensorhub register_notifier\n");
    tran_aee_register_notifier(&tran_aee_notifier);
    INIT_DELAYED_WORK(&feature_dev->tran_sensor_aee_delayed_work, tran_sensor_aee_work_handler);
#endif
//USD:modify by hongliang.zhang for [sensorhub tne] 20250520 end

    if(sensor_props.sar_debug){
        register_accdet_notifier(&accdet_init_notifier);
    }

//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 start
    if(sensor_props.disp_notif){
        feature_dev->disp_wq = create_singlethread_workqueue("disp_wq");
        if (!feature_dev->disp_wq) {
            pr_err("[tran_disp] Failed to create disp workqueue.");
            ret = -ENOMEM;
            goto err_free_feature_dev;
        }
        INIT_WORK(&feature_dev->work_disp, tran_disp_work_func);

        // 初始化 disp_notifier 功能
        feature_dev->sensor_disp_notif.notifier_call = sensor_disp_notifier_callback;
        ret = mtk_disp_notifier_register("Sensor", &feature_dev->sensor_disp_notif);
        if (ret) {
            pr_err("[tran_disp]Unable to register sensor_disp_notif, ret:%d\n", ret);
        }
    }
//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 end
    pr_info("[transceiver_features] All features initialized successfully\n");
    return 0;

err_free_feature_dev:
    kfree(feature_dev);
    return ret;
}

void transceiver_features_exit(void) {
    if (!feature_dev)
        return;

    if(sensor_props.cwb_support){
        hrtimer_cancel(&feature_dev->cwb_timer);
        destroy_workqueue(feature_dev->cwb_wq);
    }
    
    if(sensor_props.hall_sensor){
        hrtimer_cancel(&feature_dev->hall_timer);
        destroy_workqueue(feature_dev->hall_wq);
    }
    
    if(sensor_props.vrefresh_support){
        hrtimer_cancel(&feature_dev->vrefresh_timer);
        destroy_workqueue(feature_dev->vrefresh_wq);
    }
    
    if(sensor_props.brightness_level_support){
        hrtimer_cancel(&feature_dev->brightness_timer);
        destroy_workqueue(feature_dev->brightness_wq);
    }
    
    if(sensor_props.charger_poll){
        hrtimer_cancel(&feature_dev->charger_timer);
        destroy_workqueue(feature_dev->charger_wq);
    }
//USD:modify by hongliang.zhang for [sensorhub tne] 20250520 start
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
    sensorhub_cloud_engine_exit();
    tran_aee_unregister_notifier(&tran_aee_notifier);
    cancel_delayed_work_sync(&feature_dev->tran_sensor_aee_delayed_work);
#endif
//USD:modify by hongliang.zhang for [sensorhub tne] 20250520 end

//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 start
    if(sensor_props.disp_notif){
        if (mtk_disp_notifier_unregister(&feature_dev->sensor_disp_notif))
            pr_err("[tran_sensor]Error occurred while unregistering sensor_disp_notif.");
    }
//USD: add CM5H8918-2346 get lcm supplier info by zhigang.yang 20250206 end

    kfree(feature_dev);
    pr_info("[transceiver_features] All features cleaned up successfully\n");
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Transceiver Features Module");
MODULE_AUTHOR("xihang.jin@transsion.com");
