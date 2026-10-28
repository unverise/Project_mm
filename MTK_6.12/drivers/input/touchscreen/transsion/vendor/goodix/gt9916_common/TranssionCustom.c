/*
* This program is created for the custom of touchpanel function and
porting .
*
* File Name: TranssionCustom.c
*
* Created: 2016-06-20
*
*/

#include "TranssionCustom.h"
//#include "tpd.h"
#include "goodix_ts_core.h"
/* BSP: add ACBHBHW-2 for tp_vendor/bv show test by yun.luo 20220426 start */
static char final_ctp_firmware_version[16] = "0x1f";
static int tran_tp_remove(struct platform_device *pdev);
static int tran_tp_probe(struct platform_device *pdev);
/* BSP: add ACBHBHW-2 for tp_vendor/bv show test by yun.luo 20220426 end */
static int gesture_enabled = 0;
int gesturechar_enabled =0;
int gesturedouble_enabled=0;
int gesturemusic_enabled=0;
static char g_gtp_gesture = 0;
static u8 gestures_flag[32];
static st_gesture_data gesture_data;
static struct mutex gesture_data_mutex;
static int gesture_status;
extern struct goodix_module goodix_modules;
/* BSP: add ACBHBHW-2 for tp_vendor/bv show test by yun.luo 20220426 start */
static char *g_get_lcm_name = NULL;
char *tran_find_lcm_driver(void)
{
        struct device_node *chosen_node;
        char *lcm_name=NULL;
        if (g_get_lcm_name != NULL)
            return g_get_lcm_name;
        chosen_node = of_find_node_by_path("/chosen");
        if (chosen_node) {
                struct tran_videolfb *videolfb_tag = NULL;
                unsigned long size = 0;

                videolfb_tag = (struct tran_videolfb *)of_get_property(
                        chosen_node, "atag,videolfb", (int *)&size);
                if (videolfb_tag) {
                    lcm_name = videolfb_tag->lcmname;
                    if(lcm_name){
	                ts_info("lcm_name=%s is find.",videolfb_tag->lcmname);
                        g_get_lcm_name = videolfb_tag->lcmname;
                        return lcm_name;
		    }
                }
                ts_err("videolfb_tag not found");
        } else {
	    ts_err("of_chosen not found.");
        }
        ts_err("lcm name is not found.just return unknow!!!");
        lcm_name = "unknow";
        return lcm_name;
}
/* BSP: add ACBHBHW-2 for tp_vendor/bv show test by yun.luo 20220426 end */
/* BSP: add ACBHBHW-2 for get boot_mode by yun.luo 20220517 start */
int tran_tp_get_boot_mode(void)
{
        struct device_node *boot_node = NULL;
        struct tag_bootmode *tags = NULL;

        boot_node = of_find_node_by_path("/chosen");
        if(!boot_node){
                ts_err("tran_get_boot_mode error");
                return -1;
        }else{
                tags = (struct tag_bootmode *)of_get_property(boot_node,"atag,boot", NULL);
                if(tags){
                        ts_info("bootmode is 0x%x",tags->bootmode);
                        return tags->bootmode;
                }else{
                        ts_err("tran_get_boot_mode bootmode error");
                        return -1;
                }
        }
}
/* BSP: add ACBHBHW-2 for get boot_mode by yun.luo 20220517 end */
//gesture mode
static ssize_t gesture_status_read(struct file *file,char *buffer, size_t count, loff_t *ppos)
{
    int len,ret = -1;
    char *page = NULL;
    char *ptr = NULL;
    page = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!page)
    {
        kfree(page);
        return -ENOMEM;
    }
    ptr = page;
    ptr += sprintf(ptr,"%c:%d;\n%c:%d;\n%c:%d;\n%c:%d;\n%c:%d;\n%c:%d;\n%c:%d;\n%c:%d;\n%c:%d;\n%c:%d;\n%s:%d;\n%s:%d;\n",'w',QUERYBIT(gestures_flag,'w' ),'o',QUERYBIT(gestures_flag,'o'),'m',QUERYBIT(gestures_flag,'m'),'e',QUERYBIT(gestures_flag,'e'),'c',QUERYBIT(gestures_flag,'c'),'z',QUERYBIT(gestures_flag,'z'),'s',QUERYBIT(gestures_flag,'s'),'v',QUERYBIT(gestures_flag,'v'),'^',QUERYBIT(gestures_flag,0X5e),'>',QUERYBIT(gestures_flag,0x3e),"all",gesturechar_enabled,"cc",QUERYBIT(gestures_flag,0xcc));

    len = ptr - page;
    if(*ppos >= len)
    {
        kfree(page);
        return 0;
    }
    ret = copy_to_user(buffer,(char *)page,len);
    *ppos += len;
    if(ret)
    {
        kfree(page);
        return ret;
    }
    kfree(page);
    return len;
}

static ssize_t gesture_status_write(struct file *filp, const char __user * buf, size_t len, loff_t * off)
{
    return 0;
}

static const struct proc_ops gesture_fops1 = {
     // .owner = THIS_MODULE,
    .proc_read = gesture_status_read,
    .proc_write = gesture_status_write,
};


static ssize_t gesture_data_read(struct file *file, char * buffer, size_t size, loff_t * ppos)
{
    int len,ret = -1;
    char *page = NULL;
    char *ptr = NULL;
    page = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!page)
    {
        kfree(page);
        return -ENOMEM;
    }
    ptr = page;
    if(gesture_data.data[0]==0xcc)
        ptr += sprintf(ptr,"%c\n",'f');
    else
        ptr += sprintf(ptr,"%c\n",gesture_data.data[0]);
    len = ptr - page;
    if(*ppos >= len)
    {
        kfree(page);
        return 0;
    }
    ret = copy_to_user(buffer,(char *)page,len);
    *ppos += len;
    ts_info("CTP gesture ret:%d ppos:%d ges:0x%x char:%c",ret,(int)*ppos,gesture_data.data[0],*ptr);
    if(ret)
    {
        kfree(page);
        return ret;
    }
    kfree(page);
    return len;

}

//BSP: add aod gesture switch NFRFP-3865 by zhengbo.tang 20201113 start
bool fp_enable=false;
//BSP: add aod gesture switch NFRFP-3865 by zhengbo.tang 20201113 end

static ssize_t gesture_data_write(struct file *filp, const char __user * buf, size_t len, loff_t * off)
{
    int tempchar3=0;
    char tempchar2,tempchar1;
    u8 tempstatus=-1;
    char buff[4]={'0','0','0','\0'};
    int ret = -1;
    ret = copy_from_user(buff, buf, 3);
    //ret=scanf(buf, "%u", buff);
    if (ret) {
        ts_err("copy_from_user failed.");
        return -EPERM;
    }

    //BSP: add aod gesture switch NFRFP-3865 by zhengbo.tang 20201113 start
    if(strncmp(buff,"fp",2)==0){
	if(buff[2]=='1'){
		ts_info("app write fp setting enable");
		fp_enable=true;
		/*BSP: add for tp finger by linsheng.liu 20220210 start*/
		goodix_gesture_enable(1);
		/*BSP: add for tp finger by linsheng.liu 20220210 end*/
	}else{
		fp_enable=false;
		/*BSP: add for tp finger by linsheng.liu 20220210 start*/
		if(gesturemusic_enabled ==0 && gesturedouble_enabled == 0 && gesturechar_enabled == 0)
			goodix_gesture_enable(0);
		/*BSP: add for tp finger by linsheng.liu 20220210 end*/
		ts_info("app write fp setting disable");
	}
	return len;
    }
    //BSP: add aod gesture switch NFRFP-3865 by zhengbo.tang 20201113 end

    if(buff[1]!='l'&&buff[1]!='0'&&buff[1]!='a'&&buff[1]!='b'&&buff[1]!='c')
    {
        ts_err("CTP-gesture bad command format 0:%c 1:%c 2:%c!!!",buff[0],buff[1],buff[2]);
        return -EPERM;

    }
    if(buff[1]=='l' && (buff[2]=='1'||buff[2]=='2'))
    {
        /*BSP: add for tp finger by linsheng.liu 20220210 start*/
        if(buff[2]=='2'){
                gesturechar_enabled=0;
                if(gesturemusic_enabled ==0 && gesturedouble_enabled == 0 && fp_enable==false)
			goodix_gesture_enable(0);
                cts_setgesturestatus(GESTURE_DISABLE_TOTALLY,0);
        }
        if(buff[2]=='1'){
                gesturechar_enabled=1;
                goodix_gesture_enable(1);
                cts_setgesturestatus(GESTURE_ENABLE_TOTALLY,1);
        }
        /*BSP: add for tp finger by linsheng.liu 20220210 end*/
        return  len;
    }
    if(buff[0]<'A'||buff[0]>'z'||(buff[2]!='1'&&buff[2]!='2')){
        ts_err("CTP-gesture bad command format 0:%c 1:%c 2:%c!!!",buff[0],buff[1],buff[2]);
        return -EPERM;
    }

    tempchar1=buff[0];
    tempchar2=buff[1];
    tempstatus=buff[2];

    ts_info("CTP-gesture buff 0:%c 1:%c 2:%c!!!",buff[0],buff[1],buff[2]);

    switch(tempchar2)
    {
        case '0':
            switch(tempstatus){
                case '2':
                    cts_setgesturestatus(GESTURE_DISABLE_PARTLY,tempchar1);
                    break;
                case '1':
                    cts_setgesturestatus(GESTURE_ENABLE_PARTLY,tempchar1);
                    break;
                default:return -EPERM;
            }
            break;
        default:
            buff[2]='\0';

            tempchar3= simple_strtoul(buff,NULL,16);

            switch(tempstatus){
                case '2':
                    /*BSP: add for tp finger by linsheng.liu 20220210 start*/
                    if(tempchar3==0xcc)
					{
						gesturedouble_enabled=0;
						if(gesturemusic_enabled ==0 && fp_enable==false && gesturechar_enabled == 0)
							goodix_gesture_enable(0);
					}
                    if((tempchar3==0xbb)||(tempchar3==0xba)||(tempchar3==0xab)||(tempchar3==0xaa))
					{
						gesturemusic_enabled=0;
						if(gesturedouble_enabled ==0 && fp_enable==false && gesturechar_enabled == 0)
							goodix_gesture_enable(0);
					}
                    /*BSP: add for tp finger by linsheng.liu 20220210 end*/
                    cts_setgesturestatus(GESTURE_DISABLE_PARTLY,tempchar3);
                    break;
                case '1':
                    /*BSP: add for tp finger by linsheng.liu 20220210 start*/
                    if(tempchar3==0xcc)
					{
						gesturedouble_enabled=1;
						goodix_gesture_enable(1);
					}
                    if((tempchar3==0xbb)||(tempchar3==0xba)||(tempchar3==0xab)||(tempchar3==0xaa))
					{
						gesturemusic_enabled=1;
						goodix_gesture_enable(1);
					}
                    /*BSP: add for tp finger by linsheng.liu 20220210 end*/
                    cts_setgesturestatus(GESTURE_ENABLE_PARTLY,tempchar3);
                    break;
                default:return -EPERM;
                }
            break;
    }

    return len;
}

static const struct proc_ops gesture_fops2 = {
    // .owner = THIS_MODULE,
    .proc_read = gesture_data_read,
    .proc_write = gesture_data_write,
};

u8 cts_is_all_dead(u8 * longlong, s32 size)
{
	int i = 0;
	u8 sum = 0;

	for (i = 0; i < size; i++) {
		sum |= longlong[i];
	}

	return !sum;
}

void cts_gesture_clear_data(void)
{
    mutex_lock(&gesture_data_mutex);
    memset(gesture_data.data, 0, 4);
    mutex_unlock(&gesture_data_mutex);
}

s32 cts_setgesturestatus(unsigned int cmd,unsigned long arg)
{
    u8 ret = 0;
    u32 value = (u32) arg;
    switch (cmd & NEGLECT_SIZE_MASK) {
        case GESTURE_ENABLE_TOTALLY:
            ts_info("ENABLE_GESTURE_TOTALLY");
            gesturechar_enabled=1;
            gesture_enabled=((gesturechar_enabled)||(gesturedouble_enabled)||(gesturemusic_enabled));
            break;

        case GESTURE_DISABLE_TOTALLY:
            ts_info("DISABLE_GESTURE_TOTALLY");
            gesturechar_enabled=0;
            gesture_enabled=((gesturechar_enabled)||(gesturedouble_enabled)||(gesturemusic_enabled));
            break;

        case GESTURE_ENABLE_PARTLY:
            SETBIT(gestures_flag, (u8) value);
            gesture_enabled = (gesturechar_enabled ||gesturedouble_enabled||(gesturemusic_enabled));
            ts_info("ENABLE_GESTURE_PARTLY, gesture = 0x%02X, gesture_enabled = %d", value, gesture_enabled);
            break;

        case GESTURE_DISABLE_PARTLY:
            ret = QUERYBIT(gestures_flag, (u8) value);
            if (!ret) {
                break;
            }
            CLEARBIT(gestures_flag, (u8) value);

            if (cts_is_all_dead(gestures_flag, sizeof(gestures_flag))) {
                //gesture_enabled = 0;
            }
            ts_info("DISABLE_GESTURE_PARTLY, gesture = 0x%02X, gesture_enabled = %d", value, gesture_enabled);
            break;
        case GESTURE_DATA_OBTAIN:
            ts_info("OBTAIN_GESTURE_DATA");

            mutex_lock(&gesture_data_mutex);
            if (gesture_data.data[1] > GESTURE_MAX_POINT_COUNT) {
                gesture_data.data[1] = GESTURE_MAX_POINT_COUNT;
            }
            if (gesture_data.data[3] > 80) {
                gesture_data.data[3] = 80;
            }
            ret = copy_to_user(((u8 __user *) arg), &gesture_data.data, 4 + gesture_data.data[1] * 4 + gesture_data.data[3]);
            mutex_unlock(&gesture_data_mutex);
            if (ret) {
                ts_err("ERROR when copy gesture data to user.");
                ret = -1;
            } else {
                ret = 4 + gesture_data.data[1] * 4 + gesture_data.data[3];
            }
            break;

        case GESTURE_DATA_ERASE:
            ts_info("ERASE_GESTURE_DATA");
            cts_gesture_clear_data();
            break;
    }

    return 0;
}

s32 cts_gesture_init_node(void)
{
    struct proc_dir_entry *proc_entry = NULL;
    struct proc_dir_entry *gesture_state = NULL;
    mutex_init(&gesture_data_mutex);
    memset(gestures_flag, 0, sizeof(gestures_flag));//for debug   0-->1
    memset((u8 *) & gesture_data, 0, sizeof(st_gesture_data));

    proc_entry = proc_create(GESTURE_NODE,0666, NULL, &gesture_fops2);
    if (proc_entry == NULL) {
        ts_err("[GESTURE]Couldn't create proc entry[GESTURE_NODE]!");
        return -1;
    } else {
        ts_info("[GESTURE]Create proc entry[GESTURE_NODE] success!");
    }

    gesture_state = proc_create("gesture_state", 0666, NULL, &gesture_fops1);
    if (gesture_state == NULL) {
        ts_err("[GESTURE]Couldn't create proc entry[gesture_state]!");
        return -1;
    } else {
        ts_info("[GESTURE]Create proc entry[gesture_state] success!");
    }
    return 0;
}

void cts_gesture_deinit_node(void)
{
    remove_proc_entry(GESTURE_NODE, NULL);
}

int Cts_SetDozeStatus(void)
{
	//BSP: add aod gesture switch NFRFP-3865 by zhengbo.tang 20201113 start
	gesture_status = (gesturechar_enabled ||gesturedouble_enabled||(gesturemusic_enabled)||(fp_enable));
	//BSP: add aod gesture switch NFRFP-3865 by zhengbo.tang 20201113 end
	return gesture_status;
}

int Cts_GestureIdTranslate(const int gesture_id)
{
    int ret = 0;
	ts_debug("[GESTURE]gesture_id=0x%x", gesture_id);
	switch(gesture_id)
	{
		case GESTURE_DOUBLECLICK:
			g_gtp_gesture=GESTURE_DC;
			break;
		case GESTURE_C:
			g_gtp_gesture=GESTURE_c;
			break;
		case GESTURE_M:
			g_gtp_gesture=GESTURE_m;
			break;
		case GESTURE_LEFT:
			g_gtp_gesture=GESTURE_LF;
			break;
		case GESTURE_RIGHT:
			g_gtp_gesture=GESTURE_RT;
			break;
		case GESTURE_UP:
			g_gtp_gesture=GESTURE_up;
			break;
		case GESTURE_DOWN:
			g_gtp_gesture=GESTURE_down;
			break;
		case GESTURE_O:
			g_gtp_gesture=GESTURE_o;
			break;
		case GESTURE_W:
			g_gtp_gesture=GESTURE_w;
			break;
		case GESTURE_E:
			g_gtp_gesture=GESTURE_e;
			break;
		case GESTURE_S:
			g_gtp_gesture=GESTURE_s;
			break;
		case GESTURE_V:
			g_gtp_gesture=GESTURE_v;
			break;
		case GESTURE_Z:
			g_gtp_gesture=GESTURE_z;
			break;
		//BSP: move aod gesture code ACBAEA-9 by zhengbo.tang 20210222 start
		case GESTURE_SINGLECLICK:
			g_gtp_gesture=GESTURE_singleclick;
			break;
		case GESTURE_AOD_DOWN:
			g_gtp_gesture=GESTURE_AOD_down;
			break;
		case GESTURE_AOD_UP:
			g_gtp_gesture=GESTURE_AOD_up;
			break;
		//BSP: move aod gesture code ACBAEA-9 by zhengbo.tang 20210222 end
		default:
			g_gtp_gesture=-1;
			break;
	}
	ret = Cts_GestureHandle();
	return ret;
}

//BSP: move aod gesture code ACBAEA-9 by zhengbo.tang 20210222 start
int g_aod_flag = 0;
//BSP: move aod gesture code ACBAEA-9 by zhengbo.tang 20210222 end

int Cts_GestureHandle(void)
{
    struct input_dev *input_dev = goodix_modules.core_data->input_dev;

    //BSP: move aod gesture code ACBAEA-9 by zhengbo.tang 20210222 start
    if(g_gtp_gesture == GESTURE_singleclick){
	ts_info("gesture single click report");
	input_report_key(input_dev, KEY_F20, 1);
	input_sync(input_dev);
	input_report_key(input_dev, KEY_F20, 0);
	input_sync(input_dev);
	return 0;
    }
    if(g_gtp_gesture == GESTURE_AOD_down && g_aod_flag ==0){
	ts_info("gesture aod key finger down");
	input_report_key(input_dev, KEY_FINGER, 1);
	input_sync(input_dev);
	g_aod_flag =1;
	return 0;
    }
    if(g_gtp_gesture == GESTURE_AOD_up && g_aod_flag ==1){

	ts_info("gesture aod key finger up");
	input_report_key(input_dev, KEY_FINGER, 0);
	input_sync(input_dev);
	g_aod_flag =0;
	return 0;
    }
    //BSP: move aod gesture code ACBAEA-9 by zhengbo.tang 20210222 end

    if( Cts_SetDozeStatus())
    {
        if (!QUERYBIT(gestures_flag, g_gtp_gesture)||(!gesturechar_enabled && g_gtp_gesture>='a'&&g_gtp_gesture<='z'))
        {
            ts_err("[GESTURE]Sorry, this gesture has been disabled.");
            return -1;
        }

        if (g_gtp_gesture != -1)
        {
            gesture_data.data[0] = g_gtp_gesture;	// gesture type
            gesture_data.data[1] = 0;	// gesture points number
            gesture_data.data[2] = 0;
            gesture_data.data[3] = 0;

            ts_info("[GESTURE]gesture report key");
            input_report_key(input_dev, KEY_F13, 1);
            input_sync(input_dev);
            input_report_key(input_dev, KEY_F13, 0);
            input_sync(input_dev);

            return 0;
        }
        else
        {
            ts_err("[GESTURE]Sorry, g_gtp_gesture is invaild.");
            return -1;
        }
    }
    return -1;
}

/* BSP: add ACBHBHW-2 for tp_vendor/bv show test by yun.luo 20220426 start */
extern u8 bin_fw_version;
static int config_id[]={
	0x61d4f2d4,
	0x61eff562,
	0x62b10aa,
	0x62f17a8,
	0x6231adfc,
	0x623ae18d,
	0x6248abf,
	0x6256bb19,
	0x627e24f9,
	0x629daca8,
	0x62cd6262,
	0x62e0fb78,
	0x637524c,
	0x631b1ffb,
};

static int config_version[]={
	0x07,
	0x0a,
	0x0c,
	0x01,
	0x03,
	0x04,
	0x05,
	0x06,
	0x08,
	0x09,
	0x0d,
	0x1e,
	0x1f,
	0x20,
};
static ssize_t tp_firmware_version_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	struct goodix_ts_core *core_data = goodix_modules.core_data;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_fw_version chip_ver;
	int i = 0;

	hw_ops->read_version(core_data, &chip_ver);
	hw_ops->get_ic_info(core_data, &core_data->ic_info);

	ts_info("fw_version = 0x%x, config_id = 0x%x",chip_ver.patch_vid[3],core_data->ic_info.version.config_id);
	ts_info("bin_fw_version = 0x%x, bin_config_id = 0x%x", bin_fw_version, bin_config_id);

	for(i=0; i<sizeof(config_id)/sizeof(int); i++){
                if(core_data->ic_info.version.config_id == config_id[i]){
			if(chip_ver.patch_vid[3] == bin_fw_version && config_id[i] == bin_config_id){
				return snprintf(buf,32,"0x%x_0x%x,bv:0x%x_0x%x\n", chip_ver.patch_vid[3], config_version[i],
						chip_ver.patch_vid[3], config_version[i]);
			}
                }
        }
        return sprintf(buf,"%s","unknow");
}

static ssize_t tp_firmware_version_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	return count;
}
static DEVICE_ATTR(tp_firmware_version, 0644, tp_firmware_version_show, tp_firmware_version_store);

static char *lcm_names[]={
        "nt37701_fhdp_dsi_cmd_boe_120hz_ad8",
        "nt37701a_fhdp_dsi_cmd_tm_boe_120hz_ad9",
};

static char *tp_names[]={
        "BOE_GT9916_00",
        "BOE_GT9916_00",
};

static ssize_t tp_vendor_show(struct device *dev,struct device_attribute *attr,char *buf)
{
        int i=0;
        char *lcm_name = "nt37701a_fhdp_dsi_cmd_tm_boe_120hz_ad9";
        lcm_name= tran_find_lcm_driver();
        ts_info("%s",lcm_name);
        for(i=0;i<sizeof(lcm_names)/sizeof(lcm_names[0]);i++){
                if(strcmp(lcm_name,lcm_names[i])==0){
                        return sprintf(buf,"%s",tp_names[i]);
                }
        }
        return sprintf(buf,"%s","unknow");
}

static ssize_t tp_vendor_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)
{
	return count;
}
static DEVICE_ATTR(tp_vendor, 0644, tp_vendor_show, tp_vendor_store);

static ssize_t tp_final_fw_version_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	return sprintf(buf,"%s\n", final_ctp_firmware_version);
}

static ssize_t tp_final_fw_version_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)
{
	strncpy(final_ctp_firmware_version, buf, sizeof(final_ctp_firmware_version)-1);
	return count;
}

static DEVICE_ATTR(tp_final_firmware_version, 0664, tp_final_fw_version_show, tp_final_fw_version_store);


const struct of_device_id touch_of_match[] = {
	{ .compatible = "mediatek,touch", },
	{},
};

const struct dev_pm_ops tpd_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
};


static struct platform_driver tpd_driver = {
	.remove = tran_tp_remove,
	.shutdown = NULL,
	.probe = tran_tp_probe,
	.driver = {
			.name = TPD_DEVICE,
			.pm = &tpd_pm_ops,
			.owner = THIS_MODULE,
			.of_match_table = touch_of_match,
	},
};
static int tran_tp_probe(struct platform_device *pdev)
{
	ts_info("in");
	if (device_create_file(&pdev->dev, &dev_attr_tp_firmware_version))
			ts_err("create attributes dev_attr_tp_firmware_version failed");
	if (device_create_file(&pdev->dev, &dev_attr_tp_vendor))
			ts_err("create attributes dev_attr_tp_vendor failed");
	if (device_create_file(&pdev->dev, &dev_attr_tp_final_firmware_version))
			ts_err("create attributes dev_attr_tp_final_firmware_version failed");
	if(sysfs_create_link(&platform_bus.kobj, &pdev->dev.kobj, TPD_DEVICE))
                    ts_err("sysfs_create_link %s fail.", TPD_DEVICE);
	ts_info("out");
	return 0;
}
static int tran_tp_remove(struct platform_device *pdev)
{
	ts_info("in");
	sysfs_remove_link(&platform_bus.kobj, TPD_DEVICE);
	device_remove_file(&pdev->dev, &dev_attr_tp_final_firmware_version);
	device_remove_file(&pdev->dev, &dev_attr_tp_vendor);
	device_remove_file(&pdev->dev, &dev_attr_tp_firmware_version);
	ts_info("out");
	return 0;
}

void tpd_device_init(void)
{
	ts_info("in");
	if (platform_driver_register(&tpd_driver) != 0)
		ts_info("unable to register touch panel driver.\n");
	ts_info("out");
}

void  tpd_device_exit(void)
{
	ts_info("in");
	platform_driver_unregister(&tpd_driver);
	ts_info("out");
}
/* BSP: add ACBHBHW-2 for tp_vendor/bv show test by yun.luo 20220426 end */
