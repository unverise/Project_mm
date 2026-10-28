/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 S5KGN5SPmipiraw_sensor.c
 *
 * Project:
 * --------
 *	 ALPS MT6785
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *	 ITD: Modify RF 2.4G WIFI ch7（2442M）6.5dB By Jesee
 *	 1. MIPI CLK
 *	 	204mhz (*12=2448mhz) => 207mhz (*12=2484mhz)
 *	 	1632mhz ==> 1656mhz
 *	 2. mipi_pixel_rate = 652800000 => 662400000,
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5kgn5spmipiraw_Sensor.h"
#include "s5kgn5sp_sensor_setting.h"

/****************************Modify Following Strings for Debug****************************/
#define PFX "S5KGN5SP"
#define LOG_INF_NEW(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_INF LOG_INF_NEW
#define LOG_1 LOG_INF("S5KGN5SP,MIPI 3LANE\n")
#define SENSORDB LOG_INF
/****************************   Modify end    *******************************************/
#define FPT_PDAF_SUPPORT
#define FPT_SEAMLESS_SUPPORT
#define HV_MIRROR_FLIP
static DEFINE_SPINLOCK(imgsensor_drv_lock);
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5KGN5SP_SENSOR_ID,	//record sensor id defined in Kd_imgsensor.h
	.checksum_value = 0xef23676a,
	.pre = {
		.pclk = 1920000000,
		.linelength  = 14768,
		.framelength = 4310,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 1492114285,
	},
	.cap = {
		.pclk = 1920000000,
		.linelength  = 14768,
		.framelength = 4310,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 1492114285,
	},
	.normal_video = {
		.pclk = 1920000000,
		.linelength  = 19696,
		.framelength = 3248,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 2296,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 1492114285,
	},
	.hs_video = {
		.pclk = 1920000000,
		.linelength  = 6336,
		.framelength = 2525,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2032,
		.grabwindow_height = 1536,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 1492114285,
	},
	.slim_video = {
		.pclk = 1920000000,
		.linelength  = 5344,
		.framelength = 1488,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 2410,
		.mipi_pixel_rate = 1492114285,
	},
	.custom1 = {
		.pclk = 1920000000,
		.linelength  = 14768,
		.framelength = 4310,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 1492114285,
	},
	.custom2 = {
		.pclk = 1920000000,
		.linelength  = 10944,
		.framelength = 2922,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3840,
		.grabwindow_height = 2160,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
		.mipi_pixel_rate = 1711542857,
	},
	.custom3 = {
		.pclk = 1920000000,
		.linelength  = 15104,
		.framelength = 6320,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 8160,
		.grabwindow_height = 6144,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 200,
		.mipi_pixel_rate = 1492114285,
	},
	.custom4 = {
		.pclk = 1920000000,
		.linelength  = 13568,
		.framelength = 4710,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 1492114285,
	},
	.custom5 = {
		.pclk = 1920000000,
		.linelength  = 13568,
		.framelength = 4710,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 1492114285,
	},

	.margin = 4,                    /*sensor framelength & shutter margin*/
	.min_shutter = 4,               /*min shutter*/
	.max_frame_length = 0xFFFF,
	.ae_shut_delay_frame = 0,
	.min_gain = 64,
    .max_gain = 4096,
    .min_gain_iso = 50,
	.gain_step = 2,
    .gain_type = 2,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 10,
	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 1,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,	//enter high speed video  delay frame num
	.slim_video_delay_frame = 2,//enter slim video delay frame num
	.custom1_delay_frame = 1,
	.custom2_delay_frame = 1,
	.custom3_delay_frame = 1,
	.custom4_delay_frame = 1,
	.custom5_delay_frame = 1,
	.frame_time_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	//.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_settle_delay_mode = 1, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gb,
	
	.mclk = 24,
	//.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.i2c_addr_table = {0x20, 0xff},
	.i2c_speed = 1000, // i2c read/write speed
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x200,					//current shutter
	.gain = 0x200,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 0,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,
	.test_pattern = 0,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = KAL_FALSE, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,//record current sensor's i2c write id
//ITD:modify s5kgn5sp long exposure by huihang.zhou 220111 start
	.current_ae_effective_frame = 2,
//ITD:modify s5kgn5sp long exposure by huihang.zhou 220111 end
};

/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
    {8160, 6144, 0,     0, 8160, 6144, 4080, 3072,  0,    0, 4080, 3072,    0,    0, 4080, 3072}, // Preview
    {8160, 6144, 0,     0, 8160, 6144, 4080, 3072,  0,    0, 4080, 3072,    0,    0, 4080, 3072}, // capture
    {8160, 6144, 0,   776, 8160, 4592, 4080, 2296,  0,    0, 4080, 2296,    0, 	  0, 4080, 2296},// video
    {8160, 6144, 16,    0, 8128, 6144, 2032, 1536,  0,    0, 2032, 1536,    0,    0, 2032, 1536}, // hs video
    {8160, 6144, 240, 912, 7680, 4320, 1920, 1080,  0,    0, 1920, 1080,    0,    0, 1920, 1080}, // slim video
    {8160, 6144, 0,     0, 8160, 6144, 4080, 3072,  0,    0, 4080, 3072,    0,    0, 4080, 3072}, // custom1 vsdof
    {8160, 6144, 240, 912, 7680, 4320, 3840, 2160,  0,    0, 3840, 2160,    0,    0, 3840, 2160}, // custom2 60fps
    {8160, 6144, 0,     0, 8160, 6144, 8160, 6144,  0,    0, 8160, 6144,    0,    0, 8160, 6144}, // custom3 fullSize  50M
    {8160, 6144,2040,1536, 4080, 3072, 4080, 3072,  0,    0, 4080, 3072,    0,    0, 4080, 3072}, // custom4  ------ isz fov 25%
    {8160, 6144,2040,1536, 4080, 3072, 4080, 3072,  0,    0, 4080, 3072,    0,    0, 4080, 3072}, // custom5  ------ isz fov 25%
};

#ifdef FPT_PDAF_SUPPORT
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info[2] = {
	{
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = {
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
	},
#ifdef HV_MIRROR_FLIP
	.iMirrorFlip = 3,
#else
	.iMirrorFlip = 0,
#endif	
	},
	{
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = {
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {2040, 1536}
	},
#ifdef HV_MIRROR_FLIP
	.iMirrorFlip = 3,
#else
	.iMirrorFlip = 0,
#endif	
	}
};

static struct SENSOR_VC_INFO2_STRUCT SENSOR_VC_INFO2[5] = {
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,//pre/cap/nal 4080 * 3072
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x0FF0, 0xC00},
			{VC_PDAF_STATS, 0x01, 0x30, 0x13EC, 0x300},
		},
		1
	},
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,//
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x0FF0, 0xC00},
			{VC_PDAF_STATS, 0x01, 0x30, 0x13EC, 0x300},
		},
		1
	},
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x0F00, 0x870},
			{VC_PDAF_STATS, 0x01, 0x30, 0x13EC, 0x23E},
		},
		1
	},
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,//CUSTOM5
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x0FF0, 0xC00},
			{VC_PDAF_STATS, 0x01, 0x30, 0x09EC, 0x180},
		},
		1
	},
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,//CUSTOM2
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x0F00, 0x870},
			{VC_PDAF_STATS, 0x01, 0x30, 0x12C0, 0x21C},
		},
		1
	},

};
#endif

static kal_uint16 read_cmos_sensor_byte(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_byte(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF),
						(char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static void check_streamoff(void)
{
	unsigned int i = 0;
	int timeout = (10000 / imgsensor.current_fps) + 1;

	mdelay(3);
	for (i = 0; i < timeout; i++) {
		if (read_cmos_sensor_byte(0x0005) != 0xFF)
			mdelay(1);
		else
			break;
	}
	pr_debug("%s exit!\n", __func__);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		write_cmos_sensor_byte(0x0100, 0x01);
	} else {
		write_cmos_sensor_byte(0x0100, 0x00);
		check_streamoff();
	}
	return ERROR_NONE;
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(0x0342, imgsensor.line_length & 0xFFFF);
}	/*	set_dummy  */

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;

	LOG_INF("framerate = %d, min framelength should enable(%d)\n",
		framerate, min_framelength_en);

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
}	/*	set_max_framerate  */

/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
//ITD: add reason bug_id CILHLJYQ-356 by huihang.zhou 2022/1/11 start
static void set_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;
	kal_uint32 frameLengthLine = 0x0;
	kal_uint32 coarseIntegrationTime = 0x0;

	spin_lock(&imgsensor_drv_lock);
//ITD:modify by camera driver team start
	imgsensor.shutter = shutter;
//ITD:modify by camera driver team end
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	//if (shutter < imgsensor_info.min_shutter)
	//	shutter = imgsensor_info.min_shutter;
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
				/ imgsensor.frame_length;
		LOG_INF("autoflicker enable, realtime_fps = %d\n",
			realtime_fps);
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
	}

	LOG_INF("vc_channel-1 0264 = 0x%x",read_cmos_sensor_byte(0x0264));
	LOG_INF("vc_channel-2 0264 = 0x%x",((read_cmos_sensor_byte(0x0264) << 8) | read_cmos_sensor_byte(0x0265)));
	/* long expsoure */
	if (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) {
		coarseIntegrationTime = shutter / 128 ;
		frameLengthLine = coarseIntegrationTime + 10;
		LOG_INF("Long shutter In shutter = %d frameLength = 0x%x coarseTime = 0x%x\n", shutter, frameLengthLine, coarseIntegrationTime);
		write_cmos_sensor(0x6028,0x4000);
		write_cmos_sensor(0x0340,frameLengthLine);
		write_cmos_sensor(0x0202,coarseIntegrationTime);
		write_cmos_sensor(0x0702,0x0700);
		write_cmos_sensor(0x0704,0x0700);
		/* Frame exposure mode customization for LE*/
		imgsensor.ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
		imgsensor.ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
		imgsensor.current_ae_effective_frame = 2;
	} else {
		// Extend frame length
		// Update Shutter
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
		write_cmos_sensor(0x0202, shutter & 0xFFFF);
        write_cmos_sensor(0x0452, shutter & 0xFFFF);
		write_cmos_sensor(0x0702,0x0000);
		write_cmos_sensor(0x0704,0x0000);
		imgsensor.current_ae_effective_frame = 2;
		LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
	}

}
//ITD: add reason bug_id CILHLJYQ-356 by huihang.zhou 2022/1/11 end

static void set_shutter_frame_length(kal_uint16 shutter,
				kal_uint16 target_frame_length,
				kal_bool auto_extend_en)
{
	spin_lock(&imgsensor_drv_lock);
	if (imgsensor.sensor_mode == IMGSENSOR_MODE_CAPTURE) {//spec case 24fps
		imgsensor.dummy_line = (target_frame_length > imgsensor_info.cap.framelength)
			? (target_frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		pr_debug("shutter: %d, frame_length: %d, cap.framelength: %d dummy_line: %d\n",
			shutter, imgsensor.frame_length,
			imgsensor_info.cap.framelength,
			imgsensor.dummy_line);
	} else if (imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM2) {//spec case 24fps
		imgsensor.dummy_line = (target_frame_length > imgsensor_info.custom2.framelength)
			? (target_frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		pr_debug("shutter: %d, frame_length: %d, custom2.framelength: %d dummy_line: %d\n",
			shutter, imgsensor.frame_length,
			imgsensor_info.custom2.framelength,
			imgsensor.dummy_line);
	} else {
		imgsensor.dummy_line = (target_frame_length > imgsensor_info.custom1.framelength)
			? (target_frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		pr_debug("shutter: %d, frame_length: %d, custom1.framelength: %d dummy_line: %d\n",
			shutter, imgsensor.frame_length,
			imgsensor_info.custom1.framelength,
			imgsensor.dummy_line);
	}
	spin_unlock(&imgsensor_drv_lock);
	//set_dummy();
	// frame_length will be set in set_shutter
	set_shutter(shutter);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;

	reg_gain = gain/2;
	return (kal_uint16)reg_gain;
}

/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
 //ITD:modify by yingfeili 20210625 start
static kal_uint16 set_gain_binning(kal_uint16 gain)
{
	kal_uint16 reg_gain,reg_gainD;
	//gain = 64 = 1x real gain.
	LOG_INF("++ gain:%d\n",gain);
	if (gain < BASEGAIN || gain > 64 * BASEGAIN) {
		LOG_INF("Error gain setting");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 64 * BASEGAIN)
		{
			reg_gainD = gain / 8;
			LOG_INF("reg_gainD:%d",reg_gainD);
			write_cmos_sensor(0x020E, (reg_gainD&0xFFFF));
			write_cmos_sensor(0x0456, (reg_gainD&0xFFFF));
			spin_lock(&imgsensor_drv_lock);
			imgsensor.gain = 0x0400;
			spin_unlock(&imgsensor_drv_lock);
			write_cmos_sensor(0x0204, 0x0400);
			return gain;
		}
	}

	LOG_INF("** gain:%d",gain);
	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("-- reg_gain = 0x%x\n ",reg_gain);
	write_cmos_sensor(0x020E, 0x0100);
	write_cmos_sensor(0x0456, 0x0100);
	write_cmos_sensor(0x0204, (reg_gain&0xFFFF));

	return gain;
}	/*	set_gain  */
 //ITD:modify by yingfeili 20210625 end

static kal_uint16 set_gain_fullsize(kal_uint16 gain)
{
	kal_uint16 reg_gain;
	//gain = 64 = 1x real gain.
	if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
		LOG_INF("Error gain setting");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 16 * BASEGAIN)
			gain = 16 * BASEGAIN;
	}
	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("set_gain_fullsize gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
	write_cmos_sensor(0x0204, (reg_gain&0xFFFF));

	return gain;
}	/*	set_gain  */

static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.mirror = image_mirror;
	spin_unlock(&imgsensor_drv_lock);
	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor_byte(0x0101, 0x00); //GR
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor_byte(0x0101, 0x01); //R
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor_byte(0x0101, 0x02); //B
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor_byte(0x0101, 0x03); //GB
		break;
	default:
		LOG_INF("Error image_mirror setting\n");
	}

}

/*************************************************************************
 * FUNCTION
 *	night_mode
 *
 * DESCRIPTION
 *	This function night mode of sensor.
 *
 * PARAMETERS
 *	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/

#define MULTI_WRITE 1
#if MULTI_WRITE
#define I2C_BUFFER_LEN 1020 /*trans# max is 255, each 4 bytes*/
#else
#define I2C_BUFFER_LEN  4
#endif
#if 0
static kal_uint16 table_write_cmos_sensor(
	kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;

	while (len > IDX) {
		addr = para[IDX];
		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}

#if MULTI_WRITE
	if ((I2C_BUFFER_LEN - tosend) < 4 || IDX == len || addr != addr_last) {
		iBurstWriteReg_multi(puSendCmd, tosend,
			imgsensor.i2c_write_id, 4, imgsensor_info.i2c_speed);

			tosend = 0;
	}
#else
		iWriteRegI2CTiming(puSendCmd, 4,
			imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

		tosend = 0;
#endif

	}
	return 0;
}
#endif
static kal_uint16 mix_write_cmos_sensor(
	kal_uint16 *para, kal_uint32 len)
{
	char *puSendCmd = NULL;
	puSendCmd = kmalloc(I2C_BUFFER_LEN, GFP_KERNEL);
	if (puSendCmd == NULL) {
		pr_err(" ioctl allocate mem failed\n");
		return -ENOMEM;
	}
	//char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	//[ITD]add i2c continue mode transfer. hong.zhang@transsion.com, 2024-2-29, start.
	#define MAXLEN_CONTINUEDATA (I2C_BUFFER_LEN+I2C_BUFFER_LEN)
	char *puSdContinCmd = NULL;
	puSdContinCmd = kmalloc(MAXLEN_CONTINUEDATA, GFP_KERNEL);
	if (puSdContinCmd == NULL) {
		pr_err(" ioctl allocate mem failed\n");
		return -ENOMEM;
	}
	//char puSdContinCmd[MAXLEN_CONTINUEDATA];
	kal_uint32 continlen=0, item=0, nextItem;
	kal_uint32 continStartItem=0, continCount=0;
	kal_uint32 unContinStartItem=0, unContinCount=0;
	kal_uint32 continNextAddr=0, continCurAddr=0;
	kal_uint32 dataOffset;
	kal_uint8 endflag=0;
	kal_uint8 lastContinStat=0, continStat=0;//0:unkonw 1:continue 2:uncontinue
	kal_uint16 *pRegSrc;
	kal_uint16 tempdata;
	dataOffset = sizeof(para[0]);
	LOG_INF("dataOffset=%d.\n", dataOffset);
//[ITD]add i2c continue mode transfer. hong.zhang@transsion.com, 2024-2-29, end.
	tosend = 0;
	IDX = 0;

//[ITD]add i2c continue mode transfer. hong.zhang@transsion.com, 2024-2-29, start.
	//while (len > IDX) {
	for (item=0; item<len; item=nextItem) {
		continCurAddr = para[item];
		nextItem = item+2;
		if (nextItem < len)
		{
			continNextAddr = para[nextItem];
		}
		else
		{
			endflag = 1;
		}

		if (endflag == 0)
		{
			if ((continCurAddr+dataOffset) == continNextAddr)
			{
				if (continStat != 1)
				{
					//conintue first addr item.
					continCount = 2;
					continStartItem = item;
					continStat = 1;
				}
				continCount += 2;
			}
			else
			{
				if (continStat == 0)
				{
					//unconintue first addr item.
					unContinCount = 0;
					unContinStartItem = item;
					continStat = 2;
					unContinCount += 2;
				}
				else if (continStat == 1)
				{
					//last continue cmd has in continCount, skip the last continue cmd campare.
					continStat = 0;
				}
				else
				{
					unContinCount += 2;
				}
			}
		}
		else
		{
			if (continStat == 2)
			{
				unContinCount += 2;
			}
			else if (continStat == 0)
			{
				unContinCount = 0;
				unContinStartItem = item;
				continStat = 2;
				unContinCount += 2;
			}
		}

		{
			if ((lastContinStat != continStat) || endflag==1)
			{
				if ((lastContinStat==1 && (continStat != 1)) || (endflag==1 && continStat==1))
				{
					//send continue cmd
					continlen = 0;
					pRegSrc = para+continStartItem;
#if MULTI_WRITE
					puSdContinCmd[continlen++] = (char)(pRegSrc[0] >> 8);
					puSdContinCmd[continlen++] = (char)(pRegSrc[0] & 0xFF);
					for (IDX=1; IDX<continCount;)
					{
						tempdata = pRegSrc[IDX];
						puSdContinCmd[continlen++] = (char)(tempdata >> 8);
						puSdContinCmd[continlen++] = (char)(tempdata & 0xFF);
						IDX += 2;
						if (((2+continlen)>MAXLEN_CONTINUEDATA) || (IDX>=continCount))
						{
							iBurstWriteReg_multi(puSdContinCmd, continlen,
								imgsensor.i2c_write_id, continlen, imgsensor_info.i2c_speed);
							continlen = 0;
							if (IDX < continCount)
							{
								puSdContinCmd[continlen++] = (char)(pRegSrc[IDX-1] >> 8);
								puSdContinCmd[continlen++] = (char)(pRegSrc[IDX-1] & 0xFF);
							}
						}
					}
#else
					for (IDX=0; IDX<continCount; )
					{
						addr = pRegSrc[IDX];
						{
							puSendCmd[tosend++] = (char)(addr >> 8);
							puSendCmd[tosend++] = (char)(addr & 0xFF);
							data = pRegSrc[IDX + 1];
							puSendCmd[tosend++] = (char)(data >> 8);
							puSendCmd[tosend++] = (char)(data & 0xFF);
							IDX += 2;
							addr_last = addr;
						}

						iWriteRegI2CTiming(puSendCmd, 4,
							imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

						tosend = 0;
					}
#endif
				}
				else if ((lastContinStat==2 && (continStat != 2)) || (endflag==1 && continStat==2))
				{
					pRegSrc = para+unContinStartItem;
					tosend = 0;
					for (IDX=0; IDX<unContinCount;)
					{
#if MULTI_WRITE
						addr = pRegSrc[IDX];
						{
							puSendCmd[tosend++] = (char)(addr >> 8);
							puSendCmd[tosend++] = (char)(addr & 0xFF);
							data = pRegSrc[IDX + 1];
							puSendCmd[tosend++] = (char)(data >> 8);
							puSendCmd[tosend++] = (char)(data & 0xFF);
							IDX += 2;
							addr_last = addr;
						}
						if ((I2C_BUFFER_LEN - tosend) < 4 || IDX == unContinCount) {
							//check send data!!!
							iBurstWriteReg_multi(puSendCmd, tosend,
								imgsensor.i2c_write_id, 4, imgsensor_info.i2c_speed);

							tosend = 0;
						}
#else
						iWriteRegI2CTiming(puSendCmd, 4,
							imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

						tosend = 0;

#endif
					}
				}
				lastContinStat = continStat;
			}
		}
	}
	kfree(puSendCmd);
	kfree(puSdContinCmd);
//[ITD]add i2c continue mode transfer. hong.zhang@transsion.com, 2024-2-29, end.
	return 0;
}

static void sensor_init(void)
{
	LOG_INF("sensor_init E\n");
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x0000, 0x0020);
	write_cmos_sensor(0x6018, 0x0001);
	mdelay(1);
	write_cmos_sensor(0xFCFC, 0x2002);
	write_cmos_sensor(0xB7F0, 0x0000);
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x7002, 0x0008);
	write_cmos_sensor(0x7004, 0x1770);
	write_cmos_sensor(0x70B8, 0x4CF0);
	write_cmos_sensor(0x6014, 0x0001);
	mdelay(20);

	mix_write_cmos_sensor(
		s5kgn5_sensor_init_setting_array,
		sizeof(s5kgn5_sensor_init_setting_array)/sizeof(kal_uint16));

	
}				/*      sensor_init  */

static void setGPHon(kal_bool enable);
static void preview_setting(void)
{
	LOG_INF(" preview_setting E\n");
	mix_write_cmos_sensor(
	s5kgn5_sensor_fmc_init_setting_array,
	sizeof(s5kgn5_sensor_fmc_init_setting_array)/sizeof(kal_uint16));
	LOG_INF("sensor_init_fmc X");
	mix_write_cmos_sensor(
	s5kgn5_binning_fmc_setting_array,
	sizeof(s5kgn5_binning_fmc_setting_array)/sizeof(kal_uint16));
	setGPHon(KAL_FALSE);
	LOG_INF(" preview_setting fmc E\n");
	LOG_INF(" preview_setting X\n");
}


static void capture_setting(kal_uint16 currefps)
{
	LOG_INF(" capture_setting E! currefps:%d\n", currefps);
	preview_setting();
	LOG_INF(" capture_setting X\n");
}

static void normal_video_setting(void)
{
	LOG_INF("normal_video_setting E\n");
	mix_write_cmos_sensor(s5kgn5_vr_setting_array,
		sizeof(s5kgn5_vr_setting_array) / sizeof(kal_uint16));
}

static void hs_video_setting(void)
{
	LOG_INF(" hs_video_setting E\n");
	mix_write_cmos_sensor(s5kgn5_120fps_setting_array,
			sizeof(s5kgn5_120fps_setting_array) / sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	LOG_INF("slim_video_setting E\n");
	mix_write_cmos_sensor(s5kgn5_240fps_setting_array,
		sizeof(s5kgn5_240fps_setting_array) / sizeof(kal_uint16));
}

static void custom1_setting(void)
{
	LOG_INF("custom1_setting E\n");
	mix_write_cmos_sensor(s5kgn5_binning_setting_array,
		sizeof(s5kgn5_binning_setting_array) / sizeof(kal_uint16));
}

static void custom2_setting(void)
{
	LOG_INF("custom2_setting E\n");
	mix_write_cmos_sensor(s5kgn5_4k_60fps_setting_array,
		sizeof(s5kgn5_4k_60fps_setting_array) / sizeof(kal_uint16));
}

static void custom3_setting(void)
{
		LOG_INF("custom3_setting normal E\n");
	mix_write_cmos_sensor(s5kgn5_fullsize_setting_array,
		sizeof(s5kgn5_fullsize_setting_array) / sizeof(kal_uint16));
}

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor_byte(0x0000) << 8) | read_cmos_sensor_byte(0x0001));
}

/*************************************************************************
 * FUNCTION
 *	get_imgsensor_id
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	*sensorID : return the sensor ID
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	LOG_INF("get_imgsensor_id\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
						imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n",
					imgsensor.i2c_write_id);
		    retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		// if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
	//const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n",
					imgsensor.i2c_write_id);
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
	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = KAL_FALSE;
	imgsensor.test_pattern = 0;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}   /*  open  */

/*************************************************************************
 * FUNCTION
 *	close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");
	/*No Need to implement this function*/

	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	preview   */

/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		LOG_INF("cap1-15fps: use cap1's setting: %d fps!\n", imgsensor.current_fps/10);
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {
		LOG_INF("cap2-24fps: use cap2's setting: %d fps!\n", imgsensor.current_fps/10);
		imgsensor.pclk = imgsensor_info.cap2.pclk;
		imgsensor.line_length = imgsensor_info.cap2.linelength;
		imgsensor.frame_length = imgsensor_info.cap2.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else{
		LOG_INF("current_fps %d is not support,use cap1\n", imgsensor.current_fps/10);
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/* capture() */

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
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	hs_video   */

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
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 * Custom1
 *
 * DESCRIPTION
 *   This function start the sensor Custom1.
 *
 * PARAMETERS
 *   *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *   None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}   /*  Custom1   */

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}   /*  Custom2   */

static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}   /*  Custom3   */

static kal_uint32 Custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}   /*  Custom4   */
static kal_uint32 Custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}   /*  Custom5   */

//ITD:modify by camera driver team end
static void setGPHon(kal_bool enable)
{
    if (enable) {
        //write_cmos_sensor(0xFCFC, 0x4000);
        write_cmos_sensor(0x0104, 0x0101);
    } else {
        //write_cmos_sensor(0xFCFC, 0x4000);
        write_cmos_sensor(0x0104, 0x0001);
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

			LOG_INF("seamless switch 1-exp!\n");

			mix_write_cmos_sensor(s5kgn5_binning_fmc_setting_array,
				sizeof(s5kgn5_binning_fmc_setting_array) / sizeof(kal_uint16));
			if (ae_ctrl) {
				LOG_INF("call SENSOR_SCENARIO_ID_NORMAL_CAPTURE %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1]);
				set_shutter(ae_ctrl[SHUTTER_NE_FRM_1]);
				set_gain_binning(ae_ctrl[GAIN_NE_FRM_1]);
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

			LOG_INF("seamless switch 5-exp!\n");
			mix_write_cmos_sensor(s5kgn5_binning_fmc_setting_array,
				sizeof(s5kgn5_binning_fmc_setting_array) / sizeof(kal_uint16));

			if (ae_ctrl) {
				LOG_INF("call SENSOR_SCENARIO_ID_CUSTOM1 %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1],ae_ctrl[SHUTTER_SE_FRM_1],ae_ctrl[GAIN_SE_FRM_1]);
				set_shutter(ae_ctrl[SHUTTER_NE_FRM_1]);
				set_gain_fullsize(ae_ctrl[GAIN_NE_FRM_1]);
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

			LOG_INF("seamless switch 5-exp!\n");
			mix_write_cmos_sensor(s5kgn5_fullsizecrop_fmc_setting_array,
				sizeof(s5kgn5_fullsizecrop_fmc_setting_array) / sizeof(kal_uint16));

			if (ae_ctrl) {
				LOG_INF("call SENSOR_SCENARIO_ID_CUSTOM4 %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1],ae_ctrl[SHUTTER_SE_FRM_1],ae_ctrl[GAIN_SE_FRM_1]);
				set_shutter(ae_ctrl[SHUTTER_NE_FRM_1]);
				set_gain_fullsize(ae_ctrl[GAIN_NE_FRM_1]);
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

			LOG_INF("seamless switch 5-exp!\n");
			mix_write_cmos_sensor(s5kgn5_fullsizecrop_fmc_setting_array,
				sizeof(s5kgn5_fullsizecrop_fmc_setting_array) / sizeof(kal_uint16));

			if (ae_ctrl) {
				LOG_INF("call SENSOR_SCENARIO_ID_CUSTOM5 %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1],ae_ctrl[SHUTTER_SE_FRM_1],ae_ctrl[GAIN_SE_FRM_1]);
				set_shutter(ae_ctrl[SHUTTER_NE_FRM_1]);
				set_gain_fullsize(ae_ctrl[GAIN_NE_FRM_1]);
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
//ITD:modify by camera driver team end
static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth
		= imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight
		= imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorPreviewWidth
		= imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight
		= imgsensor_info.pre.grabwindow_height;
	sensor_resolution->SensorVideoWidth
		= imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight
		= imgsensor_info.normal_video.grabwindow_height;
	sensor_resolution->SensorHighSpeedVideoWidth
		= imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight
		= imgsensor_info.hs_video.grabwindow_height;
	sensor_resolution->SensorSlimVideoWidth
		= imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight
		= imgsensor_info.slim_video.grabwindow_height;
	sensor_resolution->SensorCustom1Width
		= imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height
		= imgsensor_info.custom1.grabwindow_height;
	sensor_resolution->SensorCustom2Width
		= imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height
		= imgsensor_info.custom2.grabwindow_height;
	sensor_resolution->SensorCustom3Width
		= imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height
		= imgsensor_info.custom3.grabwindow_height;
	sensor_resolution->SensorCustom4Width
		= imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height
		= imgsensor_info.custom4.grabwindow_height;
	sensor_resolution->SensorCustom5Width
		= imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height
		= imgsensor_info.custom5.grabwindow_height;
	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat
		= imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;
	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame
		= imgsensor_info.ae_sensor_gain_delay_frame;
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
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x
	sensor_info->SensorPacketECCOrder = 1;

#ifdef FPT_PDAF_SUPPORT
	//sensor_info->PDAF_Support = 7;
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV_QPD;
#else
	sensor_info->PDAF_Support = 0;
#endif

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount
			= imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount
			= imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount
			= imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount
			= imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount
			= imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
	    sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount
			= imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM2:
	    sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount
			= imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM3:
	    sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount
			= imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM4:
	    sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount
			= imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM5:
	    sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount
			= imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
	break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount
			= imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}
	return ERROR_NONE;
}	/*	get_info  */

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
						MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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
	    Custom1(image_window, sensor_config_data); // Custom1
	break;
	case MSDK_SCENARIO_ID_CUSTOM2:
	    Custom2(image_window, sensor_config_data); // Custom1
	break;
	case MSDK_SCENARIO_ID_CUSTOM3:
	    Custom3(image_window, sensor_config_data); // Custom1
	break;
	case MSDK_SCENARIO_ID_CUSTOM4:
	    Custom4(image_window, sensor_config_data); // Custom4
	break;
	case MSDK_SCENARIO_ID_CUSTOM5:
	    Custom5(image_window, sensor_config_data); // Custom1
			break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) //enable auto flicker
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
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
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
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
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (framerate == 300) {
			frame_length = imgsensor_info.cap.pclk / framerate * 10
				/ imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength)
				? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length
				= imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else if (framerate == 150) {
			frame_length = imgsensor_info.cap1.pclk / framerate * 10 /
				imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength)
				? (frame_length - imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length
				= imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else if (framerate == 240) {
			frame_length = imgsensor_info.cap2.pclk / framerate * 10 /
				imgsensor_info.cap2.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap2.framelength)
				? (frame_length - imgsensor_info.cap2.framelength) : 0;
			imgsensor.frame_length
				= imgsensor_info.cap2.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			frame_length = imgsensor_info.cap1.pclk / framerate * 10 /
				imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength)
				? (frame_length - imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length
				= imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength)
			? (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length
			= imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length
			= imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10
			/ imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length
			= imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		//set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10
			/ imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length
			= imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		// set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10
			/ imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength)
			? (frame_length - imgsensor_info.custom3.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length
			= imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		//set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10
			/ imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom4.framelength)
			? (frame_length - imgsensor_info.custom4.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length
			= imgsensor_info.custom4.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		//set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk / framerate * 10
			/ imgsensor_info.custom5.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom5.framelength)
			? (frame_length - imgsensor_info.custom5.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length
			= imgsensor_info.custom1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();
		break;
	default:  //coding with  preview scenario by default
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length
			= imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		//set_dummy();
		LOG_INF("error scenario_id = %d, use preview\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
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
	default:
		break;
	}

	return ERROR_NONE;
}

//ITD: add reason bug_id CILHLJYQ-718 by huihang.zhou 2022/1/11 start
static kal_uint32 set_test_pattern_mode(kal_uint32 modes, struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
	LOG_INF("set_test_pattern_mode enable: %d", modes);
	if (modes == 2)
	{
		write_cmos_sensor(0x0600, 0x0002);
	}
	else if (((modes == 1) ||(modes == 5)) && (pdata != NULL))
	{
		write_cmos_sensor(0x0600, 0x0001);
	} else {
		write_cmos_sensor(0x0600, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = modes;
	LOG_INF("final set_test_pattern modes: %d\n", modes);
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
//ITD: add reason bug_id CILHLJYQ-718 by huihang.zhou 2022/1/11 end

/* ITD: Modify By yingfei.li 20210625 Start */
static void set_awbgain(kal_uint32 g_gain,kal_uint32 r_gain, kal_uint32 b_gain)
{
 	kal_uint32 r_gain_int = 0x0;
	kal_uint32 b_gain_int = 0x0;

	r_gain_int = r_gain * 2;
	b_gain_int = b_gain * 2;

	LOG_INF("set_awbgain: r_gain = 0x%x b_gain = 0x%x\n", r_gain_int,b_gain_int);

	write_cmos_sensor(0x0D82, r_gain_int);
	write_cmos_sensor(0x0D84, 0x0400);
	write_cmos_sensor(0x0D86, b_gain_int);
	LOG_INF("get_awbgain: r_gain = 0x%x g_gain=0x%x b_gain = 0x%x\n",read_cmos_sensor(0x0D82),read_cmos_sensor(0x0D84),read_cmos_sensor(0x0D86));
}
/* ITD: Modify By yingfei.li 20210625 End */

static kal_uint32 get_sensor_temperature(void)
{
	kal_uint32 temperature = 0;
	int temperature_convert = 0;

	temperature = (read_cmos_sensor_byte(0x0020) << 8) | read_cmos_sensor_byte(0x0021);

	if (temperature & 0x8000)
		temperature_convert = -(((~temperature)+1) / 256);
	else
		temperature_convert = temperature / 256;
	if(temperature_convert > 80)
		temperature_convert = 80;
	else if(temperature_convert < -20)
		temperature_convert = -20;

	LOG_INF("temp_c(%d), read_reg(%d)\n", temperature_convert, temperature);

	return temperature_convert;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
			     UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
#ifdef FPT_PDAF_SUPPORT
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO2_STRUCT *pvcinfo2;
#endif

#ifdef FPT_SEAMLESS_SUPPORT
	uint32_t *pAeCtrls;
	uint32_t *pScenarios;
#endif

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
/*ITD: Modify test_sensor_fusion By ISP Team Start */
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1500000;
		break;
/*ITD: Modify test_sensor_fusion By ISP Team End */
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
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
	case SENSOR_FEATURE_SET_ESHUTTER:
	    set_shutter(*feature_data);
	break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	    night_mode((BOOL) * feature_data);
	break;
	case SENSOR_FEATURE_SET_GAIN:
#ifdef FPT_SEAMLESS_SUPPORT
	    if(imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM3 || imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM5|| imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM4)
#else
		if(imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM3)
#endif
			set_gain_fullsize((UINT16) *feature_data);
	    else
			set_gain_binning((UINT16) *feature_data);
	break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
	break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
	break;
	case SENSOR_FEATURE_SET_REGISTER:
	    write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
	break;
	case SENSOR_FEATURE_GET_REGISTER:
	    sensor_reg_data->RegData = read_cmos_sensor_byte(sensor_reg_data->RegAddr);
	break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
	    // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
	    // if EEPROM does not exist in camera module.
	    *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
	    set_video_mode(*feature_data);
	break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
	    get_imgsensor_id(feature_return_para_32);
	break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
	    set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16+1));
	break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
	    set_max_framerate_by_scenario(
				(enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
	break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
	    get_default_framerate_by_scenario(
				(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
				(MUINT32 *)(uintptr_t)(*(feature_data+1)));
	break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
	    set_test_pattern_mode((UINT32)*feature_data, (struct SET_SENSOR_PATTERN_SOLID_COLOR *)(feature_data+1));
	break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
	    *feature_return_para_32 = imgsensor_info.checksum_value;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_FRAMERATE:
	    LOG_INF("current fps :%d\n", (UINT32)*feature_data_32);
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.current_fps = *feature_data_32;
	    spin_unlock(&imgsensor_drv_lock);
	break;
	case SENSOR_FEATURE_SET_HDR:
	    LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.ihdr_en = (BOOL)*feature_data_32;
	    spin_unlock(&imgsensor_drv_lock);
	break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
//ITD: add by fei.wu5 for ISZ custom5 set_awbgain 2023/9/19 start 
#ifdef FPT_SEAMLESS_SUPPORT
	    if(imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM3  || imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM4|| imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM5)
#else
		if(imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM3)
#endif
//ITD: add by fei.wu5 for ISZ custom5 set_awbgain 2023/9/19 end
			set_awbgain((UINT32)(*feature_data_32),(UINT32)*(feature_data_32 + 1), (UINT32)*(feature_data_32 + 2));
	    else
			set_awbgain((UINT32)(*feature_data_32),512,512);
	break;
	case SENSOR_FEATURE_GET_CROP_INFO:
	    LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);

	    wininfo
			= (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[7],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[8],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[9],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM4:
//ITD: add by fei.wu5 for ISZ custom5 set_awbgain 2023/9/19 start 
#ifdef FPT_SEAMLESS_SUPPORT
		case MSDK_SCENARIO_ID_CUSTOM5:
#endif
//ITD: add by fei.wu5 for ISZ custom5 set_awbgain 2023/9/19 end
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
	    *(feature_data + 1) = imgsensor_info.min_gain;
	    *(feature_data + 2) = imgsensor_info.max_gain;
	    break;
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
			*(feature_data + 2) = 2;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
		default:
			*(feature_data + 2) = 1;
			break;
        }
        break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM4:
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
	    LOG_INF("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",*feature_return_para_32);
	    *feature_para_len = 4;
	    break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom1.pclk /
			(imgsensor_info.custom1.linelength - 80))*
			imgsensor_info.custom1.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom2.pclk /
			(imgsensor_info.custom2.linelength - 80))*
			imgsensor_info.custom2.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom3.pclk /
			(imgsensor_info.custom3.linelength - 80))*
			imgsensor_info.custom3.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom4.pclk /
			(imgsensor_info.custom4.linelength - 80))*
			imgsensor_info.custom4.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom5.pclk /
			(imgsensor_info.custom5.linelength - 80))*
			imgsensor_info.custom5.grabwindow_width;


			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom4.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom5.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
#ifdef FPT_PDAF_SUPPORT
/******************** PDAF START ********************/
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n", *feature_data);
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
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
//ITD: modify ci6 portrait open pdaf by huihang.zhou 2022/3/23 start
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
//ITD: modify ci6 portrait open pdaf by huihang.zhou 2022/3/23 end
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%llu\n", *feature_data);
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[0],
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
		memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[1],
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO2:
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO2 %d\n",
			(UINT16) *feature_data);
		pvcinfo2 =
	    (struct SENSOR_VC_INFO2_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[0],
			       sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[4],
				sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[3],
			       sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[3],
			       sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[0],
			       sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[2],
			       sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		default:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[0],
			       sizeof(struct SENSOR_VC_INFO2_STRUCT));
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
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:/*lzl*/
		pr_debug("SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME\n");
		set_shutter_frame_length((UINT16)*(feature_data),
				(UINT16)(*(feature_data+1)),
				(unsigned char) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * set_shutter_frame_length() support
		 * N+1 shutter take effect (auto extend on)
		 */
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
//ITD: Modify Long Shutter By Jesse Start
	case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
		memcpy(feature_return_para_32,
		&imgsensor.ae_frm_mode, sizeof(struct IMGSENSOR_AE_FRM_MODE));
		break;
	case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
		*feature_return_para_32 = imgsensor.current_ae_effective_frame;
		break;
//ITD: Modify Long Shutter By Jesse End
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
//ITD:modify by camera driver team start
/******************** SEAMLESS START ********************/
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
		if ((feature_data + 1) != NULL)
		{
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
				break;
			case MSDK_SCENARIO_ID_CUSTOM5:
				*pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				*pScenarios = MSDK_SCENARIO_ID_CUSTOM4;
				break;
			case MSDK_SCENARIO_ID_CUSTOM4:
				*pScenarios = MSDK_SCENARIO_ID_CUSTOM1;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			default:
				*pScenarios = 0xff;
				break;
				}
			}
		}else {
			LOG_INF("input pScenarios vector is NULL!\n");
			return ERROR_INVALID_SCENARIO_ID;
		}
        LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %llu %d\n",
            *feature_data,
            *pScenarios);
        break;
/*  case SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH:
        extend_frame_length((MUINT32) *feature_data);
        break;
*/
#endif
/******************** SEAMLESS END ********************/

	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		LOG_INF("get_sensor_temperature \n");
		*feature_return_para_32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
//ITD:modify by camera driver team end
	default:
		break;
	}
	return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5KGN5SP_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	/* S5KGN5SP_MIPI_RAW_SensorInit */
