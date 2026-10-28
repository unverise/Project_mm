/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * DW9784CAF voice coil motor driver
 *
 *
 */
//ITD:modify by camera driver team start
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
//#include "dw9784fw.h"
#include "lens_info.h"

#define AF_DRVNAME "DW9784CAF_DRV"

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...) \
	pr_info(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

extern int dw9784caf_init(void);
extern void dw9784caf_poweroff(void);
extern int dw9784caf_target(unsigned long afTarget);

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4CurrPosition;

static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	stMotorInfo.bIsMotorMoving = 1;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo,
			 sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

void DW9784CAF_EnbleLowPower(bool enable)
{
}

/* initAF include driver initialization and standby mode */
static int initAF(void)
{
	int ret = 0;
	LOG_INF(" wxy dw9784caf init af+\n");
	LOG_INF("initAF +\n");
	if (*g_pAF_Opened == 1) {
		ret = dw9784caf_init();
		if(ret < 0){
			LOG_INF(" dw9784caf init af fail\n");
			return ret;
		}
		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}
	dw9784caf_target(1024);
	LOG_INF("initAF -\n");
	LOG_INF(" wxy dw9784caf init af-\n");
	return 0;
}

/* moveAF only use to control moving the motor */
static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;
	//LOG_INF(" wxy dw9784caf move+ to %lu \n",a_u4Position);
	a_u4Position *= 2;
	ret = dw9784caf_target(a_u4Position);
	return ret;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	LOG_INF(" wxy setAFInf+\n");
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	LOG_INF(" wxy setAFInf-\n");
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	LOG_INF(" wxy setAFMacro+\n");
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	LOG_INF(" wxy setAFMacro-\n");
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long DW9784CAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		    unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue =
			getAFInfo((__user struct stAF_MotorInfo *)(a_u4Param));
		break;

	case AFIOC_T_MOVETO:
		i4RetValue = moveAF(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;
	case AFIOC_S_SETPOWERDOWN:
		DW9784CAF_EnbleLowPower((bool)a_u4Param);
		break;
	default:
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int DW9784CAF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
LOG_INF(" wxy DW9784CAF_Release+\n");
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		LOG_INF("apply\n");
		dw9784caf_target(1024);
		dw9784caf_poweroff();
		mdelay(12);
	}
	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	mdelay(1);

	LOG_INF("DW9784CAF Power down mode!\n");
	LOG_INF("End\n");
	LOG_INF(" wxy DW9784CAF_Release-\n");

	return 0;
}

int DW9784CAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			  spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	LOG_INF(" wxy DW9784CAF_SetI2Cclient+\n");
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	initAF();
	LOG_INF(" wxy DW9784CAF_SetI2Cclient-\n");
	return 1;
}

int DW9784CAF_GetFileName(unsigned char *pFileName)
{
	#if SUPPORT_GETTING_LENS_FOLDER_NAME
	char FilePath[256];
	char *FileString;
	LOG_INF(" wxy DW9784CAF_GetFileName+\n");
	sprintf(FilePath, "%s", __FILE__);
	FileString = strrchr(FilePath, '/');
	*FileString = '\0';
	FileString = (strrchr(FilePath, '/') + 1);
	strncpy(pFileName, FileString, AF_MOTOR_NAME);
	LOG_INF("FileName : %s\n", pFileName);
	#else
	pFileName[0] = '\0';
	#endif
	LOG_INF(" wxy DW9784CAF_GetFileName-\n");
	return 1;
}
