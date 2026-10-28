#Transsion Top Secret
#
#Copyright (C) 2023 Transsion Inc.
#
#@author chonglin.yang
#@version 1.0, 2023-10-20
#
subdir-ccflags-y += -I$(srctree)/drivers/android/
obj-$(CONFIG_TRAN_HUNG_DETECT) += thd.o
thd-y	+= \
	trace_hung_binder.o \
	trace_hung_cpu.o \
	trace_hung_task.o \
	system_monitor.o \
	binder_monitor.o \
	thd_util.o \
	aegisk.o \
	thd_main.o