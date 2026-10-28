#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/vmpressure.h>
#include <linux/vmstat.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/writeback.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/mm_inline.h>
#include <linux/backing-dev.h>
#include <linux/rmap.h>
#include <linux/topology.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/compaction.h>
#include <linux/notifier.h>
#include <linux/rwsem.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/memcontrol.h>
#include <linux/delayacct.h>
#include <linux/sysctl.h>
#include <linux/oom.h>
#include <linux/prefetch.h>
#include <linux/printk.h>
#include <linux/dax.h>
#include <linux/psi.h>
#include <linux/sched/loadavg.h>
#include <linux/version.h>
#include <linux/sizes.h>
#include <linux/mmzone.h>

#include <asm/tlbflush.h>
#include <asm/div64.h>
#include <linux/atomic.h>
#include <linux/swapops.h>
#include <linux/balloon_compaction.h>
#include <trace/events/vmscan.h>
#include <uapi/linux/sched/types.h>

#include <trace/hooks/vmscan.h>
#include <trace/hooks/mm.h>
#include <linux/ratelimit.h>
#include "memcg_control.h"
#include "../zram_drv.h"

#ifdef CONFIG_ESWAP
#include "eswap_api.h"
#endif

// #define ESWAP_ZSWAPD_DROPCACHE_SUPPORT 1

enum zswapd_event_item {
	ZSWAPD_WAKEUP,
	ZSWAPD_WAKEUP_WORK,
	ZSWAPD_EMPTY_ROUND_SKIP_TIMES,
	ZSWAPD_EMPTY_ROUND,
	ZSWAPD_SUITABLE_SKIP,
	ZSWAPD_DROP_CACHE,
	ZSWAPD_REFAULT,
	ZSWAPD_SHRINK_ANON,
	ZSWAPD_RECLAIMED,
	ZSWAPD_ESWAP_OUT,
	ZSWAPD_MEDIUM_PRESS,
	ZSWAPD_CRITICAL_PRESS,
	ZSWAPD_MEMCG_REFAULT_SKIP,
	ZSWAPD_MEMCG_RATIO_SKIP,
	ZSWAPD_SNAPSHOT_TIMES,
	NR_ZSWAPD_EVENT_ITEMS,
};

static const char *zswapd_event_name[NR_ZSWAPD_EVENT_ITEMS] = {
	"memfusiond_wakeup",
	"memfusiond_wakeup_work",
	"memfusiond_empty_round_skip",
	"memfusiond_empty_round",
	"memfusiond_suitable_skip",
	"memfusiond_drop_cache",
	"memfusiond_refault",
	"memfusiond_shrink_anon",
	"memfusiond_reclaimed",
	"memfusiond_writeback",
	"memfusiond_medium_press",
	"memfusiond_critical_press",
	"memfusiond_memcg_refault_skip",
	"memfusiond_memcg_ratio_skip",
	"memfusiond_snapshot_times",
};

struct zswapd_event_state {
	unsigned long event[NR_ZSWAPD_EVENT_ITEMS];
};

static DEFINE_PER_CPU(struct zswapd_event_state, zswapd_event_states) = {{0}};

static inline void count_zswapd_event(enum zswapd_event_item item)
{
	this_cpu_inc(zswapd_event_states.event[item]);
}

static inline void count_zswapd_events(enum zswapd_event_item item, long delta)
{
	this_cpu_add(zswapd_event_states.event[item], delta);
}

static inline void sum_zswapd_events(unsigned long *ret)
{
	int cpu;
	int i;

	for_each_online_cpu(cpu) {
		struct zswapd_event_state *this = &per_cpu(zswapd_event_states, cpu);

		for (i = 0; i < NR_ZSWAPD_EVENT_ITEMS; i++)
			ret[i] += this->event[i];
	}
}

static inline void all_zswapd_events(unsigned long *ret)
{
	cpus_read_lock();
	sum_zswapd_events(ret);
	cpus_read_unlock();
}

int zswapd_events_show(struct seq_file *m, void *v)
{
	struct zswapd_event_state *event_buf = NULL;
	int i;

	event_buf = (struct zswapd_event_state *)kzalloc(
			sizeof(struct zswapd_event_state), GFP_KERNEL);
	if (!event_buf)
		return -ENOMEM;

	all_zswapd_events(event_buf->event);
	for (i = 0; i < NR_ZSWAPD_EVENT_ITEMS; i++) {
		seq_printf(m, "%s %lu\n", zswapd_event_name[i], event_buf->event[i]);
	}

	kfree(event_buf);
	return 0;
}

struct zswap_struct {
	wait_queue_head_t wqh;
	struct task_struct *zswapd;
	pg_data_t *pgdat;
	unsigned long last_time;
	unsigned long skip_interval_ms;
	pid_t pid;
	bool last_round_is_empty;
	atomic_t event;
	int event_async;
};

#define PGDAT_ZSWAP(pgdat) ((struct zswap_struct *)(pgdat->android_oem_data1))
#define PGDAT_ZSWAPD_FIELD(pgdat, field) (PGDAT_ZSWAP(pgdat)->field)	

#define INIT_PGDAT_ZSWAP(pgdat, pzs) do {			\
	pgdat->android_oem_data1 = (u64)(pzs);			\
} while (0)

#ifdef ESWAP_ZSWAPD_DROPCACHE_SUPPORT
static const char *fs_type_names[] = {
	"f2fs",
	"ext4",
	"ext3",
	"erofs",
};

#define FS_NUM (sizeof(fs_type_names) / sizeof(fs_type_names[0]))

static struct file_system_type *fs_types[FS_NUM] = {0};

static void get_fs_types(void)
{
	int i;
	for (i = 0; i < FS_NUM; i++) {
		fs_types[i] = get_fs_type(fs_type_names[i]);
	}
	return;
}

static void put_fs_types(void)
{
	int i;
	for (i = 0; i < FS_NUM; i++) {
		struct file_system_type *fs = fs_types[i];
		if (fs) {
			module_put(fs->owner);
		}
	}
	return;
}

static void iterate_supers_specific(
	void (*f)(struct super_block *, void *), void *arg)
{
	int i;
	for (i = 0; i < FS_NUM; i++) {
		struct file_system_type *fs = fs_types[i];
		if (fs) {
			iterate_supers_type(fs, f, arg);
		}
	}
}

static void do_drop_cache(struct super_block *sb, void *unused)
{
	struct inode *inode, *toput_inode = NULL;
	unsigned long nr_pages = 0;

	spin_lock(&sb->s_inode_list_lock);
	list_for_each_entry(inode, &sb->s_inodes, i_sb_list) {
		spin_lock(&inode->i_lock);
		/*
		 * We must skip inodes in unusual state. We may also skip
		 * inodes without pages but we deliberately won't in case
		 * we need to reschedule to avoid softlockups.
		 */
		if ((inode->i_state & (I_FREEING|I_WILL_FREE|I_NEW)) ||
		    (inode->i_mapping->nrpages == 0 && !need_resched())) {
			spin_unlock(&inode->i_lock);
			continue;
		}

		// __iget(inode);
		atomic_inc(&inode->i_count);

		spin_unlock(&inode->i_lock);
		spin_unlock(&sb->s_inode_list_lock);

		nr_pages = invalidate_mapping_pages(inode->i_mapping, 0, -1);
		iput(toput_inode);
		toput_inode = inode;

		cond_resched();
		spin_lock(&sb->s_inode_list_lock);
	}
	spin_unlock(&sb->s_inode_list_lock);
	iput(toput_inode);

	ZSWAPD_DEBUG(ZSD_WARN, "drop %lu pages\n", nr_pages);
}

static void drop_caches(void)
{
	iterate_supers_specific(do_drop_cache, NULL);
}

#else
static inline void get_fs_types(void)
{
}

static inline void put_fs_types(void)
{
}

static inline void drop_caches(void)
{
}
#endif // ESWAP_ZSWAPD_DROPCACHE_SUPPORT
 
//unsigned long long global_anon_refault_ratio;

/*
 * zswapd memcg reclaimation support
 */
static DEFINE_SPINLOCK(mgasync_list_lock);
static LIST_HEAD(mgasync_list);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
enum scan_balance {
	SCAN_EQUAL,
	SCAN_FRACT,
	SCAN_ANON,
	SCAN_FILE,
	SCAN_NONE,
};

#define ZSWAPD_RECLAIM_TUNE_NONE 0
#define ZSWAPD_RECLAIM_TUNE_ANON 1
#define ZSWAPD_RECLAIM_TUNE_FILE 2

static int zswapd_reclaim_tune = ZSWAPD_RECLAIM_TUNE_NONE;
static int zswapd_swappiness = -1;

// only one user-spcae helper is allowed to do the reclaim at ont time
static int helper_reclaim_tune = ZSWAPD_RECLAIM_TUNE_NONE;
static int helper_swappiness = -1;
static struct task_struct *helper_task = NULL;
static DEFINE_MUTEX(helper_lock);

// infact there is only one node - one zswapd, the following code built
// beyond this simple fact, it will be much more complicated if you really
// want to handle the multi-node multi-zswapd case ...
static int current_is_zswapd(void)
{
	pg_data_t *pgdat = NULL;
	struct zswap_struct *zswap = NULL;
	int nid;

	for_each_online_node(nid) {
		pgdat = NODE_DATA(nid);
		zswap = PGDAT_ZSWAP(pgdat);

		if (zswap && current == zswap->zswapd) {
			return 1;
		}
	}

	return 0;
}

static inline int current_is_zswapd_helper(void)
{
	return (current == helper_task);
}

static void zswapd_tune_scan_type_hook(void *data, enum scan_balance *scan)
{
	if (current_is_zswapd()) {
		if (zswapd_reclaim_tune == ZSWAPD_RECLAIM_TUNE_ANON) {
			*scan = SCAN_ANON;
			return;
		} else if (zswapd_reclaim_tune == ZSWAPD_RECLAIM_TUNE_FILE) {
			*scan = SCAN_FILE;
			return;
		}
		return;
	}

	if (current_is_zswapd_helper()) {
		if (helper_reclaim_tune == ZSWAPD_RECLAIM_TUNE_ANON) {
			*scan = SCAN_ANON;
			return;
		} else if (helper_reclaim_tune == ZSWAPD_RECLAIM_TUNE_FILE) {
			*scan = SCAN_FILE;
			return;
		}
		return;
	}

	return;
}

static void zswapd_tune_swappiness_hook(void *data, int *swappiness)
{
	if (current_is_zswapd() && zswapd_swappiness != -1) {
		*swappiness = zswapd_swappiness;
		return;
	}

	if (current_is_zswapd_helper() && helper_swappiness != -1) {
		*swappiness = helper_swappiness;
		return;
	}
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
unsigned long memcg_pages_local(struct mem_cgroup *memcg, enum node_stat_item item)
{
	unsigned long size = 0;
	pg_data_t *pgdat = NULL;
	struct lruvec *lruvec = NULL;
	int nid;

	for_each_online_node(nid) {
		pgdat = NODE_DATA(nid);
		lruvec = mem_cgroup_lruvec(memcg, pgdat);
		size += lruvec_page_state_local(lruvec, item);
	}
	return size;
}
#else
static unsigned long lruvec_lru_size(struct lruvec *lruvec, enum lru_list lru,
				     int zone_idx)
{
	unsigned long size = 0;
	int zid;

	for (zid = 0; zid <= zone_idx; zid++) {
		struct zone *zone = &lruvec_pgdat(lruvec)->node_zones[zid];

		if (!managed_zone(zone))
			continue;

		if (!mem_cgroup_disabled())
			size += mem_cgroup_get_zone_lru_size(lruvec, lru, zid);
		else
			size += zone_page_state(zone, NR_ZONE_LRU_BASE + lru);
	}
	return size;
}

unsigned long memcg_pages_local(struct mem_cgroup *memcg, enum node_stat_item item)
{
	enum lru_list lru;
	unsigned long pages = 0;
	int nid;

	switch (item) {
	case NR_INACTIVE_ANON:
		lru = LRU_INACTIVE_ANON;
		break;
	case NR_ACTIVE_ANON:
		lru = LRU_ACTIVE_ANON;
		break;
	case NR_INACTIVE_FILE:
		lru = LRU_INACTIVE_FILE;
		break;
	case NR_ACTIVE_FILE:
		lru = LRU_ACTIVE_FILE;
		break;
	default:
		return 0;
	}

	for_each_online_node(nid) {
		int zone_idx;
		pg_data_t *pgdat = NODE_DATA(nid);
		struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);

		for (zone_idx = 0; zone_idx < MAX_NR_ZONES; zone_idx++)
			pages += lruvec_lru_size(lruvec, lru, zone_idx);
	}

	return pages;
}
#endif

unsigned long memcg_pages(struct mem_cgroup *memcg, enum node_stat_item item)
{
	unsigned long size = 0;
	pg_data_t *pgdat = NULL;
	struct lruvec *lruvec = NULL;
	int nid;

	for_each_online_node(nid) {
		pgdat = NODE_DATA(nid);
		lruvec = mem_cgroup_lruvec(memcg, pgdat);
		size += lruvec_page_state(lruvec, item);
	}
	return size;
}

pid_t get_zswapd_pid(int nid)
{
	pg_data_t *pgdat = NULL;
	struct zswap_struct *zswap = NULL;
	pid_t pid = -1;

	pgdat = NODE_DATA(nid);
	zswap = PGDAT_ZSWAP(pgdat);

	if (zswap) {
		pid = zswap->pid;
	}

	return pid;
}

static void wakeup_zswapd_for_async(pg_data_t *pgdat)
{
	struct zswap_struct *zswap = PGDAT_ZSWAP(pgdat);
	if (!zswap)
		return;

	zswap->event_async = 1;
	wake_up_interruptible(&zswap->wqh);
}

static void wake_all_zswapds_for_async(void)
{
	pg_data_t *pgdat = NULL;
	int nid;

	for_each_online_node(nid) {
		pgdat = NODE_DATA(nid);
		wakeup_zswapd_for_async(pgdat);
	}
}

struct mgasync *dequeue_mgasync(void)
{
	struct list_head *head = &mgasync_list;
	struct spinlock *lock = &mgasync_list_lock;
	struct mgasync *mga = NULL;

	spin_lock(lock);
	if (!list_empty(head)) {
		mga = list_first_entry(head, struct mgasync, node);
		list_del_init(&mga->node);
	}
	spin_unlock(lock);

	return mga;
}

void enqueue_mgasync(struct mgasync *mga)
{
	struct list_head *head = &mgasync_list;
	struct spinlock *lock = &mgasync_list_lock;

	spin_lock(lock);
	list_add_tail(&mga->node, head);
	spin_unlock(lock);

	wake_all_zswapds_for_async();
}

void abort_memcg_mgasync(struct mem_cgroup *memcg)
{
	struct list_head *head = &mgasync_list;
	struct spinlock *lock = &mgasync_list_lock;
	struct mgasync *pos, *n;
	LIST_HEAD(moved);

	/* isolate from queue */
	spin_lock(lock);
	list_for_each_entry_safe(pos, n, head, node) {
		if (pos->memcg == memcg) {
			list_move(&pos->node, &moved);
		}
	}
	spin_unlock(lock);

	/* garbage collect */
	list_for_each_entry_safe(pos, n, &moved, node) {
		struct mem_cgroup *__memcg = pos->memcg;
		list_del_init(&pos->node);

		css_put(&__memcg->css);
		kfree(pos);
	}
}

struct mgasync *alloc_init_mgasync_reclaim(struct mem_cgroup *memcg, unsigned long nr_to_reclaim,
	unsigned long step, int swappiness, int max_time, int local)
{
	struct mgasync *mga;

	if (!css_tryget(&memcg->css)) {
		return NULL;
	}

	mga = kzalloc(sizeof(*mga), GFP_KERNEL);
	if (mga == NULL) {
		css_put(&memcg->css);
		return NULL;
	}

	mga->memcg = memcg;
	INIT_LIST_HEAD(&mga->node);
	mga->nr_to_reclaim = nr_to_reclaim;
	mga->step = step;
	mga->swappiness = swappiness;
	mga->max_time= max_time;
	mga->local = local;

	return mga;
}

static bool isolate_folio(struct lruvec *lruvec, struct folio *folio)
{
	bool success;

	/* swap constrained */
	// if (!(sc->gfp_mask & __gfp_io) &&
	//    (folio_test_dirty(folio) ||
	//     (folio_test_anon(folio) && !folio_test_swapcache(folio))))
	//	return false;

	/* raced with release_pages() */
	if (!folio_try_get(folio))
		return false;

	/* raced with another isolation */
	if (!folio_test_clear_lru(folio)) {
		folio_put(folio);
		return false;
	}

	/* see the comment on lru_refs_flags */
	if (!folio_test_referenced(folio))
		set_mask_bits(&folio->flags, LRU_REFS_MASK, 0);

	/* for shrink_folio_list() */
	folio_clear_reclaim(folio);

	success = lru_gen_del_folio(lruvec, folio, true);
	VM_WARN_ON_ONCE_FOLIO(!success, folio);

	return true;
}

static unsigned long force_reclaim_lruvec(struct lruvec *lruvec,
		unsigned long nr_to_reclaimed, int type)
{
	struct lru_gen_folio *lrugen = &lruvec->lrugen;
	LIST_HEAD(list);

	int gen, zone_idx, isolated = 0;
	unsigned long gen_idx;
	unsigned long nr_scanned = 0, nr_reclaimed = 0;

	spin_lock_irq(&lruvec->lru_lock);

	for (gen_idx = READ_ONCE((lruvec)->lrugen.min_seq[type]);
	     gen_idx <= lrugen->max_seq && nr_scanned < nr_to_reclaimed;
	     gen_idx++) {
		gen = lru_gen_from_seq(gen_idx);

		for (zone_idx = MAX_NR_ZONES - 1;
		     zone_idx >= 0 && nr_scanned < nr_to_reclaimed;
		     zone_idx--) {
			LIST_HEAD(moved);
			int skipped_zone = 0;
			struct list_head *head = &lrugen->folios[gen][type][zone_idx];

			while (!list_empty(head) && nr_scanned < nr_to_reclaimed) {
				struct folio *folio = lru_to_folio(head);
				int delta = folio_nr_pages(folio);

				VM_WARN_ON_ONCE_FOLIO(folio_test_unevictable(folio), folio);
				VM_WARN_ON_ONCE_FOLIO(folio_test_active(folio), folio);
				VM_WARN_ON_ONCE_FOLIO(folio_is_file_lru(folio) != type, folio);
				VM_WARN_ON_ONCE_FOLIO(folio_zonenum(folio) != zone_idx, folio);

				nr_scanned += (unsigned long)delta;

				/*
				 * We are deactivating a folio for accelerating reclaiming.
				 * VM couldn't reclaim the folio unless we clear PG_young.
				 * As a side effect, it makes confuse idle-page tracking
				 * because they will miss recent referenced history.
				 */
				folio_clear_referenced(folio);
				folio_test_clear_young(folio);

				if (isolate_folio(lruvec, folio/*, sc*/)) {
					list_add(&folio->lru, &list);
					isolated += delta;
				} else {
					list_move(&folio->lru, &moved);
					skipped_zone += delta;
				}
			}

			if (skipped_zone)
				list_splice(&moved, head);
		}
	}

	spin_unlock_irq(&lruvec->lru_lock);

	if (isolated)
		nr_reclaimed = reclaim_pages(&list);

	ZSWAPD_DEBUG(ZSD_DEBUG, "scanned %lu, isolated %d, reclaimed %lu (%lu), "
		"gen_info %d %d %lu %lu %lu\n", nr_scanned, isolated, nr_reclaimed, nr_to_reclaimed,
		gen, zone_idx, gen_idx, READ_ONCE((lruvec)->lrugen.min_seq[type]), lrugen->max_seq);
	return nr_reclaimed;
}

unsigned long memcg_force_reclaim_anon_or_file(struct mem_cgroup *target_memcg,
	unsigned long nr_to_reclaimed, int type, int local)
{
	unsigned long nr_reclaimed = 0;
	struct mem_cgroup *memcg;
	int times = 0, empty = 1;

	while (nr_to_reclaimed) {
		empty = 1;

		/* start from local one */
		memcg = mem_cgroup_iter_internal(target_memcg, NULL, NULL);
		do {
			int nid;
			unsigned long memcg_reclaimed = 0;

			if (signal_pending(current)) {
				/* bail out */
				nr_to_reclaimed = 0;
				break;
			}

			for_each_node_state(nid, n_memory) {
				pg_data_t *pgdat = NODE_DATA(nid);
				struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
				unsigned long nr_pages = (nr_to_reclaimed < 256) ? nr_to_reclaimed : 256;
				unsigned long reclaimed = 0;

				reclaimed = force_reclaim_lruvec(lruvec, nr_pages, type);

				if (reclaimed) {
					empty = 0;
					nr_reclaimed += reclaimed;
					memcg_reclaimed += reclaimed;

					if (nr_to_reclaimed < reclaimed) {
						/* job is done, bail out */
						nr_to_reclaimed = 0;
						break;
					}
					nr_to_reclaimed -= reclaimed;
				}
			}

			ZSWAPD_DEBUG(ZSD_DEBUG, "memcg %d, times %d, reclaim %lu, remaining %lu\n",
					memcg->id.id, times, memcg_reclaimed, nr_to_reclaimed);

			/* only iterate children memcg for non local reclaim */
		} while (nr_to_reclaimed &&
			 !local &&
			 (memcg = mem_cgroup_iter_internal(target_memcg, memcg, NULL)));

		mem_cgroup_iter_break_internal(target_memcg, memcg);

		/* after we iterate all the memcg for all nodes,
		 * still nothing to reclaim, bail out
		 */
		if (empty) {
			/* bail out */
			nr_to_reclaimed = 0;
			break;
		}

		times++;
	}

	ZSWAPD_DEBUG(ZSD_DEBUG, "total reclaim %lu, empty out %d\n", nr_reclaimed, empty);
	return nr_reclaimed;
}

#if KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE
unsigned long memcg_force_reclaim(struct mem_cgroup *memcg,
	unsigned long nr_to_reclaimed, int *swappiness)
{
	unsigned int reclaim_options;

	reclaim_options = MEMCG_RECLAIM_MAY_SWAP | MEMCG_RECLAIM_PROACTIVE; 
	return try_to_free_mem_cgroup_pages(memcg, nr_to_reclaimed,
				GFP_KERNEL, reclaim_options, swappiness);

}
#else
unsigned long memcg_force_reclaim(struct mem_cgroup *memcg,
	unsigned long nr_to_reclaimed, int *swappiness)
{
	unsigned long nr_reclaimed = 0;
	unsigned int reclaim_options;
	int target_tune = ZSWAPD_RECLAIM_TUNE_NONE, target_swappiness = -1;
	int __swappiness = (swappiness != NULL) ? *swappiness : -1;

	if (__swappiness != -1) {
		target_swappiness = __swappiness;
		switch (target_swappiness) {
		case 200:
			target_tune = ZSWAPD_RECLAIM_TUNE_ANON;
			break;
		case 0:
			target_tune = ZSWAPD_RECLAIM_TUNE_FILE;
			break;
		default:
			target_tune = ZSWAPD_RECLAIM_TUNE_NONE;
			break;
		}
	}

	if (current_is_zswapd()) {
		zswapd_swappiness = target_swappiness;
		zswapd_reclaim_tune = target_tune;
	} else {
		mutex_lock(&helper_lock);
		helper_task = current;
		helper_swappiness = target_swappiness;
		helper_reclaim_tune = target_tune;
	}

	reclaim_options = MEMCG_RECLAIM_MAY_SWAP | MEMCG_RECLAIM_PROACTIVE; 

	nr_reclaimed = try_to_free_mem_cgroup_pages(memcg, nr_to_reclaimed,
				GFP_KERNEL, reclaim_options);

	if (current_is_zswapd()) {
		zswapd_swappiness = -1;
		zswapd_reclaim_tune = ZSWAPD_RECLAIM_TUNE_NONE;
	} else {
		helper_swappiness = -1;
		helper_reclaim_tune = ZSWAPD_RECLAIM_TUNE_NONE;
		helper_task = NULL;
		mutex_unlock(&helper_lock);
	}
	return nr_reclaimed;
}
#endif

static void handle_mgasync_reclaim(pg_data_t *pgdat)
{
	struct mgasync *mga = NULL;

	while ((mga = dequeue_mgasync()) != NULL) {
		struct mem_cgroup *memcg = mga->memcg;
		unsigned long nr_reclaimed = 0;
		unsigned int nr_retries = MAX_RECLAIM_RETRIES;
		unsigned long last_time = jiffies;
		unsigned long step = mga->step;
		unsigned long nr_to_reclaim = mga->nr_to_reclaim;
		int swappiness = mga->swappiness;
		int max_time = mga->max_time;
		int local = mga->local;

		while (nr_reclaimed < nr_to_reclaim) {
			/* Will converge on zero, but reclaim enforces a minimum */
			unsigned long batch_size = (nr_to_reclaim - nr_reclaimed) / 4;
			unsigned long reclaimed;
			unsigned long now;

			/*
			 * step configurable, default to 1024 pages, that is 4MB every time
			 * minimal SWAP_CLUSTER_MAX, ie. 32 pages.
			 */
			if (batch_size > (unsigned long) step)
				batch_size = (unsigned long) step;

			if (batch_size < SWAP_CLUSTER_MAX)
				batch_size = SWAP_CLUSTER_MAX; 


			if (swappiness == ANON_SWAPPINESS || swappiness == FILE_SWAPPINESS)
				reclaimed = memcg_force_reclaim_anon_or_file(memcg, batch_size,
					swappiness == ANON_SWAPPINESS ? LRU_GEN_ANON : LRU_GEN_FILE, !!local);
			else
				reclaimed = memcg_force_reclaim(memcg, batch_size, 
					swappiness == -1 ? NULL : &swappiness);

			ZSWAPD_DEBUG(ZSD_DEBUG, "time %u, retries %u, reclaimed %lu, "
				"tot_reclaimed %lu, tot_to_reclaime %lu\n",
				jiffies_to_msecs(jiffies - last_time),
				nr_retries, reclaimed, nr_reclaimed, nr_to_reclaim);

			if (!reclaimed && !--nr_retries) {
				ZSWAPD_DEBUG(ZSD_ERR, "reclaimed %lu retries %u\n", reclaimed, nr_retries);
				break;
			}

			nr_reclaimed += reclaimed;

			/* timeout, abort */
			now = jiffies;
			if (max_time && jiffies_to_msecs(now - last_time) > (unsigned int)max_time) {
				ZSWAPD_DEBUG(ZSD_ERR, "timeout %u, tot_reclaimed %lu, retries %u\n",
					jiffies_to_msecs(now - last_time), nr_reclaimed, nr_retries);
				break;
			}

			if (mem_cgroup_get_reclaim_abort(memcg)) {
				ZSWAPD_DEBUG(ZSD_ERR, "Aborted\n");
				break;
			}
		}

		/* garbage collect */
		css_put(&memcg->css);
		kfree(mga);
	}
}

#ifdef CONFIG_ESWAP
static struct snapshot_struct {
	wait_queue_head_t wqh;
	struct task_struct *task;

	atomic64_t min_interval_ms;

	unsigned long anon_rfs;
	unsigned long last_time;
	int enable;
	int event;
} snapshot;

inline void snapshot_set_min_interval_ms(u64 val)
{
	atomic64_set(&snapshot.min_interval_ms, val);
}

inline u64 snapshot_get_min_interval_ms(void)
{
	return atomic64_read(&snapshot.min_interval_ms);
}

static unsigned int curr_avail(void)
{
	unsigned long freemem;
	unsigned long active_file;
	unsigned long inactive_file;
	unsigned long misc_reclaimable;
	unsigned long avail_pages;

	freemem = global_zone_page_state(NR_FREE_PAGES);
	active_file = global_node_page_state(NR_ACTIVE_FILE);
	inactive_file = global_node_page_state(NR_INACTIVE_FILE);
	misc_reclaimable = global_node_page_state(NR_KERNEL_MISC_RECLAIMABLE);

	avail_pages = freemem +
		inactive_file * get_inactive_file_ratio_value() / 100 +
		active_file * get_active_file_ratio_value() / 100 +
		misc_reclaimable;

	return (unsigned int)(PAGES_TO_MB(avail_pages));
}

static inline bool avail_above_mmmin(void)
{
	// return (curr_avail() + 500) >= avail_min_th();
	return true;
}

static inline bool avail_above_min(void)
{
	return curr_avail() >= avail_min_th();
}

static inline bool avail_above_mid(void)
{
	return curr_avail() >= avail_mid_th();
}

static inline bool avail_above_high(void)
{
	return curr_avail() >= avail_high_th();
}

static void snapshot_anon_refaults(struct snapshot_struct *snapshot)
{
	struct mem_cgroup *memcg = NULL;
	struct memcg_estrack *est;

	while ((memcg = get_next_memcg(memcg))) {
		est = mem_cgroup_estrack(memcg);
		est->anon_rfs = (u64)atomic64_read(&est->eswap_fault_cnt);
	}

	snapshot->anon_rfs = eswap_get_zram_pagefault();
	snapshot->last_time = jiffies;
}

/*
 * Return true if refault changes between two read operations.
 */
static bool get_memcg_anon_refault_status(struct mem_cgroup *memcg)
{
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);
	unsigned long long curr_anon_rfs, anon_rfs;
	unsigned long anon_total;
	int ratio = 0;

	curr_anon_rfs = (u64)atomic64_read(&est->eswap_fault_cnt);
	anon_rfs = est->anon_rfs;

	if (curr_anon_rfs == anon_rfs)
		return false;

	anon_total = memcg_pages(memcg, NR_INACTIVE_ANON) +
			memcg_pages(memcg, NR_ACTIVE_ANON) +
			(unsigned long)atomic_read(&est->eswap_stored_pages) +
			(unsigned long)atomic_read(&est->zram_page_size);
	ratio = (curr_anon_rfs - anon_rfs) * 100 / (anon_total + 1);
	if (ratio < 0)
		ratio = 0;

	if (ratio > atomic_read(&est->anon_rf_ratio))
		return true;

	return false;
}

// pretend snapshot to be node-aware
static inline struct snapshot_struct *pgdat_snapshot(pg_data_t *p)
{
	return &snapshot;
}

static bool get_area_anon_refault_status(pg_data_t *pgdat)
{
	unsigned long long cur_anon_rfs;
	unsigned long long cur_time;
	unsigned long long ratio;

	struct snapshot_struct *snapshot = pgdat_snapshot(pgdat);

	cur_anon_rfs = eswap_get_zram_pagefault();
	cur_time = jiffies;

	if (cur_anon_rfs == snapshot->anon_rfs ||
			cur_time == snapshot->last_time) {
		return false;
	}

	ratio = (cur_anon_rfs - snapshot->anon_rfs) * 100 /
		(jiffies_to_msecs(cur_time - snapshot->last_time) + 1);

	// global_anon_refault_ratio = ratio;

	if (ratio > get_area_anon_refault_threshold_value())
		return true;

	return false;
}

static void wakeup_snapshotd(struct snapshot_struct *snapshot)
{
	unsigned long interval ;
	unsigned long min_th = atomic64_read(&snapshot->min_interval_ms);

	interval = jiffies_to_msecs(jiffies - snapshot->last_time);
	if (interval >= min_th) {
		snapshot->event = 1;
		wake_up(&snapshot->wqh);
	}
}

static int snapshotd(void *p)
{
	struct snapshot_struct *snapshot = (struct snapshot_struct *)p;

	while (!kthread_should_stop()) {
		wait_event_freezable(snapshot->wqh, (snapshot->event || kthread_should_stop()));
		snapshot->event = 0;
		snapshot_anon_refaults(snapshot);
		count_zswapd_event(ZSWAPD_SNAPSHOT_TIMES);
	}

	return 0;
}

void snapshotd_enable(int val)
{
	snapshot.enable = !!val;
}

/* This snapshotd start function will be called by init. */
static int snapshotd_run(void)
{
	snapshot.event = 0;
	snapshot.enable = 0;
	init_waitqueue_head(&snapshot.wqh);
	atomic64_set(&snapshot.min_interval_ms, SNAPSHOT_MIN_INTERVAL_MS);

	snapshot.task = kthread_run(snapshotd, &snapshot, "snapshotd");
	if (IS_ERR(snapshot.task)) {
		pr_err("Failed to start snapshotd\n");
		return PTR_ERR(snapshot.task);
	}

	return 0;
}

static void snapshotd_stop(void)
{
	kthread_stop(snapshot.task);
}

static u64 get_zram_dynamic_threshold_pages(void)
{
	/*
	 * after : before = comp_percent : 100
	 * before = after * 100 / comp_percernt
	 */
	u64 delta;
	u64 nr_total = totalram_pages();
	u64 threshold = (zswapd_get_zram_wm_percent() ? : 37);
	unsigned int target, curr;

	/* abs{ B_target - B_current } */
	target = avail_mid_th();
	curr = curr_avail();
	threshold = threshold * nr_total / 100;

	if (curr >= target) {
		return threshold;
	}

	delta = target - curr;
	delta = MB_TO_PAGES(delta);

	/* after_comp to before_comp */
	delta = (delta * 100 / zswapd_get_zram_comp_percent());

	// this case should be impossible to happen
	// guard it anyway ...
	if (threshold < delta) {
		return 0;
	}

	return (threshold - delta);
}

bool zram_watermark_ok(void)
{
	u64 threshold = get_zram_dynamic_threshold_pages();
	u64 nr_zram_used = eswap_get_zram_used_pages();

	return (nr_zram_used > threshold);
}

static bool too_many_file_cache(void)
{
	unsigned long free_pages;
	unsigned long file_pages;
	unsigned long anon_pages;
	long free_swap_pages;

	if (!zswapd_drop_caches_enabled())
		return false;

	/* skip when free > 40M */
	free_pages = global_zone_page_state(NR_FREE_PAGES);
	if (free_pages > MB_TO_PAGES(40))
		return false;

	/* skip when anon > 1200M */
	anon_pages = global_node_page_state(NR_ACTIVE_ANON) +
			global_node_page_state(NR_INACTIVE_ANON);
	if (anon_pages > MB_TO_PAGES(1200))
		return false;

	/* skip when free swap > 2G */
	free_swap_pages = get_nr_swap_pages();
	if (free_swap_pages > GB_TO_PAGES(2))
		return false;

	/* skip when file < 2G */
	file_pages = global_node_page_state(NR_ACTIVE_FILE) +
			global_node_page_state(NR_INACTIVE_FILE);
	if (file_pages < GB_TO_PAGES(2))
		return false;

	/*
	 * 1. page cache >= 2GB
	 * 2. free swap <= 2GB
	 * 3. anon pages <= 1200MB
	 * 4. free mem <= 40MB
	 */
	return true;
}

static void wakeup_zswapd(pg_data_t *pgdat, int force)
{
	unsigned long curr_interval;
	struct zswap_struct *zswap = PGDAT_ZSWAP(pgdat);

	if (!zswap)
		return;

	if (!wq_has_sleeper(&zswap->wqh))
		return;

	/* even we force wakeup zswapds, it will still
	 * be rate-limited in the zswapd thread
	 */
	if (force)
		goto force_wakeup;

	/* wake up when the buffer is lower than min_avail_buffer */
	if (avail_above_min() && !too_many_file_cache()) {
		count_zswapd_event(ZSWAPD_SUITABLE_SKIP);
		return;
	}

	/* avail-memory is less than min-th, or, file-cache is enough */

	curr_interval = jiffies_to_msecs(jiffies - zswap->last_time);
	if (curr_interval < zswap->skip_interval_ms) {
		count_zswapd_event(ZSWAPD_EMPTY_ROUND_SKIP_TIMES);
		return;
	}

force_wakeup:
	count_zswapd_event(ZSWAPD_WAKEUP);

	if (atomic_cmpxchg(&zswap->event, 0, 1) == 0) {
		/* make anon pagefault snapshots, wake up snapshotd */
		if (snapshot.enable) {
			wakeup_snapshotd(&snapshot);
		}
		wake_up_interruptible(&zswap->wqh);
	}
}

void wake_all_zswapds(void)
{
	pg_data_t *pgdat = NULL;
	int nid;

	for_each_online_node(nid) {
		pgdat = NODE_DATA(nid);

		// general wakeup
		wakeup_zswapd(pgdat, 0);
	}
}

static void force_wake_all_zswapds(void)
{
	pg_data_t *pgdat = NULL;
	int nid;

	for_each_online_node(nid) {
		pgdat = NODE_DATA(nid);

		// force wakeup
		wakeup_zswapd(pgdat, 1);
	}
}

void zswapd_page_alloc_slowpath_hook(void *data,
	gfp_t gfp_mask, unsigned int order, unsigned long alloc_start)
{
	wake_all_zswapds();
}

void zswapd_page_alloc_dr_hook(void *data, gfp_t gfp_mask, int order,
	int *alloc_flags, int migratetype, struct zone *preferred_zone,
	struct page **page, bool *should_alloc_retry )
{
	force_wake_all_zswapds();
}

void try_to_wake_all_zswapds(void)
{
	if (zram_watermark_ok())
		wake_all_zswapds();
}

static unsigned long __calc_nr_to_reclaim(void)
{
	unsigned int curr, high, max_val;
	unsigned int reclaim_size = 0;

	high = avail_high_th();
	curr = curr_avail();
	if (curr < high) {
		reclaim_size = high - curr;
	}

	max_val = zswapd_get_max_reclaim_size();
	reclaim_size = min(reclaim_size, max_val);

	return MB_TO_PAGES(reclaim_size);
}

static unsigned long zswapd_memcg_anon_reclaim_pages(struct mem_cgroup *memcg)
{
	unsigned long nr_anon_pages = 0;
	unsigned long nr_zram_pages = 0;
	unsigned long nr_eswap_pages = 0;
	unsigned long nr_total, nr_to_reclaimed;
	int zram_ratio, target_ratio;
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);

	if (est->skip_reclaim_anon)
		return 0;

	if (get_memcg_anon_refault_status(memcg)) {
		count_zswapd_event(ZSWAPD_MEMCG_REFAULT_SKIP);
		return 0;
	}

	nr_anon_pages = memcg_pages(memcg, NR_INACTIVE_ANON) +
			memcg_pages(memcg, NR_ACTIVE_ANON);
	nr_zram_pages = (unsigned long)atomic_read(&est->zram_page_size);
	nr_eswap_pages = (unsigned long)atomic_read(&est->eswap_stored_pages);
	nr_total = nr_anon_pages + nr_zram_pages + nr_eswap_pages;

	zram_ratio = (nr_zram_pages + nr_eswap_pages) * 100 / (nr_total + 1);

	target_ratio = atomic_read(&est->mem2zram_ratio);
	if (zram_ratio >= target_ratio) {
		count_zswapd_event(ZSWAPD_MEMCG_RATIO_SKIP);
		return 0;
	}

	if (target_ratio == 100) {
		return nr_anon_pages;
	}

	nr_to_reclaimed = (target_ratio - zram_ratio) * nr_total / 100;
	return nr_to_reclaimed;
}

static void zswapd_shrink_node(struct zswap_struct *zswap)
{
	unsigned long nr_reclaimed = 0, nr_to_reclaimed = 0;
	unsigned long nr_to_reclaimed_memcg = 0;
	struct mem_cgroup *memcg = NULL;

	nr_to_reclaimed = __calc_nr_to_reclaim();

	while ((memcg = get_next_memcg(memcg))) {
		if (avail_above_high() || (nr_to_reclaimed <= nr_reclaimed))
			break;

		if (try_to_freeze() || kthread_should_stop())
			break;

		/* case 1: nothing to recalim in this memcg */
		nr_to_reclaimed_memcg = zswapd_memcg_anon_reclaim_pages(memcg);
		if (!nr_to_reclaimed_memcg) {
			continue;
		}

		nr_reclaimed += memcg_force_reclaim_anon_or_file(memcg,
					nr_to_reclaimed_memcg, LRU_GEN_ANON, 0);
	}

	get_next_memcg_break(memcg);

	count_zswapd_events(ZSWAPD_RECLAIMED, nr_reclaimed);

	/*
	 * When meets the first empty round, set the interval to t.
	 * If the following round is still empty, set the interval
	 * to 2t. If the round is always empty, then 4t, 8t, and so on.
	 * But make sure the interval is not more than the max_skip_interval.
	 * Once a non-empty round occurs, reset the interval to 0.
	 */
	if (nr_reclaimed < zswapd_get_empty_round_threshold_pages()) {
		count_zswapd_event(ZSWAPD_EMPTY_ROUND);
		if (zswap->last_round_is_empty)
			zswap->skip_interval_ms = min(zswap->skip_interval_ms * 2,
						zswapd_get_max_skip_interval_ms());
		else
			zswap->skip_interval_ms = zswapd_get_skip_interval_ms();
		zswap->last_round_is_empty = true;
	} else {
		zswap->skip_interval_ms = 1000;
		zswap->last_round_is_empty = false;
	}
}

static u64 zram_watermark_diff_bytes(void)
{
	u64 threshold = get_zram_dynamic_threshold_pages();
	u64 nr_zram_used = eswap_get_zram_used_pages();

	if (nr_zram_used > threshold)
		return (nr_zram_used - threshold) * PAGE_SIZE + 50 * SZ_1M;

	return 0;
}

static u64 zswapd_avail_expected_bytes(void)
{
	unsigned int high, curr;

	high = avail_high_th();
	curr = curr_avail();
	if (curr < high)
		return (high - curr) * SZ_1M;

	return 0;
}

static u64 get_do_eswap_size(bool refault)
{
	u64 size = 0;
	u64 expected = zswapd_avail_expected_bytes();

	if (zswapd_get_zram_wm_percent() == 0) {
		size = (zram_watermark_ok() || refault) ? expected : 0;
	} else {
		size = max(zram_watermark_diff_bytes(), expected); 
	}

	return size;
}

static int zswapd(void *p)
{
	pg_data_t *pgdat = (pg_data_t *)p;
	struct zswap_struct *zswap = PGDAT_ZSWAP(pgdat);
	struct task_struct *tsk = current;
	const struct cpumask *cpumask = cpumask_of_node(pgdat->node_id);
	static DEFINE_RATELIMIT_STATE(zswapd_report, 1*HZ, 1);

	if (!cpumask_empty(cpumask))
		set_cpus_allowed_ptr(tsk, cpumask);

	set_freezable();

	while (!kthread_should_stop()) {
		bool refault = false;

		wait_event_freezable(zswap->wqh, (atomic_read(&zswap->event) ||
					zswap->event_async || kthread_should_stop()));

		/* normal event and async event are both handling mgasync */
		zswap->event_async = 0;
		handle_mgasync_reclaim(pgdat);

		/* only normal event handles global reclaiming and eswaping */
		if (atomic_cmpxchg(&zswap->event, 1, 0) == 0) {
			continue;
		}

		if (!__ratelimit(&zswapd_report))
			continue;

		count_zswapd_event(ZSWAPD_WAKEUP_WORK);
		zswapd_pressure_report(ZLEVEL_LOW);

		if (swap_is_low()) {
			zswapd_pressure_report(ZLEVEL_CRITICAL);
			count_zswapd_event(ZSWAPD_CRITICAL_PRESS);
		}

		if (too_many_file_cache()) {
			drop_caches();
			count_zswapd_event(ZSWAPD_DROP_CACHE);
			ZSWAPD_DEBUG(ZSD_ERR, "too many file cache and low free, drop cache!\n");
		}

		if (get_area_anon_refault_status(pgdat)) {
			refault = true;
			count_zswapd_event(ZSWAPD_REFAULT);
			goto do_eswap;
		}

		if (zswapd_anon_reclaim_enabled()) {
			count_zswapd_event(ZSWAPD_SHRINK_ANON);
			zswapd_shrink_node(zswap);
		}

		zswap->last_time = jiffies;

do_eswap:
		if (!eswap_reclaim_work_running() &&
			!avail_above_min() && avail_above_mmmin()) {
			u64 size = get_do_eswap_size(refault);
			if (size >= SZ_1M) {
				if (size >= 32 * SZ_1M) {
					size = 32 * SZ_1M;
				}

				count_zswapd_event(ZSWAPD_ESWAP_OUT);
				size = eswap_memcgs_out(size);
				eswap_reclaim_work_done();
			}
		}

		/* After zram-writeback, we expect the avail resume to
		 * suitable mid level. That is normal case.
		 *
		 * If not, the current workingset may be consume memoy
		 * too quick, notify user-space manager to do somthing.
		 */
		if (!avail_above_mid()) {
			zswapd_pressure_report(ZLEVEL_MEDIUM);
			count_zswapd_event(ZSWAPD_MEDIUM_PRESS);
		}
	}

	kfree(zswap);
	return 0;
}

static int register_hooks_for_eswap(void)
{
	return 0;

#if 0
	int ret;

	ret = register_trace_android_vh_alloc_pages_slowpath(zswapd_page_alloc_slowpath_hook, NULL);
	if (ret) {
		pr_err("%s: register slowpath failed!\n", __func__);
		return ret;
	}

	ret = register_trace_android_vh_should_alloc_pages_retry(zswapd_page_alloc_dr_hook, NULL);
	if (ret) {
		pr_err("%s: register dr hook failed!\n", __func__);
		unregister_trace_android_vh_alloc_pages_slowpath(zswapd_page_alloc_slowpath_hook, NULL);
		return ret;
	}

	return 0;
#endif
}

static void unregister_hooks_for_eswap(void)
{
	return;
#if 0
	unregister_trace_android_vh_should_alloc_pages_retry(zswapd_page_alloc_dr_hook, NULL);
	unregister_trace_android_vh_alloc_pages_slowpath(zswapd_page_alloc_slowpath_hook, NULL);
#endif
}
#else
static int zswapd(void *p)
{
	pg_data_t *pgdat = (pg_data_t *)p;
	struct zswap_struct *zswap = PGDAT_ZSWAP(pgdat);
	struct task_struct *tsk = current;
	const struct cpumask *cpumask = cpumask_of_node(pgdat->node_id);
	static DEFINE_RATELIMIT_STATE(zswapd_report, 1*HZ, 1);

	if (!cpumask_empty(cpumask))
		set_cpus_allowed_ptr(tsk, cpumask);

	set_freezable();

	while (!kthread_should_stop()) {
		wait_event_freezable(zswap->wqh, (atomic_read(&zswap->event) ||
					zswap->event_async || kthread_should_stop()));

		/* normal event and async event are both handling mgasync */
		zswap->event_async = 0;
		handle_mgasync_reclaim(pgdat);

		/* only normal event handles global reclaiming and eswaping */
		if (atomic_cmpxchg(&zswap->event, 1, 0) == 0) {
			continue;
		}

		if (!__ratelimit(&zswapd_report))
			continue;

		count_zswapd_event(ZSWAPD_WAKEUP_WORK);
		zswapd_pressure_report(ZLEVEL_LOW);

		if (swap_is_low()) {
			zswapd_pressure_report(ZLEVEL_CRITICAL);
			count_zswapd_event(ZSWAPD_CRITICAL_PRESS);
		}

		zswap->last_time = jiffies;

	}

	kfree(zswap);
	return 0;
}

static inline int snapshotd_run(void)
{
	return 0;
}

static inline void snapshotd_stop(void)
{
}

static inline int register_hooks_for_eswap(void)
{
	return 0;
}

static inline void unregister_hooks_for_eswap(void)
{
}
#endif

/* This zswapd start function will be called by init and node-hot-add. */
static int zswapd_run(int nid)
{
	pg_data_t *pgdat = NODE_DATA(nid);
	struct zswap_struct *pzs = NULL;
	struct sched_param param = {
		.sched_priority = 125,
	};

	if (PGDAT_ZSWAP(pgdat) != (struct zswap_struct *)0)
		return 0;

	pzs = kzalloc(sizeof(struct zswap_struct), GFP_KERNEL);
	if (pzs == NULL)
		return -EINVAL;

	init_waitqueue_head(&pzs->wqh);
	atomic_set(&pzs->event, 0);
	pzs->event_async = 0;
	pzs->skip_interval_ms = 1000;
	pzs->pgdat = pgdat;

	pzs->zswapd = kthread_create(zswapd, pgdat, "memfusiond%d", nid);

	if (IS_ERR(pzs->zswapd)) {
		int err = PTR_ERR(pzs->zswapd);
		kfree(pzs);
		
		pr_err("Failed to start zswapd on node %d, err %d\n", nid, err);
		return err;
	}

	pzs->pid = pzs->zswapd->pid;
	sched_setscheduler_nocheck(pzs->zswapd, SCHED_NORMAL, &param);
	set_user_nice(pzs->zswapd, PRIO_TO_NICE(param.sched_priority));

	INIT_PGDAT_ZSWAP(pgdat, pzs);

	/* let zswapd work after everything is fine */
	wake_up_process(pzs->zswapd);
	return 0;
}

/*
 * Called by memory hotplug when all memory in a node is offlined.  Caller must
 * hold mem_hotplug_begin/end().
 */
static void zswapd_stop(int nid)
{
	pg_data_t *pgdat = NODE_DATA(nid);
	struct zswap_struct *zswap = PGDAT_ZSWAP(pgdat); 

	if (zswap && zswap->zswapd) {
		kthread_stop(zswap->zswapd);
		zswap->zswapd = NULL;
		zswap->pid = -1;
	}
}

static int zswapds_run(void)
{
	int nid, ret;

	for_each_node_state(nid, N_MEMORY) {
		ret = zswapd_run(nid);
		if (ret) {
			goto stopall;
		}
	}

	return 0;

stopall:
	for_each_node_state(nid, N_MEMORY) {
		zswapd_stop(nid);
	}
	return ret;
}

static void zswapds_stop(void)
{
	int nid;

	for_each_node_state(nid, N_MEMORY) {
		zswapd_stop(nid);
	}
}

/* It's optimal to keep kswapds on the same CPUs as their memory, but
 * not required for correctness.  So if the last cpu in a node goes
 * away, we get changed to run anywhere: as the first one comes back,
 * restore their cpu bindings.
 */
static int zswapd_cpu_online(unsigned int cpu)
{
	int nid;

	for_each_node_state(nid, N_MEMORY) {
		pg_data_t *pgdat = NODE_DATA(nid);
		struct zswap_struct *zswap = PGDAT_ZSWAP(pgdat); 
		const struct cpumask *mask;

		mask = cpumask_of_node(pgdat->node_id);

		if (cpumask_any_and(cpu_online_mask, mask) < nr_cpu_ids) {
			/* One of our CPUs online: restore mask */
			if (zswap) {
				struct task_struct *zswapd = zswap->zswapd;
				set_cpus_allowed_ptr(zswapd, mask);
			}
		}
	}
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
static int register_swappiness_tune_hooks(void)
{
	int ret;
	ret = register_trace_android_vh_tune_scan_type(zswapd_tune_scan_type_hook, NULL);
	if (ret) {
		pr_err("%s: register tune_scan_type failed!\n", __func__);
		return ret;
	}

	ret = register_trace_android_vh_tune_swappiness(zswapd_tune_swappiness_hook, NULL);
	if (ret) {
		pr_err("%s: register tune_swappiness failed!\n", __func__);
		unregister_trace_android_vh_tune_scan_type(zswapd_tune_scan_type_hook, NULL);
		return ret;
	}

	return 0;
}

static void unregister_swappiness_tune_hooks(void)
{
	unregister_trace_android_vh_tune_swappiness(zswapd_tune_swappiness_hook, NULL);
	unregister_trace_android_vh_tune_scan_type(zswapd_tune_scan_type_hook, NULL);
}
#else
static inline int register_swappiness_tune_hooks(void)
{
	return 0;
}

static void unregister_swappiness_tune_hooks(void)
{
	return;
}
#endif

static int zswapd_cpuhp_state = -1;

int zswapd_module_init(void)
{
	int ret;

	get_fs_types();

	ret = register_swappiness_tune_hooks();
	if (ret) {
		put_fs_types();
		return ret;
	}

	ret = snapshotd_run();
	if (ret) {
		pr_err("%s: failed to run snapshotd.\n", __func__);
		goto snapshotd_err;
	}

	ret = zswapds_run();
	if (ret) {
		pr_err("%s: failed to run zswapd.\n", __func__);
		goto zswapd_err;
	}

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
					"mm/memfusiond:online", zswapd_cpu_online,
					NULL);
	if (ret < 0) {
		pr_err("%s: failed to register hotplug callbacks.\n", __func__);
		goto cpuhp_err;
	}

	zswapd_cpuhp_state = ret;

	ret = register_hooks_for_eswap();
	if (ret) {
		pr_err("%s: register eswap hooks failed!\n", __func__);
		goto hook_eswap_err; 
	}

	return 0;

hook_eswap_err:
	cpuhp_remove_state_nocalls(zswapd_cpuhp_state);
	zswapd_cpuhp_state = -1;

cpuhp_err:
	zswapds_stop();

zswapd_err:
	snapshotd_stop();

snapshotd_err:
	unregister_swappiness_tune_hooks();
	put_fs_types();
	return ret;
}

void zswapd_module_exit(void)
{
	unregister_hooks_for_eswap();
	cpuhp_remove_state_nocalls(zswapd_cpuhp_state);
	zswapds_stop();
	snapshotd_stop();
	unregister_swappiness_tune_hooks();
	put_fs_types();
	return;
}
