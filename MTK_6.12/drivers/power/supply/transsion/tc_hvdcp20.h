/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 Transsion Inc.
 */

#ifndef __TC_HVDCP20_H
#define __TC_HVDCP20_H

#include "tc_algorithm_class.h"

/* hvdcp20.0 */
#define TA_ICHG_LEAVE_THRESHOLD 1000000 /* uA */
#define TA_START_BATTERY_SOC	0
#define TA_STOP_BATTERY_SOC	90
#define RECOVER_9V_GAP    500000

#define DISABLE_VBAT_THRESHOLD  4350000
#define DEFAULT_ICHG    2000000

#define ECABLEOUT	1	/* cable out */
#define EHAL		2	/* hal operation error */

#define HVDCP20_ERROR_LEVEL     1
#define HVDCP20_INFO_LEVEL	     2
#define HVDCP20_DEBUG_LEVEL     3

extern int hvdcp20_get_debug_level(void);

#define hvdcp20_err(fmt, args...)					\
do {								\
	if (hvdcp20_get_debug_level() >= HVDCP20_ERROR_LEVEL) {	\
		pr_notice("[ALG_HVDCP20]" fmt, ##args);				\
	}							\
} while (0)

#define hvdcp20_info(fmt, args...)					\
do {								\
	if (hvdcp20_get_debug_level() >= HVDCP20_INFO_LEVEL) { \
		pr_notice("[ALG_HVDCP20]" fmt, ##args);			\
	}							\
} while (0)

#define hvdcp20_dbg(fmt, args...)					\
do {								\
	if (hvdcp20_get_debug_level() >= HVDCP20_DEBUG_LEVEL) {	\
		pr_notice("[ALG_HVDCP20]" fmt, ##args);				\
	}							\
} while (0)

struct tc_hvdcp20 {
	struct platform_device *pdev;
	struct tchg_alg_device *alg;

	struct mutex access_lock;
	struct wakeup_source *suspend_lock;
	struct mutex cable_out_lock;
	struct mutex data_lock;
	bool is_cable_out_occur; /* Plug out happened while detect HVDCP20 */
	struct power_supply *bat_psy;

	int vbat_threshold; /* For checking Ready */

	int ta_vchr_target;
	bool is_fast_chr;

	/* hvdcp20.0 dtsi */
	int ta_ichg_level_threshold;	/* ma */
	int ta_stop_battery_soc;
	int recover_9v_gap;

	enum tchg_alg_state state;
	u32 *chr_volt;
	u32 *chr_volt_mivr;
	u32 *chr_volt_input_cur;
	u32 *chr_volt_chg_cur;
};

extern int hvdcp20_hal_init_hardware(struct tchg_alg_device *alg);
extern int hvdcp20_hal_get_uisoc(struct tchg_alg_device *alg);
extern int hvdcp20_hal_get_charger_type(struct tchg_alg_device *alg);
extern int hvdcp20_hal_set_mivr(struct tchg_alg_device *alg, bool enable, int uV);
extern int hvdcp20_hal_get_charger_cnt(struct tchg_alg_device *alg);
extern int hvdcp20_hal_get_vbus(struct tchg_alg_device *alg);
extern int hvdcp20_hal_get_vbat(struct tchg_alg_device *alg);
extern int hvdcp20_hal_get_ibat(struct tchg_alg_device *alg);
extern int hvdcp20_hal_get_mivr_state(struct tchg_alg_device *alg,enum tchg_idx chgidx, bool *in_loop);
extern int hvdcp20_hal_get_charger_current_limit(struct tchg_alg_device *alg);
extern int hvdcp20_hal_enable_vbus_ovp(struct tchg_alg_device *alg, bool enable);
extern int hvdcp20_hal_get_log_level(struct tchg_alg_device *alg);
extern int ta_dev_set_usb_ctrl_hvdcp20(struct tchg_alg_device *alg, int val);
extern int hvdcp20_hal_get_input_current(struct tchg_alg_device *alg,
	enum tchg_idx chgidx, u32 *uA);
extern int hvdcp20_hal_set_input_current(struct tchg_alg_device *alg, bool enable, u32 uA);

#endif /* __TC_HVDCP20_H */
