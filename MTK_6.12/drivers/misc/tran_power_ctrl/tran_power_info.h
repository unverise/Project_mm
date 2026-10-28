/**
  *Copyright (c) 2025 Transsion Inc
  *Author jian.zhang7
  *Add tran_power_ctrl module in kernel
  */
#ifndef __TRAN_POWER_INFO_H__
#define __TRAN_POWER_INFO_H__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/sort.h>
#include <linux/platform_device.h>

// Define constants to replace magic numbers
#define IDLE_STATUS_UNKNOWN_IDX    18
#define BLOCK_SYSTEM_UNKNOWN_IDX   13
#define WAKEUP_SOURCE_UNKNOWN_IDX  31

// Define wakeup source structure, including name and count
struct wakeup_source_info {
    char name[32];
    atomic_t count;
    atomic_t suspend_count;
};
struct block_system_info {
    char name[32];
    atomic_t count;
};
struct idle_status_info {
    char name[32];
    atomic_t count;
};

static int block_system_cmp(const void *a, const void *b)
{
    const struct block_system_info *info_a = (const struct block_system_info *)a;
    const struct block_system_info *info_b = (const struct block_system_info *)b;
    if (atomic_read(&info_a->count) < atomic_read(&info_b->count))
        return 1;
    else if (atomic_read(&info_a->count) > atomic_read(&info_b->count))
        return -1;
    return 0;
}

static int idle_status_cmp(const void *a, const void *b)
{
    const struct idle_status_info *info_a = (const struct idle_status_info *)a;
    const struct idle_status_info *info_b = (const struct idle_status_info *)b;
    if (atomic_read(&info_a->count) < atomic_read(&info_b->count))
        return 1;
    else if (atomic_read(&info_a->count) > atomic_read(&info_b->count))
        return -1;
    return 0;
}


#endif // __TRAN_POWER_INFO_H__
