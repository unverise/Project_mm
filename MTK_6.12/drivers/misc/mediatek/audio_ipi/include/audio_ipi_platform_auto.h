/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __AUDIO_IPI_PLATFORM_AUTO_H__
#define __AUDIO_IPI_PLATFORM_AUTO_H__

#if IS_ENABLED(CONFIG_DEVICE_MODULES_VHOST_ADSP)
#define MAX_VHOST_ADSP_INSTANCE 4
struct virt_ipi_dump_info_t {
	uint64_t phys_addr_base;
	uint64_t virt_addr_base;
	uint64_t phys_rp_addr;  // save rq address
	uint64_t virt_rp_addr;
	uint64_t buffer_size;
	uint64_t write_offset;
	uint64_t write_size;
};

struct virt_ipi_buffer_info_t {
	struct virt_ipi_dump_info_t dsp_dump_buffer[MAX_VHOST_ADSP_INSTANCE];
	uint8_t *tmp_linear_buffer;
	spinlock_t dump_lock;
	bool is_spinlock_init;
};

struct vadsp_guest_dump_t {
	uint64_t phys_addr_base;
	uint64_t buffer_size;
	uint64_t write_offset;
	uint64_t write_size;
	uint64_t phys_rp_addr;
};

typedef void (*vadsp_ipi_callback_t)(struct vadsp_guest_dump_t *dump_info, unsigned int vm_num);

extern long audio_ipi_kernel_ioctl(unsigned int cmd, unsigned long arg);
extern int audio_ipi_set_dump_buffer_info(
	uint64_t vm_id,
	uint64_t buffer_addr,
	uint64_t buffer_size,
	uint64_t rp_addr);
extern int vadsp_task_register_callback(vadsp_ipi_callback_t  vadsp_ipi_notify_func);
extern int audio_ipi_dma_msg_send(struct ipi_msg_t *p_ipi_msg);
#endif
#endif /*__AUDIO_IPI_PLATFORM_AUTO_H__ */
