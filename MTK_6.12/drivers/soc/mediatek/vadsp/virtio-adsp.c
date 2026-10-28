// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2024 MediaTek Inc.
 * virt-adsp: Virtio adsp device
 */
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/vringh.h>
#include <uapi/linux/virtio_ids.h>
#include <uapi/linux/virtio_thread.h>
#include <linux/virtio_ring.h>
#include <linux/virtio_config.h>
#include <asm/arch_timer.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/kfifo.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <sound/soc.h>
#include "virtio-adsp.h"
#include "virtio_pcm.h"
#include "virtio_card.h"
#include "audio_task.h"
#include "virt_utils.h"


#define LOCAL_ADSP_TEST 1
#define MAX_ADSP_IOV_SIZE (35)
#define MAX_REC_TIME_CNT 50
#define MAX_START_TIME_FIFO_LEN (1024)
#define SND_DSP_DTS_SIZE (4)
#define MAX_CPU_NUM 8

enum {
	VIRTIO_ADSP_VQ_EVT = 0,
	VIRTIO_ADSP_VQ_REQ = 1,
	VIRTIO_ADSP_VQ_MAX = 2,
};

struct adsp_event {
	int32_t type;
	union {
		struct adsp_irq_event irq_evt;
		struct adsp_mem_event mem_evt;
		struct adsp_ipi_event ipi_evt;
		struct adsp_recovery_event recovery_evt;
	};
};

struct debug_work {
	struct work_struct work;
	struct workqueue_struct *work_queue;
};

struct recovery_work {
	struct work_struct work;
	struct workqueue_struct *work_queue;
	uint32_t cmd_type;
};

struct virtio_adsp {
	struct virtio_device *vdev;
	struct virtqueue *vqs[VIRTIO_ADSP_VQ_MAX];
	spinlock_t lock;
	spinlock_t evt_lock;
	adsp_event_cb_func cb;
	void *data;
	struct completion req_done;
	void *shm_region;
	struct debug_work dbg_work;
	struct recovery_work recovery_work;
	int time_dbg_enable;
};

struct virtio_adsp_event_cb {
	adsp_event_cb_func cb;
	void *data;
};

struct virtio_adsp_ipi_cb {
	adsp_ipi_cb_func cb;
};

struct virtio_adsp_mbrain_cb {
	adsp_mbrain_cb_func cb;
};

struct virtio_adsp_virtdsp_cb {
	adsp_virtdsp_cb_func cb;
};

struct adsp_pointer_request {
	uint64_t task_id;
};

struct adsp_response {
	int32_t ret;
};

struct adsp_copy_request {
	uint64_t task_id;
	uint64_t phy_addr;
	uint64_t copy_size;
};

struct adsp_trigger_request {
	uint64_t type;
	uint64_t id;
	uint64_t cmd;
};

enum {
	ADSP_OPS_GET_POINTER,
	ADSP_OPS_COPY_DL,
	ADSP_OPS_TRIGGER,
	ADSP_OPS_COPY_UL,
};

struct adsp_request {
	uint8_t type;
	union {
		struct adsp_pointer_request ptr_req;
		struct adsp_copy_request copy_req;
		struct adsp_trigger_request trigger_req;
	};
};

struct virtio_adsp_req {
	struct adsp_request req;
	struct adsp_response res;
	struct completion done;
};

struct virtio_adsp_rec_ts {
	struct timespec64 st;
	struct timespec64 et;
	long interval_ns;
};
struct virtio_adsp_irq_skip_info {
	struct timespec64 ts[MAX_REC_TIME_CNT];
	int rec_cnt;
	int skip_cnt;
};

struct virtio_adsp_rec_ts_info {
	struct virtio_adsp_rec_ts ts[MAX_REC_TIME_CNT];
	DECLARE_KFIFO(st_queue, struct timespec64, MAX_START_TIME_FIFO_LEN);
	int rec_cnt;
};

struct virtio_adsp_bind_cpu_info {
	unsigned int cpu_num;
	unsigned int cpu_array[MAX_CPU_NUM];
};

static struct virtio_adsp_bind_cpu_info audio_bind_cpu;
static struct mtk_adsp_task_attr adsp_task_attr[AUDIO_TASK_DAI_NUM] = {
	[AUDIO_TASK_VOIP_ID] = {true, -1, -1, -1,
				VOIP_FEATURE_ID, false},
	[AUDIO_TASK_PRIMARY_ID] = {true, -1, -1, -1,
				   PRIMARY_FEATURE_ID, false},
	[AUDIO_TASK_OFFLOAD_ID] = {true, -1, -1, -1,
				   OFFLOAD_FEATURE_ID, false},
	[AUDIO_TASK_DEEPBUFFER_ID] = {false, -1, -1, -1,
				      DEEPBUF_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK_ID] = {false, -1, -1, -1,
				    AUDIO_PLAYBACK_FEATURE_ID, false},
	[AUDIO_TASK_MUSIC_ID] = {false, -1, -1, -1,
				 AUDIO_MUSIC_FEATURE_ID, false},
	[AUDIO_TASK_CAPTURE_UL1_ID] = {true, -1, -1, -1,
				       CAPTURE_FEATURE_ID, false},
	[AUDIO_TASK_A2DP_ID] = {true, -1, -1, -1,
				A2DP_PLAYBACK_FEATURE_ID, false},
	[AUDIO_TASK_BLEDL_ID] = {true, -1, -1, -1,
				BLEDL_FEATURE_ID, false},
	[AUDIO_TASK_BLEUL_ID] = {true, -1, -1, -1,
				BLEUL_FEATURE_ID, false},
	[AUDIO_TASK_BTDL_ID] = {true, -1, -1, -1,
				BTDL_FEATURE_ID, false},
	[AUDIO_TASK_BTUL_ID] = {true, -1, -1, -1,
				BTUL_FEATURE_ID, false},
	[AUDIO_TASK_DATAPROVIDER_ID] = {true, -1, -1, -1,
				       AUDIO_DATAPROVIDER_FEATURE_ID, false},
	[AUDIO_TASK_CALL_FINAL_ID] = {true, -1, -1, -1,
				      CALL_FINAL_FEATURE_ID, false},
	[AUDIO_TASK_FAST_ID] = {false, -1, -1, -1, FAST_FEATURE_ID, false},
	[AUDIO_TASK_SPATIALIZER_ID] = {false, -1, -1, -1, SPATIALIZER_FEATURE_ID, false},
	[AUDIO_TASK_KTV_ID] = {true, -1, -1, -1, KTV_FEATURE_ID, false},
	[AUDIO_TASK_CAPTURE_RAW_ID] = {false, -1, -1, -1,
				       CAPTURE_RAW_FEATURE_ID, false},
	[AUDIO_TASK_FM_ADSP_ID] = {true, -1, -1, -1,
				   FM_ADSP_FEATURE_ID, false},
	[AUDIO_TASK_UL_PROCESS_ID] = {true, -1, -1, -1,
				      CAPTURE_FEATURE_ID, false},
	[AUDIO_TASK_ECHO_REF_ID] = {true, -1, -1, -1,
				    CAPTURE_FEATURE_ID, false},
	[AUDIO_TASK_ECHO_REF_DL_ID] = {true, -1, -1, -1,
				       AUDIO_PLAYBACK_FEATURE_ID, false},
	[AUDIO_TASK_USBDL_ID] = {true, -1, -1, -1,
				USB_DL_FEATURE_ID, false},
	[AUDIO_TASK_USBUL_ID] = {true, -1, -1, -1,
				USB_UL_FEATURE_ID, false},
	[AUDIO_TASK_MDDL_ID] = {true, -1, -1, -1,
				VOICE_CALL_SUB_FEATURE_ID, false},
	[AUDIO_TASK_MDUL_ID] = {true, -1, -1, -1,
				VOICE_CALL_FEATURE_ID, false},
	[AUDIO_TASK_CALLDL_ID] = {true, -1, -1, -1,
				VOICE_CALL_SUB_FEATURE_ID, false},
	[AUDIO_TASK_CALLUL_ID] = {true, -1, -1, -1,
				VOICE_CALL_FEATURE_ID, false},
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	[AUDIO_TASK_HFP_CLIENT_RX_ADSP_ID] = {true, -1, -1, -1,
				HFP_CLIENT_RX_FEATURE_ID, false},
	[AUDIO_TASK_HFP_CLIENT_TX_ADSP_ID] = {true, -1, -1, -1,
				HFP_CLIENT_TX_FEATURE_ID, false},
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	[AUDIO_TASK_ANC_ADSP_ID] = {true, -1, -1, -1,
				   ANC_FEATURE_ID, false},
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	[AUDIO_TASK_EXTSTREAM1_ADSP_ID] = {true, -1, -1, -1,
				EXTSTREAM1_FEATURE_ID, false},
	[AUDIO_TASK_EXTSTREAM2_ADSP_ID] = {true, -1, -1, -1,
				EXTSTREAM2_FEATURE_ID, false},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT)
	[AUDIO_TASK_SUB_PLAYBACK_ID] = {false, -1, -1, -1,
				    SUB_PLAYBACK_FEATURE_ID, false},
#endif
	[AUDIO_TASK_PLAYBACK0_ID] = {true, -1, -1, -1,
				   PLAYBACK0_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK1_ID] = {true, -1, -1, -1,
				   PLAYBACK1_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK2_ID] = {true, -1, -1, -1,
				   PLAYBACK2_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK3_ID] = {true, -1, -1, -1,
				   PLAYBACK3_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK4_ID] = {true, -1, -1, -1,
				   PLAYBACK4_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK5_ID] = {true, -1, -1, -1,
				   PLAYBACK5_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK6_ID] = {true, -1, -1, -1,
				   PLAYBACK6_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK7_ID] = {true, -1, -1, -1,
				   PLAYBACK7_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK8_ID] = {true, -1, -1, -1,
				   PLAYBACK8_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK9_ID] = {true, -1, -1, -1,
				   PLAYBACK9_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK10_ID] = {true, -1, -1, -1,
				   PLAYBACK10_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK11_ID] = {true, -1, -1, -1,
				   PLAYBACK11_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK12_ID] = {true, -1, -1, -1,
				   PLAYBACK12_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK13_ID] = {true, -1, -1, -1,
				   PLAYBACK13_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK14_ID] = {true, -1, -1, -1,
				   PLAYBACK14_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK15_ID] = {true, -1, -1, -1,
				   PLAYBACK15_FEATURE_ID, false},
	[AUDIO_TASK_CAPTURE_MCH_ID] = {true, -1, -1, -1,
				   CAPTURE_MCH_FEATURE_ID, false},
#endif
};

static int dsp_task_scence[AUDIO_TASK_DAI_NUM] = {
	[AUDIO_TASK_VOIP_ID]        = TASK_SCENE_VOIP,
	[AUDIO_TASK_PRIMARY_ID]     = TASK_SCENE_PRIMARY,
	[AUDIO_TASK_OFFLOAD_ID]     = TASK_SCENE_PLAYBACK_MP3,
	[AUDIO_TASK_DEEPBUFFER_ID]  = TASK_SCENE_DEEPBUFFER,
	[AUDIO_TASK_PLAYBACK_ID]    = TASK_SCENE_AUDPLAYBACK,
	[AUDIO_TASK_MUSIC_ID]       = TASK_SCENE_MUSIC,
	[AUDIO_TASK_CAPTURE_RAW_ID] = TASK_SCENE_CAPTURE_RAW,
	[AUDIO_TASK_CAPTURE_UL1_ID] = TASK_SCENE_CAPTURE_UL1,
	[AUDIO_TASK_A2DP_ID]        = TASK_SCENE_A2DP,
	[AUDIO_TASK_BLEDL_ID]       = TASK_SCENE_BLEDL,
	[AUDIO_TASK_BLEUL_ID]       = TASK_SCENE_BLEUL,
	[AUDIO_TASK_BTDL_ID]        = TASK_SCENE_BTDL,
	[AUDIO_TASK_BTUL_ID]        = TASK_SCENE_BTUL,
	[AUDIO_TASK_DATAPROVIDER_ID] = TASK_SCENE_DATAPROVIDER,
	[AUDIO_TASK_CALL_FINAL_ID]  = TASK_SCENE_CALL_FINAL,
	[AUDIO_TASK_FAST_ID]        = TASK_SCENE_FAST,
	[AUDIO_TASK_KTV_ID]         = TASK_SCENE_KTV,
	[AUDIO_TASK_FM_ADSP_ID]     = TASK_SCENE_FM_ADSP,
	[AUDIO_TASK_UL_PROCESS_ID]  = TASK_SCENE_UL_PROCESS,
	[AUDIO_TASK_ECHO_REF_ID]    = TASK_SCENE_ECHO_REF_UL,
	[AUDIO_TASK_ECHO_REF_DL_ID] = TASK_SCENE_ECHO_REF_DL,
	[AUDIO_TASK_USBDL_ID]       = TASK_SCENE_USB_DL,
	[AUDIO_TASK_USBUL_ID]       = TASK_SCENE_USB_UL,
	[AUDIO_TASK_MDDL_ID]        = TASK_SCENE_MD_DL,
	[AUDIO_TASK_MDUL_ID]        = TASK_SCENE_MD_UL,
	[AUDIO_TASK_SPATIALIZER_ID] = TASK_SCENE_SPATIALIZER,
	[AUDIO_TASK_CALLDL_ID]      = TASK_SCENE_PHONE_CALL_SUB,
	[AUDIO_TASK_CALLUL_ID]      = TASK_SCENE_PHONE_CALL,
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	[AUDIO_TASK_HFP_CLIENT_RX_ADSP_ID]  = TASK_SCENE_HFP_CLIENT_RX,
	[AUDIO_TASK_HFP_CLIENT_TX_ADSP_ID]  = TASK_SCENE_HFP_CLIENT_TX,
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	[AUDIO_TASK_ANC_ADSP_ID]    = TASK_SCENE_ANC,
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	[AUDIO_TASK_EXTSTREAM1_ADSP_ID]    = TASK_SCENE_EXTSTREAM1,
	[AUDIO_TASK_EXTSTREAM2_ADSP_ID]    = TASK_SCENE_EXTSTREAM2,
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT)
	[AUDIO_TASK_SUB_PLAYBACK_ID]    = TASK_SCENE_SUB_PLAYBACK,
#endif
	[AUDIO_TASK_PLAYBACK0_ID]   = TASK_SCENE_PLAYBACK0,
	[AUDIO_TASK_PLAYBACK1_ID]   = TASK_SCENE_PLAYBACK1,
	[AUDIO_TASK_PLAYBACK2_ID]   = TASK_SCENE_PLAYBACK2,
	[AUDIO_TASK_PLAYBACK3_ID]   = TASK_SCENE_PLAYBACK3,
	[AUDIO_TASK_PLAYBACK4_ID]   = TASK_SCENE_PLAYBACK4,
	[AUDIO_TASK_PLAYBACK5_ID]   = TASK_SCENE_PLAYBACK5,
	[AUDIO_TASK_PLAYBACK6_ID]   = TASK_SCENE_PLAYBACK6,
	[AUDIO_TASK_PLAYBACK7_ID]   = TASK_SCENE_PLAYBACK7,
	[AUDIO_TASK_PLAYBACK8_ID]   = TASK_SCENE_PLAYBACK8,
	[AUDIO_TASK_PLAYBACK9_ID]   = TASK_SCENE_PLAYBACK9,
	[AUDIO_TASK_PLAYBACK10_ID]  = TASK_SCENE_PLAYBACK10,
	[AUDIO_TASK_PLAYBACK11_ID]  = TASK_SCENE_PLAYBACK11,
	[AUDIO_TASK_PLAYBACK12_ID]  = TASK_SCENE_PLAYBACK12,
	[AUDIO_TASK_PLAYBACK13_ID]  = TASK_SCENE_PLAYBACK13,
	[AUDIO_TASK_PLAYBACK14_ID]  = TASK_SCENE_PLAYBACK14,
	[AUDIO_TASK_PLAYBACK15_ID]  = TASK_SCENE_PLAYBACK15,
	[AUDIO_TASK_CAPTURE_MCH_ID]  = TASK_SCENE_CAPTURE_MCH,
#endif
};

static char *dsp_task_dsp_name[AUDIO_TASK_DAI_NUM] = {
	[AUDIO_TASK_VOIP_ID]        = "mtk-dsp-voip",
	[AUDIO_TASK_PRIMARY_ID]     = "mtk-dsp-primary",
	[AUDIO_TASK_OFFLOAD_ID]     = "mtk-dsp-offload",
	[AUDIO_TASK_DEEPBUFFER_ID]  = "mtk-dsp-deep",
	[AUDIO_TASK_PLAYBACK_ID]    = "mtk-dsp-playback",
	[AUDIO_TASK_MUSIC_ID]       = "mtk-dsp-music",
	[AUDIO_TASK_CAPTURE_UL1_ID] = "mtk-dsp-capture1",
	[AUDIO_TASK_A2DP_ID]        = "mtk-dsp-a2dp",
	[AUDIO_TASK_BLEDL_ID]       = "mtk-dsp-bledl",
	[AUDIO_TASK_BLEUL_ID]       = "mtk-dsp-bleul",
	[AUDIO_TASK_BTDL_ID]        = "mtk-dsp-btdl",
	[AUDIO_TASK_BTUL_ID]        = "mtk-dsp-btul",
	[AUDIO_TASK_DATAPROVIDER_ID] = "mtk-dsp-dataprovider",
	[AUDIO_TASK_CALL_FINAL_ID]  = "mtk-dsp-call-final",
	[AUDIO_TASK_FAST_ID]        = "mtk-dsp-fast",
	[AUDIO_TASK_SPATIALIZER_ID] = "mtk-dsp-spatializer",
	[AUDIO_TASK_KTV_ID]         = "mtk-dsp-ktv",
	[AUDIO_TASK_CAPTURE_RAW_ID] = "mtk-dsp-capture-raw",
	[AUDIO_TASK_FM_ADSP_ID]     = "mtk-dsp-fm",
	[AUDIO_TASK_UL_PROCESS_ID]  = "mtk-dsp-ulproc",
	[AUDIO_TASK_ECHO_REF_ID]    = "mtk-dsp-echoref",
	[AUDIO_TASK_ECHO_REF_DL_ID] = "mtk-dsp-echodl",
	[AUDIO_TASK_USBDL_ID]       = "mtk-dsp-usbdl",
	[AUDIO_TASK_USBUL_ID]       = "mtk-dsp-usbul",
	[AUDIO_TASK_MDDL_ID]        = "mtk-dsp-mddl",
	[AUDIO_TASK_MDUL_ID]        = "mtk-dsp-mdul",
	[AUDIO_TASK_CALLDL_ID]      = "mtk-dsp-calldl",
	[AUDIO_TASK_CALLUL_ID]      = "mtk-dsp-callul",
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	[AUDIO_TASK_HFP_CLIENT_RX_ADSP_ID]      = "mtk-dsp-hfp-client-rx",
	[AUDIO_TASK_HFP_CLIENT_TX_ADSP_ID]      = "mtk-dsp-hfp-client-tx",
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	[AUDIO_TASK_ANC_ADSP_ID]    = "mtk-dsp-anc",
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	[AUDIO_TASK_EXTSTREAM1_ADSP_ID]   = "mtk-dsp-extstream1",
	[AUDIO_TASK_EXTSTREAM2_ADSP_ID]   = "mtk-dsp-extstream2",
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT)
	[AUDIO_TASK_SUB_PLAYBACK_ID]   = "mtk-dsp-sub-playback",
#endif
	[AUDIO_TASK_PLAYBACK0_ID]   = "mtk-dsp-playback0",
	[AUDIO_TASK_PLAYBACK1_ID]   = "mtk-dsp-playback1",
	[AUDIO_TASK_PLAYBACK2_ID]   = "mtk-dsp-playback2",
	[AUDIO_TASK_PLAYBACK3_ID]   = "mtk-dsp-playback3",
	[AUDIO_TASK_PLAYBACK4_ID]   = "mtk-dsp-playback4",
	[AUDIO_TASK_PLAYBACK5_ID]   = "mtk-dsp-playback5",
	[AUDIO_TASK_PLAYBACK6_ID]   = "mtk-dsp-playback6",
	[AUDIO_TASK_PLAYBACK7_ID]   = "mtk-dsp-playback7",
	[AUDIO_TASK_PLAYBACK8_ID]   = "mtk-dsp-playback8",
	[AUDIO_TASK_PLAYBACK9_ID]   = "mtk-dsp-playback9",
	[AUDIO_TASK_PLAYBACK10_ID]   = "mtk-dsp-playback10",
	[AUDIO_TASK_PLAYBACK11_ID]   = "mtk-dsp-playback11",
	[AUDIO_TASK_PLAYBACK12_ID]   = "mtk-dsp-playback12",
	[AUDIO_TASK_PLAYBACK13_ID]   = "mtk-dsp-playback13",
	[AUDIO_TASK_PLAYBACK14_ID]   = "mtk-dsp-playback14",
	[AUDIO_TASK_PLAYBACK15_ID]   = "mtk-dsp-playback15",
	[AUDIO_TASK_CAPTURE_MCH_ID] = "mtk-dsp-capture-mch",
#endif
};

#define VIRTIO_ADSP_SHMCAP_ID_CACHE 0
#define MAX_TRACE_MSG_LEN 256
#define LOCAL_ERR 0
#define LOCAL_WARN 1
#define LOCAL_INFO 2
#define LOCAL_DBG 3
#if LOCAL_ADSP_TEST
static int trace_level = LOCAL_DBG;
#else
static int trace_level = LOCAL_ERR;
#endif
int adsp_trace(int level, const char *fmt, ...)
{
	char trace_msg[MAX_TRACE_MSG_LEN];
	int len;
	va_list args;

	if (trace_level < level)
		return -1;

	va_start(args, fmt);
	len = vsnprintf(trace_msg, MAX_TRACE_MSG_LEN, fmt, args);
	va_end(args);

	if(len < 0)
		return -1;
#if IS_ENABLED(CONFIG_MTK_VIRTSND_DEBUG_SUPPORT)
	//trace_printk(trace_msg);
#endif
	return 0;
}

static struct virtio_adsp_rec_ts_info vadsp_rec_ts[TASK_SCENE_SIZE][REC_TIME_POS_NUM];
static struct virtio_adsp_irq_skip_info rec_irq_skip;

static struct virtio_adsp *adsp_handler;

static struct virtio_adsp_event_cb adsp_event_cb;
static struct virtio_adsp_ipi_cb adsp_ipi_cb;
static struct virtio_adsp_virtdsp_cb adsp_virtdsp_cb;
static struct virtio_adsp_mbrain_cb adsp_mbrain_cb;

struct adsp_mem task_ring_mem[TASK_SCENE_SIZE] = {{0,0}};
struct adsp_mem task_atod_mem[TASK_SCENE_SIZE] = {{0,0}};
struct adsp_mem task_dtoa_mem[TASK_SCENE_SIZE] = {{0,0}};
struct adsp_mem adsp_rsv_mem = {0, 0};
struct adsp_shm_region adsp_shm = {0, 0};

static uint64_t irq_cnt[TASK_SCENE_SIZE] = {0};
int32_t cur_ptr[TASK_SCENE_SIZE] = {0};
static bool vadsp_runtime_enable = true;

uint64_t get_irq_cnt(int task_scene)
{
	if (task_scene > 0 && task_scene < TASK_SCENE_SIZE)
		return irq_cnt[task_scene];
	return 0;
}

int get_rec_time_prev_cnt(int task_id, enum rec_time_pos pos, int cnt)
{
	int prev_cnt = -1;

	if(task_id < 0 || task_id >= TASK_SCENE_SIZE || pos >= REC_TIME_POS_NUM)
		return prev_cnt;
	if (cnt > 0 && cnt < MAX_REC_TIME_CNT) {
		prev_cnt = cnt - 1;
	} else if (cnt == 0) {
		if (vadsp_rec_ts[task_id][pos].ts[MAX_REC_TIME_CNT - 1].st.tv_sec != 0)
			prev_cnt = MAX_REC_TIME_CNT - 1;
	}

	return prev_cnt;
}

void virtio_adsp_rec_time_init(void)
{
	int task = 0, pos = 0;

	memset(&vadsp_rec_ts, 0, sizeof(vadsp_rec_ts));
	memset(&rec_irq_skip, 0, sizeof(rec_irq_skip));

	for (task = 0; task < TASK_SCENE_SIZE; task++) {
		for (pos = 0; pos < REC_TIME_POS_NUM; pos++)
			INIT_KFIFO(vadsp_rec_ts[task][pos].st_queue);
	}
}

int virtio_adsp_rec_irq_time_skip(void)
{
	int ret = 0;
	int cnt = 0;

	if (adsp_handler && adsp_handler->time_dbg_enable == 0)
		return -1;

	cnt = rec_irq_skip.rec_cnt;
	ktime_get_ts64(&(rec_irq_skip.ts[cnt]));

	rec_irq_skip.skip_cnt++;
	rec_irq_skip.rec_cnt++;
	if (rec_irq_skip.rec_cnt > MAX_REC_TIME_CNT - 1)
		rec_irq_skip.rec_cnt = 0;

	return ret;
}

int virtio_adsp_rec_time_start(int task_id, enum rec_time_pos pos)
{
	struct timespec64 ts;
	int ret = 0;

	if (adsp_handler && adsp_handler->time_dbg_enable == 0)
		return -1;
	if (task_id < 0 || task_id >= TASK_SCENE_SIZE ||pos >= REC_TIME_POS_NUM)
		return -1;
	ktime_get_ts64(&ts);

	ret = kfifo_put(&vadsp_rec_ts[task_id][pos].st_queue, ts);
	if (ret == 0) {
		pr_info("%s() kfifo is full, len:%d\n", __func__,
				kfifo_len(&vadsp_rec_ts[task_id][pos].st_queue));
		virtio_adsp_rec_irq_time_skip();
	}

	return 0;
}

int virtio_adsp_rec_time_end(int task_id, enum rec_time_pos pos)
{
	int ret = 0, cnt = 0, prev_cnt = 0;
	struct timespec64 ts;

	if (adsp_handler && adsp_handler->time_dbg_enable == 0)
		return -1;

	if (task_id < 0 || task_id >= TASK_SCENE_SIZE || pos >= REC_TIME_POS_NUM)
		return -1;
	if (kfifo_len(&vadsp_rec_ts[task_id][pos].st_queue) == 0) {
		pr_info("%s() kfifo is empty\n", __func__);
		return -1;
	}

	cnt = vadsp_rec_ts[task_id][pos].rec_cnt;
	if(cnt < 0)
		return -1;

	ret = kfifo_get(&vadsp_rec_ts[task_id][pos].st_queue, &ts);
	if (ret == 0) {
		pr_info("%s() kfifo is empty 1\n", __func__);
		return -1;
	}
	vadsp_rec_ts[task_id][pos].ts[cnt].st.tv_sec = ts.tv_sec;
	vadsp_rec_ts[task_id][pos].ts[cnt].st.tv_nsec = ts.tv_nsec;
	ktime_get_ts64(&(vadsp_rec_ts[task_id][pos].ts[cnt].et));

	prev_cnt = get_rec_time_prev_cnt(task_id, pos, cnt);
	if (prev_cnt != -1 && prev_cnt >=0)
		vadsp_rec_ts[task_id][pos].ts[cnt].interval_ns =
			vadsp_rec_ts[task_id][pos].ts[cnt].st.tv_nsec
			- vadsp_rec_ts[task_id][pos].ts[prev_cnt].st.tv_nsec;

	vadsp_rec_ts[task_id][pos].rec_cnt++;
	if (vadsp_rec_ts[task_id][pos].rec_cnt > MAX_REC_TIME_CNT - 1)
		vadsp_rec_ts[task_id][pos].rec_cnt = 0;

	return 0;
}

int virtio_adsp_check_time_start(int task_id, enum rec_time_pos pos)
{
	int cnt = 0, prev_cnt = 0;

	if (adsp_handler && adsp_handler->time_dbg_enable == 0)
		return -1;
	if (task_id < 0 || task_id >= TASK_SCENE_SIZE || pos >= REC_TIME_POS_NUM)
		return -1;
	cnt = vadsp_rec_ts[task_id][pos].rec_cnt;
	if(cnt < 0)
		return -1;
	ktime_get_ts64(&(vadsp_rec_ts[task_id][pos].ts[cnt].st));
	prev_cnt = get_rec_time_prev_cnt(task_id, pos, cnt);
	if (prev_cnt != -1 && prev_cnt >=0)
		vadsp_rec_ts[task_id][pos].ts[cnt].interval_ns =
			vadsp_rec_ts[task_id][pos].ts[cnt].st.tv_nsec
			- vadsp_rec_ts[task_id][pos].ts[prev_cnt].st.tv_nsec;

	return 0;
}

int virtio_adsp_check_time_end(int task_id,
	enum rec_time_pos pos)
{
	int ret = 0;
	int cnt = 0;

	if (adsp_handler && adsp_handler->time_dbg_enable == 0)
		return -1;

	if (task_id < 0 || task_id >= TASK_SCENE_SIZE ||pos >= REC_TIME_POS_NUM)
		return -1;
	cnt = vadsp_rec_ts[task_id][pos].rec_cnt;
	if(cnt < 0)
		return -1;

	ktime_get_ts64(&(vadsp_rec_ts[task_id][pos].ts[cnt].et));
	vadsp_rec_ts[task_id][pos].rec_cnt++;
	if (vadsp_rec_ts[task_id][pos].rec_cnt > MAX_REC_TIME_CNT - 1)
		vadsp_rec_ts[task_id][pos].rec_cnt = 0;

	return ret;
}

int virtio_adsp_print_time_info(void)
{
	int task = 0, pos = 0, cnt = 0;

	pr_info("%s() ++\n", __func__);
	for (task = 0; task < TASK_SCENE_SIZE; task++) {
		for (pos = 0; pos < REC_TIME_POS_NUM; pos++) {
			for (cnt = 0; cnt < MAX_REC_TIME_CNT; cnt++) {
				if (vadsp_rec_ts[task][pos].ts[cnt].st.tv_sec == 0)
					continue;
				pr_info("%s() task:%d, pos:%d, idx:%d rec_cnt:%d time: %ld.%09ldns -> %ld.%09ldns, interval:%09ldns used time: %09ldns",
						__func__, task, pos, cnt,
						vadsp_rec_ts[task][pos].rec_cnt,
						(long)vadsp_rec_ts[task][pos].ts[cnt].st.tv_sec,
						(long)vadsp_rec_ts[task][pos].ts[cnt].st.tv_nsec,
						(long)vadsp_rec_ts[task][pos].ts[cnt].et.tv_sec,
						(long)vadsp_rec_ts[task][pos].ts[cnt].et.tv_nsec,
						vadsp_rec_ts[task][pos].ts[cnt].interval_ns,
						vadsp_rec_ts[task][pos].ts[cnt].et.tv_nsec -
						vadsp_rec_ts[task][pos].ts[cnt].st.tv_nsec);
			}
		}
	}
	for (cnt = 0; cnt < MAX_REC_TIME_CNT; cnt++) {
		if (rec_irq_skip.ts[cnt].tv_sec == 0)
			continue;
		pr_info("%s() skip irq idx:%d skip_cnt:%d, rec_cnt:%d time: %ld.%09ldns ",
			__func__, cnt, rec_irq_skip.skip_cnt,
			rec_irq_skip.rec_cnt,
			(long)rec_irq_skip.ts[cnt].tv_sec,
			(long)rec_irq_skip.ts[cnt].tv_nsec);
	}
	pr_info("%s --\n", __func__);
	return 0;
}

void debug_work_handler(struct work_struct *work)
{
	virtio_adsp_print_time_info();
	adsp_handler->time_dbg_enable = 1;
}

void print_debug_info(struct virtio_adsp *vadsp)
{
	int ret = 0;

	if (vadsp == NULL || vadsp->time_dbg_enable == 0)
		return ;

	ret = queue_work(vadsp->dbg_work.work_queue,
		&vadsp->dbg_work.work);
	if (ret == true)
		adsp_handler->time_dbg_enable = 0;
}

void init_debug_work(struct virtio_adsp *vadsp)
{
	vadsp->dbg_work.work_queue =
		create_singlethread_workqueue("vadsp_dbg_wq");
	INIT_WORK(&vadsp->dbg_work.work, debug_work_handler);
}

void destroy_debug_work(struct virtio_adsp *vadsp)
{
	if (vadsp->dbg_work.work_queue) {
		destroy_workqueue(vadsp->dbg_work.work_queue);
		vadsp->dbg_work.work_queue = NULL;
	}
}

void host_work_handler(struct work_struct *work)
{
	struct recovery_work *host_work = container_of(work, struct recovery_work, work);

	if (adsp_virtdsp_cb.cb)
		adsp_virtdsp_cb.cb(host_work->cmd_type);
}

void init_host_work(struct virtio_adsp *vadsp)
{
	vadsp->recovery_work.work_queue =
		create_singlethread_workqueue("vadsp_recovert_event");
	INIT_WORK(&vadsp->recovery_work.work, host_work_handler);
}

void destroy_host_work(struct virtio_adsp *vadsp)
{
	if (vadsp->recovery_work.work_queue) {
		destroy_workqueue(vadsp->recovery_work.work_queue);
		vadsp->recovery_work.work_queue = NULL;
	}
}

static uint64_t get_cntpct(void)
{
	uint64_t cval;

	asm volatile("mrs %0, cntpct_el0" : "=r" (cval));
	return cval;
}

static struct mtk_adsp_task_attr *vadsp_get_adsp_task_attr(int adsp_id)
{
	if (adsp_id >= AUDIO_TASK_DAI_NUM || adsp_id < 0) {
		pr_info("%s(), adsp_id err: %d, return\n",
			__func__, adsp_id);
		return NULL;
	}

	return &adsp_task_attr[adsp_id];
}

static int vadsp_set_task_attr(int dsp_id, int task_enum, int param)
{

	struct mtk_adsp_task_attr *task_attr;

	if (dsp_id >= AUDIO_TASK_DAI_NUM) {
		pr_info("%s() dspid over max: %d\n", __func__, dsp_id);
		return -1;
	}

	if (task_enum >= ADSP_TASK_ATTR_NUM) {
		pr_info("%s() task_enum over max: %d\n", __func__, task_enum);
		return -1;
	}

	task_attr = vadsp_get_adsp_task_attr(dsp_id);
	if (!task_attr) {
		pr_info("%s() task_attr NULL dsp_id %d, return\n",
			__func__, dsp_id);
		return -1;
	}

	switch (task_enum) {
	case ADSP_TASK_ATTR_DEFAULT:
		task_attr->default_enable = param;
		break;
	case ADSP_TASK_ATTR_MEMDL:
		task_attr->afe_memif_dl = param;
		break;
	case ADSP_TASK_ATTR_MEMUL:
		task_attr->afe_memif_ul = param;
		break;
	case ADSP_TASK_ATTR_MEMREF:
		task_attr->afe_memif_ref = param;
		break;
	case ADSP_TASK_ATTR_RUNTIME:
		task_attr->runtime_enable = param;
		break;
	case ADSP_TASK_ATTR_REF_RUNTIME:
		task_attr->ref_runtime_enable = param;
		break;
	case ADSP_TASK_ATTR_PROPERTY:
		task_attr->task_property = param;
		break;
	case ADSP_TASK_ATTR_LATENCY_RATE:
		task_attr->task_latency.rate = param;
		break;
	case ADSP_TASK_ATTR_LATENCY_FRAME:
		task_attr->task_latency.frame = param;
		break;
	case ADSP_TASK_ATTR_LATENCY_IRQNUM:
		task_attr->task_latency.irq_num = param;
		break;
	case ADSP_TASK_ATTR_ADSP_LATENCY_SUPPORT:
		task_attr->task_latency.adsp_support_latency = param;
		break;
	case ADSP_TASK_ATTR_KERNEL_LATENCY_SUPPORT:
		task_attr->kernel_dynamic_config = param;
		break;
	}
	return 0;
}

void vadsp_dump_all_task_attr(void)
{
	int i = 0;
	struct mtk_adsp_task_attr *task_attr = NULL;

	for (i = 0; i < AUDIO_TASK_DAI_NUM; i++) {
		task_attr = vadsp_get_adsp_task_attr(i);
		if (!task_attr)
			continue;

		pr_info("[VADSP]%s() i:%d  name:%s, dl[%d] ul[%d] ref[%d] feature[%d] default[%d] runtime[%d]\n",
			__func__,
			i, dsp_task_dsp_name[i],
			task_attr->afe_memif_dl,
			task_attr->afe_memif_ul,
			task_attr->afe_memif_ref,
			task_attr->adsp_feature_id,
			task_attr->default_enable,
			task_attr->runtime_enable);
	}
}

static int vadsp_task_bind_core_init(struct device_node *of_node)
{
	int ret = 0;

	if (of_node) {
		ret = of_property_count_u32_elems(of_node, "audio-bind-vcpu");
		if (ret <= 0 || ret > MAX_CPU_NUM) {
			pr_info("get audio bind vcpu num failed:%d", ret);
			return 0;
		}
		audio_bind_cpu.cpu_num = ret;
		pr_info("VADSP]%s get audio bind vcpu :%d",  __func__, ret);

		ret = of_property_read_u32_array(of_node,
			"audio-bind-vcpu", audio_bind_cpu.cpu_array, audio_bind_cpu.cpu_num);
		if (ret) {
			pr_info("get audio bind vcpu array failed:%d", ret);
			return 0;
		}
	}
	return 0;
}

static int vadsp_pcm_taskattr_init(struct device_node *of_node)
{
	struct mtk_adsp_task_attr task_attr;
	int ret = 0;
	int dsp_id = 0;

	pr_info("[VADSP]%s\n", __func__);

	if (of_node) {
		for (dsp_id = 0; dsp_id < AUDIO_TASK_DAI_NUM; dsp_id++) {
			ret = of_property_read_u32_array(of_node,
						 dsp_task_dsp_name[dsp_id],
						 (unsigned int *)&task_attr,
						 SND_DSP_DTS_SIZE);
			if (ret != 0) {
				pr_info("%s error dsp_id[%d]\n",
					__func__, dsp_id);
				continue;
			}
			vadsp_set_task_attr(dsp_id,
				      ADSP_TASK_ATTR_DEFAULT,
				      task_attr.default_enable);
			vadsp_set_task_attr(dsp_id,
				      ADSP_TASK_ATTR_MEMDL,
				      task_attr.afe_memif_dl);
			vadsp_set_task_attr(dsp_id,
				      ADSP_TASK_ATTR_MEMUL,
				      task_attr.afe_memif_ul);
			vadsp_set_task_attr(dsp_id,
				      ADSP_TASK_ATTR_MEMREF,
				      task_attr.afe_memif_ref);
		}
		vadsp_dump_all_task_attr();

		ret = of_property_read_u32(of_node,
			"swdsp-smartpa-process-enable",
			&(task_attr.task_property));
		if (ret)
			task_attr.task_property = 0;
		vadsp_set_task_attr(AUDIO_TASK_PLAYBACK_ID, ADSP_TASK_ATTR_PROPERTY,
				task_attr.task_property);

		/* a2dp irq clear in adsp */
		ret = of_property_read_u32(of_node,
			"mtk-dsp-a2dp-irq",
			&(task_attr.task_property));
		if (ret)
			task_attr.task_property = 0;
		vadsp_set_task_attr(AUDIO_TASK_A2DP_ID, ADSP_TASK_ATTR_PROPERTY,
			      task_attr.task_property);

		ret = of_property_read_u32(of_node,
			"adsp-dynamic-buffer",
			&(task_attr.kernel_dynamic_config));
		if (ret)
			task_attr.kernel_dynamic_config = 0;
		vadsp_set_task_attr(AUDIO_TASK_PLAYBACK_ID, ADSP_TASK_ATTR_KERNEL_LATENCY_SUPPORT,
			      task_attr.kernel_dynamic_config);
		vadsp_task_bind_core_init(of_node);
	}
	return 0;
}

static int vadsp_get_taskid_by_afe_daiid(int afe_dai_id)
{
	int i = 0;
	struct mtk_adsp_task_attr *task_attr = NULL;

	for (i = 0; i < AUDIO_TASK_DAI_NUM; i++) {
		task_attr = vadsp_get_adsp_task_attr(i);
		if ((task_attr == NULL) || !(task_attr->default_enable & 0x1))
			continue;
		if (task_attr->afe_memif_dl == afe_dai_id ||
		     task_attr->afe_memif_ul == afe_dai_id)
			return i;
		else if (task_attr->afe_memif_ref == afe_dai_id)
			return i;
	}
	return -1;
}

int vadsp_get_dspscene_by_dspdaiid(int id)
{
	if (id < 0 || id >= AUDIO_TASK_DAI_NUM) {
		pr_info("%s(), id err: %d\n", __func__, id);
		return -1;
	}

	return dsp_task_scence[id];
}

static int nid_to_afe_dai_id[MT6881_MEMIF_NUM] = {
	[PLAYBACK_1_NID] = MT6881_MEMIF_DL0,
	[PLAYBACK_12_NID] = MT6881_MEMIF_DL1,
	[PLAYBACK_2_NID] = MT6881_MEMIF_DL2,
	[PLAYBACK_3_NID] = MT6881_MEMIF_DL3,
	[PLAYBACK_5_NID] = MT6881_MEMIF_DL4,
	[PLAYBACK_8_NID] = MT6881_MEMIF_DL5,
	[PLAYBACK_6_NID] = MT6881_MEMIF_DL6,
	[PLAYBACK_7_NID] = MT6881_MEMIF_DL7,
	[PLAYBACK_9_NID] = MT6881_MEMIF_DL8,
	[PLAYBACK_13_NID] = MT6881_MEMIF_DL44,
	[PLAYBACK_4_NID] = MT6881_MEMIF_DL45,
	[PLAYBACK_25_NID] = MT6881_MEMIF_DL46,
	[PLAYBACK_11_NID] = MT6881_MEMIF_DL_24CH,
	[PLAYBACK_48_NID] = MT6881_MEMIF_DL_48CH,
	[CAPTURE_1_NID] = MT6881_MEMIF_VUL9,
	[CAPTURE_2_NID] = MT6881_MEMIF_VUL1,
	[CAPTURE_3_NID] = MT6881_MEMIF_VUL0,
	[CAPTURE_4_NID] = MT6881_MEMIF_VUL3,
	[CAPTURE_5_NID] = MT6881_MEMIF_VUL7,
	[CAPTURE_6_NID] = MT6881_MEMIF_VUL4,
	[CAPTURE_7_NID] = MT6881_MEMIF_VUL2,
	[CAPTURE_8_NID] = MT6881_MEMIF_VUL5,
	[CAPTURE_9_NID] = MT6881_MEMIF_VUL_CM0,
	[CAPTURE_10_NID] = MT6881_MEMIF_VUL_CM1,
	[CAPTURE_11_NID] = MT6881_MEMIF_VUL10,
	[CAPTURE_12_NID] = MT6881_MEMIF_VUL6,
	[CAPTURE_13_NID] = MT6881_MEMIF_VUL38,
	[CAPTURE_MONO_1_NID] = MT6881_MEMIF_VUL8,
	[CAPTURE_MONO_2_NID] = MT6881_MEMIF_VUL37,
	[CAPTURE_ETDM_IN1_NID] = MT6881_MEMIF_ETDM_IN1,
	[CAPTURE_ETDM_IN2_NID] = MT6881_MEMIF_ETDM_IN2,
	[CAPTURE_ETDM_IN6_NID] = MT6881_MEMIF_ETDM_IN6,
};

static struct dsp_task_info nid_to_task_info[] = {
	[DSP_VOIP_NID] = {TASK_SCENE_VOIP, AUDIO_TASK_VOIP_ID, 1},
	[DSP_PRIMARY_NID] = {TASK_SCENE_PRIMARY, AUDIO_TASK_PRIMARY_ID, 1},
	[DSP_DEEPBUFFER_NID] = {TASK_SCENE_DEEPBUFFER, AUDIO_TASK_DEEPBUFFER_ID, 1},
	[DSP_PLAYBACK_FAST_NID] = {TASK_SCENE_FAST, AUDIO_TASK_FAST_ID, 1},
	[DSP_PLAYBACK_SPATIAL_NID] = {TASK_SCENE_SPATIALIZER, AUDIO_TASK_SPATIALIZER_ID, 1},
	[DSP_PLAYBACK_PLAYBACK_NID] = {TASK_SCENE_AUDPLAYBACK, AUDIO_TASK_PLAYBACK_ID, 0},
	[DSP_CAPTURE_UL1_NID] = {TASK_SCENE_CAPTURE_UL1, AUDIO_TASK_CAPTURE_UL1_ID, 0},
	[DSP_PLAYBACK_KTV_NID] = {TASK_SCENE_KTV, AUDIO_TASK_KTV_ID, 0},
	[DSP_CAPTURE_RAW_NID] = {TASK_SCENE_CAPTURE_RAW, AUDIO_TASK_CAPTURE_RAW_ID, 1},
	[DSP_PLAYBACK_FM_ADSP_NID] = {TASK_SCENE_FM_ADSP, AUDIO_TASK_FM_ADSP_ID, 0},
	[DSP_PLAYBACK_A2DP_NID] = {TASK_SCENE_A2DP, AUDIO_TASK_A2DP_ID, 1},
	[DSP_PLAYBACK_BLEDL_NID] = {TASK_SCENE_BLEDL, AUDIO_TASK_BLEDL_ID, 1},
	[DSP_CAPTURE_BLE_NID] = {TASK_SCENE_BLEUL, AUDIO_TASK_BLEUL_ID, 1},
	[DSP_PLAYBACK_BT_NID] = {TASK_SCENE_BTDL, AUDIO_TASK_BTDL_ID, 1},
	[DSP_CAPTURE_BT_NID] = {TASK_SCENE_BTUL, AUDIO_TASK_BTUL_ID, 1},
	[DSP_CAPTURE_PROCESS_NID] = {TASK_SCENE_UL_PROCESS, AUDIO_TASK_UL_PROCESS_ID, 1},
	[DSP_PLAYBACK_ECHOREF_NID] = {TASK_SCENE_ECHO_REF_DL, AUDIO_TASK_ECHO_REF_DL_ID, 1},
	[DSP_PLAYBACK_USB_NID] = {TASK_SCENE_USB_DL, AUDIO_TASK_USBDL_ID, 1},
	[DSP_CAPTURE_USB_NID] = {TASK_SCENE_USB_UL, AUDIO_TASK_USBUL_ID, 1},
	[DSP_CAPTURE_MDDL_NID] = {TASK_SCENE_MD_UL, AUDIO_TASK_MDUL_ID, 1},
	[DSP_PLAYBACK_MDUL_NID] = {TASK_SCENE_MD_DL, AUDIO_TASK_MDUL_ID, 1},
	[DSP_PLAYBACK_CALLDL_NID] = {TASK_SCENE_BLECALLDL, AUDIO_TASK_CALLDL_ID, 1},
	[DSP_CAPTURE_CALLUL_NID] = {TASK_SCENE_BLECALLUL, AUDIO_TASK_CALLUL_ID, 1},
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	[DSP_PLAYBACK_HFP_CLIENT_RX_NID] = {TASK_SCENE_HFP_CLIENT_RX, AUDIO_TASK_HFP_CLIENT_RX_ADSP_ID, 0},
	[DSP_PLAYBACK_HFP_CLIENT_TX_NID] = {TASK_SCENE_HFP_CLIENT_TX, AUDIO_TASK_HFP_CLIENT_TX_ADSP_ID, 0},
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	[DSP_PLAYBACK_ANC_NID] = {TASK_SCENE_ANC, AUDIO_TASK_ANC_ADSP_ID, 0},
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	[DSP_PLAYBACK_EXTSTREAM1_NID] = {TASK_SCENE_EXTSTREAM1, AUDIO_TASK_EXTSTREAM1_ADSP_ID, 0},
	[DSP_PLAYBACK_EXTSTREAM2_NID] = {TASK_SCENE_EXTSTREAM2, AUDIO_TASK_EXTSTREAM2_ADSP_ID, 0},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT)
	[DSP_PLAYBACK_SUB_PLAYBACK_NID] = {TASK_SCENE_SUB_PLAYBACK, AUDIO_TASK_SUB_PLAYBACK_ID, 0},
#endif
	[DSP_PLAYBACK_PLAYBACK0_NID] = {TASK_SCENE_PLAYBACK0, AUDIO_TASK_PLAYBACK0_ID, 1},
	[DSP_PLAYBACK_PLAYBACK1_NID] = {TASK_SCENE_PLAYBACK1, AUDIO_TASK_PLAYBACK1_ID, 1},
	[DSP_PLAYBACK_PLAYBACK2_NID] = {TASK_SCENE_PLAYBACK2, AUDIO_TASK_PLAYBACK2_ID, 1},
	[DSP_PLAYBACK_PLAYBACK3_NID] = {TASK_SCENE_PLAYBACK3, AUDIO_TASK_PLAYBACK3_ID, 1},
	[DSP_PLAYBACK_PLAYBACK4_NID] = {TASK_SCENE_PLAYBACK4, AUDIO_TASK_PLAYBACK4_ID, 1},
	[DSP_PLAYBACK_PLAYBACK5_NID] = {TASK_SCENE_PLAYBACK5, AUDIO_TASK_PLAYBACK5_ID, 1},
	[DSP_PLAYBACK_PLAYBACK6_NID] = {TASK_SCENE_PLAYBACK6, AUDIO_TASK_PLAYBACK6_ID, 1},
	[DSP_PLAYBACK_PLAYBACK7_NID] = {TASK_SCENE_PLAYBACK7, AUDIO_TASK_PLAYBACK7_ID, 1},
	[DSP_PLAYBACK_PLAYBACK8_NID] = {TASK_SCENE_PLAYBACK8, AUDIO_TASK_PLAYBACK8_ID, 1},
	[DSP_PLAYBACK_PLAYBACK9_NID] = {TASK_SCENE_PLAYBACK9, AUDIO_TASK_PLAYBACK9_ID, 1},
	[DSP_PLAYBACK_PLAYBACK10_NID] = {TASK_SCENE_PLAYBACK10, AUDIO_TASK_PLAYBACK10_ID, 1},
	[DSP_PLAYBACK_PLAYBACK11_NID] = {TASK_SCENE_PLAYBACK11, AUDIO_TASK_PLAYBACK11_ID, 1},
	[DSP_PLAYBACK_PLAYBACK12_NID] = {TASK_SCENE_PLAYBACK12, AUDIO_TASK_PLAYBACK12_ID, 1},
	[DSP_PLAYBACK_PLAYBACK13_NID] = {TASK_SCENE_PLAYBACK13, AUDIO_TASK_PLAYBACK13_ID, 1},
	[DSP_PLAYBACK_PLAYBACK14_NID] = {TASK_SCENE_PLAYBACK14, AUDIO_TASK_PLAYBACK14_ID, 1},
	[DSP_PLAYBACK_PLAYBACK15_NID] = {TASK_SCENE_PLAYBACK15, AUDIO_TASK_PLAYBACK15_ID, 1},
	[DSP_CAPTURE_MCH_NID] = {TASK_SCENE_CAPTURE_MCH, AUDIO_TASK_CAPTURE_MCH_ID, 0},
#endif
};

int get_taskid_by_nid(int id)
{
	if (id < DSP_VOIP_NID || id >= MAX_NID) {
		//pr_debug("%s() err id:%d\n", __func__, id);
		return -1;
	}
	return nid_to_task_info[id].task_scene;
}
EXPORT_SYMBOL(get_taskid_by_nid);

int is_adsp_pcm(int pcm_id)
{
	return (NID_TO_DSP_DAI_ID(pcm_id) < 0) ? false : true;
}
EXPORT_SYMBOL(is_adsp_pcm);

int is_adsp_fe_pcm(int pcm_id)
{
	if (pcm_id < DSP_VOIP_NID || pcm_id >= MAX_NID)
		return 0;
	return nid_to_task_info[pcm_id].is_fe;
}
EXPORT_SYMBOL(is_adsp_fe_pcm);

int is_adsp_memif(int direction, int pcm_id)
{
	int afe_daiid = -1;

	if (vadsp_runtime_enable && pcm_id >= 0 && pcm_id < MT6881_MEMIF_NUM)
		afe_daiid = nid_to_afe_dai_id[pcm_id];
	else
		return 0;
	return vadsp_get_taskid_by_afe_daiid(afe_daiid) >= 0;
}
EXPORT_SYMBOL(is_adsp_memif);

/* vadsp kcontrols*/
static int set_vadsp_runtime_enable(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	vadsp_runtime_enable = val;
	return 0;
}

static int get_vadsp_runtime_enable(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = vadsp_runtime_enable;
	return 0;
}

static const struct snd_kcontrol_new vadsp_kcontrols[] = {
	SOC_SINGLE_EXT("vadsp_runtime_en", SND_SOC_NOPM, 0, 0xff, 0,
		       get_vadsp_runtime_enable, set_vadsp_runtime_enable)
};

static int vadsp_card_add_controls(struct snd_card *card,
		const struct snd_kcontrol_new *controls, int num_controls)
{
	int i;

	for (i = 0; i < num_controls; i++) {
		const struct snd_kcontrol_new *control = &controls[i];
		struct snd_kcontrol_new template;
		struct snd_kcontrol *kcontrol;
		int err;

		memcpy(&template, control, sizeof(template));
		template.index = 0;

		kcontrol = snd_ctl_new1(&template, card);
		if (!kcontrol)
			return -ENOMEM;

		err = snd_ctl_add(card, kcontrol);
		if (err < 0) {
			pr_err("[VADSP] Failed to add kcontrol %s: %d\n",
				control->name, err);
			return err;
		}
	pr_info("[VADSP] add kcontrol %s\n", control->name);
	}

	return 0;
}

static uint64_t  vadsp_get_time_ns(void)
{
	uint64_t cntvct = 0;
	uint64_t cntfrq = 0;
	uint64_t time_ns = 0;

	cntvct = __arch_counter_get_cntvct();
	cntfrq = arch_timer_get_cntfrq();
	time_ns = cntvct * 1000ULL / (cntfrq / 1000000ULL);

	return time_ns;
}

int vadsp_build_devs(struct snd_card *card)
{
	int rt = 0;
	//add vadsp kcontrols
	rt = vadsp_card_add_controls(card, vadsp_kcontrols,  ARRAY_SIZE(vadsp_kcontrols));

	return rt;
}
EXPORT_SYMBOL(vadsp_build_devs);
/* end vadsp kcontrols */

void get_adsp_ring_dram(int task_id, unsigned long long *phy_addr,
	unsigned long long *size)
{
	if (task_id < 0 || task_id >= TASK_SCENE_SIZE) {
		pr_info("[VADSP]%s() taskid:%d\n", __func__, task_id);
		return ;
	}

	*phy_addr = task_ring_mem[task_id].phy_addr;
	*size = task_ring_mem[task_id].size;
	pr_info("[VADSP]%s() taskid:%d, phy_addr:0x%llx, size:0x%llx\n", __func__,
			task_id, *phy_addr, *size);
}

void get_adsp_dtoa_dram(int task_id, unsigned long long *phy_addr,
	unsigned long long *size)
{
	if (task_id < 0 || task_id >= TASK_SCENE_SIZE)
		return ;

	*phy_addr = task_dtoa_mem[task_id].phy_addr;
	*size = task_dtoa_mem[task_id].size;
}

void get_adsp_atod_dram(int task_id, unsigned long long *phy_addr,
	unsigned long long *size)
{
	if (task_id < 0 || task_id >= TASK_SCENE_SIZE)
		return ;

	*phy_addr = task_atod_mem[task_id].phy_addr;
	*size = task_atod_mem[task_id].size;
}

unsigned char *vadsp_phy_to_virt(unsigned long long phy_addr)
{
	if (adsp_shm.region != NULL && phy_addr > 0 && adsp_rsv_mem.phy_addr > 0
		&& adsp_rsv_mem.size > 0 && phy_addr > adsp_rsv_mem.phy_addr
		&& phy_addr < adsp_rsv_mem.phy_addr + adsp_rsv_mem.size)
		return adsp_shm.region + phy_addr - adsp_rsv_mem.phy_addr;

	pr_info("[VADSP]%s() region:0x%llx, rsv phy:0x%llx, size:0x%llx, phy:0x%llx",
		__func__, (uint64_t)adsp_shm.region, adsp_rsv_mem.phy_addr, adsp_rsv_mem.size,
		phy_addr);
	return NULL;
}

void set_adsp_shm_region(void *region, unsigned long long size)
{
	adsp_shm.region = region;
	adsp_shm.size = size;
}

void get_adsp_audio_reserve_dram(unsigned long long *phy_addr,
	unsigned long long *size)
{
	*phy_addr = adsp_rsv_mem.phy_addr;
	*size = adsp_rsv_mem.size;
}

void vadsp_irq_event_handler(struct adsp_event_cb_data *data)
{
	struct virtio_snd *snd = NULL;
	// struct virtio_pcm *vpcm;
	// struct virtio_pcm_stream *vs;
	struct virtio_pcm_substream *vss;
	int ret = 0;
	int i;
	int task_id;
	uint64_t start_time = 0;
	uint64_t end_time = 0;
	struct elapse_info info;

	if (data == NULL)
		return;
	if (data->irq.task > 0 && data->irq.task < TASK_SCENE_SIZE)
		irq_cnt[data->irq.task]++;

	snd = data->data;
	if (snd == NULL)
		return;
	if (data->irq.task >= TASK_SCENE_SIZE)
		return;

	start_time = vadsp_get_time_ns();
	adsp_trace(LOCAL_DBG, "[VADSP] %s() task:%llu, core:%d, xrun:%d, ptr:0x%x, irq_cnt:%llu, cntpct:%llu\n",
					__func__, data->irq.task,
					data->irq.core, data->irq.xrun, data->irq.ptr,
					irq_cnt[data->irq.task], get_cntpct());
	for (i = 0; i < snd->nsubstreams; ++i) {
		vss = &snd->substreams[i];

		task_id = get_taskid_by_nid(vss->nid);
		if (task_id < 0 || data->irq.task != task_id || task_id >= TASK_SCENE_SIZE)
			continue;

		cur_ptr[task_id] = data->irq.ptr;

		if (vss->xfer_enabled) {
			info.irq_evt.core = data->irq.core;
			info.irq_evt.task = data->irq.task;
			info.irq_evt.xrun = data->irq.xrun;
			info.irq_evt.ptr = data->irq.ptr;
			info.substream = vss->substream;
			info.irq_cnt = irq_cnt[data->irq.task];
			ret = kfifo_put(&snd->elapse_info_queue, info);
			adsp_trace(LOCAL_DBG,
				"[VADSP] %s() task:%llu, core:%d, xrun:%d, ptr:0x%x\n",
				__func__, info.irq_evt.task,
				info.irq_evt.core, info.irq_evt.xrun, info.irq_evt.ptr);
			adsp_trace(LOCAL_DBG,
				"[VADSP] %s() substream:0x%llx, len:%d, irq_cnt:%llu, cntpct:%llu\n",
				__func__,
				(uint64_t)info.substream,
				kfifo_len(&snd->elapse_info_queue),
				irq_cnt[data->irq.task], get_cntpct());
			if (ret != 0)
				complete(&snd->wait_info_completion);

			end_time = vadsp_get_time_ns();
			if (adsp_mbrain_cb.cb)
				adsp_mbrain_cb.cb(0, (uint32_t)info.irq_evt.task,
						VADSP_IRQ_EVENT_HANDLER, start_time, end_time, irq_cnt[data->irq.task]);
		}
	}
}

void vadsp_ipi_event_handler(struct adsp_event_cb_data *data)
{
	if (adsp_ipi_cb.cb)
		adsp_ipi_cb.cb((struct adsp_ipi_event *)&data->ipi_info);
}

void vadsp_mem_event_handler(struct adsp_event_cb_data *data)
{
	pr_info("[VADSP]%s(), type %u task:0x%llx, phy:0x%llx, size:0x%llx\n",
			 __func__, data->mem.type, data->mem.task, data->mem.phy_addr,
			 data->mem.size);
	switch (data->mem.type) {
	case MEM_TYPE_A2D:
		if (data->mem.task >= TASK_SCENE_SIZE)
			return ;
		task_atod_mem[data->mem.task].phy_addr = data->mem.phy_addr;
		task_atod_mem[data->mem.task].size = data->mem.size;
	break;
	case MEM_TYPE_D2A:
		if (data->mem.task >= TASK_SCENE_SIZE)
			return ;
		task_dtoa_mem[data->mem.task].phy_addr = data->mem.phy_addr;
		task_dtoa_mem[data->mem.task].size = data->mem.size;
	break;
	case MEM_TYPE_RING:
		if (data->mem.task >= TASK_SCENE_SIZE)
			return ;
		task_ring_mem[data->mem.task].phy_addr = data->mem.phy_addr;
		task_ring_mem[data->mem.task].size = data->mem.size;
	break;
	case MEM_TYPE_RSV:
		adsp_rsv_mem.phy_addr = data->mem.phy_addr;
		adsp_rsv_mem.size = data->mem.size;
	break;
	};
}

static void vadsp_user_event_notify(struct adsp_event_cb_data *data)
{
	uint32_t event = data->recovery_evt.cmd_type;

	adsp_handler->recovery_work.cmd_type = event;
	queue_work(adsp_handler->recovery_work.work_queue,
		&adsp_handler->recovery_work.work);
}

int set_task_cpu_affinity(struct task_struct *task)
{
	struct cpumask new_mask;
	int cpu_index = 0;

	if (audio_bind_cpu.cpu_num < 1 || audio_bind_cpu.cpu_num > MAX_CPU_NUM) {
		pr_info("[vadsp]Invalid CPU number\n");
		return 0;
	}

	cpumask_clear(&new_mask);
	for (cpu_index = 0; cpu_index < audio_bind_cpu.cpu_num; cpu_index++)
		cpumask_set_cpu(audio_bind_cpu.cpu_array[cpu_index], &new_mask);

	if (set_cpus_allowed_ptr(task, &new_mask) != 0)
		pr_info("[vadsp]Failed to set CPU affinity\n");

	return 0;
}
EXPORT_SYMBOL(set_task_cpu_affinity);

void vadsp_event_handler(struct adsp_event_cb_data *data)
{
	if (data == NULL) {
		pr_info("[VADSP]%s() data is NULL\n", __func__);
		return;
	}
	switch (data->type) {
	case ADSP_EVT_IRQ:
		return vadsp_irq_event_handler(data);
	break;
	case ADSP_EVT_MEM:
		pr_info("[VADSP]%s() mem event\n", __func__);
		return vadsp_mem_event_handler(data);
	case ADSP_EVT_IPI:
		return vadsp_ipi_event_handler(data);
	case ADSP_EVT_RECOVERY:
		return vadsp_user_event_notify(data);
	default:
		return;
	}
}
EXPORT_SYMBOL(vadsp_event_handler);

void virtio_register_adsp_event_cb(adsp_event_cb_func cb,
				void *data)
{
	adsp_event_cb.cb = cb;
	adsp_event_cb.data = data;

	pr_info("[VADSP]%s() L%d cb:0x%llx, data:0x%llx!!\n",
			__func__, __LINE__, (uint64_t)adsp_event_cb.cb, (uint64_t)adsp_event_cb.data);
}
EXPORT_SYMBOL(virtio_register_adsp_event_cb);

void virtio_unregister_adsp_event_cb(void)
{
	adsp_event_cb.cb = NULL;
	pr_info("[VADSP]%s() L%d\n", __func__, __LINE__);
}
EXPORT_SYMBOL(virtio_unregister_adsp_event_cb);

void virtio_register_adsp_ipi_cb(adsp_ipi_cb_func cb)
{
	adsp_ipi_cb.cb = cb;
	pr_info("[VADSP]%s() L%d\n", __func__, __LINE__);
}
EXPORT_SYMBOL(virtio_register_adsp_ipi_cb);

void virtio_unregister_adsp_ipi_cb(void)
{
	adsp_ipi_cb.cb = NULL;
	pr_info("[VADSP]%s() L%d\n", __func__, __LINE__);
}
EXPORT_SYMBOL(virtio_unregister_adsp_ipi_cb);

void virtio_register_adsp_mbrain_cb(adsp_mbrain_cb_func cb)
{
	adsp_mbrain_cb.cb = cb;
	pr_info("[VADSP]%s() L%d\n", __func__, __LINE__);
}
EXPORT_SYMBOL(virtio_register_adsp_mbrain_cb);

void virtio_unregister_adsp_mbrain_cb(void)
{
	adsp_mbrain_cb.cb = NULL;
	pr_info("[VADSP]%s() L%d\n", __func__, __LINE__);
}
EXPORT_SYMBOL(virtio_unregister_adsp_mbrain_cb);

void virtio_register_adsp_virtdsp_cb(adsp_virtdsp_cb_func cb)
{
	adsp_virtdsp_cb.cb = cb;
	pr_info("[VADSP]%s() L%d\n", __func__, __LINE__);
}
EXPORT_SYMBOL(virtio_register_adsp_virtdsp_cb);

void virtio_unregister_adsp_virtdsp_cb(void)
{
	adsp_virtdsp_cb.cb = NULL;
	pr_info("[VADSP]%s() L%d\n", __func__, __LINE__);
}
EXPORT_SYMBOL(virtio_unregister_adsp_virtdsp_cb);

static int elapse_task_loop(void *pvParameters)
{
	struct virtio_snd *snd = (struct virtio_snd  *)pvParameters;
	int ret = 0;
	struct elapse_info info;
	uint64_t start_time = 0;
	uint64_t end_time = 0;

	if (snd == NULL)
		return -1;

	while (!kthread_should_stop()) {
		//wait here;
		ret = wait_for_completion_interruptible(&snd->wait_info_completion);
		if (ret != 0) {
			pr_info("[VADSP]%s() wait_for_completion_interruptible ret:%d\n",
					__func__, ret);
			continue;
		}
		if (kthread_should_stop())
			break;

		start_time = vadsp_get_time_ns();
		memset(&info, 0, sizeof(info));
		ret = kfifo_get(&snd->elapse_info_queue, &info);
		if (ret == 0)
			continue;

		adsp_trace(LOCAL_DBG,
			"[VADSP] %s() task:%llu, core:%d, xrun:%d, ptr:0x%x, substream:0x%llx len:%d, irq_cnt:%llu, cntpct:%llu\n",
			__func__, info.irq_evt.task,
			info.irq_evt.core, info.irq_evt.xrun, info.irq_evt.ptr, (uint64_t)info.substream,
			kfifo_len(&snd->elapse_info_queue),
			info.irq_cnt, get_cntpct());
		snd_pcm_period_elapsed(info.substream);
		end_time = vadsp_get_time_ns();
		if (adsp_mbrain_cb.cb)
			adsp_mbrain_cb.cb(0, info.irq_evt.task, ELAPSE_TASK_LOOP, start_time, end_time, info.irq_cnt);
	}
	pr_info("[VADSP]%s() exit\n", __func__);
	return 0;
}

static int set_elapse_task_priority(struct task_struct *task, int policy,
	int prio)
{
	struct sched_param param;

	param.sched_priority = prio;
	return sched_setscheduler(task, policy, &param);
}

void create_elapse_task(void *snd_data)
{
	struct virtio_snd *snd = NULL;
	int ret = 0;

	snd = (struct virtio_snd *)snd_data;
	INIT_KFIFO(snd->elapse_info_queue);
	init_completion(&snd->wait_info_completion);

	snd->elapse_task = kthread_create(elapse_task_loop, snd, "%s", "elapse_task");
	ret = PTR_ERR_OR_ZERO(snd->elapse_task);
	if (ret)
		return ;

	set_elapse_task_priority(snd->elapse_task, SCHED_FIFO, 50);
	set_task_cpu_affinity(snd->elapse_task);
	wake_up_process(snd->elapse_task);
}
EXPORT_SYMBOL(create_elapse_task);

void destroy_elapse_task(void *snd_data)
{
	struct virtio_snd *snd = NULL;

	snd = (struct virtio_snd *)snd_data;
	if (snd && snd->elapse_task) {
		complete(&snd->wait_info_completion);
		kthread_stop(snd->elapse_task);
		snd->elapse_task = NULL;
	}
}
EXPORT_SYMBOL(destroy_elapse_task);

static void handle_adsp_event(struct adsp_event *evt)
{
	struct adsp_event_cb_data data;

	if (evt == NULL) {
		adsp_trace(LOCAL_DBG, "[VADSP]%s() L%d evt is NULL!!\n", __func__, __LINE__);
		return;
	}

	data.type = evt->type;
	switch (evt->type) {
	case ADSP_EVT_IRQ:
		data.irq.task = evt->irq_evt.task;
		data.irq.core = evt->irq_evt.core;
		data.irq.xrun = evt->irq_evt.xrun;
		data.irq.ptr = evt->irq_evt.ptr;
		data.data = adsp_event_cb.data;
		adsp_trace(LOCAL_DBG, "[VADSP]%s() L%d task:%llu, core:%d, xrun:%d, ptr:0x%x!!\n",
			__func__, __LINE__, evt->irq_evt.task, evt->irq_evt.core, evt->irq_evt.xrun,
			evt->irq_evt.ptr);
	break;
	case ADSP_EVT_MEM:
		data.mem.task = evt->mem_evt.task;
		data.mem.phy_addr = evt->mem_evt.phy_addr;
		data.mem.size = evt->mem_evt.size;
		data.mem.type = evt->mem_evt.type;
		adsp_trace(LOCAL_DBG, "[VADSP]%s() L%d task:%llu, phy:0x%llx, size:%llu, type:%d, cb:0x%llx!!\n",
			__func__, __LINE__, evt->mem_evt.task, evt->mem_evt.phy_addr, evt->mem_evt.size,
			evt->mem_evt.type, (uint64_t)adsp_event_cb.cb);
	break;
	case ADSP_EVT_IPI:
		data.ipi_info.phy_addr = evt->ipi_evt.phy_addr;
		data.ipi_info.buffer_size = evt->ipi_evt.buffer_size;
		data.ipi_info.write_offset = evt->ipi_evt.write_offset;
		data.ipi_info.write_size = evt->ipi_evt.write_size;
		data.ipi_info.phy_rp_addr = evt->ipi_evt.phy_rp_addr;
		data.data = adsp_event_cb.data;
		adsp_trace(LOCAL_DBG,
			"[VADSP]%s() L%d phy_base:0x%llx, size:0x%llx, write:0x%llx, w_size:0x%llx, rp_base:0x%llx\n",
			__func__, __LINE__, evt->ipi_evt.phy_addr,
			evt->ipi_evt.buffer_size, evt->ipi_evt.write_offset,
			evt->ipi_evt.write_size, evt->ipi_evt.phy_rp_addr);
	break;
	case ADSP_EVT_RECOVERY:
		data.recovery_evt.cmd_type = evt->recovery_evt.cmd_type;
		data.data = adsp_event_cb.data;
		adsp_trace(LOCAL_DBG, "[VADSP]%s() L%d ADSP_EVT_RECOVERY cmd_type:0x%x\n",
			__func__, __LINE__, evt->recovery_evt.cmd_type);
	break;
	default:
		return ;
	};
	if (adsp_event_cb.cb)
		adsp_event_cb.cb(&data);
}

static int virtio_adsp_queue_evt_buffer(struct virtqueue *vq,
					struct adsp_event *evt)
{
	int ret;
	struct scatterlist sg;

	sg_init_one(&sg, evt, sizeof(struct adsp_event));

	ret = virtqueue_add_inbuf(vq, &sg, 1, evt, GFP_ATOMIC);

	if (ret)
		return ret;

	virtqueue_kick(vq);

	return 0;
}

void adsp_event_done(struct virtqueue *vq)
{
	struct virtio_adsp *vadsp = vq->vdev->priv;
	struct adsp_event *evt;
	unsigned long flags;
	unsigned int len;
	bool notify = false;
	uint64_t cnt = get_cntpct();
	uint64_t start_time = 0;
	uint64_t end_time = 0;

	start_time = vadsp_get_time_ns();
	adsp_trace(LOCAL_DBG, "[VADSP]%s() L%d cntpct:%llu", __func__,
			__LINE__, cnt);

	spin_lock_irqsave(&vadsp->evt_lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((evt = virtqueue_get_buf(vq, &len)) != NULL) {
			handle_adsp_event(evt);
			virtio_adsp_queue_evt_buffer(vq, evt);
		}
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));

	if (virtqueue_kick_prepare(vq))
		notify = true;

	spin_unlock_irqrestore(&vadsp->evt_lock, flags);

	if (notify)
		virtqueue_notify(vq);

	end_time = vadsp_get_time_ns();
	if (adsp_mbrain_cb.cb)
		adsp_mbrain_cb.cb(0, (uint32_t)0xff,
				ADSP_EVENT_DONE, start_time, end_time, 0);
}

static int submit_adsp_request(struct virtqueue *vq,
			       struct virtio_adsp_req *vreq)
{
	struct scatterlist req_sg, res_sg,
		*sgs[MAX_ADSP_IOV_SIZE + 2];
	unsigned int num_out = 0, num_in = 0;

	sg_init_one(&req_sg, &vreq->req, sizeof(vreq->req));
	sgs[num_out++] = &req_sg;

	sg_init_one(&res_sg, &vreq->res, sizeof(vreq->res));
	sgs[num_out + num_in++] = &res_sg;
	adsp_trace(LOCAL_DBG, "[VADSP] %s() req task_id:%llu\n", __func__, vreq->req.ptr_req.task_id);

	return virtqueue_add_sgs(vq, sgs, num_out, num_in, vreq, GFP_ATOMIC);
}

static int submit_req_and_wait_result(struct virtio_adsp_req *vreq)
{
	unsigned long flags;
	bool notify = false;
	int err;
	struct virtqueue *vq = adsp_handler->vqs[VIRTIO_ADSP_VQ_REQ];

	if (adsp_handler == NULL || vreq == NULL) {
		pr_info("%s: Invalid parameters adsp_handler:%p\n", __func__, adsp_handler);
		return -1;
	}

	spin_lock_irqsave(&adsp_handler->lock, flags);
	err = submit_adsp_request(vq, vreq);
	if (err) {
		spin_unlock_irqrestore(&adsp_handler->lock, flags);
		err = -ENOMEM;
		goto out;
	}

	if (virtqueue_kick_prepare(vq))
		notify = true;
	spin_unlock_irqrestore(&adsp_handler->lock, flags);

	if (notify)
		virtqueue_notify(vq);

	wait_for_completion(&vreq->done);
	err = vreq->res.ret;

	adsp_trace(LOCAL_DBG, "[VADSP] %s() req task_id:%llu, ret:%d\n", __func__, vreq->req.ptr_req.task_id, err);
out:
	return err;
}

void virtio_adsp_reset_pointer(uint64_t task_id)
{
	cur_ptr[task_id] = 0;
	pr_info("[VADSP]reset pointer\n");
}
EXPORT_SYMBOL(virtio_adsp_reset_pointer);

int32_t virtio_adsp_get_pointer(uint64_t task_id)
{
	int32_t ret = 0;

	ret = cur_ptr[task_id];
	adsp_trace(LOCAL_DBG, "[VADSP] %s() task_id:%llu, ret:%d\n",
			__func__, task_id, ret);
	if (ret == -1) {
		pr_info("[VADSP] %s() xrun task_id:%llu, ret:%d\n",
			__func__, task_id, ret);
	}
	return ret;
}
EXPORT_SYMBOL(virtio_adsp_get_pointer);

uint64_t cpy_cnt[TASK_SCENE_SIZE] = {0};
uint64_t get_cpy_cnt(int task_scene)
{
	if (task_scene > 0 && task_scene < TASK_SCENE_SIZE)
		return cpy_cnt[task_scene];
	return 0;
}
uint64_t ul_cpy_cnt[TASK_SCENE_SIZE] = {0};
uint64_t get_ul_cpy_cnt(int task_scene)
{
	if (task_scene > 0 && task_scene < TASK_SCENE_SIZE)
		return ul_cpy_cnt[task_scene];
	return 0;
}

int32_t virtio_adsp_copy_pcm_dl(uint64_t task_id, uint64_t phy_addr, uint64_t copy_size)
{
	int32_t ret = 0;
	struct virtio_adsp_req *vreq = kzalloc(sizeof(*vreq), GFP_KERNEL);
	uint64_t cntpct_b, cntpct_e;
	uint64_t start_time = 0;
	uint64_t end_time = 0;

	if (!vreq)
		return -ENOMEM;

	if (task_id > 0 && task_id < TASK_SCENE_SIZE)
		cpy_cnt[task_id]++;

	virtio_adsp_check_time_start(task_id, REC_TIME_POS_CPY);
	cntpct_b = get_cntpct();
	start_time = vadsp_get_time_ns();

	vreq->req.copy_req.task_id = task_id;
	vreq->req.copy_req.phy_addr = phy_addr;
	vreq->req.copy_req.copy_size = copy_size;
	vreq->req.type = ADSP_OPS_COPY_DL;
	init_completion(&vreq->done);

	ret = submit_req_and_wait_result(vreq);
	kfree(vreq);
	cntpct_e = get_cntpct();
	end_time = vadsp_get_time_ns();
	if (adsp_mbrain_cb.cb) {
		adsp_mbrain_cb.cb(0, (uint32_t)task_id,
				VIRTIO_ADSP_COPY_PCM_DL, start_time, end_time, get_cpy_cnt(task_id));
	}

	adsp_trace(LOCAL_DBG,
		"[VADSP]%s() taskid:%llu, phy_addr:0x%llx, copy_size:%llu, cpy_cnt:%llu ret:%d, cntpct_b:%llu, cntpct_diff:%llu\n",
		__func__,
		task_id, phy_addr, copy_size, get_cpy_cnt(task_id), ret, cntpct_b, cntpct_e - cntpct_b);
	virtio_adsp_check_time_end(task_id, REC_TIME_POS_CPY);

	return ret;
}
EXPORT_SYMBOL(virtio_adsp_copy_pcm_dl);

int32_t virtio_adsp_copy_pcm_ul(uint64_t task_id, uint64_t phy_addr, uint64_t copy_size)
{
	int32_t ret = 0;
	struct virtio_adsp_req *vreq = kzalloc(sizeof(*vreq), GFP_KERNEL);
	uint64_t cntpct_b, cntpct_e;
	uint64_t start_time = 0;
	uint64_t end_time = 0;

	if (!vreq)
		return -ENOMEM;

	if (task_id > 0 && task_id < TASK_SCENE_SIZE)
		ul_cpy_cnt[task_id]++;

	cntpct_b = get_cntpct();
	start_time = vadsp_get_time_ns();

	vreq->req.copy_req.task_id = task_id;
	vreq->req.copy_req.phy_addr = phy_addr;
	vreq->req.copy_req.copy_size = copy_size;
	vreq->req.type = ADSP_OPS_COPY_UL;
	init_completion(&vreq->done);

	ret = submit_req_and_wait_result(vreq);
	kfree(vreq);
	cntpct_e = get_cntpct();
	end_time = vadsp_get_time_ns();
	if (adsp_mbrain_cb.cb && task_id < TASK_SCENE_SIZE) {
		adsp_mbrain_cb.cb(0, (uint32_t)task_id,
				VIRTIO_ADSP_COPY_PCM_UL, start_time, end_time, ul_cpy_cnt[task_id]);
	}

	adsp_trace(LOCAL_DBG,
		"[VADSP]%s() taskid:%llu, phy_addr:0x%llx, copy_size:%llu, ret:%d, cntpct_b:%llu, cntpct_diff:%llu\n",
		 __func__,
		task_id, phy_addr, copy_size, ret, cntpct_b, cntpct_e - cntpct_b);
	return ret;
}
EXPORT_SYMBOL(virtio_adsp_copy_pcm_ul);

int32_t virtio_adsp_pcm_trigger(uint64_t type, uint64_t id, uint64_t cmd)
{
	int32_t ret = 0;
	struct virtio_adsp_req *vreq = kzalloc(sizeof(*vreq), GFP_KERNEL);

	if (!vreq)
		return -ENOMEM;
	vreq->req.trigger_req.type = type;
	vreq->req.trigger_req.id = id;
	vreq->req.trigger_req.cmd = cmd;
	vreq->req.type = ADSP_OPS_TRIGGER;
	init_completion(&vreq->done);

	ret = submit_req_and_wait_result(vreq);
	kfree(vreq);
	adsp_trace(LOCAL_DBG, "[VADSP]%s() type:%llu, id:%llu, cmd:%llu ret:%d\n", __func__,
				type, id, cmd, ret);
	return ret;
}
EXPORT_SYMBOL(virtio_adsp_pcm_trigger);
//static uint64_t ptr_resp_cnt[TASK_SCENE_SIZE] = {0};
void adsp_request_done(struct virtqueue *vq)
{
	struct virtio_adsp *vadsp = vq->vdev->priv;
	struct virtio_adsp_req *vreq = NULL;
	unsigned long flags;
	unsigned int len;
	bool notify = false;
	uint64_t start_time = 0;
	uint64_t end_time = 0;
	uint32_t task_id = 0;
	uint32_t req_type = 0;

	start_time = vadsp_get_time_ns();
	spin_lock_irqsave(&vadsp->lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((vreq = virtqueue_get_buf(vq, &len)) != NULL) {
			req_type = vreq->req.type;
			if (req_type == ADSP_OPS_COPY_DL)
				task_id = vreq->req.copy_req.task_id;

			complete(&vreq->done);
			if (req_type == ADSP_OPS_COPY_DL) {
				end_time = vadsp_get_time_ns();
				if (adsp_mbrain_cb.cb)
					adsp_mbrain_cb.cb(0, task_id,
						ADSP_REQUEST_DONE, start_time, end_time, 0);
			}
		}
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));

	if (virtqueue_kick_prepare(vq))
		notify = true;
	spin_unlock_irqrestore(&vadsp->lock, flags);

	if (notify)
		virtqueue_notify(vq);
}

int fill_adsp_event(struct virtio_adsp *vadsp)
{
	int i;
	struct adsp_event *evts;
	struct virtqueue *vq = vadsp->vqs[VIRTIO_ADSP_VQ_EVT];
	size_t nr_evts = vq->num_free;
	int ret;

	evts = kmalloc_array(nr_evts, sizeof(*evts), GFP_KERNEL);
	if (!evts)
		return -ENOMEM;

	for (i = 0; i < nr_evts; i++) {
		ret = virtio_adsp_queue_evt_buffer(vq, &evts[i]);
		if (ret)
			return ret;
	}

	pr_info("[VADSP] %s() nr_evts:%lu, i:%d\n", __func__, nr_evts, i);

	return 0;
}

static struct dentry *debugfs_dentry;

static char virtio_adsp_debug_buffer[4095] = {0};
static unsigned int  buf_offset;
static int temp_len;
#define PRINT_LOG(fmt, arg...) pr_info("[VADSP]"fmt, ##arg)
#define VADSP_DEBUG_PRINTF(fmt, arg...)  \
do { \
	PRINT_LOG(fmt, ##arg); \
	if (buf_offset < (sizeof(virtio_adsp_debug_buffer) - 1)) { \
		int available_space = sizeof(virtio_adsp_debug_buffer) - buf_offset - 1; \
		temp_len = \
		snprintf(virtio_adsp_debug_buffer + buf_offset, available_space, fmt, ##arg); \
		if (temp_len < 0) { \
			return -ENOMEM; \
		} else { \
			buf_offset += temp_len; \
		} \
		virtio_adsp_debug_buffer[buf_offset] = 0;\
	} \
} while (0)

static ssize_t virtio_adsp_debug_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;

	n = strlen(virtio_adsp_debug_buffer);
	if(n < 0)
		return -ENOMEM;
	virtio_adsp_debug_buffer[n++] = 0;

	return simple_read_from_buffer(ubuf, count, ppos,
			virtio_adsp_debug_buffer, n);
}

static unsigned int virtio_adsp_debug_get_ptr(
	char *str_p)
{
	uint64_t task_id;
	int ret = 0;

	if (strncmp(str_p, "help", 4) == 0) {
		VADSP_DEBUG_PRINTF("p:[task_id]\n");
		return 0;
	}
	ret = sscanf(str_p, "%llu", &task_id);
	if (ret < 0)
		VADSP_DEBUG_PRINTF("sscanf fail\n");

	VADSP_DEBUG_PRINTF("%s task_id = %llu\n", __func__, task_id);
#if LOCAL_ADSP_TEST
	virtio_adsp_get_pointer(task_id);
#endif
	return 0;
}

static unsigned int virtio_adsp_debug_trace_level(
	char *str_p)
{
	int ret = 0;
	int level = LOCAL_ERR;

	if (strncmp(str_p, "help", 4) == 0) {
		VADSP_DEBUG_PRINTF("l:[trace_level]\n");
		return 0;
	}
	ret = sscanf(str_p, "%d", &level);
	if (ret < 0)
		VADSP_DEBUG_PRINTF("sscanf fail\n");

	VADSP_DEBUG_PRINTF("%s trace_level = %d --> %d\n", __func__,
			trace_level, level);
	trace_level = level;

	return ret;
}

static unsigned int adsp_debug_write_task_ad_mem(
	char *str_p)
{
	int ret = 0;
	uint64_t task;
	int32_t type;
	int32_t val;
	uint64_t len;
	struct adsp_mem mem;
	struct adsp_mem rsv_mem;

	memset(&mem, 0, sizeof(struct adsp_mem));

	if (strncmp(str_p, "help", 4) == 0) {
		VADSP_DEBUG_PRINTF("wtm:[task],[type],[val],[len]\n");
		return 0;
	}
	ret = sscanf(str_p, "%llu,%d,%d,%llu", &task, &type, &val, &len);
	if (ret < 0)
		VADSP_DEBUG_PRINTF("sscanf fail\n");

	if (type == MEM_TYPE_A2D)
		get_adsp_atod_dram(task, &mem.phy_addr, &mem.size);
	else if (type == MEM_TYPE_D2A)
		get_adsp_dtoa_dram(task, &mem.phy_addr, &mem.size);
	else
		return 0;

	get_adsp_audio_reserve_dram(&rsv_mem.phy_addr, &rsv_mem.size);
	VADSP_DEBUG_PRINTF("%s task: %llu, type: 0x%x, phy: 0x%llx, size: %lld, val:%d, len:%llu\n", __func__,
			task, type, mem.phy_addr, mem.size, val, len);
	VADSP_DEBUG_PRINTF("%s shm_region:0x%llx, rsv phy: 0x%llx, size: %lld\n", __func__,
			(uint64_t)adsp_handler->shm_region,
			rsv_mem.phy_addr, rsv_mem.size);
	if (mem.phy_addr == 0 || rsv_mem.phy_addr == 0 || adsp_handler->shm_region == NULL)
		return 0;

	if (len > mem.size)
		return 0;
	memset((void *)(adsp_handler->shm_region + mem.phy_addr - rsv_mem.phy_addr), val, len);
	VADSP_DEBUG_PRINTF("%s shm_addr:0x%llx, val:%d\n", __func__,
			(uint64_t)(adsp_handler->shm_region + mem.phy_addr - rsv_mem.phy_addr),
			*(int *)(adsp_handler->shm_region + mem.phy_addr - rsv_mem.phy_addr));
	return 0;
}

static unsigned int adsp_debug_read_task_ad_mem(
	char *str_p)
{
	int ret = 0;
	uint64_t task;
	int32_t type;
	struct adsp_mem mem;
	struct adsp_mem rsv_mem;

	memset(&mem, 0, sizeof(struct adsp_mem));

	if (strncmp(str_p, "help", 4) == 0) {
		VADSP_DEBUG_PRINTF("rtm:[task],[type]\n");
		return 0;
	}
	ret = sscanf(str_p, "%llu,%d", &task, &type);
	if (ret < 0)
		VADSP_DEBUG_PRINTF("sscanf fail\n");

	if (type == MEM_TYPE_A2D)
		get_adsp_atod_dram(task, &mem.phy_addr, &mem.size);
	else if (type == MEM_TYPE_D2A)
		get_adsp_dtoa_dram(task, &mem.phy_addr, &mem.size);
	else
		return 0;

	get_adsp_audio_reserve_dram(&rsv_mem.phy_addr, &rsv_mem.size);
	VADSP_DEBUG_PRINTF("%s task: %llu, type: 0x%x, phy: 0x%llx, size: %lld\n", __func__,
			task, type,
			mem.phy_addr, mem.size);
	VADSP_DEBUG_PRINTF("%s shm_region:0x%llx, rsv phy: 0x%llx, size: %lld\n", __func__,
			(uint64_t)adsp_handler->shm_region,
			rsv_mem.phy_addr, rsv_mem.size);
	VADSP_DEBUG_PRINTF("%s shm_addr:0x%llx, val:%d\n", __func__,
			(uint64_t)(adsp_handler->shm_region + mem.phy_addr - rsv_mem.phy_addr),
			*(int *)(adsp_handler->shm_region + mem.phy_addr - rsv_mem.phy_addr));

	return 0;
}

static unsigned int adsp_debug_read_shm_region(
	char *str_p)
{
	int ret = 0;
	int32_t offset = 0;

	if (strncmp(str_p, "help", 4) == 0) {
		VADSP_DEBUG_PRINTF("rsr:[offset]\n");
		return 0;
	}
	ret = sscanf(str_p, "%d", &offset);
	if (ret < 0)
		VADSP_DEBUG_PRINTF("sscanf fail\n");

	VADSP_DEBUG_PRINTF("%s shm_region:0x%llx offset:%d, val:%d\n", __func__,
			(uint64_t)adsp_handler->shm_region,
			offset, *(int *)(adsp_handler->shm_region + offset));
	return 0;
}

static unsigned int adsp_time_debug_enable(
	char *str_p)
{
	int ret = 0;
	int32_t enable = 0;

	if (strncmp(str_p, "help", 4) == 0) {
		VADSP_DEBUG_PRINTF("td:[enable]\n");
		return 0;
	}
	ret = sscanf(str_p, "%d", &enable);
	if (ret < 0)
		VADSP_DEBUG_PRINTF("sscanf fail\n");

	if (enable == 1) {
		if (adsp_handler)
			adsp_handler->time_dbg_enable = 1;
		memset(vadsp_rec_ts, 0, sizeof(vadsp_rec_ts));
	} else if (enable == 0) {
		if (adsp_handler)
			adsp_handler->time_dbg_enable = 0;
	} else if (enable == 2) {
		print_debug_info(adsp_handler);
	}
	VADSP_DEBUG_PRINTF("%s enable:%d\n", __func__, enable);
	return 0;
}

static void virtio_adsp_process_dbg_cmd(
	char *opt)
{
	if (strncmp(opt, "p:", 2) == 0)
		virtio_adsp_debug_get_ptr(opt + 2);
	if (strncmp(opt, "l:", 2) == 0)
		virtio_adsp_debug_trace_level(opt + 2);
	if (strncmp(opt, "wtm:", 4) == 0)
		adsp_debug_write_task_ad_mem(opt + 4);
	if (strncmp(opt, "rtm:", 4) == 0)
		adsp_debug_read_task_ad_mem(opt + 4);
	if (strncmp(opt, "rsr:", 4) == 0)
		adsp_debug_read_shm_region(opt + 4);
	if (strncmp(opt, "td:", 3) == 0)
		adsp_time_debug_enable(opt + 3);
}

static ssize_t virtio_adsp_debug_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(virtio_adsp_debug_buffer) - 1;
	ssize_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	memset(virtio_adsp_debug_buffer, 0, count);

	if (copy_from_user(&virtio_adsp_debug_buffer, ubuf, count))
		return -EFAULT;

	virtio_adsp_debug_buffer[count] = 0;
	buf_offset = 0;

	virtio_adsp_process_dbg_cmd(virtio_adsp_debug_buffer);

	return ret;
}

static const struct file_operations virtio_adsp_debug_fops = {
	.open = simple_open,
	.read = virtio_adsp_debug_read,
	.write = virtio_adsp_debug_write,
	.llseek = default_llseek,
};

void init_debugfs(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_dentry = debugfs_create_file("virtio_adsp_debug",
				0644, NULL, NULL, &virtio_adsp_debug_fops);
#endif
}

void clean_debugfs(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove(debugfs_dentry);
#endif
}

static int virtio_adsp_shm_remap(struct virtio_device *vdev)
{
	struct virtio_shm_region cache_reg;
	bool have_cache;
	struct virtio_adsp *vadsp = vdev->priv;

	/* Get cache region */
	have_cache = virtio_get_shm_region(vdev, &cache_reg,
			(u8)VIRTIO_ADSP_SHMCAP_ID_CACHE);
	if (!have_cache) {
		dev_info(&vdev->dev, "%s: No cache capability\n", __func__);
		return -ENXIO;
	}
	dev_notice(&vdev->dev, "Cache len: 0x%llx @ 0x%llx\n",
		cache_reg.len, cache_reg.addr);

	vadsp->shm_region = devm_memremap(&vdev->dev, cache_reg.addr,
		cache_reg.len, MEMREMAP_WB);
	dev_info(&vdev->dev, "[VADSP]cache_reg addr:0x%llx, len:0x%llx, shm_region:0x%llx\n",
			cache_reg.addr, cache_reg.len, (uint64_t)vadsp->shm_region);
	if (IS_ERR(vadsp->shm_region))
		return PTR_ERR(vadsp->shm_region);
	set_adsp_shm_region(vadsp->shm_region, cache_reg.len);
	return 0;
}

static int virtio_adsp_probe(struct virtio_device *vdev)
{
	struct virtqueue_info vqs_info[VIRTIO_ADSP_VQ_MAX] = {
		[0] = { "event",
					    adsp_event_done },
		[1] = { "request",
					  adsp_request_done },
	};
	struct virtio_adsp *vadsp;
	struct device_node *of_node = NULL;
	int err = -EINVAL;

	dev_info(&vdev->dev, "[VADSP]virtio adsp handler probe\n");
	vadsp = kzalloc(sizeof(*vadsp), GFP_KERNEL);
	if (!vadsp)
		return -ENOMEM;

	vadsp->vdev = vdev;
	spin_lock_init(&vadsp->lock);
	spin_lock_init(&vadsp->evt_lock);

	err = virtio_find_vqs(vdev, VIRTIO_ADSP_VQ_MAX, vadsp->vqs, vqs_info,
			      NULL);
	if (err)
		goto err;

	virtio_device_ready(vdev);

	err = fill_adsp_event(vadsp);
	if (err)
		goto err;
	of_node = of_find_node_by_name(NULL, "snd-audio-dsp");
	vadsp_pcm_taskattr_init(of_node);

	vdev->priv = vadsp;
	vadsp->time_dbg_enable = 1; // for audio underrun debug
	virtio_adsp_rec_time_init();
	adsp_handler = vadsp;
	virtio_adsp_shm_remap(vdev);
	init_debugfs();
	init_debug_work(vadsp);
	init_host_work(vadsp);
	dev_info(&vdev->dev, "[VADSP]virtio adsp handler initialized\n");
	return 0;
err:
	dev_warn(&vdev->dev, "[VADSP]virtio adsp handler probe failed:%d\n", err);

	if (vadsp->vdev)
		vdev->config->del_vqs(vadsp->vdev);
	kfree(vadsp);
	return err;
}

static void virtio_adsp_remove(struct virtio_device *vdev)
{
	struct virtio_adsp *vadsp = vdev->priv;

	vdev->config->reset(vdev);
	vdev->vringh_config->del_vrhs(vadsp->vdev);
	vdev->config->del_vqs(vadsp->vdev);
	destroy_debug_work(vadsp);
	destroy_host_work(vadsp);
	kfree(vadsp);
	clean_debugfs();
}

static unsigned int features[] = {
	/* none */
};
static struct virtio_device_id id_table[] = {
	{ VIRTIO_SUBSYSTEM_ID_MTK_ADSP, PCI_SUBVENDOR_ID_MTK },
	{ 0 },
};

static struct virtio_driver virtio_adsp_driver = {
	.driver.name         = KBUILD_MODNAME,
	.driver.owner        = THIS_MODULE,
	.feature_table       = features,
	.feature_table_size  = ARRAY_SIZE(features),
	.id_table            = id_table,
	.probe               = virtio_adsp_probe,
	.remove              = virtio_adsp_remove,
};

module_virtio_driver_thread(virtio_adsp_driver);
MODULE_DEVICE_TABLE(virtio, id_table);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Virtio Adsp");
MODULE_AUTHOR("Feilong Wei");

