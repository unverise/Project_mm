#ifndef __ESWAP_AREA_H__
#define __ESWAP_AREA_H__

#include <linux/memcontrol.h>
#include <linux/types.h>
#include <linux/shrinker.h>
#include <linux/blkdev.h>

#include "eswap_list.h"
#include "eswap_internal.h"

#ifndef MEM_CGROUP_ID_MAX
#define MEM_CGROUP_ID_MAX       ((1UL << MEM_CGROUP_ID_SHIFT) - 1)
#endif

#define MAX_EXTRA_AREAS 16

struct eswap_area;

struct eswap_extra_area {
	unsigned long size;
	int nr_exts;
	int area_id_offset;

	unsigned long *bitmap;
	atomic_t last_alloc_bit;

	atomic_t stored_exts;
	atomic64_t bd_count;
	atomic64_t bd_reads;
	atomic64_t bd_writes;
	atomic64_t bd_read_pages;
	atomic64_t bd_write_pages;

	atomic64_t eswapout_pages;
	atomic64_t eswapin_pages_pf;

	struct hh_list_head *ext;
	struct hh_list_head *rmap;

	atomic_t *ext_stored_pages;

	// atomic_t *ext_stored_size;

	struct block_device *bdev;
	struct file *fp_ori;
	struct eswap_area *main;
};

/* how much memory eswap_area takes
 * For a 8G zram/swap config,
 * nr_objs = 2097152, lru list takes 2097152 * 8 = 16M
 * Total takes 16MB
 *
 * For a 1G rom config, 64KB extent config
 * nr_exts = 16384, both ext and rmap list takes 128KB
 * the ext_stored_pages takes 64KB
 * Total takes 320KB
 *
 * The extent counter for memcg, takes 2 * 2^16 = 128KB
 * Total takes 128KB
 *
 * To sum up: 16MB + 320KB + 128KB
 */
struct eswap_area {
	int nr_objs;
	int nr_mcgs;
	int area_base_bits;
	int extra_areas_cnt;

	struct hh_list_head *lru;

	struct hh_list_table *ext_table;
	struct hh_list_table *obj_table;

	struct eswap_extra_area *extra_areas[MAX_EXTRA_AREAS];

	unsigned short mcg_id_cnt[MEM_CGROUP_ID_MAX + 1];
};

/* Note: the lower 20 bits of ext_stored_pages are used as size count,
 * the remaining 12 bits ares used as page counter.
  */
static inline atomic_t *eswap_area_get_ext_stored_pages_counter(struct eswap_area *area, int extid)
{
	struct eswap_extra_area *earea;
	earea = area->extra_areas[extid >> area->area_base_bits];
	return &(earea->ext_stored_pages[extid - earea->area_id_offset]);
}

static inline void eswap_area_extent_counter_inc(struct eswap_area *area, int extid, int size)
{
	struct eswap_extra_area *earea = area->extra_areas[extid >> area->area_base_bits];
	int local_extid = extid - earea->area_id_offset;

	atomic64_inc(&earea->bd_count);
	atomic_add(size + (1 << 20), &earea->ext_stored_pages[local_extid]);
}

static inline void eswap_area_extent_counter_add(struct eswap_area *area, int extid, int cnt, int size)
{
	struct eswap_extra_area *earea = area->extra_areas[extid >> area->area_base_bits];
	int local_extid = extid - earea->area_id_offset;

	atomic64_add(cnt, &earea->bd_count);
	atomic_add(size + (cnt << 20), &earea->ext_stored_pages[local_extid]);
}

static inline void eswap_area_extent_counter_dec(struct eswap_area *area, int extid, int size)
{
	struct eswap_extra_area *earea = area->extra_areas[extid >> area->area_base_bits];
	int local_extid = extid - earea->area_id_offset;

	atomic64_dec(&earea->bd_count);
	atomic_sub(size + (1 << 20), &earea->ext_stored_pages[local_extid]);
}

static inline void eswap_area_extent_counter_sub(struct eswap_area *area, int extid, int cnt, int size)
{
	struct eswap_extra_area *earea = area->extra_areas[extid >> area->area_base_bits];
	int local_extid = extid - earea->area_id_offset;

	atomic64_sub(cnt, &earea->bd_count);
	atomic_sub(size + (cnt << 20), &earea->ext_stored_pages[local_extid]);
}

static inline bool eswap_area_extent_counter_dec_test_empty(struct eswap_area *area, int extid, int size)
{
	struct eswap_extra_area *earea = area->extra_areas[extid >> area->area_base_bits];
	int local_extid = extid - earea->area_id_offset;

	atomic64_dec(&earea->bd_count);
	return atomic_sub_and_test(size + (1 << 20), &earea->ext_stored_pages[local_extid]);
}

static inline void eswap_area_inc_bd_writes(struct zram *zram, int extid, int page_cnt)
{
	struct eswap_area *area = zram->area;
	struct eswap_extra_area *earea = area->extra_areas[extid >> area->area_base_bits];
	atomic64_inc(&earea->bd_writes);
	atomic64_add(page_cnt, &earea->bd_write_pages);
}

static inline void eswap_area_inc_bd_reads(struct zram *zram, int extid, int page_cnt)
{
	struct eswap_area *area = zram->area;
	struct eswap_extra_area *earea = area->extra_areas[extid >> area->area_base_bits];
	atomic64_inc(&earea->bd_reads);
	atomic64_add(page_cnt, &earea->bd_read_pages);
}

static inline void eswap_area_inc_eswapin_pages_pf(struct zram *zram, int extid)
{
	struct eswap_area *area = zram->area;
	struct eswap_extra_area *earea = area->extra_areas[extid >> area->area_base_bits];
	atomic64_inc(&earea->eswapin_pages_pf);
}

static inline void eswap_area_inc_eswapout_pages(struct zram *zram, int extid)
{
	struct eswap_area *area = zram->area;
	struct eswap_extra_area *earea = area->extra_areas[extid >> area->area_base_bits];
	atomic64_inc(&earea->eswapout_pages);
}

static inline void eswap_area_add_eswapin_pages_pf(struct zram *zram, int extid, int objs)
{
	struct eswap_area *area = zram->area;
	struct eswap_extra_area *earea = area->extra_areas[extid >> area->area_base_bits];
	atomic64_add(objs, &earea->eswapin_pages_pf);
}

static inline void eswap_area_add_eswapout_pages(struct zram *zram, int extid, int objs)
{
	struct eswap_area *area = zram->area;
	struct eswap_extra_area *earea = area->extra_areas[extid >> area->area_base_bits];
	atomic64_add(objs, &earea->eswapout_pages);
}

static inline struct block_device *map_extid_to_bdev(struct eswap_area *area, int extid)
{
	struct eswap_extra_area *earea;
	earea = area->extra_areas[extid >> area->area_base_bits];
	return earea->bdev;
}

static inline sector_t map_extid_to_sector(struct eswap_area *area, int extid)
{
	struct eswap_extra_area *earea;
	earea = area->extra_areas[extid >> area->area_base_bits];
	return (sector_t)((extid - earea->area_id_offset) * EXTENT_SECTOR_CNT);
}

static inline int map_extid_to_areaid(struct eswap_area *area, int extid)
{
	return extid >> area->area_base_bits;
}

static inline int map_extid_to_eextid(struct eswap_area *area, int extid)
{
	struct eswap_extra_area *earea;
	earea = area->extra_areas[extid >> area->area_base_bits];
	return (extid - earea->area_id_offset);
}

static inline int esentry_extid(unsigned long e)
{
	return (e) >> EXTENT_SHIFT;
}

static inline int esentry_pgid(unsigned long e)
{
	return ((e) & EXTENT_MASK) >> PAGE_SHIFT;
}

static inline int esentry_pgoff(unsigned long e)
{
	return (e) & (PAGE_SIZE - 1);
}

struct mem_cgroup *get_memcg_with_css_get(unsigned short mcg_id);

struct mem_cgroup *get_mem_cgroup(unsigned short mcg_id);

int obj_idx(struct eswap_area *area, int idx);
int ext_idx(struct eswap_area *area, int idx);
int mcg_idx(struct eswap_area *area, int idx);

extern struct eswap_area *alloc_eswap_area(struct block_device *, struct file *,
			unsigned long ori, unsigned long comp);
extern int add_eswap_extra_area(struct eswap_area *, struct block_device *,
			struct file *, unsigned long comp_size);
extern void free_eswap_area(struct eswap_area *area);

extern void eswap_area_free_extent(struct eswap_area *area, int ext_id);
extern int eswap_area_alloc_extent(struct eswap_area *area, struct mem_cgroup *mcg);

extern int fetch_extent_from_memcg_by_extid(struct eswap_area *area, int extid);
extern int fetch_extent_from_memcg(struct eswap_area *area, struct mem_cgroup *mcg,
				bool (*filter)(struct eswap_area *area, int ext_id));
extern void put_extent_to_memcg(struct eswap_area *area, int extid);

int get_memcg_zram_entry(struct eswap_area *area, struct mem_cgroup *mcg);
int get_extent_zram_entry(struct eswap_area *area, int extid);
#endif // __ESWAP_AREA_H__
