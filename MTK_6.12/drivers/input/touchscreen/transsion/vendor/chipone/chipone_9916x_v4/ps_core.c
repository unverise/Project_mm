#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include "cts_core.h"
#include "transsion_core.h"
#include "cts_sysfs.h"
#include "hf_manager.h"

extern u32 proximity_mode_enable;

struct test_device {
	struct hf_device hf_dev;
};

struct test_device tp_ps_device;


struct sensor_info support_sensors1[] = {
	{
		.sensor_type = SENSOR_TYPE_PROXIMITY,
		.gain = 1,
		.name = {'p', 's'},
		.vendor = {'t', 'p', 'p', 's'},
	},
};

int ps_enable(struct hf_device *hfdev, int sensor_type, int en)
{
	TRAN_INFO("%s sensor_type:%d val:%d\n", __func__, sensor_type, en);
	if (sensor_type == SENSOR_TYPE_PROXIMITY && en == 1) {
		tp_ps_enable(1);
		proximity_mode_enable = 1;
	} else if (sensor_type == SENSOR_TYPE_PROXIMITY && en == 0) {
		tp_ps_enable(0);
		tran_ps_state = 1;
		proximity_mode_enable = 0;
		//tran_gesture_fun_enable = SetDozeStatus();
		//TRAN_INFO("tran_gesture_fun_enable = %d\n",tran_gesture_fun_enable );
	}
	return 0;
}

int ps_batch(struct hf_device *hfdev, int sensor_type,
		int64_t delay, int64_t latency)
{
	TRAN_INFO("%s id:%d delay:%lld latency:%lld\n", __func__, sensor_type, delay, latency);
	return 0;
}

int ps_flush(struct hf_device *hfdev, int sensor_type)
{
	struct test_device *driver_dev = hf_device_get_private_data(hfdev);
	struct hf_manager *manager = driver_dev->hf_dev.manager;
	struct hf_manager_event event;

	TRAN_INFO("%s %s ps_state=%d\n", __func__, driver_dev->hf_dev.dev_name, tran_ps_state);

	memset(&event, 0, sizeof(struct hf_manager_event));
	event.timestamp = get_interrupt_timestamp(manager);
	event.sensor_type = SENSOR_TYPE_PROXIMITY;
	event.accurancy = SENSOR_ACCURANCY_HIGH;
	event.action = FLUSH_ACTION;
	event.word[0] = tran_ps_state;
	manager->report(manager, &event);
	manager->complete(manager);
	return 0;
}

int ps_sample(struct hf_device *hfdev)
{
	struct test_device *driver_dev = hf_device_get_private_data(hfdev);
	struct hf_manager *manager = driver_dev->hf_dev.manager;
	struct hf_manager_event event;

	TRAN_INFO("%s %s ps_state=%d\n", __func__, driver_dev->hf_dev.dev_name, tran_ps_state);

	memset(&event, 0, sizeof(struct hf_manager_event));
	event.timestamp = get_interrupt_timestamp(manager);
	event.sensor_type = SENSOR_TYPE_PROXIMITY;
	event.accurancy = SENSOR_ACCURANCY_HIGH;
	event.action = DATA_ACTION;
	event.word[0] = tran_ps_state;
	manager->report(manager, &event);
	manager->complete(manager);
	return 0;
}

int tp_ps_init(void)
{
	int err = 0;

	tp_ps_device.hf_dev.dev_name = "PROXIMITY";
	tp_ps_device.hf_dev.device_poll = HF_DEVICE_IO_POLLING;
	tp_ps_device.hf_dev.device_bus = HF_DEVICE_IO_SYNC;
	tp_ps_device.hf_dev.support_list = support_sensors1;
	tp_ps_device.hf_dev.support_size = ARRAY_SIZE(support_sensors1);
	tp_ps_device.hf_dev.enable = ps_enable;
	tp_ps_device.hf_dev.batch = ps_batch;
	tp_ps_device.hf_dev.flush = ps_flush;
	tp_ps_device.hf_dev.sample = ps_sample;
	hf_device_set_private_data(&tp_ps_device.hf_dev, &tp_ps_device);
	err = hf_device_register_manager_create(&tp_ps_device.hf_dev);
	if (err < 0) {
		TRAN_ERROR("%s hf_manager_create fail\n", __func__);
		goto hf_device_register_err;
	}
	TRAN_INFO(" %s success\n", __func__);
	return 0;

hf_device_register_err:
	TRAN_ERROR(" %s failed\n", __func__);
	return -EINVAL;
}

/*int __init ps_init(void)
{
	tp_ps_init();
	return 0;
}*/

void __exit ps_exit(void)
{
	TRAN_INFO("%s\n", __func__);
}

/*module_init(ps_init);
module_exit(ps_exit);*/

MODULE_DESCRIPTION("TP simulates sense of distance");
MODULE_AUTHOR("<xiaobo.fan@transsion.com>");
MODULE_LICENSE("GPL v2");
