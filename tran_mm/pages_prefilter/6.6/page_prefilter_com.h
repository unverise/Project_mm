#ifndef __TRANMEM_PAGE_PREFILTER_COMPATIBILITY_H__
#define __TRANMEM_PAGE_PREFILTER_COMPATIBILITY_H__

#include <linux/memcontrol.h>
#include <linux/mm_types.h>
#include <linux/swap.h>

/***************** function for different version START *****************/
#define TRAN_MAX_SWAPPINESS        (200  + 1)
#define DEF_DYNAMIC_SWAPPINESS     200

//folio_mapcount()
extern int tran_folio_mapcount(struct folio *folio);

//memcg_page_state
extern unsigned long tran_memcg_page_state(struct mem_cgroup *memcg, int idx);

/***************** function for different version END *******************/

#endif   //__TRANMEM_PAGE_PREFILTER_COMPATIBILITY_H__
