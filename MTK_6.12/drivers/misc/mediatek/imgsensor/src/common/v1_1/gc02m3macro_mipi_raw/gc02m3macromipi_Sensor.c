 /*
 *
 * Filename:
 * ---------
 *     gc02m3macromipi_Sensor.c
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
 */

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

#include "gc02m3macromipi_Sensor.h"

/************************** Modify Following Strings for Debug **************************/
#define PFX "gc02m3macro_camera_sensor"
#define LOG_1 LOG_INF("GC02M3MACRO, MIPI 1LANE\n")
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)

#define MULTI_WRITE    1


static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = GC02M3MACRO_SENSOR_ID,
#if GC02M3MACRO_MIRROR_NORMAL
	.checksum_value = 0x1246a0f3,
#elif GC02M3MACRO_MIRROR_HV
	.checksum_value = 0xdd7e91b8,
#else
	.checksum_value = 0xdb6e1e1f,
#endif
	.pre = {
		.pclk = 109000000,
		.linelength = 2788,
		.framelength = 1302,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 65400000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 109000000,
		.linelength = 2788,
		.framelength = 1302,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 65400000,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 109000000,
		.linelength = 2788,
		.framelength = 1302,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 65400000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 109000000,
		.linelength = 2788,
		.framelength = 1302,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 65400000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 109000000,
		.linelength = 2788,
		.framelength = 1302,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 65400000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 109000000,
		.linelength = 2788,
		.framelength = 1302,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 65400000,
		.max_framerate = 300,
	},

	.margin = 16,
	.min_shutter = 4,
	.min_gain = 64,
	.max_gain = 1024,
	.min_gain_iso = 100,
	.gain_step = 1,
	.gain_type = 4,
	.max_frame_length = 0x3fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 5,

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_1_LANE,
	.i2c_addr_table = {0x20, 0xff},
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = GC02M3MACRO_MIRROR,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3ED,
	.gain = 0x40,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x20,

};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200}, /* Preview */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200}, /* capture */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200}, /* video */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200}, /* HS video */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200}  /* slim video */
};

#if MULTI_WRITE
#define I2C_BUFFER_LEN    510
#else
#define I2C_BUFFER_LEN    2
#endif
/*
static kal_uint16 gc02m3macro_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;
	while (len > IDX) {
		addr = para[IDX];

		puSendCmd[tosend++] = (char)(addr & 0xff);
		data = para[IDX + 1];
		puSendCmd[tosend++] = (char)(data & 0xff);
		IDX += 2;
		addr_last = addr;
		
#if MULTI_WRITE
		if ((I2C_BUFFER_LEN - tosend) < 2 || IDX == len || addr != addr_last) {
			LOG_INF("burst write\n");
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id, 2,
						 imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2CTiming(puSendCmd, 2, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
		tosend = 0;
#endif
	}
	return 0;
}
*/
static void gc02m3macro_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
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

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[4] = {
        (char)((addr >> 8) & 0xff),
        (char)(addr & 0xff),
        (char)((para >> 8) & 0xff),
        (char)(para & 0xff)
    };

    iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id);
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
static void set_dummy(void)
{
	LOG_INF("frame length = %d\n", imgsensor.frame_length);
    write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);
}

static kal_uint32 return_sensor_id(void)
{
    kal_uint32 sensor_id = 0;

    sensor_id = (read_cmos_sensor(0x03f0) << 8) | read_cmos_sensor(0x03f1);
    return sensor_id;
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	
	kal_uint32 frame_length = imgsensor.frame_length;
	

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;
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

static void set_shutter(kal_uint16 shutter)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    /* kal_uint32 frame_length = 0; */
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
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
	
	if (imgsensor.autoflicker_en) {
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else{
			write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);
		}
	} else{
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);
	}
	write_cmos_sensor(0x0202, shutter);
	LOG_INF("shutter = %d, framelength = %d\n", shutter, imgsensor.frame_length);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = gain << 4;

	if (reg_gain < GC02M3MACRO_SENSOR_GAIN_BASE)
		reg_gain = GC02M3MACRO_SENSOR_GAIN_BASE;
	else if (reg_gain > GC02M3MACRO_SENSOR_GAIN_MAX)
		reg_gain = GC02M3MACRO_SENSOR_GAIN_MAX;

	return (kal_uint16)reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;
	reg_gain = gain2reg(gain);
    LOG_INF("gain = %d, reg_gain = %d\n", gain, reg_gain);
	write_cmos_sensor(0x0204, reg_gain & 0xffff);

	return reg_gain;
}

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le: 0x%x, se: 0x%x, gain: 0x%x\n", le, se, gain);
}

/*static void set_mirror_flip(kal_uint8 image_mirror)
*{
*	LOG_INF("image_mirror = %d\n", image_mirror);
*}
*/

static void night_mode(kal_bool enable)
{
	/* No Need to implement this function */
}

kal_uint16 addr_data_pair_init_gc02m3macro[] = {
	
    0x031c,0x84,
    0x0801,0x0c,
    0x0e93,0xc4,
    0x03e0,0x0a,
    0x0822,0x04,
    0x0823,0x6d,
    0x0824,0x04,
    0x0315,0x33,
    0x0822,0x44,

    0x031c,0x00,
    0x0801,0x00,
    0x031c,0xc4,
    0x0801,0x0c,

    0x0084,0x21,
    0x0e94,0x3a,
    0x0320,0x60,
    0x0804,0x18,
    0x0e34,0x03,

    0x0e85,0x00,
    0x0e86,0x01,
    0x0e87,0x03,
    0x0e88,0x27,
    0x0346,0x00,
    0x0347,0x12,
    0x034a,0x04,
    0x034b,0xbc,

    0x0202,0x04,
    0x0203,0x96,
    0x0215,0x15,
    0x0216,0x16,
    0x0340,0x05,
    0x0341,0x16,
    0x0342,0x05 ,
    0x0343,0x72,
    0x0e90,0x0c,
    0x0e35,0x57,
    0x0e36,0x5b,
    0x0e90,0x0c,
    0x0e91,0x54,
    0x0e74,0x06,
    0x0e1d,0x01,
    0x0e1c,0x69,
    0x0e19,0x3f,
    0x0e62,0x01,
    0x0e61,0x6d,
    0x0e55,0x3f,
    0x0e77,0x50,
    0x0e46,0x40,
    0x0e47,0x1e,
    0x0e59,0x2a,
    0x0e82,0x20,
    0x0e76,0x06,
    0x0e4e,0x01,
    0x0e4f,0x02,
    0x0e50,0x01,
    0x0e5a,0x01,
    0x0e5b,0x02,
    0x0e5c,0x01,

    0x0365,0x08,
    0x036b,0x01,
    0x0360,0x95,
    0x0361,0x00,
    0x0362,0x90,
    0x0361,0x20,

    0x0323,0x00,

    0x0b80,0xc4,
    0x0b81,0x93,
    0x0b82,0x1e,
    0x0b83,0x00,

    0x0b84,0x00,
    0x0b85,0xfe,
    0x0b86,0x13,
    0x0b87,0x00,

    0x0b88,0x09,
    0x0b89,0x02,
    0x0b8a,0x18,
    0x0b8b,0x00,

    0x0b8c,0x44,
    0x0b8d,0x22,
    0x0b8e,0x18,
    0x0b8f,0x00,

    0x0b90,0x3b,
    0x0b91,0x01,
    0x0b92,0x18,
    0x0b93,0x00,

    0x0b94,0x31,
    0x0b95,0x10,
    0x0b96,0x18,
    0x0b97,0x00,

    0x0b98,0x3a,
    0x0b99,0x30,
    0x0b9a,0x1e,
    0x0b9b,0x00,

    0x0b9c,0xc0,
    0x0b9d,0x1c,
    0x0b9e,0x13,
    0x0b9f,0x00,

    0x0ba0,0x08,
    0x0ba1,0x01,
    0x0ba2,0x18,
    0x0ba3,0x00,

    0x0ba4,0x10,
    0x0ba5,0xfe,
    0x0ba6,0x13,
    0x0ba7,0x00,

    0x0ba8,0x00,
    0x0ba9,0xfe,
    0x0baa,0x13,
    0x0bab,0x00,

    0x0bac,0x10,
    0x0bad,0x0e,
    0x0bae,0x18,
    0x0baf,0x00,

    0x0bb0,0x00,
    0x0bb1,0x0e,
    0x0bb2,0x18,
    0x0bb3,0x00,

    0x0bb4,0xc8,
    0x0bb5,0x1c,
    0x0bb6,0x13,
    0x0bb7,0x00,

    0x0bb8,0x1b,
    0x0bb9,0x01,
    0x0bba,0x18,
    0x0bbb,0x00,

    0x0bbc,0x00,
    0x0bbd,0xfe,
    0x0bbe,0x13,
    0x0bbf,0x00,

    0x0bc0,0x00,
    0x0bc1,0xfe,
    0x0bc2,0x13,
    0x0bc3,0x00,

    0x0bc4,0xc0,
    0x0bc5,0x1c,
    0x0bc6,0x13,
    0x0bc7,0x00,

    0x0bc8,0x08,
    0x0bc9,0x01,
    0x0bca,0x18,
    0x0bcb,0x00,

    0x0bcc,0x10,
    0x0bcd,0xfe,
    0x0bce,0x13,
    0x0bcf,0x00,

    0x0bd0,0x00,
    0x0bd1,0xfe,
    0x0bd2,0x13,
    0x0bd3,0x00,

    0x0bd4,0x10,
    0x0bd5,0x0e,
    0x0bd6,0x18,
    0x0bd7,0x00,

    0x0bd8,0x00,
    0x0bd9,0x0e,
    0x0bda,0x18,
    0x0bdb,0x00,

    0x0bdc,0xc8,
    0x0bdd,0x1c,
    0x0bde,0x13,
    0x0bdf,0x00,

    0x0be0,0x3b,
    0x0be1,0x01,
    0x0be2,0x18,
    0x0be3,0x00,

    0x0be4,0x00,
    0x0be5,0x02,
    0x0be6,0x18,
    0x0be7,0x00,

    0x0be8,0x01,
    0x0be9,0x10,
    0x0bea,0x18,
    0x0beb,0x00,

    0x0bec,0x02,
    0x0bed,0x30,
    0x0bee,0x1e,
    0x0bef,0x00,

    0x0bf0,0x1a,
    0x0bf1,0x01,
    0x0bf2,0x18,
    0x0bf3,0x00,

    0x0bf4,0x04,
    0x0bf5,0x22,
    0x0bf6,0x18,
    0x0bf7,0x00,

    0x0bf8,0x44,
    0x0bf9,0x93,
    0x0bfa,0x1e,
    0x0bfb,0x00,

    0x0339,0x19,
    0x033a,0x1f,
    0x0338,0x0f,

    0x00c0,0x25,
    0x00c3,0x70,
    0x00c5,0x00,

    0x0323,0x01,

    0x0b00,0x40,
    0x0b01,0x90,
    0x0b02,0x64,
    0x0b03,0x00,

    0x0b04,0x00,
    0x0b05,0x00,
    0x0b06,0x41,
    0x0b07,0x00,

    0x0b08,0x5b,
    0x0b09,0x30,
    0x0b0a,0x64,
    0x0b0b,0x00,

    0x0b0c,0x00,
    0x0b0d,0x82,
    0x0b0e,0x41,
    0x0b0f,0x00,

    0x0b10,0x7e,
    0x0b11,0x80,
    0x0b12,0x65,
    0x0b13,0x00,

    0x0b14,0x00,
    0x0b15,0x24,
    0x0b16,0x42,
    0x0b17,0x00,

    0x0b18,0xb2,
    0x0b19,0xe0,
    0x0b1a,0x62,
    0x0b1b,0x00,

    0x0b1c,0x00,
    0x0b1d,0x46,
    0x0b1e,0x42,
    0x0b1f,0x00,

    0x0b20,0xf5,
    0x0b21,0xe0,
    0x0b22,0x32,
    0x0b23,0x00,

    0x0b24,0x00,
    0x0b25,0x48,
    0x0b26,0x42,
    0x0b27,0x00,

    0x0b28,0x55,
    0x0b29,0x01,
    0x0b2a,0x40,
    0x0b2b,0x00,

    0x0b2c,0x00,
    0x0b2d,0xaa,
    0x0b2e,0x42,
    0x0b2f,0x00,

    0x0b30,0xd9,
    0x0b31,0x51,
    0x0b32,0x50,
    0x0b33,0x00,

    0x0b34,0x92,
    0x0b35,0x5a,
    0x0b36,0x52,
    0x0b37,0x00,

    0x0b38,0x56,
    0x0b39,0x62,
    0x0b3a,0x40,
    0x0b3b,0x00,

    0x0b3c,0xde,
    0x0b3d,0xda,
    0x0b3e,0x52,
    0x0b3f,0x00,

    0x0b40,0x9f,
    0x0b41,0x02,
    0x0b42,0x40,
    0x0b43,0x00,

    0x0b44,0xfd,
    0x0b45,0xda,
    0x0b46,0x52,
    0x0b47,0x00,

    0x0b48,0xbc,
    0x0b49,0x03,
    0x0b4a,0x40,
    0x0b4b,0x00,

    0x0b4c,0x49,
    0x0b4d,0x0b,
    0x0b4e,0x53,
    0x0b4f,0x00,

    0x0206,0x1a,

    0x0365,0x00,
    0x036b,0x00,
    0x0360,0x00,
    0x0361,0x00,

    0x0089,0x03,
    0x0080,0x00,
    0x0e84,0x02,
    0x0218,0x00,
    0x0210,0x02,
    0x0101,GC02M3MACRO_MIRROR,
    0x0058,0x65,
    0x02e7,0x10,
    0x0090,0x02,
    0x0091,0x05,

    0x0040,0x12,
    0x0038,0xf0,
    0x0039,0x0f,
    0x0020,0x07,
    0x0092,0x40,

    0x0041,0x20,
    0x003a,0x0f,
    0x003b,0x00,
    0x0023,0x03,
    0x0043,0x08,

    0x0204,0x04,
    0x0205,0x00,
    0x0208,0x68,

    0x0350,0x81,
    0x0351,0x00,
    0x0352,0x06,
    0x0353,0x00,
    0x0354,0x08,
    0x034c,0x06,
    0x034d,0x40,
    0x034e,0x04,
    0x034f,0xb0,

    0x0806,0x07,
    0x0807,0xd0,
    0x0810,0x31,
    0x0e30,0x3a,
    0x0e31,0xd4,
    0x0e33,0x35,
    0x0803,0x20,
    0x0811,0x00,
    0x0814,0x04,
    0x0816,0x05,
    0x0817,0x16,
    0x0818,0x02,
    0x0819,0x0d,
    0x081a,0x07,
    0x081b,0x05,
    0x081c,0x08,
    0x081d,0x08,
    0x0084,0x01,

    0x031c,0xc0,
    0x0801,0x08,
    0x03fe,0x10,
    0x03fe,0x00,
    0x080e,0x10,
    0x080e,0x00,
    0x031c,0xc8,
    0x0801,0x1b,
    0x03fe,0x00,
    0x03fe,0x00,
    0x03fe,0x00,
    0x03fe,0x00,
    0x031c,0xc0,
    0x0801,0x08,
    0x03fe,0x10,
    0x03fe,0x00,
    0x080e,0x10,
    0x080e,0x00,
    0x031c,0xc8,
    0x0801,0x3b,

    0x0244,0x1f,
    0x0245,0x29,
    0x0246,0x1b,
    0x0247,0x05,
    0x0317,0x01,

    0x0100,0x00,
};

kal_uint16 addr_data_pair_preview_gc02m3macro[] = {
	//0x03fe, 0x00,
	0x0100,0x01,
};

kal_uint16 addr_data_pair_capture_gc02m3macro[] = {
	//0x03fe, 0x00,
	0x0100,0x01,
};

kal_uint16 addr_data_pair_normal_video_gc02m3macro[] = {
	//0x03fe, 0x00,
	0x0100,0x01,
};

kal_uint16 addr_data_pair_hs_video_gc02m3macro[] = {
	//0x03fe, 0x00,
	0x0100,0x01,
};

kal_uint16 addr_data_pair_slim_video_gc02m3macro[] = {
	//0x03fe, 0x00,
	0x0100,0x01,
};

kal_uint16 addr_data_streamon_gc02m3macro[] = {
	0x0100,0x01,
};

kal_uint16 addr_data_streamoff_gc02m3macro[] = {
	0x0100,0x00,
};

static void sensor_init(void)
{
	LOG_INF("E\n");
    pr_info("PJH debugsensor init start!\n");
	gc02m3macro_table_write_cmos_sensor(
		addr_data_pair_init_gc02m3macro,
		sizeof(addr_data_pair_init_gc02m3macro) /
		sizeof(kal_uint16));
    pr_info("PJH debug 0x0100 : 0x%x\n",read_cmos_sensor(0x0100));
    pr_info("PJH debug 0x0317 : 0x%x\n",read_cmos_sensor(0x0317));
}

static void preview_setting(void)
{
	LOG_INF("E\n");
	gc02m3macro_table_write_cmos_sensor(
		addr_data_pair_preview_gc02m3macro,
		sizeof(addr_data_pair_preview_gc02m3macro) /
		sizeof(kal_uint16));
}

static void capture_setting(void)
{
	LOG_INF("E\n");
	gc02m3macro_table_write_cmos_sensor(
		addr_data_pair_capture_gc02m3macro,
		sizeof(addr_data_pair_capture_gc02m3macro) /
		sizeof(kal_uint16));
}

static void normal_video_setting(void)
{
	LOG_INF("E\n");
	gc02m3macro_table_write_cmos_sensor(
		addr_data_pair_normal_video_gc02m3macro,
		sizeof(addr_data_pair_normal_video_gc02m3macro) /
		sizeof(kal_uint16));
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	gc02m3macro_table_write_cmos_sensor(
		addr_data_pair_hs_video_gc02m3macro,
		sizeof(addr_data_pair_hs_video_gc02m3macro) /
		sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	gc02m3macro_table_write_cmos_sensor(
		addr_data_pair_slim_video_gc02m3macro,
		sizeof(addr_data_pair_slim_video_gc02m3macro) /
		sizeof(kal_uint16));
}

static kal_uint32 set_test_pattern_mode(kal_uint32 modes, struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
    LOG_INF("enable: %d\n", modes);

    if (modes == 2)
    {
        write_cmos_sensor_8bit(0x008c, 0x04);
    }
    else if (modes == 5 && (pdata != NULL)) {
            write_cmos_sensor_8bit(0x008c, 0x04);
            write_cmos_sensor_8bit(0x008d, 0x00);
    }
    else
        write_cmos_sensor_8bit(0x008c, 0x00);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = modes;
    LOG_INF("final set_test_pattern modes: %d\n", modes);
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
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
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
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

	LOG_1;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
	    retry = 2;
	}

	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in */
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
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
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
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting();
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
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
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
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
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
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	return ERROR_NONE;
}

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
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

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
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}
	return ERROR_NONE;
}

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
		if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
			frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ?
				(frame_length - imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
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
		}
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
	default:
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		gc02m3macro_table_write_cmos_sensor(
			addr_data_streamon_gc02m3macro,
			sizeof(addr_data_streamon_gc02m3macro) /
			sizeof(kal_uint16));
	} else {
		gc02m3macro_table_write_cmos_sensor(
			addr_data_streamoff_gc02m3macro,
			sizeof(addr_data_streamoff_gc02m3macro) /
			sizeof(kal_uint16));
	}
	mdelay(5);
	return ERROR_NONE;
}
static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		*(feature_data + 1) = 1;
		*(feature_data + 2) = imgsensor_info.margin;
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
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.pre.pclk;
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
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
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
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(feature_data + 2) = 2;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
		default:
			*(feature_data + 2) = 1;
			break;
        }
        break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		default:
			*feature_return_para_32 = 1;//1:full size 2:binning_averaged
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
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
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
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)*feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE = %d, SE = %d, Gain = %d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
		ihdr_write_shutter_gain((UINT16)*feature_data, (UINT16)*(feature_data + 1),
			(UINT16)*(feature_data + 2));
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;
		case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
			LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
			streaming_control(KAL_FALSE);
			break;
		case SENSOR_FEATURE_SET_STREAMING_RESUME:
			LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, Dream01\n");
			streaming_control(KAL_TRUE);
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

UINT32 GC02M3MACRO_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
