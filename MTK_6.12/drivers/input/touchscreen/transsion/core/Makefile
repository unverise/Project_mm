#
# adaptive-ts.ko
#
# Kbuild: for kernel building external module
#

KO_MODULE_NAME := adaptive-ts
$(warning =======================TRAN TP CORE===============================)
$(warning DEVICE_MODULES_PATH=$(DEVICE_MODULES_PATH))
$(warning CONFIG_DRM_MEDIATEK_V2=$(CONFIG_DRM_MEDIATEK_V2))
$(warning =======================TRAN TP CORE===============================)

#
# core source code
#
$(KO_MODULE_NAME)-y := transsion_core.o transsion_gesture.o transsion_node.o \
                       transsion_ext_function.o transsion_aidl_interface.o \
                       transsion_sub_device.o transsion_thp_interface.o \
                       ts_debug_ftrace.o



# mtk platform
$(KO_MODULE_NAME)-y += platform/mediatek/transsion_mtk_node.o\
		platform/mediatek/transsion_mtk_platform.o\
		platform/mediatek/transsion_mtk_sub_device.o

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/transsion/core/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/tranlog/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/transsion/include/tk/
#
# Target
#
obj-m += $(KO_MODULE_NAME).o
