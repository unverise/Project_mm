#define LOG_TAG         "Tool"
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_firmware.h"
#include "cts_test.h"
#include "cts_tcs.h"
#include "cts_transfer.h"

#ifdef CONFIG_CTS_LEGACY_TOOL

#define CTS_TOOL_IOCTL_TCS_RW_BUF_MAX_SIZ     1024
#define CTS_TOOL_IOCTL_TCS_RW_OPFLAG_READ     1
#define CTS_TOOL_IOCTL_TCS_RW_OPFLAG_WRITE    2

#pragma pack(1)
/** Tool command structure */
struct cts_tool_cmd {
    u8 cmd;
    u8 flag;
    u8 circle;
    u8 times;
    u8 retry;
    u32 data_len;
    u8 addr_len;
    u32 addr;
    u8 tcs[2];
    u8 data[PAGE_SIZE];
};
struct cts_ioctl_tcs_rw_data {
    u32 opflags;
    u32 txlen;
    u32 rxlen;
    u8 __user *txbuf;
    u8 __user *rxbuf;
};
#pragma pack()

#define CTS_TOOL_CMD_HEADER_LENGTH            (16)

enum cts_tool_cmd_code {
    CTS_TOOL_CMD_GET_PANEL_PARAM = 0,
    CTS_TOOL_CMD_GET_DOWNLOAD_STATUS = 2,
    CTS_TOOL_CMD_GET_RAW_DATA = 4,
    CTS_TOOL_CMD_GET_DIFF_DATA = 6,
    CTS_TOOL_CMD_READ_HOSTCOMM = 12,
    CTS_TOOL_CMD_READ_ADC_STATUS = 14,
    CTS_TOOL_CMD_READ_GESTURE_INFO = 16,
    CTS_TOOL_CMD_READ_HOSTCOMM_MULTIBYTE = 18,
    CTS_TOOL_CMD_READ_PROGRAM_MODE_MULTIBYTE = 20,
    CTS_TOOL_CMD_READ_ICTYPE = 22,
    CTS_TOOL_CMD_I2C_DIRECT_READ = 24,
    CTS_TOOL_CMD_GET_DRIVER_INFO = 26,
    CTS_TOOL_CMD_TCS_READ_HW_CMD = 28,
    CTS_TOOL_CMD_TCS_READ_DDI_CMD = 30,
    CTS_TOOL_CMD_TCS_READ_FW_CMD = 32,
    CTS_TOOL_CMD_GET_BASE_DATA = 34,
    CTS_TOOL_CMD_GET_INT_DATAS = 36,

    CTS_TOOL_CMD_UPDATE_PANEL_PARAM_IN_SRAM = 1,
    CTS_TOOL_CMD_DOWNLOAD_FIRMWARE_WITH_FILENAME = 3,
    CTS_TOOL_CMD_DOWNLOAD_FIRMWARE = 5,
    CTS_TOOL_CMD_WRITE_HOSTCOMM = 11,
    CTS_TOOL_CMD_WRITE_HOSTCOMM_MULTIBYTE = 15,
    CTS_TOOL_CMD_WRITE_PROGRAM_MODE_MULTIBYTE = 17,
    CTS_TOOL_CMD_I2C_DIRECT_WRITE = 19,
    CTS_TOOL_CMD_TCS_WRITE_HW_CMD = 21,
    CTS_TOOL_CMD_TCS_WRITE_DDI_CMD = 23,
    CTS_TOOL_CMD_TCS_WRITE_FW_CMD = 25,
    CTS_TOOL_CMD_TCS_ENABLE_GET_RAWDATA_CMD = 27,
    CTS_TOOL_CMD_TCS_DISABLE_GET_RAWDATA_CMD = 29,
    CTS_TOOL_CMD_SET_INT_DATA_TYPE_AND_METHOD = 31,
    CTS_TOOL_CMD_SET_INT_DATA_LEN = 37,
    CTS_TOOL_CMD_ENABLE_IRQ = 39,

};

struct cts_test_ioctl_data {
    __u32 ntests;
    struct cts_test_param __user *tests;
};


#define CTS_TOOL_IOCTL_GET_DRIVER_VERSION   _IOR('C', 0x00, u32 *)
#define CTS_TOOL_IOCTL_GET_DEVICE_TYPE      _IOR('C', 0x01, u32 *)
#define CTS_TOOL_IOCTL_GET_FW_VERSION       _IOR('C', 0x02, u16 *)
#define CTS_TOOL_IOCTL_GET_RESOLUTION       _IOR('C', 0x03, u32 *) /* X in LSW, Y in MSW */
#define CTS_TOOL_IOCTL_GET_ROW_COL          _IOR('C', 0x04, u16 *) /* row in LSW, col in MSW */
#define CTS_TOOL_IOCTL_GET_MODULE_ID        _IOWR('C', 0x05, u32 *)

#define CTS_TOOL_IOCTL_TEST                 _IOWR('C', 0x10, struct cts_test_ioctl_data *)
#define CTS_TOOL_IOCTL_RDWR_REG             _IOWR('C', 0x20, struct cts_rdwr_reg_ioctl_data *)
#define CTS_TOOL_IOCTL_UPGRADE_FW           _IOWR('C', 0x21, struct cts_upgrade_fw_ioctl_data *)

#define CTS_TOOL_IOCTL_TCS_RW               _IOWR('C', 0x30, struct cts_ioctl_tcs_rw_data *)

#define CTS_DRIVER_VERSION_CODE ((CFG_CTS_DRIVER_MAJOR_VERSION << 16) | \
        (CFG_CTS_DRIVER_MINOR_VERSION << 8) | \
        (CFG_CTS_DRIVER_PATCH_VERSION << 0))

static struct cts_tool_cmd cts_tool_cmd;

static int cts_tool_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
    file->private_data = pde_data(inode);
#else
    file->private_data = PDE_DATA(inode);
#endif
    return 0;
}

static ssize_t cts_tool_read(struct file *file,
        char __user *buffer, size_t count, loff_t *ppos)
{
#define RAWDATA_BUFFER_SIZE(cts_dev) \
    (cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * 2)
    struct chipone_ts_data *cts_data;
    struct cts_tool_cmd *cmd;
    struct cts_device *cts_dev;
    int ret = 0;

    cts_data = (struct chipone_ts_data *)file->private_data;
    if (cts_data == NULL) {
        cts_err("Read with private_data = NULL");
        return -EIO;
    }

    cmd = &cts_tool_cmd;
    cts_dev = &cts_data->cts_dev;
    cts_lock_device(cts_dev);

    cts_dbg("read: cmd:%d, addr:0x%06x, tcm:0x%04x, datalen:%d",
            cmd->cmd, cmd->addr, cmd->tcs, cmd->data_len);

    switch (cmd->cmd) {
    case CTS_TOOL_CMD_TCS_READ_HW_CMD:
    case CTS_TOOL_CMD_TCS_READ_DDI_CMD:
    case CTS_TOOL_CMD_TCS_READ_FW_CMD:
        cts_tcs_read_data(cts_dev, cmd->addr, cmd->data, cmd->data_len);
        break;
    case CTS_TOOL_CMD_GET_PANEL_PARAM:
        cts_info("Get panel param len: %u", cmd->data_len);
        break;
    case CTS_TOOL_CMD_GET_DOWNLOAD_STATUS:
        cmd->data[0] = 100;     //TODO: return the real progress
        cts_info("Get update status = %hhu", cmd->data[0]);
        break;

    case CTS_TOOL_CMD_GET_RAW_DATA:
    case CTS_TOOL_CMD_GET_DIFF_DATA:
    case CTS_TOOL_CMD_GET_BASE_DATA:
        if (cmd->cmd == CTS_TOOL_CMD_GET_RAW_DATA) {
            ret = cts_tcs_top_get_rawdata(cts_dev, cmd->data,
                    RAWDATA_BUFFER_SIZE(cts_dev));
        } else if (cmd->cmd == CTS_TOOL_CMD_GET_DIFF_DATA) {
            ret = cts_tcs_top_get_manual_diff(cts_dev, cmd->data,
                    RAWDATA_BUFFER_SIZE(cts_dev));
        } else if (cmd->cmd == CTS_TOOL_CMD_GET_BASE_DATA) {
            ret = cts_tcs_top_get_basedata(cts_dev, cmd->data,
                    RAWDATA_BUFFER_SIZE(cts_dev));
        }
        if (ret) {
            cts_err("Get %s data failed %d",
                cmd->cmd == CTS_TOOL_CMD_GET_RAW_DATA ? "raw" :
                cmd->cmd ==
                CTS_TOOL_CMD_GET_DIFF_DATA ? "diff" : "base",
                ret);
        }
        break;
    case CTS_TOOL_CMD_GET_INT_DATAS:
        memcpy(cmd->data, cts_dev->pdata->int_data, cts_dev->fwdata.int_data_size);
        break;
    case CTS_TOOL_CMD_READ_HOSTCOMM:
        ret = cts_read_hw_reg(cts_dev, cmd->addr, cmd->data,
                cmd->data_len, false);
        if (ret) {
            cts_err("Read firmware reg addr 0x%04x failed %d",
                cmd->addr, ret);
        } else {
            cts_dbg("Read firmware reg addr 0x%04x, val=0x%02x",
                cmd->addr, cmd->data[0]);
        }
        break;

#ifdef CFG_CTS_GESTURE
    case CTS_TOOL_CMD_READ_GESTURE_INFO:
        ret = cts_get_gesture_info(cts_dev, cmd->data);
        if (ret)
            cts_err("Get gesture info failed %d", ret);
        break;
#endif /* CFG_CTS_GESTURE */

    case CTS_TOOL_CMD_READ_HOSTCOMM_MULTIBYTE:
        cmd->data_len = min((size_t)cmd->data_len, sizeof(cmd->data));
        ret = cts_read_hw_reg(cts_dev, cmd->addr, cmd->data, cmd->data_len,
                false);
        if (ret) {
            cts_err("Read firmware reg addr 0x%04x len %u failed %d",
                    cmd->addr, cmd->data_len, ret);
        } else {
            cts_dbg("Read firmware reg addr 0x%04x len %u",
                cmd->addr, cmd->data_len);
        }
        break;

    case CTS_TOOL_CMD_READ_PROGRAM_MODE_MULTIBYTE:
        cts_dbg("Read under program mode addr 0x%06x len %u",
            cmd->flag, cmd->addr,
            cmd->data_len);
        ret = cts_enter_program_mode(cts_dev);
        if (ret) {
            cts_err("Enter program mode failed %d", ret);
            break;
        }

        ret = cts_read_sram(&cts_data->cts_dev, cmd->addr, cmd->data, cmd->data_len,
                false);
        if (ret) {
            cts_err("Read under program mode I2C xfer failed %d",
                ret);
        }

        ret = cts_enter_normal_mode(cts_dev);
        if (ret) {
            cts_err("Enter normal mode failed %d", ret);
            break;
        }
        break;

    case CTS_TOOL_CMD_READ_ICTYPE:
        cts_info("Get IC type");
        break;

    case CTS_TOOL_CMD_I2C_DIRECT_READ:
        if(cts_dev->bus_type != CTS_BUS_I2C) {
            cts_warn("bus type is not i2c, bus_type:%d\n", cts_dev->bus_type);
            return 0;
        }
        cts_dbg("Direct read from addr 0x%06x size %u",
                cmd->addr, cmd->data_len);

        ret = cts_read(cts_dev, cmd->addr, cmd->data, cmd->data_len,
                false);
        if(ret)
            cts_err("cts_read failed, ret: %d\n", ret);

        break;
    case CTS_TOOL_CMD_GET_DRIVER_INFO:
        break;

    default:
        cts_warn("Read unknown command %u", cmd->cmd);
        ret = -EINVAL;
        break;
    }

    cts_unlock_device(cts_dev);

    if (ret == 0) {
        if (cmd->cmd == CTS_TOOL_CMD_I2C_DIRECT_READ) {
            ret = copy_to_user(buffer + CTS_TOOL_CMD_HEADER_LENGTH,
                    cmd->data, cmd->data_len);
        } else {
            ret = copy_to_user(buffer, cmd->data, cmd->data_len);
        }
        if (ret) {
            cts_err("Copy data to user buffer failed %d", ret);
            return 0;
        }

        return cmd->data_len;
    }

    return 0;
}

static ssize_t cts_tool_write(struct file *file,
        const char __user *buffer, size_t count, loff_t *ppos)
{
    struct chipone_ts_data *cts_data;
    struct cts_device *cts_dev;
    struct cts_tool_cmd *cmd;
    int enable_irq = 0;
    int ret = 0;

    if (count < CTS_TOOL_CMD_HEADER_LENGTH || count > PAGE_SIZE) {
        cts_err("Write len %zu invalid", count);
        return -EFAULT;
    }

    cts_data = (struct chipone_ts_data *)file->private_data;
    if (cts_data == NULL) {
        cts_err("Write with private_data = NULL");
        return -EIO;
    }

    cmd = &cts_tool_cmd;
    ret = copy_from_user(cmd, buffer, CTS_TOOL_CMD_HEADER_LENGTH);
    cts_dbg("write: cmd:%d, addr:0x%06x, tcmd:0x%04x data:0x%02x, datalen:%d",
            cmd->cmd, cmd->addr, cmd->tcs, cmd->data[0], cmd->data_len);

    if (ret) {
        cts_err("Copy command header from user buffer failed %d", ret);
        return -EIO;
    } else
        ret = CTS_TOOL_CMD_HEADER_LENGTH;

    if (cmd->data_len > PAGE_SIZE) {
        cts_err("Write with invalid count %d", cmd->data_len);
        return -EIO;
    }

    if (cmd->cmd & BIT(0)) {
        if (cmd->data_len) {
            ret = copy_from_user(cmd->data, buffer + CTS_TOOL_CMD_HEADER_LENGTH,
                    cmd->data_len);
            if (ret) {
                cts_err("Copy command payload from user buffer len %u failed %d",
                        cmd->data_len, ret);
                return -EIO;
            }
        }
    } else {
        /* Prepare for read command */
        cts_dbg("Write read command(%d) header, prepare read size: %d",
                cmd->cmd, cmd->data_len);
        return CTS_TOOL_CMD_HEADER_LENGTH + cmd->data_len;
    }

    cts_dev = &cts_data->cts_dev;
    cts_lock_device(cts_dev);

    switch (cmd->cmd) {
    case CTS_TOOL_CMD_TCS_WRITE_DDI_CMD:
        cts_tcs_write_ddi_reg(cts_dev, cmd->addr, cmd->data, cmd->data_len);
        break;
    case CTS_TOOL_CMD_TCS_WRITE_HW_CMD:
    case CTS_TOOL_CMD_TCS_WRITE_FW_CMD:
        cts_tcs_write_reg(cts_dev, cmd->addr, cmd->data, cmd->data_len);
        break;
    case CTS_TOOL_CMD_TCS_ENABLE_GET_RAWDATA_CMD:
    case CTS_TOOL_CMD_TCS_DISABLE_GET_RAWDATA_CMD:
        cts_info("Do nothing");
        break;
    case CTS_TOOL_CMD_SET_INT_DATA_TYPE_AND_METHOD:
        cts_set_int_data_types(cts_dev, cmd->data[0]);
        cts_set_int_data_method(cts_dev, cmd->data[2]);
        break;
    case CTS_TOOL_CMD_UPDATE_PANEL_PARAM_IN_SRAM:
        cts_info("Write panel param len %u data\n", cmd->data_len);
        cts_warn("cmd not handled, need to be implemented\n");
        cts_reset_device(cts_dev);
        mdelay(100);
        break;
    case CTS_TOOL_CMD_SET_INT_DATA_LEN:
        cts_dev->fwdata.int_data_size = (size_t)cmd->data_len;
        break;
    case CTS_TOOL_CMD_ENABLE_IRQ:
        enable_irq = cmd->data[0];
        if (enable_irq)
            cts_plat_enable_irq(cts_dev->pdata);
        else
            cts_plat_disable_irq(cts_dev->pdata);
        break;

    case CTS_TOOL_CMD_DOWNLOAD_FIRMWARE_WITH_FILENAME:
        cts_info("Write firmware path: '%.*s'", cmd->data_len, cmd->data);

        memcpy(cts_data->cts_dev.fwdata.cts_fw->name, cmd->data, cmd->data_len);
        cts_data->cts_dev.fwdata.cts_fw->name[cmd->data_len] = '\0';
        break;

    case CTS_TOOL_CMD_DOWNLOAD_FIRMWARE:
        cts_info("Start download firmware path: '%s'",
            cts_data->cts_dev.fwdata.cts_fw->name);

        ret = cts_stop_device(cts_dev);
        if (ret) {
            cts_err("Stop device failed %d", ret);
            break;
        }
/**
 *TODO:  Use async mode such as thread, otherwise,
 *host can not get update status.
 */
        ret = cts_update_firmware_from_file(cts_dev);
        if (ret)
            cts_err("Updata firmware failed %d", ret);

        ret = cts_start_device(cts_dev);
        if (ret) {
            cts_err("Start device failed %d", ret);
            break;
        }
        break;

    case CTS_TOOL_CMD_WRITE_HOSTCOMM:
        cts_warn("hostcomm is not supported any more\n");
        break;

    case CTS_TOOL_CMD_WRITE_HOSTCOMM_MULTIBYTE:
        cts_warn("hostcomm is not supported any more\n");
        break;

    case CTS_TOOL_CMD_WRITE_PROGRAM_MODE_MULTIBYTE:
        cts_dbg("Write to addr 0x%06x size %u under program mode",
            cmd->addr, cmd->data_len);
        ret = cts_enter_program_mode(cts_dev);
        if (ret) {
            cts_err("Enter program mode failed %d", ret);
            break;
        }

        ret = cts_write_sram(cts_dev, cmd->addr, cmd->data, cmd->data_len, false);
        if (ret)
            cts_err("Write program mode multibyte failed %d", ret);

        ret = cts_enter_normal_mode(cts_dev);
        if (ret) {
            cts_err("Enter normal mode failed %d", ret);
            break;
        }

        break;

    case CTS_TOOL_CMD_I2C_DIRECT_WRITE:
        if(cts_dev->bus_type != CTS_BUS_I2C) {
            cts_warn("bus type is not i2c, bus_type:%d\n", cts_dev->bus_type);
            return 0;
        }

        cts_dbg("Direct write to addr 0x%06x size %u",
                cmd->addr, cmd->data_len);

        ret = cts_write_sram(cts_dev, cmd->addr, cmd->data, cmd->data_len, false);
        if (ret)
            cts_err("cts_write_sram failed %d", ret);
        break;
    default:
        cts_warn("Write unknown command %u", cmd->cmd);
        ret = -EINVAL;
        break;
    }

    cts_unlock_device(cts_dev);

    return ret ? 0 : cmd->data_len + CTS_TOOL_CMD_HEADER_LENGTH;
}

static long cts_ioctl_tcs_rw(struct cts_device *cts_dev,
        struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct cts_ioctl_tcs_rw_data rwdata;
    u8 *rxbuf = NULL;

    if (copy_from_user(&rwdata,
            (struct cts_ioctl_tcs_rw_data __user *)arg,
            sizeof(rwdata))) {
        cts_err("Copy ioctl tcs rw arg to kernel failed");
        return -EFAULT;
    }

    if ((rwdata.opflags != CTS_TOOL_IOCTL_TCS_RW_OPFLAG_READ) &&
        (rwdata.opflags != CTS_TOOL_IOCTL_TCS_RW_OPFLAG_WRITE))
    {
        cts_err("ioctl tcs rw with invalid opflags %u", rwdata.opflags);
        return -EINVAL;
    }

    if (rwdata.txlen > CTS_TOOL_IOCTL_TCS_RW_BUF_MAX_SIZ) {
        cts_err("Send too long: %d", rwdata.txlen);
        return -EINVAL;
    }

    if (rwdata.rxlen > CTS_TOOL_IOCTL_TCS_RW_BUF_MAX_SIZ) {
        cts_err("Recv too long: %d", rwdata.rxlen);
        return -EINVAL;
    }

    if (!rwdata.txbuf || !rwdata.rxbuf) {
        cts_err("Txbuf of Rxbuf is NULL!");
        return -EINVAL;
    }

    cts_lock_device(cts_dev);
    if (cts_dev->bus_type == CTS_BUS_SPI) {
        ret = copy_from_user(cts_dev->pdata->spi_tx_buf, rwdata.txbuf, rwdata.txlen);
        if (ret)
            cts_err("copy from user failed, bus_type:%d\n", cts_dev->bus_type);
        ret = cts_tcs_spi_xtrans(cts_dev, cts_dev->pdata->spi_tx_buf, rwdata.txlen,
            cts_dev->pdata->spi_rx_buf, rwdata.rxlen);
        rxbuf = cts_dev->pdata->spi_rx_buf;
    } else if (cts_dev->bus_type == CTS_BUS_I2C) {
        ret = copy_from_user(cts_dev->pdata->i2c_tx_buf, rwdata.txbuf, rwdata.txlen);
        if (ret)
            cts_err("copy from user failed, bus_type:%d\n", cts_dev->bus_type);
        ret = cts_plat_i2c_transfer(cts_dev->pdata, CTS_I2C_NORM_ADDR_DEFAULT,
                cts_dev->pdata->i2c_tx_buf, rwdata.txlen, cts_dev->pdata->i2c_rx_buf,
                rwdata.rxlen, 3, 10);
        rxbuf = cts_dev->pdata->i2c_rx_buf;
    }
    cts_unlock_device(cts_dev);
    if (ret < 0) {
        cts_err("Trans failed %d", ret);
        goto out;
    }

    ret = copy_to_user(rwdata.rxbuf, rxbuf, rwdata.rxlen);
    if (ret < 0) {
        cts_err("Copy to user failed %d", ret);
        goto out;
    }

    ret = 0;

out:
    return ret;
}

void free_params(struct cts_test_param *params, int num_param)
{
    int i;

    for (i = 0; i < num_param; i++) {
        if (params[i].test_data_buf) {
            vfree(params[i].test_data_buf);
            params[i].test_data_buf = NULL;
        }
        if (params[i].test_data_size) {
            kfree(params[i].test_data_size);
            params[i].test_data_size = NULL;
        }
        if (params[i].max) {
            kfree(params[i].max);
            params[i].max = NULL;
        }
        if (params[i].min) {
            kfree(params[i].min);
            params[i].min = NULL;
        }
        if (params[i].num_invalid_node) {
            kfree(params[i].invalid_nodes);
            params[i].invalid_nodes = NULL;
            params[i].num_invalid_node = 0;
        }
        if (params[i].test_result) {
            kfree(params[i].test_result);
            params[i].test_result = NULL;
        }
        if (params[i].elapsed_time_ms) {
            kfree(params[i].elapsed_time_ms);
            params[i].elapsed_time_ms = NULL;
        }
        if (params[i].test_data_filepath) {
            kfree(params[i].test_data_filepath);
            params[i].test_data_filepath = NULL;
        }
        if (params[i].driver_log_buf) {
            vfree(params[i].driver_log_buf);
            params[i].driver_log_buf = NULL;
        }
        if (params[i].driver_log_filepath) {
            kfree(params[i].driver_log_filepath);
            params[i].driver_log_filepath = NULL;
        }
    }
}

int test_param_prepare(struct cts_test_param **params,
    struct cts_test_param __user *user_params, int test_case_count,
    int num_nodes)
{
    struct cts_test_param *test_params = NULL;
    int threshold_cnt;
    int data_buf_len = 0;
    int i = 0;

    if (test_case_count > CTS_TEST_ITEM_MAX) {
        cts_err("ioctl test with too many tests %u", test_case_count);
        return -EINVAL;
    }

    test_params = kzalloc(test_case_count * sizeof(*test_params), GFP_KERNEL);
    if (test_params == NULL) {
        cts_err("Alloc test params memory failed");
        return -ENOMEM;
    }

    if (copy_from_user(test_params, user_params,
            test_case_count * sizeof(*test_params))) {
        cts_err("%s:%d, copy test data from userspace failed");
        goto copy_user_data_err;
    }

    for (i = 0; i < test_case_count; i++) {
        cts_info("ioctl test item %d: %d(%s) flags: %08x sample_frames: %d",
            i, test_params[i].test_item, cts_test_item_str(test_params[i].test_item),
            test_params[i].flags, test_params[i].sample_frames);

        test_params[i].invalid_nodes = NULL;
        test_params[i].min = NULL;
        test_params[i].max = NULL;
        test_params[i].test_data_buf = NULL;
        test_params[i].test_data_size = NULL;
        test_params[i].test_data_filepath = NULL;
        test_params[i].driver_log_buf = NULL;
        test_params[i].driver_log_wr_size = NULL;
        test_params[i].driver_log_filepath = NULL;
        data_buf_len = 0;

        switch (test_params[i].test_item) {
            case CTS_TEST_RESET_PIN:
            case CTS_TEST_INT_PIN:
                test_params[i].test_result = kzalloc(sizeof(int), GFP_KERNEL);
                test_params[i].elapsed_time_ms = kzalloc(sizeof(s64), GFP_KERNEL);
                if (!test_params[i].test_result || !test_params[i].elapsed_time_ms)
                    goto alloc_mem_err;

                if (test_params[i].flags & CTS_TEST_FLAG_DRIVER_LOG_TO_USERSPACE) {
                    test_params[i].driver_log_buf = vzalloc(test_params[i].driver_log_buf_size);
                    test_params[i].driver_log_wr_size = kzalloc(sizeof(int), GFP_KERNEL);
                    if (!test_params[i].driver_log_buf || !test_params[i].driver_log_wr_size)
                        goto alloc_mem_err;
                }
                continue;

            case CTS_TEST_RAWDATA:
                data_buf_len = 2 * num_nodes * test_params[i].sample_frames;
                break;
            case CTS_TEST_NOISE:
                data_buf_len = 2 * num_nodes * (test_params[i].sample_frames + 3);
                break;
            case CTS_TEST_OPEN:
                data_buf_len = 2 * num_nodes;
                break;
            case CTS_TEST_SHORT:
                data_buf_len = 2 * num_nodes * 10;
                break;
            case CTS_TEST_COMPENSATE_CAP:
                data_buf_len = num_nodes;
                break;
            default:
                cts_warn("item(%d) not supported\n", test_params[i].test_item);
                continue;
        }

        if (data_buf_len > 2 * num_nodes * SAMPLE_FRAMES_MAX) {
            cts_warn("sample too much frames, frames count:%d",
                test_params[i].sample_frames);
            goto alloc_mem_err;
        }
        test_params[i].test_data_buf = vzalloc(data_buf_len);
        test_params[i].test_data_buf_size = data_buf_len;
        test_params[i].test_data_size = kzalloc(sizeof(int), GFP_KERNEL);
        if (!test_params[i].test_data_buf || !test_params[i].test_data_size)
            goto alloc_mem_err;

        if (test_params[i].flags & CTS_TEST_FLAG_VALIDATE_DATA) {
            threshold_cnt = test_params[i].flags & CTS_TEST_FLAG_VALIDATE_PER_NODE ?
                    num_nodes : 1;
            if (test_params[i].flags & CTS_TEST_FLAG_VALIDATE_MAX) {
                test_params[i].max = kzalloc(sizeof(int) * threshold_cnt, GFP_KERNEL);
                if (!test_params[i].max)
                    goto alloc_mem_err;
                if (copy_from_user(test_params[i].max, user_params[i].max,
                    threshold_cnt * sizeof(int)))
                    goto copy_param_err;
                cts_info("%s, max[0]:%d", __func__, test_params[i].max[0]);
            }

            if (test_params[i].flags & CTS_TEST_FLAG_VALIDATE_MIN) {
                test_params[i].min = kzalloc(sizeof(int) * threshold_cnt, GFP_KERNEL);
                if (!test_params[i].min)
                    goto alloc_mem_err;
                if (copy_from_user(test_params[i].min, user_params[i].min,
                    threshold_cnt * sizeof(int)))
                    goto copy_param_err;
                cts_info("%s, min[0]:%d", __func__, test_params[i].min[0]);
            }

            if (test_params[i].flags & CTS_TEST_FLAG_VALIDATE_SKIP_INVALID_NODE &&
                    test_params[i].num_invalid_node > 0 &&
                    test_params[i].num_invalid_node < num_nodes) {
                test_params[i].invalid_nodes =
                    kzalloc(sizeof(int) * test_params[i].num_invalid_node, GFP_KERNEL);
                if (!test_params[i].invalid_nodes)
                    goto alloc_mem_err;
                if (copy_from_user(test_params[i].invalid_nodes,
                        user_params[i].invalid_nodes, threshold_cnt * sizeof(int)))
                    goto copy_param_err;
            }
        }

        test_params[i].test_result = kzalloc(sizeof(int), GFP_KERNEL);
        test_params[i].elapsed_time_ms = kzalloc(sizeof(s64), GFP_KERNEL);
        if (!test_params[i].test_result || !test_params[i].elapsed_time_ms)
            goto alloc_mem_err;

        if (test_params[i].flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE) {
            test_params[i].test_data_filepath =
                    kzalloc(sizeof(char) * PATH_MAX, GFP_KERNEL);
            if (!test_params[i].test_data_filepath)
                goto alloc_mem_err;
            if (copy_from_user(test_params[i].test_data_filepath,
                    user_params[i].test_data_filepath, PATH_MAX))
                    goto copy_param_err;
            cts_info("test data file path:%s", test_params[i].test_data_filepath);
        }

        if (test_params[i].flags & CTS_TEST_FLAG_DRIVER_LOG_TO_USERSPACE) {
            test_params[i].driver_log_buf = vzalloc(test_params[i].driver_log_buf_size);
            test_params[i].driver_log_wr_size = kzalloc(sizeof(int), GFP_KERNEL);
            if (!test_params[i].driver_log_buf || !test_params[i].driver_log_wr_size)
                goto alloc_mem_err;
        }

        if (test_params[i].flags & CTS_TEST_FLAG_DRIVER_LOG_TO_FILE) {
            test_params[i].driver_log_filepath =
                    kzalloc(sizeof(char) * PATH_MAX, GFP_KERNEL);
            if (!test_params[i].driver_log_filepath)
                goto alloc_mem_err;
            if (copy_from_user(test_params[i].driver_log_filepath,
                    user_params[i].driver_log_filepath, PATH_MAX))
                    goto copy_param_err;
            cts_info("driver log file path:%s", test_params[i].driver_log_filepath);
        }
    }
    *params = test_params;

    return 0;

copy_param_err:
    cts_err("copy parameters from user side failed");
alloc_mem_err:
    free_params(test_params, i);
copy_user_data_err:
    kfree(test_params);
    params = NULL;
    return -EFAULT;
}

int do_one_test(struct cts_device *cts_dev, struct cts_test_param *params)
{
    int ret = 0;

    switch (params->test_item) {
    case CTS_TEST_RESET_PIN:
        *params->test_result =
            cts_test_reset_pin(cts_dev);
        break;
    case CTS_TEST_INT_PIN:
        *params->test_result =
            cts_test_int_pin(cts_dev);
        break;
    case CTS_TEST_RAWDATA:
        *params->test_result =
            cts_test_rawdata(cts_dev, params);
        break;
    case CTS_TEST_NOISE:
        *params->test_result =
            cts_test_noise(cts_dev, params);
        break;
    case CTS_TEST_OPEN:
        *params->test_result =
            cts_test_open(cts_dev, params);
        break;
    case CTS_TEST_SHORT:
        *params->test_result =
            cts_test_short(cts_dev, params);
        break;
    case CTS_TEST_COMPENSATE_CAP:
        *params->test_result =
            cts_test_compensate_cap(cts_dev, params);
        break;
    default:
        ret = -ENOTSUPP;
        cts_err("Un-supported test item:%d", params->test_item);
        break;
    }
    return ret;
}

int _do_tests(struct cts_device *cts_dev, struct cts_test_param *params,
    struct cts_test_param __user *user_params, int test_case_count)
{
    int i, ret = 0;
    int redirect_log;
    int log_to_user;
    int file_append;

    if(!params) {
        cts_err("%s:%d, wrong input params!", __func__, __LINE__);
        return -1;
    }

    for(i = 0; i < test_case_count; i++) {
        log_to_user = params[i].flags & CTS_TEST_FLAG_DRIVER_LOG_TO_USERSPACE;
        redirect_log = log_to_user ||
                params[i].flags & CTS_TEST_FLAG_DRIVER_LOG_TO_FILE;
        file_append = params[i].flags & CTS_TEST_FLAG_DRIVER_LOG_TO_FILE_APPEND;

        if (redirect_log) {
            ret = cts_start_driver_log_redirect(
                params[i].driver_log_filepath, file_append,
                params[i].driver_log_buf, params[i].driver_log_buf_size,
                params[i].driver_log_level);
            if (ret) {
                cts_err("Start driver log redirect failed %d, test item:%d",
                    ret, params[i].test_item);
                continue;
            }
        }

        ret = do_one_test(cts_dev, &params[i]);
        if (ret)
            break;

        if (redirect_log)
            cts_stop_driver_log_redirect();

        if (log_to_user) {
            *params[i].driver_log_wr_size = cts_get_driver_log_redirect_size();
            if (params[i].driver_log_wr_size > 0) {
                cts_info("Copy driver log to user, size: %d", params[i].driver_log_wr_size);
                if (copy_to_user(user_params[i].driver_log_buf, params[i].driver_log_buf,
                        *params[i].driver_log_wr_size)) {
                    cts_err("Copy driver log to user failed");
                    params[i].driver_log_wr_size = 0;
                }
            }
        }

        if (*params[i].test_result &&
            params[i].flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED)
            break;
    }

    return ret;
}

int do_product_test(struct cts_device *cts_dev, struct cts_test_param *params,
    struct cts_test_param __user *user_params, int test_case_count)
{
    int ret = 0;

    ret = prepare_product_test(cts_dev);
    if (ret) {
        cts_err("prepare product test failed");
        goto prepare_failed;
    }

    ret = _do_tests(cts_dev, params, user_params, test_case_count);

prepare_failed:
    post_product_test(cts_dev);

    return ret;
}

int copy_test_info_to_user(struct cts_test_param *params,
    struct cts_test_param __user *user_params, int test_case_count)
{
    int i, ret = 0;

    for (i = 0; i < test_case_count; i++) {
        if (params[i].flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE &&
            *params[i].test_data_size > 0) {
            ret = copy_to_user(user_params[i].test_data_buf,
                    params[i].test_data_buf, *params[i].test_data_size);
            if (ret)
                continue;
            put_user(*params[i].test_data_size, user_params[i].test_data_size);
        }

        if (params[i].flags & CTS_TEST_FLAG_DRIVER_LOG_TO_USERSPACE &&
            *params[i].driver_log_wr_size > 0) {
            ret = copy_to_user(user_params[i].driver_log_buf,
                    params[i].driver_log_buf, *params[i].driver_log_wr_size);
            if (ret)
                continue;
            put_user(*params[i].driver_log_wr_size,
                user_params[i].driver_log_wr_size);
        }

        put_user(*params[i].test_result, user_params[i].test_result);
    }

    return ret;
}

static int cts_ioctl_test(struct cts_device *cts_dev, void __user *arg)
{
    struct cts_test_ioctl_data ioctl_data;
    struct cts_test_param *test_params = NULL;
    int test_case_count;
    u32 num_nodes = 0;
    u8 rows, cols;
    int ret = 0;

    if (!cts_dev) {
        cts_err("cts_dev is null");
        return -EFAULT;
    }

    if (cts_dev->hwdata->support_cascade) {
        rows = cts_dev->fwdata.panel_info.fs_rows;
        cols = cts_dev->fwdata.panel_info.fs_cols;
    } else {
        rows = cts_dev->fwdata.xy_info.rows;
        cols = cts_dev->fwdata.xy_info.cols;
    }

    num_nodes = rows * cols;

    if (copy_from_user(&ioctl_data, arg, sizeof(ioctl_data))) {
        cts_err("copy test data from userspace failed");
        return -EFAULT;
    }

    test_case_count = ioctl_data.ntests;
    ret = test_param_prepare(&test_params, ioctl_data.tests,
            test_case_count, num_nodes);
    if (ret) {
        cts_err("initialize test params from userspace failed");
        return ret;
    }

    cts_info("ioctl test total %d items", test_case_count);

    ret = do_product_test(cts_dev, test_params, ioctl_data.tests,
            test_case_count);
    if (!ret)
        ret = copy_test_info_to_user(test_params, ioctl_data.tests,
                test_case_count);

    free_params(test_params, test_case_count);
    return ret;
}

static long cts_tool_ioctl(struct file *file, unsigned int cmd,
        unsigned long arg)
{
    struct chipone_ts_data *cts_data;
    struct cts_device *cts_dev;
    unsigned int modid;
    u16 rows_cols;
    u8 rows, cols;

    cts_info("ioctl, cmd=0x%016x, arg=0x%016lx", cmd, arg);

    cts_data = file->private_data;
    if (cts_data == NULL) {
        cts_err("IOCTL with private data = NULL");
        return -EFAULT;
    }

    cts_dev = &cts_data->cts_dev;

    if (cts_dev->hwdata->support_cascade) {
        rows = cts_dev->fwdata.panel_info.fs_rows;
        cols = cts_dev->fwdata.panel_info.fs_cols;
    } else {
        rows = cts_dev->fwdata.xy_info.rows;
        cols = cts_dev->fwdata.xy_info.cols;
    }

    cts_dbg("cmd ver:%x, dev-type:%x, fw_ver:%x, get_row:%x, mod_id:%x, test:%x, tcs:%x",
        CTS_TOOL_IOCTL_GET_DRIVER_VERSION,
        CTS_TOOL_IOCTL_GET_DEVICE_TYPE,
        CTS_TOOL_IOCTL_GET_FW_VERSION,
        CTS_TOOL_IOCTL_GET_ROW_COL,
        CTS_TOOL_IOCTL_GET_MODULE_ID,
        CTS_TOOL_IOCTL_TEST,
        CTS_TOOL_IOCTL_TCS_RW);

    switch (cmd) {
    case CTS_TOOL_IOCTL_GET_DRIVER_VERSION:
        return put_user(CTS_DRIVER_VERSION_CODE,
                (unsigned int __user *)arg);
    case CTS_TOOL_IOCTL_GET_DEVICE_TYPE:
        return put_user(cts_dev->hwdata->hwid,
                (unsigned int __user *)arg);
    case CTS_TOOL_IOCTL_GET_FW_VERSION:
        return put_user(cts_dev->fwdata.version,
                (unsigned short __user *)arg);
    case CTS_TOOL_IOCTL_GET_ROW_COL:
        rows_cols = (rows << 8) | cols;
        return put_user(rows_cols,
                (unsigned short __user *)arg);
    case CTS_TOOL_IOCTL_GET_MODULE_ID:
        modid = cts_dev->fwdata.module_id;
        return put_user(modid,
                (unsigned int __user *)arg);

    case CTS_TOOL_IOCTL_TEST:
        return cts_ioctl_test(cts_dev, (void __user *)arg);

    case CTS_TOOL_IOCTL_TCS_RW:
        return cts_ioctl_tcs_rw(cts_dev, file, cmd, arg);

    default:
        cts_warn("unknown cmd!");
        break;
    }

    return -ENOTSUPP;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static struct file_operations cts_tool_fops = {
    .owner = THIS_MODULE,
    .llseek = no_llseek,
    .open = cts_tool_open,
    .read = cts_tool_read,
    .write = cts_tool_write,
    .unlocked_ioctl = cts_tool_ioctl,
};
#else
static struct proc_ops cts_tool_fops = {
    .proc_lseek = noop_llseek,  
    .proc_open = cts_tool_open,
    .proc_read = cts_tool_read,
    .proc_write = cts_tool_write,
    .proc_ioctl = cts_tool_ioctl,
};
#endif

int cts_tool_init(struct chipone_ts_data *cts_data)
{
    cts_info("Init");

    cts_data->procfs_entry = proc_create_data(CFG_CTS_TOOL_PROC_FILENAME,
            0666, NULL, &cts_tool_fops, cts_data);
    if (cts_data->procfs_entry == NULL) {
        cts_err("Create proc entry failed");
        return -EFAULT;
    }

    return 0;
}

void cts_tool_deinit(struct chipone_ts_data *data)
{
    cts_info("Deinit");

    if (data->procfs_entry)
        remove_proc_entry(CFG_CTS_TOOL_PROC_FILENAME, NULL);
}
#endif /* CONFIG_CTS_LEGACY_TOOL */
