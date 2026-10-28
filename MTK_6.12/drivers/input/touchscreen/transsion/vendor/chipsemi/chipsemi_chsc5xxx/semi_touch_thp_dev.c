/**
********************************************************************************
* Copyright (C) 2021-2031,   All Rights Reserved.
*
* @file    : semi_touch_thp_dev.c
*
* @brief   : Mainly responsible for SPI initialization, creating DEV nodes,
             creating Pro nodes, and interrupting data reading
*
* @author  : Touch Team
*
* @version : 0.1
*
* @data    : 2024-08-022
*
* @par     :
*
* History  :
*
*  1.  Date: 2024-08-02
*
*    Author:
*
*    Modification:
*
*  2. ...
*
********************************************************************************
*/

/**
********************************************************************************
*.Included header files
********************************************************************************
*/
#include "semi_touch_interface.h"

#if SEMI_TOUCH_THP_DRIVER_EN

#include <linux/firmware.h>
#include <linux/gpio.h>

/**
********************************************************************************
*.Private constant and macro definitions using #define
********************************************************************************
*/
#define CHSC_THP_VERSION         "thp driver v: 2025031301"


#define DEV_CNT                  (1)
#define SPI_WR_BY_THP            (0)


/**
********************************************************************************
*.Private enumerations, structures and unions using typedef
********************************************************************************
*/
enum ioctl_cmd
{
    COMMON_DATA_CMD = 0,
    HARDWARE_PARAM_CMD,
    SELECT_MMAP_CMD,
    SELECT_TOUCH_ID,
    GET_FRAME_DATA_INDEX,
    RAW_DATA_INDEX,
};


/**
********************************************************************************
*.Global variable or extern global variabls/functions
********************************************************************************
*/
extern int touch_point_to_input(unsigned char *readbuffer, unsigned short thp_cmd_gesture_flg);
extern int semi_touch_enter_burn_mode(void);
extern int semi_touch_bulk_write(unsigned char *psrc, unsigned int adr, unsigned int len);
int thp_report_pos_to_input(unsigned char *readbuffer, unsigned short len);
static int semi_thp_get_cur_frame_len(struct sm_touch_dev *st_dev);

/**
********************************************************************************
*.Static variables
********************************************************************************
*/
s8 thp_drive_log_level = LG_ERROR; // LG_ERROR  LG_INFO
int boot_update_step = 0;
int chrdev_init_flag = 0;

static dev_t semi_touch_raw_devno;
static struct cdev touch_raw_dev;
static struct proc_dir_entry *touch_cmd_entry = NULL;


struct class *class_semi_touch_raw;
struct device *device_semi_touch_raw;


static DECLARE_WAIT_QUEUE_HEAD(touch_waitq);

static DECLARE_WAIT_QUEUE_HEAD(touch_cmd_waitq);


static volatile int ev_raw = 0;
static volatile int ev_cmd   = 0;

static u8 wake_up_cnt = 0;
static u8 ev_raw__cnt = 0;

static int spi_read_bytes_raw_process(unsigned int reg, unsigned char *buffer,
                                      unsigned short len);

semi_thp semi_thp_dev =
{
    .thp_en    = THP_EN,
    .debug_flg = 0,
    .spi_speed = 0,
    .cur_frame_len_bak    = 0,
    .cur_frame_len_bak_en = 0,
    .cur_frame_len = CUR_FRAME_LEN,
    .frame_data_en = 0,
    .read_frame_err = 0,
    .thp_get_frame_en = 1,
    .bottom_top_dif_max = 0,
    .loss_frame_cnt = 0,
    .read_mode = SPI_WR_BY_THP,
    .frame_log_cnt_limit = 2,

    .frame_data = {0},
    .version[0]  = 0,
};

common_data_t common_data_to_thp;
//common_data_t common_data_to_ic;

static int write_cmd_cnt = 0;

/**
********************************************************************************
*.
********************************************************************************
*/
static struct semi_touch_pdata *touch_pdata = NULL;


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
void input_lock(void)
{
    if (!touch_pdata)
    {
        return ;
    }

    spin_lock(&touch_pdata->input_lock);

}


void input_unlock(void)
{
    if (!touch_pdata)
    {
        return ;
    }

    spin_unlock(&touch_pdata->input_lock);
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
int semi_touch_cfg_force_update(unsigned char *parray, unsigned int cfg_single_len)
{
    int ret = SEMI_DRV_ERR_OK;

    boot_update_log("force_update\r\n");

#if BIN_CMD_FORCE_UPDATE_EN == 1

    if (0 == caculate_check_sum_u32((unsigned int *)parray, cfg_single_len))
    {

        boot_update_log("force_update\r\n");

        ret = semi_touch_enter_burn_mode();
        check_return_if_fail(ret, NULL);


        ret = semi_touch_bulk_write(parray, CFG_ROM_ADDRESS, cfg_single_len);
        check_return_if_fail(ret, NULL);


        boot_update_log("force_update\r\n");


    }
    else
    {
        ret = -SEMI_DRV_ERR_CHECKSUM;
        kernel_log_e("firmware config checksum error\n");
    }
#endif

    return ret;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
int semi_touch_boot_force_update(unsigned char *pdata, unsigned int len,
                                 unsigned int crc_len, unsigned int adr)
{
    int ret = SEMI_DRV_ERR_OK;

    boot_update_log("force_update\r\n");


#if BIN_CMD_FORCE_UPDATE_EN == 1

    if (*(unsigned int *)(pdata + crc_len - 4) == caculate_checksum_ex(pdata, crc_len - 4))
    {

        boot_update_log("force_update\r\n");

        ret = semi_touch_enter_burn_mode();
        check_return_if_fail(ret, NULL);

        ret = semi_touch_bulk_write((unsigned char *)pdata, adr, len);
        check_return_if_fail(ret, NULL);

        boot_update_log("force_update\r\n");


    }
    else
    {

        ret = -SEMI_DRV_ERR_CHECKSUM;
        kernel_log_e("firmware boot checksum error\n");
    }
#endif

    return ret;
}

/**
********************************************************************************
* @brief  : file set spi speed
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
u32 semi_touch_file_set_spi_speed(u32 speed_hz, struct sm_touch_dev *st_dev)
{

#if FILE_SET_SPI_SPEED_EN == 1

    int ret;

    const struct firmware *fw = NULL;
    char fwname[50] = { 0 };
    u32 pupd[10] = { 0 };
    u32 speed_hz_t = 0;


    snprintf(fwname, sizeof(fwname), "SetSpiPeed.bin");

    ret = request_firmware(&fw, fwname, &st_dev->client->dev);

    if (0 == ret)
    {
        kernel_log_a("open %s success,file size is %lu\n", fwname, fw->size);
    }


    if ((fw != NULL) && (fw->size < sizeof(pupd)))
    {

        memcpy(pupd, fw->data, fw->size);

        speed_hz_t = pupd[0] + pupd[1] * 1000000;

        kernel_log_a("defualt speed_hz %d  fw->size %d\n", speed_hz, (u32)fw->size);
        kernel_log_a("speed_hz_t %d  0x%8x  0x%8x ** \r\n", speed_hz_t, pupd[0], pupd[1]);

        if ((speed_hz_t > 100) && (speed_hz_t < 90000000))
        {
            speed_hz =  speed_hz_t;
        }
    }

    release_firmware(fw);

#endif

    semi_thp_dev.spi_speed = speed_hz;

    kernel_log_d("speed_hz %d\n", semi_thp_dev.spi_speed);

    return  semi_thp_dev.spi_speed;
}


/**
********************************************************************************
* @brief  : force_update
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
int semi_touch_fw_force_update_check(struct sm_touch_dev *st_dev, s32  fwn)
{
    int ret;
    u8 *pupd;
    const struct firmware *fw = NULL;
    char fwname[50] = { 0 };
    unsigned int adr;

    unsigned int len = 0;


    fwn = fwn - 1000;

    if (fwn == 200)
    {
        snprintf(fwname, sizeof(fwname), "cfg_mercury_default.bin");
        adr = CFG_ROM_ADDRESS;

    }
    else if (fwn == 0)
    {
        snprintf(fwname, sizeof(fwname), "FirmwareChecked0.bin");
        adr = 0;

    }
    else if (fwn == 1)
    {
        snprintf(fwname, sizeof(fwname), "FirmwareChecked1.bin");
        adr = 0;

    }
    else if (fwn == 2)
    {
        snprintf(fwname, sizeof(fwname), "boot_FirmwareChecked.bin");
        adr = 0;

    }
    else if ((fwn == 3) || (fwn == 4))
    {
        snprintf(fwname, sizeof(fwname), "boot_FirmwareChecked.bin");
        adr = 0;
        len = (4 * 1024) + 500;



    }
    else if (fwn == 8)
    {
        snprintf(fwname, sizeof(fwname), "FirmwareChecked8.bin");
        adr = (8 * 1024);

    }
    else if (fwn == 9)
    {
        snprintf(fwname, sizeof(fwname), "FirmwareChecked9.bin");
        adr = (8 * 1024);

    }
    else
    {
        kernel_log_d("unable to open firmware fwn %d\n", fwn);
        return SEMI_DRV_ERR_OK;
    }


    ret = request_firmware(&fw, fwname, &st_dev->client->dev);

    if (ret)
    {
        kernel_log_d("Update fail, unable to open firmware %s fw %p\n", fwname, fw);

        return SEMI_DRV_ERR_OK;

    }
    else
    {
        kernel_log_d("open %s success,firmware size is %lu\n", fwname, fw->size);
    }


    pupd = kzalloc(fw->size, GFP_KERNEL);
    if (!pupd)
    {
        kernel_log_d("Update fail, kzalloc %lu bytes fail\n", fw->size);
        goto exit;
    }

    memcpy(pupd, fw->data, fw->size);
    if (fw->size > 1024)
    {

        boot_update_log("force_update begin** \r\n");

        semi_thp_dev.debug_flg = 1;

        if (adr == CFG_ROM_ADDRESS)
        {
            semi_touch_cfg_force_update(pupd, fw->size);


        }
        else
        {
            if (len == 0) len = fw->size;

            semi_touch_boot_force_update(pupd, len, fw->size, adr);
        }


        boot_update_log("force_update finish**\r\n");

        mdelay(200);
        semi_touch_reset(do_report_after_reset);
    }

    kfree(pupd);

exit:

    release_firmware(fw);


    return SEMI_DRV_ERR_OK;
}

/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
int semi_touch_fw_force_update_boot(struct sm_touch_dev *st_dev, s32  *value)
{
    s32  fwn = *value ;

    fwn = fwn - 1000;

    if ((fwn == 3) || (fwn == 4))
    {
        boot_update_log("force_update begin** \r\n");

        semi_touch_fw_force_update_check(st_dev, *value);

        boot_update_log("force_update finish**\r\n");
    }


    if (fwn == 3)
    {
        *value = 1000; // "FirmwareChecked0.bin"


    }
    else if (fwn == 4)
    {
        *value = 1001; //"FirmwareChecked1.bin"
    }

    return SEMI_DRV_ERR_OK;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/

void semi_touch_ctrl_ic_interface(struct sm_touch_dev *st_dev, s32  value)
{
    unsigned char bootCheckOk;


    switch (value)
    {

        case 1300:
            kernel_log_a("retry %d " "\n", value);
            semi_touch_reset(no_report_after_reset);
            break;

        case 1301:
            kernel_log_a("value %d " "\n", value);
            semi_touch_reset(do_report_after_reset);
            break;

        case 1302:
            kernel_log_a("value %d " "\n", value);
            semi_touch_enter_burn_mode();
            break;

        case 1303:
            kernel_log_a("value %d " "\n", value);
            semi_touch_start_up_check(&bootCheckOk, check_backup_if_fail);
            break;

        case 1304:
            kernel_log_a("semi_io_pin_low value %d " "\n", value);
            semi_io_pin_low(st_dev->rst_pin);

            break;

        case 1305:
            kernel_log_a("semi_io_pin_high value %d " "\n", value);
            semi_io_pin_high(st_dev->rst_pin);
            break;


        default:

            break;
    }
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
void semi_touch_fast_debug_fun_check(struct sm_touch_dev *st_dev, common_data_t *common_data)
{
    s32 value = common_data->data_buf[0];

    if ((value > 999) && (value <= 1300))
    {
        semi_touch_fw_force_update_boot(st_dev, &value);
        semi_touch_fw_force_update_check(st_dev, value);

    }
    else if ((value > 1300) && (value <= 1400))
    {
        semi_touch_ctrl_ic_interface(st_dev, value);
    }

}



/**
********************************************************************************
* @brief  :THP_HAL_BUILD_VERSION
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static void thp_cmd_set_ic_value(common_data_t *common_data, int *buf_len)
{
    int ret = -1;
    int retry = 0;
    int mode = common_data->mode;
    u32 addr;

    char *temp_buf = (char *)common_data->data_buf;

    switch (mode)
    {

        case SET_OPEN_TRANSPORT_MODE:
//        common_data_to_ic = *common_data;

            kernel_log_i("mode %d data_buf **[%8x][%x][%x][%x][%x]** data_len %d len_limit %d" "\n",
                         mode,
                         common_data->data_buf[0],
                         temp_buf[4],
                         temp_buf[5],
                         temp_buf[6],
                         temp_buf[7],
                         common_data->data_len,
                         (u16)sizeof(common_data->data_buf[0]) * CMD_DATA_BUF_SIZE);

            addr = (u32) common_data->data_buf[0];

            for (retry = 0; retry < 2; retry++)
            {
                ret = semi_touch_write_bytes(addr, &temp_buf[4],  common_data->data_len - 4);
                if (ret == SEMI_DRV_ERR_OK)
                {
                    break;
                }
                kernel_log_w("retry %d " "\n", retry);
            }

            break;


        default:

            break;
    }

}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int thp_cmd_get_ic_value(common_data_t *common_data, int *buf_len)
{
    int ret = -1;
    int retry = 0;

    char *temp_buf = (char *)common_data->data_buf;
    u16 mode = common_data->mode;
    static  __attribute__((unused)) u16 read_ic_cnt = 0;
    u32 addr;

    switch (mode)
    {

        case GET_OPEN_TRANSPORT_MODE:
            read_ic_cnt++;

            kernel_log_i("mode %d data_buf **[%8x][%x][%x][%x][%x]** data_len %d len_limit %d" "\n",
                         mode,
                         common_data->data_buf[0],
                         temp_buf[4],
                         temp_buf[5],
                         temp_buf[6],
                         temp_buf[7],
                         common_data->data_len,
                         (u16)sizeof(common_data->data_buf[0]) * CMD_DATA_BUF_SIZE);


//                    memcpy(temp_buf, common_data_to_ic.data_buf, common_data->data_len);
//                    temp_buf[0] = read_ic_cnt;

            addr = (u32) common_data->data_buf[0];

            for (retry = 0; retry < 2; retry++)
            {
                if (semi_thp_dev.read_mode == 1)
                {
                    ret = spi_read_bytes_raw_process(addr, temp_buf,  common_data->data_len);
                }
                else
                {
                    ret = semi_touch_read_bytes(addr, temp_buf,  common_data->data_len);
                }

                if (ret == SEMI_DRV_ERR_OK)
                {
                    break;
                }
                kernel_log_w("retry %d " "\n", retry);
            }


            kernel_log_i("mode %d data_buf **[%8x][%x][%x][%x][%x]** data_len %d len_limit %d" "\n",
                         mode,
                         common_data->data_buf[0],
                         temp_buf[4],
                         temp_buf[5],
                         temp_buf[6],
                         temp_buf[7],
                         common_data->data_len,
                         (u16)sizeof(common_data->data_buf[0]) * CMD_DATA_BUF_SIZE);

            break;

#if 0
        case THP_REPORT_CLOSE_EN:
            common_data->data_len = snprintf(temp_buf, 10, "%d\n", hal_module_param->report_close_en);
            value = hal_module_param->report_close_en;
            break;
#endif

        default:
            return 0;

    }


    return ret;
}


/**
********************************************************************************
* @brief  :THP_HAL_BUILD_VERSION
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static void user_cmd_set_cur_value(common_data_t *common_data, int *buf_len)
{

    int mode = common_data->mode;

    char *temp_buf = (char *)common_data->data_buf;
    s32 value      = common_data->data_buf[0];

    if (mode == THP_DRIVE_FAST_DEBUG)
    {
        if (value >= 40000)
        {
            mode   = value - 40000;
            value  = common_data->data_buf[1];
        }
    }


    switch (mode)
    {

        case THP_DRIVE_FAST_DEBUG:
            semi_touch_fast_debug_fun_check(&st_dev, common_data);
            break;

        case THP_HAL_INIT_READY:

            break;

        case THP_HAL_BUILD_VERSION:
            kernel_log_a("mode %d data_buf **[%s]** data_len %d len_limit %d" "\n",
                         mode,
                         temp_buf,
                         common_data->data_len,
                         (u16)sizeof(common_data->data_buf[0]) * CMD_DATA_BUF_SIZE);

            if (common_data->data_len < sizeof(semi_thp_dev.version))
            {

                snprintf(semi_thp_dev.version, 248, "thp:[%s]", temp_buf);
            }

            return;


        case THP_LOCK_SCAN_MODE:

            break;

        case THP_DRIVE_GET_FRAME_EN:
            semi_thp_dev.thp_get_frame_en = temp_buf[0];
            break;


        case THP_DRIVE_LOG_LEVERL:

            semi_thp_dev.frame_log_cnt_limit = 2;
            if ((temp_buf[0] == (char)(-10))&&(common_data->data_len == 8))
            {
                value = common_data->data_buf[1];
                if (value >= 0 )
                {
                    semi_thp_dev.frame_log_cnt_limit = value;
                }
                kernel_log_a("data_len %d" "frame_log_cnt_limit %d" "\n",
                             common_data->data_len,value);
            }

            thp_drive_log_level              = temp_buf[0];


            break;

        case THP_DRIVE_CUR_FRAME_LEN:

            if (value == 0)
            {
                semi_thp_dev.cur_frame_len_bak = CUR_FRAME_LEN;
            }
            else
            {

                if (value >= sizeof(semi_thp_dev.frame_data))
                {
                    value = sizeof(semi_thp_dev.frame_data);
                }
                semi_thp_dev.cur_frame_len_bak = value;
            }

            semi_touch_mode_init(&st_dev);

            semi_thp_get_cur_frame_len(&st_dev);

            semi_thp_dev.cur_frame_len_bak_en = 1;

            break;

        case THP_DRIVE_CUR_FRAME_ADD:
            if (value >=0 )
            {
                st_dev.stc.thp_addr = value;
            }
            kernel_log_a("thp_addr %d %x " "\n",value, value);
            break;

        case THP_DRIVE_READ_MODE:
            if (value >=0 )
            {
                semi_thp_dev.read_mode = value;
            }
            kernel_log_a("read_mode %d" "\n",value);
            break;

        default:
            return;
    }


    kernel_log_i("mode %d data_buf **[%d]** data_len %d len_limit %d" "\n",
                 mode,
                 temp_buf[0],
                 common_data->data_len,
                 (u16)sizeof(common_data->data_buf[0]) * CMD_DATA_BUF_SIZE);
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int user_cmd_get_cur_value(common_data_t *common_data, int *buf_len)
{
    int value = 0;


//    char *temp_buf = (char *)common_data->data_buf;
//    u16 mode = common_data->mode;
//    u16 len_limit = sizeof(common_data->data_buf[0]) * CMD_DATA_BUF_SIZE;
//
//    switch (mode) {
//
//#if 0
//    case THP_REPORT_CLOSE_EN:
//        common_data->data_len = snprintf(temp_buf, 10, "%d\n", hal_module_param->report_close_en);
//        value = hal_module_param->report_close_en;
//        break;
//#endif
//
//    default:
//        return value;
//
//    }
//
//    kernel_log_i("mode %d data_buf **[%s]** data_len %d" "\n",
//             mode, common_data->data_buf, common_data->data_len);

    return value;
}

/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
u8 receive_common_data(common_data_t *common_data, int *buf_len)
{
    int result = 0;

    if (!common_data)
    {
        kernel_log_r("common data is %p\n", common_data);
        return -1;
    }

    if (common_data->data_len > sizeof(common_data->data_buf))
    {
        kernel_log_r("data_len is %d\n", common_data->data_len);
        return -1;
    }


    kernel_log_i("cmd:%d, mode:%d, size:%d, value[0]:%d\n",
                 common_data->cmd,
                 common_data->mode,
                 common_data->data_len,
                 common_data->data_buf[0]);


    switch (common_data->cmd)
    {

        case GET_CUR_VALUE:
            result = user_cmd_get_cur_value(common_data, buf_len);
            break;

        case SET_CUR_VALUE:
            user_cmd_set_cur_value(common_data, buf_len);
            break;

        case SET_CMD_FOR_DRIVER:
            user_cmd_set_cur_value(common_data, buf_len);
            break;



        case GET_THP_IC_CUR_VALUE:
            result = thp_cmd_get_ic_value(common_data, buf_len);
            break;

        case SET_THP_IC_CUR_VALUE:
            thp_cmd_set_ic_value(common_data, buf_len);
            break;




        default:
            kernel_log_r(" don't support ioctl_cmd %d\n", common_data->cmd);

            break;
    }

    return result;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static s64 gett_do_gettimeofday_ns(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
    struct timespec64 time1;
    ktime_get_real_ts64(&time1);

    return time1.tv_sec * UMECS_PER_SEC + (time1.tv_nsec);

#else
    struct timeval time1;

    do_gettimeofday(&time1);

    return (time1.tv_sec * UMECS_PER_SEC + time1.tv_usec) * 1000;
#endif

}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
//s64 get_current_kernel_time(void)
//{
//    struct timespec read_frame_time;
//
//    read_frame_time =  current_kernel_time();
//
//    return read_frame_time.tv_sec * (s64)1000000000 + read_frame_time.tv_nsec;
//
//}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
//s64 timeval_diff(struct timeval time1, struct timeval time2)
//{
//    int64_t t1 =  time1.tv_sec * UMECS_PER_SEC + time1.tv_usec;
//    int64_t t2 =  time2.tv_sec * UMECS_PER_SEC + time2.tv_usec;
//
//    if (t1 > t2)
//        return t1 - t2;
//    else
//        return t2 - t1;
//}

/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval : 0 report point packet    1 cmd packet
*
********************************************************************************
*/
static s8 check_semi_report_point(void *point)
{
    report_piont_t_semi *preport_piont_t = (report_piont_t_semi *)point;


    if ((preport_piont_t->input_style == INPUT_STYLE) &&
        (preport_piont_t->reportflg == REPORT_FLG))
    {

        return 0;

    }
    else if ((preport_piont_t->input_style == CMD_STYLE) &&
             (preport_piont_t->reportflg == CMD_FLG))
    {

        return 1;
    }

    kernel_log_r("input_style err "
                 " input_style %d "
                 "reportflg %d ",
                 preport_piont_t->input_style,
                 preport_piont_t->reportflg);

    return -1;

}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
int touch_point_to_input_limit(int id)
{
    if (semi_thp_dev.thp_en == 1)
    {
        return -1;
    }

    if (id < NO_THP_POINT_TO_INPUT_LIMIT)
    {
        return -1;
    }

    return 0;
}



/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
int copy_touch_rawdata(char *raw_base,  int len)
{

    if (!touch_pdata || len < 0)
    {
        return -ENODEV;
    }


    memcpy((unsigned char *)touch_pdata->raw_buf[touch_pdata->raw_tail],
           (unsigned char *)raw_base,  len);

    touch_pdata->raw_len = len;


    spin_lock(&touch_pdata->raw_lock);

    touch_pdata->raw_tail++;
    if (touch_pdata->raw_tail == RAW_BUF_NUM)
        touch_pdata->raw_tail = 0;

    spin_unlock(&touch_pdata->raw_lock);

    return 0;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int semi_touch_read_bytes_raw(struct hal_io_packet *ppacket, unsigned char *buffer)
{
    int error = SEMI_DRV_ERR_OK;

    struct spi_device *spi_device = (struct spi_device *)ppacket->hal_adapter;
    u32 write_len = 4;
    u32 read_len  = ppacket->io_length;


    u8 *rx_buf;

    struct spi_message *message;
    struct spi_transfer *transfer;


    if (!write_len && !read_len)
    {
        return -ENOMEM;
    }


    rx_buf = kzalloc(2056, GFP_KERNEL);
    if (!rx_buf)
    {
        kernel_log_r("Failed to allocate memory for read buffer\n");
        return -ENOMEM;
    }

    // int spi_message and spi_transfer
    message = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
    transfer = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
    if (!message || !transfer)
    {
        kfree(rx_buf);
        kfree(message);
        kfree(transfer);
        kernel_log_r("Failed to allocate memory for SPI message or transfer\n");
        return -ENOMEM;
    }




    // set spi_transfer
    transfer->tx_buf = (unsigned char *)ppacket;
    transfer->rx_buf = NULL;
    transfer->len    = write_len;

    if (write_len && read_len)
    {
        transfer->cs_change = 1;
    }


    spi_message_init(message);
    spi_message_add_tail(transfer, message);
    error = spi_sync(spi_device, message);    //SPI write operation




    //SPI read operation
    if (error < 0)
    {
        kernel_log_r("spi_sync write failed: %d\n", error);
    }
    else
    {

        transfer->tx_buf = NULL;
        transfer->rx_buf = rx_buf;
        transfer->len    = read_len;

        if (write_len && read_len)
        {
            transfer->cs_change = 0;
        }

        if (!read_len)
        {
            return -ENOMEM;
        }

        spi_message_init(message);
        spi_message_add_tail(transfer, message);

        error = spi_sync(spi_device, message);


        memcpy(buffer, rx_buf, read_len);


        if (error < 0)
        {
            kernel_log_r("spi_sync read failed to read data: %d\n", error);
        }
        else
        {
            kernel_log_d0("Read %d bytes of data\n", read_len);
        }
    }


    kfree(rx_buf);
    kfree(message);
    kfree(transfer);

    return error;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int spi_read_bytes_raw_process(unsigned int reg, unsigned char *buffer, unsigned short len)
{
    int ret = SEMI_DRV_ERR_OK;
    unsigned int addr = reg;
    unsigned short once;
    static struct hal_io_packet packet;

    const unsigned short max_len = READ_BUF_MAX_SIZE;

    mutex_lock(&st_dev.hal.bus_lock);

    while (len > 0)
    {
        once = min(len, max_len);
        packet.io_register = swab32(addr | HAL_RD_ADDR_TAG);
        packet.io_length = once;
        packet.hal_adapter = st_dev.hal.hal_param;

        ret = semi_touch_read_bytes_raw(&packet, buffer);
        check_break_if_fail(ret, NULL);


        addr += once;
        buffer += once;
        len -= once;
    }

    mutex_unlock(&st_dev.hal.bus_lock);

    return ret >= 0 ? SEMI_DRV_ERR_OK : ret;
}


/**
********************************************************************************
* @brief  : get thp raw and report point
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int read_and_report_touch_raw(unsigned char *readbuffer, unsigned short len, s64 *time)
{
    int ret = 0, retry = 0;
    int read_flag = -SEMI_DRV_ERR_CHECKSUM;

    time[0] = gett_do_gettimeofday_ns();

    ____need_improve;

    if (!st_dev.stc.initialize_ok)
        ret = -SEMI_DRV_ERR_NO_INIT;

    check_return_if_fail(ret, NULL);


    if (is_guesture_activate(st_dev.stc.ctp_run_status))
    {

        for (retry = 0; retry < 30; retry++)
        {
            msleep(10);

            ret = semi_touch_read_bytes(st_dev.stc.thp_addr, readbuffer, len);

            if (ret == SEMI_DRV_ERR_OK)
            {
                read_flag = SEMI_DRV_ERR_OK;
                break;
            }

            kernel_log_w("guesture read, retry = %d\n", retry);
        }


    }
    else
    {
        for (retry = 0; retry < 2; retry++)
        {
            if (semi_thp_dev.read_mode == 1)
            {
                ret = spi_read_bytes_raw_process(st_dev.stc.thp_addr, readbuffer, len);
            }
            else
            {
                ret = semi_touch_read_bytes(st_dev.stc.thp_addr, readbuffer, len);
            }

            if (ret == SEMI_DRV_ERR_OK)
            {
                read_flag = SEMI_DRV_ERR_OK;
                break;
            }

            kernel_log_w("retry %d " "\n", retry);

        }
    }



    if (SEMI_DRV_ERR_OK != read_flag)
        ret = -SEMI_DRV_ERR_CHECKSUM;

    return ret;
}

/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static void test_raw_data(s16 *raw_data)
{
    int len = 40 * 60;
    int i = 0;

    while (len--)
    {

        raw_data[i] = i;
        i++;

    }
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
int test_raw_data_process(unsigned int *raw_data, int irq_cnt,
                          s64 time, u8 *readbuffer, u16 len)
{
//    unsigned char *readbuffer  =  semi_thp_dev.frame_data   //get_readbuffer();
    unsigned short report_size = ((SEMI_TOUCH_MAX_POINTS * 5 + 3) + 3) & 0xfc;

    report_piont_t_semi *preport_piont_t;


    tp_frame_drive *p_frame;
    tp_raw *p_tp_raw;


    raw_data_t *main_raw;
    raw_data_t *scap_raw;
    raw_data_t *scap2_raw;
    u8 *debug_buf;

    s8 col_num = 38;// long
    s8 row_num = 17;// short

    if (!raw_data)
    {
        return -ENOMEM;
    }


    p_frame  = (tp_frame_drive *)raw_data;



    p_tp_raw = (tp_raw *)p_frame->thp_frame_buf;

    p_frame->frame_cnt      = irq_cnt;
    p_frame->time_ns        = time * 1000;
    p_frame->fod_pressed    = 1;
    p_frame->fod_trackingId = 2;




    {
        main_raw = p_tp_raw->frame_data;
        test_raw_data(main_raw);


        p_tp_raw->col_num      = col_num;
        p_tp_raw->row_num      = row_num;

        p_tp_raw->ic_head_cnt  = irq_cnt;

        p_tp_raw->debug_buf_size = 255 ;

        p_tp_raw->ic_frame_cnt = irq_cnt ;


        scap_raw =  &main_raw[row_num * col_num];
        scap2_raw = &scap_raw[row_num + col_num];
        debug_buf = (u8 *)&scap2_raw[row_num + col_num];


        preport_piont_t = (report_piont_t_semi *)debug_buf;


        preport_piont_t->input_style = INPUT_STYLE;
        preport_piont_t->report_size = report_size;

        if ((p_tp_raw->debug_buf_size) &&
            (report_size < sizeof(preport_piont_t->readbuffer))
           )
        {
            memcpy(preport_piont_t->readbuffer, readbuffer, report_size);
        }
        preport_piont_t->reportflg = REPORT_FLG;
        preport_piont_t->irq_cnt   = irq_cnt;


        main_raw[row_num * col_num - 1] = irq_cnt;
    }

    return 0;

}



/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
void all_v_int_by_cmd(void)
{
    if (thp_drive_log_level != -88) return;


    semi_thp_dev.loss_frame_cnt = 0;
    semi_thp_dev.bottom_top_dif_max = 0;



    kernel_log_a("finish!\n");


}


/**
********************************************************************************
* @brief  :Copy TP data to the location specified by MMAP
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static void buf_log(u8 *readbuffer, u16 len,int irq_cnt)
{
    u16 i ;
    u16 j ;
    u16 k ;
    static u32 buf_log_cnt = 0;
    static u32 buf_log_cnt_bak = 0;

    if (thp_drive_log_level != (-10) ) return;

    if (len == 0 ) return;

    if (semi_thp_dev.frame_log_cnt_limit )
    {
        buf_log_cnt = semi_thp_dev.frame_log_cnt_limit;
        buf_log_cnt_bak = buf_log_cnt;
        semi_thp_dev.frame_log_cnt_limit = 0;
    }

    if (buf_log_cnt == 0) return;

    if (buf_log_cnt > 0) buf_log_cnt--;


    kernel_log_i_10(
        "drive_cnt %4d  "

        "loss_frame_cnt %4d "
        "bottom_top_dif_max %3d "
        "thp_addr %x "
        "cur_frame_len %d "
        "read_mode %d "
        "buf_log_cnt %d %d "
        "\n",

        irq_cnt,

        semi_thp_dev.loss_frame_cnt,
        semi_thp_dev.bottom_top_dif_max,
        st_dev.stc.thp_addr,
        semi_thp_dev.cur_frame_len,
        semi_thp_dev.read_mode,
        buf_log_cnt,
        buf_log_cnt_bak
    );

    i = (len / 20);
    j = (len %20);
    k = 0;

    while (i--)
    {
        kernel_log_i_10(
            "%2x  " "%2x  " "%2x  " "%2x  "
            "%2x  " "%2x  " "%2x  " "%2x  "
            "%2x  " "%2x  " "%2x  " "%2x  "
            "%2x  " "%2x  " "%2x  " "%2x  "
            "%2x  " "%2x  " "%2x  " "%2x  "

            "\n",

            readbuffer[k+0],readbuffer[k+1],readbuffer[k+2],readbuffer[k+3],
            readbuffer[k+4],readbuffer[k+5],readbuffer[k+6],readbuffer[k+7],
            readbuffer[k+8],readbuffer[k+9],readbuffer[k+10],readbuffer[k+11],
            readbuffer[k+12],readbuffer[k+13],readbuffer[k+14],readbuffer[k+15],
            readbuffer[k+16],readbuffer[k+17],readbuffer[k+18],readbuffer[k+19]
        );

        k += 20;
    }


    while (j--)
    {
        kernel_log_i_10("%x  "  "\n",readbuffer[k]);

        k += 1;
    }

    kernel_log_i_10("\n");
    kernel_log_i_10("\n");

}

/**
********************************************************************************
* @brief  :Copy TP data to the location specified by MMAP
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int raw_data_process(unsigned int *raw_data, int irq_cnt, s64 time,
                            u8 *readbuffer, u16 len)
{
    static u8 top_int = 0;
    u8 bottom_top_dif = 0;

    tp_frame_drive_nbuf *pdrive_nbuf;

    tp_frame_drive *p_frame;
    tp_raw *p_tp_raw;

    u8  bottom;
    u8  top;

    u8  *pbottom;
    u8  *ptop;

    if (!raw_data)
    {
        return -ENOMEM;
    }

    pdrive_nbuf = (tp_frame_drive_nbuf *)raw_data;
    ptop        = &pdrive_nbuf->top;
    pbottom     = &pdrive_nbuf->bottom;

    if (top_int == 0)
    {
        top_int    = 1;

        ptop[0]    = 0;
        pbottom[0] = 0;

        pdrive_nbuf->buf_len = sizeof(tp_frame_drive);
        pdrive_nbuf->buf_len_r = -(pdrive_nbuf->buf_len + 1);
    }

    bottom = pbottom[0];
    top    = ptop[0];

    if (top >= DRIVE_BUF_NUM)
    {
        ptop[0] = 0;
        top     = 0;
    }

    p_frame  = (tp_frame_drive *)&pdrive_nbuf->buf[top];

    p_frame->frame_cnt      = irq_cnt;
    p_frame->time_ns        = time;
    p_frame->wake_up_cnt    = wake_up_cnt;
    p_frame->ev_raw__cnt    = ev_raw__cnt;

    p_frame->fod_pressed    = 1;
    p_frame->bottom         = bottom;
    p_frame->top            = top;
    p_frame->fod_trackingId = 2;


    p_tp_raw = (tp_raw *)p_frame->thp_frame_buf;

    memcpy(p_tp_raw, readbuffer, len);

    p_frame->thp_dbg_buf[0] = semi_thp_dev.loss_frame_cnt;
    p_frame->thp_dbg_buf[1] = bottom_top_dif;
    p_frame->thp_dbg_buf[2] = semi_thp_dev.bottom_top_dif_max;


    // Collect statistics on cache usage
    if (top >= bottom)
    {
        bottom_top_dif = top - bottom;
    }
    else
    {
        bottom_top_dif = top + DRIVE_BUF_NUM - bottom;
    }

    if (bottom_top_dif > semi_thp_dev.bottom_top_dif_max)
    {
        semi_thp_dev.bottom_top_dif_max = bottom_top_dif;
    }


    top++;
    if (top >= DRIVE_BUF_NUM)
    {
        top = 0;
    }

    if (top != bottom)
    {
        ptop[0]= top;
    }
    else
    {
        semi_thp_dev.loss_frame_cnt++;
    }

    all_v_int_by_cmd();

    buf_log(readbuffer,len,irq_cnt);

    kernel_log_i_8(
        "drive_cnt %4d  "
        "ic_head %4d %4d  "
        "ic_frame %4d "
        "wake_up %4d "
        "ev_raw %4d "

        "bottom %4d "
        "top %4d "
        "loss_frame_cnt %4d "
        "bottom_top_dif %3d %3d"

        "\n",
        irq_cnt,
        p_tp_raw->ic_head_cnt,
        irq_cnt - p_tp_raw->ic_head_cnt,

        p_tp_raw->ic_frame_cnt,
        p_frame->wake_up_cnt,
        p_frame->ev_raw__cnt,
        p_frame->bottom,
        p_frame->top,
        semi_thp_dev.loss_frame_cnt,
        bottom_top_dif,
        semi_thp_dev.bottom_top_dif_max
    );

    return 0;

}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int semi_touch_proc_thp_cmd_open(struct inode *inode, struct file *filp)
{
    static int runcnt = 0;

    runcnt++;

    kernel_log_a("ok %d \r\n", runcnt);


    return 0;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static ssize_t semi_touch_proc_thp_cmd_read(struct file *fp, char __user *buff,
        size_t len, loff_t *ppos)
{
    int ret = 0;

    static int cnt = 0;


    if (len <= 0)  return -EINVAL;



    cnt++;

    kernel_log_a("ok4 %d  ret %d  len %d",
                 cnt,
                 ret,
                 (int)len
                );


    return ret;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static ssize_t semi_touch_proc_thp_cmd_write(struct file *fp, const char __user *buff,
        size_t len, loff_t *ppos)
{
    int ret = 0;

    static int cnt = 9;

    cnt++;

    kernel_log_a("ok %d\r\n", cnt);



    return ret;
}


/**
********************************************************************************
* @brief  :  POLLRDNORM; have cmd          POLLPRI have raw
*            wake_up_interruptible(&touch_cmd_waitq);

* @param  :
*
* @retval :
*
********************************************************************************
*/
static unsigned int semi_touch_proc_thp_cmd_poll(struct file *file,
        poll_table *wait)
{
    unsigned int mask = 0;

    poll_wait(file, &touch_cmd_waitq, wait); //Not immediately entering sleep

    spin_lock(&touch_pdata->cmd_lock);
    if (ev_cmd == 1)
    {
        mask |= POLLRDNORM;
        // ev_cmd = 0;
    }
    spin_unlock(&touch_pdata->cmd_lock);


    return mask;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static long semi_touch_proc_thp_cmd_ioctl(struct file *fp, unsigned int op_type, unsigned long args)
{
    kernel_log_a("proc op_type = %x, args = %d\r\n", op_type, (int)args);




    return 0;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int semi_touch_proc_thp_cmd__release(struct inode *inode, struct file *filp)
{

    return 0;
}


/**
********************************************************************************
*.
********************************************************************************
*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static const struct proc_ops semi_touch_thp_cmd_fops =
{
    .proc_open           = semi_touch_proc_thp_cmd_open,
    .proc_write          = semi_touch_proc_thp_cmd_write,
    .proc_read           = semi_touch_proc_thp_cmd_read,
    .proc_poll           = semi_touch_proc_thp_cmd_poll,
    .proc_ioctl = semi_touch_proc_thp_cmd_ioctl,
#ifdef CONFIG_COMPAT
    .proc_compat_ioctl   = semi_touch_proc_thp_cmd_ioctl,
#endif
    .proc_release        = semi_touch_proc_thp_cmd__release,
};

#else
static const struct file_operations semi_touch_thp_cmd_fops =
{
    .owner          = THIS_MODULE,
    .open           = semi_touch_proc_thp_cmd_open,
    .write          = semi_touch_proc_thp_cmd_write,
    .read           = semi_touch_proc_thp_cmd_read,
    .poll           = semi_touch_proc_thp_cmd_poll,
    .unlocked_ioctl = semi_touch_proc_thp_cmd_ioctl,
    .compat_ioctl   = semi_touch_proc_thp_cmd_ioctl,
    .release        = semi_touch_proc_thp_cmd__release,
};
#endif


/**
********************************************************************************
* @brief  :proc_create  SEMI_TOUCH_PROC_NAME_RAW
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int semi_touch_create_cmd_proc(char *proc_name)
{
    int ret = 0;

    touch_cmd_entry = proc_create(proc_name, 0777, NULL, &semi_touch_thp_cmd_fops);
    check_return_if_zero(touch_cmd_entry, NULL);

    kernel_log_a("create /proc/%s is ok %s\r\n", proc_name, CHSC_THP_VERSION);

    return ret;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int semi_touch_remove_cmd_proc(char *proc_name)
{
    int ret = 0;

    kernel_log_a("remove  /proc/%s  %s\r\n", proc_name, CHSC_THP_VERSION);
    if (touch_cmd_entry)
    {
        proc_remove(touch_cmd_entry);
    }
    return ret;
}



/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int semi_touch_dev_open(struct inode *inode, struct file *filp)
{
    static int runcnt = 0;

    runcnt++;

    kernel_log_a("ok %d \r\n", runcnt);


    if (!touch_pdata)
    {

        kernel_log_r("touch_pdata NULL\n");

        return -ENOMEM;
    }


    if (semi_thp_dev.thp_en == 2)
    {
        semi_thp_dev.thp_en = 1;

        kernel_log_a("ok %d thp_en %d\r\n", runcnt, semi_thp_dev.thp_en);
    }


    filp->private_data = touch_pdata;

    return 0;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
ssize_t semi_touch_dev_write1(struct file *filp,
                              const char __user *buf,
                              size_t cnt,
                              loff_t *off)
{



    return 0;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static ssize_t semi_touch_dev_write(struct file *filp,
                                    const char __user *buf,
                                    size_t cnt,
                                    loff_t *off)
{
    static u32 cnt_255_err = 0;

    int ret = 0;

    int data_style = 0;

    s8 write_data[255];
    report_piont_t_semi *preport_piont_t = (report_piont_t_semi *)write_data;

    if (cnt > sizeof(write_data))
    {

        kernel_log_r("cnt %d \r\n", (int)cnt);

        return -ENOMEM;
    }

    mutex_lock(&touch_pdata->write_mutex);

    preport_piont_t->input_style = 0;
    preport_piont_t->reportflg   = 0;


    ret = copy_from_user(write_data, buf, cnt);

    if (cnt != 255) cnt_255_err++;

    kernel_log_i_11("cnt %d  ret %d  cnt_255_err %d \r\n",
                    (int)cnt,ret, cnt_255_err);


    if (ret != 0)
    {
        mutex_unlock(&touch_pdata->write_mutex);
    }
    check_return_if_fail(ret, NULL);

    data_style = check_semi_report_point((void *)preport_piont_t);


    if (data_style == 0)   //Report point data packet
    {

        unsigned char *readbuffer  =  preport_piont_t->readbuffer;
        unsigned short report_size =  preport_piont_t->report_size;

        thp_report_pos_to_input(readbuffer, report_size);
    }
    else if (data_style == 1)     // cmd data packet
    {
        common_data_t *pcommon_data = (common_data_t *)preport_piont_t->readbuffer;

        if (write_cmd_cnt < 10000)
        {
            write_cmd_cnt++;
        }

        common_data_to_thp = *pcommon_data;

//        {
//
//            kernel_log_r("%d send thp socket data: cmd = %d, mode = %d, length = %d, value = ",
//                         write_cmd_cnt, common_data.cmd, common_data.mode, common_data.data_len);
//
////            for (int i = 0; i < common_data.data_len; i++) {
////                kernel_log_r("%d,", common_data.data_buf[i]);
////            }
//            kernel_log_r("%d,", common_data.data_buf[0]);
////            kernel_log_r("%d,", common_data.data_buf[1]);
//
//            kernel_log_r("\n");
//        }


        spin_lock(&touch_pdata->cmd_lock);
        ev_cmd = 1;
        spin_unlock(&touch_pdata->cmd_lock);
        wake_up_interruptible(&touch_cmd_waitq);

    }

    else
    {
        kernel_log_a("semi_touch_dev_write1 \r\n");

        semi_touch_dev_write1(filp, buf, cnt, off);
    }

    mutex_unlock(&touch_pdata->write_mutex);

    return ret;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static ssize_t semi_touch_dev_read(struct file *filp, char __user *buf,
                                   size_t cnt, loff_t *off)
{
    __attribute__((unused)) int copy_number = 0;
    s32 data_size = sizeof(common_data_t);

    s16 *read_data = NULL;

    static int read_data_cnt = 0;

    read_data_cnt++;

    kernel_log_i0("ok %d %s\r\n", read_data_cnt, "789");


    read_data = (s16 *)kzalloc(cnt, GFP_KERNEL);


    if (cnt == data_size)
    {

        if (write_cmd_cnt == 0)
        {
            common_data_to_thp.touch_id    = 0;
            common_data_to_thp.cmd         = SET_CUR_VALUE;
            common_data_to_thp.mode        = THP_ALG_LOG_LEVEL;
            common_data_to_thp.data_len    = 1;
            common_data_to_thp.data_buf[0] = 5;
        }

        common_data_to_thp.data_buf[CMD_DATA_BUF_SIZE - 1] = read_data_cnt;
        common_data_to_thp.data_buf[CMD_DATA_BUF_SIZE - 2] = write_cmd_cnt;

        spin_lock(&touch_pdata->cmd_lock);
        if (ev_cmd == 1)
        {
            ev_cmd = 0;
        }
        spin_unlock(&touch_pdata->cmd_lock);


        if (read_data != NULL)
            memcpy(read_data, &common_data_to_thp, data_size);

    }
    else
    {

    }

    copy_number = copy_to_user(buf, read_data, cnt);

    kfree(read_data);




    return 0;
}


/**
********************************************************************************
* @brief  :  POLLRDNORM; have cmd          POLLPRI have raw
*            wake_up_interruptible(&touch_waitq);

* @param  :
*
* @retval :
*
********************************************************************************
*/
static unsigned int semi_touch_dev_poll(struct file *file,
                                        poll_table *wait)
{
    unsigned int mask = 0;

    poll_wait(file, &touch_waitq, wait); //Not immediately entering sleep


    spin_lock(&touch_pdata->raw_lock);
    if (ev_raw == 1)
    {
        mask |= POLLPRI;
        ev_raw = 0;
        ev_raw__cnt++;
    }
    spin_unlock(&touch_pdata->raw_lock);

    return mask;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int semi_touch_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct semi_touch_pdata *pdata = file->private_data;


    unsigned long start = vma->vm_start;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long page;
    unsigned long pos;

    if (!pdata)
    {
        kernel_log_r("invalid memory\n");
        return -ENOMEM;
    }

    /*
    tx_num = pdata->touch_data->get_touch_tx_num();
    rx_num = pdata->touch_data->get_touch_rx_num();
    */

    pos = (unsigned long)pdata->phy_base + offset;
    page = pos >> PAGE_SHIFT;

    if (remap_pfn_range(vma, start, page, size, PAGE_SHARED))
    {
        return -EAGAIN;
    }
    else
    {
        kernel_log_a("remap_pfn_range %u, size:%ld, success\n", (unsigned int)page, size);
    }


    return 0;
}


/**
********************************************************************************
* @brief  : ioctl
*
* @param  :
* @param cmd
* @param size
* @param arg
* @param dir Read or Write: 1 indicates write, 2 indicates read,
                    3 indicates read/write
*
* @retval :
*          _IOC(dir,type,nr,size)
           _IOC_DIR(nr)    _IOC_TYPE(nr)  _IOC_NR(nr) _IOC_SIZE(nr)
 *
 *   0xE0000000     DIR
 *   0x80000000     DIR = WRITE
 *   0x40000000     DIR = READ
 *   0x20000000     DIR = NONE
 *   0x3FFF0000   SIZE (overlaps NONE bit)
 *   0x0000FF00   TYPE
 *   0x000000FF   NR (CMD)
********************************************************************************
*/
static long semi_touch_dev_ioctl(struct file *file, unsigned int cmd,
                                 unsigned long arg)
{
    int ret = -EINVAL;

    static char flg = 0;
    volatile  __attribute__((unused)) int press = 0;

    int buf[MAX_BUF_SIZE] = {0,};
    common_data_t *common_data;

    struct semi_touch_pdata *pdata = touch_pdata;

    void __user *argp = (void __user *) arg;

    struct semi_touch_interface *touch_data = NULL;

    int user_cmd  = _IOC_NR(cmd);
    int buf_len   = 0;
    int addr_flg  = 0;


    mutex_lock(&pdata->ioctl_mutex);

    if ((user_cmd != GET_FRAME_DATA_INDEX) ||
        ((user_cmd == GET_FRAME_DATA_INDEX) && (flg == 0)))
    {
        kernel_log_i("proc op_type = %x, args = %d\r\n", cmd, (int)arg);
        kernel_log_i("DIR = %x TYPE = %x  cmd = %x SIZE = %x \r\n",
                     _IOC_DIR(cmd),
                     _IOC_TYPE(cmd),
                     _IOC_NR(cmd),
                     _IOC_SIZE(cmd));

    }

    if ((user_cmd == GET_FRAME_DATA_INDEX) && (flg == 0))
    {
        flg = 1;
    }


    touch_data = pdata->touch_data[0];

    if (!pdata || !touch_data)
    {

        kernel_log_r("invalid memory pdata %p, touch_data %p\n", pdata, touch_data);

        mutex_unlock(&pdata->ioctl_mutex);

        return -ENOMEM;
    }


    ret = 0;
    switch (user_cmd)
    {

        case COMMON_DATA_CMD:
            buf_len  = _IOC_SIZE(cmd);
            addr_flg = 1;

            kernel_log_i(" COMMON_DATA_CMD buf_len %d\n", buf_len);
            break;

        case HARDWARE_PARAM_CMD:
            buf_len  = _IOC_SIZE(cmd);
            addr_flg = 0;
            kernel_log_i(" HARDWARE_PARAM_CMD SIZE %d \n", buf_len);

            break;

        case SELECT_MMAP_CMD:

            addr_flg = 0;
            kernel_log_i(" SELECT_MMAP_CMD  %d\n", (int)arg);
            break;

        case SELECT_TOUCH_ID:

            kernel_log_i(" SELECT_TOUCH_ID  touch_id %d\n", (int)arg);
            buf_len = 0;

            break;

        case GET_FRAME_DATA_INDEX:
            ret =  -EINVAL;

            spin_lock(&touch_pdata->raw_lock);
            if (ev_raw == 0)
            {
                ev_raw = -EINVAL;
                ret = 0;
            }
            press = ev_raw;

            spin_unlock(&touch_pdata->raw_lock);

            kernel_log_e0(" GET_FRAME_DATA_INDEX ret %d  press %d\n", ret, press);

            break;


        case RAW_DATA_INDEX:
            kernel_log_i(" RAW_DATA_INDEX  %d\n", ret);
            break;

        default:
            kernel_log_r(" don't support ioctl_cmd\n");

            kernel_log_r("proc op_type = %x, args = %d\r\n", cmd, (int)arg);
            kernel_log_r("DIR = %x TYPE = %x  cmd = %x SIZE = %x \r\n",
                         _IOC_DIR(cmd),
                         _IOC_TYPE(cmd),
                         _IOC_NR(cmd),
                         _IOC_SIZE(cmd));

            ret = -EINVAL;
            break;
    }

    common_data = (common_data_t *)buf;

    if ((addr_flg == 1) &&
        (buf_len > 0) &&
        (buf_len <=  sizeof(buf)))
    {

        ret = copy_from_user(&buf, (int __user *)argp, buf_len);

        receive_common_data(common_data, &buf_len);
    }

    if ((user_cmd != GET_FRAME_DATA_INDEX) ||
        ((user_cmd == GET_FRAME_DATA_INDEX) && (flg == 0)))
    {
        kernel_log_i("user_cmd:%d, "
                     "cmd:%d, mode:%d, data_len:%d "
                     "data_buf[0]:%d, data_buf[1]:%d"
                     "\n",
                     user_cmd,
                     common_data->cmd,
                     common_data->mode,
                     common_data->data_len,
                     common_data->data_buf[0],
                     common_data->data_buf[1]
                    );

        kernel_log_i("\n");
        kernel_log_i("\n");
    }



    if ((addr_flg == 1) &&
        (buf_len > 0) &&
        (buf_len <=  sizeof(buf)))
    {
        ret = copy_to_user((int __user *)argp, &buf, buf_len);
    }

    mutex_unlock(&pdata->ioctl_mutex);


    THP_CMD_TO_IC_PATH();

    return ret;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int semi_touch_dev_release(struct inode *inode, struct file *filp)
{
    semi_thp_dev.thp_en = THP_EN;

    kernel_log_a("thp_en %d\r\n", semi_thp_dev.thp_en);

    return 0;
}


/**
********************************************************************************
*.
********************************************************************************
*/
static const struct file_operations semi_touch_raw_dev_fops =
{
    .owner          = THIS_MODULE,
    .open           = semi_touch_dev_open,
    .write          = semi_touch_dev_write,
    .read           = semi_touch_dev_read,
    .poll           = semi_touch_dev_poll,
    .mmap           = semi_touch_dev_mmap,
    .unlocked_ioctl = semi_touch_dev_ioctl,
    .compat_ioctl   = semi_touch_dev_ioctl,
    .release        = semi_touch_dev_release,
};

/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int semi_device_create_int(void)
{
    int ret = -1;

    ret = alloc_chrdev_region(&semi_touch_raw_devno, 0, DEV_CNT, DEV_NAME);
    if (ret < 0)
    {
        kernel_log_r("fail to alloc semi_touch_raw_devno\n");
        goto alloc_err;
    }

    touch_raw_dev.owner = THIS_MODULE;
    cdev_init(&touch_raw_dev, &semi_touch_raw_dev_fops);


    ret = cdev_add(&touch_raw_dev, semi_touch_raw_devno, DEV_CNT);
    if (ret < 0)
    {
        kernel_log_r("fail to add cdev\n");
        goto add_err;
    }


    class_semi_touch_raw = class_create(DEV_NAME);

    device_semi_touch_raw = device_create(class_semi_touch_raw,
                                          NULL,
                                          semi_touch_raw_devno,
                                          NULL,
                                          DEV_NAME);

    kernel_log_a("device_create /dev/%s ok\n", DEV_NAME);

    chrdev_init_flag = 1;

    return 0;


add_err:
    unregister_chrdev_region(semi_touch_raw_devno, DEV_CNT);
    kernel_log_r("\n error! \n");

alloc_err:

    return -1;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int semi_touch_proc_init(void)
{

    semi_touch_create_cmd_proc(SEMI_TOUCH_PROC_NAME_RAW);


    kernel_log_a("ok\n\n");

    return 0;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static void  semi_touch_proc_exit(void)
{

    semi_touch_remove_cmd_proc(SEMI_TOUCH_PROC_NAME_RAW);


    kernel_log_a("exit\n\n\n");
}

/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int  semi_touch_dev_init(void)
{
    int error = 0;

    error = semi_device_create_int();

    return error;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int  semi_touch_dev_exit(void)
{
    int error = 0;

    if (class_semi_touch_raw)
    {
        device_destroy(class_semi_touch_raw, semi_touch_raw_devno);
        class_destroy(class_semi_touch_raw);
    }

    if (chrdev_init_flag)
    {
        cdev_del(&touch_raw_dev);
        unregister_chrdev_region(semi_touch_raw_devno, DEV_CNT);
        chrdev_init_flag = 0;
    }

    return error;
}


/**
********************************************************************************
* @brief  :  semi_touch_pdata int
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int semi_touch_pdata_int(void)
{
    int ret = 0;


    int i = 0;
    struct semi_touch_pdata *pdata =  NULL;

    kernel_log_a("enter PAGE_SIZE %d \n", (int)(PAGE_SIZE));


    pdata = kzalloc(sizeof(struct semi_touch_pdata), GFP_KERNEL);
    if (!pdata)
    {
        ret = -ENOMEM;
        kernel_log_r("alloc mem for raw data\n");
        goto pdata_err;

    }


    pdata->raw_data = (unsigned int *)kzalloc(RAW_SIZE, GFP_KERNEL);

    if (!pdata->raw_data)
    {
        ret = -ENOMEM;
        kernel_log_r("alloc mem for raw data\n");
        goto parse_dt_err;
    }


    for (i = 0; i < RAW_BUF_NUM; i++)
    {

        pdata->raw_buf[i] = (unsigned int *)kzalloc(RAW_SIZE, GFP_KERNEL);

        if (!pdata->raw_buf[i])
        {
            ret = -ENOMEM;
            kernel_log_r("alloc mem for raw buf data\n");
            goto parse_dt_err;
        }
    }


    pdata->raw_head = 0;
    pdata->raw_tail = 0;
    pdata->phy_base = virt_to_phys(pdata->raw_data);



    kernel_log_i0(": kernel base:%d, phy base:%d\n",
                  ((unsigned long)pdata->raw_data),
                  ((unsigned long)pdata->phy_base));


    spin_lock_init(&pdata->raw_lock);
    spin_lock_init(&pdata->cmd_lock);
    spin_lock_init(&pdata->input_lock);




    mutex_init(&pdata->ioctl_mutex);
    mutex_init(&pdata->write_mutex);


    pdata->touch_data[0] = (struct semi_touch_interface *)kzalloc(
                               sizeof(struct semi_touch_interface), GFP_KERNEL);
    if (pdata->touch_data[0] == NULL)
    {
        ret = -ENOMEM;
        kernel_log_r("alloc mem for touch_data\n");
        goto sys_group_err;
    }

    pdata->touch_data[1] = (struct semi_touch_interface *)kzalloc(
                               sizeof(struct semi_touch_interface), GFP_KERNEL);
    if (pdata->touch_data[1] == NULL)
    {
        ret = -ENOMEM;
        kernel_log_r("alloc mem for touch_data\n");
        goto sys_group_err;
    }


    pdata->last_touch_events = (struct last_touch_event *)kzalloc(
                                   sizeof(struct last_touch_event), GFP_KERNEL);
    if (pdata->last_touch_events == NULL)
    {
        ret = -ENOMEM;
        kernel_log_r(": alloc mem for last touch evnets\n");
        goto sys_group_err;
    }

    touch_pdata = pdata;

    return ret;


pdata_err:
    if (pdata)
    {
        kfree(pdata);
        pdata = NULL;
        touch_pdata = pdata;
    }


sys_group_err:
    if (pdata->touch_data[0])
    {
        kfree(pdata->touch_data[0]);
        pdata->touch_data[0] = NULL;
    }

    if (pdata->touch_data[1])
    {
        kfree(pdata->touch_data[1]);
        pdata->touch_data[1] = NULL;
    }

    if (pdata->last_touch_events)
    {
        kfree(pdata->last_touch_events);
        pdata->last_touch_events = NULL;
    }


parse_dt_err:
    if (pdata->raw_data)
    {
        kfree(pdata->raw_data);
        pdata->raw_data = NULL;
    }


    for (i = 0; i < RAW_BUF_NUM; i++)
    {
        if (pdata->raw_buf[i])
        {
            kfree(pdata->raw_buf[i]);
            pdata->raw_buf[i] = NULL;
        }
    }


    kernel_log_r("fail!\n");

    return ret;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
static int semi_thp_get_cur_frame_len(struct sm_touch_dev *st_dev)
{
    int ret = -SEMI_DRV_ERR_HAL_IO;
    s16 cur_frame_len;
    s16 cur_frame_len_r;
    u8 check_err = 0;

    st_dev->stc.thp_addr   = st_dev->stc.base_addr;


    if (semi_thp_dev.thp_en == 0) return  SEMI_DRV_ERR_OK;


    ret = semi_touch_read_bytes(st_dev->stc.thp_addr,
                                (unsigned char *)&semi_thp_dev.frame_data,
                                MCAP_THP_FRAM_HEAD);

    check_return_if_fail(ret, NULL);


    cur_frame_len   = (s16)(semi_thp_dev.frame_data.cur_frame_len); //swab16
    cur_frame_len_r = (s16)(semi_thp_dev.frame_data.cur_frame_len_r);

    if (cur_frame_len + cur_frame_len_r != -1)
    {
        check_err = 1;
        goto check_return;
    }

    if (semi_thp_dev.frame_data.crc_len + semi_thp_dev.frame_data.crc_r_len != -1)
    {
        check_err = 2;
        goto check_return;
    }


    semi_thp_dev.frame_data_en = 1;
    if (cur_frame_len < READ_BUF_MAX_SIZE)
    {
        semi_thp_dev.cur_frame_len = cur_frame_len;
    }

    kernel_log_a("get cur_frame_len %d  ok"

                 " code_v %d "

                 " CUR_FRAME_LEN %d \r\n",

                 semi_thp_dev.cur_frame_len,

                 semi_thp_dev.frame_data.code_v,

                 CUR_FRAME_LEN);


check_return:

    if (check_err != 0)
    {
        kernel_log_r("get cur_frame_len err  "
                     " cur_frame_len %d "
                     " cur_frame_len_r %d "
                     " code_v %d "
                     " check_err %d "
                     " CUR_FRAME_LEN %d \r\n",

                     cur_frame_len,
                     cur_frame_len_r,
                     semi_thp_dev.frame_data.code_v,
                     check_err,
                     CUR_FRAME_LEN);
    }


    return ret;
}


/**
********************************************************************************
* @brief  :
* @param  :
*
* @retval :
*
********************************************************************************
*/
int semi_thp_get_thp_ver(char *ver)
{
    char thp_ver[] = CHSC_THP_VERSION;

    return sprintf(ver, "%s\n", thp_ver);
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
int read_and_report_touch_check_crc(unsigned char *readbuffer, unsigned short len)
{
    int ret = 0;
    int read_flag = -SEMI_DRV_ERR_CHECKSUM;
    unsigned short tem = 0;

    static int crc_cnt = 0;


    if (!st_dev.stc.initialize_ok) ret = -SEMI_DRV_ERR_NO_INIT;

    check_return_if_fail(ret, NULL);


    tem = (unsigned short) readbuffer[SEMI_TOUCH_MAX_POINTS * 6 + 3] << 8;
    tem |= readbuffer[SEMI_TOUCH_MAX_POINTS * 6 + 2];
    if (0 == ((caculate_checksum_u816(readbuffer, SEMI_TOUCH_MAX_POINTS * 6 + 2) + tem) & 0xFFFF))
    {
        read_flag = SEMI_DRV_ERR_OK;
    }
    else
    {
        crc_cnt++;
        kernel_log_d("crc err = %d\n", crc_cnt);
    }


    if (SEMI_DRV_ERR_OK != read_flag)
        ret = -SEMI_DRV_ERR_CHECKSUM;

    return ret;
}


/**
********************************************************************************
* @brief  : Monitor to the gesture status of the packet obtained by the driver
* @param  :
*
* @retval :
*
********************************************************************************
*/
void debug_gest_code(rpt_content_t *point)
{
#define GEST_CLICK      0x01
#define GEST_CODE_LS    0x20
#define GEST_CODE_RS    0x21
#define GEST_CODE_US    0x22
#define GEST_CODE_DS    0x23
#define GEST_CODE_DC    180 // 0x24
#define GEST_CODE_SC    0x25
#define GEST_CODE_IS    0x26
#define GEST_CODE_DOWN  0x27    //touch down
#define GEST_CODE_UP    0x28    //touch leave
#define GEST_CODE_PALM  0xDE


    static  u16 DcCnt;      //Double click count
    u16 DcCntflg;   //Double click
    static  u16 gest_cnt;

    //if (LG_ALWAYS > thp_drive_log_level) return;

    DcCntflg = 0;

    if (point->num  == GEST_CODE_DC)   // GEST_CODE_DC
    {
        DcCnt++;

        DcCntflg = 29999;
    }


    if ((point->num  != 0xff) && (point->num >= GEST_CODE_LS))   // GEST_CODE_DC
    {
        gest_cnt++;

    }

    kernel_log_i_9(
        "gest_cnt %d "
        "DcCnt %d "
        "DcCntflg %d "

        "act %d "
        "num %d "


        "id %d "
        "event %d "
        "x %d "
        "y %d "

        "\n",

        gest_cnt,
        DcCnt,
        DcCntflg,

        point->act,
        point->num,


        point->points[0].rp.id,
        point->points[0].rp.event,

        ((u32) point->points[0].rp.x_l8 +
         (((u32)point->points[0].rp.x_h8) << 8)),

        ((u32) point->points[0].rp.y_l8 +
         (((u32)point->points[0].rp.y_h8) << 8))
    );

}

/**
********************************************************************************
* @brief  : 
* @param  :
*
* @retval :
*
********************************************************************************
*/
int thp_report_pos_to_input(unsigned char *readbuffer, unsigned short len)
{
    u8 report_pos_flg = 0;


#if 0
    static u16 crc_err_cnt = 0;



    if (read_and_report_touch_check_crc(readbuffer, len) >= 0)
    {
        touch_point_to_input(readbuffer, len);
    }
    else
    {
        crc_err_cnt++;
        kernel_log_r("crc_err_cnt %d \n",crc_err_cnt);

    }

    kernel_log_i_11("crc_err_cnt %d ,report_cnt %u\n",
                    crc_err_cnt,
                    *((u16*)&readbuffer[SEMI_TOUCH_MAX_POINTS * 6 + 4])

                   );
#else

    touch_point_to_input(readbuffer, 0);
#endif

    return report_pos_flg;
}
/**
********************************************************************************
* @brief  : Extract the point data packet from the THP data packet, parse gestures, interaction commands and point information
            If it is thp firmware, the reported data needs to be set invalid (cannot be set to 0XF8).
            Otherwise, both thp and firmware will report points
* @param  :
*
* @retval :
*
********************************************************************************
*/
int thp_read_report_buf( tp_raw * tp_data)
{
    static u16 crc_err_cnt = 0;

    report_piont_t_semi *preport_piont_t;

    const unsigned char report_size = ((SEMI_TOUCH_MAX_POINTS * 6 + 2) + 8) & 0xfc;
    unsigned char *readbuffer ;
    u8 report_pos_flg = 0; //


    readbuffer      = (unsigned char *)tp_data->frame_data;
    readbuffer      = &readbuffer[FRAME_MC_SC_LEN];
    preport_piont_t = (report_piont_t_semi *)readbuffer;
    readbuffer      = preport_piont_t->readbuffer;


    if (read_and_report_touch_check_crc(readbuffer, report_size) >= 0)
    {
        if (readbuffer[0] == 0xf7)
        {
            readbuffer[0]  = 0xf8;
        }
        else if (readbuffer[0] == 0xf8)
        {
            readbuffer[0]  = 0xf7;
        }

        touch_point_to_input(readbuffer, 1);
    }
    else
    {
        crc_err_cnt++;
        kernel_log_r("crc_err_cnt %d \n",crc_err_cnt);

        return report_pos_flg;
    }

    debug_gest_code((rpt_content_t *)readbuffer);


    if (readbuffer[0] == 0xf8)
    {
        report_pos_flg = 1;
    }

    return report_pos_flg;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
int semi_thp_irq_handler(int irq_cnt)
{
    u8 *readbuffer  = NULL;
    u16 report_size ;
    s64 time;

    if (!touch_pdata)
    {

        kernel_log_r("touch_pdata NULL\n");

        return -ENOMEM;
    }

    if (!touch_pdata->raw_data)
    {

        kernel_log_r("touch_pdata->raw_data NULL\n");

        return -ENOMEM;
    }


    if (semi_thp_dev.thp_get_frame_en != 1)
    {

        if (semi_thp_dev.thp_get_frame_en > 1)
        {
            semi_thp_dev.thp_get_frame_en--;
        }

        kernel_log_r("thp_get_frame_en %d\n",
                     semi_thp_dev.thp_get_frame_en);

        return 0;
    }

    if (semi_thp_dev.cur_frame_len_bak_en == 1)
    {
        semi_thp_dev.cur_frame_len_bak_en = 0;

        if (semi_thp_dev.cur_frame_len_bak > 1)
            semi_thp_dev.cur_frame_len = semi_thp_dev.cur_frame_len_bak;

        kernel_log_w("cur_frame_len %d\n",
                     semi_thp_dev.cur_frame_len);
    }

    report_size = semi_thp_dev.cur_frame_len;
    readbuffer  = (unsigned char *)&semi_thp_dev.frame_data;

    if ((int)read_and_report_touch_raw(readbuffer, report_size, &time) >= 0)
    {

    }
    else
    {
        semi_thp_dev.read_frame_err++;
    }

    raw_data_process(touch_pdata->raw_data, irq_cnt, time, readbuffer, report_size);

    //test_raw_data_process(touch_pdata->raw_data, irq_cnt, time, readbuffer, report_size);


    if (thp_read_report_buf(&semi_thp_dev.frame_data)== 1)
    {
        return 0;
    }


    spin_lock(&touch_pdata->raw_lock);
    ev_raw = 1;
    spin_unlock(&touch_pdata->raw_lock);


    wake_up_interruptible(&touch_waitq);
    wake_up_cnt++;

    return 0;

}

/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
int  semi_thp_driver_init(struct sm_touch_dev *st_dev)
{
    int error = 0;

    kernel_log_a("semi_thp_driver_init %s\r\n", CHSC_THP_VERSION);
    kernel_log_a("thp_en %d\r\n", semi_thp_dev.thp_en);

    error += semi_thp_get_cur_frame_len(st_dev);

    error += semi_touch_pdata_int();


    error += semi_touch_dev_init();

    error += semi_touch_proc_init();


    kernel_log_a("semi_thp_driver_init %d\n", error);


    return error;
}


/**
********************************************************************************
* @brief  :
*
* @param  :
*
* @retval :
*
********************************************************************************
*/
void  semi_thp_driver_exit(void)
{
    semi_touch_dev_exit();

    semi_touch_proc_exit();

    kernel_log_a("semi_thp_driver_exit*********************************\r\n\r\n");

}
#endif
/*********************** (C) COPYRIGHT  ***********END OF FILE ****************/
