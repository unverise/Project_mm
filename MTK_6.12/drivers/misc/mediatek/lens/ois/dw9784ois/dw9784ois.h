
#ifndef __DW9784OIS_H__
#define __DW9784OIS_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/pinctrl/consumer.h>
#include <linux/firmware.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#define kal_bool bool
#define KAL_FALSE 0
#define KAL_TRUE 1

// ----- Gyro Offset Calibration Message ----------------------
#define GYRO_OFFSET_CAL_OK						0x00
#define GYRO_CAL_TIME_OVER						0xFF

#define X_GYRO_OFFSET_SPEC__NG				        0x0001
#define X_GYRO_OFFSET_SPEC__ABNORMAL				0x0010
#define Y_GYRO_OFFSET_SPEC__NG				        0x0002
#define Y_GYRO_OFFSET_SPEC__ABNORMAL				0x0020

// ----- OIS Mode Select ----------------------------
#define eAction_OIS_ON_SERVO_OFF				0
#define eAction_OIS_OFF_SERVO_ON				1
#define eAction_OIS_OFF_SERVO_OFF				2
#define eAction_STANDBY				            3

/* platform data */
struct dw9784_platform_data {
	u8 dw9784_pin_enable;
	u8 pam_sync_pin_enable;
	u8 thermal_comp_mode_enable;
	u8 dw9784_pin_disable;
	u8 vout_mode_enable;
};

/* dw9784 chip data */
struct dw9784_chip_data {
	struct miscdevice misc;
	struct i2c_client *client;
	struct dw9784_platform_data *pdata;
	struct mutex lock;
	u8 last_flag;
	u8 no_pdata;
};

#define DW9784_DATA_NUM 		16
#define DW9784_DATA_BIT 		36
#define ONE_SB_No_BIT 		    4
#define OISPLT_DATA_NUM         16

#define BUFFER_ARRAY_SIZE 6
#define CHUNK_SIZE 16

struct LensPos{
    signed short i4x;
    signed short i4y;
    signed short i4z;
};

struct DW9784PosInfo{
    unsigned long long offsettimestamps[OISPLT_DATA_NUM];
    struct LensPos lens_pos[DW9784_DATA_NUM];
};

struct DW9784TransmitBuffer{
    uint16_t latestOISTimeStampH_high; 
    uint16_t latestOISTimeStampH_low;  
    uint16_t latestOISTimeStampL_high;
    uint16_t latestOISTimeStampL_low;  

    uint16_t positionDataX[16];
    uint16_t positionDataY[16]; 
};

struct DW9784TransmitBufferData {
    struct DW9784TransmitBuffer buffer;
    unsigned int flagnum;
};

struct DW9784TransmitBufferData bufferArray[BUFFER_ARRAY_SIZE];

struct IODW9784PInfo{
    int size;
    struct DW9784PosInfo *pInfo;
};

static int RamRead32A_DW9784OIS(unsigned short a_u4Addr, unsigned short *a_puBuff);
static int RamReadBulkDW9784OIS(unsigned short start_addr, unsigned short *buffer, int count);
static int RamWrite32A_DW9784OIS(unsigned short RamAddr, unsigned short RamData, unsigned short delayTime);

//ois interface
static void ChipInfo(void);
static int  CalSave(void);
static int  dw9784_reset(void);
static void getGyroData(void);
// static void getPosData(void);
static int  OisModeSelect(int nMode);
static int  GyroOfsCalibration(void);
static unsigned short GyroOffsetStatus(void);

//ioctl
#define DW9784_POWERON         _IO('p',  0)
#define DW9784_RESUMEOIS       _IO('p',  1)
#define DW9784_SUSPENDOIS      _IO('p',  2)
#define DW9784_CLOSE           _IO('p',  3)
#define DW9784_SET_OISMODE     _IOW('p', 4, unsigned int)
#define DW9784_GET_STATES      _IOR('p', 5, unsigned int)
#define DW9784_GET_POS_INFO    _IOR('p', 6, struct IODW9784PInfo)
#define DW9784_GYRO_CALI       _IOR('p', 7, unsigned int)
#define DW9784_FW_UPDATE       _IOR('p', 8, unsigned int)
#define DW9784_ATA_TEST        _IO('p',  9)
#define DW9784_GYRO_TEST       _IO('p',  10)
#define OISPLT_SET_PWM         _IOW('p', 11, unsigned int)
#define OISPLT_SET_WORKMODE    _IOW('p', 12, unsigned int)
#endif