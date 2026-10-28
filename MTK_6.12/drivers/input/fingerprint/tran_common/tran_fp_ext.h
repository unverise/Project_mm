#ifndef __TRAN_FP_EXT_H__
#define __TRAN_FP_EXT_H__

#include "tran_fp_main.h"

int tran_proc_init(struct tran_fp_data *spidev);
void silfp_work_func(struct work_struct *work);
int cdfinger_customize_input_init(struct tran_fp_data *tran_fp_datap);

int gf_spi_transfer(unsigned char *chip_id_str);
int sf_spi_transfer(unsigned char *chip_id_str);
int cf_spi_transfer(unsigned char *chip_id_str);
int of_spi_transfer(unsigned char *chip_id_str);
int sif_spi_transfer(unsigned char *chip_id_str);
int cdf_spi_transfer(unsigned char *chip_id_str);
int ff_spi_transfer(unsigned char *chip_id_str);
int ftf_spi_transfer(unsigned char *chip_id_str);
int egf_spi_transfer(unsigned char *chip_id_str);
int ancf_spi_transfer(unsigned char *chip_id_str);

int gf_proc_init(struct tran_fp_data *tran_fp_dev);
int sf_proc_init(struct tran_fp_data *tran_fp_dev);
int cf_proc_init(struct tran_fp_data *tran_fp_dev);
int of_proc_init(struct tran_fp_data *tran_fp_dev);
int sif_proc_init(struct tran_fp_data *tran_fp_dev);
int cdf_proc_init(struct tran_fp_data *tran_fp_dev);
int ff_proc_init(struct tran_fp_data *tran_fp_dev);
int ftf_proc_init(struct tran_fp_data *tran_fp_dev);
int egf_proc_init(struct tran_fp_data *tran_fp_dev);
int ancf_proc_init(struct tran_fp_data *tran_fp_dev);

int gf_proc_deinit(struct tran_fp_data *tran_fp_dev);
int sf_proc_deinit(struct tran_fp_data *tran_fp_dev);
int cf_proc_deinit(struct tran_fp_data *tran_fp_dev);
int of_proc_deinit(struct tran_fp_data *tran_fp_dev);
int sif_proc_deinit(struct tran_fp_data *tran_fp_dev);
int cdf_proc_deinit(struct tran_fp_data *tran_fp_dev);
int ff_proc_deinit(struct tran_fp_data *tran_fp_dev);
int ftf_proc_deinit(struct tran_fp_data *tran_fp_dev);
int egf_proc_deinit(struct tran_fp_data *tran_fp_dev);
int ancf_proc_deinit(struct tran_fp_data *tran_fp_dev);

int gf_fp_irq_init(void);
int sf_fp_irq_init(void);
int cf_fp_irq_init(void);
int of_fp_irq_init(void);
int sif_fp_irq_init(void);
int cdf_fp_irq_init(void);
int ff_fp_irq_init(void);
int ftf_fp_irq_init(void);
int egf_fp_irq_init(void);
int ancf_fp_irq_init(void);
#ifdef CONFIG_HAS_EARLYSUSPEND
void gf_early_suspend(struct early_suspend *handler);
void gf_late_resume(struct early_suspend *handler);
void sf_early_suspend(struct early_suspend *handler);
void sf_late_resume(struct early_suspend *handler);
void cf_early_suspend(struct early_suspend *handler);
void cf_late_resume(struct early_suspend *handler);
void of_early_suspend(struct early_suspend *handler);
void of_late_resume(struct early_suspend *handler);
void sif_early_suspend(struct early_suspend *handler);
void sif_late_resume(struct early_suspend *handler);
void cdf_early_suspend(struct early_suspend *handler);
void cdf_late_resume(struct early_suspend *handler);
void ff_early_suspend(struct early_suspend *handler);
void ff_late_resume(struct early_suspend *handler);
void ftf_early_suspend(struct early_suspend *handler);
void ftf_late_resume(struct early_suspend *handler);
void egf_early_suspend(struct early_suspend *handler);
void egf_late_resume(struct early_suspend *handler);
#else
int gf_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
int sf_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
int cf_fb_notifier_callback(struct notifier_block* self, unsigned long event, void* data);
int of_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
int sif_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
int cdf_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
int ff_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
int ftf_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
int egf_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
int ancf_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);

#endif


#ifdef CONFIG_MICROTRUST_TEE_SUPPORT
#define MEMCPY_UUID_FP memcpy(&uuid_fp, &vendor_uuid, sizeof(struct TEEC_UUID));
#else
#define MEMCPY_UUID_FP ;
#endif

#define FP_VENDOR_INIT_AFTER_READ_CHIP_ID(flag,dev_name,i_delay,p_delay,hal_name,uuid_timelow,uuid_timemid,uuid_timehigh_ver,vendor_distinguish_str) \
do  \
{ \
    TRAN_FP_INFO("this is finger chip of " #flag);\
    g_fp_name_flg = flag;\
    tran_fp_datap->miscdev.name = dev_name;\
    tran_fp_datap->irq_init = vendor_distinguish_str##f_fp_irq_init;\
    tran_fp_datap->proc_init = vendor_distinguish_str##f_proc_init;\
    tran_fp_datap->proc_deinit = vendor_distinguish_str##f_proc_deinit;\
    tran_fp_datap->inter_delay = i_delay;\
    tran_fp_datap->post_delay = p_delay;\
    strcpy(tran_fp_datap->modalias, hal_name);\
    vendor_uuid.timeLow = uuid_timelow;\
    vendor_uuid.timeMid = uuid_timemid;\
    vendor_uuid.timeHiAndVersion = uuid_timehigh_ver;\
    memcpy(vendor_uuid.clockSeqAndNode, flag##_clockSeqAndNode, 8);\
    MEMCPY_UUID_FP \
} while(0)

#define FP_VENDOR_INIT_AFTER_READ_CHIP_ID_1(vendor_distinguish_str) \
do  \
{ \
    tran_fp_datap->early_suspend.level = (EARLY_SUSPEND_LEVEL_DISABLE_FB - 1);\
    tran_fp_datap->early_suspend.suspend = vendor_distinguish_str##f_early_suspend;\
    tran_fp_datap->early_suspend.resume = vendor_distinguish_str##f_late_resume;\
} while(0)

#define FP_VENDOR_INIT_AFTER_READ_CHIP_ID_2(vendor_distinguish_str) \
do  \
{ \
    tran_fp_datap->notifier.notifier_call = vendor_distinguish_str##f_fb_notifier_callback;\
} while(0)

#endif
