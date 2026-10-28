#include <linux/power_supply.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>

#include "transsion_core.h"
#include "transsion_node.h"
#include "transsion_gesture.h"

#define SET_TP_RESET_HIGH    1
#define SET_TP_RESET_LOW    2

// USD: add to Touch Gesture by ke.li 20251128 start
extern void __weak tran_register_tp_func(void *func, int type);
// USD: add to Touch Gesture by ke.li 20251128 end

static u32 set_tp_reset(struct tran_ts_core *ts_core, void *pdata)
{
    struct tran_ts_controller *pController = ts_core->controller[0];
    int low_high;

    if(!pController || pdata == NULL)
        return 0;

    low_high = *(int *)pdata;

    if(low_high == SET_TP_RESET_HIGH) {
        pController->ext_ops->tran_tp_reset(1);
        return 1;
    } else if(low_high == SET_TP_RESET_LOW) {
        pController->ext_ops->tran_tp_reset(0);
        return 1;
    }

    return 0;
}

static u32 set_tp_irq(struct tran_ts_core *ts_core, void *pdata)
{
	struct tran_ts_controller *pController = g_tran_ts.controller[0];
	uint8_t enable = 0;

	if(pController == NULL) {
		return 0;
	}

	if (pdata == NULL) {
		TRAN_INFO("Error: pdata is NULL enable use default value 0\n");
	} else {
		enable = *(uint8_t *)pdata;
	}

	if(pController->ext_ops->irq_enable_en != NULL) {
		pController->ext_ops->irq_enable_en(enable);
		return 1;
	}else{
		return 0;
	}

}

static u32 call_tp_fwload(struct tran_ts_core *ts_core, void *pdata)
{
	struct tran_ts_controller *pController = g_tran_ts.controller[0];

	if(pController == NULL) {
		return 0;
	}

	if(pController->ext_ops->call_tp_fwload != NULL) {
		pController->ext_ops->call_tp_fwload();
		return 1;
	}else{
		return 0;
	}

}

static u32 set_tp_tddi_resume(struct tran_ts_core *ts_core, void *pdata)
{
    struct tran_ts_controller *pController = ts_core->controller[0];

    if(pController && pController->ext_ops->resume_by_ddi != NULL) {
        pController->ext_ops->resume_by_ddi();
        return 1;
    }

    return 0;
}

static u32 get_tp_mode(struct tran_ts_core *ts_core, void *pdata)
{
    int ps_state,gesture_state;
    u32 func_mode = FUNC_MODE_NONE;
    struct tran_ts_controller *pController = g_tran_ts.controller[0];

//    ps_state = g_tran_ts.tpd_ops->ps_state_get(NULL);
    ps_state = TRAN_PS_OFF;
    gesture_state= g_tran_ts.tpd_ops->gesture_state_get(pController);

    /* get function mode */
    if(ps_state == TRAN_PS_ON) {
        func_mode = FUNC_MODE_PS;
    } else if(ps_state == TRAN_PS_OFF && gesture_state != 0) {
        func_mode = FUNC_MODE_GESTURE; /* include fod */
    } else {
        func_mode = FUNC_MODE_NONE;
    }

    return func_mode; /* gesture or ps or suspend */
}


u32 tran_call_tp_cmd_handle(u32 cmd, void *pdata)
{
    u32 ret = 0;
    struct tran_ts_controller *pController = g_tran_ts.controller[0];

    if(!pController || !pController->controller_init_state) {
        return 0;
    }

    TRAN_INFO("get ext cmd 0x%x",cmd);

    switch(cmd)
    {
    case SET_TS_REST:
        ret = set_tp_reset(&g_tran_ts, pdata);
        break;

    case SET_TS_DDI_RESUME:
        ret = set_tp_tddi_resume(&g_tran_ts, pdata);
        break;
    case GET_TS_MODE:
        ret = get_tp_mode(&g_tran_ts, pdata);
        break;
    case SET_TS_IRQ_EN:
        ret = set_tp_irq(&g_tran_ts, pdata);
        break;
    case CALL_TP_FWLOAD:
        ret = call_tp_fwload(&g_tran_ts, pdata);
        break;
    default:
        ret = 0;
        break;
    }

    return ret;
}
//EXPORT_SYMBOL_GPL(tran_call_tp_cmd_handle);

//USD: add display kick idle by jiangzhu.yang start
void tran_disp_kick_ilde(struct tran_ts_controller *ctr)
{
//    tran_disp_touch_kickidle();
}
//USD: add display kick idle by jiangzhu.yang end

// USD: add to Touch Gesture by ke.li 20251128 start
void ext_tp_function(struct device *dev)
{
    int ret = 0;
    struct device_node *np = dev->of_node;

    TRAN_INFO("register tp ext function start");

    ret = of_property_read_bool(np, "touchscreen,ops_mediator_support");
    if (ret)
    {
        tran_register_tp_func(tran_call_tp_cmd_handle, 0);
        TRAN_INFO("register tp opsMediator success");
    }
    else
    {
        TRAN_INFO("opsMediator not support");
    }
    TRAN_INFO("register tp ext function done");
}
// USD: add to Touch Gesture by ke.li 20251128 end