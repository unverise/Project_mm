#ifndef __TRANSSION_GESTURE_H__
#define __TRANSSION_GESTURE_H__

#include "transsion_core.h"

#define GESTURE_LF                0xBB
#define GESTURE_RT                0xAA
#define GESTURE_down            0xAB
#define GESTURE_up                0xBA
#define GESTURE_DC                0xCC
#define GESTURE_OC                0xBE
#define GESTURE_o                0x6F
#define GESTURE_w                0x77
#define GESTURE_m                0x6D
#define GESTURE_e                0x65
#define GESTURE_c                0x63
#define GESTURE_s                0x73
#define GESTURE_v                0x76
#define GESTURE_z                0x7A
#define GESTURE_fod                0xFC
//BSP: move aod gesture code ACBAEA-9 by zhengbo.tang 20210222 start
#define GESTURE_singleclick     0x4c
#define GESTURE_FOD_down        0x46
#define GESTURE_FOD_up          0x55
//BSP: move aod gesture code ACBAEA-9 by zhengbo.tang 20210222 end


enum TRAN_GESTURE_TYPE
{
    TRAN_GESTURE_DOUBLE = 0,
    TRAN_GESTURE_MUSIC = 1,
    TRAN_GESTURE_CHAR = 2,
    TRAN_GESTURE_CHAR_ALL = 3,
    TRAN_GESTURE_SPECIAL = 4,
    TRAN_GESTURE_ERR,
};

struct tran_gestre_map_s
{
    char cmd[3];
    bool enable;
    unsigned char code;
    enum TRAN_GESTURE_TYPE gesture_type;
};

extern void gesture_init(struct tran_ts_core *pdata);
extern int get_gesture_status(char *buffer);
extern int set_gesture_status(char *buffer, int len);
extern int get_gesture_code(char *buffer);
#endif
