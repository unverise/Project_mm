#include <linux/fs.h>
#include <linux/hash.h>
#include <linux/memcontrol.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/namei.h>
#include <linux/pagemap.h>
#include <linux/path.h>
#include <linux/proc_fs.h>
#include <linux/rwlock.h>
#include <linux/seq_file.h>

#include "pf_common.h"
#include "inodes_protect.h"

#define TRANMM "[tranmem_pf_inodes]"
#define MAX_BUF_LEN 16
#define MAX_HASH_BUCKETS 8
#define HASH_BITS  ilog2(MAX_HASH_BUCKETS)

static const char *default_files[] = {
	"/system/bin/surfaceflinger",
	"/vendor/lib64/egl/libGLES_mali.so",
	"/system/framework/services.jar",
	"/system/framework/framework.jar",
};

static rwlock_t inode_protect_rwlocks[MAX_HASH_BUCKETS];
static struct list_head inode_protects[MAX_HASH_BUCKETS];

#define OVERLAYFS_PATH_REAL_SYMBOL_NAME   "ovl_i_path_real"
typedef struct inode *(*ovl_i_path_real_t)(struct inode *inode, struct path *path);
static ovl_i_path_real_t ovl_i_path_real_dup = NULL;

static atomic_t g_ip_enabled = ATOMIC_INIT(0);

static bool g_ip_debug = false;

static inline bool is_inodes_protect_enabled(void)
{
	return atomic_read(&g_ip_enabled) == 1;
}

static inline unsigned int inode_hash_bucket(unsigned long ino)
{
	return hash_long(ino, HASH_BITS);
}

static inode_protect_t* is_inode_in_list(unsigned long ino, dev_t s_dev)
{
	unsigned long flags;
	inode_protect_t *entry = NULL;
	bool found = false;
	unsigned int bucket = inode_hash_bucket(ino);

	read_lock_irqsave(&inode_protect_rwlocks[bucket], flags);
	if (!list_empty(&inode_protects[bucket])) {
		list_for_each_entry(entry, &inode_protects[bucket], list) {
			if (entry->ino != ino)
				continue;

			if (entry->s_dev == s_dev) {
				found = true;
				if (unlikely(g_ip_debug))
					pr_info(TRANMM "%s: find ino(%lu) dev(0x%x)\n", __func__, ino, s_dev);
				break;
			} else {
				if (unlikely(g_ip_debug))
					pr_info(TRANMM "%s: just different dev, ino(%lu) dev(0x%x:0x%x)\n", __func__, ino, entry->s_dev, s_dev);
			}
		}
	}

	read_unlock_irqrestore(&inode_protect_rwlocks[bucket], flags);

	return found ? entry : NULL;
}

/**
 * Add the inode to the protected list.
 * The file path must have exist when invoke this function.
 * @param path: The path of the file.
 * @param ino: The inode number.
 * @param s_dev: The device number.
 * @return: True if the inode is added to the protected list, false otherwise.
 */
static bool add_2_inode_list(const char* path, const unsigned long ino, const dev_t s_dev)
{
	inode_protect_t *entry = NULL;
	unsigned long flags;
	unsigned int bucket = inode_hash_bucket(ino);

	if (is_inode_in_list(ino, s_dev)) {
		if (unlikely(g_ip_debug))
			pr_info(TRANMM "%s: ino(%lu) has been added in list.\n", __func__, ino);
		return false;
	}

	entry = kmalloc(sizeof(inode_protect_t), GFP_KERNEL);
	if (!entry) {
		pr_err(TRANMM "kmalloc failed for inode entry\n");
		return false;
	}

	memset(entry, 0, sizeof(inode_protect_t));
	snprintf(entry->file_name, MAX_LEN_FILE_PATH, "%s", path);
	entry->ino = ino;
	entry->s_dev = s_dev;
	atomic64_set(&entry->nr_protected, 0);

	write_lock_irqsave(&inode_protect_rwlocks[bucket], flags);
	list_add_tail(&entry->list, &inode_protects[bucket]);
	write_unlock_irqrestore(&inode_protect_rwlocks[bucket], flags);

	if (unlikely(g_ip_debug))
		pr_info(TRANMM "%s: add success for %s, ino(%lu), dev(0x%x).\n", __func__, path, ino, s_dev);

	return true;
}

static void del_from_inode_list(const unsigned long ino, const dev_t s_dev)
{
	inode_protect_t *entry = NULL, *next_entry = NULL, *found_entry = NULL;
	unsigned long flags;
	unsigned int bucket = inode_hash_bucket(ino);

	write_lock_irqsave(&inode_protect_rwlocks[bucket], flags);
	list_for_each_entry_safe(entry, next_entry, &inode_protects[bucket], list) {
		if (entry->ino == ino && entry->s_dev == s_dev) {
			list_del(&entry->list);
			found_entry = entry;
			break;
		}
	}
	write_unlock_irqrestore(&inode_protect_rwlocks[bucket], flags);

	if (found_entry) {
		kfree(found_entry);

		if (unlikely(g_ip_debug))
			pr_info(TRANMM "%s: delete success for ino(%lu), dev(0x%x).\n", __func__, ino, s_dev);
	} else {
		if (unlikely(g_ip_debug))
			pr_info(TRANMM "%s: don't find in list. ino(%lu), dev(0x%x).\n", __func__, ino, s_dev);
	}
}

static void del_all_inode_list(void)
{
	inode_protect_t *entry, *entry_tmp;
	unsigned long flags;
	int i;

	for (i = 0; i < MAX_HASH_BUCKETS; i++) {
		write_lock_irqsave(&inode_protect_rwlocks[i], flags);

		list_for_each_entry_safe(entry, entry_tmp, &inode_protects[i], list) {
			list_del(&entry->list);
			kfree(entry);
		}

		write_unlock_irqrestore(&inode_protect_rwlocks[i], flags);
	}

	if (unlikely(g_ip_debug))
		pr_info(TRANMM "%s: delete all inodes from list.\n", __func__);
}

static bool is_overlayfs(struct super_block *i_sb)
{
	if (!i_sb || !i_sb->s_type || !i_sb->s_type->name)
		return false;

	if (unlikely(g_ip_debug))
		pr_info(TRANMM "%s: s_type->name: %s\n", __func__, i_sb->s_type->name);

	if (!strcmp(i_sb->s_type->name, "overlay"))
		return true;

	return false;
}

static int get_inode_dev_from_path(const char *path_name, inode_protect_t* entry)
{
	struct path path, real_path;
	struct inode *inode = NULL;
	int ret = 0;

	if (unlikely(g_ip_debug))
		pr_info(TRANMM "get_inode_dev_from_path: %s\n", path_name);

	if (!path_name || !entry) {
		pr_err(TRANMM "%s: invalid path_name or entry\n", __func__);
		return -EINVAL;
	}

	if (strlen(path_name) >= MAX_LEN_FILE_PATH) {
		pr_err(TRANMM "%s: path_name is too long: %s\n", __func__, path_name);
		return -EINVAL;
	}

	ret = kern_path(path_name, LOOKUP_FOLLOW, &path);
	if (ret) {
		pr_err(TRANMM "%s: can't parse path:%s, error_no:%d\n", __func__, path_name, ret);
		return ret;
	}

	inode = d_inode(path.dentry);
	if (!inode || !inode->i_sb) {
		pr_err(TRANMM "%s: can't parse inode for path: %s\n", __func__, path_name);
		ret = -ENOENT;
		goto out;
	}

	if (is_overlayfs(inode->i_sb)) {
		if (!ovl_i_path_real_dup) {
			pr_err(TRANMM "%s: ovl_i_path_real_dup is NULL\n", __func__);
			ret = -EINVAL;
			goto out;
		}

		inode = ovl_i_path_real_dup(inode, &real_path);
		if (!inode || !inode->i_sb) {
			pr_err(TRANMM "%s: can't parse inode for real path.\n", __func__);
			ret = -ENOENT;
			goto out;
		}
	}

	entry->ino = inode->i_ino;
	entry->s_dev = inode->i_sb->s_dev;

	if (unlikely(g_ip_debug)) {
		pr_info(TRANMM "parse successfully for path: %s\n", path_name);
		pr_info(TRANMM "inode: %lu, dev: %u:%u(0x%x)\n",
			entry->ino,
			MAJOR(entry->s_dev),
			MINOR(entry->s_dev),
			entry->s_dev);
	}

out:
	path_put(&path);
	return ret;
}

static inode_protect_t* get_protect_inode(struct folio *folio)
{
	struct inode *f_inode = NULL;
	unsigned long ino = 0;
	dev_t f_s_dev = 0;
	struct address_space *mapping = NULL;
	struct mem_cgroup *memcg = NULL;

	memcg = folio_memcg(folio);
	mapping = folio_mapping(folio);
	f_inode = mapping ? mapping->host : NULL;

	if (!is_valid_memcg(memcg) || !mapping || !f_inode || !f_inode->i_sb)
		return NULL;

	ino = f_inode->i_ino;
	f_s_dev = f_inode->i_sb->s_dev;

	return is_inode_in_list(ino, f_s_dev);
}

static void add_default_inodes(void)
{
	int i;
	inode_protect_t entry = {0};
	int ret;

	for (i = 0; i < ARRAY_SIZE(default_files); i++) {
		const char *path = default_files[i];

		if (!path || *path == '\0')
			continue;

		pr_info(TRANMM "Adding default file to protection list: %s\n", path);

		ret = get_inode_dev_from_path(path, &entry);
		if (ret) {
			pr_err(TRANMM "Failed to get inode information for path: %s, error: %d\n", path, ret);
			continue;
		}

		if (!add_2_inode_list(path, entry.ino, entry.s_dev)) {
			pr_err(TRANMM "Failed to add file to protection list: %s\n", path);
			continue;
		}

		pr_info(TRANMM "Successfully added default file to protection list: %s\n", path);
	}
}

static bool inodes_protect_enabled_change(bool enabled)
{
	bool old = is_inodes_protect_enabled();
	if (enabled == old)
		return old;

	if (enabled) {
		add_default_inodes();
		atomic_set(&g_ip_enabled, 1);
	} else {
		atomic_set(&g_ip_enabled, 0);
		del_all_inode_list();
	}

	pr_info(TRANMM "%s: inodes protect enabled changed from %d to %d\n", __func__, old, enabled);

	return old;
}

/**************************Files ops START *********************************/
static ssize_t inodes_debug_write(struct file *file,
			const char __user *ubuf, size_t len, loff_t *ppos)
{
	int ret;
	char kbuf[MAX_BUF_LEN] = {0};
	char *str;
	int value;

	if (len > MAX_BUF_LEN - 1) {
		return -EINVAL;
	}

	if (copy_from_user(kbuf, ubuf, len))
		return -EFAULT;

	kbuf[len] = '\0';

	str = strstrip(kbuf);
	if (!str) {
		return -EINVAL;
	}

	ret = kstrtoint(str, 10, &value);
	if (ret) {
		return -EINVAL;
	}

	g_ip_debug = (value > 0);

	pr_info(TRANMM "inodes_debug_write: debug mode set to %d\n", value);

	return len;
}

static int inodes_debug_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_ip_debug);
	return 0;
}

static int inodes_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, inodes_debug_show, NULL);
}

static const struct proc_ops inodes_debug_fops = {
	.proc_open = inodes_debug_open,
	.proc_read = seq_read,
	.proc_write = inodes_debug_write,
	.proc_release = single_release,
};

static ssize_t inodes_files_write(struct file *file,
			const char __user *ubuf, size_t len, loff_t *ppos)
{
	char kbuf[MAX_LEN_FILE_PATH] = {0};
	char *input = NULL;
	inode_protect_t entry = {0};
	unsigned long ino;
	unsigned int s_dev;
	int ret = 0;

	if (unlikely(!is_inodes_protect_enabled())) {
		pr_err(TRANMM "%s: inodes_protect is disabled\n", __func__);
		return -EINVAL;
	}

	if (len > MAX_LEN_FILE_PATH - 1) {
		pr_err(TRANMM "%s: Input length exceeds maximum allowed (%d)\n", __func__, MAX_LEN_FILE_PATH - 1);
		return -EINVAL;
	}

	if (copy_from_user(kbuf, ubuf, len)) {
		pr_err(TRANMM "%s: Failed to copy input from user space\n", __func__);
		return -EFAULT;
	}

	kbuf[len] = '\0';

	input = strstrip(kbuf);
	if (!input || *input == '\0') {
		pr_err(TRANMM "%s: Empty input provided\n", __func__);
		return -EINVAL;
	}

	/**
	 * Check the input format, must be:
	 *   1. del:<ino>,<dev>     del inode from inode_protects;
	 *   2. add:<path>          add file to inode_protects;
	 */
	pr_info(TRANMM "string input: %s\n", input);
	if (strncmp(input, "del:", 4) == 0) {
		if (sscanf(input + 4, "%lu,%u", &ino, &s_dev) != 2) {
			pr_err(TRANMM "Invalid inode format. Expected: del:<ino>,<dev>\n");
			return -EINVAL;
		}

		del_from_inode_list(ino, (dev_t)s_dev);
	} else if (strncmp(input, "add:", 4) == 0) {
		char *path = input + 4;
		ret = get_inode_dev_from_path(path, &entry);
		if (ret) {
			pr_err(TRANMM "Failed to get inode information for path: %s\n", path);
			return ret;
		}

		if (!add_2_inode_list(path, entry.ino, entry.s_dev)) {
			pr_err(TRANMM "Failed to add file to protection list: %s\n", path);
			return -EEXIST;
		}

		pr_info(TRANMM "Successfully added file to protection list: %s\n", path);
	}

	return len;
}

static int inodes_files_show(struct seq_file *m, void *v)
{
	inode_protect_t *entry = NULL;
	unsigned long flags;
	int i;

	if (unlikely(!is_inodes_protect_enabled()))
		return 0;

	seq_printf(m, "All files have been protected:\n");
	seq_printf(m, "    %-20s %-15s %-20s %s\n", "Inode Number", "Device", "Protect Cnt", "File Name");
	seq_printf(m, "    %-20s %-15s %-20s %s\n", "------------", "------", "-----------", "-----------");

	for (i = 0; i < MAX_HASH_BUCKETS; i++) {
		read_lock_irqsave(&inode_protect_rwlocks[i], flags);

		list_for_each_entry(entry, &inode_protects[i], list) {
			seq_printf(m, "[%d] %-20lu %3u:%-11u %-20llu %s\n", i,
				entry->ino,
				MAJOR(entry->s_dev),
				MINOR(entry->s_dev),
				atomic64_read(&entry->nr_protected),
				entry->file_name);
		}

		read_unlock_irqrestore(&inode_protect_rwlocks[i], flags);
	}

	return 0;
}

static int inodes_files_open(struct inode *inode, struct file *file)
{
	return single_open(file, inodes_files_show, NULL);
}

static const struct proc_ops inodes_files_fops = {
	.proc_open = inodes_files_open,
	.proc_read = seq_read,
	.proc_write = inodes_files_write,
	.proc_release = single_release,
};

static ssize_t inodes_enabled_write(struct file *file,
			const char __user *ubuf, size_t len, loff_t *ppos)
{
	char kbuf[MAX_BUF_LEN] = {0};
	char *str;
	int value;
	int ret;

	if (len > MAX_BUF_LEN - 1) {
		pr_err(TRANMM "Buffer length exceeds maximum allowed (%d)\n", MAX_BUF_LEN - 1);
		return -EINVAL;
	}

	if (copy_from_user(kbuf, ubuf, len)) {
		pr_err(TRANMM "Failed to copy buffer from user space\n");
		return -EFAULT;
	}

	kbuf[len] = '\0';
	str = strstrip(kbuf);
	if (!str) {
		pr_err(TRANMM "Empty buffer provided\n");
		return -EINVAL;
	}

	ret = kstrtoint(str, 10, &value);
	if (ret) {
		pr_err(TRANMM "Failed to convert string to integer\n");
		return -EINVAL;
	}

	inodes_protect_enabled_change(value > 0);
	return len;
}

static int inodes_enabled_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", is_inodes_protect_enabled());
	return 0;
}

static int inodes_enabled_open(struct inode *inode, struct file *file)
{
	return single_open(file, inodes_enabled_show, NULL);
}

static const struct proc_ops inodes_enabled_fops = {
	.proc_open = inodes_enabled_open,
	.proc_read = seq_read,
	.proc_write = inodes_enabled_write,
	.proc_release = single_release,
};

/**************************Files ops END ***********************************/

/**************************Cloud START***********************************/
bool update_inodes_protect_enabled(bool enabled)
{
	return inodes_protect_enabled_change(enabled);
}
/**************************Cloud END*************************************/

void inodes_protect_proc_create(struct proc_dir_entry* parent)
{
	proc_create("inodes_protect_debug",    0666, parent, &inodes_debug_fops);
	proc_create("inodes_protect_enabled",  0666, parent, &inodes_enabled_fops);
	proc_create("inodes_protect_files",    0666, parent, &inodes_files_fops);
}

bool folio_protected_by_inode(struct folio *folio)
{
	inode_protect_t *entry = NULL;

	if (unlikely(!is_inodes_protect_enabled()))
		return false;

	entry = get_protect_inode(folio);
	if (entry)
		atomic64_inc(&entry->nr_protected);

	return entry != NULL;
}

int inodes_protect_init(void)
{
	int i;
	unsigned long address;
	pr_info(TRANMM "inodes_protect_init\n");

	for (i = 0; i < MAX_HASH_BUCKETS; i++) {
		rwlock_init(&inode_protect_rwlocks[i]);
		INIT_LIST_HEAD(&inode_protects[i]);
	}

	if (get_symbol_address(OVERLAYFS_PATH_REAL_SYMBOL_NAME, &address))
		ovl_i_path_real_dup = (ovl_i_path_real_t)address;

	return 0;
}

void inodes_protect_exit(void)
{
	del_all_inode_list();
}

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
