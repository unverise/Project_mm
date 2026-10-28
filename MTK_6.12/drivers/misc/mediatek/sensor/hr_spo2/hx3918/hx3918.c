
//#include <stdio.h>
//#include <stdbool.h>
//#include <stdint.h>
//#include <stdlib.h>
//#include <math.h>

#include "hx3918.h"
#ifdef  TYHX_DEMO
#include "twi_master.h"
#include "SEGGER_RTT.h"
#include "app_timer.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_drv_gpiote.h"
#include "drv_oled.h"
#include "opr_oled.h"

#include "oled_iic.h"
#include "word.h"
#include "iic.h"
#endif
//#include "tyhx_hrs_custom.h"
#ifdef SAR_ALG_LIB
#include "hx3918_prox.h"
#endif

#ifdef HRS_ALG_LIB
#include "hx3918_hrs_agc.h"
#include "tyhx_hrs_alg.h"
#endif

#ifdef SPO2_ALG_LIB
#include "hx3918_spo2_agc.h"
#include "tyhx_spo2_alg.h"
#endif

#ifdef HRV_ALG_LIB
#include "hx3918_hrv_agc.h"
#include "tyhx_hrv_alg.h"
#endif

#ifdef CHECK_TOUCH_LIB
#include "hx3918_check_touch.h"
#endif

#include "hx3918_factory_test.h"



#ifdef SPO2_VECTOR
#include "spo2_vec.h"
uint32_t spo2_send_cnt = 0;
int32_t red_buf_vec[8];
int32_t ir_buf_vec[8];
int32_t green_buf_vec[8];
#endif

#ifdef HR_VECTOR
#include "hr_vec.h"
uint32_t hrs_send_cnt = 0;
int32_t PPG_buf_vec[8];
#endif

#ifdef HRV_TESTVEC
#include "hrv_testvec.h"
#endif

#ifdef GSENSER_DATA
//#include "lis3dh_drv.h"
#endif
tyhx_hrsresult_t hrsresult;
//USD: add hx3918 Movement amplitude by longtao.ran 20240924 start
#ifdef GSENSER_DATA
int16_t gsen_fifo_x[64];  //ppg time 330ms..330/40 = 8.25
int16_t gsen_fifo_y[64];
int16_t gsen_fifo_z[64];
#else
int16_t gen_dummy[64] = {0};
#endif
//USD: add hx3918 Movement amplitude by longtao.ran 20240924 end

//SPO2 agc
const uint8_t  hx3918_spo2_agc_green_idac = GREEN_AGC_OFFSET;
const uint8_t  hx3918_spo2_agc_red_idac = RED_AGC_OFFSET;
const uint8_t  hx3918_spo2_agc_ir_idac = IR_AGC_OFFSET;
//hrs agc
const uint8_t  hx3918_hrs_agc_idac = HR_GREEN_AGC_OFFSET;
//hrv agc
const uint8_t  hx3918_hrv_agc_idac = HR_GREEN_AGC_OFFSET;

const uint8_t  red_led_max_init = 200;
const uint8_t  ir_led_max_init = 200;
const uint8_t  green_led_max_init = 150;

uint8_t low_vbat_flag =0;


const uint16_t static_thre_val = 150;
const uint8_t  gsen_lv_val = 0;


//HRS_INFRARED_THRES
const int32_t  hrs_ir_unwear_thres = 5000;
const int32_t  hrs_ir_wear_thres = 6000;
//HRV_INFRARED_THRES
const int32_t  hrv_ir_unwear_thres = 5000;
const int32_t  hrv_ir_wear_thres = 6000;
//SPO2_INFRARED_THRES
const uint8_t  spo2_check_unwear_oft = 4;
const int32_t  spo2_ir_wear_thres = 20000;
//CHECK_WEAR_MODE_THRES
const int32_t  check_mode_unwear_thre = 5000;
const int32_t  check_mode_wear_thre = 6000;

int32_t  sar_wear_thres = 3000;  // ���趨Ϊ������1/3, ����С��32767
int32_t  sar_unwear_thres = 2400; // ���趨Ϊ sar_wear_thres *0.8

uint8_t alg_ram[11 * 1024] __attribute__((aligned(4)));
ppg_sensor_data_t ppg_s_dat;
uint8_t read_fifo_first_flg = 0;
hrs_sports_mode_t hrs_mode = NORMAL_MODE;



//////// spo2 para and switches
/* USD add hr_spo2 Algorithm update by longtao.ran 20241010 start */
#if 0
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
#endif
/* USD add hr_spo2 Algorithm update by longtao.ran 20241010 end */


WORK_MODE_T work_mode_flag = HRS_MODE;
hx3918_hrs_wear_msg_code_t g_hrs_wear_status;
uint8_t g_hr_result;
uint8_t g_spo2_result;
//USD: add NFRFP-20767 by longtao.ran for [add heart_rate_oximetry test] 20241009 start
factory_result_t hr_spo2_test_result;
spo2_living_code_t g_spo2_living_status = MSG_SPO2_LIVING_INITIAL;
spo2_alg_msg_code_t g_spo2_alg_status = MSG_SPO2_ALG_NOT_OPEN;
uint8_t g_motion;
/* USD modify by hongliang.zhang for hr_spo2 20251024 start */
extern void tran_get_g_sensor_val(int16_t *gsen_data_x, int16_t *gsen_data_y, int16_t *gsen_data_z);
/* USD modify by hongliang.zhang for hr_spo2 20251024 end */
//USD: add NFRFP-20767 by longtao.ran for [add heart_rate_oximetry test] 20241009 end
void display_refresh(void)
{

}


void hx3918_delay(uint32_t ms)
{
    //USD: add hx3918 drv by longtao.ran 20240829 start
    mdelay(ms);
    //USD: add hx3918 drv by longtao.ran 20240829 end
}

//Timer interrupt 40ms repeat mode
void gsen_read_timeout_handler(void * p_context)
{
    hx3918_agc_Int_handle();


#ifdef GSEN_40MS_TIMMER
    hx3918_gesensor_Int_handle();
#endif

#ifdef LAB_TEST_INT_MODE
    if(work_mode_flag == LAB_TEST_MODE)// add ericy 20210127
    {
        hx3690l_lab_test_read();
    }
#endif
}

#if 0
bool hx3918_write_reg(uint8_t addr, uint8_t data)
{
#ifdef  TYHX_DEMO
    uint8_t data_buf[2];    
    data_buf[0] = addr;
    data_buf[1] = data;
    twi_pin_switch(1);
    twi_master_transfer(0x88, data_buf, 2, true);    //write    
#endif
    return true;
}

uint8_t hx3918_read_reg(uint8_t addr)
{
    uint8_t data_buf = 0;  
#ifdef  TYHX_DEMO
     twi_pin_switch(1);
    twi_master_transfer(0x88, &addr, 1, false);      //write
    twi_master_transfer(0x89, &data_buf, 1, true);//read
#endif
    return data_buf;
}

bool hx3918_brust_read_reg(uint8_t addr, uint8_t *buf, uint8_t length)
{
#ifdef  TYHX_DEMO
	twi_pin_switch(1);
    twi_master_transfer(0x88, &addr, 1, false);      //write
    twi_master_transfer(0x89, buf, length, true); //read
#endif
    return true;
}


/*320ms void hx3918_ppg_Int_handle(void)*/
void hx3918_ppg_timer_cfg(bool en)    //320ms
{
    if(en)
    {
#ifdef  TYHX_DEMO
        hx3918_320ms_timers_start();
#endif
    }
    else
    {
#ifdef  TYHX_DEMO
        hx3918_320ms_timers_stop();
#endif
    }

}

/* 40ms void agc_timeout_handler(void * p_context) */
void hx3918_agc_timer_cfg(bool en)
{
    if(en)
    {
#ifdef  TYHX_DEMO
        gsen_read_timers_start();
#endif
    }
    else
    {
#ifdef  TYHX_DEMO
        gsen_read_timers_stop();
#endif
    }

}
#endif

#if defined(INT_MODE)
void hx3918_gpioint_cfg(bool en)
{
    if(en)
    {
        hx3918_gpioint_enable();
    }
    else
    {
        hx3918_gpioint_disable();
    }

}
#endif

uint8_t hx3918_chip_id = 0;
bool hx3918_chip_check(void)
{
    uint8_t i = 0;

    for(i=0; i<10; i++)
    {
        hx3918_write_reg(0x01, 0x00);
        hx3918_delay(5);
        hx3918_chip_id = hx3918_read_reg(0x00);
        if (hx3918_chip_id == 0x27)             
        {
            TYHX_LOG("chip id  = 0x%x	check ok \n",hx3918_chip_id);
            return true;
        }
    }
    TYHX_LOG("hx3918_chip_check fail\n");
    return false;
}


uint8_t hx3918_read_fifo_size(void)
{
    uint8_t fifo_num_temp = 0;
    fifo_num_temp = hx3918_read_reg(0x40)&0x7f;  // ericy 230529

    return fifo_num_temp;
}
uint16_t hx3918_read_fifo_data(int32_t *buf, uint8_t phase_num, uint8_t sig)
{
    uint8_t data_flg_start = 0;
    uint8_t data_flg_end = 0;
    uint8_t databuf[3];
    uint32_t ii=0;
    uint16_t data_len = 0;
    uint16_t fifo_data_length = 0;
    uint16_t fifo_read_length = 0;
    uint16_t fifo_read_bytes = 0;
    uint16_t fifo_out_length = 0;
    uint16_t fifo_out_count = 0;
    /* USD add hr_spo2 fix ke by longtao.ran 20240918 start */
    uint8_t fifo_data_buf[300] = {0};

    data_len = hx3918_read_reg(0x40);// ericy 230529
    fifo_data_length = data_len;
    //DEBUG_PRINTF("data_len:   %d\r\n",data_len);
    if(fifo_data_length<2*phase_num || fifo_data_length > 64)
    {
        DEBUG_PRINTF("read error, data_len = %d\n",data_len);
        return 0;
    }
    /* USD add hr_spo2 fix ke by longtao.ran 20240918 end */
    fifo_read_length = ((fifo_data_length-phase_num)/phase_num)*phase_num;
    fifo_read_bytes = fifo_read_length*3;
    if(read_fifo_first_flg == 1)
    {
        hx3918_brust_read_reg(0x41, databuf, 3);	// ericy 230529
        read_fifo_first_flg = 0;
    }
    hx3918_brust_read_reg(0x41, fifo_data_buf, fifo_read_bytes);	// ericy 230529
//		for(ii=0; ii<fifo_read_bytes; ii++)
//		{
//			DEBUG_PRINTF("%d/%d, %d\r\n", ii+1,fifo_read_bytes,fifo_data_buf[ii]);
//		}
    for(ii=0; ii<fifo_read_length; ii++)
    {
        if(sig==0)
        {
            buf[ii] = (int32_t)(fifo_data_buf[ii*3]|(fifo_data_buf[ii*3+1]<<8)|((fifo_data_buf[ii*3+2]&0x1f)<<16));
        }
        else
        {
            if((fifo_data_buf[ii*3+2]&0x10)!=0)
            {
                buf[ii] = (int32_t)(fifo_data_buf[ii*3]|(fifo_data_buf[ii*3+1]<<8)|((fifo_data_buf[ii*3+2]&0x0f)<<16))-1048576;
            }
            else
            {
                buf[ii] = (int32_t)(fifo_data_buf[ii*3]|(fifo_data_buf[ii*3+1]<<8)|((fifo_data_buf[ii*3+2]&0x1f)<<16));
            }
        }
        //DEBUG_PRINTF("%d/%d, %d %d\r\n", ii+1,fifo_read_length,buf[ii],(fifo_data_buf[ii*3+2]>>5)&0x03);
    }
    data_flg_start = (fifo_data_buf[2]>>5)&0x03;
    data_flg_end = (fifo_data_buf[fifo_read_bytes-1]>>5)&0x03;
    fifo_out_length = fifo_read_length;
    if(data_flg_start>0)
    {
        fifo_out_length = fifo_read_length-phase_num+data_flg_start;
        for(ii=0; ii<fifo_out_length; ii++)
        {
            /* USD add hr_spo2 fix ke by longtao.ran 20240918 start */
            if(ii+phase_num-data_flg_start < 0){
                DEBUG_PRINTF("read error, data_flg_start = %d\n",data_flg_start);
                return 0;
            }
            /* USD add hr_spo2 fix ke by longtao.ran 20240918 end */
            buf[ii] = buf[ii+phase_num-data_flg_start];
        }
        for(ii=fifo_out_length; ii<fifo_read_length; ii++)
        {
            buf[ii] = 0;
        }
    }
    if(data_flg_end<phase_num-1)
    {
        for(ii=fifo_out_length; ii<fifo_out_length+phase_num-data_flg_end-1; ii++)
            hx3918_brust_read_reg(0x41, databuf, 3);// ericy 230529
        buf[ii] = (int32_t)(databuf[0]|(databuf[1]<<8)|((databuf[2]&0x1f)<<16));
    }
//		for(ii=0; ii<fifo_out_length; ii++)
//		{
//			DEBUG_PRINTF("%d/%d, %d\r\n", ii+1,fifo_out_length,buf[ii]);
//		}
    fifo_out_length = fifo_out_length+phase_num-data_flg_end-1;
    fifo_out_count = fifo_out_length/phase_num;
    if(data_len==64)
    {
        uint8_t reg_0x2d = hx3918_read_reg(0x3d);// ericy 230529
        hx3918_write_reg(0x3d,0x00);// ericy 230529
        hx3918_delay(5);
        hx3918_write_reg(0x3d,reg_0x2d);// ericy 230529
        read_fifo_first_flg = 1;
    }
    return fifo_out_count;
}

void read_data_packet(int32_t *ps_data)  // 3918
{
    uint8_t  databuf1[6] = {0};
    uint8_t  databuf2[6] = {0};
    hx3918_brust_read_reg(0x02, databuf1, 6);
    hx3918_brust_read_reg(0x08, databuf2, 6);

    ps_data[0] = ((databuf1[0]) | (databuf1[1] << 8) | (databuf1[2] << 16));
    ps_data[1] = ((databuf1[3]) | (databuf1[4] << 8) | (databuf1[5] << 16));
    ps_data[2] = ((databuf2[0]) | (databuf2[1] << 8) | (databuf2[2] << 16));
    ps_data[3] = ((databuf2[3]) | (databuf2[4] << 8) | (databuf2[5] << 16));
    //DEBUG_PRINTF(" %d %d %d %d \r\n",  ps_data[0], ps_data[1], ps_data[2], ps_data[3]);
}

void hx3918_vin_check(uint16_t led_vin)
{
    low_vbat_flag = 0;
    if(led_vin < 3700)
    {
        low_vbat_flag = 1;
    }
}

void hx3918_ppg_off(void)
{
    hx3918_write_reg(0x2a, 0x00);
    hx3918_write_reg(0x2b, 0x00);
    hx3918_write_reg(0x16, 0x00);
    hx3918_write_reg(0x01, 0x01);
}

void hx3918_ppg_on(void)
{
    hx3918_write_reg(0x01, 0x00);
    hx3918_delay(5);
}

void hx3918_data_reset(void)
{
#if defined(TIMMER_MODE)
    hx3918_ppg_timer_cfg(false);
    hx3918_agc_timer_cfg(false);
#elif defined(INT_MODE)
    hx3918_gpioint_cfg(false);
#endif
#if defined(HRS_ALG_LIB)
    hx3918_hrs_data_reset();
#endif
#if defined(SPO2_ALG_LIB)
    hx3918_spo2_data_reset();
#endif
#if defined(HRV_ALG_LIB)
    hx3918_hrv_data_reset();
#endif
    TYHX_LOG("hx3918 data reset!\r\n");
}

void Efuse_Mode_Check(void)
{
    uint8_t  REG_45_E, REG_46_E, REG_47_E, REG_48_E;

    uint8_t chip_vision_check = 0;
    chip_vision_check = hx3918_read_reg(0x47)>>6;
    if(chip_vision_check > 0)
    {
        return;
    }
    hx3918_write_reg(0x4b, 0x10);
    hx3918_write_reg(0x4d, 0x18);
    hx3918_write_reg(0x4e, 0x40);
    hx3918_write_reg(0x4C, 0x04);
    hx3918_write_reg(0x4C, 0x00);

    REG_45_E = hx3918_read_reg(0x45);
    REG_46_E = hx3918_read_reg(0x46);
    REG_47_E = hx3918_read_reg(0x47);
    REG_48_E = hx3918_read_reg(0x48);

    hx3918_write_reg(0X4B,0x20);
    hx3918_write_reg(0x45,REG_45_E);
    hx3918_write_reg(0x46,REG_46_E);
    hx3918_write_reg(0x47,REG_47_E);
    hx3918_write_reg(0x48,REG_48_E);
    hx3918_write_reg(0X4B,0x00);
    
    hx3918_write_reg(0x4d, 0x20);
    hx3918_write_reg(0x4e, 0x00);
    hx3918_write_reg(0x4C, 0x00);
     
}

bool hx3918_init(WORK_MODE_T mode)
{
    /* USD add hr_spo2 bring up by longtao.ran 20241129 start */
    uint8_t count_block_num;             //delay the block of some single good signal after a series of bad signal
    uint8_t spo2_low_xcorr_thre;         //(64*xcorr)'s square below this threshold, means error signal
    uint8_t spo2_cali = 1;                   //spo2_result cali mode
    //uint8_t xcorr_mode;                  //xcorr mode switch
    //uint8_t quick_result;                //come out the spo2 result quickly ;0 is normal,1 is quick
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
    uint16_t ir_ac_touch_thre = 200;           //AC_min*0.3
    uint16_t ir_fft_pow_thre = 200;            //fft_pow_min
    uint8_t  slope_para_max;
    uint8_t  slope_para_min;
    uint32_t amb_thre;            //ambient light

    work_mode_flag = mode;
    hx3918_data_reset();
    hx3918_vin_check(3800);
    hx3918_ppg_on();
    Efuse_Mode_Check();
    switch (work_mode_flag)
    {
    case HRS_MODE:
        tyhx_hrs_set_living(0,0,0); //  qu=15  std=30
        if(hx3918_hrs_enable()== SENSOR_OP_FAILED)
        {
            return false;
        }
        break;

    case LIVING_MODE:
#ifdef CHECK_LIVING_LIB
        if(hx3918_hrs_enable()== SENSOR_OP_FAILED)
        {
            return false;
        }
#endif
        break;

    case SPO2_MODE:
#ifdef SPO2_DATA_CALI
        hx3918_spo2_data_cali_init();
#endif
#ifdef SPO2_ALG_LIB
        spo2_low_xcorr_thre = hr_spo2_cfg.spo2_low_xcorr_thre;
        spo2_low_snr_thre = hr_spo2_cfg.spo2_low_snr_thre;
        count_block_num = hr_spo2_cfg.count_block_num;
        spo2_base_line_init = hr_spo2_cfg.spo2_base_line_init;
        spo2_slope = hr_spo2_cfg.spo2_slope;
        spo2_gsen_pow_thre = hr_spo2_cfg.spo2_gsen_pow_thre;
        mean_num = hr_spo2_cfg.mean_num;
        spo2_deglitch_thre = hr_spo2_cfg.spo2_deglitch_thre;
        spo2_remove_jump_thre = hr_spo2_cfg.spo2_remove_jump_thre;
        spo2_low_clip_end_time = hr_spo2_cfg.spo2_low_clip_end_time;
        spo2_low_clip_dn = hr_spo2_cfg.spo2_low_clip_dn;
        spo2_normal_clip_dn = hr_spo2_cfg.spo2_normal_clip_dn;
        slope_para_max = hr_spo2_cfg.slope_para_max;
        slope_para_min = hr_spo2_cfg.slope_para_min;
        spo2_low_clip_end_time = hr_spo2_cfg.spo2_low_clip_end_time;
        amb_thre = hr_spo2_cfg.amb_thre;

        tyhx_spo2_para_usuallyadjust(spo2_low_xcorr_thre,spo2_low_snr_thre,count_block_num,spo2_base_line_init,spo2_slope,spo2_gsen_pow_thre,amb_thre);
        tyhx_spo2_para_barelychange(mean_num,spo2_deglitch_thre,spo2_remove_jump_thre,spo2_low_clip_end_time,spo2_low_clip_dn, \
                                    spo2_normal_clip_dn,ir_ac_touch_thre,ir_fft_pow_thre,spo2_cali,slope_para_max,slope_para_min);
        if(hx3918_spo2_enable()== SENSOR_OP_FAILED)
        {
            return false;
        }
#endif
        break;
	/* USD add hr_spo2 bring up by longtao.ran 20241129 end */

    case HRV_MODE:
#ifdef HRV_ALG_LIB
        if(hx3918_hrv_enable()== SENSOR_OP_FAILED)
        {
            return false;
        }

        break;
#endif
    case WEAR_MODE:
#ifdef CHECK_TOUCH_LIB

        if(hx3918_check_touch_enable()== SENSOR_OP_FAILED)
        {
            return false;
        }
#endif
        break;

    case FT_LEAK_LIGHT_MODE:
        if (!hx3918_chip_check())
        {
            AGC_LOG("hx3918 check id failed!\r\n");
            return false;
        }
        hr_spo2_test_result = hx3918_factroy_test(LEAK_LIGHT_TEST);
        break;

    case FT_GRAY_CARD_MODE:
        hr_spo2_test_result = hx3918_factroy_test(GRAY_CARD_TEST);
        break;

    case FT_INT_TEST_MODE:
        hx3918_factroy_test(FT_INT_TEST);
//			  hx3918_gpioint_cfg(true);
        break;

    case LAB_TEST_MODE:
#ifdef LAB_TEST
        if(hx3918_lab_test_enable()== SENSOR_OP_FAILED)
        {
            return false;
        }
#endif
        break;

    default:
        break;
    }

    return true;
}

void hx3918_agc_Int_handle(void)
{

    switch (work_mode_flag)
    {
    case HRS_MODE:
    {
#ifdef HRS_ALG_LIB
        HRS_CAL_SET_T cal;
        cal = PPG_hrs_agc();
        if(cal.work)
        {
            //AGC_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,rf=%d\r\n", \
            //cal.led_cur, cal.led_idac, cal.amb_idac,cal.led_step,cal.ontime);
        }
#endif
        break;
    }
    case LIVING_MODE:
    {
#ifdef HRS_ALG_LIB
        HRS_CAL_SET_T cal;
        cal = PPG_hrs_agc();
        if(cal.work)
        {
            //TYHX_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,rf=%d\r\n", \
            //cal.led_cur, cal.led_idac, cal.amb_idac,cal.led_step,cal.ontime);
        }
#endif
        break;
    }
    case HRV_MODE:
    {
#ifdef HRV_ALG_LIB
        HRV_CAL_SET_T cal;
        cal = PPG_hrv_agc();
        if(cal.work)
        {
            //AGC_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,rf=%d\r\n", \
            //cal.led_cur, cal.led_idac, cal.amb_idac,cal.led_step,cal.ontime);
        }
#endif
        break;
    }
    case SPO2_MODE:
    {
#ifdef SPO2_ALG_LIB
        SPO2_CAL_SET_T cal;
        cal = PPG_spo2_agc();
        if(cal.work)
        {
            //AGC_LOG("INT_AGC: Rled_drv=%d,Irled_drv=%d,RledDac=%d,IrledDac=%d,ambDac=%d,Rledstep=%d,Irledstep=%d,Rrf=%d,Irrf=%d,\r\n",\
            //cal.red_cur, cal.ir_cur,cal.red_idac,cal.ir_idac,cal.amb_idac,cal.red_led_step,cal.red_led_step,cal.ontime,cal.state);
        }
#endif
        break;
    }
    case LAB_TEST_MODE:
#ifdef LAB_TEST
        hx3918_lab_test_Int_handle();
#endif
    default:
        break;
    }
}
//USD: add hx3918 Movement amplitude by longtao.ran 20240924 start
void hx3918_gesensor_Int_handle(void)
{
    //readFifoByHrsModule();
#ifdef GSENSER_DATA
    uint8_t ii = 0;
    //struct gsen_fifo *g_sensor_data;
#if 0
    if(work_mode_flag == WEAR_MODE)
    {
        return;
    }
#endif
    //LIS3DH_GetAccAxesRaw(&gsen_buf);
    //g_sensor_data = tran_get_g_sensor_val();
    for (ii=0; ii<9; ii++) {
        gsen_fifo_x[ii] = gsen_fifo_x[ii+1];
        gsen_fifo_y[ii] = gsen_fifo_y[ii+1];
        gsen_fifo_z[ii] = gsen_fifo_z[ii+1];
    }
    //gsen_fifo_x[9] = g_sensordata[0];
    //gsen_fifo_y[9] = g_sensordata[1];
    //gsen_fifo_z[9] = g_sensordata[2];
    //DEBUG_PRINTF("gsen_x=%d gsen_y=%d gsen_z=%d\r\n",g_sensordata[0],g_sensordata[1],g_sensordata[2]);
#endif
}
//USD: add hx3918 Movement amplitude by longtao.ran 20240924 end

void agc_timeout_handler(void * p_context)
{
#ifdef TIMMER_MODE
    switch (work_mode_flag)
    {
    case HRS_MODE:
    {
#ifdef HRS_ALG_LIB
        HRS_CAL_SET_T cal;
        cal = PPG_hrs_agc();
        if(cal.work)
        {
            //AGC_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,rf=%d\r\n", \
            //cal.led_cur, cal.led_idac, cal.amb_idac,cal.led_step,cal.ontime);
        }
#endif
        break;
    }
    case LIVING_MODE:
    {
#ifdef HRS_ALG_LIB
        HRS_CAL_SET_T cal;
        cal = PPG_hrs_agc();
        if(cal.work)
        {
            //AGC_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,rf=%d\r\n", \
            //cal.led_cur, cal.led_idac, cal.amb_idac,cal.led_step,cal.ontime);
        }
#endif
        break;
    }
    case HRV_MODE:
    {
#ifdef HRV_ALG_LIB
        HRV_CAL_SET_T cal;
        cal = PPG_hrv_agc();
        if(cal.work)
        {
            //AGC_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,rf=%d\r\n", \
            //cal.led_cur, cal.led_idac, cal.amb_idac,cal.led_step,cal.ontime);
        }
#endif
        break;
    }
    case SPO2_MODE:
    {
#ifdef SPO2_ALG_LIB
        SPO2_CAL_SET_T cal;
        cal = PPG_spo2_agc();
        if(cal.work)
        {
            //AGC_LOG("AGC: Rled_drv=%d,Irled_drv=%d,RledDac=%d,IrledDac=%d,ambDac=%d,Rledstep=%d,Irledstep=%d,Rrf=%d,Irrf=%d,\r\n", \
            //cal.red_cur, cal.ir_cur,cal.red_idac,cal.ir_idac,cal.amb_idac,cal.red_led_step,cal.red_led_step,cal.ontime,cal.state);
        }
#endif
        break;
    }
    case LAB_TEST_MODE:
    {
#ifdef LAB_TEST
        hx3918_lab_test_Int_handle();
#endif
    break;
    }
    default:
        break;
    }
#endif
}

//void ppg_timeout_handler(void * p_context)
void heart_rate_meas_timeout_handler(void * p_context)
{
	hx3918_ppg_Int_handle();
}

void hx3918_ppg_Int_handle(void)
{
#if defined(INT_MODE)
    hx3918_agc_Int_handle();
#endif
#ifdef SAR_ALG_LIB
		hx3918_report_prox_status();
#endif
	
    switch(work_mode_flag)
    {
    case HRS_MODE:
#ifdef HRS_ALG_LIB
        hx3918_hrs_ppg_Int_handle();
#endif

        break;

    case SPO2_MODE:
#ifdef SPO2_ALG_LIB
        hx3918_spo2_ppg_Int_handle();
#endif
        break;

    case HRV_MODE:
#ifdef HRV_ALG_LIB
        hx3918_hrv_ppg_Int_handle();
#endif
        break;

    case WEAR_MODE:
#ifdef CHECK_TOUCH_LIB
        hx3918_wear_ppg_Int_handle();
#endif
        break;

    case LAB_TEST_MODE:
#ifdef LAB_TEST
        hx3918_lab_test_Int_handle();
#endif

    default:
        break;
    }

}

#ifdef HRS_ALG_LIB
int16_t gsen_fifo_x_send[64]= {0};
int16_t gsen_fifo_y_send[64]= {0};
int16_t gsen_fifo_z_send[64]= {0};
void hx3918_hrs_ppg_Int_handle(void)
{
    uint8_t        ii=0;
    hrs_results_t alg_results = {MSG_HRS_ALG_NOT_OPEN,0,0,0,0};
    hx3918_hrs_wear_msg_code_t hrs_wear_status = MSG_HRS_INIT;
    int32_t *PPG_buf = &(ppg_s_dat.green_data[0]);
    //int32_t *ir_buf = &(ppg_s_dat.ir_data[0]);
    uint8_t *count = &(ppg_s_dat.count);

    //HRS_CAL_SET_T cal= get_hrs_agc_status();
#ifdef BP_CUSTDOWN_ALG_LIB
    //bp_results_t    bp_alg_results;
#endif
#ifdef HR_VECTOR
    for(ii=0; ii<8; ii++)
    {
        PPG_buf_vec[ii] = hrm_input_data[hrs_send_cnt+ii];
        gsen_fifo_x[ii] = gsen_input_data_x[hrs_send_cnt+ii];
        gsen_fifo_y[ii] = gsen_input_data_y[hrs_send_cnt+ii];
        gsen_fifo_z[ii] = gsen_input_data_z[hrs_send_cnt+ii];
    }
    hrs_send_cnt = hrs_send_cnt+8;
    *count = 8;
    tyhx_hrs_alg_send_data(PPG_buf_vec,*count, 0, gsen_fifo_x, gsen_fifo_y, gsen_fifo_z);
#else
    if(hx3918_hrs_read(&ppg_s_dat) == 0)
    {
        return;
    }
#ifndef GSEN_40MS_TIMMER
    //readFifoByHrsModule();
#endif
    for(ii=0; ii<*count; ii++)
    {
#ifdef GSENSER_DATA
        gsen_fifo_x_send[ii] = gsen_fifo_x[32-*count+ii];
        gsen_fifo_y_send[ii] = gsen_fifo_y[32-*count+ii];
        gsen_fifo_z_send[ii] = gsen_fifo_z[32-*count+ii];
#endif
        //DEBUG_PRINTF("HX %d/%d  %d %d %d %d %d %d\r\n",ii,*count, PPG_buf[ii],ir_buf[ii],gsen_fifo_x_send[ii],gsen_fifo_y_send[ii],gsen_fifo_z_send[ii],ppg_s_dat.green_cur);
    }
    hrs_wear_status = hx3918_hrs_get_wear_status();
	hrsresult.wearstatus = hrs_wear_status;
    if(hrs_wear_status == MSG_HRS_WEAR)
    {
        tyhx_hrs_alg_send_data(PPG_buf, *count, gsen_fifo_x_send, gsen_fifo_y_send, gsen_fifo_z_send);
    }
#endif
    alg_results = tyhx_hrs_alg_get_results();
    hrsresult.lastesthrs = alg_results.hr_result;
	g_hr_result = alg_results.hr_result;
	g_hrs_wear_status = hrs_wear_status;

	//DEBUG_PRINTF("[hx3918 test]:%d %d\r\n",alg_results.hr_result,alg_results.data_cnt);

//            TYHX_LOG("living=%d",hrsresult.hr_living);
#ifdef HRS_BLE_APP
    {
        rawdata_vector_t rawdata;
        for(ii=0; ii<*count; ii++)
        {
            rawdata.vector_flag = HRS_VECTOR_FLAG;
            rawdata.data_cnt = alg_results.data_cnt-*count+ii;
            rawdata.hr_result = alg_results.hr_result;
            rawdata.red_raw_data = PPG_buf[ii];
            rawdata.ir_raw_data = ir_buf[ii];
            rawdata.gsensor_x = gsen_fifo_x[ii];
            rawdata.gsensor_y = gsen_fifo_y[ii];
            rawdata.gsensor_z = gsen_fifo_z[ii];
            rawdata.red_cur = cal.led_cur;
            rawdata.ir_cur = alg_results.hrs_alg_status;

            ble_rawdata_vector_push(rawdata);
        }
    }
    ble_rawdata_send_handler();
#endif
}

#ifdef CHECK_LIVING_LIB
void hx3918_living_Int_handle(void)
{

    uint8_t        ii=0;
    hx3918_living_results_t living_alg_results = {MSG_LIVING_NO_WEAR,0,0,0};

    int32_t *PPG_buf = &(ppg_s_dat.green_data[0]);
    uint8_t *count = &(ppg_s_dat.count);
    hx3918_hrs_wear_msg_code_t     hrs_wear_status = MSG_HRS_NO_WEAR;
    int16_t 	gsen_fifo_x_send[32]= {0};
    int16_t 	gsen_fifo_y_send[32]= {0};
    int16_t 	gsen_fifo_z_send[32]= {0};

    if(hx3918_hrs_read(&ppg_s_dat) == 0)
    {
        return;
    }
#ifndef GSEN_40MS_TIMMER
    //readFifoByHrsModule();
#endif
    for(ii=0; ii<*count; ii++)
    {
        gsen_fifo_x_send[ii] = gsen_fifo_x[32-*count+ii];
        gsen_fifo_y_send[ii] = gsen_fifo_y[32-*count+ii];
        gsen_fifo_z_send[ii] = gsen_fifo_z[32-*count+ii];
        //DEBUG_PRINTF("%d/%d %d %d %d %d %d %d\r\n",1+ii,*count,PPG_buf[ii],ir_buf[ii],gsen_fifo_x[ii],gsen_fisignal_qualityfo_y[ii],gsen_fifo_z[ii],hrs_s_dat.agc_green);
    }

    living_alg_results = hx3918_living_get_results();
    //TYHX_LOG("%d %d %d %d\r\n",living_alg_results.data_cnt,living_alg_results.motion_status,living_alg_results.signal_quality,living_alg_results.wear_status);

}
#endif

/* USD add hr_spo2 bring up by longtao.ran 20241216 start */
#ifdef SPO2_ALG_LIB
void hx3918_spo2_ppg_Int_handle(void)
{
    uint8_t        ii=0;
    spo2_results_t alg_results = {MSG_SPO2_ALG_NOT_OPEN,MSG_SPO2_LIVING_INITIAL,0,0,0,0,0,0,1,0,0,0,0};
    SPO2_CAL_SET_T cal=get_spo2_agc_status();
    hx3918_spo2_wear_msg_code_t spo2_wear_status = MSG_SPO2_INIT;
    //int32_t *green_buf = &(ppg_s_dat.green_data[0]);
    int32_t *red_buf = &(ppg_s_dat.red_data[0]);
    int32_t *ir_buf = &(ppg_s_dat.ir_data[0]);
    uint8_t *count = &(ppg_s_dat.count);
    /* USD add hr_spo2 bring up by longtao.ran 20241129 start */
    int32_t *amb_buf = &(ppg_s_dat.amb_data[0]);
    /* USD add hr_spo2 bring up by longtao.ran 20241129 end */
#ifdef SPO2_DATA_CALI
    int32_t red_data_cali, ir_data_cali;
#endif

#ifdef SPO2_VECTOR
    for(ii=0; ii<8; ii++)
    {
        red_buf_vec[ii] = vec_red_data[spo2_send_cnt+ii];
        ir_buf_vec[ii] = vec_ir_data[spo2_send_cnt+ii];
        green_buf_vec[ii] = vec_green_data[spo2_send_cnt+ii];
    }
    spo2_send_cnt = spo2_send_cnt+8;
    *count = 8;
    for(ii=0; ii<10; ii++)
    {
        gsen_fifo_x[ii] = vec_red_data[spo2_send_cnt1+ii];;
        gsen_fifo_y[ii] = vec_ir_data[spo2_send_cnt1+ii];;
        gsen_fifo_z[ii] = vec_green_data[spo2_send_cnt1+ii];;
    }
    spo2_send_cnt1 = spo2_send_cnt1+10;
    hx3918_spo2_alg_send_data(red_buf_vec, ir_buf_vec, green_buf_vec, *count, gsen_fifo_x, gsen_fifo_y, gsen_fifo_z);
#else
    if(hx3918_spo2_read(&ppg_s_dat) == 0)
    {
        return;
    }
#ifndef GSEN_40MS_TIMMER
    //readFifoByHrsModule();
#endif
    for(ii=0; ii<*count; ii++)
    {
//USD:add X6855OS151-5 by yuan.tao for [add amb_buf] 20250108 start
#if 0
        DEBUG_PRINTF("HX %d/%d %d %d %d,%d %d %d,%d %d %d,%d %d %d amb_buf:%d\r\n" ,1+ii,*count,\
               green_buf[ii],red_buf[ii],ir_buf[ii],cal.green_idac,cal.red_idac,cal.ir_idac,cal.green_cur,cal.red_cur,cal.ir_cur,gsen_fifo_x[ii],gsen_fifo_y[ii],gsen_fifo_z[ii], amb_buf[ii]);
#endif
    }
    spo2_wear_status = hx3918_spo2_get_wear_status();
	hrsresult.wearstatus = spo2_wear_status;
    if(spo2_wear_status == MSG_SPO2_WEAR)
    {

    /* USD modify by hongliang.zhang for hr_spo2 20251024 start */
        if (hr_spo2_cfg.gsensor_data_support) {
            if (cal.red_cur >= 200 || cal.ir_cur >= 200) {
                tyhx_spo2_set_living(hr_spo2_cfg.living_mode, 15, hr_spo2_cfg.st_thre);
            } else {
                tyhx_spo2_set_living(hr_spo2_cfg.living_mode, hr_spo2_cfg.qu_thre, hr_spo2_cfg.st_thre);
            }
#ifdef GSENSER_DATA
            tran_get_g_sensor_val(gsen_fifo_x, gsen_fifo_y, gsen_fifo_z);
            DEBUG_PRINTF("gsensor x=%d, y=%d, z=%d, amb_buf = %d\n",gsen_fifo_x[1], gsen_fifo_y[1], gsen_fifo_z[1], *amb_buf);
            tyhx_spo2_alg_send_data(red_buf, ir_buf, ir_buf, cal.red_idac, cal.ir_idac, cal.ir_idac, gsen_fifo_x, gsen_fifo_y, gsen_fifo_z, amb_buf, *count);
#endif
        } else {
#ifdef GSENSER_DATA
            tyhx_spo2_alg_send_data(red_buf, ir_buf, ir_buf, cal.red_idac, cal.ir_idac, cal.ir_idac, gsen_fifo_x, gsen_fifo_y, gsen_fifo_z, amb_buf, *count);
#else
            tyhx_spo2_alg_send_data(red_buf, ir_buf, ir_buf, cal.red_idac, cal.ir_idac, cal.ir_idac, gen_dummy, gen_dummy, gen_dummy, amb_buf, *count);
#endif
        }
        /* USD modify by hongliang.zhang for hr_spo2 20251024 end */
//USD:add TOS1501-49072 by yuan.tao for [add Dynamic threshold for Live detection] 20250122 end

    }
#endif

    alg_results = tyhx_spo2_alg_get_results();
    hrsresult.lastestspo2 = alg_results.spo2_result;
	g_spo2_result = alg_results.spo2_result;
	//USD: add hx3918 drv by longtao.ran 20240924 start
	g_hr_result = alg_results.hr_result;
	if (spo2_wear_status != MSG_SPO2_WEAR)
	{
		g_spo2_result = 0;
		g_hr_result = 0;
	}
	g_hrs_wear_status = (hx3918_hrs_wear_msg_code_t) spo2_wear_status;
	g_spo2_living_status = alg_results.spo2_living_status;
	g_spo2_alg_status = alg_results.spo2_alg_status;
	g_motion = alg_results.motion;
	DEBUG_PRINTF("SPO2=%d, HR=%d, wear status=%d, living_status=%d, alg_status=%d, motion=%d\n",\
        g_spo2_result,\
        g_hr_result,\
        g_hrs_wear_status,\
        g_spo2_living_status,\
        g_spo2_alg_status,\
        g_motion);
    //TYHX_LOG("SPO2=%d\r\n",hrsresult.lastestspo2);
	//USD: add hx3918 drv by longtao.ran 20240924 end

#ifdef HRS_BLE_APP
    {
        rawdata_vector_t rawdata;
        for(ii=0; ii<*count; ii++)
        {
#ifdef SPO2_DATA_CALI
            ir_data_cali = hx3918_ir_data_cali(ir_buf[ii]);
            red_data_cali = hx3918_red_data_cali(red_buf[ii]);
            rawdata.red_raw_data = red_data_cali;
            rawdata.ir_raw_data = ir_data_cali;
#else
            rawdata.red_raw_data = red_buf[ii];
            rawdata.ir_raw_data = ir_buf[ii];
#endif
            rawdata.vector_flag = SPO2_VECTOR_FLAG;
            //rawdata.data_cnt = alg_results.data_cnt-*count+ii;
            rawdata.data_cnt = cal.green_idac;
            rawdata.hr_result = alg_results.spo2_result;
//            rawdata.gsensor_x = gsen_fifo_x[ii];
//            rawdata.gsensor_y = gsen_fifo_y[ii];
//            rawdata.gsensor_z = gsen_fifo_z[ii];
            rawdata.gsensor_x = green_buf[ii]>>5;
            rawdata.gsensor_y = cal.red_idac;
            rawdata.gsensor_z = cal.ir_idac;
            rawdata.red_cur = cal.red_cur;
            rawdata.ir_cur = cal.ir_cur;
            ble_rawdata_vector_push(rawdata);
        }
    }
    ble_rawdata_send_handler();
#endif
}
/* USD add hr_spo2 bring up by longtao.ran 20241216 end */
#endif

#ifdef HRV_ALG_LIB
void hx3918_hrv_ppg_Int_handle(void)
{
    uint8_t        ii=0;
    hrv_results_t alg_hrv_results= {MSG_HRV_ALG_NOT_OPEN,0,0,0,0,0};
    int32_t *PPG_buf = &(ppg_s_dat.green_data[0]);
    int32_t *ir_buf = &(ppg_s_dat.ir_data[0]);
    uint8_t *count = &(ppg_s_dat.count);
    hx3918_hrv_wear_msg_code_t wear_mode;

#ifdef HRV_TESTVEC
    int32_t hrm_raw_data;
    hrm_raw_data = vec_data[vec_data_cnt];
    vec_data_cnt++;
    alg_hrv_results = hx3918_hrv_alg_send_data(hrm_raw_data, 0, 0);
#else
    if(hx3918_hrv_read(&ppg_s_dat) == NULL)
    {
        return;
    }
    for(ii=0; ii<*count; ii++)
    {
        //DEBUG_PRINTF("%d/%d %d\r\n" ,1+ii,*count,PPG_buf[ii]);
    }
    wear_mode = hx3918_hrv_get_wear_status();
	hrsresult.wearstatus = wear_mode;
	g_hrs_wear_status = wear_mode;
    if(wear_mode==MSG_HRV_WEAR)
    {
//        alg_hrv_results = tyhx_hrv_alg_send_bufdata(PPG_buf, *count, 0);
//        hrsresult.lastesthrv = alg_hrv_results .hrv_result;
//        TYHX_LOG("get the hrv = %d\r\n",alg_hrv_results .hrv_result);
    }
#endif

#ifdef HRS_BLE_APP
    {
        rawdata_vector_t rawdata;

        HRS_CAL_SET_T cal= get_hrs_agc_status();
        for(ii=0; ii<*count; ii++)
        {
            rawdata.vector_flag = HRS_VECTOR_FLAG;
            rawdata.data_cnt = 0;
            rawdata.hr_result = alg_hrv_results.hrv_result;
            rawdata.red_raw_data = PPG_buf[ii];
            rawdata.ir_raw_data = 0;
            rawdata.gsensor_x = gsen_fifo_x[ii];
            rawdata.gsensor_y = gsen_fifo_y[ii];
            rawdata.gsensor_z = gsen_fifo_z[ii];
            rawdata.red_cur = cal.led_cur;
            rawdata.ir_cur = 0;
            ble_rawdata_vector_push(rawdata);
        }
    }
    ble_rawdata_send_handler();
#endif
}
#endif


#ifdef CHECK_TOUCH_LIB
void hx3918_wear_ppg_Int_handle(void)
{
    uint8_t count = 0;
    uint8_t ii = 0;
    hx3918_wear_msg_code_t hx3918_check_mode_status = MSG_NO_WEAR;
    count = hx3918_check_touch_read(&ppg_s_dat);
    hx3918_check_mode_status = hx3918_check_touch(ppg_s_dat.s_buf,count);
    hrsresult.wearstatus = hx3918_check_mode_status;
	g_hrs_wear_status = hx3918_check_mode_status;
    

#ifdef HRS_BLE_APP
    {
        rawdata_vector_t rawdata;
        //for(ii=0; ii<*count; ii++)
        //app_ctrl_test = true;
        {
            rawdata.vector_flag = HRS_VECTOR_FLAG;
            rawdata.data_cnt = 0;
            rawdata.hr_result = 0;
            rawdata.red_raw_data = 0;
            rawdata.ir_raw_data = lp[0];
            rawdata.gsensor_x = bl[0];
            rawdata.gsensor_y = gdiff[1];
            rawdata.gsensor_z = nv_base_data[0];
            rawdata.red_cur = 0;
            rawdata.ir_cur = 0;

            ble_rawdata_vector_push(rawdata);
        }
    }
    ble_rawdata_send_handler();
#endif

}
#endif


void hx3918_lab_test_Int_handle(void)
{
#ifdef LAB_TEST
#if defined(TIMMER_MODE)
    uint32_t data_buf[4];
    hx3918_lab_test_read_packet(data_buf);
    //DEBUG_PRINTF("%d %d %d %d\r\n", data_buf[0], data_buf[1], data_buf[2], data_buf[3]);
#else
    uint8_t count = 0;
    int32_t buf[32];
    count = hx3918_read_fifo_data(buf, 2, 1);
    for (uint8_t i=0; i<count; i++)
    {
        //DEBUG_PRINTF("%d/%d %d %d\r\n" ,1+i,count,buf[i*2],buf[i*2+1]);
    }
#endif
#endif
}
#endif //CHIP_HX3918

