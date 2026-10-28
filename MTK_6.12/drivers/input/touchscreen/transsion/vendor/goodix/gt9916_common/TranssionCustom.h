#ifndef _TRANSSION_CUSTOM_H
#define _TRANSSION_CUSTOM_H

#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/proc_fs.h>

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <generated/autoconf.h>
#include <linux/kobject.h>
#include <linux/regulator/consumer.h>
#define TranssionCustom

#ifdef TranssionCustom
//gesture mode
typedef enum
{
    DOZE_DISABLED = 0,
    DOZE_ENABLED = 1,
    DOZE_WAKEUP = 2,
}DOZE_T;
//static DOZE_T doze_status = DOZE_DISABLED;
#define FTS_MAGIC_NUMBER        'G'
#define NEGLECT_SIZE_MASK           (~(_IOC_SIZEMASK << _IOC_SIZESHIFT))

#define GESTURE_ENABLE_TOTALLY      _IO(FTS_MAGIC_NUMBER, 1)	// 1
#define GESTURE_DISABLE_TOTALLY     _IO(FTS_MAGIC_NUMBER, 2)
#define GESTURE_ENABLE_PARTLY       _IO(FTS_MAGIC_NUMBER, 3)
#define GESTURE_DISABLE_PARTLY      _IO(FTS_MAGIC_NUMBER, 4)
#define GESTURE_DATA_OBTAIN         (_IOR(FTS_MAGIC_NUMBER, 6, u8) & NEGLECT_SIZE_MASK)
#define GESTURE_DATA_ERASE          _IO(FTS_MAGIC_NUMBER, 7)

#define GESTURE_NODE "gesture_function"
#define GESTURE_MAX_POINT_COUNT    64
//#define KEY_FINGER 195
#pragma pack(1)
typedef struct {
	u8 ic_msg[6];		/*from the first byte */
	u8 gestures[4];
	u8 data[3 + GESTURE_MAX_POINT_COUNT * 4 + 80];	/*80 bytes for extra data */
} st_gesture_data;
#pragma pack()

#define SETBIT(longlong, bit)   (longlong[bit/8] |=  (1 << bit%8))
#define CLEARBIT(longlong, bit) (longlong[bit/8] &=(~(1 << bit%8)))
#define QUERYBIT(longlong, bit) (!!(longlong[bit/8] & (1 << bit%8)))

extern u8 cts_is_all_dead(u8 * longlong, s32 size);
extern void cts_gesture_clear_data(void);
extern s32 cts_setgesturestatus(unsigned int cmd,unsigned long arg);
extern s32 cts_gesture_init_node(void);
extern void cts_gesture_deinit_node(void);
int Cts_GestureHandle(void);

extern int Cts_SetDozeStatus(void);
extern int Cts_GestureIdTranslate(const int gesture_id);

#define GESTURE_LF		        0xBB
#define GESTURE_RT		        0xAA
#define GESTURE_down            0xAB
#define GESTURE_up		        0xBA
#define GESTURE_DC		        0xCC
#define GESTURE_o		        0x6F
#define GESTURE_w		        0x77
#define GESTURE_m		        0x6D
#define GESTURE_e		        0x65
#define GESTURE_c		        0x63
#define GESTURE_s		        0x73
#define GESTURE_v		        0x76
#define GESTURE_z		        0x7A
//BSP: move aod gesture code ACBAEA-9 by zhengbo.tang 20210222 start
#define GESTURE_singleclick         	0x4c
#define GESTURE_AOD_down                0x46
#define GESTURE_AOD_up                  0x55
//BSP: move aod gesture code ACBAEA-9 by zhengbo.tang 20210222 end


#define GESTURE_UP                          0xba
#define GESTURE_C                           0x63
#define GESTURE_O                           0x6f
#define GESTURE_M                           0x6d
#define GESTURE_W                           0x77
#define GESTURE_E                           0x65
#define GESTURE_S                           0x73
#define GESTURE_Z                           0x7a
#define GESTURE_V                           0x76
#define GESTURE_DOUBLECLICK                 0xcc
#define GESTURE_DOWN                        0xab
#define GESTURE_LEFT                        0xbb
#define GESTURE_RIGHT                       0xaa
//BSP: move aod gesture code ACBAEA-9 by zhengbo.tang 20210222 start
#define GESTURE_SINGLECLICK                 0x4c
#define GESTURE_AOD_DOWN                    0x46
#define GESTURE_AOD_UP                      0x55
//BSP: move aod gesture code ACBAEA-9 by zhengbo.tang 20210222 end

#endif
/* BSP: add ACBHBHW-2 for get boot_mode by yun.luo 20220517 start */
struct tag_bootmode {
        u32 size;
        u32 tag;
        u32 bootmode;
        u32 boottype;
};
extern int tran_tp_get_boot_mode(void);
/* BSP: add ACBHBHW-2 for get boot_mode by yun.luo 20220517 end */
/* BSP: add ACBHBHW-2 for tp_vendor/bv show test by yun.luo 20220426 start */
extern void tpd_device_init(void);
extern void tpd_device_exit(void);
/* BSP: add ACBHBHW-2 for tp_vendor/bv show test by yun.luo 20220426 end */
#endif
