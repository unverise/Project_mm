#define LOG_TAG         "PRW"
//#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_prw.h"
#include "cts_transfer.h"


int cts_spi_create_prw_packet(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr,
    const void *data, const u32 len, bool is_read)
{
    int pack_len = 0;
    u8  spi_addr_prog = CTS_SPI_PROG_ADDR_DEFAULT;

    memset(pdata->spi_tx_buf, 0x0, CTS_MAX_SPI_BUF_SIZE);
    memset(pdata->spi_tx_buf, 0x0, CTS_MAX_SPI_BUF_SIZE);

    if (hwdata && hwdata->spi_addr_prog != 0)
        spi_addr_prog = hwdata->spi_addr_prog;

    if (is_read) {
        /* PRW READ:
         * MOSI ==> |CMD|ADDR|     |    |
         * MISO ==> |   |    |DUMMY|DATA|
         */
        pdata->spi_tx_buf[0] = spi_addr_prog | READ_BIT;  //CMD
        _put_unaligned_be24(addr, &pdata->spi_tx_buf[PRW_ADDR_OFFSET]);
        pack_len = PRW_ADDR_OFFSET + REG_ADDR_SIZE + 1 + len;
    } else {
        /* PRW WRITE:
         * MOSI ==> |CMD|ADDR|DATA|
         * MISO ==> |   |    |    |
         */
        pdata->spi_tx_buf[0] = spi_addr_prog;  //CMD
        _put_unaligned_be24(addr, &pdata->spi_tx_buf[PRW_ADDR_OFFSET]);
        memcpy(pdata->spi_tx_buf + PRW_WR_DATA_OFFSET, data, len);
        pack_len = PRW_WR_DATA_OFFSET + len;
    }

    cts_dbg("%s:%d, pack_len = %d\n", __func__, __LINE__, pack_len);
    return pack_len;
}

int _cts_spi_prw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 len)
{
    int pack_len = 0;
    int ret;
    int retry = 3;

    pack_len = cts_spi_create_prw_packet(pdata, hwdata, addr, data, len, true);

    do {
        ret = cts_spi_send_recv(pdata, pdata->spi_tx_buf, pack_len,
                pdata->spi_rx_buf);
        if (!ret)
            break;
    } while (retry-- > 0);

    if (ret)
        cts_err("spi prw read failed %d", ret);
    else
        memcpy(data, pdata->spi_rx_buf + PRW_RD_DATA_OFFSET, len);

    return ret;
}

int cts_spi_prw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 size)
{
    int read_len = 0;
    int len, ret;

    len = min(size, CTS_MAX_SPI_PAYLOAD_SIZE);

    do{
        ret = _cts_spi_prw_read(pdata, hwdata, addr + read_len,
                data + read_len, len);
        if (ret) {
            cts_err("spi prw read data failed, addr:%x, len:%d\n",
                addr + read_len, len);
            break;
        }
        read_len += len;
        len = min(size - read_len , CTS_MAX_SPI_PAYLOAD_SIZE);
    } while (size > read_len);

    return ret;
}

int _cts_spi_prw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 len)
{
    int pack_len = 0;
    int ret, retry = 3;

    pack_len = cts_spi_create_prw_packet(pdata, hwdata, addr, data, len, false);

    do {
        ret = cts_spi_send_recv(pdata, pdata->spi_tx_buf, pack_len,
                pdata->spi_rx_buf);
        if (!ret)
            break;
    } while (retry-- > 0);

    if (ret)
        cts_err("spi prw write failed %d", ret);

    return ret;
}

int cts_spi_prw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 size)
{
    int wr_len = 0;
    int len, ret;

    len = min(size, CTS_MAX_SPI_PAYLOAD_SIZE);

    do{
        ret = _cts_spi_prw_write(pdata, hwdata, addr + wr_len,
                data + wr_len, len);
        if (ret) {
            cts_err("spi prw write data failed, addr:%x, len:%d\n",
                addr + wr_len, len);
            break;
        }
        wr_len += len;
        len = min(size - wr_len, CTS_MAX_SPI_PAYLOAD_SIZE);
    } while (size > wr_len);

    return ret;
}

int cts_i2c_create_prw_packet(struct cts_platform_data *pdata, u32 addr,
    const void *data, const u32 len, bool is_read)
{
    int tx_pack_len = 0;

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
        tx_pack_len = PRW_ADDR_SIZE;
    } else {
        /* DRW WRITE:
         * SDA(master) ==> S|(I2C_ADDR)|REG_ADDR|DATA|P
         * SDA(slave)  ==> (empty)
         * note: I2C_ADDR is assigned to member 'addr' of structure i2c_msg,
         *       only payload is on tx/rx buffer.
         */
        _put_unaligned_be24(addr, pdata->i2c_tx_buf);
        memcpy(pdata->i2c_tx_buf + PRW_ADDR_SIZE, data, len);
        tx_pack_len = PRW_ADDR_SIZE + len;
    }

    return tx_pack_len;
}

int _cts_i2c_prw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr,
    void *data, const u32 len)
{
    int tx_pack_len = 0;
    int ret;
//    int i;

    u8  i2c_addr_prog = CTS_I2C_PROG_ADDR_DEFAULT;

    if (hwdata && hwdata->i2c_addr_prog != 0)
        i2c_addr_prog = hwdata->i2c_addr_prog;

    tx_pack_len = cts_i2c_create_prw_packet(pdata, addr, data, len, true);

    ret = cts_plat_i2c_transfer(pdata, i2c_addr_prog,
            pdata->i2c_tx_buf, tx_pack_len, pdata->i2c_rx_buf,
            len, 3, 1);
    if (ret)
        cts_err("i2c prw read failed %d", ret);
    else
        memcpy(data, pdata->i2c_rx_buf, len);

    return ret;
}

int cts_i2c_prw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 size)
{
    int read_len = 0;
    int len, ret;

    len = min(size, CTS_MAX_I2C_PAYLOAD_SIZE);

    do{
        ret = _cts_i2c_prw_read(pdata, hwdata, addr + read_len,
                data + read_len, len);
        if (ret) {
            cts_err("i2c prw read data failed, addr:%x, len:%d\n",
                addr + read_len, len);
            break;
        }
        read_len += len;
        len = min(size - read_len , CTS_MAX_I2C_PAYLOAD_SIZE);
    } while (size > read_len);

    return ret;
}

int _cts_i2c_prw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr,
    const void *data, const u32 len)
{
    int tx_pack_len = 0;
    int ret;

    u8  i2c_addr_prog = CTS_I2C_PROG_ADDR_DEFAULT;

    if (hwdata && hwdata->i2c_addr_prog != 0)
        i2c_addr_prog = hwdata->i2c_addr_prog;

    tx_pack_len = cts_i2c_create_prw_packet(pdata, addr, data, len, false);

    ret = cts_plat_i2c_transfer(pdata, i2c_addr_prog,
            pdata->i2c_tx_buf, tx_pack_len, NULL, 0, 3, 1);
    if (ret)
        cts_err("i2c prw write failed %d", ret);

    return ret;
}

int cts_i2c_prw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 size)
{
    int wr_len = 0;
    int len, ret;
    u8 buff[4] = {0xaa, 0xbb, 0xcc, 0xdd};



    len = min(size, CTS_MAX_I2C_PAYLOAD_SIZE);

    do{
        ret = _cts_i2c_prw_write(pdata, hwdata, addr + wr_len,
                data + wr_len, len);
        if (ret) {
            cts_err("i2c prw write data failed, addr:%x, len:%d\n",
                addr + wr_len, len);
            _cts_i2c_prw_write(pdata, hwdata, (u32)0x000000,
                buff, 4);
            break;
        }
        wr_len += len;
        len = min(size - wr_len, CTS_MAX_I2C_PAYLOAD_SIZE);
    } while (size > wr_len);

    return ret;
}

int cts_i3c_prw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr,
    void *data, const u32 len)
{
    cts_warn("i3c is not supported yet!\n");
    return 0;
}

int cts_i3c_prw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr,
    const void *data, const u32 len)
{
    cts_warn("i3c is not supported yet!\n");
    return 0;
}