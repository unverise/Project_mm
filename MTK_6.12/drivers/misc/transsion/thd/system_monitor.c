/*Transsion Top Secret*/
/*
 * Copyright (C) 2024 Transsion Inc.
 *
 * @author chonglin.yang
 */
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "thd_util.h"
#include "aegisk.h"

#define ENHANCE_SIGNAL_DEVICE_NAME      "ensignal"
static int monitor_procs_count = 0;
static LIST_HEAD (monitor_procs);
static DEFINE_MUTEX(monitor_procs_lock);
//The value below can't be changed , some will be used in userspace
#define LOCKED            0x10001
#define UNLOCKED          0x10002
#define SET_TIMEOUT       0x10003
#define SET_SYSTEM_SERVER 0x10004
#define SET_ANDROID_APP   0x10005
#define PRINT_BACKTRACE   0x10006
#define START_TRANSACTION_CLIENT 0x10007
#define FINISH_TRANSACTION_CLIENT 0x10008
#define START_HANDLE_MSG  0x10009
#define FINISH_HANDLE_MSG 0x1000A
#define SET_INIT_INFO     0x1000B
#define START_TRANSACTION_SERVER 0x1000C
#define FINISH_TRANSACTION_SERVER 0x1000D
#define START_TRANSACTION_CLIENT_TO_NOT_MONITORED 0x1000E
#define START_TRANSACTION_SERVER_DUMP 0x1000F
#define START_TRANSACTION_SERVER_DUMP_FROM_NOT_MONITORED 0x10010
#define START_TRANSACTION_CLIENT_DUMP 0x10011

enum {
	LOCKER_MUTEX_LOCK                            = 0x01,
	LOCKER_BINDER_TRANSACTION                          ,
	LOCKER_SYS_LOOPER_HANDLE_MSG                       ,
	LOCKER_APP_LOOPER_HANDLE_MSG                       ,
	LOCKER_BINDER_TRANSACTION_OUT                      ,
	LOCKER_BINDER_TRANSACTION_OUT_DUMP                 ,
	LOCKER_BINDER_TRANSACTION_OUT_NOT_MONITORED        ,
	LOCKER_BINDER_TRANSACTION_DUMP                     ,
	LOCKER_BINDER_TRANSACTION_DUMP_FROM_NOT_MONITORED  ,
};

static char* getLockerString(int type) {
	switch(type){
		case LOCKER_MUTEX_LOCK:
			return "LOCKER_MUTEX_LOCK";
		case LOCKER_BINDER_TRANSACTION:
			return "LOCKER_BINDER_TRANSACTION";
		case LOCKER_SYS_LOOPER_HANDLE_MSG:
			return "LOCKER_SYS_LOOPER_HANDLE_MSG";
		case LOCKER_APP_LOOPER_HANDLE_MSG:
			return "LOCKER_APP_LOOPER_HANDLE_MSG";
		case LOCKER_BINDER_TRANSACTION_OUT:
			return "LOCKER_BINDER_TRANSACTION_OUT";
		case LOCKER_BINDER_TRANSACTION_OUT_DUMP:
			return "LOCKER_BINDER_TRANSACTION_OUT_DUMP";
		case LOCKER_BINDER_TRANSACTION_OUT_NOT_MONITORED:
			return "LOCKER_BINDER_TRANSACTION_OUT_NOT_MONITORED";
		case LOCKER_BINDER_TRANSACTION_DUMP:
			return "LOCKER_BINDER_TRANSACTION_DUMP";
		case LOCKER_BINDER_TRANSACTION_DUMP_FROM_NOT_MONITORED:
			return "LOCKER_BINDER_TRANSACTION_DUMP_FROM_NOT_MONITORED";
		default:
			return "UNKONWN";
	}
}

struct monitor_proc
{
	//int event_count;
	int pid;
	atomic_t tmp_ref;
	//atomic_t enable_locker_monitor;
	//struct task_struct *sigHandleThread;
	struct task_struct *group_leader;
	//struct binder_proc *proc;
	//struct list_head req_event;
	//struct mutex event_mutex;
	//struct mutex thread_mutex;
	struct list_head list;
	//for locker monitor
	//all lockers in this process.
	struct list_head hash_locker[10];
	struct mutex hash_locker_lock;
};

struct lock_info_kernel
{
	u64 size;
	u64 id;
	u64 timeout;
	int type;
	int pid;
	char name[];
};

struct locker {
	struct list_head list;
	struct lock_info_kernel* info;
	struct monitor_proc *debug_proc;

	struct mutex mutex;//protect worker&tid&schedule_timestamp&dumped , include renew info
	int tid;//current thread id,or waiting threadid(binder_out)
	unsigned long schedule_timestamp;
	struct delayed_work worker;
	bool dumped;
	//void *private_data;// for mutex static
};

struct tne_caller_record_entry {
	struct hlist_node hlist;
	int target_pid;
	unsigned long last_tne_called_time;
};
static HLIST_HEAD(tne_call_record_entries);
static spinlock_t tne_call_record_lock;
static unsigned long last_app_looper_hung_tne_called_time=0;
static spinlock_t app_looper_hung_tne_call__lock;

static void free_locker(struct locker * locker)
{
	if(IS_ERR_OR_NULL(locker))
	{
		return;
	}
	mutex_lock(&locker->mutex);
	list_del(&locker->list);
	cancel_delayed_work_sync(&locker->worker);
	kfree(locker->info);
	mutex_unlock(&locker->mutex);
	kfree(locker);
}

static inline void reset_locker(struct locker *locker)
{
	char buff[200] = {0};

	mutex_lock (&locker->mutex);//hold this mutex to modify locker->tid
	if (locker->tid != -1) {
		if (locker->dumped) {
			switch (locker->info->type) {
				case LOCKER_SYS_LOOPER_HANDLE_MSG:
				case LOCKER_APP_LOOPER_HANDLE_MSG:
					snprintf(buff, sizeof(buff), "System Monitor: finish handle msg: pid=%d  tid=%d handle msg time=%u ms\n",
						locker->info->pid,locker->tid, jiffies_to_msecs(jiffies - locker->schedule_timestamp));
				break;
			}
			printk("%s",buff);
		}
		cancel_delayed_work(&locker->worker);
		locker->tid = -1;
	}
	mutex_unlock (&locker->mutex);
}


static inline void schedule_locker(struct locker *locker)
{
	if(atomic_read(&locker->debug_proc->tmp_ref) <= 0) { //debug, we are quitting, skip schedule
		printk("System Monitor: schedule_locker, we are quitting, just return \n");
		return;
	}

	mutex_lock (&locker->mutex);
	locker->dumped = false;
	locker->schedule_timestamp = jiffies;
	if (schedule_delayed_work(&locker->worker,locker->info->timeout) == false) {
		printk ("System Monitor: warning schedule locker worker locker=%p  lock_id=%llu lock type=%d \n",locker,locker->info->id, locker->info->type);
	}
	mutex_unlock (&locker->mutex);
}

void add_record_entry(int target_pid){
	struct tne_caller_record_entry * record_entry;
	record_entry = kzalloc(sizeof(*record_entry), GFP_ATOMIC);
	record_entry->target_pid = target_pid;
	record_entry->last_tne_called_time = jiffies;
	spin_lock(&tne_call_record_lock);
	hlist_add_head(&record_entry->hlist, &tne_call_record_entries);
	spin_unlock(&tne_call_record_lock);
}

int is_target_pid_tne_called_in_time(int target_pid){
	int ret = 0;
	struct tne_caller_record_entry *entry;
	struct hlist_node *tmp;
	unsigned long interval;
	spin_lock(&tne_call_record_lock);
	hlist_for_each_entry_safe(entry, tmp, &tne_call_record_entries, hlist) {
		interval = (jiffies - entry->last_tne_called_time) / HZ;
		if(target_pid == entry->target_pid) {
			if(interval <= 60){
				ret = 1;
				//printk("system monitor hung find record : %d,interval:%lu", entry->target_pid,interval);
				goto out;
			}
		}

		if(interval > 60){
			//printk("system monitor.hung record too long,delete it : %d,interval:%lu", entry->target_pid,interval);
			hlist_del(&entry->hlist);
			kfree(entry);
		}
	}
out:
	spin_unlock(&tne_call_record_lock);
	return ret;
}


#define DEFAULT_TIMEOUT  100   //100ms
void locker_timeout_handler(struct work_struct *work)
{
	struct locker *locker = container_of(work, struct locker, worker.work);
	int name_length = 0;

	if(atomic_read(&locker->debug_proc->tmp_ref) <= 0) { //debug, we are quitting, skip schedule
		printk("System Monitor: locker_timeout_handler, we are quitting, just return \n");
		return;
	}

	mutex_lock (&locker->mutex);

	if(locker->tid != -1) {
		switch(locker->info->type) {
			case LOCKER_MUTEX_LOCK:
			case LOCKER_SYS_LOOPER_HANDLE_MSG:
			case LOCKER_APP_LOOPER_HANDLE_MSG: {
				unsigned long timeout = 0;
				unsigned long app_looper_hung_tne_call_interval =0;
				timeout = jiffies - locker->schedule_timestamp;
				if(is_target_pid_tne_called_in_time(locker->info->pid)){
					locker->dumped = true;
				}
				if(locker->info->type == LOCKER_APP_LOOPER_HANDLE_MSG && !locker->dumped){
					spin_lock(&app_looper_hung_tne_call__lock);
					app_looper_hung_tne_call_interval = (jiffies - last_app_looper_hung_tne_called_time) / HZ;
					spin_unlock(&app_looper_hung_tne_call__lock);
					if(app_looper_hung_tne_call_interval <= 180){
						locker->dumped = true;
						printk("System Monitor:app looper hung tne called in times  ,pid:%d \n",locker->info->pid);
					}
				}
			#if IS_ENABLED(CONFIG_TRAN_FREEZE_AEGIS_SUPPORT)
				if(!locker->dumped)
				{
					printk("System Monitor:%s %s,%d,call tne  ,pid:%d \n",getLockerString(locker->info->type), __func__,__LINE__,locker->info->pid);
					if(locker->info->type == LOCKER_SYS_LOOPER_HANDLE_MSG){
						char pids[256] = {'\0'};
						snprintf(pids, sizeof(pids), "%d", locker->info->pid);
						startAegisK("SYS_LOOPER_HUNG", pids, "FLAG_TNE_LOOPER_HANDLE_MSG");
					}else if (locker->info->type == LOCKER_APP_LOOPER_HANDLE_MSG){
						char pids[256] = {'\0'};
						snprintf(pids, sizeof(pids), "%d", locker->info->pid);
						startAegisK("APP_LOOPER_HUNG", pids, "FLAG_TNE_LOOPER_HANDLE_MSG");
						spin_lock(&app_looper_hung_tne_call__lock);
						last_app_looper_hung_tne_called_time = jiffies;
						spin_unlock(&app_looper_hung_tne_call__lock);
					}
					locker->dumped = true;
					add_record_entry(locker->info->pid);
				}
			#endif
				if((timeout) < 240*HZ) {
					char name_cache[128] = {'\0'}; // add by chonglin.yang for fix kasan locker->info->name index out of array
					name_length = sizeof(name_cache)-1 >= locker->info->size ? locker->info->size : sizeof(name_cache)-1;
					strncpy(name_cache,locker->info->name,name_length);
					printk ("System Monitor: %s %s timeout pid=%d  tid=%d  duration=%u ms\n",
					getLockerString(locker->info->type), name_cache,
					locker->info->pid,locker->tid, jiffies_to_msecs(jiffies - locker->schedule_timestamp));
					schedule_delayed_work(&locker->worker, timeout * 2);
				} else {
					locker->tid = -1;
				}
			}
				break;
			default:
				break;
		}
	}

	mutex_unlock (&locker->mutex);
}

static struct locker *create_locker(struct monitor_proc *debug_proc, struct lock_info_kernel *info, int tid)
{
	//void * private_data = NULL;
	struct locker *locker = NULL;

	locker = (struct locker *)kzalloc(sizeof (struct locker),GFP_KERNEL);
	if (!locker) {
		printk("System Monitor: %s,%d \n", __func__,__LINE__);
		return NULL;
	}

	INIT_LIST_HEAD(&locker->list);
	locker->info = info;
	locker->tid = tid;
	locker->dumped = false;

	INIT_DELAYED_WORK(&locker->worker,locker_timeout_handler);
	locker->debug_proc = debug_proc;
	mutex_init(&locker->mutex);

	return locker;
}

static void add_locker_to_list(struct monitor_proc *debug_proc, struct locker * locker)
{
	unsigned long hash_value;

	switch(locker->info->type) {
		case LOCKER_SYS_LOOPER_HANDLE_MSG:
		case LOCKER_APP_LOOPER_HANDLE_MSG:
			hash_value = ((unsigned long)((locker->info->id>>4) + locker->info->pid)) % ARRAY_SIZE(debug_proc->hash_locker);

			mutex_lock(&debug_proc->hash_locker_lock);
			list_add(&locker->list,&debug_proc->hash_locker[hash_value]);
			mutex_unlock(&debug_proc->hash_locker_lock);
		break;
	}
}

static struct locker *find_locker(struct monitor_proc *debug_proc, int pid, u64 lock_id, int type)
{
	struct locker *locker = NULL,*n = NULL;
	unsigned long hash_value;
	int found = 0;

	switch(type) {
		case LOCKER_SYS_LOOPER_HANDLE_MSG:
		case LOCKER_APP_LOOPER_HANDLE_MSG:
			hash_value = ((unsigned long)((lock_id>>4) + pid)) % ARRAY_SIZE (debug_proc->hash_locker);
			mutex_lock(&debug_proc->hash_locker_lock);
			list_for_each_entry_safe(locker, n,&debug_proc->hash_locker[hash_value], list) {
				if(!IS_ERR_OR_NULL(locker) && !IS_ERR_OR_NULL(locker->info) &&locker->info->id == lock_id && locker->info->pid == pid) {
					found = 1;
					break;
				}
			}
			mutex_unlock(&debug_proc->hash_locker_lock);
		break;
	}

	if (found == 0) {
		//printk("System Monitor: %s,%d \n", __func__,__LINE__);
		locker = NULL;
	}

	return locker;
}


static int hander_locker_init_info(struct monitor_proc *debug_proc, unsigned int cmd, u64 arg, unsigned int flags)
{
	u64 size = 0;
	struct lock_info_kernel *info = NULL;
	struct locker *locker;

	if (copy_from_user(&size,(const void __user *)arg, sizeof (size)))
	{
		printk("System Monitor: %s,%d \n", __func__,__LINE__);
		return -EFAULT;
	}

	info = (struct lock_info_kernel *)kzalloc(sizeof(struct lock_info_kernel) + size, GFP_KERNEL);

	if(IS_ERR_OR_NULL(info)) {
		printk("System Monitor: %s,%d \n", __func__,__LINE__);
		return -ENOMEM;
	}

	if (copy_from_user(info, (const void __user *)arg, sizeof(struct lock_info_kernel) + size)) {
		//copy full structure
		printk("System Monitor: %s,%d \n", __func__,__LINE__);
		return -EFAULT;
	}

	if(current->group_leader->pid != info->pid) { //this info may be need to modify for transaction
		printk("System Monitor: %s,%d \n", __func__,__LINE__);
		return -EIO;//we check the message from framework
	}

	info->timeout = __msecs_to_jiffies(info->timeout); //ms to jiffies

	locker = find_locker(debug_proc, current->group_leader->pid, info->id, info->type);
	if (IS_ERR_OR_NULL(locker)) {
		locker = create_locker(debug_proc, info, current->pid);
		if (!locker) {
			printk("System Monitor: %s,%d \n", __func__,__LINE__);
			return -ENOMEM;
		}
		add_locker_to_list(debug_proc, locker);
	}

	return 0;
}

static int command_locker(struct monitor_proc *debug_proc, unsigned int cmd, u64 arg, unsigned int flags)
{
	int ret = 0;
	int pid = current->group_leader->pid;

	if(IS_ERR_OR_NULL(debug_proc)) {
		printk("System Monitor: %s,%d \n", __func__,__LINE__);
		return -EFAULT;
	}

	if (debug_proc->pid != pid) {
		//please open it again.
		printk("System Monitor: %s,%d ,debug_proc->pid:%d,current->group_leader->pid:%d \n", __func__,__LINE__,debug_proc->pid,pid);
		return -EIO;
	}

	switch (cmd) {
		//looper handle message monitor start
		case START_HANDLE_MSG: {
			u64 lock_id;
			struct locker *locker;
			lock_id = arg;
			locker = find_locker(debug_proc, pid, lock_id, LOCKER_SYS_LOOPER_HANDLE_MSG);
			if (IS_ERR_OR_NULL(locker)) {
				printk ("System Monitor: this handle does not inited. \n");
				ret = -EFAULT;
				goto out;
			}

			mutex_lock(&locker->mutex);
			locker->tid = current->pid;
			mutex_unlock(&locker->mutex);

			schedule_locker(locker);
		}
			break;

		case FINISH_HANDLE_MSG: {
			u64 lock_id;
			struct locker *locker;
			lock_id = arg;
			locker = find_locker(debug_proc, pid, lock_id, LOCKER_SYS_LOOPER_HANDLE_MSG);
			if (locker != NULL) {
				reset_locker(locker);
			}
		}
			break;
		//looper handle message monitor end

		case SET_TIMEOUT:
			printk("System Monitor: should not support Now!\n");
			break;
		case SET_INIT_INFO: {
			ret = hander_locker_init_info(debug_proc, cmd, arg, flags);
		}
			break;
		default:
			break;
	}

out:
	return ret;
}

static struct monitor_proc * new_monitor_proc(void)
{
	int i = 0;
	struct monitor_proc *debug_proc;
	debug_proc = kzalloc(sizeof(struct monitor_proc), GFP_KERNEL);
	if (!debug_proc) {
	    return NULL;
	}

	++monitor_procs_count;
	debug_proc->group_leader = current->group_leader;
	debug_proc->pid = current->group_leader->pid;
	list_add (&debug_proc->list,&monitor_procs);

	//locker monitor
	for (;i < ARRAY_SIZE (debug_proc->hash_locker);++i) {
		INIT_LIST_HEAD (&debug_proc->hash_locker[i]);
	}
	mutex_init (&debug_proc->hash_locker_lock);

	return debug_proc;
}

static void free_monitor_proc_locked(struct monitor_proc *debug_proc)
{
	struct locker *locker;
	int i = 0;

	//free hash_locker
	mutex_lock(&debug_proc->hash_locker_lock);
	for (i = 0; i < ARRAY_SIZE (debug_proc->hash_locker); ++i) {
		while (!list_empty(&debug_proc->hash_locker[i])) {
			locker = list_first_entry(&debug_proc->hash_locker[i], struct locker, list);
			free_locker(locker);
	    }
	}
	mutex_unlock(&debug_proc->hash_locker_lock);
	--monitor_procs_count;
	kfree(debug_proc);
}

static void reset_monitor_proc_locked(struct monitor_proc *debug_proc)
{
	struct locker *locker,*n;
	int i = 0;

	//reset hash_locker
	mutex_lock(&debug_proc->hash_locker_lock);
	for (i = 0; i < ARRAY_SIZE (debug_proc->hash_locker); ++i) {
		list_for_each_entry_safe(locker,n,&debug_proc->hash_locker[i],list) {
			if(!IS_ERR_OR_NULL(locker))
			{
				//printk("System Monitor: reset_monitor_proc_locked \n");
				reset_locker(locker);
			}
		}
	}
	mutex_unlock(&debug_proc->hash_locker_lock);
}

static int monitor_release(struct inode *inode, struct file *file)
{
	struct monitor_proc *debug_proc = file->private_data;
	//printk("System Monitor: %s\n", __func__);
	file->private_data = NULL;
	if (atomic_dec_and_test(&debug_proc->tmp_ref)) {
		mutex_lock(&monitor_procs_lock);
		list_del(&debug_proc->list);
		mutex_unlock(&monitor_procs_lock);
		free_monitor_proc_locked(debug_proc);
	}
	return 0;
}

static int monitor_open(struct inode *nodp, struct file *file)
{
	struct monitor_proc *proc,*n;
	struct monitor_proc *debug_proc = NULL;
	//printk("System Monitor: %s\n", __func__);

	//Get/Create current proc's debug_proc struct;return ENOMEM for create fail
	mutex_lock(&monitor_procs_lock);
	list_for_each_entry_safe(proc, n, &monitor_procs, list) {
		if (proc->pid == current->group_leader->pid/*debug_proc->pid*/) {
			debug_proc = proc;
			break;
		}
	}

	if (IS_ERR_OR_NULL(debug_proc)) {// not found
		debug_proc = new_monitor_proc();
		if(IS_ERR_OR_NULL(debug_proc)) {
			mutex_unlock(&monitor_procs_lock);
			return -ENOMEM;
		}
	}

	atomic_inc(&debug_proc->tmp_ref);// increase ref
	mutex_unlock(&monitor_procs_lock);

	// set the private_data, which will be used in ioctl
	file->private_data = debug_proc;

	return 0;
}

static ssize_t monitor_write(struct file *filp, const char __user *user, size_t size, loff_t *offset)
{
	printk("System Monitor: %s\n", __func__);
	return 0;
}

static ssize_t monitor_read(struct file *file, char __user *user, size_t size,loff_t *ppos)
{
	printk("System Monitor: %s\n", __func__);
	return 0;
}

static long monitor_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct monitor_proc *debug_proc = filp->private_data;
	long ret = 0;
	//printk("System Monitor: %s,cmd:%d\n", __func__,cmd);
	ret = command_locker(debug_proc, cmd, (u64)arg, LOCKER_MUTEX_LOCK);
	return ret;
}


static unsigned int monitor_poll(struct file * filp, struct poll_table_struct * table)
{
	printk("System Monitor: %s\n", __func__);
	return 0;
}


static const struct file_operations monitor_fops = {
	.owner = THIS_MODULE,
	.open = monitor_open,
	.read = monitor_read,
	.write = monitor_write,
	.unlocked_ioctl = monitor_ioctl,
	.compat_ioctl = monitor_ioctl,
	.release = monitor_release,
	.poll = monitor_poll,
};

static struct miscdevice monitor_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = ENHANCE_SIGNAL_DEVICE_NAME,
	.fops = &monitor_fops,
};

 static void reset_monitor_procs(void)
 {
	 struct monitor_proc *proc,*n;
	 printk("System Monitor: %s,monitor_procs_count:%d \n", __func__,monitor_procs_count);

	 mutex_lock(&monitor_procs_lock);
	 list_for_each_entry_safe(proc, n, &monitor_procs, list) {
		 if(!IS_ERR_OR_NULL(proc))
		 {
			 reset_monitor_proc_locked(proc);
		 }
	 }
	 mutex_unlock(&monitor_procs_lock);
 }

 bool system_monitor_init(void)
{
	int ret = -1;
	ret = misc_register(&monitor_miscdev);
	printk("System Monitor: %s,%d ,ret:%d \n", __func__,__LINE__,ret);
	return ret == 0 ? true : false;
}

 void system_monitor_exit(void)
 {
	printk("System Monitor: %s\n", __func__);
	reset_monitor_procs();
	misc_deregister(&monitor_miscdev);
 }
