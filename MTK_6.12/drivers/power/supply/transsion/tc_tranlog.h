#ifndef __TC_TRANLOG_H__
#define __TC_TRANLOG_H__

#if IS_ENABLED(CONFIG_TRAN_TRANLOG)

#include "tranlog.h"

#else

static inline ssize_t tranlog_local(const char *tag, const char* fmt, ...){
    return 0;
}

static inline  void tranlog_local_log_nodel(const char *tag, const char* fmt, ...){
    return;
}

static inline  void tranlog_setnv_s32(const char *key, int value, int op){
    return;
}

static inline  void tranlog_setnv_s64(const char *key, long long value, int op){
    return;
}

static inline  void tranlog_setnv_string(const char *key, const char* fmt, ...){
    return;
}


static inline  int32_t tranlog_getnv_s32(const char *key, int32_t def){
    return 0;
}

static inline  int64_t tranlog_getnv_s64(const char *key, int64_t def){
    return 0;
}


struct athena_msg {
    char *key;
    void *value;
    int type;
    struct athena_msg *next;
};

static inline  struct athena_msg* alloc_msg(void){
    return NULL;
}

static inline  void free_msg(struct athena_msg **athena){
    return;
}

static inline  void submit_msg(struct athena_msg **athena, int version, long long tid, char *tag){
    return;
}


static inline  void msg_put_string(struct athena_msg **athena, char *key, char *value){
    return;
}

static inline  void msg_put_int(struct athena_msg **athena, char *key, int value){
    return;
}

static inline  void msg_put_long(struct athena_msg **athena, char *key, long value){
    return;
}

static inline  void msg_put_ll(struct athena_msg **athena, char *key, long long value){
    return;
}

static inline  int is_tid_enable(long long tid){
    return 0;
}

static inline  int register_athena_notifier(struct notifier_block *nb){
    return 0;
}

static inline  int unregister_athena_notifier(struct notifier_block *nb){
    return 0;
}

static inline  ssize_t tranlog_dump(const char *tag, const char* fmt, ...){
    return 0;
}

static inline ssize_t tranlog_dump_trigger(const char *tag){
    return 0;
}

struct cloud_engine_config
{
    char* id;
    char* config;
};

static inline  int register_cloudengine_notifier(const char* id, const char* ver, struct notifier_block *nb){
    return 0;
}

static inline  int unregister_cloudengine_notifier(const char* id, struct notifier_block *nb){
    return 0;
}

static inline  void cloudengine_feed_back(const char* id, int state){
    return;
}

static inline  char* cloudengine_getConfig(const char* id){
    return NULL;
}

static inline  char* cloudengine_getStringByContent(char* content, const char* key){
    return NULL;
}

static inline  int cloudengine_getIntByContent(char* content, const char* key){
    return 0;
}

static inline  char* cloudengine_getStringByID(const char* id, const char* key){
    return NULL;
}

static inline  int cloudengine_getIntByID(const char* id, const char* key){
    return 0;
}

#endif

#endif