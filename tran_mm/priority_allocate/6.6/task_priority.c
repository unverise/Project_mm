// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 Transsion. All rights reserved.
 */

#include <linux/module.h>
#include <trace/hooks/vmscan.h>
#include <trace/hooks/mm.h>
#include <linux/swap.h>
#include <linux/proc_fs.h>
#include <linux/gfp.h>
#include <linux/spinlock.h>
#include <uapi/linux/sched/types.h>
#include <linux/cpufreq.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/pagemap.h>
#include <linux/page-flags.h>
#include <linux/pageblock-flags.h>
#include <linux/debugfs.h>
#include <linux/memcontrol.h>
#include <linux/mm_types.h>
#include <linux/kasan.h>
#include <linux/page_owner.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/cgroup.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/vmstat.h>
#include <linux/oom.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/page_ref.h>
#include <linux/mmzone.h>
#include <linux/kprobes.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/seq_file.h>
#include <linux/sched/stat.h>
#include <linux/sched/types.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/cputime.h>
#include <kernel/sched/sched.h>

#include "priority_allocate.h"

struct priority_data {
    pid_t pid;
    bool tag;
};

struct task_priority_struct {
	bool high_priority;
};

#define TRANMEM_MANAGEMENT_OFFSET (2)

#define PRIORITY_ALLOCATE_MAGIC 'U'

#define SET_TASK_PRIORITY       _IOW(PRIORITY_ALLOCATE_MAGIC, 0x01, struct priority_data)
#define GET_TASK_PRIORITY       _IOW(PRIORITY_ALLOCATE_MAGIC, 0x02, struct priority_data)

static inline struct task_priority_struct *get_task_priority_struct(struct task_struct *task)
{
    // task->ANDROID_OEM_DATA_ARRAY(1, xx);
    return (struct task_priority_struct *)&(task->android_oem_data1[TRANMEM_MANAGEMENT_OFFSET]);
}

inline bool get_priority_tags(struct task_struct *task)
{
    struct task_priority_struct *task_priority = get_task_priority_struct(task);
    if (IS_ERR_OR_NULL(task_priority)) {
        return false;
    }

    return task_priority->high_priority;
}

inline void set_priority_tags(struct task_struct *task, bool priority_tag)
{
    struct task_priority_struct *task_priority = get_task_priority_struct(task);
    if (IS_ERR_OR_NULL(task_priority)) {
        return;
    }

    task_priority->high_priority = priority_tag;
}

bool task_is_high_priority(struct task_struct *task)
{
    struct task_priority_struct *task_priority = get_task_priority_struct(task);
    if (IS_ERR_OR_NULL(task_priority)) {
        return false;
    }

    return task_priority->high_priority;
}

static long task_priority_ioctl(struct file *file, unsigned int cmd, unsigned long parm)
{
    struct priority_data vip_tags = {0};
    struct task_struct *task = NULL;

    switch (cmd)
    {
        case SET_TASK_PRIORITY:{
            
            if (!check_priority_allocate_feature()) {
                pr_err("priority allcoate disabled!\n");
                return -EFAULT;
            }

            if (copy_from_user(&vip_tags, (struct priority_data *)parm, sizeof(struct priority_data))) {
                pr_err("receive priority_data failed!\n");
                return -EFAULT;
            }

            if (vip_tags.pid <= 0 || vip_tags.pid > PID_MAX_DEFAULT) {
                pr_err("%d is not available pid!\n", vip_tags.pid);
                return -EINVAL;
            }

            rcu_read_lock();
            task = find_task_by_vpid(vip_tags.pid);
            if (task) {
                get_task_struct(task);
                set_priority_tags(task, vip_tags.tag);
                put_task_struct(task);
            }
            rcu_read_unlock();

            break;
        }
        case GET_TASK_PRIORITY:{

            if (!check_priority_allocate_feature()) {
                pr_err("priority allcoate disabled!\n");
                return -EFAULT;
            }

            if (copy_from_user(&vip_tags, (struct priority_data *)parm, sizeof(struct priority_data))) {
                pr_err("receive priority_data failed!\n");
                return -EFAULT;
            }

            if (vip_tags.pid <= 0 || vip_tags.pid > PID_MAX_DEFAULT) {
                pr_err("%d is not available pid!\n", vip_tags.pid);
                return -EINVAL;
            }

            rcu_read_lock();
            task = find_task_by_vpid(vip_tags.pid);
            if (task) {
                get_task_struct(task);
                vip_tags.tag = get_priority_tags(task);
                put_task_struct(task);
            }
            rcu_read_unlock();

            if (copy_to_user((struct priority_data *)parm, &vip_tags, sizeof(struct priority_data)))
            {
                pr_err("send vip_tags failed!\n");
                return -EFAULT;
            }
            break;
        }
        default:
            break;
    }

    return 0;
}

static const struct proc_ops task_priority_fops = {
    .proc_ioctl = task_priority_ioctl,
    .proc_lseek = default_llseek,
};

int vip_task_priority_init(struct proc_dir_entry *priority_allocate_dir)
{
    int ret = 0;

    if (priority_allocate_dir == NULL)
        return -1;

    if (proc_create("task_priority", 0666, priority_allocate_dir, &task_priority_fops) == NULL)
        ret = -1;

    return ret;
}
