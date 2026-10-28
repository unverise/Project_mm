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
#include "tran_flashlight.h"
#include "tran_flash_custom.h"

#ifdef pr_debug
#undef pr_debug
#define pr_debug pr_info
#endif
static int channel_switch = 0;
static int device_id = 0;
static int chip_id = 0;
static int flash_type = 0;// 1 CONFIG_TRAN_FLASH_BRATHE_LED_COMMMON 2 : fornt double color led 3:back double color led 4 photonic
int TRAN_ENABLE_BACK_WARM_LED = 0;
int TRAN_DISABLE_BACK_WARM_LED = 0;
int TRAN_BACK_WARM_TORCH_REGISTER = 0;
int TRAN_ENABLE_FLASH_MODE = 0;
int TRAN_ENABLE_TORCH_MODE = 0;
int TRAN_ENABLE_BACK_LED = 0;
int TRAN_ENABLE_FRONT_LED = 0;
int TRAN_DISABLE_BACK_LED = 0;
int TRAN_DISABLE_FRONT_LED = 0;
static const int back_channel_current[23] = {//const
	32, 120, 162, 227, 292, 357, 422, 487, 552, 617, 682,
	747, 812, 877, 942, 1007, 1072, 1137, 1202, 1267, 1332,
	1397, 1462
};

static const int back_photonic_channel_current[30] = {//const
	32, 60, 97, 125, 162, 193, 227, 255, 292, 321,
	357, 381, 422, 450, 487, 520, 552, 585, 617, 652,
	682, 712, 747, 780, 812, 844, 877, 909, 942, 970
};

static const int front_channel_current[2] = {//const
	250, 550
};

static const int front_cold_channel_current[29] = {//const
	190,410,10,15,20,25,30,35,50,55,60,70,75,90,95,100,105,120,125,130,135,140,150,155,170,190,200,210,230
};

static const int back_warm_channel_current[29] = {//const
	15,30,5,10,15,20,25,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30
};

static const int front_warm_channel_current[29] = {//const
	60,140,10,15,20,25,30,35,50,55,60,70,75,90,95,100,105,120,125,130,135,140,150,155,170,190,200,210,230
};

static int tran_flash_read(struct i2c_client *client, u8 reg)
{
	int ret;
	struct tran_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed reading at 0x%02x\n", reg);

	return ret;
}

static int tran_flash_write(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct tran_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

static int tran_set_current(int channel, int current)
{
	pr_info("tran_set_current channel %d current %d\n",channel,current);
	switch (channel) {
		case BACK_CHANNEL:
			if (current > TRAN_TORCH_MAX_CURRENT) {
				tran_device.channel[BACK_CHANNEL].state = TRAN_FLASH;
				tran_flash_write(tran_device.i2c_client, TRAN_BACK_TORCH_REGISTER, 0);
				if((device_id == 0x02)||(chip_id == 0x22)||(device_id == 0x11))
					tran_flash_write(tran_device.i2c_client, TRAN_BACK_FLASH_REGISTER, (0xff & TRAN_AW36515_FLASH_CURRENT_TO_REG(current)));
				else
					tran_flash_write(tran_device.i2c_client, TRAN_BACK_FLASH_REGISTER, (0xff & TRAN_OCP81375_FLASH_CURRENT_TO_REG(current)));
			} else {
				tran_device.channel[BACK_CHANNEL].state = TRAN_TORCH;
				tran_flash_write(tran_device.i2c_client, TRAN_BACK_FLASH_REGISTER, 0);
				if((device_id == 0x02)||(chip_id == 0x22))
					tran_flash_write(tran_device.i2c_client, TRAN_BACK_TORCH_REGISTER, (0xff & TRAN_AW36515_TORCH_CURRENT_TO_REG(current)));
				else
					tran_flash_write(tran_device.i2c_client, TRAN_BACK_TORCH_REGISTER, (0xff & TRAN_OCP81375_TORCH_CURRENT_TO_REG(current)));
			}
			break;
		case FRONT_CHANNEL:
			if(flash_type == 1)
			{
				if (tran_device.channel[channel].regulator)
					(void)regulator_enable(tran_device.channel[channel].regulator);
				if (regulator_is_enabled(tran_device.channel[BACK_WARM_CHANNEL].regulator))
				{
					pr_info("%s BACK_WARM_CHANNEL close front 4444",__func__);
					regulator_disable(tran_device.channel[BACK_WARM_CHANNEL].regulator);
				}
			}
			if (current > TRAN_TORCH_MAX_CURRENT) {
				tran_device.channel[FRONT_CHANNEL].state = TRAN_FLASH;
				tran_flash_write(tran_device.i2c_client, TRAN_FRONT_TORCH_REGISTER, 0);
				if((device_id == 0x02)||(chip_id == 0x22)||(device_id == 0x11))
					tran_flash_write(tran_device.i2c_client, TRAN_FRONT_FLASH_REGISTER, (0xff & TRAN_AW36515_FLASH_CURRENT_TO_REG(current)));
				else
					tran_flash_write(tran_device.i2c_client, TRAN_FRONT_FLASH_REGISTER, (0xff & TRAN_OCP81375_FLASH_CURRENT_TO_REG(current)));
			} else {
				tran_device.channel[FRONT_CHANNEL].state = TRAN_TORCH;
				tran_flash_write(tran_device.i2c_client, TRAN_FRONT_FLASH_REGISTER, 0);
				if((device_id == 0x02)||(chip_id == 0x22)||(device_id == 0x11))
					tran_flash_write(tran_device.i2c_client, TRAN_FRONT_TORCH_REGISTER, (0xff & TRAN_AW36515_TORCH_CURRENT_TO_REG(current)));
				else
					tran_flash_write(tran_device.i2c_client, TRAN_FRONT_TORCH_REGISTER, (0xff & TRAN_OCP81375_TORCH_CURRENT_TO_REG(current)));
			}
			break;
		case BACK_WARM_CHANNEL:
			if (current > TRAN_TORCH_MAX_CURRENT) {
				tran_device.channel[BACK_WARM_CHANNEL].state = TRAN_FLASH;
				tran_flash_write(tran_device.i2c_client, TRAN_BACK_WARM_TORCH_REGISTER, 0);
				if((device_id == 0x02)||(chip_id == 0x22)||(device_id == 0x11))
					tran_flash_write(tran_device.i2c_client, TRAN_BACK_WARM_FLASH_REGISTER, (0xff & TRAN_AW36515_FLASH_CURRENT_TO_REG(current)));
				else
					tran_flash_write(tran_device.i2c_client, TRAN_BACK_WARM_FLASH_REGISTER, (0xff & TRAN_OCP81375_FLASH_CURRENT_TO_REG(current)));
			} else {
				if(flash_type == 1)
				{
					if (tran_device.channel[BACK_CHANNEL].state == TRAN_TORCH || tran_device.channel[FRONT_CHANNEL].state == TRAN_TORCH)
					{
						pr_info("%s BACK_WARM_CHANNEL close",__func__);
						return -1;
					}
				}
				tran_device.channel[BACK_WARM_CHANNEL].state = TRAN_TORCH;
				tran_flash_write(tran_device.i2c_client, TRAN_BACK_WARM_FLASH_REGISTER, 0);
				if((device_id == 0x02)||(chip_id == 0x22)||(device_id == 0x11))
					tran_flash_write(tran_device.i2c_client, TRAN_BACK_WARM_TORCH_REGISTER, (0xff & TRAN_AW36515_TORCH_CURRENT_TO_REG(current)));
				else
					tran_flash_write(tran_device.i2c_client, TRAN_BACK_WARM_TORCH_REGISTER, (0xff & TRAN_OCP81375_TORCH_CURRENT_TO_REG(current)));
			}
			break;
		case FRONT_WARM_CHANNEL:
			if (current > TRAN_TORCH_MAX_CURRENT) {
				tran_device.channel[FRONT_WARM_CHANNEL].state = TRAN_FLASH;
				tran_flash_write(tran_device.i2c_client, TRAN_FRONT_WARM_TORCH_REGISTER, 0);
				if((device_id == 0x02)||(chip_id == 0x22)||(device_id == 0x11))
					tran_flash_write(tran_device.i2c_client, TRAN_FRONT_WARM_FLASH_REGISTER, (0xff & TRAN_AW36515_FLASH_CURRENT_TO_REG(current)));
				else
					tran_flash_write(tran_device.i2c_client, TRAN_FRONT_WARM_FLASH_REGISTER, (0xff & TRAN_OCP81375_FLASH_CURRENT_TO_REG(current)));
			} else {
				tran_device.channel[FRONT_WARM_CHANNEL].state = TRAN_TORCH;
				tran_flash_write(tran_device.i2c_client, TRAN_FRONT_WARM_FLASH_REGISTER, 0);
				if((device_id == 0x02)||(chip_id == 0x22)||(device_id == 0x11))
					tran_flash_write(tran_device.i2c_client, TRAN_FRONT_WARM_TORCH_REGISTER, (0xff & TRAN_AW36515_TORCH_CURRENT_TO_REG(current)));
				else
					tran_flash_write(tran_device.i2c_client, TRAN_FRONT_WARM_TORCH_REGISTER, (0xff & TRAN_OCP81375_TORCH_CURRENT_TO_REG(current)));
			}
			break;
		default:
			pr_info("Error channel");
	}

	mdelay(3);
	return 0;
}

static int tran_enable(int channel)
{
	ktime_t ktime;
	unsigned int s, ns;
	int enableregval = 0;
	unsigned char temp = 0x03;

	if (tran_device.channel[channel].timeout_ms) {
		s = tran_device.channel[channel].timeout_ms / 1000;
		ns = tran_device.channel[channel].timeout_ms % 1000 * 1000000;
		ktime = ktime_set(s, ns);
		tran_timer_start(channel, ktime);
		//mdelay(tran_device.channel[channel].timeout_ms);
	}

	pr_info("tran_enable  %d timeout_ms %d xxx",channel,tran_device.channel[channel].timeout_ms);
	if (tran_device.channel[channel].regulator)
		(void)regulator_enable(tran_device.channel[channel].regulator);
	switch (channel) {
		case BACK_CHANNEL:
			if((flash_type == 1)&& (TRAN_ENABLE_BACK_WARM_LED == TRAN_ENABLE_FRONT_LED))//TRAN_ENABLE_BACK_WARM_LED == TRAN_ENABLE_FRONT_LED  --->  frontlight and backwarm user common Hardware LED1
			{
					if (regulator_is_enabled(tran_device.channel[BACK_WARM_CHANNEL].regulator))
					{
						pr_info("%s BACK_WARM_CHANNEL close",__func__);
						regulator_disable(tran_device.channel[BACK_WARM_CHANNEL].regulator);
						temp = TRAN_DISABLE_BACK_WARM_LED;
					}else if(tran_device.channel[FRONT_CHANNEL].state == TRAN_DISABLE){
						temp = TRAN_DISABLE_BACK_WARM_LED;
						pr_info("%s FRONT_CHANNEL close",__func__);
					}
					tran_get_hw_fault();
			}
			if((flash_type == 1) && (TRAN_ENABLE_BACK_WARM_LED == TRAN_ENABLE_BACK_LED))//TRAN_ENABLE_BACK_WARM_LED == TRAN_ENABLE_BACK_LED  --->  backlight and backwarm user common Hardware LED2
			{
				if (regulator_is_enabled(tran_device.channel[BACK_WARM_CHANNEL].regulator))
				{
					pr_info("%s BACK_WARM_CHANNEL close",__func__);
					regulator_disable(tran_device.channel[BACK_WARM_CHANNEL].regulator);
				}
			}
			if (tran_device.channel[BACK_CHANNEL].current > TRAN_TORCH_MAX_CURRENT) {
				tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, TRAN_ENABLE_FLASH_MODE | TRAN_ENABLE_BACK_LED);
			} else {
				enableregval = tran_flash_read(tran_device.i2c_client, TRAN_ENABLE_REGISTER) & 0x03 & temp;
				enableregval = enableregval | (TRAN_ENABLE_TORCH_MODE | TRAN_ENABLE_BACK_LED);
				tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, enableregval);
			}
			break;
		case FRONT_CHANNEL:
			if((flash_type == 1) && (TRAN_ENABLE_BACK_WARM_LED == TRAN_ENABLE_FRONT_LED))
			{
				if (regulator_is_enabled(tran_device.channel[BACK_WARM_CHANNEL].regulator))
				{
					pr_info("%s BACK_WARM_CHANNEL close front 333",__func__);
					regulator_disable(tran_device.channel[BACK_WARM_CHANNEL].regulator);
					temp = TRAN_DISABLE_BACK_WARM_LED;
				}
				tran_get_hw_fault();
			}
			if (tran_device.channel[FRONT_CHANNEL].current > TRAN_TORCH_MAX_CURRENT) {
				tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, TRAN_ENABLE_FLASH_MODE | TRAN_ENABLE_FRONT_LED);
			} else {
				enableregval = tran_flash_read(tran_device.i2c_client, TRAN_ENABLE_REGISTER) & 0x03 & temp;
				enableregval = enableregval | (TRAN_ENABLE_TORCH_MODE | TRAN_ENABLE_FRONT_LED);
				if((flash_type == 1) && (TRAN_ENABLE_BACK_WARM_LED == TRAN_ENABLE_BACK_LED)) //TRAN_ENABLE_BACK_WARM_LED == TRAN_ENABLE_BACK_LED  --->  backlight and backwarm user common Hardware LED2
				{
					if (regulator_is_enabled(tran_device.channel[BACK_WARM_CHANNEL].regulator))
					{
						tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, enableregval&TRAN_DISABLE_BACK_LED);
					}
					else
						tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, enableregval);
				}
				else
					tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, enableregval);
			}
			break;
		case BACK_WARM_CHANNEL:
			if (tran_device.channel[BACK_WARM_CHANNEL].current > TRAN_TORCH_MAX_CURRENT) {
				tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, TRAN_ENABLE_FLASH_MODE | TRAN_ENABLE_BACK_WARM_LED);
			} else {
				enableregval = tran_flash_read(tran_device.i2c_client, TRAN_ENABLE_REGISTER) & 0x03;
				enableregval = enableregval | (TRAN_ENABLE_TORCH_MODE | TRAN_ENABLE_BACK_WARM_LED);
				tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, enableregval);
			}
			break;
		case FRONT_WARM_CHANNEL:
			if (tran_device.channel[FRONT_WARM_CHANNEL].current > TRAN_TORCH_MAX_CURRENT) {
				tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, TRAN_ENABLE_FLASH_MODE | TRAN_ENABLE_FRONT_WARM_LED);
			} else {
				enableregval = tran_flash_read(tran_device.i2c_client, TRAN_ENABLE_REGISTER) & 0x03;
				enableregval = enableregval | (TRAN_ENABLE_TORCH_MODE | TRAN_ENABLE_FRONT_WARM_LED);
				tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, enableregval);
			}
			break;
		default:
			pr_info("Error channel");
	}
	if(tran_device.regulator_control!=NULL)
		(void)regulator_enable(tran_device.regulator_control);
#if 1
	pr_info("%s TRAN_ENABLE_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_ENABLE_REGISTER));
	pr_info("%s TRAN_BACK_FLASH_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_BACK_FLASH_REGISTER));
	pr_info("%s TRAN_BACK_TORCH_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_BACK_TORCH_REGISTER));
	pr_info("%s TRAN_FRONT_FLASH_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_FRONT_FLASH_REGISTER));
	pr_info("%s TRAN_FRONT_TORCH_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_FRONT_TORCH_REGISTER));
#endif

	return 0;
}

static int tran_disable(int channel)
{
	int enableregval = 0;
	enableregval = tran_flash_read(tran_device.i2c_client, TRAN_ENABLE_REGISTER);
	tran_timer_cancel(channel);

	if((tran_device.regulator_control!=NULL)&&regulator_is_enabled(tran_device.regulator_control))
		regulator_disable(tran_device.regulator_control);
	switch (channel) {
		case BACK_CHANNEL:
			if(tran_device.channel[FRONT_WARM_CHANNEL].state == TRAN_DISABLE)
				tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, enableregval & TRAN_DISABLE_BACK_LED);
			break;
		case FRONT_CHANNEL:
			if(flash_type == 1)
			{
				//if(tran_device.channel[BACK_WARM_CHANNEL].state == TRAN_DISABLE)
				tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, enableregval & TRAN_DISABLE_FRONT_LED);
			}
			else
				tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, enableregval & TRAN_DISABLE_FRONT_LED);
			break;
		case BACK_WARM_CHANNEL:
			if(flash_type == 1)
			{
				if(tran_device.channel[FRONT_CHANNEL].state == TRAN_DISABLE)
					tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, enableregval & TRAN_DISABLE_BACK_WARM_LED);
			}
			else{
				if(tran_device.channel[BACK_CHANNEL].state == TRAN_DISABLE)
					tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, enableregval & TRAN_DISABLE_BACK_WARM_LED);
			}
			break;
		case FRONT_WARM_CHANNEL:
			if(tran_device.channel[BACK_CHANNEL].state == TRAN_DISABLE)
				tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, enableregval & TRAN_DISABLE_FRONT_WARM_LED);
			break;
		default:
			pr_info("Error channel");
	}

	if ((tran_device.channel[channel].regulator != NULL) && regulator_is_enabled(tran_device.channel[channel].regulator)){
		regulator_disable(tran_device.channel[channel].regulator);
	}
	tran_device.channel[channel].state = TRAN_DISABLE;

#if 0
	pr_info("%s TRAN_ENABLE_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_ENABLE_REGISTER));
	pr_info("%s TRAN_BACK_FLASH_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_BACK_FLASH_REGISTER));
	pr_info("%s TRAN_BACK_TORCH_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_BACK_TORCH_REGISTER));
	pr_info("%s TRAN_FRONT_FLASH_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_FRONT_FLASH_REGISTER));
	pr_info("%s TRAN_FRONT_TORCH_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_FRONT_TORCH_REGISTER));
#endif

	return 0;
}

static int tran_set_scenario(int scenario)
{
	return 0;
}

static int tran_get_hw_fault(void)
{
	int enableregval = 0;
	enableregval = tran_flash_read(tran_device.i2c_client, TRAN_FLAGS1_REGISTER);
	PR_INFO(debug_mask,"enableregval 0x0A = %d\n",enableregval);
	enableregval += tran_flash_read(tran_device.i2c_client, TRAN_FLAGS2_REGISTER);
	PR_INFO(debug_mask,"enableregval 0x0B = %d\n",enableregval);
	return enableregval;
}

static int tran_init(void)
{
	pr_debug("%s:%d start", __FUNCTION__, __LINE__);

	tran_device.channel[BACK_CHANNEL].duty = 0;
	tran_device.channel[FRONT_CHANNEL].duty = 0;
	tran_device.channel[BACK_WARM_CHANNEL].duty = 0;
	tran_device.channel[FRONT_WARM_CHANNEL].duty = 0;

	tran_device.channel[BACK_CHANNEL].state = TRAN_DISABLE;
	tran_device.channel[FRONT_CHANNEL].state = TRAN_DISABLE;
	tran_device.channel[BACK_WARM_CHANNEL].state = TRAN_DISABLE;
	tran_device.channel[FRONT_WARM_CHANNEL].state = TRAN_DISABLE;

	tran_device.channel[BACK_CHANNEL].current = 0;
	tran_device.channel[FRONT_CHANNEL].current = 0;
	tran_device.channel[BACK_WARM_CHANNEL].current = 0;
	tran_device.channel[FRONT_WARM_CHANNEL].current = 0;

	tran_device.channel[BACK_CHANNEL].timeout_ms = 0;
	tran_device.channel[FRONT_CHANNEL].timeout_ms = 0;
	tran_device.channel[BACK_WARM_CHANNEL].timeout_ms = 0;
	tran_device.channel[FRONT_WARM_CHANNEL].timeout_ms = 0;

	if (tran_device.regulator)
		(void)regulator_enable(tran_device.regulator);
	mdelay(3);
	tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, 0x00);
	tran_flash_write(tran_device.i2c_client, TRAN_BOOST_CONFIG_REGISTER, 0x09);
	tran_flash_write(tran_device.i2c_client, TRAN_TIMING_CONFIG_REGISTER, 0x1f);
	//LED1 torch&flash bit7 set to 0, defualt 1 //OCP81378
	tran_flash_write(tran_device.i2c_client, TRAN_FRONT_FLASH_REGISTER, 0x00);
	tran_flash_write(tran_device.i2c_client, TRAN_FRONT_TORCH_REGISTER, 0x00);

	pr_debug("%s:%d end", __FUNCTION__, __LINE__);
	return 0;
}

static int tran_uninit(void)
{
	PR_INFO(debug_mask,"%s:%d start", __FUNCTION__, __LINE__);

	tran_device.channel[BACK_CHANNEL].duty = 0;
	tran_device.channel[FRONT_CHANNEL].duty = 0;
	tran_device.channel[BACK_WARM_CHANNEL].duty = 0;
	tran_device.channel[FRONT_WARM_CHANNEL].duty = 0;

	tran_device.channel[BACK_CHANNEL].state = TRAN_NONE;
	tran_device.channel[FRONT_CHANNEL].state = TRAN_NONE;
	tran_device.channel[BACK_WARM_CHANNEL].state = TRAN_NONE;
	tran_device.channel[FRONT_WARM_CHANNEL].state = TRAN_NONE;

	tran_device.channel[BACK_CHANNEL].current = 0;
	tran_device.channel[FRONT_CHANNEL].current = 0;
	tran_device.channel[BACK_WARM_CHANNEL].current = 0;
	tran_device.channel[FRONT_WARM_CHANNEL].current = 0;

	tran_get_hw_fault();
	tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, 0x00);

	if((tran_device.regulator_control!=NULL)&&regulator_is_enabled(tran_device.regulator_control))
		regulator_disable(tran_device.regulator_control);

	if ((tran_device.channel[BACK_CHANNEL].regulator != NULL) && regulator_is_enabled(tran_device.channel[BACK_CHANNEL].regulator)){
		regulator_disable(tran_device.channel[BACK_CHANNEL].regulator);
	}
	if ((tran_device.channel[FRONT_CHANNEL].regulator != NULL) && regulator_is_enabled(tran_device.channel[FRONT_CHANNEL].regulator)){
		regulator_disable(tran_device.channel[FRONT_CHANNEL].regulator);
	}
	if ((tran_device.channel[BACK_WARM_CHANNEL].regulator != NULL) && regulator_is_enabled(tran_device.channel[BACK_WARM_CHANNEL].regulator)){
		regulator_disable(tran_device.channel[BACK_WARM_CHANNEL].regulator);
	}
	if ((tran_device.channel[FRONT_WARM_CHANNEL].regulator != NULL) && regulator_is_enabled(tran_device.channel[FRONT_WARM_CHANNEL].regulator)){
		regulator_disable(tran_device.channel[FRONT_WARM_CHANNEL].regulator);
	}
	if ((tran_device.regulator != NULL) && regulator_is_enabled(tran_device.regulator)){
		regulator_disable(tran_device.regulator);
	}


	pr_debug("%s:%d end", __FUNCTION__, __LINE__);
	return 0;
}

static void tran_work_disable_back(struct work_struct *data)
{
	pr_debug("ht work queue callback\n");
	tran_disable(BACK_CHANNEL);
}

static void tran_work_disable_front(struct work_struct *data)
{
	pr_debug("lt work queue callback\n");
	tran_disable(FRONT_CHANNEL);
}

static void tran_work_disable_back_warm(struct work_struct *data)
{
	pr_debug("lt work queue callback\n");
	tran_disable(BACK_WARM_CHANNEL);
}

static void tran_work_disable_front_warm(struct work_struct *data)
{
	pr_debug("lt work queue callback\n");
	tran_disable(FRONT_WARM_CHANNEL);
}

static enum hrtimer_restart tran_timer_func_back(struct hrtimer *timer)
{
	schedule_work(&tran_device.channel[BACK_CHANNEL].workqueue);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tran_timer_func_front(struct hrtimer *timer)
{
	schedule_work(&tran_device.channel[FRONT_CHANNEL].workqueue);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tran_timer_func_back_warm(struct hrtimer *timer)
{
	schedule_work(&tran_device.channel[BACK_WARM_CHANNEL].workqueue);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tran_timer_func_front_warm(struct hrtimer *timer)
{
	schedule_work(&tran_device.channel[FRONT_WARM_CHANNEL].workqueue);
	return HRTIMER_NORESTART;
}

static int tran_timer_start(int channel, ktime_t ktime)
{
	if (tran_device.channel[channel].state != TRAN_FLASH && tran_device.channel[channel].state != TRAN_TORCH)
		return -1;

	if (channel < MAX_CHANNEL)
		hrtimer_start(&tran_device.channel[channel].timer, ktime, HRTIMER_MODE_REL);
	else
		return -1;

	return 0;
}

static int tran_timer_cancel(int channel)
{
	if (tran_device.channel[channel].state != TRAN_FLASH && tran_device.channel[channel].state != TRAN_TORCH)
		return -1;

	if (channel < MAX_CHANNEL)
		hrtimer_cancel(&tran_device.channel[channel].timer);
	else
		return -1;

	return 0;
}

static int tran_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		PR_INFO(debug_mask,"FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n", channel, (int)fl_arg->arg);
		if(fl_arg->arg >= 0)
			tran_device.channel[channel].timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		PR_INFO(debug_mask,"FLASH_IOC_SET_DUTY(%d): %d\n", channel, (int)fl_arg->arg);
		if (fl_arg->arg < 0)
			fl_arg->arg = 0;
		tran_device.channel[channel].current = tran_device.channel[channel].current_pointer[fl_arg->arg];
		tran_device.channel[channel].duty = fl_arg->arg;

		tran_set_current(channel, tran_device.channel[channel].current);
		break;

	case FLASH_IOC_SET_CURRENT:
		PR_INFO(debug_mask,"FLASH_IOC_SET_CURRENT(%d): %d\n", channel, (int)fl_arg->arg);
		if (fl_arg->arg > TRAN_FLASH_MAX_CURRENT)
			fl_arg->arg = TRAN_FLASH_MAX_CURRENT;
		tran_device.channel[channel].current = fl_arg->arg;
		tran_device.channel[channel].duty = 0;

		tran_set_current(channel, tran_device.channel[channel].current);
		break;

	case FLASH_IOC_GENERATE_DEV_PARA:
		PR_INFO(debug_mask,"FLASH_IOC_GENERATE_DEV_PARA(%d): %d\n", channel, (int)fl_arg->arg);
		break;

	case FLASH_IOC_SET_SCENARIO:
		PR_INFO(debug_mask,"FLASH_IOC_SET_SCENARIO(%d): %d\n", channel, (int)fl_arg->arg);
		tran_set_scenario(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n", channel, (int)fl_arg->arg);
		mutex_lock(&tran_device.lock);
		if (fl_arg->arg == 1)
			tran_enable(channel);
		else
			tran_disable(channel);
		mutex_unlock(&tran_device.lock);
		break;

	case FLASH_IOC_IS_CHARGER_READY:
		PR_INFO(debug_mask,"FLASH_IOC_SET_ONOFF(%d): %d\n", channel, (int)fl_arg->arg);
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		PR_INFO(debug_mask,"FLASH_IOC_GET_DUTY_NUMBER(%d)\n", tran_device.channel[channel].current_number);
		fl_arg->arg = tran_device.channel[channel].current_number;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		PR_INFO(debug_mask,"FLASH_IOC_GET_MAX_TORCH_DUTY\n");
		if (channel == BACK_CHANNEL)
		{
			if(channel_switch ==  1)
				fl_arg->arg = TRAN_FRONT_FLASH_MIN_DUTY;
			else
				fl_arg->arg = TRAN_BACK_FLASH_MIN_DUTY;
		}
		else if (channel == FRONT_CHANNEL)
		{
			if(channel_switch ==  1)
				fl_arg->arg = TRAN_BACK_FLASH_MIN_DUTY;
			else
				fl_arg->arg = TRAN_FRONT_FLASH_MIN_DUTY;
		}
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		PR_INFO(debug_mask,"FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n", channel, fl_arg->arg);
		if (fl_arg->arg >= tran_device.channel[channel].current_number)
			fl_arg->arg = tran_device.channel[channel].current_number - 1;
		fl_arg->arg = tran_device.channel[channel].current_pointer[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_FAULT:
		PR_INFO(debug_mask,"FLASH_IOC_GET_HW_FAULT(%d)\n", channel);
		fl_arg->arg = tran_get_hw_fault();
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n", channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int tran_open(void)
{
	return 0;
}

static int tran_release(void)
{
#if 0
	mutex_lock(&tran_device.lock);
	tran_device.use_count--;
	if (!tran_device.use_count)
		tran_uninit();

	if (tran_device.use_count < 0)
		tran_device.use_count = 0;
	mutex_unlock(&tran_device.lock);
#endif
	pr_debug("Release: %d\n", tran_device.use_count);
	return 0;
}

static int tran_set_driver(int set)
{
	int ret = 0;

	mutex_lock(&tran_device.lock);
	if (set) {
		if (!tran_device.use_count)
			ret = tran_init();

		tran_device.use_count++;
		pr_debug("Set driver: %d\n", tran_device.use_count);
	} else {
		tran_device.use_count--;
		if (!tran_device.use_count)
			ret = tran_uninit();

		if (tran_device.use_count < 0)
			tran_device.use_count = 0;
		pr_debug("Unset driver: %d\n", tran_device.use_count);
	}
	mutex_unlock(&tran_device.lock);

	return 0;
}
static ssize_t tran_strobe_store(struct flashlight_arg arg)
{
	tran_set_driver(1);
	tran_device.channel[arg.channel].current = tran_device.channel[arg.channel].current_pointer[arg.level];
	tran_set_current(arg.channel, tran_device.channel[arg.channel].current);
	tran_enable(arg.channel);
	mdelay(arg.dur);
	tran_disable(arg.channel);
	tran_set_driver(0);
	tran_release();

	return 0;
}

static struct flashlight_operations tran_ops = {
	tran_open,
	tran_release,
	tran_ioctl,
	tran_strobe_store,
	tran_set_driver
};

#if 0
static void current_cover_4_feature(unsigned int type, unsigned int channel, unsigned int feature)
{
	pr_info("current_cover_4_feature: type = %d, channel = %d, feature = %d", type, channel, feature);

	if (type == 0)
	{
		switch (feature) {
			case FLASH_CUSTOM_STROBE_TYPE_NORMAL:
				tran_device.channel[channel].current_pointer = back_channel_current;
				tran_device.channel[channel].current_number = sizeof(back_channel_current)/sizeof(int);
			break;
			case FLASH_CUSTOM_STROBE_TYPE_COLDCOLOR:
				tran_device.channel[channel].current_pointer = back_channel_current; //back cold no defined
				tran_device.channel[channel].current_number = sizeof(back_channel_current)/sizeof(int);
			break;
			case FLASH_CUSTOM_STROBE_TYPE_WARMCOLOR:
				tran_device.channel[channel].current_pointer = back_channel_current; //back warm no defined
				tran_device.channel[channel].current_number = sizeof(back_channel_current)/sizeof(int);
			break;
			case FLASH_CUSTOM_STROBE_TYPE_PHOTONIC:
				tran_device.channel[channel].current_pointer = back_photonic_channel_current;
				tran_device.channel[channel].current_number = sizeof(back_photonic_channel_current)/sizeof(int);
			break;
			default:
				pr_err("current_cover_4_feature: error feature = %d", feature);
			break;
		}
	}
	else if (type == 1)
	{
		switch (feature) {
			case FLASH_CUSTOM_STROBE_TYPE_NORMAL:
				tran_device.channel[channel].current_pointer = front_channel_current;
				tran_device.channel[channel].current_number = sizeof(front_channel_current)/sizeof(int);
			break;
			case FLASH_CUSTOM_STROBE_TYPE_COLDCOLOR:
				tran_device.channel[channel].current_pointer = front_cold_channel_current;
				tran_device.channel[channel].current_number = sizeof(front_cold_channel_current)/sizeof(int);
			break;
			case FLASH_CUSTOM_STROBE_TYPE_WARMCOLOR:
				tran_device.channel[channel].current_pointer = front_warm_channel_current;
				tran_device.channel[channel].current_number = sizeof(front_warm_channel_current)/sizeof(int);
			break;
			case FLASH_CUSTOM_STROBE_TYPE_PHOTONIC:
				tran_device.channel[channel].current_pointer = front_channel_current; //front photonic no defined
				tran_device.channel[channel].current_number = sizeof(front_channel_current)/sizeof(int);
			break;
			default:
				pr_err("current_cover_4_feature: error feature = %d", feature);
			break;
		}
	}
	else
		pr_err("current_cover_4_feature: error type = %d", type);
}
#endif

static int tran_parse_dt(struct i2c_client *client)
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
		if (flashlight_dev_register_by_device_id(&flash_dev_id[i],&tran_ops))
			return -EFAULT;
		i++;
	}
	if((flash_dev_id[0].type == 0) && (flash_dev_id[0].channel == 1))
		channel_switch = 1;	
	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int tran_i2c_probe(struct i2c_client *client)
{
	int err;
	struct tran_chip_data *chip;
	struct tran_platform_data *pdata = client->dev.platform_data;
	int back_dtsconfig,front_dtsconfig,back_warm_dtsconfig,front_warm_dtsconfig;

	pr_debug("%s:%d start", __FUNCTION__, __LINE__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	chip = kzalloc(sizeof(struct tran_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	client->addr = TRAN_I2C_SLAVE_ADDR;
	chip->client = client;

	if (!pdata) {
		pr_err("Platform data does not exist\n");
		pdata = kzalloc(sizeof(struct tran_platform_data), GFP_KERNEL);
	}

	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	tran_device.i2c_client = client;

	mutex_init(&chip->lock);

	tran_device.regulator = regulator_get_optional(&client->dev, "tran_regulator");
	if (IS_ERR(tran_device.regulator)) {
		err = PTR_ERR(tran_device.regulator);
		tran_device.regulator = NULL;
		pr_err("tran_device.regulator get fail!");
		if(err == -ENODEV)
			pr_err("tran_device.regulator get fail!  DTS no found,no using dummy regulator");
		else
			pr_err("tran_device.regulator get fail! err number = %d",err);
	}
	of_property_read_u32(client->dev.of_node, "flash_type", &flash_type);
	pr_info("tran_i2c_probe flash_type=%d\n",flash_type);

	tran_device.regulator_control = regulator_get_optional(&client->dev, "tran_control");
	if (IS_ERR(tran_device.regulator_control)) {
		err = PTR_ERR(tran_device.regulator_control);
		tran_device.regulator_control = NULL;
		TRAN_ENABLE_FLASH_MODE = TRAN_ENABLE_FLASH_MODE_INT;
		TRAN_ENABLE_TORCH_MODE = TRAN_ENABLE_TORCH_MODE_INT;

		TRAN_ENABLE_BACK_LED = TRAN_ENABLE_BACK_LED_INT;
		TRAN_ENABLE_FRONT_LED = TRAN_ENABLE_FRONT_LED_INT;
		TRAN_DISABLE_BACK_LED = TRAN_DISABLE_BACK_LED_INT;
		TRAN_DISABLE_FRONT_LED = TRAN_DISABLE_FRONT_LED_INT;
		pr_err("tran_device.regulator_control get fail!");
		if(err == -ENODEV)
			pr_err("tran_device.regulator_control get fail!  DTS no found,no using dummy regulator");
		else
			pr_err("tran_device.regulator_control get fail! err number = %d",err);
	}else{
		TRAN_ENABLE_FLASH_MODE = TRAN_ENABLE_FLASH_MODE_EXT;
		TRAN_ENABLE_TORCH_MODE = TRAN_ENABLE_TORCH_MODE_EXT;

		TRAN_ENABLE_BACK_LED = TRAN_ENABLE_BACK_LED_EXT;
		TRAN_ENABLE_FRONT_LED = TRAN_ENABLE_FRONT_LED_EXT;
		TRAN_DISABLE_BACK_LED = TRAN_DISABLE_BACK_LED_EXT;
		TRAN_DISABLE_FRONT_LED = TRAN_DISABLE_FRONT_LED_EXT;
	}


	tran_device.channel[BACK_CHANNEL].regulator = regulator_get_optional(&client->dev, "back");
	if (IS_ERR(tran_device.channel[BACK_CHANNEL].regulator)) {
		err = PTR_ERR(tran_device.channel[BACK_CHANNEL].regulator);
		tran_device.channel[BACK_CHANNEL].regulator = NULL;
		if(err == -ENODEV)
			pr_err("tran_device.channel[BACK_CHANNEL].regulator get fail!  DTS no found,no using dummy regulator");
		else
			pr_err("tran_device.channel[BACK_CHANNEL].regulator get fail! err number = %d",err);
	}else{
		back_dtsconfig = 1;
		pr_info("tran_device.channel[BACK_CHANNEL].regulator get success!");
	}

	tran_device.channel[FRONT_CHANNEL].regulator = regulator_get_optional(&client->dev, "front");
	if (IS_ERR(tran_device.channel[FRONT_CHANNEL].regulator)) {
		err = PTR_ERR(tran_device.channel[FRONT_CHANNEL].regulator);
		tran_device.channel[FRONT_CHANNEL].regulator = NULL;
		if(err == -ENODEV)
			pr_err("tran_device.channel[FRONT_CHANNEL].regulator get fail!  DTS no found,no using dummy regulator");
		else
			pr_err("tran_device.channel[FRONT_CHANNEL].regulator get fail! err number = %d",err);
	}else{
		front_dtsconfig = 1;
		pr_info("tran_device.channel[FRONT_CHANNEL].regulator get success!");
	}

	tran_device.channel[BACK_WARM_CHANNEL].regulator = regulator_get_optional(&client->dev, "back_warm");
	if (IS_ERR(tran_device.channel[BACK_WARM_CHANNEL].regulator)) {
		err = PTR_ERR(tran_device.channel[BACK_WARM_CHANNEL].regulator);
		tran_device.channel[BACK_WARM_CHANNEL].regulator = NULL;
		if(err == -ENODEV)
			pr_err("tran_device.channel[BACK_WARM_CHANNEL].regulator get fail!  DTS no found,no using dummy regulator");
		else
			pr_err("tran_device.channel[BACK_WARM_CHANNEL].regulator get fail! err number = %d",err);
	}else{
		back_warm_dtsconfig = 1;
		pr_info("tran_device.channel[BACK_WARM_CHANNEL].regulator get success!");
	}

	tran_device.channel[FRONT_WARM_CHANNEL].regulator = regulator_get_optional(&client->dev, "front_warm");
	if (IS_ERR(tran_device.channel[FRONT_WARM_CHANNEL].regulator)) {
		err = PTR_ERR(tran_device.channel[FRONT_WARM_CHANNEL].regulator);
		tran_device.channel[FRONT_WARM_CHANNEL].regulator = NULL;
		if(err == -ENODEV)
			pr_err("tran_device.channel[FRONT_WARM_CHANNEL].regulator get fail!  DTS no found,no using dummy regulator");
		else
			pr_err("tran_device.channel[FRONT_WARM_CHANNEL].regulator get fail! err number = %d",err);
	}else{
		front_warm_dtsconfig = 1;
		pr_info("tran_device.channel[FRONT_WARM_CHANNEL].regulator get success!");
	}

	if(flash_type == 1 && back_warm_dtsconfig == 1 && front_dtsconfig == 1 && back_dtsconfig == 0)
	{
		TRAN_ENABLE_BACK_WARM_LED =  TRAN_ENABLE_FRONT_LED;
		TRAN_DISABLE_BACK_WARM_LED = TRAN_DISABLE_FRONT_LED;

		TRAN_BACK_WARM_TORCH_REGISTER = TRAN_FRONT_TORCH_REGISTER;
	}
	else if(flash_type == 1 && back_warm_dtsconfig == 1 && back_dtsconfig == 1 && front_dtsconfig == 0)
	{
		TRAN_ENABLE_BACK_WARM_LED =  TRAN_ENABLE_BACK_LED;
		TRAN_DISABLE_BACK_WARM_LED = TRAN_DISABLE_BACK_LED;

		TRAN_BACK_WARM_TORCH_REGISTER = TRAN_BACK_TORCH_REGISTER;
	}
	else
	{
		TRAN_ENABLE_BACK_WARM_LED  = TRAN_ENABLE_FRONT_LED;
		TRAN_DISABLE_BACK_WARM_LED  = TRAN_DISABLE_FRONT_LED;

		TRAN_BACK_WARM_TORCH_REGISTER = TRAN_FRONT_TORCH_REGISTER;
	}

	tran_device.use_count = 0;
	mutex_init(&tran_device.lock);

	INIT_WORK(&tran_device.channel[BACK_CHANNEL].workqueue, tran_work_disable_back);
	INIT_WORK(&tran_device.channel[FRONT_CHANNEL].workqueue, tran_work_disable_front);
	INIT_WORK(&tran_device.channel[BACK_WARM_CHANNEL].workqueue, tran_work_disable_back_warm);
	INIT_WORK(&tran_device.channel[FRONT_WARM_CHANNEL].workqueue, tran_work_disable_front_warm);

	hrtimer_init(&tran_device.channel[BACK_CHANNEL].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_init(&tran_device.channel[FRONT_CHANNEL].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_init(&tran_device.channel[BACK_WARM_CHANNEL].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_init(&tran_device.channel[FRONT_WARM_CHANNEL].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	tran_device.channel[BACK_CHANNEL].timer.function = tran_timer_func_back;
	tran_device.channel[FRONT_CHANNEL].timer.function = tran_timer_func_front;
	tran_device.channel[BACK_WARM_CHANNEL].timer.function = tran_timer_func_back_warm;
	tran_device.channel[FRONT_WARM_CHANNEL].timer.function = tran_timer_func_front_warm;

	tran_device.channel[BACK_CHANNEL].timeout_ms = 0;
	tran_device.channel[FRONT_CHANNEL].timeout_ms = 0;
	tran_device.channel[BACK_WARM_CHANNEL].timeout_ms = 0;
	tran_device.channel[FRONT_WARM_CHANNEL].timeout_ms = 0;

	if (tran_device.regulator)
		(void)regulator_enable(tran_device.regulator);
	mdelay(3);
	if(tran_device.i2c_client == NULL)
	{
		pr_err("i2c client is null");
		return -1;
	}
	device_id = tran_flash_read(tran_device.i2c_client, TRAN_DEVICE_ID_REGISTER);
	chip_id = tran_flash_read(tran_device.i2c_client, TRAN_CHIP_ID_REGISTER);
	tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, 0x00);
	if (tran_device.regulator)
		regulator_disable(tran_device.regulator);
	pr_info("tran read DEVICE_ID = 0x%x", device_id);
	pr_info("tran read chip_id = 0x%x", chip_id);
	if ((device_id == 0x02)||(device_id == 0x3a)||(chip_id == 0x22)||(device_id == 0x11)) {
		pr_err("tran DEVICE_ID match, strat register device");
		if (tran_parse_dt(client)) {
			pr_err("aw36515 flashlight_dev_register fail.");
			err = -EFAULT;
			goto err_free;
		}
	} else {
		pr_err("tran DEVICE_ID mismatch, skip.");
		err = -EFAULT;
		goto err_free;
	}
	if(channel_switch == 0)
	{
		tran_device.channel[BACK_CHANNEL].current_pointer = back_channel_current;
		tran_device.channel[BACK_CHANNEL].current_number = sizeof(back_channel_current)/sizeof(int);
	//#if defined(CONFIG_TRAN_FLASH_BRATHE_LED_COMMMON)
		if((flash_type == 0)||(flash_type == 1)||(flash_type == 3)||(flash_type == 4))
		{
		tran_device.channel[FRONT_CHANNEL].current_pointer = front_channel_current;
		tran_device.channel[FRONT_CHANNEL].current_number = sizeof(front_channel_current)/sizeof(int);
		}
		else{
		tran_device.channel[FRONT_CHANNEL].current_pointer = front_cold_channel_current;
		tran_device.channel[FRONT_CHANNEL].current_number = sizeof(front_cold_channel_current)/sizeof(int);
		}
		
		if(flash_type == 4)
		{
			tran_device.channel[BACK_WARM_CHANNEL].current_pointer = back_photonic_channel_current;
			tran_device.channel[BACK_WARM_CHANNEL].current_number = sizeof(back_photonic_channel_current)/sizeof(int);
		}
		else
		{
			tran_device.channel[BACK_WARM_CHANNEL].current_pointer = back_warm_channel_current;
			tran_device.channel[BACK_WARM_CHANNEL].current_number = sizeof(back_warm_channel_current)/sizeof(int);
		}
		tran_device.channel[FRONT_WARM_CHANNEL].current_pointer = front_warm_channel_current;
		tran_device.channel[FRONT_WARM_CHANNEL].current_number = sizeof(front_warm_channel_current)/sizeof(int);
	}
	else
	{
		tran_device.channel[BACK_CHANNEL].current_pointer = front_channel_current;
		tran_device.channel[BACK_CHANNEL].current_number = sizeof(front_channel_current)/sizeof(int);
	//#if defined(CONFIG_TRAN_FLASH_BRATHE_LED_COMMMON)
		if((flash_type == 0)||(flash_type == 1)||(flash_type == 3)||(flash_type == 4))
		{
		tran_device.channel[FRONT_CHANNEL].current_pointer = back_channel_current;
		tran_device.channel[FRONT_CHANNEL].current_number = sizeof(back_channel_current)/sizeof(int);
		}else{
		tran_device.channel[FRONT_CHANNEL].current_pointer = front_cold_channel_current;
		tran_device.channel[FRONT_CHANNEL].current_number = sizeof(front_cold_channel_current)/sizeof(int);
		}
		tran_device.channel[BACK_WARM_CHANNEL].current_pointer = back_channel_current;
		tran_device.channel[BACK_WARM_CHANNEL].current_number = sizeof(back_channel_current)/sizeof(int);
		tran_device.channel[FRONT_WARM_CHANNEL].current_pointer = front_warm_channel_current;
		tran_device.channel[FRONT_WARM_CHANNEL].current_number = sizeof(front_warm_channel_current)/sizeof(int);
	}
	pr_debug("%s:%d end", __FUNCTION__, __LINE__);

	return 0;

err_free:
	kfree(chip->pdata);
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static void tran_i2c_remove(struct i2c_client *client)
{

	pr_debug("%s:%d start", __FUNCTION__, __LINE__);

	flush_work(&tran_device.channel[BACK_CHANNEL].workqueue);
	flush_work(&tran_device.channel[FRONT_CHANNEL].workqueue);

	pr_debug("%s:%d end", __FUNCTION__, __LINE__);
}

static void tran_shutdown(struct i2c_client *client)
{
	//aw36515_flash_data->led_mode = 0;
	//aw36515_mode_ctrl(aw36515_flash_data);
	pr_info("tran_shutdown IN");
	if(tran_device.i2c_client != NULL){
		pr_info("tran_shutdown close torch");
		pr_info("%s TRAN_ENABLE_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_ENABLE_REGISTER));
		pr_info("%s TRAN_BACK_FLASH_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_BACK_FLASH_REGISTER));
		pr_info("%s TRAN_BACK_TORCH_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_BACK_TORCH_REGISTER));
		pr_info("%s TRAN_FRONT_FLASH_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_FRONT_FLASH_REGISTER));
		pr_info("%s TRAN_FRONT_TORCH_REGISTER = %d", __func__, tran_flash_read(tran_device.i2c_client, TRAN_FRONT_TORCH_REGISTER));
		tran_flash_write(tran_device.i2c_client, TRAN_ENABLE_REGISTER, TRAN_DISABLE_BACK_LED & TRAN_DISABLE_FRONT_LED);
	}
	pr_info("tran_shutdown OUT");
}

static const struct i2c_device_id tran_i2c_id[] = {
	{TRAN_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, tran_i2c_id);

static const struct of_device_id tran_i2c_of_match[] = {
	{.compatible = TRAN_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, tran_i2c_of_match);

static struct i2c_driver tran_i2c_driver = {
	.driver = {
		   .name = TRAN_NAME,
		   .of_match_table = tran_i2c_of_match,
		   },
	.probe = tran_i2c_probe,
	.remove = tran_i2c_remove,
	.shutdown = tran_shutdown,
	.id_table = tran_i2c_id,
};

module_i2c_driver(tran_i2c_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jie.Liu <jie.liu@transsion.com>");
MODULE_DESCRIPTION("Texas Instruments aw36515 flash driver");
