#ifndef __IO_RECORD_H__
#define __IO_RECORD_H__

#include <linux/printk.h>

/* PROCFS */
#define PROC_ENTRY(name) {__stringify(name), &tran_ ## name ## _proc_fops}
#define PROC_FOPS_RW(name) \
static const struct proc_ops tran_ ## name ## _proc_fops = { \
    .proc_read    = tran_ ## name ## _proc_read, \
    .proc_write    = tran_ ## name ## _proc_write,\
}

#define PROC_FOPS_RO(name) \
static const struct proc_ops tran_ ## name ## _proc_fops = { \
    .proc_open    = tran_ ## name ## _proc_open, \
}

#define TAG    "[IO/record] "
#define tag_pr_notice(fmt, args...)    pr_notice(TAG fmt, ##args)
#define tag_pr_info(fmt, args...)    pr_info(TAG fmt, ##args)
#define tag_pr_debug(fmt, args...)    pr_debug(TAG fmt, ##args)
#define tag_pr_deferred(fmt, args...)   printk_deferred(TAG fmt, ##args)


#define NUM_IO_INFO_IN_BUF (32 * 1024) /* # of struct io_info */
#define RESULT_BUF_SIZE_IN_BYTES (512 * 1024) /* 512 KB */
#define RESULT_BUF_END_MAGIC (~0) /* -1 */

#define MAX_FILEPATH_LEN 256

struct io_info {
    struct file *file;
    struct inode *inode;
    int offset;
    int nr_pages;
};

enum io_record_cmd_types {
    IO_RECORD_INIT = 1,
    IO_RECORD_START = 2,
    IO_RECORD_STOP = 3,
    IO_RECORD_POST_PROCESSING = 4,
    IO_RECORD_POST_PROCESSING_DONE = 5,
};

#endif