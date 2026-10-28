#ifndef __TRANMEM_INODES_PROTECT_H__
#define __TRANMEM_INODES_PROTECT_H__

#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mm_types.h>

#define MAX_LEN_FILE_PATH   128

typedef struct {
    char file_name[MAX_LEN_FILE_PATH];
    atomic64_t nr_protected;
    unsigned long ino;
    dev_t s_dev;
    struct list_head list;
} inode_protect_t;


/**
 * Whether the folio is protected by inode.
 * If the folio is protected by inode, increment the nr_protected.
 * @param folio: the folio to check.
 * @return: true if the folio is protected by inode, false otherwise.
 */
bool folio_protected_by_inode(struct folio *folio);

/**
 * Create more proc files in {@param parent} dir.
 */
void inodes_protect_proc_create(struct proc_dir_entry* parent);

/**
 * Init for inodes protection.
 */
int  inodes_protect_init(void);
void inodes_protect_exit(void);

#endif  //__TRANMEM_INODES_PROTECT_H__