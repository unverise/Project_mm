#include <linux/mm.h>

#include "page_prefilter_com.h"

int tran_folio_mapcount(struct folio *folio)
{
	return folio_mapcount(folio);
}

unsigned long tran_memcg_page_state(struct mem_cgroup *memcg, int idx)
{
	return memcg_page_state(memcg, idx);
}
