/*
 */
#ifndef __ESWAP_IO_SCHEDULER__
#define __ESWAP_IO_SCHEDULER__
#include <linux/blkdev.h>
#include "eswap_internal.h"
/* io scheduler sees io_extent and io_segment, but not io_extent_mgmt */
struct io_extent {
	struct block_device *bdev;
	struct page **dest_pages;
	struct list_head list;

	sector_t addr;
	int ext_id;
	int pages_sz;
	bool sensitive;	// only used in pf path
};

struct eswap_io_scheduler_config {
	struct zram *zram;
	enum eswap_scenario scenario;
	void (*io_extent_done_cb)(struct io_extent *, enum eswap_scenario, int);
	void (*complete_notify)(void *);
	void *private;
	struct eswap_key_point_record *record;
};

extern int eswap_io_scheduler_init(void);
extern void eswap_io_scheduler_deinit(void);
extern void *eswap_io_scheduler_get(struct eswap_io_scheduler_config *);
extern int eswap_extent_io(void *io_scheduler, struct io_extent *io_ext);
extern int eswap_io_scheduler_put(void *io_scheduler);

#endif // __ESWAP_IO_SCHEDULER__
