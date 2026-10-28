#ifndef __DIRECT_RECLAIM_H
#define __DIRECT_RECLAIM_H

#include <linux/interrupt.h>

#define DIRECT_RECLAIM_RATIO_LIMIT_FIXED 10  /* <= 0.01% is normal,0.01*FIXED_FACTOR = 10 */
#define PERCENT_FACTOR 100                   //%
#define FIXED_FACTOR 1000                    //0.001
#define INTEGER_PART_DIRECT_RECLAIM(x) (x / FIXED_FACTOR)
#define FRACTIONAL_PART_DIRECT_RECLAIM(x) (x % FIXED_FACTOR)

extern atomic_long_t key_process_pgfault_total;
extern atomic_long_t key_process_direct_reclaim_total;
extern void tranmem_direct_reclaim_init(void);
extern void tranmem_direct_reclaim_exit(void);
extern void update_direct_reclaim_data(void);
extern void show_direct_reclaim_data(void);
extern void tranmem_direct_reclaim_proc_create(struct proc_dir_entry* parent);
extern void check_direct_reclaim_ratio_key_process_limit(unsigned long current_direct_reclaim_effciency);

#endif   //__DIRECT_RECLAIM_H
