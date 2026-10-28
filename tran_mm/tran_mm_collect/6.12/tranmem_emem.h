#ifndef __TRANMEM_EMEM_H
#define __TRANMEM_EMEM_H

#define TRANMM "[TranMemoryInfo]"
#define TRAN_INTERVAL_SECOND 10

enum emem_log_type {
	DURATION = 0,
	EXCEPTION,
};

// Convert number of pages to KB or MB
// Assuming PAGE_SHIFT is always larger than or equal to 12
#define PAGE_KB(nr_pages)		(unsigned long)((nr_pages) << (PAGE_SHIFT - 10))
#define PAGE_MB(nr_pages)		(unsigned long)(PAGE_KB(nr_pages) / 1024)

void tran_memory_info_kernel_trace_start(char * name);
void tran_memory_info_kernel_trace_change(char * tag , int flag);
void tran_memory_info_kernel_trace_end(char * name);

int emem_init(struct proc_dir_entry *dir);
void emem_exit(void);

bool is_exception(void);
bool emem_trigger(int adj, const char *caller, enum emem_log_type type);

#define emem_trigger_default()	emem_trigger(1, __func__, EXCEPTION)
#define emem_trigger_adj(adj)	emem_trigger(adj, __func__, EXCEPTION)

#endif /* __TRANMEM_EMEM_H */
