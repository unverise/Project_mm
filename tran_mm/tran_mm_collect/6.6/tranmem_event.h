/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 transsion Inc.
 */
#if !defined(__TRANMEM_EVENT_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __TRANMEM_EVENT_H__

#include <linux/types.h>
#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM trans_mem


TRACE_EVENT(tranmem_workingset_refault,

	TP_PROTO(u64 folio_addr),

	TP_ARGS(folio_addr),

	TP_STRUCT__entry(
		__field(u64,    folio)
	),

	TP_fast_assign(
		__entry->folio     =  folio_addr;
	),

	TP_printk("folio=%llu", __entry->folio)
);

TRACE_EVENT(tranmem_lruvec_state,

	TP_PROTO(int folio_state),

	TP_ARGS(folio_state),

	TP_STRUCT__entry(
		__field(int,    idx)
	),

	TP_fast_assign(
		__entry->idx     =  folio_state;
	),

	TP_printk("idx=%d", __entry->idx)
);

TRACE_EVENT(tranmem_ioblock,

	TP_PROTO(u64 folio_addr),

	TP_ARGS(folio_addr),

	TP_STRUCT__entry(
		__field(u64,    folio)
	),

	TP_fast_assign(
		__entry->folio     =  folio_addr;
	),

	TP_printk("folio=%llu", __entry->folio)
);


#endif /*__TRANMEM_EVENT_H__ */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE tranmem_event

/* This part must be outside protection */
#include <trace/define_trace.h>

