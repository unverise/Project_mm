# SPDX-License-Identifier: GPL-2.0
#
# Makefile for misc devices that really don't fit anywhere else.
#

#SDD:add for tnek.ko by yanbin.zhang 2024/07/18 start
obj-$(CONFIG_TRAN_TNEK_SUPPORT) += transsion/
#SDD:add for tnek.ko by yanbin.zhang 2024/07/18 end

#SDD:add for thd.ko by chonglin.yang 2025/07/14 start
obj-$(CONFIG_TRAN_HUNG_DETECT) += transsion/
#SDD:add for thd.ko by chonglin.yang 2025/07/14 end

obj-y				+= mediatek/
obj-y               += tranlog/

#USD: add for freeze by jiacheng.deng 20250722 start
obj-y               += hiber/
#USD: add for freeze by jiacheng.deng 20250722 end

#USD:get cpu suspend time and 26M off time by jian.zhang7  20250919 start
obj-$(CONFIG_TRAN_POWER_CTRL)   += tran_power_ctrl/
#USD:get cpu suspend time and 26M off time by jian.zhang7  20250919 end
