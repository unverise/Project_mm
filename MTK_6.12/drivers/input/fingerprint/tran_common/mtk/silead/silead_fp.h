#ifndef __SILEAD_FP_H__
#define __SILEAD_FP_H__

#include <linux/printk.h>
#include <linux/proc_fs.h>
#include "tran_fp_common_data_def.h"

#define SIL_FP_DEV_VERSION "v0.3.0"

enum _pwdn_mode_t {
	SIFP_PWDN_NONE = 0,
	SIFP_PWDN_POWEROFF = 1, /* shutdown the avdd power supply */
	SIFP_PWDN_FLASH = 2, /* shutdown avdd 200ms for H/W full reset */
	SIFP_PWDN_MAX,
};

enum _netlink_cmd_t {
	SIFP_NETLINK_START = 0,
	SIFP_NETLINK_IRQ = 1,
	SIFP_NETLINK_SCR_OFF,
	SIFP_NETLINK_SCR_ON,
	SIFP_NETLINK_CONNECT,
	SIFP_NETLINK_DISCONNECT,
	SIFP_NETLINK_TP_TOUCHDOWN,
	SIFP_NETLINK_TP_TOUCHUP,
	SIFP_NETLINK_MAX,
};

enum _fp_nav_key_v_t {
    NAV_KEY_UNKNOWN = 0,
    NAV_KEY_START = 1,
    NAV_KEY_UP = NAV_KEY_START,
    NAV_KEY_DOWN,
    NAV_KEY_RIGHT,
    NAV_KEY_LEFT,
    NAV_KEY_CLICK,
    NAV_KEY_DCLICK,
    NAV_KEY_LONGPRESS,
    NAV_KEY_CLICK_DOWN,
    NAV_KEY_CLICK_UP,
    NAV_KEY_MAX,
    NAV_KEY_WAITMORE = 1000,
};

#define IS_KEY_VALID(k) ((k) > NAV_KEY_UNKNOWN && (k) < NAV_KEY_MAX)

enum _fp_nav_key_f_t {
	NAV_KEY_FLAG_UP = 0,
	NAV_KEY_FLAG_DOWN,
	NAV_KEY_FLAG_CLICK,
};

struct fp_dev_key_t {
	int value;
	uint32_t flag;   /* key down = 1, key up = 0, key down+up = 2 */
};

#define DEVNAME_LEN  16
struct fp_dev_init_t {
	uint8_t mode;
	uint8_t bits;
	uint16_t delay;
	uint32_t speed;
	char dev[DEVNAME_LEN];
	uint8_t nl_id;
	uint8_t dev_id;
	uint16_t reserve;
	uint32_t reg;
	char ta[DEVNAME_LEN];
};

struct fp_dev_debug_t {
	uint8_t cmd[4];
};

struct fp_dev_kmap_t {
	uint16_t k[NAV_KEY_MAX-NAV_KEY_START];  /* Up/Down/Right/Left/Click/Double Click/Longpress */
};

struct fp_dev_touch_info {
    uint8_t touch_state;
    uint8_t area_rate;
    uint16_t x;
    uint16_t y;
};

#define PROC_VND_ID_LEN   32

#define SIFP_IOC_MAGIC	's'

#define SIFP_IOC_RESET        _IOW(SIFP_IOC_MAGIC, 10, u8)

#define SIFP_IOC_ENABLE_IRQ   _IOR(SIFP_IOC_MAGIC,  11, u8)
#define SIFP_IOC_DISABLE_IRQ  _IO(SIFP_IOC_MAGIC,  12)
#define SIFP_IOC_WAIT_IRQ     _IOR(SIFP_IOC_MAGIC, 13, u8)
#define SIFP_IOC_CLR_IRQ      _IO(SIFP_IOC_MAGIC,  14)
//#define SPFP_IOC_EXIT       _IOR(SIFP_IOC_MAGIC, 1, u8)

#define SIFP_IOC_KEY_EVENT    _IOW(SIFP_IOC_MAGIC, 15, struct fp_dev_key_t)
#define SIFP_IOC_INIT         _IOR(SIFP_IOC_MAGIC, 16, struct fp_dev_init_t)
#define SIFP_IOC_DEINIT       _IO(SIFP_IOC_MAGIC,  17)
#define SIFP_IOC_IRQ_STATUS   _IOR(SIFP_IOC_MAGIC, 18, u8)
#define SIFP_IOC_DEBUG        _IOR(SIFP_IOC_MAGIC, 19, struct fp_dev_debug_t)
#define SIFP_IOC_SCR_STATUS   _IOR(SIFP_IOC_MAGIC, 20, u8)
#define SIFP_IOC_GET_VER      _IOR(SIFP_IOC_MAGIC, 21, char[10])
#define SIFP_IOC_SET_KMAP     _IOW(SIFP_IOC_MAGIC, 22, uint16_t[7])
#define SIFP_IOC_ACQ_SPI      _IO(SIFP_IOC_MAGIC,  23)
#define SIFP_IOC_RLS_SPI      _IO(SIFP_IOC_MAGIC,  24)
#define SIFP_IOC_PKG_SIZE     _IOR(SIFP_IOC_MAGIC, 25, u8)
#define SIFP_IOC_DBG_LEVEL    _IOWR(SIFP_IOC_MAGIC,26, u8)
#define SIFP_IOC_WAKELOCK     _IOW(SIFP_IOC_MAGIC, 27, u8)
#define SIFP_IOC_PWDN         _IOW(SIFP_IOC_MAGIC, 28, u8)
#define SIFP_IOC_PROC_NODE    _IOW(SIFP_IOC_MAGIC, 29, char[PROC_VND_ID_LEN])
#define SIFP_IOC_SET_FEATURE  _IOW(SIFP_IOC_MAGIC, 30, u8)

#define FEATURE_FLASH_CS      0x01

#define RESET_TIME            2	/* Default chip reset wait time(ms) */
#define RESET_TIME_MULTIPLE   1 /* Multiple for reset time multiple*wait_time */
#define SIFP_NETLINK_ROUTE    30
#define NL_MSG_LEN            16

//#define PROC_DIR		"fp"      /* if defined, create node under /proc/fp/xxx */
#define PROC_NODE		"fp_id"   /* proc node name */

#define BSP_SIL_NETLINK

#define DEVICE "/dev/spidev0.0"
  //#define BSP_SIL_IRQ_CONFIRM
#define PKG_SIZE 1
  //#define BSP_SIL_DYNAMIC_SPI
#ifndef CONFIG_SILEAD_FP_PLATFORM
#define BSP_SIL_CTRL_SPI
#endif /* !CONFIG_SILEAD_FP_PLATFORM */

#define SIL_EVENT_BLANK         FB_EVENT_BLANK
#define SIL_EVENT_UNBLANK       FB_BLANK_UNBLANK
#define SIL_EVENT_POWERDOWN     FB_BLANK_POWERDOWN
#define SIL_REGISTER_CLIENT(a)  do{ fb_register_client(a); }while(0)

typedef enum {
    ERR_LOG=0,
    DBG_LOG,
    INFO_LOG,
    ALL_LOG,
} fp_debug_level_t;

#define LOG_TAG "[+silead_fp-] "
#define LOG_MSG_DEBUG(level, fmt, args...) do { \
			if (sil_debug_level >= level) {\
				pr_warn(LOG_TAG fmt, ##args); \
			} \
		} while (0)
int silfp_proc_create_node(struct tran_fp_data *tran_fp_dev);
int silfp_keyevent(struct tran_fp_data     *tran_fp_datap, struct fp_dev_key_t *pkey);
void silfp_wakelock_ctl(struct tran_fp_data *tran_fp_datap, unsigned char lock);
int slf_write_reg(void *chip_conf, uint32_t reg, uint32_t val);
#endif /* __SILEAD_FP_H__ */
