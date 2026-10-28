/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_AW2023_LED_H__
#define __LINUX_AW2023_LED_H__

/* The definition of each time described as shown in figure.
 *        /-----------\
 *       /      |      \
 *      /|      |      |\
 *     / |      |      | \-----------
 *       |hold_time_ms |      |
 *       |             |      |
 * rise_time_ms  fall_time_ms |
 *                       off_time_ms
 */

#include "tran_led_class.h"

#define AW2023_MAX_BRIGHTNESS 200
#define AW2023_IMAX   0x3
#define AW2023_MIN_BRIGHTNESS_VALUE 130
#define AW2023_REPEAT_MAX_TIMES 15

/* register address */
#define AW2023_REG_RESET					0x00
#define AW2023_REG_GCR						0x01
#define AW2023_REG_STATUS					0x02
#define AW2023_REG_PATST					0x03
#define AW2023_REG_CUR						0x04
#define AW2023_REG_LCTR					0x30
#define AW2023_REG_LCFG0					0x31
#define AW2023_REG_LCFG1					0x32
#define AW2023_REG_LCFG2					0x33
#define AW2023_REG_PWM0						0x34
#define AW2023_REG_PWM1						0x35
#define AW2023_REG_PWM2						0x36
#define AW2023_REG_LED0T0					0x37
#define AW2023_REG_LED0T1					0x38
#define AW2023_REG_LED0T2					0x39
#define AW2023_REG_LED1T0					0x3A
#define AW2023_REG_LED1T1					0x3B
#define AW2023_REG_LED1T2					0x3C
#define AW2023_REG_LED2T0					0x3D
#define AW2023_REG_LED2T1					0x3E
#define AW2023_REG_LED2T2					0x3F

/* register bits */
#define AW2023_CHIPID						0x09
#define AW2023_RESET_MASK					0x55
#define AW2023_CHIP_DISABLE_MASK			0x00
#define AW2023_CHIP_ENABLE_MASK				0x01
#define AW2023_LED_BREATH_MODE_MASK			0x10
#define AW2023_LED_MANUAL_MODE_MASK			0x00
#define AW2023_LED_BREATHE_PWM_MASK			0xFF
#define AW2023_LED_MANUAL_PWM_MASK			0xFF
#define AW2023_LED_FADEIN_MODE_MASK			0x20
#define AW2023_LED_FADEOUT_MODE_MASK		0x40
#define Delay_time	0x00


#define MAX_RISE_TIME_MS					15
#define MAX_HOLD_TIME_MS					15
#define MAX_FALL_TIME_MS					15
#define MAX_OFF_TIME_MS						15



/* aw2023 register read/write access*/
#define REG_NONE_ACCESS						0
#define REG_RD_ACCESS						1 << 0
#define REG_WR_ACCESS						1 << 1
#define AW2023_REG_MAX						0x7F

struct aw2023_led {
	struct i2c_client *client;
	struct led_classdev cdev;
	struct device *dev;

	const char *led_name;
	struct tran_led_device *led_dev;
	struct work_struct brightness_work;
	struct mutex lock;
	int id;

	enum ChipType chip_type;
	int32_t cmd_data[TRAN_LED_CMD_LENGTH];

	int imax;

	//int led_current;
	int rise_time_ms;
	int hold_time_ms;
	int fall_time_ms;
	int off_time_ms;
	int led_mode;
	int led_color;
	int brightness_value;
	int brightness_count;
	//USD: add for Combo effect by hongwu.zhan 20251020 start
	struct work_struct combo_work;        // Combo effect work queue
	struct delayed_work combo_dwork;      // Delayed work queue
	bool is_second_stage;                 // second stage flag
	//USD: add for Combo effect by hongwu.zhan 20251020 end
	bool custom_white_enabled;
	int min_brightness;
};


enum  led_color_value {
	COLOR_RED = 0,
	COLOR_GREEN,
	COLOR_BLUE,
	COLOR_YELLOW,	//huang r+g
	COLOR_PURPLE,	//zi r+b
	COLOR_CYAN,		//qing g+b
	COLOR_WHITE,	//r+g+b
	COLOR_ALL_MAX,
};

enum  led_work_value {
	AW2023_LED_OFF,
	AW2023_LED_NORMAL,
	AW2023_LED_BLINK,
	AW2023_LED_BREATHE,
	//USD: add for Combo effect by hongwu.zhan 20251020 start
	AW2023_LED_COMBO1,  // combination Effect 1
	AW2023_LED_COMBO2,  // combination Effect 2
	AW2023_LED_COMBO3,  // combination Effect 3
	//USD: add for Combo effect by hongwu.zhan 20251020 end
	AW2023_LED_ALL_MAX,
};


#define GETARRAYNUM(array) (ARRAY_SIZE(array))

#endif
