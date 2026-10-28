/******************************************************************************
 *
 * Filename:
 * ---------
 *     hi3231qx6855mipi_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *-----------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 *
 * Version:  V20230322112910 by GC-S-TEAM
 *
 ******************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "hi3231qx6855mipiraw_Sensor.h"
#include "hi3231qx6855_sensor_setting.h"
/************************** Modify Following Strings for Debug **************************/
#define PFX "hi3231qx6855_camera_sensor"
#define LOG_1 LOG_INF("HI3231QX6855, MIPI 4LANE\n")
/****************************   Modify end    *******************************************/
#define HI3231QX6855_DEBUG 0
#if HI3231QX6855_DEBUG
#define LOG_INF(format, args...)    pr_info(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif
#define per_frame 1
#define HI3231QX6855_QBGC_OPC_CALIB   1
#ifdef HI3231QX6855_QBGC_OPC_CALIB
#define HYNIX_SIZEOF_I2C_BUF 254
#define HI3231QX6855_OTP_QBGC_LEN        768
#define HI3231QX6855_OTP_OPC_LEN         1280
#define XTC_START_ADDR   0x07D3
#define XTC_DATA_LEN     (HI3231QX6855_OTP_QBGC_LEN + HI3231QX6855_OTP_OPC_LEN)
#define QBGC_START_ADDR  0x07D3
#define OPC_START_ADDR   0x0AD3
static kal_uint8  QBGCbuffer[HI3231QX6855_OTP_QBGC_LEN] = {0};
static kal_uint8  OPCbuffer[HI3231QX6855_OTP_OPC_LEN+2] = {0}; //sw remosaic lib need
static kal_uint16 QBGC_setting_burst[HI3231QX6855_OTP_QBGC_LEN];
extern bool _read_hi3231qx6855_eeprom(kal_uint16 addr, BYTE *data, int size);
#endif
static kal_uint16 fixed_framerate;
static DEFINE_SPINLOCK(imgsensor_drv_lock);
static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = HI3231QX6855_SENSOR_ID,
    .checksum_value = 0x959c80e5,
    .pre = {
        .pclk = 128000000,
        .linelength =  1616,
        .framelength = 2640,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 576000000,//1440*4/10
    },
    .cap = {
        .pclk = 128000000,
        .linelength =  1616,
        .framelength = 2640,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 576000000,//1440*4/10
    },
    .normal_video = {
        .pclk = 128000000,
        .linelength =  1616,
        .framelength = 2640,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 1840,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 576000000,//1440*4/10
    },
    .hs_video = {
        .pclk = 128000000,
        .linelength =  808,
        .framelength = 1318,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1632,
        .grabwindow_height = 1224,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 1200,
        .mipi_pixel_rate = 320000000,//800*4/10
    },
    .slim_video = {
        .pclk = 128000000,
        .linelength =  808,
        .framelength = 1318,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1632,
        .grabwindow_height = 1224,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 1200,
        .mipi_pixel_rate = 320000000,//800*4/10
    },
    .custom1 = {
        .pclk = 128000000,
        .linelength =  1616,
        .framelength = 2640,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 576000000,//1440*4/10
    },
    .custom2 = {
        .pclk = 128000000,
        .linelength =  808,
        .framelength = 2640,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 1840,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 600,
        .mipi_pixel_rate = 576000000,//1440*4/10
    },
    .custom3 = {
        .pclk = 128000000,
        .linelength = 1616,
        .framelength = 5277,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 6528,
        .grabwindow_height = 4896,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 150,
        .mipi_pixel_rate = 550400000,//1376*4/10
    },
    .margin = 8,
    .min_shutter = 8,
    .min_gain = 64,
    .max_gain = 4096,
    .min_gain_iso = 100,
    .gain_step = 8,//64/16=4
    .gain_type = 3,
    .max_frame_length = 0xFFFFFFFF,
    .ae_shut_delay_frame = 0,
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2,
    .ihdr_support = 0,
    .ihdr_le_firstline = 0,
    .sensor_mode_num = 8,
    .cap_delay_frame = 2,
    .pre_delay_frame = 2,
    .video_delay_frame = 2,
    .frame_time_delay_frame = 1,
    .hs_video_delay_frame = 3,
    .slim_video_delay_frame = 3,
    .custom1_delay_frame = 2,
    .custom2_delay_frame = 2,
    .custom3_delay_frame = 2,
    .isp_driving_current = ISP_DRIVING_6MA,
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2,
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gb,
    .mclk = 24,
    .mipi_lane_num = SENSOR_MIPI_4_LANE,
    .i2c_addr_table = {0x46,0xff},
    .i2c_speed = 1000,
};
static struct imgsensor_struct imgsensor = {
    .mirror = IMAGE_HV_MIRROR,
    .sensor_mode = IMGSENSOR_MODE_INIT,
    .shutter = 0x0100,
    .gain = 0xe0,
    .dummy_pixel = 0,
    .dummy_line = 0,
    .current_fps = 300,
    .autoflicker_en = KAL_FALSE,
    .test_pattern = KAL_FALSE,
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .ihdr_en = 0,
    .i2c_write_id = 0x46,
};
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[8] = {
    {6592, 4960, 32,   32, 6528, 4896, 3264, 2448,   0,   0, 3264, 2448, 0, 0, 3264, 2448},
    {6592, 4960, 32,   32, 6528, 4896, 3264, 2448,   0,   0, 3264, 2448, 0, 0, 3264, 2448},
    {6592, 4960, 32,   32, 6528, 4896, 3264, 2448,   0, 304, 3264, 1840, 0, 0, 3264, 1840},
    {6592, 4960, 16,   32, 6560, 4896, 1640, 1224,   4,   0, 1632, 1224, 0, 0, 1632, 1224},
    {6592, 4960, 16,   32, 6560, 4896, 1640, 1224,   4,   0, 1632, 1224, 0, 0, 1632, 1224},
    {6592, 4960, 32,   32, 6528, 4896, 3264, 2448,   0,   0, 3264, 2448, 0, 0, 3264, 2448},
    {6592, 4960, 32,   32, 6528, 4896, 3264, 2448,   0, 304, 3264, 1840, 0, 0, 3264, 1840},
    {6592, 4960, 32,   32, 6528, 4896, 6528, 4896,   0,   0, 6528, 4896, 0, 0, 6528, 4896},
};
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte = 0;
    char pu_send_cmd[2] = {
        (char)((addr >> 8) & 0xff),
        (char)(addr & 0xff)
    };
    iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
    return get_byte;
}
static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[4] = {
        (char)((addr >> 8) & 0xff),
        (char)(addr & 0xff),
        (char)((para >> 8) & 0xff),
        (char)(para & 0xff)
    };
    iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id);
}
#define MULTI_WRITE 1
#if MULTI_WRITE
static const int I2C_BUFFER_LEN = 1020; /*trans# max is 255, each 4 bytes*/
#else
static const int I2C_BUFFER_LEN = 4;
#endif
#if 0
static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
    char puSendCmd[1020];
    kal_uint32 tosend, IDX;
    kal_uint16 addr = 0, addr_last = 0, data;
    tosend = 0;
    IDX = 0;
    while (len > IDX) {
        addr = para[IDX];
        if(tosend == 0)
        {
            puSendCmd[tosend++] = (char)(addr >> 8);
            puSendCmd[tosend++] = (char)(addr & 0xFF);
            data = para[IDX + 1];
            puSendCmd[tosend++] = (char)(data >> 8);
            puSendCmd[tosend++] = (char)(data & 0xFF);
            IDX += 2;
            addr_last = addr;
        }
        else if ( addr == addr_last || addr == addr_last + 2)
        {
            data = para[IDX + 1];
            puSendCmd[tosend++] = (char)(data >> 8);
            puSendCmd[tosend++] = (char)(data & 0xFF);
            IDX += 2;
            addr_last = addr;
        }
        if (tosend >= I2C_BUFFER_LEN
                || IDX == len || addr != addr_last) {
            //   LOG_INF("IDX %d, tosend %d ,addr_last 0x%x,addr 0x%x\n", IDX, tosend, addr_last, addr);
            iBurstWriteReg_multi(puSendCmd,
                    tosend,
                    imgsensor.i2c_write_id,
                    tosend,
                    imgsensor_info.i2c_speed);
            tosend = 0;
        }
    }
    return 0;
}
#endif
static kal_uint16 mix_write_cmos_sensor(
	kal_uint16 *para, kal_uint32 len)
{
	//char puSendCmd[1020];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	//[ITD]add i2c continue mode transfer. hong.zhang@transsion.com, 2024-2-29, start.
	#define MAXLEN_CONTINUEDATA (1020+1020)
	//char puSdContinCmd[MAXLEN_CONTINUEDATA];
    char *puSendCmd = kmalloc(1020, GFP_KERNEL);
    char *puSdContinCmd = kmalloc(MAXLEN_CONTINUEDATA, GFP_KERNEL);
    if (!puSendCmd || !puSdContinCmd) {
        kfree(puSendCmd);
        kfree(puSdContinCmd);
        return -ENOMEM; // ???????????????????
    }
	
	kal_uint32 continlen=0, item=0, nextItem;
	kal_uint32 continStartItem=0, continCount=0;
	kal_uint32 unContinStartItem=0, unContinCount=0;
	kal_uint32 continNextAddr=0, continCurAddr=0;
	kal_uint32 dataOffset;
	kal_uint8 endflag=0;
	kal_uint8 lastContinStat=0, continStat=0;//0:unkonw 1:continue 2:uncontinue
	kal_uint16 *pRegSrc;
	kal_uint16 tempdata;
	dataOffset = sizeof(para[0]);
	LOG_INF("dataOffset=%d.\n", dataOffset);
//[ITD]add i2c continue mode transfer. hong.zhang@transsion.com, 2024-2-29, end.
	tosend = 0;
	IDX = 0;
//[ITD]add i2c continue mode transfer. hong.zhang@transsion.com, 2024-2-29, start.
	//while (len > IDX) {
	for (item=0; item<len; item=nextItem) {
		continCurAddr = para[item];
		nextItem = item+2;
		if (nextItem < len)
		{
			continNextAddr = para[nextItem];
		}
		else
		{
			endflag = 1;
		}
		if (endflag == 0)
		{
			if ((continCurAddr+dataOffset) == continNextAddr)
			{
				if (continStat != 1)
				{
					//conintue first addr item.
					continCount = 2;
					continStartItem = item;
					continStat = 1;
				}
				continCount += 2;
			}
			else
			{
				if (continStat == 0)
				{
					//unconintue first addr item.
					unContinCount = 0;
					unContinStartItem = item;
					continStat = 2;
					unContinCount += 2;
				}
				else if (continStat == 1)
				{
					//last continue cmd has in continCount, skip the last continue cmd campare.
					continStat = 0;
				}
				else
				{
					unContinCount += 2;
				}
			}
		}
		else
		{
			if (continStat == 2)
			{
				unContinCount += 2;
			}
			else if (continStat == 0)
			{
				unContinCount = 0;
				unContinStartItem = item;
				continStat = 2;
				unContinCount += 2;
			}
		}
		{
			if ((lastContinStat != continStat) || endflag==1)
			{
				if ((lastContinStat==1 && (continStat != 1)) || (endflag==1 && continStat==1))
				{
					//send continue cmd
					continlen = 0;
					pRegSrc = para+continStartItem;
#if MULTI_WRITE
					puSdContinCmd[continlen++] = (char)(pRegSrc[0] >> 8);
					puSdContinCmd[continlen++] = (char)(pRegSrc[0] & 0xFF);
					for (IDX=1; IDX<continCount;)
					{
						tempdata = pRegSrc[IDX];
						puSdContinCmd[continlen++] = (char)(tempdata >> 8);
						puSdContinCmd[continlen++] = (char)(tempdata & 0xFF);
						IDX += 2;
						if (((2+continlen)>MAXLEN_CONTINUEDATA) || (IDX>=continCount))
						{
							iBurstWriteReg_multi(puSdContinCmd, continlen,
								imgsensor.i2c_write_id, continlen, imgsensor_info.i2c_speed);
							continlen = 0;
							if (IDX < continCount)
							{
								puSdContinCmd[continlen++] = (char)(pRegSrc[IDX-1] >> 8);
								puSdContinCmd[continlen++] = (char)(pRegSrc[IDX-1] & 0xFF);
							}
						}
					}
#else
					for (IDX=0; IDX<continCount; )
					{
						addr = pRegSrc[IDX];
						{
							puSendCmd[tosend++] = (char)(addr >> 8);
							puSendCmd[tosend++] = (char)(addr & 0xFF);
							data = pRegSrc[IDX + 1];
							puSendCmd[tosend++] = (char)(data >> 8);
							puSendCmd[tosend++] = (char)(data & 0xFF);
							IDX += 2;
							addr_last = addr;
						}
						iWriteRegI2CTiming(puSendCmd, 4,
							imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
						tosend = 0;
					}
#endif
				}
				else if ((lastContinStat==2 && (continStat != 2)) || (endflag==1 && continStat==2))
				{
					pRegSrc = para+unContinStartItem;
					tosend = 0;
					for (IDX=0; IDX<unContinCount;)
					{
#if MULTI_WRITE
						addr = pRegSrc[IDX];
						{
							puSendCmd[tosend++] = (char)(addr >> 8);
							puSendCmd[tosend++] = (char)(addr & 0xFF);
							data = pRegSrc[IDX + 1];
							puSendCmd[tosend++] = (char)(data >> 8);
							puSendCmd[tosend++] = (char)(data & 0xFF);
							IDX += 2;
							addr_last = addr;
						}
						if ((I2C_BUFFER_LEN - tosend) < 4 || IDX == unContinCount) {
							//check send data!!!
							iBurstWriteReg_multi(puSendCmd, tosend,
								imgsensor.i2c_write_id, 4, imgsensor_info.i2c_speed);
							tosend = 0;
						}
#else
						iWriteRegI2CTiming(puSendCmd, 4,
							imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
						tosend = 0;
#endif
					}
				}
				lastContinStat = continStat;
			}
		}
	}
	kfree(puSendCmd);
    kfree(puSdContinCmd);
//[ITD]add i2c continue mode transfer. hong.zhang@transsion.com, 2024-2-29, end.
	return 0;
}
static kal_uint32 return_sensor_id(void)
{
    kal_uint32 sensor_id = 0;
    sensor_id = ((read_cmos_sensor(0x0716) << 8) | read_cmos_sensor(0x0717)) + 5;
    return sensor_id;
}
static bool skhynix_write_burst_mode(kal_uint16 setting_table[], kal_uint32 nSetSize)
{
    kal_int8 Hynix_i2c_buf[HYNIX_SIZEOF_I2C_BUF] = {0,};
    bool err = false;
    kal_uint32 total_set_count = nSetSize;
    kal_uint16 set_count = 0;
    kal_uint16 buf_count = 0;
    for (set_count = 0; set_count < (total_set_count-2); set_count+=2)
    {
        if (!buf_count)
        {
            Hynix_i2c_buf[buf_count] = setting_table[set_count] >> 8;
            buf_count++;
            Hynix_i2c_buf[buf_count] = setting_table[set_count] & 0xFF;
            buf_count++;
        }
        Hynix_i2c_buf[buf_count] = setting_table[set_count+1] >> 8;
        buf_count++;
        Hynix_i2c_buf[buf_count] = setting_table[set_count+1] & 0xFF;
        buf_count++;
        if((setting_table[set_count+2] == setting_table[set_count] + 2) &&
        (buf_count < HYNIX_SIZEOF_I2C_BUF) )
        {
            continue;
        }
        err = iWriteRegI2CTiming(Hynix_i2c_buf, buf_count, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
        if (err)
            return true;
        buf_count = 0;
    }
    if (buf_count > 0)
    {
        err = iWriteRegI2CTiming(Hynix_i2c_buf, buf_count, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
        if (err)
            return true;
        buf_count = 0;
    }
    write_cmos_sensor(setting_table[total_set_count-2], setting_table[total_set_count-1]);
    return false;
}
#if HI3231QX6855_QBGC_OPC_CALIB
static void apply_sensor_Cali(void)
{
    kal_uint8 *QGC_CalData = (kal_uint8*)QBGCbuffer;
    kal_uint16 length_qbgc = 0;
    kal_uint16 hi3231_qbgc_addr;
    kal_uint16 hi3231_qbgc_data;
    int i = 0;
    for (i = 0; i < 768; i++)
    {
        //sum += buf->xgcData[i];
        LOG_INF("cyxxqcData[%d] = 0x%x %d", i, QGC_CalData[i], QGC_CalData[i]);
    }
    /////////// QBGC_calibration_write//////////////
    LOG_INF("[Start]:QGC  start\n");
    //*********Burst write apply QBGC  start**************//
    {
        write_cmos_sensor(0xffff, 0x1840); // QBGC enable
        hi3231_qbgc_addr = 0x0000;
        for(i = 0; i < 768;i+=2)
        {
            hi3231_qbgc_data = ((((QGC_CalData[i+1]) & (0x00ff))<<8) +((QGC_CalData[i]) & (0x00ff)));
            QBGC_setting_burst[2*length_qbgc]=hi3231_qbgc_addr;
            QBGC_setting_burst[2*length_qbgc+1]=hi3231_qbgc_data;
            length_qbgc++;
            hi3231_qbgc_addr += 2;
        }
        //Burst write QBGC  //
        if(length_qbgc == (768/2))
        {
            skhynix_write_burst_mode(QBGC_setting_burst, sizeof(QBGC_setting_burst)/sizeof(kal_uint16));
        }
        // QBGC SRAM memory access disable
        write_cmos_sensor(0xffff, 0x0000); // QBGC disable
        LOG_INF("[End]:QBGC Line End\n");
        //*********Burst write apply QBGC end**************//
    }
    LOG_INF("[End]:QBGC End\n");
}
#endif
static void set_dummy(void)
{
    LOG_INF("frame length = %d\n", imgsensor.frame_length);
    write_cmos_sensor(0x050C, imgsensor.frame_length & 0xFFFF);
    //write_cmos_sensor(0x0206,imgsensor .line_length);
}
static void set_max_framerate(kal_uint16 framerate, kal_bool min_framelength_en)
{
    kal_uint32 frame_length = imgsensor.frame_length;
    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
    spin_lock(&imgsensor_drv_lock);
    imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
        frame_length : imgsensor.min_frame_length;
    imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
        imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    }
    if (min_framelength_en)
        imgsensor.min_frame_length = imgsensor.frame_length;
    spin_unlock(&imgsensor_drv_lock);
    set_dummy();
}
static void set_shutter(kal_uint16 shutter)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    #ifdef LONG_EXP
    /* add for long shutter */
    static bool bNeedSetNormalMode = KAL_FALSE;
    kal_uint32 cal_shutter = 0;
    kal_uint16 long_exp_h = 0;
    kal_uint16 long_exp_m = 0;
    kal_uint16 long_exp_l = 0;
    #endif
    /*kal_uint32 frame_length = 0;*/
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
    /* if shutter bigger than frame_length, should extend frame length first */
    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);
    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
    shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
    set_dummy();
    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if(realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(296, 0);
        else if(realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(146, 0);
    } else {
        // Extend frame length
        write_cmos_sensor(0x050C, imgsensor.frame_length & 0xFFFF);
    }
    // Update Shutter
    write_cmos_sensor(0x0510, shutter);
    LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
}
static void set_shutter_frame_length(kal_uint16 shutter,kal_uint16 target_frame_length, kal_bool auto_extend_en)
{
    spin_lock(&imgsensor_drv_lock);
    if ( imgsensor.sensor_mode == IMGSENSOR_MODE_CAPTURE ) {
        imgsensor.dummy_line = (target_frame_length > imgsensor_info.cap.framelength) ? (target_frame_length - imgsensor_info.cap.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        pr_debug("shutter: %d, frame_length: %d, cap.framelength: %d dummy_line: %d\n", shutter, imgsensor.frame_length, imgsensor_info.cap.framelength, imgsensor.dummy_line);
    }
    spin_unlock(&imgsensor_drv_lock);
    set_shutter(shutter);
}
static kal_uint16 gain2reg(kal_uint16 gain)
{
    kal_uint16 reg_gain = 0x0000;
    reg_gain = gain / 4 - 16;
    return (kal_uint16)reg_gain;
    return reg_gain;
}
static kal_uint16 set_gain_binning(kal_uint16 gain)
{
    kal_uint32 reg_gain = 0;
    if (gain < BASEGAIN || gain > 64 * BASEGAIN) {
        LOG_INF("Error gain setting");
        if (gain < BASEGAIN)
            gain = BASEGAIN;
        else if (gain > 64 * BASEGAIN)
            gain = 64 * BASEGAIN;
    }
    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d, reg_gain = %d\n", gain, reg_gain);
    write_cmos_sensor(0x050A,reg_gain);
    return gain;
}
static kal_uint16 set_gain_fullsize(kal_uint16 gain)
{
     kal_uint16 reg_gain;
     if (gain < BASEGAIN || gain >16 * BASEGAIN) {
         LOG_INF("Error gain setting");
         if (gain < BASEGAIN)
             gain = BASEGAIN;
         else if (gain > 16 * BASEGAIN)
             gain = 16 * BASEGAIN;
     }
     reg_gain = gain2reg(gain);
     spin_lock(&imgsensor_drv_lock);
     imgsensor.gain = reg_gain;
     spin_unlock(&imgsensor_drv_lock);
     LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
     write_cmos_sensor(0x050A,reg_gain);
     return gain;
}
static void set_mirror_flip(kal_uint8 image_mirror)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.mirror = image_mirror;
    spin_unlock(&imgsensor_drv_lock);
    switch (image_mirror) {
    case IMAGE_NORMAL:
        write_cmos_sensor(0x0202, 0x0000);
        break;
    case IMAGE_H_MIRROR:
        write_cmos_sensor(0x0202, 0x0100);
        break;
    case IMAGE_V_MIRROR:
        write_cmos_sensor(0x0202, 0x0200);
        break;
    case IMAGE_HV_MIRROR:
        write_cmos_sensor(0x0202, 0x0300);
        break;
    default:
        LOG_INF("Error image_mirror setting");
        break;
    }
}
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}
static kal_uint32 streaming_control(kal_bool enable)
{
    pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
    if (enable)
        write_cmos_sensor(0x0b00, 0x0100); // stream on
    else
        write_cmos_sensor(0x0b00, 0x0000); // stream off
    mdelay(5);
    return ERROR_NONE;
}
static kal_uint32 set_test_pattern_mode(kal_uint32 modes, struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
    pr_err("debugxxx set_test_pattern modes: %d,\n", modes);
    if(modes==2){
        LOG_INF("debugxxx set_test_pattern modes: %d,\n", modes);
        write_cmos_sensor(0x0B04, 0x0201);
        write_cmos_sensor(0x0B06, 0x010D);
        write_cmos_sensor(0x0C0A, 0x0200);
    }
    else if (modes == 5 && (pdata != NULL)) {
        LOG_INF("debugxxx set_test_pattern modes: %d,\n", modes);
        write_cmos_sensor(0x0B04, 0x0201);
        write_cmos_sensor(0x0C0A, 0x0101);
    }else {
        write_cmos_sensor(0x0C0A, 0x0000);
    };
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = modes;
    LOG_INF("final set_test_pattern modes: %d\n", modes);
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}
static void sensor_init(void)
{
    mix_write_cmos_sensor(addr_data_pair_init_hi3231qx6855,
        sizeof(addr_data_pair_init_hi3231qx6855)/sizeof(kal_uint16));
}
static void preview_setting(void)
{
    mix_write_cmos_sensor(addr_data_pair_preview_hi3231qx6855,
      sizeof(addr_data_pair_preview_hi3231qx6855)/sizeof(kal_uint16));
}
static void capture_setting(kal_uint16 currefps)
{
    mix_write_cmos_sensor(addr_data_pair_capture_30fps_hi3231qx6855,
        sizeof(addr_data_pair_capture_30fps_hi3231qx6855)/sizeof(kal_uint16));
}
static void normal_video_setting(kal_uint16 currefps)
{
    mix_write_cmos_sensor(addr_data_pair_video_hi3231qx6855,
        sizeof(addr_data_pair_video_hi3231qx6855)/sizeof(kal_uint16));
}
static void hs_video_setting(void)
{
    mix_write_cmos_sensor(addr_data_pair_hs_video_hi3231qx6855,
        sizeof(addr_data_pair_hs_video_hi3231qx6855)/sizeof(kal_uint16));
}
static void slim_video_setting(void)
{
    mix_write_cmos_sensor(addr_data_pair_slim_video_hi3231qx6855,
        sizeof(addr_data_pair_slim_video_hi3231qx6855)/sizeof(kal_uint16));
}
static void custom1_setting(void)
{
    mix_write_cmos_sensor(addr_data_pair_custom1_hi3231qx6855,
        sizeof(addr_data_pair_custom1_hi3231qx6855) / sizeof(kal_uint16));
}
static void custom2_setting(void)
{
    mix_write_cmos_sensor(addr_data_pair_custom2_hi3231qx6855,
        sizeof(addr_data_pair_custom2_hi3231qx6855) / sizeof(kal_uint16));
}
static void custom3_setting(void)
{
    mix_write_cmos_sensor(addr_data_pair_custom3_hi3231qx6855,
        sizeof(addr_data_pair_custom3_hi3231qx6855) / sizeof(kal_uint16));
}
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    //int j = 0;
    kal_uint8 retry = 2;
    kal_uint8 *XTCbuffer;
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = return_sensor_id();
            if (*sensor_id == imgsensor_info.sensor_id) {
                pr_info("[hi3231qx6855_camera_sensor]get_imgsensor_id:i2c write id: 0x%x, sensor id: 0x%x\n",
                    imgsensor.i2c_write_id, *sensor_id);
                #if HI3231QX6855_QBGC_OPC_CALIB
                XTCbuffer = (kal_uint8 *)kmalloc(XTC_DATA_LEN, GFP_KERNEL);
                if(_read_hi3231qx6855_eeprom(XTC_START_ADDR, XTCbuffer, XTC_DATA_LEN)) {
                    pr_info("hi3231qx6855 read xtc data succeed!");
                    memcpy(QBGCbuffer, XTCbuffer, HI3231QX6855_OTP_QBGC_LEN);
                    memcpy(&OPCbuffer[2], XTCbuffer + HI3231QX6855_OTP_QBGC_LEN, HI3231QX6855_OTP_OPC_LEN);
                }
                kfree(XTCbuffer);
                #if 0
                for(j = 0; j < HI3231QX6855_OTP_QBGC_LEN; j++)
                    pr_info("hi3231qx6855 QBGCbuffer[0x%x] = %d", QBGC_START_ADDR+j, QBGCbuffer[j]);
                for(j = 0; j < HI3231QX6855_OTP_OPC_LEN; j++)
                    pr_info("hi3231qx6855 OPCbuffer[0x%x] = %d", OPC_START_ADDR+j, OPCbuffer[j+2]);
                #endif
                #endif
                return ERROR_NONE;
            }
            pr_info("[hi3231qx6855_camera_sensor]get_imgsensor_id:Read sensor id fail, write id: 0x%x, id: 0x%x\n",
                imgsensor.i2c_write_id, *sensor_id);
            retry--;
        } while (retry > 0);
        i++;
        retry = 2;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        /* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}
static kal_uint32 open(void)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint32 sensor_id = 0;
    LOG_1;
    LOG_INF("imgsensor_open\n");
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = return_sensor_id();
            if (sensor_id == imgsensor_info.sensor_id) {
                pr_debug("[hi3231qx6855_camera_sensor]open:i2c write id: 0x%x, sensor id: 0x%x\n",
                    imgsensor.i2c_write_id, sensor_id);
                break;
            }
            pr_debug("[hi3231qx6855_camera_sensor]open:Read sensor id fail, write id: 0x%x, id: 0x%x\n",
                imgsensor.i2c_write_id, sensor_id);
            retry--;
        } while (retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }
    if (imgsensor_info.sensor_id != sensor_id)
        return ERROR_SENSOR_CONNECT_FAIL;
    /* initail sequence write in  */
    sensor_init();
    #if HI3231QX6855_QBGC_OPC_CALIB
    apply_sensor_Cali();
    #endif
    spin_lock(&imgsensor_drv_lock);
    imgsensor.autoflicker_en = KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.dummy_pixel = 0;
    imgsensor.dummy_line = 0;
    imgsensor.ihdr_en = 0;
    imgsensor.test_pattern = KAL_FALSE;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}
static kal_uint32 close(void)
{
    LOG_INF("E\n");
    /* No Need to implement this function */
    return ERROR_NONE;
}
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    /* imgsensor.video_mode = KAL_FALSE; */
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    set_mirror_flip(imgsensor.mirror);
    mdelay(10);
    return ERROR_NONE;
    return ERROR_NONE;
}
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
        LOG_INF(
            "Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
            imgsensor.current_fps,
            imgsensor_info.cap.max_framerate / 10);
    imgsensor.pclk = imgsensor_info.cap.pclk;
    imgsensor.line_length = imgsensor_info.cap.linelength;
    imgsensor.frame_length = imgsensor_info.cap.framelength;
    imgsensor.min_frame_length = imgsensor_info.cap.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    capture_setting(imgsensor.current_fps);
    set_mirror_flip(imgsensor.mirror);
    mdelay(10);
    return ERROR_NONE;
}
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
    imgsensor.pclk = imgsensor_info.normal_video.pclk;
    imgsensor.line_length = imgsensor_info.normal_video.linelength;
    imgsensor.frame_length = imgsensor_info.normal_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
    imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    normal_video_setting(imgsensor.current_fps);
    set_mirror_flip(imgsensor.mirror);
    return ERROR_NONE;
}
static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    //imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting();
    set_mirror_flip(imgsensor.mirror);
    return ERROR_NONE;
}
static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting();
    set_mirror_flip(imgsensor.mirror);
    return ERROR_NONE;
}
static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT
*sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
    imgsensor.pclk = imgsensor_info.custom1.pclk;
    imgsensor.line_length = imgsensor_info.custom1.linelength;
    imgsensor.frame_length = imgsensor_info.custom1.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom1_setting();
    set_mirror_flip(imgsensor.mirror);
    return ERROR_NONE;
}
static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
    imgsensor.pclk = imgsensor_info.custom2.pclk;
    imgsensor.line_length = imgsensor_info.custom2.linelength;
    imgsensor.frame_length = imgsensor_info.custom2.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom2_setting();
    set_mirror_flip(imgsensor.mirror);
    return ERROR_NONE;
}
static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
    imgsensor.pclk = imgsensor_info.custom3.pclk;
    imgsensor.line_length = imgsensor_info.custom3.linelength;
    imgsensor.frame_length = imgsensor_info.custom3.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom3_setting();
    set_mirror_flip(imgsensor.mirror);
    return ERROR_NONE;
}
static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    LOG_INF("E\n");
    sensor_resolution->SensorFullWidth =
        imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight =
        imgsensor_info.cap.grabwindow_height;
    sensor_resolution->SensorPreviewWidth =
        imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight =
        imgsensor_info.pre.grabwindow_height;
    sensor_resolution->SensorVideoWidth =
        imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight =
        imgsensor_info.normal_video.grabwindow_height;
    sensor_resolution->SensorHighSpeedVideoWidth =
        imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight =
        imgsensor_info.hs_video.grabwindow_height;
    sensor_resolution->SensorSlimVideoWidth =
        imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight =
        imgsensor_info.slim_video.grabwindow_height;
    sensor_resolution->SensorCustom1Width  =
        imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height  =
        imgsensor_info.custom1.grabwindow_height;
    sensor_resolution->SensorCustom2Width =
        imgsensor_info.custom2.grabwindow_width;
    sensor_resolution->SensorCustom2Height =
        imgsensor_info.custom2.grabwindow_height;
    sensor_resolution->SensorCustom3Width =
        imgsensor_info.custom3.grabwindow_width;
    sensor_resolution->SensorCustom3Height =
        imgsensor_info.custom3.grabwindow_height;
    return ERROR_NONE;
}
static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
    MSDK_SENSOR_INFO_STRUCT *sensor_info,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);
    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; /* inverse with datasheet */
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */
    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
    sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;
    sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
    sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
    sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
    sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
    sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
    sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
    sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
    sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
    sensor_info->SensorClockFreq = imgsensor_info.mclk;
    sensor_info->SensorClockDividCount = 3; /* not use */
    sensor_info->SensorClockRisingCount = 0;
    sensor_info->SensorClockFallingCount = 2; /* not use */
    sensor_info->SensorPixelClockCount = 3; /* not use */
    sensor_info->SensorDataLatchCount = 2; /* not use */
    sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
    sensor_info->SensorHightSampling = 0;  /* 0 is default 1x */
    sensor_info->SensorPacketECCOrder = 1;
    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
        break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
        break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
        break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
        break;
    case MSDK_SCENARIO_ID_CUSTOM1:
        sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
        break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
        break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
    break;
    default:
        sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
    break;
    }
    return ERROR_NONE;
}
static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    pr_info("scenario_id = %d\n", scenario_id);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.current_scenario_id = scenario_id;
    spin_unlock(&imgsensor_drv_lock);
    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            preview(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            capture(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            normal_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            hs_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            slim_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            custom1(image_window, sensor_config_data); // Custom1
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            custom2(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            custom3(image_window, sensor_config_data);
            break;
        default:
            LOG_INF("Error ScenarioId setting");
            preview(image_window, sensor_config_data);
            return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
}
static kal_uint32 set_video_mode(UINT16 framerate)
{
    /*This Function not used after ROME*/
    LOG_INF("framerate = %d\n ", framerate);
    /* SetVideoMode Function should fix framerate */
    if (framerate == 0)
        return ERROR_NONE;
    spin_lock(&imgsensor_drv_lock);
    if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 296;
    else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 146;
    else
        imgsensor.current_fps = framerate;
    spin_unlock(&imgsensor_drv_lock);
    set_max_framerate(imgsensor.current_fps, 1);
    return ERROR_NONE;
}
static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
    LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
    spin_lock(&imgsensor_drv_lock);
    if (enable) /* enable auto flicker */
        imgsensor.autoflicker_en = KAL_TRUE;
    else /* Cancel Auto flick */
        imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}
static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    kal_uint32 frame_length;
    LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
    fixed_framerate = framerate;
    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        frame_length = imgsensor_info.pre.pclk / framerate * 10 /
            imgsensor_info.pre.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length >
            imgsensor_info.pre.framelength) ?
            (frame_length - imgsensor_info.pre.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.pre.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        if (framerate == 0)
            return ERROR_NONE;
        frame_length = imgsensor_info.normal_video.pclk /
            framerate * 10 / imgsensor_info.normal_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length >
            imgsensor_info.normal_video.framelength) ?
        (frame_length - imgsensor_info.normal_video.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.normal_video.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        if (imgsensor.current_fps ==
                imgsensor_info.cap1.max_framerate) {
        frame_length = imgsensor_info.cap1.pclk / framerate * 10 /
                imgsensor_info.cap1.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length >
            imgsensor_info.cap1.framelength) ?
            (frame_length - imgsensor_info.cap1.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.cap1.framelength +
                imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        } else {
            if (imgsensor.current_fps !=
                imgsensor_info.cap.max_framerate)
            LOG_INF("fps %d fps not support,use cap: %d fps!\n",
            framerate, imgsensor_info.cap.max_framerate/10);
            frame_length = imgsensor_info.cap.pclk /
                framerate * 10 / imgsensor_info.cap.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length >
                imgsensor_info.cap.framelength) ?
            (frame_length - imgsensor_info.cap.framelength) : 0;
            imgsensor.frame_length =
                imgsensor_info.cap.framelength +
                imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
        }
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        frame_length = imgsensor_info.hs_video.pclk /
            framerate * 10 / imgsensor_info.hs_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length >
            imgsensor_info.hs_video.framelength) ? (frame_length -
            imgsensor_info.hs_video.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.hs_video.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        frame_length = imgsensor_info.slim_video.pclk /
            framerate * 10 / imgsensor_info.slim_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length >
            imgsensor_info.slim_video.framelength) ? (frame_length -
            imgsensor_info.slim_video.framelength) : 0;
        imgsensor.frame_length =
            imgsensor_info.slim_video.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CUSTOM1:
        frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? (frame_length - imgsensor_info.custom2.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ? (frame_length - imgsensor_info.custom3.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    default:  //coding with  preview scenario by default
        frame_length = imgsensor_info.pre.pclk / framerate * 10 /
                        imgsensor_info.pre.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length >
            imgsensor_info.pre.framelength) ?
            (frame_length - imgsensor_info.pre.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.pre.framelength +
                imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        LOG_INF("error scenario_id = %d, we use preview scenario\n",
                scenario_id);
    break;
    }
    return ERROR_NONE;
}
static kal_uint32 set_framerate_by_thermal(MUINT32 framerate)
{
    kal_uint32 frame_length;
    LOG_INF("sensorMode:[%d], framerate:[%d]\n", imgsensor.sensor_mode, framerate);
    if (framerate > fixed_framerate) {
        LOG_INF("framerate:[%d] > fixed_framerate:[%d], return\n", framerate, fixed_framerate);
        return ERROR_NONE;
    }
    switch (imgsensor.sensor_mode) {
    case IMGSENSOR_MODE_PREVIEW:
        frame_length = imgsensor_info.pre.pclk / framerate * 10
            / imgsensor_info.pre.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
            ? (frame_length - imgsensor_info.pre.framelength) : 0;
        imgsensor.frame_length
            = imgsensor_info.pre.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case IMGSENSOR_MODE_VIDEO:
        if (framerate == 0)
            return ERROR_NONE;
        frame_length = imgsensor_info.normal_video.pclk / framerate * 10
            / imgsensor_info.normal_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength)
            ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
        imgsensor.frame_length
            = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case IMGSENSOR_MODE_CAPTURE:
        frame_length = imgsensor_info.cap.pclk / framerate * 10 /
            imgsensor_info.cap.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength)
            ? (frame_length - imgsensor_info.cap.framelength) : 0;
        imgsensor.frame_length
            = imgsensor_info.cap.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    default:
        break;
    }
    return ERROR_NONE;
}
static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    LOG_INF("scenario_id = %d\n", scenario_id);
    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        *framerate = imgsensor_info.pre.max_framerate;
    break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        *framerate = imgsensor_info.normal_video.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        *framerate = imgsensor_info.cap.max_framerate;
    break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        *framerate = imgsensor_info.hs_video.max_framerate;
    break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        *framerate = imgsensor_info.slim_video.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CUSTOM1:
        *framerate = imgsensor_info.custom1.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        *framerate = imgsensor_info.custom2.max_framerate;
        break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        *framerate = imgsensor_info.custom3.max_framerate;
        break;
    default:
        break;
    }
    return ERROR_NONE;
}
static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
    UINT8 *feature_para, UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
    UINT16 *feature_data_16 = (UINT16 *)feature_para;
    UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
    UINT32 *feature_data_32 = (UINT32 *)feature_para;
    unsigned long long *feature_data = (unsigned long long *)feature_para;
    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;
    LOG_INF("feature_id = %d\n", feature_id);
    switch (feature_id) {
    case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
        *(feature_data + 1) = imgsensor_info.min_gain;
        *(feature_data + 2) = imgsensor_info.max_gain;
        break;
    case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
        *(feature_data + 0) = imgsensor_info.min_gain_iso;
        *(feature_data + 1) = imgsensor_info.gain_step;
        *(feature_data + 2) = imgsensor_info.gain_type;
        break;
    case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
        *(feature_data + 1) = imgsensor_info.min_shutter;
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_CUSTOM1:
        case MSDK_SCENARIO_ID_CUSTOM2:
        case MSDK_SCENARIO_ID_CUSTOM4:
        case MSDK_SCENARIO_ID_CUSTOM5:
            *(feature_data + 2) = 2;
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
        default:
            *(feature_data + 2) = 1;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_BINNING_TYPE:
        switch (*(feature_data + 1)) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        default:
            *feature_return_para_32 = 1;
            break;
        }
        pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",*feature_return_para_32);
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
        *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 2000000;
        break;
    case SENSOR_FEATURE_GET_PERIOD:
        *feature_return_para_16++ = imgsensor.line_length;
        *feature_return_para_16 = imgsensor.frame_length;
            *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        *feature_return_para_32 = imgsensor.pclk;
            *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.cap.pclk;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.normal_video.pclk;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.hs_video.pclk;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.slim_video.pclk;
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom1.pclk;
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom2.pclk;
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom3.pclk;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.pre.pclk;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.cap.framelength << 16)
                + imgsensor_info.cap.linelength;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.normal_video.framelength << 16)
                + imgsensor_info.normal_video.linelength;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.hs_video.framelength << 16)
                + imgsensor_info.hs_video.linelength;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.slim_video.framelength << 16)
                + imgsensor_info.slim_video.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom1.framelength << 16)
                + imgsensor_info.custom1.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom2.framelength << 16)
                + imgsensor_info.custom2.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom3.framelength << 16)
                + imgsensor_info.custom3.linelength;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.pre.framelength << 16)
                + imgsensor_info.pre.linelength;
            break;
        }
        break;
    case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:/*lzl*/
        pr_debug("SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME\n");
        set_shutter_frame_length((UINT16)*(feature_data),
                (UINT16)(*(feature_data+1)),
                (unsigned char) (*(feature_data + 2)));
        break;
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
        *(feature_data + 1) = 1;
        *(feature_data + 2) = imgsensor_info.margin;
        break;
    case SENSOR_FEATURE_SET_ESHUTTER:
        set_shutter(*feature_data);
        break;
    case SENSOR_FEATURE_SET_NIGHTMODE:
            night_mode((unsigned char) * feature_data);
        break;
    case SENSOR_FEATURE_SET_GAIN:
        if(imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM3)
            set_gain_fullsize((UINT16) *feature_data);
        else
            set_gain_binning((UINT16) *feature_data);
        break;
    case SENSOR_FEATURE_SET_FLASHLIGHT:
        break;
    case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
        break;
    case SENSOR_FEATURE_SET_REGISTER:
        write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
        break;
    case SENSOR_FEATURE_GET_REGISTER:
        sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
        break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
            *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_VIDEO_MODE:
        set_video_mode(*feature_data);
        break;
    case SENSOR_FEATURE_CHECK_SENSOR_ID:
        get_imgsensor_id(feature_return_para_32);
        break;
    case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            set_auto_flicker_mode((unsigned char)*feature_data_16, *(feature_data_16+1));
        break;
    case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
        break;
    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data),(MUINT32 *)(uintptr_t)(*(feature_data+1)));
        break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
            set_test_pattern_mode((UINT32)*feature_data, (struct SET_SENSOR_PATTERN_SOLID_COLOR *)(feature_data+1));
        break;
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
            *feature_return_para_32 = imgsensor_info.checksum_value;
            *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_FRAMERATE:
        LOG_INF("current fps :%d\n", *feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.current_fps = *feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
        break;
    case SENSOR_FEATURE_SET_FPS_BY_THERMAL:
        LOG_INF("set_framerate_by_thermal :[%d] \n",(UINT32)*feature_data_32);
        set_framerate_by_thermal(*feature_data_32);
        break;
    case SENSOR_FEATURE_SET_HDR:
        LOG_INF("ihdr enable :%d\n", (unsigned char)*feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.ihdr_en = (unsigned char)*feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
        break;
    case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
        switch (*feature_data) {
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);
        wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
        switch (*feature_data_32) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[7], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        }
            break;
    case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n", (UINT16)*feature_data, (UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
        /* ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2)); */
        break;
    case SENSOR_FEATURE_GET_4CELL_DATA:
        {
            int type = (kal_uint16)(*feature_data);
            char *data = (char *)(uintptr_t)(*(feature_data+1));
            kal_uint8 *XTCbuffer;
            XTCbuffer = (kal_uint8 *)kmalloc(XTC_DATA_LEN, GFP_KERNEL);
            if ( type == FOUR_CELL_CAL_TYPE_GAIN_TBL ) {
                if (OPCbuffer[2] == 0) {
                    printk("read 4cell data again \n");
                    _read_hi3231qx6855_eeprom(XTC_START_ADDR, XTCbuffer, XTC_DATA_LEN);
                    memcpy(&OPCbuffer[2], XTCbuffer + HI3231QX6855_OTP_QBGC_LEN, HI3231QX6855_OTP_OPC_LEN); //OPC data swremosaic lib need
                }
                OPCbuffer[0] = (HI3231QX6855_OTP_OPC_LEN & 0xff);/*Low*/
                OPCbuffer[1] = ((HI3231QX6855_OTP_OPC_LEN >> 8) & 0xff);/*High*/
                if (data != NULL) {
                    printk("return data\n");
                    memcpy(data, OPCbuffer, sizeof(OPCbuffer) / sizeof(OPCbuffer[0]));
                }
                LOG_INF("Read Cross Talk = %02x %02x %02x %02x %02x %02x\n",
                (UINT16)data[0], (UINT16)data[1],
                (UINT16)data[2], (UINT16)data[3],
                (UINT16)data[4], (UINT16)data[5]);
            }
            kfree(XTCbuffer);
        break;
        }
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
        streaming_control(KAL_FALSE);
        break;
    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
            *feature_data);
        if (*feature_data != 0)
            set_shutter(*feature_data);
        streaming_control(KAL_TRUE);
        break;
    case SENSOR_FEATURE_GET_PIXEL_RATE:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
            (imgsensor_info.cap.pclk /
            (imgsensor_info.cap.linelength - 80))*
            imgsensor_info.cap.grabwindow_width;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
            (imgsensor_info.normal_video.pclk /
            (imgsensor_info.normal_video.linelength - 80))*
            imgsensor_info.normal_video.grabwindow_width;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
            (imgsensor_info.hs_video.pclk /
            (imgsensor_info.hs_video.linelength - 80))*
            imgsensor_info.hs_video.grabwindow_width;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
            (imgsensor_info.slim_video.pclk /
            (imgsensor_info.slim_video.linelength - 80))*
            imgsensor_info.slim_video.grabwindow_width;
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
            (imgsensor_info.custom1.pclk /
            (imgsensor_info.custom1.linelength - 80))*
            imgsensor_info.custom1.grabwindow_width;
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
            (imgsensor_info.custom2.pclk /
            (imgsensor_info.custom2.linelength - 80))*
            imgsensor_info.custom2.grabwindow_width;
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
            (imgsensor_info.custom3.pclk /
            (imgsensor_info.custom3.linelength - 80))*
            imgsensor_info.custom3.grabwindow_width;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
            (imgsensor_info.pre.pclk /
            (imgsensor_info.pre.linelength - 80))*
            imgsensor_info.pre.grabwindow_width;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                imgsensor_info.cap.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                imgsensor_info.normal_video.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                imgsensor_info.hs_video.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                imgsensor_info.slim_video.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
             *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                 = imgsensor_info.custom1.mipi_pixel_rate;
             break;
         case MSDK_SCENARIO_ID_CUSTOM2:
             *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                 = imgsensor_info.custom2.mipi_pixel_rate;
             break;
         case MSDK_SCENARIO_ID_CUSTOM3:
             *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                 = imgsensor_info.custom3.mipi_pixel_rate;
             break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                imgsensor_info.pre.mipi_pixel_rate;
            break;
        }
        break;
    default:
        break;
    }
    return ERROR_NONE;
}
static struct SENSOR_FUNCTION_STRUCT sensor_func = {
    open,
    get_info,
    get_resolution,
    feature_control,
    control,
    close
};
UINT32 HI3231QX6855_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    /* Check Sensor status here */
    if (pfFunc != NULL)
        *pfFunc = &sensor_func;
    return ERROR_NONE;
}