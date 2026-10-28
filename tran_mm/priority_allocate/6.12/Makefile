$(info Include Path: $(DEVICE_MODULES_PATH)/drivers/misc/tranlog/)
$(info Include Path: $(srctree)/../vendor/transsion/kernel_modules/tran_mm/tran_zram/6.12/zram/)

ccflags-y += \
	-I$(srctree)/include/ \
	-I$(srctree)/include/linux/ \
	-I$(srctree)/mm/ \
	-I$(srctree)/ \
	-I$(srctree)/$(src) \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/tranlog/ \
	-I$(DEVICE_MODULES_PATH)/../vendor/transsion/kernel_modules/uas/include

obj-m += priority_allocate.o

priority_allocate-y += priority_allocates.o
priority_allocate-y += task_priority.o
