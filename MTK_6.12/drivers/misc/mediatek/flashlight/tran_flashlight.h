#ifndef _FLASHLIGHT_TRAN_MOS_H
#define _FLASHLIGHT_TRAN_MOS_H

#ifndef TRAN_DTNAME
#define TRAN_DTNAME "mediatek,flashlights_tran"
#endif

#ifdef current
#undef current
#endif

#define TRAN_NAME "flashlights-tran"
#define TRAN_I2C_SLAVE_ADDR 0x63
//TORCH CURRENT min 10mA, step 1.96mA, max 500mA
#define TRAN_TORCH_MIN_CURRENT     4
#define TRAN_AW36515_TORCH_CURRENT_STEP    196
#define TRAN_AW36515_TORCH_CURRENT_OFFSET  98
#define TRAN_TORCH_MAX_CURRENT     500

#define TRAN_AW36515_TORCH_CURRENT_TO_REG(a)     \
	((a) < TRAN_TORCH_MIN_CURRENT ? 0 : \
	((((a) * 100 - TRAN_AW36515_TORCH_CURRENT_OFFSET) / TRAN_AW36515_TORCH_CURRENT_STEP) & 0xFF))

//FLASH CURRENT min 500mA, step 7.83mA, max 1999mA
#define TRAN_FLASH_MIN_CURRENT     500
#define TRAN_AW36515_FLASH_CURRENT_STEP    783
#define TRAN_AW36515_FLASH_CURRENT_OFFSET  391
#define TRAN_FLASH_MAX_CURRENT    1999

#define TRAN_AW36515_FLASH_CURRENT_TO_REG(a)     \
	((a) < TRAN_FLASH_MIN_CURRENT ? 0 : \
	((((a) * 100 - TRAN_AW36515_FLASH_CURRENT_OFFSET) / TRAN_AW36515_FLASH_CURRENT_STEP) & 0xFF))

//TORCH CURRENT min 10mA, step 3.90mA, max 500mA
#define OCP81375_TORCH_MIN_CURRENT     10
#define TRAN_OCP81375_TORCH_CURRENT_STEP    390
#define TRAN_OCP81375_TORCH_CURRENT_OFFSET  390
#define OCP81375_TORCH_MAX_CURRENT     500

#define TRAN_OCP81375_TORCH_CURRENT_TO_REG(a)     \
	((a) < TRAN_TORCH_MIN_CURRENT ? 0 : \
	((((a) * 100 - TRAN_OCP81375_TORCH_CURRENT_OFFSET) / TRAN_OCP81375_TORCH_CURRENT_STEP) & 0x7F))

//FLASH CURRENT min 500mA, step 15.63mA, max 1999mA
#define OCP81375_FLASH_MIN_CURRENT     500
#define TRAN_OCP81375_FLASH_CURRENT_STEP    1563
#define TRAN_OCP81375_FLASH_CURRENT_OFFSET  1563
#define OCP81375_FLASH_MAX_CURRENT     1999

#define TRAN_OCP81375_FLASH_CURRENT_TO_REG(a)     \
	((a) < TRAN_FLASH_MIN_CURRENT ? 0 : \
	((((a) * 100 - TRAN_OCP81375_FLASH_CURRENT_OFFSET) / TRAN_OCP81375_FLASH_CURRENT_STEP) & 0x7F))

#define OCP81375_ENABLE_FLASH_MODE  0b1100  //bit3,4 11:flash 00:Standby
#define OCP81375_ENABLE_TORCH_MODE  0b1000  //bit3,4 10:torch 01:IR-Drive

#define TRAN_ENABLE_FLASH_MODE_INT  0b1100  //bit3,4 11:flash 00:Standby
#define TRAN_ENABLE_TORCH_MODE_INT  0b1000  //bit3,4 10:torch 01:IR-Drive

#define TRAN_ENABLE_FLASH_MODE_EXT  0b00100000  //bit3,4 11:flash 00:Standby
#define TRAN_ENABLE_TORCH_MODE_EXT  0b00010000  //bit3,4 10:torch 01:IR-Drive

#if defined(CONFIG_TRAN_FLASHLIGHT_REGISTER_SWITCH)
#define TRAN_ENABLE_BACK_LED    0b0001  //bit0 enable led1
#define TRAN_ENABLE_FRONT_LED   0b0010  //bit1 enable led2
#define TRAN_DISABLE_BACK_LED   0b1110
#define TRAN_DISABLE_FRONT_LED  0b1101
#else
#define TRAN_ENABLE_BACK_LED_EXT    0b00000010
#define TRAN_ENABLE_FRONT_LED_EXT   0b00000001
#define TRAN_DISABLE_BACK_LED_EXT   0b00110001
#define TRAN_DISABLE_FRONT_LED_EXT  0b00110010
#endif

#define TRAN_ENABLE_BACK_LED_INT    0b0010
#define TRAN_ENABLE_FRONT_LED_INT   0b0001
#define TRAN_DISABLE_BACK_LED_INT   0b1101
#define TRAN_DISABLE_FRONT_LED_INT  0b1110
//redefine front warm enable/disable val
#if 0
#if defined(CONFIG_TRAN_FLASH_BRATHE_LED_COMMMON)
#define TRAN_ENABLE_BACK_WARM_LED   TRAN_ENABLE_BACK_LED
#define TRAN_DISABLE_BACK_WARM_LED  TRAN_DISABLE_BACK_LED
#else
#define TRAN_ENABLE_BACK_WARM_LED   TRAN_ENABLE_FRONT_LED
#define TRAN_DISABLE_BACK_WARM_LED  TRAN_DISABLE_FRONT_LED
#endif
#endif
#define TRAN_ENABLE_FRONT_WARM_LED  TRAN_ENABLE_BACK_LED
#define TRAN_DISABLE_FRONT_WARM_LED TRAN_DISABLE_BACK_LED

#define TRAN_BACK_FLASH_MIN_DUTY  8
#define TRAN_FRONT_FLASH_MIN_DUTY 1

enum TRAN_REGISTER_ADDR {
	TRAN_CHIP_ID_REGISTER        =  0x00,
	TRAN_ENABLE_REGISTER         =  0x01,
	TRAN_IVFM_REGISTER           =  0x02,
#if defined(CONFIG_TRAN_FLASHLIGHT_REGISTER_SWITCH)
	TRAN_BACK_FLASH_REGISTER     =  0x03, //led1 flash
	TRAN_FRONT_FLASH_REGISTER    =  0x04, //led2 flash
	TRAN_BACK_TORCH_REGISTER     =  0x05, //led1 torch
	TRAN_FRONT_TORCH_REGISTER    =  0x06, //led2 torch
#else
	TRAN_FRONT_FLASH_REGISTER    =  0x03,
	TRAN_BACK_FLASH_REGISTER     =  0x04,
	TRAN_FRONT_TORCH_REGISTER    =  0x05,
	TRAN_BACK_TORCH_REGISTER     =  0x06,
#endif
	TRAN_BOOST_CONFIG_REGISTER   =  0x07,
	TRAN_TIMING_CONFIG_REGISTER  =  0x08,
	TRAN_FLAGS1_REGISTER         =  0x0A,
	TRAN_FLAGS2_REGISTER         =  0x0B,
	TRAN_DEVICE_ID_REGISTER      =  0x0C,
	TRAN_LAST_FLASH_REGISTER     =  0x0D,
};

//redefine back/front warm flash/torch register
#define TRAN_BACK_WARM_FLASH_REGISTER TRAN_FRONT_FLASH_REGISTER
#if 0
#if defined(CONFIG_TRAN_FLASH_BRATHE_LED_COMMMON)
#define TRAN_BACK_WARM_TORCH_REGISTER TRAN_BACK_TORCH_REGISTER
#else
#define TRAN_BACK_WARM_TORCH_REGISTER TRAN_FRONT_TORCH_REGISTER
#endif
#endif
#define TRAN_FRONT_WARM_FLASH_REGISTER TRAN_BACK_FLASH_REGISTER
#define TRAN_FRONT_WARM_TORCH_REGISTER TRAN_BACK_TORCH_REGISTER

//add back/front warm channel
enum TRAN_CHANNEL_NUM {
	BACK_CHANNEL       = 0,
	FRONT_CHANNEL      = 1,
	BACK_WARM_CHANNEL  = 2,
	FRONT_WARM_CHANNEL = 3,
	MAX_CHANNEL        = 4,
};

enum TRAN_STATE {
	TRAN_DISABLE  = 0,
	TRAN_ENABLE   = 1,
	TRAN_TORCH    = 2,
	TRAN_FLASH    = 3,
	TRAN_NONE     = 4,
};

enum FLASH_CUSTOM_STROBE_TYPE_ENUM {
	FLASH_CUSTOM_STROBE_TYPE_INIT      = 0,
	FLASH_CUSTOM_STROBE_TYPE_NORMAL    = 1,
	FLASH_CUSTOM_STROBE_TYPE_COLDCOLOR = 2,
	FLASH_CUSTOM_STROBE_TYPE_WARMCOLOR = 4,
	FLASH_CUSTOM_STROBE_TYPE_PHOTONIC  = 8,
};

struct TRAN_CHANNEL {
	unsigned char duty;
	unsigned char state;
	unsigned int current;
	unsigned int feature;
	struct hrtimer timer;
	unsigned int timeout_ms;
	struct regulator *regulator;
	struct work_struct workqueue;
	unsigned char current_number;
	unsigned const int *current_pointer;//const
};

struct TRAN_DEVICE {
	int use_count;
	struct mutex lock;
	struct regulator *regulator;
	struct regulator *regulator_control;
	struct i2c_client *i2c_client;
	struct TRAN_CHANNEL channel[MAX_CHANNEL];
};

struct tran_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

struct tran_chip_data {
	struct i2c_client *client;
	struct tran_platform_data *pdata;
	struct mutex lock;
};

struct TRAN_DEVICE tran_device;

static int tran_flash_read(struct i2c_client *client, u8 reg);
static int tran_flash_write(struct i2c_client *client, u8 reg, u8 val);
static int tran_set_current(int channel, int current);
static int tran_enable(int channel);
static int tran_disable(int channel);
static int tran_set_scenario(int scenario);
static int tran_get_hw_fault(void);
static int tran_init(void);
static int tran_uninit(void);
static void tran_work_disable_back(struct work_struct *data);
static void tran_work_disable_front(struct work_struct *data);
static void tran_work_disable_back_warm(struct work_struct *data);
static void tran_work_disable_front_warm(struct work_struct *data);
static enum hrtimer_restart tran_timer_func_back(struct hrtimer *timer);
static enum hrtimer_restart tran_timer_func_front(struct hrtimer *timer);
static enum hrtimer_restart tran_timer_func_back_warm(struct hrtimer *timer);
static enum hrtimer_restart tran_timer_func_front_warm(struct hrtimer *timer);
static int tran_timer_start(int channel, ktime_t ktime);
static int tran_timer_cancel(int channel);

#endif