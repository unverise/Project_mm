/* SPDX-License-Identifier: GPL-2.0+*/
/*
 * Copyright (c) 2024 MediaTek Inc.
 * virt-adsp: Virtio adsp device
 */
#ifndef VIRTIO_ADSP_H
#define VIRTIO_ADSP_H
#include <linux/types.h>
#include "mtk-base-dsp.h"
#define DSP_NID_OFFSET 97
#define NID_TO_DSP_DAI_ID(x) (x - DSP_NID_OFFSET)

enum {
	ADSP_EVT_IRQ,
	ADSP_EVT_MEM,
	ADSP_EVT_IPI,
	ADSP_EVT_RECOVERY,
};

enum {
	MEM_TYPE_RING,
	MEM_TYPE_A2D,
	MEM_TYPE_D2A,
	MEM_TYPE_RSV,
};

struct adsp_irq_event {
	uint64_t task;
	int32_t core;
	int32_t xrun;
	int32_t ptr;
};

struct adsp_shm_region {
	void *region;
	unsigned long long size;
};

struct adsp_mem {
	uint64_t phy_addr;
	uint64_t size;
};

struct adsp_mem_event {
	uint64_t task;
	uint64_t phy_addr;
	uint64_t size;
	int32_t type;
};

struct adsp_ipi_event {
	uint64_t phy_addr;
	uint64_t buffer_size;
	uint64_t write_offset;
	uint64_t write_size;
	uint64_t phy_rp_addr;
};

struct adsp_recovery_event {
	uint32_t cmd_type;
};

struct adsp_event_cb_data {
	int32_t type;
	union {
		struct adsp_irq_event irq;
		struct adsp_mem_event mem;
		struct adsp_ipi_event ipi_info;
		struct adsp_recovery_event recovery_evt;
	};
	void *data;
};

enum {
	TYPE_PCM_ID,
	TYPE_TASK_ID
};

enum rec_time_pos {
	REC_TIME_POS_CPY,
	REC_TIME_POS_PTR,
	REC_TIME_POS_IRQ,
	REC_TIME_POS_NUM,
};

enum {
	PLAYBACK_1_NID,
	PLAYBACK_12_NID,
	PLAYBACK_2_NID,
	PLAYBACK_3_NID,
	PLAYBACK_5_NID,
	PLAYBACK_8_NID,
	PLAYBACK_6_NID,
	PLAYBACK_7_NID,
	PLAYBACK_9_NID,
	PLAYBACK_13_NID,
	PLAYBACK_4_NID,
	PLAYBACK_25_NID,
	PLAYBACK_11_NID,
	PLAYBACK_48_NID,
	CAPTURE_1_NID,
	CAPTURE_2_NID,
	CAPTURE_3_NID,
	CAPTURE_4_NID,
	CAPTURE_5_NID,
	CAPTURE_6_NID,
	CAPTURE_7_NID,
	CAPTURE_8_NID,
	CAPTURE_9_NID,
	CAPTURE_10_NID,
	CAPTURE_11_NID,
	CAPTURE_12_NID,
	CAPTURE_13_NID,
	CAPTURE_MONO_1_NID,
	CAPTURE_MONO_2_NID,
	CAPTURE_ETDM_IN1_NID,
	CAPTURE_ETDM_IN2_NID,
	CAPTURE_ETDM_IN6_NID,
	HOSTLESS_LPBK_NID,
	HOSTLESS_FM_NID,
	HOSTLESS_SPEECH_NID,
	HOSTLESS_BT_NID,
	HOSTLESS_SPH_ECHO_REF_NID,
	HOSTLESS_SPK_INIT_NID,
	HOSTLESS_ADDA_DL_I2S_OUT_NID,
	HOSTLESS_SRC_1_NID,
	HOSTLESS_SRC_3_NID,
	HOSTLESS_HW_SRC_1_OUT_NID,
	HOSTLESS_HW_SRC_1_IN_NID,
	HOSTLESS_HW_SRC_2_OUT_NID,
	HOSTLESS_HW_SRC_2_IN_NID,
	HOSTLESS_HW_SRC_3_OUT_NID,
	HOSTLESS_HW_SRC_3_IN_NID,
	HOSTLESS_SRC_BARGEIN_NID,
	HOSTLESS_HW_GAIN_AAUDIO_NID = 93,
	HOSTLESS_SRC_AAUDIO_NID,
	DSP_VOIP_NID = DSP_NID_OFFSET,
	DSP_PRIMARY_NID,
	DSP_DEEPBUFFER_NID,
	DSP_PLAYBACK_FAST_NID,
	DSP_PLAYBACK_SPATIAL_NID,
	DSP_PLAYBACK_PLAYBACK_NID,
	DSP_CAPTURE_UL1_NID,
	DSP_CALL_FINAL_NID,
	DSP_PLAYBACK_KTV_NID,
	DSP_CAPTURE_RAW_NID,
	DSP_PLAYBACK_FM_ADSP_NID,
	DSP_PLAYBACK_A2DP_NID,
	DSP_PLAYBACK_BLEDL_NID,
	DSP_CAPTURE_BLE_NID,
	DSP_PLAYBACK_BT_NID,
	DSP_CAPTURE_BT_NID,
	DSP_CAPTURE_PROCESS_NID,
	DSP_PLAYBACK_ECHOREF_NID,
	DSP_PLAYBACK_USB_NID,
	DSP_CAPTURE_USB_NID,
	DSP_CAPTURE_MDDL_NID,
	DSP_PLAYBACK_MDUL_NID,
	DSP_PLAYBACK_CALLDL_NID,
	DSP_CAPTURE_CALLUL_NID,
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	DSP_PLAYBACK_HFP_CLIENT_RX_NID,
	DSP_PLAYBACK_HFP_CLIENT_TX_NID,
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	DSP_PLAYBACK_ANC_NID,
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	DSP_PLAYBACK_EXTSTREAM1_NID,
	DSP_PLAYBACK_EXTSTREAM2_NID,
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT)
	DSP_PLAYBACK_SUB_PLAYBACK_NID,
#endif
	DSP_PLAYBACK_PLAYBACK0_NID,
	DSP_PLAYBACK_PLAYBACK1_NID,
	DSP_PLAYBACK_PLAYBACK2_NID,
	DSP_PLAYBACK_PLAYBACK3_NID,
	DSP_PLAYBACK_PLAYBACK4_NID,
	DSP_PLAYBACK_PLAYBACK5_NID,
	DSP_PLAYBACK_PLAYBACK6_NID,
	DSP_PLAYBACK_PLAYBACK7_NID,
	DSP_PLAYBACK_PLAYBACK8_NID,
	DSP_PLAYBACK_PLAYBACK9_NID,
	DSP_PLAYBACK_PLAYBACK10_NID,
	DSP_PLAYBACK_PLAYBACK11_NID,
	DSP_PLAYBACK_PLAYBACK12_NID,
	DSP_PLAYBACK_PLAYBACK13_NID,
	DSP_PLAYBACK_PLAYBACK14_NID,
	DSP_PLAYBACK_PLAYBACK15_NID,
	DSP_CAPTURE_MCH_NID,
#endif
	MAX_NID,
};

struct dsp_task_info {
	int task_scene;
	int dai_id;
	int is_fe;
};

typedef int (*adsp_mbrain_cb_func)(bool is_pcm,
		uint32_t id, uint32_t pos, uint64_t stime,
		uint64_t etime, uint64_t count);
void virtio_register_adsp_mbrain_cb(adsp_mbrain_cb_func cb);

typedef void (*adsp_event_cb_func)(struct adsp_event_cb_data *data);
void virtio_register_adsp_event_cb(adsp_event_cb_func cb,
				void *data);

typedef void (*adsp_ipi_cb_func)(struct adsp_ipi_event *data);
void virtio_register_adsp_ipi_cb(adsp_ipi_cb_func cb);

typedef void (*adsp_virtdsp_cb_func)(uint32_t cmd_type);
void virtio_register_adsp_virtdsp_cb(adsp_virtdsp_cb_func cb);

int32_t virtio_adsp_get_pointer(uint64_t task_id);
int32_t virtio_adsp_copy_pcm_dl(uint64_t task_id, uint64_t phy_addr, uint64_t copy_size);
int32_t virtio_adsp_pcm_trigger(uint64_t type, uint64_t task_id, uint64_t cmd);
int32_t virtio_adsp_copy_pcm_ul(uint64_t task_id, uint64_t phy_addr, uint64_t copy_size);
int virtio_adsp_check_time_start(int task_id, enum rec_time_pos pos);
int virtio_adsp_check_time_end(int task_id, enum rec_time_pos pos);
int virtio_adsp_rec_time_start(int task_id, enum rec_time_pos pos);
int virtio_adsp_rec_time_end(int task_id, enum rec_time_pos pos);
void virtio_adsp_reset_pointer(uint64_t task_id);

int get_taskid_by_nid(int id);
int is_adsp_pcm(int pcm_id);
int is_adsp_fe_pcm(int pcm_id);
int is_adsp_memif(int direction, int pcm_id);
void get_adsp_ring_dram(int task_id, unsigned long long *phy_addr,
	unsigned long long *size);
void get_adsp_dtoa_dram(int task_id, unsigned long long *phy_addr,
	unsigned long long *size);
void get_adsp_atod_dram(int task_id, unsigned long long *phy_addr,
	unsigned long long *size);
unsigned char *vadsp_phy_to_virt(unsigned long long phy_addr);
void set_adsp_shm_region(void *region, unsigned long long size);
void get_adsp_audio_reserve_dram(unsigned long long *phy_addr,
	unsigned long long *size);
void vadsp_event_handler(struct adsp_event_cb_data *data);
void create_elapse_task(void *snd_data);
int set_task_cpu_affinity(struct task_struct *task);
int vadsp_build_devs(struct snd_card *card);
void destroy_elapse_task(void *snd_data);
void virtio_unregister_adsp_event_cb(void);
void virtio_unregister_adsp_ipi_cb(void);
void virtio_unregister_adsp_mbrain_cb(void);
void virtio_unregister_adsp_virtdsp_cb(void);

#endif
