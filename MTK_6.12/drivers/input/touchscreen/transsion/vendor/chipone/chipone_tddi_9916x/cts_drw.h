#ifndef CTS_DRW_H
#define CTS_DRW_H


#define CTS_DRW_PROTOCOL

#define DRW_CRC_EN_MASK     (0x04)
#define DRW_CRC_INVERT_MASK (0x02)
#define DRW_ADDR_OFFSET         1
#define DRW_DATA_LEN_OFFSET     4
#define DRW_HCRC_OFFSET         7
#define DRW_DUMMY_HEAD_OFFSET   9
#define DRW_DATA_OFFSET         13
#define DRW_DCRC_OFFSET(data_len)       (data_len + DRW_DATA_OFFSET)
#define DRW_DUMMY_TAIL_OFFSET(data_len) (DRW_DCRC_OFFSET(data_len) + 2)
#define DRW_HACK_OFFSET(data_len)       (DRW_DCRC_OFFSET(data_len) + 3)
#define DRW_AACK_OFFSET(data_len)       (DRW_DCRC_OFFSET(data_len) + 4)
/* drw protocol will increase 18 bytes based on the size of payload */
#define DRW_SPI_WRAPPER_SIZE        18

#define DRW_ACK_VALID       0x0a

/* CMD(1 byte) + addr(3 bytes) + data_len(3 bytes) +
 * hcrc(2 bytes) + h_dummy(4 bytes) + dcrc(2 bytes) +
 * t_dummy(1 byte) + hack(1 byte) + aack(1 byte) = 18 bytes
 */
#define DRW_PROTOCOL_BYTES          (18u)

int cts_spi_drw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 len);

int cts_spi_drw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 len);

int cts_i2c_drw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 len);

int cts_i2c_drw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 len);

int cts_i3c_drw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 len);

int cts_i3c_drw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 len);

#endif