#ifndef __LINUX_NOVATEK_TRANSSION_H__
#define __LINUX_NOVATEK_TRANSSION_H__

#include "transsion_gesture.h"
#include "transsion_core.h"

int get_tpd_ops(struct tran_ts_controller *controller, struct tran_core_ops **ops);

extern struct tran_ts_controller nt36xxx_controller;

#endif

