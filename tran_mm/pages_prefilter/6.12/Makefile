ccflags-y += \
        -I$(srctree)/include/ \
        -I$(srctree)/include/linux/ \
        -I$(srctree)/mm/ \
        -I$(srctree)/ \
        -I$(srctree)/$(src)/ \
        -I$(DEVICE_MODULES_PATH)/drivers/misc/tranlog/

subdir-ccflags-y += \
        -I$(srctree)/include/ \
        -I$(srctree)/include/linux/ \
        -I$(srctree)/mm/ \
        -I$(srctree)/

pages_prefilter-y += page_prefilter.o
pages_prefilter-y += page_prefilter_com.o
pages_prefilter-y += tranmem_cloud.o
pages_prefilter-y += inodes_protect.o

obj-m += pages_prefilter.o
