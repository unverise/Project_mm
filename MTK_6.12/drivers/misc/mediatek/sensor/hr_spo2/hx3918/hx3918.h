#ifndef _hx3918_H_
#define _hx3918_H_

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

//#include <stdint.h>
//#include <stdbool.h>


//#include "tyhx_mems.h"
//#define DEBUG_PPG_ONLY     // ֻ��IR �ж�����ͷ����, ������Ӧ���ܹر�,  ����HX3918_PROX.CҪ�������
/********************* alg_swi *********************
HRS_ALG_LIB   ���ʹ���
SPO2_ALG_LIB  Ѫ������
HRV_ALG_LIB   HRV����
CHECK_TOUCH_LIB   ��������ܣ�������Ѫ��ģʽ��
CHECK_LIVING_LIB  ������ʶ���ܣ�������Ѫ��ģʽ��
BP_CUSTDOWN_ALG_LIB  Ѫѹ����

TIMMER_MODE  ��ʱ��ģʽ�� ��PPG���ݣ� Ĭ�ϲ��ö�ʱ��ģʽ
INT_MODE     �ж�ģʽ����PPG����

DEMO_MI      demo������ã� �ͻ��رգ� ������else���������
HRS_BLE_APP  DEMO�������ƺ꣬ �ͻ��ر�
SPO2_DATA_CALI   Ѫ��DC����ģʽ�� �ͻ��ر�
HRS_DEBUG    ���Ժ꣬ ����ƽ̨�����ӡ����
AGC_DEBUG    ���Ժ꣬ ����ƽ̨�����ӡ����
GSEN_40MS_TIMMER   DEMO����ʹ�ã� �ͻ��ر�
NEW_GSEN_SCHME     DEMO����ʹ�ã� �ͻ��ر�
*/
#define HRS_ALG_LIB
#define SPO2_ALG_LIB
//#define HRV_ALG_LIB
//#define CHECK_TOUCH_LIB
//#define CHECK_LIVING_LIB
//#define SAR_ALG_LIB
#define BP_CUSTDOWN_ALG_LIB

#define NO_MATH_LIB

//**************** read_data_mode ******************//
#define TIMMER_MODE           //timmer read fifo
//#define INT_MODE							//fifo_all_most_full
//#define TYHX_DEMO           // for ty demo
//****************** gsen_cgf *********************//
//USD: add hx3918 Movement amplitude by longtao.ran 20240924 start
/* USD modify by honglian.zhang for hr_spo2 20251118 start */
#define GSENSER_DATA
/* USD modify by honglian.zhang for hr_spo2 20251118 end */
//#define GSEN_40MS_TIMMER
//USD: add hx3918 Movement amplitude by longtao.ran 20240924 end
//#define NEW_GSEN_SCHME

//****************** other_cgf ********************//
#define DEMO_COMMON
//#define TWON3IN1LED
//#define EVB
//#define INT_FLOATING
//#define HRS_BLE_APP
//#define SPO2_DATA_CALI

//***************** vecter_swi ********************//
//#define SPO2_VECTOR
//#define HR_VECTOR
//#define HRV_TESTVEC

//**************** lab_test_mode ******************//
//#define LAB_TEST             
//#define LAB_TEST_AGC							

//****************** print_swi ********************//
//#define TYHX_DEBUG
#define AGC_DEBUG
#define HRS_DEBUG

/* USD add hr_spo2 bring up by longtao.ran 20240920 start */
#define I2C_RETRY_NUMBER                    3
#define I2C_BUF_LENGTH                      4096
#define HS_I2C_SLAVE_ADDR                  0x44
#define kfree_safe(pbuf) do {\
    if (pbuf) {\
        kfree(pbuf);\
        pbuf = NULL;\
    }\
} while(0)
/* USD add hr_spo2 bring up by longtao.ran 20240920 end */

//**************************************************//

#ifdef TYHX_DEBUG
#define  TYHX_LOG(format,x...)     printk(KERN_INFO "[TYHX_DBG][HR_SPO2][%04d][%s] " format, __LINE__, __func__, ## x); 
#else
#define	 TYHX_LOG(...)
#endif

#ifdef AGC_DEBUG
#define  AGC_LOG(format,x...)    printk(KERN_INFO "[TYHX_DBG][HR_SPO2][%04d][%s] " format, __LINE__, __func__, ## x); 
#else
#define	 AGC_LOG(...)
#endif

#ifdef HRS_DEBUG
#define  DEBUG_PRINTF(format,x...)    printk(KERN_INFO "[TYHX_DBG][HR_SPO2][%04d][%s] " format, __LINE__, __func__, ## x); 
#else
#define	 DEBUG_PRINTF(...)
#endif


#if 0
//#include <stdint.h>
/* portable 8-bit unsigned integer */
typedef unsigned char           uint8_t;
/* portable 8-bit signed integer */
typedef signed char             int8_t;
/* portable 16-bit unsigned integer */
typedef unsigned short int      uint16_t;
/* portable 16-bit signed integer */
typedef signed short int        int16_t;
/* portable 32-bit unsigned integer */
typedef unsigned int            uint32_t;
/* portable 32-bit signed integer */
typedef signed int              int32_t;
#endif

#ifndef  bool
#define  bool unsigned char
#endif
#ifndef  true
#define  true  1
#endif
#ifndef  false
#define  false  0
#endif

//#define  NULL  0

//#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
typedef struct
{
    uint16_t      lastesthrs;  			//??????????
    uint8_t       lastestspo2;			//????
    uint8_t       lastesthrv;			//HRV??
    uint16_t      wearstatus;
	uint32_t      living_data_cnt;
	uint8_t       living_signal_quality;
	uint8_t       living_motion_status;
	int32_t 	  leak_green;
	int32_t 	  leak_red;
	int32_t	      leak_ir;
	int32_t 	  card_green;
	int32_t 	  card_red;
	int32_t	  	  card_ir;
	uint8_t       hrv_tired_value;
	uint8_t       hrv_spirit_pressure;
	uint8_t       hr_living; 
    uint32_t      hr_living_qu;
    uint32_t      hr_living_std;
	uint8_t       living_status;                //living_status
} tyhx_hrsresult_t;

#if defined(DEMO_COMMON)
    #define HRS4100_IIC_CLK  30
    #define HRS4100_IIC_SDA  0   
    #define LIS3DH_IIC_CLK   18
    #define LIS3DH_IIC_SDA   16
    #define EXT_INT_PIN      1
    #define GREEN_LED_SLE    2
    #define RED_LED_SLE      4
    #define IR_LED_SLE       2
    #define RED_AGC_OFFSET   12
    #define IR_AGC_OFFSET    12
    #define GREEN_AGC_OFFSET    8
    #define HR_GREEN_AGC_OFFSET 8
    #define BIG_SCREEN  
#elif defined(TWON3IN1LED)
   #define HRS4100_IIC_CLK  30
    #define HRS4100_IIC_SDA  0   
    #define LIS3DH_IIC_CLK   18
    #define LIS3DH_IIC_SDA   16
    #define EXT_INT_PIN      1
    #define GREEN_LED_SLE    2
    #define RED_LED_SLE      4
    #define IR_LED_SLE       1
    #define RED_AGC_OFFSET   12
    #define IR_AGC_OFFSET    12
    #define GREEN_AGC_OFFSET    8
    #define HR_GREEN_AGC_OFFSET 8
    #define BIG_SCREEN  
#elif defined(DEMO_NEW)
  #define HRS4100_IIC_CLK  0
  #define HRS4100_IIC_SDA  30   
  #define LIS3DH_IIC_CLK   18
  #define LIS3DH_IIC_SDA   16
  #define EXT_INT_PIN      1
  #define GREEN_LED_SLE    1
  #define RED_LED_SLE      2
  #define IR_LED_SLE       4
  #define RED_AGC_OFFSET   50
  #define IR_AGC_OFFSET    50
	#define HR_GREEN_AGC_OFFSET 4
  #define BIG_SCREEN
  
#elif defined(DEMO_BAND)
  #define HRS4100_IIC_CLK  4
  #define HRS4100_IIC_SDA  3  
  #define LIS3DH_IIC_CLK   14
  #define LIS3DH_IIC_SDA   12
  #define EXT_INT_PIN      7
  #define GREEN_LED_SLE    0
  #define RED_LED_SLE      2
  #define IR_LED_SLE       1
  #define RED_AGC_OFFSET   64
  #define IR_AGC_OFFSET    64
  #define GREEN_AGC_OFFSET 8
	#define HR_GREEN_AGC_OFFSET 8
  
#elif defined(DEMO_GT01)
  #define HRS4100_IIC_CLK  5
  #define HRS4100_IIC_SDA  6
  #define LIS3DH_IIC_CLK   14
  #define LIS3DH_IIC_SDA   12 
  #define EXT_INT_PIN      7
  #define GREEN_LED_SLE    1
  #define RED_LED_SLE      4
  #define IR_LED_SLE       2
  #define RED_AGC_OFFSET   32
  #define IR_AGC_OFFSET    32
  #define GREEN_AGC_OFFSET 8
  #define BIG_SCREEN
  
#elif defined(EVB)
  #define HRS4100_IIC_CLK  9
  #define HRS4100_IIC_SDA  10  
  #define LIS3DH_IIC_CLK   13
  #define LIS3DH_IIC_SDA   14
  #define EXT_INT_PIN      11
  #define GREEN_LED_SLE    1
  #define RED_LED_SLE      4
  #define IR_LED_SLE       2
  #define RED_AGC_OFFSET   64
  #define IR_AGC_OFFSET    64
  #define GREEN_AGC_OFFSET 8
	#define HR_GREEN_AGC_OFFSET 10
  
#else
  #define HRS4100_IIC_CLK  9
  #define HRS4100_IIC_SDA  10  
  #define LIS3DH_IIC_CLK   13
  #define LIS3DH_IIC_SDA   14
  #define EXT_INT_PIN      11
  #define GREEN_LED_SLE    1
  #define RED_LED_SLE      4
  #define IR_LED_SLE       2
  #define RED_AGC_OFFSET   64
  #define IR_AGC_OFFSET    64
  #define GREEN_AGC_OFFSET 8
  
#endif


typedef enum {    
	PPG_INIT, 
	PPG_OFF, 
	PPG_LED_OFF,
	CAL_INIT,
	CAL_OFF,
	RECAL_INIT    
} hx3918_mode_t;

typedef enum {    
	SENSOR_OK, 
	SENSOR_OP_FAILED,   
} SENSOR_ERROR_T;

typedef enum {    
	HRS_MODE, 
	SPO2_MODE,
	WEAR_MODE,
	HRV_MODE,
	LIVING_MODE,
	LAB_TEST_MODE,
	FT_LEAK_LIGHT_MODE,
	FT_GRAY_CARD_MODE,
	FT_INT_TEST_MODE,
    NULL_MODE
} WORK_MODE_T;
/* USD add hr_spo2 bring up by longtao.ran 20241119 start */
typedef struct {
	uint8_t count;
    int32_t green_data[32];
	int32_t red_data[16];
	int32_t ir_data[16];  
	int32_t s_buf[64];
	int32_t amb_data[32];
    int32_t sar_data;
	uint8_t green_cur;
	uint8_t red_cur; 
	uint8_t ir_cur;
	uint8_t green_offset;
	uint8_t red_offset; 
	uint8_t ir_offset;     
}ppg_sensor_data_t;
/* USD add hr_spo2 bring up by longtao.ran 20241119 end */

typedef enum {
	MSG_LIVING_NO_WEAR,
	MSG_LIVING_WEAR
} hx3918_living_wear_msg_code_t;

typedef enum {
	MSG_NO_WEAR,
	MSG_WEAR
} hx3918_wear_msg_code_t;

typedef struct {
	hx3918_living_wear_msg_code_t  wear_status;
	uint32_t           data_cnt;
	uint8_t            signal_quality;
	uint8_t            motion_status; 
} hx3918_living_results_t;

typedef struct {
    int32_t p1_noise; 
	int32_t p2_noise; 
	int32_t p3_noise;
	int32_t p4_noise;
}NOISE_PS_T;
/* USD add hr_spo2 bring up by longtao.ran 20241119 start */
struct ldr_channel_data_t {
	uint8_t green_led_sle;
	uint8_t red_led_sle; 
	uint8_t ir_led_sle;
};
struct cust_hr_spo2_data_t{
	uint8_t cust_enable;
	uint8_t spo2_result;
	uint8_t hr_result;
	uint8_t hrs_wear_status;
	uint8_t spo2_living_status;
	uint8_t spo2_alg_status;
	uint8_t motion;
};
struct hr_spo2_cfg_t{
	//wear
	uint8_t spo2_check_unwear_oft;
	int32_t spo2_ir_wear_thres;
	int32_t spo2_ir_no_wear_thres;

	//living
	uint8_t living_mode;
	uint8_t qu_thre;
	uint8_t st_thre;

	// spo2 para and switches
	uint8_t count_block_num;             //delay the block of some single good signal after a series of bad signal
	uint8_t spo2_low_xcorr_thre;         //(64*xcorr)'s square below this threshold, means error signal
	uint8_t spo2_cali;                   //spo2_result cali mode
	uint8_t xcorr_mode;                  //xcorr mode switch
	uint8_t quick_result;                //come out the spo2 result quickly ;0 is normal,1 is quick
	uint16_t mean_num;                   //the length of smooth-average ;the value of MEAN_NUM can be given only 256 and 512
	//uint8_t   G_SENSOR = 0;            //if =1, open the gsensor mode
	uint8_t spo2_gsen_pow_thre;          //gsen pow judge move, range:0-200;
	uint32_t spo2_base_line_init;        //spo2 baseline init, = 103000 + ratio(a variable quantity,depends on different cases)*SPO2_SLOPE
	int32_t spo2_deglitch_thre;          //remove signal glitch over this threshold
	int32_t spo2_remove_jump_thre;       //remove signal jump over this threshold
	uint32_t spo2_slope;                 //increase this slope, spo2 reduce more
	uint16_t spo2_low_clip_end_time;     //low clip mode before this data_cnt, normal clip mode after this
	uint16_t spo2_low_clip_dn;           //spo2 reduce 0.15/s at most in low clip mode
	uint16_t spo2_normal_clip_dn;        //spo2 reduce 0.5/s at most in normal clip mode
	uint8_t  spo2_low_snr_thre;          //snr below this threshold, means error signal
	uint16_t ir_ac_touch_thre;           //AC_min*0.3
	uint16_t ir_fft_pow_thre;            //fft_pow_min
	uint8_t  slope_para_max;
	uint8_t  slope_para_min;
	uint32_t amb_thre;            //ambient light
  /* USD modify by hongliang.zhang for hr_spo2 20251024 start */
  uint8_t gsensor_data_support;
  /* USD modify by hongliang.zhang for hr_spo2 20251024 end */
};

extern struct ldr_channel_data_t ldr_channel_data;
extern struct cust_hr_spo2_data_t cust_hr_spo2_data;
extern struct hr_spo2_cfg_t hr_spo2_cfg;
/* USD add hr_spo2 bring up by longtao.ran 20241119 end */

extern uint8_t alg_ram[11*1024];
extern uint8_t hx3918_chip_id;
extern int32_t  sar_wear_thres;
extern int32_t  sar_unwear_thres;

extern WORK_MODE_T work_mode_flag;

void hx_set_touch_mode(uint8_t current_mode);
uint16_t hx3918_read_fifo_data(int32_t *buf, uint8_t phase_num, uint8_t sig);
void read_data_packet(int32_t *ps_data);
void hx3918_delay_us(uint32_t us);
void hx3918_delay(uint32_t ms);
bool hx3918_write_reg(uint8_t addr, uint8_t data); 
bool hx3918_write_reg(uint8_t addr, uint8_t data);
uint8_t hx3918_read_reg(uint8_t addr); 
bool hx3918_brust_read_reg(uint8_t addr , uint8_t *buf, uint8_t length);
bool hx3918_chip_check(void);
uint8_t hx3918_read_fifo_size(void);
void hx3918_ppg_off(void);
void hx3918_ppg_on(void);  
void hx3918_ppg_timer_cfg(bool en);
void hx3918_agc_timer_cfg(bool en);
void hx3918_gpioint_cfg(bool en);
bool hx3918_init(WORK_MODE_T mode);
void hx3918_agc_Int_handle(void); 
void hx3918_gesensor_Int_handle(void);
void hx3918_spo2_ppg_init(void);
void hx3918_spo2_ppg_Int_handle(void);
void hx3918_wear_ppg_Int_handle(void);
void hx3918_ft_hrs_Int_handle(void);
void hx3918_ft_spo2_Int_handle(void);
void hx3918_hrs_ppg_init(void);
void hx3918_hrs_ppg_Int_handle(void);
void hx3918_hrv_ppg_Int_handle(void);
void hx3918_living_Int_handle(void);
//void hx3918_gesensor_Int_handle(void);
void hx3918_ppg_Int_handle(void);
uint32_t hx3918_timers_start(void);
uint32_t hx3918_timers_stop(void);
uint32_t hx3918_gpioint_init(void);

uint32_t hx3918_gpioint_enable(void);
uint32_t hx3918_gpioint_disable(void);
uint32_t hx3918_40ms_timers_start(void);
uint32_t hx3918_40ms_timers_stop(void);

void hx3918_vin_check(uint16_t led_vin);

#ifdef LAB_TEST
void hx3918_lab_test_init(void);
void hx3918_test_alg_cfg(void);
SENSOR_ERROR_T hx3918_lab_test_enable(void);
void hx3918_lab_test_Int_handle(void);
void hx3918_lab_test_read_packet(uint32_t *data_buf);
#endif

#ifdef CHECK_TOUCH_LIB
hx3918_wear_msg_code_t hx3918_check_touch(int32_t *ir_data, uint8_t data_len);
SENSOR_ERROR_T hx3918_check_touch_enable(void);
#endif

bool hx3918_living_send_data(int32_t *new_raw_data, uint8_t dat_len, uint32_t green_data_als, int16_t *gsen_data_x, int16_t *gsen_data_y, int16_t *gsen_data_z);
hx3918_living_results_t hx3918_living_get_results(void);
void agc_timeout_handler(void * p_context);
void heart_rate_meas_timeout_handler(void * p_context);

int16_t hx3918xxx_read_two_reg_low_triple(uint8_t addh, uint8_t addl);
void display_refresh(void);
#endif
