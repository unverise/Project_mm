#include "transsion_core.h"
//#include "transsion_gesture.h"
#include "transsion_thp_interface.h"

#ifndef CREATE_TRACE_POINTS
#define CREATE_TRACE_POINTS
#endif
#include "ts_debug.h"


void tran_ts_trace_queue_event_cnt(uint8_t hh, uint8_t tt, uint64_t drive_frame_cnt,
        uint32_t loss_frame_cnt, uint32_t cache_frame_cnt)
{
    trace_queue_event_cnt(hh, tt, drive_frame_cnt, loss_frame_cnt, cache_frame_cnt);
}
EXPORT_SYMBOL(tran_ts_trace_queue_event_cnt);
void tran_ts_trace_queue_event_lost(uint8_t hh, uint8_t tt, uint64_t drive_frame_cnt,
        uint32_t loss_frame_cnt, uint32_t cache_frame_cnt)
{
    trace_queue_event_lost(hh, tt, drive_frame_cnt, loss_frame_cnt, cache_frame_cnt);
}
EXPORT_SYMBOL(tran_ts_trace_queue_event_lost);
void tran_ts_trace_queue_event_cache(uint8_t hh, uint8_t tt, uint64_t drive_frame_cnt,
        uint32_t loss_frame_cnt, uint32_t cache_frame_cnt)
{
    trace_queue_event_cache(hh, tt, drive_frame_cnt, loss_frame_cnt, cache_frame_cnt);
}
EXPORT_SYMBOL(tran_ts_trace_queue_event_cache);

void tran_ts_trace_touchscreen_report(int x, int y, int touch_id, int flag)
{
    trace_touchscreen_report(x, y, touch_id, flag);
}
EXPORT_SYMBOL(tran_ts_trace_touchscreen_report);

void tran_ts_trace_drv_poll_cnt(int poll_cnt)
{
    trace_drv_poll_cnt(poll_cnt);
}
EXPORT_SYMBOL(tran_ts_trace_drv_poll_cnt);

