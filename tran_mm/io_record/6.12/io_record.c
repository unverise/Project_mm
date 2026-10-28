#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/sort.h>
#include <linux/vmalloc.h>
#include <linux/rcupdate.h>
#include <linux/array_size.h>
#include <trace/hooks/fs.h>
#include <trace/hooks/mm.h>
#include "io_record.h"

static struct io_info *record_buf; /* array of struct io_info */
static void *result_buf; /* buffer used for post processing result */
static void *result_buf_cursor; /* this is touched by post processing only */
static atomic_t record_buf_cursor = ATOMIC_INIT(-1); /* record_buf array idx */
static atomic_t record_target = ATOMIC_INIT(-1); /* pid # of group leader */
static atomic_t record_enable = ATOMIC_INIT(0);
static DEFINE_MUTEX(status_lock);
static enum io_record_cmd_types current_status = IO_RECORD_INIT;

static inline void set_record_target(int pid)
{
    atomic_set(&record_target, pid);
}

static inline void set_record_enable(bool enable)
{
    atomic_set(&record_enable, enable);
}

static void write_to_result_buf(void *src, int size)
{
    memcpy(result_buf_cursor, src, size);
    result_buf_cursor = result_buf_cursor + size;
}

/* this assumes that start_idx~end_idx belong to the same inode */
static int fill_result_buf(int start_idx, int end_idx)
{
    struct file *file;
    char *path, strbuf[MAX_FILEPATH_LEN];
    int i, size_expected, pathsize, result_buf_used;
    int prev_offset = -1;
    int max_size = 0;
    void *buf_start;

    file = record_buf[start_idx].file;
    path = d_path(&file->f_path, strbuf, MAX_FILEPATH_LEN);
    if (!path || IS_ERR(path))
        return 0;

    /* max size check (not strict) */
    result_buf_used = result_buf_cursor - result_buf;
    size_expected = sizeof(int) * 2 +                         /* end magic of this attempt */
            sizeof(int) + strlen(path) +              /* for path string */
            sizeof(int) * 2 * (end_idx - start_idx) + /* data */
            sizeof(int);                              /* end magic of post-processing */
    if (size_expected > RESULT_BUF_SIZE_IN_BYTES - result_buf_used)
        return -EINVAL;

    buf_start = result_buf_cursor;
    pathsize = strlen(path);
    write_to_result_buf(&pathsize, sizeof(int));
    write_to_result_buf(path, pathsize);

    /* fill the result buf using the record buf */
    for (i = start_idx; i < end_idx; i++) {
        if (prev_offset == -1) {
            prev_offset = record_buf[i].offset;
            max_size = record_buf[i].nr_pages;
            continue;
        }
        /* in the last range */
        if (prev_offset + max_size >=
            record_buf[i].offset + record_buf[i].nr_pages)
            continue;

        if (prev_offset + max_size >= record_buf[i].offset) {
            max_size = record_buf[i].offset +
                record_buf[i].nr_pages - prev_offset;
        } else {
            write_to_result_buf(&prev_offset, sizeof(int));
            write_to_result_buf(&max_size, sizeof(int));
            prev_offset = record_buf[i].offset;
            max_size = record_buf[i].nr_pages;
        }
    }
    /* fill the record buf */
    write_to_result_buf(&prev_offset, sizeof(int));
    write_to_result_buf(&max_size, sizeof(int));

    /* fill the record buf with final magic */
    prev_offset = RESULT_BUF_END_MAGIC;
    max_size = RESULT_BUF_END_MAGIC;
    write_to_result_buf(&prev_offset, sizeof(int));
    write_to_result_buf(&max_size, sizeof(int));

    /* return # of bytes written to result buf */
    return result_buf_cursor - buf_start;
}

static void io_info_swap(void *lhs, void *rhs, int size)
{
    struct io_info tmp;
    struct io_info *linfo = (struct io_info *)lhs;
    struct io_info *rinfo = (struct io_info *)rhs;

    memcpy(&tmp, linfo, sizeof(struct io_info));
    memcpy(linfo, rinfo, sizeof(struct io_info));
    memcpy(rinfo, &tmp, sizeof(struct io_info));
}

static int io_info_compare(const void *lhs, const void *rhs)
{
    struct io_info *linfo = (struct io_info *)lhs;
    struct io_info *rinfo = (struct io_info *)rhs;

    if ((unsigned long)linfo->inode > (unsigned long)rinfo->inode)
        return 1;
    if ((unsigned long)linfo->inode < (unsigned long)rinfo->inode)
        return -1;
    if ((unsigned long)linfo->offset > (unsigned long)rinfo->offset)
        return 1;
    if ((unsigned long)linfo->offset < (unsigned long)rinfo->offset)
        return -1;
    return 0;
}

static void release_records(void)
{
    struct io_info *info;
    int i;

    for (i = 0; i < atomic_read(&record_buf_cursor); i++) {
        info = record_buf + i;
        fput(info->file);
    }
}

/* change the current status, and do the init jobs for the status */
static void change_current_status(enum io_record_cmd_types status)
{
    switch (status) {
    case IO_RECORD_INIT:
        set_record_enable(false);
        set_record_target(-1);
        release_records();
        atomic_set(&record_buf_cursor, 0);
        result_buf_cursor = result_buf;
        break;
    case IO_RECORD_START:
        set_record_enable(true);
        break;
    case IO_RECORD_STOP:
        set_record_enable(false);
        break;
    default:
        break;
    }
    current_status = status;
}

static inline bool change_status_if_valid(enum io_record_cmd_types next_status)
{
    bool ret = false;

    if (!record_buf)
        return ret;

    if (next_status == IO_RECORD_INIT &&
        current_status != IO_RECORD_POST_PROCESSING)
        ret = true;
    else if (next_status == current_status + 1)
        ret = true;
    if (ret)
        change_current_status(next_status);

    return ret;
}

static bool post_processing_records(void)
{
    struct inode *prev = NULL;
    int start_idx = -1, end_idx = -1;
    int last_magic = RESULT_BUF_END_MAGIC;
    int i;
    bool ret = false;
    unsigned long start_jiffies, end_jiffies;

    start_jiffies = jiffies;

    mutex_lock(&status_lock);
    if (!change_status_if_valid(IO_RECORD_POST_PROCESSING))
        goto out;

    /* From this point, we assume that no one touches record buf */
    /* sort based on inode pointer address */
    sort(record_buf, atomic_read(&record_buf_cursor),
         sizeof(struct io_info), &io_info_compare, &io_info_swap);

    /* fill the result buf per inode */
    for (i = 0; i < atomic_read(&record_buf_cursor); i++) {
        if (prev != record_buf[i].inode) {
            end_idx = i;
            /* if result buf full, break without write */
            if (prev && fill_result_buf(start_idx, end_idx) < 0)
                break;
            prev = record_buf[i].inode;
            start_idx = i;
        }
    }

    if (start_idx != -1)
        fill_result_buf(start_idx, i);

    /* fill the last magic to indicate end of result */
    write_to_result_buf(&last_magic, sizeof(int));
    ret = change_status_if_valid(IO_RECORD_POST_PROCESSING_DONE);
out:
    mutex_unlock(&status_lock);

    end_jiffies = jiffies;
    tag_pr_info("post_processing_records took %u ms, processed %d records\n",
             jiffies_to_msecs(end_jiffies - start_jiffies),
             atomic_read(&record_buf_cursor));

    return ret;
}


static bool set_record_status(enum io_record_cmd_types status, int pid)
{
    bool ret;

    mutex_lock(&status_lock);
    ret = change_status_if_valid(status);
    if (ret && status == IO_RECORD_START)
        set_record_target(pid);
    mutex_unlock(&status_lock);
    return ret;
}


static ssize_t tran_io_record_proc_read(struct file *file,
        char __user *buf, size_t count, loff_t *ppos)
{
    int result_buf_size;
    int ret;

    mutex_lock(&status_lock);
    if (current_status != IO_RECORD_POST_PROCESSING_DONE) {
        ret = -EFAULT;
        goto out;
    }

    result_buf_size = result_buf_cursor - result_buf;
    if (*ppos >= result_buf_size) {
        ret = 0;
        goto out;
    }

    ret = *ppos + count < result_buf_size ?
            count : result_buf_size - *ppos;
    if (copy_to_user(buf, result_buf + *ppos, ret)) {
        ret = -EFAULT;
        goto out;
    }

    *ppos = *ppos + ret;
out:
    mutex_unlock(&status_lock);
    return ret;
}

static ssize_t tran_io_record_proc_write(struct file *file,
        const char __user *buffer, size_t count, loff_t *pos)
{
    int pid = 0;
    int cmd = 0;
    int task_pid = -1;
    struct task_struct *task;
    bool ret = false;

    char *buf = (char *) __get_free_page(GFP_USER);

    if (!buf)
        return -ENOMEM;

    if (count > 255) {
        free_page((unsigned long)buf);
        return -EINVAL;
    }

    if (copy_from_user(buf, buffer, count)) {
        free_page((unsigned long)buf);
        return -EINVAL;
    }

    if (sscanf(buf, "%d %d", &pid, &cmd) != 2) {
        free_page((unsigned long)buf);
        return -EINVAL;
    }

    free_page((unsigned long)buf);
    if (pid <= 0 || cmd <= 0)
        return -EINVAL;

    rcu_read_lock();
    task = find_task_by_vpid(pid);
    if (task) {
        get_task_struct(task);
        task_pid = task_pid_nr(task);
        put_task_struct(task);
    }
    rcu_read_unlock();

    if (task_pid != -1) {
        switch (cmd) {
        case IO_RECORD_INIT:
        case IO_RECORD_START:
        case IO_RECORD_STOP:
            ret = set_record_status(cmd, task_pid);
            break;
        case IO_RECORD_POST_PROCESSING:
            ret = post_processing_records();
            break;
        default:
            ret = false;
            break;
        }
    }

    return ret ? count : -EINVAL;
}

static inline bool __get_record_status(void)
{
    return atomic_read(&record_enable);
}

static void io_record_store(struct file *file, pgoff_t offset, int nr_pages)
{
    struct io_info *info;
    int cnt;

    if (!file || !nr_pages)
        return;

    /* check without lock */
    if (task_tgid_nr(current) != atomic_read(&record_target))
        return;

    cnt = atomic_read(&record_buf_cursor);
    if (cnt < 0 || cnt >= NUM_IO_INFO_IN_BUF)
        return;

    if (!__get_record_status())
        return;

    /* strict check */
    if (task_tgid_nr(current) != atomic_read(&record_target))
        return;

    cnt = atomic_inc_return(&record_buf_cursor) - 1;

    /* buffer is full */
    if (cnt >= NUM_IO_INFO_IN_BUF) {
        atomic_dec(&record_buf_cursor);
        return;
    }

    info = record_buf + cnt;

    get_file(file); /* will be put in release_records */
    info->file = file;
    info->inode = file_inode(file);
    info->offset = (int)offset;
    info->nr_pages = nr_pages;
}

static void io_record_do_read_fault(void *data, struct vm_fault *vmf,
        unsigned long fault_around_pages)
{
    if (vmf->vma->vm_ops->map_pages && fault_around_pages == 1)
        io_record_store(vmf->vma->vm_file, vmf->pgoff, 1);
}

static void io_record_filemap_map_pages(void *data, struct file *file,
        pgoff_t orig_start_pgoff, pgoff_t first_pgoff,
        pgoff_t last_pgoff, vm_fault_t ret)
{
    if (ret == VM_FAULT_NOPAGE)
        io_record_store(file, orig_start_pgoff, last_pgoff - orig_start_pgoff + 1);
}

static void io_record_filemap_read(void *data, struct file *file,
        loff_t pos, size_t size)
{
    io_record_store(file, pos >> PAGE_SHIFT,
            (size + PAGE_SIZE - 1) >> PAGE_SHIFT);
}

PROC_FOPS_RW(io_record);

static int __init init_io_record(void)
{
    struct proc_dir_entry *dir = NULL;
    int i;

    struct pentry {
        const char *name;
        const struct proc_ops *fops;
    };

    const struct pentry entries[] = {
        PROC_ENTRY(io_record),
    };

    dir = proc_mkdir("tran_io_record", NULL);
    if (!dir) {
        tag_pr_notice("fail to create /proc/tran_io_record @ %s()\n",
                                __func__);
        return -ENOMEM;
    }

    for (i = 0; i < ARRAY_SIZE(entries); i++) {
        if (!proc_create
            (entries[i].name, 0664, dir, entries[i].fops)) {
                tag_pr_notice("%s(), create /proc/tran_io_record/%s failed\n",
                    __func__, entries[i].name);
                remove_proc_entry("tran_io_record", NULL);
                return -ENOMEM;
            }

    }

    record_buf = vzalloc(sizeof(struct io_info) * NUM_IO_INFO_IN_BUF);
    if (!record_buf) {
        remove_proc_entry("tran_io_record", NULL);
        return -ENOMEM;
    }

    result_buf = vzalloc(RESULT_BUF_SIZE_IN_BYTES);
    if (!result_buf) {
        vfree(record_buf);
        remove_proc_entry("tran_io_record", NULL);
        return -ENOMEM;
    }

    mutex_lock(&status_lock);
    change_status_if_valid(IO_RECORD_INIT);
    mutex_unlock(&status_lock);

    if (register_trace_android_vh_do_read_fault(
            io_record_do_read_fault, NULL)) {
        tag_pr_notice("fail to register trace android_vh_do_read_fault @ %s()\n",
                        __func__);
        goto err_free_buf;
    }

    if (register_trace_android_vh_filemap_map_pages(
            io_record_filemap_map_pages, NULL)) {
        tag_pr_notice("fail to register trace android_vh_filemap_map_pages @ %s()\n",
                        __func__);
        goto err_unregister_read_fault;
    }

    if (register_trace_android_vh_filemap_read(
            io_record_filemap_read, NULL)) {
        tag_pr_notice("fail to register trace android_vh_filemap_read @ %s()\n",
                        __func__);
        goto err_unregister_map_pages;
    }

    return 0;

err_unregister_map_pages:
    unregister_trace_android_vh_filemap_map_pages(
            io_record_filemap_map_pages, NULL);
err_unregister_read_fault:
    unregister_trace_android_vh_do_read_fault(
            io_record_do_read_fault, NULL);
err_free_buf:
    vfree(result_buf);
    vfree(record_buf);
    remove_proc_entry("tran_io_record", NULL);
    return -EINVAL;
}

static void __exit exit_io_record(void)
{
    vfree(record_buf);
    vfree(result_buf);

    unregister_trace_android_vh_do_read_fault(
            io_record_do_read_fault, NULL);
    unregister_trace_android_vh_filemap_map_pages(
            io_record_filemap_map_pages, NULL);
    unregister_trace_android_vh_filemap_read(
            io_record_filemap_read, NULL);

    remove_proc_entry("tran_io_record", NULL);
}

module_init(init_io_record);
module_exit(exit_io_record);

MODULE_LICENSE("GPL");