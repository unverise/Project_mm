#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
subdir-ccflags-y += -I$(srctree)/include/linux/
ccflags-y += -I$(srctree)/drivers/misc/tranlog/
ccflags-y += -I$(srctree)/drivers/misc/transsion/include/tk/
ccflags-y += -I$(srctree)/drivers/misc/transsion/trancare/
ccflags-y += -I$(srctree)/drivers/misc/transsion/tnek/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/tranlog/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/transsion/include/tk/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/transsion/include/

obj-$(CONFIG_TRAN_CUSTOM_KEY_BIGDATA) += tran_key_bigdata.o
