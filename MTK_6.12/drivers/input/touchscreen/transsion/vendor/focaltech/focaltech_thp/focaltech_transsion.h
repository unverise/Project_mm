//USD:add rain touch code by linsheng.liu 20240625 start
#ifndef __LINUX_FOCALTECH_TRANSSION_H__
#define __LINUX_FOCALTECH_TRANSSION_H__

#include "transsion_gesture.h"
#include "transsion_core.h"

int get_tpd_ops(struct tran_ts_controller *controller, struct tran_core_ops **ops);

extern struct tran_ts_controller ft3683g_controller;

/*USD:add for status notify by guoxin.zhu 20250307 start*/
typedef enum {
    FHP_RESUME_MODE = 0, /* thp mode */
    FHP_GESTURE_MODE = 1, /* mcu mode */
    FHP_FOD_DOWN = 2,
    FHP_FOD_UP = 3,
    FHP_SLEEP_MODE = 4, /* sleep mode */

    FHP_ROTAION_DOWN = 5,
    FHP_ROTAION_RIGHT = 6,
    FHP_ROTAION_LEFT = 7,

    FHP_SCENE_NORMAL = 8,
    FHP_SCENE_SV = 9,
    FHP_SCENE_GAME = 10,
    FHP_KEYPAD_ON = 11,
    FHP_KEYPAD_OFF = 12,
    FHP_SCENE_CAMERA = 13,
    FHP_SCENE_POCKET_ON = 14,
    FHP_SCENE_POCKET_OFF = 15,
    FHP_STATUS_MAX
} FHP_STATUS_SWITCH_TRIGGER;

typedef enum {
    FHP_SUSPEND_TYPE = 1,
    FHP_FOD_TYPE = 2,
    FHP_ROTATION_TYPE = 3,
    FHP_SCENE_TYPE = 4,
    FHP_KEYPAD_TYPE = 5,
    FHP_POCKET_TYPE = 6,
} FHP_STATUS_TYPE;
/*USD:add for status notify by guoxin.zhu 20250307 end*/

#endif
//USD:add rain touch code by linsheng.liu 20240625 end
