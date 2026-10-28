/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/leds.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-mapping.h>
#include <linux/hrtimer.h>
#include <linux/fb.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/kthread.h>	/* For Kthread_run */
#include <linux/alarmtimer.h>
#include <linux/mutex.h>
#include "leds_aw2023.h"


#define AW2023_DRIVER_VERSION "V1.0.2"



const  unsigned int TIME_OF_PATTERN[] = {
	0, 130, 260, 380, 510, 770, 1040, 1600, 2100, 2600, 3100, 4200, 5200, 6200, 7300, 8300
};

static int findClosest(const unsigned int *pList, unsigned int number, unsigned int level)
{
    int closest = 8300;
    int diff = 8300;
	int currentDiff = 0;
	int i = 0;

	for (i = 0; i < number; i++) {
		currentDiff = abs(pList[i] - level);
		if (currentDiff < diff) {
			diff = currentDiff;
			closest = pList[i];
		}
	}

	for (i = 0; i < number; i++) {
		if (closest == pList[i])
			return i;
	}
	return 0;
}
static int aw2023_write(struct aw2023_led *led, u8 reg, u8 val)
{
	int ret = -EINVAL, retry_times = 0;

	do {
		ret = i2c_smbus_write_byte_data(led->client, reg, val);
		retry_times ++;
		if(retry_times == 5)
			break;
	}while (ret < 0);
	mdelay(1);

	return ret;
}

static int aw2023_read(struct aw2023_led *led, u8 reg, u8 *val)
{
	int ret = -EINVAL, retry_times = 0;

	do{
		ret = i2c_smbus_read_byte_data(led->client, reg);
		retry_times ++;
		if(retry_times == 5)
			break;
	}while (ret < 0);
	if (ret < 0)
		return ret;

	*val = ret;
	return 0;
}

static ssize_t aw2023_show_registers(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    struct aw2023_led *led = dev_get_drvdata(dev);
    u8 addr;
    u8 val;
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "aw2023");
    for (addr = 0x0; addr <= 0x3E; addr++) {
        ret = aw2023_read(led, addr, &val);
        if (ret == 0) {
            len = snprintf(buf + idx, PAGE_SIZE - idx,
                           "Reg[%.2X] = 0x%.2x\n", addr, val);
            idx += len;
        } else {
            dev_err(dev, "Failed to read register 0x%.2X\n", addr);
        }
    }

    return idx;
}

static ssize_t aw2023_store_register(struct device *dev,
                                     struct device_attribute *attr, const char *buf, size_t count)
{
    struct aw2023_led *led = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2) {
        ret = aw2023_write(led, reg, val);
        if (ret) {
            dev_err(dev, "Failed to write register 0x%.2X\n", reg);
            return ret;
        }
    } else {
        dev_err(dev, "Invalid input format\n");
        return -EINVAL;
    }

    return count;
}
static DEVICE_ATTR(aw2023_registers_debug, 0664, aw2023_show_registers, aw2023_store_register);

static void aw2023_soft_reset(struct aw2023_led *led)
{
	aw2023_write(led, AW2023_REG_RESET, AW2023_RESET_MASK);
	msleep(5);
}

static void aw2023_turn_off_all_leds(struct aw2023_led *led)
{
    u8 val;

    pr_info("%s: turning off all LEDs\n", __func__);
    mutex_lock(&led->lock);
    // Disable all LEDs
    aw2023_write(led, AW2023_REG_LCTR, 0x00);
    // Check if all LEDs are off
    aw2023_read(led, AW2023_REG_LCTR, &val);
    if (val == 0) {
        // Power off the chip if all LEDs are off
        aw2023_write(led, AW2023_REG_GCR, AW2023_CHIP_DISABLE_MASK);
    }
    mutex_unlock(&led->lock);
}

static void aw2023_brightness_work(struct work_struct *work)
{
	struct aw2023_led *led = container_of(work, struct aw2023_led,
					brightness_work);
	u8 val;
	int i = 0;

	pr_info("func=%s,brightness=%d\n",__func__,led->cdev.brightness);
	mutex_lock(&led->lock);

	/* enable aw2023 if disabled */
	aw2023_read(led, AW2023_REG_GCR, &val);
	if (!(val&AW2023_CHIP_ENABLE_MASK)) {
		aw2023_write(led, AW2023_REG_GCR, AW2023_CHIP_ENABLE_MASK);
		msleep(2);
	}

	if (led->cdev.brightness > 0) {
		if (led->cdev.brightness > led->cdev.max_brightness)
			led->cdev.brightness = led->cdev.max_brightness;
		switch (led->led_color) {
			case COLOR_RED:
			case COLOR_GREEN:
			case COLOR_BLUE:
				led->id = led->led_color;
				aw2023_write(led, AW2023_REG_PWM0 + led->id, led->cdev.brightness);
				aw2023_write(led, AW2023_REG_LCFG0 + led->id, led->imax);
				aw2023_write(led, AW2023_REG_CUR + led->id, led->imax);
				aw2023_write(led, AW2023_REG_GCR, 0x1);
				aw2023_write(led, AW2023_REG_LCTR, (1 << led->id)); //0x30 BIT2:LED2 BIT1:LED1 BIT0:LED0
				break;
			case COLOR_YELLOW: //R+G
				for (i=0;i<2;i++) {
					led->id = i;
					aw2023_write(led, AW2023_REG_PWM0 + led->id, led->cdev.brightness);
					aw2023_write(led, AW2023_REG_CUR + led->id, led->imax);
					aw2023_write(led, AW2023_REG_LCFG0 + led->id, led->imax);
				}
				aw2023_write(led, AW2023_REG_GCR, 0x1);
				aw2023_write(led, AW2023_REG_LCTR, 0x03); //0x30 BIT2:LED2 BIT1:LED1 BIT0:LED0
				break;
			case COLOR_PURPLE: //zi r+b
				for (i=0;i<2;i++) {
					led->id = 2*i;
					aw2023_write(led, AW2023_REG_PWM0 + led->id, led->cdev.brightness);
					aw2023_write(led, AW2023_REG_CUR + led->id, led->imax);
					aw2023_write(led, AW2023_REG_LCFG0 + led->id, led->imax);
				}
				aw2023_write(led, AW2023_REG_GCR, 0x1);
				aw2023_write(led, AW2023_REG_LCTR, 0X05); //0x30 BIT2:LED2(G) BIT1:LED1(G) BIT0:LED0(R)
				break;
			case COLOR_CYAN: //qing g+b
				for (i=1;i<3;i++) {
					led->id = i;
					aw2023_write(led, AW2023_REG_PWM0 + led->id, led->cdev.brightness);
					aw2023_write(led, AW2023_REG_CUR + led->id, led->imax);
					aw2023_write(led, AW2023_REG_LCFG0 + led->id, led->imax);
				}
				aw2023_write(led, AW2023_REG_GCR, 0x1);
				aw2023_write(led, AW2023_REG_LCTR, 0X06); //0x30 BIT2:LED2(G) BIT1:LED1(G) BIT0:LED0(R)	
				break;
			case COLOR_WHITE: //bau r+g+b
				for (i=0;i<3;i++) {
					led->id = i;
					aw2023_write(led, AW2023_REG_PWM0 + led->id, led->cdev.brightness);
					aw2023_write(led, AW2023_REG_CUR + led->id, led->imax);
					aw2023_write(led, AW2023_REG_LCFG0 + led->id, led->imax);
				}
				if (led->custom_white_enabled) {
					aw2023_write(led, AW2023_REG_LCFG0 + 2, 5);//LED2:blue->5
				}
				aw2023_write(led, AW2023_REG_GCR, 0x1);
				aw2023_write(led, AW2023_REG_LCTR, 0X07); //0x30 BIT2:LED2(G) BIT1:LED1(G) BIT0:LED0(R)	
				break;
			default:
				aw2023_write(led, AW2023_REG_GCR, 0x00);
				aw2023_write(led, AW2023_REG_LCTR, 0x0); //0x30 bit2:LED3	bit1:LED2 bit0:LED1	
				break;
		}
	} else {
		aw2023_write(led, AW2023_REG_GCR, 0x0);
		aw2023_write(led, AW2023_REG_LCTR, 0x0);
	}

	/*
	* If value in AW_REG_LED_ENABLE is 0, it means the RGB leds are
	* all off. So we need to power it off.
	*/
	aw2023_read(led, AW2023_REG_LCTR, &val);
	if (val == 0) {
		aw2023_write(led, AW2023_REG_GCR, AW2023_CHIP_DISABLE_MASK);

			mutex_unlock(&led->lock);
			return;
	}

	mutex_unlock(&led->lock);
}

static void aw2023_led_time_regester(struct aw2023_led *led)
{
	int size = GETARRAYNUM(TIME_OF_PATTERN);
	led->rise_time_ms = findClosest(TIME_OF_PATTERN, size, led->rise_time_ms);
	led->fall_time_ms = findClosest(TIME_OF_PATTERN, size, led->fall_time_ms);
	led->hold_time_ms = findClosest(TIME_OF_PATTERN, size, led->hold_time_ms);
	led->off_time_ms = findClosest(TIME_OF_PATTERN, size, led->off_time_ms);
}

static void aw2023_led_breathe_set(struct aw2023_led *led)
{
	u8 val;
	int i=0;

	pr_info("func=%s,led->id=%d,imax=%d,brightness_value=%d,rise_time_ms=%d,fall_time_ms=%d,brightness_count=%d\n",
		__func__,led->id,led->imax,led->brightness_value,led->rise_time_ms,led->fall_time_ms,led->brightness_count);
	// Turn off all LEDs before executing any lighting command
	aw2023_turn_off_all_leds(led);

	/* enable regulators if they are disabled */
	/* enable aw2023 if disabled */
	aw2023_read(led, AW2023_REG_GCR, &val);
	if (!(val&0x01)) {
		aw2023_write(led, AW2023_REG_GCR, AW2023_CHIP_ENABLE_MASK);
	}

	led->cdev.brightness = led->brightness_value ? led->cdev.max_brightness : 0;
	aw2023_led_time_regester(led);
	if (led->brightness_value > 0) {
		switch (led->led_color) {
			case COLOR_RED:
			case COLOR_GREEN:
			case COLOR_BLUE:
				led->id = led->led_color;
				aw2023_write(led, AW2023_REG_PWM0 + led->id, led->brightness_value);
				aw2023_write(led, AW2023_REG_LCFG0 + led->id , (led->imax | 1 <<4));
				aw2023_write(led, AW2023_REG_CUR + led->id, led->imax);
				aw2023_write(led, AW2023_REG_LED0T0 + led->id*3,(led->rise_time_ms << 4 | led->hold_time_ms));
				aw2023_write(led, AW2023_REG_LED0T1 + led->id*3, (led->fall_time_ms << 4 | led->off_time_ms));
				aw2023_write(led, AW2023_REG_LED0T2 + led->id*3, Delay_time<<4 | led->brightness_count);

				aw2023_write(led, AW2023_REG_GCR, 0x1);
				aw2023_write(led, AW2023_REG_LCTR, (1 << led->id)); //0x30 BIT2:led2  BIT1:LED1  BIT0:LED0
				break;
			case COLOR_YELLOW: //huang r+g
				for (i=0; i<2;i++) {
					led->id = i;
					aw2023_write(led, AW2023_REG_PWM0 + led->id, led->brightness_value);
					aw2023_write(led, AW2023_REG_LCFG0 + led->id , (led->imax | 1 <<4));
					aw2023_write(led, AW2023_REG_CUR + led->id, led->imax);
					aw2023_write(led, AW2023_REG_LED0T0 + led->id*3,(led->rise_time_ms << 4 | led->hold_time_ms));
					aw2023_write(led, AW2023_REG_LED0T1 + led->id*3, (led->fall_time_ms << 4 | led->off_time_ms));
					aw2023_write(led, AW2023_REG_LED0T2 + led->id*3, Delay_time<<4 | led->brightness_count);			
				}
				aw2023_write(led, AW2023_REG_GCR, 0x1);
				aw2023_write(led, AW2023_REG_LCTR, 0x03); //0x30 BIT2:led2  BIT1:LED1  BIT0:LED0
				break;
			case COLOR_PURPLE: //zi r+b
				for (i=0; i<2;i++) {
					led->id = 2*i;
					aw2023_write(led, AW2023_REG_PWM0 + led->id, led->brightness_value);
					aw2023_write(led, AW2023_REG_LCFG0 + led->id , (led->imax | 1 <<4));
					aw2023_write(led, AW2023_REG_CUR + led->id, led->imax);
					aw2023_write(led, AW2023_REG_LED0T0 + led->id*3,(led->rise_time_ms << 4 | led->hold_time_ms));
					aw2023_write(led, AW2023_REG_LED0T1 + led->id*3, (led->fall_time_ms << 4 | led->off_time_ms));
					aw2023_write(led, AW2023_REG_LED0T2 + led->id*3, Delay_time<<4 | led->brightness_count);			
				}
				aw2023_write(led, AW2023_REG_GCR, 0x1);
				aw2023_write(led, AW2023_REG_LCTR, 0x05); //0x30 BIT2:led2  BIT1:LED1  BIT0:LED0
				break;
			case COLOR_CYAN:	//qing g+b
				for (i=1; i<3;i++) {
					led->id = i;
					aw2023_write(led, AW2023_REG_PWM0 + led->id, led->brightness_value);
					aw2023_write(led, AW2023_REG_LCFG0 + led->id , (led->imax | 1 <<4));
					aw2023_write(led, AW2023_REG_CUR + led->id, led->imax);
					aw2023_write(led, AW2023_REG_LED0T0 + led->id*3,(led->rise_time_ms << 4 | led->hold_time_ms));
					aw2023_write(led, AW2023_REG_LED0T1 + led->id*3, (led->fall_time_ms << 4 | led->off_time_ms));
					aw2023_write(led, AW2023_REG_LED0T2 + led->id*3, Delay_time<<4 | led->brightness_count);			
				}
				aw2023_write(led, AW2023_REG_GCR, 0x1);
				aw2023_write(led, AW2023_REG_LCTR, 0x06); //0x30 BIT2:led2  BIT1:LED1  BIT0:LED0
				break;
			case COLOR_WHITE:	//bai r+g+b
				for (i=0; i<3;i++) {
					led->id = i;
					aw2023_write(led, AW2023_REG_PWM0 + led->id, led->brightness_value);
					aw2023_write(led, AW2023_REG_LCFG0 + led->id , (led->imax | 1 <<4));
					aw2023_write(led, AW2023_REG_CUR + led->id, led->imax);
					aw2023_write(led, AW2023_REG_LED0T0 + led->id*3,(led->rise_time_ms << 4 | led->hold_time_ms));
					aw2023_write(led, AW2023_REG_LED0T1 + led->id*3, (led->fall_time_ms << 4 | led->off_time_ms));
					aw2023_write(led, AW2023_REG_LED0T2 + led->id*3, Delay_time<<4 | led->brightness_count);			
				}
				if (led->custom_white_enabled) {
					aw2023_write(led, AW2023_REG_LCFG0 + 2 , (5 | 1 <<4)); //LED2:blue->5
				}
				aw2023_write(led, AW2023_REG_GCR, 0x1);
				aw2023_write(led, AW2023_REG_LCTR, 0x07); //0x30 BIT2:led2  BIT1:LED1  BIT0:LED0
				break;
			default:
				aw2023_write(led, AW2023_REG_GCR, 0x0);
				aw2023_write(led, AW2023_REG_LCTR, 0x0);
				break;
		}
	} else {
		aw2023_write(led, AW2023_REG_LCTR, 0x0);
		aw2023_write(led, AW2023_REG_LCTR, 0x0);
	}

	/*
	* If value in AW_REG_LED_ENABLE is 0, it means the RGB leds are
	* all off. So we need to power it off.
	*/
	aw2023_read(led, AW2023_REG_LCTR, &val);
	if (val == 0) {
		aw2023_write(led, AW2023_REG_GCR, AW2023_CHIP_DISABLE_MASK);
			return;
	}
}

//USD: add for Combo effect by hongwu.zhan 20251020 start
static void aw2023_combo_work_handler(struct work_struct *work)
{
    struct aw2023_led *led = container_of(work, struct aw2023_led, combo_work);

    switch (led->led_mode) {
        case AW2023_LED_COMBO1:
            /*  Effect 1  */
            led->rise_time_ms = 0;
            led->hold_time_ms = 380;
            led->fall_time_ms = 770;
            led->off_time_ms = 0;
            led->brightness_count = 1;
            aw2023_led_breathe_set(led);
            break;
        case AW2023_LED_COMBO2:
            if (!led->is_second_stage) {
                /* Stage 1: 3 fast blinks */
                led->rise_time_ms = 130;
                led->fall_time_ms = 130;
                led->brightness_count = 3;
                aw2023_led_breathe_set(led);
                schedule_delayed_work(&led->combo_dwork, msecs_to_jiffies(800));
                led->is_second_stage = true;
            } else {
                /* Stage 2: single fade effect */
                led->rise_time_ms = 0;
                led->hold_time_ms = 130;
                led->fall_time_ms = 770;
                led->brightness_count = 1;
                aw2023_led_breathe_set(led);
                led->is_second_stage = false;
            }
            break;
        case AW2023_LED_COMBO3:
            if (!led->is_second_stage) {
                /* Stage 1: 130ms rise + 260ms fall */
                led->rise_time_ms = 130;
                led->fall_time_ms = 260;
                led->brightness_count = 1;
                aw2023_led_breathe_set(led);
                schedule_delayed_work(&led->combo_dwork, msecs_to_jiffies(410));
                led->is_second_stage = true;
            } else {
                /* Stage 2: 260ms rise + 510ms fall */
                led->rise_time_ms = 260;
                led->fall_time_ms = 510;
                led->brightness_count = 1;
                aw2023_led_breathe_set(led);
                led->is_second_stage = false;
            }
            break;
    }
}

/**
 * Delayed work handler
 */
static void aw2023_combo_delayed_work_handler(struct work_struct *work)
{
    struct aw2023_led *led = container_of(work, struct aw2023_led, combo_dwork.work);
    schedule_work(&led->combo_work);
}

/**
 * Set combo effect (minimal version)
 */
static void aw2023_led_combo_effect(struct aw2023_led *led)
{
    // Turn off all LEDs before executing any lighting command
    aw2023_turn_off_all_leds(led);
    led->is_second_stage = false;
    schedule_work(&led->combo_work);
}
//USD: add for Combo effect by hongwu.zhan 20251020 end

static void aw2023_set_brightness(struct led_classdev *cdev,
				enum led_brightness brightness)
{
	struct aw2023_led *led = container_of(cdev, struct aw2023_led, cdev);

	led->cdev.brightness = brightness;

	schedule_work(&led->brightness_work);
}

static int aw2023_led_work(struct aw2023_led *led)
{
	led->id = led->led_color;
	led->brightness_count = min(led->brightness_count, AW2023_REPEAT_MAX_TIMES);
	pr_info("aw2023 led_mode=%d,led_color=%d,brightness_value=%d,rise_time_ms=%d,hold_time_ms=%d,fall_time_ms=%d,off_time_ms=%d\n",
			led->led_mode, led->led_color,led->brightness_value,
			led->rise_time_ms, led->hold_time_ms, led->fall_time_ms,led->off_time_ms);

	// Set the min brightness from device tree
	if (led->brightness_value > 0 && led->brightness_value < led->min_brightness) {
		led->brightness_value = led->min_brightness;
		pr_info("func=%s, using min brightness from device tree brightness=%d\n",__func__, led->brightness_value);
	}
	switch (led->led_mode) {
		case AW2023_LED_OFF:
			led->led_mode = 0;
			led->brightness_value = 0;
			aw2023_soft_reset(led);
			break;
		case AW2023_LED_NORMAL:
			aw2023_set_brightness(&led->cdev,led->brightness_value);
			break;
		case AW2023_LED_BLINK:
		case AW2023_LED_BREATHE:
			aw2023_led_breathe_set(led);
			break;
		//USD: add for Combo effect by hongwu.zhan 20251020 start
		case AW2023_LED_COMBO1:
		case AW2023_LED_COMBO2:
		case AW2023_LED_COMBO3:
			aw2023_led_combo_effect(led);
			break;
		//USD: add for Combo effect by hongwu.zhan 20251020 end
		default:

			break;
	}

	return 0;
}

static int aw2023_get_tran_led_cmd(struct tran_led_device *led_dev, int32_t * cmd_data,int size)
{
	struct aw2023_led *led = dev_get_drvdata(&led_dev->dev);

    if (size != sizeof(led->cmd_data)) {
        pr_err("Invalid command data size\n");
        return -EINVAL;
    }

	memcpy(cmd_data, led->cmd_data, sizeof(led->cmd_data));
	return 0;
}

static int aw2023_set_tran_led_cmd(struct tran_led_device *led_dev, int32_t *cmd_data, int size)
{
    struct aw2023_led *led;
    int32_t *p;

    led = dev_get_drvdata(&led_dev->dev);
    if (!led) {
        pr_err("Failed to get device data\n");
        return -EINVAL;
    }

    if (size != sizeof(led->cmd_data) || cmd_data[0] != DRIVER_SOLUTION) {
        pr_err("Invalid command data\n");
        return -EINVAL;
    }

	memcpy(led->cmd_data, cmd_data, sizeof(led->cmd_data));

    p = cmd_data + 2;
    led->led_mode = *p++;
    led->led_color = *p++;
    led->brightness_value = *p++;

    switch (led->led_mode) {
    case AW2023_LED_BLINK:
        led->hold_time_ms = *p++;
        led->off_time_ms = *p++;
        break;
    case AW2023_LED_BREATHE:
        led->rise_time_ms = *p++;
        led->fall_time_ms = *p++;
        break;
    default:
        break;
    }

    led->brightness_count = *p;

    aw2023_led_work(led);

    return 0;
}

static int aw2023_get_chip_type(struct tran_led_device *led_dev, enum ChipType * p)
{
    struct aw2023_led *led;

    led = dev_get_drvdata(&led_dev->dev);
    if (!led) {
        pr_err("Failed to get device data\n");
        return -EINVAL;
    }
	* p = led->chip_type;
	return 0;
}

static int aw2023_store_tran_led_cmd(struct tran_led_device *led_dev,const char *buf)
{
    struct aw2023_led *led;
	int rc;
	int temp[2];

    led = dev_get_drvdata(&led_dev->dev);
    if (!led) {
        pr_err("Failed to get device data\n");
        return -EINVAL;
    }

	rc = sscanf(buf, "%d %d %d %d %d %d\n",
			&led->led_mode,&led->led_color,&led->brightness_value,
			&temp[0], &temp[1], &led->brightness_count);
	if(led->led_mode == AW2023_LED_BLINK){
		led->hold_time_ms = temp[0];
		led->off_time_ms = temp[1];
	}else if(led->led_mode == AW2023_LED_BREATHE){
		led->rise_time_ms = temp[0];
		led->fall_time_ms = temp[1];
	}
	aw2023_led_work(led);
	return rc;
}

static int aw2023_show_tran_led_cmd(struct tran_led_device *led_dev, char *buf)
{
    struct aw2023_led *led;

    led = dev_get_drvdata(&led_dev->dev);
    if (!led) {
        pr_err("Failed to get device data\n");
        return -EINVAL;
    }

	return sprintf(buf, "%d\n", led->led_mode);
}

static const struct tran_led_ops leds_aw2023_ops = {
	/* app & hal api */
	.get_chip_type 		= aw2023_get_chip_type,
	.set_tran_led_cmd 	= aw2023_set_tran_led_cmd,
	.get_tran_led_cmd 	= aw2023_get_tran_led_cmd,

	/* midtest mode api */
	.store_tran_led_cmd = aw2023_store_tran_led_cmd,
	.show_tran_led_cmd 	= aw2023_show_tran_led_cmd,
};

static int aw2023_check_chipid(struct aw2023_led *led)
{
	u8 val;
	u8 cnt;

	for(cnt = 5; cnt > 0; cnt --)
	{
		aw2023_read(led, AW2023_REG_RESET, &val);
		dev_notice(&led->client->dev,"aw2023 chip id %0x",val);
		if (val == AW2023_CHIPID)
			return 0;
	}
		return -EINVAL;
}

static struct attribute *aw2023_led_attributes[] = {
	NULL,
};

static struct attribute_group aw2023_led_attr_group = {
	.attrs = aw2023_led_attributes
};
static int aw2023_led_err_handle(struct aw2023_led *led_info)
{
	sysfs_remove_group(&led_info->cdev.dev->kobj,&aw2023_led_attr_group);
	led_classdev_unregister(&led_info->cdev);
	cancel_work_sync(&led_info->brightness_work);
	return 0;
}

static int aw2023_parse_led_cdev(struct aw2023_led *led_info,
        struct device_node *np)
{
    struct device_node *temp = NULL;
    int ret = -1;

    pr_info("%s: start\n", __func__);

	if (led_info == NULL)
		return -EINVAL;

	ret = of_property_read_string(np, "led_name", &led_info->led_name);
	if (ret < 0) {
		led_info->led_name = "primary_led";
		pr_err("%s no led_name(%d)\n", __func__, ret);
	}

    // Initialize value false
    led_info->custom_white_enabled = false;
    // Initialize min_brightness with default value 1
    led_info->min_brightness = 1;

    for_each_child_of_node(np, temp) {
        ret = of_property_read_string(temp, "aw2023,name",
            &led_info->cdev.name);
        if (ret < 0) {
            pr_info("Failure reading led name, ret = %d\n", ret);
            goto free_err;
        }
        ret = of_property_read_u32(temp, "aw2023,imax",
            &led_info->imax);
        if (ret < 0) {
            pr_info("Failure reading imax, ret = %d\n", ret);
            led_info->imax = AW2023_IMAX;
        }
        ret = of_property_read_u32(temp, "aw2023,brightness",
            &led_info->cdev.brightness);
        if (ret < 0) {
           pr_info("Failure reading brightness, ret = %d\n", ret);
           led_info->cdev.brightness = AW2023_MAX_BRIGHTNESS;
        }
        ret = of_property_read_u32(temp, "aw2023,max_brightness",
            &led_info->cdev.max_brightness);
        if (ret < 0) {
           pr_info("Failure reading max brightness, ret = %d\n", ret);
            led_info->cdev.brightness = AW2023_MAX_BRIGHTNESS;
        }
        ret = of_property_read_u32(temp, "aw2023,min-brightness",
            &led_info->min_brightness);
        if (ret < 0) {
            pr_info("Failure reading min brightness, using default value 1, ret = %d\n", ret);
            led_info->min_brightness = 1;
        }
        if (of_property_read_bool(temp, "aw2023,custom_white")) {
            led_info->custom_white_enabled = true;
            pr_info("aw2023,custom_white property found, enabling custom white LED config\n");
        }
    }
	INIT_WORK(&led_info->brightness_work, aw2023_brightness_work);
    led_info->cdev.brightness_set = aw2023_set_brightness;
    ret = led_classdev_register(&led_info->client->dev, &led_info->cdev);
    if (ret) {
        pr_info("unable to register led ret=%d\n", ret);
        goto free_err;
    }

    ret = sysfs_create_group(&led_info->cdev.dev->kobj,
            &aw2023_led_attr_group);
    if (ret) {
        pr_info("led sysfs ret: %d\n", ret);
        goto free_class;
    }
	pr_info("%s: imax=%d,brightness=%d,max_value=%d end\n",
			__func__,led_info->imax,led_info->cdev.brightness,led_info->cdev.max_brightness);
    return 0;

free_class:
 	aw2023_led_err_handle(led_info);
	led_classdev_unregister(&led_info->cdev);
	cancel_work_sync(&led_info->brightness_work);
	return ret;
free_err:
	aw2023_led_err_handle(led_info);
    return ret;
}

static int aw2023_led_probe(struct i2c_client *client)
{
	struct device_node *node;
	struct aw2023_led *led_info;
	int ret = 0;

	node = client->dev.of_node;
	if (node == NULL)
		return -EINVAL;

	led_info = devm_kzalloc(&client->dev,sizeof(struct aw2023_led), GFP_KERNEL);
	if (!led_info)
		return -ENOMEM;

	led_info->client = client;
	led_info->client->dev = client->dev;
	led_info->led_mode = 0;
	led_info->dev = &client->dev;
	led_info->brightness_value = 0;
	led_info->led_color = 0;

	mutex_init(&led_info->lock);

	i2c_set_clientdata(client, led_info);

	ret = aw2023_check_chipid(led_info);
	if (ret) {
		pr_err("Check chip id error\n");
		goto fail_parsed_node;
	}

	led_info->chip_type = AW2023;

	/* soft rst */
	aw2023_soft_reset(led_info);

	dev_set_drvdata(&client->dev, led_info);

    ret = aw2023_parse_led_cdev(led_info, node);
    if (ret < 0) {
        pr_err("%s error creating led class dev\n", __func__);
        goto fail_parsed_node;
    }

	ret = device_create_file(&client->dev, &dev_attr_aw2023_registers_debug);

	led_info->led_dev = tran_led_device_register(led_info->led_name, led_info->dev, led_info, &leds_aw2023_ops, NULL);
	if (IS_ERR_OR_NULL(led_info->led_dev)) {
		pr_err("%s : register tran led failed!\n", __func__);
		goto fail_parsed_node;
	}

	//USD: add for Combo effect by hongwu.zhan 20251020 start
	INIT_WORK(&led_info->combo_work, aw2023_combo_work_handler);
	INIT_DELAYED_WORK(&led_info->combo_dwork, aw2023_combo_delayed_work_handler);
	//USD: add for Combo effect by hongwu.zhan 20251020 end
	pr_info("%s: end\n", __func__);
	return 0;

fail_parsed_node:
	mutex_destroy(&led_info->lock);
	devm_kfree(&client->dev, led_info);
	led_info = NULL;
	pr_info("%s: fail\n", __func__);
	return ret;
}

static void aw2023_led_remove(struct i2c_client *client)
{
	struct aw2023_led *led_info = i2c_get_clientdata(client);
	if (IS_ERR_OR_NULL(led_info))
		return;

	aw2023_led_err_handle(led_info);
	tran_led_device_unregister(led_info->led_dev);
	//USD: add for Combo effect by hongwu.zhan 20251020 start
	cancel_work_sync(&led_info->combo_work);
	cancel_delayed_work_sync(&led_info->combo_dwork);
	//USD: add for Combo effect by hongwu.zhan 20251020 end
	mutex_destroy(&led_info->lock);
	devm_kfree(&client->dev, led_info);
	led_info = NULL;
	return;
}

static void aw2023_led_shutdown(struct i2c_client *client)
{
	struct aw2023_led *led_info = i2c_get_clientdata(client);

	aw2023_write(led_info, AW2023_REG_GCR, AW2023_CHIP_DISABLE_MASK);
	//tran_led_device_unregister(led_info->led_dev);
}

static const struct i2c_device_id aw2023_led_id[] = {
	{"aw2023_led", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, aw2023_led_id);

static struct of_device_id aw2023_match_table[] = {
	{ .compatible = "awinic,aw2023_led",},
	{ },
};

static struct i2c_driver aw2023_led_driver = {
	.probe = aw2023_led_probe,
	.remove = aw2023_led_remove,
	.shutdown = aw2023_led_shutdown,
	.driver = {
		.name = "aw2023_led",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw2023_match_table),
	},
	.id_table = aw2023_led_id,
};

static int __init aw2023_led_init(void)
{
	pr_info("%s: driver version: %s\n", __func__, AW2023_DRIVER_VERSION);
	return i2c_add_driver(&aw2023_led_driver);
}
module_init(aw2023_led_init);

static void __exit aw2023_led_exit(void)
{
	i2c_del_driver(&aw2023_led_driver);
}
module_exit(aw2023_led_exit);

MODULE_AUTHOR("<liweilei@awinic.com.cn>");
MODULE_DESCRIPTION("AWINIC AW2023 LED driver");
MODULE_LICENSE("GPL v2");
