/*Transsion Top Secret*/
/*
 * Copyright (C) 2023 Transsion Inc.
 *
 * @author Qiwu.Chen
 */
#include "tranlog.h"

extern int register_cloudengine_notifier(const char* id, const char* ver, struct notifier_block *nb);
extern int unregister_cloudengine_notifier(const char* id, struct notifier_block *nb);
extern void cloudengine_feed_back(const char* id, int state);
extern char* cloudengine_getConfig(const char* id);
extern char* cloudengine_getStringByContent(char* content, const char* key);
extern int cloudengine_getIntByContent(char* content, const char* key);
extern char* cloudengine_getStringByID(const char* id, const char* key);
extern int cloudengine_getIntByID(const char* id, const char* key);

extern void dump_user_stacktrace(struct pt_regs *regs, struct task_struct *tsk, const char *loglvl);
const static int64_t FLAG_TNE_USER_LOG_MAIN_SYS = 1 << 1; //mobile log:android and system log
const static int64_t FLAG_TNE_USER_LOG_EVENT = 1 << 2;
const static int64_t FLAG_TNE_USER_LOG_RADIO = 1 << 3;
const static int64_t FLAG_TNE_USER_LOG_CRASH = 1 << 4;
const static int64_t FLAG_TNE_USER_LOG_KERNEL = 1 << 5;
const static int64_t FLAG_TNE_USER_ANR_TRACE = 1 << 6; //ams dumptrace ,begin with anr_
const static int64_t FLAG_TNE_USER_BINDER = 1 << 7;
const static int64_t FLAG_TNE_USER_MAPS = 1 << 8;
const static int64_t FLAG_TNE_USER_SIGNAL_TRACE = 1 << 9; //signal catcher trace, signal 3 begin with trace_
const static int64_t FLAG_TNE_USER_NATIVE_TRACE = 1<< 10; //debuggerd for native trace
const static int64_t FLAG_TNE_USER_LAST_KMSG = 1<< 11; //
const static int64_t FLAG_TNE_USER_PS = 1<<12;
const static int64_t FLAG_TNE_USER_PROC_MEMINFO = 1<<13;
const static int64_t FLAG_TNE_USER_ZONE_INFO = 1<<14;
const static int64_t FLAG_TNE_USER_BLOCKIO = 1<<15;
const static int64_t FLAG_TNE_USER_MMSTAT = 1<<16;
const static int64_t FLAG_TNE_USER_RAM_CONSOLE = 1<<17; //contain Internal error: Oops or Kernel panic
const static int64_t FLAG_TNE_USER_PROPERTY = 1<<18;
const static int64_t FLAG_TNE_USER_DF = 1<<19;
const static int64_t FLAG_TNE_USER_MOUNT = 1<<20;
const static int64_t FLAG_TNE_MINIDUMP = 1<<21;
const static int64_t FLAG_TNE_USER_WINDOW = 1<<22;
const static int64_t FLAG_TNE_USER_PROC_BASIC = 1<<23;
const static int64_t FLAG_TNE_USER_BASIC = 1<<24;
const static int64_t FLAG_TNE_USER_CPU_INFO = 1<<25;
