#ifndef _TYHX_HRS_ALG_PARA_H_
#define _TYHX_HRS_ALG_PARA_H_

//#include "tyhx_hrs_alg.h"



#define HRM_FS            25    //unit is Hz,David Zou,20170120
#define HRM_TIME_INTERVAL 1000/HRM_FS    //1000/HRM_FS  //unit is ms
#define HRM_TEST_TIME_MAX 480    //unit is points


#define PRA_RATIO 1 //add ericy 20180618 for hx3600 osr1024 max 19 bits 

#define HRM_CHIP_SETTLE   9        //unit is points, should be times of 4, filter start to work  
#define HRM_FLT_SETTLE    16       //unit is points, fifo start to work, David Zou 20161206  
#define HRM_JH_PRE        50     //human judge before formally HR calculation
#define HRM_JH_THRES      30     //huamn judge threshold
#define HRM_FAST_REP_TIME -32    //report before FFT is totally done 


#define HRM_DC_THRES_HIGH 30000   //threshold of valid test
#define HRM_DC_THRES_LOW  6000     ////20161117 added by ericy for "low power in no_touch state", David Zou, 20161602

#define INFRARED_THRES  250000     //31000-329500 For no touch check 


#define AGC_SETTLE     300 //
#define frac_bits      6
#define FIR1_B1        1    
#define FIR1_B2        0
#define FIR1_B3        -1
#define IIR1_A2        -109    //-113, Version_new
#define IIR1_A3        51      //52, Version_new
#define FIR2_B1        1    
#define FIR2_B2        0
#define FIR2_B3        -1
#define IIR2_A2        -83     //-78, Version_new
#define IIR2_A3        42      //36, Version_new

#define PER_REL_SHIFT   2     //limit is avg_period +/- avg_period>>PER_REL_SHIFT
#define IIR_GAIN        1
#define FFT_POW_GAIN    IIR_GAIN*IIR_GAIN
#define SLOW_UP_FACTOR  3     //old hr weights up_factor and new hr weights 1  
#define SLOW_UP_SHIFT   2     //2^upshift=up_factor+1                          
#define HR_CLIP        8      //max variation between two calculation(8x), David Zou 20161206  5
#define HR_SLOW_CLIP   5      //max variation between two calculation(8x)                      5
#define HR_CLIP_STATIC 8      //max variation between two calculation(8x), David Zou 20161206
#define CLIP_CNT_LIM   2      //clip cnt when start 3x
#define FIFO_REJ_MAX   2      //max reject time for fifo
#define FFT_WIN_SIZE   200    //fft window size, Modified
#define DYNAMIC_THRES  20*FFT_POW_GAIN   //threshold to enter dynamic state, matlab V55
#define DYNAMIC_THRES2 1000*FFT_POW_GAIN  //threshold to enter semidynamic state, matlab V55
#define G_MAX_FORBID   1      //threshold to enter dynamic state
#define SAMPLE_SETTLE  2560
#define SAMPLE_DEPTH   2560
#define GSEN_PEAKS_NUM 5      //number of peaks in Gsensor, increased matlab V55
#define HR_PEAKS_NUM   6      //number of peaks in HRM, Modified, increased matlab V55
#define HR_PEAKS_NUM_JUMP  6  //number of peaks in HRM jump mode, Modified
#define HRM_JUMP_LIM   8     //range between two calculation
#define PEAK_MERGE_RANGE  3   //range when merge hrm peaks
#define HR_UP_TIME     25     //samples, 20180705
#define HR_CLIP_INTUP  3      //BPM, Version_new
#define HR_CLIP_INTDN  3      //BPM, Version_new
#define HR_CLIP_FRAC   8      
#define DYNAMIC_START_FREQ 15 //lowest freq points when dynamic, Version_new
#define GSEN_BLOCK1     3 
#define GSEN_BLOCK2     3     // Modified, matlab V55 
#define GSEN_NEAR_THRES 4
#define GSEN_BLOCK_JUMP1  1   //jump points1 when calculate jump
#define GSEN_BLOCK_JUMP2  4   //jump points2 when calculate jump
#define NO_FOUND_LIM   2      //no found limit to restart peak search 
#define HRM_LIM_SHIFT  5      //shift of hrm lim
#define GSEN_LIM_SHIFT 3      //shift of gsen lim
#define RELIABLE_RATIO 5      //ratio of raliable blocking of gsen
#define HANDOVER_CLIP_RATIO 3 //ratio of clip when handover happend
#define FAIL_FRAME_THRES 3000 //threshold to indicate a fail frame //20180614mark
#define ABNORMAL_JUMP_THRES 20000  
#define GLITCH_THRES     20000     //20180624mark
#define HIGHLIGHT_THRES  15000 //20180624mark
#define WEAK_SIG_THRES   60000  // 6e4 //20180624mark

typedef struct {
	int32_t flt_in_dly; 
	int32_t flt_in_dly2;
	int32_t sec1_dly;
	int32_t sec1_dly2;
	int32_t sec1_out; 
	int32_t fir2_dly; 
	int32_t fir2_dly2; 
	int32_t sec2_dly;
	int32_t sec2_dly2;
	int32_t out;
} iir_data_t;

typedef struct {  
	int16_t pow_all_fifo[5];
	int16_t gsen_powpeak;
	int16_t pow_all_temp;
	int32_t gsen_data_cnt;
	uint8_t motion_status;
	uint8_t motion_pow_lv0_cnt;
	uint8_t motion_pow_lv1_cnt;
	uint8_t motion_pow_lv2_cnt;
	uint8_t motion_pow_lv3_cnt;
	uint8_t motion_pow_lv4_cnt; 
	uint8_t motion_pow_lv5_cnt; 
	uint8_t status_this;
	uint8_t status_last;
	uint8_t hr_up_cnt;
	uint8_t hr_down_cnt;
	uint8_t hr_up_exp;
	uint8_t hr_down_exp;
	iir_data_t iir_data_gsen_x;
	iir_data_t iir_data_gsen_y;
	iir_data_t iir_data_gsen_z;
} motion_pedometer_t;

typedef struct { 
	uint8_t sq_con;
	uint8_t sq_pow;
	uint8_t sq_score;
	uint8_t hr_peak_fifo[4];
	uint8_t con_cnt;
	uint8_t nocon_cnt;
	uint8_t npeak_cnt;	
} signal_quality_t;

typedef struct {
	//hrs_alg_msg_code_t    alg_status;
	uint8_t     hr_result;
	uint8_t     hr_internal;
	uint8_t     hr_internal_pre;
	uint8_t     hr_result_qual;
	uint16_t    hr_times_8;
	uint32_t    data_cnt;
	iir_data_t  iir_data;
  int32_t     raw_data_fifo[4];	
  int32_t     abnormal_jump_delta; 
	int32_t     new_raw_data_dcrm_pre;
	uint8_t     start_ind;
  uint16_t    fft_phase[110];
  uint16_t    fft_phase_start[110];
	int32_t     hrm_fft_out[110][2];
	int32_t     gsen_x_fft_out[110][2];	
	int32_t     hrm_data_fifo[FFT_WIN_SIZE];
  int32_t     gsen_data_fifo[FFT_WIN_SIZE];
	uint8_t     senddata_motion_thisdex;
	uint8_t     new_start;
	uint8_t     hr_hold;
	uint8_t     hr_block;
    uint8_t     no_found_cnt;
	uint8_t     hr_track_peaks[HR_PEAKS_NUM];
	uint8_t     hr_track_peaks_cnt_pre[HR_PEAKS_NUM];    
	uint8_t     run_state_cnt;
	uint8_t     walk_state_cnt;
	uint16_t    gsenx_peak_pre;
	uint16_t    hq_hr, hq_hr_walk;
	uint16_t    hq_hr_cnt,hq_hr_walk_cnt;
	uint16_t    hr_max_peak_pre;
	uint16_t    walk_peak, run_peak;	
	uint8_t     runrun_cnt;
	uint8_t     run_dn_cnt;
	uint32_t    new_raw_data_pre;
	uint8_t     signal_err_cnt;
	uint8_t     signal_block_cnt;
	uint8_t     first_shot_1;
	uint8_t     first_shot_2;
	uint8_t     hr_quality_fifo[4];
	uint8_t     hr_quality;	
	uint32_t    calorie;
	uint32_t    bp_cnt;
	uint8_t     sbp;
	uint8_t     dbp;
	uint8_t     hr_runpeak_follow_cnt;
    
    uint8_t     hrm_snr_smooth;    
    int32_t     check_living_green[16];
    int32_t     check_living_green_std;
    int32_t     check_living_green_std_smooth;
    int32_t     check_living_green_std_fifo[5];
    int32_t     hrm_snr_value_fifo[5];
    
	motion_pedometer_t   gsen_pedometer;
	signal_quality_t     ppg_signal_quality;
} alg_data_t;

#endif
