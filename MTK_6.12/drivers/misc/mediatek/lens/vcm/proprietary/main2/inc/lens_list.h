/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef _LENS_LIST_H

#define _LENS_LIST_H

//ITD: Modify Lens Drv Of ISP Team Start
#define CN3927VAF_SetI2Cclient CN3927VAF_SetI2Cclient_Main2
#define CN3927VAF_Ioctl        CN3927VAF_Ioctl_Main2
#define CN3927VAF_Release      CN3927VAF_Release_Main2
#define CN3927VAF_GetFileName  CN3927VAF_GetFileName_Main2
extern int  CN3927VAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient, spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long CN3927VAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param);
extern int  CN3927VAF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int  CN3927VAF_GetFileName(unsigned char *pFileName);
//ITD: Modify Lens Drv Of ISP Team End
#define CN3938VAF_SetI2Cclient CN3938VAF_SetI2Cclient_Main2
#define CN3938VAF_Ioctl        CN3938VAF_Ioctl_Main2
#define CN3938VAF_Release      CN3938VAF_Release_Main2
#define CN3938VAF_GetFileName  CN3938VAF_GetFileName_Main2
extern int  CN3938VAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient, spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long CN3938VAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param);
extern int  CN3938VAF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int  CN3938VAF_GetFileName(unsigned char *pFileName);

#define AK7371AF_SetI2Cclient AK7371AF_SetI2Cclient_Main2
#define AK7371AF_Ioctl AK7371AF_Ioctl_Main2
#define AK7371AF_Release AK7371AF_Release_Main2
#define AK7371AF_PowerDown AK7371AF_PowerDown_Main2
#define AK7371AF_GetFileName AK7371AF_GetFileName_Main2
extern int AK7371AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long AK7371AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int AK7371AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int AK7371AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int AK7371AF_GetFileName(unsigned char *pFileName);

#define AK7374AF_SetI2Cclient AK7374AF_SetI2Cclient_Main2
#define AK7374AF_Ioctl AK7374AF_Ioctl_Main2
#define AK7374AF_Release AK7374AF_Release_Main2
#define AK7374AF_PowerDown AK7374AF_PowerDown_Main2
#define AK7374AF_GetFileName AK7374AF_GetFileName_Main2
extern int AK7374AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long AK7374AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int AK7374AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int AK7374AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int AK7374AF_GetFileName(unsigned char *pFileName);

#define AW8601CSRAF_SetI2Cclient AW8601CSRAF_SetI2Cclient_Main2
#define AW8601CSRAF_Ioctl AW8601CSRAF_Ioctl_Main2
#define AW8601CSRAF_Release AW8601CSRAF_Release_Main2
#define AW8601CSRAF_PowerDown AW8601CSRAF_PowerDown_Main2
#define AW8601CSRAF_GetFileName AW8601CSRAF_GetFileName_Main2
extern int AW8601CSRAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long AW8601CSRAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
				unsigned long a_u4Param);
extern int AW8601CSRAF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int AW8601CSRAF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int AW8601CSRAF_GetFileName(unsigned char *pFileName);

#define GT9772AF_SetI2Cclient GT9772AF_SetI2Cclient_Main2
#define GT9772AF_Ioctl GT9772AF_Ioctl_Main2
#define GT9772AF_Release GT9772AF_Release_Main2
#define GT9772AF_PowerDown GT9772AF_PowerDown_Main2
#define GT9772AF_GetFileName GT9772AF_GetFileName_Main2
extern int GT9772AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long GT9772AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
				unsigned long a_u4Param);
extern int GT9772AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int GT9772AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int GT9772AF_GetFileName(unsigned char *pFileName);

#define LC898212XDAF_F_SetI2Cclient LC898212XDAF_F_SetI2Cclient_Main2
#define LC898212XDAF_F_Ioctl LC898212XDAF_F_Ioctl_Main2
#define LC898212XDAF_F_Release LC898212XDAF_F_Release_Main2
#define LC898212XDAF_F_GetFileName LC898212XDAF_F_GetFileName_Main2
extern int LC898212XDAF_F_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				       spinlock_t *pAF_SpinLock,
				       int *pAF_Opened);
extern long LC898212XDAF_F_Ioctl(struct file *a_pstFile,
				 unsigned int a_u4Command,
				 unsigned long a_u4Param);
extern int LC898212XDAF_F_Release(struct inode *a_pstInode,
				  struct file *a_pstFile);
extern int LC898212XDAF_F_GetFileName(unsigned char *pFileName);

#define LC898217AF_SetI2Cclient LC898217AF_SetI2Cclient_Main2
#define LC898217AF_Ioctl LC898217AF_Ioctl_Main2
#define LC898217AF_Release LC898217AF_Release_Main2
#define LC898217AF_PowerDown LC898217AF_PowerDown_Main2
#define LC898217AF_GetFileName LC898217AF_GetFileName_Main2
extern int LC898217AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int LC898217AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int LC898217AF_GetFileName(unsigned char *pFileName);

#define LC898217AFA_SetI2Cclient LC898217AFA_SetI2Cclient_Main2
#define LC898217AFA_Ioctl LC898217AFA_Ioctl_Main2
#define LC898217AFA_Release LC898217AFA_Release_Main2
#define LC898217AFA_PowerDown LC898217AFA_PowerDown_Main2
#define LC898217AFA_GetFileName LC898217AFA_GetFileName_Main2
extern int LC898217AFA_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AFA_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AFA_Release(struct inode *a_pstInode,
			       struct file *a_pstFile);
extern int LC898217AFA_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int LC898217AFA_GetFileName(unsigned char *pFileName);

#define LC898217AFB_SetI2Cclient LC898217AFB_SetI2Cclient_Main2
#define LC898217AFB_Ioctl LC898217AFB_Ioctl_Main2
#define LC898217AFB_Release LC898217AFB_Release_Main2
#define LC898217AFB_PowerDown LC898217AFB_PowerDown_Main2
#define LC898217AFB_GetFileName LC898217AFB_GetFileName_Main2
extern int LC898217AFB_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AFB_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AFB_Release(struct inode *a_pstInode,
			       struct file *a_pstFile);
extern int LC898217AFB_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int LC898217AFB_GetFileName(unsigned char *pFileName);

#define LC898217AFC_SetI2Cclient LC898217AFC_SetI2Cclient_Main2
#define LC898217AFC_Ioctl LC898217AFC_Ioctl_Main2
#define LC898217AFC_Release LC898217AFC_Release_Main2
#define LC898217AFC_PowerDown LC898217AFC_PowerDown_Main2
#define LC898217AFC_GetFileName LC898217AFC_GetFileName_Main2
extern int LC898217AFC_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AFC_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AFC_Release(struct inode *a_pstInode,
			       struct file *a_pstFile);
extern int LC898217AFC_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int LC898217AFC_GetFileName(unsigned char *pFileName);

extern int bu64748af_SetI2Cclient_Main2(struct i2c_client *pstAF_I2Cclient,
					spinlock_t *pAF_SpinLock,
					int *pAF_Opened);
extern long bu64748af_Ioctl_Main2(struct file *a_pstFile,
				  unsigned int a_u4Command,
				  unsigned long a_u4Param);
extern int bu64748af_Release_Main2(struct inode *a_pstInode,
				   struct file *a_pstFile);
extern int bu64748af_PowerDown_Main2(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int bu64748af_GetFileName_Main2(unsigned char *pFileName);

#define DW9718TAF_SetI2Cclient DW9718TAF_SetI2Cclient_Main2
#define DW9718TAF_Ioctl DW9718TAF_Ioctl_Main2
#define DW9718TAF_Release DW9718TAF_Release_Main2
#define DW9718TAF_GetFileName DW9718TAF_GetFileName_Main2
extern int DW9718TAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				  spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9718TAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			    unsigned long a_u4Param);
extern int DW9718TAF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9718TAF_GetFileName(unsigned char *pFileName);

#define DW9784CAF_SetI2Cclient DW9784CAF_SetI2Cclient_Main2
#define DW9784CAF_Ioctl DW9784CAF_Ioctl_Main2
#define DW9784CAF_Release DW9784CAF_Release_Main2
#define DW9784CAF_GetFileName DW9784CAF_GetFileName_Main2
extern int DW9784CAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				  spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9784CAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			    unsigned long a_u4Param);
extern int DW9784CAF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9784CAF_GetFileName(unsigned char *pFileName);

//ITD: add AF common by hui.wang6 20251215 start
#define TRANMAIN2AF_SetI2Cclient TRANMAIN2AF_SetI2Cclient_Main2
#define TRANMAIN2AF_Ioctl TRANMAIN2AF_Ioctl_Main2
#define TRANMAIN2AF_Release TRANMAIN2AF_Release_Main2
#define TRANMAIN2AF_GetFileName TRANMAIN2AF_GetFileName_Main2
extern int TRANMAIN2AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
	spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long TRANMAIN2AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
	unsigned long a_u4Param);
extern int TRANMAIN2AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int TRANMAIN2AF_GetFileName(unsigned char *pFileName);
//ITD: add AF common by hui.wang6 20251215 end
#endif
