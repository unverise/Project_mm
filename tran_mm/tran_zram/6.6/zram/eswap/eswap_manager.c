#include <linux/kernel.h>
#include <linux/swap.h>

#include "../zram_drv.h"
#include "eswap_list.h"
#include "eswap_area.h"
#include "eswap_lru_rmap.h"
#include "eswap_manager.h"
#include "memcg_control.h"

#define MOVE_TO_ESWAP_POINTS 5

static int init_io_extent_mgmt(struct io_extent_mgmt *io_ext,
	struct eswap_page_pool *pool, bool fast, bool nofail)
{
	int i;
	io_ext->ext_id = -EINVAL;
	io_ext->pool = pool;
	for (i = 0; i < EXTENT_PG_CNT; i++) {
		io_ext->pages[i] = eswap_alloc_page(pool, GFP_ATOMIC, fast, nofail);
		if (io_ext->pages[i])
			continue;

		// clean up
		while (i) {
			eswap_page_recycle(io_ext->pages[--i], pool);
		}

		eswap_debug(ESD_ERR, "alloc page[%d] failed\n", i);
		return -ENOMEM;
	}

	return 0;
}

static struct io_extent_mgmt *alloc_init_io_extent_mgmt(struct eswap_page_pool *pool, bool fast, bool nofail)
{
	int i;
	struct io_extent_mgmt *io_ext;

	// io_ext = eswap_malloc(sizeof(struct io_extent_mgmt), fast, nofail);
	io_ext = kzalloc(sizeof(struct io_extent_mgmt), GFP_NOWAIT);
	if (!io_ext) {
		eswap_debug(ESD_ERR, "alloc io_ext failed\n");
		return NULL;
	}

	io_ext->ext_id = -EINVAL;
	io_ext->pool = pool;
	for (i = 0; i < EXTENT_PG_CNT; i++) {
		io_ext->pages[i] = eswap_alloc_page(pool, GFP_ATOMIC, fast, nofail);
		if (io_ext->pages[i])
			continue;

		// clean up
		while (i) {
			eswap_page_recycle(io_ext->pages[--i], pool);
		}
		eswap_free(io_ext);

		eswap_debug(ESD_ERR, "alloc page[%d] failed\n", i);
		return NULL;
	}

	return io_ext;
}

static void discard_io_extent_mgmt(struct io_extent_mgmt *io_ext, enum eswap_scenario scen)
{
	struct zram *zram = NULL;
	int i;

	if (!io_ext->mcg)
		zram = io_ext->zram;
	else
		zram = MEMCG_ESTRACK_FIELD(io_ext->mcg, zram);
	if (!zram) {
		eswap_debug(ESD_ERR, "NULL zram\n");
		goto out;
	}

	for (i = 0; i < (int)EXTENT_PG_CNT; i++) {
		if (io_ext->pages[i]) {
			eswap_page_recycle(io_ext->pages[i], io_ext->pool);
		}
	}

	if (io_ext->ext_id < 0)
		goto out;

	eswap_debug(ESD_DEBUG, "ext = %d, scen = %d\n", io_ext->ext_id, scen);

	if (scen != ESWAP_OUT) {
		put_extent_to_memcg(zram->area, io_ext->ext_id);
		goto out;
	}

	for (i = 0; i < io_ext->cnt; i++) {
		u32 index = io_ext->index[i];

		zram_slot_lock(zram, index);
		if (zram_test_flag(zram, index, ZRAM_ESWAP_TRACK)) {
			/* only add to the lru when it is still tracked */
			if (io_ext->mcg) {
				zram_lru_add_tail(zram, index, io_ext->mcg);
			}
		}
		zram_clear_flag(zram, index, ZRAM_UNDER_WB);
		zram_slot_unlock(zram, index);
	}
	eswap_area_free_extent(zram->area, io_ext->ext_id);
out:
	/* in case of ESWAP_IN_FAULT, io_ext is on stack */
	if (scen != ESWAP_IN_FAULT)
		eswap_free(io_ext);
}

static void copy_to_pages(u8 *src, struct page *pages[], unsigned long eswpentry, int size)
{
	u8 *dst = NULL;
	int pg_id = esentry_pgid(eswpentry);
	int offset = esentry_pgoff(eswpentry);
	int first_pg_len = 0;

	if (!src) {
		eswap_debug(ESD_ERR, "NULL src\n");
		return;
	}
	if (!pages) {
		eswap_debug(ESD_ERR, "NULL pages\n");
		return;
	}
	if (size < 0 || size > (int)PAGE_SIZE) {
		eswap_debug(ESD_ERR, "size = %d invalid\n", size);
		return;
	}

	first_pg_len = PAGE_SIZE - offset;

	dst = page_to_virt(pages[pg_id]);
	if (size <= first_pg_len) {
		memcpy(dst + offset, src, size);
		return;
	}

	if (pg_id == EXTENT_PG_CNT - 1) {
		eswap_debug(ESD_ERR, "ext overflow, addr = %lx, size = %d\n",
			 eswpentry, size);
		return;
	}
	memcpy(dst + offset, src, first_pg_len);
	dst = page_to_virt(pages[pg_id + 1]);
	memcpy(dst, src + first_pg_len, size - first_pg_len);
}

static void copy_from_pages(u8 *dst, struct page *pages[], unsigned long eswpentry, int size)
{
	u8 *src = NULL;
	int pg_id = esentry_pgid(eswpentry);
	int offset = esentry_pgoff(eswpentry);
	int first_pg_len = 0;

	if (size < 0 || size > PAGE_SIZE) {
		eswap_debug(ESD_ERR, "size = %d invalid\n", size);
		return;
	}

	first_pg_len = PAGE_SIZE - offset;

	src = page_to_virt(pages[pg_id]);
	if (size <= first_pg_len) {
		memcpy(dst, src + offset, size);
		return;
	}

	if (unlikely(pg_id == EXTENT_PG_CNT - 1)) {
		eswap_debug(ESD_ERR, "ext overflow, addr = %lx, size = %d\n",
				eswpentry, size);
		return;
	}
	memcpy(dst, src + offset, first_pg_len);
	src = page_to_virt(pages[pg_id + 1]);
	memcpy(dst + first_pg_len, src, size - first_pg_len);
}

static bool zram_test_skip(struct zram *zram, u32 index, struct mem_cgroup *mcg)
{
	if (zram_test_flag(zram, index, ZRAM_WB))
		return true;
	if (zram_test_flag(zram, index, ZRAM_UNDER_WB))
		return true;
	if (zram_test_flag(zram, index, ZRAM_BATCHING_OUT))
		return true;
	if (zram_test_flag(zram, index, ZRAM_SAME))
		return true;
	if (mcg != zram_get_memcg(zram, index))
		return true;
	if (!zram_get_obj_size(zram, index))
		return true;

	if (!zram_test_flag(zram, index, ZRAM_ESWAP_TRACK))
		return true;

	return false;
}

static bool zram_test_overwrite(struct zram *zram, u32 index, int ext_id)
{
	unsigned long handle;

	if (!zram_test_flag(zram, index, ZRAM_WB))
		return true;
#ifdef CONFIG_ZRAM_DEDUP
	handle = zram_get_direct_handle(zram, index);
#else
	handle = zram_get_handle(zram, index);
#endif
	if (ext_id != esentry_extid(handle))
		return true;

	return false;
}

static int move_to_eswap_locked(struct zram *zram, u32 index,
	unsigned long eswpentry, struct mem_cgroup *mcg)
{
	int size;
	int preempt_cnt[MOVE_TO_ESWAP_POINTS];

	preempt_cnt[0] = preempt_count();

	size = zram_get_obj_size(zram, index);
	preempt_cnt[1] = preempt_count();
	zram_clear_flag(zram, index, ZRAM_UNDER_WB);
#ifdef CONFIG_ZRAM_DEDUP
	zram_free_handle(zram, index);
#else
	zs_free(zram->mem_pool, zram_get_handle(zram, index));
#endif
	preempt_cnt[2] = preempt_count();

	// TODO: do we really need this ?
	zram_set_memcg(zram, index, mcg);

	preempt_cnt[3] = preempt_count();
#ifdef CONFIG_ESWAP_IDLE_WRITEBACK
	if (zram_test_flag(zram, index, ZRAM_IDLE))
		memcg_idle_dec(zram, index);
#endif
	zram_set_flag(zram, index, ZRAM_WB);
	if (size == PAGE_SIZE)
		zram_set_flag(zram, index, ZRAM_HUGE);
	zram_set_element(zram, index, eswpentry);
	zram_rmap_insert(zram, index, eswpentry);
	preempt_cnt[4] = preempt_count();
	if (preempt_cnt[0] != preempt_cnt[4]) {
		int i;
		for (i = 0; i < MOVE_TO_ESWAP_POINTS; i++)
			eswap_debug(ESD_ERR, "[%d]:%d\n", i, preempt_cnt[i]);
		eswap_show_stack(current, NULL);
	}

	return size;
}

static int __move_to_zram(struct zram *zram, u32 index,
	unsigned long handle, struct io_extent_mgmt *io_ext)
{
	int size;
	struct mem_cgroup *memcg = io_ext->mcg;

	zram_slot_lock(zram, index);
	if (zram_test_overwrite(zram, index, io_ext->ext_id)) {
		zram_slot_unlock(zram, index);
		zs_free(zram->mem_pool, handle);
		return 0;
	}
	size = zram_get_obj_size(zram, index);
	zram_rmap_erase(zram, index);
	zram_set_handle(zram, index, handle);
#ifdef CONFIG_ESWAP_IDLE_WRITEBACK
	if (zram_test_flag(zram, index, ZRAM_IDLE))
		memcg_idle_inc(zram, index);
#endif
	zram_clear_flag(zram, index, ZRAM_WB);
	zram_lru_add_tail(zram, index, memcg);
	zram_set_flag(zram, index, ZRAM_FROM_ESWAP);
	zram_slot_unlock(zram, index);

	return size;
}

static int move_to_zram(struct zram *zram, u32 index, struct io_extent_mgmt *io_ext)
{
	unsigned long handle, eswpentry;
	int size, i;
	u8 *dst = NULL;

	if (index >= (u32)zram->area->nr_objs) {
		eswap_debug(ESD_ERR, "index = %u invalid\n", index);
		return -EINVAL;
	}

	zram_slot_lock(zram, index);
#ifdef CONFIG_ZRAM_DEDUP
	eswpentry = zram_get_direct_handle(zram, index);
#else
	eswpentry = zram_get_element(zram, index);
#endif
	if (zram_test_overwrite(zram, index, io_ext->ext_id)) {
		zram_slot_unlock(zram, index);
		return 0;
	}
	size = zram_get_obj_size(zram, index);
	zram_slot_unlock(zram, index);

	for (i = esentry_pgid(eswpentry) - 1; i >= 0 && io_ext->pages[i]; i--) {
		eswap_page_recycle(io_ext->pages[i], io_ext->pool);
		io_ext->pages[i] = NULL;
	}

	handle = zs_malloc(zram->mem_pool, size,  __GFP_DIRECT_RECLAIM | __GFP_KSWAPD_RECLAIM |
						__GFP_NOWARN | __GFP_HIGHMEM | 
						__GFP_MOVABLE | __GFP_CMA);
	if (!handle) {
		eswap_debug(ESD_ERR, "alloc handle failed, size = %d\n", size);
		return -ENOMEM;
	}
	dst = zs_map_object(zram->mem_pool, handle, ZS_MM_WO);
	copy_from_pages(dst, io_ext->pages, eswpentry, size);
	zs_unmap_object(zram->mem_pool, handle);

	size = __move_to_zram(zram, index, handle, io_ext);
	return size;
}

static void extent_unlock(struct io_extent_mgmt *io_ext)
{
	int extid, k = 0, sz = 0;
	struct mem_cgroup *memcg= NULL;
	struct zram *zram = NULL;
	unsigned long eswpentry;
	struct memcg_estrack *est;
	struct eswap_stat *stat = eswap_get_stat_obj();
	ktime_t now;

	memcg = io_ext->mcg;
	if (!memcg) {
		eswap_debug(ESD_ERR, "NULL memcg\n");
		goto out;
	}

	est = mem_cgroup_estrack(memcg);
	if (est->offline) {
		eswap_debug(ESD_ERR, "offline memcg\n");
		goto out;
	}

	zram = est->zram;
	if (!zram) {
		eswap_debug(ESD_ERR, "NULL zram\n");
		goto out;
	}

	extid = io_ext->ext_id;
	if (extid < 0 || est->in_swapin)
		goto out;

	eswap_debug(ESD_DEBUG, "add extid = %d, cnt = %d.\n", extid, io_ext->cnt);
	if (unlikely(io_ext->cnt > EXTENT_MAX_OBJ_CNT)) { // most pages in one extent
		eswap_debug(ESD_ERR, " extent items overflow %d\n", io_ext->cnt);
		eswap_show_stack(current, NULL);
	}

	eswpentry = ((unsigned long)extid) << EXTENT_SHIFT;
	now = (ktime_t)io_ext->private;
#if 1
	for (k = 0; k < io_ext->cnt; k++) {
		u32 index = io_ext->index[k];
		zram_slot_lock(zram, index);

		/* if we found any entry is untracked during eswap-out,
		 * which means this entry is not uptodate any more,
		 * treat this io_ext as invalid, roll everything back
		 */
		if (!zram_test_flag(zram, index, ZRAM_ESWAP_TRACK)) {
			while (k >= 0) {
				index = io_ext->index[k];
				zram_slot_unlock(zram, index);
				k--;
			}

			k = 0;
			goto out;
		}
	}

	/* all the entries in this extent is still tracked and locked
	 * so we can safely move it to the eswap
	 */
	for (k = 0; k < io_ext->cnt; k++) {
		int size;
		u32 index = io_ext->index[k];

		size = move_to_eswap_locked(zram, index, eswpentry, memcg);
		eswpentry += size;
		sz += size;

		zram_record_stay_time(zram, index, now);
	}

	/* all the entries are ZRAM_WB now */
	for (k = 0; k < io_ext->cnt; k++) {
		u32 index = io_ext->index[k];
		zram_slot_unlock(zram, index);
	}
#else
	for (k = 0; k < io_ext->cnt; k++) {
		int size;
		u32 index = io_ext->index[k];

		zram_slot_lock(zram, index);
		size = move_to_eswap_locked(zram, index, eswpentry, memcg);
		eswpentry += size;
		sz += size;
		zram_slot_unlock(zram, index);
	}
#endif
	put_extent_to_memcg(zram->area, extid);
	io_ext->ext_id = -EINVAL;

	/* statistics for the whole extent
	 * k pages moved back to zram, which takes size totally
	 */

	atomic64_inc(&est->eswap_out_extent_cnt);

	// for global
	atomic64_add(k, &stat->eswapout_pages);
	atomic64_add(k, &stat->eswap_stored_pages);
	atomic64_add(sz, &stat->eswap_stored_bytes);

	// for earea
	eswap_area_add_eswapout_pages(zram, extid, k);
	eswap_area_extent_counter_add(zram->area, extid, k, sz);

	// for memcg
	atomic_add(k, &MEMCG_ESTRACK_FIELD(memcg, eswap_stored_pages));
	atomic_add(sz, &MEMCG_ESTRACK_FIELD(memcg, eswap_stored_size));

	// for zram
	atomic64_sub(sz, &zram->stats.compr_data_size);
out:
	discard_io_extent_mgmt(io_ext, ESWAP_OUT);
	if (memcg)
		css_put(&memcg->css);
}

static void extent_add(struct io_extent_mgmt *io_ext, enum eswap_scenario scen)
{
	struct mem_cgroup *memcg = NULL;
	struct zram *zram = NULL;
	struct memcg_estrack *est;
	struct eswap_stat *stat = eswap_get_stat_obj();
	int extid, k = 0, size = 0, objs = 0;

	memcg = io_ext->mcg;
	if (!memcg) {
		eswap_debug(ESD_ERR, "NULL memcg for extid %d\n", io_ext->ext_id);
		goto out;
	}

	est = mem_cgroup_estrack(memcg);
	zram = est->zram;
	if (!zram) {
		eswap_debug(ESD_ERR, "NULL zram\n");
		goto out;
	}

	extid = io_ext->ext_id;
	if (extid < 0)
		goto out;

	io_ext->cnt = zram_rmap_get_extent_index(zram->area, extid, io_ext->index);
	eswap_debug(ESD_DEBUG, "extid = %d, cnt = %d.\n", extid, io_ext->cnt);
	for (k = 0; k < io_ext->cnt; k++) {
		int ret = move_to_zram(zram, io_ext->index[k], io_ext);

		/* if error happens inside, stop immdediately 
		 * but, we stiil need to do the math
		 */
		if (ret < 0)
			goto statistics;

		/* if ret == 0, which means override happens*/
		if (ret == 0) {
			eswap_debug(ESD_ERR, "override happened, index %d extent %d\n",
				io_ext->index[k], extid);
			atomic64_inc(&stat->override);
			continue;
		}

		objs++;
		size += ret;
		if (scen == ESWAP_IN_FAULT)
			zram_report_stay_time_for_pf(zram, io_ext->index[k]);
	}
	eswap_area_free_extent(zram->area, extid);
	io_ext->ext_id = -EINVAL;

	eswap_debug(ESD_DEBUG, "extent add OK, free extid = %d.\n", extid);

statistics:
	/* statistics for the whole extent
	 * objs pages moved back to zram, which takes size totally
	 */
	if (likely(!(memcg == root_mem_cgroup && io_ext->private == 1)))
		atomic64_inc(&est->eswap_in_extent_cnt);

	// for global
	atomic64_add(objs, &stat->eswapin_pages);
	atomic64_sub(objs, &stat->eswap_stored_pages);
	atomic64_sub(size, &stat->eswap_stored_bytes);
	if (scen == ESWAP_IN_FAULT)
		atomic64_add(objs, &stat->eswapin_pages_pf);

	// for memcg
	if (likely(!(memcg == root_mem_cgroup && io_ext->private == 1))) {
		atomic_sub(objs, &est->eswap_stored_pages);
		atomic_sub(size, &est->eswap_stored_size);
	}

	// for earea
	eswap_area_extent_counter_sub(zram->area, extid, objs, size);
	if (scen == ESWAP_IN_FAULT)
		eswap_area_add_eswapin_pages_pf(zram, extid, objs);

	// for zram
	atomic64_add(size, &zram->stats.compr_data_size);
out:
	discard_io_extent_mgmt(io_ext, scen);
	if (memcg)
		css_put(&memcg->css);
}

static void extent_clear(struct zram *zram, int ext_id)
{
	int *index = NULL;
	int cnt;
	int k;
	struct eswap_stat *stat = eswap_get_stat_obj();

	index = kzalloc(sizeof(int) * EXTENT_MAX_OBJ_CNT, GFP_NOIO);
	if (!index)
		index = kzalloc(sizeof(int) * EXTENT_MAX_OBJ_CNT,
				GFP_NOIO | __GFP_NOFAIL);

	cnt = zram_rmap_get_extent_index(zram->area, ext_id, index);

	for (k = 0; k < cnt; k++) {
		zram_slot_lock(zram, index[k]);
		if (zram_test_overwrite(zram, index[k], ext_id)) {
			zram_slot_unlock(zram, index[k]);
			continue;
		}
		zram_clear_memcg(zram, index[k]);
		zram_set_flag(zram, index[k], ZRAM_MCGID_CLEAR);
		atomic64_inc(&stat->mcgid_clear);
		zram_slot_unlock(zram, index[k]);
	}

	kfree(index);
}

static int shrink_entry(struct zram *zram, struct mem_cgroup *memcg,
	struct page *pages[], u32 index, unsigned long ext_off)
{
	unsigned long handle;
	int size;
	u8 *src = NULL;
	struct eswap_stat *stat = eswap_get_stat_obj();

	if (index >= (u32)zram->area->nr_objs) {
		eswap_debug(ESD_ERR, "index = %u invalid\n", index);
		return -EINVAL;
	}

#ifdef CONFIG_ESWAP_IDLE_WRITEBACK
	if (task_eswap_idle_reclaim(current) && !zram_test_flag(zram, index, ZRAM_IDLE))
		return -ENOENT;
#endif

	zram_slot_lock(zram, index);

#ifdef CONFIG_ZRAM_DEDUP
	handle = zram_get_direct_handle(zram, index);
#else
	handle = zram_get_handle(zram, index);
#endif
	if (!handle || zram_test_skip(zram, index, memcg)) {
		zram_slot_unlock(zram, index);
		return 0;
	}

	size = zram_get_obj_size(zram, index);
	if (ext_off + size > EXTENT_SIZE) {
		zram_slot_unlock(zram, index);
		return -ENOSPC;
	}

	/* check the target object ok, copy the content */
	src = zs_map_object(zram->mem_pool, handle, ZS_MM_RO);
	copy_to_pages(src, pages, ext_off, size);
	zs_unmap_object(zram->mem_pool, handle);

	/* delete the target object from zram lru, and mark it under wb*/
	zram_lru_del(zram, index);
	zram_set_flag(zram, index, ZRAM_UNDER_WB);
	if (zram_test_flag(zram, index, ZRAM_FROM_ESWAP)) {
		atomic64_inc(&stat->refault_pages);
		atomic64_add(size, &stat->refault_bytes);
	}
	zram_slot_unlock(zram, index);

	return size;
}

#define ONE_SHRINK_SIZE		(EXTENT_MAX_OBJ_CNT * 4)
#define USE_ONSTACK_SWAP_INDEX	(ONE_SHRINK_SIZE < PAGE_SIZE)

static int filter(void *data, int i)
{
	struct io_extent_mgmt *io_ext = (struct io_extent_mgmt *)data;
	struct zram_table_entry *zram_entry = &io_ext->zram->table[i];
	ktime_t current_ts = (ktime_t)io_ext->private;

	if (!zram_kernel_wb_actime_limit)
		return 1;

	s64 delta_time = ktime_ms_delta(current_ts, zram_entry->ac_time);
	if (delta_time < (zram_kernel_wb_actime_limit * 1000))
		return 0;

	zram_report_stay_time_for_wb(io_ext->zram, i, delta_time);
	return 1;
}

static int shrink_entry_list(struct io_extent_mgmt *io_ext)
{
	struct mem_cgroup *memcg = NULL;
	struct zram *zram = NULL;
	unsigned long stored_size;
	int swap_cnt, i;
	int swap_size = 0;
	int ret = 0;
	struct memcg_estrack *est;
#if USE_ONSTACK_SWAP_INDEX
	unsigned int swap_index[EXTENT_MAX_OBJ_CNT]; // takes 64 * 4, 256
#else
	unsigned int *swap_index;
#endif

	memcg = io_ext->mcg;
	est = mem_cgroup_estrack(memcg);
	zram = est->zram;
	eswap_debug(ESD_DEBUG, "memcg = %d\n", memcg->id.id);

#ifdef CONFIG_ESWAP_IDLE_WRITEBACK
	if (task_eswap_idle_reclaim(current))
		stored_size = atomic64_read(&est->zram_idle_size);
	else
		stored_size = (unsigned long)atomic_read(&est->zram_stored_size);
#else
	stored_size = (unsigned long)atomic_read(&est->zram_stored_size);
#endif
	eswap_debug(ESD_DEBUG, "zram_stored_size = %lu\n", stored_size);

	if (stored_size < EXTENT_SIZE) {
		ret = -ENOENT;
		goto err_out;
	}

#if !USE_ONSTACK_SWAP_INDEX
	swap_index = kzalloc(sizeof(unsigned int) * EXTENT_MAX_OBJ_CNT, GFP_NOIO);
	if (!swap_index) {
		ret = -ENOMEM;
		goto err_out;
	}
#endif

	ret = eswap_area_alloc_extent(zram->area, memcg);
	if (ret < 0) {
		goto err_alloc_area;
	}

	io_ext->ext_id = ret;

	swap_cnt = zram_get_memcg_coldest_index(zram, memcg, swap_index,
			EXTENT_MAX_OBJ_CNT, filter, io_ext);

	io_ext->cnt = 0;
	for (i = 0; i < swap_cnt && swap_size < EXTENT_SIZE; i++) {
		int size = shrink_entry(zram, io_ext->mcg, io_ext->pages, swap_index[i], swap_size);
		if (size < 0)
			break;

		if (size == 0)
			continue;

		swap_size += size;
		io_ext->index[io_ext->cnt++] = swap_index[i];
	}

	eswap_debug(ESD_DEBUG, "fill extent = %d, cnt = %d, overhead = %lu (%lu %d).\n",
		 io_ext->ext_id, io_ext->cnt, EXTENT_SIZE - swap_size, EXTENT_SIZE, swap_size);

	if (swap_size == 0) {
		ret = -ENOENT;
		goto err_shrink;
	}

#if !USE_ONSTACK_SWAP_INDEX
	/* get swap size successfully */
	kfree(swap_index);
#endif
	return swap_size;

err_shrink:
	eswap_area_free_extent(zram->area, io_ext->ext_id);
	io_ext->ext_id = -EINVAL;

err_alloc_area:
#if !USE_ONSTACK_SWAP_INDEX
	kfree(swap_index);
#endif

err_out:
	return ret;
}

/*
 * The function is used to initialize private member in memcg
 * memcg, zram, zram->zrea will be guaranted not null here!
 */
int memcg_estrack_init(struct mem_cgroup *memcg, struct zram *zram)
{
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);
	int midx = mcg_idx(zram->area, memcg->id.id);

	if (midx < 0) {
		eswap_debug(ESD_ERR, "invalid midx %d\n", memcg->id.id);
		return -EINVAL;
	}

	hh_list_init(midx, zram->area->obj_table);
	hh_list_init(midx, zram->area->ext_table);

	est->in_swapin = false;
	atomic_set(&est->zram_stored_size, 0);
	atomic_set(&est->zram_page_size, 0);
	atomic_set(&est->eswap_stored_pages, 0);
	atomic_set(&est->eswap_stored_size, 0);
	atomic64_set(&est->eswap_fault_cnt, 0);

	atomic64_set(&est->eswap_out_cnt, 0);
	atomic64_set(&est->eswap_in_cnt, 0);
	atomic64_set(&est->eswap_in_fault_cnt, 0);
	atomic64_set(&est->eswap_out_extent_cnt, 0);
	atomic64_set(&est->eswap_in_extent_cnt, 0);

	smp_wmb();
	est->zram = zram;

	/* even use wmb here, other user might still see
	 * zram inited but other-lenments not inited
	 */

	eswap_debug(ESD_DEBUG, "new memcg in zram, id = %d.\n", memcg->id.id);
	return 0;
}

void eswap_manager_memcg_deinit(struct mem_cgroup *mcg)
{
	struct zram *zram = NULL;
	struct eswap_area *area = NULL;
	struct eswap_stat *stat = eswap_get_stat_obj();
	struct memcg_estrack *est = mem_cgroup_estrack(mcg);
	int midx, last_index = -1;

	atomic64_inc(&stat->memcg_deinit_cnt);
	zram = est->zram;
	if (!zram || !zram->area) {
		eswap_debug(ESD_ERR, "invalid mcg %d\n", mcg->id.id);
		return;
	}

	area = zram->area;
	eswap_debug(ESD_DEBUG, "deinit mcg %d\n", mcg->id.id);

	midx = mcg_idx(area, mcg->id.id);
	if (midx < 0) {
		eswap_debug(ESD_ERR, "invalid midx %d\n", mcg->id.id);
		return;
	}

	while (1) {
		int index = get_memcg_zram_entry(area, mcg);
		if (index == -ENOENT)
			break;
		if (index < 0) {
			eswap_debug(ESD_ERR, "invalid index\n");
			return;
		}

		if (last_index == index) {
			eswap_debug(ESD_ERR, "dup index %d, mcg[%s] head %lx\n",
					index, est->name, est->zram_lru);
			eswap_show_stack(current, NULL);
		}

		zram_slot_lock(zram, index);
		if (index == last_index || mcg == zram_get_memcg(zram, index)) {
			hh_list_del(obj_idx(area, index), midx, area->obj_table);
			zram_clear_memcg(zram, index);
			zram_set_flag(zram, index, ZRAM_MCGID_CLEAR);
			atomic64_inc(&stat->mcgid_clear);
		}
		zram_slot_unlock(zram, index);
		last_index = index;
	}

	do {
		int extid = fetch_extent_from_memcg(area, mcg, NULL);
		if (extid < 0)
			break;

		extent_clear(zram, extid);
		hh_list_set_mcgid(ext_idx(area, extid), area->ext_table, 0);
		put_extent_to_memcg(area, extid);
	} while (true);

	eswap_debug(ESD_DEBUG, "deinit mcg %d, extent done\n", mcg->id.id);
}

static void eswap_extent_del_obj(struct zram *zram, u32 index);

/* The function is used to del ZS object from ZRAM LRU list
 * zram index has been locked
 */
void memcg_estrack_zram_lru_del(struct zram *zram, u32 index)
{
	struct eswap_stat *stat = eswap_get_stat_obj();

	zram_clear_flag(zram, index, ZRAM_FROM_ESWAP);

	if (zram_test_flag(zram, index, ZRAM_MCGID_CLEAR)) {
		zram_clear_flag(zram, index, ZRAM_MCGID_CLEAR);
		atomic64_dec(&stat->mcgid_clear);
	}

	if (zram_test_flag(zram, index, ZRAM_WB)) {
		eswap_extent_del_obj(zram, index);
		zram_rmap_erase(zram, index);
		zram_clear_flag(zram, index, ZRAM_WB);
		zram_clear_memcg(zram, index);
		zram_set_handle(zram, index, 0);

		/* we has set handle to 0 here, so dec pages_stored
		 * because zram_free_page will do the following:
		 *
		 *      if (!handle)
		 *              return;
		 * before
		 *      atomic64_dec(&zram->stats.pages_stored);
		 *      zram_set_handle(zram, index, 0);
		 *      zram_set_obj_size(zram, index, 0);
		 */
		atomic64_dec(&zram->stats.pages_stored);
	} else {
#ifdef CONFIG_ESWAP_IDLE_WRITEBACK
		if (zram_test_flag(zram, index, ZRAM_IDLE))
			memcg_idle_dec(zram, index);
#endif
		zram_lru_del(zram, index);
	}
}

/*
 * The function is used to alloc a new extent,
 * then reclaim ZRAM by LRU list to the new extent.
 */
struct io_extent_mgmt *io_extent_mgmt_create(struct mem_cgroup *memcg,
	struct eswap_page_pool *pool, unsigned long *to_reclaimed, ktime_t now)
{
	struct io_extent_mgmt *io_ext = NULL;
	struct zram *zram = NULL;
	struct eswap_area *area = NULL;
	int reclaim_size, ret;

	io_ext = alloc_init_io_extent_mgmt(pool, false, true);
	if (!io_ext)
		return ERR_PTR(-ENOMEM);

	io_ext->private = (unsigned long)now;

	io_ext->mcg = memcg;
	io_ext->zram = MEMCG_ESTRACK_FIELD(memcg, zram);
	zram = io_ext->zram;
	area = io_ext->zram->area;
	ret = shrink_entry_list(io_ext);
	if (ret < 0) {
		discard_io_extent_mgmt(io_ext, ESWAP_OUT);
		return ERR_PTR(ret);
	}
	reclaim_size = ret;

	css_get(&memcg->css);

	eswap_debug(ESD_DEBUG, "memcg = %d, extid = %d\n", memcg->id.id, io_ext->ext_id);

	*to_reclaimed = reclaim_size;
	return io_ext;
}

/*
 * The function will be called when eswap write done.
 * The function should register extent to eswap manager.
 */
void io_extent_mgmt_register(struct io_extent_mgmt *io_ext)
{
	eswap_debug(ESD_DEBUG, "ext_id = %d\n", io_ext->ext_id);
	extent_unlock(io_ext);
}

/* If extent empty, return false, otherise return true. */
static void eswap_extent_del_obj(struct zram *zram, u32 index)
{
	int ext_id;
	struct mem_cgroup *mcg = NULL;
	unsigned long eswpentry;
	int size;
	struct eswap_stat *stat = eswap_get_stat_obj();
	struct memcg_estrack *est = NULL;

#ifdef CONFIG_ZRAM_DEDUP
	eswpentry = zram_get_direct_handle(zram, index);
#else
	eswpentry = zram_get_handle(zram, index);
#endif

	size = zram_get_obj_size(zram, index);
	ext_id = esentry_extid(eswpentry);

	atomic64_sub(size, &stat->eswap_stored_bytes);
	atomic64_dec(&stat->eswap_stored_pages);
	mcg = zram_get_memcg(zram, index);
	if (mcg) {
		est = mem_cgroup_estrack(mcg);
		atomic_sub(size, &est->eswap_stored_size);
		atomic_dec(&est->eswap_stored_pages);
	}

	if (!eswap_area_extent_counter_dec_test_empty(zram->area, ext_id, size))
		return;

	/* 
	 * all objs are deleted from the extent, now, we need to:
	 * 1. del an extent node from the memcg ext_lru list
	 * 2. handle area's extent bitmap
	 * 3. clear the extent's mcg id
	 * 4. accounting handling
	 */
	ext_id = fetch_extent_from_memcg_by_extid(zram->area, ext_id);
	if (ext_id < 0)
		return;

	atomic64_inc(&stat->notify_free);
	if (mcg) {
		atomic64_inc(&est->eswap_ext_notify_free);
	}

	eswap_debug(ESD_DEBUG, "free ext_id = %d\n", ext_id);
	eswap_area_free_extent(zram->area, ext_id);
}

/* The function will be called when eswap execute fault out. */
struct io_extent_mgmt *eswap_extent_find_mgmt_by_idx(unsigned long eswpentry,
	struct zram *zram, struct io_extent_mgmt *io_ext)
{
	int extid, ret;

	ret = fetch_extent_from_memcg_by_extid(zram->area, esentry_extid(eswpentry));
	if (ret < 0) {
		goto err;
	}
	extid = ret;

	if (io_ext) {
		ret = init_io_extent_mgmt(io_ext, NULL /*pool*/, true, true);
		if (ret)
			goto err_fetched;
	} else {
		io_ext = alloc_init_io_extent_mgmt(NULL, true, true);
		if (!io_ext) {
			eswap_debug(ESD_ERR, "io_ext alloc failed\n");
			ret = -ENOMEM;
			goto err_fetched;
		}
	}

	io_ext->ext_id = extid;
	io_ext->zram = zram;
	io_ext->mcg = get_memcg_with_css_get(
		hh_list_get_mcgid(ext_idx(zram->area, extid), zram->area->ext_table));
	if (io_ext->mcg == NULL) {
		io_ext->mcg = root_mem_cgroup;

		/* pin the memcg */
		css_get(&root_mem_cgroup->css);

		/* use privtae to tell we lost memcg info in the pf path */
		io_ext->private = 1;

		eswap_debug(ESD_ERR, "find memcg error, ext = %d\n", extid);
	}
	eswap_debug(ESD_DEBUG, "get entry = %lx ext = %d\n", eswpentry, extid);
	return io_ext;

err_fetched:
	put_extent_to_memcg(zram->area, extid);
err:
	return ERR_PTR(ret);
}

/* The function will be called when eswap executes batch out. */
struct io_extent_mgmt *eswap_extent_find_mgmt_by_memcg(struct mem_cgroup *memcg,
	struct eswap_page_pool *pool)
{
	int extid, ret;
	struct io_extent_mgmt *io_ext = NULL;
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);

	/* delete ext node from memcg */
	ret = fetch_extent_from_memcg(est->zram->area, memcg, NULL);
	if (ret < 0) {
		return ERR_PTR(ret);
	}
	extid = ret;

	/* new a io_extent_mgmt */
	// io_ext = alloc_init_io_extent_mgmt(pool, true, false);
	io_ext = alloc_init_io_extent_mgmt(pool, false, true);
	if (!io_ext) {
		put_extent_to_memcg(est->zram->area, extid);
		eswap_debug(ESD_ERR, "io_ext alloc failed\n");
		return ERR_PTR(-ENOMEM);
	}

	/* init io_extent_mgmt */
	io_ext->ext_id = extid;
	io_ext->mcg = memcg;
	io_ext->zram = est->zram;

	/* pin the memcg */
	css_get(&memcg->css);

	eswap_debug(ESD_DEBUG, "get memcg = %d, ext = %d\n", memcg->id.id, extid);
	return io_ext;
}

/*
 * The function will be called when eswap read done.
 * The function should extra the extent to ZRAM, then destroy
 */
void io_extent_mgmt_unregister(struct io_extent_mgmt *io_ext, enum eswap_scenario scenario)
{
	eswap_debug(ESD_DEBUG, "extid = %d\n", io_ext->ext_id);
	extent_add(io_ext, scenario);
}

/*
 * The function will be called
 * when schedule meets exception proceeding extent
 */
void io_extent_mgmt_exception(struct io_extent_mgmt *io_ext, enum eswap_scenario scenario)
{
	struct mem_cgroup *memcg = io_ext->mcg;

	eswap_debug(ESD_DEBUG, "ext_id = %d, scen = %d\n", io_ext->ext_id, scenario);
	discard_io_extent_mgmt(io_ext, scenario);
	if (memcg)
		css_put(&memcg->css);
}

#ifdef CONFIG_ESWAP_IDLE_WRITEBACK
void memcg_idle_inc(struct zram *zram, u32 index)
{
	struct mem_cgroup *mcg = zram_get_memcg(zram, index);
	int size = zram_get_obj_size(zram, index);

	if (!mcg)
		return;

	atomic64_inc(&MEMCG_ESTRACK_FIELD(mcg, zram_idle_page));
	atomic64_add(size, &MEMCG_ESTRACK_FIELD(mcg, zram_idle_size));
}

void memcg_idle_dec(struct zram *zram, u32 index)
{
	struct mem_cgroup *mcg = zram_get_memcg(zram, index);
	int size = zram_get_obj_size(zram, index);

	if (!mcg)
		return;

	atomic64_dec(&MEMCG_ESTRACK_FIELD(mcg, zram_idle_page));
	atomic64_sub(size, &MEMCG_ESTRACK_FIELD(mcg, zram_idle_size));
}

void memcg_idle_count(struct zram *zram)
{
	struct mem_cgroup *mcg = NULL;

	eswap_debug(ESD_ERR, "==========start============\n");
	for (mcg = get_next_memcg(NULL); mcg; mcg = get_next_memcg(mcg))
		eswap_debug(ESD_ERR,
			"[%d:%s] idle = %lld, zfault = %lld, hfault = %lld\n",
			mcg->id.id,
			MEMCG_ESTRACK_FIELD(mcg, name),
			atomic64_read(&MEMCG_ESTRACK_FIELD(mcg, zram_idle_page)),
			atomic64_read(&MEMCG_ESTRACK_FIELD(mcg, eswap_fault_cnt)),
			atomic64_read(&MEMCG_ESTRACK_FIELD(mcg, eswap_in_fault_cnt)));
	eswap_debug(ESD_ERR, "==========end============\n");
}
#endif

u64 eswap_get_original_size(void)
{
	struct eswap_stat *stat = eswap_get_stat_obj();
	return atomic64_read(&stat->eswap_stored_pages) << PAGE_SHIFT;
}

u64 eswap_get_compress_size(void)
{
	struct eswap_stat *stat = eswap_get_stat_obj();
	return atomic64_read(&stat->eswap_stored_bytes);
}
