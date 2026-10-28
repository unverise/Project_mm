#include <linux/delay.h>
#include <linux/slab.h>
//#include <linux/uaccess.h>
#include <linux/fs.h>
//#include <uapi/asm-generic/fcntl.h>
#include "semi_touch_test_5xxx.h"
#include "semi_touch_interface.h"

#if SEMI_TOUCH_FACTORY_TEST_EN

extern int semi_touch_run_ram_code(unsigned char code);

#define semi_touch_log_file(fmt, ...) do{ memset(gFactory.catch_buffer, 0, sizeof(gFactory.catch_buffer)); sprintf(gFactory.catch_buffer, fmt, ##__VA_ARGS__); }while(0)

#define MAX_TX_NUM_5xxx                            20
#define MAX_RX_NUM_5xxx                            42
#define MAX_CAP_DATA_SIZE                        (MAX_TX_NUM_5xxx * MAX_RX_NUM_5xxx * 2)
//#define SAVE_LOG_NAME                            "/sdcard/chsc_factory_test_result.txt"

struct factory_test_init
{
    unsigned char rowsCnt;
    unsigned char colsCnt;
    unsigned short sensor_2_ic_map[MAX_RX_NUM_5xxx + MAX_TX_NUM_5xxx];
    char catch_buffer[256];
    unsigned char read_buffer[MAX_CAP_DATA_SIZE];
    unsigned long long vid_pid;
    struct file * file;
    loff_t pos;
};

static struct factory_test_init gFactory;

int semi_touch_test_prepare(void)
{
    int ret = 0, index = 0;
    //unsigned char read_buffer[256];
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    close_esd_function(st_dev.stc.custom_function_en);
    #if 0
    gFactory.pos = 0;
    gFactory.file = filp_open(SAVE_LOG_NAME, O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (IS_ERR(gFactory.file))
    {
        kernel_log_d("open file %s error\n", SAVE_LOG_NAME);
        return -EINVAL;
    }
    else if (NULL == gFactory.file)
    {
        kernel_log_d("open file %s error\n", SAVE_LOG_NAME);
        return -EINVAL;
    }
    #endif
    msleep(50);
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    cmd_send_tp.id = CMD_IDENTITY;
    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 2000);
    check_return_if_fail(ret, NULL);

    if((ack_from_tp.d0 == 0xE902) && (ack_from_tp.d1 == 0x16fd))
    {
        ret = semi_touch_read_bytes(0x20000080, gFactory.read_buffer, 256);
        check_return_if_fail(ret, NULL);
        memcpy(&gFactory.vid_pid, &gFactory.read_buffer[1], 5);

        gFactory.rowsCnt = gFactory.read_buffer[0x1a];
        gFactory.colsCnt = gFactory.read_buffer[0x19];
        for(index = 0; index < MAX_TX_NUM_5xxx; index++)
        {
            gFactory.sensor_2_ic_map[index] = gFactory.read_buffer[0x50 + index];
        }
        for(index = 0; index < MAX_RX_NUM_5xxx; index++)
        {
            gFactory.sensor_2_ic_map[index + MAX_TX_NUM_5xxx] = gFactory.read_buffer[0x64 + index];
        }
    }
    else
    {
        ret = -SEMI_DRV_ERR_NO_INIT;
    }

    kernel_log_d("row = %d, col = %d, product vid_pid_cfg = 0x%010llx\n",
                 gFactory.rowsCnt, gFactory.colsCnt, gFactory.vid_pid);

    return ret;
}

unsigned int semi_touch_log_file_imp(char* rxbuff, char* sztext)
{
#if 0
    mm_segment_t old_fs;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    if(!IS_ERR(gFactory.file))
    {
        //gFactory.file->f_op->llseek(gFactory.file, 0, SEEK_END);
        //gFactory.file->f_op->write(gFactory.file, sztext, strlen(sztext), &gFactory.file->f_pos);

        kernel_write(gFactory.file, sztext, strlen(sztext), &gFactory.pos);
    }

    set_fs(old_fs);
#endif
    memcpy(rxbuff, sztext, strlen(sztext));

    return strlen(sztext);
}

void semi_touch_print_matrix(short* matrix, int rows, int cols, char* tmpbuff, int* len)
{
    int row = 0, col = 0;
    char* szResult = tmpbuff;
    for(row = 0; row < rows; row++)
    {
        for(col = 0; col < cols; col++)
        {
            semi_touch_log_file("%-6d", *matrix);
            szResult += semi_touch_log_file_imp(szResult, gFactory.catch_buffer);
            *len = szResult - tmpbuff;
            matrix++;
        }
        semi_touch_log_file("\n");
        szResult += semi_touch_log_file_imp(szResult, gFactory.catch_buffer);
        *len = szResult - tmpbuff;
    }
    semi_touch_log_file("\n");
    szResult += semi_touch_log_file_imp(szResult, gFactory.catch_buffer);
    *len = szResult - tmpbuff;
}

int semi_touch_rawdata_test(char* tmpbuff, int* len)
{
    int ret = 0, raw_averate = 0, failer_cnt = 0;
    int row = 0, col = 0, channels = 0, index = 0;
    int resultlen;
    short *rawdata_p = NULL;
    short *max_p = st_dev.rawdata_max, *min_p = st_dev.rawdata_min;
    unsigned short *pTail = NULL;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    char* aResult = tmpbuff;
    char* bResult = tmpbuff;
    char* cResult = tmpbuff;

    cmd_send_tp.id = 0x04;
    cmd_send_tp.d0 = 0x03;
    cmd_send_tp.d1 = 0x01;
    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);
    msleep(50);

    semi_touch_log_file("\n----------------------Product vid_pid_cfg is 0x%010llx----------------------\n",
                        gFactory.vid_pid);
    aResult += semi_touch_log_file_imp(aResult, gFactory.catch_buffer);

    semi_touch_log_file("\n\n------------------------------MCap RawData Test------------------------------\n\n");
    aResult += semi_touch_log_file_imp(aResult, gFactory.catch_buffer);
    *len = aResult - tmpbuff;
    channels = gFactory.rowsCnt * gFactory.colsCnt + gFactory.rowsCnt + gFactory.colsCnt;
    for(index = 0; index < 5; index++)
    {
        cmd_send_tp.id = CMD_CTP_SSCAN;
        cmd_send_tp.d0 = 0;
        ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
        check_return_if_fail(ret, NULL);

        msleep(50);

        ret = semi_touch_read_bytes(st_dev.stc.rawdata_addr, gFactory.read_buffer, (channels * 2 + 6 + 3) & 0xfffc);
        check_return_if_fail(ret, NULL);

        pTail = (unsigned short*)&gFactory.read_buffer[channels * 2];
        if(*pTail + *(pTail + 1) == 0xffff)
        {
            if(*pTail == caculate_checksum_u16( (unsigned short*)gFactory.read_buffer, channels * 2 ))
            {
                break;
            }
        }

        cmd_send_tp.id = CMD_CTP_SSCAN;
        cmd_send_tp.d0 = 1;
        ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
        check_return_if_fail(ret, NULL);
    }

    rawdata_p = (short*)gFactory.read_buffer;
    semi_touch_log_file("mcap_rawdata:\n");
    aResult += semi_touch_log_file_imp(aResult, gFactory.catch_buffer);
    *len = aResult - tmpbuff;
    bResult += (*len);
    semi_touch_print_matrix(rawdata_p, gFactory.rowsCnt, gFactory.colsCnt, bResult, &resultlen);
    *len +=resultlen;
    cResult += (*len);

    for(index = 0; index < gFactory.rowsCnt * gFactory.colsCnt; index++)
    {
        raw_averate += *rawdata_p;
        rawdata_p++;
    }
    raw_averate /= (gFactory.rowsCnt * gFactory.colsCnt);

    semi_touch_log_file("\navage = %d\n", raw_averate);
    cResult += semi_touch_log_file_imp(cResult, gFactory.catch_buffer);
    *len = cResult - tmpbuff;

    rawdata_p = (short*)gFactory.read_buffer;
    for(row = 0; row < gFactory.rowsCnt; row++)
    {
        for(col = 0; col < gFactory.colsCnt; col++)
        {
            //not realy used node
            if ((row == st_dev.invalid_row_col[0] && col == st_dev.invalid_row_col[1]) ||
                (row == st_dev.invalid_row_col[2] && col == st_dev.invalid_row_col[3]) ||
                (row == st_dev.invalid_row_col[4] && col == st_dev.invalid_row_col[5]) ||
                (row == st_dev.invalid_row_col[6] && col == st_dev.invalid_row_col[7]))
            {
            }
            else
            {
                if(*rawdata_p > *max_p)
                {
                    failer_cnt++;
                    semi_touch_log_file("node(%d, %d) out of range: %d(%d-%d)\n", row, col, *rawdata_p, *min_p, *max_p);
                    cResult += semi_touch_log_file_imp(cResult, gFactory.catch_buffer);
                    *len = cResult - tmpbuff;
                }
                else if(*rawdata_p < *min_p)
                {
                    failer_cnt++;
                    semi_touch_log_file("node(%d, %d) out of range: %d(%d-%d)\n", row, col, *rawdata_p, *min_p, *max_p);
                    cResult += semi_touch_log_file_imp(cResult, gFactory.catch_buffer);
                    *len = cResult - tmpbuff;
                }
            }

            min_p++;
            max_p++;
            rawdata_p++;
        }
    }

    if(failer_cnt)
    {
        semi_touch_log_file("\nrawdata test fail\n");
        cResult += semi_touch_log_file_imp(cResult, gFactory.catch_buffer);
        *len = cResult - tmpbuff;
        return 1;
    }
    else
    {
        semi_touch_log_file("\nrawdata test pass\n");
        cResult += semi_touch_log_file_imp(cResult, gFactory.catch_buffer);
        *len = cResult - tmpbuff;
        return 0;
    }
}

int semi_touch_short_test(char* tmpbuff, int* len)
{
    int ret = 0, tx = 0, rx = 0, loop = 0, failer_cnt = 0;
    int resulttlen;
    int resultrlen;
    short *short_p = NULL, ic_channle = 0;
    unsigned int u32_para_buff[4];
    short short_maxtrix[2][MAX_RX_NUM_5xxx];
    char* aResult = tmpbuff;
    char* bResult = tmpbuff;
    char* cResult = tmpbuff;
    char* dResult = tmpbuff;

    semi_touch_log_file("\n\n------------------------------Short Test------------------------------\n\n");
    aResult += semi_touch_log_file_imp(aResult, gFactory.catch_buffer);
    *len = aResult - tmpbuff;

    ret = semi_touch_run_ram_code(1/*RAM_CODE_SHORT_DATA_SHARE*/);
    check_return_if_fail(ret, NULL);

    for(loop  = 0; loop < 20; loop++)
    {
        msleep(30);
        u32_para_buff[0] = 0;

        semi_touch_read_bytes(0x20000014, (unsigned char *)u32_para_buff, 12);
        if(0x54000000 == u32_para_buff[0])
        {
            break;
        }
    }

    if(0x54000000 == u32_para_buff[0])
    {
        loop = 1;
        semi_touch_write_bytes(0x20000010, (unsigned char *)&loop, 4 );
        msleep(30);
        ret = semi_touch_read_bytes(u32_para_buff[1], gFactory.catch_buffer, (MAX_TX_NUM_5xxx + MAX_RX_NUM_5xxx) * 2);
        check_return_if_fail(ret, NULL);

        u32_para_buff[3] = caculate_checksum_ex((unsigned char *)gFactory.catch_buffer, (MAX_TX_NUM_5xxx + MAX_RX_NUM_5xxx)*2);

        if(u32_para_buff[3] != u32_para_buff[2])
        {
            msleep(10);
            semi_touch_read_bytes(0x20000014, (unsigned char *)u32_para_buff, 12 );
            semi_touch_read_bytes(u32_para_buff[1], (unsigned char *)gFactory.catch_buffer, (MAX_TX_NUM_5xxx + MAX_RX_NUM_5xxx)*2);
            u32_para_buff[3] = caculate_checksum_ex((unsigned char *)gFactory.catch_buffer, (MAX_TX_NUM_5xxx + MAX_RX_NUM_5xxx)*2);
        }

        if(u32_para_buff[3] == u32_para_buff[2])
        {
            short_p = (short*)gFactory.catch_buffer;
            for(tx = 0; tx < gFactory.colsCnt; tx++)
            {
                ic_channle = gFactory.sensor_2_ic_map[tx];
                short_maxtrix[0][tx] = short_p[ic_channle];
            }
            for(rx = 0; rx < gFactory.rowsCnt; rx++)
            {
                ic_channle = gFactory.sensor_2_ic_map[rx + MAX_TX_NUM_5xxx];
                short_maxtrix[1][rx] = short_p[ic_channle + MAX_TX_NUM_5xxx];
            }

            bResult += (*len);
            semi_touch_print_matrix(&short_maxtrix[0][0], 1, gFactory.colsCnt, bResult, &resulttlen);
            *len +=resulttlen;
            cResult += (*len);
            semi_touch_print_matrix(&short_maxtrix[1][0], 1, gFactory.rowsCnt, cResult, &resultrlen);
            *len +=resultrlen;
            dResult += (*len);

            for(tx = 0; tx < gFactory.colsCnt; tx++)
            {
                if (short_maxtrix[0][tx] > st_dev.short_limits)
                {
                    failer_cnt++;
                    semi_touch_log_file("node(%d, %d) out of range: %d\n", 0, tx, short_maxtrix[0][tx]);
                    dResult += semi_touch_log_file_imp(dResult, gFactory.catch_buffer);
                    *len = dResult - tmpbuff;
                }
            }
            for(rx = 0; rx < gFactory.rowsCnt; rx++)
            {
                if (short_maxtrix[1][rx] > st_dev.short_limits)
                {
                    failer_cnt++;
                    semi_touch_log_file("node(%d, %d) out of range: %d\n", 1, rx, short_maxtrix[1][rx]);
                    dResult += semi_touch_log_file_imp(dResult, gFactory.catch_buffer);
                    *len = dResult - tmpbuff;
                }
            }

        }
        else
        {
            failer_cnt++;
        }

    }
    else
    {
        failer_cnt++;
    }

    if(failer_cnt)
    {
        semi_touch_log_file("\nshort test fail\n");
        dResult += semi_touch_log_file_imp(dResult, gFactory.catch_buffer);
        *len = dResult - tmpbuff;
        return 2;
    }
    else
    {
        semi_touch_log_file("\nshort test pass\n");
        dResult += semi_touch_log_file_imp(dResult, gFactory.catch_buffer);
        *len = dResult - tmpbuff;
        return 0;
    }
}

void semi_touch_factory_test_over(void)
{
    #if 0
    if(!IS_ERR(gFactory.file))
    {
        filp_close(gFactory.file, NULL);
    }
    #endif
    semi_touch_reset(do_report_after_reset);

    open_esd_function(st_dev.stc.custom_function_en);
}

int semi_tran_tp_selftest(void)
{
#if SEMI_TOUCH_FACTORY_TEST_EN
    int ret;
    __attribute__((unused)) int count;
    int read_len = 0;
    char* ker_buf = NULL;
    char* szCopy = NULL;
    char* rzCopy = NULL;

    ker_buf = kmalloc(16384, GFP_KERNEL);
    memset(ker_buf, 0, 16384);
    szCopy = ker_buf;
    rzCopy = ker_buf;

    ret = semi_touch_start_factory_test(szCopy,&read_len);
    rzCopy += read_len;
    count = sprintf(rzCopy, "TestResult is %s\n", (0 != ret) ? "Failed" : "Pass");
    count += read_len;

    kfree(ker_buf);
    ker_buf = NULL;
    szCopy = NULL;
    rzCopy = NULL;

    return ret ;
#else
    return -EPERM;
#endif
}


int semi_touch_start_factory_test(char*filebuff, int*lenth)
{
    int ret = 0;
    int readraw_len = 0 ;
    int readshort_len = 0 ;
    char* rzResult = filebuff;
    char* szResult = filebuff;

    if(0 == ret)
        ret = semi_touch_test_prepare();

    if(0 == ret)
        ret = semi_touch_rawdata_test(rzResult, &readraw_len);
    szResult += readraw_len;

    if(0 == ret)
        ret = semi_touch_short_test(szResult, &readshort_len);

    semi_touch_factory_test_over();

    *lenth = readraw_len + readshort_len;
    // if(0 == ret)
    //     sprintf(detail, "rawdata test = %s\n, short test = %s\n", "PASS", "PASS");
    // else if(1 == ret)
    //     sprintf(detail, "rawdata test = %s\n", "NG");
    // else if(2 == ret)
    //     sprintf(detail, "short test = %s\n", "NG");
    // else
    //     sprintf(detail, "exception, code = %d\n", ret);

    return ret;

}

#endif
