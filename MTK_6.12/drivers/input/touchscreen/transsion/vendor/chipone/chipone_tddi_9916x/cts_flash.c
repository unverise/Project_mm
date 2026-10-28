#define LOG_TAG         "Flash"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sfctrl.h"
#include "cts_strerror.h"
#include "cts_transfer.h"
#include "cts_flash.h"

/* NOTE: double check command sets and memory organization when you add
 * more flash chips.  This current list focusses on newer chips, which
 * have been converging on command sets which including JEDEC ID.
 */
static const struct cts_flash cts_flashes[] = {
    /* Winbond */
    { "Winbond,W25Q10EW",
      0xEF6011, 0x100, 0x1000, 0x8000, 0x20000},
    { "Winbond,W25Q20EW",
      0xEF6012, 0x100, 0x1000, 0x8000, 0x40000},
    { "Winbond,W25Q40EW",
      0xEF6013, 0x100, 0x1000, 0x8000, 0x80000},
    { "Winbond,W25Q20BW",
      0xEF5012, 0x100, 0x1000, 0x8000, 0x40000},
    { "Winbond,W25Q40BW",
      0xEF5013, 0x100, 0x1000, 0x8000, 0x80000},

    /* Giga device */
    { "Giga,GD25LQ10B",
      0xC86011, 0x100, 0x1000, 0x8000, 0x20000},
    { "Giga,GD25LD20CEIGR",
      0xC86012, 0x100, 0x1000, 0x8000, 0x40000},
    { "Giga,GD25LD40CEIGR",
      0xC86013, 0x100, 0x1000, 0x8000, 0x80000},

    /* Macronix */
    { "Macronix,MX25U1001E",
      0xC22531,  0x20, 0x1000, 0x10000, 0x20000},
    { "Macronix,MX25R2035F",
      0xC22812, 0x100, 0x1000,  0x8000, 0x40000},
    { "Macronix,MX25R4035F",
      0xC22813, 0x100, 0x1000,  0x8000, 0x80000},

    /* Puya-Semi */
    { "Puya-Semi,P25Q20LUVHIR",
      0x856012, 0x100, 0x1000, 0x8000, 0x40000},
    { "Puya-Semi,P25Q40LUXHIR",
      0x856013, 0x100, 0x1000, 0x8000, 0x80000},
    { "Puya-Semi,P25Q11L",
      0x854011, 0x100, 0x1000, 0x8000, 0x20000},
    { "Puya-Semi,P25Q21L",
      0x854012, 0x100, 0x1000, 0x8000, 0x40000},
    { "Puya-Semi,P25T22L",
      0x854412, 0x100, 0x1000, 0x8000, 0x40000},


    /* Boya */
    { "Boya,BY25D10",
      0x684011, 0x100, 0x1000, 0x8000, 0x20000},

    /* EON */
    { "EoN,EN25S20A",
      0x1C3812, 0x100, 0x1000, 0x8000, 0x40000},

    /* XTXTECH */
    { "XTX,XT25Q01",
      0x0B6011, 0x100, 0x1000,      0, 0x20000},
    { "XTX,XT25Q02",
      0x0B6012, 0x100, 0x1000,      0, 0x40000},

    /* Xinxin-Semi */
    { "Xinxin-Semi,XM25QU20B",
      0x205012, 0x100, 0x1000, 0x8000, 0x40000},
    { "Xinxin-Semi,XM25QU40B",
      0x205013, 0x100, 0x1000, 0x8000, 0x80000},

    { "KangYong,XK25Q20T",
      0xEB6012, 0x100, 0x1000, 0x8000, 0x40000},

    {"FuDan,FM25W04",
      0x1328A1, 0x100, 0x1000, 0x8000, 0x40000},
    {"FuDan,FM25W04",
      0x136085, 0x100, 0x1000, 0x8000, 0x40000},
    {"PQ40,L2EJ",
      0x136085, 0x100, 0x1000, 0x8000, 0x40000},
};

static const struct cts_flash *find_flash_by_jedec_id(u32 jedec_id)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cts_flashes); i++) {
        if (cts_flashes[i].jedec_id == jedec_id) {
            return &cts_flashes[i];
        }
    }

    return NULL;
}

int probe_flash(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Probe flash");

    if (cts_dev->hwdata->sfctrl->ops->rdid != NULL) {
        u32 id;

        cts_info("Read JEDEC ID");

        cts_enter_program_mode(cts_dev);

        ret = cts_dev->hwdata->sfctrl->ops->rdid(cts_dev, &id);
        if (ret) {
            cts_err("Read JEDEC ID failed %d(%s)",
                ret, cts_strerror(ret));
            return ret;
        }

        cts_dev->flash = find_flash_by_jedec_id(id);
        if (cts_dev->flash == NULL) {
            cts_err("Unknown JEDEC ID: %06x", id);
            return -ENODEV;
        }

        cts_info("Flash type: '%s'", cts_dev->flash->name);
        return 0;
    } else {
        cts_err("Probe flash with sfctrl->ops->rdid == NULL");
        return -ENOTSUPP;
    }
}

/** Make sure sector addr is sector aligned && < flash total size */
static int erase_sector_retry(const struct cts_device *cts_dev,
        u32 sector_addr, int retry)
{
    int ret, retries;

    cts_info("  Erase sector 0x%06x", sector_addr);

    retries = 0;
    do {
        retries++;
        ret = cts_dev->hwdata->sfctrl->ops->se(cts_dev, sector_addr);
        if (!ret)
            break;
        cts_err("Erase sector 0x%06x failed %d(%s) retries %d",
            sector_addr, ret, cts_strerror(ret), retries);
    } while (retries < retry);

    return ret;
}

/** Make sure sector addr is sector aligned && < flash total size */
static inline int erase_sector(const struct cts_device *cts_dev,
        u32 sector_addr)
{
    return erase_sector_retry(cts_dev, sector_addr, CTS_RETRY);
}

/** Make sure block addr is block aligned && < flash total size */
static int erase_block_retry(const struct cts_device *cts_dev,
        u32 block_addr, int retry)
{
    int ret, retries;

    cts_info("  Erase block  0x%06x", block_addr);

    retries = 0;
    do {
        retries++;

        ret = cts_dev->hwdata->sfctrl->ops->be(cts_dev, block_addr);
        if (!ret)
            break;

        cts_err("Erase block 0x%06x failed %d(%s) retries %d",
            block_addr, ret, cts_strerror(ret), retries);
    } while (retries < retry);

    return ret;
}

/** Make sure block addr is block aligned && < flash total size */
static inline int erase_block(const struct cts_device *cts_dev,
        u32 block_addr)
{
    return erase_block_retry(cts_dev, block_addr,
        CTS_RETRY);
}

int cts_prepare_flash_operation(struct cts_device *cts_dev)
{
    u32 hwid;
    u32 state;
    int retries;
    int ret = 0;
    u8  diva = 0;
    u8 val;

    cts_info("Prepare for flash operation");
    hwid = cts_dev->hwdata->hwid;

    if (hwid == CTS_DEV_HWID_ICNL9911D || hwid == CTS_DEV_HWID_ICNL9911C ||
        hwid == CTS_DEV_HWID_ICNL9951) {
        if(!cts_is_program_mode(cts_dev))
            cts_enter_program_mode(cts_dev);

        ret = cts_read_hw_reg(cts_dev,
                cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_CLK_DIV_CFG,
                &diva, 1, false);
        if (ret) {
            cts_warn("Read DIVA failed %d(%s)", ret, cts_strerror(ret));
        } else {
            if (diva == 0x0C)
                return ret;

            diva = 0x0C;
            ret = cts_write_hw_reg(cts_dev,
                    cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_CLK_DIV_CFG,
                    &diva, 1, false);
            if (ret) {
                cts_err("Write DIVA failed %d(%s)", ret, cts_strerror(ret));
                return ret;
            }

            /* Reset SFCTL */
            val = CTS_RST_BIT & CTS_RST_BIT_MASK;
            ret = cts_write_hw_reg(cts_dev,
                    cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_RESET_CONFIG,
                    &val, 1, false);
            if (ret) {
                cts_err("Reset sfctl failed %d(%s)",
                    ret, cts_strerror(ret));
                return ret;
            }

            /* wait sfctl init done */
            retries = 0;
            do {
                ret = cts_read_hw_reg(cts_dev,
                        cts_dev->hwdata->sfctl_base + CTS_SFCTRL_HW_STATE,
                        (u8 *)&state, 4, false);
                if (!ret && (state & CTS_SF_INIT_DONE))
                    break;

                mdelay(5);
            } while (++retries < 200);

            if (!(state & CTS_SF_INIT_DONE)) {
                cts_warn("Wait SF_INIT_DONE failed %d(%s)", ret, cts_strerror(ret));
                ret = -EINVAL;
            }
        }
    }
    return ret;
}

int cts_post_flash_operation(struct cts_device *cts_dev)
{
    u32 hwid;
    u8 div;

    cts_info("Post flash operation");

    hwid = cts_dev->hwdata->hwid;
    if (hwid == CTS_DEV_HWID_ICNL9911 || hwid == CTS_DEV_HWID_ICNL9911C \
      ||hwid == CTS_DEV_HWID_ICNL9951) {
        div = 4;
        return cts_write_hw_reg(cts_dev,
                cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_CLK_DIV_CFG,
                &div, 1, false);
    }
    return 0;
}

int _cts_read_flash(const struct cts_device *cts_dev,
        u32 flash_addr, void *dst, size_t size)
{
    struct cts_sfctrl *sfctrl;
    const struct cts_flash *flash;
    int ret;

    cts_info("Read from 0x%06x size %zu", flash_addr, size);

    sfctrl = cts_dev->hwdata->sfctrl;
    flash = cts_dev->flash;

    if (flash == NULL ||
        sfctrl == NULL ||
        sfctrl->ops == NULL ||
        sfctrl->ops->read == NULL) {
        cts_err("Read not supported");
        return -ENOTSUPP;
    }

    if (flash_addr > flash->total_size) {
        cts_err("Read from 0x%06x > flash size 0x%06zx",
            flash_addr, flash->total_size);
        return -EINVAL;
    }
    size = min(size, flash->total_size - flash_addr);

    if(sfctrl->xchg_sram_size <= 0 || sfctrl->xchg_sram_size > 0x40)
        sfctrl->xchg_sram_size = CFG_MAX_SRAM_XCHG_SIZE;

    cts_info("Read actually from 0x%06x size %zu", flash_addr, size);

    while (size) {
        size_t l;

        l = min(sfctrl->xchg_sram_size, (u32)size);

        ret = sfctrl->ops->read(cts_dev, flash_addr, dst, l);
        if(ret < 0) {
            cts_err("Read from 0x%06x size %zu failed %d(%s)",
                flash_addr, size, ret, cts_strerror(ret));
            return ret;
        }

        dst += l;
        size -= l;
        flash_addr += l;
    }

    return 0;
}

int cts_read_flash(const struct cts_device *cts_dev,
        u32 flash_addr, void *dst, size_t size)
{
    int retry = 0;
    int ret = 0;

    do {
            ret = _cts_read_flash(cts_dev, flash_addr, dst, size);
            if (!ret)
                    break;
            mdelay(CTS_M_DELAY);
    } while (retry < CTS_RETRY);

    return ret;
}

int cts_read_flash_to_sram_retry(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, u8 *data, size_t size, int retry)
{
    const struct cts_sfctrl *sfctrl;
    const struct cts_flash *flash;
    int ret, retries;

    cts_info("Read from 0x%06x to sram 0x%06x size %zu",
        flash_addr, sram_addr, size);

    sfctrl = cts_dev->hwdata->sfctrl;
    flash  = cts_dev->flash;

    if (flash == NULL ||
        sfctrl == NULL ||
        sfctrl->ops == NULL ||
        sfctrl->ops->read_to_sram == NULL) {
        cts_err("Read to sram not supported");
        return -ENOTSUPP;
    }

    if (flash_addr > flash->total_size) {
        cts_err("Read to sram from 0x%06x > flash size 0x%06zx",
            flash_addr, flash->total_size);
        return -EINVAL;
    }
    size = min(size, flash->total_size - flash_addr);

    cts_info("Read to sram actually from 0x%06x size %zu",
        flash_addr, size);

    retries = 0;
    do {
        retries++;
        ret = sfctrl->ops->read_to_sram(cts_dev,
           flash_addr, sram_addr, data, size);
        if(!ret)
            break;

        cts_err("Read from 0x%06x to sram 0x%06x size %zu "
                "failed %d(%s) retries %d",
            flash_addr, sram_addr, size,
            ret, cts_strerror(ret), retries);
        mdelay(CTS_M_DELAY);
    } while (retries < retry);

    return ret;
}

static inline int cts_read_flash_to_sram(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, u8 *data, size_t size)
{
    return cts_read_flash_to_sram_retry(cts_dev,
            flash_addr, sram_addr, data, size, CTS_RETRY);
}

int cts_program_flash(const struct cts_device *cts_dev,
        u32 flash_addr, const void *src, size_t size)
{
    const struct cts_sfctrl *sfctrl;
    const struct cts_flash *flash;
    int ret;

    cts_info("Program to 0x%06x size %zu", flash_addr, size);

    sfctrl = cts_dev->hwdata->sfctrl;
    flash  = cts_dev->flash;

    if (flash == NULL ||
        sfctrl == NULL ||
        sfctrl->ops == NULL ||
        sfctrl->ops->program == NULL) {
        cts_err("Program not supported");
        return -ENOTSUPP;
    }

    if (flash_addr >= flash->total_size) {
        cts_err("Program from 0x%06x >= flash size 0x%06zx",
            flash_addr, flash->total_size);
        return -EINVAL;
    }
    size = min(size, flash->total_size - flash_addr);

    cts_info("Program actually to 0x%06x size %zu", flash_addr, size);

    while (size) {
        size_t l, offset;

        l = min(flash->page_size, size);
        offset = flash_addr & (flash->page_size - 1);

        if (offset) {
            l = min(flash->page_size - offset, l);
        }

        ret = sfctrl->ops->program(cts_dev, flash_addr, src, l);
        if(ret) {
            cts_err("Program to 0x%06x size %zu failed %d(%s)",
                flash_addr, l, ret, cts_strerror(ret));
            return ret;
        }

        src += l;
        size -= l;
        flash_addr += l;
    }

    return 0;
}

int cts_program_flash_from_sram(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, size_t size)
{
    const struct cts_sfctrl *sfctrl;
    const struct cts_flash *flash;
    int ret;

    cts_info("Program to 0x%06x from sram 0x%06x size %zu",
        flash_addr, sram_addr, size);

    sfctrl = cts_dev->hwdata->sfctrl;
    flash  = cts_dev->flash;

    if (flash == NULL ||
        sfctrl == NULL ||
        sfctrl->ops == NULL ||
        sfctrl->ops->program_from_sram == NULL) {
        cts_err("Program from sram not supported");
        return -ENOTSUPP;
    }

    if (flash_addr >= flash->total_size) {
        cts_err("Program from 0x%06x >= flash size 0x%06zx",
            flash_addr, flash->total_size);
        return -EINVAL;
    }
    size = min(size, flash->total_size - flash_addr);

    cts_info("Program actually to 0x%06x from sram 0x%06x size %zu",
        flash_addr, sram_addr, size);

    while (size) {
        size_t l, offset;

        l = min(flash->page_size, size);
        offset = flash_addr & (flash->page_size - 1);

        if (offset) {
            l = min(flash->page_size - offset, l);
        }

        ret = sfctrl->ops->program_from_sram(cts_dev,
            flash_addr, sram_addr, l);
        if(ret) {
            cts_err("Program to 0x%06x from sram 0x%06x size %zu "
                   "failed %d(%s)",
                flash_addr, sram_addr, l, ret, cts_strerror(ret));
            return ret;
        }

        size -= l;
        flash_addr += l;
        sram_addr  += l;
    }

    return 0;
}

int cts_erase_flash(const struct cts_device *cts_dev, u32 addr, size_t size)
{
    const struct cts_sfctrl *sfctrl;
    const struct cts_flash *flash;
    u8 *data_head = NULL;
    u8 *data_tail = NULL;
    int head_data_len, tail_data_len;
    u32 start_addr, addr_cursor;
    size_t real_size, size_cnt;
    int ret;

    cts_info("Erase from 0x%06x size %zu", addr, size);

    sfctrl = cts_dev->hwdata->sfctrl;
    flash  = cts_dev->flash;

    if (flash == NULL ||
        sfctrl == NULL || sfctrl->ops == NULL ||
        sfctrl->ops->se == NULL || sfctrl->ops->be == NULL ||
        flash == NULL) {
        cts_err("falsh not initialized\n");
        return -EINVAL;
    }

    /* Addr and size MUST sector aligned */
    start_addr = rounddown(addr, flash->sector_size);
    head_data_len = addr - start_addr;
    real_size = roundup(size + head_data_len, flash->sector_size);
    if (start_addr + real_size > flash->total_size) {
        cts_err("Erase from 0x%06x, length:%x, bigger than flash size 0x%06zx",
            start_addr, real_size, flash->total_size);
        return -EINVAL;
    }
    tail_data_len = real_size - size - head_data_len;

    /* step 1: readback the data that is beyond the area to erase if any */
    if (head_data_len) {
        cts_info("readback data in the sector, data length(%d) before target area\n",
            head_data_len);
        ret = cts_read_flash_to_sram(cts_dev, start_addr, 0, data_head, flash->sector_size);
        if (ret) {
            cts_err("readback from flash failed, flash addr:%x\n", start_addr);
            return ret;
        }
    }
    if ((real_size > flash->sector_size || head_data_len == 0) &&
        tail_data_len) {
        cts_info("readback data in the sector, data length(%d) after target area\n",
            tail_data_len);
        ret = cts_read_flash_to_sram(cts_dev, start_addr + real_size - flash->sector_size,
                0 + flash->sector_size, data_tail, flash->sector_size);
        if (ret) {
            cts_err("readback from flash failed, flash addr:%x\n",
                start_addr + real_size - flash->sector_size);
            return ret;
        }
    }

    cts_info("Erase actually from 0x%06x size %zu", start_addr, real_size);

    /* step 2: erase the target area, prefer to use block erase to speed up */
    addr_cursor = start_addr;
    size_cnt = real_size;
    if (flash->block_size) {
        while (addr_cursor != ALIGN(addr, flash->block_size) &&
               size_cnt >= flash->sector_size) {
            ret = erase_sector(cts_dev, addr_cursor);
            if (ret) {
                cts_err("Erase sector 0x%06x size 0x%04zx failed %d(%s)",
                    addr, flash->sector_size, ret, cts_strerror(ret));
                return ret;
            }
            addr_cursor += flash->sector_size;
            size_cnt -= flash->sector_size;
        }

        while (size_cnt >= flash->block_size) {
            cts_info("%s:%d, erase block from addr:0x%x\n", __func__, __LINE__, addr_cursor);
            ret = erase_block(cts_dev, addr_cursor);
            if (ret) {
                cts_err("Erase block 0x%06x size 0x%04zx failed %d(%s)",
                    addr_cursor, flash->block_size, ret, cts_strerror(ret));
                return ret;
            }
            addr_cursor += flash->block_size;
            size_cnt -= flash->block_size;
        }
    }

    while (size_cnt >= flash->sector_size) {
        ret = erase_sector(cts_dev, addr_cursor);
        if (ret) {
            cts_err("Erase sector 0x%06x size 0x%04zx failed %d(%s)",
                addr, flash->sector_size, ret, cts_strerror(ret));
            return ret;
        }
        addr_cursor += flash->sector_size;
        size_cnt -= flash->sector_size;
    }

    /* step 3: writeback the data that is beyond the area to erase if any */
    if (head_data_len) {
        ret = cts_program_flash_from_sram(cts_dev, start_addr, 0, head_data_len);
        if (ret) {
            cts_err("writeback to flash failed, flash addr:%x\n", start_addr);
            return ret;
        }
    }

    if (tail_data_len) {
        if (real_size > flash->sector_size || !head_data_len)
            ret = cts_program_flash_from_sram(cts_dev,
                    start_addr + real_size - tail_data_len,
                    flash->sector_size * 2 - tail_data_len,
                    tail_data_len);
        else
            ret = cts_program_flash_from_sram(cts_dev,
                    start_addr + real_size - tail_data_len,
                    flash->sector_size - tail_data_len,
                    tail_data_len);
        if (ret) {
            cts_err("writeback to flash failed, flash addr:%x\n",
                start_addr + real_size - tail_data_len);
            return ret;
        }
    }
    return 0;
}

