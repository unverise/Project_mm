/******************************************************************************
 *
 * Filename:
 * ---------
 *     gc50d1mipi_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *-----------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 *
 * Version:  V20230322112910 by GC-S-TEAM
 *
 ******************************************************************************/



#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "gc50d1mipi_Sensor.h"
#include "gc50d1_sensor_setting.h"


/************************** Modify Following Strings for Debug **************************/
#define PFX "gc50d1_camera_sensor"
#define LOG_1 LOG_INF("GC50D1, MIPI 4LANE\n")
/****************************   Modify end    *******************************************/
#define GC50D1_DEBUG 1
#define CT_DEBUG     0
#if GC50D1_DEBUG
#define LOG_INF(format, args...)    pr_err(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

#define MAX_PROJECT_NAME_LEN 20
static char project_name[MAX_PROJECT_NAME_LEN];

#define GC50D1_XTALK_PAGE_SIZE 256

#define FPT_PDAF_SUPPORT
#define FPT_SEAMLESS_SUPPORT 
#define EEPROM_WRITE_ID           0xA0
#define CROSSTALK_BUF_SIZE        1568
#define CROSSTALK_START_ADDR      0x0a08
#define CROSSTALK_CHECKSUM_ADDR   0x1028
#define OTP_FLAG_EMPTY            0x00
#define OTP_FLAG_VALID            0x01
static kal_uint8 gc50d1_xtalk_data[CROSSTALK_BUF_SIZE];

// extern bool gc50d1_read_crosstalk_data(kal_uint16 addr, BYTE *data, int size);

//ITD: Modify setFPS By Thermal By Jie.liu 230517 Start
static kal_uint16 fixed_framerate;
//ITD: Modify setFPS By Thermal By Jie.liu 230517 End
static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = GC50D1_SENSOR_ID,
	.checksum_value = 0xd38815b2,
	.pre = {
		.pclk = 428000000,
		.linelength = 4224,
		.framelength = 3376,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 94,
		.mipi_pixel_rate = 819200000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 428000000,
		.linelength = 4224,
		.framelength = 3376,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 94,
		.mipi_pixel_rate = 819200000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 428000000,
		.linelength = 4224,
		.framelength = 3376,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 2304,
		.mipi_data_lp2hs_settle_dc = 94,
		.mipi_pixel_rate = 819200000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 428000000,
		.linelength = 2112,
		.framelength = 1688,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1152,
		.mipi_data_lp2hs_settle_dc = 97,
		.mipi_pixel_rate = 512000000,
		.max_framerate = 1200,
	},
	.slim_video = { //settings not support 240fps, only 120fps
		.pclk = 428000000,
		.linelength = 2112,
		.framelength = 1688,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1152,
		.mipi_data_lp2hs_settle_dc = 97,
		.mipi_pixel_rate = 512000000,
		.max_framerate = 2400,
	},
	.custom1 = {
		.pclk = 428000000,
		.linelength = 4224,
		.framelength = 3376,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 94,
		.mipi_pixel_rate = 819200000,
		.max_framerate = 300,
	},
	.custom2 = {
		.pclk = 856000000,
		.linelength = 4224,
		.framelength = 3376,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3840,
		.grabwindow_height = 2160,
		.mipi_data_lp2hs_settle_dc = 76,
		.mipi_pixel_rate = 982400000,
		.max_framerate = 600,
	},
	.custom3 = {
		.pclk = 428000000,
		.linelength = 4224,
		.framelength = 6712,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 8192,
		.grabwindow_height = 6144,
		.mipi_data_lp2hs_settle_dc = 81,
		.mipi_pixel_rate = 896000000,
		.max_framerate = 150,
	},
	.custom4 = {
		.pclk = 428000000,
		.linelength = 4224,
		.framelength = 3376,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 92,
		.mipi_pixel_rate = 819200000,
		.max_framerate = 300,
	},
	.custom5 = {
		.pclk = 428000000,
		.linelength = 4224,
		.framelength = 3376,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 92,
		.mipi_pixel_rate = 819200000,
		.max_framerate = 300,
	},
	.custom6 = {
		.pclk = 428000000,
		.linelength = 4224,
		.framelength = 3376,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 92,
		.mipi_pixel_rate = 819200000,
		.max_framerate = 300,
	},
	.custom7 = {
		.pclk = 428000000,
		.linelength = 4224,
		.framelength = 3376,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 94,
		.mipi_pixel_rate = 819200000,
		.max_framerate = 300,
	},

	.min_gain = 64,
	.max_gain = 2048,
	.min_gain_iso = 50,
	.gain_step = 1,
	.gain_type = 4,

	.margin = 32,
	.min_shutter = 4,
	.max_frame_length = 0xfffe,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 12,

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.custom3_delay_frame = 2,
	.custom4_delay_frame = 2,
	.custom5_delay_frame = 2,
	.custom6_delay_frame = 2,
	.custom7_delay_frame = 2,
	.frame_time_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gb,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20, 0xff},
	.i2c_speed = 1000,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x900,
	.gain = 0x40,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = 0,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x20,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[12] = {
	{ 8192, 6144, 0,   0, 8192, 6144, 4096, 3072,   0,   0, 4096, 3072, 0, 0, 4096, 3072}, /* Preview */
	{ 8192, 6144, 0,   0, 8192, 6144, 4096, 3072,   0,   0, 4096, 3072, 0, 0, 4096, 3072}, /* capture */
	{ 8192, 6144, 0,   0, 8192, 6144, 4096, 3072,   0, 384, 4096, 2304, 0, 0, 4096, 2304}, /* video */
	{ 8192, 6144, 0, 768, 8192, 4608, 2048, 1152,   0,   0, 2048, 1152, 0, 0, 2048, 1152}, /* hs video */
	{ 8192, 6144, 0, 768, 8192, 4608, 2048, 1152,   0,   0, 2048, 1152, 0, 0, 2048, 1152}, /* slim video */
	{ 8192, 6144, 0,   0, 8192, 6144, 4096, 3072,   0,   0, 4096, 3072, 0, 0, 4096, 3072}, /* custom1 */
	{ 8192, 6144, 0,   0, 8192, 6144, 4096, 3072, 128, 456, 3840, 2160, 0, 0, 3840, 2160}, /* custom2 */
	{ 8192, 6144, 0,   0, 8192, 6144, 8192, 6144,   0,   0, 8192, 6144, 0, 0, 8192, 6144}, /* custom3 */
	{ 8192, 6144, 2048, 1536, 4096, 3072, 4096, 3072, 0,  0, 4096, 3072, 0, 0, 4096, 3072},  /* custom4 */ 
	{ 8192, 6144, 2048, 1536, 4096, 3072, 4096, 3072, 0,  0, 4096, 3072, 0, 0, 4096, 3072},  /* custom5 */
	{ 8192, 6144, 2048, 1536, 4096, 3072, 4096, 3072, 0,  0, 4096, 3072, 0, 0, 4096, 3072},  /* custom6 */
	{ 8192, 6144, 0,   0, 8192, 6144, 4096, 3072, 416, 312, 3264, 2448, 0, 0, 3264, 2448}, /* custom7 */
};

#ifdef FPT_PDAF_SUPPORT
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info[5] = {
	/* raw size:4096*3072 pd binning size:4096x768 */
	{
		.i4OffsetX = 0,
		.i4OffsetY = 0,
		.i4PitchX  = 0,
		.i4PitchY  = 0,
		.i4PairNum  =0,
		.i4SubBlkW  =0,
		.i4SubBlkH  =0,
		.i4PosL = {{0,0},{0,0},{0,0},{0,0}},
		.i4PosR = {{0,0},{0,0},{0,0},{0,0}},
		.i4BlockNumX = 0,
		.i4BlockNumY = 0,
		.iMirrorFlip = 3,
		.i4Crop = { {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {2048,1536}, {416, 312} },
	},
	/* raw size:4096*3072 pd centercrop size:2048x768 */
	{
		.i4OffsetX = 0,
		.i4OffsetY = 0,
		.i4PitchX  = 0,
		.i4PitchY  = 0,
		.i4PairNum  =0,
		.i4SubBlkW  =0,
		.i4SubBlkH  =0,
		.i4PosL = {{0,0},{0,0},{0,0},{0,0}},
		.i4PosR = {{0,0},{0,0},{0,0},{0,0}},
		.i4BlockNumX = 0,
		.i4BlockNumY = 0,
		.iMirrorFlip = 3,
		.i4Crop = { {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {2048,1536}, {2048,1536}, {2048,1536}, {416, 312} },
	},
	/* raw size:4096*2304 pd size:4096x576 */
	{
		.i4OffsetX = 0,
		.i4OffsetY = 0,
		.i4PitchX  = 0,
		.i4PitchY  = 0,
		.i4PairNum  =0,
		.i4SubBlkW  =0,
		.i4SubBlkH  =0,
		.i4PosL = {{0,0},{0,0},{0,0},{0,0}},
		.i4PosR = {{0,0},{0,0},{0,0},{0,0}},
		.i4BlockNumX = 0,
		.i4BlockNumY = 0,
		.iMirrorFlip = 3,
		.i4Crop = { {0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {2048,1536}, {416, 312} },
	},
	/* raw size:3840*2160 pd size:480x536 */
	{
		.i4OffsetX = 32,
		.i4OffsetY = 30,
		.i4PitchX  = 16,
		.i4PitchY  = 16,
		.i4PairNum	=4,
		.i4SubBlkW	=8,
		.i4SubBlkH	=8,
		.i4PosL = {{34,35},{46,35},{38,43},{42,43}},
		.i4PosR = {{34,31},{46,31},{38,39},{42,39}},
		.i4BlockNumX = 240,
		.i4BlockNumY = 134,
		.iMirrorFlip = 3,
		.i4Crop = { {0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 0}, {0, 0}, {128, 456}, {0, 0}, {0, 0}, {0, 0}, {2048,1536}, {416, 312} },
	},
	/* raw size:3264*2448 pd size:3264x612 */
	{
		.i4OffsetX = 0,
		.i4OffsetY = 0,
		.i4PitchX  = 0,
		.i4PitchY  = 0,
		.i4PairNum  =0,
		.i4SubBlkW  =0,
		.i4SubBlkH  =0,
		.i4PosL = {{0,0},{0,0},{0,0},{0,0}},
		.i4PosR = {{0,0},{0,0},{0,0},{0,0}},
		.i4BlockNumX = 0,
		.i4BlockNumY = 0,
		.iMirrorFlip = 3,
		.i4Crop = { {0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {2048,1536}, {416, 312} },
	}
};

static struct SENSOR_VC_INFO2_STRUCT SENSOR_VC_INFO2[5] = {
	/* raw size:4096*3072 pd binning size:4096*768 */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x1000, 0x0C00},
			{VC_PDAF_STATS, 0x02, 0x30, 0x1400, 0x0300},
		},
		1
	},
	/* raw size:4096*3072 pd centercrop size:2048*768 */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x1000, 0x0C00},
			{VC_PDAF_STATS, 0x02, 0x30, 0x0A00, 0x0300},//dt=0x30  sizh=2048*10/8=2560
		},
		1
	},
	/* raw size:4096*2304 pd size:4096*576 */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x1000, 0x0900},
			{VC_PDAF_STATS, 0x02, 0x30, 0x1400, 0x0240},
		},
		1
	},
	/* raw size:3840*2160 pd size:480*536 */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x0F00, 0x0870},
			{VC_PDAF_STATS, 0x02, 0x30, 0x0258, 0x0218},
		},
		1
	},
	/* raw size:3264*2448 pd size:3264x612 */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0xCC0, 0x0990},
			{VC_PDAF_STATS, 0x02, 0x30, 0xFF0, 0x0264},
		},
		1
	},

};
#endif

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {
		(char)((addr >> 8) & 0xff),
		(char)(addr & 0xff)
	};

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}
static kal_uint16 read_eeprom(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xff) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, EEPROM_WRITE_ID);
	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {
		(char)((addr >> 8) & 0xff),
		(char)(addr & 0xff),
		(char)((para >> 8) & 0xff),
		(char)(para & 0xff)
	};

	iWriteRegI2CTiming(pu_send_cmd, 4, imgsensor.i2c_write_id,1000);
}

static void write_cmos_sensor_8bit(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {
		(char)((addr >> 8) & 0xff),
		(char)(addr & 0xff),
		(char)(para & 0xff)
	};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

#if 0
static void table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend = 0, idx = 0;
	kal_uint16 addr = 0, data = 0;

	while (len > idx) {
		addr = para[idx];
		puSendCmd[tosend++] = (char)((addr >> 8) & 0xff);
		puSendCmd[tosend++] = (char)(addr & 0xff);
		data = para[idx + 1];
		puSendCmd[tosend++] = (char)(data & 0xff);
		idx += 2;
#if MULTI_WRITE
		if (tosend >= I2C_BUFFER_LEN || idx == len) {
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id,
					3, imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2CTiming(puSendCmd, 3, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
		tosend = 0;
#endif
	}
}
#endif

#define I2C_BURST_BUFFER_LEN 1024
static kal_uint16 burst_write_cmos_sensor(
	kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[1020];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;
	tosend = 0;
	IDX = 0;
	while (len > IDX) {
		addr = para[IDX];
		if(tosend == 0)
		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;
		}
		else if ( addr == addr_last + 1)
		{
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;
		}
		else
		{
			if(tosend > 3)
				goto write;
//			LOG_INF("IDX %d, tosend %d ,addr_last 0x%x,addr 0x%x\n", IDX, tosend, addr_last, addr);
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id,
					3, imgsensor_info.i2c_speed);
			tosend = 0;
			continue;
		}

write:
#if MULTI_WRITE
		if (tosend >= I2C_BURST_BUFFER_LEN || IDX == len || addr != addr_last) {
//			 pr_err("IDX %d, tosend %d ,addr_last 0x%x,addr 0x%x\n", IDX, tosend, addr_last, addr);
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id, tosend, imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2CTiming(puSendCmd, 3, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
		tosend = 0;
#endif
	}
	return 0;
}

bool gc50d1_read_crosstalk_data(kal_uint16 addr, BYTE *data, int size)
{
	kal_uint16 i = 0;
	kal_uint16 ct_flag = 0;
	kal_uint32 checksum = 0;
    LOG_INF("WYT start read crosstalk");
	memset(data, 0, size * sizeof(kal_uint8));
	// ct_flag = read_eeprom(addr);
	// pr_err("ct_flag = %d\n", ct_flag);
	ct_flag = 1;
	if (ct_flag == OTP_FLAG_VALID) {
		for (i = 0; i < size; i++){
			data[i] = read_eeprom(addr + i);
			checksum += data[i];
		}
		if ((checksum % 256) == read_eeprom(CROSSTALK_CHECKSUM_ADDR))
			pr_err("check success! calc_checksum = %d, read_checksum = %d\n", (checksum % 256), read_eeprom(addr + size));
		else {
			memset(data, 0, size);
			pr_err("check error! calc_checksum = %d, read_checksum = %d\n", (checksum % 256), read_eeprom(addr + size));
			return false;
		}
#if CT_DEBUG
		for (i = 0; i < size; i++)
			LOG_INF("ct_data[%d] = %x\n", i, data[i]);
#endif
	} else{
		LOG_INF("crosstalk data is zero!\n");
		return false;
	}
	return true;
}

static void gc50d1_write_crosstalk_data(kal_uint8 *data,int len)
{
	kal_uint32 i = 0, page = 0, j = 0; 
	kal_uint32 start_page = 0x06, start_addr = 0x0600;
	kal_uint16 gc50d1_xtc_addr_data[GC50D1_XTALK_PAGE_SIZE * 2];

	write_cmos_sensor_8bit(0x031c, 0x60);
	write_cmos_sensor_8bit(0x0004, 0x06);
	write_cmos_sensor_8bit(0x001f, 0x01);
	mdelay(5);
	LOG_INF("start write crosstalk_data!\n");
	
	
	for (page = 0;  page < ((len/GC50D1_XTALK_PAGE_SIZE+1)); page++)
	{
	if (6 == page)
        {
            write_cmos_sensor_8bit(0x0004, start_page+0x10*page);
            for (i = 0; i < 32 ; i++)
            {
                gc50d1_xtc_addr_data[2 * i] = start_addr + i;
                gc50d1_xtc_addr_data[2 * i + 1] = data[j];
                j++;				
            }            
            burst_write_cmos_sensor(gc50d1_xtc_addr_data, 32 * 2);
            //for (i = 0; i < 32 ; i++)
            //{
            //    LOG_INF("zzzXTCdata[%d] = 0x%x\n",start_addr+i,read_cmos_sensor(start_addr+i));
            //}

        	LOG_INF("page = %x\n", read_cmos_sensor(0x0004) );	
        	LOG_INF("sram_i = %d\n", i );		
        	LOG_INF("CTdata_j = %d\n", j );			
        	LOG_INF("XTCdata = 0x%x\n",read_cmos_sensor(0x0600));
        	LOG_INF("XTCdata = 0x%x\n",read_cmos_sensor(0x061f));
        	LOG_INF("XTCdata = 0x%x\n",read_cmos_sensor(0x06ff));		
        }
        else
        {
            write_cmos_sensor_8bit(0x0004, start_page+0x10*page);
            for (i = 0; i < GC50D1_XTALK_PAGE_SIZE ; i++)
            {
                gc50d1_xtc_addr_data[2 * i] = start_addr + i;
                gc50d1_xtc_addr_data[2 * i + 1] = data[j];
                j++;
            }
            burst_write_cmos_sensor(gc50d1_xtc_addr_data, GC50D1_XTALK_PAGE_SIZE*2);
//			for (i = 0; i < GC50D1_XTALK_PAGE_SIZE ; i++)
//			{
//                LOG_INF("zzzXTCdata[%d] = 0x%x\n",start_addr+i,read_cmos_sensor(start_addr+i));
//            }
			
            LOG_INF("page = %x\n", read_cmos_sensor(0x0004) );	
            LOG_INF("sram_i = %d\n", i );		
            LOG_INF("CTdata_j = %d\n", j );			
            LOG_INF("XTCdata = 0x%x\n",read_cmos_sensor(0x0600));
            LOG_INF("XTCdata = 0x%x\n",read_cmos_sensor(0x06ff));		
		}
	}
	write_cmos_sensor_8bit(0x031c, 0x9b);	
	write_cmos_sensor_8bit(0x0004, 0x07);

	LOG_INF("end write crosstalk_data!\n");
}

static kal_uint32 return_sensor_id(void)
{
	kal_uint32 sensor_id = 0;

	sensor_id = (read_cmos_sensor(0x03f0) << 8) | read_cmos_sensor(0x03f1);
	return sensor_id;
}

static void set_dummy(void)
{
	LOG_INF("frame length = %d\n", imgsensor.frame_length);
	write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);
}

static void set_max_framerate(kal_uint16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
		frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}

static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	static bool bNeedSetNormalMode = false;
	kal_uint32 cal_shutter = 0;
	kal_uint16 long_exp_h = 0;
	kal_uint16 long_exp_m = 0;
	kal_uint16 long_exp_l = 0;

	/*kal_uint32 frame_length = 0;*/
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

	if (shutter >= 0x76bd) {  //300ms
		bNeedSetNormalMode = true;
		cal_shutter = shutter - 0x2794;//100ms
		long_exp_h = (cal_shutter >> 16) & 0x3F;
		long_exp_m = (cal_shutter >> 8) & 0xFF;
		long_exp_l = cal_shutter & 0xFF;
		write_cmos_sensor_8bit(0x0202, 0x27);
		write_cmos_sensor_8bit(0x0203, 0x94);
		write_cmos_sensor_8bit(0x0340, 0x27);
		write_cmos_sensor_8bit(0x0341, 0xa4);
		write_cmos_sensor_8bit(0x0230, 0x30);
		write_cmos_sensor_8bit(0x0232, long_exp_h);
		write_cmos_sensor_8bit(0x0233, long_exp_m);
		write_cmos_sensor_8bit(0x0234, long_exp_l);
		pr_info("shutter = %d, cal_shutter = %d, exp_h = %d, exp_m = %d, exp_l = %d", shutter, cal_shutter, long_exp_h, long_exp_m, long_exp_l);
	} else {
		if (bNeedSetNormalMode) {
			write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);
			write_cmos_sensor(0x0202, shutter);
			write_cmos_sensor_8bit(0x0230, 0x20);
			write_cmos_sensor_8bit(0x0232, 0x00);
			write_cmos_sensor_8bit(0x0233, 0x00);
			write_cmos_sensor_8bit(0x0234, 0x00);
			bNeedSetNormalMode = false;
			pr_info("exit long exposure, shutter = %d", shutter);
		}

		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

		if (imgsensor.autoflicker_en) {
			if (realtime_fps >= 297 && realtime_fps <= 305)
				set_max_framerate(296, 0);
			else if (realtime_fps >= 147 && realtime_fps <= 150)
				set_max_framerate(146, 0);
			else if (realtime_fps >= 593 && realtime_fps <= 607)
				set_max_framerate(592, 0);
			else
				write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);
		} else
			write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);

		/* Update Shutter */
		write_cmos_sensor(0x0202, shutter);
	}
	LOG_INF("shutter = %d, framelength = %d\n",
		shutter, imgsensor.frame_length);
}
static void set_shutter_frame_length(
				kal_uint16 shutter, kal_uint16 frame_length,
				kal_bool auto_extend_en)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	LOG_INF(
		"set_shutter_frame_length+ shutter =%d, framelength =%d\n",
		shutter, frame_length);
	spin_lock(&imgsensor_drv_lock);
	/*Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;
	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter =
	(shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else if (realtime_fps >= 593 && realtime_fps <= 607)
			set_max_framerate(592, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	}
	/* Update Shutter */
	write_cmos_sensor(0x0202, shutter & 0xFFFF);
	LOG_INF(
		"set_shutter_frame_length- shutter =%d, framelength =%d/%d, dummy_line=%d\n",
		shutter, imgsensor.frame_length,
		frame_length, dummy_line);
}	/* set_shutter_frame_length */
static kal_uint32 gain2reg(kal_uint32 gain)
{
	kal_uint32 reg_gain = gain * 128;
	LOG_INF(" sensor_base_gain = %d\n", SENSOR_BASE_GAIN);
	LOG_INF(" gain = %d\n", gain);
	LOG_INF(" reg_gain = %d\n", reg_gain);
	reg_gain = (reg_gain < SENSOR_BASE_GAIN) ? SENSOR_BASE_GAIN : reg_gain;
	if(imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM3)
		reg_gain = (reg_gain > 8 * SENSOR_BASE_GAIN) ? (8 * SENSOR_BASE_GAIN) : reg_gain;
	else if (imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM4 || imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM5 || imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM6)
	{
		reg_gain = (reg_gain > 16 * SENSOR_BASE_GAIN) ? (16 * SENSOR_BASE_GAIN) : reg_gain;
	} else
		reg_gain = (reg_gain > 32 * SENSOR_BASE_GAIN) ? (32 * SENSOR_BASE_GAIN) : reg_gain;

	return reg_gain;
}


static void set_mirror_flip(kal_uint8 image_mirror)
{
	pr_err("image_mirror = %d\n", image_mirror);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.mirror = image_mirror;
	spin_unlock(&imgsensor_drv_lock);
	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor_8bit(0x0101, 0x00); //GR
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor_8bit(0x0101, 0x01); //R
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor_8bit(0x0101, 0x02); //B
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8bit(0x0101, 0x03); //GB
		break;
	default:
		pr_err("Error image_mirror setting\n");
	}
}

static kal_uint32 set_gain(kal_uint32 gain)
{
	kal_uint32 reg_gain = 0;
	kal_uint32 gain_h = 0;
	kal_uint32 gain_m = 0;
	kal_uint32 gain_l = 0;
	LOG_INF(" basegain = %d\n", BASEGAIN);
	LOG_INF(" gain = %d\n", gain);
	reg_gain = gain2reg(gain);

	spin_lock(&imgsensor_drv_lock);
	gain_h = (reg_gain >> 16) & 0x7;
	gain_m = (reg_gain >> 8) & 0xff;
	gain_l = reg_gain & 0xff;
	spin_unlock(&imgsensor_drv_lock);

	LOG_INF("gain = %d, reg_gain = %d, gain_h = %d, gain_m = %d, gian_l = %d\n", gain, reg_gain, gain_h, gain_m, gain_l);
	write_cmos_sensor_8bit(0x0204, gain_h);
	write_cmos_sensor_8bit(0x0205, gain_m);
	write_cmos_sensor_8bit(0x0206, gain_l);

	return gain;
}

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le: 0x%x, se: 0x%x, gain: 0x%x\n", le, se, gain);
}

static void night_mode(kal_bool enable)
{
	/* No Need to implement this function */
}

static void check_streamoff(void)
{
	unsigned int i = 0;
	int timeout = (10000 / imgsensor.current_fps) + 1;
	mdelay(3);
	for (i = 0; i < timeout; i++) {
		if (read_cmos_sensor(0x0031c) != 0x60)
			mdelay(1);
		else
			break;
	}
	LOG_INF("check_streamoff i = %d ,%x\n",i,read_cmos_sensor(0x0031c));
	LOG_INF("%s exit!\n", __func__);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("zzz streaming_enable(0=Sw Standby,1=streaming): %d %x\n", enable,read_cmos_sensor(0x0031c));
	
	if (enable)
		write_cmos_sensor_8bit(0x0100, 0x01); // stream on
	else
	{
		write_cmos_sensor_8bit(0x0100, 0x80); // stream off
		check_streamoff();
	}
	mdelay(5);
	return ERROR_NONE;
}

static void sensor_init(void)
{
	burst_write_cmos_sensor(gc50d1_init_addr_data,
		sizeof(gc50d1_init_addr_data)/sizeof(kal_uint16));
    //ITD:add project name by yutao.wu 20250826 start
	 if (project_name[0] != '\0' && (strcmp((const char *)project_name, "tran_lk7k") == 0 )) {
		set_mirror_flip(IMAGE_NORMAL);
	}
	else if(project_name[0] != '\0' && (strcmp((const char *)project_name, "tran_lk6") == 0 )){
		set_mirror_flip(IMAGE_HV_MIRROR);
	}
	else{
		set_mirror_flip(imgsensor.mirror);
	}
    //ITD:add project name by yutao.wu 20250826 end

}

static void preview_setting(void)
{
	burst_write_cmos_sensor(gc50d1_pre_addr_data,
		sizeof(gc50d1_pre_addr_data)/sizeof(kal_uint16));
}

static void capture_setting(void)
{
	burst_write_cmos_sensor(gc50d1_cap_addr_data,
		sizeof(gc50d1_cap_addr_data)/sizeof(kal_uint16));
}

static void normal_video_setting(void)
{
	burst_write_cmos_sensor(gc50d1_video_addr_data,
		sizeof(gc50d1_video_addr_data)/sizeof(kal_uint16));
}

static void hs_video_setting(void)
{
	burst_write_cmos_sensor(gc50d1_hs_video_addr_data,
		sizeof(gc50d1_hs_video_addr_data)/sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	burst_write_cmos_sensor(gc50d1_slim_video_addr_data,
		sizeof(gc50d1_slim_video_addr_data)/sizeof(kal_uint16));
}

static void custom1_setting(void)
{
	burst_write_cmos_sensor(gc50d1_cus1_addr_data,
		sizeof(gc50d1_cus1_addr_data)/sizeof(kal_uint16));
}

static void custom2_setting(void)
{
	burst_write_cmos_sensor(gc50d1_cus2_addr_data,
		sizeof(gc50d1_cus2_addr_data)/sizeof(kal_uint16));
}

static void custom3_setting(void)
{
	burst_write_cmos_sensor(gc50d1_cus3_addr_data,
		sizeof(gc50d1_cus3_addr_data)/sizeof(kal_uint16));
}

static void custom4_setting(void)
{
	burst_write_cmos_sensor(gc50d1_cus4_addr_data,
		sizeof(gc50d1_cus4_addr_data)/sizeof(kal_uint16));
}

static void custom5_setting(void)
{
	burst_write_cmos_sensor(gc50d1_cus5_addr_data,
		sizeof(gc50d1_cus5_addr_data)/sizeof(kal_uint16));
}

static void custom6_setting(void)
{
	burst_write_cmos_sensor(gc50d1_cus6_addr_data,
		sizeof(gc50d1_cus6_addr_data)/sizeof(kal_uint16));
}

static void custom7_setting(void)
{
	burst_write_cmos_sensor(gc50d1_3264_2448_30fps_data,
		sizeof(gc50d1_3264_2448_30fps_data)/sizeof(kal_uint16));
}

static kal_uint32 set_test_pattern_mode(kal_uint32 modes, struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
	LOG_INF("enable: %d\n", modes);

	if (modes == 2)
	{
		write_cmos_sensor_8bit(0x035d, 0x01);
	}
	else if (modes == 5 && (pdata != NULL)) {
			write_cmos_sensor_8bit(0x035e, 0x00);
			write_cmos_sensor_8bit(0x035d, 0x01);
	}
	else
		write_cmos_sensor_8bit(0x035d, 0x00);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = modes;
	LOG_INF("final set_test_pattern modes: %d\n", modes);
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

//ITD:add get chip id by haiyang.he 20250829 start
#define CHIP_ID_SIZE 12
static unsigned char gc50d1_otp_read_byte(u16 addr)
{
  write_cmos_sensor_8bit(0x0a69, (addr >> 8) & 0xff);
  write_cmos_sensor_8bit(0x0a6a, addr & 0xff);
  write_cmos_sensor_8bit(0x0a66, 0x20);
  return (kal_uint8)read_cmos_sensor(0x0a6c);
}

static kal_uint32 gc50d1_get_chip_id(unsigned char *chip_id)
{
  int i = 0;
  unsigned char chipIdData[32];

  write_cmos_sensor_8bit(0x031c, 0x60);
  write_cmos_sensor_8bit(0x0a67, 0x80);
  write_cmos_sensor_8bit(0x0a70, 0x80);
  write_cmos_sensor_8bit(0x0a66, 0x00);
  write_cmos_sensor_8bit(0x0a51, 0x00);
  write_cmos_sensor_8bit(0x0a56, 0x04);
  write_cmos_sensor_8bit(0x0a78, 0x00);
  write_cmos_sensor_8bit(0x0a8a, 0x02);
  write_cmos_sensor_8bit(0x0a8b, 0x01);
  write_cmos_sensor_8bit(0x0a74, 0x17);
  write_cmos_sensor_8bit(0x0a75, 0x03);
  write_cmos_sensor_8bit(0x0a76, 0x00);
  write_cmos_sensor_8bit(0x0a68, 0x04);
  write_cmos_sensor_8bit(0x0a6f, 0x04);
  write_cmos_sensor_8bit(0x0a52, 0x0c);
  write_cmos_sensor_8bit(0x0a4e, 0x40);

  mdelay(10);

  for (i=0; i<CHIP_ID_SIZE; i++)
  {
    chipIdData[i] = gc50d1_otp_read_byte(0x20 + 8 * i);
    sprintf(chip_id+2*i, "%02X",chipIdData[i]);
   // pr_info("lot id i::%d is %02X", i, chipIdData[i]);
  }
  pr_info("get_chip_id chip id:%s", chip_id);

  return ERROR_NONE;
}
//ITD:add get chip id by haiyang.he 20250829 end

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_info("[gc50d1_camera_sensor]get_imgsensor_id:i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				gc50d1_read_crosstalk_data(CROSSTALK_START_ADDR, &gc50d1_xtalk_data[0], CROSSTALK_BUF_SIZE);
				return ERROR_NONE;
			}
			pr_info("[gc50d1_camera_sensor]get_imgsensor_id:Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}


	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;
	// kal_uint16 ct_flag = 0;
	LOG_1;
	LOG_INF("imgsensor_open\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_info("[gc50d1_camera_sensor]open:i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			pr_info("[gc50d1_camera_sensor]open:Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}

	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();
	if(gc50d1_xtalk_data[0] != 0)
		gc50d1_write_crosstalk_data(&gc50d1_xtalk_data[0],CROSSTALK_BUF_SIZE);

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = 0;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 close(void)
{
	LOG_INF("E\n");
	/* No Need to implement this function */
	return ERROR_NONE;
}

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/*imgsensor.current_fps = 300*/
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	custom4_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	custom5_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 custom6(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM6;
	imgsensor.pclk = imgsensor_info.custom6.pclk;
	imgsensor.line_length = imgsensor_info.custom6.linelength;
	imgsensor.frame_length = imgsensor_info.custom6.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom6.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	custom6_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 custom7(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM7;
	imgsensor.pclk = imgsensor_info.custom7.pclk;
	imgsensor.line_length = imgsensor_info.custom7.linelength;
	imgsensor.frame_length = imgsensor_info.custom7.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom7.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	custom7_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static void setGPHon(kal_bool enable)
{
	if(enable)
	{
		write_cmos_sensor_8bit(0x01c0, 0x11); // group hold on	
	}else
	{
		write_cmos_sensor_8bit(0x01c0, 0x01); 
		write_cmos_sensor_8bit(0x01c0, 0x61); 
	}
}
#ifdef FPT_SEAMLESS_SUPPORT 
enum {
    SHUTTER_NE_FRM_1 = 0,
    GAIN_NE_FRM_1,
    FRAME_LEN_NE_FRM_1,
    HDR_TYPE_FRM_1,
    SHUTTER_NE_FRM_2,
    GAIN_NE_FRM_2,
    FRAME_LEN_NE_FRM_2,
    HDR_TYPE_FRM_2,
    SHUTTER_SE_FRM_1,
    GAIN_SE_FRM_1,
    SHUTTER_SE_FRM_2,
    GAIN_SE_FRM_2,
    SHUTTER_ME_FRM_1,
    GAIN_ME_FRM_1,
    SHUTTER_ME_FRM_2,
    GAIN_ME_FRM_2,
};

static kal_uint32 seamless_switch(enum MSDK_SCENARIO_ID_ENUM scenario_id, uint32_t *ae_ctrl)
{
	LOG_INF("seamless switch to %d !\n", scenario_id);
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		{
			spin_lock(&imgsensor_drv_lock);
			imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
			imgsensor.pclk = imgsensor_info.pre.pclk;
			imgsensor.line_length = imgsensor_info.pre.linelength;
			imgsensor.frame_length = imgsensor_info.pre.framelength;
			imgsensor.min_frame_length = imgsensor_info.pre.framelength;
			imgsensor.autoflicker_en = KAL_FALSE;
			//imgsensor.extend_frame_length_en = KAL_FALSE;
			spin_unlock(&imgsensor_drv_lock);

			burst_write_cmos_sensor(gc50d1_pre_addr_data,
				sizeof(gc50d1_pre_addr_data) / sizeof(kal_uint16));
			setGPHon(KAL_TRUE);
			if (ae_ctrl) {
				LOG_INF("call SENSOR_SCENARIO_ID_NORMAL_CAPTURE %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1]);
				set_shutter(ae_ctrl[SHUTTER_NE_FRM_1]);
				set_gain(ae_ctrl[GAIN_NE_FRM_1]);
			}
            setGPHon(KAL_FALSE);
		}
		break;

		case MSDK_SCENARIO_ID_CUSTOM1:
		{
			LOG_INF("seamless switch to CUSTOM1 size!\n");
			spin_lock(&imgsensor_drv_lock);
			imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
			imgsensor.pclk = imgsensor_info.custom1.pclk;
			imgsensor.line_length = imgsensor_info.custom1.linelength;
			imgsensor.frame_length = imgsensor_info.custom1.framelength;
			imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
			imgsensor.autoflicker_en = KAL_FALSE;
			//imgsensor.extend_frame_length_en = KAL_FALSE;
			spin_unlock(&imgsensor_drv_lock);

			burst_write_cmos_sensor(gc50d1_cus1_addr_data,
				sizeof(gc50d1_cus1_addr_data) / sizeof(kal_uint16));
			setGPHon(KAL_TRUE);
			if (ae_ctrl) {
				LOG_INF("call SENSOR_SCENARIO_ID_CUSTOM1 %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1],ae_ctrl[SHUTTER_SE_FRM_1],ae_ctrl[GAIN_SE_FRM_1]);
				set_shutter(ae_ctrl[SHUTTER_NE_FRM_1]);
				set_gain(ae_ctrl[GAIN_NE_FRM_1]);
			}
            setGPHon(KAL_FALSE);
		}
		break;

		case MSDK_SCENARIO_ID_CUSTOM4:
		{
			LOG_INF("seamless switch to CUSTOM4 size!\n");
			spin_lock(&imgsensor_drv_lock);
			imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
			imgsensor.pclk = imgsensor_info.custom4.pclk;
			imgsensor.line_length = imgsensor_info.custom4.linelength;
			imgsensor.frame_length = imgsensor_info.custom4.framelength;
			imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
			imgsensor.autoflicker_en = KAL_FALSE;
			//imgsensor.extend_frame_length_en = KAL_FALSE;
			spin_unlock(&imgsensor_drv_lock);

			burst_write_cmos_sensor(gc50d1_cus4_addr_data,
				sizeof(gc50d1_cus4_addr_data) / sizeof(kal_uint16));
			setGPHon(KAL_TRUE);
			if (ae_ctrl) {
				LOG_INF("call SENSOR_SCENARIO_ID_CUSTOM4 %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1],ae_ctrl[SHUTTER_SE_FRM_1],ae_ctrl[GAIN_SE_FRM_1]);
				set_shutter(ae_ctrl[SHUTTER_NE_FRM_1]);
				set_gain(ae_ctrl[GAIN_NE_FRM_1]);
			}
            setGPHon(KAL_FALSE);
		}
		break;

		case MSDK_SCENARIO_ID_CUSTOM5:
		{
			LOG_INF("seamless switch to CUSTOM5 size!\n");
			spin_lock(&imgsensor_drv_lock);
			imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
			imgsensor.pclk = imgsensor_info.custom5.pclk;
			imgsensor.line_length = imgsensor_info.custom5.linelength;
			imgsensor.frame_length = imgsensor_info.custom5.framelength;
			imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
			imgsensor.autoflicker_en = KAL_FALSE;
			//imgsensor.extend_frame_length_en = KAL_FALSE;
			spin_unlock(&imgsensor_drv_lock);

			burst_write_cmos_sensor(gc50d1_cus5_addr_data,
				sizeof(gc50d1_cus5_addr_data) / sizeof(kal_uint16));
			setGPHon(KAL_TRUE);
			if (ae_ctrl) {
				LOG_INF("call SENSOR_SCENARIO_ID_CUSTOM5 %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1],ae_ctrl[SHUTTER_SE_FRM_1],ae_ctrl[GAIN_SE_FRM_1]);
				set_shutter(ae_ctrl[SHUTTER_NE_FRM_1]);
				set_gain(ae_ctrl[GAIN_NE_FRM_1]);
			}
            setGPHon(KAL_FALSE);
		}
		break;

		case MSDK_SCENARIO_ID_CUSTOM6:
		{
			LOG_INF("seamless switch to CUSTOM6 size!\n");
			spin_lock(&imgsensor_drv_lock);
			imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM6;
			imgsensor.pclk = imgsensor_info.custom6.pclk;
			imgsensor.line_length = imgsensor_info.custom6.linelength;
			imgsensor.frame_length = imgsensor_info.custom6.framelength;
			imgsensor.min_frame_length = imgsensor_info.custom6.framelength;
			imgsensor.autoflicker_en = KAL_FALSE;
			//imgsensor.extend_frame_length_en = KAL_FALSE;
			spin_unlock(&imgsensor_drv_lock);

			burst_write_cmos_sensor(gc50d1_cus6_addr_data,
				sizeof(gc50d1_cus6_addr_data) / sizeof(kal_uint16));
			setGPHon(KAL_TRUE);
			if (ae_ctrl) {
				LOG_INF("call SENSOR_SCENARIO_ID_CUSTOM6 %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1],ae_ctrl[SHUTTER_SE_FRM_1],ae_ctrl[GAIN_SE_FRM_1]);
				set_shutter(ae_ctrl[SHUTTER_NE_FRM_1]);
				set_gain(ae_ctrl[GAIN_NE_FRM_1]);
			}
            setGPHon(KAL_FALSE);
		}
		break;

		default:
		{
			LOG_INF(
			"error! wrong setting in set_seamless_switch = %d",
			scenario_id);
			return 0xff;
		}
	}

	return 0;
}
#endif

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;
	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;
	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;
	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;
	sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;
	sensor_resolution->SensorCustom2Width = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;
	sensor_resolution->SensorCustom3Width = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height = imgsensor_info.custom3.grabwindow_height;
	sensor_resolution->SensorCustom4Width = imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height = imgsensor_info.custom4.grabwindow_height;
	sensor_resolution->SensorCustom5Width = imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height = imgsensor_info.custom5.grabwindow_height;
	sensor_resolution->SensorCustom6Width = imgsensor_info.custom6.grabwindow_width;
	sensor_resolution->SensorCustom6Height = imgsensor_info.custom6.grabwindow_height;
	sensor_resolution->SensorCustom7Width = imgsensor_info.custom7.grabwindow_width;
	sensor_resolution->SensorCustom7Height = imgsensor_info.custom7.grabwindow_height;

	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; /* inverse with datasheet */
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;

    //ITD:add project name by yutao.wu 20250826 start
	if(strcmp((const char *)project_name, "tran_lk7k") == 0)
		imgsensor_info.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr;
	else if(strcmp((const char *)project_name, "tran_lk6") == 0)
	    imgsensor_info.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gb;
    //ITD:add project name by yutao.wu 20250826 end

	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;
	sensor_info->Custom6DelayFrame = imgsensor_info.custom6_delay_frame;
	sensor_info->Custom7DelayFrame = imgsensor_info.custom7_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

#ifdef FPT_PDAF_SUPPORT
	sensor_info->PDAF_Support = (scenario_id == MSDK_SCENARIO_ID_CUSTOM2) ? PDAF_SUPPORT_CAMSV : PDAF_SUPPORT_CAMSV_QPD;
#else
	sensor_info->PDAF_Support = PDAF_SUPPORT_NA;
#endif

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		sensor_info->SensorGrabStartX = imgsensor_info.custom6.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom6.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom6.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM7:
		sensor_info->SensorGrabStartX = imgsensor_info.custom7.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom7.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom7.mipi_data_lp2hs_settle_dc;
		break;

	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		custom3(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		custom4(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		custom5(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		custom6(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM7:
		custom7(image_window, sensor_config_data);
		break;

	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	/*This Function not used after ROME*/
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	/***********
	 *if (framerate == 0)	 //Dynamic frame rate
	 *	return ERROR_NONE;
	 *spin_lock(&imgsensor_drv_lock);
	 *if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
	 *	imgsensor.current_fps = 296;
	 *else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
	 *	imgsensor.current_fps = 146;
	 *else
	 *	imgsensor.current_fps = framerate;
	 *spin_unlock(&imgsensor_drv_lock);
	 *set_max_framerate(imgsensor.current_fps, 1);
	 ********/
	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else /* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
//ITD: Modify setFPS By Thermal By Jie.liu 230808 Start
	fixed_framerate = framerate;
//ITD: Modify setFPS By Thermal By Jie.liu 230808 End
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ?
			(frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate, imgsensor_info.cap.max_framerate / 10);
		frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ?
			(frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ?
			(frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ?
			(frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ?
			(frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ?
			(frame_length - imgsensor_info.custom3.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10 / imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom4.framelength) ?
			(frame_length - imgsensor_info.custom4.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk / framerate * 10 / imgsensor_info.custom5.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom5.framelength) ?
			(frame_length - imgsensor_info.custom5.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom5.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		frame_length = imgsensor_info.custom6.pclk / framerate * 10 / imgsensor_info.custom6.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom6.framelength) ?
			(frame_length - imgsensor_info.custom6.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom6.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM7:
		frame_length = imgsensor_info.custom7.pclk / framerate * 10 / imgsensor_info.custom7.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom7.framelength) ?
			(frame_length - imgsensor_info.custom7.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom7.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;        
	default:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}

//ITD: Modify setFPS By Thermal By Jie.liu 230808 Start
static kal_uint32 set_framerate_by_thermal(MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("sensorMode:[%d], framerate:[%d]\n", imgsensor.sensor_mode, framerate);
	if (framerate > fixed_framerate) {
		LOG_INF("framerate:[%d] > fixed_framerate:[%d], return\n", framerate, fixed_framerate);
		return ERROR_NONE;
	}
	switch (imgsensor.sensor_mode) {
	case IMGSENSOR_MODE_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length
			= imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case IMGSENSOR_MODE_VIDEO:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10
			/ imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength)
			? (frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length
			= imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case IMGSENSOR_MODE_CAPTURE:
		frame_length = imgsensor_info.cap.pclk / framerate * 10 /
			imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength)
			? (frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length
			= imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:
		break;
	}
	return ERROR_NONE;
}
//ITD: Modify setFPS By Thermal By Jie.liu 230808 End

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		*framerate = imgsensor_info.custom6.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM7:
		*framerate = imgsensor_info.custom7.max_framerate;
		break;

	default:
		break;
	}

	return ERROR_NONE;
}

static void feedback_awbgain(kal_uint32 gr_gain, kal_uint32 r_gain, kal_uint32 b_gain, kal_uint32 gb_gain)
{
	UINT32 r_gain_int = 0;
	UINT32 b_gain_int = 0;
	UINT32 g_gain_int = 0;
	//kal_uint16 d = 0, e = 0, f = 0, g = 0, h = 0 , i = 0;
	pr_debug("feedback_awbgain: gr_gain = 0x%x r_gain = 0x%x b_gain = 0x%x gb_gain = 0x%x\n",gr_gain, r_gain,b_gain, gb_gain);
	pr_debug("feedback_awbgain: gr_gain = %d r_gain = %d b_gain = %d gb_gain = %d\n",gr_gain, r_gain,b_gain, gb_gain);
	r_gain_int = r_gain / 8;
	b_gain_int = b_gain / 8;
	g_gain_int = ((gr_gain + gb_gain) / 2) / 8;

	pr_info("r_gain = 0x%x b_gain = 0x%x g_gain = 0x%x\n", r_gain_int, b_gain_int, g_gain_int);
	pr_info("r_gain = 0d%d b_gain = 0d%d g_gain = 0d%d\n", r_gain_int, b_gain_int, g_gain_int);
	setGPHon(KAL_TRUE);
	write_cmos_sensor_8bit(0x0d18, (r_gain_int >> 8) & 0x01);
	write_cmos_sensor_8bit(0x0d19, r_gain_int);
	write_cmos_sensor_8bit(0x0d1a, (g_gain_int >> 8) & 0x01);
	write_cmos_sensor_8bit(0x0d1b, g_gain_int);
	write_cmos_sensor_8bit(0x0d1c, (b_gain_int >> 8) & 0x01);
	write_cmos_sensor_8bit(0x0d1d, b_gain_int);
	setGPHon(KAL_FALSE);
	#if 0
	d = read_cmos_sensor(0x0d18);
	e = read_cmos_sensor(0x0d19);
	f = read_cmos_sensor(0x0d1a);
	g = read_cmos_sensor(0x0d1b);
	h = read_cmos_sensor(0x0d1c);
	i = read_cmos_sensor(0x0d1d);	
	pr_info("0x0d18 = 0x%x 0x0d19 = 0x%x 0x0d1a = 0x%x 0x0d1b = 0x%x 0x0d1c = 0x%x 0x0d1d = 0x%x\n", d, e, f, g, h, i);
	#endif
}

static const char* show_feature_id(MSDK_SENSOR_FEATURE_ENUM feature_id) {
	switch(feature_id) {
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		return "GET_GAIN_RANGE_BY_SCENARIO";
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		return "GET_BASE_GAIN_ISO_AND_STEP";
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		return "GET_MIN_SHUTTER_BY_SCENARIO";
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		return "GET_BINNING_TYPE";
	case SENSOR_FEATURE_GET_PERIOD:
		return "case GET_PERIOD";
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		return "GET_PIXEL_CLOCK_FREQ";
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		return "GET_MIPI_PIXEL_RATE";
	case SENSOR_FEATURE_SET_ESHUTTER:
		return "SET_ESHUTTER";
	case SENSOR_FEATURE_SET_NIGHTMODE:
		return "SET_NIGHTMODE";
	case SENSOR_FEATURE_SET_GAIN:
		return "SET_GAIN";
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		return "SET_FLASHLIGHT";
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		return "SET_ISP_MASTER_CLOCK_FREQ";
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		return "GET_OFFSET_TO_START_OF_EXPOSURE";
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		return "GET_PIXEL_CLOCK_FREQ_BY_SCENARIO";
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		return "GET_PERIOD_BY_SCENARIO";
	case SENSOR_FEATURE_SET_REGISTER:
		return "SET_REGISTER";
	case SENSOR_FEATURE_GET_REGISTER:
		return "GET_REGISTER";
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		return "GET_LENS_DRIVER_ID";
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		return "SET_VIDEO_MODE";
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		return "CHECK_SENSOR_ID";
	case SENSOR_FEATURE_CHECK_CHIP_ID:
		return "CHECK_CHIP_ID";
	case SENSOR_FEATURE_GET_PROJECT_NAME:
		return "GET_PROJECT_NAME";
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		return "SET_AUTO_FLICKER_MODE";
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		return "SET_MAX_FRAME_RATE_BY_SCENARIO";
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		return "GET_DEFAULT_FRAME_RATE_BY_SCENARIO";
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		return "SET_TEST_PATTERN";
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		return "GET_TEST_PATTERN_CHECKSUM_VALUE";
	case SENSOR_FEATURE_SET_FRAMERATE:
		return "SET_FRAMERATE";
	case SENSOR_FEATURE_SET_FPS_BY_THERMAL:
		return "SET_FPS_BY_THERMAL";
	case SENSOR_FEATURE_SET_HDR:
		return "SET_HDR";
	case SENSOR_FEATURE_GET_CROP_INFO:
		return "GET_CROP_INFO";
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		return "SET_SHUTTER_FRAME_TIME";
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		return "GET_FRAME_CTRL_INFO_BY_SCENARIO";
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		return "GET_AWB_REQ_BY_SCENARIO";
	case SENSOR_FEATURE_SET_AWB_GAIN:
		return "SET_AWB_GAIN";
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		return "SET_IHDR_SHUTTER_GAIN";
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		return "SET_STREAMING_SUSPEND";
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		return "SET_STREAMING_RESUME";
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
		return "SEAMLESS_SWITCH";
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
		return "GET_SEAMLESS_SCENARIOS";
	case SENSOR_FEATURE_GET_4CELL_DATA:
		return "GET_4CELL_DATA";
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		return "GET_SENSOR_PDAF_CAPACITY";
	case SENSOR_FEATURE_GET_PDAF_INFO:
		return "GET_PDAF_INFO";
	case SENSOR_FEATURE_GET_VC_INFO2:
		return "GET_VC_INFO2";
	case SENSOR_FEATURE_GET_PDAF_DATA:
		return "GET_PDAF_DATA";
	case SENSOR_FEATURE_SET_PDAF:
		return "SET_PDAF";
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		return "GET_TEMPERATURE_VALUE";
	default:
		return "Unknow";
	}

	return "Unknow";
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	char *name_ptr = NULL;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

#ifdef FPT_PDAF_SUPPORT
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO2_STRUCT *pvcinfo;
#endif

#ifdef FPT_SEAMLESS_SUPPORT
	uint32_t *pAeCtrls;
	uint32_t *pScenarios;
#endif

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;

	LOG_INF("feature_id = %d, %s\n", feature_id, show_feature_id(feature_id));
	switch (feature_id) {
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
	    // switch (*feature_data) {
		// 	case MSDK_SCENARIO_ID_CUSTOM5:
		// 		*(feature_data + 1) = imgsensor_info.min_gain;
		// 		*(feature_data + 2) = 1024;//16x gain
		// 		break;
		// 	case MSDK_SCENARIO_ID_CUSTOM3:
		// 		*(feature_data + 1) = imgsensor_info.min_gain;
		// 		*(feature_data + 2) = 512;//8x gain
		// 		break;
		// 	default:
				*(feature_data + 1) = imgsensor_info.min_gain;
				*(feature_data + 2) = imgsensor_info.max_gain;
			break;
		// }
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
	    *(feature_data + 0) = imgsensor_info.min_gain_iso;
	    *(feature_data + 1) = imgsensor_info.gain_step;
	    *(feature_data + 2) = imgsensor_info.gain_type;
	    break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
	    *(feature_data + 1) = imgsensor_info.min_shutter;
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
        case MSDK_SCENARIO_ID_CUSTOM7:
			*(feature_data + 2) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
		default:
			*(feature_data + 2) = 1;
			break;
        }
        break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CUSTOM3:
			*feature_return_para_32 = 1;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		default:
			*feature_return_para_32 = 1;
			break;
        }
	    pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",*feature_return_para_32);
	    *feature_para_len = 4;
	    break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		{
			kal_uint32 rate;

			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				rate = imgsensor_info.cap.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				rate = imgsensor_info.normal_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				rate = imgsensor_info.hs_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				rate = imgsensor_info.slim_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				rate = imgsensor_info.custom1.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				rate = imgsensor_info.custom2.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				rate = imgsensor_info.custom3.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM4:
				rate = imgsensor_info.custom4.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM5:
				rate = imgsensor_info.custom5.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM6:
				rate = imgsensor_info.custom6.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM7:
				rate = imgsensor_info.custom7.mipi_pixel_rate;
				break;            
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				rate = imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		}
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
			set_gain((UINT32)*feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	//modfiy for sensor_fusion by renzhi.li 20241211 start
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 2000000;
		break;
	//modfiy for sensor_fusion by renzhi.li 20241211 end
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom5.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom6.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom7.pclk;
			break;          

		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom2.framelength << 16)
				+ imgsensor_info.custom2.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom3.framelength << 16)
				+ imgsensor_info.custom3.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom4.framelength << 16)
				+ imgsensor_info.custom4.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom5.framelength << 16)
				+ imgsensor_info.custom5.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom6.framelength << 16)
				+ imgsensor_info.custom6.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom7.framelength << 16)
				+ imgsensor_info.custom7.linelength;
			break;            
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8bit(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		LOG_INF("adb_i2c_read 0x%x = 0x%x\n", sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
//ITD:add get chip id by haiyang.he 20250829 start
	case SENSOR_FEATURE_CHECK_CHIP_ID:
		gc50d1_get_chip_id(feature_para);
		break;
//ITD:add get chip id by haiyang.he 20250829 end
//ITD:add project name by yutao.wu 20250826 start
	case SENSOR_FEATURE_GET_PROJECT_NAME:
        if (!feature_para) {
            pr_err("ERROR: feature_para is NULL\n");
            break;
        }
        name_ptr = (char *)feature_para;

        if (!name_ptr) {
            pr_err("ERROR: name_ptr is NULL after assignment\n");
            break;
        }
        if (strnlen(name_ptr, MAX_PROJECT_NAME_LEN) == 0 || strnlen(name_ptr, MAX_PROJECT_NAME_LEN) >= MAX_PROJECT_NAME_LEN) {
            pr_err("ERROR: Invalid project name length");
            break;
        }
        strncpy((char *)project_name, name_ptr, sizeof(project_name) - 1);
        project_name[sizeof(project_name) - 1] = '\0';
        LOG_INF("Got project name: %s\n", project_name);
        break;
//ITD:add project name by yutao.wu 20250826 end

	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((UINT32)*feature_data, (struct SET_SENSOR_PATTERN_SOLID_COLOR *)(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		if (project_name[0] != '\0' && (strcmp((const char *)project_name, "tran_lk7k") == 0 || strcmp((const char *)project_name, "tran_lk6") == 0)) {
			imgsensor_info.checksum_value = 0x8439f2b5;
		}
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps: %d\n", (UINT32)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
//ITD: Modify setFPS By Thermal By Jie.liu 230808 Start
	case SENSOR_FEATURE_SET_FPS_BY_THERMAL:
	    LOG_INF("set_framerate_by_thermal :[%d] \n",(UINT32)*feature_data_32);
	    set_framerate_by_thermal(*feature_data_32);
	break;
//ITD: Modify setFPS By Thermal By Jie.liu 230808 End
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable: %d\n", (BOOL)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)*feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId: %d\n", (UINT32)*feature_data);
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[7], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[8], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[9], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[10], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM7:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[11], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;        
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:/*lzl*/
		pr_debug("SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME\n");
		set_shutter_frame_length((UINT16)*(feature_data),
				(UINT16)(*(feature_data+1)),
				(BOOL) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		*(feature_data + 1) = 1;
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:			
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		if(imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM4 || imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM5 || imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM6){
			pr_debug("SENSOR_FEATURE_SET_AWB_GAIN feature_id = %d\n",feature_id);
			feedback_awbgain((UINT32)*(feature_data_32 + 0), (UINT32)*(feature_data_32 + 1), (UINT32)*(feature_data_32 + 2), (UINT32)*(feature_data_32 + 3));
		}
	break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE = %d, SE = %d, Gain = %d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
		ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
#ifdef FPT_SEAMLESS_SUPPORT
    case SENSOR_FEATURE_SEAMLESS_SWITCH:
    {
        LOG_INF("SENSOR_FEATURE_SEAMLESS_SWITCH");
        if ((feature_data + 1) != NULL)
            pAeCtrls = (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
        else
            pr_debug("warning! no ae_ctrl input");

        if (feature_data == NULL) {
            pr_info("error! input scenario is null!");
            return ERROR_INVALID_SCENARIO_ID;
        }
        LOG_INF("call seamless_switch ");
        seamless_switch((*feature_data), pAeCtrls);
    }
        break;
    case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
        if ((feature_data + 1) != NULL) {
            pScenarios = (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
            if(((feature_data + 2) != NULL ) && (MUINT32 *)((uintptr_t)(*(feature_data + 2))) != 0){
            	switch (*feature_data) {
            	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            		*pScenarios = MSDK_SCENARIO_ID_CUSTOM5|TRAN_SENSOR_MODE_ISZ_PRE;
            		*(pScenarios + 1) = MSDK_SCENARIO_ID_CUSTOM6|TRAN_SENSOR_MODE_ISZ_QUAD;
            		*(pScenarios + 2) = MSDK_SCENARIO_ID_CUSTOM3|TRAN_SENSOR_MODE_FULL_SIZE;
            		*(pScenarios + 3)= MSDK_SCENARIO_ID_CUSTOM4|TRAN_SENSOR_MODE_ISZ_VSDOF;
            		*(pScenarios + 4)= MSDK_SCENARIO_ID_CAMERA_PREVIEW|TRAN_SENSOR_MODE_NONE;
            		break;
            	default:
            		*pScenarios = 0xff;
            		break;
            	}
            } else {
            	switch (*feature_data) {
            	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            		*pScenarios = MSDK_SCENARIO_ID_CUSTOM5;
            		*(pScenarios + 1) = MSDK_SCENARIO_ID_CUSTOM6;
            		break;
            	case MSDK_SCENARIO_ID_CUSTOM5:
            		*pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
            		*(pScenarios + 1) = MSDK_SCENARIO_ID_CUSTOM6;
            		break;
            	case MSDK_SCENARIO_ID_CUSTOM6:
            		*pScenarios = MSDK_SCENARIO_ID_CUSTOM5;
            		break;
            	case MSDK_SCENARIO_ID_CUSTOM1:
            		*pScenarios = MSDK_SCENARIO_ID_CUSTOM4;
            		break;
            	case MSDK_SCENARIO_ID_CUSTOM4:
            		*pScenarios = MSDK_SCENARIO_ID_CUSTOM1;
            		break;
            	case MSDK_SCENARIO_ID_SLIM_VIDEO:
            	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            	default:
            		*pScenarios = 0xff;
            		break;
            	}
            }
       } else {
            LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS input pScenarios vector is NULL!\n");
            return ERROR_INVALID_SCENARIO_ID;
        }
        LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %llu %u\n",
            *feature_data,
            *pScenarios);
        break;
/*  case SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH:
        extend_frame_length((MUINT32) *feature_data);
        break;
*/
#endif
	case SENSOR_FEATURE_GET_4CELL_DATA:
	{
		int type = (kal_uint16)(*feature_data);
		char *data = (char *)(uintptr_t)(*(feature_data+1));
		if ( type == FOUR_CELL_CAL_TYPE_GAIN_TBL ) {
			if (gc50d1_xtalk_data[0] == 0) {
				printk("read 4cell data again \n");
				gc50d1_read_crosstalk_data(CROSSTALK_START_ADDR, &gc50d1_xtalk_data[0], CROSSTALK_BUF_SIZE);
			}

			gc50d1_xtalk_data[0] = (kal_uint8)(CROSSTALK_BUF_SIZE & 0xff);/*Low*/
			gc50d1_xtalk_data[1] = (kal_uint8)((CROSSTALK_BUF_SIZE >> 8) & 0xff);/*High*/
			if (data != NULL) {
				printk("return data\n");
				memcpy(data, gc50d1_xtalk_data, sizeof(gc50d1_xtalk_data) / sizeof(gc50d1_xtalk_data[0]));
			}
			LOG_INF("Read Cross Talk = %02x %02x %02x %02x %02x %02x\n",
				(UINT16)data[0], (UINT16)data[1],
				(UINT16)data[2], (UINT16)data[3],
				(UINT16)data[4], (UINT16)data[5]);
		}
		break;
	}
#ifdef FPT_PDAF_SUPPORT
/******************** PDAF START ********************/
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n", *feature_data);
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;

		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%llu\n", *feature_data);

        //ITD:add project name by fei.wu5 20250801 start
            if (strcmp((const char *)project_name, "tran_lk7k") == 0 || strcmp((const char *)project_name, "tran_lk6")  == 0) {
                imgsensor_pd_info[0].iMirrorFlip = IMAGE_NORMAL;
                imgsensor_pd_info[1].iMirrorFlip = IMAGE_NORMAL;
                imgsensor_pd_info[2].iMirrorFlip = IMAGE_NORMAL;
                imgsensor_pd_info[3].iMirrorFlip = IMAGE_NORMAL;
            }
        //ITD:add project name by fei.wu5 20250801 end

		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[0], sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[1], sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[2], sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:			
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[3], sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_CUSTOM7:			
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[4], sizeof(struct SET_PD_BLOCK_INFO_T));
			break;        
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM3:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO2:
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n", (UINT16) *feature_data);
		pvcinfo = (struct SENSOR_VC_INFO2_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO2[0], sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO2[1], sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO2[2], sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO2[3], sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM7:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO2[4], sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;        
		default:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO2[1], sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
	case SENSOR_FEATURE_SET_PDAF:
		LOG_INF("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;
/******************** PDAF END ********************/
#endif
       case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
           *feature_return_para_32 = -127000;
           *feature_para_len = 4;
           break;
	default:
		break;
	}
	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 GC50D1_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
