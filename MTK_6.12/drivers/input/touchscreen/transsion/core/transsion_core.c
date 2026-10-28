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

extern struct tran_ts_core g_tran_ts;
extern struct tran_core_ops g_tpd_ops;
extern struct platform_driver sub_tpd_driver;
//int g_h833c_hw = 0;

// USD: add to Touch Gesture by ke.li 20251128 start
extern void ext_tp_function(struct device *dev);
// USD: add to Touch Gesture by ke.li 20251128 end

//USD: add tne log by jiangzhu.yang start
#if TRAN_TNE_LOG_EN
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
#include "tranlog.h"
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT) && IS_ENABLED(CONFIG_TRAN_FREEZE_AEGIS_SUPPORT)
#include <tk/tnek.h>
extern int startAegisK(const char *code, const char *pids, const char *extra);
__attribute__((unused)) const static int64_t FLAG_TNE_USER_LOG_MAIN_SYS = 1 << 1; //mobile log:android and system log
__attribute__((unused)) const static int64_t FLAG_TNE_USER_LOG_KERNEL = 1 << 5;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_LAST_KMSG = 1<< 11;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_PROPERTY = 1<<18;
#endif
extern int register_cloudengine_notifier(const char* id, const char* ver, struct notifier_block *nb);
extern int unregister_cloudengine_notifier(const char* id, struct notifier_block *nb);
extern void cloudengine_feed_back(const char* id, int state);
extern char* cloudengine_getConfig(const char* id);
extern char* cloudengine_getStringByContent(char* content, const char* key);
extern int cloudengine_getIntByContent(char* content, const char* key);
extern char* cloudengine_getStringByID(const char* id, const char* key);
extern int cloudengine_getIntByID(const char* id, const char* key);

static char* regID = "1008000016";
static const char tp_module_cloudengine_key[] = "tp_module_cloudengine_key";
static bool is_cloud_engine_notifier_registed = false;
static bool tp_module_tnelog_enable = true;
#endif
#endif
//USD: add tne log by jiangzhu.yang end

struct tran_ctrl_match_work {
    struct work_struct work;
    struct tran_ts_controller *ctrl;
};

struct ts_machine
{
    enum TRAN_TRIGER_EVENT triger;
    enum TS_FUNC_MODE ts_func_mode;
    enum TS_CONTROLLER_MODE current_core_mode;
    enum TS_CONTROLLER_MODE next_core_mode;

};

static struct ts_machine ts_state_machine[] = {
    /*                |        function mode       | cur state         | next state            */
    /* resume */
    {RESUME_TRIGER,     FUNC_MODE_NONE,     TRAN_SUSPEND_MODE,          TRAN_ACTIVE_MODE},
    {RESUME_TRIGER,     FUNC_MODE_NONE,     TRAN_GESTURE_SUSPEND_MODE,  TRAN_ACTIVE_MODE},
    {RESUME_TRIGER,     FUNC_MODE_GESTURE,  TRAN_GESTURE_SUSPEND_MODE,  TRAN_ACTIVE_MODE},
    {RESUME_TRIGER,     FUNC_MODE_GESTURE,  TRAN_SUSPEND_MODE,          TRAN_ACTIVE_MODE},
    /* suspend */
    {SUSPEND_TRIGER,     FUNC_MODE_NONE,    TRAN_ACTIVE_MODE,           TRAN_SUSPEND_MODE},
    {SUSPEND_TRIGER,     FUNC_MODE_NONE,    TRAN_GESTURE_SUSPEND_MODE,  TRAN_SUSPEND_MODE},
    {SUSPEND_TRIGER,     FUNC_MODE_GESTURE, TRAN_ACTIVE_MODE,           TRAN_GESTURE_SUSPEND_MODE},
    {SUSPEND_TRIGER,     FUNC_MODE_GESTURE, TRAN_SUSPEND_MODE,          TRAN_GESTURE_SUSPEND_MODE}, //gesture_flag = 2
};

/* 1: success 0: fail */
int get_tpd_ops(struct tran_ts_controller *controller, struct tran_core_ops **ops)
{
    if(controller->tran_ts_data != NULL) {
        *ops = controller->tran_ts_data->tpd_ops;
        if(*ops != NULL)
            return 1;
    }

    TRAN_INFO("get ops fail !");
    return 0;
}

static char* enum_to_str(u32 enumVar)
{
    switch(enumVar) {
    case TRAN_ACTIVE_MODE:
        return ENUM_TO_STRING(TRAN_ACTIVE_MODE);
    case TRAN_SUSPEND_MODE:
        return ENUM_TO_STRING(TRAN_SUSPEND_MODE);
    case TRAN_PS_SUSPEND_MODE:
        return ENUM_TO_STRING(TRAN_PS_SUSPEND_MODE);
    case TRAN_GESTURE_SUSPEND_MODE:
        return ENUM_TO_STRING(TRAN_GESTURE_SUSPEND_MODE);
    default:
        return "unknow";
    }
}

struct tran_ts_controller *get_active_controller(void)
{

    /* Check whether controller is registered */
    if(g_tran_ts.controller[1] == NULL && g_tran_ts.controller[0] == NULL)
        return NULL;

    /* Check whether controller is ready */
    if((g_tran_ts.controller[0] && g_tran_ts.controller[0]->controller_init_state == true ) \
            && g_tran_ts.controller[0]->controller_state == TRAN_ACTIVE_MODE) {
        return g_tran_ts.controller[0];
    } else if((g_tran_ts.controller[1] && g_tran_ts.controller[1]->controller_init_state == true ) \
              && g_tran_ts.controller[1]->controller_state == TRAN_ACTIVE_MODE) {
        return g_tran_ts.controller[1];
    } else {
        return NULL;
    }
}

int state_change_machine(u32 triger, struct tran_ts_controller *pController)
{
    int i;
    int gesture_state = 0;
    u32 func_mode = FUNC_MODE_NONE;
    u32 current_state;

    /* check if controller is ready */
    if(!pController || pController->controller_init_state == false) {
        return -1;
    }

    current_state = pController->controller_state;

    if(triger == RESUME_TRIGER && current_state == TRAN_ACTIVE_MODE) {
        TRAN_DEBUG("cur ic:%s %s can't change state, current %s", pController->cur_ic, \
                   ENUM_TO_STRING(RESUME_TRIGER), enum_to_str(current_state));
        return 0;
    }

    if(g_tran_ts.tpd_ops->gesture_state_get)
        gesture_state = g_tran_ts.tpd_ops->gesture_state_get(pController);

    /* get function mode */
    if(gesture_state != 0) {
        func_mode = FUNC_MODE_GESTURE; /* include fod */
    } else {
        func_mode = FUNC_MODE_NONE;
    }

    /* change core mode */
    for(i = 0; i < sizeof(ts_state_machine)/sizeof(struct ts_machine); i++) {
        if(ts_state_machine[i].ts_func_mode == func_mode \
                && ts_state_machine[i].current_core_mode == current_state \
                && ts_state_machine[i].triger == triger) {

            pController->controller_state = ts_state_machine[i].next_core_mode;
            TRAN_INFO("[%s]: %s --> %s", pController->supplier->controller_type?"sub":"main", \
                      enum_to_str(ts_state_machine[i].current_core_mode), \
                      enum_to_str(pController->controller_state));
            break;
        }
    }
    return 0;
}

static void usb_event_callback(struct work_struct *work)
{
    struct tran_ts_controller *pController = NULL;

    pController = get_active_controller();
    if(IS_ERR_OR_NULL(pController)) {
        TRAN_ERROR("Failed to get pController");
        return;
    }

    if(pController->controller_state == TRAN_SUSPEND_MODE) {
        TRAN_INFO("in suspend cancel ops");
        return;
    }

    if(pController && pController->controller_init_state) {
        if(g_tran_ts.charger_in) {
            pController->ops->set_ts_mode(SET_NOISE_MODE, g_tran_ts.charger_in);
            TRAN_INFO("noise high mode in state = %d",g_tran_ts.charger_in);
        } else {
            pController->ops->set_ts_mode(SET_NOISE_MODE, g_tran_ts.charger_in);
            TRAN_INFO("noise normal mode in state = %d",g_tran_ts.charger_in);
        }
    } else {
        TRAN_INFO("controller probe is not finish!");
    }
}

/* tp state recovery */
static int tp_state_recovery(void)
{
    int ret;
    struct tran_ts_controller *pController = NULL;

    pController = get_active_controller();
    if(IS_ERR_OR_NULL(pController)) {
        TRAN_ERROR("Failed to get pController");
        ret = PTR_ERR(pController);
        return ret;
    }

    TRAN_INFO("%s recovery function:",pController->cur_ic);
    if (pController->controller_init_state && pController->ops->set_ts_mode) {
        pController->ops->set_ts_mode(SET_EDGE_REJECT_MODE, g_tran_ts.switch_mode);

        if (g_tran_ts.charger_in) {
            pController->ops->set_ts_mode(SET_NOISE_MODE, g_tran_ts.charger_in);

        }
        //edge leve
        pController->ops->set_ts_mode(SET_EDGE_LEVER_MODE, g_tran_ts.g_tran_edge_level);

        //esports
        pController->ops->set_ts_mode(TRANSIENT_SAMPLING_RATE_SWITCH, g_tran_ts.g_tran_transient_report_rate);
        pController->ops->set_ts_mode(REPORT_RATE_SWITCH, g_tran_ts.g_tran_report_rate);
        pController->ops->set_ts_mode(LOCK_POINT_SWITCH, g_tran_ts.g_tran_game_mode);

        //pocket_mode
        pController->ops->set_ts_mode(SET_POCKET_MODE, g_tran_ts.tran_pocket_state);

        //get scene
        pController->ops->set_ts_mode(GET_SCENE_STATUS, g_tran_ts.g_tran_tp_scene);

        //USD: add for tp feature by jiafeng.fan 20250905 start
        pController->ops->set_ts_mode(MICRO_CONTROL_ACCURACY, g_tran_ts.g_tran_micro_control_accuracy);
        TRAN_INFO("resume recover micro control accuracyt state = %d",g_tran_ts.g_tran_micro_control_accuracy);
        //USD: add for tp feature by jiafeng.fan 20250905 end
        TRAN_INFO("edge leve : %d",g_tran_ts.g_tran_edge_level);
        TRAN_INFO("edge mode: %d",g_tran_ts.switch_mode);
        TRAN_INFO("noise mode: %d",g_tran_ts.charger_in);
        TRAN_INFO("transient report rate state : %d",g_tran_ts.g_tran_transient_report_rate);
        TRAN_INFO("tran report rate state : %d",g_tran_ts.g_tran_report_rate);
        TRAN_INFO("lock point state : %d",g_tran_ts.g_tran_game_mode);
        TRAN_INFO("resume recover pocket state = %d",g_tran_ts.tran_pocket_state);
        TRAN_INFO("resume recover scene state = %d",g_tran_ts.g_tran_tp_scene);
    }

    return 0;
}

/* just resume work */
static void dsp_event_callback(struct work_struct *work)
{
    struct tran_ts_controller *pController = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];

    if(pController && pController->controller_init_state) {
        pController->ops->tran_ts_resume(NULL, 0);
    } else {
        TRAN_INFO("vendor_probe_result is false!");
        if(pController) {
            mutex_unlock(&pController->dsp_event_mutex);
        }
        return;
    }

    state_change_machine(RESUME_TRIGER, pController);/* must after resume */

    /* After restoring IC status in the screen on */
    tp_state_recovery();
    mutex_unlock(&pController->dsp_event_mutex);
    TRAN_INFO("[main] drm notify tp resume end");
}

static void sub_dsp_event_callback(struct work_struct *work)
{
    struct tran_ts_controller *pController = g_tran_ts.controller[TRAN_SUB_CONTROLLER];

    if(pController && pController->controller_init_state) {
        pController->ops->tran_ts_resume(NULL, 0);
    } else {
        TRAN_INFO("vendor_probe_result is false!");
        if(pController) {
            mutex_unlock(&pController->dsp_event_mutex);
        }
        return;
    }

    state_change_machine(RESUME_TRIGER, pController);/* must after resume */

    /* After restoring IC status in the screen on */
    tp_state_recovery();
    mutex_unlock(&pController->dsp_event_mutex);
    TRAN_INFO("[sub] drm notify tp resume end");
}

static int check_support_ic(struct tran_ts_core *pdata, struct tran_ts_controller *controller, struct supplier_list *supList)
{
    int i;
    char **support_ic = NULL;

    if(controller != NULL) {
        support_ic = controller->support_ic;
        for(i=0; support_ic[i] != NULL; i++) {
            if(strstr(supList->supplier_info, support_ic[i]))
            {
                controller->cur_ic = support_ic[i];
                controller->tran_ts_data = pdata;
                controller->supplier = supList;
                pdata->controller[supList->controller_type] = controller;
                TRAN_INFO("controller[%d],support_ic: %s", supList->controller_type, support_ic[i]);
                return 1;
            }
        }
    }

    pdata->controller[supList->controller_type] = NULL;
    TRAN_INFO("controller not support ic which in supplier-%d", supList->supplier_num);
    return 0;
}

static inline void tran_platform_init(struct tran_ts_controller *ctrl) {
    /* just init once */
    if(!g_tran_ts.platform_data->init_flag) {
        TRAN_FUNC_ENTER();
        /* platform init
        * 1.dsp notify
        * 2.charger notify
        * 3.midtest info
        */
        gesture_init(&g_tran_ts);

        /* common node
        * 1.display rotation function
        * 2.gesture function
        * 3.tp selftest function
        */
        tran_node_init(&g_tran_ts);
        g_tran_ts.platform_data->init_flag = 1;
        TRAN_FUNC_EXIT();
    }

    if(ctrl->supplier->controller_type == TRAN_MAIN_CONTROLLER) {
        g_tran_ts.platform_data->ops->plat_init(&g_tran_ts);
        TRAN_INFO("main plat init");
    } else if(ctrl->supplier->controller_type == TRAN_SUB_CONTROLLER) {
        g_tran_ts.platform_data->ops->sub_plat_init(&g_tran_ts);
        TRAN_INFO("sub plat init");
    }
}

static int match_tp_controller(struct tran_ts_core *pdata, struct tran_ts_controller *ctrl)
{
    int ret = 0;
    int is_empty_name = -1;
    char lcm_name[64]= {0};
    struct supplier_list *supList = NULL;
    struct supplier_list *tmpList = NULL;
    struct tran_ts_platform *platData = pdata->platform_data;

    /* 参数检查 */
    if (!ctrl || !ctrl->vendor_name) {
        TRAN_ERROR("Invalid controller or vendor_name is NULL");
        return -1;
    }

    /* get lcm name */
    if (platData && platData->ops && platData->ops->get_lcm_name) {
        is_empty_name = platData->ops->get_lcm_name(lcm_name);
        TRAN_INFO("get_lcm_name %s, is_empty %d", lcm_name, is_empty_name);
    }

    /* 第一阶段：查找匹配的供应商 - 需要锁保护 */
    mutex_lock(&pdata->core_data_mutex);
    
    list_for_each_entry(tmpList, &pdata->supplier_head, next) {
        if (tmpList->is_matched)
            continue;

        /* find supplier by lcm name */
        if (is_empty_name == 0 && strstr(tmpList->supplier_info, lcm_name)) {
            TRAN_INFO("match supplier-%d", tmpList->supplier_num);
            supList = tmpList;
            break;
        } else if (strstr(tmpList->supplier_info, "any")) {
            TRAN_INFO("Force match supplier-%d", tmpList->supplier_num);
            supList = tmpList;
            break;
        }
    }

    if (!supList) {
        TRAN_ERROR("no supplier matched!!");
        mutex_unlock(&pdata->core_data_mutex);
        return -1;
    }

    /* match controller and supplier */
    if (!strstr(supList->supplier_info, ctrl->vendor_name)) {
        TRAN_ERROR("vendor_name %s not match with supplier_info %s", 
                  ctrl->vendor_name, supList->supplier_info);
        mutex_unlock(&pdata->core_data_mutex);
        return -1;
    }

    /* get support ic */
    ret = check_support_ic(pdata, ctrl, supList);
    if (!ret) {
        TRAN_ERROR("No supported IC found in supplier-%d", supList->supplier_num);
        mutex_unlock(&pdata->core_data_mutex);
        return -1;
    }

    /* 标记供应商已匹配 */
    supList->is_matched = true;
    
    /* 完成共享资源访问，释放锁 */
    mutex_unlock(&pdata->core_data_mutex);

    /* 第二阶段：初始化控制器 - 不需要锁保护 */
    TRAN_INFO("start load %s controller vendor:%s ic:%s", 
              supList->controller_type ? "sub" : "main", 
              ctrl->vendor_name, ctrl->cur_ic);

    ctrl->ops->tran_ts_init();

    /* 等待初始化完成，添加超时处理 */
    if (0 == wait_for_completion_timeout(&ctrl->comp, msecs_to_jiffies(8000))) {
        TRAN_ERROR("load %s controller vendor:%s ic:%s timeout !!!", 
                   supList->controller_type ? "sub" : "main", 
                   ctrl->vendor_name, ctrl->cur_ic);
        return -1;
    } 

    TRAN_INFO("load %s controller vendor:%s ic:%s success !!!", 
              supList->controller_type ? "sub" : "main", 
              ctrl->vendor_name, ctrl->cur_ic);
    
    return 0;
}

static void controller_match_work(struct work_struct *work)
{
    int ret;
    struct tran_ctrl_match_work *ctrl_match_work = container_of(work, struct tran_ctrl_match_work, work);
    struct tran_ts_controller *pController = ctrl_match_work->ctrl;

    /* boot mode eq 1, don't load*/
    if(g_tran_ts.controller[TRAN_MAIN_CONTROLLER] != NULL \
            && g_tran_ts.controller[TRAN_SUB_CONTROLLER] != NULL) {
        TRAN_ERROR("Not allow to match controller");
        return;
    }

    ret = match_tp_controller(&g_tran_ts, pController);
    if(ret) {
        /* do nothing */
    } else {
        tran_platform_init(pController);
    }

    kfree(ctrl_match_work);
}

int register_ts_controller(struct tran_ts_controller *controller)
{
    struct tran_ctrl_match_work *work = NULL;

    if(controller == NULL || g_tran_ts.boot_mode == TRAN_NOT_RUN_MODE) {
        TRAN_INFO("Don't register reason %s",controller?"TRAN_NOT_RUN_MODE":"controller is null");
        return -1;
    }

    if (list_empty(&g_tran_ts.supplier_head)) {
        return -1;
    }

    /* init ctrl match work */
    work = (struct tran_ctrl_match_work *)kmalloc(sizeof(struct tran_ctrl_match_work), GFP_KERNEL);
    if(work == NULL) {
        TRAN_ERROR("request tran ctrl match work mem failed");
        return -1;
    }

    /* init work */
    INIT_WORK(&work->work, controller_match_work);
    work->ctrl = controller;

    /*USD: modify for deadlock by guoxin.zhu 20240624 start*/
    /* schedule match work */
    if (!queue_work(g_tran_ts.tpd_init_workqueue, &work->work))
        TRAN_ERROR("touch device queue_work failed\n");
    /*USD: modify for deadlock by guoxin.zhu 20240624 end*/
    return 0;

}
EXPORT_SYMBOL_GPL(register_ts_controller);

int unregister_ts_controller(struct tran_ts_controller *controller)
{
    /* 注销设备节点 */

    /* 注销通知�?*/

    /* 释放内存 */
    return 0;
}
EXPORT_SYMBOL_GPL(unregister_ts_controller);

static int release_all_list(void)
{
    return 0;
}

static int parse_touch_feature(struct device_node *np, struct tran_ts_core *core_data)
{
    struct touchscreen_feature *feature = &core_data->features;
    int ret = 0;
    u32 value;

    /* esport version */
    if (!of_property_read_u32(np, "touchscreen,esports-engine", &value)) {
        if (value > 4) {
            TRAN_ERROR("Invalid esports version: %d\n", value);
            feature->esport = 0;
        } else {
            feature->esport = value;
        }
    } else {
        feature->esport = 0; // not support esports */
    }

    /* 口袋模式防误触 */
    feature->pocket_amt = of_property_read_bool(np, "touchscreen,pocket-mode-amt-support");
    /* 边缘防误触 */
    feature->curved_edge_amt = of_property_read_bool(np, "touchscreen,curved-edge-amt-support");
    /* 游戏防误触 */
    feature->game_amt = of_property_read_bool(np, "touchscreen,gaming-amt-support");

    /* 动态报点率模式 */
    feature->dynamic_report_rate = of_property_read_bool(np, "touchscreen,dynamic-report-rate-support");

    /* 屏下指纹支持 */
    /* BSP:modify cn5c fingerprint-support AR-202507-001606 by jiafeng.fan 20250812 start */
    feature->fod = of_property_read_bool(np, "touchscreen,fingerprint-support");
    /* BSP:modify cn5c fingerprint-support AR-202507-001606 by jiafeng.fan 20250812 end */

    /* 双屏 */
    feature->fold_touch = of_property_read_bool(np, "touchscreen,fold-touch");

    /* 获取场景 */
    feature->tp_scene = of_property_read_bool(np, "touchscreen,tp-scene-support");

    return ret;
}

static int tran_parse_dt(struct device *dev, struct tran_ts_core *pdata)
{
    int i,len,ret;
    int supplier_num = 0;
    int controller_type = 0;
    /* Multiples of resolution */
    u32 resolution_multiples = 1;
    /* tx/rx num */
    u32 tx_num = 0;
    u32 rx_num = 0;
    const char *supplier_info = NULL;
    const char *supplier_vendor= NULL;
    struct supplier_list *tmpList = NULL;
    const char *supplier_fw = NULL;
    const char *supplier_testfw = NULL;
    char supplierx[32] = {0};

    struct device_node *np = dev->of_node;

    TRAN_FUNC_ENTER();
    /* get support function info */
    parse_touch_feature(np, pdata);

    /* get tp supplier map */
    ret = of_property_read_u32(np, "supplier-num", &supplier_num);
    if(0 > ret) {
        TRAN_ERROR("supplier-num: can not find!");
        return ret;
    }
    TRAN_INFO("supplier-num: %d",supplier_num);

    /* get controller type */
    ret = of_property_read_u32(np, "controller_type", &controller_type);
    if(0 > ret) {
        TRAN_INFO("controller_type: can not find, use default value!");
        controller_type = 0;
    }
    TRAN_INFO("controller_type: %d",controller_type);

    /* get tp supplier info */
    for(i=0; i<supplier_num; i++) {
        /* clear data */
        tmpList = NULL;
        supplier_info = NULL;
        supplier_vendor= NULL;
        memset(supplierx,0,32);

        /* get supplier info */
        snprintf(supplierx, sizeof("supplier-x"), "supplier-%d", i);
        supplier_info = of_get_property(np, (const char*)supplierx, &len);
        if(supplier_info == NULL) {
            TRAN_ERROR("supplier-%d: can not find!", i);
            return ret;
        } else {
            TRAN_INFO("supplier-%d: %s", i, supplier_info);
        }
        /* get fw info */
        snprintf(supplierx, sizeof("supplier-vendor-fwname-x"), "supplier-vendor-fwname-%d", i);
        supplier_fw = of_get_property(np, (const char*)supplierx, &len);
        if(IS_ERR_OR_NULL(supplier_fw)) {
           TRAN_ERROR("supplier-vendor-fwname-%d: can not find!", i);
           supplier_fw = NULL;
        } else {
            TRAN_INFO("supplier-vendor-fwname-%d: %s", i, supplier_fw);
         }
        /* get ini info */
        snprintf(supplierx, sizeof("supplier-vendor-testname-x"), "supplier-vendor-testname-%d", i);
        supplier_testfw = of_get_property(np, (const char*)supplierx, &len);
        if(IS_ERR_OR_NULL(supplier_testfw)) {
            TRAN_ERROR("supplier-vendor-testname-%d: can not find!", i);
            supplier_testfw = NULL;
         } else {
            TRAN_INFO("supplier-vendor-testname-%d: %s", i, supplier_testfw);
         }
        /* get Module factory */
        snprintf(supplierx, sizeof("supplier-vendor-x"), "supplier-vendor-%d", i);
        supplier_vendor = of_get_property(np, (const char*)supplierx, &len);
        if(supplier_vendor == NULL) {
            TRAN_INFO("supplier-vendor-%d: can not find!", i);
        } else {
            TRAN_INFO("supplier-vendor-%d: %s", i, supplier_vendor);
        }

        /* get Multiples of resolution */
        snprintf(supplierx, sizeof("supplier-resolution-x"), "supplier-resolution-%d", i);
        ret = of_property_read_u32(np, (const char*)supplierx, &resolution_multiples);
        if(0 > ret) {
            TRAN_ERROR("supplier-resolution-%d: can not find!", i);
            resolution_multiples = 1;
        }
        TRAN_INFO("supplier-resolution-%d: %d", i, resolution_multiples);

        /* get tx/rx num */
        snprintf(supplierx, sizeof("supplier-tx-num-x"), "supplier-tx-num-%d", i);
        ret = of_property_read_u32(np, (const char*)supplierx, &tx_num);
        if(0 > ret) {
            TRAN_ERROR("supplier-tx-num-%d: unable to find!", i);
            tx_num = 1;
        }
        TRAN_INFO("supplier-tx-num-%d: %d", i, tx_num);

        snprintf(supplierx, sizeof("supplier-rx-num-x"), "supplier-rx-num-%d", i);
        ret = of_property_read_u32(np, (const char*)supplierx, &rx_num);
        if(0 > ret) {
            TRAN_ERROR("supplier-rx-num-%d: unable to find!", i);
            rx_num = 1;
        }
        TRAN_INFO("supplier-rx-num-%d: %d", i, rx_num);

        /* request mem */
        tmpList = (struct supplier_list *)vmalloc(sizeof(struct supplier_list));
        if(tmpList == NULL) {
            TRAN_ERROR("request supplier_list mem failed!");
            return ret;
        }

        /* init & add to list */
        tmpList->supplier_num = i;
        tmpList->supplier_info = (char *)supplier_info;
        tmpList->supplier_vendor = (char *)supplier_vendor;
        tmpList->supplier_fw = (char *)supplier_fw;
        tmpList->supplier_testfw = (char *)supplier_testfw;
        tmpList->resolution_multiples = resolution_multiples;
        tmpList->controller_type = controller_type;
        tmpList->tx_num = tx_num;
        tmpList->rx_num = rx_num;
        tmpList->is_matched = false;
        mutex_lock(&g_tran_ts.core_data_mutex);
        list_add_tail(&tmpList->next, &pdata->supplier_head);
        mutex_unlock(&g_tran_ts.core_data_mutex);
    }

    TRAN_FUNC_EXIT();

    return 0;
}

static void controller_init_finish(struct tran_ts_controller *ctr)
{
    ctr->controller_init_state = true;
    complete(&ctr->comp);
    TRAN_INFO("%s controller load finish", ctr->cur_ic);
}

static void controller_uninit_finish(struct tran_ts_controller *ctr)
{
    ctr->controller_init_state = false;
    TRAN_INFO("%s controller remove finish", ctr->cur_ic);
}

//USD: add tne log by jiangzhu.yang start
#if TRAN_TNE_LOG_EN
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
//USD: add tne log by junyu.chen2 20250922 start
void tran_tp_module_tnelog(char *str)
{
#if IS_ENABLED(CONFIG_TRAN_FREEZE_AEGIS_SUPPORT)
	//TRAN_INFO("startAegisK: %s", str);
        startAegisK("TP_HUNG","",str);
#endif
}
//USD: add tne log by junyu.chen2 20250922 end
#endif
static int tp_module_cloudengine_change(struct notifier_block *nb, unsigned long action, void* data)
{
    struct cloud_engine_config* cec;
    char* id;
    __attribute__((unused)) char* config;
    char* get_cloudengine_key;

    if(NULL == data)
        return -1;
    cec = (struct cloud_engine_config*)data;
    id = cec->id;
    if(strcmp(regID, id) == 0)
    {
        config = cec->config;
        //Cloud control impl

        get_cloudengine_key = cloudengine_getStringByID(regID, tp_module_cloudengine_key);

        if(get_cloudengine_key == NULL ) {
            pr_info("tp_module_cloudengine_change get_cloudengine_key is null\n");
            return 0;
        } else if (strcmp("true",get_cloudengine_key) == 0) {
            tp_module_tnelog_enable = true;
        } else if (strcmp("false",get_cloudengine_key) == 0) {
            tp_module_tnelog_enable = false;
        }
        pr_info("tp_module_cloudengine_change get_cloudengine_key:%s, tp_module_tnelog_enable:%d\n", get_cloudengine_key, tp_module_tnelog_enable);
        cloudengine_feed_back(regID, 1);
    }
    return 0;
}

struct notifier_block tp_module_notifier_block = {
    .notifier_call = tp_module_cloudengine_change,
};

static int cloud_engine_tp_module_init(void)
{
    if(!is_cloud_engine_notifier_registed) {
        char ver[]="v1.0";
        int ret = -1;
        ret = register_cloudengine_notifier(regID, ver, &tp_module_notifier_block);
        if(ret == 0) {
            is_cloud_engine_notifier_registed = true;
            tp_module_tnelog_enable = true;
            pr_info("cloud_engine_tp_module_init successfull,tp_module_tnelog_enable:%d", tp_module_tnelog_enable);
        }
    }
    return 0;
}

static void cloud_engine_tp_module_exit(void)
{
    int ret = -1;
    if(is_cloud_engine_notifier_registed) {
        ret = unregister_cloudengine_notifier(regID, &tp_module_notifier_block);
        if(ret == 0) {
            pr_info("cloud_engine_tp_module_exit successfull");
            is_cloud_engine_notifier_registed = false;
        }
    }
}

//module_init(cloud_engine_tp_module_init);
//module_exit(cloud_engine_tp_module_exit);
//MODULE_LICENSE("GPL");
#endif
#endif
//USD: add tne log by jiangzhu.yang end

static int tpd_probe(struct platform_device *pdev)
{
    struct tran_ts_platform *tran_pdata = g_tran_ts.platform_data;

    TRAN_FUNC_ENTER();
    TRAN_INFO("mian ctp probe start");

    if(tran_pdata == NULL) {
        TRAN_ERROR("tran platform is NULL!");
        return 0;
    }

    /* parse dts */
    tran_parse_dt(&pdev->dev, &g_tran_ts);

    g_tran_ts.pdev = pdev;

// USD: add to Touch Gesture by ke.li 20251128 start
    ext_tp_function(&pdev->dev);
// USD: add to Touch Gesture by ke.li 20251128 end

    TRAN_FUNC_EXIT();
    return 0;
}

static void tpd_remove(struct platform_device *pdev)
{
    /* deinit notifier */
    struct tran_ts_platform *pdata = g_tran_ts.platform_data;
    if(pdata && pdata->ops->plat_deinit)
        pdata->ops->plat_deinit(&g_tran_ts);

    release_all_list();
    /*remove salve device*/

    /* free memory */

}

const struct dev_pm_ops tpd_pm_ops = {
    .suspend = NULL,
    .resume = NULL,
};

const struct of_device_id touch_of_match[] = {
    { .compatible = "mediatek,touch", },
    {},
};

static void tran_shutdown(struct platform_device * pdev)
{
    struct tran_ts_platform *pdata = g_tran_ts.platform_data;

    TRAN_FUNC_ENTER();
    if(pdata && pdata->ops->plat_deinit)
        pdata->ops->plat_deinit(&g_tran_ts);
}

static struct platform_driver tpd_driver = {
    .remove = tpd_remove,
    .shutdown = tran_shutdown,
    .probe = tpd_probe,
    .driver = {
        .name = "mtk-tpd",
        .pm = &tpd_pm_ops,
        .owner = THIS_MODULE,
        .of_match_table = touch_of_match,
    },
};

struct tran_core_ops g_tpd_ops = {
    .controller_init_completion = controller_init_finish,
    .controller_uninit_completion = controller_uninit_finish,
    //USD: add display kick idle by jiangzhu.yang start
    .tran_call_disp_kick_idle = tran_disp_kick_ilde,
    //USD: add display kick idle by jiangzhu.yang end
    //USD: add tne log by jiangzhu.yang start
#if TRAN_TNE_LOG_EN
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
    .tran_call_tp_module_tnelog = tran_tp_module_tnelog,
#endif
#endif
#endif
    //USD: add tne log by jiangzhu.yang end
};

struct tran_ts_core g_tran_ts = {
    .supplier_head = LIST_HEAD_INIT(g_tran_ts.supplier_head),
    .controller_head = LIST_HEAD_INIT(g_tran_ts.controller_head),
    .core_data_mutex = __MUTEX_INITIALIZER(g_tran_ts.core_data_mutex),
    .dsp_event_mutex = __MUTEX_INITIALIZER(g_tran_ts.dsp_event_mutex),
    .tpd_ops = &g_tpd_ops,
};

/*bsp:add for h833c by guoxin.zhu 20231205 start*/
//static int hw_is_h833c(void)
//{
//    struct device_node *np;
//    const char *cmd_line;
//    char *boot_param = "ae10_hw_h833c";
//    int ret = 0;
//
//    np = of_find_node_by_path("/chosen");
//    if (!np) {
//        TRAN_INFO("Can't get the /chosen");
//        return -EIO;
//    }
//
//    ret = of_property_read_string(np, "bootargs", &cmd_line);
//    if (ret < 0) {
//        TRAN_INFO("Can't get the bootargs");
//        return ret;
//    }
//
//    if(strstr(cmd_line, boot_param)) {
//        return true;
//    } else {
//        return false;
//    }
//}
/*bsp:add for h833c by guoxin.zhu 20231205 end*/

static int __init flod_main_device_init(void)
{
    struct tran_ts_platform *tran_pdata = NULL;

    /*bsp:add for h833c by guoxin.zhu 20231205 start*/
//    g_h833c_hw = hw_is_h833c();
    /*bsp:add for h833c by guoxin.zhu 20231205 end*/
    /*get platform info*/
    init_tran_platform_info();
    tran_pdata = g_tran_ts.platform_data;

    /*USD: modify for deadlock by guoxin.zhu 20240624 start*/
    /* init */
    INIT_WORK(&g_tran_ts.tpd_dsp_work, dsp_event_callback);
    INIT_WORK(&g_tran_ts.tpd_sub_dsp_work, sub_dsp_event_callback);
    INIT_WORK(&g_tran_ts.tpd_usb_work, usb_event_callback);
    /* work queuen init*/
    g_tran_ts.tpd_workqueue = create_singlethread_workqueue("tpd_work_queue");
    g_tran_ts.tpd_init_workqueue = create_singlethread_workqueue("tpd_init_queue");
    /*USD: modify for deadlock by guoxin.zhu 20240624 end*/

    if(tran_pdata && tran_pdata->ops->get_boot_mode) {
        g_tran_ts.boot_mode = tran_pdata->ops->get_boot_mode();
        if(g_tran_ts.boot_mode == TRAN_NORMAL_MODE || g_tran_ts.boot_mode == TRAN_FACTORY_MODE) {
            TRAN_INFO("bootmode %d load touch !", g_tran_ts.boot_mode);
        }
        else
        {
            TRAN_ERROR("bootmode don't load touch !");
            return 0;
        }
    }

    if (platform_driver_register(&tpd_driver) != 0) {
        TRAN_ERROR("unable to register main touch panel driver.\n");
    }

    if (platform_driver_register(&sub_tpd_driver) != 0) {
        TRAN_ERROR("unable to register sub touch panel driver.\n");
    }
    TRAN_INFO("sub_tpd_driver !");
    //USD: add tne log by jiangzhu.yang start
#if TRAN_TNE_LOG_EN
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
    cloud_engine_tp_module_init();
#endif
#endif
    //USD: add tne log by jiangzhu.yang end

    return 0;

}

static void __exit flod_main_device_exit(void)
{
    TRAN_FUNC_ENTER();
    platform_driver_unregister(&tpd_driver);
    //USD: add tne log by jiangzhu.yang start
#if TRAN_TNE_LOG_EN
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
    cloud_engine_tp_module_exit();
#endif
#endif
    //USD: add tne log by jiangzhu.yang end
    TRAN_FUNC_EXIT();
}

module_init(flod_main_device_init);
module_exit(flod_main_device_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Transsion touch panel driver");
MODULE_AUTHOR("<Transsion_BSP@transsion.com>");
