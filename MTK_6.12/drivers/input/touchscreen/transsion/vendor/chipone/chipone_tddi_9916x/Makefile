#
# chipone_icnl99xx.ko
#
# Kbuild: for kernel building external module
#
# Note:
# - Please use these predefined name for keeping code:
#   KO_MODULE_NAME KO_MODULE_PATH KO_MODULE_SRC
#

KO_MODULE_NAME := chipone_icnl99xx

$(warning =======================transsion tp chipone_icnl99xx===============================)
$(warning =======================transsion tp chipone_icnl99xx===============================)

#
# source
#
$(KO_MODULE_NAME)-y := chipone_transsion.o \
        cts_driver.o \
        cts_core.o \
        cts_sfctrlv2.o \
        cts_flash.o \
        cts_firmware.o \
        cts_test.o \
        cts_charger_detect.o \
        cts_earjack_detect.o \
        cts_tcs.o \
        cts_platform.o \
        cts_tool.o \
        cts_sysfs.o \
        cts_strerror.o \
        cts_oem.o \
        cts_drw.o \
        cts_prw.o \
        cts_krang.o \
        cts_transfer.o

#
# Build Options
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/include/linux
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/transsion/core/include/

#
# Final Objects
#
obj-m := $(KO_MODULE_NAME).o