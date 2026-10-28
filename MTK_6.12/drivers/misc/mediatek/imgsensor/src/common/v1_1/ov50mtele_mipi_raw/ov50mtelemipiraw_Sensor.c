/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     OV50MTELEmipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 * Setting version:
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#define PFX "OV50MTELE"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/of_gpio.h> 
#include "ov50mtelemipiraw_Sensor.h"
#include "ov50mtele_sensor_setting.h"

#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

extern int debug_imgsensor;
#undef pr_debug
#define pr_debug(fmt, arg...)		\
	do {					\
		if (debug_imgsensor >= 1)		\
			pr_info(PFX "[%s] " fmt, __func__, ## arg);	\
	} while (0)

#define MULTI_WRITE 1
//#define FPT_SEAMLESS_SUPPORT
#define FPT_PDAF_SUPPORT 1
#define FPT_SHDR_SUPPORT 0
#define FOUR_CELL_SIZE 3568   /*size = 8006 = 0x1F46*/

static char CROSSTALK_DATA[FOUR_CELL_SIZE+2] = { 0 };
extern void ov50mtele_read_crosstalk_data(char *data);
int cam2_pin_rst = 0;
static DEFINE_SPINLOCK(imgsensor_drv_lock);
//ITD: Modify setFPS By Thermal By Jie.liu 230517 Start
static kal_uint16 fixed_framerate;
//ITD: Modify setFPS By Thermal By Jie.liu 230517 End
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV50MTELE_SENSOR_ID,
	.checksum_value = 0x91f70be9,
	.pre = {
		.pclk = 54000000,
		.linelength = 480,
		.framelength = 3750,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3632,
		.grabwindow_height =2720,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 300,
		.mipi_pixel_rate = 540000000,
	},
	.cap = {
		.pclk = 54000000,
		.linelength = 480,
		.framelength = 3750,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3632,
		.grabwindow_height =2720,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 300,
		.mipi_pixel_rate = 540000000,
	},
	.normal_video = {
		.pclk = 54000000,
		.linelength = 480,
		.framelength = 3750,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3632,
		.grabwindow_height =2048,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 300,
		.mipi_pixel_rate = 540000000,
	},
	.hs_video = {
		.pclk = 75000000,
		.linelength = 480,
		.framelength = 1304,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 1200,
		.mipi_pixel_rate = 540000000,
	},
	.slim_video = {
		.pclk = 75000000,
		.linelength = 480,
		.framelength = 1304,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 1200,
		.mipi_pixel_rate = 540000000,
	},
	.custom1 = {
		.pclk = 54000000,
		.linelength = 480,
		.framelength = 3750,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3632,
		.grabwindow_height =2720,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 300,
		.mipi_pixel_rate = 540000000,
	},
	.custom2 = {
		.pclk = 64000000,
		.linelength = 480,
		.framelength = 2222,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3632,
		.grabwindow_height =2048,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 600,
		.mipi_pixel_rate = 540000000,
	},
	.custom3 = {
		.pclk = 75000000,
		.linelength = 480,
		.framelength = 15616,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 7264,
		.grabwindow_height = 5440,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 100,
		.mipi_pixel_rate = 540000000,
	},
	.custom4 = {
		.pclk = 75000000,
		.linelength = 394,
		.framelength = 6342,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3632,
		.grabwindow_height = 2720,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 300,
		.mipi_pixel_rate = 540000000,
	},
	.custom5 = {
		.pclk = 75000000,
		.linelength = 394,
		.framelength = 6342,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3632,
		.grabwindow_height = 2720,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 300,
		.mipi_pixel_rate = 540000000,
	},
	.custom6 = {
		.pclk = 75000000,
		.linelength = 394,
		.framelength = 6342,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3632,
		.grabwindow_height = 2720,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 300,
		.mipi_pixel_rate = 540000000,
	},
	.margin = 20,
	.min_shutter = 10,
	.min_gain = 64,
	.max_gain = 4080, //63.75
	.min_gain_iso = 50,
	.exp_step = 4,
	.gain_step = 1,
	.gain_type = 1,
	.max_frame_length = 0xffffeb,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 10,

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
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.i2c_addr_table = {0x6C, 0xff},
	.i2c_speed = 1000,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,
	.gain = 0x100,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 30,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = 0,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x6C,
	.current_ae_effective_frame = 2,
	.sat_mode = 0,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[11] = {
	{ 8192,  6144,  464,  352, 7264,  5440,  3632,  2720, 0000,  0000,  3632,  2720,  0,  0,  3632,  2720},	/*Preview*/
	{ 8192,  6144,  464,  352, 7264,  5440,  3632,  2720, 0000,  0000,  3632,  2720,  0,  0,  3632,  2720},	/*capture*/
	{ 8192,  6144,  464, 1024, 7264,  4096,  3632,  2048, 0000,  0000,  3632,  2048,  0,  0,  3632,  2048},	/*normal video*/
	{ 8192,  6144,  512,  912, 7680,  4320,  1920,  1080, 0000,  0000,  1920,  1080,  0,  0,  1920,  1080},	/*hs video*/
	{ 8192,  6144,  512,  912, 7680,  4320,  1920,  1080, 0000,  0000,  1920,  1080,  0,  0,  1920,  1080},	/*slim video*/
	{ 8192,  6144,  464,  352, 7264,  5440,  3632,  2720, 0000,  0000,  3632,  2720,  0,  0,  3632,  2720},	/*custom1 DualCam */
	{ 8192,  6144,  464, 1024, 7264,  4096,  3632,  2048, 0000,  0000,  3632,  2048,  0,  0,  3632,  2048},	/*custom2 60fps*/
	{ 8192,  6144,  464,  352, 7264,  5440,  7264,  5440, 0000,  0000,  7264,  5440,  0,  0,  7264,  5440},	/*custom3 fullsize*/
	{ 8192,  6144,  2280,1712, 3632,  2720,  3632,  2720, 0000,  0000,  3632,  2720,  0,  0,  3632,  2720},	/*custom4 insensorzoom*/
	{ 8192,  6144,  2280,1712, 3632,  2720,  3632,  2720, 0000,  0000,  3632,  2720,  0,  0,  3632,  2720},	/*custom5 insensorzoom*/
	{ 8192,  6144,  2280,1712, 3632,  2720,  3632,  2720, 0000,  0000,  3632,  2720,  0,  0,  3632,  2720},	/*custom6 insensorzoom quad*/
};

#if FPT_PDAF_SUPPORT
/*PD information update*/
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	 .i4OffsetX = 24,
	 .i4OffsetY = 0,
	 .i4PitchX = 16,
	 .i4PitchY = 16,
	 .i4PairNum = 8,
	 .i4SubBlkW = 8,
	 .i4SubBlkH = 4,
	 .i4PosR = {{30, 2}, {38, 2}, {26, 6}, {34, 6},
		{30, 10}, {38, 10}, {26, 14}, {34, 14} },
	 .i4PosL = {{29, 2}, {37, 2}, {25, 6}, {33, 6},
		{29, 10}, {37, 10}, {25, 14}, {33, 14} },
	 .iMirrorFlip = 0,
	 .i4BlockNumX = 224,
	 .i4BlockNumY = 170,
	 .i4Crop = {
		{0,0},{0,0},{0,0},{0,0},{0,0},
		{0,0},{0,0},{0,0},{0,0},{0,0}
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_vr_pd_info = {
	 .i4OffsetX = 24,
	 .i4OffsetY = 0,
	 .i4PitchX = 16,
	 .i4PitchY = 16,
	 .i4PairNum = 8,
	 .i4SubBlkW = 8,
	 .i4SubBlkH = 4,
	 .i4PosR = {{30, 2}, {38, 2}, {26, 6}, {34, 6},
		{30, 10}, {38, 10}, {26, 14}, {34, 14} },
	 .i4PosL = {{29, 2}, {37, 2}, {25, 6}, {33, 6},
		{29, 10}, {37, 10}, {25, 14}, {33, 14} },
	 .iMirrorFlip = 0,
	 .i4BlockNumX = 224,
	 .i4BlockNumY = 128,
	 .i4Crop = {
		{0,0},{0,0},{0,0},{0,0},{0,0},
		{0,0},{0,0},{0,0},{0,0},{0,0}
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_fullcrop_pd_info = {
	 .i4OffsetX = 24,
	 .i4OffsetY = 0,
	 .i4PitchX = 16,
	 .i4PitchY = 16,
	 .i4PairNum = 8,
	 .i4SubBlkW = 8,
	 .i4SubBlkH = 4,
	 .i4PosR = {{30, 2}, {38, 2}, {26, 6}, {34, 6},
		{30, 10}, {38, 10}, {26, 14}, {34, 14} },
	 .i4PosL = {{29, 2}, {37, 2}, {25, 6}, {33, 6},
		{29, 10}, {37, 10}, {25, 14}, {33, 14} },
	 .iMirrorFlip = 0,
	 .i4BlockNumX = 112,
	 .i4BlockNumY = 128,
	 .i4Crop = {
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {2280, 1712}
	},
};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
    /* Preview mode setting 2000(pxiel)*1500 */
    {
        0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
        0x00, 0x2b, 0x0E30, 0xAA0, 0x00, 0x00, 0x0280, 0x0001,
        0x01, 0x2B, 0x1c0, 0x550, 0x03, 0x00, 0x0000, 0x0000
    },
    /* Capture mode setting 2000(pxiel)*1500 */
    {
        0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
        0x00, 0x2b, 0x0E30, 0x800, 0x00, 0x00, 0x0280, 0x0001,
        0x01, 0x2B, 0x1c0, 0x400, 0x03, 0x00, 0x0000, 0x0000
    },
    /* Video mode setting 2000(pxiel)*1124 */
    {
        0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
        0x00, 0x2b, 0x0E30, 0xAA0, 0x00, 0x00, 0x0280, 0x0001,
        0x01, 0x2B, 0x01C0, 0x2A8, 0x03, 0x00, 0x0000, 0x0000
    },
};
#endif

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765	/*trans# max is 255, each 3 bytes*/
#else
#define I2C_BUFFER_LEN 3
#endif

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
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;
		}
#if MULTI_WRITE
		if ((I2C_BUFFER_LEN - tosend) < 3 ||
			len == IDX ||
			addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend,
				imgsensor.i2c_write_id,
				3, imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;

#endif
	}
	return 0;
}

#if 1
#define I2C_BUF_LIMIT 1020
#define ADDR_SIZE 2

#define BURST_DATA_SAFE_MAX (I2C_BUF_LIMIT - ADDR_SIZE) // 1020 - 2 = 1018

static kal_uint8 g_burstBuf[I2C_BUF_LIMIT];

/* Continuous burst write: 16-bit address + 8-bit data */
static kal_uint16 burst_write_cmos_sensor(kal_uint16 start_addr, kal_uint8 *para, kal_uint32 len)
{
	char buf[I2C_BUF_LIMIT];
	kal_uint32 pos = 0;

	buf[pos++] = (char)(start_addr >> 8);
	buf[pos++] = (char)(start_addr & 0xFF);

	for (kal_uint32 i = 0; i < len; i++)
	{
		buf[pos++] = para[i];
	}

#if MULTI_WRITE
	iBurstWriteReg_multi(buf, pos, imgsensor.i2c_write_id, pos, imgsensor_info.i2c_speed);
#else
	iWriteRegI2CTiming(buf, pos, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
#endif
	return 0;
}

/* Table write for non-continuous regs */
static kal_uint16 table_write_cmos_sensor_1(kal_uint16 *para, kal_uint32 len)
{
	char buf[I2C_BUF_LIMIT];
	kal_uint32 pos = 0;
	for (kal_uint32 i = 0; i < len; i += 2)
	{
		buf[pos++] = (char)(para[i] >> 8);
		buf[pos++] = (char)(para[i] & 0xFF);
		buf[pos++] = (char)(para[i + 1] & 0xFF);
		if ((I2C_BUF_LIMIT - pos) < 3)
		{
			iBurstWriteReg_multi(buf, pos, imgsensor.i2c_write_id, 3, imgsensor_info.i2c_speed);
			pos = 0;
		}
	}
	if (pos > 0)
		iBurstWriteReg_multi(buf, pos, imgsensor.i2c_write_id, 3, imgsensor_info.i2c_speed);
	return 0;
}

static kal_uint16 BurstContinue_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	kal_uint32 continLen = 0;
	kal_uint32 item = 0, nextItem, IDX;
	kal_uint8 endflag = 0;
	kal_uint8 lastMode = 0, mode = 0; // 0:unknown 1:continue 2:non-continuous
	kal_uint32 continStartItem = 0, continCount = 0;
	kal_uint32 unContinStartItem = 0, unContinCount = 0;
	kal_uint16 continCurAddr, continNextAddr;
	kal_uint16 *pRegSrc, regaddr, lastAddr;
	kal_uint16 tempdata;
	const kal_uint32 dataOffset = 1;

	for (item = 0; item < len; item = nextItem)
	{
		continCurAddr = para[item];
		nextItem = item + 2;

		if (nextItem < len)
			continNextAddr = para[nextItem];
		else
			endflag = 1;

		if (!endflag)
		{
			if ((continCurAddr + dataOffset) == continNextAddr)
			{
				if (mode != 1)
				{
					continCount = 2;
					continStartItem = item;
					mode = 1;
				}
				continCount += 2;
			}
			else
			{
				if (mode == 0)
				{
					unContinCount = 0;
					unContinStartItem = item;
					mode = 2;
					unContinCount += 2;
				}
				else if (mode == 1)
				{
					mode = 0;
				}
				else
				{
					unContinCount += 2;
				}
			}
		}
		else
		{
			if (mode == 2)
			{
				unContinCount += 2;
			}
			else if (mode == 0)
			{
				unContinCount = 0;
				unContinStartItem = item;
				mode = 2;
				unContinCount += 2;
			}
		}

		if ((lastMode != mode) || endflag)
		{
			/* Continuous block handling */
			if ((lastMode == 1 && mode != 1) || (endflag && mode == 1))
			{
				pRegSrc = para + continStartItem;
				regaddr = pRegSrc[0];
				continLen = 0;

				for (IDX = 1; IDX < continCount;)
				{
					tempdata = pRegSrc[IDX];
					g_burstBuf[continLen++] = (kal_uint8)(tempdata & 0xFF);
					lastAddr = pRegSrc[IDX - 1];
					IDX += 2;

					if (continLen >= BURST_DATA_SAFE_MAX || IDX >= continCount)
					{
						burst_write_cmos_sensor(regaddr, g_burstBuf, continLen);
						continLen = 0;
						if (IDX < continCount)
						{
							regaddr = lastAddr + dataOffset;
						}
					}
				}
			}
			/* Non-continuous block handling */
			else if ((lastMode == 2 && mode != 2) || (endflag && mode == 2))
			{
				pRegSrc = para + unContinStartItem;
				table_write_cmos_sensor_1(pRegSrc, unContinCount);
			}
			lastMode = mode;
		}
	}
	return 0;
}
#endif

#define _I2C_BUF_SIZE 256
static kal_uint16 _i2c_data[_I2C_BUF_SIZE];
static unsigned int _size_to_write;
static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len);
static void commit_write_sensor(void)
{
	if (_size_to_write) {
		table_write_cmos_sensor(_i2c_data, _size_to_write);
		memset(_i2c_data, 0x0, sizeof(_i2c_data));
		_size_to_write = 0;
	}
}
static void set_cmos_sensor_8(kal_uint16 reg, kal_uint16 val)
{
	if (_size_to_write + 2 >= _I2C_BUF_SIZE)
		commit_write_sensor();
		
	_i2c_data[_size_to_write++] = reg;
	_i2c_data[_size_to_write++] = val;
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pusendcmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};
	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	write_cmos_sensor(0x380c, imgsensor.line_length >> 8);
	write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);
	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
			frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
	imgsensor.frame_length = imgsensor_info.max_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;
	}
	if (min_framelength_en)
	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);

	set_dummy();
}

static void write_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	// OV Recommend Solution
	// if shutter bigger than frame_length, should extend frame length first
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
	imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	// Framelength should be an even number
	shutter = (shutter >> 1) << 1;
	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else if (realtime_fps >= 593 && realtime_fps <= 607)
			set_max_framerate(592, 0);
		else {
			// Extend frame length
			write_cmos_sensor(0x3208, 0x00);
			write_cmos_sensor(0x3840, imgsensor.frame_length >> 16);
			write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
			write_cmos_sensor(0x3208, 0x10);
			write_cmos_sensor(0x3208, 0xa0);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(0x3208, 0x00);
		write_cmos_sensor(0x3840, imgsensor.frame_length >> 16);
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x3208, 0x10);
		write_cmos_sensor(0x3208, 0xa0);

	}

	// Update Shutter
	set_cmos_sensor_8(0x3208, 0x01);
	set_cmos_sensor_8(0x3840, imgsensor.frame_length >> 16);
	set_cmos_sensor_8(0x380e, imgsensor.frame_length >> 8);
	set_cmos_sensor_8(0x380f, imgsensor.frame_length & 0xFF);
	set_cmos_sensor_8(0x3500, (shutter >> 16) & 0xFF);
	set_cmos_sensor_8(0x3501, (shutter >> 8) & 0xFF);
	set_cmos_sensor_8(0x3502, (shutter)  & 0xFF);
	set_cmos_sensor_8(0x3208, 0x11);
	set_cmos_sensor_8(0x3208, 0xa1);


	imgsensor.current_ae_effective_frame = 2;
    LOG_INF("aaa reg shutter =%d \n", ((read_cmos_sensor(0x3500) << 16) | (read_cmos_sensor(0x3501) << 8) | (read_cmos_sensor(0x3502))));
	LOG_INF("aaa reg linelength =%d imgsensor.line_length = %d\n", ((read_cmos_sensor(0x380c) << 8) | (read_cmos_sensor(0x380d))),imgsensor.line_length);
	LOG_INF("aaa reg framelength =%d, imgsensor.frame_length is %d\n", ((read_cmos_sensor(0x3840) << 16) |
				(read_cmos_sensor(0x380e) << 8) | read_cmos_sensor(0x380f)), imgsensor.frame_length);
	LOG_INF("bbb shutter =%d, framelength =%d, realtime_fps =%d\n",
	        shutter, imgsensor.frame_length, realtime_fps);
}
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	write_shutter(shutter);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 iReg = 0x0000;

	//platform 1xgain = 64, sensor driver 1*gain = 0x100
	iReg = gain*256/BASEGAIN;
	return iReg;		/* sensorGlobalGain */
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;
	unsigned long flags;

	reg_gain = gain2reg(gain);
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.gain = reg_gain;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
	set_cmos_sensor_8(0x03508, (reg_gain >> 8));
	set_cmos_sensor_8(0x03509, (reg_gain&0xff));
	commit_write_sensor();

	return gain;
}

static kal_uint16 set_gain_fullsize(kal_uint16 gain)
{
	kal_uint16 reg_gain;
	unsigned long flags;

	if (gain < BASEGAIN || gain > (BASEGAIN * 255) >> 5) {
		LOG_INF("Error gain setting");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > (BASEGAIN * 255) >> 5)
			gain = (BASEGAIN * 255) >> 5; //7.96875x
	}
	reg_gain = gain2reg(gain);
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.gain = reg_gain;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	LOG_INF("set_gain_fullsize gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
	set_cmos_sensor_8(0x03508, (reg_gain >> 8));
	set_cmos_sensor_8(0x03509, (reg_gain&0xff));
	commit_write_sensor();

	return gain;
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
	/* LOG_INF("shutter =%d, frame_time =%d\n", shutter, frame_time); */

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger
	 * than frame exposure
	 */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure
	 * lines must be updated here.
	 */

	/* OV Recommend Solution */
	/* if shutter bigger than frame_length,
	 * should extend frame length first
	 */
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

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;

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
	}
	LOG_INF("set_shutter_frame_length set shutter =%d, imgsensor.frame_length = %d,frame_length =%d\n", shutter, imgsensor.frame_length, frame_length);
	
	// Extend frame length
	/* Update Shutter */
	write_cmos_sensor(0x3208, 0x01);
	write_cmos_sensor(0x3840, imgsensor.frame_length >> 16);
	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x3500, (shutter >> 16) & 0xFF);
	write_cmos_sensor(0x3501, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x3502, (shutter)  & 0xFF);
	write_cmos_sensor(0x3208, 0x11);
	write_cmos_sensor(0x3208, 0xa1);
}


static void ihdr_write_shutter_gain(kal_uint16 le,
				kal_uint16 se, kal_uint16 gain)
{
}

static void night_mode(kal_bool enable)
{
}
#if FPT_SHDR_SUPPORT
//add 60A stagger HDR start
static void hdr_write_tri_shutter(kal_uint16 le, kal_uint16 me, kal_uint16 se)
{
	kal_uint16 realtime_fps = 0;

	le = (kal_uint16)max(imgsensor_info.min_shutter, (kal_uint32)le);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = max((kal_uint32)(le + me + se + imgsensor_info.margin),
		imgsensor.min_frame_length);
	imgsensor.frame_length = min(imgsensor.frame_length, imgsensor_info.max_frame_length);
	spin_unlock(&imgsensor_drv_lock);

	LOG_INF("E!  le:0x%x,le:0x%x se:0x%x autoflicker_en %d frame_length %d\n",
		 le,me, se, imgsensor.autoflicker_en, imgsensor.frame_length);

	if (imgsensor.autoflicker_en) {
		realtime_fps =
			imgsensor.pclk / imgsensor.line_length * 10 /
			imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else{
			write_cmos_sensor(0x3840, imgsensor.frame_length >> 16);
			write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
		}
	} else {
		write_cmos_sensor(0x3840, imgsensor.frame_length >> 16);
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}

	write_cmos_sensor(0x3500, (le >> 16) & 0xFF);
	write_cmos_sensor(0x3501, (le >> 8) & 0xFF);
	write_cmos_sensor(0x3502, (le)  & 0xFF);

	/* Muddle exposure */
	if (me != 0) {
		write_cmos_sensor(0x3580, (me >> 16) & 0xFF);
		write_cmos_sensor(0x3581, (me >> 8) & 0xFF);
		write_cmos_sensor(0x3582, (me)  & 0xFF);
	}

	/* Short exposure */
	write_cmos_sensor(0x3540, (se >> 16) & 0xFF);
	write_cmos_sensor(0x3541, (se >> 8) & 0xFF);
	write_cmos_sensor(0x3542, (se)  & 0xFF);

	LOG_INF("L!  le:0x%x, me:0x%x, se:0x%x\n", le, me, se);
}

static void hdr_write_tri_gain(kal_uint16 lgain, kal_uint16 mg, kal_uint16 sg)
{
	kal_uint16 reg_lg, reg_mg, reg_sg;

	reg_lg = gain2reg(lgain);
	reg_mg = gain2reg(mg);
	reg_sg = gain2reg(sg);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_lg;
	spin_unlock(&imgsensor_drv_lock);

	write_cmos_sensor(0x3508, (reg_lg >> 8));
	write_cmos_sensor(0x3509, (reg_lg&0xff));

	if (mg != 0) {
	write_cmos_sensor(0x3588, (reg_mg >> 8));
	write_cmos_sensor(0x3589, (reg_mg&0xff));
	}
	
	/* Short Gian */
	write_cmos_sensor(0x3548, (reg_sg >> 8));
	write_cmos_sensor(0x3549, (reg_sg&0xff));
	
	LOG_INF(
		"lgain:0x%x, reg_lg:0x%x, sg:0x%x, reg_mg:0x%x, mg:0x%x, reg_sg:0x%x\n",
		lgain, reg_lg, mg, reg_mg, sg, reg_sg);
}
//add 60A stagger HDR end
#endif

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
			write_cmos_sensor(0x3208, 0x01);
			write_cmos_sensor(0x3016, 0xb9);
			write_cmos_sensor(0x3014, 0xd6);
			write_cmos_sensor(0x3017, 0xf5);
			table_write_cmos_sensor(addr_data_pair_preview_ov50mtele,
				sizeof(addr_data_pair_preview_ov50mtele) / sizeof(kal_uint16));
			write_cmos_sensor(0x3016, 0xb0);
			write_cmos_sensor(0x3014, 0xd2);
			write_cmos_sensor(0x3017, 0xf0);
			write_cmos_sensor(0x3208, 0x11);
			write_cmos_sensor(0x3208, 0xa1);
			if (ae_ctrl) {
				LOG_INF("call SENSOR_SCENARIO_ID_NORMAL_CAPTURE %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1]);
				set_shutter(ae_ctrl[SHUTTER_NE_FRM_1]);
				set_gain(ae_ctrl[GAIN_NE_FRM_1]);
			}
			// setGPHon(KAL_FALSE);
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
			write_cmos_sensor(0x3208, 0x01);
			write_cmos_sensor(0x3016, 0xb9);
			write_cmos_sensor(0x3014, 0xd6);
			write_cmos_sensor(0x3017, 0xf5);
			table_write_cmos_sensor(addr_data_pair_custom5_ov50mtele,
				sizeof(addr_data_pair_custom5_ov50mtele) / sizeof(kal_uint16));
			write_cmos_sensor(0x3016, 0xb0);
			write_cmos_sensor(0x3014, 0xd2);
			write_cmos_sensor(0x3017, 0xf0);
			write_cmos_sensor(0x3208, 0x11);
			write_cmos_sensor(0x3208, 0xa1);
			if (ae_ctrl) {
				LOG_INF("call SENSOR_SCENARIO_ID_CUSTOM5 %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1],ae_ctrl[SHUTTER_SE_FRM_1],ae_ctrl[GAIN_SE_FRM_1]);
				set_shutter(ae_ctrl[SHUTTER_NE_FRM_1]);
				set_gain_fullsize(ae_ctrl[GAIN_NE_FRM_1]);
			}
          //  setGPHon(KAL_FALSE);
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

			LOG_INF("seamless switch 5-exp!\n");
			write_cmos_sensor(0x3208, 0x01);
			write_cmos_sensor(0x3016, 0xb9);
			write_cmos_sensor(0x3014, 0xd6);
			write_cmos_sensor(0x3017, 0xf5);
			table_write_cmos_sensor(addr_data_pair_custom6_ov50mtele,
				sizeof(addr_data_pair_custom6_ov50mtele) / sizeof(kal_uint16));
			write_cmos_sensor(0x3016, 0xb0);
			write_cmos_sensor(0x3014, 0xd2);
			write_cmos_sensor(0x3017, 0xf0);
			write_cmos_sensor(0x3208, 0x11);
			write_cmos_sensor(0x3208, 0xa1);
			if (ae_ctrl) {
				LOG_INF("call SENSOR_SCENARIO_ID_CUSTOM6 %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1],ae_ctrl[SHUTTER_SE_FRM_1],ae_ctrl[GAIN_SE_FRM_1]);
				set_shutter(ae_ctrl[SHUTTER_NE_FRM_1]);
				set_gain_fullsize(ae_ctrl[GAIN_NE_FRM_1]);
			}
          //  setGPHon(KAL_FALSE);
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

static void sensor_init(void)
{
	LOG_INF("ov50mtele_init +\n");
	BurstContinue_write_cmos_sensor(
		addr_data_pair_init_ov50mtele,
		sizeof(addr_data_pair_init_ov50mtele) / sizeof(kal_uint16));
	LOG_INF("ov50mtele_init -\n");
}

static void preview_setting(void)
{
	LOG_INF("preview_setting\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3002, 0x00);
	table_write_cmos_sensor(
		addr_data_pair_preview_ov50mtele,
		sizeof(addr_data_pair_preview_ov50mtele) /
		sizeof(kal_uint16));
}

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("capture_setting currefps = %d\n",currefps);
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3002, 0x00);
	table_write_cmos_sensor(
		addr_data_pair_capture_ov50mtele,
		sizeof(addr_data_pair_capture_ov50mtele) /
		sizeof(kal_uint16));
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("normal_video_setting\n");
	//vsync
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3002, 0x80);
	write_cmos_sensor(0x3841, 0x00);
	write_cmos_sensor(0x381a, 0x00);
	write_cmos_sensor(0x381b, 0x01);
	write_cmos_sensor(0x381c, 0x00);
	write_cmos_sensor(0x381d, 0xf0);
	write_cmos_sensor(0x381e, 0x00);
	write_cmos_sensor(0x368f, 0x04);
	write_cmos_sensor(0x381f, 0x01);
	write_cmos_sensor(0x3864, 0x04);

	table_write_cmos_sensor(
		addr_data_pair_video_ov50mtele,
		sizeof(addr_data_pair_video_ov50mtele) /
		sizeof(kal_uint16));
}

static void hs_video_setting(void)
{
	LOG_INF("hs_video_setting\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3002, 0x00);
	table_write_cmos_sensor(
		addr_data_pair_hs_video_ov50mtele,
		sizeof(addr_data_pair_hs_video_ov50mtele) /
		sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	LOG_INF("slim_video_setting\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3002, 0x00);
	table_write_cmos_sensor(
		addr_data_pair_slim_video_ov50mtele,
		sizeof(addr_data_pair_slim_video_ov50mtele) /
		sizeof(kal_uint16));
}

static void custom1_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3002, 0x00);
	table_write_cmos_sensor(addr_data_pair_custom1_ov50mtele,
							sizeof(addr_data_pair_custom1_ov50mtele) / sizeof(kal_uint16));
} /*	custom1_setting  */

static void custom2_setting(void)
{
	LOG_INF("E\n");
	// vsync
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3002, 0x80);
	write_cmos_sensor(0x3841, 0x00);
	write_cmos_sensor(0x381a, 0x00);
	write_cmos_sensor(0x381b, 0x01);
	write_cmos_sensor(0x381c, 0x00);
	write_cmos_sensor(0x381d, 0xf0);
	write_cmos_sensor(0x381e, 0x00);
	write_cmos_sensor(0x368f, 0x04);
	write_cmos_sensor(0x381f, 0x01);
	write_cmos_sensor(0x3864, 0x04);

	table_write_cmos_sensor(addr_data_pair_custom2_ov50mtele,
							sizeof(addr_data_pair_custom2_ov50mtele) / sizeof(kal_uint16));
}	/*	custom2_setting  */

static void custom3_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3002, 0x00);
	table_write_cmos_sensor(addr_data_pair_custom3_ov50mtele,
							sizeof(addr_data_pair_custom3_ov50mtele) / sizeof(kal_uint16));
} /*	custom3_setting  */

static void custom4_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3002, 0x00);
	table_write_cmos_sensor(addr_data_pair_custom4_ov50mtele,
							sizeof(addr_data_pair_custom4_ov50mtele) / sizeof(kal_uint16));
} /*	custom4_setting  */

static void custom5_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3002, 0x00);
	table_write_cmos_sensor(addr_data_pair_custom5_ov50mtele,
							sizeof(addr_data_pair_custom5_ov50mtele) / sizeof(kal_uint16));
} /*	custom5_setting  */

static void custom6_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3002, 0x00);
	table_write_cmos_sensor(addr_data_pair_custom6_ov50mtele,
		sizeof(addr_data_pair_custom6_ov50mtele) / sizeof(kal_uint16));
}	/*	custom5_setting  */

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x300a) << 16) |
		(read_cmos_sensor(0x300b) << 8) | read_cmos_sensor(0x300c));
}

#define CHIP_ID_SIZE 16
static kal_uint32 ov50mtele_get_chip_id(unsigned char *chip_id)
{
	int i = 0;
	unsigned char chipIdData[36];

	//sensor_init();
	write_cmos_sensor(0x0103, 0x01);
	write_cmos_sensor(0x3d84, 0x00);
	write_cmos_sensor(0x3d85, 0x0b);
	write_cmos_sensor(0x0100, 0x01);

	mdelay(10);

	for (i = 0; i < CHIP_ID_SIZE; i++) {
		chipIdData[i] = read_cmos_sensor(0xc000 + i);//OTP data read
		pr_info("ov50mtele_get_chip_id chipIdData[%d], %02X", i, chipIdData[i]);
		sprintf(chip_id+2*i, "%02X",chipIdData[i]);
	}

	pr_info("ov50mtele_get_chip_id chip_id is %s", chip_id);

	return 0;
}

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
			pr_info("WYT : return_sensor_id: 0x%x sensor_id: 0x%x\n", *sensor_id,imgsensor_info.sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_info("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				ov50mtele_read_crosstalk_data(&CROSSTALK_DATA[2]);
				return ERROR_NONE;
			}
			retry--;
		} while (retry > 0);
		i++;
		retry = 1;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		pr_info("get_imgsensor_id: 0x%x fail\n", *sensor_id);
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 1;
	kal_uint32 sensor_id = 0;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_info("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id) {
		pr_info("Open sensor id: 0x%x fail\n", sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	sensor_init();
	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
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
	imgsensor.sat_mode = 0;
	return ERROR_NONE;
}   /*  close  */

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor.current_fps;
	//imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.current_fps = imgsensor.current_fps;
	//imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;
}   /* capture() */

static kal_uint32 normal_video(
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	//imgsensor.current_fps = 300;
	//imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	return ERROR_NONE;
}

static kal_uint32 hs_video(
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	return ERROR_NONE;
}

static kal_uint32 slim_video(
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	return ERROR_NONE;
}

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
    custom4_setting();
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
    custom5_setting();
    return ERROR_NONE;
}   /*  Custom5   */

static kal_uint32 Custom6(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM6;
    imgsensor.pclk = imgsensor_info.custom6.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom6.linelength;
    imgsensor.frame_length = imgsensor_info.custom6.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom6.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom6_setting();
    return ERROR_NONE;
}   /*  Custom6   */

static kal_uint32 get_resolution(
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	sensor_resolution->SensorFullWidth =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;

    sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

    sensor_resolution->SensorCustom2Width =
		imgsensor_info.custom2.grabwindow_width;
    sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;

    sensor_resolution->SensorCustom3Width =
		imgsensor_info.custom3.grabwindow_width;
    sensor_resolution->SensorCustom3Height =
		imgsensor_info.custom3.grabwindow_height;

    sensor_resolution->SensorCustom4Width =
		imgsensor_info.custom4.grabwindow_width;
    sensor_resolution->SensorCustom4Height =
		imgsensor_info.custom4.grabwindow_height;

    sensor_resolution->SensorCustom5Width =
		imgsensor_info.custom5.grabwindow_width;
    sensor_resolution->SensorCustom5Height =
		imgsensor_info.custom5.grabwindow_height;

    sensor_resolution->SensorCustom6Width =
		imgsensor_info.custom6.grabwindow_width;
    sensor_resolution->SensorCustom6Height =
		imgsensor_info.custom6.grabwindow_height;

	return ERROR_NONE;
}   /*  get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
		      MSDK_SENSOR_INFO_STRUCT *sensor_info,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	if (scenario_id == 0)
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;

    sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
    sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
    sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
    sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
    sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;
	sensor_info->Custom6DelayFrame = imgsensor_info.custom6_delay_frame;
	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
#if FPT_PDAF_SUPPORT
/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode*/
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;
#else
	sensor_info->PDAF_Support = 0;
#endif

//add for 64B staggerhdr start
	//sensor_info->HDR_Support = HDR_SUPPORT_STAGGER_DOL;
//add for 64B staggerhdr end

	sensor_info->HDR_Support = 0; /*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR*/
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
	sensor_info->SensorHightSampling = 0;   // 0 is default 1x
	sensor_info->SensorPacketECCOrder = 1;

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

		sensor_info->SensorGrabStartX =
			imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.normal_video.starty;

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
		sensor_info->SensorGrabStartX =
			imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom4.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom5.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom6.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom6.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom6.mipi_data_lp2hs_settle_dc;
	break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
	break;
	}

	return ERROR_NONE;
}   /*  get_info  */

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
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
		Custom1(image_window, sensor_config_data);
	break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		Custom2(image_window, sensor_config_data);
	break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		Custom3(image_window, sensor_config_data);
	break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		Custom4(image_window, sensor_config_data);
	break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		Custom5(image_window, sensor_config_data);
	break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		Custom6(image_window, sensor_config_data);
	break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
	return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}   /* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
	if (framerate == 0)
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

static kal_uint32 set_auto_flicker_mode(kal_bool enable,
			UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n",
		enable, framerate);

	spin_lock(&imgsensor_drv_lock);
	if (enable) //enable auto flicker
	imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 framerate)
{
	kal_uint32 frameHeight;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	if (framerate == 0)
		return ERROR_NONE;
//ITD: Modify setFPS By Thermal By Jie.liu 230517 Start
	fixed_framerate = framerate;
//ITD: Modify setFPS By Thermal By Jie.liu 230517 End
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	    frameHeight = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.pre.framelength) ?
			(frameHeight - imgsensor_info.pre.framelength):0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
	    frameHeight = imgsensor_info.normal_video.pclk / framerate * 10 /
				imgsensor_info.normal_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.normal_video.framelength) ?
		(frameHeight - imgsensor_info.normal_video.framelength):0;
	    imgsensor.frame_length = imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    //imgsensor.line_length = imgsensor.line_length/2;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	    frameHeight = imgsensor_info.cap.pclk / framerate * 10 /
			imgsensor_info.cap.linelength;
	    spin_lock(&imgsensor_drv_lock);

		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.cap.framelength) ?
			(frameHeight - imgsensor_info.cap.framelength):0;
	    imgsensor.frame_length = imgsensor_info.cap.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    frameHeight = imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.hs_video.framelength) ?
			(frameHeight - imgsensor_info.hs_video.framelength):0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    frameHeight = imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.slim_video.framelength) ?
			(frameHeight - imgsensor_info.slim_video.framelength):0;
	    imgsensor.frame_length = imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
	    frameHeight = imgsensor_info.custom1.pclk / framerate * 10 /
			imgsensor_info.custom1.linelength;
	    spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.custom1.framelength) ?
			(frameHeight - imgsensor_info.custom1.framelength):0;
	    imgsensor.frame_length = imgsensor_info.custom1.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM2:
	    frameHeight = imgsensor_info.custom2.pclk / framerate * 10 /
			imgsensor_info.custom2.linelength;
	    spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.custom2.framelength) ?
			(frameHeight - imgsensor_info.custom2.framelength):0;
	    imgsensor.frame_length = imgsensor_info.custom2.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM3:
	    frameHeight = imgsensor_info.custom3.pclk / framerate * 10 /
			imgsensor_info.custom3.linelength;
	    spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.custom3.framelength) ?
			(frameHeight - imgsensor_info.custom3.framelength):0;
	    imgsensor.frame_length = imgsensor_info.custom3.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM4:
	    frameHeight = imgsensor_info.custom4.pclk / framerate * 10 /
			imgsensor_info.custom4.linelength;
	    spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.custom4.framelength) ?
			(frameHeight - imgsensor_info.custom4.framelength):0;
	    imgsensor.frame_length = imgsensor_info.custom4.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM5:
	    frameHeight = imgsensor_info.custom5.pclk / framerate * 10 /
			imgsensor_info.custom5.linelength;
	    spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.custom5.framelength) ?
			(frameHeight - imgsensor_info.custom5.framelength):0;
	    imgsensor.frame_length = imgsensor_info.custom5.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
		imgsensor.line_length = imgsensor.line_length/2;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM6:
	    frameHeight = imgsensor_info.custom6.pclk / framerate * 10 /
			imgsensor_info.custom6.linelength;
	    spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.custom6.framelength) ?
			(frameHeight - imgsensor_info.custom6.framelength):0;
	    imgsensor.frame_length = imgsensor_info.custom6.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
		imgsensor.line_length = imgsensor.line_length/2;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	default:  //coding with  preview scenario by default
	    frameHeight = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.pre.framelength) ?
			(frameHeight - imgsensor_info.pre.framelength):0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	}
	return ERROR_NONE;
}

//ITD: Modify setFPS By Thermal By Jie.liu 230517 Start
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
//ITD: Modify setFPS By Thermal By Jie.liu 230517 End

static kal_uint32 get_default_framerate_by_scenario(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 *framerate)
{
	if (scenario_id == 0)
	LOG_INF("[3058]scenario_id = %d\n", scenario_id);

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
	default:
	break;
	}

	return ERROR_NONE;
}
static kal_uint32 set_test_pattern_mode(kal_uint32 modes, struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
	pr_err("set_test_pattern modes: %d\n", modes);
	if (modes) {
		//if (modes == 5 && (pdata != NULL)) {
			//write_cmos_sensor(0x5101, 0x11);
            write_cmos_sensor(0x350A, 0x00);
		//}
	} else {
        //write_cmos_sensor(0x5101, 0x00);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = modes;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_int32 get_sensor_temperature(void)
{
	INT32 temperature = 0;
	INT32 temperature_convert = 0;

	/*TEMP_SEN_CTL */
	write_cmos_sensor(0x4d12, 0x01);
	temperature = (read_cmos_sensor(0x4d13) << 8) |
		read_cmos_sensor(0x4d14);
	if (temperature < 0xc000)
		temperature_convert = temperature / 256;
	else
		temperature_convert = (temperature - 0xc000 ) / 256;

	//pr_err("get sensor temperature: %d\n", temperature_convert);
  
	return temperature_convert;
}

static kal_uint32 ana_gain_table_16x[] = {
     1024,  1088,  1152,  1216,  1280,  1344,  1408,  1472,
     1536,  1600,  1664,  1728,  1792,  1856,  1920,  1984,
     2048,  2176,  2304,  2432,  2560,  2688,  2816,  2944,
     3072,  3200,  3328,  3456,  3584,  3712,  3840,  3968,
     4096,  4352,  4608,  4864,  5120,  5376,  5632,  5888,
     6144,  6400,  6656,  6912,  7168,  7424,  7680,  7936,
     8192,  8704,  9216,  9728, 10240, 10752, 11264, 11776,
    12288, 12800, 13312, 13824, 14336, 14848, 15360, 15872
};

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
	{
		write_cmos_sensor(0x0100, 0x01);
	}
	else
	{
		write_cmos_sensor(0x0100, 0x00);
	}
	mdelay(10);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
			UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

#if FPT_PDAF_SUPPORT
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
#endif
#ifdef FPT_SEAMLESS_SUPPORT
	uint32_t *pAeCtrls;
	uint32_t *pScenarios;
#endif
	if (!((feature_id == 3040) || (feature_id == 3058)))
		LOG_INF("feature_id = %d\n", feature_id);

	switch (feature_id) {
    case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(ana_gain_table_16x);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)ana_gain_table_16x,
			sizeof(ana_gain_table_16x));
		}
		break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM3:
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
		*(feature_data + 2) = imgsensor_info.exp_step;
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
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom6.pclk /
			(imgsensor_info.custom6.linelength - 80))*
			imgsensor_info.custom6.grabwindow_width;

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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
//ITD:modify x6821 its sensorfusion fail by qinlong.lu 2022/07/26 start
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 14100000;
	break;
//ITD:modify x6821 its sensorfusion fail by qinlong.lu 2022/07/26 start
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
		if(imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM3 || imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM5||imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM6)
			set_gain_fullsize((UINT16) *feature_data);
		else
			set_gain((UINT16) *feature_data);
	break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
	break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
	break;
	case SENSOR_FEATURE_SET_REGISTER:
	    write_cmos_sensor(sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
	break;
	case SENSOR_FEATURE_GET_REGISTER:
	    sensor_reg_data->RegData =
			read_cmos_sensor(sensor_reg_data->RegAddr);
	break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
	    *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
	    set_video_mode(*feature_data);
	break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
	    get_imgsensor_id(feature_return_para_32);
	break;
	case SENSOR_FEATURE_CHECK_CHIP_ID:
		ov50mtele_get_chip_id(feature_para);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
	    set_auto_flicker_mode((BOOL)*feature_data_16,
			*(feature_data_16+1));
	break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data+1));
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
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		LOG_INF("current fps :%d\n", imgsensor.current_fps);
	break;
//ITD: Modify setFPS By Thermal By Jie.liu 230517 Start
	case SENSOR_FEATURE_SET_FPS_BY_THERMAL:
	    LOG_INF("set_framerate_by_thermal :[%d] \n",(UINT32)*feature_data_32);
	    set_framerate_by_thermal(*feature_data_32);
	break;
//ITD: Modify setFPS By Thermal By Jie.liu 230517 End
	case SENSOR_FEATURE_GET_CROP_INFO:
	    LOG_INF("GET_CROP_INFO scenarioId:%d\n",
			*feature_data_32);

	    wininfo = (struct  SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[6],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[7],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[8],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[9],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[10],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		}
	break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	    LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
	    ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data+1),
				(UINT16)*(feature_data+2));
	break;
	case SENSOR_FEATURE_SET_TRANCAM_MODE:
		LOG_INF("SENSOR_FEATURE_SET_TRANCAM_MODE:ov50mtele sat mode = %d", (UINT16) *feature_data);
		imgsensor.sat_mode = (UINT16) *feature_data;
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
			case MSDK_SCENARIO_ID_CUSTOM6:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.custom6.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
	break;

#if FPT_PDAF_SUPPORT
/******************** PDAF START ********************/
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)
			(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_vr_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:
		memcpy((void *)PDAFinfo, (void *)&imgsensor_fullcrop_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
            break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		pr_info("SENSOR_FEATURE_GET_VC_INFO %d\n",(UINT16) (*feature_data));
		pvcinfo = (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));
		switch (*feature_data_32){
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
		     memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
			   sizeof(struct SENSOR_VC_INFO_STRUCT));
	    break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM2:
		     memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
			   sizeof(struct SENSOR_VC_INFO_STRUCT));
	    break;
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:
			 memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
			   sizeof(struct SENSOR_VC_INFO_STRUCT));
		break;
		default:
		    memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
			    sizeof(struct SENSOR_VC_INFO_STRUCT));
		    break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		break;
	case SENSOR_FEATURE_SET_PDAF:
			imgsensor.pdaf_mode = *feature_data_16;
		break;
/******************** PDAF END ********************/
#endif
#if FPT_SHDR_SUPPORT
//add 60A stagger HDR start
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		/*
		 * HDR_NONE = 0,
		 * HDR_RAW = 1,
		 * HDR_CAMSV = 2,
		 * HDR_RAW_ZHDR = 9,
		 * HDR_MultiCAMSV = 10,
		 * HDR_RAW_STAGGER_2EXP = 0xB,
		 * HDR_RAW_STAGGER_MIN = HDR_RAW_STAGGER_2EXP,
		 * HDR_RAW_STAGGER_3EXP = 0xC,
		 * HDR_RAW_STAGGER_MAX = HDR_RAW_STAGGER_3EXP,
		 */
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0xB;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM1:
	    case MSDK_SCENARIO_ID_CUSTOM2:
	    case MSDK_SCENARIO_ID_CUSTOM3:
	    case MSDK_SCENARIO_ID_CUSTOM4:
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		}
		LOG_INF(
			"SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu, HDR:%llu\n",
			*feature_data, *(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO:
		if (*feature_data == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_STAGGER_2EXP:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM5;
				break;
			default:
				break;
			}
		} else if (*feature_data == MSDK_SCENARIO_ID_CUSTOM5) {
			switch (*(feature_data + 1)) {
			case HDR_NONE:
				*(feature_data + 2) = MSDK_SCENARIO_ID_VIDEO_PREVIEW;
				break;
			default:
				break;
			}
		}
		LOG_INF("SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO %d %d %d\n",
							(UINT16) (*feature_data),
				(UINT16) *(feature_data + 1),
				(UINT16) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		*(feature_data + 1) = 1; //always 1
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_GET_STAGGER_MAX_EXP_TIME:
		if (*feature_data == MSDK_SCENARIO_ID_CUSTOM5) {
			switch (*(feature_data + 1)) {
			case VC_STAGGER_NE:
			case VC_STAGGER_ME:
			case VC_STAGGER_SE:
			default:
				*(feature_data + 2) = 8388607 - imgsensor_info.margin;
				break;
			}
		} else {
			*(feature_data + 2) = 0;
		}
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER: //for 2EXP
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
				(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		// implement write shutter for NE/SE
		hdr_write_tri_shutter((UINT16)*feature_data,
		             0,
					(UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_DUAL_GAIN: //for 2EXP
		LOG_INF("SENSOR_FEATURE_SET_DUAL_GAIN LE=%d, SE=%d\n",
				(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		// implement write gain for NE/SE
		hdr_write_tri_gain((UINT16)*feature_data,
		             0,
					(UINT16)*(feature_data+1));
		break;
//add 60A stagger HDR end
#endif
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		LOG_INF("SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME\n");
		set_shutter_frame_length(
			(UINT16) (*feature_data),
			(UINT16) (*(feature_data + 1)),
			(BOOL) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
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
            pScenarios = (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
        else {
            LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS input pScenarios vector is NULL!\n");
            return ERROR_INVALID_SCENARIO_ID;
        }
        switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				*pScenarios = MSDK_SCENARIO_ID_CUSTOM5;
				*(pScenarios+1) = MSDK_SCENARIO_ID_CUSTOM6;
				break;
			case MSDK_SCENARIO_ID_CUSTOM5:
				*pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
				*(pScenarios+1) = MSDK_SCENARIO_ID_CUSTOM6;
				break;
			case MSDK_SCENARIO_ID_CUSTOM6:
				*pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
				*(pScenarios+1) = MSDK_SCENARIO_ID_CUSTOM6;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
			case MSDK_SCENARIO_ID_CUSTOM4:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			default:
				*pScenarios = 0xff;
				break;
        }
        if((feature_data + 1) != NULL){
            if((MUINT32 *)((uintptr_t)(*(feature_data + 1))) != 0){
                switch (*feature_data) {
                    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                        *pScenarios = MSDK_SCENARIO_ID_CUSTOM5|TRAN_SENSOR_MODE_ISZ_PRE;
                        *(pScenarios + 1) = MSDK_SCENARIO_ID_CUSTOM6|TRAN_SENSOR_MODE_ISZ_QUAD;
			            *(pScenarios + 2) = MSDK_SCENARIO_ID_CUSTOM3|TRAN_SENSOR_MODE_FULL_SIZE;
                        *(pScenarios + 3)= MSDK_SCENARIO_ID_CAMERA_PREVIEW|TRAN_SENSOR_MODE_NONE;
                        break;
                    default:
                        *pScenarios = 0xff;
                        break;
                }
            }
        }
        LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n",
            *feature_data,
            *pScenarios);
        break;
/*  case SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH:
        extend_frame_length((MUINT32) *feature_data);
        break;
*/
#endif
/******************** SEAMLESS END ********************/
	case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
		*feature_return_para_32 = imgsensor.current_ae_effective_frame;
		LOG_INF("GET AE EFFECTIVE %d\n", *feature_return_para_32);
		break;
	case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
		memcpy(feature_return_para_32, &imgsensor.ae_frm_mode, sizeof(struct IMGSENSOR_AE_FRM_MODE));
		LOG_INF("GET_AE_FRAME_MODE");
		break;
	case SENSOR_FEATURE_GET_4CELL_DATA:/*get 4 cell data from eeprom*/
	{
		int type = (kal_uint16)(*feature_data);
		char *data = (char *)(uintptr_t)(*(feature_data+1));

		if ( type == FOUR_CELL_CAL_TYPE_GAIN_TBL ) {
			LOG_INF("Read Cross Talk Start");

			if (CROSSTALK_DATA[2] == 0 && CROSSTALK_DATA[3] == 0 && CROSSTALK_DATA[4] == 0) {
				printk("read 4cell data again \n");
				ov50mtele_read_crosstalk_data(&CROSSTALK_DATA[2]);
			}

			CROSSTALK_DATA[0] = (FOUR_CELL_SIZE & 0xff);/*Low*/
			CROSSTALK_DATA[1] = ((FOUR_CELL_SIZE >> 8) & 0xff);/*High*/
			if (data != NULL) {
				printk("return data\n");
				memcpy(data, CROSSTALK_DATA, sizeof(CROSSTALK_DATA) / sizeof(CROSSTALK_DATA[0]));
	                }
			LOG_INF("Read Cross Talk = %02x %02x %02x %02x %02x %02x\n",
				(UINT16)data[0], (UINT16)data[1],
				(UINT16)data[2], (UINT16)data[3],
				(UINT16)data[4], (UINT16)data[5]);
		}
		break;
	}
	default:
	break;
	}

	return ERROR_NONE;
}   /*  feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 OV50MTELE_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
	*pfFunc =  &sensor_func;
	return ERROR_NONE;
}
