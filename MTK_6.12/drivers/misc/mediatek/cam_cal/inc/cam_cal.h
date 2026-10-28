/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CAM_CAL_H
#define _CAM_CAL_H

#include <linux/ioctl.h>
#ifdef CONFIG_COMPAT
/*64 bit*/
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#define CAM_CALAGIC 'i'
/*IOCTRL(inode * ,file * ,cmd ,arg )*/
/*S means "set through a ptr"*/
/*T means "tell by a arg value"*/
/*G means "get by a ptr"*/
/*Q means "get by return a value"*/
/*X means "switch G and S atomically"*/
/*H means "switch T and Q atomically"*/

/**********************************************
 *
 **********************************************/

/*CAM_CAL write*/
#define CAM_CALIOC_S_WRITE _IOW(CAM_CALAGIC, 0, struct stCAM_CAL_INFO_STRUCT)
/*CAM_CAL read*/
#define CAM_CALIOC_G_READ _IOWR(CAM_CALAGIC, 5, struct stCAM_CAL_INFO_STRUCT)
/*CAM_CAL set sensor info*/
#define CAM_CALIOC_S_SENSOR_INFO \
	_IOW(CAM_CALAGIC, 10, struct CAM_CAL_SENSOR_INFO)
/*CAM_CAL read (GKI)*/
#define CAM_CALIOC_G_GKI_READ _IOWR(CAM_CALAGIC, 15, struct STRUCT_CAM_CAL_DATA_STRUCT)
/* [ITD]modify for sensorHub OIS by jiale.fei 20231031 start */
#define CAM_CALIOC_G_OIS_READ _IOWR(CAM_CALAGIC, 17, struct STRUCT_CAM_CAL_OISGyroGainXY_STRUCT)
/* [ITD]modify for sensorHub OIS by jiale.fei 20231031 end */
#define CAM_CALIOC_G_GKI_QUERY _IOR(CAM_CALAGIC, 20, struct STRUCT_CAM_CAL_DATA_STRUCT)
#define CAM_CALIOC_G_GKI_NEED_POWER_ON \
	_IOWR(CAM_CALAGIC, 25, struct STRUCT_CAM_CAL_NEED_POWER_ON)
/* [ITD]Add OTP Cloud for T1101M110-281 By sunyu 20240528 Start */
#define CAM_CALIOC_G_CHIP_ID \
	_IOWR(CAM_CALAGIC, 50, struct stCAM_CAL_INFO_STRUCT)
/* [ITD]Add OTP Cloud for T1101M110-281 By sunyu 20240528 end */

#ifdef CONFIG_COMPAT
#define COMPAT_CAM_CALIOC_S_WRITE \
	_IOW(CAM_CALAGIC, 0, struct COMPAT_stCAM_CAL_INFO_STRUCT)
#define COMPAT_CAM_CALIOC_G_READ \
	_IOWR(CAM_CALAGIC, 5, struct COMPAT_stCAM_CAL_INFO_STRUCT)
#endif
#endif /*_CAM_CAL_H*/
