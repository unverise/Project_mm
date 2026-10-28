/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 tran_ufs_cdev, Inc.
 *
 */
#ifndef __TRAN_UFS_CDEV_H__
#define __TRAN_UFS_CDEV_H__
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include "../bootdevice/bootdevice/tran-proc-ufs.h"
#define UFS_MAX_DEVICES				256
#define UFS_IOCTL_AFC_ENTER			0x5395
#define UFS_IOCTL_AFC_EXIT			0x5396
#define PWM_MODE_VAL				0x22
#define HS_MODE_VAL				0x11
#define UFS_CDEV_TIMEOUT			50000
#define UFS_IOCTL_FFU                               0x5389
//MD:add XLLSHLSSP-17 by xiaoxiang.xiong for ffu start
#define UFS_IOCTL_QUERY                            0x5388
#define UFS_IOCTL_SG_IO								0x538a
#define UFS_IOCTL_GET_FW_VER                        0x5390  /* Query production revision level */
//MD:add XLLSHLSSP-17 by xiaoxiang.xiong for ffu end
#define UFS_IOCTL_GET_DEVICE_INFO                   0x5392
//MD:add XLLSHLSSP-17 by xiaoxiang.xiong for ffu start
#define QUERY_ATTR_IDN_BOOT_LUN_EN 		    0x0
//MD:add XLLSHLSSP-17 by xiaoxiang.xiong for ffu end
#define UFS_IOCTL_FFU_MAX_FW_SIZE_BYTES             (1L * 1024 * 1024)
#define UFS_IOCTL_FFU_MAX_MODEL_BYTES		16
#define UFS_IOCTL_FFU_MAX_VENDOR_BYTES		8
#define UFS_IOCTL_FFU_MAX_FW_SIZE		0x200000	/* 2MB */
#define UFS_UPIU_MAX_GENERAL_LUN		8
#define MAX_WRITE_BUFFER_SIZE (512 * 1024)
#define UFS_FFU_STATUS_OK  1
#define UFS_SLOW_MODE	(SLOW_MODE | (SLOW_MODE << 4))
/* FFU */
#define WB_MODE_MASK				0x1F
#define DOWNLOAD_MODE				0xE
enum ufs_ffu_status {
	UFS_FFU_STATUS_NO_INFORMATION		= 0x0,
	UFS_FFU_STATUS_SUCCESSFUL_UPDATE	= 0x1,
	UFS_FFU_STATUS_CORRUPTION_ERR		= 0x2,
	UFS_FFU_STATUS_INTERNAL_ERROR		= 0x3,
	UFS_FFU_STATUS_VERSION_MISMATCH		= 0x4,
	UFS_FFU_STATUS_GENERAL_ERROR		= 0xFF,
};
//MD:add XLLSHLSSP-17 by xiaoxiang.xiong for ffu start
struct tran_ufs_ioctl_query_data {
	/*
	 * User should select one of the opcode defined in "enum query_opcode".
	 * Please check include/uapi/scsi/ufs/ufs.h for the definition of it.
	 * Note that only UPIU_QUERY_OPCODE_READ_DESC,
	 * UPIU_QUERY_OPCODE_READ_ATTR & UPIU_QUERY_OPCODE_READ_FLAG are
	 * supported as of now. All other query_opcode would be considered
	 * invalid.
	 * As of now only read query operations are supported.
	 */
	__u32 opcode;
	/*
	 * User should select one of the idn from "enum flag_idn" or "enum
	 * attr_idn" or "enum desc_idn" based on whether opcode above is
	 * attribute, flag or descriptor.
	 * Please check include/uapi/scsi/ufs/ufs.h for the definition of it.
	 */
	__u8 idn;
	__u8 idx;
	/*
	 * User should specify the size of the buffer (buffer[0] below) where
	 * it wants to read the query data (attribute/flag/descriptor).
	 * As we might end up reading less data then what is specified in
	 * buf_size. So we are updating buf_size to what exactly we have read.
	 */
	__u16 buf_byte;
	/*
	 * placeholder for the start of the data buffer where kernel will copy
	 * the query data (attribute/flag/descriptor) read from the UFS device
	 * Note:
	 * For Read Attribute you will have to allocate 4 bytes
	 * For Read Flag you will have to allocate 1 byte
	 */
	__u8 *buf_ptr;
};
/**
 * struct ufs_ioctl_ffu - used to transfer data to and from user via ioctl
 * @buf_size: number of allocated bytes/data size on return
 * @buf_ptr: data location
 */
 struct ufs_ioctl_query_fw_ver_data {
	/*
	 * User should specify the size of the buffer (buf_ptr below) where
	 * it wants to read firmware version data.
	 */
	__u16 buf_byte;
	/*
	 * placeholder for the start address of the data buffer where kernel
	 * will copy
	 * the data.
	 */
	__u8 *buf_ptr;
};
//MD:add XLLSHLSSP-17 by xiaoxiang.xiong for ffu end
struct ufs_ioctl_ffu_data {
	__u32			buf_byte;
	__u32			chunk_byte;
	__u8			*buf_ptr;
};
struct ufs_ioctl_query_device_info {
	__u8			vendor[UFS_IOCTL_FFU_MAX_VENDOR_BYTES];
	__u8			model[UFS_IOCTL_FFU_MAX_MODEL_BYTES];
	__u8			fw_rev[UFS_IOCTL_FFU_MAX_FW_VER_BYTES];
	__u16			manid;
	__u32			max_hw_sectors_size;
};
struct tran_ufs_cdev_data {
	dev_t			devt;
	struct ufs_hba		*hba;
	struct device		*ufs_dev;
	struct cdev		chrdev;
};
void tran_ufs_set_blocked_in_ffu_state(struct ufs_hba *hba,
				       struct ufshcd_lrb *lrbp, int *err);
int tran_ufs_cdev_init(struct ufs_hba *hba);
void tran_ufs_cdev_exit(void);
#endif
