#
#  novatek_nt36xxx.ko
#
# Makefile: for external make invocation
#
# Note:
# - Please refer to modules/sample/Makefile to find out what should be
#   done in this Makefile
#

KO_MODULE_NAME := novatek_nt36xxx

$(warning =======================transsion tp novatek_novatek_common===============================)
$(warning =======================transsion tp novatek_novatek_common===============================)

#
# source
#
$(KO_MODULE_NAME)-y := nt36xxx.o \
                novatek_transsion.o \
                nt36xxx_ext_proc.o \
                nt36xxx_fw_update.o \
                nt36xxx_mp_ctrlram.o

#
# Build Options
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/include/linux
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/transsion/core/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/tranlog/

#
# Final Objects
#
obj-m := $(KO_MODULE_NAME).o