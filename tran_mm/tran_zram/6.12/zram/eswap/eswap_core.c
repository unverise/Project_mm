#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sched/task.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/memcontrol.h>
#include <linux/err.h>
#include <linux/refcount.h>

#include "../zram_drv.h"
#include "memcg_control.h"
#include "eswap_internal.h"
#include "eswap_manager.h"
#include "eswap_schedule.h"
#include "eswap_area.h"
#include "eswap_acct.h"
#include "eswap_lru_rmap.h"

extern int zram_kernel_wb_count_limit;

#define MAX_USED_EXT_RATIO	99

struct simple_wait_queue_entry {
	struct task_struct	*tsk;
	struct list_head	entry;
};

static atomic_t extent_readio_waiter;
struct simple_wait_queue_head {
	spinlock_t		lock;
	struct list_head	head;
};

static struct simple_wait_queue_head extent_readio_swqh = {
	.lock		= __SPIN_LOCK_UNLOCKED(extent_readio_swqh.lock),
	.head		= LIST_HEAD_INIT(extent_readio_swqh.head)
};

static int wait_for_any_extent_readio_done_timeout(signed long t, int delay_cnt)
{
	unsigned long flags;
	struct list_head *head = &extent_readio_swqh.head;
	spinlock_t *lock = &extent_readio_swqh.lock;
	struct simple_wait_queue_entry swq = {
		.tsk = current,
		.entry = LIST_HEAD_INIT((swq).entry),
	};

	if (!delay_cnt++)
		atomic_inc(&extent_readio_waiter);

	// add to list
	spin_lock_irqsave(lock, flags);
	list_add(&swq.entry, head);
	spin_unlock_irqrestore(lock, flags);

	// sleep 1 jiffies at most
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout_interruptible(t);

	// cleanup, remove from list
	__set_current_state(TASK_RUNNING);
	spin_lock_irqsave(lock, flags);
	if (!list_empty(&swq.entry))
		list_del_init(&swq.entry);
	spin_unlock_irqrestore(lock, flags);

	return delay_cnt;
}

static void exit_wait_for_extent_readio(int delay_cnt, const char *log, atomic64_t *counter)
{
	if (likely(delay_cnt == 0))
		return;

	atomic_dec(&extent_readio_waiter);
	if (counter)
		atomic64_inc(counter);

	eswap_debug(ESD_WARN, "%s %d %d\n", log, delay_cnt, atomic_read(&extent_readio_waiter));
}

static void publish_extent_readio_done(void)
{
	unsigned long flags;
	struct list_head *head = &extent_readio_swqh.head;
	spinlock_t *lock = &extent_readio_swqh.lock;
	struct simple_wait_queue_entry *curr, *next;

	// quick path, no one is waiting
	if (!atomic_read(&extent_readio_waiter))
		return;

	spin_lock_irqsave(lock, flags);
	list_for_each_entry_safe(curr, next, head, entry) {
		wake_up_process(curr->tsk);
		list_del_init(&curr->entry);
	}
	spin_unlock_irqrestore(lock, flags);
}

struct async_req {
	struct mem_cgroup *mcg;
	unsigned long size;
	struct work_struct work;
	int nice;
	bool preload;
};

struct schedule_para {
	void *io_scheduler;
	struct eswap_page_pool page_pool;
	struct eswap_key_point_record record;
};

static void for_each_memcg(int (*iter)(struct mem_cgroup *, void *),
			   void *data)
{
	struct mem_cgroup *memcg = NULL;
	while ((memcg = get_next_memcg(memcg))) {
		if (iter(memcg, data)) {
			get_next_memcg_break(memcg);
			break;
		}
	}
}

/*
 * This interface will be called when anon page is added to ZRAM.
 * eswap should trace this ZRAM in memcg LRU list.
 * 1. Add ZRAM obj into LRU.
 * 2. Updata the stored size in memcg and ZRAM.
 * 3. ZRAM capcity pressure check & report
 */
int eswap_track(struct zram *zram, u32 index,
	struct mem_cgroup *memcg, struct mem_cgroup *my_memcg)
{
	struct memcg_estrack *est;
	int err = 0;

	if (!eswap_enable())
		return -EINVAL;

	if (index >= (u32) zram->area->nr_objs) {
		eswap_debug(ESD_ERR, "index = %d invalid\n", index);
		return 0;
	}

	if (unlikely(!memcg))
		return 0;

	if (zram_get_obj_size(zram, index) == 0)
		return 0;

	/* do not track same page from the begining */
	if (zram_test_flag(zram, index, ZRAM_SAME)) {
		/* track the original memcg but not eswap */
		zram_set_memcg(zram, index, my_memcg);
		return 0;
	}

	est = mem_cgroup_estrack(memcg);
	if (est == NULL) {
		est = __memcg_estrack_alloc(memcg, NULL);
		if (IS_ERR(est)) {
			return PTR_ERR(est);
		}
	}

	if (unlikely(!est->zram)) {
		spin_lock(&est->zram_init_lock);
		if (!est->zram) {
			err = memcg_estrack_init(memcg, zram);
		}
		spin_unlock(&est->zram_init_lock);

		if (err)
			return -EINVAL;
	}

	if (zram_test_flag(zram, index, ZRAM_UNDER_WB)) {
		return -EINVAL;
	}

	err = zram_lru_add(zram, index, memcg);
	if (err)
		return err;

	zram_set_flag(zram, index, ZRAM_ESWAP_TRACK);

	return 0;
}

/* This interface will be called when anon page in ZRAM is freed. */
void eswap_untrack(struct zram *zram, u32 index)
{
	/*
	 * 1. When the ZS object in the writeback or swapin,
	 * it can't be untrack.
	 * 2. Updata the stored size in memcg and ZRAM.
	 * 3. Remove ZRAM obj from LRU.
	 *
	 */
	int delay_count = 0;

	if (!eswap_enable())
		return;

	if (index >= (u32) zram->area->nr_objs) {
		eswap_debug(ESD_ERR, "index = %d invalid\n", index);
		return;
	}

	if (!zram_test_flag(zram, index, ZRAM_ESWAP_TRACK))
		return;

	/* entry already under writeback, eswap-wb routine
	 * will handle the untrack case
	 */
	if (zram_test_flag(zram, index, ZRAM_UNDER_WB)) {
		struct eswap_stat *stat = eswap_get_stat_obj();

		atomic64_inc(&stat->under_wb_untrack);
		zram_clear_flag(zram, index, ZRAM_ESWAP_TRACK);
		return;
	}

	while (zram_test_flag(zram, index, ZRAM_BATCHING_OUT)) {
		zram_slot_unlock(zram, index);
		delay_count = wait_for_any_extent_readio_done_timeout(1, delay_count);
		zram_slot_lock(zram, index);
	}

	memcg_estrack_zram_lru_del(zram, index);
	zram_clear_flag(zram, index, ZRAM_ESWAP_TRACK);

	exit_wait_for_extent_readio(delay_count, "untrack busy delayed count",
				&(eswap_get_stat_obj()->faulting_2));
}

struct mem_cgroup *eswap_untrack_memcg_get(struct zram *zram, u32 index)
{
	/*
	 * 1. When the ZS object in the writeback or swapin,
	 * it can't be untrack.
	 * 2. Updata the stored size in memcg and ZRAM.
	 * 3. Remove ZRAM obj from LRU.
	 *
	 */
	int delay_count = 0;
	struct mem_cgroup *memcg  = NULL;

	if (!eswap_enable())
		return NULL;

	if (index >= (u32) zram->area->nr_objs) {
		eswap_debug(ESD_ERR, "index = %d invalid\n", index);
		return NULL;
	}

	if (!zram_test_flag(zram, index, ZRAM_ESWAP_TRACK))
		return NULL;

	memcg = zram_get_memcg_with_css_get(zram, index);

	/* zram same page nerver enter eswap */
	if (zram_test_flag(zram, index, ZRAM_SAME)) {
		/* track the original memcg but not eswap */
		zram_clear_memcg(zram, index);
		return memcg;
	}

	/* entry already under writeback, eswap-wb routine
	 * will handle the untrack case
	 */
	if (zram_test_flag(zram, index, ZRAM_UNDER_WB)) {
		struct eswap_stat *stat = eswap_get_stat_obj();

		atomic64_inc(&stat->under_wb_untrack);
		zram_clear_flag(zram, index, ZRAM_ESWAP_TRACK);
		return memcg;
	}

	while (zram_test_flag(zram, index, ZRAM_BATCHING_OUT)) {
		zram_slot_unlock(zram, index);
		delay_count = wait_for_any_extent_readio_done_timeout(1, delay_count);
		zram_slot_lock(zram, index);
	}

	memcg_estrack_zram_lru_del(zram, index);
	zram_clear_flag(zram, index, ZRAM_ESWAP_TRACK);

	exit_wait_for_extent_readio(delay_count, "untrack busy delayed count",
				&(eswap_get_stat_obj()->faulting_2));
	return memcg;
}

void eswap_untrack_memcg_put(struct mem_cgroup *memcg)
{
	if (memcg)
		css_put(&memcg->css);
	return;
}

static unsigned long memcg_reclaim_size(struct mem_cgroup *memcg)
{
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);

	unsigned long zram_size = (unsigned long)atomic_read(&est->zram_stored_size);
	unsigned long cur_size = (unsigned long)atomic_read(&est->eswap_stored_size);
	unsigned long new_size = (zram_size + cur_size) * get_zram2flash_ratio(memcg) / 100;

#ifdef CONFIG_ESWAP_IDLE_WRITEBACK
	if (task_eswap_idle_reclaim(current))
		return atomic64_read(&est->zram_idle_size);
#endif

	return (new_size > cur_size) ? (new_size - cur_size) : 0;
}

static int account_memcg_size(struct mem_cgroup *memcg, void *data)
{
	unsigned long *out_size = (unsigned long *)data;

	*out_size += memcg_reclaim_size(memcg);
	return 0;
}

static inline void stat_alloc_fail(enum eswap_scenario scenario, int err)
{
	struct eswap_stat *stat = eswap_get_stat_obj();

	if ((err != -ENOMEM) || (scenario >= ESWAP_SCENARIO_NUM))
		return;

	atomic64_inc(&stat->alloc_fail_cnt[scenario]);
}

struct ioe_mgmt {
	struct io_extent io_ext;
	struct io_extent_mgmt *io_ext_mgmt;
};

static void io_extent_attach_mgmt(struct io_extent *io_ext,
				  struct io_extent_mgmt *io_ext_mgmt)
{
	struct ioe_mgmt *ioe = (struct ioe_mgmt *)io_ext;

	io_ext->ext_id = io_ext_mgmt->ext_id;
	io_ext->bdev = map_extid_to_bdev(io_ext_mgmt->zram->area, io_ext_mgmt->ext_id);
	io_ext->addr = map_extid_to_sector(io_ext_mgmt->zram->area, io_ext_mgmt->ext_id);
	io_ext->dest_pages = io_ext_mgmt->pages;
	io_ext->pages_sz = EXTENT_PG_CNT;

	ioe->io_ext_mgmt = io_ext_mgmt;
}

static inline struct io_extent *alloc_io_extent_acct(struct eswap_key_point_record *record,
	enum eswap_scenario scenario, bool fast, bool nofail)
{
	struct io_extent *entry = NULL;

	esacct_lat_start(record, ESWAP_IOENTRY_ALLOC);
	//entry = (struct io_extent *) eswap_malloc(sizeof(struct ioe_mgmt), fast, nofail);
	entry = (struct io_extent *) kzalloc(sizeof(struct ioe_mgmt), GFP_NOWAIT);
	esacct_lat_end(record, ESWAP_IOENTRY_ALLOC);

	if (unlikely(!entry)) {
		eswap_debug(ESD_ERR, "alloc io entry failed!\n");
		stat_alloc_fail(scenario, -ENOMEM);
	}

	return entry;
}

typedef void (*extent_done_func)(struct io_extent *, enum eswap_scenario, int);
typedef void (*io_complete_func)(void *);

static void eswap_io_extent_done_cb(struct io_extent *io_ext,
	enum eswap_scenario scenario, int err)
{
	struct ioe_mgmt *ioe;
	struct io_extent_mgmt *io_ext_mgmt;

	/* user NULL io_extent and 0 err
	 * to tell us segment is done successfully
	 */
	if (io_ext == NULL && !err) {
		/* update statics with external watcher everytime io finished */
		sync_info_with_extern();
		return;
	}

	ioe = (struct ioe_mgmt *)io_ext;
	io_ext_mgmt = ioe->io_ext_mgmt;

	if (unlikely(!io_ext_mgmt)) {
		eswap_debug(ESD_ERR, "NULL io_ext_mgmt\n");
		return;
	}

	if (likely(!err)) {
		switch (scenario) {
		case ESWAP_IN_FAULT:
		case ESWAP_IN_PRELOAD:
		case ESWAP_IN_BATCH:
			io_extent_mgmt_unregister(io_ext_mgmt, scenario);

			/* may be some one is waiting for us
			 * because they read the same extent at the same time ..
			 */
			publish_extent_readio_done();
			break;

		case ESWAP_OUT:
			io_extent_mgmt_register(io_ext_mgmt);
			break;
		default:
			break;
		}
	} else {
		io_extent_mgmt_exception(io_ext_mgmt, scenario);
	}

	/* in the case of ESWAP_IN_FAULT, ioe_mgmt is in stack .. */
	if (scenario != ESWAP_IN_FAULT)
		eswap_free(ioe);

// #ifdef CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT
//	if (!err)
//		try_to_wake_all_zswapds();
// #endif
}

/* in the case of ESWAP_IN_FAULT, schedule_para is in stack
 * and this calllback will not be registered
 */
static void eswap_io_scheduler_done_cb(void *data)
{
	struct schedule_para *sched  = (struct schedule_para *)data;
	struct page *free_page = NULL;
	struct list_head *list = &sched->page_pool.page_pool_list;
	spinlock_t *lock = &sched->page_pool.page_pool_lock;
	int count = 0;

	esacct_end(&sched->record);

	spin_lock(lock);
	while (!list_empty(list)) {
		free_page = list_first_entry(list, struct page, lru);
		list_del_init(&free_page->lru);
		__free_page(free_page);

		count++;
	}
	spin_unlock(lock);

	eswap_debug(ESD_DEBUG, "count %d\n", count);
	eswap_free(sched);
}

static void *eswap_io_scheduler_get_acct(struct zram *zram, enum eswap_scenario scenario,
	struct eswap_key_point_record *record, extent_done_func f1, io_complete_func f2, void *private)
{
	struct eswap_io_scheduler_config config;
	void *p;

	esacct_lat_start(record, ESWAP_INIT);

	config.zram = zram;
	config.scenario = scenario;
	config.record = record;
	config.io_extent_done_cb = f1;
	config.complete_notify = f2;
	config.private = private;

	p = eswap_io_scheduler_get(&config);
	if (!p)
		goto err;

	esacct_lat_end(record, ESWAP_INIT);
	return p;

err:
	esacct_lat_end(record, ESWAP_INIT);
	eswap_debug(ESD_ERR, "plug start failed!\n");
	stat_alloc_fail(scenario, -ENOMEM);
	if (scenario != ESWAP_IN_FAULT)
		esacct_end(record);
	return NULL;
}

static inline int reclaim_check(struct mem_cgroup *memcg,
	unsigned long *require_size, unsigned long *mcg_reclaim_sz)
{
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);
	unsigned long size;
	if (unlikely(!est->zram))
		return -EAGAIN;

	if (!zram_should_writeback(est->zram, 0, false))
		return -EINVAL;

	if (unlikely(est->in_swapin))
		return -EAGAIN;

	if (*require_size < MIN_RECLAIM_ZRAM_SZ)
		return -EIO;

	size = memcg_reclaim_size(memcg);

	if (size < MIN_RECLAIM_ZRAM_SZ)
		return -EAGAIN;

	if (size > MAX_RECLAIM_ZRAM_SZ)
		size = MAX_RECLAIM_ZRAM_SZ;

	*mcg_reclaim_sz = size;
	return 0;
}

static inline int eswap_update_reclaim_sz(unsigned long *require_size,
	unsigned long *mcg_reclaim_sz, unsigned long reclaim_size)
{
	if (*require_size > reclaim_size)
		*require_size -= reclaim_size;
	else
		*require_size = 0;

	if (*mcg_reclaim_sz > reclaim_size)
		*mcg_reclaim_sz -= reclaim_size;
	else
		return -EINVAL;

	if (*mcg_reclaim_sz < MIN_RECLAIM_ZRAM_SZ)
		return -EAGAIN;
	return 0;
}

static inline int eswap_extent_write_acct(
	void *io_scheduler, 
	struct io_extent *io_ext,
	struct eswap_key_point_record *record)
{
	int ret;
	esacct_lat_start(record, ESWAP_IO_EXTENT);
	ret = eswap_extent_io(io_scheduler, io_ext);
	esacct_lat_end(record, ESWAP_IO_EXTENT);
	if (unlikely(ret)) {
		eswap_debug(ESD_ERR, "eswap write failed! %d\n", ret);
		stat_alloc_fail(ESWAP_OUT, ret);
	}
	return ret;
}


static inline int eswap_extent_read_acct(
	void *io_scheduler, 
	struct io_extent *io_ext,
	struct eswap_key_point_record *record,
	enum eswap_scenario scenario)
{
	int ret;
	esacct_lat_start(record, ESWAP_IO_EXTENT);
	ret = eswap_extent_io(io_scheduler, io_ext);
	esacct_lat_end(record, ESWAP_IO_EXTENT);
	if (unlikely(ret)) {
		eswap_debug(ESD_ERR, "eswap read failed! %d\n", ret);
		stat_alloc_fail(scenario, ret);
	}
	return ret;
}

static int do_eswap_extent_out(struct mem_cgroup *memcg, struct schedule_para *sched,
	unsigned long *require_size, unsigned long *mcg_reclaim_sz, ktime_t now)
{
	int ret;
	unsigned long bytes_to_reclaimed = 0;
	struct io_extent_mgmt *io_ext_mgmt;
	struct io_extent *io_ext;

	io_ext = alloc_io_extent_acct(&sched->record, ESWAP_OUT, false, true);
	if (!io_ext) {
		*require_size = 0;
		return -ENOMEM;
	}

	esacct_lat_start(&sched->record, ESWAP_FIND_EXTENT);
	// io_ext_mgmt = io_extent_mgmt_create(memcg, NULL, &bytes_to_reclaimed);
	io_ext_mgmt = io_extent_mgmt_create(memcg, &sched->page_pool, &bytes_to_reclaimed, now);
	esacct_lat_end(&sched->record, ESWAP_FIND_EXTENT);
	if (unlikely(IS_ERR(io_ext_mgmt))) {
		if (PTR_ERR(io_ext_mgmt) != -ENOENT)
			*require_size = 0;
		eswap_free(io_ext);
		return -EAGAIN;
	}

	io_extent_attach_mgmt(io_ext, io_ext_mgmt);

	ret = eswap_extent_write_acct(sched->io_scheduler, io_ext, &sched->record);
	if (ret) {
		*require_size = 0;
		return ret;
	}

	ret = eswap_update_reclaim_sz(require_size, mcg_reclaim_sz, bytes_to_reclaimed);
	return ret;
}

static int __do_memcg_eswap_out(struct mem_cgroup *memcg, void *data)
{
	/*
	 * 1. Recaim one extent from ZRAM in cur memcg.
	 * 2. If extent exist, dispatch it to UFS.
	 */
	int ret, io_err = 0;
	unsigned long *require_size = (unsigned long *)data;
	unsigned long mcg_reclaim_sz;
	struct schedule_para *sched = NULL;
	struct eswap_key_point_record *record;
	ktime_t start, now;
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);

	ret = reclaim_check(memcg, require_size, &mcg_reclaim_sz);
	if (ret)
		return ret == -EAGAIN ? 0 : ret;

	//sched = eswap_malloc(sizeof(struct schedule_para), false, true);
	sched = kzalloc(sizeof(struct schedule_para), GFP_NOWAIT);
	if (unlikely(!sched)) {
		eswap_debug(ESD_ERR, "alloc sched failed!\n");
		stat_alloc_fail(ESWAP_OUT, -ENOMEM);
		return -ENOMEM;
	}

	record = &sched->record;
	start = ktime_get();
	esacct_start(record, start, ESWAP_OUT);

	INIT_LIST_HEAD(&sched->page_pool.page_pool_list);
	spin_lock_init(&sched->page_pool.page_pool_lock);

	sched->io_scheduler = eswap_io_scheduler_get_acct(est->zram, ESWAP_OUT, record,
				eswap_io_extent_done_cb, eswap_io_scheduler_done_cb, sched);
	if (unlikely(!sched->io_scheduler)) {
		eswap_free(sched);
		ret = -EIO;
		goto out;
	}

	now = ktime_get_boottime();
	while (*require_size) {
		int to_reclaim_kb = (int)(mcg_reclaim_sz / 1024);
		zram_trace_write_start(to_reclaim_kb);
		io_err = do_eswap_extent_out(memcg, sched, require_size, &mcg_reclaim_sz, now);
		zram_trace_write_end(to_reclaim_kb);
		if (io_err)
			break;
	}

	ret = eswap_io_scheduler_put(sched->io_scheduler);
	if (unlikely(ret)) {
		eswap_debug(ESD_ERR, "eswap write flush failed! %d\n", ret);
		stat_alloc_fail(ESWAP_OUT, ret);
		*require_size = 0;
		goto err_eswap_out;
	}

	/* if plug-finsh succeed, fall back to check the fault in ioerr
	 * note that EAGAIN indicats no error
	 * ret = io_err;
	 */
	ret = (io_err != -EAGAIN) ? io_err : 0;

err_eswap_out:
	atomic64_inc(&est->eswap_out_cnt);
out:
	return ret;
}

static inline void __eswapout_inc(void)
{
	struct eswap_stat *stat = eswap_get_stat_obj();
	atomic64_inc(&stat->eswapout_inflight);
}

static inline void __eswapout_dec(void)
{
	struct eswap_stat *stat = eswap_get_stat_obj();
	atomic64_dec(&stat->eswapout_inflight);
}

/* 1. Set process prio.
 * 2. Reclaim object from each memcg.
 */
static void eswap_out_work(struct work_struct *work)
{
	struct async_req *rq = container_of(work, struct async_req, work);
	int old_nice = task_nice(current);

	set_user_nice(current, rq->nice);
	__eswapout_inc();
	for_each_memcg(__do_memcg_eswap_out, &rq->size);
	__eswapout_dec();
	set_user_nice(current, old_nice);
	eswap_free(rq);
}

/* if eswap full, do not swap out */
static inline int is_eswap_full(void)
{
	long used_exts_num = (long) eswap_get_used_exts_num();
	long max_exts_num = (long) eswap_get_exts_num();

	if (used_exts_num  * 100  > max_exts_num * MAX_USED_EXT_RATIO)
		return 1;

	return 0;
}

extern u64 eswap_get_original_size(void);
void eswap_write_back_sync(unsigned long size)
{
	if (!eswap_enable() || !eswap_swapout_enable())
		return;

	if (eswap_get_original_size() >= (zram_kernel_wb_count_limit * SZ_4K))
		return;

	if (is_eswap_full())
		return;

	if (size >= 32 * SZ_1M)
		size = 32 * SZ_1M;

	// synchronized write back operation
	__eswapout_inc();
	for_each_memcg(__do_memcg_eswap_out, &size);
	__eswapout_dec();
}

/* This interface is for anon mem reclaim.
 * 1. Estimate push in size.
 * 2. Wakeup push in worker.
 */
unsigned long eswap_memcgs_out(unsigned long size)
{
	struct async_req *rq = NULL;
	unsigned long out_size = 0;

	if (!eswap_enable() || !eswap_swapout_enable())
		return 0;

	if (is_eswap_full())
		return 0;

	for_each_memcg(account_memcg_size, &out_size);

	// rq = eswap_malloc(sizeof(struct async_req), false, true);
	rq = kzalloc(sizeof(struct async_req), GFP_NOWAIT);
	if (unlikely(!rq)) {
		eswap_debug(ESD_ERR, "alloc async req fail!\n");
		stat_alloc_fail(ESWAP_OUT, -ENOMEM);
		return 0;
	}

	rq->size = size;
	rq->nice = task_nice(current);
	INIT_WORK(&rq->work, eswap_out_work);

	queue_work(eswapout_workqueue(), &rq->work);

	return out_size > size ? size : out_size;
}

static int do_eswap_extent_batch_in(struct schedule_para *sched,
	struct mem_cgroup *memcg, bool preload)
{
	int ret;
	struct io_extent *io_ext;
	struct io_extent_mgmt *io_ext_mgmt;

	io_ext = alloc_io_extent_acct(&sched->record,
				preload ? ESWAP_IN_PRELOAD : ESWAP_IN_BATCH,
				!preload, preload);
	if (!io_ext) {
		return -ENOMEM;
	}

	esacct_lat_start(&sched->record, ESWAP_FIND_EXTENT);
	io_ext_mgmt = eswap_extent_find_mgmt_by_memcg(memcg, &sched->page_pool);
	esacct_lat_end(&sched->record, ESWAP_FIND_EXTENT);
	if (IS_ERR(io_ext_mgmt)) {
		stat_alloc_fail(ESWAP_IN_BATCH, PTR_ERR(io_ext_mgmt));
		eswap_free(io_ext);
		return -EAGAIN;
	}

	io_extent_attach_mgmt(io_ext, io_ext_mgmt);

	ret = eswap_extent_read_acct(sched->io_scheduler, io_ext, &sched->record, ESWAP_IN_BATCH);
	return ret;
}

static int eswap_do_batch_in_init(struct schedule_para **out_sched,
	struct mem_cgroup *memcg, bool preload)
{
	struct schedule_para *sched = NULL;
	struct eswap_key_point_record *record;
	ktime_t start;
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);

	// sched = eswap_malloc(sizeof(struct schedule_para), !preload, preload);
	sched = kzalloc(sizeof(struct schedule_para), GFP_NOWAIT);
	if (unlikely(!sched)) {
		eswap_debug(ESD_ERR, "alloc sched failed!\n");
		stat_alloc_fail(ESWAP_IN_BATCH, -ENOMEM);
		return -ENOMEM;
	}

	record = &sched->record;
	start = ktime_get();
	esacct_start(record, start, preload ? ESWAP_IN_PRELOAD : ESWAP_IN_BATCH);

	INIT_LIST_HEAD(&sched->page_pool.page_pool_list);
	spin_lock_init(&sched->page_pool.page_pool_lock);

	sched->io_scheduler = eswap_io_scheduler_get_acct(est->zram,
				preload ? ESWAP_IN_PRELOAD : ESWAP_IN_BATCH, record,
				eswap_io_extent_done_cb, eswap_io_scheduler_done_cb, sched);
	if (unlikely(!sched->io_scheduler)) {
		eswap_free(sched);
		return -EIO;
	}

	*out_sched = sched;

	return 0;
}

/* This interface is for app warm start.
 * 1. Get all the extent ids from the memcg.
 * 2. Send extents to UFS one by one.
 */
static int eswap_do_batch_in(struct mem_cgroup *memcg,
	struct memcg_estrack *est, unsigned long size, bool preload)
{
	int ret = 0;
	int io_err = 0;
	struct schedule_para *sched = NULL;

	ret = eswap_do_batch_in_init(&sched, memcg, preload);
	if (unlikely(ret))
		return ret;

	while (size >= EXTENT_SIZE) {
		//if (do_eswap_extent_batch_in(sched, memcg, preload, &io_err))
		//	break;
		io_err = do_eswap_extent_batch_in(sched, memcg, preload);
		if (io_err)
			break;

		size -= EXTENT_SIZE;
	}

	ret = eswap_io_scheduler_put(sched->io_scheduler);
	if (unlikely(ret)) {
		eswap_debug(ESD_ERR, "eswap read flush failed! %d\n", ret);
		stat_alloc_fail(ESWAP_IN_BATCH, ret);
		atomic64_inc(&est->eswap_in_cnt);
		return ret;
	}

	// if plug-finsh succeed, errno fall back to check the fault in ioerr
	//ret = io_err;
	ret = (io_err != -EAGAIN) ? io_err : 0;

	atomic64_inc(&est->eswap_in_cnt);

	zram_report_for_preload(atomic_read(&MEMCG_ESTRACK_FIELD(memcg, eswap_stored_pages)));
	return ret;
}

int eswap_batch_in(struct mem_cgroup *memcg, unsigned long size, bool touch, bool preload)
{
	int ret;
	struct eswap_stat *stat;
	struct memcg_estrack *est;

	if (!eswap_enable())
		return 0;

	if (unlikely(!memcg)) {
		eswap_debug(ESD_ERR, "empty  memcg\n");
		return -EINVAL;

	}

	if (unlikely(!(est = mem_cgroup_estrack(memcg))->zram)) {
		eswap_debug(ESD_WARN, "no zram for memcg, %d\n", memcg->id.id);
		return -EINVAL;
	}

	stat = eswap_get_stat_obj();
	if (!size) {
		size = (unsigned long)(eswap_get_exts_num() << EXTENT_SHIFT);
	}

	atomic64_inc(&stat->eswapin_inflight);
	est->in_swapin = true;
	if (touch)
		mem_cgroup_app_value_promote(est);
	ret = eswap_do_batch_in(memcg, est, size, preload);
	est->in_swapin = false;
	atomic64_dec(&stat->eswapin_inflight);

	return ret;
}

static void fault_stat(struct zram *zram, u32 index)
{
	struct mem_cgroup *memcg = NULL;
	struct eswap_stat *stat = eswap_get_stat_obj();

	atomic64_inc(&stat->eswap_faultin_cnt_1);
	memcg = zram_get_memcg(zram, index);
	if (memcg)
		atomic64_inc(&MEMCG_ESTRACK_FIELD(memcg, eswap_fault_cnt));
}

static void fault_stat_2(struct zram *zram, u32 index)
{
	struct mem_cgroup *memcg= NULL;
	struct eswap_stat *stat = eswap_get_stat_obj();

	atomic64_inc(&stat->eswap_faultin_cnt_2);

	memcg = zram_get_memcg(zram, index);
	if (memcg) {
		atomic64_inc(&MEMCG_ESTRACK_FIELD(memcg, eswap_in_fault_cnt));

		if (!atomic64_read(&stat->eswapout_inflight) &&
		    !atomic64_read(&stat->eswapin_inflight))
			memcg_app_score_update_from_pf(memcg);
	}
}

static int fault_out_check(struct zram *zram, u32 index, unsigned long *zentry)
{
	int delay_count = 0;
	struct eswap_stat *stat = eswap_get_stat_obj();

	if (!eswap_enable())
		return -ENOENT;

	// robin.ren
	if (zram_test_flag(zram, index, ZRAM_FROM_ESWAP)) {
		zram_clear_flag(zram, index, ZRAM_FROM_ESWAP);
		atomic64_inc(&stat->eswap_fault_in_hits);
	}

	if (!zram_test_flag(zram, index, ZRAM_WB))
		return -ENOENT;

	fault_stat(zram, index);

#ifdef CONFIG_ZRAM_DEDUP
	*zentry = zram_get_direct_handle(zram, index);
#else
	*zentry = zram_get_element(zram, index);
#endif

	while (zram_test_flag(zram, index, ZRAM_BATCHING_OUT)) {
		unsigned long addr;

		zram_slot_unlock(zram, index);
		delay_count = wait_for_any_extent_readio_done_timeout(1, delay_count);
		zram_slot_lock(zram, index);

#ifdef CONFIG_ZRAM_DEDUP
		addr = zram_get_direct_handle(zram, index);
#else
		addr = zram_get_element(zram, index);
#endif
		if (!zram_test_flag(zram, index, ZRAM_WB)) {
			exit_wait_for_extent_readio(delay_count, "pf busy delayed count", &stat->faulting_1);
			return -ENOENT;
		}

		if (addr != *zentry) {
			exit_wait_for_extent_readio(delay_count, "pf busy delayed count", NULL);
			eswap_debug(ESD_ERR, "addr changed, index = %d,"
				" old = %lx, new = %lx\n", index, *zentry, addr);
			return -EIO;
		}
	}

	zram_set_flag(zram, index, ZRAM_BATCHING_OUT);
	zram_slot_unlock(zram, index);

	exit_wait_for_extent_readio(delay_count, "pf busy delayed count", &stat->faulting_1);
	return 0;
}

static int eswap_fault_in_get_extent(struct zram *zram, struct schedule_para *sched,
	struct io_extent *io_ext, struct io_extent_mgmt *io_ext_mgmt, unsigned long zentry, u32 index)
{
	int delay_count = 0;
	struct io_extent_mgmt *ret;

	/* note, io_ext_mgmt is non null here, it is a on stack object */
	esacct_lat_start(&sched->record, ESWAP_FIND_EXTENT);
	ret = eswap_extent_find_mgmt_by_idx(zentry, zram, io_ext_mgmt);
	esacct_lat_end(&sched->record, ESWAP_FIND_EXTENT);

	if (unlikely(PTR_ERR(ret) == -EBUSY)) {
		/* The extent maybe in unexpected case, wait here */
		while (1) {
			/* The extent doesn't exist in eswap */
			zram_slot_lock(zram, index);
			if (!zram_test_flag(zram, index, ZRAM_WB)) {
				zram_slot_unlock(zram, index);

				exit_wait_for_extent_readio(delay_count, "get extent busy delayed count",
						&(eswap_get_stat_obj()->fltin_extent_busy));
				return -EAGAIN;
			}
			zram_slot_unlock(zram, index);

			/* Get extent again */
			esacct_lat_start(&sched->record, ESWAP_FIND_EXTENT);
			ret = eswap_extent_find_mgmt_by_idx(zentry, zram, io_ext_mgmt);
			esacct_lat_end(&sched->record, ESWAP_FIND_EXTENT);
			if (likely(PTR_ERR(ret) != -EBUSY)) {
				break;
			}

			delay_count = wait_for_any_extent_readio_done_timeout(1, delay_count);
		}
	}

	exit_wait_for_extent_readio(delay_count, "get extent busy delayed count",
				&(eswap_get_stat_obj()->fltin_extent_busy));

	if (IS_ERR(ret)) {
		stat_alloc_fail(ESWAP_IN_FAULT, PTR_ERR(ret));
		return PTR_ERR(ret);
	}

	io_extent_attach_mgmt(io_ext, io_ext_mgmt);
	fault_stat_2(zram, index);

	return 0;
}

static int fault_out_exit_check(struct zram *zram, u32 index, unsigned long zentry, int ret)
{
	zram_slot_lock(zram, index);
	if (likely(!ret)) {
		if (zram_test_flag(zram, index, ZRAM_WB)) {
			eswap_debug(ESD_ERR, "still in WB, index = %d\n", index);
			ret = -EIO;
		}

		// robin.ren
		if (!ret)
			zram_clear_flag(zram, index, ZRAM_FROM_ESWAP);
	}
	zram_clear_flag(zram, index, ZRAM_BATCHING_OUT);

	return ret;
}

static int do_eswap_extent_fault_in(struct zram *zram, u32 index, struct schedule_para *sched,
	struct ioe_mgmt *ioe, struct io_extent_mgmt *io_ext_mgmt, unsigned long zentry)
{
	struct io_extent *io_ext = (struct io_extent *)ioe;
	int ret;

	ret = eswap_fault_in_get_extent(zram, sched, io_ext, io_ext_mgmt, zentry, index);
	if (ret)
		return ret;

	ret = eswap_extent_read_acct(sched->io_scheduler, io_ext, &sched->record, ESWAP_IN_FAULT);
	return ret;
}

/*
 * This interface will be called when ZRAM is read.
 * eswap should be searched before ZRAM is read.
 * This function require ZRAM slot lock being held.
 * 1. Find the extent in ZRAM by the index.
 * 2. if extent exist, dispatch it to UFS.
 */
int __eswap_fault_in(struct zram *zram, u32 index)
{
	int ret = 0;
	int io_err;
	struct schedule_para sched;
	struct ioe_mgmt ioe = {};
	struct io_extent_mgmt io_ext_mgmt = {};

	struct eswap_key_point_record *record = &sched.record;
	unsigned long zentry;
	ktime_t start;

	ret = fault_out_check(zram, index, &zentry);
	if (ret) {
		return (ret == -ENOENT) ? 0 : ret;
	}

	/* pass sensitive info to scheduler */
	ioe.io_ext.sensitive = update_read_stats(zram);

	// index is unlocked
	memset(record, 0, sizeof(struct eswap_key_point_record));

	start = ktime_get();
	esacct_start(record, start, ESWAP_IN_FAULT);

	INIT_LIST_HEAD(&sched.page_pool.page_pool_list);
	spin_lock_init(&sched.page_pool.page_pool_lock);

	zram_trace_read_start(zram, index);

	sched.io_scheduler = eswap_io_scheduler_get_acct(zram, ESWAP_IN_FAULT, record,
							eswap_io_extent_done_cb, NULL, NULL);
	if (unlikely(!sched.io_scheduler)) {
		ret = -EIO;
		eswap_record_fail(ESWAP_FAULT_OUT_INIT_FAIL, index, 0, record->task_comm);
		goto out_trace;
	}

	io_err = do_eswap_extent_fault_in(zram, index, &sched, &ioe, &io_ext_mgmt, zentry);

	ret = eswap_io_scheduler_put(sched.io_scheduler);
	if (unlikely(ret)) {
		eswap_debug(ESD_ERR, "eswap flush failed! %d\n", ret);
		stat_alloc_fail(ESWAP_IN_FAULT, ret);
		goto out_trace;
	}
	
	// if plug-finsh succeed, fall back to check the fault in ioerr
	ret = (io_err != -EAGAIN) ? io_err : 0;

out_trace:
	zram_trace_read_end(zram, index);

	esacct_lat_start(record, ESWAP_ZRAM_LOCK);
	ret = fault_out_exit_check(zram, index, zentry, ret);
	esacct_lat_end(record, ESWAP_ZRAM_LOCK);
	esacct_end(record);
	return ret;
}

struct eswap_pageout_work {
	u32 index;
	int ret;
	unsigned long zentry;
	bool sensitive;
	struct zram *zram;
	struct work_struct work;
	struct completion done;

	struct schedule_para sched;
};

static void eswap_fault_in_workfn(struct work_struct *work)
{
	int ret = 0;
	int io_err;
	struct eswap_pageout_work *epw = container_of(work, struct eswap_pageout_work, work);
	struct zram *zram = epw->zram;
	u32 index = epw->index;
	unsigned long zentry = epw->zentry;
	struct schedule_para *sched = &epw->sched;
	struct ioe_mgmt ioe = {};
	struct io_extent_mgmt io_ext_mgmt = {};
	struct eswap_key_point_record *record = &sched->record;

	/* pass sensitive info to scheduler */
	ioe.io_ext.sensitive = epw->sensitive;

	zram_trace_read_start(zram, index);

	sched->io_scheduler = eswap_io_scheduler_get_acct(zram, ESWAP_IN_FAULT, record,
							eswap_io_extent_done_cb, NULL, NULL);
	if (unlikely(!sched->io_scheduler)) {
		ret = -EIO;
		eswap_record_fail(ESWAP_FAULT_OUT_INIT_FAIL, index, 0, record->task_comm);
		goto out_trace;
	}

	io_err = do_eswap_extent_fault_in(zram, index, sched, &ioe, &io_ext_mgmt, zentry);

	ret = eswap_io_scheduler_put(sched->io_scheduler);
	if (unlikely(ret)) {
		eswap_debug(ESD_ERR, "eswap flush failed! %d\n", ret);
		stat_alloc_fail(ESWAP_IN_FAULT, ret);
		goto out_trace;
	}

	// if plug-finsh succeed, fall back to check the fault in ioerr
	ret = (io_err != -EAGAIN) ? io_err : 0;

out_trace:
	zram_trace_read_end(zram, index);

	epw->ret = ret;
	complete(&epw->done);
}

int eswap_fault_in(struct zram *zram, u32 index)
{
	int ret = 0;
	unsigned long zentry;
	struct eswap_pageout_work epw = {};
	struct eswap_key_point_record *record = &epw.sched.record;

	ret = fault_out_check(zram, index, &zentry);
	if (ret) {
		return (ret == -ENOENT) ? 0 : ret;
	}

	/* index is unlocked */
	memset(record, 0, sizeof(struct eswap_key_point_record));

	INIT_LIST_HEAD(&epw.sched.page_pool.page_pool_list);
	spin_lock_init(&epw.sched.page_pool.page_pool_lock);

	epw.sensitive = update_read_stats(zram);
	epw.zram = zram;
	epw.index = index;
	epw.zentry = zentry;

	/* on stack work and complete */
	init_completion(&epw.done);
	INIT_WORK_ONSTACK(&epw.work, eswap_fault_in_workfn);

	esacct_start(record, ktime_get(), ESWAP_IN_FAULT);
	queue_work(pageout_workqueue(), &epw.work);
	wait_for_completion(&epw.done);

	/* work is one */
	ret = epw.ret;
	esacct_lat_start(record, ESWAP_ZRAM_LOCK);

	ret = fault_out_exit_check(zram, index, zentry, ret);
	esacct_lat_end(record, ESWAP_ZRAM_LOCK);
	esacct_end(record);
	return ret;
}

/*
 * This interface will be called when ZRAM is freed.
 * eswap entry should be deleted before ZRAM is freed.
 * If obj can be deleted from ZRAM,
 * return true, otherwise return false.
 * 1. Find the extent in ZRAM by the index.
 * 2. Delete the zs Object in the extent.
 * 3. If extent is empty, free the extent.
 */
bool eswap_delete(struct zram *zram, u32 index)
{
	if (!eswap_enable())
		return true;

	if (zram_test_flag(zram, index, ZRAM_UNDER_WB) ||
			zram_test_flag(zram, index, ZRAM_BATCHING_OUT)) {
		struct eswap_stat *stat = eswap_get_stat_obj();

		if (zram_test_flag(zram, index, ZRAM_BATCHING_OUT))
			atomic64_inc(&stat->faulting_3);

		atomic64_inc(&stat->miss_free);
		return false;
	}

	return true;
}

void eswap_mem_cgroup_remove(struct mem_cgroup *memcg)
{
	if (!eswap_enable())
		return;
	eswap_manager_memcg_deinit(memcg);
}

#ifdef CONFIG_ESWAP_IDLE_WRITEBACK
void eswap_idle_inc(struct zram *zram, u32 index)
{
	if (!eswap_enable())
		return;

	if (zram_test_flag(zram, index, ZRAM_WB))
		return;

	if (!zram_test_flag(zram, index, ZRAM_IDLE))
		memcg_idle_inc(zram, index);
}

void eswap_idle_dec(struct zram *zram, u32 index)
{
	if (!eswap_enable())
		return;

	if (zram_test_flag(zram, index, ZRAM_WB))
		return;

	if (zram_test_flag(zram, index, ZRAM_IDLE))
		memcg_idle_dec(zram, index);
}

void eswap_idle_writeback(void)
{
	struct mem_cgroup *memcg = NULL;
	unsigned long idle_size;

	if (!eswap_enable() || !eswap_swapout_enable())
		return;

	eswap_debug(ESD_DEBUG, "idle reclaim start.\n");
	task_set_eswap_idle_reclaim(current);
	for (memcg = get_next_memcg(NULL); memcg; memcg = get_next_memcg(memcg))
		if (memcg == root_mem_cgroup ||
				!strcmp(MEMCG_ESTRACK_FIELD(memcg, name), "activityApp")) {
			idle_size = atomic64_read(&MEMCG_ESTRACK_FIELD(memcg, zram_idle_size));
			__do_memcg_eswap_out(memcg, &idle_size);
		}
	task_clear_eswap_idle_reclaim(current);
	eswap_debug(ESD_DEBUG, "idle reclaim end.\n");
}
#endif

int memcg_eswap_out(struct mem_cgroup *memcg)
{
	unsigned long size = FT_RECLAIM_SZ * MIN_RECLAIM_ZRAM_SZ;

	if (!eswap_swapout_enable()) {
		eswap_debug(ESD_ERR, "reclaim_in disable!\n");
		return -EINVAL;
	}

	return __do_memcg_eswap_out(memcg, &size);
}

int memcg_eswap_batch_in(struct mem_cgroup *memcg, bool touch, bool preload)
{
	return eswap_batch_in(memcg, FT_RECLAIM_SZ * MIN_RECLAIM_ZRAM_SZ, touch, preload);
}

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
