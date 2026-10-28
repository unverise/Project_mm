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
#include "gc32e1x6855mipi_Sensor.h"
/****************************Modify Following Strings for Debug**************************/
#define PFX "gc32e1x6855_otp"
/****************************   Modify end    *******************************************/
#define GC32E1X6855_OTP_DEBUG 0
#if GC32E1X6855_OTP_DEBUG
#define LOG_INF(format, args...)	pr_info(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif
#define EEPROM_WRITE_ID           0xA2
#define OTP_FLAG_EMPTY            0x00
#define OTP_FLAG_VALID            0x01
static kal_uint16 read_eeprom(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xff) };
	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, EEPROM_WRITE_ID);
	return get_byte;
}
bool gc32e1x6855_read_crosstalk_data(kal_uint16 addr, BYTE *data, int size)
{
	kal_uint16 i = 0;
	kal_uint16 ct_flag = 0;
	kal_uint32 checksum = 0;
    LOG_INF("start hyper crosstalk");
	ct_flag = read_eeprom(addr - 1);
	LOG_INF("ct_flag = %d\n", ct_flag);
	if (ct_flag == OTP_FLAG_VALID) {
		for (i = 0; i < size; i++) {
			data[i] = read_eeprom(addr + i);
			checksum = checksum + data[i];
			LOG_INF("ct_data[%d] = %x\n", i, data[i]);
		}
		if ((checksum % 256) == read_eeprom(addr + size))
			pr_info("check success! calc_checksum = %d, read_checksum = %d\n", (checksum % 256), read_eeprom(addr + size));
		else {
			memset(data, 0, size);
			pr_err("check error! calc_checksum = %d, read_checksum = %d\n", (checksum % 256), read_eeprom(addr + size));
			return false;
		}
	} else {
		pr_err("crosstalk data is zero!\n");
		return false;
	}
	return true;
}