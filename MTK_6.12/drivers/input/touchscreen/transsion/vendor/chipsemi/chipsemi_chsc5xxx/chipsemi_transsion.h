#ifndef __LINUX_CHIPONE_TRANSSION_H__
#define __LINUX_CHIPONE_TRANSSION_H__

#include "transsion_gesture.h"
#include "transsion_core.h"

int get_tpd_ops(struct tran_ts_controller *controller, struct tran_core_ops **ops);

extern struct tran_ts_controller chipsemi_controller;

#endif

