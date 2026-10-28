/*
 *
 */
#ifndef __ESWAP_INTERNAL_H__
#define __ESWAP_INTERNAL_H__

#include <linux/bio.h>
#include <linux/kref.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched/debug.h>

#if IS_ENABLED(CONFIG_TRANSSION_BLK)
#include "transsion-blk-types.h"
#endif

#include "../zram_drv.h"

#define ZRAM_IODELAY_500us ZRAM_IO_DELAY_500us
#define ZRAM_IODELAY_1000us ZRAM_IO_DELAY_1000us
#define ZRAM_IODELAY_3000us ZRAM_IO_DELAY_3000us
#define ZRAM_IODELAY_5000us ZRAM_IO_DELAY_5000us
#define ZRAM_IODELAY_30000us ZRAM_IO_DELAY_30000us
#define ZRAM_IODELAY_50000us ZRAM_IO_DELAY_50000us

#define BIO_MAX_PAGES           256

/*
 * EXTENT_SIZE		-- 32 KB
 * EXTENT_PG_CNT	-- 8 pages
 * EXTENT_SECTOR_CNT	-- 64 sector/512Byte
 */
// #define EXTENT_SHIFT		15
#define EXTENT_SHIFT		16
#define EXTENT_SIZE		(1UL << EXTENT_SHIFT)
#define EXTENT_MASK		(EXTENT_SIZE - 1)
#define EXTENT_PG_CNT		(EXTENT_SIZE >> PAGE_SHIFT)
#define EXTENT_SECTOR_CNT	(EXTENT_PG_CNT << 3)

#define EXTENT_MAX_OBJ_CNT	(8 * EXTENT_PG_CNT)

#define MAX_FAIL_RECORD_NUM	10
#define BLOCK_SECTOR_SHIFT	3

#define MIN_RECLAIM_ZRAM_SZ	(256 * 1024)
#define MAX_RECLAIM_ZRAM_SZ	(8 * 1024 * 1024)
#define FT_RECLAIM_SZ		500

enum {
	ESD_ERR = 0,
	ESD_WARN,
	ESD_INFO,
	ESD_DEBUG,
	ESD_MAX
};

extern inline int eswap_loglevel(void);
extern inline int eswap_acct_detail(void);

#define eswap_debug(lvl, fmt, args...) do {				\
	if (lvl <= eswap_loglevel()) {					\
		pr_notice("ESWAP %s: " fmt, __func__, ##args);		\
	}								\
} while (0)
	
enum eswap_scenario {
	ESWAP_OUT = 0,
	ESWAP_IN_FAULT,
	ESWAP_IN_BATCH,
	ESWAP_IN_PRELOAD,
	ESWAP_SCENARIO_NUM
};

enum eswap_fail_point {
	ESWAP_FAULT_OUT_INIT_FAIL = 0,
	ESWAP_FAULT_OUT_ENTRY_ALLOC_FAIL,
	ESWAP_FAULT_OUT_IO_ENTRY_PARA_FAIL,
	ESWAP_FAULT_OUT_SEGMENT_ALLOC_FAIL,
	ESWAP_FAULT_OUT_BIO_ALLOC_FAIL,
	ESWAP_FAULT_OUT_BIO_ADD_FAIL,
	ESWAP_FAULT_OUT_IO_FAIL,
	ESWAP_FAIL_POINT_BUTT
};

struct eswap_fail_record {
	unsigned char task_comm[TASK_COMM_LEN];
	enum eswap_fail_point point;
	ktime_t time;
	u32 index;
	int ext_id;
};

struct eswap_fail_record_info {
	int num;
	spinlock_t lock;
	struct eswap_fail_record record[MAX_FAIL_RECORD_NUM];
};

/* mapping from eswap_stat to zram_stats
 * eswap_stored_pages --> bd_count,		no. of pages in backing device
 * eswap_bd_reads --> bd_reads,			no. of reads from backing device
 * eswap_bd_writes --> bd_writes,		no. of writes from backing device
 * eswap_bd_read_delay --> all PF read delay in us
 * eswap_bd_write_delay --> all PF read delay in us
 */
struct eswap_stat {
	atomic64_t eswapout_cnt;
	atomic64_t eswapout_bytes;
	atomic64_t eswapout_pages;

	atomic64_t eswapout_inflight;
	atomic64_t eswapin_cnt;
	atomic64_t eswapin_bytes;
	atomic64_t eswapin_pages;
	atomic64_t eswapin_pages_pf;
	atomic64_t eswapin_inflight;
	atomic64_t eswap_faultin_cnt_1;
	atomic64_t eswap_faultin_cnt_2;

	atomic64_t eswap_bd_reads;
	atomic64_t eswap_bd_writes;
	atomic64_t eswap_bd_batch_reads;

	atomic64_t eswap_bd_read_delay;
	atomic64_t eswap_bd_write_delay;
	atomic64_t eswap_bd_read_max_delay;

	atomic64_t refault_bytes;
	atomic64_t refault_pages;

	atomic64_t zram_stored_bytes;
	atomic64_t zram_stored_pages;

	atomic64_t eswap_stored_bytes;
	atomic64_t eswap_stored_pages;

	atomic64_t eswap_fault_in_hits;

	atomic64_t notify_free;
	atomic64_t miss_free;
	atomic64_t under_wb_untrack;
	atomic64_t override;

	atomic64_t faulting_1;
	atomic64_t faulting_2;
	atomic64_t faulting_3;
	atomic64_t fltin_extent_busy;

	atomic64_t memcg_cnt;
	atomic64_t ext_cnt;
	atomic64_t mcgid_clear;
	atomic64_t memcg_deinit_cnt;

	atomic64_t io_fail_cnt[ESWAP_SCENARIO_NUM];
	atomic64_t alloc_fail_cnt[ESWAP_SCENARIO_NUM];

	atomic_t used_exts_num;
	atomic_t max_used_exts_num;

	atomic64_t sensitive_delay;
	atomic64_t io_lt_500; /* 0~500 us/page */
	atomic64_t io_lt_1000; /*500~1000 us/page */
	atomic64_t io_lt_3000; /*1000~3000 us/page */
	atomic64_t io_lt_5000; /*3000~5000 us/page */
	atomic64_t io_lt_30000; /*5000~30000 us/page */
	atomic64_t io_lt_50000; /*30000~50000 us/page */
	atomic64_t io_gt_50000; /*50000~unlimited us/page */

	struct {
		atomic64_t total_lat;
		atomic64_t max_lat;
		atomic64_t timeout_cnt;
	} lat[ESWAP_SCENARIO_NUM];

	struct eswap_fail_record_info record;

	unsigned long last_time;
};

struct eswap_page_pool {
	struct list_head page_pool_list;
	spinlock_t page_pool_lock;
};

extern inline bool eswap_enable(void);
extern void *eswap_malloc(size_t size, bool fast, bool nofail);
extern void eswap_free(const void *mem);
extern struct page *eswap_alloc_page(struct eswap_page_pool *,
				gfp_t gfp, bool fast, bool nofail);
extern void eswap_page_recycle(struct page *page, struct eswap_page_pool *);

extern inline struct eswap_stat *eswap_get_stat_obj(void);
extern inline struct zram *eswap_get_global_zram(void);
struct eswap_area;
extern inline struct eswap_area *eswap_get_global_area(void);

extern int memcg_estrack_init(struct mem_cgroup *memcg, struct zram *zram);
extern void eswap_manager_memcg_deinit(struct mem_cgroup *mcg);

extern void memcg_estrack_zram_lru_del(struct zram *, u32 index);

extern struct mem_cgroup *zram_get_memcg(struct zram *zram, u32 index);

static inline void eswap_show_stack(struct task_struct *task, unsigned long *fp)
{
	WARN_ON(1);
}

extern void eswap_record_fail(enum eswap_fail_point point,
	u32 index, int ext_id, unsigned char *task_comm);

extern inline bool eswap_swapout_enable(void);
extern inline struct workqueue_struct *eswapout_workqueue(void);
extern inline struct workqueue_struct *pageout_workqueue(void);

extern inline void eswap_used_exts_num_inc(void);
extern inline void eswap_used_exts_num_dec(void);
extern inline int eswap_get_used_exts_num(void);
extern inline int eswap_get_exts_num(void);

#ifdef CONFIG_ESWAP_IDLE_WRITEBACK
void memcg_idle_count(struct zram *zram);
void memcg_idle_inc(struct zram *zram, u32 index);
void memcg_idle_dec(struct zram *zram, u32 index);
#endif

#if IS_ENABLED(CONFIG_TRANSSION_BLK)
extern bool zram_vip_io_sched_enable;
static inline bool schedule_support_vip_in_fault(void)
{
	return zram_vip_io_sched_enable;
}
#else
static inline bool schedule_support_vip_in_fault(void)
{
	return false;
}
#endif

#if defined(CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT) || defined(CONFIG_ZRAM_MEMORY_TRACKING)
#include "../zram_process_reclaim.h"

extern int zram_kernel_wb_actime_limit;
extern int zram_kernel_wb_count_limit;
static inline void zram_record_stay_time(struct zram *zram, int index, ktime_t now)
{
	struct zram_table_entry *zram_entry = &zram->table[index];
	zram_entry->stay_time = (unsigned int)(ktime_ms_delta(now, zram_entry->ac_time) / 1000);
	return;
}

#define MF22PRELOG "[MF2.2] "
static inline void zram_report_stay_time_for_wb(struct zram *zram, int index, s64 stay_time)
{
	if (is_log_enable(ZRAM_WB_LOG)) {
		bool is_key_task = zram_test_flag(zram, index, ZRAM_KEY);
		pr_err(MF22PRELOG"[write begin][%d] %lldmin %llds %lldms %lld%c, %s",
			index, stay_time/60000, (stay_time % 60000)/1000, stay_time%1000,
			stay_time,
			is_key_task ? 'K' :'.', "normal-io");
	}
}

static inline void zram_report_stay_time_for_pf(struct zram *zram, int index)
{
	if (is_log_enable(ZRAM_WB_LOG)) {
		bool is_key_task = zram_test_flag(zram, index, ZRAM_KEY);
		struct zram_table_entry *zram_entry = &zram->table[index];

		unsigned int stay_time = zram_entry->stay_time;
		pr_err(MF22PRELOG"[PF Read][%d] %umin %us %u%c, %s",
			index, stay_time/60, stay_time%60, stay_time,
			is_key_task ? 'K' :'.',
			schedule_support_vip_in_fault() ? "vip-io" : "normal-io");
	}
}

static inline void zram_report_for_preload(int pages)
{
	if (is_log_enable(ZRAM_WB_LOG)) {
		pr_err(MF22PRELOG"[Preload Read]%d, %s", pages, "normal-io");
	}
}

#undef MF22PRELOG
#else
#define zram_kernel_wb_actime_limit 0
static inline void zram_record_stay_time(struct zram *zram, int index, ktime_t now) {}
static inline void zram_report_stay_time_for_wb(struct zram *zram, int index, s64 stay_time) {}
static inline void zram_report_stay_time_for_pf(struct zram *zram, int index) {}
static inline void zram_report_for_preload(int pages) {}
#endif

#if defined(CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT)
extern bool zram_should_writeback(struct zram *zram, int pages, bool trigger);
extern bool update_read_stats(struct zram* zram);

extern int init_lru_writeback(struct zram *zram);
extern void memfusion_psi_io(struct psi_data *wbd_psi_data, unsigned long add_data, bool update_flag);
extern void memfusion_psi_mem(struct psi_data *wbd_psi_data, unsigned long add_data, bool update_flag);
static inline void update_psi_io(struct zram *zram, unsigned long add_data, bool update_flag)
{
	return memfusion_psi_io(&zram->wbd_psi_data, add_data, update_flag);
}
static inline void update_psi_mem(struct zram *zram, unsigned long add_data, bool update_flag)
{
	return memfusion_psi_mem(&zram->wbd_psi_data, add_data, update_flag);
}

extern struct zram *g_backingdev;
static inline int misc_init(struct zram *zram)
{
	return init_lru_writeback(zram);
}
static inline void misc_deinit(struct zram *zram)
{
	(void)zram;
	g_backingdev = NULL;
}

extern void sync_dump_info_with_eswap(struct zram *zram);
extern void sync_rw_data_with_eswap(struct zram *zram);
static inline void sync_info_with_extern(void)
{
	if (g_backingdev) {
		sync_dump_info_with_eswap(g_backingdev);
		sync_rw_data_with_eswap(g_backingdev);
	}
}
#else
static inline bool zram_should_writeback(struct zram *, int , bool) { return true; }
static bool update_read_stats(struct zram* zram) { return false; }
static inline void update_psi_io(struct zram *zram, unsigned long add_data, bool update_flag) {}
static inline void update_psi_mem(struct zram *zram, unsigned long add_data, bool update_flag) {}

static inline int misc_init(struct zram *zram) { return 0; }
static inline void misc_deinit(struct zram *zram) { return; }

static inline void sync_info_with_extern(void) {}
#endif

#if defined(CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT) && defined(CONFIG_FTRACE)
extern inline void zram_kernel_trace_start(char * name);
extern inline void zram_kernel_trace_change(char * tag , int flag);
extern inline void zram_kernel_trace_end(char * name);
extern inline void zram_kernel_trace_read_start(bool is_kern, unsigned int seconds);
extern inline void zram_kernel_trace_read_end(bool is_kern, unsigned int seconds);
extern inline void zram_aggr_trace_start(int val, char *name);
extern inline void zram_aggr_trace_end(int val, char *name);

static inline void zram_trace_read_start(struct zram *zram, u32 index)
{
	bool is_kern = zram_test_flag(zram, index, ZRAM_COLD);
	unsigned int stay_time = zram->table[index].stay_time;

	zram_kernel_trace_read_start(is_kern, stay_time);
}

static inline void zram_trace_read_end(struct zram *zram, u32 index)
{
	bool is_kern = zram_test_flag(zram, index, ZRAM_COLD);
	unsigned int stay_time = zram->table[index].stay_time;

	zram_kernel_trace_read_end(is_kern, stay_time);
}

static inline void zram_trace_write_start(int size)
{
	zram_aggr_trace_start(size, "zram_memcg_write_io");
}

static inline void zram_trace_write_end(int size)
{
	zram_aggr_trace_end(size, "zram_memcg_write_io");
}
#else
static inline void zram_trace_read_start(struct zram *zram, u32 index) {}
static inline void zram_trace_read_end(struct zram *zram, u32 index) {}
static inline void zram_trace_write_start(int) {}
static inline void zram_trace_write_end(int) {}
#endif

#endif // __ESWAP_INTERNAL_H__
