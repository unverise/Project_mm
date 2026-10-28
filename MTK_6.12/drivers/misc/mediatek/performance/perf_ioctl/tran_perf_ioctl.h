/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef TRAN_PERF_IOCTL_H
#define TRAN_PERF_IOCTL_H
#include <linux/ioctl.h>



// SPD: add game hint V3.0 by xinxing.wang at 20241120 start
#if defined(CONFIG_TRAN_GAME_PERF_DATA)
struct _TRAN_PERF_DATE_PACKAGE {
    __s64 t_cpu_01;
    __s64 t_cpu_02;
    __s64 t_gpu_01;
    __s64 t_gpu_02;
};
#endif
// SPD: add game hint V3.0 by xinxing.wang at 20241120 end

// SPD: add game hint V3.0 by xinxing.wang at 20241120 start
#if defined(CONFIG_TRAN_GAME_PERF_DATA)
#define FPSGO_GET_PERF_DATA                  _IOW('g', 1, struct _TRAN_PERF_DATE_PACKAGE)
#endif
// SPD: add game hint V3.0 by xinxing.wang at 20241120 end
#endif

