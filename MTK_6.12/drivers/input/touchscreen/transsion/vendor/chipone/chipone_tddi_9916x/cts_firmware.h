#ifndef CTS_FIRMWARE_H
#define CTS_FIRMWARE_H

#include <linux/firmware.h>

#define CTS_FW_SPLIT_BIN_NUM      2
#define CTS_SPLIT_BIN_MAX_LEN   (64 * 1024)
#define CTS_FW_CRC_LEN          4
#define CTS_SRAM_FW_START_ADDR_DEFAULT  0
/* fw code in flash start from 0x0000 except for split bins */
#define CTS_FLASH_FW_START_ADDR_DEFAULT 0

/* For most of the cases 2 lines flash is used, and the magic num is defined
 * as 0x7473756E, but the magic num should be 0x71756164 for the special cases
 * 4 lines flash.
 */
#define CTS_FW_EXIST_CODE           cpu_to_le32(0x7473756E)
#define CTS_FW_EXIST_CODE_QUAD      cpu_to_le32(0x71756164)
#define CTS_FW_CRC_CHECK_EN_FLASH   cpu_to_le32(~0x0000C35A)
#define CTS_FW_CRC_CHECK_EN_SRAM    cpu_to_le32(0xCC33555A)
#define CTS_DDI_INIT_EN             cpu_to_le32(0x0000C35A)

#define CTS_SPLIT_BIN_NORMAL  CTS_FW_BIN_0
#define CTS_SPLIT_BIN_GESTURE CTS_FW_BIN_1
#define CTS_SPLIT_BIN_TEST    CTS_FW_BIN_1

enum bin_idx {
    CTS_FW_BIN_0 = 0x1,
    CTS_FW_BIN_1 = 0x2,
    CTS_FW_BIN_ALL = 0x3,
};

/* In the split bins case, each bin has a unique flash addr to store crc info,
 * so it's inside struct cts_fw_bins.
 */
struct cts_flash_fw_info {
    __le32 fw_crc;
    __le32 fw_len;
    __le32 fw_crc_check_en;
    /* This code is used for split bin case*/
    __le32 fw_exist_code_split;
    /* This code is used for single bin case*/
    __le32 fw_exist_code;
};

struct cts_flash_split_fw_info {
    __le32 bin_flash_addr;
    __le32 bin_sram_addr;
    struct cts_flash_fw_info fw_info;
};

/* Each bin shares the same addr to store crc info even in the case of split bins
 * so it's outside struct cts_fw_bins.
 */
struct cts_sram_fw_info {
    __le32 sram_crc_en;
    __le32 :32;
    __le32 fw_crc;
    __le32 fw_len;
};

struct cts_fw_bins {
    const u8 *bin;
    /* This address records the fw info*/
    __le32 flash_fw_info_addr;
    /* This addr records the address of the bin in flash, and is used for split bin only*/
    __le32 flash_fw_addr_addr;
    __le32 flash_fw_addr;
    struct cts_flash_fw_info flash_fw_info;
    struct cts_sram_fw_info sram_fw_info;
};

struct cts_ddi_param_info {
    __le32 ddi_init_en;
    __le32 ddi_param_crc_en;
    __le32 ddi_param_crc;
    __le32 ddi_param_len;
};

struct cts_firmware {
    char name[CFG_CTS_FW_FILE_NAME_MAX_LEN];
    u32 hwid;
    u16 fwid;
    /* The fw is divided into 2 bins if the sram is smaller than the size of
     * full firmware code. Code of normal mode is placed in bin0 and guesture
     * mode shares bin1 with product test.
     */
    bool is_split_bins;
    /* Flash bin has ddi params, and the data is in the same format as
     * partition table of flash
     */
    bool is_flash_bin;
    u32 sram_code_region_size;
    u32 flash_code_region_size;
    u32 sram_fw_start_addr;
    u32 sram_fw_crc_addr;

    const struct firmware *fw;

    struct cts_fw_bins bins[CTS_FW_SPLIT_BIN_NUM];

    u32 ddi_param_addr;
    u32 ddi_init_en_addr;
    const u8 *ddi_params;
    struct cts_ddi_param_info ddi_param_info;
};

/* TODO: need to confirm that the offset is valid or not */
#define FIRMWARE_VERSION_OFFSET     0x100
#define FIRMWARE_VERSION(firmware)  \
    get_unaligned_le16((firmware)->fw->data + FIRMWARE_VERSION_OFFSET)


int cts_pre_init_fwdata(const struct cts_device *cts_dev,
    struct cts_device_fwdata *fwdata);
void cts_deinit_fwdata(struct cts_device *cts_dev);
extern int cts_request_firmware(const struct cts_device *cts_dev,
    struct cts_firmware **firmware, u32 hwid, u16 fwid,
    u16 curr_firmware_ver);
extern void cts_release_firmware(const struct cts_device *cts_dev,
    const struct cts_firmware *firmware);

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
extern int cts_get_num_driver_builtin_firmware(void);
extern const struct cts_firmware *cts_request_driver_builtin_firmware_by_name(
        const char *name);
extern const struct cts_firmware *cts_request_driver_builtin_firmware_by_index(
        u32 index);
#else /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */
static inline int cts_get_num_driver_builtin_firmware(void)
{
    return 0;
}

static inline const struct cts_firmware
*cts_request_driver_builtin_firmware_by_name(const char *name)
{
    return NULL;
}

static inline const struct cts_firmware
*cts_request_driver_builtin_firmware_by_index(u32 index)
{
    return NULL;
}
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */
extern bool cts_is_firmware_updating(const struct cts_device *cts_dev);

extern int cts_update_firmware(struct cts_device *cts_dev,
    const struct cts_firmware *firmware, int bin_index);

#ifdef CFG_CTS_FIRMWARE_IN_FS
extern int cts_request_newer_firmware_from_fs(const struct cts_device *cts_dev,
    struct cts_firmware *firmware, u16 curr_version);
extern int cts_update_firmware_from_file(struct cts_device *cts_dev);

#endif /* CFG_CTS_FIRMWARE_IN_FS */

#endif /* CTS_FIRMWARE_H */
