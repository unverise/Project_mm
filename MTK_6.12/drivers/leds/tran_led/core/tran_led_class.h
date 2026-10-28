#ifndef LINUX_TRAN_LED_CLASS_H
#define LINUX_TRAN_LED_CLASS_H

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/cdev.h>

#define TRAN_LED_CMD_LENGTH 8



enum ChipType {
	DRV_OTHER = 0,
	AW2013,
	AW2023,
	AW2033,
	AW2026,
	AW22118,
	AW22127,

	/* hal + algo so solution */
	HAL_SO_SOLUTION_CHIP = 100,
	AW20144 = HAL_SO_SOLUTION_CHIP,
	AW20216,
	AW21009,
	AW21012,
	AW21018,
	LIB_OTHER,
	TYPE_MAX,
};

enum CmdType {
	BIN_SOLUTION = 0x00,
	DRIVER_SOLUTION,
	/* hal + algo so solution */
	HAL_SO_SOLUTION,
    DRIVER_FRAME,
};

struct tran_led_properties {
	const char *alias_name;
};

struct tran_led_device {
	struct tran_led_properties props;
	const struct tran_led_ops *ops;
	struct platform_device *pdev;
	struct srcu_notifier_head evt_nh;
	struct device dev;
	struct led_classdev cdev;
	const struct attribute_group *dev_groups;
};

struct tran_led_ops {
	int (*set_i2c_retries)(struct tran_led_device *dev, unsigned long arg);
	int (*set_i2c_timeout)(struct tran_led_device *dev, unsigned long arg);
	int (*rw_i2c_data)(struct tran_led_device *dev, unsigned long arg);
	int (*get_chip_type)(struct tran_led_device *dev, enum ChipType * p);
	int (*get_tran_led_cmd)(struct tran_led_device *dev, int32_t * cmd_data,int size);
	int (*set_tran_led_cmd)(struct tran_led_device *dev, int32_t * cmd_data,int size);
	int (*get_frame_size)(struct tran_led_device *dev);

	int (*store_tran_led_cmd)(struct tran_led_device *dev,const char *buf);
	int (*show_tran_led_cmd)(struct tran_led_device *dev, char *buf);
	int (*store_tran_led_check)(struct tran_led_device *dev,const char *buf);
	int (*show_tran_led_check)(struct tran_led_device *dev, char *buf);
};

/********** API FOR APP **********/
extern int tran_led_dev_set_i2c_retries(struct tran_led_device *dev, unsigned long arg);
extern int tran_led_dev_set_i2c_timeout(struct tran_led_device *dev, unsigned long arg);
extern int tran_led_dev_rw_i2c_data(struct tran_led_device *dev, unsigned long arg);
extern int tran_led_dev_get_chip_type(struct tran_led_device *dev, enum ChipType * p);
extern int tran_led_dev_set_cmd(struct tran_led_device *dev, int32_t * cmd_data,int size);
extern int tran_led_dev_get_cmd(struct tran_led_device *dev, int32_t * cmd_data,int size);

/********** API FOR MIDTEST MODE OR FACTORY MODE **********/
extern int tran_led_dev_store_tran_led_cmd(struct tran_led_device *dev,const char *buf);
extern int tran_led_dev_show_tran_led_cmd(struct tran_led_device *dev, char *buf);
extern int tran_led_dev_store_tran_led_check(struct tran_led_device *dev,const char *buf);
extern int tran_led_dev_show_tran_led_check(struct tran_led_device *dev, char *buf);

extern struct tran_led_device *tran_led_device_register(const char *name,
		struct device *parent, void *devdata,
		const struct tran_led_ops *ops,
		const struct tran_led_properties *props);
extern void tran_led_device_unregister(struct tran_led_device *led_dev);
extern struct tran_led_device *get_leddev_by_name(const char *name);
extern int tran_led_class_init(struct platform_device *pdev);
extern int tran_led_class_destroy(struct platform_device *pdev);

#define to_led_device(obj) container_of(obj, struct tran_led_device, dev)

#endif
