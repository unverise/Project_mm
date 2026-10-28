#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/crc8.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include "tc_common_class.h"
#include "tc_misc_intf.h"
#include "tc_ta_class.h"
#include "tc_charger_class.h"

#include <linux/pinctrl/consumer.h>

#define TFCP_TA_DRV_VERSION "1.0.1_G"

/* Register table */
#define TFCP_REG_VERINFO		0xA0
#define TFCP_REG_FWCODE		0xA1
#define TFCP_REG_MFINFO		0xA2
#define TFCP_REG_DATECODE	0xA3
#define TFCP_REG_MAXPWR		0xA4
#define TFCP_REG_MAXVC		0xA5
#define TFCP_REG_MINVC		0xA6
#define TFCP_REG_READVC		0xA7
#define TFCP_REG_CHGSTAT		0xA8
#define TFCP_REG_TEMP		0xA9
#define TFCP_REG_READ_AUTH	0xAA
#define TFCP_REG_OUTPUT_CTRL	0xB0
#define TFCP_REG_MODE		0xB1
#define TFCP_REG_SETVC		0xB2
#define TFCP_REG_WDT		0xB3
#define TFCP_REG_SET_AUTH	0xB4

#define TFCP_BLOCK_MOSFET_SUPPORT_MASK 0x20000000

#define TFCP_VOLT_SHFT	0
#define TFCP_VOLT_MASK	0xFFFF
#define TFCP_CURR_SHFT	16
#define TFCP_CURR_MASK	0xFFFF0000
#define TFCP_WDTEN_MASK	BIT(1)

#define TFCP_MAX_POWER_DURATION_SHFT	12
#define TFCP_MAX_POWER_DURATION_MASK	0xF000
#define I2C_RETRY_COUNT	5
#define I2CW_CHECK_COUNT 5

enum ta_power{
	TA_25W = 25,
	TA_33W = 33,
	TA_45W = 45,
	TA_68W = 68,
	TA_120W = 120,
	TA_180W = 180,
	TA_MAX,
};

enum ta_power_timer{
	TA_180W_T = 1,
	TA_120W_T = 3,
	TA_68W_T = 5,
	TA_45W_T = 15,
	TA_33W_T = 20,
	TA_25W_T = 30,
	TA_TIME_MAX,
};

enum accbyte{
	ACC_BYTE0 = 1,
	ACC_BYTE1 = 2,
	ACC_BYTE2 = 4,
	ACC_BYTE3 = 8,
	ACC_BYTE4 = 16,
	ACC_BYTE_MAX,
};

static u8 tfcp_crc_tbl[CRC8_TABLE_SIZE];

struct tfcp_info {
	struct device *dev;
	struct i2c_client *client;
	struct tc_ta_classdev ptc;
	struct mutex io_lock;
	struct mutex suspend_lock;
	u32 sha256_msg[64];
	u32 sha256_state[8];
	struct power_supply *psy;
	u32 tfcp_shutdown;
	struct tran_device *ac_ctl_dev;
};

static bool tfcp_ta_can_communicate(struct tfcp_info *ri)
{
	union com_propval tran_val = {0, };
	int chg_type;

	if (!ri->ac_ctl_dev)
		ri->ac_ctl_dev = tran_get_by_name("adapter_control");

	chg_type = tc_get_alias_type();
	if (chg_type == TC_UNKNOWN) {
		pr_err("TA plug out, forbid communication\n");
		return false;
	}

	tran_dev_get_prop(ri->ac_ctl_dev, TRAN_PROP_ADAPTER_SWITCH_STATUS, &tran_val);
	if (tran_val.intval != 0) {
		pr_err("tran command close TA output, forbid communication\n");
		return false;
	}

	return true;
}

static int tfcp_ta_read_device(struct i2c_client *client, u8 addr, int len, u8 *dst)
{
	int ret, retry = 0;
	u8 crc, data[256];
	struct tfcp_info *ri = i2c_get_clientdata(client);
	int i;

	if (ri->tfcp_shutdown == 1)
		return 0;

	pm_stay_awake(ri->dev);
	do {
		mutex_lock(&ri->suspend_lock);
		charger_dev_i2c_trans(get_charger_by_name("primary_dvchg"), true);

		if (!tfcp_ta_can_communicate(ri)) {
			ret = -EINVAL;
			dev_err(ri->dev, "%s %d\n", __func__, __LINE__);
			goto out;
		}

		data[0] = addr;
		data[1] = crc8(tfcp_crc_tbl, data, 1, 0);
		ret = i2c_master_send(client, data, 2);
		if (ret < 0){
			dev_err(ri->dev, "%s ret=%d a\n", __func__, ret);
			goto retry;
		}
		usleep_range(800, 800);
		ret = i2c_master_recv(client, data, len + 1);
		if (ret < 0){
			dev_err(ri->dev, "%s ret=%d b\n", __func__, ret);
			goto retry;
		}
		crc = crc8(tfcp_crc_tbl, data, len, 0);
		if (crc != data[len]) {
			dev_err(ri->dev, "%s ret=%d c\n", __func__, ret);
			for (i = 0; i <= len; i++)
				dev_err(ri->dev, "%s data[%d]=%d c\n",__func__, i, data[i]);
			ret = -EINVAL;
			goto retry;
		}
		memcpy(dst, data, len);
		ret = 0;
		goto out;

retry:
		retry++;
		if (retry >= I2C_RETRY_COUNT) {
			dev_err(ri->dev, "%s retry count exceeded\n", __func__);
			goto out;
		}

		udelay(20);
		charger_dev_i2c_trans(get_charger_by_name("primary_dvchg"), false);
		usleep_range(800, 800);
		mutex_unlock(&ri->suspend_lock);
	} while (ret < 0 && retry < I2C_RETRY_COUNT);

out:
	charger_dev_i2c_trans(get_charger_by_name("primary_dvchg"), false);
	usleep_range(800, 800);
	mutex_unlock(&ri->suspend_lock);

	pm_relax(ri->dev);
	return (ret >= 0) ? 0 : ret;
}

static int tfcp_ta_write_device(struct i2c_client *client, u8 addr, int len, const u8 *src)
{
	int ret, retry = 0;
	struct tfcp_info *ri = i2c_get_clientdata(client);
	u8 data[256];

	if (ri->tfcp_shutdown == 1)
		return 0;

	pm_stay_awake(ri->dev);
	mutex_lock(&ri->suspend_lock);
	charger_dev_i2c_trans(get_charger_by_name("primary_dvchg"), true);
	do {
		if (!tfcp_ta_can_communicate(ri)) {
			ret = -EINVAL;
			dev_err(ri->dev, "%s %d\n", __func__, __LINE__);
			goto out;
		}

		data[0] = addr;
		data[1] = len;
		memcpy(&data[2], src, len);
		data[len + 2] = crc8(tfcp_crc_tbl, data, len + 2, 0);
		ret = i2c_master_send(client, data, len + 3);
		if (ret < 0) {
			retry++;
			udelay(20);
			dev_err(ri->dev, "%s ret=%d\n", __func__, ret);
		}
	} while (ret < 0 && retry < I2C_RETRY_COUNT);
out:
	charger_dev_i2c_trans(get_charger_by_name("primary_dvchg"), false);
	usleep_range(800, 800);
	mutex_unlock(&ri->suspend_lock);
	pm_relax(ri->dev);
	return (ret >= 0) ? 0 : ret;
}

static int tfcp_ta_i2c_write8_none_read_back(struct tfcp_info *ri, u8 addr, u8 data)
{
	int ret, retry = 0;

	mutex_lock(&ri->io_lock);
	do {
		ret = tfcp_ta_write_device(ri->client, addr, 1, &data);
		if (ret < 0) {
			dev_err(ri->dev, "write none read back failed, ret:%d\n", ret);
			goto retry;
		}
		break;
retry:
		retry++;
		udelay(20);
	} while (ret < 0 && retry < I2CW_CHECK_COUNT);
	mutex_unlock(&ri->io_lock);
	return ret;
}

static int __maybe_unused tfcp_ta_i2c_read8(struct tfcp_info *ri, u8 addr, u8 *data)
{
	int ret;

	mutex_lock(&ri->io_lock);
	ret = tfcp_ta_read_device(ri->client, addr, ACC_BYTE0, data);
	mutex_unlock(&ri->io_lock);
	return ret;
}

static int tfcp_ta_i2c_read16(struct tfcp_info *ri, u8 addr, u16 *data)
{
	int ret;
	u16 _data;

	mutex_lock(&ri->io_lock);
	ret = tfcp_ta_read_device(ri->client, addr, ACC_BYTE1, (u8 *)&_data);
	if (ret < 0)
		goto out;
	*data = le16_to_cpu(_data);
out:
	mutex_unlock(&ri->io_lock);
	return ret;
}

static int tfcp_ta_i2c_read32(struct tfcp_info *ri, u8 addr, u32 *data)
{
	int ret;
	u32 _data;

	mutex_lock(&ri->io_lock);
	ret = tfcp_ta_read_device(ri->client, addr, ACC_BYTE2, (u8 *)&_data);
	if (ret < 0)
		goto out;
	*data = le32_to_cpu(_data);
out:
	mutex_unlock(&ri->io_lock);
	return ret;
}

static int tfcp_ta_i2c_read_block(struct tfcp_info *ri, u8 addr, u32 len, u8 *data)
{
	int ret;

	mutex_lock(&ri->io_lock);
	ret = tfcp_ta_read_device(ri->client, addr, len, data);
	mutex_unlock(&ri->io_lock);
	return ret;
}

static int tfcp_ta_i2c_write8(struct tfcp_info *ri, u8 addr, u8 data)
{
	int ret, retry = 0;
	u8 rdata;

	mutex_lock(&ri->io_lock);
	do {
		ret = tfcp_ta_write_device(ri->client, addr, ACC_BYTE0, &data);
		if (ret < 0)
			goto retry;
		ret = tfcp_ta_read_device(ri->client, addr, ACC_BYTE0, &rdata);
		if (ret < 0)
			goto retry;
		if (rdata != data) {
			dev_err(ri->dev, "%s check fail(0x%02X,0x%02X)\n",
				__func__, data, rdata);
			ret = -EINVAL;
			goto retry;
		}
		break;
retry:
		retry++;
		udelay(20);
	} while (ret < 0 && retry < I2CW_CHECK_COUNT);
	mutex_unlock(&ri->io_lock);
	return ret;
}

static int tfcp_ta_i2c_write16(struct tfcp_info *ri, u8 addr, u16 data)
{
	int ret, retry = 0;
	u16 _data, rdata;

	mutex_lock(&ri->io_lock);
	do {
		_data = cpu_to_le16(data);
		ret = tfcp_ta_write_device(ri->client, addr, ACC_BYTE1, (u8 *)&_data);
		if (ret < 0){
			dev_err(ri->dev, "%s data=%d, ret=%d aa\n",__func__, data, ret);
			goto retry;
		}
		ret = tfcp_ta_read_device(ri->client, addr,ACC_BYTE1, (u8 *)&rdata);
		if (ret < 0){
			dev_err(ri->dev, "%s data=%d, ret=%d BB\n",__func__, data, ret);
			goto retry;
		}
		rdata = le16_to_cpu(rdata);
		if (rdata != _data) {
			dev_err(ri->dev, "%s check fail(0x%04X,0x%04X)\n",
				__func__, _data, rdata);
			ret = -EINVAL;
			goto retry;
		}
		break;
retry:
		retry++;
		udelay(20);
	} while (ret < 0 && retry < I2CW_CHECK_COUNT);
	mutex_unlock(&ri->io_lock);
	return ret;
}

static int tfcp_ta_i2c_write32(struct tfcp_info *ri, u8 addr, u32 data)
{
	int ret, retry = 0;
	u32 _data, rdata;

	mutex_lock(&ri->io_lock);

	do {
		_data = cpu_to_le32(data);
		ret = tfcp_ta_write_device(ri->client, addr, ACC_BYTE2, (u8 *)&_data);
		if (ret < 0)
			goto retry;
		ret = tfcp_ta_read_device(ri->client, addr, ACC_BYTE2, (u8 *)&rdata);
		if (ret < 0)
			goto retry;
		rdata = le32_to_cpu(rdata);
		if (rdata != _data) {
			dev_err(ri->dev, "%s check fail(0x%08X,0x%08X)\n",
				__func__, data, rdata);
			ret = -EINVAL;
			goto retry;
		}
		break;
retry:
		retry++;
		udelay(20);
	} while (ret < 0 && retry < I2CW_CHECK_COUNT);

	mutex_unlock(&ri->io_lock);
	return ret;
}

static int __maybe_unused tfcp_ta_i2c_write_block(struct tfcp_info *ri, u8 addr, u32 len, const u8 *data)
{
	int ret, retry = 0, i;
	u8 rdata[256];

	mutex_lock(&ri->io_lock);

	do {
		ret = tfcp_ta_write_device(ri->client, addr, len, data);
		if (ret < 0){
			dev_err(ri->dev, "%s write fail\n", __func__);
			goto retry;
		}
		usleep_range(5000, 6000);
		ret = tfcp_ta_read_device(ri->client, addr, len, rdata);
		if (ret < 0){
			dev_err(ri->dev, "%s write fail\n", __func__);
			goto retry;
		}
		if (memcmp(data, rdata, len) != 0) {
			dev_err(ri->dev, "%s check fail\n", __func__);
			for (i = 0; i < len; i++)
				dev_err(ri->dev, "%s (0x%02X, 0x%02X)\n",
					__func__, rdata[i], data[i]);
			ret = -EINVAL;
			goto retry;
		}
		break;
retry:
		retry++;
		udelay(20);
	} while (ret < 0 && retry < I2CW_CHECK_COUNT);

	mutex_unlock(&ri->io_lock);
	return ret;
}

static int tfcp_ta_i2c_update32(struct tfcp_info *ri, u8 addr, u32 data, u32 mask)
{
	int ret, retry = 0;
	u32 _data, rdata;

	mutex_lock(&ri->io_lock);

	do {
		ret = tfcp_ta_read_device(ri->client, addr, ACC_BYTE2, (u8 *)&_data);
		if (ret < 0)
			goto retry;
		_data = le32_to_cpu(_data);
		_data &= ~mask;
		_data |= (data & mask);
		_data = cpu_to_le32(_data);
		ret = tfcp_ta_write_device(ri->client, addr, ACC_BYTE2, (u8 *)&_data);
		if (ret < 0)
			goto retry;
		ret = tfcp_ta_read_device(ri->client, addr, ACC_BYTE2, (u8 *)&rdata);
		if (ret < 0)
			goto retry;
		rdata = le32_to_cpu(rdata);
		if (rdata != _data) {
			dev_err(ri->dev, "%s check fail(0x%08X,0x%08X)\n",
				__func__, _data, rdata);
			ret = -EINVAL;
			goto retry;
		}
		break;
retry:
		retry++;
		udelay(20);
	} while (ret < 0 && retry < I2CW_CHECK_COUNT);
	mutex_unlock(&ri->io_lock);
	return ret;
}

static int tfcp_ta_i2c_update8(struct tfcp_info *ri, u8 addr, u8 data, u8 mask)
{
	int ret, retry = 0;
	u8 _data, rdata;

	mutex_lock(&ri->io_lock);

	do {
		ret = tfcp_ta_read_device(ri->client, addr, ACC_BYTE0, &_data);
		if (ret < 0)
			goto retry;
		_data &= ~mask;
		_data |= (data & mask);
		ret = tfcp_ta_write_device(ri->client, addr, ACC_BYTE0, &_data);
		if (ret < 0)
			goto retry;
		ret = tfcp_ta_read_device(ri->client, addr, ACC_BYTE0, &rdata);
		if (ret < 0)
			goto retry;
		if (rdata != _data) {
			dev_err(ri->dev, "%s check fail(0x%02X,0x%02X)\n",
				__func__, _data, rdata);
			ret = -EINVAL;
			goto retry;
		}
		break;
retry:
		retry++;
		udelay(20);
	} while (ret < 0 && retry < I2CW_CHECK_COUNT);
	mutex_unlock(&ri->io_lock);
	return ret;
}

static inline int tfcp_ta_i2c_set8(struct tfcp_info *ri, u8 addr, u8 mask)
{
	return tfcp_ta_i2c_update8(ri, addr, mask, mask);
}

static inline int tfcp_ta_i2c_clr8(struct tfcp_info *ri, u8 addr, u8 mask)
{
	return tfcp_ta_i2c_update8(ri, addr, 0x00, mask);
}

static struct tfcp_info *to_tfcp_info(struct tc_ta_classdev *ptc)
{
	return container_of(ptc, struct tfcp_info, ptc);
}

static int tfcp_tc_ta_get_verinfo(struct tc_ta_classdev *ptc, u8 *info, u32 length)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);
	int ret;
	u32 data;
	u8 *_info = (u8 *)&data;

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_VERINFO, &data);
	if (ret < 0)
		return ret;
	length = (length > ACC_BYTE2) ? ACC_BYTE2 : length;
	memcpy(info, _info, length);
	dev_info(ri->dev, "%s 0x%04X\n", __func__, data);
	return 0;
}

static int tfcp_tc_ta_get_fwcode(struct tc_ta_classdev *ptc, u8 *code, u32 length)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);
	int ret;
	u32 data;
	u8 *_code = (u8 *)&data;

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_FWCODE, &data);
	if (ret < 0)
		return ret;
	length = (length > ACC_BYTE2) ? ACC_BYTE2 : length;
	memcpy(code, _code, length);
	dev_info(ri->dev, "%s 0x%04X\n", __func__, data);
	return 0;
}

static int tfcp_tc_ta_get_mfinfo(struct tc_ta_classdev *ptc, u8 *code, u32 length)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);
	int ret;
	u8 data[8];
	u8 *_code = (u8 *)data;

	ret = tfcp_ta_i2c_read_block(ri, TFCP_REG_MFINFO, ACC_BYTE3, data);
	if (ret < 0)
		return ret;
	length = (length > ACC_BYTE3) ? ACC_BYTE3 : length;
	memcpy(code, _code, length);

	return 0;
}

static int tfcp_tc_ta_get_datecode(struct tc_ta_classdev *ptc, u8 *code, u32 length)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);
	int ret;
	u32 data;
	u8 *_code = (u8 *)&data;

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_DATECODE, &data);
	if (ret < 0)
		return ret;
	length = (length > ACC_BYTE2) ? ACC_BYTE2 : length;
	memcpy(code, _code, length);
	dev_info(ri->dev, "%s 0x%04X\n", __func__, data);
	return 0;
}

static int tfcp_tc_ta_get_min_voltage(struct tc_ta_classdev *ptc, u32 *volt)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u32 data;

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_MINVC, &data);
	if (ret < 0)
		return ret;
	*volt = (data & TFCP_VOLT_MASK) >> TFCP_VOLT_SHFT;
	dev_info(ri->dev, "%s %d\n", __func__, *volt);
	return 0;
}

static int tfcp_tc_ta_get_max_voltage(struct tc_ta_classdev *ptc, u32 *volt)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u32 data;

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_MAXVC, &data);
	if (ret < 0)
		return ret;
	*volt = (data & TFCP_VOLT_MASK) >> TFCP_VOLT_SHFT;
	dev_info(ri->dev, "%s %d\n", __func__, *volt);
	return 0;
}

static int tfcp_tc_ta_get_min_current(struct tc_ta_classdev *ptc, u32 *curr)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u32 data;

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_MINVC, &data);
	if (ret < 0)
		return ret;
	*curr = (data & TFCP_CURR_MASK) >> TFCP_CURR_SHFT;
	dev_info(ri->dev, "%s %d\n", __func__, *curr);
	return 0;
}

static int tfcp_tc_ta_get_max_current(struct tc_ta_classdev *ptc, u32 *curr)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u32 data;

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_MAXVC, &data);
	if (ret < 0)
		return ret;
	*curr = (data & TFCP_CURR_MASK) >> TFCP_CURR_SHFT;
	dev_info(ri->dev, "%s %d\n", __func__, *curr);
	return 0;
}

static int tfcp_tc_ta_get_output_voltage(struct tc_ta_classdev *ptc,u32 *volt)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u32 data;

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_READVC, &data);
	if (ret < 0)
		return ret;
	*volt = (data & TFCP_VOLT_MASK) >> TFCP_VOLT_SHFT;
	dev_info(ri->dev, "%s %d\n", __func__, *volt);
	return 0;
}

static int tfcp_tc_ta_get_output_current(struct tc_ta_classdev *ptc,u32 *curr)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u32 data;

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_READVC, &data);
	if (ret < 0)
		return ret;
	*curr = (data & TFCP_CURR_MASK) >> TFCP_CURR_SHFT;
	dev_info(ri->dev, "%s %d\n", __func__, *curr);
	return 0;
}

static int tfcp_tc_ta_get_status(struct tc_ta_classdev *ptc, u32 *status)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_CHGSTAT, status);
	if (ret < 0)
		return ret;
	dev_info(ri->dev, "%s 0x%04X\n", __func__, *status);
	return 0;
}

static int tfcp_tc_ta_get_temp1(struct tc_ta_classdev *ptc, u32 *degree)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u16 data;

	ret = tfcp_ta_i2c_read16(ri, TFCP_REG_TEMP, &data);
	if (ret < 0)
		return ret;
	*degree = (data & 0xff);
	dev_info(ri->dev, "%s %d\n", __func__, *degree);
	return 0;
}

static int tfcp_tc_ta_get_temp2(struct tc_ta_classdev *ptc, u32 *degree)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u16 data;

	ret = tfcp_ta_i2c_read16(ri, TFCP_REG_TEMP, &data);
	if (ret < 0)
		return ret;
	*degree = (data & 0xff00) >> ACC_BYTE3;
	dev_info(ri->dev, "%s %d\n", __func__, *degree);
	return 0;
}

static int tfcp_tc_ta_set_output_control(struct tc_ta_classdev *ptc,u32 control)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u8 data = control & 0x01;

	dev_info(ri->dev, "%s 0x%02X\n", __func__, data);
	return tfcp_ta_i2c_write8_none_read_back(ri, TFCP_REG_OUTPUT_CTRL, data);
}

static int tfcp_tc_ta_set_mode(struct tc_ta_classdev *ptc, u32 mode)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u8 data = mode & 0xFF;

	dev_info(ri->dev, "%s 0x%02X\n", __func__, data);
	return tfcp_ta_i2c_write8(ri, TFCP_REG_MODE, data);
}

static int tfcp_tc_ta_set_voltage(struct tc_ta_classdev *ptc, u32 volt)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);

	dev_info(ri->dev, "%s %d\n", __func__, volt);
	return tfcp_ta_i2c_update32(ri, TFCP_REG_SETVC, volt << TFCP_VOLT_SHFT,
				   TFCP_VOLT_MASK);
}

static int tfcp_tc_ta_set_current(struct tc_ta_classdev *ptc, u32 curr)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);

	dev_info(ri->dev, "%s %d\n", __func__, curr);
	return tfcp_ta_i2c_update32(ri, TFCP_REG_SETVC, curr << TFCP_CURR_SHFT,
				   TFCP_CURR_MASK);
}

static int tfcp_tc_ta_set_voltage_current(struct tc_ta_classdev *ptc,u32 volt, u32 curr)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u32 reg = ((volt << TFCP_VOLT_SHFT) & TFCP_VOLT_MASK) |
		  ((curr << TFCP_CURR_SHFT) & TFCP_CURR_MASK);
	u8 _reg;

	dev_info(ri->dev, "%s %d %d\n", __func__, volt, curr);
	ret = tfcp_ta_i2c_write32(ri, TFCP_REG_SETVC, reg);
	tfcp_ta_i2c_read8(ri, TFCP_REG_MODE, &_reg);
	dev_info(ri->dev, "mode = 0x%02X\n", _reg);
	return ret;
}

static int tfcp_tc_ta_get_output_voltage_current(struct tc_ta_classdev *ptc,
						  u32 *volt, u32 *curr)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u32 data;

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_READVC, &data);
	if (ret < 0)
		return ret;
	*volt = (data & TFCP_VOLT_MASK) >> TFCP_VOLT_SHFT;
	*curr = (data & TFCP_CURR_MASK) >> TFCP_CURR_SHFT;
	dev_info(ri->dev, "%s %d %d\n", __func__, *volt, *curr);
	return 0;
}

static int tfcp_tc_ta_get_power_limit(struct tc_ta_classdev *ptc, u32 *power)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u16 data;

	ret = tfcp_ta_i2c_read16(ri, TFCP_REG_MAXPWR, &data);
	if (ret < 0){
		dev_info(ri->dev, "%s fail\n", __func__);
		return ret;
	}
	*power = data / 10;
	/* Adapter history problems 65 == 68 */
	if (*power == 65)
		*power = 68;
	dev_info(ri->dev, "%s %d\n", __func__, *power);
	return 0;
}

static int tfcp_tc_ta_authentication(struct tc_ta_classdev *ptc)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);

	dev_info(ri->dev, "%s\n", __func__);
	return 0;
}

static int tfcp_tc_ta_enable_wdt(struct tc_ta_classdev *ptc, bool en)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);

	dev_info(ri->dev, "%s %d\n", __func__, en);
	return (en ? tfcp_ta_i2c_set8 : tfcp_ta_i2c_clr8)
		(ri, TFCP_REG_MODE, TFCP_WDTEN_MASK);
}

static int tfcp_tc_ta_set_wdt(struct tc_ta_classdev *ptc, u32 ms)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);

	dev_info(ri->dev, "%s %d\n", __func__, ms);
	return tfcp_ta_i2c_write16(ri, TFCP_REG_WDT, ms);
}

static int tfcp_tc_ta_suspend(struct tc_ta_classdev *ptc)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);

	dev_info(ri->dev, "%s\n", __func__);
	return 0;
}

static int tfcp_tc_ta_resume(struct tc_ta_classdev *ptc)
{
	struct tfcp_info *ri = to_tfcp_info(ptc);

	dev_info(ri->dev, "%s\n", __func__);
	return 0;
}

static int tfcp_tc_ta_get_max_power_duration(struct tc_ta_classdev *ptc, u32 *time)
{
	int ret;
	u32 status, power;
	struct tfcp_info *ri = to_tfcp_info(ptc);

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_CHGSTAT, &status);
	if (ret < 0)
		return ret;
	dev_info(ri->dev, "%s 0x%04X\n", __func__, status);
	*time = (status & TFCP_MAX_POWER_DURATION_MASK) >> TFCP_MAX_POWER_DURATION_SHFT;
	if (*time != 0) {
		dev_info(ri->dev, "currently using an marked TA");
		goto out;
	}

	ret = tfcp_tc_ta_get_power_limit(ptc, &power);
	if (ret < 0)
		return ret;

	switch (power) {
		case TA_25W:
			*time = TA_25W_T;
			break;
		case TA_33W:
			*time = TA_33W_T;
			break;
		case TA_45W:
			*time = TA_45W_T;
			break;
		case TA_68W:
			*time = TA_68W_T;
			break;
		case TA_120W:
			*time = TA_120W_T;
			break;
		case TA_180W:
			*time = TA_180W_T;
			break;
		default:
			*time = TA_68W_T;
			break;
	}
out:
	*time = *time * 60;
	return 0;
}

static int tfcp_tc_ta_get_output_control_support(struct tc_ta_classdev *ptc,
					bool *support)
{
	int ret;
	struct tfcp_info *ri = to_tfcp_info(ptc);
	u32 status;

	ret = tfcp_ta_i2c_read32(ri, TFCP_REG_CHGSTAT, &status);
	if (ret < 0) {
		dev_err(ri->dev, "get output control support failed:%d\n", ret);
		return ret;
	}

	dev_info(ri->dev, "get chg stat:0x%x\n", status);

	*support = !!(status & TFCP_BLOCK_MOSFET_SUPPORT_MASK);

	return 0;
}

static const struct tc_ta_device_ops tfcp_tc_ta_ops = {
	.get_verinfo = tfcp_tc_ta_get_verinfo,
	.get_fwcode = tfcp_tc_ta_get_fwcode,
	.get_mfinfo = tfcp_tc_ta_get_mfinfo,
	.get_datecode = tfcp_tc_ta_get_datecode,
	.get_min_voltage = tfcp_tc_ta_get_min_voltage,
	.get_max_voltage = tfcp_tc_ta_get_max_voltage,
	.get_min_current = tfcp_tc_ta_get_min_current,
	.get_max_current = tfcp_tc_ta_get_max_current,
	.get_output_voltage = tfcp_tc_ta_get_output_voltage,
	.get_output_current = tfcp_tc_ta_get_output_current,
	.get_status = tfcp_tc_ta_get_status,
	.get_temp1 = tfcp_tc_ta_get_temp1,
	.get_temp2 = tfcp_tc_ta_get_temp2,
	.set_output_control = tfcp_tc_ta_set_output_control,
	.set_mode = tfcp_tc_ta_set_mode,
	.set_voltage = tfcp_tc_ta_set_voltage,
	.set_current = tfcp_tc_ta_set_current,
	.set_voltage_current = tfcp_tc_ta_set_voltage_current,
	.get_output_voltage_current = tfcp_tc_ta_get_output_voltage_current,
	.authentication = tfcp_tc_ta_authentication,
	.enable_wdt = tfcp_tc_ta_enable_wdt,
	.set_wdt = tfcp_tc_ta_set_wdt,
	.suspend = tfcp_tc_ta_suspend,
	.resume = tfcp_tc_ta_resume,
	.get_power_limit = tfcp_tc_ta_get_power_limit,
	.get_max_power_duration = tfcp_tc_ta_get_max_power_duration,
	.get_output_control_support = tfcp_tc_ta_get_output_control_support,
};

static const struct tc_ta_classdev tfcp_tc_ta = {
	.name = "tfcp_ta",
	.ops = &tfcp_tc_ta_ops,
};

static struct pinctrl *pinctrl;
struct pinctrl_state *pinctrl_i2c_data_gpio;
struct pinctrl_state *pinctrl_i2c_clk_gpio;

static void tran_i2c_driver_ability_init(struct device *dev)
{
	int ret = 0;
	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		pr_err("Cannot find ftm pinctrl!\n");
		return;
	}

	pinctrl_i2c_data_gpio = pinctrl_lookup_state(pinctrl, "tfcp_i2c_sda_mode");
	if (IS_ERR(pinctrl_i2c_data_gpio)) {
		ret = PTR_ERR(pinctrl_i2c_data_gpio);
		pr_err("Cannot find pinctrl pinctrl_i2c_data_gpio\n");
	} else {
		pinctrl_select_state(pinctrl, pinctrl_i2c_data_gpio);
	}

	pinctrl_i2c_clk_gpio = pinctrl_lookup_state(pinctrl, "tfcp_i2c_sclk_mode");
	if (IS_ERR(pinctrl_i2c_clk_gpio)) {
		ret = PTR_ERR(pinctrl_i2c_clk_gpio);
		pr_err("Cannot find pinctrl pinctrl_i2c_clk_gpio\n");
	} else {
		pinctrl_select_state(pinctrl, pinctrl_i2c_clk_gpio);
	}
}

static int tfcp_ta_i2c_probe(struct i2c_client *client)
{
	int ret;
	struct tfcp_info *ri;

	dev_info(&client->dev, "%s\n", __func__);
	ri = devm_kzalloc(&client->dev, sizeof(*ri), GFP_KERNEL);
	if (!ri)
		return -ENOMEM;
	ri->dev = &client->dev;
	ri->client = client;
	mutex_init(&ri->io_lock);
	mutex_init(&ri->suspend_lock);
	memcpy(&ri->ptc, &tfcp_tc_ta, sizeof(struct tc_ta_classdev));
	i2c_set_clientdata(client, ri);
	crc8_populate_msb(tfcp_crc_tbl, 0x7);

	tran_i2c_driver_ability_init(ri->dev);

	ret = devm_tc_ta_classdev_register(ri->dev, &ri->ptc);
	if (ret < 0) {
		dev_err(ri->dev, "%s add prop ta classdev fail(%d)\n", __func__,ret);
		return ret;
	}
	ri->tfcp_shutdown = 0;
	dev_info(ri->dev, "%s successfully\n", __func__);
	return 0;
}

static void tfcp_ta_i2c_remove(struct i2c_client *client)
{
	struct tfcp_info *ri = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s\n", __func__);
	if (ri) {
		mutex_destroy(&ri->io_lock);
		mutex_destroy(&ri->suspend_lock);
	}
}

static int __maybe_unused tfcp_ta_i2c_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct tfcp_info *ri = i2c_get_clientdata(i2c);

	dev_info(dev, "%s\n", __func__);
	mutex_lock(&ri->suspend_lock);
	return 0;
}

static int __maybe_unused tfcp_ta_i2c_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct tfcp_info *ri = i2c_get_clientdata(i2c);

	dev_info(dev, "%s\n", __func__);
	mutex_unlock(&ri->suspend_lock);
	return 0;
}

static SIMPLE_DEV_PM_OPS(tfcp_ta_pm_ops, tfcp_ta_i2c_suspend, tfcp_ta_i2c_resume);

static void tfcp_ta_i2c_shutdown(struct i2c_client *client)
{
	struct tfcp_info *ri = i2c_get_clientdata(client);
	dev_info(&client->dev, "%s\n", __func__);
	ri->tfcp_shutdown = 1;
}

static const struct of_device_id tfcp_ta_of_id[] = {
	{ .compatible = "tc,tfcp_ta", },
	{},
};
MODULE_DEVICE_TABLE(of, tfcp_ta_of_id);

static const struct i2c_device_id tfcp_ta_i2c_id[] = {
	{ "tfcp_ta", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, tfcp_ta_i2c_id);

struct i2c_driver tfcp_ta_i2c_driver = {
	.driver = {
		.name = "tfcp_ta",
		.owner = THIS_MODULE,
		.pm = &tfcp_ta_pm_ops,
		.of_match_table = of_match_ptr(tfcp_ta_of_id),
	},
	.probe = tfcp_ta_i2c_probe,
	.shutdown = tfcp_ta_i2c_shutdown,
	.remove = tfcp_ta_i2c_remove,
	.id_table = tfcp_ta_i2c_id,
};
module_i2c_driver(tfcp_ta_i2c_driver);

MODULE_DESCRIPTION("Transsion TFCP Fast Charge TA Driver");
MODULE_VERSION(TFCP_TA_DRV_VERSION);
MODULE_LICENSE("GPL");

/*
 * Revision
 * 1.0.1
 * (1) Check power supply online before doing i2c operation.
 *
 * 1.0.0
 * Initial release
 */
