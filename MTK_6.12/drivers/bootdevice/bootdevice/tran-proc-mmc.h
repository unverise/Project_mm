
#ifndef _TRAN_PROC_MMC_H_
#define _TRAN_PROC_MMC_H_
#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>
#include <linux/proc_fs.h>
#ifdef CONFIG_TRAN_PROC_MMC
void proc_mmc_bootdevice_create(struct proc_dir_entry *bootdevice_dir);
void set_bootdevice_cid(u32 *cid);
void set_bootdevice_csd(u32 *csd);
void set_bootdevice_serial(unsigned int serial);
void set_bootdevice_product_name(char *product_name);
void set_bootdevice_manfid(unsigned int manfid);
void set_bootdevice_oemid(unsigned short oemid);
void set_bootdevice_fwrev(u8 *fwrev);
void set_bootdevice_ife_time_est_typ(u8 life_time_est_typ_a, u8 life_time_est_typ_b);
void set_bootdevice_rev(u8 rev);
void set_bootdevice_pre_eol_info(u8 pre_eol_info);
void set_bootdevice_enhanced_area_offset(unsigned long long enhanced_area_offset);
void set_bootdevice_enhanced_area_size(unsigned int enhanced_area_size);
void set_bootdevice_prv(unsigned char prv);
void set_bootdevice_hwrev(unsigned char hwrev);
void set_bootdevice_ocr(u32	ocr);
void set_bootdevice_erase_size(unsigned int erase_size);
void set_bootdevice_size(unsigned int size);
void set_bootdevice_type(void);
void set_bootdevice_name(const char *name);
void set_bootdevice_ext_csd(u8 *ext_csd);
void set_bootdevice_ffu_support(bool ffu_support);
void tran_mmc_proc_bootdevice_work(struct work_struct *work);
#else
static inline __attribute__((unused)) void
tran_mmc_proc_bootdevice_work(struct work_struct *work
                __attribute__((unused)))
{
        return;
}
EXPORT_SYMBOL(tran_mmc_proc_bootdevice_work);
#endif
#endif
