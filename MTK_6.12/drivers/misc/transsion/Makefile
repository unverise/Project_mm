#Transsion Top Secret
#
#Copyright (C) 2019 Transsion Inc.
#
#@author zhongyu.shi
#@version 1.0, 2019-06-03
#
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/transsion/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/transsion/include
obj-$(CONFIG_TRAN_TNEK_SUPPORT) += tnek/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/transsion/thd
obj-$(CONFIG_TRAN_HUNG_DETECT) += thd/

#USD: add gpio sar monitor by ningninggao 20241114 start
obj-$(CONFIG_TRAN_GPIO_SAR_MONITOR_MODULE) += tran_gpio_sar_monitor/
#USD: add gpio sar monitor by ningninggao 20241114 start
