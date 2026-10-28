/*
 * Copyright (C) 2016 MediaTek Inc.
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
#include "ov50mtelemipiraw_Sensor.h"

#define PFX "OV50MTELE_CROSSTALK"
#define LOG_INF(format, args...)    pr_info(PFX "[%s] " format, __func__, ##args)

#define EEPROM_READ_ID               0xA0
#define EEPROM_WRITE_ID              0xA1
#define CROSSTALK_FLAG_ADDR          0x154D
#define CROSSTALK_DATA_ADDR          0x154E
#define CROSSTALK_CHECKSUM_ADDR      0x19CE
#define CROSSTALK_DATA_LENGTH        (CROSSTALK_CHECKSUM_ADDR - CROSSTALK_DATA_ADDR)
#define MAX_READ_WRITE_SIZE          255

static bool _read_ov50mtele_eeprom(kal_uint16 addr, u8 *data, int size)
{
    int i = 0;
    int offset = addr;
    int ret;
    u8 pu_send_cmd[2];

    for (i = 0; i < size; i += MAX_READ_WRITE_SIZE) {
        pu_send_cmd[0] = (u8) (offset >> 8);
        pu_send_cmd[1] = (u8) (offset & 0xFF);

        if (i + MAX_READ_WRITE_SIZE > size) {
            ret = iReadRegI2C(pu_send_cmd, 2, (u8 *) (data + i), (size - i), EEPROM_READ_ID);

        } else {
            ret = iReadRegI2C(pu_send_cmd, 2, (u8 *) (data + i), MAX_READ_WRITE_SIZE, EEPROM_READ_ID);
        }

        if (ret < 0) {
            LOG_INF("read eeprom failed!\n");
            return false;
        }

        offset += MAX_READ_WRITE_SIZE;
    }

    LOG_INF("exit _read_eeprom size = %d, addr = 0x%x\n", size, addr);
    return true;
}

void ov50mtele_read_crosstalk_data(char *data)
{
    int i;
    int flag = 0;
    int temp = 0;
    int checksum = 0;

    _read_ov50mtele_eeprom(CROSSTALK_FLAG_ADDR, (u8 *)&flag, 1);
    if(flag == 1)
        _read_ov50mtele_eeprom(CROSSTALK_DATA_ADDR, (u8 *)data, CROSSTALK_DATA_LENGTH);

    for(i = 0; i < CROSSTALK_DATA_LENGTH; i++)
    {
        temp = temp + data[i];
        LOG_INF("data[0x%x] = %d", CROSSTALK_DATA_ADDR+i, data[i]);
    }

    _read_ov50mtele_eeprom(CROSSTALK_CHECKSUM_ADDR, (u8 *)&checksum, 1);

    LOG_INF("ov50mtele flag = %d, checksum = %d", flag, checksum);

    if(temp%256 == checksum)
        LOG_INF("ov50mtele crosstalk checksum succeed!");
    else
        LOG_INF("ov50mtele crosstalk checksum err, checksum = %d, temp = %d", checksum, temp);
}
