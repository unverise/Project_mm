include $(wildcard $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include)

$(info log shrink_slabd $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR))

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/
ccflags-y += -I$(srctree)/../kernel-6.6/kernel/

modules modules_install clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(KBUILD_OPTIONS) KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" $(@)
