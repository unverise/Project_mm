#ifndef __HK32F0301_H__
#define __HK32F0301_H__

/*******************************************************************************
 *
 * struct
 *
 ******************************************************************************/
struct hk32fxxxx {
	struct i2c_client *i2c;
	struct device *dev;
	struct led_classdev cdev;
	struct delayed_work wait_fw_update_work;
	struct mutex i2c_lock;
	struct mutex fw_lock;
	struct hrtimer timer;
	uint8_t chipid;
	uint32_t sub_supplies;
	struct regulator *ldo_supply;//for KN6
	int led_ldo_gpio;
	int enable_gpio;
	unsigned int imax;
	unsigned int max_brightness;
	unsigned int latest_cmd;
    struct tran_led_device *led_dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_i2c_clk_gpio;
	struct pinctrl_state *pinctrl_i2c_sda_gpio;

	u8 *fwver_data;
	u32 fwver_length;
	u32 fwver_crc;
	u32 frame_size;
	char fwver_version[12];
	char fw_bin_name[64];

#define TRAN_LED_CMD_LENGTH 8
	int32_t cmd_data[TRAN_LED_CMD_LENGTH];
	unsigned int test_result;
    u8 factory_data[32];
    int device_index;
    u8 *hk32fxxxx_leds_userdata;//fw data
    u32 wakeup_flag;//for X6891 APT
};

struct cmd_struct {
    int32_t cmd_type;
    int32_t hw_id;
    int32_t effect_type;
    int32_t loop;
    int32_t brightness;
    int32_t expand;
    int32_t reserve_1;
    int32_t reserve_2;
};

enum factory_mode {
    HK32FXXXX_FACTORY_NONE = 0,
    HK32FXXXX_FACTORY_SHORT = 1,
    HK32FXXXX_FACTORY_OPEN = 2,
};

#endif
