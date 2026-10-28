#ifndef __ESWAP_EXTENT_MANAGEMENT__
#define __ESWAP_EXTENT_MANAGEMENT__
#include "eswap_internal.h"

/* eswap mamnager sees io_extent_mgmt */
struct io_extent_mgmt {
	struct zram *zram;
	struct mem_cgroup *mcg;	// will pin css
	struct eswap_page_pool *pool;

	int ext_id;
	int cnt;

	/* for writeback path: used as access time ktime
	 * for pagefault path: used as an indicator of losing memcg info
	 */
	unsigned long private;

	struct page *pages[EXTENT_PG_CNT];
	u32 index[EXTENT_MAX_OBJ_CNT];
};

extern void io_extent_mgmt_register(struct io_extent_mgmt *io_ext);
extern void io_extent_mgmt_unregister(struct io_extent_mgmt *, enum eswap_scenario);
extern void io_extent_mgmt_exception(struct io_extent_mgmt *, enum eswap_scenario);

extern struct io_extent_mgmt *io_extent_mgmt_create(struct mem_cgroup *memcg,
	struct eswap_page_pool *pool, unsigned long *to_reclaimed, ktime_t);
extern struct io_extent_mgmt *eswap_extent_find_mgmt_by_idx(unsigned long eswpentry,
	struct zram *, struct io_extent_mgmt *);
extern struct io_extent_mgmt *eswap_extent_find_mgmt_by_memcg(
	struct mem_cgroup *, struct eswap_page_pool *);

#endif // __ESWAP_EXTENT_MANAGEMENT__
