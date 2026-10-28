# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

imgsensor_isp6s-objs += \
	../common/v1_1/camera_hw/imgsensor_cfg_table.o \
	../common/v1_1/camera_hw/gpio/gpio.o \
	../common/v1_1/camera_hw/mclk/mclk.o
#ITD:modify by camera driver team start
ifeq ($(CONFIG_TRAN_CAMERA_REGULATOR_SUPPORT), y)
imgsensor_isp6s-objs				+= ../common/v1_1/camera_hw/regulator/tran_regulator.o
else
imgsensor_isp6s-objs				+= ../common/v1_1/camera_hw/regulator/regulator.o
endif
#ITD:modify by camera driver team end
