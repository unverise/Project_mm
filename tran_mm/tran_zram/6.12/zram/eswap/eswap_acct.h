/*
 */
#ifndef __ESWAP_PERF_H__
#define __ESWAP_PERF_H__
#include "eswap_internal.h"

enum eswap_key_point {
	ESWAP_START = 0,
	ESWAP_INIT,
	ESWAP_IOENTRY_ALLOC,
	ESWAP_FIND_EXTENT,
	ESWAP_IO_EXTENT,
	ESWAP_SEGMENT_ALLOC,
	ESWAP_BIO_ALLOC,
	ESWAP_SUBMIT_BIO,
	ESWAP_END_IO,
	ESWAP_SCHED_WORK,
	ESWAP_END_WORK,
	ESWAP_CALL_BACK,
	ESWAP_WAKE_UP,
	ESWAP_ZRAM_LOCK,
	ESWAP_DONE,
	ESWAP_KYE_POINT_BUTT
};

struct eswap_key_point_info {
	unsigned int record_cnt;
	unsigned int end_cnt;
	ktime_t first_time;
	ktime_t last_time;
	s64 proc_total_time;
	s64 proc_max_time;
	spinlock_t time_lock;
};

struct eswap_key_point_record {
	struct timer_list lat_monitor;
	unsigned long warning_threshold;
	int page_cnt;
	int segment_cnt;
	int nice;
	bool timeout_flag;
	unsigned char task_comm[TASK_COMM_LEN];
	struct task_struct *task;
	enum eswap_scenario scenario;
	struct eswap_key_point_info key_point[ESWAP_KYE_POINT_BUTT];
};

extern void esacct_start(struct eswap_key_point_record *, ktime_t start, enum eswap_scenario);
extern void esacct_end(struct eswap_key_point_record *record);
extern void esacct_lat_start(struct eswap_key_point_record *, enum eswap_key_point);
extern void esacct_lat_end(struct eswap_key_point_record *, enum eswap_key_point);
extern void esacct_lat_point(struct eswap_key_point_record *, enum eswap_key_point);
extern void esacct_async_perf(struct eswap_key_point_record *, enum eswap_key_point, ktime_t);
extern void esacct_io_stat(struct eswap_key_point_record *, int page_cnt, int segment_cnt);

#endif // __ESWAP_PERF_H__
