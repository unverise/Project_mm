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

#define KMSG_COMPONENT "zram"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/bitops.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/backing-dev.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/err.h>
#include <linux/idr.h>
#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/cpuhotplug.h>
#include <linux/part_stat.h>


//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
#include <linux/sched/loadavg.h>
#include <linux/sched/cputime.h>
#include <linux/sched/stat.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/clock.h>

#include <linux/ftrace.h>
#include <linux/delay.h>
#include <linux/swap.h>

#include <linux/sched/loadavg.h>

#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/jiffies.h>
#include <linux/vmstat.h>
#include <linux/statfs.h>
#include <uapi/linux/sched/types.h>
#include <linux/proc_fs.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/notifier.h>

#include <linux/signal.h>
#include <linux/sched/signal.h>
#include <linux/pid.h>
#include <linux/signal_types.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/notifier.h>
#include "tranlog.h"

#include <linux/sched.h>
#include <linux/memcontrol.h>
#include <linux/err.h>
#include <linux/page_counter.h>
#include <linux/cgroup.h>
#include <trace/hooks/vmscan.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/cgroup.h>
#include <trace/hooks/sched.h>
#include <linux/tracepoint.h>
#include <linux/kprobes.h>
#include <trace/hooks/dtask.h>
#if IS_ENABLED(CONFIG_TRANS_SCHED_FEATURE)
#include <trans_sched_interface.h>
#endif
#if IS_ENABLED(CONFIG_TRANSSION_BLK)
#include "transsion-blk-types.h"
#endif
#include "tranlog.h"
#include "backend_wkdm.h"
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end
#include "zram_drv.h"
#include "zram_process_reclaim.h"
static DEFINE_IDR(zram_index_idr);
/* idr index must be protected */
static DEFINE_MUTEX(zram_index_mutex);

#ifdef CONFIG_ESWAP
#include <linux/memcontrol.h>
#include "eswap/eswap_api.h"
#endif
#if defined(CONFIG_ESWAP) || defined(CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT)
#include "eswap/memcg_control.h"
#endif
static int zram_major;
static const char *default_compressor = CONFIG_ZRAM_DEF_COMP;

/* Module params (documentation at end) */
static unsigned int num_devices = 1;

#define MF22PRELOG "[MF2.2] "

//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
#define PAGE2KB(pages)		((pages) << (PAGE_SHIFT - 10))
#define PAGE2MB(pages)		((pages) >> (20 - PAGE_SHIFT))

/* Normal version: writeback 4M per-time*/
#define DEFAULT_ZRAM_WRITE_LIMIT 1024

__read_mostly int g_swap_kernel_write_enable;
__read_mostly int g_swap_fwk_write_enable;
/* process compress switch */
__read_mostly bool g_process_reclaim_enable = false;
/* memcg compress switch */
__read_mostly bool g_memcg_compress_enable = false;
__read_mostly bool g_memcg_compress_v2_enable = false;

__read_mostly bool zram_skip_compress_abort = false;
module_param(zram_skip_compress_abort, bool, 0644);

__read_mostly bool zram_reclaim_shared_page_enable = false;
module_param(zram_reclaim_shared_page_enable, bool, 0644);

__read_mostly static bool zram_cpu_io_enable = true;
module_param(zram_cpu_io_enable, bool, 0644);

//static unsigned long long next_wakeup_jiffies = 0UL;
static u64 next_update_jiffies = 0UL;
static s64 fwk_thrashing[3] = {0L,0L,0L};
static atomic64_t fwk_thrasing_pages_delta = ATOMIC_INIT(0);
static DEFINE_SPINLOCK(fwk_thrash_lock);

int zram_write_limit_debug = DEFAULT_ZRAM_WRITE_LIMIT;
module_param(zram_write_limit_debug, int, 0644);

int zram_kwrite_boost = 0;
module_param(zram_kwrite_boost, int, 0644);

#define MEMFUSION_DRAW_FLAG 0x1
struct zram_endio{
	bio_end_io_t* callback;
	u64 start;
	u32 index;
	u32 flags;
};

/* we don't care about concurrency write/read. */
static unsigned int ui_swapsize = 0;
static unsigned int ui_switch_fail_cnt = 0;

__read_mostly int zram_wbd_debug = 0;
module_param(zram_wbd_debug, int, 0644);

__read_mostly bool zram_vip_io_sched_enable = true;
module_param(zram_vip_io_sched_enable, bool, 0644);

__read_mostly static bool zram_writeback_vmpressure = false;
module_param(zram_writeback_vmpressure, bool, 0644);

__read_mostly static bool zram_writeback_aggregation = true;
module_param(zram_writeback_aggregation, bool, 0644);

__read_mostly static bool zram_delayacct_enable = false;
module_param(zram_delayacct_enable, bool, 0644);

static atomic64_t zram_bd_index_times = ATOMIC64_INIT(0);

/* report one time must be over 20h */
__read_mostly int zram_wbd_report = 72000000;
module_param(zram_wbd_report, int, 0644);

int zram_wb_switch_delay_time = 5;
module_param(zram_wb_switch_delay_time, int, 0644);

unsigned long long zram_write_limit_time_stamp = 0;

#define ZRAM_NR_PAGES_4G 1048576UL
static int zram_writeback_disable_gt4G = 1;
module_param(zram_writeback_disable_gt4G, int, 0644);

/*used to enable "echo XXX > /sys/block/zram0/writeback". default disabled*/
static bool zram_writeback_store_enabled = 0;
module_param(zram_writeback_store_enabled, bool, 0644);

static unsigned long long zram_writeback_store_times = 0;

static int zram_write_limit_timeup = 30;	//zram write limit last at most 30s.

static unsigned long cpu_usage_threshold = 60;  //!!! need to define threshold
static unsigned long iowait_threshold = 30;  //!!! need to define threshold
static unsigned long loadavg_threshold = 35;
#ifdef CONFIG_ZRAM_MULTI_COMP
static unsigned int zram_comp_ratio_limit = 40; //zram compress ratio low threshold
module_param(zram_comp_ratio_limit, uint, 0644);
static unsigned long zram_recomp_min_pages = 3000; //the min number of zram recompressible pages
module_param(zram_recomp_min_pages, ulong, 0644);
static unsigned int zram_recomp_cpu_usage_thd = 30; //zram recompression cpu usage threshold
module_param(zram_recomp_cpu_usage_thd, uint, 0644);
static unsigned int zram_recomp_interval = 30; //zram recompression timer interval in seconds
module_param(zram_recomp_interval, uint, 0644);
#endif
static atomic_t high_loading_cooling = ATOMIC_INIT(0);
static unsigned long long high_loading_cooling_cnt = 0;
static unsigned long long screen_on_break_cnt = 0;

#define MEM_CGROUP_NAME_MAX_LEN 32

static int cpu_io_update_interval = 5; //5s, cpu & io usage update interval in get_cpu_io_usage()
static unsigned long long cpu_usage_cnt = 0;
static unsigned long long iowait_cnt = 0;
static unsigned long long loadavg_cnt = 0;
static unsigned long long cpu_io_skip_cnt = 0;

unsigned long long bdev_write_page_time = 0;
unsigned long long bdev_write_page_count = 0;

static unsigned long long zram_kwbd_wb_failed = 0;

/*read and write synchronization*/
static atomic64_t zram_wbd_ifbreak = ATOMIC_INIT(0);
static unsigned long long zram_wbd_ifbreak_times = 0;
/*synchronization of screen on and off
* 1:screen off means allow write-back.
* 0:screen on means don't allow write-back.
*/
static atomic_t zram_wbd_switch = ATOMIC_INIT(0);
static unsigned long long time_stamp_wakeup;
static bool skip_wakeup = false;

static int zram_bdev_install_error = 0;
static int zram_bdev_install_errorno = 0;

enum zram_bdev_install_failed_reason {
    ZRAM_BDEV_STATE_OK = 0,
    ZRAM_BDEV_OPEN_ERROR = 1,
    ZRAM_BDEV_FALLOCATE_ERROR = 2,
    ZRAM_BDEV_LD_ATTACT_ERROR = 3,
    ZRAM_BDEV_LDFD_GET_ERROR = 4,
    ZRAM_BDEV_LD_ENABLE_DIRECTIO_ERROR = 5,
    ZRAM_BDEV_INSTALL_BDEV_ERROR = 6,
};

unsigned long bdev_size = 0;
static char backing_dev_size[6]= "0.00";
static bool g_muticomp_enable __read_mostly = true;
static char *tran_zram_cloud_config_ID = "1003000057";
/* muticomp_enable:  >0 enabled, <0 disabled */
static char *cloud_key_muticomp_enable = "muticomp_enable";

static bool update_could_key_muticomp_enable(bool enabled)
{
	bool old = g_muticomp_enable;
	g_muticomp_enable = enabled;

	return old;
}

static int tran_zram_cloud_config_changed(struct notifier_block *nb, unsigned long action, void* data)
{
	int state;
	struct cloud_engine_config* cec;
	char* id;

	if (!data){
		pr_err(MF22PRELOG"%s: data is NULL, use the default configuration.\n", __func__);
		return -1;
	}

	cec = (struct cloud_engine_config*)data;
	id = cec->id;
	if (strcmp(tran_zram_cloud_config_ID, id) != 0) {
		pr_err(MF22PRELOG"%s: id %s is mismatch with %s, use the default configuration.\n",
				__func__, id, tran_zram_cloud_config_ID);
		return 0;
	}

	state = cloudengine_getIntByID(tran_zram_cloud_config_ID, cloud_key_muticomp_enable);
	if (0 == state)
		pr_err(MF22PRELOG"%s: get %s value failed\n", __func__, cloud_key_muticomp_enable);
	else
		pr_info(MF22PRELOG"%s: %s changes from %d to %d\n", __func__, cloud_key_muticomp_enable,
				update_could_key_muticomp_enable(state > 0), state);

	return 0;
}

struct notifier_block tran_zram_cloud_notifier_block = {
	.notifier_call = tran_zram_cloud_config_changed,
};

#define ZRAM_MEMFUSOIN_OLD 0x1
#define ZRAM_MEMCG_COMPACT_V2 0x2
int memfusion_version(void)
{
	if (g_swap_kernel_write_enable)
		return ZRAM_MEMFUSOIN_OLD;
	else if (g_memcg_compress_v2_enable)
		return ZRAM_MEMCG_COMPACT_V2;

	return 0;
}

#define is_memfusion (ZRAM_MEMFUSOIN_OLD == memfusion_version())
#define is_memcg_compact_v2 (ZRAM_MEMCG_COMPACT_V2 == memfusion_version())

#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end

#if defined(CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT) && defined(CONFIG_FTRACE)

#define ZRAM_WBD_PAGE_CNT        "zram_pages_count"
#define ZRAM_WBD_WRITE_IO        "zram_wbd_write_io"
#define ZRAM_READ_IO            "zram_wbd_read_io"
#define ZRAM_RESET_NAME        "zram_reset_last_bdev"
#define ZRAM_WBD_OUT_REASON        "zram_out_reason"
#define ZRAM_WBD_AGGR_WRITE        "zram_aggr_write_io"
#define ZRAM_READ_PAGE            "zram_read_page"

/*
 * it means that the page being read was moved to backing-dev
 * by writeback_store(e.g google or framework).
 */
#define ZRAM_READ_USER_IO        "zram_read_user_io"

/*it means that the page being read was moved to backing-dev by kernel.*/
#define ZRAM_READ_KERN_IO   "zram_read_kern_io"


#define MF22_TRACE_MAX_LEN 128
static noinline int tracing_mark_write(char *fmt, ...)
{
	char buf[MF22_TRACE_MAX_LEN];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (len >= MF22_TRACE_MAX_LEN) {
		pr_err("%s trace size %u exceed limit", __func__, len);
		return -1;
	}

	trace_puts(buf);
	return 0;
}

inline void zram_kernel_trace_start(char * name)
{
	tracing_mark_write("B|%d|%s\n",current->tgid,name);
}

inline void zram_kernel_trace_change(char * tag , int flag)
{
	tracing_mark_write("C|%d|%s|%d\n", current->tgid, tag, flag);
}

inline void zram_kernel_trace_end(char * name)
{
	tracing_mark_write("E|%d\n",current->tgid);
}
inline void zram_kernel_trace_read_start(bool is_kern, unsigned int seconds)
{
	if (is_kern)
		tracing_mark_write("B|%u|%u|%s\n",
			current->tgid, seconds, ZRAM_READ_KERN_IO);
	else
		tracing_mark_write("B|%u|%u|%s\n",
			current->tgid, seconds, ZRAM_READ_USER_IO);
}
inline void zram_kernel_trace_read_end(bool is_kern, unsigned int seconds)
{
	tracing_mark_write("E|%u|%u\n",current->tgid, seconds);
}

inline void zram_aggr_trace_start(int val, char *name)
{
	tracing_mark_write("B|%d|%d|%s\n", current->tgid, val, name);
}

inline void zram_aggr_trace_end(int val, char *name)
{
	tracing_mark_write("E|%d|%d\n", current->tgid, val);
}

inline void trace_zram_read_page_start(int prio)
{
        tracing_mark_write("B|%d|%d|%s\n", current->tgid, prio, ZRAM_READ_PAGE);
}

inline void trace_zram_read_page_end(int prio)
{
        tracing_mark_write("E|%d|%d|%s\n", current->tgid, prio, ZRAM_READ_PAGE);
}
#else
static inline void zram_kernel_trace_start(char * name){}
static inline void zram_kernel_trace_change(char * tag , int flag){}
static inline void zram_kernel_trace_end(char * name){}
static inline void zram_kernel_trace_read_start(bool is_kern, unsigned int seconds) {}
static inline void zram_kernel_trace_read_end(bool is_kern, unsigned int seconds) {}
static inline void zram_aggr_trace_start(int val, char *name){}
static inline void zram_aggr_trace_end(int val, char *name){}
#endif

/*
 * Pages that compress to sizes equals or greater than this are stored
 * uncompressed in memory.
 */
static size_t huge_class_size;

static const struct block_device_operations zram_devops;

static void zram_free_page(struct zram *zram, size_t index);
static int zram_read_page(struct zram *zram, struct page *page, u32 index,
			  struct bio *parent);

#ifndef CONFIG_ESWAP
static int zram_slot_trylock(struct zram *zram, u32 index)
{
	return bit_spin_trylock(ZRAM_LOCK, &zram->table[index].flags);
}

static void zram_slot_lock(struct zram *zram, u32 index)
{
	bit_spin_lock(ZRAM_LOCK, &zram->table[index].flags);
}

static void zram_slot_unlock(struct zram *zram, u32 index)
{
	bit_spin_unlock(ZRAM_LOCK, &zram->table[index].flags);
}

static inline struct zram *dev_to_zram(struct device *dev)
{
	return (struct zram *)dev_to_disk(dev)->private_data;
}

#endif // !CONFIG_ESWAP

static inline bool init_done(struct zram *zram)
{
	return zram->disksize;
}

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
static int zram_get_bd_index(struct zram *zram, u32 index)
{
	return zram->table[index].bd_index;
}
static void zram_set_bd_index(struct zram *zram, u32 index, int bd_index)
{
	zram->table[index].bd_index = bd_index;
}
#endif

#ifndef CONFIG_ESWAP
static unsigned long zram_get_handle(struct zram *zram, u32 index)
{
	return zram->table[index].handle;
}

static void zram_set_handle(struct zram *zram, u32 index, unsigned long handle)
{
	zram->table[index].handle = handle;
}

/* flag operations require table entry bit_spin_lock() being held */
static bool zram_test_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	return zram->table[index].flags & BIT(flag);
}

static void zram_set_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	zram->table[index].flags |= BIT(flag);
}

static void zram_clear_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	zram->table[index].flags &= ~BIT(flag);
}

static inline void zram_set_element(struct zram *zram, u32 index,
			unsigned long element)
{
	zram->table[index].element = element;
}

static unsigned long zram_get_element(struct zram *zram, u32 index)
{
	return zram->table[index].element;
}

static size_t zram_get_obj_size(struct zram *zram, u32 index)
{
	return zram->table[index].flags & (BIT(ZRAM_FLAG_SHIFT) - 1);
}

static void zram_set_obj_size(struct zram *zram,
					u32 index, size_t size)
{
	unsigned long flags = zram->table[index].flags >> ZRAM_FLAG_SHIFT;

	zram->table[index].flags = (flags << ZRAM_FLAG_SHIFT) | size;
}
#endif // !CONFIG_ESWAP

static inline bool zram_allocated(struct zram *zram, u32 index)
{
	return zram_get_obj_size(zram, index) ||
			zram_test_flag(zram, index, ZRAM_SAME) ||
			zram_test_flag(zram, index, ZRAM_WB);
}

#if PAGE_SIZE != 4096
static inline bool is_partial_io(struct bio_vec *bvec)
{
	return bvec->bv_len != PAGE_SIZE;
}
#define ZRAM_PARTIAL_IO		1
#else
static inline bool is_partial_io(struct bio_vec *bvec)
{
	return false;
}
#endif

static inline void zram_set_priority(struct zram *zram, u32 index, u32 prio)
{
	prio &= ZRAM_COMP_PRIORITY_MASK;
	/*
	 * Clear previous priority value first, in case if we recompress
	 * further an already recompressed page
	 */
	zram->table[index].flags &= ~(ZRAM_COMP_PRIORITY_MASK <<
				      ZRAM_COMP_PRIORITY_BIT1);
	zram->table[index].flags |= (prio << ZRAM_COMP_PRIORITY_BIT1);
}

static inline u32 zram_get_priority(struct zram *zram, u32 index)
{
	u32 prio = zram->table[index].flags >> ZRAM_COMP_PRIORITY_BIT1;

	return prio & ZRAM_COMP_PRIORITY_MASK;
}

static void zram_accessed(struct zram *zram, u32 index)
{
	zram_clear_flag(zram, index, ZRAM_IDLE);
#ifdef CONFIG_ZRAM_TRACK_ENTRY_ACTIME
	zram->table[index].ac_time = ktime_get_boottime();
#endif
}

static inline void update_used_max(struct zram *zram,
					const unsigned long pages)
{
	unsigned long cur_max = atomic_long_read(&zram->stats.max_used_pages);

	do {
		if (cur_max >= pages)
			return;
	} while (!atomic_long_try_cmpxchg(&zram->stats.max_used_pages,
					  &cur_max, pages));
}

static inline void zram_fill_page(void *ptr, unsigned long len,
					unsigned long value)
{
	WARN_ON_ONCE(!IS_ALIGNED(len, sizeof(unsigned long)));
	memset_l(ptr, value, len / sizeof(unsigned long));
}

static bool page_same_filled(void *ptr, unsigned long *element)
{
	unsigned long *page;
	unsigned long val;
	unsigned int pos, last_pos = PAGE_SIZE / sizeof(*page) - 1;

	page = (unsigned long *)ptr;
	val = page[0];

	if (val != page[last_pos])
		return false;

	for (pos = 1; pos < last_pos; pos++) {
		if (val != page[pos])
			return false;
	}

	*element = val;

	return true;
}

static ssize_t initstate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 val;
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	val = init_done(zram);
	up_read(&zram->init_lock);

	return scnprintf(buf, PAGE_SIZE, "%u\n", val);
}

static ssize_t disksize_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", zram->disksize);
}

static ssize_t mem_limit_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	u64 limit;
	char *tmp;
	struct zram *zram = dev_to_zram(dev);

	limit = memparse(buf, &tmp);
	if (buf == tmp) /* no chars parsed, invalid input */
		return -EINVAL;

	down_write(&zram->init_lock);
	zram->limit_pages = PAGE_ALIGN(limit) >> PAGE_SHIFT;
	up_write(&zram->init_lock);

	return len;
}

static ssize_t mem_used_max_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int err;
	unsigned long val;
	struct zram *zram = dev_to_zram(dev);

	err = kstrtoul(buf, 10, &val);
	if (err || val != 0)
		return -EINVAL;

	down_read(&zram->init_lock);
	if (init_done(zram)) {
		atomic_long_set(&zram->stats.max_used_pages,
				zs_get_total_pages(zram->mem_pool));
	}
	up_read(&zram->init_lock);

	return len;
}

/*
 * Mark all pages which are older than or equal to cutoff as IDLE.
 * Callers should hold the zram init lock in read mode
 */
static void mark_idle(struct zram *zram, ktime_t cutoff)
{
	int is_idle = 1;
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	int index;

	for (index = 0; index < nr_pages; index++) {
		/*
		 * Do not mark ZRAM_UNDER_WB slot as ZRAM_IDLE to close race.
		 * See the comment in writeback_store.
		 *
		 * Also do not mark ZRAM_SAME slots as ZRAM_IDLE, because no
		 * post-processing (recompress, writeback) happens to the
		 * ZRAM_SAME slot.
		 *
		 * And ZRAM_WB slots simply cannot be ZRAM_IDLE.
		 */
		zram_slot_lock(zram, index);
		if (!zram_allocated(zram, index) ||
		    zram_test_flag(zram, index, ZRAM_WB) ||
		    zram_test_flag(zram, index, ZRAM_UNDER_WB) ||
		    zram_test_flag(zram, index, ZRAM_SAME)) {
			zram_slot_unlock(zram, index);
			continue;
		}

#ifdef CONFIG_ZRAM_TRACK_ENTRY_ACTIME
		is_idle = !cutoff ||
			ktime_after(cutoff, zram->table[index].ac_time);
#endif
		if (is_idle)
			zram_set_flag(zram, index, ZRAM_IDLE);
		else
			zram_clear_flag(zram, index, ZRAM_IDLE);
		zram_slot_unlock(zram, index);
	}
}

static ssize_t idle_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	ktime_t cutoff_time = 0;
	ssize_t rv = -EINVAL;

	if (!sysfs_streq(buf, "all")) {
		/*
		 * If it did not parse as 'all' try to treat it as an integer
		 * when we have memory tracking enabled.
		 */
		u64 age_sec;

		if (IS_ENABLED(CONFIG_ZRAM_TRACK_ENTRY_ACTIME) && !kstrtoull(buf, 0, &age_sec))
			cutoff_time = ktime_sub(ktime_get_boottime(),
					ns_to_ktime(age_sec * NSEC_PER_SEC));
		else
			goto out;
	}

	down_read(&zram->init_lock);
	if (!init_done(zram))
		goto out_unlock;

	/*
	 * A cutoff_time of 0 marks everything as idle, this is the
	 * "all" behavior.
	 */
	mark_idle(zram, cutoff_time);
	rv = len;

out_unlock:
	up_read(&zram->init_lock);
out:
	return rv;
}

//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
static int zram_wbd(void *p);
struct zram *g_backingdev = NULL;
struct zram *g_zram;
int g_backing_dev_avail_count = -1;
static char zram_bdev_name_buf[MAX_BDEV_LIST_LEN+1] = {0};
static void zram_accessed(struct zram *zram, u32 index);

void bdev_size_lookup(void);
static void zram_list_add_to_tail(struct zram_table_entry* zram_entry, struct list_head* head,
									struct zram* zram) {
	struct list_head *new = &zram_entry->fifo_klist;

	spin_lock(&zram->wbd_klist_lock);
	if (list_empty(new)) {
		list_add_tail(new, head);
		atomic64_inc(&zram->stats.wbd_klist_pages);
	} else {
		list_move_tail(new, head);
	}
	spin_unlock(&zram->wbd_klist_lock);
}

static void zram_list_del(struct zram_table_entry* zram_entry, struct zram* zram) {
	struct list_head *cur = &zram_entry->fifo_klist;

	if (!list_empty(cur)) {
		list_del_init(cur);
		atomic64_dec(&zram->stats.wbd_klist_pages);
	}
}

// USD: add for get hour stats by zihan.ju at 20240705 start
void init_hour_stats(struct hour_stats* stat_data)
{
	stat_data->hout_stat_index = 0;
	stat_data->hout_stat_last_data = 0;
	stat_data->hout_stat_next = ktime_get_boottime();
	spin_lock_init(&stat_data->hout_stat_update_lock);
}

void add_to_table(struct hour_stats* stat_data, int data)
{
	int index = stat_data->hout_stat_index;

	atomic_set(&stat_data->hout_stat_data[index], (data>0?data:0));
	++index;
	// Max hour stats data is 24
	if(index >= ZRAM_READBACK_24HOUR)
		stat_data->hout_stat_index = 0;
	else
		stat_data->hout_stat_index = index;
}

//Add for get read data/hour
bool try_to_add_to_table(struct hour_stats* stat_data, ktime_t now, int data)
{
	unsigned long miss_hour = 0;
	int add_data = 0;
	int duration = 0;
	int cnt = 0;

	if (!spin_trylock(&stat_data->hout_stat_update_lock)) {
		return false;
	}

	// if the time is not in next hour, return
	if(ktime_compare(stat_data->hout_stat_next, now) >= 0)
		goto out;

	add_data = data - stat_data->hout_stat_last_data;
	// if the time is in next hour, add the miss hour data
	duration = ktime_ms_delta(now, stat_data->hout_stat_next);
	miss_hour = div64_u64(duration , 3600 * 1000);
	if (is_log_enable(ZRAM_READ_LOG))
		pr_err("[Memfusion2_2]now = %lld, update_time = %lld, miss_hour = %lu\n",
			ktime_to_ms(now), ktime_to_ms(stat_data->hout_stat_next), miss_hour);
	if (miss_hour > 0) {
		//add the miss hour data
		while (miss_hour > 0) {
			add_to_table(stat_data, 0);
			miss_hour--;
			cnt++;
			if (cnt >= ZRAM_READBACK_24HOUR) {
				break;
			}
		}
	}

	//update the next hour time
	stat_data->hout_stat_next = ktime_add_ms(stat_data->hout_stat_next, 3600 * 1000 * (miss_hour + 1));
	add_to_table(stat_data, add_data);
	stat_data->hout_stat_last_data = data;

out:
	spin_unlock(&stat_data->hout_stat_update_lock);
	return true;
}

void format_hour_readback(char *buf, int count, bool report) {
	int i, copied;
	ssize_t written = 0;
	atomic_t *hout_stat_data;
	int readback;

	if (!g_backingdev) {
		pr_err("[Memfusion2_2] not ready.\n");
		return;
	}

	hout_stat_data = g_backingdev->wbd_stat.rd_hour.hout_stat_data;
	for (i = 0; i < ZRAM_READBACK_24HOUR; i++) {
		readback = atomic_read(&hout_stat_data[i]);
		if (report) {
			atomic_set(&hout_stat_data[i], 0);
		} else {
			pr_err("[Memfusion2_2][%d] %d\n", i, readback);
		}

		if (i + 1 < ZRAM_READBACK_24HOUR)
			copied = snprintf(buf+written, count, "%d,", readback);
		else
			copied = snprintf(buf+written, count, "%d", readback);
		if (count <= copied) {
			pr_err("[Memfusion2_2] buffer is too small to format hour data\n");
			break;
		}
		written += copied;
		count -= copied;
	}

	if (report)
		pr_err("rsa : [%s]\n", buf);
	else
		pr_err("[Memfusion2_2] [%s] left:%d total:%d\n", buf, count, ZRAM_HOUR_BUF_LEN);

	return;
}
// USD: add for get hour stats by zihan.ju at 20240705 end

void get_all_cpustat(struct kernel_cpustat *cpu_stat)
{
	int i = 0;
	u64 user, nice, system, idle, iowait, irq, softirq, steal;
	u64 guest, guest_nice;

	if (!cpu_stat)
		return;

	user = nice = system = idle = iowait =
	irq = softirq = steal = 0;
	guest = guest_nice = 0;
	for_each_possible_cpu(i) {
		user += kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle += kcpustat_cpu(i).cpustat[CPUTIME_IDLE];
		iowait += kcpustat_cpu(i).cpustat[CPUTIME_IOWAIT];
		irq += kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq += kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		steal += kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		guest += kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		guest_nice += kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];
	}
	cpu_stat->cpustat[CPUTIME_USER] = nsec_to_clock_t(user);
	cpu_stat->cpustat[CPUTIME_NICE] = nsec_to_clock_t(nice);
	cpu_stat->cpustat[CPUTIME_SYSTEM] = nsec_to_clock_t(system);
	cpu_stat->cpustat[CPUTIME_IDLE] = nsec_to_clock_t(idle);
	cpu_stat->cpustat[CPUTIME_IOWAIT] = nsec_to_clock_t(iowait);
	cpu_stat->cpustat[CPUTIME_IRQ] = nsec_to_clock_t(irq);
	cpu_stat->cpustat[CPUTIME_SOFTIRQ] = nsec_to_clock_t(softirq);
	cpu_stat->cpustat[CPUTIME_STEAL] = nsec_to_clock_t(steal);
	cpu_stat->cpustat[CPUTIME_GUEST] = nsec_to_clock_t(guest);
	cpu_stat->cpustat[CPUTIME_GUEST_NICE] = nsec_to_clock_t(guest_nice);
}

static bool debug_log_enable = false;
module_param(debug_log_enable, bool, 0644);
//judge cpu usage percentage and iowait percentage
static unsigned long g_io_time = 0, io_time = 0;
static unsigned long g_usage_time = 0, usage_time = 0;
static unsigned long loadavg[3] = {0};
static bool compare_cpu_stat(struct kernel_cpustat *curr_cpustat, struct kernel_cpustat *prev_cpustat, bool is_recomp)
{
	unsigned long user_time = 0, system_time = 0;
	unsigned long irq_time = 0, idle_time = 0;
	unsigned long total_time = 0, usage_time = 0;

	user_time = (unsigned long) ((curr_cpustat->cpustat[CPUTIME_USER] + curr_cpustat->cpustat[CPUTIME_NICE]) -
							(prev_cpustat->cpustat[CPUTIME_USER] + prev_cpustat->cpustat[CPUTIME_NICE]));
	system_time = (unsigned long) (curr_cpustat->cpustat[CPUTIME_SYSTEM] - prev_cpustat->cpustat[CPUTIME_SYSTEM]);
	io_time = (unsigned long) (curr_cpustat->cpustat[CPUTIME_IOWAIT] - prev_cpustat->cpustat[CPUTIME_IOWAIT]);

	irq_time = (unsigned long) ((curr_cpustat->cpustat[CPUTIME_IRQ] + curr_cpustat->cpustat[CPUTIME_SOFTIRQ]) -
							(prev_cpustat->cpustat[CPUTIME_IRQ] + prev_cpustat->cpustat[CPUTIME_SOFTIRQ]));

	idle_time = (unsigned long) ((curr_cpustat->cpustat[CPUTIME_IDLE] > prev_cpustat->cpustat[CPUTIME_IDLE]) ?
							curr_cpustat->cpustat[CPUTIME_IDLE] - prev_cpustat->cpustat[CPUTIME_IDLE] : 0);

	idle_time += (unsigned long) ((curr_cpustat->cpustat[CPUTIME_STEAL] + curr_cpustat->cpustat[CPUTIME_GUEST]) -
							(prev_cpustat->cpustat[CPUTIME_STEAL] + prev_cpustat->cpustat[CPUTIME_GUEST]));
	total_time = user_time + system_time + io_time + irq_time + idle_time;
	usage_time = (user_time + system_time + io_time + irq_time) * 100;
	io_time = io_time * 100;

	usage_time /= total_time;
	io_time /= total_time;
	if (debug_log_enable) {
		pr_err(MF22PRELOG"cpu usage_time is %lu", usage_time);
		pr_err(MF22PRELOG"cpu io_time is %lu", io_time);
		pr_err(MF22PRELOG"loadavg is %lu", loadavg[0]);
	}

	/* Recompress zram pages if cpu usage is below a cpu usage threshold */
	if (is_recomp) {
		if (usage_time <= zram_recomp_cpu_usage_thd)
			return true;
		else
			return false;
	}

	if (usage_time >= cpu_usage_threshold){
		cpu_usage_cnt++;
		g_usage_time = usage_time;
		return false;
	} else if(io_time >= iowait_threshold) {
		iowait_cnt++;
		g_io_time = io_time;
		return false;
	} else
		return true;
}

static bool get_cpu_io_usage(void)
{
	static unsigned long long cpu_usage_time_stamp = 0;
	struct kernel_cpustat temp_cpustat;

	static bool usage_free = true;

	if (time_is_before_jiffies64(cpu_usage_time_stamp)) {
		cpu_usage_time_stamp = jiffies_64 + cpu_io_update_interval*HZ;
		get_all_cpustat(&temp_cpustat);
		usage_free = compare_cpu_stat(&temp_cpustat, &g_backingdev->prev_cpustat, false);

		memcpy(&g_backingdev->prev_cpustat, &temp_cpustat, sizeof(struct kernel_cpustat));
	}

	return usage_free;

}

static bool get_loadavg(void)
{
		return true;
}

int init_lru_writeback(struct zram *zram)
{
	struct sched_param param = { .sched_priority = 0 };
	int ret = 0;

	init_waitqueue_head(&zram->wbd_wait);

	// USD: add for calculate PSI time by zihan.ju at 20240522 start
	init_psi_task(&zram->wbd_psi_data);
	// USD: add for calculate PSI time by zihan.ju at 20240522 end

	// USD: add for get hour stats by zihan.ju at 20240705 start
	init_hour_stats(&zram->wbd_stat.rd_hour);
	// USD: add for get hour stats by zihan.ju at 20240705 end

	zram->wbd = kthread_run(zram_wbd, zram, "%s_wbd", zram->disk->disk_name);
	if (IS_ERR(zram->wbd)) {
		ret = PTR_ERR(zram->wbd);
		goto out;
	}

	if(g_backingdev == NULL)
		g_backingdev = zram;
	bdev_size = g_backingdev->nr_pages << (PAGE_SHIFT - 10);
	bdev_size_lookup();

	sched_setscheduler(zram->wbd, SCHED_IDLE, &param);
	get_all_cpustat(&g_backingdev->prev_cpustat);

#if IS_ENABLED(CONFIG_TRANSSION_BLK)
	pr_info(MF22PRELOG"init_lru_writeback done, support vip-io\n");
#else
	pr_info(MF22PRELOG"init_lru_writeback done\n");
#endif

out:
	return ret;
}

static void deinit_lru_writeback(struct zram *zram)
{
	pr_err(MF22PRELOG"deinit lru_writeback begin");
	if (!IS_ERR_OR_NULL(zram->wbd)) {
		g_backingdev = NULL;
		kthread_stop(zram->wbd);
		zram->wbd = NULL;
		pr_err(MF22PRELOG"deinit lru_writeback end");
	}
}
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end


#ifdef CONFIG_ZRAM_WRITEBACK
static ssize_t writeback_limit_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	u64 val;
	ssize_t ret = -EINVAL;

	if (kstrtoull(buf, 10, &val))
		return ret;

	down_read(&zram->init_lock);
	spin_lock(&zram->wb_limit_lock);
	zram->wb_limit_enable = val;
	spin_unlock(&zram->wb_limit_lock);
	up_read(&zram->init_lock);
	ret = len;

	return ret;
}

static ssize_t writeback_limit_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	bool val;
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	spin_lock(&zram->wb_limit_lock);
	val = zram->wb_limit_enable;
	spin_unlock(&zram->wb_limit_lock);
	up_read(&zram->init_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t writeback_limit_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	u64 val;
	ssize_t ret = -EINVAL;

	if (kstrtoull(buf, 10, &val))
		return ret;

	down_read(&zram->init_lock);
	spin_lock(&zram->wb_limit_lock);
	zram->bd_wb_limit = val;
	spin_unlock(&zram->wb_limit_lock);
	up_read(&zram->init_lock);
	ret = len;

	return ret;
}

static ssize_t writeback_limit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u64 val;
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	spin_lock(&zram->wb_limit_lock);
	val = zram->bd_wb_limit;
	spin_unlock(&zram->wb_limit_lock);
	up_read(&zram->init_lock);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", val);
}

static void reset_bdev(struct zram *zram)
{
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	u8 *wb_table;
#endif
	struct block_device *bdev;

	if (!zram->backing_dev)
		return;

	bdev = zram->bdev;
	blkdev_put(bdev, zram);
	/* hope filp_close flush all of IO */
	filp_close(zram->backing_dev, NULL);
	zram->backing_dev = NULL;
	zram->bdev = NULL;
	zram->disk->fops = &zram_devops;
	kvfree(zram->bitmap);
	zram->bitmap = NULL;
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 2025/10/01 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	if (is_memfusion) {
		kvfree(zram->chunk_bitmap_list[0]);
		zram->chunk_bitmap_list[0] = NULL;

		spin_lock(&zram->wb_table_lock);
		wb_table = zram->wb_table[0];
		zram->wb_table[0] = NULL;
		spin_unlock(&zram->wb_table_lock);
		kvfree(wb_table);

		deinit_lru_writeback(zram);
	}
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 2025/10/01 end
}

static ssize_t backing_dev_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct file *file;
	struct zram *zram = dev_to_zram(dev);
	char *p;
	ssize_t ret;

	down_read(&zram->init_lock);
	file = zram->backing_dev;
	if (!file) {
		memcpy(buf, "none\n", 5);
		up_read(&zram->init_lock);
		return 5;
	}

	p = file_path(file, buf, PAGE_SIZE - 1);
	if (IS_ERR(p)) {
		ret = PTR_ERR(p);
		goto out;
	}

	ret = strlen(p);
	memmove(buf, p, ret);
	buf[ret++] = '\n';
out:
	up_read(&zram->init_lock);
	return ret;
}

static ssize_t backing_dev_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	char *file_name;
	size_t sz;
	struct file *backing_dev = NULL;
	struct inode *inode;
	struct address_space *mapping;
	unsigned int bitmap_sz;
	unsigned long nr_pages, *bitmap = NULL;
	struct block_device *bdev = NULL;
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	unsigned long *chunk_bitmap = NULL;
	u8* wb_table = NULL;
#endif
	int err;
	struct zram *zram = dev_to_zram(dev);

	if (is_memcg_compact_v2) {
		pr_info(MF22PRELOG"Can't setup backing device for memcg compress v2\n");
		return -EINVAL;
	}

	file_name = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!file_name)
		return -ENOMEM;

	down_write(&zram->init_lock);
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	if (g_backing_dev_avail_count + 1 >= MAX_BACKING_DEV) {
		err = -EINVAL;
		pr_info(MF22PRELOG"Can't setup backing device for over-limit\n");
		goto out;
	}

	if ((g_backing_dev_avail_count == -1) && init_done(zram)) {
		pr_info(MF22PRELOG"Can't setup first backing device for initialized device\n");
		err = -EBUSY;
		goto out;
	} else if ((g_backing_dev_avail_count > -1) && !init_done(zram)) {
		pr_info(MF22PRELOG"Can't setup more backing device for uninitialized device\n");
		err = -EBUSY;
		goto out;
	}
#else
	if (init_done(zram)) {
		pr_info("Can't setup backing device for initialized device\n");
		err = -EBUSY;
		goto out;
	}
#endif

	strscpy(file_name, buf, PATH_MAX);
	/* ignore trailing newline */
	sz = strlen(file_name);
	if (sz > 0 && file_name[sz - 1] == '\n')
		file_name[sz - 1] = 0x00;

	backing_dev = filp_open_block(file_name, O_RDWR|O_LARGEFILE, 0);
	if (IS_ERR(backing_dev)) {
		err = PTR_ERR(backing_dev);
		backing_dev = NULL;
		goto out;
	}

	mapping = backing_dev->f_mapping;
	inode = mapping->host;

	/* Support only block device in this moment */
	if (!S_ISBLK(inode->i_mode)) {
		err = -ENOTBLK;
		goto out;
	}

	bdev = blkdev_get_by_dev(inode->i_rdev, BLK_OPEN_READ | BLK_OPEN_WRITE,
				 zram, NULL);
	if (IS_ERR(bdev)) {
		err = PTR_ERR(bdev);
		bdev = NULL;
		goto out;
	}

	nr_pages = i_size_read(inode) >> PAGE_SHIFT;
	/* Refuse to use zero sized device (also prevents self reference) */
	if (!nr_pages) {
		err = -EINVAL;
		goto out;
	}

	bitmap_sz = BITS_TO_LONGS(nr_pages) * sizeof(long);
	bitmap = kvzalloc(bitmap_sz, GFP_KERNEL);
	if (!bitmap) {
		err = -ENOMEM;
		goto out;
	}

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	chunk_bitmap = kvzalloc(bitmap_sz/NR_ZWBS, GFP_KERNEL);
	if (!chunk_bitmap) {
		err = -ENOMEM;
		goto out;
	}

	wb_table = kvzalloc(sizeof(u8) * nr_pages, GFP_KERNEL);
	if (!wb_table) {
		err = -ENOMEM;
		goto out;
	}

	if (zram->backing_dev) {
		zram->bdev_list[g_backing_dev_avail_count + 1] = bdev;
		zram->backing_dev_list[g_backing_dev_avail_count + 1] = backing_dev;
		zram->bitmap_list[g_backing_dev_avail_count + 1] = bitmap;
		zram->chunk_bitmap_list[g_backing_dev_avail_count + 1] = chunk_bitmap;
		zram->wb_table[g_backing_dev_avail_count + 1] = wb_table;
		zram->nr_pages_list[g_backing_dev_avail_count + 1] = nr_pages;

		zram->nr_pages_total += nr_pages;
		bdev_size = zram->nr_pages_total << (PAGE_SHIFT - 10);
		bdev_size_lookup();
		g_backing_dev_avail_count++;
		up_write(&zram->init_lock);

		pr_info(MF22PRELOG"setup backing device %s\n", file_name);
		kfree(file_name);

		return len;
	}
#endif // CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT

	reset_bdev(zram);

	zram->bdev = bdev;
	zram->backing_dev = backing_dev;
	zram->bitmap = bitmap;
	zram->nr_pages = nr_pages;
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	err = init_lru_writeback(zram);
	if (err){
		pr_err(MF22PRELOG"init lru writeback fail");
		goto init_lru_writeback_fail;
	}

	// Only setup first backing device would come here
	atomic_set(&zram->backing_dev_index, 0);
	zram->bdev_list[0] = bdev;
	zram->backing_dev_list[0] = backing_dev;
	zram->bitmap_list[0] = bitmap;
	zram->chunk_bitmap_list[0] = chunk_bitmap;
	zram->wb_table[0] = wb_table;
	zram->nr_pages_list[0] = nr_pages;
	zram->nr_pages_total += nr_pages;
	bdev_size = zram->nr_pages_total << (PAGE_SHIFT - 10);
	g_backing_dev_avail_count = 0;
	bdev_size_lookup();
	pr_info(MF22PRELOG"backing device index:%d\n", atomic_read(&zram->backing_dev_index));
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end
	up_write(&zram->init_lock);

	pr_info("setup backing device %s\n", file_name);
	kfree(file_name);

	return len;

//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
init_lru_writeback_fail:
	zram->bdev = NULL;
	zram->backing_dev = NULL;
	zram->bitmap = NULL;
	zram->nr_pages = 0;
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end

out:
	kvfree(bitmap);
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	kvfree(chunk_bitmap);
	kvfree(wb_table);
#endif
	if (bdev)
		blkdev_put(bdev, zram);

	if (backing_dev)
		filp_close(backing_dev, NULL);

	up_write(&zram->init_lock);

	kfree(file_name);

	return err;
}

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
static unsigned long chunk_to_blk_idx(unsigned long idx)
{
	return idx * NR_ZWBS;
}

static unsigned long blk_to_chunk_idx(unsigned long idx)
{
	return idx / NR_ZWBS;
}

static unsigned long alloc_chunk_bdev_list(struct zram *zram, int bd_index)
{
	unsigned long blk_idx;
	int i;
	unsigned long chunk_idx = 1;
	unsigned long max_idx = blk_to_chunk_idx(zram->nr_pages_list[bd_index]);

retry:
	/* skip 0 bit to confuse zram.handle = 0 */
	chunk_idx = find_next_zero_bit(zram->chunk_bitmap_list[bd_index], max_idx, chunk_idx);
	if (chunk_idx == max_idx)
		return 0;

	spin_lock(&zram->chunk_bitmap_lock);
	if (test_and_set_bit(chunk_idx, zram->chunk_bitmap_list[bd_index])) {
		spin_unlock(&zram->chunk_bitmap_lock);
		goto retry;
	}
	blk_idx = chunk_to_blk_idx(chunk_idx);
	for (i = 0; i < NR_ZWBS; i++)
		BUG_ON(test_and_set_bit(blk_idx + i, zram->bitmap_list[bd_index]));
	spin_unlock(&zram->chunk_bitmap_lock);
	atomic64_add(NR_ZWBS, &zram->stats.bd_count_list[bd_index]);
	atomic64_add(NR_ZWBS, &zram->stats.bd_count);

	return blk_idx;
}

static void free_chunk_bdev_list(struct zram *zram, unsigned long chunk_idx, int bd_index)
{
	unsigned long blk_idx;
	int i;

	blk_idx = chunk_to_blk_idx(chunk_idx);
	spin_lock(&zram->chunk_bitmap_lock);
	for (i = 0; i < NR_ZWBS; i++) {
		if (test_bit(blk_idx + i, zram->bitmap_list[bd_index])) {
			spin_unlock(&zram->chunk_bitmap_lock);
			return;
		}
	}
	clear_bit(chunk_idx, zram->chunk_bitmap_list[bd_index]);
	spin_unlock(&zram->chunk_bitmap_lock);
}

static unsigned long alloc_block_bdev_list(struct zram *zram, int bd_index)
{
	unsigned long blk_idx = 1;
	u8* wb_table;

retry:
	/* skip 0 bit to confuse zram.handle = 0 */
	blk_idx = find_next_zero_bit(zram->bitmap_list[bd_index], zram->nr_pages_list[bd_index], blk_idx);
	if (blk_idx == zram->nr_pages_list[bd_index])
		return 0;

	spin_lock(&zram->chunk_bitmap_lock);
	if (test_and_set_bit(blk_idx, zram->bitmap_list[bd_index])) {
		spin_unlock(&zram->chunk_bitmap_lock);
		goto retry;
	}
	set_bit(blk_to_chunk_idx(blk_idx), zram->chunk_bitmap_list[bd_index]);
	spin_unlock(&zram->chunk_bitmap_lock);

	spin_lock(&zram->wb_table_lock);
	wb_table = zram->wb_table[bd_index];
	if (wb_table) {
		if (wb_table[blk_idx] > 0)
			atomic64_inc(&zram->stats.bd_table_except);
		wb_table[blk_idx] = 0;
	}
	spin_unlock(&zram->wb_table_lock);

	atomic64_inc(&zram->stats.bd_count_list[bd_index]);
	atomic64_inc(&zram->stats.bd_count);
	return blk_idx;
}
static void free_block_bdev_list(struct zram *zram, unsigned long blk_idx, int bd_index)
{
	int was_set;
	u8* wb_table;

	spin_lock(&zram->wb_table_lock);
	wb_table = zram->wb_table[bd_index];
	if (!wb_table || wb_table[blk_idx] == 0)
		goto out;
	wb_table[blk_idx]--;
	atomic64_dec(&zram->stats.bd_objcnt);

	if (wb_table[blk_idx] > 0) {
		spin_unlock(&zram->wb_table_lock);
		return;
	}
out:
	spin_unlock(&zram->wb_table_lock);
	was_set = test_and_clear_bit(blk_idx, zram->bitmap_list[bd_index]);
	WARN_ON_ONCE(!was_set);
	atomic64_dec(&zram->stats.bd_count_list[bd_index]);
	atomic64_dec(&zram->stats.bd_count);

	free_chunk_bdev_list(zram, blk_to_chunk_idx(blk_idx), bd_index);
}
#else
static unsigned long alloc_block_bdev(struct zram *zram)
{
	unsigned long blk_idx = 1;
retry:
	/* skip 0 bit to confuse zram.handle = 0 */
	blk_idx = find_next_zero_bit(zram->bitmap, zram->nr_pages, blk_idx);
	if (blk_idx == zram->nr_pages)
		return 0;

	if (test_and_set_bit(blk_idx, zram->bitmap))
		goto retry;

	atomic64_inc(&zram->stats.bd_count);
	return blk_idx;
}

static void free_block_bdev(struct zram *zram, unsigned long blk_idx)
{
	int was_set;

	was_set = test_and_clear_bit(blk_idx, zram->bitmap);
	WARN_ON_ONCE(!was_set);
	atomic64_dec(&zram->stats.bd_count);
}
#endif

//USD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 20240506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
/* Please keep consistent with the order of enum zram_key_task. */
static char *zram_key_name[KEY_MAX_PROCESS] = {
	SYSTEM_SERVER,
	SYSTEM_UI,
	MF22_SRUFACE,
	MF22_COMPOSER,
	ZERO_SCREEN,
	SSIONLAUNCHER
};

static void zram_io_delay_stat(struct zram* zram, s64 duration)
{
	if (duration <= ZRAM_IO_DELAY_500us)
		atomic64_inc(&zram->stats.io_lt_500);
	else if (duration <= ZRAM_IO_DELAY_1000us)
		atomic64_inc(&zram->stats.io_lt_1000);
	else if (duration <= ZRAM_IO_DELAY_3000us)
		atomic64_inc(&zram->stats.io_lt_3000);
	else if (duration <= ZRAM_IO_DELAY_5000us)
		atomic64_inc(&zram->stats.io_lt_5000);
	else if (duration <= ZRAM_IO_DELAY_30000us)
		atomic64_inc(&zram->stats.io_lt_30000);
	else if (duration <= ZRAM_IO_DELAY_50000us)
		atomic64_inc(&zram->stats.io_lt_50000);
	else
		atomic64_inc(&zram->stats.io_gt_50000);
}

struct zram_key_task_info {
	pid_t tgid;
	struct mem_cgroup *memcg;
};

/* The list of key task. */
static struct zram_key_task_info zram_key_task_list[KEY_MAX_PROCESS];

/* Calculate the average time of all pages read from ROM by dongyun.liu */
static void zram_custom_end_io(struct work_struct *work)
{
	struct zram_wb_work *zw = container_of(work, struct zram_wb_work, work);
	struct page *src_page = zw->src_page;
	struct page *dst_page = zw->dst_page;
	struct bio *bio = zw->bio;
	s64 duration;
	bool is_draw = zw->flags & MEMFUSION_DRAW_FLAG;
	u8 *src, *dst;

	struct zram* zram = zw->zram;
	ktime_t now = ktime_get_boottime();

	duration = ktime_us_delta(now, zw->start);
	if (duration > 0) {
		zram_io_delay_stat(zram, duration);
		atomic64_add(duration, &zram->stats.swapin_delay);
		atomic64_inc(&zram->stats.swapin_count);
		if (is_draw)
			atomic64_add(duration, &zram->stats.draw_io_delay);
	}

	if (is_log_enable(ZRAM_READ_LOG))
		pr_err("end io:%lld d:%lld cnt:%lld p-bio:0x%px %c %u",
			duration, ktime_sub_ns(ktime_get_boottime(), now),
			atomic64_read(&zram->stats.swapin_count), bio,
			is_draw?'Y':'N', zw->index);

	src = kmap_atomic(src_page);
	dst = kmap_atomic(dst_page);
	memcpy(dst, src, PAGE_SIZE);
	kunmap_atomic(dst);
	kunmap_atomic(src);

	if (zw->bio_chain)
		bio_endio(zw->bio_chain);
	bio_put(bio);

	kfree(zw);
	__free_page(src_page);

	// USD: add for get hour stats by zihan.ju at 20240705 start
	try_to_add_to_table(&zram->wbd_stat.rd_hour, now, atomic64_read(&zram->stats.swapin_count) * 4);
	// USD: add for get hour stats by zihan.ju at 20240705 end

	// USD: add for calculate PSI time by zihan.ju at 20240522 start
	memfusion_psi_io(&zram->wbd_psi_data, duration, true);
	memfusion_psi_mem(&zram->wbd_psi_data, duration, true);
	// USD: add for calculate PSI time by zihan.ju at 20240522 end
}

#define is_fg_process() (!(current->flags & PF_KTHREAD) && !current->signal->oom_score_adj)

static void probe_android_set_task_comm_hook(void *unused, struct task_struct *tsk, bool exec)
{
	if ((tsk->flags & PF_KTHREAD) || (tsk->tgid != tsk->pid) || is_global_init(tsk))
		return;

	if (zram_key_task_list[KEY_SYSTEM_SERVER].tgid != tsk->tgid && !strncmp(tsk->comm, SYSTEM_SERVER, SYSTEM_SERVER_LEN)) {
		zram_key_task_list[KEY_SYSTEM_SERVER].tgid = tsk->tgid;
		zram_key_task_list[KEY_SYSTEM_SERVER].memcg = mem_cgroup_from_css(task_css(tsk, memory_cgrp_id));
	} else if (zram_key_task_list[KEY_SYSTEM_UI].tgid != tsk->tgid && !strncmp(tsk->comm, SYSTEM_UI, SYSTEM_UI_LEN)) {
		zram_key_task_list[KEY_SYSTEM_UI].tgid = tsk->tgid;
		zram_key_task_list[KEY_SYSTEM_UI].memcg = mem_cgroup_from_css(task_css(tsk, memory_cgrp_id));
	} else if (zram_key_task_list[KEY_SURFACEFLINGER].tgid != tsk->tgid && !strncmp(tsk->comm, MF22_SRUFACE, MF22_SRUFACE_LEN)) {
		zram_key_task_list[KEY_SURFACEFLINGER].tgid = tsk->tgid;
		zram_key_task_list[KEY_SURFACEFLINGER].memcg = mem_cgroup_from_css(task_css(tsk, memory_cgrp_id));
	} else if (zram_key_task_list[KEY_COMPOSE_X].tgid != tsk->tgid && !strncmp(tsk->comm, MF22_COMPOSER, MF22_COMPOSER_LEN)) {
		zram_key_task_list[KEY_COMPOSE_X].tgid = tsk->tgid;
		zram_key_task_list[KEY_COMPOSE_X].memcg = mem_cgroup_from_css(task_css(tsk, memory_cgrp_id));
	} else if (zram_key_task_list[KEY_LAUNCHER].tgid != tsk->tgid && (!strncmp(tsk->comm, SSIONLAUNCHER, SSIONLAUNCHER_LEN))) {
		zram_key_task_list[KEY_LAUNCHER].tgid = tsk->tgid;
		zram_key_task_list[KEY_LAUNCHER].memcg = mem_cgroup_from_css(task_css(tsk, memory_cgrp_id));
	} else if (zram_key_task_list[KEY_ZERO_SCREEN].tgid != tsk->tgid && !strncmp(tsk->comm, ZERO_SCREEN, ZERO_SCREEN_LEN)) {
		zram_key_task_list[KEY_ZERO_SCREEN].tgid = tsk->tgid;
		zram_key_task_list[KEY_ZERO_SCREEN].memcg = mem_cgroup_from_css(task_css(tsk, memory_cgrp_id));
	}
}

bool update_read_stats(struct zram* zram) {
	struct task_struct* task = current;
	int index;
	enum zram_tasktype type = 0;
	bool is_draw = false;

	/* calculate key task read page from backing device */
	for(index = 0; index < KEY_MAX_PROCESS; index++) {
		/* Please carefully modify the code here. */
		if (task->tgid ==  zram_key_task_list[index].tgid && zram_key_name[index] &&
			!strncmp(task->comm, zram_key_name[index], TASK_COMM_LEN)) {
			atomic64_inc(&zram->wbd_stat.key_process_reads);
			type |= ZRAM_KEY_TASK;
			break;
		}
	}

	/* calculate foreground process read page from backing device */
	if (is_fg_process()) {
		atomic64_inc(&zram->wbd_stat.fg_reads);
		if (task->pid == task->tgid) {
			atomic64_inc(&zram->wbd_stat.ui_reads);
			type |= ZRAM_UI_THREAD;
		}
		type |= ZRAM_FG_TASK;
	}

	/* calculate read page of draw thread from backing device */
	if (!strncmp(task->comm, MF22_SRUFACE, MF22_SRUFACE_LEN)) {
		atomic64_inc(&zram->wbd_stat.surface_reads);
		type |= ZRAM_SURFACE;
	} else if (!strncmp(task->comm, MF22_RENDER, MF22_RENDER_LEN)) {
		atomic64_inc(&zram->wbd_stat.render_reads);
		type |= ZRAM_RENDER;
	} else if (!strncmp(task->comm, MF22_BINDER, MF22_BINDER_LEN)) {
		atomic64_inc(&zram->wbd_stat.binder_reads);
		type |= ZRAM_BINDER;
	} else if (!strncmp(task->comm, MF22_COMPOSER, MF22_COMPOSER_LEN)) {
		atomic64_inc(&zram->wbd_stat.composer_reads);
		type |= ZRAM_COMPOSER;
	}

	if (type & (ZRAM_UI_THREAD| ZRAM_SURFACE|ZRAM_RENDER|ZRAM_BINDER|ZRAM_COMPOSER))
		is_draw = true;

	if (is_log_enable(ZRAM_READ_LOG))
		pr_err(MF22PRELOG"[%s] %hd %hd %d t:0x%x %c %c.%c.%c.%c.%c.%c.%c.",
			task->comm, task->pid, task->tgid, task->signal->oom_score_adj, type, is_draw?'Y':'N',
			(type & ZRAM_KEY_TASK) ? 'K' : '.',
			(type & ZRAM_FG_TASK) ? 'F' : '.',
			(type & ZRAM_UI_THREAD) ? 'U' : '.',
			(type & ZRAM_SURFACE) ? 'S' : '.',
			(type & ZRAM_RENDER) ? 'R' : '.',
			(type & ZRAM_BINDER) ? 'B' : '.',
			(type & ZRAM_COMPOSER) ? 'C' : '.');

	return is_draw;
}

void update_fwk_thrashing_stat(bool is_compress, u32 index) {
	u64 now;
	s64 *thrashing;

	if (!g_process_reclaim_enable)
		return;

	atomic64_inc(&fwk_thrasing_pages_delta);

	if (is_log_enable(ZRAM_CGROUP_LOG))
		pr_err("[%s] cprs %u\n", current->comm, index);

	if (!spin_trylock(&fwk_thrash_lock)) {
		return;
	}

	now = get_jiffies_64();
	thrashing = fwk_thrashing;

	// now > last_update_jiffies+3*HZ
	if (time_after64(now, next_update_jiffies+2*HZ)) {
		thrashing[0] = 0;
		thrashing[1] = 0;
		thrashing[2] = 0;
	}

	/* Due to timing issues, few pages statistics may be missed, but it can be ignored. */
	if (time_after64(now, next_update_jiffies)) {
		thrashing[2] = thrashing[1];
		thrashing[1] = thrashing[0];
		thrashing[0] = atomic64_read(&fwk_thrasing_pages_delta);
		atomic64_set(&fwk_thrasing_pages_delta, 0L);
		next_update_jiffies = now + HZ;
	}

	spin_unlock(&fwk_thrash_lock);
	return;
}

static void zram_dcprs_page_end_io(struct bio *bio)
{
	struct page *page = bio->bi_io_vec[0].bv_page;
	struct zram_wb_work *zw = (struct zram_wb_work *)page_private(page);
	int errno = blk_status_to_errno(bio->bi_status);

	if (errno) {
		pr_err(MF22PRELOG"read %u I/O error %d \n", zw->index, errno);
		if (zw->bio_chain)
			bio_endio(zw->bio_chain);
		bio_put(bio);
		if (zw->src_page)
			__free_page(zw->src_page);
		kfree(zw);
		return;
	}

	INIT_WORK(&zw->work, zram_custom_end_io);
	schedule_work(&zw->work);
}

#endif
//USD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 20240506 end
static void read_from_bdev_async(struct zram *zram, struct page *page,
			unsigned long entry, struct bio *parent)
{
	struct bio *bio;
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	int bd_index;
	unsigned int stay_time;
	bool is_draw;
	/*get zram->table[index]*/
	u32 index = (u32)entry;
	struct page* src_page;
	struct zram_wb_work *zw;
	ktime_t start;
	bool is_vipio = false;
	unsigned long handle;

	zram_slot_lock(zram, index);
	stay_time = zram->table[index].stay_time;
	/* warning: we use index instead of zram_get_element(zram, index) in read_frome_bdev */
	handle = zram_get_element(zram, index);
	bd_index = zram_get_bd_index(zram, index);
	zram_slot_unlock(zram, index);
	if (bd_index < 0) {
		pr_err(MF22PRELOG"wow, [%s] %hd %hd index:%u bd_index:%d handle:%lu p-bio:0x%px\n",
			current->comm, current->pid, current->tgid, index, bd_index, handle, parent);
		atomic64_inc(&zram_bd_index_times);
	}
	atomic64_inc(&zram->stats.bd_reads_list[bd_index]);

#if IS_ENABLED(CONFIG_TRANSSION_BLK)
	if (zram_vip_io_sched_enable) {
		parent->bi_opf |= (REQ_SYNC | REQ_FG);
		is_vipio = true;
	}
#endif

	bio = bio_alloc(zram->bdev_list[bd_index], 1, parent->bi_opf, GFP_NOIO);

	if (is_log_enable(ZRAM_READ_LOG))
		start = ktime_get();

	src_page = alloc_page(GFP_NOIO|__GFP_HIGHMEM);
	if (!src_page) {
		pr_info(MF22PRELOG"read cprs failed to alloc page\n");
		bio_put(bio);
		return;
	}

	zw = kzalloc(sizeof(struct zram_wb_work), GFP_NOIO);
	if (!zw) {
		pr_info(MF22PRELOG"read cprs failed to alloc zww\n");
		__free_page(src_page);
		bio_put(bio);
		return;
	}

	is_draw = update_read_stats(zram);
	zw->index = index;
	zw->start = ktime_get_boottime();
	zw->flags |= (is_draw?MEMFUSION_DRAW_FLAG:0);
	zw->src_page = src_page;
	zw->dst_page = page;
	zw->zram = zram;
	zw->bio = bio;
	set_page_private(src_page, (unsigned long)zw);

	bio->bi_end_io = zram_dcprs_page_end_io;
	bio->bi_iter.bi_sector = (handle>>AGGR_BLKIDX_SHIFT) * (PAGE_SIZE >> 9);
	__bio_add_page(bio, src_page, PAGE_SIZE, 0);
	if (parent) {
		zw->bio_chain = bio_alloc(zram->bdev_list[bd_index], 1, parent->bi_opf, GFP_NOIO);
		if (!zw->bio_chain) {
			kfree(zw);
			__free_page(src_page);
			bio_put(bio);
			return;
		}
		bio_chain(zw->bio_chain, parent);
	}
	submit_bio(bio);

	if (is_log_enable(ZRAM_READ_LOG))
		pr_err(MF22PRELOG"[%s] read child:0x%px parent:0x%px index:%u %lld us %c stay:%u s vip-io[%c]\n",
			current->comm, bio, parent, index, ktime_us_delta(ktime_get(), start),
			is_draw?'Y':'N', stay_time, is_vipio?'Y':'N');
#else
	bio = bio_alloc(zram->bdev, 1, parent->bi_opf, GFP_NOIO);
	bio->bi_iter.bi_sector = entry * (PAGE_SIZE >> 9);
	__bio_add_page(bio, page, PAGE_SIZE, 0);

	bio_chain(bio, parent);
	submit_bio(bio);
#endif
}

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
static int zram_reset_segload_bdev(struct zram *zram)
{
	int i;
	struct file *backing_dev = NULL;
	unsigned long *bitmap = NULL;
	struct block_device *bdev = NULL;
	unsigned long *chunk_bitmap;
	u8 *wb_table;

	zram_kernel_trace_start(ZRAM_RESET_NAME);
	pr_err(MF22PRELOG"[reset segload bdev start \n");

	zram->wbd_running = false;
	if(zram->wbd != NULL)
		kthread_stop(zram->wbd);

	while (g_backing_dev_avail_count > 0) {
		g_backing_dev_avail_count -= 1;
		i = g_backing_dev_avail_count + 1;
		backing_dev = zram->backing_dev_list[i];
		bitmap = zram->bitmap_list[i];
		chunk_bitmap = zram->chunk_bitmap_list[i];
		spin_lock(&zram->wb_table_lock);
		wb_table = zram->wb_table[i];
		zram->wb_table[i] = NULL;
		spin_unlock(&zram->wb_table_lock);
		bdev = zram->bdev_list[i];
		if (bdev)
			blkdev_put(bdev, zram);
		if (backing_dev)
			filp_close(backing_dev, NULL);

		kvfree(bitmap);
		kvfree(chunk_bitmap);
		kvfree(wb_table);
		zram->backing_dev_list[i] = NULL;
		zram->bitmap_list[i] = NULL;
		zram->chunk_bitmap_list[i] = NULL;
		zram->bdev_list[i] = NULL;
		pr_err(MF22PRELOG"[zram_reset_segload_bdev]reset %d bdev end! \n", i);
	}
	zram_kernel_trace_end(ZRAM_RESET_NAME);

	return 0;
}

static atomic_t is_app_launch = ATOMIC_INIT(0);
static int zram_balance_threshold = 26;	/* min swap-used threshold */

/*Record the number of times the maximum write limit was exceeded*/
static unsigned long long klist_wb_over_cnt = 0;
static unsigned long long klist_less_time_cnt = 0;

module_param(zram_balance_threshold, int, 0644);

static int zram_balance_ratio = 25;	/* nand writeback ratio */
module_param(zram_balance_ratio, int, 0644);

/*
* limit the max pages move to backing device of kernel reclaim
* 76800 pages = 300MB
* 89600 pages = 350MB
* 102400 pages = 400MB
* 204800 pages = 800MB
*/
#define ZRAM_NR_PAGE_WB_LIMIT 204800
#define ZRAM_STAY_MIN_TIME 86400
/* RAM > 4G default is 800M, RAM <= 4G default is 350M */
int zram_kernel_wb_count_limit = 89600;
module_param(zram_kernel_wb_count_limit, int, 0644);

/*unit seconds, default is 24h*/
int zram_kernel_wb_actime_limit = ZRAM_STAY_MIN_TIME;
module_param(zram_kernel_wb_actime_limit, int, 0644);

/* cold pages time threshold*/
static int zram_stats_actime = ZRAM_STAY_MIN_TIME;
module_param(zram_stats_actime, int, 0644);

static int zram_wb_multi = 5;
module_param(zram_wb_multi, int, 0644);

static u32 entry_to_index(struct zram *zram, struct zram_table_entry *entry)
{
	return (u32)(((unsigned long)entry - (unsigned long)zram->table) /
			sizeof(struct zram_table_entry));
}

static int last_write_cnt = 0;
static int actual_write_cnt = 0;
static unsigned long long insuff_time_trigger_cnt = 0;

//#define ZRAM_WBD_INTERVAL 5 * HZ
static int zram_wbd_interval = 2;
static unsigned long long max_pages_cnt = 0;
static unsigned long long ratio_cnt = 0;
static unsigned long long total_trigger_cnt = 0;
static unsigned long long total_write_cnt = 0;
static unsigned long long continue_write_cnt = 0;
static unsigned long long wakeup_wbd_cnt = 0;
static unsigned long long app_launch = 0;
bool zram_should_writeback(struct zram *zram, int pages, bool trigger)
{
	unsigned long total = zram->disksize >> PAGE_SHIFT;
	unsigned long stored = atomic64_read(&zram->stats.pages_stored);
	unsigned long min_stored = total * zram_balance_threshold / 100;
	unsigned long writtenback = atomic64_read(&zram->stats.bd_count);
	int writtenback_ratio = stored ? (writtenback * 100) / stored : 0;
	int min_writtenback_ratio = zram_balance_ratio;
	int margin = max_t(int, 1, zram_balance_ratio / 10);
	int writeback_max;
	bool ret = true;

	if (trigger)
		total_trigger_cnt++;
	else
		total_write_cnt++;

	if (writtenback > zram_kernel_wb_count_limit)
	{
		klist_wb_over_cnt++;
		return false;
	}

	if (!zram_writeback_vmpressure && !atomic_read(&zram_wbd_switch)) {
		screen_on_break_cnt++;
		return false;
	}

	if (atomic_read(&high_loading_cooling)) {
		high_loading_cooling_cnt++;
		return false;
	}

	if (atomic_read(&is_app_launch)) {
		app_launch++;
		return false;
	}

	if ((zram_write_limit_debug < DEFAULT_ZRAM_WRITE_LIMIT) && !time_is_after_jiffies64(zram_write_limit_time_stamp)) {
		zram_write_limit_debug = DEFAULT_ZRAM_WRITE_LIMIT;
	}

	/* stop thread when writtenback enough */
	writeback_max = (zram_kwrite_boost>0)?zram_kwrite_boost:zram_write_limit_debug;
	if (pages > writeback_max) {
		max_pages_cnt++;
		return false;
	}

	if (trigger)
		min_writtenback_ratio -= margin;
	else
		min_writtenback_ratio += margin;

	//zram used > 50% && backing_dev_used / zram_used(include wb) < 25% (could increase)
	if (min_stored > stored || min_writtenback_ratio < writtenback_ratio) {
		ratio_cnt++;
		ret = false;
	}

	if (!get_loadavg()) {
		loadavg_cnt++;
		return false;
	}

	if(zram_cpu_io_enable && !get_cpu_io_usage()) {
		cpu_io_skip_cnt++;
		return false;
	}

	/*zram_wbd calls*/
	if (!trigger && ret)
		continue_write_cnt++;

	/*__zram_bvec_write calls*/
	if (trigger && ret)
		wakeup_wbd_cnt++;

	return ret;
}

void try_wakeup_zram_wbd_lite(struct zram *zram, int multi)
{
	struct timespec64 ts;
	ktime_t now;

	if (!zram)
		return;

	/* proctect contention*/
	if (!spin_trylock(&zram->wakeup_wbd_lock))
		return;

	/* zram0_wbd should be running after startup X seconds. */
	now = ktime_get_boottime();
	ts = ktime_to_timespec64(now);
	if (ts.tv_sec < zram_kernel_wb_actime_limit) {
		insuff_time_trigger_cnt++;
		goto OUT;
	}

	if (zram->backing_dev && !zram->wbd_running && zram_should_writeback(zram, 0, true)) {
		if (ZRAM_WB_ALL == multi) {
			zram_kwrite_boost = zram_kernel_wb_count_limit;
		} else {
			zram_kwrite_boost = zram_write_limit_debug * multi;
		}

		zram->wbd_running = true;
		wake_up(&zram->wbd_wait);
	}

OUT:
	spin_unlock(&zram->wakeup_wbd_lock);
	return;

}

/*ret > 0: it means get memcg patch success. */
static int zram_get_memcg_path(struct page* page, char* path, int len) {
	struct mem_cgroup *memcg;
	struct cgroup *cgrp;
	int ret = -1;
	struct folio *folio = page_folio(page);
	/* get_mem_cgroup_from_page ? */
	memcg = folio_memcg(folio);
	if (!memcg) {
		return ret;
	}

	cgrp = memcg->css.cgroup;
	if (!cgrp) {
		return ret;
	}

	ret = kernfs_path_from_node(cgrp->kn, NULL, path, len);

	return ret;
}

/* return true: indicates that this page belongs to a critical process. */
static bool page_is_key(struct zram *zram, struct page *page, u32 index) {
	char path[MEM_CGROUP_NAME_MAX_LEN] = {0};
	struct mem_cgroup *memcg;
	struct folio *folio = page_folio(page);
	int i, len = 0;
	if (is_log_enable(ZRAM_CGROUP_LOG)) {
		len = zram_get_memcg_path(page, path, MEM_CGROUP_NAME_MAX_LEN);
		pr_err("[%s] index: %u path:[%s]\n", current->comm, index, (len>0)?path:"err");
	}

	memcg = folio_memcg(folio);
	if (mem_cgroup_is_root(memcg)) {
		return false;
	}

	for (i = 0; i < KEY_MAX_PROCESS; i++) {
		if (memcg == zram_key_task_list[i].memcg) {
			zram_slot_lock(zram, index);
			zram_set_flag(zram, index, ZRAM_KEY);
			zram_slot_unlock(zram, index);
			atomic64_inc(&zram->stats.key_pages);
			return true;
		}
	}

	return false;
}

static int start_zram_writeback(struct zram *zram, u32 index, struct page *page, unsigned int stay_time) {
	struct bio bio;
	struct bio_vec bio_vec;
	ssize_t ret = 0;
	int err;
	unsigned long blk_idx = 0;
	ktime_t current_ts;
	unsigned long io_cost_time = 0;
	int bd_index = -1;

	down_read(&zram->init_lock);

	if (!zram->backing_dev) {
		ret = -ENODEV;
		goto release_init_lock;
	}

	bd_index = atomic_read(&zram->backing_dev_index);
	if (!blk_idx) {
		blk_idx = alloc_block_bdev_list(zram,bd_index);
		if (!blk_idx) {
			ret = -ENOSPC;
			goto no_bk;
		}
	}

	zram_slot_lock(zram, index);
	if (!zram_allocated(zram, index))
		goto out;

	if (zram_test_flag(zram, index, ZRAM_WB) ||
			zram_test_flag(zram, index, ZRAM_SAME) ||
			zram_test_flag(zram, index, ZRAM_UNDER_WB))
		goto out;

	/*
		* Clearing ZRAM_UNDER_WB is duty of caller.
		* IOW, zram_free_page never clear it.
		*/
	zram_set_flag(zram, index, ZRAM_UNDER_WB);
	/* Need for hugepage writeback racing */
	zram_set_flag(zram, index, ZRAM_IDLE);
	zram_slot_unlock(zram, index);
	if (zram_read_page(zram, page, index, NULL)) {
		zram_slot_lock(zram, index);
		zram_clear_flag(zram, index, ZRAM_UNDER_WB);
		zram_clear_flag(zram, index, ZRAM_IDLE);
		zram_slot_unlock(zram, index);
		goto no_bk;
	}

	bio_init(&bio, zram->bdev_list[bd_index], &bio_vec, 1,
	     REQ_OP_WRITE | REQ_SYNC);

	bio.bi_iter.bi_sector = blk_idx * (PAGE_SIZE >> 9);
	__bio_add_page(&bio, page, PAGE_SIZE, 0);

	current_ts = ktime_get_boottime();
	zram_kernel_trace_start(ZRAM_WBD_WRITE_IO);
	/*
		* XXX: A single page IO would be inefficient for write
		* but it would be not bad as starter.
		*/
	err = submit_bio_wait(&bio);
	zram_kernel_trace_end(ZRAM_WBD_WRITE_IO);
	io_cost_time = ktime_us_delta(ktime_get_boottime(), current_ts);
	memfusion_psi_io(&zram->wbd_psi_data, io_cost_time, true);

	if (err) {
		zram_slot_lock(zram, index);
		zram_clear_flag(zram, index, ZRAM_UNDER_WB);
		zram_clear_flag(zram, index, ZRAM_IDLE);
		zram_slot_unlock(zram, index);
		/*
			* Return last IO error unless every IO were
			* not suceeded.
			*/
		ret = err;
		goto no_bk;
	}

	atomic64_inc(&zram->stats.bd_writes_list[bd_index]);
	atomic64_inc(&zram->stats.bd_writes);

	/*
		* We released zram_slot_lock so need to check if the slot was
		* changed. If there is freeing for the slot, we can catch it
		* easily by zram_allocated.
		* A subtle case is the slot is freed/reallocated/marked as
		* ZRAM_IDLE again. To close the race, idle_store doesn't
		* mark ZRAM_IDLE once it found the slot was ZRAM_UNDER_WB.
		* Thus, we could close the race by checking ZRAM_IDLE bit.
		*/
	zram_slot_lock(zram, index);
	if (!zram_allocated(zram, index) ||
			!zram_test_flag(zram, index, ZRAM_IDLE)) {
		zram_clear_flag(zram, index, ZRAM_UNDER_WB);
		zram_clear_flag(zram, index, ZRAM_IDLE);
		goto out;
	}

	/*USD: we released memory of entry when the page have been moved to backing device.*/
	zram_free_page(zram, index);
	zram_clear_flag(zram, index, ZRAM_UNDER_WB);
	zram_set_flag(zram, index, ZRAM_WB);
	/* only zram_wbd can write page to ROM */
	zram_set_flag(zram, index, ZRAM_COLD);
	atomic64_inc(&zram->stats.bd_ker_reclaim);
	zram->table[index].ac_time = ktime_get_boottime();
	zram->table[index].stay_time = stay_time;
	/*USD: we set blk_idx to zram->table[index].element in order to find page in backing device in future.*/
	zram_set_bd_index(zram,index,bd_index);
	blk_idx <<= AGGR_BLKIDX_SHIFT;
	zram_set_element(zram, index, blk_idx);
	blk_idx = 0;
	atomic64_inc(&zram->stats.pages_stored);
	bdev_write_page_time += io_cost_time;
	bdev_write_page_count++;
out:
	zram_slot_unlock(zram, index);

no_bk:
	if (blk_idx)
		free_block_bdev_list(zram, blk_idx, bd_index);

release_init_lock:
	up_read(&zram->init_lock);

	return ret;
}

#define PSI_FSHIFT		11		/* nr of bits of precision */
#define PSI_FIXED_1		(1<<PSI_FSHIFT)	/* 1.00 as fixed-point */
#define PSI_UPDATE_INTERVAL (2 * HZ - 1)	/* 1 sec intervals to update psi*/
#define PSI_EXP	    (2 * HZ + 1)	/* 2 sec intervals (only for jiffies) */
#define EXP_10s		1677 		/* 1/exp(2s/10s) as fixed-point */
#define EXP_60s		1981 		/* 1/exp(2s/60s) as fixed-point  */
#define EXP_300s	2034 		/* 1/exp(2s/300s) as fixed-point */
#define PSI_LOAD_INT(x) ((x) >> PSI_FSHIFT)
#define PSI_LOAD_FRAC(x) PSI_LOAD_INT(((x) & (PSI_FIXED_1-1)) * 100)

// Use to calculate the power of a fixed-point number
static unsigned long fixed_power_int(unsigned long x, unsigned int frac_bits, unsigned int n)
{
	unsigned long result = 1UL << frac_bits;

	if (n) {
		for (;;) {
			if (n & 1) {
				result *= x;
				result += 1UL << (frac_bits - 1);
				result >>= frac_bits;
			}
			n >>= 1;
			if (!n)
				break;
			x *= x;
			x += 1UL << (frac_bits - 1);
			x >>= frac_bits;
		}
	}

	return result;
}

unsigned long calc_load_n(unsigned long load, unsigned long exp,
	    unsigned long active, unsigned int n)
{
	return calc_load(load, fixed_power_int(exp, PSI_FSHIFT, n), active);
}

// Use to calculate the average of the last 10s
void psi_calc_avgs(unsigned long* result, unsigned long get_time, unsigned long period, int missed_periods)
{
	unsigned long pct;
	if(missed_periods)
	{
		*result = calc_load_n(*result, EXP_10s, 0, missed_periods);
	}
	pct = div64_u64(get_time * 100, period * NR_CPUS);
	pct *= PSI_FIXED_1;
	*result = calc_load(*result, EXP_10s, pct);
}

// Use to init psi data
void init_psi_task(struct psi_data* wbd_psi_data)
{
	int i = 0;
	int now = jiffies;
	spin_lock_init(&wbd_psi_data->psi_update_lock);
	atomic64_set(&wbd_psi_data->psi_cpu_times, 0);
	atomic64_set(&wbd_psi_data->psi_io_times, 0);
	atomic64_set(&wbd_psi_data->psi_mem_times, 0);
	wbd_psi_data->psi_cpu_collect_time = 0;
	atomic64_set(&wbd_psi_data->psi_io_collect_time, 0);
	atomic64_set(&wbd_psi_data->psi_mem_collect_time, 0);
	for(i = 0;i < NR_ZRAM_PSI_RESOURCES;i++)
	{
		wbd_psi_data->psi_last_total[i] = 0;
		wbd_psi_data->psi_avg[i] = 0;
	}
	wbd_psi_data->psi_update_last_time = now;
	wbd_psi_data->psi_update_next_time = now;
	wbd_psi_data->psi_update_time = now;
}

// Use to get string name of psi type
void psi_update_name(char* name, enum psi_res wbd_psi_type)
{
	switch(wbd_psi_type)
	{
		case PSI_CPU:
			strncpy(name, "cpu", 3);
			break;
		case PSI_IO:
			strncpy(name, "io", 2);
			break;
		case PSI_MEM:
			strncpy(name, "mem", 3);
			break;
		default:
			strncpy(name, "err", 3);
			break;
	}
}

void psi_show(struct psi_data* wbd_psi_data)
{
	// show psi_avg_data
	int i = 0;
	char name[4];
	pr_err("[Memfusion2_2]Memfusion PSI data show start\n");
	pr_err("[Memfusion2_2] data_avg: 10s\n");
	for(i = 0;i < NR_ZRAM_PSI_RESOURCES;i++)
	{
		psi_update_name(name, i);
		pr_err("[Memfusion2_2] %s_avg: %lu.%02lu\n", name,  PSI_LOAD_INT(wbd_psi_data->psi_avg[i]), PSI_LOAD_FRAC(wbd_psi_data->psi_avg[i]));
	}
	pr_err("[Memfusion2_2]Max Data:\n");
	for(i = 0;i < NR_ZRAM_PSI_RESOURCES;i++)
	{
		psi_update_name(name, i);
		pr_err("[Memfusion2_2] %s_max_10: %lu.%02lu\n", name, PSI_LOAD_INT(wbd_psi_data->psi_max[i]), PSI_LOAD_FRAC(wbd_psi_data->psi_max[i]));
	}
	pr_err("[Memfusion2_2]Total Data:\n");
	for(i = 0;i < NR_ZRAM_PSI_RESOURCES;i++)
	{
		psi_update_name(name, i);
		pr_err("[Memfusion2_2] %s_total: %lu\n", name, wbd_psi_data->psi_last_total[i]);
	}
	pr_err("[Memfusion2_2]Memfusion PSI data show end\n");
}

void psi_show_file(struct seq_file *m, struct psi_data* wbd_psi_data)
{
	// show psi_avg_data
	int i = 0;
	char name[4];
	seq_printf(m, "[Memfusion2_2]Memfusion PSI data show start\n");
	seq_printf(m, "[Memfusion2_2] data_avg: 10s  60s  300s\n");
	for(i = 0;i < NR_ZRAM_PSI_RESOURCES;i++)
	{
		psi_update_name(name, i);
		seq_printf(m, "[Memfusion2_2] %s_avg: %lu.%02lu \n", name,  PSI_LOAD_INT(wbd_psi_data->psi_avg[i]), PSI_LOAD_FRAC(wbd_psi_data->psi_avg[i]));
	}
	seq_printf(m, "[Memfusion2_2]Max Data:\n");
	for(i = 0;i < NR_ZRAM_PSI_RESOURCES;i++)
	{
		psi_update_name(name, i);
		seq_printf(m, "[Memfusion2_2] %s_max_10: %lu.%02lu {%lu}\n", name, PSI_LOAD_INT(wbd_psi_data->psi_max[i]), PSI_LOAD_FRAC(wbd_psi_data->psi_max[i]), PSI_LOAD_INT(wbd_psi_data->psi_max[i]) * 100 + PSI_LOAD_FRAC(wbd_psi_data->psi_max[i]));
	}
	seq_printf(m, "[Memfusion2_2]Total Data:\n");
	for(i = 0;i < NR_ZRAM_PSI_RESOURCES;i++)
	{
		psi_update_name(name, i);
		seq_printf(m, "[Memfusion2_2] %s_total: %lu\n", name, wbd_psi_data->psi_last_total[i]);
	}
	seq_printf(m, "[Memfusion2_2]Memfusion PSI data show end\n");
}

// Used to update Target PSI Value
static int psi_update_average(struct psi_data* wbd_psi_data)
{
	int i = 0;
	// All for jiffies(fast!!)
	unsigned long expires = 0;
	unsigned long missed_periods = 0;
	unsigned long period = 0;
	unsigned long now = 0;
	unsigned long time[NR_ZRAM_PSI_RESOURCES] = {0};
	unsigned long psi_time = jiffies;

	if(!spin_trylock(&wbd_psi_data->psi_update_lock))
	{
		return 0;
	}

	// Update Next try update PSI average time
	wbd_psi_data->psi_update_time = psi_time + PSI_UPDATE_INTERVAL;

	//get time from atomic64_t data(cpu/io/mem)
	time[PSI_CPU] = atomic64_read(&wbd_psi_data->psi_cpu_times);
	time[PSI_IO] = atomic64_read(&wbd_psi_data->psi_io_times);
	time[PSI_MEM] = atomic64_read(&wbd_psi_data->psi_mem_times);

	atomic64_set(&wbd_psi_data->psi_cpu_times, 0);
	atomic64_set(&wbd_psi_data->psi_io_times, 0);
	atomic64_set(&wbd_psi_data->psi_mem_times, 0);

	// Use jiffies to get last time and now time
	expires = wbd_psi_data->psi_update_next_time;
	now = jiffies;
	if((now - expires) > PSI_EXP)
	{
		missed_periods = div_u64(now - expires, PSI_EXP);
	}

	// Use us to calculate the period
	period = jiffies_to_nsecs(now - (wbd_psi_data->psi_update_last_time + missed_periods * PSI_EXP));

  	// Update psi time date[CPU/IO/MEM]
	wbd_psi_data->psi_update_last_time = now;
	wbd_psi_data->psi_update_next_time = expires + (missed_periods + 1) * PSI_EXP;

	// Check if period if 0, return false
	if(period == 0)
	{
		spin_unlock(&wbd_psi_data->psi_update_lock);
		return 0;
	}

	for(i = 0; i < NR_ZRAM_PSI_RESOURCES; i++)
	{
		// if psi time is 0, no need update now, return false
		if((long)time[i] <= 0)
			continue;

		// Add psi time to total data
		wbd_psi_data->psi_last_total[i] += time[i];

		if(time[i] > period)
		{
			time[i] = period;
		}

		// Use us to calculate psi avgs
		psi_calc_avgs(&wbd_psi_data->psi_avg[i], time[i], period, missed_periods);

		// update psi_max
		if(wbd_psi_data->psi_avg[i] > wbd_psi_data->psi_max[i])
			wbd_psi_data->psi_max[i] = wbd_psi_data->psi_avg[i];
	}

	spin_unlock(&wbd_psi_data->psi_update_lock);
	return 1;
}

static bool memfusion_psi_update(struct psi_data *wbd_psi_data)
{
	// if psi update time is not arrived, return false
	if(time_is_after_jiffies(wbd_psi_data->psi_update_time))
		return false;

	// update psi data
	if(!psi_update_average(wbd_psi_data))
		return false;

	return true;
}

void memfusion_psi_cpu(struct psi_data *wbd_psi_data, unsigned long cpu_data, bool update_flag)
{
	unsigned long psi_time = jiffies;

	// Jump this time if update time less then 1 tick
	if(wbd_psi_data->psi_cpu_collect_time == psi_time)
		return;

	atomic64_set(&wbd_psi_data->psi_cpu_times, cpu_data - wbd_psi_data->psi_last_total[PSI_CPU]);
	wbd_psi_data->psi_cpu_collect_time = psi_time;

	// try to update psi_cpu data
	if(update_flag)
		memfusion_psi_update(wbd_psi_data);
}

void memfusion_psi_io(struct psi_data *wbd_psi_data, unsigned long add_data, bool update_flag)
{
	unsigned long psi_time = jiffies;
	unsigned long psi_timeout = 0;

	psi_timeout = psi_time - atomic64_read(&wbd_psi_data->psi_io_collect_time);
	// add_data is for us
	if(add_data * 1000  < jiffies_to_nsecs(psi_timeout)) 
		atomic64_add(add_data*1000, &wbd_psi_data->psi_io_times);
	else
		atomic64_add(jiffies_to_nsecs(psi_timeout), &wbd_psi_data->psi_io_times);
	atomic64_set(&wbd_psi_data->psi_io_collect_time, psi_time);

	// try to update psi_io data
	if(update_flag)
		memfusion_psi_update(wbd_psi_data);
}

void memfusion_psi_mem(struct psi_data *wbd_psi_data, unsigned long add_data, bool update_flag)
{
	unsigned long psi_time = jiffies;
	unsigned long psi_timeout = 0;

	psi_timeout = psi_time - atomic64_read(&wbd_psi_data->psi_mem_collect_time);
	// add_data is for us
	if(add_data * 1000  < jiffies_to_nsecs(psi_timeout)) 
		atomic64_add(add_data*1000, &wbd_psi_data->psi_mem_times);
	else
		atomic64_add(jiffies_to_nsecs(psi_timeout), &wbd_psi_data->psi_mem_times);
	atomic64_set(&wbd_psi_data->psi_mem_collect_time, psi_time);

	// try to update psi_mem data
	if(update_flag)
		memfusion_psi_update(wbd_psi_data);
}

void update_bdev_index(struct zram* zram) {
	int i, candidate_bdev_idx;

	down_read(&zram->init_lock);
	candidate_bdev_idx = g_backing_dev_avail_count;
	for ( i = 0; i <= candidate_bdev_idx; i++) {
		// 90%
		if (((u64)atomic64_read(&zram->stats.bd_count_list[i]) * 10) > (zram->nr_pages_list[i] * 9)) {
			if (((u64)atomic64_read(&zram->stats.bd_count_list[i]) * 100) < (zram->nr_pages_list[i] * 99))
				if(i < candidate_bdev_idx){
					candidate_bdev_idx = i;
					break;
				}
		} else {
			candidate_bdev_idx = i;
			break;
		}
	}
	atomic_set(&zram->backing_dev_index, candidate_bdev_idx);
	up_read(&zram->init_lock);
}

static int zram_fill_cprs_page(struct zram *zram, u32 index,
				struct zwbs **zwbs, int idx, u32 *prio)
{
	struct zram_wb_header *zhdr;
	struct page *page = zwbs[idx]->page;
	int offset = zwbs[idx]->off;
	unsigned long handle;
	void *src, *dst;
	int size;
	int header_sz = 0;

	zram_slot_lock(zram, index);
	if (!zram_allocated(zram, index) ||
			!zram_test_flag(zram, index, ZRAM_IDLE) ||
			zram_test_flag(zram, index, ZRAM_WB) ||
			zram_test_flag(zram, index, ZRAM_SAME) ||
			zram_test_flag(zram, index, ZRAM_UNDER_WB)) {
		zram_slot_unlock(zram, index);
		return 0;
	}

	size = zram_get_obj_size(zram, index);
	if (size != PAGE_SIZE)
		header_sz = sizeof(struct zram_wb_header);

	if (offset + header_sz + size > PAGE_SIZE) {
		zram_slot_unlock(zram, index);
		return -ENOSPC;
	}

	/* record compression algorithm priority */
	*prio = zram_get_priority(zram, index);

	/*
	 * Clearing ZRAM_UNDER_WB is duty of caller.
	 * IOW, zram_free_page never clear it.
	 */
	zram_set_flag(zram, index, ZRAM_UNDER_WB);
	/* Need for hugepage writeback racing */
	zram_set_flag(zram, index, ZRAM_IDLE);

	handle = zram_get_element(zram, index);
	if (!handle) {
		zram_clear_flag(zram, index, ZRAM_UNDER_WB);
		zram_clear_flag(zram, index, ZRAM_IDLE);
		zram_slot_unlock(zram, index);
		return -ENOENT;
	}
	src = zs_map_object(zram->mem_pool, handle, ZS_MM_RO);
	dst = kmap_atomic(page);
	if (header_sz) {
		zhdr = (struct zram_wb_header *)(dst + offset);
		zhdr->index = index;
		zhdr->size = size;
		dst = (u8 *)(zhdr + 1);
	}
	memcpy(dst, src, size);
	kunmap_atomic(dst);
	zs_unmap_object(zram->mem_pool, handle);
	zram_slot_unlock(zram, index);

	return size;
}

static void zram_writeback_clear_flags(struct zram *zram, struct zwbs *zwbs)
{
	struct zram_wb_entry *entry = zwbs->entry;
	unsigned int count = zwbs->cnt;
	unsigned long index;
	int i;
	bool is_allocated;

	for (i = 0; i < count; i++) {
		index = entry[i].index;
		is_allocated = false;
		zram_slot_lock(zram, index);
		if (zram_allocated(zram, index)) {
			zram_clear_flag(zram, index, ZRAM_UNDER_WB);
			zram_clear_flag(zram, index, ZRAM_IDLE);
			is_allocated = true;
		}
		zram_slot_unlock(zram, index);

		if (is_allocated)
			zram_list_add_to_tail(&zram->table[index], &zram->wbd_klist, zram);
	}
}

static void zram_writeback_aggr_done(struct zram *zram,
		struct zwbs *zwbs, int bd_index, unsigned long blk_idx)
{
	unsigned long index;
	unsigned int offset;
	unsigned int size;
	unsigned int count = zwbs->cnt;
	struct zram_wb_entry *entry = zwbs->entry;
	int i;
	u32 prio, stay_time;

	spin_lock(&zram->wb_table_lock);
	if (!zram->wb_table[bd_index]) {
		spin_unlock(&zram->wb_table_lock);
		return;
	}
	zram->wb_table[bd_index][blk_idx] = count;
	spin_unlock(&zram->wb_table_lock);
	atomic64_add(count, &zram->stats.bd_objcnt);

	for (i = 0; i < count && i < NR_ZWBS; i++) {
		index = entry[i].index;
		offset = entry[i].offset;
		size = entry[i].size;
		prio = entry[i].prio;
		stay_time = entry[i].stay_time;
		/*
		 * We released zram_slot_lock so need to check if the slot was
		 * changed. If there is freeing for the slot, we can catch it
		 * easily by zram_allocated.
		 * A subtle case is the slot is freed/reallocated/marked as
		 * ZRAM_IDLE again. To close the race, idle_store doesn't
		 * mark ZRAM_IDLE once it found the slot was ZRAM_UNDER_WB.
		 * Thus, we could close the race by checking ZRAM_IDLE bit.
		 */
		zram_slot_lock(zram, index);
		if (!zram_allocated(zram, index) ||
				!zram_test_flag(zram, index, ZRAM_IDLE)) {
			zram_clear_flag(zram, index, ZRAM_UNDER_WB);
			zram_clear_flag(zram, index, ZRAM_IDLE);
			free_block_bdev_list(zram, blk_idx, bd_index);
			zram_slot_unlock(zram, index);
			continue;
		}

		zram_free_page(zram, index);
		zram_clear_flag(zram, index, ZRAM_UNDER_WB);
		zram_set_flag(zram, index, ZRAM_WB);
		zram_set_flag(zram, index, ZRAM_COLD);
		if (size > 0 && size <= PAGE_SIZE)
			atomic64_add(size, &zram->stats.bd_size);
		else
			atomic64_add(size, &zram->stats.bd_size_inc_except);

		/* record element as "blk_idx|algo_prio|offset|size" */
		if (size == PAGE_SIZE)
			size = 0;
		zram_set_element(zram, index,
				(blk_idx << AGGR_BLKIDX_SHIFT) | (prio << AGGR_ALGO_SHIFT) |
				(offset << AGGR_OFFSET_SHIFT) | size);
		atomic64_inc(&zram->stats.bd_ker_reclaim);
		zram->table[index].stay_time = entry[i].stay_time;
		zram_set_bd_index(zram, index, bd_index);
		zram_slot_unlock(zram, index);
		atomic64_inc(&zram->stats.pages_stored);
	}
}

static void zram_update_max_stats(struct zram *zram)
{
	unsigned long bd_count, bd_size;

	bd_count = atomic64_read(&zram->stats.bd_count);
	if (bd_count <= atomic64_read(&zram->stats.bd_max_count))
		return;

	bd_size = atomic64_read(&zram->stats.bd_size);

	atomic64_set(&zram->stats.bd_max_count, bd_count);
	atomic64_set(&zram->stats.bd_max_size, bd_size);
}

static unsigned long try_alloc_block_bdev(struct zram *zram, int *nr_pages, int bd_index)
{
	unsigned long blk_idx;

	/* found free chunk, return blk_idx */
	if (NR_ZWBS == *nr_pages) {
		blk_idx = alloc_chunk_bdev_list(zram, bd_index);
		if (blk_idx)
			return blk_idx;
	}
	/* block index fragmentation*/
	*nr_pages = 1;
	return alloc_block_bdev_list(zram, bd_index);
}

static int zram_start_writeback_aggr(struct zram *zram, struct zwbs **zwbs,
		int nr_to_write, int objcnt)
{
	struct bio bio;
	struct bio_vec bio_vec[NR_ZWBS];
	unsigned long blk_idx;
	int ret = 0;
	int i, bd_index, idx = 0;
	ktime_t current_ts;
	unsigned long io_cost_time = 0;
	int nr_pages = nr_to_write;

	if (nr_pages > NR_ZWBS) {
		ret = -EINVAL;
		goto out;
	}

	bd_index = atomic_read(&zram->backing_dev_index);

retry:
	blk_idx = try_alloc_block_bdev(zram, &nr_pages, bd_index);
	if (!blk_idx) {
		ret = -ENOSPC;
		goto out;
	}

	bio_init(&bio, zram->bdev_list[bd_index], bio_vec, nr_pages,
#if IS_ENABLED(CONFIG_TRANSSION_BLK)
		REQ_OP_WRITE | REQ_SYNC | REQ_FG);
#else
		REQ_OP_WRITE | REQ_SYNC);
#endif

	bio.bi_iter.bi_sector = blk_idx * (PAGE_SIZE >> 9);
	for (i = 0; i < nr_pages; i++)
		__bio_add_page(&bio, zwbs[idx + i]->page, PAGE_SIZE, 0);

	current_ts = ktime_get_boottime();

	zram_aggr_trace_start(objcnt, ZRAM_WBD_AGGR_WRITE);
	ret = submit_bio_wait(&bio);
	zram_aggr_trace_end(objcnt, ZRAM_WBD_AGGR_WRITE);
	io_cost_time = ktime_us_delta(ktime_get_boottime(), current_ts);
	if (!ret) {
		for (i = 0; i < nr_pages; i++) {
			zram_writeback_aggr_done(zram, zwbs[idx + i], bd_index, blk_idx + i);
		}
		bdev_write_page_count += nr_pages;
		bdev_write_page_time += io_cost_time;

		zram_update_max_stats(zram);
		atomic64_add(nr_pages, &zram->stats.bd_writes_list[bd_index]);
		atomic64_add(nr_pages, &zram->stats.bd_writes);

		memfusion_psi_io(&zram->wbd_psi_data, io_cost_time, true);

		idx += nr_pages;
		if (idx < nr_to_write && idx < NR_ZWBS)
			goto retry;
	} else {
		if (blk_idx)
			for (i = 0; i < nr_pages; i++)
				free_block_bdev_list(zram, blk_idx + i, bd_index);
		for (i = idx; i < nr_pages; i++)
			zram_writeback_clear_flags(zram, zwbs[i]);
	}
out:
	return ret;
}

static void mark_end_of_page(struct zwbs *zwbs)
{
	struct zram_wb_header *zhdr;
	struct page *page = zwbs->page;
	int offset = zwbs->off;
	void *mem;

	if (offset + sizeof(struct zram_wb_header) <= PAGE_SIZE) {
		mem = kmap_atomic(page);
		zhdr = (struct zram_wb_header *)(mem + offset);
		zhdr->index = UINT_MAX;
		zhdr->size = 0;
		kunmap_atomic(mem);
	}
}

static void free_writeback_buffer(struct zram_writeback_buffer *buf)
{
	struct zwbs **zwbs;
	int i;

	if (!buf)
		return;

	zwbs = buf->zwbs;
	for (i = 0; i < NR_ZWBS; i++) {
		if (!zwbs[i])
			break;
		if (zwbs[i]->page)
			__free_page(zwbs[i]->page);
		kfree(zwbs[i]);
	}
	kfree(buf);
}

static struct zram_writeback_buffer *alloc_writeback_buffer(void)
{
	struct zram_writeback_buffer *buf;
	struct zwbs **zwbs;
	int i;

	buf = kzalloc(sizeof(struct zram_writeback_buffer), GFP_KERNEL);
	if (!buf)
		return NULL;

	zwbs = buf->zwbs;
	for (i = 0; i < NR_ZWBS; i++) {
		zwbs[i] = kzalloc(sizeof(struct zwbs), GFP_KERNEL);
		if (!zwbs[i])
			goto out;
		zwbs[i]->page = alloc_page(GFP_KERNEL);
		if (!zwbs[i]->page)
			goto out;
	}
	return buf;

out:
	free_writeback_buffer(buf);
	return NULL;
}

static int zram_writeback_aggr(struct zram *zram, u32 index,
		unsigned int stay_time, struct zram_writeback_buffer **buf)
{
	struct zwbs **zwbs;
	struct zram_writeback_buffer *tmpbuf = *buf;
	int size, i, j, ret = 0;
	u32 prio = ZRAM_PRIMARY_COMP;

retry:
	/* allocate new buffer for writeback */
	if (NULL == tmpbuf) {
		tmpbuf = *buf = alloc_writeback_buffer();
		if (NULL == tmpbuf)
			return -ENOMEM;
	}

	zwbs = tmpbuf->zwbs;
	i = tmpbuf->cnt;

	size = zram_fill_cprs_page(zram, index, zwbs, i, &prio);
	if (size > 0) {
		struct zram_wb_entry *entry = zwbs[i]->entry;

		entry[zwbs[i]->cnt].index = index;
		entry[zwbs[i]->cnt].offset = zwbs[i]->off;
		entry[zwbs[i]->cnt].size = size;
		entry[zwbs[i]->cnt].prio = prio;
		entry[zwbs[i]->cnt].stay_time = stay_time;
		zwbs[i]->off += size;
		if (size < PAGE_SIZE)
			zwbs[i]->off += sizeof(struct zram_wb_header);
		zwbs[i]->cnt++;
		tmpbuf->objcnt++;
		if (is_log_enable(ZRAM_WB_LOG))
			pr_err(MF22PRELOG"aggr %d %u %u nodcpr %u %u\n", i, zwbs[i]->cnt, zwbs[i]->off, index, size);
	}
	/* writeback when either the page or the entry is full. */
	if (-ENOSPC == size || ZRAM_WB_THRESHOLD == zwbs[i]->cnt) {
		mark_end_of_page(zwbs[i]);
		if (NR_ZWBS == ++tmpbuf->cnt) {
			ret = zram_start_writeback_aggr(zram, zwbs, NR_ZWBS, tmpbuf->objcnt);
			for (j = 0; j < NR_ZWBS; j++) {
				zwbs[j]->cnt = 0;
				zwbs[j]->off = 0;
			}
			tmpbuf->cnt = 0;
			tmpbuf->objcnt = 0;
		}
		if (0 == ret && -ENOSPC == size) {
			if (is_log_enable(ZRAM_WB_LOG))
				pr_err(MF22PRELOG"aggr retry %d %u %u nodcpr %u %d\n", i, zwbs[i]->cnt, zwbs[i]->off, index, size);
			goto retry;
		}
	}

	return ret;
}

static int zram_wbd(void *p) {
	struct zram *zram = (struct zram *)p;
	struct zram_table_entry *zram_entry = NULL;
	u32 index, i;
	struct page *page;

	ktime_t current_ts;
	s64 delta_time;
	int kernel_write_one_time;
	bool is_key_task, aggr_wb;
	unsigned int stay_time;
	struct zram_writeback_buffer *buf = NULL;
	u64 bd_count;
	int ret;

	set_freezable();

	page = alloc_page(GFP_KERNEL);
	if (!page)
		return 0;
	set_page_private(page, (unsigned long)zram);

	while (!kthread_should_stop()) {
		int nr_pages = 0;
		int nr_written_pages = 0;

		// USD: add for calculate PSI time by zihan.ju at 20240522 start
		memfusion_psi_cpu(&zram->wbd_psi_data, current->se.sum_exec_runtime, true);
		// USD: add for calculate PSI time by zihan.ju at 20240522 end
		wait_event_freezable(zram->wbd_wait, zram->wbd_running || kthread_should_stop());

		zram_kernel_trace_change(ZRAM_WBD_OUT_REASON, 0);

		/* sleep 5 seconds.*/
		wait_event_freezable_timeout(zram->wbd_wait, kthread_should_stop(), zram_wb_switch_delay_time*1000);
		if (kthread_should_stop()) {
			pr_err("memfusion2_2 thread will exit");
			goto OUT;
		}

#if defined(CONFIG_ESWAP)
		if (eswap_enable()) {
			eswap_write_back_sync(32 * SZ_1M);
			goto OUT;
		}
#endif

		if (unlikely(!g_swap_kernel_write_enable)) {
			zram_kernel_trace_change(ZRAM_WBD_OUT_REASON, 1);
			goto OUT;
		}

		// reset wbd_ifbreak_flag
		atomic64_set(&zram_wbd_ifbreak, 0);

		zram_kernel_trace_change(ZRAM_WBD_PAGE_CNT,0);

		update_bdev_index(zram);

		aggr_wb = zram_writeback_aggregation;
		/*Optimized the ZRAM write-back mechanism by dongyun.liu*/
		kernel_write_one_time = (zram_kwrite_boost>0) ? zram_kwrite_boost : zram_write_limit_debug;
		for (i = 0; i < kernel_write_one_time; i++) {
			if (try_to_freeze() || kthread_should_stop()) {
				zram_kernel_trace_change(ZRAM_WBD_OUT_REASON, 2);
				break;
			}

			if (atomic64_read(&zram_wbd_ifbreak)) {
				zram_wbd_ifbreak_times++;
				break;
			}

			if (!zram_should_writeback(zram, ++nr_pages, false)) {
				zram_kernel_trace_change(ZRAM_WBD_OUT_REASON, 3);
				break;
			}

			spin_lock(&zram->wbd_klist_lock);
			if (list_empty(&zram->wbd_klist)) {
				spin_unlock(&zram->wbd_klist_lock);
				break;
			}
			zram_entry = list_first_entry(&zram->wbd_klist, struct zram_table_entry, fifo_klist);
			zram_list_del(zram_entry, zram);
			spin_unlock(&zram->wbd_klist_lock);

			index = entry_to_index(zram, zram_entry);
			if (!zram_slot_trylock(zram, index)){
				/*trylock failed*/
				zram_list_add_to_tail(&zram->table[index], &zram->wbd_klist, zram);
				continue;
			}

			if (!zram_allocated(zram, index) || zram_test_flag(zram, index, ZRAM_UNDER_WB)) {
				zram_slot_unlock(zram, index);
				continue;
			}

			//zram_slot_lock(zram, index);
			current_ts = ktime_get_boottime();
			delta_time = ktime_ms_delta(current_ts, zram_entry->ac_time);
			is_key_task = zram_test_flag(zram, index, ZRAM_KEY);
			if(delta_time < (zram_kernel_wb_actime_limit*1000)) {
				zram_slot_unlock(zram, index);
				klist_less_time_cnt++;
				/*put the page to list tail, because it be eliminated.*/
				zram_list_add_to_tail(&zram->table[index], &zram->wbd_klist, zram);
				if (is_log_enable(ZRAM_WB_LOG)) {
					pr_err(MF22PRELOG"[%d][break][%d] %lldmin %llds %lldms %lld %s",
							i, index, delta_time/60000, delta_time%60000/1000, delta_time%1000,
							delta_time, ((zram_entry != &zram->table[index])?"nomatch":"ok"));
				}
				continue;
			}
			zram_set_flag(zram, index, ZRAM_IDLE);
			zram_slot_unlock(zram, index);

			stay_time = delta_time/ZRAM_MS_PER_SEC;
			if (is_log_enable(ZRAM_WB_LOG)) {
				pr_err(MF22PRELOG"[%d][write begin][%d] %lldmin %llds %lldms %lld %u%c",
					i, index, delta_time/60000, delta_time%60000/1000, delta_time%1000, delta_time, stay_time, is_key_task?'K':'.');
			}

			if (is_key_task) {
				if (is_log_enable(ZRAM_WB_LOG))
					pr_err(MF22PRELOG"[%d][skip][%d] keytask", i, index);
				continue;
			}

			if (aggr_wb) {
				ret = zram_writeback_aggr(zram, index, stay_time, &buf);
				if (ret) {
					zram_kwbd_wb_failed++;
					if (is_log_enable(ZRAM_WB_LOG))
						pr_err(MF22PRELOG"aggr err %d\n", ret);
					break;
				}
			} else if (start_zram_writeback(zram, index, page, stay_time)) {
				zram_kernel_trace_change(ZRAM_WBD_OUT_REASON, 4);
				zram_kwbd_wb_failed++;

				/*If the write back fails, we should put the page at the end of the fifo_klist
				  to prevent the failure of writing the page back again*/
				zram_list_add_to_tail(&zram->table[index], &zram->wbd_klist, zram);
				break;
			}

			nr_written_pages++;
		}

		if (nr_written_pages > 0) {
			actual_write_cnt++;
			last_write_cnt = nr_written_pages;
			if (is_log_enable(ZRAM_WB_LOG)) {
				bd_count = ((u64)atomic64_read(&zram->stats.bd_count))<<(PAGE_SHIFT-10);
				pr_err(MF22PRELOG"%s %llu KB in ROM, actual ROM usage is %llu KB\n",
					aggr_wb?"aggrwb":"wb", !aggr_wb?bd_count:((u64)atomic64_read(&zram->stats.bd_objcnt)<<(PAGE_SHIFT-10)), bd_count);
			}
		}
		zram_kernel_trace_change(ZRAM_WBD_PAGE_CNT, nr_pages);
		/* reset the punitive writeback */
		zram_kwrite_boost = 0;
OUT:
		free_writeback_buffer(buf);
		buf = NULL;
		zram->wbd_running = false;
	}
	__free_page(page);
	free_writeback_buffer(buf);

	return 0;
}

#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end


#define PAGE_WB_SIG "page_index="

#define PAGE_WRITEBACK			0
#define HUGE_WRITEBACK			(1<<0)
#define IDLE_WRITEBACK			(1<<1)
#define INCOMPRESSIBLE_WRITEBACK	(1<<2)

static ssize_t writeback_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	unsigned long index = 0;
	struct bio bio;
	struct bio_vec bio_vec;
	struct page *page;
	ssize_t ret = len;
	int mode, err;
	unsigned long blk_idx = 0;

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	int bd_index;
	unsigned long physpages;
	pr_err(MF22PRELOG"writeback store called.");
	if (unlikely(zram_wbd_debug)) {
		dump_stack();
	}

	if (unlikely(!zram_writeback_store_enabled)) {
		pr_info("[memfusion2_2]writeback_store disabled\n");
		return len;
	}
  
	physpages = get_num_physpages();
	if (zram_writeback_disable_gt4G && physpages > ZRAM_NR_PAGES_4G) {
		pr_err("[memfusion2_2]writeback_store disabled due to %lu > %lu\n", physpages, ZRAM_NR_PAGES_4G);
		return ret;
	}

	/*we just counting, so don't care about concurrency problem*/
	zram_writeback_store_times++;
#endif

	if (sysfs_streq(buf, "idle"))
		mode = IDLE_WRITEBACK;
	else if (sysfs_streq(buf, "huge"))
		mode = HUGE_WRITEBACK;
	else if (sysfs_streq(buf, "huge_idle"))
		mode = IDLE_WRITEBACK | HUGE_WRITEBACK;
	else if (sysfs_streq(buf, "incompressible"))
		mode = INCOMPRESSIBLE_WRITEBACK;
	else {
		if (strncmp(buf, PAGE_WB_SIG, sizeof(PAGE_WB_SIG) - 1))
			return -EINVAL;

		if (kstrtol(buf + sizeof(PAGE_WB_SIG) - 1, 10, &index) ||
				index >= nr_pages)
			return -EINVAL;

		nr_pages = 1;
		mode = PAGE_WRITEBACK;
	}

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	/* update backing device to writeback by hold zram->init_lock*/
	update_bdev_index(zram);
#endif

	down_read(&zram->init_lock);
	if (!init_done(zram)) {
		ret = -EINVAL;
		goto release_init_lock;
	}

	if (!zram->backing_dev) {
		ret = -ENODEV;
		goto release_init_lock;
	}

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		ret = -ENOMEM;
		goto release_init_lock;
	}

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	bd_index = atomic_read(&zram->backing_dev_index);
#endif

	for (; nr_pages != 0; index++, nr_pages--) {
		spin_lock(&zram->wb_limit_lock);
		if (zram->wb_limit_enable && !zram->bd_wb_limit) {
			spin_unlock(&zram->wb_limit_lock);
			ret = -EIO;
			break;
		}
		spin_unlock(&zram->wb_limit_lock);

		if (!blk_idx) {
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
			blk_idx = alloc_block_bdev_list(zram, bd_index);
#else
			blk_idx = alloc_block_bdev(zram);
#endif
			if (!blk_idx) {
				ret = -ENOSPC;
				break;
			}
		}

		zram_slot_lock(zram, index);
		if (!zram_allocated(zram, index))
			goto next;

		if (zram_test_flag(zram, index, ZRAM_WB) ||
				zram_test_flag(zram, index, ZRAM_SAME) ||
				zram_test_flag(zram, index, ZRAM_UNDER_WB))
			goto next;

		if (mode & IDLE_WRITEBACK &&
		    !zram_test_flag(zram, index, ZRAM_IDLE))
			goto next;
		if (mode & HUGE_WRITEBACK &&
		    !zram_test_flag(zram, index, ZRAM_HUGE))
			goto next;
		if (mode & INCOMPRESSIBLE_WRITEBACK &&
		    !zram_test_flag(zram, index, ZRAM_INCOMPRESSIBLE))
			goto next;

		/*
		 * Clearing ZRAM_UNDER_WB is duty of caller.
		 * IOW, zram_free_page never clear it.
		 */
		zram_set_flag(zram, index, ZRAM_UNDER_WB);
		/* Need for hugepage writeback racing */
		zram_set_flag(zram, index, ZRAM_IDLE);
		zram_slot_unlock(zram, index);
		if (zram_read_page(zram, page, index, NULL)) {
			zram_slot_lock(zram, index);
			zram_clear_flag(zram, index, ZRAM_UNDER_WB);
			zram_clear_flag(zram, index, ZRAM_IDLE);
			zram_slot_unlock(zram, index);
			continue;
		}

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
		bio_init(&bio, zram->bdev_list[bd_index], &bio_vec, 1,
			 REQ_OP_WRITE | REQ_SYNC);
#else
		bio_init(&bio, zram->bdev, &bio_vec, 1,
			 REQ_OP_WRITE | REQ_SYNC);
#endif
		bio.bi_iter.bi_sector = blk_idx * (PAGE_SIZE >> 9);
		__bio_add_page(&bio, page, PAGE_SIZE, 0);

		/*
		 * XXX: A single page IO would be inefficient for write
		 * but it would be not bad as starter.
		 */
		err = submit_bio_wait(&bio);
		if (err) {
			zram_slot_lock(zram, index);
			zram_clear_flag(zram, index, ZRAM_UNDER_WB);
			zram_clear_flag(zram, index, ZRAM_IDLE);
			zram_slot_unlock(zram, index);
			/*
			 * BIO errors are not fatal, we continue and simply
			 * attempt to writeback the remaining objects (pages).
			 * At the same time we need to signal user-space that
			 * some writes (at least one, but also could be all of
			 * them) were not successful and we do so by returning
			 * the most recent BIO error.
			 */
			ret = err;
			continue;
		}

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
		atomic64_inc(&zram->stats.bd_writes_list[bd_index]);
		atomic64_inc(&zram->stats.bd_wb_reclaim);
#endif
		atomic64_inc(&zram->stats.bd_writes);
		/*
		 * We released zram_slot_lock so need to check if the slot was
		 * changed. If there is freeing for the slot, we can catch it
		 * easily by zram_allocated.
		 * A subtle case is the slot is freed/reallocated/marked as
		 * ZRAM_IDLE again. To close the race, idle_store doesn't
		 * mark ZRAM_IDLE once it found the slot was ZRAM_UNDER_WB.
		 * Thus, we could close the race by checking ZRAM_IDLE bit.
		 */
		zram_slot_lock(zram, index);
		if (!zram_allocated(zram, index) ||
			  !zram_test_flag(zram, index, ZRAM_IDLE)) {
			zram_clear_flag(zram, index, ZRAM_UNDER_WB);
			zram_clear_flag(zram, index, ZRAM_IDLE);
			goto next;
		}

		/* USD: we released memory of entry when the page have been moved to backing device.*/
		zram_free_page(zram, index);
		zram_clear_flag(zram, index, ZRAM_UNDER_WB);
		zram_set_flag(zram, index, ZRAM_WB);
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
		zram_set_bd_index(zram, index, bd_index);
		blk_idx <<= AGGR_BLKIDX_SHIFT;
#endif
		/*USD: we set blk_idx to zram->table[index].element in order to find page in backing device in future*/
		zram_set_element(zram, index, blk_idx);
		blk_idx = 0;
		atomic64_inc(&zram->stats.pages_stored);
		spin_lock(&zram->wb_limit_lock);
		if (zram->wb_limit_enable && zram->bd_wb_limit > 0)
			zram->bd_wb_limit -=  1UL << (PAGE_SHIFT - 12);
		spin_unlock(&zram->wb_limit_lock);
next:
		zram_slot_unlock(zram, index);
	}

	if (blk_idx)
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
		free_block_bdev_list(zram, blk_idx, bd_index);
#else
		free_block_bdev(zram, blk_idx);
#endif
	__free_page(page);
release_init_lock:
	up_read(&zram->init_lock);

	return ret;
}

struct zram_work {
	struct work_struct work;
	struct zram *zram;
	unsigned long entry;
	struct page *page;
	int error;
};

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
static noinline void zram_handle_remain_nodcprs(struct zram *zram, struct page *page,
				unsigned int blk_idx, int bd_index)
{
	struct zram_wb_header *zhdr;
	unsigned long alloced_pages;
	unsigned long handle;
	unsigned int offset = 0;
	unsigned int size;
	u32 index;
	u8 *mem, *src, *dst;
	u32 cnt = 0;

	mem = kmap_atomic(page);
	while ((offset + sizeof(struct zram_wb_header) < PAGE_SIZE) &&
		(cnt++ < ZRAM_WB_THRESHOLD)) {
		zhdr = (struct zram_wb_header *)(mem + offset);
		index = zhdr->index;
		size = zhdr->size;

		/* invalid index */
		if (index >= (zram->disksize >> PAGE_SHIFT))
			break;

		if (!zram_slot_trylock(zram, index))
			goto next;

		if (!zram_allocated(zram, index) ||
			!zram_test_flag(zram, index, ZRAM_WB) ||
			zram_test_flag(zram, index, ZRAM_READ_BDEV)) {
			zram_slot_unlock(zram, index);
			goto next;
		}
		handle = zram_get_element(zram, index);
		if ((handle >> AGGR_BLKIDX_SHIFT) != blk_idx ||
			((handle >> AGGR_OFFSET_SHIFT) & AGGR_OFFSET_MASK) != offset ||
			(handle & AGGR_CPRS_SZ_MASK) != size) {
			zram_slot_unlock(zram, index);
			goto next;
		}

		handle = zs_malloc(zram->mem_pool, size,
				__GFP_KSWAPD_RECLAIM |
				__GFP_NOWARN |
				__GFP_HIGHMEM |
				__GFP_MOVABLE |
				__GFP_CMA);
		if (IS_ERR_VALUE(handle)) {
			zram_slot_unlock(zram, index);
			break;
		}
		alloced_pages = zs_get_total_pages(zram->mem_pool);
		update_used_max(zram, alloced_pages);

		dst = zs_map_object(zram->mem_pool, handle, ZS_MM_WO);
		src = (u8 *)(zhdr + 1);
		memcpy(dst, src, size);
		zs_unmap_object(zram->mem_pool, handle);

		atomic64_inc(&zram->stats.meta_data_size);
		atomic64_add(size, &zram->stats.compr_data_size);
		zram_free_page(zram, index);
		zram_set_element(zram, index, handle);
		zram_set_obj_size(zram, index, size);
		zram_slot_unlock(zram, index);

		zram_list_add_to_tail(&zram->table[index], &zram->wbd_klist, zram);
		atomic64_inc(&zram->stats.pages_stored);
next:
		offset += (size + sizeof(struct zram_wb_header));
	}
	kunmap_atomic(mem);
	free_block_bdev_list(zram, blk_idx, bd_index);
	atomic64_inc(&zram->stats.bd_objcnt);
}

static void handle_nodcprs(struct work_struct *work)
{
	struct zram_wb_work *zw = container_of(work, struct zram_wb_work, work);
	struct zram_wb_header *zhdr;
	struct zram *zram = zw->zram;
	struct zcomp_strm *zstrm;
	struct page *src_page = zw->src_page;
	struct page *dst_page = zw->dst_page;
	struct bio *bio = zw->bio;

	unsigned int blk_idx = zw->handle >> AGGR_BLKIDX_SHIFT;
	u32 prio = (zw->handle >> AGGR_ALGO_SHIFT) & ZRAM_COMP_PRIORITY_MASK;
	unsigned int offset = (zw->handle >> AGGR_OFFSET_SHIFT) & AGGR_OFFSET_MASK;
	unsigned int size = zw->handle & AGGR_CPRS_SZ_MASK;
	int bd_index;

	u8 *src, *dst;
	int ret;
	bool is_draw = zw->flags & MEMFUSION_DRAW_FLAG;
	ktime_t now = ktime_get_boottime();
	s64 duration;

	duration = ktime_us_delta(now, zw->start);
	if (duration) {
		zram_io_delay_stat(zram, duration);
		atomic64_add(duration, &zram->stats.swapin_delay);
		atomic64_inc(&zram->stats.swapin_count);
		if (is_draw)
			atomic64_add(duration, &zram->stats.draw_io_delay);
	}

	if (is_log_enable(ZRAM_READ_LOG))
		pr_err(MF22PRELOG"no-dcpr end io:%lld d:%lld cnt:%lld p-bio:0x%px %c %u sz %u\n",
			duration, ktime_sub_ns(ktime_get_boottime(), now),
			atomic64_read(&zram->stats.swapin_count), bio,
			is_draw?'Y':'N', zw->index, size);

	src = kmap_atomic(src_page);
	zhdr = (struct zram_wb_header *)(src + offset);

	if (zhdr->size != size || zhdr->index != zw->index) {
		pr_err(MF22PRELOG"hdr err, size/index should be %u/%u but was %u/%u  src=0x%px offset=%u\n",
			size, zw->index, zhdr->size, zhdr->index, src, offset);
		BUG();
	}

	zram_slot_lock(zram, zw->index);
	bd_index = zram_get_bd_index(zram, zw->index);
	zram_clear_flag(zram, zw->index, ZRAM_READ_BDEV);
	zram_slot_unlock(zram, zw->index);

	dst = kmap_atomic(dst_page);
	zstrm = zcomp_stream_get(zram->comps[prio]);
	ret = zcomp_decompress(zstrm, src + offset + sizeof(struct zram_wb_header), size, dst);
	zcomp_stream_put(zram->comps[prio]);
	if (ret) {
		pr_err(MF22PRELOG"Decompression failed! err=%d offset=%u size=%u addr=%p\n",
			ret, offset, size, src);
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_OFFSET, 16, 1,
				src, PAGE_SIZE, 1);
		BUG_ON(ret);
	}
	kunmap_atomic(dst);
	kunmap_atomic(src);

	/* increment refcount to prevent freeing block */
	spin_lock(&zram->wb_table_lock);
	if (zram->wb_table[bd_index])
		zram->wb_table[bd_index][blk_idx]++;
	spin_unlock(&zram->wb_table_lock);

	if (zw->bio_chain)
		bio_endio(zw->bio_chain);
	bio_put(bio);

	zram_handle_remain_nodcprs(zram, src_page, blk_idx, bd_index);
	kfree(zw);
	__free_page(src_page);

	/*do something insignificant*/
	try_to_add_to_table(&zram->wbd_stat.rd_hour, now, atomic64_read(&zram->stats.swapin_count) * 4);
	memfusion_psi_io(&zram->wbd_psi_data, duration, true);
	memfusion_psi_mem(&zram->wbd_psi_data, duration, true);
}

static void zram_nodcprs_page_end_io(struct bio *bio)
{
	struct page *page = bio->bi_io_vec[0].bv_page;
	struct zram_wb_work *zw = (struct zram_wb_work *)page_private(page);
	int errno = blk_status_to_errno(bio->bi_status);

	if (errno) {
		pr_err(MF22PRELOG"read %u I/O error %d \n", zw->index, errno);
		if (zw->bio_chain)
			bio_endio(zw->bio_chain);
		bio_put(bio);
		if (zw->src_page)
			__free_page(zw->src_page);
		kfree(zw);
		return;
	}

	INIT_WORK(&zw->work, handle_nodcprs);
	schedule_work(&zw->work);
}

static int read_nodcprs_from_bdev(struct zram *zram, struct page *page, int bd_index,
		u32 index, unsigned long handle, struct bio *parent)
{
	struct zram_wb_work *zw;
	struct bio *bio;
	unsigned long blk_idx = handle >> AGGR_BLKIDX_SHIFT;
	bool is_draw;
	bool is_vipio = false;
	struct page* src_page;
	unsigned int stay_time;

	atomic64_inc(&zram->stats.bd_reads_list[bd_index]);
	atomic64_inc(&zram->stats.bd_reads);

#if IS_ENABLED(CONFIG_TRANSSION_BLK)
	if (zram_vip_io_sched_enable) {
		parent->bi_opf |= (REQ_SYNC | REQ_FG);
		is_vipio = true;
	}
#endif

	bio = bio_alloc(zram->bdev_list[bd_index], 1, parent->bi_opf, GFP_NOIO);
	if (!bio)
		return -ENOMEM;

	src_page = alloc_page(GFP_NOIO|__GFP_HIGHMEM);
	if (!src_page) {
		pr_info(MF22PRELOG"read nodcprs failed to alloc page\n");
		bio_put(bio);
		return -ENOMEM;
	}

	zw = kzalloc(sizeof(struct zram_wb_work), GFP_NOIO);
	if (!zw) {
		pr_info(MF22PRELOG"read nodcprs failed to alloc zww\n");
		__free_page(src_page);
		bio_put(bio);
		return -ENOMEM;
	}

	is_draw = update_read_stats(zram);
	zw->index = index;
	zw->start = ktime_get_boottime();
	zw->flags |= (is_draw?MEMFUSION_DRAW_FLAG:0);
	zw->src_page = src_page;
	zw->dst_page = page;
	zw->zram = zram;
	zw->bio = bio;
	zw->handle = handle;
	set_page_private(src_page, (unsigned long)zw);

	bio->bi_end_io = zram_nodcprs_page_end_io;
	bio->bi_iter.bi_sector = blk_idx * (PAGE_SIZE >> 9);
	__bio_add_page(bio, src_page, PAGE_SIZE, 0);

	if (parent) {
		zw->bio_chain = bio_alloc(zram->bdev_list[bd_index], 1, parent->bi_opf, GFP_NOIO);
		if (!zw->bio_chain) {
			kfree(zw);
			__free_page(src_page);
			bio_put(bio);
			return -ENOMEM;
		}
		bio_chain(zw->bio_chain, parent);
	}

	if (is_log_enable(ZRAM_READ_LOG)) {
		zram_slot_lock(zram, index);
		stay_time = zram->table[index].stay_time;
		zram_slot_unlock(zram, index);
		if (is_log_enable(ZRAM_READ_LOG))
			pr_err(MF22PRELOG"[%s] read nodcprs child:0x%px index:%u %lld us %c stay:%u s vip-io[%c]\n",
				current->comm, bio, index, ktime_us_delta(ktime_get(), zw->start),
				is_draw?'Y':'N', stay_time, is_vipio?'Y':'N');
	}

	submit_bio(bio);
	return 1;
}
#endif

static void zram_sync_read(struct work_struct *work)
{
	struct zram_work *zw = container_of(work, struct zram_work, work);
	struct bio_vec bv;
	struct bio bio;

	bio_init(&bio, zw->zram->bdev, &bv, 1, REQ_OP_READ);
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	bio.bi_iter.bi_sector = zram_get_element(zw->zram, zw->entry) * (PAGE_SIZE >> 9);
#else
	bio.bi_iter.bi_sector = zw->entry * (PAGE_SIZE >> 9);
#endif
	__bio_add_page(&bio, zw->page, PAGE_SIZE, 0);
	zw->error = submit_bio_wait(&bio);
}

/*
 * Block layer want one ->submit_bio to be active at a time, so if we use
 * chained IO with parent IO in same context, it's a deadlock. To avoid that,
 * use a worker thread context.
 */
static int read_from_bdev_sync(struct zram *zram, struct page *page,
				unsigned long entry)
{
	struct zram_work work;

	work.page = page;
	work.zram = zram;
	work.entry = entry;

	INIT_WORK_ONSTACK(&work.work, zram_sync_read);
	queue_work(system_unbound_wq, &work.work);
	flush_work(&work.work);
	destroy_work_on_stack(&work.work);

	return work.error;
}

static int read_from_bdev(struct zram *zram, struct page *page,
			unsigned long entry, struct bio *parent)
{
	atomic64_inc(&zram->stats.bd_reads);
	if (!parent) {
		if (WARN_ON_ONCE(!IS_ENABLED(ZRAM_PARTIAL_IO)))
			return -EIO;
		return read_from_bdev_sync(zram, page, entry);
	}
	read_from_bdev_async(zram, page, entry, parent);
	return 0;
}
#else
static inline void reset_bdev(struct zram *zram) {};
static int read_from_bdev(struct zram *zram, struct page *page,
			unsigned long entry, struct bio *parent)
{
	return -EIO;
}

static void free_block_bdev(struct zram *zram, unsigned long blk_idx) {};
#endif

#ifdef CONFIG_ZRAM_MEMORY_TRACKING

static struct dentry *zram_debugfs_root;

static void zram_debugfs_create(void)
{
	zram_debugfs_root = debugfs_create_dir("zram", NULL);
}

static void zram_debugfs_destroy(void)
{
	debugfs_remove_recursive(zram_debugfs_root);
}

static ssize_t read_block_state(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	char *kbuf;
	ssize_t index, written = 0;
	struct zram *zram = file->private_data;
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	struct timespec64 ts;
//SPD: add XLBYYHBWQ-3103 for MemFusion2.1 by shuang.zhou at 20210720 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	ktime_t current_ts = ktime_get_boottime();
#endif
//SPD: add XLBYYHBWQ-3103 for MemFusion2.1 by shuang.zhou at 20210720 end

	kbuf = kvmalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	down_read(&zram->init_lock);
	if (!init_done(zram)) {
		up_read(&zram->init_lock);
		kvfree(kbuf);
		return -EINVAL;
	}

	for (index = *ppos; index < nr_pages; index++) {
		int copied;

		zram_slot_lock(zram, index);
		if (!zram_allocated(zram, index))
			goto next;

		ts = ktime_to_timespec64(zram->table[index].ac_time);
//SPD: add XLBYYHBWQ-3103 for MemFusion2.1 by shuang.zhou at 20210720 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
		copied = snprintf(kbuf + written, count,
			"%12zd %12lld.%06lu %c %c %c %c %c %c %c %c %12lld\n",
			index, (s64)ts.tv_sec,
			ts.tv_nsec / NSEC_PER_USEC,
			zram_test_flag(zram, index, ZRAM_SAME) ? 's' : '.',
			zram_test_flag(zram, index, ZRAM_WB) ? 'w' : '.',
			zram_test_flag(zram, index, ZRAM_HUGE) ? 'h' : '.',
			zram_test_flag(zram, index, ZRAM_IDLE) ? 'i' : '.',
			zram_get_priority(zram, index) ? 'r' : '.',
			zram_test_flag(zram, index,
				       ZRAM_INCOMPRESSIBLE) ? 'n' : '.',
			zram_test_flag(zram, index, ZRAM_COLD) ? 'C' : '.',
			zram_test_flag(zram, index, ZRAM_KEY) ? 'K' : '.',
			ktime_ms_delta(current_ts,zram->table[index].ac_time));
#else
		copied = snprintf(kbuf + written, count,
			"%12zd %12lld.%06lu %c%c%c%c%c%c\n",
			index, (s64)ts.tv_sec,
			ts.tv_nsec / NSEC_PER_USEC,
			zram_test_flag(zram, index, ZRAM_SAME) ? 's' : '.',
			zram_test_flag(zram, index, ZRAM_WB) ? 'w' : '.',
			zram_test_flag(zram, index, ZRAM_HUGE) ? 'h' : '.',
			zram_test_flag(zram, index, ZRAM_IDLE) ? 'i' : '.',
			zram_get_priority(zram, index) ? 'r' : '.',
			zram_test_flag(zram, index,
				       ZRAM_INCOMPRESSIBLE) ? 'n' : '.');
#endif
		if (count <= copied) {
			zram_slot_unlock(zram, index);
			break;
		}
		written += copied;
		count -= copied;
next:
		zram_slot_unlock(zram, index);
		*ppos += 1;
	}

	up_read(&zram->init_lock);
	if (copy_to_user(buf, kbuf, written))
		written = -EFAULT;
	kvfree(kbuf);

	return written;
}

static const struct file_operations proc_zram_block_state_op = {
	.open = simple_open,
	.read = read_block_state,
	.llseek = default_llseek,
};

static void zram_debugfs_register(struct zram *zram)
{
	if (!zram_debugfs_root)
		return;

	zram->debugfs_dir = debugfs_create_dir(zram->disk->disk_name,
						zram_debugfs_root);
	debugfs_create_file("block_state", 0400, zram->debugfs_dir,
				zram, &proc_zram_block_state_op);
}

static void zram_debugfs_unregister(struct zram *zram)
{
	debugfs_remove_recursive(zram->debugfs_dir);
}
#else
static void zram_debugfs_create(void) {};
static void zram_debugfs_destroy(void) {};
static void zram_debugfs_register(struct zram *zram) {};
static void zram_debugfs_unregister(struct zram *zram) {};
#endif

/*
 * We switched to per-cpu streams and this attr is not needed anymore.
 * However, we will keep it around for some time, because:
 * a) we may revert per-cpu streams in the future
 * b) it's visible to user space and we need to follow our 2 years
 *    retirement rule; but we already have a number of 'soon to be
 *    altered' attrs, so max_comp_streams need to wait for the next
 *    layoff cycle.
 */
static ssize_t max_comp_streams_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", num_online_cpus());
}

static ssize_t max_comp_streams_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	return len;
}

static void comp_algorithm_set(struct zram *zram, u32 prio, const char *alg)
{
	/* Do not free statically defined compression algorithms */
	if (zram->comp_algs[prio] != default_compressor)
		kfree(zram->comp_algs[prio]);

	zram->comp_algs[prio] = alg;
}

static ssize_t __comp_algorithm_show(struct zram *zram, u32 prio, char *buf)
{
	ssize_t sz;

	down_read(&zram->init_lock);
	sz = zcomp_available_show(zram->comp_algs[prio], buf);
	up_read(&zram->init_lock);

	return sz;
}

static int __comp_algorithm_store(struct zram *zram, u32 prio, const char *buf)
{
	char *compressor;
	size_t sz;

	sz = strlen(buf);
	if (sz >= CRYPTO_MAX_ALG_NAME)
		return -E2BIG;

	compressor = kstrdup(buf, GFP_KERNEL);
	if (!compressor)
		return -ENOMEM;

	/* ignore trailing newline */
	if (sz > 0 && compressor[sz - 1] == '\n')
		compressor[sz - 1] = 0x00;

	if (!zcomp_available_algorithm(compressor)) {
		kfree(compressor);
		return -EINVAL;
	}

	down_write(&zram->init_lock);
	if (init_done(zram)) {
		up_write(&zram->init_lock);
		kfree(compressor);
		pr_info("Can't change algorithm for initialized device\n");
		return -EBUSY;
	}

	comp_algorithm_set(zram, prio, compressor);
	up_write(&zram->init_lock);
	return 0;
}

static ssize_t comp_algorithm_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct zram *zram = dev_to_zram(dev);

	return __comp_algorithm_show(zram, ZRAM_PRIMARY_COMP, buf);
}

static ssize_t comp_algorithm_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	int ret;

	ret = __comp_algorithm_store(zram, ZRAM_PRIMARY_COMP, buf);
	return ret ? ret : len;
}

#ifdef CONFIG_ZRAM_MULTI_COMP
static ssize_t recomp_algorithm_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t sz = 0;
	u32 prio;

	for (prio = ZRAM_SECONDARY_COMP; prio < ZRAM_MAX_COMPS; prio++) {
		if (!zram->comp_algs[prio])
			continue;

		sz += scnprintf(buf + sz, PAGE_SIZE - sz - 2, "#%d: ", prio);
		sz += __comp_algorithm_show(zram, prio, buf + sz);
	}

	return sz;
}

static ssize_t recomp_algorithm_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	int prio = ZRAM_SECONDARY_COMP;
	char *args, *param, *val;
	char *alg = NULL;
	int ret;

	args = skip_spaces(buf);
	while (*args) {
		args = next_arg(args, &param, &val);

		if (!val || !*val)
			return -EINVAL;

		if (!strcmp(param, "algo")) {
			alg = val;
			continue;
		}

		if (!strcmp(param, "priority")) {
			ret = kstrtoint(val, 10, &prio);
			if (ret)
				return ret;
			continue;
		}
	}

	if (!alg)
		return -EINVAL;

	if (prio < ZRAM_SECONDARY_COMP || prio >= ZRAM_MAX_COMPS)
		return -EINVAL;

	ret = __comp_algorithm_store(zram, prio, alg);
	return ret ? ret : len;
}
#endif

static ssize_t compact_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	if (!init_done(zram)) {
		up_read(&zram->init_lock);
		return -EINVAL;
	}

	zs_compact(zram->mem_pool);
	up_read(&zram->init_lock);

	return len;
}

static ssize_t io_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret;

	down_read(&zram->init_lock);
	ret = scnprintf(buf, PAGE_SIZE,
			"%8llu %8llu 0 %8llu\n",
			(u64)atomic64_read(&zram->stats.failed_reads),
			(u64)atomic64_read(&zram->stats.failed_writes),
			(u64)atomic64_read(&zram->stats.notify_free));
	up_read(&zram->init_lock);

	return ret;
}

static ssize_t mm_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	struct zs_pool_stats pool_stats;
	u64 orig_size, mem_used = 0;
	long max_used;
	ssize_t ret;

	memset(&pool_stats, 0x00, sizeof(struct zs_pool_stats));

	down_read(&zram->init_lock);
	if (init_done(zram)) {
		mem_used = zs_get_total_pages(zram->mem_pool);
		zs_pool_stats(zram->mem_pool, &pool_stats);
	}

	orig_size = atomic64_read(&zram->stats.pages_stored);
	max_used = atomic_long_read(&zram->stats.max_used_pages);

	ret = scnprintf(buf, PAGE_SIZE,
			"%8llu %8llu %8llu %8lu %8ld %8llu %8lu %8llu %8llu %8llu %8llu\n",
			orig_size << PAGE_SHIFT,
			(u64)atomic64_read(&zram->stats.compr_data_size),
			mem_used << PAGE_SHIFT,
			zram->limit_pages << PAGE_SHIFT,
			max_used << PAGE_SHIFT,
			(u64)atomic64_read(&zram->stats.same_pages),
			atomic_long_read(&pool_stats.pages_compacted),
			(u64)atomic64_read(&zram->stats.huge_pages),
			(u64)atomic64_read(&zram->stats.huge_pages_since),
			(u64)atomic64_read(&zram->stats.incompressible_pages),
			(u64)atomic64_read(&zram->stats.incompressible_pages_since));
	up_read(&zram->init_lock);

	return ret;
}

#ifdef CONFIG_ZRAM_WRITEBACK
#define FOUR_K(x) ((x) * (1 << (PAGE_SHIFT - 12)))
static ssize_t bd_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret;

	down_read(&zram->init_lock);
	ret = scnprintf(buf, PAGE_SIZE,
		"%8llu %8llu %8llu\n",
			FOUR_K((u64)atomic64_read(&zram->stats.bd_count)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_reads)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_writes)));
	up_read(&zram->init_lock);

	return ret;
}
#endif

static ssize_t debug_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int version = 1;
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret;

	down_read(&zram->init_lock);
	ret = scnprintf(buf, PAGE_SIZE,
			"version: %d\n%8llu %8llu\n",
			version,
			(u64)atomic64_read(&zram->stats.writestall),
			(u64)atomic64_read(&zram->stats.miss_free));
	up_read(&zram->init_lock);

	return ret;
}

static DEVICE_ATTR_RO(io_stat);
static DEVICE_ATTR_RO(mm_stat);
#ifdef CONFIG_ZRAM_WRITEBACK
static DEVICE_ATTR_RO(bd_stat);
#endif
static DEVICE_ATTR_RO(debug_stat);

static void zram_meta_free(struct zram *zram, u64 disksize)
{
	size_t num_pages = disksize >> PAGE_SHIFT;
	size_t index;

	if (!zram->table)
		return;

	/* Free all pages that are still in this zram device */
	for (index = 0; index < num_pages; index++)
		zram_free_page(zram, index);

	zs_destroy_pool(zram->mem_pool);
	vfree(zram->table);
	zram->table = NULL;
}

static bool zram_meta_alloc(struct zram *zram, u64 disksize)
{
	size_t num_pages;
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	int i;
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end

	num_pages = disksize >> PAGE_SHIFT;
	zram->table = vzalloc(array_size(num_pages, sizeof(*zram->table)));
	if (!zram->table)
		return false;

	zram->mem_pool = zs_create_pool(zram->disk->disk_name);
	if (!zram->mem_pool) {
		vfree(zram->table);
		zram->table = NULL;
		return false;
	}
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	for (i = 0; i < num_pages; i++)
		INIT_LIST_HEAD(&zram->table[i].fifo_klist);
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end

	if (!huge_class_size)
		huge_class_size = zs_huge_class_size(zram->mem_pool);
	return true;
}

/*
 * To protect concurrent access to the same index entry,
 * caller should hold this table index entry's bit_spinlock to
 * indicate this index entry is accessing.
 */
static void zram_free_page(struct zram *zram, size_t index)
{
	unsigned long handle;
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	bool new_memfusion = is_memcg_compact_v2;
	bool old_memfusion = is_memfusion;
#endif
#if defined(CONFIG_ESWAP)
	struct mem_cgroup *memcg = NULL;
#endif

#ifdef CONFIG_ZRAM_TRACK_ENTRY_ACTIME
	zram->table[index].ac_time = 0;
    zram->table[index].stay_time = 0;
#endif

#if defined(CONFIG_ESWAP)
	if (new_memfusion)
		memcg = eswap_untrack_memcg_get(zram, index);
#endif
	if (zram_test_flag(zram, index, ZRAM_IDLE))
		zram_clear_flag(zram, index, ZRAM_IDLE);

#if defined(CONFIG_ESWAP)
	if (zram_test_flag(zram, index, ZRAM_HUGE)) {
		zram_clear_flag(zram, index, ZRAM_HUGE);
		atomic64_dec(&zram->stats.huge_pages);
		if (new_memfusion && memcg)
			atomic64_dec(&MEMCG_ESTRACK_FIELD(memcg, huge_pages));
	}
#else
	if (zram_test_flag(zram, index, ZRAM_HUGE)) {
		zram_clear_flag(zram, index, ZRAM_HUGE);
		atomic64_dec(&zram->stats.huge_pages);
	}
#endif

#ifdef CONFIG_ZRAM_MULTI_COMP
	if (zram_get_priority(zram, index) == ZRAM_SECONDARY_COMP)
		atomic64_dec(&zram->stats.wkdm_pages);

	if (zram_test_flag(zram, index, ZRAM_INCOMPRESSIBLE)) {
		zram_clear_flag(zram, index, ZRAM_INCOMPRESSIBLE);
		atomic64_dec(&zram->stats.incompressible_pages);
	}
#endif

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	if (zram_test_flag(zram, index, ZRAM_APPCOMPACT)) {
		zram_clear_flag(zram, index, ZRAM_APPCOMPACT);
	}

	if (zram_test_flag(zram, index, ZRAM_COLD)) {
		zram_clear_flag(zram, index, ZRAM_COLD);
	}

	if (zram_test_flag(zram, index, ZRAM_KEY)) {
		zram_clear_flag(zram, index, ZRAM_KEY);
		atomic64_dec(&zram->stats.key_pages);
	}

	if (zram_test_flag(zram, index, ZRAM_READ_BDEV))
		zram_clear_flag(zram, index, ZRAM_READ_BDEV);

	if (!new_memfusion && old_memfusion) {
		spin_lock(&zram->wbd_klist_lock);
		zram_list_del(&zram->table[index], zram);
		spin_unlock(&zram->wbd_klist_lock);
	}
#endif

	zram_set_priority(zram, index, 0);

	if (zram_test_flag(zram, index, ZRAM_WB)) {
		zram_clear_flag(zram, index, ZRAM_WB);
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
		int size,bd_index;
		if (!new_memfusion && old_memfusion) {
			bd_index = zram_get_bd_index(zram, index);
			handle = zram_get_element(zram, index);
			size = handle & AGGR_CPRS_SZ_MASK;
			if (size == 0)
				size = PAGE_SIZE;

			if (size > 0 && size <= PAGE_SIZE)
				atomic64_sub(size, &zram->stats.bd_size);
			else
				atomic64_add(size, &zram->stats.bd_size_dec_except);
			free_block_bdev_list(zram, handle >> AGGR_BLKIDX_SHIFT, bd_index);
			if (is_log_enable(ZRAM_FREE_LOG))
				pr_err("free index:%zu comm:%s pid:%hd tgid:%hd actime:%lld",
					index, current->comm, current->pid, current->tgid, zram->table[index].ac_time);
		}
#else
		free_block_bdev(zram, zram_get_element(zram, index));
#endif
		goto out;
	}

	/*
	 * No memory is allocated for same element filled pages.
	 * Simply clear same page flag.
	 */
	if (zram_test_flag(zram, index, ZRAM_SAME)) {
		zram_clear_flag(zram, index, ZRAM_SAME);
		atomic64_dec(&zram->stats.same_pages);
#if defined(CONFIG_ESWAP)
		if (new_memfusion && memcg) {
			atomic64_dec(&MEMCG_ESTRACK_FIELD(memcg_eswap_memcg(memcg), same_pages));
			memcg_dec_samepages_counter(memcg);
		}
#endif
		goto out;
	}

	handle = zram_get_handle(zram, index);
#if defined(CONFIG_ESWAP)
	if (!handle) {
		if (new_memfusion)
			eswap_untrack_memcg_put(memcg);
		return;
	}
#else
	if (!handle)
		return;
#endif

	zs_free(zram->mem_pool, handle);

	atomic64_sub(zram_get_obj_size(zram, index),
			&zram->stats.compr_data_size);
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	if (!new_memfusion && old_memfusion)
		atomic64_dec(&zram->stats.meta_data_size);
#endif
out:
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	if (!new_memfusion && old_memfusion)
		zram_set_bd_index(zram, index, -1);
#endif
	atomic64_dec(&zram->stats.pages_stored);
	zram_set_handle(zram, index, 0);
	zram_set_obj_size(zram, index, 0);
	WARN_ON_ONCE(zram->table[index].flags &
		~(1UL << ZRAM_LOCK | 1UL << ZRAM_UNDER_WB));
#if defined(CONFIG_ESWAP)
	if (new_memfusion)
		eswap_untrack_memcg_put(memcg);
#endif
}

/*
 * Reads (decompresses if needed) a page from zspool (zsmalloc).
 * Corresponding ZRAM slot should be locked.
 */
static int zram_read_from_zspool(struct zram *zram, struct page *page,
				 u32 index)
{
	struct zcomp_strm *zstrm;
	unsigned long handle;
	unsigned int size;
	void *src, *dst;
	u32 prio;
	int ret;

	handle = zram_get_handle(zram, index);
	if (!handle || zram_test_flag(zram, index, ZRAM_SAME)) {
		unsigned long value;
		void *mem;

		value = handle ? zram_get_element(zram, index) : 0;
		mem = kmap_atomic(page);
		zram_fill_page(mem, PAGE_SIZE, value);
		kunmap_atomic(mem);
		return 0;
	}

	size = zram_get_obj_size(zram, index);

	if (size != PAGE_SIZE) {
		prio = zram_get_priority(zram, index);
		zstrm = zcomp_stream_get(zram->comps[prio]);
	}

	src = zs_map_object(zram->mem_pool, handle, ZS_MM_RO);
	if (size == PAGE_SIZE) {
		dst = kmap_atomic(page);
		memcpy(dst, src, PAGE_SIZE);
		kunmap_atomic(dst);
		ret = 0;
	} else {
		dst = kmap_atomic(page);
		ret = zcomp_decompress(zstrm, src, size, dst);
		kunmap_atomic(dst);
		zcomp_stream_put(zram->comps[prio]);
	}
	zs_unmap_object(zram->mem_pool, handle);
	return ret;
}

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
static void zram_refault_time(struct zram *zram, u32 index, unsigned long zm_delta, unsigned int stay_time){
	unsigned long delta;

	delta = zm_delta + stay_time * 1000;
	if (delta <= ZRAM_REFAULT_10s)
	 atomic64_inc(&zram->stats.rf_lt_10);
	else if (delta <= ZRAM_REFAULT_60s)
	 atomic64_inc(&zram->stats.rf_lt_60);
	else if (delta <= ZRAM_REFAULT_300s)
	 atomic64_inc(&zram->stats.rf_lt_300);
	else if (delta <= ZRAM_REFAULT_1800s)
	 atomic64_inc(&zram->stats.rf_lt_1800);
	else if (delta <= ZRAM_REFAULT_3600s)
	 atomic64_inc(&zram->stats.rf_lt_3600);
	else if (delta <= ZRAM_REFAULT_43200s)
	 atomic64_inc(&zram->stats.rf_lt_43200);
	else if (delta <= ZRAM_REFAULT_86400s)
	 atomic64_inc(&zram->stats.rf_lt_86400);
	else
	 atomic64_inc(&zram->stats.rf_gt_86400);
}
#endif

static int zram_read_page(struct zram *zram, struct page *page, u32 index,
			  struct bio *parent)
{
	int ret;
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	ktime_t start, delta;
	unsigned long zm_delta;
	bool need_refault = false;
	bool delayacct, is_kern;
	bool is_appcompact = false;
	u32 prio = zram_get_priority(zram, index);

	delayacct = zram_delayacct_enable;
	if (delayacct)
		start = ktime_get_ns();
	if (prio != ZRAM_PRIMARY_COMP)
		trace_zram_read_page_start(prio);
#endif

	zram_slot_lock(zram, index);

#ifdef CONFIG_ESWAP
	/* should always allow fault in, because something in eswap need come back
	 * even eswap is turn off
	 */
	ret = eswap_fault_in(zram, index);
	if (unlikely(ret)) {
		pr_err("search in eswap failed! err=%d, page=%u\n", ret, index);
		zram_slot_unlock(zram, index);
		return ret;
	}
#endif

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	if (zram_test_flag(zram, index, ZRAM_APPCOMPACT)) {
		is_appcompact = true;
	}
	if (!zram_test_flag(zram, index, ZRAM_UNDER_WB)){
	    zm_delta = ktime_ms_delta(ktime_get_boottime(), zram->table[index].ac_time);
	    need_refault = true;
	}
#endif

	if (!zram_test_flag(zram, index, ZRAM_WB)) {
		/* Slot should be locked through out the function call */
		ret = zram_read_from_zspool(zram, page, index);
		zram_slot_unlock(zram, index);

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
		if (need_refault)
		    zram_refault_time(zram, index, zm_delta, 0);

		if (delayacct) {
			delta = ktime_get_ns() - start;
			if (delta > 0) {
				atomic64_add(delta, &zram->stats.decompress_delay[prio]);
				atomic64_inc(&zram->stats.decompress_pages[prio]);
			}
		}
		if (prio != ZRAM_PRIMARY_COMP)
			trace_zram_read_page_end(prio);
		if (is_appcompact) {
			atomic64_inc(&fwk_decompress_pages_since);
			update_fwk_thrashing_stat(false, index);
		}
#endif
	} else {
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
		is_kern = zram_test_flag(zram, index, ZRAM_COLD);
		unsigned int stay_time = zram->table[index].stay_time;
		unsigned long handle = zram_get_element(zram, index);
		int bd_index = zram_get_bd_index(zram, index);

		zram_set_flag(zram, index, ZRAM_READ_BDEV);
#endif
		/*
		 * The slot should be unlocked before reading from the backing
		 * device.
		 */
		zram_slot_unlock(zram, index);
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
		if (need_refault)
		    zram_refault_time(zram, index, zm_delta, stay_time);

		if (!zram_writeback_vmpressure)
			atomic64_set(&zram_wbd_ifbreak, 1);

		zram_kernel_trace_read_start(is_kern, stay_time);
		if (0 == (handle & AGGR_CPRS_SZ_MASK))
			ret = read_from_bdev(zram, page, index, parent);
		else
			ret = read_nodcprs_from_bdev(zram, page, bd_index, index, handle, parent);
		zram_kernel_trace_read_end(is_kern, stay_time);
#else
		ret = read_from_bdev(zram, page, zram_get_element(zram, index),
				     parent);
#endif
	}

	/* Should NEVER happen. Return bio error if it does. */
	if (WARN_ON(ret < 0))
		pr_err("Decompression failed! err=%d, page=%u\n", ret, index);

	return ret;
}

/*
 * Use a temporary buffer to decompress the page, as the decompressor
 * always expects a full page for the output.
 */
static int zram_bvec_read_partial(struct zram *zram, struct bio_vec *bvec,
				  u32 index, int offset)
{
	struct page *page = alloc_page(GFP_NOIO);
	int ret;

	if (!page)
		return -ENOMEM;
	ret = zram_read_page(zram, page, index, NULL);
	if (likely(!ret))
		memcpy_to_bvec(bvec, page_address(page) + offset);
	__free_page(page);
	return ret;
}

static int zram_bvec_read(struct zram *zram, struct bio_vec *bvec,
			  u32 index, int offset, struct bio *bio)
{
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	atomic64_inc(&zram->stats.num_reads);
#endif
	if (is_partial_io(bvec))
		return zram_bvec_read_partial(zram, bvec, index, offset);
	return zram_read_page(zram, bvec->bv_page, index, bio);
}

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
static void try_to_wakeup_zram_wbd(struct zram *zram, u32 index, struct page *page, bool is_key_task)
{
	if (atomic_read(&zram_wbd_switch) || zram_writeback_vmpressure) {
		if (skip_wakeup) {
			if (!time_is_after_jiffies64(time_stamp_wakeup)) {
				try_wakeup_zram_wbd_lite(zram, 0);
				atomic_set(&high_loading_cooling, 0);
				skip_wakeup = false;
			}
		} else {
			skip_wakeup = true;
			time_stamp_wakeup = jiffies_64 + zram_wbd_interval* HZ; //at least disable writeback for 2s
		}
	}

	if (is_log_enable(ZRAM_CGROUP_LOG)) {
		pr_err("[%s] index:%u keypages:%lld cg:%lx root:%lx page_is_key:%d\n",
			current->comm, index, atomic64_read(&zram->stats.key_pages),
			(long)folio_memcg(page_folio(page)), (long)root_mem_cgroup, is_key_task);
	}
}

static bool current_is_key_task(struct task_struct *task)
{
	/* kswapd should be considered as key task if there are throttled direct reclaimers */
	if (current_is_kswapd() && waitqueue_active(&(NODE_DATA(0)->pfmemalloc_wait)))
		return true;

	/* The memory allocation stalls of kernel threads indirectly throttle for user tasks */
	//if (current->flags & PF_KTHREAD)
	if (current_is_kswapd())
		return false;
	
	/* skip app compaction case */
	if (task == READ_ONCE(zram_compressed_task))
		return false;

	/* RT task */
	if (rt_task(task))
		return true;

	/* UX task */
#if IS_ENABLED(CONFIG_TRANS_SCHED_FEATURE)
	if (g_trans_sched_interface.check_ux_task && g_trans_sched_interface.check_ux_task(task))
		return true;
#endif

	/* Top-app or foreground task */
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	struct cgroup *grp;

	rcu_read_lock();
	grp = task_cgroup(task, cpu_cgrp_id);
	rcu_read_unlock();

	if (grp->kn->name && (!strcmp("top-app", grp->kn->name) || !strcmp("foreground", grp->kn->name)))
		return true;
#endif

	return is_critical(task);
}

static bool zram_should_switch_alg(struct zram *zram)
{
	if (!g_muticomp_enable || !zram->comp_algs[ZRAM_SECONDARY_COMP] || !current_is_key_task(current))
		return false;

	/*
	 * Stop switching to secondary algorithm if zram compression ratio is low
	 * due to insufficient recompression opportunities.
	 */
	u64 orig_size = atomic64_read(&zram->stats.pages_stored) << PAGE_SHIFT;
	u64 comp_size = atomic64_read(&zram->stats.compr_data_size);

	if (orig_size * zram_comp_ratio_limit < comp_size * 100) {
		atomic64_inc(&zram->stats.skip_switch_alg_pages);
		return false;
	}

	return true;
}
#endif

static int zram_write_page(struct zram *zram, struct page *page, u32 index)
{
	int ret = 0;
	unsigned long alloced_pages;
	unsigned long handle = -ENOMEM;
	unsigned int comp_len = 0;
	void *src, *dst, *mem;
	struct zcomp_strm *zstrm;
	unsigned long element = 0;
	enum zram_pageflags flags = 0;
#ifdef CONFIG_ESWAP
	struct mem_cgroup *memcg = page_eswap_memcg(page);
	struct mem_cgroup *my_memcg = page_memcg(page);
#endif
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	ktime_t start, delta;
	bool delayacct = zram_delayacct_enable;
	bool is_key_task = page_is_key(zram, page, index);
	bool is_app_compact = false;
#ifdef CONFIG_ZRAM_MULTI_COMP
	u32 prio = zram_should_switch_alg(zram) ? ZRAM_SECONDARY_COMP : ZRAM_PRIMARY_COMP;
#else
	u32 prio = ZRAM_PRIMARY_COMP;
#endif

	if (delayacct)
		start = ktime_get_ns();
#endif

	mem = kmap_atomic(page);
	if (page_same_filled(mem, &element)) {
		kunmap_atomic(mem);
		/* Free memory associated with this sector now. */
		flags = ZRAM_SAME;
		atomic64_inc(&zram->stats.same_pages);

#ifdef CONFIG_ESWAP
		if (is_memcg_compact_v2 && memcg) {
			atomic64_inc(&MEMCG_ESTRACK_FIELD(memcg, same_pages));
			memcg_inc_samepages_counter(my_memcg);
		}
#endif
		goto out;
	}
	kunmap_atomic(mem);

compress_again:
	zstrm = zcomp_stream_get(zram->comps[prio]);
	src = kmap_atomic(page);
	ret = zcomp_compress(zstrm, src, &comp_len);
	kunmap_atomic(src);

	if (unlikely(ret)) {
		zcomp_stream_put(zram->comps[prio]);
		pr_err("Compression failed! err=%d\n", ret);
		zs_free(zram->mem_pool, handle);
		return ret;
	}

	if (comp_len >= huge_class_size)
		comp_len = PAGE_SIZE;
	/*
	 * handle allocation has 2 paths:
	 * a) fast path is executed with preemption disabled (for
	 *  per-cpu streams) and has __GFP_DIRECT_RECLAIM bit clear,
	 *  since we can't sleep;
	 * b) slow path enables preemption and attempts to allocate
	 *  the page with __GFP_DIRECT_RECLAIM bit set. we have to
	 *  put per-cpu compression stream and, thus, to re-do
	 *  the compression once handle is allocated.
	 *
	 * if we have a 'non-null' handle here then we are coming
	 * from the slow path and handle has already been allocated.
	 */
	if (IS_ERR_VALUE(handle))
		handle = zs_malloc(zram->mem_pool, comp_len,
				__GFP_KSWAPD_RECLAIM |
				__GFP_NOWARN |
				__GFP_HIGHMEM |
				__GFP_MOVABLE |
				__GFP_CMA);
	if (IS_ERR_VALUE(handle)) {
		zcomp_stream_put(zram->comps[prio]);
		atomic64_inc(&zram->stats.writestall);
		handle = zs_malloc(zram->mem_pool, comp_len,
				GFP_NOIO | __GFP_HIGHMEM |
				__GFP_MOVABLE | __GFP_CMA);
		if (IS_ERR_VALUE(handle))
			return PTR_ERR((void *)handle);

		if (comp_len != PAGE_SIZE)
			goto compress_again;
		/*
		 * If the page is not compressible, you need to acquire the
		 * lock and execute the code below. The zcomp_stream_get()
		 * call is needed to disable the cpu hotplug and grab the
		 * zstrm buffer back. It is necessary that the dereferencing
		 * of the zstrm variable below occurs correctly.
		 */
		zstrm = zcomp_stream_get(zram->comps[prio]);
	}

	alloced_pages = zs_get_total_pages(zram->mem_pool);
	update_used_max(zram, alloced_pages);

	if (zram->limit_pages && alloced_pages > zram->limit_pages) {
		zcomp_stream_put(zram->comps[prio]);
		zs_free(zram->mem_pool, handle);
		return -ENOMEM;
	}

	dst = zs_map_object(zram->mem_pool, handle, ZS_MM_WO);

	src = zstrm->buffer;
	if (comp_len == PAGE_SIZE)
		src = kmap_atomic(page);
	memcpy(dst, src, comp_len);
	if (comp_len == PAGE_SIZE)
		kunmap_atomic(src);

	zcomp_stream_put(zram->comps[prio]);
	zs_unmap_object(zram->mem_pool, handle);
	atomic64_add(comp_len, &zram->stats.compr_data_size);
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	atomic64_inc(&zram->stats.meta_data_size);
#endif
out:
	/*
	 * Free memory associated with this sector
	 * before overwriting unused sectors.
	 */
	zram_slot_lock(zram, index);
	zram_free_page(zram, index);

	if (comp_len == PAGE_SIZE) {
		zram_set_flag(zram, index, ZRAM_HUGE);
		atomic64_inc(&zram->stats.huge_pages);
		atomic64_inc(&zram->stats.huge_pages_since);
#ifdef CONFIG_ESWAP
		if (is_memcg_compact_v2 && memcg) {
			atomic64_inc(&MEMCG_ESTRACK_FIELD(memcg, huge_pages));
		}
#endif
	}

	if (flags) {
		zram_set_flag(zram, index, flags);
		zram_set_element(zram, index, element);
	}  else {
		zram_set_handle(zram, index, handle);
		zram_set_obj_size(zram, index, comp_len);
		zram_set_priority(zram, index, prio);
	}

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	if (current == READ_ONCE(zram_compressed_task)) {
		zram_set_flag(zram, index, ZRAM_APPCOMPACT);
		is_app_compact = true;
	}
#endif

#ifdef CONFIG_ESWAP
	if (is_memcg_compact_v2 && !is_key_task) {
		eswap_track(zram, index, memcg, my_memcg);
		try_to_wakeup_zram_wbd(zram, index, page, is_key_task);
	}
#endif
	zram_slot_unlock(zram, index);

	/* Update stats */
	atomic64_inc(&zram->stats.pages_stored);

//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 20250113 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	if (is_app_compact) {
		atomic64_inc(&fwk_compress_pages_since);
		update_fwk_thrashing_stat(true, index);
	}

	if (!is_memcg_compact_v2 && is_memfusion && (ZRAM_SAME != flags) && !is_key_task) {
		zram_list_add_to_tail(&zram->table[index], &zram->wbd_klist, zram);
		try_to_wakeup_zram_wbd(zram, index, page, is_key_task);
	}

#ifdef CONFIG_ZRAM_MULTI_COMP
	/* Update stats for WKDM-compressed pages */
	if (prio == ZRAM_SECONDARY_COMP) {
		atomic64_inc(&zram->stats.wkdm_pages);
		atomic64_inc(&zram->stats.wkdm_compr_pages_since);
		atomic64_add(comp_len, &zram->stats.wkdm_compr_data_size);
	} else {
		atomic64_inc(&zram->stats.lz4_compr_pages_since);
		atomic64_add(comp_len, &zram->stats.lz4_compr_data_size);
	}
#endif

	if (delayacct) {
		delta = ktime_get_ns() - start;
		if (delta > 0) {
			atomic64_add(delta, &zram->stats.compress_delay[prio]);
			atomic64_inc(&zram->stats.compress_pages[prio]);
		}
	}
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by dongyun.liu at 20250113 end
	return ret;
}

/*
 * This is a partial IO. Read the full page before writing the changes.
 */
static int zram_bvec_write_partial(struct zram *zram, struct bio_vec *bvec,
				   u32 index, int offset, struct bio *bio)
{
	struct page *page = alloc_page(GFP_NOIO);
	int ret;

	if (!page)
		return -ENOMEM;

	ret = zram_read_page(zram, page, index, bio);
	if (!ret) {
		memcpy_from_bvec(page_address(page) + offset, bvec);
		ret = zram_write_page(zram, page, index);
	}
	__free_page(page);
	return ret;
}

static int zram_bvec_write(struct zram *zram, struct bio_vec *bvec,
			   u32 index, int offset, struct bio *bio)
{
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	atomic64_inc(&zram->stats.num_writes);
#endif
	if (is_partial_io(bvec))
		return zram_bvec_write_partial(zram, bvec, index, offset, bio);
	return zram_write_page(zram, bvec->bv_page, index);
}

#ifdef CONFIG_ZRAM_MULTI_COMP
/*
 * This function will decompress (unless it's ZRAM_HUGE) the page and then
 * attempt to compress it using provided compression algorithm priority
 * (which is potentially more effective).
 *
 * Corresponding ZRAM slot should be locked.
 */
static int zram_recompress(struct zram *zram, u32 index, struct page *page,
			   u32 threshold, u32 prio, u32 prio_max)
{
	struct zcomp_strm *zstrm = NULL;
	unsigned long handle_old;
	unsigned long handle_new;
	unsigned int comp_len_old;
	unsigned int comp_len_new;
	unsigned int class_index_old;
	unsigned int class_index_new;
	u32 num_recomps = 0;
	void *src, *dst;
	int ret;

	handle_old = zram_get_handle(zram, index);
	if (!handle_old)
		return -EINVAL;

	comp_len_old = zram_get_obj_size(zram, index);
	/*
	 * Do not recompress objects that are already "small enough".
	 */
	if (comp_len_old < threshold)
		return 0;

	ret = zram_read_from_zspool(zram, page, index);
	if (ret)
		return ret;

	/*
	 * We touched this entry so mark it as non-IDLE. This makes sure that
	 * we don't preserve IDLE flag and don't incorrectly pick this entry
	 * for different post-processing type (e.g. writeback).
	 */
	zram_clear_flag(zram, index, ZRAM_IDLE);

	class_index_old = zs_lookup_class_index(zram->mem_pool, comp_len_old);
	/*
	 * Iterate the secondary comp algorithms list (in order of priority)
	 * and try to recompress the page.
	 */
	for (; prio < prio_max; prio++) {
		if (!zram->comps[prio])
			continue;

		/*
		 * Skip if the object is already re-compressed with a higher
		 * priority algorithm (or same algorithm).
		 */
		if (prio <= zram_get_priority(zram, index))
			continue;

		num_recomps++;
		zstrm = zcomp_stream_get(zram->comps[prio]);
		src = kmap_atomic(page);
		ret = zcomp_compress(zstrm, src, &comp_len_new);
		kunmap_atomic(src);

		if (ret) {
			zcomp_stream_put(zram->comps[prio]);
			return ret;
		}

		class_index_new = zs_lookup_class_index(zram->mem_pool,
							comp_len_new);

		/* Continue until we make progress */
		if (class_index_new >= class_index_old ||
		    (threshold && comp_len_new >= threshold)) {
			zcomp_stream_put(zram->comps[prio]);
			continue;
		}

		/* Recompression was successful so break out */
		break;
	}

	/*
	 * We did not try to recompress, e.g. when we have only one
	 * secondary algorithm and the page is already recompressed
	 * using that algorithm
	 */
	if (!zstrm)
		return 0;

	if (class_index_new >= class_index_old) {
		/*
		 * Secondary algorithms failed to re-compress the page
		 * in a way that would save memory, mark the object as
		 * incompressible so that we will not try to compress
		 * it again.
		 *
		 * We need to make sure that all secondary algorithms have
		 * failed, so we test if the number of recompressions matches
		 * the number of active secondary algorithms.
		 */
		if (num_recomps == zram->num_active_comps - 1) {
			zram_set_flag(zram, index, ZRAM_INCOMPRESSIBLE);
			atomic64_inc(&zram->stats.incompressible_pages);
			atomic64_inc(&zram->stats.incompressible_pages_since);
		}
		return 0;
	}

	/* Successful recompression but above threshold */
	if (threshold && comp_len_new >= threshold)
		return 0;

	/*
	 * No direct reclaim (slow path) for handle allocation and no
	 * re-compression attempt (unlike in zram_write_bvec()) since
	 * we already have stored that object in zsmalloc. If we cannot
	 * alloc memory for recompressed object then we bail out and
	 * simply keep the old (existing) object in zsmalloc.
	 */
	handle_new = zs_malloc(zram->mem_pool, comp_len_new,
			       __GFP_KSWAPD_RECLAIM |
			       __GFP_NOWARN |
			       __GFP_HIGHMEM |
			       __GFP_MOVABLE);
	if (IS_ERR_VALUE(handle_new)) {
		zcomp_stream_put(zram->comps[prio]);
		return PTR_ERR((void *)handle_new);
	}

	dst = zs_map_object(zram->mem_pool, handle_new, ZS_MM_WO);
	memcpy(dst, zstrm->buffer, comp_len_new);
	zcomp_stream_put(zram->comps[prio]);

	zs_unmap_object(zram->mem_pool, handle_new);

	zram_free_page(zram, index);
	zram_set_handle(zram, index, handle_new);
	zram_set_obj_size(zram, index, comp_len_new);
	zram_set_priority(zram, index, prio);

	atomic64_add(comp_len_new, &zram->stats.compr_data_size);
	atomic64_inc(&zram->stats.pages_stored);

	return 0;
}

#define RECOMPRESS_IDLE		(1 << 0)
#define RECOMPRESS_HUGE		(1 << 1)

static ssize_t recompress_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	u32 prio = ZRAM_SECONDARY_COMP, prio_max = ZRAM_MAX_COMPS;
	struct zram *zram = dev_to_zram(dev);
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	char *args, *param, *val, *algo = NULL;
	u32 mode = 0, threshold = 0;
	unsigned long index;
	struct page *page;
	ssize_t ret;

	args = skip_spaces(buf);
	while (*args) {
		args = next_arg(args, &param, &val);

		if (!val || !*val)
			return -EINVAL;

		if (!strcmp(param, "type")) {
			if (!strcmp(val, "idle"))
				mode = RECOMPRESS_IDLE;
			if (!strcmp(val, "huge"))
				mode = RECOMPRESS_HUGE;
			if (!strcmp(val, "huge_idle"))
				mode = RECOMPRESS_IDLE | RECOMPRESS_HUGE;
			continue;
		}

		if (!strcmp(param, "threshold")) {
			/*
			 * We will re-compress only idle objects equal or
			 * greater in size than watermark.
			 */
			ret = kstrtouint(val, 10, &threshold);
			if (ret)
				return ret;
			continue;
		}

		if (!strcmp(param, "algo")) {
			algo = val;
			continue;
		}
	}

	if (threshold >= huge_class_size)
		return -EINVAL;

	down_read(&zram->init_lock);
	if (!init_done(zram)) {
		ret = -EINVAL;
		goto release_init_lock;
	}

	if (algo) {
		bool found = false;

		for (; prio < ZRAM_MAX_COMPS; prio++) {
			if (!zram->comp_algs[prio])
				continue;

			if (!strcmp(zram->comp_algs[prio], algo)) {
				prio_max = min(prio + 1, ZRAM_MAX_COMPS);
				found = true;
				break;
			}
		}

		if (!found) {
			ret = -EINVAL;
			goto release_init_lock;
		}
	}

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		ret = -ENOMEM;
		goto release_init_lock;
	}

	ret = len;
	for (index = 0; index < nr_pages; index++) {
		int err = 0;

		zram_slot_lock(zram, index);

		if (!zram_allocated(zram, index))
			goto next;

		if (mode & RECOMPRESS_IDLE &&
		    !zram_test_flag(zram, index, ZRAM_IDLE))
			goto next;

		if (mode & RECOMPRESS_HUGE &&
		    !zram_test_flag(zram, index, ZRAM_HUGE))
			goto next;

		if (zram_test_flag(zram, index, ZRAM_WB) ||
		    zram_test_flag(zram, index, ZRAM_UNDER_WB) ||
		    zram_test_flag(zram, index, ZRAM_SAME) ||
		    zram_test_flag(zram, index, ZRAM_INCOMPRESSIBLE))
			goto next;

		err = zram_recompress(zram, index, page, threshold,
				      prio, prio_max);
next:
		zram_slot_unlock(zram, index);
		if (err) {
			ret = err;
			break;
		}

		cond_resched();
	}

	__free_page(page);

release_init_lock:
	up_read(&zram->init_lock);
	return ret;
}
#endif

static void zram_bio_discard(struct zram *zram, struct bio *bio)
{
	size_t n = bio->bi_iter.bi_size;
	u32 index = bio->bi_iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
	u32 offset = (bio->bi_iter.bi_sector & (SECTORS_PER_PAGE - 1)) <<
			SECTOR_SHIFT;

	/*
	 * zram manages data in physical block size units. Because logical block
	 * size isn't identical with physical block size on some arch, we
	 * could get a discard request pointing to a specific offset within a
	 * certain physical block.  Although we can handle this request by
	 * reading that physiclal block and decompressing and partially zeroing
	 * and re-compressing and then re-storing it, this isn't reasonable
	 * because our intent with a discard request is to save memory.  So
	 * skipping this logical block is appropriate here.
	 */
	if (offset) {
		if (n <= (PAGE_SIZE - offset))
			return;

		n -= (PAGE_SIZE - offset);
		index++;
	}

	while (n >= PAGE_SIZE) {
		zram_slot_lock(zram, index);
		zram_free_page(zram, index);
		zram_slot_unlock(zram, index);
		atomic64_inc(&zram->stats.notify_free);
		index++;
		n -= PAGE_SIZE;
	}

	bio_endio(bio);
}

static void zram_bio_read(struct zram *zram, struct bio *bio)
{
	unsigned long start_time = bio_start_io_acct(bio);
	struct bvec_iter iter = bio->bi_iter;

	do {
		u32 index = iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
		u32 offset = (iter.bi_sector & (SECTORS_PER_PAGE - 1)) <<
				SECTOR_SHIFT;
		struct bio_vec bv = bio_iter_iovec(bio, iter);

		bv.bv_len = min_t(u32, bv.bv_len, PAGE_SIZE - offset);

		if (zram_bvec_read(zram, &bv, index, offset, bio) < 0) {
			atomic64_inc(&zram->stats.failed_reads);
			bio->bi_status = BLK_STS_IOERR;
			break;
		}
		flush_dcache_page(bv.bv_page);

		zram_slot_lock(zram, index);
		zram_accessed(zram, index);
		zram_slot_unlock(zram, index);

		bio_advance_iter_single(bio, &iter, bv.bv_len);
	} while (iter.bi_size);

	bio_end_io_acct(bio, start_time);
	bio_endio(bio);
}

static void zram_bio_write(struct zram *zram, struct bio *bio)
{
	unsigned long start_time = bio_start_io_acct(bio);
	struct bvec_iter iter = bio->bi_iter;

	do {
		u32 index = iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
		u32 offset = (iter.bi_sector & (SECTORS_PER_PAGE - 1)) <<
				SECTOR_SHIFT;
		struct bio_vec bv = bio_iter_iovec(bio, iter);

		bv.bv_len = min_t(u32, bv.bv_len, PAGE_SIZE - offset);

		if (zram_bvec_write(zram, &bv, index, offset, bio) < 0) {
			atomic64_inc(&zram->stats.failed_writes);
			bio->bi_status = BLK_STS_IOERR;
			break;
		}

		zram_slot_lock(zram, index);
		zram_accessed(zram, index);
		zram_slot_unlock(zram, index);

		bio_advance_iter_single(bio, &iter, bv.bv_len);
	} while (iter.bi_size);

	bio_end_io_acct(bio, start_time);
	bio_endio(bio);
}

/*
 * Handler function for all zram I/O requests.
 */
static void zram_submit_bio(struct bio *bio)
{
	struct zram *zram = bio->bi_bdev->bd_disk->private_data;

	switch (bio_op(bio)) {
	case REQ_OP_READ:
		zram_bio_read(zram, bio);
		break;
	case REQ_OP_WRITE:
		zram_bio_write(zram, bio);
		break;
	case REQ_OP_DISCARD:
	case REQ_OP_WRITE_ZEROES:
		zram_bio_discard(zram, bio);
		break;
	default:
		WARN_ON_ONCE(1);
		bio_endio(bio);
	}
}

static void zram_slot_free_notify(struct block_device *bdev,
				unsigned long index)
{
	struct zram *zram;

	zram = bdev->bd_disk->private_data;

	atomic64_inc(&zram->stats.notify_free);
	if (!zram_slot_trylock(zram, index)) {
		atomic64_inc(&zram->stats.miss_free);
		return;
	}

#ifdef CONFIG_ESWAP
	if (is_memcg_compact_v2 && !eswap_delete(zram, index)) {
		zram_slot_unlock(zram, index);
		atomic64_inc(&zram->stats.miss_free);
		return;
	}
#endif
	zram_free_page(zram, index);
	zram_slot_unlock(zram, index);
}

static void zram_destroy_comps(struct zram *zram)
{
	u32 prio;

	for (prio = 0; prio < ZRAM_MAX_COMPS; prio++) {
		struct zcomp *comp = zram->comps[prio];

		zram->comps[prio] = NULL;
		if (!comp)
			continue;
		zcomp_destroy(comp);
		zram->num_active_comps--;
	}

	for (prio = ZRAM_PRIMARY_COMP; prio < ZRAM_MAX_COMPS; prio++) {
		/* Do not free statically defined compression algorithms */
		if (zram->comp_algs[prio] != default_compressor)
			kfree(zram->comp_algs[prio]);
		zram->comp_algs[prio] = NULL;
	}
}

static void zram_reset_device(struct zram *zram)
{
	down_write(&zram->init_lock);

	zram->limit_pages = 0;

	set_capacity_and_notify(zram->disk, 0);
	part_stat_set_all(zram->disk->part0, 0);

	/* I/O operation under all of CPU are done so let's free */
	zram_meta_free(zram, zram->disksize);
	zram->disksize = 0;
	zram_destroy_comps(zram);
	memset(&zram->stats, 0, sizeof(zram->stats));
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	if (is_memfusion)
		zram_reset_segload_bdev(zram);
#endif
	reset_bdev(zram);
	comp_algorithm_set(zram, ZRAM_PRIMARY_COMP, default_compressor);
	up_write(&zram->init_lock);
#ifdef CONFIG_ZRAM_MULTI_COMP
	if (zram->recompd) {
		kthread_stop(zram->recompd);
		zram->recompd = NULL;
	}
#endif
}

#ifdef CONFIG_ZRAM_MULTI_COMP
static inline int zram_recompress_page(struct zram *zram, u32 index, struct page *page, u32 prio, bool recomp_huge_page)
{
	struct zcomp_strm *zstrm;
	unsigned long handle_old;
	unsigned long handle_new;
	unsigned int comp_len_old;
	unsigned int comp_len_new;
	unsigned int class_index_old;
	unsigned int class_index_new;
	void *src, *dst;
	int ret;
	bool delayacct = zram_delayacct_enable;
	ktime_t start, delta;

	if (prio >= zram->num_active_comps || !zram->comps[prio])
		return -EINVAL;

	handle_old = zram_get_handle(zram, index);
	if (!handle_old)
		return -EINVAL;

	/* Skip if the object is already recompressed with the same priority algorithm. */
	if (prio == zram_get_priority(zram, index))
		return -EINVAL;

	if (delayacct)
		start = ktime_get_ns();

	ret = zram_read_from_zspool(zram, page, index);
	if (ret)
		return ret;

	zram_clear_flag(zram, index, ZRAM_IDLE);
	comp_len_old = zram_get_obj_size(zram, index);
	class_index_old = zs_lookup_class_index(zram->mem_pool, comp_len_old);
	zstrm = zcomp_stream_get(zram->comps[prio]);
	src = kmap_local_page(page);
	ret = zcomp_compress(zstrm, src, &comp_len_new);
	kunmap_local(src);
	/* Compression error */
	if (ret) {
		zram_set_flag(zram, index, ZRAM_INCOMPRESSIBLE);
		atomic64_inc(&zram->stats.incompressible_pages);
		atomic64_inc(&zram->stats.incompressible_pages_since);
		zcomp_stream_put(zram->comps[prio]);
		return ret;
	}

	class_index_new = zs_lookup_class_index(zram->mem_pool, comp_len_new);
	if (recomp_huge_page && class_index_new >= class_index_old) {
		/*
		 * Secondary algorithms failed to recompress the huge page
		 * in a way that would save memory, mark the object as
		 * incompressible so that we will not try to compress
		 * it again.
		 */
		zram_set_flag(zram, index, ZRAM_INCOMPRESSIBLE);
		atomic64_inc(&zram->stats.incompressible_pages);
		atomic64_inc(&zram->stats.incompressible_pages_since);
		zcomp_stream_put(zram->comps[prio]);
		return 0;
	}

	/*
	 * No direct reclaim (slow path) for handle allocation and no
	 * re-compression attempt (unlike in zram_write_bvec()) since
	 * we already have stored that object in zsmalloc. If we cannot
	 * alloc memory for recompressed object then we bail out and
	 * simply keep the old (existing) object in zsmalloc.
	 */
	handle_new = zs_malloc(zram->mem_pool, comp_len_new,
			       __GFP_KSWAPD_RECLAIM |
			       __GFP_NOWARN |
			       __GFP_HIGHMEM |
			       __GFP_MOVABLE);
	if (IS_ERR_VALUE(handle_new)) {
		zcomp_stream_put(zram->comps[prio]);
		return PTR_ERR((void *)handle_new);
	}

	dst = zs_map_object(zram->mem_pool, handle_new, ZS_MM_WO);
	memcpy(dst, zstrm->buffer, comp_len_new);
	zcomp_stream_put(zram->comps[prio]);

	zs_unmap_object(zram->mem_pool, handle_new);

	zram_free_page(zram, index);
	zram_set_handle(zram, index, handle_new);
	zram_set_obj_size(zram, index, comp_len_new);
	zram_set_priority(zram, index, prio);

	atomic64_add(comp_len_new, &zram->stats.compr_data_size);
	atomic64_inc(&zram->stats.pages_stored);
	if (delayacct) {
		delta = ktime_get_ns() - start;
		if (delta > 0) {
			atomic64_add(delta, &zram->stats.recompress_delay[prio]);
			atomic64_inc(&zram->stats.recompress_pages[prio]);
		}
	}

	return 0;
}

/* Get the number of pages that can be recompressed. */
static inline u64 zram_get_recomp_pages(struct zram *zram)
{
	return atomic64_read(&zram->stats.wkdm_pages) + atomic64_read(&zram->stats.huge_pages);
}

static bool zram_should_recompress(struct zram *zram)
{
	struct kernel_cpustat curr_cpustat;

	if (unlikely(!g_muticomp_enable))
		return false;

	/* skip recompression if there are not enough recompressible pages */
	if (zram_get_recomp_pages(zram) - atomic64_read(&zram->recomp_pages) < zram_recomp_min_pages)
		return false;

	get_all_cpustat(&curr_cpustat);

	return compare_cpu_stat(&curr_cpustat, &zram->recomp_cstat, true);
}

static int zram_recompress_kthread(void *p) {
	struct zram *zram = (struct zram *)p;
	struct page *page;
	u64 index, nr_pages;

	if (!init_done(zram))
		return -EINVAL;

	page = alloc_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;
	set_page_private(page, (unsigned long)zram);

	while (!kthread_should_stop()) {
		get_all_cpustat(&zram->recomp_cstat);
		msleep_interruptible(zram_recomp_interval * 1000);

		if (!zram_should_recompress(zram))
			continue;

		down_read(&zram->init_lock);
		nr_pages = zram->disksize >> PAGE_SHIFT;
		for (index = 0; index < nr_pages; index++) {
			int err = 0;
			u32 prio;

			zram_slot_lock(zram, index);
			if (!zram_allocated(zram, index))
				goto next;

			if (zram_test_flag(zram, index, ZRAM_WB) ||
				zram_test_flag(zram, index, ZRAM_UNDER_WB) ||
				zram_test_flag(zram, index, ZRAM_SAME) ||
				zram_test_flag(zram, index, ZRAM_INCOMPRESSIBLE))
				goto next;

			prio = zram_get_priority(zram, index);
			if (prio == ZRAM_SECONDARY_COMP) /* Recompress WKDM-Compressed page by lz4 */
				err = zram_recompress_page(zram, index, page, ZRAM_PRIMARY_COMP, false);
			else if (zram_test_flag(zram, index, ZRAM_HUGE)) /* Recompress huge page by zstd */
				err = zram_recompress_page(zram, index, page, ZRAM_TERTIARY_COMP, true);
next:
			zram_slot_unlock(zram, index);

			cond_resched();
		}
		atomic64_set(&zram->recomp_pages, zram_get_recomp_pages(zram));
		up_read(&zram->init_lock);
	}
	__free_page(page);

	return 0;
}
#endif

static ssize_t disksize_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	u64 disksize;
	struct zcomp *comp;
	struct zram *zram = dev_to_zram(dev);
	int err;
	u32 prio;
#ifdef CONFIG_ZRAM_MULTI_COMP
	struct cpumask mask = {0x7}; //default bind cpu:0-2
	struct sched_param param = {  .sched_priority = SCHED_IDLE };
#endif

	disksize = memparse(buf, NULL);
	if (!disksize)
		return -EINVAL;

	down_write(&zram->init_lock);
	if (init_done(zram)) {
		pr_info("Cannot change disksize for initialized device\n");
		err = -EBUSY;
		goto out_unlock;
	}

	disksize = PAGE_ALIGN(disksize);
	if (!zram_meta_alloc(zram, disksize)) {
		err = -ENOMEM;
		goto out_unlock;
	}

	for (prio = 0; prio < ZRAM_MAX_COMPS; prio++) {
		if (!zram->comp_algs[prio])
			continue;

		comp = zcomp_create(zram->comp_algs[prio]);
		if (IS_ERR(comp)) {
			pr_err("Cannot initialise %s compressing backend\n",
			       zram->comp_algs[prio]);
			err = PTR_ERR(comp);
			goto out_free_comps;
		}

		zram->comps[prio] = comp;
		zram->num_active_comps++;
	}
	zram->disksize = disksize;
	set_capacity_and_notify(zram->disk, zram->disksize >> SECTOR_SHIFT);
#ifdef CONFIG_ZRAM_MULTI_COMP
	if (g_muticomp_enable && zram->num_active_comps > 1) {
		zram->recompd = kthread_run(zram_recompress_kthread, zram, "%s_recompd", zram->disk->disk_name);
		if (IS_ERR(zram->recompd)) {
			err = PTR_ERR(zram->recompd);
			goto out_free_comps;
		}

		if (set_cpus_allowed_ptr(zram->recompd, &mask)) {
			err = -EINVAL;
			kthread_stop(zram->recompd);
			zram->recompd = NULL;
			goto out_free_comps;
		}
		sched_setscheduler(zram->recompd, SCHED_IDLE, &param);
	}
#endif
	up_write(&zram->init_lock);
#ifdef CONFIG_ESWAP
	if (is_memcg_compact_v2)
		eswap_init(zram);
#endif

	return len;

out_free_comps:
	zram_destroy_comps(zram);
	zram_meta_free(zram, disksize);
out_unlock:
	up_write(&zram->init_lock);
	return err;
}

static ssize_t reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int ret;
	unsigned short do_reset;
	struct zram *zram;
	struct gendisk *disk;

	ret = kstrtou16(buf, 10, &do_reset);
	if (ret)
		return ret;

	if (!do_reset)
		return -EINVAL;

	zram = dev_to_zram(dev);
	disk = zram->disk;

	mutex_lock(&disk->open_mutex);
	/* Do not reset an active device or claimed device */
	if (disk_openers(disk) || zram->claim) {
		mutex_unlock(&disk->open_mutex);
		return -EBUSY;
	}

	/* From now on, anyone can't open /dev/zram[0-9] */
	zram->claim = true;
	mutex_unlock(&disk->open_mutex);
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	if (is_memfusion)
		deinit_lru_writeback(zram);
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end
	/* Make sure all the pending I/O are finished */
	sync_blockdev(disk->part0);
	zram_reset_device(zram);

	mutex_lock(&disk->open_mutex);
	zram->claim = false;
	mutex_unlock(&disk->open_mutex);

	return len;
}

static int zram_open(struct gendisk *disk, blk_mode_t mode)
{
	struct zram *zram = disk->private_data;

	WARN_ON(!mutex_is_locked(&disk->open_mutex));

	/* zram was claimed to reset so open request fails */
	if (zram->claim)
		return -EBUSY;
	return 0;
}

static const struct block_device_operations zram_devops = {
	.open = zram_open,
	.submit_bio = zram_submit_bio,
	.swap_slot_free_notify = zram_slot_free_notify,
	.owner = THIS_MODULE
};

static DEVICE_ATTR_WO(compact);
static DEVICE_ATTR_RW(disksize);
static DEVICE_ATTR_RO(initstate);
static DEVICE_ATTR_WO(reset);
static DEVICE_ATTR_WO(mem_limit);
static DEVICE_ATTR_WO(mem_used_max);
static DEVICE_ATTR_WO(idle);
static DEVICE_ATTR_RW(max_comp_streams);
static DEVICE_ATTR_RW(comp_algorithm);
#ifdef CONFIG_ZRAM_WRITEBACK
static DEVICE_ATTR_RW(backing_dev);
static DEVICE_ATTR_WO(writeback);
static DEVICE_ATTR_RW(writeback_limit);
static DEVICE_ATTR_RW(writeback_limit_enable);
#endif
#ifdef CONFIG_ZRAM_MULTI_COMP
static DEVICE_ATTR_RW(recomp_algorithm);
static DEVICE_ATTR_WO(recompress);
#endif

#ifdef CONFIG_ESWAP
static DEVICE_ATTR_RW(eswap_enable);
static DEVICE_ATTR_RW(eswap_online);
static DEVICE_ATTR_RW(eswap_extra_online);
#endif

static struct attribute *zram_disk_attrs[] = {
	&dev_attr_disksize.attr,
	&dev_attr_initstate.attr,
	&dev_attr_reset.attr,
	&dev_attr_compact.attr,
	&dev_attr_mem_limit.attr,
	&dev_attr_mem_used_max.attr,
	&dev_attr_idle.attr,
	&dev_attr_max_comp_streams.attr,
	&dev_attr_comp_algorithm.attr,
#ifdef CONFIG_ZRAM_WRITEBACK
	&dev_attr_backing_dev.attr,
	&dev_attr_writeback.attr,
	&dev_attr_writeback_limit.attr,
	&dev_attr_writeback_limit_enable.attr,
#endif
	&dev_attr_io_stat.attr,
	&dev_attr_mm_stat.attr,
#ifdef CONFIG_ZRAM_WRITEBACK
	&dev_attr_bd_stat.attr,
#endif
	&dev_attr_debug_stat.attr,
#ifdef CONFIG_ZRAM_MULTI_COMP
	&dev_attr_recomp_algorithm.attr,
	&dev_attr_recompress.attr,
#endif
#ifdef CONFIG_ESWAP
	&dev_attr_eswap_enable.attr,
	&dev_attr_eswap_online.attr,
	&dev_attr_eswap_extra_online.attr,
#endif
	NULL,
};

ATTRIBUTE_GROUPS(zram_disk);

/*
 * Allocate and initialize new zram device. the function returns
 * '>= 0' device_id upon success, and negative value otherwise.
 */
static int zram_add(void)
{
	struct zram *zram;
	int ret, device_id;

	zram = kzalloc(sizeof(struct zram), GFP_KERNEL);
	if (!zram)
		return -ENOMEM;

	ret = idr_alloc(&zram_index_idr, zram, 0, 0, GFP_KERNEL);
	if (ret < 0)
		goto out_free_dev;
	device_id = ret;

	init_rwsem(&zram->init_lock);
#ifdef CONFIG_ZRAM_WRITEBACK
	spin_lock_init(&zram->wb_limit_lock);
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	//if (memfusion_version() == 1) {
		pr_info(MF22PRELOG"init zram list lock");
		INIT_LIST_HEAD(&zram->wbd_klist);
		spin_lock_init(&zram->wakeup_wbd_lock);
		spin_lock_init(&zram->wbd_klist_lock);
		spin_lock_init(&zram->wb_table_lock);
		spin_lock_init(&zram->chunk_bitmap_lock);
	//}
	memset(zram_key_task_list, 0, sizeof(zram_key_task_list));
	g_zram = zram;
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end

	/* gendisk structure */
	zram->disk = blk_alloc_disk(NUMA_NO_NODE);
	if (!zram->disk) {
		pr_err("Error allocating disk structure for device %d\n",
			device_id);
		ret = -ENOMEM;
		goto out_free_idr;
	}

	zram->disk->major = zram_major;
	zram->disk->first_minor = device_id;
	zram->disk->minors = 1;
	zram->disk->flags |= GENHD_FL_NO_PART;
	zram->disk->fops = &zram_devops;
	zram->disk->private_data = zram;
	snprintf(zram->disk->disk_name, 16, "zram%d", device_id);

	comp_algorithm_set(zram, ZRAM_PRIMARY_COMP, default_compressor);

	/* Actual capacity set using sysfs (/sys/block/zram<id>/disksize */
	set_capacity(zram->disk, 0);
	/* zram devices sort of resembles non-rotational disks */
	blk_queue_flag_set(QUEUE_FLAG_NONROT, zram->disk->queue);
	blk_queue_flag_set(QUEUE_FLAG_SYNCHRONOUS, zram->disk->queue);

	/*
	 * To ensure that we always get PAGE_SIZE aligned
	 * and n*PAGE_SIZED sized I/O requests.
	 */
	blk_queue_physical_block_size(zram->disk->queue, PAGE_SIZE);
	blk_queue_logical_block_size(zram->disk->queue,
					ZRAM_LOGICAL_BLOCK_SIZE);
	blk_queue_io_min(zram->disk->queue, PAGE_SIZE);
	blk_queue_io_opt(zram->disk->queue, PAGE_SIZE);
	zram->disk->queue->limits.discard_granularity = PAGE_SIZE;
	blk_queue_max_discard_sectors(zram->disk->queue, UINT_MAX);

	/*
	 * zram_bio_discard() will clear all logical blocks if logical block
	 * size is identical with physical block size(PAGE_SIZE). But if it is
	 * different, we will skip discarding some parts of logical blocks in
	 * the part of the request range which isn't aligned to physical block
	 * size.  So we can't ensure that all discarded logical blocks are
	 * zeroed.
	 */
	if (ZRAM_LOGICAL_BLOCK_SIZE == PAGE_SIZE)
		blk_queue_max_write_zeroes_sectors(zram->disk->queue, UINT_MAX);

	blk_queue_flag_set(QUEUE_FLAG_STABLE_WRITES, zram->disk->queue);
	ret = device_add_disk(NULL, zram->disk, zram_disk_groups);
	if (ret)
		goto out_cleanup_disk;

	zram_debugfs_register(zram);
	pr_info("Added device: %s\n", zram->disk->disk_name);
	return device_id;

out_cleanup_disk:
	put_disk(zram->disk);
out_free_idr:
	idr_remove(&zram_index_idr, device_id);
out_free_dev:
	kfree(zram);
	return ret;
}

static int zram_remove(struct zram *zram)
{
	bool claimed;

	mutex_lock(&zram->disk->open_mutex);
	if (disk_openers(zram->disk)) {
		mutex_unlock(&zram->disk->open_mutex);
		return -EBUSY;
	}

	claimed = zram->claim;
	if (!claimed)
		zram->claim = true;
	mutex_unlock(&zram->disk->open_mutex);
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	if (is_memfusion)
		deinit_lru_writeback(zram);
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end
	zram_debugfs_unregister(zram);

	if (claimed) {
		/*
		 * If we were claimed by reset_store(), del_gendisk() will
		 * wait until reset_store() is done, so nothing need to do.
		 */
		;
	} else {
		/* Make sure all the pending I/O are finished */
		sync_blockdev(zram->disk->part0);
		zram_reset_device(zram);
	}

	pr_info("Removed device: %s\n", zram->disk->disk_name);

	del_gendisk(zram->disk);

	/* del_gendisk drains pending reset_store */
	WARN_ON_ONCE(claimed && zram->claim);

	/*
	 * disksize_store() may be called in between zram_reset_device()
	 * and del_gendisk(), so run the last reset to avoid leaking
	 * anything allocated with disksize_store()
	 */
	zram_reset_device(zram);

	put_disk(zram->disk);
	kfree(zram);
	return 0;
}

/* zram-control sysfs attributes */

/*
 * NOTE: hot_add attribute is not the usual read-only sysfs attribute. In a
 * sense that reading from this file does alter the state of your system -- it
 * creates a new un-initialized zram device and returns back this device's
 * device_id (or an error code if it fails to create a new device).
 */
static ssize_t hot_add_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int ret;

	mutex_lock(&zram_index_mutex);
	ret = zram_add();
	mutex_unlock(&zram_index_mutex);

	if (ret < 0)
		return ret;
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}
/* This attribute must be set to 0400, so CLASS_ATTR_RO() can not be used */
static struct class_attribute class_attr_hot_add =
	__ATTR(hot_add, 0400, hot_add_show, NULL);

static ssize_t hot_remove_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	struct zram *zram;
	int ret, dev_id;

	/* dev_id is gendisk->first_minor, which is `int' */
	ret = kstrtoint(buf, 10, &dev_id);
	if (ret)
		return ret;
	if (dev_id < 0)
		return -EINVAL;

	mutex_lock(&zram_index_mutex);

	zram = idr_find(&zram_index_idr, dev_id);
	if (zram) {
		ret = zram_remove(zram);
		if (!ret)
			idr_remove(&zram_index_idr, dev_id);
	} else {
		ret = -ENODEV;
	}

	mutex_unlock(&zram_index_mutex);
	return ret ? ret : count;
}
static CLASS_ATTR_WO(hot_remove);

static struct attribute *zram_control_class_attrs[] = {
	&class_attr_hot_add.attr,
	&class_attr_hot_remove.attr,
	NULL,
};
ATTRIBUTE_GROUPS(zram_control_class);

static struct class zram_control_class = {
	.name		= "zram-control",
	.class_groups	= zram_control_class_groups,
};

static int zram_remove_cb(int id, void *ptr, void *data)
{
	WARN_ON_ONCE(zram_remove(ptr));
	return 0;
}

static void destroy_devices(void)
{
	class_unregister(&zram_control_class);
	idr_for_each(&zram_index_idr, &zram_remove_cb, NULL);
	zram_debugfs_destroy();
	idr_destroy(&zram_index_idr);
	unregister_blkdev(zram_major, "zram");
	cpuhp_remove_multi_state(CPUHP_ZCOMP_PREPARE);
}

//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
//extern struct swap_info_struct* get_swapfile_sis(void); //tran_swapfile.c
extern struct proc_dir_entry *proc_mkdir(const char *name, struct proc_dir_entry *parent);

int vma_pagefault_offset = -1;
int vma_pamajfault_offset = 1;
int mm_users_threshold = 36;
int mm_count_threshold = 24;
int workingset_refault_para = 0;

//extern unsigned int get_rom_life_time(void);

static unsigned int get_device_condition(void) {
	int device_life_time = 1;
	int avg_erase_tlc = -1;
	//device_life_time = get_rom_life_time();
	pr_err("get_rom_life_time: not exist!\n");

	if ((device_life_time > 0) &&(device_life_time <= 10))
		avg_erase_tlc = device_life_time*300;

	return avg_erase_tlc;
}

static unsigned long total_write_limit_mb = 10*1024;	//default 10GB
static unsigned int kern_fw_percent = 50;	//default kernel:fw = 1:1,50%
static int swap_write_config_show(struct seq_file *m, void *v)
{
	seq_printf(m,"total_write_limit_mb = %lu, kern_fw_percent = %u\n", total_write_limit_mb, kern_fw_percent);
	return 0;
}

static int swap_write_config_open(struct inode *inode, struct file *file)
{
	return single_open(file, swap_write_config_show, inode->i_private);
}

static ssize_t swap_write_config_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[10]={'\0'};

	if (count >= 10) {
		return count;
	}

	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("get swaps para failed, please try again!\n");
			return -EFAULT;
		}
		sscanf(buf, "%lu,%u", &total_write_limit_mb, &kern_fw_percent);
		pr_err("total_write_limit_mb = %lu, kern_fw_percent = %u\n",total_write_limit_mb, kern_fw_percent);
	}
	return count;
}

const struct proc_ops swap_write_config_fops = {
	.proc_open		= swap_write_config_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 		= swap_write_config_write,
};

void bdev_size_lookup(void) {
    u64 size = bdev_size >> 10;
    u64 gigabytes;
    u64 remainder;
    u64 total;
    if (size <= 0 || size > 16 * 1024) {
        pr_err("swap size is out of range: %lluKB, please check!\n", size);
        snprintf(backing_dev_size, sizeof(backing_dev_size), "0.00");
    } else if (size < 1024) {
        snprintf(backing_dev_size, sizeof(backing_dev_size), "0.%llu0",  size % 1000 / 100);
    } else {
        gigabytes = size / 1024;  // Round to the nearest GB
        remainder = size % 1024;
        total = gigabytes * 1000 + remainder;
        snprintf(backing_dev_size, sizeof(backing_dev_size), "%llu.%llu0", total / 1000, total % 1000 / 100);
    }
}

static int swap_size_show(struct seq_file *m, void *v)
{
	unsigned long size = bdev_size >> 10;

	seq_printf(m, "%s", backing_dev_size);
	pr_err("bdev_size =%luKB( %luMB)\n", bdev_size, size);
	return 0;
}

static int swap_size_open(struct inode *inode, struct file *file)
{
	return single_open(file, swap_size_show, inode->i_private);
}

static ssize_t swap_size_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[5]={'\0'};
	unsigned long size = bdev_size >> 10;

	if (count >= 5) {
		return count;
	}
	if(copy_from_user(buf, ubuf, count))
	{
		pr_err("get swaps para failed, please try again!\n");
		return count;
	}
	buf[4] = '\0';
	memset(backing_dev_size, 0, sizeof(backing_dev_size));
	strncpy(backing_dev_size, buf, sizeof(backing_dev_size) - 1);
	pr_err("buf = %s, backing_dev_size =%luKB( %luMB)\n", buf, bdev_size, size);

	return count;
}

const struct proc_ops swap_size_fops = {
	.proc_open		= swap_size_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write		= swap_size_write,
};

static int tlc_value_show(struct seq_file *m, void *v)
{
	unsigned int result = get_device_condition();
	seq_printf(m, "%s\t%d\n", "avg_erase_tlc", result);

	return 0;
}
static int tlc_value_open(struct inode *inode, struct file *file)
{
	return single_open(file, tlc_value_show, inode->i_private);
}

const struct proc_ops tlc_value_fops = {
	.proc_open		= tlc_value_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
};

#define ZRAM0_NAME    "zram0   "
#define BACK_DEV_NAME "swapfile"
#define SEG_LOAD "backing_dev"

/*keep same with enum emem_type of tranmem_emem.h */
enum emem_type {
	DURANTION = 0,
	EXCEPTION,
};

#define ZRAM_EMEM_MAX_LEN 256
#define zram_emem_format(buf, format, data, left, copied, written) {\
	if (left > 32) {	\
		copied = snprintf(buf+written, left, format, data);\
		written += copied;\
		left -= copied;\
	}\
}

void zram_emem_printk(struct sysinfo *si, int type) {
	unsigned long swapused, anon, mem_used, compress, tmp, swapin, delaytime;
	char buf[ZRAM_EMEM_MAX_LEN];
	int written = 0;
	int left = ZRAM_EMEM_MAX_LEN;
	int copied = 0;
	u64 bdev_used = 0, bdev_read = 0, bdev_write = 0;
	struct zram* zram = g_zram;
	s64 *thrashing;
	s64 thrashing_val[3];
	u64 now;

	if (!zram || !init_done(zram)) {
		pr_info("[TranMemoryInfo] zram is not ready\n");
		return;
	}

	if (!down_read_trylock(&zram->init_lock)) {
		pr_info("[TranMemoryInfo] zram is busy\n");
		return;
	}
	bdev_used = PAGE2KB((u64)atomic64_read(&zram->stats.bd_count));
	bdev_read = PAGE2KB((u64)atomic64_read(&zram->stats.bd_reads));
	bdev_write = PAGE2KB((u64)atomic64_read(&zram->stats.bd_writes));

	/* System Total Compressible Anon Page */
	swapused = PAGE2KB(si->totalswap - si->freeswap);
	anon = PAGE2KB(global_node_page_state(NR_ACTIVE_ANON) + global_node_page_state(NR_INACTIVE_ANON));
	//zram_emem_format(buf, "stca:%lu;", anon, left, copied, written);

	/* System Compression Proportion */
	tmp = anon+swapused;
	if (tmp > 0)
		tmp = (swapused*100/tmp);
	//zram_emem_format(buf, "scp:%lu;", tmp, left, copied, written);

	/*
	 * Zram Compress Ratio
	 * eg: ZRAM:	 1,060K physical used for	  3,584K in swap (4,206,388K total swap)
	 * 1060K is mem_used, 3,584K
	 */
	compress = mem_used = 0;
	mem_used = PAGE2KB(zs_get_total_pages(zram->mem_pool));
	compress = (mem_used > 0)?(swapused*100 / mem_used):0UL;
	//zram_emem_format(buf, "zcpr:%lu;", compress, left, copied, written);

	now = (u64)ktime_divns(ktime_get_boottime(), NSEC_PER_SEC);
	pr_info("[TranMemoryInfo] memfusion%s: ZU:%llu; ZR:%llu; ZW:%llu; BU:%llu; BR:%llu; BW:%llu; "
		"STCA:%lu; SCP:%lu; ZCPR:%lu; T:%llu\n",
		g_backingdev?"":" zero",
		PAGE2KB((u64)atomic64_read(&zram->stats.pages_stored)),
		PAGE2KB((u64)atomic64_read(&zram->stats.num_reads)),
		PAGE2KB((u64)atomic64_read(&zram->stats.num_writes)),
		bdev_used, bdev_read, bdev_write, anon, tmp, compress, now);

	if (EXCEPTION == type) {
		memset(buf, 0, ZRAM_EMEM_MAX_LEN);
		/* App Save Ratio , per thousand*/
		tmp = compress * (swapused + anon);
		if (tmp > 0)
			tmp = (1000*swapused*(compress - 100)/tmp);
		zram_emem_format(buf, "asr:%lu;", tmp, left, copied, written);

		/* Compressed Pages In Bdev, unit is KB */
		tmp = PAGE2KB(atomic64_read(&zram->stats.bd_objcnt));
		zram_emem_format(buf, "cpib:%lu;", tmp, left, copied, written);

		/* Bdev Actual Saved Physical Memory, unit is KB */
		tmp = atomic64_read(&zram->stats.bd_size)>>10;
		zram_emem_format(buf, "basm:%lu;", tmp, left, copied, written);

		/* Zram Thrashing */
		thrashing = fwk_thrashing;
		spin_lock(&fwk_thrash_lock);
		if (time_after64(get_jiffies_64(), next_update_jiffies+2*HZ)) {
			thrashing_val[0] = thrashing[0] = 0L;
			thrashing_val[1] = thrashing[1] = 0L;
			thrashing_val[2] = thrashing[2] = 0L;
		} else {
			thrashing_val[0] = thrashing[0];
			thrashing_val[1] = thrashing[1];
			thrashing_val[2] = thrashing[2];
		}
		spin_unlock(&fwk_thrash_lock);
		/*recent 1s to now , in pages/s */
		zram_emem_format(buf, "zst:%lld", thrashing_val[0], left, copied, written);
		/*recent 2s to recent 1s , in pages/s */
		zram_emem_format(buf, ",%lld", thrashing_val[1], left, copied, written);
		/*recent 3s to recent 2s , in pages/s */
		zram_emem_format(buf, ",%lld;", thrashing_val[2], left, copied, written);

		/* Memfusion Write IO */
		if (is_memcg_compact_v2)
			tmp = (atomic64_read(&zram->stats.swapout_count) > 0)?(atomic64_read(&zram->stats.swapout_delay)/atomic64_read(&zram->stats.swapout_count)):0ULL;
		else
			tmp = (bdev_write_page_count>0)?(bdev_write_page_time/bdev_write_page_count):0ULL;
		zram_emem_format(buf, "mwio:%lu;", tmp, left, copied, written);

		/* Memfusion Read IO */
		delaytime = atomic64_read(&zram->stats.swapin_delay);
		swapin = atomic64_read(&zram->stats.swapin_count);
		tmp = (swapin > 0)?(delaytime/swapin):0ULL;
		zram_emem_format(buf, "mrio:%lu;", tmp, left, copied, written);

		pr_info("[TranMemoryInfo] %s\n", buf);
	}
	up_read(&zram->init_lock);

	return;
}
EXPORT_SYMBOL_GPL(zram_emem_printk);

static void dump_zram(struct seq_file *m, struct zram *zram) {
	seq_printf(m, "Filename\tType\t\tSize\t\tUsed\t\tPriority\tRead\t\tWrite\t\tType\n");
	seq_printf(m, "%s\t%s\t", ZRAM0_NAME, "partition");
	seq_printf(m, "%-16llu",  (zram->disksize >> 10));
	seq_printf(m, "%-16llu",  (u64)atomic64_read(&zram->stats.pages_stored) << (PAGE_SHIFT - 10));
	seq_printf(m, "%-8d\t",  -2);
	seq_printf(m, "%-16llu",  (u64)atomic64_read(&zram->stats.num_reads) << (PAGE_SHIFT - 10));
	seq_printf(m, "%-16llu",  (u64)atomic64_read(&zram->stats.num_writes) << (PAGE_SHIFT - 10));
	seq_printf(m, "%s\n",	 "Hot");

	return;
}

static int dump_info_show(struct seq_file *m, void *v)
{
	int i;
	u64 tmp;

	if (g_backingdev) {
		if (!down_read_trylock(&g_backingdev->init_lock)) {
			seq_printf(m, "zram is busy, please try again later\n");
			return 0;
		}

		if (!init_done(g_backingdev)) {
			up_read(&g_backingdev->init_lock);
			seq_printf(m, "zram is not ready yet, please try again later\n");
			return 0;
		}
		dump_zram(m, g_backingdev);

		seq_printf(m, "%s\t%s\t\t", BACK_DEV_NAME, "file");
		seq_printf(m, "%-16lu",  g_backingdev->nr_pages_total << (PAGE_SHIFT - 10));
		seq_printf(m, "%-16llu",  (u64)atomic64_read(&g_backingdev->stats.bd_count) << (PAGE_SHIFT - 10));
		seq_printf(m, "%-8d\t",  -3);
		seq_printf(m, "%-16llu",  (u64)atomic64_read(&g_backingdev->stats.bd_reads)<<(PAGE_SHIFT - 10));
		seq_printf(m, "%-16llu",  (u64)atomic64_read(&g_backingdev->stats.bd_writes)<<(PAGE_SHIFT - 10));
		seq_printf(m, "%s\n",    "Cold");

		seq_printf(m, "\nNow We Have %d Backing_Dev , Now is %d Backing_Dev",
					(g_backing_dev_avail_count + 1), atomic_read(&g_backingdev->backing_dev_index));
		seq_printf(m, "\nBDEV details: \n");

		for (i = 0; i <= g_backing_dev_avail_count; i++) {
			seq_printf(m, "%s%d\t%s\t\t", SEG_LOAD, i, "file");
			seq_printf(m, "%-16lu",  g_backingdev->nr_pages_list[i] << (PAGE_SHIFT - 10));
			seq_printf(m, "%-16llu",  (u64)atomic64_read(&g_backingdev->stats.bd_count_list[i]) << (PAGE_SHIFT - 10));
			seq_printf(m, "%-8d\t",  -3);
			seq_printf(m, "%-16llu",  (u64)atomic64_read(&g_backingdev->stats.bd_reads_list[i]) << (PAGE_SHIFT - 10));
			seq_printf(m, "%-16llu",  (u64)atomic64_read(&g_backingdev->stats.bd_writes_list[i]) << (PAGE_SHIFT - 10));
			seq_printf(m, "%s\n",    "Cold");
		}

		seq_printf(m, "\nbacking_dev_size size:%s GB\n", backing_dev_size);
		if (eswap_swapout_enable())
			seq_printf(m, "Swap_Write_Enable:Kernel[On], Frameworks[On]\n");
		else
			seq_printf(m, "Swap_Write_Enable:Kernel[%s], Frameworks[%s]\n",
				g_swap_kernel_write_enable?"On":"Off", g_swap_fwk_write_enable?"On":"Off");

		seq_printf(m, "%-20s %-16llu\n", "framework_write", (u64)atomic64_read(&g_backingdev->stats.bd_fwk_reclaim) << (PAGE_SHIFT - 10));
		tmp = is_memfusion?(u64)atomic64_read(&g_backingdev->stats.bd_ker_reclaim):(u64)atomic64_read(&g_backingdev->stats.bd_writes);
		seq_printf(m, "%-20s %-16llu\n", "kernel_write", tmp << (PAGE_SHIFT - 10));
		seq_printf(m, "%-20s %-16llu\n", "wbs_write", (u64)atomic64_read(&g_backingdev->stats.bd_wb_reclaim) << (PAGE_SHIFT - 10));
		/* nr. pages of key tasks which not be allowed writeback. */
		seq_printf(m, "%-20s %-16llu\n", "key_pages", (u64)atomic64_read(&g_backingdev->stats.key_pages) << (PAGE_SHIFT - 10));
		seq_printf(m, "%-20s %-16llu\n", "same_pages", (u64)atomic64_read(&g_backingdev->stats.same_pages) << (PAGE_SHIFT - 10));
		seq_printf(m, "%-20s %-16llu\n", "list_pages", (u64)atomic64_read(&g_backingdev->stats.wbd_klist_pages) << (PAGE_SHIFT - 10));
		if (zram_writeback_aggregation)
			seq_printf(m, "%-20s %-16llu\n", "bd_pages", (u64)atomic64_read(&g_backingdev->stats.bd_objcnt) << (PAGE_SHIFT - 10));
		up_read(&g_backingdev->init_lock);

		seq_printf(m, "%-20s %-16llu\n", "bd_index_excpt", (u64)atomic64_read(&zram_bd_index_times));
		seq_printf(m, "\n1. Limit Reason\n");
		seq_printf(m, "%-20s %-16llu\n", "app_launch:", app_launch);
		seq_printf(m, "%-20s %-16llu\n", "screen_on_break:", screen_on_break_cnt);

		seq_printf(m, "\n2. Block Reason(should writeback)\n");
		seq_printf(m, "%-20s %-16llu\n", "wbs_tigger:", zram_writeback_store_times);
		seq_printf(m, "%-20s %-16llu (%llu)\n", "total_trigger:", total_trigger_cnt, insuff_time_trigger_cnt);
		seq_printf(m, "%-20s %-16llu\n", "total_write:", total_write_cnt);

		seq_printf(m, "%-20s %-16llu\n", "max_pages:", max_pages_cnt);
		seq_printf(m, "%-20s %-16llu\n", "ratio_cnt:", ratio_cnt);
		seq_printf(m, "%-20s %-16llu (%d)\n", "wakeup_wbd_cnt:", wakeup_wbd_cnt, actual_write_cnt);
		seq_printf(m, "%-20s %-16llu (%d)\n", "continue_write_cnt:", continue_write_cnt, last_write_cnt);

		seq_printf(m, "%-20s %-16llu\n", "klist_skip_cnt:", klist_wb_over_cnt);
		seq_printf(m, "%-20s %-16llu\n", "klist_insuff_time_cnt:", klist_less_time_cnt);
		seq_printf(m, "%-20s %-16llu\n", "read_break_cnt", zram_wbd_ifbreak_times);
		seq_printf(m, "%-20s %-16llu\n", "kwbd_wb_failed:", zram_kwbd_wb_failed);

		seq_printf(m, "\n3. Block Reason(system loading)\n");
		seq_printf(m, "%-20s %-16llu\n", "loadavg_cnt:", loadavg_cnt);
		seq_printf(m, "%-20s %-16llu\n", "cpu_usage_cnt:", cpu_usage_cnt);
		seq_printf(m, "%-20s %-16llu\n", "iowait_cnt:", iowait_cnt);
		seq_printf(m, "%-20s %-16llu\n", "cpu_io_skip_cnt:", cpu_io_skip_cnt);
		seq_printf(m, "%-20s %-16llu\n", "high_loading:", high_loading_cooling_cnt);

		seq_printf(m, "\n4. System Loading\n");
		seq_printf(m, "%s\t%lu,%lu,%lu/%lu\n","Current loadavg/loadavg[0] threshold:", loadavg[0], loadavg[1], loadavg[2], loadavg_threshold);
		seq_printf(m, "%s\t%lu/%lu/%lu\n", "Current iowait/iowait_threshold/Last trigger usage:", io_time, iowait_threshold, g_io_time);
		seq_printf(m, "%s\t%lu/%lu/%lu\n", "Current cpu usage/cpu_usage_threshold/Last trigger usage:", usage_time, cpu_usage_threshold, g_usage_time);
	}else if (g_zram) {
		if (!down_read_trylock(&g_zram->init_lock)) {
			seq_printf(m, "zram is busy, please try again later\n");
			return 0;
		}

		if (!init_done(g_zram)) {
			up_read(&g_zram->init_lock);
			seq_printf(m, "zram is not ready yet, please try again later\n");
			return 0;
		}
		dump_zram(m, g_zram);
		up_read(&g_zram->init_lock);
	}

	/* Errors occurred with both zram and the backing device.*/
	if (NULL == g_backingdev) {
		seq_printf(m, "\nbacking_dev_size size:0.00 GB\n");
		seq_printf(m, "Swap_Write_Enable:Kernel[%s], Frameworks[%s]\n", g_swap_kernel_write_enable?"On":"Off", g_swap_fwk_write_enable?"On":"Off");
		seq_printf(m, "%s\t%d\n", "framework", 0);
		seq_printf(m, "%s\t%d\n", "kernel", 0);
		switch(zram_bdev_install_error)
		{
			case ZRAM_BDEV_OPEN_ERROR:
				seq_printf(m, "%s\t%s\n", "zram_bdev_install_error_reason:", "Cannot open target path.");
				break;
			case ZRAM_BDEV_FALLOCATE_ERROR:
				seq_printf(m, "%s\t%s\n", "zram_bdev_install_error_reason", "Cannot open fallocate path.");
				break;
			case ZRAM_BDEV_LD_ATTACT_ERROR:
				seq_printf(m, "%s\t%s\n", "zram_bdev_install_error_reason", "Cannot create loop_device path.");
				break;
			case ZRAM_BDEV_LDFD_GET_ERROR:
				seq_printf(m, "%s\t%s\n", "zram_bdev_install_error_reason", "Cannot get loop_fd path.");
				break;
			case ZRAM_BDEV_LD_ENABLE_DIRECTIO_ERROR:
				seq_printf(m, "%s\t%s\n", "zram_bdev_install_error_reason", "Cannot EnableDirectIo loop_fd path.");
				break;
			case ZRAM_BDEV_INSTALL_BDEV_ERROR:
				seq_printf(m, "%s\t%s\n", "zram_bdev_install_error_reason", "Zram Backing device install failed.");
				break;
			default:
				break;
		}
		if(zram_bdev_install_error)
			seq_printf(m, "%s\tinstall:%d errno:%d\n", "zram_bdev_install_errorno: ", zram_bdev_install_error, zram_bdev_install_errorno);
	}

	return 0;
}

static int dump_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_info_show, inode->i_private);
}

const struct proc_ops dump_info_fops = {
	.proc_open		= dump_info_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
};

static ssize_t swap_write_enable_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[16]={'\0'};
	int tmp = 0;

	if (count >= 16) {
		return count;
	}
	if(copy_from_user(buf, ubuf, count))
	{
		pr_err("get swaps para failed, please try again!\n");
		return count;
	}
	buf[1] = '\0';
	sscanf(buf, "%d", &tmp);
	if (tmp >= 0 && tmp <= 3)
	{
		/* When the new-memfusion is enabled, cloud control may enable the old-memfusion
		 * in abnormal scenarios, which can cause KE. Therefore, once the new-memfusion
		 * is enabled, no one is allowed to enable the old-memfusion anymore.
		 */
		if ((tmp > 0) && g_memcg_compress_v2_enable) {
			pr_err(MF22PRELOG"operation not allowed because the new memfusion is already enabled.\n");
			return count;
		}

		g_swap_kernel_write_enable = tmp & 0x1;
		g_swap_fwk_write_enable = (tmp & 0x2) >> 1;

		//disable new-memfusion swapout meanwhile
		if (!g_swap_kernel_write_enable) {
			eswapout_enable(false);
		}

		pr_err(MF22PRELOG"memfusion kernel_write_enable = %d, fwk_write_enable = %d %s\n",
			g_swap_kernel_write_enable, g_swap_fwk_write_enable, !g_swap_kernel_write_enable?"disable new-memfusion":"");
	} else
		pr_err(MF22PRELOG"memfusion Wrong arguments!\n");

	return count;
}

static int swap_write_enable_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_swap_kernel_write_enable | (g_swap_fwk_write_enable << 1));
	return 0;
}
static int swap_write_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, swap_write_enable_show, inode->i_private);
}

const struct proc_ops swap_write_enable_fops = {
	.proc_open		= swap_write_enable_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 		= swap_write_enable_write,
};

static int swap_usage_show(struct seq_file *m, void *v)
{
	if (g_backingdev != NULL)
	{
		down_read(&g_backingdev->init_lock);
		seq_printf(m, "%llu\n", (u64)atomic64_read(&g_backingdev->stats.bd_count) << (PAGE_SHIFT - 10));
		up_read(&g_backingdev->init_lock);
	}
	else
		seq_printf(m, "%d\n", 0);

	return 0;
}
static int swap_usage_open(struct inode *inode, struct file *file)
{
	return single_open(file, swap_usage_show, inode->i_private);
}

const struct proc_ops swap_usage_fops = {
	.proc_open		= swap_usage_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
};

static int swap_write_count_show(struct seq_file *m, void *v)
{
	if (g_backingdev != NULL) {
		down_read(&g_backingdev->init_lock);
		seq_printf(m, "%s\t%llu\n", "framework", (u64)atomic64_read(&g_backingdev->stats.bd_fwk_reclaim) << (PAGE_SHIFT - 10));
		seq_printf(m, "%s\t%llu\n", "kernel", (u64)atomic64_read(&g_backingdev->stats.bd_ker_reclaim) << (PAGE_SHIFT - 10));
		up_read(&g_backingdev->init_lock);
	} else {
		seq_printf(m, "%s\t%d\n", "framework", 0);
		seq_printf(m, "%s\t%d\n", "kernel", 0);
	}

	return 0;
}
static int swap_write_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, swap_write_count_show, inode->i_private);
}

const struct proc_ops swap_write_count_fops = {
	.proc_open		= swap_write_count_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
};

static ssize_t writen_count_per_day_write(struct file *File, const char __user *buffer, size_t count, loff_t *pos) {
	return count;
}

static int writen_count_per_day_show(struct seq_file *m, void *v) 
{
	seq_printf(m, "%s\t%d\n", "framework", 0);
	seq_printf(m, "%s\t%d\n", "kernel", 0);

	return 0;
}

static int writen_count_per_day_open(struct inode *inode, struct file *file)
{
	return single_open(file, writen_count_per_day_show, NULL);
}

const struct proc_ops writen_count_per_day_fops = {
	.proc_write = writen_count_per_day_write,
	.proc_open = writen_count_per_day_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t vma_para_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char tmp[64] = {0};
	char *buf;
	char *buf2;
	int ret;
	int i = 0;

	if (count >= 64) {
		return count;
	}
	if (copy_from_user(tmp, ubuf, count)) {
		ret = -EFAULT;
		return ret;
	}

	buf = strstrip(tmp);
	pr_info(MF22PRELOG"mm_para is %s", buf);
	while ((buf2 = strsep(&buf, " ")) != NULL) {
		pr_info(MF22PRELOG"buf2 is %s", buf2);
		if (i == 0) {
			sscanf(buf2, "%d", &vma_pagefault_offset);
		} else {
			sscanf(buf2, "%d", &vma_pamajfault_offset);
		}
		i++;
	}
	ret = count;

	return ret;
}

static int vma_para_show(struct seq_file *m, void *v) {

	seq_printf(m, "%s\t%d\n", "vma_pagefault_offset : ", vma_pagefault_offset);
	seq_printf(m, "%s\t%d\n", "vma_pamajfault_offset : ", vma_pamajfault_offset);

	return 0;
}

static int vma_para_open(struct inode *inode, struct file *file)
{
	return single_open(file, vma_para_show, NULL);
}

const struct proc_ops vma_para_fops = {
//	.owner = THIS_MODULE,
	.proc_write = vma_para_write,
	.proc_open = vma_para_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t mnt_threthold_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char tmp[64] = {0};
	char *buf;
	char *buf2;
	int ret;
	int i = 0;

	if (count >= 64) {
		return count;
	}
	if (copy_from_user(tmp, ubuf, count)) {
		ret = -EFAULT;
		return ret;
	}

	buf = strstrip(tmp);
	pr_info(MF22PRELOG"mm_para is %s", buf);
	while ((buf2 = strsep(&buf, " ")) != NULL) {
		pr_info("buf2 is %s", buf2);
		if (i == 0) {
			sscanf(buf2, "%d", &mm_users_threshold);
		} else {
			sscanf(buf2, "%d", &mm_count_threshold);
		}
		i++;
	}
	ret = count;

	return ret;
}

static int mnt_threthold_show(struct seq_file *m, void *v) {

	seq_printf(m, "%s\t%d\n", "mm_users_threshold : ", mm_users_threshold);
	seq_printf(m, "%s\t%d\n", "mm_count_threshold : ", mm_count_threshold);

	return 0;
}

static int mnt_threthold_open(struct inode *inode, struct file *file)
{
	return single_open(file, mnt_threthold_show, NULL);
}

const struct proc_ops mnt_threthold_fops = {
//	.owner = THIS_MODULE,
	.proc_write = mnt_threthold_write,
	.proc_open = mnt_threthold_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t zram_drv_para_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char tmp[64] = {0};
	char *buf;
	int ret;

	if (count >= 64) {
		return count;
	}
	if (copy_from_user(tmp, ubuf, count)) {
		ret = -EFAULT;
		return ret;
	}

	buf = strstrip(tmp);
	pr_info(MF22PRELOG"zram_drv_para is %s", buf);
	ret = count;

	return ret;
}

static int zram_drv_para_show(struct seq_file *m, void *v) {

	seq_printf(m, "%s\t%d\n", "zram_cold_threlod : ", 10);

	return 0;
}

static int zram_drv_para_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_drv_para_show, NULL);
}

const struct proc_ops zram_drv_para_fops = {
//	.owner = THIS_MODULE,
	.proc_write = zram_drv_para_write,
	.proc_open = zram_drv_para_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t workingset_refault_para_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char tmp[64] = {0};
	char *buf;
	int ret;

	if (count >= 64) {
		return count;
	}
	if (copy_from_user(tmp, ubuf, count)) {
		ret = -EFAULT;
		return ret;
	}

	buf = strstrip(tmp);
	pr_info(MF22PRELOG"workingset_para is %s", buf);
	sscanf(buf, "%d", &workingset_refault_para);
	ret = count;

	return ret;
}

static int workingset_refault_para_show(struct seq_file *m, void *v) {

	seq_printf(m, "%s\t%d\n", "workingset_refault_para : ", workingset_refault_para);

	return 0;
}

static int workingset_refault_para_open(struct inode *inode, struct file *file)
{
	return single_open(file, workingset_refault_para_show, NULL);
}

const struct proc_ops workingset_refault_para_fops = {
//	.owner = THIS_MODULE,
	.proc_write = workingset_refault_para_write,
	.proc_open = workingset_refault_para_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t zram_writeback_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[ZRAM_HOUR_BUF_LEN]={'\0'};
	int writeback_count_temp = 0;

	if (count >= 10) {
		return count;
	}

	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("memfusion2_2get writeback para failed, please try again!\n");
			return -EFAULT;
		}

		sscanf(buf, "%d", &writeback_count_temp);
		if(writeback_count_temp < 0)
		{
			format_hour_readback(buf, ZRAM_HOUR_BUF_LEN, false);
		}
		else if (writeback_count_temp < zram_write_limit_debug)
		{
			pr_err("memfusion2_2:zram_write_limit_debug = [%d]->[%d] \n", zram_write_limit_debug, writeback_count_temp);
			zram_write_limit_debug = writeback_count_temp;
			zram_write_limit_time_stamp = jiffies_64 + zram_write_limit_timeup *HZ;
		}
		pr_err("memfusion2_2:zram_write_limit_debug = %d \n",zram_write_limit_debug);
	}

	return count;
}

static int zram_writeback_show(struct seq_file *m, void *v)
{
	seq_printf(m,"is a debug interface to writeback %d,debug_log_enable =[%d]\n",zram_write_limit_debug, debug_log_enable);
	return 0;
}

static int zram_writeback_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_writeback_show, inode->i_private);
}


const struct proc_ops zram_writeback_fops = {
	.proc_open		= zram_writeback_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_writeback_write,
};

static ssize_t zram_start_thread_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[10]={'\0'};
	int start_or_wakeup = 0;

	if (count >= 10) {
		return count;
	}

	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("memfusion2_2:get swaps para failed, please try again!\n");
			return -EFAULT;
		}
		sscanf(buf, "%d", &start_or_wakeup);
		pr_err("memfusion2_2:start_or_wakeup = %d \n",start_or_wakeup);
	}
	if (start_or_wakeup == 1)
	{
		if(g_backingdev != NULL)
			try_wakeup_zram_wbd_lite(g_backingdev, ZRAM_WB_ALL);
		else
			pr_err("memfusion2_2:Please wait for zram init done!! \n");
	}

	return count;
}

static int zram_start_thread_show(struct seq_file *m, void *v)
{
	seq_printf(m,"is a debug interface to start thread \n");
	return 0;
}

static int zram_start_thread_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_start_thread_show, inode->i_private);
}

const struct proc_ops zram_start_thread_fops = {
	.proc_open		= zram_start_thread_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_start_thread_write,
};

static ssize_t system_loading_threthold_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char tmp[64] = {0};
	char *buf;
	char *buf2;
	int ret;
	int i = 0;

	if (count >= 64) {
		return count;
	}
	if (copy_from_user(tmp, ubuf, count)) {
		ret = -EFAULT;
		return ret;
	}

	buf = strstrip(tmp);
	pr_info(MF22PRELOG"system_loading_threthold is %s", buf);
	while ((buf2 = strsep(&buf, " ")) != NULL) {
		pr_info(MF22PRELOG"buf2 is %s", buf2);
		if (i == 0) {
			sscanf(buf2, "%ld", &loadavg_threshold);
		} else if (i == 1){
			sscanf(buf2, "%ld", &iowait_threshold);
		} else {
			sscanf(buf2, "%ld", &cpu_usage_threshold);
		}
		i++;
	}
	ret = count;

	return ret;
}

static int system_loading_threthold_show(struct seq_file *m, void *v) {

	seq_printf(m, "%s\t%lu\n", "loadavg[0] threshold : ", loadavg_threshold);
	seq_printf(m, "%s\t%lu\n", "iowait_threshold : ", iowait_threshold);
	seq_printf(m, "%s\t%lu\n", "cpu_usage_threshold : ", cpu_usage_threshold);

	seq_printf(m, "Usage:\necho aa bb cc > system_loading_threthold\n aa:loadavg[0] threshold \n bb:iowait_threshold \n cc:cpu_usage_threshold \n");

	return 0;
}

static int system_loading_threthold_open(struct inode *inode, struct file *file)
{
	return single_open(file, system_loading_threthold_show, NULL);
}

const struct proc_ops system_loading_threthold_fops = {
//	.owner = THIS_MODULE,
	.proc_write = system_loading_threthold_write,
	.proc_open = system_loading_threthold_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t high_load_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[10]={'\0'};
	int high_loading_cooling_temp = 0;

	if (count >= 10) {
		return count;
	}

	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("memfusion2_2:get high_loading_cooling para failed, please try again!\n");
			return -EFAULT;
		}
		sscanf(buf, "%d", &high_loading_cooling_temp);
		pr_info(MF22PRELOG"high_loading_cooling_write high_loading_cooling_temp %d", high_loading_cooling_temp);
	}
	if (high_loading_cooling_temp) {
		atomic_set(&high_loading_cooling, 1);
		pr_info(MF22PRELOG"high_loading_cooling_write 1 %u", atomic_read(&high_loading_cooling));
	} else {
		atomic_set(&high_loading_cooling, 0);
		pr_info(MF22PRELOG"high_loading_cooling_write 0 %u", atomic_read(&high_loading_cooling));
	}

	return count;
}


static int proc_high_load_show(struct seq_file *m, void *v)
{
	seq_printf(m,"high_loading_cooling %u \n", atomic_read(&high_loading_cooling));
	return 0;
}

static int high_load_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_high_load_show, inode->i_private);
}

const struct proc_ops high_load_fops = {
	.proc_open		= high_load_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= high_load_write,
};


static ssize_t am_app_launch_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[10]={'\0'};
	int is_app_launch_temp = 0;

	if (count >= 10) {
		return count;
	}

	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("memfusion2_2:get app launch para failed, please try again!\n");
			return -EFAULT;
		}
		sscanf(buf, "%d", &is_app_launch_temp);
	}
	if (is_app_launch_temp) {
		atomic_set(&is_app_launch, 1);
	} else {
		atomic_set(&is_app_launch, 0);
	}

	return count;
}


static int proc_am_app_launch_show(struct seq_file *m, void *v)
{
	seq_printf(m,"am_app_launch %u \n", atomic_read(&is_app_launch));
	return 0;
}

static int am_app_launch_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_am_app_launch_show, inode->i_private);
}

const struct proc_ops am_app_launch_fops = {
	.proc_open		= am_app_launch_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= am_app_launch_write,
};

static ssize_t zram_wbd_interval_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[10]={'\0'};

	if (count >= 10) {
		return count;
	}

	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("memfusion2_2:get swaps para failed, please try again!\n");
			return -EFAULT;
		}
		sscanf(buf, "%d", &zram_wbd_interval);
		pr_err("memfusion2_2:zram_wbd_interval = %d \n",zram_wbd_interval);
	}
	return count;
}

static int zram_wbd_interval_show(struct seq_file *m, void *v)
{
	seq_printf(m,"zram_wbd_interval:%d\n", zram_wbd_interval);
	pr_err("memfusion2_2:zram_wbd_interval = %d \n", zram_wbd_interval);
	return 0;
}

static int zram_wbd_interval_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_wbd_interval_show, inode->i_private);
}

const struct proc_ops zram_wbd_interval_fops = {
	.proc_open		= zram_wbd_interval_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_wbd_interval_write,
};

static ssize_t bdev_rw_data_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[10]={'\0'};
	int bdev_rw_temp = 0;

	if (count >= 10) {
		return count;
	}
	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("memfusion2_2:get bdev_rw_data para failed, please try again!\n");
			return -EFAULT;
		}
		if (sscanf(buf, "%d",&bdev_rw_temp) == 1)
		{
			if(bdev_rw_temp == 0)
			{
				bdev_write_page_time = 0;
				bdev_write_page_count = 0;
			}
		}
	}
	return count;
}

static int bdev_rw_data_show(struct seq_file *m, void *v)
{
	struct zram* zram = g_backingdev;

	if (!zram || !init_done(zram)) {
		seq_printf(m,"bdev_read_page_time: 0 \n");
		seq_printf(m,"bdev_read_page_count: 0 \n");
		return 0;
	}

	seq_printf(m,"bdev_write_page_time: %lld \n", bdev_write_page_time);
	seq_printf(m,"bdev_write_page_count: %lld \n", bdev_write_page_count);

	seq_printf(m,"bdev_read_page_time: %lld \n", (zram?atomic64_read(&zram->stats.swapin_delay):0ULL));
	seq_printf(m,"bdev_read_page_count: %lld \n", (zram?atomic64_read(&zram->stats.swapin_count):0ULL));

	if (zram_writeback_aggregation) {
		/* Indicates how much physical memory was saved by moving the data to ROM. unit KB */
		seq_printf(m,"bdev_cprs_size: %lld \n", (zram?(atomic64_read(&zram->stats.bd_size)>>10):0ULL));
		/* Indicates how much ROM space the extracted data would occupy based on its decompressed size. unit KB */
		seq_printf(m,"bdev_cprs_objcnt: %lld \n", (zram?(atomic64_read(&zram->stats.bd_objcnt)<<(PAGE_SHIFT-10)):0ULL));
		/* Indicates total ROM space used in backing device, unit KB */
		seq_printf(m,"bdev_actual_used: %lld \n", (zram?(atomic64_read(&zram->stats.bd_count)<<(PAGE_SHIFT-10)):0ULL));
		seq_printf(m,"bdev_max_cprs_size: %lld \n", (zram?(atomic64_read(&zram->stats.bd_max_size)>>10):0ULL));
		seq_printf(m,"bdev_max_bd_count: %lld \n", (zram?(atomic64_read(&zram->stats.bd_max_count)<<(PAGE_SHIFT-10)):0ULL));
		seq_printf(m,"bdev_sz_inc_exp: %lld \n", (zram?atomic64_read(&zram->stats.bd_size_inc_except):0ULL));
		seq_printf(m,"bdev_sz_dec_exp: %lld \n", (zram?atomic64_read(&zram->stats.bd_size_dec_except):0ULL));
		seq_printf(m,"bdev_table_exp: %lld \n", (zram?atomic64_read(&zram->stats.bd_table_except):0ULL));
	}

	if (zram_delayacct_enable) {
		atomic64_t compress_delay   = ATOMIC64_INIT(0);
		atomic64_t compress_pages   = ATOMIC64_INIT(0);
		atomic64_t decompress_delay = ATOMIC64_INIT(0);
		atomic64_t decompress_pages = ATOMIC64_INIT(0);

		for (int i = 0; i < zram->num_active_comps; i++) {
			if (atomic64_read(&zram->stats.compress_pages[i]) > 0) {
				if (zram->num_active_comps > 1) {
					seq_printf(m,"zram_wdelay[%d]: %lld \n", i, (zram?atomic64_read(&zram->stats.compress_delay[i]):0ULL));
					seq_printf(m,"zram_wpages[%d]: %lld \n", i, (zram?atomic64_read(&zram->stats.compress_pages[i]):0ULL));
				}
				atomic64_add(atomic64_read(&zram->stats.compress_delay[i]), &compress_delay);
				atomic64_add(atomic64_read(&zram->stats.compress_pages[i]), &compress_pages);
			}

			if (atomic64_read(&zram->stats.decompress_pages[i]) > 0) {
				if (zram->num_active_comps > 1) {
					seq_printf(m,"zram_rdelay[%d]: %lld \n", i, (zram?atomic64_read(&zram->stats.decompress_delay[i]):0ULL));
					seq_printf(m,"zram_rpages[%d]: %lld \n", i, (zram?atomic64_read(&zram->stats.decompress_pages[i]):0ULL));
				}
				atomic64_add(atomic64_read(&zram->stats.decompress_delay[i]), &decompress_delay);
				atomic64_add(atomic64_read(&zram->stats.decompress_pages[i]), &decompress_pages);
			}

			if (atomic64_read(&zram->stats.recompress_pages[i]) > 0) {
				seq_printf(m,"zram_recomp_delay[%d]: %lld \n", i, (zram?atomic64_read(&zram->stats.recompress_delay[i]):0ULL));
				seq_printf(m,"zram_recomp_pages[%d]: %lld \n", i, (zram?atomic64_read(&zram->stats.recompress_pages[i]):0ULL));
			}
		}
		seq_printf(m,"zram_wdelay: %lld \n", atomic64_read(&compress_delay));
		seq_printf(m,"zram_wpages: %lld \n", atomic64_read(&compress_pages));
		seq_printf(m,"zram_rdelay: %lld \n", atomic64_read(&decompress_delay));
		seq_printf(m,"zram_rpages: %lld \n", atomic64_read(&decompress_pages));
		if (atomic64_read(&zram->stats.wkdm_compr_pages_since) > 0) {
			seq_printf(m,"zram_wkdm_orig_size: %lld \n", atomic64_read(&zram->stats.wkdm_compr_pages_since) << PAGE_SHIFT);
			seq_printf(m,"zram_wkdm_comp_size: %lld \n", atomic64_read(&zram->stats.wkdm_compr_data_size));
		}
		if (atomic64_read(&zram->stats.lz4_compr_pages_since) > 0) {
			seq_printf(m,"zram_lz4_orig_size: %lld \n", atomic64_read(&zram->stats.lz4_compr_pages_since) << PAGE_SHIFT);
			seq_printf(m,"zram_lz4_comp_size: %lld \n", atomic64_read(&zram->stats.lz4_compr_data_size));
		}
		if (atomic64_read(&zram->stats.skip_switch_alg_pages) > 0)
			seq_printf(m,"zram_skip_switch_alg_pages: %lld \n", atomic64_read(&zram->stats.skip_switch_alg_pages));
	}

	seq_printf(m,"rf_lt_10: %lld \n", (zram?PAGE2MB(atomic64_read(&zram->stats.rf_lt_10)):0ULL));
	seq_printf(m,"rf_lt_60: %lld \n", (zram?PAGE2MB(atomic64_read(&zram->stats.rf_lt_60)):0ULL));
	seq_printf(m,"rf_lt_300: %lld \n", (zram?PAGE2MB(atomic64_read(&zram->stats.rf_lt_300)):0ULL));
	seq_printf(m,"rf_lt_1800: %lld \n", (zram?PAGE2MB(atomic64_read(&zram->stats.rf_lt_1800)):0ULL));
	seq_printf(m,"rf_lt_3600: %lld \n", (zram?PAGE2MB(atomic64_read(&zram->stats.rf_lt_3600)):0ULL));
	seq_printf(m,"rf_lt_43200: %lld \n", (zram?PAGE2MB(atomic64_read(&zram->stats.rf_lt_43200)):0ULL));
	seq_printf(m,"rf_lt_86400: %lld \n", (zram?PAGE2MB(atomic64_read(&zram->stats.rf_lt_86400)):0ULL));
	seq_printf(m,"rf_gt_86400: %lld \n", (zram?PAGE2MB(atomic64_read(&zram->stats.rf_gt_86400)):0ULL));

	seq_printf(m,"io_lt_500: %lld \n", (zram?atomic64_read(&zram->stats.io_lt_500):0ULL));
	seq_printf(m,"io_lt_1000: %lld \n", (zram?atomic64_read(&zram->stats.io_lt_1000):0ULL));
	seq_printf(m,"io_lt_3000: %lld \n", (zram?atomic64_read(&zram->stats.io_lt_3000):0ULL));
	seq_printf(m,"io_lt_5000: %lld \n", (zram?atomic64_read(&zram->stats.io_lt_5000):0ULL));
	seq_printf(m,"io_lt_30000: %lld \n", (zram?atomic64_read(&zram->stats.io_lt_30000):0ULL));
	seq_printf(m,"io_lt_50000: %lld \n", (zram?atomic64_read(&zram->stats.io_lt_50000):0ULL));
	seq_printf(m,"io_gt_50000: %lld \n", (zram?atomic64_read(&zram->stats.io_gt_50000):0ULL));

	return 0;
}

static int bdev_rw_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, bdev_rw_data_show, inode->i_private);
}

const struct proc_ops bdev_rw_data_fops = {
	.proc_open		= bdev_rw_data_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= bdev_rw_data_write,
};

int zram_process_reclaim_stat(struct process_reclaim_stat *stat) {
	struct sysinfo sysinfo;
	struct zram* zram = g_backingdev;
	unsigned long pages[NR_LRU_LISTS];
	s64 *thrashing = fwk_thrashing;
	int lru;
	int ret = 0;

	if (!zram || !init_done(zram)) {
		return -1;
	}

    stat->log_enable = is_log_enable(ZRAM_COMPRESS_LOG);
	spin_lock(&fwk_thrash_lock);
	if (time_after64(get_jiffies_64(), next_update_jiffies+2*HZ)) {
		stat->thrash1 = thrashing[0] = 0;
		stat->thrash2 = thrashing[1] = 0;
		stat->thrash3 = thrashing[2] = 0;
	} else {
		stat->thrash1 = thrashing[0];
		stat->thrash2 = thrashing[1];
		stat->thrash3 = thrashing[2];
	}
	spin_unlock(&fwk_thrash_lock);

	si_meminfo(&sysinfo);
	/*swap_use unit: pages*/
	si_swapinfo(&sysinfo);

	stat->totalswap = sysinfo.totalswap << (PAGE_SHIFT - 10);
	stat->freeswap = sysinfo.freeswap << (PAGE_SHIFT - 10);
	stat->sharedram = sysinfo.sharedram << (PAGE_SHIFT - 10);

	/* same as zram physical used from dumpsys meminfo*/
	stat->zram_phy_used = zs_get_total_pages(zram->mem_pool) << (PAGE_SHIFT - 10);

	for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
		pages[lru] = global_node_page_state(NR_LRU_BASE + lru);

	stat->anon_active = pages[LRU_ACTIVE_ANON] << (PAGE_SHIFT - 10);
	stat->anon_inactive = pages[LRU_INACTIVE_ANON] << (PAGE_SHIFT - 10);
	stat->file_active = pages[LRU_ACTIVE_FILE] << (PAGE_SHIFT - 10);
	stat->file_inactive = pages[LRU_INACTIVE_FILE] << (PAGE_SHIFT - 10);
	stat->unevictable = pages[LRU_UNEVICTABLE] << (PAGE_SHIFT - 10);

	stat->fwk_compress_pages_since = atomic64_read(&fwk_compress_pages_since);
	stat->fwk_compress_freq_since = atomic64_read(&fwk_compress_freq_since);
	stat->fwk_compress_runtime_since = atomic64_read(&fwk_compress_runtime_since)/1000;
	stat->fwk_decompress_pages_since = atomic64_read(&fwk_decompress_pages_since);

	if (is_log_enable(ZRAM_COMPRESS_LOG))
	    pr_err("[MF]  [0]thrash1 : %u pages/s\n"
			"[MF]  [1]thrash2 : %u pages/s\n"
			"[MF]  [2]thrash3 : %u pages/s\n"
			"[MF]  [3]totalswap :%lu KB\n"
			"[MF]  [4]freeswap :%lu  KB\n"
			"[MF]  [5]sharedram :%lu KB\n"
			"[MF]  [6]zram_phy_used :%lu \n"
			"[MF]  [7]anon_active :%lu \n"
			"[MF]  [8]anon_inactive :%lu \n"
			"[MF]  [9]file_active :%lu \n"
			"[MF]  [10]file_inactive :%lu \n"
			"[MF]  [11]unevictable :%lu \n"
			"[MF]  [12]compress_pages_since :%lld \n"
			"[MF]  [13]compress_freq_since :%lld \n"
			"[MF]  [14]compress_runtime_since :%lld \n"
			"[MF]  [15]decompress_pages_since :%lld \n",
			stat->thrash1,	stat->thrash2,	stat->thrash3,	stat->totalswap,	stat->freeswap,
			stat->sharedram,	stat->zram_phy_used,	stat->anon_active,	stat->anon_inactive,
			stat->file_active,	stat->file_inactive, stat->unevictable,
			stat->fwk_compress_pages_since,	stat->fwk_compress_freq_since,
			stat->fwk_compress_runtime_since,	stat->fwk_decompress_pages_since);

	return ret;
}

MODULE_IMPORT_NS(MINIDUMP);
static int zram_stat_show(struct seq_file *m, void *v)
{
	u64 compr_data_size, huge_pages_since, pages_store_since, accurate_org_pages;
	unsigned long mem_used = 0;
	struct sysinfo sysinfo;
	unsigned long swap_use;
	struct zram* zram = g_backingdev;

	if (!zram || !init_done(zram)) {
		seq_printf(m, "Not ready\n");
		return 0;
	}

	accurate_org_pages  = (u64)atomic64_read(&zram->stats.meta_data_size);
	compr_data_size = (u64)atomic64_read(&zram->stats.compr_data_size);

	/*no. of huge pages since zram set up, it's safe to reset pages_store_since & huge_pages_since after read*/
	huge_pages_since = (u64)atomic64_read(&zram->stats.huge_pages_since);
	pages_store_since = (u64)atomic64_read(&zram->stats.num_writes) - atomic64_read(&zram->stats.failed_writes);

	/* (init_done(zram)) is not necessary, being able to run here indicates that zram has been initialized.*/
	mem_used = zs_get_total_pages(zram->mem_pool);

	/*swap_use unit: pages*/
	si_swapinfo(&sysinfo);
	swap_use = (sysinfo.totalswap > sysinfo.freeswap)?(sysinfo.totalswap - sysinfo.freeswap):0U;

	/*unit:
	 * eg: ZRAM:     1,060K physical used for     3,584K in swap (4,206,388K total swap)
	 * DumpOriginSize 	same with swap used show in dumpsys meminfo (eg: 3,584K)
	 * DumpComprSize 	same with physical used show in dumpsys meminfo (eg: 1,060K)
	 *
	 * PageStoreSince 	no. of pages write since zram set up
	 * UnCompressSince  no. of huge pages since zram set up
	 *
	 * PureOriginSize   Bytes   origin size without ZRAM_SAME/ZRAM_WB/IDLE_HUGE/DEDUP
	 * PureComprSize    Bytes   currently compressed size in zram
	 */
	seq_printf(m,   "DumpOriginSize:    %-18lu KB\n"
			"DumpComprSize:     %-18lu KB\n"
			"PagesStoredSince:  %-18llu Pages\n"
			"HugePagesSince:    %-18llu Pages\n"
			"PureOriginSize:    %-18llu Bytes\n"
			"PureComprSize:     %-18llu Bytes\n",
			swap_use << (PAGE_SHIFT - 10),
			mem_used << (PAGE_SHIFT - 10),
			pages_store_since,
			huge_pages_since,
			accurate_org_pages << PAGE_SHIFT,
			compr_data_size);

	return 0;
}


static int zram_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_stat_show, inode->i_private);
}

const struct proc_ops zram_stat_fops = {
	.proc_open		= zram_stat_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
};

static ssize_t zram_wb_switch_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[10]={'\0'};
	int zram_wb_switch_temp = 0;

	if (count >= 10) {
		return count;
	}
	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("memfusion2_2:get zram_wb_switch para failed, please try again!\n");
			return -EFAULT;
		}
		if (sscanf(buf, "%d",&zram_wb_switch_temp) == 1)
		{
			if(zram_wb_switch_temp == 1)
			{
				// Allow to start zram_writeback
				skip_wakeup = true;
				atomic_set(&zram_wbd_switch, 1);
				try_wakeup_zram_wbd_lite(g_backingdev, zram_wb_multi);
			}
			else if(zram_wb_switch_temp == 0)
			{
				// Defuse the zram_writeback
				atomic_set(&zram_wbd_switch, 0);// break now zram_Wbd
			}
			else
			{
				pr_err("memfusion2_2:zram_wb_switch_temp is invalid, please try again!\n");
			}
		}
	}
	return count;
}

static int zram_wb_switch_show(struct seq_file *m, void *v)
{
	seq_printf(m, "zram_sreen_switch:%d wb_delay_time = %d\n",
				atomic_read(&zram_wbd_switch), zram_wb_switch_delay_time);
	return 0;
}

static int zram_wb_switch_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_wb_switch_show, inode->i_private);
}

const struct proc_ops zram_wb_switch_fops = {
	.proc_open		= zram_wb_switch_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_wb_switch_write,
};

static ssize_t zram_wb_actime_limit_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[30]={'\0'};
	int zram_wb_actime_limit_temp = 0;

	if (count >= 30) {
		return count;
	}
	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("memfusion2_2:get zram_wb_actime_limit para failed, please try again!\n");
			return -EFAULT;
		}
		if (sscanf(buf, "%d", &zram_wb_actime_limit_temp) == 1)
		{
			if(zram_wb_actime_limit_temp < 0)
			{
				pr_err("memfusion2_2:zram_wb_actime_limit_temp is invalid, please try again!\n");
			}
			else
			{
				zram_kernel_wb_actime_limit = zram_wb_actime_limit_temp;
				pr_err("memfusion2_2:zram_kernel_wb_actime_limit = %d\n",zram_kernel_wb_actime_limit);
			}
		}
	}
	return count;
}

static int zram_wb_actime_limit_show(struct seq_file *m, void *v)
{
	seq_printf(m,"memfusion2_2:zram_kernel_wb_actime_limit %d\n", zram_kernel_wb_actime_limit);
	return 0;
}

static int zram_wb_actime_limit_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_wb_actime_limit_show, inode->i_private);
}

const struct proc_ops zram_wb_actime_limit_fops = {
	.proc_open		= zram_wb_actime_limit_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_wb_actime_limit_write,
};

static ssize_t zram_psi_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[30]={'\0'};
	int zram_psi_temp = 0;

	if (count >= 30) {
		return count;
	}
	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("memfusion2_2:get zram_wb_actime_limit para failed, please try again!\n");
			return -EFAULT;
		}
		if (sscanf(buf, "%d", &zram_psi_temp) == 1)
		{
			if(zram_psi_temp > 0)
			{
				pr_err("[Memfusion2_2][%d]PSI test start, Now time = %u\n",zram_psi_temp, jiffies_to_msecs(jiffies));
				memfusion_psi_io(&g_backingdev->wbd_psi_data, 0, true);
				memfusion_psi_mem(&g_backingdev->wbd_psi_data, 0, true);
				memfusion_psi_cpu(&g_backingdev->wbd_psi_data, g_backingdev->wbd->se.sum_exec_runtime, true);
			}
			else
			{
				psi_show(&g_backingdev->wbd_psi_data);
			}
		}
	}
	return count;
}

static int zram_psi_show(struct seq_file *m, void *v)
{
	psi_show_file(m, &g_backingdev->wbd_psi_data);
	return 0;
}

static int zram_psi_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_psi_show, inode->i_private);
}

const struct proc_ops zram_psi_fops = {
	.proc_open		= zram_psi_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_psi_write,
};

static ssize_t zram_wb_count_limit_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[30]={'\0'};
	int zram_wb_count_limit_temp = 0;

	if (count >= 30) {
		return count;
	}
	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("memfusion2_2:get zram_wb_count_limit para failed, please try again!\n");
			return -EFAULT;
		}
		if (sscanf(buf, "%d", &zram_wb_count_limit_temp) == 1)
		{
			if(zram_wb_count_limit_temp < 0)
			{
				pr_err("memfusion2_2:zram_wb_count_limit_temp is invalid, please try again!\n");
			}
			else
			{
				zram_kernel_wb_count_limit = zram_wb_count_limit_temp;
				pr_err("memfusion2_2:zram_kernel_wb_count_limit = %d\n", zram_kernel_wb_count_limit);
			}
		}
	}
	return count;
}

static int zram_wb_count_limit_show(struct seq_file *m, void *v)
{
	seq_printf(m,"memfusion2_2:zram_kernel_wb_count_limit = %d\n", zram_kernel_wb_count_limit);
	return 0;
}

static int zram_wb_count_limit_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_wb_count_limit_show, inode->i_private);
}

const struct proc_ops zram_wb_count_limit_fops = {
	.proc_open		= zram_wb_count_limit_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_wb_count_limit_write,
};

#define zram_format_data(buf, format, data1, data2, left, copied, written) {\
	if (left > 96) {	\
		copied = snprintf(buf+written, left, format, data1, data2);\
		written += copied;\
		left -= copied;\
	}\
}

/* we only count no. pages of objsize>0, ZRAM_WB, exclude ZRAM_SAME */
static inline bool zram_stats_allocated(struct zram *zram, u32 index)
{
	return zram_get_obj_size(zram, index) || zram_test_flag(zram, index, ZRAM_WB);
}

struct zram_cold_pages{
	unsigned long num_cold8h;
	unsigned long num_cold16h;
	unsigned long num_cold24h;
};
static int zram_calc_cold_page(struct zram_cold_pages* cold,
									struct zram* zram)
{
	int ret = 0;
	u32 index;
	unsigned long nr_pages;
	ktime_t current_ts;
	s64 delta;

	down_read(&zram->init_lock);
	current_ts = ktime_get_boottime();

	if (!init_done(zram)) {
		ret = -EINVAL;
		goto release_init_lock;
	}

	if (!zram->backing_dev) {
		ret = -ENODEV;
		goto release_init_lock;
	}

	nr_pages = zram->disksize >> PAGE_SHIFT;
	for (index = 0; index < nr_pages; index++) {
		zram_slot_lock(zram, index);
		if (!zram_stats_allocated(zram, index)) {
			zram_slot_unlock(zram, index);
			continue;
		}
		delta = ktime_ms_delta(current_ts, zram->table[index].ac_time);
		zram_slot_unlock(zram, index);
		if (delta >= ZRAM_24H_TO_MS) {
			cold->num_cold24h++;
			cold->num_cold16h++;
			cold->num_cold8h++;
		} else if (delta >= ZRAM_16H_TO_MS) {
			cold->num_cold16h++;
			cold->num_cold8h++;
		} else if (delta >= ZRAM_8H_TO_MS) {
			cold->num_cold8h++;
		}
	}
release_init_lock:
	up_read(&zram->init_lock);

	return ret;
}

static void zram_format_swap_refault_delay(struct zram* zram, char *buf, int left) {
	int copied = 0;
	int written = 0;

	memset(buf, 0, left);

	/*stay 0~10s */
	zram_emem_format(buf, "%llu,", PAGE2MB(atomic64_read(&zram->stats.rf_lt_10)), left, copied, written);
	atomic64_set(&zram->stats.rf_lt_10, 0);

	/*stay 10~60s */
	zram_emem_format(buf, "%llu,", PAGE2MB(atomic64_read(&zram->stats.rf_lt_60)), left, copied, written);
	atomic64_set(&zram->stats.rf_lt_60, 0);

	/*stay 60~300s */
	zram_emem_format(buf, "%llu,", PAGE2MB(atomic64_read(&zram->stats.rf_lt_300)), left, copied, written);
	atomic64_set(&zram->stats.rf_lt_300, 0);

	/*stay 300~1800 */
	zram_emem_format(buf, "%llu,", PAGE2MB(atomic64_read(&zram->stats.rf_lt_1800)), left, copied, written);
	atomic64_set(&zram->stats.rf_lt_1800, 0);

	/*stay 1800~3600s */
	zram_emem_format(buf, "%llu,", PAGE2MB(atomic64_read(&zram->stats.rf_lt_3600)), left, copied, written);
	atomic64_set(&zram->stats.rf_lt_3600, 0);

	/*stay 3600~43200s */
	zram_emem_format(buf, "%llu,", PAGE2MB(atomic64_read(&zram->stats.rf_lt_43200)), left, copied, written);
	atomic64_set(&zram->stats.rf_lt_43200, 0);

	/*stay 43200~86400s */
	zram_emem_format(buf, "%llu,", PAGE2MB(atomic64_read(&zram->stats.rf_lt_86400)), left, copied, written);
	atomic64_set(&zram->stats.rf_lt_86400, 0);

	/*stay 86400~unlimited */
	zram_emem_format(buf, "%llu", PAGE2MB(atomic64_read(&zram->stats.rf_gt_86400)), left, copied, written);
	atomic64_set(&zram->stats.rf_gt_86400, 0);

	return;
}

static void zram_format_io_delay(struct zram* zram, char *buf, int left) {
	int copied = 0;
	int written = 0;

	/* 0~500 us/page */
	zram_emem_format(buf, "%llu,", atomic64_read(&zram->stats.io_lt_500), left, copied, written);
	atomic64_set(&zram->stats.io_lt_500, 0);

	/*500~1000 us/page */
	zram_emem_format(buf, "%llu,", atomic64_read(&zram->stats.io_lt_1000), left, copied, written);
	atomic64_set(&zram->stats.io_lt_1000, 0);

	/*1000~3000 us/page */
	zram_emem_format(buf, "%llu,", atomic64_read(&zram->stats.io_lt_3000), left, copied, written);
	atomic64_set(&zram->stats.io_lt_3000, 0);

	/*3000~5000 us/page */
	zram_emem_format(buf, "%llu,", atomic64_read(&zram->stats.io_lt_5000), left, copied, written);
	atomic64_set(&zram->stats.io_lt_5000, 0);

	/*5000~30000 us/page */
	zram_emem_format(buf, "%llu,", atomic64_read(&zram->stats.io_lt_30000), left, copied, written);
	atomic64_set(&zram->stats.io_lt_30000, 0);

	/*30000~50000 us/page */
	zram_emem_format(buf, "%llu,", atomic64_read(&zram->stats.io_lt_50000), left, copied, written);
	atomic64_set(&zram->stats.io_lt_50000, 0);

	/*50000~unlimited us/page */
	zram_emem_format(buf, "%llu", atomic64_read(&zram->stats.io_gt_50000), left, copied, written);
	atomic64_set(&zram->stats.io_gt_50000, 0);

	return;
}

static int zram_send_bigdata(struct zram* zram) {
	struct athena_msg *msg = NULL;
	ktime_t now;
	s64 count = 0;
	int ret = 0;
	int written = 0;
	int left = ZRAM_SEND_MAX_LEN;
	int copied = 0;
	s64 new_writes, new_reads, new, old, delta, duration;
	int num;
	struct zram_cold_pages cold;
	struct wbd_stats* wbd_stat;
	char buf[ZRAM_SEND_MAX_LEN];
	struct timespec64 ts;
	char data_write[ZRAM_HOUR_BUF_LEN] = {0};
	unsigned long swapused, anon, mem_used, compress, tmp, swapin, delaytime;

	if (!zram || !init_done(zram)) {
		pr_err("memfusion is not ready yet.");
		return ret;
	}

	/*
	 * For MTK platform, data must be reported X seconds after startup.
	 * For Unisoc platform, please delete this.
	 */
	now = ktime_get_boottime();
	ts = ktime_to_timespec64(now);
	if (ts.tv_sec < zram_kernel_wb_actime_limit) {
		pr_err("Reporting must be %d sec after startup. now is %lld",
			zram_kernel_wb_actime_limit, ts.tv_sec);
		return ret;
	}

	wbd_stat = &zram->wbd_stat;
	duration = ktime_ms_delta(now, wbd_stat->last_report);
	/* Reporting interval must over 20h */
	if (duration < zram_wbd_report) {
		pr_err("Reporting interval time has not reached condition. %lld",
			duration);
		return ret;
	}

	msg = alloc_msg();
	if (!msg) {
		pr_err("Reporting failed due to nomem.");
		return ret;
	}

	memset(buf, 0, ZRAM_SEND_MAX_LEN);
	/* Bdev Total Size */
	num = ui_swapsize;
	msg_put_int(&msg, "bts", num);
	zram_format_data(buf, "bts  : %-6d MB %d\n", num, 0, left, copied, written);

	/* Bdev Actual Size */
	num = (int)(zram->nr_pages_total >> 8);
	msg_put_int(&msg, "bas", num);
	zram_format_data(buf, "bas  : %-6d MB %d\n", num, 0, left, copied, written);

	/* Bdev Used (must send) */
	if (is_memcg_compact_v2)
		num = (int)(eswap_get_used_ext_pages() << (PAGE_SHIFT - 10));
	else
		num = (int)(atomic64_read(&zram->stats.bd_count) << (PAGE_SHIFT - 10));
	msg_put_int(&msg, "bu", num);
	zram_format_data(buf, "bu   : %-6d KB %d\n", num, 0, left, copied, written);

	/* Bdev ReadBack Size (must exist) */
	old = wbd_stat->old.bd_reads;
	if (is_memcg_compact_v2)
		new = atomic64_read(&zram->stats.bd_reads);
	else
		new = atomic64_read(&zram->stats.swapin_count);
	wbd_stat->old.bd_reads = new;
	delta = (new - old) << (PAGE_SHIFT - 10);
	msg_put_int(&msg, "brbs", (int)delta);
	zram_format_data(buf, "brbs : %-6lld KB T:%-6lld KB\n",
		delta, new<<(PAGE_SHIFT-10), left, copied, written);

	/* FG Process Readback Size per-day (must send) */
	old = wbd_stat->old.fg_reads;
	new = atomic64_read(&wbd_stat->fg_reads);
	wbd_stat->old.fg_reads = new;
	delta = (new - old) <<(PAGE_SHIFT - 10);
	msg_put_int(&msg, "fgrs", (int)(delta>0?delta:0));
	zram_format_data(buf, "fgrs : %-6lld KB T:%-6lld KB\n",
		delta, new<<(PAGE_SHIFT-10), left, copied, written);

	/* Important Process Readback Size (must send) */
	old = wbd_stat->old.key_process_reads;
	new = atomic64_read(&wbd_stat->key_process_reads);
	wbd_stat->old.key_process_reads = new;
	delta = (new - old) <<(PAGE_SHIFT - 10);
	msg_put_int(&msg, "iprs", (int)(delta>0?delta:0));
	zram_format_data(buf, "iprs : %-6lld KB T:%-6lld KB\n",
		delta, new<<(PAGE_SHIFT - 10), left, copied, written);

	/* Surfaceflinger Thread Readback Size (must send) */
	old = wbd_stat->old.surface_reads;
	new = atomic64_read(&wbd_stat->surface_reads);
	wbd_stat->old.surface_reads = new;
	delta = (new - old)<<(PAGE_SHIFT - 10);
	msg_put_int(&msg, "srs", (int)(delta>0?delta:0));
	zram_format_data(buf, "srs  : %-6lld KB T:%-6lld KB\n",
		delta, new<<(PAGE_SHIFT - 10), left, copied, written);
	count = delta;

	/* Render Thread Readback Size (must send) */
	old = wbd_stat->old.render_reads;
	new = atomic64_read(&wbd_stat->render_reads);
	wbd_stat->old.render_reads = new;
	delta = (new - old)<<(PAGE_SHIFT - 10);
	msg_put_int(&msg, "rrs", (int)(delta>0?delta:0));
	zram_format_data(buf, "rrs  : %-6lld KB T:%-6lld KB\n",
		delta, new<<(PAGE_SHIFT - 10), left, copied, written);
	count += delta;

	/* Binder Thread Readback Size (must send) */
	old = wbd_stat->old.binder_reads;
	new = atomic64_read(&wbd_stat->binder_reads);
	wbd_stat->old.binder_reads = new;
	delta = (new - old)<<(PAGE_SHIFT - 10);
	count += delta;
	msg_put_int(&msg, "brs", (int)(delta>0?delta:0));
	zram_format_data(buf, "brs  : %-6lld KB T:%-6lld KB\n",
		delta, new<<(PAGE_SHIFT - 10), left, copied, written);

	/* UI Thread Readback Size (must send) */
	old = wbd_stat->old.ui_reads;
	new = atomic64_read(&wbd_stat->ui_reads);
	wbd_stat->old.ui_reads = new;
	delta = (new - old)<<(PAGE_SHIFT - 10);
	count += delta;
	msg_put_int(&msg, "urs", (int)(delta>0?delta:0));
	zram_format_data(buf, "urs  : %-6lld KB T:%-6lld KB\n",
		delta, new<<(PAGE_SHIFT - 10), left, copied, written);

	/* Composer Thread Readback Size (must send) */
	old = wbd_stat->old.composer_reads;
	new = atomic64_read(&wbd_stat->composer_reads);
	wbd_stat->old.composer_reads = new;
	delta = (new - old)<<(PAGE_SHIFT - 10);
	count += delta;
	msg_put_int(&msg, "crs", (int)(delta>0?delta:0));
	zram_format_data(buf, "crs  : %-6lld KB T:%-6lld KB\n",
		delta, new<<(PAGE_SHIFT - 10), left, copied, written);

	/* Draw Thread Read Size (must send) */
	msg_put_int(&msg, "dtrs", (int)count);
	wbd_stat->old.draw_reads += count;
	zram_format_data(buf, "dtrs : %-6lld KB T:%lld KB\n",
		count, wbd_stat->old.draw_reads, left, copied, written);

	/* Memfusion cpu (must send) */
	count = zram->wbd->se.sum_exec_runtime;
	delta = count - wbd_stat->old.sum_exec_runtime;
	wbd_stat->old.sum_exec_runtime = count;
	do_div(delta, ZRAM_NS_PER_MS);
	msg_put_int(&msg, "mfc", (int)delta);
	zram_format_data(buf, "mfc  : %-6lld ms T:%-6lld ns\n",
		delta, count, left, copied, written);

	/* Memfusion Readback IO (must send) */
	old = wbd_stat->old.swapin_delay;
	new = atomic64_read(&zram->stats.swapin_delay);
	wbd_stat->old.swapin_delay = new;
	delta = new - old;
	do_div(delta, ZRAM_US_PER_MS);
	msg_put_int(&msg, "mfri", (int)delta);
	zram_format_data(buf, "mfri: %-6lld ms T:%-6lld us\n",
	delta, atomic64_read(&zram->stats.swapin_delay), left, copied, written);

	/* Draw Thread Readback IO (must send) */
	old = wbd_stat->old.draw_io_delay;
	new = atomic64_read(&zram->stats.draw_io_delay);
	wbd_stat->old.draw_io_delay = new;
	delta = new - old;
	do_div(delta, ZRAM_US_PER_MS);
	msg_put_int(&msg, "msfc", (int)delta);
	zram_format_data(buf, "msfc: %-6lld ms T:%-6lld us\n",
	delta, atomic64_read(&zram->stats.draw_io_delay), left, copied, written);

	/* Memfusion thrashing */
	new_reads = atomic64_read(&zram->stats.bd_reads);
	new_writes = atomic64_read(&zram->stats.bd_writes);
	delta = new_reads*1000;
	if (new_writes > 0 && new_writes > new_reads) {
		do_div(delta, new_writes);
	} else {
		delta = 0;
	}
	msg_put_int(&msg, "mft", (int)delta);
	zram_format_data(buf, "mft : %lld/1000 T:%lld\n",
		delta, new_reads, left, copied, written);

	/* Swap thrashing (zram) */
	new_reads = atomic64_read(&zram->stats.num_reads);
	new_writes = atomic64_read(&zram->stats.num_writes);
	delta = new_reads*1000;
	if (new_writes > 0 && new_writes > new_reads) {
		do_div(delta, new_writes);
	} else {
		delta = 0;
	}
	msg_put_int(&msg, "st", (int)delta);
	zram_format_data(buf, "st : %d/1000 T:%lld\n",
		(int)delta, new_reads<<(PAGE_SHIFT-10), left, copied, written);

	/* Zram Cold Page(zram) */
	memset(&cold, 0, sizeof(cold));
	zram_calc_cold_page(&cold, zram);

	msg_put_int(&msg, "zcpe", cold.num_cold8h>>8);
	zram_format_data(buf, "zcpe: %-6lu MB T:%-6d MB\n",
		cold.num_cold8h>>8, 0, left, copied, written);

	msg_put_int(&msg, "zcps", cold.num_cold16h>>8);
	zram_format_data(buf, "zcps: %-6lu MB T:%-6d MB\n",
		cold.num_cold16h>>8, 0, left, copied, written);

	msg_put_int(&msg, "zcpo", cold.num_cold24h>>8);
	zram_format_data(buf, "zcpo: %-6lu MB T:%-6d MB\n",
		cold.num_cold24h>>8, 0, left, copied, written);

	/* Memfusion PSI CPU */
	num = PSI_LOAD_INT(zram->wbd_psi_data.psi_max[PSI_CPU]) * 100 + PSI_LOAD_FRAC(zram->wbd_psi_data.psi_max[PSI_CPU]);
	msg_put_int(&msg, "mpc", num);
	zram_format_data(buf, "mpc : %-6d T:%-6d\n", num, 0, left, copied, written);

	/* Memfusion PSI IO */
	num = PSI_LOAD_INT(zram->wbd_psi_data.psi_max[PSI_IO]) * 100 + PSI_LOAD_FRAC(zram->wbd_psi_data.psi_max[PSI_IO]);
	msg_put_int(&msg, "mpi", num);
	zram_format_data(buf, "mpi : %-6d T:%-6d\n", num, 0, left, copied, written);

	/* Memfusion PSI MEM */
	num = PSI_LOAD_INT(zram->wbd_psi_data.psi_max[PSI_MEM]) * 100 + PSI_LOAD_FRAC(zram->wbd_psi_data.psi_max[PSI_MEM]);
	msg_put_int(&msg, "mpm", num);
	zram_format_data(buf, "mpm : %-6d T:%-6d\n", num, 0, left, copied, written);

	/* Memfusion Invalid Count Reason */
	if (zram_bdev_install_error > ZRAM_BDEV_INSTALL_BDEV_ERROR)
		zram_bdev_install_error = ZRAM_BDEV_INSTALL_BDEV_ERROR;
	msg_put_int(&msg, "mir", zram_bdev_install_error);
	zram_format_data(buf, "mir : %-6d T:%-6d\n",
		zram_bdev_install_error, 0, left, copied, written);

	/* Memfusion 24h read count */
	format_hour_readback(data_write, ZRAM_HOUR_BUF_LEN, true);
	msg_put_string(&msg, "rsa", data_write);

	swapused = PAGE2MB(atomic64_read(&zram->stats.pages_stored));
	anon = PAGE2MB(global_node_page_state(NR_ACTIVE_ANON) + global_node_page_state(NR_INACTIVE_ANON));
	/* System Total Compressible Anon Page, unit is MB */
	msg_put_int(&msg, "stca", (int)anon);
	zram_emem_format(buf, "stca:%d;", (int)anon, left, copied, written);

	/* System Compression Proportion */
	tmp = anon+swapused;
	if (tmp > 0)
		tmp = (swapused*100/tmp);
	msg_put_int(&msg, "scp", (int)tmp);
	zram_emem_format(buf, "scp:%d;", (int)tmp, left, copied, written);

	/* Swap Used , unit is MB */
	msg_put_int(&msg, "swu", (int)swapused);
	zram_emem_format(buf, "swu:%d;", (int)swapused, left, copied, written);

	/* SwapTotal , unit is MB */
	tmp = zram->disksize >> 20;
	snprintf(data_write, ZRAM_HOUR_BUF_LEN, "%lu", tmp);
	msg_put_string(&msg, "rsb", data_write);
	zram_emem_format(buf, "rsb:%d;", (int)tmp, left, copied, written);

	/*
	 * Zram Compress Ratio
	 * eg: ZRAM:	 1,060K physical used for	  3,584K in swap (4,206,388K total swap)
	 * 1060K is mem_used, 3,584K
	 */
	compress = mem_used = 0;
	if (init_done(zram)) {
		mem_used = PAGE2MB(zs_get_total_pages(zram->mem_pool));
		compress = (mem_used > 0)?(swapused*100 / mem_used):0UL;
	}
	msg_put_int(&msg, "zcpr", (int)compress);
	zram_emem_format(buf, "zcpr:%d;", (int)compress, left, copied, written);

	/* App Save Ratio */
	tmp = compress * (swapused + anon);
	if (tmp > 0)
		tmp = (1000*swapused*(compress - 100)/tmp);
	msg_put_int(&msg, "asr", (int)tmp);
	zram_emem_format(buf, "asr:%d;", (int)tmp, left, copied, written);

	/* Compressed Pages In Bdev, unit is MB */
	num = (int)PAGE2MB(atomic64_read(&zram->stats.bd_objcnt));
	msg_put_int(&msg, "cpib", num);
	zram_emem_format(buf, "cpib:%d;", num, left, copied, written);

	/* Bdev Actual Saved Physical Memory, unit is MB */
	num = (int)(atomic64_read(&zram->stats.bd_size)>>20);
	msg_put_int(&msg, "basm", num);
	zram_emem_format(buf, "basm:%d;", num, left, copied, written);

	/* Swap refault time distribution */
	zram_format_swap_refault_delay(zram, data_write, ZRAM_HOUR_BUF_LEN);
	msg_put_string(&msg, "srtd", data_write);
	zram_emem_format(buf, "srtd:%s;", data_write, left, copied, written);

	/* Memfusion Write IO */
	if (is_memcg_compact_v2) {
		old = atomic64_read(&zram->stats.swapout_count);
		tmp = (old > 0)?(atomic64_read(&zram->stats.swapout_delay)/old):0ULL;
	} else {
		tmp = (bdev_write_page_count>0)?(bdev_write_page_time/bdev_write_page_count):0ULL;
	}
	msg_put_int(&msg, "mwio", tmp);
	zram_emem_format(buf, "mwio:%lu;", tmp, left, copied, written);

	/* Memfusion Read IO */
	delaytime = atomic64_read(&zram->stats.swapin_delay);
	swapin = atomic64_read(&zram->stats.swapin_count);
	tmp = (swapin > 0)?(delaytime/swapin):0ULL;
	msg_put_int(&msg, "mrio", tmp);
	zram_emem_format(buf, "mrio:%lu;", tmp, left, copied, written);

	/* Memfusion IO Delay Distribution */
	zram_format_io_delay(zram, data_write, ZRAM_HOUR_BUF_LEN);
	msg_put_string(&msg, "midd", data_write);
	zram_emem_format(buf, "midd:%s;", data_write, left, copied, written);

	submit_msg(&msg, 3, MEMFUSION22_TID, "memfusion");
	free_msg(&msg);

	zram->wbd_stat.last_report = ktime_get_boottime();
	duration = ktime_us_delta(zram->wbd_stat.last_report, now);

	zram_format_data(buf, "time : %lld %d\n",
		ktime_to_ms(zram->wbd_stat.last_report), 0, left, copied, written);

	zram_format_data(buf, "duration[%lld us] leftspace[:%d]\n",
		duration, left, left, copied, written);

	buf[ZRAM_SEND_MAX_LEN-1] = '\0';
	pr_err("Reporting finish:\n%s\n", buf);

	/* Wake up the zram thread to write the page back to ROM*/
	if (is_memfusion)
		try_wakeup_zram_wbd_lite(g_backingdev, ZRAM_WB_ALL);

	return ret;
}

static int zram_stats_spec_pages(unsigned int *nr_cold_pages, unsigned int* nr_key_pages)
{
	int ret = 0;
	u32 index;
	unsigned long nr_pages;
	ktime_t current_ts;
	s64 delta;
	unsigned int total_cold =0;
	unsigned int key_pages = 0;
	bool iskey;
	struct zram *zram = g_backingdev;

	if (!zram) {
		return -EINVAL;
	}
	down_read(&zram->init_lock);
	current_ts = ktime_get_boottime();

	if (!init_done(zram)) {
		ret = -EINVAL;
		goto release_init_lock;
	}

	if (!zram->backing_dev) {
		ret = -ENODEV;
		goto release_init_lock;
	}

	nr_pages = zram->disksize >> PAGE_SHIFT;
	for (index = 0; index < nr_pages; index++) {
		zram_slot_lock(zram, index);
		if (!zram_stats_allocated(zram, index)) {
			zram_slot_unlock(zram, index);
			continue;
		}
		delta = ktime_ms_delta(current_ts, zram->table[index].ac_time);
		iskey = zram_test_flag(zram, index, ZRAM_KEY);
		zram_slot_unlock(zram, index);
		if (delta >= zram_stats_actime*1000) {
			total_cold++;
			if (iskey)
				key_pages++;
		}
	}
release_init_lock:
	up_read(&zram->init_lock);

	if (nr_cold_pages)
		*nr_cold_pages = total_cold;

	if (nr_key_pages)
		*nr_key_pages = key_pages;

	return ret;
}

/* wbd running status information  */
static int zram_run_stat_show(struct seq_file *m, void *v)
{
	unsigned int nr_cold_pages = 0;
	unsigned int nr_key_pages = 0;
	struct process_reclaim_stat stat;
	int i, offset = 0;
	char buf[SZ_128] = {'\0'};

	zram_stats_spec_pages(&nr_cold_pages, &nr_key_pages);

	seq_printf(m, "%-20s %-16lld\n",  "report", g_backingdev?ktime_to_ms(g_backingdev->wbd_stat.last_report):0);
	seq_printf(m, "%-20s %-16u\n",  "cold", nr_cold_pages<<(PAGE_SHIFT - 10));
	seq_printf(m, "%-20s %-16u\n",  "key-cold", nr_key_pages<<(PAGE_SHIFT - 10));
	seq_printf(m, "%-20s %-16u\n",  "stat-time", zram_stats_actime);

	memset(&stat, 0, sizeof(stat));
	zram_process_reclaim_stat(&stat);

	seq_printf(m, "%-20s %-16u\n",  "[0] thrash1", stat.thrash1);
	seq_printf(m, "%-20s %-16u\n",  "[1] thrash2", stat.thrash2);
	seq_printf(m, "%-20s %-16u\n",  "[2] thrash3", stat.thrash3);
	seq_printf(m, "%-20s %-16lu\n",  "[3] totalswap", stat.totalswap);
	seq_printf(m, "%-20s %-16lu\n",  "[4] freeswap", stat.freeswap);
	seq_printf(m, "%-20s %-16lu\n",  "[5] sharedram", stat.sharedram);
	seq_printf(m, "%-20s %-16lu\n",  "[6] zram_phy_used", stat.zram_phy_used);
	seq_printf(m, "%-20s %-16lu\n",  "[7] anon_active", stat.anon_active);
	seq_printf(m, "%-20s %-16lu\n",  "[8] anon_inactive", stat.anon_inactive);
	seq_printf(m, "%-20s %-16lu\n",  "[9] file_active", stat.file_active);
	seq_printf(m, "%-20s %-16lu\n",  "[10] file_inactive", stat.file_inactive);
	seq_printf(m, "%-20s %-16lu\n",  "[11] unevictable", stat.unevictable);
	seq_printf(m, "%-20s %-16lld\n",  "[12] cprs", stat.fwk_compress_pages_since<<(PAGE_SHIFT-10));
	seq_printf(m, "%-20s %-16lld\n",  "[13] cprs_freq", stat.fwk_compress_freq_since);
	seq_printf(m, "%-20s %-16lld\n",  "[14] cprs_runtime", stat.fwk_compress_runtime_since);
	seq_printf(m, "%-20s %-16lld\n",  "[15] dprs", stat.fwk_decompress_pages_since<<(PAGE_SHIFT-10));
	seq_printf(m, "%-20s %-16lld\n",  "[16] memcg_reclaim_v1_total_cnt", atomic64_read(&memrec_stat_v1.total_cnt));
	seq_printf(m, "%-20s %-16lld\n",  "[17] memcg_reclaim_v1_fail_cnt", atomic64_read(&memrec_stat_v1.fail_cnt));
	seq_printf(m, "%-20s %-16lld\n",  "[18] memcg_reclaim_v1_reclaimed_pages", atomic64_read(&memrec_stat_v1.reclaimed_pages));
	seq_printf(m, "%-20s %-16lld\n",  "[19] memcg_reclaim_v1_reclaimed_delay", atomic64_read(&memrec_stat_v1.reclaimed_delay));

	for (i = MEMORY_BK_RECLAIM_SIGNAL; i < MEMORY_BK_RECLAIM_MAX; i++) {
		offset += scnprintf(buf + offset, sizeof(buf) - offset, "%lld", atomic64_read(&memrec_stat_v1.abort_reason_cnt[i]));
		if (i < MEMORY_BK_RECLAIM_MAX - 1)
			offset += scnprintf(buf + offset, sizeof(buf) - offset, ",");
	}
	seq_printf(m, "%-20s %-16s\n",  "[20] memcg_reclaim_v1_abort_reason_cnt", buf);

	return 0;
}

static ssize_t zram_run_stat_write(struct file *file,const char __user *ubuf,size_t count, loff_t *pos)
{
	char buf[32]={'\0'};
	unsigned int type = 0;

	if (count >= 32) {
		return count;
	}

	if (count > 0) {
		if (copy_from_user(buf, ubuf, count)) {
			return -EFAULT;
		}
		sscanf(buf, "%d", &type);
		if (256 == type) {
			/* mean send data to normal server*/
			zram_send_bigdata(g_backingdev);
		}
	}

	return count;
}

static int zram_run_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_run_stat_show, inode->i_private);
}

const struct proc_ops zram_run_stat_fops = {
	.proc_open		= zram_run_stat_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_run_stat_write,
};

static ssize_t ui_swapsize_write(struct file *file,const char __user *ubuf,size_t count, loff_t *pos)
{
	char buf[32]={'\0'};
	unsigned int size = 0;

	if (count >= 32) {
		return count;
	}

	if (count > 0) {
		if (copy_from_user(buf, ubuf, count)) {
			return -EFAULT;
		}
		sscanf(buf, "%d", &size);
		ui_swapsize = size;
		//g_swap_kernel_write_enable = 1;
		//g_swap_fwk_write_enable = 1;
		//pr_err(MF22PRELOG"set kernel/fwk enable");
	}

	return count;
}

/* ui swapsize */
static int ui_swapsize_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u", ui_swapsize);
	return 0;
}

static int ui_swapsize_open(struct inode *inode, struct file *file)
{
	return single_open(file, ui_swapsize_show, inode->i_private);
}

const struct proc_ops ui_swapsize_fops = {
	.proc_open		= ui_swapsize_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= ui_swapsize_write,
};

static ssize_t switch_cnt_write(struct file *file,const char __user *ubuf,size_t count, loff_t *pos)
{
	char buf[32]={'\0'};
	unsigned int cnt = 0;

	if (count >= 32) {
		return count;
	}

	if (count > 0) {
		if (copy_from_user(buf, ubuf, count)) {
			return -EFAULT;
		}
		sscanf(buf, "%d", &cnt);
		ui_switch_fail_cnt += cnt;
	}

	return count;
}

/* switch cnt */
static int switch_cnt_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u", ui_switch_fail_cnt);
	return 0;
}

static int switch_cnt_open(struct inode *inode, struct file *file)
{
	return single_open(file, switch_cnt_show, inode->i_private);
}

const struct proc_ops switch_cnt_fops = {
	.proc_open		= switch_cnt_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= switch_cnt_write,
};

/* key process pid */
static int zram_key_pid_show(struct seq_file *m, void *v)
{
	int index;
	pid_t pid;
	struct task_struct *task, *group_leader;

	seq_printf(m, "tgid comm mainthread max[%d]\n", KEY_MAX_PROCESS);
	for (index = 0; index < KEY_MAX_PROCESS; index++) {
		pid = zram_key_task_list[index].tgid;
		if (pid > 0) {
			task = pid_task(find_get_pid(pid), PIDTYPE_PID);
			if (task) {
				group_leader = pid_task(find_get_pid(task->tgid), PIDTYPE_PID);
				if (group_leader)
					seq_printf(m, "%d %s %s\n", pid, task->comm, group_leader->comm);
				else
					seq_printf(m, "not found pid:%d\n", pid);
			}
		}
	}

	return 0;
}

static int zram_key_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_key_pid_show, inode->i_private);
}

const struct proc_ops zram_key_pid_fops = {
	.proc_open		= zram_key_pid_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
};

static void should_cancel_madvise(void* data, struct vm_area_struct *vma, bool* abort_madvise)
{
	enum zram_cancel_madvise reason = ZRAM_CANCEL_BY_NONE;
	unsigned long total, anon, swap;
	struct mm_struct *mm = vma->vm_mm;
	s64 delta_time, delta_compressed;
	struct task_struct* target_task = NULL;
	struct zram_compress_single *params;
	bool frozen_state;

	if (unlikely(zram_skip_compress_abort)) {
		return;
	}

	if ((current != READ_ONCE(zram_compressed_task)) || !g_process_reclaim_enable)
		return;

	params = get_compress_params();
	target_task = params->target_task;
	if (target_task) {
		frozen_state = READ_ONCE(target_task->__state) & TASK_FROZEN;
		if (frozen_state != (params->frozen & 0x1)) {
			params->frozen_chg++;
			params->frozen = ((params->frozen << 1 ) | frozen_state);
		}
	}

	/*
	 * If the compress thread is running and we receive the
	 * abort event, we will send SIGUSR2 singnal to teriminate
	 * the compress thread. So if there is a SIGUSR2 signal in
	 * current thread, abort madvise.
	 */
	if (signal_pending(current) &&
		(sigismember(&current->signal->shared_pending.signal, SIGUSR2) ||
		 sigismember(&current->pending.signal, SIGUSR2))) {
		if (is_log_enable(ZRAM_COMPRESS_LOG))
			pr_err("comm:%s compress recv break signal\n", current->comm);
		reason = ZRAM_CANCEL_BY_SIGNAL;
		goto ABORT_MADVISE;
	}

	/*
	 * Release mmap_lock temporarily if someone wants to
	 * access it for write request.
	 */
	if (mmap_lock_is_contended(mm)) {
		reason = ZRAM_CANCEL_BY_MMAP_LOCK;
		if (is_log_enable(ZRAM_COMPRESS_LOG))
			pr_err("comm:%s mmap lock is contended\n", current->comm);
		goto ABORT_MADVISE;
	}

	/* Single total compression data exceeds threshold. */
	delta_time = local_clock() - params->start_time;
	if (delta_time >= params->cprs_time_limit) {
		if (is_log_enable(ZRAM_COMPRESS_LOG))
			pr_err("comm:%s compress timeout delta:%lld ms thres:%llu ms\n",
				current->comm, delta_time/1000000, params->cprs_time_limit/1000000);
		reason = ZRAM_CANCEL_BY_TIME;
		goto ABORT_MADVISE;
	}

	/* Single compression time exceeds threshold. */
	delta_compressed = atomic64_read(&fwk_compress_pages_since) - params->compressed_snapshot;
	if (delta_compressed >= params->cprs_data_limit) {
		if (is_log_enable(ZRAM_COMPRESS_LOG))
			pr_err("comm:%s compress total overflow. cprs:%lld limit:%llu\n",
				current->comm, delta_compressed<<(PAGE_SHIFT-10),  params->cprs_data_limit<<(PAGE_SHIFT-10));
		reason = ZRAM_CANCEL_BY_SIZE;
		goto ABORT_MADVISE;
	}

	/*
	 * By the time process_madvise called mm_access, mm_struct was already protected.
	 * Target process's compressed anon pages exceeds the threshold percentage of its VmRss memory.
	 */
	anon = get_mm_counter(mm, MM_ANONPAGES);
	swap = get_mm_counter(mm, MM_SWAPENTS);
	total = anon + swap;
	if (swap * 100 > params->cprs_prop_limit*total) {
		reason = ZRAM_CANCEL_BY_PROP;
		if (is_log_enable(ZRAM_COMPRESS_LOG))
			pr_err("comm:%s compress prop overflow. swap:%ld total:%lu\n",
				current->comm, swap<<(PAGE_SHIFT-10), total<<(PAGE_SHIFT-10));
		goto ABORT_MADVISE;
	}

	/* The target process is running. */
	if (target_task && (target_task->__state == TASK_RUNNING) && (target_task->mm == mm)) {
		reason = ZRAM_CANCEL_BY_RUNNING;
		if (is_log_enable(ZRAM_COMPRESS_LOG))
			pr_err("comm:%s compress %s abort by running.\n", current->comm, target_task->comm);
		goto ABORT_MADVISE;
	}

	return;

ABORT_MADVISE:
	*abort_madvise = true;
	zram_last_compressed_abort = reason;
	return;
}

static int memfusion_register_vendor_hook(void) {
	int ret;

	ret = register_trace_android_vh_madvise_cold_or_pageout_abort(should_cancel_madvise, NULL);
	if (ret != 0) {
		pr_err("register trace_android_vh_should_end_madvise failed! ret=%d\n", ret);
	}

	ret = register_trace_android_rvh_set_task_comm(probe_android_set_task_comm_hook, NULL);
	if (ret)
		pr_err(MF22PRELOG"register trace_android_rvh_set_task_comm failed! ret=%d", ret);

	return ret;
}

static void memfusion_unregister_vendor_hook(void) {
	unregister_trace_android_vh_madvise_cold_or_pageout_abort(should_cancel_madvise, NULL); 
}

static int zram_aggr_wb_show(struct seq_file *m, void *v) {
	seq_printf(m, "aggr-wb: %s\n", zram_writeback_aggregation?"on":"off");
	return 0;
}

static int zram_aggr_wb_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_aggr_wb_show, NULL);
}

static ssize_t zram_aggr_wb_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[32]={'\0'};
	int aggr_wb = 0;

	if (count >= 32) {
		return count;
	}

	if (count > 0) {
		if (copy_from_user(buf, ubuf, count)) {
			return -EFAULT;
		}
		sscanf(buf, "%d", &aggr_wb);
		zram_writeback_aggregation = aggr_wb>0?true:false;
		zram_vip_io_sched_enable = aggr_wb>0?true:false;
		pr_err(MF22PRELOG"aggregation writeback status: %s\n", aggr_wb>0?"on":"off");
	}
	return count;
}

const struct proc_ops zram_aggr_wb_fops = {
	.proc_write = zram_aggr_wb_write,
	.proc_open = zram_aggr_wb_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t zram_backing_dev_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[10]={'\0'};
	int bd_index_get;

	if (count >= 10) {
		return count;
	}

	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("[Memfusion2_2]get swaps para failed, please try again!\n");
			return -EFAULT;
		}
		sscanf(buf, "%d", &bd_index_get);
		if(bd_index_get > g_backing_dev_avail_count)
		{
			bd_index_get = g_backing_dev_avail_count;
			pr_err("[Memfusion2_2][Error]bd_index_get has been set too high!!\n bd_index_get would set to %d \n",g_backing_dev_avail_count);
		}
		else if(bd_index_get < 0)
		{
			bd_index_get = 0;
			pr_err("[Memfusion2_2][Error]bd_index_get has been set too high!!\n bd_index_get would set to %d \n",0);
		}
		if(g_backingdev != NULL)
		{
			down_read(&g_backingdev->init_lock);
			atomic_set(&g_backingdev->backing_dev_index, bd_index_get);
			up_read(&g_backingdev->init_lock);
			pr_err("[Memfusion2_2]backing_dev_index = %d \n", bd_index_get);
		}
	}
	return count;
}

static int zram_backing_dev_show(struct seq_file *m, void *v)
{
	int bd_index_get;
	if(g_backingdev)
	{
		down_read(&g_backingdev->init_lock);
		bd_index_get = atomic_read(&g_backingdev->backing_dev_index);
		seq_printf(m,"g_backing_dev_avail_count:%d , g_backingdev->backing_dev_index:%d\n", (g_backing_dev_avail_count + 1),bd_index_get);
		up_read(&g_backingdev->init_lock);
	}
	else
		seq_printf(m,"[Memfusion2_2][zram_backing_dev]Zram is not ready! Please wait for system complately started!");
	return 0;
}

static int zram_backing_dev_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_backing_dev_show, inode->i_private);
}

const struct proc_ops zram_backing_dev_fops = {
	.proc_open		= zram_backing_dev_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_backing_dev_write,
};

static ssize_t zram_bd_used_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[256]={'\0'};
	int zram_bd_install_error_get, zram_bd_install_errorno_get;

	if (count >= 256) {
		return count;
	}

	if(count > 0)
	{
		if(copy_from_user(buf, ubuf, count))
		{
			pr_err("[Memfusion2_2]get swaps para failed, please try again!\n");
			return -EFAULT;
		}
		if(sscanf(buf, "%d:%d", &zram_bd_install_error_get, &zram_bd_install_errorno_get) == 2)
		{
			zram_bdev_install_error = zram_bd_install_error_get;
			zram_bdev_install_errorno = zram_bd_install_errorno_get;
			pr_info("[Memfusion2_2]bd_install_state = %d, errorno = %d\n", zram_bdev_install_error, zram_bdev_install_errorno);
		}
		else
		{
			pr_err("[Memfusion2_2]get swaps para failed, please try again!\n");
		}
	}
	return count;
}

static int zram_bd_used_show(struct seq_file *m, void *v)
{
	if(g_zram)
	{
		// If zram backing dev is ready, show backing dev used percentage and size
		if(g_backingdev)
		{
			down_read(&g_backingdev->init_lock);
#if defined(CONFIG_ESWAP)
			if (eswap_enable()) {
				seq_printf(m,"%lld%%:%dm\n", (((s64) eswap_get_used_ext_pages() * 100) / (g_backingdev->nr_pages_total)),(int)(bdev_size >> 10));
			} else {
				seq_printf(m,"%lld%%:%dm\n", ((atomic64_read(&g_backingdev->stats.bd_count) * 100) / (g_backingdev->nr_pages_total)),(int)(bdev_size >> 10));
			}
#else
			seq_printf(m,"%lld%%:%dm\n", ((atomic64_read(&g_backingdev->stats.bd_count) * 100) / (g_backingdev->nr_pages_total)),(int)(bdev_size >> 10));
#endif
			up_read(&g_backingdev->init_lock);
		}
		else
		{
			// if zram exists but zram backing dev is not exist, show 0%:0m
			seq_printf(m,"0%%:0m\n");
		}
	}
	else
	{
		// If zram does not exist, show blank
		seq_printf(m," ");
	}
	return 0;
}

static int zram_bd_used_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_bd_used_show, inode->i_private);
}

const struct proc_ops zram_bd_used_fops = {
	.proc_open		= zram_bd_used_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_bd_used_write,
};

static ssize_t zram_bd_list_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	/*eg: count:36 string:"/dev/block/loop45$$/dev/block/loop46" */
	if ((count > 0) && (count < MAX_BDEV_LIST_LEN)) {
		if(copy_from_user(zram_bdev_name_buf, ubuf, count))
		{
			pr_err("get zram_bd_list failed\n");
			return -EFAULT;
		}
		zram_bdev_name_buf[count]= '\0';
	}
	return count;
}

static int zram_bd_list_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s", zram_bdev_name_buf);
	return 0;
}
static int zram_bd_list_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_bd_list_show, inode->i_private);
}
const struct proc_ops zram_bd_list_fops = {
	.proc_open		= zram_bd_list_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_bd_list_write,
};

bool zram_memcg_compress_enable(void) {
	return g_memcg_compress_enable;
}
static int zram_memcg_compress_show(struct seq_file *m, void *v) {
	seq_printf(m, "memcg-compress: %s\n", g_memcg_compress_enable?"on":"off");
	return 0;
}

static int zram_memcg_compress_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_memcg_compress_show, NULL);
}

static ssize_t zram_memcg_compress_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[32]={'\0'};
	int temp = 0;

	if (count >= 32) {
		return count;
	}

	if (count > 0) {
		if (copy_from_user(buf, ubuf, count)) {
		    return -EFAULT;
		}
		sscanf(buf, "%d", &temp);
		g_memcg_compress_enable = (temp>0?true:false);
		pr_err("memcg compact v1.1 status: %s\n", g_memcg_compress_enable?"on":"off");
	}

	return count;
}

const struct proc_ops zram_memcg_compress_fops = {
	.proc_open		= zram_memcg_compress_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_memcg_compress_write,
};

static int zram_memcg_compress_v2_show(struct seq_file *m, void *v) {
	seq_printf(m, "memcg-compress-v2: %s\n", is_memcg_compact_v2?"on":"off");
	return 0;
}

static int zram_memcg_compress_v2_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_memcg_compress_v2_show, NULL);
}

static ssize_t zram_memcg_compress_v2_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char buf[32]={'\0'};
	int temp = 0;

	if (count >= 32) {
		return count;
	}

	if (count > 0) {
		if (copy_from_user(buf, ubuf, count)) {
		    return -EFAULT;
		}
		sscanf(buf, "%d", &temp);
		/* Except for lib_fsmgr, no one else is allowed to write to /proc/memfusion/memcg_compress_v2. */
		if (2 != temp) {
			pr_err(MF22PRELOG"memcg compact v2 except for lib_fsmgr, no one else has permission to modify this. %d\n", temp);
			return count;
		}

		g_memcg_compress_v2_enable = (temp>0?true:false);
		if (g_memcg_compress_v2_enable) {
			g_swap_kernel_write_enable = false;
			g_swap_fwk_write_enable = false;
		}

		pr_err(MF22PRELOG"memcg compact v2 status: %s kernel_write_enable = %d fwk_write_enable = %d\n",
			g_memcg_compress_v2_enable?"on":"off",
			g_swap_kernel_write_enable, g_swap_fwk_write_enable);
	}

	return count;
}

#ifdef CONFIG_ZRAM_MULTI_COMP
static int zram_muticomp_enable_show(struct seq_file *m, void *v) {
	seq_printf(m, "zram muticomp status: %s\n", g_muticomp_enable?"on":"off");
	return 0;
}

static int zram_muticomp_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, zram_muticomp_enable_show, NULL);
}

static ssize_t zram_muticomp_enable_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	int temp;

	if (kstrtoint_from_user(ubuf, count, 10, &temp))
		return -EINVAL;

	g_muticomp_enable = (temp>0?true:false);
	pr_info("zram muticomp status: %s\n", g_muticomp_enable?"on":"off");

	return count;
}

const struct proc_ops zram_muticomp_enable_fops = {
	.proc_open		= zram_muticomp_enable_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_muticomp_enable_write,
};
#endif

const struct proc_ops zram_memcg_compress_v2_fops = {
	.proc_open		= zram_memcg_compress_v2_open,
	.proc_read		= seq_read,
	.proc_lseek 	= seq_lseek,
	.proc_release 	= single_release,
	.proc_write 	= zram_memcg_compress_v2_write,
};

static void tran_memfusion_proc_create(void)
{
	struct proc_dir_entry *memfusion_dir = NULL;
	memfusion_dir = proc_mkdir("memfusion", NULL);
	if (NULL == memfusion_dir){
		pr_err(MF22PRELOG"create proc memfusion file failed\n");
		return;
	}

	proc_create("swap_write_config", S_IALLUGO, memfusion_dir, &swap_write_config_fops);
	proc_create("swap_size", S_IALLUGO, memfusion_dir, &swap_size_fops);
	proc_create("swap_write_enable", S_IALLUGO, memfusion_dir, &swap_write_enable_fops);
	proc_create("swap_usage", S_IALLUGO, memfusion_dir, &swap_usage_fops);
	proc_create("swap_write_count", S_IALLUGO, memfusion_dir, &swap_write_count_fops);
	proc_create("dump_info", S_IALLUGO, memfusion_dir, &dump_info_fops);
	proc_create("tlc_value", S_IALLUGO, memfusion_dir, &tlc_value_fops);
	proc_create("writen_count_per_day", S_IALLUGO, memfusion_dir, &writen_count_per_day_fops);
	proc_create("vma_para", S_IALLUGO, memfusion_dir, &vma_para_fops);
	proc_create("mnt_threthold", S_IALLUGO, memfusion_dir, &mnt_threthold_fops);
	proc_create("zram_drv_para", S_IALLUGO, memfusion_dir, &zram_drv_para_fops);
	proc_create("workingset_refault_para", S_IALLUGO, memfusion_dir, &workingset_refault_para_fops);
	proc_create("zram_writeback", S_IALLUGO, memfusion_dir, &zram_writeback_fops);
	proc_create("zram_start_thread", S_IALLUGO, memfusion_dir, &zram_start_thread_fops);
	proc_create("am_app_launch", S_IALLUGO, memfusion_dir, &am_app_launch_fops);
	proc_create("high_load", S_IALLUGO, memfusion_dir, &high_load_fops);
	proc_create("zram_wbd_interval", S_IALLUGO, memfusion_dir, &zram_wbd_interval_fops);
	proc_create("system_loading_threthold", S_IALLUGO, memfusion_dir, &system_loading_threthold_fops);
	proc_create("bdev_rw_data", S_IALLUGO, memfusion_dir, &bdev_rw_data_fops);
	proc_create("zram_stat", S_IALLUGO, memfusion_dir, &zram_stat_fops);
	proc_create("zram_wb_switch", S_IALLUGO, memfusion_dir, &zram_wb_switch_fops);
	proc_create("zram_wb_actime_limit", S_IALLUGO, memfusion_dir, &zram_wb_actime_limit_fops);
	proc_create("zram_wb_count_limit", S_IALLUGO, memfusion_dir, &zram_wb_count_limit_fops);
	proc_create("zram_psi", S_IALLUGO, memfusion_dir, &zram_psi_fops);
	proc_create("ui_swapsize", S_IALLUGO, memfusion_dir, &ui_swapsize_fops);
	proc_create("sw_fails_cnt", S_IALLUGO, memfusion_dir, &switch_cnt_fops);
	proc_create("key_pid", S_IALLUGO, memfusion_dir, &zram_key_pid_fops);
	proc_create("run_stat", S_IALLUGO, memfusion_dir, &zram_run_stat_fops);
	proc_create("zram_backing_dev", S_IALLUGO, memfusion_dir, &zram_backing_dev_fops);
	proc_create("zram_bd_used", S_IALLUGO, memfusion_dir, &zram_bd_used_fops);
	proc_create("zram_bd_list", S_IALLUGO, memfusion_dir, &zram_bd_list_fops);
	proc_create("aggr_enable", S_IALLUGO, memfusion_dir, &zram_aggr_wb_fops);
	proc_create("memcg_compress", S_IALLUGO, memfusion_dir, &zram_memcg_compress_fops);
	proc_create("memcg_compress_v2", S_IALLUGO, memfusion_dir, &zram_memcg_compress_v2_fops);
#ifdef CONFIG_ZRAM_MULTI_COMP
	proc_create("muticomp_enable", S_IALLUGO, memfusion_dir, &zram_muticomp_enable_fops);
#endif
	zram_process_reclaim_init(memfusion_dir);

	pr_info(MF22PRELOG"create proc memfusion file success\n");
}
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end
static int __init zram_init(void)
{
	int ret;

#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	unsigned long physpages;
	g_swap_kernel_write_enable = 0;
	g_swap_fwk_write_enable = 0;

	physpages = get_num_physpages();
	if (physpages > ZRAM_NR_PAGES_4G) {
		zram_kernel_wb_count_limit = ZRAM_NR_PAGE_WB_LIMIT;
	}
#endif
	BUILD_BUG_ON(__NR_ZRAM_PAGEFLAGS > BITS_PER_LONG);

	ret = cpuhp_setup_state_multi(CPUHP_ZCOMP_PREPARE, "block/zram:prepare",
				      zcomp_cpu_up_prepare, zcomp_cpu_dead);
	if (ret < 0)
		return ret;

	ret = class_register(&zram_control_class);
	if (ret) {
		pr_err("Unable to register zram-control class\n");
		cpuhp_remove_multi_state(CPUHP_ZCOMP_PREPARE);
		return ret;
	}

	zram_debugfs_create();
	zram_major = register_blkdev(0, "zram");
	if (zram_major <= 0) {
		pr_err("Unable to get major number\n");
		class_unregister(&zram_control_class);
		cpuhp_remove_multi_state(CPUHP_ZCOMP_PREPARE);
		return -EBUSY;
	}

	while (num_devices != 0) {
		mutex_lock(&zram_index_mutex);
		ret = zram_add();
		mutex_unlock(&zram_index_mutex);
		if (ret < 0)
			goto out_error;
		num_devices--;
	}
#if IS_ENABLED(CONFIG_ZRAM_MULTI_COMP)
	ret = wkdm_init();
	if (ret < 0) {
		pr_err("Failed to register WKDM algorithm: %d\n", ret);
		goto out_error;
	}
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	tran_memfusion_proc_create();
	memfusion_register_vendor_hook();
#endif
//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end

#if defined(CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT) || defined(CONFIG_ESWAP)
	/* if eswap init failed, eswap won't work,
	 * but zram works as normal
	 */
	mem_eswap_init();
#endif
	register_cloudengine_notifier(tran_zram_cloud_config_ID, "v1.x", &tran_zram_cloud_notifier_block);

	return 0;

out_error:
	destroy_devices();
	return ret;
}

static void __exit zram_exit(void)
{
#if defined(CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT) || defined(CONFIG_ESWAP)
	mem_eswap_exit();
#endif
	destroy_devices();
#if IS_ENABLED(CONFIG_ZRAM_MULTI_COMP)
	wkdm_exit();
#endif
	//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 start
#ifdef CONFIG_TRAN_MEMORY_FUSION2_2_SUPPORT
	memfusion_unregister_vendor_hook();
	zram_process_reclaim_exit();
#endif
	//SPD:add MemFusion2.2 for XLBYYHBWQ-3103 by zihan.ju at 20230506 end
	unregister_cloudengine_notifier(tran_zram_cloud_config_ID, &tran_zram_cloud_notifier_block);
}

module_init(zram_init);
module_exit(zram_exit);

module_param(num_devices, uint, 0);
MODULE_PARM_DESC(num_devices, "Number of pre-created zram devices");

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Nitin Gupta <ngupta@vflare.org>");
MODULE_DESCRIPTION("Compressed RAM Block Device");
