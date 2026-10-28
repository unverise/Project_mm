# SPDX-License-Identifier: GPL-2.0
# UFSHCD makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/blocktag/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power_throttling/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/ufs/vendor
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/ufs/
ccflags-y += -I$(srctree)/drivers/ufs/core/
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(srctree)/kernel/
#STD: add tran_cdev for healthinfo_driver by xujie at 20250818 start
ccflags-y += -I$(srctree)/include/ufs
#STD: add tran_cdev for healthinfo_driver by xujie at 20250818 end


#STD: add proc tran_bootdevice by yong.yang6 20250808 start
ccflags-y += -I$(srctree)/drivers/bootdevice
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/bootdevice
#STD: add proc tran_bootdevice by yong.yang6 20250808 end

# obj-$(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK) += vendor/
# obj-$(CONFIG_SCSI_UFS_MEDIATEK_DBG) += ufs-mediatek-dbg.o
