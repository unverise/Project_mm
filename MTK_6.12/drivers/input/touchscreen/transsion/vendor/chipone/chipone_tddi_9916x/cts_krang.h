#ifndef CTS_KRANG_H
#define CTS_KRANG_H

#define CTS_KRANG

/* DMA0 is for firmware, DMA1 is for driver */
#define CTS_KR_DMA1_LEN     0x0010
#define CTS_KR_DMA1_DST     0x0014
#define CTS_KR_DMA1_SRC     0x0018
#define CTS_KR_DMA1_CTL     0x001c

#define CTS_KR_M_TO_S1      0x0080
#define CTS_KR_M_TO_S2      0x0040
#define CTS_KR_S1_TO_M      0x0300
#define CTS_KR_S2_TO_M      0x0500
#define CTS_KR_RCP_TRIG     0
#define CTS_KR_TABW_TRIG     4
#define CTS_KR_RCP_DONE     0
#define CTS_KR_TABW_DONE     4

#define CTS_KR_TABW_TO_M     31
#define CTS_KR_TABW_TO_S1    30
#define CTS_KR_TABW_TO_S2    29

#define CTS_KR_RCP_TO_M     24
#define CTS_KR_RCP_TO_S1    23
#define CTS_KR_RCP_TO_S2    22
#define CTS_KR_RCP_FROM_S1    25
#define CTS_KR_RCP_FROM_S2    26
#define CTS_TABW_SINGLE_BYTE_CTL  24

#define CTS_KR_MSTER    BIT(0)
#define CTS_KR_SLAVE1   BIT(1)
#define CTS_KR_SLAVE2   BIT(2)

#define CTS_KR_BIT_MAP_MASTER    0
#define CTS_KR_BIT_MAP_SLAVE1    1
#define CTS_KR_BIT_MAP_SLAVE2    2

#define CTS_KR_MAX_LEN  (32 * 1024)

static __always_inline void _assign_bit(long nr, volatile unsigned long *addr,
				       bool value)
{
	if (value)
		set_bit(nr, addr);
	else
		clear_bit(nr, addr);
}

int cts_read_slave(struct cts_device *cts_dev, u32 addr,
    u8 *data, u32 len, unsigned long client_id);

int cts_write_slave(struct cts_device *cts_dev, u32 addr,
    u8 *data, u32 len, unsigned long client_id);

#endif