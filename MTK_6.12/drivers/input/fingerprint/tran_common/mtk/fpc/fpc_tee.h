#ifndef __FPC_TEE_H__
#define __FPC_TEE_H__

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#else
#include <linux/fb.h>
#include <linux/notifier.h>
#endif

#include "tran_fp_common_data_def.h"

void fpc1022_workqueue_notify(struct work_struct *ws);

#endif    /* __FPC_TEE_H__*/
