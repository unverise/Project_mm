/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ts_debug

#if !defined(_TRACE_TS_DEBUG_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TS_DEBUG_H

#include <linux/tracepoint.h>
//#include <linux/trace_events.h>

TRACE_EVENT(touchscreen_report,
    TP_PROTO(int x, int y, int touch_id, int flag),
    TP_ARGS(x, y, touch_id, flag),
    TP_STRUCT__entry(
        __field(int, x)
        __field(int, y)
        __field(int, touch_id)
        __field(int, flag)
    ),
    TP_fast_assign(
        __entry->x = x;
        __entry->y = y;
        __entry->touch_id = touch_id;
        __entry->flag = flag;
    ),
    TP_printk("x=%d y=%d touch_id=%d flag=%d",
        __entry->x, __entry->y, __entry->touch_id, __entry->flag)
);
DECLARE_EVENT_CLASS(drv_poll,
    TP_PROTO(int poll_cnt),
    TP_ARGS(poll_cnt),
    TP_STRUCT__entry(
        __field(int, poll_cnt)
    ),
    TP_fast_assign(
        __entry->poll_cnt = poll_cnt;
    ),
    TP_printk("poll_cnt=%d", __entry->poll_cnt)
);

DEFINE_EVENT(drv_poll, drv_poll_cnt,
    TP_PROTO(int poll_cnt),
    TP_ARGS(poll_cnt)
);
DEFINE_EVENT(drv_poll, drv_poll_wait_start,
    TP_PROTO(int poll_cnt),
    TP_ARGS(poll_cnt)
);
DEFINE_EVENT(drv_poll, drv_poll_wait_end,
    TP_PROTO(int poll_cnt),
    TP_ARGS(poll_cnt)
);

DECLARE_EVENT_CLASS(queue_event,
    TP_PROTO(uint8_t hh, uint8_t tt, uint64_t drive_frame_cnt, uint32_t loss_frame_cnt, uint32_t cache_frame_cnt),
    TP_ARGS(hh, tt, drive_frame_cnt, loss_frame_cnt, cache_frame_cnt),
    TP_STRUCT__entry(
        __field(uint8_t, hh)
        __field(uint8_t, tt)
        __field(uint64_t, drive_frame_cnt)
        __field(uint32_t, loss_frame_cnt)
        __field(uint32_t, cache_frame_cnt)
    ),
    TP_fast_assign(
        __entry->hh = hh;
        __entry->tt = tt;
        __entry->drive_frame_cnt = drive_frame_cnt;
        __entry->loss_frame_cnt = loss_frame_cnt;
        __entry->cache_frame_cnt = cache_frame_cnt;
    ),
    TP_printk("head=%d tail=%d drive_frame_cnt=%llu loss_frame_cnt=%u cache_frame_cnt=%u",
        __entry->hh, __entry->tt, __entry->drive_frame_cnt, __entry->loss_frame_cnt, __entry->cache_frame_cnt)
);
DEFINE_EVENT(queue_event, queue_event_lost,
    TP_PROTO(uint8_t hh, uint8_t tt, uint64_t drive_frame_cnt, uint32_t loss_frame_cnt, uint32_t cache_frame_cnt),
    TP_ARGS(hh, tt, drive_frame_cnt, loss_frame_cnt, cache_frame_cnt)
);
DEFINE_EVENT(queue_event, queue_event_cnt,
    TP_PROTO(uint8_t hh, uint8_t tt, uint64_t drive_frame_cnt, uint32_t loss_frame_cnt, uint32_t cache_frame_cnt),
    TP_ARGS(hh, tt, drive_frame_cnt, loss_frame_cnt, cache_frame_cnt)
);
DEFINE_EVENT(queue_event, queue_event_cache,
    TP_PROTO(uint8_t hh, uint8_t tt, uint64_t drive_frame_cnt, uint32_t loss_frame_cnt, uint32_t cache_frame_cnt),
    TP_ARGS(hh, tt, drive_frame_cnt, loss_frame_cnt, cache_frame_cnt)
);

#endif /* _TRACE_TS_DEBUG_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE ts_debug
/* This part must be outside protection */
#include <trace/define_trace.h>
