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
#include "cw8722.h"

static int flash_on;

static const int back_channel_current[30] = {//const
	32, 97, 162, 227, 292, 357, 422, 487, 552, 617, 682, 
	747, 812, 877, 942, 1007, 1072, 1137, 1202, 1267, 1332, 
	1397, 1462, 1527, 1592, 1657, 1722, 1787, 1852, 1917
};

static const int front_channel_current[2] = {//const
	220, 550
};

static int cw8722_flash_read(struct i2c_client *client, u8 reg)
{
	int ret;
	struct cw8722_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed reading at 0x%02x\n", reg);

	return ret;
}

static int cw8722_flash_write(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct cw8722_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

static int cw8722_set_current(int channel, int current)
{
	switch (channel) {
		case BACK_CHANNEL:
			if (current > CW8722_TORCH_MAX_CURRENT) {
				cw8722_device.channel[BACK_CHANNEL].state = CW8722_FLASH;
				cw8722_flash_write(cw8722_device.i2c_client, CW8722_BACK_FLASH_REGISTER, (0xff & CW8722_FLASH_CURRENT_TO_REG(current)));
				cw8722_flash_write(cw8722_device.i2c_client, CW8722_BACK_TORCH_REGISTER, 0);
			} else {
				cw8722_device.channel[BACK_CHANNEL].state = CW8722_TORCH;
				cw8722_flash_write(cw8722_device.i2c_client, CW8722_BACK_TORCH_REGISTER, (0xff & CW8722_TORCH_CURRENT_TO_REG(current)));
				cw8722_flash_write(cw8722_device.i2c_client, CW8722_BACK_FLASH_REGISTER, 0);
			}
			break;
		case FRONT_CHANNEL:
			if (current > CW8722_TORCH_MAX_CURRENT) {
				cw8722_device.channel[FRONT_CHANNEL].state = CW8722_FLASH;
				cw8722_flash_write(cw8722_device.i2c_client, CW8722_FRONT_FLASH_REGISTER, (0xff & CW8722_FLASH_CURRENT_TO_REG(current)));
				cw8722_flash_write(cw8722_device.i2c_client, CW8722_FRONT_TORCH_REGISTER,0);
			} else {
				cw8722_device.channel[FRONT_CHANNEL].state = CW8722_TORCH;
				cw8722_flash_write(cw8722_device.i2c_client, CW8722_FRONT_TORCH_REGISTER, (0xff & CW8722_TORCH_CURRENT_TO_REG(current)));
				cw8722_flash_write(cw8722_device.i2c_client, CW8722_FRONT_FLASH_REGISTER, 0);
			}
			break;
		default:
			pr_info("Error channel");
	}

	mdelay(3);
	return 0;
}

static int cw8722_enable(int channel)
{
	ktime_t ktime;
	unsigned int s, ns;
	int enableregval = 0;

	if (cw8722_device.channel[channel].timeout_ms) {
		s = cw8722_device.channel[channel].timeout_ms / 1000;
		ns = cw8722_device.channel[channel].timeout_ms % 1000 * 1000000;
		ktime = ktime_set(s, ns);
		cw8722_timer_start(channel, ktime);
	}

	switch (channel) {
		case BACK_CHANNEL:
			if (cw8722_device.channel[BACK_CHANNEL].current > CW8722_TORCH_MAX_CURRENT) {
				cw8722_flash_write(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER, CW8722_ENABLE_FLASH_MODE | CW8722_ENABLE_BACK_LED);
			} else {
				enableregval = cw8722_flash_read(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER) & 0x03;
				enableregval = enableregval | (CW8722_ENABLE_TORCH_MODE | CW8722_ENABLE_BACK_LED);
				cw8722_flash_write(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER, enableregval);
			}
			break;
		case FRONT_CHANNEL:
			if (cw8722_device.channel[FRONT_CHANNEL].current > CW8722_TORCH_MAX_CURRENT) {
				cw8722_flash_write(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER, CW8722_ENABLE_FLASH_MODE | CW8722_ENABLE_FRONT_LED);
			} else {
				enableregval = cw8722_flash_read(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER) & 0x03;
				enableregval = enableregval | (CW8722_ENABLE_TORCH_MODE | CW8722_ENABLE_FRONT_LED);
				cw8722_flash_write(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER, enableregval);
			}
			break;
		default:
			pr_info("Error channel");
	}

	return 0;
}

static int cw8722_disable(int channel)
{
	int enableregval = 0;
	enableregval = cw8722_flash_read(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER);
	cw8722_timer_cancel(channel);

	switch (channel) {
		case BACK_CHANNEL:
			cw8722_flash_write(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER, enableregval & CW8722_DISABLE_BACK_LED);
			break;
		case FRONT_CHANNEL:
			cw8722_flash_write(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER, enableregval & CW8722_DISABLE_FRONT_LED);
			break;
		default:
			pr_info("Error channel");
	}

	cw8722_device.channel[channel].state = CW8722_DISABLE;
	return 0;
}

static int cw8722_set_scenario(int scenario)
{
	return 0;
}

static int cw8722_get_hw_fault(void)
{
	int enableregval = 0;
	enableregval = cw8722_flash_read(cw8722_device.i2c_client, CW8722_FLAGS1_REGISTER);
	PR_INFO(debug_mask,"enableregval 0x0A = %d\n",enableregval);
	enableregval += cw8722_flash_read(cw8722_device.i2c_client, CW8722_FLAGS2_REGISTER);
	PR_INFO(debug_mask,"enableregval 0x0B = %d\n",enableregval);
	return enableregval;
}

static int cw8722_init(void)
{
	int ret = 0;
	pr_debug("%s:%d start", __FUNCTION__, __LINE__);

	cw8722_device.channel[BACK_CHANNEL].duty = 0;
	cw8722_device.channel[FRONT_CHANNEL].duty = 0;

	cw8722_device.channel[BACK_CHANNEL].state = CW8722_DISABLE;
	cw8722_device.channel[FRONT_CHANNEL].state = CW8722_DISABLE;

	cw8722_device.channel[BACK_CHANNEL].current = 0;
	cw8722_device.channel[FRONT_CHANNEL].current = 0;

	if (cw8722_device.regulator)
		ret = regulator_enable(cw8722_device.regulator);
	mdelay(3);
	cw8722_flash_write(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER, 0x00);
	cw8722_flash_write(cw8722_device.i2c_client, CW8722_BOOST_CONFIG_REGISTER, 0x09);
	cw8722_flash_write(cw8722_device.i2c_client, CW8722_TIMING_CONFIG_REGISTER, 0x1f);
	cw8722_flash_write(cw8722_device.i2c_client, 0x03, 0x00);
	cw8722_flash_write(cw8722_device.i2c_client, 0x05, 0x00);

	pr_debug("%s:%d end", __FUNCTION__, __LINE__);
	return 0;
}

static int cw8722_uninit(void)
{
	PR_INFO(debug_mask,"%s:%d start", __FUNCTION__, __LINE__);

	cw8722_device.channel[BACK_CHANNEL].duty = 0;
	cw8722_device.channel[FRONT_CHANNEL].duty = 0;

	cw8722_device.channel[BACK_CHANNEL].state = CW8722_NONE;
	cw8722_device.channel[FRONT_CHANNEL].state = CW8722_NONE;

	cw8722_device.channel[BACK_CHANNEL].current = 0;
	cw8722_device.channel[FRONT_CHANNEL].current = 0;

	cw8722_get_hw_fault();
	if(cw8722_device.i2c_client != NULL){
		pr_info("cw8722_shutdown close torch");
		cw8722_flash_write(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER, 0x00);
	}
	if (cw8722_device.regulator)
		regulator_disable(cw8722_device.regulator);

	PR_INFO(debug_mask,"%s:%d end", __FUNCTION__, __LINE__);
	return 0;
}

static void cw8722_work_disable_ch1(struct work_struct *data)
{
	PR_INFO(debug_mask,"ht work queue callback\n");
	cw8722_disable(BACK_CHANNEL);
}

static void cw8722_work_disable_ch2(struct work_struct *data)
{
	PR_INFO(debug_mask,"lt work queue callback\n");
	cw8722_disable(FRONT_CHANNEL);
}

static enum hrtimer_restart cw8722_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&cw8722_device.channel[BACK_CHANNEL].workqueue);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart cw8722_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&cw8722_device.channel[FRONT_CHANNEL].workqueue);
	return HRTIMER_NORESTART;
}

static int cw8722_timer_start(int channel, ktime_t ktime)
{
	if (cw8722_device.channel[channel].state != CW8722_FLASH)
		return -1;

	if (channel < MAX_CHANNEL)
		hrtimer_start(&cw8722_device.channel[channel].timer, ktime, HRTIMER_MODE_REL);
	else
		return -1;

	return 0;
}

static int cw8722_timer_cancel(int channel)
{
	if (cw8722_device.channel[channel].state != CW8722_FLASH)
		return -1;

	if (channel < MAX_CHANNEL)
		hrtimer_cancel(&cw8722_device.channel[channel].timer);
	else
		return -1;

	return 0;
}

static int cw8722_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		PR_INFO(debug_mask,"FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n", channel, (int)fl_arg->arg);
		cw8722_device.channel[channel].timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		PR_INFO(debug_mask,"FLASH_IOC_SET_DUTY(%d): %d\n", channel, (int)fl_arg->arg);
		if (fl_arg->arg < 0)
			fl_arg->arg = 0;
		cw8722_device.channel[channel].current = cw8722_device.channel[channel].current_pointer[fl_arg->arg];
		cw8722_device.channel[channel].duty = fl_arg->arg;

		cw8722_set_current(channel, cw8722_device.channel[channel].current);
		break;

	case FLASH_IOC_SET_CURRENT:
		PR_INFO(debug_mask,"FLASH_IOC_SET_CURRENT(%d): %d\n", channel, (int)fl_arg->arg);
		if (fl_arg->arg > CW8722_FLASH_MAX_CURRENT)
			fl_arg->arg = CW8722_FLASH_MAX_CURRENT;
		cw8722_device.channel[channel].current = fl_arg->arg;
		cw8722_device.channel[channel].duty = 0;

		cw8722_set_current(channel, cw8722_device.channel[channel].current);
		break;

	case FLASH_IOC_GENERATE_DEV_PARA:
		PR_INFO(debug_mask,"FLASH_IOC_GENERATE_DEV_PARA(%d): %d\n", channel, (int)fl_arg->arg);
		break;

	case FLASH_IOC_SET_SCENARIO:
		PR_INFO(debug_mask,"FLASH_IOC_SET_SCENARIO(%d): %d\n", channel, (int)fl_arg->arg);
		cw8722_set_scenario(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		PR_INFO(debug_mask,"FLASH_IOC_SET_ONOFF(%d): %d\n", channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1)
			cw8722_enable(channel);
		else
			cw8722_disable(channel);
		break;

	case FLASH_IOC_IS_CHARGER_READY:
		PR_INFO(debug_mask,"FLASH_IOC_SET_ONOFF(%d): %d\n", channel, (int)fl_arg->arg);
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		PR_INFO(debug_mask,"FLASH_IOC_GET_DUTY_NUMBER(%d)\n", cw8722_device.channel[channel].current_number);
		fl_arg->arg = cw8722_device.channel[channel].current_number;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		PR_INFO(debug_mask,"FLASH_IOC_GET_MAX_TORCH_DUTY\n");
		if (channel == BACK_CHANNEL)
			fl_arg->arg = CW8722_BACK_FLASH_MIN_DUTY;
		else
			fl_arg->arg = CW8722_FRONT_FLASH_MIN_DUTY;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		PR_INFO(debug_mask,"FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n", channel, fl_arg->arg);
		if (fl_arg->arg >= cw8722_device.channel[channel].current_number)
			fl_arg->arg = cw8722_device.channel[channel].current_number - 1;
		fl_arg->arg = cw8722_device.channel[channel].current_pointer[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_FAULT:
		PR_INFO(debug_mask,"FLASH_IOC_GET_HW_FAULT(%d)\n", channel);
		fl_arg->arg = cw8722_get_hw_fault();
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n", channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int cw8722_open(void)
{
	return 0;
}

static int cw8722_release(void)
{
	PR_INFO(debug_mask,"Release: %d\n", cw8722_device.use_count);
	return 0;
}

static int cw8722_set_driver(int set)
{
	int ret = 0;

	mutex_lock(&cw8722_device.lock);
	if (set) {
		if (!cw8722_device.use_count)
			ret = cw8722_init();

		cw8722_device.use_count++;
		PR_INFO(debug_mask,"Set driver: %d\n", cw8722_device.use_count);
	} else {
		cw8722_device.use_count--;
		if (!cw8722_device.use_count)
			ret = cw8722_uninit();

		if (cw8722_device.use_count < 0)
			cw8722_device.use_count = 0;
		PR_INFO(debug_mask,"Unset driver: %d\n", cw8722_device.use_count);
	}
	mutex_unlock(&cw8722_device.lock);

	return 0;
}

static ssize_t cw8722_strobe_store(struct flashlight_arg arg)
{
	cw8722_set_driver(1);
	cw8722_device.channel[arg.channel].current = cw8722_device.channel[arg.channel].current_pointer[arg.level];
	cw8722_set_current(arg.channel, cw8722_device.channel[arg.channel].current);
	cw8722_enable(arg.channel);
	mdelay(arg.dur);
	cw8722_disable(arg.channel);
	cw8722_set_driver(0);
	cw8722_release();

	return 0;
}

static struct flashlight_operations cw8722_ops = {
	cw8722_open,
	cw8722_release,
	cw8722_ioctl,
	cw8722_strobe_store,
	cw8722_set_driver
};

static int cw8722_parse_dt(struct i2c_client *client)
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
		if (flashlight_dev_register_by_device_id(&flash_dev_id[i],&cw8722_ops))
			return -EFAULT;
		i++;
	}
	return 0;
err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int cw8722_i2c_probe(struct i2c_client *client)
{
	int err;
	u16 device_id = 0;

	struct cw8722_chip_data *chip;
	struct cw8722_platform_data *pdata = client->dev.platform_data;

	pr_debug("%s:%d start", __FUNCTION__, __LINE__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	chip = kzalloc(sizeof(struct cw8722_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}

	client->addr = CW8722_I2C_SLAVE_ADDR;
	chip->client = client;

	if (!pdata) {
		pr_err("Platform data does not exist\n");
		pdata = kzalloc(sizeof(struct cw8722_platform_data), GFP_KERNEL);
	}

	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	cw8722_device.i2c_client = client;

	mutex_init(&chip->lock);
	mutex_init(&cw8722_device.lock);

	INIT_WORK(&cw8722_device.channel[BACK_CHANNEL].workqueue, cw8722_work_disable_ch1);
	INIT_WORK(&cw8722_device.channel[FRONT_CHANNEL].workqueue, cw8722_work_disable_ch2);

	hrtimer_init(&cw8722_device.channel[BACK_CHANNEL].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cw8722_device.channel[BACK_CHANNEL].timer.function = cw8722_timer_func_ch1;
	hrtimer_init(&cw8722_device.channel[FRONT_CHANNEL].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cw8722_device.channel[FRONT_CHANNEL].timer.function = cw8722_timer_func_ch2;
	cw8722_device.channel[BACK_CHANNEL].timeout_ms = 100;
	cw8722_device.channel[FRONT_CHANNEL].timeout_ms = 100;

	cw8722_device.regulator = regulator_get(&client->dev, "cw8722_regulator");
	if (IS_ERR(cw8722_device.regulator)) {
		err = PTR_ERR(cw8722_device.regulator);
		cw8722_device.regulator = NULL;
		pr_err("cw8722_device.regulator get fail!");
		goto err_free;
	}

	if (cw8722_device.regulator)
		err = regulator_enable(cw8722_device.regulator);
	mdelay(3);
	device_id = cw8722_flash_read(cw8722_device.i2c_client, CW8722_CHIP_ID_REGISTER);
	cw8722_flash_write(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER, 0x00);
	if (cw8722_device.regulator)
		regulator_disable(cw8722_device.regulator);

	pr_info("cw8722 read DEVICE_ID = 0x%x", device_id);

	if (device_id == 0x22) {
		if (cw8722_parse_dt(client)) {
			pr_err("cw8722 flashlight_dev_register fail.");
			err = -EFAULT;
			goto err_free;
		}
		cw8722_device.channel[BACK_CHANNEL].current_pointer = back_channel_current;
		cw8722_device.channel[BACK_CHANNEL].current_number = sizeof(back_channel_current)/sizeof(int);
		cw8722_device.channel[FRONT_CHANNEL].current_pointer = front_channel_current;
		cw8722_device.channel[FRONT_CHANNEL].current_number = sizeof(front_channel_current)/sizeof(int);
	} else {
		pr_err("cw8722 DEVICE_ID mismatch, skip.");
		err = -EFAULT;
		goto err_free;
	}

	cw8722_device.use_count = 0;
	pr_debug("%s:%d end", __FUNCTION__, __LINE__);

	return 0;

err_free:
	kfree(chip->pdata);
	i2c_set_clientdata(client, NULL);
	kfree(chip);

err_out:
	return err;
}

static void cw8722_i2c_remove(struct i2c_client *client)
{
	pr_debug("%s:%d start", __FUNCTION__, __LINE__);

	flush_work(&cw8722_device.channel[BACK_CHANNEL].workqueue);
	flush_work(&cw8722_device.channel[FRONT_CHANNEL].workqueue);


	pr_debug("%s:%d end", __FUNCTION__, __LINE__);
}

static void cw8722_shutdown(struct i2c_client *client)
{
	pr_info("cw8722_shutdown IN");
	if(cw8722_device.i2c_client != NULL){
		pr_info("cw8722_shutdown close torch");
		cw8722_flash_write(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER, 0x00);
	}
	pr_info("cw8722_shutdown OUT");
}
static int __maybe_unused cw8722_suspend(struct device *dev)
{
	unsigned int reg_val;

	reg_val = cw8722_flash_read(cw8722_device.i2c_client, CW8722_ENABLE_REGISTER);
	pr_info("flash status:[0x%x]", reg_val);
	if ((reg_val&0xF) == 0) {
		flash_on = 0;
		return cw8722_uninit();
	} else {
		flash_on = 1;
		pr_info("flash on, return\n");
		return 0;
	}
}
static int __maybe_unused cw8722_resume(struct device *dev)
{

	if (flash_on == 0) {
		return cw8722_init();
	} else {
		pr_info("flash on, return\n");
		return 0;
	}
}

static const struct i2c_device_id cw8722_i2c_id[] = {
	{CW8722_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, cw8722_i2c_id);

static const struct of_device_id cw8722_i2c_of_match[] = {
	{.compatible = CW8722_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, cw8722_i2c_of_match);

static struct i2c_driver cw8722_i2c_driver = {
	.driver = {
		   .name = CW8722_NAME,
		   .of_match_table = cw8722_i2c_of_match,
		   },
	.probe = cw8722_i2c_probe,
	.remove = cw8722_i2c_remove,
	.shutdown = cw8722_shutdown,
	.id_table = cw8722_i2c_id,
};

module_i2c_driver(cw8722_i2c_driver);
MODULE_AUTHOR("Jie.Liu <jie.liu@transsion.com>");
MODULE_DESCRIPTION("Texas Instruments cw8722 flash driver");
MODULE_LICENSE("GPL");