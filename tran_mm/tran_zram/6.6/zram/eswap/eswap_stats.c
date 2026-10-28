#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/atomic.h>
#include <linux/zsmalloc.h>
#include <linux/memcontrol.h>

#include "eswap_internal.h"
#include "memcg_estrack.h"
#include "eswap_area.h"

#define SCENARIO_NAME_LEN 32
#define MBYTE_SHIFT 20

#define BYTES_TO_MB(v)	((v) >> MBYTE_SHIFT)
#define PAGES_TO_MB(v)	((v) >> (MBYTE_SHIFT - PAGE_SHIFT))

static char scenario_name[ESWAP_SCENARIO_NUM][SCENARIO_NAME_LEN] = {
	"eswap_out",
	"eswap_in_fault",
	"eswap_in_batch",
	"eswap_in_preload"
};

static void lat_show(struct seq_file *m, struct eswap_stat *stat)
{
	int i;
	for (i = 0; i < ESWAP_SCENARIO_NUM; ++i) {
		seq_printf(m, "%s_total_lat:%lld\n", scenario_name[i],
			atomic64_read(&stat->lat[i].total_lat));
		seq_printf(m, "%s_max_lat:%lld\n", scenario_name[i],
			atomic64_read(&stat->lat[i].max_lat));
		seq_printf(m, "%s_timeout_cnt:%lld\n", scenario_name[i],
			atomic64_read(&stat->lat[i].timeout_cnt));
	}
}

static void stats_show(struct seq_file *m, struct eswap_stat *stat)
{
	seq_printf(m, "eswap_enable:              %d\n", eswap_enable());
	seq_printf(m, "eswapout_enable:           %d\n", eswap_swapout_enable());

	seq_printf(m, "eswap_out_times:           %lld\n", atomic64_read(&stat->eswapout_cnt));
	seq_printf(m, "eswap_out_comp_size:       %lld MB\n", BYTES_TO_MB(atomic64_read(&stat->eswapout_bytes)));
	seq_printf(m, "eswap_out_orig_size:       %lld MB\n", PAGES_TO_MB(atomic64_read(&stat->eswapout_pages)));

	seq_printf(m, "eswap_in_times:            %lld\n", atomic64_read(&stat->eswapin_cnt));
	seq_printf(m, "eswap_in_comp_size:        %lld MB\n", BYTES_TO_MB(atomic64_read(&stat->eswapin_bytes)));
	seq_printf(m, "eswap_in_orig_size:        %lld MB\n", PAGES_TO_MB(atomic64_read(&stat->eswapin_pages)));

	seq_printf(m, "eswap_faultin_cnt_1:       %lld\n", atomic64_read(&stat->eswap_faultin_cnt_1));
	seq_printf(m, "eswap_faultin_cnt_2:       %lld\n", atomic64_read(&stat->eswap_faultin_cnt_2));
	seq_printf(m, "eswap_faultin_hits :       %lld\n", atomic64_read(&stat->eswap_fault_in_hits));

	seq_printf(m, "eswap_refault_comp_size:   %lld MB\n", BYTES_TO_MB(atomic64_read(&stat->refault_bytes)));
	seq_printf(m, "eswap_refault_orig_size:   %lld MB\n", PAGES_TO_MB(atomic64_read(&stat->refault_pages)));

	seq_printf(m, "eswap_zram_comp_size:      %lld MB\n", BYTES_TO_MB(atomic64_read(&stat->zram_stored_bytes)));
	seq_printf(m, "eswap_zram_orig_size:      %lld MB\n", PAGES_TO_MB(atomic64_read(&stat->zram_stored_pages)));

	seq_printf(m, "eswap_store_comp_size:     %lld MB\n", BYTES_TO_MB(atomic64_read(&stat->eswap_stored_bytes)));
	seq_printf(m, "eswap_store_orig_size:     %lld MB\n", PAGES_TO_MB(atomic64_read(&stat->eswap_stored_pages)));

	seq_printf(m, "eswap_bd_reads:            %lld\n", atomic64_read(&stat->eswap_bd_reads));
	seq_printf(m, "eswap_bd_writes:           %lld\n", atomic64_read(&stat->eswap_bd_writes));
	seq_printf(m, "eswap_bd_read_delay:       %lld\n", atomic64_read(&stat->eswap_bd_read_delay));
	seq_printf(m, "eswap_bd_write_delay:      %lld\n", atomic64_read(&stat->eswap_bd_write_delay));
	seq_printf(m, "eswap_bd_batch_reads:      %lld\n", atomic64_read(&stat->eswap_bd_batch_reads));
	seq_printf(m, "eswap_bd_read_max_delay:   %lld\n", atomic64_read(&stat->eswap_bd_read_max_delay));

	seq_printf(m, "eswap_store_memcg_cnt:     %lld\n", atomic64_read(&stat->memcg_cnt));
	seq_printf(m, "eswap_store_extent_cnt:    %lld\n", atomic64_read(&stat->ext_cnt));
	seq_printf(m, "eswap_memcg_deinit_cnt:    %lld\n", atomic64_read(&stat->memcg_deinit_cnt));

	seq_printf(m, "eswap_miss_free:           %lld\n", atomic64_read(&stat->miss_free));
	seq_printf(m, "eswap_untrack_underwb:     %lld\n", atomic64_read(&stat->under_wb_untrack));
	seq_printf(m, "eswap_override:            %lld\n", atomic64_read(&stat->override));
	seq_printf(m, "eswap_notify_free_size:    %lld MB\n", atomic64_read(&stat->notify_free) >> (MBYTE_SHIFT - EXTENT_SHIFT));

	seq_printf(m, "eswap_faulting_1:          %lld\n", atomic64_read(&stat->faulting_1));
	seq_printf(m, "eswap_faulting_2:          %lld\n", atomic64_read(&stat->faulting_2));
	seq_printf(m, "eswap_faulting_3:          %lld\n", atomic64_read(&stat->faulting_3));
	seq_printf(m, "fltin_extent_busy:         %lld\n", atomic64_read(&stat->fltin_extent_busy));

	seq_printf(m, "all_exts_num:              %d\n", eswap_get_exts_num());
	seq_printf(m, "used_exts_num:             %d\n", atomic_read(&stat->used_exts_num));
	seq_printf(m, "max_used_exts_num:         %d\n", atomic_read(&stat->max_used_exts_num));
}

static void fail_show(struct seq_file *m, struct eswap_stat *stat)
{
	int i;

	for (i = 0; i < ESWAP_SCENARIO_NUM; ++i) {
		seq_printf(m, "eswap_%s_io_fail_cnt:%lld\n", scenario_name[i],
					atomic64_read(&stat->io_fail_cnt[i]));
		seq_printf(m, "eswap_%s_alloc_fail_cnt:%lld\n", scenario_name[i],
					atomic64_read(&stat->alloc_fail_cnt[i]));
	}
}

void eswap_stat_show(struct seq_file *m)
{
	struct eswap_stat *stat = NULL;

	if (!eswap_enable())
		return;

	stat = eswap_get_stat_obj();
	stats_show(m, stat);
	lat_show(m, stat);
	fail_show(m, stat);
}

void eswap_area_stat_show(struct seq_file *m)
{
	struct eswap_area *area = NULL;
	struct eswap_stat *stat = NULL;

	if (!eswap_enable())
		return;

	stat = eswap_get_stat_obj();
	area = eswap_get_global_area();
	if (area != NULL) {
		int extents = 0;
		int memcgs = 0;
		int mcgid = 0;
		int i = 0;

		// seq_printf(m, "eswap_stored_exts:         %d\n", atomic_read(&area->stored_exts));
		seq_printf(m, "eswap_store_memcg_cnt:     %lld\n", atomic64_read(&stat->memcg_cnt));

		for (mcgid = 0; mcgid < area->nr_mcgs; mcgid++) {
			int midx, eidx;
			int eidxs = 0;
			int cnt = 0;
			int exts_for_memcg = 0;
			if (!area->mcg_id_cnt[mcgid])
				continue;

			exts_for_memcg = (int)area->mcg_id_cnt[mcgid];
			extents += exts_for_memcg;
			memcgs++;

			midx = mcg_idx(area, mcgid);

			hh_lock_list(midx, area->ext_table);
			seq_printf(m, "\t\textid ");
			hh_list_for_each_entry(eidx, midx, area->ext_table) {
				int extid = eidx - area->nr_objs - area->nr_mcgs;
				seq_printf(m, "%d  ", extid);
				eidxs++;
				cnt++;
				if (cnt == 20) {
					cnt = 0;
					seq_printf(m, "\n\t\t      ");
				}
			}
			seq_printf(m, "\n");
			hh_unlock_list(midx, area->ext_table);

			seq_printf(m, "\teswap_memcg %d, extents %u, eidxs %d\n",
				mcgid, exts_for_memcg, eidxs);
		}
		seq_printf(m, "total eswap_memcg %d, total extents %d\n", memcgs, extents);


		for (i = 0; i < MAX_EXTRA_AREAS; i++) {
			struct eswap_extra_area *earea = area->extra_areas[i];
			if (!earea)
				continue;

			seq_printf(m, "earea_%d nr_exts %d, area_id_offset %d, stored_exts %d,"
					"bd_count %lu, bd_reads %lu, bd_writes %lu\n",
				i,
				earea->nr_exts,
				earea->area_id_offset,
				atomic_read(&earea->stored_exts),
				(unsigned long)atomic64_read(&earea->bd_count),
				(unsigned long)atomic64_read(&earea->bd_reads),
				(unsigned long)atomic64_read(&earea->bd_writes));
		}
	}
}

unsigned long eswap_get_zram_used_pages(void)
{
	struct eswap_stat *stat = NULL;

	if (!eswap_enable())
		return 0;

	stat = eswap_get_stat_obj();
	return atomic64_read(&stat->zram_stored_pages);
}

unsigned long eswap_get_eswap_used_pages(void)
{
	struct eswap_stat *stat = NULL;

	if (!eswap_enable())
		return 0;

	stat = eswap_get_stat_obj();
	return atomic64_read(&stat->eswap_stored_pages);
}

unsigned long long eswap_get_zram_pagefault(void)
{
	struct eswap_stat *stat = NULL;

	if (!eswap_enable())
		return 0;

	stat = eswap_get_stat_obj();
	return atomic64_read(&stat->eswap_faultin_cnt_1);
}

bool eswap_reclaim_work_running(void)
{
	struct eswap_stat *stat = NULL;
	unsigned long curr_interval;

	if (!eswap_enable())
		return false;

	stat = eswap_get_stat_obj();

	/* rate limit */
	curr_interval = jiffies_to_msecs(jiffies - stat->last_time);
	if (curr_interval < 10000)
		return true;

	return atomic64_read(&stat->eswapout_inflight) ? true : false;
}

void eswap_reclaim_work_done(void)
{
	struct eswap_stat *stat = eswap_get_stat_obj();
	stat->last_time = jiffies;
}

void eswap_record_fail(enum eswap_fail_point point,
	u32 index, int ext_id, unsigned char *task_comm)
{
	struct eswap_stat *stat = NULL;
	unsigned long flags;
	unsigned int copylen = strlen(task_comm) + 1;

	stat = eswap_get_stat_obj();
	if (unlikely(!stat)) {
		eswap_debug(ESD_ERR, "can't get stat obj!\n");
		return;
	}

	if (copylen > TASK_COMM_LEN) {
		eswap_debug(ESD_ERR, "task_comm len %u is err\n", copylen);
		return;
	}

	spin_lock_irqsave(&stat->record.lock, flags);
	if (stat->record.num < MAX_FAIL_RECORD_NUM) {
		stat->record.record[stat->record.num].point = point;
		stat->record.record[stat->record.num].index = index;
		stat->record.record[stat->record.num].ext_id = ext_id;
		stat->record.record[stat->record.num].time = ktime_get();
		memcpy(stat->record.record[stat->record.num].task_comm,
			task_comm, copylen);
		stat->record.num++;
	}
	spin_unlock_irqrestore(&stat->record.lock, flags);
}

static void get_fail_record(struct eswap_fail_record_info *record_info)
{
	struct eswap_stat *stat = NULL;
	unsigned long flags;

	if (!eswap_enable())
		return;

	stat = eswap_get_stat_obj();

	spin_lock_irqsave(&stat->record.lock, flags);
	memcpy(record_info, &stat->record,
		sizeof(struct eswap_fail_record_info));
	stat->record.num = 0;
	spin_unlock_irqrestore(&stat->record.lock, flags);
}

static ssize_t fail_record_show(char *buf)
{
	int i;
	ssize_t size = 0;
	struct eswap_fail_record_info record_info = { 0 };

	get_fail_record(&record_info);

	size += scnprintf(buf + size, PAGE_SIZE,
			"eswap_fail_record_num: %d\n", record_info.num);
	for (i = 0; i < record_info.num; ++i)
		size += scnprintf(buf + size, PAGE_SIZE - size,
			"point[%u]time[%lld]taskname[%s]index[%u]ext_id[%d]\n",
			record_info.record[i].point,
			ktime_us_delta(ktime_get(),
				record_info.record[i].time),
			record_info.record[i].task_comm,
			record_info.record[i].index,
			record_info.record[i].ext_id);

	return size;
}

ssize_t eswap_report_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return fail_record_show(buf);
}

