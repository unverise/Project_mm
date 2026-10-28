/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 TRANSSION Inc.
 * Transsion device stability message manager by qiwu.chen on 20240310
 */

#ifndef _TRAN_DEVMSG_H
#define _TRAN_DEVMSG_H

#define TRAN_DEV_MSG_MAGIC "TDSM"
#define TRAN_DEVMSG_MAX_SIZE SZ_256
#define TRAN_DEVMSG_MBLOCK_SIZE 0x10000

/* device exp type */
enum TRAN_DEV_EXP_TYPE {
	CPU_DEV_EXP,
	DRAM_DEV_EXP,
	FLASH_DEV_EXP,
	POWER_LOSS_EXP,
	DM_VERITY_EXP,
	ROOT_HIJACK_EXP,
	DEV_EXP_TYPE_END
};

/* boot phase of devmsg */
enum TRAN_DEVMSG_PHASE {
	DEVMSG_PHASE_PL,
	DEVMSG_PHASE_LK,
	DEVMSG_PHASE_KERNEL,
	DEVMSG_PHASE_ATF,
	DEVMSG_PHASE_GZ,
	DEVMSG_PHASE_END
};

/* cpu exp type */
enum TRAN_CPU_DEV_EXP_TYPE {
	UNDEFINED_INST,
	SERROR,
	CACHE_PARITY,
	DEVMPU_VIO,
	EMIMPU_VIO,
	DEVAPC,
	CPU_DEV_EXP_TYPE_END
};

/* flash devmsg description */
enum TRAN_FLASH_DEVMSG_TYPE {
	MSDC_REQ_TIMEOUT,
	UFS_PA_ERR,
	UFS_DL_ERR,
	UFS_NL_ERR,
	UFS_TL_ERR,
	UFS_DME_ERR,
	UFS_AUTO_HIBERN8_ERR,
	UFS_FATAL_ERR,
	UFS_LINK_STARTUP_FAIL,
	UFS_RESUME_ERR,
	UFS_SUSPEND_ERR,
	UFS_HOST_RESET,
	UFS_CMD_ABORT,
	FLASH_DEVMSG_TYPE_END
};

/* DRAM devmsg description */
enum TRAN_DRAM_DEVMSG_TYPE {
       KERNEL_EXCEPTION,
       PL_MEM_TEST_FAIL1,
       PL_MEM_TEST_FAIL2,
       PL_DRAM_GATING_ERR,
       PL_DRAM_FATAL_EXCEPTION,
       DRAM_DEVMSG_TYPE_END
};

/* Power exception devmsg description */
enum TRAN_POWER_EXP_DEVMSG_TYPE {
       POWER_OCP,
       POWER_LOW_BAT,
       POWER_2SEC_REBOOT,
       POWER_LOSS,
       POWER_EXP_DEVMSG_TYPE_END
};

/* time format description */
struct tran_devmsg_time {
	u16 tm_sec;
	u16 tm_min;
	u16 tm_hour;
	u16 tm_mday;
	u16 tm_mon;
	u16 tm_year;
};

/* devmsg format description */
struct tran_devmsg_desc {
	enum TRAN_DEV_EXP_TYPE id;
	enum TRAN_DEVMSG_PHASE phase;
	u32 type; //msg type
	u32 idx;  //msg idx
	u32 start; //msg ctx start offset
	u32 len; //msg ctx length
	struct tran_devmsg_time tm; //msg timestamp
	u32 reserved; //reserved 4 bytes for aligned
};

/* devmsg header description */
struct tran_devmsg_header {
	u8 magic[4];
	u32 offset; //current devmsg record offset
	u32 reboot_count; //current abnormal reboot count
	u32 count;  //current devmsg count
};

bool tran_record_devmsg_api(const enum TRAN_DEV_EXP_TYPE id, u32 msgtype, const char *fmt, ...);
int __init tran_devmsg_init(u32 paddr, u32 size);
void __exit tran_devmsg_exit(void);

#endif

