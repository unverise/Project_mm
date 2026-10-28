
# Ensure tran_power_ctrl is loaded early by adding it to the core-y list
$(warning #tran_power_ctrl#: $(CONFIG_TRAN_POWER_CTRL))
obj-$(CONFIG_TRAN_POWER_CTRL) += tran_power_ctrl.o
# Export symbols for use by other modules
export-objs += tran_power_ctrl.o

$(warning #tran_power_ctrl#: obj-$(CONFIG_TRAN_POWER_CTRL))
