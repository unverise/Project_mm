#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/zsmalloc.h>
#include <linux/vmalloc.h>

#include "eswap_area.h"
#include "memcg_control.h"

/* we use this mutex to synchronize:
 * extra_area adding and bitmap allocating from extra area
 */
static DEFINE_MUTEX(extra_area_add_mutex);

struct mem_cgroup *get_memcg_with_css_get(unsigned short mcg_id)
{
	struct mem_cgroup *memcg = NULL;

	rcu_read_lock();
	do {
		memcg = mem_cgroup_from_id(mcg_id);
		if (!memcg)
			break;
	} while (!css_tryget(&memcg->css));
	rcu_read_unlock();

	return memcg;
}

struct mem_cgroup *get_mem_cgroup(unsigned short mcg_id)
{
	struct mem_cgroup *memcg = NULL;

	rcu_read_lock();
	memcg = mem_cgroup_from_id(mcg_id);
	rcu_read_unlock();

	return memcg;
}

static int extra_area_extent_bit2id(struct eswap_extra_area *area, int bit)
{
	if (bit < 0 || bit >= area->nr_exts) {
		eswap_debug(ESD_ERR, "bit = %d invalid\n", bit);
		return -EINVAL;
	}
	eswap_debug(ESD_DEBUG, "bit = %d valid\n", bit);

	return area->nr_exts - bit - 1;
}

static int extra_area_extent_id2bit(struct eswap_extra_area *area, int id)
{
	if (id < 0 || id >= area->nr_exts) {
		eswap_debug(ESD_ERR, "id = %d invalid\n", id);
		return -EINVAL;
	}

	return area->nr_exts - id - 1;
}

int obj_idx(struct eswap_area *area, int id)
{
	if (likely(area != NULL && id >= 0 && id < area->nr_objs))
		return id;

	if (!area)
		eswap_debug(ESD_ERR, "NULL area\n");
	else
		eswap_debug(ESD_ERR, "id = %d invalid\n", id);

	return -EINVAL;
}

int ext_idx(struct eswap_area *area, int id)
{
	if (likely(area)) {
		struct eswap_extra_area *earea = area->extra_areas[id >> area->area_base_bits];
		int extra_extid = id - earea->area_id_offset;
		if (extra_extid >= 0 && extra_extid < earea->nr_exts)
			return id + area->nr_objs + area->nr_mcgs;
	}

	if (!area)
		eswap_debug(ESD_ERR, "NULL area\n");
	else
		eswap_debug(ESD_ERR, "id = %d invalid\n", id);

	return -EINVAL;
}

/* translate memcg.id.id to eswap_area's idx
 * NOTE: for memcg.id.id, 0 is invalid
 */
int mcg_idx(struct eswap_area *area, int id)
{
	if (likely(area != NULL && id > 0 && id < area->nr_mcgs))
		return id + area->nr_objs;
	
	if (!area)
		eswap_debug(ESD_ERR, "NULL area\n");
	else
		eswap_debug(ESD_ERR, "id = %d invalid\n", id);

	return -EINVAL;
}

static struct hh_list_head *get_obj_table_node(int idx, void *private)
{
	struct eswap_area *area = private;
	struct eswap_extra_area *earea;

	if (unlikely(!area || idx < 0)) {
		eswap_debug(ESD_ERR, "NULL area or id %d\n", idx);
		return NULL;
	}

	if (idx < area->nr_objs)
		return &area->lru[idx];

	idx -= area->nr_objs;
	if (idx == 0)
		goto err_out;

	if (idx < area->nr_mcgs) {
		struct mem_cgroup *memcg = get_mem_cgroup(idx);
		struct memcg_estrack *est;

		if (!memcg)
			goto err_out;

		est = mem_cgroup_estrack(memcg);
		return (struct hh_list_head *)(&est->zram_lru);
	}

	idx -= area->nr_mcgs;

	earea = area->extra_areas[idx >> area->area_base_bits];
	idx -= earea->area_id_offset;
	if (idx >= 0 && idx < earea->nr_exts)
		return &earea->rmap[idx];

err_out:
	eswap_debug(ESD_ERR, "idx = %d invalid\n", idx);
	return NULL;
}

static int init_obj_list_table_extra(struct eswap_extra_area *earea)
{
	int i;
	struct eswap_area *area = earea->main; 

	earea->rmap = vzalloc(sizeof(struct hh_list_head) * earea->nr_exts);
	if (!earea->rmap)
		return -ENOMEM;

	for (i = 0; i < earea->nr_exts; i++)
		hh_list_init(i + earea->area_id_offset + area->nr_objs + area->nr_mcgs, area->obj_table);

	return 0;
}

static struct hh_list_head *get_ext_table_node(int idx, void *private)
{
	struct eswap_area *area = private;
	struct eswap_extra_area *earea;

	if (unlikely(!area)) {
		eswap_debug(ESD_ERR, "NULL area\n");
		return NULL;
	}

	if (idx < area->nr_objs)
		goto err_out;

	idx -= area->nr_objs;
	if (idx == 0)
		goto err_out;

	if (idx < area->nr_mcgs) {
		struct mem_cgroup *memcg = get_mem_cgroup(idx);
		struct memcg_estrack *est;

		if (!memcg)
			return NULL;
		est = mem_cgroup_estrack(memcg);
		return (struct hh_list_head *)(&est->ext_lru);
	}

	idx -= area->nr_mcgs;

	earea = area->extra_areas[idx >> area->area_base_bits];
	idx -= earea->area_id_offset;
	if (idx >= 0 && idx < earea->nr_exts)
		return &earea->ext[idx];
err_out:
	eswap_debug(ESD_ERR, "idx = %d invalid\n", idx);
	return NULL;
}

static int init_ext_list_table_extra(struct eswap_extra_area *earea)
{
	int i;
	struct eswap_area *area = earea->main; 

	earea->ext = vzalloc(sizeof(struct hh_list_head) * earea->nr_exts);
	if (!earea->ext)
		return -ENOMEM;

	for (i = 0; i < earea->nr_exts; i++)
		hh_list_init(i + earea->area_id_offset + area->nr_objs + area->nr_mcgs, area->ext_table);

	return 0;
}

static void free_extra_area(struct eswap_extra_area *area)
{
	vfree(area->bitmap);
	vfree(area->ext_stored_pages);
	// vfree(area->ext_stored_size);
	
	vfree(area->rmap);
	area->rmap = NULL;

	vfree(area->ext);
	area->ext = NULL;

	vfree(area);
}

static struct eswap_extra_area *add_extra_area(struct block_device *bdev, struct file *fp,
	unsigned long comp_size, struct eswap_area *main)
{
	struct eswap_extra_area *area = NULL;
	int id = main->extra_areas_cnt;

	if (comp_size & (EXTENT_SIZE - 1)) {
		eswap_debug(ESD_ERR, "disksize = %lu align invalid (32K align needed)\n", comp_size);
		goto err_out;
	}

	area = vzalloc(sizeof(struct eswap_extra_area));
	if (!area) {
		eswap_debug(ESD_ERR, "area alloc failed\n");
		goto err_out;
	}

	area->main = main;
	area->area_id_offset = id * (1 << main->area_base_bits);

	area->size = comp_size;
	area->nr_exts = (int)comp_size >> EXTENT_SHIFT;
	area->bitmap = vzalloc(BITS_TO_LONGS(area->nr_exts) * sizeof(long));

	/* skip 0 bit to confuse zram.handle = 0 */
	atomic_set(&area->last_alloc_bit, 1);

	area->bdev = bdev;
	if (!area->bitmap) {
		eswap_debug(ESD_ERR, "area->bitmap alloc failed\n");
		goto err_free;
	}

	area->ext_stored_pages = (atomic_t*)vzalloc(sizeof(atomic_t) * area->nr_exts);
	if (!area->ext_stored_pages) {
		eswap_debug(ESD_ERR, "area->ext_stored_pages alloc failed\n");
		goto err_free;
	}

	// in order to init ext and rmap list 
	main->extra_areas[id] = area;

	//area->ext_stored_size = vzalloc(sizeof(atomic_t) * area->nr_exts);
	//if (!area->ext_stored_size) {
	//	eswap_debug(ESD_ERR, "area->ext_stored_size alloc failed\n");
	//	goto err_free;
	//}

	if (init_obj_list_table_extra(area)) {
		eswap_debug(ESD_ERR, "init obj list table failed\n");
		goto err_free;
	}

	if (init_ext_list_table_extra(area)) {
		eswap_debug(ESD_ERR, "init ext list table failed\n");
		goto err_free;
	}

	area->fp_ori = fp;
	main->extra_areas_cnt++;
	eswap_debug(ESD_INFO, "eswap_extra_area add %d OK.\n", id);
	return area;

err_free:
	free_extra_area(area);
err_out:
	eswap_debug(ESD_ERR, "eswap_extra_area init failed.\n");
	return NULL;
}

struct eswap_area *alloc_eswap_area(struct block_device *bdev, struct file *fp,
	unsigned long ori_size, unsigned long comp_size)
{
	struct eswap_area *area;
	struct eswap_extra_area *earea;
	int i, area_base_bits;
	unsigned long nr_exts = 0;

	if (comp_size & (EXTENT_SIZE - 1)) {
		eswap_debug(ESD_ERR, "disksize = %lu align invalid (32K align needed)\n", comp_size);
		goto err_out;
	}

	nr_exts = comp_size >> EXTENT_SHIFT;
	if (nr_exts == 0)
		return NULL;

	area_base_bits = 0;
	while (nr_exts) {
		area_base_bits++;
		nr_exts >>= 1;
	}

	area = vzalloc(sizeof(struct eswap_area));
	if (!area) {
		eswap_debug(ESD_ERR, "area alloc failed\n");
		goto err_out;
	}

	area->area_base_bits = area_base_bits;
	area->nr_mcgs = MEM_CGROUP_ID_MAX;
	area->nr_objs = ori_size >> PAGE_SHIFT;

	area->lru = (struct hh_list_head*)vzalloc(sizeof(struct hh_list_head) * area->nr_objs);
	if (!area->lru) {
		eswap_debug(ESD_ERR, "area lru failed\n");
		goto err_free_area;
	}

	area->obj_table = alloc_table(get_obj_table_node, area);
	if (!area->obj_table) {
		eswap_debug(ESD_ERR, "area obj table failed\n");
		goto err_free_lru;
	}

	for (i = 0; i < area->nr_objs; i++)
		hh_list_init(obj_idx(area, i), area->obj_table);

	area->ext_table = alloc_table(get_ext_table_node, area);
	if (!area->ext_table) {
		eswap_debug(ESD_ERR, "area ext table failed\n");
		goto err_free_obj_table;
	}

	earea = add_extra_area(bdev, fp, comp_size, area);
	if (earea == NULL) {
		eswap_debug(ESD_ERR, "area extra failed\n");
		goto err_free_ext_table;
	}

	eswap_debug(ESD_INFO, "eswap_area init OK.\n");
	return area;

err_free_ext_table:
	kfree(area->ext_table);
err_free_obj_table:
	kfree(area->obj_table);
err_free_lru:
	vfree(area->lru);
err_free_area:
	vfree(area);
err_out:
	eswap_debug(ESD_ERR, "eswap_area init failed.\n");
	return NULL;
}

int add_eswap_extra_area(struct eswap_area *area, struct block_device *bdev,
	struct file *fp, unsigned long comp_size)
{
	struct eswap_extra_area *earea;

	if (area == NULL || area->extra_areas_cnt >= MAX_EXTRA_AREAS)
		return -1;

	mutex_lock(&extra_area_add_mutex);
	earea = add_extra_area(bdev, fp, comp_size, area);
	mutex_unlock(&extra_area_add_mutex);

	if (earea == NULL)
		return -1;

	eswap_debug(ESD_INFO, "eswap_area init OK.\n");
	return 0;
}

static void eswap_area_free_extent_to_extra(struct eswap_extra_area *area, int extid)
{
	int eidx;
	int off = area->area_id_offset;
	struct eswap_area *main = area->main;

	if (!area) {
		eswap_debug(ESD_ERR, "NULL area\n");
		return;
	}

	if (extid - off < 0 || extid - off >= area->nr_exts) {
		eswap_debug(ESD_ERR, "INVALID ext %d\n", extid);
		return;
	}

	eswap_debug(ESD_DEBUG, "free ext id = %d.\n", extid);

	eidx = ext_idx(main, extid);
	hh_list_set_mcgid(eidx, main->ext_table, 0);

	if (!test_and_clear_bit(extra_area_extent_id2bit(area, extid - off), area->bitmap)) {

		eswap_debug(ESD_ERR, "bit not set, local ext = %d, area %d, off = %d\n",
					extid - off, extid >> main->area_base_bits, off);
		WARN_ON_ONCE(1);
	}

	atomic_dec(&area->stored_exts);
	eswap_used_exts_num_dec();
}

void eswap_area_free_extent(struct eswap_area *area, int extid)
{
	int area_id = extid >> area->area_base_bits;
	return eswap_area_free_extent_to_extra(area->extra_areas[area_id], extid);
}

static int alloc_bitmap(unsigned long *bitmap, int max, int last_bit)
{
	int bit;

	/* skip 0 bit to confuse zram.handle = 0 */
	const int start_bit = 1;
retry:
	bit = find_next_zero_bit(bitmap, max, last_bit);
	if (bit == max) {
		if (last_bit == start_bit) {
			return -ENOSPC;
		}
		last_bit = start_bit;
		goto retry;
	}
	if (test_and_set_bit(bit, bitmap))
		goto retry;

	return bit;
}

static int eswap_area_alloc_extent_from_extra(struct eswap_extra_area *area, struct mem_cgroup *memcg)
{
	int last_bit, bit;
	int extid, mcgid;
	int eidx;

	last_bit = atomic_read(&area->last_alloc_bit);
	eswap_debug(ESD_DEBUG, "last_bit = %d.\n", last_bit);

	bit = alloc_bitmap(area->bitmap, area->nr_exts, last_bit);
	if (bit < 0) {
		eswap_debug(ESD_WARN, "alloc bitmap failed, bit = %d\n", bit);
		return bit;
	}

	extid = extra_area_extent_bit2id(area, bit);
	extid += area->area_id_offset;

	eidx = ext_idx(area->main, extid);
	mcgid = hh_list_get_mcgid(eidx, area->main->ext_table);
	if (mcgid) {
		eswap_debug(ESD_WARN, "already has memcg %d, ext %d\n", mcgid, extid);
		goto err_out;
	}
	hh_list_set_mcgid(eidx, area->main->ext_table, memcg->id.id);

	atomic_set(&area->last_alloc_bit, bit);

	atomic_inc(&area->stored_exts);
	eswap_used_exts_num_inc();

	eswap_debug(ESD_DEBUG, "mcgid = %d, ext id = %d\n", memcg->id.id, extid);
	return extid;

err_out:
	clear_bit(bit, area->bitmap);
	WARN_ON_ONCE(1);
	return -EBUSY;
}

int eswap_area_alloc_extent(struct eswap_area *area, struct mem_cgroup *memcg)
{
	int i, extid;
	int check_extra, err = -ENOSPC;
	struct eswap_extra_area *earea;

	/* for extra areas which index > 0, they are adding dynamically
	 * so we use extra_area_add_mutex to avoid racing
	 * might not be efficient but safe comes first
	 */
	for (i = 0; i < MAX_EXTRA_AREAS; i++) {
		check_extra = (i > 0);

		if (check_extra && !mutex_trylock(&extra_area_add_mutex)) {
			err = -ENOSPC;
			break;
		}

		earea = area->extra_areas[i];

		// no more extra area available
		if (earea == NULL) {
			if (check_extra)
				mutex_unlock(&extra_area_add_mutex);

			eswap_debug(ESD_WARN, "alloc bitmap failed for area %d.\n", i);
			err = -ENOSPC;
			break;
		}

		extid = eswap_area_alloc_extent_from_extra(earea, memcg);

		if (check_extra)
			mutex_unlock(&extra_area_add_mutex);

		if (extid >= 0)
			return extid;
	}

	return err;
}

/* del an extent node from the memcg ext_lru list */
int fetch_extent_from_memcg_by_extid(struct eswap_area *area, int extid)
{
	int mcgid, eidx;

	if (!area || !area->ext_table) {
		eswap_debug(ESD_ERR, "NULL area or table\n");
		return -EINVAL;
	}

	eidx = ext_idx(area, extid);
	
	if (eidx < 0) {
		eswap_debug(ESD_ERR, "ext = %d invalid\n", extid);
		return -EINVAL;
	}

	if (!hh_list_clear_priv(eidx, area->ext_table))
		return -EBUSY;

	mcgid = hh_list_get_mcgid(eidx, area->ext_table);
	if (mcgid) {
		int midx = mcg_idx(area, mcgid);
		struct eswap_stat *stat = eswap_get_stat_obj();
		if (midx < 0)
			return -EINVAL;

		hh_lock_list(midx, area->ext_table);
		atomic64_dec(&stat->ext_cnt);
		area->mcg_id_cnt[mcgid]--;
		if (area->mcg_id_cnt[mcgid] == 0)
			atomic64_dec(&stat->memcg_cnt);

		hh_list_del_nolock(eidx, midx, area->ext_table);
		hh_unlock_list(midx, area->ext_table);
	}

	eswap_debug(ESD_DEBUG, "ext id = %d\n", extid);
	return extid;
}

/* add an extent node into the memcg ext_lru list */
void put_extent_to_memcg(struct eswap_area *area, int extid)
{
	int mcgid;
	int eidx;

	if (!area) {
		eswap_debug(ESD_ERR, "NULL area\n");
		return;
	}

	eidx = ext_idx(area, extid);
	if (eidx < 0) {
		eswap_debug(ESD_ERR, "ext = %d invalid\n", extid);
		return;
	}

	mcgid = hh_list_get_mcgid(eidx, area->ext_table);
	if (mcgid) {
		int midx = mcg_idx(area, mcgid);
		struct eswap_stat *stat = eswap_get_stat_obj();
		if (midx < 0)
			return;

		hh_lock_list(midx, area->ext_table);

		hh_list_add_nolock(eidx, midx, area->ext_table);

		atomic64_inc(&stat->ext_cnt);
		if (area->mcg_id_cnt[mcgid] == 0)
			atomic64_inc(&stat->memcg_cnt);
		area->mcg_id_cnt[mcgid]++;

		hh_unlock_list(midx, area->ext_table);
	}

	if (!hh_list_set_priv(eidx, area->ext_table)) {
		eswap_debug(ESD_ERR, "private not set, ext = %d\n", extid);
		WARN_ON_ONCE(1);
		return;
	}

	eswap_debug(ESD_DEBUG, "put extent %d.\n", extid);
}

int fetch_extent_from_memcg(struct eswap_area *area, struct mem_cgroup *memcg,
			bool (*filter)(struct eswap_area *area, int extid))
{
	int mcgid, extid = -ENOENT, local_extid = -1;
	int midx, eidx;
	struct eswap_extra_area *earea;

	if (!area || !area->ext_table) {
		eswap_debug(ESD_ERR, "NULL area or table\n");
		return -EINVAL;
	}

	if (!memcg) {
		eswap_debug(ESD_ERR, "NULL memcg\n");
		return -EINVAL;
	}

	mcgid = memcg->id.id;
	midx = mcg_idx(area, mcgid);
	if (midx < 0)
		return -EINVAL;

	hh_lock_list(midx, area->ext_table);
	if (memcg != get_mem_cgroup(mcgid)) {
		struct memcg_estrack *est = mem_cgroup_estrack(memcg);
		extid = ((struct hh_list_head*)(&est->ext_lru))->next;
		extid -= (area->nr_objs + area->nr_mcgs);
		eswap_debug(ESD_ERR, "memcg corrupted, get extid %d directly.\n", extid);

		earea = area->extra_areas[extid >> area->area_base_bits];
		local_extid = extid - earea->area_id_offset;
		if (local_extid < 0 || local_extid >= earea->nr_exts)
			extid = -ENOENT;
		goto unlock;
	}

	hh_list_for_each_entry(eidx, midx, area->ext_table) {
		if (filter && !filter(area, eidx - area->nr_objs - area->nr_mcgs))
			continue;

		if (hh_list_clear_priv(eidx, area->ext_table)) {
			extid = eidx -(area->nr_objs + area->nr_mcgs);

			earea = area->extra_areas[extid >> area->area_base_bits];
			local_extid = extid - earea->area_id_offset;
			if (local_extid >= 0 && local_extid < earea->nr_exts) {
				struct eswap_stat *stat = eswap_get_stat_obj();
				atomic64_dec(&stat->ext_cnt);
				area->mcg_id_cnt[mcgid]--;
				if (area->mcg_id_cnt[mcgid] == 0)
					atomic64_dec(&stat->memcg_cnt);

				hh_list_del_nolock(eidx, midx, area->ext_table);
				eswap_debug(ESD_DEBUG, "ext id = %d\n", extid);
				break;
			} else {
				eswap_debug(ESD_ERR, "unmatched ext id = %d\n", extid);
				continue;
			}
		}
	}

unlock:
	hh_unlock_list(midx, area->ext_table);
	return extid;
}

int get_memcg_zram_entry(struct eswap_area *area, struct mem_cgroup *mcg)
{
	int mcgid, midx, idx;
	int index = -ENOENT;

	if (!area) {
		eswap_debug(ESD_ERR, "NULL area\n");
		return -EINVAL;
	}
	if (!area->obj_table) {
		eswap_debug(ESD_ERR, "NULL table\n");
		return -EINVAL;
	}
	if (!mcg) {
		eswap_debug(ESD_ERR, "NULL mcg\n");
		return -EINVAL;
	}

	mcgid = mcg->id.id;
	midx = mcg_idx(area, mcgid);
	if (midx < 0)
		return -EINVAL;

	hh_lock_list(midx, area->obj_table);
	if (mcg != get_mem_cgroup(mcgid)) {
		struct memcg_estrack *est = mem_cgroup_estrack(mcg);
		index = ((struct hh_list_head*)(&est->zram_lru))->next;
		eswap_debug(ESD_ERR, "memcg corrupted, get index %d directly.\n", index);
		if (index >= area->nr_objs)
			index = -ENOENT;
		goto unlock;
	}

	hh_list_for_each_entry(idx, midx, area->obj_table) {
		index = idx;
		break;
	}
unlock:
	hh_unlock_list(midx, area->obj_table);

	return index;
}

int get_extent_zram_entry(struct eswap_area *area, int extid)
{
	int index = -ENOENT;
	int idx, eidx;

	if (!area) {
		eswap_debug(ESD_ERR, "NULL area\n");
		return -EINVAL;
	}

	if (!area->obj_table) {
		eswap_debug(ESD_ERR, "NULL table\n");
		return -EINVAL;
	}

	eidx = ext_idx(area, extid);
	if (eidx < 0) {
		eswap_debug(ESD_ERR, "ext = %d invalid\n", extid);
		return -EINVAL;
	}

	hh_lock_list(eidx, area->obj_table);
	hh_list_for_each_entry(idx, eidx, area->obj_table) {
		index = idx;
		break;
	}
	hh_unlock_list(eidx, area->obj_table);

	return index;
}
