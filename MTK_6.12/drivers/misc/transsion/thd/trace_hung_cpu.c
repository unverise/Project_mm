/*Transsion Top Secret*/
/*
 * Copyright (C) 2023 Transsion Inc.
 *
 * @author chonglin.yang
 */

#include <trace/hooks/softlockup.h>
#include <linux/printk.h>
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
#include <tk/tnek.h>
#endif
#include "thd_util.h"

/*void watchdog_timer_softlockup (void *data, int duration, struct pt_regs *regs, bool is_panic) {
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
	tnek_caller_api("0xffffff33", FLAG_TNE_USER_BASIC | FLAG_TNE_USER_LOG_CRASH | FLAG_TNE_USER_LOG_MAIN_SYS | FLAG_TNE_USER_LOG_EVENT | FLAG_TNE_USER_LOG_RADIO | FLAG_TNE_USER_LOG_KERNEL | FLAG_TNE_USER_LAST_KMSG  | FLAG_TNE_USER_CPU_INFO | FLAG_TNE_USER_PROPERTY, 0, "");
#endif
}*/

bool trace_hung_cpu_init(void) {
	/*int ret = 0;
	ret = register_trace_android_vh_watchdog_timer_softlockup(watchdog_timer_softlockup,NULL);
	if(ret != 0){
		printk("register_trace_android_vh_watchdog_timer_softlockup failed.\n");
		return false;
	}*/
	return true;
}
void trace_hung_cpu_exit(void) {
	//unregister_trace_android_vh_watchdog_timer_softlockup(watchdog_timer_softlockup,NULL);
}
