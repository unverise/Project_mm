#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/timer.h>
#include <linux/zsmalloc.h>
#include <linux/spinlock.h>
#include <linux/sched/task.h>
#include <linux/version.h>
#include <linux/fs.h>

#include "eswap_acct.h"

#define DUMP_BUF_LEN 512

// threshold in ms
static unsigned long warning_threshold[ESWAP_SCENARIO_NUM] = {
	0,
	5,
	5000,
	0
};

const char *key_point_name[ESWAP_KYE_POINT_BUTT] = {
	"START",
	"INIT",
	"IOENTRY_ALLOC",
	"FIND_EXTENT",
	"IO_EXTENT",
	"SEGMENT_ALLOC",
	"BIO_ALLOC",
	"SUBMIT_BIO",
	"END_IO",
	"SCHED_WORK",
	"END_WORK",
	"CALL_BACK",
	"WAKE_UP",
	"ZRAM_LOCK",
	"DONE"
};

static void dump_point_lat(struct eswap_key_point_record *record, ktime_t start)
{
	int i;

	for (i = 0; i < ESWAP_KYE_POINT_BUTT; ++i) {
		if (!record->key_point[i].record_cnt)
			continue;

		eswap_debug(ESD_ERR,
			"%s diff %lld cnt %u end %u lat %lld\n",
			key_point_name[i],
			ktime_us_delta(record->key_point[i].first_time, start),
			record->key_point[i].record_cnt,
			record->key_point[i].end_cnt,
			record->key_point[i].proc_total_time);
	}
}

static void dump_no_record_point(struct eswap_key_point_record *record, char *log, unsigned int *count)
{
	int i;
	unsigned int point = 0;

	for (i = 0; i < ESWAP_KYE_POINT_BUTT; ++i)
		if (record->key_point[i].record_cnt)
			point = i;

	point++;
	if (point < ESWAP_KYE_POINT_BUTT)
		*count += snprintf(log + *count,
			(size_t)(DUMP_BUF_LEN - *count),
			" no_record_point %s", key_point_name[point]);
	else
		*count += snprintf(log + *count,
			(size_t)(DUMP_BUF_LEN - *count), " all_point_record");
}

static long long calc_speed(s64 page_cnt, s64 time)
{
	s64 size;

	if (!page_cnt)
		return 0;

	size = page_cnt * PAGE_SIZE * BITS_PER_BYTE;
	if (time)
		return size * USEC_PER_SEC / time;
	else
		return S64_MAX;
}

static void dump_lat(struct eswap_key_point_record *record, ktime_t curr_time, bool perf_end_flag)
{
	char log[DUMP_BUF_LEN] = { 0 };
	unsigned int count = 0;
	ktime_t start;
	s64 total_time;

	start = record->key_point[ESWAP_START].first_time;
	total_time = ktime_us_delta(curr_time, start);
	count += snprintf(log + count,
		(size_t)(DUMP_BUF_LEN - count),
		"totaltime(us) %lld scenario %u task %s nice %d",
		total_time, record->scenario, record->task_comm, record->nice);

	if (perf_end_flag)
		count += snprintf(log + count, (size_t)(DUMP_BUF_LEN - count),
			" page %d segment %d speed(bps) %lld threshold %lu",
			record->page_cnt, record->segment_cnt,
			calc_speed(record->page_cnt, total_time),
			record->warning_threshold);
	else
		count += snprintf(log + count, (size_t)(DUMP_BUF_LEN - count),
			" state %c", task_state_to_char(record->task));

	dump_no_record_point(record, log, &count);

	eswap_debug(ESD_ERR, "perf end flag %u %s\n", perf_end_flag, log);

	if (eswap_acct_detail())
		dump_point_lat(record, start);
}

static unsigned long esacct_warning_threshold(enum eswap_scenario scenario)
{
	if (!eswap_acct_detail())
		return 0;

	if (unlikely(scenario >= ESWAP_SCENARIO_NUM))
		return 0;

	return warning_threshold[scenario];
}

static void esacct_warning(struct timer_list *timer)
{
	struct eswap_key_point_record *record = from_timer(record, timer, lat_monitor);
	if (!record->warning_threshold)
		return;

	dump_lat(record, ktime_get(), false);

	if (likely(record->task))
		eswap_show_stack(record->task, NULL);
	record->warning_threshold <<= 2;
	record->timeout_flag = true;
	mod_timer(&record->lat_monitor, jiffies + msecs_to_jiffies(record->warning_threshold));
}

static void esacct_init_monitor(struct eswap_key_point_record *record, enum eswap_scenario scenario)
{
	record->warning_threshold = esacct_warning_threshold(scenario);

	if (!record->warning_threshold)
		return;

	record->task = current;
	get_task_struct(record->task);
	timer_setup(&record->lat_monitor, esacct_warning, 0);
	mod_timer(&record->lat_monitor, jiffies + msecs_to_jiffies(record->warning_threshold));
}

static void esacct_stop_monitor(struct eswap_key_point_record *record)
{
	if (!record->warning_threshold)
		return;

	del_timer_sync(&record->lat_monitor);
	put_task_struct(record->task);
}

static void esacct_init(struct eswap_key_point_record *record, enum eswap_scenario scenario)
{
	int i;

	for (i = 0; i < ESWAP_KYE_POINT_BUTT; ++i)
		spin_lock_init(&record->key_point[i].time_lock);

	record->nice = task_nice(current);
	record->scenario = scenario;
	get_task_comm(record->task_comm, current);
	esacct_init_monitor(record, scenario);
}

static void esacct_start_proc(struct eswap_key_point_record *record,
	enum eswap_key_point type, ktime_t curr_time)
{
	struct eswap_key_point_info *key_point = &record->key_point[type];

	if (!key_point->record_cnt)
		key_point->first_time = curr_time;

	key_point->record_cnt++;
	key_point->last_time = curr_time;
}

static void esacct_end_proc(struct eswap_key_point_record *record,
	enum eswap_key_point type, ktime_t curr_time)
{
	struct eswap_key_point_info *key_point = &record->key_point[type];
	s64 diff_time = ktime_us_delta(curr_time, key_point->last_time);

	key_point->proc_total_time += diff_time;
	if (diff_time > key_point->proc_max_time)
		key_point->proc_max_time = diff_time;

	key_point->end_cnt++;
	key_point->last_time = curr_time;
}

void esacct_async_perf(struct eswap_key_point_record *record,
	enum eswap_key_point type, ktime_t start)
{
	unsigned long flags;

	spin_lock_irqsave(&record->key_point[type].time_lock, flags);
	esacct_start_proc(record, type, start);
	esacct_end_proc(record, type, ktime_get());
	spin_unlock_irqrestore(&record->key_point[type].time_lock, flags);
}

void esacct_lat_point(struct eswap_key_point_record *record, enum eswap_key_point type)
{
	esacct_start_proc(record, type, ktime_get());
	record->key_point[type].end_cnt++;
}

void esacct_start(struct eswap_key_point_record *record, ktime_t start, enum eswap_scenario scenario)
{
	esacct_init(record, scenario);
	esacct_start_proc(record, ESWAP_START, start);
	record->key_point[ESWAP_START].end_cnt++;
}

/* reclaim_in: 2s, fault_out: 100ms, batch_out: 500ms, pre_out: 2s */
static s64 timeout_value[ESWAP_SCENARIO_NUM] = {
	2000000,
	100000,
	500000,
	2000000
};

static void esacct_lat_stat(struct eswap_key_point_record *record)
{
	struct eswap_stat *stat = eswap_get_stat_obj();
	s64 curr_lat;

	if (!stat || (record->scenario >= ESWAP_SCENARIO_NUM))
		return;

	curr_lat = ktime_us_delta(record->key_point[ESWAP_DONE].first_time,
				record->key_point[ESWAP_START].first_time);
	atomic64_add(curr_lat, &stat->lat[record->scenario].total_lat);

	if (curr_lat > atomic64_read(&stat->lat[record->scenario].max_lat))
		atomic64_set(&stat->lat[record->scenario].max_lat, curr_lat);
	if (curr_lat > timeout_value[record->scenario])
		atomic64_inc(&stat->lat[record->scenario].timeout_cnt);
}

void esacct_end(struct eswap_key_point_record *record)
{
	int loglevel;

	esacct_stop_monitor(record);
	esacct_lat_point(record, ESWAP_DONE);
	esacct_lat_stat(record);

	loglevel = record->timeout_flag ? ESD_ERR : ESD_DEBUG;
	if (loglevel > eswap_loglevel())
		return;

	dump_lat(record, record->key_point[ESWAP_DONE].first_time, true);
}

void esacct_lat_start(struct eswap_key_point_record *record, enum eswap_key_point type)
{
	esacct_start_proc(record, type, ktime_get());
}

void esacct_lat_end(struct eswap_key_point_record *record, enum eswap_key_point type)
{
	esacct_end_proc(record, type, ktime_get());
}

void esacct_io_stat(struct eswap_key_point_record *record, int page_cnt, int segment_cnt)
{
	record->page_cnt = page_cnt;
	record->segment_cnt = segment_cnt;
}

