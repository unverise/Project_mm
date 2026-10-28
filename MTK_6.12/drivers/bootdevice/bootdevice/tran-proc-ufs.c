/**
  *Copyright 2022 Transsion
  *Author jinkai.zhang
  *bootdevice info node
*/

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/bitfield.h>
#include <linux/unaligned.h>
#include <linux/nls.h>
#include <ufs/ufshcd.h>
#include <ufs/ufs.h>
#include "ufs-mediatek.h"
#include "tran-proc-ufs.h"
#include "core/ufshcd-priv.h"

#include "tran-proc-bootdevice.h"

struct ufs_hba *g_hba;
static struct __bootdevice bootdevice;
#define WB_DIR                   "tran_bootdevice/feature/write_booster"

#define QUERY_REQ_RETRIES 3
#define UFS_FEATURE_SUPPORT_TW_BIT 0x100

#define UFS_VENDOR_MICRON       0x12C
#define UFS_VENDOR_TOSHIBA      0x198
#define UFS_VENDOR_SAMSUNG      0x1CE
#define UFS_VENDOR_SKHYNIX      0x1AD
#define UFS_VENDOR_YMTC         0xA9B
#define UFS_VENDOR_KSI          0x298
#define UFS_VENDOR_HOSING       0xA6B
#define UFS_VENDOR_HOSING_NEW   0xCD6
#define UFS_VENDOR_JBL          0x1D6
#define UFS_VENDOR_JBL_NEW      0xBD6
#define UFS_VENDOR_PHISON       0xA25
#define UFS_VENDOR_BIWIN_QLC    0xDAB

struct UFS_MID_MNAME
{
	u16 id;
	char *name;
};

static struct __bootdevice bootdevice;

static size_t strlcpy(char *dst, const char *src, size_t size)
{
        size_t len;

        for (len = 0; len < size; len++) {
                dst[len] = src[len];
                if (!dst[len])
                        return len;
        }
        if (size)
                dst[size-1] = '\0';

        while (src[len])
                len++;

        return len;
}

void proc_ufs_get_rawCapacity(struct ufs_hba *hba)
{
	int err;
	size_t buff_len = 8;
	u8 *desc_buf;

	desc_buf = kmalloc(buff_len, GFP_KERNEL);
	if (!desc_buf) {
		dev_err(hba->dev, "[tran_proc_bootdevice] %s: alloc fail!\n", __func__);
		return;
	}

	ufshcd_rpm_get_sync(hba);
	err = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0, GEOMETRY_DESC_PARAM_DEV_CAP,
			desc_buf, buff_len);
	ufshcd_rpm_put_sync(hba);
	if (err) {
		dev_err(hba->dev,
			"[tran_proc_bootdevice] %s: Failed reading Geometry Desc. err = %d\n",
				__func__, err);
		goto out;
	}
	bootdevice.size = get_unaligned_be64(desc_buf);

out:
	kfree(desc_buf);
	return;
}

static int proc_ufs_rawCapacity_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%ldbyte\n", bootdevice.size * 512);
	return 0;
}

static int proc_ufs_rawCapacity_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_rawCapacity_read, NULL);
}

void proc_ufs_set_fw_version(const char * fw_version)
{
	char fwrev[MAX_FIRMWARE_LEN + 1]={0};
	int i;
	size_t fw_version_len;

	if(!fw_version) {
    		pr_err("Invalid firmware version\n");
    		return;
	}

	fw_version_len = strnlen(fw_version, 4);

	if(fw_version_len > MAX_FIRMWARE_LEN / 2) {
    		pr_err("Too long firmware version\n");
    		return;
	}

	for (i = 0; i < fw_version_len; ++i)
		sprintf(fwrev + i * 2, "%02x", (unsigned char)fw_version[i]);

	for (i = fw_version_len * 2; i < MAX_FIRMWARE_LEN; ++i)
		fwrev[i] = '0';

	fwrev[MAX_FIRMWARE_LEN] = '\0';
	strlcpy(bootdevice.fwrev, fwrev, sizeof(bootdevice.fwrev));
}

static int proc_ufs_fw_rev_read(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%s\n", bootdevice.fwrev);
	return 0;
}

static int proc_ufs_fw_rev_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_fw_rev_read, NULL);
}

void proc_ufs_set_manufacturerid(u16 manufacturerid)
{
	bootdevice.manfid = manufacturerid;
}

static int proc_ufs_maufId_read(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", bootdevice.manfid);
	return 0;
}

static int proc_ufs_maufId_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_maufId_read, NULL);
}

void proc_ufs_set_productName(char *productName)
{
	int i = 0;
	size_t product_name_len;

	if (!productName) {
		pr_err("[tran_proc_bootdevice]:productName is NULL!\n");
        	return;
	}

	product_name_len = strnlen(productName, MAX_NAME_LEN);

	for (i = 0; i < product_name_len; i++) {
		if ((productName[i] == '-') || (productName[i] == '.')) {
			productName[i] = '_';
		}
	}

	strlcpy(bootdevice.product_name, productName, sizeof(bootdevice.product_name));
}

static int proc_ufs_productName_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", bootdevice.product_name);
	return 0;
}

static int proc_ufs_productName_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_productName_read, NULL);
}

static char *get_ufs_manfid_name(u16 mid)
{
	int i = 0;
	struct UFS_MID_MNAME ufs_manfid_name_tab[] =
	{
		{UFS_VENDOR_TOSHIBA,  "Toshiba"},
		{UFS_VENDOR_MICRON,   "Micron"},
		{UFS_VENDOR_SAMSUNG,  "Samsung"},
		{UFS_VENDOR_SKHYNIX,  "Hynix"},
		{UFS_VENDOR_YMTC,     "Ymtc"},
		{UFS_VENDOR_KSI,      "Kingston"},
		{UFS_VENDOR_HOSING,   "Hosing"},
		{UFS_VENDOR_HOSING_NEW, "Hosing"},
		{UFS_VENDOR_JBL, "Jbl"},
		{UFS_VENDOR_JBL_NEW, "Jbl"},
		{UFS_VENDOR_PHISON, "Phison"},
		{UFS_VENDOR_KSI, "Kingston"},
		{UFS_VENDOR_BIWIN_QLC, "Biwin"},
		{0x0, NULL},
	};

	for(;ufs_manfid_name_tab[i].id != 0; i++)
	{
		if(mid == ufs_manfid_name_tab[i].id)
		{
			return ufs_manfid_name_tab[i].name;
		}
	}
	/*No found*/
	return "NOCONFIGUFSDEV";
}

static int proc_ufs_flashId_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%s+%s\n", get_ufs_manfid_name(bootdevice.manfid), 
		bootdevice.product_name);
	return 0;
}

static int proc_ufs_flashId_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_flashId_read, NULL);
}

static int proc_bootdevice_type_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", "UFS");
	return 0;
}

static int proc_bootdevice_type_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_bootdevice_type_read, NULL);
}

void proc_ufs_set_manufDate(struct ufs_hba*hba)
{
	u8 desc_buf[8] = {0};
	int ret;
	int param_size = 2;

	if (param_size > 8) {
		pr_err("[tran_proc_bootdevice] %s buffer lengther is too long!,length:%d\n",
			__func__, param_size);
		return;
	}

	ufshcd_rpm_get_sync(hba);
	ret = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_DEVICE, 0,
				DEVICE_DESC_PARAM_MANF_DATE, desc_buf, param_size);
	ufshcd_rpm_put_sync(hba);
	if (ret) {
		pr_err("[tran_proc_bootdevice] %s get manfdate fail,ret:%d!\n", __func__, ret);
		return;
	}
	bootdevice.manfdate = get_unaligned_be16(desc_buf);
}

static int proc_ufs_manufDate_read(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", bootdevice.manfdate);
	return 0;
}

static int proc_ufs_manufDate_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_manufDate_read, NULL);
}

static int proc_ufs_pre_eol_info_read(struct seq_file *m, void *v)
{
	u8 health_dec[32];
	int size = 32;
	int ret = 0;
	struct ufs_hba *hba = g_hba;

	ufshcd_rpm_get_sync(hba);
	ret = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_HEALTH, 0, 0, health_dec, size);
	ufshcd_rpm_put_sync(hba);
	if (ret) {
		dev_err(hba->dev,
			"[tran_proc_bootdevice] %s: get health descirptor fail, ret:%d!\n", __func__, ret);
		return 0;
	}

	seq_printf(m, "0x%x\n", health_dec[2]);

	return 0;
}

static int proc_ufs_pre_eol_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_pre_eol_info_read, NULL);
}

static int proc_ufs_life_time_a_read(struct seq_file *m, void *v)
{
	u8 health_dec[32];
	int size = 32;
	int ret = 0;
	struct ufs_hba *hba = g_hba;

	ufshcd_rpm_get_sync(hba);
	ret = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_HEALTH, 0, 0, health_dec, size);
	ufshcd_rpm_put_sync(hba);
	if (ret) {
		dev_err(hba->dev,
			"[tran_proc_bootdevice] %s: get health descirptor fail, ret:%d!\n", __func__, ret);
		return 0;
	}

	seq_printf(m, "0x%x\n", health_dec[3]);

	return 0;
}

static int proc_ufs_life_time_a_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_life_time_a_read, NULL);
}

static int proc_ufs_life_time_b_read(struct seq_file *m, void *v)
{
	u8 health_dec[32];
	int size = 32;
	int ret = 0;
	struct ufs_hba *hba = g_hba;

	ufshcd_rpm_get_sync(hba);
	ret = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_HEALTH, 0, 0, health_dec, size);
	ufshcd_rpm_put_sync(hba);
	if (ret) {
		dev_err(hba->dev,
			"[tran_proc_bootdevice] %s: get health descirptor fail, ret:%d!\n", __func__, ret);
		return 0;
	}

	seq_printf(m, "0x%x\n", health_dec[4]);

	return 0;
}

static int proc_ufs_life_time_b_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_life_time_b_read, NULL);
}

#define UFS_FEATURE_SUPPORT_FFU_BIT 0x1
void proc_ufs_set_ffu_support(struct ufs_hba*hba)
{
	int ret = 0;
	int desc_len = QUERY_DESC_MAX_SIZE;
	u8 *desc_buf;

	desc_buf = kzalloc(QUERY_DESC_MAX_SIZE, GFP_ATOMIC);
	if (!desc_buf) {
		dev_err(hba->dev, "[tran_proc_bootdevice] %s: alloc buffer fail!\n", __func__);
		return;
	}

	ufshcd_rpm_get_sync(hba);
	ret = ufshcd_query_descriptor_retry(hba,
		UPIU_QUERY_OPCODE_READ_DESC, QUERY_DESC_IDN_DEVICE,
		0, 0, desc_buf, &desc_len);
	ufshcd_rpm_put_sync(hba);
	if (ret) {
		dev_err(hba->dev,
			"[tran_proc_bootdevice]%s: get device descirptor fail, ret:%d!\n", __func__, ret);
		kfree(desc_buf);
		return;
	}
	bootdevice.ffu_support = desc_buf[DEVICE_DESC_PARAM_UFS_FEAT] & UFS_FEATURE_SUPPORT_FFU_BIT;
	kfree(desc_buf);
}

static int proc_ufs_ffu_support_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", bootdevice.ffu_support);
	return 0;
}

static int proc_ufs_ffu_support_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_ffu_support_read, NULL);
}

static const struct proc_ops proc_ufs_capacity_fops=
{
	.proc_open=proc_ufs_rawCapacity_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};

static const struct proc_ops proc_ufs_fw_rev_fops=
{
	.proc_open=proc_ufs_fw_rev_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};

static const struct proc_ops proc_ufs_maufId_fops=
{
	.proc_open=proc_ufs_maufId_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};

static const struct proc_ops proc_ufs_productName_fops=
{
	.proc_open=proc_ufs_productName_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};

static const struct proc_ops proc_ufs_manufDate_fops=
{
	.proc_open=proc_ufs_manufDate_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};

static const struct proc_ops proc_ufs_flashId_fops=
{
	.proc_open=proc_ufs_flashId_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};

static const struct proc_ops proc_bootdevice_type_fops=
{
	.proc_open=proc_bootdevice_type_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};

static const struct proc_ops proc_ufs_pre_eol_info_fops=
{
	.proc_open=proc_ufs_pre_eol_info_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};

static const struct proc_ops proc_ufs_life_time_a_fops=
{
	.proc_open=proc_ufs_life_time_a_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};

static const struct proc_ops proc_ufs_life_time_b_fops=
{
	.proc_open=proc_ufs_life_time_b_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};

static const struct proc_ops proc_ufs_ffu_support_fops=
{
	.proc_open=proc_ufs_ffu_support_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};

#define PROC_MODE 0444
static const struct proc_ops *proc_fops_list[] = {
	&proc_ufs_productName_fops,
	&proc_ufs_maufId_fops,
	&proc_ufs_fw_rev_fops,
	&proc_ufs_life_time_a_fops,
	&proc_ufs_life_time_b_fops,
	&proc_ufs_pre_eol_info_fops,
	&proc_ufs_capacity_fops,
	&proc_bootdevice_type_fops,
	&proc_ufs_flashId_fops,
	&proc_ufs_manufDate_fops,
	&proc_ufs_ffu_support_fops
};
static char *const tran_node_info[] = {
	"product_name",
	"manfid",
	"fw_version",
	"life_time_est_typ_a",
	"life_time_est_typ_b",
	"pre_eol_info",
	"size",
	"type",
	"flashId",
	"manfdate",
	"ffu_support",
};

static int proc_ufs_wbEnable_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_hba->dev_info.wb_enabled);
	return 0;
}

static inline bool ufshcd_is_wb_flags(enum flag_idn idn)
{
	return ((idn >= QUERY_FLAG_IDN_WB_EN) &&
		(idn <= QUERY_FLAG_IDN_WB_BUFF_FLUSH_DURING_HIBERN8));
}

static ssize_t proc_ufs_wbEnable_write(struct file *file, const char *buf, size_t count, loff_t *data)
{
	int ret = 0;
	u8 index = 0;
	unsigned long val;
	enum query_opcode opcode;
	struct ufs_hba *hba = g_hba;
	char cmd_buf[16];

	if (count == 0 || count > 15)
		return -EINVAL;

	if (copy_from_user(cmd_buf, buf, count))
		return -EINVAL;

	cmd_buf[count] = '\0';
	if (kstrtoul(cmd_buf, 15, &val))
		return -EINVAL;

	if (!(val == 0  || val == 1))
		return -EINVAL;

	if (!ufshcd_is_wb_allowed(hba))
		return -EINVAL;

	if (val)
		opcode = UPIU_QUERY_OPCODE_SET_FLAG;
	else
		opcode = UPIU_QUERY_OPCODE_CLEAR_FLAG;

	dev_err(hba->dev, "val: %lu, opcode:%d\n", val, opcode);
	index = ufshcd_wb_get_query_index(hba);
	
	ufshcd_rpm_get_sync(hba);
	ret = ufshcd_query_flag_retry(hba, opcode, QUERY_FLAG_IDN_WB_EN, index, NULL);
	ufshcd_rpm_put_sync(hba);
	if (ret) {
		dev_err(hba->dev, "%s WB - buf enable/disable failed %d\n", __func__, ret);
		return -EINVAL;
	}

	if (val)
		hba->dev_info.wb_enabled = true;
	else
		hba->dev_info.wb_enabled = false;

	dev_err(hba->dev, "WB enable/disabled: %d\n", ret);

	return count;
}


static int wb_flush_en_proc_show(struct seq_file *m, void *v)
{
	bool flag;
	u8 index = 0;
	int ret = 0;
	struct ufs_hba *hba = g_hba;

	if (ufshcd_is_wb_flags(QUERY_FLAG_IDN_WB_BUFF_FLUSH_EN))
		index = ufshcd_wb_get_query_index(hba);

	ufshcd_rpm_get_sync(hba);
	ret = ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG,
		QUERY_FLAG_IDN_WB_BUFF_FLUSH_EN, index, &flag);
	ufshcd_rpm_put_sync(hba);
	if (ret) {
		return -EINVAL;
	}
	seq_printf(m, "%s\n", flag ? "true" : "false");

	return 0;
}

static int wb_flush_en_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, wb_flush_en_proc_show, inode->i_private);
}

static ssize_t wb_flush_en_proc_write(struct file *file, const char *buf,
	size_t count, loff_t *data)
{
	int ret = 0;
	u8 index = 0;
	unsigned long val;
	enum query_opcode opcode;
	struct ufs_hba *hba = g_hba;
	char cmd_buf[16];

	if (count == 0 || count > 15)
		return -EINVAL;

	if (copy_from_user(cmd_buf, buf, count))
		return -EINVAL;

	cmd_buf[count] = '\0';
	if (kstrtoul(cmd_buf, 15, &val))
		return -EINVAL;

	if (!(val == 0  || val == 1))
		return -EINVAL;

	if (!ufshcd_is_wb_allowed(hba))
		return -EINVAL;

	if (val)
		opcode = UPIU_QUERY_OPCODE_SET_FLAG;
	else
		opcode = UPIU_QUERY_OPCODE_CLEAR_FLAG;

	index = ufshcd_wb_get_query_index(hba);

	ufshcd_rpm_get_sync(hba);
	ret = ufshcd_query_flag_retry(hba, opcode, QUERY_FLAG_IDN_WB_BUFF_FLUSH_EN,
		index, NULL);
	ufshcd_rpm_put_sync(hba);
	if (ret) {
		dev_err(hba->dev, "%s WB - buf flush enable/disable failed %d\n", __func__, ret);
		return -EINVAL;
	}

	if (val)
		hba->dev_info.wb_buf_flush_enabled = true;
	else
		hba->dev_info.wb_buf_flush_enabled = false;

	dev_dbg(hba->dev, "WB - Flush enable/disabled: %d\n", ret);

	return count;
}

static int wb_flush_during_h8_proc_show(struct seq_file *m, void *v)
{
	bool flag;
	u8 index = 0;
	int ret = 0;
	struct ufs_hba *hba = g_hba;

	if (ufshcd_is_wb_flags(QUERY_FLAG_IDN_WB_BUFF_FLUSH_DURING_HIBERN8))
		index = ufshcd_wb_get_query_index(hba);

	ufshcd_rpm_get_sync(hba);
	ret = ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG,
		QUERY_FLAG_IDN_WB_BUFF_FLUSH_DURING_HIBERN8, index, &flag);
	ufshcd_rpm_put_sync(hba);
	if (ret) {
		return -EINVAL;
	}
	seq_printf(m, "%s\n", flag ? "true" : "false");

	return 0;
}

static int wb_flush_during_h8_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, wb_flush_during_h8_proc_show, inode->i_private);
}

static ssize_t wb_flush_during_h8_proc_write(struct file *file, const char *buf,
	size_t count, loff_t *data)
{
	int ret = 0;
	u8 index = 0;
	unsigned long val;
	enum query_opcode opcode;
	struct ufs_hba *hba = g_hba;
	char cmd_buf[16];

	if (count == 0 || count > 15)
		return -EINVAL;

	if (copy_from_user(cmd_buf, buf, count))
		return -EINVAL;

	cmd_buf[count] = '\0';
	if (kstrtoul(cmd_buf, 15, &val))
		return -EINVAL;

	if (!(val == 0  || val == 1))
		return -EINVAL;

	if (!ufshcd_is_wb_allowed(hba))
		return -EINVAL;

	if (val)
		opcode = UPIU_QUERY_OPCODE_SET_FLAG;
	else
		opcode = UPIU_QUERY_OPCODE_CLEAR_FLAG;

	index = ufshcd_wb_get_query_index(hba);

	ufshcd_rpm_get_sync(hba);
	ret = ufshcd_query_flag_retry(hba, opcode,
		QUERY_FLAG_IDN_WB_BUFF_FLUSH_DURING_HIBERN8, index, NULL);
	ufshcd_rpm_put_sync(hba);
	if (ret) {
		dev_err(hba->dev, "%s WB - buf flush during h8 enable/disable failed %d\n",
			__func__, ret);
		return -EINVAL;
	}
	
	if (val)
		hba->dev_info.wb_buf_flush_enabled = true;
	else
		hba->dev_info.wb_buf_flush_enabled = false;

	dev_dbg(hba->dev, "WB - Flush during h8 enable/disabled: %d\n", ret);

	return count;
}

static int proc_ufs_wbEnable_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_wbEnable_read, NULL);
}

static int proc_ufs_wbAvailable_buffer_read(struct seq_file *m, void *v)
{
	int available_buffer_size = 0;
	int lun = 2;
	int err = 0;
	int selector = 0;
	struct ufs_hba *hba = g_hba;

	lun = ufshcd_wb_get_query_index(hba);

	ufshcd_rpm_get_sync(hba);
	err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, 
		QUERY_ATTR_IDN_AVAIL_WB_BUFF_SIZE,
		(u8)lun, selector, &available_buffer_size);
	ufshcd_rpm_put_sync(hba);
	if (err) {
		pr_err("%s get available_buffer_size fail:%d\n", __func__, err);
		return 0;
	}

	seq_printf(m, "0x%x\n", available_buffer_size);

	return 0;
}

static int proc_ufs_wbAvailable_buffer_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_wbAvailable_buffer_read, NULL);
}

static int proc_ufs_wbCurrent_buffer_read(struct seq_file *m, void *v)
{
	int wbCurrent_buffer = 0;
	int lun = 2;
	int err = 0;
	int selector = 0;
	struct ufs_hba *hba = g_hba;

	lun = ufshcd_wb_get_query_index(hba);

	ufshcd_rpm_get_sync(hba);
	err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, 
		QUERY_ATTR_IDN_CURR_WB_BUFF_SIZE,
		(u8)lun, selector, &wbCurrent_buffer);
	ufshcd_rpm_put_sync(hba);
	if (err) {
		pr_err("%s get wbCurrent_buffer fail:%d\n", __func__, err);
		return 0;
	}
	seq_printf(m, "0x%x\n", wbCurrent_buffer);

	return 0;
}

static int proc_ufs_wbCurrent_buffer_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ufs_wbCurrent_buffer_read, NULL);
}

static const struct proc_ops proc_ufs_wbEnable_fops=
{
	.proc_open=proc_ufs_wbEnable_open,
	.proc_write = proc_ufs_wbEnable_write,
	.proc_read=seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops proc_ufs_wbCurrent_buffer_fops=
{
	.proc_open=proc_ufs_wbCurrent_buffer_open,
	.proc_read=seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops proc_ufs_wbAvailable_buffer_fops=
{
	.proc_open=proc_ufs_wbAvailable_buffer_open,
	.proc_read=seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops proc_ufs_wb_flush_en_proc_fops = {
	.proc_open = wb_flush_en_proc_open,
	.proc_write = wb_flush_en_proc_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};


static const struct proc_ops proc_ufs_wb_flush_during_h8_proc_fops = {
	.proc_open = wb_flush_during_h8_proc_open,
	.proc_write = wb_flush_during_h8_proc_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

void proc_ufs_feature_create(struct proc_dir_entry *feature_dir)
{
	static struct proc_dir_entry *wb_proc_entry = NULL;
	wb_proc_entry = proc_mkdir(WB_DIR, NULL);
	if (wb_proc_entry == NULL)
	{
			pr_err("Error: %s Could not initialize /proc/%s\n", __func__, WB_DIR);
			return;
	}
	proc_create("wb_enable", 0666, wb_proc_entry,
			&proc_ufs_wbEnable_fops);
	proc_create("wb_current_buffer", S_IALLUGO, wb_proc_entry,
			&proc_ufs_wbCurrent_buffer_fops);
	proc_create("wb_available_buffer", S_IALLUGO, wb_proc_entry,
			&proc_ufs_wbAvailable_buffer_fops);
	proc_create("wb_flush_en", 0666, wb_proc_entry,
			&proc_ufs_wb_flush_en_proc_fops);
	proc_create("wb_flush_during_h8", 0666, wb_proc_entry,
			&proc_ufs_wb_flush_during_h8_proc_fops);
}


void tran_ufs_proc_bootdevice_work(struct work_struct *work)
{
#ifdef CONFIG_TRAN_PROC_UFS	
	struct delayed_work *d = to_delayed_work(work);
	struct ufs_mtk_host *host;
	host = container_of(d, struct ufs_mtk_host, proc_bootdevice_work);

	if (host == NULL){
		pr_err("[tran_proc_bootdevice] host is NULL!\n");
		return;
	}

	g_hba = host->hba;
	if (g_hba == NULL){
		pr_err("[tran_proc_bootdevice] g_hba is NULL!\n");
		return;
	}

	if (host->hba->ufs_device_wlun == NULL) {
		pr_err("[tran_proc_bootdevice] host->hba->ufs_device_wlun is NULL!\n");
		return;
	}

	proc_ufs_get_rawCapacity(host->hba);
	proc_ufs_set_manufacturerid(host->hba->dev_info.wmanufacturerid);
	proc_ufs_set_productName(host->hba->dev_info.model);
	proc_ufs_set_manufDate(host->hba);
	proc_ufs_set_fw_version(host->hba->ufs_device_wlun->rev);
	proc_ufs_set_ffu_support(host->hba);
#endif
}
EXPORT_SYMBOL(tran_ufs_proc_bootdevice_work);

void proc_ufs_bootdevice_create(struct proc_dir_entry *bootdevice_dir)
{
	struct proc_dir_entry *prEntry;
	int i, node;

	node = ARRAY_SIZE(tran_node_info);
	for (i = 0; i < node; i++) {
		prEntry = proc_create(tran_node_info[i], PROC_MODE, bootdevice_dir, proc_fops_list[i]);
		if (!prEntry) {
			pr_err("[tran_proc_bootdevice] %s,failed to create node: /proc/bootdevice/%s\n",
				__func__, tran_node_info[i]);
			return;
		}
	}
}
