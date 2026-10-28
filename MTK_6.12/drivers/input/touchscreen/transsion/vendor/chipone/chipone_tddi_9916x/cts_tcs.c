#include <linux/kernel.h>

#define LOG_TAG                    "TCS"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_tcs.h"
#include "cts_transfer.h"


#define CMD_INFO_CHIP_FW_ID_RO                     (0x4003)
#define CMD_INFO_FW_VER_RO                         (0x4005)
#define CMD_INFO_TOUCH_XY_INFO_RO                  (0x4007)
#define CMD_INFO_TOUCH_IC_CHANNEL_INFO_RO          (0x4009)
#define CMD_INFO_TOUCH_PANEL_INFO_RO               (0x400a)
#define CMD_INFO_MODULE_ID_RO                      (0x4011)
#define CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW         (0x6101)
#define CMD_TP_DATA_READ_START_RO                  (0x4102)
#define CMD_TP_DATA_COORDINATES_RO                 (0x4103)
#define CMD_TP_DATA_RAW_RO                         (0x4104)
#define CMD_TP_DATA_DIFF_RO                        (0x4105)
#define CMD_TP_DATA_BASE_RO                        (0x4106)
#define CMD_TP_DATA_CNEG_RO                        (0x410a)
#define CMD_TP_DATA_WR_REG_RAM_SEQUENCE_WO         (0x2114)
#define CMD_TP_DATA_WR_REG_RAM_BATCH_WO            (0x2115)
#define CMD_TP_DATA_WR_DDI_REG_SEQUENCE_WO         (0x2116)
#define CMD_GET_DATA_BY_POLLING_RO                 (0x4123)
#define CMD_TP_DATA_STATUS_RO                      (0x4125)
#define CMD_SYS_STS_READ_RO                        (0x4200)
#define CMD_SYS_STS_WORK_MODE_RW                   (0x6201)
#define CMD_SYS_STS_DAT_RDY_FLAG_RW                (0x6203)
#define CMD_SYS_STS_PWR_STATE_RW                   (0x6204)
#define CMD_SYS_STS_CHARGER_PLUGIN_RW              (0x6205)
#define CMD_SYS_STS_DDI_CODE_VER_RO                (0x4206)
#define CMD_SYS_STS_DAT_TRANS_IN_NORMAL_RW         (0x6207)
#define CMD_SYS_STS_VSTIM_LVL_RW                   (0x6208)
#define CMD_SYS_STS_CNEG_RDY_FLAG_RW               (0x6211)
#define CMD_SYS_STS_EP_PLUGIN_RW                   (0x6213)
#define CMD_SYS_STS_RESET_WO                       (0x2216)
#define CMD_SYS_STS_INT_TEST_EN_RW                 (0x6217)
#define CMD_SYS_STS_SET_INT_PIN_RW                 (0x6218)
#define CMD_SYS_STS_CNEG_RD_EN_RW                  (0x6219)
#define CMD_SYS_STS_HI_SENSE_EN_RW                 (0x621a)
#define CMD_SYS_STS_INT_MODE_RW                    (0x6223)
#define CMD_SYS_STS_INT_KEEP_TIME_RW               (0x6224)
#define CMD_SYS_STS_CURRENT_WORKMODE_RO            (0x4233)
#define CMD_SYS_STS_DATA_CAPTURE_SUPPORT_RO        (0x423f)
#define CMD_SYS_STS_DATA_CAPTURE_EN_RW             (0x6240)
#define CMD_SYS_STS_DATA_CAPTURE_FUNC_MAP_RW       (0x6241)
#define CMD_SYS_STS_PANEL_DIRECTION_RW             (0x6242)
#define CMD_SYS_STS_KRANG_WORK_MODE_RW             (0x2243)
#define CMD_SYS_STS_KRANG_CURRENT_WORKMODE_RO      (0x4244)
#define CMD_TP_STD_CMD_SET_KRANG_STOP              (0x224a)
#define CMD_SYS_STS_GAME_MODE_RW                   (0x624e)
#define CMD_SYS_STS_POCKET_MODE_EN_RW              (0x6250)
#define CMD_SYS_STS_PRODUCTION_TEST_EN_RW          (0x6252)
#define CMD_SYS_STS_KRANG_WORK_STS_RO              (0x4259)
#define CMD_GSTR_WAKEUP_EN_RW                      (0x6301)
#define CMD_GSTR_DAT_RDY_FLAG_GSTR_RW              (0x631e)
#define CMD_GSTR_ENTER_MAP_RW                      (0x6328)
#define CMD_MNT_EN_RW                              (0x6401)
#define CMD_MNT_FORCE_EXIT_MNT_WO                  (0x2403)
#define CMD_DDI_ESD_EN_RW                          (0x6501)
#define CMD_DDI_ESD_OPTIONS_RW                     (0x6502)
#define CMD_CNEG_EN_RW                             (0x6601)
#define CMD_CNEG_OPTIONS_RW                        (0x6602)
#define CMD_COORD_FLIP_X_EN_RW                     (0x6702)
#define CMD_COORD_FLIP_Y_EN_RW                     (0x6703)
#define CMD_COORD_SWAP_AXES_EN_RW                  (0x6704)
#define CMD_PARA_PROXI_EN_RW                       (0x692a)
#define CMD_PARA_KUNCKLE_RW                        (0x694b)
#define CMD_OPENSHORT_EN_RW                        (0x6b01)
#define CMD_OPENSHORT_MODE_SEL_RW                  (0x6b02)
#define CMD_OPENSHORT_SHORT_SEL_RW                 (0x6b03)
#define CMD_OPENSHORT_SHORT_DISP_ON_EN_RW          (0x6b04)

//USD: add for esports feature by jiangzhu.yang 20240826 start
#define CMD_CTS_GAME_MODE                          (0x6253)
//USD: add for esports feature by jiangzhu.yang 20240826 end

//USD:modify for lcd aod by guoli.niu 20250321 start
#define CMD_SYS_STS_AOD_PWR_STATE_RW               (0x632C)
//USD:modify for lcd aod by guoli.niu 20250321 end

//BSP:add for pocket mode by xihao.ding 20251129 start
#define CMD_POCKET_FUNCTION_SUPPORT                (0x6258)
//BSP:add for pocket mode by xihao.ding 20251129 end

void dump_spi(const char *prefix, u8 *data, u32 datalen)
{
    u8 str[1024];
    int offset = 0;
    int i;

    offset += snprintf(str + offset, sizeof(str) - offset, "%s", prefix);
    for (i = 0; i < datalen; i++) {
        offset += snprintf(str + offset, sizeof(str) - offset, " %02x", data[i]);
    }
    cts_err("%s", str);
}

static u16 tcs_calculate_crc(const u8 *data, u16 len)
{
    u16 tcs_crc = 0;
    int i;

    for(i = 0; i < len; i++) {
        tcs_crc += data[i];
    }

    tcs_crc = ~tcs_crc;
    tcs_crc = (tcs_crc & 0xff) | (0x1 << 8);

    return tcs_crc;
}

int _cts_i2c_read(struct cts_platform_data *pdata, u8 i2c_addr,
        void *rbuf, size_t rlen)
{
    int ret = 0;

    struct i2c_msg msgs[2] = {
        {
            .addr = i2c_addr,
            .flags = I2C_M_RD,
            .buf = (u8 *) rbuf,
            .len = rlen,
        }
    };

    ret = i2c_transfer(pdata->i2c_client->adapter,
            &msgs[0], 1);
    if (ret == 1)
        ret = 0;

    return ret;
}

int cts_i2c_read(struct cts_platform_data *pdata, u8 i2c_addr,
        void *rbuf, size_t rlen)
{
    int read_len = 0;
    int len, ret;

    len = min((u32) rlen, CTS_MAX_I2C_PAYLOAD_SIZE);

    do{
        ret = _cts_i2c_read(pdata, i2c_addr, rbuf + read_len, len);
        if (ret) {
            cts_err("tcs i2c read data failed, addr:%x, len:%d\n",
                rbuf + read_len, len);
            break;
        }
        read_len += len;
        len = min((u32) (rlen - read_len), CTS_MAX_I2C_PAYLOAD_SIZE);
    } while (rlen > read_len);

    return ret;
}

int _cts_i2c_write(struct cts_platform_data *pdata, u8 i2c_addr,
        const void *wbuf, size_t wlen)
{
    int ret = 0;

    struct i2c_msg msgs[2] = {
        {
            .addr = i2c_addr,
            .flags = 0,
            .buf = (u8 *) wbuf,
            .len = wlen,
        }
    };

    ret = i2c_transfer(pdata->i2c_client->adapter,
            &msgs[0], 1);
    if (ret == 1)
        ret = 0;

    return ret;
}

int cts_i2c_write(struct cts_platform_data *pdata, u8 i2c_addr,
        const void *wbuf, size_t wlen)
{
    int write_len = 0;
    int len, ret;

    len = min((u32) wlen, CTS_MAX_I2C_PAYLOAD_SIZE);

    do{
        ret = _cts_i2c_write(pdata, i2c_addr, wbuf + write_len, len);
        if (ret) {
            cts_err("tcs i2c read data failed, addr:%x, len:%d\n",
                wbuf + write_len, len);
            break;
        }
        write_len += len;
        len = min((u32) (wlen - write_len), CTS_MAX_I2C_PAYLOAD_SIZE);
    } while (wlen > write_len);

    return ret;
}

int cts_tcs_i2c_transfer(struct cts_platform_data *pdata, u8 i2c_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen)
{
    int ret = 0;

    ret = cts_i2c_write(pdata, i2c_addr, wbuf, wlen);
    if (ret)
        goto out;

    ret = cts_i2c_read(pdata, i2c_addr, rbuf, rlen);

out:
    return ret;
}

static int cts_tcs_i2c_read_pack(u8 *tx, u16 cmd, u16 rdatalen)
{
    tcs_i2c_tx_head *txhdr = (tcs_i2c_tx_head *) tx;
    u16 is_read;
    int packlen = 0;

    is_read = cmd & BIT(TCS_READ_BIT);
    if (0 == is_read) {
        return packlen;
    }
    txhdr->cmd = cpu_to_le16(cmd & ~BIT(TCS_WRITE_BIT));
    txhdr->datlen = cpu_to_le16(rdatalen);
    txhdr->check_l = ~((txhdr->cmd & 0xff)
            + ((txhdr->cmd >> 8) & 0xff)
            + (rdatalen & 0xff)
            + ((rdatalen >> 8) & 0xff));
    txhdr->check_h = 1;
    packlen += sizeof(tcs_i2c_tx_head);

    return packlen;
}

static int cts_tcs_i2c_read(const struct cts_device *cts_dev, u16 cmd,
        u8 *buf, u32 len)
{
    int txlen;
    u8 i2c_addr;
    u16 cmd_recv;
    u16 cmd_send;
    u8 error_code;
    int ret;
    int i;

    i2c_addr = CTS_I2C_NORM_ADDR_DEFAULT;
    if (cts_dev->hwdata && cts_dev->hwdata->i2c_addr_norm != 0)
        i2c_addr = cts_dev->hwdata->i2c_addr_norm;

    txlen = cts_tcs_i2c_read_pack(cts_dev->pdata->i2c_tx_buf, cmd, len);
    cts_dbg("%s:%d, cmd:%x, len:%d, i2c_addr:0x%x", __func__, __LINE__, cmd, len, i2c_addr);
    for (i = 0; i < txlen; i++)
        cts_dbg("tx[i]:%02x", *(cts_dev->pdata->i2c_tx_buf + i));

    ret = cts_tcs_i2c_transfer(cts_dev->pdata, i2c_addr,
        cts_dev->pdata->i2c_tx_buf, txlen, cts_dev->pdata->i2c_rx_buf,
        len + sizeof(tcs_rx_tail));
    if (ret == 0) {
        error_code = *(cts_dev->pdata->i2c_rx_buf + len);
        cmd_send = le16_to_cpup((__le16 *)cts_dev->pdata->i2c_tx_buf);
        cmd_recv = le16_to_cpup((__le16 *)(cts_dev->pdata->i2c_rx_buf + len + 1));
        if (error_code != 0) {
            cts_warn("error code:0x%02x, send %04x, %04x recv", error_code,
                    cmd_send, cmd_recv);
            ret = -EIO;
        }

        memcpy(buf, cts_dev->pdata->i2c_rx_buf, len);
    }

    return ret;
}

static int cts_tcs_i2c_write_pack(u8 *tx, u16 cmd, u8 *wdata, u16 wlen)
{
    tcs_i2c_tx_head *txhdr = (tcs_i2c_tx_head *) tx;
    int packlen = 0;
    u16 data_crc = 0;
    u16 is_write;

    is_write = cmd & BIT(TCS_WRITE_BIT);
    if (0 == is_write) {
        return packlen;
    }
    txhdr->cmd = cpu_to_le16(cmd & ~BIT(TCS_READ_BIT));
    txhdr->datlen = cpu_to_le16(wlen);
    txhdr->check_l = ~((txhdr->cmd & 0xff)
            + ((txhdr->cmd >> 8) & 0xff)
            + (wlen & 0xff)
            + ((wlen >> 8) & 0xff));
    txhdr->check_h = 1;
    packlen += sizeof(tcs_i2c_tx_head);
    cts_info("%s:%d, packlen:%d", __func__, __LINE__, packlen);

    if (wlen > 0) {
        memcpy(tx + packlen, wdata, wlen);
        data_crc = tcs_calculate_crc(wdata, wlen);
        put_unaligned_le16(data_crc, tx + packlen + wlen);
        packlen += (wlen + sizeof(data_crc));
        cts_info("%s:%d, packlen:%d, wlen:%d, sizeof(data_crc):%d", __func__, __LINE__, packlen, wlen, sizeof(data_crc));
    }

    return packlen;
}

static int cts_tcs_i2c_write(const struct cts_device *cts_dev,
        u16 cmd, u8 *wbuf, u32 wlen)
{
    int txlen;
    u8 i2c_addr;
    u16 cmd_recv;
    u16 cmd_send;
    u8 error_code;
    int ret = 0;
    int i;

    i2c_addr = CTS_I2C_NORM_ADDR_DEFAULT;
    if (cts_dev->hwdata && cts_dev->hwdata->i2c_addr_norm != 0)
        i2c_addr = cts_dev->hwdata->i2c_addr_norm;

    txlen = cts_tcs_i2c_write_pack(cts_dev->pdata->i2c_tx_buf, cmd, wbuf, wlen);
    cts_info("tx_len:%d, wlen:%d", txlen, wlen);
    for (i = 0; i < txlen; i++)
        cts_info("i2c_addr:0x%x, tx[i]:%02x", i2c_addr, *(cts_dev->pdata->i2c_tx_buf + i));

    ret =  cts_tcs_i2c_transfer(cts_dev->pdata, i2c_addr,
            cts_dev->pdata->i2c_tx_buf, txlen, cts_dev->pdata->i2c_rx_buf,
            sizeof(tcs_rx_tail));

    if(!ret) {
        error_code = *cts_dev->pdata->i2c_rx_buf;
        cmd_send = le16_to_cpup((__le16 *)cts_dev->pdata->i2c_tx_buf);
        cmd_recv = le16_to_cpup((__le16 *)(cts_dev->pdata->i2c_rx_buf + 1));
        if (error_code != 0) {
            cts_err("error code:0x%02x, send %04x, %04x recv", error_code,
                    cmd_send, cmd_recv);
            ret = -EIO;
        }
    }

    return ret;
}

int cts_tcs_spi_xtrans(const struct cts_device *cts_dev, u8 *tx,
        size_t txlen, u8 *rx, size_t rxlen)
{
    struct chipone_ts_data *cts_data = container_of(cts_dev,
        struct chipone_ts_data, cts_dev);
    struct spi_transfer xfer[2];
    struct spi_message msg;
    tcs_spi_tx_head *txhdr = (tcs_spi_tx_head *)tx;
    tcs_rx_tail *rx_tail = (tcs_rx_tail *)(rx + rxlen);
    u16 cmd_recv, cmd_send;
    int ret;

    memset(&xfer[0], 0, sizeof(struct spi_transfer));
    //xfer[0].delay_usecs = 0;
    xfer[0].speed_hz = cts_dev->pdata->spi_speed * 1000u;
    xfer[0].tx_buf = tx;
    xfer[0].rx_buf = NULL;
    xfer[0].len = txlen;

    spi_message_init(&msg);
    spi_message_add_tail(&xfer[0], &msg);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(cts_dev->pdata, 0);
#endif
    ret = spi_sync(cts_data->spi_client, &msg);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(cts_dev->pdata, 1);
#endif
    if (ret < 0) {
        cts_err("spi_sync xfer[0] failed: %d", ret);
        return ret;
    }
    udelay(100);

    memset(&xfer[1], 0, sizeof(struct spi_transfer));
    //xfer[1].delay_usecs = 0;
    xfer[1].speed_hz = cts_dev->pdata->spi_speed * 1000u;
    xfer[1].tx_buf = NULL;
    xfer[1].rx_buf = rx;
    xfer[1].len = rxlen + sizeof(tcs_rx_tail);

    spi_message_init(&msg);
    spi_message_add_tail(&xfer[1], &msg);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(cts_dev->pdata, 0);
#endif
    ret = spi_sync(cts_data->spi_client, &msg);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(cts_dev->pdata, 1);
#endif
    if (ret < 0) {
        cts_err("spi_sync xfer[1] failed: %d", ret);
        return ret;
    }

    cmd_send = le16_to_cpu(txhdr->cmd);
    cmd_recv = le16_to_cpu(rx_tail->cmd);
    if (cmd_recv != cmd_send) {
        cts_warn("cmd check error, send %04x != %04x recv", cmd_send, cmd_recv);
        ret = -EINVAL;
    }

    return ret;
}

static int cts_tcs_spi_read_1_cs(const struct cts_device *cts_dev, u8 *tx,
        u32 txlen, u8 *rx, u32 rxlen)
{
    struct chipone_ts_data *cts_data = container_of(cts_dev,
        struct chipone_ts_data, cts_dev);
    struct spi_transfer xfer[1];
    struct spi_message msg;
    tcs_spi_tx_head *txhdr = (tcs_spi_tx_head *)tx;
    tcs_rx_tail *rx_tail = (tcs_rx_tail *)(rx + rxlen);
    u16 cmd_recv, cmd_send;
    int ret;

    memset(&xfer[0], 0, sizeof(struct spi_transfer));
    //xfer[0].delay_usecs = 0;
    xfer[0].speed_hz = cts_dev->pdata->spi_speed * 1000u;
    xfer[0].tx_buf = tx;
    xfer[0].rx_buf = rx;
    xfer[0].len = txlen > (rxlen + sizeof(tcs_rx_tail)) ?
            txlen : (rxlen + sizeof(tcs_rx_tail));

    spi_message_init(&msg);
    spi_message_add_tail(&xfer[0], &msg);

#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(cts_dev->pdata, 0);
#endif
    ret = spi_sync(cts_data->spi_client, &msg);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(cts_dev->pdata, 1);
#endif
    if (ret < 0) {
        cts_err("spi_sync xfer[0] failed: %d", ret);
        return ret;
    }

    cmd_send = le16_to_cpu(txhdr->cmd);
    cmd_recv = le16_to_cpu(rx_tail->cmd);
    if (cmd_recv != cmd_send)
        cts_warn("cmd check error, send %04x != %04x recv", cmd_send, cmd_recv);

    return 0;
}

static int cts_tcs_spi_read_pack(u8 *tx, u16 cmd, u16 rdatalen)
{
    tcs_spi_tx_head *txhdr = (tcs_spi_tx_head *) tx;
    u16 is_read;
    int packlen = 0;
    u16 crc16;

    is_read = cmd & BIT(TCS_READ_BIT);
    if (0 == is_read) {
        return packlen;
    }
    txhdr->addr = TCS_RD_ADDR;
    txhdr->cmd = cpu_to_le16(cmd & ~BIT(TCS_WRITE_BIT));
    txhdr->datlen = cpu_to_le16(rdatalen);
    crc16 = cts_crc16((const u8 *)txhdr, offsetof(tcs_spi_tx_head, crc16));
    txhdr->crc16 = cpu_to_le16(crc16);
    packlen += sizeof(tcs_spi_tx_head);

    return packlen;
}

static int cts_tcs_spi_write_pack(u8 *tx, u16 cmd, u8 *wdata, u16 wdatalen)
{
    tcs_spi_tx_head *txhdr = (tcs_spi_tx_head *) tx;
    u16 is_write;
    int packlen = 0;
    u16 crc16;

    is_write = cmd & BIT(TCS_WRITE_BIT);
    if (0 == is_write) {
        return packlen;
    }
    txhdr->addr = TCS_WR_ADDR;
    txhdr->cmd = cpu_to_le16(cmd & ~BIT(TCS_READ_BIT));
    txhdr->datlen = cpu_to_le16(wdatalen);
    crc16 = cts_crc16((const u8 *)txhdr, offsetof(tcs_spi_tx_head, crc16));
    txhdr->crc16 = cpu_to_le16(crc16);
    packlen += sizeof(tcs_spi_tx_head);

    if (wdatalen > 0) {
        memcpy(tx + sizeof(tcs_spi_tx_head), wdata, wdatalen);
        crc16 = cts_crc16(wdata, wdatalen);
        put_unaligned_le16(crc16, tx + sizeof(tcs_spi_tx_head) + wdatalen);
        packlen += wdatalen + sizeof(crc16);
    }

    return packlen;
}

static int cts_tcs_spi_read(const struct cts_device *cts_dev,
        u16 cmd, u8 *rdata, u32 rdatalen)
{
    int txlen;
    int ret;

    txlen = cts_tcs_spi_read_pack(cts_dev->pdata->spi_tx_buf, cmd, rdatalen);
    if (0 == txlen) {
        cts_err("spi read pack len: %d", txlen);
        return -EINVAL;
    }
    ret = cts_tcs_spi_xtrans(cts_dev, cts_dev->pdata->spi_tx_buf, txlen,
            cts_dev->pdata->spi_rx_buf, rdatalen);
    if (ret) {
        return ret;
    }

    memcpy(rdata, cts_dev->pdata->spi_rx_buf, rdatalen);

    return ret;
}

static int cts_tcs_spi_read_touch_data(const struct cts_device *cts_dev,
        u16 cmd, u8 *rdata, u32 rdatalen)
{
    int txlen;
    int ret;

    txlen = cts_tcs_spi_read_pack(cts_dev->pdata->spi_tx_buf, cmd, rdatalen);
    if (0 == txlen) {
        cts_err("spi read pack len: %d", txlen);
        return -EINVAL;
    }
    ret = cts_tcs_spi_read_1_cs(cts_dev, cts_dev->pdata->spi_tx_buf, txlen,
            cts_dev->pdata->spi_rx_buf, rdatalen);
    if (ret) {
        return ret;
    }

    memcpy(rdata, cts_dev->pdata->spi_rx_buf, rdatalen);

    return ret;
}

int cts_tcs_get_touch_data(const struct cts_device *cts_dev,
        u16 cmd, u8 *buf, u32 len)
{
    int ret = -EINVAL;

    if (cts_dev->bus_type == CTS_BUS_I2C)
        ret = cts_tcs_i2c_read(cts_dev, cmd, buf, len);
    else if (cts_dev->bus_type == CTS_BUS_SPI)
        ret = cts_tcs_spi_read_touch_data(cts_dev, cmd, buf, len);
    else
        cts_err("%s(%d): unkown bus type: %d\n", __func__, __LINE__,
            cts_dev->bus_type);

    return ret;
}

static int cts_tcs_spi_write(const struct cts_device *cts_dev,
        u16 cmd, u8 *data, u32 wlen)
{
    int txlen;
    int ret;

    txlen = cts_tcs_spi_write_pack(cts_dev->pdata->spi_tx_buf, cmd, data, wlen);
    if (0 == txlen) {
        cts_err("spi write pack len: %d", txlen);
        return -EINVAL;
    }
    ret = cts_tcs_spi_xtrans(cts_dev, cts_dev->pdata->spi_tx_buf, txlen,
            cts_dev->pdata->spi_rx_buf, 0);
    return ret;
}

int cts_tcs_read(const struct cts_device *cts_dev,
        u16 cmd, u8 *buf, size_t len)
{
    int ret = -EINVAL;
    int i;

    for (i = 0; i < CTS_RETRY; i++) {
        if (cts_dev->bus_type == CTS_BUS_I2C)
            ret = cts_tcs_i2c_read(cts_dev, cmd, buf, len);
        else if (cts_dev->bus_type == CTS_BUS_SPI)
            ret = cts_tcs_spi_read(cts_dev, cmd, buf, len);
        else
            cts_err("%s(%d): unkown bus type: %d\n", __func__, __LINE__,
                cts_dev->bus_type);

        if (!ret)
            break;
        mdelay(CTS_M_DELAY);
    }

    return ret;
}

int cts_tcs_write(const struct cts_device *cts_dev,
        u16 cmd, u8 *buf, size_t len)
{
    int ret = -EINVAL;
    int i;

    for (i = 0; i < CTS_RETRY; i++) {
        if (cts_dev->bus_type == CTS_BUS_I2C)
            ret = cts_tcs_i2c_write(cts_dev, cmd, buf, len);
        else if (cts_dev->bus_type == CTS_BUS_SPI)
            ret = cts_tcs_spi_write(cts_dev, cmd, buf, len);
        else
            cts_err("%s(%d): unkown bus type: %d\n", __func__, __LINE__,
                cts_dev->bus_type);

        if (!ret)
            break;
        mdelay(CTS_M_DELAY);
    }

    return ret;
}

int cts_tcs_get_fw_ver(const struct cts_device *cts_dev, u16 *fwver)
{
    u8 buf[4] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_FW_VER_RO, buf, sizeof(buf));
    if (ret == 0) {
        *fwver = buf[0] | (buf[1] << 8);
    }
    return ret;
}

int cts_tcs_get_lib_ver(const struct cts_device *cts_dev, u16 *libver)
{
    u8 buf[4] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_FW_VER_RO, buf, sizeof(buf));
    if (ret == 0) {
        *libver = buf[2] | (buf[3] << 8);
    }
    return ret;
}

int cts_tcs_get_fw_id(const struct cts_device *cts_dev, u16 *fwid)
{
    u8 buf[4] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_CHIP_FW_ID_RO, buf, sizeof(buf));
    if (ret == 0) {
        *fwid = buf[0] | (buf[1] << 8);
    }

    return ret;
}

int cts_tcs_get_ddi_ver(const struct cts_device *cts_dev, u8 *ddiver)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_DDI_CODE_VER_RO, buf, sizeof(buf));
    if (ret == 0) {
        *ddiver = buf[0];
    }
    return ret;
}

int cts_tcs_get_touch_xy_info(const struct cts_device *cts_dev,
    struct cts_touch_xy_info *xy_info)
{
    u8 buf[10] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_TOUCH_XY_INFO_RO, buf, sizeof(buf));
    if (ret == 0) {
        xy_info->res_x = get_unaligned_le16(buf);
        xy_info->res_y = get_unaligned_le16(&buf[2]);
        xy_info->rows = buf[5];//buf[4];
        xy_info->cols = buf[4];//buf[5];
        xy_info->lp_rows = buf[7];//buf[6];
        xy_info->lp_cols = buf[6];//buf[7];
    }

    return ret;
}

int cts_tcs_get_ic_channel_info(const struct cts_device *cts_dev,
    struct cts_ic_channel_info *ic_channel_info)
{
    u8 buf[10] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_TOUCH_IC_CHANNEL_INFO_RO, buf,
            sizeof(buf));
    if (ret == 0) {
        memcpy(ic_channel_info, buf, sizeof(struct cts_ic_channel_info));
    }

    return ret;
}

int cts_tcs_get_panel_info(const struct cts_device *cts_dev,
    struct cts_panel_info *panel_info)
{
    u8 buf[16] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_TOUCH_PANEL_INFO_RO, buf, sizeof(buf));
    if (ret == 0) {
        panel_info->panel_type = buf[0];
        panel_info->panel_scal = buf[1];
        panel_info->panel_size = buf[2];
        panel_info->res_x = get_unaligned_le16(&buf[3]);
        panel_info->res_y = get_unaligned_le16(&buf[5]);
        panel_info->fs_cols = buf[7];
        panel_info->fs_rows = buf[8];
        panel_info->gs_cols = buf[9];
        panel_info->gs_rows = buf[10];
        panel_info->pc_cols = buf[11];
        panel_info->pc_rows = buf[12];
        panel_info->pr_cols = buf[13];
        panel_info->pr_rows = buf[14];
    }

    return ret;
}

int cts_tcs_get_flip_x(const struct cts_device *cts_dev, bool *flip_x)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_COORD_FLIP_X_EN_RW, buf, sizeof(buf));
    if (ret == 0) {
        *flip_x = !!buf[0];
    }
    return ret;
}

int cts_tcs_get_flip_y(const struct cts_device *cts_dev, bool *flip_y)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_COORD_FLIP_Y_EN_RW, buf, sizeof(buf));
    if (ret == 0) {
        *flip_y = !!buf[0];
    }
    return ret;
}

int cts_tcs_get_swap_axes(const struct cts_device *cts_dev, bool *swap_axes)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_COORD_SWAP_AXES_EN_RW, buf, sizeof(buf));
    if (ret == 0) {
        *swap_axes = !!buf[0];
    }
    return ret;
}

int cts_tcs_get_int_mode(const struct cts_device *cts_dev, u8 *int_mode)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_INT_MODE_RW, buf, sizeof(buf));
    if (ret == 0) {
        *int_mode = buf[0];
    }
    return ret;
}

int cts_tcs_get_int_keep_time(const struct cts_device *cts_dev,
        u16 *int_keep_time)
{
    u8 buf[2] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_INT_KEEP_TIME_RW, buf, sizeof(buf));
    if (ret == 0) {
        *int_keep_time = (buf[0] | (buf[1] << 8));
    }
    return ret;

}

int cts_tcs_get_rawdata_target(const struct cts_device *cts_dev,
        u16 *rawdata_target)
{
    u8 buf[2] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_CNEG_OPTIONS_RW, buf, sizeof(buf));
    if (ret == 0) {
        *rawdata_target = (buf[0] | (buf[1] << 8));
    }
    return ret;

}

int cts_tcs_get_esd_method(const struct cts_device *cts_dev, u8 *esd_method)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_DDI_ESD_OPTIONS_RW, buf, sizeof(buf));
    if (ret == 0) {
        *esd_method = buf[0];
    }
    return ret;
}

int cts_tcs_get_esd_protection(const struct cts_device *cts_dev,
        u8 *esd_protection)
{
    u8 buf[4] = { 0 };
    int ret;

    buf[0] = 0x01;
    buf[1] = 0x56;
    buf[2] = 0x81;
    buf[3] = 0x00;

    ret = cts_tcs_write(cts_dev, CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
            buf, sizeof(buf));
    if (ret != 0)
        return ret;

    ret = cts_tcs_read(cts_dev, CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
            esd_protection, sizeof(u8));

    return ret;
}

int cts_tcs_get_data_ready_flag(const struct cts_device *cts_dev, u8 *ready)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_DAT_RDY_FLAG_RW, buf, sizeof(buf));
    if (ret == 0) {
        *ready = buf[0];
    }
    return ret;
}

int cts_tcs_clr_gstr_ready_flag(const struct cts_device *cts_dev)
{
    u8 ready = 0;

    return cts_tcs_write(cts_dev, CMD_GSTR_DAT_RDY_FLAG_GSTR_RW,
            &ready, sizeof(ready));
}

int cts_tcs_clr_data_ready_flag(const struct cts_device *cts_dev)
{
    u8 ready = 0;

    return cts_tcs_write(cts_dev, CMD_SYS_STS_DAT_RDY_FLAG_RW,
            &ready, sizeof(ready));
}

int _cts_tcs_read_data(const struct cts_device *cts_dev, u32 addr,
        u8 *data, u32 size)
{
    __le32 read_addr = 0;
    int ret;

    read_addr = cpu_to_le32((addr << 8) | TCS_READ_DATA_TYPE_REG_RAM);
    ret = cts_tcs_write(cts_dev, CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
        (u8 *) &read_addr, 4);
    if (ret) {
        cts_err("tcs read reg/sram failed!\n");
        return ret;
    }

    return cts_tcs_read(cts_dev, CMD_TP_DATA_READ_START_RO, data, size);
}

#define TCS_WR_MAX_LEN      1000
int cts_tcs_read_data(const struct cts_device *cts_dev, u32 addr,
        u8 *data, u32 size)
{
    int len, ret;
    int rx_len = 0;

    len = min((int) size, TCS_WR_MAX_LEN);

    do{
        ret = _cts_tcs_read_data(cts_dev, addr + rx_len, data + rx_len, len);
        if (ret) {
            cts_err("tcs write data failed, addr:%x, len:%d\n", addr + rx_len,
                len);
            break;
        }
        rx_len += len;
        len = min((int) size - rx_len, TCS_WR_MAX_LEN);
    } while (size > rx_len);

    return ret;
}

/* defined in tcs cmd spec V2 */
#define TCS_WR_MAX_LEN      1000

static int cts_tcs_write_data(const struct cts_device *cts_dev, u32 addr,
        const u8 *data, u32 size, enum cts_device_type device_type)
{
    u8 buff[TCS_WR_MAX_LEN + 6];
    u16 size_le;
    u32 addr_le;
    int ret = 0;

    size_le = cpu_to_le16(size);
    memcpy(buff, &size_le, sizeof(size_le));
    addr_le = cpu_to_le32(addr & 0x00ffffff);
    memcpy(buff + sizeof(size_le), &addr_le, sizeof(addr_le));
    memcpy(buff + 6, data, size);
    if (device_type == CTS_TP) {
        ret = cts_tcs_write(cts_dev, CMD_TP_DATA_WR_REG_RAM_SEQUENCE_WO, buff,
                size +6);
    } else if (device_type == CTS_DDI) {
        ret = cts_tcs_write(cts_dev, CMD_TP_DATA_WR_DDI_REG_SEQUENCE_WO, buff,
                size +6);
    }

    return ret;
}

int cts_tcs_write_tp_data(const struct cts_device *cts_dev, u32 addr,
        const u8 *data, u32 size)
{
    int wr_len = 0;
    int len, ret;

    len = min((int) size, TCS_WR_MAX_LEN);

    do{
        ret = cts_tcs_write_data(cts_dev, addr + wr_len, data + wr_len, len, CTS_TP);
        if (ret) {
            cts_err("tcs write data failed, addr:%x, len:%d\n", addr + wr_len,
                len);
            break;
        }
        wr_len += len;
        len = min((int) size - wr_len , TCS_WR_MAX_LEN);
    } while (size > wr_len);

    return ret;
}

int cts_tcs_write_reg(const struct cts_device *cts_dev, u32 addr,
        u8 *regbuf, u32 size)
{
    return cts_tcs_write_tp_data(cts_dev, addr, regbuf, size);
}

int cts_tcs_write_ddi_reg(const struct cts_device *cts_dev, u32 addr,
        const u8 *data, u32 size)
{
    int wr_len = 0;
    int len, ret;

    len = min((int) size, TCS_WR_MAX_LEN);

    do{
        ret = cts_tcs_write_data(cts_dev, addr + wr_len, data + wr_len, len, CTS_DDI);
        if (ret) {
            cts_err("tcs write data failed, addr:%x, len:%d\n", addr + wr_len,
                len);
            break;
        }
        wr_len += len;
        len = min((int) size - wr_len , TCS_WR_MAX_LEN);
    } while (size > wr_len);

    return ret;
}

int cts_tcs_read_ddi_reg(const struct cts_device *cts_dev, u32 addr,
        u8 *regbuf, u32 size)
{
    return cts_tcs_read_data(cts_dev, addr, regbuf, size);
}
/* TODO: INT_DATA_TYPE size is different between tddi and touch */
void cts_tcs_calc_int_data_size(struct cts_device *cts_dev)
{
#define INT_DATA_TYPE_U8_SIZ        \
    (cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * sizeof(u8))
#define INT_DATA_TYPE_U16_SIZ        \
    (cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * sizeof(u16))

    int data_size = TOUCH_DATA_SIZE;
    u16 data_types = cts_dev->fwdata.int_data_types;
    u8 data_method = cts_dev->fwdata.int_data_method;

    cts_info("data_method:%d, data_type:%d", data_method, data_types);

    if (data_method == INT_DATA_METHOD_NONE) {
        cts_dev->fwdata.int_data_size = TOUCH_INFO_SIZE;
        return;
    } else if (data_method == INT_DATA_METHOD_DEBUG) {
        data_size += INT_DATA_INFO_SIZE;
        data_size += (INT_DATA_TYPE_LEN_SIZE + INT_DATA_TYPE_U16_SIZ);
        cts_dev->fwdata.int_data_size = data_size;
        return;
    }

    if (data_types != INT_DATA_TYPE_NONE) {
        data_size += INT_DATA_INFO_SIZE;
        if ((data_types & INT_DATA_TYPE_RAWDATA)) {
            data_size += (INT_DATA_TYPE_LEN_SIZE + INT_DATA_TYPE_U16_SIZ);
        }
        if ((data_types & INT_DATA_TYPE_MANUAL_DIFF)) {
            data_size += (INT_DATA_TYPE_LEN_SIZE + INT_DATA_TYPE_U16_SIZ);
        }
        if ((data_types & INT_DATA_TYPE_REAL_DIFF)) {
            data_size += (INT_DATA_TYPE_LEN_SIZE + INT_DATA_TYPE_U16_SIZ);
        }
        if ((data_types & INT_DATA_TYPE_NOISE_DIFF)) {
            data_size += (INT_DATA_TYPE_LEN_SIZE + INT_DATA_TYPE_U16_SIZ);
        }
        if ((data_types & INT_DATA_TYPE_BASEDATA)) {
            data_size += (INT_DATA_TYPE_LEN_SIZE + INT_DATA_TYPE_U16_SIZ);
        }
        if ((data_types & INT_DATA_TYPE_CNEGDATA)) {
            data_size += (INT_DATA_TYPE_LEN_SIZE + INT_DATA_TYPE_U8_SIZ);
        }
    }

    if (data_size > CFG_CTS_INT_DATA_MAX_SIZE) {
        cts_warn("int_data_size(%d) is longer than buffer size(8192 byte)\n",
            data_size);
        data_size = CFG_CTS_INT_DATA_MAX_SIZE;
        }

    cts_info("data_size: %d", data_size);

    cts_dev->fwdata.int_data_size = data_size;
}

void cts_tcs_drop_frames(struct cts_device *cts_dev, int frame_count)
{
    int retries, i, ret;
    u8 ready = 0;

    for (i = 0; i < frame_count; i++) {
        retries = 100;
        cts_tcs_clr_data_ready_flag(cts_dev);
        do {
            ret = cts_tcs_get_data_ready_flag(cts_dev, &ready);
            if (!ret && ready)
                break;
            mdelay(10);
        } while (!ready && --retries);

        if (!retries) {
            cts_warn("get data ready flag timeout");
            break;
        }
    }
}

int cts_tcs_polling_data(struct cts_device *cts_dev,
        u8 *buf, size_t size)
{
    int retries = 100;
    u8 ready = 0;
    int ret;


    if (size < TOUCH_INFO_SIZE || size > CFG_CTS_INT_DATA_MAX_SIZE) {
        cts_warn("invalid data size:%d", size);
        return -EINVAL;
    }

    do {
        ret = cts_tcs_get_data_ready_flag(cts_dev, &ready);
        if (!ret && ready)
            break;
        mdelay(10);
    } while (!ready && --retries);
    cts_info("get data rdy, retries left %d", retries);

    if (!ready) {
        cts_err("time out wait for data rdy");
        return -EIO;
    }

    retries = 3;
    do {
        ret = cts_tcs_get_touch_data(cts_dev, CMD_GET_DATA_BY_POLLING_RO,
            cts_dev->pdata->int_data, size);

        mdelay(1);
    } while (ret && --retries);

    if (cts_tcs_clr_data_ready_flag(cts_dev))
        cts_err("Clear data ready flag failed");

    return ret;
}

int cts_tcs_polling_test_data(struct cts_device *cts_dev,
        u8 *buf, size_t size)
{
    int offset = TOUCH_DATA_SIZE + INT_DATA_INFO_SIZE + INT_DATA_TYPE_LEN_SIZE;
    int retries = 5;
    int ret;

    while (retries--) {
        ret = cts_tcs_polling_data(cts_dev, buf, size + offset);
        if (!ret) {
            memcpy(buf, cts_dev->pdata->int_data + offset, size);
            break;
        }
    }

    return ret;
}

static int polling_data(struct cts_device *cts_dev, u8 *buf, size_t size,
        enum int_data_type type)
{
    u8 old_int_data_method;
    u16 old_int_data_types;
    int offset = TOUCH_DATA_SIZE + INT_DATA_INFO_SIZE + INT_DATA_TYPE_LEN_SIZE;
    int retries = 5;
    int ret;

    old_int_data_types = cts_dev->fwdata.int_data_types;
    old_int_data_method = cts_dev->fwdata.int_data_method;

    cts_set_int_data_types(cts_dev, type);
    cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);

    while (retries--) {
        ret = cts_tcs_polling_data(cts_dev, buf, size + offset);
        if (!ret) {
            memcpy(buf, cts_dev->pdata->int_data + offset, size);
            break;
        }
    }

    cts_set_int_data_method(cts_dev, old_int_data_method);
    cts_set_int_data_types(cts_dev, old_int_data_types);

    return ret;
}

int cts_test_polling_rawdata(struct cts_device *cts_dev, u8 *buf, size_t size)
{
    int offset = TOUCH_DATA_SIZE + INT_DATA_INFO_SIZE + INT_DATA_TYPE_LEN_SIZE;
    int retries = CTS_RETRY;
    int ret;

    while (retries--) {
        ret = cts_tcs_polling_data(cts_dev, buf, size + offset);
        if (!ret) {

            memcpy(buf, cts_dev->pdata->int_data + offset, size);
            break;
        }
    }

    return ret;
}

int cts_tcs_top_get_rawdata(struct cts_device *cts_dev, u8 *buf, size_t size)
{
    return polling_data(cts_dev, buf, size, INT_DATA_TYPE_RAWDATA);
}

int cts_tcs_top_get_manual_diff(struct cts_device *cts_dev, u8 *buf, size_t size)
{
    return polling_data(cts_dev, buf, size, INT_DATA_TYPE_MANUAL_DIFF);
}

int cts_tcs_top_get_real_diff(struct cts_device *cts_dev, u8 *buf, size_t size)
{
    return polling_data(cts_dev, buf, size, INT_DATA_TYPE_REAL_DIFF);
}

int cts_tcs_top_get_noise_diff(struct cts_device *cts_dev, u8 *buf, size_t size)
{
    return polling_data(cts_dev, buf, size, INT_DATA_TYPE_NOISE_DIFF);
}

int cts_tcs_top_get_basedata(struct cts_device *cts_dev, u8 *buf, size_t size)
{
    return polling_data(cts_dev, buf, size, INT_DATA_TYPE_BASEDATA);
}

int cts_tcs_top_get_cnegdata(struct cts_device *cts_dev, u8 *buf, size_t size)
{
    return polling_data(cts_dev, buf, size, INT_DATA_TYPE_CNEGDATA);
}

int cts_tcs_reset_device(const struct cts_device *cts_dev)
{
    u8 buf[2] = { 0x01, 0xfe };
    int ret;
    u8 val;

    /* normal */
    cts_info("tp reset in normal mode");
    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_RESET_WO,
            buf, sizeof(buf));
    if (!ret) {
        mdelay(40);
        return 0;
    }
    /* program */
    cts_info("tp reset in program mode");
    val = CTS_RST_BIT_MASK & CTS_CHIP_TP_RST_BIT;
    ret = cts_write_ddi_reg(cts_dev,
            cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_RESET_CONFIG,
             &val, 1, true);
    if (!ret) {
        mdelay(40);
        return 0;
    }
    return ret;
}

int cts_tcs_set_int_test(const struct cts_device *cts_dev, u8 enable)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_INT_TEST_EN_RW, &enable,
            sizeof(enable));
}

int cts_tcs_set_int_pin(const struct cts_device *cts_dev, u8 high)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_SET_INT_PIN_RW, &high,
            sizeof(high));
}

int cts_tcs_get_module_id(const struct cts_device *cts_dev, u32 *modId)
{
    u8 buf[4] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_MODULE_ID_RO, buf, sizeof(buf));
    if (ret == 0) {
        *modId = *(u32 *)buf;
    }
    return ret;
}


int cts_tcs_get_gestureinfo(struct cts_device *cts_dev,
        struct cts_touch_info_legacy *gesture_info)
{
    size_t size = cts_dev->fwdata.int_data_size;
    int ret;


    ret = cts_tcs_get_touch_data(cts_dev, CMD_TP_DATA_COORDINATES_RO,
            cts_dev->pdata->int_data, size);

    if (cts_tcs_clr_gstr_ready_flag(cts_dev)) {
        cts_err("Clear gesture ready flag failed");
    }
    if (ret < 0) {
        cts_err("Get gesture info failed: ret=%d", ret);
        return ret;
    }

    memcpy(gesture_info, cts_dev->pdata->int_data, sizeof(*gesture_info));

    return ret;
}

int cts_tcs_get_touchinfo(struct cts_device *cts_dev, void *touch_info)
{
    size_t size = cts_dev->fwdata.int_data_size;
    int ret;

    if (!size) {
        size = TOUCH_INFO_SIZE + INT_DATA_INFO_SIZE;
    }

    memset(touch_info, 0, size);

    cts_lock_device(cts_dev);
    ret = cts_tcs_get_touch_data(cts_dev, CMD_TP_DATA_COORDINATES_RO,
            cts_dev->pdata->int_data, size);
    cts_unlock_device(cts_dev);
    if (unlikely(ret != 0)) {
        cts_err("cts_tcs_get_touch_data failed");
        return ret;
    }

    memcpy(touch_info, cts_dev->pdata->int_data, size);
#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    if (unlikely((touch_info->debug_msg.reset_flag & 0xFFFFFF) != 0xFFFFFF
    || (touch_info->debug_msg.reset_flag & 0xFFFF) != 0xFFFF)) {
        cts_err("reset flag:0x%08x error", touch_info->debug_msg.reset_flag);
        cts_show_touch_debug_msg(&touch_info->debug_msg);
    }
#endif

    return ret;
}

int cts_tcs_get_touch_status(struct cts_device *cts_dev)
{
    return  cts_tcs_read(cts_dev, CMD_TP_DATA_STATUS_RO,
            cts_dev->pdata->int_data, TOUCH_INFO_SIZE);
}

int cts_tcs_get_workmode(const struct cts_device *cts_dev, u8 *workmode)
{
    u8 buf = 0;
    u16 tcs_cmd = CMD_SYS_STS_CURRENT_WORKMODE_RO;
    int ret;

    if (cts_dev->hwdata->support_cascade)
        tcs_cmd = CMD_SYS_STS_KRANG_CURRENT_WORKMODE_RO;

    ret = cts_tcs_read(cts_dev, tcs_cmd, &buf, sizeof(buf));
    if (ret == 0) {
        *workmode = buf;
    }

    return ret;
}

int cts_tcs_set_krang_stop(struct cts_device *cts_dev)
{
    u8 stop = 1;
    int ret;

    ret = cts_tcs_write(cts_dev, CMD_TP_STD_CMD_SET_KRANG_STOP,
            &stop, sizeof(stop));
    if (ret < 0) {
        cts_err("Set krang stop failed!");
    }

    return ret;
}

bool cts_tcs_is_krang_stopped(struct cts_device *cts_dev)
{
    u8 status;
    int ret;
    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_KRANG_WORK_STS_RO, &status, sizeof(status));
    if (ret) {
        cts_err("get krang status failed, ret:%d\n", ret);
        return false;
    }

    return (status == 0);
}

int cts_tcs_set_workmode(const struct cts_device *cts_dev, u8 workmode)
{
    u8 work_mode_current;
    u16 tcs_cmd = CMD_SYS_STS_WORK_MODE_RW;
    int ret, i = 0;

    if (cts_dev->rtdata.boot_mode == CTS_BOOT_MODE_PROGRAM) {
        cts_warn("%s: %d, sending tcs cmd under program mode!!!",
            __func__, __LINE__);
        return -EPERM;
    }

    cts_info("%s, set work mode to %d", __func__, workmode);
    ret = cts_tcs_get_workmode(cts_dev, &work_mode_current);
    if (workmode == work_mode_current)
        return ret;

    if (cts_dev->hwdata->support_cascade)
        tcs_cmd = CMD_SYS_STS_KRANG_WORK_MODE_RW;

    ret = cts_tcs_write(cts_dev, tcs_cmd,
            &workmode, sizeof(workmode));
    if (ret) {
        cts_err("sent tcs cmd(%x) failed %d", tcs_cmd, ret);
        return ret;
    }

    do {
		ret = cts_tcs_get_workmode(cts_dev, &work_mode_current);
		if (ret)
		    cts_err("Get real workmode failed %d", ret);
		else if (workmode == work_mode_current)
			return 0;

		mdelay(30);
    } while (i++ < CTS_RETRY + 5);

    return -EINVAL;
}

int cts_tcs_set_openshort_mode(const struct cts_device *cts_dev, u8 mode)
{
    return cts_tcs_write(cts_dev, CMD_OPENSHORT_MODE_SEL_RW, &mode,
            sizeof(mode));
}

int cts_tcs_get_curr_mode(const struct cts_device *cts_dev, u8 *currmode)
{
    u8 buf = 0;
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_CURRENT_WORKMODE_RO,
            &buf, sizeof(buf));
    if (ret == 0) {
        *currmode = buf;
    }

    return ret;
}

int cts_tcs_set_tx_vol(const struct cts_device *cts_dev, u8 txvol)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_VSTIM_LVL_RW, &txvol,
            sizeof(txvol));
}

int cts_tcs_set_short_test_type(const struct cts_device *cts_dev,
        u8 short_type)
{
    return cts_tcs_write(cts_dev, CMD_OPENSHORT_SHORT_SEL_RW,
            &short_type, sizeof(short_type));
}

int cts_tcs_is_openshort_enabled(const struct cts_device *cts_dev, u8 *enabled)
{
    u8 buf = 0;
    int ret;

    ret = cts_tcs_write(cts_dev, CMD_OPENSHORT_EN_RW, &buf, sizeof(buf));
    if (ret == 0) {
        *enabled = buf;
    }

    return ret;
}

int cts_tcs_set_openshort_enable(const struct cts_device *cts_dev, u8 enable)
{
    return cts_tcs_write(cts_dev, CMD_OPENSHORT_EN_RW, &enable, sizeof(enable));
}

int cts_tcs_set_esd_enable(const struct cts_device *cts_dev, u8 enable)
{
    return cts_tcs_write(cts_dev, CMD_DDI_ESD_EN_RW, &enable, sizeof(enable));
}

int cts_tcs_is_cneg_enabled(const struct cts_device *cts_dev, u8 *enabled)
{
    return cts_tcs_read(cts_dev, CMD_CNEG_EN_RW, enabled, sizeof(*enabled));
}

int cts_tcs_is_mnt_enabled(const struct cts_device *cts_dev, u8 *enabled)
{
    return cts_tcs_read(cts_dev, CMD_MNT_EN_RW, enabled, sizeof(*enabled));
}

int cts_tcs_set_cneg_enable(const struct cts_device *cts_dev, u8 enable)
{
    return cts_tcs_write(cts_dev, CMD_CNEG_EN_RW, &enable, sizeof(enable));
}

int cts_tcs_set_mnt_enable(const struct cts_device *cts_dev, u8 enable)
{
    return cts_tcs_write(cts_dev, CMD_MNT_EN_RW, &enable, sizeof(enable));
}

int cts_tcs_is_display_on(const struct cts_device *cts_dev, u8 *display_on)
{
    u8 buf = 0;
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_OPENSHORT_SHORT_DISP_ON_EN_RW,
            &buf, sizeof(buf));
    if (ret == 0) {
        *display_on = buf;
    }

    return ret;
}

int cts_tcs_set_pwr_mode(const struct cts_device *cts_dev, u8 pwr_mode)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_PWR_STATE_RW,
            &pwr_mode, sizeof(pwr_mode));
}

int cts_tcs_set_display_on(const struct cts_device *cts_dev, u8 display_on)
{
    return cts_tcs_write(cts_dev, CMD_OPENSHORT_SHORT_DISP_ON_EN_RW,
            &display_on, sizeof(display_on));
}


int cts_tcs_set_charger_plug(struct cts_device *cts_dev, u8 set)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set charger enable:%d", set);
    status->charger = set;

    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_CHARGER_PLUGIN_RW, &set, 1);
    if (ret < 0) {
        cts_info("Set charger failed!");
    }

    return ret;
}

int cts_tcs_get_charger_plug(const struct cts_device *cts_dev, u8 *isset)
{
    u8 buf = 0;
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_CHARGER_PLUGIN_RW,&buf, sizeof(buf));
    if (ret == 0) {
        *isset = buf;
    }

    return ret;
}

int cts_tcs_set_earjack_plug(struct cts_device *cts_dev, u8 set)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set earjack enable:%d", set);

    status->earjack = set;

    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_EP_PLUGIN_RW, &set, 1);
    if (ret) {
        cts_info("Set earjack failed!");
    }

    return ret;
}

int cts_tcs_get_earjack_plug(const struct cts_device *cts_dev, u8 *isset)
{
    u8 buf = 0;
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_EP_PLUGIN_RW, &buf, sizeof(buf));
    if (ret == 0) {
        *isset = buf;
    }

    return ret;
}

int cts_tcs_set_panel_direction(const struct cts_device *cts_dev, u8 direction)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_PANEL_DIRECTION_RW,
            &direction, sizeof(direction));
}

int cts_tcs_get_panel_direction(const struct cts_device *cts_dev, u8 *direction)
{
    u8 buf = 0;
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_PANEL_DIRECTION_RW,
            &buf, sizeof(buf));
    if (ret == 0) {
        *direction = buf;
    }

    return ret;
}

//USD: add for esports feature by jiangzhu.yang 20240826 start
int cts_tcs_game_mode_select(const struct cts_device *cts_dev, uint8_t cmd)
{
    u8 buf[2] = { 0 };
    int ret;

    switch (cmd) {
        case 1:
            buf[0] = 0x82;
            ret = cts_tcs_write(cts_dev, CMD_CTS_GAME_MODE,
            buf, sizeof(buf));
            break;
        case 0:
            buf[0] = 0x81;
            ret = cts_tcs_write(cts_dev, CMD_CTS_GAME_MODE,
            buf, sizeof(buf));
            break;
        default:
            cts_err("Invalid arg for mode");
            return 0;
        }
    return ret;
}
//USD: add for esports feature by jiangzhu.yang 20240826 end

//BSP:add for pocket mode by xihao.ding 20251129 start
int cts_tcs_set_pocket_mode(const struct cts_device *cts_dev, u8 mode)
{
    return cts_tcs_write(cts_dev,CMD_POCKET_FUNCTION_SUPPORT, &mode,
                         sizeof(mode));
}
//BSP:add for pocket mode by xihao.ding 20251129 end

int cts_tcs_set_game_mode(struct cts_device *cts_dev, u8 enable)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set game enable:%d", enable);

    status->game = enable;

    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_GAME_MODE_RW,
            &enable, sizeof(enable));
    if (ret) {
        cts_err("Set game failed!");
    }

    return ret;
}

int cts_tcs_get_game_mode(const struct cts_device *cts_dev, u8 *enabled)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_GAME_MODE_RW, buf, sizeof(buf));
    if (ret == 0) {
        *enabled = buf[0];
    }

    return ret;
}

int cts_tcs_get_has_int_data(const struct cts_device *cts_dev, bool *has)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_DATA_CAPTURE_SUPPORT_RO,
            buf, sizeof(buf));
    if (ret == 0) {
        *has = !!buf[0];
    }
    return ret;
}

int cts_tcs_get_int_data_types(const struct cts_device *cts_dev, u16 *type)
{
    u8 buf[2] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_DATA_CAPTURE_FUNC_MAP_RW,
            buf, sizeof(buf));
    if (ret == 0) {
        *type = buf[0] | (buf[1] << 8);
    }
    return ret;
}

int cts_tcs_set_int_data_types(const struct cts_device *cts_dev, u16 type)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_DATA_CAPTURE_FUNC_MAP_RW,
            (u8 *) &type, sizeof(type));
}

int cts_tcs_get_int_data_method(const struct cts_device *cts_dev, u8 *method)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_DATA_CAPTURE_EN_RW,
            buf, sizeof(buf));
    if (ret == 0) {
        *method = buf[0];
    }
    return ret;
}

int cts_tcs_set_int_data_method(const struct cts_device *cts_dev, u8 method)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_DATA_CAPTURE_EN_RW,
            &method, sizeof(method));
}

int cts_tcs_set_proximity_mode(struct cts_device *cts_dev, u8 enable)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set proximity enable:%d", enable);
    status->proximity = enable;

    ret = cts_tcs_write(cts_dev, CMD_PARA_PROXI_EN_RW, &enable, 1);
    if (ret != 0) {
        cts_err("Set proximity failed!");
    }

    return ret;
}

int cts_tcs_set_knuckle_mode(struct cts_device *cts_dev, u8 enable)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set knuckle enable:%d", enable);
    status->knuckle = enable;

    ret = cts_tcs_write(cts_dev, CMD_PARA_KUNCKLE_RW, &enable, 1);
    if (ret != 0) {
        cts_err("Set knuckle failed!");
    }

    return ret;
}

int cts_tcs_set_glove_mode(struct cts_device *cts_dev, u8 enable)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set glove enable:%d", enable);
    status->glove = enable;

    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_HI_SENSE_EN_RW, &enable, 1);
    if (ret != 0) {
        cts_err("Set glove failed!");
    }

    return ret;
}

int cts_tcs_set_pocket_enable(struct cts_device *cts_dev, u8 enable)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set pocket enable:%d", enable);
    status->pocket = enable;

    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_POCKET_MODE_EN_RW, &enable, 1);
    if (ret != 0) {
        cts_err("Set pocket failed!");
    }

    return ret;
}

void cts_tcs_reinit_fw_status(struct cts_device *cts_dev)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    cts_tcs_set_charger_plug(cts_dev, status->charger);
    cts_tcs_set_proximity_mode(cts_dev, status->proximity);
    cts_tcs_set_earjack_plug(cts_dev, status->earjack);
    cts_tcs_set_knuckle_mode(cts_dev, status->knuckle);
    cts_tcs_set_glove_mode(cts_dev, status->glove);
    cts_tcs_set_pocket_enable(cts_dev, status->pocket);
    cts_tcs_set_game_mode(cts_dev, status->game);
}


int cts_tcs_set_product_en(struct cts_device *cts_dev, u8 enable)
{
    int ret;
    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_PRODUCTION_TEST_EN_RW,
            &enable, sizeof(enable));
    if (ret) {
        cts_err("Set product_en failed!");
    }

    return ret;
}

//USD:modify for lcd aod by guoli.niu 20250321 start
int cts_tcs_set_aod_pwr_mode(const struct cts_device *cts_dev, u8 pwr_mode)
{
    u8 state = 0;
    int retry = 0;
    int ret = 0;
    for (retry = 0; retry < 20; retry++) {
        cts_tcs_write(cts_dev, CMD_SYS_STS_AOD_PWR_STATE_RW,
                      &pwr_mode, sizeof(pwr_mode));

        mdelay(5);

        cts_tcs_read(cts_dev, CMD_SYS_STS_AOD_PWR_STATE_RW,
                     &state, sizeof(state));

        if(state == pwr_mode)
            break;
    }

    if(retry == 20){
        cts_err("Set aod mode failed!");
        ret = -1;
    }

    return ret;
}
//USD:modify for lcd aod by guoli.niu 20250321 end