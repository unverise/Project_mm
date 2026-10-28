#ifndef GC08A8REAR_STANDBY
#define GC08A8REAR_STANDBY

#include <linux/delay.h>
#include <linux/string.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include "kd_camera_typedef.h"
#include "kd_camera_feature.h"

struct regulator *gc08a8rear_get_rst_pin(void)
{
	struct platform_device *pdev = NULL;
	struct device_node *of_node = NULL;
	of_node = of_find_compatible_node(NULL, NULL, "mediatek,imgsensor");
	if (of_node == NULL)
		return 0;
	pdev = of_find_device_by_node(of_node);
	if (pdev == NULL)
		return 0;
	return regulator_get(&pdev->dev, "cam2_vcamrst");
}

kal_uint8 gc08a8rear_is_hwstandby(struct regulator *regulator)
{
	if (regulator == NULL)
		return -1;
	if (regulator_is_enabled(regulator) == 1)
		return false; //no hwstandby
	else
		return true;  //hwstandby
}

void gc08a8rear_into_hwstandby(struct regulator *regulator)
{
	if (regulator_is_enabled(regulator))
		regulator_disable(regulator);
	pr_info("gc08a8rear into hwstandby");
}


void gc08a8rear_exit_hwstandby(struct regulator *regulator)
{
	int ret = 0;
	if (regulator){
		ret = regulator_enable(regulator);
		if (ret < 0) {
			pr_err("Failed to enable regulator: %d\n", ret);
		}
	}
	pr_info("gc08a8rear exit hwstandby");
}
#endif