#include <linux/mm.h>

#include "page_prefilter_com.h"

int tran_folio_mapcount(struct folio *folio)
{
	int mapcount;
	if (likely(!folio_test_large(folio))) {
		mapcount = atomic_read(&folio->_mapcount) + 1;
		if (page_mapcount_is_type(mapcount))
			mapcount = 0;
		return mapcount;
	}
	return folio_large_mapcount(folio);
}

unsigned long tran_memcg_page_state(struct mem_cgroup *memcg, int idx)
{
	return memcg_page_state(memcg, idx);
}
