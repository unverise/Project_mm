/*
 * tran_common sub2af 
 */
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"
#include "tran_commonaf.h"

#define AF_DRVNAME "TRANSUB2AF_DRV"

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...) \
	pr_info(AF_DRVNAME " [%s] " format, __func__, ##args)
#define LOG_DBG(format, args...) \
	pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#define LOG_DBG(format, args...)
#endif

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 4096;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

static int sub2af_bit=0;
static int sub2af_i2cslave_idx=0;
static int sub2af_chipid_idx=0;

static int s4AF_WriteReg(u16 a_u2Data)
{
	int ret = -1;
	char puSendCmd[3] = {0};
	unsigned int nArrayIndex = 0;
	int i = 0, j = 0;
	int retry = 3;

	g_pstAF_I2Cclient->addr = af_i2c_slave_addr[sub2af_i2cslave_idx];
	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	//LOG_INF("((a_u2Data >> 2) & 0xff=0x%x  (a_u2Data & 0x3) << 6)=0x%x\n",(char)((a_u2Data >> 2) & 0xff), (u16)((a_u2Data & 0x3) << 6));
	if (sub2af_chipid_idx >= ARRAY_SIZE(tran_vcm_configs) || sub2af_chipid_idx < 0) {
		LOG_INF("Invalid sub2af_chipid_idx: %d\n", sub2af_chipid_idx);
		return -1;
	}

	for (i = 0; i < tran_vcm_configs[sub2af_chipid_idx].I2CSendNum; ++i, nArrayIndex = 0) {
		int nCommNum = tran_vcm_configs[sub2af_chipid_idx].I2Cfmt[i].AddrNum +
			tran_vcm_configs[sub2af_chipid_idx].I2Cfmt[i].DataNum;

		// Fill address
		for (j = 0; j < tran_vcm_configs[sub2af_chipid_idx].I2Cfmt[i].AddrNum; ++j) {
			if (nArrayIndex >= ARRAY_SIZE(puSendCmd)) {
				LOG_INF("nArrayIndex(%d) exceeds the size of puSendCmd(%d)\n",
					nArrayIndex, (int)ARRAY_SIZE(puSendCmd));
				return -1;
			}
			puSendCmd[nArrayIndex] = tran_vcm_configs[sub2af_chipid_idx].I2Cfmt[i].Addr[j];
			++nArrayIndex;
		}

		// Fill data
		for (j = 0; j < tran_vcm_configs[sub2af_chipid_idx].I2Cfmt[i].DataNum; ++j) {
			if (nArrayIndex >= ARRAY_SIZE(puSendCmd)) {
				LOG_INF("nArrayIndex(%d) exceeds the size of puSendCmd(%d)\n",
					nArrayIndex, (int)ARRAY_SIZE(puSendCmd));
				return -1;
			}
			puSendCmd[nArrayIndex] =
				((((a_u2Data >> tran_vcm_configs[sub2af_chipid_idx].I2Cfmt[i].BitR[j]) &
				tran_vcm_configs[sub2af_chipid_idx].I2Cfmt[i].Mask1[j]) <<
				tran_vcm_configs[sub2af_chipid_idx].I2Cfmt[i].BitL[j]) &
				tran_vcm_configs[sub2af_chipid_idx].I2Cfmt[i].Mask2[j]) |
				tran_vcm_configs[sub2af_chipid_idx].I2Cfmt[i].CtrlData[j];
			++nArrayIndex;
		}

		while (retry-- > 0) {
			ret = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, nCommNum);
			if (ret >= 0)
				break;
		}

		if (ret < 0) {
			LOG_INF(
				"puSendCmd I2C failure i2c_master_send: %d, I2Cfmt[%d].AddrNum/DataNum: %d/%d\n",
				ret, i, tran_vcm_configs[sub2af_chipid_idx].I2Cfmt[i].AddrNum,
				tran_vcm_configs[sub2af_chipid_idx].I2Cfmt[i].DataNum);
			return ret;
		}
	}

	return 0;
}

static void register_setting(char table[][3], int table_size)
{
	int  ret = 0,i = 0,read_count = 0;
	for (i = 0; i < table_size; ++i) {

		LOG_INF("table[%d] = [0x%x 0x%x 0x%x]\n",
			i, table[i][0],
			table[i][1],
			table[i][2]);

		if (table[i][0] == 0)
			break;

		// write command
		if (table[i][0] == 0x1) {

			// write register
			ret = i2c_smbus_write_byte_data(g_pstAF_I2Cclient,
					table[i][1],
					table[i][2]);
        }else if (table[i][0] == 0x2) {
			read_count = 0;

			// read register and check value
			do {
				if (read_count > 10) {
					LOG_INF(
						"timeout, i2c read fail: %d, table[%d] = [0x%x 0x%x 0x%x]\n",
						ret, i, table[i][0],
						table[i][1],
						table[i][2]);
					break;
				}
				ret = i2c_smbus_read_byte_data(g_pstAF_I2Cclient,
					table[i][1]);
				read_count++;
			} while (ret != table[i][2]);

		// delay command
		} else if (table[i][0] == 0x3) {

			LOG_INF("delay time: %dms\n", table[i][1]);
			mdelay(table[i][1]);

		} else {
			// reserved
		}
		udelay(100);
	}
}

static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	stMotorInfo.bIsMotorMoving = 1;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo,
		sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

int tran_get_sub2af_parse_dts(struct device *lens_device)
{
	int ret;

	lens_device->of_node = of_find_compatible_node(
		NULL, NULL, "mediatek,camera_af_lens");
	if (!lens_device->of_node) {
	    LOG_INF("Failed to find camera_af_lens node, using defaults\n");
	    sub2af_bit = 10;
	} else {
	    ret=of_property_read_u32(lens_device->of_node,"sub2af_bit", &sub2af_bit);
	    if(ret==0){
	        LOG_INF("sub2af_bit is found=%d!\n",sub2af_bit);
	    }else{
	        sub2af_bit=10;
	        LOG_INF("use default af_bit is 10bit\n");
	    }
	}

	return 0;
}

static int read_af_chip_id(struct i2c_client *pstAF_I2Cclient,u8 addr)
{
	int i4RetValue = 0;
	u8 i = 0;
	u8 get_byte = 0;
	char * a_puBuff = &get_byte;
	char puReadCmd[1];
	char puSendCmd[2];
	puReadCmd[0] = (char)(addr);
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	while (i < ARRAY_SIZE(af_i2c_slave_addr) && af_i2c_slave_addr[i] != 0xFF) {
		g_pstAF_I2Cclient->addr = af_i2c_slave_addr[i];
		g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

		i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puReadCmd, 1);
		if(i4RetValue>=0){
			sub2af_i2cslave_idx=i;
			LOG_INF("I2C read i2c_master_send pass!! i4RetValue=%d afchip_id_addr_table=0x%x sub2af_i2cslave_idx=0x%x i=%d",i4RetValue,addr,(g_pstAF_I2Cclient->addr*2),i);
			break;
		}else{
			LOG_INF(" I2C read  i2c_master_send failed!! i4RetValue=%d afchip_id_addr_table=0x%x sub2af_i2cslave_idx=0x%x i=%d",i4RetValue,addr,(g_pstAF_I2Cclient->addr*2),i);
		}
		i++;
	}
	if(i4RetValue<0){
		LOG_INF(" I2C read  i2c_master_send failed! no find correct i2c addr");
		return -1;
	}

	g_pstAF_I2Cclient->addr = af_i2c_slave_addr[sub2af_i2cslave_idx];
	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	puSendCmd[0] = (char)0xED;
	puSendCmd[1] = (char)0xAB;
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puReadCmd, 1);
	LOG_INF("g_pstAF_I2Cclient->addr=0x%x sub2af_i2cslave_idx=%d \n",g_pstAF_I2Cclient->addr,sub2af_i2cslave_idx);

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, a_puBuff, 1);
	if (i4RetValue < 0) {
		puSendCmd[0] = (char)0xDF;
		puSendCmd[1] = (char)0x5B;
		i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
		LOG_INF(" I2C read  i2c_master_recv failed!! i4RetValue=%d g_pstAF_I2Cclient->addr=0x%x i=%d \n",i4RetValue,g_pstAF_I2Cclient->addr,i);
		return -1;
	}

	puSendCmd[0] = (char)0xDF;
	puSendCmd[1] = (char)0x5B;
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	LOG_INF(" I2C read  (*a_puBuff)=0x%x get_byte=0x%x\n",(*a_puBuff),get_byte);

	return (int)(*a_puBuff);
 }

int tran_get_sub2af_chip_id(struct i2c_client *pstAF_I2Cclient,struct device *lens_device)
{
	int i = 0,j=0;
	int chip_id = 0;
	int chip_id_sec = 0;
	tran_get_sub2af_parse_dts(lens_device);
	msleep(5);
	while(i < ARRAY_SIZE(afchip_id_addr_table) && afchip_id_addr_table[i] != 0xFF) {
		 chip_id = read_af_chip_id(pstAF_I2Cclient,afchip_id_addr_table[i]);
		 LOG_INF("chip_id=0x%x afchip_id_addr_table=0x%x\n",chip_id,afchip_id_addr_table[i]);
		 for(j = 0; j < ARRAY_SIZE(tran_vcm_configs); j++){
			if(chip_id == tran_vcm_configs[j].af_id){
				if(tran_vcm_configs[j].af_id_sec[0]== 0xFF){
					sub2af_chipid_idx = j;
					LOG_INF("match success,read chip_id =0x%x is match with af_id=0x%x i==%d sub2af_chipid_idx=%d j=%d\n",chip_id,tran_vcm_configs[j].af_id,i,sub2af_chipid_idx,j);
					return sub2af_chipid_idx;
				}else{
					chip_id_sec = read_af_chip_id(pstAF_I2Cclient,tran_vcm_configs[j].af_id_sec[0]);
					LOG_INF("the first chip_id is same it's chip_id=0x%x , need use second chip_id_sec addr=0x%x chip_id_sec=0x%x j=%d\n",chip_id,tran_vcm_configs[j].af_id_sec[0],chip_id_sec,j);
					if(chip_id_sec == tran_vcm_configs[j].af_id_sec[1]){
						sub2af_chipid_idx = j;
						LOG_INF("match success, read chip_id_sec=0x%x is match with af_id_sec=0x%x i==%d sub2af_chipid_idx=%d j=%d\n",chip_id_sec,tran_vcm_configs[j].af_id_sec[1],i,sub2af_chipid_idx,j);
						return sub2af_chipid_idx;
					}else{
						LOG_INF("match failed,the first chip_id is same, but read chip_id_sec=0x%x no match with af_id_sec=0x%x  sub2af_chipid_idx=%d j==%d\n",chip_id_sec,tran_vcm_configs[j].af_id_sec[1],sub2af_chipid_idx,j);
					}
				}
			}else{
				LOG_INF("match failed,read chip_id =0x%x but no match with tran_vcm_configs[j].af_id=0x%x sub2af_chipid_idx=%d j==%d\n",chip_id,tran_vcm_configs[j].af_id,sub2af_chipid_idx,j);
			}
		 }
		 i++;
	}
	LOG_INF("tran_get_af_chip_id read chip id failed chip_id=0x%x\n",chip_id);

	return -ENODEV;
}

static int initdrv(void)
{
	if(sub2af_bit==11){
		register_setting(tran_vcm_configs[sub2af_chipid_idx].init_table_11bit, 16);
		LOG_INF("initdrv  main_af->af_11bit=%d\n",sub2af_bit);
	}else{
		register_setting(tran_vcm_configs[sub2af_chipid_idx].init_table, 16);
		LOG_INF("initdrv user main_af->af_10bit\n");
	}
	LOG_INF("initdrv sub2af_chipid_idx=%d af_id=0x%x af_id_sec=0x%x\n",sub2af_chipid_idx,tran_vcm_configs[sub2af_chipid_idx].af_id,tran_vcm_configs[sub2af_chipid_idx].af_id_sec[1]);
	return 0;
}

static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;
	LOG_INF("moveAF  ++ \n");
	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		LOG_INF("moveAF  initdrv++ \n");
		initdrv();
		LOG_INF("moveAF  initdrv-- \n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	if (g_u4CurrPosition == a_u4Position)
		return 0;

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	LOG_INF("moveAF  g_u4TargetPosition=%lu \n",g_u4TargetPosition);
	if (s4AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
	} else {
		LOG_INF("set I2C failed when moving the motor\n");
		ret = -1;
	}
	LOG_INF("moveAF  -- \n");
	return ret;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	LOG_INF("setAFInf  ++ \n");
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	LOG_INF("setAFInf  -- \n");
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	LOG_INF("setAFMacro  ++ \n");
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	LOG_INF("setAFMacro  -- \n");
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long TRANSUB2AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		unsigned long a_u4Param)
{
	long i4RetValue = 0;
	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		LOG_DBG("TRANSUB2AF_Ioctl AFIOC_G_MOTORINFO ++ \n");
		i4RetValue = getAFInfo((__user struct stAF_MotorInfo *)
				(a_u4Param));
		break;

	case AFIOC_T_MOVETO:
		LOG_DBG("TRANSUB2AF_Ioctl AFIOC_T_MOVETO ++ \n");
		i4RetValue = moveAF(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
		LOG_DBG("TRANSUB2AF_Ioctl AFIOC_T_SETINFPOS ++ \n");
		i4RetValue = setAFInf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
		LOG_DBG("TRANSUB2AF_Ioctl AFIOC_T_SETMACROPOS ++ \n");
		i4RetValue = setAFMacro(a_u4Param);
		break;

	default:
		LOG_DBG("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}
	return i4RetValue;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int TRANSUB2AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	int val,ret;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	LOG_INF("TRANSUB2AF_Release ++\n");

	diff_dac = tran_vcm_configs[sub2af_chipid_idx].origin_focus_pos - g_u4TargetPosition;

	if(tran_vcm_configs[sub2af_chipid_idx].move_steps <= 0){
		tran_vcm_configs[sub2af_chipid_idx].move_steps =100;
	}

	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		tran_vcm_configs[sub2af_chipid_idx].move_steps;

	LOG_INF("%s sub2_vcm->focus->val= %lu tran_vcm_configs[sub2af_chipid_idx].move_steps=%d origin_focus_pos=%d diff_dac=%d nStep_count=%d\n",  __func__, g_u4TargetPosition,tran_vcm_configs[sub2af_chipid_idx].move_steps, tran_vcm_configs[sub2af_chipid_idx].origin_focus_pos,diff_dac,nStep_count);

	val = g_u4TargetPosition;
	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (tran_vcm_configs[sub2af_chipid_idx].move_steps*(-1)) :
			tran_vcm_configs[sub2af_chipid_idx].move_steps);

		ret = moveAF(val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d\n",
				__func__, ret);
			return ret;
		}

		msleep(tran_vcm_configs[sub2af_chipid_idx].move_delay_ms);
		LOG_INF("TRANSUB2AF_Release moveAF i=%d val=%d\n",i,val);
	}

	moveAF(tran_vcm_configs[sub2af_chipid_idx].origin_focus_pos);
	if (*g_pAF_Opened == 2){
		LOG_INF("Wait\n");
		register_setting(tran_vcm_configs[sub2af_chipid_idx].release_table, 8);
	}
	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("TRANSUB2AF_Release --\n");

	return 0;
}

int TRANSUB2AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
	spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	LOG_INF(" TRANSUB2AF_SetI2Cclient ++\n");
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;
	LOG_INF(" TRANSUB2AF_SetI2Cclient --\n");
	return 1;
}

int TRANSUB2AF_GetFileName(unsigned char *pFileName)
{
	#if SUPPORT_GETTING_LENS_FOLDER_NAME
	char FilePath[256];
	char *FileString;
	LOG_INF(" TRANSUB2AF_GetFileName ++\n");
	sprintf(FilePath, "%s", __FILE__);
	FileString = strrchr(FilePath, '/');
	*FileString = '\0';
	FileString = (strrchr(FilePath, '/') + 1);
	strncpy(pFileName, FileString, AF_MOTOR_NAME);
	LOG_INF("FileName : %s\n", pFileName);
	#else
	pFileName[0] = '\0';
	#endif
	LOG_INF(" TRANSUB2AF_GetFileName --\n");
	return 1;
}
