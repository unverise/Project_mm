ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/2.0/core
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/2.0/sensorhub
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include
#USD: add for sunmao4.0 by xihang.jin 20250701 start
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2

obj-$(CONFIG_MTK_SENSORHUB) += sensorhub.o
sensorhub-objs += tiny_crc8.o
sensorhub-objs += tiny_crc32.o
sensorhub-objs += ready.o
sensorhub-objs += ipi_comm.o
sensorhub-objs += sensor_comm.o
sensorhub-objs += sensor_list.o
sensorhub-objs += transceiver.o
sensorhub-objs += share_memory.o
sensorhub-objs += timesync.o
sensorhub-objs += debug.o
sensorhub-objs += custom_cmd.o
sensorhub-objs += sensor_freq.o
sensorhub-objs += transceiver_feature.o
sensorhub-objs += tran_sensor_parse_dts.o

sensorhub-$(CONFIG_MTK_TINYSYS_SAP_SUPPORT) += sap_custom_cmd.o
#USD: add for sunmao4.0 by xihang.jin 20250701 end
#obj-y += test/
