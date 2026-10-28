#define LOG_TAG         "Krang"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_transfer.h"
#include "cts_sfctrl.h"
#include "cts_krang.h"


static int cts_rcp_read_slave_bytes(const struct cts_device *cts_dev,
    u32 src_addr, u32 dst_addr, u8 *data, u32 len, unsigned long rcp_flag,
    bool force_drw)
{
    unsigned long dma_ctrl = 0;
    unsigned long dma_len_reg = 0;
    u8  buf[4] = {};
    int retry = 0;
    int ret = 0;
    int read_len = len;

    if(read_len == 0)
        return 0;

    /* dma_len_reg collects the number of words to read, increase the
    * len of byte to word_size to get the correct reg value in dma_len_reg
    * calculation.
    */
    if (read_len == 1)
        read_len = CTS_WORD_SIZE;

    /* set casc flag and length */
    dma_len_reg = (read_len / CTS_WORD_SIZE) | rcp_flag;
    put_unaligned_le32((u32) dma_len_reg, buf);
    ret = cts_write_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_LEN,
        buf, CTS_WORD_SIZE, force_drw);
    if (ret) {
        cts_err("write CTS_KR_DMA1_LEN failed, ret:%d", ret);
        goto out;
    }

    /* dst addr of sram */
    put_unaligned_le32(dst_addr, buf);
    ret = cts_write_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_DST,
        buf, CTS_WORD_SIZE, force_drw);
    if (ret) {
        cts_err("write CTS_KR_DMA1_SRC failed, ret:%d", ret);
        goto out;
    }

    /* src addr in slave */
    put_unaligned_le32(src_addr, buf);
    ret = cts_write_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_SRC,
        buf, CTS_WORD_SIZE, force_drw);
    if (ret) {
        cts_err("write CTS_KR_DMA1_SRC failed, ret:%d", ret);
        goto out;
    }

    /* trigger RCP */
    set_bit(CTS_KR_RCP_TRIG, &dma_ctrl);
    put_unaligned_le32((u32) dma_ctrl, buf);
    ret = cts_write_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_CTL,
        buf, CTS_WORD_SIZE, force_drw);
    if (ret) {
        cts_err("write CTS_KR_DMA1_CTL failed, ret:%d", ret);
        goto out;
    }

    /* waiting for RCP complete */
    set_bit(CTS_KR_RCP_DONE, &dma_ctrl);
    do {
        ret = cts_read_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_CTL,
                buf, CTS_WORD_SIZE, force_drw);
        if (ret)
            break;
        dma_ctrl = get_unaligned_le32(buf);
        mdelay(CTS_M_DELAY);
    } while (test_bit(CTS_KR_RCP_DONE, &dma_ctrl) && retry++ < 7);

    if (test_bit(CTS_KR_RCP_DONE, &dma_ctrl)) {
        ret = -EINVAL;
        goto out;
    }

    /* get the data from dst addr in sram */
    ret = cts_read_sram(cts_dev, dst_addr, data, len, force_drw);
    if (ret)
        cts_err("read sram failed, ret:%d", ret);

out:
    return ret;
}

static int _cts_read_slave(struct cts_device *cts_dev, u32 addr, u32 sram_addr,
    u8 *data, u32 len, unsigned long client_id)
{
    unsigned long rcp_flag = 0;

    bool force_drw = false;
    int unaligned_bytes = 0;
    int unaligned_bytes_tail = 0;
    int i, ret;

    if ((cts_dev->bus_type == CTS_BUS_SPI &&
        cts_dev->hwdata->spi_protocol == CTS_SPI_DRW) ||
        (cts_dev->bus_type == CTS_BUS_I2C &&
        cts_dev->hwdata->i2c_protocol == CTS_I2C_DRW))
        force_drw = true;

    if (cts_is_sram_mode(cts_dev) && !force_drw) {
        cts_err("KRANG is not accessable under sram boot with DRW disabled\n");
        return -EPERM;
    }

    if (len > CFG_MAX_SRAM_XCHG_SIZE) {
        cts_warn("read too long(%d bytes) from slave, ", len,
            "support 512 bytes at most for single transfer\n");
        len = CFG_MAX_SRAM_XCHG_SIZE;
    }

    set_bit(CTS_KR_RCP_TO_M, &rcp_flag);
    _assign_bit(CTS_KR_RCP_FROM_S1, &rcp_flag,
        test_bit(CTS_KR_BIT_MAP_SLAVE1, &client_id));
    _assign_bit(CTS_KR_RCP_FROM_S2, &rcp_flag,
        test_bit(CTS_KR_BIT_MAP_SLAVE2, &client_id));

    if (len < CTS_WORD_SIZE) {
        for (i = 0; i < len; i++) {
            ret = cts_rcp_read_slave_bytes(cts_dev, addr + i, sram_addr + i,
                    data + i, 1, (u32) rcp_flag, force_drw);
            if (ret) {
                cts_err("RCP read slave failed, client_id:%x, len:%d, ret:%d",
                    client_id, len, ret);
                break;
            }
        }
    } else {
        unaligned_bytes = get_unaligned_bytes(addr, len);
        if (unaligned_bytes) {
            for(i = 0; i < unaligned_bytes; i++) {
                ret = cts_rcp_read_slave_bytes(cts_dev, addr + i, sram_addr + i,
                    data + i, 1, (u32) rcp_flag, force_drw);
                if (ret) {
                    cts_err("RCP read slave failed, client_id:%x, len:%d, ret:%d",
                    client_id, len, ret);
                        goto out;
                }
            }
        }

        if (len - unaligned_bytes) {
            unaligned_bytes_tail = (len - unaligned_bytes) % CTS_WORD_SIZE;
            ret = cts_rcp_read_slave_bytes(cts_dev, addr + unaligned_bytes,
                    sram_addr + unaligned_bytes, data + unaligned_bytes,
                    len - unaligned_bytes - unaligned_bytes_tail,
                    (u32) rcp_flag, force_drw);
            if (ret)
                cts_err("RCP read slave failed, client_id:%x, len:%d, ret:%d",
                    client_id, len, ret);

            for (i = 0; i < unaligned_bytes_tail; i++) {
                ret = cts_rcp_read_slave_bytes(cts_dev, addr + len - unaligned_bytes_tail + i,
                    sram_addr + len - unaligned_bytes_tail + i,
                    data + len - unaligned_bytes_tail + i, 1, (u32) rcp_flag, force_drw);
                if (ret) {
                    cts_err("RCP read slave failed, client_id:%x, len:%d, ret:%d",
                    client_id, len, ret);
                        goto out;
                }
            }
        }
    }

out:
    return ret;
}

int cts_read_slave(struct cts_device *cts_dev, u32 addr,
    u8 *data, u32 len, unsigned long client_id)
{
    int read_cnt = 0;
    u32 max_buf_size;
    u32 sram_xchange_addr = cts_dev->hwdata->sfctrl->xchg_sram_base;
    int rx_len, ret;

    max_buf_size = min(CFG_MAX_SRAM_XCHG_SIZE, CTS_KR_MAX_LEN);
    rx_len = min(len, max_buf_size);

    do{
        ret = _cts_read_slave(cts_dev, addr + read_cnt, sram_xchange_addr + read_cnt,
                data + read_cnt, rx_len, client_id);
        if (ret) {
            cts_err("read slave failed, addr:%x, len:%d\n",
                addr + read_cnt, rx_len);
            break;
        }
        read_cnt += rx_len;
        rx_len = min(len - (u32) read_cnt , max_buf_size);
    } while (len > read_cnt);

    return ret;
}

/* cts_rcp_write_slave_bytes() - write long bytes to slave.
 * @cts_dev:    cts_dev has krang_base info
 * @src_addr:   sram address to copy from
 * @dst_addr:   slave address to write to
 * @data:       data to write to slave
 * @len:        number of bytes to write
 * @rcp_flag:   flag that indicate the data path
 * @force_drw: force to use drw even in normal mode
 * @return:  return 0 on success
 */
static int cts_rcp_write_slave_bytes(const struct cts_device *cts_dev,
    u32 src_addr, u32 dst_addr, u8 *data, u32 len, unsigned long rcp_flag, bool force_drw)
{
    unsigned long dma_ctrl = 0;
    unsigned long dma_len_reg = 0;
    u8  buf[4] = {};
    int retry = 0;
    int ret = 0;

    if(len == 0)
        return 0;

    /* set casc flag and length */
    dma_len_reg = (len / CTS_WORD_SIZE) | rcp_flag;
    put_unaligned_le32((u32) dma_len_reg, buf);
    ret = cts_write_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_LEN,
        buf, CTS_WORD_SIZE, force_drw);
    if (ret) {
        cts_err("write CTS_KR_DMA1_LEN failed, ret:%d", ret);
        goto out;
    }

    /* dst addr of slave */
    put_unaligned_le32((u32) dst_addr, buf);
    ret = cts_write_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_DST,
        buf, CTS_WORD_SIZE, force_drw);
    if (ret) {
        cts_err("write CTS_KR_DMA1_SRC failed, ret:%d", ret);
        goto out;
    }

    /* src addr in sram */
    put_unaligned_le32((u32) src_addr, buf);
    ret = cts_write_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_SRC,
        buf, CTS_WORD_SIZE, force_drw);
    if (ret) {
        cts_err("write CTS_KR_DMA1_SRC failed, ret:%d", ret);
        goto out;
    }

    /* Write data to src_addr in sram, data maybe null when downloading fw. */
    if (data != NULL) {
        ret = cts_write_sram(cts_dev, src_addr, data, len, force_drw);
        if (ret) {
            cts_err("write data to sram(addr:0x%x) failed, ret:%d", src_addr, ret);
            goto out;
        }
    }

    /* trigger RCP */
    set_bit(CTS_KR_RCP_TRIG, &dma_ctrl);
    put_unaligned_le32((u32) dma_ctrl, buf);
    ret = cts_write_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_CTL,
        buf, CTS_WORD_SIZE, force_drw);
    if (ret) {
        cts_err("write CTS_KR_DMA1_CTL failed, ret:%d", ret);
        goto out;
    }

    /* waiting for RCP complete */
    set_bit(CTS_KR_RCP_DONE, &dma_ctrl);
    do {
        ret = cts_read_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_CTL,
                buf, CTS_WORD_SIZE, force_drw);
        if (ret)
            break;
        dma_ctrl = get_unaligned_le32(buf);
        mdelay(CTS_M_DELAY);
    } while (test_bit(CTS_KR_RCP_DONE, &dma_ctrl) && retry++ < 7);

    if (test_bit(CTS_KR_RCP_DONE, &dma_ctrl)) {
        cts_err("wait for RCP done timeout");
        ret = -EINVAL;
        goto out;
    }

out:
    return ret;
}

static int cts_tabw_write_slave_byte(const struct cts_device *cts_dev,
    u32 src_addr, u32 dst_addr, u8 *data, u32 len, unsigned long tabw_flag,
    bool force_drw)
{
    unsigned long dma_ctrl = 0;
    unsigned long word_for_addr = 0;
    u8  buf[8] = {};
    int retry = 0;
    int ret = 0;

    /* set tabw flag and length */
    set_bit(CTS_TABW_SINGLE_BYTE_CTL, &tabw_flag);

    put_unaligned_le32(len, buf);
    ret = cts_write_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_LEN,
        buf, CTS_WORD_SIZE, force_drw);
    if (ret) {
        cts_err("write CTS_KR_DMA1_LEN failed, ret:%d", ret);
        goto out;
    }

    /* src addr of sram for data exchange, A + D mode */
    put_unaligned_le32(src_addr, buf);
    ret = cts_write_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_DST,
        buf, CTS_WORD_SIZE, force_drw);
    if (ret) {
        cts_err("write CTS_KR_DMA1_SRC failed, ret:%d", ret);
        goto out;
    }

    /* write the A + D word pair to sram */
    word_for_addr = dst_addr | tabw_flag;
    set_bit(CTS_TABW_SINGLE_BYTE_CTL, &word_for_addr);
    put_unaligned_le32((u32) word_for_addr, buf);
    buf[4] = *data;
    ret = cts_write_sram(cts_dev, src_addr, buf, CTS_WORD_SIZE * 2, force_drw);

    /* trigger RCP */
    set_bit(CTS_KR_TABW_TRIG, &dma_ctrl);
    put_unaligned_le32((u32) dma_ctrl, buf);
    ret = cts_write_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_CTL,
        buf, CTS_WORD_SIZE, force_drw);
    if (ret) {
        cts_err("write CTS_KR_DMA1_CTL failed, ret:%d", ret);
        goto out;
    }

    /* waiting for RCP complete */
    set_bit(CTS_KR_TABW_DONE, &dma_ctrl);
    do {
        ret = cts_read_hw_reg(cts_dev, cts_dev->hwdata->krang_base + CTS_KR_DMA1_CTL,
                buf, CTS_WORD_SIZE, force_drw);
        if (ret)
            break;
        dma_ctrl = get_unaligned_le32(buf);
        mdelay(CTS_M_DELAY);
    } while (test_bit(CTS_KR_TABW_DONE, &dma_ctrl) && retry++ < 7);

    if (test_bit(CTS_KR_TABW_DONE, &dma_ctrl))
        ret = -EINVAL;

out:
    return ret;
}

static int _cts_write_slave(struct cts_device *cts_dev, u32 src_addr, u32 dst_addr,
    u8 *data, u32 len, unsigned long client_id)
{
    unsigned long rcp_flag = 0;
    unsigned long tabw_flag = 0;

    bool force_drw = false;
    int unaligned_bytes = 0;
    int unaligned_bytes_tail = 0;
    int i, ret;

    if ((cts_dev->bus_type == CTS_BUS_SPI &&
        cts_dev->hwdata->spi_protocol == CTS_SPI_DRW) ||
        (cts_dev->bus_type == CTS_BUS_I2C &&
        cts_dev->hwdata->i2c_protocol == CTS_I2C_DRW))
        force_drw = true;

    if (cts_is_sram_mode(cts_dev)) {
        if (!force_drw) {
            cts_err("KRANG is not accessable under sram boot with DRW disabled\n");
            return -EPERM;
        } else if (len > CTS_WORD_SIZE) {
            cts_err("long write to slave is forbidden in sram boot!!!\n");
            return -EPERM;
        }
    }

    _assign_bit(CTS_KR_RCP_TO_M, &rcp_flag,
        test_bit(CTS_KR_BIT_MAP_MASTER, &client_id));
    _assign_bit(CTS_KR_RCP_TO_S1, &rcp_flag,
        test_bit(CTS_KR_BIT_MAP_SLAVE1, &client_id));
    _assign_bit(CTS_KR_RCP_TO_S2, &rcp_flag,
        test_bit(CTS_KR_BIT_MAP_SLAVE2, &client_id));
    _assign_bit(CTS_KR_TABW_TO_M, &tabw_flag,
        test_bit(CTS_KR_BIT_MAP_MASTER, &client_id));
    _assign_bit(CTS_KR_TABW_TO_S1, &tabw_flag,
        test_bit(CTS_KR_BIT_MAP_SLAVE1, &client_id));
    _assign_bit(CTS_KR_TABW_TO_S2, &tabw_flag,
        test_bit(CTS_KR_BIT_MAP_SLAVE2, &client_id));

    if (len < CTS_WORD_SIZE) {
        for (i = 0; i < len; i++) {
            ret = cts_tabw_write_slave_byte(cts_dev, src_addr + i, dst_addr + i,
                    data + i, 1, (u32) tabw_flag, force_drw);
            if (ret) {
                cts_err("tabw write slave failed, client_id:%x, len:%d, ret:%d",
                    client_id, len, ret);
                break;
            }
        }
    } else {
        unaligned_bytes = get_unaligned_bytes(dst_addr, len);
        if (unaligned_bytes) {
            for(i = 0; i < unaligned_bytes; i++) {
                ret = cts_tabw_write_slave_byte(cts_dev, src_addr + i,
                    dst_addr + i, data + i, 1, (u32) tabw_flag, force_drw);
                if (ret) {
                    cts_err("tabw write slave failed, client_id:%x, len:%d, ret:%d",
                    client_id, len, ret);
                        goto out;
                }
            }
        }

        if (len - unaligned_bytes) {
            unaligned_bytes_tail = (len - unaligned_bytes) % CTS_WORD_SIZE;
            ret = cts_rcp_write_slave_bytes(cts_dev, src_addr + unaligned_bytes,
                    dst_addr + unaligned_bytes, data + unaligned_bytes,
                    len - unaligned_bytes - unaligned_bytes_tail,
                    (u32) rcp_flag, force_drw);
            if (ret)
                cts_err("RCP write slave failed, client_id:%x, len:%d, ret:%d",
                    client_id, len, ret);

            for (i = 0; i < unaligned_bytes_tail; i++) {
                ret = cts_tabw_write_slave_byte(cts_dev,
                    src_addr + len - unaligned_bytes_tail + i,
                    dst_addr + len - unaligned_bytes_tail + i,
                    data + len - unaligned_bytes_tail + i, 1, (u32) tabw_flag,
                    force_drw);
                if (ret) {
                    cts_err("tabw write slave failed, client_id:%x, len:%d, ret:%d",
                    client_id, len, ret);
                        goto out;
                }
            }
        }
    }

out:
    return ret;
}

int cts_write_slave(struct cts_device *cts_dev, u32 addr,
    u8 *data, u32 len, unsigned long client_id)
{
#define SRAM_TEMP_ADDR      0x27E00
    u32 sram_addr = SRAM_TEMP_ADDR;
    u8 *data_ptr = NULL;
    int write_cnt = 0;
    int tx_len, ret;

    /* 3 use case:
     * a. download firmware to slave, src addr start from 0x0
     * b. write a word to slave, use xchg_sram_base for rcp
     * c. write a byte to slave, use xchg_sram_base for tabw
     */
    if (data == NULL)
        sram_addr = 0x0;

    tx_len = min(len, (u32) CTS_KR_MAX_LEN);
    data_ptr = data;

    do{
        if (data != NULL)
            data_ptr += write_cnt;
        ret = _cts_write_slave(cts_dev, sram_addr + write_cnt,
                addr + write_cnt, data_ptr, tx_len, client_id);
        if (ret) {
            cts_err("write slave failed, addr:%x, len:%d\n",
                addr + write_cnt, tx_len);
            break;
        }
        write_cnt += tx_len;
        tx_len = min(len - write_cnt , (u32) CTS_KR_MAX_LEN);
    } while (len > write_cnt);

    return ret;
}