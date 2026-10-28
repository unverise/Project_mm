// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "tran_custom.h"
static struct pinctrl *pinctrl;
static struct pinctrl_state *pinctrl_i2c_data_gpio;
static struct pinctrl_state *pinctrl_i2c_clk_gpio;

static void tran_i2c_driver_ability_init(struct device *dev)
{
	int ret = 0;
	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		pr_err("Cannot find ftm pinctrl!\n");
		return;
	}
	pinctrl_i2c_clk_gpio = pinctrl_lookup_state(pinctrl, "i2c_ldo_sclk_mode");
	if (IS_ERR(pinctrl_i2c_clk_gpio)) {
		ret = PTR_ERR(pinctrl_i2c_clk_gpio);
		pr_err("Cannot find pinctrl pinctrl_i2c_clk_gpio\n");
	} else {
		pinctrl_select_state(pinctrl, pinctrl_i2c_clk_gpio);
	}
	pinctrl_i2c_data_gpio = pinctrl_lookup_state(pinctrl, "i2c_ldo_sda_mode");
	if (IS_ERR(pinctrl_i2c_data_gpio)) {
		ret = PTR_ERR(pinctrl_i2c_data_gpio);
		pr_err("Cannot find pinctrl pinctrl_i2c_data_gpio\n");
	} else {
		pinctrl_select_state(pinctrl, pinctrl_i2c_data_gpio);
	}
}

static int tran_get_regulator_chip_id(struct regmap *regmap)
{
	int i = 0,j;
	while(i < ARRAY_SIZE(chip_id_addr_table) && chip_id_addr_table[i] != 0xff) {
		if(!regmap_read(regmap, chip_id_addr_table[i], &chip_id)){
			for(j = 0; j < ARRAY_SIZE(tran_regulator_list); j++){
				if(chip_id == tran_regulator_list[j]){
					pr_info("tran_ldo read chip id success=%x\n",chip_id);
					return 0;
				}
			}
		} else {
			pr_err("tran_ldo read chip id failed times = %d\n",i);
		}
		i++;
	}
	pr_err("tran_ldo read chip id failed\n");
	return -ENODEV;
}
static int my_regulator_enable_regmap(struct regulator_dev *rdev)
{
	int ret;
	if ((chip_id == DIO8016_ID || chip_id == WL28661D_ID)){
		pr_info("%s disable discharge resistor!\n",__func__);
		regmap_update_bits(rdev->regmap, et5904_registers.rdis,
						rdev->desc->enable_mask, ~rdev->desc->enable_mask);
	}
	ret = regulator_enable_regmap(rdev);
	return ret;
}

static int my_regulator_disable_regmap(struct regulator_dev *rdev)
{
	int ret;
	if ((chip_id == DIO8016_ID || chip_id == WL28661D_ID)){
		pr_info("%s enable discharge resistor!\n",__func__);
		regmap_update_bits(rdev->regmap, et5904_registers.rdis,
						rdev->desc->enable_mask, rdev->desc->enable_mask);
	}
	ret = regulator_disable_regmap(rdev);
	return ret;
}

static struct regulator_desc* create_tran_ldo_configs(int i2cid, const struct tran_ldo_config *ldod_config, const struct tran_ldo_config *ldoa_config) {
	static struct regulator_desc descs[4];
	int i;

	for (i = 0; i < 4; i++) {
		struct regulator_desc *desc = &descs[i];
		const char* supply = (i < 2) ? "VIND" : "VINA";
		const struct tran_ldo_config *config = (i < 2) ? ldod_config : ldoa_config;
		memset(desc, 0, sizeof(*desc));

		switch (i + 1) {
			case 1:
				desc->vsel_reg = config->reg->ldo1vout;
				break;
			case 2:
				desc->vsel_reg = config->reg->ldo2vout;
				break;
			case 3:
				desc->vsel_reg = config->reg->ldo3vout;
				break;
			case 4:
				desc->vsel_reg = config->reg->ldo4vout;
				break;
			default:
				pr_err("Invalid LDO number: %d\n", i + 1);
				continue; // Skip to the next iteration on error
		}

		desc->name = kasprintf(GFP_KERNEL, "TRANLDO%d%d", i + 1, i2cid);
		if (!desc->name) {
			pr_err("Failed to allocate memory for name\n");
			continue; // Skip to the next iteration on error
		}

		desc->of_match = kasprintf(GFP_KERNEL, "TRANLDO%d%d", i + 1, i2cid);
		if (!desc->of_match) {
			pr_err("Failed to allocate memory for of_match\n");
			kfree(desc->name);
			continue; // Skip to the next iteration on error
		}

		desc->regulators_node = of_match_ptr("regulators");
		desc->type = REGULATOR_VOLTAGE;
		desc->owner = THIS_MODULE;
		desc->linear_ranges = config->linear_ranges;
		desc->n_linear_ranges = config->n_linear_ranges;
		desc->n_voltages = config->n_voltages;
		desc->vsel_mask = config->vsel_mask;
		desc->enable_reg = config->reg->enable;
		desc->enable_mask = BIT(i);
		desc->enable_time = config->enable_time;
		desc->supply_name = supply;
		desc->ops = &tran_ops;
	}

	return descs;
}

static void select_ldo_configs(const struct tran_ldo_config **ldod_config, const struct tran_ldo_config **ldoa_config) {
	int i;
	for(i = 0;i < ARRAY_SIZE(tran_ldo_configs); i++) {
		if (tran_ldo_configs[i].id == chip_id) {
			*ldod_config = &tran_ldo_configs[i];
			*ldoa_config = &tran_ldo_configs[i + 1];
			break;
		}
	}
}
static int tran_regulator_register(struct i2c_client *i2c)
{
	int i = 0,i2cid,ret;
	struct regulator_config config = { };
	struct regulator_dev *rdev;
	struct regulator_desc *regulators = NULL;
	const struct tran_ldo_config *ldod_config = NULL;
	const struct tran_ldo_config *ldoa_config = NULL;

	regulators = devm_kmalloc_array(&i2c->dev, 4, sizeof(*regulators), GFP_KERNEL);
	if (regulators == NULL) {
		pr_err("tran_ldo kmalloc regulators failed\n");
		return -ENOMEM;
	}
	i2cid = i2c_adapter_id(i2c->adapter);
	config.dev = &i2c->dev;
	config.init_data = NULL;

	select_ldo_configs(&ldod_config, &ldoa_config);
	if(ldod_config == NULL || ldoa_config == NULL){
		pr_err("tran_ldo select ldo configs failed\n");
		return -EINVAL;
	}
	regulators = create_tran_ldo_configs(i2cid, ldod_config, ldoa_config);
	if(regulators == NULL){
		pr_err("tran_ldo get regulators failed\n");
		return -EINVAL;
	}
	for (i = 0; i < 4; i++) {
		rdev = devm_regulator_register(&i2c->dev, &regulators[i], &config);
		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			dev_err(&i2c->dev, "Failed to register %s: %d\n", regulators[i].name, ret);
			return ret;
		}
	}
	pr_info("tran_regulator_register success\n");
	return 0;
}

static int tran_ldo_i2c_probe(struct i2c_client *i2c)
{
	struct regmap *regmap;
	int ret;
	unsigned int tranldo_en;
	struct device_node *np = i2c->dev.of_node;

	pr_info("tran_ldo_i2c_probe start\n");
	tran_i2c_driver_ability_init(&i2c->dev);
	regmap = devm_regmap_init_i2c(i2c, &tran_ldo_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(&i2c->dev, "tran_ldo Failed to create regmap: %d\n", ret);
		return ret;
	}
	ret = tran_get_regulator_chip_id(regmap);
	if (ret < 0) {
		dev_err(&i2c->dev, "tran_ldo Invalid chip id\n");
		return -EINVAL;
	}

	tranldo_en = of_get_named_gpio(np,"tranldo_en",0);
	if (gpio_is_valid(tranldo_en)) {
		ret = gpio_request(tranldo_en, "tranldo_en");
		if (ret < 0) {
			pr_err("gpio request fail!");
		}else{
			gpio_direction_output(tranldo_en, 0);
		}
		if(chip_id == 0x04){  //dio8016
			gpio_set_value(tranldo_en, 1);
		}else{
			gpio_set_value(tranldo_en, 0);
		}
	}else{
		pr_err("tranldo_en get fail!");
	}

	i2c_id = i2c_adapter_id(i2c->adapter);
	if(i2c_id >= 0 &&i2c_id <= LDOMAX_I2C_ID){
		debug_regmap[i2c_id] = regmap;
	}else{
		pr_err("tran_ldo i2c id is invalid\n");
		return -EINVAL;
	}

	ret = tran_regulator_register(i2c);
	if(ret < 0){
		pr_err("tran_ldo Failed to register regulator: %d\n", ret);
	}
	pr_info("tran_ldo_i2c_probe end\n");
	return ret;
}

static void tranldo_reg_shutdown(struct i2c_client *i2c)
{
#if IS_ENABLED(CONFIG_TRAN_DUAL_INTERNAL_TEMP_PATCH)
#if IS_ENABLED(TRAN_AE10)
	int i2cid;
	unsigned int val;
	i2cid = i2c_adapter_id(i2c->adapter);
	if(i2cid == MAINLCM_LDO_ID || i2cid == LCM_SUB_LDO_ID){
		if(debug_regmap[i2cid] != NULL){
			printk("[et5904][%d]: shutdown entry", i2cid);
			regmap_update_bits(debug_regmap[i2cid], ET5904_ENABLE, 0xff, 0x00);
			regmap_read(debug_regmap[i2cid], ET5904_ENABLE, &val);
			printk("[et5904][%d]:dump ET5904_ENABLE = 0x%x", i2cid, val);
		}
	}
#endif
#endif
}

#ifdef CONFIG_OF
static const struct of_device_id tran_ldo_dt_ids[] = {
	{ .compatible = "tran,tran_ldo", },
	{}
};

MODULE_DEVICE_TABLE(of, tran_ldo_dt_ids);
#endif

static const struct i2c_device_id tran_ldo_i2c_id[] = {
	{ "tran_ldo", },
	{}
};
MODULE_DEVICE_TABLE(i2c, tran_ldo_i2c_id);

static struct i2c_driver tran_ldo_regulator_driver = {
	.driver = {
		.name = "tran_ldo",
		.of_match_table = of_match_ptr(tran_ldo_dt_ids),
	},
	.probe = tran_ldo_i2c_probe,
	.id_table = tran_ldo_i2c_id,
	.shutdown = tranldo_reg_shutdown,
};
module_i2c_driver(tran_ldo_regulator_driver);

MODULE_DESCRIPTION("Tran 4LDO  driver");
MODULE_AUTHOR("fei wu5 <fei.wu5@transsion.com>");
MODULE_LICENSE("GPL");
