 /*Transsion Top Secret*/
//SDD:modifyed by boqing.cai 20191205 start
/*
 * Copyright (C) 2019 Transsion Inc.
 *
 * @author zhongyu.shi
 * @version 1.0, 2019-06-03
 * @version 2.0 modified by boqing.cai 20191205 add tnek read and add list to save tne data
 * modified by xqzhan 20191224 add tnek wirte and modify some structs
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include "tnek.h"
#include <linux/poll.h>
#include <linux/slab.h>

#define DEVICE_NAME	"tnek"

static int tnek_open (struct inode *inode, struct file *filp);
static int tnek_release (struct inode *inode, struct file *filp);
static ssize_t tnek_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t tnek_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

static unsigned int tnek_poll(struct file *filp, struct poll_table_struct *ptable);
static long tnek_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

wait_queue_head_t tne_pollwait;
wait_queue_head_t tne_readwait;
static atomic_t tne_read_start = ATOMIC_INIT(0);//use atomic

static const struct file_operations tnek_fops = {
	.owner = THIS_MODULE,
	.open = tnek_open,
	.poll = tnek_poll,
	.unlocked_ioctl = tnek_ioctl,
	.compat_ioctl = tnek_ioctl,
	.release = tnek_release,
	.read = tnek_read,
	.write = tnek_write,
};

static struct miscdevice tnek_dev = {
	//SDD:modify by zhongyu.shi for XLLLHLWS-785 2019/10/10 start
	.minor = MISC_DYNAMIC_MINOR,
	//SDD:modify by zhongyu.shi for XLLLHLWS-785 2019/10/10 end
	.name = DEVICE_NAME,
	.fops = &tnek_fops,
};

static long tnek_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static int tnek_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int tnek_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static unsigned int tnek_poll(struct file *filp, struct poll_table_struct *ptable)
{
	if (tnek_get_tne_queue_len() > 0)
		return POLLIN | POLLRDNORM;
	poll_wait(filp, &tne_pollwait, ptable);
	return 0;
}

static ssize_t tnek_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct tnek_api_list_node* node = NULL;
	struct tnek_api *api = NULL;
	int ret = -1;
	int ret_size = sizeof(struct tnek_api);

	if(count < ret_size) {
		printk("tnek: %s, readbuf is too small,just quit.\n",__func__);
		return -ENOMEM;
	}

//non block read
	if (filp->f_flags & O_NONBLOCK){
		node = tnek_get_node_from_list();
		if(node == NULL) {
			return -EAGAIN;
		}
	}

//block read
	while (node == NULL) {
		node = tnek_get_node_from_list();
		if(node != NULL){
			break;
		}
		//wait_event_interruptible will be wakeup by signal
		//wait_event will be not killed before wakeup
		//SDD:modify BLSLEDPUB-3824 by boqing.cai 2020/2/17 start
		//ret = wait_event_killable(tne_readwait, (atomic_read(&tne_read_start) == 1));
		ret = wait_event_interruptible(tne_readwait, (atomic_read(&tne_read_start) == 1));
		//SDD:modify BLSLEDPUB-3824 by boqing.cai 2020/02/17 end
		if(ret == -ERESTARTSYS) {
			return ret;
		}
		atomic_set(&tne_read_start, 0);
	}

	api = &node->content;
	ret = copy_to_user(buf, api, ret_size);
	printk("tnek: %s, tag=%s, type=%lld, pid=%d, ret=%d\n", __func__,  api->tag, api->type, api->pid, ret_size);
	tnek_free_node(node);
	if(ret != 0) {
		printk("tnek: %s, copy_to_user return %d.\n",__func__,ret);
		return -ENOMEM;
	}

	return ret_size;
}

static ssize_t tnek_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct tnek_api *api;
	api = kzalloc(sizeof(struct tnek_api), GFP_KERNEL);
	if (!api) {
		return -ENOMEM;
	}
 	if (count != sizeof(struct tnek_api)) {
		printk("k-5.10 tnek_write count illegality\n");
		kfree(api);
		return -1;
	}
	if (copy_from_user(api, buf, count)) {
		kfree(api);
		return -EFAULT;
	}

	printk("tnek:tnek_write:tag=%s,api->pid=%d\n", api->tag, api->pid);
	tnek_caller_api(api->tag, api->type, api->pid, api->extra);
	kfree(api);
	return 0;
}


void tnek_wake_up_poll_and_read(void)
{
	atomic_set(&tne_read_start, 1);
	wake_up(&tne_pollwait);
	wake_up(&tne_readwait);
}

static int __init tnek_init(void)
{
	int ret = 0;
	init_waitqueue_head(&tne_pollwait);
	init_waitqueue_head(&tne_readwait);
    tnek_api_init();
	ret = misc_register(&tnek_dev);
	if (unlikely(ret)){
		printk("tnek: misc_register failed!\n");
		goto out;
	}
	else {
		printk("tnek: misc_register success!\n");
}
	out:
		return ret;
}

static void __exit tnek_exit(void)
{
	misc_deregister(&tnek_dev);
    tnek_api_exit();
}
module_init(tnek_init);
module_exit(tnek_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Transsion TNE Kernel Driver");
MODULE_AUTHOR("Transsion. SPD_TI");
