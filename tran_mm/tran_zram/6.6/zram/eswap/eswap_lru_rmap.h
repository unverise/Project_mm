#ifndef __ESWAP_LRU_MAP__
#define __ESWAP_LRU_MAP__

struct zram;
struct mem_cgroup;

extern void zram_set_memcg(struct zram *zram, u32 index, struct mem_cgroup *mcg);
extern void zram_clear_memcg(struct zram *zram, u32 index);
extern struct mem_cgroup *zram_get_memcg(struct zram *zram, u32 index);
extern struct mem_cgroup *zram_get_memcg_with_css_get(struct zram *zram, u32 index);

extern int zram_get_memcg_coldest_index(struct zram *, struct mem_cgroup *, unsigned int *index,
	int max_cnt, int (*filter)(void *data, int i), void *data);

extern int zram_rmap_get_extent_index(struct eswap_area *area,
		int ext_id, int *index);

extern int zram_lru_add(struct zram *zram, u32 index, struct mem_cgroup *);
extern void zram_lru_add_tail(struct zram *zram, u32 index, struct mem_cgroup *);
void zram_lru_del(struct zram *zram, u32 index);
extern void zram_rmap_insert(struct zram *zram, u32 index, unsigned long eswpentry);
extern void zram_rmap_erase(struct zram *zram, u32 index);

#endif // __ESWAP_LRU_MAP__
