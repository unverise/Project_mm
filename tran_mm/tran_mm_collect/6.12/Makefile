$(info Include Path: $(DEVICE_MODULES_PATH)/drivers/misc/tranlog/)
$(info Include Path: $(srctree)/../vendor/transsion/kernel_modules/tran_mm/tran_zram/6.12/zram/)

ccflags-y += \
	-I$(srctree)/include/ \
	-I$(srctree)/include/linux/ \
	-I$(DEVICE_MODULES_PATH)/../kernel-6.12/ \
	-I$(srctree)/mm/ \
	-I$(srctree)/ \
	-I$(srctree)/$(src)/ \
	-I$(srctree)/../vendor/transsion/kernel_modules/tran_mm/tran_zram/6.12/zram/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/tranlog/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/transsion/tnek/ \
	-I$(srctree)/../kernel-6.12/kernel/

obj-m += tran_mm_collect.o

tran_mm_collect-y += tranmem_cloud.o
tran_mm_collect-y += tranmem_init.o
tran_mm_collect-y += tranmem_task_struct.o
tran_mm_collect-y += tranmem_emem.o
tran_mm_collect-y += tranmem_psi.o
#tran_mm_collect-y += tranmem_ioblock.o