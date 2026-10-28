#ifndef __TRAN_POWER_CTRL__
#define __TRAN_POWER_CTRL__

extern void tran_suspend_info_increment(const char *idle,const char *wakeup,const int time_suspend,const int time_26M);
extern void tran_block_system_count_increment(const char *name);
#endif