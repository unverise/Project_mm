ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensors-1.0/accelerometer/inc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensors-1.0/heart_rhythm/hx3918
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/2.0/core
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/hr_spo2/tran_hr_spo2

obj-m   +=  tran_hx3918.o
tran_hx3918-objs = tyhx_linux_hx3918.o hx3918.o hx3918_factory_test.o hx3918_hrs_agc.o hx3918_spo2_agc.o tyhx_hrs_alg.o tyhx_spo2_alg.o
