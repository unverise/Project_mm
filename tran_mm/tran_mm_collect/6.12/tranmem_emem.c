/*
 * The enhance meminfo show system memory information, when processes with
 * range of oom_score_adj values will get killed. The killed process
 * oom_score_adj values is written in /proc/sys/vm/emem_trigger. If the
 * written value is less then Threshold, the meminfo is shown.
 *
 * Threshold of killed process adj is set in
 * /sys/module/tran_mm_collect/parameters/......
 */
#include <linux/reciprocal_div.h>
#include <linux/list_lru.h>
#include <linux/local_lock.h>
#include <linux/random.h>
#include <linux/kobject.h>
#include <linux/sched/mm.h>
#include <linux/memcontrol.h>
#include <linux/kfence.h>
#include <linux/kasan.h>
#include <linux/stackdepot.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/signal.h>
#include <linux/mmzone.h>
#include <linux/mm.h>
#include <linux/workqueue.h>
#include <linux/swap.h>
#include <linux/vmalloc.h>
#include <linux/uidgid.h>
#include <linux/kprobes.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/vmstat.h>
#include <linux/mmzone.h>
#include <mm/slab.h>
#include <linux/sched.h>
#include <linux/sched/stat.h>
#include <linux/sched/cputime.h>
#include <linux/sched/types.h>
#include <uapi/linux/sched/types.h>
#include "tranmem_emem.h"
#include "tranmem_task_struct.h"
#include "tranmem_direct_reclaim.h"
#ifdef CONFIG_ION
#include <linux/ion.h>
#endif
#include <trace/hooks/mm.h>
#ifdef CONFIG_CMA
#include <linux/cma.h>
#endif
#include <linux/seq_file.h>
#include <trace/hooks/vmscan.h>
#include <linux/string.h>

#define DEFAULT_PROC_ADJ		900
#define EMEM_SHOW_KILL_ADJ900_INTERVAL	600

#ifdef CONFIG_SPRD_DEBUG
#define EMEM_SHOW_INTERVAL		2
#else
#define EMEM_SHOW_INTERVAL		10
#endif

struct emem_work {
	int adj;
	struct work_struct work_duration;
	struct work_struct work_exception;
};
struct emem_work g_ework;

static struct workqueue_struct *emem_wq = NULL;

typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
// The GKI function address, which is got from it's registed kprobe function and 
// is used to get the addresses of global variables and functions within GKI and outside KMI
static kallsyms_lookup_name_t kallsyms_lookup_name_dup;

#if defined(CONFIG_SLUB_DEBUG)
// The address of global variable slab_caches, which is used to get the information of each slab memory
static struct list_head *slab_caches_symbol = NULL;
// The address of global variable slab_mutex, which is used to get the information of each slab memory
static struct mutex *slab_mutex_symbol = NULL;
#endif

// The address of global variable nr_vmap_nodes, which is used to get the information of each vmalloc memory
static __read_mostly unsigned int *nr_vmap_nodes_symbol = NULL;
// The address of global variable vmap_nodes, which is used to get the information of each vmalloc memory
static struct vmap_node *vmap_nodes_symbol = NULL;

// The address of global variable modules, which is used to get the information of each insmoded modules(driver)
static struct list_head *modules_symbol = NULL;
// The address of global variable modules_mutex, which is used to get the information of each insmoded modules(driver)
static struct mutex *module_mutex_symbol = NULL;

// Function pointer for dynamically resolved mtk_tranmem_print_gpu_mem() via kallsyms
typedef int (*kallsyms_mtk_tranmem_print_gpu_mem_t)(const char *tag, const unsigned int total_threshold);
static kallsyms_mtk_tranmem_print_gpu_mem_t kallsyms_mtk_tranmem_print_gpu_mem = NULL;

#define MAX_COLLECT_LENGTH 17280 /* 8640 * 2 */
#define CPU_USAGE_SCALE 100
#define NSEC_PER_MSEC	1000000L

/* reclaim_info_status control states */
#define RECLAIM_STAT_STOP    0
#define RECLAIM_STAT_START   1
#define RECLAIM_STAT_CLEAR   2

#ifdef CONFIG_CMA
static unsigned long *totalcma_pages_symbol = NULL;
#endif

typedef enum collect_data_type {
	TRANMEM_KSWAPD_CPU_USAGE=0,
	TRANMEM_KCOMPACTD_CPU_USAGE,
	NR_TRANMEM_COLLECT_DATA_TYPE,
} collect_data_t;

static const char* const collect_data_type_str[NR_TRANMEM_COLLECT_DATA_TYPE] = {
	"kswapd_cpu_usage",
	"kcompact_cpu_usage",
};

/* Distribution statistics for CPU usage */
#define DISTRIBUTION_MAX_VALUE 200
#define DISTRIBUTION_INTERVAL 5
#define DISTRIBUTION_BUCKETS ((DISTRIBUTION_MAX_VALUE / DISTRIBUTION_INTERVAL) + 1)

struct distribution_stats {
	int buckets[DISTRIBUTION_BUCKETS];
	int above_threshold_count;
	int valid_count;
	int enabled;
};

struct distribution_stats g_dist_stats[NR_TRANMEM_COLLECT_DATA_TYPE];
DEFINE_MUTEX(g_dist_stats_lock);

static int kswapd_cpu_usage_threshold = 15;  /* Default threshold for kswapd CPU usage */
static int kcompact_cpu_usage_threshold = 15;  /* Default threshold for kcompact CPU usage */
static pid_t g_kswapd_pid = 0;
static pid_t g_kcompactd_pid = 0;
static unsigned long g_physpages = 0;

static int *collect_data_thresholds[NR_TRANMEM_COLLECT_DATA_TYPE] = {
	&kswapd_cpu_usage_threshold,
	&kcompact_cpu_usage_threshold,
};

/* only greather than the threshold will be printed */
// only when the physical memory size is greater than this threshold(KB), the corresponding vmalloc information will be outputted
static int vmalloc_show_threshold_kb = 10240;
module_param_named(vmalloc_show_threshold_kb, vmalloc_show_threshold_kb, int, 0640);
// only when the physical memory size is greater than this threshold(KB), the corresponding slab information will be outputted
static int slab_show_threshold_kb = 10240;
module_param_named(slab_show_threshold_kb, slab_show_threshold_kb, int, 0640);
// only when the physical memory size is greater than this threshold(KB), the corresponding module information will be outputted
static int modules_show_threshold_kb = 1024;
module_param_named(modules_show_threshold_kb, modules_show_threshold_kb, int, 0640);
// only when the dmabuf total memory size is greater than this threshold(KB), the corresponding dmabuf detail information will be outputted
static long dmabuf_show_threshold_kb = 512000;
module_param_named(dmabuf_show_threshold_kb, dmabuf_show_threshold_kb, long, 0640);
// only when the GPU memory size is greater than this threshold(KB), the GPU usage information of the corresponding process will be outputted
static unsigned int gpu_show_threshold_kb = 512000;
module_param_named(gpu_show_threshold_kb, gpu_show_threshold_kb, uint, 0644);

/* File cache thresholds in MB */
//3G-500MB, 4G-650MB, 6G-1GB, 8G-1.5GB, 12G-1.8GB
#define FILECACHE_EXCEPTION_THRESHOLD_3G    500
#define FILECACHE_EXCEPTION_THRESHOLD_4G    650
#define FILECACHE_EXCEPTION_THRESHOLD_6G    1024
#define FILECACHE_EXCEPTION_THRESHOLD_8G    1536
#define FILECACHE_EXCEPTION_THRESHOLD_12G   1843
/* Available memory thresholds in MB */
//3G-500MB, 4G-650MB, 6G-1GB, 8G-1.5GB, 12G-1.8GB
#define AVAILABLE_EXCEPTION_THRESHOLD_3G    500
#define AVAILABLE_EXCEPTION_THRESHOLD_4G    650
#define AVAILABLE_EXCEPTION_THRESHOLD_6G    1024
#define AVAILABLE_EXCEPTION_THRESHOLD_8G    1536
#define AVAILABLE_EXCEPTION_THRESHOLD_12G   1843

// only when the available memory size is smaller than this threshold(MB), the memory status is considered as exception
static long available_exception_threshold_mb = 1843;
module_param_named(available_exception_threshold_mb, available_exception_threshold_mb, long, 0640);

// only when the file memory size is smaller than this threshold(MB), the memory status is considered as exception
static long file_exception_threshold_mb = 1843;
module_param_named(file_exception_threshold_mb, file_exception_threshold_mb, long, 0640);

// only when the RSS+Swap memory size is greater than this threshold(MB), the corresponding process information will be outputted
static int process_show_threshold_mb = 200;
module_param_named(process_show_threshold_mb, process_show_threshold_mb, int, 0640);

static bool symbols_initialized = false;

/* User knob to enable/disable enhance meminfo feature */
static int emem_enable = 1;
module_param_named(emem_enable, emem_enable, int, 0644);

/* killed process oom score adj threshold */
static int adj_threshold = 200;
module_param_named(adj_threshold, adj_threshold, int, 0644);

extern struct sighand_struct *tranmm_lock_task_sighand(struct task_struct *task,
						       unsigned long *flags);
extern bool kernel_log_enable(void);
extern void zram_emem_printk(struct sysinfo *si, int type);
extern void dma_heap_emem_printk(long threshold);

static void init_memory_exception_threshold(void)
{
	int total_gb = (int)((((unsigned long long)totalram_pages() << PAGE_SHIFT) + (1ULL<<30) - 1) >> 30);


	switch (total_gb) {
	case 3:
		available_exception_threshold_mb = AVAILABLE_EXCEPTION_THRESHOLD_3G;
		file_exception_threshold_mb = FILECACHE_EXCEPTION_THRESHOLD_3G;
		break;
	case 4:
		available_exception_threshold_mb = AVAILABLE_EXCEPTION_THRESHOLD_4G;
		file_exception_threshold_mb = FILECACHE_EXCEPTION_THRESHOLD_4G;
		break;
	case 6:
		available_exception_threshold_mb = AVAILABLE_EXCEPTION_THRESHOLD_6G;
		file_exception_threshold_mb = FILECACHE_EXCEPTION_THRESHOLD_6G;
		break;
	case 8:
		available_exception_threshold_mb = AVAILABLE_EXCEPTION_THRESHOLD_8G;
		file_exception_threshold_mb = FILECACHE_EXCEPTION_THRESHOLD_8G;
		break;
	case 12:
		available_exception_threshold_mb = AVAILABLE_EXCEPTION_THRESHOLD_12G;
		file_exception_threshold_mb = FILECACHE_EXCEPTION_THRESHOLD_12G;
		break;
	default:
		break;
	}

	pr_info(TRANMM "Memory thresholds initialized: available=%ld MB, filecache=%ld MB for %dGB RAM\n",
            available_exception_threshold_mb,
            file_exception_threshold_mb,
            total_gb);
}
/*
 * This structure defines a single, solid model where a list and
 * rb-tree are part of one entity protected by the lock. Nodes are
 * sorted in ascending order, thus for O(1) access to left/right
 * neighbors a list is used as well as for sequential traversal.
 */
struct rb_list {
	struct rb_root root;
	struct list_head head;
	spinlock_t lock;
};

/*
 * A fast size storage contains VAs up to 1M size. A pool consists
 * of linked between each other ready to go VAs of certain sizes.
 * An index in the pool-array corresponds to number of pages + 1.
 */
#define MAX_VA_SIZE_PAGES 256

struct vmap_pool {
	struct list_head head;
	unsigned long len;
};

/*
 * An effective vmap-node logic. Users make use of nodes instead
 * of a global heap. It allows to balance an access and mitigate
 * contention.
 */
struct vmap_node {
	/* Simple size segregated storage. */
	struct vmap_pool pool[MAX_VA_SIZE_PAGES];
	spinlock_t pool_lock;
	bool skip_populate;

	/* Bookkeeping data of this node. */
	struct rb_list busy;
	struct rb_list lazy;

	/*
	 * Ready-to-free areas.
	 */
	struct list_head purge_list;
	struct work_struct purge_work;
	unsigned long nr_purged;
};

#ifdef CONFIG_FTRACE
#define TRACE_MSG_LEN 1024
/* User knob to enable/disable trace of enhance meminfo feature */
static int emem_trace_enable = 1;

module_param_named(emem_trace, emem_trace_enable, int, 0644);
static noinline int tracing_mark_write(char *fmt, ...)
{
	char buf[TRACE_MSG_LEN];	va_list args;
	int len;
	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if (len >= TRACE_MSG_LEN) {
		pr_err("%s trace size %u exceed limit", __func__, len);
		return -1;
	}
	trace_puts(buf);
	return 0;
}

void tran_memory_info_kernel_trace_start(char * name){
	if (emem_trace_enable)
		tracing_mark_write("B|%d|%s\n",current->tgid, name);
}

void tran_memory_info_kernel_trace_change(char * tag , int flag){
	if (emem_trace_enable)
		tracing_mark_write("C|%d|%s|%d\n", current->tgid, tag, flag);
}

void tran_memory_info_kernel_trace_end(char * name){
	if (emem_trace_enable)
		tracing_mark_write("E|%d\n", current->tgid);
}
#else
int tracing_mark_write(char *fmt, ...)
{
}
void tran_memory_info_kernel_trace_start(char * name)
{
}
void tran_memory_info_kernel_trace_change(char * tag , int flag)
{
}
void tran_memory_info_kernel_trace_end(char * name)
{
}
#endif

#if defined(CONFIG_SLUB_DEBUG)
/*
 * Output the memory used information of each slab to the kernel log
 * Only when the physical memory usage is greater than slab_show_threshold_kb(KB), the corresponding slab information will be outputted to the kernel log.
 */
static void dump_slabinfo(void){
	struct kmem_cache *s;
	struct slabinfo sinfo;
	unsigned long slab_total = 0;
	unsigned long slab_used_total = 0;

	if (!slab_mutex_symbol || !slab_caches_symbol) {
		pr_err(TRANMM "(%s) slab_mutex(0x%p) or slab_caches address(0x%p) get fail, vmalloc info can not be accessed\n", __func__, slab_mutex_symbol, slab_caches_symbol);
		return;
	}

	tran_memory_info_kernel_trace_start(TRANMM"dump_slabinfo");
	pr_info(TRANMM "Slab info Attention: only greater than %dkB will be printed\n", slab_show_threshold_kb);
	pr_info(TRANMM "%-20s %-8s %-8s\n", "Name", "Used(kB)", "Total(kB)");

	mutex_lock(slab_mutex_symbol);

	list_for_each_entry(s, slab_caches_symbol, list) {
		get_slabinfo(s, &sinfo);
		if (sinfo.num_objs > 0) {
			if (((sinfo.active_objs * s->size) / 1024) >= slab_show_threshold_kb) {
				pr_info(TRANMM "%-20s %-8lu %-8lu\n", s->name, (sinfo.active_objs * s->size) / 1024, (sinfo.num_objs * s->size) / 1024);
				slab_used_total = slab_used_total + (sinfo.active_objs * s->size) / 1024;
				slab_total = slab_total + (sinfo.num_objs * s->size) / 1024;
			}
		}
	}
	pr_info(TRANMM "slab used total:%10luKB\n", slab_used_total);
	pr_info(TRANMM "slab total:%10luKB\n", slab_total);
	mutex_unlock(slab_mutex_symbol);

	tran_memory_info_kernel_trace_end(TRANMM"dump_slabinfo");
	return;
}
#else
static inline void dump_slabinfo(void){
}
#endif

// mm/oom_kill.c find_lock_task_mm() function, don't modify.
static struct task_struct *tran_find_lock_task_mm(struct task_struct *p)
{
	struct task_struct *t;

	rcu_read_lock();

	for_each_thread(p, t) {
		if (spin_trylock(&t->alloc_lock)) {
			if (likely(t->mm))
				goto found;
			spin_unlock(&t->alloc_lock);
		}
	}
	t = NULL;
found:
	rcu_read_unlock();

	return t;
}

extern u64 tranmem_get_fail;

static void dump_tasks_info(int threshold)
{
	struct task_struct *task;
	struct task_struct *p;
	unsigned long flags;
	unsigned long total;

	tran_memory_info_kernel_trace_start(TRANMM"dump_tasks_info");
	pr_info(TRANMM "tranmem_task_struct is not correctly initialized %llu times\n", tranmem_get_fail);
	pr_info(TRANMM "Enhanced Mem Attention: only greater than %dMB will be printed\n", threshold);
	pr_info(TRANMM "%-6s%-6s%-6s%-13s%-8s%-10s%-10s%-10s%-5s%-6s%s\n", "pid", "uid", "tgid", "rssfile(MB)", "rss(MB)", "swap(MB)","total(MB)", "VmHWM(MB)", "cpu", "adj", "name");

	rcu_read_lock();
	for_each_process(p) {
		if (!p || is_global_init(p) || (p->flags & PF_KTHREAD) || (p->pid != p->tgid))
			continue;

		task = tran_find_lock_task_mm(p);

		if (!task)
			continue;

		if (!tranmm_lock_task_sighand(task, &flags)) {
			spin_unlock(&task->alloc_lock);
			continue;
		}
		total = PAGE_MB(get_mm_rss(task->mm) + get_mm_counter(task->mm, MM_SWAPENTS));
		if (total >= threshold) {
			pr_info(TRANMM "%-6d%-6d%-6d%-13lu%-8lu%-10lu%-10lu%-10lu%-5u%-6d%s\n",
				task->pid, from_kuid(&init_user_ns, task_uid(task)), task->tgid,
				PAGE_MB(get_mm_counter(task->mm, MM_FILEPAGES)), PAGE_MB(get_mm_rss(task->mm)), PAGE_MB(get_mm_counter(task->mm, MM_SWAPENTS)),
				total, PAGE_MB(get_mm_hiwater_rss(task->mm)), task_cpu(task), task->signal->oom_score_adj, task->comm);
		}
		unlock_task_sighand(task, &flags);
		spin_unlock(&task->alloc_lock);
	}
	rcu_read_unlock();

	tran_memory_info_kernel_trace_end(TRANMM"dump_tasks_info");
}

// When the available memory size is smaller than available_exception_threshold_mb(MB), 
// or when the file memory size is smaller than file_exception_threshold_mb(MB), the memory status is considered as exception
bool is_exception(void)
{
	unsigned long file_pages;
	unsigned long available;

	file_pages =  global_node_page_state(NR_ACTIVE_FILE) + global_node_page_state(NR_INACTIVE_FILE);
	available = (unsigned long)si_mem_available();

	if (PAGE_MB(file_pages) < file_exception_threshold_mb)
		pr_err(TRANMM "Excep FilePages %lu MB < %ld MB\n", PAGE_MB(file_pages), file_exception_threshold_mb);
	else if (PAGE_MB(available) < available_exception_threshold_mb)
		pr_err(TRANMM "Excep MemAva %lu MB < %ld MB\n", PAGE_MB(available), available_exception_threshold_mb);
	else
		return false;

	return true;
}

static void add_cpu_usage(const collect_data_t type, unsigned long value)
{
	int bucket_index;
	struct distribution_stats *dist_stats = &g_dist_stats[type];
	int threshold = *collect_data_thresholds[type];
	mutex_lock(&g_dist_stats_lock);
	if (!dist_stats->enabled) {
		mutex_unlock(&g_dist_stats_lock);
		return;
	}

	if (dist_stats->valid_count > MAX_COLLECT_LENGTH) {
		dist_stats->valid_count = 0;
		dist_stats->above_threshold_count = 0;
		memset(dist_stats->buckets, 0, sizeof(int) * DISTRIBUTION_BUCKETS);
	}

	bucket_index = value / DISTRIBUTION_INTERVAL;
	if (bucket_index >= DISTRIBUTION_BUCKETS) {
		bucket_index = DISTRIBUTION_BUCKETS - 1;
	}
	dist_stats->buckets[bucket_index]++;
	dist_stats->valid_count++;
	if (value > threshold) {
		dist_stats->above_threshold_count++;
	}
	mutex_unlock(&g_dist_stats_lock);
}

static int meminfo_proc_show(struct sysinfo *si)
{
	long cached;
	long available;
	unsigned long pages[NR_LRU_LISTS];
	unsigned long sreclaimable, sunreclaim, total_anon_file, alloc_stall;
	unsigned long memfree, buffer, kreclaimable, mapped, shmem, pagetables, vmallocused, cmatotal;
	unsigned long cachedkernel, kernelused;
	int lru;
	int ret = 0;

	struct task_struct *kswapd_task;
	struct task_struct *kcompactd_task;
	struct task_cputime kswapd_cputime;
	struct task_cputime kcompactd_cputime;
	static bool first_call = true;

	unsigned long pgsteal_kswapd = 0;
	u64 kswapd_runtime_ms = 0;
	u64 kcompactd_runtime_ms = 0;
	u64 current_time_ms = 0;

	static unsigned long prev_pgsteal_kswapd = 0;
	static u64 prev_kswapd_runtime_ms = 0;
	static u64 prev_kcompactd_runtime_ms = 0;
	static u64 prev_time_ms = 0;

	unsigned long delta_pgsteal_kswapd = 0;
	u64 delta_kswapd_runtime_ms = 0;
	u64 delta_kcompactd_runtime_ms = 0;
	u64 delta_time_ms = 0;

	unsigned long kswapd_efficiency = 0;
	unsigned long kswapd_cpu_usage = 0; /* CPU usage percentage */
	unsigned long kcompact_cpu_usage = 0;

	tran_memory_info_kernel_trace_start(TRANMM"meminfo_proc_show");

#ifdef CONFIG_VM_EVENT_COUNTERS
	unsigned long *v;
	v = kmalloc_array(NR_VM_EVENT_ITEMS, sizeof(unsigned long), GFP_KERNEL);
	if (!v) {
		ret = -ENOMEM;
		goto out;
	}

	all_vm_events(v);
	/* sectors -> kbytes */
	v[PGPGIN] /= 2;
	v[PGPGOUT] /= 2;
#endif

	cached = global_node_page_state(NR_FILE_PAGES) -
			total_swapcache_pages() - si->bufferram;
	if (cached < 0)
		cached = 0;

	for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
		pages[lru] = global_node_page_state(NR_LRU_BASE + lru);

	available = si_mem_available();
	sreclaimable = global_node_page_state_pages(NR_SLAB_RECLAIMABLE_B);
	sunreclaim = global_node_page_state_pages(NR_SLAB_UNRECLAIMABLE_B);
	total_anon_file = pages[LRU_ACTIVE_ANON] + pages[LRU_ACTIVE_FILE] + pages[LRU_INACTIVE_ANON] + pages[LRU_INACTIVE_FILE];
	memfree = si->freeram;
	buffer = si->bufferram;
	kreclaimable = sreclaimable + global_node_page_state(NR_KERNEL_MISC_RECLAIMABLE);
	mapped = global_node_page_state(NR_FILE_MAPPED);
	shmem = si->sharedram;
	pagetables = global_node_page_state(NR_PAGETABLE);
	vmallocused = vmalloc_nr_pages();
#ifdef CONFIG_CMA
	cmatotal = *totalcma_pages_symbol;
#else
	cmatotal = 0;
#endif
	cachedkernel = buffer + kreclaimable + cached - mapped;
	kernelused = shmem + sunreclaim + pagetables + vmallocused;

	pr_info(TRANMM "meminfo usage CachedKernel+MemFree:%lukB;KernelUsed:%lukB;PhysicalSize:%lukB;MemTotal:%lukB;MemFree:%lukB;MemAvailable:%lukB;Buffers:%lukB;Cached:%lukB;SwapCached:%lukB;Active:%lukB;Inactive:%lukB;Active(anon):%lukB;Inactive(anon):%lukB;Active(file):%lukB;Inactive(file):%lukB;Unevictable:%lukB;Mlocked:%lukB;Dirty:%lukB;Writeback:%lukB;AnonPages:%lukB;Mapped:%lukB;Shmem:%lukB;KReclaimable:%lukB;Slab:%lukB;SReclaimable:%lukB;SUnreclaim:%lukB;PageTables:%lukB;VmallocUsed:%lukB;CmaTotal:%lukB\n",
		PAGE_KB(cachedkernel + memfree), PAGE_KB(kernelused), PAGE_KB(g_physpages), PAGE_KB(si->totalram), PAGE_KB(memfree), PAGE_KB(available), PAGE_KB(buffer), PAGE_KB(cached),
		PAGE_KB(total_swapcache_pages()), PAGE_KB(pages[LRU_ACTIVE_ANON] + pages[LRU_ACTIVE_FILE]), PAGE_KB(pages[LRU_INACTIVE_ANON] + pages[LRU_INACTIVE_FILE]),
		PAGE_KB(pages[LRU_ACTIVE_ANON]), PAGE_KB(pages[LRU_INACTIVE_ANON]), PAGE_KB(pages[LRU_ACTIVE_FILE]), PAGE_KB(pages[LRU_INACTIVE_FILE]), PAGE_KB(pages[LRU_UNEVICTABLE]),
		PAGE_KB(global_zone_page_state(NR_MLOCK)), PAGE_KB(global_node_page_state(NR_FILE_DIRTY)), PAGE_KB(global_node_page_state(NR_WRITEBACK)),
		PAGE_KB(global_node_page_state(NR_ANON_MAPPED)), PAGE_KB(mapped), PAGE_KB(si->sharedram),
		PAGE_KB(kreclaimable), PAGE_KB(sreclaimable + sunreclaim), PAGE_KB(sreclaimable), PAGE_KB(sunreclaim),
		PAGE_KB(pagetables), PAGE_KB(vmallocused), PAGE_KB(cmatotal));
//	pr_info(TRANMM "%-30s%-8lu kB\n", "SwapTotal:", PAGE_KB(si->totalswap));
//	pr_info(TRANMM "%-30s%-8lu kB\n", "SwapFree:", PAGE_KB(si->freeswap));
//	pr_info(TRANMM "%-30s%-8lu kB\n", "swaped:", PAGE_KB(si->totalswap - si->freeswap));
//	pr_info(TRANMM "%-30s%-8lu kB\n", "KernelStack:", global_node_page_state(NR_KERNEL_STACK_KB));
//	pr_info(TRANMM "%-30s%-8lu kB\n", "ShadowCallStack:",  global_node_page_state(NR_KERNEL_SCS_KB));
//	pr_info(TRANMM "%-30s%-8lu kB\n", "PageTables:", PAGE_KB(global_node_page_state(NR_PAGETABLE)));

//	pr_info(TRANMM "%-30s%-8lu kB\n", "Bounce:", PAGE_KB(global_zone_page_state(NR_BOUNCE)));
//	pr_info(TRANMM "%-30s%-8lu kB\n", "WritebackTmp:", PAGE_KB(global_node_page_state(NR_WRITEBACK_TEMP)));
//	pr_info(TRANMM "%-30s%-8lu kB\n", "VmallocTotal:", (unsigned long)VMALLOC_TOTAL >> 10);
//	pr_info(TRANMM "%-30s%-8lu kB\n", "VmallocUsed:", vmalloc_nr_pages());
//	pr_info(TRANMM "%-30s%-8lu kB\n", "VmallocChunk:", 0ul);

	// Output the information of vmstat
#ifdef CONFIG_VM_EVENT_COUNTERS
       alloc_stall = v[ALLOCSTALL_NORMAL] + v[ALLOCSTALL_MOVABLE];
#ifdef CONFIG_ZONE_DMA
       alloc_stall += v[ALLOCSTALL_DMA];
#endif
#ifdef CONFIG_ZONE_DMA32
       alloc_stall += v[ALLOCSTALL_DMA32];
#endif
#ifdef CONFIG_HIGHMEM
       alloc_stall += v[ALLOCSTALL_HIGH];
#endif
#ifdef CONFIG_ZONE_DEVICE
       alloc_stall += v[ALLOCSTALL_DEVICE];
#endif

	if (g_kswapd_pid && g_kcompactd_pid) {
		rcu_read_lock();
		kswapd_task = find_task_by_vpid(g_kswapd_pid);
		kcompactd_task = find_task_by_vpid(g_kcompactd_pid);

		if (kswapd_task) {
			thread_group_cputime_adjusted(kswapd_task, &kswapd_cputime.utime, &kswapd_cputime.stime);
			kswapd_runtime_ms =  div_u64(kswapd_cputime.stime, NSEC_PER_MSEC);
		}

		if (kcompactd_task) {
			thread_group_cputime_adjusted(kcompactd_task, &kcompactd_cputime.utime, &kcompactd_cputime.stime);
			kcompactd_runtime_ms = div_u64(kcompactd_cputime.stime, NSEC_PER_MSEC);
		}
		rcu_read_unlock();
	}

	pr_info(TRANMM "vmstat usage workingset_refault_anon:%lu;workingset_refault_file:%lu;workingset_activate_anon:%lu;workingset_activate_file:%lu;workingset_restore_anon:%lu;workingset_restore_file:%lu;pgpgin:%lu;pgpgout:%lu;pswpin:%lu;pswpout:%lu;alloc_stall:%lu;pgfault:%lu;pgmajfault:%lu;pgscan_kswapd:%lu;pgscan_direct:%lu;pgscan_anon:%lu;pgscan_file:%lu;pgscan_total:%lu;pgscan_percentage:%lu;pgsteal_kswapd:%lu;pgsteal_direct:%lu;pgsteal_anon:%lu;pgsteal_file:%lu;pgsteal_total:%lu;pgsteal_percentage:%lu;pageoutrun:%lu;oom_kill:%lu;\n",
		global_node_page_state(WORKINGSET_REFAULT_ANON), global_node_page_state(WORKINGSET_REFAULT_FILE), global_node_page_state(WORKINGSET_ACTIVATE_ANON),
		global_node_page_state(WORKINGSET_ACTIVATE_FILE), global_node_page_state(WORKINGSET_RESTORE_ANON), global_node_page_state(WORKINGSET_RESTORE_FILE),
		v[PGPGIN], v[PGPGOUT], v[PSWPIN], v[PSWPOUT], alloc_stall, v[PGFAULT], v[PGMAJFAULT], v[PGSCAN_KSWAPD], v[PGSCAN_DIRECT], v[PGSCAN_ANON], v[PGSCAN_FILE], v[PGSCAN_KSWAPD]+v[PGSCAN_DIRECT],
		100*(v[PGSCAN_KSWAPD]+v[PGSCAN_DIRECT])/total_anon_file, v[PGSTEAL_KSWAPD], v[PGSTEAL_DIRECT], v[PGSTEAL_ANON], v[PGSTEAL_FILE], v[PGSTEAL_KSWAPD]+v[PGSTEAL_DIRECT],
		100*(v[PGSTEAL_KSWAPD]+v[PGSTEAL_DIRECT])/total_anon_file, v[PAGEOUTRUN], v[OOM_KILL]);

	current_time_ms = jiffies_to_msecs(jiffies);
	pgsteal_kswapd = v[PGSTEAL_KSWAPD];
	if (first_call) {
		first_call = false;
	} else {
		if (kswapd_runtime_ms > prev_kswapd_runtime_ms) {
			delta_kswapd_runtime_ms = kswapd_runtime_ms - prev_kswapd_runtime_ms;
		}
		if (kcompactd_runtime_ms > prev_kcompactd_runtime_ms) {
			delta_kcompactd_runtime_ms = kcompactd_runtime_ms - prev_kcompactd_runtime_ms;
		}
		delta_time_ms = current_time_ms - prev_time_ms;
		delta_pgsteal_kswapd = pgsteal_kswapd - prev_pgsteal_kswapd;
		if (delta_kswapd_runtime_ms > 0 && delta_time_ms > 0) {
			kswapd_cpu_usage = div_u64(delta_kswapd_runtime_ms * CPU_USAGE_SCALE,  delta_time_ms);
			kcompact_cpu_usage = div_u64(delta_kcompactd_runtime_ms * CPU_USAGE_SCALE, delta_time_ms);
			kswapd_efficiency = PAGE_KB(delta_pgsteal_kswapd) / delta_kswapd_runtime_ms;
			pr_info(TRANMM "kswapd_runtime: %llu ms, kswapd_efficiency: %lu kB/ms, kswapd_cpu_usage: %lu %%, kcompact_cpu_usage: %lu %%\n",
				delta_kswapd_runtime_ms, kswapd_efficiency, kswapd_cpu_usage, kcompact_cpu_usage);
		} else {
			pr_info(TRANMM "kswapd_runtime: %llu ms, kswapd_cpu_usage: %lu %%, kcompact_cpu_usage: %lu %%\n",
				delta_kswapd_runtime_ms, kswapd_cpu_usage, kcompact_cpu_usage);
		}

		add_cpu_usage(TRANMEM_KSWAPD_CPU_USAGE, kswapd_cpu_usage);
		add_cpu_usage(TRANMEM_KCOMPACTD_CPU_USAGE, kcompact_cpu_usage);
	}

	prev_pgsteal_kswapd = pgsteal_kswapd;
	prev_kswapd_runtime_ms = kswapd_runtime_ms;
	prev_kcompactd_runtime_ms = kcompactd_runtime_ms;
	prev_time_ms = current_time_ms;

//	pr_info(TRANMM "%-30s%-8lu\n", "pgscan_direct_throttle:", v[PGSCAN_DIRECT_THROTTLE]);
//	pr_info(TRANMM "%-30s%-8lu\n", "kswapd_inodesteal:", v[KSWAPD_INODESTEAL]);
//	pr_info(TRANMM "%-30s%-8lu\n", "kswapd_low_wmark_hit_quickly:", v[KSWAPD_LOW_WMARK_HIT_QUICKLY]);
#endif

#ifdef CONFIG_VM_EVENT_COUNTERS
	kfree(v);
#endif
out:
	tran_memory_info_kernel_trace_end(TRANMM"meminfo_proc_show");
	return ret;
}

/*
 * Output the memory used information of each vmalloc memory to the kernel log
 * Only when the physical memory usage is greater than vmalloc_show_threshold, the corresponding vmalloc memory information will be outputted to the kernel log.
 */
static void dump_vmalloc_info(void)
{
	struct vmap_node *vn;
	struct vmap_area *va;
	struct vm_struct *v;
	unsigned long used_pages = 0;
	int i;
	char log_buf[512];
	int len;
	size_t buf_size;
	int temp_len;

	if (!nr_vmap_nodes_symbol || !vmap_nodes_symbol) {
		pr_err(TRANMM "(%s) nr_vmap_nodes(0x%p) or vmap_nodes address(0x%p) get fail, vmalloc info can not be accessed\n", __func__, nr_vmap_nodes_symbol, vmap_nodes_symbol);
		return;
	}

	tran_memory_info_kernel_trace_start(TRANMM"dump_vmalloc_info");
	pr_info(TRANMM "Vmalloc info Attention: only greater than %dkB will be printed\n", vmalloc_show_threshold_kb);
	pr_info(TRANMM "%-10s %-20s\n", "Size(kB)", "Caller");

	for (i = 0; i < *nr_vmap_nodes_symbol; i++) {
		vn = &vmap_nodes_symbol[i];

		spin_lock(&vn->busy.lock);
		list_for_each_entry(va, &vn->busy.head, list) {
			if (!va->vm)
				continue;

			v = va->vm;
			if (v->nr_pages > 0) {
				used_pages += v->nr_pages;
				if ((v->nr_pages << (PAGE_SHIFT - 10)) < vmalloc_show_threshold_kb)
					continue;

				buf_size = sizeof(log_buf);
				memset(log_buf, 0, sizeof(log_buf));
				len = 0;

				if (v->nr_pages)
					len += scnprintf(log_buf + len, buf_size - len, "%-10d", v->nr_pages << (PAGE_SHIFT - 10));

				if (v->caller) {
					temp_len = scnprintf(NULL, 0, " %pS", v->caller);
					if (temp_len  > (buf_size>>1))
						len += scnprintf(log_buf + len, buf_size - len, " [INFO TRUNCATED]");
					else
						len += scnprintf(log_buf + len, buf_size - len, " %pS", v->caller);
				} else {
					len += scnprintf(log_buf + len, buf_size - len, " <unknown caller>");
				}

				if (v->phys_addr)
					len += scnprintf(log_buf + len, buf_size - len, " phys=%pa", &v->phys_addr);

				if (v->flags & VM_IOREMAP)
					len += scnprintf(log_buf + len, buf_size - len, " ioremap");

				if (v->flags & VM_SPARSE)
					len += scnprintf(log_buf + len, buf_size - len, " sparse");

				if (v->flags & VM_ALLOC)
					len += scnprintf(log_buf + len, buf_size - len, " vmalloc");

				if (v->flags & VM_MAP)
					len += scnprintf(log_buf + len, buf_size - len, " vmap");

				if (v->flags & VM_USERMAP)
					len += scnprintf(log_buf + len, buf_size - len, " user");

				if (v->flags & VM_DMA_COHERENT)
					len += scnprintf(log_buf + len, buf_size - len, " dma-coherent");

				if (is_vmalloc_addr(v->pages))
					len += scnprintf(log_buf + len, buf_size - len, " vpages");

				pr_info(TRANMM "%s\n", log_buf);
			}
		}
		spin_unlock(&vn->busy.lock);
	}
	pr_info(TRANMM "Total used:%lukB\n", (unsigned long)(used_pages << (PAGE_SHIFT - 10)));

	tran_memory_info_kernel_trace_end(TRANMM"dump_vmalloc_info");
	return;
}

/* MODULE_REF_BASE is the base reference count by kmodule loader. */
#define MODULE_REF_BASE	1

static int tran_module_refcount(struct module *mod)
{
	return atomic_read(&mod->refcnt) - MODULE_REF_BASE;
}

static unsigned int module_total_size(struct module *mod)
{
	int size = 0;

	for_each_mod_mem_type(type)
		size += mod->mem[type].size;
	return size >> 10;
}

/*
 * Output the memory used information of each module(driver) memory to the kernel log
 * Only when the physical memory usage is greater than modules_show_threshold_kb(KB), the corresponding module(driver) memory information will be outputted to the kernel log.
 */
static void dump_modules_info(void)
{
	struct module *mod;
	unsigned int size;
	unsigned int used_count;
	unsigned long total_used = 0;

	if (!module_mutex_symbol || !modules_symbol) {
		pr_err(TRANMM "(%s) module_mutex(0x%p) or modules address(0x%p) get fail, vmalloc info can not be accessed\n", __func__, module_mutex_symbol, modules_symbol);
		return;
	}

	tran_memory_info_kernel_trace_start(TRANMM"dump_modules_info");
	pr_info(TRANMM "Modules Info Atention: only greater than %dkB will be printed\n", modules_show_threshold_kb);
	pr_info(TRANMM "%-40s%-10s%-10s\n", "ModuleName", "Size(kB)", "UsedNum");

	mutex_lock(module_mutex_symbol);

	list_for_each_entry(mod, modules_symbol, list) {
		/* We always print live modules. */
		if (!mod || mod->state != MODULE_STATE_LIVE)
			continue;
		size = module_total_size(mod);
		size /= 1024;
		total_used += size;
		if (size < modules_show_threshold_kb)
			continue;
		used_count = tran_module_refcount(mod);
		pr_info(TRANMM "%-40s%-10u%-10u\n", mod->name, size, used_count);
	}

	mutex_unlock(module_mutex_symbol);
	pr_info(TRANMM "Total used:%lukB\n", total_used);

	tran_memory_info_kernel_trace_end(TRANMM"dump_modules_info");
	return;
}

// NOTICE: This function is adapted from kernel-6.1
// These code must be checked when porting to other kernel versions
/*
 * The function fair_sched_class.update_curr accesses the struct curr
 * and its field curr->exec_start; when called from task_sched_runtime(),
 * we observe a high rate of cache misses in practice.
 * Prefetching this data results in improved performance.
 */
static inline void tran_prefetch_curr_exec_start(struct task_struct *p)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	struct sched_entity *curr = p->se.cfs_rq->curr;
#else
	struct sched_entity *curr = task_rq(p)->cfs.curr;
#endif
	prefetch(curr);
	prefetch(&curr->exec_start);
}

// NOTICE: This function is adapted from kernel-6.1
// These code must be checked when porting to other kernel versions
/*
 * Return accounted runtime for the task.
 * In case the task is currently running, return the runtime plus current's
 * pending runtime that have not been accounted yet.
 */
unsigned long long tran_task_sched_runtime(struct task_struct *p)
{
	struct rq_flags rf;
	struct rq *rq;
	u64 ns;

#if defined(CONFIG_64BIT) && defined(CONFIG_SMP)
	/*
	 * 64-bit doesn't need locks to atomically read a 64-bit value.
	 * So we have a optimization chance when the task's delta_exec is 0.
	 * Reading ->on_cpu is racy, but this is OK.
	 *
	 * If we race with it leaving CPU, we'll take a lock. So we're correct.
	 * If we race with it entering CPU, unaccounted time is 0. This is
	 * indistinguishable from the read occurring a few cycles earlier.
	 * If we see ->on_cpu without ->on_rq, the task is leaving, and has
	 * been accounted, so we're correct here as well.
	 */
	if (!p->on_cpu || !task_on_rq_queued(p))
		return p->se.sum_exec_runtime;
#endif

	rq = task_rq_lock(p, &rf);
	/*
	 * Must be ->curr _and_ ->on_rq.  If dequeued, we would
	 * project cycles that may never be accounted to this
	 * thread, breaking clock_gettime().
	 */
	if (task_current_donor(rq, p) && task_on_rq_queued(p)) {
		tran_prefetch_curr_exec_start(p);
		update_rq_clock(rq);
		p->sched_class->update_curr(rq);
	}
	ns = p->se.sum_exec_runtime;
	task_rq_unlock(rq, p, &rf);

	return ns;
}

static void dump_buddyinfo(void)
{
	int nid;
	int order;
	struct zone *zone;
	unsigned long flags;
	char buf[256];
	int len;

	pr_info(TRANMM "Buddyinfo:\n");
	pr_info(TRANMM "Order:%23d%6d%6d%6d%6d%6d%6d%6d%6d%6d%6d\n", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

	for_each_online_node(nid) {
		pg_data_t *pgdat = NODE_DATA(nid);
		struct zone *node_zones;

		if (!pgdat)
			continue;

		node_zones = pgdat->node_zones;
		for (zone = node_zones; zone - node_zones < MAX_NR_ZONES; ++zone) {

			if (!populated_zone(zone))
				continue;

			spin_lock_irqsave(&zone->lock, flags);

			len = 0;
			len += scnprintf(buf + len, sizeof(buf) - len, "Buddy:Node %d, zone %8s ", pgdat->node_id, zone->name);
			for (order = 0; order < NR_PAGE_ORDERS; ++order)
				len += scnprintf(buf + len, sizeof(buf) - len, "%-6lu", data_race(zone->free_area[order].nr_free));

			spin_unlock_irqrestore(&zone->lock, flags);

			pr_info(TRANMM "%s\n", buf);
		}
	}
}

static atomic64_t critical_thread_alloc_execution_time_total = ATOMIC64_INIT(0);  //ns
static atomic64_t critical_thread_alloc_consume_time_total = ATOMIC64_INIT(0);  //ns
#define ALLOC_NUM_INTERVALS 15
extern int page_fault_time_enable;
static atomic64_t critical_alloc_execution_time_counts[ALLOC_NUM_INTERVALS];
static atomic64_t critical_alloc_consume_time_counts[ALLOC_NUM_INTERVALS];
static const char* ALLOC_INTERVAL_STRINGS[ALLOC_NUM_INTERVALS] = {
    "[0ns,1ns]",
    "(1ns,10ns]",
    "(10ns,100ns]",
    "(100ns,1us]",
    "(1us,10us]",
    "(10us,100us]",
    "(100us,1ms]",
    "(1ms,2ms]",
    "(2ms,3ms]",
    "(3ms,5ms]",
    "(5ms,10ms]",
    "(10ms,15ms]",
    "(15ms,20ms]",
    "(20ms,30ms]",
    "(30ms,inf]",
};

/*
 * Update the critical thread time distribution based on the time_interval_ns
 * @distribution: the critical thread time distribution array
 * @total: the total critical thread time
 * @time_interval_ns: the gap time between two critical thread events
 */
void update_critical_thread_time_distribution(atomic64_t *distribution, atomic64_t *total, s64 time_interval_ns)
{
	// page_fault_time_enable=0, disable this feature
	if ((page_fault_time_enable <= PAGE_FAULT_TIME_DISABLE) || (page_fault_time_enable >= PAGE_FAULT_TIME_INVALID) ||
		((page_fault_time_enable == PAGE_FAULT_TIME_ONLY_CRITICAL) && !is_critical(current)))
		return;

	if(time_interval_ns <= 1)
		atomic64_inc(&distribution[0]);
	else if(time_interval_ns <= 10)
		atomic64_inc(&distribution[1]);
	else if(time_interval_ns <= 100)
		atomic64_inc(&distribution[2]);
	else if(time_interval_ns <= 1000)
		atomic64_inc(&distribution[3]);
	else if(time_interval_ns <= 10000)
		atomic64_inc(&distribution[4]);
	else if(time_interval_ns <= 100000)
		atomic64_inc(&distribution[5]);
	else if(time_interval_ns <= 1000000)
		atomic64_inc(&distribution[6]);
	else if(time_interval_ns <= 2000000)
		atomic64_inc(&distribution[7]);
	else if(time_interval_ns <= 3000000)
		atomic64_inc(&distribution[8]);
	else if(time_interval_ns <= 5000000)
		atomic64_inc(&distribution[9]);
	else if(time_interval_ns <= 10000000)
		atomic64_inc(&distribution[10]);
	else if(time_interval_ns <= 15000000)
		atomic64_inc(&distribution[11]);
	else if(time_interval_ns <= 20000000)
		atomic64_inc(&distribution[12]);
	else if(time_interval_ns <= 30000000)
		atomic64_inc(&distribution[13]);
	else
		atomic64_inc(&distribution[14]);

	atomic64_add(time_interval_ns, total);
}

#define MAX_ORDER_ALLOC_PAGES (MAX_PAGE_ORDER+2)
struct alloc_page_info {
	// the total call times of alloc_page
	atomic64_t call_times;
	// the total exctuion time of alloc_page
	atomic64_t execution_time;
	// the total consume time of alloc_page
	atomic64_t consume_time;
};
struct alloc_page_info alloc_times[MAX_ORDER_ALLOC_PAGES];

/*
 * Output the alloc_page information of each order to the kernel log
 * @order: the order of alloc_page
 * @total_times: the total call times of alloc_page
 * @total_mem: the total memory of alloc_page
 * @total_execution_time: the total execution time of alloc_page
 * @total_consume_time: the total consume time of alloc_page
 */
static void dump_alloc_page(void) {
	int i;
	int j;
	u64 total_times;
	u64 total_mem;
	u64 total_execution_time;
	u64 total_consume_time;
	if ((page_fault_time_enable > PAGE_FAULT_TIME_DISABLE) && (page_fault_time_enable < PAGE_FAULT_TIME_INVALID)) {
		pr_info(TRANMM"%-10s%-12s%-12s%-16s%-16s\n", "Order", "AllocTimes",
			"AllocMemory(MB)", "TotalExcTime(us)", "TotalConsTime(us)");
		for (i=0; i<MAX_ORDER_ALLOC_PAGES; i++) {
			total_times = (u64)atomic64_read(&(alloc_times[i].call_times));
			total_mem = (u64)PAGE_MB(total_times << i);
			total_execution_time = (u64)atomic64_read(&(alloc_times[i].execution_time));
			total_consume_time = (u64)atomic64_read(&(alloc_times[i].consume_time));
			pr_info(TRANMM"Order:%-4d%-12llu%-12llu%-16llu%-16llu\n", i, total_times, total_mem,
				total_execution_time/NSEC_PER_USEC, total_consume_time/NSEC_PER_USEC);
		}
		pr_info(TRANMM"alloc_time:%-16s%-16s%-16s\n", "TimeRange", "CriProAlExeTime", "CriProAlConsTime");
		for(j = 0; j < ALLOC_NUM_INTERVALS; j++){
			pr_info(TRANMM"alloc_time:%-16s%-16llu%-16llu\n", ALLOC_INTERVAL_STRINGS[j],
				(u64)atomic64_read(&critical_alloc_execution_time_counts[j]),
				(u64)atomic64_read(&critical_alloc_consume_time_counts[j]));
		}
	}

	return;
}

struct alloc_time_data {
	unsigned long long enter_execution_time;
	ktime_t enter_consume_time;
	unsigned int order;
};

/*
 * Pre-handler: record the enter time of __alloc_pages
 */
static int alloc_handler_pre(struct kretprobe_instance *ri, struct pt_regs *regs) {
	struct alloc_time_data *data;

	if (IS_ERR_OR_NULL(ri) || IS_ERR_OR_NULL(ri->data)) {
		pr_err(TRANMM"alloc_handler_pre: kretprobe_instance is IS_ERR_OR_NULL\n");
		return 0;
	}

	if (IS_ERR_OR_NULL(regs)) {
		pr_err(TRANMM"alloc_handler_pre: regs is IS_ERR_OR_NULL\n");
		return 0;
	}

	data = (struct alloc_time_data *)ri->data;
	data->enter_execution_time = tran_task_sched_runtime(current);
	data->enter_consume_time = ktime_get();
	data->order = (unsigned int)regs_get_kernel_argument(regs, 1);

	return 0;
}

/*
 * Post-handler: record the exit time of __alloc_pages and update the alloc_page statistics
 */
static int alloc_handler_post(struct kretprobe_instance *ri, struct pt_regs *regs) {
	unsigned long long end_execution_ns;
	ktime_t end_consume_ns;
	s64 execution_ns;
	s64 consume_ns;
	struct alloc_time_data *data;
	// Used to store the secondry last input parameter(flags) of function to trace(__alloc_pages)
	unsigned int func_order = 100;
	struct page *ret_value = NULL;

	if (IS_ERR_OR_NULL(ri) || IS_ERR_OR_NULL(ri->data)) {
		pr_err(TRANMM"alloc_handler_post: kretprobe_instance is IS_ERR_OR_NULL\n");
		return 0;
	}

	if (IS_ERR_OR_NULL(regs)) {
		pr_err(TRANMM"alloc_handler_post: regs is IS_ERR_OR_NULL\n");
		return 0;
	}

	ret_value = (struct page *)regs_return_value(regs);
	// If the return value is NULL or ERR, the allocation fails, we ignore this record
	if (IS_ERR_OR_NULL(ret_value))
		return 0;

	data = (struct alloc_time_data *)ri->data;
	func_order = data->order;
	end_execution_ns = tran_task_sched_runtime(current);
	end_consume_ns = ktime_get();
	execution_ns = end_execution_ns - data->enter_execution_time;
	consume_ns = ktime_to_ns(ktime_sub(end_consume_ns, data->enter_consume_time));
	if (execution_ns < 0) {
		pr_err(TRANMM"alloc_handler_post: time reversal detected: exec_start=%llu, exec_end=%llu\n",
			data->enter_execution_time, end_execution_ns);
		return 0;
	}

	if (is_critical(current)) {
		update_critical_thread_time_distribution(critical_alloc_execution_time_counts,
			&critical_thread_alloc_execution_time_total, execution_ns);
		update_critical_thread_time_distribution(critical_alloc_consume_time_counts,
			&critical_thread_alloc_consume_time_total, consume_ns);
	}

	if ((func_order >= 0) && (func_order <= MAX_PAGE_ORDER)) {
		atomic64_inc(&(alloc_times[func_order].call_times));
		atomic64_add(execution_ns, &(alloc_times[func_order].execution_time));
		atomic64_add(consume_ns, &(alloc_times[func_order].consume_time));
	} else {
		atomic64_inc(&(alloc_times[MAX_PAGE_ORDER+1].call_times));
		atomic64_add(execution_ns, &(alloc_times[MAX_PAGE_ORDER+1].execution_time));
		atomic64_add(consume_ns, &(alloc_times[MAX_PAGE_ORDER+1].consume_time));
	}

	return 0;
}

static struct kretprobe alloc_kp = {
    .kp.symbol_name = "__alloc_pages_noprof",
    .entry_handler = alloc_handler_pre,
    .handler = alloc_handler_post,
    .data_size = sizeof(struct alloc_time_data),
    .maxactive = 1024,
};

int register_alloc_page(void){
	int ret = 0;
	int i = 0;

	ret = register_kretprobe(&alloc_kp);
	if (ret < 0) {
		pr_err(TRANMM"[tranmem_init]Failed to register kretprobe(__alloc_pages): %d\n", ret);
	} else {
		pr_info(TRANMM"[tranmem_init]Succeed to register kretprobe(__alloc_pages): %d\n", ret);
		for (i = 0; i < ALLOC_NUM_INTERVALS; i++) {
			atomic64_set(&critical_alloc_execution_time_counts[i], 0);
			atomic64_set(&critical_alloc_consume_time_counts[i], 0);
		}
	}

	return ret;
}
void unregister_alloc_page(void){
	unregister_kretprobe(&alloc_kp);
}

static atomic64_t user_last_time = ATOMIC64_INIT(0);
static atomic64_t kernel_last_time = ATOMIC64_INIT(0);

static bool emem_available(int adj, bool is_kernel)
{
	struct timespec64 time;
	u64 current_time = 0;
	u64 prev_last_time = 0;
	int interval = EMEM_SHOW_KILL_ADJ900_INTERVAL;
	s64 delta = 0;

	if (!is_kernel) {
		if (adj > DEFAULT_PROC_ADJ)
			return false;

		if (adj <= adj_threshold)
			interval = EMEM_SHOW_INTERVAL;
	}

	ktime_get_real_ts64(&time);
	current_time = time.tv_sec;
	if (is_kernel)
		prev_last_time = atomic64_read(&kernel_last_time);
	else
		prev_last_time = atomic64_read(&user_last_time);

	delta = (s64)current_time - (s64)prev_last_time;

	if (delta < 0) {
		if (is_kernel)
			atomic64_set(&kernel_last_time, current_time);
		else
			atomic64_set(&user_last_time, current_time);
		//If the current time is before the last recored time, update the record time
		if (-delta > interval) {
			return true;
		} else {
			pr_warn(TRANMM "Time rollback detected, but not enough to trigger print\n");
			return false;
		}
	}

	if (delta < interval) {
		return false;
	}

	if (is_kernel)
		atomic64_set(&kernel_last_time, current_time);
	else
		atomic64_set(&user_last_time, current_time);

	return true;
}

static void emem_duration_workfn(struct work_struct *work)
{
	struct emem_work *ework = container_of(work, struct emem_work, work_duration);
	int adj = ework->adj;
	struct sysinfo si;
	ktime_t time_s, time_e;
	u64 exetime_s, exetime_e;
	bool is_available = emem_available(adj, true);

	if (kernel_log_enable()) {
		time_s = ktime_get_boottime();
		exetime_s = current->se.sum_exec_runtime;
	}

	si_meminfo(&si);
	si_swapinfo(&si);

	meminfo_proc_show(&si);
	zram_emem_printk(&si, DURATION);

#ifdef CONFIG_ION
	pr_info(TRANMM "ION:            %8lu kB\n", PAGE_KB(get_ion_heap_total_pages()));
#endif

	if (adj <= adj_threshold) {
		if (is_available)
			dump_tasks_info(0);
		else
			dump_tasks_info(process_show_threshold_mb);
		dump_vmalloc_info();
		dump_modules_info();
		dump_slabinfo();
	} else {
		dump_tasks_info(process_show_threshold_mb);
	}
	dump_buddyinfo();
	dump_alloc_page();
	if (kallsyms_mtk_tranmem_print_gpu_mem){
		if ((adj <= adj_threshold) && is_available)
			kallsyms_mtk_tranmem_print_gpu_mem(TRANMM,0);
		else
			kallsyms_mtk_tranmem_print_gpu_mem(TRANMM, gpu_show_threshold_kb);
	}
	if ((adj <= adj_threshold) && is_available)
		dma_heap_emem_printk(0);
	else
		dma_heap_emem_printk(dmabuf_show_threshold_kb);

	if (kernel_log_enable()) {
		time_e = ktime_get_boottime();
		exetime_e = current->se.sum_exec_runtime;

		pr_info(TRANMM "notice work_time consumes[%llu]ms, sum_exec_time[%llu]\n",
				ktime_ms_delta(time_e, time_s),
				(exetime_e - exetime_s) / NSEC_PER_MSEC);
	}

	return;
}

static void emem_exception_workfn(struct work_struct *work)
{
	struct emem_work *ework = container_of(work, struct emem_work, work_exception);
	int adj = ework->adj;
	struct sysinfo si;
	ktime_t time_s, time_e;
	u64 exetime_s, exetime_e;

	if (!emem_available(adj, false))
		return;

	if (adj <= adj_threshold)
		pr_info(TRANMM "Excep LMKP\n");

	if (kernel_log_enable()) {
		time_s = ktime_get_boottime();
		exetime_s = current->se.sum_exec_runtime;
	}

	si_meminfo(&si);
	si_swapinfo(&si);

	meminfo_proc_show(&si);
	zram_emem_printk(&si, EXCEPTION);

#ifdef CONFIG_ION
	pr_info(TRANMM "ION:            %8lu kB\n", PAGE_KB(get_ion_heap_total_pages()));
#endif

	if (adj <= adj_threshold) {
		dump_tasks_info(0);
		dump_vmalloc_info();
		dump_modules_info();
	} else {
		dump_tasks_info(process_show_threshold_mb);
	}
	dump_slabinfo();
	dump_alloc_page();
	dump_buddyinfo();
	dma_heap_emem_printk(0);
	if (kallsyms_mtk_tranmem_print_gpu_mem){
		kallsyms_mtk_tranmem_print_gpu_mem(TRANMM,0);
	}
	if (kernel_log_enable()) {
		time_e = ktime_get_boottime();
		exetime_e = current->se.sum_exec_runtime;

		pr_info(TRANMM "notice work_time consumes[%llu]ms, sum_exec_time[%llu]\n",
				ktime_ms_delta(time_e, time_s),
				(exetime_e - exetime_s) / NSEC_PER_MSEC);
	}

	return;
}

bool emem_trigger(int adj, const char *caller, enum emem_log_type type)
{
	int ret = 0;

	if (!emem_enable)
		return false;

	/* Check if workqueue is initialized */
	if (!emem_wq) {
		pr_err(TRANMM "emem_wq is not initialized, caller %s\n", caller);
		return false;
	}

	g_ework.adj = adj;
	if (type == DURATION) {
		ret = queue_work(emem_wq, &g_ework.work_duration);
	} else if (type == EXCEPTION) {
		ret = queue_work(emem_wq, &g_ework.work_exception);
	} else {
		pr_err(TRANMM "Enhance meminfo, wrong type(%d) caller %s\n", type, caller);
		return false;
	}

	if (!ret) {
		pr_err(TRANMM "Failed to queue work for caller %s, type %d\n", caller, type);
		return false;
	}

	return true;
}

static ssize_t emem_trigger_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	char buffer[12];
	int adj;

	memset(buffer, 0, sizeof(buffer));

	if (count >= sizeof(buffer)) {
		pr_err(TRANMM"emem_trigger_write: the length of written parameter is longer than 12, which is invalid!\n");
		return -EINVAL;
	}

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	if (kstrtoint(buffer, 0, &adj)) {
		pr_err(TRANMM "Failed to convert input string '%s' to integer\n", buffer);
		return -EINVAL;
	}

	emem_trigger(adj, __func__, EXCEPTION);

	return count;
}

static const struct proc_ops emem_trigger_fops = {
	.proc_write	= emem_trigger_write,
	.proc_lseek	= noop_llseek,
};

static int reclaim_info_stats_show(struct seq_file *m, void *v)
{
	int i, j;

	seq_printf(m, "# CPU usage format: type:distribution[0-4,5-9,10-14,...,195-199,200+];above_threshold_count,threshold\n");

	mutex_lock(&g_dist_stats_lock);
	for (i = 0; i < NR_TRANMEM_COLLECT_DATA_TYPE; i++) {
		struct distribution_stats dist_stats = g_dist_stats[i];
		int threshold = *collect_data_thresholds[i];

		if (dist_stats.enabled && dist_stats.valid_count > 0) {
			seq_printf(m, "%s:", collect_data_type_str[i]);
			/* Print distribution buckets */
			for (j = 0; j < DISTRIBUTION_BUCKETS; j++) {
				if (j == 0) {
					seq_printf(m, "[");
				}
				seq_printf(m, "%d", dist_stats.buckets[j]);
				if (j < DISTRIBUTION_BUCKETS - 1) {
					seq_printf(m, ",");
				} else {
					seq_printf(m, "]");
				}
			}
			seq_printf(m, ";%d,%d\n", dist_stats.above_threshold_count, threshold);
		} else {
			seq_printf(m, "%s:[no_data];%d,%d\n", collect_data_type_str[i], 0, threshold);
		}
	}
	mutex_unlock(&g_dist_stats_lock);
	return 0;
}

static int reclaim_info_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, reclaim_info_stats_show, NULL);
}

static ssize_t reclaim_info_stats_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *ppos)
{
	char buffer[64];
	int i;
	int cmd;

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		return count;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	/* Remove trailing newline */
	if (buffer[count - 1] == '\n')
		buffer[count - 1] = '\0';

	if (kstrtoint(buffer, 0, &cmd)) {
		return -EINVAL;
	}

	switch (cmd) {
		case RECLAIM_STAT_STOP:
			mutex_lock(&g_dist_stats_lock);
			for (i = 0; i < NR_TRANMEM_COLLECT_DATA_TYPE; i++) {
				g_dist_stats[i].enabled = 0;
			}
			mutex_unlock(&g_dist_stats_lock);
			pr_info(TRANMM "reclaim_info_stats: statistics stopped\n");
			break;
		case RECLAIM_STAT_START:
			mutex_lock(&g_dist_stats_lock);
			for (i = 0; i < NR_TRANMEM_COLLECT_DATA_TYPE; i++) {
				g_dist_stats[i].enabled = 1;
				g_dist_stats[i].valid_count = 0;
				g_dist_stats[i].above_threshold_count = 0;
				memset(g_dist_stats[i].buckets, 0, sizeof(int) * DISTRIBUTION_BUCKETS);
			}
			mutex_unlock(&g_dist_stats_lock);
			pr_info(TRANMM "reclaim_info_stats: statistics started\n");
			break;
		case RECLAIM_STAT_CLEAR:
			mutex_lock(&g_dist_stats_lock);
			for (i = 0; i < NR_TRANMEM_COLLECT_DATA_TYPE; i++) {
				g_dist_stats[i].valid_count = 0;
				g_dist_stats[i].above_threshold_count = 0;
				memset(g_dist_stats[i].buckets, 0, sizeof(int) * DISTRIBUTION_BUCKETS);
			}
			mutex_unlock(&g_dist_stats_lock);
			pr_info(TRANMM "reclaim_info_stats: statistics cleared\n");
			break;
		default:
			pr_err(TRANMM "reclaim_info_stats: invalid command %d (0=stop, 1=start, 2=clear)\n", cmd);
			return -EINVAL;
	}

	return count;
}

static const struct proc_ops reclaim_info_stats_fops = {
	.proc_open	= reclaim_info_stats_open,
	.proc_read	= seq_read,
	.proc_write	= reclaim_info_stats_write,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static int kswapd_kcompact_pid_init(void) {
	struct task_struct *task;
	int ret = -1;

	if (!g_kswapd_pid || !g_kcompactd_pid) {
		rcu_read_lock();
		for_each_process(task) {
			if (strncmp(task->comm, "kswapd0", 7) == 0) {
				g_kswapd_pid = task->pid;
				pr_info(TRANMM "Found kswapd with PID: %d\n", g_kswapd_pid);
			} else if (strncmp(task->comm, "kcompactd0", 10) == 0) {
				g_kcompactd_pid = task->pid;
				pr_info(TRANMM "Found kcompactd with PID: %d\n", g_kcompactd_pid);
			}

			if (g_kswapd_pid && g_kcompactd_pid) {
				ret = 0;
				break;
			}
		}
		rcu_read_unlock();
	}
	return ret;
}

/**
void show_mem_handler(void *data, unsigned int filter, nodemask_t *nodemask)
{
	pr_info(TRANMM "Enhance meminfo, caller __show_mem\n");
	emem_trigger(1, __func__, EXCEPTION);
}
**/

static int get_symbol_address(void){
	static struct vmap_node **vmap_node_ptr;

	if (symbols_initialized) {
		return 0;
	}

	struct kprobe kallsyms_lookup_name_kp = {
		.symbol_name = "kallsyms_lookup_name"
	};

	/* get some symbols address using kprobe */
	if (register_kprobe(&kallsyms_lookup_name_kp)) {
		pr_err(TRANMM "get kallsyms_lookup_name addr from kprobe failed!\n");
		// if there is resource initlized before register_kprobe, they need to be freed here befrore return
		return -1;
	}

	kallsyms_lookup_name_dup = (kallsyms_lookup_name_t)kallsyms_lookup_name_kp.addr;
	unregister_kprobe(&kallsyms_lookup_name_kp);

	kallsyms_mtk_tranmem_print_gpu_mem = (kallsyms_mtk_tranmem_print_gpu_mem_t)kallsyms_lookup_name_dup("mtk_tranmem_print_gpu_mem");
	if (!kallsyms_mtk_tranmem_print_gpu_mem) {
		pr_warn(TRANMM "get mtk_tranmem_print_gpu_mem addr failed!\n");
	}

#if defined(CONFIG_SLUB_DEBUG)
	slab_caches_symbol = (struct list_head *)kallsyms_lookup_name_dup("slab_caches");
	if (!slab_caches_symbol) {
		pr_err(TRANMM "get slab_caches addr failed!\n");
		goto err;
	}
	slab_mutex_symbol = (struct mutex *)kallsyms_lookup_name_dup("slab_mutex");
	if (!slab_mutex_symbol) {
		pr_err(TRANMM "get slab_mutex addr failed!\n");
		goto err;
	}
#endif

	nr_vmap_nodes_symbol = (unsigned int *)kallsyms_lookup_name_dup("nr_vmap_nodes");
	if (!nr_vmap_nodes_symbol) {
		pr_err(TRANMM "get nr_vmap_nodes addr failed!\n");
		goto err;
	}

	vmap_node_ptr= (struct vmap_node **)kallsyms_lookup_name_dup("vmap_nodes");
	if (!vmap_node_ptr) {
		pr_err(TRANMM "get vmap_nodes addr failed!\n");
		goto err;
	}
	vmap_nodes_symbol = *vmap_node_ptr;
	if (!vmap_nodes_symbol) {
		pr_err(TRANMM "get vmap_nodes addr failed!\n");
		goto err;
	}

	modules_symbol = (struct list_head *)kallsyms_lookup_name_dup("modules");
	if (!modules_symbol) {
		pr_err(TRANMM "get modules addr failed!\n");
		goto err;
	}
	module_mutex_symbol = (struct mutex *)kallsyms_lookup_name_dup("module_mutex");
	if (!module_mutex_symbol) {
		pr_err(TRANMM "get module_mutex addr failed!\n");
		goto err;
	}
#ifdef CONFIG_CMA
	totalcma_pages_symbol = (unsigned long *)kallsyms_lookup_name_dup("totalcma_pages");
	if (!totalcma_pages_symbol) {
		pr_err(TRANMM "get totalcma_pages addr failed!\n");
		goto err;
	}
#endif
	symbols_initialized = true;
	return 0;

err:
#if defined(CONFIG_SLUB_DEBUG)
	slab_caches_symbol = NULL;
	slab_mutex_symbol = NULL;
#endif
	nr_vmap_nodes_symbol = NULL;
	vmap_nodes_symbol = NULL;
	modules_symbol = NULL;
	module_mutex_symbol = NULL;
	return -1;
}

int emem_init(struct proc_dir_entry *dir)
{
	int ret = 0;
	struct proc_dir_entry *entry;

	g_physpages = get_num_physpages();
	init_memory_exception_threshold();

	for (int i= 0; i < NR_TRANMEM_COLLECT_DATA_TYPE; i++)
	{
		g_dist_stats[i].enabled = 0;
		g_dist_stats[i].valid_count = 0;
		g_dist_stats[i].above_threshold_count = 0;
		memset(g_dist_stats[i].buckets, 0, sizeof(int) * DISTRIBUTION_BUCKETS);
	}

	ret = kswapd_kcompact_pid_init();
	if (ret != 0) {
		pr_err(TRANMM "kswapd_kcompact_pid_init failed!\n");
		return ret;
	}

	ret = get_symbol_address();
	if (ret != 0) {
		pr_err(TRANMM "get some symbol address failed!\n");
		return ret;
	}

	emem_wq = create_singlethread_workqueue("emem_wq");
	if (!emem_wq) {
		pr_err(TRANMM "Failed to create workqueue\n");
		return -ENOMEM;
	}

	entry = proc_create("emem_trigger", S_IWUGO, dir, &emem_trigger_fops);
	if (!entry) {
		pr_err(TRANMM "Failed to create emem_trigger proc entry\n");
		if (emem_wq) {
			destroy_workqueue(emem_wq);
		}
		return -ENOMEM;
	}

	entry = proc_create("reclaim_info_stats", S_IALLUGO, dir, &reclaim_info_stats_fops);
	if (!entry) {
		pr_err(TRANMM "Failed to create reclaim_info_stats proc entry\n");
		if (emem_wq) {
			destroy_workqueue(emem_wq);
		}
		return -ENOMEM;
	}

	INIT_WORK(&g_ework.work_duration, emem_duration_workfn);
	INIT_WORK(&g_ework.work_exception, emem_exception_workfn);

	return 0;
}

void emem_exit(void)
{
	if (emem_wq) {
		flush_workqueue(emem_wq);
		destroy_workqueue(emem_wq);
	}

	return;
}

MODULE_IMPORT_NS(MINIDUMP);
