#define LOG_TAG         "Firmware"
#include <linux/path.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/vmalloc.h>
#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sfctrl.h"
#include "cts_strerror.h"
#include "cts_transfer.h"
#include "cts_flash.h"
#include "cts_firmware.h"
#include "cts_krang.h"
#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
#include "cts_builtin_firmware.h"
#define NUM_DRIVER_BUILTIN_FIRMWARE ARRAY_SIZE(cts_driver_builtin_firmwares)
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */


static struct cts_firmware cts_firmwares[] = {
    {
        .name = "ICNL9905.bin",
        .hwid = CTS_DEV_HWID_ICNL9905,
        .fwid = CTS_DEV_FWID_ICNL9905,
        .sram_code_region_size = 64 * 1024,
        .flash_code_region_size = 116 * 1024,
        .sram_fw_start_addr = CTS_SRAM_FW_START_ADDR_DEFAULT,
        .sram_fw_crc_addr = 0x12FF0,
        .ddi_param_addr = 0x01000,
        .ddi_init_en_addr = 0x0000,
        .is_split_bins = true,
        .bins = {
            {
                .flash_fw_addr_addr = 0x00020,
                .flash_fw_info_addr = 0x00028,
                .flash_fw_addr = 0x3000,
            },
            {
                .flash_fw_addr_addr = 0x00040,
                .flash_fw_info_addr = 0x00048,
                .flash_fw_addr = 0x3000 + CTS_SPLIT_BIN_MAX_LEN,
            },
        },
    },
    {
        .hwid = CTS_DEV_HWID_ICNL9951,
        .fwid = CTS_DEV_FWID_ICNL9951,
        .name = "ICNL9951.bin",
        .sram_code_region_size = 128 * 1024,
        .flash_code_region_size = 128 * 1024,
        .sram_fw_start_addr = CTS_SRAM_FW_START_ADDR_DEFAULT,
        .sram_fw_crc_addr = 0x37FF0,
        .ddi_param_addr = 0x31000,
        .ddi_init_en_addr = 0x33000,
        .is_split_bins = false,
        .bins = {
            {
                .flash_fw_info_addr = 0x30000,
                .flash_fw_addr = CTS_FLASH_FW_START_ADDR_DEFAULT,
            },
        },
    },
    {
        .hwid = CTS_DEV_HWID_ICNL9951R,
        .fwid = CTS_DEV_FWID_ICNL9951R,
//        .name = "ICNL9951R.bin",
        .name = "ICNL9951R_CASC.bin", // for cascade
        .sram_code_region_size = 128 * 1024,
        .flash_code_region_size = 128 * 1024,
        .sram_fw_start_addr = CTS_SRAM_FW_START_ADDR_DEFAULT,
        .sram_fw_crc_addr = 0x37FF0,
        .ddi_param_addr = 0x31000,
        .ddi_init_en_addr = 0x33000,
        .is_split_bins = false,
        .bins = {
            {
                .flash_fw_info_addr = 0x30000,
                .flash_fw_addr = CTS_FLASH_FW_START_ADDR_DEFAULT,
            },
        },
    },
    {
        .hwid = CTS_DEV_HWID_ICNL9951C,
        .fwid = CTS_DEV_FWID_ICNL9951C,
//        .name = "ICNL9951C_VSTIM_1F__RXCFBADJ_0x10.bin",  // for signle IC
//        .name = "ICNL9951C_CASC.bin",    // for cascade
        .name = "ICNL9951C.bin",
        .sram_code_region_size = 128 * 1024,
        .flash_code_region_size = 128 * 1024,
        .sram_fw_start_addr = CTS_SRAM_FW_START_ADDR_DEFAULT,
        .sram_fw_crc_addr = 0x37FF0,
        .ddi_param_addr = 0x31000,
        .ddi_init_en_addr = 0x33000,
        .is_split_bins = false,
        .bins = {
            {
                .flash_fw_info_addr = 0x30000,
                .flash_fw_addr = CTS_FLASH_FW_START_ADDR_DEFAULT,
            },
        },
    },
    {
        .hwid = CTS_DEV_HWID_ICNL9916X,
        .fwid = CTS_DEV_FWID_ICNL9916X,
        .name = "chipone_ts_fw",
        .sram_code_region_size = 80 * 1024,
        .flash_code_region_size = 80 * 1024,
        .sram_fw_start_addr = CTS_SRAM_FW_START_ADDR_DEFAULT,
        .sram_fw_crc_addr = 0x1FFF0,
        .ddi_param_addr = 0x19000,
        .ddi_init_en_addr = 0x1B000,
        .is_split_bins = false,
        .bins = {
            {
                .flash_fw_info_addr = 0x17000,
                .flash_fw_addr = CTS_FLASH_FW_START_ADDR_DEFAULT,
            },
        },
    }
};


#define CTS_SP2_CFG3    0x78438u

int cts_pre_init_fwdata(const struct cts_device *cts_dev,
    struct cts_device_fwdata *fwdata)
{
    u8 buf[1] = {};
    int i, ret = -EINVAL;

    if(!cts_dev->hwdata) {
        cts_err("hwdata is not initialized, can not identify IC id\n");
        return ret;
    }

    cts_info("Init fwdata for IC(0x%x)", cts_dev->hwdata->hwid);

    for (i = 0; i < ARRAY_SIZE(cts_firmwares); i++) {
        if (cts_dev->hwdata->hwid == cts_firmwares[i].hwid) {
            fwdata->cts_fw = &cts_firmwares[i];
            cts_info("init IC(0x%x) with fw(%s)\n", cts_dev->hwdata->hwid,
                cts_firmwares[i].name);
                ret = 0;
        }
    }

    buf[0] = 0x12;
    /* to fix a hw bug for IC 9951R only */
    if(cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9951R)
        ret = cts_write_hw_reg(cts_dev, CTS_SP2_CFG3, buf, 1, false);

    if (ret)
        cts_err("fwdata init failed for IC(0x%x)\n", cts_dev->hwdata->hwid);

    return ret;
}

void cts_deinit_fwdata(struct cts_device *cts_dev)
{
    cts_info("%s:%d, caller:%ps", __func__, __LINE__, __builtin_return_address(0));
    cts_dev->fwdata.cts_fw = NULL;
}

static inline bool cts_is_flash_bin(const struct cts_device *cts_dev,
    const struct cts_firmware *firmware)
{
    return firmware->fw->size == cts_dev->flash->total_size;
}

static int calc_crc_in_flash(const struct cts_device *cts_dev,
        u32 flash_addr, size_t size, u32 *crc)
{
    return cts_dev->hwdata->sfctrl->ops->calc_flash_crc(cts_dev,
            flash_addr, size, crc);
}

static int cts_calc_sram_crc(const struct cts_device *cts_dev,
        u32 sram_addr, size_t size, u32 *crc)
{
    cts_info("Calc crc from sram 0x%06x size %zu", sram_addr, size);

    return cts_dev->hwdata->sfctrl->ops->calc_sram_crc(cts_dev,
            sram_addr, size, crc);
}

static void flash_fw_info_to_sram(struct cts_sram_fw_info *sram_fw_info,
    struct cts_flash_fw_info *flash_fw_info)
{
    sram_fw_info->fw_crc = flash_fw_info->fw_crc;
    sram_fw_info->fw_len = flash_fw_info->fw_len;
    sram_fw_info->sram_crc_en = CTS_FW_CRC_CHECK_EN_SRAM;
}

static void sram_fw_info_to_flash(struct cts_sram_fw_info *sram_fw_info,
    struct cts_flash_fw_info *flash_fw_info, bool is_split_bins)
{
    flash_fw_info->fw_crc = sram_fw_info->fw_crc;
    flash_fw_info->fw_len = sram_fw_info->fw_len;
    flash_fw_info->fw_crc_check_en = CTS_FW_CRC_CHECK_EN_FLASH;
    if (is_split_bins)
        flash_fw_info->fw_exist_code_split = CTS_FW_EXIST_CODE;
    else
        flash_fw_info->fw_exist_code = CTS_FW_EXIST_CODE;
}

static void parse_firmware(const struct cts_device *cts_dev,
    struct cts_firmware *firmware)
{
    int i, num_bins;

    num_bins = firmware->is_split_bins ? CTS_FW_SPLIT_BIN_NUM : 1;
    if (cts_dev->hwdata->has_flash)
        firmware->is_flash_bin = cts_is_flash_bin(cts_dev, firmware);
    else
        firmware->is_flash_bin = false;
    cts_info("%s:%d, %s, num_bins:%d", __func__, __LINE__,
        firmware->is_flash_bin ? "flash bin" : "normal bin", num_bins);
    if (unlikely(firmware->is_flash_bin)) {
        for (i = 0; i < num_bins; i++) {
            /* In flash-bin case, the address of bins is defined in fw,
             * otherwise use the address defined in array cts_firmwares.
             */
            if (firmware->is_split_bins)
                firmware->bins[i].flash_fw_addr = (__le32) *(firmware->fw->data +
                le32_to_cpu(firmware->bins[i].flash_fw_addr_addr));
            firmware->bins[i].bin = firmware->fw->data +
                    le32_to_cpu(firmware->bins[i].flash_fw_addr);
            memcpy(&firmware->bins[i].flash_fw_info,
                firmware->fw->data + le32_to_cpu(firmware->bins[i].flash_fw_info_addr),
                sizeof(struct cts_flash_fw_info));
            flash_fw_info_to_sram(&firmware->bins[i].sram_fw_info,
                &firmware->bins[i].flash_fw_info);
        }
        /* parse ddi reg info */
        firmware->ddi_params = firmware->fw->data +
                le32_to_cpu(firmware->ddi_param_addr);
        memcpy(&firmware->ddi_param_info,
            firmware->fw->data + firmware->ddi_init_en_addr,
            sizeof(struct cts_ddi_param_info));
    } else {
        for (i = 0; i < num_bins; i++) {
            firmware->bins[i].bin = firmware->fw->data +
                    i * CTS_SPLIT_BIN_MAX_LEN;
            if (firmware->is_split_bins)
                firmware->bins[i].sram_fw_info.fw_len =
                        (__le32) *(firmware->fw->data +
                        2 * CTS_SPLIT_BIN_MAX_LEN +
                        i * CTS_FW_CRC_LEN);
            else
                firmware->bins[i].sram_fw_info.fw_len = cpu_to_le32(firmware->fw->size);

            firmware->bins[i].sram_fw_info.fw_crc =
                    cts_crc32(firmware->bins[i].bin,
                        le32_to_cpu(firmware->bins[i].sram_fw_info.fw_len));
            firmware->bins[i].sram_fw_info.sram_crc_en = CTS_FW_CRC_CHECK_EN_SRAM;
            sram_fw_info_to_flash(&firmware->bins[i].sram_fw_info,
                &firmware->bins[i].flash_fw_info, firmware->is_split_bins);
        }
    }
}

static bool is_firmware_size_valid(const struct cts_device *cts_dev,
    const struct cts_firmware *firmware)
{
    int max_fw_size = CTS_MAX_FW_SIZE_DEFAULT;

    if (cts_dev->flash)
        max_fw_size = cts_dev->flash->total_size;

    return (firmware->fw->size > CTS_MIN_FW_SIZE_DEFAULT &&
        firmware->fw->size <= max_fw_size);
}

static bool is_flash_bin_valid(const struct cts_firmware *firmware)
{
    __le32 crc_cal;
    int i = 0;
    int num_bins = 0;

    num_bins = firmware->is_split_bins ? CTS_FW_SPLIT_BIN_NUM : 1;
    for(i = 0; i < num_bins; i++) {
        crc_cal = cpu_to_le32(cts_crc32(firmware->bins[i].bin,
                le32_to_cpu(firmware->bins[i].flash_fw_info.fw_len)));
        if(crc_cal != firmware->bins[i].flash_fw_info.fw_crc) {
            cts_warn("fw crc validation failed, crc_cal:%x, crc in bins[%d]:%x\n",
                crc_cal, i, firmware->bins[i].flash_fw_info.fw_crc);
                return false;
        }
    }

    /* validate ddi reg crc */
    crc_cal = cpu_to_le32(cts_crc32(firmware->ddi_params,
            le32_to_cpu(firmware->ddi_param_info.ddi_param_len)));
    if(crc_cal != firmware->ddi_param_info.ddi_param_crc) {
        cts_warn("ddi reg crc validation failed, crc_cal:%x, crc in bin:%x\n",
            crc_cal, firmware->ddi_param_info.ddi_param_crc);
            return false;
    }

    return true;
}

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
#ifdef CONFIG_CTS_SYSFS
int cts_get_num_driver_builtin_firmware(void)
{
    return NUM_DRIVER_BUILTIN_FIRMWARE;
}

const struct cts_firmware *cts_request_driver_builtin_firmware_by_name(const char *name)
{
    const struct cts_firmware *firmware;
    int i;

    cts_info("Request driver builtin by name '%s'", name);

    firmware = cts_driver_builtin_firmwares;
    for (i = 0; i < NUM_DRIVER_BUILTIN_FIRMWARE; i++, firmware++) {
        if (strcmp(firmware->name, name) == 0) {
            cts_info("Found driver builtin '%s' "
                "hwid: %06x fwid: %04x ver: %04x",
                firmware->name, firmware->hwid,
                firmware->fwid,
                FIRMWARE_VERSION(firmware));
            return firmware;
        }
    }
    cts_warn(" builtin firmware not found with name:%s"
        "hwid: %06x fwid: %04x INVALID", name);

    return NULL;
}

const struct cts_firmware *cts_request_driver_builtin_firmware_by_index(
    u32 index)
{
    const struct cts_firmware *firmware = NULL;

    cts_info("Request driver builtin by index %u", index);

    if (index < NUM_DRIVER_BUILTIN_FIRMWARE) {
        firmware = cts_driver_builtin_firmwares + index;
        cts_info("Found driver builtin '%s' "
            "hwid: %06x fwid: %04x ver: %04x",
            firmware->name, firmware->hwid, firmware->fwid,
            FIRMWARE_VERSION(firmware));
        return firmware;
    }
    cts_warn(" builtin firmware not found with hwid:%x, fwid:%x"
        "hwid: %06x fwid: %04x INVALID", firmware->hwid, firmware->hwid);

    return NULL;
}
#endif /* CONFIG_CTS_SYSFS */

struct cts_firmware *cts_request_newer_driver_builtin_firmware(
    u32 hwid, u16 fwid, u16 device_fw_ver)
{
#define MATCH_HWID(firmware, hwid)  ((firmware)->hwid == (hwid))
#define MATCH_FWID(firmware, fwid)  ((firmware)->fwid == (fwid))

    struct cts_firmware *firmware = NULL;
    int i;

    cts_info
    ("Request driver builtin if match hwid: %06x fwid: %04x && ver > %04x",
    hwid, fwid, device_fw_ver);

    firmware = cts_driver_builtin_firmwares;
    for (i = 0; i < ARRAY_SIZE(cts_driver_builtin_firmwares); i++, firmware++) {
        if (MATCH_HWID(firmware, hwid) && MATCH_FWID(firmware, fwid)) {
            cts_info("Found matched driver builtin '%s' "
                "hwid: %06x fwid: %04x ver: %04x",
                firmware->name, firmware->hwid, firmware->fwid,
                FIRMWARE_VERSION(firmware));

            if (FIRMWARE_VERSION(firmware) > device_fw_ver) {
                cts_info("Found newer driver builtin '%s' "
                    "hwid: %06x fwid: %04x ver: %04x > %04x",
                    firmware->name, firmware->hwid,
                    firmware->fwid, FIRMWARE_VERSION(firmware),
                    device_fw_ver);
                return firmware;
            }
        }
    }

    cts_info("No newer driver builtin found");

    return NULL;

#undef MATCH_HWID
#undef MATCH_FWID
}
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */

#ifdef CFG_CTS_FIRMWARE_IN_FS
bool is_filesystem_mounted(const char *filepath)
{
#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
    return false;
#else
    struct path root_path;
    struct path path;
    int ret;

    ret = kern_path("/", LOOKUP_FOLLOW, &root_path);
    if (ret)
        return false;

    ret = kern_path(filepath, LOOKUP_FOLLOW, &path);
    if (ret)
        goto err_put_root_path;

    if (path.mnt->mnt_sb == root_path.mnt->mnt_sb)
        /* not mounted */
        ret = false;
    else
        ret = true;

    path_put(&path);
err_put_root_path:
    path_put(&root_path);

    return !!ret;
#endif
}

#if 0 // APIs called in this function are deprecated, stop using it
static int cts_request_firmware_full_filepath(struct cts_firmware *firmware,
    const char *filepath, int curr_version)
{
#ifdef CFG_CTS_FOR_GKI
    cts_info("forbiddon with GKI! Please use request_firmware!");
    return -EPERM;
#else
    struct file *file;
    u16 version;
    u8 buff[2];
    loff_t pos = 0;
    int read_size;
    int ret = 0;

    file = filp_open(filepath, O_RDONLY, 0);
    if (IS_ERR(file)) {
        cts_err("Open file '%s' failed %ld", filepath, PTR_ERR(file));
        return PTR_ERR(file);
    }

    firmware->size = file_inode(file)->i_size;
    if (!is_firmware_size_valid(firmware)) {
        cts_info("File '%s' size: %zu invalid", filepath, firmware->size);
        ret = -EINVAL;
        goto err_close_file;
    }

    pos = FIRMWARE_VERSION_OFFSET;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    read_size = kernel_read(file, buff, 2, &pos);
#else
    read_size = kernel_read(file, pos, buff, 2);
#endif
    if (read_size < 0) {
        cts_err("Read version from offset 0x100 failed");
        ret = -EIO;
        goto err_close_file;
    }
    version = get_unaligned_le16(buff);

    if (version <= curr_version) {
        cts_info("File '%s' size: %zu version: %04x <= %04x",
            filepath, firmware->size, version, curr_version);
        ret = -EINVAL;
        goto err_close_file;
    }

    cts_info("File '%s' size: %zu version: %04x",
        filepath, firmware->size, version);

    firmware->data = (u8 *) vmalloc(firmware->size);
    if (firmware->data == NULL) {
        cts_err("Request form fs alloc firmware data failed");
        ret = -ENOMEM;
        goto err_close_file;
    }

    pos = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    read_size = kernel_read(file, firmware->data, firmware->size, &pos);
#else
    read_size = kernel_read(file, pos, firmware->data, firmware->size);
#endif
    if (read_size < 0 || read_size != firmware->size) {
        cts_err("Request from fs read whole file failed %d", read_size);
        ret = -EIO;
        goto err_free_firmware_data;
    }

    {
        int r = filp_close(file, NULL);

        if (r)
            cts_warn("Close file '%s' failed %d", filepath, r);
    }

    return 0;

err_free_firmware_data:
    vfree(firmware->data);
    firmware->data = NULL;

err_close_file:{
        int r = filp_close(file, NULL);

        if (r)
            cts_warn("Close file '%s' failed %d", filepath, r);
    }

    return ret;
#endif
}
#endif

static int cts_wrap_request_firmware(const struct cts_device *cts_dev,
    struct cts_firmware *firmware, struct device *device, int curr_version)
{
    int ret;

    ret = request_firmware(&firmware->fw, firmware->name, device);
    if (ret) {
        cts_err("Could not load firmware from %s: %d", firmware->name, ret);
        return ret;
    }

    /* Check firmware */
    if (!is_firmware_size_valid(cts_dev, firmware)) {
        cts_err("Invalid firmware size %zu\n", firmware->fw->size);
        ret = -EINVAL;
        goto err_release_firmware;
    }

    /*bsp:add for bringup by guoxin.zhu 20230629 start*/
    if(g_cts_data != NULL) {
        g_cts_data->bv = FIRMWARE_VERSION(firmware)&0xff;
    }
    /*bsp:add for bringup by guoxin.zhu 20230629 end*/
	
    /* Check firmware version */
	/*bsp:modify useless compare process by jiangzhu.yang 20230731 start*/
    if (curr_version > 0 && curr_version >= FIRMWARE_VERSION(firmware)) {
        cts_info("Request firmware file with version 0x%04x < 0x%04x",
            FIRMWARE_VERSION(firmware), curr_version);
        ret = -EINVAL;
        goto err_release_firmware;
    }
    /*bsp:modify useless compare process by jiangzhu.yang 20230731 end*/
	
    parse_firmware(cts_dev, firmware);

    if (firmware->is_flash_bin) {
        ret = is_flash_bin_valid(firmware);
        if (ret)
            goto err_release_firmware;
    }
    return ret;

err_release_firmware:
    release_firmware(firmware->fw);
    return ret;
}

int cts_request_newer_firmware_from_fs(const struct cts_device *cts_dev,
    struct cts_firmware *firmware, u16 curr_version)
{
    int ret = -EINVAL;
	/*bsp:add for provide by guoxin.zhu 20230911 start*/
	char firmware_name[256] = "";
    int num = chipone_controller.supplier->supplier_num;
	
	sprintf(firmware_name,"%s_0%d.bin", firmware->name,num);
	memset(firmware->name,0,CFG_CTS_FW_FILE_NAME_MAX_LEN);
	memcpy(firmware->name,firmware_name,CFG_CTS_FW_FILE_NAME_MAX_LEN);
	/*bsp:add for provide by guoxin.zhu 20230911 end*/
	
    cts_info("Request from file '%s' if version > %04x",
        cts_dev->fwdata.cts_fw->name, curr_version);
	
#if 0
    if (strchr(filepath, '/') != NULL) {    /* Full path */
        cts_info("Filepath is fullpath, direct read it out");
        ret = cts_request_firmware_full_filepath(firmware, filepath, curr_version);
    } else { /* ONLY file name is givin */
#endif
    cts_info("Filepath is only filename, use request_firmware()");
    ret = cts_wrap_request_firmware(cts_dev, firmware,
        &cts_dev->pdata->ts_input_dev->dev, curr_version);
    if (ret)
        cts_err("Request from file '%s' failed %d",
            cts_dev->fwdata.cts_fw->name, ret);

    return ret;
}

int cts_update_firmware_from_file(struct cts_device *cts_dev)
{
    struct cts_firmware *firmware;
    int bin_type;
    int ret;
	
    firmware = cts_dev->fwdata.cts_fw;
    cts_info("Update from file '%s' to %s", firmware->name,
        cts_dev->hwdata->has_flash ? "flash" : "sram");

    ret = cts_request_newer_firmware_from_fs(cts_dev, firmware, 0);
    if (ret) {
        cts_err("Request from file '%s' failed", firmware->name);
        return -EFAULT;
    }

    bin_type = cts_dev->hwdata->has_flash ? CTS_FW_BIN_ALL :
            CTS_SPLIT_BIN_NORMAL;
    ret = cts_update_firmware(cts_dev, firmware, bin_type);
    if (ret) {
        cts_err("Update to %s from file failed %d",
            cts_dev->hwdata->has_flash ? "flash" : "sram", ret);
        goto err_release_firmware;
    }

    cts_info("Update from file success");

err_release_firmware:
    cts_release_firmware(cts_dev, firmware);

    return ret;
}
#endif /*CFG_CTS_FIRMWARE_IN_FS */

int cts_request_firmware(const struct cts_device *cts_dev,
        struct cts_firmware **firmware, u32 hwid, u16 fwid,
        u16 curr_firmware_ver)
{
    int ret;

    cts_info("Request newer if match hwid: %06x fwid: %04x && ver > %04x",
        hwid, fwid, curr_firmware_ver);

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
    *firmware = cts_request_newer_driver_builtin_firmware(
        hwid, fwid, curr_firmware_ver);
#endif

#ifdef CFG_CTS_FIRMWARE_IN_FS
#ifndef CFG_CTS_FOR_GKI
    if (is_filesystem_mounted(CFG_CTS_FIRMWARE_FILEPATH))
#endif
        *firmware = cts_dev->fwdata.cts_fw;
        cts_info(" firmware addr %p", cts_dev->fwdata.cts_fw);
        ret = cts_request_newer_firmware_from_fs(cts_dev, *firmware,
                curr_firmware_ver);
#endif /* CFG_CTS_FIRMWARE_IN_FS */

    return ret;
}

void cts_release_firmware(const struct cts_device *cts_dev,
    const struct cts_firmware *firmware)
{
    cts_info("Release firmware %p", firmware);

    if (firmware == cts_dev->fwdata.cts_fw) {
        if (firmware->fw) {
            /* Use request_firmware() get struct firmware * */
            cts_info("Release firmware from request_firmware");
            release_firmware(firmware->fw);
        } else
            /* Builtin firmware with non-NULL name, no need to free */
            cts_info("Release firmware from driver built-in");
    }
}

int cts_update_flash_bin(struct cts_device *cts_dev,
    const struct cts_firmware *firmware)
{
    int tx_len, sram_buf_size, flash_size;
    int wr_size = 0;
    int ret = 0;

    flash_size = cts_dev->flash->total_size;
    sram_buf_size = firmware->sram_code_region_size;

    cts_info("sram code region size:%d\n", sram_buf_size);
    if (sram_buf_size < 10 * 1024 || sram_buf_size > 256 * 1024)
        cts_warn("Size of sram code region may be incorrect, size:%d\n",
            sram_buf_size);

    ret = cts_prepare_flash_operation(cts_dev);
    if (ret) {
        cts_warn("Prepare flash operation failed %d", ret);
        goto err_out;
    }

    ret = cts_erase_flash(cts_dev, 0, flash_size);
    if (ret) {
        cts_err("Erase whole flash failed %d", ret);
        goto err_out;
    }

    do {
        tx_len = min(flash_size - wr_size, sram_buf_size);
        ret = cts_write_sram(cts_dev, CTS_SRAM_FW_START_ADDR_DEFAULT,
            firmware->fw->data + wr_size, tx_len, false);
        if (ret) {
            cts_err("write fw to sram failed, tx_len:%d, wr_size:%d, ret=%d\n",
                tx_len, wr_size, ret);
                goto err_out;
        }

        ret = cts_program_flash_from_sram(cts_dev, 0 + wr_size,
                CTS_SRAM_FW_START_ADDR_DEFAULT, tx_len);
        if (ret) {
            cts_err("copy fw from sram to flash failed wr_size:%d, tx_len:%d",
                wr_size, tx_len);
            goto err_out;
        }
        wr_size += tx_len;
    } while (wr_size < flash_size);

    cts_reset_device(cts_dev);

    if (cts_plat_is_normal_mode(cts_dev->pdata)) {
        cts_err("update fw to flash, enter normal mode failed!\n");
        ret = -EINVAL;
    }

err_out:
    return ret;
}

int cts_update_fw_to_flash(struct cts_device *cts_dev,
    const struct cts_firmware *firmware)
{
    struct cts_flash_split_fw_info split_fw_info;
    int sram_buf_size, flash_code_region_size, flash_size;
    int fw_info_size, fw_bin_num, i, ret;
    __le32 flash_crc_cal;
    bool is_split_bins = firmware->is_split_bins;

    flash_code_region_size = firmware->flash_code_region_size;
    sram_buf_size = firmware->sram_code_region_size;
    flash_size = cts_dev->flash->total_size;

    cts_info("%s:%d, sram code region size:%d, flash code region size:%d , flash_size:%d\n",
        __func__, __LINE__, sram_buf_size, flash_code_region_size, flash_size);

    if (sram_buf_size < 10 * 1024 || sram_buf_size > 256 * 1024 ||
        flash_code_region_size < 10 * 1024 || flash_code_region_size > 256 * 1024)
        cts_warn("Size of code region may be incorrect,"
            "sram code region size:%d, flash code region size:%d\n",
            sram_buf_size, flash_code_region_size);

    ret = cts_prepare_flash_operation(cts_dev);
    if (ret) {
        cts_warn("Prepare flash operation failed %d", ret);
        goto err_out;
    }

    /* bins[0] start from the beginning of code region in flash, then
     * bins[1] goes after.
     */
    ret = cts_erase_flash(cts_dev, firmware->bins[0].flash_fw_addr,
            flash_code_region_size);
    if (ret) {
        cts_err("Erase code region in flash failed %d", ret);
        goto err_out;
    }

    fw_info_size = is_split_bins ?
            (CTS_FW_SPLIT_BIN_NUM * sizeof(struct cts_flash_split_fw_info)) :
            sizeof(struct cts_flash_fw_info);
    cts_info("fw_info_size:%d\n",fw_info_size);
    fw_bin_num = is_split_bins ? CTS_FW_SPLIT_BIN_NUM : 1;
    for (i = 0; i < fw_bin_num; i++) {
        /* write fw to sram, and then copy fw from sram to flash */
        cts_info("%s:%d, num_of_bins:%d, write bin[%d] to sram, size:0x%x",
            __func__, __LINE__, fw_bin_num, i,
            le32_to_cpu(firmware->bins[i].sram_fw_info.fw_len));
        ret = cts_write_sram(cts_dev, 0x00, firmware->bins[i].bin,
            le32_to_cpu(firmware->bins[i].sram_fw_info.fw_len), false);
        if (ret) {
            cts_err("write bins[%d] to sram failed!\n", i);
            goto err_out;
        }

        ret = cts_program_flash_from_sram(cts_dev,
                firmware->bins[i].flash_fw_addr, 0,
                le32_to_cpu(firmware->bins[i].sram_fw_info.fw_len));
        if (ret) {
            cts_err("write bins[%d] from sram to flash failed!\n", i);
            goto err_out;
        }

        /* write fw info to flash */
        if (is_split_bins) {
            split_fw_info.bin_flash_addr = firmware->bins[i].flash_fw_addr;
            split_fw_info.bin_sram_addr = CTS_SRAM_FW_START_ADDR_DEFAULT;
            memcpy(&split_fw_info.fw_info, &firmware->bins[i].flash_fw_info,
                sizeof(struct cts_flash_fw_info));
            ret = cts_program_flash(cts_dev, firmware->bins[i].flash_fw_addr_addr,
                    &split_fw_info, sizeof(struct cts_flash_split_fw_info));
            if (ret) {
                cts_err("write fw info of bins[%d] to flash failed!\n", i);
                goto err_out;
            }
        } else {
            ret = cts_program_flash(cts_dev, firmware->bins[i].flash_fw_info_addr,
                    &firmware->bins[i].flash_fw_info, sizeof(struct cts_flash_fw_info));
            if (ret) {
                cts_err("write fw info of bins[%d] to flash failed!\n", i);
                goto err_out;
            }
        }

        /* readback crc from flash and validate it. */
        calc_crc_in_flash(cts_dev, firmware->bins[i].flash_fw_addr,
            le32_to_cpu(firmware->bins[i].sram_fw_info.fw_len), &flash_crc_cal);
        if (flash_crc_cal != firmware->bins[i].flash_fw_info.fw_crc)
            cts_warn("crc check failed, fw_crc:%x, flash_crc:%x\n",
                firmware->bins[i].flash_fw_info.fw_crc, flash_crc_cal);
    }
    cts_post_flash_operation(cts_dev);

    cts_reset_device(cts_dev);
    if (!cts_plat_is_normal_mode(cts_dev->pdata)) {
        cts_info("update fw to flash, enter normal mode failed!\n");
        ret = -EINVAL;
    }
err_out:
    return ret;
}

static inline int bin_type_to_bin_index(int bin_type){
    return bin_type - 1;
}

int cts_update_fw_to_sram_none_casc(struct cts_device *cts_dev,
    const struct cts_firmware *firmware, int bin_type)
{
    int index, ret;
    __le32 sram_crc_cal;
    bool is_split_bins = firmware->is_split_bins;

    index = is_split_bins ? bin_type_to_bin_index(bin_type) : 0;
    ret = cts_write_sram(cts_dev, CTS_SRAM_FW_START_ADDR_DEFAULT,
            firmware->bins[index].bin,
            le32_to_cpu(firmware->bins[index].sram_fw_info.fw_len), false);
    if (ret) {
        cts_err("write fw to sram failed!\n");
        goto out;
    }

    ret = cts_calc_sram_crc(cts_dev, CTS_SRAM_FW_START_ADDR_DEFAULT,
            firmware->bins[index].sram_fw_info.fw_len, &sram_crc_cal);
    if (ret) {
        cts_err("calculate crc in sram failed!\n");
        goto out;
    }
    if (sram_crc_cal != firmware->bins[index].sram_fw_info.fw_crc) {
        cts_err("calculate crc mismatch, crc:%x, crc_calculate:%x\n",
            firmware->bins[index].sram_fw_info.fw_crc, sram_crc_cal);
        goto out;
    }

    ret = cts_write_sram(cts_dev, firmware->sram_fw_crc_addr,
           (u8 *) &firmware->bins[index].sram_fw_info,
            sizeof(struct cts_sram_fw_info), false);
    if (ret) {
        cts_err("write crc to sram failed, crc addr:%x\n",
            firmware->sram_fw_crc_addr);
        goto out;
    }

    ret = cts_enter_normal_mode(cts_dev);

out:
    return ret;
}

#ifdef CTS_KRANG

int cts_update_fw_to_sram_casc(struct cts_device *cts_dev,
    const struct cts_firmware *firmware, int bin_type)
{
    int index, ret;
    __le32 sram_crc_cal;
    bool is_split_bins = firmware->is_split_bins;

    index = is_split_bins ? bin_type_to_bin_index(bin_type) : 0;
    ret = cts_write_sram(cts_dev, CTS_SRAM_FW_START_ADDR_DEFAULT,
            firmware->bins[index].bin,
            le32_to_cpu(firmware->bins[index].sram_fw_info.fw_len), false);
    if (ret) {
        cts_err("write fw to sram failed!\n");
        goto out;
    }

    ret = cts_calc_sram_crc(cts_dev, CTS_SRAM_FW_START_ADDR_DEFAULT,
            firmware->bins[index].sram_fw_info.fw_len, &sram_crc_cal);
    if (ret) {
        cts_err("calculate crc in sram failed!\n");
        goto out;
    }
    if (sram_crc_cal != firmware->bins[index].sram_fw_info.fw_crc) {
        cts_err("calculate crc mismatch, crc:%x, crc_calculate:%x\n",
            firmware->bins[index].sram_fw_info.fw_crc, sram_crc_cal);
        goto out;
    }

    /* write fw code to slave1 */
    ret = cts_write_slave(cts_dev, CTS_SRAM_FW_START_ADDR_DEFAULT, NULL,
            firmware->bins[index].sram_fw_info.fw_len, CTS_KR_SLAVE1);
    ret = cts_write_sram(cts_dev, firmware->sram_fw_crc_addr,
           (u8 *) &firmware->bins[index].sram_fw_info,
            sizeof(struct cts_sram_fw_info), false);
    if (ret) {
        cts_err("write crc to sram failed, crc addr:%x\n",
            firmware->sram_fw_crc_addr);
        goto out;
    }

    /* write fw info to slave1 */
    ret = cts_write_slave(cts_dev, firmware->sram_fw_crc_addr,
            (u8 *) &firmware->bins[index].sram_fw_info,
            sizeof(struct cts_sram_fw_info), CTS_KR_SLAVE1);

    ret = cts_enter_normal_mode(cts_dev);

    //TODO:: implete slave crc check
out:
    return ret;
}

#else //ifdef CTS_KRANG

static int cts_switch_to_ic(struct cts_device *cts_dev, uint8_t type)
{
    uint8_t dat = 0x83;
    int count = 0;
    int retries = 10;
    int rc;

    if (type == 1) {
        dat = 0x81;
    } else if (type == 2) {
        dat = 0x82;
    } else if (type == 3) {
        dat = 0x83;
    }

    while (retries--) {
        if (cts_dev->bus_type == CTS_BUS_SPI) {
            rc = cts_spi_send_recv(cts_dev->pdata, &dat,
                    1, NULL);
        } else if (cts_dev->bus_type == CTS_BUS_I2C) {
            rc = cts_plat_i2c_transfer(cts_dev->pdata,
                    CTS_I2C_PROG_ADDR_DEFAULT, &dat,
                    1, NULL, 0, 0, 0);
        }
        if (rc)
            cts_err("Send %d to ic failed: rc=%d", dat, rc);
        else {
            count++;
            if (count == 2)
                return 0;
        }
    }

    return -EIO;
}

int cts_switch_to_master(struct cts_device *cts_dev)
{
    return cts_switch_to_ic(cts_dev, 1);
}

int cts_switch_to_slave(struct cts_device *cts_dev)
{
    return cts_switch_to_ic(cts_dev, 2);
}

int cts_switch_to_all(struct cts_device *cts_dev)
{
    return cts_switch_to_ic(cts_dev, 3);
}


static int cts_prepare_update_firmware(struct cts_device *cts_dev)
{
    int ret = -EINVAL;

    if (cts_dev->bus_type != CTS_BUS_SPI) {
        cts_err("%s:%d, 8X feature is for SPI only, current bus type:%d",
            __func__, __LINE__, cts_dev->bus_type);
        return ret;
    }

    ret = cts_switch_to_master(cts_dev);
    if (ret) {
        cts_err("Switch to master faild %d", ret);
        return ret;
    }

    ret = cts_enter_program_mode(cts_dev);
    if (ret) {
        cts_err("Device enter program mode failed %d", ret);
        return ret;
    }

    ret = cts_switch_to_slave(cts_dev);
    if (ret) {
        cts_err("Switch to slave field %d", ret);
        return ret;
    }

    ret = cts_enter_program_mode(cts_dev);
    if (ret) {
        cts_err("Slave device enter program mode failed %d", ret);
        return ret;
    }

    ret = cts_switch_to_all(cts_dev);
    if (ret) {
        cts_err("Switch to all failed %d", ret);
        return ret;
    }

    return ret;
}

int cts_update_fw_to_sram_casc(struct cts_device *cts_dev,
    const struct cts_firmware *firmware, int bin_type)
{
    int index, ret;
    __le32 sram_crc_cal;
    bool is_split_bins = firmware->is_split_bins;

    index = is_split_bins ? bin_type_to_bin_index(bin_type) : 0;
    ret = cts_write_sram(cts_dev, CTS_SRAM_FW_START_ADDR_DEFAULT,
            firmware->bins[index].bin,
            le32_to_cpu(firmware->bins[index].sram_fw_info.fw_len), false);
    if (ret) {
        cts_err("write fw to sram failed!\n");
        goto out;
    }

    /* validate crc of fw from master */
    ret = cts_prepare_update_firmware(cts_dev);
    if (ret) {
        cts_err("master&slave preparation faild %d", ret);
        goto out;
    }

    ret = cts_calc_sram_crc(cts_dev, CTS_SRAM_FW_START_ADDR_DEFAULT,
            firmware->bins[index].sram_fw_info.fw_len, &sram_crc_cal);
    if (ret) {
        cts_err("calculate crc in sram failed!\n");
        goto out;
    }
    if (sram_crc_cal != firmware->bins[index].sram_fw_info.fw_crc) {
        cts_err("calculate crc mismatch, crc:%x, crc_calculate:%x\n",
            firmware->bins[index].sram_fw_info.fw_crc, sram_crc_cal);
        goto out;
    }

    /* validate crc of fw from slave */
    ret = cts_switch_to_slave(cts_dev);
    if (ret) {
        cts_err("Switch to slave faild %d", ret);
        goto out;
    }

    ret = cts_calc_sram_crc(cts_dev, CTS_SRAM_FW_START_ADDR_DEFAULT,
            firmware->bins[index].sram_fw_info.fw_len, &sram_crc_cal);
    if (ret) {
        cts_err("calculate crc in sram failed!\n");
        goto out;
    }
    if (sram_crc_cal != firmware->bins[index].sram_fw_info.fw_crc) {
        cts_err("calculate crc mismatch, crc:%x, crc_calculate:%x\n",
            firmware->bins[index].sram_fw_info.fw_crc, sram_crc_cal);
        goto out;
    }

    /* validate crc of fw from master */
    ret = cts_switch_to_master(cts_dev);
    if (ret) {
        cts_err("Switch to master faild %d", ret);
        goto out;
    }

    ret = cts_calc_sram_crc(cts_dev, CTS_SRAM_FW_START_ADDR_DEFAULT,
            firmware->bins[index].sram_fw_info.fw_len, &sram_crc_cal);
    if (ret) {
        cts_err("calculate crc in sram failed!\n");
        goto out;
    }
    if (sram_crc_cal != firmware->bins[index].sram_fw_info.fw_crc) {
        cts_err("calculate crc mismatch, crc:%x, crc_calculate:%x\n",
            firmware->bins[index].sram_fw_info.fw_crc, sram_crc_cal);
        goto out;
    }

    ret = cts_switch_to_all(cts_dev);
    if (ret) {
        cts_err("Switch to all failed %d", ret);
        goto out;
    }

    ret = cts_enter_normal_mode(cts_dev);

out:
    return ret;
}
#endif //ifdef CTS_KRANG

int cts_update_fw_to_sram(struct cts_device *cts_dev,
    const struct cts_firmware *firmware, int bin_type)
{
    if (cts_dev->hwdata->support_cascade &&
        cts_dev->pdata->is_cascade_enabled)
        return cts_update_fw_to_sram_casc(cts_dev, firmware, bin_type);
    else
        return cts_update_fw_to_sram_none_casc(cts_dev, firmware, bin_type);

}

#define HCLK_BUS    0x70206

int cts_update_firmware(struct cts_device *cts_dev,
        const struct cts_firmware *firmware, int bin_type)
{
    u8 buf[2] = {};
    int ret;
    bool to_flash = cts_dev->hwdata->has_flash;
    bool is_flash_bin = firmware->is_flash_bin;
    bool enabled = cts_is_device_enabled(cts_dev);

    cts_info("Update firmware to %s ver: %04x size: %zu",
        to_flash ? "flash" : "sram",
        FIRMWARE_VERSION(firmware), firmware->fw->size);

    if (enabled) {
        ret = cts_stop_device(cts_dev);
        if (ret) {
            cts_err("Stop device failed %d(%s)",
                ret, cts_strerror(ret));
            return ret;
        }
    }

    if (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9951C) {
        buf[0] = 0xcf;
        ret = cts_write(cts_dev, (u32) HCLK_BUS, buf, 1, CTS_TP, true);
        if (ret) {
            cts_err("fix HCLK_BUS failed %d", ret);
            goto out;
        }
    }

    ret = cts_enter_program_mode(cts_dev);
    if (ret) {
        cts_err("Device enter program mode failed %d", ret);
        goto out;
    }

    cts_dev->rtdata.updating = true;

    if (unlikely(is_flash_bin)) {
        if (!to_flash) {
            cts_err("firmware format is flash-bin, but flash is not installed!\n");
            ret = -EINVAL;
            goto out;
        }
        ret = cts_update_flash_bin(cts_dev, firmware);
        if (!ret)
            cts_info("updata flash-bin success!\n");
    } else {
        if (to_flash)
            ret = cts_update_fw_to_flash(cts_dev, firmware);
        else
            ret = cts_update_fw_to_sram(cts_dev, firmware, bin_type);
    }

    if(ret == 0)
        cts_info("Update fw success!!!");
out:
    cts_dev->rtdata.updating = false;

#ifdef CONFIG_CTS_CHARGER_DETECT
    if (cts_is_charger_exist(cts_dev)) {
        int r = cts_set_dev_charger_attached(cts_dev, true);
        if (r)
            cts_err("Set dev charger attached failed %d", r);
    }
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    if (cts_is_earjack_exist(cts_dev)) {
        int r = cts_set_dev_earjack_attached(cts_dev, true);
        if (r)
            cts_err("Set dev earjack attached failed %d", r);
    }
#endif

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev))
        cts_enter_glove_mode(cts_dev);
#endif

    return ret;
}

bool cts_is_firmware_updating(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.updating;
}
