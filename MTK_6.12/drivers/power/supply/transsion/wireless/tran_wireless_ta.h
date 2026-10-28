/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __TRAN_WIRELESS_TA_H
#define __TRAN_WIRELESS_TA_H

#include "tc_charger_class.h"
#include "tc_adapter_class.h"
#include "wireless_manager.h"
#include "wireless_class.h"

enum wls_check {
	MAXIC_WLS_NORMAL,
	MAXIC_WLS_FAST=2,
	MAXIC_WLS_SIT_SHIFT  = 4,
};

enum usb_type{
	ADAPTER_NONE,
	ADAPTER_SDP,
	ADAPTER_CDP,
	ADAPTER_DCP,
	ADAPTER_QC2=5,
	ADAPTER_QC3,
	ADAPTER_PD,
	ADAPTER_RFC_25W=25,
	ADAPTER_RFC_33W=33,
	ADAPTER_RFC_40W=40,
	ADAPTER_RFC_65W=65,
};

enum hall_pdp_num{
	HALL_PDP_0 = 0,
	HALL_PDP_1,
	HALL_PDP_2,
	HALL_PDP_MAX,
};

struct wireless_dt_args {
	bool support_cc;
	bool support_s65w;
	u32 vta_step;
	u32 ita_step;
	u32 ita_gap_per_vstep;
	bool pwr_lmt;
	bool support_meas_cap;
	bool support_status;
	u32 ita_gap_cv_mode;
	u32 hall_close_pdp_th[HALL_PDP_MAX];
	u32 hall_close_pdp[HALL_PDP_MAX];
};

struct wireless_ta {
	struct device *dev;
	struct charger_device *chg_dev;
	struct tadapter_device *apd;
	struct wireless_dt_args *desc;
};

#endif
