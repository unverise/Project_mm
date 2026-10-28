/*
 * Compressed RAM block device
 *
 * Copyright (C) 2008, 2009, 2010  Nitin Gupta
 *               2012, 2013 Minchan Kim
 *
 * This code is released using a dual license strategy: BSD/GPL
 * You can choose the licence that better fits your requirements.
 *
 * Released under the terms of 3-clause BSD License
 * Released under the terms of GNU General Public License Version 2.0
 *
 */

#ifndef _ZRAM_DRV_H_
#define _ZRAM_DRV_H_

#include <linux/rwsem.h>
#include <linux/zsmalloc.h>
#include <linux/crypto.h>
#include <linux/kernel_stat.h>
#ifdef CONFIG_ESWAP
#include <linux/blkdev.h>
#endif
#include "zcomp.h"

#ifdef CONFIG_ZRAM_MULTI_COMP
#define ZRAM_PRIMARY_COMP	0U
#define ZRAM_SECONDARY_COMP	1U
#define ZRAM_TERTIARY_COMP	2U
#define ZRAM_MAX_COMPS	4U
#else
#define ZRAM_PRIMARY_COMP	0U
#define ZRAM_SECONDARY_COMP	0U
#define ZRAM_TERTIARY_COMP	0U
#define ZRAM_MAX_COMPS	1U
#endif

#define TRANMEM_MANAGEMENT_OFFSET (2)

struct task_priority_struct {
        bool high_priority;
};

static inline struct task_priority_struct *get_task_priority_struct(struct task_struct *task)
{
    if (!task)
        return NULL;

    return (struct task_priority_struct *)&(task->android_oem_data1[TRANMEM_MANAGEMENT_OFFSET]);
}

static inline bool is_critical(struct task_struct *task)
{
    struct task_priority_struct *task_priority = get_task_priority_struct(task);
    if (IS_ERR_OR_NULL(task_priority)) {
        return false;
    }

    return task_priority->high_priority;
}

#define SECTORS_PER_PAGE_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define SECTORS_PER_PAGE	(1 << SECTORS_PER_PAGE_SHIFT)
#define ZRAM_LOGICAL_BLOCK_SHIFT 12
#define ZRAM_LOGICAL_BLOCK_SIZE	(1 << ZRAM_LOGICAL_BLOCK_SHIFT)
#define ZRAM_SECTOR_PER_LOGICAL_BLOCK	\
	(1 << (ZRAM_LOGICAL_BLOCK_SHIFT - SECTOR_SHIFT))

//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 20241001 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
#define CONFIG_ZRAM_TRACK_ENTRY_ACTIME 1

#define MEMFUSION22_TID 541861000015

#define SYSTEM_SERVER "system_server"
#define SYSTEM_SERVER_LEN 13

#define SYSTEM_UI "ndroid.systemui"
#define SYSTEM_UI_LEN 15

#define ZERO_SCREEN "cher:zeroscreen"
#define ZERO_SCREEN_LEN 15

#define SSIONLAUNCHER "ssion.launcher3"
#define SSIONLAUNCHER_LEN 15

enum zram_key_task{
	KEY_SYSTEM_SERVER = 0,
	KEY_SYSTEM_UI,
	KEY_SURFACEFLINGER,
	KEY_COMPOSE_X,
	KEY_ZERO_SCREEN,
	KEY_LAUNCHER,
	KEY_MAX_PROCESS,
};

#define ZRAM_WB_ALL -1

/* surfaceflinger thread */
#define MF22_SRUFACE "surfaceflinger"
#define MF22_SRUFACE_LEN 14

/* RenderThread thread */
#define MF22_RENDER "RenderThread"
#define MF22_RENDER_LEN 12

/* binder:* thread*/
#define MF22_BINDER "binder:"
#define MF22_BINDER_LEN 7

/* composer@* thread*/
#define MF22_COMPOSER "composer@"
#define MF22_COMPOSER_LEN 9

enum zram_tasktype{
	ZRAM_KEY_TASK = 0x1,
	ZRAM_FG_TASK = 0x2,
	ZRAM_UI_THREAD = 0x4,
	ZRAM_SURFACE = 0x8,
	ZRAM_RENDER = 0x10,
	ZRAM_BINDER = 0x20,
	ZRAM_COMPOSER = 0x40,
};

#define ZRAM_SEND_MAX_LEN 1024
#define ZRAM_HOUR_BUF_LEN 256
#define ZRAM_MINS_PER_DAY 1440
#define ZRAM_NS_PER_MS  1000000
#define ZRAM_US_PER_MS  1000
#define ZRAM_NS_PER_US  1000
#define ZRAM_MS_PER_SEC  1000

#define ZRAM_24H_TO_MS 86400000
#define ZRAM_16H_TO_MS 57600000
#define ZRAM_8H_TO_MS  28800000

/* seconds after compression is it decompressed */
#define ZRAM_REFAULT_10s    10000UL
#define ZRAM_REFAULT_60s    60000UL
#define ZRAM_REFAULT_300s   300000UL
#define ZRAM_REFAULT_1800s  1800000UL
#define ZRAM_REFAULT_3600s  3600000UL
#define ZRAM_REFAULT_43200s 43200000UL
#define ZRAM_REFAULT_86400s 86400000UL

/* IO delay distribution, unit is us*/
#define ZRAM_IO_DELAY_500us 500U
#define ZRAM_IO_DELAY_1000us 1000U
#define ZRAM_IO_DELAY_3000us 3000U
#define ZRAM_IO_DELAY_5000us 5000U
#define ZRAM_IO_DELAY_30000us 30000U
#define ZRAM_IO_DELAY_50000us 50000U

#define MAX_BACKING_DEV 16
/* backing device: strlen(/dev/block/loop45$$)=19 up to 16 bakcing deivces*/
#define MAX_BDEV_LIST_LEN (20*(MAX_BACKING_DEV))
#define NR_ZRAM_PSI_RESOURCES 3
// USD: add for calculate PSI time by zihan.ju at 20240522 start
struct psi_data{
	spinlock_t psi_update_lock;
	unsigned long psi_update_last_time; // [0]:cpu, [1]:io, [2]:mem
	unsigned long psi_update_next_time; // [0]:cpu, [1]:io, [2]:mem
	unsigned long psi_avg[NR_ZRAM_PSI_RESOURCES];
	unsigned long psi_last_total[NR_ZRAM_PSI_RESOURCES];
	unsigned long psi_update_time;
	unsigned long psi_max[NR_ZRAM_PSI_RESOURCES];
	atomic64_t psi_cpu_times;
	atomic64_t psi_io_times;
	atomic64_t psi_mem_times;
	unsigned long psi_cpu_collect_time;
	atomic64_t psi_io_collect_time;
	atomic64_t psi_mem_collect_time;
};
// USD: add for calculate PSI time by zihan.ju at 20240522 end

#define NR_ZWBS 32
#define ZRAM_WB_THRESHOLD 32
#define ZWBS_ALIGN_MASK (~(NR_ZWBS - 1))

#define AGGR_OFFSET_SHIFT PAGE_SHIFT
#define AGGR_ALGO_SHIFT (PAGE_SHIFT * 2)
#define AGGR_BLKIDX_SHIFT (PAGE_SHIFT * 2 + 2)

#define AGGR_CPRS_SZ_MASK (PAGE_SIZE - 1)
#define AGGR_OFFSET_MASK (PAGE_SIZE - 1)

struct zram_wb_header {
	u32 index;
	u32 size;
};

struct zram_wb_work {
	struct work_struct work;
	struct page *src_page;
	struct page *dst_page;
	struct bio *bio;
	struct bio *bio_chain;

	struct zram *zram;

	unsigned long handle;
	u64 start;	/*submit_bio time*/
	u32 flags;
	u32 index;	/*zram->table[index]*/
};

struct zram_wb_entry {
	unsigned long index;
	unsigned int offset;
	unsigned int size;
	unsigned int prio;
	unsigned int stay_time;
};

/* sizeof(zwbs) = 784 */
struct zwbs {
	struct zram_wb_entry entry[ZRAM_WB_THRESHOLD];
	struct page *page;
	u32 cnt;
	u32 off;
};

struct zram_writeback_buffer {
	struct zwbs *zwbs[NR_ZWBS];
	int cnt;
	int objcnt;
};

#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 20241001 end

/*
 * ZRAM is mainly used for memory efficiency so we want to keep memory
 * footprint small and thus squeeze size and zram pageflags into a flags
 * member. The lower ZRAM_FLAG_SHIFT bits is for object size (excluding
 * header), which cannot be larger than PAGE_SIZE (requiring PAGE_SHIFT
 * bits), the higher bits are for zram_pageflags.
 *
 * We use BUILD_BUG_ON() to make sure that zram pageflags don't overflow.
 */
#define ZRAM_FLAG_SHIFT (PAGE_SHIFT + 1)

/* Only 2 bits are allowed for comp priority index */
#define ZRAM_COMP_PRIORITY_MASK	0x3

/* Flags for zram pages (table[page_no].flags) */
enum zram_pageflags {
	/* zram slot is locked */
	ZRAM_LOCK = ZRAM_FLAG_SHIFT,
	ZRAM_SAME,	/* Page consists the same element */
	ZRAM_WB,	/* page is stored on backing_device */
	ZRAM_UNDER_WB,	/* page is under writeback */
	ZRAM_HUGE,	/* Incompressible page */
	ZRAM_IDLE,	/* not accessed page since last idle marking */
	ZRAM_INCOMPRESSIBLE, /* none of the algorithms could compress it */

	ZRAM_COMP_PRIORITY_BIT1, /* First bit of comp priority index */
	ZRAM_COMP_PRIORITY_BIT2, /* Second bit of comp priority index */
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 20241001 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	ZRAM_COLD, /* Page is reclaimed by kernel (not framework or google thread)*/
	ZRAM_KEY, /* set key tasks flag which will not be allowed write back to ROM*/
	ZRAM_APPCOMPACT, /* The page was compressed by appcompact. */
	ZRAM_READ_BDEV,
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 20241001 end

#ifdef CONFIG_ESWAP
	ZRAM_BATCHING_OUT,
	ZRAM_FROM_ESWAP,
	ZRAM_MCGID_CLEAR,
	ZRAM_ESWAP_TRACK,
#endif
	__NR_ZRAM_PAGEFLAGS,
};

/*-- Data structures */

/* Allocated for each disk page */
struct zram_table_entry {
	union {
		unsigned long handle;
		unsigned long element;
	};
	unsigned long flags;
#ifdef CONFIG_ZRAM_TRACK_ENTRY_ACTIME
	ktime_t ac_time;
#endif

//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 20241001 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	struct list_head fifo_klist;
	int bd_index;
	unsigned int stay_time;/*Record the time the page stayed in zram before being moved to ROM. unit is seconds*/
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 20241001 end
};

//USD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 20240520 start
struct last_stats{
	s64 bd_reads;		/* old no. of reads from backing device */
	s64 fg_reads;		/* old no. of foreground process reads from backing device */

	s64 key_process_reads;
	s64 surface_reads;	/* old no. of surfaceflinger reads from backing device */
	s64 binder_reads;	/* old no. of binder writes from backing device */
	s64 render_reads;	/* old no. of render read from backing device */
	s64 ui_reads;       	/* old no. of UI thread read from backing device */
	s64 composer_reads;     /* old no. of composer_reads thread read from backing device */
	s64 draw_reads;		/* old no. of render/binder/surfaceflinger read from backing device */

	s64 swapin_count;
	s64 swapin_delay;
	s64 draw_io_delay;

	u64 zram_reads;		/* old no. of reads from zram */
	u64 zram_writes;	/* old no. of writes from zram */
	u64 sum_exec_runtime;	/* old time of zram_wbd thread */
};

// USD: add for get hour stats by zihan.ju at 20240705 start
#define ZRAM_READBACK_24HOUR 24
struct hour_stats{
	atomic_t hout_stat_data[ZRAM_READBACK_24HOUR];
	unsigned int hout_stat_index;
	unsigned int hout_stat_last_data;	/* no. of pages read from backing device */
	ktime_t hout_stat_next;
	spinlock_t hout_stat_update_lock;
};
// USD: add for get hour stats by zihan.ju at 20240705 end

struct wbd_stats{
	ktime_t last_report;    /* call submit_msg to Big-Data-Center timestamp*/
	struct last_stats old;
	struct hour_stats rd_hour;

	atomic64_t fg_reads;		/* no. of pages read from backing device */
	atomic64_t surface_reads;	/* no. of pages surface read from backing device */
	atomic64_t binder_reads;	/* no. of pages binder read from backing device */
	atomic64_t render_reads;	/* no. of pages render read from backing device */
	atomic64_t ui_reads;		/* no. of pages ui read from backing device */
	atomic64_t composer_reads;     /*  no. of pages composer reads thread read from backing device */
	atomic64_t key_process_reads;	/* no. of pages key process read from backing device */
};
//USD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 20240520 end

struct zram_stats {
	atomic64_t compr_data_size;	/* compressed size of pages stored */
	atomic64_t failed_reads;	/* can happen when memory is too low */
	atomic64_t failed_writes;	/* can happen when memory is too low */
	atomic64_t notify_free;	/* no. of swap slot free notifications */
	atomic64_t same_pages;		/* no. of same element filled pages */
	atomic64_t huge_pages;		/* no. of huge pages */
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	atomic64_t num_reads;	/* failed + successful */
	atomic64_t num_writes;	/* --do-- */
	atomic64_t meta_data_size;	/* size of zram_entries */

	atomic64_t bd_objcnt;
	atomic64_t bd_size;
	atomic64_t bd_max_count;
	atomic64_t bd_max_size;

	atomic64_t bd_size_dec_except;
	atomic64_t bd_size_inc_except;
	atomic64_t bd_table_except;
#endif
	atomic64_t huge_pages_since;	/* no. of huge pages since zram set up */
	atomic64_t pages_stored;	/* no. of pages currently stored */
	atomic_long_t max_used_pages;	/* no. of maximum pages stored */
	atomic64_t writestall;		/* no. of write slow paths */
	atomic64_t miss_free;		/* no. of missed free */
#ifdef	CONFIG_ZRAM_WRITEBACK
	atomic64_t bd_count;		/* no. of pages in backing device */
	atomic64_t bd_reads;		/* no. of reads from backing device */
	atomic64_t bd_writes;		/* no. of writes from backing device */
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	atomic64_t bd_fwk_reclaim;
	atomic64_t bd_ker_reclaim;
	/* used for collect page writeback by writeback_store*/
	atomic64_t	bd_wb_reclaim;
	/* compresss & decompress stats*/
	atomic64_t compress_delay[ZRAM_MAX_COMPS];
	atomic64_t compress_pages[ZRAM_MAX_COMPS];
	atomic64_t decompress_delay[ZRAM_MAX_COMPS];
	atomic64_t decompress_pages[ZRAM_MAX_COMPS];
	atomic64_t recompress_delay[ZRAM_MAX_COMPS];
	atomic64_t recompress_pages[ZRAM_MAX_COMPS];
#ifdef CONFIG_ZRAM_MULTI_COMP
	atomic64_t wkdm_pages;	/* no. of WKDM-compressed pages currently stored */
	atomic64_t incompressible_pages;	/* no. of incompressible pages currently stored */
	atomic64_t incompressible_pages_since;	/* no. of incompressible pages since zram set up */
	atomic64_t skip_switch_alg_pages;	/* no. of pages skip switching secondary algorithm due to low compress ratio */
	atomic64_t wkdm_compr_pages_since;	/* no. of WKDM-compressed pages since zram set up */
	atomic64_t wkdm_compr_data_size;	/* WKDM compressed size of pages stored since zram set up */
	atomic64_t lz4_compr_pages_since;	/* no. of LZ4-compressed pages since zram set up */
	atomic64_t lz4_compr_data_size;	/* LZ4 compressed size of pages stored since zram set up */
#endif
	/* used for calculate the time it takes to read/write a page from backing device by dongyun.liu*/
	atomic64_t swapin_count;
	atomic64_t swapout_count;
	/* unit is us */
	atomic64_t swapin_delay;
	atomic64_t swapout_delay;
	atomic64_t draw_io_delay;
	/* swap refault time distribution */
	atomic64_t rf_lt_10; /*stay 0~10s */
	atomic64_t rf_lt_60; /*stay 10~60s */
	atomic64_t rf_lt_300; /*stay 60~300s */
	atomic64_t rf_lt_1800; /*stay 300~1800 */
	atomic64_t rf_lt_3600; /*stay 1800~3600s */
	atomic64_t rf_lt_43200; /*stay 3600~43200s */
	atomic64_t rf_lt_86400; /*stay 43200~86400s */
	atomic64_t rf_gt_86400; /*stay 86400~unlimited */
	/* IO delay distribution */
	atomic64_t io_lt_500; /* 0~500 us/page */
	atomic64_t io_lt_1000; /*500~1000 us/page */
	atomic64_t io_lt_3000; /*1000~3000 us/page */
	atomic64_t io_lt_5000; /*3000~5000 us/page */
	atomic64_t io_lt_30000; /*5000~30000 us/page */
	atomic64_t io_lt_50000; /*30000~50000 us/page */
	atomic64_t io_gt_50000; /*50000~unlimited us/page */

	/* no. of pages of key process such as systemui/system_server/surfaceflinger*/
	atomic64_t key_pages;
	/* no. of pages of are not in /dev/memcg/antireclaim group*/
	atomic64_t wbd_klist_pages;
	// All the totaldata need zram_update_count() to update data
	atomic64_t bd_count_list[MAX_BACKING_DEV];	/* no. of pages in backing device list */
	atomic64_t bd_reads_list[MAX_BACKING_DEV];	/* no. of reads from backing device list */
	atomic64_t bd_writes_list[MAX_BACKING_DEV]; 	/* no. of writes from backing device list */
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end
};

struct zram {
	struct zram_table_entry *table;
	struct zs_pool *mem_pool;
	struct zcomp *comps[ZRAM_MAX_COMPS];
	struct gendisk *disk;
	/* Prevent concurrent execution of device init */
	struct rw_semaphore init_lock;
	/*
	 * the number of pages zram can consume for storing compressed data
	 */
	unsigned long limit_pages;

	struct zram_stats stats;
	/*
	 * This is the limit on amount of *uncompressed* worth of data
	 * we can store in a disk.
	 */
	u64 disksize;	/* bytes */
	const char *comp_algs[ZRAM_MAX_COMPS];
	s8 num_active_comps;
	/*
	 * zram is claimed so open request will be failed
	 */
	bool claim; /* Protected by disk->open_mutex */
#ifdef CONFIG_ZRAM_WRITEBACK
	struct file *backing_dev;
	spinlock_t wb_limit_lock;
	bool wb_limit_enable;
	u64 bd_wb_limit;
	struct block_device *bdev;
	unsigned long *bitmap;
	unsigned long nr_pages;
#endif
#ifdef CONFIG_ESWAP
	struct eswap_area *area;
#endif
#ifdef CONFIG_ZRAM_MEMORY_TRACKING
	struct dentry *debugfs_dir;
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	CACHELINE_PADDING(_pad1_);

	/* Prevent contention when calling try_wakeup_zram_wbd_lite */
	spinlock_t wakeup_wbd_lock;
	spinlock_t wb_table_lock;
	spinlock_t chunk_bitmap_lock;
	spinlock_t wbd_klist_lock;

	atomic_t backing_dev_index;

	struct file *backing_dev_list[MAX_BACKING_DEV];
	struct block_device *bdev_list[MAX_BACKING_DEV];
	unsigned long nr_pages_list[MAX_BACKING_DEV];
	unsigned long* bitmap_list[MAX_BACKING_DEV];
	unsigned long* chunk_bitmap_list[MAX_BACKING_DEV];
	u8* wb_table[MAX_BACKING_DEV];

	unsigned long nr_pages_total;

	// USD: add for calculate PSI time by zihan.ju at 20240522 start
	struct psi_data wbd_psi_data;
	// USD: add for calculate PSI time by zihan.ju at 20240522 end

	struct task_struct *wbd;
	wait_queue_head_t wbd_wait;
	bool wbd_running;
	struct list_head wbd_klist;
	struct list_head wbd_anti_list;
	bool io_complete;
	struct kernel_cpustat prev_cpustat;
	struct wbd_stats wbd_stat;
#ifdef CONFIG_ZRAM_MULTI_COMP
	atomic64_t recomp_pages;
	struct task_struct *recompd;
	struct kernel_cpustat recomp_cstat;
#endif
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end
};

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
extern struct zram *g_backingdev;
extern void init_psi_task(struct psi_data* wbd_psi_data);
extern void init_hour_stats(struct hour_stats* stat_data);
extern void get_all_cpustat(struct kernel_cpustat *cpu_stat);
extern void memfusion_psi_cpu(struct psi_data *wbd_psi_data, unsigned long cpu_data, bool update_flag);
extern void memfusion_psi_io(struct psi_data *wbd_psi_data, unsigned long add_data, bool update_flag);
extern void memfusion_psi_mem(struct psi_data *wbd_psi_data, unsigned long add_data, bool update_flag);
extern bool try_to_add_to_table(struct hour_stats* stat_data, ktime_t now, int data);
#endif

#ifdef CONFIG_ESWAP
static inline struct zram *dev_to_zram(struct device *dev)
{
	return (struct zram *)dev_to_disk(dev)->private_data;
}

static inline int zram_slot_trylock(struct zram *zram, u32 index)
{
	return bit_spin_trylock(ZRAM_LOCK, &zram->table[index].flags);
}

static inline void zram_slot_lock(struct zram *zram, u32 index)
{
	bit_spin_lock(ZRAM_LOCK, &zram->table[index].flags);
}

static inline void zram_slot_unlock(struct zram *zram, u32 index)
{
	bit_spin_unlock(ZRAM_LOCK, &zram->table[index].flags);
}

static inline unsigned long zram_get_handle(struct zram *zram, u32 index)
{
	return zram->table[index].handle;
}

static inline void zram_set_handle(struct zram *zram,
					u32 index, unsigned long handle)
{
	zram->table[index].handle = handle;
}

static inline bool zram_test_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	return zram->table[index].flags & BIT(flag);
}

static inline void zram_set_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	zram->table[index].flags |= BIT(flag);
}

static inline void zram_clear_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	zram->table[index].flags &= ~BIT(flag);
}


static inline void zram_set_element(struct zram *zram, u32 index,
			unsigned long element)
{
	zram->table[index].element = element;
}

static inline unsigned long zram_get_element(struct zram *zram, u32 index)
{
	return zram->table[index].element;
}

static inline size_t zram_get_obj_size(struct zram *zram, u32 index)
{
	return zram->table[index].flags & (BIT(ZRAM_FLAG_SHIFT) - 1);
}

static inline void zram_set_obj_size(struct zram *zram,
					u32 index, size_t size)
{
	unsigned long flags = zram->table[index].flags >> ZRAM_FLAG_SHIFT;

	zram->table[index].flags = (flags << ZRAM_FLAG_SHIFT) | size;
}

#endif // CONFIG_ESWAP

#endif
