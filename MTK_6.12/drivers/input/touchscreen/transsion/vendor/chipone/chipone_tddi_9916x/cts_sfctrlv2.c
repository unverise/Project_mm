#define LOG_TAG         "SFCtrl"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sfctrl.h"
#include "cts_transfer.h"

#define rSFCTRLv2_CMD_SEL               (0x0000)
#define rSFCTRLv2_FLASH_ADDR            (0x0004)
#define rSFCTRLv2_SRAM_ADDR             (0x0008)
#define rSFCTRLv2_DATA_LENGTH           (0x000C)
#define rSFCTRLv2_START_DEXC            (0x0010)
#define rSFCTRLv2_RELEASE_FLASH         (0x0014)
#define rSFCTRLv2_HW_STATE              (0x0018)
#define rSFCTRLv2_CRC_RESULT            (0x001C)
#define rSFCTRLv2_SRAM_CRC_START        (0x0020)
#define rSFCTRLv2_FLASH_CRC_START       (0x0022)
#define rSFCTRLv2_SF_BUSY               (0x0024)

/** Constants for register @ref rSFCTRLv2_CMD_SEL */
enum sfctrlv2_cmd {
    SFCTRLv2_CMD_FAST_READ = 0x01u,
    SFCTRLv2_CMD_SE = 0x02u,
    SFCTRLv2_CMD_BE = 0x03u,
    SFCTRLv2_CMD_PP = 0x04u,
    SFCTRLv2_CMD_RDSR = 0x05u,
    SFCTRLv2_CMD_RDID = 0x06u,
    SFCTRLv2_CMD_CHIP_ERASE = 0x0cu,
};

/** SPI flash controller v2 operation flags. */
enum sfctrlv2_opflags {
    SFCTRLv2_OPFLAG_READ = BIT(0),
    SFCTRLv2_OPFLAG_SET_FLASH_ADDR = BIT(1),
    SFCTRLv2_OPFLAG_SRAM_DATA_XCHG = BIT(2),
    SFCTRLv2_OPFLAG_SET_DATA_LENGTH = BIT(3),
};

#define SFCTRLv2_CMD_RDID_FLAGS \
    (SFCTRLv2_OPFLAG_READ | \
    SFCTRLv2_OPFLAG_SRAM_DATA_XCHG)

#define SFCTRLv2_CMD_RDSR_FLAGS    \
    (SFCTRLv2_OPFLAG_READ | \
    SFCTRLv2_OPFLAG_SRAM_DATA_XCHG)

#define SFCTRLv2_CMD_SE_FLAGS \
    (SFCTRLv2_OPFLAG_SET_FLASH_ADDR)

#define SFCTRLv2_CMD_BE_FLAGS \
    (SFCTRLv2_OPFLAG_SET_FLASH_ADDR)

#define SFCTRLv2_CMD_PP_FLAGS \
    (SFCTRLv2_OPFLAG_SET_FLASH_ADDR | \
    SFCTRLv2_OPFLAG_SRAM_DATA_XCHG  | \
    SFCTRLv2_OPFLAG_SET_DATA_LENGTH)

#define SFCTRLv2_CMD_FAST_READ_FLAGS  \
    (SFCTRLv2_OPFLAG_READ | \
    SFCTRLv2_OPFLAG_SET_FLASH_ADDR | \
    SFCTRLv2_OPFLAG_SRAM_DATA_XCHG  | \
    SFCTRLv2_OPFLAG_SET_DATA_LENGTH)

#define sfctrl_reg_addr(cts_dev, offset)  \
    ((cts_dev)->hwdata->sfctl_base + offset)

static int wait_sfctrl_xfer_comp(const struct cts_device *cts_dev)
{
    int retries = 0;
    int ret;
    u8 status;

    do {
        ret = cts_read_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_SF_BUSY),
            &status, 1, false);

        if (!ret && status == 0)
            return 0;
        else if (ret)
            break;
        mdelay(20);
    } while (status && retries++ < 50);

    return -ETIMEDOUT;
}

static int sfctrlv2_rdsr(const struct cts_device *cts_dev, u8 *status)
{
#define RDSR_XCHG_SRAM_ADDR     (cts_dev->hwdata->sfctrl->xchg_sram_base + cts_dev->hwdata->sfctrl->xchg_sram_size - 1)
    int ret;
    u32 val;

    val = SFCTRLv2_CMD_RDSR;
    ret = cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_CMD_SEL),
            (u8 *) &val, sizeof(u8), false);
    if (ret)
        cts_err("write SFCTRLv2_CMD_RDSR failed %d", ret);
    val = RDSR_XCHG_SRAM_ADDR;
    ret = cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_SRAM_ADDR),
            (u8 *) &val, sizeof(u32), false);
    if (ret)
        cts_err("write RDSR_XCHG_SRAM_ADDR failed %d", ret);
    val = 1;
    ret = cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_START_DEXC),
            (u8 *) &val, sizeof(u8), false);
    if (ret)
        cts_err("write rSFCTRLv2_START_DEXC failed %d", ret);

    ret = wait_sfctrl_xfer_comp(cts_dev);
    if (ret != 0) {
        cts_err("Wait sfctrl ready failed %d", ret);
        return ret;
    }

    ret = cts_read_sram(cts_dev, RDSR_XCHG_SRAM_ADDR, status, sizeof(u8), false);
    if (ret) {
        cts_err("Read exchange sram failed %d", ret);
        return ret;
    }

    return 0;
}

static int wait_flash_wip_clear(const struct cts_device *cts_dev)
{
#define FLASH_SR_WIP        BIT(0)    /*!< Write in progress */

    int ret, retries = 0;
    u8 status;

    do {
        ret = sfctrlv2_rdsr(cts_dev, &status);
        if (ret) {
            cts_err("Read flash status register failed %d", ret);
            return ret;
        }
        cts_info("%s:%d, status:%x", __func__, __LINE__, status);

        if (status & FLASH_SR_WIP)
            mdelay(30);
        else
            return 0;
    } while (status & FLASH_SR_WIP && retries++ < 100);
    return -ETIMEDOUT;
#undef FLASH_SR_WIP
}

static int sfctrlv2_transfer(const struct cts_device *cts_dev,
        u8 cmd, void *data, u32 flash_addr, u32 sram_addr,
        size_t size, u32 flags)
{
    int ret;
    u8 buffer[4];
cts_dbg("%s:%d, rSFCTRLv2_CMD_SEL", __func__, __LINE__);
    cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_CMD_SEL),
        &cmd, 1, false);

    if (flags & SFCTRLv2_OPFLAG_SRAM_DATA_XCHG) {
        cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_SRAM_ADDR),
            (u8 *) &sram_addr, sizeof(sram_addr), false);

/** - Write data to exchange SRAM if operation is write and
 *        data != NULL(data not in SRAM)
 */
        if ((!(flags & SFCTRLv2_OPFLAG_READ)) && data) {
            ret = cts_write_sram(cts_dev, sram_addr, data, size, false);
            if (ret) {
                cts_err("Write data to exchange sram 0x%06x size %zu failed %d",
                        sram_addr, size, ret);
                return ret;
            }
        }
    }

    if (flags & SFCTRLv2_OPFLAG_SET_FLASH_ADDR) {
        put_unaligned_le32(flash_addr, buffer);
        cts_dbg("%s:%d, flash addr:0x%x", __func__, __LINE__, flash_addr);
        cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_FLASH_ADDR),
            buffer, sizeof(flash_addr), false);
    }
    if (flags & SFCTRLv2_OPFLAG_SET_DATA_LENGTH) {
        put_unaligned_le32(size, buffer);
        cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_DATA_LENGTH),
            buffer, 4, false);
    }

    buffer[0] = 1;
    cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_START_DEXC),
        buffer, 1, false);

    ret = wait_sfctrl_xfer_comp(cts_dev);
    if (ret != 0) {
        cts_err("Wait sfctrl ready failed %d", ret);
        return ret;
    }

    ret = wait_flash_wip_clear(cts_dev);
    if (ret != 0) {
        cts_err("Wait WIP clear failed %d", ret);
        return ret;
    }

    if ((flags & SFCTRLv2_OPFLAG_READ) && data) {
        ret = cts_read_sram(cts_dev, sram_addr, data, size, false);
        if (ret) {
            cts_err("Read data from exchange sram 0x%06x size %zu failed %d",
                    sram_addr, size, ret);
            return ret;
        }
    }

    return 0;
}

static int sfctrlv2_rdid(const struct cts_device *cts_dev, u32 *id)
{
    int ret;
    u8 buf[4] = {0};

    ret = sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_RDID, buf,
            0, cts_dev->hwdata->sfctrl->xchg_sram_base, 4,
            SFCTRLv2_CMD_RDID_FLAGS);
    *id = ret ? 0 : le32_to_cpup((const __le32 *)buf);
    *id = *id & CTS_FLASH_ID_MASK;

    return ret;
}

static int sfctrlv2_se(const struct cts_device *cts_dev, u32 sector_addr)
{
    return sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_SE, NULL,
            sector_addr, 0, 0, SFCTRLv2_CMD_SE_FLAGS);
}

static int sfctrlv2_be(const struct cts_device *cts_dev, u32 block_addr)
{
    return sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_BE, NULL,
            block_addr, 0, 0, SFCTRLv2_CMD_BE_FLAGS);
}

static int sfctrlv2_pp(const struct cts_device *cts_dev,
        u32 flash_addr, const void *src, size_t size)
{
    return sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_PP, (void *)src,
            flash_addr, cts_dev->hwdata->sfctrl->xchg_sram_base, size,
            SFCTRLv2_CMD_PP_FLAGS);
}

static int sfctrlv2_program_flash_from_sram(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, size_t len)
{
    return sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_PP, NULL,
            flash_addr, sram_addr, len, SFCTRLv2_CMD_PP_FLAGS);
}

static int sfctrlv2_read(const struct cts_device *cts_dev,
        u32 flash_addr, void *dst, size_t size)
{
    return sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_FAST_READ, dst,
        flash_addr, cts_dev->hwdata->sfctrl->xchg_sram_base, size,
        SFCTRLv2_CMD_FAST_READ_FLAGS);
}

static int sfctrlv2_read_flash_to_sram(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, void *dst, size_t size)
{
    return sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_FAST_READ, dst,
            flash_addr, sram_addr, size, SFCTRLv2_CMD_FAST_READ_FLAGS);
}

int sfctrlv2_calc_sram_crc(const struct cts_device *cts_dev,
        u32 sram_addr, size_t size, u32 *crc)
{
    int ret;
    u8 val;

    cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_SRAM_ADDR),
        (u8 *) &sram_addr, sizeof(sram_addr), false);
    mdelay(1);
    cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_DATA_LENGTH),
        (u8 *) &size, 4, false);
    mdelay(1);
    val = 1;
    cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_SRAM_CRC_START),
        &val, 1, false);
    mdelay(10);

    ret = wait_sfctrl_xfer_comp(cts_dev);
    if (ret != 0) {
        cts_err("Wait sfctrl ready failed %d", ret);
        return ret;
    }

    cts_read_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_CRC_RESULT),
        (u8 *) crc, sizeof(*crc), false);

    return 0;
}

int sfctrlv2_calc_flash_crc(const struct cts_device *cts_dev,
        u32 flash_addr, size_t size, u32 *crc)
{
    int ret;
    u8 val;

    cts_dbg("Calc crc from flash 0x%06x size %zu", flash_addr, size);

    cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_FLASH_ADDR),
        (u8 *) &flash_addr, sizeof(flash_addr), false);
    cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_DATA_LENGTH),
        (u8 *) &size, 4, false);
    val = 1;
    cts_write_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_FLASH_CRC_START),
        &val, 1, false);

    ret = wait_sfctrl_xfer_comp(cts_dev);
    if (ret != 0) {
        cts_err("Wait sfctrl ready failed %d", ret);
        return ret;
    }

    cts_read_hw_reg(cts_dev, sfctrl_reg_addr(cts_dev, rSFCTRLv2_CRC_RESULT),
        (u8 *) crc, sizeof(*crc), false);

    return 0;
}

const struct cts_sfctrl_ops cts_sfctrlv2_ops = {
    .rdid = sfctrlv2_rdid,
    .se = sfctrlv2_se,
    .be = sfctrlv2_be,
    .read = sfctrlv2_read,
    .read_to_sram = sfctrlv2_read_flash_to_sram,
    .program = sfctrlv2_pp,
    .program_from_sram = sfctrlv2_program_flash_from_sram,
    .calc_sram_crc = sfctrlv2_calc_sram_crc,
    .calc_flash_crc = sfctrlv2_calc_flash_crc,
};
