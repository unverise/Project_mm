/**
  *Copyright 2022 Transsion inc
  *Author jinkai.zhang
  *bootdevice info node
*/

#include <linux/proc_fs.h>
#include <linux/proc_ns.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/setup.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/devfreq.h>

#ifdef CONFIG_TRAN_PROC_UFS
#include "tran-proc-ufs.h"
#endif
#ifdef CONFIG_TRAN_PROC_MMC
#include "tran-proc-mmc.h"
#endif
#include "tran-proc-bootdevice.h"
#include "ufs-mediatek.h"

#define BOOTDEVICE_DIR "tran_bootdevice"
#define UFS_FEATURE_DIR "tran_bootdevice/feature"

#define NORMAL_BOOT_MODE 0

static unsigned int get_boot_mode_from_dts(void)
{
	struct device_node *np_chosen = NULL;
	struct tag_bootmode *tag = NULL;

	np_chosen = of_find_node_by_path("/chosen");
	if (!np_chosen) {
		pr_notice("[tran_proc_bootdevice]: warning: not find node: '/chosen'\n");

		np_chosen = of_find_node_by_path("/chosen@0");
		if (!np_chosen) {
			pr_notice("[tran_proc_bootdevice]: warning: not find node: '/chosen@0'\n");
				return NORMAL_BOOT_MODE;
		}
	}

	tag = (struct tag_bootmode *)
			of_get_property(np_chosen, "atag,boot", NULL);
	if (!tag) {
		pr_notice("[tran_proc_bootdevice]: error: not find tag: 'atag,boot';\n");
		return NORMAL_BOOT_MODE;
	}

	pr_notice("[tran_proc_bootdevice]: bootmode: 0x%x boottype: 0x%x.\n",tag->bootmode, tag->boottype);

	return tag->boottype;
}

static int __init tran_create_bootdevice_init(void)
{
	int err = 0;
	int type = 0;

	struct proc_dir_entry *bootdevice_dir;
	bootdevice_dir = proc_mkdir(BOOTDEVICE_DIR, NULL);
	if (!bootdevice_dir) {
		pr_err("[tran_proc_bootdevice] %s: failed to create /proc/%s\n", __func__, BOOTDEVICE_DIR);
		return -1;
	}
	type = get_boot_mode_from_dts();
  	if (type == BOOTDEV_UFS) {
#ifdef CONFIG_TRAN_PROC_UFS
                struct proc_dir_entry *feature_dir;
                feature_dir = proc_mkdir(UFS_FEATURE_DIR, NULL);
                if (!feature_dir) {
                        pr_err("%s: failed to create /proc/%s\n", __func__, UFS_FEATURE_DIR);
                        err = -1;
                }
                proc_ufs_feature_create(feature_dir);

                pr_err("[tran_proc_bootdevice] %s:proc_ufs_bootdevice_create\n", __func__);
                proc_ufs_bootdevice_create(bootdevice_dir);
#endif
	} else if (type == BOOTDEV_SDMMC) {
#ifdef CONFIG_TRAN_PROC_MMC
		pr_err("[tran_proc_bootdevice] %s:proc_mmc_bootdevice_create\n", __func__);
		proc_mmc_bootdevice_create(bootdevice_dir);
#endif
	} else {
		pr_err("[tran_proc_bootdevice] %s: failed to create /proc/%s, not find bootdevice\n",
			__func__, BOOTDEVICE_DIR);
		err = 0;
	}
	return err;
}

static void __exit tran_create_bootdevice_exit(void)
{
	return;
}

module_init(tran_create_bootdevice_init)
module_exit(tran_create_bootdevice_exit)
MODULE_DESCRIPTION("TRAN Unifed Node Driver");
MODULE_AUTHOR("transsion");
MODULE_LICENSE("GPL v2");
