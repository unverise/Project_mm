#include <linux/kernel.h>
#include <linux/swap.h>
#include <linux/ktime.h>

#include "../zram_drv.h"
#include "eswap_internal.h"

#include "eswap_list.h"
#include "eswap_area.h"
#include "eswap_lru_rmap.h"
#include "memcg_estrack.h"

#define esentry_extid(e) ((e) >> EXTENT_SHIFT)

struct mem_cgroup *zram_get_memcg(struct zram *zram, u32 index)
{
	unsigned short mcgid;

	mcgid = hh_list_get_mcgid(obj_idx(zram->area, index), zram->area->obj_table);
	return get_mem_cgroup(mcgid);
}

struct mem_cgroup *zram_get_memcg_with_css_get(struct zram *zram, u32 index)
{
	unsigned short mcgid;

	mcgid = hh_list_get_mcgid(obj_idx(zram->area, index), zram->area->obj_table);
	return get_memcg_with_css_get(mcgid);
}

/* set memcg'id and pin it */
void zram_set_memcg(struct zram *zram, u32 index, struct mem_cgroup *memcg)
{
	unsigned short mcgid = 0;

	if (memcg) {
		mcgid = memcg->id.id;
	}

	hh_list_set_mcgid(obj_idx(zram->area, index), zram->area->obj_table, mcgid);
}

/* clear memcg'id and unpin it */
void zram_clear_memcg(struct zram *zram, u32 index)
{
	hh_list_set_mcgid(obj_idx(zram->area, index), zram->area->obj_table, 0);
}

int zram_get_memcg_coldest_index(struct zram *zram, struct mem_cgroup *memcg,
	unsigned int *index, int max_cnt,
	int (*filter)(void *, int), void *data)
{
	int cnt = 0;
	struct eswap_area *area = zram->area;
	int midx = mcg_idx(area, memcg->id.id);
	u32 i;

	if (midx < 0)
		return 0;

	hh_lock_list(midx, area->obj_table);
	hh_list_for_each_entry_reverse(i, midx, area->obj_table) {
		if (i >= (u32)area->nr_objs) {
			eswap_debug(ESD_ERR, "index = %u invalid\n", i);
			continue;
		}

		if (filter && !filter(data, i))
			break;

		index[cnt++] = i;
		if (cnt >= max_cnt)
			break;
	}
	hh_unlock_list(midx, area->obj_table);

	return cnt;
}

int zram_rmap_get_extent_index(struct eswap_area *area, int ext_id, int *index)
{
	int cnt = 0, eidx;
	u32 i;

	eidx = ext_idx(area, ext_id);
	if (eidx < 0) {
		eswap_debug(ESD_ERR, "ext = %d invalid %d\n", ext_id, eidx);
		return 0;
	}

	hh_lock_list(eidx, area->obj_table);
	hh_list_for_each_entry(i, eidx, area->obj_table) {
		if (cnt >= (int)EXTENT_MAX_OBJ_CNT) {
			WARN_ON_ONCE(1);
			break;
		}
		index[cnt++] = i;
	}
	hh_unlock_list(eidx, area->obj_table);

	return cnt;
}

int zram_lru_add(struct zram *zram, u32 index, struct mem_cgroup *memcg)
{
	unsigned long size;
	int midx;
	struct eswap_stat *stat = eswap_get_stat_obj();
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);

	if (!est->zram->area) {
		eswap_debug(ESD_ERR, "invalid memcg\n");
		return -EINVAL;
	}

	midx = mcg_idx(zram->area, memcg->id.id);
	if (midx < 0)
		return -EINVAL;

	zram_set_memcg(zram, index, memcg);
	hh_list_add(obj_idx(zram->area, index), midx, zram->area->obj_table);

	size = zram_get_obj_size(zram, index);

	atomic_add(size, &est->zram_stored_size);
	atomic_inc(&est->zram_page_size);

	atomic64_add(size, &stat->zram_stored_bytes);
	atomic64_inc(&stat->zram_stored_pages);

	return 0;
}

void zram_lru_add_tail(struct zram *zram, u32 index, struct mem_cgroup *memcg)
{
	unsigned long size;
	struct eswap_stat *stat = eswap_get_stat_obj();
	struct memcg_estrack *est;

	if (!zram || !memcg) {
		eswap_debug(ESD_ERR, "NULL zram or memcg\n");
		return;
	}

	est = mem_cgroup_estrack(memcg);

	if (!est->zram || !est->zram->area) {
		eswap_debug(ESD_ERR, "invalid memcg\n");
		return;
	}

	if (zram_test_flag(zram, index, ZRAM_WB)) {
		eswap_debug(ESD_ERR, "WB object, index = %u\n", index);
		return;
	}

	zram_set_memcg(zram, index, memcg);
	hh_list_add_tail(obj_idx(zram->area, index),
		mcg_idx(zram->area, memcg->id.id), zram->area->obj_table);

	size = zram_get_obj_size(zram, index);

	atomic_add(size, &est->zram_stored_size);
	atomic_inc(&est->zram_page_size);

	atomic64_add(size, &stat->zram_stored_bytes);
	atomic64_inc(&stat->zram_stored_pages);
}

void zram_lru_del(struct zram *zram, u32 index)
{
	struct mem_cgroup *memcg = NULL;
	struct memcg_estrack *est = NULL;
	unsigned long size;
	struct eswap_stat *stat = eswap_get_stat_obj();

	if (zram_test_flag(zram, index, ZRAM_WB)) {
		eswap_debug(ESD_ERR, "WB object, index = %u\n", index);
		return;
	}

	memcg = zram_get_memcg(zram, index);
	if (!memcg || !(est = mem_cgroup_estrack(memcg)))
		return;

	if (!est->zram || !est->zram->area)
		return;

	size = zram_get_obj_size(zram, index);
	hh_list_del(obj_idx(zram->area, index),
			mcg_idx(zram->area, memcg->id.id),
			zram->area->obj_table);
	zram_clear_memcg(zram, index);

	atomic_sub(size, &est->zram_stored_size);
	atomic_dec(&est->zram_page_size);

	atomic64_sub(size, &stat->zram_stored_bytes);
	atomic64_dec(&stat->zram_stored_pages);
}

void zram_rmap_insert(struct zram *zram, u32 index, unsigned long eswpentry)
{
	u32 ext_id = esentry_extid(eswpentry);
	hh_list_add_tail(obj_idx(zram->area, index),
			 ext_idx(zram->area, ext_id),
			 zram->area->obj_table);
}

void zram_rmap_erase(struct zram *zram, u32 index)
{
	unsigned long eswpentry;
	u32 ext_id;

#ifdef CONFIG_ZRAM_DEDUP
	eswpentry = zram_get_direct_handle(zram, index);
#else
	eswpentry = zram_get_handle(zram, index);
#endif
	ext_id = esentry_extid(eswpentry);
	hh_list_del(obj_idx(zram->area, index),
		ext_idx(zram->area, ext_id),
		zram->area->obj_table);
}
