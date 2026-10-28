/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 Transsion Inc.
 *
 */

#ifndef __TRAN_THERMAL_CFG_H__
#define __TRAN_THERMAL_CFG_H__

struct tran_thermal_cfg_struct {
    int maxFrameRate;
    int hFrameRate[2];
    int lFrameRate[2];
};

struct tran_thermal_cfg_struct g_tran_thermal_cfg = {
    300,
    {40000,280},
    {46000,260}
};

#endif