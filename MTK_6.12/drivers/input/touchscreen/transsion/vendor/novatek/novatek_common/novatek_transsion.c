#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include "nt36xxx.h"
#include "novatek_transsion.h"

#define CMD_ENTER_COMMON_USB_PLUGIN    0x53
#define CMD_ENTER_COMMON_USB_PLUGOUT   0x51
#define NVT_CMD_PROX_ON                0xB5
#define NVT_CMD_PROX_OFF               0xB6
#define NVT_CMD_PROX_EVENT             0xB9
#define NVT_CMD_EDGE_LIMIT_VERTICAL    0xBA
#define NVT_CMD_EDGE_LIMIT_LEFT_UP     0xBB
#define NVT_CMD_EDGE_LIMIT_RIGHT_UP    0xBC
//extend 0x7f command set
#define NVT_EXT_CMD                    0x7F
#define NVT_EXT_CMD_AOD_ON             0x01
#define NVT_EXT_CMD_AOD_OFF            0x02
//USD: add for X6725BF15-2 by junyu.chen2 20250225 start
#define NVT_EXT_CMD_POCKET_ON          0x77
#define NVT_EXT_CMD_POCKET_OFF         0x78
//USD: add for X6725BF15-2 by junyu.chen2 20250225 end
extern int register_ts_controller(struct tran_ts_controller *controller);
extern int unregister_ts_controller(struct tran_ts_controller *controller);

struct tran_ts_controller nt36xxx_controller;

/* proximity start */
static int8_t nvt_customizeCmd(uint8_t *cmdBuf, uint8_t cmdLen)
{
    uint8_t buf[8] = {0};
    uint8_t retry = 0;
    int8_t ret = 0;

    if (((cmdLen+1) > sizeof(buf)) || (cmdLen == 0)) {
        NVT_LOG("cmdLen %d error, buffer size %ld\n", cmdLen, sizeof(buf));
        ret = -1;
        return ret;
    };

    memcpy(buf+1, cmdBuf, cmdLen);

    if (nvt_check_fw_reset_state(RESET_STATE_REK) == 0) {
        if (cmdLen == 1)
            NVT_LOG("command = 0x%02X\n", buf[1]);
        else if (cmdLen == 2)
            NVT_LOG("command = 0x%02X 0x%02X\n", buf[1], buf[2]);
        else
            NVT_LOG("command = 0x%02X 0x%02X 0x%02X\n", buf[1], buf[2], buf[3]);

        for (retry = 0; retry < 20; retry++) {
            //---set xdata index to EVENT BUF ADDR---
            nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

            //---switch HOST_CMD---
            buf[0] = EVENT_MAP_HOST_CMD;
            CTP_SPI_WRITE(ts->client, buf, cmdLen + 1);

            msleep(20);

            buf[0] = EVENT_MAP_HOST_CMD;
            buf[1] = 0xFF;
            CTP_SPI_READ(ts->client, buf, 2);

            if (buf[1] == 0x00)
                break;
        }

        if (unlikely(retry == 20)) {
            NVT_ERR("customizeCmd 0x%02X failed, buf[1]=0x%02X\n", cmdBuf[0], buf[1]);
            ret = -1;
        }
    }
    NVT_LOG("--\n");

    return ret;
}

static void nt36xxx_proximity_switch(bool onoff) {
    uint8_t cmd;

    if(onoff) {//proximity on
        cmd = NVT_CMD_PROX_ON;
    } else {//proximity off
        cmd = NVT_CMD_PROX_OFF;
    }
    nvt_customizeCmd(&cmd, 1);
}

static void nt36xxx_usb_charger_switch(bool state) {
    uint8_t cmd;

    if(state) {//charger plugin
        cmd = CMD_ENTER_COMMON_USB_PLUGIN;
    } else {//charger plugout
        cmd = CMD_ENTER_COMMON_USB_PLUGOUT;
    }
    nvt_customizeCmd(&cmd, 1);
}

int32_t nvt_set_edge_reject_switch(uint8_t edge_reject_switch)
{
    uint8_t cmd;
    int32_t ret = 0;

    if (edge_reject_switch == 3) { // right up
        cmd = NVT_CMD_EDGE_LIMIT_LEFT_UP;
    } else if (edge_reject_switch == 1) { // left up
        cmd = NVT_CMD_EDGE_LIMIT_RIGHT_UP;
    } else{
        cmd = NVT_CMD_EDGE_LIMIT_VERTICAL;
    }

    ret = nvt_customizeCmd(&cmd, 1);
    if (ret < 0) {
        NVT_ERR("Write edge reject switch command fail!\n");
    }

    NVT_LOG("--\n");
    return ret;
}
//USD: add for X6725BF15-2 by junyu.chen2 20250225 start
void set_pocket_mode_switch(uint8_t is_pocket_in )
{
    uint8_t cmd[1];
    int val = 0;
    NVT_LOG("enter pocket mode");
    if (is_pocket_in == 1) { // pocket on
        cmd[0] = NVT_EXT_CMD_POCKET_ON;
    } else { // pocket off
        cmd[0] = NVT_EXT_CMD_POCKET_OFF;
    }
    val = nvt_customizeCmd(cmd, 1);
    if(!val){
        NVT_LOG("cjy %s pocket mode",is_pocket_in ?"enable":"disable");
        ts->is_pocket_in = is_pocket_in;
    }
}
//USD: add for X6725BF15-2 by junyu.chen2 20250225 end
//BSP: add aod mode by guoli.niu 20240702 start
void nvt_aod_mode_switch(uint8_t aod_enable)
{
    uint8_t cmd[2];
    int val = 0;
    if (aod_enable == 1) { // aod on
        if(ts->lcd_call_fw){
            int ret = 0;
            NVT_LOG("Moving FW update to LCD resume ");
            ret = wait_for_completion_timeout(&ts->fw_comp, msecs_to_jiffies(300));
            if (ret == 0)
                NVT_LOG("FW Download timeout!!!");
        } else if (nvt_choose_fw(true)){
            NVT_ERR("download firmware failed, ignore check fw state\n");
        }
        cmd[0] = NVT_EXT_CMD;
        cmd[1] = NVT_EXT_CMD_AOD_ON;
    } else { // aod off
        cmd[0] = NVT_EXT_CMD;
        cmd[1] = NVT_EXT_CMD_AOD_OFF;
    }
    val = nvt_customizeCmd(cmd, 2);
    if(!val){
        NVT_LOG("%s aod mode",aod_enable?"enable":"disable");
        ts->aod_enable = aod_enable;
    }
}
//BSP: add aod mode by guoli.niu 20240702 end

static inline void nt36xxx_ps_resume(void){
    /*bsp:add for wake system in ps suspend mode 20231103 start*/
    if(ts->irq_wakeup_enabled == true){
        disable_irq_wake(ts->client->irq);
        ts->irq_wakeup_enabled = false;
    }
    /*bsp:add for wake system in ps suspend mode 20231103 end*/
	NVT_LOG("ps resume");
}

static inline void nt36xxx_ps_suspend(void) {
    int i;
    uint8_t cmd;

    /*bsp:add for wake system in ps suspend mode 20231103 start*/
    if(ts->irq_wakeup_enabled == false){
        enable_irq_wake(ts->client->irq);
        ts->irq_wakeup_enabled = true;
    }
    /*bsp:add for wake system in ps suspend mode 20231103 end*/

    if(nt36xxx_controller.tran_ts_data->ps_state.lastPsState == TRAN_PS_NEAR){
        cmd = NVT_CMD_PROX_EVENT;
        nvt_customizeCmd(&cmd, 1);
    }
    /* release all touches */
#if MT_PROTOCOL_B
    for (i = 0; i < ts->max_touch_num; i++) {
        input_mt_slot(ts->input_dev, i);
        input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
        input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
        input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
    }
#endif
    input_report_key(ts->input_dev, BTN_TOUCH, 0);
#if !MT_PROTOCOL_B
    input_mt_sync(ts->input_dev);
#endif
    input_sync(ts->input_dev);

    NVT_LOG("ps suspend");
}

/* proximity end */

static int tran_ts_init(void)
{
    int ret = 0;
    TRAN_FUNC_ENTER();
    nvt_driver_init();
    TRAN_FUNC_EXIT();
    return ret;
}

static void suspend(struct device *h, int opt)
{
    if(ts != NULL) {
        if(nt36xxx_controller.controller_state == TRAN_PS_SUSPEND_MODE) {
            nt36xxx_ps_suspend();
        } else {
            nvt_ts_suspend(&ts->client->dev);
        }
    }
}

static void resume(struct device *h, int opt)
{
    if(ts != NULL) {
        if(nt36xxx_controller.controller_state == TRAN_PS_SUSPEND_MODE) {
            nt36xxx_ps_resume();
        } else {
            nvt_ts_resume(&ts->client->dev);
        }
    }
}

static void set_ts_mode(u32 cmd, u32 state)
{
    switch(cmd) {
    case SET_NOISE_MODE:
        nt36xxx_usb_charger_switch(state);
        break;

    case SET_PS_MODE:
        nt36xxx_proximity_switch(state);
        break;

    case SET_GAME_MODE:
        break;

    case SET_EDGE_REJECT_MODE:
        nvt_set_edge_reject_switch(state);
        break;

    case LCD_AOD_MODE:
        nvt_aod_mode_switch(state);
        break;
    //USD: add for X6725BF15-2 by junyu.chen2 20250225 start
    case SET_POCKET_MODE:
        set_pocket_mode_switch(state);
        break;
    //USD: add for X6725BF15-2 by junyu.chen2 20250225 end
    default:
        TRAN_INFO("can't find cmd=0x%x",cmd);
        break;
    }
}

static unsigned char get_firmware_version(void)
{
    u8 ret = 0;

    nvt_get_fw_info();
    ret = ts->fw_ver;

    return ret;
}

static unsigned char get_base_version(void)
{
    u8 ret = 0;

    ret = ts->bv;

    return ret;
}

static int tp_selftest(void)
{
    int ret = 0;
    TRAN_FUNC_ENTER();
    ret = tran_get_nvt_SelftestResult();
    TRAN_FUNC_EXIT();
    return ret;

}

static int get_selftest_data(char * data)
{
    TRAN_FUNC_ENTER();

    return 0;
}

static int tran_selftest_open(struct inode *inode, struct file *file)
{
    TRAN_FUNC_ENTER();
    nvt_selftest_open(inode,file);
    return 0;
}

static void resume_by_ddi(void)
{
    /* do nothing */
}

static void tp_reset_level(int val)
{
    /* do noting */
}

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

static struct vendor_gesture_map  gestures_maps[] = {
    {GESTURE_SLIDE_LEFT,       GESTURE_LF},
    {GESTURE_SLIDE_UP,         GESTURE_up},
    {GESTURE_SLIDE_RIGHT,      GESTURE_RT},
    {GESTURE_SLIDE_DOWN,       GESTURE_down},
    {GESTURE_DOUBLE_CLICK,     GESTURE_DC},
    {GESTURE_WORD_W,           GESTURE_w},
    {GESTURE_WORD_O,           GESTURE_o},
    {GESTURE_WORD_M,           GESTURE_m},
    {GESTURE_WORD_e,           GESTURE_e},
    {GESTURE_WORD_C,           GESTURE_c},
    {GESTURE_WORD_Z,           GESTURE_z},
    {GESTURE_WORD_S,           GESTURE_s},
    {GESTURE_WORD_V,           GESTURE_v},
    {GESTURE_ONECLICK,         GESTURE_OC},
    {0,0},
};

/*USD: add for bringup X6872H965-2 by longtao.ran 20240521 start*/
static char *support_list[] = {
    "nt36528a",
    "nt36528",
    "nt36672c",
    "nt36672e",
    "nt36672n",
    "nt36523n",
    "nt36532",
    "nt36672s",
    NULL /* not modify */
};
/*USD: add for bringup X6872H965-2 by longtao.ran 20240521 end*/

static struct tran_controller_ops controller_ops = {
    .tran_ts_init = tran_ts_init,
    .tran_ts_suspend = suspend,
    .tran_ts_resume = resume,
    .set_ts_mode= set_ts_mode,
    .get_firmware_version = get_firmware_version,
    .get_base_version = get_base_version,
    .tran_selftest = tp_selftest,
    .get_selftest_data = get_selftest_data,
    .tran_selftest_open = tran_selftest_open,
};
//#USD: add for fwload by junyu.chen2 20251216 start
static struct tran_controller_ext_ops controller_ext_ops = {
    .tran_tp_reset = tp_reset_level,
    .resume_by_ddi = resume_by_ddi,
    .call_tp_fwload = nvt_fwresume_work,
};
//#USD: add for fwload by junyu.chen2 20251216 end

struct tran_ts_controller nt36xxx_controller = {
    .cur_ic = NULL,
    .tran_ts_data = NULL,
    .selftest_data = NULL,
    .controller_state = TRAN_ACTIVE_MODE,
    .vendor_name = "novatek",
    .gesture_flag = true,
    .gesture_map = gestures_maps,
    .support_ic = support_list,
    .ops = &controller_ops,
    .ext_ops = &controller_ext_ops,
    .dsp_event_mutex = __MUTEX_INITIALIZER(nt36xxx_controller.dsp_event_mutex),
};

static int __init controller_module_init(void)
{
    register_ts_controller(&nt36xxx_controller);
    return 0;
}

static void __exit controller_module_exit(void)
{
    /* 清除transsion core的资源 */
    unregister_ts_controller(&nt36xxx_controller);

    /* 清除当前驱动的资源 */
    nvt_driver_exit();
}

module_init(controller_module_init);
module_exit(controller_module_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
