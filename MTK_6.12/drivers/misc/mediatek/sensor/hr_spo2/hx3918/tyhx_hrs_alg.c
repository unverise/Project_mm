//#include <stdio.h>
//#include <stdbool.h>
//#include <stdint.h>
//#include <stdlib.h>
//#include <math.h>
#include "hx3918.h"
#include "tyhx_hrs_alg.h"
#include "tyhx_hrs_alg_para.h"


#ifdef NO_MATH_LIB
//extern int16_t  round(int16_t x);
//static int32_t rand(void)
//{
//    return 0;
//}
static uint32_t hr_sqrt(uint32_t x)
{
    uint32_t val = x;
    uint32_t last;
    if (val == 0)
    {
        return 0;
    }
    do
    {
        last = val;
        val = (val + x / val) / 2;
    }
    while (abs(val - last) > 1);
    return val;
}
#endif

#ifdef HRS_ALG_LIB

static uint16_t STATIC_THRE = 150;
static uint8_t GSEN_FUN_LV = 0;
static alg_data_t *alg_data; 
//static alg_data_t _alg_data; 
static hrs_sports_mode_t sports_mode_menu = NORMAL_MODE;
static hrs_alg_msg_code_t alg_status = MSG_HRS_ALG_NOT_OPEN;

#ifdef BP_CUSTDOWN_ALG_LIB
static   bp_results_t alg_bp_results = {MSG_BP_ALG_NOT_OPEN, 0, 0, 0, 0, 0};
bp_exinf_t body_sign = {0, 0, 0, 0, 0, 0};
#endif

uint8_t debug_flag = 0;

static uint8_t Check_Living_Mode = 0;
static uint8_t QU_Thre = 0;
static uint8_t ST_Thre = 0;
static living_msg_code_t living_status = MSG_LIVING_INITIAL;

void tyhx_hrs_set_alg_para(hrs_sports_mode_t sports_mode, uint16_t static_thre_val, uint8_t gsen_fun_lv_val)
{
	sports_mode_menu = sports_mode;
	STATIC_THRE = static_thre_val;
	GSEN_FUN_LV = gsen_fun_lv_val;
}

void tyhx_hrs_set_living(uint8_t mode, uint8_t qu_thre, uint8_t st_thre)
{
    Check_Living_Mode = mode;
    QU_Thre = qu_thre;
    ST_Thre = st_thre;
}

uint8_t  const sin_table[256] = {0,1,3,4,6,7,9,10,12,14,15,17,18,20,21,23,25,26,28,29,31,32,34,36,37,39,
                                40,42,43,45,46,48,49,51,53,54,56,57,59,60,62,63,65,66,68,69,71,72,74,75,
                                77,78,80,81,83,84,86,87,89,90,92,93,95,96,97,99,100,102,103,105,106,108,
                                109,110,112,113,115,116,117,119,120,122,123,124,126,127,128,130,131,132,
                                134,135,136,138,139,140,142,143,144,146,147,148,149,151,152,153,155,156,
                                157,158,159,161,162,163,164,166,167,168,169,170,171,173,174,175,176,177,
                                178,179,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,
                                197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,211,212,213,
                                214,215,216,217,217,218,219,220,221,221,222,223,224,225,225,226,227,227,
                                228,229,230,230,231,232,232,233,234,234,235,235,236,237,237,238,238,239,
                                239,240,241,241,242,242,243,243,244,244,244,245,245,246,246,247,247,247,
                                248,248,249,249,249,250,250,250,251,251,251,251,252,252,252,252,253,253,
                                253,253,254,254,254,254,254,254,255,255,255,255,255,255,255,255,255,255,
                                255,255,255,255
                                };

bool tyhx_gsen_pedometer_init(void)
{
    uint16_t ii;

	for (ii = 0; ii < 5; ii++)
    {
        alg_data->gsen_pedometer.pow_all_fifo[ii] = 0;
    }
	alg_data->gsen_pedometer.gsen_powpeak = 0;
    alg_data->gsen_pedometer.gsen_data_cnt = 0;
    alg_data->gsen_pedometer.motion_status = 0;
		
    alg_data->gsen_pedometer.motion_pow_lv0_cnt = 0;
    alg_data->gsen_pedometer.motion_pow_lv1_cnt = 0;
    alg_data->gsen_pedometer.motion_pow_lv2_cnt = 0;
    alg_data->gsen_pedometer.motion_pow_lv3_cnt = 0;
    alg_data->gsen_pedometer.motion_pow_lv4_cnt = 0; 
    alg_data->gsen_pedometer.motion_pow_lv5_cnt = 0;

    alg_data->gsen_pedometer.status_this = 0;
    alg_data->gsen_pedometer.status_last = 0;
    alg_data->gsen_pedometer.hr_up_cnt = 0;
    alg_data->gsen_pedometer.hr_down_cnt = 0;
    alg_data->gsen_pedometer.hr_up_exp = 0;
    alg_data->gsen_pedometer.hr_down_exp = 0;		
		
    alg_data->gsen_pedometer.iir_data_gsen_x.flt_in_dly = 0;
    alg_data->gsen_pedometer.iir_data_gsen_x.flt_in_dly2 = 0;
    alg_data->gsen_pedometer.iir_data_gsen_x.sec1_dly = 0;
    alg_data->gsen_pedometer.iir_data_gsen_x.sec1_dly2 = 0;
    alg_data->gsen_pedometer.iir_data_gsen_x.sec1_out = 0;
    alg_data->gsen_pedometer.iir_data_gsen_x.fir2_dly = 0;
    alg_data->gsen_pedometer.iir_data_gsen_x.fir2_dly2 = 0;
    alg_data->gsen_pedometer.iir_data_gsen_x.sec2_dly = 0;
    alg_data->gsen_pedometer.iir_data_gsen_x.sec2_dly2 = 0;
    alg_data->gsen_pedometer.iir_data_gsen_x.out = 0;
    alg_data->gsen_pedometer.iir_data_gsen_y.flt_in_dly = 0;
    alg_data->gsen_pedometer.iir_data_gsen_y.flt_in_dly2 = 0;
    alg_data->gsen_pedometer.iir_data_gsen_y.sec1_dly = 0;
    alg_data->gsen_pedometer.iir_data_gsen_y.sec1_dly2 = 0;
    alg_data->gsen_pedometer.iir_data_gsen_y.sec1_out = 0;
    alg_data->gsen_pedometer.iir_data_gsen_y.fir2_dly = 0;
    alg_data->gsen_pedometer.iir_data_gsen_y.fir2_dly2 = 0;
    alg_data->gsen_pedometer.iir_data_gsen_y.sec2_dly = 0;
    alg_data->gsen_pedometer.iir_data_gsen_y.sec2_dly2 = 0;
    alg_data->gsen_pedometer.iir_data_gsen_y.out = 0;
    alg_data->gsen_pedometer.iir_data_gsen_z.flt_in_dly = 0;
    alg_data->gsen_pedometer.iir_data_gsen_z.flt_in_dly2 = 0;
    alg_data->gsen_pedometer.iir_data_gsen_z.sec1_dly = 0;
    alg_data->gsen_pedometer.iir_data_gsen_z.sec1_dly2 = 0;
    alg_data->gsen_pedometer.iir_data_gsen_z.sec1_out = 0;
    alg_data->gsen_pedometer.iir_data_gsen_z.fir2_dly = 0;
    alg_data->gsen_pedometer.iir_data_gsen_z.fir2_dly2 = 0;
    alg_data->gsen_pedometer.iir_data_gsen_z.sec2_dly = 0;
    alg_data->gsen_pedometer.iir_data_gsen_z.sec2_dly2 = 0;
    alg_data->gsen_pedometer.iir_data_gsen_z.out = 0;
		
    alg_data->ppg_signal_quality.con_cnt = 0;
    alg_data->ppg_signal_quality.nocon_cnt = 0;
    alg_data->ppg_signal_quality.npeak_cnt = 0;
    alg_data->ppg_signal_quality.sq_con = 0;
    alg_data->ppg_signal_quality.sq_pow = 0;
    alg_data->ppg_signal_quality.sq_score = 0;
    for (ii = 0; ii < 4; ii++)
    {
        alg_data->ppg_signal_quality.hr_peak_fifo[ii] = 0;
    }
		
    return true;
}
bool tyhx_motion_pedometer(int16_t gsen_data_x, int16_t gsen_data_y, int16_t gsen_data_z)
{
    uint16_t ii;
    int16_t gsen_data_flt_x = 0;
    int16_t gsen_data_flt_y = 0;
    int16_t gsen_data_flt_z = 0;
    int32_t gsen_data_pow = 0;
	uint8_t motion_pow_lv_temp = 0;

    int16_t g_fir1_x = 0;
    int16_t g_fir2_x = 0;
    int16_t g_fir1_y = 0;
    int16_t g_fir2_y = 0;
    int16_t g_fir1_z = 0;
    int16_t g_fir2_z = 0;


    alg_data->gsen_pedometer.gsen_data_cnt++;
    if (alg_data->gsen_pedometer.gsen_data_cnt == 100000)
    {
        alg_data->gsen_pedometer.gsen_data_cnt = 0;
    }

    g_fir1_x = gsen_data_x * FIR1_B1 + (alg_data->gsen_pedometer.iir_data_gsen_x.flt_in_dly) * FIR1_B2 \
               + (alg_data->gsen_pedometer.iir_data_gsen_x.flt_in_dly2) * FIR1_B3;

    alg_data->gsen_pedometer.iir_data_gsen_x.flt_in_dly2 = alg_data->gsen_pedometer.iir_data_gsen_x.flt_in_dly;
    alg_data->gsen_pedometer.iir_data_gsen_x.flt_in_dly  = gsen_data_x;

    alg_data->gsen_pedometer.iir_data_gsen_x.sec1_out = g_fir1_x - ((IIR1_A2 * (alg_data->gsen_pedometer.iir_data_gsen_x.sec1_dly) + \
            IIR1_A3 * (alg_data->gsen_pedometer.iir_data_gsen_x.sec1_dly2)) >> 6);

    alg_data->gsen_pedometer.iir_data_gsen_x.sec1_dly2 = alg_data->gsen_pedometer.iir_data_gsen_x.sec1_dly;
    alg_data->gsen_pedometer.iir_data_gsen_x.sec1_dly = alg_data->gsen_pedometer.iir_data_gsen_x.sec1_out;

    g_fir2_x = alg_data->gsen_pedometer.iir_data_gsen_x.sec1_out * FIR2_B1 + \
               (alg_data->gsen_pedometer.iir_data_gsen_x.fir2_dly) * FIR2_B2 + \
               (alg_data->gsen_pedometer.iir_data_gsen_x.fir2_dly2) * FIR2_B3;

    alg_data->gsen_pedometer.iir_data_gsen_x.fir2_dly2 = alg_data->gsen_pedometer.iir_data_gsen_x.fir2_dly;
    alg_data->gsen_pedometer.iir_data_gsen_x.fir2_dly = alg_data->gsen_pedometer.iir_data_gsen_x.sec1_out;

    alg_data->gsen_pedometer.iir_data_gsen_x.out = g_fir2_x - ((IIR2_A2 * (alg_data->gsen_pedometer.iir_data_gsen_x.sec2_dly) + \
            IIR2_A3 * (alg_data->gsen_pedometer.iir_data_gsen_x.sec2_dly2)) >> 6);

    alg_data->gsen_pedometer.iir_data_gsen_x.sec2_dly2 = alg_data->gsen_pedometer.iir_data_gsen_x.sec2_dly;
    alg_data->gsen_pedometer.iir_data_gsen_x.sec2_dly = alg_data->gsen_pedometer.iir_data_gsen_x.out;


    g_fir1_y = gsen_data_y * FIR1_B1 + (alg_data->gsen_pedometer.iir_data_gsen_y.flt_in_dly) * FIR1_B2 \
               + (alg_data->gsen_pedometer.iir_data_gsen_y.flt_in_dly2) * FIR1_B3;

    alg_data->gsen_pedometer.iir_data_gsen_y.flt_in_dly2 = alg_data->gsen_pedometer.iir_data_gsen_y.flt_in_dly;
    alg_data->gsen_pedometer.iir_data_gsen_y.flt_in_dly  = gsen_data_y;

    alg_data->gsen_pedometer.iir_data_gsen_y.sec1_out = g_fir1_y - ((IIR1_A2 * (alg_data->gsen_pedometer.iir_data_gsen_y.sec1_dly) + \
            IIR1_A3 * (alg_data->gsen_pedometer.iir_data_gsen_y.sec1_dly2)) >> 6);

    alg_data->gsen_pedometer.iir_data_gsen_y.sec1_dly2 = alg_data->gsen_pedometer.iir_data_gsen_y.sec1_dly;
    alg_data->gsen_pedometer.iir_data_gsen_y.sec1_dly = alg_data->gsen_pedometer.iir_data_gsen_y.sec1_out;

    g_fir2_y = alg_data->gsen_pedometer.iir_data_gsen_y.sec1_out * FIR2_B1 + \
               (alg_data->gsen_pedometer.iir_data_gsen_y.fir2_dly) * FIR2_B2 + \
               (alg_data->gsen_pedometer.iir_data_gsen_y.fir2_dly2) * FIR2_B3;

    alg_data->gsen_pedometer.iir_data_gsen_y.fir2_dly2 = alg_data->gsen_pedometer.iir_data_gsen_y.fir2_dly;
    alg_data->gsen_pedometer.iir_data_gsen_y.fir2_dly = alg_data->gsen_pedometer.iir_data_gsen_y.sec1_out;

    alg_data->gsen_pedometer.iir_data_gsen_y.out = g_fir2_y - ((IIR2_A2 * (alg_data->gsen_pedometer.iir_data_gsen_y.sec2_dly) + \
            IIR2_A3 * (alg_data->gsen_pedometer.iir_data_gsen_y.sec2_dly2)) >> 6);

    alg_data->gsen_pedometer.iir_data_gsen_y.sec2_dly2 = alg_data->gsen_pedometer.iir_data_gsen_y.sec2_dly;
    alg_data->gsen_pedometer.iir_data_gsen_y.sec2_dly = alg_data->gsen_pedometer.iir_data_gsen_y.out;


    g_fir1_z = gsen_data_z * FIR1_B1 + (alg_data->gsen_pedometer.iir_data_gsen_z.flt_in_dly) * FIR1_B2 \
               + (alg_data->gsen_pedometer.iir_data_gsen_z.flt_in_dly2) * FIR1_B3;

    alg_data->gsen_pedometer.iir_data_gsen_z.flt_in_dly2 = alg_data->gsen_pedometer.iir_data_gsen_z.flt_in_dly;
    alg_data->gsen_pedometer.iir_data_gsen_z.flt_in_dly  = gsen_data_z;

    alg_data->gsen_pedometer.iir_data_gsen_z.sec1_out = g_fir1_z - ((IIR1_A2 * (alg_data->gsen_pedometer.iir_data_gsen_z.sec1_dly) + \
            IIR1_A3 * (alg_data->gsen_pedometer.iir_data_gsen_z.sec1_dly2)) >> 6);

    alg_data->gsen_pedometer.iir_data_gsen_z.sec1_dly2 = alg_data->gsen_pedometer.iir_data_gsen_z.sec1_dly;
    alg_data->gsen_pedometer.iir_data_gsen_z.sec1_dly = alg_data->gsen_pedometer.iir_data_gsen_z.sec1_out;

    g_fir2_z = alg_data->gsen_pedometer.iir_data_gsen_z.sec1_out * FIR2_B1 + \
               (alg_data->gsen_pedometer.iir_data_gsen_z.fir2_dly) * FIR2_B2 + \
               (alg_data->gsen_pedometer.iir_data_gsen_z.fir2_dly2) * FIR2_B3;

    alg_data->gsen_pedometer.iir_data_gsen_z.fir2_dly2 = alg_data->gsen_pedometer.iir_data_gsen_z.fir2_dly;
    alg_data->gsen_pedometer.iir_data_gsen_z.fir2_dly = alg_data->gsen_pedometer.iir_data_gsen_z.sec1_out;

    alg_data->gsen_pedometer.iir_data_gsen_z.out = g_fir2_z - ((IIR2_A2 * (alg_data->gsen_pedometer.iir_data_gsen_z.sec2_dly) + \
            IIR2_A3 * (alg_data->gsen_pedometer.iir_data_gsen_z.sec2_dly2)) >> 6);

    alg_data->gsen_pedometer.iir_data_gsen_z.sec2_dly2 = alg_data->gsen_pedometer.iir_data_gsen_z.sec2_dly;
    alg_data->gsen_pedometer.iir_data_gsen_z.sec2_dly = alg_data->gsen_pedometer.iir_data_gsen_z.out;

    gsen_data_flt_x = (alg_data->gsen_pedometer.iir_data_gsen_x.out);
    gsen_data_flt_y = (alg_data->gsen_pedometer.iir_data_gsen_y.out);
    gsen_data_flt_z = (alg_data->gsen_pedometer.iir_data_gsen_z.out);

#ifdef NO_MATH_LIB
    gsen_data_pow = hr_sqrt(gsen_data_flt_x * gsen_data_flt_x + gsen_data_flt_y * gsen_data_flt_y \
                         + gsen_data_flt_z * gsen_data_flt_z);
#else
    gsen_data_pow = sqrt(gsen_data_flt_x * gsen_data_flt_x + gsen_data_flt_y * gsen_data_flt_y \
                         + gsen_data_flt_z * gsen_data_flt_z);
#endif

    for (ii = 4; ii > 0; ii--)
    {
        alg_data->gsen_pedometer.pow_all_fifo[ii] = alg_data->gsen_pedometer.pow_all_fifo[ii - 1];
    }
    alg_data->gsen_pedometer.pow_all_fifo[0] = gsen_data_pow;

    if (alg_data->gsen_pedometer.pow_all_fifo[2] > alg_data->gsen_pedometer.pow_all_fifo[4] && \
        alg_data->gsen_pedometer.pow_all_fifo[2] > alg_data->gsen_pedometer.pow_all_fifo[3] && \
        alg_data->gsen_pedometer.pow_all_fifo[2] > alg_data->gsen_pedometer.pow_all_fifo[1] && \
        alg_data->gsen_pedometer.pow_all_fifo[2] > alg_data->gsen_pedometer.pow_all_fifo[0])
    {
        alg_data->gsen_pedometer.gsen_powpeak = alg_data->gsen_pedometer.pow_all_fifo[2];
    }
    else if (alg_data->gsen_pedometer.pow_all_fifo[2] == alg_data->gsen_pedometer.pow_all_fifo[4] && \
             alg_data->gsen_pedometer.pow_all_fifo[2] == alg_data->gsen_pedometer.pow_all_fifo[3] && \
             alg_data->gsen_pedometer.pow_all_fifo[2] == alg_data->gsen_pedometer.pow_all_fifo[1] && \
             alg_data->gsen_pedometer.pow_all_fifo[2] == alg_data->gsen_pedometer.pow_all_fifo[0] && \
             alg_data->gsen_pedometer.pow_all_fifo[2] <= 20)
    {
        alg_data->gsen_pedometer.gsen_powpeak = alg_data->gsen_pedometer.pow_all_fifo[2];
    }

    if (alg_data->gsen_pedometer.gsen_powpeak > 500)
    {
        alg_data->gsen_pedometer.pow_all_temp = (alg_data->gsen_pedometer.pow_all_temp * 127 + alg_data->gsen_pedometer.gsen_powpeak) >> 7;
    }
    else
    {
        alg_data->gsen_pedometer.pow_all_temp = (alg_data->gsen_pedometer.pow_all_temp * 15 + alg_data->gsen_pedometer.gsen_powpeak) >> 4;
    }

    if(alg_data->gsen_pedometer.pow_all_temp<STATIC_THRE)
    {
        motion_pow_lv_temp = 0;
    }
    else if(alg_data->gsen_pedometer.pow_all_temp<500)
	{
        motion_pow_lv_temp = 1;
	}
    else if(alg_data->gsen_pedometer.pow_all_temp<1500)
	{
        motion_pow_lv_temp = 2; 
	}
    else if(alg_data->gsen_pedometer.pow_all_temp<4000)
	{
        motion_pow_lv_temp = 3;
	}
    else if(alg_data->gsen_pedometer.pow_all_temp<8000)
	{
        motion_pow_lv_temp = 4; 
	}
    else
	{
        motion_pow_lv_temp = 5;
	}
    
    if(motion_pow_lv_temp>0)
	{
        if(alg_data->gsen_pedometer.motion_pow_lv1_cnt<25)
		{
            alg_data->gsen_pedometer.motion_pow_lv1_cnt++;
        }
        if(alg_data->gsen_pedometer.motion_pow_lv0_cnt>0)
		{
            alg_data->gsen_pedometer.motion_pow_lv0_cnt--;
        }
	}
    else
	{
        if(alg_data->gsen_pedometer.motion_pow_lv1_cnt>0)
		{
            alg_data->gsen_pedometer.motion_pow_lv1_cnt--;
        }
        if(alg_data->gsen_pedometer.motion_pow_lv0_cnt<25)
		{
            alg_data->gsen_pedometer.motion_pow_lv0_cnt++;
        }
	}
    
    if(motion_pow_lv_temp>1)
	{
        if(alg_data->gsen_pedometer.motion_pow_lv2_cnt<25)
		{
            alg_data->gsen_pedometer.motion_pow_lv2_cnt++;
        }
	}
    else
	{
        if(alg_data->gsen_pedometer.motion_pow_lv2_cnt>0)
		{
            alg_data->gsen_pedometer.motion_pow_lv2_cnt--;
        }
	}
    
    if(motion_pow_lv_temp>2)
	{
        if(alg_data->gsen_pedometer.motion_pow_lv3_cnt<25)
		{
            alg_data->gsen_pedometer.motion_pow_lv3_cnt++;
        }
	}
    else
	{
        if(alg_data->gsen_pedometer.motion_pow_lv3_cnt>0)
		{
            alg_data->gsen_pedometer.motion_pow_lv3_cnt--;
        }
	}
    
    if(motion_pow_lv_temp>3)
	{
        if(alg_data->gsen_pedometer.motion_pow_lv4_cnt<25)
		{
            alg_data->gsen_pedometer.motion_pow_lv4_cnt++;
        }
	}
    else
	{
        if(alg_data->gsen_pedometer.motion_pow_lv4_cnt>0)
		{
            alg_data->gsen_pedometer.motion_pow_lv4_cnt--;
        }
	}
    
    if(motion_pow_lv_temp>4)
    {
        if(alg_data->gsen_pedometer.motion_pow_lv5_cnt<25)
        {
            alg_data->gsen_pedometer.motion_pow_lv5_cnt++;
        }
    }
    else
    {
        if(alg_data->gsen_pedometer.motion_pow_lv5_cnt>0)
        {
            alg_data->gsen_pedometer.motion_pow_lv5_cnt--;
        }
    }

    if(alg_data->gsen_pedometer.motion_pow_lv0_cnt >= 25)
    {
        alg_data->gsen_pedometer.motion_status = 0;
    }
    else
    {
        switch(alg_data->gsen_pedometer.motion_status)
        {
            case 0:
                if(alg_data->gsen_pedometer.motion_pow_lv1_cnt==25)
                {
                    alg_data->gsen_pedometer.motion_status = 1;
                }
                break;
            case 1:
                if(alg_data->gsen_pedometer.motion_pow_lv1_cnt==0)
                {
                    alg_data->gsen_pedometer.motion_status = 0;
                }
                else if(alg_data->gsen_pedometer.motion_pow_lv2_cnt==25)
                {
                    alg_data->gsen_pedometer.motion_status = 2;
                }
                break;
            case 2:
                if(alg_data->gsen_pedometer.motion_pow_lv2_cnt==0)
                {
                    alg_data->gsen_pedometer.motion_status = 1;
                }
                else if(alg_data->gsen_pedometer.motion_pow_lv3_cnt==25)
                {
                    alg_data->gsen_pedometer.motion_status = 3;
                }
                break;
            case 3:
                if(alg_data->gsen_pedometer.motion_pow_lv3_cnt==0)
                {
                    alg_data->gsen_pedometer.motion_status = 2;
                }
                else if(alg_data->gsen_pedometer.motion_pow_lv4_cnt==25)
                {
                    alg_data->gsen_pedometer.motion_status = 4;
                }
                break;
            case 4:
                if(alg_data->gsen_pedometer.motion_pow_lv4_cnt==0)
                {
                    alg_data->gsen_pedometer.motion_status = 3;
                }
                else if(alg_data->gsen_pedometer.motion_pow_lv5_cnt==25)
                {
                    alg_data->gsen_pedometer.motion_status = 5;
                }
                break;						
            case 5:
                if(alg_data->gsen_pedometer.motion_pow_lv5_cnt==0)
                {
                    alg_data->gsen_pedometer.motion_status = 4;
                }
                break;	
            default:
                break;
		}
    }
    //DEBUG_PRINTF(0, "%d  %d  %d  %d  %d\r\n",alg_data->data_cnt, gsen_data_flt_x, gsen_data_flt_y, gsen_data_flt_z,STATIC_THRE);
    //DEBUG_PRINTF(0, "%d  %d  %d  %d\r\n",alg_data->data_cnt, gsen_data_pow, alg_data->gsen_pedometer.pow_all_temp, alg_data->gsen_pedometer.motion_status);
    return true;
}

void tyhx_hrs_alg_close(void)
{
    if(alg_data == 0)
    {
        return;
    }
    else
    {
        alg_data = 0;
    }
}

bool tyhx_hrs_alg_open(void)
{
    uint16_t ii;
	alg_status = MSG_HRS_ALG_OPEN;
	living_status = MSG_LIVING_INITIAL;
    alg_data = (alg_data_t*)alg_ram;
    //alg_data = &_alg_data;
    //alg_data->alg_status = MSG_HRS_ALG_OPEN;

    alg_data->hr_result = 0;
	alg_data->hr_internal = 72;
    alg_data->hr_internal_pre = 72;
    alg_data->hr_times_8 = 0;
    alg_data->data_cnt = 0;
    alg_data->iir_data.fir2_dly = 0;
    alg_data->iir_data.fir2_dly2 = 0;
    alg_data->iir_data.sec1_dly = 0;
    alg_data->iir_data.sec1_dly2 = 0;
    alg_data->iir_data.sec1_out = 0;
    alg_data->iir_data.fir2_dly = 0;
    alg_data->iir_data.fir2_dly2 = 0;
    alg_data->iir_data.sec2_dly = 0;
    alg_data->iir_data.sec2_dly2 = 0;
    alg_data->iir_data.out = 0;
	alg_data->raw_data_fifo[3] = 0;
    alg_data->raw_data_fifo[2] = 0;
    alg_data->raw_data_fifo[1] = 0;
    alg_data->raw_data_fifo[0] = 0;
	alg_data->abnormal_jump_delta = 0;
    alg_data->new_raw_data_dcrm_pre = 0;

    alg_data->start_ind = 0;
    for (ii = 0; ii < 110; ii++)
    {
        alg_data->fft_phase[ii] = 0;
		alg_data->fft_phase_start[ii] = 0;
        alg_data->hrm_fft_out[ii][0] = 0;
        alg_data->hrm_fft_out[ii][1] = 0;
		alg_data->gsen_x_fft_out[ii][0] = 0;
        alg_data->gsen_x_fft_out[ii][1] = 0;
    }
    for (ii = 0; ii < FFT_WIN_SIZE; ii++)
    {
        alg_data->hrm_data_fifo[ii] = 0;
        alg_data->gsen_data_fifo[ii] = 0;
    }

	alg_data->senddata_motion_thisdex = 0;
    alg_data->new_start = 1;
    alg_data->hr_hold = 0;
    alg_data->hr_block = 0;
	alg_data->no_found_cnt = 0;

    for (ii = 0; ii < HR_PEAKS_NUM; ii++)
    {
        alg_data->hr_track_peaks[ii] = 0;
        alg_data->hr_track_peaks_cnt_pre[ii] = 0;
    }
    alg_data->run_state_cnt = 0;
    alg_data->walk_state_cnt = 0;
    alg_data->gsenx_peak_pre = 0;
    alg_data->hr_max_peak_pre = 0;
    alg_data->run_peak = 0;
    alg_data->walk_peak = 0;
    alg_data->hq_hr_cnt = 0;
    alg_data->hq_hr_walk_cnt = 0;
    alg_data->hq_hr = 0;
    alg_data->hq_hr_walk = 0;

    alg_data->runrun_cnt = 0;
    alg_data->run_dn_cnt = 0;
    alg_data->new_raw_data_pre = 0;
    alg_data->signal_err_cnt = 0;
    alg_data->signal_block_cnt = 0;  
    alg_data->first_shot_1 = 80;
    alg_data->first_shot_2 = 80;		
	for (ii = 0; ii < 4; ii++)
    {
        alg_data->hr_quality_fifo[ii] = 0;
    }
    alg_data->hr_quality = 4;
    alg_data->calorie = 0;
		
	alg_data->bp_cnt = 0;
    alg_data->sbp = 0;
    alg_data->dbp = 0;
	alg_data->hr_runpeak_follow_cnt = 0;
    alg_data->hrm_snr_smooth = 20;  
    for(ii=0;ii<16;ii++)
    {
        alg_data->check_living_green[ii] = 0;
    }
    alg_data->check_living_green_std = 0;
    alg_data->check_living_green_std_smooth = 0;
    for(ii=0;ii<5;ii++)
    {
        alg_data->check_living_green_std_fifo[ii] = 0;
        alg_data->hrm_snr_value_fifo[ii] = 0;
    }
		
    tyhx_gsen_pedometer_init();
    return  true;
}

bool  tyhx_hrs_alg_open_deep(void)
{
    tyhx_hrs_alg_open();
    return  true;    
}

uint16_t tyhx_hrm_snr(uint16_t *hrm_max_index, uint32_t *hrm_max_pow2)
{			
    uint16_t ii=0,kk=0;  
    uint32_t tot_pow2 = 0;
    uint32_t sig_pow2 = 0;
    uint32_t sig2_pow2 = 0;
    uint32_t sig3_pow2 = 0;
    uint8_t data_move_val = 18;
    uint32_t max_pow2_cali = 0;
    uint16_t hrm_snr = 0;
  
//    for(ii=0;ii<6;ii++)
//    {
//        DEBUG_PRINTF("%d,%d\r\n",hrm_max_index[ii],hrm_max_pow2[ii]);
//    }
    
    max_pow2_cali = hrm_max_pow2[HR_PEAKS_NUM-1];
    while(max_pow2_cali > 1000000)
    {
        data_move_val++;
        max_pow2_cali = max_pow2_cali>>2;
    }
    
    for (ii=0;ii<110;ii++) 
    {
        tot_pow2 = tot_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                 + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
    }
    
    if ( hrm_max_index[HR_PEAKS_NUM-1] >= 7 && hrm_max_index[HR_PEAKS_NUM-1] <= 102 )
    {
        for (ii = hrm_max_index[HR_PEAKS_NUM-1]-7;ii <= hrm_max_index[HR_PEAKS_NUM-1]+7;ii++)
        {
             sig_pow2 = sig_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                        + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
        }
    }     
    else if ( hrm_max_index[HR_PEAKS_NUM-1] < 7 )
    {
        for (ii = 0;ii <= hrm_max_index[HR_PEAKS_NUM-1]+7;ii++)
        {
             sig_pow2 = sig_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                        + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
        }
    }
    else if ( hrm_max_index[HR_PEAKS_NUM-1] > 102 )
    {
        for (ii = hrm_max_index[HR_PEAKS_NUM-1]-7;ii<110;ii++)
        {
             sig_pow2 = sig_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                        + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
        }
    }      
      
    for(kk=0;kk<HR_PEAKS_NUM-1;kk++)
    {
        if ( (abs(((hrm_max_index[HR_PEAKS_NUM-1]+27)*750>>9) * 2 - ((hrm_max_index[kk]+27)*750>>9)) <= 3  \
            || abs(((hrm_max_index[kk]+27)*750>>9) * 2 - ((hrm_max_index[HR_PEAKS_NUM-1]+27)*750>>9)) <= 3)  \
            && hrm_max_pow2[HR_PEAKS_NUM-1] > hrm_max_pow2[kk] * 2 && hrm_max_index[kk]>0 )
        {
            sig2_pow2 = 0;
            if ( hrm_max_index[kk] >= 7 && hrm_max_index[kk] <= 102 )
            {
                for (ii = hrm_max_index[kk]-7;ii <= hrm_max_index[kk]+7;ii++)
                {
                     sig2_pow2 = sig2_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                            + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
                }
            }     
            else if ( hrm_max_index[kk] < 7 )
            {
                for (ii = 0;ii <= hrm_max_index[kk]+7;ii++)
                {
                     sig2_pow2 = sig2_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                            + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
                }
            }
            else if ( hrm_max_index[kk] > 102 )
            {
                for (ii = hrm_max_index[kk]-7;ii < 110;ii++)
                {
                     sig2_pow2 = sig2_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                            + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
                }
            } 
        }
        if ( abs(abs(hrm_max_index[HR_PEAKS_NUM-1] - hrm_max_index[kk])-10) <= 1  \
            && hrm_max_pow2[HR_PEAKS_NUM-1] > hrm_max_pow2[kk] * 2 && hrm_max_index[kk]>0)
        {
            if ( hrm_max_index[kk] >= 3 && hrm_max_index[kk] <= 106 )
            {
                for (ii = hrm_max_index[kk]-3;ii <= hrm_max_index[kk]+3;ii++)
                {
                     sig3_pow2 = sig3_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                            + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
                }
            }     
            else if ( hrm_max_index[kk] < 3 )
            {
                for (ii = 0;ii <= hrm_max_index[kk]+3;ii++)
                {
                     sig3_pow2 = sig3_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                            + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
                }
            }
            else if ( hrm_max_index[kk] > 106 )
            {
                for (ii = hrm_max_index[kk]-3;ii < 110;ii++)
                {
                     sig3_pow2 = sig3_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                            + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
                }
            }  
        }
    }
    
    if ( (abs(((hrm_max_index[HR_PEAKS_NUM-1]+27)*750>>9) * 2 - ((hrm_max_index[HR_PEAKS_NUM-2]+27)*750>>9)) <= 4  \
        || abs(((hrm_max_index[HR_PEAKS_NUM-2]+27)*750>>9) * 2 - ((hrm_max_index[HR_PEAKS_NUM-1]+27)*750>>9)) <= 4)  \
        && sig2_pow2==0)
    {
        sig2_pow2 = 0;
        if ( hrm_max_index[HR_PEAKS_NUM-2] >= 7 && hrm_max_index[HR_PEAKS_NUM-2] <= 102 )
        {
            for (ii = hrm_max_index[HR_PEAKS_NUM-2]-7;ii <= hrm_max_index[HR_PEAKS_NUM-2]+7;ii++)
            {
                 sig2_pow2 = sig2_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                        + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
            }
        }     
        else if ( hrm_max_index[HR_PEAKS_NUM-2] < 7 )
        {
            for (ii = 0;ii <= hrm_max_index[HR_PEAKS_NUM-2]+7;ii++)
            {
                 sig2_pow2 = sig2_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                        + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
            }
        }
        else if ( hrm_max_index[HR_PEAKS_NUM-2] > 102 )
        {
            for (ii = hrm_max_index[HR_PEAKS_NUM-2]-7;ii < 110;ii++)
            {
                 sig2_pow2 = sig2_pow2 + (alg_data->hrm_fft_out[ii][0]>>data_move_val)*(alg_data->hrm_fft_out[ii][0]>>data_move_val)  \
                        + (alg_data->hrm_fft_out[ii][1]>>data_move_val)*(alg_data->hrm_fft_out[ii][1]>>data_move_val);
            }
        } 
    }
       
    if ( tot_pow2 > sig_pow2 + sig2_pow2 + sig3_pow2 + 64)
    {
        hrm_snr = (sig_pow2 + sig2_pow2 + sig3_pow2) / ((tot_pow2 - sig_pow2 - sig2_pow2 - sig3_pow2)>>3);
        if(hrm_snr > 100)
        {
          hrm_snr = 100;
        }
    }
    else
    {
        hrm_snr = 100; 
    }
    if(hrm_snr == 0)
    {
        hrm_snr = 1;
    }

    DEBUG_PRINTF("%d   %d %d %d   %d %d\r\n" ,alg_data->data_cnt,tot_pow2,sig_pow2,sig2_pow2,hrm_snr,data_move_val);

    return hrm_snr;  
}

int32_t tyhx_check_living_std(int32_t *data_in)
{
    uint8_t ii = 0;
    int32_t data_sum = 0;
    int32_t data_ave = 0;
    int32_t data_dif = 0;
    int32_t data_dif_sum = 0;
    int32_t data_dif_ave = 0;
    
    for(ii=0;ii<16;ii++)
    {
        data_sum = data_sum + data_in[ii];
    }
    data_ave = data_sum>>4;
    for(ii=0;ii<16;ii++)
    {
        data_dif = abs(data_in[ii] - data_ave);
        if(data_dif>2000)
        {
            data_dif = 2000;
        }
        data_dif_sum = data_dif_sum + data_dif;
    }
    data_dif_ave = data_dif_sum>>4;
    
    return data_dif_ave;
}

bool tyhx_hrs_alg_send_data(int32_t *new_raw_data, uint8_t dat_len, int16_t *gsen_data_x, int16_t *gsen_data_y, int16_t *gsen_data_z)
{
    uint8_t  i = 0;
		if(alg_data == NULL)
		{
			return false;
		}

    for (i = 0; i < dat_len; i++)
    {
        tyhx_hrs_alg(new_raw_data[i], gsen_data_x[i], gsen_data_y[i], gsen_data_z[i]);
        //SEGGER_RTT_printf(0,"%d %d %d %d %d\r\n" , i, new_raw_data[i], gsen_data_x[i],gsen_data_y[i],gsen_data_z[i]);
    }

    return true;
}

bool tyhx_hrs_alg(int32_t new_raw_data, int16_t gsen_data_x, int16_t gsen_data_y, int16_t gsen_data_z)
{
#ifdef MTK25XX
    // stack 太小， 定义为全局变量
    uint8_t  ii = 0, jj = 0, kk = 0;
    tyhx_hrs_send_data_init();
#else

    ///////////////////////////////////////参数初始化开始//////////////////////////////////////////////////////////
    int32_t  fir1 = 0, fir2 = 0;
    uint8_t  hr_new = 0;
    uint16_t  ii = 0, jj = 0;

    uint32_t this_pow2 = 0, max_pow2 = 0, max_pow2_x = 0;
    uint32_t pre1_pow2 = 0; 
    uint32_t pre2_pow2 = 0;
    uint32_t hr_track_pow_thr = 10;
    uint16_t  gsen_x_max_index[GSEN_PEAKS_NUM] = {0}; 
	uint16_t  gsen_x_max_value[GSEN_PEAKS_NUM] = {0};
    uint32_t gsen_x_max_pow2[GSEN_PEAKS_NUM] = {0}; 
    uint16_t  hrm_max_index[HR_PEAKS_NUM] = {0};
	uint16_t  hrm_max_value[HR_PEAKS_NUM] = {0}; 
    uint32_t hrm_max_pow2[HR_PEAKS_NUM] = {0};
    bool fail_frame1 = 0, fail_frame2 = 0;
    bool peaks_within_range = 0;
    bool block = 0;
    bool found = 0;
    uint8_t hr_valid_ind = 0;
    uint16_t center_hr = 0;
    int8_t hrm_jump_uplim = 8, hrm_jump_dnlim = 8;
    int8_t  gsen_block = 0, gsen_block_gen = 0;

    int32_t new_raw_data_dcrm = 0;
    int16_t gsen_data_x_in = 0, gsen_data_y_in = 0, gsen_data_z_in = 0;

    int8_t hr_clip_inside_up = 0;
    int8_t hr_clip_inside_dn = 0;

    uint8_t gsen_block_start = 0;
    uint8_t track_peak_flag = 0;
    uint8_t hr_track_peaks_cnt[HR_PEAKS_NUM] = {0};
    uint8_t hr_peaks_cnt_max = 0;
		
    bool  gx_run_state = 0,  run_state = 0; 
    bool  gx_walk_state = 0,  walk_state = 0; 
    bool  hq_hr_walk_vld = 0;

    uint8_t step_fre = 0;
    uint16_t hrm_snr = 0;
    //uint8_t hr_quality_temp = 0;
    int32_t check_living_green_std_sum = 0;
    int32_t hrm_snr_value_sum = 0;
		
    uint8_t time_range = 0;
    uint8_t phase_sin_1q = 0;	
    uint8_t	phase_cos_1q = 0;              
    int32_t	sin_c = 0;
    int32_t	cos_c = 0;	
    int32_t temp_c = 0;
#endif

    ///////////开始计算gsensor矢量///////////////////////////////////////
    gsen_data_x_in = gsen_data_x >> 4;
    gsen_data_y_in = gsen_data_y >> 4;
    gsen_data_z_in = gsen_data_z >> 4;

    alg_data->raw_data_fifo[3] = alg_data->raw_data_fifo[2];
    alg_data->raw_data_fifo[2] = alg_data->raw_data_fifo[1];
    alg_data->raw_data_fifo[1] = alg_data->raw_data_fifo[0];
    alg_data->raw_data_fifo[0] = new_raw_data;

    if (alg_data->data_cnt > HRM_CHIP_SETTLE - 2)
		{
			if ((((alg_data->raw_data_fifo[1] - alg_data->raw_data_fifo[2]) > GLITCH_THRES) && ((alg_data->raw_data_fifo[1] - new_raw_data) > GLITCH_THRES)) || \
                (((alg_data->raw_data_fifo[1] - alg_data->raw_data_fifo[2]) < -GLITCH_THRES) && ((alg_data->raw_data_fifo[1] - new_raw_data) < -GLITCH_THRES)))
        {
            new_raw_data = alg_data->raw_data_fifo[2];
            alg_data->raw_data_fifo[1] = alg_data->raw_data_fifo[2];
        }
        else
        {
            new_raw_data = alg_data->raw_data_fifo[1];
        }
    }
    //DEBUG_PRINTF(0,"%d %d %d %d \r\n" , alg_data->data_cnt , alg_data->raw_data_fifo[0] , new_raw_data, agc_status );
    // 20181108 add for iwown ericy

    ///Remove abnormal judge///////////////////////////////////////////////////////////////////////////
    if (alg_data->data_cnt > HRM_CHIP_SETTLE)
    {
        if ((abs((new_raw_data - alg_data->abnormal_jump_delta) - alg_data->new_raw_data_dcrm_pre) > ABNORMAL_JUMP_THRES))
        {
            alg_data->abnormal_jump_delta = new_raw_data - alg_data->new_raw_data_dcrm_pre;
        }
        new_raw_data_dcrm = new_raw_data - alg_data->abnormal_jump_delta;
        //DEBUG_PRINTF(0,"%d %d %d %d\r\n",alg_data->data_cnt, alg_data->new_raw_data_dcrm_pre, new_raw_data_dcrm, alg_data->abnormal_jump_thres);
    }
    else
    {
        new_raw_data_dcrm = new_raw_data;
    }
    alg_data->new_raw_data_dcrm_pre = new_raw_data_dcrm;
		//DEBUG_PRINTF("%d %d %d\r\n",alg_data->data_cnt, new_raw_data, new_raw_data_dcrm);
		
    if (abs(new_raw_data - alg_data->new_raw_data_pre) > 50000)
    {
        if (alg_data->signal_err_cnt < 10)
        {
            alg_data->signal_err_cnt++;
        }
    }
    else
    {
        if (alg_data->signal_err_cnt > 0)
        {
            alg_data->signal_err_cnt--;
        }
    }
    if (alg_data->signal_err_cnt >= 5 && alg_data->data_cnt > 320)
    {
        alg_data->signal_block_cnt = 50;
    }
    else
    {
        if (alg_data->signal_block_cnt > 0)
        {
            alg_data->signal_block_cnt--;
        }
    }
    alg_data->new_raw_data_pre = new_raw_data;

		
    if (alg_data->data_cnt == HRM_CHIP_SETTLE)
    {
        alg_data->iir_data.flt_in_dly = alg_data->raw_data_fifo[1]; 
        alg_data->iir_data.flt_in_dly2 = alg_data->raw_data_fifo[1];
        alg_data->gsen_pedometer.iir_data_gsen_x.flt_in_dly = gsen_data_x_in;
        alg_data->gsen_pedometer.iir_data_gsen_x.flt_in_dly2 = gsen_data_x_in;
        alg_data->gsen_pedometer.iir_data_gsen_y.flt_in_dly = gsen_data_y_in;
        alg_data->gsen_pedometer.iir_data_gsen_y.flt_in_dly2 = gsen_data_y_in;
        alg_data->gsen_pedometer.iir_data_gsen_z.flt_in_dly = gsen_data_z_in;
        alg_data->gsen_pedometer.iir_data_gsen_z.flt_in_dly2 = gsen_data_z_in;
    }
    
    ///////////////////////////  check_living_std /////////////////////////////
    if(alg_data->data_cnt > HRM_CHIP_SETTLE+HRM_FLT_SETTLE)
    {
        if(alg_data->data_cnt%3 == 0)
        {
            for(ii=0;ii<15;ii++)
            {
                alg_data->check_living_green[ii] = alg_data->check_living_green[ii+1];
            }
            alg_data->check_living_green[15] = new_raw_data_dcrm>>3;
        }
    }
    if(alg_data->data_cnt >= HRM_CHIP_SETTLE+HRM_FLT_SETTLE+75 && alg_data->data_cnt%25 == 0)
    {
        if(alg_data->senddata_motion_thisdex > 0)
        {
            alg_data->hr_quality = 255;
        }
        alg_data->check_living_green_std = tyhx_check_living_std(alg_data->check_living_green);
        if(alg_data->data_cnt == HRM_CHIP_SETTLE+HRM_FLT_SETTLE+75)
        {
            for(ii=0;ii<5;ii++)
            {
                alg_data->check_living_green_std_fifo[ii] = alg_data->check_living_green_std;
                check_living_green_std_sum = check_living_green_std_sum + alg_data->check_living_green_std_fifo[ii];
            }
            alg_data->check_living_green_std_smooth = check_living_green_std_sum/5;
        }
        else
        {
            for(ii=0;ii<4;ii++)
            {
                alg_data->check_living_green_std_fifo[ii] = alg_data->check_living_green_std_fifo[ii+1];
                check_living_green_std_sum = check_living_green_std_sum + alg_data->check_living_green_std_fifo[ii];
            }
            alg_data->check_living_green_std_fifo[4] = alg_data->check_living_green_std;
            check_living_green_std_sum = check_living_green_std_sum + alg_data->check_living_green_std_fifo[4];
            alg_data->check_living_green_std_smooth = check_living_green_std_sum/5;
        }
        //DEBUG_PRINTF("%d,%d\r\n",alg_data->data_cnt,alg_data->check_living_green_std);
    }
    ///////////////////////////  check_living_end /////////////////////////////

    if (alg_data->data_cnt > HRM_CHIP_SETTLE)
    {
        if(gsen_data_x==0 && gsen_data_y==0 && gsen_data_z==0)
        {
        }
        else
        {
          tyhx_motion_pedometer(gsen_data_x_in, gsen_data_y_in, gsen_data_z_in);
        }

        fir1 = new_raw_data_dcrm * FIR1_B1 \
               + (alg_data->iir_data.flt_in_dly) * FIR1_B2 \
               + (alg_data->iir_data.flt_in_dly2) * FIR1_B3;

        alg_data->iir_data.flt_in_dly2 = alg_data->iir_data.flt_in_dly;
        alg_data->iir_data.flt_in_dly  = new_raw_data_dcrm;

        alg_data->iir_data.sec1_out = fir1 - ((IIR1_A2 * (alg_data->iir_data.sec1_dly) + \
                                      IIR1_A3 * (alg_data->iir_data.sec1_dly2)) >> 6);

        alg_data->iir_data.sec1_dly2 = alg_data->iir_data.sec1_dly;
        alg_data->iir_data.sec1_dly = alg_data->iir_data.sec1_out;

        fir2 = alg_data->iir_data.sec1_out * FIR2_B1 + \
               (alg_data->iir_data.fir2_dly) * FIR2_B2 + \
               (alg_data->iir_data.fir2_dly2) * FIR2_B3;

        alg_data->iir_data.fir2_dly2 = alg_data->iir_data.fir2_dly;
        alg_data->iir_data.fir2_dly = alg_data->iir_data.sec1_out;

        alg_data->iir_data.out = fir2 - ((IIR2_A2 * (alg_data->iir_data.sec2_dly) + \
                                 IIR2_A3 * (alg_data->iir_data.sec2_dly2)) >> 6);

        alg_data->iir_data.sec2_dly2 = alg_data->iir_data.sec2_dly;
        alg_data->iir_data.sec2_dly = alg_data->iir_data.out;
    } 
		
    if (alg_data->data_cnt > HRM_CHIP_SETTLE+HRM_FLT_SETTLE)
    {
        //DEBUG_PRINTF("%d %d %d\r\n",alg_data->data_cnt,alg_data->iir_data.out,alg_data->gsen_pedometer.iir_data_gsen_x.out);
        for (ii=0;ii<110;ii++) 
        {
            alg_data->fft_phase[ii] = alg_data->fft_phase[ii] + 28 + ii;		
          //DEBUG_PRINTF("%d %d %d\r\n",alg_data->data_cnt,ii,alg_data->fft_phase[ii]);
            if (alg_data->fft_phase[ii] >= 1024) 
            {
                alg_data->fft_phase[ii] = alg_data->fft_phase[ii] - 1024;		
            }							
            if(alg_data->fft_phase[ii] >= 768)
            {
                phase_sin_1q = alg_data->fft_phase[ii] - 768 ;
            }
            else if(alg_data->fft_phase[ii] >= 512)
            {
                phase_sin_1q = alg_data->fft_phase[ii] - 512 ;
            }
            else if(alg_data->fft_phase[ii] >= 256)
            {
                phase_sin_1q = alg_data->fft_phase[ii] - 256 ;							
            }
            else
            {
                phase_sin_1q = alg_data->fft_phase[ii] ;	
            }		
            phase_cos_1q = 255 - phase_sin_1q;              
            sin_c = sin_table[phase_sin_1q];
            cos_c = sin_table[phase_cos_1q];					
            if (alg_data->fft_phase[ii] < 256) {
                    //sin_c = sin_c;     ???????????????
                    //cos_c = cos_c;
            } else if (alg_data->fft_phase[ii] < 512) {
                    temp_c = sin_c;
                    sin_c = cos_c;
                    cos_c = -temp_c;
            } else if (alg_data->fft_phase[ii] < 768) {
                    sin_c = -sin_c;
                    cos_c = -cos_c;
            } else {
                    temp_c = sin_c;
                    sin_c = -cos_c;
                    cos_c = temp_c;
            }    				
            alg_data->hrm_fft_out[ii][0] = alg_data->hrm_fft_out[ii][0] + cos_c*(alg_data->iir_data.out>>3);
            alg_data->hrm_fft_out[ii][1] = alg_data->hrm_fft_out[ii][1] + sin_c*(alg_data->iir_data.out>>3);    
            alg_data->gsen_x_fft_out[ii][0] = alg_data->gsen_x_fft_out[ii][0] + cos_c*alg_data->gsen_pedometer.iir_data_gsen_x.out;
            alg_data->gsen_x_fft_out[ii][1] = alg_data->gsen_x_fft_out[ii][1] + sin_c*alg_data->gsen_pedometer.iir_data_gsen_x.out;
            //DEBUG_PRINTF("%d %d %d %d %d %d %d\r\n",alg_data->data_cnt, ii, cos_c, sin_c, alg_data->iir_data.out, alg_data->hrm_fft_out[ii][0],alg_data->hrm_fft_out[ii][1]);
        }
    }
            
    if (alg_data->data_cnt > HRM_CHIP_SETTLE+HRM_FLT_SETTLE+FFT_WIN_SIZE) 
    {
        for (ii=0;ii<110;ii++) 
        {
            alg_data->fft_phase_start[ii] = alg_data->fft_phase_start[ii] + 28 + ii;		
            if (alg_data->fft_phase_start[ii] >= 1024) 
            {
                alg_data->fft_phase_start[ii] = alg_data->fft_phase_start[ii] - 1024;		
            }							
            if(alg_data->fft_phase_start[ii] >= 768)
            {
                phase_sin_1q = alg_data->fft_phase_start[ii] - 768 ;
            }
            else if(alg_data->fft_phase_start[ii] >= 512)
            {
                phase_sin_1q = alg_data->fft_phase_start[ii] - 512 ;
            }
            else if(alg_data->fft_phase_start[ii] >= 256)
            {
                phase_sin_1q = alg_data->fft_phase_start[ii] - 256 ;							
            }
            else
            {
                phase_sin_1q = alg_data->fft_phase_start[ii] ;	
            }		
            phase_cos_1q = 255 - phase_sin_1q;              
            sin_c = sin_table[phase_sin_1q];
            cos_c = sin_table[phase_cos_1q];					
            if (alg_data->fft_phase_start[ii] < 256) {
                    //sin_c = sin_c;  ????????????????????
                    //cos_c = cos_c;
            } else if (alg_data->fft_phase_start[ii] < 512) {
                    temp_c = sin_c;
                    sin_c = cos_c;
                    cos_c = -temp_c;
            } else if (alg_data->fft_phase_start[ii] < 768) {
                    sin_c = -sin_c;
                    cos_c = -cos_c;
            } else {
                    temp_c = sin_c;
                    sin_c = -cos_c;
                    cos_c = temp_c;
            } 

            alg_data->hrm_fft_out[ii][0] = alg_data->hrm_fft_out[ii][0] - cos_c*(alg_data->hrm_data_fifo[alg_data->start_ind]>>3);
            alg_data->hrm_fft_out[ii][1] = alg_data->hrm_fft_out[ii][1] - sin_c*(alg_data->hrm_data_fifo[alg_data->start_ind]>>3);
            alg_data->gsen_x_fft_out[ii][0] = alg_data->gsen_x_fft_out[ii][0] - cos_c*alg_data->gsen_data_fifo[alg_data->start_ind];
            alg_data->gsen_x_fft_out[ii][1] = alg_data->gsen_x_fft_out[ii][1] - sin_c*alg_data->gsen_data_fifo[alg_data->start_ind];
        }
    }
    if (alg_data->data_cnt > HRM_CHIP_SETTLE+HRM_FLT_SETTLE)
    {
        alg_data->hrm_data_fifo[alg_data->start_ind] = alg_data->iir_data.out; 
        alg_data->gsen_data_fifo[alg_data->start_ind]	= alg_data->gsen_pedometer.iir_data_gsen_x.out;
        alg_data->start_ind++;		    
        if (alg_data->start_ind > (FFT_WIN_SIZE-1))
        {
            alg_data->start_ind = 0;
        }
    }

    //////////////////dynamic clip generation ///////////////////////////////
	alg_data->senddata_motion_thisdex = alg_data->gsen_pedometer.motion_status;
    hrm_jump_uplim = 52;
    if (alg_data->hr_internal_pre < 70)
    {
        hrm_jump_uplim = 82;
    }
    else if (alg_data->hr_internal_pre < 90)
    {
        hrm_jump_uplim = 72;
    }
    else if (alg_data->hr_internal_pre < 110)
    {
        hrm_jump_uplim = 62;
    }
    else if (alg_data->hr_internal_pre < 140)
    {
        hrm_jump_uplim = 62;
    }
    else
    {
        hrm_jump_uplim = 50;
    }
    if(sports_mode_menu == RIDE_MODE && alg_data->hr_internal_pre<130)
    {
        hrm_jump_dnlim = 30;
    }
    else if(sports_mode_menu == WALK_MODE && alg_data->hr_internal_pre<130)
    {
        hrm_jump_dnlim = 30;
    }
    else
    {
        hrm_jump_dnlim = 40;
        if(alg_data->senddata_motion_thisdex>=4)
        {
            hrm_jump_dnlim = 30;
        }
    }


///////////////计算幅度，判断GSENSOR峰值位置，计算HRS峰值位置，判断是否挖数据////////////////
    if (alg_data->data_cnt % HR_UP_TIME == 0 && alg_data->data_cnt >= FFT_WIN_SIZE + HRM_CHIP_SETTLE + HRM_FLT_SETTLE)
    {
        if (alg_data->hr_internal < 150)
		{
            gsen_block = GSEN_BLOCK1;
		}
        else
		{
            gsen_block = GSEN_BLOCK2;
		}
        // find peaks in Gsen spectrum
        pre1_pow2 = 0xD693A400;    // 3.6e9, a very large one to make 60BPM not a peak
        pre2_pow2 = 0xDC898500;    // 3.7e9, a very large one to make 60BPM not a peak
        max_pow2_x = 0;
        for (ii = 0; ii < 110; ii++)
        {
            this_pow2 = ((alg_data->gsen_x_fft_out[ii][0]) >> 18) * ((alg_data->gsen_x_fft_out[ii][0]) >> 18)  \
                        + ((alg_data->gsen_x_fft_out[ii][1]) >> 18) * ((alg_data->gsen_x_fft_out[ii][1]) >> 18);
						//DEBUG_PRINTF("%d %d %d %d %d\r\n",alg_data->data_cnt, ii, alg_data->gsen_x_fft_out[ii][0],alg_data->gsen_x_fft_out[ii][1],this_pow2);
						//DEBUG_PRINTF("%d %d %d\r\n",alg_data->data_cnt, ii, this_pow2);
            if (this_pow2 > max_pow2_x)
            {
                max_pow2_x = this_pow2;
            }

            if ((this_pow2 < pre1_pow2) && (pre1_pow2 >= pre2_pow2))
            {
                for (jj = 0; jj < GSEN_PEAKS_NUM; jj++)
                {
                    if (pre1_pow2 > gsen_x_max_pow2[jj])
                    {
                        if (jj > 0)
                        {
                            gsen_x_max_index[jj - 1] = gsen_x_max_index[jj];
                            gsen_x_max_pow2[jj - 1] = gsen_x_max_pow2[jj];
														gsen_x_max_value[jj - 1] = gsen_x_max_value[jj];
                        }
                        gsen_x_max_index[jj] = ii;
												gsen_x_max_value[jj] = ((ii + 27)*750)>>9;
                        gsen_x_max_pow2[jj] = pre1_pow2;
                    }
                }
            }
            pre2_pow2 = pre1_pow2;
            pre1_pow2 = this_pow2; 
        }

        //////Gsensor fft结果中找峰值结束////////

        ////////////////// cz_20190314 ////////////////////
        step_fre = gsen_x_max_value[GSEN_PEAKS_NUM - 1];

        if (alg_data->senddata_motion_thisdex >= 4 && step_fre <= 100)
        {
            step_fre = 2 * step_fre;
        }
        else if (alg_data->senddata_motion_thisdex >= 1 && step_fre <= 60)
        {
            step_fre = 2 * step_fre;
        }

        if (alg_data->senddata_motion_thisdex == 0)
        {
            alg_data->gsen_pedometer.hr_up_exp = 110;
            alg_data->gsen_pedometer.hr_down_exp = 60;
        }
        else if (alg_data->senddata_motion_thisdex < 4)
        {

			if (step_fre > 120)
            {
                step_fre = 120;
            }
            else if (step_fre < 60)
            {
                step_fre = 60;
            }
            alg_data->gsen_pedometer.hr_up_exp = 100 + (step_fre - 40) / 4;
            alg_data->gsen_pedometer.hr_down_exp = 50 + (step_fre - 40) / 4;
        }
        else
        {
            if (step_fre < 60)
            {
                step_fre = 60;
            }
            if (step_fre < 100)
            {
                alg_data->gsen_pedometer.hr_up_exp = 130 + (step_fre * 2 - 40) / 4;
                alg_data->gsen_pedometer.hr_down_exp = 80 + (step_fre * 2 - 40) / 4;
            }
            else
            {
                alg_data->gsen_pedometer.hr_up_exp = 130 + (step_fre - 40) / 4;
                alg_data->gsen_pedometer.hr_down_exp = 80 + (step_fre - 40) / 4;
            }
        }
        //////////////////////////////////////////////////////////////////////////////////////////////////////

        if (gsen_x_max_value[GSEN_PEAKS_NUM - 1] >= 140)
        {
            if (abs(gsen_x_max_value[GSEN_PEAKS_NUM - 1] / 2 - gsen_x_max_value[GSEN_PEAKS_NUM - 2]) <= 5)
            {
                gx_run_state = 1;
                alg_data->run_peak = gsen_x_max_value[GSEN_PEAKS_NUM - 1] / 2;
            }
        }
        else if (gsen_x_max_value[GSEN_PEAKS_NUM - 1] >= 70)
        {
            if (abs(gsen_x_max_value[GSEN_PEAKS_NUM - 2] / 2 - gsen_x_max_value[GSEN_PEAKS_NUM - 1]) <= 5)
            {
                gx_run_state = 1;
                alg_data->run_peak = gsen_x_max_value[GSEN_PEAKS_NUM - 1];
            }
        }
        run_state = gx_run_state && alg_data->senddata_motion_thisdex >= 4;
				
        if (run_state == 1)
        {
            alg_data->run_state_cnt = alg_data->run_state_cnt + 1;
            if (alg_data->run_state_cnt > 20)
            {
                alg_data->run_state_cnt = 20;
            }
        }
        else
        {
            if (alg_data->run_state_cnt > 0)
            {
                alg_data->run_state_cnt = alg_data->run_state_cnt - 1;
            }
        }
        if(alg_data->run_state_cnt<10)
        {
            alg_data->run_peak = 0;
        }

        // walk state judgement
        if (abs(gsen_x_max_value[GSEN_PEAKS_NUM - 1] - alg_data->gsenx_peak_pre) <= 3 && gsen_x_max_pow2[GSEN_PEAKS_NUM - 2] != 0)
        {
            if (((gsen_x_max_pow2[GSEN_PEAKS_NUM - 1] >> 2) > gsen_x_max_pow2[GSEN_PEAKS_NUM - 2]) \
               || (abs(gsen_x_max_value[GSEN_PEAKS_NUM - 1] / 2 - gsen_x_max_value[GSEN_PEAKS_NUM - 2]) <= 2) \
               || (abs(gsen_x_max_value[GSEN_PEAKS_NUM - 2] / 2 - gsen_x_max_value[GSEN_PEAKS_NUM - 1]) <= 2))
            {
                gx_walk_state = 1;
							
				if (gsen_x_max_value[GSEN_PEAKS_NUM - 1] >= 90)
                {
                    alg_data->walk_peak = gsen_x_max_value[GSEN_PEAKS_NUM - 1] / 2;
                }
                else
                {
                    alg_data->walk_peak = gsen_x_max_value[GSEN_PEAKS_NUM - 1];
                }
            }
        }
        alg_data->gsenx_peak_pre = gsen_x_max_value[GSEN_PEAKS_NUM - 1];

        walk_state = (!run_state) && gx_walk_state && alg_data->senddata_motion_thisdex > 1 && alg_data->senddata_motion_thisdex < 4;

        if (walk_state == 1)
        {
            alg_data->walk_state_cnt = alg_data->walk_state_cnt + 1;
            if (alg_data->walk_state_cnt > 20)
            {
                alg_data->walk_state_cnt = 20;
            }
        }
        else
        {
            if (alg_data->walk_state_cnt > 0)
            {
                alg_data->walk_state_cnt = alg_data->walk_state_cnt - 1;
            }
        }

        if (alg_data->walk_state_cnt < 10)
        {
            alg_data->walk_peak = 0;
        }
        
        center_hr = alg_data->hr_internal;
        //// find primary peaks in HRM ////////////////////////////////////////////////////////////
        pre1_pow2 = 0xD693A400;    // 3600000000 a very large one to make 45BPM not a peak
        pre2_pow2 = 0xDC898500;    // 3700000000 a very large one to make 45BPM not a peak
        for (ii = 0; ii < 110; ii++)
        {
            this_pow2 = ((alg_data->hrm_fft_out[ii][0]) >> 18) * ((alg_data->hrm_fft_out[ii][0]) >> 18)  \
                        + ((alg_data->hrm_fft_out[ii][1]) >> 18) * ((alg_data->hrm_fft_out[ii][1]) >> 18);
						//DEBUG_PRINTF("%d %d %d %d %d\r\n",alg_data->data_cnt, ii, alg_data->hrm_fft_out[ii][0],alg_data->hrm_fft_out[ii][1],this_pow2);

            peaks_within_range = (((((ii+27) * 750 >> 9) - center_hr) < hrm_jump_uplim) && (center_hr - ((ii+27) * 750 >> 9) < hrm_jump_dnlim)) \
								|| (alg_data->new_start && (ii < 80) && (ii > 10)) || alg_data->senddata_motion_thisdex == 0;

            if (alg_data->senddata_motion_thisdex == 0)
            {
                peaks_within_range = peaks_within_range && ((ii+27) * 750 >> 9) > 40;
            }
            else
            {
                peaks_within_range = peaks_within_range && ((ii+27) * 750 >> 9) > 50;
            }
//            if (alg_data->run_state_cnt >= 10 &&  run_state == 1 && alg_data->hr_internal_pre > alg_data->run_peak)
//            {
//                peaks_within_range = peaks_within_range && ((44 + (ii * 750 >> 9)) + 5 >  alg_data->run_peak) ;
//            }

            if ((this_pow2 < pre1_pow2) && (pre1_pow2 >= pre2_pow2) && peaks_within_range)
            {
                for (jj = 0; jj < HR_PEAKS_NUM; jj++)
                {
                    if (pre1_pow2 > hrm_max_pow2[jj])
                    {
                        if (jj > 0)
                        {
                            hrm_max_index[jj - 1] = hrm_max_index[jj];
                            hrm_max_pow2[jj - 1] = hrm_max_pow2[jj];
							hrm_max_value[jj - 1] = hrm_max_value[jj];
                        }
                        hrm_max_index[jj] = ii;
                        hrm_max_pow2[jj] = pre1_pow2;
						hrm_max_value[jj] = ((ii + 27)*750)>>9;
                    }
                }
            }
            pre2_pow2 = pre1_pow2;
            pre1_pow2 = this_pow2;
            if (this_pow2 > max_pow2)
            {
                max_pow2 = this_pow2;
            }
        }
//				for(ii=0;ii<HR_PEAKS_NUM;ii++)
//				{
//				DEBUG_PRINTF("%d %d %d %d %d\r\n",alg_data->data_cnt, ii, hrm_max_index[ii],hrm_max_pow2[ii],hrm_max_value[ii]);
//				}

        if(alg_data->senddata_motion_thisdex==0 && (alg_data->data_cnt <= 400 || GSEN_FUN_LV>0))
        {
            hrm_snr = tyhx_hrm_snr(hrm_max_index, hrm_max_pow2);
        }

        // Find track peaks and calculate track peaks count ////////////////////

        hr_track_pow_thr = 10 * FFT_POW_GAIN;
        if ((run_state == 1) && (alg_data->run_state_cnt >= 10))
        {
            hr_track_pow_thr = 500 * FFT_POW_GAIN;
        }

        for (ii = 0; ii < HR_PEAKS_NUM; ii++)
        {
            track_peak_flag = 0;
            for (jj = HR_PEAKS_NUM; jj > 0; jj--)
            {
                if ((hrm_max_index[ii] > 0) && (track_peak_flag == 0) \
                    && (abs(hrm_max_value[ii] - alg_data->hr_track_peaks[jj - 1]) <= 5) \
                    && (hrm_max_pow2[ii] >= hr_track_pow_thr))
                {
                    hr_track_peaks_cnt[ii] = alg_data->hr_track_peaks_cnt_pre[jj - 1] + 1;
                    track_peak_flag = 1;
                }
            } // for jj
        } // for ii

        for (ii = 0; ii < HR_PEAKS_NUM; ii++)
        {
            if (hr_track_peaks_cnt[ii] > 30)
            {
                hr_track_peaks_cnt[ii] = 30;
            }
            alg_data->hr_track_peaks[ii] = hrm_max_value[ii];  // hr_peaks_pre = hr_peaks
            alg_data->hr_track_peaks_cnt_pre[ii] = hr_track_peaks_cnt[ii];
        }


        ///////////////////// find high quanlity hr /////////////////////////////////
        // --- find high quanlity hr peak, not walk and run state
        if ((abs(hrm_max_value[HR_PEAKS_NUM - 1] - alg_data->hr_max_peak_pre) <= 3) \
           && ((hrm_max_pow2[HR_PEAKS_NUM - 1] / 4 >= hrm_max_pow2[HR_PEAKS_NUM - 2]) \
           || (abs(hrm_max_value[HR_PEAKS_NUM - 1] * 2 - hrm_max_value[HR_PEAKS_NUM - 2]) <= 3)))
        {
            if ((abs(hrm_max_value[HR_PEAKS_NUM - 1] - gsen_x_max_value[GSEN_PEAKS_NUM - 1]) >= 5) \
                && (abs(hrm_max_value[HR_PEAKS_NUM - 1] - gsen_x_max_value[GSEN_PEAKS_NUM - 1] * 2) >= 5)
				&& (abs(hrm_max_value[HR_PEAKS_NUM - 1] - gsen_x_max_value[GSEN_PEAKS_NUM - 1] / 2) >= 5))
            {
                alg_data->hq_hr = hrm_max_value[HR_PEAKS_NUM - 1];
                alg_data->hq_hr_cnt = alg_data->hq_hr_cnt + 2;
            }
            else
            {
                if (alg_data->hq_hr_cnt > 0)
                {
                    alg_data->hq_hr_cnt = alg_data->hq_hr_cnt - 1;
                }
            }
        }
        else
        {
            if (alg_data->hq_hr_cnt > 0)
            {
                alg_data->hq_hr_cnt = alg_data->hq_hr_cnt - 1;
            }
        }
        alg_data->hr_max_peak_pre = hrm_max_value[HR_PEAKS_NUM - 1];
        if (alg_data->hq_hr_cnt >= 20)
        {
            alg_data->hq_hr_cnt = 20;
        }

        if (alg_data->run_state_cnt >= 5 || alg_data->walk_state_cnt >= 5)
        {
            alg_data->hq_hr_cnt = 0;
        }

        // --- find high quanlity hr peak in walk state
        if (alg_data->walk_state_cnt >= 10)
        {
            if (alg_data->hr_internal_pre > alg_data->walk_peak && alg_data->hr_internal_pre < (uint16_t)alg_data->walk_peak * 2 + 10)
            {
                for (ii = 0; ii < HR_PEAKS_NUM; ii++)
                {
                    if (hrm_max_value[ii] > (alg_data->walk_peak * 10)>>3 \
                        && (hrm_max_value[ii] < alg_data->walk_peak * 2 - 10) \
                        && hr_track_peaks_cnt[ii] >= 5 \
                        && abs(abs(hrm_max_value[ii] - alg_data->walk_peak * 2) - 11) > 2 \
                        && hrm_max_pow2[ii] > (max_pow2 >> 2) \
                        && hrm_max_pow2[ii] > 6000)
                    {
                        alg_data->hq_hr_walk = hrm_max_value[ii];
                        hq_hr_walk_vld = 1;
                    }
                } // for
            }
            if (hq_hr_walk_vld)
            {
                alg_data->hq_hr_walk_cnt = 5;
            }
            else if (alg_data->hq_hr_walk_cnt > 0)
            {
                alg_data->hq_hr_walk_cnt = alg_data->hq_hr_walk_cnt - 1;
            }
        }
        else
        {
            alg_data->hq_hr_walk = 0;
            alg_data->hq_hr_walk_cnt = 0;
        }

        // Fail Frame judgement ///////////////////////////////////////////////
        // fail_frame1
        if (max_pow2 <= 50 * FFT_POW_GAIN)
        {
            fail_frame1 = 1;
        }

        // fail_frame2
		fail_frame2 = 0;

        // fail_frame3
        if (alg_data->data_cnt > 628)    // hr_index > 20 ( matlab)
        {
            for (ii = 0; ii < HR_PEAKS_NUM; ii++)
            {
                if (hr_peaks_cnt_max < hr_track_peaks_cnt[ii])
                {
                    hr_peaks_cnt_max = hr_track_peaks_cnt[ii];
                }
            }

            if (hr_peaks_cnt_max <= 12)
            {
                //fail_frame3 = 1;
            }
        }

        if (alg_data->hr_hold > 10)
        {
            fail_frame1 = 0;
            fail_frame2 = 0;
           // fail_frame3 = 0;
        }

        if (alg_data->hr_hold > 20)
        {
            alg_data->hr_hold = 0;
        }

        alg_data->hr_internal_pre = alg_data->hr_internal;

        if (alg_data->senddata_motion_thisdex >= 4 && alg_data->run_state_cnt > 5)
        {
            if (alg_data->runrun_cnt < 60)
            {
                alg_data->runrun_cnt++;
            }
        }
        else
        {
            if (alg_data->runrun_cnt > 1)
            {
                alg_data->runrun_cnt = alg_data->runrun_cnt - 2;
            }
        }
        
        if(alg_data->data_cnt == 375)
        {
            debug_flag = 1;
        }

        if ((alg_data->new_start == 1) && (alg_data->senddata_motion_thisdex == 0))
        {
            hr_new = hrm_max_value[HR_PEAKS_NUM - 1];

            if (fail_frame1 == 1)
            {
                for (ii = 0; ii < HR_PEAKS_NUM; ii++)
                {
                    if (hrm_max_value[ii] <= 100)
                    {
                        hr_new = hrm_max_value[ii];
                    }
                }
            }
        }
//        else if ((fail_frame3) && (alg_data->new_start == 0) && (alg_data->run_state_cnt < 18) && (alg_data->run_state_cnt >= 2))
//        {
//            hr_new = alg_data->hr_internal_pre;
//            alg_data->hr_hold = alg_data->hr_hold + 1;
//        }
        else if (alg_data->senddata_motion_thisdex == 0)
        {
            if (alg_data->data_cnt > 640 && (hrm_max_value[HR_PEAKS_NUM - 1] < 60 || hrm_max_value[HR_PEAKS_NUM - 1] > 100))
            {
                if (hrm_max_pow2[HR_PEAKS_NUM - 1] > 4000*FFT_POW_GAIN && hr_track_peaks_cnt[HR_PEAKS_NUM - 1] > 5 && hrm_max_pow2[HR_PEAKS_NUM - 1] > hrm_max_pow2[HR_PEAKS_NUM - 2]*2)
                {
                    hr_new = hrm_max_value[HR_PEAKS_NUM - 1];
                }
                else
                {
                    hr_new = alg_data->hr_internal_pre;
                }
                if (hr_track_peaks_cnt[HR_PEAKS_NUM - 1] > 20 && hrm_max_pow2[HR_PEAKS_NUM - 1] > hrm_max_pow2[HR_PEAKS_NUM - 2]*2)
                {
                    hr_new = hrm_max_value[HR_PEAKS_NUM - 1];
                }
            }
            else
            {
                if (hrm_max_index[HR_PEAKS_NUM - 1] == 0)
                {
                    hr_new = 80;
                }
                else
                {
                    hr_new = hrm_max_value[HR_PEAKS_NUM - 1];
                }
            }
            if(abs(hrm_max_value[HR_PEAKS_NUM - 1]-hrm_max_value[HR_PEAKS_NUM - 2]*2)<=3 \
               && hrm_max_pow2[HR_PEAKS_NUM - 1]<hrm_max_pow2[HR_PEAKS_NUM - 2]*2 && hrm_max_value[HR_PEAKS_NUM - 2]<55)
            {
                hr_new = hrm_max_value[HR_PEAKS_NUM - 2];
            }
            
            if(sports_mode_menu == RIDE_MODE)
            {
                if(hr_new<90 && hr_new<alg_data->hr_internal_pre-10 && alg_data->data_cnt>800)
                {
                    for(ii=0;ii<HR_PEAKS_NUM;ii++)
                    {
                        if( abs(hrm_max_value[ii]-hrm_max_value[HR_PEAKS_NUM - 1]*2)>3 && hrm_max_value[ii] > hrm_max_value[HR_PEAKS_NUM - 1] \
                            && hrm_max_value[ii]>90 && hrm_max_value[ii]<140 && hrm_max_pow2[ii]*6 > hrm_max_pow2[HR_PEAKS_NUM - 1] )
                        {
                            hr_new = hrm_max_value[ii];
                        }
                    }
                }
            }
            alg_data->hr_hold = 0;
        }
        else
        {
            found = 0;
            for (ii = 0; ii < HR_PEAKS_NUM; ii++)
            {
                block = 0;
                if (hrm_max_index[ii] > 0)
                {
//                    if (alg_data->senddata_motion_thisdex >= 4)
//                    {
//                        gsen_block_start = GSEN_PEAKS_NUM - 3;
//                    }
//                    else
//                    {
//                        gsen_block_start = 0;
//                    }
										gsen_block_start = 0;
                    for (jj = gsen_block_start; jj < GSEN_PEAKS_NUM; jj++)
                    {
                        gsen_block_gen = gsen_block;
                        if(sports_mode_menu == RIDE_MODE)
                        {
                            if(alg_data->walk_state_cnt<10 && alg_data->run_state_cnt<10 && alg_data->data_cnt>2000)
                            {
                                if(hrm_max_value[ii] < 80 )
                                {
                                    block = 1;
                                }
                            }
                            if ((gsen_x_max_index[jj] > 0 && gsen_x_max_value[jj] < 90) || gsen_x_max_value[jj] > 135)
                            {
                                if ((abs(hrm_max_value[ii] - gsen_x_max_value[jj]) <= gsen_block_gen) \
                                    && !(hrm_max_pow2[ii] > (max_pow2 >> 3) && gsen_x_max_pow2[jj] < (max_pow2_x >> 4)))
                                {
                                    block = 1;
                                }
                            }
                        }
                        else
                        {
                            if (gsen_x_max_index[jj] > 0)
                            {
                                if (abs(hrm_max_value[ii] - gsen_x_max_value[jj]) <= gsen_block_gen)
                                {
                                    block = 1;
                                }
                                if(jj==GSEN_PEAKS_NUM-1 || abs(gsen_x_max_value[jj]*2-gsen_x_max_value[GSEN_PEAKS_NUM-1])<3 || abs(gsen_x_max_value[jj]-gsen_x_max_value[GSEN_PEAKS_NUM-1]*2)<3)
                                {
                                }
                                else
                                {
                                    if(hrm_max_pow2[ii] > (max_pow2 >> 3) && gsen_x_max_pow2[jj] < (max_pow2_x >> 4))
                                    {
                                        block = 0;
                                    }
                                }
                            }
                        }
                    }
                    if(gsen_x_max_value[GSEN_PEAKS_NUM-1]>140 && alg_data->senddata_motion_thisdex >= 4)
                    {
                        if(abs(hrm_max_value[ii] - gsen_x_max_value[GSEN_PEAKS_NUM-1]/2)<=2)
                        {
                                block = 1;
                        }
                    }
                    else if(gsen_x_max_value[GSEN_PEAKS_NUM-1]<90 && alg_data->senddata_motion_thisdex >= 4)
                    {
                        if(abs(hrm_max_value[ii] - gsen_x_max_value[GSEN_PEAKS_NUM-1]*2)<=2)
                        {
                                block = 1;
                        }
                    }

                    if (max_pow2 <= 100 * FFT_POW_GAIN)   // weak signal
                    {
                        if (abs(hrm_max_value[ii] - alg_data->hr_internal_pre) >= 10)
                        {
                            block = 1;
                        }
                    }

                    if (fail_frame2 == 1)
                    {
                        if (hrm_max_value[ii] <= 100 || hr_track_peaks_cnt[ii] <= 10)
                        {
                            block = 1;
                        }
                    }

                    if ((alg_data->hq_hr_cnt > 4) && (abs(hrm_max_value[ii] - alg_data->hq_hr) >= 10))
                    {
                        block = 1;
                    }

                    // --- walking
                    if (alg_data->walk_state_cnt >= 10)
                    {
                        if (alg_data->hq_hr_walk_cnt == 0 && alg_data->hr_internal_pre <= (uint16_t)alg_data->walk_peak * 2 \
                           && (hrm_max_value[ii] < (alg_data->walk_peak * 10)>>3 || hrm_max_value[ii] > alg_data->walk_peak * 2 || hrm_max_value[ii] > 140))
                        {
                            block = 1;
                        }
//                        if (alg_data->hq_hr_walk_cnt == 0 && hr_valid_ind == 0 \
//                                && alg_data->hr_internal_pre < (uint16_t)alg_data->walk_peak * 2 + 10 \
//                                && (((hrm_max_index[ii] + 1) * 750 >> 9) + 44 >= (uint16_t)alg_data->walk_peak * 3 / 2) \
//                                && (((hrm_max_index[ii] + 1) * 750 >> 9) + 44 < (uint16_t)alg_data->walk_peak * 2 + 10) \
//                                && hr_track_peaks_cnt[ii] >= 10)    // hr in gsen
//                        {
//                            block = 0;
//                        }
                    }
                    // --- running
                    if (alg_data->run_state_cnt >= 10)
                    {

                        if ((abs(abs(hrm_max_value[ii] - alg_data->run_peak) - 11) <= 2) \
                            && hrm_max_pow2[ii] < (hrm_max_pow2[HR_PEAKS_NUM - 1] >> 3) \
                            && hr_track_peaks_cnt[ii] > 10)
                        {
                            block = 1;
                        }
                        if ((abs(abs(hrm_max_value[ii] - 2 * alg_data->run_peak) - 11) <= 2) \
                            && hrm_max_pow2[ii] < (hrm_max_pow2[HR_PEAKS_NUM - 1] >> 3) \
                            && hr_track_peaks_cnt[ii] > 10)
                        {
                            block = 1;
                        }

                        if (alg_data->hr_internal_pre >= 140)
                        {
                            if (hrm_max_value[ii] <= 130 && hr_track_peaks_cnt[ii] < 10)
                            {
                                block = 1;
                            }

                            if (ii == HR_PEAKS_NUM - 1 && abs(hrm_max_value[ii] - 2 * alg_data->run_peak) <= 1 && hr_valid_ind < 2 && hr_track_peaks_cnt[ii] >= 10)
                            {
                                block = 0;
                            }
                        }
                    }

                    if (ii == HR_PEAKS_NUM - 1 && block == 1)
                    {
                        if( (hr_valid_ind<4 || hrm_max_value[hr_valid_ind]<120) && hrm_max_pow2[hr_valid_ind]*4 < hrm_max_pow2[HR_PEAKS_NUM - 1] \
                            && abs(hrm_max_value[ii] - alg_data->hr_internal_pre) <= 10 && hrm_max_value[hr_valid_ind] < alg_data->hr_internal_pre - 10)
                        {
                            block = 0;
                        }
                        if(hrm_max_pow2[hr_valid_ind]*8 < hrm_max_pow2[HR_PEAKS_NUM - 1] && alg_data->senddata_motion_thisdex >= 4 \
                            && abs(hrm_max_value[ii] - alg_data->hr_internal_pre) <= 10 && hrm_max_value[ii] > 150 && hrm_max_value[ii] < 190)
                        {
                            block = 0;
                        }
                        if(abs(hrm_max_value[ii] - alg_data->hr_internal_pre) <= 20 && hrm_max_value[hr_valid_ind] < alg_data->hr_internal_pre - 5 \
                            && hrm_max_value[ii] > 140 && hrm_max_value[hr_valid_ind] < 120 && alg_data->senddata_motion_thisdex >= 4)
                        {
                            block = 0;
                        }
                        if(abs(hrm_max_value[ii] - alg_data->hr_internal_pre) <= 20 && hrm_max_value[ii] > 90 \
                            && alg_data->senddata_motion_thisdex > 0 && alg_data->senddata_motion_thisdex < 4)
                        {
                            if(hr_valid_ind <= 2 || hrm_max_value[hr_valid_ind] < 70)
                            {
                                    block = 0;
                            }
                            else if(sports_mode_menu==WALK_MODE && hrm_max_value[ii] > 120 && abs(hrm_max_value[hr_valid_ind] - alg_data->hr_internal_pre) > 20)
                            {
                                    block = 0;
                            }
                        }
                        if( abs(hrm_max_value[ii] - alg_data->hr_internal_pre) <= 20 && hrm_max_value[hr_valid_ind] < alg_data->hr_internal_pre - 20 \
                            && alg_data->senddata_motion_thisdex >= 4 && alg_data->run_state_cnt >= 10)
                        {
                            block = 0;
                        }
                        if(abs(hrm_max_value[ii] - alg_data->hr_internal_pre) <= 10 && abs(hrm_max_value[hr_valid_ind] - alg_data->hr_internal_pre) > 20)
                        {
                            if(alg_data->senddata_motion_thisdex>=4 && hrm_max_value[ii]>140)
                            {
                                block = 0;
                            }
                            else if(alg_data->senddata_motion_thisdex<4 && hrm_max_value[ii]>90)
                            {
                                block = 0;
                            }
                        }
                        if(alg_data->hr_internal_pre>90 && hrm_max_value[ii] > 80 && hrm_max_value[hr_valid_ind] < 80)
                        {
                            block = 0;
                        }											
                    }

                    if (alg_data->run_state_cnt >= 10 && hrm_max_value[ii] >= 180 && ii < HR_PEAKS_NUM - 2 && hr_track_peaks_cnt[ii] <= 10)
                    {
                        block = 1;
                    }

                    if (hrm_max_value[ii] > 180 && abs(hrm_max_value[ii] - alg_data->hr_internal_pre) > 10 && hr_track_peaks_cnt[ii] <= 10)
                    {
                        block = 1;
                    }

                    if (alg_data->runrun_cnt >= 30 && alg_data->run_state_cnt >= 10 && alg_data->senddata_motion_thisdex >= 4)
                    {
                        if (hr_valid_ind >= 1 && hrm_max_value[hr_valid_ind] >= alg_data->hr_internal_pre - 20 \
                            && hrm_max_value[ii] < alg_data->hr_internal_pre - 20 && alg_data->hr_internal_pre > 120)
                        {
                            block = 1;
                        }
                    }

                    if (abs(abs(hrm_max_value[ii] - gsen_x_max_value[GSEN_PEAKS_NUM - 1]) - 5) <= 3 \
						&& hrm_max_pow2[ii]*4 < hrm_max_pow2[HR_PEAKS_NUM - 1] && abs(hrm_max_value[ii] - alg_data->hr_internal_pre) > 10)
                    {
                        block = 1;
                    }
										
                    if (alg_data->senddata_motion_thisdex > 0 && alg_data->senddata_motion_thisdex < 4 \
                        && hrm_max_value[ii] > 110 && hr_track_peaks_cnt[ii] <= 6 && ii < 3)
                    {
                        block = 1;
                    }

                    if (alg_data->runrun_cnt > 20 && alg_data->senddata_motion_thisdex >= 4)
                    {
                        if (hrm_max_value[ii] < 50 + alg_data->runrun_cnt)
                        {
                            block = 1;
                        }
                    }
                    else if(alg_data->runrun_cnt > 5 && alg_data->senddata_motion_thisdex >= 4)
                    {
                        if (alg_data->hr_internal_pre<110 && hrm_max_value[ii] < alg_data->hr_internal_pre-10)
                        {
                            block = 1;
                        }
					}
                    
                    if(alg_data->senddata_motion_thisdex >0 && alg_data->senddata_motion_thisdex < 4)
                    {   
                        if(alg_data->hr_internal_pre>160)
                        {
                            if(hrm_max_value[ii]>alg_data->hr_internal_pre)
                            {
                                block = 1;
                            }
                        }
                        else if(alg_data->hr_internal_pre>140)
                        {
                            if(hrm_max_value[ii]>alg_data->hr_internal_pre+10)
                            {
                                block = 1;
                            }
                        }
                        else
                        {
                            if(hrm_max_value[ii]>150)
                            { 
                                block = 1;
                            }
                        }
                    }
										
                    if(sports_mode_menu == RIDE_MODE)
                    {
                        if(alg_data->senddata_motion_thisdex > 0 && hrm_max_value[hr_valid_ind] > alg_data->hr_internal_pre-15 \
                          && hrm_max_pow2[hr_valid_ind]*2 > hrm_max_pow2[HR_PEAKS_NUM - 1] && hrm_max_value[ii] <= alg_data->hr_internal_pre-15)
                        {
                            block = 1;
                        }
                    }
                    else
                    {
                        if(alg_data->senddata_motion_thisdex > 0 && alg_data->senddata_motion_thisdex < 4 && gsen_x_max_value[GSEN_PEAKS_NUM - 1] > 100 \
                          && abs(hrm_max_value[ii] * 2 - gsen_x_max_value[GSEN_PEAKS_NUM - 1]) < 3)
                        {                     
                          block = 1;
                        }
                    }
										
                    if(sports_mode_menu == HIGH_POW_MODE && alg_data->senddata_motion_thisdex > 0)
                    {
                        if(alg_data->senddata_motion_thisdex > 3)
                        {
                            if(alg_data->hr_internal_pre < 130 && hrm_max_value[ii] < alg_data->hr_internal_pre-10)
                            {
                                block = 1;
                            }
                        }
                        else
                        {
                            if(alg_data->hr_internal_pre < 110 && hrm_max_value[ii] < alg_data->hr_internal_pre-10)
                            {
                                block = 1;
                            }
                        }
                    }
										
                    if ((block == 0) && (found == 0))
                    {
                        found = 1;
                        hr_valid_ind = ii;
                    }
                    else if (block == 0)
                    {
                        hr_valid_ind = ii;
                    }
                }
            }

            if (found)
            {
                if (alg_data->signal_block_cnt > 0 && alg_data->data_cnt > 320)
                {
                    hr_new = alg_data->hr_internal_pre;
                }
                else
                {
                    hr_new = hrm_max_value[hr_valid_ind];
                }
                alg_data->hr_hold = 0;
                alg_data->hr_block = 0;
                alg_data->no_found_cnt = 0;
            }
            else
            {
                if (alg_data->senddata_motion_thisdex >= 4 && alg_data->run_state_cnt >= 10 && alg_data->hr_internal_pre < 140)
                {
                    hr_new = alg_data->hr_internal_pre + 1;
                }
                else
                {
                    hr_new = alg_data->hr_internal_pre;
                }
                alg_data->no_found_cnt = alg_data->no_found_cnt + 1;
                alg_data->hr_hold = alg_data->hr_hold + 1;
            }

            alg_data->ppg_signal_quality.sq_pow = 0;
            alg_data->ppg_signal_quality.sq_con = 0;
            alg_data->ppg_signal_quality.sq_score = 0;
            if (found)
            {
                if(hr_valid_ind<HR_PEAKS_NUM-1)
                {
                    if (hrm_max_pow2[hr_valid_ind]*2 > hrm_max_pow2[HR_PEAKS_NUM - 1])
                    {
                        alg_data->ppg_signal_quality.sq_pow = 2;
                    }
                    else if ((hrm_max_pow2[hr_valid_ind]<<3) > hrm_max_pow2[HR_PEAKS_NUM - 1])
                    {
                        alg_data->ppg_signal_quality.sq_pow = 1;
                    }
                    else
                    {
                        alg_data->ppg_signal_quality.sq_pow = 0;
                    }
                }
                else
                {
                    alg_data->ppg_signal_quality.sq_pow = 1;
                }

                for (ii = 3; ii > 0; ii--)
                {
                    alg_data->ppg_signal_quality.hr_peak_fifo[ii] = alg_data->ppg_signal_quality.hr_peak_fifo[ii - 1];
                }
                alg_data->ppg_signal_quality.hr_peak_fifo[0] = hrm_max_value[hr_valid_ind];
                if (abs(alg_data->ppg_signal_quality.hr_peak_fifo[0] - alg_data->ppg_signal_quality.hr_peak_fifo[1]) <= 3)
                {
                    alg_data->ppg_signal_quality.sq_con = 1;
                    alg_data->ppg_signal_quality.con_cnt++;
                    if (alg_data->ppg_signal_quality.con_cnt >= 10)
                    {
                        alg_data->ppg_signal_quality.con_cnt = 10;
                    }
                    if (alg_data->ppg_signal_quality.con_cnt >= 3)
                    {
                        alg_data->ppg_signal_quality.nocon_cnt = 0;
                    }
                }
                else if (abs(alg_data->ppg_signal_quality.hr_peak_fifo[0] - alg_data->ppg_signal_quality.hr_peak_fifo[2]) <= 5 \
						&& abs(alg_data->ppg_signal_quality.hr_peak_fifo[0] - alg_data->ppg_signal_quality.hr_peak_fifo[3]) <= 8)
                {
                    alg_data->ppg_signal_quality.sq_con = 1;
                    alg_data->ppg_signal_quality.con_cnt++;
                    if (alg_data->ppg_signal_quality.con_cnt >= 10)
                    {
                        alg_data->ppg_signal_quality.con_cnt = 10;
                    }
                    if (alg_data->ppg_signal_quality.con_cnt >= 3)
                    {
                        alg_data->ppg_signal_quality.nocon_cnt = 0;
                    }
                }
                else
                {
                    alg_data->ppg_signal_quality.sq_con = 0;
                    alg_data->ppg_signal_quality.nocon_cnt++;
                    if (alg_data->ppg_signal_quality.nocon_cnt >= 3)
                    {
                        alg_data->ppg_signal_quality.nocon_cnt = 0;
                        alg_data->ppg_signal_quality.con_cnt = 0;
                    }
                }

                alg_data->ppg_signal_quality.npeak_cnt = 0;
                if(hr_valid_ind<HR_PEAKS_NUM-1)
                {
                    for (ii = 0; ii < hr_valid_ind; ii++)
                    {
                        if (hrm_max_pow2[ii]*2 > hrm_max_pow2[hr_valid_ind])
                        {
                            alg_data->ppg_signal_quality.npeak_cnt++;
                        }
                    }
                }
                else
                {
                    alg_data->ppg_signal_quality.npeak_cnt = 3;
                }	
                if (alg_data->ppg_signal_quality.hr_peak_fifo[0] < 170)
                {
                    alg_data->ppg_signal_quality.sq_score = alg_data->ppg_signal_quality.sq_con* \
                    (alg_data->ppg_signal_quality.sq_pow * 10 + alg_data->ppg_signal_quality.con_cnt * 2)* \
                    (6 - alg_data->ppg_signal_quality.npeak_cnt) / 6;
                }
                else
                {
                    alg_data->ppg_signal_quality.sq_score = alg_data->ppg_signal_quality.sq_con* \
                    (alg_data->ppg_signal_quality.sq_pow * 10 + alg_data->ppg_signal_quality.con_cnt * 2 - 10)* \
                    (6 - alg_data->ppg_signal_quality.npeak_cnt) / 6;
                }
            }
            else
            {
                for (ii = 3; ii > 0; ii--)
                {
                    alg_data->ppg_signal_quality.hr_peak_fifo[ii] = alg_data->ppg_signal_quality.hr_peak_fifo[ii - 1];
                }
                alg_data->ppg_signal_quality.hr_peak_fifo[0] = 0;
                alg_data->ppg_signal_quality.sq_pow = 0;
                alg_data->ppg_signal_quality.sq_con = 0;
                alg_data->ppg_signal_quality.sq_score = 0;
                alg_data->ppg_signal_quality.nocon_cnt++;
                if (alg_data->ppg_signal_quality.nocon_cnt >= 3)
                {
                    alg_data->ppg_signal_quality.nocon_cnt = 0;
                    alg_data->ppg_signal_quality.con_cnt = 0;
                }
            }
        }

        if (alg_data->no_found_cnt > 100)
        {
            alg_data->no_found_cnt = 100;
        }

        alg_data->hr_internal = hr_new;

        if (alg_data->gsen_pedometer.status_this != alg_data->gsen_pedometer.motion_status)
        {
            alg_data->gsen_pedometer.status_last = alg_data->gsen_pedometer.status_this;
            alg_data->gsen_pedometer.status_this = alg_data->gsen_pedometer.motion_status;
            alg_data->gsen_pedometer.hr_up_cnt = 0;
            alg_data->gsen_pedometer.hr_down_cnt = 0;
        }
        if (alg_data->gsen_pedometer.status_this > alg_data->gsen_pedometer.status_last)
        {
            alg_data->gsen_pedometer.hr_up_cnt++;
            if (alg_data->gsen_pedometer.hr_up_cnt >= 32)
            {
                alg_data->gsen_pedometer.hr_up_cnt = 32;
            }
            alg_data->gsen_pedometer.hr_down_cnt = 0;
        }
        if (alg_data->gsen_pedometer.status_this < alg_data->gsen_pedometer.status_last)
        {
            alg_data->gsen_pedometer.hr_up_cnt = 0;
            alg_data->gsen_pedometer.hr_down_cnt++;
            if (alg_data->gsen_pedometer.hr_down_cnt >= 32)
            {
                alg_data->gsen_pedometer.hr_down_cnt = 32;
            }
        }

        if (alg_data->senddata_motion_thisdex == 0)
        {
            if (alg_data->hr_internal_pre < 70)
            {
                hr_clip_inside_up = 2;
                hr_clip_inside_dn = 1;
            }
            else if (alg_data->hr_internal_pre > 100)
            {
                hr_clip_inside_up = 1;
                hr_clip_inside_dn = 2;
            }
            else if (alg_data->hr_internal_pre > 120)
            {
                hr_clip_inside_up = 1;
                hr_clip_inside_dn = 3;
            }
            else
            {
                hr_clip_inside_up = 2;
                hr_clip_inside_dn = 2;
            }
        }
        else if (alg_data->senddata_motion_thisdex < 4)
        {
            if (alg_data->hr_internal_pre > (alg_data->gsen_pedometer.hr_up_exp + alg_data->gsen_pedometer.hr_down_exp) / 2)
            {
                hr_clip_inside_up = 1;
                hr_clip_inside_dn = 1;
            }
            else
            {
                hr_clip_inside_up = 2;
                hr_clip_inside_dn = 1;
            }
            if (alg_data->ppg_signal_quality.sq_score >= 35)
            {
                hr_clip_inside_up = 2;
                hr_clip_inside_dn = 2;
            }
            else if (alg_data->ppg_signal_quality.sq_score < 10)
            {
                if (alg_data->hr_internal_pre > (alg_data->gsen_pedometer.hr_up_exp + alg_data->gsen_pedometer.hr_down_exp) / 2 + 10)
                {
                    hr_clip_inside_up = 0;
                    hr_clip_inside_dn = 1;
                }
                else if (alg_data->hr_internal_pre < (alg_data->gsen_pedometer.hr_up_exp + alg_data->gsen_pedometer.hr_down_exp) / 2 - 10)
                {
                    hr_clip_inside_up = 1;
                    hr_clip_inside_dn = 0;
                }
            }
        }
        else
        {
            if (alg_data->ppg_signal_quality.sq_score >= 35)
            {
                if(alg_data->hr_internal_pre>130)
                {
                    hr_clip_inside_up = 3;
                    hr_clip_inside_dn = 2;
                }
                else
                {
                    hr_clip_inside_up = 4;
                    hr_clip_inside_dn = 2;											
                }
                if (alg_data->hr_internal_pre >= alg_data->gsen_pedometer.hr_up_exp)
                {
                    hr_clip_inside_up = 2;
                }
                if (alg_data->hr_internal_pre > 170)
                {
                    hr_clip_inside_up = 1;
                }
            }
            else if (alg_data->ppg_signal_quality.sq_score >= 20 && alg_data->ppg_signal_quality.sq_score < 35)
            {
                if (alg_data->hr_internal_pre > (alg_data->gsen_pedometer.hr_up_exp + alg_data->gsen_pedometer.hr_down_exp) / 2)
                {
                    hr_clip_inside_up = 2;
                }
                else if(alg_data->hr_internal_pre <= alg_data->gsen_pedometer.hr_down_exp)
                {
                    hr_clip_inside_up = 5;
                }
                else
                {
                    hr_clip_inside_up = 3;
                }
                if (alg_data->hr_internal_pre >= alg_data->gsen_pedometer.hr_up_exp)
                {
                    hr_clip_inside_dn = 2;
                }
                else
                {
                    hr_clip_inside_dn = 1;
                }
            }
            else
            {
                if (alg_data->hr_internal_pre < 110 && alg_data->run_state_cnt >= 10)
                {
                    hr_clip_inside_up = 3;
                    hr_clip_inside_dn = -1;
                }
                else if (alg_data->hr_internal_pre < 140)
                {
                    hr_clip_inside_up = 2;
                    hr_clip_inside_dn = 1;
                }
                else if (alg_data->hr_internal_pre > alg_data->gsen_pedometer.hr_up_exp + 10)
                {
                    hr_clip_inside_up = 0;
                    hr_clip_inside_dn = 1;
                }
                else
                {
                    hr_clip_inside_up = 1;
                    hr_clip_inside_dn = 1;
                }
                if(alg_data->hr_internal_pre > 120 && abs(alg_data->hr_internal-alg_data->hr_internal_pre) <= 2)
                {
                    hr_clip_inside_up = 1;
                    hr_clip_inside_dn = 1;
                }
            }
            if(gsen_x_max_value[GSEN_PEAKS_NUM-1]<90 \
                && (abs(abs(alg_data->hr_internal - gsen_x_max_value[GSEN_PEAKS_NUM-1]*2) - 10 ) <= 2 || abs(alg_data->hr_internal - gsen_x_max_value[GSEN_PEAKS_NUM-1]*2)<=3))
            {
                if(alg_data->hr_runpeak_follow_cnt>=1)
                {
                    alg_data->hr_runpeak_follow_cnt = 0;
                    hr_clip_inside_up = 1;
                    hr_clip_inside_dn = 1;
                }
                else
                {
                    alg_data->hr_runpeak_follow_cnt++;
                    hr_clip_inside_up = 0;
                    hr_clip_inside_dn = 0;
                }
            }
        }

        if(sports_mode_menu == RIDE_MODE && alg_data->senddata_motion_thisdex > 0)
        {
            if(alg_data->hr_internal_pre<120)
            {
                hr_clip_inside_up = 2;
                hr_clip_inside_dn = 1;
            }
            else
            {
                hr_clip_inside_up = 1;
                hr_clip_inside_dn = 1;						
			}
        }
        else if(sports_mode_menu == WALK_MODE && alg_data->senddata_motion_thisdex > 0)
        {
            if(alg_data->hr_internal_pre<120)
            {
                hr_clip_inside_up = 2;
                hr_clip_inside_dn = 1;						
            }
            else
            {
                hr_clip_inside_up = 1;
                hr_clip_inside_dn = 1;					
			}
        }
        else if(sports_mode_menu == ALLDAY_HR_MODE && alg_data->senddata_motion_thisdex > 0)
        {
            if(alg_data->senddata_motion_thisdex < 4)
            {
                if(alg_data->hr_internal_pre>100)
                {
                    hr_clip_inside_up = 1;
                }
                else if(alg_data->hr_internal_pre>100+alg_data->senddata_motion_thisdex*10)
                {
                    hr_clip_inside_up = 0;
                }
            }
            else
            {
                if(alg_data->hr_internal_pre>100+alg_data->senddata_motion_thisdex*10)
                {
                    hr_clip_inside_up = 1;
                }
            }
        }
        else if(sports_mode_menu == HIGH_POW_MODE && alg_data->senddata_motion_thisdex > 1)
        {
            if(alg_data->hr_internal_pre<100)
            {
                if(alg_data->senddata_motion_thisdex == 2)
                {
                    hr_clip_inside_up = 3;
                    hr_clip_inside_dn = 0;
                }
                else if(alg_data->senddata_motion_thisdex == 3)
                {
                    hr_clip_inside_up = 3;
                    hr_clip_inside_dn = -1;
                }
                else
                {
                    hr_clip_inside_up = 3;
                    hr_clip_inside_dn = -1;
                }		
            }
            else if(alg_data->hr_internal_pre<120)
            {
                if(alg_data->senddata_motion_thisdex == 2)
                {
                    hr_clip_inside_up = 3;
                    hr_clip_inside_dn = 1;
                }
                else if(alg_data->senddata_motion_thisdex == 3)
                {
                    hr_clip_inside_up = 3;
                    hr_clip_inside_dn = 0;
                }
                else
                {
                    hr_clip_inside_up = 3;
                    hr_clip_inside_dn = -1;
                }				
            }
            else if(alg_data->hr_internal_pre<140)
            {
                if(alg_data->senddata_motion_thisdex == 2)
                {
                    hr_clip_inside_up = 2;
                    hr_clip_inside_dn = 1;
                }	
                else
                {
                    hr_clip_inside_up = 3;
                    hr_clip_inside_dn = 1;
                }
            }
            else
            {
                hr_clip_inside_up = 2;
                hr_clip_inside_dn = 1;
            }
        }
        
        if(sports_mode_menu == RIDE_MODE)
        {
            if (alg_data->run_dn_cnt > 2 && alg_data->senddata_motion_thisdex > 0)
            {
                hr_clip_inside_dn = 0;
            }
        }
        else if(sports_mode_menu == WALK_MODE)
        {
            if (alg_data->run_dn_cnt > 2 && alg_data->senddata_motion_thisdex > 0 && alg_data->hr_internal_pre < 110)
            {
                hr_clip_inside_dn = 0;
            }
        }
        else
        {
            if (alg_data->runrun_cnt > 20 && alg_data->run_dn_cnt > 3 && alg_data->senddata_motion_thisdex >= 4)
            {
                if(alg_data->hr_internal_pre < 120)
                {
                    hr_clip_inside_dn = -1;
                }
                else if(alg_data->hr_internal_pre < 140)
                {
                    hr_clip_inside_dn = 0;
                }
            }
        }


		if(alg_data->data_cnt == FFT_WIN_SIZE + HRM_CHIP_SETTLE + HRM_FLT_SETTLE)
        {
            if(sports_mode_menu == ALLDAY_HR_MODE)
            {
                if(alg_data->senddata_motion_thisdex==0 && hrm_snr>16)
                {
                    if(alg_data->hr_internal>120)
                    {
                        alg_data->hr_internal = 120 + alg_data->hr_internal%10;
                    }
                    else if(alg_data->hr_internal<75)
                    {
                        alg_data->hr_internal = 70 + alg_data->hr_internal%10;
                    }
                }
                else
                {
                    if(alg_data->hr_internal>100)
                    {
                        alg_data->hr_internal = 100 + alg_data->hr_internal%10;
                    }
                    else if(alg_data->hr_internal<75)
                    {
                        alg_data->hr_internal = 70 + alg_data->hr_internal%10;
                    }
                }
            }
            else
            {
                if(alg_data->senddata_motion_thisdex==0 && hrm_snr>16)
                {
                }
                else
                {
                    if(alg_data->hr_internal>95)
                    {
                        alg_data->hr_internal = 90 + alg_data->hr_internal%10;
                    }
                }
            }
            alg_data->new_start = 0;
            alg_data->hr_internal_pre = alg_data->hr_internal;
			alg_status = MSG_HRS_READY;
        }

        /////////////////////////// signal_quality /////////////////////////////////////
        if(alg_data->senddata_motion_thisdex==0)
        {
            if(alg_data->data_cnt == FFT_WIN_SIZE+HRM_CHIP_SETTLE+HRM_FLT_SETTLE)
            {
                for(ii=0;ii<5;ii++)
                {
                    alg_data->hrm_snr_value_fifo[ii] = hrm_snr;
                    hrm_snr_value_sum = hrm_snr_value_sum + alg_data->hrm_snr_value_fifo[ii];
                }
                alg_data->hrm_snr_smooth = hrm_snr_value_sum/5;
            }
            else if(alg_data->data_cnt > FFT_WIN_SIZE+HRM_CHIP_SETTLE+HRM_FLT_SETTLE)
            {
                if(hrm_snr>0)
                {
                    for(ii=0;ii<4;ii++)
                    {
                        alg_data->hrm_snr_value_fifo[ii] = alg_data->hrm_snr_value_fifo[ii+1];
                        hrm_snr_value_sum = hrm_snr_value_sum + alg_data->hrm_snr_value_fifo[ii];
                    }
                    alg_data->hrm_snr_value_fifo[4] = hrm_snr;
                    hrm_snr_value_sum = hrm_snr_value_sum + alg_data->hrm_snr_value_fifo[4]; 
                    alg_data->hrm_snr_smooth = hrm_snr_value_sum/5;
                }
            }
            alg_data->hr_quality = alg_data->hrm_snr_smooth;
        }
        else
        {
            alg_data->hr_quality = 255;
        }
		//DEBUG_PRINTF("%d %d %d %d %d\r\n", alg_data->data_cnt, alg_data->hr_quality, hrm_snr, alg_data->check_living_green_std_smooth, living_status);
        ////////////////////////////////////////////////////////////////////////////////

        if (alg_data->new_start == 0)
        {
			if (alg_data->hr_internal_pre - hr_clip_inside_dn > alg_data->hr_internal)
            {
                    alg_data->hr_internal = alg_data->hr_internal_pre - hr_clip_inside_dn;
            }
            else if (alg_data->hr_internal_pre + hr_clip_inside_up < alg_data->hr_internal)
            {
                    alg_data->hr_internal = alg_data->hr_internal_pre + hr_clip_inside_up;
            }
        }
        else
        {
            alg_data->hr_internal = hr_new;
        }

        
        if(sports_mode_menu == RIDE_MODE || sports_mode_menu == WALK_MODE)
        {
            if (alg_data->hr_internal < alg_data->hr_internal_pre && alg_data->senddata_motion_thisdex > 0)
            {
                if (alg_data->run_dn_cnt < 10)
                {
                    alg_data->run_dn_cnt++;
                }
            }
            else
            {
                if (alg_data->run_dn_cnt > 0)
                {
                    alg_data->run_dn_cnt--;
                }
            }
        }
        else
        {
            if (alg_data->runrun_cnt > 20 && alg_data->hr_internal < alg_data->hr_internal_pre)
            {
                if (alg_data->run_dn_cnt < 10)
                {
                    alg_data->run_dn_cnt++;
                }
            }
            else
            {
                if (alg_data->run_dn_cnt > 0)
                {
                    alg_data->run_dn_cnt--;
                }
            }
        }

///////////////////////////////////////////CAL_START///////////////////////////////////////
        if(alg_data->data_cnt>750)
        {
            if(alg_data->data_cnt<15000)
            {
                time_range = 6;
            }
            else if(alg_data->data_cnt<45000)
            {
                time_range = 8;
            }
            else
            {
                time_range = 10;
            }
            
            if(alg_data->hr_internal<80)
            {
                alg_data->calorie = alg_data->calorie+15;
            }
            else if(alg_data->hr_internal<100)
            {
                alg_data->calorie = alg_data->calorie+20;
            }
            else if(alg_data->hr_internal<120)
            {
                alg_data->calorie = alg_data->calorie+10*time_range;
            }
            else if(alg_data->hr_internal<140)
            {	
                alg_data->calorie = alg_data->calorie+15*time_range;
            }
            else if(alg_data->hr_internal<160)
            {
                alg_data->calorie = alg_data->calorie+20*time_range;
            }
            else if(alg_data->hr_internal<180)
            {
                alg_data->calorie = alg_data->calorie+22*time_range;
            }
            else
            {
                alg_data->calorie = alg_data->calorie+25*time_range;
            }
        }
				
				
///////////////////////////////////////////CAL_END/////////////////////////////////////////
        if (alg_data->hr_times_8 == 0)
        {
            alg_data->hr_times_8 = alg_data->hr_internal << 3;
        }
        else
        {
            alg_data->hr_times_8 = (alg_data->hr_times_8 * 3 + (alg_data->hr_internal << 3)) >> 2;
        } 
		alg_data->hr_result = alg_data->hr_times_8 >> 3;

#ifdef BP_CUSTDOWN_ALG_LIB

        if ((alg_data->data_cnt >400) && (alg_data->hr_result != 0) && (alg_data->sbp == 0))
        {
            tyhx_bp_age_fn(alg_data->hr_result, body_sign);     // 计算血压值
        }    //alg_data->data_cnt == BP_TEST_TIME
#endif
    } 

    if(Check_Living_Mode == 1 && alg_data->data_cnt >= HRM_CHIP_SETTLE+HRM_FLT_SETTLE+75)
    {
        if(alg_data->check_living_green_std_smooth >= ST_Thre)
        {
            if(alg_data->hr_quality >= QU_Thre)
            {                    
                living_status = MSG_LIVING_TRUE;
            }
        }
        else
        {
            living_status = MSG_LIVING_FAIL;
        }
        //DEBUG_PRINTF("%d %d %d\r\n",alg_data->data_cnt,alg_data->check_living_green_std_smooth,alg_data->hr_quality);
    }
    else if(Check_Living_Mode == 2 && alg_data->data_cnt >= HRM_CHIP_SETTLE+HRM_FLT_SETTLE+75)
    {
        if(alg_data->check_living_green_std_smooth >= ST_Thre)
        {
            if(alg_data->hr_quality >= QU_Thre)
            {                    
                living_status = MSG_LIVING_TRUE;
            }
        }
        else
        {
            if(alg_data->hr_quality >= QU_Thre*3/2)
            {
                living_status = MSG_LIVING_TRUE;
            }
        }
        //DEBUG_PRINTF("%d %d %d\r\n",alg_data->data_cnt,alg_data->check_living_green_std_smooth,alg_data->hr_quality);
    }
    else
    {
        living_status = MSG_LIVING_INITIAL;
    }
//////////////////// print hr_results ////////////////////
    if ((((alg_data->data_cnt) % HR_UP_TIME) == 0) && \
        (alg_data->data_cnt >= 175) && alg_data->data_cnt <= 7000) {
        DEBUG_PRINTF("TP3:                    %d %d %d     %d %d   %d %d   %d %d\r\n", \
                     alg_data->data_cnt, alg_data->hr_result, alg_data->hr_internal, \
                     hr_new, found, hr_clip_inside_up, hr_clip_inside_dn, alg_data->ppg_signal_quality.sq_score, alg_data->senddata_motion_thisdex);
    }


    //save previouse data for min point search///////////////////////////////////////////////////////////////////////////////
    alg_data->data_cnt = alg_data->data_cnt + 1;
    
    return true;
}

HRS_ALG_INFO_T tyhx_get_hrs_alg_info(void)
{
		HRS_ALG_INFO_T hrs_alg_info;
		hrs_alg_info.data_cnt = alg_data->data_cnt;
		hrs_alg_info.motion_status = alg_data->senddata_motion_thisdex;
		hrs_alg_info.gsen_pow = alg_data->gsen_pedometer.pow_all_temp;
		hrs_alg_info.up_clip = 0;
		hrs_alg_info.dn_clip = 0;
		return hrs_alg_info;
}

hrs_results_t tyhx_hrs_alg_get_results(void)
{
    hrs_results_t alg_results = {MSG_HRS_ALG_NOT_OPEN,MSG_LIVING_INITIAL,0,0,0,0,0};
	alg_results.hrs_alg_status = alg_status;
    alg_results.living_status = living_status;
    alg_results.data_cnt = alg_data->data_cnt;
    alg_results.hr_result_qual = alg_data->hr_quality;
    alg_results.hr_result_std = alg_data->check_living_green_std_smooth;
    if (alg_results.hrs_alg_status == MSG_HRS_READY && (alg_results.living_status == MSG_LIVING_TRUE || Check_Living_Mode==0))
    {
        alg_results.hr_result =  alg_data->hr_result;
		alg_results.cal_result = alg_data->calorie>>10;
    }
    else
    {
        alg_results.hr_result = 0;
		alg_results.cal_result = 0;
    }
    return alg_results;
}

void tyhx_hrs_set_sports_mode(hrs_sports_mode_t sports_mode)
{
		sports_mode_menu = sports_mode;
}

#ifdef BP_CUSTDOWN_ALG_LIB

void tyhx_bp_set_exinf(uint8_t age, uint8_t height, uint8_t weight, uint8_t gender, uint8_t ref_sbp, uint8_t ref_dbp)
{
    body_sign.age = age;
    body_sign.height = height;
    body_sign.weight = weight;
    body_sign.gender = gender;
    body_sign.ref_sbp = ref_sbp;
    body_sign.ref_dbp = ref_dbp;
}

void tyhx_bp_age_fn(uint8_t hr_result, bp_exinf_t body_s)
{
    int16_t   sbp_age = 0;
    int16_t   dbp_age = 0;
    int16_t   sbp_hr_new = 0;
    int16_t   dbp_hr_new = 0;
    uint16_t  body_bmi_8 = 0;
    uint8_t age = body_s.age;
    uint8_t gender = body_s.gender;

    if (age <= 25)
    {
        sbp_age = 115 - (gender << 2) - gender;
        dbp_age = 73  - (gender << 1);
    }
    else if (age <= 30)
    {
        sbp_age = 115 - (gender << 1) - gender;
        dbp_age = 75  - (gender << 1);
    }
    else if (age <= 35)
    {
        sbp_age = 117 - (gender << 1) - gender;
        dbp_age = 76  - (gender << 1);
    }
    else if (age <= 40)
    {
        sbp_age = 120 - (gender << 2);
        dbp_age = 80  - (gender << 1) - gender;
    }
    else if (age <= 45)
    {
        sbp_age = 124 - (gender << 1);
        dbp_age = 81  - (gender << 1) - gender;
    }
    else if (age <= 50)
    {
        sbp_age = 128 ;
        dbp_age = 82  - (gender << 1) - gender;
    }
    else if (age <= 55)
    {
        sbp_age = 134 ;
        dbp_age = 84  - (gender << 2);
    }
    else if (age <= 60)
    {
        sbp_age = 137 + (gender << 1);
        dbp_age = 84  - (gender << 1);
    }
    else
    {
        sbp_age = 148 - (gender << 1) - gender;
        dbp_age = 86  - (gender << 1) - gender;
    }

    if (hr_result < 65)
    {
        if (hr_result < 58)
        {
            sbp_hr_new = (130 + (((hr_result - 70) * 3) >> 1)); // hr < 58
            dbp_hr_new = (85 + (hr_result - 70));
        }
        else
        {
            sbp_hr_new = (120 + (((hr_result - 70) * 3) >> 1)); // 58< hr < 65
            dbp_hr_new = (75 + (hr_result - 70));
        }
    }
    else if (hr_result > 85)
    {
        sbp_hr_new = (95 + (((hr_result - 70) * 3) >> 1)); // 85< hr
        dbp_hr_new = 73 + hr_result % 10;
    }
    else
    {
        sbp_hr_new = (110 + (((hr_result - 70) * 3) >> 1)); // 65< hr <85
        dbp_hr_new = 68 + hr_result % 10;
    }
    if (sbp_hr_new > 140)
    {
        sbp_hr_new = 135 + hr_result % 10;
    }

    if ((body_s.height != 0) && (body_s.weight != 0))
    {
        body_bmi_8 = ((10000 * body_s.weight) << 3) / (body_s.height * body_s.height); // add 8*bmi 20170428
    }

    if ((body_s.age > 0) && (body_s.gender == 0))
    {
        //  for male
        if (body_s.age < 35)
        {
            sbp_hr_new = ((sbp_hr_new - 115) >> 1) + sbp_age ;
            dbp_hr_new = ((dbp_hr_new - 74) >> 1) +  dbp_age ;
            if ((body_bmi_8 != 0) && (body_bmi_8 < 184) && (hr_result > 70))
            {
                sbp_hr_new = sbp_hr_new - 5 ;
            }
        }
        else
        {
            sbp_hr_new = ((sbp_hr_new - 115) >> 1) + sbp_age + ((body_bmi_8 - 184) * 46) / 80;
            dbp_hr_new = ((dbp_hr_new - 74) >> 1) +  dbp_age ;
            if ((body_s.height < 165) && (body_bmi_8 > 200))
            {
                sbp_hr_new = sbp_hr_new + 10 ;
            }
        }
    }

    if ((body_s.age > 0) && (body_s.gender == 1))
    {
        //  for female
        if (body_s.age < 35)
        {
            sbp_hr_new = ((sbp_hr_new - 110) >> 2) + sbp_age ;
            dbp_hr_new = ((dbp_hr_new - 72) >> 2) +  dbp_age ;
            if ((body_bmi_8 != 0) && (body_bmi_8 < 176))
            {
                sbp_hr_new = sbp_hr_new - 5 ;
            }
        }
        else
        {
            sbp_hr_new = ((sbp_hr_new - 110) >> 1) + sbp_age + ((body_bmi_8 - 184) >> 2);
            dbp_hr_new = ((dbp_hr_new - 72) >> 1) +  dbp_age ;
        }
    }

    if (body_s.ref_sbp != 0 && body_s.ref_dbp != 0 && body_s.gender == 0)
    {
        sbp_hr_new = sbp_hr_new + body_s.ref_sbp - 118;
        dbp_hr_new = dbp_hr_new + body_s.ref_dbp - 75;
    } // calculate bp by input ref_bp

    if (body_s.ref_sbp != 0 && body_s.ref_dbp != 0 && body_s.gender == 1)
    {
        sbp_hr_new = sbp_hr_new + body_s.ref_sbp - 112;
        dbp_hr_new = dbp_hr_new + body_s.ref_dbp - 70;
    } // calculate bp by input ref_bp

    alg_data->sbp = sbp_hr_new ;
    alg_data->dbp = dbp_hr_new ;

    // DEBUG_PRINTF(0," BP_CUSTDOWN_ALG_LIB bp  is hrs:%d, sbp: %d , dbp: %d \r\n" ,hr_result, alg_data->sbp , alg_data->dbp );
}

void tyhx_restart_bp_cal(void)
{
    alg_data->bp_cnt = alg_data->data_cnt;
    alg_data->sbp = 0;
    alg_data->dbp = 0;

}
bp_results_t tyhx_alg_get_bp_results(void)
{
    alg_bp_results.sbp = alg_data->sbp;
    alg_bp_results.dbp = alg_data->dbp;
    alg_bp_results.hr_result = alg_data->hr_result;
    alg_bp_results.data_cnt =  alg_data->data_cnt;
    //      alg_bp_results.object_flg = alg_data->object_flg;
    return   alg_bp_results;
}
#endif


#endif /*HRS_ALG_LIB*/
