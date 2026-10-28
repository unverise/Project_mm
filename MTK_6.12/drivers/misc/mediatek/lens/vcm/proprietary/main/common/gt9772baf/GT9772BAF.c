/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*
 * GT9772BAF voice coil motor driver
 *
 *
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/time.h>
#include <linux/uaccess.h>

#include "lens_info.h"

#define AF_DRVNAME "GT9772BAF_DRV"
#define AF_I2C_SLAVE_ADDR 0x18

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;
static long g_i4MotorStatus;
static long g_i4Dir;
static int g_sr = 3;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

/* ITD: Enable AF Low Power Mode By ISP Team Start */
void GT9772BAF_EnbleLowPower(bool enable)
{
	int i4RetValue = 0;
	char puSendCmd[2] = {0};

	pr_err("%s \n",__func__);
	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
	if(enable){
		puSendCmd[0] = 0x02;
		puSendCmd[1] = 0x01;//pwoer down mode
	}else{
		puSendCmd[0] = 0x02;
		puSendCmd[1] = 0x00;//power active
	}
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0){
		pr_err("GT9772B I2C tranfer err!");
	}
}
/* ITD: Enable AF Low Power Mode By ISP Team End */

static int i2c_read(u8 a_u2Addr, u8 *a_puBuff)
{
	int i4RetValue = 0;
	char puReadCmd[1] = {(char)(a_u2Addr)};

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puReadCmd, 1);
	if (i4RetValue < 0) {
		LOG_INF(" I2C write failed!!\n");
		return -1;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, (char *)a_puBuff, 1);
	if (i4RetValue < 0) {
		LOG_INF(" I2C read failed!!\n");
		return -1;
	}

	return 0;
}

static u8 read_data(u8 addr)
{
	u8 get_byte = 0xFF;

	i2c_read(addr, &get_byte);

	return get_byte;
}

#if 0
static int s4GT9772BAF_ReadReg(unsigned short *a_pu2Result)
{
	*a_pu2Result = (read_data(0x02) << 8) + (read_data(0x04) & 0xff);

	return 0;
}
#endif
static int s4AF_ReadReg(unsigned short *a_pu2Result)
{
	int i4RetValue = 0;
	char pBuff[2];

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, pBuff, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C read failed!!\n");
		return -1;
	}

	*a_pu2Result = (((u16) pBuff[0]) << 2) + (pBuff[1]);

	return 0;
}
static int s4AF_WriteReg(u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[3] = { 0x03, (char)(a_u2Data >> 8), (char)(a_u2Data & 0xFF) };

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	return 0;
}


static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	if (g_i4MotorStatus == 1)
		stMotorInfo.bIsMotorMoving = 1;
	else
		stMotorInfo.bIsMotorMoving = 0;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}


/* initAF include driver initialization and standby mode */
static int gt9772b_init(void)
{
	int i4RetValue = 0;
	char puSendCmd[3];

	printk("[GT9772BAF] gt9772b_init - beg\n");
	mdelay(5);
	puSendCmd[0] = 0x0B;
	puSendCmd[1] = 0x53;	//Init Setting Iout max=100mA
	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0)
	{
	   printk("[GT9772BAF] I2C send failed!! \n");
	   i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	   printk("[GT9772BAF] I2C send failed-two!! i4RetValue = %d \n",i4RetValue);
	}
	mdelay(5);
	puSendCmd[0] = 0x02;
	puSendCmd[1] = 0x02;   //EN AAC Mode
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0)
	{
	   printk("[GT9772BAF] I2C send failed!! \n");
	}
	puSendCmd[0] = 0x06;
	puSendCmd[1] = 0x00;   //AAC2
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0)
	{
	   printk("[GT9772BAF] I2C send failed!! \n");
	}
	puSendCmd[0] = 0x07;
	puSendCmd[1] = 0x07;   //TVIB=14ms
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0)
	{
	   printk("[GT9772BAF] I2C send failed!! \n");
	}



	LOG_INF("[GT9772BAF] GT97772 SRC Init End!! \n");

	return i4RetValue;
}


/* moveAF only use to control moving the motor */
static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		printk("out of range\n");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		unsigned short InitPos;

		gt9772b_init();
		ret = s4AF_ReadReg(&InitPos);

		if (ret == 0) {
			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = (unsigned long)InitPos;
			spin_unlock(g_pAF_SpinLock);

		} else {
			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = 0;
			spin_unlock(g_pAF_SpinLock);
		}

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	if (g_u4CurrPosition < a_u4Position) {
		spin_lock(g_pAF_SpinLock);
		g_i4Dir = 1;
		spin_unlock(g_pAF_SpinLock);
	} else if (g_u4CurrPosition > a_u4Position) {
		spin_lock(g_pAF_SpinLock);
		g_i4Dir = -1;
		spin_unlock(g_pAF_SpinLock);
	} else {
		return 0;
	}

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	/* LOG_INF("move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition); */

	spin_lock(g_pAF_SpinLock);
	g_sr = 3;
	g_i4MotorStatus = 0;
	spin_unlock(g_pAF_SpinLock);

	if (s4AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
	} else {
		printk("set I2C failed when moving the motor\n");

		spin_lock(g_pAF_SpinLock);
		g_i4MotorStatus = -1;
		spin_unlock(g_pAF_SpinLock);
	}

	return 0;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long GT9772BAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
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
/* ITD: Modify Enable LPM By Jesse 20190513 Start */
	case AFIOC_S_SETPOWERDOWN:
		GT9772BAF_EnbleLowPower((bool)a_u4Param);
		break;
/* ITD: Modify Enable LPM By Jesse 20190513 End */
	default:
		LOG_INF("No CMD\n");
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
int GT9772BAF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {

		LOG_INF("Wait\n");
		s4AF_WriteReg(250);
		msleep(15);
		s4AF_WriteReg(150);
		msleep(15);
		s4AF_WriteReg(100);
		msleep(15);
	}
	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");

	return 0;
}
/* ITD: Modify By ISP Team End */

int GT9772BAF_PowerDown(struct i2c_client *pstAF_I2Cclient,
			int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_Opened = pAF_Opened;

	LOG_INF("+\n");
	if (*g_pAF_Opened == 0) {
		int i4RetValue = 0;
		u8 data = 0x0;
		char puSendCmd[2] = {0x02, 0x01};

		g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
		g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
		i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

		data = read_data(0x02);
		LOG_INF("Addr:0x00 Data:0x%x\n", data);

		LOG_INF("apply - %d\n", i4RetValue);

		if (i4RetValue < 0)
			return -1;
	}
	LOG_INF("-\n");

	return 0;
}

int GT9772BAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			   spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	//initAF();

	return 1;
}

int GT9772BAF_GetFileName(unsigned char *pFileName)
{
	#if SUPPORT_GETTING_LENS_FOLDER_NAME
	char FilePath[256];
	char *FileString;

	sprintf(FilePath, "%s", __FILE__);
	FileString = strrchr(FilePath, '/');
	*FileString = '\0';
	FileString = (strrchr(FilePath, '/') + 1);
	strncpy(pFileName, FileString, AF_MOTOR_NAME);
	LOG_INF("FileName : %s\n", pFileName);
	#else
	pFileName[0] = '\0';
	#endif
	return 1;
}
