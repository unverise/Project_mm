/*Transsion Top Secret*/
/*
 * Copyright (C) 2023 Transsion Inc.
 *
 * @author chonglin.yang
 */
#include <uapi/linux/android/binder.h>
#include <trace/hooks/binder.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/task.h>
#include <linux/delay.h>
#include <linux/pid.h>
#include <uapi/linux/unistd.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <drivers/android/binder_internal.h>
#include "thd_util.h"
#include "aegisk.h"

struct binder_entry {
	struct hlist_node hlist;
	int pid;
	int tid;
	int target_pid;
	int to_tsk_tgid;
	int target_tid;
	__u32 code;
	int debug_id;
	int is_comm_printed;
	int is_tne_called;
	unsigned long last_switch_time;
	struct task_struct *from_proc_tsk;
	char from_proc_comm[TASK_COMM_LEN];
	struct task_struct *from_thread_tsk;
	char from_thread_comm[TASK_COMM_LEN];
	struct task_struct *to_tsk;
	char to_tsk_comm[TASK_COMM_LEN];
	struct task_struct *to_thread_tsk;
	char to_thread_comm[TASK_COMM_LEN];
};

struct binder_reply_entry {
	struct hlist_node hlist;
	int pid;
	int target_pid;
	unsigned long reply_time;
};

struct tne_caller_record_entry {
	struct hlist_node hlist;
	int target_pid;
	unsigned long last_tne_called_time;
};
static HLIST_HEAD(tne_record_entries);

//for sync transaction
static HLIST_HEAD(binder_sync_entries);
static int binder_sync_entry_count = 0;
static spinlock_t trace_binder_sync_lock;
#define MAX_BINDER_SYNC_ENTRY_COUNT    200
//for sync transaction reply
static HLIST_HEAD(binder_sync_reply_entries);
static spinlock_t trace_binder_sync_reply_lock;


static struct delayed_work binder_delayed_work;

static const char *const server_black_list[] = {
	"com.google",
	".gms.",
	"id.gms.",
	"com.android",
	"fingerprint",
	"c2@1.2-mediatek",
	"gle.android.gms"
};

static const char *const server_group_white_list[] = {
	"system_server",
	"SWWatchDog",
	"ndroid.systemui",
	"composer",
	"nderTestService", // for binder test
	"audioserver",
	"m.android.phone",
	"surfaceflinger"
};

int needCallTNE(char* client_comm,struct task_struct *server_tsk,char* server_comm,int server_tsk_tgid) {
	int ret = 0;
	int i= 0;
	int size_server_black_list = ARRAY_SIZE(server_black_list);
	int size_server_group_white_list = ARRAY_SIZE(server_group_white_list);
	struct task_struct *group_tsk = NULL;
	if (IS_ERR_OR_NULL(server_tsk) || IS_ERR_OR_NULL(server_comm) || IS_ERR_OR_NULL(client_comm)){
		return ret;
	}
	rcu_read_lock();
	group_tsk = find_task_by_vpid(server_tsk_tgid);
	if (IS_ERR_OR_NULL(group_tsk)){
		rcu_read_unlock();
		return ret;
	}
	get_task_struct(group_tsk);
	rcu_read_unlock();
	//black list
	if (!strncmp(client_comm, "dumpstate", 9) && !strncmp(server_comm, "system_server", 13)){
		goto out;
	}
	for (i = 0; i < size_server_black_list; i++){
		if (!strncmp(server_comm, server_black_list[i], strlen(server_black_list[i]))){
			goto out;
		}
	}

	//server group white list
	for (i = 0; i < size_server_group_white_list; i++){
		if (!strncmp(group_tsk->comm, server_group_white_list[i], strlen(server_group_white_list[i]))){
			ret = 1;
			goto out;
		}
	}
	//vold, vdc, keystore
	if (server_tsk->parent && !strncmp(server_tsk->parent->comm, "init",4)) {
		if (strncmp(server_comm, "binder:", 7) != 0){
			ret = 1;
		}
		goto out;
	}
out:
	put_task_struct(group_tsk);
	return ret;
}

int is_binder_sync_entry_exist(int proc_pid,int target_proc_pid) {
	struct binder_entry *binder_entry;
	struct hlist_node *tmp;
	int ret = 0;
	spin_lock(&trace_binder_sync_lock);
	hlist_for_each_entry_safe(binder_entry, tmp, &binder_sync_entries, hlist) {
		if (binder_entry->pid==proc_pid && binder_entry->target_pid==target_proc_pid) {
			ret=1;
			goto out;
		}
	}
out:
	spin_unlock(&trace_binder_sync_lock);
	return ret;
}
int is_binder_sync_reply_exist(int pid,int target_pid) {
	struct binder_reply_entry *binder_reply_entry;
	struct hlist_node *tmp;
	int ret = 0;
	spin_lock(&trace_binder_sync_reply_lock);
	hlist_for_each_entry_safe(binder_reply_entry, tmp, &binder_sync_reply_entries, hlist) {
		if (binder_reply_entry->pid==pid && binder_reply_entry->target_pid==target_pid) {
			ret=1;
			goto out;
		}
	}
out:
	spin_unlock(&trace_binder_sync_reply_lock);
	return ret;
}

void new_record_entry(int target_pid){
	struct tne_caller_record_entry * record_entry;
	record_entry = kzalloc(sizeof(*record_entry), GFP_ATOMIC);
	record_entry->target_pid = target_pid;
	record_entry->last_tne_called_time = jiffies;
	hlist_add_head(&record_entry->hlist, &tne_record_entries);
}

int is_target_tne_called_in_time(int target_pid){
	int ret = 0;
	struct tne_caller_record_entry *entry;
	struct hlist_node *tmp;
	unsigned long interval;
	hlist_for_each_entry_safe(entry, tmp, &tne_record_entries, hlist) {
		interval = (jiffies - entry->last_tne_called_time) / HZ;
		if(target_pid == entry->target_pid) {
			if(interval <= 60){
				ret = 1;
				//printk("binder hung find record : %d,interval:%d", entry->target_pid,interval);
				goto out;
			}
		}

		if(interval > 60){
			//printk("binder hung record too long,delete it : %d,interval:%d", entry->target_pid,interval);
			hlist_del(&entry->hlist);
			kfree(entry);
		}
	}
out:
	return ret;
}

int check_if_thd_alive(struct task_struct *p, int tid) {
	int ret = 0;
	struct task_struct *tid_tsk = NULL;
	unsigned int state;
	if (IS_ERR_OR_NULL(p)) {
		return ret;
	}
	rcu_read_lock();
	tid_tsk = find_task_by_vpid(tid);
	if (IS_ERR_OR_NULL(tid_tsk)) {
		printk("check_if_thd_alive,find_task_by_vpid failed");
		rcu_read_unlock();
		return ret;
	}
	state = READ_ONCE(tid_tsk->__state);
	if ((state&( TASK_UNINTERRUPTIBLE | TASK_INTERRUPTIBLE | __TASK_STOPPED | __TASK_TRACED)) || (state == TASK_RUNNING)) {
		ret = 1;
	}
	rcu_read_unlock();
	//printk("check_if_thd_alive for %d:%d ret:%d,tid_tsk->state:%x", p->pid, tid_tsk->pid, ret,tid_tsk->state);
	return ret;
}

void post_to_aegis(char *from_comm, char *to_comm, int from_pid, int from_tid, int target_pid, int target_tid,int code){
	char buffer[256] = {'\0'};
	snprintf(buffer, sizeof(buffer), "from %s to %s ,%d:%d code 0x:%x to %d:%d \n",
							from_comm,to_comm,from_pid, from_tid, code, target_pid,target_tid);
		uploadBinderInfo2Aegis("check_binder_hung", buffer, "");
}
void clear_aegis_binder_buffer(void){
	uploadBinderInfo2Aegis("clear_binder_buffer", "", "");
}

void check_sync_binder_hung(void){
	struct binder_entry *entry;
	struct hlist_node *tmp;
	unsigned long interval;
	spin_lock(&trace_binder_sync_lock);
	binder_sync_entry_count = 0;
	hlist_for_each_entry_safe(entry, tmp, &binder_sync_entries, hlist) {
		if (check_if_thd_alive(entry->from_proc_tsk, entry->tid)&&check_if_thd_alive(entry->to_tsk, entry->target_pid)) {
			interval = (jiffies - entry->last_switch_time) / HZ;
			if ( interval > 0 && interval%10 == 0 ) {
				printk("%s,binder hung %ld s from %d:%d code 0x:%x to %d,%d,debug_id:%d", entry->from_thread_comm,interval,
					entry->pid, entry->tid, entry->code, entry->target_pid,entry->target_tid,entry->debug_id);
			}
			if (entry->is_comm_printed == 0 && interval >= 5) {
				printk("binder hung from %s : %s  to %s ,%d:%d code 0x:%x to %d \n",entry->from_proc_comm,entry->from_thread_comm,
					entry->to_tsk_comm,entry->pid, entry->tid, entry->code, entry->target_pid);
				entry->is_comm_printed= 1;
			}
			if(is_target_tne_called_in_time(entry->target_pid)){
				entry->is_tne_called = 1;
			}
			if (interval >= 30 && entry->is_tne_called == 0 &&
				needCallTNE(entry->from_proc_comm,entry->to_tsk,entry->to_tsk_comm,entry->to_tsk_tgid)) {
				char pids[256] = {'\0'};
				snprintf(pids, sizeof(pids), "%d,%d", entry->pid,entry->target_pid);
				startAegisK("BINDER_TARGET_HUNG", pids, "FLAG_TNE_BINDER_HUNG_MSG");
				entry->is_tne_called = 1;
				new_record_entry(entry->target_pid);
			}
			if (interval >= 60) {
				printk("binder hung too long,delete entry %s, from %d:%d code:%d to %d", entry->from_thread_comm,
					entry->pid, entry->tid, entry->code, entry->target_pid);
				hlist_del(&entry->hlist);
				kfree(entry);
			}else{
				if(binder_sync_entry_count == 0){
					clear_aegis_binder_buffer();
				}
				binder_sync_entry_count++;
				post_to_aegis(entry->from_proc_comm,entry->to_tsk_comm,entry->pid, entry->tid, entry->target_pid, entry->target_tid, entry->code);
			}
		} else {
			hlist_del(&entry->hlist);
			kfree(entry);
		}
	}
	spin_unlock(&trace_binder_sync_lock);
}
void check_sync_binder_reply(struct binder_reply_entry *binder_reply_entry){
	struct binder_entry *entry;
	struct hlist_node *tmp;

	spin_lock(&trace_binder_sync_lock);
	hlist_for_each_entry_safe(entry, tmp, &binder_sync_entries, hlist) {
		if (entry->pid == binder_reply_entry->pid && entry->target_pid == binder_reply_entry->target_pid) {
			hlist_del(&entry->hlist);
			kfree(entry);
			binder_sync_entry_count--;
			goto out;
		}
		if(!check_if_thd_alive(entry->from_proc_tsk, entry->tid) ||
				!check_if_thd_alive(entry->to_tsk, entry->target_pid)){
			hlist_del(&entry->hlist);
			kfree(entry);
		}
	}

out:
	spin_unlock(&trace_binder_sync_lock);
}
void check_sync_binder_reply_entries(void){
	struct binder_reply_entry *binder_reply_entry;
	struct hlist_node *tmp;

	spin_lock(&trace_binder_sync_reply_lock);
	hlist_for_each_entry_safe(binder_reply_entry, tmp, &binder_sync_reply_entries, hlist) {

		check_sync_binder_reply(binder_reply_entry);

		hlist_del(&binder_reply_entry->hlist);
		kfree(binder_reply_entry);
	}
	spin_unlock(&trace_binder_sync_reply_lock);
}

void check_binder_hung(struct work_struct *work)
{

	int ret = 0;
	check_sync_binder_reply_entries();
	check_sync_binder_hung();

	ret = schedule_delayed_work(&binder_delayed_work, msecs_to_jiffies(1000));
	 if (ret < 0) {
		printk("Failed to schedule delayed work,ret = %d \n",ret);
	 }
}


void clear_binder_entries(void)
{
	struct binder_entry *entry;
	struct binder_reply_entry *binder_reply_entry;
	struct hlist_node *tmp;

	spin_lock(&trace_binder_sync_lock);
	printk("clear_binder_sync_entries.\n");
	hlist_for_each_entry_safe(entry, tmp, &binder_sync_entries, hlist) {
		hlist_del(&entry->hlist);
		kfree(entry);
	}
	spin_unlock(&trace_binder_sync_lock);

	spin_lock(&trace_binder_sync_reply_lock);
	printk("clear_binder_sync_reply_entries.\n");
	hlist_for_each_entry_safe(binder_reply_entry, tmp, &binder_sync_reply_entries, hlist) {
		hlist_del(&binder_reply_entry->hlist);
		kfree(binder_reply_entry);
	}
	spin_unlock(&trace_binder_sync_reply_lock);
}

const unsigned int FLAG_FILTER_BINDER_HUNG_TRACE = 0x80;
void trace_binder_proc_transaction(void *data,struct task_struct *current_task,
									struct task_struct *target_proc_task,struct task_struct *target_thread_task,
									int node_debug_id,struct binder_transaction *t,bool pending_async){
	if(!current_task || !target_proc_task || !t ||
		t->code == 0x5f434d44 || t->code == 0x5F444D50){
		//SHELL_COMMAND_TRANSACTION = ('_'<<24)|('C'<<16)|('M'<<8)|'D'; DUMP_TRANSACTION = ('_'<<24)|('D'<<16)|('M'<<8)|'P';
		goto out;
	}
	if(t->flags & FLAG_FILTER_BINDER_HUNG_TRACE){
		goto out;
	}
	if(!!(t->flags & TF_ONE_WAY)){
		if(t->buffer->oneway_spam_suspect){
			printk("binder hung oneway spam suspect,from %d:%d code:%d to %d", t->from_pid, t->from_tid, t->code, target_proc_task->pid);
		}
		goto out;
	}
	if(is_binder_sync_entry_exist(t->from_pid,target_proc_task->pid)){
		goto out;
	}
	spin_lock(&trace_binder_sync_lock);
	if(binder_sync_entry_count >= MAX_BINDER_SYNC_ENTRY_COUNT){
		spin_unlock(&trace_binder_sync_lock);
		goto out;
	}
	spin_unlock(&trace_binder_sync_lock);

	struct binder_entry *binder_entry;
	binder_entry = kzalloc(sizeof(*binder_entry), GFP_ATOMIC);
	if (binder_entry) {
		binder_entry->pid = t->from_pid;
		binder_entry->tid = t->from_tid;
		binder_entry->code = t->code;
		binder_entry->debug_id = node_debug_id;
		binder_entry->is_tne_called = 0;
		binder_entry->is_comm_printed = 0;
		binder_entry->target_pid = target_proc_task->pid;
		binder_entry->to_tsk_tgid = target_proc_task->tgid;
		binder_entry->target_tid = target_thread_task ? target_thread_task->pid : 0;
		binder_entry->from_thread_tsk = t->from? t->from->task: NULL;
		strncpy(binder_entry->from_thread_comm,t->from? t->from->task->comm: "",TASK_COMM_LEN);
		binder_entry->from_proc_tsk = current_task;
		strncpy(binder_entry->from_proc_comm,current_task->comm,TASK_COMM_LEN);
		binder_entry->to_tsk = target_proc_task;
		strncpy(binder_entry->to_tsk_comm,target_proc_task->comm,TASK_COMM_LEN);
		binder_entry->last_switch_time = jiffies;
		binder_entry->to_thread_tsk = target_thread_task? target_thread_task: NULL;
		strncpy(binder_entry->to_thread_comm,target_thread_task? target_thread_task->comm: "",TASK_COMM_LEN);
		spin_lock(&trace_binder_sync_lock);
		hlist_add_head(&binder_entry->hlist, &binder_sync_entries);
		spin_unlock(&trace_binder_sync_lock);
	} else {
		printk("trans_in_binder kzalloc fail");
	}
out:
	return;
}

void trace_binder_reply(void *data, struct binder_proc *target_proc, struct binder_proc *proc,
									struct binder_thread *thread,struct binder_transaction_data *tr)
{
	struct binder_reply_entry *binder_reply;
	if(!target_proc || !proc || !thread || !tr || (tr->flags & TF_ONE_WAY)){
		goto out;
	}
	/*
		reply_entry->pid == target_proc->pid  这里target_proc为服务端响应的目标进程，reply_entry->pid请求时客户端进程（客户端校验）
		reply_entry->target_pid == proc->pid  这里proc为服务端响应的进程，reply_entry->target_pid为发起请求时的目标进程（服务端校验）
	*/
	if(is_binder_sync_reply_exist(target_proc->pid,proc->pid)){
		goto out;
	}

	binder_reply = kzalloc(sizeof(*binder_reply), GFP_ATOMIC);
	if(!binder_reply){
		goto out;
	}
	binder_reply->pid = target_proc->pid;
	binder_reply->target_pid = proc->pid;
	binder_reply->reply_time = jiffies;
	spin_lock(&trace_binder_sync_reply_lock);
	hlist_add_head(&binder_reply->hlist, &binder_sync_reply_entries);
	spin_unlock(&trace_binder_sync_reply_lock);
out:
	return;
}

bool trace_hung_binder_init(void)
{
	int ret = 0;
	spin_lock_init(&trace_binder_sync_lock);
	spin_lock_init(&trace_binder_sync_reply_lock);

	ret = register_trace_android_vh_binder_proc_transaction(trace_binder_proc_transaction, NULL);
	if (ret != 0) {
		printk("register_trace_android_vh_binder_proc_transaction failed.\n");
		return false;
	}
	ret = register_trace_android_vh_binder_reply(trace_binder_reply, NULL);
	if (ret != 0) {
		printk("register_trace_android_vh_binder_reply failed.\n");
		return false;
	}
	INIT_DELAYED_WORK(&binder_delayed_work, check_binder_hung);
	schedule_delayed_work(&binder_delayed_work, msecs_to_jiffies(1000));
	return true;
}

void trace_hung_binder_exit(void)
{
	unregister_trace_android_vh_binder_proc_transaction(trace_binder_proc_transaction, NULL);
	unregister_trace_android_vh_binder_reply(trace_binder_reply, NULL);
	cancel_delayed_work(&binder_delayed_work);
	clear_binder_entries();
}
