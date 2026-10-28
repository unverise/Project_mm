ccflags-y += -I$(srctree)/drivers/mmc
ccflags-y += -I$(srctree)/drivers/ufs
ccflags-y += -I$(srctree)/drivers/mmc/host/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/mmc/host
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/ufs
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/ufs/vendor

obj-$(CONFIG_TRAN_PROC_BOOTDEVICE) += tran-proc-bootdevice-mod.o
tran-proc-bootdevice-mod-y += tran-proc-bootdevice.o
tran-proc-bootdevice-mod-$(CONFIG_TRAN_PROC_UFS) += tran-proc-ufs.o
tran-proc-bootdevice-mod-$(CONFIG_TRAN_PROC_MMC) += tran-proc-mmc.o
