/*
 * leds-hk32f0301.c   hk32f0301 led module
 *
 * Version: v0.3.0
 *
 * Copyright (c) 2020 AWINIC Technology CO., LTD
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation;either version 2 of the License, or (at your
 * option) any later version.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/firmware.h>
#include <linux/i2c-dev.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio/consumer.h>
#include "leds-hk32f0301.h"
#include "../core/tran_led_class.h"

#define TRAN_LED_FRAME_TYPE 0x03

#define CMD_OPEN_ALL       (0x02)
#define CMD_CLOSE_ALL      (0x05)

#define HK32FXXXX_I2C_NAME		"hk32f0301_led"
#define HK32FXXXX_DRIVER_VERSION	"20251203"
#define HK32FXXXX_READ_CHIPID_RETRIES	5
#define HK32FXXXX_I2C_RETRIES		5

#define HK32FXXXX_I2C_FAIL		-1
#define HK32FXXXX_I2C_SUCCESS		0

#define HK32FXXXX_1K_BYTE		1024
#define MCU_FWVER_ADDRESS		0x08000D00
#define MCU_FWVER_ADDRESS_64K		0x08001000
#define FWVER_UPDATE_DATA_MAX		1032
#define FWVER_IAP_STATUS		0x55
#define FWVER_APP_STATUS		0xAA

static const unsigned char support_chip_id[] = {
0x40, 0x41, 0x42, 0x43, 0x52, 0x53, 0x61
};

static const unsigned char two_supplies_list[] = {
 0x52, 0x53
};


/*******************************************************************************
 *
 * hk32f0301 leds command
 *
 ******************************************************************************/
//static unsigned char leds_open_all[] = {0x02, 0x0F};
static unsigned char leds_close_all[] = {0x05, 0x03};
//static unsigned char leds_audio[] = {0x03, 0x00};
static unsigned char leds_read_chipid[] = {0xA1, 0x1F, 0xFF, 0xF1, 0x3E, 0x00, 0x01};
//static unsigned char leds_read_firmwareid[] = {0xA1, 0x08, 0x00, 0x3F, 0x00, 0x00, 0x0c};
static unsigned char leds_read_firmwareid_64K[] = {0xA1, 0x08, 0x00, 0xFE, 0x00, 0x00, 0x0c};
//static unsigned char leds_set_brightness_all[] = {0x04,0x01};
/* fw update use */
static unsigned char leds_fwver_update_iap[] = {0xAE};
//static unsigned char leds_fwver_update_userdata[] = {0xA3, 0x08, 0x00, 0x14, 0x00, 0x04, 0x00, 0x04, 0x00, 0xff};
//static unsigned char leds_fwver_update_fwver_id[] = {0xA3, 0x08, 0x00, 0x3F, 0x00, 0x00, 0x0c, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned char leds_fwver_update_fwver_id_64K[] = {0xA3, 0x08, 0x00, 0xFE, 0x00, 0x00, 0x0c, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
//static unsigned char leds_fwver_update_crc[] = {0xA3, 0x08, 0x00, 0x3E, 0x00, 0x00, 0x04, 0xff,0xff,0xff,0xff,0xff};
static unsigned char leds_fwver_update_crc_64K[] = {0xA3, 0x08, 0x00, 0xFC, 0x00, 0x00, 0x04, 0xff,0xff,0xff,0xff,0xff};
static unsigned char leds_fwver_get_crc_result[] = {0xAC};
static unsigned char leds_fwver_running[] = {0xAB};
static unsigned char leds_fwver_read_program_status[] = {0xAD};
static unsigned char leds_read_factory_data[] = {0xA1, 0x11, 0x00, 0x11, 0x00, 0x00, 0x20};
static unsigned char leds_read_factory[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x88, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xc0};

enum hk32fxxxx_effect {
	HK32FXXXX_PARTY_BREATHMODE,
	HK32FXXXX_PARTY_WATERMODE,
	HK32FXXXX_PARTY_FLASHMODE,
	HK32FXXXX_CHARGE_PREVIEW,
	HK32FXXXX_CHARGE,
	HK32FXXXX_CHARGE_FULL,//5
	HK32FXXXX_PHOTO_3S,
	HK32FXXXX_PHOTO_5S,
	HK32FXXXX_PHOTO_10S,
	HK32FXXXX_CALL,
	HK32FXXXX_RECORDING,//10
	HK32FXXXX_NOTICE,
	HK32FXXXX_BOOT,
	HK32FXXXX_AUDIO_MODE_PREVIEW,
	HK32FXXXX_AUDIO_MODE,
	HK32FXXXX_GAME_START,
	HK32FXXXX_FLOAX_PREVIEW,
	HK32FXXXX_AWAKE,
	HK32FXXXX_ANALYSYS,
	HK32FXXXX_ANSWER,
	/*MLBB GAME*/
	HK32FXXXX_MLBB_GAME,//20
	HK32FXXXX_MLBB_GAME_FIRSTBLOOD = HK32FXXXX_MLBB_GAME,
	HK32FXXXX_MLBB_GAME_SINGLEKILL,
	HK32FXXXX_MLBB_GAME_KILLSTREAKS,
	/*HOK GAME*/
	HK32FXXXX_HOK_GAME,
	HK32FXXXX_HOK_GAME_FIRSTBLOOD = HK32FXXXX_HOK_GAME,
	HK32FXXXX_HOK_GAME_SINGLEKILL,
	HK32FXXXX_HOK_GAME_KILLSTREAKS,
	/*PUBG GAME*/
	HK32FXXXX_PUBG_GAME,
	HK32FXXXX_PUBG_GAME_FIRE = HK32FXXXX_PUBG_GAME,
	HK32FXXXX_PUBG_GAME_CLOSESCOPE,
	HK32FXXXX_PUBG_GAME_OPENSCOPE,
	HK32FXXXX_PUBG_GAME_DRIVING,
	/*FF GAME*/
	HK32FXXXX_FF_GAME,
	HK32FXXXX_FF_GAME_FIRE = HK32FXXXX_FF_GAME,
	HK32FXXXX_FF_GAME_CLOSESCOPE,
	HK32FXXXX_FF_GAME_OPENSCOPE,

	HK32FXXXX_EFFECT_MAX,
};

/*******************************************************************************
 *
 * hk32f0301 i2c wakeup
 *
 ******************************************************************************/
static void hk32fxxxx_leds_wakeup(struct hk32fxxxx *chip)
{
#if 1
	unsigned char leds_wakeup[1] = {0x55};
	int msg_num = 1;

	struct i2c_msg msg_leds_wakeup[] = {
		{
			.addr = 0,
			.flags = 0, //write
			.buf = leds_wakeup,
			.len = ARRAY_SIZE(leds_wakeup),
		},
	};

	i2c_transfer(chip->i2c->adapter, msg_leds_wakeup, msg_num);
#endif
}



/*******************************************************************************
 *
 * hk32f0301 i2c read/write
 *
 ******************************************************************************/
static int hk32fxxxx_rw_multi_data(struct hk32fxxxx *chip,
		struct i2c_msg *regs, int num_regs)
{
	if (!chip || !chip->i2c || !chip->dev || !regs || num_regs <= 0) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	int ret = -1;
	int cnt = 0;

	if (!chip->i2c->adapter) {
		dev_err(chip->dev, "[%s] i2c adapter is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&chip->i2c_lock);
	if (chip->wakeup_flag == 1) {
		hk32fxxxx_leds_wakeup(chip);
	}
	while (cnt < HK32FXXXX_I2C_RETRIES) {
		ret = i2c_transfer(chip->i2c->adapter, regs, num_regs);
		if (ret < 0) {
			dev_err(chip->dev, "[%s] i2c transfer error:cnt:%d!\n", __func__, cnt);
		} else {
			//dev_err(chip->dev, "[%s] i2c transfer success!\n", __func__);
			break;
		}
		cnt++;
		msleep(10);
	}

	mutex_unlock(&chip->i2c_lock);

	return ret;
}

/*********************************************
*test
***************************************************/
static int hk32fxxxx_factory_test(struct hk32fxxxx *chip, int mode)
{
	unsigned char test_cmd[2] = {0xAF, 0x00};

	struct i2c_msg msg_test[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0,
			.buf = test_cmd,
			.len = ARRAY_SIZE(test_cmd),
		},
	};

	// dev_info(chip->dev, "%s: mode=%d\n", __func__, mode);

	if (!chip || !chip->dev) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	switch (mode) {
		case HK32FXXXX_FACTORY_SHORT:
			test_cmd[1] = 0x01;
			usleep_range(1500000, 2000000);
			break;
		case HK32FXXXX_FACTORY_OPEN:
			test_cmd[1] = 0x02;
			usleep_range(1500000, 2000000);
			break;
		default:
			return -1;
	}
	return hk32fxxxx_rw_multi_data(chip, msg_test, ARRAY_SIZE(msg_test));
}

static int hk32fxxxx_read_factory_result(struct hk32fxxxx *chip)
{
	u8 read_buf[32] = {0};
	int ret;
	int i = 0;
	chip->test_result = 0;
	int error_led = 0;
	struct i2c_msg msg_write[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0,
			.buf = leds_read_factory_data,
			.len = ARRAY_SIZE(leds_read_factory_data),
		},
	};
	struct i2c_msg msg_read[] = {
		{
			.addr = chip->i2c->addr,
			.flags = I2C_M_RD,
			.buf = read_buf,
			.len = ARRAY_SIZE(read_buf),
		},
	};

	dev_info(chip->dev, "%s: in\n", __func__);
	// for (int i = 0; i < msg_write[0].len; i++) {
	// 	dev_info(chip->dev,	"i2c write buf[%d] = 0x%02x\n",	i, msg_write[0].buf[i]);
	// }

	ret = hk32fxxxx_rw_multi_data(chip, msg_write, ARRAY_SIZE(msg_write));
	if (ret < 0)
		return ret;

	msleep(1);
	ret = hk32fxxxx_rw_multi_data(chip, msg_read, ARRAY_SIZE(msg_read));
	if (ret < 0)
		return ret;

	// for (int i = 0; i < msg_read[0].len; i++) {
	// 	dev_info(chip->dev,	"read_buf[%d] = 0x%02x\n", i,	read_buf[i]);
	// }
	memcpy(chip->factory_data, read_buf, sizeof(read_buf));
	for (i = 0; i < 32; i++) {
		if ((read_buf[i] & leds_read_factory[i]) != leds_read_factory[i]) {
			error_led++;
		}
	}
	if (error_led < 13) {
		chip->test_result = 0;
	} else {
		chip->test_result = 1;
	}
	// dev_info(chip->dev, "error_led = %d\n", error_led);
	error_led = 0;
	return chip->test_result;
}

/*****************************************************
 *
 * hk32fxxxx leds interface
 *
 *****************************************************/
static int hk32fxxxx_leds_open_all(struct hk32fxxxx *chip, unsigned char brightness)
{
	static unsigned char leds_open_all_args[2] = {0x02, 0x0F};
	leds_open_all_args[1] = brightness;
	struct i2c_msg msg_leds_open_all[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0, //write
			.buf = leds_open_all_args,
			.len = ARRAY_SIZE(leds_open_all_args),
		},
	};

	return hk32fxxxx_rw_multi_data(chip, msg_leds_open_all, ARRAY_SIZE(msg_leds_open_all));
}

static int hk32fxxxx_leds_close_all(struct hk32fxxxx *chip)
{
	struct i2c_msg msg_leds_close_all[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0,
			.buf = leds_close_all,
			.len = ARRAY_SIZE(leds_close_all),
		},
	};

	return hk32fxxxx_rw_multi_data(chip, msg_leds_close_all, ARRAY_SIZE(msg_leds_close_all));
}

static bool hk32fxxxx_check_chipid(struct hk32fxxxx *chip)
{
	int i = 0;
	int lenth = sizeof(support_chip_id) / sizeof(support_chip_id[0]);
	for(i=0; i<lenth; i++){
		if (chip->chipid == support_chip_id[i]){
			return true;
		}
	}
	return false;
}

static bool hk32fxxxx_init_fw_name(struct hk32fxxxx *chip)
{
	int i = 0;
	int length = sizeof(two_supplies_list) / sizeof(two_supplies_list[0]);
	char *suffix = ".bin";
	char new_fw_name[64] = {0};
	char *dot_bin_pos = NULL;

	if (chip->sub_supplies) {
		if (chip->chipid == chip->sub_supplies)
		{
			dot_bin_pos = strstr(chip->fw_bin_name, suffix);
			if (dot_bin_pos) {
				snprintf(new_fw_name, sizeof(new_fw_name), "%.*s_2%s", (int)(dot_bin_pos - chip->fw_bin_name), chip->fw_bin_name, suffix);
				strncpy(chip->fw_bin_name, new_fw_name, sizeof(chip->fw_bin_name) - 1);
				chip->fw_bin_name[sizeof(chip->fw_bin_name) - 1] = '\0';
				dev_info(chip->dev, "[%s] ss Two supplies ID matched, fw_name changed to: %s\n", __func__, chip->fw_bin_name);
				return true;
			} else {
				dev_err(chip->dev, "[%s] ss Invalid fw_bin_name format: %s\n", __func__, chip->fw_bin_name);
				return false;
			}
		}
	}

	for (i = 0; i < length; i++) {
		if (chip->chipid == two_supplies_list[i]) {
			dot_bin_pos = strstr(chip->fw_bin_name, suffix);
			if (dot_bin_pos) {
				snprintf(new_fw_name, sizeof(new_fw_name), "%.*s_2%s", (int)(dot_bin_pos - chip->fw_bin_name), chip->fw_bin_name, suffix);
				strncpy(chip->fw_bin_name, new_fw_name, sizeof(chip->fw_bin_name) - 1);
				chip->fw_bin_name[sizeof(chip->fw_bin_name) - 1] = '\0';
				dev_info(chip->dev, "[%s] Two supplies ID matched, fw_name changed to: %s\n", __func__, chip->fw_bin_name);
				return true;
			} else {
				dev_err(chip->dev, "[%s] Invalid fw_bin_name format: %s\n", __func__, chip->fw_bin_name);
				return false;
			}
		}
	}
	dev_info(chip->dev, "[%s] fw_name: %s\n", __func__, chip->fw_bin_name);
	return false;
}



static int hk32fxxxx_read_chipid(struct hk32fxxxx *chip)
{
	u8 read_buf[1];
	int ret;
	struct i2c_msg msg_leds_read_chipid[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0,
			.buf = leds_read_chipid,
			.len = ARRAY_SIZE(leds_read_chipid),
		},
	};

	struct i2c_msg msg_leds_read_chipid_2nd[] = {
		{
			.addr = chip->i2c->addr,
			.flags = I2C_M_RD,
			.buf = read_buf,
			.len = ARRAY_SIZE(read_buf),
		},
	};

	ret = hk32fxxxx_rw_multi_data(chip, msg_leds_read_chipid, ARRAY_SIZE(msg_leds_read_chipid));
	if (ret < 0)
		return ret;

	msleep(1);
	hk32fxxxx_rw_multi_data(chip, msg_leds_read_chipid_2nd, ARRAY_SIZE(msg_leds_read_chipid_2nd));

	chip->chipid = read_buf[0];
	if (hk32fxxxx_check_chipid(chip)) {
		dev_info(chip->dev, "[%s] chipid read successful:0x%02x!\n", __func__, read_buf[0]);
	} else {
		dev_err(chip->dev, "[%s] chipid is wrong:0x%02x!\n", __func__, read_buf[0]);
		return HK32FXXXX_I2C_FAIL;
	}

	return 0;
}



static int hk32fxxxx_read_program_status(struct hk32fxxxx *chip)
{
	if (!chip || !chip->i2c || !chip->dev) {
		pr_err("%s: Invalid chip, i2c, or dev pointer\n", __func__);
		return -EINVAL;
	}

	u8 read_buf[1] = {0}; /* Initialize to avoid uninitialized value */
	struct i2c_msg msg_leds_read_program_status[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0,
			.buf = leds_fwver_read_program_status,
			.len = ARRAY_SIZE(leds_fwver_read_program_status),
		},
	};

	struct i2c_msg msg_leds_read_program_status_2nd[] = {
		{
			.addr = chip->i2c->addr,
			.flags = I2C_M_RD,
			.buf = read_buf,
			.len = ARRAY_SIZE(read_buf),
		},
	};

	int ret;
	ret = hk32fxxxx_rw_multi_data(chip, msg_leds_read_program_status, ARRAY_SIZE(msg_leds_read_program_status));
	if (ret < 0) {
		dev_err(chip->dev, "%s: Failed to send program status request, ret=%d\n", __func__, ret);
		return ret;
	}

	msleep(1);

	ret = hk32fxxxx_rw_multi_data(chip, msg_leds_read_program_status_2nd, ARRAY_SIZE(msg_leds_read_program_status_2nd));
	if (ret < 0) {
		dev_err(chip->dev, "%s: Failed to read program status, ret=%d\n", __func__, ret);
		return ret;
	}

	dev_info(chip->dev, "[%s]:%d!\n", __func__, read_buf[0]);

	if (read_buf[0] == FWVER_IAP_STATUS) {
		dev_err(chip->dev, "[%s] The program is running in IAP, need to changed!\n", __func__);
		return HK32FXXXX_I2C_FAIL;
	}

	return 0;
}


/*******************************************************************************
 *
 * hk32fxxxx read fwver id
 *
 ******************************************************************************/
static int hk32fxxxx_check_firmwareid(struct hk32fxxxx *chip)
{
	if (!chip || !chip->i2c || !chip->dev) {
		pr_err("%s: Invalid chip, i2c, or dev pointer\n", __func__);
		return -EINVAL;
	}

	/* Ensure we have valid version data */
	if (!chip->fwver_version[0]) {
		dev_err(chip->dev, "%s: Empty firmware version\n", __func__);
		return -EINVAL;
	}

	u8 read_buf[12] = {0};

	struct i2c_msg msg_leds_read_firmwareid[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0,
			.buf = leds_read_firmwareid_64K,
			.len = ARRAY_SIZE(leds_read_firmwareid_64K),
		},
	};

	struct i2c_msg msg_leds_read_firmwareid_2nd[] = {
		{
			.addr = chip->i2c->addr,
			.flags = I2C_M_RD,
			.buf = read_buf,
			.len = ARRAY_SIZE(read_buf),
		},
	};

	if (hk32fxxxx_check_chipid(chip)) {
		memcpy(msg_leds_read_firmwareid[0].buf, leds_read_firmwareid_64K, sizeof(leds_read_firmwareid_64K));
	}

	int ret;
	ret = hk32fxxxx_rw_multi_data(chip, msg_leds_read_firmwareid, ARRAY_SIZE(msg_leds_read_firmwareid));
	if (ret < 0) {
		dev_err(chip->dev, "%s: Failed to send firmware ID request, ret=%d\n", __func__, ret);
		return ret;
	}

	msleep(1);

	ret = hk32fxxxx_rw_multi_data(chip, msg_leds_read_firmwareid_2nd, ARRAY_SIZE(msg_leds_read_firmwareid_2nd));
	if (ret < 0) {
		dev_err(chip->dev, "%s: Failed to read firmware ID, ret=%d\n", __func__, ret);
		return ret;
	}

	if (memcmp(read_buf, chip->fwver_version, 12) == 0) {
		dev_info(chip->dev, "[%s] firmwareid matches version: %.12s\n", __func__, chip->fwver_version);
	} else {
		dev_err(chip->dev, "[%s] firmwareid does not match version. Read: %.12s, Expected: %.12s\n", __func__, read_buf, chip->fwver_version);
		return HK32FXXXX_I2C_FAIL;
	}
	return 0;
}

/*******************************************************************************
 *
 * hk32fxxxx fw update
 *
 ******************************************************************************/
static int hk32fxxxx_fwver_update_iap(struct hk32fxxxx *chip)
{
	int ret = 0;

	if (!chip) {
		pr_err("%s: Invalid chip pointer\n", __func__);
		return -EINVAL;
	}

	struct i2c_msg msg_leds_fwver_update_iap[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0,
			.buf = leds_fwver_update_iap,
			.len = ARRAY_SIZE(leds_fwver_update_iap),
		},
	};

	ret = hk32fxxxx_rw_multi_data(chip, msg_leds_fwver_update_iap, ARRAY_SIZE(msg_leds_fwver_update_iap));
	if (ret < 0) {
		dev_err(chip->dev, "%s: Failed to enter IAP mode, ret=%d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int hk32fxxxx_fwver_update_userdata(struct hk32fxxxx *chip)
{
	int count = 0;
	unsigned char temp[FWVER_UPDATE_DATA_MAX];
	int NumbOfSingle;
	int NumbOf1K;
	unsigned int McuFlashAddress = MCU_FWVER_ADDRESS;

	struct i2c_msg msg_leds_update_userdata[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0,
		},
	};

	dev_info(chip->dev, "%s:enter\n", __func__);
	NumbOf1K = chip->fwver_length/HK32FXXXX_1K_BYTE;
	NumbOfSingle = chip->fwver_length % HK32FXXXX_1K_BYTE;
	if (hk32fxxxx_check_chipid(chip)) {
		McuFlashAddress = MCU_FWVER_ADDRESS_64K;
	}

	for (count = 0; count < NumbOf1K; count++) {
		if (count * HK32FXXXX_1K_BYTE >= chip->fwver_length) {
			dev_err(chip->dev, "%s: Invalid data offset: %d exceeds length %d\n", 
				__func__, count * HK32FXXXX_1K_BYTE, chip->fwver_length);
			return -EINVAL;
		}

		temp[0] = 0xA3;
		temp[1] = (McuFlashAddress&0xFF000000)>>24;
		temp[2] = (McuFlashAddress&0x00FF0000)>>16;
		temp[3] = (McuFlashAddress&0x0000FF00)>>8;
		temp[4] = (McuFlashAddress&0x000000FF);
		temp[5] = HK32FXXXX_1K_BYTE/256;
		temp[6] = HK32FXXXX_1K_BYTE%256;
		temp[7] = 0xFF;

		if (8 + HK32FXXXX_1K_BYTE > FWVER_UPDATE_DATA_MAX) {
			dev_err(chip->dev, "%s: Data size exceeds buffer size: %d > %d\n", 
				__func__, 8 + HK32FXXXX_1K_BYTE, FWVER_UPDATE_DATA_MAX);
			return -EINVAL;
		}

		memcpy(temp + 8, (chip->hk32fxxxx_leds_userdata + count * HK32FXXXX_1K_BYTE), HK32FXXXX_1K_BYTE);

		msg_leds_update_userdata[0].buf = temp;
		msg_leds_update_userdata[0].len = 8 + HK32FXXXX_1K_BYTE;
		hk32fxxxx_rw_multi_data(chip, msg_leds_update_userdata, ARRAY_SIZE(msg_leds_update_userdata));

		msleep(30);

		McuFlashAddress += HK32FXXXX_1K_BYTE;
	}

	if (NumbOfSingle <= 0 || NumbOfSingle > FWVER_UPDATE_DATA_MAX - 8) {
		dev_err(chip->dev, "%s: Invalid NumbOfSingle: %d\n", __func__, NumbOfSingle);
		return -EINVAL;
	}

	if (count * HK32FXXXX_1K_BYTE >= chip->fwver_length) {
		dev_err(chip->dev, "%s: Invalid data offset for single block: %d exceeds length %d\n", 
			__func__, count * HK32FXXXX_1K_BYTE, chip->fwver_length);
		return -EINVAL;
	}

	temp[1] = (McuFlashAddress&0xFF000000)>>24;
	temp[2] = (McuFlashAddress&0x00FF0000)>>16;
	temp[3] = (McuFlashAddress&0x0000FF00)>>8;
	temp[4] = (McuFlashAddress&0x000000FF);
	temp[5] = NumbOfSingle / 256;
	temp[6] = NumbOfSingle % 256;

	memcpy(temp + 8, (chip->hk32fxxxx_leds_userdata + count * HK32FXXXX_1K_BYTE), NumbOfSingle);

	msg_leds_update_userdata[0].buf = temp;
	msg_leds_update_userdata[0].len = 8 + NumbOfSingle;
	hk32fxxxx_rw_multi_data(chip, msg_leds_update_userdata, ARRAY_SIZE(msg_leds_update_userdata));

	return 0;
}

static int hk32fxxxx_fwver_update_fwver_id(struct hk32fxxxx *chip)
{
	if (!chip || !chip->i2c) {
		pr_err("%s: Invalid chip or i2c pointer\n", __func__);
		return -EINVAL;
	}

	const char *fwver = leds_fwver_update_fwver_id_64K;
	struct i2c_msg msg_leds_fwver_update_fwver_id[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0,
			.buf = leds_fwver_update_fwver_id_64K,
			.len = ARRAY_SIZE(leds_fwver_update_fwver_id_64K),
		},
	};

	/* Ensure we have valid version data */
	if (!chip->fwver_version[0]) {
		dev_err(chip->dev, "%s: Empty firmware version\n", __func__);
		return -EINVAL;
	}

	/* Ensure we don't overflow the buffer */
	if (sizeof(leds_fwver_update_fwver_id_64K) < 12) {
		dev_err(chip->dev, "%s: Buffer too small for version data\n", __func__);
		return -EINVAL;
	}

	memcpy(leds_fwver_update_fwver_id_64K + (sizeof(leds_fwver_update_fwver_id_64K) - 12), chip->fwver_version, 12);

	hk32fxxxx_rw_multi_data(chip, msg_leds_fwver_update_fwver_id, ARRAY_SIZE(msg_leds_fwver_update_fwver_id));

	/* Safely advance pointer and ensure it's within bounds */
	if (sizeof(leds_fwver_update_fwver_id_64K) > 8) {
		fwver = fwver + 8;
		dev_info(chip->dev, "%s: to: %.12s\n", __func__, fwver);
	}
	return 0;
}

static int hk32fxxxx_fwver_update_crc(struct hk32fxxxx *chip)
{
	if (!chip || !chip->i2c || !chip->dev) {
		pr_err("%s: Invalid chip, i2c, or dev pointer\n", __func__);
		return -EINVAL;
	}

	unsigned char temp[4];
	struct i2c_msg msg_leds_fwver_update_crc[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0,
			.buf = leds_fwver_update_crc_64K,
			.len = ARRAY_SIZE(leds_fwver_update_crc_64K),
		},
	};

	temp[0] = chip->fwver_length >> 8;
	temp[1] = chip->fwver_length & 0xff;
	temp[2] = chip->fwver_crc >> 8;
	temp[3] = chip->fwver_crc & 0xff;

	dev_info(chip->dev, "%s:enter\n", __func__);

    memcpy(leds_fwver_update_crc_64K + (sizeof(leds_fwver_update_crc_64K) - 4), temp, sizeof(temp));

	hk32fxxxx_rw_multi_data(chip, msg_leds_fwver_update_crc, ARRAY_SIZE(msg_leds_fwver_update_crc));

	return 0;
}

static int hk32fxxxx_fwver_get_crc_result(struct hk32fxxxx *chip)
{
	if (!chip || !chip->i2c || !chip->dev) {
		pr_err("%s: Invalid chip, i2c, or dev pointer\n", __func__);
		return -EINVAL;
	}

	u8 read_buf[1] = {0}; /* Initialize to avoid uninitialized value */

	if( strstr(chip->fw_bin_name, "6877") == NULL &&
		strstr(chip->fw_bin_name, "6878") == NULL &&
		strstr(chip->fw_bin_name, "6873") == NULL ){//for apt crc check
		hk32fxxxx_fwver_update_crc(chip);
		msleep(200);
	}

	struct i2c_msg msg_leds_fwver_get_crc_result[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0,
			.buf = leds_fwver_get_crc_result,
			.len = ARRAY_SIZE(leds_fwver_get_crc_result),
		},
	};

	struct i2c_msg msg_leds_fwver_get_crc_result_2nd[] = {
		{
			.addr = chip->i2c->addr,
			.flags = I2C_M_RD,
			.buf = read_buf,
			.len = ARRAY_SIZE(read_buf),
		},
	};

	int ret;
	ret = hk32fxxxx_rw_multi_data(chip, msg_leds_fwver_get_crc_result, ARRAY_SIZE(msg_leds_fwver_get_crc_result));
	if (ret < 0) {
		dev_err(chip->dev, "%s: Failed to send CRC result request, ret=%d\n", __func__, ret);
		return ret;
	}

	msleep(50);

	ret = hk32fxxxx_rw_multi_data(chip, msg_leds_fwver_get_crc_result_2nd, ARRAY_SIZE(msg_leds_fwver_get_crc_result_2nd));
	if (ret < 0) {
		dev_err(chip->dev, "%s: Failed to read CRC result, ret=%d\n", __func__, ret);
		return ret;
	}

	dev_info(chip->dev, "%s:get crc result:%d\n", __func__, read_buf[0]);
	if (read_buf[0] > 0)
		return 1;
	else
		return 0;
}


static int hk32fxxxx_fwver_running(struct hk32fxxxx *chip)
{
	if (!chip || !chip->i2c || !chip->dev) {
		pr_err("%s: Invalid chip, i2c, or dev pointer\n", __func__);
		return -EINVAL;
	}

	struct i2c_msg msg_leds_fwver_running[] = {
		{
			.addr = chip->i2c->addr,
			.flags = 0,
			.buf = leds_fwver_running,
			.len = ARRAY_SIZE(leds_fwver_running),
		},
	};

	int ret;
	dev_info(chip->dev, "%s:enter\n", __func__);
	ret = hk32fxxxx_rw_multi_data(chip, msg_leds_fwver_running, ARRAY_SIZE(msg_leds_fwver_running));
	if (ret < 0) {
		dev_err(chip->dev, "%s: Failed to set running mode, ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int hk32fxxxx_fwver_update_process(struct hk32fxxxx *hk32fxxxx)
{
	int ret = 0;

	if (!hk32fxxxx) {
		pr_err("%s: Invalid hk32fxxxx pointer\n", __func__);
		return -EINVAL;
	}

	dev_info(hk32fxxxx->dev, "%s: Starting firmware update process\n", __func__);

	ret = hk32fxxxx_fwver_update_iap(hk32fxxxx);
	if (ret < 0) {
		dev_err(hk32fxxxx->dev, "%s: Failed to enter IAP mode, ret=%d\n", __func__, ret);
		return ret;
	}
	msleep(200);

	ret = hk32fxxxx_fwver_update_userdata(hk32fxxxx);
	if (ret < 0) {
		dev_err(hk32fxxxx->dev, "%s: Failed to update user data, ret=%d\n", __func__, ret);
		//hk32fxxxx_fwver_running(hk32fxxxx);
		return ret;
	}
	msleep(500);

	ret = hk32fxxxx_fwver_update_crc(hk32fxxxx);
	if (ret < 0) {
		dev_err(hk32fxxxx->dev, "%s: Failed to update CRC, ret=%d\n", __func__, ret);
		//hk32fxxxx_fwver_running(hk32fxxxx);
		return ret;
	}
	msleep(200);

	if (hk32fxxxx_fwver_get_crc_result(hk32fxxxx) == 0) {
		dev_err(hk32fxxxx->dev, "%s: CRC test result failed!\n", __func__);
		msleep(100);
		//hk32fxxxx_fwver_running(hk32fxxxx);
		return -EINVAL;
	}
	msleep(200);

	ret = hk32fxxxx_fwver_update_fwver_id(hk32fxxxx);
	if (ret < 0) {
		dev_err(hk32fxxxx->dev, "%s: Failed to update firmware ID, ret=%d\n", __func__, ret);
		//hk32fxxxx_fwver_running(hk32fxxxx);
		return ret;
	}
	msleep(200);

	ret = hk32fxxxx_fwver_running(hk32fxxxx);
	if (ret < 0) {
		dev_err(hk32fxxxx->dev, "%s: Failed to resume running state, ret=%d\n", __func__, ret);
		return ret;
	}
	msleep(500);

	dev_info(hk32fxxxx->dev, "%s: Firmware update process completed successfully\n", __func__);
	return 0;
}

/*fw update work */
static void hk32fxxxx_leds_fwver_update_work(struct work_struct *work)
{
	struct hk32fxxxx *hk32fxxxx = container_of(to_delayed_work(work), struct hk32fxxxx, wait_fw_update_work);
	int ret = 0;
	int retry_times = 3;
	const struct firmware *fw = NULL;

	if (!hk32fxxxx) {
		pr_err("%s: Invalid hk32fxxxx pointer\n", __func__);
		return;
	}

	if (!hk32fxxxx->dev) {
		pr_err("%s: Invalid device pointer\n", __func__);
		return;
	}

	if (hk32fxxxx->fw_bin_name[0] == '\0') {
		dev_err(hk32fxxxx->dev, "%s: Firmware name not set\n", __func__);
		return;
	}

	mutex_lock(&hk32fxxxx->fw_lock);

	dev_info(hk32fxxxx->dev, "%s: Requesting firmware %s\n", __func__, hk32fxxxx->fw_bin_name);
	ret = request_firmware(&fw, hk32fxxxx->fw_bin_name, hk32fxxxx->dev);
	if (ret < 0) {
		dev_err(hk32fxxxx->dev, "%s: Failed to request firmware %s, ret=%d\n", 
			__func__, hk32fxxxx->fw_bin_name, ret);
		goto unlock;
	}

	if (!fw || !fw->data) {
		dev_err(hk32fxxxx->dev, "%s: Invalid firmware data\n", __func__);
		if (fw) {
			release_firmware(fw);
		}
		goto unlock;
	}

	if (fw->size < 16) {
		dev_err(hk32fxxxx->dev, "%s: Firmware size too small: %zu bytes\n", __func__, fw->size);
		release_firmware(fw);
		goto unlock;
	}

	hk32fxxxx->fwver_length = fw->data[0] | (fw->data[1] << 8);
	hk32fxxxx->fwver_crc = fw->data[2] | (fw->data[3] << 8);

	memcpy(hk32fxxxx->fwver_version, &fw->data[4], 12);
	dev_info(hk32fxxxx->dev, "%s: Copied version date: %.12s\n", __func__, hk32fxxxx->fwver_version);

	if (fw->size - 16 != hk32fxxxx->fwver_length) {
		dev_warn(hk32fxxxx->dev, "%s: Firmware header length (%d) doesn't match actual data size (%zu)\n",  __func__, hk32fxxxx->fwver_length, fw->size - 5);
		hk32fxxxx->fwver_length = fw->size - 16;
	}

	if (hk32fxxxx->hk32fxxxx_leds_userdata) {
		kfree(hk32fxxxx->hk32fxxxx_leds_userdata);
		hk32fxxxx->hk32fxxxx_leds_userdata = NULL;
	}

	hk32fxxxx->hk32fxxxx_leds_userdata = kzalloc(fw->size - 16, GFP_KERNEL);
	if (!hk32fxxxx->hk32fxxxx_leds_userdata) {
		dev_err(hk32fxxxx->dev, "%s: Failed to allocate memory for firmware data\n", __func__);
		release_firmware(fw);
		goto unlock;
	}

	memcpy(hk32fxxxx->hk32fxxxx_leds_userdata, fw->data + 16, fw->size - 16);

	release_firmware(fw);
	fw = NULL;

	dev_info(hk32fxxxx->dev, "%s: Firmware %s loaded successfully\n", __func__, hk32fxxxx->fw_bin_name);
	dev_info(hk32fxxxx->dev, "%s: fwver_length: 0x%x (%d)\n", __func__, hk32fxxxx->fwver_length, hk32fxxxx->fwver_length);
	dev_info(hk32fxxxx->dev, "%s: fwver_crc: 0x%x (%d)\n", __func__, hk32fxxxx->fwver_crc, hk32fxxxx->fwver_crc);
	dev_info(hk32fxxxx->dev, "%s: fwver_version: %.12s\n", __func__, hk32fxxxx->fwver_version);

	ret = hk32fxxxx_check_firmwareid(hk32fxxxx);
	if (ret == 0 && (hk32fxxxx_fwver_get_crc_result(hk32fxxxx) == 1)) {
		dev_info(hk32fxxxx->dev, "%s: Firmware ID check passed, no need to update\n", __func__);
		hk32fxxxx_fwver_running(hk32fxxxx);
		msleep(50);//for ready
		goto cleanup;
	}

	ret = hk32fxxxx_read_program_status(hk32fxxxx);
	if (ret < 0) {
		dev_info(hk32fxxxx->dev, "%s: Program status check failed, running firmware\n", __func__);
		//hk32fxxxx_fwver_running(hk32fxxxx);
	}
	msleep(100);
	dev_info(hk32fxxxx->dev, "%s: Starting firmware update process\n", __func__);

	while(retry_times--){
		ret = hk32fxxxx_fwver_update_process(hk32fxxxx);
		if (ret != 0) {
			dev_err(hk32fxxxx->dev, "%s: Firmware update process failed retry, ret=%d\n", __func__, ret);
		} else {
			msleep(100);
			dev_info(hk32fxxxx->dev, "%s: Firmware update completed successfully\n", __func__);
			goto cleanup;
		}
	}

cleanup:
	if (hk32fxxxx->hk32fxxxx_leds_userdata) {
		kfree(hk32fxxxx->hk32fxxxx_leds_userdata);
		hk32fxxxx->hk32fxxxx_leds_userdata = NULL;
	}

unlock:
	mutex_unlock(&hk32fxxxx->fw_lock);
}

/*******************************************************************************
 *
 * hk32fxxxx hardware enable
 *
 ******************************************************************************/
static int hk32fxxxx_hw_enable(struct hk32fxxxx *chip, bool flag)
{
	if (chip && gpio_is_valid(chip->enable_gpio)) {
		if (flag) {
			gpio_set_value_cansleep(chip->enable_gpio, 1);
			dev_err(chip->dev,"%s:set gpio high\n", __func__);
			usleep_range(1000, 1500);
		} else {
			gpio_set_value_cansleep(chip->enable_gpio, 0);
		}
	} else {
		dev_err(chip->dev,"%s:failed\n", __func__);
	}

	return 0;
}

static void hk32fxxxx_effect_cmd_set(struct hk32fxxxx *chip, char *cmd, int len)
{
	struct i2c_msg msg_leds_fwver_effect_mode_select[] = {
        {
            .addr = chip->i2c->addr,
            .flags = 0,
            .buf = cmd,
            .len = len,
        },
    };

    dev_info(chip->dev, "%s: I2C addr=0x%x, cmd_len=%d\n", __func__, chip->i2c->addr, len);
    dev_info(chip->dev, "%s: cmd data=", __func__);
    for (int i = 0; i < len; i++) {
        dev_info(chip->dev, "%s val%d=0x%02x ", __func__, i,  cmd[i]);
    }

    hk32fxxxx_rw_multi_data(chip, msg_leds_fwver_effect_mode_select, ARRAY_SIZE(msg_leds_fwver_effect_mode_select));
}



void hk32fxxxx_write_frame(struct hk32fxxxx *hk32fxxxx, unsigned char * frame){
	struct i2c_msg msg_leds_wakeup[] = {
		{
			.addr = 0, /* Will be set later */
			.flags = 0, //write
			.buf = NULL, /* Will be set later */
			.len = 0,
		},
	};

	if (IS_ERR_OR_NULL(frame)) {
		pr_err("hk32fxxxx_write_frame bad args !!\n");
		return;
	}

	if (!hk32fxxxx || !hk32fxxxx->i2c) {
		pr_err("hk32fxxxx_write_frame invalid hk32fxxxx pointer or i2c pointer\n");
		return;
	}

	/* Set the address and buffer now that we've validated the pointers */
	msg_leds_wakeup[0].addr = hk32fxxxx->i2c->addr;
	msg_leds_wakeup[0].buf = frame;
	msg_leds_wakeup[0].len = hk32fxxxx->frame_size > 0 ? hk32fxxxx->frame_size - 1 : 0;

	hk32fxxxx_rw_multi_data(hk32fxxxx, msg_leds_wakeup, 1);
}

void process_with_one_data(struct hk32fxxxx *hk32fxxxx, struct cmd_struct *tran_led_cmd)
{
	unsigned char reg_cmd[4] = {0};
	int lenth = 4;

	if (!hk32fxxxx || !hk32fxxxx->dev || !tran_led_cmd) {
		pr_err("%s: Invalid parameters\n", __func__);
		return;
	}

	reg_cmd[0] = tran_led_cmd->effect_type;
	reg_cmd[1] = tran_led_cmd->loop;
	reg_cmd[2] = tran_led_cmd->brightness;
	reg_cmd[3] = tran_led_cmd->expand;
	hk32fxxxx_effect_cmd_set(hk32fxxxx, reg_cmd, lenth);
	return;
}

void process_with_two_data(struct hk32fxxxx *hk32fxxxx, struct cmd_struct *tran_led_cmd)
{
	unsigned char reg_cmd[5] = {0};
	int lenth = 5;

	if (!hk32fxxxx || !hk32fxxxx->dev || !tran_led_cmd) {
		pr_err("%s: Invalid parameters\n", __func__);
		return;
	}

	reg_cmd[0] = tran_led_cmd->effect_type;
	reg_cmd[1] = tran_led_cmd->loop;
	reg_cmd[2] = tran_led_cmd->brightness;
	reg_cmd[3] = tran_led_cmd->expand;
	reg_cmd[4] = tran_led_cmd->reserve_1;
	hk32fxxxx_effect_cmd_set(hk32fxxxx, reg_cmd, lenth);
	return;
}

void process_with_three_data(struct hk32fxxxx *hk32fxxxx, struct cmd_struct *tran_led_cmd)
{
	unsigned char reg_cmd[6] = {0};
	int lenth = 6;

	if (!hk32fxxxx || !hk32fxxxx->dev || !tran_led_cmd) {
		pr_err("%s: Invalid parameters\n", __func__);
		return;
	}

	reg_cmd[0] = tran_led_cmd->effect_type;
	reg_cmd[1] = tran_led_cmd->loop;
	reg_cmd[2] = tran_led_cmd->brightness;
	reg_cmd[3] = tran_led_cmd->expand;
	reg_cmd[4] = tran_led_cmd->reserve_1;
	reg_cmd[5] = tran_led_cmd->reserve_2;
	hk32fxxxx_effect_cmd_set(hk32fxxxx, reg_cmd, lenth);
	return;
}

void process_only_effect(struct hk32fxxxx *hk32fxxxx, struct cmd_struct *tran_led_cmd)
{
	unsigned char reg_cmd[3] = {0};
	int lenth = 3;

	if (!hk32fxxxx || !hk32fxxxx->dev || !tran_led_cmd) {
		pr_err("%s: Invalid parameters\n", __func__);
		return;
	}

	dev_info(hk32fxxxx->dev, "%s in\n", __func__);

    reg_cmd[0] = tran_led_cmd->effect_type;
	reg_cmd[1] = tran_led_cmd->loop;
	reg_cmd[2] = tran_led_cmd->brightness;
	hk32fxxxx_effect_cmd_set(hk32fxxxx, reg_cmd, lenth);
	return;
}


void prase_device_expand(struct hk32fxxxx *hk32fxxxx, int32_t *cmd, struct cmd_struct *tran_led_cmd, int size)
{
	unsigned char *frame;
	int i = 0;

	if (!hk32fxxxx || !hk32fxxxx->dev || !cmd || !tran_led_cmd) {
		pr_err("%s: Invalid parameters\n", __func__);
		return;
	}

	if (hk32fxxxx->frame_size <= 1)
		return;

	frame = kzalloc(hk32fxxxx->frame_size - 1, GFP_KERNEL);
	if (!frame) {
		dev_err(hk32fxxxx->dev, "alloc frame failed\n");
		return;
	}

	frame[0] = 0x03; //ic frame type
	for (i = 0; i < hk32fxxxx->frame_size - 2; i++) {
		if (cmd[i+2] < 0) {
			frame[i+1] = 0;
		} else if (cmd[i + 2] > 255) {
			frame[i+1] = 255;
		} else {
			frame[i+1] = (unsigned char)cmd[i + 2];
		}
	}
	hk32fxxxx_write_frame(hk32fxxxx, frame);
	kfree(frame);
	return;
}

static int hk32fxxxx_store_tran_led_cmd(struct tran_led_device *dev, int32_t *cmd, int size)
{
    struct hk32fxxxx *hk32fxxxx = dev->dev.driver_data;
    struct cmd_struct tran_led_cmd;

    //cmd check
    if ((size < TRAN_LED_CMD_LENGTH*4) || 
        (cmd[0] != DRIVER_SOLUTION && cmd[0] != DRIVER_FRAME && cmd[0] != BIN_SOLUTION)) {
        dev_info(hk32fxxxx->dev,"%s %d, Invalid command data size=%d!!!\n", __func__,__LINE__, size);
        //return -EINVAL;
    }

    tran_led_cmd.cmd_type = cmd[0];
    tran_led_cmd.hw_id = cmd[1];
    tran_led_cmd.effect_type = cmd[2];
    tran_led_cmd.loop = cmd[3];
    tran_led_cmd.brightness = cmd[4];
    tran_led_cmd.expand = cmd[5];
    tran_led_cmd.reserve_1 = cmd[6];
    tran_led_cmd.reserve_2 = cmd[7];

    //dev_info(hk32fxxxx->dev,"%s 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", __func__,tran_led_cmd.cmd_type,
        //tran_led_cmd.hw_id, tran_led_cmd.effect_type, tran_led_cmd.loop,tran_led_cmd.brightness,tran_led_cmd.expand,tran_led_cmd.reserve_1,tran_led_cmd.reserve_2);
    hk32fxxxx->latest_cmd = tran_led_cmd.effect_type;
    //hk32fxxxx_hw_enable(hk32fxxxx, true);

    if(tran_led_cmd.cmd_type == TRAN_LED_FRAME_TYPE){// ju zhen
        prase_device_expand(hk32fxxxx, cmd, &tran_led_cmd, size);
        return 0;
    }

    if(tran_led_cmd.effect_type == 0){//close all
        hk32fxxxx_leds_close_all(hk32fxxxx);
    } else if ((tran_led_cmd.effect_type > 0) && (tran_led_cmd.effect_type <= 100)){//only loop bright
        process_only_effect(hk32fxxxx, &tran_led_cmd);
    }else if ((tran_led_cmd.effect_type > 100) && (tran_led_cmd.effect_type <= 150)){//with loop bright xx
        process_with_one_data(hk32fxxxx, &tran_led_cmd);
    }else if ((tran_led_cmd.effect_type > 150) && (tran_led_cmd.effect_type <= 200)){//with loop bright xx xx
        process_with_two_data(hk32fxxxx, &tran_led_cmd);
    }else if ((tran_led_cmd.effect_type > 200) && (tran_led_cmd.effect_type <= 250)){//with loop bright xx xx xx
        process_with_three_data(hk32fxxxx, &tran_led_cmd);
    }

    return 0;
}

int store_tran_led_cmd(struct tran_led_device *dev, const char *buf){
    int num[10] = {0};
    struct cmd_struct tran_led_cmd;
    if (!dev || !buf) {
        pr_err("%s: Invalid dev or buf pointer\n", __func__);
        return -EINVAL;
    }

    struct hk32fxxxx *hk32fxxxx = dev->dev.driver_data;
    if (!hk32fxxxx) {
        pr_err("%s: Invalid driver data\n", __func__);
        return -EINVAL;
    }

    memset(num, 0, sizeof(num));
    if (sscanf((const char *)buf, "%x %x %x %x %x %x", &num[0], &num[1], &num[2], &num[3], &num[4], &num[5]) == 6) {
        dev_err(hk32fxxxx->dev,"%s 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",__func__,num[0], num[1], num[2], num[3], num[4], num[5]);
        //hk32fxxxx_hw_enable(hk32fxxxx, true);
        if(num[1] == 0){
            hk32fxxxx_leds_close_all(hk32fxxxx);
        } else if(num[1] == 9){
            if(num[2] != 0){
                hk32fxxxx_leds_open_all(hk32fxxxx, (unsigned char)num[2]);
            } else {
                hk32fxxxx_leds_open_all(hk32fxxxx, 0x0f);
            }
        } else {
            tran_led_cmd.effect_type = num[0];
            tran_led_cmd.loop = num[1];
            tran_led_cmd.brightness = num[2];
            tran_led_cmd.expand = num[3];
            tran_led_cmd.reserve_1 = num[4];
            tran_led_cmd.reserve_2 = num[5];
        
            if(tran_led_cmd.effect_type == 0){//close all
                hk32fxxxx_leds_close_all(hk32fxxxx);
            } else if ((tran_led_cmd.effect_type > 0) && (tran_led_cmd.effect_type <= 100)){//only loop bright
                process_only_effect(hk32fxxxx, &tran_led_cmd);
            }else if ((tran_led_cmd.effect_type > 100) && (tran_led_cmd.effect_type <= 150)){//with loop bright xx
                process_with_one_data(hk32fxxxx, &tran_led_cmd);
            }else if ((tran_led_cmd.effect_type > 150) && (tran_led_cmd.effect_type <= 200)){//with loop bright xx xx
                process_with_two_data(hk32fxxxx, &tran_led_cmd);
            }else if ((tran_led_cmd.effect_type > 200) && (tran_led_cmd.effect_type <= 250)){//with loop bright xx xx xx
                process_with_three_data(hk32fxxxx, &tran_led_cmd);
            }
        }
    }
    return 0;
}

//         static int hk32fxxxx_show_tran_led_cmd(struct tc_led_device *dev,        char *buf)
static int hk32fxxxx_show_tran_led_cmd(struct tran_led_device *led_dev, int32_t * cmd_data,int size)
{
	struct hk32fxxxx *hk32fxxxx = led_dev->dev.driver_data;
    memcpy(cmd_data, hk32fxxxx->cmd_data, sizeof(hk32fxxxx->cmd_data));
    return 0;
}

static int hk32fxxxx_get_frame_size(struct tran_led_device *led_dev)
{
	struct hk32fxxxx *hk32fxxxx = led_dev->dev.driver_data;
	if(hk32fxxxx){
		return hk32fxxxx->frame_size;
	}

	return 0;
}

static struct attribute *hk32fxxxx_attributes[] = {
	NULL,
};

static struct attribute_group hk32fxxxx_attribute_group = {
	.attrs = hk32fxxxx_attributes
};

/*******************************************************************************
 *
 * parse device tree
 *
 ******************************************************************************/

static int hk32fxxxx_parse_dts(struct device *dev, struct hk32fxxxx *hk32fxxxx,
						struct device_node *np)
{
	int ret = -1;

	//add supply-type by qixuan.qin 20251209 start
	hk32fxxxx->ldo_supply = regulator_get_optional(hk32fxxxx->dev, "avdd");
	if (IS_ERR(hk32fxxxx->ldo_supply)) {
		dev_err(hk32fxxxx->dev,"ldo supply request failed!\n");
		hk32fxxxx->ldo_supply = NULL;
	}
	//add supply-type by qixuan.qin 20251209 end
	//USD:zheyi.bai start
	hk32fxxxx->led_ldo_gpio = of_get_named_gpio(np, "led-ldo-gpio", 0);
	if(hk32fxxxx->led_ldo_gpio < 0) {
		dev_err(hk32fxxxx->dev, "%s: parse led-ldo-gpio failed\n", __func__);
		//return ret;//need check
	}
	//USD:zheyi.bai end

	hk32fxxxx->enable_gpio = of_get_named_gpio(np, "enable-gpio", 0);
	if(hk32fxxxx->enable_gpio < 0) {
		dev_err(hk32fxxxx->dev, "%s: parse enable_gpio failed\n", __func__);
		//return ret;//need check
	}

	ret = of_property_read_u32(np, "hk32fxxxx,max_brightness",
					&hk32fxxxx->max_brightness);
	if (ret < 0) {
		dev_err(hk32fxxxx->dev,
		"%s: parse max-brightness err, ret = %d\n", __func__, ret);
	}

	ret = of_property_read_u32(np, "hk32fxxxx,imax", &hk32fxxxx->imax);
	if (ret < 0) {
		dev_err(hk32fxxxx->dev,
			"%s: parse imax err, ret = %d\n", __func__, ret);
	}

	hk32fxxxx->imax = min(hk32fxxxx->imax,hk32fxxxx->max_brightness);
	dev_err(hk32fxxxx->dev,"%s: imax:%d, max_brightness:%d\n", __func__,hk32fxxxx->imax, hk32fxxxx->max_brightness);

    ret = of_property_read_u32(np, "hk32fxxxx,device-index", &hk32fxxxx->device_index);
    if (ret < 0) {
        dev_info(hk32fxxxx->dev, "%s: no device-index specified, using default 0\n", __func__);
        hk32fxxxx->device_index = 0;
    }

	ret = of_property_read_u32(np, "hk32fxxxx,frame-size", &hk32fxxxx->frame_size);
	if (ret < 0) {
		dev_info(hk32fxxxx->dev, "%s: no hk32fxxxx,frame-size in dts, using default 0 \n", __func__);
		hk32fxxxx->frame_size = 0;
	}
	dev_info(hk32fxxxx->dev, "%s: hk32fxxxx,frame-size = %d\n", __func__, hk32fxxxx->frame_size);

	/*USD: add for X6891 avoid nonwakeup func in mcu by hongwu.zhan 20251009 start*/
	ret = of_property_read_u32(np, "hk32fxxxx,wakeup-flag", &hk32fxxxx->wakeup_flag);
	if (ret < 0) {
		dev_info(hk32fxxxx->dev, "%s: no wakeup-flag specified, using default 1\n", __func__);
		hk32fxxxx->wakeup_flag = 1;
	}
	dev_info(hk32fxxxx->dev, "%s: wakeup_flag:%d\n", __func__, hk32fxxxx->wakeup_flag);
	/*USD: add for X6891 avoid nonwakeup func in mcu by hongwu.zhan 20251009 end*/

	ret = of_property_read_u32(np, "hk32fxxxx,sub-supplies", &hk32fxxxx->sub_supplies);
	if (ret < 0) {
		dev_info(hk32fxxxx->dev, "%s: no sub-supplies specified, using default 0\n", __func__);
		hk32fxxxx->sub_supplies = 0;
	}
	dev_info(hk32fxxxx->dev, "%s: sub_supplies:%d\n", __func__, hk32fxxxx->sub_supplies);
	/*Add i2c pinctrl by junwan.liu 20260126 end*/

	return 0;
}

static int hk32fxxxx_parse_led_cdev(struct hk32fxxxx *hk32fxxxx,
						struct device_node *np)
{
    int ret = -1;
    char led_name[32] = {0};

    if (hk32fxxxx->device_index == 0) {
        snprintf(led_name, sizeof(led_name), "hk32fxxxx_led");
    } else {
        snprintf(led_name, sizeof(led_name), "hk32fxxxx_led_%d", hk32fxxxx->device_index);
    }

    hk32fxxxx->cdev.name = kstrdup(led_name, GFP_KERNEL);
    if (!hk32fxxxx->cdev.name)
        return -ENOMEM;

    ret = led_classdev_register(hk32fxxxx->dev, &hk32fxxxx->cdev);
    if (ret) {
        dev_err(hk32fxxxx->dev, "%s:unable to register led ret=%d\n", __func__, ret);
        kfree(hk32fxxxx->cdev.name);
        return 0;
    }

    ret = sysfs_create_group(&hk32fxxxx->cdev.dev->kobj, &hk32fxxxx_attribute_group);
    if (ret) {
        dev_err(hk32fxxxx->dev, "%s:led sysfs ret=%d\n", __func__, ret);
        goto free_class;
    }

    return 0;

free_class:
    led_classdev_unregister(&hk32fxxxx->cdev);
    return ret;
}

static int hk32fxxxx_parse_fw_name(struct hk32fxxxx *hk32fxxxx,
						struct device_node *np)
{
	int ret;
	const char *fw_name = NULL;

	ret = of_property_read_string(np, "hk32fxxxx,firmware-name", &fw_name);
	if (ret == 0 && fw_name != NULL) {
		snprintf(hk32fxxxx->fw_bin_name, sizeof(hk32fxxxx->fw_bin_name), "%s", fw_name);
		dev_info(hk32fxxxx->dev, "%s: Using firmware name from device tree: %s\n", __func__, hk32fxxxx->fw_bin_name);
		hk32fxxxx_init_fw_name(hk32fxxxx);
		return 0;
	}

	dev_info(hk32fxxxx->dev, "%s: No firmware name in device tree, using chipID based name\n", __func__);
	snprintf(hk32fxxxx->fw_bin_name, sizeof(hk32fxxxx->fw_bin_name), "hk32fxxxx_default.bin");
	dev_info(hk32fxxxx->dev, "%s: Using default firmware name: %s\n", __func__, hk32fxxxx->fw_bin_name);
	hk32fxxxx_init_fw_name(hk32fxxxx);
	return 0;
}

static int hk32fxxxx_store_tran_led_check(struct tran_led_device *dev, const char *cmd)
{
	int num[10];
	memset(num, 0, sizeof(num));
	struct hk32fxxxx *hk32fxxxx = dev->dev.driver_data;
    if (!hk32fxxxx) {
        pr_err("Failed to get device data\n");
        return -EINVAL;
    }
	dev_info(hk32fxxxx->dev, "%s in\n", __func__);

	if (sscanf(cmd, "%d", &num[0]) == 1) {
		dev_err(hk32fxxxx->dev,"%s %d\n",__func__,num[0]);
		hk32fxxxx->latest_cmd = num[0];
		switch(num[0]){//type
			case 1:
				hk32fxxxx_factory_test(hk32fxxxx, HK32FXXXX_FACTORY_SHORT);
				break;
			case 2:
				hk32fxxxx_factory_test(hk32fxxxx, HK32FXXXX_FACTORY_OPEN);
				break;
			default:
				dev_err(hk32fxxxx->dev,"%s(no para) %d\n", __func__,__LINE__);
				break;
		}
	}
	return 0;
}

static int hk32fxxxx_show_tran_led_check(struct tran_led_device *led_dev,char *buf)
{
	struct hk32fxxxx *hk32fxxxx = led_dev->dev.driver_data;
    if (!hk32fxxxx) {
        pr_err("Failed to get device data\n");
        return -EINVAL;
    }
	dev_info(hk32fxxxx->dev, "%s in\n", __func__);

	if (hk32fxxxx_read_factory_result(hk32fxxxx) < 0) {
		return -EINVAL;
	} else {
		return sprintf(buf, "%d\n",  hk32fxxxx->test_result);
    }
}

static const struct tran_led_ops leds_hk32fxxxx_ops = {
	/* app & hal api */
	//.get_chip_type 		= aw2023_get_chip_type,
	.set_tran_led_cmd 	= hk32fxxxx_store_tran_led_cmd,
	.get_tran_led_cmd 	= hk32fxxxx_show_tran_led_cmd,

	/* midtest mode api */
	.store_tran_led_cmd = store_tran_led_cmd,
	//.show_tran_led_cmd 	= aw2023_show_tran_led_cmd,
	.get_frame_size = hk32fxxxx_get_frame_size,
	.store_tran_led_check 	= hk32fxxxx_store_tran_led_check,
	.show_tran_led_check 	= hk32fxxxx_show_tran_led_check,
};


static void hk32fxxxx_i2c_driver_ability_init(struct hk32fxxxx *hk32fxxxx)
{
#if 0
    int ret = 0;

    hk32fxxxx->pinctrl = devm_pinctrl_get(hk32fxxxx->dev);
    if (IS_ERR(hk32fxxxx->pinctrl)) {
        dev_err(hk32fxxxx->dev, "%s: Failed to get pinctrl\n", __func__);
        return;
    }

    ret = pinctrl_select_state(hk32fxxxx->pinctrl, pinctrl_lookup_state(hk32fxxxx->pinctrl, "default"));
    if (ret) {
        dev_err(hk32fxxxx->dev, "%s: Failed to select default pinctrl state\n", __func__);
        return;
    }
#endif
}


/*******************************************************************************
 *
 * i2c driver probe
 *
 ******************************************************************************/
static int hk32fxxxx_i2c_probe(struct i2c_client *i2c)
{
	struct hk32fxxxx *hk32fxxxx;
	struct device_node *np = i2c->dev.of_node;
	int ret;
    char dev_name[32] = {0};

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		pr_err("check_functionality failed\n");
		return -EIO;
	}

	hk32fxxxx = devm_kzalloc(&i2c->dev, sizeof(struct hk32fxxxx), GFP_KERNEL);
	if (hk32fxxxx == NULL)
		return -ENOMEM;

	hk32fxxxx->dev = &i2c->dev;
	hk32fxxxx->i2c = i2c;
	i2c_set_clientdata(i2c, hk32fxxxx);
	mutex_init(&hk32fxxxx->i2c_lock);
	mutex_init(&hk32fxxxx->fw_lock);

	dev_info(hk32fxxxx->dev, "%s:enter\n", __func__);
	hk32fxxxx_i2c_driver_ability_init(hk32fxxxx);

	if (np) {
		ret = hk32fxxxx_parse_dts(&i2c->dev, hk32fxxxx, np);
		if (ret) {
			pr_err("failed to parse device tree node\n");
			goto err_parse_dt;
		}
	}

    if (hk32fxxxx->device_index == 0) {
        snprintf(dev_name, sizeof(dev_name), "primary_led");
    } else {
        snprintf(dev_name, sizeof(dev_name), "second_led");
    }

	//USD:zheyi.bai start
	if (gpio_is_valid(hk32fxxxx->led_ldo_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, hk32fxxxx->led_ldo_gpio,
				GPIOF_OUT_INIT_HIGH, "hk32fxxxx_ldo");
		if (ret < 0) {
			pr_err("led_ldo_gpio request failed\n");
			//goto err_gpio_request;
		}
	}
	//USD:zheyi.bai end

	if (gpio_is_valid(hk32fxxxx->enable_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, hk32fxxxx->enable_gpio,
				GPIOF_OUT_INIT_HIGH, "hk32fxxxx_en");
		if (ret < 0) {
			pr_err("enable gpio request failed\n");
			//goto err_gpio_request;
		}
	}
	//USD:add for 4 channl Tran LDO supply by qixuan.qin start
	if (hk32fxxxx->ldo_supply != NULL) {
		ret = regulator_enable(hk32fxxxx->ldo_supply);
		if (ret) {
			pr_err("ldo enable failed\n");
		}
		ret = regulator_get_voltage(hk32fxxxx->ldo_supply);
		if (ret >= 0) {
			pr_info("ldo voltage is %d\n", ret);
		} else {
			pr_err("ldo get voltage failed\n");
		}
	} else {
		pr_err("ldo supply is NULL\n");
	}
	//USD:add for 4 channl Tran LDO supply by qixuan.qin end

	/* hardware enable */
	hk32fxxxx_hw_enable(hk32fxxxx, true);

	msleep(200);
	ret = hk32fxxxx_read_chipid(hk32fxxxx);
	if (ret < 0) {
		pr_err("hk32fxxxx_read_chipid failed ret=%d\n", ret);
		goto err_id;
	}

	dev_set_drvdata(&i2c->dev, hk32fxxxx);

	ret = hk32fxxxx_parse_led_cdev(hk32fxxxx, np);
	if (ret < 0) {
		pr_err("error creating led class dev\n");
		goto err_sysfs;
	}

	ret = hk32fxxxx_parse_fw_name(hk32fxxxx, np);
	if(ret < 0) {
		pr_err("error hk32fxxxx_parse_fw_name\n");
		goto err_parse_dt;
	}

	INIT_DELAYED_WORK(&hk32fxxxx->wait_fw_update_work, hk32fxxxx_leds_fwver_update_work);

    hk32fxxxx->led_dev = tran_led_device_register(dev_name, hk32fxxxx->dev, hk32fxxxx, &leds_hk32fxxxx_ops, NULL);
    if (IS_ERR_OR_NULL(hk32fxxxx->led_dev)) {
        pr_err("%s : register tran led failed   !!!!!\n", __func__);
    }

    schedule_delayed_work(&hk32fxxxx->wait_fw_update_work, (100 + 2000 * hk32fxxxx->device_index));

	msleep(100);

	dev_err(hk32fxxxx->dev, "%s:probe completed successful\n", __func__);
	return 0;

err_sysfs:
//err_gpio_request:
err_parse_dt:
err_id:
	devm_kfree(&i2c->dev, hk32fxxxx);
	hk32fxxxx = NULL;
	return ret;
}

/*******************************************************************************
 *
 * i2c driver remove
 *
 ******************************************************************************/

static void hk32fxxxx_i2c_remove(struct i2c_client *i2c)
{
	struct hk32fxxxx *hk32fxxxx = i2c_get_clientdata(i2c);

	pr_info("enter %s\n", __func__);

	if (!hk32fxxxx) {
		pr_err("%s: Invalid hk32fxxxx pointer\n", __func__);
		return;
	}

	cancel_delayed_work_sync(&hk32fxxxx->wait_fw_update_work);

	if (hk32fxxxx->cdev.dev) {
		sysfs_remove_group(&hk32fxxxx->cdev.dev->kobj, &hk32fxxxx_attribute_group);
	}

	led_classdev_unregister(&hk32fxxxx->cdev);

	if (hk32fxxxx->cdev.name) {
		kfree(hk32fxxxx->cdev.name);
		hk32fxxxx->cdev.name = NULL;
	}

	if (hk32fxxxx->led_dev) {
		tran_led_device_unregister(hk32fxxxx->led_dev);
		hk32fxxxx->led_dev = NULL;
	}

	if (hk32fxxxx->hk32fxxxx_leds_userdata) {
		kfree(hk32fxxxx->hk32fxxxx_leds_userdata);
		hk32fxxxx->hk32fxxxx_leds_userdata = NULL;
	}

	if (hk32fxxxx->ldo_supply != NULL) {
		regulator_disable(hk32fxxxx->ldo_supply);
		regulator_put(hk32fxxxx->ldo_supply);
		hk32fxxxx->ldo_supply = NULL;
	} else {
		pr_err("ldo supply is NULL\n");
	}

	hk32fxxxx_hw_enable(hk32fxxxx, false);

	devm_kfree(&i2c->dev, hk32fxxxx);
}

static void hk32fxxxx_i2c_shutdown(struct i2c_client *i2c)
{
	struct hk32fxxxx *hk32fxxxx = i2c_get_clientdata(i2c);

	pr_info("enter %s\n", __func__);

	if (!hk32fxxxx) {
		pr_err("%s: Invalid hk32fxxxx pointer\n", __func__);
		return;
	}

	if (hk32fxxxx->ldo_supply != NULL) {
		regulator_disable(hk32fxxxx->ldo_supply);
		regulator_put(hk32fxxxx->ldo_supply);
		hk32fxxxx->ldo_supply = NULL;
		pr_info("%s: ldo disable\n", __func__);
	}
	pr_info("%s: shutdown\n", __func__);
}

static const struct i2c_device_id hk32fxxxx_i2c_id[] = {
	{HK32FXXXX_I2C_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, hk32fxxxx_i2c_id);

static const struct of_device_id hk32fxxxx_dt_match[] = {
	{.compatible = "suijing,hk320f0301_led"},
	{.compatible = "suijing,hk320f0301_led1"},
	{.compatible = "suijing,hk320f0301_led2"},
	{},
};

static struct i2c_driver hk32fxxxx_i2c_driver = {
	.driver = {
		.name = HK32FXXXX_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(hk32fxxxx_dt_match),
	},
	.probe = hk32fxxxx_i2c_probe,
	.remove = hk32fxxxx_i2c_remove,
	.shutdown = hk32fxxxx_i2c_shutdown,
	.id_table = hk32fxxxx_i2c_id,
};

static int __init hk32fxxxx_i2c_init(void)
{
	int ret = -1;

	pr_info("hk32fxx driver version %s\n", HK32FXXXX_DRIVER_VERSION);

	ret = i2c_add_driver(&hk32fxxxx_i2c_driver);
	if (ret) {
		pr_err("add hk32fxx driver failed\n");
		return ret;
	}

	return 0;
}

module_init(hk32fxxxx_i2c_init);

static void __exit hk32fxxxx_i2c_exit(void)
{
	i2c_del_driver(&hk32fxxxx_i2c_driver);
}

module_exit(hk32fxxxx_i2c_exit);

MODULE_DESCRIPTION("HK32F0301 LED Driver");
MODULE_LICENSE("GPL v2");
