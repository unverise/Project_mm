#ifndef __LINUX_FOCALTECH_TRANSSION_H__
#define __LINUX_FOCALTECH_TRANSSION_H__

#include "transsion_gesture.h"
#include "transsion_core.h"
#include "focaltech_tran_thp.h"

int get_tpd_ops(struct tran_ts_controller *controller, struct tran_core_ops **ops);

extern struct tran_ts_controller ft3683g_controller;

#endif
