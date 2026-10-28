/*
 * fm1210_ow_gpio.h - for FM1210 one wire device module.
 */
#ifndef __FM1210_OW_GPIO_H__
#define __FM1210_OW_GPIO_H__
#include <tc_common_class.h>
#define ON_SUCCESS (0x0000u) /*!< 0x0000 - Generic success status. */
#define OW_DEBUG 0

#if OW_DEBUG == 1
#define ds_dbg printk
#else
#define ds_dbg pr_debug
#endif

#define HIGH    1
#define LOW     0

#define DEVICE_NAME "fm1210"
#define OWSE_DQ     193 //EXYNOS4_GPA0(7)             /* J38_13 SWI*/
#define OWSE_DBG    22 //EXYNOS4212_GPM4(7)          /* XispSPIMISO,J38_14 DEBUG*/
#define OWSE_LPC    185 //EXYNOS4212_GPM4(4)          /* XispSPICLK,J38_15 PMOS*/

#if OW_DEBUG == 1
#define ow_dbg_low()    gpio_set_value(OWSE_DBG, 0)
#define ow_dbg_high()   gpio_set_value(OWSE_DBG, 1)
void ow_dbg_switch_gpio(u8 cnt);
#else
#define ow_dbg_low()
#define ow_dbg_high()
#define ow_dbg_switch_gpio(x)
#endif

/* sys command */
#define MAGIC_NUMBER    'w'
#define SWI_IOC_RD_MODE _IOR(MAGIC_NUMBER, 1, __u8)
#define SWI_IOC_WR_MODE _IOW(MAGIC_NUMBER, 1, __u8)

/* enable HM code */
#define SWI_HM

/*ow time data */
struct ow_tm_data {
	uint16_t	tm_trstL;               //RST
	uint16_t	tm_trecRST;             //RST
	uint16_t	tm_trec;                //
	uint16_t	tm_tw1L;                //
	uint16_t	tm_tw0L;
	uint16_t	tm_trL;
	uint16_t	tm_trgt;
	uint16_t	tm_tpdL;
	uint16_t	tm_tbytegt;
	uint16_t	tm_tcmdgt;
	uint16_t	tm_tsamp;
};

struct file_status {
	//b0-b7
	u8	read_access : 2;    //
	u8	write_access : 2;   //
	u8	enc_type : 2;       //
	u8	key_type : 2;       //
};

#define ACCESS_ALLOWED	0x01
#define ENC_CLEARTEXT	0x01


struct fm1210 {
	/* Context related to Linux character devices. */
	struct device *dev;
	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *ctx_psy;
	struct work_struct test_work;
	struct notifier_block bat_authent_notifier_block;
	char *regID;
	int bat_authent_cloud_feedback;

	struct mutex ow_access;
	unsigned int swi_gpio;
	unsigned int dbg_gpio;
	unsigned int board_id;
	u32 loop_cnt;
	u32 loop_error_cnt;

	int base_year;
	char mac[128];
	char active[128];
	struct delayed_work update_work;
	struct tran_device *tc_ctx_dev;
	struct tran_properties tc_charger_props;
};

/* ow driver ops */
struct ow_ops {
	void	(*wbit)(struct fm1210 *chip_ctx, u8 bi);
	void	(*wbyte)(struct fm1210 *chip_ctx, u8 by);
	u8	(*rbit)(struct fm1210 *chip_ctx);
	u8	(*rbyte)(struct fm1210 *chip_ctx);
};

/* ow driver ops */
struct ow_ops_hm {
	void	(*wbit)(struct fm1210 *chip_ctx, u8 bi);
	void	(*wbyte)(struct fm1210 *chip_ctx, u16 by);
	u8	(*rbit)(struct fm1210 *chip_ctx);
	u16	(*rbyte)(struct fm1210 *chip_ctx);
};

/* PAGE SIZE */
#define PAGE_BLOCK_SIZE 16
#define ROMID_ADDR      0

#define PAGE1_ADDR      1
#define PAGE1_BLOCK     2
#define PAGE1_SIZE      PAGE1_BLOCK*PAGE_BLOCK_SIZE
#define PAGE1_OFS       (PAGE1_SIZE << 1)
#define PAGE1_RET       (PAGE1_OFS + 1)

#define PAGE2_ADDR      (PAGE1_ADDR + PAGE1_BLOCK) 
#define PAGE2_BLOCK     3
#define PAGE2_SIZE      PAGE2_BLOCK*PAGE_BLOCK_SIZE
#define PAGE2_OFS       (PAGE2_SIZE << 1)
#define PAGE2_RET       (PAGE2_OFS + 1)

#define PAGE3_ADDR      (PAGE2_ADDR + PAGE2_BLOCK) 
#define PAGE3_BLOCK     2
#define PAGE3_SIZE      PAGE3_BLOCK*PAGE_BLOCK_SIZE
#define PAGE3_OFS       (PAGE3_SIZE << 1)
#define PAGE3_RET       (PAGE3_OFS + 1)

#define PAGE4_ADDR      (PAGE3_ADDR + PAGE3_BLOCK) 
#define PAGE4_BLOCK     3
#define PAGE4_SIZE      PAGE4_BLOCK*PAGE_BLOCK_SIZE
#define PAGE4_OFS       (PAGE4_SIZE << 1)
#define PAGE4_RET       (PAGE4_OFS + 1)

#define PAGE5_ADDR      (PAGE4_ADDR + PAGE4_BLOCK) 
#define PAGE5_BLOCK     1
#define PAGE5_SIZE      PAGE5_BLOCK*PAGE_BLOCK_SIZE
#define PAGE5_OFS       (PAGE5_SIZE << 1)
#define PAGE5_RET       (PAGE5_OFS + 1)

#define PAGE6_ADDR      (PAGE5_ADDR + PAGE5_BLOCK) 
#define PAGE6_BLOCK     1
#define PAGE6_SIZE      PAGE6_BLOCK*PAGE_BLOCK_SIZE
#define PAGE6_OFS       (PAGE6_SIZE << 1)
#define PAGE6_RET       (PAGE6_OFS + 1)

#define PAGE7_ADDR      (PAGE6_ADDR + PAGE6_BLOCK) 
#define PAGE7_BLOCK     1
#define PAGE7_SIZE      PAGE7_BLOCK*PAGE_BLOCK_SIZE
#define PAGE7_OFS       (PAGE7_SIZE << 1)
#define PAGE7_RET       (PAGE7_OFS + 1)

#define PAGE8_ADDR      (PAGE7_ADDR + PAGE7_BLOCK) 
#define PAGE8_BLOCK     1
#define PAGE8_SIZE      PAGE8_BLOCK*PAGE_BLOCK_SIZE
#define PAGE8_OFS       (PAGE8_SIZE << 1)
#define PAGE8_RET       (PAGE8_OFS + 1)

#define PAGE_ALL_SIZE   224
#define PAGE_ALL_OFS    (PAGE_ALL_SIZE << 1)
#define PAGE_ALL_RET    (PAGE_ALL_OFS + 1)
#define STATUS_SIZE		2
#define STATUS_SIZE_OFS (STATUS_SIZE << 1)
#define STATUS_SIZE_RET (PAGE2_OFS << 1 + 1)

#define ECC_SIG_SIZE    64
#define ECC_SIG_OFS     (ECC_SIG_SIZE << 1)
#define ECC_SIG_RET     (ECC_SIG_OFS + 1)
#define DEV_CERT_SIZE   144
#define DEV_CERT_OFS    (DEV_CERT_SIZE << 1)
#define READ_CERT_RET   (DEV_CERT_OFS + 1)

#define READ_UID_SIZE   8
#define READ_UID_OFS    (READ_UID_SIZE << 1)
#define READ_UID_RET    (READ_UID_OFS + 1)

#define PAGE_CMD_LENGTH     7
#define PAGE_STATE_LENGTH   4

#define RESULT_SUCCESS  0
#define RESULT_FAIL     1
#define RESULT_NOKNOWN  0xFA

#define RESULT_ERROR -1
/* INPUT DATA MAX SIZE */
#define ECC_HASH_MAX_SIZE 65

/* LOCK PAGE SIZE */
#define LOCK_PAGE_RET     10

/* COMM PROTOCOL */
#define SW_SUCCESS          0x9000
#define SW_BYTE_NUM         2
#define CMD_BYTE_LENGTH     1
#define CMD_SEND_LENGTH     3
#define RECV_PACK_LENGTH    3
#define RECV_DATA_LENGTH    2
#define RECV_DATA_OFFSET    1

#define MAX_RETRY_COUNT     3 //todo: retry 3 times final

/* Vendor info */
//#define SE_VENDOR_ID 

/* para max size */

#define PARA_MAX_RET   5

#define PARA_LENGTH     8
#define PARA_OFS        (PARA_LENGTH << 1)
#define PARA_RET        (PARA_OFS + 1)


/* time max size */

#define TIME_MAX_SIZE   7
#define TIME_LENGTH     22
#define TIME_OFS        (TIME_LENGTH << 1)
#define TIME_RET        (TIME_OFS + 1)

#define EE_PUMP_ACTIVE	0x5A
#define EE_PUMP_CLOSE	0

#define LOOP_PARA_SIZE  9
#define TOGLE_PARA_SIZE 9

/* USER API COMMAND TIME DELAY */
#define CMD_GET_UID_DELAY       1
#define CMD_READ_CERT_DELAY     3
#define CMD_ECC_SIGN_DELAY      100
#define CMD_WRITE_BLOCK_DELAY   10
#define CMD_READ_BLOCK_DELAY    1
#define CMD_LOCK_BLOCK_DELAY    10
#define CMD_DEVICE_SELECT_DELAY 1
#define CMD_EE_PUMP_DELAY       1

#define CMD_GAP_DELAY           1

#define CMD_OW_ADR              0x04

static u8   ow_transceive(struct fm1210 *chip_ctx, u8 *write_buf, u16 write_len, u8 *read_buf, u16 *read_len, u16 delay_ms);
static u8   ow_transceive_hm(struct fm1210 *chip_ctx, u8 *write_buf, u16 write_len, u8 *read_buf, u16 *read_len, u16 delay_ms);

#endif
