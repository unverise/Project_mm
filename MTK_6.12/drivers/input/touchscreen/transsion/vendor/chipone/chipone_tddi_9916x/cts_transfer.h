#ifndef CTS_TRANSFER_H
#define CTS_TRANSFER_H
#include <linux/version.h>
#define     READ_BIT        BIT(0)
#define     REG_ADDR_SIZE   3
#define     CTS_RETRY               (3)
#define     CTS_M_DELAY             (5)

#define     CTS_WORD_SIZE    4

#define     CTS_SPI_PROG_ADDR_DEFAULT   0x60
#define     CTS_I2C_PROG_ADDR_DEFAULT   0x30
#define     CTS_SPI_NORM_ADDR_DEFAULT   0xF0
#define     CTS_I2C_NORM_ADDR_DEFAULT   0x48

#define     CTS_MAX_READ_LEN                (1024 * 20)
#define     CTS_MAX_WRITE_LEN               (1024 * 200)
#define     CTS_PROTOCOL_SIZE   (32u)       // protocol wrapper size should be larger than any protocol
#define     CTS_MAX_I2C_PAYLOAD_SIZE    (32u)
#define     CTS_MAX_I2C_BUF_SIZE        (CTS_MAX_I2C_PAYLOAD_SIZE + CTS_PROTOCOL_SIZE)
#define     CTS_MAX_SPI_PAYLOAD_SIZE    (8192u)
#define     CTS_MAX_SPI_BUF_SIZE       (CTS_MAX_SPI_PAYLOAD_SIZE + CTS_PROTOCOL_SIZE)
#define     CFG_CTS_INT_DATA_MAX_SIZE   (8192u)
#define     CFG_MAX_SRAM_XCHG_SIZE      512

enum cts_device_type {
    CTS_TP,
    CTS_DDI,
    CTS_DEV_TYPE_MAX
};

static inline void _put_unaligned_be24(const u32 val, u8 *p)
{
	*p++ = (val >> 16) & 0xff;
	*p++ = (val >> 8) & 0xff;
	*p++ = val & 0xff;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
static inline u32 __get_unaligned_be24(const u8 *p)
{
	return p[0] << 16 | p[1] << 8 | p[2];
}
#endif
int get_unaligned_bytes(u32 addr, u32 len);

/* cts_read() - read data from touch IC. if DRW is supported, use DRW as
 *              preferred in both normal mode and program mode. Otherwise,
 *              use tcs for sram mode, and PRW for program mode.
 * @cts_dev: cts_dev has bus_type and protocol type info
 * @addr:    address to read from
 * @data:    data read back
 * @force_drw: force to use drw even in normal mode
 * @return:  return 0 on success
 */
int cts_read(const struct cts_device *cts_dev, u32 addr, u8 *data, u32 len,
    bool force_drw);

/* cts_write() - read data from touch IC. if DRW is supported, use tcs
 *              for sram mode, and PRW/DRW for non-sram mode.
 * @cts_dev: cts_dev has bus_type and protocol type info
 * @addr:    address to read from
 * @data:    data read back
 * @force_drw: force to use drw even in normal mode
 * @return:  return 0 on success
 */
int cts_write(const struct cts_device *cts_dev, u32 addr,
    const u8 *data, u32 len, enum cts_device_type device_type,
    bool force_drw);

int cts_read_sram(const struct cts_device *cts_dev, u32 addr, u8 *data, u32 len,
    bool force_drw);
int cts_write_sram(const struct cts_device *cts_dev, u32 addr,
    const u8 *data, u32 len, bool force_drw);
int cts_read_hw_reg(const struct cts_device *cts_dev,
    u32 addr, u8 *data, u32 len, bool force_drw);
int cts_read_try_protocols(const struct cts_device *cts_dev,
    u32 addr, u8 *data, u32 len);
int cts_write_hw_reg(const struct cts_device *cts_dev, u32 addr,
    const u8 *data, u32 len, bool force_drw);
int cts_write_ddi_reg(const struct cts_device *cts_dev, u32 addr,
    const u8 *data, u32 len, bool force_drw);
int cts_transfer(const struct cts_device *cts_dev, u32 addr,
    const u8 *data, u32 len, u8 * rdata, u32 rlen);
#endif
