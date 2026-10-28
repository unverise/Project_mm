
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/timekeeping.h>
#include <linux/version.h>
#include "semi_touch_interface.h"

unsigned char H_V_CODE = 0;
unsigned short G_GESTURE_MARK = 0xA5;
unsigned short G_GESTURE_BACK = 0xA5;
unsigned int G_GAME_BACK = 0x00040404;

enum HAL_IO_DIR { HAL_IO_WRITE, HAL_IO_READ, };

int semi_touch_reset(enum reset_action action)
{
    int ret = 0, self_check = 0;
    //struct i2c_client *client = st_dev.client;

    //ptr_cmd = NULL;
    set_status_ready_upgrade(st_dev.stc.ctp_run_status);
    //disable_irq(client->irq);

    semi_io_pin_low(st_dev.rst_pin);
    msleep(10);
    semi_io_pin_high(st_dev.rst_pin);

    mdelay(5);
    semi_touch_write_bytes(0x20000018, (unsigned char *)&self_check, 4);

    if(do_report_after_reset == action)
    {
        set_status_pointing(st_dev.stc.ctp_run_status);
        //enable_irq(client->irq);
    }

    return ret;
}

s64 semi_touch_get_rtime(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
#else
    struct timeval ts;
    do_gettimeofday(&ts);
#endif
    kernel_log_d("get the numbers of seconds are %lld\r\n",ts.tv_sec);//or lld%
    return ts.tv_sec;

}

void semi_touch_custom_cmd_delay(void)
{
    int index;
    for (index = 0; index < 30; index++)
    {
        if (st_dev.cmd_flag == 0)
        {
            break;
        }
        msleep(1);
    }
}

int semi_touch_suspend_ctrl(unsigned char en)
{
    int ret = 0;
    struct m_ctp_cmd_std_t cmd_send_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));
    //struct m_ctp_rsp_std_t ack_from_tp;

    if(en)
    {
        cmd_send_tp.id = CMD_CTP_IOCTL;
        cmd_send_tp.d0 = 0x05;
        cmd_send_tp.d1 = en;
        semi_touch_custom_cmd_delay();

        ret = cmd_send_to_tp_no_check(&cmd_send_tp);
        check_return_if_fail(ret, NULL);
    }
    else
    {
        semi_touch_reset_and_detect();
    }

    if(en)
        enter_suspend_gate(st_dev.stc.ctp_run_status);
    else
        leave_suspend_gate(st_dev.stc.ctp_run_status);

    return ret;
}

int semi_touch_proximity_switch(unsigned char en)
{
    int ret = 0;

    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    if(!is_proximity_function_en(st_dev.stc.custom_function_en))
        return ret;

    cmd_send_tp.id = CMD_CTP_IOCTL;
    cmd_send_tp.d0 = 0x11;
    cmd_send_tp.d1 = en;

    semi_touch_custom_cmd_delay();
    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    if(en)
        enter_proximity_gate(st_dev.stc.ctp_run_status);
    else
        leave_proximity_gate(st_dev.stc.ctp_run_status);

    return ret;
}

int semi_touch_glove_switch(unsigned char en)
{
    int ret = 0;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    if(!is_glove_function_en(st_dev.stc.custom_function_en))
        return ret;

    cmd_send_tp.id = CMD_CTP_IOCTL;
    cmd_send_tp.d0 = 0x10;
    cmd_send_tp.d1 = en;
    semi_touch_custom_cmd_delay();

    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    if(en)
        enter_glove_gate(st_dev.stc.ctp_run_status);
    else
        leave_glove_gate(st_dev.stc.ctp_run_status);

    return ret;
}

int semi_touch_guesture_switch(unsigned short mark, unsigned char en)
{
    int ret = 0;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    if(!is_guesture_function_en(st_dev.stc.custom_function_en))
        return ret;

    cmd_send_tp.id = CMD_CTP_IOCTL;
    cmd_send_tp.d0 = 0x02;
    cmd_send_tp.d1 = en;
    cmd_send_tp.d2 = mark;
    G_GESTURE_BACK = mark;  //Backup instruction
    semi_touch_custom_cmd_delay();

    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    if(en)
        enter_guesture_gate(st_dev.stc.ctp_run_status);
    else
        leave_guesture_gate(st_dev.stc.ctp_run_status);

    return ret;
}

int semi_touch_orientation_switch(unsigned char en)
{
    int ret = 0;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    cmd_send_tp.id = CMD_CTP_IOCTL;
    cmd_send_tp.d0 = 0x2A;
    cmd_send_tp.d1 = en;

    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    if(en)
        enter_orientation_gate(st_dev.stc.ctp_run_status);
    else
        leave_orientation_gate(st_dev.stc.ctp_run_status);

    return ret;
}

int semi_touch_wet_finger_switch(unsigned char en)
{
    int ret = 0;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    if(!is_wet_finger_function_en(st_dev.stc.custom_function_en))
        return ret;

    cmd_send_tp.id = CMD_CTP_IOCTL;
    cmd_send_tp.d0 = 0x12;
    cmd_send_tp.d1 = en;

    semi_touch_custom_cmd_delay();
    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    if(en)
        enter_wet_finger_gate(st_dev.stc.ctp_run_status);
    else
        leave_wet_finger_gate(st_dev.stc.ctp_run_status);

    return ret;
}

int semi_touch_h_v_switch(unsigned char code, unsigned char en)
{
    int ret = 0;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    if(!is_h_v_sw_function_en(st_dev.stc.custom_function_en))
        return ret;

    if(!is_status_pointing(st_dev.stc.ctp_run_status))//upgrading fw
        return ret;

    cmd_send_tp.id = CMD_CTP_IOCTL;
    cmd_send_tp.d0 = code;//0x13, 0x14, 0x15, 0x16
    cmd_send_tp.d1 = en;
    H_V_CODE = code;

    semi_touch_custom_cmd_delay();
    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    if(en)//1-open,0-close
        enter_h_v_sw_gate(st_dev.stc.ctp_run_status);
    else
        leave_h_v_sw_gate(st_dev.stc.ctp_run_status);

    return ret;
}

int semi_touch_charger_switch(unsigned char en)
{
    int ret = 0;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    if(!is_charger_function_en(st_dev.stc.custom_function_en))
        return ret;

    if(!is_status_pointing(st_dev.stc.ctp_run_status))//upgrading fw
        return ret;

    cmd_send_tp.id = CMD_CTP_IOCTL;
    cmd_send_tp.d0 = 0x17;
    cmd_send_tp.d1 = en;

    semi_touch_custom_cmd_delay();
    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    if(en)
        enter_charger_gate(st_dev.stc.ctp_run_status);
    else
        leave_charger_gate(st_dev.stc.ctp_run_status);

    return ret;
}

int semi_touch_game_mode_switch(unsigned int mark, unsigned char en)
{
    int ret = 0;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    if(!is_game_mode_function_en(st_dev.stc.custom_function_en))
        return ret;

    cmd_send_tp.id = CMD_CTP_IOCTL;
    cmd_send_tp.d0 = 0x18;
    cmd_send_tp.d1 = en;
    cmd_send_tp.d2 = mark & 0xFF;//TapSen
    cmd_send_tp.d3 = (mark >> 8) & 0xFF;//SwipeResp
    cmd_send_tp.d4 = (mark >> 16) & 0xFF;//MicCA
    G_GAME_BACK = mark & 0xFFFFFF;
    semi_touch_custom_cmd_delay();

    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    if(en)
        enter_game_mode_gate(st_dev.stc.ctp_run_status);
    else
        leave_game_mode_gate(st_dev.stc.ctp_run_status);

    return ret;
}

int semi_touch_palm_mode_switch(unsigned char en)
{
    int ret = 0;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    if (!is_palm_mode_function_en(st_dev.stc.custom_function_en))
        return ret;
    if (!is_status_pointing(st_dev.stc.ctp_run_status)) //upgrading fw
        return ret;

    cmd_send_tp.id = CMD_CTP_IOCTL;
    cmd_send_tp.d0 = 0x19;
    cmd_send_tp.d1 = en;
    semi_touch_custom_cmd_delay();

    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    if (en)
        enter_palm_mode_gate(st_dev.stc.ctp_run_status);
    else
        leave_palm_mode_gate(st_dev.stc.ctp_run_status);

    return ret;
}

int semi_touch_send_password_rtime(void)
{
    int ret = 0;
    int pass=0xA55AAA55;
    s64 time = 0;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;

    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));
    time = semi_touch_get_rtime();

    cmd_send_tp.id = CMD_CTP_CMD;
    cmd_send_tp.d0 = 0x0d;
    cmd_send_tp.d1 = pass&0xFFFF;
    cmd_send_tp.d2 = (pass>>16)&0xFFFF;
    cmd_send_tp.d3 = time&0xFFFF;
    cmd_send_tp.d4 = (time>>16)&0xFFFF;

    semi_touch_custom_cmd_delay();
    //ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    ret = cmd_send_to_tp_step_100us(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    return ret;
}

int semi_touch_send_rtime(void)
{
    int ret = 0;
    s64 time = 0;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;

    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));
    time = semi_touch_get_rtime();

    cmd_send_tp.id = CMD_CTP_CMD;
    cmd_send_tp.d0 = 0x0e;
    cmd_send_tp.d1 = time&0xFFFF;
    cmd_send_tp.d2 = (time>>16)&0xFFFF;
    semi_touch_custom_cmd_delay();

    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    return ret;
}

int semi_touch_send_value(unsigned short num_index)
{
    int ret = 0;
    int i = 0;

    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));
    i = num_index & 0xff;

    cmd_send_tp.id = CMD_CTP_CMD;
    cmd_send_tp.d0 = 0x0f;
    cmd_send_tp.d1 = num_index;
    if (i <= (CMD_BUFF_MAX - 4))
    {
        cmd_send_tp.d2 = st_dev.cmd_buff[i];
        cmd_send_tp.d3 = st_dev.cmd_buff[i + 1];
        cmd_send_tp.d4 = st_dev.cmd_buff[i + 2];
        cmd_send_tp.d5 = st_dev.cmd_buff[i + 3];
    }
    else if(i <= (CMD_BUFF_MAX - 3))
    {
        cmd_send_tp.d2 = st_dev.cmd_buff[i];
        cmd_send_tp.d3 = st_dev.cmd_buff[i + 1];
        cmd_send_tp.d4 = st_dev.cmd_buff[i + 2];
    }
    else if(i <= (CMD_BUFF_MAX - 2))
    {
        cmd_send_tp.d2 = st_dev.cmd_buff[i];
        cmd_send_tp.d3 = st_dev.cmd_buff[i + 1];
    }
    else
    {
        cmd_send_tp.d2 = st_dev.cmd_buff[i];
    }

    semi_touch_custom_cmd_delay();
    //ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    ret = cmd_send_to_tp_step_100us(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    kernel_log_d("send value succeed\r\n");

    return ret;
}

int semi_touch_send_ioctrl(unsigned char index)
{
    int ret = 0;

    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    cmd_send_tp.id = CMD_IRQ_IOCTL;
    cmd_send_tp.d0 = index;
    semi_touch_custom_cmd_delay();

    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    return ret;
}

int semi_touch_send_cmd_status(unsigned short cmd, unsigned short index,
                               unsigned short num, unsigned short checksum, s64 time)
{
    int ret = 0;

    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    cmd_send_tp.id = CMD_CTP_CMD;
    cmd_send_tp.d0 = cmd;
    cmd_send_tp.d1 = index;
    cmd_send_tp.d2 = num;
    cmd_send_tp.d3 = checksum;
    cmd_send_tp.d4 = time & 0xFFFF;
    cmd_send_tp.d5 = (time >> 16) & 0xFFFF;
    semi_touch_custom_cmd_delay();

    ret = cmd_send_to_tp_step_100us(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    return ret;
}

int semi_touch_heart_beat(void)
{
    int ret = 0, retry = 0;
    unsigned int regdata = 0;
    unsigned int regframe = 0;
    unsigned int ctp_stc_backup = st_dev.stc.ctp_run_status;
    struct _frame_cnt *ppt = NULL;

#if SEMI_TOUCH_THP_DRIVER_EN
    if (1 != semi_thp_dev.thp_get_frame_en)
        return ret;

    if (1 == semi_thp_dev.debug_flg)
        return ret;
#endif

    if(is_suspend_activate(st_dev.stc.ctp_run_status))
        return ret;
    if(is_guesture_activate(st_dev.stc.ctp_run_status))
        return ret;
    if(!is_status_pointing(st_dev.stc.ctp_run_status))
        return ret;
    if(!is_esd_function_en(st_dev.stc.custom_function_en))
        return ret;

    //slave report 0xFD to detect if ic is still alive
    if(semi_touch_check_watch_dog_feed(st_dev.stc.dog_feed_flag))
    {
        //dog feed in time
    }
    else
    {
        for(retry = 0; retry < 10; retry++)
        {
            msleep(2);
            ret = semi_touch_read_bytes(0x20000018, (unsigned char*)&regdata, 4);
            if (0x43534843 == regdata)
            {
                ret = semi_touch_read_bytes(0x20000010, (unsigned char *)&regframe, 4);
                break;
            }
        }
        ppt = (struct _frame_cnt *)&regframe;
        if (((ppt->frame_cnt + ppt->frame_cnt_check + 1) & 0xFFFF) != 0)
        {
            //reset tp + iic detected
            ret = semi_touch_reset_and_detect();
            check_return_if_fail(ret, NULL);

            if(is_guesture_activate(ctp_stc_backup))
            {
                ret = semi_touch_guesture_switch(G_GESTURE_BACK, 1);
                check_return_if_fail(ret, NULL);
            }
            if(is_glove_activate(ctp_stc_backup))
            {
                ret = semi_touch_glove_switch(1);
                check_return_if_fail(ret, NULL);
            }
            if(is_wet_finger_activate(ctp_stc_backup))
            {
                ret = semi_touch_wet_finger_switch(1);
                check_return_if_fail(ret, NULL);
            }
            if(is_h_v_sw_activate(ctp_stc_backup))
            {
                ret = semi_touch_h_v_switch(H_V_CODE, 1);
                check_return_if_fail(ret, NULL);
            }
            if(is_charger_activate(ctp_stc_backup))
            {
                ret = semi_touch_charger_switch(1);
                check_return_if_fail(ret, NULL);
            }
            if(is_game_mode_activate(ctp_stc_backup))
            {
                ret = semi_touch_game_mode_switch(G_GAME_BACK, 1);
                check_return_if_fail(ret, NULL);
            }
            if (is_palm_mode_activate(ctp_stc_backup))
            {
                ret = semi_touch_palm_mode_switch(1);
                check_return_if_fail(ret, NULL);
            }
            if(is_proximity_activate(ctp_stc_backup))
            {
                ret = semi_touch_proximity_switch(1);
                check_return_if_fail(ret, NULL);
            }
            semi_touch_send_rtime();
        }
    }

    semi_touch_reset_watch_dog(st_dev.stc.dog_feed_flag);

    return ret;
}

int semi_touch_write_bytes(unsigned int reg, const unsigned char* buffer, unsigned short len)
{
    int ret = SEMI_DRV_ERR_OK;
    unsigned int addr = reg;
    unsigned short once;
    static struct hal_io_packet packet;
    const unsigned short max_len = MAX_IO_BUFFER_LEN;
    mutex_lock(&st_dev.hal.bus_lock);

    while(len > 0)
    {
        once = min(len, max_len);
        packet.io_register = swab32(addr);
        memcpy(packet.io_buffer, buffer, once);
        packet.io_length = once;
        packet.hal_adapter = st_dev.hal.hal_param;

        ret = (*st_dev.hal.hal_write_fun)(&packet);
        check_break_if_fail(ret, NULL);

        addr += once;
        buffer += once;
        len -= once;
    }

    mutex_unlock(&st_dev.hal.bus_lock);

    return ret >= 0 ? SEMI_DRV_ERR_OK : ret;
}

int semi_touch_read_bytes(unsigned int reg, unsigned char* buffer, unsigned short len)
{
    int ret = SEMI_DRV_ERR_OK;
    unsigned int addr = reg;
    unsigned short once;
    static struct hal_io_packet packet;
    const unsigned short max_len = MAX_IO_BUFFER_LEN;
    mutex_lock(&st_dev.hal.bus_lock);

    while(len > 0)
    {
        once = min(len, max_len);
        packet.io_register = swab32(addr | HAL_RD_ADDR_TAG);
        packet.io_length = once;
        packet.hal_adapter = st_dev.hal.hal_param;

        ret = (*st_dev.hal.hal_read_fun)(&packet);
        check_break_if_fail(ret, NULL);
        memcpy(buffer, packet.io_buffer, once);

        addr += once;
        buffer += once;
        len -= once;
    }

    mutex_unlock(&st_dev.hal.bus_lock);

    return ret >= 0 ? SEMI_DRV_ERR_OK : ret;
}

int cmd_send_to_tp(struct m_ctp_cmd_std_t *ptr_cmd, struct m_ctp_rsp_std_t *ptr_rsp, const int delay)
{
    int ret = -SEMI_DRV_ERR_HAL_IO;
    unsigned int retry = 0, cmd_rsp_ok = 0;
    unsigned int regdata = 0;
    int once_delay = delay;

    mutex_lock(&st_dev.custom_lock);
    st_dev.cmd_flag = 1;
    ptr_cmd->tag = 0xE9;
    ptr_cmd->chk = 1 + ~caculate_checksum_u16((unsigned short*)&ptr_cmd->d0, sizeof(struct m_ctp_cmd_std_t) - 2);
    if(is_status_pointing(st_dev.stc.ctp_run_status))
    {
        for (retry = 0; retry < 10; retry++)
        {
            ret = semi_touch_read_bytes(0x20000018, (unsigned char*)&regdata, 4);
            if(0x43534843 == regdata) break;
            msleep(2);
        }
    }
    ret = semi_touch_write_bytes(TP_CMD_BUFF_ADDR, (unsigned char*)ptr_cmd, sizeof(struct m_ctp_cmd_std_t));
    //check_return_if_fail(ret, NULL);
    if (ret)
    {
        mutex_unlock(&st_dev.custom_lock);
        st_dev.cmd_flag = 0;
        kernel_log_e("err code = %d\r\n", ret);
        return ret;
    }
    retry = 0;
    while(retry++ < 40)
    {
        once_delay = delay;
        do
        {
            udelay(1500);
            once_delay -= 1500;
        }
        while (once_delay > 0);
        ret = semi_touch_read_bytes(TP_RSP_BUFF_ADDR, (unsigned char*)ptr_rsp, sizeof(struct m_ctp_rsp_std_t));
        //check_return_if_fail(ret, NULL);
        if (ret)
        {
            mutex_unlock(&st_dev.custom_lock);
            st_dev.cmd_flag = 0;
            kernel_log_e("err code = %d\r\n", ret);
            return ret;
        }

        if(ptr_cmd->id != ptr_rsp->id)
        {
            continue;
        }

        if(!caculate_checksum_u16((unsigned short*)ptr_rsp, sizeof(struct m_ctp_rsp_std_t)))
        {
            if(0 == ptr_rsp->cc)
            {
                cmd_rsp_ok = 1; //success
            }
            break;
        }
    }

    if(!cmd_rsp_ok) ret = -SEMI_DRV_ERR_TIMEOUT;
    mutex_unlock(&st_dev.custom_lock);
    st_dev.cmd_flag = 0;

    return ret;
}

int cmd_send_to_tp_step_100us(struct m_ctp_cmd_std_t *ptr_cmd, struct m_ctp_rsp_std_t *ptr_rsp, const int delay_us)
{
    int ret = -SEMI_DRV_ERR_HAL_IO;
    unsigned int retry = 0, cmd_rsp_ok = 0;
    int once_delay = delay_us;
    mutex_lock(&st_dev.custom_lock);
    st_dev.cmd_flag = 1;
    ptr_cmd->tag = 0xE9;
    ptr_cmd->chk = 1 + ~caculate_checksum_u16((unsigned short *)&ptr_cmd->d0, sizeof(struct m_ctp_cmd_std_t) - 2);
    ret = semi_touch_write_bytes(TP_CMD_BUFF_ADDR, (unsigned char *)ptr_cmd, sizeof(struct m_ctp_cmd_std_t));
    if (ret)
    {
        mutex_unlock(&st_dev.custom_lock);
        st_dev.cmd_flag = 0;
        kernel_log_e("err code = %d\r\n", ret);
        return ret;
    }
    retry = 0;
    while (retry++ < 400)
    {
        once_delay = delay_us;
        do
        {
            udelay(50);
            once_delay -= 50;
        }
        while (once_delay > 0);
        ret = semi_touch_read_bytes(TP_RSP_BUFF_ADDR, (unsigned char *)ptr_rsp, sizeof(struct m_ctp_rsp_std_t));
        if (ret)
        {
            mutex_unlock(&st_dev.custom_lock);
            st_dev.cmd_flag = 0;
            kernel_log_e("err code = %d\r\n", ret);
            return ret;
        }
        if (ptr_cmd->id != ptr_rsp->id)
        {
            continue;
        }
        if (!caculate_checksum_u16((unsigned short *)ptr_rsp, sizeof(struct m_ctp_rsp_std_t)))
        {
            if (0 == ptr_rsp->cc)
            {
                cmd_rsp_ok = 1; //success
            }
            break;
        }
    }
    if (!cmd_rsp_ok) ret = -SEMI_DRV_ERR_TIMEOUT;
    mutex_unlock(&st_dev.custom_lock);
    st_dev.cmd_flag = 0;
    return ret;
}
int cmd_send_to_tp_no_check(struct m_ctp_cmd_std_t *ptr_cmd)
{
    int ret = -SEMI_DRV_ERR_HAL_IO;
    unsigned int retry = 0,regdata = 0;

    ptr_cmd->tag = 0xE9;
    ptr_cmd->chk = 1 + ~caculate_checksum_u16((unsigned short*)&ptr_cmd->d0, sizeof(struct m_ctp_cmd_std_t) - 2);
    if(is_status_pointing(st_dev.stc.ctp_run_status))
    {
        for(retry = 0; retry < 20; retry++)
        {
            ret = semi_touch_read_bytes(0x20000018, (unsigned char*)&regdata, 4);
            if(0x43534843 == regdata) break;
            msleep(2);
        }
    }

    ret = semi_touch_write_bytes(TP_CMD_BUFF_ADDR, (unsigned char*)ptr_cmd, sizeof(struct m_ctp_cmd_std_t));
    check_return_if_fail(ret, NULL);

    return ret;
}

int read_and_report_touch_points(unsigned char *readbuffer, unsigned short len)
{
    int ret = 0, retry = 0;
    int read_flag = -SEMI_DRV_ERR_CHECKSUM;
    unsigned short tem = 0;

    if(!st_dev.stc.initialize_ok)
        ret = -SEMI_DRV_ERR_NO_INIT;
    check_return_if_fail(ret, NULL);

    if(is_guesture_activate(st_dev.stc.ctp_run_status))
    {
        for(retry = 0; retry < 30; retry++)
        {
            msleep(10);
            ret = semi_touch_read_bytes(st_dev.stc.touch_addr, readbuffer, len);
            tem = (unsigned short) readbuffer[SEMI_TOUCH_MAX_POINTS * 6 + 3] << 8;
            tem |= readbuffer[SEMI_TOUCH_MAX_POINTS * 6 + 2];
            if (0 == ((caculate_checksum_u816(readbuffer, SEMI_TOUCH_MAX_POINTS * 6 + 2) + tem) & 0xFFFF))
            {
                read_flag = SEMI_DRV_ERR_OK;
                break;
            }
            kernel_log_d("guesture read, retry = %d\n", retry);
        }
    }
    else
    {
        for(retry = 0; retry < 2; retry++)
        {
            ret = semi_touch_read_bytes(st_dev.stc.touch_addr, readbuffer, len);
            tem = (unsigned short) readbuffer[SEMI_TOUCH_MAX_POINTS * 6 + 3] << 8;
            tem |= readbuffer[SEMI_TOUCH_MAX_POINTS * 6 + 2];
            if (0 == ((caculate_checksum_u816(readbuffer, SEMI_TOUCH_MAX_POINTS * 6 + 2) + tem) & 0xFFFF))
            {
                read_flag = SEMI_DRV_ERR_OK;
                break;
            }
        }
    }
    if(SEMI_DRV_ERR_OK != read_flag)
        ret = -SEMI_DRV_ERR_CHECKSUM;

    return ret;
}

int semi_touch_mode_init(struct sm_touch_dev *st_dev)
{
    int ret = -SEMI_DRV_ERR_HAL_IO;
    unsigned char bootCheckOk = 0;
    unsigned char readbuffer[8] = {0};
    unsigned int rawdata_addr;
    unsigned int differ_addr;
    unsigned int base_addr;
    unsigned char addrCheckOk = 0;

    semi_touch_start_up_check(&bootCheckOk, only_sp_check);

    if(!bootCheckOk)
    {
        //reset tp + iic detected
        semi_touch_reset_and_detect();
        semi_touch_start_up_check(&bootCheckOk, only_sp_check);

        if(!bootCheckOk)
        {
            ret = -SEMI_DRV_ERR_TIMEOUT;
            check_return_if_fail(ret, NULL);
        }
    }

addrCheckerr:

    ret = semi_touch_read_bytes(0x20000020, readbuffer, sizeof(readbuffer));
    check_return_if_fail(ret, NULL);

    rawdata_addr = 0x20000000 + ((unsigned int)(readbuffer[0x01] << 8) + readbuffer[0x00]);
    differ_addr  = 0x20000000 + ((unsigned int)(readbuffer[0x03] << 8) + readbuffer[0x02]);
    base_addr    = 0x20000000 + ((unsigned int)(readbuffer[0x05] << 8) + readbuffer[0x04]);

    ret = semi_touch_read_bytes(0x20000020, readbuffer, sizeof(readbuffer));
    check_return_if_fail(ret, NULL);

    st_dev->stc.rawdata_addr = 0x20000000 + ((unsigned int)(readbuffer[0x01] << 8) + readbuffer[0x00]);
    st_dev->stc.differ_addr = 0x20000000 + ((unsigned int)(readbuffer[0x03] << 8) + readbuffer[0x02]);
    st_dev->stc.base_addr = 0x20000000 + ((unsigned int)(readbuffer[0x05] << 8) + readbuffer[0x04]);
    st_dev->stc.touch_addr = 0x20000000 + 0x2c;

    if ((st_dev->stc.rawdata_addr != rawdata_addr) ||
        (st_dev->stc.differ_addr  != differ_addr) ||
        (st_dev->stc.base_addr    != base_addr))
    {
        addrCheckOk++;
        if (addrCheckOk < 2)
        {
            goto addrCheckerr;
        }
        ret = -SEMI_DRV_ERR_HAL_IO;
        check_return_if_fail(ret, NULL);
    }

    st_dev->chsc_nodes_dir = NULL;
    st_dev->stc.initialize_ok = true;
    set_status_pointing(st_dev->stc.ctp_run_status);

    semi_touch_create_work_queue(work_queue_custom_work, typename(work_queue_custom_work));

    return ret;
}

int semi_touch_device_prob(void)
{
    int retry, regdata, loop;
    int ret = -SEMI_DRV_ERR_HAL_IO;
    int read_id_flag = 0;
    for (retry = 0; retry < 2; retry++)
    {
        semi_touch_reset(do_report_after_reset);
        for(loop = 0; loop < 10; loop++)
        {
            ret = semi_touch_read_bytes(0x40008034, (unsigned char *)&regdata, 4);
            check_return_if_fail(ret, NULL);
            if (0x550 == (regdata & 0xfffffff0))
            {
                 read_id_flag = 1;
                break;
            }
            msleep(5);
        }

        if (read_id_flag)
        {
          break;
        }
    }

    kernel_log_d("ic type=0x%x, loop = %d\r\n", regdata, loop);

    return (read_id_flag) ? SEMI_DRV_ERR_OK : -SEMI_DRV_ERR_HAL_IO;
}

int semi_touch_reset_and_detect(void)
{
    int ret = -SEMI_DRV_ERR_HAL_IO;

    //semi_touch_reset(no_report_after_reset); //delete

    ret = semi_touch_device_prob();

    msleep(50);
    set_status_pointing(st_dev.stc.ctp_run_status);

    return ret;
}
int semi_touch_start_up_check(unsigned char* checkOK, unsigned char opt)
{
    int ret = -EINVAL, retry = 0;
    img_header_t image_header;

    st_dev.fw_ver = st_dev.vid_pid = 0;

    for(retry = 0; retry < 20; retry++)
    {
        ret = semi_touch_read_bytes(0x20000014, (unsigned char*)&image_header, 8);
        check_return_if_fail(ret, NULL);

        if(image_header.sig == 0x43534843) //"CHSC"
        {
            *checkOK = 1;
            st_dev.fw_ver = image_header.fw_ver;
            ret = semi_touch_read_bytes(0x20000080, (unsigned char*)&image_header.resv2, 8);
            check_return_if_fail(ret, NULL);
            //st_dev.vid_pid = image_header.vid_pid;
            //st_dev.vid_pid = (*((unsigned long long*)((void*)(&image_header.resv2)+1))) & 0xffffffffff;
            memcpy(&st_dev.vid_pid, (unsigned char*)&image_header.resv2 + 1, 5);
            break;
        }
        else if(image_header.sig == 0x4F525245) //boot self check fail
        {
            *checkOK = 0;
            kernel_log_d("boot self check fail, upgrade is needed\r\n");
            break;
        }
        else //may be impossible; no firmware?
        {
            *checkOK = 0;
            kernel_log_d("retry-%d, firmware is not ready\r\n", retry);
        }

        msleep(10);
    }

    if(0 == *checkOK && check_backup_if_fail == opt)
    {
        ret = semi_touch_get_backup_pid(&st_dev.vid_pid);
        check_return_if_fail(ret, NULL);
    }

    return ret;
}
int semi_touch_destroy_work_queue(void)
{
    int index = 0;

    for(index = 0; index < st_dev.asyn_work.new_work_idx; index++)
    {
        cancel_delayed_work(&st_dev.asyn_work.works_list[index].work);
    }

    for(index = 0; index < work_queue_max; index++)
    {
        if(st_dev.asyn_work.work_queue[index])
        {
            destroy_workqueue(st_dev.asyn_work.work_queue[index]);
            st_dev.asyn_work.work_queue[index] = NULL;
        }
    }

    st_dev.asyn_work.new_work_idx = 0;

    return SEMI_DRV_ERR_OK;
}
int semi_touch_create_work_queue(enum work_queue_t queue_type, const char* queue_name)
{
    if(NULL == st_dev.asyn_work.work_queue[queue_type])
    {
        st_dev.asyn_work.work_queue[queue_type] = create_singlethread_workqueue(queue_name);
        check_return_if_zero(st_dev.asyn_work.work_queue[queue_type], NULL);
    }
    else
    {
        //aready exist!!!
    }

    return SEMI_DRV_ERR_OK;
}
int semi_touch_queue_asyn_work(enum work_queue_t queue_type, work_func_t work_func, int ms)
{
    int ret = 0, work_index = 0;
    struct delayed_work *workimp = NULL;

    check_return_if_zero(st_dev.asyn_work.work_queue[queue_type], NULL);

    for(work_index = 0; work_index < st_dev.asyn_work.new_work_idx; work_index++)
    {
        if((unsigned long)work_func == st_dev.asyn_work.works_list[work_index].uid)
        {
            workimp = &st_dev.asyn_work.works_list[work_index].work;
            break;
        }
    }

    if(NULL == workimp)
    {
        if(st_dev.asyn_work.new_work_idx < ASYN_WORK_MAX)
        {
            work_index = st_dev.asyn_work.new_work_idx++;
            workimp = &st_dev.asyn_work.works_list[work_index].work;
            st_dev.asyn_work.works_list[work_index].uid = (unsigned long)work_func;
            INIT_DELAYED_WORK(workimp, work_func);
        }
    }

    if(NULL != workimp)
    {
        queue_delayed_work(st_dev.asyn_work.work_queue[queue_type], workimp, msecs_to_jiffies(ms));
    }
    else
    {
        ret = -SEMI_DRV_ERR_NOT_MATCH;
        check_return_if_fail(ret, NULL);
    }

    return ret;
}