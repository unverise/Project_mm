#define LOG_TAG         "Test"

//#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_strerror.h"
#include "cts_test.h"
#include "cts_tcs.h"
#include "cts_transfer.h"


#ifdef CTS_CONFIG_MKDIR_FOR_CTS_TEST
/* for ksys_mkdir/sys_mkdir */
#include <linux/syscalls.h>
#include <linux/namei.h>
#endif /* CTS_CONFIG_MKDIR_FOR_CTS_TEST */

const char *cts_test_item_str(int test_item)
{
#define case_test_item(item) \
    case CTS_TEST_ ## item: return #item "-TEST"

    switch (test_item) {
        case_test_item(RESET_PIN);
        case_test_item(INT_PIN);
        case_test_item(RAWDATA);
        case_test_item(NOISE);
        case_test_item(OPEN);
        case_test_item(SHORT);
        case_test_item(COMPENSATE_CAP);

    default:
        return "INVALID";
    }
#undef case_test_item
}

#define RAWDATA_BUFFER_SIZE(cts_dev) \
    (cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * 2)


const static struct cts_test_val cts_test_val[] = {
    {
        .hwid = CTS_DEV_HWID_ICNL9916X,
        .short_to_gnd_loop = 1,
        .short_to_row_loop = 6,
        .short_to_col_loop = 3
    },
    {
        .hwid = CTS_DEV_HWID_ICNL9951C,
        .short_to_gnd_loop = 1,
        .short_to_row_loop = 3,
        .short_to_col_loop = 1
    }
};

int set_display_state(const struct cts_device *cts_dev, bool active)
{
    int ret;
    u8 access_flag;
    u8 val;

    cts_info("Set display state to %s", active ? "ACTIVE" : "SLEEP");

    ret = cts_read_hw_reg(cts_dev,
            cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_DDI_REG_CTRL,
            &access_flag, 1, false);
    if (ret) {
        cts_err("Read display access flag failed %d", ret);
        return ret;
    }

    access_flag |= 0x01;
    ret = cts_write_ddi_reg(cts_dev,
            cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_DDI_REG_CTRL,
            &access_flag, 1, false);
    if (ret) {
        cts_err("Write display access flag %02x failed %d", access_flag, ret);
        return ret;
    }

    val = 0x55;
    if (active) {
        ret = cts_write_ddi_reg(cts_dev, 0x3C044, &val, 1, false);
        if (ret) {
            cts_err("Write DCS-CMD11 fail");
            return ret;
        }

        msleep(100);

        ret = cts_write_ddi_reg(cts_dev, 0x3C0A4, &val, 1, false);
        if (ret) {
            cts_err("Write DCS-CMD29 fail");
            return ret;
        }

        msleep(100);
    } else {
        ret = cts_write_ddi_reg(cts_dev, 0x3C0A0, &val, 1, false);
        if (ret) {
            cts_err("Write DCS-CMD28 fail");
            return ret;
        }

        msleep(100);

        ret = cts_write_ddi_reg(cts_dev, 0x3C040, &val, 1, false);
        if (ret) {
            cts_err("Write DCS-CMD10 fail");
            return ret;
        }

        msleep(100);
    }

    ret = cts_write_ddi_reg(cts_dev,
            cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_DDI_REG_CTRL,
            &access_flag, 1, false);
    if (ret) {
        cts_err("Restore display access flag %02x failed %d", access_flag, ret);
        return ret;
    }

    return 0;
}

int cts_write_file(struct file *filp, const void *data, size_t size)
{
#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): kernel_write is forbiddon with GKI Version!", __func__);
    return -EPERM;
#else
    loff_t pos;
    ssize_t ret;

    pos = filp->f_pos;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    ret = kernel_write(filp, data, size, &pos);
#else
    ret = kernel_write(filp, data, size, pos);
#endif

    if (ret >= 0) {
        filp->f_pos += ret;
    }

    return ret;
#endif
}

#ifdef CTS_CONFIG_MKDIR_FOR_CTS_TEST
/* copied from fs/namei.c */
long do_mkdirat(int dfd, const char __user *pathname, umode_t mode)
{
    struct dentry *dentry;
    struct path path;
    int error;
    unsigned int lookup_flags = LOOKUP_DIRECTORY;

retry:
    dentry = user_path_create(dfd, pathname, &path, lookup_flags);
    if (IS_ERR(dentry))
          return PTR_ERR(dentry);

    if (!IS_POSIXACL(path.dentry->d_inode))
        mode &= ~current_umask();
    error = security_path_mkdir(&path, dentry, mode);
    if (!error)
        error = vfs_mkdir(path.dentry->d_inode, dentry, mode);
    done_path_create(&path, dentry);
    if (retry_estale(error, lookup_flags)) {
        lookup_flags |= LOOKUP_REVAL;
        goto retry;
    }
    return error;
}

/* Make directory for filepath
 * If filepath = "/A/B/C/D.file", it will make dir /A/B/C recursive
 * like userspace mkdir -p
 */
int cts_mkdir_for_file(const char *filepath, umode_t mode)
{
#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
    return -EPERM;
#else
    char *dirname = NULL;
    int dirname_len;
    char *s;
    int ret;
    mm_segment_t fs;

    if (filepath == NULL) {
        cts_err("Create dir for file with filepath = NULL");
        return -EINVAL;
    }

    if (filepath[0] == '\0' || filepath[0] != '/') {
        cts_err("Create dir for file with invalid filepath[0]: %c", filepath[0]);
        return -EINVAL;
    }

    dirname_len = strrchr(filepath, '/') - filepath;
    if (dirname_len == 0) {
        cts_warn("Create dir for file '%s' in root dir", filepath);
        return 0;
    }

    dirname = kstrndup(filepath, dirname_len, GFP_KERNEL);
    if (dirname == NULL) {
        cts_err("Create dir alloc mem for dirname failed");
        return -ENOMEM;
    }

    cts_info("Create dir '%s' for file '%s'", dirname, filepath);

    fs = get_fs();
    set_fs(KERNEL_DS);

    s = dirname + 1;    /* Skip leading '/' */
    while (1) {
        char c = '\0';

        /* Bypass leading non-'/'s and then subsequent '/'s */
        while (*s) {
            if (*s == '/') {
                do {
                    ++s;
                } while (*s == '/');
                c = *s;    /* Save current char */
                *s = '\0';    /* and replace it with nul */
                break;
            }
            ++s;
        }

        cts_dbg(" - Create dir '%s'", dirname);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
        ret = ksys_mkdir(dirname, 0777);
#else
        ret = sys_mkdir(dirname, 0777);
#endif
        if (ret < 0 && ret != -EEXIST) {
            cts_info("Create dir '%s' failed %d(%s)", dirname, ret,
                cts_strerror(ret));
            /* Remove any inserted nul from the path */
            *s = c;
            break;
        }
        /* Reset ret to 0 if return -EEXIST */
        ret = 0;

        if (c) {
            /* Remove any inserted nul from the path */
            *s = c;
        } else {
            break;
        }
    }

    set_fs(fs);

    if (dirname) {
        kfree(dirname);
    }

    return ret;
#endif
}
#endif /* CTS_CONFIG_MKDIR_FOR_CTS_TEST */

struct file *cts_test_data_filp;
int cts_start_dump_test_data_to_file(const char *filepath, bool append_to_file)
{
    int ret;

#define START_BANNER \
    ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"
    if (!filepath) {
        cts_err("invalid file path");
        return -EINVAL;
    }
    cts_info("Start dump test data to file '%s'", filepath);

#ifdef CTS_CONFIG_MKDIR_FOR_CTS_TEST
    ret = cts_mkdir_for_file(filepath, 0777);
    if (ret) {
        cts_err("Create dir for test data file failed %d", ret);
        return ret;
    }
#endif /* CTS_CONFIG_MKDIR_FOR_CTS_TEST */

#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): filp_open is forbiddon with GKI Version!", __func__);
    ret = -EPERM;
    return ret;
#else
    cts_test_data_filp = filp_open(filepath,
        O_WRONLY | O_CREAT | (append_to_file ? O_APPEND : O_TRUNC),
        S_IRUGO | S_IWUGO);
    if (IS_ERR(cts_test_data_filp)) {
        ret = PTR_ERR(cts_test_data_filp);
        cts_test_data_filp = NULL;
        cts_err("Open file '%p' for test data failed %d", cts_test_data_filp, ret);
        return ret;
    }

    cts_write_file(cts_test_data_filp, START_BANNER, strlen(START_BANNER));

    return 0;
#endif
#undef START_BANNER
}

void cts_stop_dump_test_data_to_file(void)
{
#define END_BANNER \
    "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
#ifndef CFG_CTS_FOR_GKI
    int r;
#endif

    cts_info("Stop dump test data to file");

    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, END_BANNER, strlen(END_BANNER));
#ifndef CFG_CTS_FOR_GKI
        r = filp_close(cts_test_data_filp, NULL);
        if (r) {
            cts_err("Close test data file failed %d", r);
        }
#endif
        cts_test_data_filp = NULL;
    } else {
        cts_warn("Stop dump tsdata to file with filp = NULL");
    }
#undef END_BANNER
}

static void cts_dump_tsdata(struct cts_device *cts_dev,
    const char *desc, const u16 *data, bool to_console)
{
#define SPLIT_LINE_STR \
    "--------------------------------------------------------"\
    "--------------------------------------------------------"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%-5u "
#define DATA_FORMAT_STR     "%-5u "

    int r, c;
    u8 rows, cols;
    u32 max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    char line_buf[256];
    int count = 0;

    if (cts_dev->hwdata->support_cascade) {
        rows = cts_dev->fwdata.panel_info.fs_rows;
        cols = cts_dev->fwdata.panel_info.fs_cols;
    } else {
        rows = cts_dev->fwdata.xy_info.rows;
        cols = cts_dev->fwdata.xy_info.cols;
    }

    max = min = data[0];
    sum = 0;
    max_r = max_c = min_r = min_c = 0;
    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            u16 val = data[r * cts_dev->hwdata->num_col + c];

            sum += val;
            if (val > max) {
                max = val;
                max_r = r;
                max_c = c;
            } else if (val < min) {
                min = val;
                min_r = r;
                min_c = c;
            }
        }
    }
    average = sum / (rows * cols);

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count,
        " %s test data MIN: [%u][%u]=%u, MAX: [%u][%u]=%u, AVG=%u",
        desc, min_r, min_c, min, max_r, max_c, max, average);
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count, "   |  ");
    for (c = 0; c < cols; c++) {
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            COL_NUM_FORMAT_STR, c);
    }
    if (to_console) {
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    for (r = 0; r < rows; r++) {
        count = 0;
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            ROW_NUM_FORMAT_STR, r);
        for (c = 0; c < cols; c++) {
            count += scnprintf(line_buf + count, sizeof(line_buf) - count,
                DATA_FORMAT_STR, data[r * cts_dev->hwdata->num_col + c]);
        }
        if (to_console) {
            cts_info("%s", line_buf);
        }
        if (cts_test_data_filp) {
            cts_write_file(cts_test_data_filp, line_buf, count);
            cts_write_file(cts_test_data_filp, "\n", 1);
        }
    }
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }
#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
}

static bool is_invalid_node(u32 *invalid_nodes, u32 num_invalid_nodes,
    u16 row, u16 col)
{
    int i;

    for (i = 0; i < num_invalid_nodes; i++) {
        if (MAKE_INVALID_NODE(row, col) == invalid_nodes[i]) {
            return true;
        }
    }

    return false;
}

/* Return number of failed nodes */
static int validate_tsdata(struct cts_device *cts_dev, const char *desc, u16 *data,
    u32 *invalid_nodes, u32 num_invalid_nodes, bool per_node, int *min, int *max)
{
#define SPLIT_LINE_STR \
    "------------------------------"

    int r, c;
    u8 rows, cols;
    int failed_cnt = 0;

    if (cts_dev->hwdata->support_cascade) {
        rows = cts_dev->fwdata.panel_info.fs_rows;
        cols = cts_dev->fwdata.panel_info.fs_cols;
    } else {
        rows = cts_dev->fwdata.xy_info.rows;
        cols = cts_dev->fwdata.xy_info.cols;
    }

    cts_info("%s validate data: %s, num invalid node: %u, thresh[0]=[%d, %d]",
        desc, per_node ? "Per-Node" : "Uniform-Threshold",
        num_invalid_nodes, min ? min[0] : INT_MIN, max ? max[0] : INT_MAX);

    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            int offset = r * cts_dev->hwdata->num_col + c;

            if (num_invalid_nodes &&
                is_invalid_node(invalid_nodes, num_invalid_nodes, r, c)) {
                continue;
            }

            if ((min != NULL && data[offset] < min[per_node ? offset : 0]) ||
                (max != NULL && data[offset] > max[per_node ? offset : 0])) {
                if (failed_cnt == 0) {
                    cts_info(SPLIT_LINE_STR);
                    cts_info("%s failed nodes:", desc);
                }
                failed_cnt++;

                cts_info("  %3d: [%-2d][%-2d] = %u", failed_cnt, r, c, data[offset]);
            }
        }
    }

    if (failed_cnt) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s test %d node total failed", desc, failed_cnt);
    }

    return failed_cnt;

#undef SPLIT_LINE_STR
}

int prepare_product_test(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Prepare test");

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        return ret;
    }

    cts_reset_device(cts_dev);

    cts_dev->rtdata.testing = true;

    return 0;
}

void post_product_test(struct cts_device *cts_dev)
{
    cts_info("Post test");

    cts_reset_device(cts_dev);

    cts_tcs_reset_device(cts_dev);

    /* reset int data size to normal size */
    cts_enter_normal_mode(cts_dev);

    cts_start_device(cts_dev);

    cts_dev->rtdata.testing = false;
}

int _prepare_test(struct cts_device *cts_dev, int fw_data_type,
    int int_data_method, int work_mode, int open_short_type,
    bool is_stylus);

#ifdef CFG_CTS_HAS_RESET_PIN
int cts_test_reset_pin(struct cts_device *cts_dev)
{
    ktime_t start_time, end_time, delta_time;
    u8 reg_val_default = 0;
    u8 reg_val_new = 0;
    u8 reg_val_readback = 0;
    u8 reg_val_rst = 0;
    int ret;


    if (cts_dev == NULL) {
        cts_err("Reset-pin test with invalid param: cts_dev: %p",
            cts_dev);
        return -EINVAL;
    }

    cts_info("Reset-Pin test, reset pin num:%d",cts_dev->pdata->rst_gpio);

    start_time = ktime_get();

    cts_lock_device(cts_dev);

    ret = cts_enter_program_mode(cts_dev);
    if (ret) {
        cts_err("Enter program mode failed %d", ret);
        goto print_test_time;
    }

    ret = cts_read_hw_reg(cts_dev, CTS_TEST_REG_FOR_RESET_TEST, &reg_val_default,
        1, false);
    if (ret) {
        cts_err("Reset-Pin test, reading 'REG_FOR_RESET_TEST' failed");
        goto print_test_time;
    }
    reg_val_new = ~reg_val_default;

    ret = cts_write_hw_reg(cts_dev, CTS_TEST_REG_FOR_RESET_TEST, &reg_val_new,
        1, false);
    if (ret) {
        cts_err("Reset-Pin test, writing 'REG_FOR_RESET_TEST' failed");
        goto print_test_time;
    }
    ret = cts_read_hw_reg(cts_dev, CTS_TEST_REG_FOR_RESET_TEST, &reg_val_readback,
        1, false);
    if (reg_val_new != reg_val_readback) {
        cts_err("reg_val_new(%d) != reg_val_readback(%d)",
            reg_val_new, reg_val_readback);
        goto print_test_time;
    }

    cts_reset_device(cts_dev);
    mdelay(80);

    ret = cts_enter_program_mode(cts_dev);
    if (ret) {
        cts_err("Enter program mode failed %d", ret);
        goto print_test_time;
    }

    ret = cts_read_hw_reg(cts_dev, CTS_TEST_REG_FOR_RESET_TEST, &reg_val_rst,
        1, false);
    if (ret) {
        cts_err("Reset-Pin test, reading 'REG_FOR_RESET_TEST' failed");
        goto print_test_time;
    }

    ret = CTS_TEST_FAIL;
    if (reg_val_rst == reg_val_default && reg_val_rst != reg_val_new)
        ret = CTS_TEST_PASS;

    cts_info("%s: %d, reg_val_default:%d, reg_val_new:%d, reg_val_rst:%d, ret:%d",
        __func__, __LINE__, reg_val_default, reg_val_new, reg_val_rst, ret);

print_test_time:
    cts_unlock_device(cts_dev);
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret) {
        cts_info("Reset-Pin test FAIL %d(%s), ELAPSED TIME: %lldms",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Reset-Pin test PASS, ELAPSED TIME: %lldms",
            ktime_to_ms(delta_time));
    }

    return ret;
}
#endif

int cts_test_int_pin(struct cts_device *cts_dev)
{
    ktime_t start_time, end_time, delta_time;
    int ret, i = 0;

    if (cts_dev == NULL) {
        cts_err("Int-pin test with invalid param: cts_dev: %p",
            cts_dev);
        return -EINVAL;
    }

    cts_info("Int-Pin test");

    start_time = ktime_get();
    g_int_pin_test_value = CTS_INT_DEFAULT_TEST_VAL;
    cts_lock_device(cts_dev);

    ret = cts_switch_to_test_irq(cts_dev->pdata);
    if (ret) {
        cts_err("switch to test irq failed");
        goto unlock_device;
    }

    ret = cts_enter_normal_mode(cts_dev);
    if (ret) {
        cts_err("test int pin, enter normal mode failed");
        goto unlock_device;
    }

    ret = cts_tcs_set_int_test(cts_dev, 1);
    if (ret) {
        cts_err("Enable Int Test failed");
        goto unlock_device;
    }
    ret = cts_tcs_set_int_pin(cts_dev, 1);
    if (ret) {
        cts_err("Enable Int Test High failed");
        goto exit_int_test;
    }
    mdelay(10);
    if (cts_plat_get_int_pin(cts_dev->pdata) == 0) {
        cts_err("INT pin state != HIGH");
        ret = -EFAULT;
        goto exit_int_test;
    }

    ret = cts_tcs_set_int_pin(cts_dev, 0);
    if (ret) {
        cts_err("Enable Int Test LOW failed");
        goto exit_int_test;
    }
    mdelay(10);
    if (cts_plat_get_int_pin(cts_dev->pdata) != 0) {
        cts_err("INT pin state != LOW");
        ret = -EFAULT;
        goto exit_int_test;
    }

    ret = CTS_TEST_FAIL;
    do {
        if (g_int_pin_test_value != CTS_INT_DEFAULT_TEST_VAL) {
            ret = CTS_TEST_PASS;
            break;
        }
        mdelay(10);
        i++;
    } while (i < 20);

exit_int_test:
        if (cts_tcs_set_int_test(cts_dev, 0))
            cts_err("Disable Int Test failed");

    mdelay(10);

unlock_device:
    cts_unlock_device(cts_dev);

    if (restore_normal_irq(cts_dev->pdata))
        cts_err("test int pin, restore normal irq failed");

    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret) {
        cts_info("Int-Pin test FAIL %d(%s), ELAPSED TIME: %lldms",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Int-Pin test PASS, ELAPSED TIME: %lldms",
            ktime_to_ms(delta_time));
    }

    return ret;
}

int _prepare_test(struct cts_device *cts_dev, int fw_data_type,
    int int_data_method, int work_mode, int open_short_type,
    bool is_stylus)
{
    int wok_mode_cfg = CTS_FIRMWARE_WORK_MODE_CFG;
    int data_type = fw_data_type;
    int ret;

    ret = cts_enter_normal_mode(cts_dev);
    if (ret) {
        cts_err("enter normal mode failed %d", ret);
        goto err_out;
    }

    if (cts_dev->hwdata->support_cascade)
        wok_mode_cfg = CTS_FIRMWARE_KRANG_WORK_MODE_CFG;

    ret = cts_tcs_set_workmode(cts_dev, wok_mode_cfg);
    if (ret) {
        cts_err("Set firmware work mode to config mode failed %d", ret);
        goto err_out;
    }

    ret = cts_tcs_set_product_en(cts_dev, 1);
    if (ret) {
        cts_err("Set product en failed %d", ret);
        goto err_out;
    }

    if (open_short_type !=CTS_TEST_NONE_OPEN_SHORT) {
        ret = cts_tcs_set_openshort_mode(cts_dev, open_short_type);
        if (ret) {
            cts_err("Set open-short mode failed %d", ret);
            goto err_out;
        }

        if (open_short_type == CTS_TEST_SHORT) {
            ret = cts_tcs_set_short_test_type(cts_dev, CTS_SHORT_TEST_UNDEFINED);
            if (ret) {
                cts_err("Set short type to standby failed %d", ret);
                goto err_out;
            }
        }
    }

    ret = cts_tcs_set_pwr_mode(cts_dev, PWR_MODE_ACTIVE);
    if (ret) {
        cts_err("Set power mode to active state failed %d", ret);
        goto err_out;
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set int data method to none failed %d", ret);
        goto err_out;
    }

    if (is_stylus){
        data_type |= CTS_INT_DATA_STYLUS;
    }
	
    ret = cts_tcs_set_workmode(cts_dev, work_mode);
    if (ret) {
        cts_err("Set firmware work mode:%d failed %d", work_mode, ret);
        goto err_out;
    }
	
    ret = cts_set_int_data_types(cts_dev, data_type);
    if (ret) {
        cts_err("Set int data type:%d failed %d", data_type, ret);
        goto err_out;
    }

    ret = cts_set_int_data_method(cts_dev, int_data_method);
    if (ret) {
        cts_err("Set int data method:%d failed %d", int_data_method, ret);
        goto err_out;
    }

err_out:
    return ret;
}

extern int cts_test_polling_rawdata(struct cts_device *cts_dev,
    u8 *buf, size_t size);

int prepare_rawdata_test(struct cts_device *cts_dev)
{
    int work_mode = CTS_FIRMWARE_WORK_MODE_NORMAL;

    if (cts_dev->hwdata->support_cascade)
        work_mode = CTS_FIRMWARE_KRANG_WORK_MODE_FINGER_DBG;

    return _prepare_test(cts_dev, INT_DATA_TYPE_RAWDATA,
            INT_DATA_METHOD_POLLING, work_mode,
            CTS_TEST_NONE_OPEN_SHORT, false);
}

int cts_test_rawdata(struct cts_device *cts_dev, struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool stop_test_if_validate_fail = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int frame = 0;
    int  fail_frame = 0;
    u16 *rawdata = NULL;
    ktime_t start_time, end_time, delta_time;
    int ret;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Rawdata test with invalid param.");
        return -EINVAL;
    }

    if (param->sample_frames <= 0) {
        cts_info("Rawdata test with invalide frame num: %u", param->sample_frames);
        return -EINVAL;
    }

    num_nodes = cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_test_if_validate_fail =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    cts_info("Rawdata test, flags: 0x%08x, frames: %d, num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->sample_frames, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        rawdata = (u16 *) param->test_data_buf;
        if (rawdata == NULL) {
            cts_err("user memory is null, remove flag "
                "\"CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE\" and try again");
            ret = -ENOMEM;
            goto show_test_result;
        }

    } else {
        rawdata = (u16 *) kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (rawdata == NULL) {
            cts_err("Allocate memory for rawdata failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    if (dump_test_data_to_file) {
        ret = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (ret)
            cts_err("Start dump test data to file failed %d", ret);
    }

    cts_lock_device(cts_dev);

    ret = prepare_rawdata_test(cts_dev);
    if (ret)
        goto free_mem;

    /* drop 2 frames so that the rawdata will be more stable*/
    cts_tcs_drop_frames(cts_dev, 2);

    for (frame = 0; frame < param->sample_frames; frame++) {
        cts_dbg("start polling the %d th frame", frame + 1);
        ret = cts_test_polling_rawdata(cts_dev, (u8 *)rawdata,
                RAWDATA_BUFFER_SIZE(cts_dev));
        if (ret < 0)
            goto free_mem;

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Rawdata", rawdata, dump_test_data_to_console);
        }

        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev, "Rawdata", rawdata, param->invalid_nodes,
                param->num_invalid_node, validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Rawdata test failed %d", ret);
                fail_frame++;
                cts_err("Rawdata test has %d nodes failed", ret);
                if (stop_test_if_validate_fail) {
                    break;
                }
            }
        }

        if (dump_test_data_to_user) {
            rawdata += num_nodes;
            *param->test_data_size += tsdata_frame_size;
        }
    }

free_mem:
    cts_unlock_device(cts_dev);

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    if (!dump_test_data_to_user && rawdata != NULL) {
        kfree(rawdata);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);

    if (ret) {
        cts_info("Rawdata test FAIL %d(%s), ELAPSED TIME: %lldms",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else if (fail_frame > 0) {
        cts_info("Rawdata test has %d frame(s) FAIL, ELAPSED TIME: %lldms",
            fail_frame, ktime_to_ms(delta_time));
    } else if (fail_frame == 0) {
        cts_info("Rawdata test PASS, ELAPSED TIME: %lldms",
            ktime_to_ms(delta_time));
    }

    return (ret ? ret : (fail_frame ? fail_frame : 0));
}

int prepare_noise_test(struct cts_device *cts_dev)
{
    int work_mode = CTS_FIRMWARE_WORK_MODE_NORMAL;

    if (cts_dev->hwdata->support_cascade)
        work_mode = CTS_FIRMWARE_KRANG_WORK_MODE_FINGER_DBG;

    return _prepare_test(cts_dev, INT_DATA_TYPE_RAWDATA,
            INT_DATA_METHOD_POLLING, work_mode,
            CTS_TEST_NONE_OPEN_SHORT, false);
}

int cts_test_noise(struct cts_device *cts_dev, struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int frame = 0;
    u16 *buffer = NULL;

    u16 *curr_rawdata = NULL;
    u16 *max_rawdata = NULL;
    u16 *min_rawdata = NULL;
    u16 *noise = NULL;
    bool first_frame = true;
    ktime_t start_time, end_time, delta_time;
    int ret, i;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Noise test with invalid param.");
        return -EINVAL;
    }

    if (param->sample_frames < 2) {
        cts_err("Noise test with invalide frame num: %u", param->sample_frames);
        return -EINVAL;
    }

    num_nodes = cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    cts_info("Noise test, flags: 0x%08x, frames: %d, num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->sample_frames, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        curr_rawdata = (u16 *) param->test_data_buf;
        if (curr_rawdata == NULL) {
            cts_err("user memory is null, remove flag "
                "\"CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE\" and try again");
            ret = -ENOMEM;
            goto show_test_result;
        }
    } else {
        curr_rawdata = (u16 *) kzalloc(tsdata_frame_size, GFP_KERNEL);
        if (curr_rawdata == NULL) {
            cts_err("Allocate memory for rawdata failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    if (driver_validate_data) {
        buffer = (u16 *) kzalloc(3 * tsdata_frame_size, GFP_KERNEL);
        if (buffer == NULL) {
            cts_err("Alloc mem for touch data failed");
            ret = -ENOMEM;
            goto free_mem;
        }
        max_rawdata    = buffer;
        min_rawdata    = buffer + 1 * num_nodes;
        noise        = buffer + 2 * num_nodes;
    }

    if (dump_test_data_to_file) {
        ret = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (ret) {
            cts_err("Start dump test data to file failed %d", ret);
        }
    }

    cts_lock_device(cts_dev);

    ret = prepare_noise_test(cts_dev);
    if (ret)
        goto free_mem;

    /* drop 2 frames so that the rawdata will be more stable*/
    cts_tcs_drop_frames(cts_dev, 2);

    for (frame = 0; frame < param->sample_frames; frame++) {
        cts_dbg("start polling the %d th frame", frame + 1);
        ret = cts_test_polling_rawdata(cts_dev, (u8 *)curr_rawdata,
            RAWDATA_BUFFER_SIZE(cts_dev));
        if (ret < 0)
            goto free_mem;

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Noise-rawdata", curr_rawdata,
                dump_test_data_to_console);
        }

        if (driver_validate_data) {
            if (unlikely(first_frame)) {
                memcpy(max_rawdata, curr_rawdata, tsdata_frame_size);
                memcpy(min_rawdata, curr_rawdata, tsdata_frame_size);
                first_frame = false;
            } else {
                for (i = 0; i < num_nodes; i++) {
                    if (curr_rawdata[i] > max_rawdata[i]) {
                        max_rawdata[i] = curr_rawdata[i];
                    } else if (curr_rawdata[i] < min_rawdata[i]) {
                        min_rawdata[i] = curr_rawdata[i];
                    }
                }
            }
        }

        if (dump_test_data_to_user) {
            curr_rawdata += num_nodes;
            *param->test_data_size += tsdata_frame_size;
        }
    }

    if (driver_validate_data) {
        for (i = 0; i < num_nodes; i++) {
            noise[i] = max_rawdata[i] - min_rawdata[i];
        }

        if (dump_test_data_to_user) {
            memcpy(param->test_data_buf + (frame + 0) * num_nodes,
                noise, tsdata_frame_size);
            memcpy(param->test_data_buf + (frame + 1) * num_nodes,
                max_rawdata, tsdata_frame_size);
            memcpy(param->test_data_buf + (frame + 2) * num_nodes,
                min_rawdata, tsdata_frame_size);
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Noise", noise,
                dump_test_data_to_console);
            cts_dump_tsdata(cts_dev, "Rawdata MAX", max_rawdata,
                dump_test_data_to_console);
            cts_dump_tsdata(cts_dev, "Rawdata MIN", min_rawdata,
                dump_test_data_to_console);
        }

        ret = validate_tsdata(cts_dev, "Noise test", noise, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
    }

free_mem:
    cts_unlock_device(cts_dev);

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    if (!dump_test_data_to_user)
        kfree(curr_rawdata);

    if (driver_validate_data && buffer != NULL) {
        kfree(buffer);
        buffer = NULL;
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Noise test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Noise test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Noise test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}

int prepare_open_test(struct cts_device *cts_dev)
{
    int work_mode = CTS_FIRMWARE_WORK_MODE_OPEN_SHORT;

    if (cts_dev->hwdata->support_cascade)
        work_mode = CTS_FIRMWARE_KRANG_WORK_MODE_OPEN_SHORT;

    return _prepare_test(cts_dev, INT_DATA_TYPE_RAWDATA,
            INT_DATA_METHOD_POLLING, work_mode, CTS_TEST_TYPE_OPEN, false);
}

/* Return 0 success
    negative value while error occurs
    positive value means how many nodes fail */
int cts_test_open(struct cts_device *cts_dev, struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int ret;
    u16 *test_result = NULL;
    ktime_t start_time, end_time, delta_time;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Open test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    num_nodes = cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    cts_info("Open test, flags: 0x%08x, num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        test_result = (u16 *) param->test_data_buf;
        if (test_result == NULL) {
            cts_err("user memory is null, remove flag "
                "\"CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE\" and try again");
            ret = -ENOMEM;
            goto show_test_result;
        }
    } else {
        test_result = (u16 *) kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (test_result == NULL) {
            cts_err("Allocate memory for test result faild");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    cts_lock_device(cts_dev);

    ret = prepare_open_test(cts_dev);
    if (ret)
        goto free_mem;

    /* drop 1 frame so that the rawdata will be more stable*/
    cts_tcs_drop_frames(cts_dev, 1);

    ret = cts_tcs_polling_test_data(cts_dev, (u8 *)test_result,
            RAWDATA_BUFFER_SIZE(cts_dev));
    if (ret) {
        cts_err("Read test result failed %d", ret);
        goto free_mem;
    }

    if (dump_test_data_to_user) {
        *param->test_data_size += tsdata_frame_size;
    }

    if (dump_test_data_to_file) {
        ret = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (ret) {
            cts_err("Start dump test data to file failed %d", ret);
        }
    }

    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_tsdata(cts_dev, "Open-circuit", test_result,
            dump_test_data_to_console);
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    if (driver_validate_data) {
        ret = validate_tsdata(cts_dev, "Open-circuit", test_result, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
    }

free_mem:
    cts_unlock_device(cts_dev);

    if (!dump_test_data_to_user && test_result) {
        kfree(test_result);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Open test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Open test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Open test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    cts_reset_device(cts_dev);

    return ret;
}

int prepare_short_test(struct cts_device *cts_dev)
{
    int work_mode = CTS_FIRMWARE_WORK_MODE_OPEN_SHORT;

    if (cts_dev->hwdata->support_cascade)
        work_mode = CTS_FIRMWARE_KRANG_WORK_MODE_OPEN_SHORT;

    return _prepare_test(cts_dev, INT_DATA_TYPE_RAWDATA,
            INT_DATA_METHOD_POLLING, work_mode, CTS_TEST_TYPE_SHORT, false);
}

/* Return 0 success
    negative value while error occurs
    positive value means how many nodes fail */
int cts_test_short(struct cts_device *cts_dev, struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool stop_if_failed = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int loopcnt;
    int ret;
    u16 *test_result = NULL;
    ktime_t start_time, end_time, delta_time;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Short test with invalid param: cts_dev: %p test param: %p",
             cts_dev, param);
        return -EINVAL;
    }

    num_nodes = cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_if_failed =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    cts_info("Short test, flags: 0x%08x, num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        test_result = (u16 *) param->test_data_buf;
        if (test_result == NULL) {
            cts_err("user memory is null, remove flag "
                "\"CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE\" and try again");
            ret = -ENOMEM;
            goto show_test_result;
        }
    } else {
        test_result = (u16 *) kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (test_result == NULL) {
            cts_err("Allocate test result buffer failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    if (dump_test_data_to_file) {
        ret = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (ret) {
            cts_err("Start dump test data to file failed %d", ret);
        }
    }

    cts_lock_device(cts_dev);

    ret = prepare_short_test(cts_dev);
    if (ret)
        goto short_test_abort;

    cts_info("Test short to GND");
    ret = cts_tcs_set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_GND);
    if (ret) {
        cts_err("Set short test type to SHORT_TO_GND failed %d", ret);
        goto short_test_abort;
    }

    for (loopcnt = 0; loopcnt < cts_dev->test_vals->short_to_gnd_loop; loopcnt++) {
        ret = cts_tcs_polling_test_data(cts_dev, (u8 *)test_result,
            RAWDATA_BUFFER_SIZE(cts_dev));
        if (ret) {
            cts_err("Read test result failed %d", ret);
            goto short_test_abort;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "GND-short", test_result,
                dump_test_data_to_console);
        }
        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev, "GND-short", test_result, param->invalid_nodes,
                param->num_invalid_node, validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Short to GND test failed %d", ret);
                if (stop_if_failed) {
                    goto short_test_abort;
                }
            }
        }
        if (dump_test_data_to_user) {
            test_result += num_nodes;
            *param->test_data_size += tsdata_frame_size;
        }
    }

    /* Short between colums */
    cts_info("Test short between columns");
    ret = cts_tcs_set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_COLS);
    if (ret) {
        cts_err("Set short test type to BETWEEN_COLS failed %d", ret);
        goto short_test_abort;
    }

    for (loopcnt = 0; loopcnt < cts_dev->test_vals->short_to_col_loop; loopcnt++) {
        ret = cts_tcs_polling_test_data(cts_dev, (u8 *)test_result,
                RAWDATA_BUFFER_SIZE(cts_dev));
        if (ret) {
            cts_err("Read test result failed %d", ret);
            goto short_test_abort;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Col-short", test_result,
                dump_test_data_to_console);
        }

        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev, "Col-short", test_result, param->invalid_nodes,
                param->num_invalid_node, validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Short between columns test failed %d", ret);
                if (stop_if_failed) {
                    goto short_test_abort;
                }
            }
        }
        if (dump_test_data_to_user) {
            test_result += num_nodes;
            *param->test_data_size += tsdata_frame_size;
        }
    }

    /* Short between rows */
    cts_info("Test short between rows");
    ret = cts_tcs_set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_ROWS);
    if (ret) {
        cts_err("Set short test type to BETWEEN_ROWS failed %d", ret);
        goto short_test_abort;
    }

	for (loopcnt = 0; loopcnt < cts_dev->test_vals->short_to_row_loop; loopcnt++) {
        ret = cts_tcs_polling_test_data(cts_dev, (u8 *)test_result,
                RAWDATA_BUFFER_SIZE(cts_dev));
        if (ret) {
            cts_err("Read test result failed %d", ret);
            goto short_test_abort;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Row-short", test_result,
                dump_test_data_to_console);
        }
        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev, "Row-short", test_result, param->invalid_nodes,
                param->num_invalid_node, validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Short between rows test failed %d", ret);
                if (stop_if_failed) {
                    goto short_test_abort;
                }
            }
        }
        if (dump_test_data_to_user) {
            test_result += num_nodes;
            *param->test_data_size += tsdata_frame_size;
        }
    }

short_test_abort:
    cts_unlock_device(cts_dev);

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    if (!dump_test_data_to_user && test_result) {
        kfree(test_result);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Short test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Short test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Short test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    cts_reset_device(cts_dev);

    return ret;
}

static int validate_comp_cap(struct cts_device *cts_dev, const char *desc, u8 *cap,
    u32 *invalid_nodes, u32 num_invalid_nodes, bool per_node, int *min, int *max)
{
#define SPLIT_LINE_STR \
    "------------------------------"

    int r, c;
    u8 rows, cols, hw_cols;
    int failed_cnt = 0;

    if (cts_dev->hwdata->support_cascade) {
        rows = cts_dev->fwdata.panel_info.fs_rows;
        cols = cts_dev->fwdata.panel_info.fs_cols;
        hw_cols = cts_dev->fwdata.channel_info.ic_fs_cols;
    } else {
        rows = cts_dev->fwdata.xy_info.rows;
        cols = cts_dev->fwdata.xy_info.cols;
        hw_cols = cts_dev->hwdata->num_col;
    }

    cts_info("Validate %s data: %s, num invalid node: %u, thresh[0]=[%d, %d]",
            desc, per_node ? "Per-Node" : "Uniform-Threshold",
            num_invalid_nodes, min ? min[0] : INT_MIN, max ? max[0] : INT_MAX);

    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            int offset = r * hw_cols + c;

            if (num_invalid_nodes &&
                is_invalid_node(invalid_nodes, num_invalid_nodes, r, c)) {
                continue;
            }

            if ((min != NULL && cap[offset] < min[per_node ? offset : 0]) ||
                (max != NULL && cap[offset] > max[per_node ? offset : 0])) {
                if (failed_cnt == 0) {
                    cts_info(SPLIT_LINE_STR);
                    cts_info("%s failed nodes:", desc);
                }
                failed_cnt++;

                cts_info("  %3d: [%-2d][%-2d] = %u", failed_cnt, r, c, cap[offset]);
            }
        }
    }

    if (failed_cnt) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s test %d node total failed", desc, failed_cnt);
    }

    return failed_cnt;

#undef SPLIT_LINE_STR
}

void cts_dump_comp_cap(struct cts_device *cts_dev, u8 *cap, bool to_console)
{
#define SPLIT_LINE_STR \
    "-----------------------------------------------------------------------------"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%3u "
#define DATA_FORMAT_STR     "%4d"

    int r, c;
    u8 rows, cols, hw_cols;
    u32 max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    char line_buf[256];
    int count;

    if (cts_dev->hwdata->support_cascade) {
        rows = cts_dev->fwdata.panel_info.fs_rows;
        cols = cts_dev->fwdata.panel_info.fs_cols;
        hw_cols = cts_dev->fwdata.channel_info.ic_fs_cols;
    } else {
        rows = cts_dev->fwdata.xy_info.rows;
        cols = cts_dev->fwdata.xy_info.cols;
        hw_cols = cts_dev->hwdata->num_col;
    }

    max = min = cap[0];
    sum = 0;
    max_r = max_c = min_r = min_c = 0;
    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            u16 val = cap[r * hw_cols + c];

            sum += val;
            if (val > max) {
                max = val;
                max_r = r;
                max_c = c;
            } else if (val < min) {
                min = val;
                min_r = r;
                min_c = c;
            }
        }
    }
    average = sum / (rows * cols);

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count,
        " Compensate Cap MIN: [%u][%u]=%u, MAX: [%u][%u]=%u, AVG=%u",
        min_r, min_c, min, max_r, max_c, max, average);
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count, "      ");
    for (c = 0; c < cols; c++) {
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            COL_NUM_FORMAT_STR, c);
    }
    if (to_console) {
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    for (r = 0; r < rows; r++) {
        count = 0;
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            ROW_NUM_FORMAT_STR, r);
        for (c = 0; c < cols; c++) {
            count += scnprintf(line_buf + count, sizeof(line_buf) - count,
                DATA_FORMAT_STR, cap[r * hw_cols + c]);
        }
        if (to_console) {
            cts_info("%s", line_buf);
        }
        if (cts_test_data_filp) {
            cts_write_file(cts_test_data_filp, line_buf, count);
            cts_write_file(cts_test_data_filp, "\n", 1);
        }
    }

    if (to_console) {
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }
#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
}

int prepare_compensate_cap_test(struct cts_device *cts_dev)
{
    int work_mode = CTS_FIRMWARE_WORK_MODE_NORMAL;

    if (cts_dev->hwdata->support_cascade)
        work_mode = CTS_FIRMWARE_KRANG_WORK_MODE_FINGER_DBG;

    return _prepare_test(cts_dev, INT_DATA_TYPE_CNEGDATA,
            INT_DATA_METHOD_POLLING, work_mode,
            CTS_TEST_NONE_OPEN_SHORT, false);
}

int cts_test_compensate_cap(struct cts_device *cts_dev,
    struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    u8 *cap = NULL;
    int ret = 0;
    u8 hw_rows, hw_cols;
    ktime_t start_time, end_time, delta_time;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Compensate cap test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    if (cts_dev->hwdata->support_cascade) {
        hw_rows = cts_dev->fwdata.panel_info.fs_rows;
        hw_cols = cts_dev->fwdata.panel_info.fs_cols;
    } else {
        hw_rows = cts_dev->hwdata->num_row;
        hw_cols = cts_dev->hwdata->num_col;
    }

    num_nodes = hw_rows * hw_cols;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
            !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    cts_info("Compensate cap test, flags: 0x%08x num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d, num_nodes:%d",
         param->flags, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size, num_nodes);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        cap = (u8 *) param->test_data_buf;
        if (cap == NULL) {
            cts_err("user memory is null, remove flag "
                "\"CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE\" and try again");
            ret = -ENOMEM;
            goto show_test_result;
        }
    } else {
        cap = (u8 *) kzalloc(num_nodes, GFP_KERNEL);
        if (cap == NULL) {
            cts_err("Alloc mem for compensate cap failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    cts_lock_device(cts_dev);

    ret = prepare_compensate_cap_test(cts_dev);
    if (ret) {
        cts_err("Prepare test failed %d", ret);
        goto free_mem;
    }

    /* drop 1 frame so that the rawdata will be more stable*/
    cts_tcs_drop_frames(cts_dev, 1);

    ret = cts_tcs_polling_test_data(cts_dev, cap, num_nodes);
    if (ret)
        goto free_mem;

    if (dump_test_data_to_user)
        *param->test_data_size = num_nodes;

    if (dump_test_data_to_file) {
        ret = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (ret) {
            cts_err("Start dump test data to file failed %d", ret);
        }
    }

    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_comp_cap(cts_dev, cap, dump_test_data_to_console);
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    if (driver_validate_data) {
        ret = validate_comp_cap(cts_dev, "Compensate-Cap", cap, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
    }

free_mem:
    cts_unlock_device(cts_dev);

    if (!dump_test_data_to_user && cap) {
        kfree(cap);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Compensate-Cap test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Compensate-Cap test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Compensate-Cap test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}

int cts_init_test_vals(struct cts_device *cts_dev,
    const struct cts_test_val *test_val)
{
    u32 hwid = cts_dev->hwdata->hwid;
    int i;

    cts_info("Init test data, hwid: %06x", hwid);

    for (i = 0; i < ARRAY_SIZE(cts_test_val); i++) {
        if (hwid == cts_test_val[i].hwid) {
            cts_dev->test_vals = &cts_test_val[i];
            return 0;
        }
    }

    return -EINVAL;
}
