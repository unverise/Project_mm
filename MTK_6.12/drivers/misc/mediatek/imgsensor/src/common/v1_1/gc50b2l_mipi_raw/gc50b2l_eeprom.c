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
#include "gc50b2lmipi_Sensor.h"
/****************************Modify Following Strings for Debug**************************/
#define PFX "gc50b2l_otp"
/****************************   Modify end    *******************************************/
#define GC50B2L_OTP_DEBUG 0
#if GC50B2L_OTP_DEBUG
#define LOG_INF(format, args...)	pr_info(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif
#define EEPROM_WRITE_ID           0xA0
#define XTC_BIN_FLAG_ADDR         0x0D58
#define XTC_BIN_CHECKSUM_ADDR     0x1A60
#define XTC_FULL_FLAG_ADDR        0x0D59
#define XTC_FULL_CHECKSUM_ADDR    0x1A61
static kal_uint16 read_eeprom(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xff) };
	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, EEPROM_WRITE_ID);
	return get_byte;
}
bool gc50b2l_read_bin_xtc_data(kal_uint16 addr, BYTE *data, int size)
{
	kal_uint16 i = 0;
	kal_uint16 ct_flag = 0;
	kal_uint32 checksum = 0;
	LOG_INF("start hyper xtc");
	ct_flag = read_eeprom(XTC_BIN_FLAG_ADDR);
	LOG_INF("ct_flag = %d\n", ct_flag);
	if (ct_flag == 0x01) {
		for (i = 0; i < size; i++) {
			data[i] = read_eeprom(addr + i);
			checksum = checksum + data[i];
			LOG_INF("ct_data[%d] = %x\n", i, data[i]);
		}
		if ((checksum % 256) == read_eeprom(XTC_BIN_CHECKSUM_ADDR))
			pr_info("%s check success! calc_checksum = %d, read_checksum = %d\n", __func__, (checksum % 256), read_eeprom(XTC_BIN_CHECKSUM_ADDR));
		else {
			memset(data, 0, size);
			pr_err("%s check error! calc_checksum = %d, read_checksum = %d\n", __func__, (checksum % 256), read_eeprom(XTC_BIN_CHECKSUM_ADDR));
			return false;
		}
	} else {
		pr_err("%s xtc data flag is invalid! flag = %d\n", __func__, ct_flag);
		return false;
	}
	return true;
}
bool gc50b2l_read_full_xtc_data(kal_uint16 addr, BYTE *data, int size)
{
	kal_uint16 i = 0;
	kal_uint16 ct_flag = 0;
	kal_uint32 checksum = 0;
	LOG_INF("start hyper xtc");
	ct_flag = read_eeprom(XTC_FULL_FLAG_ADDR);
	LOG_INF("ct_flag = %d\n", ct_flag);
	if (ct_flag == 0x01) {
		for (i = 0; i < size; i++) {
			data[i] = read_eeprom(addr + i);
			checksum = checksum + data[i];
			LOG_INF("ct_data[%d] = %x\n", i, data[i]);
		}
		if ((checksum % 256) == read_eeprom(XTC_FULL_CHECKSUM_ADDR))
			pr_info("%s check success! calc_checksum = %d, read_checksum = %d\n", __func__, (checksum % 256), read_eeprom(XTC_FULL_CHECKSUM_ADDR));
		else {
			memset(data, 0, size);
			pr_err("%s check error! calc_checksum = %d, read_checksum = %d\n", __func__, (checksum % 256), read_eeprom(XTC_FULL_CHECKSUM_ADDR));
			return false;
		}
	} else {
		pr_err("%s xtc data flag is invalid! flag = %d\n", __func__, ct_flag);
		return false;
	}
	return true;
}