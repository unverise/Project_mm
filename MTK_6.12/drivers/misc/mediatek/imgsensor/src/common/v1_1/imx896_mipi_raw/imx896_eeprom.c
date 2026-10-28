// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "IMX896_pdafotp"
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

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
#include "imx896mipiraw_Sensor.h"

#define Sleep(ms) mdelay(ms)

#define IMX896_EEPROM_READ_ID  0xA0
#define IMX896_EEPROM_WRITE_ID 0xA1
#define IMX896_I2C_SPEED       100
#define IMX896_MAX_OFFSET      0xFFFF

#define MTK_IDENTITY_VALUE 0x010B00FF
#define LRC_SIZE 140
#define DCC_SIZE 96

struct EEPROM_PDAF_INFO {
	kal_uint16 LRC_addr;
	unsigned int LRC_size;
	kal_uint16 DCC_addr;
	unsigned int DCC_size;
};

enum EEPROM_PDAF_INFO_FMT {
	MTK_FMT = 0,
	OP_FMT,
	FMT_MAX
};

static struct EEPROM_PDAF_INFO eeprom_pdaf_info[] = {
	{/* MTK_FMT */
		.LRC_addr = 0x14FE,
		.LRC_size = LRC_SIZE,
		.DCC_addr = 0x763,
		.DCC_size = DCC_SIZE
	},
	{/* OP_FMT */
		.LRC_addr = 0x700,
		.LRC_size = LRC_SIZE,
		.DCC_addr = 0xB06,
		.DCC_size = DCC_SIZE
	},
};

static DEFINE_MUTEX(gimx896_eeprom_mutex);

static bool selective_read_eeprom(kal_uint16 addr, BYTE *data)
{
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	if (addr > IMX896_MAX_OFFSET)
		return false;

	if (iReadRegI2C(pu_send_cmd, 2, (u8 *) data,
			1, IMX896_EEPROM_READ_ID) < 0) {
		return false;
	}
	return true;
}

static bool read_imx896_eeprom(kal_uint16 addr, BYTE *data, int size)
{
	int i = 0;
	int offset = addr;

	/*LOG_INF("enter read_eeprom size = %d\n", size);*/
	for (i = 0; i < size; i++) {
		if (!selective_read_eeprom(offset, &data[i]))
			return false;
		/*LOG_INF("read_eeprom 0x%0x %d\n", offset, data[i]);*/
		offset++;
	}
	return true;
}

static struct EEPROM_PDAF_INFO *get_eeprom_pdaf_info(void)
{
	static struct EEPROM_PDAF_INFO *pinfo;
	BYTE read_data[4];

	mutex_lock(&gimx896_eeprom_mutex);
	if (pinfo == NULL) {
		read_imx896_eeprom(0x1, read_data, 4);
		if (((read_data[3] << 24) |
		     (read_data[2] << 16) |
		     (read_data[1] << 8) |
		     read_data[0]) == MTK_IDENTITY_VALUE) {
			pinfo = &eeprom_pdaf_info[MTK_FMT];
		} else {
			pinfo = &eeprom_pdaf_info[OP_FMT];
		}
	}
	mutex_unlock(&gimx896_eeprom_mutex);

	return pinfo;
}

unsigned int read_imx896_LRC(BYTE *data)
{
	static BYTE IMX896_LRC_data[LRC_SIZE] = { 0 };
	static unsigned int readed_size;
	struct EEPROM_PDAF_INFO *pinfo = get_eeprom_pdaf_info();

	LOG_INF("read imx896 LRC, addr = %d, size = %u\n",
		pinfo->LRC_addr, pinfo->LRC_size);

	mutex_lock(&gimx896_eeprom_mutex);
	if ((readed_size == 0) &&
	    read_imx896_eeprom(pinfo->LRC_addr,
			       IMX896_LRC_data, pinfo->LRC_size)) {
		readed_size = pinfo->LRC_size;
	}
	mutex_unlock(&gimx896_eeprom_mutex);

	memcpy(data, IMX896_LRC_data, pinfo->LRC_size);
	return readed_size;
}


unsigned int read_imx896_DCC(BYTE *data)
{
	static BYTE IMX896_DCC_data[DCC_SIZE] = { 0 };
	static unsigned int readed_size;
	struct EEPROM_PDAF_INFO *pinfo = get_eeprom_pdaf_info();

	LOG_INF("read imx896 DCC, addr = %d, size = %u\n",
		pinfo->DCC_addr, pinfo->DCC_size);

	mutex_lock(&gimx896_eeprom_mutex);
	if ((readed_size == 0) &&
	    read_imx896_eeprom(pinfo->DCC_addr,
			       IMX896_DCC_data, pinfo->DCC_size)) {
		readed_size = pinfo->DCC_size;
	}
	mutex_unlock(&gimx896_eeprom_mutex);

	memcpy(data, IMX896_DCC_data, pinfo->DCC_size);
	return readed_size;
}
