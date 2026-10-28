#include "cts_platform.h"
#include "cts_core.h"
#include "cts_transfer.h"
#include "cts_drw.h"

int cts_spi_create_drw_packet(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr,
    const void *data, const u32 len, bool is_read)
{
    int hcrc_cal = 0;
    int dcrc_cal = 0;
    int packet_len = 0;
    int hcrc_len = DRW_HCRC_OFFSET;
    u8  spi_addr_prog = CTS_SPI_PROG_ADDR_DEFAULT;

    memset(pdata->spi_tx_buf, 0x0, CTS_MAX_SPI_BUF_SIZE);
    memset(pdata->spi_rx_buf, 0x0, CTS_MAX_SPI_BUF_SIZE);

    if (hwdata && hwdata->spi_addr_prog != 0)
        spi_addr_prog = hwdata->spi_addr_prog;

    if (is_read) {
        /* DRW READ:
        * MOSI ==> |CMD|ADDR|DATA_LEN|HCRC|     |    |    |     |    |    |
        * MISO ==> |   |    |        |    |DUMMY|DATA|DCRC|DUMMY|HACK|AACK|
        */
        pdata->spi_tx_buf[0] = spi_addr_prog | is_read;  //CMD
        _put_unaligned_be24(addr, &pdata->spi_tx_buf[DRW_ADDR_OFFSET]);
        _put_unaligned_be24(len, &pdata->spi_tx_buf[DRW_DATA_LEN_OFFSET]);

        if (!(spi_addr_prog & DRW_CRC_EN_MASK)) {
            hcrc_cal = cts_crc16(pdata->spi_tx_buf, hcrc_len);
            if (!(spi_addr_prog & DRW_CRC_INVERT_MASK)) {
                hcrc_cal = ~hcrc_cal;
            }
            put_unaligned_be16(hcrc_cal, &pdata->spi_tx_buf[DRW_HCRC_OFFSET]);
        }
    } else {
        /* DRW WRITE:
         * MOSI ==> |CMD|ADDR|DATA_LEN|HCRC|DUMMY|DATA|DCRC|DUMMY|    |    |
         * MISO ==> |   |    |        |    |     |    |    |     |HACK|AACK|
         */
        pdata->spi_tx_buf[0] = spi_addr_prog;  //CMD
        _put_unaligned_be24(addr, &pdata->spi_tx_buf[DRW_ADDR_OFFSET]);
        _put_unaligned_be24(len, &pdata->spi_tx_buf[DRW_DATA_LEN_OFFSET]);
        if (!(spi_addr_prog & DRW_CRC_EN_MASK)) {
            hcrc_cal = cts_crc16(pdata->spi_tx_buf, hcrc_len);
            dcrc_cal = cts_crc16(data, len);
            if (!(spi_addr_prog & DRW_CRC_INVERT_MASK)) {
                hcrc_cal = ~hcrc_cal;
                dcrc_cal = ~dcrc_cal;
            }
            put_unaligned_be16(hcrc_cal, &pdata->spi_tx_buf[DRW_HCRC_OFFSET]);
            put_unaligned_be16(dcrc_cal, &pdata->spi_tx_buf[DRW_DCRC_OFFSET(len)]);
        }
        memset(&pdata->spi_tx_buf[DRW_DUMMY_HEAD_OFFSET], 0xDD,
                DRW_DATA_OFFSET - DRW_DUMMY_HEAD_OFFSET);
        memcpy(&pdata->spi_tx_buf[DRW_DATA_OFFSET], data, len);
        memset(&pdata->spi_tx_buf[DRW_DUMMY_TAIL_OFFSET(len)], 0xDD,
                DRW_HACK_OFFSET(len) - DRW_DUMMY_TAIL_OFFSET(len));
    }

    packet_len = DRW_AACK_OFFSET(len) + 1;
    cts_dbg("%s:%d, packet_len = %d\n", __func__, __LINE__, packet_len);

    return packet_len;
}

int _cts_spi_drw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 len)
{
    u16 crc16_calc, crc16_recv;
    bool crc_check_en = false;
    bool crc_invert_en = false;
    int pack_len = 0;
    int ret, retry = CTS_RETRY;
    u8  cmd;

    pack_len = cts_spi_create_drw_packet(pdata, hwdata, addr, data, len, true);
    cmd = pdata->spi_tx_buf[0];

    do {
        ret = cts_spi_send_recv(pdata, pdata->spi_tx_buf, pack_len,
            pdata->spi_rx_buf);
        if (ret) {
            cts_err("SPI drw write failed %d", ret);
            continue;
        }

        memcpy(data, pdata->spi_rx_buf + DRW_DATA_OFFSET, len);
        crc16_calc = (u16) cts_crc16(data, len);
        crc16_recv = get_unaligned_be16(&pdata->spi_rx_buf[DRW_DCRC_OFFSET(len)]);
        crc_check_en = !(cmd & DRW_CRC_EN_MASK);
        crc_invert_en = !(cmd & DRW_CRC_INVERT_MASK);
        if (crc_check_en) {
            if (crc_invert_en && crc16_recv != (uint16_t)(~crc16_calc)) {
                cts_warn("SPI RX CRC error: rx_crc %04x != %04x",
                    crc16_recv, ~crc16_calc);
                ret = -EINVAL;
                continue;
            } else if (!crc_invert_en && crc16_recv != (uint16_t)(crc16_calc)) {
                cts_warn("SPI RX CRC error: rx_crc %04x != %04x, spi_addr:%x",
                    crc16_recv, crc16_calc, cmd);
                ret = -EINVAL;
                continue;
            }
        }
        break;
    } while (--retry > 0);

    return ret;
}

int cts_spi_drw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 size)
{
    int read_len = 0;
    int len, ret;

    len = min(size, CTS_MAX_SPI_PAYLOAD_SIZE);

    do{
        ret = _cts_spi_drw_read(pdata, hwdata, addr + read_len,
                data + read_len, len);
        if (ret) {
            cts_err("spi drw read data failed, addr:%x, len:%d\n",
                addr + read_len, len);
            break;
        }
        read_len += len;
        len = min(size - read_len , CTS_MAX_SPI_PAYLOAD_SIZE);
    } while (size > read_len);

    return ret;
}

int _cts_spi_drw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 len)
{
    int pack_len = 0;
    u8 hack, aack;
    int ret, retry = CTS_RETRY;

    pack_len = cts_spi_create_drw_packet(pdata, hwdata, addr, data, len, false);

    do {
        ret = cts_spi_send_recv(pdata, pdata->spi_tx_buf, pack_len,
            pdata->spi_rx_buf);
        if (ret) {
            cts_err("SPI drw write failed %d", ret);
            continue;
        }

        hack = pdata->spi_rx_buf[DRW_HACK_OFFSET(len)];
        aack = pdata->spi_rx_buf[DRW_AACK_OFFSET(len)];
        if (hack != DRW_ACK_VALID || aack != DRW_ACK_VALID)
            continue;

        return ret;
    } while (--retry > 0);

    return ret;
}

int cts_spi_drw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 size)
{
    int wr_len = 0;
    int len, ret;

    len = min(size, CTS_MAX_SPI_PAYLOAD_SIZE);

    do{
        ret = _cts_spi_drw_write(pdata, hwdata, addr + wr_len,
                data + wr_len, len);
        if (ret) {
            cts_err("spi drw write data failed, addr:%x, len:%d\n",
                addr + wr_len, len);
            break;
        }
        wr_len += len;
        len = min(size - wr_len, CTS_MAX_SPI_PAYLOAD_SIZE);
    } while (size > wr_len);

    return ret;
}

int cts_i2c_create_drw_packet(struct cts_platform_data *pdata, u32 addr,
    const void *data, const u32 len, bool is_read)
{
    int tx_pack_len = 0;
    int i;

    memset(pdata->i2c_tx_buf, 0x0, CTS_MAX_I2C_BUF_SIZE);
    memset(pdata->i2c_rx_buf, 0x0, CTS_MAX_I2C_BUF_SIZE);
    if (is_read) {
        /* DRW READ:
         * SDA(master) ==> S|(I2C_ADDR)|REG_ADDR|Sr|(I2C_ADDR)|
         * SDA(slave)  ==>                                    |DATA|P
         * note: I2C_ADDR is assigned to member 'addr' of structure i2c_msg,
         *       only payload is on tx/rx buffer.
         */
        _put_unaligned_be24(addr, pdata->i2c_tx_buf);
        tx_pack_len = REG_ADDR_SIZE;
    } else {
        /* DRW WRITE:
         * SDA(master) ==> S|(I2C_ADDR)|REG_ADDR|DATA|P
         * SDA(slave)  ==> (empty)
         * note: I2C_ADDR is assigned to member 'addr' of structure i2c_msg,
         *       only payload is on tx/rx buffer.
         */
        _put_unaligned_be24(addr, pdata->i2c_tx_buf);
        memcpy(pdata->i2c_tx_buf + REG_ADDR_SIZE, data, len);
        tx_pack_len = REG_ADDR_SIZE + len;
    }

    for (i = 0; i < tx_pack_len; i++)
        cts_dbg("i2c drw tx buf[%d]:%02x", i, *(pdata->i2c_tx_buf + i));

    return tx_pack_len;
}

int _cts_i2c_drw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 len)
{
    int tx_pack_len = 0;
    int ret;
    u8 i2c_addr_prog = CTS_I2C_PROG_ADDR_DEFAULT;
    int i;

    if (hwdata && hwdata->i2c_addr_prog != 0)
        i2c_addr_prog = hwdata->i2c_addr_prog;
    tx_pack_len = cts_i2c_create_drw_packet(pdata, addr, data, len, true);

    ret = cts_plat_i2c_transfer(pdata, i2c_addr_prog,
            pdata->i2c_tx_buf, tx_pack_len, pdata->i2c_rx_buf,
            len, 3, 1);
    if (ret)
        cts_err("i2c drw read failed %d", ret);
    else
        memcpy(data, pdata->i2c_rx_buf, len);

    for (i = 0; i < len; i++)
        cts_dbg("i2c rx data[%d]:%x", i, *(pdata->i2c_rx_buf + i));

    return ret;
}

int cts_i2c_drw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 size)
{
    int read_len = 0;
    int len, ret;

    len = min(size, CTS_MAX_I2C_PAYLOAD_SIZE);

    do{
        ret = _cts_i2c_drw_read(pdata, hwdata, addr + read_len,
                data + read_len, len);
        if (ret) {
            cts_err("i2c drw read data failed, addr:%x, len:%d\n",
                addr + read_len, len);
            break;
        }
        read_len += len;
        len = min(size - read_len , CTS_MAX_I2C_PAYLOAD_SIZE);
    } while (size > read_len);

    return ret;
}

int _cts_i2c_drw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 len)
{
    int tx_pack_len = 0;
    int ret;
    u8 i2c_addr_prog = CTS_I2C_PROG_ADDR_DEFAULT;

    if (hwdata && hwdata->i2c_addr_prog != 0)
        i2c_addr_prog = hwdata->i2c_addr_prog;

    tx_pack_len = cts_i2c_create_drw_packet(pdata, addr, data, len, false);

    ret = cts_plat_i2c_transfer(pdata, i2c_addr_prog,
            pdata->i2c_tx_buf, tx_pack_len, NULL, 0, 3, 1);
    if (ret)
        cts_err("i2c drw read failed %d", ret);

    return ret;
}

int cts_i2c_drw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 size)
{
    int wr_len = 0;
    int len, ret;

    len = min(size, CTS_MAX_I2C_PAYLOAD_SIZE);

    do{
        ret = _cts_i2c_drw_write(pdata, hwdata, addr + wr_len,
                data + wr_len, len);
        if (ret) {
            cts_err("i2c drw write data failed, addr:%x, len:%d\n",
                addr + wr_len, len);
            break;
        }
        wr_len += len;
        len = min(size - wr_len, CTS_MAX_I2C_PAYLOAD_SIZE);
    } while (size > wr_len);

    return ret;
}

int cts_i3c_drw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 len)
{
    cts_warn("i3c is not supported yet!\n");
    return 0;
}

int cts_i3c_drw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 len)
{
    cts_warn("i3c is not supported yet!\n");
    return 0;
}