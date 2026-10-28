#ifndef _TRAN_REGULATOR_H_
#define _TRAN_REGULATOR_H_

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/pinctrl/consumer.h>

#define LDOMAX_I2C_ID 20

static struct regmap *debug_regmap[LDOMAX_I2C_ID] = {};
static int chip_id = -1;
static int i2c_id = -1;
static const struct regmap_config tran_ldo_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x0E,
};

static int my_regulator_enable_regmap(struct regulator_dev *rdev);
static int my_regulator_disable_regmap(struct regulator_dev *rdev);

static const struct regulator_ops tran_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = my_regulator_enable_regmap,
	.disable = my_regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
};

struct  tranldo_register{
	int rdis;
	int ldo1vout;
	int ldo2vout;
	int ldo3vout;
	int ldo4vout;
	int seq1;
	int seq2;
	int enable;
};
struct tran_ldo_config {
	int id;
	int n_linear_ranges;
	int n_voltages;
	int enable_time;
	int vsel_mask;
	const struct linear_range *linear_ranges;
	const struct  tranldo_register *reg;
};

#endif