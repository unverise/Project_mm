#ifndef __TRANMEM_PSI_H
#define __TRANMEM_PSI_H

#include <linux/interrupt.h>

extern void tranmem_psi_init(void);
extern void tranmem_psi_exit(void);

extern void update_psi_mem_data(void);
extern bool tranmem_psi_enabled(void);
extern void tranmem_psi_proc_create(struct proc_dir_entry* parent);

#endif   //__TRANMEM_PSI_H
