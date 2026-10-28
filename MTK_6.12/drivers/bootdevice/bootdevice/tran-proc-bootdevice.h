/**
  *Copyright 2022 Transsion inc
  *Author jinkai.zhang
  *get ufs info
*/
#ifndef _TRAN_PROC_BOOTDEVICE_H_
#define _TRAN_PROC_BOOTDEVICE_H_
#include <linux/proc_fs.h>
#include <linux/proc_ns.h>
#include <linux/mmc/mmc.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/mmc/card.h>

#define PROC_MODE 0444
#define MAX_NAME_LEN 32
#define MAX_FIRMWARE_LEN 16
#define MAX_MMC_FIRMWARE_LEN 16
#define MAX_VENDOR_NAME_LEN 10
#define MAX_SERIAL_NUM_LEN 128
struct __bootdevice {
	const struct device *dev;
	u32 raw_cid[4];
	u32 raw_csd[4];
	u8 rev;
	unsigned short oemid;
	unsigned char hwrev;
	unsigned long long enhanced_area_offset;
	unsigned int enhanced_area_size;
	unsigned char prv;
	u32	ocr;
	unsigned int erase_size;
	const char *name;
	u8 ext_csd[512];
	unsigned int serial;
	int manfdate;
	int available_buffer_size;
	bool wbEnable;
	int wbCurrent_buffer;
	u32 alloc_unit_size;
	char flash_id[MAX_VENDOR_NAME_LEN + MAX_NAME_LEN + 1];
	char product_name[MAX_NAME_LEN + 1];
	u8 life_time_est_typ_a;
	u8 life_time_est_typ_b;
	unsigned int manfid;
	u8 fwrev[MAX_FIRMWARE_LEN + 1];
	u8 mmc_fwrev[MAX_MMC_FIRMWARE_LEN +1];
	u8 pre_eol_info;
	unsigned long size;
	u8 type;
	bool ffu_support;
};
#endif
