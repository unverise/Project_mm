/*
 *
 */

#ifndef __MEMCG_CONTROL_H__
#define __MEMCG_CONTROL_H__
#include <linux/mmzone.h>
#include <linux/memcontrol.h>
#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/version.h>

/*
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + ((c) > 255 ? 255 : (c)))
#endif

#ifndef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE KERNEL_VERSION(6, 12, 23)
#endif
*/

#ifndef MIN_SWAPPINESS
#define MIN_SWAPPINESS 0
#endif

#ifndef MAX_SWAPPINESS
#define MAX_SWAPPINESS 200
#endif

#define ANON_SWAPPINESS (MAX_SWAPPINESS + 1)
#define FILE_SWAPPINESS (MAX_SWAPPINESS + 2)

// n can be an expression
#define MB_TO_PAGES(n)	((unsigned long)((n) * (SZ_1M / PAGE_SIZE)))
#define GB_TO_PAGES(n)	((unsigned long)((n) * (SZ_1G / PAGE_SIZE)))
#define PAGES_TO_MB(n)	((n) * PAGE_SIZE / SZ_1M)
#define PAGES_TO_KB(n)	((n) * (PAGE_SIZE / SZ_1K))

#define MAX_RECLAIM_RETRIES 16

#if defined(CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT) || defined(CONFIG_ESWAP)
#include "memcg_estrack.h"
extern struct mem_cgroup *get_next_memcg(struct mem_cgroup *prev);
extern void get_next_memcg_break(struct mem_cgroup *prev);
extern struct mem_cgroup *get_prev_memcg(struct mem_cgroup *next);
extern void get_prev_memcg_break(struct mem_cgroup *next);

extern struct memcg_estrack *__memcg_estrack_alloc(struct mem_cgroup *, const char *);
extern struct mem_cgroup *mem_cgroup_iter_internal(struct mem_cgroup *root,
	struct mem_cgroup *prev, struct mem_cgroup_reclaim_cookie *reclaim);
extern void mem_cgroup_iter_break_internal(struct mem_cgroup *root, struct mem_cgroup *prev);

extern int mem_eswap_init(void);
extern void mem_eswap_exit(void);
#endif

#ifdef CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT
#include <linux/seq_file.h>

#define SNAPSHOT_MIN_INTERVAL_MS 200
extern int zswapd_debug;

enum stop_reclaim_reason{
	MEMORY_BK_RECLAIM_NONE = 0,
	MEMORY_BK_RECLAIM_SIGNAL,
	MEMORY_BK_RECLAIM_TIME,
	MEMORY_BK_RECLAIM_SIZE,
	MEMORY_BK_RECLAIM_PROP,
	MEMORY_BK_RECLAIM_ABORT,
	MEMORY_BK_RECLAIM_MAX,
};

struct memcg_reclaim_stat_v1 {
	atomic64_t total_cnt; /* Total count of memcg reclaim events */
	atomic64_t fail_cnt; /* Total failed count of memcg reclaim events */
	atomic64_t reclaimed_pages; /* Total number of memcg reclaimed pages */
	atomic64_t reclaimed_delay;  /* Total delay time of memcg reclaim in ms */
	atomic64_t abort_reason_cnt[MEMORY_BK_RECLAIM_MAX]; /* Number of memcg reclaim abort by each reason */
};
extern struct memcg_reclaim_stat_v1 memrec_stat_v1;

enum {
	ZSD_ERR = 0,
	ZSD_WARN,
	ZSD_INFO,
	ZSD_DEBUG,
	ZSD_MAX
};

#define ZSWAPD_DEBUG(lvl, fmt, args...) do {				\
	if (unlikely(lvl <= zswapd_debug)) {				\
		pr_notice("memfusiond %s: " fmt,  __func__, ##args);	\
	}								\
} while (0)

extern inline bool zswapd_anon_reclaim_enabled(void);
extern inline bool zswapd_drop_caches_enabled(void);
extern inline unsigned int zswapd_get_zram_wm_percent(void);
extern inline unsigned int zswapd_get_zram_comp_percent(void);
extern inline unsigned int zswapd_get_max_reclaim_size(void);
extern inline unsigned long zswapd_get_skip_interval_ms(void);
extern inline unsigned long zswapd_get_max_skip_interval_ms(void);
extern inline unsigned long zswapd_get_empty_round_threshold_pages(void);

extern int swap_is_low(void);
extern inline unsigned int avail_min_th(void);
extern inline unsigned int avail_mid_th(void);
extern inline unsigned int avail_high_th(void);
extern inline unsigned int get_inactive_file_ratio_value(void);
extern inline unsigned int get_active_file_ratio_value(void);
extern inline unsigned long long get_area_anon_refault_threshold_value(void);

#ifdef CONFIG_ESWAP
extern void snapshotd_enable(int val);
extern inline void snapshot_set_min_interval_ms(u64 val);
extern inline u64 snapshot_get_min_interval_ms(void);
extern void memcg_app_score_update_from_pf(struct mem_cgroup *target);
extern void mem_cgroup_app_value_promote(struct memcg_estrack *est);
#endif

struct mgasync {
	struct list_head node;
	struct mem_cgroup *memcg;
	unsigned long nr_to_reclaim;
	unsigned long step;
	int swappiness;
	int max_time;
	int local;
};

extern void enqueue_mgasync(struct mgasync *mga);
extern void abort_memcg_mgasync(struct mem_cgroup *memcg);
extern struct mgasync *alloc_init_mgasync_reclaim(struct mem_cgroup *,
	unsigned long, unsigned long, int, int, int);

extern unsigned long memcg_pages(struct mem_cgroup *memcg, enum node_stat_item item);
extern unsigned long memcg_pages_local(struct mem_cgroup *memcg, enum node_stat_item item);

extern unsigned long memcg_force_reclaim(struct mem_cgroup *, unsigned long nr_pages, int *swappiness);
extern unsigned long memcg_force_reclaim_anon_or_file(struct mem_cgroup *,
	unsigned long nr_pages, int type, int local);

extern int zswapd_events_show(struct seq_file *m, void *v);

extern pid_t get_zswapd_pid(int);
extern void wake_all_zswapds(void);
extern void try_to_wake_all_zswapds(void);

enum zswapd_pressure_level {
	ZLEVEL_LOW = 0,
	ZLEVEL_MEDIUM,
	ZLEVEL_CRITICAL,
	ZLEVEL_COUNT
};
extern void zswapd_pressure_report(enum zswapd_pressure_level level);
extern int get_zram2flash_ratio(struct mem_cgroup *memcg);

extern int zswapd_module_init(void);
extern void zswapd_module_exit(void);

#else
static inline int get_zram2flash_ratio(struct mem_cgroup *memcg)
{
	return 10;
}

static inline int zswapd_module_init(void)
{
	return 0;
}

static inline void zswapd_module_exit(void)
{
}
#endif

#ifdef CONFIG_ESWAP
extern int eswap_module_init(void);
extern void eswap_module_exit(void);

extern void eswap_mem_cgroup_remove(struct mem_cgroup *memcg);

extern struct mem_cgroup *page_eswap_memcg(struct page *page);
extern struct mem_cgroup *memcg_eswap_memcg(struct mem_cgroup *);
#else
static inline void eswap_mem_cgroup_remove(struct mem_cgroup *memcg) {}

static inline int eswap_module_init(void)
{
	return 0;
}

static inline void eswap_module_exit(void)
{
}
#endif

#endif // __MEMCG_CONTROL_H__
