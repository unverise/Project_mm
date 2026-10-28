#ifndef __ESWAP_INF_H__
#define __ESWAP_INF_H__

#include <linux/memcontrol.h>
#include "../zram_drv.h"

#ifdef CONFIG_ESWAP
extern u64 eswap_get_original_size(void);
extern u64 eswap_get_compress_size(void);
extern int eswap_get_used_ext_pages(void);
extern void eswap_init(struct zram *zram);
extern int eswap_track(struct zram *zram, u32 index, struct mem_cgroup *memcg, struct mem_cgroup *my_memcg);
extern void eswap_untrack(struct zram *zram, u32 index);
extern struct mem_cgroup *eswap_untrack_memcg_get(struct zram *zram, u32 index);
extern void eswap_untrack_memcg_put(struct mem_cgroup *);
extern int eswap_fault_in(struct zram *zram, u32 index);
extern bool eswap_delete(struct zram *zram, u32 index);
extern int eswap_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg);

extern inline bool eswap_enable(void);
extern inline void eswapout_enable(bool en);
extern inline bool eswap_swapout_enable(void);
extern void eswap_write_back_sync(unsigned long size);
extern int eswap_module_init(void);
extern void eswap_module_exit(void);

extern ssize_t eswap_online_store(struct device *, struct device_attribute *, const char *, size_t );
extern ssize_t eswap_online_show(struct device *, struct device_attribute *, char *);
extern ssize_t eswap_enable_store(struct device *, struct device_attribute *, const char *, size_t );
extern ssize_t eswap_enable_show(struct device *, struct device_attribute *, char *);
extern ssize_t eswap_extra_online_store(struct device *, struct device_attribute *, const char *, size_t );
extern ssize_t eswap_extra_online_show(struct device *, struct device_attribute *, char *);

extern int eswap_batch_in(struct mem_cgroup *mcg, unsigned long size, bool touch, bool preload);

extern unsigned long eswap_memcgs_out(unsigned long size);

extern unsigned long eswap_get_zram_used_pages(void);
extern unsigned long eswap_get_eswap_used_pages(void);
extern unsigned long long eswap_get_zram_pagefault(void);

extern int memcg_eswap_out(struct mem_cgroup *memcg);
extern int memcg_eswap_batch_in(struct mem_cgroup *memcg, bool touch, bool preload);

extern bool eswap_reclaim_work_running(void);
extern void eswap_reclaim_work_done(void);

extern void eswap_stat_show(struct seq_file *m);
extern void eswap_area_stat_show(struct seq_file *m);
extern inline int eswap_loglevel(void);
extern inline int eswap_set_loglevel(int);
extern inline int eswap_acct_detail(void);
extern inline int eswap_set_acct_detail(int);

extern void eswap_idle_inc(struct zram *zram, u32 index);
extern void eswap_idle_dec(struct zram *zram, u32 index);
extern void eswap_idle_writeback(void);
#else

static inline unsigned long eswap_memcgs_out(unsigned long size)
{
	return 0;
}

static inline unsigned long eswap_get_zram_used_pages(void)
{
	return 0;
}

static inline unsigned long eswap_get_eswap_used_pages(void)
{
	return 0;
}

static inline unsigned long long eswap_get_zram_pagefault(void)
{
	return 0;
}

static inline bool eswap_reclaim_work_running(void)
{
	return false;
}
static inline void eswap_reclaim_work_done(void) {}

static inline void eswap_stat_show(struct seq_file *m) {}
static inline void eswap_area_stat_show(struct seq_file *m) {}

static inline void eswap_idle_inc(struct zram *zram, u32 index) {}
static inline void eswap_idle_dec(struct zram *zram, u32 index) {}
static inline void eswap_idle_writeback(void) {}
#endif

#endif // __ESWAP_INF_H__
