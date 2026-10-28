/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2014 Jiri Pirko <jiri@resnulli.us>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __LINUX_TC_WATER_DETECT_H
#define __LINUX_TC_WATER_DETECT_H

enum {
	SCHEME_SUBPMIC = 0,
	SCHEME_GPIO,
	SCHEME_GPIO_AND_SBU_1_2,
	SCHEME_USB_SWITCH,
	SCHEME_USB_ID_AND_GPIO,
	SCHEME_USB_ID_AND_GPIO_AND_SBU_1_2,
};

enum {
	WD_USER_CMD_UNKNOWN = 0,
	WD_USER_FORCE_CHARGING = 5,
	WD_USER_DISCHARGING = 10,
};

enum {
	WD_TRIGGER = 0,
	WD_USER_SELECT_FORCE_CHARGING,
	WD_USER_SELECT_DISCHARGING,
};


#endif

