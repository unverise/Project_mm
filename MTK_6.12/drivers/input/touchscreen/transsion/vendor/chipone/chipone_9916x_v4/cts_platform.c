#define LOG_TAG         "Plat"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_firmware.h"
#include "cts_sysfs.h"
#include "cts_tcs.h"

#ifndef KEY_F26
#define KEY_F26 198
#endif

//USD: add spi cs timing by jiangzhu.yang start
#ifdef CONFIG_SPI_MT65XX
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
#include <linux/platform_data/spi-mt65xx.h>
#endif
#endif
//USD: add spi cs timing by jiangzhu.yang end
//zhw
static inline void _put_unaligned_be24(u32 v, void *p)
{
    u8 *puc = (u8 *) p;

    puc[0] = (v >> 16) & 0xFF;
    puc[1] = (v >> 8) & 0xFF;
    puc[2] = (v >> 0) & 0xFF;
}
//zhw

#ifdef CFG_CTS_FW_LOG_REDIRECT
size_t cts_plat_get_max_fw_log_size(struct cts_platform_data *pdata)
{
    return CTS_FW_LOG_BUF_LEN;
}

u8 *cts_plat_get_fw_log_buf(struct cts_platform_data *pdata, size_t size)
{
    return pdata->fw_log_buf;
}
#endif

size_t cts_plat_get_max_i2c_xfer_size(struct cts_platform_data *pdata)
{
    return CFG_CTS_MAX_I2C_XFER_SIZE;
}

#ifdef CONFIG_CTS_I2C_HOST
u8 *cts_plat_get_i2c_xfer_buf(struct cts_platform_data *pdata, size_t xfer_size)
{
    return pdata->i2c_fifo_buf;
}

int cts_plat_i2c_write(struct cts_platform_data *pdata, u8 i2c_addr,
                       const void *src, size_t len, int retry, int delay)
{
    int ret = 0, retries = 0;

    struct i2c_msg msg = {
        .flags = 0,
        .addr = i2c_addr,
        .buf = (u8 *) src,
        .len = len,
    };

    do {
        ret = i2c_transfer(pdata->i2c_client->adapter, &msg, 1);
        if (ret != 1) {
            if (ret >= 0)
                ret = -EIO;

            if (delay)
                mdelay(delay);
            continue;
        } else
            return 0;
    } while (++retries < retry);

    return ret;
}

int cts_plat_i2c_read(struct cts_platform_data *pdata, u8 i2c_addr,
                      const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
                      int retry, int delay)
{
    int num_msg, ret = 0, retries = 0;

    struct i2c_msg msgs[2] = {
        {
            .addr = i2c_addr,
            .flags = 0,
            .buf = (u8 *) wbuf,
            .len = wlen
        },
        {
            .addr = i2c_addr,
            .flags = I2C_M_RD,
            .buf = (u8 *) rbuf,
            .len = rlen
        }
    };

    if (wbuf == NULL || wlen == 0)
        num_msg = 1;
    else
        num_msg = 2;

    do {
        ret = i2c_transfer(pdata->i2c_client->adapter,
                           msgs + ARRAY_SIZE(msgs) - num_msg, num_msg);

        if (ret != num_msg) {
            if (ret >= 0)
                ret = -EIO;

            if (delay)
                mdelay(delay);
            continue;
        } else
            return 0;
    } while (++retries < retry);

    return ret;
}
#else
void dump_spi_common(const char *prefix, u8 *data, size_t datalen)
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

#ifdef CFG_CTS_MANUAL_CS
int cts_plat_set_cs(struct cts_platform_data *pdata, int val)
{
    if (val)
        gpio_set_value(pdata->cs_gpio, 1);
    else
        gpio_set_value(pdata->cs_gpio, 0);

    return 0;
}
#endif

int cts_plat_create_drw_packet(struct cts_platform_data *pdata, u32 addr,
    const void *src, const u32 len, struct cts_device_rtdata rtdata,
    bool is_read)
{
    int hcrc_cal = 0;
    int dcrc_cal = 0;
    int packet_len = -EINVAL;
    int hcrc_len = DRW_HCRC_OFFSET;

    if (rtdata.slave_addr != CTS_DEV_PROG_MODE_SPI_DRW_ADDR)
        cts_warn("%s, using wrong addr(%x) for spi drw transfer!!!",
        __func__, rtdata.slave_addr);
	
    if (is_read) {
        /* DRW READ:
        * MOSI ==> |CMD|ADDR|DATA_LEN|HCRC|     |    |    |     |    |    |
        * MISO ==> |   |    |        |    |DUMMY|DATA|DCRC|DUMMY|HACK|AACK|
        */
        pdata->spi_tx_buf[0] = rtdata.slave_addr | is_read;  //CMD
        _put_unaligned_be24(addr, &pdata->spi_tx_buf[DRW_ADDR_OFFSET]);
        _put_unaligned_be24(len, &pdata->spi_tx_buf[DRW_DATA_LEN_OFFSET]);

        if (!(rtdata.slave_addr & DRW_CRC_EN_MASK)) {
            hcrc_cal = cts_crc16(pdata->spi_tx_buf, hcrc_len);
            if (!(rtdata.slave_addr & DRW_CRC_INVERT_MASK)) {
                hcrc_cal = ~hcrc_cal;
            }
            put_unaligned_be16(hcrc_cal, &pdata->spi_tx_buf[DRW_HCRC_OFFSET]);
        }
    } else {
        /* DRW WRITE:
         * MOSI ==> |CMD|ADDR|DATA_LEN|HCRC|DUMMY|DATA|DCRC|DUMMY|    |    |
         * MISO ==> |   |    |        |    |     |    |    |     |HACK|AACK|
         */
        pdata->spi_tx_buf[0] = rtdata.slave_addr;  //CMD
        _put_unaligned_be24(addr, &pdata->spi_tx_buf[DRW_ADDR_OFFSET]);
        _put_unaligned_be24(len, &pdata->spi_tx_buf[DRW_DATA_LEN_OFFSET]);
        if (!(rtdata.slave_addr & DRW_CRC_EN_MASK)) {
            hcrc_cal = cts_crc16(pdata->spi_tx_buf, hcrc_len);
            dcrc_cal = cts_crc16(src, len);
            if (!(rtdata.slave_addr & DRW_CRC_INVERT_MASK)) {
                hcrc_cal = ~hcrc_cal;
                dcrc_cal = ~dcrc_cal;
            }
            put_unaligned_be16(hcrc_cal, &pdata->spi_tx_buf[DRW_HCRC_OFFSET]);
            put_unaligned_be16(dcrc_cal, &pdata->spi_tx_buf[DRW_DCRC_OFFSET(len)]);
        }
        memset(&pdata->spi_tx_buf[DRW_DUMMY_HEAD_OFFSET], 0xDD,
                DRW_DATA_OFFSET - DRW_DUMMY_HEAD_OFFSET);
        memcpy(&pdata->spi_tx_buf[DRW_DATA_OFFSET], src, len);
        memset(&pdata->spi_tx_buf[DRW_DUMMY_TAIL_OFFSET(len)], 0xDD,
                DRW_HACK_OFFSET(len) - DRW_DUMMY_TAIL_OFFSET(len));
    }

    packet_len = DRW_AACK_OFFSET(len) + 1;
    cts_info("%s:%d, packet_len = %d\n", __func__, __LINE__, packet_len);

    return packet_len;
}

int cts_spi_send_recv(struct cts_platform_data *pdata, u8 *tx_buffer,
        size_t len, u8 *rx_buffer)
{
    struct chipone_ts_data *cts_data;
    struct spi_message msg;
    struct spi_transfer cmd = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
        .delay_usecs = 0,
#endif
        .speed_hz = pdata->spi_speed * 1000u,
        .tx_buf = tx_buffer,
        .rx_buf = rx_buffer,
        .len = len,
        /* .tx_dma = 0, */
        /* .rx_dma = 0, */
    };
    int ret = 0;

    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
    cts_dbg("%s:%d, transfer length:%d\n", __func__, __LINE__, len);

#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(pdata, 0);
#endif
    spi_message_init(&msg);
    spi_message_add_tail(&cmd, &msg);
    ret = spi_sync(cts_data->spi_client, &msg);
    if (ret)
        cts_err("spi sync failed %d", ret);

    udelay(100);

#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(pdata, 1);
#endif
    return ret;
}

size_t cts_plat_get_max_spi_xfer_size(struct cts_platform_data *pdata)
{
    return CFG_CTS_MAX_SPI_XFER_SIZE;
}

u8 *cts_plat_get_spi_xfer_buf(struct cts_platform_data *pdata, size_t xfer_size)
{
    return pdata->spi_cache_buf;
}

int cts_plat_spi_write(struct cts_platform_data *pdata, u32 addr,
                       const void *src, size_t len, int retry, int delay)
{
    int ret = 0, retries = 0;

    struct cts_device *cts_dev = NULL;
    size_t packet_len;
    u8 hack = 0;
    u8 aack = 0;
    bool crc_check_en = false;

    if(!pdata || !(pdata->cts_dev)) {
        cts_err("%s:%d, invalid parameter!\n", __func__, __LINE__);
        return -EINVAL;
    }
 

    if (len > CFG_CTS_MAX_SPI_XFER_SIZE) {
        cts_err("write too much data:wlen=%zu", len);
        return -EIO;
    }

    cts_dev = pdata->cts_dev;
    if (cts_dev->rtdata.program_mode) {
#ifdef CTS_DRW_PROTOCOL
         packet_len = cts_plat_create_drw_packet(pdata, addr, src, len,
                      cts_dev->rtdata, 0);

        do {
             ret = cts_spi_send_recv(pdata, pdata->spi_tx_buf, packet_len,
             pdata->spi_rx_buf);

            if (ret) {
                cts_err("SPI write failed %d", ret);
                if (delay)
                    mdelay(delay);
                continue;
            }
            crc_check_en = !(cts_dev->rtdata.slave_addr & DRW_CRC_EN_MASK);
            if (crc_check_en) {
                hack = pdata->spi_rx_buf[DRW_HACK_OFFSET(len)];
                aack = pdata->spi_rx_buf[DRW_AACK_OFFSET(len)];
                if (hack == ACK_VALID && aack == ACK_VALID)
                    return ret;
                continue;
            }
            return ret;
        } while (++retries < retry);
#else
        cts_warn("%s:%d, PRW_MODE is not support!\n", __func__, __LINE__);
        return -EPROTONOSUPPORT;

#endif
    }
    return ret;
}

int cts_plat_spi_read(struct cts_platform_data *pdata, u32 addr, void *rbuf,
    size_t rlen, int retry, int delay)

{
    int ret = 0, retries = 0;
    u16 crc16_calc, crc16_recv;
    size_t packet_len;
    struct cts_device *cts_dev = NULL;
    bool crc_check_en = false;
    bool crc_invert_en = false;

    if(!pdata || !(pdata->cts_dev)) {
       cts_err("%s:%d, invalid parameter!\n", __func__, __LINE__);
       return -EINVAL;
    }

    if (rlen > CFG_CTS_MAX_SPI_XFER_SIZE) {
        cts_err("read too much rlen=%zd", rlen);
        return -EIO;
    }

    cts_dev = pdata->cts_dev;
    if (cts_dev->rtdata.program_mode) {
#ifdef CTS_DRW_PROTOCOL
       packet_len = cts_plat_create_drw_packet(pdata, addr, NULL, rlen,
                    cts_dev->rtdata, 1);

        do {
           ret = cts_spi_send_recv(pdata, pdata->spi_tx_buf, packet_len,
                                    pdata->spi_rx_buf);
            if (ret) {
                cts_err("SPI read failed %d", ret);
                if (delay)
                    mdelay(delay);
                continue;
            }
            memcpy(rbuf, pdata->spi_rx_buf + DRW_DATA_OFFSET, rlen);
            crc16_calc = (u16) cts_crc16(rbuf, rlen);
            crc16_recv = get_unaligned_be16(&pdata->spi_rx_buf[DRW_DCRC_OFFSET(rlen)]);
            crc_check_en = !(cts_dev->rtdata.slave_addr & DRW_CRC_EN_MASK);
            crc_invert_en = !(cts_dev->rtdata.slave_addr & DRW_CRC_INVERT_MASK);
            if (crc_check_en) {
                if (crc_invert_en && crc16_recv != (uint16_t)(~crc16_calc)) {
                        cts_warn("SPI RX CRC error: rx_crc %04x != %04x",
                            crc16_recv, ~crc16_calc);
                        continue;
                } else if (!crc_invert_en && crc16_recv != (uint16_t)(crc16_calc)) {
                    cts_warn("SPI RX CRC error: rx_crc %04x != %04x",
                        crc16_recv, crc16_calc);
                    continue;
                }
                return ret;
            }
            return ret;
        } while (++retries < retry);
#else
     cts_warn("%s:%d, PRW_MODE is not support!\n", __func__, __LINE__);
     return -EPROTONOSUPPORT;
#endif

    }
    if (retries >= retry)
        cts_err("SPI read too much retry");

    return -EIO;
}

int cts_plat_spi_read_delay_idle(struct cts_platform_data *pdata, u32 addr,
        void *rbuf, size_t rlen, int retry, int delay, int idle)
{
    int ret = 0, retries = 0;
    u16 crc16_calc, crc16_recv;
    size_t packet_len = 0;
    struct cts_device *cts_dev = NULL;
    bool crc_check_en = false;
    bool crc_invert_en = false;

    if (rlen > CFG_CTS_MAX_SPI_XFER_SIZE) {
    cts_err("read too much data, rlen=%zu", rlen);
    return -E2BIG;
    }

    if(!pdata || !(pdata->cts_dev)) {
       cts_err("%s:%d, invalid parameter!\n", __func__, __LINE__);
       return -EINVAL;
    }
    cts_dev = pdata->cts_dev;

    if (cts_dev->rtdata.program_mode) {
#ifdef CTS_DRW_PROTOCOL
        packet_len = cts_plat_create_drw_packet(pdata, addr, NULL, rlen,
        cts_dev->rtdata, 1);
        do {
            ret = cts_spi_send_recv(pdata, pdata->spi_tx_buf,
            packet_len, pdata->spi_rx_buf);

            if (ret) {
                cts_err("SPI read failed %d", ret);
                if (delay)
                    mdelay(delay);
                continue;
            }
             memcpy(rbuf, pdata->spi_rx_buf + DRW_DATA_OFFSET, rlen);
             crc16_calc = (u16) cts_crc32(rbuf, rlen);
             crc16_recv = get_unaligned_be16(&pdata->spi_rx_buf[DRW_DCRC_OFFSET(rlen)]);
             crc_check_en = !(cts_dev->rtdata.slave_addr & DRW_CRC_EN_MASK);
             crc_invert_en = !(cts_dev->rtdata.slave_addr & DRW_CRC_INVERT_MASK);
             if (crc_check_en) {
                 if (crc_invert_en && crc16_recv != (uint16_t)(~crc16_calc)) {
                     cts_warn("SPI RX CRC error: rx_crc %04x != %04x",
                               crc16_recv, ~crc16_calc);
                     continue;
                 } else if (!crc_invert_en && crc16_recv != (uint16_t)(crc16_calc)) {
                   cts_warn("SPI RX CRC error: rx_crc %04x != %04x",
                             crc16_recv, crc16_calc);
                    continue;
                }
                return ret;
            }
            return ret;
        } while (++retries < retry);
#else
     cts_warn("%s:%d, PRW_MODE is not support!\n", __func__, __LINE__);
    return -EPROTONOSUPPORT;
#endif

    }
    if (retries >= retry)
        cts_err("cts_plat_spi_read error");

    return -EIO;
}
#endif

int cts_plat_is_normal_mode(struct cts_platform_data *pdata)
{
    u16 fwid;
    /*
        u8 tx_buf[4] = { 0 };
        u32 addr;
    */
    int ret;

    cts_set_normal_addr(pdata->cts_dev);
    ret = cts_tcs_get_fw_id(pdata->cts_dev, &fwid);
    /*
            addr = CTS_DEVICE_FW_REG_CHIP_TYPE;
            put_unaligned_be16(addr, tx_buf);
            ret = cts_plat_spi_read(pdata, CTS_DEV_NORMAL_MODE_SPIADDR,
                    tx_buf, 2, &fwid, 2, 3, 10);
            fwid = be16_to_cpu(fwid);
    */
    if (ret || !cts_is_fwid_valid(fwid))
        return false;
    pdata->cts_dev->rtdata.program_mode = false;

    return true;
}

bool g_int_pin_test_value = true;
static void cts_plat_handle_test_irq(struct cts_platform_data *pdata)
{
    cts_info("Handle test IRQ");
    g_int_pin_test_value = !CTS_INT_DEFAULT_TEST_VAL;
}

static void cts_plat_handle_irq(struct cts_platform_data *pdata)
{
    int ret;

    cts_dbg("Handle IRQ");

    cts_lock_device(pdata->cts_dev);
    ret = cts_irq_handler(pdata->cts_dev);
    if (ret)
        cts_err("Device handle IRQ failed %d", ret);
    cts_unlock_device(pdata->cts_dev);
}

static irqreturn_t cts_plat_test_irq_handler(int irq, void *dev_id)
{
    struct cts_platform_data *pdata;
#ifndef CONFIG_GENERIC_HARDIRQS
    struct chipone_ts_data *cts_data;
#endif

    cts_info("IRQ test handler");

    pdata = (struct cts_platform_data *)dev_id;
    if (pdata == NULL) {
        cts_err("IRQ test handler with NULL dev_id");
        return IRQ_NONE;
    }
#ifdef CONFIG_GENERIC_HARDIRQS
    cts_plat_handle_test_irq(pdata);
#else
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

    if (queue_work(cts_data->workqueue, &pdata->ts_test_irq_work)) {
        cts_info("IRQ test queue work");
        cts_plat_disable_irq(pdata);
    } else
        cts_warn("IRQ test handler queue work failed as already on the queue");
#endif /* CONFIG_GENERIC_HARDIRQS */

    return IRQ_HANDLED;
}

static irqreturn_t cts_plat_irq_handler(int irq, void *dev_id)
{
    struct cts_platform_data *pdata;
#ifndef CONFIG_GENERIC_HARDIRQS
    struct chipone_ts_data *cts_data;
#endif

    cts_dbg("IRQ handler");

    pdata = (struct cts_platform_data *)dev_id;
    if (pdata == NULL) {
        cts_err("IRQ handler with NULL dev_id");
        return IRQ_NONE;
    }
#ifdef CONFIG_GENERIC_HARDIRQS
    cts_plat_handle_irq(pdata);
#else
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

    if (queue_work(cts_data->workqueue, &pdata->ts_irq_work)) {
        cts_dbg("IRQ queue work");
        cts_plat_disable_irq(pdata);
    } else
        cts_warn("IRQ handler queue work failed as already on the queue");
#endif /* CONFIG_GENERIC_HARDIRQS */

    return IRQ_HANDLED;
}

#ifndef CONFIG_GENERIC_HARDIRQS
static void cts_plat_touch_dev_test_irq_work(struct work_struct *work)
{
    struct cts_platform_data *pdata =
        container_of(work, struct cts_platform_data, ts_test_irq_work);

    cts_dbg("IRQ work");

    cts_plat_handle_test_irq(pdata);

    cts_plat_enable_irq(pdata);
}

static void cts_plat_touch_dev_irq_work(struct work_struct *work)
{
    struct cts_platform_data *pdata =
        container_of(work, struct cts_platform_data, ts_irq_work);

    cts_dbg("IRQ work");

    cts_plat_handle_irq(pdata);

    cts_plat_enable_irq(pdata);
}
#endif /* CONFIG_GENERIC_HARDIRQS */

#ifdef CONFIG_CTS_OF
static int cts_plat_parse_dt(struct cts_platform_data *pdata,
                             struct device_node *dev_node)
{
    int ret = 0;

    cts_info("Parse device tree");

    pdata->int_gpio = of_get_named_gpio(dev_node, CFG_CTS_OF_INT_GPIO_NAME, 0);
    if (!gpio_is_valid(pdata->int_gpio)) {
        cts_err("Parse INT GPIO from dt failed %d", pdata->int_gpio);
        pdata->int_gpio = -1;
    }
    cts_info("  %-12s: %d", "int gpio", pdata->int_gpio);

    pdata->irq = gpio_to_irq(pdata->int_gpio);
    if (pdata->irq < 0) {
        cts_err("Parse irq failed %d", ret);
        return pdata->irq;
    }
    cts_info("  %-12s: %d", "irq num", pdata->irq);

#ifdef CFG_CTS_HAS_RESET_PIN
    pdata->rst_gpio = of_get_named_gpio(dev_node, CFG_CTS_OF_RST_GPIO_NAME, 0);
    if (!gpio_is_valid(pdata->rst_gpio)) {
        cts_err("Parse RST GPIO from dt failed %d", pdata->rst_gpio);
        pdata->rst_gpio = -1;
    }
    cts_info("  %-12s: %d", "rst gpio", pdata->rst_gpio);
#endif /* CFG_CTS_HAS_RESET_PIN */

#ifdef CFG_CTS_MANUAL_CS
    pdata->cs_gpio = of_get_named_gpio(dev_node, CFG_CTS_OF_CS_GPIO_NAME, 0);
    if (!gpio_is_valid(pdata->cs_gpio)) {
        cts_err("Parse CS GPIO from dt failed %d", pdata->cs_gpio);
        pdata->cs_gpio = -1;
    }
    cts_info("  %-12s: %d", "cs gpio", pdata->cs_gpio);
#endif

    ret = of_property_read_u32(dev_node, CFG_CTS_OF_X_RESOLUTION_NAME,
                               &pdata->res_x);
    if (ret)
        cts_warn("Parse X resolution from dt failed %d", ret);

    cts_info("  %-12s: %d", "X resolution", pdata->res_x);

    ret = of_property_read_u32(dev_node, CFG_CTS_OF_Y_RESOLUTION_NAME,
                               &pdata->res_y);
    if (ret)
        cts_warn("Parse Y resolution from dt failed %d", ret);

    cts_info("  %-12s: %d", "Y resolution", pdata->res_y);

    if (of_property_read_u32(dev_node, "chipone,def-build-id", &pdata->build_id)) {
        pdata->build_id = 0;
        cts_info("chipone,build_id undefined.");
    } else
        cts_info("chipone,build_id=0x%04X", pdata->build_id);

    if (of_property_read_u32(dev_node, "chipone,def-config-id", &pdata->config_id)) {
        pdata->config_id = 0;
        cts_info("chipone,config_id undefined.");
    } else
        cts_info("chipone,config_id=0x%04X", pdata->config_id);

    //BSP:add for pocket mode by xihao.ding 20251129 start
    pdata->pocket_function_support = of_property_read_bool(dev_node, "chipone,pocket-function-support");
    cts_info("pocket_function_support is %d", pdata->pocket_function_support);
    //BSP:add for pocket mode by xihao.ding 20251129 end
#ifdef CFG_CTS_FW_UPDATE_SYS
    ret = of_property_read_string(dev_node, CFG_CTS_OF_PANEL_SUPPLIER,
                                  &pdata->panel_supplier);
    if (ret) {
        pdata->panel_supplier = NULL;
        cts_warn("read panel supplier failed, ret=%d", ret);
    } else
        cts_info("panel supplier=%s", (char *)pdata->panel_supplier);
#endif

    return 0;
}
#endif /* CONFIG_CTS_OF */

#ifdef CFG_CTS_FORCE_UP
static void cts_plat_touch_event_timeout_work(struct work_struct *work)
{
    struct cts_platform_data *pdata = container_of(work,
                                      struct cts_platform_data, touch_event_timeout_work.work);

    cts_warn("Touch event timeout work");

    cts_plat_release_all_touch(pdata);
}
#endif

///USD: add spi cs timing by jiangzhu.yang start
#ifdef CONFIG_SPI_MT65XX
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
static struct mtk_chip_config spi_ctrdata = {
	.cs_setuptime = 100, //25*9.6ns  960ns == 100*9.6 ns
	//.rx_mlsb = 1,
	//.tx_mlsb = 1,
};
#endif
#endif
//USD: add spi cs timing by jiangzhu.yang end
#ifndef CONFIG_CTS_I2C_HOST
int cts_plat_spi_setup(struct cts_platform_data *pdata)
{
    int ret;
 
    //TODO: SPI setting is configured by hardcode!!!
    //pdata->spi_client->chip_select = 1;
    pdata->spi_client->mode = SPI_MODE_0;
    pdata->spi_client->bits_per_word = 8;
    //USD: add spi cs timing by jiangzhu.yang start
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    pdata->spi_client->cs_setup.value = 1; //delay 1
    pdata->spi_client->cs_setup.unit = 0;  //delay us
    #else
    #ifdef CONFIG_SPI_MT65XX
    pdata->spi_client->controller_data = (void *)& spi_ctrdata;
    #endif
    #endif
    //USD: add spi cs timing by jiangzhu.yang end

    //cts_info("chip_select  :%d", pdata->spi_client->chip_select);
    cts_info("spi_mode     :%d", pdata->spi_client->mode);
    cts_info("bits_per_word:%d", pdata->spi_client->bits_per_word);

    ret = spi_setup(pdata->spi_client);
    if (ret)
        cts_err("spi_setup err!");
    return 0;
}
#endif

#ifdef CONFIG_CTS_I2C_HOST
int cts_init_platform_data(struct cts_platform_data *pdata,
                           struct i2c_client *i2c_client)
#else
int cts_init_platform_data(struct cts_platform_data *pdata,
                           struct spi_device *spi)
#endif
{
    struct input_dev *input_dev;
    int ret = 0;

    cts_info("cts_init_platform_data Init");

#ifdef CONFIG_CTS_OF
    {
        struct device *dev;

#ifdef CONFIG_CTS_I2C_HOST
        dev = &i2c_client->dev;
#else
        dev = &spi->dev;
#endif /* CONFIG_CTS_I2C_HOST */
        ret = cts_plat_parse_dt(pdata, dev->of_node);
        if (ret) {
            cts_err("Parse dt failed %d", ret);
            return ret;
        }
    }
#endif /* CONFIG_CTS_OF */

#ifdef CONFIG_CTS_I2C_HOST
    pdata->i2c_client = i2c_client;
    pdata->i2c_client->irq = pdata->irq;
#else
    pdata->spi_client = spi;
    pdata->spi_client->irq = pdata->irq;
#endif /* CONFIG_CTS_I2C_HOST */

    mutex_init(&pdata->dev_lock);
    spin_lock_init(&pdata->irq_lock);

    input_dev = input_allocate_device();
    if (input_dev == NULL) {
        cts_err("Failed to allocate input device.");
        return -ENOMEM;
    }


    /** - Init input device */
    input_dev->name = CFG_CTS_DEVICE_NAME;
#ifdef CONFIG_CTS_I2C_HOST
    input_dev->id.bustype = BUS_I2C;
    input_dev->dev.parent = &pdata->i2c_client->dev;
#else
    input_dev->id.bustype = BUS_SPI;
    input_dev->dev.parent = &pdata->spi_client->dev;
#endif /* CONFIG_CTS_I2C_HOST */
    input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
#ifdef CFG_CTS_PALM_DETECT
    set_bit(CFG_CTS_PALM_EVENT, input_dev->keybit);
#endif

#ifdef CFG_CTS_SWAP_XY
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->res_y, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->res_x, 0, 0);
#else /* CFG_CTS_SWAP_XY */
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->res_x, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->res_y, 0, 0);
#endif /* CFG_CTS_SWAP_XY */

    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,
                         CFG_CTS_MAX_TOUCH_NUM * 2, 0, 0);

    input_set_capability(input_dev, EV_KEY, BTN_TOUCH);
    /*bsp: add for bringup by guoxin.zhu 20230608 start*/
    input_set_capability(input_dev, EV_KEY, KEY_F26);
    input_set_capability(input_dev, EV_KEY, KEY_F20);
    /*bsp: add for bringup by guoxin.zhu 20230608 end*/
    //BSP:add for pocket mode by xihao.ding 20251129 start
    input_set_capability(input_dev, EV_KEY, KEY_LARGE_AREA);
    //BSP:add for pocket mode by xihao.ding 20251129 end

    input_set_capability(input_dev, EV_KEY, BTN_TOUCH);

#ifdef CONFIG_CTS_SLOTPROTOCOL
    input_mt_init_slots(input_dev, CFG_CTS_MAX_TOUCH_NUM, 0);
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
    __set_bit(EV_ABS, input_dev->evbit);
    input_set_drvdata(input_dev, pdata);
    ret = input_register_device(input_dev);
    if (ret) {
        cts_err("Failed to register input device");
        return ret;
    }

    pdata->ts_input_dev = input_dev;
    /*bsp: add for bringup by guoxin.zhu 20230608 start*/
    chipone_controller.input_dev = input_dev;
    /*bsp: add for bringup by guoxin.zhu 20230608 end*/

#if !defined(CONFIG_GENERIC_HARDIRQS)
    INIT_WORK(&pdata->ts_test_irq_work, cts_plat_touch_dev_test_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */

#ifdef CONFIG_CTS_VIRTUALKEY
    {
        u8 vkey_keymap[CFG_CTS_NUM_VKEY] = CFG_CTS_VKEY_KEYCODES;

        memcpy(pdata->vkey_keycodes, vkey_keymap, sizeof(vkey_keymap));
        pdata->vkey_num = CFG_CTS_NUM_VKEY;
    }
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CFG_CTS_GESTURE
    {
        u8 gesture_keymap[CFG_CTS_NUM_GESTURE][2] = CFG_CTS_GESTURE_KEYMAP;

        memcpy(pdata->gesture_keymap, gesture_keymap, sizeof(gesture_keymap));
        pdata->gesture_num = CFG_CTS_NUM_GESTURE;
    }
#endif /* CFG_CTS_GESTURE */

#ifdef CFG_CTS_FORCE_UP
    INIT_DELAYED_WORK(&pdata->touch_event_timeout_work,
                      cts_plat_touch_event_timeout_work);
#endif

#ifndef CONFIG_CTS_I2C_HOST
    pdata->spi_speed = CFG_CTS_SPI_SPEED_KHZ;
    cts_plat_spi_setup(pdata);
#endif
    return 0;
}

int cts_deinit_platform_data(struct cts_platform_data *pdata)
{
    cts_info("De-Init platform_data");
    input_unregister_device(pdata->ts_input_dev);
    return 0;
}

int cts_plat_request_resource(struct cts_platform_data *pdata)
{
    int ret;

    cts_info("Request resource");

    ret = gpio_request_one(pdata->int_gpio, GPIOF_IN,
                           CFG_CTS_DEVICE_NAME "-int");
    if (ret) {
        cts_err("Request INT gpio (%d) failed %d", pdata->int_gpio, ret);
        goto err_out;
    }
#ifdef CFG_CTS_HAS_RESET_PIN
    ret = gpio_request_one(pdata->rst_gpio, GPIOF_OUT_INIT_HIGH,
                           CFG_CTS_DEVICE_NAME "-rst");
    if (ret) {
        cts_err("Request RST gpio (%d) failed %d", pdata->rst_gpio, ret);
        goto err_free_int;
    }
#endif /* CFG_CTS_HAS_RESET_PIN */

#ifdef CFG_CTS_MANUAL_CS
    ret = gpio_request_one(pdata->cs_gpio, GPIOF_OUT_INIT_HIGH,
                           CFG_CTS_DEVICE_NAME "-cs");
    if (ret) {
        cts_err("Request CS gpio (%d) failed %d", pdata->cs_gpio, ret);
        goto err_request_cs_gpio;
    }
#endif

    return 0;

#ifdef CFG_CTS_MANUAL_CS
err_request_cs_gpio:
#endif

#ifdef CONFIG_CTS_REGULATOR
err_free_rst:
#endif /* CONFIG_CTS_REGULATOR */
#ifdef CFG_CTS_HAS_RESET_PIN
    gpio_free(pdata->rst_gpio);
err_free_int:
#endif /* CFG_CTS_HAS_RESET_PIN */
    gpio_free(pdata->int_gpio);
err_out:
    return ret;
}

void cts_plat_free_resource(struct cts_platform_data *pdata)
{
    cts_info("Free resource");

    if (gpio_is_valid(pdata->int_gpio))
        gpio_free(pdata->int_gpio);

#ifdef CFG_CTS_HAS_RESET_PIN
    if (gpio_is_valid(pdata->rst_gpio))
        gpio_free(pdata->rst_gpio);

#endif /* CFG_CTS_HAS_RESET_PIN */
#ifdef CFG_CTS_MANUAL_CS
    if (gpio_is_valid(pdata->cs_gpio))
        gpio_free(pdata->cs_gpio);

#endif
}

int cts_plat_request_test_irq(struct cts_platform_data *pdata)
{
    int ret;

    cts_info("Request test IRQ");

#ifdef CONFIG_GENERIC_HARDIRQS
    ret = request_threaded_irq(pdata->irq, NULL, cts_plat_test_irq_handler,
            IRQF_TRIGGER_FALLING | IRQF_ONESHOT, CFG_CTS_DRIVER_NAME, pdata);
#else /* CONFIG_GENERIC_HARDIRQS */
    ret = request_irq(pdata->irq, cts_plat_test_irq_handler,
            IRQF_TRIGGER_FALLING | IRQF_ONESHOT, CFG_CTS_DRIVER_NAME, pdata);
#endif /* CONFIG_GENERIC_HARDIRQS */
    if (ret) {
        cts_err("Request test IRQ failed %d", ret);
        return ret;
    }

    cts_plat_disable_irq(pdata);

    return 0;
}

int cts_plat_request_irq(struct cts_platform_data *pdata)
{
    int ret;

    cts_info("Request IRQ");

#ifdef CONFIG_GENERIC_HARDIRQS
    ret = request_threaded_irq(pdata->irq, NULL, cts_plat_irq_handler,
                               IRQF_TRIGGER_FALLING | IRQF_ONESHOT, CFG_CTS_DRIVER_NAME, pdata);
#else /* CONFIG_GENERIC_HARDIRQS */
    ret = request_irq(pdata->irq, cts_plat_irq_handler,
                      IRQF_TRIGGER_FALLING | IRQF_ONESHOT, CFG_CTS_DRIVER_NAME, pdata);
#endif /* CONFIG_GENERIC_HARDIRQS */
    if (ret) {
        cts_err("Request IRQ failed %d", ret);
        return ret;
    }

    cts_plat_disable_irq(pdata);

    return 0;
}

int cts_switch_to_test_irq(struct cts_platform_data *pdata)
{
    int ret;

#ifndef CONFIG_GENERIC_HARDIRQS
    if (work_pending(&pdata->ts_irq_work))
        cancel_work_sync(&pdata->ts_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */
    cts_plat_free_irq(pdata);

    ret = cts_plat_request_test_irq(pdata);
    if (ret) {
        cts_err("request rest irq failed");
        goto restore_irq;
    }

    ret = cts_plat_enable_irq(pdata);
    if (ret) {
        cts_err("enable test irq failed");
        goto enable_irq_err;
    }
    return ret;

enable_irq_err:
    cts_plat_free_irq(pdata);
restore_irq:
    if (cts_plat_request_irq(pdata))
        cts_err("restore normal irq handler failed");

    return ret;
}

int restore_normal_irq(struct cts_platform_data *pdata)
{
    int ret;

#ifndef CONFIG_GENERIC_HARDIRQS
    if (work_pending(&pdata->ts_test_irq_work))
        cancel_work_sync(&pdata->ts_test_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */
    cts_plat_free_irq(pdata);

    ret = cts_plat_request_irq(pdata);
    if (ret)
        cts_err("restore normal irq handler failed");

    return ret;
}

void cts_plat_free_irq(struct cts_platform_data *pdata)
{
    free_irq(pdata->irq, pdata);
    /* clear the flag to prevent unbanlanced irq dis/enable */
    pdata->irq_is_disable = false;
}

int cts_plat_enable_irq(struct cts_platform_data *pdata)
{
    unsigned long irqflags;

    cts_dbg("Enable IRQ");

    if (pdata->irq > 0) {
        spin_lock_irqsave(&pdata->irq_lock, irqflags);
        if (pdata->irq_is_disable) {/* && !cts_is_device_suspended(pdata->chip)) */
            cts_dbg("Real enable IRQ");
            enable_irq(pdata->irq);
            pdata->irq_is_disable = false;
        }
        spin_unlock_irqrestore(&pdata->irq_lock, irqflags);

        return 0;
    }

    return -ENODEV;
}

int cts_plat_disable_irq(struct cts_platform_data *pdata)
{
    unsigned long irqflags;

    cts_dbg("Disable IRQ");

    if (pdata->irq > 0) {
        spin_lock_irqsave(&pdata->irq_lock, irqflags);
        if (!pdata->irq_is_disable) {
            cts_dbg("Real disable IRQ");
            disable_irq_nosync(pdata->irq);
            pdata->irq_is_disable = true;
        }
        spin_unlock_irqrestore(&pdata->irq_lock, irqflags);

        return 0;
    }

    return -ENODEV;
}

#ifdef CFG_CTS_HAS_RESET_PIN
int cts_plat_reset_device(struct cts_platform_data *pdata)
{
    /* !!!can not be modified */
    /* !!!can not be modified */
    /* !!!can not be modified */
   if(!pdata) {
        cts_info("%s:%d, invalid input: pdata\n", __func__, __LINE__);
        return -EINVAL;
    }
    gpio_set_value(pdata->rst_gpio, 1);
    mdelay(1);
    gpio_set_value(pdata->rst_gpio, 0);
    mdelay(10);
    gpio_set_value(pdata->rst_gpio, 1);
    mdelay(40);

    return 0;
}

int cts_plat_set_reset(struct cts_platform_data *pdata, int val)
{
    cts_info("Set Reset to %s", val ? "HIGH" : "LOW");
    if (val)
        gpio_set_value(pdata->rst_gpio, 1);
    else
        gpio_set_value(pdata->rst_gpio, 0);

    return 0;
}
#endif /* CFG_CTS_HAS_RESET_PIN */

int cts_plat_get_int_pin(struct cts_platform_data *pdata)
{
    return gpio_get_value(pdata->int_gpio);
}

int cts_plat_power_up_device(struct cts_platform_data *pdata)
{
    cts_info("Power up device");

    return 0;
}

int cts_plat_power_down_device(struct cts_platform_data *pdata)
{
    cts_info("Power down device");

    return 0;
}

int cts_plat_init_touch_device(struct cts_platform_data *pdata)
{
    cts_info("Init touch device");

    return 0;
}

void cts_plat_deinit_touch_device(struct cts_platform_data *pdata)
{
    cts_info("De-init touch device");

#ifndef CONFIG_GENERIC_HARDIRQS
    if (work_pending(&pdata->ts_irq_work))
        cancel_work_sync(&pdata->ts_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */
}

#ifdef CFG_CTS_PALM_DETECT
void cts_report_palm_event(struct cts_platform_data *pdata)
{
    input_report_key(pdata->ts_input_dev, CFG_CTS_PALM_EVENT, 1);
    input_sync(pdata->ts_input_dev);
    msleep(100);
    input_report_key(pdata->ts_input_dev, CFG_CTS_PALM_EVENT, 0);
    input_sync(pdata->ts_input_dev);
}
#endif

int cts_plat_process_touch_msg(struct cts_platform_data *pdata,
                               struct cts_device_touch_msg *msgs, int num)
{
    struct chipone_ts_data *cts_data;
    struct input_dev *input_dev = pdata->ts_input_dev;
    int i;
    int contact = 0;
    //BSP: add for factory mode by hongwu.zhan 20240108 start
    static u16 touch_x = 0;
    static u16 touch_y = 0;
    int boot_mode = 0;
    int resolution_multiples = chipone_controller.supplier->resolution_multiples;
    //BSP: add for factory mode by hongwu.zhan 20240108 end
#ifdef CONFIG_CTS_SLOTPROTOCOL
    static unsigned char finger_last[CFG_CTS_MAX_TOUCH_NUM] = { 0 };
    unsigned char finger_current[CFG_CTS_MAX_TOUCH_NUM] = { 0 };
#endif
    boot_mode = chipone_controller.tran_ts_data->boot_mode;

    cts_dbg("Process touch %d msgs", num);

    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

    if (num == 0 || num > CFG_CTS_MAX_TOUCH_NUM)
        return 0;

    for (i = 0; i < num; i++) {
        u16 x, y;

        x = le16_to_cpu(msgs[i].x);
        y = le16_to_cpu(msgs[i].y);
        //BSP: add for factory mode by hongwu.zhan 20240108 start
        if(boot_mode == TRAN_FACTORY_MODE) {
           x = x / resolution_multiples;
           y = y / resolution_multiples;
           touch_x = x;
           touch_y = y;
        } else {
           touch_x = x / resolution_multiples;
           touch_y = y / resolution_multiples;
        }
        //BSP: add for factory mode by hongwu.zhan 20240108 end

#ifdef CFG_CTS_SWAP_XY
        swap(x, y);
#endif /* CFG_CTS_SWAP_XY */
#ifdef CFG_CTS_WRAP_X
        x = wrap(pdata->res_x, x);
#endif /* CFG_CTS_WRAP_X */
#ifdef CFG_CTS_WRAP_Y
        y = wrap(pdata->res_y, y);
#endif /* CFG_CTS_WRAP_Y */
        cts_dbg("  Process touch msg[%d]: id[%u] ev=%u x=%u y=%u p=%u",
                i, msgs[i].id, msgs[i].event, x, y, msgs[i].pressure);
        if (msgs[i].event == CTS_DEVICE_TOUCH_EVENT_DOWN
                || msgs[i].event == CTS_DEVICE_TOUCH_EVENT_MOVE
                || msgs[i].event == CTS_DEVICE_TOUCH_EVENT_STAY) {
            if (msgs[i].id < CFG_CTS_MAX_TOUCH_NUM)
                finger_current[msgs[i].id] = 1;
        }
#ifdef CONFIG_CTS_SLOTPROTOCOL
        /* input_mt_slot(input_dev, msgs[i].id); */
        /*BSP:add for debug by guoxin.zhu 20230223 start*/
        do {
            if(msgs[i].id == 0) {
                if(finger_current[msgs[i].id] == 1 && finger_last[msgs[i].id] == 0) {
                    cts_info("Point[%d] DOWN: x=%d, y=%d", msgs[i].id, touch_x, touch_y);
                }
                else if(finger_current[msgs[i].id] == 0 && finger_last[msgs[i].id] == 1) {
                    cts_info("Point[%d] UP: x=%d, y=%d", msgs[i].id, touch_x, touch_y);
                }
            }
        } while(0);
        /*BSP:add for debug by guoxin.zhu 20230223 end*/
        switch (msgs[i].event) {
        case CTS_DEVICE_TOUCH_EVENT_DOWN:
        case CTS_DEVICE_TOUCH_EVENT_MOVE:
        case CTS_DEVICE_TOUCH_EVENT_STAY:
            contact++;
            input_mt_slot(input_dev, msgs[i].id);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
            input_report_abs(input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, msgs[i].pressure);
            input_report_abs(input_dev, ABS_MT_PRESSURE, msgs[i].pressure);
            break;

        case CTS_DEVICE_TOUCH_EVENT_UP:
            /* input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false); */
            break;

        default:
            cts_warn("Process touch msg with unknwon event %u id %u",
                     msgs[i].event, msgs[i].id);
            break;
        }
#else /* CONFIG_CTS_SLOTPROTOCOL */
        /**
        * If the driver reports one of BTN_TOUCH or ABS_PRESSURE
        * in addition to the ABS_MT events, the last SYN_MT_REPORT event
        * may be omitted. Otherwise, the last SYN_REPORT will be dropped
        * by the input core, resulting in no zero-contact event
        * reaching userland.
        */
        switch (msgs[i].event) {
        case CTS_DEVICE_TOUCH_EVENT_DOWN:
        case CTS_DEVICE_TOUCH_EVENT_MOVE:
        case CTS_DEVICE_TOUCH_EVENT_STAY:
            contact++;
            input_report_abs(input_dev, ABS_MT_PRESSURE, msgs[i].pressure);
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, msgs[i].pressure);
            input_report_key(input_dev, BTN_TOUCH, 1);
            input_report_abs(input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
            input_mt_sync(input_dev);
            break;

        case CTS_DEVICE_TOUCH_EVENT_UP:
            break;
        default:
            cts_warn("Process touch msg with unknwon event %u id %u",
                     msgs[i].event, msgs[i].id);
            break;
        }
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    }

#ifdef CONFIG_CTS_SLOTPROTOCOL
    for (i = 0; i < CFG_CTS_MAX_TOUCH_NUM; i++) {
        if (finger_last[i] != 0 && finger_current[i] == 0) {
            input_mt_slot(input_dev, i);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
        }
        finger_last[i] = finger_current[i];
    }
    input_report_key(input_dev, BTN_TOUCH, contact > 0);
#else
    if (contact == 0) {
        input_report_key(input_dev, BTN_TOUCH, 0);
        input_mt_sync(input_dev);
    }
#endif
    input_sync(input_dev);
#ifdef CFG_CTS_FORCE_UP
    if (contact) {
        if (delayed_work_pending(&pdata->touch_event_timeout_work)) {
            mod_delayed_work(cts_data->workqueue,
                             &pdata->touch_event_timeout_work, msecs_to_jiffies(100));
        } else {
            queue_delayed_work(cts_data->workqueue,
                               &pdata->touch_event_timeout_work, msecs_to_jiffies(100));
        }
    } else {
        cancel_delayed_work_sync(&pdata->touch_event_timeout_work);
    }
#endif

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    if (contact) {
        if (delayed_work_pending(&cts_data->heart_work)) {
            mod_delayed_work(cts_data->heart_workqueue,
                             &cts_data->heart_work, msecs_to_jiffies(2000));
        } else {
            queue_delayed_work(cts_data->heart_workqueue,
                               &cts_data->heart_work, msecs_to_jiffies(2000));
        }
    }
#endif

    return 0;
}

int cts_plat_release_all_touch(struct cts_platform_data *pdata)
{
    struct input_dev *input_dev = pdata->ts_input_dev;

#if defined(CONFIG_CTS_SLOTPROTOCOL)
    int id;
#endif /* CONFIG_CTS_SLOTPROTOCOL */

    cts_info("Release all touch");

#ifdef CONFIG_CTS_SLOTPROTOCOL
    for (id = 0; id < CFG_CTS_MAX_TOUCH_NUM; id++) {
        input_mt_slot(input_dev, id);
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
    }
    input_report_key(input_dev, BTN_TOUCH, 0);
#else
    input_report_key(input_dev, BTN_TOUCH, 0);
    input_mt_sync(input_dev);
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    input_sync(input_dev);

    return 0;
}

#ifdef CONFIG_CTS_VIRTUALKEY
int cts_plat_init_vkey_device(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Init VKey");

    pdata->vkey_state = 0;

    for (i = 0; i < pdata->vkey_num; i++) {
        input_set_capability(pdata->ts_input_dev,
                             EV_KEY, pdata->vkey_keycodes[i]);
    }

    return 0;
}

void cts_plat_deinit_vkey_device(struct cts_platform_data *pdata)
{
    cts_info("De-init VKey");

    pdata->vkey_state = 0;
}

int cts_plat_process_vkey(struct cts_platform_data *pdata, u8 vkey_state)
{
    u8 event;
    int i;

    event = pdata->vkey_state ^ vkey_state;

    cts_dbg("Process vkey state=0x%02x, event=0x%02x", vkey_state, event);

    for (i = 0; i < pdata->vkey_num; i++) {
        input_report_key(pdata->ts_input_dev, pdata->vkey_keycodes[i],
                         vkey_state & BIT(i) ? 1 : 0);
    }

    pdata->vkey_state = vkey_state;

    return 0;
}

int cts_plat_release_all_vkey(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Release all vkeys");

    for (i = 0; i < pdata->vkey_num; i++) {
        if (pdata->vkey_state & BIT(i)) {
            input_report_key(pdata->ts_input_dev, pdata->vkey_keycodes[i], 0);
        }
    }

    pdata->vkey_state = 0;

    return 0;
}
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CFG_CTS_GESTURE
int cts_plat_enable_irq_wake(struct cts_platform_data *pdata)
{
    int ret;

    cts_info("Enable IRQ wake");

    if (pdata->irq > 0) {
        ret = enable_irq_wake(pdata->irq);
        if (ret < 0) {
            cts_err("Enable irq wake failed");
            return -EINVAL;
        }
        pdata->irq_wake_enabled = true;
        return 0;
    }

    cts_warn("Enable irq wake while irq invalid %d", pdata->irq);
    return -ENODEV;
}

int cts_plat_disable_irq_wake(struct cts_platform_data *pdata)
{
    int ret;

    cts_info("Disable IRQ wake");

    if (pdata->irq > 0) {
        ret = disable_irq_wake(pdata->irq);
        if (ret < 0) {
            cts_warn("Disable irq wake while already disabled");
            return -EINVAL;
        }
        pdata->irq_wake_enabled = false;
        return 0;
    }

    cts_warn("Disable irq wake while irq invalid %d", pdata->irq);
    return -ENODEV;
}

int cts_plat_init_gesture(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Init gesture");

    /* TODO: If system will issure enable/disable command, comment following line. */
    /* cts_enable_gesture_wakeup(pdata->cts_dev); */

    for (i = 0; i < pdata->gesture_num; i++) {
        input_set_capability(pdata->ts_input_dev, EV_KEY,
                             pdata->gesture_keymap[i][1]);
    }
    /*bsp:add gesture key by jiangzhu.yang 20230718 start*/
    input_set_capability(pdata->ts_input_dev, EV_KEY, KEY_F26);
    input_set_capability(pdata->ts_input_dev, EV_KEY, KEY_F20);
    /*bsp:add gesture key by jiangzhu.yang 20230718 end*/

    return 0;
}

void cts_plat_deinit_gesture(struct cts_platform_data *pdata)
{
    cts_info("De-init gesture");
}

int cts_plat_process_gesture_info(struct cts_platform_data *pdata,
                                  struct cts_device_gesture_info *gesture_info)
{
    /*BSP: modify for transsion core by guoxin.zhu 20230613 start*/
    struct tran_core_ops *ops = NULL;

    cts_info("Process gesture, id=0x%02x", gesture_info->gesture_id);

    if(get_tpd_ops(&chipone_controller, &ops)) {
        if (0 > ops->gesture_id_report(gesture_info->gesture_id, &chipone_controller)) {
            cts_err("[GESTURE]gesture touch data report fail");
            return EINVAL;
        }
        return 0;
    }

    return 0;
#if 0
    int i;

#if defined(CFG_CTS_GESTURE_REPORT_KEY)
    for (i = 0; i < CFG_CTS_NUM_GESTURE; i++) {
        if (gesture_info->gesture_id == pdata->gesture_keymap[i][0]) {
            cts_info("Report key[%u]", pdata->gesture_keymap[i][1]);
            input_report_key(pdata->ts_input_dev, pdata->gesture_keymap[i][1], 1);
            input_sync(pdata->ts_input_dev);

            input_report_key(pdata->ts_input_dev, pdata->gesture_keymap[i][1], 0);
            input_sync(pdata->ts_input_dev);

            return 0;
        }
    }
#endif /* CFG_CTS_GESTURE_REPORT_KEY */

    cts_warn("Process unrecognized gesture id=%u", gesture_info->gesture_id);

    return -EINVAL;
#endif
    /*BSP: modify for transsion core by guoxin.zhu 20230613 end*/

}

#endif /* CFG_CTS_GESTURE */
