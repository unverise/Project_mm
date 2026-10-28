#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include "semi_touch_interface.h"

#if SEMI_TOUCH_BOOTUP_UPDATE_EN
#include "fw_update_packet_v0x51_p0x4.h"
#endif

#define BURN_W                              0x0
#define BURN_R                              0x3
#define CORE_R                              0x4
#define CORE_W                              0x5
#define CHIPSEMI_TS_FW_BIN                  "chipsemi_ts_fw.bin"
#define BOOT_STATUS_BTMODE                  (0x42544C44)  //BTLD
#define BOOT_STATUS_BTGOHD                  (~0x42544C44)  //go

enum MCAP_RAM_CODE
{
    RAM_CODE_BURN_MCAP_SHARE  = 0,
    RAM_CODE_SHORT_DATA_SHARE = 1,
    SEMI_TOUCH_RAM_CODE_COUNT,
};

struct bin_code_chain bin_code_map[SEMI_TOUCH_RAM_CODE_COUNT] =
{
    {fw_burn_mcapshare, sizeof(fw_burn_mcapshare)},
    {fw_short_mcapshare, sizeof(fw_short_mcapshare)},
};

int semi_touch_write_core_data_and_check(unsigned int addr, const unsigned char *buffer, unsigned short len)
{
    int ret = 0, once = 0, index = 0, retry = 0;
    unsigned char core_cmp_buffer[MAX_CORE_WRITE_LEN];
    const int max_try = 3;

    while (len > 0)
    {
        retry = 0;
        do
        {
            ret = SEMI_DRV_ERR_OK;
            once = min(len, (unsigned short)MAX_CORE_WRITE_LEN);
            ret = semi_touch_write_bytes(addr, buffer, once);
            ret = semi_touch_read_bytes(addr, core_cmp_buffer, once);
            for (index = 0; index < once; index++)
            {
                if (core_cmp_buffer[index] != buffer[index])
                {
                    //kernel_log_d("err pos = %d\r\n", index);
                    ret = -SEMI_DRV_ERR_CHECKSUM;
                    break;
                }
            }
            if (SEMI_DRV_ERR_OK == ret)
            {
                break;
            }
        }
        while (++retry < max_try);

        check_return_if_fail(ret, NULL);

        addr += once;
        buffer += once;
        len -= once;
    }

    return ret;
}
int semi_touch_run_ram_code(unsigned char code)
{
    int retry;
    int ret = 0, reg_value = 0;

    if (code >= SEMI_TOUCH_RAM_CODE_COUNT) return -EINVAL;

    for (retry = 0; retry < 5; retry++)
    {
        //reset mcu
        semi_touch_reset(no_report_after_reset);

        //hold mcu
        reg_value = TP_HOLD_MCU_VAL;
        ret = semi_touch_write_bytes(TP_HOLD_MCU_ADDR, (unsigned char *)&reg_value, 4);
        if (ret < 0)
        {
            continue;
        }
        //open auto feed
        reg_value = TP_AUTO_FEED_VAL;
        ret = semi_touch_write_bytes(TP_AUTO_FEED_ADDR, (unsigned char *)&reg_value, 4);
        if (ret < 0)
        {
            continue;
        }

        //run ramcode
        ret = semi_touch_write_core_data_and_check(0x20000000, bin_code_map[code].bin_code_addr, bin_code_map[code].bin_code_len);
        if (ret < 0)
        {
            continue;
        }

        break;
    }
    check_return_if_fail(ret, NULL);

    //remap
    reg_value = TP_REMAP_MCU_VAL;
    ret = semi_touch_write_bytes(TP_REMAP_MCU_ADDR, (unsigned char *)&reg_value, 4);
    check_return_if_fail(ret, NULL);

    //release mcu
    reg_value = TP_RELEASE_MCU_VAL;
    ret = semi_touch_write_bytes(TP_RELEASE_MCU_ADDR, (unsigned char *)&reg_value, 4);
    check_return_if_fail(ret, NULL);

    mdelay(30);

    return 0;
}

int semi_touch_bootup_hold(void)
{
    int ret = 0;
    unsigned int retry = 0;
    unsigned int loop = 0;
    unsigned int cmdcode = 0;
    unsigned int boottype = 0;
    unsigned int holddone = 0;

    cmdcode = (~0x42544C44);
    for (retry = 0; retry < 5; retry++)
    {
        semi_touch_reset(no_report_after_reset);
        for (loop = 0; loop < 30; loop++)
        {
            ret = semi_touch_write_bytes(0x2000001c, (u8*)&cmdcode, 4);
            check_return_if_fail(ret, NULL);
            mdelay(3);
            ret = semi_touch_read_bytes(0x20000018, (u8*)&boottype, 4);
            check_return_if_fail(ret, NULL);
            if ((boottype == 0x42544C44) || (boottype == 0x4F525245)) //already hold int boot mode
            {
                holddone = 1;
                break;
            }
        }
        if (holddone)
        {
            break;
        }
    }

    if (!holddone)
    {
        return -SEMI_DRV_ERR_TIMEOUT;
    }

    cmdcode = 0;
    ret = semi_touch_write_bytes(0x20000018, (u8*)&cmdcode, 4);
    check_return_if_fail(ret, NULL);

    kernel_log_d("pass:hold boot, retry=%d!\r\n", retry);

    return SEMI_DRV_ERR_OK;
}


int semi_touch_run_boot_ram_code(void)
{
    int ret = 0;

    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;
    memset(&cmd_send_tp, 0, sizeof(cmd_send_tp));

    cmd_send_tp.d0 = 0;
    cmd_send_tp.d1 = 0xAA55;//keylow
    cmd_send_tp.d2 = 0xA55A;//keyhigh
    cmd_send_tp.d3 = 0x5550;//magic number
    cmd_send_tp.d4 = 0x444d;//magic number
    cmd_send_tp.id = 0xfe;

    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    if (ret != 0)
    {
        kernel_log_d("cmd error:enter\r\n");
        return ret;
    }

    return ret;
}

int semi_touch_run_boot_ram_code_process(void)
{
    int ret = 0;

    ret = semi_touch_bootup_hold();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_run_boot_ram_code();
    check_return_if_fail(ret, NULL);
    mdelay(10);

    return ret;
}

int semi_touch_run_ram_code_second(unsigned char code)
{
    int retry;
    int ret = 0, reg_value = 0;

    if (code >= SEMI_TOUCH_RAM_CODE_COUNT) return -EINVAL;

    for (retry = 0; retry < 3; retry++)
    {
        //bootload hold
        ret = semi_touch_bootup_hold();
        if (ret < 0)
        {
            continue;
        }

        //hold mcu
        reg_value = TP_HOLD_MCU_VAL;
        ret = semi_touch_write_bytes(TP_HOLD_MCU_ADDR, (unsigned char *)&reg_value, 4);
        if (ret < 0)
        {
            continue;
        }
        //open auto feed
        reg_value = TP_AUTO_FEED_VAL;
        ret = semi_touch_write_bytes(TP_AUTO_FEED_ADDR, (unsigned char *)&reg_value, 4);
        if (ret < 0)
        {
            continue;
        }

        //run ramcode
        ret = semi_touch_write_core_data_and_check(0x20000000, bin_code_map[code].bin_code_addr, bin_code_map[code].bin_code_len);
        if (ret < 0)
        {
            continue;
        }

        break;
    }
    check_return_if_fail(ret, NULL);

    //remap
    reg_value = TP_REMAP_MCU_VAL;
    ret = semi_touch_write_bytes(TP_REMAP_MCU_ADDR, (unsigned char *)&reg_value, 4);
    check_return_if_fail(ret, NULL);

    //release mcu
    reg_value = TP_RELEASE_MCU_VAL;
    ret = semi_touch_write_bytes(TP_RELEASE_MCU_ADDR, (unsigned char *)&reg_value, 4);
    check_return_if_fail(ret, NULL);

    mdelay(30);

    return 0;
}


/*
    This function will put IC into NVM mode, call it carefully and must reset
    the chip before entering normal mode.
*/
int semi_touch_enter_burn_mode(void)
{
    int ret = 0;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;

    cmd_send_tp.id = CMD_IDENTITY;
    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    //if empty chip

    if ((ack_from_tp.d0 == 0xE9A2) && (ack_from_tp.d1 == 0x165d))
    {
        set_status_upgrade_run(st_dev.stc.ctp_run_status);
        return SEMI_DRV_ERR_OK;
    }

#if 0
    ret = semi_touch_run_ram_code(RAM_CODE_BURN_MCAP_SHARE);
    check_return_if_fail(ret, NULL);
#else
    ret = semi_touch_run_boot_ram_code_process();
    #if SEMI_TOUCH_THP_DRIVER_EN
        FORCE_UPDATE_RUN_RAM_CODE();
    #endif
    check_return_if_fail(ret, NULL);
#endif

    cmd_send_tp.id = CMD_IDENTITY;
    ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    if ((ack_from_tp.d0 == 0xE9A2) && (ack_from_tp.d1 == 0x165d))
    {
        set_status_upgrade_run(st_dev.stc.ctp_run_status);
        return SEMI_DRV_ERR_OK;
    }

    set_status_pointing(st_dev.stc.ctp_run_status);
    return -SEMI_DRV_ERR_HAL_IO;
}
int semi_touch_core_write(struct apk_complex_data *apk_comlex_addr)
{
    int ret = -EINVAL;
    struct m_ctp_cmd_std_t *ptr_cmd = (struct m_ctp_cmd_std_t *)apk_comlex_addr->stm_cmd_buffer;
    unsigned short left = ptr_cmd->d1;
    unsigned int addr = (ptr_cmd->d5 << 16) + ptr_cmd->d0;

    if (ptr_cmd->d2 != caculate_checksum_u816((unsigned char *)apk_comlex_addr->stm_txrx_buffer, left))
    {
        return -SEMI_DRV_ERR_CHECKSUM;
    }

    ret = semi_touch_write_bytes(addr, apk_comlex_addr->stm_txrx_buffer, left);

    return ret;
}
int semi_touch_core_read(struct apk_complex_data *apk_comlex_addr)
{
    int ret = -EINVAL;
    struct m_ctp_cmd_std_t *ptr_cmd = (struct m_ctp_cmd_std_t *)apk_comlex_addr->stm_cmd_buffer;
    struct m_ctp_rsp_std_t *driv_rsp = (struct m_ctp_rsp_std_t *)apk_comlex_addr->stm_rsp_buffer;
    unsigned short left = ptr_cmd->d1;
    unsigned int addr = (ptr_cmd->d5 << 16) + ptr_cmd->d0;

    ret = semi_touch_read_bytes(addr, apk_comlex_addr->stm_txrx_buffer, left);
    driv_rsp->d0 = caculate_checksum_u816(apk_comlex_addr->stm_txrx_buffer, left);

    return ret;
}
int semi_touch_burn_write(struct apk_complex_data *apk_comlex_addr)
{
    int ret = -EINVAL;
    int addr = -1;
    unsigned short * ppt = NULL;
    unsigned short left;

    struct m_ctp_cmd_std_t *ptr_cmd = (struct m_ctp_cmd_std_t *)apk_comlex_addr->stm_cmd_buffer;
    struct m_ctp_rsp_std_t *driv_rsp = (struct m_ctp_rsp_std_t *)apk_comlex_addr->stm_rsp_buffer;
    left = ptr_cmd->d1;
    ppt = (unsigned short *)&apk_comlex_addr->stm_txrx_buffer[0x32];

    addr = (ptr_cmd->d5 << 16) + ptr_cmd->d0;

#ifndef  FORCE_UPDATE_EN
    if (0 == addr && *ppt != BOOT_LOAD_MARK)
    {
        kernel_log_e("Burn fw from addr 0x0,may cross the line!\r\n");
        return -EINVAL;
    }
#endif

#if SEMI_TOUCH_THP_DRIVER_EN
    semi_thp_dev.debug_flg = 1;
#endif

    ret = semi_touch_enter_burn_mode();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_write_bytes(TP_WR_BUFF_ADDR, apk_comlex_addr->stm_txrx_buffer, left);
    check_return_if_fail(ret, NULL);

    return cmd_send_to_tp(ptr_cmd, driv_rsp, 7500);
}
int semi_touch_burn_read(struct apk_complex_data *apk_comlex_addr)
{
    int ret = -EINVAL;
    struct m_ctp_cmd_std_t *ptr_cmd = (struct m_ctp_cmd_std_t *)apk_comlex_addr->stm_cmd_buffer;
    struct m_ctp_rsp_std_t ack_from_tp;
    unsigned short left = ptr_cmd->d1;

    ret = semi_touch_enter_burn_mode();
    check_return_if_fail(ret, NULL);

    ret = cmd_send_to_tp(ptr_cmd, &ack_from_tp, 200);
    check_return_if_fail(ret, NULL);

    ret = semi_touch_read_bytes(TP_RD_BUFF_ADDR, apk_comlex_addr->stm_txrx_buffer, left);
    memcpy(apk_comlex_addr->stm_rsp_buffer, &ack_from_tp, sizeof(ack_from_tp));

    return ret;
}
int semi_touch_memory_write(struct apk_complex_data *apk_comlex_addr)
{
    struct m_ctp_cmd_std_t *ptr_cmd = (struct m_ctp_cmd_std_t *)apk_comlex_addr->stm_cmd_buffer;

    if (CORE_W == ptr_cmd->d3)
    {
        return semi_touch_core_write(apk_comlex_addr);
    }
    else
    {
        return semi_touch_burn_write(apk_comlex_addr);
    }
}
int semi_touch_memory_read(struct apk_complex_data *apk_comlex_addr)
{
    struct m_ctp_cmd_std_t *ptr_cmd = (struct m_ctp_cmd_std_t *)apk_comlex_addr->stm_cmd_buffer;

    if (CORE_R == ptr_cmd->d3)
    {
        return semi_touch_core_read(apk_comlex_addr);
    }
    else
    {
        return semi_touch_burn_read(apk_comlex_addr);
    }
}

int semi_touch_bulk_read(unsigned char *pdes, unsigned int adr, unsigned int len)
{
    int ret = -EINVAL;
    unsigned int left = len;
    unsigned int local_check, retry;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;

    cmd_send_tp.id = CMD_MEM_RD;

    while (left)
    {
        len = (left > 1024) ? 1024 : left;

        cmd_send_tp.d0 = adr & 0xffff;
        cmd_send_tp.d1 = len;
        cmd_send_tp.d2 = 0;
        cmd_send_tp.d3 = BURN_R;
        cmd_send_tp.d5 = (adr >> 16) & 0xffff;

        retry = 0;
        while (retry++ < 3)
        {
            ack_from_tp.id = CMD_NA;
            ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 200);
            if (SEMI_DRV_ERR_OK != ret) continue;

            semi_touch_read_bytes(TP_RD_BUFF_ADDR, pdes, len);
            local_check = caculate_checksum_ex(pdes, len);
            if ((ack_from_tp.d0 != (unsigned short)local_check) || (ack_from_tp.d1 != (unsigned short)(local_check >> 16))) continue;

            break;
        }

        adr  += len;
        left -= len;
        pdes += len;
        check_break_if_fail(ret, NULL);
    }

    return ret;
}

int semi_touch_bulk_write(unsigned char *psrc, unsigned int adr, unsigned int len)
{
    int ret = -EINVAL;
    unsigned int left = len;
    unsigned int retry, combChk;
    struct m_ctp_cmd_std_t cmd_send_tp;
    struct m_ctp_rsp_std_t ack_from_tp;

    cmd_send_tp.id = CMD_MEM_WR;

    while (left)
    {
        len     = (left > 1024) ? 1024 : left;
        combChk = caculate_checksum_ex(psrc, len);

        cmd_send_tp.d0 = adr & 0xffff;    /* addrss space[0,64K)  */
        cmd_send_tp.d1 = len;
        cmd_send_tp.d3 = BURN_W;
        cmd_send_tp.d2 = (unsigned short) combChk;
        cmd_send_tp.d4 = (unsigned short) (combChk >> 16);
        cmd_send_tp.d5 = (adr >> 16) & 0xffff;

        retry = 0;
        while (++retry <= 3)
        {
            ret = semi_touch_write_bytes(TP_WR_BUFF_ADDR, psrc, len);
            if (SEMI_DRV_ERR_OK != ret) continue;

            ack_from_tp.id = CMD_NA;
            ret = cmd_send_to_tp(&cmd_send_tp, &ack_from_tp, 7500);
            if (SEMI_DRV_ERR_OK != ret) continue;

            break;
        }

        left -= len;
        adr  += len;
        psrc += len;
        check_break_if_fail(ret, NULL);
    }

    return ret;
}

int semi_touch_get_backup_pid(unsigned long long *id)
{
    int ret = SEMI_DRV_ERR_OK;

    ret = semi_touch_enter_burn_mode();
    check_return_if_fail(ret, NULL);

    ret = semi_touch_bulk_read((unsigned char *)id, VID_PID_BACKUP_ADDR, 8);
    check_return_if_fail(ret, NULL);

    *id = *id & 0xffffffffff;

    return ret;
}

int semi_touch_burn_erase(void)
{
    int ret = SEMI_DRV_ERR_OK;

    //reserver

    return ret;
}

unsigned long long config_to_vid_pid(unsigned char *ptcfg, unsigned int len)
{
    unsigned long long upd_vid_pid;

    upd_vid_pid = ptcfg[5];
    upd_vid_pid = (upd_vid_pid << 8) | ptcfg[4];
    upd_vid_pid = (upd_vid_pid << 8) | ptcfg[3];
    upd_vid_pid = (upd_vid_pid << 8) | ptcfg[2];
    upd_vid_pid = (upd_vid_pid << 8) | ptcfg[1];

    return upd_vid_pid;
}

static int semi_touch_check_cfg_update(unsigned char *parray, unsigned int cfg_single_len, unsigned int cfg_num, unsigned int force_update)
{
    int index, ret = SEMI_DRV_ERR_OK;
    int idx_active;
    unsigned long long upd_vid_pid;
    unsigned char *ptcfg = parray;

    if (0 == cfg_num) return SEMI_DRV_ERR_OK;
    if (st_dev.vid_pid == 0) return SEMI_DRV_ERR_OK;     /* no available version information */

    idx_active = -1;
    for (index = 0; index < cfg_num; index++)
    {
        upd_vid_pid = config_to_vid_pid(ptcfg, cfg_single_len);
        /*BSP: add for firmware version by longtao.ran 20240416 start*/
        st_dev.upd_vid_pid = upd_vid_pid;
        /*BSP: add for firmware version by longtao.ran 20240416 end*/

        if ((st_dev.vid_pid & 0xffffffff00) == (upd_vid_pid & 0xffffffff00))
        {
            kernel_log_d("tp vid_pid = 0x%010llx, udp vid_pid = 0x%010llx\r\n", st_dev.vid_pid, upd_vid_pid);
            if ((st_dev.vid_pid != upd_vid_pid) || force_update) //<
            {
                idx_active = index;
                break;
            }
        }

        ptcfg = ptcfg + cfg_single_len;
    }

    if (idx_active >= 0)
    {
        if (0 == caculate_check_sum_u32((unsigned int *)ptcfg, cfg_single_len))
        {
            kernel_log_d("start update cfg...\r\n");

            ret = semi_touch_enter_burn_mode();
            check_return_if_fail(ret, NULL);

            ret = semi_touch_burn_erase();
            check_return_if_fail(ret, NULL);

            ret = semi_touch_bulk_write(ptcfg, CFG_ROM_ADDRESS, cfg_single_len);
            check_return_if_fail(ret, NULL);

            st_dev.vid_pid = upd_vid_pid;
        }
        else
        {
            ret = -SEMI_DRV_ERR_CHECKSUM;
            kernel_log_e("firmware config checksum error\n");
        }
    }

    return ret;
}

int semi_touch_check_boot_update(unsigned char *pdata, unsigned int len, unsigned long long *vlist, unsigned int n_match, unsigned int force_update)
{
    int ret = SEMI_DRV_ERR_OK;
    int k, idx_active;
    unsigned short upd_boot_ver = 0;

    idx_active = -1;
    upd_boot_ver = (pdata[0x3f] << 8) + pdata[0x3e];
    /*BSP: add for firmware version by longtao.ran 20240416 start*/
    st_dev.upd_boot_ver = upd_boot_ver;
    /*BSP: add for firmware version by longtao.ran 20240416 end*/
    for (k = 0; k < n_match; k++)
    {
        if ((vlist[k] & 0xffffffff00) == (st_dev.vid_pid & 0xffffffff00))
        {
            if ((st_dev.fw_ver != upd_boot_ver) || force_update) //or<
            {
                kernel_log_d("tp vid_pid = 0x%010llx, udp vid_pid = 0x%010llx\r\n", st_dev.vid_pid, vlist[k]);
                idx_active = k;
                break;
            }
        }
    }

    kernel_log_d("tp boot ver = 0x%x, udp boot ver = 0x%x\r\n", st_dev.fw_ver, upd_boot_ver);
    /*
        If we need to compare versions, only update the newer version
    */
    if (idx_active >= 0)
    {
        if (*(unsigned int *)(pdata + len - 4) == caculate_checksum_ex(pdata, len - 4))
        {
            kernel_log_d("start update boot...\r\n");
            ret = semi_touch_enter_burn_mode();
            check_return_if_fail(ret, NULL);

            ret = semi_touch_bulk_write((unsigned char *)pdata, BOOT_ROM_ADDRESS, len);//0x00000000
            check_return_if_fail(ret, NULL);

            st_dev.fw_ver = upd_boot_ver;
        }
        else
        {
            ret = -SEMI_DRV_ERR_CHECKSUM;
            kernel_log_e("firmware boot checksum error\n");
        }
    }

    return ret;
}

/*
    Return Value:
        0x01: reject caller's request
        -1  : some error
        0x00: successfull
*/
int semi_touch_update_updfile(const unsigned char *pdata, unsigned int len, unsigned int force_update)
{
    int ret = SEMI_DRV_ERR_OK;
    unsigned int cfg_single_len = 0, cfg_offset = 0;
    struct chsc_updfile_header *upd_header = (struct chsc_updfile_header *)pdata;

    if (len < sizeof(struct chsc_updfile_header)) return -SEMI_DRV_INVALID_PARAM;
    if (upd_header->sig != 0x43534843) return -SEMI_DRV_ERR_NOT_MATCH;
    if (0 == upd_header->n_cfg) return -SEMI_DRV_INVALID_PARAM;

    cfg_offset = (upd_header->n_cfg * VID_PID_ONE_SPACE_LEN) + sizeof(struct chsc_updfile_header) - VID_PID_ONE_SPACE_LEN;
    cfg_single_len = upd_header->len_cfg / upd_header->n_cfg;

    if ((cfg_offset + upd_header->len_cfg + upd_header->len_boot) != len)  return -SEMI_DRV_INVALID_PARAM;

    //check if config need update
    if (SEMI_DRV_ERR_OK == ret)
    {
        ret = semi_touch_check_cfg_update((unsigned char *)(pdata + cfg_offset), cfg_single_len, upd_header->n_cfg, force_update);
    }

    //check if boot need update
    if (SEMI_DRV_ERR_OK == ret)
    {
        ret = semi_touch_check_boot_update((unsigned char *)(pdata + cfg_offset + upd_header->len_cfg), upd_header->len_boot, upd_header->vlist, upd_header->n_cfg, force_update);
    }

    return ret;
}

int semi_touch_check_and_update(const unsigned char *udp, unsigned int len, unsigned char bootCheckOk)
{
    int ret = -EINVAL;

    if ((0 != st_dev.vid_pid) && (0xffffffffff != st_dev.vid_pid))
    {
        ret = semi_touch_update_updfile(udp, len, bootCheckOk ? 0 : 1);
        check_return_if_fail(ret, NULL);
    }
    else //we don't know what kind if product it is
    {
        ret = -SEMI_DRV_ERR_NOT_MATCH;
        kernel_log_d("opps! what happeded...\r\n");
    }

    return ret;
}
/*
    Get and check boot up information of touch IC
*/

int semi_touch_bootup_update_check(void)
{
    int ret = SEMI_DRV_ERR_OK;
#if SEMI_TOUCH_BOOTUP_UPDATE_EN
    unsigned int iter = 0;
    unsigned char matched = 0, bootCheckOk = 0;
    struct bin_code_chain *firmware = firmware_chain_header;
    struct chsc_updfile_header *udp_header = NULL;

    semi_touch_start_up_check(&bootCheckOk, check_backup_if_fail);
    while (NULL != firmware)
    {
        udp_header = (struct chsc_updfile_header *)firmware->bin_code_addr;
        for (iter = 0; (iter < udp_header->n_cfg) && (iter < 5); iter++)
        {
            kernel_log_d("check if firmware need update, product vid_pid = 0x%010llx, upd vid_pid = 0x%010llx, boot check = %d\r\n", st_dev.vid_pid, *(udp_header->vlist + iter), bootCheckOk);

            if ((st_dev.vid_pid & 0xffffffff00) != (*(udp_header->vlist + iter) & 0xffffffff00)) continue;
            ret = semi_touch_check_and_update(firmware->bin_code_addr, firmware->bin_code_len, bootCheckOk);
            check_break_if_fail(ret, NULL);

            matched = 1;
            break;
        }

        firmware = firmware->next;
        if (matched) break;
    }
#endif

    return ret;
}


#if SEMI_TOUCH_FW_UPDATE_EN
#if 0
static ssize_t show_chsc_fw_update(struct device *dev, struct device_attribute *attr, char *buf)
{
    char *ptr = buf;
    int ret;
    u8 *pupd;
    u8 bootCheckOk = 0;
    const struct firmware *fw = NULL;
    char fwname[50] = { 0 };

    snprintf(fwname, sizeof(fwname), CHIPSEMI_TS_FW_BIN);
    ret = request_firmware(&fw, fwname, &st_dev.client->dev);
    if (ret)
    {
        ptr += sprintf(ptr, "Update fail, unable to open firmware %s\n", fwname);
        return (ptr - buf);
    }
    else
    {
        ptr += sprintf(ptr, "open %s success,firmware size is %d\n", fwname, fw->size);
    }

    /* alloc successive memory */
    pupd = kzalloc(fw->size, GFP_KERNEL);
    if (!pupd)
    {
        ptr += sprintf(ptr, "Update fail, kzalloc %d bytes fail\n", fw->size);
        goto exit;
    }

    memcpy(pupd, fw->data, fw->size);
    if (fw->size > 1024)
    {
        semi_touch_start_up_check(&bootCheckOk, check_backup_if_fail);
        semi_touch_check_and_update(pupd, fw->size, bootCheckOk);
        semi_touch_mode_init(&st_dev);
    }
    kfree(pupd);

exit:

    release_firmware(fw);
    return (ptr - buf);

}

static ssize_t store_chsc_fw_update(struct device *dev,
                                    struct device_attribute *attr, const char *buf, size_t count)
{

    return -EPERM;
}

static DEVICE_ATTR(chsc_fw_update, 0664, show_chsc_fw_update, store_chsc_fw_update);

static struct attribute *chsc_attrs[] =
{
    &dev_attr_chsc_fw_update.attr,
    NULL,
};

static struct attribute_group chsc_attr_group =
{
    .attrs = chsc_attrs,
};
#endif

int semi_touch_fw_update_check(struct sm_touch_dev *st_dev)
{
#if 0
    struct hal_device *client = st_dev->client;
    int ret = SEMI_DRV_ERR_OK;
    ret = sysfs_create_group(&client->dev.kobj, &chsc_attr_group);
    check_return_if_fail(ret, NULL);
#endif
    int ret;
    u8 *pupd = NULL;
    u8 bootCheckOk = 0;
    const struct firmware *fw = NULL;
    char fwname[50] = { 0 };
    struct chsc_updfile_header* udp_header = NULL;

    semi_touch_start_up_check(&bootCheckOk, check_backup_if_fail);
    snprintf(fwname, sizeof(fwname), CHIPSEMI_TS_FW_BIN);
    ret = request_firmware(&fw, fwname, &st_dev->client->dev);
    if (ret)
    {
        kernel_log_d("Update fail, unable to open firmware %s\n", fwname);
        return SEMI_DRV_ERR_OK;
    }

    kernel_log_d("open %s success,firmware size is %lu\n", fwname, fw->size);

    if (fw->size > MIN_FW_SIZE)
    {
        pupd = kzalloc(fw->size, GFP_KERNEL);
        if (!pupd)
        {
            kernel_log_d("Update fail, kzalloc %lu bytes for %s failed\n", fw->size, fwname);
            goto exit;
        }

        memcpy(pupd, fw->data, fw->size);

        udp_header = (struct chsc_updfile_header*)pupd;

        kernel_log_d("Check if firmware need update, product vid_pid = 0x%010llx, upd vid_pid = 0x%010llx, boot check = %d\r\n",
                    st_dev->vid_pid, *(udp_header->vlist), bootCheckOk);

        if ((st_dev->vid_pid & 0xffffffff00) == (*(udp_header->vlist) & 0xffffffff00))
        {
            semi_touch_check_and_update(pupd, fw->size, bootCheckOk);
        }
    }
    else
    {
        kernel_log_d("%s size too small (%lu)\n", fwname, fw->size);
    }

exit:

    if(pupd != NULL)
    {
        kfree(pupd);
    }
    if (fw != NULL)
    {
        release_firmware(fw);
    }

    return SEMI_DRV_ERR_OK;
}
#else
int semi_touch_fw_update_check(struct sm_touch_dev *st_dev)
{
    return SEMI_DRV_ERR_OK;
}
#endif

int semi_touch_online_update_check(char *file_path)
{
#if SEMI_TOUCH_ONLINE_UPDATE_EN
    mm_segment_t old_fs;
    unsigned int file_size, file_sig;
    const unsigned char *file_buffer = NULL;
    static struct file *file = NULL;
    loff_t fpos = 0;
    unsigned char bootCheckOk = 0;

    file = filp_open(file_path, O_RDWR, 0);
    if (IS_ERR(file))
    {
        kernel_log_d("open file %s error\n", file_path);
        return -EINVAL;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    fpos = 0;
    vfs_llseek(file, 0, SEEK_SET);
    kernel_read(file, (char *)&file_sig, sizeof(file_sig), &fpos); //vfs_read
    if (0x43534843 != file_sig)
    {
        filp_close(file, NULL);
        kernel_log_d("file validation check fail...\n");
        return -EINVAL;
    }

    file_sig = 0;
    file_size = vfs_llseek(file, 0, SEEK_END);
    file_buffer = kmalloc(file_size, GFP_KERNEL);

    fpos = 0;
    vfs_llseek(file, 0, SEEK_SET);
    if (file_size == kernel_read(file, (char *)file_buffer, file_size, &fpos))   //vfs_read
    {
        semi_touch_start_up_check(&bootCheckOk, check_backup_if_fail);
        semi_touch_check_and_update(file_buffer, file_size, bootCheckOk);
        semi_touch_mode_init(&st_dev);

        //file->f_op->llseek(file, 0, SEEK_SET);
        //file->f_op->write(file, (char*)&file_sig, sizeof(file_sig), &file->f_pos);
    }

    kfree(file_buffer);
    set_fs(old_fs);
    filp_close(file, NULL);
#endif //SEMI_TOUCH_ONLINE_UPDATE_EN

    return SEMI_DRV_ERR_OK;
}

