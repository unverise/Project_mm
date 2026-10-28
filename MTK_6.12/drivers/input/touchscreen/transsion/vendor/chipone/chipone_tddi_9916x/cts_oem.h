#ifndef CTS_OEM_H
#define CTS_OEM_H

struct chipone_ts_data;

extern int cts_oem_init(struct chipone_ts_data *cts_data);
extern int cts_oem_deinit(struct chipone_ts_data *cts_data);
/*BSP: modify for midtest by jiangzhu.yang 20230719 start*/
extern int tran_get_SelftestResult(void);
extern int32_t selftest_proc_open(struct inode *inode, struct file *file);
/*BSP: modify for midtest by jiangzhu.yang 20230719 end*/

#endif /* CTS_VENDOR_H */

