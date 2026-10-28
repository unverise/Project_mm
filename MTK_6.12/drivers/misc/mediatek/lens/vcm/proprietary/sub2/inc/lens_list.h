/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



#ifndef _LENS_LIST_H

#define _LENS_LIST_H

//ITD: add AF common by hui.wang6 20251215 start
#define TRANSUB2AF_SetI2Cclient TRANSUB2AF_SetI2Cclient_Sub2
#define TRANSUB2AF_Ioctl TRANSUB2AF_Ioctl_Sub2
#define TRANSUB2AF_Release TRANSUB2AF_Release_Sub2
#define TRANSUB2AF_GetFileName TRANSUB2AF_GetFileName_Sub2
extern int TRANSUB2AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
	spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long TRANSUB2AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
	unsigned long a_u4Param);
extern int TRANSUB2AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int TRANSUB2AF_GetFileName(unsigned char *pFileName);
//ITD: add AF common by hui.wang6 20251215 end

#endif
