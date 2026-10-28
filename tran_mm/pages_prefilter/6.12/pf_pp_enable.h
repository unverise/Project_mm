#ifndef __TRANMEM_PAGE_PREFILTER_PP_ENABLE_H__
#define __TRANMEM_PAGE_PREFILTER_PP_ENABLE_H__

#include <linux/memcontrol.h>

#define PP_RESERVED_INDEX   1
#define PP_ENABLED(memcg)      memcg->android_oem_data1[PP_RESERVED_INDEX]
#define PP_ENABLED_PTR(memcg)  &memcg->android_oem_data1[PP_RESERVED_INDEX]

/*
 * memcg pp_enabled layout:
 *
 *         <----49|------32 bits------|---  11 bits --|------4 bits-----|---2 bits---|
 * |-- Reserved --|-- File Threshold--|-- Q reserve --|-- aging bypass--|--Enabled --|
 */
#define PPE_ENABLED_BIT           0
#define PPE_SPECIAL_ENABLED_BIT   1

#define PPE_ENABLED_WIDTH      2
#define PPE_AGING_BYPASS_WIDTH 4
#define PPE_QRESERVE_WIDTH     11        //reserved here
#define PPE_FILE_THD_WIDTH     32        //max 4G
#define PPE_RESERVED_WIDTH     (BITS_PER_LONG - PPE_ENABLED_WIDTH - PPE_AGING_BYPASS_WIDTH -\
        PPE_QRESERVE_WIDTH - PPE_FILE_THD_WIDTH)

#define PPE_ENABLED_SHIFT           PPE_ENABLED_WIDTH
#define PPE_ENABLED_MASK            ((1ULL << PPE_ENABLED_SHIFT) - 1)

#define PPE_AGING_BYPASS_SHIFT      (PPE_ENABLED_WIDTH)
#define PPE_AGING_BYPASS_MASK       ((1ULL << PPE_AGING_BYPASS_WIDTH) - 1)

#define PPE_FILE_THD_SHIFT          (PPE_ENABLED_WIDTH + PPE_AGING_BYPASS_WIDTH + PPE_QRESERVE_WIDTH)
#define PPE_FILE_THD_MASK           ((1ULL << PPE_FILE_THD_WIDTH) - 1)

#define VAL_MEMCG_FILE_THD(v)       ((v) & PPE_FILE_THD_MASK)
#define CLR_MEMCG_FILE_THD(v)       ((v) & ~(PPE_FILE_THD_MASK << PPE_FILE_THD_SHIFT))
#define GET_MEMCG_FILE_THD(v)       (((v) >> PPE_FILE_THD_SHIFT) & PPE_FILE_THD_MASK)
#define SET_MEMCG_FILE_THD(v,t)     (CLR_MEMCG_FILE_THD(v) | (VAL_MEMCG_FILE_THD(t) << PPE_FILE_THD_SHIFT))

#define VAL_AGING_BYPASS_CNT(v)     ((v) & PPE_AGING_BYPASS_MASK)
#define CLR_AGING_BYPASS_CNT(v)     ((v) & ~(PPE_AGING_BYPASS_MASK << PPE_AGING_BYPASS_SHIFT))
#define GET_AGING_BYPASS_CNT(v)     (((v) >> PPE_AGING_BYPASS_SHIFT) & PPE_AGING_BYPASS_MASK)
#define SET_AGING_BYPASS_CNT(v,t)   (CLR_AGING_BYPASS_CNT(v) | (VAL_AGING_BYPASS_CNT(t) << PPE_AGING_BYPASS_SHIFT))

static inline u64 read_pp_enabled(const struct mem_cgroup *memcg)
{
	return READ_ONCE(PP_ENABLED(memcg));
}

static inline void write_pp_enabled(struct mem_cgroup *memcg, u64 val)
{
	WRITE_ONCE(PP_ENABLED(memcg), val);
}

static inline u64 pp_aging_bypass_inc(struct mem_cgroup *memcg)
{
	u64 old, new;

	do {
		old = read_pp_enabled(memcg);

		u64 cnt = GET_AGING_BYPASS_CNT(old);
		if (cnt >= PPE_AGING_BYPASS_MASK)
			return old;

		new = SET_AGING_BYPASS_CNT(old, cnt + 1);
	} while (cmpxchg64(PP_ENABLED_PTR(memcg), old, new) != old);

	return new;
}

static inline u64 pp_aging_bypass_clear(struct mem_cgroup *memcg)
{
	u64 old, new;

	do {
		old = read_pp_enabled(memcg);

		u64 cnt = GET_AGING_BYPASS_CNT(old);
		if (cnt == 0)
			return old;

		new = CLR_AGING_BYPASS_CNT(old);
	} while (cmpxchg64(PP_ENABLED_PTR(memcg), old, new) != old);

	return new;
}

static inline u64 pp_file_thd_set(struct mem_cgroup *memcg, u64 thd)
{
	u64 old, new;

	do {
		old = read_pp_enabled(memcg);

		new = SET_MEMCG_FILE_THD(old, thd);
	} while (cmpxchg64(PP_ENABLED_PTR(memcg), old, new) != old);

	return new;
}

static inline u64 pp_file_thd_clear(struct mem_cgroup *memcg)
{
	u64 old, new;

	do {
		old = read_pp_enabled(memcg);
		new = CLR_MEMCG_FILE_THD(old);
	} while (cmpxchg64(PP_ENABLED_PTR(memcg), old, new) != old);

	return new;
}

static inline u64 pp_bit_set(struct mem_cgroup *memcg, int bit)
{
	u64 old, new;
	u64 mask = 1ULL << bit;

	do {
		old = read_pp_enabled(memcg);
		if ((old & mask))
			return old;

		new = old | mask;
	} while (cmpxchg64(PP_ENABLED_PTR(memcg), old, new) != old);

	return new;
}

static inline u64 pp_bit_clear(struct mem_cgroup *memcg, int bit)
{
	u64 old, new;
	u64 mask = 1ULL << bit;

	do {
		old = read_pp_enabled(memcg);
		if (!(old & mask))
			return old;

		new = old & ~mask;
	} while (cmpxchg64(PP_ENABLED_PTR(memcg), old, new) != old);

	return new;
}

#endif  //__TRANMEM_PAGE_PREFILTER_PP_ENABLE_H__
