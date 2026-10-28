#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include "tran-proc-bootdevice.h"
#include "mtk-mmc.h"
#define  EMMC_TYPE      0
#define  UFS_TYPE       1
static struct __bootdevice bootdevice;
struct EMMC_MID_MNAME
{
	unsigned int id;
	char *name;
};
#define EMMC_MANFID_SANDISK              0x2
#define EMMC_MANFID_TOSHIBA              0x11
#define EMMC_MANFID_MICRON               0x13
#define EMMC_MANFID_SAMSUNG              0x15
#define EMMC_MANFID_SANDISK_NEW          0x45
#define EMMC_MANFID_HYNIX                0x90
#define EMMC_MANFID_KSI                  0x70
#define EMMC_MANFID_JBL                  0x88
#define EMMC_MANFID_JBL_N                0xD6
#define EMMC_MANFID_JBL_NEW              ((0x0A<<8)+0xD6)
#define EMMC_MANFID_HOSING               ((0x0B<<8)+0xD6) //BANK INDEX NUMBER
#define EMMC_MANFID_BIWIN                0xF4
#define EMMC_MANFID_BIWIN_N				 0xAB
#define EMMC_MANFID_KEYMOS               0xFE
#define EMMC_MANFID_YMTC                 0x9B
#define EMMC_MANFID_FLASTOR				 0XF86
#define EMMC_MANFID_HIKSEMI              0x2C
#define EMMC_MANFID_TWSC                 0xC4
#define EMMC_MANFID_SCY                  0xDF
struct EMMC_MID_MNAME emmc_manfid_name_tab[] =
{
	{EMMC_MANFID_SANDISK,     "Sandisk"},
	{EMMC_MANFID_TOSHIBA,     "Toshiba"},
	{EMMC_MANFID_MICRON,      "Micron"},
	{EMMC_MANFID_SAMSUNG,     "Samsung"},
	{EMMC_MANFID_SANDISK_NEW, "Sandisk"},
	{EMMC_MANFID_HYNIX,       "Hynix"},
	{EMMC_MANFID_KSI,         "Kingston"},
	{EMMC_MANFID_JBL,         "Jbl"},
	{EMMC_MANFID_JBL_N,       "Jbl"},
	{EMMC_MANFID_JBL_NEW,     "Jbl"},
	{EMMC_MANFID_HOSING,      "Hosing"},
	{EMMC_MANFID_BIWIN,       "Biwin"},
	{EMMC_MANFID_BIWIN_N,	  "Biwin"},
	{EMMC_MANFID_KEYMOS,      "Keymos"},
	{EMMC_MANFID_YMTC,        "Ymtc"},
	{EMMC_MANFID_FLASTOR,	  "Flastor"},
	{EMMC_MANFID_HIKSEMI,     "Hiksemi"},
	{EMMC_MANFID_TWSC,        "Twsc"},
	{EMMC_MANFID_SCY,         "Scy"},
	{0x0, NULL},
};
static char *get_emmc_manfid_name(unsigned int mid)
{
	int i = 0;
	for(;emmc_manfid_name_tab[i].id != 0; i++)
	{
		if(mid == emmc_manfid_name_tab[i].id)
		{
			return emmc_manfid_name_tab[i].name;
		}
	}
	/*No found*/
	return "NOCONFIGEMMCDEV";
}

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


void set_bootdevice_cid(u32 *cid)
{
	memcpy(bootdevice.raw_cid, cid, sizeof(bootdevice.raw_cid));
}
void set_bootdevice_csd(u32 *csd)
{
	memcpy(bootdevice.raw_csd, csd, sizeof(bootdevice.raw_csd));
}
void set_bootdevice_product_name(char *product_name)
{
	strlcpy(bootdevice.product_name,
		product_name,
		sizeof(bootdevice.product_name));
}
void set_bootdevice_manfid(u32 *cid)
{
	unsigned char byte1;
	unsigned char byte2;
	byte1 = (cid[0] >> 24) & 0xff;
	byte2 = (cid[0] >> 16) & 0xff;
	bootdevice.manfid = byte1 + ((byte2 >> 2) << 8);
}
void set_bootdevice_oemid(unsigned short oemid)
{
	bootdevice.oemid = oemid;
}
void set_bootdevice_fwrev(u8 *fwrev)
{
	memcpy(bootdevice.mmc_fwrev, fwrev, MAX_MMC_FIRMWARE_LEN);
}
void set_bootdevice_ife_time_est_typ(u8 life_time_est_typ_a, u8 
life_time_est_typ_b)
{
	bootdevice.life_time_est_typ_a = life_time_est_typ_a;
	bootdevice.life_time_est_typ_b = life_time_est_typ_b;
}
void set_bootdevice_rev(u8 rev)
{
	bootdevice.rev = rev;
}
void set_bootdevice_pre_eol_info(u8 pre_eol_info)
{
	bootdevice.pre_eol_info = pre_eol_info;
}
void set_bootdevice_serial(unsigned int serial)
{
	bootdevice.serial = serial;
}
void set_bootdevice_enhanced_area_offset(unsigned long long 
enhanced_area_offset)
{
	bootdevice.enhanced_area_offset = enhanced_area_offset;
}
void set_bootdevice_enhanced_area_size(unsigned int enhanced_area_size)
{
	bootdevice.enhanced_area_size = enhanced_area_size;
}
void set_bootdevice_prv(unsigned char prv)
{
	bootdevice.prv = prv;
}
void set_bootdevice_hwrev(unsigned char hwrev)
{
	bootdevice.hwrev = hwrev;
}
void set_bootdevice_ocr(u32	ocr)
{
	bootdevice.ocr = ocr;
}
void set_bootdevice_erase_size(unsigned int erase_size)
{
	bootdevice.erase_size = erase_size;
}
void set_bootdevice_size(unsigned int size)
{
	bootdevice.size = (unsigned long)(size * 512);
}
void set_bootdevice_type(void)
{
	if (1)
		bootdevice.type = EMMC_TYPE;
	else
		bootdevice.type = UFS_TYPE;
}
void set_bootdevice_name(const char *name)
{
	bootdevice.name = name;
}
void set_bootdevice_ext_csd(u8 *ext_csd)
{
	int i;
	for (i = 0; i < 512; i++)
		bootdevice.ext_csd[i] = ext_csd[i];
}
void set_bootdevice_ffu_support(bool ffu_support)
{
	bootdevice.ffu_support = ffu_support;
}
void tran_mmc_proc_bootdevice_work(struct work_struct *work)
{
#ifdef CONFIG_TRAN_PROC_MMC	
	struct delayed_work *d = to_delayed_work(work);
	struct msdc_host *host;
	struct mmc_host *mmc;
	struct mmc_card *card;
	host = container_of(d, struct msdc_host, tran_mmc_bootdevice_work);
	if (host == NULL) {
		pr_err("[tran_proc_bootdevice] host is NULL!\n");
		return;
	}
	mmc= mmc_from_priv(host);
	if (mmc == NULL) {
		pr_err("[tran_proc_bootdevice] mmc is NULL\n");
		return;
	}
	card = mmc->card;
	if (card == NULL) {
		pr_err("[tran_proc_bootdevice] card is NULL\n");
		return;
	}
	set_bootdevice_cid(card->raw_cid);
	set_bootdevice_name(mmc->parent->driver->name);
	set_bootdevice_csd(card->raw_csd);
	set_bootdevice_product_name(card->cid.prod_name);
	set_bootdevice_manfid(card->raw_cid);
	set_bootdevice_oemid(card->cid.oemid);
	set_bootdevice_serial(card->cid.serial);
	set_bootdevice_prv(card->cid.prv);
	set_bootdevice_hwrev(card->cid.hwrev);
	set_bootdevice_ocr(card->ocr);
	set_bootdevice_erase_size(card->erase_size << 9);
	set_bootdevice_fwrev(card->ext_csd.fwrev);
	set_bootdevice_ife_time_est_typ(card->ext_csd.device_life_time_est_typ_a,
		card->ext_csd.device_life_time_est_typ_b);
	set_bootdevice_rev(card->ext_csd.rev);
	set_bootdevice_pre_eol_info(card->ext_csd.pre_eol_info);
	set_bootdevice_enhanced_area_offset(card->ext_csd.enhanced_area_offset);
	set_bootdevice_enhanced_area_size(card->ext_csd.enhanced_area_size);
	set_bootdevice_size(((u32)card->ext_csd.raw_sectors[3]<<24)+
		((u32)card->ext_csd.raw_sectors[2]<<16) +
		((u32)card->ext_csd.raw_sectors[1]<<8)+
		((u32)card->ext_csd.raw_sectors[0]));
	set_bootdevice_type();
	set_bootdevice_ffu_support(card->ext_csd.ffu_capable);
#endif
}
EXPORT_SYMBOL(tran_mmc_proc_bootdevice_work);
static int tran_cid_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%08x%08x%08x%08x\n", bootdevice.raw_cid[0], bootdevice.raw_cid[1],
		bootdevice.raw_cid[2], bootdevice.raw_cid[3]);
	return 0;
}
static int tran_cid_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_cid_show, inode->i_private);
}
static const struct proc_ops cid_fops = {
	.proc_open = tran_cid_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_csd_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%08x%08x%08x%08x\n", bootdevice.raw_csd[0], bootdevice.raw_csd[1],
		bootdevice.raw_csd[2], bootdevice.raw_csd[3]);
	return 0;
}
static int tran_csd_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_csd_show, inode->i_private);
}
static const struct proc_ops csd_fops = {
	.proc_open = tran_csd_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_prod_name_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%02X%02X%02X%02X%02X%02X\n", \
			bootdevice.product_name[0], bootdevice.product_name[1],
			bootdevice.product_name[2], bootdevice.product_name[3],
			bootdevice.product_name[4], bootdevice.product_name[5]);
	return 0;
}
static int tran_prod_name_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_prod_name_show, inode->i_private);
}
static const struct proc_ops prod_name_fops = {
	.proc_open = tran_prod_name_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_manfid_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", bootdevice.manfid);
	return 0;
}
static int tran_manfid_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_manfid_show, inode->i_private);
}
static const struct proc_ops manfid_fops = {
	.proc_open = tran_manfid_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_oemid_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%08x\n", bootdevice.oemid);
	return 0;
}
static int tran_oemid_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_oemid_show, inode->i_private);
}
static const struct proc_ops oemid_fops = {
	.proc_open = tran_oemid_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_fwrev_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%*phN\n",MMC_FIRMWARE_LEN,bootdevice.mmc_fwrev);
	return 0;
}
static int tran_fwrev_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_fwrev_show, inode->i_private);
}
static const struct proc_ops fwrev_fops = {
	.proc_open = tran_fwrev_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_life_time_est_typ_a_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%x\n", bootdevice.life_time_est_typ_a);
	return 0;
}
static int tran_life_time_est_typ_a_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_life_time_est_typ_a_show, inode->i_private);
}
static const struct proc_ops life_time_est_typ_a_fops = {
	.proc_open = tran_life_time_est_typ_a_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_life_time_est_typ_b_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%x\n", bootdevice.life_time_est_typ_b);
	return 0;
}
static int tran_life_time_est_typ_b_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_life_time_est_typ_b_show, inode->i_private);
}
static const struct proc_ops life_time_est_typ_b_fops = {
	.proc_open = tran_life_time_est_typ_b_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_rev_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%x\n", bootdevice.rev);
	return 0;
}
static int tran_rev_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_rev_show, inode->i_private);
}
static const struct proc_ops rev_fops = {
	.proc_open = tran_rev_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_pre_eol_info_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%x\n", bootdevice.pre_eol_info);
	return 0;
}
static int tran_pre_eol_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_pre_eol_info_show, inode->i_private);
}
static const struct proc_ops pre_eol_info_fops = {
	.proc_open = tran_pre_eol_info_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_enhanced_area_size_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%x\n", bootdevice.enhanced_area_size);
	return 0;
}
static int tran_enhanced_area_size_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_enhanced_area_size_show, inode->i_private);
}
static const struct proc_ops enhanced_area_size_fops = {
	.proc_open = tran_enhanced_area_size_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_enhanced_area_offset_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%llu\n", bootdevice.enhanced_area_offset);
	return 0;
}
static int tran_enhanced_area_offset_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_enhanced_area_offset_show, inode->i_private);
}
static const struct proc_ops enhanced_area_offset_fops = {
	.proc_open = tran_enhanced_area_offset_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_serial_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%08x\n", bootdevice.serial);
	return 0;
}
static int tran_serial_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_serial_show, inode->i_private);
}
static const struct proc_ops serial_fops = {
	.proc_open = tran_serial_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_hwrev_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", bootdevice.hwrev);
	return 0;
}
static int tran_hwrev_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_hwrev_show, inode->i_private);
}
static const struct proc_ops hwrev_fops = {
	.proc_open = tran_hwrev_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_prv_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", bootdevice.prv);
	return 0;
}
static int tran_prv_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_prv_show, inode->i_private);
}
static const struct proc_ops prv_fops = {
	.proc_open = tran_prv_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_ocr_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%08x\n", bootdevice.ocr);
	return 0;
}
static int tran_ocr_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_ocr_show, inode->i_private);
}
static const struct proc_ops ocr_fops = {
	.proc_open = tran_ocr_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_erase_size_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", bootdevice.erase_size);
	return 0;
}
static int tran_erase_size_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_erase_size_show, inode->i_private);
}
static const struct proc_ops erase_size_fops = {
	.proc_open = tran_erase_size_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_type_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", bootdevice.type == EMMC_TYPE ? "MMC" : "UFS");
	return 0;
}
static int tran_type_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_type_show, inode->i_private);
}
static const struct proc_ops type_fops = {
	.proc_open = tran_type_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_size_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%ldbyte\n", bootdevice.size);
	return 0;
}
static int tran_size_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_size_show, inode->i_private);
}
static const struct proc_ops size_fops = {
	.proc_open = tran_size_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_name_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", bootdevice.name);
	return 0;
}
static int tran_name_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_name_show, inode->i_private);
}
static const struct proc_ops name_fops = {
	.proc_open = tran_name_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_ext_csd_show(struct seq_file *m, void *v)
{
	int i;
	for (i = 0; i < 512; i++)
		seq_printf(m, "%02x", bootdevice.ext_csd[i]);
	return 0;
}
static int tran_ext_csd_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_ext_csd_show, inode->i_private);
}
static const struct proc_ops ext_csd_fops = {
	.proc_open = tran_ext_csd_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_flashId_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s+%02X%02X%02X%02X%02X%02X\n", 
		get_emmc_manfid_name(bootdevice.manfid),
		bootdevice.product_name[0], bootdevice.product_name[1],
		bootdevice.product_name[2],bootdevice.product_name[3],
		bootdevice.product_name[4], bootdevice.product_name[5]);
	return 0;
}
static int tran_flashId_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_flashId_show, inode->i_private);
}
static const struct proc_ops flashId_fops = {
	.proc_open = tran_flashId_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int tran_ffu_support_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", bootdevice.ffu_support);
	return 0;
}
static int tran_ffu_support_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_ffu_support_show, inode->i_private);
}
static const struct proc_ops ffu_support_fops = {
	.proc_open = tran_ffu_support_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#define PROC_MODE 0444
static const struct proc_ops *proc_fops_list[] = {
	&cid_fops,
	&csd_fops,
	&prod_name_fops,
	&manfid_fops,
	&oemid_fops,
	&fwrev_fops,
	&life_time_est_typ_a_fops,
	&life_time_est_typ_b_fops,
	&rev_fops,
	&pre_eol_info_fops,
	&enhanced_area_size_fops,
	&enhanced_area_offset_fops,
	&serial_fops,
	&hwrev_fops,
	&prv_fops,
	&ocr_fops,
	&erase_size_fops,
	&size_fops,
	&type_fops,
	&name_fops,
	&ext_csd_fops,
	&flashId_fops,
	&ffu_support_fops
};
static char *const tran_emmc_node_info[] = {
	"cid",
	"csd",
	"product_name",
	"manfid",
	"oemid",
	"fw_version",
	"life_time_est_typ_a",
	"life_time_est_typ_b",
	"rev",
	"pre_eol_info",
	"enhanced_area_size",
	"enhanced_area_offset",
	"serial",
	"hwrev",
	"prv",
	"ocr",
	"erase_size",
	"size",
	"type",
	"name",
	"ext_csd",
	"flashId",
	"ffu_support"
};
int proc_mmc_bootdevice_create(struct proc_dir_entry *bootdevice_dir)
{
	struct proc_dir_entry *prEntry;
	int i, node;
	node = ARRAY_SIZE(tran_emmc_node_info);
	pr_err("[tran_proc_bootdevice] %s node:%d\n", __func__, node);
	for (i = 0; i < node; i++) {
		prEntry = proc_create(tran_emmc_node_info[i], PROC_MODE, bootdevice_dir, 
			proc_fops_list[i]);
		if (!prEntry) {
			pr_err("[tran_proc_bootdevice] %s,failed to create node: /proc/tran_bootdevice/%s\n",
				__func__, tran_emmc_node_info[i]);
			return -1;
		}
	}
	return 0;
}
