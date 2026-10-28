#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/memcontrol.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/file.h>
#include <linux/cgroup.h>

#include "../zram_drv.h"
#include "eswap_schedule.h"
#include "eswap_area.h"
#include "eswap_internal.h"

/* static page pool for page fault scenario:
 * reserve 16 pages, to make eswap fault in easier ...
 * 8 * 16 * 4 = 0.5M
 */
#define MAX_PF_POOL_PAGES	(8 * EXTENT_PG_CNT)
static LIST_HEAD(pf_page_pool_list);
static DEFINE_SPINLOCK(pf_page_pool_lock);
static int pf_pool_pages = 0;

struct zs_ext_para {
	struct eswap_page_pool *pool;
	size_t alloc_size;
	bool fast;
	bool nofail;
};

struct {
	atomic_t online;
	atomic_t swapout_enable;
	int log_level;
	int acct_detail;
	int extra_areas;
	struct eswap_stat stat;
	struct workqueue_struct *reclaim_wq;
	struct workqueue_struct *pageout_wq;
	struct zram *zram;
	struct file *fp_ori;

	int nr_exts;
	int nr_pages;

	/* used to sync with eswap_init and eswap_bind */
	struct completion init_complete;
} global_settings;

void *eswap_malloc(size_t size, bool fast, bool nofail)
{
	void *mem = NULL;

	if (likely(fast)) {
		mem = kzalloc(size, GFP_ATOMIC);
		if (likely(mem || !nofail))
			return mem;
	}

	mem = kzalloc(size, GFP_NOIO);

	return mem;
}

void eswap_free(const void *mem)
{
	kfree(mem);
}

static struct page *eswap_alloc_page_common(void *data, gfp_t gfp)
{
	struct page *page = NULL;
	struct zs_ext_para *ext_para = (struct zs_ext_para *)data;

	if (ext_para->pool) {
		spin_lock(&ext_para->pool->page_pool_lock);
		if (!list_empty(&ext_para->pool->page_pool_list)) {
			page = list_first_entry(&ext_para->pool->page_pool_list, struct page, lru);
			list_del(&page->lru);
		}
		spin_unlock(&ext_para->pool->page_pool_lock);
	}

	if (!page) {
		if (ext_para->fast) {
			page = alloc_page(GFP_ATOMIC);
			if (likely(page))
				goto out;
		}
		if (ext_para->nofail)
			page = alloc_page(GFP_NOIO);
		else
			page = alloc_page(gfp);
	}
out:
	return page;
}

struct page *eswap_alloc_page(struct eswap_page_pool *pool,
		gfp_t gfp, bool fast, bool nofail)
{
	struct zs_ext_para ext_para;

	ext_para.pool = pool;
	ext_para.fast = fast;
	ext_para.nofail = nofail;

	if (pool == NULL) {
		struct list_head *list = &pf_page_pool_list;
		struct page *page = NULL;

		spin_lock(&pf_page_pool_lock);
		if (!list_empty(list)) {
			page = list_first_entry(list, struct page, lru);
			list_del_init(&page->lru);
			pf_pool_pages--;
		}
		spin_unlock(&pf_page_pool_lock);

		if (page != NULL)
			return page;
	}

	return eswap_alloc_page_common((void *)&ext_para, gfp);
}

void eswap_page_recycle(struct page *page, struct eswap_page_pool *pool)
{
	if (pool) {
		spin_lock(&pool->page_pool_lock);
		list_add(&page->lru, &pool->page_pool_list);
		spin_unlock(&pool->page_pool_lock);
	} else {
		struct list_head *list = &pf_page_pool_list;
		bool freed = false;
		spin_lock(&pf_page_pool_lock);
		if (pf_pool_pages < MAX_PF_POOL_PAGES) {
			list_add(&page->lru, list);
			pf_pool_pages++;
			freed = true;
		}
		spin_unlock(&pf_page_pool_lock);

		// return to buddy
		if (!freed)
			__free_page(page);
	}
}

inline struct eswap_stat *eswap_get_stat_obj(void)
{
	return &global_settings.stat;
}

inline struct zram *eswap_get_global_zram(void)
{
	return global_settings.zram;
}

inline struct eswap_area *eswap_get_global_area(void)
{
	return global_settings.zram->area;
}

inline void eswap_used_exts_num_inc(void)
{
	int val;

	val = atomic_inc_return(&global_settings.stat.used_exts_num);
	if (atomic_read(&global_settings.stat.max_used_exts_num) < val)
		atomic_set(&global_settings.stat.max_used_exts_num, val);
}

inline void eswap_used_exts_num_dec(void)
{
	atomic_dec(&global_settings.stat.used_exts_num);
}

inline int eswap_get_used_exts_num(void)
{
	return atomic_read(&global_settings.stat.used_exts_num);
}

int eswap_get_used_ext_pages(void)
{
	return (atomic_read(&global_settings.stat.used_exts_num) * EXTENT_PG_CNT);
}

inline int eswap_get_exts_num(void)
{
	return global_settings.nr_exts;
}

inline int eswap_loglevel(void)
{
	return global_settings.log_level;
}

inline int eswap_acct_detail(void)
{
	return global_settings.acct_detail;
}

inline int eswap_set_loglevel(int level)
{
	if (level < ESD_ERR || level > ESD_MAX)
		return -EINVAL;
	global_settings.log_level = level;
	return 0;
}

inline int eswap_set_acct_detail(int en)
{
	return global_settings.acct_detail = !!en;
}

inline bool eswap_swapout_enable(void)
{
	return !!atomic_read(&global_settings.swapout_enable);
}

inline bool eswap_enable(void)
{
	return atomic_read(&global_settings.online);
}

inline void eswapout_enable(bool en)
{
	atomic_set(&global_settings.swapout_enable, en ? 1 : 0);
}

static inline void eswap_stat_init(struct eswap_stat *stat)
{
	spin_lock_init(&stat->record.lock);
}

static int eswap_basic_setting_init(struct zram *zram)
{
	eswap_stat_init(&global_settings.stat);

	global_settings.log_level = ESD_ERR;
	global_settings.acct_detail = 0;
	global_settings.zram = zram;

	/* disable eswapout by default */
	eswapout_enable(false);

	/* and make eswap offline */
	atomic_set(&global_settings.online, 0);
	misc_init(zram);

	global_settings.reclaim_wq = alloc_workqueue("eswapout_wq",
						WQ_CPU_INTENSIVE, 0);
	if (unlikely(!global_settings.reclaim_wq)) {
		misc_deinit(zram);
		global_settings.zram = NULL;
		eswap_debug(ESD_ERR, "reclaim workqueue allocation failed!\n");
		return -1;
	}

	global_settings.pageout_wq = alloc_workqueue("pageout_wq", WQ_HIGHPRI | WQ_UNBOUND, 0);
	if (unlikely(!global_settings.pageout_wq)) {
		destroy_workqueue(global_settings.reclaim_wq);
		misc_deinit(global_settings.zram);
		global_settings.zram = NULL;
		return -1;
	}

	return 0;
}

void eswap_setting_deinit(void)
{
	destroy_workqueue(global_settings.pageout_wq);
	destroy_workqueue(global_settings.reclaim_wq);
	misc_deinit(global_settings.zram);
	global_settings.zram = NULL;
}

inline struct workqueue_struct *eswapout_workqueue(void)
{
	return global_settings.reclaim_wq;
}

inline struct workqueue_struct *pageout_workqueue(void)
{
	return global_settings.pageout_wq;
}

/* This interface will be called when user set the ZRAM size. */
void eswap_init(struct zram *zram)
{
	int ret;

	if (!global_settings.nr_pages) {
		eswap_debug(ESD_ERR, "wow, failed init!\n");
		return;
	}

	ret = eswap_basic_setting_init(zram);
	if (unlikely(ret)) {
		eswap_debug(ESD_ERR, "failed basic init!\n");
		return;
	}

	/* zram is ready, eswap is ready, it can be onlined */
	//complete(&global_settings.init_complete);

#if 1
	zram->area = alloc_eswap_area(zram->bdev, global_settings.fp_ori, zram->disksize, global_settings.nr_pages << PAGE_SHIFT);
	if (!zram->area) {
		if (global_settings.fp_ori)
			fput(global_settings.fp_ori);

		eswap_debug(ESD_ERR, "init manager failed! %d\n", ret);
		ret = -ENOMEM;
		//goto err_alloc_space;
	}

	global_settings.stat.last_time = jiffies;

	/*
	 * care should be taken to make zram->area
	 * visable to all cpus before online
	 */
	smp_wmb();
	atomic_set(&global_settings.online, 1);

	/* enable swapout */
	eswapout_enable(1);

#endif
	eswap_debug(ESD_ERR, "succeed!\n");
	sync_info_with_extern();
}

static void eswap_close_bdev(struct block_device *bdev, struct file *backing_dev, void *holder)
{
	if (bdev)
		blkdev_put(bdev, holder);

	if (backing_dev)
		filp_close(backing_dev, NULL);
}

static struct file *eswap_open_bdev(const char *file_name)
{
	struct file *backing_dev = NULL;

	backing_dev = filp_open_block(file_name, O_RDWR | O_DIRECT | O_LARGEFILE, 0);
	if (unlikely(IS_ERR(backing_dev))) {
		eswap_debug(ESD_ERR, "open the %s failed! eno = %ld\n",
				file_name, PTR_ERR(backing_dev));
		backing_dev = NULL;
		return NULL;
	}

	if (unlikely(!S_ISBLK(backing_dev->f_mapping->host->i_mode))) {
		eswap_debug(ESD_ERR, "%s isn't a blk device\n", file_name);
		eswap_close_bdev(NULL, backing_dev, NULL);
		return NULL;
	}

	return backing_dev;
}

static int eswap_bind_area(struct eswap_area *area, struct zram *zram, const char *file_name)
{
	struct file *backing_dev = NULL;
	struct inode *inode = NULL;
	struct block_device *bdev = NULL;
	unsigned long size;
	int err;
	char *pfd = NULL;
	char *pdev = NULL;
	struct file *fp = NULL;
	char localbdev[100];

	strncpy(localbdev, file_name, 100);
	localbdev[99] = '\0';

	pdev = strchr(localbdev, ' ');
	if (pdev != NULL) {
		*pdev = '\0';
		pdev++;
		pfd = localbdev;
		file_name = pdev;
	}

	eswap_debug(ESD_ERR, "%s %s\n", pfd ? pfd : "NA", pdev ? pdev : "NA"); 

	/* if pfd is non null, pin the file */
	if (pfd != NULL) {
		int fd;
		if (kstrtoint(pfd, 0, &fd))
			return -EINVAL;

		if (fd < 0)
			return -EINVAL;

		fp = fget(fd);
		if (fp == NULL)
			return -EINVAL;
	}

	backing_dev = eswap_open_bdev(file_name);
	if (unlikely(!backing_dev)) {
		if (fp)
			fput(fp);
		return -EINVAL;
	}

	inode = backing_dev->f_mapping->host;

	ihold(inode);
	bdev = I_BDEV(inode);
	//bdev = bdgrab(I_BDEV(inode));

	bdev = blkdev_get_by_dev(bdev->bd_dev, BLK_OPEN_READ | BLK_OPEN_WRITE | BLK_OPEN_EXCL, area, NULL);
	if (IS_ERR(bdev)) {
		err = PTR_ERR(bdev);
		eswap_debug(ESD_ERR, "%s blkdev_get failed! eno = %d\n",
					file_name, err);
		bdev = NULL;
		goto out;
	}

	size = (unsigned long)i_size_read(inode);

	err = set_blocksize(bdev, PAGE_SIZE);
	if (unlikely(err)) {
		eswap_debug(ESD_ERR, "%s set blocksize failed! eno = %d\n", file_name, err);
		goto out;
	}

	err = add_eswap_extra_area(area, bdev, fp, size);
	if (err) {
		eswap_debug(ESD_ERR, "%s add extra area err! eno = %d\n", file_name, err);
		goto out;
	}

	eswap_debug(ESD_ERR, "%s\n", file_name);
	global_settings.nr_exts += (int)(size >> EXTENT_SHIFT);
	global_settings.nr_pages += (int)(size >> PAGE_SHIFT);
	eswap_debug(ESD_ERR, "nr_pages %lu\n", size >> PAGE_SHIFT);

	down_write(&zram->init_lock);
	zram->nr_pages += (size >> PAGE_SHIFT);
	up_write(&zram->init_lock);

	return 0;
out:
	eswap_close_bdev(bdev, backing_dev, area);
	if (fp)
		fput(fp);
	return err;
}

// static int eswap_bind_zram(struct zram *zram, const char *file_name)
static struct block_device *eswap_bind_zram(struct zram *zram, const char *file_name, struct file *fp)
{
	struct file *backing_dev = NULL;
	struct inode *inode = NULL;
	unsigned long nr_pages;
	struct block_device *bdev = NULL;
	int err;

	backing_dev = eswap_open_bdev(file_name);
	if (unlikely(!backing_dev))
		// return -EINVAL;
		return NULL;

	inode = backing_dev->f_mapping->host;

	ihold(inode);
	bdev = I_BDEV(inode);
	//bdev = bdgrab(I_BDEV(inode));

	bdev = blkdev_get_by_dev(bdev->bd_dev, BLK_OPEN_READ | BLK_OPEN_WRITE | BLK_OPEN_EXCL, zram, NULL);
	if (IS_ERR(bdev)) {
		err = PTR_ERR(bdev);
		eswap_debug(ESD_ERR, "%s blkdev_get failed! eno = %d\n", file_name, err);
		bdev = NULL;
		goto out;
	}

	nr_pages = (unsigned long)i_size_read(inode) >> PAGE_SHIFT;

	err = set_blocksize(bdev, PAGE_SIZE);
	if (unlikely(err)) {
		eswap_debug(ESD_ERR,
			"%s set blocksize failed! eno = %d\n", file_name, err);
		goto out;
	}

	eswap_debug(ESD_ERR, "%s\n", file_name);
	global_settings.nr_pages += nr_pages;
	global_settings.nr_exts += (int)((nr_pages << PAGE_SHIFT) >> EXTENT_SHIFT);
	global_settings.fp_ori = fp;

	down_write(&zram->init_lock);
	zram->bdev = bdev;
	zram->backing_dev = backing_dev;
	zram->nr_pages += nr_pages;
	up_write(&zram->init_lock);

	eswap_debug(ESD_ERR, "nr_pages %lu\n", nr_pages);
	return bdev;

out:
	eswap_close_bdev(bdev, backing_dev, zram);
	return NULL;
}

static int eswap_online(const char *bdevname, struct zram *zram)
{
	int ret;
	struct block_device *bdev;
	char *pfd = NULL;
	char *pdev = NULL;
	struct file *fp = NULL;
	char localbdev[100];

	/* this is a one-time job */
	/* once online, eswap can not be offline */
	if (atomic_read(&global_settings.online))
		return 0;

	if (bdevname == NULL) {
		eswap_debug(ESD_ERR, "bdevname is NULL!\n");
		return -EINVAL;
	}

	/* must be after the online check */
	// wait_for_completion(&global_settings.init_complete);
	if (zram == NULL) {
		eswap_debug(ESD_ERR, "zram is NULL!\n");
		return -EINVAL;
	}

	strncpy(localbdev, bdevname, 100);
	localbdev[99] = '\0';

	pdev = strchr(localbdev, ' ');
	if (pdev != NULL) {
		*pdev = '\0';
		pdev++;
		pfd = localbdev;
		bdevname = pdev;
	}
	eswap_debug(ESD_ERR, "%s %s\n", pfd ? pfd : "NA", pdev ? pdev : "NA"); 

	/* if pfd is non null, pin the file */
	if (pfd != NULL) {
		int fd;
		if (kstrtoint(pfd, 0, &fd))
			return -EINVAL;

		if (fd < 0)
			return -EINVAL;

		fp = fget(fd);
		if (fp == NULL)
			return -EINVAL;
	}

	ret = eswap_io_scheduler_init();
	if (unlikely(ret)) {
		eswap_debug(ESD_ERR, "init schedule failed! %d\n", ret);
		goto err_out;
	}

	bdev = eswap_bind_zram(zram, bdevname, fp);
	if (NULL == bdev) {
		eswap_debug(ESD_ERR, "alloc space failed! %d\n", ret);
		goto err_alloc_space;
	}

	return 0;
#if 0
	zram->area = alloc_eswap_area(bdev, zram->disksize, global_settings.nr_pages << PAGE_SHIFT);
	if (!zram->area) {
		eswap_debug(ESD_ERR, "init manager failed! %d\n", ret);
		ret = -ENOMEM;
		goto err_alloc_space;
	}

	global_settings.stat.last_time = jiffies;

	/*
	 * care should be taken to make zram->area
	 * visable to all cpus before online
	 */
	smp_wmb();
	atomic_set(&global_settings.online, 1);
	return 0;
#endif

err_alloc_space:
	eswap_io_scheduler_deinit();
err_out:
	if (fp)
		fput(fp);
	return ret;
}

static int eswap_online_extra(const char *bdevname)
{
	int ret;
	if (!eswap_enable())
		return -EPERM;

	ret = eswap_bind_area(global_settings.zram->area, global_settings.zram, bdevname);
	if (ret == 0)
		global_settings.extra_areas++;

	sync_info_with_extern();
	return ret;
}

ssize_t eswap_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	int ret;
	unsigned long val;

	if (!atomic_read(&global_settings.online)) {
		eswap_debug(ESD_ERR, "eswap is offline\n");
		return -EINVAL;
	}

	ret = kstrtoul(buf, 0, &val);
	if (unlikely(ret)) {
		eswap_debug(ESD_ERR, "val is error!\n");
		return -EINVAL;
	}

	/* eswapout enable or disable */
	eswapout_enable(!!val);

	return len;
}

ssize_t eswap_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "eswap %s reclaim_in %s\n",
			atomic_read(&global_settings.online) ? "online" : "offline",
			eswap_swapout_enable() ? "enable" : "disable");
}

ssize_t eswap_online_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t len)
{
	char *devname = (char *)buf;
	struct zram *zram = dev_to_zram(dev);

	if (devname[len-1] == '\n')
		devname[len-1] = '\0';

	return eswap_online(devname, zram) ? : len;
}

ssize_t eswap_online_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&global_settings.online));
}

ssize_t eswap_extra_online_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t len)
{
	char *devname = (char *)buf;
	if (devname[len-1] == '\n')
		devname[len-1] = '\0';

	return eswap_online_extra(devname) ? : len;
}

ssize_t eswap_extra_online_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", global_settings.extra_areas);
}

int eswap_module_init(void)
{
	int i = 0;
	struct list_head *list = &pf_page_pool_list;

	for (i = 0; i < MAX_PF_POOL_PAGES; i++) {
		struct page *page = alloc_page(GFP_KERNEL);
		if (page == NULL) {
			while (!list_empty(list)) {
				page = list_first_entry(list, struct page, lru);
				list_del_init(&page->lru);
				__free_page(page);
			}
			return -ENOMEM;
		}

		list_add(&page->lru, list);
		pf_pool_pages++;
	}

	// default offline
	atomic_set(&global_settings.online, 0);
	init_completion(&global_settings.init_complete);

	return 0;
}

void eswap_module_exit(void)
{
	struct page *page;
	struct list_head *list = &pf_page_pool_list;

	spin_lock(&pf_page_pool_lock);
	while (!list_empty(list)) {
		page = list_first_entry(list, struct page, lru);
		list_del_init(&page->lru);
		pf_pool_pages--;

		__free_page(page);

	}
	spin_unlock(&pf_page_pool_lock);
	pr_notice("%s\n", __func__);
}
