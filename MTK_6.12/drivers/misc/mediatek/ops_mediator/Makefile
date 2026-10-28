KO_MODULE_NAME := opsMediator

#
# core source
#

$(KO_MODULE_NAME)-y += mediator.o
#
# Build Options
#
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

#
# Final Objects
#
obj-m := $(KO_MODULE_NAME).o