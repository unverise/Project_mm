#ifndef __TRAN_TRANLOG_H__
#define __TRAN_TRANLOG_H__

#define TRANLOG_OP_NONE       1
#define TRANLOG_OP_ADD        2
#define TRANLOG_OP_SUB        3
#define TRANLOG_OP_MUL        4
#define TRANLOG_OP_DIV        5

extern ssize_t tranlog_local(const char *tag, const char* fmt, ...);
extern void tranlog_local_log_nodel(const char *tag, const char* fmt, ...);
extern void tranlog_setnv_s32(const char *key, int value, int op);
extern void tranlog_setnv_s64(const char *key, long long value, int op);
extern void tranlog_setnv_string(const char *key, const char* fmt, ...);

extern int32_t tranlog_getnv_s32(const char *key, int32_t def);
extern int64_t tranlog_getnv_s64(const char *key, int64_t def);

/* example:
*
*    struct athena_msg *msg = NULL;
*    int tid = 10230001;
*
*    msg = alloc_msg();
*    msg_put_string(&msg, "hello", "world");
*    msg_put_int(&msg, "int", 1);
*    msg_put_long(&msg, "long", 1);
*    submit_msg(&msg, 1, tid, "power_current");
*    free_msg(&msg);
*
*/

struct athena_msg {
    char *key;
    void *value;
    int type;
    struct athena_msg *next;
};

extern struct athena_msg* alloc_msg(void);
extern void free_msg(struct athena_msg **athena);
extern void submit_msg(struct athena_msg **athena, int version, long long tid, char *tag);

extern void msg_put_string(struct athena_msg **athena, char *key, char *value);
extern void msg_put_int(struct athena_msg **athena, char *key, int value);
extern void msg_put_long(struct athena_msg **athena, char *key, long value);
extern void msg_put_ll(struct athena_msg **athena, char *key, long long value);


#define TID_STATE_ENABLED  1
#define TID_STATE_DISABLED 0
extern int is_tid_enable(long long tid);
extern int register_athena_notifier(struct notifier_block *nb);
extern int unregister_athena_notifier(struct notifier_block *nb);

//SPD:add by chongyang.zhang for dump data 2020.07.10 start
extern ssize_t tranlog_dump(const char *tag, const char* fmt, ...);
ssize_t tranlog_dump_trigger(const char *tag);
//SPD:add by chongyang.zhang for dump data 2020.07.10 end

//SPD:add by chongyang.zhang for cloud engine 2020.12.24 start
struct cloud_engine_config
{
    char* id;
    char* config;
};

extern int register_cloudengine_notifier(const char* id, const char* ver, struct notifier_block *nb);
extern int unregister_cloudengine_notifier(const char* id, struct notifier_block *nb);
extern void cloudengine_feed_back(const char* id, int state);
extern char* cloudengine_getConfig(const char* id);
extern char* cloudengine_getStringByContent(char* content, const char* key);
extern int cloudengine_getIntByContent(char* content, const char* key);
extern char* cloudengine_getStringByID(const char* id, const char* key);
extern int cloudengine_getIntByID(const char* id, const char* key);
//SPD:add by chongyang.zhang for cloud engine 2020.12.24 end

#endif
