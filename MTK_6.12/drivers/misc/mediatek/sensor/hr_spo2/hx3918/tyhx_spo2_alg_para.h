#ifndef _TYHX_SPO2_ALG_PARA_H_
#define _TYHX_SPO2_ALG_PARA_H_

//version:3.3.33


//#define SPO2_FIR1_B1        1    
//#define SPO2_FIR1_B2        0
//#define SPO2_FIR1_B3        -1
//#define SPO2_IIR1_A2        -113
//#define SPO2_IIR1_A3        52
//#define SPO2_FIR2_B1        1    
//#define SPO2_FIR2_B2        0
//#define SPO2_FIR2_B3        -1
//#define SPO2_IIR2_A2        -78
//#define SPO2_IIR2_A3        36

#define SPO2_FIR1_B1        1    
#define SPO2_FIR1_B2        0
#define SPO2_FIR1_B3        -1
#define SPO2_IIR1_A2        -111
#define SPO2_IIR1_A3        51
#define SPO2_FIR2_B1        1    
#define SPO2_FIR2_B2        0
#define SPO2_FIR2_B3        -1
#define SPO2_IIR2_A2        -68
#define SPO2_IIR2_A3        32

#define SPO2_GSEN_FIR1_B1        1    
#define SPO2_GSEN_FIR1_B2        0
#define SPO2_GSEN_FIR1_B3        -1
#define SPO2_GSEN_IIR1_A2        -109    //-113, Version_new
#define SPO2_GSEN_IIR1_A3        51      //52, Version_new
#define SPO2_GSEN_FIR2_B1        1    
#define SPO2_GSEN_FIR2_B2        0
#define SPO2_GSEN_FIR2_B3        -1
#define SPO2_GSEN_IIR2_A2        -83     //-78, Version_new
#define SPO2_GSEN_IIR2_A3        42      //36, Version_new

#define SPO2_FIR1_B1_0204        1    
#define SPO2_FIR1_B2_0204        0
#define SPO2_FIR1_B3_0204        -1
#define SPO2_IIR1_A2_0204        -499
#define SPO2_IIR1_A3_0204        245    
#define SPO2_FIR2_B1_0204        1    
#define SPO2_FIR2_B2_0204        0
#define SPO2_FIR2_B3_0204        -1
#define SPO2_IIR2_A2_0204        -504     
#define SPO2_IIR2_A3_0204        249 

#define SPO2_CHIP_SETTLE    9
#define SPO2_FLT_SETTLE     16
#define SPO2_FFT_WIN_SIZE   150
#define SPO2_HR_PEAKS_NUM   6

#define USER_MOTION_ERR  1
#define USER_SCREENUP_ERR  2
#define USER_SIGNAL_ERR  3


typedef struct {
	int32_t     fir1_dly;   
	int32_t     fir1_dly2;
	int32_t     sec1_dly;
	int32_t     sec1_dly2;
	int32_t     sec1_out;   
	int32_t     fir2_dly;   
	int32_t     fir2_dly2;  
	int32_t     sec2_dly;
	int32_t     sec2_dly2;
	int32_t     filter_out;
} spo2_iir_data_t;

typedef struct {  
	int16_t     pow_all_fifo[5];
    int32_t     gsen_powpeak;
	int16_t     pow_all_temp;
	int32_t     gsen_data_cnt;
	uint8_t     motion_status;
	spo2_iir_data_t iir_data_gsen_x;
	spo2_iir_data_t iir_data_gsen_y;
	spo2_iir_data_t iir_data_gsen_z;
} spo2_motion_pedometer_t;

typedef struct {  
    int32_t     check_living_green[16];
    int32_t     check_living_green_std;
    int32_t     check_living_green_std_avg;
    int32_t     check_living_green_std_fifo[5];
    int32_t     check_living_ir[16];
    int32_t     check_living_ir_std;
    int32_t     check_living_ir_std_avg;
    int32_t     check_living_ir_std_fifo[5];
    int32_t     check_living_snr_value_fifo[5];
    int32_t     check_living_snr_avg;
    uint8_t     checkliving_close;
} spo2_checkliving_t;

typedef struct {  
    uint32_t    data_cnt;
    uint8_t     spo2_result;
    uint8_t     hr_result;
    int32_t     gsen_z_smooth;

    spo2_iir_data_t  red_iir_data;
    spo2_iir_data_t  ir_iir_data;
    spo2_iir_data_t  green_iir_data;
    spo2_iir_data_t  red0204_iir_data;

    int32_t     ir_fifo_filter[7];
    int32_t     red_fifo_filter[7];
    int32_t     green_fifo_filter[7];
    int32_t     red0204_fifo_filter[7];
    int32_t     ir_peak_fifo[8];
    int32_t     red_peak_fifo[8];
    int32_t     green_peak_fifo[8];
    int32_t     ir_valley_fifo[8];
    int32_t     red_valley_fifo[8];
    int32_t     green_valley_fifo[8];
    int32_t     ir_peak_ave;
    int32_t     red_peak_ave;
    int32_t     green_peak_ave;
    int32_t     red0204_peak_ave;
    int32_t     ir_valley_ave;
    int32_t     red_valley_ave;
    int32_t     green_valley_ave;
    int32_t     red0204_valley_ave;

    int32_t	    ir_data_dc; 
    int32_t	    ir_data_ac; 
    int32_t	    red_data_dc; 
    int32_t	    red_data_ac; 
    int32_t	    dc_dacia;
    int32_t	    green_data_dc; 
    int32_t	    green_data_ac; 
    int32_t     red0204_data_ac;
    int32_t     ir_iir_data_smooth;
    int32_t     red_iir_data_smooth;
    int32_t     green_iir_data_smooth;
    int32_t     red0204_iir_data_smooth;
    int32_t     baseline;
    int32_t     spo2_sa;
    int32_t     spo2_sa_final;
    int32_t     spo2_sa_temp;
    int32_t     spo2_gi_temp;

    uint16_t    fft_phase[110];
    uint16_t    fft_phase_start[110];
    int32_t     ir_fft_out[110][2];
    int32_t     ir_flt_data_fifo[150];
    int32_t     red_fft_out[110][2];
    int32_t     red_flt_data_fifo[150];
    int32_t     green_fft_out[110][2];
    int32_t     green_flt_data_fifo[150];

    uint8_t     red_offset_50;
    uint8_t     ir_offset_50;
    uint8_t     green_offset_50;

    uint8_t     hr;
    uint8_t     hr_pre;
    uint8_t     ir_hr;
    uint8_t     red_hr;
    uint8_t     green_hr;

    uint8_t     start_ind;
    uint8_t     green_start_ind; 
    uint16_t    ir_fft_snr;
    uint16_t    red_fft_snr;
    uint16_t    green_fft_snr;
    uint8_t     count_block;
    uint8_t     snr_block;
    
    int32_t     red_data_pre;
    int32_t     ir_data_pre;
    int32_t     green_data_pre;
    int32_t     red_jump_delta;
    int32_t     ir_jump_delta;
    int32_t     green_jump_delta;
    int32_t     red_data_cali;
    int32_t     ir_data_cali;
    int32_t     green_data_cali;
    int32_t     red_data_fifo[4];
    int32_t     ir_data_fifo[4];
    int32_t     green_data_fifo[4];
    
    int8_t      rand_cnt;
    int8_t      rand_cnt1;
    int8_t      spo2_rand;
    int8_t      spo2_rand1;
    uint16_t    signal_quality_fifo[6];

    uint8_t     screen_up;
    uint8_t     motion;
    uint8_t     poor_signal;
    int32_t     ir_baseline;
    uint8_t     spo2_result_flag;
    
    uint8_t     hq_sig_cnt;
    uint8_t     hq_sig_flag;
    uint8_t     spo2_result_min;
    
    uint16_t    red_snr_sum;
    uint16_t    red_snr_avg;
    uint8_t     lq_sig_flag;
    
    uint8_t     motion_err_cnt; 
    uint8_t     screenup_err_cnt; 
    uint8_t     signal_err_cnt; 
    uint8_t     user_err_cnt; 
    uint8_t     user_err;
//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 start
    uint32_t    mic_move_cnt;
//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 end
    spo2_motion_pedometer_t   tyhx_pedometer;
    spo2_checkliving_t  checkliving_data;
} spo2_alg_data_t;

#endif
