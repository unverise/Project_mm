#define PFX "hi3231qx6855_otp"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include "kd_camera_typedef.h"

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "hi3231qx6855mipiraw_Sensor.h"

#define HI3231QX6855_OTP_DEBUG 0
#if HI3231QX6855_OTP_DEBUG
#define LOG_INF(format, args...)    pr_info(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

#define HI3231QX6855_EEPROM_READ_ID    0xA2
#define HI3231QX6855_I2C_SPEED         100

static kal_uint16 read_eeprom(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xff) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, HI3231QX6855_EEPROM_READ_ID);
	return get_byte;
}

bool _read_hi3231qx6855_eeprom(kal_uint16 addr, BYTE *data, int size)
{
	int i = 0;
	int checksum = 0;
	int offset = addr;
	int ret;
	u8 pu_send_cmd[2];

	if(read_eeprom(addr - 1) == 1) {
	#define MAX_READ_WRITE_SIZE 255
		for (i = 0; i < size; i += MAX_READ_WRITE_SIZE) {
			pu_send_cmd[0] = (u8) (offset >> 8);
			pu_send_cmd[1] = (u8) (offset & 0xFF);

			if (i + MAX_READ_WRITE_SIZE > size) {
				ret = iReadRegI2C(pu_send_cmd, 2,
						 (u8 *) (data + i),
						 (size - i),
						 HI3231QX6855_EEPROM_READ_ID);

			} else {
				ret = iReadRegI2C(pu_send_cmd, 2,
						 (u8 *) (data + i),
						 MAX_READ_WRITE_SIZE,
						 HI3231QX6855_EEPROM_READ_ID);
			}
			if (ret < 0) {
				pr_err("read lrc failed!\n");
				return false;
			}

			offset += MAX_READ_WRITE_SIZE;
		}
	} else {
		pr_err("hi3231qx6855 xtc flag invalid!");
		return false;
	}

	for(i = 0; i < size; i++) {
		checksum = checksum + data[i];
		LOG_INF("data[0x%x] = %d", addr + i, data[i]);
	}

	if(checksum % 256 == read_eeprom(addr + size)) {
		pr_info("checksum succeed! checksum = %d, read_checksum = %d", checksum % 256, read_eeprom(addr + size));
	} else {
		pr_err("checksum fail! checksum = %d, read_checksum = %d", checksum % 256, read_eeprom(addr + size));
		return false;
	}

	LOG_INF("exit _read_eeprom size = %d\n", size);
	return true;
}