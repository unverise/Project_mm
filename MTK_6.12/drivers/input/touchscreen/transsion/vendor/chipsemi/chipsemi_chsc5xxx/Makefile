#
# chipsemi_chsc5xxx.ko
#
# Kbuild: for kernel building external module
#
# Note:
# - Please use these predefined name for keeping code:
#   KO_MODULE_NAME KO_MODULE_PATH KO_MODULE_SRC
#

KO_MODULE_NAME := chipsemi_chsc5xxx
KO_MODULE_PATH := $(src)
KO_MODULE_SRC  :=

#
# source
#
$(KO_MODULE_NAME)-y := chipsemi_transsion.o \
        basic_util.o \
        spi_communication.o \
        semi_touch_function.o \
        semi_touch_upgrade.o \
        semi_touch_apk.o \
        semi_touch_device.o \
        semi_touch_custom.o \
        semi_touch_test_5xxx.o \
        semi_touch_thp_dev.o \
        qual_core.o

#
# Build Options
#
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/transsion/core/include/

#
# Final Objects
#
obj-m := $(KO_MODULE_NAME).o
