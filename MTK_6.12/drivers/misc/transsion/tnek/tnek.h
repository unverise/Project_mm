/*Transsion Top Secret*/
/*
 * Copyright (C) 2019 Transsion Inc.
 *
 * @author zhongyu.shi
 * @version 1.0, 2019-06-03
 * @version 2.0 modifyed by boqing.cai 20191205
 * modified by xqzhan 20191224  modify some structs
 */
#ifndef __TNEK_H
#define __TNEK_H

#include <linux/types.h>


#define FLAG_TNE_INVALID  0
#define FLAG_TNE_BINDER       (1 << 0)
#define FLAG_TNE_WINDOW     (1 << 1)
#define FLAG_TNE_PROC_MEM  (1 << 2)
#define FLAG_TNE_PROC_BASIC  (1 << 3)
#define FLAG_TNE_PROC_STACK  (1 << 4)
#define FLAG_TNE_SYS_MEM  (1 << 5)
#define FLAG_TNE_ANR_TRACE  (1 << 6)
#define FLAG_TNE_SYS_TRACE  (1 << 7)
#define FLAG_TNE_HPROF  (1 << 8)
#define FLAG_TNE_LOG  (1 << 9)
#define FLAG_TNE_FD  (1 << 10)
#define FLAG_TNE_BASIC  (1 << 11)
#define FLAG_TNE_PACKAGE  (1 << 12)
#define FLAG_TNE_CAMERA  (1 << 13)
#define FLAG_TNE_MEDIA  (1 << 14)
#define FLAG_TNE_NET  (1 << 15)
#define FLAG_TNE_COREDUMP  (1 << 16)
#define FLAG_TNE_CPU_INFO  (1 << 17)
#define FLAG_TNE_DB_INFO  (1 << 18)
#define FLAG_TNE_BUGREPORT  (1 << 19)
#define FLAG_TNE_POWER  (1 << 20)
#define FLAG_TNE_THERMAL  (1 << 21)
#define FLAG_TNE_PERF_BUFFER  (1 << 22)
#define FLAG_TNE_POWER_BUFFER  (1 << 23)
#define FLAG_TNE_STAB_BUFFER  (1 << 24)
#define FLAG_TNE_COM_BUFFER  (1 << 25)
#define FLAG_TNE_CAMERA_BUFFER  (1 << 26)
#define FLAG_TNE_MODEM_BUFFER  (1 << 27)
#define FLAG_TNE_AUDIO_DUMP  (1 << 29)
#define FLAG_TNE_DISPLAY  (1 << 30)
#define FLAG_USAGESTATS  (1 << 31)
#define FLAG_TNE_BINDER_CALLS_STATS  (1 << 32)
#define FLAG_TNE_LOOPER_STATS  (1 << 33)
#define FLAG_TNE_SYS_MEM_EXT  (1 << 34)
#define FLAG_TNE_APP_INFO  (1 << 35)
#define FLAG_TNE_COPY_BUFFER  (1 << 62)

#define EXCEPTION_TYPE_ANR  (FLAG_TNE_BINDER | FLAG_TNE_WINDOW | FLAG_TNE_SYS_MEM | FLAG_TNE_LOG | FLAG_TNE_SYS_TRACE | FLAG_TNE_ANR_TRACE)
#define EXCEPTION_TYPE_SWT  EXCEPTION_TYPE_ANR | FLAG_TNE_BASIC
#define EXCEPTION_TYPE_NE  FLAG_TNE_LOG
#define EXCEPTION_TYPE_JE  FLAG_TNE_BINDER | FLAG_TNE_PACKAGE | FLAG_TNE_LOG | FLAG_TNE_WINDOW
#define EXCEPTION_TYPE_KE  FLAG_TNE_LOG

//the following definition compatible with older version
#define DUMP_BASIC        FLAG_TNE_LOG
#define DUMP_MEMINFO        FLAG_TNE_SYS_MEM
#define DUMP_WINDOW        FLAG_TNE_WINDOW
#define DUMP_PROPERTY        FLAG_TNE_BASIC
#define DUMP_CPU               FLAG_TNE_CPU_INFO
#define DUMP_BINDER           FLAG_TNE_BINDER
#define DUMP_SWT               (FLAG_TNE_ANR_TRACE|FLAG_TNE_BINDER |FLAG_TNE_LOG | FLAG_TNE_BASIC)
#define DUMP_SYS_HPROF        FLAG_TNE_HPROF
#define DUMP_SYS_TRACE        FLAG_TNE_PROC_STACK
#define DUMP_COMMON        DUMP_BASIC


#define TNE_LEN_TAG 30
#define TNE_LEN_EXTRA 256
#define TNE_LEN_STACK 700
#define TNE_LEN_PID 32

struct tnek_api {
	char tag[TNE_LEN_TAG];
	int64_t type;
	int32_t pid;
	char extra[TNE_LEN_EXTRA];
	char stack[TNE_LEN_STACK];
	char multiPid[TNE_LEN_PID];
};

struct tnek_api_list_node {
	struct list_head list;
	struct tnek_api content;
};
#define TNE_MAX_LIST_LEN		9

int tnek_api_init(void);
void tnek_api_exit(void);
extern void simp_tnek_caller_api(char *tag, int32_t pid);
extern void tnek_caller_api(char *tag, int64_t type, int32_t pid, char* info);
extern void simp_multipid_tnek_caller_api(char *tag, char* pid);
extern void multipid_tnek_caller_api(char *tag, int64_t type, char* pid, char* info);
extern struct tnek_api_list_node* tnek_get_node_from_list(void);
extern int tnek_get_tne_queue_len(void);
extern void tnek_free_node(struct tnek_api_list_node *node);
#endif // __TNEK_H
