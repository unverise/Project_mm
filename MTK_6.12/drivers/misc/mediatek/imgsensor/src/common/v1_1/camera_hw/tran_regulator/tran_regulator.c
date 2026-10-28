// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "tran_regulator.h"

//ITD: modify regulator voltage by qinlong.lu 2023/11/07 start
static DEFINE_MUTEX(ext_ldo_mutex);
static const int regulator_voltage[] = {
	REGULATOR_VOLTAGE_0,
	REGULATOR_VOLTAGE_900,
	REGULATOR_VOLTAGE_1000,
	REGULATOR_VOLTAGE_1002,
	REGULATOR_VOLTAGE_1050,
	REGULATOR_VOLTAGE_1098,
	REGULATOR_VOLTAGE_1100,
	REGULATOR_VOLTAGE_1200,
	REGULATOR_VOLTAGE_1210,
	REGULATOR_VOLTAGE_1220,
	REGULATOR_VOLTAGE_1250,
	REGULATOR_VOLTAGE_1260,
	REGULATOR_VOLTAGE_1270,
	REGULATOR_VOLTAGE_1280,
	REGULATOR_VOLTAGE_1290,
	REGULATOR_VOLTAGE_1300,
	REGULATOR_VOLTAGE_1500,
	REGULATOR_VOLTAGE_1800,
	REGULATOR_VOLTAGE_1804,
	REGULATOR_VOLTAGE_2200,
	REGULATOR_VOLTAGE_2300,
	REGULATOR_VOLTAGE_2500,
	REGULATOR_VOLTAGE_2800,
	REGULATOR_VOLTAGE_2900,
	REGULATOR_VOLTAGE_3300,
};

struct REGULATOR_CTRL tran_regulator_control[REGULATOR_TYPE_MAX_NUM] = {
	{"vcampdn"},
	{"vcamrst"},
	{"vcama"},
	{"vcama1"},
	{"vcamaf"},
	{"oisvdd"},
	{"oisvdda"},
	{"oisvddio"},
	{"vcamd"},
	{"vcamd1"},
	{"vcamio"},
};

static struct REGULATOR reg_instance;

static enum IMGSENSOR_RETURN regulator_init(
	void *pinstance,
	struct IMGSENSOR_HW_DEVICE_COMMON *pcommon)
{
	struct REGULATOR *preg = (struct REGULATOR *)pinstance;
	int type, idx, ret = 0;
	char str_regulator_name[LENGTH_FOR_SNPRINTF];

	for (idx = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		idx < IMGSENSOR_SENSOR_IDX_MAX_NUM;
		idx++) {
		for (type = 0;
			type < REGULATOR_TYPE_MAX_NUM;
			type++) {
			memset(str_regulator_name, 0,
				sizeof(str_regulator_name));
			ret = snprintf(str_regulator_name,
				sizeof(str_regulator_name),
				"cam%d_%s",
				idx,
				tran_regulator_control[type].pregulator_type);
			if (ret < 0)
				return ret;
			preg->pregulator[idx][type] = regulator_get_optional(
					&pcommon->pplatform_device->dev,
					str_regulator_name);
			PK_PR_ERR("regulator init [%d][%d] %s +\n",
						idx, type, str_regulator_name);
			if (IS_ERR(preg->pregulator[idx][type])) {
				preg->pregulator[idx][type] = NULL;
				PK_INFO("ERROR: regulator[%d][%d]  %s fail!\n",
						idx, type, str_regulator_name);
			}
			atomic_set(&preg->enable_cnt[idx][type], 0);
		}
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN regulator_release(void *pinstance)
{
	struct REGULATOR *preg = (struct REGULATOR *)pinstance;
	int type, idx;
	struct regulator *pregulator = NULL;
	atomic_t *enable_cnt = NULL;

	for (idx = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		idx < IMGSENSOR_SENSOR_IDX_MAX_NUM;
		idx++) {

		for (type = 0; type < REGULATOR_TYPE_MAX_NUM; type++) {
			pregulator = preg->pregulator[idx][type];
			enable_cnt = &preg->enable_cnt[idx][type];
			if (pregulator != NULL) {
				for (; atomic_read(enable_cnt) > 0; ) {
					regulator_disable(pregulator);
					atomic_dec(enable_cnt);
				}
			}
		}
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN regulator_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN	   pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	struct regulator	 *pregulator;
	struct REGULATOR	 *preg = (struct REGULATOR *)pinstance;
	int reg_type_offset;
	atomic_t			 *enable_cnt;
	unsigned int voltage;

	if ((pin == IMGSENSOR_HW_PIN_PDN || pin == IMGSENSOR_HW_PIN_RST) && pin_state == IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH)
		pin_state = IMGSENSOR_HW_PIN_STATE_LEVEL_1800;

	if (pin > IMGSENSOR_HW_PIN_DOVDD   ||
	    pin < IMGSENSOR_HW_PIN_PDN    ||
	    pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
            pin_state >= IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH)
		return IMGSENSOR_RETURN_ERROR;

	reg_type_offset = REGULATOR_TYPE_VCAMPDN; //0
	PK_PR_ERR("[regulator]preg->pregulator + %d %d %d %d",reg_type_offset, (reg_type_offset/*0*/ + pin - IMGSENSOR_HW_PIN_PDN), IMGSENSOR_HW_PIN_PDN, pin);
	if((reg_type_offset/*0*/ + pin - IMGSENSOR_HW_PIN_PDN) >= REGULATOR_TYPE_MAX_NUM) return IMGSENSOR_RETURN_ERROR;
	pregulator = preg->pregulator[(unsigned int)sensor_idx][
		reg_type_offset/*0*/ + pin - IMGSENSOR_HW_PIN_PDN/*3*/];
	enable_cnt = &preg->enable_cnt[(unsigned int)sensor_idx][
		reg_type_offset + pin - IMGSENSOR_HW_PIN_PDN];

	if (pregulator) {
		if (pin_state != IMGSENSOR_HW_PIN_STATE_LEVEL_0) {
			voltage = regulator_voltage[pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0];
		//[ITD]modify by haiyang.he for 4LDO compatible 20250911 start
			if (regulator_set_voltage(pregulator,
				regulator_voltage[
				pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0],
				regulator_voltage[
				pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0] + 20000)) {
		//[ITD]modify by haiyang.he for 4LDO compatible 20250911 end
				PK_PR_ERR(
				  "[regulator]fail to regulator_set_voltage, powertype:%d powerId:%d\n",
				  pin,
				  regulator_voltage[
				  pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
			}
			if (regulator_enable(pregulator)) {
				PK_PR_ERR(
				"[regulator]fail to regulator_enable, powertype:%d powerId:%d\n",
				pin,
				regulator_voltage[
				  pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
				return IMGSENSOR_RETURN_ERROR;
			}
			atomic_inc(enable_cnt);
		} else {
			if (regulator_is_enabled(pregulator)) {
				PK_DBG("[regulator]%d is enabled\n", pin);
				if (regulator_disable(pregulator)) {
					PK_PR_ERR("[regulator]fail to regulator_disable, powertype: %d\n", pin);
					return IMGSENSOR_RETURN_ERROR;
				}
			}
			atomic_dec(enable_cnt);
		}
	} else {
		PK_PR_ERR("regulator == NULL %d %d %d\n",
				reg_type_offset,
				pin,
				IMGSENSOR_HW_PIN_AVDD);
	}

	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN regulator_dump(void *pinstance)
{
	struct REGULATOR *preg = (struct REGULATOR *)pinstance;
	int i, j;
	int enable = 0;

	for (j = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		j < IMGSENSOR_SENSOR_IDX_MAX_NUM;
		j++) {

		for (i = REGULATOR_TYPE_VCAMPDN;
		i < REGULATOR_TYPE_MAX_NUM;
		i++) {
			if (!preg->pregulator[j][i])
				continue;

			if (regulator_is_enabled(preg->pregulator[j][i]) &&
				atomic_read(&preg->enable_cnt[j][i]) != 0)
				enable = 1;
			else
				enable = 0;

			PK_DBG("[sensor_dump][regulator] index= %d, %s = %d, enable = %d\n",
				j,
				tran_regulator_control[i].pregulator_type,
				regulator_get_voltage(preg->pregulator[j][i]),
				enable);
		}
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

static struct IMGSENSOR_HW_DEVICE device = {
	.id	= IMGSENSOR_HW_ID_REGULATOR,
	.pinstance = (void *)&reg_instance,
	.init	  = regulator_init,
	.set	   = regulator_set,
	.release   = regulator_release,
	.dump	  = regulator_dump
};

enum IMGSENSOR_RETURN imgsensor_hw_regulator_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	return IMGSENSOR_RETURN_SUCCESS;
}
//ITD: modify regulator voltage by qinlong.lu 2023/11/07 end

