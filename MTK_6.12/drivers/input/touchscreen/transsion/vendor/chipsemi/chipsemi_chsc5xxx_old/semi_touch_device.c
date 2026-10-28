#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#ifdef CONFIG_IDE
#include <linux/ide.h>
#endif
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
//#include <linux/i2c.h>

#include "semi_touch_interface.h"
/*bsp:add for transsion core by longtao.ran 20230515 start*/
#include "chipsemi_transsion.h"
/*bsp:add for transsion core by longtao.ran 20230515 end*/

struct sm_touch_dev st_dev =
{
    .stc.initialize_ok = false,
    .stc.ctp_run_status = 0,
    .stc.custom_function_en = 0,
    .cmd_flag = 0,
    .gest_flag = 0,
    .cnt_last_int = 0,
    .resume_cnt = 0,
    .suspend_cnt = 0,
};

static int input_device_init(struct sm_touch_dev *st_dev)
{
    int ret = 0;
    struct hal_device *client = st_dev->client;

    st_dev->input = devm_input_allocate_device(&client->dev);
    check_return_if_fail(st_dev->input, NULL);

    st_dev->input->name = CHSC_DEVICE_NAME;
    st_dev->input->id.bustype = BUS_HAL;
    st_dev->input->dev.parent = &st_dev->client->dev;

    st_dev->input->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    st_dev->input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
    __set_bit(INPUT_PROP_DIRECT, st_dev->input->propbit);
    set_bit(FP_KEY_VALUE, st_dev->input->keybit); //Fingerprint gesture
    input_set_capability(st_dev->input, EV_KEY, FP_KEY_VALUE); //Fingerprint gesture

    //input_set_abs_params(st_dev->input, ABS_X, 0, SEMI_TOUCH_SOLUTION_X, 0, 0);
    //input_set_abs_params(st_dev->input, ABS_Y, 0, SEMI_TOUCH_SOLUTION_Y, 0, 0);

    input_set_abs_params(st_dev->input, ABS_MT_POSITION_X, 0, st_dev->solution_x, 0, 0);
    input_set_abs_params(st_dev->input, ABS_MT_POSITION_Y, 0, st_dev->solution_y, 0, 0);

    input_set_abs_params(st_dev->input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(st_dev->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);

#if MULTI_PROTOCOL_TYPE_A == MULTI_PROTOCOL_TYPE
    input_set_abs_params(st_dev->input, ABS_MT_TRACKING_ID, 0, 255, 0, 0);
#elif MULTI_PROTOCOL_TYPE_B == MULTI_PROTOCOL_TYPE
    input_mt_init_slots(st_dev->input, SEMI_TOUCH_MAX_POINTS, INPUT_MT_DIRECT /*1*/);
#endif

    ret = input_register_device(st_dev->input);
    check_return_if_fail(ret, NULL);
    /*BSP:add by longtao.ran for bringup 20230411 start*/
    chipsemi_controller.input_dev = st_dev->input;
    /*BSP:add by longtao.ran for bringup 20230411 end*/
    return 0;
}

static int input_device_deinit(void)
{
    int ret = 0;

    ret = semi_touch_hall_exit();

    if(st_dev.input)
    {
        input_unregister_device(st_dev.input);
    }

    return ret;
}

bool semi_touch_vkey_handled(bool pointed, unsigned int x, unsigned int y)
{
#if SEMI_TOUCH_VKEY_MAPPING
    int key = 0;
    for(key = 0; key < st_dev.stc.vkey_num; key++)
    {
        //kernel_log_d("x = %d, y = %d, key_x = %d, key_y = %d\n", x, y, st_dev.stc.vkey_dim_map[key][0], st_dev.stc.vkey_dim_map[key][1]);
        if(x == st_dev.stc.vkey_dim_map[key][0] && y == st_dev.stc.vkey_dim_map[key][1])
        {
            //kernel_log_d("Point[%d] DOWN: x=%d, y=%d", pointed, x, y);
            input_report_key(st_dev.input, st_dev.stc.vkey_evt_arr[key], pointed ? 1 : 0);
            return true;
        }
    }
#endif

    return false;
}
int semi_touch_cmd_handle(rpt_flash_t *point)
{
    int ret = 0;
    int i, count;
    unsigned int addr;
    unsigned short *tmp_buf = NULL;
    unsigned short ckecksum = 0;
    unsigned short match = 1;
    s64 time;

    static unsigned short cnt_last = 0;
    if (0xF2 == point->act && 0 == point->num && point->cmdNeg == (unsigned char)(0-point->cmd))
    {
        if ( (point->OutputIrqCntNeg == (unsigned short)(0-point->OutputIrqCnt)) &&
             (point->cntNeg == (unsigned short)(0-point->cnt)))
        {
            if (st_dev.cnt_last_int == 1)
            {
                if (cnt_last == point->cnt)
                {
                    return 0;
                }
            }
            else
            {
                st_dev.cnt_last_int = 1;
            }

            cnt_last = point->cnt;
        }
        if (1 == point->cmd && 0xA55A == point->d0 && 0x3789 == point->d1)
        {
            kernel_log_d("enter cmd1, get password and time\r\n");
            ret = semi_touch_send_password_rtime();
        }

        else if (2 == point->cmd)
        {
            kernel_log_d("enter cmd2, only get time\r\n");
            ret = semi_touch_send_rtime();
        }

        else if (3 == point->cmd && ((point->d0 & 0xff) < CMD_BUFF_MAX))
        {
            kernel_log_d("enter cmd3, set value\r\n");
            i = point->d0 & 0xff;
            count = (point->d0 & 0xff00) >> 8;
            if (i >= 0 && count > 0 && count <= 4 && ((i + count) <= CMD_BUFF_MAX))
            {
                memcpy((unsigned char *)&st_dev.cmd_buff[i], (unsigned char *)&point->d1, count << 1);
                ret = semi_touch_send_value(point->d0);
            }
        }

        else if (4 == point->cmd)
        {
            kernel_log_d("enter cmd4, get value\r\n");
            i = point->d0 & 0xff;
            count = (point->d0 & 0xff00) >> 8;
            if (i < CMD_BUFF_MAX && count <= 4)
            {
                ret = semi_touch_send_value(point->d0);
            }
        }

        else if (5 == point->cmd)
        {
            kernel_log_d("enter cmd5, set value\r\n");
            match = caculate_checksum_u16(&point->d0, 12);
            if (0 == match && point->d3 < 0x4000 && point->d3 == (0xFFFF - point->d4))
            {
                tmp_buf = kzalloc(CMD_TXRX_BUFF_MAX*2, GFP_KERNEL);
                if (!tmp_buf)
                {
                    kernel_log_e("alloc tmp_buf failed \r\n");
                    goto exit;
                }
                addr = TP_CMD_BUFF_ADDR + point->d3;
                if (point->d0 >= 0 && point->d0 < CMD_TXRX_BUFF_MAX && point->d1 > 0 &&
                    (point->d0 + point->d1) <= CMD_TXRX_BUFF_MAX)
                {
                    i = point->d0;
                    ret = semi_touch_read_bytes(addr, (unsigned char *)&tmp_buf[i], (point->d1 << 1));
                    if (point->d2 == caculate_checksum_u16(&tmp_buf[i], (point->d1 << 1)) && 0 == ret)
                    {
                        memcpy((unsigned char *)&st_dev.cmd_txrx_buff[i], (unsigned char *)&tmp_buf[i],
                               (point->d1 << 1));
                        time = semi_touch_get_rtime();
                        ret = semi_touch_send_cmd_status(0x12, i, point->d1, point->d2, time);
                    }
                    else
                    {
                        kernel_log_e("checksum miss match, faild to setData!\r\n");
                        goto exit;
                    }
                }
                else
                {
                    kernel_log_e("May cross the line, faild to setData!\r\n");
                    goto exit;
                }
            }
            else
            {
                kernel_log_e("cmd5 miss match, faild to setData!\r\n");
                goto exit;
            }
        }

        else if (6 == point->cmd)
        {
            kernel_log_d("enter cmd6, get value\r\n");
            match = caculate_checksum_u16(&point->d0, 12);
            if (match == 0 && point->d3 < 0x4000 && point->d3 == (0xFFFF - point->d4))
            {
                addr = TP_CMD_BUFF_ADDR + point->d3;
                if (point->d0 >= 0 && point->d0 < CMD_TXRX_BUFF_MAX && point->d1 > 0 &&
                    (point->d0 + point->d1) <= CMD_TXRX_BUFF_MAX)
                {
                    i = point->d0;
                    ckecksum = caculate_checksum_u16(&st_dev.cmd_txrx_buff[i], (point->d1 << 1));
                    if (0 == semi_touch_write_bytes(addr, (unsigned char *)&st_dev.cmd_txrx_buff[i],
                                                    (point->d1 << 1)))
                    {
                        time = semi_touch_get_rtime();
                        ret = semi_touch_send_cmd_status(0x11, i, point->d1, ckecksum, time);
                    }
                    else
                    {
                        kernel_log_e("write to tp error, faild to getData!\r\n");
                        goto exit;
                    }
                }
                else
                {
                    kernel_log_e("May cross the line, faild to getData!\r\n");
                    goto exit;
                }
            }
            else
            {
                kernel_log_e("cmd6 miss match, faild to getData!\r\n");
                goto exit;
            }
        }

        else if (7 ==point->cmd)
        {
            kernel_log_d("enter cmd7, clear value\r\n");
            match = caculate_checksum_u16(&point->d0, 12);
            if (0 == match)
            {
                if (point->d0 >= 0 && point->d0 < CMD_TXRX_BUFF_MAX && point->d1 > 0 &&
                    (point->d0 + point->d1) <= CMD_TXRX_BUFF_MAX)
                {
                    i = point->d0;
                    memset((unsigned char *)&st_dev.cmd_txrx_buff[i], 0, (point->d1 << 1));
                    time = semi_touch_get_rtime();
                    ret = semi_touch_send_cmd_status(0x13, point->d0, point->d1, 0, time);
                }
                else
                {
                    kernel_log_e("May cross the line,, faild to clear value!\r\n");
                    goto exit;
                }
            }
            else
            {
                kernel_log_e("checksum miss match, faild to clear value!\r\n");
                goto exit;
            }
        }
    }

    if (tmp_buf)
        kfree(tmp_buf);

    return ret;

exit:
    time = 0;
    time = semi_touch_get_rtime();

    if (5 == point->cmd)
    {
        ret = semi_touch_send_cmd_status(0x12, 0xffff, point->d1, point->d2, time);
    }
    else if (6 == point->cmd)
    {
        ret = semi_touch_send_cmd_status(0x11, 0xffff, point->d1, point->d2, time);
    }
    else if (7 == point->cmd)
    {
        ret = semi_touch_send_cmd_status(0x13, 0xffff, point->d1, point->d2, time);
    }

    if (tmp_buf)
        kfree(tmp_buf);

    return ret;
}
unsigned int mysqrt(unsigned int a) //max value is 5004841
{
   unsigned int x;
    int i = 20;
    if(!a) return 0;
    x = a>>1;
    while(i--)
    {
        x=(x+a/x)>>1;
    }

    return x;
}

/*USD:modify for transsion core by yun.luo 20241225 start*/
static int tran_pocket_state_report(void)
{
    int ret = 0;
    unsigned int regdata = 0xff;
    static bool palm_status = false;
    if (is_palm_mode_function_en(st_dev.stc.custom_function_en)\
        && is_palm_mode_activate(st_dev.stc.ctp_run_status)\
        && st_dev.pocket_func_support) {
        ret = semi_touch_read_bytes(0x200008D0, (unsigned char *)&regdata, 4);//read palm status from register,1-open,0-close
        if (ret != 0){
            kernel_log_d("read palm failed,ret=%d \n", ret);
            return ret;
        }
        regdata = (regdata & 0xff00) >> 8;
        //kernel_log_d("read palm status: %d\n", regdata);
        if ((1 == regdata) && (!palm_status)) {
                input_report_key(st_dev.input, KEY_LARGE_AREA, 1);
                input_sync(st_dev.input);
                palm_status = true;
                kernel_log_d("Large area touch down");
                return 0;
        } else if ((0 == regdata) && (palm_status)) {
                input_report_key(st_dev.input, KEY_LARGE_AREA, 0);
                input_sync(st_dev.input);
                palm_status = false;
                kernel_log_d("Large area touch up");
        }
        //return 0;
    }
    return 1;
}
/*USD:modify for transsion core by yun.luo 20241225 end*/

int touch_point_to_input(unsigned char *readbuffer, unsigned short thp_cmd_gesture_flg)
{
    bool pointed = 0;
    int  index = 0, pointNum = 0;
    union rpt_point_t *ppt;
    rpt_flash_t *ppt2;

    unsigned int tmpbuff_x = 0;
    unsigned int tmpbuff_y = 0;
    static unsigned int tmpbuff_x1 = 0;
    static unsigned int tmpbuff_y1 = 0;
    unsigned int distance = 1000;
    static unsigned char count  = 0;
    static unsigned char count2 = 0;
    static unsigned char fp_id = 0xff;
    /*USD:modify for transsion core by yun.luo 20241225 start*/
    static unsigned int touch_down = 0;
    int boot_mode = chipsemi_controller.tran_ts_data->boot_mode;
    /*USD:modify for transsion core by yun.luo 20241225 end*/

    semi_touch_watch_dog_feed(st_dev.stc.dog_feed_flag);

    if (st_dev.gest_flag == 1 &&
        0xA5 != readbuffer[SEMI_TOUCH_MAX_POINTS * 6 + 4] &&
        0xFE != readbuffer[SEMI_TOUCH_MAX_POINTS * 6 + 5])
    {
        if (count2 < 1)
        {
            semi_touch_guesture_switch(G_GESTURE_BACK, 1);
        }
        if (12 == (count2++))
            count2 = 0;
        return 0;
    }

    pointNum = (readbuffer[1] & 0x0f);
    ppt = (union rpt_point_t *)&readbuffer[2];

    if (0xF2 == readbuffer[0])
    {
        ppt2 = (struct _rpt_flash_t*) &readbuffer[0];
        semi_touch_cmd_handle(ppt2);
        return 0;
    }
    else if (0xFC == readbuffer[0] && semi_touch_proximity_report(readbuffer[1]))
    {
        return 0;
    }
    else if (0xA5 == readbuffer[SEMI_TOUCH_MAX_POINTS * 6 + 4] &&
               0xFE == readbuffer[SEMI_TOUCH_MAX_POINTS * 6 + 5] &&
             semi_touch_gesture_report(readbuffer[1]))
    {
        return 0;
    }

#if SEMI_TOUCH_THP_DRIVER_EN
    if ((thp_cmd_gesture_flg == 1)&& !(is_guesture_activate(st_dev.stc.ctp_run_status)))
    {
        return 0;
    }
#endif

#if SEMI_TOUCH_THP_DRIVER_EN
    input_lock();
#endif

    if (0xF8 == readbuffer[0])
    {
#if MULTI_PROTOCOL_TYPE_A == MULTI_PROTOCOL_TYPE
        for (index = 0; index < SEMI_TOUCH_MAX_POINTS; index++)
        {
            //EVENT_UP = 0x04
            pointed = (0x04 == ppt->rp.event) ? false : true;

            tmpbuff_x = (unsigned int)(ppt->rp.x_h8 << 8) | ppt->rp.x_l8;
            tmpbuff_y = (unsigned int)(ppt->rp.y_h8 << 8) | ppt->rp.y_l8;

            /*USD:modify for transsion core by yun.luo 20241225 start*/
            if (st_dev.multiple_solution > 1) {
                tmpbuff_x1 = tmpbuff_x / st_dev.multiple_solution;
                tmpbuff_y1 = tmpbuff_y / st_dev.multiple_solution;
            } else {
                tmpbuff_x1 = tmpbuff_x;
                tmpbuff_y1 = tmpbuff_y;
            }
            if(boot_mode == TRAN_FACTORY_MODE) {
                tmpbuff_x = tmpbuff_x1;
                tmpbuff_y = tmpbuff_y1;
            }
            /*USD:modify for transsion core by yun.luo 20241225 end*/

            distance = mysqrt((tmpbuff_x1 - st_dev.fp_x_center) * (tmpbuff_x1 - st_dev.fp_x_center) + \
                        (tmpbuff_y1 - st_dev.fp_y_center) * (tmpbuff_y1 - st_dev.fp_y_center));//sqrt()

            if (ppt->rp.id != 0x0f)
            {
                if (semi_touch_vkey_handled(pointed,
                                            (unsigned int)(ppt->rp.x_h8 << 8) | ppt->rp.x_l8,
                                            (unsigned int)(ppt->rp.y_h8 << 8) | ppt->rp.y_l8))
                {
                }
                else
                {
                if ((distance < st_dev.fp_radius) && (pointed) && (count < 1))
                    {
                        //kernel_log_d("enter distance= (%d)\n", distance);
                        input_report_key(st_dev.input, FP_KEY_VALUE, 1);
                        count += 1;
                        fp_id = ppt->rp.id;
                        input_sync(st_dev.input);
                    }

                    if (pointed)
                    {
                        input_report_key(st_dev.input, BTN_TOUCH, 1);
                        input_report_abs(st_dev.input, ABS_MT_POSITION_X, tmpbuff_x);
                        input_report_abs(st_dev.input, ABS_MT_POSITION_Y, tmpbuff_y);
                        input_report_abs(st_dev.input, ABS_MT_TOUCH_MAJOR, ppt->rp.z);
                        input_report_abs(st_dev.input, ABS_MT_WIDTH_MAJOR, ppt->rp.z);
                        input_report_abs(st_dev.input, ABS_MT_TRACKING_ID, ppt->rp.id);
                        input_mt_sync(st_dev.input);
                    }
                    else
                    {
                        if ((count) && (fp_id != 0xff) && (fp_id == ppt->rp.id))
                        {
                            //kernel_log_d("leave count= (%d)\n", count);
                            input_report_key(st_dev.input, FP_KEY_VALUE, 0);
                            count = 0;
                            fp_id = 0xff;
                            //input_mt_sync(st_dev.input);
                            input_sync(st_dev.input);
                        }
                    }
                }
            }
            ppt++;
        }

#elif MULTI_PROTOCOL_TYPE_B == MULTI_PROTOCOL_TYPE
        for (index = 0; index < SEMI_TOUCH_MAX_POINTS; index++)
        {
            //EVENT_UP = 0x04
            pointed = (0x04 == ppt->rp.event) ? false : true;

            tmpbuff_x = (unsigned int)(ppt->rp.x_h8 << 8) | ppt->rp.x_l8;
            tmpbuff_y = (unsigned int)(ppt->rp.y_h8 << 8) | ppt->rp.y_l8;

            /*USD:modify for transsion core by yun.luo 20241225 start*/
            if (st_dev.multiple_solution > 1) {
                tmpbuff_x1 = tmpbuff_x / st_dev.multiple_solution;
                tmpbuff_y1 = tmpbuff_y / st_dev.multiple_solution;
            } else {
                tmpbuff_x1 = tmpbuff_x;
                tmpbuff_y1 = tmpbuff_y;
            }
            if(boot_mode == TRAN_FACTORY_MODE) {
                tmpbuff_x = tmpbuff_x1;
                tmpbuff_y = tmpbuff_y1;
            }
            /*USD:modify for transsion core by yun.luo 20241225 end*/

            distance = mysqrt((tmpbuff_x1 - st_dev.fp_x_center) * (tmpbuff_x1 - st_dev.fp_x_center) + \
                    (tmpbuff_y1 - st_dev.fp_y_center) * (tmpbuff_y1 - st_dev.fp_y_center));//sqrt()

            if (ppt->rp.id != 0x0f)
            {

                if (semi_touch_vkey_handled(pointed,
                                            (unsigned int)(ppt->rp.x_h8 << 8) | ppt->rp.x_l8,
                                            (unsigned int)(ppt->rp.y_h8 << 8) | ppt->rp.y_l8))
                {

                }
                else
                {
                    if ((distance < st_dev.fp_radius) && (pointed) && (count < 1) && st_dev.tran_fingerprint_status)
                    {
                        kernel_log_d("fod finger down\n");
                        input_report_key(st_dev.input, FP_KEY_VALUE, 1);
                        count += 1;
                        fp_id = ppt->rp.id;
                        input_sync(st_dev.input);
                    }

                    if (pointed)
                    {
                        /*USD:modify for transsion core by yun.luo 20241225 start*/
                        if (touch_down == 0) {
                            kernel_log_d("Down point%d= (%d,%d)\n",ppt->rp.id, tmpbuff_x1, tmpbuff_y1);
                            touch_down++;
                        }
                        /*USD:modify for transsion core by yun.luo 20241225 end*/
                        input_mt_slot(st_dev.input, ppt->rp.id);
                        input_mt_report_slot_state(st_dev.input, MT_TOOL_FINGER, pointed);
                        input_report_abs(st_dev.input, ABS_MT_POSITION_X, tmpbuff_x);
                        input_report_abs(st_dev.input, ABS_MT_POSITION_Y, tmpbuff_y);
                        input_report_abs(st_dev.input, ABS_MT_TOUCH_MAJOR, ppt->rp.z);
                        input_report_abs(st_dev.input, ABS_MT_WIDTH_MAJOR, ppt->rp.z);
                        input_report_key(st_dev.input, BTN_TOUCH, 1);
                    }
                    else
                    {
                        if ((count) && (fp_id != 0xff) && (fp_id == ppt->rp.id))
                        {
                            kernel_log_d("fod finger up\n");
                            input_report_key(st_dev.input, FP_KEY_VALUE, 0);
                            count = 0;
                            fp_id = 0xff;
                            input_sync(st_dev.input);
                        }
                            /*USD:modify for transsion core by yun.luo 20241225 end*/
                            touch_down = 0;
                            kernel_log_d("Up point%d= (%d,%d)\n",ppt->rp.id, tmpbuff_x1, tmpbuff_y1);
                            /*USD:modify for transsion core by yun.luo 20241225 end*/
                            input_mt_slot(st_dev.input, ppt->rp.id);
                            input_mt_report_slot_state(st_dev.input, MT_TOOL_FINGER, pointed);

                    }
                }
            }
            ppt++;
        }
#endif
    }

#if SEMI_TOUCH_THP_DRIVER_EN
    if ((thp_cmd_gesture_flg == 1) && !(is_guesture_activate(st_dev.stc.ctp_run_status)))
    {
        input_unlock();
        return 0;
    }
#endif

    if (0 == pointNum)
    {
        semi_touch_clear_report();
    }
    else
    {
        input_sync(st_dev.input);
    }

#if SEMI_TOUCH_THP_DRIVER_EN
    input_unlock();
#endif

    return 0;
}

static irqreturn_t semi_touch_irq_handler_imp(int irq, void *p)
{
    const unsigned char report_size = ((SEMI_TOUCH_MAX_POINTS * 6 + 2) + 8) & 0xfc;//36-5points,68-10points
    unsigned char readbuffer[84];
    int ret = 0;

#if SEMI_TOUCH_THP_DRIVER_EN
    static int drive_frame_cnt  = 0;
    drive_frame_cnt++;
#endif

    if (!ack_pointing_action(st_dev.stc.ctp_run_status)) return IRQ_RETVAL(IRQ_HANDLED);

    /*USD:modify for transsion core by yun.luo 20241225 start*/
    ret = tran_pocket_state_report();
    if (ret == 0) {
        return IRQ_RETVAL(IRQ_HANDLED);
    }
    /*USD:modify for transsion core by yun.luo 20241225 end*/

#if SEMI_TOUCH_THP_DRIVER_EN
    if (semi_thp_dev.thp_en != 1)
#endif
    {
        if (read_and_report_touch_points(readbuffer, report_size) >= 0)
        {

            touch_point_to_input(readbuffer, 0);
        }
        return IRQ_RETVAL(IRQ_HANDLED);
    }

#if SEMI_TOUCH_THP_DRIVER_EN
    semi_thp_irq_handler(drive_frame_cnt);
#endif

    return IRQ_RETVAL(IRQ_HANDLED);

}

#if SEMI_TOUCH_IRQ_VAR_QUEUE
static void semi_touch_irq_work_fun(struct work_struct *work)
{
    semi_touch_irq_handler_imp(st_dev.client->irq, &st_dev);
}

static irqreturn_t semi_touch_irq_handler(int irq, void *p)
{
    if(SEMI_DRV_ERR_OK == semi_touch_wake_lock())
    {
        semi_touch_queue_asyn_work(work_queue_interrupt, semi_touch_irq_work_fun, 0);
    }

    return IRQ_RETVAL(IRQ_HANDLED);
}
#endif

irqreturn_t semi_touch_clear_report(void)
{
#if MULTI_PROTOCOL_TYPE_A == MULTI_PROTOCOL_TYPE
    input_report_key(st_dev.input, BTN_TOUCH, 0);
    input_mt_sync(st_dev.input);
    input_sync(st_dev.input);
#elif MULTI_PROTOCOL_TYPE_B == MULTI_PROTOCOL_TYPE
    int index = 0;
    for(index = 0; index < SEMI_TOUCH_MAX_POINTS; index++)
    {
        input_mt_slot(st_dev.input, index);
        input_mt_report_slot_state(st_dev.input, MT_TOOL_FINGER, false);

    }
    input_report_key(st_dev.input, BTN_TOUCH, 0);
    input_sync(st_dev.input);
#endif

    return IRQ_RETVAL(IRQ_HANDLED);
}

static int semi_touch_irq_init(struct sm_touch_dev *st_dev)
{
    int ret = 0;
    struct hal_device *client = st_dev->client;

    client->irq = semi_touch_get_irq(st_dev->int_pin);
    check_return_if_fail(client->irq, NULL);

#if SEMI_TOUCH_IRQ_VAR_QUEUE
    semi_touch_create_work_queue(work_queue_interrupt, typename(work_queue_interrupt));
    ret = devm_request_irq(&client->dev, client->irq, semi_touch_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND, CHSC_DEVICE_NAME, st_dev);
#else
    ret = devm_request_threaded_irq(&client->dev, client->irq, NULL, semi_touch_irq_handler_imp, IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND, CHSC_DEVICE_NAME, st_dev);
#endif
    check_return_if_fail(ret, NULL);

    return 0;
}

#if 0
int semi_touch_resolution_adaption(struct sm_touch_dev *st_dev)
{
    int ret = -SEMI_DRV_ERR_HAL_IO;
    unsigned char readbuffer[256] = {0};
    unsigned short index, pix_x, pix_y;
    const int vkey_evt[] = SEMI_TOUCH_KEY_EVT;

    ret = semi_touch_read_bytes(0x20000080, readbuffer, sizeof(readbuffer));
    check_return_if_fail(ret, NULL);

    //xy switch
    if(readbuffer[0x0f] & 0x02)
    {
        pix_x = (unsigned short)((readbuffer[0x09] << 8) + readbuffer[0x08]);
        pix_y = (unsigned short)((readbuffer[0x07] << 8) + readbuffer[0x06]);
    }
    else
    {
        pix_x = (unsigned short)((readbuffer[0x07] << 8) + readbuffer[0x06]);
        pix_y = (unsigned short)((readbuffer[0x09] << 8) + readbuffer[0x08]);
    }

    input_set_abs_params(st_dev->input, ABS_MT_POSITION_X, 0, SEMI_TOUCH_SOLUTION_X, 0, 0);
    input_set_abs_params(st_dev->input, ABS_MT_POSITION_Y, 0, SEMI_TOUCH_SOLUTION_Y, 0, 0);

    kernel_log_d("resolution = (%d, %d)\n", pix_x, pix_y);

    st_dev->stc.vkey_num = readbuffer[0x52];
    memcpy(st_dev->stc.vkey_evt_arr, vkey_evt, sizeof(int) * MAX_VKEY_NUMBER);
    //key xy switch
    for(index = 0; (index < st_dev->stc.vkey_num) && (index < MAX_VKEY_NUMBER); index++)
    {
        if(readbuffer[0x53] & 0x02)
        {
            pix_x = (readbuffer[0x55] << 8) + readbuffer[0x54];
            pix_y = st_dev->stc.vkey_num > 5 ? (index + 1) : (readbuffer[0x57 + index * 2] << 8) + readbuffer[0x56 + index * 2];
        }
        else
        {
            pix_y = (readbuffer[0x55] << 8) + readbuffer[0x54];
            pix_x = st_dev->stc.vkey_num > 5 ? (index + 1) : (readbuffer[0x57 + index * 2] << 8) + readbuffer[0x56 + index * 2];
        }

        st_dev->stc.vkey_dim_map[index][0] = pix_x;
        st_dev->stc.vkey_dim_map[index][1] = pix_y;
        st_dev->stc.vkey_dim_map[index][2] = 10;
        st_dev->stc.vkey_dim_map[index][3] = 10;

        set_bit(st_dev->stc.vkey_evt_arr[index], st_dev->input->keybit);
        input_set_capability(st_dev->input, EV_KEY, st_dev->stc.vkey_evt_arr[index]);

        kernel_log_d("vkey index = %d, xy = (%d, %d), event = %d\n", index, pix_x, pix_y, st_dev->stc.vkey_evt_arr[index]);
    }

    return ret;
}
#endif
/*USD:modify for transsion core by yun.luo 20241225 start*/
static void semi_touch_parse_dt(struct sm_touch_dev *st_dev)
{
    int ret = 0;
    struct hal_device *client = st_dev->client;
    struct device_node *np = client->dev.of_node;
    u32 temp_val = 0;

    ret = of_property_read_u32(np, "chipsemi,x-coords", &temp_val);
    if (ret < 0) {
        kernel_log_e("Solution Coords undefined! use default:1080 \n");
        st_dev->solution_x = SEMI_TOUCH_SOLUTION_X;
    } else {
        st_dev->solution_x = temp_val;
    }

    ret = of_property_read_u32(np, "chipsemi,y-coords", &temp_val);
    if (ret < 0) {
        kernel_log_e("Solution Coords undefined! use default:2436 \n");
        st_dev->solution_y = SEMI_TOUCH_SOLUTION_Y;
    } else {
        st_dev->solution_y = temp_val;
    }

    ret = of_property_read_u32(np, "chipsemi,multiple-solution", &temp_val);
    if (ret < 0) {
        kernel_log_e("Unable to get multiple-solution, use default value:1 \n");
        st_dev->multiple_solution = 1;
    } else {
        st_dev->multiple_solution = temp_val;
    }

    st_dev->fod_func_support = of_property_read_bool(np, "chipsemi,fod-function-support");
    if (st_dev->fod_func_support) {
        st_dev->fod_func_support = true;
    } else {
        st_dev->fod_func_support = false;
    }

    st_dev->switch_report_support = of_property_read_bool(np, "chipsemi,switch-report-support");
    if (st_dev->switch_report_support) {
        st_dev->switch_report_support = true;
    } else {
        st_dev->switch_report_support = false;
    }

    st_dev->pocket_func_support = of_property_read_bool(np, "chipsemi,pocket-function-support");
    if (st_dev->pocket_func_support) {
        st_dev->pocket_func_support = true;
    } else {
        st_dev->pocket_func_support = false;
    }

    if (st_dev->fod_func_support) {
        ret = of_property_read_u32(np, "chipsemi,fp_x_center", &st_dev->fp_x_center);
        if (ret) {
            kernel_log_e("get chipsemi,fp_x_center faild! \n");
		    st_dev->fp_x_center = FP_X_CENTER;
		    st_dev->fp_y_center = FP_Y_CENTER;
		    st_dev->fp_radius   = FP_RADIUS;

		    st_dev->rawdata_row = FRAME_RX;
		    st_dev->rawdata_col = FRAME_TX;
			goto touch_factory_test;
        }

        ret = of_property_read_u32(np, "chipsemi,fp_y_center", &st_dev->fp_y_center);
        if (ret) {
            kernel_log_e("get chipsemi,fp_y_center faild! \n");
            //return ret;
        }

        ret = of_property_read_u32(np, "chipsemi,fp_radius", &st_dev->fp_radius);
        if (ret) {
            kernel_log_e("get chipsemi,fp_radius faild! \n");
            //return ret;
        }
touch_factory_test:
        kernel_log_d("Get fp_x_center:%d, fp_y_center:%d, fp_radius:%d \n",
            st_dev->fp_x_center, st_dev->fp_y_center, st_dev->fp_radius);
    }

    kernel_log_d("Get display-coords:(%d , %d), multiple-solution:%d \n",
        st_dev->solution_x, st_dev->solution_y, st_dev->multiple_solution);

    kernel_log_d("Get fod-function:%d, switch-report:%d, pocket-function:%d \n",
        st_dev->fod_func_support, st_dev->switch_report_support, st_dev->pocket_func_support);

}
/*USD:modify for transsion core by yun.luo 20241225 end*/

int semi_touch_init(void *hal)
{
    int ret = 0;
    memset(&st_dev, 0, sizeof(st_dev));

    st_dev.int_pin = semi_touch_get_int();
    check_return_if_fail(st_dev.int_pin, NULL);
    st_dev.rst_pin = semi_touch_get_rst();
    check_return_if_fail(st_dev.rst_pin, NULL);
    st_dev.vdd_pin = semi_touch_get_vdd();
    check_return_if_fail(st_dev.vdd_pin, NULL);
    st_dev.vddio_pin = semi_touch_get_vddio();
    ret = semi_touch_get_info();
    check_return_if_fail(ret, NULL);
     //USD:Modify by yun.luo 20241212 start
    //check_return_if_fail(st_dev.vddio_pin, NULL);

    semi_io_direction_out(st_dev.vdd_pin, 1);
    msleep(5);
    if (st_dev.vddio_pin > 0) {
        semi_io_direction_out(st_dev.vddio_pin, 1);
    }
    //USD:Modify by yun.luo 20241212 end
    msleep(5);
    semi_io_direction_in(st_dev.int_pin);
    semi_io_direction_out(st_dev.rst_pin, 1);

    mutex_init(&st_dev.hal.bus_lock);
    mutex_init(&st_dev.custom_lock);

    ret = semi_touch_hall_init(hal);
    check_return_if_fail(ret, NULL);
    /*USD:modify for transsion core by yun.luo 20241225 start*/
    semi_touch_parse_dt(&st_dev);
    /*USD:modify for transsion core by yun.luo 20241225 end*/

#if SEMI_TOUCH_DMA_TRANSFER
    ret = semi_touch_hall_init_buff(&st_dev);
    check_return_if_fail(ret, NULL);
#endif

    ret = semi_touch_device_prob();
    check_return_if_fail(ret, NULL);

    ret = input_device_init(&st_dev);
    check_return_if_fail(ret, NULL);

    ret = semi_touch_create_apk_proc(&st_dev);
    check_return_if_fail(ret, NULL);

    ret = semi_touch_fw_update_check(&st_dev);
    check_return_if_fail(ret, NULL);

    ret = semi_touch_bootup_update_check();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_mode_init(&st_dev);
    check_return_if_fail(ret, NULL);

    //ret = semi_touch_resolution_adaption(&st_dev);
    //check_return_if_fail(ret, NULL);

#if SEMI_TOUCH_THP_DRIVER_EN
    ret = semi_thp_driver_init(&st_dev);
    check_return_if_fail(ret, NULL);
#endif

    ret = semi_touch_irq_init(&st_dev);
    check_return_if_fail(ret, NULL);

    ret = semi_touch_custom_work(&st_dev);
    check_return_if_fail(ret, NULL);
    //USD:Modify by yun.luo 20241212 start
    //ret = semi_touch_work_done();
    //check_return_if_fail(ret, NULL);
    //USD:Modify by yun.luo 20241212 end
    return ret;
}

int semi_touch_deinit(void *hal)
{
    int ret = 0;

    ret = semi_touch_custom_clean_up();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_destroy_work_queue();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_remove_apk_proc(&st_dev);
    check_return_if_fail(ret, NULL);

    input_device_deinit();
    //USD:Modify by yun.luo 20241212 start
    //semi_touch_resource_release();
    //USD:Modify by yun.luo 20241212 end
#if SEMI_TOUCH_THP_DRIVER_EN
    semi_thp_driver_exit();
#endif
    return ret;
}
