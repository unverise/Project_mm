#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include "flashlight-core.h"
#include "flashlight-dt.h"
#include "ocp81378.h"

static int flash_on;

static const int back_channel_current[30] = {//const
	32, 97, 162, 227, 292, 357, 422, 487, 552, 617, 682, 
	747, 812, 877, 942, 1007, 1072, 1137, 1202, 1267, 1332, 
	1397, 1462, 1527, 1592, 1657, 1722, 1787, 1852, 1917
};

static const int front_channel_current[2] = {//const
#if defined(TRAN_X6880)
	230, 550
#else
	200, 550
#endif
};

static int ocp81378_flash_read(struct i2c_client *client, u8 reg)
{
	int ret;
	struct ocp81378_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed reading at 0x%02x\n", reg);

	return ret;
}

static int ocp81378_flash_write(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct ocp81378_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

static int ocp81378_set_current(int channel, int current)
{
	switch (channel) {
		case BACK_CHANNEL:
			if (current > OCP81378_TORCH_MAX_CURRENT) {
				ocp81378_device.channel[BACK_CHANNEL].state = OCP81378_FLASH;
				ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_BACK_FLASH_REGISTER, (0xff & OCP81378_FLASH_CURRENT_TO_REG(current)));
				ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_BACK_TORCH_REGISTER, 0);
			} else {
				ocp81378_device.channel[BACK_CHANNEL].state = OCP81378_TORCH;
				ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_BACK_TORCH_REGISTER, (0xff & OCP81378_TORCH_CURRENT_TO_REG(current)));
				ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_BACK_FLASH_REGISTER, 0);
			}
			break;
		case FRONT_CHANNEL:
			if (current > OCP81378_TORCH_MAX_CURRENT) {
				ocp81378_device.channel[FRONT_CHANNEL].state = OCP81378_FLASH;
				ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_FRONT_FLASH_REGISTER, (0xff & OCP81378_FLASH_CURRENT_TO_REG(current)));
				ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_FRONT_TORCH_REGISTER, 0);
			} else {
				ocp81378_device.channel[FRONT_CHANNEL].state = OCP81378_TORCH;
				ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_FRONT_TORCH_REGISTER, (0xff & OCP81378_TORCH_CURRENT_TO_REG(current)));
				ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_FRONT_FLASH_REGISTER, 0);
			}
			break;
		default:
			pr_info("Error channel");
	}

	mdelay(3);
	return 0;
}

static int ocp81378_enable(int channel)
{
	ktime_t ktime;
	unsigned int s, ns;
	int enableregval = 0;

	if (ocp81378_device.channel[channel].timeout_ms) {
		s = ocp81378_device.channel[channel].timeout_ms / 1000;
		ns = ocp81378_device.channel[channel].timeout_ms % 1000 * 1000000;
		ktime = ktime_set(s, ns);
		ocp81378_timer_start(channel, ktime);
	}

	switch (channel) {
		case BACK_CHANNEL:
			if (ocp81378_device.channel[BACK_CHANNEL].current > OCP81378_TORCH_MAX_CURRENT) {
				ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER, OCP81378_ENABLE_FLASH_MODE | OCP81378_ENABLE_BACK_LED);
			} else {
				enableregval = ocp81378_flash_read(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER) & 0x03;
				enableregval = enableregval | (OCP81378_ENABLE_TORCH_MODE | OCP81378_ENABLE_BACK_LED);
				ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER, enableregval);
			}
			break;
		case FRONT_CHANNEL:
			if (ocp81378_device.channel[FRONT_CHANNEL].current > OCP81378_TORCH_MAX_CURRENT) {
				ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER, OCP81378_ENABLE_FLASH_MODE | OCP81378_ENABLE_FRONT_LED);
			} else {
				enableregval = ocp81378_flash_read(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER) & 0x03;;
				enableregval = enableregval | (OCP81378_ENABLE_TORCH_MODE | OCP81378_ENABLE_FRONT_LED);
				ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER, enableregval);
			}
			break;
		default:
			pr_info("Error channel");
	}

	return 0;
}

static int ocp81378_disable(int channel)
{
	int enableregval = 0;
	enableregval = ocp81378_flash_read(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER);
	ocp81378_timer_cancel(channel);

	switch (channel) {
		case BACK_CHANNEL:
			ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER, enableregval & OCP81378_DISABLE_BACK_LED);
			break;
		case FRONT_CHANNEL:
			ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER, enableregval & OCP81378_DISABLE_FRONT_LED);
			break;
		default:
			pr_info("Error channel");
	}


	ocp81378_device.channel[channel].state = OCP81378_DISABLE;
	return 0;
}

static int ocp81378_set_scenario(int scenario)
{
	return 0;
}

static int ocp81378_get_hw_fault(void)
{
	int enableregval = 0;
	enableregval = ocp81378_flash_read(ocp81378_device.i2c_client, OCP81378_FLAGS1_REGISTER);
	PR_INFO(debug_mask,"enableregval 0x0A = %d\n",enableregval);
	enableregval += ocp81378_flash_read(ocp81378_device.i2c_client, OCP81378_FLAGS2_REGISTER);
	PR_INFO(debug_mask,"enableregval 0x0B = %d\n",enableregval);
	return enableregval;
}

static int ocp81378_init(void)
{
	int ret = 0;
	pr_debug("%s:%d start", __FUNCTION__, __LINE__);

	ocp81378_device.channel[BACK_CHANNEL].duty = 0;
	ocp81378_device.channel[FRONT_CHANNEL].duty = 0;

	ocp81378_device.channel[BACK_CHANNEL].state = OCP81378_DISABLE;
	ocp81378_device.channel[FRONT_CHANNEL].state = OCP81378_DISABLE;

	ocp81378_device.channel[BACK_CHANNEL].current = 0;
	ocp81378_device.channel[FRONT_CHANNEL].current = 0;

	if (ocp81378_device.regulator)
		ret = regulator_enable(ocp81378_device.regulator);
	mdelay(3);
	ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER, 0x00);
	ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_BOOST_CONFIG_REGISTER, 0x09);
	ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_TIMING_CONFIG_REGISTER, 0x1f);
	//LED1 torch&flash bit7 set to 0, defualt 1
	ocp81378_flash_write(ocp81378_device.i2c_client, 0x03, 0x00);
	ocp81378_flash_write(ocp81378_device.i2c_client, 0x05, 0x00);

	pr_debug("%s:%d end", __FUNCTION__, __LINE__);
	return 0;
}

static int ocp81378_uninit(void)
{
	PR_INFO(debug_mask,"%s:%d start", __FUNCTION__, __LINE__);

	ocp81378_device.channel[BACK_CHANNEL].duty = 0;
	ocp81378_device.channel[FRONT_CHANNEL].duty = 0;

	ocp81378_device.channel[BACK_CHANNEL].state = OCP81378_NONE;
	ocp81378_device.channel[FRONT_CHANNEL].state = OCP81378_NONE;

	ocp81378_device.channel[BACK_CHANNEL].current = 0;
	ocp81378_device.channel[FRONT_CHANNEL].current = 0;

	ocp81378_get_hw_fault();
	if (ocp81378_device.regulator)
		regulator_disable(ocp81378_device.regulator);

	PR_INFO(debug_mask,"%s:%d end", __FUNCTION__, __LINE__);
	return 0;
}

static void ocp81378_work_disable_ch1(struct work_struct *data)
{
	PR_INFO(debug_mask,"ht work queue callback\n");
	ocp81378_disable(BACK_CHANNEL);
}

static void ocp81378_work_disable_ch2(struct work_struct *data)
{
	PR_INFO(debug_mask,"lt work queue callback\n");
	ocp81378_disable(FRONT_CHANNEL);
}

static enum hrtimer_restart ocp81378_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&ocp81378_device.channel[BACK_CHANNEL].workqueue);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart ocp81378_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&ocp81378_device.channel[FRONT_CHANNEL].workqueue);
	return HRTIMER_NORESTART;
}

static int ocp81378_timer_start(int channel, ktime_t ktime)
{
	if (ocp81378_device.channel[channel].state != OCP81378_FLASH)
		return -1;

	if (channel < MAX_CHANNEL)
		hrtimer_start(&ocp81378_device.channel[channel].timer, ktime, HRTIMER_MODE_REL);
	else
		return -1;

	return 0;
}

static int ocp81378_timer_cancel(int channel)
{
	if (ocp81378_device.channel[channel].state != OCP81378_FLASH)
		return -1;

	if (channel < MAX_CHANNEL)
		hrtimer_cancel(&ocp81378_device.channel[channel].timer);
	else
		return -1;

	return 0;
}

static int ocp81378_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		PR_INFO(debug_mask,"FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n", channel, (int)fl_arg->arg);
		ocp81378_device.channel[channel].timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		PR_INFO(debug_mask,"FLASH_IOC_SET_DUTY(%d): %d\n", channel, (int)fl_arg->arg);
		if (fl_arg->arg < 0)
			fl_arg->arg = 0;
		ocp81378_device.channel[channel].current = ocp81378_device.channel[channel].current_pointer[fl_arg->arg];
		ocp81378_device.channel[channel].duty = fl_arg->arg;

		ocp81378_set_current(channel, ocp81378_device.channel[channel].current);
		break;

	case FLASH_IOC_SET_CURRENT:
		PR_INFO(debug_mask,"FLASH_IOC_SET_CURRENT(%d): %d\n", channel, (int)fl_arg->arg);
		if (fl_arg->arg > OCP81378_FLASH_MAX_CURRENT)
			fl_arg->arg = OCP81378_FLASH_MAX_CURRENT;
		ocp81378_device.channel[channel].current = fl_arg->arg;
		ocp81378_device.channel[channel].duty = 0;

		ocp81378_set_current(channel, ocp81378_device.channel[channel].current);
		break;

	case FLASH_IOC_GENERATE_DEV_PARA:
		PR_INFO(debug_mask,"FLASH_IOC_GENERATE_DEV_PARA(%d): %d\n", channel, (int)fl_arg->arg);
		break;

	case FLASH_IOC_SET_SCENARIO:
		PR_INFO(debug_mask,"FLASH_IOC_SET_SCENARIO(%d): %d\n", channel, (int)fl_arg->arg);
		ocp81378_set_scenario(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		PR_INFO(debug_mask,"FLASH_IOC_SET_ONOFF(%d): %d\n", channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1)
			ocp81378_enable(channel);
		else
			ocp81378_disable(channel);
		break;

	case FLASH_IOC_IS_CHARGER_READY:
		PR_INFO(debug_mask,"FLASH_IOC_SET_ONOFF(%d): %d\n", channel, (int)fl_arg->arg);
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		PR_INFO(debug_mask,"FLASH_IOC_GET_DUTY_NUMBER(%d)\n", ocp81378_device.channel[channel].current_number);
		fl_arg->arg = ocp81378_device.channel[channel].current_number;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		PR_INFO(debug_mask,"FLASH_IOC_GET_MAX_TORCH_DUTY\n");
		if (channel == BACK_CHANNEL)
			fl_arg->arg = OCP81378_BACK_FLASH_MIN_DUTY;
		else
			fl_arg->arg = OCP81378_FRONT_FLASH_MIN_DUTY;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		PR_INFO(debug_mask,"FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n", channel, fl_arg->arg);
		if (fl_arg->arg >= ocp81378_device.channel[channel].current_number)
			fl_arg->arg = ocp81378_device.channel[channel].current_number - 1;
		fl_arg->arg = ocp81378_device.channel[channel].current_pointer[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_FAULT:
		PR_INFO(debug_mask,"FLASH_IOC_GET_HW_FAULT(%d)\n", channel);
		fl_arg->arg = ocp81378_get_hw_fault();
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n", channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int ocp81378_open(void)
{
	return 0;
}

static int ocp81378_release(void)
{
	PR_INFO(debug_mask,"Release: %d\n", ocp81378_device.use_count);
	return 0;
}

static int ocp81378_set_driver(int set)
{
	int ret = 0;

	mutex_lock(&ocp81378_device.lock);
	if (set) {
		if (!ocp81378_device.use_count)
			ret = ocp81378_init();

		ocp81378_device.use_count++;
		PR_INFO(debug_mask,"Set driver: %d\n", ocp81378_device.use_count);
	} else {
		ocp81378_device.use_count--;
		if (!ocp81378_device.use_count)
			ret = ocp81378_uninit();

		if (ocp81378_device.use_count < 0)
			ocp81378_device.use_count = 0;
		PR_INFO(debug_mask,"Unset driver: %d\n", ocp81378_device.use_count);
	}
	mutex_unlock(&ocp81378_device.lock);

	return 0;
}

static ssize_t ocp81378_strobe_store(struct flashlight_arg arg)
{
	ocp81378_set_driver(1);
	ocp81378_device.channel[arg.channel].current = ocp81378_device.channel[arg.channel].current_pointer[arg.level];
	ocp81378_set_current(arg.channel, ocp81378_device.channel[arg.channel].current);
	ocp81378_enable(arg.channel);
	mdelay(arg.dur);
	ocp81378_disable(arg.channel);
	ocp81378_set_driver(0);
	ocp81378_release();

	return 0;
}

static struct flashlight_operations ocp81378_ops = {
	ocp81378_open,
	ocp81378_release,
	ocp81378_ioctl,
	ocp81378_strobe_store,
	ocp81378_set_driver
};

static int ocp81378_parse_dt(struct i2c_client *client)
{
	struct device_node *np, *cnp;
	struct device *dev = &client->dev;
	const char *name_ptr;
	int i = 0;
	struct flashlight_device_id flash_dev_id[3];

	if (!dev || !dev->of_node)
		return -ENODEV;
	np = dev->of_node;
	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type",
					&flash_dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp,
					"ct", &flash_dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp,
					"part", &flash_dev_id[i].part))
			goto err_node_put;

		of_property_read_string(cnp, "name", &name_ptr);
		strncpy(flash_dev_id[i].name, name_ptr, sizeof(flash_dev_id[i].name));

		if (of_property_read_u32(cnp,
					"ch", &flash_dev_id[i].channel))
			goto err_node_put;
		if (of_property_read_u32(cnp,
					"decouple", &flash_dev_id[i].decouple))
			goto err_node_put;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				flash_dev_id[i].type,
				flash_dev_id[i].ct,
				flash_dev_id[i].part,
				flash_dev_id[i].name,
				flash_dev_id[i].channel,
				flash_dev_id[i].decouple);
		if (flashlight_dev_register_by_device_id(&flash_dev_id[i],&ocp81378_ops))
			return -EFAULT;
		i++;
	}
	return 0;
err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}
static int ocp81378_i2c_probe(struct i2c_client *client)
{
	int err;
	u16 device_id = 0;

	struct ocp81378_chip_data *chip;
	struct ocp81378_platform_data *pdata = client->dev.platform_data;

	pr_debug("%s:%d start", __FUNCTION__, __LINE__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	chip = kzalloc(sizeof(struct ocp81378_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}

	client->addr = OCP81378_I2C_SLAVE_ADDR;
	chip->client = client;

	if (!pdata) {
		pr_err("Platform data does not exist\n");
		pdata = kzalloc(sizeof(struct ocp81378_platform_data), GFP_KERNEL);
	}

	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	ocp81378_device.i2c_client = client;

	mutex_init(&chip->lock);
	mutex_init(&ocp81378_device.lock);

	INIT_WORK(&ocp81378_device.channel[BACK_CHANNEL].workqueue, ocp81378_work_disable_ch1);
	INIT_WORK(&ocp81378_device.channel[FRONT_CHANNEL].workqueue, ocp81378_work_disable_ch2);

	hrtimer_init(&ocp81378_device.channel[BACK_CHANNEL].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ocp81378_device.channel[BACK_CHANNEL].timer.function = ocp81378_timer_func_ch1;
	hrtimer_init(&ocp81378_device.channel[FRONT_CHANNEL].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ocp81378_device.channel[FRONT_CHANNEL].timer.function = ocp81378_timer_func_ch2;
	ocp81378_device.channel[BACK_CHANNEL].timeout_ms = 100;
	ocp81378_device.channel[FRONT_CHANNEL].timeout_ms = 100;

	ocp81378_device.regulator = regulator_get(&client->dev, "ocp81378_regulator");
	if (IS_ERR(ocp81378_device.regulator)) {
		err = PTR_ERR(ocp81378_device.regulator);
		ocp81378_device.regulator = NULL;
		pr_err("ocp81378_device.regulator get fail!");
		goto err_free;
	}
	if (ocp81378_device.regulator)
		err = regulator_enable(ocp81378_device.regulator);
	mdelay(3);
	device_id = ocp81378_flash_read(ocp81378_device.i2c_client, OCP81378_DEVICE_ID_REGISTER);
	ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER, 0x00);
	if (ocp81378_device.regulator)
		regulator_disable(ocp81378_device.regulator);

	pr_info("ocp81378 read DEVICE_ID = 0x%x", device_id);

	if (device_id == 0x3a) {
		if (ocp81378_parse_dt(client)) {
			pr_err("ocp81378 flashlight_dev_register fail.");
			err = -EFAULT;
			goto err_free;
		}
		ocp81378_device.channel[BACK_CHANNEL].current_pointer = back_channel_current;
		ocp81378_device.channel[BACK_CHANNEL].current_number = sizeof(back_channel_current)/sizeof(int);
		ocp81378_device.channel[FRONT_CHANNEL].current_pointer = front_channel_current;
		ocp81378_device.channel[FRONT_CHANNEL].current_number = sizeof(front_channel_current)/sizeof(int);
	} else {
		pr_err("ocp81378 DEVICE_ID mismatch, skip.");
		err = -EFAULT;
		goto err_free;
	}

	ocp81378_device.use_count = 0;
	pr_debug("%s:%d end", __FUNCTION__, __LINE__);

	return 0;

err_free:
	kfree(chip->pdata);
	i2c_set_clientdata(client, NULL);
	kfree(chip);

err_out:
	return err;
}

static void ocp81378_i2c_remove(struct i2c_client *client)
{
	pr_debug("%s:%d start", __FUNCTION__, __LINE__);

	flush_work(&ocp81378_device.channel[BACK_CHANNEL].workqueue);
	flush_work(&ocp81378_device.channel[FRONT_CHANNEL].workqueue);


	pr_debug("%s:%d end", __FUNCTION__, __LINE__);
}

static void ocp81378_shutdown(struct i2c_client *client)
{
	pr_info("ocp81378_shutdown IN");
	if(ocp81378_device.i2c_client != NULL){
		pr_info("ocp81378_shutdown close torch");
		ocp81378_flash_write(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER, OCP81378_DISABLE_FRONT_LED & OCP81378_DISABLE_FRONT_LED);
	}
	pr_info("ocp81378_shutdown OUT");
}
static int __maybe_unused ocp81378_suspend(struct device *dev)
{
	unsigned int reg_val;

	reg_val = ocp81378_flash_read(ocp81378_device.i2c_client, OCP81378_ENABLE_REGISTER);
	pr_info("flash status:[0x%x]", reg_val);
	if ((reg_val&0xF) == 0) {
		flash_on = 0;
		return ocp81378_uninit();
	} else {
		flash_on = 1;
		pr_info("flash on, return\n");
		return 0;
	}
}
static int __maybe_unused ocp81378_resume(struct device *dev)
{

	if (flash_on == 0) {
		return ocp81378_init();
	} else {
		pr_info("flash on, return\n");
		return 0;
	}
}

static const struct i2c_device_id ocp81378_i2c_id[] = {
	{OCP81378_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ocp81378_i2c_id);

static const struct of_device_id ocp81378_i2c_of_match[] = {
	{.compatible = OCP81378_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, ocp81378_i2c_of_match);

static struct i2c_driver ocp81378_i2c_driver = {
	.driver = {
		   .name = OCP81378_NAME,
		   .of_match_table = ocp81378_i2c_of_match,
		   },
	.probe = ocp81378_i2c_probe,
	.remove = ocp81378_i2c_remove,
	.shutdown = ocp81378_shutdown,
	.id_table = ocp81378_i2c_id,
};

module_i2c_driver(ocp81378_i2c_driver);
MODULE_AUTHOR("Jie.Liu <jie.liu@transsion.com>");
MODULE_DESCRIPTION("Texas Instruments ocp81378 flash driver");
MODULE_LICENSE("GPL");