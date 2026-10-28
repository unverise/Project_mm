#ifndef _FLASHLIGHT_OCP81378_H
#define _FLASHLIGHT_OCP81378_H

#ifndef OCP81378_DTNAME
#define OCP81378_DTNAME "mediatek,ocp81378"
#endif

#ifdef current
#undef current
#endif

#define OCP81378_NAME "ocp81378"

#define OCP81378_I2C_SLAVE_ADDR 0x63

//TORCH CURRENT min 10mA, step 3.90mA, max 490mA
#define OCP81378_TORCH_MIN_CURRENT     10
#define OCP81378_TORCH_CURRENT_STEP    390
#define OCP81378_TORCH_CURRENT_OFFSET  390
#define OCP81378_TORCH_MAX_CURRENT     500

#define OCP81378_TORCH_CURRENT_TO_REG(a)     \
	((a) < OCP81378_TORCH_MIN_CURRENT ? 0 : \
	((((a) * 100 - OCP81378_TORCH_CURRENT_OFFSET) / OCP81378_TORCH_CURRENT_STEP) & 0x7F))

//FLASH CURRENT min 500mA, step 15.63mA, max 1999mA
#define OCP81378_FLASH_MIN_CURRENT     500
#define OCP81378_FLASH_CURRENT_STEP    1563
#define OCP81378_FLASH_CURRENT_OFFSET  1563
#define OCP81378_FLASH_MAX_CURRENT     1999

#define OCP81378_FLASH_CURRENT_TO_REG(a)     \
	((a) < OCP81378_FLASH_MIN_CURRENT ? 0 : \
	((((a) * 100 - OCP81378_FLASH_CURRENT_OFFSET) / OCP81378_FLASH_CURRENT_STEP) & 0x7F))

#define OCP81378_ENABLE_FLASH_MODE  0b1100  //bit3,4 11:flash 00:Standby
#define OCP81378_ENABLE_TORCH_MODE  0b1000  //bit3,4 10:torch 01:IR-Drive

#if defined(CONFIG_TRAN_FLASHLIGHT_REGISTER_SWITCH)
#define OCP81378_ENABLE_BACK_LED    0b0001  //bit0 enable led1
#define OCP81378_ENABLE_FRONT_LED   0b0010  //bit1 enable led2
#define OCP81378_DISABLE_BACK_LED   0b1110
#define OCP81378_DISABLE_FRONT_LED  0b1101
#else
#define OCP81378_ENABLE_BACK_LED    0b0010
#define OCP81378_ENABLE_FRONT_LED   0b0001
#define OCP81378_DISABLE_BACK_LED   0b1101
#define OCP81378_DISABLE_FRONT_LED  0b1110
#endif

#define OCP81378_BACK_FLASH_MIN_DUTY  8
#define OCP81378_FRONT_FLASH_MIN_DUTY 1

enum OCP81378_REGISTER_ADDR {
	OCP81378_CHIP_ID_REGISTER        =  0x00,
	OCP81378_ENABLE_REGISTER         =  0x01,
	OCP81378_IVFM_REGISTER           =  0x02,
#if defined(CONFIG_TRAN_FLASHLIGHT_REGISTER_SWITCH)
	OCP81378_BACK_FLASH_REGISTER     =  0x03,  //led1 flash
	OCP81378_FRONT_FLASH_REGISTER    =  0x04,  //led2 flash
	OCP81378_BACK_TORCH_REGISTER     =  0x05,  //led1 torch
	OCP81378_FRONT_TORCH_REGISTER    =  0x06,  //led2 torch
#else
	OCP81378_FRONT_FLASH_REGISTER    =  0x03,
	OCP81378_BACK_FLASH_REGISTER     =  0x04,
	OCP81378_FRONT_TORCH_REGISTER    =  0x05,
	OCP81378_BACK_TORCH_REGISTER     =  0x06,
#endif
	OCP81378_BOOST_CONFIG_REGISTER   =  0x07,
	OCP81378_TIMING_CONFIG_REGISTER  =  0x08,
	OCP81378_FLAGS1_REGISTER         =  0x0A,
	OCP81378_FLAGS2_REGISTER         =  0x0B,
	OCP81378_DEVICE_ID_REGISTER      =  0x0C,
	OCP81378_LAST_FLASH_REGISTER     =  0x0D,
};

enum OCP81378_CHANNEL_NUM {
	BACK_CHANNEL     = 0,
	FRONT_CHANNEL    = 1,
	MAX_CHANNEL      = 2,
};

enum OCP81378_STATE {
	OCP81378_DISABLE  = 0,
	OCP81378_ENABLE   = 1,
	OCP81378_TORCH    = 2,
	OCP81378_FLASH    = 3,
	OCP81378_NONE     = 4,
};

struct OCP81378_CHANNEL {
	unsigned char duty;
	unsigned char state;
	unsigned int current;
	struct hrtimer timer;
	unsigned int timeout_ms;
	struct work_struct workqueue;
	unsigned char current_number;
	unsigned const int *current_pointer;//const
};

struct OCP81378_DEVICE {
	int use_count;
	struct mutex lock;
	struct regulator *regulator;
	struct i2c_client *i2c_client;
	struct OCP81378_CHANNEL channel[MAX_CHANNEL];
};

struct ocp81378_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

struct ocp81378_chip_data {
	struct i2c_client *client;
	struct ocp81378_platform_data *pdata;
	struct mutex lock;
};

struct OCP81378_DEVICE ocp81378_device;

static int ocp81378_flash_read(struct i2c_client *client, u8 reg);
static int ocp81378_flash_write(struct i2c_client *client, u8 reg, u8 val);
static int ocp81378_set_current(int channel, int current);
static int ocp81378_enable(int channel);
static int ocp81378_disable(int channel);
static int ocp81378_set_scenario(int scenario);
static int ocp81378_get_hw_fault(void);
static int ocp81378_init(void);
static int ocp81378_uninit(void);
static void ocp81378_work_disable_ch1(struct work_struct *data);
static void ocp81378_work_disable_ch2(struct work_struct *data);
static enum hrtimer_restart ocp81378_timer_func_ch1(struct hrtimer *timer);
static enum hrtimer_restart ocp81378_timer_func_ch2(struct hrtimer *timer);
static int ocp81378_timer_start(int channel, ktime_t ktime);
static int ocp81378_timer_cancel(int channel);

#endif