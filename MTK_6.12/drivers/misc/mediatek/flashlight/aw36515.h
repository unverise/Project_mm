#ifndef _FLASHLIGHT_AW36515_H
#define _FLASHLIGHT_AW36515_H

#ifndef AW36515_DTNAME
#define AW36515_DTNAME "mediatek,aw36515"
#endif

#ifdef current
#undef current
#endif
#define AW36515_I2C_SLAVE_ADDR 0x63
#define AW36515_NAME "aw36515"

//TORCH CURRENT min 10mA, step 1.96mA, max 500mA
#define AW36515_TORCH_MIN_CURRENT     10
#define AW36515_TORCH_CURRENT_STEP    196
#define AW36515_TORCH_CURRENT_OFFSET  98
#define AW36515_TORCH_MAX_CURRENT     500

#define AW36515_TORCH_CURRENT_TO_REG(a)     \
	((a) < AW36515_TORCH_MIN_CURRENT ? 0 : \
	((((a) * 100 - AW36515_TORCH_CURRENT_OFFSET) / AW36515_TORCH_CURRENT_STEP) & 0xFF))

//FLASH CURRENT min 500mA, step 7.83mA, max 1999mA
#define AW36515_FLASH_MIN_CURRENT     500
#define AW36515_FLASH_CURRENT_STEP    783
#define AW36515_FLASH_CURRENT_OFFSET  391
#define AW36515_FLASH_MAX_CURRENT    1999

#define AW36515_FLASH_CURRENT_TO_REG(a)     \
	((a) < AW36515_FLASH_MIN_CURRENT ? 0 : \
	((((a) * 100 - AW36515_FLASH_CURRENT_OFFSET) / AW36515_FLASH_CURRENT_STEP) & 0xFF))

#define AW36515_ENABLE_FLASH_MODE  0b1100  //bit3,4 11:flash 00:Standby
#define AW36515_ENABLE_TORCH_MODE  0b1000  //bit3,4 10:torch 01:IR-Drive

#if defined(CONFIG_TRAN_FLASHLIGHT_REGISTER_SWITCH)
#define AW36515_ENABLE_BACK_LED    0b0001  //bit0 enable led1
#define AW36515_ENABLE_FRONT_LED   0b0010  //bit1 enable led2
#define AW36515_DISABLE_BACK_LED   0b1110
#define AW36515_DISABLE_FRONT_LED  0b1101
#else
#define AW36515_ENABLE_BACK_LED    0b0010
#define AW36515_ENABLE_FRONT_LED   0b0001
#define AW36515_DISABLE_BACK_LED   0b1101
#define AW36515_DISABLE_FRONT_LED  0b1110
#endif

#define AW36515_BACK_FLASH_MIN_DUTY  8
#define AW36515_FRONT_FLASH_MIN_DUTY 1

enum AW36515_REGISTER_ADDR {
	AW36515_CHIP_ID_REGISTER        =  0x00,
	AW36515_ENABLE_REGISTER         =  0x01,
	AW36515_IVFM_REGISTER           =  0x02,
#if defined(CONFIG_TRAN_FLASHLIGHT_REGISTER_SWITCH)
	AW36515_BACK_FLASH_REGISTER     =  0x03, //led1 flash
	AW36515_FRONT_FLASH_REGISTER    =  0x04, //led2 flash
	AW36515_BACK_TORCH_REGISTER     =  0x05, //led1 torch
	AW36515_FRONT_TORCH_REGISTER    =  0x06, //led2 torch
#else
	AW36515_FRONT_FLASH_REGISTER    =  0x03,
	AW36515_BACK_FLASH_REGISTER     =  0x04,
	AW36515_FRONT_TORCH_REGISTER    =  0x05,
	AW36515_BACK_TORCH_REGISTER     =  0x06,
#endif
	AW36515_BOOST_CONFIG_REGISTER   =  0x07,
	AW36515_TIMING_CONFIG_REGISTER  =  0x08,
	AW36515_FLAGS1_REGISTER         =  0x0A,
	AW36515_FLAGS2_REGISTER         =  0x0B,
	AW36515_DEVICE_ID_REGISTER      =  0x0C,
	AW36515_LAST_FLASH_REGISTER     =  0x0D,
};

enum AW36515_CHANNEL_NUM {
	BACK_CHANNEL   = 0,
	FRONT_CHANNEL  = 1,
	MAX_CHANNEL    = 2,
};

enum AW36515_STATE {
	AW36515_DISABLE  = 0,
	AW36515_ENABLE   = 1,
	AW36515_TORCH    = 2,
	AW36515_FLASH    = 3,
	AW36515_NONE     = 4,
};

struct AW36515_CHANNEL {
	unsigned char duty;
	unsigned char state;
	unsigned int current;
	struct hrtimer timer;
	unsigned int timeout_ms;
	struct work_struct workqueue;
	unsigned char current_number;
	unsigned const int *current_pointer;//const
};

struct AW36515_DEVICE {
	int use_count;
	struct mutex lock;
	struct regulator *regulator;
	struct i2c_client *i2c_client;
	struct AW36515_CHANNEL channel[MAX_CHANNEL];
};

struct aw36515_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

struct aw36515_chip_data {
	struct i2c_client *client;
	struct aw36515_platform_data *pdata;
	struct mutex lock;
};

struct AW36515_DEVICE aw36515_device;

static int aw36515_flash_read(struct i2c_client *client, u8 reg);
static int aw36515_flash_write(struct i2c_client *client, u8 reg, u8 val);
static int aw36515_set_current(int channel, int current);
static int aw36515_enable(int channel);
static int aw36515_disable(int channel);
static int aw36515_set_scenario(int scenario);
static int aw36515_get_hw_fault(void);
static int aw36515_init(void);
static int aw36515_uninit(void);
static void aw36515_work_disable_ch1(struct work_struct *data);
static void aw36515_work_disable_ch2(struct work_struct *data);
static enum hrtimer_restart aw36515_timer_func_ch1(struct hrtimer *timer);
static enum hrtimer_restart aw36515_timer_func_ch2(struct hrtimer *timer);
static int aw36515_timer_start(int channel, ktime_t ktime);
static int aw36515_timer_cancel(int channel);

#endif