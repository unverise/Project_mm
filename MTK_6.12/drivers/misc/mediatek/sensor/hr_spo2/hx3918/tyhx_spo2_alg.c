//#include <stdio.h>
//#include <stdbool.h>
//#include <stdint.h>
//#include <stdlib.h>
//#include <math.h>
#include "hx3918.h"
#include "tyhx_spo2_alg.h"
#include "tyhx_spo2_alg_para.h"
//version:3.3.33

#ifdef NO_MATH_LIB
/* USD add hr_spo2 Algorithm update by longtao.ran 20241010 start */
//extern int16_t  round(int16_t x);
//static int32_t rand(void)
//{
//    return 0;
//}
static int32_t hr_sqrt(int32_t x)
{
	int32_t val = x;
	int32_t last;
	if (val == 0) {
		return 0;
	}
	do {
		last = val;
		val = (val + x / val) / 2;
	} while (abs(val - last) > 1);
	return val;
}
#endif

//#define min(x,y)  ( (x)>(y)?(y):(x) )
//#define max(x,y)  ( (x)<(y)?(y):(x) )

#ifdef SPO2_ALG_LIB

spo2_alg_data_t *spo2_alg_data;
static spo2_alg_msg_code_t spo2_alg_status = MSG_SPO2_ALG_NOT_OPEN;
static spo2_living_code_t spo2_living_status = MSG_SPO2_LIVING_INITIAL;
static spo2_mode_t spo2_mode = SPO2_NORMAL_MODE;//SPO2_NORMAL_MODE SPO2_ALLDAY_MODE
static uint8_t SPO2_Check_Living_Mode = 0;
static uint8_t SPO2_QU_Thre = 0;
static uint8_t SPO2_ST_Thre = 0;
//spo2_alg_data_t _spo2_alg_data={0};
//spo2_gesture_recognize_t spo2_gesture_status = {1,0,0};

uint8_t MOTION_STATUS = 0;
static spo2_switch_t spo2_switch = {1, 1};
/* USD add hr_spo2 bring up by longtao.ran 20241129 start */
static spo2_usuallyadjust_t spo2_usuallyadjust = {30, 40, 50, 123000, 50000, 100, 300000};
static spo2_barelychange_t spo2_barelychange = {32, 100000, 80000, 1500, 150, 500, 200, 500, 1, 28, 3};
/* USD add hr_spo2 bring up by longtao.ran 20241129 end */

void tyhx_spo2_set_living(uint8_t living_mode, uint8_t qu_thre, uint8_t st_thre)
{
	SPO2_Check_Living_Mode = living_mode;
	SPO2_QU_Thre = qu_thre;
	SPO2_ST_Thre = st_thre;
}
void tyhx_spo2_set_mode(spo2_mode_t mode)
{
	spo2_mode = mode;
}
void tyhx_spo2_set_switch(uint8_t XCORR_MODE, uint8_t QUICK_RESULT)
{
	spo2_switch.XCORR_MODE = XCORR_MODE;
	spo2_switch.QUICK_RESULT = QUICK_RESULT;
}
/* USD add hr_spo2 bring up by longtao.ran 20241129 start */
void tyhx_spo2_para_usuallyadjust(uint8_t SPO2_LOW_XCORR_THRE, uint8_t SPO2_LOW_SNR_THRE, uint8_t COUNT_BLOCK_NUM, uint32_t SPO2_BASE_LINE_INIT, uint32_t SPO2_SLOPE, uint16_t SPO2_GSEN_POW_THRE, uint32_t AMB_THRE)
{
	spo2_usuallyadjust.SPO2_LOW_XCORR_THRE = SPO2_LOW_XCORR_THRE;
	spo2_usuallyadjust.SPO2_LOW_SNR_THRE = SPO2_LOW_SNR_THRE;
	spo2_usuallyadjust.COUNT_BLOCK_NUM = COUNT_BLOCK_NUM;
	spo2_usuallyadjust.SPO2_BASE_LINE_INIT = SPO2_BASE_LINE_INIT;
	spo2_usuallyadjust.SPO2_SLOPE = SPO2_SLOPE;
	spo2_usuallyadjust.SPO2_GSEN_POW_THRE = SPO2_GSEN_POW_THRE;
	spo2_usuallyadjust.AMB_THRE = AMB_THRE;
}
/* USD add hr_spo2 bring up by longtao.ran 20241129 end */
void tyhx_spo2_para_barelychange(uint16_t MEAN_NUM, int32_t SOP2_DEGLITCH_THRE, int32_t SPO2_REMOVE_JUMP_THRE, \
								 uint16_t SPO2_LOW_CLIP_END_TIME, uint16_t SPO2_LOW_CLIP_DN, uint16_t SPO2_NORMAL_CLIP_DN, \
								 uint16_t IR_AC_TOUCH_THRE, uint16_t IR_FFT_POW_THRE, uint8_t SPO2_CALI, uint8_t SLOPE_PARA_MAX, uint8_t SLOPE_PARA_MIN)
{
	spo2_barelychange.MEAN_NUM = MEAN_NUM;
	spo2_barelychange.SOP2_DEGLITCH_THRE = SOP2_DEGLITCH_THRE;
	spo2_barelychange.SPO2_REMOVE_JUMP_THRE = SPO2_REMOVE_JUMP_THRE;
	spo2_barelychange.SPO2_LOW_CLIP_END_TIME = SPO2_LOW_CLIP_END_TIME;
	spo2_barelychange.SPO2_LOW_CLIP_DN = SPO2_LOW_CLIP_DN;
	spo2_barelychange.SPO2_NORMAL_CLIP_DN = SPO2_NORMAL_CLIP_DN;
	spo2_barelychange.IR_AC_TOUCH_THRE = IR_AC_TOUCH_THRE;
	spo2_barelychange.IR_FFT_POW_THRE = IR_FFT_POW_THRE;
	spo2_barelychange.SPO2_CALI = SPO2_CALI;
	spo2_barelychange.SLOPE_PARA_MAX = SLOPE_PARA_MAX;
	spo2_barelychange.SLOPE_PARA_MIN = SLOPE_PARA_MIN;
}

uint8_t const spo2_data_black[32] = {85, 86, 86, 87, 87, 88, 88, 89, 89, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 91, 91, 91, 92, 92, 92, 93, 94, 95, 96, 97, 98, 99};
uint8_t const spo2_data_table[32] = {79, 80, 81, 82, 83, 83, 84, 84, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 88, 89, 90, 90, 91, 92, 92, 93, 94, 95, 96, 97, 98, 99};
uint8_t const spo2_data_hqsig[32] = {76, 76, 77, 78, 79, 80, 81, 82, 83, 83, 83, 83, 84, 85, 86, 87, 88, 88, 89, 90, 90, 91, 91, 92, 92, 93, 94, 95, 96, 97, 98, 99};
uint8_t const spo2_sin_table[256] = {0, 1, 3, 4, 6, 7, 9, 10, 12, 14, 15, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 32, 34, 36, 37, 39,
									 40, 42, 43, 45, 46, 48, 49, 51, 53, 54, 56, 57, 59, 60, 62, 63, 65, 66, 68, 69, 71, 72, 74, 75,
									 77, 78, 80, 81, 83, 84, 86, 87, 89, 90, 92, 93, 95, 96, 97, 99, 100, 102, 103, 105, 106, 108,
									 109, 110, 112, 113, 115, 116, 117, 119, 120, 122, 123, 124, 126, 127, 128, 130, 131, 132,
									 134, 135, 136, 138, 139, 140, 142, 143, 144, 146, 147, 148, 149, 151, 152, 153, 155, 156,
									 157, 158, 159, 161, 162, 163, 164, 166, 167, 168, 169, 170, 171, 173, 174, 175, 176, 177,
									 178, 179, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196,
									 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 211, 212, 213,
									 214, 215, 216, 217, 217, 218, 219, 220, 221, 221, 222, 223, 224, 225, 225, 226, 227, 227,
									 228, 229, 230, 230, 231, 232, 232, 233, 234, 234, 235, 235, 236, 237, 237, 238, 238, 239,
									 239, 240, 241, 241, 242, 242, 243, 243, 244, 244, 244, 245, 245, 246, 246, 247, 247, 247,
									 248, 248, 249, 249, 249, 250, 250, 250, 251, 251, 251, 251, 252, 252, 252, 252, 253, 253,
									 253, 253, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
									 255, 255, 255, 255
									};

void tyhx_sig_snr(int32_t fft_out[110][2], uint16_t *fft_snr, uint8_t *fft_hrm)
{
	uint16_t ii = 0, jj = 0, kk = 0;
	uint32_t tot_pow2 = 0;
	uint32_t sig_pow2 = 0;
	uint32_t sig2_pow2 = 0;
	uint32_t sig3_pow2 = 0;
	uint8_t  data_move_val = 18;
	uint32_t max_pow2_cali = 0;

	int32_t pre1_pow2 = 1000000;
	int32_t pre2_pow2 = 1000001;
	int32_t this_pow2 = 0;
	int32_t max_pow2 = 0;
	uint16_t  fft_max_index[SPO2_HR_PEAKS_NUM] = {0};
	uint16_t  fft_max_value[SPO2_HR_PEAKS_NUM] = {0};
	uint32_t  fft_max_pow2[SPO2_HR_PEAKS_NUM] = {0};

	for (ii = 0; ii < 110; ii++) {
		this_pow2 = ((fft_out[ii][0]) >> 18) * ((fft_out[ii][0]) >> 18) + ((fft_out[ii][1]) >> 18) * ((fft_out[ii][1]) >> 18);
//        if(spo2_alg_data->data_cnt==200)
//        {
//            DEBUG_PRINTF("%d %d\r\n", ii, this_pow2);
//        }

		if (this_pow2 < pre1_pow2 && pre1_pow2 >= pre2_pow2) {
			for (jj = 0; jj < SPO2_HR_PEAKS_NUM; jj++) {
				if (pre1_pow2 > fft_max_pow2[jj]) {
					if (jj > 0) {
						fft_max_index[jj - 1] = fft_max_index[jj];
						fft_max_pow2[jj - 1] = fft_max_pow2[jj];
						fft_max_value[jj - 1] = fft_max_value[jj];
					}
					fft_max_index[jj] = ii;
					fft_max_pow2[jj] = pre1_pow2;
					fft_max_value[jj] = ((ii + 27) * 750) >> 9;
				}
			}
		}
		pre2_pow2 = pre1_pow2;
		pre1_pow2 = this_pow2;
		if (this_pow2 > max_pow2) {
			max_pow2 = this_pow2;
		}
	}
	*fft_hrm = fft_max_value[5];

//    for(ii=0;ii<6;ii++)
//    {
//        DEBUG_PRINTF("%d %d %d %d %d\r\n" ,spo2_alg_data->data_cnt, ii, fft_max_index[ii], fft_max_pow2[ii], fft_max_value[ii]);
//    }

	max_pow2_cali = fft_max_pow2[5];
	while (max_pow2_cali > 1000000) {
		data_move_val++;
		max_pow2_cali = max_pow2_cali >> 2;
	}

	for (ii = 0; ii < 110; ii++) {
		tot_pow2 = tot_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
				   + (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
	}

	if (fft_max_index[SPO2_HR_PEAKS_NUM - 1] >= 7 && fft_max_index[SPO2_HR_PEAKS_NUM - 1] <= 102) {
		for (ii = fft_max_index[SPO2_HR_PEAKS_NUM - 1] - 7; ii <= fft_max_index[SPO2_HR_PEAKS_NUM - 1] + 7; ii++) {
			sig_pow2 = sig_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
					   + (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
		}
	} else if (fft_max_index[SPO2_HR_PEAKS_NUM - 1] < 7) {
		for (ii = 0; ii <= fft_max_index[SPO2_HR_PEAKS_NUM - 1] + 7; ii++) {
			sig_pow2 = sig_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
					   + (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
		}
	} else if (fft_max_index[SPO2_HR_PEAKS_NUM - 1] > 102) {
		for (ii = fft_max_index[SPO2_HR_PEAKS_NUM - 1] - 7; ii < 110; ii++) {
			sig_pow2 = sig_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
					   + (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
		}
	}

	for (kk = 0; kk < SPO2_HR_PEAKS_NUM - 1; kk++) {
		if ((abs(((fft_max_index[SPO2_HR_PEAKS_NUM - 1] + 27) * 750 >> 9) * 2 - ((fft_max_index[kk] + 27) * 750 >> 9)) <= 3  \
			 || abs(((fft_max_index[kk] + 27) * 750 >> 9) * 2 - ((fft_max_index[SPO2_HR_PEAKS_NUM - 1] + 27) * 750 >> 9)) <= 3)  \
			&& fft_max_pow2[SPO2_HR_PEAKS_NUM - 1] > fft_max_pow2[kk] * 2 && fft_max_index[kk] > 0) {
			sig2_pow2 = 0;
			if (fft_max_index[kk] >= 7 && fft_max_index[kk] <= 102) {
				for (ii = fft_max_index[kk] - 7; ii <= fft_max_index[kk] + 7; ii++) {
					sig2_pow2 = sig2_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
								+ (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
				}
			} else if (fft_max_index[kk] < 7) {
				for (ii = 0; ii <= fft_max_index[kk] + 7; ii++) {
					sig2_pow2 = sig2_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
								+ (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
				}
			} else if (fft_max_index[kk] > 102) {
				for (ii = fft_max_index[kk] - 7; ii < 110; ii++) {
					sig2_pow2 = sig2_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
								+ (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
				}
			}
		}
		if (abs(abs(fft_max_index[SPO2_HR_PEAKS_NUM - 1] - fft_max_index[kk]) - 10) <= 1  \
			&& fft_max_pow2[SPO2_HR_PEAKS_NUM - 1] > fft_max_pow2[kk] * 2 && fft_max_index[kk] > 0) {
			if (fft_max_index[kk] >= 3 && fft_max_index[kk] <= 106) {
				for (ii = fft_max_index[kk] - 3; ii <= fft_max_index[kk] + 3; ii++) {
					sig3_pow2 = sig3_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
								+ (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
				}
			} else if (fft_max_index[kk] < 3) {
				for (ii = 0; ii <= fft_max_index[kk] + 3; ii++) {
					sig3_pow2 = sig3_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
								+ (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
				}
			} else if (fft_max_index[kk] > 106) {
				for (ii = fft_max_index[kk] - 3; ii < 110; ii++) {
					sig3_pow2 = sig3_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
								+ (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
				}
			}
		}
	}

	if ((abs(((fft_max_index[SPO2_HR_PEAKS_NUM - 1] + 27) * 750 >> 9) * 2 - ((fft_max_index[SPO2_HR_PEAKS_NUM - 2] + 27) * 750 >> 9)) <= 4  \
		 || abs(((fft_max_index[SPO2_HR_PEAKS_NUM - 2] + 27) * 750 >> 9) * 2 - ((fft_max_index[SPO2_HR_PEAKS_NUM - 1] + 27) * 750 >> 9)) <= 4)  \
		&& sig2_pow2 == 0) {
		sig2_pow2 = 0;
		if (fft_max_index[SPO2_HR_PEAKS_NUM - 2] >= 7 && fft_max_index[SPO2_HR_PEAKS_NUM - 2] <= 102) {
			for (ii = fft_max_index[SPO2_HR_PEAKS_NUM - 2] - 7; ii <= fft_max_index[SPO2_HR_PEAKS_NUM - 2] + 7; ii++) {
				sig2_pow2 = sig2_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
							+ (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
			}
		} else if (fft_max_index[SPO2_HR_PEAKS_NUM - 2] < 7) {
			for (ii = 0; ii <= fft_max_index[SPO2_HR_PEAKS_NUM - 2] + 7; ii++) {
				sig2_pow2 = sig2_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
							+ (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
			}
		} else if (fft_max_index[SPO2_HR_PEAKS_NUM - 2] > 102) {
			for (ii = fft_max_index[SPO2_HR_PEAKS_NUM - 2] - 7; ii < 110; ii++) {
				sig2_pow2 = sig2_pow2 + (fft_out[ii][0] >> data_move_val) * (fft_out[ii][0] >> data_move_val)  \
							+ (fft_out[ii][1] >> data_move_val) * (fft_out[ii][1] >> data_move_val);
			}
		}
	}

	if (tot_pow2 > sig_pow2 + sig2_pow2 + sig3_pow2 + 64) {
		*fft_snr = (sig_pow2 + sig2_pow2 + sig3_pow2) / ((tot_pow2 - sig_pow2 - sig2_pow2 - sig3_pow2) >> 3);
		if (*fft_snr > 100) {
			*fft_snr = 100;
		}
	} else {
		*fft_snr = 100;
	}
	if (*fft_snr == 0) {
		*fft_snr = 1;
	}

//    if(spo2_alg_data->data_cnt==625)
//    {
//        DEBUG_PRINTF("%d %d %d  %d %d\r\n" ,tot_pow2,sig_pow2,sig2_pow2,*fft_snr,data_move_val);
//    }
}
//snr end

bool tyhx_spo2_pedometer_init(void)
{
	uint16_t ii;

	for (ii = 0; ii < 5; ii++) {
		spo2_alg_data->tyhx_pedometer.pow_all_fifo[ii] = 0;
	}
	spo2_alg_data->tyhx_pedometer.gsen_powpeak = 0;
	spo2_alg_data->tyhx_pedometer.pow_all_temp = 0;
	spo2_alg_data->tyhx_pedometer.gsen_data_cnt = 0;
	spo2_alg_data->tyhx_pedometer.motion_status = 0;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir1_dly = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir1_dly2 = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec1_dly = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec1_dly2 = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec1_out = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir2_dly = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir2_dly2 = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec2_dly = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec2_dly2 = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.filter_out = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir1_dly = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir1_dly2 = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec1_dly = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec1_dly2 = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec1_out = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir2_dly = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir2_dly2 = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec2_dly = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec2_dly2 = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.filter_out = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir1_dly = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir1_dly2 = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec1_dly = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec1_dly2 = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec1_out = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir2_dly = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir2_dly2 = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec2_dly = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec2_dly2 = 0;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.filter_out = 0;
	return true;
}

bool tyhx_spo2_checkliving_init(void)
{
	uint8_t ii = 0;

	for (ii = 0; ii < 16; ii++) {
		spo2_alg_data->checkliving_data.check_living_green[ii] = 0;
		spo2_alg_data->checkliving_data.check_living_ir[ii] = 0;
	}
	for (ii = 0; ii < 5; ii++) {
		spo2_alg_data->checkliving_data.check_living_green_std_fifo[ii] = 0;
		spo2_alg_data->checkliving_data.check_living_ir_std_fifo[ii] = 0;
		spo2_alg_data->checkliving_data.check_living_snr_value_fifo[ii] = 0;
	}
	spo2_alg_data->checkliving_data.check_living_green_std = 0;
	spo2_alg_data->checkliving_data.check_living_green_std_avg = 0;
	spo2_alg_data->checkliving_data.check_living_ir_std = 0;
	spo2_alg_data->checkliving_data.check_living_ir_std_avg = 0;
	spo2_alg_data->checkliving_data.check_living_snr_avg = 0;
	spo2_alg_data->checkliving_data.checkliving_close = 0;

	return true;
}

uint8_t tyhx_gsen_motion(int16_t gsen_data_x, int16_t gsen_data_y, int16_t gsen_data_z)
{
	uint16_t ii;
	int16_t gsen_data_flt_x = 0;
	int16_t gsen_data_flt_y = 0;
	int16_t gsen_data_flt_z = 0;
	int32_t gsen_data_pow = 0;
	int16_t g_fir1_x = 0;
	int16_t g_fir2_x = 0;
	int16_t g_fir1_y = 0;
	int16_t g_fir2_y = 0;
	int16_t g_fir1_z = 0;
	int16_t g_fir2_z = 0;

	spo2_alg_data->tyhx_pedometer.gsen_data_cnt++;
	if (spo2_alg_data->tyhx_pedometer.gsen_data_cnt == 100000) {
		spo2_alg_data->tyhx_pedometer.gsen_data_cnt = 0;
	}

	if (spo2_alg_data->tyhx_pedometer.gsen_data_cnt == 1) {
		spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir1_dly = gsen_data_x;
		spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir1_dly2 = gsen_data_x;
		spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir1_dly = gsen_data_y;
		spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir1_dly2 = gsen_data_y;
		spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir1_dly = gsen_data_z;
		spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir1_dly2 = gsen_data_z;
		return  spo2_alg_data->tyhx_pedometer.motion_status;
	}

	g_fir1_x = gsen_data_x * SPO2_GSEN_FIR1_B1 + (spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir1_dly) * SPO2_GSEN_FIR1_B2 \
			   + (spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir1_dly2) * SPO2_GSEN_FIR1_B3;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir1_dly2 = spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir1_dly;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir1_dly  = gsen_data_x;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec1_out = g_fir1_x - ((SPO2_GSEN_IIR1_A2 * (spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec1_dly) + \
			SPO2_GSEN_IIR1_A3 * (spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec1_dly2)) >> 6);

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec1_dly2 = spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec1_dly;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec1_dly = spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec1_out;

	g_fir2_x = spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec1_out * SPO2_GSEN_FIR2_B1 + \
			   (spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir2_dly) * SPO2_GSEN_FIR2_B2 + \
			   (spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir2_dly2) * SPO2_GSEN_FIR2_B3;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir2_dly2 = spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir2_dly;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.fir2_dly = spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec1_out;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.filter_out = g_fir2_x - ((SPO2_GSEN_IIR2_A2 * (spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec2_dly) + \
			SPO2_GSEN_IIR2_A3 * (spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec2_dly2)) >> 6);

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec2_dly2 = spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec2_dly;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.sec2_dly = spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.filter_out;

	g_fir1_y = gsen_data_y * SPO2_GSEN_FIR1_B1 + (spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir1_dly) * SPO2_GSEN_FIR1_B2 \
			   + (spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir1_dly2) * SPO2_GSEN_FIR1_B3;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir1_dly2 = spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir1_dly;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir1_dly  = gsen_data_y;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec1_out = g_fir1_y - ((SPO2_GSEN_IIR1_A2 * (spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec1_dly) + \
			SPO2_GSEN_IIR1_A3 * (spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec1_dly2)) >> 6);

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec1_dly2 = spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec1_dly;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec1_dly = spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec1_out;

	g_fir2_y = spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec1_out * SPO2_GSEN_FIR2_B1 + \
			   (spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir2_dly) * SPO2_GSEN_FIR2_B2 + \
			   (spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir2_dly2) * SPO2_GSEN_FIR2_B3;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir2_dly2 = spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir2_dly;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.fir2_dly = spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec1_out;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.filter_out = g_fir2_y - ((SPO2_GSEN_IIR2_A2 * (spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec2_dly) + \
			SPO2_GSEN_IIR2_A3 * (spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec2_dly2)) >> 6);

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec2_dly2 = spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec2_dly;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.sec2_dly = spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.filter_out;


	g_fir1_z = gsen_data_z * SPO2_GSEN_FIR1_B1 + (spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir1_dly) * SPO2_GSEN_FIR1_B2 \
			   + (spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir1_dly2) * SPO2_GSEN_FIR1_B3;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir1_dly2 = spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir1_dly;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir1_dly  = gsen_data_z;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec1_out = g_fir1_z - ((SPO2_GSEN_IIR1_A2 * (spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec1_dly) + \
			SPO2_GSEN_IIR1_A3 * (spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec1_dly2)) >> 6);

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec1_dly2 = spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec1_dly;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec1_dly = spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec1_out;

	g_fir2_z = spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec1_out * SPO2_GSEN_FIR2_B1 + \
			   (spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir2_dly) * SPO2_GSEN_FIR2_B2 + \
			   (spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir2_dly2) * SPO2_GSEN_FIR2_B3;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir2_dly2 = spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir2_dly;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.fir2_dly = spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec1_out;

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.filter_out = g_fir2_z - ((SPO2_GSEN_IIR2_A2 * (spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec2_dly) + \
			SPO2_GSEN_IIR2_A3 * (spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec2_dly2)) >> 6);

	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec2_dly2 = spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec2_dly;
	spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.sec2_dly = spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.filter_out;

	gsen_data_flt_x = spo2_alg_data->tyhx_pedometer.iir_data_gsen_x.filter_out;
	gsen_data_flt_y = spo2_alg_data->tyhx_pedometer.iir_data_gsen_y.filter_out;
	gsen_data_flt_z = spo2_alg_data->tyhx_pedometer.iir_data_gsen_z.filter_out;

#ifdef NO_MATH_LIB
	gsen_data_pow = hr_sqrt(gsen_data_flt_x * gsen_data_flt_x + gsen_data_flt_y * gsen_data_flt_y \
							+ gsen_data_flt_z * gsen_data_flt_z);
#else
	gsen_data_pow = sqrt(gsen_data_flt_x * gsen_data_flt_x + gsen_data_flt_y * gsen_data_flt_y \
						 + gsen_data_flt_z * gsen_data_flt_z);
#endif

	for (ii = 4; ii > 0; ii--) {
		spo2_alg_data->tyhx_pedometer.pow_all_fifo[ii] = spo2_alg_data->tyhx_pedometer.pow_all_fifo[ii - 1];
	}
	spo2_alg_data->tyhx_pedometer.pow_all_fifo[0] = gsen_data_pow;

	if (spo2_alg_data->tyhx_pedometer.pow_all_fifo[2] > spo2_alg_data->tyhx_pedometer.pow_all_fifo[4] && \
		spo2_alg_data->tyhx_pedometer.pow_all_fifo[2] > spo2_alg_data->tyhx_pedometer.pow_all_fifo[3] && \
		spo2_alg_data->tyhx_pedometer.pow_all_fifo[2] > spo2_alg_data->tyhx_pedometer.pow_all_fifo[1] && \
		spo2_alg_data->tyhx_pedometer.pow_all_fifo[2] > spo2_alg_data->tyhx_pedometer.pow_all_fifo[0]) {
		spo2_alg_data->tyhx_pedometer.gsen_powpeak = spo2_alg_data->tyhx_pedometer.pow_all_fifo[2];
	} else if (spo2_alg_data->tyhx_pedometer.pow_all_fifo[2] > spo2_alg_data->tyhx_pedometer.pow_all_fifo[4] && \
			   spo2_alg_data->tyhx_pedometer.pow_all_fifo[2] > spo2_alg_data->tyhx_pedometer.pow_all_fifo[3] && \
			   spo2_alg_data->tyhx_pedometer.pow_all_fifo[2] > spo2_alg_data->tyhx_pedometer.pow_all_fifo[1] && \
			   spo2_alg_data->tyhx_pedometer.pow_all_fifo[2] > spo2_alg_data->tyhx_pedometer.pow_all_fifo[0] && \
			   spo2_alg_data->tyhx_pedometer.pow_all_fifo[2] <= 20) {
		spo2_alg_data->tyhx_pedometer.gsen_powpeak = spo2_alg_data->tyhx_pedometer.pow_all_fifo[2];
	}
	if (spo2_alg_data->tyhx_pedometer.gsen_data_cnt >= 50) {
		if (spo2_alg_data->tyhx_pedometer.gsen_powpeak > 500) {
			spo2_alg_data->tyhx_pedometer.pow_all_temp = (spo2_alg_data->tyhx_pedometer.pow_all_temp * 31 + spo2_alg_data->tyhx_pedometer.gsen_powpeak) >> 5;
		} else {
			spo2_alg_data->tyhx_pedometer.pow_all_temp = (spo2_alg_data->tyhx_pedometer.pow_all_temp * 7 + spo2_alg_data->tyhx_pedometer.gsen_powpeak) >> 3;
		}
	}

	if (spo2_alg_data->tyhx_pedometer.pow_all_temp < spo2_usuallyadjust.SPO2_GSEN_POW_THRE) { //spo2_usuallyadjust.SPO2_GSEN_POW_THRE
		spo2_alg_data->tyhx_pedometer.motion_status = 0;
	} else if (spo2_alg_data->tyhx_pedometer.pow_all_temp >= spo2_usuallyadjust.SPO2_GSEN_POW_THRE && spo2_alg_data->tyhx_pedometer.pow_all_temp < 500) { //spo2_usuallyadjust.SPO2_GSEN_POW_THRE
		spo2_alg_data->tyhx_pedometer.motion_status = 1;
	} else if (spo2_alg_data->tyhx_pedometer.pow_all_temp >= 500 && spo2_alg_data->tyhx_pedometer.pow_all_temp < 1000) {
		spo2_alg_data->tyhx_pedometer.motion_status = 2;
	} else if (spo2_alg_data->tyhx_pedometer.pow_all_temp >= 1000) {
		spo2_alg_data->tyhx_pedometer.motion_status = 3;
	}
	//DEBUG_PRINTF("%d %d %d  %d  %d  %d\r\n",spo2_alg_data->data_cnt,gsen_data_x,gsen_data_y,gsen_data_z,spo2_alg_data->tyhx_pedometer.pow_all_temp,spo2_alg_data->tyhx_pedometer.motion_status);
	return  spo2_alg_data->tyhx_pedometer.motion_status;
}

/* USD add hr_spo2 fix ke by longtao.ran 20241017 start */
void tyhx_spo2_alg_close(void)
{

}
/* USD add hr_spo2 fix ke by longtao.ran 20241017 end */

bool tyhx_spo2_alg_open(void)
{
	uint16_t ii;
	spo2_alg_data = (spo2_alg_data_t *)alg_ram;
//    spo2_alg_data = &_spo2_alg_data;
	spo2_alg_status = MSG_SPO2_ALG_OPEN;
	spo2_living_status = MSG_SPO2_LIVING_INITIAL;

	spo2_alg_data->data_cnt = 0;
	spo2_alg_data->spo2_result = 0;
	spo2_alg_data->hr_result = 0;
	spo2_alg_data->gsen_z_smooth = 3000;;

	spo2_alg_data->ir_iir_data.fir1_dly = 0;
	spo2_alg_data->ir_iir_data.fir1_dly2 = 0;
	spo2_alg_data->ir_iir_data.sec1_dly = 0;
	spo2_alg_data->ir_iir_data.sec1_dly2 = 0;
	spo2_alg_data->ir_iir_data.sec1_out = 0;
	spo2_alg_data->ir_iir_data.fir2_dly = 0;
	spo2_alg_data->ir_iir_data.fir2_dly2 = 0;
	spo2_alg_data->ir_iir_data.sec2_dly = 0;
	spo2_alg_data->ir_iir_data.sec2_dly2 = 0;
	spo2_alg_data->ir_iir_data.filter_out = 0;

	spo2_alg_data->red_iir_data.fir1_dly = 0;
	spo2_alg_data->red_iir_data.fir1_dly2 = 0;
	spo2_alg_data->red_iir_data.sec1_dly = 0;
	spo2_alg_data->red_iir_data.sec1_dly2 = 0;
	spo2_alg_data->red_iir_data.sec1_out = 0;
	spo2_alg_data->red_iir_data.fir2_dly = 0;
	spo2_alg_data->red_iir_data.fir2_dly2 = 0;
	spo2_alg_data->red_iir_data.sec2_dly = 0;
	spo2_alg_data->red_iir_data.sec2_dly2 = 0;
	spo2_alg_data->red_iir_data.filter_out = 0;

	spo2_alg_data->green_iir_data.fir1_dly = 0;
	spo2_alg_data->green_iir_data.fir1_dly2 = 0;
	spo2_alg_data->green_iir_data.sec1_dly = 0;
	spo2_alg_data->green_iir_data.sec1_dly2 = 0;
	spo2_alg_data->green_iir_data.sec1_out = 0;
	spo2_alg_data->green_iir_data.fir2_dly = 0;
	spo2_alg_data->green_iir_data.fir2_dly2 = 0;
	spo2_alg_data->green_iir_data.sec2_dly = 0;
	spo2_alg_data->green_iir_data.sec2_dly2 = 0;
	spo2_alg_data->green_iir_data.filter_out = 0;

	spo2_alg_data->red0204_iir_data.fir1_dly = 0;
	spo2_alg_data->red0204_iir_data.fir1_dly2 = 0;
	spo2_alg_data->red0204_iir_data.sec1_dly = 0;
	spo2_alg_data->red0204_iir_data.sec1_dly2 = 0;
	spo2_alg_data->red0204_iir_data.sec1_out = 0;
	spo2_alg_data->red0204_iir_data.fir2_dly = 0;
	spo2_alg_data->red0204_iir_data.fir2_dly2 = 0;
	spo2_alg_data->red0204_iir_data.sec2_dly = 0;
	spo2_alg_data->red0204_iir_data.sec2_dly2 = 0;
	spo2_alg_data->red0204_iir_data.filter_out = 0;

	for (ii = 0; ii < 7; ii++) {
		spo2_alg_data->ir_fifo_filter[ii] = 0;
		spo2_alg_data->red_fifo_filter[ii] = 0;
		spo2_alg_data->green_fifo_filter[ii] = 0;
		spo2_alg_data->red0204_fifo_filter[ii] = 0;
	}
	for (ii = 0; ii < 8; ii++) {
		spo2_alg_data->ir_peak_fifo[ii] = 0;
		spo2_alg_data->red_peak_fifo[ii] = 0;
		spo2_alg_data->green_peak_fifo[ii] = 0;
		spo2_alg_data->ir_valley_fifo[ii] = 0;
		spo2_alg_data->red_valley_fifo[ii] = 0;
		spo2_alg_data->green_valley_fifo[ii] = 0;
	}
	spo2_alg_data->ir_peak_ave = 0;
	spo2_alg_data->red_peak_ave = 0;
	spo2_alg_data->green_peak_ave = 0;
	spo2_alg_data->red0204_peak_ave = 0;
	spo2_alg_data->ir_valley_ave = 0;
	spo2_alg_data->red_valley_ave = 0;
	spo2_alg_data->green_valley_ave = 0;
	spo2_alg_data->red0204_valley_ave = 0;

	spo2_alg_data->ir_data_dc = 0;
	spo2_alg_data->ir_data_ac = 0;
	spo2_alg_data->red_data_dc = 0;
	spo2_alg_data->red_data_ac = 0;
	spo2_alg_data->green_data_dc = 0;
	spo2_alg_data->green_data_ac = 0;
	spo2_alg_data->red0204_data_ac = 0;
	spo2_alg_data->ir_iir_data_smooth = 0;
	spo2_alg_data->red_iir_data_smooth = 0;
	spo2_alg_data->green_iir_data_smooth = 0;
	spo2_alg_data->red0204_iir_data_smooth = 0;
	spo2_alg_data->baseline = spo2_usuallyadjust.SPO2_BASE_LINE_INIT;
	spo2_alg_data->spo2_sa = 0;
	spo2_alg_data->spo2_sa_final = spo2_usuallyadjust.SPO2_BASE_LINE_INIT - 103000 + 3000;
	spo2_alg_data->spo2_sa_temp = 0;
	spo2_alg_data->spo2_gi_temp = 0;

	for (ii = 0; ii < 110; ii++) {
		spo2_alg_data->fft_phase[ii] = 0;
		spo2_alg_data->fft_phase_start[ii] = 0;
		spo2_alg_data->ir_fft_out[ii][0] = 0;
		spo2_alg_data->ir_fft_out[ii][1] = 0;
		spo2_alg_data->red_fft_out[ii][0] = 0;
		spo2_alg_data->red_fft_out[ii][1] = 0;
		spo2_alg_data->green_fft_out[ii][0] = 0;
		spo2_alg_data->green_fft_out[ii][1] = 0;
	}
	for (ii = 0; ii < 150; ii++) {
		spo2_alg_data->ir_flt_data_fifo[ii] = 0;
		spo2_alg_data->red_flt_data_fifo[ii] = 0;
		spo2_alg_data->green_flt_data_fifo[ii] = 0;
	}

	spo2_alg_data->red_offset_50 = 0;
	spo2_alg_data->ir_offset_50 = 0;
	spo2_alg_data->green_offset_50 = 0;

	spo2_alg_data->hr = 0;
	spo2_alg_data->hr_pre = 0;
	spo2_alg_data->green_hr = 0;
	spo2_alg_data->ir_hr = 0;
	spo2_alg_data->red_hr = 0;

	spo2_alg_data->start_ind = 0;
	spo2_alg_data->green_start_ind = 0;
	spo2_alg_data->ir_fft_snr = 0;
	spo2_alg_data->red_fft_snr = 0;
	spo2_alg_data->green_fft_snr = 0;
	spo2_alg_data->count_block = 0;
	spo2_alg_data->snr_block = 0;

	spo2_alg_data->red_data_pre = 0;
	spo2_alg_data->ir_data_pre = 0;
	spo2_alg_data->green_data_pre = 0;
	spo2_alg_data->red_jump_delta = 0;
	spo2_alg_data->ir_jump_delta = 0;
	spo2_alg_data->green_jump_delta = 0;
	spo2_alg_data->red_data_cali = 0;
	spo2_alg_data->ir_data_cali = 0;
	spo2_alg_data->green_data_cali = 0;
	for (ii = 0; ii < 4; ii++) {
		spo2_alg_data->red_data_fifo[ii] = 0;
		spo2_alg_data->ir_data_fifo[ii] = 0;
		spo2_alg_data->green_data_fifo[ii] = 0;
	}
	for (ii = 0; ii < 6; ii++) {
		spo2_alg_data->signal_quality_fifo[ii] = 20;
	}
	spo2_alg_data->rand_cnt = 0;
	spo2_alg_data->rand_cnt1 = 0;
	spo2_alg_data->spo2_rand = 0;
	spo2_alg_data->spo2_rand1 = 0;

	spo2_alg_data->motion = 0;
	spo2_alg_data->poor_signal = 20;
	spo2_alg_data->screen_up = 1;
	spo2_alg_data->ir_baseline = 0;
	spo2_alg_data->spo2_result_flag = 0;

	spo2_alg_data->hq_sig_cnt = 0;
	spo2_alg_data->hq_sig_flag = 0;
	spo2_alg_data->spo2_result_min = 100;
	spo2_alg_data->red_snr_sum = 0;
	spo2_alg_data->red_snr_avg = 0;
	spo2_alg_data->lq_sig_flag = 0;

	spo2_alg_data->motion_err_cnt = 0;
	spo2_alg_data->screenup_err_cnt = 0;
	spo2_alg_data->signal_err_cnt = 0;
	spo2_alg_data->user_err_cnt = 0;
	spo2_alg_data->user_err = 0;
//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 start
    spo2_alg_data->mic_move_cnt = 0;
//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 end
	tyhx_spo2_pedometer_init();
	tyhx_spo2_checkliving_init();

	return true;
}
bool tyhx_spo2_alg_open_deep(void)
{

	tyhx_spo2_alg_open();
	return true;
}

bool tyhx_gesture_recognize(int16_t gsen_data_x, int16_t gsen_data_y, int16_t gsen_data_z)
{
	int16_t gsen_data_x_cali = 0;
	int16_t gsen_data_y_cali = 0;
	int16_t gsen_data_z_cali = 0;

	gsen_data_x_cali = gsen_data_x >> 4;
	gsen_data_y_cali = gsen_data_y >> 4;
	gsen_data_z_cali = gsen_data_z >> 4;

	spo2_alg_data->motion = tyhx_gsen_motion(gsen_data_x_cali, gsen_data_y_cali, gsen_data_z_cali);
	spo2_alg_data->gsen_z_smooth = (spo2_alg_data->gsen_z_smooth * 7 + gsen_data_z) >> 3;


	if (spo2_alg_data->data_cnt > 25 && spo2_alg_data->gsen_z_smooth < 2500) {
		spo2_alg_data->screen_up = 0;
	} else {
		spo2_alg_data->screen_up = 1;
	}

	if (spo2_alg_data->motion > 1 && spo2_alg_data->motion_err_cnt < 255) {
		spo2_alg_data->motion_err_cnt++;
	}
	if (spo2_alg_data->screen_up == 0 && spo2_alg_data->screenup_err_cnt < 255) {
		spo2_alg_data->screenup_err_cnt++;
	}
	if (spo2_alg_data->poor_signal < 16 && spo2_alg_data->signal_err_cnt < 255) {
		spo2_alg_data->signal_err_cnt++;
	}
	if (spo2_alg_data->motion > 1 || spo2_alg_data->screen_up == 0 || spo2_alg_data->poor_signal < 16) {
		if (spo2_alg_data->user_err_cnt < 255) {
			spo2_alg_data->user_err_cnt++;
		}
		if (spo2_alg_data->user_err_cnt >= 200) {
			if (spo2_alg_data->motion_err_cnt > 100) {
				spo2_alg_data->user_err = USER_MOTION_ERR;
			} else if (spo2_alg_data->screenup_err_cnt > 100) {
				spo2_alg_data->user_err = USER_SCREENUP_ERR;
			} else if (spo2_alg_data->signal_err_cnt > 100) {
				spo2_alg_data->user_err = USER_SIGNAL_ERR;
			} else {
				spo2_alg_data->user_err = USER_MOTION_ERR;
			}
		} else if (spo2_alg_data->motion_err_cnt > 125) {
			spo2_alg_data->user_err = USER_MOTION_ERR;
		} else if (spo2_alg_data->screenup_err_cnt > 150) {
			spo2_alg_data->user_err = USER_SCREENUP_ERR;
		}
	}

	return true;
}

int32_t spo2_sort_max(int32_t *data, uint8_t length)
{
	uint8_t ii, jj;
	int32_t data_temp = 0;
	int32_t data_n[7];

	for (ii = 0; ii < length; ii++) {
		data_n[ii] = data[ii];
		//DEBUG_PRINTF("%d %d %d \r\n", spo2_alg_data->data_cnt, ii, data[ii]);
	}
	for (ii = 0; ii < length - 1; ii++) {
		for (jj = 0; jj < length - 1 - ii; jj++) {
			if (data_n[jj] >= data_n[jj + 1]) {
				data_temp = data_n[jj];
				data_n[jj] = data_n[jj + 1];
				data_n[jj + 1] = data_temp;
			}
		}
	}
//    for(ii=0;ii<length;ii++)
//    {
//        DEBUG_PRINTF("%d %d %d \r\n", spo2_alg_data->data_cnt, ii, data_n[ii]);
//    }
	return data_n[length - 1];
}

int32_t spo2_sort_min(int32_t *data, uint8_t length)
{
	uint8_t ii, jj;
	int32_t data_temp = 0;
	int32_t data_n[7];

	for (ii = 0; ii < 7; ii++) {
		data_n[ii] = data[ii];
	}
	for (ii = 0; ii < length - 1; ii++) {
		for (jj = 0; jj < length - 1 - ii; jj++) {
			if (data_n[jj] >= data_n[jj + 1]) {
				data_temp = data_n[jj];
				data_n[jj] = data_n[jj + 1];
				data_n[jj + 1] = data_temp;
			}
		}
	}
//    for(ii=0;ii<length;ii++)
//    {
//        DEBUG_PRINTF("%d %d %d \r\n", spo2_alg_data->data_cnt, ii, data_n[ii]);
//    }
	return data_n[0];
}

void tyhx_spo2_data_cali(int32_t red_new_raw_data, int32_t ir_new_raw_data, int32_t green_new_raw_data)
{
	uint8_t ii = 0;
	for (ii = 3; ii > 0; ii--) {
		spo2_alg_data->red_data_fifo[ii] = spo2_alg_data->red_data_fifo[ii - 1];
		spo2_alg_data->ir_data_fifo[ii] = spo2_alg_data->ir_data_fifo[ii - 1];
		spo2_alg_data->green_data_fifo[ii] = spo2_alg_data->green_data_fifo[ii - 1];
	}
	spo2_alg_data->red_data_fifo[0] = red_new_raw_data;
	spo2_alg_data->ir_data_fifo[0] = ir_new_raw_data;
	spo2_alg_data->green_data_fifo[0] = green_new_raw_data;
	if (spo2_alg_data->data_cnt > SPO2_CHIP_SETTLE - 2) {
		if ((((spo2_alg_data->red_data_fifo[1] - spo2_alg_data->red_data_fifo[2]) > spo2_barelychange.SOP2_DEGLITCH_THRE) && ((spo2_alg_data->red_data_fifo[1] - red_new_raw_data) > spo2_barelychange.SOP2_DEGLITCH_THRE)) || \
			(((spo2_alg_data->red_data_fifo[1] - spo2_alg_data->red_data_fifo[2]) < -spo2_barelychange.SOP2_DEGLITCH_THRE) && ((spo2_alg_data->red_data_fifo[1] - red_new_raw_data) < -spo2_barelychange.SOP2_DEGLITCH_THRE))) {
			red_new_raw_data = spo2_alg_data->red_data_fifo[2];
			spo2_alg_data->red_data_fifo[1] = spo2_alg_data->red_data_fifo[2];
		} else {
			red_new_raw_data = spo2_alg_data->red_data_fifo[1];
		}

		if ((((spo2_alg_data->ir_data_fifo[1] - spo2_alg_data->ir_data_fifo[2]) > spo2_barelychange.SOP2_DEGLITCH_THRE) && ((spo2_alg_data->ir_data_fifo[1] - ir_new_raw_data) > spo2_barelychange.SOP2_DEGLITCH_THRE)) || \
			(((spo2_alg_data->ir_data_fifo[1] - spo2_alg_data->ir_data_fifo[2]) < -spo2_barelychange.SOP2_DEGLITCH_THRE) && ((spo2_alg_data->ir_data_fifo[1] - ir_new_raw_data) < -spo2_barelychange.SOP2_DEGLITCH_THRE))) {
			ir_new_raw_data = spo2_alg_data->ir_data_fifo[2];
			spo2_alg_data->ir_data_fifo[1] = spo2_alg_data->ir_data_fifo[2];
		} else {
			ir_new_raw_data = spo2_alg_data->ir_data_fifo[1];
		}

		if ((((spo2_alg_data->green_data_fifo[1] - spo2_alg_data->green_data_fifo[2]) > spo2_barelychange.SOP2_DEGLITCH_THRE) && ((spo2_alg_data->green_data_fifo[1] - green_new_raw_data) > spo2_barelychange.SOP2_DEGLITCH_THRE)) || \
			(((spo2_alg_data->green_data_fifo[1] - spo2_alg_data->green_data_fifo[2]) < -spo2_barelychange.SOP2_DEGLITCH_THRE) && ((spo2_alg_data->green_data_fifo[1] - green_new_raw_data) < -spo2_barelychange.SOP2_DEGLITCH_THRE))) {
			green_new_raw_data = spo2_alg_data->green_data_fifo[2];
			spo2_alg_data->green_data_fifo[1] = spo2_alg_data->green_data_fifo[2];
		} else {
			green_new_raw_data = spo2_alg_data->green_data_fifo[1];
		}
		//DEBUG_PRINTF("  data cali: %d   %d %d %d\r\n" ,spo2_alg_data->data_cnt, green_data_fifo[2],green_data_fifo[1],green_new_raw_data);
		if ((abs((red_new_raw_data - spo2_alg_data->red_jump_delta) - spo2_alg_data->red_data_pre) > spo2_barelychange.SPO2_REMOVE_JUMP_THRE)) {
			spo2_alg_data->red_jump_delta = red_new_raw_data - spo2_alg_data->red_data_pre;
		}
		spo2_alg_data->red_data_cali = red_new_raw_data - spo2_alg_data->red_jump_delta;
		spo2_alg_data->red_data_pre = spo2_alg_data->red_data_cali;
		if ((abs((ir_new_raw_data - spo2_alg_data->ir_jump_delta) - spo2_alg_data->ir_data_pre) > spo2_barelychange.SPO2_REMOVE_JUMP_THRE)) {
			spo2_alg_data->ir_jump_delta = ir_new_raw_data - spo2_alg_data->ir_data_pre;
		}
		spo2_alg_data->ir_data_cali = ir_new_raw_data - spo2_alg_data->ir_jump_delta;
		spo2_alg_data->ir_data_pre = spo2_alg_data->ir_data_cali;
		if ((abs((green_new_raw_data - spo2_alg_data->green_jump_delta) - spo2_alg_data->green_data_pre) > spo2_barelychange.SPO2_REMOVE_JUMP_THRE)) {
			spo2_alg_data->green_jump_delta = green_new_raw_data - spo2_alg_data->green_data_pre;
		}
		spo2_alg_data->green_data_cali = green_new_raw_data - spo2_alg_data->green_jump_delta;
		spo2_alg_data->green_data_pre = spo2_alg_data->green_data_cali;
	} else {
		spo2_alg_data->red_data_cali = spo2_alg_data->red_data_fifo[1];
		spo2_alg_data->ir_data_cali = spo2_alg_data->ir_data_fifo[1];
		spo2_alg_data->green_data_cali = spo2_alg_data->green_data_fifo[1];
		spo2_alg_data->red_data_pre = spo2_alg_data->red_data_fifo[1];
		spo2_alg_data->ir_data_pre = spo2_alg_data->ir_data_fifo[1];
		spo2_alg_data->green_data_pre = spo2_alg_data->green_data_fifo[1];
	}
	//DEBUG_PRINTF("%d %d %d %d\r\n", spo2_alg_data->data_cnt, spo2_alg_data->red_data_cali, spo2_alg_data->ir_data_cali, spo2_alg_data->green_data_cali);
}
void tyhx_spo2_flt(void)
{
	int32_t  ir_fir1 = 0, ir_fir2 = 0, red_fir1 = 0, red_fir2 = 0, green_fir1 = 0, green_fir2 = 0;
	int32_t  red0204_fir1 = 0, red0204_fir2 = 0;
	if (spo2_alg_data->data_cnt > SPO2_CHIP_SETTLE) {
		ir_fir1 = spo2_alg_data->ir_data_cali * SPO2_FIR1_B1 + spo2_alg_data->ir_iir_data.fir1_dly * SPO2_FIR1_B2 + \
				  spo2_alg_data->ir_iir_data.fir1_dly2 * SPO2_FIR1_B3;
		spo2_alg_data->ir_iir_data.fir1_dly2 = spo2_alg_data->ir_iir_data.fir1_dly;
		spo2_alg_data->ir_iir_data.fir1_dly  = spo2_alg_data->ir_data_cali;

		spo2_alg_data->ir_iir_data.sec1_out = ir_fir1 - ((SPO2_IIR1_A2 * spo2_alg_data->ir_iir_data.sec1_dly + \
											  SPO2_IIR1_A3 * spo2_alg_data->ir_iir_data.sec1_dly2) >> 6);
		spo2_alg_data->ir_iir_data.sec1_dly2 = spo2_alg_data->ir_iir_data.sec1_dly;
		spo2_alg_data->ir_iir_data.sec1_dly = spo2_alg_data->ir_iir_data.sec1_out;

		ir_fir2 = spo2_alg_data->ir_iir_data.sec1_out * SPO2_FIR2_B1 + spo2_alg_data->ir_iir_data.fir2_dly * SPO2_FIR2_B2 + \
				  spo2_alg_data->ir_iir_data.fir2_dly2 * SPO2_FIR2_B3;
		spo2_alg_data->ir_iir_data.fir2_dly2 = spo2_alg_data->ir_iir_data.fir2_dly;
		spo2_alg_data->ir_iir_data.fir2_dly = spo2_alg_data->ir_iir_data.sec1_out;

		spo2_alg_data->ir_iir_data.filter_out = ir_fir2 - ((SPO2_IIR2_A2 * spo2_alg_data->ir_iir_data.sec2_dly + \
												SPO2_IIR2_A3 * spo2_alg_data->ir_iir_data.sec2_dly2) >> 6);
		spo2_alg_data->ir_iir_data.sec2_dly2 = spo2_alg_data->ir_iir_data.sec2_dly;
		spo2_alg_data->ir_iir_data.sec2_dly = spo2_alg_data->ir_iir_data.filter_out;

		red_fir1 = spo2_alg_data->red_data_cali * SPO2_FIR1_B1 + spo2_alg_data->red_iir_data.fir1_dly * SPO2_FIR1_B2 + \
				   spo2_alg_data->red_iir_data.fir1_dly2 * SPO2_FIR1_B3;
		spo2_alg_data->red_iir_data.fir1_dly2 = spo2_alg_data->red_iir_data.fir1_dly;
		spo2_alg_data->red_iir_data.fir1_dly  = spo2_alg_data->red_data_cali;

		spo2_alg_data->red_iir_data.sec1_out = red_fir1 - ((SPO2_IIR1_A2 * spo2_alg_data->red_iir_data.sec1_dly + \
											   SPO2_IIR1_A3 * spo2_alg_data->red_iir_data.sec1_dly2) >> 6);
		spo2_alg_data->red_iir_data.sec1_dly2 = spo2_alg_data->red_iir_data.sec1_dly;
		spo2_alg_data->red_iir_data.sec1_dly = spo2_alg_data->red_iir_data.sec1_out;

		red_fir2 = spo2_alg_data->red_iir_data.sec1_out * SPO2_FIR2_B1 + spo2_alg_data->red_iir_data.fir2_dly * SPO2_FIR2_B2 + \
				   spo2_alg_data->red_iir_data.fir2_dly2 * SPO2_FIR2_B3;
		spo2_alg_data->red_iir_data.fir2_dly2 = spo2_alg_data->red_iir_data.fir2_dly;
		spo2_alg_data->red_iir_data.fir2_dly = spo2_alg_data->red_iir_data.sec1_out;

		spo2_alg_data->red_iir_data.filter_out = red_fir2 - ((SPO2_IIR2_A2 * spo2_alg_data->red_iir_data.sec2_dly + \
				SPO2_IIR2_A3 * spo2_alg_data->red_iir_data.sec2_dly2) >> 6);
		spo2_alg_data->red_iir_data.sec2_dly2 = spo2_alg_data->red_iir_data.sec2_dly;
		spo2_alg_data->red_iir_data.sec2_dly = spo2_alg_data->red_iir_data.filter_out;

		green_fir1 = spo2_alg_data->green_data_cali * SPO2_FIR1_B1 + spo2_alg_data->green_iir_data.fir1_dly * SPO2_FIR1_B2 + \
					 spo2_alg_data->green_iir_data.fir1_dly2 * SPO2_FIR1_B3;
		spo2_alg_data->green_iir_data.fir1_dly2 = spo2_alg_data->green_iir_data.fir1_dly;
		spo2_alg_data->green_iir_data.fir1_dly  = spo2_alg_data->green_data_cali;

		spo2_alg_data->green_iir_data.sec1_out = green_fir1 - ((SPO2_IIR1_A2 * spo2_alg_data->green_iir_data.sec1_dly + \
				SPO2_IIR1_A3 * spo2_alg_data->green_iir_data.sec1_dly2) >> 6);
		spo2_alg_data->green_iir_data.sec1_dly2 = spo2_alg_data->green_iir_data.sec1_dly;
		spo2_alg_data->green_iir_data.sec1_dly = spo2_alg_data->green_iir_data.sec1_out;

		green_fir2 = spo2_alg_data->green_iir_data.sec1_out * SPO2_FIR2_B1 + spo2_alg_data->green_iir_data.fir2_dly * SPO2_FIR2_B2 + \
					 spo2_alg_data->green_iir_data.fir2_dly2 * SPO2_FIR2_B3;
		spo2_alg_data->green_iir_data.fir2_dly2 = spo2_alg_data->green_iir_data.fir2_dly;
		spo2_alg_data->green_iir_data.fir2_dly = spo2_alg_data->green_iir_data.sec1_out;

		spo2_alg_data->green_iir_data.filter_out = green_fir2 - ((SPO2_IIR2_A2 * spo2_alg_data->green_iir_data.sec2_dly + \
				SPO2_IIR2_A3 * spo2_alg_data->green_iir_data.sec2_dly2) >> 6);
		spo2_alg_data->green_iir_data.sec2_dly2 = spo2_alg_data->green_iir_data.sec2_dly;
		spo2_alg_data->green_iir_data.sec2_dly = spo2_alg_data->green_iir_data.filter_out;

		red0204_fir1 = (spo2_alg_data->red_data_cali >> 8) * SPO2_FIR1_B1_0204 + spo2_alg_data->red0204_iir_data.fir1_dly * SPO2_FIR1_B2_0204 + \
					   spo2_alg_data->red0204_iir_data.fir1_dly2 * SPO2_FIR1_B3_0204;
		spo2_alg_data->red0204_iir_data.fir1_dly2 = spo2_alg_data->red0204_iir_data.fir1_dly;
		spo2_alg_data->red0204_iir_data.fir1_dly  = spo2_alg_data->red_data_cali >> 8;

		spo2_alg_data->red0204_iir_data.sec1_out = red0204_fir1 - ((SPO2_IIR1_A2_0204 * spo2_alg_data->red0204_iir_data.sec1_dly + \
				SPO2_IIR1_A3_0204 * spo2_alg_data->red0204_iir_data.sec1_dly2) >> 8);
		spo2_alg_data->red0204_iir_data.sec1_dly2 = spo2_alg_data->red0204_iir_data.sec1_dly;
		spo2_alg_data->red0204_iir_data.sec1_dly = spo2_alg_data->red0204_iir_data.sec1_out;

		red0204_fir2 = spo2_alg_data->red0204_iir_data.sec1_out * SPO2_FIR2_B1_0204 + spo2_alg_data->red0204_iir_data.fir2_dly * SPO2_FIR2_B2_0204 + \
					   spo2_alg_data->red0204_iir_data.fir2_dly2 * SPO2_FIR2_B3_0204;
		spo2_alg_data->red0204_iir_data.fir2_dly2 = spo2_alg_data->red0204_iir_data.fir2_dly;
		spo2_alg_data->red0204_iir_data.fir2_dly = spo2_alg_data->red0204_iir_data.sec1_out;

		spo2_alg_data->red0204_iir_data.filter_out = red0204_fir2 - ((SPO2_IIR2_A2_0204 * spo2_alg_data->red0204_iir_data.sec2_dly + \
				SPO2_IIR2_A3_0204 * spo2_alg_data->red0204_iir_data.sec2_dly2) >> 8);
		spo2_alg_data->red0204_iir_data.sec2_dly2 = spo2_alg_data->red0204_iir_data.sec2_dly;
		spo2_alg_data->red0204_iir_data.sec2_dly = spo2_alg_data->red0204_iir_data.filter_out;
		//DEBUG_PRINTF("%d %d\r\n", spo2_alg_data->data_cnt, spo2_alg_data->red0204_iir_data.filter_out);
	}
}

void tyhx_spo2_fft(void)
{
	uint8_t ii;
	uint32_t phase_cos_1q = 0, phase_sin_1q = 0;
	int32_t  cos_c = 0, sin_c = 0, temp_c = 0;

	if (spo2_alg_data->data_cnt > SPO2_CHIP_SETTLE + SPO2_FLT_SETTLE) {
		for (ii = 0; ii < 110; ii++) {
			spo2_alg_data->fft_phase[ii] = spo2_alg_data->fft_phase[ii] + 28 + ii;
			if (spo2_alg_data->fft_phase[ii] >= 1024) {
				spo2_alg_data->fft_phase[ii] = spo2_alg_data->fft_phase[ii] - 1024;
			}
			if (spo2_alg_data->fft_phase[ii] >= 768) {
				phase_sin_1q = spo2_alg_data->fft_phase[ii] - 768 ;
			} else if (spo2_alg_data->fft_phase[ii] >= 512) {
				phase_sin_1q = spo2_alg_data->fft_phase[ii] - 512 ;
			} else if (spo2_alg_data->fft_phase[ii] >= 256) {
				phase_sin_1q = spo2_alg_data->fft_phase[ii] - 256 ;
			} else {
				phase_sin_1q = spo2_alg_data->fft_phase[ii] ;
			}
			phase_cos_1q = 255 - phase_sin_1q;
			sin_c = spo2_sin_table[phase_sin_1q];
			cos_c = spo2_sin_table[phase_cos_1q];
			if (spo2_alg_data->fft_phase[ii] < 256) {
				//sin_c = sin_c;
				//cos_c = cos_c;
			} else if (spo2_alg_data->fft_phase[ii] < 512) {
				temp_c = sin_c;
				sin_c = cos_c;
				cos_c = -temp_c;
			} else if (spo2_alg_data->fft_phase[ii] < 768) {
				sin_c = -sin_c;
				cos_c = -cos_c;
			} else {
				temp_c = sin_c;
				sin_c = -cos_c;
				cos_c = temp_c;
			}
			spo2_alg_data->ir_fft_out[ii][0] = spo2_alg_data->ir_fft_out[ii][0] + cos_c * (spo2_alg_data->ir_iir_data.filter_out >> 3);
			spo2_alg_data->ir_fft_out[ii][1] = spo2_alg_data->ir_fft_out[ii][1] + sin_c * (spo2_alg_data->ir_iir_data.filter_out >> 3);
			spo2_alg_data->red_fft_out[ii][0] = spo2_alg_data->red_fft_out[ii][0] + cos_c * (spo2_alg_data->red_iir_data.filter_out >> 3);
			spo2_alg_data->red_fft_out[ii][1] = spo2_alg_data->red_fft_out[ii][1] + sin_c * (spo2_alg_data->red_iir_data.filter_out >> 3);
			spo2_alg_data->green_fft_out[ii][0] = spo2_alg_data->green_fft_out[ii][0] + cos_c * (spo2_alg_data->green_iir_data.filter_out >> 3);
			spo2_alg_data->green_fft_out[ii][1] = spo2_alg_data->green_fft_out[ii][1] + sin_c * (spo2_alg_data->green_iir_data.filter_out >> 3);
		}
	}

	if (spo2_alg_data->data_cnt > SPO2_CHIP_SETTLE + SPO2_FLT_SETTLE + SPO2_FFT_WIN_SIZE) {
		for (ii = 0; ii < 110; ii++) {
			spo2_alg_data->fft_phase_start[ii] = spo2_alg_data->fft_phase_start[ii] + 28 + ii;
			if (spo2_alg_data->fft_phase_start[ii] >= 1024) {
				spo2_alg_data->fft_phase_start[ii] = spo2_alg_data->fft_phase_start[ii] - 1024;
			}
			if (spo2_alg_data->fft_phase_start[ii] >= 768) {
				phase_sin_1q = spo2_alg_data->fft_phase_start[ii] - 768 ;
			} else if (spo2_alg_data->fft_phase_start[ii] >= 512) {
				phase_sin_1q = spo2_alg_data->fft_phase_start[ii] - 512 ;
			} else if (spo2_alg_data->fft_phase_start[ii] >= 256) {
				phase_sin_1q = spo2_alg_data->fft_phase_start[ii] - 256 ;
			} else {
				phase_sin_1q = spo2_alg_data->fft_phase_start[ii] ;
			}
			phase_cos_1q = 255 - phase_sin_1q;
			sin_c = spo2_sin_table[phase_sin_1q];
			cos_c = spo2_sin_table[phase_cos_1q];
			if (spo2_alg_data->fft_phase_start[ii] < 256) {
				// sin_c = sin_c;
				// cos_c = cos_c;
			} else if (spo2_alg_data->fft_phase_start[ii] < 512) {
				temp_c = sin_c;
				sin_c = cos_c;
				cos_c = -temp_c;
			} else if (spo2_alg_data->fft_phase_start[ii] < 768) {
				sin_c = -sin_c;
				cos_c = -cos_c;
			} else {
				temp_c = sin_c;
				sin_c = -cos_c;
				cos_c = temp_c;
			}

			spo2_alg_data->ir_fft_out[ii][0] = spo2_alg_data->ir_fft_out[ii][0] - cos_c * (spo2_alg_data->ir_flt_data_fifo[spo2_alg_data->start_ind] >> 3);
			spo2_alg_data->ir_fft_out[ii][1] = spo2_alg_data->ir_fft_out[ii][1] - sin_c * (spo2_alg_data->ir_flt_data_fifo[spo2_alg_data->start_ind] >> 3);
			spo2_alg_data->red_fft_out[ii][0] = spo2_alg_data->red_fft_out[ii][0] - cos_c * (spo2_alg_data->red_flt_data_fifo[spo2_alg_data->start_ind] >> 3);
			spo2_alg_data->red_fft_out[ii][1] = spo2_alg_data->red_fft_out[ii][1] - sin_c * (spo2_alg_data->red_flt_data_fifo[spo2_alg_data->start_ind] >> 3);
			spo2_alg_data->green_fft_out[ii][0] = spo2_alg_data->green_fft_out[ii][0] - cos_c * (spo2_alg_data->green_flt_data_fifo[spo2_alg_data->start_ind] >> 3);
			spo2_alg_data->green_fft_out[ii][1] = spo2_alg_data->green_fft_out[ii][1] - sin_c * (spo2_alg_data->green_flt_data_fifo[spo2_alg_data->start_ind] >> 3);
		}
	}
	if (spo2_alg_data->data_cnt > SPO2_CHIP_SETTLE + SPO2_FLT_SETTLE) {
		spo2_alg_data->ir_flt_data_fifo[spo2_alg_data->start_ind] = spo2_alg_data->ir_iir_data.filter_out;
		spo2_alg_data->red_flt_data_fifo[spo2_alg_data->start_ind] = spo2_alg_data->red_iir_data.filter_out;
		spo2_alg_data->green_flt_data_fifo[spo2_alg_data->start_ind] = spo2_alg_data->green_iir_data.filter_out;
		spo2_alg_data->start_ind++;
		if (spo2_alg_data->start_ind > (SPO2_FFT_WIN_SIZE - 1)) {
			spo2_alg_data->start_ind = 0;
		}
	}
}

int32_t tyhx_spo2_check_living_std(int32_t *data_in)
{
	uint8_t ii = 0;
	int32_t data_sum = 0;
	int32_t data_ave = 0;
	int32_t data_dif = 0;
	int32_t data_dif_sum = 0;
	int32_t data_dif_ave = 0;

	for (ii = 0; ii < 16; ii++) {
		data_sum = data_sum + data_in[ii];
	}
	data_ave = data_sum >> 4;
	for (ii = 0; ii < 16; ii++) {
		data_dif = abs(data_in[ii] - data_ave);
		if (data_dif > 1000) {
			data_dif = 1000;
		}
		data_dif_sum = data_dif_sum + data_dif;
	}
	data_dif_ave = data_dif_sum >> 4;

	return data_dif_ave;
}

void tyhx_spo2_std_result(int32_t green_data_in, int32_t ir_data_in)
{
	uint8_t ii = 0;
	int32_t check_living_green_std_sum = 0;
	int32_t check_living_ir_std_sum = 0;

	if (spo2_alg_data->data_cnt % 3 == 0) {
		for (ii = 0; ii < 15; ii++) {
			spo2_alg_data->checkliving_data.check_living_green[ii] = spo2_alg_data->checkliving_data.check_living_green[ii + 1];
			spo2_alg_data->checkliving_data.check_living_ir[ii] = spo2_alg_data->checkliving_data.check_living_ir[ii + 1];
		}
		spo2_alg_data->checkliving_data.check_living_green[15] = green_data_in >> 3;
		spo2_alg_data->checkliving_data.check_living_ir[15] = ir_data_in >> 3;
	}
	if (spo2_alg_data->data_cnt >= 75 && spo2_alg_data->data_cnt % 25 == 0) {
		spo2_alg_data->checkliving_data.check_living_green_std = tyhx_spo2_check_living_std(spo2_alg_data->checkliving_data.check_living_green);
		spo2_alg_data->checkliving_data.check_living_ir_std = tyhx_spo2_check_living_std(spo2_alg_data->checkliving_data.check_living_ir);
		if (spo2_alg_data->data_cnt == 75) {
			for (ii = 0; ii < 5; ii++) {
				spo2_alg_data->checkliving_data.check_living_green_std_fifo[ii] = spo2_alg_data->checkliving_data.check_living_green_std;
				check_living_green_std_sum = check_living_green_std_sum + spo2_alg_data->checkliving_data.check_living_green_std_fifo[ii];
				spo2_alg_data->checkliving_data.check_living_ir_std_fifo[ii] = spo2_alg_data->checkliving_data.check_living_ir_std;
				check_living_ir_std_sum = check_living_ir_std_sum + spo2_alg_data->checkliving_data.check_living_ir_std_fifo[ii];
			}
			spo2_alg_data->checkliving_data.check_living_green_std_avg = check_living_green_std_sum / 5;
			spo2_alg_data->checkliving_data.check_living_ir_std_avg = check_living_ir_std_sum / 5;
		} else {
			for (ii = 0; ii < 4; ii++) {
				spo2_alg_data->checkliving_data.check_living_green_std_fifo[ii] = spo2_alg_data->checkliving_data.check_living_green_std_fifo[ii + 1];
				check_living_green_std_sum = check_living_green_std_sum + spo2_alg_data->checkliving_data.check_living_green_std_fifo[ii];
				spo2_alg_data->checkliving_data.check_living_ir_std_fifo[ii] = spo2_alg_data->checkliving_data.check_living_ir_std_fifo[ii + 1];
				check_living_ir_std_sum = check_living_ir_std_sum + spo2_alg_data->checkliving_data.check_living_ir_std_fifo[ii];
			}
			spo2_alg_data->checkliving_data.check_living_green_std_fifo[4] = spo2_alg_data->checkliving_data.check_living_green_std;
			check_living_green_std_sum = check_living_green_std_sum + spo2_alg_data->checkliving_data.check_living_green_std_fifo[4];
			spo2_alg_data->checkliving_data.check_living_green_std_avg = check_living_green_std_sum / 5;
			spo2_alg_data->checkliving_data.check_living_ir_std_fifo[4] = spo2_alg_data->checkliving_data.check_living_ir_std;
			check_living_ir_std_sum = check_living_ir_std_sum + spo2_alg_data->checkliving_data.check_living_ir_std_fifo[4];
			spo2_alg_data->checkliving_data.check_living_ir_std_avg = check_living_ir_std_sum / 5;
		}
		//DEBUG_PRINTF("%d,%d\r\n",alg_data->data_cnt,alg_data->check_living_green_std);
	}
}

int32_t tyhx_spo2_high_pass_smooth(int32_t data_in)
{
	int32_t data_out;

	if (spo2_alg_data->data_cnt < SPO2_CHIP_SETTLE + 2) {
		spo2_alg_data->ir_baseline = data_in;
	} else {
		spo2_alg_data->ir_baseline = (spo2_alg_data->ir_baseline * 15 + data_in) >> 4;
	}
	data_out = data_in - spo2_alg_data->ir_baseline;

	return data_out;
}
/* USD add hr_spo2 bring up by longtao.ran 20241129 start */
bool tyhx_spo2_alg_send_data(int32_t *red_new_raw_data, int32_t *ir_new_raw_data, int32_t *green_new_raw_data, uint8_t R_LEDDAC, uint8_t IR_LEDDAC, uint8_t G_LEDDAC, int16_t *gsen_data_x, int16_t *gsen_data_y, int16_t *gsen_data_z, int32_t *amb_data, uint8_t dat_len)
{
	uint8_t  i = 0;

	if (spo2_alg_data == NULL) {
		return false;
	}
	for (i = 0; i < dat_len; i++) {
		tyhx_spo2_alg(red_new_raw_data[i], ir_new_raw_data[i], green_new_raw_data[i], R_LEDDAC, IR_LEDDAC, G_LEDDAC, amb_data[i]);
		//DEBUG_PRINTF(" send data: %d %d %d %d %d %d\r\n" ,red_new_raw_data[i],ir_new_raw_data[i],green_new_raw_data[i],R_LEDDAC,IR_LEDDAC,G_LEDDAC);
		tyhx_gesture_recognize(gsen_data_x[i], gsen_data_y[i], gsen_data_z[i]);
	}
	return true;
}

void tyhx_spo2_alg(int32_t red_new_raw_data, int32_t ir_new_raw_data, int32_t green_new_raw_data, uint8_t R_LEDDAC, uint8_t IR_LEDDAC, uint8_t G_LEDDAC, int32_t amb_data)
{
	uint16_t ii = 0;
	int32_t red_fifo_filter_max = 0;
	int32_t red_fifo_filter_min = 0;
	int32_t ir_fifo_filter_max = 0;
	int32_t ir_fifo_filter_min = 0;
	int32_t green_fifo_filter_max = 0;
	int32_t green_fifo_filter_min = 0;
	int32_t red0204_fifo_filter_max = 0;
	int32_t red0204_fifo_filter_min = 0;
	int32_t red_peak_sum = 0;
	int32_t ir_peak_sum = 0;
	int32_t green_peak_sum = 0;
	int32_t red_valley_sum = 0;
	int32_t ir_valley_sum = 0;
	int32_t green_valley_sum = 0;
//    int32_t spo2_sa_temp = 0;
//    int32_t spo2_gi_temp = 0;
	int32_t spo2_sa_cali = 0;
	uint8_t spo2_result_temp = 0;
	uint8_t spo2_result_num = 0;
	uint8_t hr_clip = 0;
	uint16_t signal_quality_sum = 0;
	int32_t red_data_ac_temp = 0;
	int32_t ir_data_ac_temp = 0;
	int32_t green_data_ac_temp = 0;
	int32_t red0204_data_ac_temp = 0;
	int32_t ir_high_pass_smooth_data = 0;
	int32_t spo2_check_living_snr_sum = 0;
//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 start
    int32_t check_living_snr = 0;
//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 end

	tyhx_spo2_data_cali(red_new_raw_data, ir_new_raw_data, green_new_raw_data);
	ir_high_pass_smooth_data = tyhx_spo2_high_pass_smooth(ir_new_raw_data);
	if (spo2_alg_data->data_cnt == SPO2_CHIP_SETTLE) {
		spo2_alg_data->ir_offset_50 = IR_LEDDAC;
		spo2_alg_data->red_offset_50 = R_LEDDAC;
		spo2_alg_data->green_offset_50 = G_LEDDAC;

		spo2_alg_data->ir_iir_data.fir1_dly =  spo2_alg_data->ir_data_cali;
		spo2_alg_data->ir_iir_data.fir1_dly2 = spo2_alg_data->ir_iir_data.fir1_dly;
		spo2_alg_data->red_iir_data.fir1_dly = spo2_alg_data->red_data_cali;
		spo2_alg_data->red_iir_data.fir1_dly2 = spo2_alg_data->red_iir_data.fir1_dly;
		spo2_alg_data->green_iir_data.fir1_dly = spo2_alg_data->green_data_cali;
		spo2_alg_data->green_iir_data.fir1_dly2 = spo2_alg_data->green_iir_data.fir1_dly;
		spo2_alg_data->red0204_iir_data.fir1_dly = spo2_alg_data->red_data_cali >> 8;
		spo2_alg_data->red0204_iir_data.fir1_dly2 = spo2_alg_data->red0204_iir_data.fir1_dly;
	}

	if (spo2_alg_data->data_cnt > SPO2_CHIP_SETTLE) {
		if (spo2_alg_data->ir_offset_50 > spo2_alg_data->red_offset_50) {
			spo2_alg_data->dc_dacia = spo2_alg_data->ir_offset_50 - spo2_alg_data->red_offset_50;
		} else {
			spo2_alg_data->dc_dacia = spo2_alg_data->red_offset_50 - spo2_alg_data->ir_offset_50;
		}
		spo2_alg_data->ir_data_dc = (spo2_alg_data->ir_data_cali + spo2_alg_data->ir_offset_50 * 130000) >> 10;
		spo2_alg_data->red_data_dc = (spo2_alg_data->red_data_cali + spo2_alg_data->red_offset_50 * 130000) >> 10;
		spo2_alg_data->green_data_dc = (spo2_alg_data->green_data_cali + spo2_alg_data->green_offset_50 * 130000) >> 10;
		tyhx_spo2_flt();
		tyhx_spo2_fft();
	}

//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 start
    if(spo2_alg_data->data_cnt>=25 && spo2_alg_data->data_cnt<=250 && spo2_alg_data->data_cnt%25==0)
    {
        if(spo2_alg_data->tyhx_pedometer.pow_all_temp > 200)
        {
            spo2_alg_data->mic_move_cnt = spo2_alg_data->mic_move_cnt+2;
        }
        else if(spo2_alg_data->tyhx_pedometer.pow_all_temp > 80)
        {
            spo2_alg_data->mic_move_cnt = spo2_alg_data->mic_move_cnt+1;
        }
    }
//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 end

	if (spo2_alg_data->data_cnt == SPO2_CHIP_SETTLE + SPO2_FLT_SETTLE) {
		spo2_alg_data->red_iir_data_smooth = spo2_alg_data->red_iir_data.filter_out;
		spo2_alg_data->ir_iir_data_smooth = spo2_alg_data->ir_iir_data.filter_out;
		spo2_alg_data->green_iir_data_smooth = spo2_alg_data->green_iir_data.filter_out;
	} else if (spo2_alg_data->data_cnt > SPO2_CHIP_SETTLE + SPO2_FLT_SETTLE) {
		tyhx_spo2_std_result(spo2_alg_data->green_data_cali, ir_high_pass_smooth_data);

		spo2_alg_data->red_iir_data_smooth = (spo2_alg_data->red_iir_data_smooth * 15 + spo2_alg_data->red_iir_data.filter_out) >> 4;
		spo2_alg_data->ir_iir_data_smooth = (spo2_alg_data->ir_iir_data_smooth * 15 + spo2_alg_data->ir_iir_data.filter_out) >> 4;
		spo2_alg_data->green_iir_data_smooth = (spo2_alg_data->green_iir_data_smooth * 15 + spo2_alg_data->green_iir_data.filter_out) >> 4;
		spo2_alg_data->red0204_iir_data_smooth = (spo2_alg_data->red0204_iir_data_smooth * 63 + spo2_alg_data->red0204_iir_data.filter_out) >> 6;
		//DEBUG_PRINTF("%d,%d,%d,%d\r\n",spo2_alg_data->data_cnt,spo2_alg_data->red_iir_data_smooth,spo2_alg_data->ir_iir_data_smooth,spo2_alg_data->green_iir_data_smooth);
		//DEBUG_PRINTF("%d,%d,%d,%d\r\n",spo2_alg_data->data_cnt,spo2_alg_data->red_iir_data.filter_out,spo2_alg_data->ir_iir_data.filter_out,spo2_alg_data->green_iir_data.filter_out);
		//DEBUG_PRINTF("%d,%d\r\n",spo2_alg_data->data_cnt,spo2_alg_data->red0204_iir_data_smooth);
		for (ii = 0; ii < 6; ii++) {
			spo2_alg_data->red_fifo_filter[ii] = spo2_alg_data->red_fifo_filter[ii + 1];
			spo2_alg_data->ir_fifo_filter[ii] = spo2_alg_data->ir_fifo_filter[ii + 1];
			spo2_alg_data->green_fifo_filter[ii] = spo2_alg_data->green_fifo_filter[ii + 1];
			spo2_alg_data->red0204_fifo_filter[ii] = spo2_alg_data->red0204_fifo_filter[ii + 1];
		}
		spo2_alg_data->red_fifo_filter[6] = spo2_alg_data->red_iir_data_smooth;
		spo2_alg_data->ir_fifo_filter[6] = spo2_alg_data->ir_iir_data_smooth;
		spo2_alg_data->green_fifo_filter[6] = spo2_alg_data->green_iir_data_smooth;
		spo2_alg_data->red0204_fifo_filter[6] = spo2_alg_data->red0204_iir_data_smooth;

		red_fifo_filter_max = spo2_sort_max(spo2_alg_data->red_fifo_filter, 7);
		red_fifo_filter_min = spo2_sort_min(spo2_alg_data->red_fifo_filter, 7);
		ir_fifo_filter_max = spo2_sort_max(spo2_alg_data->ir_fifo_filter, 7);
		ir_fifo_filter_min = spo2_sort_min(spo2_alg_data->ir_fifo_filter, 7);
		green_fifo_filter_max = spo2_sort_max(spo2_alg_data->green_fifo_filter, 7);
		green_fifo_filter_min = spo2_sort_min(spo2_alg_data->green_fifo_filter, 7);
		red0204_fifo_filter_max = spo2_sort_max(spo2_alg_data->red0204_fifo_filter, 7);
		red0204_fifo_filter_min = spo2_sort_min(spo2_alg_data->red0204_fifo_filter, 7);

		if (spo2_alg_data->red_fifo_filter[3] > 0 && spo2_alg_data->red_fifo_filter[3] == red_fifo_filter_max) {
			for (ii = 0; ii < 7; ii++) {
				spo2_alg_data->red_peak_fifo[ii] = spo2_alg_data->red_peak_fifo[ii + 1];
				red_peak_sum = red_peak_sum + spo2_alg_data->red_peak_fifo[ii];
			}
			if (spo2_alg_data->data_cnt > 200 && spo2_alg_data->red_fifo_filter[3] > spo2_alg_data->red_peak_ave * 2) {
				spo2_alg_data->red_peak_fifo[7] = spo2_alg_data->red_peak_ave * 2;
			} else {
				spo2_alg_data->red_peak_fifo[7] = spo2_alg_data->red_fifo_filter[3];
			}
			red_peak_sum = red_peak_sum + spo2_alg_data->red_peak_fifo[7];
			spo2_alg_data->red_peak_ave = red_peak_sum >> 3;
		} else if (spo2_alg_data->red_fifo_filter[3] < 0 && spo2_alg_data->red_fifo_filter[3] == red_fifo_filter_min) {
			for (ii = 0; ii < 7; ii++) {
				spo2_alg_data->red_valley_fifo[ii] = spo2_alg_data->red_valley_fifo[ii + 1];
				red_valley_sum = red_valley_sum + spo2_alg_data->red_valley_fifo[ii];
			}
			if (spo2_alg_data->data_cnt > 200 && spo2_alg_data->red_fifo_filter[3] < spo2_alg_data->red_valley_ave * 2) {
				spo2_alg_data->red_valley_fifo[7] = spo2_alg_data->red_valley_ave * 2;
			} else {
				spo2_alg_data->red_valley_fifo[7] = spo2_alg_data->red_fifo_filter[3];
			}
			red_valley_sum = red_valley_sum + spo2_alg_data->red_valley_fifo[7];
			spo2_alg_data->red_valley_ave = red_valley_sum >> 3;
		}

		if (spo2_alg_data->ir_fifo_filter[3] > 0 && spo2_alg_data->ir_fifo_filter[3] == ir_fifo_filter_max) {
			for (ii = 0; ii < 7; ii++) {
				spo2_alg_data->ir_peak_fifo[ii] = spo2_alg_data->ir_peak_fifo[ii + 1];
				ir_peak_sum = ir_peak_sum + spo2_alg_data->ir_peak_fifo[ii];
			}
			spo2_alg_data->ir_peak_fifo[7] = spo2_alg_data->ir_fifo_filter[3];
			ir_peak_sum = ir_peak_sum + spo2_alg_data->ir_peak_fifo[7];
			spo2_alg_data->ir_peak_ave = ir_peak_sum >> 3;
		} else if (spo2_alg_data->ir_fifo_filter[3] < 0 && spo2_alg_data->ir_fifo_filter[3] == ir_fifo_filter_min) {
			for (ii = 0; ii < 7; ii++) {
				spo2_alg_data->ir_valley_fifo[ii] = spo2_alg_data->ir_valley_fifo[ii + 1];
				ir_valley_sum = ir_valley_sum + spo2_alg_data->ir_valley_fifo[ii];
			}
			spo2_alg_data->ir_valley_fifo[7] = spo2_alg_data->ir_fifo_filter[3];
			ir_valley_sum = ir_valley_sum + spo2_alg_data->ir_valley_fifo[7];
			spo2_alg_data->ir_valley_ave = ir_valley_sum >> 3;
		}

		if (spo2_alg_data->green_fifo_filter[3] > 0 && spo2_alg_data->green_fifo_filter[3] == green_fifo_filter_max) {
			for (ii = 0; ii < 7; ii++) {
				spo2_alg_data->green_peak_fifo[ii] = spo2_alg_data->green_peak_fifo[ii + 1];
				green_peak_sum = green_peak_sum + spo2_alg_data->green_peak_fifo[ii];
			}
			spo2_alg_data->green_peak_fifo[7] = spo2_alg_data->green_fifo_filter[3];
			green_peak_sum = green_peak_sum + spo2_alg_data->green_peak_fifo[7];
			spo2_alg_data->green_peak_ave = green_peak_sum >> 3;
		} else if (spo2_alg_data->green_fifo_filter[3] < 0 && spo2_alg_data->green_fifo_filter[3] == green_fifo_filter_min) {
			for (ii = 0; ii < 7; ii++) {
				spo2_alg_data->green_valley_fifo[ii] = spo2_alg_data->green_valley_fifo[ii + 1];
				green_valley_sum = green_valley_sum + spo2_alg_data->green_valley_fifo[ii];
			}
			spo2_alg_data->green_valley_fifo[7] = spo2_alg_data->green_fifo_filter[3];
			green_valley_sum = green_valley_sum + spo2_alg_data->green_valley_fifo[7];
			spo2_alg_data->green_valley_ave = green_valley_sum >> 3;
		}

		if (spo2_alg_data->red0204_fifo_filter[3] > 0 && spo2_alg_data->red0204_fifo_filter[3] == red0204_fifo_filter_max) {
			spo2_alg_data->red0204_peak_ave = spo2_alg_data->red0204_fifo_filter[3];
		} else if (spo2_alg_data->red0204_fifo_filter[3] < 0 && spo2_alg_data->red0204_fifo_filter[3] == red0204_fifo_filter_min) {
			spo2_alg_data->red0204_valley_ave = spo2_alg_data->red0204_fifo_filter[3];
		}
		red_data_ac_temp = (spo2_alg_data->red_peak_ave - spo2_alg_data->red_valley_ave) * 4 / 10;
		ir_data_ac_temp = (spo2_alg_data->ir_peak_ave - spo2_alg_data->ir_valley_ave) * 4 / 10;
		green_data_ac_temp = (spo2_alg_data->green_peak_ave - spo2_alg_data->green_valley_ave) * 4 / 10;
		spo2_alg_data->red_data_ac = (spo2_alg_data->red_data_ac * 63 + red_data_ac_temp) >> 6;
		spo2_alg_data->ir_data_ac = (spo2_alg_data->ir_data_ac * 63 + ir_data_ac_temp) >> 6;
		spo2_alg_data->green_data_ac = (spo2_alg_data->green_data_ac * 63 + green_data_ac_temp) >> 6;

		red0204_data_ac_temp = spo2_alg_data->red0204_peak_ave - spo2_alg_data->red0204_valley_ave;
		if (red0204_data_ac_temp > spo2_alg_data->red0204_data_ac) {
			spo2_alg_data->red0204_data_ac = (spo2_alg_data->red0204_data_ac * 63 + red0204_data_ac_temp) >> 6;
		} else {
			spo2_alg_data->red0204_data_ac = red0204_data_ac_temp;
		}
		//DEBUG_PRINTF("%d %d %d \r\n",spo2_alg_data->data_cnt, spo2_alg_data->red_peak_ave, spo2_alg_data->red_valley_ave);
		//DEBUG_PRINTF("%d %d\r\n",spo2_alg_data->data_cnt, spo2_alg_data->red0204_data_ac);
	}

	if (spo2_alg_data->count_block > 0) {
		spo2_alg_data->count_block--;
	}
	if (spo2_alg_data->data_cnt >= 200 && spo2_alg_data->data_cnt % 25 == 0) {
		tyhx_sig_snr(spo2_alg_data->ir_fft_out, &spo2_alg_data->ir_fft_snr, &spo2_alg_data->ir_hr);
		tyhx_sig_snr(spo2_alg_data->red_fft_out, &spo2_alg_data->red_fft_snr, &spo2_alg_data->red_hr);
		tyhx_sig_snr(spo2_alg_data->green_fft_out, &spo2_alg_data->green_fft_snr, &spo2_alg_data->green_hr);
		//DEBUG_PRINTF("%d %d %d \r\n",spo2_alg_data->data_cnt,spo2_alg_data->ir_fft_snr,spo2_alg_data->red_fft_snr);

		for (ii = 0; ii < 5; ii++) {
			spo2_alg_data->signal_quality_fifo[ii] = spo2_alg_data->signal_quality_fifo[ii + 1];
			signal_quality_sum = signal_quality_sum + spo2_alg_data->signal_quality_fifo[ii];
		}
		spo2_alg_data->signal_quality_fifo[5] = (spo2_alg_data->green_fft_snr + spo2_alg_data->ir_fft_snr) / 2;
		signal_quality_sum = signal_quality_sum + spo2_alg_data->signal_quality_fifo[5];
		spo2_alg_data->poor_signal = signal_quality_sum / 6;

//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 start
        if(spo2_alg_data->green_fft_snr<25)
        {
            check_living_snr = spo2_alg_data->green_fft_snr;
        }
        else
        {
            check_living_snr = 25;
        }
        
        if(spo2_alg_data->data_cnt == 200)
        {
            for(ii=0;ii<5;ii++)
            {
                spo2_alg_data->checkliving_data.check_living_snr_value_fifo[ii] = check_living_snr;
            }
            spo2_alg_data->checkliving_data.check_living_snr_avg = check_living_snr;
        }
        else
        {
            for(ii=0;ii<4;ii++)
            {
                spo2_alg_data->checkliving_data.check_living_snr_value_fifo[ii] = spo2_alg_data->checkliving_data.check_living_snr_value_fifo[ii+1];
                spo2_check_living_snr_sum = spo2_check_living_snr_sum + spo2_alg_data->checkliving_data.check_living_snr_value_fifo[ii];
            }
            spo2_alg_data->checkliving_data.check_living_snr_value_fifo[4] = check_living_snr;
            spo2_check_living_snr_sum = spo2_check_living_snr_sum + spo2_alg_data->checkliving_data.check_living_snr_value_fifo[4];
            spo2_alg_data->checkliving_data.check_living_snr_avg = spo2_check_living_snr_sum/5;
        }
//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 end

		if (spo2_alg_data->data_cnt == 200) {
			if (spo2_alg_data->green_fft_snr > 32) {
				spo2_alg_data->hr = spo2_alg_data->green_hr;
				if (spo2_mode == SPO2_ALLDAY_MODE && spo2_alg_data->green_hr > 80) {
					spo2_alg_data->hr = 80 + spo2_alg_data->green_hr % 10;
				}
			} else {
				spo2_alg_data->hr = spo2_alg_data->green_hr;
				if (spo2_alg_data->green_hr > 80) {
					spo2_alg_data->hr = 80 + spo2_alg_data->green_hr % 10;
				} else if (spo2_alg_data->green_hr < 60) {
					spo2_alg_data->hr = 60 + spo2_alg_data->green_hr % 10;
				}
			}
		} else {
			if (spo2_alg_data->green_fft_snr > 32) {
				if (spo2_alg_data->green_hr > 100) {
					hr_clip = 2;
				} else {
					hr_clip = 4;
				}
			} else if (spo2_alg_data->green_fft_snr > 24) {
				hr_clip = 2;
			} else if (spo2_alg_data->green_fft_snr > 16) {
				hr_clip = 1;
			} else {
				hr_clip = 0;
			}
			spo2_alg_data->hr = spo2_alg_data->green_hr;
			if (spo2_alg_data->hr > spo2_alg_data->hr_pre + hr_clip) {
				spo2_alg_data->hr = spo2_alg_data->hr_pre + hr_clip;
			} else if (spo2_alg_data->hr < spo2_alg_data->hr_pre - hr_clip) {
				spo2_alg_data->hr = spo2_alg_data->hr_pre - hr_clip;
			}
		}
		spo2_alg_data->hr_pre = spo2_alg_data->hr;


		if (spo2_alg_data->red_data_ac > 0 && spo2_alg_data->ir_data_ac > 0 && spo2_alg_data->ir_data_dc > 0 && spo2_alg_data->red_data_dc > 0 \
			&& spo2_alg_data->green_data_ac > 0 && spo2_alg_data->green_data_dc > 0) {
			spo2_alg_data->spo2_sa_temp = ((spo2_usuallyadjust.SPO2_SLOPE / 100) * spo2_alg_data->red_data_ac) / spo2_alg_data->ir_data_ac;
			spo2_alg_data->spo2_sa_temp = (spo2_alg_data->spo2_sa_temp * spo2_alg_data->ir_data_dc) / spo2_alg_data->red_data_dc;
			spo2_alg_data->spo2_sa_temp = spo2_alg_data->spo2_sa_temp * 100;
			spo2_alg_data->spo2_gi_temp = (spo2_alg_data->green_data_ac * 5000) / spo2_alg_data->ir_data_ac;
			spo2_alg_data->spo2_gi_temp = (spo2_alg_data->spo2_gi_temp * spo2_alg_data->ir_data_dc) / spo2_alg_data->green_data_dc;
		} else {
			spo2_alg_data->spo2_sa_temp = spo2_usuallyadjust.SPO2_BASE_LINE_INIT - 103000 + 5000;
			spo2_alg_data->spo2_gi_temp = 50000;
		}
//USD:add X6855OS151-5 by yuan.tao for [update algo] 20250108 start
        //amb_rectify
        if(amb_data > spo2_usuallyadjust.AMB_THRE+14500)
        {
            spo2_alg_data->count_block = 40;
        }
        else if(amb_data > spo2_usuallyadjust.AMB_THRE && spo2_alg_data->spo2_sa_temp > spo2_usuallyadjust.SPO2_BASE_LINE_INIT-103000+4000)
        {
            if((amb_data - spo2_usuallyadjust.AMB_THRE)*2 < spo2_usuallyadjust.SPO2_BASE_LINE_INIT-105000)
            {
                spo2_alg_data->spo2_sa_temp = spo2_alg_data->spo2_sa_temp-(amb_data - spo2_usuallyadjust.AMB_THRE);
            }
            else
            {
                spo2_alg_data->spo2_sa_temp = spo2_usuallyadjust.SPO2_BASE_LINE_INIT-100000;
            }

        }
//USD:add X6855OS151-5 by yuan.tao for [update algo] 20250108 end

		//DEBUG_PRINTF("%d %d %d %d %d %d\r\n",spo2_alg_data->data_cnt,spo2_alg_data->red_data_ac,spo2_alg_data->ir_data_ac,spo2_alg_data->red_data_dc,spo2_alg_data->ir_data_dc,spo2_gi_temp);
		//DEBUG_PRINTF("%d %d %d %d %d %d\r\n",spo2_alg_data->data_cnt,spo2_alg_data->red_data_ac,spo2_alg_data->ir_data_ac,spo2_alg_data->red_data_dc,spo2_alg_data->ir_data_dc,spo2_sa_temp);
		if (spo2_alg_data->red_data_ac > 8000) {
			spo2_sa_cali = spo2_usuallyadjust.SPO2_SLOPE / 10 * (spo2_alg_data->red_data_ac - 8000) / 3000;
			if (spo2_sa_cali > spo2_usuallyadjust.SPO2_SLOPE * 4 / 10) {
				spo2_sa_cali = spo2_usuallyadjust.SPO2_SLOPE * 4 / 10;
			}
		}

		if (spo2_alg_data->ir_fft_snr > spo2_usuallyadjust.SPO2_LOW_SNR_THRE && spo2_alg_data->red_fft_snr > spo2_usuallyadjust.SPO2_LOW_SNR_THRE \
			&& spo2_alg_data->spo2_gi_temp > 35000 && spo2_alg_data->hr > 70) {
			spo2_alg_data->hq_sig_cnt++;
		} else if (spo2_alg_data->ir_fft_snr > spo2_usuallyadjust.SPO2_LOW_SNR_THRE + 20 && spo2_alg_data->red_fft_snr > spo2_usuallyadjust.SPO2_LOW_SNR_THRE + 20) {
			spo2_alg_data->hq_sig_cnt++;
		}
		if (spo2_alg_data->data_cnt <= 600  && spo2_alg_data->hq_sig_cnt >= 8) {
			spo2_alg_data->hq_sig_flag = 1;
		}

		if (spo2_alg_data->data_cnt <= 450) {
			spo2_alg_data->red_snr_sum = spo2_alg_data->red_snr_sum + spo2_alg_data->red_fft_snr;
		}
		if (spo2_alg_data->data_cnt == 450) {
			spo2_alg_data->red_snr_avg = spo2_alg_data->red_snr_sum / 11;
			if (spo2_alg_data->red_snr_avg < spo2_usuallyadjust.SPO2_LOW_SNR_THRE - 10) {
				spo2_alg_data->lq_sig_flag = 1;
			}
		}

		if (spo2_alg_data->ir_fft_snr < (spo2_usuallyadjust.SPO2_LOW_SNR_THRE * 3) >> 2 || spo2_alg_data->red_fft_snr < spo2_usuallyadjust.SPO2_LOW_SNR_THRE / 2) {
			spo2_alg_data->count_block = 40;
			spo2_alg_data->snr_block++;
		}
//        else if(spo2_alg_data->red_fft_snr < (spo2_usuallyadjust.SPO2_LOW_SNR_THRE*3)>>2 && (spo2_alg_data->hr < 65 || spo2_alg_data->red_data_ac < 2000))
//        {
//            spo2_alg_data->count_block = 40;//2024.0110
//        }
//        else if(spo2_alg_data->spo2_sa_final > spo2_usuallyadjust.SPO2_BASE_LINE_INIT-103000+spo2_usuallyadjust.SPO2_SLOPE*0.2 && spo2_alg_data->spo2_sa_temp > spo2_alg_data->spo2_sa_final)
//        {
//            if(spo2_alg_data->data_cnt>200 && spo2_alg_data->hr>75 && spo2_alg_data->green_data_ac > 15000 && spo2_alg_data->spo2_gi_temp < 30000 && spo2_alg_data->red_data_ac > spo2_alg_data->green_data_ac)//AC CHANGE
//            {
//                spo2_alg_data->count_block = 40;
//            }
//            else if(spo2_alg_data->spo2_sa_temp > spo2_usuallyadjust.SPO2_SLOPE*5/2)
//            {
//                spo2_alg_data->count_block = 40;
//            }
//        }

		if (spo2_alg_data->data_cnt > 100 && spo2_alg_data->motion > 1) {
			spo2_alg_data->count_block = 60;
		}

		//BrBlock
		if (spo2_alg_data->data_cnt > 200 && spo2_alg_data->red_data_ac * 2 < spo2_alg_data->red0204_data_ac) {
			if (spo2_alg_data->dc_dacia > 15 && spo2_alg_data->hr < 75 && spo2_alg_data->red0204_data_ac > 5000) {
				spo2_alg_data->count_block = 40;
			} else if (spo2_alg_data->hr < 70 && spo2_alg_data->red_data_ac * 4 < spo2_alg_data->red0204_data_ac) {
				spo2_alg_data->count_block = 40;
			}
		}
//         N65 fature block
//        if(spo2_alg_data->spo2_gi_temp < 6000||(spo2_alg_data->red_data_ac > spo2_alg_data->green_data_ac && spo2_alg_data->green_data_ac>4000 && spo2_alg_data->spo2_gi_temp > 60000))
//        {
//            spo2_alg_data->count_block = 40;
//        }
		if (spo2_alg_data->dc_dacia > 15 && spo2_alg_data->hr < 65 && spo2_alg_data->spo2_sa_temp > spo2_usuallyadjust.SPO2_BASE_LINE_INIT - 103000 + 19000) {
			spo2_alg_data->count_block = 40;
		}
		/* USD add hr_spo2 Algorithm update by longtao.ran 20241017 start */
		if (spo2_alg_data->data_cnt > 200 && spo2_alg_data->spo2_sa_temp > spo2_usuallyadjust.SPO2_BASE_LINE_INIT - 103000 + 35000) {
			spo2_alg_data->count_block = 40;
		}

		//printf("spo2_alg_status:%d\r\n",spo2_alg_status);
		/* USD add hr_spo2 Algorithm update by longtao.ran 20241017 end */

//        printf("cnt:%d gi_tem:%d sa_tem:%d sa:%d sa_fin:%d result:%d block%d rAc%d rSnr%d Hr%d snrB:%d\r\n",spo2_alg_data->data_cnt,spo2_alg_data->spo2_gi_temp,spo2_alg_data->spo2_sa_temp,spo2_alg_data->spo2_sa,spo2_alg_data->spo2_sa_final,spo2_alg_data->spo2_result,spo2_alg_data->count_block,spo2_alg_data->red_data_ac,spo2_alg_data->red_fft_snr,spo2_alg_data->hr,spo2_alg_data->snr_block);
		if (spo2_alg_data->count_block == 0) {
			spo2_alg_data->spo2_sa = spo2_alg_data->spo2_sa_temp;


//            if(spo2_alg_data->data_cnt>400 && spo2_alg_data->spo2_gi_temp<25*1000)
//            {
//                spo2_alg_data->spo2_sa = spo2_alg_data->spo2_sa - 1500;
//                if(spo2_alg_data->spo2_gi_temp<8000 && spo2_alg_data->dc_dacia > 20)
//                {
//                        spo2_alg_data->spo2_sa = spo2_alg_data->spo2_sa - 16000;//2024.0110 black gi adjust
//                }
//            }
//            else if(spo2_alg_data->red_data_ac>6000 && spo2_alg_data->spo2_gi_temp<32*1000 && spo2_alg_data->green_data_ac > 5000)
//            {
//                spo2_alg_data->spo2_sa = spo2_alg_data->spo2_sa - 4500;
//            }
//            if(spo2_alg_data->spo2_sa > spo2_usuallyadjust.SPO2_SLOPE*12/10)
//            {
////                spo2_alg_data->spo2_sa = ((spo2_alg_data->spo2_sa - spo2_usuallyadjust.SPO2_SLOPE*11/10)>>4)+spo2_usuallyadjust.SPO2_SLOPE*11/10;
//            }
//            DEBUG_PRINTF("%d %d %d %d %d\r\n",spo2_alg_data->data_cnt,spo2_alg_data->spo2_gi_temp,spo2_alg_data->spo2_sa,spo2_alg_data->spo2_sa_final,spo2_alg_data->spo2_result);

//            if(spo2_mode == SPO2_ALLDAY_MODE)
//            {
//                if(spo2_alg_data->spo2_sa_temp>50000)
//                {
//                    spo2_alg_data->spo2_sa = spo2_usuallyadjust.SPO2_BASE_LINE_INIT-95000;
//                }
//                else if(spo2_alg_data->spo2_sa_temp>spo2_usuallyadjust.SPO2_SLOPE*1)
//                {
//                    spo2_alg_data->spo2_sa = (spo2_usuallyadjust.SPO2_SLOPE + spo2_alg_data->spo2_sa_temp)>>1;
//                }
//                if(spo2_alg_data->red_data_ac*2 < spo2_alg_data->red0204_data_ac)
//                {
//                    spo2_alg_data->spo2_sa = spo2_usuallyadjust.SPO2_BASE_LINE_INIT-100000;
//                }
//            }

			if (spo2_alg_data->ir_fft_snr > spo2_usuallyadjust.SPO2_LOW_SNR_THRE) {
				if (spo2_alg_data->spo2_sa > spo2_alg_data->spo2_sa_final + 6000) {
					spo2_alg_data->spo2_sa_final = spo2_alg_data->spo2_sa_final + 1500;
				} else if (spo2_alg_data->spo2_sa < spo2_alg_data->spo2_sa_final + 6000 && spo2_alg_data->data_cnt > 600) {
					spo2_alg_data->spo2_sa_final = (spo2_alg_data->spo2_sa_final * 7 + spo2_alg_data->spo2_sa) >> 3;
				} else {
					spo2_alg_data->spo2_sa_final = (spo2_alg_data->spo2_sa_final + spo2_alg_data->spo2_sa) >> 1;
				}
			} else if (spo2_alg_data->ir_fft_snr > spo2_usuallyadjust.SPO2_LOW_SNR_THRE / 2 && spo2_alg_data->spo2_sa_final < spo2_usuallyadjust.SPO2_BASE_LINE_INIT - 103000 + spo2_usuallyadjust.SPO2_SLOPE / 7) {
				if (spo2_alg_data->spo2_sa > spo2_alg_data->spo2_sa_final + 3000) {
					spo2_alg_data->spo2_sa_final = spo2_alg_data->spo2_sa_final + 1000;
				} else {
					spo2_alg_data->spo2_sa_final = (spo2_alg_data->spo2_sa_final * 3 + spo2_alg_data->spo2_sa) >> 2;
				}
			}
			//DEBUG_PRINTF("%d %d %d %d\r\n",spo2_alg_data->data_cnt,spo2_alg_data->spo2_sa_final,spo2_alg_data->spo2_sa,spo2_sa_temp);
			//DEBUG_PRINTF("%d %d %d %d %d\r\n",spo2_alg_data->data_cnt,spo2_alg_data->red_data_ac,spo2_alg_data->ir_data_ac,spo2_alg_data->red_data_dc,spo2_alg_data->ir_data_dc);
		}
		//2024.0110 result 95 tendency when sa_temp gose high at the first time then be blocked
		else {
			if (spo2_alg_data->spo2_result_flag < 2 && spo2_alg_data->spo2_sa_final > spo2_usuallyadjust.SPO2_BASE_LINE_INIT - 103000 + 5000) {
				spo2_alg_data->spo2_sa_final = spo2_alg_data->spo2_sa_final - 200;
			} else if (spo2_alg_data->spo2_result_flag == 2 && spo2_alg_data->spo2_sa_final < spo2_usuallyadjust.SPO2_SLOPE * 4 / 10 + spo2_usuallyadjust.SPO2_BASE_LINE_INIT - 103000) {
				spo2_alg_data->spo2_sa_final = spo2_alg_data->spo2_sa_final + 200;
			}
		}

		spo2_result_temp = (spo2_usuallyadjust.SPO2_BASE_LINE_INIT - spo2_alg_data->spo2_sa_final) >> 10; //2024.0110

		if (spo2_result_temp > 99) {
			spo2_result_temp = 99;
		} else if (spo2_result_temp < 68) {
			spo2_result_temp = 68;
		}
		spo2_result_num = spo2_result_temp - 68;
//        DEBUG_PRINTF("%d \r\n",spo2_result_num);
		if (spo2_alg_data->hq_sig_flag == 1) {
			if (spo2_alg_data->dc_dacia > 15) {
				spo2_alg_data->spo2_result = spo2_data_black[spo2_result_num];
			} else {
				spo2_alg_data->spo2_result = spo2_data_hqsig[spo2_result_num];
			}
		} else {
			if (spo2_alg_data->dc_dacia > 15) {
				spo2_alg_data->spo2_result = spo2_data_black[spo2_result_num];
			} else {
				spo2_alg_data->spo2_result = spo2_data_table[spo2_result_num];
			}
		}

//        if(spo2_alg_data->spo2_result_min > spo2_alg_data->spo2_result)
//        {
//            spo2_alg_data->spo2_result_min = spo2_alg_data->spo2_result;
//        }
//        if(spo2_alg_data->hq_sig_flag==1 && spo2_alg_data->spo2_result > spo2_alg_data->spo2_result_min && spo2_alg_data->spo2_result_min<95)
//        {
//            spo2_alg_data->spo2_result = spo2_alg_data->spo2_result_min;
//        }
//        if(spo2_alg_data->spo2_result > spo2_alg_data->spo2_result_min && spo2_alg_data->spo2_result_min<90)
//        {
//            spo2_alg_data->spo2_result = spo2_alg_data->spo2_result_min;
//        }
//        if(spo2_alg_data->lq_sig_flag==1 && spo2_alg_data->spo2_result<93)
//        {
////            spo2_alg_data->spo2_result = 93;
//        }
		//AlldayModeChange_240711
		if (spo2_alg_data->spo2_result == 99 || (spo2_mode == SPO2_ALLDAY_MODE && spo2_alg_data->dc_dacia > 7) || (spo2_alg_data->data_cnt > 450 && spo2_alg_data->spo2_result == 97 && spo2_alg_data->count_block > 0 && spo2_alg_data->spo2_sa_temp < spo2_usuallyadjust.SPO2_BASE_LINE_INIT - 103000 + 6000)) { //
			spo2_alg_data->rand_cnt++;
			if (spo2_alg_data->rand_cnt == 5) {
				spo2_alg_data->spo2_rand = (abs(spo2_alg_data->red_iir_data_smooth) % 3) - 1;
				spo2_alg_data->rand_cnt = 0;
			}
			spo2_alg_data->spo2_result = 98 + spo2_alg_data->spo2_rand;
		}
//        else if((spo2_alg_data->spo2_sa_temp>=spo2_usuallyadjust.SPO2_BASE_LINE_INIT-103000+6000 && spo2_alg_data->spo2_sa_final<=spo2_usuallyadjust.SPO2_BASE_LINE_INIT-103000+3000 && spo2_alg_data->snr_block > 8) || (spo2_alg_data->spo2_sa>spo2_usuallyadjust.SPO2_BASE_LINE_INIT-103000+15000 && spo2_mode == SPO2_NORMAL_MODE))
//        {
//            spo2_alg_data->rand_cnt1++;
//            if(spo2_alg_data->rand_cnt1==5)
//            {
//                spo2_alg_data->spo2_rand1 = (abs(spo2_alg_data->red_iir_data_smooth)%2);
//                spo2_alg_data->rand_cnt1 = 0;
//            }
//            spo2_alg_data->spo2_result = 95+spo2_alg_data->spo2_rand1;
//            spo2_alg_data->spo2_sa_final = spo2_usuallyadjust.SPO2_BASE_LINE_INIT-103000+6000;
//        }
		else if (spo2_mode == SPO2_ALLDAY_MODE && spo2_alg_data->spo2_sa_final >= spo2_usuallyadjust.SPO2_BASE_LINE_INIT - 103000 + 9000) {
			spo2_alg_data->spo2_rand = (abs(spo2_alg_data->red_iir_data_smooth) % 3);
			spo2_alg_data->spo2_result = 93 + spo2_alg_data->spo2_rand;
		} else {
			spo2_alg_data->rand_cnt = 0;
			spo2_alg_data->spo2_rand = 1;
		}

//        if(spo2_mode == SPO2_ALLDAY_MODE && spo2_alg_data->spo2_result < 93)
//        {
//            spo2_alg_data->spo2_result = (spo2_alg_data->spo2_result+92)/2;
//        }

		if (spo2_alg_data->data_cnt == 700 && spo2_mode == SPO2_NORMAL_MODE) {
			if (spo2_alg_data->spo2_result >= 95) {
				spo2_alg_data->spo2_result_flag = 1;
			} else if (spo2_alg_data->spo2_result < 90) {
				spo2_alg_data->spo2_result_flag = 2;
			}
		}
//        if(spo2_alg_data->data_cnt > 700)
//        {
//            if(spo2_alg_data->spo2_result_flag==1 && spo2_alg_data->spo2_result<95)
//            {
//                spo2_alg_data->spo2_result = 95;
//            }
//            else if(spo2_alg_data->spo2_result_flag==2 && spo2_alg_data->spo2_result>89)
//            {
//                spo2_alg_data->spo2_result = 89;
//            }
//        }
		if(spo2_alg_data->data_cnt == 700)
		{
			spo2_alg_status = MSG_SPO2_READY;
		}
        
		if(spo2_alg_status == MSG_SPO2_READY)
		{
			if(spo2_alg_data->data_cnt >= 700)
			{
				spo2_alg_status = MSG_SPO2_END;
			}
		}
	}
	/* USD add hr_spo2 fix ke by longtao.ran 20241017 start */
	if (spo2_alg_data->data_cnt >= 400 && spo2_living_status == MSG_SPO2_LIVING_INITIAL) {
		spo2_living_status = MSG_SPO2_LIVING_FAIL;
	}
	/* USD add hr_spo2 fix ke by longtao.ran 20241017 end */
	if (spo2_alg_data->green_data_dc > 80) {
		if (SPO2_Check_Living_Mode > 0 && spo2_living_status != MSG_SPO2_LIVING_FAIL && spo2_alg_data->checkliving_data.checkliving_close == 0) {
			if (spo2_alg_data->data_cnt >= 400) {
				if (spo2_living_status == MSG_SPO2_LIVING_INITIAL && spo2_alg_data->motion == 0) {
					spo2_alg_data->checkliving_data.checkliving_close = 1;
				}
			} else if (spo2_alg_data->data_cnt >= 200) {
//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 start
                if(spo2_alg_data->mic_move_cnt >= 12 && spo2_living_status == MSG_SPO2_LIVING_INITIAL)
                {
                    spo2_living_status = MSG_SPO2_LIVING_FAIL;
                }
                else if(spo2_alg_data->mic_move_cnt >= 6)
                {
                    if(spo2_alg_data->checkliving_data.check_living_green_std_avg >= SPO2_ST_Thre)
                    {
                        if(spo2_alg_data->checkliving_data.check_living_snr_avg >= SPO2_QU_Thre+4)
                        {                    
                            spo2_living_status = MSG_SPO2_LIVING_TRUE;
                        }
                    }
                }
                else
            {
				if (spo2_alg_data->checkliving_data.check_living_green_std_avg >= SPO2_ST_Thre) {
					if (spo2_alg_data->checkliving_data.check_living_snr_avg >= SPO2_QU_Thre) {
						spo2_living_status = MSG_SPO2_LIVING_TRUE;
					}
				} else if (spo2_alg_data->checkliving_data.check_living_green_std_avg >= SPO2_ST_Thre - 10 \
						   && spo2_alg_data->checkliving_data.check_living_snr_avg >= SPO2_QU_Thre + 5) {
					spo2_living_status = MSG_SPO2_LIVING_TRUE;
				} else {
					spo2_living_status = MSG_SPO2_LIVING_FAIL;
					}
				}
			} 
//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 end
			else if (spo2_alg_data->data_cnt >= 100) {
				if (spo2_alg_data->checkliving_data.check_living_green_std_avg < SPO2_ST_Thre - 10) {
					spo2_living_status = MSG_SPO2_LIVING_FAIL;
				}
			} else {
				spo2_living_status = MSG_SPO2_LIVING_INITIAL;
			}

			if (spo2_alg_data->data_cnt > 100) {
				if (ir_new_raw_data < -800000 && red_new_raw_data < -800000 && spo2_mode == SPO2_NORMAL_MODE) {
					spo2_living_status = MSG_SPO2_LIVING_FAIL;
				}
			}
		}
	} else {
		if (SPO2_Check_Living_Mode > 0 && spo2_living_status != MSG_SPO2_LIVING_FAIL  && spo2_alg_data->checkliving_data.checkliving_close == 0) {
			if (spo2_alg_data->data_cnt >= 400) {
				if (spo2_living_status == MSG_SPO2_LIVING_INITIAL && spo2_alg_data->motion == 0) {
					spo2_alg_data->checkliving_data.checkliving_close = 1;
				}
			} else if (spo2_alg_data->data_cnt >= 200) {
				if (spo2_alg_data->checkliving_data.check_living_snr_avg >= SPO2_QU_Thre) {
					spo2_living_status = MSG_SPO2_LIVING_TRUE;
				}
			} else {
				spo2_living_status = MSG_SPO2_LIVING_INITIAL;
			}
//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 start
            if(spo2_alg_data->data_cnt > 100)
            {
                if(ir_new_raw_data<-450000 && red_new_raw_data<-450000 && spo2_mode == SPO2_NORMAL_MODE)
                {
                    spo2_living_status = MSG_SPO2_LIVING_FAIL;
                }
            }
//USD:add TOS1501-49072 by yuan.tao for [add gsensor judge] 20250218 end
			if (spo2_alg_data->green_data_dc < 50 && spo2_mode == SPO2_NORMAL_MODE) {
				if (spo2_alg_data->data_cnt >= 200) {
					if (spo2_alg_data->checkliving_data.check_living_snr_avg >= SPO2_QU_Thre - 5) {
						spo2_living_status = MSG_SPO2_LIVING_TRUE;
					}
				}
			}
		}
	}
	//DEBUG_PRINTF("%d %d %d %d\r\n",spo2_alg_data->data_cnt,spo2_alg_data->checkliving_data.check_living_green_std_avg,spo2_alg_data->checkliving_data.check_living_snr_avg,spo2_living_status);
//    DEBUG_PRINTF("%d %d %d %d %d\r\n",spo2_alg_data->spo2_result,spo2_alg_data->motion_err_cnt,spo2_alg_data->screenup_err_cnt,spo2_alg_data->signal_err_cnt,spo2_alg_data->user_err);

	spo2_alg_data->data_cnt = spo2_alg_data->data_cnt + 1;
}
/* USD add hr_spo2 bring up by longtao.ran 20241129 end */

spo2_debug_t spo2_alg_debug_results(void)
{
	spo2_debug_t spo2_debug_results = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	spo2_debug_results.data_cnt = spo2_alg_data->data_cnt;
	spo2_debug_results.motion = spo2_alg_data->motion;
	spo2_debug_results.ir_offset_50 = spo2_alg_data->ir_offset_50;
	spo2_debug_results.red_offset_50 = spo2_alg_data->red_offset_50;
	spo2_debug_results.green_offset_50 = spo2_alg_data->green_offset_50;
	spo2_debug_results.ir_dc = spo2_alg_data->ir_data_dc;
	spo2_debug_results.red_dc = spo2_alg_data->red_data_dc;
	spo2_debug_results.green_dc = spo2_alg_data->green_data_dc;
	spo2_debug_results.ir_ac = spo2_alg_data->ir_data_ac;
	spo2_debug_results.red_ac = spo2_alg_data->red_data_ac;
	spo2_debug_results.green_ac = spo2_alg_data->green_data_ac;
	spo2_debug_results.sa_temp = spo2_alg_data->spo2_sa_temp;
	spo2_debug_results.gi_temp = spo2_alg_data->spo2_gi_temp;
	spo2_debug_results.sa_final = spo2_alg_data->spo2_sa_final;
	spo2_debug_results.ir_fft_snr = spo2_alg_data->ir_fft_snr;
	spo2_debug_results.red_fft_snr = spo2_alg_data->red_fft_snr;
	spo2_debug_results.count_block = spo2_alg_data->count_block;
	spo2_debug_results.spo2_result = spo2_alg_data->spo2_result;
	return   spo2_debug_results;
}

spo2_results_t tyhx_spo2_alg_get_results(void)
{
	spo2_results_t spo2_results = {MSG_SPO2_ALG_NOT_OPEN, MSG_SPO2_LIVING_INITIAL, RESULT_WITHOUT_ERR, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
	spo2_results.spo2_alg_status = spo2_alg_status;
	spo2_results.spo2_living_status = spo2_living_status;
	spo2_results.result_err = (spo2_err_value_t)spo2_alg_data->user_err;
	spo2_results.data_cnt = spo2_alg_data->data_cnt;
	spo2_results.block_cnt = spo2_alg_data->count_block;
	spo2_results.ir_quality = spo2_alg_data->green_fft_snr;
	spo2_results.reliable_score_result = spo2_alg_data->green_fft_snr;
	spo2_results.poor_signal = spo2_alg_data->poor_signal;
	spo2_results.motion = spo2_alg_data->motion;
	spo2_results.screen_up = spo2_alg_data->screen_up;
	spo2_results.signal_snr = spo2_alg_data->checkliving_data.check_living_snr_avg;
	spo2_results.signal_std = (spo2_alg_data->checkliving_data.check_living_green_std_avg + spo2_alg_data->checkliving_data.check_living_ir_std_avg) / 2;
	spo2_results.spo_mode = spo2_mode;
	//spo2_results.signal_std = (uint32_t)spo2_alg_data->spo2_gi_temp;
	if (SPO2_Check_Living_Mode > 0) {
		/* USD add hr_spo2 Algorithm update by longtao.ran 20241017 start */
		if (spo2_alg_data->data_cnt >= 200 && spo2_living_status == MSG_SPO2_LIVING_TRUE) {
			spo2_results.hr_result = spo2_alg_data->hr;
		} else {
			spo2_results.hr_result = 0;
		}

		if ((spo2_alg_status == MSG_SPO2_READY || spo2_alg_status == MSG_SPO2_END) && spo2_living_status == MSG_SPO2_LIVING_TRUE) {
			spo2_results.spo2_result = spo2_alg_data->spo2_result;
		} else {
			spo2_results.spo2_result = 0;
		}
		/* USD add hr_spo2 Algorithm update by longtao.ran 20241017 end */
	} else {
		if (spo2_alg_status == MSG_SPO2_READY || spo2_alg_status == MSG_SPO2_END) {
			spo2_results.spo2_result = spo2_alg_data->spo2_result;
			spo2_results.hr_result = spo2_alg_data->hr;
		} else {
			spo2_results.spo2_result = 0;
			spo2_results.hr_result = 0;
		}
	}
	return   spo2_results;
}

spo2_gesture_recognize_t tyhx_spo2_gesture_get_results(void)
{
	spo2_gesture_recognize_t spo2_gesture_status = {1, 0, 0};
	spo2_gesture_status.screen_up = spo2_alg_data->screen_up;
	spo2_gesture_status.motion = spo2_alg_data->motion;
	spo2_gesture_status.poor_signal = spo2_alg_data->poor_signal;

	return spo2_gesture_status;
}
/* USD add hr_spo2 Algorithm update by longtao.ran 20241010 end */
#endif //#ifdef SPO2_ALG_LIB
