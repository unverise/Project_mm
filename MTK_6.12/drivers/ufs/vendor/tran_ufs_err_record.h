#ifndef __TRAN_UFS_ERR_RECORD_H__
#define __TRAN_UFS_ERR_RECORD_H__
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <ufs/ufshcd.h>
#include "ufs-mediatek.h"

#define UFS_ERROR_RECORD_BUFFER_SIZE 4096
#define MIN_BER_SAMPLE_TIMES 10
#define UIC_ERR_HISTORY_LEN 10
#define UFS_SIZE_1GB (1024UL * 1024UL * 1024UL)
#define UNIT_MS 1000

#define TRAN_UIC_PHY_ADAPTER_LAYER_LANE0_ERROR		0x1
#define TRAN_UIC_PHY_ADAPTER_LAYER_LANE1_ERROR		0x2
#define TRAN_UIC_PHY_ADAPTER_LAYER_LANE2_ERROR		0x4
#define TRAN_UIC_PHY_ADAPTER_LAYER_LANE3_ERROR		0x8
#define TRAN_UIC_PHY_ADAPTER_LAYER_GENERIC_ERROR		0x10

#define TRAN_UIC_DATA_LINK_LAYER_ERROR_NAC_RECEIVED	0x1
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_TCX_REP_TIMER_EXP	0x2
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_AFCX_REQ_TIMER_EXP	0x4
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_FCX_PRO_TIMER_EXP	0x8
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_CRC_ERR	0x10
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_RX_BUF_OF	0x20
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_MAX_FRAME_LENGTH_EXCE	0x40
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_WRONG_SEQ_NUM	0x80
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_AFC_FRAME_SYNTAX_ERR	0x100
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_NAC_FRAME_SYNTAX_ERR	0x200
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_EOF_SYNTAX_ERR	0x400
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_FRAME_SYNTAX_ERR	0x800
#define TRAN_UIC_DATA_LINK_LAYER_BAD_CTRL_SYMBOL_TYPE	0x1000
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_PA_INIT	0x2000
#define TRAN_UIC_DATA_LINK_LAYER_ERROR_PA_ERR_IND_RECEIVED	0x4000

/*define ufs uic error code*/
enum unipro_pa_errCode {
	UNIPRO_PA_LANE0_ERR_CNT,
	UNIPRO_PA_LANE1_ERR_CNT,
	UNIPRO_PA_LANE2_ERR_CNT,
	UNIPRO_PA_LANE3_ERR_CNT,
	UNIPRO_PA_LINE_RESET,
	UNIPRO_PA_ERR_MAX
};

enum unipro_dl_errCode {
	UNIPRO_DL_NAC_RECEIVED,
	UNIPRO_DL_TCX_REPLAY_TIMER_EXPIRED,
	UNIPRO_DL_AFCX_REQUEST_TIMER_EXPIRED,
	UNIPRO_DL_FCX_PROTECTION_TIMER_EXPIRED,
	UNIPRO_DL_CRC_ERROR,
	UNIPRO_DL_RX_BUFFER_OVERFLOW,
	UNIPRO_DL_MAX_FRAME_LENGTH_EXCEEDED,
	UNIPRO_DL_WRONG_SEQUENCE_NUMBER,
	UNIPRO_DL_AFC_FRAME_SYNTAX_ERROR,
	UNIPRO_DL_NAC_FRAME_SYNTAX_ERROR,
	UNIPRO_DL_EOF_SYNTAX_ERROR,
	UNIPRO_DL_FRAME_SYNTAX_ERROR,
	UNIPRO_DL_BAD_CTRL_SYMBOL_TYPE,
	UNIPRO_DL_PA_INIT_ERROR,
	UNIPRO_DL_PA_ERROR_IND_RECEIVED,
	UNIPRO_DL_PA_INIT,
	UNIPRO_DL_ERR_MAX
};

enum unipro_nl_errCode {
	UNIPRO_NL_UNSUPPORTED_HEADER_TYPE,
	UNIPRO_NL_BAD_DEVICEID_ENC,
	UNIPRO_NL_LHDR_TRAP_PACKET_DROPPING,
	UNIPRO_NL_ERR_MAX
};

enum unipro_tl_errCode {
	UNIPRO_TL_UNSUPPORTED_HEADER_TYPE,
	UNIPRO_TL_UNKNOWN_CPORTID,
	UNIPRO_TL_NO_CONNECTION_RX,
	UNIPRO_TL_CONTROLLED_SEGMENT_DROPPING,
	UNIPRO_TL_BAD_TC,
	UNIPRO_TL_E2E_CREDIT_OVERFLOW,
	UNIPRO_TL_SAFETY_VALVE_DROPPING,
	UNIPRO_TL_ERR_MAX
};

enum unipro_dme_errCode {
	UNIPRO_DME_GENERIC,
	UNIPRO_DME_TX_QOS,
	UNIPRO_DME_RX_QOS,
	UNIPRO_DME_PA_INIT_QOS,
	UNIPRO_DME_ERR_MAX
};


struct high_latency {
	unsigned long long latency_value;
	ktime_t time;
};

struct ufs_uic_err_history {
	unsigned long long transfered_bytes; /* unit: bytes */
	unsigned long uic_err;
	ktime_t time;
	struct list_head list;
};

void ufshcd_count_data(struct scsi_cmnd *cmd, struct ufs_hba *hba);
void tran_ufs_err_record_init(struct ufs_hba *hba);
void tran_ufs_err_record_exit(void);
void tran_record_uic_err(struct ufs_hba *hba, enum ufs_event_type evt, u32 data);
#endif
