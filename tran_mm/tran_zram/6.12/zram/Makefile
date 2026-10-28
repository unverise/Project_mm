# SPDX-License-Identifier: GPL-2.0-only
tran_zram-y	:=	zram_process_reclaim.o zcomp.o zram_drv.o

tran_zram-$(CONFIG_ZRAM_BACKEND_LZO)		+= backend_lzorle.o backend_lzo.o
tran_zram-$(CONFIG_ZRAM_BACKEND_LZ4)		+= backend_lz4.o
tran_zram-$(CONFIG_ZRAM_BACKEND_LZ4HC)	+= backend_lz4hc.o
tran_zram-$(CONFIG_ZRAM_BACKEND_ZSTD)	+= backend_zstd.o
tran_zram-$(CONFIG_ZRAM_BACKEND_DEFLATE)	+= backend_deflate.o
tran_zram-$(CONFIG_ZRAM_BACKEND_842)		+= backend_842.o

obj-$(CONFIG_ZRAM)	+=	tran_zram.o

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/tranlog/
ccflags-y += -I$(srctree)/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/trans_sched
ccflags-y += -I$(srctree)/../vendor/transsion/kernel_modules/uas/include

ifneq ($(CONFIG_TRANSSION_BLK),)
ifeq ($(CONFIG_TRANSSION_BLK),$(filter $(CONFIG_TRANSSION_BLK),y m))
ccflags-y += -I$(DEVICE_MODULES_PATH)/fs/atomic_storage/tran_block/low_latency_sched/include/
endif
endif

tran_zram-$(CONFIG_ESWAP) += eswap/eswap_acct.o
tran_zram-$(CONFIG_ESWAP) += eswap/eswap_area.o
tran_zram-$(CONFIG_ESWAP) += eswap/eswap_core.o
tran_zram-$(CONFIG_ESWAP) += eswap/eswap_ctrl.o
tran_zram-$(CONFIG_ESWAP) += eswap/eswap_helper.o
tran_zram-$(CONFIG_ESWAP) += eswap/eswap_list.o
tran_zram-$(CONFIG_ESWAP) += eswap/eswap_lru_rmap.o
tran_zram-$(CONFIG_ESWAP) += eswap/eswap_manager.o
tran_zram-$(CONFIG_ESWAP) += eswap/eswap_schedule.o
tran_zram-$(CONFIG_ESWAP) += eswap/eswap_stats.o
tran_zram-$(CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT) += eswap/memcg_reclaim.o

ifeq ($(CONFIG_ESWAP),y)
	tran_zram-y += eswap/memcg_control.o
else ifeq ($(CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT),y)
	tran_zram-y += eswap/memcg_control.o
endif
