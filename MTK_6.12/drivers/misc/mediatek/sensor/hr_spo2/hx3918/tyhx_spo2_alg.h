#ifndef _SPO2_ALG_H_
#define _SPO2_ALG_H_
#endif
/* USD add hr_spo2 algorithm update by longtao.ran 20240924 start */
//#include <stdint.h>

//version:3.3.33

//#include "tyhx_spo2_alg_para.h"



#ifdef SPO2_ALG_LIB

typedef enum {
	MSG_SPO2_ALG_NOT_OPEN,
	MSG_SPO2_ALG_OPEN,
	MSG_SPO2_READY,
	MSG_SPO2_ALG_TIMEOUT,
    MSG_SPO2_END
} spo2_alg_msg_code_t;

typedef enum {
	MSG_SPO2_LIVING_INITIAL,
	MSG_SPO2_LIVING_TRUE,
	MSG_SPO2_LIVING_FAIL
} spo2_living_code_t;

typedef enum {
	SPO2_NORMAL_MODE,
	SPO2_ALLDAY_MODE
} spo2_mode_t;

typedef enum {
    RESULT_WITHOUT_ERR,
	RESULT_MOTION_ERR,
	RESULT_SCREENUP_ERR,
    RESULT_SIGNAL_ERR
} spo2_err_value_t;

typedef struct {
    spo2_alg_msg_code_t		spo2_alg_status;
    spo2_living_code_t      spo2_living_status;
    spo2_err_value_t    result_err;
    uint32_t		data_cnt;
    uint8_t			spo2_result;
    uint8_t			hr_result;
    uint8_t			block_cnt;
    uint8_t			ir_quality;
    uint16_t		reliable_score_result;

    uint8_t			screen_up;
    uint8_t			motion;
    uint8_t			poor_signal;
    
    uint8_t         signal_snr;
    uint32_t        signal_std;
    spo2_mode_t     spo_mode;
    
} spo2_results_t;

typedef struct {
    uint8_t			screen_up;
    uint8_t			motion;
    uint8_t			poor_signal;
} spo2_gesture_recognize_t;

typedef struct {
	uint32_t data_cnt;
    uint8_t  motion;
	uint8_t ir_offset_50;
	uint8_t red_offset_50;
	uint8_t green_offset_50;
	int32_t ir_dc;
	int32_t red_dc;
	int32_t green_dc;
    int32_t dc_dacia;
    int32_t ir_ac;
	int32_t red_ac;
	int32_t green_ac;
	int32_t sa_temp;
	int32_t gi_temp;
	int32_t sa_final;
	uint16_t ir_fft_snr;
	uint16_t red_fft_snr;
    uint8_t count_block;
	uint8_t spo2_result;
} spo2_debug_t;

typedef struct {
	  uint8_t	red_offset_idac;
	  uint8_t	ir_offset_idac; 
	  uint8_t	green_offset_idac; 
	  uint16_t	spo2_start_cnt;
} spo2_agcpara_t;

typedef struct {
	uint8_t XCORR_MODE;
	uint8_t QUICK_RESULT;
} spo2_switch_t;
//USD: add hx3918 drv by longtao.ran 20240829 start
typedef struct {
	uint8_t  SPO2_LOW_XCORR_THRE;
	uint8_t  SPO2_LOW_SNR_THRE;
	uint8_t  COUNT_BLOCK_NUM;
	uint32_t SPO2_BASE_LINE_INIT;
	uint32_t SPO2_SLOPE;
	uint16_t SPO2_GSEN_POW_THRE;
	uint32_t AMB_THRE;
} spo2_usuallyadjust_t;

typedef struct {
	uint16_t MEAN_NUM;
	int32_t  SOP2_DEGLITCH_THRE;
	int32_t  SPO2_REMOVE_JUMP_THRE;
	uint16_t SPO2_LOW_CLIP_END_TIME;
	uint16_t SPO2_LOW_CLIP_DN;
	uint16_t SPO2_NORMAL_CLIP_DN;
	uint16_t IR_AC_TOUCH_THRE;
	uint16_t IR_FFT_POW_THRE;
	uint8_t  SPO2_CALI;
	uint8_t  SLOPE_PARA_MAX;
	uint8_t  SLOPE_PARA_MIN;
} spo2_barelychange_t;

void tyhx_spo2_set_switch(uint8_t XCORR_MODE,uint8_t QUICK_RESULT);
void tyhx_spo2_para_usuallyadjust(uint8_t SPO2_LOW_XCORR_THRE,uint8_t SPO2_LOW_SNR_THRE,uint8_t COUNT_BLOCK_NUM,uint32_t SPO2_BASE_LINE_INIT,uint32_t SPO2_SLOPE,uint16_t SPO2_GSEN_POW_THRE,uint32_t AMB_THRE);
void tyhx_spo2_para_barelychange(uint16_t MEAN_NUM,int32_t SOP2_DEGLITCH_THRE,int32_t SPO2_REMOVE_JUMP_THRE, \
                                   uint16_t SPO2_LOW_CLIP_END_TIME,uint16_t SPO2_LOW_CLIP_DN,uint16_t SPO2_NORMAL_CLIP_DN, \
                                   uint16_t IR_AC_TOUCH_THRE,uint16_t IR_FFT_POW_THRE,uint8_t SPO2_CALI,uint8_t SLOPE_PARA_MAX,uint8_t SLOPE_PARA_MIN);

void tyhx_spo2_set_living(uint8_t living_mode, uint8_t qu_thre, uint8_t st_thre);
void tyhx_spo2_set_mode(spo2_mode_t mode);
bool tyhx_spo2_alg_send_data(int32_t *red_new_raw_data,int32_t *ir_new_raw_data,int32_t *green_new_raw_data,uint8_t R_LEDDAC,uint8_t IR_LEDDAC,uint8_t G_LEDDAC, int16_t *gsen_data_x, int16_t *gsen_data_y, int16_t *gsen_data_z, int32_t *amb_data, uint8_t dat_len);
void tyhx_spo2_alg(int32_t red_new_raw_data , int32_t ir_new_raw_data, int32_t green_new_raw_data,uint8_t R_LEDDAC,uint8_t IR_LEDDAC,uint8_t G_LEDDAC,int32_t amb_data);
//USD: add hx3918 drv by longtao.ran 20240829 end
void tyhx_spo2_alg_close(void);
bool tyhx_spo2_alg_open(void);
bool tyhx_spo2_alg_open_deep(void);
spo2_results_t tyhx_spo2_alg_get_results(void);
spo2_debug_t tyhx_spo2_alg_debug_results(void);
/* USD add hr_spo2 algorithm update by longtao.ran 20240924 end */
#endif 

