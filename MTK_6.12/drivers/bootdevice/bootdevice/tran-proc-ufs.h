#ifndef _TRAN_PROC_UFS_H_
#define _TRAN_PROC_UFS_H_

#include <ufs/ufshcd.h>

#ifdef CONFIG_TRAN_PROC_UFS
#define UFS_IOCTL_FFU_MAX_FW_VER_BYTES              (4)
void proc_ufs_feature_create(struct proc_dir_entry *feature_dir);
void proc_ufs_bootdevice_create(struct proc_dir_entry *bootdevice_dir);
void proc_ufs_get_rawCapacity(struct ufs_hba *hba);
void proc_ufs_set_fw_version(const char * fw_version);
void proc_ufs_set_manufacturerid(u16 manufacturerid);
void proc_ufs_set_productName(char *productName);
void proc_ufs_set_manufDate(struct ufs_hba*hba);
void proc_ufs_set_serialNumber(struct ufs_hba *hba);
void proc_ufs_set_lifetime(struct ufs_hba *hba);
void proc_ufs_set_ffu_support(struct ufs_hba*hba);
void tran_ufs_proc_bootdevice_work(struct work_struct *work);
#else
static inline __attribute__((unused)) void
tran_ufs_proc_bootdevice_work(struct work_struct *work
		__attribute__((unused)))
{
	return;
}
EXPORT_SYMBOL(tran_ufs_proc_bootdevice_work);
#endif
#endif
