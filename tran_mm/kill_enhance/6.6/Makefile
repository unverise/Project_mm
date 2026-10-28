#$(info start make tran_mm_collect....)
#ifneq ($(wildcard $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include),)
#include $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include
#endif

#all:
#	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS)
#
#modules_install:
#	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules_install
#
#clean:
#	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean


include $(wildcard $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include)

$(info log tran_mm_collect $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR))

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/
ccflags-y += -I$(srctree)/../kernel-6.1/kernel/

modules modules_install clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(KBUILD_OPTIONS) KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" $(@)

