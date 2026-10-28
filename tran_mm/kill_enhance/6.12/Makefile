ccflags-y += \
	-I$(srctree)/include/ \
	-I$(srctree)/include/linux/ \
	-I$(srctree)/mm/ \
	-I$(srctree)/ \
	-I$(srctree)/$(src) \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/tranlog/

obj-m += kill_enhance.o

kill_enhance-y += kill_enhances.o
