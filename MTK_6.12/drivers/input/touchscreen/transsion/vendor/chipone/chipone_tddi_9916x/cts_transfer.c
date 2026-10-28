#define LOG_TAG "Transfer"

#include "cts_platform.h"
#include "cts_core.h"
#include "cts_config.h"
#include "cts_tcs.h"
#include "cts_transfer.h"

#include "cts_drw.h"
#include "cts_prw.h"


int get_unaligned_bytes(u32 addr, u32 len)
{
    int unaligned_bytes = 0;

    unaligned_bytes = (CTS_WORD_SIZE - (addr % CTS_WORD_SIZE)) % CTS_WORD_SIZE;

    /* Take the rest of the bytes as unaligned after cutting off the unaligned bytes
     * when the length of the data is shortor than CTS_WORD_SIZE.
     */
    if (len - unaligned_bytes < CTS_WORD_SIZE)
        unaligned_bytes = len;

    return unaligned_bytes;
}

static int cts_read_bytes(const struct cts_device *cts_dev,
    u32 addr, u8 *data, u32 len, bool force_drw)
{
    int ret = 0;
    int i;

    for (i = 0; i < CTS_RETRY; i++) {
        if(cts_is_sram_mode(cts_dev) && !force_drw) {
            ret = cts_tcs_read_data(cts_dev, addr, data, len);
            break;  // already retried in tcs api
        } else if (cts_dev->hwdata->spi_protocol == CTS_SPI_DRW ||
            cts_dev->hwdata->i2c_protocol == CTS_I2C_DRW) {
#ifdef CTS_DRW_PROTOCOL
            if (cts_dev->bus_type == CTS_BUS_SPI)
                ret = cts_spi_drw_read(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
            else if (cts_dev->bus_type == CTS_BUS_I2C)
                ret = cts_i2c_drw_read(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
            else if (cts_dev->bus_type == CTS_BUS_I3C)
                ret = cts_i3c_drw_read(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
            else
                cts_err("unkown bus type: %d\n", cts_dev->bus_type);
#else
            cts_warn("trying to use DRW, but DRW is not enabled!!\n");
#endif
        } else if (cts_dev->hwdata->spi_protocol == CTS_SPI_DRW ||
            cts_dev->hwdata->i2c_protocol == CTS_I2C_PRW) {
            if (force_drw) {
                cts_err("force to use DRW protocol, but DRW not defined\n");
                return -EPERM;
            }

#ifdef CTS_PRW_PROTOCOL
            if (cts_dev->bus_type == CTS_BUS_SPI)
                ret = cts_spi_prw_read(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
            else if (cts_dev->bus_type == CTS_BUS_I2C)
                ret = cts_i2c_prw_read(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
            else if (cts_dev->bus_type == CTS_BUS_I3C)
                ret = cts_i3c_prw_read(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
            else
                cts_err("unkown bus type: %d\n", cts_dev->bus_type);
#else
            cts_warn("trying to use PRW, but PRW is not enabled!!\n");
#endif
        }
        if(!ret)
            break;
        mdelay(CTS_M_DELAY);
    }

    return ret;
}

int cts_read(const struct cts_device *cts_dev, u32 addr, u8 *data, u32 len,
    bool force_drw)
{
    int unaligned_bytes = 0;
    int i, ret;

    if (!cts_dev || !data || !len) {
        cts_err("%s(line:%d), invalid parameter!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (len > CTS_MAX_READ_LEN) {
        cts_err("read too long, length: %d, return directly!\n", len);
        return -EINVAL;
    }

    if(len < CTS_WORD_SIZE) {
        for(i = 0; i < len; i++) {
            ret = cts_read_bytes(cts_dev, addr + i, data + i, 1, force_drw);
            if (ret)
                cts_err("read sram failed, addr:%x, data:0x%02x\n",
                    addr, *(data + i));
        }
    } else {
        unaligned_bytes = get_unaligned_bytes(addr, len);
        if (unaligned_bytes) {
            for(i = 0; i < unaligned_bytes; i++) {
                ret = cts_read_bytes(cts_dev, addr + i, data + i, 1, force_drw);
                if (ret) {
                    cts_err("read sram failed, addr:%x, data:0x%02x\n",
                        addr + i, *(data + i));
                        goto out;
                }
            }
        }

        if (len - unaligned_bytes) {
            ret = cts_read_bytes(cts_dev, addr + unaligned_bytes,
                    data + unaligned_bytes, len - unaligned_bytes, force_drw);
            if (ret)
                cts_err("read sram failed, addr:%x, data:0x%02x\n",
                    addr + unaligned_bytes, *(data + unaligned_bytes));
        }
    }

out:
    return ret;
}

int cts_read_sram(const struct cts_device *cts_dev, u32 addr, u8 *data, u32 len,
    bool force_drw)
{
    return cts_read(cts_dev, addr, data, len, force_drw);
}

int cts_read_hw_reg(const struct cts_device *cts_dev,
    u32 addr, u8 *data, u32 len, bool force_drw)
{
    return cts_read(cts_dev, addr, data, len, force_drw);
}

static int cts_read_bytes_try_protocols(const struct cts_device *cts_dev,
    u32 addr, u8 *data, u32 len)
{
    int ret = -EPERM;

    /* limitation: it may read the incorrect data when it meets
     * all of the following conditions.
     * a. the device supports prw only
     * b. drw and prw is configured at the same time
     * c. the bus type is i2c
     */
#ifdef CTS_DRW_PROTOCOL
    cts_info("%s:line:%d DRW ENABLED, bus_type:%d\n", __func__, __LINE__,
        cts_dev->bus_type);
    if (cts_dev->bus_type == CTS_BUS_SPI)
        ret = cts_spi_drw_read(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
    else if (cts_dev->bus_type == CTS_BUS_I2C)
        ret = cts_i2c_drw_read(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
    else if (cts_dev->bus_type == CTS_BUS_I3C)
        ret = cts_i3c_drw_read(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
    else
        cts_err("line:%d,unkown bus type: %d\n", __LINE__, cts_dev->bus_type);
    if (!ret)
        return ret;
#endif

#ifdef CTS_PRW_PROTOCOL
    cts_info("%s:line:%d PRW ENABLED, bus_type:%d\n", __func__, __LINE__,
        cts_dev->bus_type);
    if (cts_dev->bus_type == CTS_BUS_SPI)
        ret = cts_spi_prw_read(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
    else if (cts_dev->bus_type == CTS_BUS_I2C)
        ret = cts_i2c_prw_read(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
    else if (cts_dev->bus_type == CTS_BUS_I3C)
        ret = cts_i3c_prw_read(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
    else
        cts_err("unkown bus type: %d\n", cts_dev->bus_type);
#endif

    cts_info("%s:line:%d ret: %d\n", __func__, __LINE__, ret);
    return ret;
}

int cts_read_try_protocols(const struct cts_device *cts_dev,
    u32 addr, u8 *data, u32 len)
{
    int unaligned_bytes = 0;
    int i, ret;

    if (!cts_dev || !data || !len) {
        cts_err("%s(line:%d), invalid parameter!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (len > CTS_MAX_READ_LEN) {
        cts_err("read too long, length: %d, return directly!\n", len);
        return -EINVAL;
    }

    if(len < CTS_WORD_SIZE) {
        for(i = 0; i < len; i++) {
            ret = cts_read_bytes_try_protocols(cts_dev, addr + i, data + i, 1);
            if (ret)
                cts_err("read sram failed, addr:%x, data:0x%02x, len:%d\n",
                    addr, *(data + i), len);
        }
    } else {
        unaligned_bytes = get_unaligned_bytes(addr, len);
        if (unaligned_bytes) {
            for(i = 0; i < unaligned_bytes; i++) {
                ret = cts_read_bytes_try_protocols(cts_dev, addr + i, data + i, 1);
                if (ret) {
                    cts_err("read sram failed, addr:%x, data:0x%02x, len:%d\n",
                        addr, *(data + i), len);
                    goto out;
                }
            }
        }

        if (len - unaligned_bytes) {
            ret = cts_read_bytes_try_protocols(cts_dev, addr + unaligned_bytes,
                    data + unaligned_bytes, len - unaligned_bytes);
            if (ret)
                cts_err("read sram failed, addr:%x, data:0x%08x, len:%d\n",
                    addr + unaligned_bytes, *(data + unaligned_bytes),
                    len - unaligned_bytes);
        }
    }

out:
    return ret;
}

static int cts_write_bytes(const struct cts_device *cts_dev, u32 addr,
    const u8 *data, u32 len, enum cts_device_type device_type,
    bool force_drw)
{
    int ret = -EPERM;
    int i;

    for (i = 0; i < CTS_RETRY; i++) {
        if(cts_is_sram_mode(cts_dev) && !force_drw) {
            if (device_type == CTS_TP)
                ret = cts_tcs_write_tp_data(cts_dev, addr, data, len);
            else if (device_type == CTS_DDI)
                ret = cts_tcs_write_ddi_reg(cts_dev, addr, data, len);
            break;  // already retried in tcs api
        } else if (cts_dev->hwdata->spi_protocol == CTS_SPI_DRW ||
            cts_dev->hwdata->i2c_protocol == CTS_I2C_DRW) {
#ifdef CTS_DRW_PROTOCOL
            if (cts_dev->bus_type == CTS_BUS_SPI)
                ret = cts_spi_drw_write(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
            else if (cts_dev->bus_type == CTS_BUS_I2C)
                ret = cts_i2c_drw_write(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
            else if (cts_dev->bus_type == CTS_BUS_I3C)
                ret = cts_i3c_drw_write(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
            else
                cts_err("unkown bus type: %d\n", cts_dev->bus_type);
#else
            cts_warn("trying to use DRW, but DRW is not enabled!!\n");
#endif
        } else if (cts_dev->hwdata->spi_protocol == CTS_SPI_PRW ||
            cts_dev->hwdata->i2c_protocol == CTS_I2C_PRW) {
            if (force_drw)
                cts_warn("force to use DRW protocol, but DRW not defined\n");

#ifdef CTS_PRW_PROTOCOL
            if (cts_dev->bus_type == CTS_BUS_SPI)
                ret = cts_spi_prw_write(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
            else if (cts_dev->bus_type == CTS_BUS_I2C)
                ret = cts_i2c_prw_write(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
            else if (cts_dev->bus_type == CTS_BUS_I3C)
                ret = cts_i3c_prw_write(cts_dev->pdata, cts_dev->hwdata, addr, data, len);
            else
                cts_err("unkown bus type: %d\n", cts_dev->bus_type);
#else
            cts_warn("trying to use PRW, but PRW is not enabled!!\n");
#endif
        }
        if(!ret)
            break;
        mdelay(CTS_M_DELAY);
    }

    return ret;
}

int cts_write(const struct cts_device *cts_dev, u32 addr,
    const u8 *data, u32 len, enum cts_device_type device_type,
    bool force_drw)
{
    int unaligned_bytes = 0;
    int i, ret;

    if (!cts_dev || !data || !len) {
        cts_err("%s(line:%d), invalid parameter!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (len > CTS_MAX_WRITE_LEN) {
        cts_err("write too long, length: %d, return directly!\n", len);
        return -EINVAL;
    }

    if(len < CTS_WORD_SIZE) {
        for(i = 0; i < len; i++) {
            ret = cts_write_bytes(cts_dev, addr + i, data + i, 1,
                    device_type, force_drw);
            if (ret)
                cts_err("read sram failed, addr:%x, data:0x%02x\n",
                    addr + i, *(data + i));
        }
    } else {
        unaligned_bytes = get_unaligned_bytes(addr, len);
        if (unaligned_bytes) {
            for(i = 0; i < unaligned_bytes; i++) {
                ret = cts_write_bytes(cts_dev, addr + i, data + i, 1,
                        device_type, force_drw);
                if (ret) {
                    cts_err("read sram failed, addr:%x, data:0x%02x\n",
                        addr + i, *(data + i));
                        goto out;
                }
            }
        }

        if (len - unaligned_bytes) {
            ret = cts_write_bytes(cts_dev, addr + unaligned_bytes,
                    data + unaligned_bytes, len - unaligned_bytes, device_type,
                    force_drw);
            if (ret)
                cts_err("read sram failed, addr:%x, data:0x%02x\n",
                    addr + unaligned_bytes, *(data + unaligned_bytes));
        }
    }

out:
    return ret;
}

int cts_transfer(const struct cts_device *cts_dev, u32 addr,
    const u8 *data, u32 len, u8 * rdata, u32 rlen)
{
    if (rlen == 0)
        return cts_write(cts_dev, addr, data, len, CTS_TP, false);
    else
        return cts_read(cts_dev, addr, rdata, rlen, false);
}

int cts_write_sram(const struct cts_device *cts_dev, u32 addr,
    const u8 *data, u32 len, bool force_drw)
{
    if (cts_is_sram_mode(cts_dev) && !force_drw) {
        cts_err("write sram is permitted in program mode only\n");
        return -EPERM;
    }

    return cts_write(cts_dev, addr, data, len, CTS_TP, force_drw);
}

int cts_write_hw_reg(const struct cts_device *cts_dev, u32 addr,
    const u8 *data, u32 len, bool force_drw)
{
    return cts_write(cts_dev, addr, data, len, CTS_TP, force_drw);
}

int cts_write_ddi_reg(const struct cts_device *cts_dev, u32 addr,
    const u8 *data, u32 len, bool force_drw)
{
    return cts_write(cts_dev, addr, data, len, CTS_DDI, force_drw);
}