/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 Transsion Inc.
 */

#ifndef __TRAN_LED_CORE_H__
#define __TRAN_LED_CORE_H__

#include "tran_led_class.h"

#define LEDLOG_ERROR_LEVEL	1
#define LEDLOG_INFO_LEVEL	2
#define LEDLOG_DEBUG_LEVEL	3

extern int tlc_get_debug_level(void);

#define tlc_err(fmt, args...)					\
do {								\
	if (tlc_get_debug_level() >= LEDLOG_ERROR_LEVEL) {	\
		pr_notice("[tlc]" fmt, ##args);				\
	}							\
} while (0)

#define tlc_info(fmt, args...)					\
do {								\
	if (tlc_get_debug_level() >= LEDLOG_INFO_LEVEL) {	\
		pr_notice_ratelimited("[tlc]" fmt, ##args);		\
	}							\
} while (0)

#define tlc_debug(fmt, args...)					\
do {								\
	if (tlc_get_debug_level() >= LEDLOG_DEBUG_LEVEL) {	\
		pr_notice("[tlc]" fmt, ##args);				\
	}							\
} while (0)


struct tran_led_core {
	struct platform_device *pdev;
	struct cdev cdev;
	dev_t dev_num;

	int tran_led_dev;
	struct class * tran_led_class;
	bool support_pdlc;

	enum ChipType chip_type;
	struct tran_led_device * leddev;
	struct tran_led_device * leddev1;
	struct mutex tran_led_mutex;

	int log_level;
};

#endif /* __TRAN_LED_CORE_H__ */
