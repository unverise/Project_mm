#include <linux/proc_fs.h>
#include <linux/proc_ns.h>
#include <linux/fs.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/compat.h>
#include <linux/idr.h>
#include <linux/hdreg.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/mmc/ioctl.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio_func.h>
#include <linux/uaccess.h>
#include "../core/queue.h"
#include "../core/block.h"
#include "../core/core.h"
#include "../core/card.h"
#include "../core/crypto.h"
#include "../core/host.h"
#include "../core/bus.h"
#include "../core/mmc_ops.h"
#include "../core/quirks.h"
#include "../core/sd_ops.h"
#include "mtk-mmc.h"
#define MAX_DEVICES 256
#define CID_MANFID_SANDISK				0x2
#define CID_MANFID_TOSHIBA				0x11
#define CID_MANFID_MICRON				0x13
#define CID_MANFID_SAMSUNG				0x15
#define CID_MANFID_SANDISK_NEW			0x45
#define CID_MANFID_KSI					0x70
#define CID_MANFID_HYNIX				0x90
/* ext csd field */
#define EXT_CSD_FFU_STATUS				26
#define EXT_CSD_MODE_OPERATION_CODES	29
#define EXT_CSD_MODE_CONFIG				30
#define EXT_CSD_NUM_OF_FW_SEC_PROG		302
#define EXT_CSD_FFU_ARG					487
#define EXT_CSD_OPERATION_CODE_TIMEOUT	491
#define EXT_CSD_FFU_FEATURES			492
/*
 * eMMC5.0 Field Firmware Update (FFU) opcodes
 */
#define MMC_FFU_DOWNLOAD_OP				302
#define MMC_FFU_INSTALL_OP				303
#define MMC_FFU_MODE_SET				0x1
#define MMC_FFU_MODE_NORMAL				0x0
#define MMC_FFU_INSTALL_SET				0x1
#define MMC_FFU_ENABLE					0x0
#define MMC_FFU_FW_CONFIG				0x1
#define MMC_FFU_SUPPORTED_MODES			0x1
#define MMC_FFU_FEATURES				0x1
struct mmc_ffu_ioc_data {
	struct mmc_ioc_cmd ic;
	unsigned char *buf;
	u64 buf_bytes;
	void *reserved;
};
void tran_mmc_ioctl_init(struct msdc_host *host);
void tran_mmc_ioctl_exit(struct msdc_host *host);

