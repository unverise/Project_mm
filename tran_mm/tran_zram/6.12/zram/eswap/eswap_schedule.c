#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/bio.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sched/task.h>
#include <linux/list.h>
#include <linux/random.h>
#include <linux/zsmalloc.h>
#include <linux/delay.h>

#include "eswap_schedule.h"
#include "eswap_acct.h"
#include "eswap_area.h"

#define __SECTOR_SHIFT 9
#define PAGE_SIZE_SECTOR (PAGE_SIZE >> __SECTOR_SHIFT)
#define MAX_INFILGHT_NUM 256

/* 1 extent takes EXTENT_SIZE, 32KB, EXTENT_PG_CNT, 8 pages,
 * 1 segment takes BIO_MAX_PAGES pages at most, 256 pages at most
 */
struct io_segment {
	sector_t start;
	struct block_device *bdev;
	struct list_head io_extents;
	struct work_struct endio_work;
	struct eswap_io_scheduler *sched;

	ktime_t time_submit_bio;
	ktime_t time_end_io;
	u32 bio_result;
	int ext_id;
	int page_cnt;
};

struct eswap_io_scheduler {
	struct eswap_io_scheduler_config config;
	struct kref refcount;
	struct mutex refmutex;
	struct wait_queue_head io_wait;
	atomic_t pages_inflight;
	struct completion io_end_flag;
	struct io_segment *segment;
	bool limit_inflight_flag;
	bool wait_io_finish_flag;
	int page_cnt;
	int segment_cnt;
	int nice;
};

static struct workqueue_struct *read_wq;
static struct workqueue_struct *write_wq;

static inline void stat_io_scheduler_bytes(struct eswap_io_scheduler *sched)
{
	struct eswap_stat *stat = eswap_get_stat_obj();

	if (!sched->page_cnt)
		return;

	/* other statics moved to end_io_segment,
	 * to make thing accurate and make external watcher sync with it
	 */
	if (sched->config.scenario == ESWAP_OUT)
		atomic64_inc(&stat->eswapout_cnt);
	else
		atomic64_inc(&stat->eswapin_cnt);
}

static void io_scheduler_release(struct kref *ref)
{
	struct eswap_io_scheduler *sched = container_of(ref, struct eswap_io_scheduler, refcount);

	if (sched->config.complete_notify && sched->config.private) {
		sched->config.complete_notify(sched->config.private);
	}

	kfree(sched);
}

static void io_segment_free(struct eswap_io_scheduler *sched, struct io_segment *segment)
{
	struct io_extent *io_ext, *tmp;
	list_for_each_entry_safe(io_ext, tmp, &segment->io_extents, list)  {
		list_del(&io_ext->list);
		sched->config.io_extent_done_cb(io_ext, sched->config.scenario, -EIO);
	}

	kfree(segment);
}

static void limit_inflight(struct eswap_io_scheduler *sched)
{
	int ret;
	if (!sched->limit_inflight_flag)
		return;

	if (atomic_read(&sched->pages_inflight) >= MAX_INFILGHT_NUM) {
		do {
			eswap_debug(ESD_DEBUG, "wait inflight start\n");
			ret = wait_event_timeout(sched->io_wait,
				atomic_read(&sched->pages_inflight) < MAX_INFILGHT_NUM,
				msecs_to_jiffies(100));
		} while (!ret);
	}
}

static void wait_io_scheduler_finish(struct eswap_io_scheduler *sched)
{
	int ret;
	unsigned int wait_time;

	if (!sched->wait_io_finish_flag || !sched->page_cnt)
		return;

	if (sched->config.scenario == ESWAP_IN_FAULT) {
		/* wait forever for page fault */
		eswap_debug(ESD_DEBUG, "fault out wait finish start\n");
		wait_for_completion_io_timeout(&sched->io_end_flag, MAX_SCHEDULE_TIMEOUT);
		return;
	}

	wait_time = (sched->config.scenario == ESWAP_OUT) ? 100 : 10;

	do {
		eswap_debug(ESD_DEBUG, "wait finish start\n");
		ret = wait_event_timeout(sched->io_wait,
					 (!atomic_read(&sched->pages_inflight)),
					 msecs_to_jiffies(wait_time));
	} while (!ret);
}

static void inflight_inc(struct io_segment *segment)
{
	mutex_lock(&segment->sched->refmutex);
	kref_get(&segment->sched->refcount);
	mutex_unlock(&segment->sched->refmutex);

	atomic_add(segment->page_cnt, &segment->sched->pages_inflight);
}

static void inflight_dec(struct eswap_io_scheduler *sched, int num)
{
	if ((atomic_sub_return(num, &sched->pages_inflight) <
		MAX_INFILGHT_NUM) && sched->limit_inflight_flag &&
		wq_has_sleeper(&sched->io_wait))
		wake_up(&sched->io_wait);
}

static void io_end_wake_up(struct eswap_io_scheduler *sched)
{
	if (sched->config.scenario == ESWAP_IN_FAULT) {
		complete(&sched->io_end_flag);
		return;
	}

	if (wq_has_sleeper(&sched->io_wait))
		wake_up(&sched->io_wait);
}

static inline void io_err_record(enum eswap_fail_point point, struct eswap_io_scheduler *sched, int ext_id)
{
	if (sched->config.scenario == ESWAP_IN_FAULT)
		eswap_record_fail(point, 0, ext_id, sched->config.record->task_comm);
}

static void end_io_segment(struct work_struct *work)
{
	struct io_segment *segment = container_of(work, struct io_segment, endio_work);
	struct eswap_io_scheduler *sched = segment->sched;
	struct eswap_key_point_record *record = sched->config.record;
	enum eswap_scenario scen = sched->config.scenario;
	struct eswap_stat *stat = eswap_get_stat_obj();
	int old_nice = task_nice(current);
	ktime_t work_start;
	s64 iodelay;
	struct io_extent *io_ext, *tmp;

	iodelay = ktime_us_delta(segment->time_end_io, segment->time_submit_bio);

	/* one segment is one BIO */
	if (scen == ESWAP_OUT) {
		set_user_nice(current, sched->nice);
		atomic64_add(iodelay, &stat->eswap_bd_write_delay);
		eswap_area_inc_bd_writes(sched->config.zram, segment->ext_id, segment->page_cnt);

		// one segment is one BIO
		atomic64_inc(&stat->eswap_bd_writes);
		atomic64_add(segment->page_cnt * PAGE_SIZE, &stat->eswapout_bytes);

	} else if (scen == ESWAP_IN_FAULT) {
		io_ext = list_first_entry(&segment->io_extents, struct io_extent, list);

		atomic64_add(iodelay, &stat->eswap_bd_read_delay);
		eswap_area_inc_bd_reads(sched->config.zram, segment->ext_id, segment->page_cnt);
		atomic64_inc(&stat->eswap_bd_reads);
		atomic64_add(segment->page_cnt * PAGE_SIZE, &stat->eswapin_bytes);

		if (io_ext->sensitive)
			atomic64_add(iodelay, &stat->sensitive_delay);

		if (iodelay > atomic64_read(&stat->eswap_bd_read_max_delay))
			atomic64_set(&stat->eswap_bd_read_max_delay, iodelay);

		if (iodelay <= ZRAM_IODELAY_500us)
			atomic64_inc(&stat->io_lt_500);
		else if (iodelay <= ZRAM_IODELAY_1000us)
			atomic64_inc(&stat->io_lt_1000);
		else if (iodelay <= ZRAM_IODELAY_3000us)
			atomic64_inc(&stat->io_lt_3000);
		else if (iodelay <= ZRAM_IODELAY_5000us)
			atomic64_inc(&stat->io_lt_5000);
		else if (iodelay <= ZRAM_IODELAY_30000us)
			atomic64_inc(&stat->io_lt_30000);
		else if (iodelay <= ZRAM_IODELAY_50000us)
			atomic64_inc(&stat->io_lt_50000);
		else
			atomic64_inc(&stat->io_gt_50000);

		update_psi_io(sched->config.zram, iodelay, true);
		update_psi_mem(sched->config.zram, iodelay, true);
	} else {
		atomic64_inc(&stat->eswap_bd_batch_reads);
		atomic64_add(segment->page_cnt * PAGE_SIZE, &stat->eswapin_bytes);
	}

	if (unlikely(segment->bio_result)) {
		atomic64_inc(&stat->io_fail_cnt[scen]);

		io_ext = list_first_entry(&segment->io_extents, struct io_extent, list);
		io_err_record(ESWAP_FAULT_OUT_IO_FAIL, sched, io_ext->ext_id);

		list_for_each_entry_safe(io_ext, tmp, &segment->io_extents, list)  {
			eswap_debug(ESD_ERR, "io err extent_id %d page_num %d\n", io_ext->ext_id, io_ext->pages_sz);
			list_del(&io_ext->list);
			sched->config.io_extent_done_cb(io_ext, scen, -EIO);
		}

		goto out;
	}

	esacct_async_perf(record, ESWAP_SCHED_WORK, segment->time_end_io);
	work_start = ktime_get();

	list_for_each_entry_safe(io_ext, tmp, &segment->io_extents, list)  {
		ktime_t callback_start;

		eswap_debug(ESD_DEBUG, "extent_id %d page_num %d\n", io_ext->ext_id, io_ext->pages_sz);
		list_del(&io_ext->list);

		callback_start = ktime_get();
		sched->config.io_extent_done_cb(io_ext, scen, 0);
		esacct_async_perf(record, ESWAP_CALL_BACK, callback_start);
	}

	/* user NULL io_extent and 0 err
	 * to tell up level segment is done successfully
	 */
	sched->config.io_extent_done_cb(NULL, scen, 0);

	esacct_async_perf(record, ESWAP_END_WORK, work_start);

out:
	inflight_dec(sched, segment->page_cnt);
	kfree(segment);

	io_end_wake_up(sched);
	kref_put_mutex(&sched->refcount, io_scheduler_release, &sched->refmutex);

	if (scen == ESWAP_OUT)
		set_user_nice(current, old_nice);
}

static void end_segment_bio(struct bio *bio)
{
	struct io_segment *segment = bio->bi_private;
	struct eswap_io_scheduler *sched = NULL;
	struct workqueue_struct *workqueue = NULL;
	struct eswap_key_point_record *record = NULL;

	if (unlikely(!segment || !(segment->sched))) {
		eswap_debug(ESD_ERR, "segment or sched null\n");
		bio_put(bio);
		return;
	}

	sched = segment->sched;
	record = sched->config.record;

	esacct_async_perf(record, ESWAP_END_IO, segment->time_submit_bio);

	workqueue = (sched->config.scenario == ESWAP_OUT) ? write_wq : read_wq;
	segment->time_end_io = ktime_get();
	segment->bio_result = bio->bi_status;
	queue_work(workqueue, &segment->endio_work);
	bio_put(bio);
}

static bool io_extent_can_merge_back(struct io_segment *segment, struct io_extent *io_ext)
{
	struct io_extent *tail_io_ext = list_last_entry(&segment->io_extents, struct io_extent, list);
	return ((tail_io_ext->addr + tail_io_ext->pages_sz * PAGE_SIZE_SECTOR) == io_ext->addr);
}

static bool io_extent_can_merge_front(struct io_segment *segment, struct io_extent *io_ext)
{
	struct io_extent *head_io_ext = list_first_entry(&segment->io_extents, struct io_extent, list);
	return (head_io_ext->addr == (io_ext->addr + io_ext->pages_sz * PAGE_SIZE_SECTOR));
}

static bool try_to_merge_io_extent(struct io_segment *segment, struct io_extent *io_ext)
{
	if ((segment->page_cnt + io_ext->pages_sz) > BIO_MAX_PAGES)
		return false;

	if (segment->bdev != io_ext->bdev)
		return false;

	if (io_extent_can_merge_front(segment, io_ext)) {
		list_add(&io_ext->list, &segment->io_extents);
		segment->page_cnt += io_ext->pages_sz;
		segment->start = io_ext->addr;
		return true;
	}

	if (io_extent_can_merge_back(segment, io_ext)) {
		list_add_tail(&io_ext->list, &segment->io_extents);
		segment->page_cnt += io_ext->pages_sz;
		return true;
	}

	return false;
}

static struct bio *segment_bio_alloc(enum eswap_scenario scenario, int page_cnt, struct block_device *bdev)
{
	unsigned int op = (scenario == ESWAP_OUT) ? REQ_OP_WRITE : REQ_OP_READ;
	gfp_t gfp = (scenario != ESWAP_OUT) ? GFP_ATOMIC : GFP_NOIO;
	// struct bio *bio = bio_alloc(NULL, BIO_MAX_PAGES, REQ_SYNC, gfp);
	struct bio *bio = bio_alloc(bdev, page_cnt, op|REQ_SYNC, gfp);

	if (!bio && (scenario == ESWAP_IN_FAULT))
		// bio = bio_alloc(NULL, BIO_MAX_PAGES, REQ_SYNC, GFP_NOIO);
		bio = bio_alloc(bdev, page_cnt, op|REQ_SYNC, GFP_NOIO);

	return bio;
}

static int segment_bio_add_page(struct bio *bio, struct io_segment *segment)
{
	struct io_extent *io_ext = NULL;
	int i;
	// int k = 0;

	list_for_each_entry(io_ext, &segment->io_extents, list)  {
		for (i = 0; i < io_ext->pages_sz; i++) {
			// io_ext->dest_pages[i]->index = bio->bi_iter.bi_sector + k;
			// k += PAGE_SIZE_SECTOR;
			__bio_add_page(bio, io_ext->dest_pages[i], PAGE_SIZE, 0);
		}
	}

	return 0;
}

static void segment_set_bio_opf(struct bio *bio, struct io_segment *segment)
{
	if (segment->sched->config.scenario == ESWAP_OUT) {
		bio->bi_opf |= REQ_BACKGROUND;
		return;
	}

	bio->bi_opf |= REQ_SYNC | REQ_FG;

#if IS_ENABLED(CONFIG_TRANSSION_BLK)
	if (schedule_support_vip_in_fault() && segment->sched->config.scenario == ESWAP_IN_FAULT) {
		bio->bi_opf |= REQ_FG;
	}
#endif
}

static int __submit_segment_bio(struct io_segment *segment)
{
	struct eswap_io_scheduler *sched = segment->sched;
	//unsigned int op = (sched->config.scenario == ESWAP_OUT) ? REQ_OP_WRITE : REQ_OP_READ;
	struct eswap_key_point_record *record = sched->config.record;
	struct io_extent *head_io_entry = list_first_entry(&segment->io_extents, struct io_extent, list);
	struct bio *bio = NULL;

	esacct_lat_start(record, ESWAP_BIO_ALLOC);
	bio = segment_bio_alloc(sched->config.scenario, segment->page_cnt, segment->bdev);
	esacct_lat_end(record, ESWAP_BIO_ALLOC);
	if (unlikely(!bio)) {
		eswap_debug(ESD_ERR, "bio is null.\n");
		io_err_record(ESWAP_FAULT_OUT_BIO_ALLOC_FAIL, sched, head_io_entry->ext_id);
		return -ENOMEM;
	}

	//bio_set_dev(bio, segment->bdev);

	bio->bi_iter.bi_sector = segment->start;
	bio->bi_private = segment;
	//kernel-6.12 removed
	//bio_set_op_attrs(bio, op, 0);
	bio->bi_end_io = end_segment_bio;
	segment_set_bio_opf(bio, segment);

	if (unlikely(segment_bio_add_page(bio, segment))) {
		bio_put(bio);
		eswap_debug(ESD_ERR, "bio_add_page fail\n");
		io_err_record(ESWAP_FAULT_OUT_BIO_ADD_FAIL, sched,  head_io_entry->ext_id);
		return -EIO;
	}

	inflight_inc(segment);

	eswap_debug(ESD_DEBUG, "submit-bio %px bi_end_io %px seg:%px sector %llu ext_id %d scenario %d bi_opf:%d\n",
		bio, bio->bi_end_io, segment, segment->start, head_io_entry->ext_id, sched->config.scenario, bio->bi_opf);

	sched->page_cnt += segment->page_cnt;
	sched->segment_cnt++;
	segment->time_submit_bio = ktime_get();

	esacct_lat_start(record, ESWAP_SUBMIT_BIO);
	submit_bio(bio);
	esacct_lat_end(record, ESWAP_SUBMIT_BIO);
	return 0;
}

static int eswap_new_segment_init(struct eswap_io_scheduler *sched, struct io_extent *io_ext)
{
	// gfp_t gfp = (sched->config.scenario != ESWAP_OUT) ? GFP_ATOMIC : GFP_NOIO;
	struct io_segment *segment = NULL;
	struct eswap_key_point_record *record = sched->config.record;

	esacct_lat_start(record, ESWAP_SEGMENT_ALLOC);
	// segment = kzalloc(sizeof(struct io_segment), gfp);
	segment = kzalloc(sizeof(struct io_segment), GFP_NOWAIT);
	if (!segment && (sched->config.scenario == ESWAP_IN_FAULT))
		segment = kzalloc(sizeof(struct io_segment), GFP_NOIO);
	esacct_lat_end(record, ESWAP_SEGMENT_ALLOC);
	if (unlikely(!segment)) {
		eswap_debug(ESD_ERR, "fail NOMEM\n");
		io_err_record(ESWAP_FAULT_OUT_SEGMENT_ALLOC_FAIL, sched, io_ext->ext_id);
		return -ENOMEM;
	}

	segment->sched = sched;
	INIT_LIST_HEAD(&segment->io_extents);
	list_add_tail(&io_ext->list, &segment->io_extents);
	segment->page_cnt = io_ext->pages_sz;
	INIT_WORK(&segment->endio_work, end_io_segment);
	segment->start = io_ext->addr;
	segment->bdev = io_ext->bdev;
	segment->ext_id = io_ext->ext_id;

	sched->segment = segment;
	return 0;
}

static int submit_io_segment(struct eswap_io_scheduler *sched)
{
	int ret;
	struct io_segment *segment = sched->segment;

	limit_inflight(sched);

	ret = __submit_segment_bio(segment);
	if (unlikely(ret))
		io_segment_free(sched, segment);

	sched->segment = NULL;
	return ret;
}

static bool check_io_extent(struct io_extent *io_ext)
{
	int i;

	if (unlikely(!io_ext)) {
		eswap_debug(ESD_ERR, "io_ext null\n");

		return true;
	}

	if (unlikely((!io_ext->dest_pages) ||
		(io_ext->ext_id < 0) ||
		(io_ext->pages_sz > BIO_MAX_PAGES) ||
		(io_ext->pages_sz <= 0))) {
		eswap_debug(ESD_ERR, "ext_id %d, page_sz %d\n", io_ext->ext_id,
			io_ext->pages_sz);

		return true;
	}

	for (i = 0; i < io_ext->pages_sz; ++i) {
		if (!io_ext->dest_pages[i]) {
			eswap_debug(ESD_ERR, "dest_pages[%d] is null\n", i);
			return true;
		}
	}

	return false;
}

int eswap_extent_io(void *io_scheduler, struct io_extent *io_ext)
{
	struct eswap_io_scheduler *sched = (struct eswap_io_scheduler *)io_scheduler;
	struct io_segment *segment = sched->segment;
	int ret;

	if (unlikely(check_io_extent(io_ext))) {
		io_err_record(ESWAP_FAULT_OUT_IO_ENTRY_PARA_FAIL, sched,
				io_ext ? io_ext->ext_id : -EINVAL);
		sched->config.io_extent_done_cb(io_ext, sched->config.scenario, -EIO);
		return -EFAULT;
	}

	eswap_debug(ESD_DEBUG, "ext id %d, pages_sz %d, addr %llx\n",
		io_ext->ext_id, io_ext->pages_sz, io_ext->addr);

	if (segment != NULL) {
		/* segment merge io_ext, will consume this io_ext */
		if (try_to_merge_io_extent(segment, io_ext)) {
			if (segment->page_cnt < BIO_MAX_PAGES) {
				/* segment not full, keep merging */
				return 0;
			}

			/* segment full, submit */
			return submit_io_segment(sched);
		}

		/* merge failed, submit io_exts submitted before */
		ret = submit_io_segment(sched);
		if (unlikely(ret)) {
			eswap_debug(ESD_ERR, "submit fail %d\n", ret);
			sched->config.io_extent_done_cb(io_ext, sched->config.scenario, -EIO);
			return ret;
		}
	}

	/* sche->segment is NULL here 
	 * construct new segment for the new coming io_ext
	 */
	ret = eswap_new_segment_init(sched, io_ext);
	if (unlikely(ret)) {
		sched->config.io_extent_done_cb(io_ext, sched->config.scenario, ret);
		return ret;
	}

	return 0;
}

int eswap_io_scheduler_init(void)
{
	read_wq = alloc_workqueue("eswap_iosched_rwq", WQ_HIGHPRI | WQ_UNBOUND, 0);
	if (unlikely(!read_wq))
		return -EFAULT;

	write_wq = alloc_workqueue("eswap_iosched_wwq", WQ_CPU_INTENSIVE, 0);
	if (unlikely(!write_wq)) {
		destroy_workqueue(read_wq);
		return -EFAULT;
	}

	return 0;
}

void eswap_io_scheduler_deinit(void)
{
	destroy_workqueue(write_wq);
	destroy_workqueue(read_wq);
}

void *eswap_io_scheduler_get(struct eswap_io_scheduler_config *config)
{
	// gfp_t gfp;
	struct eswap_io_scheduler *sched = NULL;
	enum eswap_scenario scen = config->scenario;

	if (config->io_extent_done_cb == NULL || config->zram == NULL)
		return NULL;

	// gfp = (scen != ESWAP_OUT) ? GFP_ATOMIC : GFP_NOIO;
	// sched = kzalloc(sizeof(struct eswap_io_scheduler), gfp);
	sched = kzalloc(sizeof(struct eswap_io_scheduler), GFP_NOWAIT);
	if (!sched && (scen == ESWAP_IN_FAULT))
		sched = kzalloc(sizeof(struct eswap_io_scheduler), GFP_NOIO);

	if (unlikely(!sched)) {
		eswap_debug(ESD_ERR, "io_scheduler null\n");
		return NULL;
	}

	kref_init(&sched->refcount);
	mutex_init(&sched->refmutex);
	atomic_set(&sched->pages_inflight, 0);
	init_waitqueue_head(&sched->io_wait);

	sched->config.zram = config->zram;
	sched->config.scenario = scen;
	sched->config.io_extent_done_cb = config->io_extent_done_cb;
	sched->config.complete_notify = config->complete_notify;
	sched->config.private = config->private;
	sched->config.record = config->record;
	sched->limit_inflight_flag = (scen == ESWAP_OUT) || (scen == ESWAP_IN_PRELOAD);
	sched->wait_io_finish_flag = (scen == ESWAP_OUT) || (scen == ESWAP_IN_FAULT);
	sched->nice = task_nice(current);
	init_completion(&sched->io_end_flag);
	return (void *)sched;
}

/* io_scheduler validity guaranteed by the caller */
int eswap_io_scheduler_put(void *io_scheduler)
{
	int ret = 0;
	struct eswap_io_scheduler *sched = (struct eswap_io_scheduler *)io_scheduler;
	enum eswap_scenario scen = sched->config.scenario;
	struct io_segment *segment = sched->segment;

	esacct_lat_start(sched->config.record, ESWAP_IO_EXTENT);
	if (segment != NULL) {
		ret = submit_io_segment(sched);
		if (unlikely(ret))
			eswap_debug(ESD_ERR, "submit fail %d\n", ret);
	}
	esacct_lat_end(sched->config.record, ESWAP_IO_EXTENT);

	if (scen == ESWAP_IN_FAULT)
		set_user_nice(current, MIN_NICE);
	wait_io_scheduler_finish(sched);
	if (scen == ESWAP_IN_FAULT)
		set_user_nice(current, sched->nice);

	esacct_lat_point(sched->config.record, ESWAP_WAKE_UP);

	stat_io_scheduler_bytes(sched);

	esacct_io_stat(sched->config.record, sched->page_cnt, sched->segment_cnt);

	kref_put_mutex(&sched->refcount, io_scheduler_release, &sched->refmutex);
	eswap_debug(ESD_DEBUG, "io schedule finish succ\n");
	return ret;
}
