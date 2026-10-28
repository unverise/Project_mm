/*Transsion Top Secret*/
//SDD:modifyed by boqing.cai 20191205
/*
 * Copyright (C) 2019 Transsion Inc.
 *
 * @author zhongyu.shi
 * @version 1.0, 2019-06-03
 * @version 2.0 SDD:modifyed by boqing.cai 20191205
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/stacktrace.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include "tnek.h"

struct tne_req_queue {
	struct list_head list;
	int len;
	spinlock_t lock;
};
static struct tne_req_queue tne_queue;
//static struct work_struct tne_work;

#if 0
static void tnek_rm_char(char *str, char illegal)
{
	int i,j;
	for(i = j = 0; str[i] != '\0'; i++) {
		if(str[i] != illegal)
			str[j++]=str[i];
	}
	str[j] = '\0';
}
#endif

static void tnek_queue_request(struct tnek_api_list_node *node)
{
	unsigned long flags = 0;
	struct tnek_api_list_node *node_del = NULL , *n;

	spin_lock_irqsave(&tne_queue.lock, flags);

	if(tne_queue.len >= TNE_MAX_LIST_LEN ) {
		printk("tnek: %s ,Current list is longer than %d, native tne may be blocked!!! skip first.\n", __func__ ,TNE_MAX_LIST_LEN +1);
		list_for_each_entry_safe(node_del, n, &tne_queue.list, list) {
			if (node_del == NULL) {
				printk("tnek: %s, Invalid tnek_api_list_node struct\n", __func__);
				break;
			}
			list_del(&node_del->list);
			tne_queue.len--;
			break;
		}
	}

	if(tne_queue.len < TNE_MAX_LIST_LEN) {
		list_add_tail(&node->list, &tne_queue.list);
		tne_queue.len++;
	}

	spin_unlock_irqrestore(&tne_queue.lock, flags);

	if (unlikely(node_del != NULL)) {
		tnek_free_node(node_del);
	}
}

//static void tne_worker(struct work_struct *work)
//{
//}

size_t strlcpy(char *dst, const char *src, size_t size) {
    size_t src_len = strlen(src);
    if (size) {
        size_t copy_len = (src_len >= size) ? size - 1 : src_len;
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
    }
    return src_len;
}

struct tnek_api_list_node* tnek_create_node(char *tag, int64_t type, int32_t pid, char* multiPid, char* info)
{
	struct tnek_api_list_node *node = kzalloc(sizeof(struct tnek_api_list_node), GFP_ATOMIC);
	struct tnek_api *api;

	if(node != NULL) {
		api = &node->content;
		api->type = type;
		api->pid = pid;

		if (likely(tag != NULL)) {
			//snprintf(api->tag,  sizeof(api->tag), "%s", tag);
			strlcpy(api->tag, tag, sizeof(api->tag));
		}
		else {
			strlcpy(api->tag, "K_Unknown", sizeof(api->tag));
		}

		if (likely(multiPid != NULL)) {
			strlcpy(api->multiPid, multiPid, sizeof(api->multiPid));
		}
		else {
			strlcpy(api->multiPid, "", sizeof(api->multiPid));
		}

		if (info) {
			strlcpy(api->extra,  info, sizeof(api->extra));
		}
		printk("tnek: %s, type:%lld, tag:%s.\n", __func__, api->type, api->tag);

	}
	else {
		printk("tnek: %s, kzalloc fail,return.", __func__);
	}

	return node;
}

void tnek_free_node(struct tnek_api_list_node *node)
{
	if(node == NULL)
		return;

	kfree(node);
}
EXPORT_SYMBOL(tnek_free_node);

struct tnek_api_list_node* tnek_get_node_from_list(void)
{
	unsigned long flags = 0;
	struct tnek_api_list_node *node = NULL,*n;
	spin_lock_irqsave(&tne_queue.lock, flags);
	if(tne_queue.len > 0 && !list_empty(&tne_queue.list)) {
		list_for_each_entry_safe(node, n, &tne_queue.list, list) {
			if (node == NULL) {
				printk("tnek: %s, Invalid tnek_api_list_node struct\n", __func__);
				break;
			}
			list_del(&node->list);
			tne_queue.len--;
			break;
		}
	}
	spin_unlock_irqrestore(&tne_queue.lock, flags);
	return node;
}
EXPORT_SYMBOL(tnek_get_node_from_list);

int tnek_get_tne_queue_len(void)
{
	int len = 0;
	unsigned long flags = 0;
	spin_lock_irqsave(&tne_queue.lock, flags);
	len = tne_queue.len;
	spin_unlock_irqrestore(&tne_queue.lock, flags);
	return len;
}
EXPORT_SYMBOL(tnek_get_tne_queue_len);

extern void tnek_wake_up_poll_and_read(void);
void simp_tnek_caller_api(char *tag, int32_t pid)
{
	int64_t type = 0;
	char multiPid[TNE_LEN_PID] = "";
	char info[TNE_LEN_EXTRA] = "";
	struct tnek_api_list_node* current_node = tnek_create_node(tag, type, pid, multiPid, info);
	if(current_node != NULL) {
		tnek_queue_request(current_node);
//		queue_work(system_wq, &tne_work);
		tnek_wake_up_poll_and_read();
	}
}
EXPORT_SYMBOL(simp_tnek_caller_api);

void tnek_caller_api(char *tag, int64_t type, int32_t pid, char* info)
{
	char multiPid[TNE_LEN_PID] = "";
	struct tnek_api_list_node* current_node = tnek_create_node(tag, type, pid, multiPid, info);
	if(current_node != NULL) {
		tnek_queue_request(current_node);
//		queue_work(system_wq, &tne_work);
		tnek_wake_up_poll_and_read();
	}
}
EXPORT_SYMBOL(tnek_caller_api);

void simp_multipid_tnek_caller_api(char *tag, char* pid)
{
	int64_t type = 0;
	int32_t singlePid = 0;
	char info[TNE_LEN_EXTRA] = "";
	struct tnek_api_list_node* current_node = tnek_create_node(tag, type, singlePid, pid, info);
	if(current_node != NULL) {
		tnek_queue_request(current_node);
//		queue_work(system_wq, &tne_work);
		tnek_wake_up_poll_and_read();
	}
}
EXPORT_SYMBOL(simp_multipid_tnek_caller_api);

void multipid_tnek_caller_api(char *tag, int64_t type, char* pid, char* info)
{
	int32_t singlePid = 0;
	struct tnek_api_list_node* current_node = tnek_create_node(tag, type, singlePid, pid, info);
	if(current_node != NULL) {
		tnek_queue_request(current_node);
//		queue_work(system_wq, &tne_work);
		tnek_wake_up_poll_and_read();
	}
}
EXPORT_SYMBOL(multipid_tnek_caller_api);

int tnek_api_init(void)
{
//	INIT_WORK(&tne_work, tne_worker);
	spin_lock_init(&tne_queue.lock);
	INIT_LIST_HEAD(&tne_queue.list);
	tne_queue.len = 0;
	return 0;
}
EXPORT_SYMBOL(tnek_api_init);

void tnek_api_exit(void)
{
	return;
}
EXPORT_SYMBOL(tnek_api_exit);
