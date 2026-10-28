#ifndef __TRANMEM_PAGE_PREFILTER_COMMON_H__
#define __TRANMEM_PAGE_PREFILTER_COMMON_H__

#include <linux/memcontrol.h>

static inline bool tran_mem_cgroup_online(struct mem_cgroup *memcg)
{
        return memcg && !!(memcg->css.flags & CSS_ONLINE);
}

static inline bool tran_mem_cgroup_visible(struct mem_cgroup *memcg)
{
        return memcg && !!(memcg->css.flags & CSS_VISIBLE);
}

static inline bool is_valid_memcg(struct mem_cgroup *memcg)
{
    return tran_mem_cgroup_online(memcg) && tran_mem_cgroup_visible(memcg);
}

/**
 * try to get the address of a symbol.
 * @param name: symbol name;
 * @param address: the address of symbol name.
 * @return: true get successfully, false otherwise.
 */
extern bool get_symbol_address(const char *name, unsigned long *address);

#endif  //__TRANMEM_PAGE_PREFILTER_COMMON_H__