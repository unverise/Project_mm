#include <linux/memcontrol.h>
#include <linux/cgroup.h>
#include <linux/sched/mm.h>
#include <linux/mutex.h>
#include <trace/events/block.h>
#include <linux/cpuset.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/swap.h>
#include <linux/mm.h>
#include <linux/kstrtox.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/vmalloc.h>
#include <linux/dma-buf.h>
#include <linux/vmstat.h>
#include <linux/mmzone.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/parser.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/eventfd.h>
#include <trace/hooks/mm.h>
#include <linux/kprobes.h>

#ifdef CONFIG_CMA
#include <linux/cma.h>
#endif

#include "memcg_control.h"
#ifdef CONFIG_ESWAP
#include "eswap_api.h"
#endif

// score should be in the range 0 - 999
#define MAX_APP_SCORE 1000

// static LIST_HEAD(score_head);
static DEFINE_SPINLOCK(score_list_lock);

#define SCORE_LIST_NUM 10
#define SCORE_LIST_SUBRANGE (MAX_APP_SCORE / SCORE_LIST_NUM)
static struct list_head score_heads[SCORE_LIST_NUM] = {};

static inline int __get_score_list_index(int score)
{
	// 0-score list is special
	if (score == 0)
		return 0;

	if (score < SCORE_LIST_SUBRANGE)
		return 1;

	return (score / SCORE_LIST_SUBRANGE);
}

static inline int get_score_list_index(struct memcg_estrack *est)
{
	int score = atomic_read(&est->app_score);
	return __get_score_list_index(score);
}

#ifdef CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT
#define AREA_ANON_REFAULT_THRESHOLD 22000
#define EMPTY_ROUND_SKIP_INTERNVAL 500
#define MAX_SKIP_INTERVAL 4000
#define EMPTY_ROUND_CHECK_THRESHOLD 10

int zswapd_debug = ZSD_ERR;

static DEFINE_MUTEX(zswapd_pectx_mutex);
static struct eventfd_ctx *zswapd_pectx[ZLEVEL_COUNT];

static atomic_t zram_wm_percent = ATOMIC_INIT(0);
static atomic_t zram_comp_percent = ATOMIC_INIT(33);
static int zram2flash_percent = 0;
module_param(zram2flash_percent, int, 0644);

static atomic_t mid_avail = ATOMIC_INIT(0);
static atomic_t min_avail = ATOMIC_INIT(0);
static atomic_t high_avail = ATOMIC_INIT(0);
static atomic_t free_swap_th = ATOMIC_INIT(0);

static atomic_t max_reclaim_size = ATOMIC_INIT(100);
static atomic_t inactive_file_ratio = ATOMIC_INIT(100);
static atomic_t active_file_ratio = ATOMIC_INIT(100);

static atomic64_t area_anon_refault_threshold = ATOMIC_LONG_INIT(AREA_ANON_REFAULT_THRESHOLD);
static atomic64_t default_skip_interval_ms = ATOMIC_LONG_INIT(EMPTY_ROUND_SKIP_INTERNVAL);
static atomic64_t max_skip_interval_ms = ATOMIC_LONG_INIT(MAX_SKIP_INTERVAL);
static atomic64_t empty_round_threshold_pages = ATOMIC_LONG_INIT(EMPTY_ROUND_CHECK_THRESHOLD);

static bool enable_zswapd_reclaim = true;
static bool enable_zswapd_shrink_caches = false;

static u64 swap_full_percent = 100;
static atomic64_t swap_low_threshold = ATOMIC64_INIT(0); 
struct memcg_reclaim_stat_v1 memrec_stat_v1 = {0};
extern bool zram_memcg_compress_enable(void);
#endif /* CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT */

/**
 * get_next_memcg - iterate over memory cgroup score_list
 * @prev: previously returned memcg, NULL on first invocation
 *
 * Returns references to the next memg on score_list of @prev,
 * or %NULL after a full round-trip.
 *
 * Caller must pass the return value in @prev on subsequent
 * invocations for reference counting, or use get_next_memcg_break()
 * to cancel a walk before the round-trip is complete.
 */
#if 0
struct mem_cgroup *get_next_memcg(struct mem_cgroup *prev)
{
	struct mem_cgroup *memcg = NULL;
	struct list_head *pos = NULL;
	unsigned long flags;

	spin_lock_irqsave(&score_list_lock, flags);

	if (!prev) {
		pos = &score_head;
	} else {
		struct memcg_estrack *est = mem_cgroup_estrack(prev);
		pos = &est->score_node;
	}

	if (list_empty(pos)) /* deleted node */
		goto unlock;

	if (pos->next == &score_head)
		goto unlock;

	memcg = score_node_owner(pos->next);

	/* racy:
	 * thead a: owner set by __memcg_estrack_alloc not pretected, and
	 * score node added by memcg_app_score_update(memcg) protected by score_list_lock;
	 * thread b: see the score core, but does not the the owner
	 */
	if (unlikely(!memcg))
		goto unlock;

	if (!css_tryget(&memcg->css))
		memcg = NULL;

unlock:
	spin_unlock_irqrestore(&score_list_lock, flags);

	if (prev)
		css_put(&prev->css);

	return memcg;
}
#else
struct mem_cgroup *get_next_memcg(struct mem_cgroup *prev)
{
	struct mem_cgroup *memcg = NULL;
	struct list_head *pos = NULL;
	unsigned long flags;
	int index, pos_is_head;

	spin_lock_irqsave(&score_list_lock, flags);

	if (!prev) {
		index = SCORE_LIST_NUM - 1;
		pos = &score_heads[index];
		pos_is_head = 1;
	} else {
		struct memcg_estrack *est = mem_cgroup_estrack(prev);
		index = get_score_list_index(est);
		pos = &est->score_node;
		pos_is_head = 0;
	}

retry:
	if (list_empty(pos) && !pos_is_head)
		goto unlock;

	if (pos->next == &score_heads[index]) {
		/* meet the end */
		if (index == 0)
			goto unlock;

		index--;
		pos = &score_heads[index];
		pos_is_head = 1;
		goto retry;
	}

	memcg = score_node_owner(pos->next);

	/* racy:
	 * thead a: owner set by __memcg_estrack_alloc not pretected, and
	 * score node added by memcg_app_score_update(memcg) protected by score_list_lock;
	 * thread b: see the score core, but does not the the owner
	 */
	if (unlikely(!memcg))
		goto unlock;

	if (!css_tryget(&memcg->css))
		memcg = NULL;

unlock:
	spin_unlock_irqrestore(&score_list_lock, flags);

	if (prev)
		css_put(&prev->css);

	return memcg;
}
#endif

void get_next_memcg_break(struct mem_cgroup *memcg)
{
	if (memcg)
		css_put(&memcg->css);
}

#if 0
struct mem_cgroup *get_prev_memcg(struct mem_cgroup *next)
{
	struct mem_cgroup *memcg = NULL;
	struct list_head *pos = NULL;
	unsigned long flags;

	spin_lock_irqsave(&score_list_lock, flags);

	if (unlikely(!next)) {
		pos = &score_head;
	} else {
		struct memcg_estrack *est = mem_cgroup_estrack(next);
		pos = &est->score_node;
	}

	if (list_empty(pos)) /* deleted node */
		goto unlock;

	if (pos->prev == &score_head)
		goto unlock;

	memcg = score_node_owner(pos->prev);

	if (unlikely(!memcg))
		goto unlock;

	if (!css_tryget(&memcg->css))
		memcg = NULL;

unlock:
	spin_unlock_irqrestore(&score_list_lock, flags);

	if (next)
		css_put(&next->css);

	return memcg;
}
#else
struct mem_cgroup *get_prev_memcg(struct mem_cgroup *next)
{
	struct mem_cgroup *memcg = NULL;
	struct list_head *pos = NULL;
	unsigned long flags;
	int index, pos_is_head;

	spin_lock_irqsave(&score_list_lock, flags);

	if (unlikely(!next)) {
		index = 0;
		pos = &score_heads[index];
		pos_is_head = 1;
	} else {
		struct memcg_estrack *est = mem_cgroup_estrack(next);
		index = get_score_list_index(est);
		pos = &est->score_node;
		pos_is_head = 0;
	}

retry:
	if (list_empty(pos) && !pos_is_head) /* deleted node */
		goto unlock;

	if (pos->prev == &score_heads[index]) {
		/* meet the end */
		if (index == SCORE_LIST_NUM -1)
			goto unlock;

		index++;
		pos = &score_heads[index];
		pos_is_head = 1;
		goto retry;
	}

	memcg = score_node_owner(pos->prev);

	if (unlikely(!memcg))
		goto unlock;

	if (!css_tryget(&memcg->css))
		memcg = NULL;

unlock:
	spin_unlock_irqrestore(&score_list_lock, flags);

	if (next)
		css_put(&next->css);

	return memcg;
}
#endif

void get_prev_memcg_break(struct mem_cgroup *memcg)
{
	if (memcg)
		css_put(&memcg->css);
}

static void memcg_app_score_update(struct mem_cgroup *target)
{
#if 0
	struct list_head *pos = NULL;
	unsigned long flags;
	struct memcg_estrack *est_target = mem_cgroup_estrack(target);
	int score;

	if (!est_target)
		return;

	score = atomic_read(&est_target->app_score);
	spin_lock_irqsave(&score_list_lock, flags);
	list_for_each(pos, &score_head) {
		struct mem_cgroup *memcg = score_node_owner(pos);
		struct memcg_estrack *est = mem_cgroup_estrack(memcg);

		if (atomic_read(&est->app_score) < score)
			break;
	}
	list_move_tail(&est_target->score_node, pos);
	spin_unlock_irqrestore(&score_list_lock, flags);
#else
	unsigned long flags;
	struct list_head *list;
	struct memcg_estrack *est_target = mem_cgroup_estrack(target);

	if (!est_target)
		return;

	spin_lock_irqsave(&score_list_lock, flags);
	list = &score_heads[get_score_list_index(est_target)];
	list_move_tail(&est_target->score_node, list);
	spin_unlock_irqrestore(&score_list_lock, flags);
#endif
}

void memcg_app_score_update_from_pf(struct mem_cgroup *target)
{
	struct list_head *list;
	unsigned long flags;
	struct memcg_estrack *est_target = mem_cgroup_estrack(target);
	int index;

	if (!est_target)
		return;

	/* move to the tail of the target score list
	 * using the score range list, not the global list
	 */
	spin_lock_irqsave(&score_list_lock, flags);
	index = get_score_list_index(est_target);
	list = &score_heads[index];
	list_move_tail(&est_target->score_node, list);
	spin_unlock_irqrestore(&score_list_lock, flags);
}

static void memcg_estrack_offline(void *data, struct cgroup_subsys_state *css, struct mem_cgroup *memcg)
{
	unsigned long flags;
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);

	if (est) {
		spin_lock_irqsave(&score_list_lock, flags);
		if (!list_empty(&est->score_node))
			list_del_init(&est->score_node);
		spin_unlock_irqrestore(&score_list_lock, flags);

		ZSWAPD_DEBUG(ZSD_DEBUG, "id-%d, offline %d, memcg %016lx\n",
			memcg->id.id, est->offline, (unsigned long)memcg);
		est->offline = 1;
	}

	// meaningless
	//css_put(css);
}

struct memcg_estrack *__memcg_estrack_alloc(struct mem_cgroup *, const char *);

static bool is_valid_memcg_for_estrack(struct mem_cgroup *memcg, char *name)
{
	struct mem_cgroup *parent;
	struct cgroup *cgrp;
	struct kernfs_node *kn;
	struct kernfs_node *kn_p;
	char myname[ESWAP_MEM_CGROUP_NAME_MAX_LEN];
	char *p = myname;

	/* root memcg get estrack */
	if (memcg == root_mem_cgroup) {
		strcpy(name, "root");
		return true;
	}

	/* estrack does not support inheritance */
	parent = parent_mem_cgroup(memcg);
	if (!mem_cgroup_estrack(parent))
		return false;

	/* pid_xxx does not get estrack */
	cgrp = memcg->css.cgroup;
	kn = cgrp ? cgrp->kn : NULL;
	kn_p = parent->css.cgroup->kn;

	myname[0] = '\0';
	if (kn && kernfs_path_from_node(kn, kn_p, myname, ESWAP_MEM_CGROUP_NAME_MAX_LEN) > 0) {
		if (!strncmp(myname, "/pid_", 5))
			return false;
	} else {
		return false;
	}

	/* skip the leading slash */
	if (myname[0] != '\0')
		p++;

	if (!strncmp(myname, "/uid_", 5)) {
		// uid_10171, testing
		int uid;
		const char *puid = p + strlen("uid_");

		if (kstrtoint(puid, 0, &uid))
			return false;

		if (!(uid <= 19999))
			return false;
	}

	strcpy(name, p);

	/* default true */
	return true;
}

static struct memcg_estrack *memcg_estrack_alloc_online(struct mem_cgroup *memcg, char *name)
{
	return __memcg_estrack_alloc(memcg, name);
}

static void memcg_estrack_online(void *data, struct cgroup_subsys_state *css, struct mem_cgroup *memcg)
{
	char name[ESWAP_MEM_CGROUP_NAME_MAX_LEN];

	if (!is_valid_memcg_for_estrack(memcg, name))
		return;

	/* should reach here rarely,
	 * since we do not create non-pid memcg frequently
	 */
	if (zswapd_debug >= ZSD_DEBUG) {
		char fullname[128];
		struct kernfs_node *kn = memcg->css.cgroup->kn;
		if (kn && kernfs_path_from_node(kn, NULL, fullname, 128) > 0)
			ZSWAPD_DEBUG(ZSD_DEBUG, "id-%d, name %s, memcg %016lx\n",
				memcg->id.id, fullname, (unsigned long)memcg);
	}

	if (!IS_ERR(memcg_estrack_alloc_online(memcg, name)))
		memcg_app_score_update(memcg);

	// meaningless
	//css_get(css);
}

static void memcg_estrack_id_remove(void *data, struct mem_cgroup *memcg)
{
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);
	if (est) {
		eswap_mem_cgroup_remove(memcg);
	}
}

struct memcg_estrack *__memcg_estrack_alloc(struct mem_cgroup *memcg, const char *name)
{
	struct memcg_estrack *est;
	s64 old;
	int uid = -1;

	if (name == NULL)
		return ERR_PTR(-EINVAL);

	est = kzalloc(sizeof(struct memcg_estrack), GFP_ATOMIC);
	if (est == NULL)
		return ERR_PTR(-ENOMEM);
	
	INIT_LIST_HEAD(&est->score_node);
	est->owner = memcg;

	if (name) {
		// uid_10171, testing
		const char *p = name + strlen("uid_");
		strcpy(est->name, name);

		if (kstrtoint(p, 0, &uid))
			uid = -1;
	}

	/*
	 * Initializing the memcg parameters.
	 * 300 means default adj for creating the memcg.
	 * 100 means default swap-in rate.
	 * 10 means default zram2ufs rate.
	 * 60 means default mem2zram rate.
	 * 50 means default refault threshold.
	 */
	atomic_set(&est->app_score, 300);

#ifdef CONFIG_ESWAP
	atomic_set(&est->flash2zram_ratio, 20);
	spin_lock_init(&est->zram_init_lock);
#endif

#ifdef CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT
	atomic_set(&est->mem2zram_ratio, 60);
	atomic_set(&est->zram2flash_ratio, (uid <= 10000) ? 20 : 35);
	atomic_set(&est->app_score, (uid <= 10000) ? 300 : 600);
	atomic_set(&est->def_score, (uid <= 10000) ? 300 : 600);
	atomic_set(&est->anon_rf_ratio, 50);
	est->reclaim_brk_reason = 0;
	est->reclaim_oneshot = 0;
#endif

	old = atomic64_cmpxchg((atomic64_t*)(MEMCG_RAW_DATA0_PTR(memcg)), 0, (s64)est);
	if (old != 0) {
		ZSWAPD_DEBUG(ZSD_ERR, "id-%d, memcg %016lx extrack exist\n",
			memcg->id.id, (unsigned long)memcg);
		kfree(est);
		est = mem_cgroup_estrack(memcg);
	}
	est->offline = 0;
	return est;
}

#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include <linux/tracepoint.h>
#if IS_ENABLED(CONFIG_TRANS_SCHED_FEATURE)
#include <trans_sched_interface.h>
#endif

static int kcompactd_abort_th = 90;
module_param(kcompactd_abort_th, int, 0644);

static int kcompactd_abort_debug = 0;
module_param(kcompactd_abort_debug, int, 0644);

static int kcompactd_contend_cnt = 0;
module_param(kcompactd_contend_cnt, int, 0644);

static int kcompactd_abort_cnt = 0;
static int kcompactd_cnt = 0;

struct cpuload_track {
	struct timer_list timer;;
	spinlock_t lock;

	u64 last_busy_time;
	u64 last_total_time;

	int tracking;
	int cpuload;
	bool compact_abort;
	bool inited;

#define CPCNT 6
	// for debug
	unsigned int seq;
	int cps[CPCNT];
	char buf[100];
};
static struct cpuload_track cltrack;
static int kcompactd_block_user = 0;

static inline int is_current_kcompactd(void)
{
	int nid;
	for_each_online_node(nid) {
		if (NODE_DATA(nid)->kcompactd == current) {
			return 1;
		}
	}
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
struct tracepoint_node {
	struct tracepoint *tp;
	const char *name;
};
static struct tracepoint_node tn = {
	.tp = NULL,
	.name = "sched_blocked_reason",
};

static void __probe(void *data, struct task_struct *tsk)
{
	if (!is_current_kcompactd())
		return;

	if (!tsk->mm)
		return;

	if (!tsk->in_iowait)
		return;

	/* opps, kcompactd blocked user space and make it iowait */

#if IS_ENABLED(CONFIG_TRANS_SCHED_FEATURE)
	if (g_trans_sched_interface.check_ux_task && g_trans_sched_interface.check_ux_task(tsk)) {
		kcompactd_block_user = 1;
		kcompactd_contend_cnt++;
		if (kcompactd_abort_debug) {
			printk("kcompactd_block_user\n");
		}
	}
#else
	kcompactd_block_user = 1;
	kcompactd_contend_cnt++;
	if (kcompactd_abort_debug) {
		printk("kcompactd_block_user\n");
	}
#endif
}

static void find_my_tracepoint(struct tracepoint *tp, void *priv)
{
	struct tracepoint_node *t = (struct tracepoint_node *)priv;

	if (!strcmp(tp->name, t->name))
		t->tp = tp;

	return;
}

static void register_sched_block_reason_hook(void)
{
	for_each_kernel_tracepoint(find_my_tracepoint, &tn);
	if (tn.tp != NULL) {
		tracepoint_probe_register(tn.tp, __probe, NULL);
		printk("%s\n", __func__);
	}
}

static void unregister_sched_block_reason_hook(void)
{
	if (tn.tp != NULL)
		tracepoint_probe_unregister(tn.tp, __probe, NULL);
}
#endif

static u64 __get_idle_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 idle, idle_usecs = -1ULL;

	if (cpu_online(cpu))
		idle_usecs = get_cpu_idle_time_us(cpu, NULL);

	if (idle_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcs->cpustat[CPUTIME_IDLE];
	else
		idle = idle_usecs * NSEC_PER_USEC;

	return idle;
}

static void compactd_cpuload_track_func(struct timer_list *t)
{
	int i;
	unsigned long flags;
	u64 user, nice, system, idle, busy_time, total_time;
	u64 busy_delta, total_delta;
	int cp = 0;
	struct cpuload_track *clt = from_timer(clt, t, timer);
	int index, count = 0;

	user = 0;
	nice = 0;
	system = 0;
	idle = 0;
	for_each_possible_cpu(i) {
		struct kernel_cpustat kcpustat;
		u64 *cpustat = kcpustat.cpustat;

		kcpustat_cpu_fetch(&kcpustat, i);

		user		+= cpustat[CPUTIME_USER];
		nice		+= cpustat[CPUTIME_NICE];
		system		+= cpustat[CPUTIME_SYSTEM];
		idle		+= __get_idle_time(&kcpustat, i);
	}
	busy_time = nsec_to_clock_t(user + nice + system);
	total_time = nsec_to_clock_t(user + nice + system + idle);

	spin_lock_irqsave(&clt->lock, flags);
	if (!clt->tracking)
		goto out_unlock;

	busy_delta = busy_time - clt->last_busy_time;
	total_delta = total_time - clt->last_total_time;

	if (total_delta == 0) {
		/* do not update accounter and reschedule again */
		mod_timer(t, jiffies + msecs_to_jiffies(10));
		goto out_unlock;
	}

	cp = busy_delta * 100 / total_delta;
	if (cp > 100)
		cp  = 100;

	clt->last_busy_time = busy_time;
	clt->last_total_time = total_time;

	clt->cpuload = cp;
	if (clt->cpuload > kcompactd_abort_th) {
		clt->compact_abort = true;
		mod_timer(t, jiffies + msecs_to_jiffies(100));
	} else {
		clt->compact_abort = false;
		mod_timer(t, jiffies + msecs_to_jiffies(5));
	}

	if (kcompactd_abort_debug) {
		int len = 0;

		clt->cps[clt->seq % CPCNT] = cp;
		clt->seq++;

		index = (clt->seq - 1) % CPCNT;
		count = (clt->seq >= CPCNT) ? CPCNT : clt->seq;

		while (count--) {
			len += snprintf(clt->buf + len, 100 - len, "%d ", clt->cps[index]);
			if (index == 0)
				index = CPCNT;
			index--;
		}
	}

out_unlock:
	spin_unlock_irqrestore(&clt->lock, flags);

	if (kcompactd_abort_debug) {
		printk("%s %d, %s\n", __FUNCTION__, cp, clt->buf);
	}
}

static bool should_abort_compaction(struct cpuload_track *clt)
{
	unsigned long flags;
	int i;
	u64 user, nice, system, idle;

	spin_lock_irqsave(&clt->lock, flags);
	if (clt->tracking) {
		/* already tracking, check the result */
		bool should_abort = false;
		int cp = 0;

		if (clt->compact_abort) {
			should_abort = true;
			kcompactd_abort_cnt++;
			clt->tracking = 0;
			clt->seq = 0;
			timer_delete(&clt->timer);
		}

		cp = clt->cpuload;
		spin_unlock_irqrestore(&clt->lock, flags);

		if (kcompactd_abort_debug && should_abort)
			printk("%s abort %d\n", __FUNCTION__, cp);
		return should_abort || (kcompactd_block_user == 1);
	}

	/* start a new cpuload tracking */
	user = 0;
	nice = 0;
	system = 0;
	idle = 0;
	for_each_possible_cpu(i) {
		struct kernel_cpustat kcpustat;
		u64 *cpustat = kcpustat.cpustat;

		kcpustat_cpu_fetch(&kcpustat, i);

		user		+= cpustat[CPUTIME_USER];
		nice		+= cpustat[CPUTIME_NICE];
		system		+= cpustat[CPUTIME_SYSTEM];
		idle		+= __get_idle_time(&kcpustat, i);
	}

	timer_delete(&clt->timer);
	clt->last_busy_time = nsec_to_clock_t(user + nice + system);
	clt->last_total_time = nsec_to_clock_t(user + nice + system + idle);
	clt->tracking = 1;
	clt->cpuload = 0;
	clt->compact_abort = false;
	mod_timer(&clt->timer, jiffies + 1);
	spin_unlock_irqrestore(&clt->lock, flags);
	return (kcompactd_block_user == 1);
}

static void compact_finished(void *data, bool *abort)
{
	/* only abort kcompactd */
	if (!is_current_kcompactd())
		return;

	if (should_abort_compaction((struct cpuload_track *)data)) {
		kcompactd_block_user = 0;
		*abort = true;
	}

	if (kcompactd_abort_debug)
		printk("%s\n", __FUNCTION__);
	return;
}

static void compact_end(void *data, struct compact_control *cc, long vendor_ret)
{
	(void)cc;
	(void)vendor_ret;

	/* only abort kcompactd */
	if (is_current_kcompactd()) {
		struct cpuload_track *clt = (struct cpuload_track *)data;
		unsigned long flags;

		kcompactd_block_user = 0;

		spin_lock_irqsave(&clt->lock, flags);
		if (clt->tracking) {
			/* stop the tracking */
			clt->tracking = 0;
			clt->seq = 0;
			clt->compact_abort = false;
			timer_delete(&clt->timer);
		}
		kcompactd_cnt++;
		spin_unlock_irqrestore(&clt->lock, flags);

		if (kcompactd_abort_debug)
			printk("%s %d %d\n", __FUNCTION__, kcompactd_abort_cnt, kcompactd_cnt);
	}
	return;
}

static void memcg_estrack_alloc(void *data, struct mem_cgroup *memcg)
{
	ZSWAPD_DEBUG(ZSD_DEBUG, "id-%d, memcg %016lx\n", memcg->id.id, (unsigned long)memcg);
	return;
}

static void memcg_estrack_free(void *data, struct mem_cgroup *memcg)
{
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);
	if (est) {
		unsigned long flags;

		spin_lock_irqsave(&score_list_lock, flags);
		if (!list_empty(&est->score_node))
			list_del_init(&est->score_node);
		spin_unlock_irqrestore(&score_list_lock, flags);

		ZSWAPD_DEBUG(ZSD_DEBUG, "id-%d, offline %d, memcg %016lx\n",
				memcg->id.id, est->offline, (unsigned long)memcg);
		INIT_MEMCG_ESTRACK(memcg, 0);
		kfree(est);
	}
}

#if KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE
struct mem_cgroup *mem_cgroup_iter_internal(struct mem_cgroup *root,
				   struct mem_cgroup *prev,
				   struct mem_cgroup_reclaim_cookie *reclaim)
{
	struct mem_cgroup_reclaim_iter *iter;
	struct cgroup_subsys_state *css;
	struct mem_cgroup *pos;
	struct mem_cgroup *next;

	if (mem_cgroup_disabled())
		return NULL;

	if (!root)
		root = root_mem_cgroup;

	rcu_read_lock();
restart:
	next = NULL;

	if (reclaim) {
		int gen;
		int nid = reclaim->pgdat->node_id;

		iter = &root->nodeinfo[nid]->iter;
		gen = atomic_read(&iter->generation);

		/*
		 * On start, join the current reclaim iteration cycle.
		 * Exit when a concurrent walker completes it.
		 */
		if (!prev)
			reclaim->generation = gen;
		else if (reclaim->generation != gen)
			goto out_unlock;

		pos = READ_ONCE(iter->position);
	} else
		pos = prev;

	css = pos ? &pos->css : NULL;

	while ((css = css_next_descendant_pre(css, &root->css))) {
		/*
		 * Verify the css and acquire a reference.  The root
		 * is provided by the caller, so we know it's alive
		 * and kicking, and don't take an extra reference.
		 */
		if (css == &root->css || css_tryget(css))
			break;
	}

	next = mem_cgroup_from_css(css);

	if (reclaim) {
		/*
		 * The position could have already been updated by a competing
		 * thread, so check that the value hasn't changed since we read
		 * it to avoid reclaiming from the same cgroup twice.
		 */
		if (cmpxchg(&iter->position, pos, next) != pos) {
			if (css && css != &root->css)
				css_put(css);
			goto restart;
		}

		if (!next) {
			atomic_inc(&iter->generation);

			/*
			 * Reclaimers share the hierarchy walk, and a
			 * new one might jump in right at the end of
			 * the hierarchy - make sure they see at least
			 * one group and restart from the beginning.
			 */
			if (!prev)
				goto restart;
		}
	}

out_unlock:
	rcu_read_unlock();
	if (prev && prev != root)
		css_put(&prev->css);

	return next;
}
#else
struct mem_cgroup *mem_cgroup_iter_internal(struct mem_cgroup *root,
	struct mem_cgroup *prev, struct mem_cgroup_reclaim_cookie *reclaim)
{
	struct mem_cgroup_reclaim_iter *iter;
	struct cgroup_subsys_state *css = NULL;
	struct mem_cgroup *memcg = NULL;
	struct mem_cgroup *pos = NULL;

	if (mem_cgroup_disabled())
		return NULL;

	if (!root)
		root = root_mem_cgroup;

	rcu_read_lock();

	if (reclaim) {
		struct mem_cgroup_per_node *mz;

		mz = root->nodeinfo[reclaim->pgdat->node_id];
		iter = &mz->iter;

		/*
		 * On start, join the current reclaim iteration cycle.
		 * Exit when a concurrent walker completes it.
		 */
		if (!prev)
			reclaim->generation = iter->generation;
		else if (reclaim->generation != iter->generation)
			goto out_unlock;

		while (1) {
			pos = READ_ONCE(iter->position);
			if (!pos || css_tryget(&pos->css))
				break;
			/*
			 * css reference reached zero, so iter->position will
			 * be cleared by ->css_released. However, we should not
			 * rely on this happening soon, because ->css_released
			 * is called from a work queue, and by busy-waiting we
			 * might block it. So we clear iter->position right
			 * away.
			 */
			(void)cmpxchg(&iter->position, pos, NULL);
		}
	} else if (prev) {
		pos = prev;
	}

	if (pos)
		css = &pos->css;

	for (;;) {
		css = css_next_descendant_pre(css, &root->css);
		if (!css) {
			/*
			 * Reclaimers share the hierarchy walk, and a
			 * new one might jump in right at the end of
			 * the hierarchy - make sure they see at least
			 * one group and restart from the beginning.
			 */
			if (!prev)
				continue;
			break;
		}

		/*
		 * Verify the css and acquire a reference.  The root
		 * is provided by the caller, so we know it's alive
		 * and kicking, and don't take an extra reference.
		 */
		if (css == &root->css || css_tryget(css)) {
			memcg = mem_cgroup_from_css(css);
			break;
		}
	}

	if (reclaim) {
		/*
		 * The position could have already been updated by a competing
		 * thread, so check that the value hasn't changed since we read
		 * it to avoid reclaiming from the same cgroup twice.
		 */
		(void)cmpxchg(&iter->position, pos, memcg);

		if (pos)
			css_put(&pos->css);

		if (!memcg)
			iter->generation++;
	}

out_unlock:
	rcu_read_unlock();
	if (prev && prev != root)
		css_put(&prev->css);

	return memcg;
}
#endif

void mem_cgroup_iter_break_internal(struct mem_cgroup *root,
			   struct mem_cgroup *prev)
{
	if (!root)
		root = root_mem_cgroup;
	if (prev && prev != root)
		css_put(&prev->css);
}

static void memcg_estrack_traversal(struct lruvec *lruvec)
{
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);

	if (!est) {
		if (memcg->id.id) {
			char name[ESWAP_MEM_CGROUP_NAME_MAX_LEN];

			if (!is_valid_memcg_for_estrack(memcg, name))
				return;

			if (!IS_ERR(memcg_estrack_alloc_online(memcg, name)))
				memcg_app_score_update(memcg);
		}
	}
}

static void do_traversal_all_lruvec_internal(void)
{
	int nid;

	for_each_online_node(nid) {
		pg_data_t *pgdat = NODE_DATA(nid);
		struct mem_cgroup *memcg = NULL;

		memcg = mem_cgroup_iter_internal(NULL, NULL, NULL);
		do {
			struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);

			memcg_estrack_traversal(lruvec);

			memcg = mem_cgroup_iter_internal(NULL, memcg, NULL);
		} while (memcg);
	}
}

static int memcg_app_value_list_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = NULL;
	struct memcg_estrack *est;

	while ((memcg = get_next_memcg(memcg))) {
		est = mem_cgroup_estrack(memcg);
		seq_printf(m, "id %d score %d name %s\n", memcg->id.id, atomic_read(&est->app_score),
				(est->name[0] == '\0' ? "<none>" : est->name));
	}

	return 0;
}

static ssize_t mem_cgroup_name_write(struct kernfs_open_file *of, char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(memcg);

	if (!zram_memcg_compress_enable()) {
		ZSWAPD_DEBUG(ZSD_INFO, "memcg compact disabled\n");
		return nbytes;
	}

	buf = strstrip(buf);
	if (nbytes >= ESWAP_MEM_CGROUP_NAME_MAX_LEN)
		return -EINVAL;

	if (memcg)
		strncpy(est->name, buf, ESWAP_MEM_CGROUP_NAME_MAX_LEN);

	return nbytes;
}

static int mem_cgroup_name_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(seq_css(m));
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(memcg);

	seq_printf(m, "%s\n", est->name);
	return 0;
}

static int mem_cgroup_app_value_write(struct cgroup_subsys_state *css, struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(memcg);
	int score = (int)val;

	if (!zram_memcg_compress_enable()) {
		ZSWAPD_DEBUG(ZSD_INFO, "memcg compact disabled\n");
		return 0;
	}

	if (score >= MAX_APP_SCORE)
		return -EINVAL;

	if (atomic_read(&est->app_score) != score) {
		atomic_set(&est->app_score, score);
		memcg_app_score_update(memcg);
	}

	return 0;
}

// promote to 0
void mem_cgroup_app_value_promote(struct memcg_estrack *target_est)
{
	unsigned long flags;
	struct list_head *list = &score_heads[0], *l;
	struct list_head *p, *n;

	spin_lock_irqsave(&score_list_lock, flags);

	list_for_each_safe(p, n, list) {
		struct mem_cgroup *memcg = score_node_owner(p);
		struct memcg_estrack *est = mem_cgroup_estrack(memcg);
		int def_score = atomic_read(&est->def_score);

		/* on the score list 0, all app_score is 0 */
		if (0 != def_score) {
			/* l is non-0 score list */
			l = &score_heads[__get_score_list_index(def_score)];
			list_move_tail(&est->score_node, l);
			atomic_set(&est->app_score, def_score);
		}
	}

	/* promote to the score list 0 */
	l = &score_heads[0];
	list_move_tail(&target_est->score_node, l);
	atomic_set(&target_est->app_score, 0);

	spin_unlock_irqrestore(&score_list_lock, flags);
}

static int mem_cgroup_app_value_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(seq_css(m));
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(memcg);

	seq_printf(m, "%d\n", atomic_read(&est->app_score));
	return 0;
}

#ifdef CONFIG_ESWAP
struct mem_cgroup *memcg_eswap_memcg(struct mem_cgroup *memcg)
{
	while (memcg && !MEMCG_ESTRACK_VALID(memcg))
		memcg = parent_mem_cgroup(memcg);

	return memcg;
}

struct mem_cgroup *page_eswap_memcg(struct page *page)
{
	return memcg_eswap_memcg(page_memcg(page));
}

static int total_info_per_app_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = NULL;
	unsigned long anon_size_total = 0;
	unsigned long zram_size_total = 0, eswap_size_total = 0;
	unsigned long zram_compress_size_total = 0, eswap_compress_size_total = 0;
	unsigned long same_pages_total = 0, huge_pages_total = 0;
	int compress_ratio_zram, compress_ratio_eswap;

	while ((memcg = get_next_memcg(memcg))) {
		struct memcg_estrack *est = mem_cgroup_estrack(memcg);
		unsigned long anon_size, zram_size, eswap_size;
		unsigned long zram_compress_size, eswap_compress_size;
		unsigned long same_pages, huge_pages;

		if (memcg == root_mem_cgroup) {
			anon_size = memcg_pages_local(memcg, NR_ACTIVE_ANON)
					+ memcg_pages_local(memcg, NR_INACTIVE_ANON);
		} else {
			anon_size = memcg_pages(memcg, NR_ACTIVE_ANON)
					+ memcg_pages(memcg, NR_INACTIVE_ANON);
		}

		zram_size = (unsigned long)atomic_read(&est->zram_page_size);
		eswap_size = (unsigned long)atomic_read(&est->eswap_stored_pages);

		zram_compress_size = (unsigned long)atomic_read(&est->zram_stored_size);
		eswap_compress_size = (unsigned long)atomic_read(&est->eswap_stored_size);

		anon_size *= PAGE_SIZE / SZ_1K;
		zram_size *= PAGE_SIZE / SZ_1K;
		eswap_size *= PAGE_SIZE / SZ_1K;

		zram_compress_size /= SZ_1K;
		eswap_compress_size /= SZ_1K;

		same_pages = (unsigned long)atomic64_read(&est->same_pages);
		huge_pages = (unsigned long)atomic64_read(&est->huge_pages);

		anon_size_total += anon_size;
		zram_size_total += zram_size;
		eswap_size_total += eswap_size;

		zram_compress_size_total += zram_compress_size;
		eswap_compress_size_total += eswap_compress_size;
		same_pages_total += same_pages;
		huge_pages_total += huge_pages;

		compress_ratio_zram = (int)(zram_size * 100 /(zram_compress_size + 1));
		compress_ratio_eswap = (int)(eswap_size * 100 /(eswap_compress_size + 1));

		/* skip root, apps, system */
		if (est->name[0] != '\0') {
			seq_printf(m, "%s\t%8lu %8lu %8lu (%d) %8lu %8lu (%d) %8lu %8lu\n",
				est->name, anon_size,
				zram_size, zram_compress_size, compress_ratio_zram,
				eswap_size, eswap_compress_size, compress_ratio_eswap,
				same_pages, huge_pages);
		}
	}

	compress_ratio_zram = (int)(zram_size_total * 100 /(zram_compress_size_total + 1));
	compress_ratio_eswap = (int)(eswap_size_total * 100 /(eswap_compress_size_total + 1));

	seq_printf(m, "total anon %lu KB, total zram comp size (KB) %lu/%lu (%d)"
		"total eswap comp size (KB) %lu/%lu (%d)\n", anon_size_total,
		zram_compress_size_total, zram_size_total, compress_ratio_zram,
		eswap_compress_size_total, eswap_size_total, compress_ratio_eswap);
	seq_printf(m, "total same pages %lu, total huge pages %lu\n",
		same_pages_total, huge_pages_total);

	return 0;
}

int report_app_info_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = NULL;
	unsigned long anon_size;
	unsigned long zram_size;
	unsigned long eswap_size;
	unsigned long swap_out_size;
	unsigned long swap_in_size;

	while ((memcg = get_next_memcg(memcg))) {
		struct memcg_estrack *est = mem_cgroup_estrack(memcg);
		u64 score = atomic_read(&est->app_score);

		anon_size = memcg_pages(memcg, NR_INACTIVE_ANON) + memcg_pages(memcg, NR_ACTIVE_ANON);
		eswap_size = (unsigned long)atomic_read(&est->eswap_stored_pages);
		zram_size = (unsigned long)atomic_read(&est->zram_page_size);
		swap_out_size = (unsigned long )atomic64_read(&est->eswap_out_extent_cnt) << 15;
		swap_in_size = (unsigned long)atomic64_read(&est->eswap_in_extent_cnt) << 15;

		if (anon_size + zram_size + eswap_size == 0)
			continue;

		anon_size *= PAGE_SIZE / SZ_1K;
		zram_size *= PAGE_SIZE / SZ_1K;
		eswap_size *= PAGE_SIZE / SZ_1K;

		seq_printf(m, "%s,%llu,%lu,%lu,%lu,%lu,%lu\n", est->name[0] != '\0' ? est->name : "root",
			score, anon_size, zram_size, eswap_size, swap_out_size, swap_in_size);
	}
	return 0;
}

static char *extract_sub_memcg_name(struct mem_cgroup *parent,
	struct mem_cgroup *child, char *name, size_t len)
{
	struct kernfs_node *kn_p = parent->css.cgroup->kn;
	struct kernfs_node *kn = child->css.cgroup->kn;
	char *p = name;
	int ret;

	if (len < ESWAP_MEM_CGROUP_NAME_MAX_LEN)
		return NULL;

	kn_p = parent->css.cgroup->kn;
	kn = child->css.cgroup->kn;
	name[0] = '\0';
	ret = kernfs_path_from_node(kn, kn_p, name, len);
	if (ret < 0)
		return NULL;

	if (name[0] != '\0')
		p++;

	return p;
}

static int mem_cgroup_eswap_stat_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *target_memcg = mem_cgroup_from_css(seq_css(m));
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(target_memcg);

	seq_printf(m, "eswap out cnt    :     %lld\n", atomic64_read(&est->eswap_out_cnt));
	seq_printf(m, "eswap out extents:     %lld\n", atomic64_read(&est->eswap_out_extent_cnt));
	seq_printf(m, "eswap in cnt     :     %lld\n", atomic64_read(&est->eswap_in_cnt));
	seq_printf(m, "eswap in extents :     %lld\n", atomic64_read(&est->eswap_in_extent_cnt));
	seq_printf(m, "eswap fault cnt  :     %lld\n", atomic64_read(&est->eswap_in_fault_cnt));
	return 0;
}

static int mem_cgroup_eswap_zraminfo_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *target_memcg = mem_cgroup_from_css(seq_css(m));
	struct mem_cgroup *memcg;
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(target_memcg);
	int same_pages_total = 0, huge_pages_total = 0;

	memcg = mem_cgroup_iter_internal(target_memcg, NULL, NULL);
	do {
		if (memcg == target_memcg) {
			unsigned long same_pages = 0, huge_pages = 0;

			/* total info */
			same_pages = (unsigned long)atomic64_read(&est->same_pages);
			huge_pages = (unsigned long)atomic64_read(&est->huge_pages);
			seq_printf(m, "%8lu %8lu \t%s(total)\n", same_pages, huge_pages, est->name);

			/* for the memcg itself */
			same_pages = (unsigned long)memcg_read_samepages_counter(memcg);
			huge_pages = (unsigned long)memcg_read_hugepages_counter(memcg);
			same_pages_total += same_pages;
			huge_pages_total += huge_pages;

			seq_printf(m, "%8lu %8lu \t%s\n", same_pages, huge_pages, est->name);
		} else {
			int same_pages = 0, huge_pages = 0;
			char buf[ESWAP_MEM_CGROUP_NAME_MAX_LEN];
			char *myname;

			same_pages = memcg_read_samepages_counter(memcg);
			huge_pages = memcg_read_hugepages_counter(memcg);

			same_pages_total += same_pages;
			huge_pages_total += huge_pages;

			myname = extract_sub_memcg_name(target_memcg, memcg,
					&buf[0], ESWAP_MEM_CGROUP_NAME_MAX_LEN);
			seq_printf(m, "%8d %8d \t%s\n", same_pages, huge_pages, myname ? myname: "NA");
		}

		memcg = mem_cgroup_iter_internal(target_memcg, memcg, NULL);
	} while (memcg);

	/* for sanity checking */
	seq_printf(m, "%s total same and huge pages cnt : %d %d\n",
			est->name, same_pages_total, huge_pages_total);
	return 0;
}

static int mem_cgroup_eswap_all_stat_show(struct seq_file *m, void *v)
{
	eswap_stat_show(m);
	return 0;
}

static int mem_cgroup_eswap_area_stat_show(struct seq_file *m, void *v)
{
	eswap_area_stat_show(m);
	return 0;
}

static int mem_cgroup_eswap_set_loglevel(struct cgroup_subsys_state *css, struct cftype *cft, u64 val)
{
	eswap_set_loglevel((int)val);
	return 0;
}

static u64 mem_cgroup_eswap_get_loglevel(struct cgroup_subsys_state *css, struct cftype *cft)
{
	return (u64)eswap_loglevel();
}

static int mem_cgroup_eswap_set_acct_detail(struct cgroup_subsys_state *css, struct cftype *cft, u64 val)
{
	eswap_set_acct_detail((int)val);
	return 0;
}

static u64 mem_cgroup_eswap_get_acct_detail(struct cgroup_subsys_state *css, struct cftype *cft)
{
	return (u64)eswap_acct_detail();
}

static ssize_t mem_cgroup_eswap_test(struct kernfs_open_file *of, char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);
	int test = 0, ret;

	if (!est)
		return -ENOSYS;

	if (kstrtoint(buf, 0, &test)) {
		return -EINVAL;
	}

	switch (test) {
	case 1:
		/* reclaim in simulate */
		ret = memcg_eswap_out(memcg);
		break;
	case 2:
		/* batch out simulate */
		ret = memcg_eswap_batch_in(memcg, true, false);
		break;
	case 3:
		/* pre out simulate */
		ret = memcg_eswap_batch_in(memcg, true, true);
		break;
	case 4:
		memcg = NULL;
		while ((memcg = get_next_memcg(memcg)))
			memcg_eswap_out(memcg);
		break;
	case 5:
		memcg = NULL;
		while ((memcg = get_next_memcg(memcg)))
			memcg_eswap_batch_in(memcg, false, false);
		break;
	case 6:
		memcg = NULL;
		while ((memcg = get_next_memcg(memcg)))
			memcg_eswap_batch_in(memcg, false, true);
		break;
	default:
		break;
	}
	return nbytes;
}

static ssize_t mem_cgroup_eswap_flash2zram_ratio_write(struct kernfs_open_file *of,
		char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(memcg);

	int val;
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	if (val > 100 || val < 0)
		return -EINVAL;

	atomic_set(&est->flash2zram_ratio, val);
	return nbytes;
}

static int mem_cgroup_eswap_flash2zram_ratio_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(seq_css(m));
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(memcg);

	seq_printf(m, "%d\n", atomic_read(&est->flash2zram_ratio));
	return 0;
}

static ssize_t mem_cgroup_eswap_in_write(struct kernfs_open_file *of, char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(memcg);
	int ratio = 100;
	unsigned long size;

	int val;
	if (kstrtoint(buf, 0, &val)) {
		return -EINVAL;
	}

	ratio = atomic_read(&est->flash2zram_ratio);
	size = (unsigned long)atomic_read(&est->eswap_stored_size);
	if (ratio == 100)
		size = size * 2;
	else
		size = size * ratio / 100;

	eswap_batch_in(memcg, size, true, val ? true : false /*preload*/);
	return nbytes;
}
#endif

#ifdef CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT
inline bool zswapd_anon_reclaim_enabled(void)
{
	return enable_zswapd_reclaim;
}

inline bool zswapd_drop_caches_enabled(void)
{
	return enable_zswapd_shrink_caches;
}

inline unsigned int zswapd_get_zram_wm_percent(void)
{
	return atomic_read(&zram_wm_percent);
}

inline unsigned int zswapd_get_zram_comp_percent(void)
{
	return atomic_read(&zram_comp_percent);
}

inline unsigned int avail_mid_th(void)
{
	return atomic_read(&mid_avail);
}

inline unsigned int avail_min_th(void)
{
	return atomic_read(&min_avail);
}

inline unsigned int avail_high_th(void)
{
	return atomic_read(&high_avail);
}

inline unsigned int zswapd_get_max_reclaim_size(void)
{
	return atomic_read(&max_reclaim_size);
}

// pages
inline u64 get_free_swap_threshold_value(void)
{
	return MB_TO_PAGES(atomic_read(&free_swap_th));
}

inline unsigned int get_inactive_file_ratio_value(void)
{
	return atomic_read(&inactive_file_ratio);
}

inline unsigned int get_active_file_ratio_value(void)
{
	return atomic_read(&active_file_ratio);
}

inline unsigned long long get_area_anon_refault_threshold_value(void)
{
	return atomic64_read(&area_anon_refault_threshold);
}

inline unsigned long zswapd_get_skip_interval_ms(void)
{
	return atomic64_read(&default_skip_interval_ms);
}

inline unsigned long zswapd_get_max_skip_interval_ms(void)
{
	return atomic64_read(&max_skip_interval_ms);
}

inline unsigned long zswapd_get_empty_round_threshold_pages(void)
{
	return atomic64_read(&empty_round_threshold_pages);
}

int swap_is_low(void)
{
	struct sysinfo si;

	si_swapinfo(&si);
	
	if (si.totalswap == 0) {
		return 1;
	}

	return (u64)(si.freeswap) < (u64)atomic64_read(&swap_low_threshold);
}

static int zswapd_swap_full_percent_write(struct cgroup_subsys_state *css, struct cftype *cft, u64 val)
{
	struct sysinfo si;
	u64 free_low = 0;

	if (val > 100 || val < 30) {
		return -EINVAL;
	}

	si_swapinfo(&si);
	if (si.totalswap == 0) {
		return -EINVAL;
	}
	
	free_low = si.totalswap * (100 - val) / 100;
	swap_full_percent = val;
	atomic64_set(&swap_low_threshold, (s64)free_low);

	return 0;
}

static u64 zswapd_swap_full_percent_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	return swap_full_percent;
}

enum {
	MEMORY_RECLAIM_SWAPPINESS = 0,
	MEMORY_RECLAIM_STEP,
	MEMORY_RECLAIM_ASYNC,
	MEMORY_RECLAIM_PROP,
	MEMORY_RECLAIM_TIME,
	MEMORY_RECLAIM_CLEAN,
	MEMORY_RECLAIM_FLUSH,
	MEMORY_RECLAIM_LOCAL,
	MEMORY_RECLAIM_NULL,
};

#define MEMORY_BK_RECLAIM_OFFSET -100
#define MEMCG_COMPRESS_MAX_TIME 30*1000UL
static char* reason_string[MEMORY_BK_RECLAIM_MAX] = {"none", "signal", "time", "size", "prop", "abort"};
static const match_table_t tokens = {
	{ MEMORY_RECLAIM_SWAPPINESS, "swappiness=%d"},
	{ MEMORY_RECLAIM_STEP, "step=%d"},
	{ MEMORY_RECLAIM_ASYNC, "async=%d"},
	{ MEMORY_RECLAIM_PROP, "prop=%d"},
	{ MEMORY_RECLAIM_TIME, "time=%d"},
	{ MEMORY_RECLAIM_CLEAN, "clean=%d"},
	{ MEMORY_RECLAIM_FLUSH, "flush=%d"},
	{ MEMORY_RECLAIM_LOCAL, "local=%d"},
	{ MEMORY_RECLAIM_NULL, NULL },
};

typedef void (*mem_cgroup_flush_stats_func)(struct mem_cgroup *memcg);
static mem_cgroup_flush_stats_func mem_cgroup_flush_stats_addr = NULL;
static int get_mem_cgroup_flush_stats_addr(void)
{
	struct kprobe kprobe_data = {
		.symbol_name = "mem_cgroup_flush_stats"
	};

	if (mem_cgroup_flush_stats_addr)
		return 0;

	/* get some symbols address using kprobe */
	if (register_kprobe(&kprobe_data)) {
		pr_err("find memcg flush stats failed!\n");
		return -1;
	}

	mem_cgroup_flush_stats_addr = (mem_cgroup_flush_stats_func)kprobe_data.addr;
	unregister_kprobe(&kprobe_data);

	if (!mem_cgroup_flush_stats_addr) {
		return -1;
	}
	return 0;
}

static int mem_cgroup_smart_reclaim_single_snapshot(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(seq_css(m));
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(memcg);

	if (!zram_memcg_compress_enable()) {
		seq_printf(m, "0 0\n");
		return 0;
	}

	seq_printf(m, "%d %d\n", est->reclaim_brk_reason, est->reclaim_oneshot);
	return 0;
}

static ssize_t mem_cgroup_smart_reclaim(struct kernfs_open_file *of,
		char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned int nr_retries = MAX_RECLAIM_RETRIES;
	unsigned long nr_to_reclaim = 0, nr_reclaimed = 0;
	unsigned long last_time;
	int step = 1024;
	int swappiness = -1;
	int async = 0;
	int prop = 0;
	int max_time = 0;
	int clean = 0;
	int local = 0;
	char *old_buf, *start;
	substring_t args[MAX_OPT_ARGS];
	u64 swapused, anon;
	enum stop_reclaim_reason reason = MEMORY_BK_RECLAIM_NONE;
	struct cgroup *cgroup;
	int ret = 0;
	char path[64] = {0};

	if (!zram_memcg_compress_enable()) {
		ZSWAPD_DEBUG(ZSD_INFO, "memcg compact disabled\n");
		return nbytes;
	}

	abort_memcg_mgasync(memcg);
	mem_cgroup_set_reclaim_abort(memcg, false);
	ZSWAPD_DEBUG(ZSD_INFO, "force_reclaim pid %d\n", current->pid);

	buf = strstrip(buf);

	old_buf = buf;
	nr_to_reclaim = memparse(buf, &buf) / PAGE_SIZE;
	if (buf == old_buf) {
		ret = -EINVAL;
		goto OUT;
	}
	buf = strstrip(buf);

	while ((start = strsep(&buf, " ")) != NULL) {
		if (!strlen(start))
			continue;
		switch (match_token(start, tokens, args)) {
			case MEMORY_RECLAIM_SWAPPINESS:
			{
				if (match_int(&args[0], &swappiness) ||
					((swappiness < MIN_SWAPPINESS || swappiness > MAX_SWAPPINESS) &&
					swappiness != ANON_SWAPPINESS && swappiness != FILE_SWAPPINESS)) {
					ret = -EINVAL;
				}
				break;
			}
			case MEMORY_RECLAIM_STEP:
			{
				if (match_int(&args[0], &step) || (step < (int) SWAP_CLUSTER_MAX)) {
					ret = -EINVAL;
				}
				break;
			}
			case MEMORY_RECLAIM_ASYNC:
			{
				if (match_int(&args[0], &async) || (async != 0 && async != 1)) {
					ret = -EINVAL;
					goto OUT;
				}
				break;
			}
			case MEMORY_RECLAIM_PROP:
			{
				if (match_int(&args[0], &prop) || (prop < 0 || prop > 100)) {
					ret = -EINVAL;
					goto OUT;
				}
				break;
			}
			case MEMORY_RECLAIM_TIME:
			{
				if (match_int(&args[0], &max_time) || (max_time < 0 || max_time > MEMCG_COMPRESS_MAX_TIME)) {
					ret = -EINVAL;
					goto OUT;
				}
				break;
			}
			case MEMORY_RECLAIM_CLEAN:
			{
				if (match_int(&args[0], &clean)) {
					ret = -EINVAL;
					goto OUT;
				}

				if (clean && memcg != root_mem_cgroup) {
					ret = -EINVAL;
					goto OUT;
				}
				break;
			}
			case MEMORY_RECLAIM_LOCAL:
			{
				if (match_int(&args[0], &local)) {
					ret = -EINVAL;
					goto OUT;
				}
				break;
			}
			default:
				goto OUT;
		}
	}

	/* local reclaim only support anon or file reclaim */
	if (local && !(swappiness == ANON_SWAPPINESS || swappiness == FILE_SWAPPINESS))
		return -EINVAL;

OUT:
	if (ret < 0) {
		// MEMCG_ESTRACK_FIELD(memcg, reclaim_brk_reason) = ret;
		// MEMCG_ESTRACK_FIELD(memcg, reclaim_oneshot) = 0;
		mem_cgroup_set_reclaim_brk_reason(memcg, ret);
		mem_cgroup_set_reclaim_oneshot(memcg, 0);
		return nbytes;
	}

	/* for internal test, clean up offlined memcgs */
	if (clean) {
		memcg = mem_cgroup_iter_internal(NULL, NULL, NULL);
		do {
			if (memcg == root_mem_cgroup || memcg->id.id != 0)
				continue;

			memcg_force_reclaim_anon_or_file(memcg, MB_TO_PAGES(200), LRU_GEN_ANON, 0);
			memcg_force_reclaim_anon_or_file(memcg, MB_TO_PAGES(200), LRU_GEN_FILE, 0);
		} while ((memcg = mem_cgroup_iter_internal(NULL, memcg, NULL)));
		return nbytes;
	}

	/* for async mode, will return immediately, push work into zswapd */
	if (async) {
		struct mgasync *mga;
		mga = alloc_init_mgasync_reclaim(memcg, nr_to_reclaim,
				(unsigned long)step, swappiness, max_time, local);
		if (mga == NULL) {
			pr_err("%s: get mgasync failed\n", __func__);
			return -EINVAL;
		}

		enqueue_mgasync(mga);
		return nbytes;
	}

	if (!nr_to_reclaim || !prop || !max_time || (swappiness < 0)) {
		ZSWAPD_DEBUG(ZSD_WARN, "params must be set\n");
		return nbytes;
	}

	last_time = jiffies;
	while (nr_reclaimed < nr_to_reclaim) {
		/* Will converge on zero, but reclaim enforces a minimum */
		unsigned long batch_size = (nr_to_reclaim - nr_reclaimed) / 4;
		unsigned long reclaimed;
		unsigned long now;

		if (signal_pending(current)) {
			reason = MEMORY_BK_RECLAIM_SIGNAL;
			break;
		}

		if (mem_cgroup_get_reclaim_abort(memcg)) {
			reason = MEMORY_BK_RECLAIM_ABORT;
			break;
		}

		swapused = page_counter_read(&memcg->swap);
		anon = memcg_pages(memcg, NR_INACTIVE_ANON) + memcg_pages(memcg, NR_ACTIVE_ANON);
		if (swapused * 100 > prop * (swapused + anon) ) {
			ZSWAPD_DEBUG(ZSD_WARN, "anon %llu KB swap %llu KB\n", PAGES_TO_KB(anon), PAGES_TO_KB(swapused));
			reason = MEMORY_BK_RECLAIM_PROP;
			break;
		}

		/* timeout, abort */
		now = jiffies;
		if (max_time && jiffies_to_msecs(now - last_time) > max_time) {
			ZSWAPD_DEBUG(ZSD_WARN, "time %u %lu %u\n", jiffies_to_msecs(now - last_time),
				nr_reclaimed, nr_retries);
			reason = MEMORY_BK_RECLAIM_TIME;
			break;
		}

		/*
		 * step configurable, default to 1024 pages, that is 4MB every time
		 * minimal SWAP_CLUSTER_MAX, ie. 32 pages.
		 */
		if (batch_size > (unsigned long) step)
			batch_size = (unsigned long) step;

		if (batch_size < SWAP_CLUSTER_MAX)
			batch_size = SWAP_CLUSTER_MAX; 

		/*
		 * This is the final attempt, drain percpu lru caches in the
		 * hope of introducing more evictable pages for
		 * try_to_free_mem_cgroup_pages().
		 */
		// if (!nr_retries)
		//	lru_add_drain_all();

		if (swappiness == ANON_SWAPPINESS || swappiness == FILE_SWAPPINESS)
			reclaimed = memcg_force_reclaim_anon_or_file(memcg, batch_size,
					swappiness == ANON_SWAPPINESS ? LRU_GEN_ANON : LRU_GEN_FILE, !!local);
		else
			reclaimed = memcg_force_reclaim(memcg, batch_size, 
					swappiness == -1 ? NULL : &swappiness);

		ZSWAPD_DEBUG(ZSD_INFO, "time %u, retries %u, reclaimed %lu, "
			"tot_reclaimed %lu, tot_to_reclaime %lu\n",
			jiffies_to_msecs(jiffies - last_time),
			nr_retries, reclaimed, nr_reclaimed, nr_to_reclaim);

		if (0 == reclaimed && !--nr_retries) {
			ZSWAPD_DEBUG(ZSD_WARN, "reclaimed nothing %lu retries %u\n", reclaimed, nr_retries);
			break;
		}
		nr_reclaimed += reclaimed;
		if (nr_reclaimed > nr_to_reclaim) {
			ZSWAPD_DEBUG(ZSD_WARN, "reclaimed %lu overflow %lu retries %u\n", reclaimed, nr_to_reclaim, nr_retries);
			reason = MEMORY_BK_RECLAIM_SIZE;
		}
	}

	cgroup = memcg->css.cgroup;
	if (cgroup) {
		ret = kernfs_path_from_node(cgroup->kn, NULL, path, 64);
	}

	ZSWAPD_DEBUG(ZSD_INFO, "%s reclaim %lu KB anon %lu swap %lu reason %s\n",
		(ret>0)?path:"err", PAGES_TO_KB(nr_reclaimed),
		PAGES_TO_KB(memcg_pages(memcg, NR_INACTIVE_ANON) + memcg_pages(memcg, NR_ACTIVE_ANON)),
		PAGES_TO_KB(page_counter_read(&memcg->swap)), reason_string[reason]);

	// MEMCG_ESTRACK_FIELD(memcg, reclaim_brk_reason) = ((reason > 0)?(MEMORY_BK_RECLAIM_OFFSET - reason):(reason));
	// MEMCG_ESTRACK_FIELD(memcg, reclaim_oneshot) = PAGES_TO_KB(nr_reclaimed);
	mem_cgroup_set_reclaim_brk_reason(memcg, ((reason > 0)?(MEMORY_BK_RECLAIM_OFFSET - reason):(reason)));
	mem_cgroup_set_reclaim_oneshot(memcg, PAGES_TO_KB(nr_reclaimed));

	if (swappiness == ANON_SWAPPINESS) {
		atomic64_inc(&memrec_stat_v1.total_cnt);
		atomic64_add(jiffies_to_msecs(jiffies - last_time), &memrec_stat_v1.reclaimed_delay);
		if (reason != MEMORY_BK_RECLAIM_NONE)
			atomic64_inc(&memrec_stat_v1.abort_reason_cnt[reason]);

		if (nr_reclaimed)
			atomic64_add(nr_reclaimed, &memrec_stat_v1.reclaimed_pages);
		else
			atomic64_inc(&memrec_stat_v1.fail_cnt);
	}
	return nbytes;
}

static ssize_t mem_cgroup_smart_reclaim_anon(struct kernfs_open_file *of,
		char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned int nr_retries = MAX_RECLAIM_RETRIES;
	unsigned long nr_to_reclaim = 0, nr_reclaimed = 0;
	unsigned long last_time;
	int step = 1024;
	int prop = 0;
	int local = 0;
	int max_time = 0;
	int flush = 0;
	enum stop_reclaim_reason reason = MEMORY_BK_RECLAIM_NONE;
	char *old_buf, *start;
	substring_t args[MAX_OPT_ARGS];
	struct cgroup *cgroup;
	int ret = 0;
	char path[64] = {0};
	u64 swapused, anon;
	unsigned long inactive_anon, active_anon, inactive_file, active_file;

	if (!zram_memcg_compress_enable()) {
		ZSWAPD_DEBUG(ZSD_INFO, "memcg compact disabled\n");
		return nbytes;
	}

	mem_cgroup_set_reclaim_abort(memcg, false);

	buf = strstrip(buf);

	old_buf = buf;
	nr_to_reclaim = memparse(buf, &buf) / PAGE_SIZE;
	if (buf == old_buf) {
		ZSWAPD_DEBUG(ZSD_WARN, "same buff err\n");
		return -EINVAL;
	}
	buf = strstrip(buf);

	while ((start = strsep(&buf, " ")) != NULL) {
		if (!strlen(start))
			continue;
		switch (match_token(start, tokens, args)) {
			case MEMORY_RECLAIM_STEP:
			{
				if (match_int(&args[0], &step) ||
					(step < (int)SWAP_CLUSTER_MAX) || (step > 64*(int)SWAP_CLUSTER_MAX)) {
					ZSWAPD_DEBUG(ZSD_WARN, "step %d err\n", step);
					ret = -EINVAL;
				}
				break;
			}
			case MEMORY_RECLAIM_PROP:
			{
				if (match_int(&args[0], &prop) || (prop < 0 || prop > 100)) {
					ZSWAPD_DEBUG(ZSD_WARN, "prop %d err\n", prop);
					ret = -EINVAL;
					goto OUT;
				}
				break;
			}
			case MEMORY_RECLAIM_TIME:
			{
				if (match_int(&args[0], &max_time) || (max_time < 0 || max_time > MEMCG_COMPRESS_MAX_TIME)) {
					ZSWAPD_DEBUG(ZSD_WARN, "time %d err\n", max_time);
					ret = -EINVAL;
					goto OUT;
				}
				break;
			}
			case MEMORY_RECLAIM_FLUSH:
			{
				if (match_int(&args[0], &flush) || (flush != 0 && flush != 1)) {
					ZSWAPD_DEBUG(ZSD_WARN, "flush %d err\n", flush);
					ret = -EINVAL;
					goto OUT;
				}
				break;
			}
			case MEMORY_RECLAIM_LOCAL:
			{
				if (match_int(&args[0], &local)) {
					ret = -EINVAL;
					goto OUT;
				}
				break;
			}
			default:
				goto OUT;
		}
	}

OUT:
	if (ret < 0) {
		// MEMCG_ESTRACK_FIELD(memcg, reclaim_brk_reason) = ret;
		// MEMCG_ESTRACK_FIELD(memcg, reclaim_oneshot) = 0;
		mem_cgroup_set_reclaim_brk_reason(memcg, ret);
		mem_cgroup_set_reclaim_oneshot(memcg, 0);
		return nbytes;
	}

	if (!nr_to_reclaim || !prop || !max_time) {
		ZSWAPD_DEBUG(ZSD_WARN, "params must be set\n");
		return nbytes;
	}

	/*flush data*/
	if (!!flush && mem_cgroup_flush_stats_addr) {
		mem_cgroup_flush_stats_addr(memcg);
	}
	swapused = page_counter_read(&memcg->swap);
	inactive_anon = memcg_pages(memcg, NR_INACTIVE_ANON);
	active_anon = memcg_pages(memcg, NR_ACTIVE_ANON);
	inactive_file = memcg_pages(memcg, NR_INACTIVE_FILE);
	active_file = memcg_pages(memcg, NR_ACTIVE_FILE);

	ZSWAPD_DEBUG(ZSD_INFO, "pid %d comm %s flush %d [swu %llu anon %lu file %lu i_anon %lu a_anon %lu i_file %lu a_file %lu]\n",
		current->pid, current->group_leader->comm, flush,
		PAGES_TO_KB(swapused),
		PAGES_TO_KB(inactive_anon+active_anon),
		PAGES_TO_KB(inactive_file+active_file),
		PAGES_TO_KB(inactive_anon),
		PAGES_TO_KB(active_anon),
		PAGES_TO_KB(inactive_file),
		PAGES_TO_KB(active_file));

	last_time = jiffies;
	while (nr_reclaimed < nr_to_reclaim) {
		/* Will converge on zero, but reclaim enforces a minimum */
		unsigned long batch_size = (nr_to_reclaim - nr_reclaimed) / 4;
		unsigned long reclaimed;
		unsigned long now;

		if (signal_pending(current)) {
			reason = MEMORY_BK_RECLAIM_SIGNAL;
			break;
		}

		if (mem_cgroup_get_reclaim_abort(memcg)) {
			ZSWAPD_DEBUG(ZSD_WARN, "aborted\n");
			reason = MEMORY_BK_RECLAIM_ABORT;
			break;
		}

		swapused = page_counter_read(&memcg->swap);
		anon = memcg_pages(memcg, NR_INACTIVE_ANON) + memcg_pages(memcg, NR_ACTIVE_ANON);
		if (swapused * 100 > prop * (swapused + anon) ) {
			ZSWAPD_DEBUG(ZSD_WARN, "%s: anon %llu KB swap %llu KB\n", __func__, PAGES_TO_KB(anon), PAGES_TO_KB(swapused));
			reason = MEMORY_BK_RECLAIM_PROP;
			break;
		}

		/* timeout, abort */
		now = jiffies;
		if (max_time && jiffies_to_msecs(now - last_time) > (unsigned int)max_time) {
			ZSWAPD_DEBUG(ZSD_WARN, "timeout %u, tot_reclaimed %lu, retries %u\n",
				jiffies_to_msecs(now - last_time), nr_reclaimed, nr_retries);
			reason = MEMORY_BK_RECLAIM_TIME;
			break;
		}

		/*
		 * step configurable, default to 1024 pages, that is 4MB every time
		 * minimal SWAP_CLUSTER_MAX, ie. 32 pages.
		 */
		if (batch_size > (unsigned long) step)
			batch_size = (unsigned long) step;

		if (batch_size < SWAP_CLUSTER_MAX)
			batch_size = SWAP_CLUSTER_MAX; 

		/*
		 * This is the final attempt, drain percpu lru caches in the
		 * hope of introducing more evictable pages for
		 * try_to_free_mem_cgroup_pages().
		 */
		// if (!nr_retries)
		//	lru_add_drain_all();

		reclaimed = memcg_force_reclaim_anon_or_file(memcg, batch_size, LRU_GEN_ANON, !!local);
		ZSWAPD_DEBUG(ZSD_INFO, "time %u, retries %u, reclaimed %lu, "
			"tot_reclaimed %lu, tot_to_reclaime %lu\n",
			jiffies_to_msecs(jiffies - last_time),
			nr_retries, reclaimed, nr_reclaimed, nr_to_reclaim);

		if (0 == reclaimed && !--nr_retries) {
			ZSWAPD_DEBUG(ZSD_WARN, "reclaimed nothing %lu retries %u\n", reclaimed, nr_retries);
			break;
		}
		nr_reclaimed += reclaimed;
		if (nr_reclaimed > nr_to_reclaim) {
			ZSWAPD_DEBUG(ZSD_WARN, "reclaimed %lu overflow %lu retries %u\n", reclaimed, nr_to_reclaim, nr_retries);
			reason = MEMORY_BK_RECLAIM_SIZE;
			break;
		}
	}

	cgroup = memcg->css.cgroup;
	if (cgroup) {
		ret = kernfs_path_from_node(cgroup->kn, NULL, path, 64);
	}

	/*flush data*/
	if (!!flush && mem_cgroup_flush_stats_addr) {
		mem_cgroup_flush_stats_addr(memcg);
		ZSWAPD_DEBUG(ZSD_INFO, "flush data\n");
	}

	swapused = page_counter_read(&memcg->swap);
	inactive_anon = memcg_pages(memcg, NR_INACTIVE_ANON);
	active_anon = memcg_pages(memcg, NR_ACTIVE_ANON);
	inactive_file = memcg_pages(memcg, NR_INACTIVE_FILE);
	active_file = memcg_pages(memcg, NR_ACTIVE_FILE);
	ZSWAPD_DEBUG(ZSD_INFO, "%s cprs %lu KB flush %d"
						  "[swu %llu anon %lu file %lu i_anon %lu a_anon %lu i_file %lu a_file %lu] reason %s\n",
		(ret>0)?path:"err", PAGES_TO_KB(nr_reclaimed), !!flush,
		PAGES_TO_KB(swapused),
		PAGES_TO_KB(inactive_anon+active_anon),
		PAGES_TO_KB(inactive_file+active_file),
		PAGES_TO_KB(inactive_anon),
		PAGES_TO_KB(active_anon),
		PAGES_TO_KB(inactive_file),
		PAGES_TO_KB(active_file),
		reason_string[reason]);

	// MEMCG_ESTRACK_FIELD(memcg, reclaim_brk_reason) = ((reason > 0)?(MEMORY_BK_RECLAIM_OFFSET - reason):(reason));
	// MEMCG_ESTRACK_FIELD(memcg, reclaim_oneshot) = PAGES_TO_KB(nr_reclaimed);
	mem_cgroup_set_reclaim_brk_reason(memcg, ((reason > 0)?(MEMORY_BK_RECLAIM_OFFSET - reason):(reason)));
	mem_cgroup_set_reclaim_oneshot(memcg, PAGES_TO_KB(nr_reclaimed));

	atomic64_inc(&memrec_stat_v1.total_cnt);
	atomic64_add(jiffies_to_msecs(jiffies - last_time), &memrec_stat_v1.reclaimed_delay);
	if (reason != MEMORY_BK_RECLAIM_NONE)
		atomic64_inc(&memrec_stat_v1.abort_reason_cnt[reason]);

	if (nr_reclaimed)
		atomic64_add(nr_reclaimed, &memrec_stat_v1.reclaimed_pages);
	else
		atomic64_inc(&memrec_stat_v1.fail_cnt);

	return nbytes;
}

static int mem_cgroup_smart_reclaim_abort_write(struct cgroup_subsys_state *css,
		struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	bool abort = !!val;

	if (!zram_memcg_compress_enable()) {
		ZSWAPD_DEBUG(ZSD_INFO, "memcg compact disabled\n");
		return 0;
	}

	if (abort) {
		abort_memcg_mgasync(memcg);
	}
	mem_cgroup_set_reclaim_abort(memcg, abort);
	return 0;
}

static u64 mem_cgroup_smart_reclaim_abort_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	return (u64) mem_cgroup_get_reclaim_abort(memcg);
}

static int mem_cgroup_meminfo_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(seq_css(m));
	unsigned long inactive_anon, active_anon, inactive_file, active_file, swapused;

	inactive_anon = memcg_pages(memcg, NR_INACTIVE_ANON);
	active_anon = memcg_pages(memcg, NR_ACTIVE_ANON);
	inactive_file = memcg_pages(memcg, NR_INACTIVE_FILE);
	active_file = memcg_pages(memcg, NR_ACTIVE_FILE);

	swapused = page_counter_read(&memcg->swap);
	seq_printf(m, "%lu ", PAGES_TO_KB(swapused));
	seq_printf(m, "%lu ", PAGES_TO_KB(inactive_anon + active_anon));
	seq_printf(m, "%lu ", PAGES_TO_KB(inactive_file + active_file));
	seq_printf(m, "%lu ", PAGES_TO_KB(inactive_anon));
	seq_printf(m, "%lu ", PAGES_TO_KB(active_anon));
	seq_printf(m, "%lu ", PAGES_TO_KB(inactive_file));
	seq_printf(m, "%lu\n", PAGES_TO_KB(active_file));

	return 0;
}

static int mem_cgroup_lru_pages_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *target_memcg = mem_cgroup_from_css(seq_css(m));
	struct mem_cgroup *memcg;
	struct memcg_estrack *est = mem_cgroup_estrack(target_memcg);
	unsigned long act_anon, inact_anon, act_file, inact_file;

	seq_printf(m, "id  refs  anon(inactive)  anon(active)  file(inactive)  file(active)\n");
	inact_anon = memcg_pages(target_memcg, NR_INACTIVE_ANON);
	act_anon = memcg_pages(target_memcg, NR_ACTIVE_ANON);
	inact_file = memcg_pages(target_memcg, NR_INACTIVE_FILE);
	act_file = memcg_pages(target_memcg, NR_ACTIVE_FILE);
	seq_printf(m, "%4d %8d %8lu %8lu %8lu %8lu \t%s (total)\n", target_memcg->id.id,
		atomic_read(&target_memcg->id.ref.refs), inact_anon, act_anon, inact_file, act_file,
		est ? est->name : "NA");

	memcg = mem_cgroup_iter_internal(target_memcg, NULL, NULL);
	do {
		char buf[ESWAP_MEM_CGROUP_NAME_MAX_LEN];
		char *myname = NULL;

		/* get child memcg path name */
		if (memcg != target_memcg) {
			myname = extract_sub_memcg_name(target_memcg, memcg,
					&buf[0], ESWAP_MEM_CGROUP_NAME_MAX_LEN);
		}

		inact_anon = memcg_pages_local(memcg, NR_INACTIVE_ANON);
		act_anon = memcg_pages_local(memcg, NR_ACTIVE_ANON);
		inact_file = memcg_pages_local(memcg, NR_INACTIVE_FILE);
		act_file = memcg_pages_local(memcg, NR_ACTIVE_FILE);

		seq_printf(m, "%4d %8d %8lu %8lu %8lu %8lu\t%s\n", memcg->id.id,
			atomic_read(&memcg->id.ref.refs), inact_anon, act_anon, inact_file, act_file,
			myname ? myname: "NA");
	} while ((memcg = mem_cgroup_iter_internal(target_memcg, memcg, NULL)) != NULL);

	return 0;
}

static ssize_t zswapd_avail_params_write(struct kernfs_open_file *of,
			char *buf, size_t nbytes, loff_t off)
{
	int avail, min, high, free_swap_threshold_value;

	buf = strstrip(buf);
	if (sscanf(buf, "%d %d %d %d", &avail, &min, &high, &free_swap_threshold_value) != 4) {
		return -EINVAL;
	}

	if (avail < 0 || min < 0 || high < 0 || free_swap_threshold_value < 0) {
		return -EINVAL;
	}

	atomic_set(&mid_avail, avail);
	atomic_set(&min_avail, min);
	atomic_set(&high_avail, high);
	atomic_set(&free_swap_th, free_swap_threshold_value);

#ifdef CONFIG_ESWAP
	if (atomic_read(&min_avail) == 0)
		snapshotd_enable(0);
	else
		snapshotd_enable(1);

	wake_all_zswapds();
#endif

	return nbytes;
}

static int zswapd_avail_params_show(struct seq_file *m, void *v)
{
	seq_printf(m, "mid available: %u\n", atomic_read(&mid_avail));
	seq_printf(m, "min available: %u\n", atomic_read(&min_avail));
	seq_printf(m, "high available: %u\n", atomic_read(&high_avail));
	seq_printf(m, "free_swap_threshold: %d\n", atomic_read(&free_swap_th));

	return 0;
}

static ssize_t zswapd_max_reclaim_size_write(struct kernfs_open_file *of,
				char *buf, size_t nbytes, loff_t off)
{
	int val, ret;

	buf = strstrip(buf);
	ret = kstrtoint(buf, 10u, &val);
	if (ret || val < 0) {
		return -EINVAL;
	}

	atomic_set(&max_reclaim_size, val);

	return nbytes;
}

static int zswapd_max_reclaim_size_show(struct seq_file *m, void *v)
{
	seq_printf(m, "zswapd_max_reclaim_size: %u\n", atomic_read(&max_reclaim_size));
	return 0;
}

static ssize_t zswapd_tune_params_write(struct kernfs_open_file *of,
			char *buf, size_t nbytes, loff_t off)
{
	long __skip_interval_ms, __max_skip_interval_ms, __empty_round, __anon_refault_th;

	buf = strstrip(buf);
	if (sscanf(buf, "%ld %ld %ld %ld", &__skip_interval_ms, &__max_skip_interval_ms,
			&__empty_round, &__anon_refault_th) != 4) {
		return -EINVAL;
	}

	if (__skip_interval_ms < 0 || __max_skip_interval_ms < 0 ||
			__empty_round < 0 || __anon_refault_th < 0) {
		return -EINVAL;
	}

	atomic64_set(&default_skip_interval_ms, __skip_interval_ms);
	atomic64_set(&max_skip_interval_ms, __max_skip_interval_ms);
	atomic64_set(&empty_round_threshold_pages, __empty_round);
	atomic64_set(&area_anon_refault_threshold, __anon_refault_th);

	return nbytes;
}

static int zswapd_tune_params_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%lld %lld %lld %lld\n",
		atomic64_read(&default_skip_interval_ms),
		atomic64_read(&max_skip_interval_ms),
		atomic64_read(&empty_round_threshold_pages),
		atomic64_read(&area_anon_refault_threshold));

	return 0;
}

static ssize_t zswapd_buffers_ratio_params_write(struct kernfs_open_file *of,
				char *buf, size_t nbytes, loff_t off)
{
	unsigned int inactive_file_ratio_value;
	unsigned int active_file_ratio_value;

	buf = strstrip(buf);

	if (sscanf(buf, "%u %u", &inactive_file_ratio_value, &active_file_ratio_value) != 2) {
		return -EINVAL;
	}

	if (inactive_file_ratio_value > 100 || active_file_ratio_value > 100) {
		return -EINVAL;
	}

	atomic_set(&inactive_file_ratio, inactive_file_ratio_value);
	atomic_set(&active_file_ratio, active_file_ratio_value);
	return nbytes;
}

static int zswapd_buffers_ratio_params_show(struct seq_file *m, void *v)
{
	seq_printf(m, "inactive_file_ratio: %d\n", atomic_read(&inactive_file_ratio));
	seq_printf(m, "active_file_ratio: %d\n", atomic_read(&active_file_ratio));
	return 0;
}

static ssize_t mem_cgroup_zswapd_skip_reclaim_anon_write(struct kernfs_open_file *of,
		char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(memcg);
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	est->skip_reclaim_anon = !!val;
	return nbytes;
}

static int mem_cgroup_zswapd_skip_reclaim_anon_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(seq_css(m));
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(memcg);

	seq_printf(m, "%d\n", est->skip_reclaim_anon);
	return 0;
}

#ifdef CONFIG_ESWAP
static int zswapd_snapshot_min_interval_ms_write(struct cgroup_subsys_state *css, struct cftype *cft, u64 val)
{
	snapshot_set_min_interval_ms(val);
	return 0;
}

static u64 zswapd_snapshot_min_interval_ms_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	return snapshot_get_min_interval_ms();
}
#endif

static int zswapd_enable_shrink_caches_write(struct cgroup_subsys_state *css, struct cftype *cft, u64 val)
{
	enable_zswapd_shrink_caches = !!val;
	return 0;
}

static u64 zswapd_enable_shrink_caches_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	return enable_zswapd_shrink_caches;
}

static int zswapd_enable_reclaim_write(struct cgroup_subsys_state *css, struct cftype *cft, u64 val)
{
	enable_zswapd_reclaim = !!val;
	return 0;
}

static u64 zswapd_enable_reclaim_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	return enable_zswapd_reclaim;
}

static int zswapd_debug_write(struct cgroup_subsys_state *css, struct cftype *cft, u64 val)
{
	int lvl = (int) val;
	if (lvl >= ZSD_DEBUG)
		lvl = ZSD_DEBUG;

	if (lvl < ZSD_ERR)
		return lvl = ZSD_ERR;

	zswapd_debug = lvl;
	return 0;
}

static u64 zswapd_debug_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	return zswapd_debug;
}

static ssize_t zswapd_pressure_event_control(struct kernfs_open_file *of,
				char *buf, size_t nbytes, loff_t off)
{
	int efd, level;
	struct eventfd_ctx *ectx, *old;

	buf = strstrip(buf);
	if (sscanf(buf, "%d %d", &efd, &level) != 2)
		return -EINVAL;

	if (level >= ZLEVEL_COUNT)
		return -EINVAL;

	ectx = eventfd_ctx_fdget(efd);
	if (IS_ERR(ectx)) {
		return PTR_ERR(ectx);
	}
	
	/* why need to replace?
	 * considering the case when the user space manager restart
	 */
	mutex_lock(&zswapd_pectx_mutex);
	old = zswapd_pectx[level];
	zswapd_pectx[level] = ectx;
	mutex_unlock(&zswapd_pectx_mutex);

	if (old) {
		eventfd_ctx_put(old);
	}
	return nbytes;
}

void zswapd_pressure_report(enum zswapd_pressure_level level)
{
#if KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE
	mutex_lock(&zswapd_pectx_mutex);
	if (zswapd_pectx[level]) {
		eventfd_signal(zswapd_pectx[level]);
	}
	mutex_unlock(&zswapd_pectx_mutex);
#else
	int ret = 0;

	mutex_lock(&zswapd_pectx_mutex);
	if (zswapd_pectx[level]) {
		ret = eventfd_signal(zswapd_pectx[level], 1);
	}
	mutex_unlock(&zswapd_pectx_mutex);

	if (ret < 0) {
		pr_err("SWAP-MM: %s : level:%u, ret:%d ", __func__, level, ret);
	}
#endif
}

static int zswapd_pids_show(struct seq_file *m, void *v)
{
	int nid;

	for_each_online_node(nid) {
		pid_t pid = get_zswapd_pid(nid);
		if (pid != -1) {
			seq_printf(m, "%d\n", pid);
		}
	}
	return 0;
}

static int zswapd_zram_wm_percent_write(struct cgroup_subsys_state *css, struct cftype *cft, u64 val)
{
	if (val > 100)
		return -EINVAL;

	atomic_set(&zram_wm_percent, val);
	return 0;
}

static u64 zswapd_zram_wm_percent_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	return atomic_read(&zram_wm_percent);
}

static int zswapd_zram_comp_percent_write(struct cgroup_subsys_state *css, struct cftype *cft, u64 val)
{
	if (val > 100)
		return -EINVAL;

	atomic_set(&zram_comp_percent, val);
	return 0;
}

static u64 zswapd_zram_comp_percent_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	return atomic_read(&zram_comp_percent);
}

static int active_app_info_list_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = NULL;
	unsigned long anon_size, zram_size, eswap_size;
	unsigned long anon_active, anon_inactive, file_active, file_inactive;

	seq_printf(m, "id id-refs score  anon(kb)  zram(kb)  eswap(kb)  refault"
			"  anon-inact(pgs)  anon-act(pgs)  file-inact(pgs)  file_act(pgs)"
			"  name\n");
	while ((memcg = get_next_memcg(memcg))) {
		struct memcg_estrack *est = mem_cgroup_estrack(memcg);
		int score = atomic_read(&est->app_score);

		if (memcg == root_mem_cgroup) {
			anon_active = memcg_pages_local(memcg, NR_ACTIVE_ANON);
			anon_inactive = memcg_pages_local(memcg, NR_INACTIVE_ANON);
			file_active = memcg_pages_local(memcg, NR_ACTIVE_FILE);
			file_inactive = memcg_pages_local(memcg, NR_INACTIVE_FILE);
		} else {
			anon_active = memcg_pages(memcg, NR_ACTIVE_ANON);
			anon_inactive = memcg_pages(memcg, NR_INACTIVE_ANON);
			file_active = memcg_pages(memcg, NR_ACTIVE_FILE);
			file_inactive = memcg_pages(memcg, NR_INACTIVE_FILE);
		}
		anon_size = anon_active + anon_inactive;

#ifdef CONFIG_ESWAP
		eswap_size = (unsigned long)atomic_read(&est->eswap_stored_pages);
		zram_size = (unsigned long)atomic_read(&est->zram_page_size);
#else
		eswap_size = 0;
		zram_size = 0;
#endif

		/*
		if (anon_size + zram_size + eswap_size == 0)
			continue;
	
		if (est->name[0] == '\0')
			continue;
		*/

		anon_size *= PAGE_SIZE / SZ_1K;
		zram_size *= PAGE_SIZE / SZ_1K;
		eswap_size *= PAGE_SIZE / SZ_1K;

		seq_printf(m, "%8d %8d %8d %8lu %8lu %8lu %8llu %8lu %8lu %8lu %8lu %s\n",
			memcg->id.id, atomic_read(&memcg->id.ref.refs), score,
			anon_size, zram_size, eswap_size, est->anon_rfs,
			anon_inactive, anon_active, file_inactive, file_active,
			(est->name[0] == '\0' ? "<none>" : est->name));
	}
	return 0;
}

static ssize_t mem_cgroup_zswapd_param_write(struct kernfs_open_file *of,
				char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(memcg);
	int mem2zram_ratio, zram2flash_ratio, anon_rf_ratio;

	buf = strstrip(buf);

	if (sscanf(buf, "%d %d %d", &mem2zram_ratio, &zram2flash_ratio, &anon_rf_ratio) != 3)
		return -EINVAL;

	if (mem2zram_ratio > 100 || mem2zram_ratio <0)
		return -EINVAL;

	if (zram2flash_ratio > 100 || zram2flash_ratio <0)
		return -EINVAL;

	atomic_set(&est->mem2zram_ratio, mem2zram_ratio);
	atomic_set(&est->zram2flash_ratio, zram2flash_ratio);
	atomic_set(&est->anon_rf_ratio, anon_rf_ratio);

	return nbytes;
}

static int mem_cgroup_zswapd_param_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(seq_css(m));
	struct memcg_estrack *est = MEMCG_ESTRACK_GUARD(memcg);

	seq_printf(m, "memcg mem2zram_ratio: %d\n", atomic_read(&est->mem2zram_ratio));
	seq_printf(m, "memcg zram2flash_ratio: %d\n", atomic_read(&est->zram2flash_ratio));
	seq_printf(m, "memcg anon_rf_ratio: %d\n", atomic_read(&est->anon_rf_ratio));
	return 0;
}

int get_zram2flash_ratio(struct mem_cgroup *memcg)
{
	struct memcg_estrack *est = mem_cgroup_estrack(memcg);
	if (zram2flash_percent)
		return zram2flash_percent;
	return atomic_read(&est->zram2flash_ratio);
}

#endif // CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT

static struct cftype memcg_policy_files_unused[] = {
	{ },	/* terminate */
};

static struct cftype memcg_policy_files[] = {
#ifdef CONFIG_TRAN_MEMCG_COMPRESS_SUPPORT
	{
		.name = "memfusion.name",
		.flags = CFTYPE_WORLD_WRITABLE,
		.write = mem_cgroup_name_write,
		.seq_show = mem_cgroup_name_show,
	},
	{
		.name = "memfusion.app_value",
		.flags = CFTYPE_WORLD_WRITABLE,
		.write_u64 = mem_cgroup_app_value_write,
		.seq_show = mem_cgroup_app_value_show,
	},
	{
		.name = "memfusion.app_value_list",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = memcg_app_value_list_show,
	},
	{
		.name = "memfusion.active_app_info_list",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = active_app_info_list_show,
	},
	{
		.name = "memfusion.debug",
		.flags = CFTYPE_ONLY_ON_ROOT|CFTYPE_WORLD_WRITABLE,
		.write_u64 = zswapd_debug_write,
		.read_u64 = zswapd_debug_read,
	},
	{
		.name = "memfusion.meminfo",
		.seq_show = mem_cgroup_meminfo_show,
	},
	{
		.name = "memfusion.smart_reclaim",
		.flags = CFTYPE_WORLD_WRITABLE,
		.write = mem_cgroup_smart_reclaim,
	},
	{
		.name = "memfusion.smart_reclaim_anon",
		.flags = CFTYPE_WORLD_WRITABLE,
		.write = mem_cgroup_smart_reclaim_anon,
	},
	{
		.name = "memfusion.smart_reclaim_single_snapshot",
		.seq_show = mem_cgroup_smart_reclaim_single_snapshot,
	},
	{
		.name = "memfusion.smart_reclaim_abort",
		.flags = CFTYPE_WORLD_WRITABLE,
		.write_u64 = mem_cgroup_smart_reclaim_abort_write,
		.read_u64 = mem_cgroup_smart_reclaim_abort_read,
	},
	{
		.name = "memfusion.lru_pages",
		.seq_show = mem_cgroup_lru_pages_show,
	},
	{
		.name = "memfusion.events",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = zswapd_events_show,
	},
	{
		.name = "memfusion.enable_reclaim",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write_u64 = zswapd_enable_reclaim_write,
		.read_u64 = zswapd_enable_reclaim_read,
	},
	{
		.name = "memfusion.enable_shrink_caches",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write_u64 = zswapd_enable_shrink_caches_write,
		.read_u64 = zswapd_enable_shrink_caches_read,
	},
	{
		.name = "memfusion.swap_full_percent",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write_u64 = zswapd_swap_full_percent_write,
		.read_u64 = zswapd_swap_full_percent_read,
	},
	{
		.name = "memfusion.zram_wm_percent",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write_u64 = zswapd_zram_wm_percent_write,
		.read_u64 = zswapd_zram_wm_percent_read,
	},
	{
		.name = "memfusion.zram_comp_percent",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write_u64 = zswapd_zram_comp_percent_write,
		.read_u64 = zswapd_zram_comp_percent_read,
	},
	{
		.name = "memfusion.pressure_control",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write = zswapd_pressure_event_control,
	},
	{
		.name = "memfusion.pids",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = zswapd_pids_show,
	},
	{
		.name = "memfusion.avail_params",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write = zswapd_avail_params_write,
		.seq_show = zswapd_avail_params_show,
	},
	{
		.name = "memfusion.max_reclaim_size",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write = zswapd_max_reclaim_size_write,
		.seq_show = zswapd_max_reclaim_size_show,
	},
	{
		.name = "memfusion.tune_params",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write = zswapd_tune_params_write,
		.seq_show = zswapd_tune_params_show,
	},
	{
		.name = "memfusion.buffer_ratio_params",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write = zswapd_buffers_ratio_params_write,
		.seq_show = zswapd_buffers_ratio_params_show,
	},
	{
		.name = "memfusion.skip_reclaim_anon",
		.write = mem_cgroup_zswapd_skip_reclaim_anon_write,
		.seq_show = mem_cgroup_zswapd_skip_reclaim_anon_show,
	},
	{
		.name = "memfusion.memcg_params",
		.write = mem_cgroup_zswapd_param_write,
		.seq_show = mem_cgroup_zswapd_param_show,
	},
#ifdef CONFIG_ESWAP
	{
		.name = "memfusion.snapshot_min_interval",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write_u64 = zswapd_snapshot_min_interval_ms_write,
		.read_u64 = zswapd_snapshot_min_interval_ms_read,
	},
#endif
#endif
#ifdef CONFIG_ESWAP
	{
		.name = "memfusion.total_info_per_app",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = total_info_per_app_show,
	},
	{
		.name = "memfusion.report_app_info",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = report_app_info_show,
	},
	{
		.name = "memfusion.stat",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = mem_cgroup_eswap_all_stat_show,
	},
	{
		.name = "memfusion.area_stat",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = mem_cgroup_eswap_area_stat_show,
	},
	{
		.name = "memfusion.loglevel",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write_u64 = mem_cgroup_eswap_set_loglevel,
		.read_u64 = mem_cgroup_eswap_get_loglevel,
	},
	{
		.name = "memfusion.acct_detail",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write_u64 = mem_cgroup_eswap_set_acct_detail,
		.read_u64 = mem_cgroup_eswap_get_acct_detail,
	},
	{
		.name = "memfusion.test",
		.write = mem_cgroup_eswap_test,
	},
	{
		.name = "memfusion.memcg_stat",
		.seq_show = mem_cgroup_eswap_stat_show,
	},
	{
		.name = "memfusion.zraminfo",
		.seq_show = mem_cgroup_eswap_zraminfo_show,
	},
	{
		.name = "memfusion.flash2zram_ratio",
		.write = mem_cgroup_eswap_flash2zram_ratio_write,
		.seq_show = mem_cgroup_eswap_flash2zram_ratio_show,
	},
	{
		.name = "memfusion.smart_preload2zram",
		.flags = CFTYPE_WORLD_WRITABLE,
		.write = mem_cgroup_eswap_in_write,
	},
#endif
	{ },	/* terminate */
};

#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
typedef int (*cgroup_add_dfl_cftypes_func)(struct cgroup_subsys *ss, struct cftype *cfts);
static cgroup_add_dfl_cftypes_func cgroup_add_dfl_cftypes_addr = NULL;

static int get_cgroup_add_dfl_cftypes_addr(void)
{
	struct kprobe kprobe_data = {
		.symbol_name = "cgroup_add_dfl_cftypes"
	};

	if (cgroup_add_dfl_cftypes_addr)
		return 0;

	/* get some symbols address using kprobe */
	if (register_kprobe(&kprobe_data)) {
		pr_err("get cgroup_add_dfl_cftypes addr by probe failed!\n");
		return -1;
	}

	cgroup_add_dfl_cftypes_addr = (cgroup_add_dfl_cftypes_func)kprobe_data.addr;
	unregister_kprobe(&kprobe_data);

	if (!cgroup_add_dfl_cftypes_addr) {
		return -1;
	}
	return 0;
}

static int cgroup_add_cftypes(void)
{
	int ret;

	ret = get_cgroup_add_dfl_cftypes_addr();
	if (ret < 0) {
		pr_err("get_cgroup_add_dfl_cftypes_addr failed\n");
		return ret;
	}

	ret = get_mem_cgroup_flush_stats_addr();
	if (ret < 0) {
		pr_err("get memcg flush stats failed\n");
	}

	ret = cgroup_add_dfl_cftypes_addr(&memory_cgrp_subsys, memcg_policy_files);
	if (ret < 0) {
		pr_err("cgroup_add_dfl_cftypes_addr failed\n");
		return ret;
	}

	(void)memcg_policy_files_unused;

	return 0;
}
#else
static int cgroup_add_cftypes(void)
{
	int ret;

	ret = get_mem_cgroup_flush_stats_addr();
	if (ret < 0) {
		pr_err("get memcg flush stats failed\n");
	}

	(void)memcg_policy_files_unused;
	pr_err("cgroup_add_cftypes begin\n");
	return cgroup_add_dfl_cftypes(&memory_cgrp_subsys, memcg_policy_files);
}
#endif

static int compaction_track_init(void)
{
	int err = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
	register_sched_block_reason_hook();
#endif

	timer_setup(&cltrack.timer, compactd_cpuload_track_func, 0);
	spin_lock_init(&cltrack.lock);
	err = register_trace_android_vh_compact_finished(compact_finished, &cltrack);
	if (err) {
		pr_err("register compact_finished failed!\n");
		return -EINVAL;
	}

	err = register_trace_android_vh_mm_compaction_end(compact_end, &cltrack);
	if (err) {
		unregister_trace_android_vh_compact_finished(compact_finished, &cltrack);
		pr_err("register mm_compaction_end failed!\n");
		return -EINVAL;
	}

	cltrack.inited = true;
	return 0;
}

static void compaction_track_exit(void)
{
	unsigned long flags;

	if (!cltrack.inited)
		return;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
	unregister_sched_block_reason_hook();
#endif

	unregister_trace_android_vh_mm_compaction_end(compact_end, &cltrack);
	unregister_trace_android_vh_compact_finished(compact_finished, &cltrack);

	spin_lock_irqsave(&cltrack.lock, flags);
	cltrack.tracking = 0;
	cltrack.seq = 0;
	spin_unlock_irqrestore(&cltrack.lock, flags);

	timer_delete_sync(&cltrack.timer);
}

static int memcg_policy_init(void)
{
	int err = 0;
	int i;

	if (mem_cgroup_disabled())
		return 0;

	for (i = 0; i < SCORE_LIST_NUM; i++)
		INIT_LIST_HEAD(&score_heads[i]);

	err = compaction_track_init();
	if (err)
		return err;

	err = register_trace_android_vh_mem_cgroup_alloc(memcg_estrack_alloc, NULL);
	if (err) {
		pr_err("register memcg_estrack_alloc failed!\n");
		goto err_estrack_alloc;
	}

	err = register_trace_android_vh_mem_cgroup_free(memcg_estrack_free, NULL);
	if (err) {
		pr_err("register memcg_estrack_free failed!\n");
		goto err_estrack_free;
	}

	err = register_trace_android_vh_mem_cgroup_css_offline(memcg_estrack_offline, NULL);
	if (err) {
		pr_err("register_trace_android_vh_mem_cgroup_css_offline failed!\n");
		goto err_estrack_offline;
	}

	err = register_trace_android_vh_mem_cgroup_css_online(memcg_estrack_online, NULL);
	if (err) {
		pr_err("register_trace_android_vh_mem_cgroup_css_online failed!\n");
		goto err_estrack_online;
	}

	err = register_trace_android_vh_mem_cgroup_id_remove(memcg_estrack_id_remove, NULL);
	if (err) {
		pr_err("register_trace_android_vh_mem_cgroup_id_remove failed!\n");
		goto err_estrack_idremove;
	}

	do_traversal_all_lruvec_internal();

	WARN_ON(cgroup_add_cftypes());
	return 0;

err_estrack_idremove:
	unregister_trace_android_vh_mem_cgroup_css_online(memcg_estrack_online, NULL);

err_estrack_online:
	unregister_trace_android_vh_mem_cgroup_css_offline(memcg_estrack_offline, NULL);

err_estrack_offline:
	unregister_trace_android_vh_mem_cgroup_free(memcg_estrack_free, NULL);

err_estrack_free:
	unregister_trace_android_vh_mem_cgroup_alloc(memcg_estrack_alloc, NULL);
		
err_estrack_alloc:
	compaction_track_exit();
	return err;
}

static void memcg_policy_deinit(void)
{
	unregister_trace_android_vh_mem_cgroup_id_remove(memcg_estrack_id_remove, NULL);
	unregister_trace_android_vh_mem_cgroup_css_online(memcg_estrack_online, NULL);
	unregister_trace_android_vh_mem_cgroup_css_offline(memcg_estrack_offline, NULL);
	unregister_trace_android_vh_mem_cgroup_free(memcg_estrack_free, NULL);
	unregister_trace_android_vh_mem_cgroup_alloc(memcg_estrack_alloc, NULL);
	compaction_track_exit();
}

static int mem_eswap_inited = 0;
int mem_eswap_init(void)
{
	int ret = 0;

	if (mem_eswap_inited)
		return 0;

	/* first, bring up zswapd and snapshotd */
	ret = zswapd_module_init();
	if (ret) {
		pr_err("zswapd module init failed\n");
		goto zswapd_module_err;
	}

	ret = eswap_module_init();
	if (ret) {
		pr_err("eswap module init failed\n");
		goto eswap_module_err;
	}

	ret = memcg_policy_init();
	if (ret) {
		pr_err("memcg policy init failed\n");
		goto memcg_policy_err;
	}

	mem_eswap_inited = 1;
	return 0;

memcg_policy_err:
	eswap_module_exit();

eswap_module_err:
	zswapd_module_exit();

zswapd_module_err:
	return ret;
}

void mem_eswap_exit(void)
{
	if (!mem_eswap_inited)
		return;

	memcg_policy_deinit();

	eswap_module_exit();

	zswapd_module_exit();

	mem_eswap_inited = 0;
}

MODULE_IMPORT_NS(MINIDUMP);
