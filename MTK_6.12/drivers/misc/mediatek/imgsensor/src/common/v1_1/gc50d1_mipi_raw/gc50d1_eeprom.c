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
#include "gc50d1mipi_Sensor.h"

/****************************Modify Following Strings for Debug**************************/
#define PFX "gc50d1_otp"
#define CT_DEBUG            0
/****************************   Modify end    *******************************************/
#define GC50D1_OTP_DEBUG 0
#if GC50D1_OTP_DEBUG
#define LOG_INF(format, args...)	pr_info(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

#define EEPROM_WRITE_ID           0xA0

#define OTP_FLAG_EMPTY            0x00
#define OTP_FLAG_VALID            0x01


// static kal_uint16 read_eeprom(kal_uint16 addr)
// {
// 	kal_uint16 get_byte = 0;
// 	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xff) };

// 	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, EEPROM_WRITE_ID);
// 	return get_byte;
// }

