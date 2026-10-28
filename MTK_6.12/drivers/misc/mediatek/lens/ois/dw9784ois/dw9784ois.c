/*
 * Copyright (C) 2021 Transsion Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "dw9784ois.h"
#include "dw9784fw.h"

#define DEBUG_LOG
#define EIS_DEBUG 0

#ifndef DW9784_DTNAME_I2C
#define DW9784_DTNAME_I2C "mediatek,dw9784_ois_i2c"
#endif

#ifndef DW9784_DTNAME
#define DW9784_DTNAME "mediatek,dw9784_ois"
#endif

#define DW9784_NAME "dw9784_ois"

#ifdef DEBUG_LOG
#define OIS_REGDUMP "DW9784_DBG"
#define LOG_DBG(format, args...) pr_debug(OIS_REGDUMP " " format, ##args)
#define LOG_INF(format, args...) pr_info(OIS_REGDUMP " " format, ##args)
#else
#define LOG_DBG(format, args...)
#define LOG_INF(format, args...)
#endif

#define NAME_LENGTH     38
static struct class *dw9784_class = NULL;
static struct device *dw9784_dev = NULL;
static struct device_attribute dev_attr_dw9784_ois;

#ifdef CONFIG_TRAN_CAM_USE_PMU_LDO2
extern int mt6360_pmu_reg_write_ldo2(bool on);
#endif

#ifdef CONFIG_TRAN_CAM_USE_PMU_LDO5
extern int mt6360_pmu_reg_write_ldo5(bool on);
#endif

#define DW9784_FW_SIZE 10240	//FW Size : 10240 word
#define DW9784_PID_SIZE 256		//FW Size : 256 word
#define MTP_START_ADDRESS	0x8000
#define IF_START_ADDRESS	0x8000
#define BASE_PHYS_ADDR 0x70B0
static struct i2c_client *dw9784_i2c_client;

static int g_pOIS_Opened = 0;

static unsigned int g_sysfs_ois;

static int RamReadBulkDW9784OIS(uint16_t start_addr, uint16_t *buffer, int count);
/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static DEFINE_MUTEX(dw9784ois_mutex);
static struct hrtimer dw9784ois_timer;
static struct hrtimer log_timer;
static struct work_struct dw9784ois_work;
static struct workqueue_struct *dw9784ois_highpri_wq; //high priority workqueue

static struct task_struct                     *qtimerTask;
static struct kthread_worker                  qtimerWorker;
static struct kthread_work                    qtimerWork;
//ktime_t last_time = 0;
static unsigned int currentWriteIndex = 0; 
static unsigned int dataflag = 0; 
static unsigned int oislogcnt = 0;
static bool dw9784_calsave_on = 0;
static bool dw9784reset_on = 0;
static int dw9784_save_af_target = 0;
static bool use_af_in_protect = 0;
int dw9784caf_target(unsigned long afTarget);

static void dw9784ois_work_polling(struct kthread_work *work)
{
	uint64_t current_time_ns;
	ktime_t current_ktime;
	int j;
    unsigned short bulk_data[37]; // 1+4+16*2
    int ret;
    struct DW9784TransmitBufferData tempBuffer;
	// mutex_lock(&dw9784ois_mutex);
	/*get real time write to qtimer*/

		hrtimer_start(&dw9784ois_timer, ktime_set(0,32300000), HRTIMER_MODE_REL);
		current_ktime = ktime_get_real();
		current_time_ns = ktime_to_ns(current_ktime);
		//LOG_DBG("wxyy:write qtime Current system time: %llu ns\n", current_time_ns);
		RamWrite32A_DW9784OIS(0x70E1, (uint16_t)(current_time_ns & 0xFFFF), 0);
		RamWrite32A_DW9784OIS(0x70E0, (uint16_t)((current_time_ns >> 16) & 0xFFFF), 0);
		RamWrite32A_DW9784OIS(0x70DF, (uint16_t)((current_time_ns >> 32) & 0xFFFF), 0);
		RamWrite32A_DW9784OIS(0x70DE, (uint16_t)((current_time_ns >> 48) & 0xFFFF), 0);
		
		
		// mutex_unlock(&dw9784ois_mutex);
		//last_time = current_time_ns;
		udelay(300);
		//getPosData();
		ret = RamReadBulkDW9784OIS(BASE_PHYS_ADDR, bulk_data, 37);
		
		
	if (ret < 0) {
		LOG_INF("wxyy:Failed to read data from hardware. Error code: %d\n", ret);
		//mutex_unlock(&dw9784ois_mutex);
		return;
		}
	//LOG_INF("wxyy:data0 = 0x%x",bulk_data[0]);
	if (currentWriteIndex >= BUFFER_ARRAY_SIZE) {
		memmove(bufferArray, &bufferArray[1], (BUFFER_ARRAY_SIZE - 1) * sizeof(struct DW9784TransmitBufferData));
		currentWriteIndex = BUFFER_ARRAY_SIZE - 1;
		//LOG_INF("wxyy:Buffer full, removed oldest entry.\n");
		}
		
	tempBuffer.buffer.latestOISTimeStampH_high = bulk_data[1];
	tempBuffer.buffer.latestOISTimeStampH_low = bulk_data[2];
	tempBuffer.buffer.latestOISTimeStampL_high = bulk_data[3];
	tempBuffer.buffer.latestOISTimeStampL_low = bulk_data[4];

	for (j = 0; j < 16; j++) {
		tempBuffer.buffer.positionDataX[j] = bulk_data[5 + j * 2];
		tempBuffer.buffer.positionDataY[j] = bulk_data[6 + j * 2];
		}

	tempBuffer.flagnum = dataflag++;

	bufferArray[currentWriteIndex] = tempBuffer;

	currentWriteIndex++;
	
}

static enum hrtimer_restart dw9784ois_timer_func(struct hrtimer *timer)
{
	//queue_work(dw9784ois_highpri_wq, &dw9784ois_work);
	kthread_queue_work(&qtimerWorker, &qtimerWork);
	//hrtimer_forward_now(timer, ns_to_ktime(32000000));
	//hrtimer_forward_now(timer, ktime_set(0, 500000));
	return HRTIMER_NORESTART;
}

/******************************************************************************
 * ois status log print Func
 *****************************************************************************/

 static void ois_status_print(struct work_struct *data)
 {
	 // uint64_t timestamp_log; 
	 // int i;
	 // timestamp_log = ((uint64_t)(logprintbuffer.latestOISTimeStampH_high & 0xFFFF) << 48) |
	 //                 ((uint64_t)(logprintbuffer.latestOISTimeStampH_low & 0xFFFF) << 32) |
	 //                 ((uint64_t)(logprintbuffer.latestOISTimeStampL_high & 0xFFFF) << 16) |
	 //                 ((uint64_t)(logprintbuffer.latestOISTimeStampL_low & 0xFFFF));
	 // for (i = 0; i < 16; i++) {
	 // 	LOG_INF("halldata_timestamps[%d] = %llu\n", i, timestamp_log + 2000000 * i);
	 // 	LOG_INF("lens_pos[%d].i4x = %d, lens_pos[%d].i4y = %d\n", 
	 // 			i, logprintbuffer.positionDataX[i], i, logprintbuffer.positionDataY[i]);
	 // }
 
	 getGyroData();
 
 
 }
 
 static enum hrtimer_restart log_timer_func(struct hrtimer *timer)
 {
	queue_work(dw9784ois_highpri_wq, &dw9784ois_work);
	//hrtimer_forward_now(timer, ns_to_ktime(32000000));
	hrtimer_forward_now(timer, ktime_set(0, 6666666));
	return HRTIMER_RESTART;
 }

/******************************************************************************
 * IIC Read Func
 *****************************************************************************/
static int RamRead32A_DW9784OIS(unsigned short a_u4Addr, unsigned short *a_puBuff)
{
	int i4RetValue = 0;
	char puReadCmd[2] = {(char)(a_u4Addr >> 8),(char)(a_u4Addr & 0xFF)};

	unsigned short vRcvBuff = 0;
	if(dw9784_i2c_client == NULL)
	{
		LOG_INF(" dw9784_i2c_client is null!!\n");
		return -1;
	}
	i4RetValue = i2c_master_send(dw9784_i2c_client, puReadCmd, 2);
	if (i4RetValue < 0) {
		LOG_INF(" I2C Send failed!!\n");
		return -1;
	}

	i4RetValue = i2c_master_recv(dw9784_i2c_client, (u8 *)&vRcvBuff, 2);
	if (i4RetValue != 2) {
		LOG_INF(" I2C Read failed!!\n");
		return -1;
	}

    *a_puBuff = ((vRcvBuff & 0xFF) << 8) + ((vRcvBuff >> 8) & 0xFF);
	return 0;
}

#define BUFFER_SIZE 74

static int RamReadBulkDW9784OIS(uint16_t start_addr, uint16_t *buffer, int count)
{
    int i4RetValue = 0;  
    uint8_t puReadCmd[2] = {(uint8_t)(start_addr >> 8), (uint8_t)(start_addr & 0xFF)};
    uint8_t recv_buffer[BUFFER_SIZE];
    int i;

    if (!buffer || count <= 0 || count > (BUFFER_SIZE / 2)) {
        LOG_INF("Invalid parameters for bulk read: buffer=%p, count=%d\n", buffer, count);
        return -EINVAL;
    }

    i4RetValue = i2c_master_send(dw9784_i2c_client, puReadCmd, 2);
    if (i4RetValue < 0) {
        LOG_INF("I2C Send failed for start_addr=0x%X!!\n", start_addr);
        return -EIO;
    }

    i4RetValue = i2c_master_recv(dw9784_i2c_client, recv_buffer, count * 2);
    if (i4RetValue != count * 2) {
        LOG_INF("I2C Bulk Read failed!! Received bytes=%d, Expected=%d\n", i4RetValue, count * 2);
        return -EIO;
    }

    for (i = 0; i < count; i++) {
        buffer[i] = ((recv_buffer[i * 2] & 0xFF) << 8) | (recv_buffer[i * 2 + 1] & 0xFF);
    }

    // LOG_INF("Bulk read completed: start_addr=0x%X, count=%d\n", start_addr, count);
    return 0;
}
/******************************************************************************
 * IIC Write Func
 *****************************************************************************/
static int RamWrite32A_DW9784OIS(unsigned short RamAddr, unsigned short RamData, unsigned short delayTime)
{
	int i4RetValue = 0;
	char puSendCmd[4] = {
		(char)((RamAddr >> 8) & 0xFF), (char)(RamAddr & 0xFF),
		(char)((RamData >> 8) & 0xFF), (char)(RamData & 0xFF)};
	//LOG_INF(" wxy RamWrite32A_DW9784OIS addr:%d value:%d \n",RamAddr,RamData);
	//LOG_DBG("I2C w2 (%x %x)\n", RamAddr, RamData);
	if(dw9784_i2c_client == NULL)
	{
		LOG_INF("dw9784_i2c_client is null\n");
		return -1;
	}
	i4RetValue = i2c_master_send(dw9784_i2c_client, puSendCmd, 4);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}
	if(delayTime>0){
		mdelay(delayTime);
	}
    return 0;
}

static int RamBurstWrite256_DW9784OIS(unsigned short startRamAddr, unsigned short *ramDataArray, unsigned short delayTime)
{
    int i4RetValue = 0;
    char puSendCmd[2 + 256 * 2];
    int index = 0;
	int i = 0;
    puSendCmd[index++] = (char)((startRamAddr >> 8) & 0xFF); 
    puSendCmd[index++] = (char)(startRamAddr & 0xFF); 

    for ( i = 0; i < 256; i++) {
        unsigned short ramData = ramDataArray[i];
        puSendCmd[index++] = (char)((ramData >> 8) & 0xFF); 
        puSendCmd[index++] = (char)(ramData & 0xFF); 
    }
	if(dw9784_i2c_client == NULL)
	{
		LOG_INF("dw9784_i2c_client is null\n");
		return -1;
	}
    i4RetValue = i2c_master_send(dw9784_i2c_client, puSendCmd, index);
    if (i4RetValue < 0) {
        LOG_INF("I2C send failed at start address 0x%x!!\n", startRamAddr);
        return -1;
    }

    if (delayTime > 0) {
        mdelay(delayTime);
    }

    return 0;
}

/**************************************************************************
Function	: IIC Block Read
Date		: 2021.05.20
Version		: 0.001
History		:
***************************************************************************/
// static int IIC_DataRead_DW9784(unsigned char *dat,
// 		int size, unsigned char *ret, int ret_size)
// {
// 	int ret_value = 0;
// 	struct i2c_msg msg[2];
// 	struct i2c_adapter *adap = dw9784_i2c_client->adapter;

// 	memset(msg, 0, sizeof(msg));

// 	msg[0].addr = dw9784_i2c_client->addr;
// 	msg[0].flags = 0;
// 	msg[0].len = size;
// 	msg[0].buf = dat;

// 	msg[1].addr = dw9784_i2c_client->addr;
// 	msg[1].flags = I2C_M_RD;
// 	msg[1].len = ret_size;
// 	msg[1].buf = ret;

// 	ret_value = i2c_transfer(adap, msg, 2);
// 	if (ret_value < 0) {
// 		LOG_INF("I2C read - recv failed!!\n");
// 		return -1;
// 	}

// 	return 0;
// }

// static signed short dw9784formUShortDate(unsigned char gt[DW9784_DATA_BIT], unsigned char num)
// {
//     return (gt[num]<<8) + gt[num+1];
// }

/**************************************************************************
Function	: Get EIS Data
Date		: 2021.05.20
Version		: 0.001
History		:
***************************************************************************/

static void getDW9784Pos(struct DW9784PosInfo *dw9784_plt_info) {
    int i;
    uint64_t temptimestamp;
    static unsigned int lastflag = 0;
    int targetIndex = -1;
    unsigned int expectedFlag;
    struct DW9784TransmitBufferData selectedBuffer;

    // LOG_INF("wxyy:lastflag = %u\n", lastflag);
    // LOG_INF("wxyy:Buffer Range: Min flagnum = %u, Max flagnum = %u\n", bufferArray[0].flagnum, bufferArray[BUFFER_ARRAY_SIZE - 1].flagnum);

    if (lastflag < bufferArray[0].flagnum) {
        targetIndex = BUFFER_ARRAY_SIZE - 1;
        //LOG_INF("wxyy:Case 1 triggered. Selecting the last entry with max flagnum.\n");
    } else {
        // while (1) {
		// 	if(lastflag == bufferArray[BUFFER_ARRAY_SIZE - 1].flagnum)
		// 	{
		// 		LOG_INF("wxyy:No new data available. Waiting for new data...\n");
        //     	msleep(1);
		// 	}
		// 	else
		// 	break;
            
        // }

        expectedFlag = lastflag + 1;
        //LOG_INF("wxyy:Case 2 triggered. Expected flagnum = %u\n", expectedFlag);

        for (i = 0; i < BUFFER_ARRAY_SIZE; i++) {
            if (bufferArray[i].flagnum == expectedFlag) {
                targetIndex = i;
                break;
            }
        }

        if (targetIndex == -1) {
            targetIndex = BUFFER_ARRAY_SIZE - 1;
        }
    }

    if (targetIndex < 0 || targetIndex >= BUFFER_ARRAY_SIZE) {
        LOG_INF("wxyy:Invalid targetIndex: %d\n", targetIndex);
        return;
    }

    selectedBuffer = bufferArray[targetIndex];

    temptimestamp = ((uint64_t)(selectedBuffer.buffer.latestOISTimeStampH_high & 0xFFFF) << 48) |
                    ((uint64_t)(selectedBuffer.buffer.latestOISTimeStampH_low & 0xFFFF) << 32) |
                    ((uint64_t)(selectedBuffer.buffer.latestOISTimeStampL_high & 0xFFFF) << 16) |
                    ((uint64_t)(selectedBuffer.buffer.latestOISTimeStampL_low & 0xFFFF));

    lastflag = selectedBuffer.flagnum;
    //LOG_INF("wxyy:Updated lastflag to %u\n", lastflag);

    for (i = 0; i < 16; i++) {
        dw9784_plt_info->offsettimestamps[i] = temptimestamp - 30000000 + (uint64_t)(2000000 * i);
        dw9784_plt_info->lens_pos[i].i4x = selectedBuffer.buffer.positionDataX[i];
        dw9784_plt_info->lens_pos[i].i4y = selectedBuffer.buffer.positionDataY[i];
    }

	// /*debug info*/
	// for (i = 0; i < 16; i++) {
	// 	dw9784_plt_info->offsettimestamps[i] = temptimestamp - 30000000 + (uint64_t)(2000000 * i);
	// 	LOG_INF("wxyy: offsettimestamps[%d] = %llu\n", i, dw9784_plt_info->offsettimestamps[i]);

	// 	dw9784_plt_info->lens_pos[i].i4x = selectedBuffer.buffer.positionDataX[i];
	// 	dw9784_plt_info->lens_pos[i].i4y = selectedBuffer.buffer.positionDataY[i];
	// 	LOG_INF("wxyy: lens_pos[%d].i4x = %d, lens_pos[%d].i4y = %d\n", 
	// 			i, dw9784_plt_info->lens_pos[i].i4x, i, dw9784_plt_info->lens_pos[i].i4y);
	// }
	// getGyroData();
	// /*debug info*/
}
/**************************************************************************
Function	: Erase MTP Data
Date		: 2021.05.20
Version		: 0.001
History		:
***************************************************************************/
static int erase_mtp(void)
{
	LOG_DBG("dw9784 erase mtp starting..\r\n");
	// 4k Sector_0
	RamWrite32A_DW9784OIS(0xDE03, 0x0000, 2);
	// 4k Sector Erase
	RamWrite32A_DW9784OIS(0xDE04, 0x0002, 2);
	mdelay(10);

	// 4k Sector_1
	RamWrite32A_DW9784OIS(0xDE03, 0x0008, 2);
	// 4k Sector Erase
	RamWrite32A_DW9784OIS(0xDE04, 0x0002, 2);
	mdelay(10);

	// 4k Sector_2
	RamWrite32A_DW9784OIS(0xDE03, 0x0010, 2);
	// 4k Sector Erase
	RamWrite32A_DW9784OIS(0xDE04, 0x0002, 2);
	mdelay(10);

	// 4k Sector_3
	RamWrite32A_DW9784OIS(0xDE03, 0x0018, 2);
	// 4k Sector Erase
	RamWrite32A_DW9784OIS(0xDE04, 0x0002, 2);
	mdelay(10);

	// 4k Sector_4
	RamWrite32A_DW9784OIS(0xDE03, 0x0020, 2);
	// 4k Sector Erase
	RamWrite32A_DW9784OIS(0xDE04, 0x0002, 2);
	mdelay(10);
	LOG_DBG("dw9784 erase mtp finish\r\n");
	return 0;
}

void dw9784_pid_erase(void)
{
	LOG_DBG("[dw9784_pid_erase] start pid flash(IF) erase");
	RamWrite32A_DW9784OIS(0xde03, 0x0000, 2);			// page 0
	mdelay(1);
	RamWrite32A_DW9784OIS(0xde04, 0x0008, 2);			// page erase
	mdelay(10);											// need to delay after erase
	LOG_DBG("[dw9784_pid_erase] finish");	
}

static void set_download_mode(void)
{

	LOG_DBG("dw9784 set download mode starting..\r\n");
	// release all protection
	RamWrite32A_DW9784OIS(0xFAFA, 0x98AC, 2);
	mdelay(1);
	RamWrite32A_DW9784OIS(0xF053, 0x70BD, 2);
	mdelay(1);

	LOG_DBG("dw9784 set download mode finish\r\n");
}

/**************************************************************************
Function	: Load FW Data
Date		: 2021.05.20
Version		: 0.001
History		:
***************************************************************************/
static int loadDownload(void)
{
	unsigned short startRamAddr = MTP_START_ADDRESS;
	unsigned short *currentData = DW9784_BIN; 
	unsigned short delayTime = 10; 
	unsigned int i = 0;

	for ( i = 0; i < DW9784_FW_SIZE; i += 256) {
		int status = RamBurstWrite256_DW9784OIS(startRamAddr + i, currentData, delayTime);
		if (status != 0) {
			LOG_INF("Write failed at address 0x%x\n", startRamAddr + i);
			break;
		}
		currentData += 256;
	}
	return 0;
}

static int pid_loadDownload(void)
{
	unsigned short startRamAddr = IF_START_ADDRESS;
	unsigned short *currentData = DW9784_BIN + DW9784_FW_SIZE; 
	unsigned short delayTime = 10; 
	unsigned int i = 0;

	for ( i = 0; i < DW9784_PID_SIZE; i += 256) {
		int status = RamBurstWrite256_DW9784OIS(startRamAddr + i, currentData, delayTime);
		if (status != 0) {
			LOG_INF("Write failed at address 0x%x\n", startRamAddr + i);
			break;
		}
		currentData += 256;
	}
	return 0;
}
/**************************************************************************
Function	: FW Update
Date		: 2021.05.20
Version		: 0.001
History		:
***************************************************************************/
static int DW9784_FlashDownload_Seq(void)
{
	unsigned short fw_version = 0;
	unsigned short FMC = 0;
	unsigned short state_check = 0;
	// RamWrite32A_DW9784OIS(0xD002, 0x0001, 2);	// Logic reset
	// mdelay(4);

	// DSP Disable
	RamWrite32A_DW9784OIS(0xD001, 0x0000, 10);
	mdelay(4);

	// step 1	: MTP setup
	set_download_mode();		// protection release
	LOG_DBG("flash ready\r\n");

	RamWrite32A_DW9784OIS(0xDE01, 0x0000, 2);
	// FMC Register Check
	RamRead32A_DW9784OIS(0xDE01, &FMC);		// second_info: 0x0020
	LOG_DBG("FMC register value : %04x\r\n", FMC);
	if (FMC != 0)
	{
		// FW block selection
		RamWrite32A_DW9784OIS(0xDE01, 0x0000, 2);
		mdelay(1);
		FMC = 0;
		RamRead32A_DW9784OIS(0xDE01, &FMC);
		if (FMC != 0)
		{
			LOG_DBG("FMC 2nd register value : %04x\r\n", FMC);
			LOG_DBG("stop f/w download\r\n");
			return 0;
		}
	}
	
	// step 2	: release all protection
	RamWrite32A_DW9784OIS(0xFD00, 0x5252, 2);//release all protection ***
	mdelay(1);

	// step 3   : MTP Erase
	erase_mtp();
	LOG_DBG("flash erase\r\n");

	// step 4	: Seq Write All Flash
	loadDownload();
	LOG_DBG("flash write complete\r\n");

	/* step 5. change to IF flash.	(FMC register check) */
	RamWrite32A_DW9784OIS(0xDE01, 0x1000, 2);
	mdelay(1);
	RamRead32A_DW9784OIS(0xDE01, &FMC);
	mdelay(1);
	if(FMC != 0x1000) {
		LOG_DBG("FMC register check fail state:0x%04x\n", FMC);
		return 0;
	}

	// step 6	: erase pid
	dw9784_pid_erase();

	// step 7	: download pid data
	pid_loadDownload();

	// step 8 : OIS Reset after Download FW
	dw9784_reset();

	RamRead32A_DW9784OIS(0x700D, &state_check);
	if(state_check != 0x0000) {
		LOG_INF("FW checksum error state:0x%04x\n", state_check);
		return 0;
	}

	// step 9 : check fw_version
	RamRead32A_DW9784OIS(0x7001, &fw_version);
	LOG_DBG("FW version = 0x%4X\r\n", fw_version);
	return 0;
}

/**************************************************************************
Function	: Chip information
Date		: 2021.08.13
Version		: 0.001
History		:
***************************************************************************/
static unsigned short use_gyro_gain_x = 0;
static unsigned short use_gyro_gain_y = 0;
static unsigned short gainbasex = 0;
static unsigned short gainbasey = 0;
static void ChipInfo(void)
{
	unsigned short chipID;
	unsigned short fw_Ver;
	unsigned short fw_Date;
	unsigned short gyro_type;
	unsigned short gyro_gain_orient[4] = {0};
	int i;
	// unsigned short gyro_gainx,gyro_gainy;

	RamRead32A_DW9784OIS(0x7000, &chipID); // Chip ID
	RamRead32A_DW9784OIS(0x7001, &fw_Ver); // fw_Ver
	RamRead32A_DW9784OIS(0x7002, &fw_Date); // fw_Date
	RamWrite32A_DW9784OIS(0x7194, 0x000B, 2); // gyro type
	RamRead32A_DW9784OIS(0x7194, &gyro_type);
	RamRead32A_DW9784OIS(0x7182, &gainbasex);//X_GYRO_GAIN
	RamRead32A_DW9784OIS(0x7183, &gainbasey);//Y_GYRO_GAIN

	for(i = 0;i<4;i++)
	{
		RamRead32A_DW9784OIS(0x7186 +i , &gyro_gain_orient[i]);//X_GYRO_GAIN
		LOG_INF("gyro gain orient 0x%04X : 0x%04X\r\n", 0x7186 +i, gyro_gain_orient[i]);
	}

	LOG_INF("ChipID 0x7000 : 0x%04X\r\n", chipID);
	LOG_INF("fw_Ver 0x7001 : 0x%04X\r\n", fw_Ver);
	LOG_INF("fw_Date 0x7002 : 0x%04X\r\n", fw_Date);
	LOG_INF("gyro type 0x7194 : 0x%04X\r\n", gyro_type);
	LOG_INF("base gyro gain x 0x7182 : 0x%04X\r\n", gainbasex);
	LOG_INF("base gyro gain y 0x7183 : 0x%04X\r\n", gainbasey);

	// RamWrite32A_DW9784OIS(0x7182, use_gyro_gain_x, 2);
	// RamWrite32A_DW9784OIS(0x7183, use_gyro_gain_y, 2);
	// LOG_INF("change gyro gain use X : %d , Y : %d ", use_gyro_gain_x ,use_gyro_gain_y);
	// RamRead32A_DW9784OIS(0x7182, &gyro_gainx);//X_GYRO_GAIN
	// RamRead32A_DW9784OIS(0x7183, &gyro_gainy);//Y_GYRO_GAIN
	// LOG_INF("check gyro gain x 0x7182 : %d\r\n", gyro_gainx);
	// LOG_INF("check gyro gain y 0x7183 : %d\r\n", gyro_gainy);
}

/**************************************************************************
Function	: autoUpdateFwVer
Date		: 2021.05.21
Version		: 0.001
History		:
***************************************************************************/
static void autoUpdateFwVer(kal_bool enable)
{
	unsigned short chipID;
	unsigned short newfwVer;
	unsigned short currfwVer;
	unsigned short currfwDate;
	newfwVer = DW9784_BIN[10493];
	RamRead32A_DW9784OIS(0x7000, &chipID);
	RamRead32A_DW9784OIS(0x7001, &currfwVer);
	RamRead32A_DW9784OIS(0x7002, &currfwDate);
	LOG_INF("wxy: enable = %d ,newfwVer = %d , currfwVer = %d,chipID = %d",enable,newfwVer,currfwVer,chipID);
	if ((enable) && ((newfwVer != currfwVer) || (chipID == 0xFFFF) || DW9784_BIN[10494] != currfwDate)){
		LOG_DBG("CurrfwVer: 0x%x, newfwVer: 0x%x\n",currfwVer, newfwVer);
		DW9784_FlashDownload_Seq();

		LOG_DBG("autoUpdateFwVer Success\n");
	} else {
		LOG_DBG("autoUpdateFwVer Skip\n");
	}
}

/**************************************************************************
Function	: Calibration Data Save
Date		: 2021.08.13
Version		: 0.001
History		:
***************************************************************************/
static int CalSave(void)
{
    unsigned short Data;

	// RamWrite32A_DW9784OIS(0x7011, 0x0002, 2);	//Set ServoOff
	// mdelay(10);
	RamWrite32A_DW9784OIS(0x7012, 0x000A, 2);	//Set Cal
	RamWrite32A_DW9784OIS(0xFD00, 0x5252, 2);	//flash protection off
	RamWrite32A_DW9784OIS(0x700F, 0x5959, 2);	//set cal memory
	RamWrite32A_DW9784OIS(0x7011, 0x0001, 2);	//cmd_update
	mdelay(140);
	RamRead32A_DW9784OIS(0x7010, &Data);//Read status
	LOG_DBG("calsave result Data:0x%x,(0xA001 is ok)\n",Data);

	if(use_af_in_protect == 1){
	dw9784caf_target(dw9784_save_af_target);
	LOG_DBG("OIS calsave done,move af to %d\n",dw9784_save_af_target);
	use_af_in_protect = 0;
	}

	return 0;
}

/**************************************************************************
Function	: OIS Reset
Date		: 2019.05.31
Version		: 0.001
History		: 
***************************************************************************/
static int dw9784_reset(void)
{
	dw9784reset_on = 1;
	LOG_DBG("OIS Reset+ ,set dw9784reset_on = 1\n");

	RamWrite32A_DW9784OIS(0xD002, 0x0001, 4);	// Logic reset

	RamWrite32A_DW9784OIS(0xD001, 0x0001, 25);	// Active mode (DSP ON)

	RamWrite32A_DW9784OIS(0xEBF1, 0x56FA, 1);	// User protection release

	// RamWrite32A_DW9784OIS(0x701C, 0x0001, 2);	// close Pantilt
	// RamWrite32A_DW9784OIS(0x7186, 0x04B0, 2);	// Set Pantilt x 1.2 Degree wxy 
	// RamWrite32A_DW9784OIS(0x7187, 0x04B0, 2);	// Set Pantilt x 1.2 Degree wxy 
	// RamWrite32A_DW9784OIS(0x7188, 0x04B0, 2);	// Set Pantilt y 1.2 Degree wxy
	// RamWrite32A_DW9784OIS(0x7189, 0x04B0, 2);	// Set Pantilt y 1.2 Degree wxy 
	// RamWrite32A_DW9784OIS(0x71E4, 0x0384, 2);
	dw9784reset_on = 0;
	LOG_DBG("OIS Reset- ,set dw9784reset_on = 0\n");

	if(use_af_in_protect == 1){
	dw9784caf_target(dw9784_save_af_target);
	LOG_DBG("OIS reset done,move af to %d\n",dw9784_save_af_target);
	use_af_in_protect = 0;
	}

	return 0;
}

/**************************************************************************
Function	: Gyro RAW Status
Date		: 2021.08.13
Version		: 0.001
History		:
***************************************************************************/
static void getGyroData(void)
{
    short oisSt, PositionX, PositionY;
    short GyroRawX, GyroRawY;
    short GyroAngleX, GyroAngleY;
    short HallRawX, HallRawY;
    short Ois_TargetX, Ois_TargetY;

    RamRead32A_DW9784OIS(0x7010, (unsigned short*)&oisSt); // motionFlag

    RamRead32A_DW9784OIS(0x7101, (unsigned short*)&PositionX); // X Position
    RamRead32A_DW9784OIS(0x7131, (unsigned short*)&PositionY); // Y Position

    RamRead32A_DW9784OIS(0x7110, (unsigned short*)&GyroRawX); // X Gyro Raw
    RamRead32A_DW9784OIS(0x7140, (unsigned short*)&GyroRawY); // Y Gyro Raw

    RamRead32A_DW9784OIS(0x7116, (unsigned short*)&GyroAngleX); // X Gyro Filter
    RamRead32A_DW9784OIS(0x7146, (unsigned short*)&GyroAngleY); // Y Gyro Filter

    RamRead32A_DW9784OIS(0x7102, (unsigned short*)&HallRawX); // X Hall Raw
    RamRead32A_DW9784OIS(0x7132, (unsigned short*)&HallRawY); // Y Hall Raw

    RamRead32A_DW9784OIS(0x7106, (unsigned short*)&Ois_TargetX); // X Ois_Target
    RamRead32A_DW9784OIS(0x7136, (unsigned short*)&Ois_TargetY); // Y Ois_Target

    LOG_INF("dw9784GetGyroData oislogcnt: %d, oisStatus: %d, "
            "Position: %d, %d, GyroRaw: %d, %d, GyroAngle: %d, %d, "
            "HallRaw: %d, %d, Ois_Target: %d, %d\r\n",
            oislogcnt, oisSt,
            PositionX, PositionY,
            GyroRawX, GyroRawY,
            GyroAngleX, GyroAngleY,
            HallRawX, HallRawY,
            Ois_TargetX, Ois_TargetY);

    oislogcnt += 1;
}
/**************************************************************************
Function	: get position for eis
Date		: 2025.04.10
Version		: 0.001
History		:
***************************************************************************/

// static spinlock_t bufferLock;
// static unsigned int currentWriteIndex = 0; 
// static unsigned int dataflag = 0; 

// static void getPosData(void) {
//     int j;
//     unsigned short bulk_data[37]; // 1+4+16*2
//     int ret;

//     struct DW9784TransmitBufferData tempBuffer;
//     //mutex_lock(&dw9784ois_mutex);


// 	ret = RamReadBulkDW9784OIS(BASE_PHYS_ADDR, bulk_data, 37);
		
		
// 	if (ret < 0) {
// 		LOG_INF("wxyy:Failed to read data from hardware. Error code: %d\n", ret);
// 		//mutex_unlock(&dw9784ois_mutex);
// 		return;
// 		}
// 	LOG_INF("wxyy:data0 = 0x%x",bulk_data[0]);
// 	if (currentWriteIndex >= BUFFER_ARRAY_SIZE) {
// 		memmove(bufferArray, &bufferArray[1], (BUFFER_ARRAY_SIZE - 1) * sizeof(struct DW9784TransmitBufferData));
// 		currentWriteIndex = BUFFER_ARRAY_SIZE - 1;
// 		//LOG_INF("wxyy:Buffer full, removed oldest entry.\n");
// 		}
		
// 	tempBuffer.buffer.latestOISTimeStampH_high = bulk_data[1];
// 	tempBuffer.buffer.latestOISTimeStampH_low = bulk_data[2];
// 	tempBuffer.buffer.latestOISTimeStampL_high = bulk_data[3];
// 	tempBuffer.buffer.latestOISTimeStampL_low = bulk_data[4];

// 	for (j = 0; j < 16; j++) {
// 		tempBuffer.buffer.positionDataX[j] = bulk_data[5 + j * 2];
// 		tempBuffer.buffer.positionDataY[j] = bulk_data[6 + j * 2];
// 		}

// 	tempBuffer.flagnum = dataflag++;

// 	bufferArray[currentWriteIndex] = tempBuffer;

// 	currentWriteIndex++;
	
	
// }


/**************************************************************************
Function	: Gyro Offset Calibration Status Check
Date		: 2021.08.13
Version		: 0.001 
History		:
***************************************************************************/
static unsigned short GyroOffsetStatus(void)
{
	unsigned short GyroOffsetCalResult;
	unsigned short XGyroOffset, YGyroOffset;
	
	RamRead32A_DW9784OIS(0x7195, &GyroOffsetCalResult);	// Gyro Cal. Status
	RamRead32A_DW9784OIS(0x7180, &XGyroOffset); 	// X Gyro Offset  
	RamRead32A_DW9784OIS(0x7181, &YGyroOffset); 	// Y Gyro Offset

	LOG_INF("====== Gyro Offset Cali Data Check ======\r\n");
	LOG_INF("Gyro Offset Cal. Status		: %04x\r\n", GyroOffsetCalResult);
	LOG_INF("X Gyro Offset			: %04x\r\n", XGyroOffset);
	LOG_INF("Y Gyro Offset			: %04x\r\n", YGyroOffset);

	LOG_INF("=========================================\r\n");

	if (GyroOffsetCalResult == 0x8003)
		return 0x0001;
	else
		return 0x0000;
}

/**************************************************************************
Function	: OIS Mode Set
Date		: 2021.08.13
Version		: 0.001
History		: 
***************************************************************************/
static uint16_t setpantilt = 0;
// static bool iscloselenspushback = false;
// static int set7166 = 0;
// static int set7167 = 0;
// static int set7168 = 0;
static bool set701C = true;
static int set7023 = 0;
static bool isopenrun = false;
static int set71B3 = 0;
static int set71D3 = 0;
static int set7024 = 6;
static int set7169 = 150;
static bool isopenpullback = true;
static bool isopentraj = false;
static int OisModeSelect(int nMode)
{
	unsigned short gyro_gainx,gyro_gainy;
	unsigned short read_707c;
	// unsigned short read7166,read7167,read7168;
	LOG_INF("OisMode[%d]-[0:On, 1:ServoOn, 2:Off]\n",nMode);
	switch(nMode)
	{
	case eAction_OIS_ON_SERVO_OFF:
		dw9784_reset();

		/*video mode and ai mode diffrience*/
		if(isopentraj == true){
			RamWrite32A_DW9784OIS(0x7025, 0x8000, 1);
			LOG_INF("set open7025 open ");
		}
		else{
			RamWrite32A_DW9784OIS(0x7025, 0x0000, 1);
			LOG_INF("set open7025 close");
		}
		if(isopenpullback == true){
			RamWrite32A_DW9784OIS(0x707C, 20, 1);
			LOG_INF("lenspushback speed 20 open");
		}
		else{
			RamWrite32A_DW9784OIS(0x707C, 0, 1);
			LOG_INF("lenspushback speed 0 close");
		}
		/*use diy gyro gain or defult gyro gain*/
		RamRead32A_DW9784OIS(0x7182, &gyro_gainx);//X_GYRO_GAIN
		RamRead32A_DW9784OIS(0x7183, &gyro_gainy);//Y_GYRO_GAIN
		LOG_INF("gyro gain x 0x7182 : 0x%04X\r\n", gyro_gainx);
		LOG_INF("gyro gain y 0x7183 : 0x%04X\r\n", gyro_gainy);
		if(use_gyro_gain_x == 0){
			RamWrite32A_DW9784OIS(0x7182, gainbasex, 2);
		}
		else{
			RamWrite32A_DW9784OIS(0x7182, use_gyro_gain_x, 2);
		}
		if(use_gyro_gain_y == 0){
			RamWrite32A_DW9784OIS(0x7183, gainbasey * 94 / 100, 2);
		}
		else{
			RamWrite32A_DW9784OIS(0x7183, use_gyro_gain_y, 2);
		}
		
		LOG_INF("change gyro gain params X : %d , Y : %d ", use_gyro_gain_x ,use_gyro_gain_y);

		/*change x or y loop gain*/
		if(set71B3 == 0){
			// RamWrite32A_DW9784OIS(0x71B3, 0x0200, 2); //X SERVO LOOP GAIN	
			// LOG_INF("set x loop gain to 512 default");
		}
		else{
			RamWrite32A_DW9784OIS(0x71B3, set71B3, 2); //X SERVO LOOP GAIN	
			LOG_INF("set x loop gain to 0x%x", set71B3);
		}
		if(set71D3 == 0){
			// RamWrite32A_DW9784OIS(0x71D3, 0x0200, 2); //Y SERVO LOOP GAIN	
			// LOG_INF("set y loop gain to 512 default");
		}
		else{
			RamWrite32A_DW9784OIS(0x71D3, set71D3, 2); //Y SERVO LOOP GAIN	
			LOG_INF("set y loop gain to 0x%x", set71D3);
		}
		

		/*set pantilt*/
		if(setpantilt == 0){
			RamWrite32A_DW9784OIS(0x7022, 0x02BC, 2);
			LOG_INF("set pantilt to 700");
		}		
		else{
			RamWrite32A_DW9784OIS(0x7022, setpantilt, 2);
			LOG_INF("set pantilt to 0x%x", setpantilt);
		}

	

		// /*set 7166 7167 7168*/
		// if(set7166 != 0){
		// 	RamWrite32A_DW9784OIS(0x7166, set7166, 2);
		// 	LOG_INF("change 7166 to 0X%X",set7166);
		// }
		// if(set7167 != 0){
		// 	RamWrite32A_DW9784OIS(0x7167, set7167, 2);
		// 	LOG_INF("change 7167 to 0X%X",set7167);
		// }
		// if(set7168 != 0){
		// 	RamWrite32A_DW9784OIS(0x7168, set7168, 2);
		// 	LOG_INF("change 7168 to 0X%X",set7168);
		// }

		/*open detected running mode*/
		if(isopenrun == true){
			RamWrite32A_DW9784OIS(0x701E, 0x8000, 2); //0x8000 is open ,0x0000 is close
			LOG_INF("run is open");
		}
		else{
			// RamWrite32A_DW9784OIS(0x701E, 0x0000, 2); //0x8000 is open ,0x0000 is close
			LOG_INF("run is close");
		}
		
		/*control pantilt open or close*/
		if(set701C == true){
			// RamWrite32A_DW9784OIS(0x701C, 0x0000, 2);
			LOG_INF("pantilt is open");
		}
		else{
			RamWrite32A_DW9784OIS(0x701C, 0x8000, 2); 
			LOG_INF("pantilt is close");
		}

		/*control pantilt return limit*/
		if(set7023 == 0){
			// RamWrite32A_DW9784OIS(0x7023, 0x0800, 2); 
			LOG_INF("pantilt limit is 2048");
		}
		else{
			RamWrite32A_DW9784OIS(0x7023, set7023, 2); 
			LOG_INF("change 7023 to 0X%X",set7023);
		}

		/*set LPF coefficent and enable*/
		if(set7024 == 6){
			RamWrite32A_DW9784OIS(0x7024, 6, 2); 
			LOG_INF("set LPF coefficent bypass default");
		}
		else{
			RamWrite32A_DW9784OIS(0x7024, set7024, 2); 
			LOG_INF("set LPF coefficent %d ,0:50hz ,1:100hz ,2:150hz ,3:200hz ,4:250hz ,5:500hz",set7024);
		}

		/*set trajectory coeff*/
		if(set7169 == 150){
			// RamWrite32A_DW9784OIS(0x7169, 150, 2); 
			LOG_INF("set trajectory coeff 150 default");
		}
		else{
			RamWrite32A_DW9784OIS(0x7169, set7169, 2); 
			LOG_INF("set trajectory coeff  %d ",set7169);
		}

		RamWrite32A_DW9784OIS(0x7012, 0x0001, 2);
		RamWrite32A_DW9784OIS(0x7011, 0x0001, 20); //Servo ON
		RamWrite32A_DW9784OIS(0x7012, 0x0001, 2);
		RamWrite32A_DW9784OIS(0x7011, 0x0000, 2); //OIS ON


		RamRead32A_DW9784OIS(0x707C, &read_707c);//read_707c
		LOG_INF("read_707c : 0x%04X\r\n", read_707c);

		RamRead32A_DW9784OIS(0x7182, &gyro_gainx);//X_GYRO_GAIN
		RamRead32A_DW9784OIS(0x7183, &gyro_gainy);//Y_GYRO_GAIN

		// RamRead32A_DW9784OIS(0x7166, &read7166);//read 7166
		// RamRead32A_DW9784OIS(0x7167, &read7167);//read 7167
		// RamRead32A_DW9784OIS(0x7168, &read7168);//read 7168
		LOG_INF("check3 gyro gain x 0x7182 : %d\r\n", gyro_gainx);
		LOG_INF("check3 gyro gain y 0x7183 : %d\r\n", gyro_gainy);
		// LOG_INF("read 7166 : 0X%X\r\n", read7166);
		// LOG_INF("read 7167 : 0X%X\r\n", read7167);
		// LOG_INF("read 7168 : 0X%X\r\n", read7168);
		break;
	case eAction_OIS_OFF_SERVO_ON:
		dw9784_reset();
		RamWrite32A_DW9784OIS(0x7012, 0x0001, 2);
		RamWrite32A_DW9784OIS(0x7011, 0x0001, 2); //Servo ON
		break;
	case eAction_OIS_OFF_SERVO_OFF:
		RamWrite32A_DW9784OIS(0x7012, 0x0001, 2);
		RamWrite32A_DW9784OIS(0x7011, 0x0002, 2); //OIS OFF
		break;
	case eAction_STANDBY:
		RamWrite32A_DW9784OIS(0x7012, 0x0001, 2);
		RamWrite32A_DW9784OIS(0x7011, 0x0001, 20); //Servo ON
		RamWrite32A_DW9784OIS(0xD001, 0x0000, 10); //DSP disable
		break;
	default:
		break;
	}
    return 0;
}

/**************************************************************************
Function	: Gyro Offset Calibration
Date		: 2021.08.13
Version		: 0.001
History		: 
***************************************************************************/
static int GyroOfsCalibration(void)
{
	unsigned short Addr, Data, isdone;
	int ret = 0, msg = 0;

	dw9784_calsave_on = 1;
	LOG_DBG("cal set dw9784_calsave_on = 1");

	//Gyro offset
	RamWrite32A_DW9784OIS(0x7012, 0x0006, 2);//Set cali mode
	RamWrite32A_DW9784OIS(0x7011, 0x0001, 2);//MD_UPDATE execute wxy
	mdelay(1000);

	Addr = 0x7195; Data = 0; isdone = 0; //read gyro cal offset status
	RamRead32A_DW9784OIS(Addr, &Data);
	RamRead32A_DW9784OIS(0x7010, &isdone);
	LOG_INF("Gyro Offset Cali Status:0x%x\n",Data);
	LOG_INF("Gyro Offset Cali isdone?:0x%x(0x6001 is done)\n",isdone);
	if ( Data & 0x8000 )
	{
		msg += GYRO_OFFSET_CAL_OK;
	}
	else
	{
		if (Data & 0x20)
		{
			msg += Y_GYRO_OFFSET_SPEC__ABNORMAL;
		}
		if (Data & 0x10)
		{
			msg += X_GYRO_OFFSET_SPEC__ABNORMAL;
		}
		if (Data & 0x02)
		{
			msg += Y_GYRO_OFFSET_SPEC__NG;
		}
		if (Data & 0x01)
		{
			msg += X_GYRO_OFFSET_SPEC__NG;
		}
	}
	LOG_INF("Gyro Offset msg:0x%x\n",msg);
	ret = GyroOffsetStatus();
	if (ret > 0)
		CalSave();

	dw9784_calsave_on = 0;
	LOG_DBG("cal set dw9784_calsave_on = 0");

	return ret;
}

static int dw9784_powerup_sequence(void)
{
    int ret = 0;
 	g_pOIS_Opened += 1;
	return ret;
}

static int dw9784_powerdown_sequence(void)
{
	int ret = 0;
	g_pOIS_Opened = 0;
	return ret;
}

static void dw9784_ic_init(void)
{
	LOG_DBG("OIS INIT+ \n");
	if (g_pOIS_Opened == 1){
		dw9784_calsave_on = 0;
		dw9784reset_on = 0;
		dw9784_save_af_target = 0;
		use_af_in_protect = 0;
		dw9784_reset();
		ChipInfo();
		GyroOffsetStatus();
		OisModeSelect(eAction_STANDBY);
	} else {
		LOG_INF("OIS Opened, Get Flag:%d\n", g_pOIS_Opened);
	}
	LOG_DBG("OIS INIT- \n");
}

static void dw9784ois_init(void)
{
	unsigned short ismaster;
	LOG_DBG("dw9784ois_init+ \n");
	if (g_pOIS_Opened == 0){
		dw9784_powerup_sequence();
		dw9784_ic_init();
		RamRead32A_DW9784OIS(0x7020, &ismaster); // 0 is master
		LOG_INF("wxy ismaster:%d\n", ismaster);

		// if(ismaster == 0)
		// RamWrite32A_DW9784OIS(0x7020, 0x0001, 2);// 1 is intercpet
		// RamRead32A_DW9784OIS(0x7020, &ismaster); // 0 is master
		// LOG_INF("wxy modify ismaster:%d\n", ismaster);
	}else{
		g_pOIS_Opened += 1;
		LOG_INF("[OIS] Dev Opened, Set Flag:%d\n", g_pOIS_Opened);
	}
	LOG_DBG("dw9784ois_init- \n");
}

void dw9784ois_poweroff(void)
{
	LOG_DBG("dw9784ois_poweroff+ \n");
	if (g_pOIS_Opened == 1)
		dw9784_powerdown_sequence();
	else{
		g_pOIS_Opened -= 1;
		LOG_INF("Some devices are still in use. Skip");
	}
	LOG_DBG("dw9784ois_poweroff- \n");
}

int dw9784caf_init(void)
{
	LOG_DBG("dw9784caf_init+ \n");
	if (g_pOIS_Opened == 0){
		dw9784_powerup_sequence();
		dw9784_ic_init();
	}else{
		g_pOIS_Opened += 1;
		LOG_INF("[CAF] Dev Opened, Set Flag:%d\n", g_pOIS_Opened);
	}
	LOG_DBG("dw9784caf_init- \n");
	return 0;
}
EXPORT_SYMBOL(dw9784caf_init);

void dw9784caf_poweroff(void)
{
	LOG_DBG("dw9784caf_poweroff+ \n");
	if (g_pOIS_Opened == 1)
		dw9784_powerdown_sequence();
	else{
		g_pOIS_Opened -= 1;
		LOG_INF("Some devices are still in use. Skip");
	}
	LOG_DBG("dw9784caf_poweroff- \n");
}
EXPORT_SYMBOL(dw9784caf_poweroff);

int dw9784caf_target(unsigned long afTarget)
{
	int ret = 0;
	// LOG_DBG("dw9784caf_target = %d \n", afTarget);
	if(dw9784reset_on == 1 || dw9784_calsave_on == 1){
		use_af_in_protect = 1;
		dw9784_save_af_target = afTarget;
	}
	else {
		ret = RamWrite32A_DW9784OIS(0xD013, afTarget, 2);
	}
	
	return ret;
}
EXPORT_SYMBOL(dw9784caf_target);

static bool ois_log_enabled = false;
static bool isjustservoon = false;
static long dw9784_ioctl(struct file *file,
        unsigned int cmd, unsigned long arg)
{
    void __user *ubuf = (void __user *)arg;
    int calistatus = 0;
    int ret = 0;
    struct IODW9784PInfo ioInfo;
    struct DW9784PosInfo *posInfo;
	static struct sched_param param = { .sched_priority = 10 };
	int timeout_count = 0;
	int suspen_timeout_count = 0;
	// LOG_INF("wxy: dw9784_ioctl cmd %d ",cmd);
    switch (cmd) {
    case DW9784_POWERON:
	LOG_INF("wxy recive cmd %d, DW9784_POWERON",cmd);
        dw9784ois_init();
        break;
	case DW9784_RESUMEOIS:
		LOG_INF("wxy recive cmd %d, DW9784_RESUMEOIS", cmd);
		mutex_lock(&dw9784ois_mutex);
		if(isjustservoon == false){
			OisModeSelect(eAction_OIS_ON_SERVO_OFF);
		}
		else{
			OisModeSelect(eAction_OIS_OFF_SERVO_ON);
		}
		
		if(ois_log_enabled){
		hrtimer_start(&log_timer, ktime_set(0, 6666666), HRTIMER_MODE_REL);	
		}
		mutex_unlock(&dw9784ois_mutex);
		break;
    case DW9784_SUSPENDOIS:
	LOG_INF("wxy recive cmd %d, DW9784_SUSPENDOIS",cmd);
		for (suspen_timeout_count = 0; suspen_timeout_count < 1000; suspen_timeout_count++) {
			if (dw9784reset_on == 0 && dw9784_calsave_on == 0) {
				break;
			}
			mdelay(1);
		}

		if (suspen_timeout_count >= 1000) {
			LOG_INF("wxy receive cmd %d timeout", cmd);
		}

		OisModeSelect(eAction_STANDBY);
        break;
    case DW9784_CLOSE:
	LOG_INF("wxy recive cmd %d, DW9784_CLOSE",cmd);
        dw9784ois_poweroff();
        break;
    case DW9784_GET_POS_INFO:
	// LOG_INF("wxy recive cmd %d, DW9784_GET_POS_INFO",cmd);
    //     LOG_DBG("DW9784_GET_POS_INFO +");
        if (copy_from_user(&ioInfo, ubuf, sizeof(struct IODW9784PInfo))){
            ret = -EINVAL;
            break;
        }
        posInfo = kmalloc(ioInfo.size, GFP_KERNEL);
        getDW9784Pos(posInfo);
        ret = copy_to_user((void __user *)ioInfo.pInfo, posInfo, sizeof(struct DW9784PosInfo));
		
        // LOG_DBG("DW9784_GET_POS_INFO -");
        kfree(posInfo);
        break;
    case DW9784_GYRO_CALI:
		LOG_INF("wxy recive cmd %d, DW9784_GYRO_CALI",cmd);
        calistatus = GyroOfsCalibration();
        ret = copy_to_user((void __user *)ubuf, &calistatus, sizeof(int));
        break;
	case OISPLT_SET_WORKMODE:
		LOG_INF("wxy recive cmd %d, OISPLT_SET_WORKMODE",cmd);
		LOG_INF("wxy recive OISPLT_SET_WORKMODE,arg = %ld",arg);
		if(arg == 11){
			mutex_lock(&dw9784ois_mutex);
			if(qtimerTask == NULL){
			qtimerTask = kthread_run(kthread_worker_fn, &qtimerWorker, "qtimer_processor");
			}
			if (qtimerTask !=NULL) {
				sched_setscheduler_nocheck(qtimerTask, SCHED_FIFO, &param);
			}
			RamWrite32A_DW9784OIS(0x70DD, 0x1002, 2);

			hrtimer_start(&dw9784ois_timer, ktime_set(0, 32300000), HRTIMER_MODE_REL);
			mutex_unlock(&dw9784ois_mutex);
			mdelay(10);
		}
		if(arg == 101){
			ois_log_enabled = true;
            LOG_INF("OIS Debug log enabled.\n");
		}
		if(arg == 100){
			ois_log_enabled = false;
            LOG_INF("OIS Debug log disabled.\n");
		}
		if(arg >= 200000 && arg < 300000){
			use_gyro_gain_x = arg - 200000;
		}
		if(arg >= 300000 && arg < 400000){
			use_gyro_gain_y = arg - 300000;
		}
		if(arg >= 900000 && arg < 1000000){
			setpantilt = arg - 900000;
		}
		// if(arg == 81){
		// 	iscloselenspushback = true;
		// }
		// if(arg == 80){
		// 	iscloselenspushback = false;
		// }
		if(arg == 1){
			isjustservoon = true;
		}
		if(arg == 0){
			isjustservoon = false;
		}
		// if(arg >= 600000 && arg < 700000){
		// 	set7166 = arg - 600000;
		// }
		// if(arg >= 700000 && arg < 800000){
		// 	set7167 = arg - 700000;
		// }
		// if(arg >= 800000 && arg < 900000){
		// 	set7168 = arg - 800000;
		// }
		if(arg == 91){
			isopenrun = true;
		}
		if(arg == 90){
			isopenrun = false;
		}
		if(arg == 92){
			set701C = true;
		}
		if(arg == 93){
			set701C = false;
		}
		if(arg >= 500000 && arg < 600000){
			set7023 = arg - 500000;
		}
		if(arg >= 1100000 && arg < 1200000){
			set71B3 = arg - 1100000;
		}
		if(arg >= 1200000 && arg < 1300000){
			set71D3 = arg - 1200000;
		}
		if(arg >= 20 && arg < 30){
			set7024 = arg - 20;
		}
		if(arg == 102){
			isopentraj = true;
			isopenpullback = false;
		}
		if(arg == 103){
			isopentraj = false;
			isopenpullback = true;
		}
		if(arg >= 1300000 && arg < 1400000){
			set7169 = arg - 1300000;
		}
		if (arg == 200) {
			mdelay(200); 

			for (timeout_count = 0; timeout_count < 1000; timeout_count++) {
				if (dw9784reset_on == 0 && dw9784_calsave_on == 0) {
					break;
				}
				mdelay(1);
			}

			if (timeout_count >= 1000) {
				LOG_INF("wxy receive cmd %d timeout", cmd);
			}

			OisModeSelect(eAction_OIS_OFF_SERVO_ON);
		}
		break;
    default:
        LOG_DBG("No such command and arg: (%d, %ld)",
    			_IOC_NR(cmd), arg);
    	return -ENOTTY;
    }
    return 0;
}

static int ois_open(struct inode *inode, struct file *file){
	LOG_INF("ois_open\n");
	return 0;
}

static int dw9784_release(struct inode *inode, struct file *file)
{
	static bool is_thread_stopped = false;
	LOG_INF("dw9784_release\n");
	dw9784_powerdown_sequence();
    hrtimer_cancel(&dw9784ois_timer);
	hrtimer_cancel(&log_timer);
	if (qtimerTask != NULL && !IS_ERR(qtimerTask)) {
		if (!is_thread_stopped) {
			kthread_stop(qtimerTask);
			qtimerTask = NULL;
			is_thread_stopped = true;
			} else {
			LOG_INF("Thread already stopped");
			}
		} else {
			LOG_INF("Attempt to stop an invalid thread");
		}

	return 0;
}

static struct file_operations dw9784_fops = {
    .owner = THIS_MODULE,
    .open = ois_open,
    .release = dw9784_release,
    .unlocked_ioctl = dw9784_ioctl,
    .compat_ioctl = dw9784_ioctl,
};

/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static struct device *lens_device;

static struct regulator *vcam2_oisvdd;
static struct regulator *vcam2_oisvdda;

#define CAM2_OISVDD    "cam2_oisvdd"
#define CAM2_OISVDDA   "cam2_oisvdda"
static void cam2_ois_power_init(void)
{
    int ret;
    struct device_node *node, *kd_node;
	LOG_INF("cam2_ois_power_init+\n");
    if (!lens_device) {
        LOG_INF("lens_device not initialized!\n");
        return;
    }

    node = of_find_compatible_node(NULL, NULL, "mediatek,camera_tele_ois");
    if (node) {
        kd_node = lens_device->of_node;
        lens_device->of_node = node;

        if (vcam2_oisvdd == NULL) {
            vcam2_oisvdd = regulator_get(lens_device, CAM2_OISVDD);
            if (IS_ERR(vcam2_oisvdd)) {
                ret = PTR_ERR(vcam2_oisvdd);
                vcam2_oisvdd = NULL;
                LOG_INF("cannot get cam2_oisvdd regulator (%d)\n", ret);
            }
        }

        if (vcam2_oisvdda == NULL) {
            vcam2_oisvdda = regulator_get(lens_device, CAM2_OISVDDA);
            if (IS_ERR(vcam2_oisvdda)) {
                ret = PTR_ERR(vcam2_oisvdda);
                vcam2_oisvdda = NULL;
                LOG_INF("cannot get cam2_oisvdda regulator (%d)\n", ret);
            }
        }

        lens_device->of_node = kd_node;
    }
	LOG_INF("cam2_ois_power_init-\n");
}

static void cam2_ois_power_on_probe(void)
{
    int ret;
	LOG_INF("cam2_ois_power_on_probe+\n");
    if (vcam2_oisvdd) {
        ret = regulator_enable(vcam2_oisvdda);
        LOG_INF("cam2_oisvdda regulator enable (%d)\n", ret);
    }
	mdelay(1);
    if (vcam2_oisvdda) {
        ret = regulator_enable(vcam2_oisvdd);
        LOG_INF("cam2_oisvdd regulator enable (%d)\n", ret);
    }
	LOG_INF("cam2_ois_power_on_probe-\n");
	mdelay(10);
}

static void cam2_ois_power_off_probe(void)
{
    int ret;
	LOG_INF("cam2_ois_power_off_probe+\n");
    if (vcam2_oisvdda) {
        ret = regulator_disable(vcam2_oisvdd);
        LOG_INF("cam2_oisvdd regulator disable (%d)\n", ret);
    }
	mdelay(1);
    if (vcam2_oisvdd) {
        ret = regulator_disable(vcam2_oisvdda);
        LOG_INF("cam2_oisvdda regulator disable (%d)\n", ret);
    }
	LOG_INF("cam2_ois_power_off_probe-\n");
}
static int dw9784_i2c_probe(struct i2c_client *client)
{
	struct dw9784_chip_data *chip;
	struct dw9784_platform_data *pdata = client->dev.platform_data;
	int err;
	/*dw9784 position data buffer*/
	// buffer = kmalloc(sizeof(struct DW9784TransmitBuffer), GFP_KERNEL);
    // if (!buffer) {
    //     pr_err("Failed to allocate memory for OIS buffer\n");
    //     return -ENOMEM;
    // }
	LOG_INF("dw9784 dw9784_i2c_probe start.");
	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		LOG_INF("dw9784 Failed to check i2c functionality.");
		err = -ENODEV;
		goto err_out;
	}
	/* init chip private data */
	chip = kzalloc(sizeof(struct dw9784_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	/* init platform data */
	if (!pdata) {
		LOG_INF("Platform data does not exist");
		pdata = kzalloc(sizeof(struct dw9784_platform_data), GFP_KERNEL);
		if (!pdata) {
			LOG_INF("Failed to allocate memory.");
			err = -ENOMEM;
			goto err_init_pdata;
		}
		chip->no_pdata = 1;
	}
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	dw9784_i2c_client = client;

	LOG_INF("dw9784 %s IIC Addr :[0x%x]\n", __func__, dw9784_i2c_client->addr);

	chip->misc.minor = MISC_DYNAMIC_MINOR;
	chip->misc.name = "dw9784_ois";
	chip->misc.fops = &dw9784_fops;
	err = misc_register(&chip->misc);
	if (err < 0) {
		LOG_INF("Error: misc_register returned %d", err);
	}

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	dw9784ois_highpri_wq = alloc_workqueue("dw9784ois_highpri_wq", WQ_UNBOUND | WQ_HIGHPRI, 0);
	if (!dw9784ois_highpri_wq) {
		pr_err("Failed to create high priority workqueue\n");
		return -ENOMEM;
	}

	// /* init work queue */
	INIT_WORK(&dw9784ois_work, ois_status_print);
	kthread_init_worker(&qtimerWorker);
	kthread_init_work(&qtimerWork, dw9784ois_work_polling);
	/* init timer */
	hrtimer_init(&dw9784ois_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_init(&log_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dw9784ois_timer.function = dw9784ois_timer_func;
	log_timer.function = log_timer_func;
	// download fw sequence
	lens_device = &client->dev;
	cam2_ois_power_init();
	cam2_ois_power_on_probe();
	dw9784_reset();
	autoUpdateFwVer(KAL_TRUE);
	ChipInfo();
	GyroOffsetStatus();
	cam2_ois_power_off_probe();
	return 0;

err_init_pdata:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static void dw9784_i2c_remove(struct i2c_client *client)
{
	struct dw9784_chip_data *chip = i2c_get_clientdata(client);
	misc_deregister(&chip->misc);
	/* free resource */
	if (chip->no_pdata)
		kfree(chip->pdata);
	kfree(chip);
	/*free position data*/
	// if (buffer)
	// kfree(buffer);

	/* flush work queue */
	flush_work(&dw9784ois_work);
}

static const struct i2c_device_id dw9784_i2c_id[] = {
	{DW9784_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, dw9784_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id dw9784_i2c_of_match[] = {
	{.compatible = DW9784_DTNAME_I2C},
	{},
};
MODULE_DEVICE_TABLE(of, dw9784_i2c_of_match);
#endif

static struct i2c_driver dw9784_i2c_driver = {
	.driver = {
		   .name = DW9784_NAME,
#ifdef CONFIG_OF
		   .of_match_table = dw9784_i2c_of_match,
#endif
		   },
	.probe = dw9784_i2c_probe,
	.remove = dw9784_i2c_remove,
	.id_table = dw9784_i2c_id,
};

#ifdef CONFIG_OF
static const struct of_device_id dw9784_of_match[] = {
	{.compatible = DW9784_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, dw9784_of_match);
#else
static struct platform_device dw9784_platform_device = {

		.name = DW9784_NAME,
		.id = 0,
		.dev = {}

};
MODULE_DEVICE_TABLE(platform, dw9784_platform_device);
#endif


static ssize_t dw9784_ois_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	LOG_DBG("[sysfs_ois]show:%d \n", g_sysfs_ois);
	return sprintf(buf, "%u\n", g_sysfs_ois);
}

static ssize_t dw9784_ois_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int  mode = 0;
	sscanf(buf, "%d", &mode);
	LOG_DBG("[sysfs_ois]store:%d\n",mode);
    g_sysfs_ois = mode;
    switch(mode){
		LOG_INF("wxy case:%d +",mode);
        case 0: //ois-init
            dw9784ois_init();
            break;
        case 1: //ois-on
            OisModeSelect(eAction_OIS_ON_SERVO_OFF);
            break;
        case 2://ois-off
            OisModeSelect(eAction_OIS_OFF_SERVO_OFF);
            break;
        case 3://power-off
            dw9784ois_poweroff();
            break;
        case 4://getGyroData
            getGyroData();
            break;
        case 5://Gyro Offset Cali
            GyroOfsCalibration();
            break;
        case 8://down-fw
            dw9784_powerup_sequence();
            DW9784_FlashDownload_Seq();
            ChipInfo();
            dw9784_powerdown_sequence();
            break;
        default:
            break;
    }
	return size;
}

static DEVICE_ATTR_RW(dw9784_ois);
/* sys/class/dw9784/dw9784/ */

static int __init dw9784_init(void)
{
	int ret;

	ret = i2c_add_driver(&dw9784_i2c_driver);
	if (ret)
	{
		LOG_INF("dw9784 Failed to register dw9784 i2c driver: %d\n", ret);
		return ret;
	}

	LOG_DBG("[dw9784_dev]start\n");
	dw9784_class = class_create("dw9784");
	if (IS_ERR(dw9784_class))
	{
		LOG_INF("[dw9784_class]Unable to create class, err = %d\n", (int)PTR_ERR(dw9784_class));
		return 0;
	}
	dw9784_dev = device_create(dw9784_class, NULL, 0, 0,"dw9784");
	if(NULL == dw9784_dev)
	{
		LOG_INF("[dw9784_dev]device_create fail\n");
		return 0;
	}

	device_create_file(dw9784_dev, &dev_attr_dw9784_ois);

	LOG_DBG("[dw9784_dev]Done\n");
	return 0;
}

static void __exit dw9784_exit(void)
{
	i2c_del_driver(&dw9784_i2c_driver);

	device_remove_file(dw9784_dev, &dev_attr_dw9784_ois);
	device_unregister(dw9784_dev);
	if(dw9784_class != NULL)
	{
		class_destroy(dw9784_class);
	}
}

module_init(dw9784_init);
module_exit(dw9784_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("transsion ISP team");
MODULE_DESCRIPTION("TRAN DW9784 Driver");