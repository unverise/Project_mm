#ifndef __TRAN_WIRELESS_CLASS_H__
#define __TRAN_WIRELESS_CLASS_H__

#include <linux/list.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include "wireless_manager.h"
#include "tc_charger_class.h"

#define TX_VMAX_SHIFT 				3
#define TX_VMAX_MASK 				0x7
#define TX_IMAX_SHIFT 				12
#define TX_IMAX_MASK 				0x7
#define TX_MAGNETISM_SHIFT 			6
#define TX_MAGNETISM_MASK 			0x3
#define TX_BRIDGE_VOLTAGE_SHIFT 	3
#define TX_BRIDGE_VOLTAGE_MASK 		0x7
#define TX_TYPE_SHIFT 				11
#define TX_TYPE_MASK 				0x1
#define TX_MAGNETISM_FAN_SHIFT 		10
#define TX_MAGNETISM_FAN_MASK 		0x1

#define MAX_PKT_SIZE				22 /// ASK PPe2
#define MAX_MSG_SIZE				20
#define MAX_BC_SIZE					10 /// FSK PP8f

#define	CMD_EPP_NEGO_END	0x551A
#define	CMD_SITE_OCCUR		0xC31A
#define CMD_SITE_CANCEL		0xCC1A
#define CMD_EPP_OVER_LOAD	0xAD1A

enum {
	UNKNOWN,
	BPP,
	MPP_RESTRICT,
	MPP_FULL,
	MPP_CLOAK,
	MPP_FORCE,
	EPP,
};

enum private_ask_idx {
	CMD_ACK,
	CMD_TX_RESET,
	CMD_EPPEXTENDSYNC,
	CMD_GET_MAXPOWER,
	CMD_GET_ADAPTER_POWER,
	CMD_GET_ADAPTER_VMAX,
	CMD_GET_ADAPTER_IMAX,
	CMD_GET_PRODUCT_VERSION,
	CMD_ADAPTER_TYPE,
	CMD_AUTHENTICATION,
	CMD_BANK_SOC,
	CMD_MAX,
};

typedef enum {
	PP_NONE                   = 0x00,
	ENDPOWERXFERPACKET        = 0x02,
	PP18                      = 0x18,
	PP19                      = 0x19,
	PP28                      = 0x28,
	PP29                      = 0x29,
	PP38                      = 0x38,
	PP48                      = 0x48,
	PP58                      = 0x58,
	PP68                      = 0x68,
	PP78                      = 0x78,
} WPCHeaderType;

struct private_ask {
	enum private_ask_idx idx;
	u8 header;
	u8 cmd;
	u8 data[20];
	u8 header_rsp;
	u8 cmd_rsp;
	u8 length_rsp;
	bool wake_up_flag;
	u32 timeout;
};

struct tx_config {
	u8 protocol;
	u8 power;
};

#define ASK_DESC(_idx_i, _header_i, _cmd_i,_header_rsp_i,			\
		_cmd_rsp_i, _length_rsp_i, _wake_up_flag, _timeout)			\
{																	\
	.idx			= _idx_i,										\
	.header			= _header_i,									\
	.cmd			= _cmd_i,										\
	.header_rsp		= _header_rsp_i,								\
	.cmd_rsp		= _cmd_rsp_i,									\
	.length_rsp		= _length_rsp_i,								\
	.wake_up_flag	= _wake_up_flag,								\
	.timeout		= _timeout										\
}

struct power_bank {
	u8 soc;
	bool plug_adapter;
};

enum wireless_state {
	WIRELESS_WIRED_CHANGE = 0,
	WIRELESS_PG_CHANGE,
	WIRELESS_LDO_ON,
	WIRELESS_POWE_BANK,
	WIRELESS_RX_EPP_READY,
	WIRELESS_TX_AC_VALID,
	WIRELESS_RX_HW_ERR,
	WIRELESS_TX_DET_RX,
	WIRELESS_TX_RMV_RX,
	WIRELESS_TX_MODE_CLOSE,
	WIRELESS_TX_MODE_RESTART,
	WIRELESS_RX_SR_BR_H2F,
	WIRELESS_TX_INIT_DONE,
	WIRELESS_PROBE_END,
	WIRELESS_RX_MPP,
	WIRELESS_MAX,
};

struct wls_hw_info {
	/* adapter member */
	int ta_type;
	int ta_vmax;
	int ta_imax;
	int ta_pmax;

	/* tx member */
	int tx_imax;
	int tx_vmax;
	int tx_pmax;
	u8 tx_type;
	u8 tx_id;
	bool tx_magnetism;
	bool tx_magnetism_fan;
	bool power_bank;
	u8 tx_temp;
	u32 tx_bridge_vol_max;
};

struct wireless_charger;

struct wls_ops {
	int (*get_wireless_pg)(struct wireless_charger *, bool *pg_status);
	int (*get_wireless_vbus)(struct wireless_charger *, int *vbus);
	int (*get_wireless_ibus)(struct wireless_charger *, int *ibatus);
	int (*set_wireless_plug_in)(struct wireless_charger *);
	int (*set_wireless_plug_out)(struct wireless_charger *);
	int (*get_wireless_site)(struct wireless_charger *, bool *en);
	int (*get_wireless_hw_info)(struct wireless_charger *, struct wls_hw_info *info);
	int (*get_wireless_authenticate)(struct wireless_charger *, bool *en);
	int (*set_wireless_voltage)(struct wireless_charger *, int mv, bool shutdown);
	int (*dump_wireless_status)(struct wireless_charger *);
	int (*get_wireless_online)(struct wireless_charger *, bool *online);
	int (*set_wireless_tx_mode)(struct wireless_charger *, bool en, struct tx_config *txc);
	int (*get_wireless_epp_status)(struct wireless_charger *, bool *epp);
	int (*get_wireless_power)(struct wireless_charger *, int *value);
	int (*private_hash_verify)(struct wireless_charger *);
	int (*private_capacity_auth)(struct wireless_charger *);
	int (*wireless_negotiate_power)(struct wireless_charger *);
	int (*wireless_product_info)(struct wireless_charger *);
	int (*set_wireless_tx_reset)(struct wireless_charger *);
	int (*set_wireless_soft_reset)(struct wireless_charger *);
	int (*wired_path_setup)(struct wireless_charger *, bool enable);
	int (*get_wired_state)(struct wireless_charger *, bool *state);
	int (*set_wireless_volt_sync)(struct wireless_charger *, u32 mv);
	int (*set_wireless_sleep)(struct wireless_charger *, bool en);
	int (*set_wireless_lpm_mode)(struct wireless_charger *, bool en);
	int (*set_wireless_fw_update)(struct wireless_charger *, const struct firmware *fw, bool force);
	int (*get_wireless_fw_version)(struct wireless_charger *, u32 *ver);
	int (*set_ovp_ctrl)(struct wireless_charger *, bool en);
	int (*get_wireless_adc)(struct wireless_charger *, enum adc_channel chan);
	int (*get_bridge_mode)(struct wireless_charger *);
	int (*set_bridge_logic)(struct wireless_charger *);
	short (*get_tx_ce_value)(struct wireless_charger *);
	int (*tx_bridge_voltage)(struct wireless_charger *);
	int (*set_drop_voltage)(struct wireless_charger *);
	int (*wireless_power_bank)(struct wireless_charger *, struct power_bank *pb);
	int (*wireless_pb_product_info)(struct wireless_charger *);
	u8 (*get_wls_protocol)(struct wireless_charger *);
	int (*wirte_reg_data)(struct wireless_charger *, int reg, u8 data);
	int (*read_reg_data)(struct wireless_charger *, int reg, u8 *data);
	int(*tx_mode_prepare)(struct wireless_charger *, bool en);
	int (*magnetism_fan_check)(struct wireless_charger *, struct wls_hw_info *info);
};

struct wireless_charger {
	char *name;
	struct list_head list;
	struct charger_device *wm_chg;
	struct wls_ops *ops;
	int irq_gpio;
	int pg_gpio;
	int ldo_gpio;
	void *private_d;
	struct wls_hw_info *hw_info;
};

typedef union {
	u16 value;
	u8 ptr[2];
} Alig16;

typedef union {
	u32 value;
	u8  ptr[4];
} Alig32;

typedef struct {
	u8 header;
	union {
		u8 msg[MAX_MSG_SIZE-1];
		struct {
			u8 cmd;
			u8 data[MAX_MSG_SIZE-2];
		};
	};
} PktType;

extern void init_tran_ask_desc(struct private_ask *pa);
extern void wireless_ic_set_state(struct wireless_charger *wl_chg,
	enum wireless_state state);
extern int __wireless_charger_tx_bridge_voltage(struct wireless_manager *m);
extern int __wireless_charger_set_drop_voltage(struct wireless_manager *m);
extern int __wireless_charger_get_tx_ce_value(struct wireless_manager *m);
extern int __wireless_charger_bridge_mode(struct wireless_manager *m);
extern int __wireless_charger_set_bridge_logic(struct wireless_manager *m);
extern int __wireless_charger_set_ovp_ctrl(struct wireless_manager *m, int en);
extern int __wireless_charger_enter_sleep(struct wireless_manager *m, bool en);
extern int __wireless_charger_enter_lpm_mode(struct wireless_manager *m, bool en);
extern int __wireless_charger_get_fw_version(struct wireless_manager *m, u32 *ver);
extern int __wireless_charger_get_wired_state(struct wireless_manager *m, bool *state);
extern int __wireless_charger_get_pg(struct wireless_manager *m, bool *pg_status);
extern int __wireless_charger_get_vbus(struct wireless_manager *m, int *vbus);
extern int __wireless_charger_get_ibus(struct wireless_manager *m, int *ibus);
extern int __wireless_charger_set_plug_in(struct wireless_manager *m);
extern int __wireless_charger_set_plug_out(struct wireless_manager *m);
extern int __wireless_charger_get_pmax(struct wireless_manager *m, int *value);
extern int __wireless_charger_get_capacity(struct wireless_manager *m,
		struct wls_hw_info *hw_info);
extern int __wireless_charger_power_bank_info(struct wireless_manager *m,
		struct power_bank *pb);
extern int __wireless_charger_authenticate(struct wireless_manager *m, bool *en);
extern int __wireless_charger_set_voltage(struct wireless_manager *m, int mv, bool shutdown);
extern int __wireless_charger_set_volt_sync(struct wireless_manager *m, u32 mv);
extern int __wireless_charger_dump_status(struct wireless_manager *m);
extern int __wireless_charger_get_online(struct wireless_manager *m, bool *online);
extern int __wireless_charger_get_epp_status(struct wireless_manager *m, bool *epp);
extern int __set_wired_path_setup(struct wireless_manager *m, bool en);
extern int __wireless_charger_set_tx_mode(struct wireless_manager *m, bool en, struct tx_config *txc);
extern int __wireless_charger_get_power(struct wireless_manager *m, int *value);
extern int __wireless_charger_private_hash_verify(struct wireless_manager *m);
extern int __wireless_charger_private_capacity_auth(struct wireless_manager *m);
extern int __wireless_charger_negotiate_power(struct wireless_manager *m);
extern int __wireless_charger_product_info(struct wireless_manager *m);
extern int __wireless_charger_set_tx_reset(struct wireless_manager *m);
extern int __wireless_charger_set_soft_reset(struct wireless_manager *m);
extern int __wireless_charger_get_adc(struct wireless_manager *m, enum adc_channel chan);
extern int __wireless_charger_get_wls_protocol(struct wireless_manager *m);
extern void unregister_wireless_charger_device(struct wireless_charger *wcd);
extern int register_wireless_charger_device(struct wireless_charger *wcd, void *data);
extern inline u32 SizeofPkt(u8 hdr);
extern inline void decode_tx_magnetism(int product, struct wls_hw_info *hw_info);
extern inline void decode_tx_bridge_voltage(int product, struct wls_hw_info *hw_info);
extern inline void decode_tx_imax(int product, struct wls_hw_info *hw_info);
extern inline void decode_tx_vmax(int product, struct wls_hw_info *hw_info);
extern inline void decode_tx_type(int product, struct wls_hw_info *hw_info);
extern inline void decode_tx_magnetism_fan(int product, struct wls_hw_info *hw_info);
extern inline int get_responce_data(struct private_ask *private, u8 *data);
extern int wireless_wirte_data(struct wireless_manager *m, int reg, u8 data);
extern int wireless_read_data(struct wireless_manager *m, int reg, u8 *data);
extern int set_tx_mode_prepare(struct wireless_manager *m, bool en);
extern inline char *wls_protocol_mode_name(int num);
extern int __wireless_charger_pb_product_info(struct wireless_manager *m);
extern int __wirless_charger_magnetism_fan_check(struct wireless_manager *m, struct wls_hw_info *hw_info);
extern u8 __wireless_get_wls_protocol(struct wireless_manager *info);
#endif
