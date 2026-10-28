/*
 */
#ifndef __MEMCG_ESTRACK_H__
#define __MEMCG_ESTRACK_H__

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/memcontrol.h>
#include <linux/errno.h>

#define ESWAP_MEM_CGROUP_NAME_MAX_LEN 32
struct memcg_estrack {
#ifdef CONFIG_ESWAP
	unsigned long zram_lru;
	unsigned long ext_lru;
	spinlock_t zram_init_lock;
	struct zram *zram;

	atomic_t zram_stored_size;
	atomic_t zram_page_size;

	atomic_t eswap_stored_pages;
	atomic_t eswap_stored_size;

	atomic64_t eswap_ext_notify_free;

	atomic64_t eswap_out_cnt;
	atomic64_t eswap_in_cnt;

	atomic64_t eswap_fault_cnt;
	atomic64_t eswap_in_fault_cnt;

	atomic64_t eswap_out_extent_cnt;
	atomic64_t eswap_in_extent_cnt;

#ifdef CONFIG_ESWAP_IDLE_WRITEBACK
	atomic64_t zram_idle_size;
	atomic64_t zram_idle_page;
#endif

	atomic64_t same_pages;		/* no. of same element filled pages */
	atomic64_t huge_pages;		/* no. of huge pages */

	atomic_t flash2zram_ratio;

	bool in_swapin;

#endif	// CONFIG_ESWAP

#ifdef CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT
	atomic_t mem2zram_ratio;
	atomic_t zram2flash_ratio;

	/* anon refault */
	atomic_t anon_rf_ratio;
	unsigned long long anon_rfs;

	int reclaim_brk_reason;
	int reclaim_oneshot;

	bool skip_reclaim_anon;
	bool force_reclaim_abort;
#endif

	/* common part, needed by zswapd and eswap 
	 */
	struct list_head score_node;
	struct mem_cgroup *owner;
	atomic_t app_score;
	atomic_t def_score;

	int offline;

	char name[ESWAP_MEM_CGROUP_NAME_MAX_LEN];
};

static inline struct mem_cgroup *score_node_owner(struct list_head *p)
{
	struct memcg_estrack *est = container_of(p, struct memcg_estrack, score_node);
	return est->owner;
}

#define MEMCG_AUX_DATA_RECLAIM_ABORT	0x01
#define MEMCG_AUX_DATA_MASK		0x03

// return rvalue
#define MEMCG_ESTRACK(memcg)		\
	((struct memcg_estrack *)(memcg->android_oem_data1[0] & ~MEMCG_AUX_DATA_MASK))

#define MEMCG_ESTRACK_FIELD(memcg, field)	(MEMCG_ESTRACK(memcg)->field)
#define MEMCG_ESTRACK_VALID(memcg)		(MEMCG_ESTRACK(memcg) != 0)

#define INIT_MEMCG_ESTRACK(memcg, val) do {				\
	BUG_ON(val & MEMCG_AUX_DATA_MASK);				\
	memcg->android_oem_data1[0] = val;				\
} while (0)

/* simple wrapper used for memcgroup property nodes,
 * for these does not have estrack, just return -ENOSYS
 * otherwise, extract memcg estrack
 */
#define MEMCG_ESTRACK_GUARD(memcg) ({				\
	struct memcg_estrack *est = MEMCG_ESTRACK(memcg);	\
	if (!est)						\
		return -ENOSYS;					\
	est;							\
})

#define MEMCG_EST_DATA(memcg)		(memcg->android_oem_data1[0] & ~MEMCG_AUX_DATA_MASK)
#define MEMCG_AUX_DATA(memcg)		(memcg->android_oem_data1[0] & MEMCG_AUX_DATA_MASK)
#define MEMCG_RAW_DATA0(memcg)		(memcg->android_oem_data1[0])
#define MEMCG_RAW_DATA0_PTR(memcg)	(&(memcg->android_oem_data1[0]))

static inline struct memcg_estrack *mem_cgroup_estrack(struct mem_cgroup *memcg)
{
	return MEMCG_ESTRACK(memcg);
}

static inline void mem_cgroup_set_reclaim_abort(struct mem_cgroup *memcg, bool abort)
{
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);

	if (est)
		MEMCG_ESTRACK_FIELD(memcg, force_reclaim_abort) = abort;
	else {
		if (abort)
			MEMCG_RAW_DATA0(memcg) |= MEMCG_AUX_DATA_RECLAIM_ABORT;
		else
			MEMCG_RAW_DATA0(memcg) &= ~MEMCG_AUX_DATA_RECLAIM_ABORT;
	}
}

static inline bool mem_cgroup_get_reclaim_abort(struct mem_cgroup *memcg)
{
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);
	bool abort;

	if (est)
		abort = READ_ONCE(MEMCG_ESTRACK_FIELD(memcg, force_reclaim_abort));
	else
		abort = !!(READ_ONCE(MEMCG_RAW_DATA0(memcg)) & MEMCG_AUX_DATA_RECLAIM_ABORT);

	return abort;
}

static inline void mem_cgroup_set_reclaim_brk_reason(struct mem_cgroup *memcg, int reason)
{
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);
	if (est)
		est->reclaim_brk_reason = reason;
	return;
}

static inline void mem_cgroup_set_reclaim_oneshot(struct mem_cgroup *memcg, int shot)
{
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);
	if (est)
		est->reclaim_oneshot = shot;
	return;
}

#if 0
#define MEMCG_RAW_DATA1(memcg)		(memcg->android_oem_data1[1])
#define MEMCG_RAW_DATA1_PTR(memcg)	(&(memcg->android_oem_data1[1]))

static inline atomic_t *mem_cgroup_get_samepages_counter(struct mem_cgroup *memcg)
{
	char *ptr = (char *)MEMCG_RAW_DATA1_PTR(memcg);
	return (atomic_t *)ptr;
}

static inline atomic_t *mem_cgroup_get_hugepages_counter(struct mem_cgroup *memcg)
{
	char *ptr = (char *)MEMCG_RAW_DATA1_PTR(memcg);
	return (atomic_t *)(ptr + sizeof(atomic_t));
}

static inline void memcg_inc_samepages_counter(struct mem_cgroup *memcg)
{
	atomic_inc(mem_cgroup_get_samepages_counter(memcg));
}

static inline void memcg_dec_samepages_counter(struct mem_cgroup *memcg)
{
	atomic_dec(mem_cgroup_get_samepages_counter(memcg));
}

static inline int memcg_read_samepages_counter(struct mem_cgroup *memcg)
{
	return atomic_read(mem_cgroup_get_samepages_counter(memcg));
}

static inline void memcg_inc_hugepages_counter(struct mem_cgroup *memcg)
{
	atomic_inc(mem_cgroup_get_hugepages_counter(memcg));
}

static inline void memcg_dec_hugepages_counter(struct mem_cgroup *memcg)
{
	atomic_dec(mem_cgroup_get_hugepages_counter(memcg));
}

static inline int memcg_read_hugepages_counter(struct mem_cgroup *memcg)
{
	return atomic_read(mem_cgroup_get_hugepages_counter(memcg));
}

#else
static inline void memcg_inc_samepages_counter(struct mem_cgroup *memcg)
{
}

static inline void memcg_dec_samepages_counter(struct mem_cgroup *memcg)
{
}

static inline int memcg_read_samepages_counter(struct mem_cgroup *memcg)
{
	return 0;
}

static inline void memcg_inc_hugepages_counter(struct mem_cgroup *memcg)
{
}

static inline void memcg_dec_hugepages_counter(struct mem_cgroup *memcg)
{
}

static inline int memcg_read_hugepages_counter(struct mem_cgroup *memcg)
{
	return 0;
}
#endif

#endif // __MEMCG_ESTRACK_H__
