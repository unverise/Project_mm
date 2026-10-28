#ifndef __OXI_FP_DRV_H
#define __OXI_FP_DRV_H

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/compat.h>
#include <linux/types.h>
#include <linux/input.h>

#define OXIFP_VENDOR_DEF            "oxi"
#define OXIFP_VERSION               "V1.1.1.20180625"
#define OXIFP_DEBUG_LEVEL           1

#define OXIFP_RET_ERROR             -1
#define OXIFP_RET_SUCCESS           0
#define OXIFP_IOC_MAGIC             'k'

/* Write the value to be used by the GPIO */
#define OXIFP_IOC_RST_SENSOR      _IO(OXIFP_IOC_MAGIC, 4)
#define OXIFP_IOC_CS_HIGH         _IO(OXIFP_IOC_MAGIC, 6)
#define OXIFP_IOC_CS_LOW          _IO(OXIFP_IOC_MAGIC, 7)
#define OXIFP_IOC_POWER_DOWN      _IO(OXIFP_IOC_MAGIC, 8)
#define OXIFP_IOC_POWER_ON        _IO(OXIFP_IOC_MAGIC, 9)
#define OXIFP_IOC_IS_FINGER_ON    _IOR(OXIFP_IOC_MAGIC, 10, __u8)
#define OXIFP_KEY_REPORT          _IOW(OXIFP_IOC_MAGIC, 12, __u8)
#define OXIFP_WAKEUP_SYSTEM       _IOW(OXIFP_IOC_MAGIC, 13, __u8)
#define OXIFP_IOC_ATTRIBUTE       _IOW(OXIFP_IOC_MAGIC, 14, __u32)
#define OXIFP_SET_VERSION_INFO    _IOW(OXIFP_IOC_MAGIC, 18, __u8)


#include "tran_fp_common_data_def.h"

#endif	/* __OXI_FP_DRV_H*/
