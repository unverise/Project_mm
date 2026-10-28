# SPDX-License-Identifier: GPL-2.0
#
# Makefile for regulator drivers.
ccflags-y += -I$(srctree)/drivers/regulator
CFLAGS_mtk-vmm-isp71-regulator.o := -I$(DEVICE_MODULES_PATH)/drivers/regulator
CFLAGS_mtk-dvfsrc-regulator.o := -I$(DEVICE_MODULES_PATH)/drivers/regulator

obj-$(CONFIG_DEVICE_MODULES_REGULATOR_MT6357) += mt6357-regulator.o
#obj-$(CONFIG_DEVICE_MODULES_REGULATOR_MT6358) += mt6358-regulator.o
obj-$(CONFIG_REGULATOR_MT6362) += mt6362-regulator.o
#obj-$(CONFIG_REGULATOR_MTK_VMM_ISP71) += mtk-vmm-isp71-regulator.o

#ITD:camera porting by camera driver team 250531 start
obj-$(REGULATOR_TRAN_4CH_LDO_SUPPORT) += tran_ldo/
#ITD:camera porting by camera driver team 250531 end

ccflags-$(CONFIG_REGULATOR_DEBUG) += -DDEBUG
