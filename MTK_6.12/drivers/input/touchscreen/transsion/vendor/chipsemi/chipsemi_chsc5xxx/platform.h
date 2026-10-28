#ifndef __PLATFORM_H__
#define __PLATFORM_H__
#include<linux/device.h>

#define SEMI_TOUCH_5562                         0X0700
#define SEMI_TOUCH_5560                         0X0702

#define SEMI_TOUCH_IC                           SEMI_TOUCH_5560

#define SOC_PLATFORM_MTK                        0x0100
#define SOC_PLATFROM_QUAL                       0x0200
#define SOC_PLATFORM_SPRD                       0x0300
#define SOC_PLATFORM_SELECT                     SOC_PLATFROM_QUAL

#define MULTI_PROTOCOL_TYPE_A                   0
#define MULTI_PROTOCOL_TYPE_B                   1
#define MULTI_PROTOCOL_TYPE                     MULTI_PROTOCOL_TYPE_B

#define HAL_INTERFACE_I2C                       0
#define HAL_INTERFACE_SPI                       1
/*bsp:modify for transsion core by longtao.ran 20240321 start*/
#define HAL_INTERFACE_TYPE                      HAL_INTERFACE_SPI

#define SEMI_TOUCH_PROXIMITY_OPEN               0
#define SEMI_TOUCH_ESD_CHECK_OPEN               1
#define SEMI_TOUCH_GESTURE_OPEN                 1
/*bsp:modify for transsion core by longtao.ran 20240321 end*/
#define SEMI_TOUCH_GLOVE_OPEN                   0
#define SEMI_TOUCH_WET_FINGER_OPEN              0
#define SEMI_TOUCH_H_V_SW                       1
#define SEMI_TOUCH_APK_NODE_EN                  1
#define SEMI_TOUCH_ONLINE_UPDATE_EN             0
#define SEMI_TOUCH_BOOTUP_UPDATE_EN             0
#define SEMI_TOUCH_FW_UPDATE_EN                 1
#define SEMI_TOUCH_FACTORY_TEST_EN              1
#define SEMI_TOUCH_CHARGER_EN                   1
#define SEMI_TOUCH_GAME_MODE_EN                 1
#define SEMI_TOUCH_GAME_OP_MODE_EN              1
#define SEMI_TOUCH_GAME_INHIBIT_EN              1
#define SEMI_TOUCH_HIGH_SR_MODE_EN              1
#define SEMI_TOUCH_SUPER_RESOLUTION_EN          1
#define SEMI_TOUCH_PALM_MODE_EN                 1
#define SEMI_TOUCH_EAAT_MODE_EN                 1
#define SEMI_TOUCH_FINGERPRINT                  1
#define SEMI_TOUCH_REGION_CTRL_EN               1
#define SEMI_TOUCH_THP_DRIVER_EN                1


#define SEMI_TOUCH_DMA_TRANSFER                 0
#define SEMI_TOUCH_IRQ_VAR_QUEUE                0
#define SEMI_TOUCH_SUSPEND_BY_TPCMD             1
#define SEMI_TOUCH_VKEY_MAPPING                 1
#define SEMI_TOUCH_MAX_POINTS                   10
#define SEMI_TOUCH_SOLUTION_X                   1080
#define SEMI_TOUCH_SOLUTION_Y                   2436
#define CHSC_DEVICE_NAME                        "mtk-tpd"
#define CHSC_AUTO_UPDATE_PACKET_BIN             "/sdcard/chsc_auto_update_packet.bin"

#define TYPE_OF_IC(X)                           ((X) & 0xff00)
#define MAX_VKEY_NUMBER                         5
#define SEMI_TOUCH_KEY_EVT                      {KEY_MENU, KEY_HOME, KEY_BACK, KEY_VOLUMEUP, KEY_VOLUMEDOWN}

#if SEMI_TOUCH_THP_DRIVER_EN
#define SPI_DEFAULT_SPEED                       8000000 //  spi ,4000000   ,8000000  12000000
#else
#define SPI_MAX_SPEED_HZ                        8000000 //  spi ,4000000   ,8000000  12000000
#endif

#define MULTIPLE_SUPER_RESOLUTION               16
//#define SPI_MAX_SPEED_HZ                        8000000 //  spi ,4000000   ,8000000  12000000
/*USD: add for pocket anti accidental touch by longtao.ran 20240605 start*/
#ifndef KEY_LARGE_AREA
#define KEY_LARGE_AREA      251
#endif
/*bsp:modify for transsion core by longtao.ran 20240321 start*/
#define GESTURE_LEFT                         0x20
#define GESTURE_RIGHT                        0x21
#define GESTURE_UP                           0x22
#define GESTURE_DOWN                         0x23
#define GESTURE_DOUBLECLICK                  0x24
#define GESTURE_SINGLECLICK                  0x25
#define GESTURE_O                            0x30
#define GESTURE_W                            0x31
#define GESTURE_M                            0x32
#define GESTURE_E                            0x33
#define GESTURE_C                            0x34
#define GESTURE_S                            0x46
#define GESTURE_V                            0x54
#define GESTURE_Z                            0x65
#define GESTURE_L                            0x44
#define GESTURE_FPDWN                        0x56
#define GESTURE_FPUP                         0x57
#define GESTURE_FPCONTIUE                    0x58
/*bsp:modify for transsion core by longtao.ran 20240321 end*/
/*******************************************************************************************/
#if SOC_PLATFORM_SELECT == SOC_PLATFORM_MTK
#include "tpd.h"
#define semi_touch_get_int()                GTP_INT_PORT
#define semi_touch_get_rst()                GTP_RST_PORT
#define semi_io_pin_low(pin)                tpd_gpio_output(pin, 0)
#define semi_io_pin_high(pin)               tpd_gpio_output(pin, 1)
//#define semi_io_pin_level(pin)              mt_get_gpio_in(pin)
#define semi_io_direction_out(pin, level)   tpd_gpio_output(pin, level)
#define semi_io_direction_in(pin)           tpd_gpio_as_int(pin)
extern  int semi_touch_get_irq(int rst_pin);
extern  int semi_touch_work_done(void);
extern  int semi_touch_resource_release(void);
extern void semi_touch_suspend_entry(struct device *dev);
extern void semi_touch_resume_entry(struct device *dev);

#elif SOC_PLATFORM_SELECT == SOC_PLATFROM_QUAL
extern int semi_touch_get_int(void);
extern int semi_touch_get_rst(void);
extern int semi_touch_get_vdd(void);
extern int semi_touch_get_vddio(void);
extern int semi_touch_get_info(void);


#define semi_io_pin_low(pin)                gpio_set_value(pin,0)
#define semi_io_pin_high(pin)               gpio_set_value(pin,1)
//#define semi_io_pin_level(pin)              gpio_get_value(pin)
#define semi_io_direction_out(pin, level)   gpio_direction_output(pin, level)
#define semi_io_direction_in(pin)           gpio_direction_input(pin)
extern int semi_touch_get_irq(int rst_pin);
extern int semi_touch_work_done(void);
extern int semi_touch_resource_release(void);
extern int semi_touch_suspend_entry(struct device *dev);
extern int semi_touch_resume_entry(struct device *dev);

#elif SOC_PLATFORM_SELECT == SOC_PLATFORM_SPRD
extern int semi_touch_get_int(void);
extern int semi_touch_get_rst(void);
extern int semi_touch_get_info(void);
#define semi_io_pin_low(pin)                gpio_set_value(pin,0)
#define semi_io_pin_high(pin)               gpio_set_value(pin,1)
//#define semi_io_pin_level(pin)              gpio_get_value(pin)
#define semi_io_direction_out(pin, level)   gpio_direction_output(pin, level)
#define semi_io_direction_in(pin)           gpio_direction_input(pin)
extern int semi_touch_get_irq(int rst_pin);
extern int semi_touch_work_done(void);
extern int semi_touch_resource_release(void);
extern int semi_touch_suspend_entry(struct device *dev);
extern int semi_touch_resume_entry(struct device *dev);
#endif

#if SEMI_TOUCH_PROXIMITY_OPEN
extern int semi_touch_proximity_init(void);
extern bool semi_touch_proximity_report(unsigned char proximity);
extern int semi_touch_proximity_stop(void);
#else
#define semi_touch_proximity_init()           0
#define semi_touch_proximity_report(x)        true
#define semi_touch_proximity_stop()           0
#endif

#endif
