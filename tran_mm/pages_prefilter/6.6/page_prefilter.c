// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 Transsion. All rights reserved.
 */

#include <linux/cgroup.h>
#include <linux/module.h>
#include <linux/types.h>
#include <trace/hooks/vmscan.h>
#include <trace/hooks/mm.h>
#include <linux/swap.h>
#include <linux/proc_fs.h>
#include <linux/gfp.h>
#include <linux/printk.h>
#include <linux/mmzone.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/namei.h>
#include <linux/pagemap.h>
#include <linux/page-flags.h>
#include <linux/debugfs.h>
#include <linux/memcontrol.h>
#include <linux/sched/mm.h>
#include <linux/file.h>
#include <linux/sysfs.h>
#include <linux/export.h>
#include <linux/kernfs.h>
#include <linux/kallsyms.h>
#include <linux/kprobes.h>
#include <linux/seq_file.h>
#include <trace/hooks/vendor_hooks.h>

#include "tranmem_cloud.h"
#include "page_prefilter_com.h"
#include "pf_common.h"
#include "inodes_protect.h"

#define TRANMM "[tranmem_pf]"
#define MAX_MAPCOUNT_THRESHOLD 20
#define MAX_BUF_LEN 16

#define ANTI_RECLAIM_MEMCG_NAME        "/antireclaim"
#define ANTI_RECLAIM_MEMCG_PATH_LEN    32

/****************************protection of swappiness****************************/
static bool dynamic_tune_enabled __read_mostly = true;
static int g_tune_swappiness __read_mostly = -1;
// The maxmum guaranteed minimum file page size is 4GB(4K x 1048576)
#define MAX_GUARANTEED_FILE_SIZE 1048576

//2G-524288, 1G-262144, 750M-192000, 650M-166400, 550M-140800
#define THRESHOLD_FILE_PAGES_4G    166400
#define THRESHOLD_FILE_PAGES_6G    262144
#define THRESHOLD_FILE_PAGES_8G    262144

static unsigned long g_file_pages_thres __read_mostly = THRESHOLD_FILE_PAGES_8G;

//count for tuning swappiness
static atomic64_t g_ts_cnts          = ATOMIC64_INIT(0);
static atomic64_t g_ts_skip_file     = ATOMIC64_INIT(0);
static atomic64_t g_ts_skip_anon     = ATOMIC64_INIT(0);
static atomic64_t g_ts_skip_freeswap = ATOMIC64_INIT(0);
/****************************protection of swappiness****************************/

/********************************memcg start*************************************/
#define PP_RESERVED_INDEX   1
#define PP_ENEABLED(memcg)  memcg->android_oem_data1[PP_RESERVED_INDEX]

#define PPE_SET_BIT(v,n)        (v | (1<<n))
#define PPE_CLR_BIT(v,n)        (v & ~(1<<n))
#define PPE_SET_ENABLE(v)       PPE_SET_BIT(v,0)
#define PPE_SET_DISABLE(v)      PPE_CLR_BIT(v,0)
#define PPE_SET_SPECIAL(v)      PPE_SET_BIT(v,1)
#define PPE_CLR_SPECIAL(v)      PPE_CLR_BIT(v,1)
#define PPE_SET_FILE_THD(v,t)   (v | (t << 2))
#define PPE_CLR_FILE_THD(v)     (v & 3)

#define IS_ANTI_MEMCG(v)        (v & 1)
#define IS_SPECIAL_MEMCG(v)     (v & 2)
#define GET_MEMCG_FILE_THD(v)   (v >> 2)

static bool special_memcg_enabled __read_mostly = false;
static int g_memcg_file_thd_percentage __read_mostly = 60;

static bool use_memcg_v2 = false;
static atomic64_t g_memcg_anti_reclaim = ATOMIC64_INIT(0);

typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
static kallsyms_lookup_name_t kallsyms_lookup_name_dup = NULL;

static struct static_key_true *memory_on_dfl_key_sym = NULL;

typedef int (*cgroup_add_dfl_cftypes_t)(struct cgroup_subsys *ss, struct cftype *cfts);
static cgroup_add_dfl_cftypes_t cgroup_add_dfl_cftypes_dup;

#define MEMCG_SPECIAL_LEN   2
static struct mem_cgroup *memcg_special[MEMCG_SPECIAL_LEN] = {0};
static DEFINE_MUTEX(memcg_special_lock);

//fthd: file threshold
static int g_special_memcg_fthd_def = -1;

/********************************memcg end***************************************/

enum folio_references {
	FOLIOREF_RECLAIM,
	FOLIOREF_RECLAIM_CLEAN,
	FOLIOREF_KEEP,
	FOLIOREF_ACTIVATE,
};

/*
 * The pre-filtering strategy contains multiple features and is subject to the limitations of cloud control.
 * 1 All functions are turned on
 * 2 Turn off the AI prediction function
 * 3 Turn off the anti-reclaim policy
 * 4 Turn off the entire filtering strategy
 */
enum page_filter {
	PAGE_FILTER_ON = 1,
	AI_RECLAIM_OFF,
	ANTI_RECLAIM_OFF,
	PAGE_FILTER_OFF,
};

static atomic_t folio_filter_setup = ATOMIC_INIT(0);

static unsigned long default_low_memory = 0;

/* System initialization page mapcount exceeds the threshold number */
static atomic64_t nr_folio_protect       = ATOMIC64_INIT(0);
static atomic64_t nr_folio_protect_thd   = ATOMIC64_INIT(0);
static atomic64_t nr_sfolio_protect      = ATOMIC64_INIT(0);
static atomic64_t nr_sfolio_protect_thd  = ATOMIC64_INIT(0);

/*************************DEBUG for memcg START**************************/
#define MEMCG_PRO_INFO_CNTS   20

static atomic_t   pf_debug_enabled = ATOMIC_INIT(0);
static atomic64_t nr_memcg_protect = ATOMIC64_INIT(0);

struct memcg_pro_info {
	char path[4096];
	int id;
	atomic64_t cnts;
};
static struct memcg_pro_info memcg_pro_arrs[MEMCG_PRO_INFO_CNTS];

static void clear_memcg_pro_info(struct memcg_pro_info *mpi)
{
	if (!mpi)
		return;

	mpi->id = -1;
	atomic64_set(&mpi->cnts, 0);
	memset(mpi->path, 0, sizeof(mpi->path));
}

static void init_memcg_pro_infos(void)
{
	int i = 0;

	for (; i < MEMCG_PRO_INFO_CNTS; ++i) {
		clear_memcg_pro_info(memcg_pro_arrs + i);
	}
}

static void find_and_update_debug(const struct mem_cgroup *memcg)
{
	int i = 0;
	if (!memcg || !atomic_read(&pf_debug_enabled))
		return;

	int id = memcg->id.id;
	for (; i < MEMCG_PRO_INFO_CNTS; ++i) {
		if (memcg_pro_arrs[i].id == -1) {
			memcg_pro_arrs[i].id = id;
			atomic64_inc(&memcg_pro_arrs[i].cnts);
			cgroup_path(memcg->css.cgroup, memcg_pro_arrs[i].path, 4096);
			break;
		} else if (memcg_pro_arrs[i].id == id) {
			atomic64_inc(&memcg_pro_arrs[i].cnts);
			break;
		}
	}
}

static void show_memcg_pro_infos(struct seq_file *m)
{
	int i = 0;

	for (; i < MEMCG_PRO_INFO_CNTS; ++i) {
		if (memcg_pro_arrs[i].id != -1)
			seq_printf(m, "memcg %5d: %10llu %s\n", memcg_pro_arrs[i].id, atomic64_read(&memcg_pro_arrs[i].cnts), memcg_pro_arrs[i].path);
	}
}
/*************************DEBUG for memcg END **************************/

static bool mem_available_is_low(void)
{
	long available = si_mem_available();

	if (available < default_low_memory)
		return true;

	return false;
}

static inline bool folio_evictable(struct folio *folio)
{
	bool ret;

	/* Prevent address_space of inode and swap cache from being freed */
	rcu_read_lock();
	ret = !mapping_unevictable(folio_mapping(folio)) && !folio_test_mlocked(folio);
	rcu_read_unlock();
	return ret;
}

static bool is_folio_mapcount_valid(struct folio *folio)
{
	if (likely(tran_folio_mapcount(folio) >= MAX_MAPCOUNT_THRESHOLD)) {
		return true;
	}

	return false;
}

/********************************memcg start***************************************/
static int check_special_memcg(const struct mem_cgroup *memcg, bool *exists)
{
	int i = 0;

	*exists = false;

	for (i = 0; i < MEMCG_SPECIAL_LEN; i++) {
		if (memcg_special[i] == memcg) {
			*exists = true;
			break;
		} else if (memcg_special[i] == NULL) {
			break;
		}
	}

	return i;
}

static int insert_new_special_memcg(struct mem_cgroup *memcg)
{
	bool memcg_exists = false;
	int idx = check_special_memcg(memcg, &memcg_exists);

	if (memcg_exists)
		return 0;

	if (idx < MEMCG_SPECIAL_LEN) {
		memcg_special[idx] = memcg;
		return 0;
	}

	for (idx = 1; idx < MEMCG_SPECIAL_LEN; idx++) {
		memcg_special[idx - 1] = memcg_special[idx];
	}
	memcg_special[MEMCG_SPECIAL_LEN - 1] = memcg;

	return 0;
}

static int get_memcg_path(const struct mem_cgroup *memcg, char *memcg_path, int len)
{
	struct cgroup *cgrp;
	int ret = -1;

	cgrp = memcg->css.cgroup;
	if (!cgrp) {
		return ret;
	}

	ret = kernfs_path_from_node(cgrp->kn, NULL, memcg_path, len);

	return ret;
}

static u64 read_pp_enabled(const struct mem_cgroup *memcg)
{
	return READ_ONCE(PP_ENEABLED(memcg));
}

/**
 * Used to write values to memory.pp.enabled in memcg.
 * If special memcg is enabled, write 2 must be before 1.
 * For more straightforward, clear the file THD when 0 unless it's a special memcg.
 */
static void write_pp_enabled(struct mem_cgroup *memcg, u64 val)
{
	u64 ori_value = read_pp_enabled(memcg);
	bool need_update = false;

	pr_info(TRANMM"write pp.enable from %llu", ori_value);

	mutex_lock(&memcg_special_lock);

	switch (val) {
	case 0:
		ori_value = PPE_SET_DISABLE(ori_value);    //disable

		if (!IS_SPECIAL_MEMCG(ori_value))          //if not special, clear the FHD
			ori_value = PPE_CLR_FILE_THD(ori_value);
		break;
	case 1:
		ori_value = PPE_SET_ENABLE(ori_value);     //enable
		break;
	case 2:
		ori_value = PPE_SET_SPECIAL(ori_value);    //set special
		if (g_special_memcg_fthd_def >= 0) {       //debugging... modify the fthd.
			ori_value = PPE_CLR_FILE_THD(ori_value);
			ori_value = PPE_SET_FILE_THD(ori_value, g_special_memcg_fthd_def);
		}
		need_update = true;
		break;
	case 3:
		ori_value = PPE_CLR_SPECIAL(ori_value);    //clear special and it's file threshold
		ori_value = PPE_CLR_FILE_THD(ori_value);
		break;
	default:                                       //set file thd with bytes, max size 1G
		if (g_special_memcg_fthd_def < 0 || !IS_SPECIAL_MEMCG(ori_value)) {
			val = val >> PAGE_SHIFT;
			if (val > 262144)
				val = 262144;
			val = val * g_memcg_file_thd_percentage / 100;

			ori_value = PPE_CLR_FILE_THD(ori_value);
			ori_value = PPE_SET_FILE_THD(ori_value, val);
		}
		break;
	}

	mutex_unlock(&memcg_special_lock);

	WRITE_ONCE(PP_ENEABLED(memcg), ori_value);

	if (need_update) {  //update special array
		mutex_lock(&memcg_special_lock);

		insert_new_special_memcg(memcg);

		mutex_unlock(&memcg_special_lock);
	}

	pr_info("to %llu\n", ori_value);
}

static int pp_enabled_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%llu\n", read_pp_enabled(mem_cgroup_from_seq(m)));

	return 0;
}

static ssize_t pp_enabled_write(struct kernfs_open_file *of,
			       char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	u64 bytes;
	char *end;

	buf = strstrip(buf);
	bytes = memparse(buf, &end);
	if (*end != '\0')
		return -EINVAL;

	write_pp_enabled(memcg, bytes);

	return nbytes;
}

static struct cftype tran_pp_files[] = {
	{
		.name = "pp.enabled",
		.flags = CFTYPE_NOT_ON_ROOT | CFTYPE_WORLD_WRITABLE,
		.seq_show = pp_enabled_show,
		.write = pp_enabled_write,
	},
	{ }	/* terminate */
};

static void add_pp_memcg_files(void)
{
	if (cgroup_add_dfl_cftypes_dup)
		WARN_ON(cgroup_add_dfl_cftypes_dup(&memory_cgrp_subsys, tran_pp_files));
}

static bool is_memcg_low_fthd(struct mem_cgroup *memcg)
{
	u64 memcg_file_thd = GET_MEMCG_FILE_THD(read_pp_enabled(memcg));

	return memcg_file_thd && memcg_file_thd > tran_memcg_page_state(memcg, NR_FILE_PAGES);
}

static bool hit_special_memcg(const struct mem_cgroup *memcg)
{
	bool memcg_exists = false;

	if (unlikely(!special_memcg_enabled))
		return false;

	if (!use_memcg_v2)
		return false;

	check_special_memcg(memcg, &memcg_exists);

	return memcg_exists && IS_SPECIAL_MEMCG(read_pp_enabled(memcg));
}

static bool hit_folio_memcg(struct mem_cgroup *memcg)
{
	struct mem_cgroup *anti = NULL;
	char memcg_path[ANTI_RECLAIM_MEMCG_PATH_LEN] = {0};

	if (use_memcg_v2) {
		return IS_ANTI_MEMCG(read_pp_enabled(memcg));
	}

	anti = (struct mem_cgroup*)(long)atomic64_read(&g_memcg_anti_reclaim);
	if(!anti && get_memcg_path(memcg, memcg_path, ANTI_RECLAIM_MEMCG_PATH_LEN) > 0) {
		if (!strncmp(memcg_path, ANTI_RECLAIM_MEMCG_NAME, ANTI_RECLAIM_MEMCG_PATH_LEN)) { //find the memcg
			atomic64_set(&g_memcg_anti_reclaim, (long)memcg);
			anti = memcg;
		}
	}

	return anti == memcg;
}

static kprobe_opcode_t *get_kallsyms_lookup_addr(const char *symbol_name)
{
	int ret = 0;
	kprobe_opcode_t * temp_kallsyms_addr = NULL;

	struct kprobe kallsyms_lookup_kp = {
		.symbol_name = symbol_name
	};

	ret = register_kprobe(&kallsyms_lookup_kp);
	if (ret < 0) {
		pr_err(TRANMM"register_kprobe kallsyms_lookup_name failed, returned %d\n", ret);
		return NULL;
	}

	temp_kallsyms_addr = kallsyms_lookup_kp.addr;
	unregister_kprobe(&kallsyms_lookup_kp);

	return temp_kallsyms_addr;
}

static bool get_kallsyms_lookup_name_addr(void)
{
	if (unlikely(!kallsyms_lookup_name_dup)) {
		kallsyms_lookup_name_dup = (kallsyms_lookup_name_t)get_kallsyms_lookup_addr("kallsyms_lookup_name");
		if (!kallsyms_lookup_name_dup) {
			pr_err(TRANMM"Can't find function kallsyms_lookup_name.\n");
			return false;
		}
	}

	return true;
}

bool get_symbol_address(const char *symbol_name, unsigned long *address)
{
	if (!get_kallsyms_lookup_name_addr() || !symbol_name || !address)
		return false;

	*address = kallsyms_lookup_name_dup(symbol_name);
	if (!*address) {
		pr_err(TRANMM"can't find symbol %s.\n", symbol_name);
		return false;
	}

	return true;
}

static int pp_memcg_early_init(void)
{
	if (!get_kallsyms_lookup_name_addr())
		return -1;

	memory_on_dfl_key_sym = (struct static_key_true*)kallsyms_lookup_name_dup("memory_cgrp_subsys_on_dfl_key");
	if (!memory_on_dfl_key_sym) {
		pr_err(TRANMM"can't find symbol memory_cgrp_subsys_on_dfl_key.\n");
		return -1;
	}
	pr_info(TRANMM"memory_on_dfl_key_sym address = %px\n", memory_on_dfl_key_sym);

	return 0;
}

static void check_memcg_version(void)
{
	if (pp_memcg_early_init() < 0)
		return;

	if (static_key_enabled(&memory_on_dfl_key_sym->key))
		use_memcg_v2 = true;

	pr_info(TRANMM"memcg version is %d\n", use_memcg_v2);
}

static int dfl_cftypes_init(void)
{
	cgroup_add_dfl_cftypes_dup = (cgroup_add_dfl_cftypes_t)get_kallsyms_lookup_addr("cgroup_add_dfl_cftypes");
	if (!cgroup_add_dfl_cftypes_dup) {
		pr_err(TRANMM"Can't find function cgroup_add_dfl_cftypes.\n");
		return -1;
	}
	pr_info(TRANMM"cgroup_add_dfl_cftypes address = %px\n", cgroup_add_dfl_cftypes_dup);

	return 0;
}
/********************************memcg end***************************************/

/*
 * Refer to the fast reclaim path of file folio and the drop_cache reclaim process,
 * and appropriately avoid the reclaim of some mapped file folio when reclaim file folio.
 *
 * return true Prevents the folio from being reclaimed
 * return false Don't prevents the folio from being reclaimed.
 */
static bool folio_anti_reclaim(struct folio *folio)
{
	int file;
	struct mem_cgroup *memcg = NULL;

	file = folio_is_file_lru(folio);
	if (!file)
		return false;

	memcg = folio_memcg(folio);
	if (!is_valid_memcg(memcg)) {
		return false;
	}

	if (folio_protected_by_inode(folio))
		return true;

	if (hit_folio_memcg(memcg)) {
		if (is_folio_mapcount_valid(folio)) {
			atomic64_inc(&nr_folio_protect);
			return true;
		}

		if (is_memcg_low_fthd(memcg)) {
			atomic64_inc(&nr_folio_protect_thd);
			return true;
		}
	}

	if (hit_special_memcg(memcg)) {
		if (is_folio_mapcount_valid(folio)) {
			atomic64_inc(&nr_sfolio_protect);
			return true;
		}

		if (is_memcg_low_fthd(memcg)) {
			atomic64_inc(&nr_sfolio_protect_thd);
			return true;
		}
	}

	return false;
}

static void memcg_should_be_protect(void *data, struct mem_cgroup *memcg, int priority, bool *bypass)
{
	*bypass = false;

	if (unlikely(atomic_read(&folio_filter_setup) == PAGE_FILTER_OFF))
		return;

	if (!memcg || memcg->id.id <= 0 || priority <= DEF_PRIORITY - 2)
		return;

	if (unlikely(mem_available_is_low()))
		return;

	if (hit_folio_memcg(memcg)) {
		*bypass = true;

		atomic64_inc(&nr_memcg_protect);

		find_and_update_debug(memcg);
	}
}

/* If the folio is mapped by too many processes,
 * the hot rate will be higher, and this folio should be resident in
 * the active list at this time.
 */
static void folio_should_be_protect(void *data, struct folio *folio, unsigned long nr_scanned,
										s8 priority, u64 *ext, int *should_protect)
{
	*should_protect = 0;

	if (unlikely(atomic_read(&folio_filter_setup) == PAGE_FILTER_OFF)) {
		return;
	}

	if (unlikely(!folio_evictable(folio) || folio_test_unevictable(folio))) {
		return;
	}

	if (unlikely(mem_available_is_low())) {
		return;
	}

	//last check
	if (unlikely(!folio_anti_reclaim(folio))) {
		return;
	}

	if (folio_test_referenced(folio)) {
		*should_protect = FOLIOREF_ACTIVATE;
	} else {
		folio_set_referenced(folio);
		*should_protect = FOLIOREF_KEEP;
	}
}

/*
 * When system memory falls below the threshold water level,
 * Increasing swappiness can increase the memory reclaim of
 * anonymous folio and swap more memory for the system.
 */
static void dynamic_adjust_swappiness(void *data, int *swappiness)
{
	struct sysinfo val_swap;
	unsigned long nr_file_folio = 0;
	unsigned long nr_anon_folio = 0;

	if (unlikely(atomic_read(&folio_filter_setup) == PAGE_FILTER_OFF))
		return;

	if (!dynamic_tune_enabled)
		return;

	if (g_tune_swappiness < 0 || g_file_pages_thres <= 0)
		return;

	if (!current_is_kswapd())
		return;

	nr_file_folio = global_node_page_state(NR_ACTIVE_FILE) + global_node_page_state(NR_INACTIVE_FILE);
	nr_anon_folio = global_node_page_state(NR_ACTIVE_ANON) + global_node_page_state(NR_INACTIVE_ANON);

	if (nr_file_folio >= g_file_pages_thres) {              //Enough file pages
		atomic64_inc(&g_ts_skip_file);
		return;
	}

	if (nr_anon_folio * 100 < g_file_pages_thres * 70) {    //Non-enouth anon pages
		atomic64_inc(&g_ts_skip_anon);
		return;
	}

	si_swapinfo(&val_swap);

	if (val_swap.freeswap * 10 <= val_swap.totalswap) {     //Non-enough free swap
		atomic64_inc(&g_ts_skip_freeswap);
		return;
	}

	*swappiness = g_tune_swappiness;
	atomic64_inc(&g_ts_cnts);
}

static int register_folio_prefilter_vendor_hooks(void)
{
	int ret = 0;

	pr_info(TRANMM"register_folio_prefilter_vendor_hooks.\n");

	ret = register_trace_android_vh_tune_swappiness(dynamic_adjust_swappiness, NULL);
	if (ret != 0) {
		pr_err("register dynamic_adjust_swappiness failed! ret=%d\n", ret);
		goto out1;
	}

	ret = register_trace_android_vh_page_should_be_protected(folio_should_be_protect, NULL);
	if (ret != 0) {
		pr_err("register folio_should_be_protect vendor_hooks failed! ret=%d\n", ret);
		goto out2;
	}

	ret = register_trace_android_vh_should_memcg_bypass(memcg_should_be_protect, NULL);
	if (ret != 0) {
		pr_err("register memcg_should_be_protect vendor_hooks failed! ret=%d\n", ret);
		goto out3;
	}

	return ret;

out3:
	unregister_trace_android_vh_page_should_be_protected(folio_should_be_protect, NULL);

out2:
	unregister_trace_android_vh_tune_swappiness(dynamic_adjust_swappiness, NULL);
out1:
	return ret;
}

static void unregister_folio_prefilter_vendor_hooks(void)
{
	unregister_trace_android_vh_should_memcg_bypass(memcg_should_be_protect, NULL);
	unregister_trace_android_vh_page_should_be_protected(folio_should_be_protect, NULL);
	unregister_trace_android_vh_tune_swappiness(dynamic_adjust_swappiness, NULL);

	return;
}

static void init_tune_swappiness(void)
{
	int total_gb = totalram_pages() >> (30 - PAGE_SHIFT);
	total_gb += 1;
	g_tune_swappiness = DEF_DYNAMIC_SWAPPINESS;

	switch (total_gb) {
	case 4:
		g_file_pages_thres = THRESHOLD_FILE_PAGES_4G;
		break;
	case 6:
		g_file_pages_thres = THRESHOLD_FILE_PAGES_6G;
		break;
	case 8:
		g_file_pages_thres = THRESHOLD_FILE_PAGES_8G;
		break;
	default:
		break;
	}
}

/**************************Cloud START***********************************/
bool update_dynamic_tune_enabled(bool enabled)
{
	bool old = dynamic_tune_enabled;
	dynamic_tune_enabled = enabled;

	return old;
}

int update_dynamic_tune_swappiness(int swappiness)
{
	int old = (int)g_tune_swappiness;

	if (swappiness >= 0 && swappiness <= TRAN_MAX_SWAPPINESS)
		g_tune_swappiness = swappiness;

	return old;
}

int  update_dynamic_pp_threshold(int threshold)
{
	int old = (int)g_file_pages_thres;

	if (threshold < MAX_GUARANTEED_FILE_SIZE) {
		g_file_pages_thres = threshold;
	}

	return old;
}

bool update_special_memcg_enabled(bool enabled)
{
	bool old = special_memcg_enabled;
	special_memcg_enabled = enabled;

	return old;
}

int update_memcg_file_thd_percentage(int percentage)
{
	int old = g_memcg_file_thd_percentage;

	if (percentage <= 0)
		g_memcg_file_thd_percentage = 0;
	else
		g_memcg_file_thd_percentage = percentage;

	return old;
}
/**************************Cloud END***********************************/

/**************************Files ops START***********************************/

static int pf_protect_debug_show(struct seq_file *m, void *arg)
{
	seq_printf(m, " nr_folio_protect:    %llu\n", atomic64_read(&nr_folio_protect));
	seq_printf(m, "nr_folio_pro_fthd:    %llu\n", atomic64_read(&nr_folio_protect_thd));
	seq_printf(m, " percent_pro_fthd:    %d\n", g_memcg_file_thd_percentage);
	seq_printf(m, " nr_memcg_protect:    %llu\n", atomic64_read(&nr_memcg_protect));

	if (atomic_read(&pf_debug_enabled)) {
		show_memcg_pro_infos(m);
	} else {
		seq_printf(m, "memcg_protect_dbg:    disabled(echo 1 to enable)\n");
	}

	seq_putc(m, '\n');

	return 0;
}

static int pf_protect_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, pf_protect_debug_show, inode->i_private);
}

static ssize_t pf_protect_debug_write(struct file *file, const char __user *ubuf, size_t len, loff_t *ppos)
{
	int ret;
	char kbuf[MAX_BUF_LEN] = {'\0'};
	char *str;
	int value;

	if (len > MAX_BUF_LEN - 1) {
		return -EINVAL;
	}

	if (copy_from_user(&kbuf, ubuf, len))
		return -EFAULT;
	kbuf[len] = '\0';

	str = strstrip(kbuf);
	if (!str) {
		return -EINVAL;
	}

	ret = kstrtoint(str, 10, &value);
	if (ret) {
		return -EINVAL;
	}

	if (value != 0 && value != 1)
		return -EINVAL;

	if (value != atomic_read(&pf_debug_enabled)) {
		if (value)
			init_memcg_pro_infos();

		atomic_set(&pf_debug_enabled, value);
	}

	return len;
}

static const struct proc_ops pf_protect_debug_ops = {
	.proc_open           = pf_protect_debug_open,
	.proc_read           = seq_read,
	.proc_write          = pf_protect_debug_write,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

static int pf_state_show(struct seq_file *m, void *arg)
{
	seq_printf(m, "page_filter_state:     %d:%d\n", atomic_read(&folio_filter_setup), use_memcg_v2 ? 2 : 1);
	seq_putc(m, '\n');

	return 0;
}

static int pf_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, pf_state_show, inode->i_private);
}

static ssize_t pf_state_write(struct file *file, const char __user *ubuf, size_t len, loff_t *ppos)
{
	int ret;
	char kbuf[MAX_BUF_LEN] = {'\0'};
	char *str;
	int value;

	if (len > MAX_BUF_LEN - 1) {
		return -EINVAL;
	}

	if (copy_from_user(&kbuf, ubuf, len))
		return -EFAULT;
	kbuf[len] = '\0';

	str = strstrip(kbuf);
	if (!str) {
		return -EINVAL;
	}

	ret = kstrtoint(str, 10, &value);
	if (ret) {
		return -EINVAL;
	}

	if (value <= 0 || value > INT_MAX) {
		return -EINVAL;
	}

	printk("folio_filter_state_write is %d\n", value);
	atomic_set(&folio_filter_setup, value);

	return len;
}

static const struct proc_ops pf_state_ops = {
	.proc_open           = pf_state_open,
	.proc_write          = pf_state_write,
	.proc_read           = seq_read,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

static ssize_t tune_swappiness_write(struct file *file,const char __user *ubuf,size_t count,loff_t *pos)
{
	char kbuf[36]={'\0'};
	int tune_swappiness = -1;
	unsigned long minimun_file_size = 0;

	if (count >= 36 || count <= 0) {
		pr_err("tune_swappiness set failed for count exceeds limit!\n");
		return count;
	}

	if(copy_from_user(kbuf, ubuf, count))
		return -EFAULT;

	sscanf(kbuf, "%d:%lu", &tune_swappiness, &minimun_file_size);
	if ((minimun_file_size>>PAGE_SHIFT) > MAX_GUARANTEED_FILE_SIZE)
		return -EFAULT;

	if (tune_swappiness > TRAN_MAX_SWAPPINESS)
		return -EFAULT;

	g_tune_swappiness = tune_swappiness;
	//g_tune_swappiness = DEF_DYNAMIC_SWAPPINESS;
	g_file_pages_thres = minimun_file_size >> PAGE_SHIFT;

	return count;
}

static int tune_swappiness_show(struct seq_file *m, void *v)
{
	seq_printf(m, "tune_swappiness:     %d\n", g_tune_swappiness);
	seq_printf(m, "file_threshold(KB):  %lu\n", g_file_pages_thres << 2);
	seq_printf(m, "tune_enabled:        %d\n", dynamic_tune_enabled);
	seq_printf(m, "tune_cnts:           %llu\n", atomic64_read(&g_ts_cnts));
	seq_printf(m, "skip_cnts_file       %llu\n", atomic64_read(&g_ts_skip_file));
	seq_printf(m, "skip_cnts_anon       %llu\n", atomic64_read(&g_ts_skip_anon));
	seq_printf(m, "skip_cnts_swapfree   %llu\n", atomic64_read(&g_ts_skip_freeswap));

	return 0;
}

static int tune_swappiness_open(struct inode *inode, struct file *file)
{
	return single_open(file, tune_swappiness_show, NULL);
}

static const struct proc_ops tune_swappiness_fops = {
    .proc_open = tune_swappiness_open,
    .proc_read = seq_read,
    .proc_write = tune_swappiness_write,
    .proc_release = single_release,
};

static ssize_t memcg_specials_write(struct file *file, const char __user *ubuf, size_t count, loff_t *pos)
{
	int ret;
	char kbuf[MAX_BUF_LEN] = {0};
	char *str;
	int value;

	if (count > MAX_BUF_LEN - 1) {
		return -EINVAL;
	}

	if (copy_from_user(&kbuf, ubuf, count))
		return -EFAULT;
	kbuf[count] = '\0';

	str = strstrip(kbuf);
	if (!str) {
		return -EINVAL;
	}

	ret = kstrtoint(str, 10, &value);
	if (ret) {
		return -EINVAL;
	}

	g_special_memcg_fthd_def = value;

	return count;
}

static int memcg_specials_show(struct seq_file *m, void *v)
{
	seq_printf(m, "  special_enabled:    %d\n", special_memcg_enabled);
	seq_printf(m, "  def_special_thd:    %d\n", g_special_memcg_fthd_def);
	seq_printf(m, "nr_sfolio_protect:    %llu\n", atomic64_read(&nr_sfolio_protect));
	seq_printf(m, "nr_sfolio_pro_thd:    %llu\n", atomic64_read(&nr_sfolio_protect_thd));

	return 0;
}

static int memcg_specials_open(struct inode *inode, struct file *file)
{
	return single_open(file, memcg_specials_show, NULL);
}

static const struct proc_ops memcg_specials_fops = {
    .proc_open = memcg_specials_open,
    .proc_read = seq_read,
    .proc_write = memcg_specials_write,
    .proc_release = single_release,
};

/**************************Files ops END ***********************************/

static int __init folio_pre_filter_init(void)
{
	int ret = 0;

	struct proc_dir_entry *folio_prefilter_dir = NULL;
	folio_prefilter_dir = proc_mkdir("pages_prefiltered", NULL);
	if (folio_prefilter_dir == NULL){
		pr_err("proc create page_prefiltered failed\n");
		return -1;
	}

	ret = register_folio_prefilter_vendor_hooks();
	if (ret != 0)
		return ret;

	//invoke as early as possible, kallsyms_lookup_name might be needed.
	kallsyms_lookup_name_dup = NULL;
	get_kallsyms_lookup_name_addr();

	 dfl_cftypes_init();

	init_tune_swappiness();

	default_low_memory = totalram_pages() / 5;

	inodes_protect_init();

	inodes_protect_proc_create(folio_prefilter_dir);
	proc_create("page_filter_protect_debug", 0666, folio_prefilter_dir, &pf_protect_debug_ops);
	proc_create("page_filter_state", 0666, folio_prefilter_dir, &pf_state_ops);
	proc_create("tune_swappiness", 0666, folio_prefilter_dir, &tune_swappiness_fops);
	proc_create("memcg_specials", 0666, folio_prefilter_dir, &memcg_specials_fops);

	/* All functions are turned on */
	atomic_set(&folio_filter_setup, 1);

	check_memcg_version();
	add_pp_memcg_files();

	tranmem_cloudengine_init();

	pr_info("folio_pre_filter_init succeed!\n");

	return 0;
}

static void __exit folio_pre_filter_exit(void)
{
	unregister_folio_prefilter_vendor_hooks();

	tranmem_cloudengine_exit();

	inodes_protect_exit();

	pr_info("folio_pre_filter_exit succeed!\n");

	return;
}

module_init(folio_pre_filter_init);
module_exit(folio_pre_filter_exit);

MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(MINIDUMP);
