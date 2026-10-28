# Copyright (C) 2015 Transsion Inc
# Author: fan.peng
# Description: Build rules for fingerprint driver

# Include custom makefile if available
-include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/Makefile.custom

# =============================================================================
# Device Driver Selection
# =============================================================================

# SPI device configuration (if enabled)
ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
    KBUILD_CFLAGS += -DTRAN_SPI
    
    # Override platform device settings for SPI
    KBUILD_CFLAGS += -UTRAN_DEV_SEL_device -UTRAN_DEV_SEL_driver
    KBUILD_CFLAGS += -UTRAN_DEV_SEL_dev_put -UTRAN_DEV_SEL_device_id
    KBUILD_CFLAGS += -UTRAN_DEV_SEL_set_drvdata -UTRAN_DEV_SEL_get_drvdata
    KBUILD_CFLAGS += -UTRAN_DEV_SEL_register_driver -UTRAN_DEV_SEL_unregister_driver
    
    KBUILD_CFLAGS += -DTRAN_DEV_SEL_device=spi_device
    KBUILD_CFLAGS += -DTRAN_DEV_SEL_driver=spi_driver
    KBUILD_CFLAGS += -DTRAN_DEV_SEL_dev_put=spi_dev_put
    KBUILD_CFLAGS += -DTRAN_DEV_SEL_device_id=spi_device_id
    KBUILD_CFLAGS += -DTRAN_DEV_SEL_set_drvdata=spi_set_drvdata
    KBUILD_CFLAGS += -DTRAN_DEV_SEL_get_drvdata=spi_get_drvdata
    KBUILD_CFLAGS += -DTRAN_DEV_SEL_register_driver=spi_register_driver
    KBUILD_CFLAGS += -DTRAN_DEV_SEL_unregister_driver=spi_unregister_driver
endif

# =============================================================================
# Build Information
# =============================================================================
$(info CONFIG_TRAN_FP_DRV_MODE_SPI=$(CONFIG_TRAN_FP_DRV_MODE_SPI) ...)
$(info TRAN_FINGERPRINT_SUPPORT=$(TRAN_FINGERPRINT_SUPPORT) ...)
$(info CONFIG_TRAN_FP_LOG_LVL=$(CONFIG_TRAN_FP_LOG_LVL) ...)
$(info TRUSTONIC_TEE_VERSION=$(CONFIG_TRAN_FP_TEE_VERSION) ...)

# =============================================================================
# Include Paths
# =============================================================================

# Common include paths
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/fingerprint/tran_common
ccflags-y += -I$(srctree)/drivers/spi
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/teei/300/tz_driver/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/teei/300/common/include
ccflags-y += -I$(srctree)/drivers/regulator
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/gud/600/MobiCoreDriver/public

# Vendor-specific include paths
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/fingerprint/tran_common/mtk/goodix
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/fingerprint/tran_common/mtk/sunwave
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/fingerprint/tran_common/mtk/chipone
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/fingerprint/tran_common/mtk/cdfinger
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/fingerprint/tran_common/mtk/fpc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/fingerprint/tran_common/mtk/focaltech
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/fingerprint/tran_common/mtk/silead
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/fingerprint/tran_common/mtk/oxi
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/fingerprint/tran_common/mtk/egis
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/fingerprint/tran_common/mtk/anc

# Build date
ccflags-y += -D__TRAN_BUILD_DATE="$(shell date +%Y%m%d)"

# =============================================================================
# Android Version Configuration
# =============================================================================

ifeq ($(findstring android_u,$(CONFIG_TRAN_FP_ANDROID_VERSION)),android_u)
    ccflags-y += -DTRAN_FP_ANDROID_U
endif

# =============================================================================
# TEE Version Configuration
# =============================================================================

ifeq ($(findstring 510,$(CONFIG_TRAN_FP_TEE_VERSION)),510)
    ccflags-y += -DTEE_VERSION_510
endif

# =============================================================================
# Fingerprint Vendor Support
# =============================================================================

TRAN_FINGERPRINT_SUPPORT := $(subst ",,$(CONFIG_TRAN_FP_VENDOR))

# Define vendor flags in a more maintainable way
VENDOR_DEFINES := \
    sunwave:TRAN_FP_SUNWAVE \
    chipone:TRAN_FP_CHIPONE \
    cdfinger:TRAN_FP_CDFINGER \
    silead:TRAN_FP_SILEAD \
    focaltech:TRAN_FP_FOCALTECH \
    fpc:TRAN_FP_FPC \
    goodix:TRAN_FP_GOODIX \
    oxi:TRAN_FP_OXI \
    egis:TRAN_FP_EGIS \
    jiiov:TRAN_FP_JIIOV

$(foreach vendor_define,$(VENDOR_DEFINES),\
    $(eval vendor := $(word 1,$(subst :, ,$(vendor_define)))) \
    $(eval define_name := $(word 2,$(subst :, ,$(vendor_define)))) \
    $(if $(findstring $(vendor),$(TRAN_FINGERPRINT_SUPPORT)),\
        ccflags-y += -D$(define_name)))

# =============================================================================
# Log Level Configuration
# =============================================================================

LOG_LEVELS := VERBOSE DEBUG INFO WARNING ERROR
$(foreach level,$(LOG_LEVELS),\
    $(if $(findstring $(level),$(CONFIG_TRAN_FP_LOG_LVL)),\
        ccflags-y += -DTRAN_FP_LOG_LVL_$(level)))

# =============================================================================
# Object Files
# =============================================================================

obj-$(CONFIG_TRAN_FINGERPRINT_LEGACY) += tran_fp.o

# Core source files
tran_fp-y := tran_fp_main.o
tran_fp-y += tran_fp_ext.o

# Vendor-specific source files
tran_fp-y += mtk/goodix/fp_goodix_cust.o
tran_fp-y += mtk/sunwave/fp_sunwave_cust.o
tran_fp-y += mtk/chipone/fp_chipone_cust.o
tran_fp-y += mtk/cdfinger/fp_cdfinger_cust.o
tran_fp-y += mtk/fpc/fp_fpc_cust.o
tran_fp-y += mtk/focaltech/fp_focaltech_cust.o
tran_fp-y += mtk/silead/fp_silead_cust.o
tran_fp-y += mtk/oxi/fp_oxi_cust.o
tran_fp-y += mtk/egis/fp_egis_cust.o
tran_fp-y += mtk/anc/fp_jiiov_cust.o
