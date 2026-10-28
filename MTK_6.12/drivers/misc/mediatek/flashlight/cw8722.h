#ifndef _FLASHLIGHT_CW8722_H
#define _FLASHLIGHT_CW8722_H

#ifndef CW8722_DTNAME
#define CW8722_DTNAME "mediatek,cw8722"
#endif

#ifdef current
#undef current
#endif

#define CW8722_NAME "cw8722"

#define CW8722_I2C_SLAVE_ADDR 0x63

//TORCH CURRENT min 10mA, step 1.96mA, max 500mA
#define CW8722_TORCH_MIN_CURRENT     10
#define CW8722_TORCH_CURRENT_STEP    196
#define CW8722_TORCH_CURRENT_OFFSET  98
#define CW8722_TORCH_MAX_CURRENT     500

#define CW8722_TORCH_CURRENT_TO_REG(a)     \
	((a) < CW8722_TORCH_MIN_CURRENT ? 0 : \
	((((a) * 100 - CW8722_TORCH_CURRENT_OFFSET) / CW8722_TORCH_CURRENT_STEP) & 0xFF))

//FLASH CURRENT min 500mA, step 7.83mA, max 1999mA
#define CW8722_FLASH_MIN_CURRENT     500
#define CW8722_FLASH_CURRENT_STEP    783
#define CW8722_FLASH_CURRENT_OFFSET  391
#define CW8722_FLASH_MAX_CURRENT    1999

#define CW8722_FLASH_CURRENT_TO_REG(a)     \
	((a) < CW8722_FLASH_MIN_CURRENT ? 0 : \
	((((a) * 100 - CW8722_FLASH_CURRENT_OFFSET) / CW8722_FLASH_CURRENT_STEP) & 0xFF))

#define CW8722_ENABLE_FLASH_MODE  0b1100  //bit3,4 11:flash 00:Standby
#define CW8722_ENABLE_TORCH_MODE  0b1000  //bit3,4 10:torch 01:IR-Drive

#if defined(CONFIG_TRAN_FLASHLIGHT_REGISTER_SWITCH)
#define CW8722_ENABLE_BACK_LED    0b0001  //bit0 enable led1
#define CW8722_ENABLE_FRONT_LED   0b0010  //bit1 enable led2
#define CW8722_DISABLE_BACK_LED   0b1110
#define CW8722_DISABLE_FRONT_LED  0b1101
#else
#define CW8722_ENABLE_BACK_LED    0b0010
#define CW8722_ENABLE_FRONT_LED   0b0001
#define CW8722_DISABLE_BACK_LED   0b1101
#define CW8722_DISABLE_FRONT_LED  0b1110
#endif

#define CW8722_BACK_FLASH_MIN_DUTY  8
#define CW8722_FRONT_FLASH_MIN_DUTY 1

enum CW8722_REGISTER_ADDR {
	CW8722_CHIP_ID_REGISTER        =  0x00,
	CW8722_ENABLE_REGISTER         =  0x01,
	CW8722_IVFM_REGISTER           =  0x02,
#if defined(CONFIG_TRAN_FLASHLIGHT_REGISTER_SWITCH)
	CW8722_BACK_FLASH_REGISTER     =  0x03, //led1 flash
	CW8722_FRONT_FLASH_REGISTER    =  0x04, //led2 flash
	CW8722_BACK_TORCH_REGISTER     =  0x05, //led1 torch
	CW8722_FRONT_TORCH_REGISTER    =  0x06, //led2 torch
#else
	CW8722_FRONT_FLASH_REGISTER    =  0x03,
	CW8722_BACK_FLASH_REGISTER     =  0x04,
	CW8722_FRONT_TORCH_REGISTER    =  0x05,
	CW8722_BACK_TORCH_REGISTER     =  0x06,
#endif
	CW8722_BOOST_CONFIG_REGISTER   =  0x07,
	CW8722_TIMING_CONFIG_REGISTER  =  0x08,
	CW8722_FLAGS1_REGISTER         =  0x0A,
	CW8722_FLAGS2_REGISTER         =  0x0B,
	CW8722_DEVICE_ID_REGISTER      =  0x0C,
	CW8722_LAST_FLASH_REGISTER     =  0x0D,
};

enum CW8722_CHANNEL_NUM {
	BACK_CHANNEL   = 0,
	FRONT_CHANNEL  = 1,
	MAX_CHANNEL    = 2,
};

enum CW8722_STATE {
	CW8722_DISABLE  = 0,
	CW8722_ENABLE   = 1,
	CW8722_TORCH    = 2,
	CW8722_FLASH    = 3,
	CW8722_NONE     = 4,
};

struct CW8722_CHANNEL {
	unsigned char duty;
	unsigned char state;
	unsigned int current;
	struct hrtimer timer;
	unsigned int timeout_ms;
	struct work_struct workqueue;
	unsigned char current_number;
	unsigned const int *current_pointer;//const
};

struct CW8722_DEVICE {
	int use_count;
	struct mutex lock;
	struct regulator *regulator;
	struct i2c_client *i2c_client;
	struct CW8722_CHANNEL channel[MAX_CHANNEL];
};

struct cw8722_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

struct cw8722_chip_data {
	struct i2c_client *client;
	struct cw8722_platform_data *pdata;
	struct mutex lock;
};

struct CW8722_DEVICE cw8722_device;

static int cw8722_flash_read(struct i2c_client *client, u8 reg);
static int cw8722_flash_write(struct i2c_client *client, u8 reg, u8 val);
static int cw8722_set_current(int channel, int current);
static int cw8722_enable(int channel);
static int cw8722_disable(int channel);
static int cw8722_set_scenario(int scenario);
static int cw8722_get_hw_fault(void);
static int cw8722_init(void);
static int cw8722_uninit(void);
static void cw8722_work_disable_ch1(struct work_struct *data);
static void cw8722_work_disable_ch2(struct work_struct *data);
static enum hrtimer_restart cw8722_timer_func_ch1(struct hrtimer *timer);
static enum hrtimer_restart cw8722_timer_func_ch2(struct hrtimer *timer);
static int cw8722_timer_start(int channel, ktime_t ktime);
static int cw8722_timer_cancel(int channel);

#endif