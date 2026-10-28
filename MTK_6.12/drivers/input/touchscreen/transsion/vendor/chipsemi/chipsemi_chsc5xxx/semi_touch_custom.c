#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/delay.h>
//#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/version.h>
#include <linux/kstrtox.h>
#include "semi_touch_interface.h"
/*bsp:modify for transsion core by longtao.ran 20240321 start*/
#include "platform.h"
#include <linux/version.h>
#include "chipsemi_transsion.h"
/*bsp:modify for transsion core by longtao.ran 20240321 end*/
#if SEMI_TOUCH_FACTORY_TEST_EN
#include "semi_touch_test_5xxx.h"
#endif

#ifndef KEY_F26
#define KEY_F26 198
#endif
static DEFINE_MUTEX(tran_fingerprint_status_lock);

enum entry_type { chsc_version, chsc_tp_info, chsc_proximity, chsc_guesture, chsc_online_update, chsc_glove,
    chsc_suspend, chsc_orientation, chsc_esd_check, chsc_wet_finger, chsc_h_v_sw, chsc_selftest, chsc_charger,
    chsc_game_mode, chsc_cmd_change, chsc_palm_mode, chsc_game_op, chsc_high_sr, chsc_game_inhibit, chsc_free_cmd,
    chsc_pixel_locking, chsc_eaa_touch, chsc_region_ctrl, fingerprint_status, report_rate_switch, entry_max
    };

#if SEMI_TOUCH_MAKE_NODES_DIR == MAKE_NODE_UNDER_PROC
/******************************************************************************************************************************************/
/*make custom nodes under proc node*/
/******************************************************************************************************************************************/
static struct proc_dir_entry *custom_proc_entry[entry_max];

void semi_touch_create_nodes_dir(void)
{
    if (NULL == st_dev.chsc_nodes_dir)
    {
        st_dev.chsc_nodes_dir = proc_mkdir(SEMI_TOUCH_PROC_DIR, NULL);
    }
}

void semi_touch_release_nodes_dir(void)
{
    int index = 0;
    for (index = 0; index < entry_max; index++)
    {
        if (NULL != custom_proc_entry[index])
        {
            proc_remove(custom_proc_entry[index]);
        }
    }
    if (NULL != st_dev.chsc_nodes_dir)
    {
        proc_remove((struct proc_dir_entry *)st_dev.chsc_nodes_dir);
    }
}
/*bsp:modify for transsion core by longtao.ran 20240321 start*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int semi_touch_register_nodefun_imp(enum entry_type etype, char *node_name, const struct proc_ops *opt_addr)
#else
static int semi_touch_register_nodefun_imp(enum entry_type etype, char *node_name, const struct file_operations *opt_addr)
#endif
{
    struct proc_dir_entry *entry = proc_create(node_name, 0777, st_dev.chsc_nodes_dir, opt_addr);
    check_return_if_zero(entry, NULL);

    custom_proc_entry[etype] = entry;

    return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
#define semi_touch_register_nodefun(etype, fun_write, fun_read) \
{ \
    static const struct proc_ops ops_##etype = { \
    .proc_write = fun_write, \
    .proc_read  = fun_read, \
    }; \
    semi_touch_register_nodefun_imp(etype, #etype, &ops_##etype); \
}
#else
#define semi_touch_register_nodefun(etype, fun_write, fun_read) \
{ \
    static const struct file_operations ops_##etype = { \
    .owner = NULL, \
    .write = fun_write, \
    .read  = fun_read, \
    }; \
    semi_touch_register_nodefun_imp(etype, #etype, &ops_##etype); \
}
#endif
/*bsp:modify for transsion core by longtao.ran 20240321 end*/

/*USD:modify for transsion core by yun.luo 20241225 start*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int tran_touch_register_nodefun_imp(enum entry_type etype, char *node_name, const struct proc_ops *opt_addr)
#else
static int tran_touch_register_nodefun_imp(enum entry_type etype, char* node_name, const struct file_operations* opt_addr)
#endif
{
    struct proc_dir_entry* entry = proc_create(node_name, 0666, NULL, opt_addr);
    check_return_if_zero(entry, NULL);

    custom_proc_entry[etype] = entry;

    return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
#define tran_touch_register_nodefun(etype, fun_write, fun_read) \
{ \
    static const struct proc_ops ops_##etype = { \
    .proc_write = fun_write, \
    .proc_read  = fun_read, \
    }; \
    tran_touch_register_nodefun_imp(etype, #etype, &ops_##etype); \
}
#else
#define tran_touch_register_nodefun(etype, fun_write, fun_read) \
{ \
    static const struct file_operations ops_##etype = { \
    .owner = NULL, \
    .write = fun_write, \
    .read  = fun_read, \
    }; \
    tran_touch_register_nodefun_imp(etype, #etype, &ops_##etype); \
}
#endif
/*USD:modify for transsion core by yun.luo 20241225 end*/


#define kernel_buffer_prepare(copy, ker_buf, size) \
char ker_buf[size], *copy = ker_buf; \
do{memset(ker_buf, 0, sizeof(ker_buf)); if(*ppos) return 0;} while(0)

#define kernel_buffer_to_entry(ker_buf, size) \
*ppos = size; \
ret = copy_to_user(buff, ker_buf, size); \
check_return_if_fail(ret, NULL);

#define kernel_buffer_from_entry(ker_buf, size) \
ret = copy_from_user(ker_buf, buff, size); \
check_return_if_fail(ret, NULL);

#define chsc_version_node_write_declare() chsc_version_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_version_node_read_declare() chsc_version_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_tp_info_node_write_declare() chsc_tp_info_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_tp_info_node_read_declare() chsc_tp_info_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_proximity_node_write_declare() chsc_proximity_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_proximity_node_read_declare() chsc_proximity_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_guesture_node_write_declare() chsc_guesture_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_guesture_node_read_declare() chsc_guesture_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_glove_node_write_declare() chsc_glove_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_glove_node_read_declare() chsc_glove_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_suspend_node_write_declare() chsc_suspend_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_suspend_node_read_declare() chsc_suspend_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_online_update_node_write_declare() chsc_online_update_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_online_update_node_read_declare() chsc_online_update_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_orientation_node_write_declare() chsc_orientation_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_orientation_node_read_declare() chsc_orientation_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_esd_check_node_write_declare() chsc_esd_check_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_esd_check_node_read_declare() chsc_esd_check_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_wet_finger_node_write_declare() chsc_wet_finger_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_wet_finger_node_read_declare() chsc_wet_finger_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_h_v_sw_node_write_declare() chsc_h_v_sw_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_h_v_sw_node_read_declare() chsc_h_v_sw_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_tp_selftest_node_write_declare() chsc_tp_selftest_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_tp_selftest_node_read_declare() chsc_tp_selftest_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_charger_node_write_declare() chsc_charger_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_charger_node_read_declare() chsc_charger_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_game_mode_node_write_declare() chsc_game_mode_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_game_mode_node_read_declare() chsc_game_mode_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_cmd_change_node_write_declare() chsc_cmd_change_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_cmd_change_node_read_declare() chsc_cmd_change_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_palm_mode_node_write_declare() chsc_palm_mode_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_palm_mode_node_read_declare() chsc_palm_mode_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_game_op_mode_node_write_declare() chsc_game_op_mode_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_game_op_mode_node_read_declare() chsc_game_op_mode_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_high_sr_mode_node_write_declare() chsc_high_sr_mode_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_high_sr_mode_node_read_declare() chsc_high_sr_mode_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_game_touch_inhibit_node_write_declare() chsc_game_touch_inhibit_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_game_touch_inhibit_node_read_declare() chsc_game_touch_inhibit_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_free_cmd_node_write_declare() chsc_free_cmd_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_free_cmd_node_read_declare() chsc_free_cmd_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_pixel_locking_node_write_declare() chsc_pixel_locking_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_pixel_locking_node_read_declare() chsc_pixel_locking_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_eaa_touch_node_write_declare() chsc_eaa_touch_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_eaa_touch_node_read_declare() chsc_eaa_touch_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)
#define chsc_region_ctrl_node_write_declare() chsc_region_ctrl_node_write(struct file* fp, const char __user *buff, size_t len, loff_t* ppos)
#define chsc_region_ctrl_node_read_declare() chsc_region_ctrl_node_read(struct file* fp, char __user *buff, size_t len, loff_t* ppos)

#elif SEMI_TOUCH_MAKE_NODES_DIR == MAKE_NDDE_UNDER_SYS
/******************************************************************************************************************************************/
/*make custom nodes under sys file system*/
/******************************************************************************************************************************************/
void semi_touch_create_nodes_dir(void)
{
    if (NULL == st_dev.chsc_nodes_dir)
    {
        st_dev.chsc_nodes_dir = kobject_create_and_add(SEMI_TOUCH_PROC_DIR, NULL);
    }
}

void semi_touch_release_nodes_dir(void)
{
    if (NULL != st_dev.chsc_nodes_dir)
    {
        kobject_put((struct kobject *)st_dev.chsc_nodes_dir);
    }
}

#define semi_touch_register_nodefun(etype, fun_store, fun_show) \
{ \
    static struct kobj_attribute etype = __ATTR(etype, 0664, fun_show, fun_store); \
    ret = sysfs_create_file((struct kobject *)st_dev->chsc_nodes_dir, &etype.attr); \
    check_return_if_fail(ret, NULL); \
}

#define kernel_buffer_prepare(copy, ker_buf, size) \
char ker_buf[size], *copy = ker_buf; \
do{memset(ker_buf, 0, sizeof(ker_buf));} while(0)

#define kernel_buffer_to_entry(ker_buf, size) \
memcpy(buff, ker_buf, size); \
ret = 0

#define kernel_buffer_from_entry(ker_buf, size) \
memcpy(ker_buf, buff, size); \
ret = 0

#define chsc_version_node_write_declare() chsc_version_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_version_node_read_declare() chsc_version_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_tp_info_node_write_declare() chsc_tp_info_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_tp_info_node_read_declare() chsc_tp_info_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_proximity_node_write_declare() chsc_proximity_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_proximity_node_read_declare() chsc_proximity_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_guesture_node_write_declare() chsc_guesture_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_guesture_node_read_declare() chsc_guesture_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_glove_node_write_declare() chsc_glove_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_glove_node_read_declare() chsc_glove_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_suspend_node_write_declare() chsc_suspend_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_suspend_node_read_declare() chsc_suspend_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_online_update_node_write_declare() chsc_online_update_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_online_update_node_read_declare() chsc_online_update_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_orientation_node_write_declare() chsc_orientation_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_orientation_node_read_declare() chsc_orientation_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_esd_check_node_write_declare() chsc_esd_check_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_esd_check_node_read_declare() chsc_esd_check_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_wet_finger_node_write_declare() chsc_wet_finger_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_wet_finger_node_read_declare() chsc_wet_finger_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_h_v_sw_node_write_declare() chsc_h_v_sw_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_h_v_sw_node_read_declare() chsc_h_v_sw_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_tp_selftest_node_write_declare() chsc_tp_selftest_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_tp_selftest_node_read_declare() chsc_tp_selftest_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_charger_node_write_declare() chsc_charger_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_charger_node_read_declare() chsc_charger_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_game_mode_node_write_declare() chsc_game_mode_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_game_mode_node_read_declare() chsc_game_mode_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_cmd_change_node_write_declare() chsc_cmd_change_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_cmd_change_node_read_declare() chsc_cmd_change_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_palm_mode_node_write_declare() chsc_palm_mode_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_palm_mode_node_read_declare() chsc_palm_mode_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_game_op_mode_node_write_declare() chsc_game_op_mode_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_game_op_mode_node_read_declare() chsc_game_op_mode_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_high_sr_mode_node_write_declare() chsc_high_sr_mode_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_high_sr_mode_node_read_declare() chsc_high_sr_mode_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_game_touch_inhibit_node_write_declare() chsc_game_touch_inhibit_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_game_touch_inhibit_node_read_declare() chsc_game_touch_inhibit_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_free_cmd_node_write_declare() chsc_free_cmd_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_free_cmd_node_read_declare() chsc_free_cmd_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_pixel_locking_node_write_declare() chsc_pixel_locking_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_pixel_locking_node_read_declare() chsc_pixel_locking_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_eaa_touch_node_write_declare() chsc_eaa_touch_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_eaa_touch_node_read_declare() chsc_eaa_touch_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)
#define chsc_region_ctrl_node_write_declare() chsc_region_ctrl_node_write(struct kobject *dev, struct kobj_attribute *attr, const char *buff, size_t len)
#define chsc_region_ctrl_node_read_declare() chsc_region_ctrl_node_read(struct kobject* dev, struct kobj_attribute* attr, char* buff)

#endif //SEMI_TOUCH_MAKE_NODES_DIR == MAKE_NDDE_UNDER_SYS

const char* mapping_ic_from_type(unsigned char ictype)
{
    static const char *ic_name = "un-defined";

    switch (ictype)
    {
    case 0x31:
            ic_name = "CHCT5560";
        break;
    default:
        break;
    }

    return ic_name;
}

static ssize_t chsc_version_node_write_declare()
{
    return -EPERM;
}

static ssize_t chsc_version_node_read_declare()
{


    int ret ;
    int count = 0;

    unsigned char readBuffer[8] = {0};
    kernel_buffer_prepare(szCopy, szKernel, 400);



    kernel_log_d("count %d  maxsize %d\n",  count, 400);

    ret = semi_touch_read_bytes(0x20000000 + 0x80, readBuffer, 8);
    check_return_if_fail(ret, NULL);

    szCopy += sprintf(szCopy, "Ic type %s\n", mapping_ic_from_type(readBuffer[0]));

    szCopy += sprintf(szCopy, "config version is %02X\n", readBuffer[1]);
    szCopy += sprintf(szCopy, "vender id is %d, ", (readBuffer[5] << 8) + readBuffer[4]);
    szCopy += sprintf(szCopy, "product id is %d\n", (readBuffer[3] << 8) + readBuffer[2]);

    ret = semi_touch_read_bytes(0x20000000 + 0x10, readBuffer, 8);
    check_return_if_fail(ret, NULL);

    szCopy += sprintf(szCopy, "boot version is %04X\n", ((readBuffer[5] << 8) + readBuffer[4]));
    szCopy += sprintf(szCopy, "driver version is %s\n", CHSC_DRIVER_VERSION);

#if SEMI_TOUCH_THP_DRIVER_EN
    szCopy += semi_thp_get_thp_ver(szCopy);
    szCopy += sprintf(szCopy, "%s\n", semi_thp_dev.version);

    ret = szCopy - szKernel;
    szCopy += sprintf(szCopy, "count %d  maxsize %d\n",  ret, 400);


#endif
    count = szCopy - szKernel;

    kernel_log_d("count %d  maxsize %d\n",  count, 400);



    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_tp_info_node_write_declare()
{
    return -EPERM;
}

static ssize_t chsc_tp_info_node_read_declare()
{
    int ret, count;
    struct hal_device *client = st_dev.client;
    kernel_buffer_prepare(szCopy, szKernel, 400);

    szCopy += sprintf(szCopy, "Max finger number is %0d\n", SEMI_TOUCH_MAX_POINTS);
    szCopy += (int)sprintf(szCopy, "Int irq is %d\n", (int)client->irq);
    //szCopy += sprintf(szCopy, "I2c address is 0x%02X(0x%02X)\n", client->addr, (client->addr) << 1);
    szCopy += sprintf(szCopy, "Run status is 0x%08X\n", st_dev.stc.ctp_run_status);
    szCopy += sprintf(szCopy, "Fun enable is 0x%04X\n", st_dev.stc.custom_function_en);

#if SEMI_TOUCH_THP_DRIVER_EN
    szCopy += sprintf(szCopy, "thp_en is %d\n", semi_thp_dev.thp_en);
    szCopy += sprintf(szCopy, "cur_frame_len is %d\n", semi_thp_dev.cur_frame_len);
    szCopy += sprintf(szCopy, "frame_data_en is %d\n", semi_thp_dev.frame_data_en);
    szCopy += sprintf(szCopy, "read_frame_err is %d\n", semi_thp_dev.read_frame_err);
    szCopy += sprintf(szCopy, "thp_get_frame_en is %d\n", semi_thp_dev.thp_get_frame_en);
    szCopy += sprintf(szCopy, "spi_speed is %d\n",        semi_thp_dev.spi_speed);

    szCopy += sprintf(szCopy, "loss_frame_cnt is %d\n", semi_thp_dev.loss_frame_cnt);
    szCopy += sprintf(szCopy, "bottom_top_dif_max is %d\n", semi_thp_dev.bottom_top_dif_max);

    ret = szCopy - szKernel;
    szCopy += sprintf(szCopy, "count %d  maxsize %d\n",  ret, 400);
#endif

    count = szCopy - szKernel;

    kernel_log_d("count %d  maxsize %d\n",  count, 400);


    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_proximity_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('o' == szCopy[0])
    {
        open_proximity_function(st_dev.stc.custom_function_en);
    }
    else if ('c' == szCopy[0])
    {
        close_proximity_function(st_dev.stc.custom_function_en);
    }

    if (is_proximity_function_en(st_dev.stc.custom_function_en))
    {
        if ('0' == szCopy[0])
        {
            semi_touch_proximity_switch(0);
        }
        else if ('1' == szCopy[0])
        {
            semi_touch_proximity_switch(1);
        }
    }

    return len;
}

static ssize_t chsc_proximity_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "proximity switch is %d, status is %d.\n",
                      is_proximity_function_en(st_dev.stc.custom_function_en), is_proximity_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_guesture_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('o' == szCopy[0])
    {
        open_guesture_function(st_dev.stc.custom_function_en);
    }
    else if ('c' == szCopy[0])
    {
        close_guesture_function(st_dev.stc.custom_function_en);
    }

    if (is_guesture_function_en(st_dev.stc.custom_function_en))
    {
        if ('0' == szCopy[0])
        {
            semi_touch_guesture_switch(0, 0);
        }
        else if ('1' == szCopy[0])
        {
            semi_touch_guesture_switch(0xA5, 0);
        }

        else if ('2' == szCopy[0])
        {
            //G_GESTURE_MARK = 0x0;
            semi_touch_guesture_switch(0, 1);
        }
        else if ('3' == szCopy[0])
        {
            //G_GESTURE_MARK = 0xA5;
            semi_touch_guesture_switch(0xA5, 1);
        }
    }

    return len;
}

static ssize_t chsc_guesture_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "guesture switch is %d, status is %d.\n",
                      is_guesture_function_en(st_dev.stc.custom_function_en), is_guesture_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_glove_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('o' == szCopy[0])
    {
        open_glove_function(st_dev.stc.custom_function_en);
        semi_touch_glove_switch(1);
    }
    else if ('c' == szCopy[0])
    {
        semi_touch_glove_switch(0);
        close_glove_function(st_dev.stc.custom_function_en);
    }

    if (is_glove_function_en(st_dev.stc.custom_function_en))
    {
        if ('0' == szCopy[0])
        {
            semi_touch_glove_switch(0);
        }
        else if ('1' == szCopy[0])
        {
            semi_touch_glove_switch(1);
        }
    }

    return len;
}

static ssize_t chsc_glove_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "glove switch is %d, status is %d.\n",
                      is_glove_function_en(st_dev.stc.custom_function_en), is_glove_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_suspend_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('0' == szCopy[0])
    {
        semi_touch_resume_entry(&st_dev.client->dev);  //semi_touch_suspend_ctrl(0);
    }
    else if ('1' == szCopy[0])
    {
        semi_touch_suspend_entry(&st_dev.client->dev);//semi_touch_suspend_ctrl(1);
    }

    return len;
}

static ssize_t chsc_suspend_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "suspend switch is %d, status is %d.\n",
                      1, is_suspend_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_orientation_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('0' == szCopy[0])
    {
        semi_touch_orientation_switch(0);
    }
    else if ('1' == szCopy[0])
    {
        semi_touch_orientation_switch(1);
    }

    return len;
}

static ssize_t chsc_orientation_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "orientation status is %s.\n", is_orientation_activate(st_dev.stc.ctp_run_status) ? "horizontal" : "vertical");
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_esd_check_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "esd check switch is %d.\n", is_esd_function_en(st_dev.stc.custom_function_en));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_esd_check_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('o' == szCopy[0])
    {
        open_esd_function(st_dev.stc.custom_function_en);
    }
    else if ('c' == szCopy[0])
    {
        close_esd_function(st_dev.stc.custom_function_en);
    }

    return len;
}

static ssize_t chsc_online_update_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 128);
    kernel_buffer_from_entry(szKernel, (len > 128) ? 128 : len);

    if ('1' == szCopy[0])
    {
        sprintf(szCopy, "%s", CHSC_AUTO_UPDATE_PACKET_BIN);
    }
    else if (len > 1)
    {
        szCopy[len - 1] = 0;
    }

    ret = semi_touch_online_update_check((char *)szCopy);

    return len;
}

static ssize_t chsc_online_update_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "online update is %s\n", SEMI_TOUCH_ONLINE_UPDATE_EN ? "enabled" : "disabled");

    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count ;
}

static ssize_t chsc_wet_finger_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('o' == szCopy[0])
    {
        open_wet_finger_function(st_dev.stc.custom_function_en);
        semi_touch_wet_finger_switch(1);
    }
    else if ('c' == szCopy[0])
    {
        semi_touch_wet_finger_switch(0);
        close_wet_finger_function(st_dev.stc.custom_function_en);
    }

    if (is_wet_finger_function_en(st_dev.stc.custom_function_en))
    {
        if ('0' == szCopy[0])
        {
            semi_touch_wet_finger_switch(0);
        }
        else if ('1' == szCopy[0])
        {
            semi_touch_wet_finger_switch(1);
        }
    }

    return len;
}

static ssize_t chsc_wet_finger_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "wet finger switch is %d, status is %d.\n",
                      is_wet_finger_function_en(st_dev.stc.custom_function_en), is_wet_finger_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_h_v_sw_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('u' == szCopy[0])   //open up direction
    {
        open_h_v_sw_function(st_dev.stc.custom_function_en);
        semi_touch_h_v_switch(0x13, 1);
    }
    else if ('w' == szCopy[0])     //close up direction
    {
        semi_touch_h_v_switch(0x13, 0);
        close_h_v_sw_function(st_dev.stc.custom_function_en);
    }
    else if ('d' == szCopy[0])
    {
        open_h_v_sw_function(st_dev.stc.custom_function_en);
        semi_touch_h_v_switch(0x14, 1);
    }
    else if ('s' == szCopy[0])
    {
        semi_touch_h_v_switch(0x14, 0);
        close_h_v_sw_function(st_dev.stc.custom_function_en);
    }
    else if ('l' == szCopy[0])
    {
        open_h_v_sw_function(st_dev.stc.custom_function_en);
        semi_touch_h_v_switch(0x15, 1);
    }
    else if ('a' == szCopy[0])
    {
        semi_touch_h_v_switch(0x15, 0);
        close_h_v_sw_function(st_dev.stc.custom_function_en);
    }
    else if ('r' == szCopy[0])
    {
        open_h_v_sw_function(st_dev.stc.custom_function_en);
        semi_touch_h_v_switch(0x16, 1);
    }
    else if ('f' == szCopy[0])
    {
        semi_touch_h_v_switch(0x16, 0);
        close_h_v_sw_function(st_dev.stc.custom_function_en);
    }

    if (is_h_v_sw_function_en(st_dev.stc.custom_function_en))
    {
        if ('0' == szCopy[0])
        {
            semi_touch_h_v_switch(0x13, 0);
        }
        else if ('1' == szCopy[0])
        {
            semi_touch_h_v_switch(0x13, 1);
        }
        else if ('2' == szCopy[0])
        {
            semi_touch_h_v_switch(0x14, 0);
        }
        else if ('3' == szCopy[0])
        {
            semi_touch_h_v_switch(0x14, 1);
        }
        else if ('4' == szCopy[0])
        {
            semi_touch_h_v_switch(0x15, 0);
        }
        else if ('5' == szCopy[0])
        {
            semi_touch_h_v_switch(0x15, 1);
        }
        else if ('6' == szCopy[0])
        {
            semi_touch_h_v_switch(0x16, 0);
        }
        else if ('7' == szCopy[0])
        {
            semi_touch_h_v_switch(0x16, 1);
        }
    }

    return len;
}

static ssize_t chsc_h_v_sw_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "h v switch is %d, status is %d.\n",
                      is_h_v_sw_function_en(st_dev.stc.custom_function_en), is_h_v_sw_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_tp_selftest_node_write_declare()
{
    return -EPERM;
}

static ssize_t chsc_tp_selftest_node_read_declare()
{
#if SEMI_TOUCH_FACTORY_TEST_EN
    int ret, count;
    int read_len = 0;
    char *ker_buf = NULL;
    char *szCopy = NULL;
    char *rzCopy = NULL;

    ker_buf = kmalloc(24576, GFP_KERNEL);
    memset(ker_buf, 0, 24576);
    szCopy = ker_buf;
    rzCopy = ker_buf;
    if (*ppos) return 0;

    ret = semi_touch_start_factory_test(szCopy, &read_len);
    rzCopy += read_len;
    count = sprintf(rzCopy, "TestResult is %s\n", (0 != ret) ? "Failed" : "Pass");
    count += read_len;

    *ppos = count;
    ret = copy_to_user(buff, ker_buf, count);
    kfree(ker_buf);
    check_return_if_fail(ret, NULL);

    return count;
#else
    return -EPERM;
#endif
}

static ssize_t chsc_charger_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('o' == szCopy[0])
    {
        open_charger_function(st_dev.stc.custom_function_en);
        semi_touch_charger_switch(1);
    }
    else if ('c' == szCopy[0])
    {
        semi_touch_charger_switch(0);
        close_charger_function(st_dev.stc.custom_function_en);
    }

    if (is_charger_function_en(st_dev.stc.custom_function_en))
    {
        if ('0' == szCopy[0])
        {
            semi_touch_charger_switch(0);
        }
        else if ('1' == szCopy[0])
        {
            semi_touch_charger_switch(1);
        }
    }

    return len;
}

static ssize_t chsc_charger_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "charger switch is %d, status is %d.\n",
                      is_charger_function_en(st_dev.stc.custom_function_en), is_charger_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_game_mode_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('o' == szCopy[0])
    {
        open_game_mode_function(st_dev.stc.custom_function_en);
        semi_touch_game_mode_switch(1);
    }
    else if ('c' == szCopy[0])
    {
        semi_touch_game_mode_switch(0);
        close_game_mode_function(st_dev.stc.custom_function_en);
    }

    if (is_game_mode_function_en(st_dev.stc.custom_function_en))
    {
        if ('0' == szCopy[0])
        {
            semi_touch_game_mode_switch(0);
        }
        else if ('1' == szCopy[0])
        {
            semi_touch_game_mode_switch(1);
        }
    }

    return len;
}

static ssize_t chsc_game_mode_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "game mode switch is %d, status is %d.\n",
                      is_game_mode_function_en(st_dev.stc.custom_function_en), is_game_mode_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_game_op_mode_node_write_declare()
{
    int ret;
    unsigned int mark;
    kernel_buffer_prepare(szCopy, szKernel, 16);
    kernel_buffer_from_entry(szKernel, (len > 16) ? 16 : len);

    if ('o' == szCopy[0])
    {
        open_game_op_mode_function(st_dev.stc.custom_function_en);
    }
    else if ('c' == szCopy[0])
    {
        semi_touch_game_op_mode_switch(G_GAME_BACK, 0);
        close_game_op_mode_function(st_dev.stc.custom_function_en);
    }
    kernel_log_d("len is %ld\n", len);


    if (is_game_op_mode_function_en(st_dev.stc.custom_function_en) && len >= 4)
    {
        ret = kstrtouint(szCopy, 0, &mark);
        if (ret == 0) {
            kernel_log_d("Conversion successful. Result: 0x%x\n", mark);
        } else {
            kernel_log_e("Conversion failed. Error code: %d\n", ret);
            return ret;
        }
        if (0 == ((mark>>24) & 0xFF))
        {
            semi_touch_game_op_mode_switch(mark, 0);//0x00040404-close
        }
        else if (1 == ((mark>>24) & 0xFF))//0x01040404-open
        {
            semi_touch_game_op_mode_switch(mark, 1);
        }
    }

    return len;
}

static ssize_t chsc_game_op_mode_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "game op mode switch is %d, status is %d.\n",
                      is_game_op_mode_function_en(st_dev.stc.custom_function_en), is_game_op_mode_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_palm_mode_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('o' == szCopy[0])
    {
        open_palm_mode_function(st_dev.stc.custom_function_en);
        semi_touch_palm_mode_switch(1);
    }
    else if ('c' == szCopy[0])
    {
        semi_touch_palm_mode_switch(0);
        close_palm_mode_function(st_dev.stc.custom_function_en);
    }

    if (is_palm_mode_function_en(st_dev.stc.custom_function_en))
    {
        if ('0' == szCopy[0])
        {
            semi_touch_palm_mode_switch(0);
        }
        else if ('1' == szCopy[0])
        {
            semi_touch_palm_mode_switch(1);
        }
    }

    return len;
}

static ssize_t chsc_palm_mode_node_read_declare()
{
    int ret, count;
    unsigned int regdata;
    kernel_buffer_prepare(szCopy, szKernel, 256);

    semi_touch_read_bytes(0x200008D0, (unsigned char *)&regdata, 4);//read palm status from register,1-open,0-close

    szCopy += sprintf(szCopy, "palm mode switch is %d, status is %d, read switch status is %d, read palm contacting status is %d.\n",
                      is_palm_mode_function_en(st_dev.stc.custom_function_en),
                      is_palm_mode_activate(st_dev.stc.ctp_run_status), regdata & 0xff, (regdata & 0xff00) >> 8);
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_high_sr_mode_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('o' == szCopy[0])
    {
        open_high_sr_mode_function(st_dev.stc.custom_function_en);
        semi_touch_high_sr_mode_switch(1);
    }
    else if ('c' == szCopy[0])
    {
        semi_touch_high_sr_mode_switch(0);
        close_high_sr_mode_function(st_dev.stc.custom_function_en);
    }

    if (is_high_sr_mode_function_en(st_dev.stc.custom_function_en))
    {
        if ('0' == szCopy[0])
        {
            semi_touch_high_sr_mode_switch(0);
        }
        else if ('1' == szCopy[0])
        {
            semi_touch_high_sr_mode_switch(1);
        }
    }

    return len;
}

static ssize_t chsc_high_sr_mode_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "high sr mode switch is %d, status is %d.\n",
                      is_high_sr_mode_function_en(st_dev.stc.custom_function_en), is_high_sr_mode_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_game_touch_inhibit_node_write_declare()
{
    int ret, i;
    char *ptr = NULL;
    unsigned char tmpbuf[80] = {0};
    unsigned char high, low;
    kernel_buffer_prepare(szCopy, szKernel, 256);
    kernel_buffer_from_entry(szKernel, (len > 256) ? 256 : len);
    ptr = &szCopy[1];

    kernel_log_d("len =%ld.\n", len);//or %d

    //Convert characters into data
    for (i = 0; i < 80; i++) 
    {
        high = ptr[i * 2];
        low = ptr[i * 2 + 1];
        if (high >= '0' && high <= '9')
        {
            high -= '0';
        }
        else if (high >= 'A' && high <= 'F')
        {
            high -= 'A' - 10;
        }
        else if (high >= 'a' && high <= 'f')
        {
            high -= 'a' - 10;
        }
        if (low >= '0' && low <= '9')
        {
            low -= '0';
        }
        else if (low >= 'A' && low <= 'F')
        {
            low -= 'A' - 10;
        }
        else if (low >= 'a' && low <= 'f')
        {
            low -= 'a' - 10;
        }

        tmpbuf[i] = (high << 4) | low;
    }

#if 0 //for debug
    for (i = 0; i < 40; i++)
    {
        kernel_log_d("Character converted to hex: 0x%02x\n", tmpbuf[i]);
    }
#endif

    memcpy((unsigned char *)&st_dev.tran_inhibit_data, tmpbuf, 40);

    if ('o' == szCopy[0])
    {
        open_game_touch_inhibit_function(st_dev.stc.custom_function_en);
    }
    else if ('c' == szCopy[0])
    {
        semi_touch_game_inhibit_switch(0);
        close_game_touch_inhibit_function(st_dev.stc.custom_function_en);
    }

    if (is_game_touch_inhibit_function_en(st_dev.stc.custom_function_en))
    {
        if ('0' == szCopy[0])
        {
            semi_touch_game_inhibit_switch(0);
        }
        else if ('1' == szCopy[0])
        {
            semi_touch_game_inhibit_switch(1);
        }
    }

    return len;
}

static ssize_t chsc_game_touch_inhibit_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "game_touch_inhibit switch is %d, status is %d.\n",
                      is_game_touch_inhibit_function_en(st_dev.stc.custom_function_en), is_game_touch_inhibit_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_free_cmd_node_write_declare()
{
    int ret;
    unsigned int mark;
    kernel_buffer_prepare(szCopy, szKernel, 16);
    kernel_buffer_from_entry(szKernel, (len > 16) ? 16 : len);

    kernel_log_d("len is %ld\n", len);

    ret = kstrtouint(szCopy, 0, &mark);
    if (ret == 0) {
        kernel_log_d("Conversion successful. Result: 0x%x\n", mark);
    } else {
        kernel_log_e("Conversion failed. Error code: %d\n", ret);
        return ret;
    }

    semi_touch_free_cmd_switch(mark);

    return len;
}

static ssize_t chsc_free_cmd_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "free cmd switch is open, status is ok!.\n");
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_pixel_locking_node_write_declare()
{
   return -EPERM;
}

static ssize_t chsc_pixel_locking_node_read_declare()
{
    int ret, count;
    unsigned int regdata;
    kernel_buffer_prepare(szCopy, szKernel, 256);

    semi_touch_read_bytes(0x200008C0, (unsigned char *)&regdata, 4);

    szCopy += sprintf(szCopy, "The reading pixel lock range is %d.\n", regdata & 0xff);
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;

}

static ssize_t chsc_eaa_touch_node_write_declare()
{
    int ret;
    unsigned int mark;
    kernel_buffer_prepare(szCopy, szKernel, 16);
    kernel_buffer_from_entry(szKernel, (len > 16) ? 16 : len);

    if ('o' == szCopy[0])
    {
        open_eaa_touch_function(st_dev.stc.custom_function_en);
    }
    else if ('c' == szCopy[0])
    {
        semi_touch_eaa_touch_switch(G_EAAT_BACK, 0);
        close_eaa_touch_function(st_dev.stc.custom_function_en);
    }
    kernel_log_d("len is %ld\n", len);


    if (is_eaa_touch_function_en(st_dev.stc.custom_function_en) && len >= 4)
    {
        ret = kstrtouint(szCopy, 0, &mark);
        if (ret == 0) {
            kernel_log_d("Conversion successful. Result: 0x%x\n", mark);
        } else {
            kernel_log_e("Conversion failed. Error code: %d\n", ret);
            return ret;
        }
        if (0 == ((mark>>8) & 0xFF))
        {
            semi_touch_eaa_touch_switch(mark, 0);
        }
        else if (1 == ((mark>>8) & 0xFF))
        {
            semi_touch_eaa_touch_switch(mark, 1);
        }
    }

    return len;
}

static ssize_t chsc_eaa_touch_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "eaa touch switch is %d, status is %d.\n",
                      is_eaa_touch_function_en(st_dev.stc.custom_function_en), is_eaa_touch_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_region_ctrl_node_write_declare()
{
    int ret, i;
    char *ptr = NULL;
    unsigned char tmpbuf[REGION_BUFF_MAX * 2] = {0};
    unsigned char high, low;
    kernel_buffer_prepare(szCopy, szKernel, 512);
    kernel_buffer_from_entry(szKernel, (len > 512) ? 512 : len);
    ptr = &szCopy[1];

    kernel_log_d("len =%ld.\n", len);//or %d

    //Convert characters into data
    for (i = 0; i < (REGION_BUFF_MAX * 2); i++) 
    {
        high = ptr[i * 2];
        low = ptr[i * 2 + 1];
        if (high >= '0' && high <= '9')
        {
            high -= '0';
        }
        else if (high >= 'A' && high <= 'F')
        {
            high -= 'A' - 10;
        }
        else if (high >= 'a' && high <= 'f')
        {
            high -= 'a' - 10;
        }
        if (low >= '0' && low <= '9')
        {
            low -= '0';
        }
        else if (low >= 'A' && low <= 'F')
        {
            low -= 'A' - 10;
        }
        else if (low >= 'a' && low <= 'f')
        {
            low -= 'a' - 10;
        }

        tmpbuf[i] = (high << 4) | low;
    }

#if 0 //for debug
    for (i = 0; i < REGION_BUFF_MAX; i++)
    {
        kernel_log_d("Character converted to hex: 0x%02x\n", tmpbuf[i]);
    }
#endif

    memcpy((unsigned char *)&st_dev.region_ctrl_data, tmpbuf, REGION_BUFF_MAX);

    if ('o' == szCopy[0])
    {
        open_region_ctrl_function(st_dev.stc.custom_function_en);
    }
    else if ('c' == szCopy[0])
    {
        semi_touch_region_ctrl_switch(0);
        close_region_ctrl_function(st_dev.stc.custom_function_en);
    }

    if (is_region_ctrl_function_en(st_dev.stc.custom_function_en))
    {
        if ('0' == szCopy[0])
        {
            semi_touch_region_ctrl_switch(0);
        }
        else if ('1' == szCopy[0])
        {
            semi_touch_region_ctrl_switch(1);
        }
    }

    return len;
}

static ssize_t chsc_region_ctrl_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "region ctrl switch is %d, status is %d.\n",
                      is_region_ctrl_function_en(st_dev.stc.custom_function_en), is_region_ctrl_activate(st_dev.stc.ctp_run_status));
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

static ssize_t chsc_cmd_change_node_write_declare()
{
    int ret;
    kernel_buffer_prepare(szCopy, szKernel, 8);
    kernel_buffer_from_entry(szKernel, (len > 8) ? 8 : len);

    if ('a' == szCopy[0])
    {
        semi_touch_send_ioctrl(1);
    }
    else if ('b' == szCopy[0])
    {
        semi_touch_send_ioctrl(2);
    }
    else if ('c' == szCopy[0])
    {
        semi_touch_send_ioctrl(3);
    }
    else if ('d' == szCopy[0])
    {
        semi_touch_send_ioctrl(4);
    }

    return len;
}

static ssize_t chsc_cmd_change_node_read_declare()
{
    int ret, count;
    kernel_buffer_prepare(szCopy, szKernel, 128);

    szCopy += sprintf(szCopy, "cmd change switch is open, status is ok!.\n");
    count = szCopy - szKernel;

    kernel_buffer_to_entry(szKernel, count);

    return count;
}

/*USD:modify for transsion core by yun.luo 20241225 start*/
static ssize_t fingerprint_status_node_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;

    if(!st_dev.fod_func_support){
        return count;
    }

    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    ptr = page;
    ptr += sprintf(page, "%d\n", st_dev.tran_fingerprint_status);
    len = ptr -page;
    if(*f_pos >= len){
        kfree(page);
        return 0;
    }
    if(copy_to_user(buf,(char *)page,len))
    {
        kfree(page);
        return len;
    }
    *f_pos += len;
    kfree(page);
    return len;

}

static ssize_t fingerprint_status_node_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    char buff[2]={'0','\0'};
    int ret = -1;

    if(!st_dev.fod_func_support){
        return count;
    }

    ret = copy_from_user(buff, buf, 1);
    if (ret) {
        kernel_log_e("copy_from_user failed\n");
        return -EPERM;
    }

    mutex_lock(&tran_fingerprint_status_lock);
    /* 2 means enroll
     * 1 means unlock
     * 0 means close
     */
    if (buff[0]=='2') {
        st_dev.tran_fingerprint_status = 2;
        G_GESTURE_MARK = 0;
    } else if (buff[0]=='1') {
        st_dev.tran_fingerprint_status = 1;
        G_GESTURE_MARK = 0;
    } else if (buff[0]=='0') {
        st_dev.tran_fingerprint_status = 0;
        G_GESTURE_MARK = 0xA5;
    } else {
        kernel_log_e("fingerprint status invalid value:%c \n",buff[0]);
    }
    kernel_log_d("tran_fingerprint_status = %d \n", st_dev.tran_fingerprint_status);
    mutex_unlock(&tran_fingerprint_status_lock);

    return count;
}
/*USD:modify for transsion core by yun.luo 20241225 end*/

//USD:modify for transsion core by yun.luo 20241225 start
static ssize_t report_rate_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;

    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    ptr = page;
    ptr += sprintf(page, "%d\n", st_dev.report_rate_switch);
    len = ptr -page;
    if(*f_pos >= len){
        kfree(page);
        return 0;
    }
    if(copy_to_user(buf,(char *)page,len))
    {
        kfree(page);
        return len;
    }
    *f_pos += len;
    kfree(page);
    return len;
}
static ssize_t report_rate_switch_proc_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int ret;
    /* BSP:modify husan carsh by jiafeng.fan 20250904 start */
    int tmp;
    /* BSP:modify husan carsh by jiafeng.fan 20250904 end */
    char buff[2]={'0','\0'};
    if (count == 0 || count > 2) {
        kernel_log_e("Invalid value! count = %zu \n", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff,buf,1);
    if(ret){
        kernel_log_e("copy_from_user fail.\n");
        return -EPERM;
    }
    kernel_log_e("transsion report_rate_switch buff: %c\n",buff[0]);
    /* BSP:modify husan carsh by jiafeng.fan 20250904 start */
    ret = sscanf(buff, "%d", &tmp);
    /* BSP:modify husan carsh by jiafeng.fan 20250904 end */
    if (ret != 1) {
        kernel_log_e("Invalid value! ret = %d\n", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp > 1) {
        kernel_log_e("Invalid value! tmp = %d\n", tmp);
        ret = -EINVAL;
        goto out;
    }
    st_dev.report_rate_switch = tmp;
    kernel_log_d("transsion report_rate_switch %d\n",st_dev.report_rate_switch);
    if(is_suspend_activate(st_dev.stc.ctp_run_status)){
        kernel_log_d("In suspend status,goto out.\n");
        ret = count;
        goto out;
    }
    if(st_dev.switch_report_support){
        semi_touch_game_mode_switch(st_dev.report_rate_switch);
    }

    ret = count;
out:
    return ret;
}
//USD:modify for transsion core by yun.luo 20241225 end


/********************************************************************************************************************************/
/*glove function*/
int semi_touch_glove_prepare(void)
{
#if SEMI_TOUCH_GLOVE_OPEN
    open_glove_function(st_dev.stc.custom_function_en);
#endif
    return 0;
}

/********************************************************************************************************************************/
int semi_touch_wet_finger_prepare(void)
{
#if SEMI_TOUCH_WET_FINGER_OPEN
    open_wet_finger_function(st_dev.stc.custom_function_en);
#endif
    return 0;
}

/********************************************************************************************************************************/
int semi_touch_h_v_sw_prepare(void)
{
#if SEMI_TOUCH_H_V_SW
    open_h_v_sw_function(st_dev.stc.custom_function_en);
#endif
    return 0;
}

/********************************************************************************************************************************/
int semi_touch_charger_prepare(void)
{
#if SEMI_TOUCH_CHARGER_EN
    open_charger_function(st_dev.stc.custom_function_en);
#endif
    return 0;
}

/********************************************************************************************************************************/
int semi_touch_game_mode_prepare(void)
{
#if SEMI_TOUCH_GAME_MODE_EN
    open_game_mode_function(st_dev.stc.custom_function_en);
#endif
    return 0;
}

/********************************************************************************************************************************/
int semi_touch_game_op_mode_prepare(void)
{
#if SEMI_TOUCH_GAME_OP_MODE_EN
    open_game_op_mode_function(st_dev.stc.custom_function_en);
#endif
    return 0;
}

/********************************************************************************************************************************/
int semi_touch_high_sr_mode_prepare(void)
{
#if SEMI_TOUCH_HIGH_SR_MODE_EN
    open_high_sr_mode_function(st_dev.stc.custom_function_en);
#endif
    return 0;
}

/********************************************************************************************************************************/
int semi_touch_game_touch_inhibit_prepare(void)
{
#if SEMI_TOUCH_GAME_INHIBIT_EN
    open_game_touch_inhibit_function(st_dev.stc.custom_function_en);
#endif
    return 0;
}

/********************************************************************************************************************************/
int semi_touch_palm_mode_prepare(void)
{
#if SEMI_TOUCH_PALM_MODE_EN
    open_palm_mode_function(st_dev.stc.custom_function_en);
#endif
    return 0;
}

/********************************************************************************************************************************/
int semi_touch_eaa_touch_prepare(void)
{
#if SEMI_TOUCH_EAAT_MODE_EN
    open_eaa_touch_function(st_dev.stc.custom_function_en);
#endif
    return 0;
}

/********************************************************************************************************************************/
int semi_touch_region_ctrl_prepare(void)
{
#if SEMI_TOUCH_REGION_CTRL_EN
    open_region_ctrl_function(st_dev.stc.custom_function_en);
#endif
    return 0;
}
/********************************************************************************************************************************/

/*guesture support*/
#if SEMI_TOUCH_GESTURE_OPEN
//#include <linux/wakelock.h>
/*bsp:modify for transsion core by longtao.ran 20240321 start*/
#if 0
#define GESTURE_LEFT                         0x20
#define GESTURE_RIGHT                        0x21
#define GESTURE_UP                           0x22
#define GESTURE_DOWN                         0x23
#define GESTURE_DOUBLECLICK                  0x24
#define GESTURE_SINGLECLICK                  0x25
#define GESTURE_O                            0x30
#define GESTURE_W                            0x31
#define GESTURE_M                            0x32
#define GESTURE_E                            0x33
#define GESTURE_C                            0x34
#define GESTURE_S                            0x46
#define GESTURE_V                            0x54
#define GESTURE_Z                            0x65
#define GESTURE_L                            0x44
#define GESTURE_FPDWN                        0x56
#define GESTURE_FPUP                         0x57
#define GESTURE_FPCONTIUE                    0x58
#endif
//static struct wake_lock gesture_timeout_wakelock;
int semi_touch_gesture_prepare(void)
{
    //open_guesture_function(st_dev.stc.custom_function_en);
    //wake_lock_init(&gesture_timeout_wakelock, WAKE_LOCK_SUSPEND, "gesture_timeout_wakelock");

    input_set_capability(st_dev.input, EV_KEY, KEY_POWER);
    input_set_capability(st_dev.input, EV_KEY, KEY_U);
    input_set_capability(st_dev.input, EV_KEY, KEY_LEFT);
    input_set_capability(st_dev.input, EV_KEY, KEY_RIGHT);
    input_set_capability(st_dev.input, EV_KEY, KEY_UP);
    input_set_capability(st_dev.input, EV_KEY, KEY_DOWN);
    input_set_capability(st_dev.input, EV_KEY, KEY_D);
    input_set_capability(st_dev.input, EV_KEY, KEY_O);
    input_set_capability(st_dev.input, EV_KEY, KEY_W);
    input_set_capability(st_dev.input, EV_KEY, KEY_M);
    input_set_capability(st_dev.input, EV_KEY, KEY_E);
    input_set_capability(st_dev.input, EV_KEY, KEY_C);
    input_set_capability(st_dev.input, EV_KEY, KEY_S);
    input_set_capability(st_dev.input, EV_KEY, KEY_V);
    input_set_capability(st_dev.input, EV_KEY, KEY_Z);

    __set_bit(KEY_POWER, st_dev.input->keybit);
    __set_bit(KEY_U,     st_dev.input->keybit);
    __set_bit(KEY_LEFT,  st_dev.input->keybit);
    __set_bit(KEY_RIGHT, st_dev.input->keybit);
    __set_bit(KEY_UP,    st_dev.input->keybit);
    __set_bit(KEY_DOWN,  st_dev.input->keybit);
    __set_bit(KEY_D,     st_dev.input->keybit);
    __set_bit(KEY_O,     st_dev.input->keybit);
    __set_bit(KEY_W,     st_dev.input->keybit);
    __set_bit(KEY_M,     st_dev.input->keybit);
    __set_bit(KEY_E,     st_dev.input->keybit);
    __set_bit(KEY_C,     st_dev.input->keybit);
    __set_bit(KEY_S,     st_dev.input->keybit);
    __set_bit(KEY_V,     st_dev.input->keybit);
    __set_bit(KEY_Z,     st_dev.input->keybit);
    /*bsp: add for bringup by guoxin.zhu 20230608 start*/
    input_set_capability(st_dev.input, EV_KEY, KEY_F26);
    input_set_capability(st_dev.input, EV_KEY, KEY_F20);
    input_set_capability(st_dev.input, EV_KEY, KEY_FINGER);
    input_set_capability(st_dev.input, EV_KEY, KEY_LARGE_AREA);//for pocket mode by yun.luo
    /*bsp: add for bringup by guoxin.zhu 20230608 end*/

    return 0;
}
int semi_touch_gesture_stop(void)
{
    //if(is_guesture_function_en(st_dev.stc.custom_function_en))
    //{
    //    wake_lock_destroy(&gesture_timeout_wakelock);
    //}

    return 0;
}
int semi_touch_wake_lock(void)
{
    // if(is_guesture_activate(st_dev.stc.ctp_run_status))
    // {
    //     //irq_set_irq_type(st_dev.client->irq, IRQF_TRIGGER_FALLING);
    //     //wake_lock_timeout(&gesture_timeout_wakelock, msecs_to_jiffies(2500));
    //     kernel_log_d("int interrupts, guesture on\n");
    // }
    return SEMI_DRV_ERR_OK;
}
bool semi_touch_gesture_report(unsigned char gesture_id)
{
    __attribute__((unused)) int keycode = 0;
    int ret = 0;
    struct tran_core_ops *ops = NULL;

    //wake_lock_timeout(&gesture_timeout_wakelock, msecs_to_jiffies(2000));

    switch (gesture_id)
    {
        case GESTURE_LEFT:
            keycode = KEY_LEFT;
            break;
        case GESTURE_RIGHT:
            keycode = KEY_RIGHT;
            break;
        case GESTURE_UP:
            keycode = KEY_UP;
            break;
        case GESTURE_DOWN:
            keycode = KEY_DOWN;
            break;
        case GESTURE_SINGLECLICK:
            keycode = KEY_POWER;//KEY_U;
            break;
        case GESTURE_DOUBLECLICK:
            keycode = KEY_POWER;//KEY_U;
            break;
        case GESTURE_O:
            keycode = KEY_O;
            break;
        case GESTURE_W:
            keycode = KEY_W;
            break;
        case GESTURE_M:
            keycode = KEY_M;
            break;
        case GESTURE_E:
            keycode = KEY_E;
            break;
        case GESTURE_C:
            keycode = KEY_C;
            break;
        case GESTURE_S:
            keycode = KEY_S;
            break;
        case GESTURE_V:
            keycode = KEY_V;
            break;
        case GESTURE_Z:
            keycode = KEY_UP;
            break;
        case GESTURE_L:
            keycode = KEY_L;
            break;
        case GESTURE_FPDWN:
            keycode = FP_KEY_VALUE;
            break;
        case GESTURE_FPUP:
            keycode = FP_KEY_VALUE;
            break;
        case GESTURE_FPCONTIUE:
            keycode = FP_KEY_VALUE;
            break;
        default:
            break;
    }

    /*BSP: add transsion core by guoxin.zhu 20230613 start*/
    if(get_tpd_ops(&chipsemi_controller, &ops)) {
        ret = ops->gesture_id_report(gesture_id, &chipsemi_controller);
        if (ret < 0) {
            kernel_log_d("[GESTURE]gesture(0x%x) touch data report fail:%d", gesture_id, ret);
            return ret;
       }
   }
    /*BSP: add transsion core by guoxin.zhu 20220613 end*/

#if 0
    if(keycode)
    {
        if (keycode != FP_KEY_VALUE)
        {
            input_report_key(st_dev.input, keycode, 1);
            input_sync(st_dev.input);
            input_report_key(st_dev.input, keycode, 0);
            input_sync(st_dev.input);
        }
        else
        {
            if (gesture_id == GESTURE_FPUP)
            {
                input_report_key(st_dev.input, keycode, 0);
                input_sync(st_dev.input);
            }
            else
            {
                input_report_key(st_dev.input, keycode, 1);
                input_sync(st_dev.input);
            }
        }
    }
#endif
    return true;
}

#else //SEMI_TOUCH_GESTURE_OPEN
int semi_touch_gesture_prepare(void)
{
    return 0;
}
int semi_touch_gesture_stop(void)
{
    return 0;
}
int semi_touch_wake_lock(void)
{
    return 0;
}
bool semi_touch_gesture_report(unsigned char gesture_id)
{
    return 0;
}
#endif //SEMI_TOUCH_GESTURE_OPEN

/********************************************************************************************************************************/
/*esd support*/
#if SEMI_TOUCH_ESD_CHECK_OPEN
static void semi_touch_esd_work_fun(struct work_struct *work);

int semi_touch_esd_check_prepare(void)
{
    open_esd_function(st_dev.stc.custom_function_en);
    semi_touch_queue_asyn_work(work_queue_custom_work, semi_touch_esd_work_fun, 4000);

    return 0;
}

static void semi_touch_esd_work_fun(struct work_struct *work)
{
    semi_touch_heart_beat();

    semi_touch_queue_asyn_work(work_queue_custom_work, semi_touch_esd_work_fun, 4000);
}

#else //SEMI_TOUCH_ESD_CHECK_OPEN
int semi_touch_esd_check_prepare(void)
{
    return 0;
}
#endif //SEMI_TOUCH_ESD_CHECK_OPEN


/********************************************************************************************************************************/
int semi_touch_custom_work(struct sm_touch_dev *st_dev)
{
    int ret = 0;
    //unsigned char readBuffer[8];

    semi_touch_create_nodes_dir();
    semi_touch_register_nodefun(chsc_version, chsc_version_node_write, chsc_version_node_read);
    semi_touch_register_nodefun(chsc_tp_info, chsc_tp_info_node_write, chsc_tp_info_node_read);
    semi_touch_register_nodefun(chsc_proximity, chsc_proximity_node_write, chsc_proximity_node_read);
    semi_touch_register_nodefun(chsc_guesture, chsc_guesture_node_write, chsc_guesture_node_read);
    semi_touch_register_nodefun(chsc_glove, chsc_glove_node_write, chsc_glove_node_read);
    semi_touch_register_nodefun(chsc_suspend, chsc_suspend_node_write, chsc_suspend_node_read);
    semi_touch_register_nodefun(chsc_online_update, chsc_online_update_node_write, chsc_online_update_node_read);
    semi_touch_register_nodefun(chsc_orientation, chsc_orientation_node_write, chsc_orientation_node_read);
    semi_touch_register_nodefun(chsc_esd_check, chsc_esd_check_node_write, chsc_esd_check_node_read);
    semi_touch_register_nodefun(chsc_wet_finger, chsc_wet_finger_node_write, chsc_wet_finger_node_read);
    semi_touch_register_nodefun(chsc_h_v_sw, chsc_h_v_sw_node_write, chsc_h_v_sw_node_read);
    semi_touch_register_nodefun(chsc_selftest, chsc_tp_selftest_node_write, chsc_tp_selftest_node_read);
    semi_touch_register_nodefun(chsc_charger, chsc_charger_node_write, chsc_charger_node_read);
    semi_touch_register_nodefun(chsc_game_mode, chsc_game_mode_node_write, chsc_game_mode_node_read);
    semi_touch_register_nodefun(chsc_cmd_change, chsc_cmd_change_node_write, chsc_cmd_change_node_read);
    semi_touch_register_nodefun(chsc_palm_mode, chsc_palm_mode_node_write, chsc_palm_mode_node_read);
    semi_touch_register_nodefun(chsc_game_op, chsc_game_op_mode_node_write, chsc_game_op_mode_node_read);
    semi_touch_register_nodefun(chsc_high_sr, chsc_high_sr_mode_node_write, chsc_high_sr_mode_node_read);
    semi_touch_register_nodefun(chsc_game_inhibit, chsc_game_touch_inhibit_node_write, chsc_game_touch_inhibit_node_read);
    semi_touch_register_nodefun(chsc_free_cmd, chsc_free_cmd_node_write, chsc_free_cmd_node_read);
    semi_touch_register_nodefun(chsc_pixel_locking, chsc_pixel_locking_node_write, chsc_pixel_locking_node_read);
    semi_touch_register_nodefun(chsc_eaa_touch, chsc_eaa_touch_node_write, chsc_eaa_touch_node_read);
    semi_touch_register_nodefun(chsc_region_ctrl, chsc_region_ctrl_node_write, chsc_region_ctrl_node_read);
    /*USD:modify for transsion core by yun.luo 20241225 start*/
    tran_touch_register_nodefun(fingerprint_status, fingerprint_status_node_write, fingerprint_status_node_read);
    tran_touch_register_nodefun(report_rate_switch, report_rate_switch_proc_write, report_rate_switch_proc_read);
    /*USD:modify for transsion core by yun.luo 20241225 end*/

    ret = semi_touch_proximity_init();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_gesture_prepare();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_esd_check_prepare();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_glove_prepare();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_wet_finger_prepare();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_h_v_sw_prepare();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_charger_prepare();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_game_mode_prepare();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_game_op_mode_prepare();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_palm_mode_prepare();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_high_sr_mode_prepare();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_game_touch_inhibit_prepare();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_eaa_touch_prepare();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_region_ctrl_prepare();
    check_return_if_fail(ret, NULL);

    semi_touch_send_rtime();

    //this code tell us how to get tp infomation
    //ret = semi_touch_read_bytes(0x20000000 + 0x80, readBuffer, sizeof(readBuffer));
    //check_return_if_fail(ret, NULL);

    // strcat(TP_NAME, mapping_ic_from_type(readBuffer[0]));
    // TP_FW_VER  = readBuffer[1];
    // TP_VENDOR  = readBuffer[4];
    // TP_PRODUCT = readBuffer[3] << 8) + readBuffer[2];

    return ret;
}

int semi_touch_custom_clean_up(void)
{
    int ret;

    semi_touch_release_nodes_dir();

    ret = semi_touch_proximity_stop();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_gesture_stop();
    check_return_if_fail(ret, NULL);

    return ret;
}
