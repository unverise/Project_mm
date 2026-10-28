#
# focaltech_ft3681.ko
#
# Kbuild: for kernel building external module
#
# Note:
# - Please use these predefined name for keeping code:
#   KO_MODULE_NAME KO_MODULE_PATH KO_MODULE_SRC
#

KO_MODULE_NAME := focaltech_thp

$(warning =======================transsion tp focaltech_thp===============================)
$(warning =======================transsion tp focaltech_thp===============================)

#
# source
#
$(KO_MODULE_NAME)-y := focaltech_debug.o \
                focaltech_core.o \
                focaltech_ex_fun.o \
                focaltech_ex_mode.o \
                focaltech_gesture.o \
                focaltech_transsion.o \
                focaltech_esdcheck.o \
                focaltech_point_report_check.o \
                focaltech_hp.o \
                focaltech_proximity.o

$(KO_MODULE_NAME)-y += focaltech_flash.o \
                focaltech_spi_v2.o
$(KO_MODULE_NAME)-y += focaltech_flash/focaltech_upgrade_ft3683g.o

$(KO_MODULE_NAME)-y += focaltech_test/focaltech_test.o \
                focaltech_test/focaltech_test_ini.o \
                focaltech_test/supported_ic/focaltech_test_ft3683g.o

#
# Build Options
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/include/linux
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/transsion/core/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/transsion/vendor/focaltech/focaltech_thp/include/

#
# Final Objects
#
obj-m := $(KO_MODULE_NAME).o

