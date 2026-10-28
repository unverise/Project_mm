
#ifndef CTS_PRW_H
#define CTS_PRW_H


#define CTS_PRW_PROTOCOL

#define PRW_ADDR_OFFSET         1
#define PRW_WR_DATA_OFFSET      4
#define PRW_RD_DATA_OFFSET      5
#define PRW_ADDR_SIZE           3



int cts_spi_prw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 len);

int cts_spi_prw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 len);

int cts_i2c_prw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 len);

int cts_i2c_prw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 len);

int cts_i3c_prw_read(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, void *data,
    const u32 len);

int cts_i3c_prw_write(struct cts_platform_data *pdata,
    const struct cts_device_hwdata *hwdata, u32 addr, const void *data,
    const u32 len);

#endif