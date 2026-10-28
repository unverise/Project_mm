#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/bitops.h>
#include <linux/blkdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/backing-dev.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/idr.h>

#include "../zram_drv.h"
#include "eswap_area.h"

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
extern int g_backing_dev_avail_count;
extern unsigned long bdev_size;
extern void bdev_size_lookup(void);
void sync_dump_info_with_eswap(struct zram *zram)
{
	int i, areas = 0;
	struct eswap_area *area = zram->area;
	struct eswap_stat *stat = eswap_get_stat_obj();
	unsigned long total_pages = 0;

	if (!eswap_enable() || !area)
		return;

	for (i = 0; i < area->extra_areas_cnt; i++) {
		struct eswap_extra_area *earea = area->extra_areas[i];
		unsigned long nr_pages;
		if (earea == NULL)
			break;

		areas++;
		nr_pages = (earea->nr_exts * EXTENT_PG_CNT);
		total_pages += nr_pages;

		zram->nr_pages_list[i] = nr_pages;
		atomic64_set(&zram->stats.bd_count_list[i], atomic64_read(&earea->bd_count));
		// atomic64_set(&zram->stats.bd_reads_list[i], atomic64_read(&earea->bd_read_pages));
		// atomic64_set(&zram->stats.bd_writes_list[i], atomic64_read(&earea->bd_write_pages));
		atomic64_set(&zram->stats.bd_reads_list[i], atomic64_read(&earea->eswapin_pages_pf));
		atomic64_set(&zram->stats.bd_writes_list[i], atomic64_read(&earea->eswapout_pages));
	}

	zram->nr_pages_total = total_pages;
	bdev_size = zram->nr_pages_total << (PAGE_SHIFT - 10);
	bdev_size_lookup();

	atomic64_set(&zram->stats.bd_count, atomic64_read(&stat->eswap_stored_pages));

	/* Compressed Pages In Bdev */
	atomic64_set(&zram->stats.bd_objcnt, atomic64_read(&stat->eswap_stored_pages));

	/* Bdev Actual Saved Physical Memory */
	atomic64_set(&zram->stats.bd_size, atomic64_read(&stat->eswap_stored_bytes));

	// atomic64_set(&zram->stats.bd_reads, atomic64_read(&stat->eswap_bd_reads));
	// atomic64_set(&zram->stats.bd_reads, (atomic64_read(&stat->eswap_bd_reads) * EXTENT_PG_CNT));
	atomic64_set(&zram->stats.bd_reads, atomic64_read(&stat->eswapin_pages_pf));

	// atomic64_set(&zram->stats.bd_writes, atomic64_read(&stat->eswap_bd_writes));
	// atomic64_set(&zram->stats.bd_writes, atomic64_read(&stat->eswapout_bytes)/PAGE_SIZE);
	atomic64_set(&zram->stats.bd_writes, atomic64_read(&stat->eswapout_pages));

	g_backing_dev_avail_count = (areas - 1);
}

extern unsigned long long bdev_write_page_time;
extern unsigned long long bdev_write_page_count;

void sync_rw_data_with_eswap(struct zram *zram)
{
	struct eswap_stat *stat = eswap_get_stat_obj();
	ktime_t now = ktime_get_boottime();

	if (!eswap_enable())
		return;

	/* how many pages have been written back */
	// bdev_write_page_count = atomic64_read(&stat->eswap_bd_writes);
	bdev_write_page_count = atomic64_read(&stat->eswapout_bytes)/PAGE_SIZE;
	bdev_write_page_time = atomic64_read(&stat->eswap_bd_write_delay);

	/* how many pages have been read back by PF */
	// atomic64_set(&zram->stats.swapin_count, atomic64_read(&stat->eswap_bd_reads));
	atomic64_set(&zram->stats.swapin_count, (EXTENT_PG_CNT * atomic64_read(&stat->eswap_bd_reads)));
	atomic64_set(&zram->stats.swapin_delay, atomic64_read(&stat->eswap_bd_read_delay));

	atomic64_set(&zram->stats.swapout_delay, atomic64_read(&stat->eswap_bd_write_delay));
	atomic64_set(&zram->stats.swapout_count, atomic64_read(&stat->eswapout_bytes)/PAGE_SIZE);

	atomic64_set(&zram->stats.io_lt_500, atomic64_read(&stat->io_lt_500));
	atomic64_set(&zram->stats.io_lt_1000, atomic64_read(&stat->io_lt_1000));
	atomic64_set(&zram->stats.io_lt_3000, atomic64_read(&stat->io_lt_3000));
	atomic64_set(&zram->stats.io_lt_5000, atomic64_read(&stat->io_lt_5000));
	atomic64_set(&zram->stats.io_lt_30000, atomic64_read(&stat->io_lt_30000));
	atomic64_set(&zram->stats.io_lt_50000, atomic64_read(&stat->io_lt_50000));
	atomic64_set(&zram->stats.io_gt_50000, atomic64_read(&stat->io_gt_50000));

	atomic64_set(&zram->stats.draw_io_delay, atomic64_read(&stat->sensitive_delay));

	try_to_add_to_table(&zram->wbd_stat.rd_hour, now, atomic64_read(&zram->stats.swapin_count) * 4);
}
#endif
