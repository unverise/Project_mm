/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __IMGSENSOR_SENSOR_H__
#define __IMGSENSOR_SENSOR_H__

#include "kd_camera_feature.h"
#include "kd_imgsensor_define.h"
#include "imgsensor_i2c.h"

enum IMGSENSOR_STATE {
	IMGSENSOR_STATE_CLOSE,
	IMGSENSOR_STATE_OPEN,
//ITD: Modify to avoid redundant streamon/streamoff By ISP Team 240311 Start
	IMGSENSOR_STATE_STREAMOFF,
	IMGSENSOR_STATE_STREAMON
//ITD: Modify to avoid redundant streamon/streamoff By ISP Team 240311 End
};

struct IMGSENSOR_SENSOR_STATUS {
	u32 reserved:28;
	u32 arch:4;
};

struct IMGSENSOR_SENSOR_INST {
	char *psensor_name;
//ITD:add project name by fei.wu5 20150801 start
	const char *project_name;
//ITD:add project name by fei.wu5 20150801 end
	enum IMGSENSOR_STATE state;
	enum IMGSENSOR_SENSOR_IDX sensor_idx;
//ITD:modify module_id by yingwei.cao 210524 start
	u32 smart_thermal_supoort;
//ITD:modify module_id by yingwei.cao 210524 end
	struct IMGSENSOR_I2C_CFG i2c_cfg;
	struct IMGSENSOR_SENSOR_STATUS status;
	struct IMGSENSOR_SENSOR_LIST *psensor_list;
	struct mutex sensor_mutex;
	struct timespec64 profile_time;
};
//ITD:modify module_id by yingwei.cao 210524 start
struct TRAN_MATERIALCODE_INFO {
	MUINT8 material_code[32];
	u16 module_dist_addr;
	u16 module_id;
};
//ITD:modify module_id by yingwei.cao 210524 end
struct IMGSENSOR_SENSOR {
	struct IMGSENSOR_SENSOR_INST  inst;
	struct SENSOR_FUNCTION_STRUCT *pfunc;
};

#endif

