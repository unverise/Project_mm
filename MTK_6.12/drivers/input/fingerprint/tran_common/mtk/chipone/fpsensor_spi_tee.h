#ifndef __FPSENSOR_SPI_TEE_H
#define __FPSENSOR_SPI_TEE_H

#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/notifier.h>

#include "tran_fp_common_data_def.h"

#define FPSENSOR_DEV_NAME           "fpsensor"
#define FPSENSOR_NR_DEVS            1
#define FPSENSOR_IOC_MAGIC    0xf0    //CHIP
/* define commands */
#define FPSENSOR_IOC_INIT                       _IOWR(FPSENSOR_IOC_MAGIC,0,uint32_t)
#define FPSENSOR_IOC_EXIT                       _IOWR(FPSENSOR_IOC_MAGIC,1,uint32_t)
#define FPSENSOR_IOC_RESET                      _IOWR(FPSENSOR_IOC_MAGIC,2,uint32_t)
#define FPSENSOR_IOC_ENABLE_IRQ                 _IOWR(FPSENSOR_IOC_MAGIC,3,uint32_t)
#define FPSENSOR_IOC_DISABLE_IRQ                _IOWR(FPSENSOR_IOC_MAGIC,4,uint32_t)
#define FPSENSOR_IOC_GET_INT_VAL                _IOWR(FPSENSOR_IOC_MAGIC,5,uint32_t)
#define FPSENSOR_IOC_DISABLE_SPI_CLK            _IOWR(FPSENSOR_IOC_MAGIC,6,uint32_t)
#define FPSENSOR_IOC_ENABLE_SPI_CLK             _IOWR(FPSENSOR_IOC_MAGIC,7,uint32_t)
#define FPSENSOR_IOC_ENABLE_POWER               _IOWR(FPSENSOR_IOC_MAGIC,8,uint32_t)
#define FPSENSOR_IOC_DISABLE_POWER              _IOWR(FPSENSOR_IOC_MAGIC,9,uint32_t)
/* fp sensor has change to sleep mode while screen off */
#define FPSENSOR_IOC_ENTER_SLEEP_MODE           _IOWR(FPSENSOR_IOC_MAGIC,11,uint32_t)
#define FPSENSOR_IOC_REMOVE                     _IOWR(FPSENSOR_IOC_MAGIC,12,uint32_t)
#define FPSENSOR_IOC_CANCEL_WAIT                _IOWR(FPSENSOR_IOC_MAGIC,13,uint32_t)
#define FPSENSOR_IOC_GET_FP_STATUS              _IOWR(FPSENSOR_IOC_MAGIC,19,uint32_t)
#define FPSENSOR_IOC_ENABLE_REPORT_BLANKON      _IOWR(FPSENSOR_IOC_MAGIC,21,uint32_t)
#define FPSENSOR_IOC_UPDATE_DRIVER_SN           _IOWR(FPSENSOR_IOC_MAGIC,22,uint32_t)

irqreturn_t fpsensor_irq(int irq, void *handle);
#endif    /* __FPSENSOR_SPI_TEE_H */
