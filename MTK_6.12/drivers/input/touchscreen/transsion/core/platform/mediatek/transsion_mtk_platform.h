#ifndef __TRANSSON_MTK_PLATFORM_H__
#define __TRANSSON_MTK_PLATFORM_H__

#include "transsion_core.h"
//USD: add tne log by jiangzhu.yang start
#if TRAN_TNE_LOG_EN
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
#include <tk/tnek.h>
#endif
#endif
#endif
//USD: add tne log by jiangzhu.yang end
#endif

