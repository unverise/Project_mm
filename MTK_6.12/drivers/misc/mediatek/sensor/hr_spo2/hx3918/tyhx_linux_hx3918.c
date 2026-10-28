#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
//#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/version.h>
#include <linux/fs.h>   
//#include <linux/wakelock.h> 
#include <asm/io.h>
#include <linux/module.h>
#include <linux/cdev.h>
//#include <hwmsen_helper.h>
//#include <cust_eint.h>
//#include <hwmsensor.h>
//#include <sensors_io.h>
//#include <hwmsen_dev.h>
//#include <mt-plat/upmu_common.h>


#include "hx3918.h"
#include "hx3918_hrs_agc.h"
#include "hx3918_factory_test.h"
//USD: add hr_spo2 Live detection by longtao.ran 20241202 start
#include "tyhx_spo2_alg.h"
#include "tran_hr_spo2.h"
//USD: add hr_spo2 Live detection by longtao.ran 20241202 end
#define HX3918 "hx3918_heartrate"
#define HX3918E_I2C_ID 0x44
#define HEARTRATE_I2C_BUSNUM 3
/* USD add hr_spo2 Live detection by longtao.ran 20240924 start */
struct tran_hrs_controller hx3918_controller;
extern hx3918_hrs_wear_msg_code_t g_hrs_wear_status;
extern uint8_t g_hr_result;
extern uint8_t g_spo2_result;
extern spo2_living_code_t g_spo2_living_status;
extern spo2_alg_msg_code_t g_spo2_alg_status;
extern uint8_t g_motion;
/* USD add hr_spo2 Live detection by longtao.ran 20240924 end */
extern WORK_MODE_T work_mode_flag;
//USD: add NFRFP-20767 by longtao.ran for [add heart_rate_oximetry test] 20240821 start
extern factory_result_t hr_spo2_test_result;
extern uint8_t hx3918_chip_id;
//USD: add NFRFP-20767 by longtao.ran for [add heart_rate_oximetry test] 20240821 end

static int ty_heartrate_en = 0;
static int ty_spo2_en = 0 ;
unsigned int ty_heartrate_eint_open_status = 1;

#ifdef 	__report_data__
typedef struct {
	unsigned int hr_date;
	unsigned int up_date;
	unsigned int down_date;
}HX3918_report_data;
#else
unsigned int hr_date;
unsigned int up_date;
unsigned int down_date;
#endif

/* USD add hr_spo2 bring up by longtao.ran 20240823 start */
typedef struct hx3918_linux_data{
	struct i2c_client *client;
	struct input_dev *hx3918_input_dev;
	struct class *hx3918_class; 
	struct cdev *ty_cdev;
	dev_t ty_devno;
	struct device *hx3918_device;
	//HX3918_report_data *hx3918_raw_data;
	struct mutex sys_mutex;
	struct mutex bus_lock;
	u8 *bus_tx_buf;
	u8 *bus_rx_buf;
}hx3918_linux_data_t;
/* USD add hr_spo2 bring up by longtao.ran 20240823 end */

static hx3918_linux_data_t   hx3918data;
#ifdef 	__report_data__
static HX3918_report_data hx3918_report_date;
#endif
/* USD add hr_spo2 bring up by longtao.ran 20241129 start */
struct ldr_channel_data_t ldr_channel_data;
struct cust_hr_spo2_data_t cust_hr_spo2_data;
struct hr_spo2_cfg_t hr_spo2_cfg;
const int32_t  SPO2_IR_WEAR = 20000;
const int32_t  SPO2_IR_NO_WEAR = 15000;
const int32_t  LIV_MODE = 1;
const int32_t  LIV_QU = 15;
const int32_t  LIV_ST = 25;
const  uint8_t   COUNT_BLOCK_NUM = 50;            //delay the block of some single good signal after a series of bad signal
const  uint8_t   SPO2_LOW_XCORR_THRE = 30;        //(64*xcorr)'s square below this threshold, means error signal
const  uint8_t   SPO2_CALI = 1;                       //spo2_result cali mode
const  uint8_t   XCORR_MODE = 1;                  //xcorr mode switch
const  uint8_t   QUICK_RESULT = 1;                //come out the spo2 result quickly ;0 is normal,1 is quick
const  uint16_t  MEAN_NUM = 32;                  //the length of smooth-average ;the value of MEAN_NUM can be given only 256 and 512
const  uint8_t   G_SENSOR = 0;                      //if =1, open the gsensor mode
const  uint8_t   SPO2_GSEN_POW_THRE = 150;         //gsen pow judge move, range:0-200;
const  uint32_t  SPO2_BASE_LINE_INIT = 123000;    //spo2 baseline init, = 103000 + ratio(a variable quantity,depends on different cases)*SPO2_SLOPE
const  int32_t   SOP2_DEGLITCH_THRE = 100000;     //remove signal glitch over this threshold
const  int32_t   SPO2_REMOVE_JUMP_THRE = 100000;  //remove signal jump over this threshold
const  uint32_t  SPO2_SLOPE = 50000;              //increase this slope, spo2 reduce more
const  uint16_t  SPO2_LOW_CLIP_END_TIME = 1500;   //low clip mode before this data_cnt, normal clip mode after this
const  uint16_t  SPO2_LOW_CLIP_DN  = 150;         //spo2 reduce 0.15/s at most in low clip mode
const  uint16_t  SPO2_NORMAL_CLIP_DN  = 500;      //spo2 reduce 0.5/s at most in normal clip mode
const  uint8_t   SPO2_LOW_SNR_THRE = 40;          //snr below this threshold, means error signal
const  uint16_t  IR_AC_TOUCH_THRE = 200;          //AC_min*0.3
const  uint16_t  IR_FFT_POW_THRE = 200;           //fft_pow_min
const  uint8_t   SLOPE_PARA_MAX = 30;
const  uint8_t   SLOPE_PARA_MIN = 3;
const  uint32_t   AMB_THRE = 300000;
/* USD add hr_spo2 bring up by longtao.ran 20241129 end */
/* USD modify by honglian.zhang for hr_spo2 20251024 start */
const  uint8_t   GSENSOR_DATA_NO_SUPPORT = 0;
/* USD modify by honglian.zhang for hr_spo2 20251024 end */

//static struct i2c_client * ty_Heartrate_I2Cclient = NULL;
//static struct i2c_board_info  __initdata heartrate_dev={ I2C_BOARD_INFO(HX3918, HX3918E_I2C_ID)};
static const struct i2c_device_id hx3918_i2c_id[] = {{HX3918, 0},{}};
static struct hrtimer hx3918_heartrate_ctimer ; 
static struct hrtimer hx3918_agc_ctimer ;
static struct work_struct hx3918_heartrate_work; 
static struct work_struct hx3918_agc_work;
extern void heart_rate_meas_timeout_handler(void * p_context);
extern void hx3918_spo2_disable(void);
//USD: add hx3918 drv by longtao.ran 20241202 start
extern int register_hr_spo2_controller(struct tran_hrs_controller *controller);
extern int tran_spo2_register(void);
extern int tran_hr_register(void);
extern int tran_hr_destroy(void);
extern int tran_spo2_destroy(void);
//USD: add hx3918 drv by longtao.ran 20241202 end

//extern void spo2_presure_meas_timeout_handler(void);
static void hx3918_init_heartrate (int enable);
static void hx3918_init_spo2 (int enable);
void hx3918_input_ist(void);

/*=================================[begin]===== sysfs =======================================*/

static ssize_t heartrate_enable_show(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t heartrate_enable_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t spo2_result_show(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t spo2_result_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t spo2_enable_show(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t spo2_enable_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t blood_up_show(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t blood_up_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t blood_down_show(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t  blood_down_store (struct device* dev, struct device_attribute* attr, const char* buf,size_t count);
static ssize_t hr_result_show(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t  hr_result_store (struct device* dev, struct device_attribute* attr, const char* buf,size_t count);
static ssize_t wear_status_show(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t wear_status_store (struct device* dev, struct device_attribute* attr, const char* buf,size_t count);
static ssize_t set_mode_show(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t set_mode_store (struct device* dev, struct device_attribute* attr, const char* buf,size_t count);
//USD: add NFRFP-20767 by longtao.ran for [add heart_rate_oximetry test] 20241022 start
static ssize_t hr_spo2_vendor_show(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t hr_spo2_vendor_store (struct device* dev, struct device_attribute* attr, const char* buf,size_t count);
static ssize_t hr_spo2_test_show(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t hr_spo2_test_store (struct device* dev, struct device_attribute* attr, const char* buf,size_t count);
static ssize_t hr_spo2_cust_value_show(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t hr_spo2_cust_value_store(struct device* dev, struct device_attribute* attr, const char* buf,size_t count);
//USD: add NFRFP-20767 by longtao.ran for [add heart_rate_oximetry test] 20241022 end

static DEVICE_ATTR(hrs_enable, S_IWUSR | S_IWGRP | S_IRUGO, heartrate_enable_show, heartrate_enable_store);
static DEVICE_ATTR(spo2_enable, S_IWUSR | S_IWGRP | S_IRUGO, spo2_enable_show, spo2_enable_store);
static DEVICE_ATTR(spo2_value,S_IWUSR | S_IWGRP | S_IRUGO, spo2_result_show, spo2_result_store); 
static DEVICE_ATTR(up,S_IWUSR | S_IWGRP | S_IRUGO, blood_up_show,blood_up_store);
static DEVICE_ATTR(down, S_IWUSR | S_IWGRP | S_IRUGO, blood_down_show, blood_down_store);
static DEVICE_ATTR(hr_result, S_IWUSR | S_IWGRP | S_IRUGO, hr_result_show, hr_result_store);
static DEVICE_ATTR(wear_status, S_IWUSR | S_IWGRP | S_IRUGO,wear_status_show, wear_status_store);
static DEVICE_ATTR(set_mode, S_IWUSR | S_IWGRP | S_IRUGO,set_mode_show, set_mode_store);
//USD: add NFRFP-20767 by longtao.ran for [add heart_rate_oximetry test] 20241022 start
static DEVICE_ATTR(hr_spo2_test, S_IWUSR | S_IWGRP | S_IRUGO,hr_spo2_test_show, hr_spo2_test_store);
static DEVICE_ATTR(hr_spo2_vendor, S_IWUSR | S_IWGRP | S_IRUGO,hr_spo2_vendor_show, hr_spo2_vendor_store);
static DEVICE_ATTR(hr_spo2_cust_value, S_IWUSR | S_IWGRP | S_IRUGO,hr_spo2_cust_value_show, hr_spo2_cust_value_store);

static struct device_attribute  *hx3918_attr_list[]={
	&dev_attr_hrs_enable,
	&dev_attr_spo2_enable,
	&dev_attr_spo2_value,
	&dev_attr_up,
	&dev_attr_down,
	&dev_attr_hr_result,
	&dev_attr_wear_status,
	&dev_attr_set_mode,
	&dev_attr_hr_spo2_test,
	&dev_attr_hr_spo2_vendor,
	&dev_attr_hr_spo2_cust_value,
};
//USD: add NFRFP-20767 by longtao.ran for [add heart_rate_oximetry test] 20241022 end

static ssize_t heartrate_enable_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	ssize_t res ;
	res = snprintf (buf, PAGE_SIZE, "%d\n", ty_heartrate_en); 
	return res ;
}
static ssize_t heartrate_enable_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	DEBUG_PRINTF ("[begin] buf[0] = %d ty_heartrate_en = %d \n", buf[0],  ty_heartrate_en);

	if((buf != NULL) && ('1' == buf[0]))
	{
		if (ty_heartrate_en == 0){
			hx3918_init_heartrate(1);
			ty_heartrate_en =1;
		}
	} 
	else {
			if (ty_heartrate_en == 1){
			hx3918_init_heartrate(0);
			ty_heartrate_en =0;
			}
	}		
	DEBUG_PRINTF ("[end] ty_heartrate_en = %d buf[0] = %d \n", ty_heartrate_en,buf[0]);
	return count;
}

void tran_hr_enable(int enable)
{
    if(enable < 0 || enable >1){
        DEBUG_PRINTF ("Invalid value! enable = %d \n", enable);
        return ;
    }
    if(1 == enable)
    {
        if (ty_heartrate_en == 0){
            hx3918_init_heartrate(1);
            ty_heartrate_en =1;
        }
    } 
    else {
        if (ty_heartrate_en == 1){
            hx3918_init_heartrate(0);
            ty_heartrate_en =0;
        }
    }
    DEBUG_PRINTF ("enable = %d ty_heartrate_en = %d \n", enable,  ty_heartrate_en);
    return;
}

static ssize_t spo2_enable_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	ssize_t res ;
	res = snprintf (buf, PAGE_SIZE, "%d\n", ty_spo2_en); 
	return res ;
}

static ssize_t spo2_enable_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	DEBUG_PRINTF ("[begin] buf[0] = %d ty_spo2_en = %d \n", buf[0],  ty_spo2_en);

	if((buf != NULL) && ('1' == buf[0]))
	{
		if (ty_spo2_en == 0){
			hx3918_init_spo2(1);
			ty_spo2_en =1;
		}
	} 
	else {
			if (ty_spo2_en == 1){
				hx3918_init_spo2(0);
				ty_spo2_en =0;
			}
	}		
	DEBUG_PRINTF ("[end] ty_spo2_en = %d buf[0] = %d \n", ty_spo2_en,buf[0]);
	return count;
}

void tran_spo2_enable(int enable)
{
    if(enable < 0 || enable >1){
        DEBUG_PRINTF ("Invalid value! enable = %d \n", enable);
        return ;
    }
    if(1 == enable)
    {
        if (ty_spo2_en == 0){
            hx3918_init_spo2(1);
            ty_spo2_en =1;
        }
    } 
    else {
        if (ty_spo2_en == 1){
            hx3918_init_spo2(0);
            ty_spo2_en =0;
        }
    }
    DEBUG_PRINTF ("enable = %d ty_spo2_en = %d \n", enable,  ty_spo2_en);
    return;
}

static ssize_t blood_up_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	ssize_t res ;
	#ifdef 	__report_data__
	res = snprintf(buf, PAGE_SIZE,"%d\n", hx3918_report_date.up_date);
	#else
	res = snprintf (buf, PAGE_SIZE,"%d\n", up_date);
	#endif
	return res ;
}

static ssize_t blood_up_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	return 0 ;
}

static ssize_t blood_down_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	ssize_t res ;
#ifdef 	__report_data__	
	res = snprintf(buf, PAGE_SIZE,"%d\n", hx3918_report_date.down_date);
#else
	res = snprintf (buf, PAGE_SIZE,"%d\n", down_date);
#endif
	return res ;
}

static ssize_t blood_down_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	return 0 ;
}

static ssize_t hr_result_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	ssize_t res ;
	DEBUG_PRINTF("hr_result_show:g_hr_result = %d\n", g_hr_result);
	mutex_lock(&hx3918data.sys_mutex);
	res = snprintf(buf, PAGE_SIZE, "%d\n", g_hr_result);
	mutex_unlock(&hx3918data.sys_mutex);
	return res ;
}

static ssize_t hr_result_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	return 0 ;
}

static ssize_t spo2_result_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	ssize_t res ;
	DEBUG_PRINTF("hr_result_show:g_hr_result = %d\n", g_spo2_result);
	mutex_lock(&hx3918data.sys_mutex);
	res = snprintf(buf, PAGE_SIZE, "%d\n", g_spo2_result);
	mutex_unlock(&hx3918data.sys_mutex);
	return res ;
}

static ssize_t spo2_result_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	return 0 ;
}

static ssize_t wear_status_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	ssize_t res ;
	DEBUG_PRINTF("wear_status_show:g_hr_result = %d\n", g_hrs_wear_status);
	mutex_lock(&hx3918data.sys_mutex);
	res = snprintf(buf, PAGE_SIZE, "%d\n", g_hrs_wear_status);
	mutex_unlock(&hx3918data.sys_mutex);
	return res ;
}

static ssize_t wear_status_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	return 0 ;
}

static ssize_t set_mode_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	ssize_t res ;
	DEBUG_PRINTF("set_mode_show:work_mode_flag = %d\n", work_mode_flag);
	mutex_lock(&hx3918data.sys_mutex);
	res = snprintf(buf, PAGE_SIZE, "%d\n", work_mode_flag);
	mutex_unlock(&hx3918data.sys_mutex);
	return res ;
}

static ssize_t set_mode_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	int mode = 0;
	mode = buf[0] - 48;
	DEBUG_PRINTF ("[begin] mode = %d\n", mode);
	hx3918_init(mode);
	return count;
}
//USD: add NFRFP-20767 by longtao.ran for [heart_rate_oximetry Customize values] 20241022 start
static ssize_t hr_spo2_cust_value_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	ssize_t res ;
	mutex_lock(&hx3918data.sys_mutex);
	res = snprintf(buf, PAGE_SIZE, "cust_enable = %d, spo2_result = %d, hr_result = %d, wear_status =%d, living_status =%d, alg_status =%d, motion =%d\n", \
		cust_hr_spo2_data.cust_enable, \
		cust_hr_spo2_data.spo2_result, \
		cust_hr_spo2_data.hr_result, \
		cust_hr_spo2_data.hrs_wear_status, \
		cust_hr_spo2_data.spo2_living_status, \
		cust_hr_spo2_data.spo2_alg_status, \
		cust_hr_spo2_data.motion);
	mutex_unlock(&hx3918data.sys_mutex);
	return res ;
}

static ssize_t hr_spo2_cust_value_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	int ret ;
	int i = 0;
	char buff[50];
	int temp_ivalue[7];
	char *token;
	char *remaining;
	const char delim[] = " "; // Separator

	if (count > 48) {
		DEBUG_PRINTF("Length out of bounds\n.");
		return -EPERM;
	}
	ret = snprintf(buff, count, "%s", buf);
	if (ret < 0) {
		DEBUG_PRINTF("copy_from_user failed %s\n.", buf);
		return -EPERM;
	}
	remaining = buff;
	memset(temp_ivalue, 0, sizeof(temp_ivalue));
	while ((token = strsep(&remaining, delim)) != NULL) {
		ret = kstrtoint(token, 10, &temp_ivalue[i]);
		if (ret) {
			DEBUG_PRINTF("Failed to convert string to int\n");
		}
		i++;
		if (i > 7) {
			DEBUG_PRINTF("Length out of bounds\n");
			break;
		}
	}
	if ((buf != NULL) && ('1' == buf[0])) {
		cust_hr_spo2_data.cust_enable = 1;
		cust_hr_spo2_data.spo2_result = temp_ivalue[1];
		cust_hr_spo2_data.hr_result = temp_ivalue[2];
		cust_hr_spo2_data.hrs_wear_status = temp_ivalue[3];
		cust_hr_spo2_data.spo2_living_status = temp_ivalue[4];
		cust_hr_spo2_data.spo2_alg_status = temp_ivalue[5];
		cust_hr_spo2_data.motion = temp_ivalue[6];
	} else {
		cust_hr_spo2_data.cust_enable = 0;
	}
	DEBUG_PRINTF ("cust_info: cust_enable = %d, spo2_result = %d, hr_result = %d, wear_status =%d, living_status =%d, alg_status =%d, motion =%d\n", \
		cust_hr_spo2_data.cust_enable, \
		cust_hr_spo2_data.spo2_result, \
		cust_hr_spo2_data.hr_result, \
		cust_hr_spo2_data.hrs_wear_status, \
		cust_hr_spo2_data.spo2_living_status, \
		cust_hr_spo2_data.spo2_alg_status, \
		cust_hr_spo2_data.motion);

	return count;
}
//USD: add NFRFP-20767 by longtao.ran for [heart_rate_oximetry Customize values] 20241022 end

//USD: add NFRFP-20767 by longtao.ran for [add heart_rate_oximetry test] 20240910 start
static ssize_t hr_spo2_vendor_show(struct device* dev, struct device_attribute* attr, char* buf)
{
    ssize_t res ;
    bool check_rst = false;
    DEBUG_PRINTF("chip_id:%d\n",hx3918_chip_id);
    check_rst = hx3918_chip_check();
    mutex_lock(&hx3918data.sys_mutex);
    if (hx3918_chip_id == 0x27){
        res = snprintf(buf, PAGE_SIZE, "tyhx_hx3918\n");
    } else {
        res = snprintf(buf, PAGE_SIZE, "UNKNOWN\n");
    }
    mutex_unlock(&hx3918data.sys_mutex);
    return res ;
}

static ssize_t hr_spo2_vendor_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
    return 0;
}

static ssize_t hr_spo2_test_show(struct device* dev, struct device_attribute* attr, char* buf)
{
    ssize_t res ;
    DEBUG_PRINTF("%d %d %d\n",hr_spo2_test_result.green_data_final, hr_spo2_test_result.red_data_final,hr_spo2_test_result.ir_data_final);
    mutex_lock(&hx3918data.sys_mutex);
    res = snprintf(buf, PAGE_SIZE, "%d %d %d\n",hr_spo2_test_result.green_data_final, hr_spo2_test_result.red_data_final,hr_spo2_test_result.ir_data_final);
    mutex_unlock(&hx3918data.sys_mutex);
    return res ;
}

static ssize_t hr_spo2_test_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
    int mode = 0;
    mode = buf[0] - 48;
    if(mode != 0 && mode != 1){
        DEBUG_PRINTF("parameter transmission error");
        return -EINVAL;
    }
    /* transsion customization */
    /* enable = 0:FT_LEAK_LIGHT_MODE    enable = 1:FT_GRAY_CARD_MODE */
    DEBUG_PRINTF ("[begin] mode = %d\n", mode + 6);
    hx3918_init(mode + 6);
    hx3918_hrs_set_mode(PPG_OFF);
    return count;
}
//USD: add NFRFP-20767 by longtao.ran for [add heart_rate_oximetry test] 20240910 end
/*======================================[end]===== sysfs =======================================*/


/*=================================[begin] i2c ========================================*/
/* USD add hr_spo2 bring up by longtao.ran 20240920 start */
#if 0
static int hx3918_i2c_read(u8 reg, u8 *data, int len)
{
	u8 write_buf[8] = {0}; 

	int rc;
	write_buf[0] = reg;
	rc = i2c_master_send(hx3918data.client, write_buf, 1); //If everything went ok (i.e. 1 msg transmitted), return tbytes B 
	if (rc != 1) {
		DEBUG_PRINTF ("FAILED: writing to address 0x%x\n", reg);
		return -1;
	}

	rc = i2c_master_recv(hx3918data.client, data, len);	// returns negative errno, or else the number of bytes read
	if (rc  < 0) {
		DEBUG_PRINTF ("FAILED: reading address0x%x data \n", reg);
		return -1;
	}
	
	return rc;

}
#endif
static int hx3918_i2c_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen)
{
    int ret = 0;
    int i = 0;
    hx3918_linux_data_t *hs_data = &hx3918data;
    struct i2c_msg msg_list[2];
    struct i2c_msg *msg = NULL;
    int msg_num = 0;

    /* must have data when read */
    if (!hs_data || !hs_data->client || !data || !datalen
        || (datalen > I2C_BUF_LENGTH) || (cmdlen > I2C_BUF_LENGTH)) {
        DEBUG_PRINTF("fhs_data/client/cmdlen(%d)/data/datalen(%d) is invalid",cmdlen, datalen);
        return -EINVAL;
    }

    mutex_lock(&hs_data->bus_lock);
    memset(&msg_list[0], 0, sizeof(struct i2c_msg));
    memset(&msg_list[1], 0, sizeof(struct i2c_msg));
    memcpy(hs_data->bus_tx_buf, cmd, cmdlen);
    msg_list[0].addr = hs_data->client->addr;
    msg_list[0].flags = 0;
    msg_list[0].len = cmdlen;
    msg_list[0].buf = hs_data->bus_tx_buf;
    msg_list[1].addr = hs_data->client->addr;
    msg_list[1].flags = I2C_M_RD;
    msg_list[1].len = datalen;
    msg_list[1].buf = hs_data->bus_rx_buf;
    if (cmd && cmdlen) {
        msg = &msg_list[0];
        msg_num = 2;
    } else {
        msg = &msg_list[1];
        msg_num = 1;
    }

    for (i = 0; i < I2C_RETRY_NUMBER; i++) {
        ret = i2c_transfer(hs_data->client->adapter, msg, msg_num);
        if (ret < 0) {
            DEBUG_PRINTF("i2c_transfer(read) fail,ret:%d", ret);
        } else {
            memcpy(data, hs_data->bus_rx_buf, datalen);
            break;
        }
    }

    mutex_unlock(&hs_data->bus_lock);
    return ret;
}
int hx3918_i2c_write(u8 *writebuf, u32 writelen)
{
    int ret = 0;
    int i = 0;
    hx3918_linux_data_t *hs_data = &hx3918data;
    struct i2c_msg msgs;

    if (!hs_data || !hs_data->client || !writebuf || !writelen
        || (writelen > I2C_BUF_LENGTH)) {
        DEBUG_PRINTF("hs_data/client/data/datalen(%d) is invalid", writelen);
        return -EINVAL;
    }

    mutex_lock(&hs_data->bus_lock);
    memset(&msgs, 0, sizeof(struct i2c_msg));
    memcpy(hs_data->bus_tx_buf, writebuf, writelen);
    msgs.addr = hs_data->client->addr;
    msgs.flags = 0;
    msgs.len = writelen;
    msgs.buf = hs_data->bus_tx_buf;
    for (i = 0; i < I2C_RETRY_NUMBER; i++) {
        ret = i2c_transfer(hs_data->client->adapter, &msgs, 1);
        if (ret < 0) {
            DEBUG_PRINTF("i2c_transfer(write) fail,ret:%d", ret);
        } else {
            break;
        }
    }
    mutex_unlock(&hs_data->bus_lock);
    return ret;
}
#if 0
static int hx3918_i2c_write(u8 reg, u8 *data, int len)
{
	u8 buf[20] = {0}; 
	int rc = 0;
	int ret = 0;
	int i;
	buf[0] = reg;
	if (len >= 20) {
		DEBUG_PRINTF("FAILED: buffer size is limitted(20) %d\n", len);
		return -1;
	}

	for( i=0 ; i<len; i++ ) {
		buf[i+1] = data[i];
	}	
	
	rc = i2c_master_send(hx3918data.client, buf, len+1);	// Returns negative errno, or else the number of bytes written.
	if (rc != len+1) {
		DEBUG_PRINTF("FAILED: writing to reg 0x%x\n", reg);
		ret = -1;
	}
	return ret;
}
#endif

void hx3918_delay_us(uint32_t us)
{
	udelay(us);
}



bool hx3918_write_reg(uint8_t addr, uint8_t data) 
{
    u8 buf[2] = { 0 };

    buf[0] = addr;
    buf[1] = data;
    hx3918_i2c_write(buf, sizeof(buf));
    return 0; 
}
uint8_t hx3918_read_reg(uint8_t addr) 
{
	unsigned char data_buf = 0;
	hx3918_i2c_read(&addr, 1, &data_buf, 1);
	return data_buf;
}

bool hx3918_brust_read_reg(uint8_t addr , uint8_t *buf, uint8_t length) 
{
    hx3918_i2c_read(&addr, 1, buf, length);
    return true;
}
/* USD add hr_spo2 bring up by longtao.ran 20240920 end */

void hx3918_ppg_timer_cfg(bool en)
{

    if(en)
    {
        //schedule_work (&hx3918_heartrate_work);
		hrtimer_start(&hx3918_heartrate_ctimer,ktime_set(0, 320*1000000), HRTIMER_MODE_REL);	// 40ms
    }
    else
    {
        hrtimer_cancel(&hx3918_heartrate_ctimer);
		
    }

}
void hx3918_agc_timer_cfg(bool en)
{

    if(en)
    {
        //schedule_work(&hx3918_agc_work);
		hrtimer_start(&hx3918_agc_ctimer,ktime_set(0, 40*1000000), HRTIMER_MODE_REL);	// 20ms  
    }
    else
    { 
      hrtimer_cancel(&hx3918_agc_ctimer); 
    }

}
void hx3918_gpioint_cfg(bool en)
{
#if defined(INT_MODE)
    if(en)
    {
         hx3918_gpioint_enable();
    }
    else
    {
         hx3918_gpioint_disable();
    }
#endif

}
    


/*================= [begin] spo2 heartrate init ===========================================*/

static void hx3918_init_heartrate (int enable) // heartrate init
{
	if (enable == 0 ){
		DEBUG_PRINTF ("[begin] disable chip IC \n");
		//hrtimer_cancel(&hx3918_heartrate_ctimer);
		//hrtimer_cancel(&hx3918_agc_ctimer);
		//mdelay(2);
		hx3918_hrs_disable();
		//hwPowerDown (MT6323_POWER_LDO_VGP2, "heartrate");
		
		DEBUG_PRINTF ("[end] disable chip IC \n");
	} else if (enable == 1) { // \B3\F5ʼо\C6
		DEBUG_PRINTF ("[begin] enable chip IC \n");
		//hwPowerOn(MT6323_POWER_LDO_VGP2, VOL_3000, "heart_rate");
		hx3918_init(HRS_MODE);
		//hrtimer_start(&hx3918_heartrate_ctimer,ktime_set(0, 40*1000000), HRTIMER_MODE_REL);
		DEBUG_PRINTF ("[end] enable chip IC \n");
	} else {
		DEBUG_PRINTF ("enable is error \n");
	}
}

static void hx3918_init_spo2 (int enable)
{
	/* USD add hr_spo2 Live detection by longtao.ran 20240924 start */
	g_hr_result = 0;
	g_spo2_result = 0;
	g_hrs_wear_status = 0;
	g_spo2_living_status = 0;
	g_spo2_alg_status = 0;
	g_motion = 0;
	/* USD add hr_spo2 Live detection by longtao.ran 20240924 end */

	if (enable == 0 ){
		DEBUG_PRINTF ("disable chip IC \n");
		hx3918_spo2_disable();

	} else if (enable == 1) {	// \B3\F5ʼоƬ
		DEBUG_PRINTF ("enable chip IC \n");
		//hwPowerOn(MT6323_POWER_LDO_VGP2, VOL_3000, "heart_rate"); //VOL-3000 
		hx3918_init(SPO2_MODE);
		//hrtimer_start(&hx3918_heartrate_ctimer,ktime_set(0, 20*1000000), HRTIMER_MODE_REL); 
	}else {
		DEBUG_PRINTF ("enable is error \n");
	}

}

/*================= [begin] spo2 heartrate ========================================*/

static ssize_t ty_heartrate_read(struct file *filp,char *buf,size_t count,loff_t *f_ops)
{
	static int heartrate=0;
	if (! count) {
		if(copy_to_user (buf, &heartrate, count))
			DEBUG_PRINTF ("copy_to_user failed\n");
	}
	DEBUG_PRINTF ("heartrate =%d\n ", heartrate);
	return 0;
}

static ssize_t  ty_heartrate_write(struct file *filp ,const char *buf, size_t count, loff_t *f_ops)
{
	DEBUG_PRINTF (">>> %s	(%d) \n", __func__	, __LINE__);
	return 0;
}

static int ty_heartrate_open(struct inode *inode, struct file *filp)
{	
	DEBUG_PRINTF (">>>>%s	(%d) \n", __func__	, __LINE__);
	hx3918_init_heartrate(1);
	return 0;
}

static int ty_heartrate_release(struct inode *inode, struct file *filp)
{	
	DEBUG_PRINTF (">>>>%s (%d) \n", 	__func__, __LINE__	);
	return 0;
}

static struct file_operations ty_Heartrate_fpos=
{
	owner:	THIS_MODULE,
	read: 	ty_heartrate_read,
	write:	ty_heartrate_write,
	open:	ty_heartrate_open,
	release:	ty_heartrate_release,
};

static enum hrtimer_restart hx3918_hearterate_timer_func(struct hrtimer  *timer)
{
	schedule_work (&hx3918_heartrate_work);
	hx3918_ppg_timer_cfg(1);
	return HRTIMER_NORESTART ;
}

static enum hrtimer_restart hx3918_spo2_timer_func(struct hrtimer *timer)
{
	schedule_work(&hx3918_agc_work);
	hx3918_agc_timer_cfg(1);
	return HRTIMER_NORESTART ;
}

static void hx3918_heartrate_work_func(struct work_struct *work)
{
	//DEBUG_PRINTF ("enter	>>>>%s	\n", 	__func__);
	hx3918_ppg_Int_handle();
	//hx3918_input_ist();
	//DEBUG_PRINTF ("end >>>> %s \n", __func__);
}

static void hx3918_agc_work_func(struct work_struct *work)
{
//	DEBUG_PRINTF ("enter	>>>>	%s	\n", 	__func__);
	agc_timeout_handler(NULL);
//	DEBUG_PRINTF ("end >>>> %s \n", __func__);
}

static int hx3918_create_attr(struct device *dev)
{
	int idx, err = 0;
	int num = (int)(sizeof(hx3918_attr_list) / sizeof (hx3918_attr_list[0]));
	if (!dev)
	return -EINVAL;
	
	DEBUG_PRINTF("[gomtel bes](int)(sizeof(hx3918_attr_list) / sizeof(hx3918_attr_list[0])) = %d\n",num); 
	for (idx = 0; idx < num; idx++) {
		err = device_create_file (dev, hx3918_attr_list[idx]); 
		if (err) {
			DEBUG_PRINTF("[gomtel bes]device_create_file (%s) = %d\n", hx3918_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int hx3918_open(struct input_dev *dev)
{
	DEBUG_PRINTF(">>>%s (%d) \n", __func__, __LINE__);

	return 0 ;
}

static void hx3918_close(struct input_dev *dev)
{
	DEBUG_PRINTF(">>>%s (%d) \n", __func__, __LINE__);

}

void hx3918_input_ist(void)
{
	DEBUG_PRINTF("enter >>> %s \n", __func__); 
	mutex_lock(&hx3918data.sys_mutex);
	
	input_report_abs(hx3918data.hx3918_input_dev, ABS_X, (hr_date*16));
	input_report_abs(hx3918data.hx3918_input_dev,ABS_Y, (hr_date*16));
	input_report_abs(hx3918data.hx3918_input_dev,ABS_Z, (hr_date*16));
	
	input_report_abs(hx3918data.hx3918_input_dev,ABS_RX, hr_date);
	input_report_abs(hx3918data.hx3918_input_dev,ABS_RY, hr_date); 
	input_report_abs(hx3918data.hx3918_input_dev,ABS_RZ, hr_date);
	input_sync(hx3918data.hx3918_input_dev);
	mutex_unlock(&hx3918data.sys_mutex);
	DEBUG_PRINTF("end >>> %s \n", __func__);
}

static int hx3918_init_input_data(void)
{
	int ret = 0;
	DEBUG_PRINTF("%s (%d) : initialize data\n",__func__ , __LINE__);
	hx3918data.hx3918_input_dev = input_allocate_device();
	if (!hx3918data.hx3918_input_dev){
		DEBUG_PRINTF("%s %d) : could not allocate mouse input device\n", __func__	, __LINE__);
		return -ENOMEM;
	}
	
	hx3918data.hx3918_input_dev->evbit[0]= BIT_MASK(EV_ABS);
	hx3918data.hx3918_input_dev->absbit[0]= BIT_MASK(ABS_X) |BIT_MASK(ABS_Y) | BIT_MASK(ABS_Z) | BIT_MASK(ABS_RX) |  BIT_MASK(ABS_RY) |  BIT_MASK(ABS_RZ) ;

	input_abs_set_max(hx3918data .hx3918_input_dev,ABS_X, 0xffffffff);
	input_abs_set_max(hx3918data .hx3918_input_dev,ABS_Y, 0xffffffff);
	input_abs_set_max(hx3918data .hx3918_input_dev,ABS_Z, 0xffffffff);
	input_abs_set_max(hx3918data .hx3918_input_dev,ABS_RX, 0xffffffff);
	input_abs_set_max(hx3918data .hx3918_input_dev,ABS_RY, 0xffffffff);
	input_abs_set_max(hx3918data .hx3918_input_dev,ABS_RZ, 0xffffffff);

	input_set_drvdata(hx3918data.hx3918_input_dev, &hx3918data);
	hx3918data.hx3918_input_dev->name = "hx3918";
	hx3918data.hx3918_input_dev->open =hx3918_open;
	hx3918data.hx3918_input_dev->close =hx3918_close;
	
	ret = input_register_device(hx3918data.hx3918_input_dev);
	if (ret < 0) {
		input_free_device(hx3918data.hx3918_input_dev);
		DEBUG_PRINTF("could not register input device\n");
		return ret;
	}
	return 0;
}

static int hx3918_parse_dt(struct device *dev)
{
    struct device_node *dt_node = dev->of_node;
    int irq_gpio = 0;
    int power_gpio = 0;
    int ret = 0;
    /* USD add hr_spo2 bring up by longtao.ran 20241119 start */
    uint8_t ldrsel[3];
    uint8_t slope_para[2];
    int32_t wear_thres[2];
    uint8_t living_cfg[3];
    uint16_t spo2_clip_arr[3];

    if (NULL == dt_node) {
        DEBUG_PRINTF ("[heartrate ]No DTS node\n");
        return -ENODEV;
    }

//TODO: 这只是个示例。按需解析，按需使用
    irq_gpio = of_get_named_gpio(dt_node, "irq-gpios", 0);
    if(irq_gpio < 0) {
        DEBUG_PRINTF ("[heartrate ]failed to get irq_gpio from DT\n");
        return -1;
    }

	
    power_gpio = of_get_named_gpio(dt_node, "power-gpios", 0);
    if(power_gpio < 0) {
        DEBUG_PRINTF ("[heartrate ]failed to get irq_gpio from DT\n");
        return -1;
    }

	if (gpio_is_valid(power_gpio)) {
		ret = gpio_request(power_gpio, "hx_power");
		if (ret) {
		    DEBUG_PRINTF("[GPIO]reset gpio request failed");
		}

		ret = gpio_direction_output(power_gpio, 1);
		if (ret) {
		    DEBUG_PRINTF("[GPIO]set_direction for reset gpio failed");
		}
		DEBUG_PRINTF("power on");
	}

    DEBUG_PRINTF ("[heartrate ]irq_gpio=%d\n", irq_gpio);
    /* ldrsel=2: ldr2 ，ldrsel=1:  ldr1, ldrsel=4: ldr3, GREEN_LED_SLE = IR_LED_SLE */
    ret = of_property_read_u8_array(dt_node, "hx3918_ldr_channel",ldrsel, ARRAY_SIZE(ldrsel));
    if (ret != 0){
        ldr_channel_data.green_led_sle = GREEN_LED_SLE;
        ldr_channel_data.red_led_sle = RED_LED_SLE;
        ldr_channel_data.ir_led_sle = IR_LED_SLE;
        DEBUG_PRINTF("hx3918_ldr_channel not found, use default configuration");
    } else{
        ldr_channel_data.green_led_sle = ldrsel[0];
        ldr_channel_data.red_led_sle = ldrsel[1];
        ldr_channel_data.ir_led_sle = ldrsel[2];
    }
    DEBUG_PRINTF ("[ldr_channel_data]green=%d red=%d ir=%d\n", \
        ldr_channel_data.green_led_sle, \
        ldr_channel_data.red_led_sle, \
        ldr_channel_data.ir_led_sle);
	ret = of_property_read_u32_array(dt_node, "hx3918_spo2_ir_wear_thres", wear_thres, ARRAY_SIZE(wear_thres));
	if (ret != 0){
		hr_spo2_cfg.spo2_ir_wear_thres = SPO2_IR_WEAR;
		hr_spo2_cfg.spo2_ir_no_wear_thres = SPO2_IR_NO_WEAR;
		DEBUG_PRINTF("hx3918_spo2_ir_wear_thres not found, use default configuration");
	} else{
		hr_spo2_cfg.spo2_ir_wear_thres = wear_thres[0];
		hr_spo2_cfg.spo2_ir_no_wear_thres = wear_thres[1];
		DEBUG_PRINTF("[hx3918_spo2_ir_wear_thres]min=%d max=%d\n", \
			hr_spo2_cfg.spo2_ir_wear_thres,\
			hr_spo2_cfg.spo2_ir_no_wear_thres);
	}
	ret = of_property_read_u8_array(dt_node, "hx3918_slope_para", slope_para, ARRAY_SIZE(slope_para));
	if (ret != 0){
		hr_spo2_cfg.slope_para_min = SLOPE_PARA_MIN;
		hr_spo2_cfg.slope_para_max = SLOPE_PARA_MAX;
		DEBUG_PRINTF("hx3918_slope_para not found, use default configuration");
	} else{
		hr_spo2_cfg.slope_para_min = slope_para[0];
		hr_spo2_cfg.slope_para_max = slope_para[1];
		DEBUG_PRINTF("[hx3918_slope_para]min=%d max=%d\n", \
			hr_spo2_cfg.slope_para_min,\
			hr_spo2_cfg.slope_para_max);
	}
	ret = of_property_read_u8_array(dt_node, "hx3918_living_cfg", living_cfg, ARRAY_SIZE(living_cfg));
	if (ret != 0){
		hr_spo2_cfg.living_mode = LIV_MODE;
		hr_spo2_cfg.qu_thre = LIV_QU;
		hr_spo2_cfg.st_thre = LIV_ST;
		DEBUG_PRINTF("hx3918_living_cfg not found, use default configuration");
	} else{
		hr_spo2_cfg.living_mode = living_cfg[0];
		hr_spo2_cfg.qu_thre = living_cfg[1];
		hr_spo2_cfg.st_thre = living_cfg[2];
		DEBUG_PRINTF("[hx3918_living_cfg]living_mode=%d qu_thre=%d st_thre=%d\n", \
			hr_spo2_cfg.living_mode,\
			hr_spo2_cfg.qu_thre,\
			hr_spo2_cfg.st_thre);
	}
	ret = of_property_read_u16_array(dt_node, "hx3918_spo2_clip", spo2_clip_arr, ARRAY_SIZE(spo2_clip_arr));
	if (ret != 0){
		hr_spo2_cfg.spo2_low_clip_end_time = SPO2_LOW_CLIP_END_TIME;
		hr_spo2_cfg.spo2_low_clip_dn = SPO2_LOW_CLIP_DN;
		hr_spo2_cfg.spo2_normal_clip_dn = SPO2_NORMAL_CLIP_DN;
		DEBUG_PRINTF("hx3918_spo2_clip not found, use default configuration");
	} else{
		hr_spo2_cfg.spo2_low_clip_end_time = spo2_clip_arr[0];
		hr_spo2_cfg.spo2_low_clip_dn = spo2_clip_arr[1];
		hr_spo2_cfg.spo2_normal_clip_dn = spo2_clip_arr[2];
		DEBUG_PRINTF("[hx3918_spo2_clip]clip_end_time=%d clip_dn=%d spo2_normal_clip_dn=%d\n", \
			hr_spo2_cfg.spo2_low_clip_end_time,\
			hr_spo2_cfg.spo2_low_clip_dn,\
			hr_spo2_cfg.spo2_normal_clip_dn);
	}
	ret = of_property_read_u8(dt_node, "hx3918_count_block_num", &hr_spo2_cfg.count_block_num);
	if (ret != 0){
		hr_spo2_cfg.count_block_num = COUNT_BLOCK_NUM;
		DEBUG_PRINTF("hx3918_count_block_num not found, use default configuration");
	} else {
		DEBUG_PRINTF("[hx3918_slope_para]count_block_num=%d\n", hr_spo2_cfg.count_block_num);
	}
	ret = of_property_read_u8(dt_node, "hx3918_spo2_low_xcorr_thre", &hr_spo2_cfg.spo2_low_xcorr_thre);
	if (ret != 0){
		hr_spo2_cfg.spo2_low_xcorr_thre = SPO2_LOW_XCORR_THRE;
		DEBUG_PRINTF("hx3918_spo2_low_xcorr_thre not found, use default configuration");
	} else {
		DEBUG_PRINTF("[hx3918_spo2_low_xcorr_thre]spo2_low_xcorr_thre=%d\n", hr_spo2_cfg.spo2_low_xcorr_thre);
	}
	ret = of_property_read_u8(dt_node, "hx3918_quick_result", &hr_spo2_cfg.quick_result);
	if (ret != 0){
		hr_spo2_cfg.quick_result = QUICK_RESULT;
		DEBUG_PRINTF("hx3918_quick_result not found, use default configuration");
	} else {
		DEBUG_PRINTF("[hx3918_quick_result]quick_result=%d\n", hr_spo2_cfg.quick_result);
	}
	ret = of_property_read_u16(dt_node, "hx3918_mean_num", &hr_spo2_cfg.mean_num);
	if (ret != 0){
		hr_spo2_cfg.mean_num = MEAN_NUM;
		DEBUG_PRINTF("hx3918_mean_num not found, use default configuration");
	} else {
		DEBUG_PRINTF("[hx3918_mean_num]mean_num=%d\n", hr_spo2_cfg.mean_num);
	}
	ret = of_property_read_u8(dt_node, "hx3918_spo2_gsen_pow_thre", &hr_spo2_cfg.spo2_gsen_pow_thre);
	if (ret != 0){
		hr_spo2_cfg.spo2_gsen_pow_thre = SPO2_GSEN_POW_THRE;
		DEBUG_PRINTF("hx3918_spo2_gsen_pow_thre not found, use default configuration");
	} else {
		DEBUG_PRINTF("[hx3918_spo2_gsen_pow_thre]spo2_gsen_pow_thre=%d\n", hr_spo2_cfg.spo2_gsen_pow_thre);
	}
	ret = of_property_read_u32(dt_node, "hx3918_spo2_base_line_init", &hr_spo2_cfg.spo2_base_line_init);
	if (ret != 0){
		hr_spo2_cfg.spo2_base_line_init = SPO2_BASE_LINE_INIT;
		DEBUG_PRINTF("hx3918_spo2_base_line_init not found, use default configuration");
	} else {
		DEBUG_PRINTF("[hx3918_spo2_base_line_init]spo2_base_line_init=%d\n", hr_spo2_cfg.spo2_base_line_init);
	}
	ret = of_property_read_u32(dt_node, "hx3918_spo2_slope", &hr_spo2_cfg.spo2_slope);
	if (ret != 0){
		hr_spo2_cfg.spo2_slope = SPO2_SLOPE;
		DEBUG_PRINTF("hx3918_spo2_slope not found, use default configuration");
	} else {
		DEBUG_PRINTF("[hx3918_spo2_slope]spo2_slope=%d\n", hr_spo2_cfg.spo2_slope);
	}
	ret = of_property_read_u32(dt_node, "hx3918_spo2_deglitch_thre", &hr_spo2_cfg.spo2_deglitch_thre);
	if (ret != 0){
		hr_spo2_cfg.spo2_deglitch_thre = SOP2_DEGLITCH_THRE;
		DEBUG_PRINTF("hx3918_spo2_deglitch_thre not found, use default configuration");
	} else {
		DEBUG_PRINTF("[hx3918_spo2_deglitch_thre]spo2_deglitch_thre=%d\n", hr_spo2_cfg.spo2_deglitch_thre);
	}
	ret = of_property_read_u32(dt_node, "hx3918_spo2_remove_jump_thre", &hr_spo2_cfg.spo2_remove_jump_thre);
	if (ret != 0){
		hr_spo2_cfg.spo2_remove_jump_thre = SPO2_REMOVE_JUMP_THRE;
		DEBUG_PRINTF("hx3918_spo2_remove_jump_thre not found, use default configuration");
	} else {
		DEBUG_PRINTF("[hx3918_spo2_remove_jump_thre]spo2_remove_jump_thre=%d\n", hr_spo2_cfg.spo2_remove_jump_thre);
	}
	ret = of_property_read_u8(dt_node, "hx3918_spo2_low_snr_thre", &hr_spo2_cfg.spo2_low_snr_thre);
	if (ret != 0){
		hr_spo2_cfg.spo2_low_snr_thre = SPO2_LOW_SNR_THRE;
		DEBUG_PRINTF("hx3918_spo2_low_snr_thre not found, use default configuration");
	} else {
		DEBUG_PRINTF("[hx3918_spo2_low_snr_thre]spo2_low_snr_thre=%d\n", hr_spo2_cfg.spo2_low_snr_thre);
	}
	ret = of_property_read_u32(dt_node, "hx3918_spo2_amb_thre", &hr_spo2_cfg.amb_thre);
	if (ret != 0){
		hr_spo2_cfg.amb_thre = AMB_THRE;
		DEBUG_PRINTF("hx3918_spo2_amb_thre not found, use default configuration");
	} else {
		DEBUG_PRINTF("[hx3918_spo2_amb_thre]amb_thre=%d\n", hr_spo2_cfg.amb_thre);
	}
    /* USD add hr_spo2 bring up by longtao.ran 20241129 end */
	/* USD modify by honglian.zhang for hr_spo2 20251024 start */
	ret = of_property_read_u8(dt_node, "hx3918_gsensor_data_support", &hr_spo2_cfg.gsensor_data_support);
	if (ret != 0) {
		hr_spo2_cfg.gsensor_data_support = GSENSOR_DATA_NO_SUPPORT;
		DEBUG_PRINTF("gsensor_data_support not found, use default configuration");
	} else {
		DEBUG_PRINTF("gsensor_data_support=%d\n", hr_spo2_cfg.gsensor_data_support);
	}
	/* USD modify by honglian.zhang for hr_spo2 20251024 end */
    return 0;
}

/* USD add hr_spo2 bring up by longtao.ran 20240920 start */
int hx3918_i2c_bus_prepare(hx3918_linux_data_t *hs_data)
{
    DEBUG_PRINTF("hx3918_i2c_bus_prepare start\n");
    //spin_lock_init(&hs_data->irq_lock);
    //mutex_init(&hs_data->report_mutex);
    mutex_init(&hs_data->bus_lock);
    if (!i2c_check_functionality(hs_data->client->adapter, I2C_FUNC_I2C)) {
        DEBUG_PRINTF("I2C not supported");
        return -ENODEV;
    }

    if (hs_data->client->addr != HS_I2C_SLAVE_ADDR) {
        DEBUG_PRINTF("Change i2c addr 0x%02x to %x\n",
                 hs_data->client->addr, HS_I2C_SLAVE_ADDR);
        hs_data->client->addr = HS_I2C_SLAVE_ADDR;
        DEBUG_PRINTF("i2c addr=0x%x\n", hs_data->client->addr);
    }

    hs_data->bus_tx_buf = kzalloc(I2C_BUF_LENGTH, GFP_KERNEL);
    if (NULL == hs_data->bus_tx_buf) {
        DEBUG_PRINTF("failed to allocate memory for bus_tx_buf\n");
        return -ENOMEM;
    }

    hs_data->bus_rx_buf = kzalloc(I2C_BUF_LENGTH, GFP_KERNEL);
    if (NULL == hs_data->bus_rx_buf) {
        DEBUG_PRINTF("failed to allocate memory for bus_rx_buf\n");
        return -ENOMEM;
    }
    DEBUG_PRINTF("hx3918_i2c_bus_prepare end\n");
    return 0;
}
int hx3918_i2c_bus_exit(hx3918_linux_data_t *hs_data)
{
    DEBUG_PRINTF("hx3918_i2c_bus_exit start\n");
    if (hs_data && hs_data->bus_tx_buf) {
        kfree(hs_data->bus_tx_buf);
        hs_data->bus_tx_buf = NULL;
    }

    if (hs_data && hs_data->bus_rx_buf) {
        kfree(hs_data->bus_rx_buf);
        hs_data->bus_rx_buf = NULL;
    }
    DEBUG_PRINTF("hx3918_i2c_bus_exit end\n");
    return 0;
}
/* USD add hr_spo2 bring up by longtao.ran 20240920 end */
/* USD add hr_spo2 bring up by longtao.ran 20241119 start */
void hx3918_hr_spo2_data_get (struct tran_hr_spo2_data *data)
{
	data->spo2_result = g_spo2_result;
	data->hr_result = g_hr_result;
	data->hrs_wear_status = (uint8_t)g_hrs_wear_status;
	data->spo2_living_status = (uint8_t)g_spo2_living_status;
	data->spo2_alg_status = (uint8_t)g_spo2_alg_status;
	data->motion = g_motion;
}
void hx3918_cust_data_get (struct tran_cust_hr_spo2_data *data)
{
	data->cust_enable = cust_hr_spo2_data.cust_enable;
	data->spo2_result = cust_hr_spo2_data.spo2_result;
	data->hr_result = cust_hr_spo2_data.hr_result;
	data->hrs_wear_status = cust_hr_spo2_data.hrs_wear_status;
	data->spo2_living_status = cust_hr_spo2_data.spo2_living_status;
	data->spo2_alg_status = cust_hr_spo2_data.spo2_alg_status;
	data->motion = cust_hr_spo2_data.motion;
}

struct tran_hr_spo2_ops controller_ops = {
	.tran_hr_enable = hx3918_init_heartrate,
	.tran_spo2_enable = hx3918_init_spo2,
	.tran_hr_spo2_data_get = hx3918_hr_spo2_data_get,
	.tran_cust_data_get = hx3918_cust_data_get,
};
#if 0
struct tran_hr_spo2_data tran_hx3918_data = {
	.spo2_result = g_spo2_result,
	.hr_result = g_hr_result,
	.hrs_wear_status = g_hrs_wear_status,
	.spo2_living_status = g_spo2_living_status,
	.spo2_alg_status = g_spo2_alg_status,
	.motion = g_motion,
};

struct tran_cust_hr_spo2_data tran_hx3918_cust_data = {
	.cust_enable = cust_hr_spo2_data.cust_enable;
	.spo2_result = cust_hr_spo2_data.spo2_result,
	.hr_result = cust_hr_spo2_data.hr_result,
	.hrs_wear_status = cust_hr_spo2_data.hrs_wear_status,
	.spo2_living_status = cust_hr_spo2_data.spo2_living_status,
	.spo2_alg_status = cust_hr_spo2_data.spo2_alg_status,
	.motion = cust_hr_spo2_data.motion,
};
#endif
struct tran_hrs_controller hx3918_controller = {
	.ops = &controller_ops,
//	.data = &tran_hx3918_data,
//	.cust_data = &tran_hx3918_cust_data,
};
/* USD add hr_spo2 bring up by longtao.ran 20241119 end */

static int hx3918_i2c_probe(struct i2c_client* client)
{
    int ret =-1;
// struct device *ty-dev = NULL;
	//int res ;
	//uint8_t ret =0;
	DEBUG_PRINTF("hx3918_i2c_probe start\n");
	memset(&hx3918data, 0, sizeof(hx3918data));
	hx3918data.client = client;
    /* USD add hr_spo2 bring up by longtao.ran 20241119 start */
    memset(&ldr_channel_data, 0, sizeof(ldr_channel_data));
    memset(&cust_hr_spo2_data, 0, sizeof(cust_hr_spo2_data));
    memset(&hr_spo2_cfg, 0, sizeof(hr_spo2_cfg));
    ret = hx3918_i2c_bus_prepare(&hx3918data);
    if (ret < 0) {
        DEBUG_PRINTF ("[heartrate ]hx3918_i2c_bus_prepare failed\n");
        goto out;
    }
    /* USD add hr_spo2 bring up by longtao.ran 20241119 end */
    ret = hx3918_parse_dt(&client->dev);//yasin: power, irq, regs
    if (ret) {
        DEBUG_PRINTF ("[heartrate ]hx3918_parse_dt failed\n");
        ret = -ENODEV;
        goto out;
    }

	if(alloc_chrdev_region(&hx3918data.ty_devno, 0, 1, HX3918))
	{
		DEBUG_PRINTF ("[heartrate ] Allocate device no failed\n");
		goto out;
	}
	hx3918data.ty_cdev=cdev_alloc();
	if(NULL ==hx3918data.ty_cdev){
		DEBUG_PRINTF ("[heartrate ] Allocate mem for kobjecr failed\n"); 
		goto out1;
	}
	cdev_init(hx3918data.ty_cdev, &ty_Heartrate_fpos); 
	hx3918data.ty_cdev->owner = THIS_MODULE;
//Add to system
	if (cdev_add(hx3918data.ty_cdev, hx3918data.ty_devno, 1))
	{
		DEBUG_PRINTF ("[heartrate ] Attatch file operation failed\n");
		goto out2;
	}
	
	hx3918data.hx3918_class = class_create("hrdrv");
	if (IS_ERR(hx3918data.hx3918_class)){
		int ret= PTR_ERR(hx3918data.hx3918_class);
		DEBUG_PRINTF ("[heartrate] Unable to create classf err = %d\n", ret); 
		goto out3;
	}

	hx3918data.hx3918_device = device_create(hx3918data.hx3918_class, NULL, hx3918data.ty_devno, NULL, HX3918);
	dev_set_drvdata(hx3918data.hx3918_device, &hx3918data);
	if(NULL == hx3918data.hx3918_device){
		goto out4;
	}
	#ifdef __report_data__
		hx3918_report_date.hr_date = 0;
		hx3918_report_date.up_date = 0;
		hx3918_report_date.down_date = 0;
	#else
		hr_date=0; 
		up_date =0;
		down_date =0;
		g_hr_result = 0;
		g_spo2_result = 0;
	#endif
	hx3918_create_attr(hx3918data.hx3918_device);
	if(hx3918_init_input_data()< 0){ 
		goto out5;
	}
	
	mutex_init(&hx3918data.sys_mutex);
	hrtimer_init(&hx3918_heartrate_ctimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hx3918_heartrate_ctimer.function = hx3918_hearterate_timer_func;
	hrtimer_init(&hx3918_agc_ctimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hx3918_agc_ctimer.function = hx3918_spo2_timer_func;
	INIT_WORK(&hx3918_heartrate_work, hx3918_heartrate_work_func);
	INIT_WORK(&hx3918_agc_work, hx3918_agc_work_func);
	//ret = HX3918_read_reg(0x00);
	//DEBUG_PRINTF("read HX3918 = 0x%x\n",ret);
	/* USD add hr_spo2 bring up by longtao.ran 20241119 start */
	if (hx3918_chip_check() == true)
	{
		DEBUG_PRINTF("hreatrate i2c probe success\n");
		register_hr_spo2_controller(&hx3918_controller);
		tran_hr_register();
		tran_spo2_register();
		return 0;
	}
	/* USD add hr_spo2 bring up by longtao.ran 20241119 end */
	DEBUG_PRINTF("hreatrate i2c probe failed\n");
out5:
out4:
	device_destroy(hx3918data.hx3918_class,hx3918data.ty_devno);
out3:
	class_destroy(hx3918data.hx3918_class);
out2:
	cdev_del(hx3918data.ty_cdev); 
out1:
	unregister_chrdev_region(hx3918data.ty_devno, 1);
out:
	/* USD add hr_spo2 bring up by longtao.ran 20240920 start */
	kfree_safe(hx3918data.bus_tx_buf);
	kfree_safe(hx3918data.bus_rx_buf);
	/* USD add hr_spo2 bring up by longtao.ran 20240920 end */
	return -1 ;

}

static void  hx3918_i2c_remove (struct i2c_client *client){
	device_destroy (hx3918data.hx3918_class,  hx3918data.ty_devno);
	class_destroy(hx3918data.hx3918_class);
	cdev_del(hx3918data.ty_cdev);
	hrtimer_cancel(&hx3918_heartrate_ctimer);
	hrtimer_cancel(&hx3918_agc_ctimer);
	unregister_chrdev_region(hx3918data.ty_devno, 1);
	//USD: add hx3918 drv by longtao.ran 20241202 start
	tran_hr_destroy();
	tran_spo2_destroy();
	hx3918_i2c_bus_exit(&hx3918data);
	//USD: add hx3918 drv by longtao.ran 20241202 end
	return;
}

#ifdef CONFIG_OF
static const struct of_device_id hx3918_of_match[] = {
    {.compatible = "tyhx,hx3918"},
    {}
};
#endif
struct i2c_driver hx3918_i2c_driver={
	.probe = hx3918_i2c_probe,
	.remove = hx3918_i2c_remove,
	.id_table =  hx3918_i2c_id,
    .driver = {
        .name = "HX3918",
        //.pm = &hx3918_pm_ops,
#ifdef CONFIG_OF
        .of_match_table = hx3918_of_match,
#endif
    },
};

static int 	__init  hx3918_heartrate_init(void)
{
	DEBUG_PRINTF ("init module\n");
	//i2c_register_board_info (HEARTRATE_I2C_BUSNUM, &heartrate_dev, 1); 
	return i2c_add_driver(&hx3918_i2c_driver);
}

static void exit_hx3918_heartrate_exit(void)
{
	i2c_del_driver(&hx3918_i2c_driver);
	DEBUG_PRINTF("heartrate  ty_heartrate_exit - end\n");
}

module_init(hx3918_heartrate_init);
module_exit(exit_hx3918_heartrate_exit);
MODULE_AUTHOR("Andy");
MODULE_DESCRIPTION("Andy hx3918 driver");
MODULE_LICENSE("GPL");

