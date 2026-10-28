#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/input.h>
#include <linux/uaccess.h>

#include "transsion_gesture.h"
#include "transsion_core.h"

#ifndef KEY_F26
#define KEY_F26 198
#endif

//gesture mode
typedef enum
{
    DOZE_DISABLED = 0,
    DOZE_ENABLED = 1,
    DOZE_WAKEUP = 2,
} DOZE_T;

#define GESTURE_MAX_POINT_COUNT    64

#pragma pack(1)
typedef struct {
    u8 ic_msg[6];         /*from the first byte */
    u8 gestures[4];
    u8 data[3 + GESTURE_MAX_POINT_COUNT * 4 + 80];  /*80 bytes for extra data */
} st_gesture_data;
#pragma pack()

struct tran_gestre_map_s gesture_maps[]=
{
    {"al", 0, 0xFF, TRAN_GESTURE_CHAR_ALL},
    {"cc", 0, GESTURE_DC, TRAN_GESTURE_DOUBLE},
    {"aa", 0, GESTURE_RT, TRAN_GESTURE_MUSIC},
    {"bb", 0, GESTURE_LF, TRAN_GESTURE_MUSIC},
    {"ab", 0, GESTURE_down, TRAN_GESTURE_MUSIC},
    {"ba", 0, GESTURE_up, TRAN_GESTURE_MUSIC},
    {"5e", 0, 0x5E, TRAN_GESTURE_SPECIAL},
    {"3e", 0, 0x3E, TRAN_GESTURE_SPECIAL},
    {"fp", 0, GESTURE_OC, TRAN_GESTURE_SPECIAL},
    {"w0", 0, GESTURE_w, TRAN_GESTURE_CHAR},
    {"o0", 0, GESTURE_o, TRAN_GESTURE_CHAR},
    {"m0", 0, GESTURE_m, TRAN_GESTURE_CHAR},
    {"e0", 0, GESTURE_e, TRAN_GESTURE_CHAR},
    {"c0", 0, GESTURE_c, TRAN_GESTURE_CHAR},
    {"z0", 0, GESTURE_z, TRAN_GESTURE_CHAR},
    {"s0", 0, GESTURE_s, TRAN_GESTURE_CHAR},
    {"v0", 0, GESTURE_v, TRAN_GESTURE_CHAR},
};

static int GestureHandle(struct tran_ts_controller *ctr);

static int gesturechar_enabled =0;
static int gesturedouble_enabled=0;
static int gesturemusic_enabled=0;
static int gestureclick_enabled=0;

static u8 g_gtp_gesture = 0;
static u8 gestures_flag[32];
static st_gesture_data gesture_data;
static struct mutex gesture_data_mutex;
static int gesture_status;

extern char *mtkfb_find_lcm_driver(void);

static struct tran_gestre_map_s *cmd_to_map(char *cmd)
{
    int i ;
    struct tran_gestre_map_s *pmap = NULL ;
    for(i = 0 ; i < ARRAY_SIZE(gesture_maps) ; i++)
    {
        if(strncmp(cmd, gesture_maps[i].cmd, 2) == 0)
            pmap = &gesture_maps[i];
    }
    return pmap;
}

static bool code_is_enable(u8 code)
{
    int i ;
    for(i = 0 ; i < ARRAY_SIZE(gesture_maps) ; i++)
    {
        if(code  == gesture_maps[i].code) {
            if(gesture_maps[i].gesture_type != TRAN_GESTURE_CHAR || gesturechar_enabled == 1) {
                return gesture_maps[i].enable;
            } else {
                return false;
            }
        }
    }

    return false;
}


static unsigned char cmd_get_enable(char *cmd)
{
    unsigned char ret ;

    if(cmd[2]== '1') {
        ret = 1 ;
    }
    else if(cmd[2] == '2') {
        ret = 0 ;
    }
    else
        ret = 2 ;
    return ret;
}

int get_gesture_status(char *buffer)
{
    int i= 0;
    int len = 0;
    char *ptr = buffer;

    for(i = 0 ; i < ARRAY_SIZE(gesture_maps) ; i++) {
        ptr += sprintf(ptr,"%c%c : %d\n",gesture_maps[i].cmd[0],gesture_maps[i].cmd[1],gesture_maps[i].enable);
    }

    len = ptr - buffer;
    return len + 1;//返回实际buf长度,包含null字符
}

int set_gesture_status(char *buffer, int len)
{
    struct tran_gestre_map_s *pmap = NULL;
    unsigned char cmd_enable_status = 0;

    cmd_enable_status = cmd_get_enable(buffer);
    pmap = cmd_to_map(buffer);
    if(pmap == NULL || cmd_enable_status == 2 || pmap->code == 0) {
        TRAN_ERROR(" bad command format 0:%c 1:%c 2:%c!!!",buffer[0],buffer[1],buffer[2]);
        return -EPERM;
    }

    TRAN_INFO("gesture_data_write code1[%x] enable[%d]", pmap->code, cmd_enable_status);
    switch(pmap->gesture_type) {
    case TRAN_GESTURE_CHAR_ALL:
        gesturechar_enabled = cmd_enable_status;
        break;
    case TRAN_GESTURE_MUSIC:
        gesturemusic_enabled = cmd_enable_status;
        break;
    case TRAN_GESTURE_DOUBLE:
        gesturedouble_enabled = cmd_enable_status;
        break;
    case TRAN_GESTURE_SPECIAL:
        gestureclick_enabled = cmd_enable_status;
        break;
    case TRAN_GESTURE_CHAR:
        break ;
    default:
        break;
    }

    pmap->enable = cmd_enable_status;

    return 0;
}

int get_gesture_code(char *buffer)
{
    int len = 0;

    if(gesture_data.data[0]==0xcc) {
        len += sprintf(buffer,"%c\n",'f');
    } else {
        len += sprintf(buffer,"%c\n",gesture_data.data[0]);
    }

    return  len + 1; //返回实际buf长度,包含null字符
}


//gesture mode
static ssize_t gesture_status_read(struct file *file,char *buffer, size_t count, loff_t *ppos)
{
    int len,ret = -1;
    char *page = NULL;
    char *ptr = NULL;
    int i= 0;
    page = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (IS_ERR_OR_NULL(page)) {
        return -ENOMEM;
    }
    ptr = page;
    for(i = 0 ; i < ARRAY_SIZE(gesture_maps) ; i++) {
        ptr += sprintf(ptr,"%c%c : %d\n",gesture_maps[i].cmd[0],gesture_maps[i].cmd[1],gesture_maps[i].enable);
    }
    len = ptr - page;
    if(*ppos >= len) {
        kfree(page);
        return 0;
    }
    ret = copy_to_user(buffer,(char *)page,len);
    *ppos += len;
    if(ret) {
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

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations gesture_state_ops = {
    .owner = THIS_MODULE,
    .read = gesture_status_read,
    .write = gesture_status_write,
};
#else
static const struct proc_ops gesture_state_ops = {
    .proc_read = gesture_status_read,
    .proc_write = gesture_status_write,
};
#endif


static ssize_t gesture_data_read(struct file *file, char * buffer, size_t size, loff_t * ppos)
{
    int len,ret = -1;
    char *page = NULL;
    char *ptr = NULL;
    page = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (IS_ERR_OR_NULL(page)) {
        kfree(page);
        return -ENOMEM;
    }
    ptr = page;
    if(gesture_data.data[0]==0xcc)
        ptr += sprintf(ptr,"%c\n",'f');
    else
        ptr += sprintf(ptr,"%c\n",gesture_data.data[0]);
    len = ptr - page;
    if(*ppos >= len) {
        kfree(page);
        return 0;
    }
    ret = copy_to_user(buffer,(char *)page,len);
    *ppos += len;
    TRAN_INFO("CTP gesture ret:%d ppos:%d ges:%x char:%c",ret,(int)*ppos,gesture_data.data[0],*page);
    if(ret) {
        kfree(page);
        return ret;
    }
    kfree(page);
    return len;

}

static ssize_t gesture_data_write(struct file *filp, const char __user * buf, size_t len, loff_t * off)
{
    int ret ;
    char buff[4]= {'0','0','0','\0'};
    struct tran_gestre_map_s *pmap = NULL ;
    unsigned char cmd_enable_status = 0;

    ret = copy_from_user(buff, buf, 3);
    if (ret) {
        TRAN_ERROR(" copy_from_user failed.");
        return -EPERM;
    }
    cmd_enable_status = cmd_get_enable(buff);
    pmap = cmd_to_map(buff);

    if(pmap == NULL || cmd_enable_status == 2 || pmap->code == 0)
    {
        TRAN_ERROR(" bad command format 0:%c 1:%c 2:%c!!!",buff[0],buff[1],buff[2]);
        return -EPERM;
    }

    if(pmap->enable == cmd_enable_status)
        return len;

    TRAN_INFO("gesture_data_write code1[%x] enable[%d]", pmap->code, cmd_enable_status);
    switch(pmap->gesture_type)
    {
    case TRAN_GESTURE_CHAR_ALL:
        gesturechar_enabled = cmd_enable_status;
        break;
    case TRAN_GESTURE_MUSIC:
        gesturemusic_enabled = cmd_enable_status;
        break;
    case TRAN_GESTURE_DOUBLE:
        gesturedouble_enabled = cmd_enable_status;
        break;
    case TRAN_GESTURE_SPECIAL:
        gestureclick_enabled = cmd_enable_status;
        break;
    case TRAN_GESTURE_CHAR:
        break ;
    default:
        break;
    }

    pmap->enable = cmd_enable_status;

    return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations gesture_node_ops = {
    .owner = THIS_MODULE,
    .read = gesture_data_read,
    .write = gesture_data_write,
};
#else
static const struct proc_ops gesture_node_ops = {
    .proc_read = gesture_data_read,
    .proc_write = gesture_data_write,
};
#endif

u8 is_all_dead(u8 * longlong, s32 size)
{
    int i = 0;
    u8 sum = 0;

    for (i = 0; i < size; i++) {
        sum |= longlong[i];
    }

    return !sum;
}

void gesture_clear_data(void)
{
    mutex_lock(&gesture_data_mutex);
    memset(gesture_data.data, 0, 4);
    mutex_unlock(&gesture_data_mutex);
}

void gesture_deinit_node(void)
{
    remove_proc_entry("gesture_state", NULL);
    remove_proc_entry("gesture_function", NULL);
}

static int SetDozeStatus(struct tran_ts_controller *ctr)
{
    /*BSP:add when display off switch tp gesture or sleep mode node by jiangzhu.yang 20220830 start*/
    if(ctr->gesture_flag == false)
        return 0;
    /*BSP:add when display off switch tp gesture or sleep mode node by jiangzhu.yang 20220830 end*/

    gesture_status = (gesturechar_enabled ||gesturedouble_enabled||(gesturemusic_enabled)|| gestureclick_enabled);
    return gesture_status;
}

//BSP: add goodix for TP bringup by hai.tang5 20260302 start
static int gesture_switch_status(int value)
{
    int ret = 0;

    switch(value)
    {
    case TRAN_GESTURE_DOUBLE:
        ret = gesturedouble_enabled;
        break;
    case TRAN_GESTURE_MUSIC:
        ret = gesturemusic_enabled;
        break;
    case TRAN_GESTURE_CHAR_ALL:
        ret = gesturechar_enabled;
        break;
    case TRAN_GESTURE_SPECIAL:
        ret = gestureclick_enabled;
        break;
    default:
        break;
    }

    return ret;

}
//BSP: add goodix for TP bringup by hai.tang5 20260302 end

static int GestureIdTranslate(const int gesture_id, struct tran_ts_controller *ctr)
{
    int i;
    int ret = 0;

    TRAN_INFO("[GESTURE]gesture_id=0x%x", gesture_id);
    for(i=0; ctr->gesture_map[i].tran_code > 0; i++) {

        if(gesture_id == ctr->gesture_map[i].vendor_code) {
            g_gtp_gesture = ctr->gesture_map[i].tran_code;
            TRAN_INFO("[GESTURE]g_gtp_gesture=0x%x", g_gtp_gesture);
            ret = GestureHandle(ctr);
            return ret;
        }
    }
    g_gtp_gesture = -1;
    return -1;
}

static int GestureHandle(struct tran_ts_controller *ctr)
{
    if(ctr == NULL || ctr->input_dev == NULL) {
        TRAN_ERROR("not find input_dev!");
        return -1;
    }

    if (g_gtp_gesture == GESTURE_OC && gestureclick_enabled) {
        TRAN_INFO("Single finger down");
        input_report_key(ctr->input_dev, KEY_F20, 1);
        input_sync(ctr->input_dev);
        input_report_key(ctr->input_dev, KEY_F20, 0);
        input_sync(ctr->input_dev);
        return 2;
    }

    if(g_gtp_gesture == GESTURE_FOD_down && g_tran_ts.fp_status.g_fod_flag == 0) {
        TRAN_INFO("gesture FOD key finger down");
        input_report_key(ctr->input_dev, KEY_FINGER, 1);
        input_sync(ctr->input_dev);
        g_tran_ts.fp_status.g_fod_flag = 1;
        return 0;
    }
    if(g_gtp_gesture == GESTURE_FOD_up && g_tran_ts.fp_status.g_fod_flag ==1) {

        TRAN_INFO("gesture FOD key finger up");
        input_report_key(ctr->input_dev, KEY_FINGER, 0);
        input_sync(ctr->input_dev);
        g_tran_ts.fp_status.g_fod_flag = 0;
        return 0;
    }

    if (code_is_enable(g_gtp_gesture))
    {
        gesture_data.data[0] = g_gtp_gesture;    // gesture type
        gesture_data.data[1] = 0;    // gesture points number
        gesture_data.data[2] = 0;
        gesture_data.data[3] = 0;

        input_report_key(ctr->input_dev, KEY_F26, 1);
        input_sync(ctr->input_dev);
        input_report_key(ctr->input_dev, KEY_F26, 0);
        input_sync(ctr->input_dev);
        TRAN_INFO("[GESTURE]gesture report KEY_F26 done !");

        return 0;
    } else {
        TRAN_INFO("[GESTURE]Sorry, this gesture has been disabled.");
        return 1;
    }

    return -1;
}

/*gesture test*/
static ssize_t show_gesture_test(struct device *dev, struct device_attribute *attr, char *buf)
{
    char *ptr = buf;
    ptr += sprintf(ptr, "%d\n", gesture_status);

    return (ptr-buf);
}
static ssize_t store_gesture_test(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{

    if (buf[0] == '0') {
        gesturedouble_enabled = 0;
        TRAN_INFO("[GESTURE]gesture test off");
    } else if (buf[0] == '1') {
        gesturedouble_enabled = 1;
        TRAN_INFO("[GESTURE]gesture test on");
    }

    return count;
}
static DEVICE_ATTR(gesture_test, 0664, show_gesture_test, store_gesture_test);

/* add your attr in here*/
static struct attribute *gesture_attributes[] = {
    &dev_attr_gesture_test.attr,
    NULL
};

static struct attribute_group gesture_attribute_group = {
    .attrs = gesture_attributes
};


//BSP:add when display off switch tp gesture or sleep mode node by jiangzhu.yang 20220830 start
static ssize_t gesture_mode_read(struct file *file,char *buffer, size_t count, loff_t *ppos)
{
    return 0;
}

/*USD:modfiy for gesture recover for main tp by guoxin.zhu 20240522 start*/
static ssize_t gesture_mode_write(struct file *filp, const char __user * buf, size_t len, loff_t * off)
{
    int32_t ret;
    char buff[2]={'0','\0'};
    struct tran_ts_controller *pController = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];

    if(!pController || !pController->controller_init_state){
        CTP_ERROR("gesture_mode_write do nothong !");
        return len;
    }

    ret = copy_from_user(buff, buf, 1);
    if(ret){
        CTP_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    mutex_lock(&pController->dsp_event_mutex);
    if ((buff[0] == '0' || buff[0] == '3')&& !pController->gesture_write_flag) {
        if(pController->controller_state == TRAN_SUSPEND_MODE) {
            CTP_INFO("[main]: recover gesture mode force");
            pController->gesture_flag = true;
            pController->gesture_write_flag = 1;

            /* recover drv power */
            state_change_machine(RESUME_TRIGER, pController);
            pController->ops->tran_ts_resume(NULL, 0);

            /* entry gesture mode */
            state_change_machine(SUSPEND_TRIGER, pController);
            pController->ops->tran_ts_suspend(NULL, 0);

        } else {
            CTP_INFO("[main]: recover gesture mode");
            pController->gesture_flag = true;
            pController->gesture_write_flag = 1;
        }
    } else if (buff[0] == '1' && pController->gesture_write_flag) {
        CTP_INFO("[main]: enter sleep mode");
        pController->gesture_flag = false;
        pController->gesture_write_flag = 0;
        /* need tp suspend */
        if(pController->controller_state != TRAN_SUSPEND_MODE) {
            state_change_machine(SUSPEND_TRIGER, pController);
            pController->ops->tran_ts_suspend(NULL, 1);
        }
    }
    mutex_unlock(&pController->dsp_event_mutex);

    return len;
}
/*USD:modfiy for gesture recover for main tp by guoxin.zhu 20240522 end*/


#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations gesture_fops = {
    .read = gesture_mode_read,
    .write = gesture_mode_write,
};
#else
static const struct proc_ops gesture_fops = {
    .proc_read = gesture_mode_read,
    .proc_write = gesture_mode_write,
};
#endif

/*USD: modify for gesture switch state check by guoxin.zhu 20250529 start*/
static ssize_t sub_gesture_mode_write(struct file *filp, const char __user * buf, size_t len, loff_t * off)
{
    int32_t ret;
    char buff[2]={'0','\0'};
    struct tran_ts_controller *pController = g_tran_ts.controller[TRAN_SUB_CONTROLLER];

    if(!pController || !pController->controller_init_state){
        CTP_ERROR("sub device init faile do nothing !");
        return -EPERM;
    }

    ret = copy_from_user(buff, buf, 1);
    if(ret){
        CTP_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    mutex_lock(&pController->dsp_event_mutex);
    if (buff[0] == '0' && !pController->gesture_write_flag) {
        CTP_INFO("[sub][0]: recover gesture mode");
        pController->gesture_flag = true;
        pController->gesture_write_flag = 1;
    } else if (buff[0] == '1' && pController->gesture_write_flag) {
        CTP_INFO("[sub][1]: enter sleep mode");
        pController->gesture_flag = false;
        pController->gesture_write_flag = 0;
        if(pController->controller_state != TRAN_SUSPEND_MODE) {
            state_change_machine(SUSPEND_TRIGER, pController);
            pController->ops->tran_ts_suspend(NULL, 1);
        }
    } else if (buff[0] == '2' && !pController->gesture_write_flag) {
        pController->gesture_flag = true;
    if(pController->controller_state == TRAN_ACTIVE_MODE){
            CTP_INFO("[sub][2]: controller active recover gesture flag");
    }else if(pController->controller_state != TRAN_GESTURE_SUSPEND_MODE) {
            CTP_INFO("[sub][2]: enter gesture mode");
            state_change_machine(SUSPEND_TRIGER, pController);
            pController->ext_ops->tran_tp_reset(140);
            pController->ops->tran_ts_suspend(NULL, 1);
        }
        pController->gesture_write_flag = 1;
    }  else if (buff[0] == '3' && pController->gesture_write_flag) {
            CTP_INFO("[sub][3]: vip take pictures mode");
            //ts_data->resume_flag = true;
            //ts_data->gesture_flag = true;
            pController->gesture_write_flag = 0;
    }
    mutex_unlock(&pController->dsp_event_mutex);

    return len;
}
/*USD: modify for gesture switch state check by guoxin.zhu 20250529 end*/

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations sub_gesture_fops = {
    .read = gesture_mode_read,
    .write = sub_gesture_mode_write,
};
#else
static const struct proc_ops sub_gesture_fops = {
    .proc_read = gesture_mode_read,
    .proc_write = sub_gesture_mode_write,
};
#endif
//BSP:add when display off switch tp gesture or sleep mode node by jiangzhu.yang 20220830 end
static ssize_t gesture_test_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
    char buf[32];
    int len;

    len = snprintf(buf, sizeof(buf), "%d\n", g_gtp_gesture);
    if (*ppos >= len)
        return 0;
    if (copy_to_user(buffer, buf, len))
        return -EFAULT;
    *ppos += len;

    return len;
}

static ssize_t gesture_test_write(struct file *filp, const char __user * buf, size_t len, loff_t * off)
{
    int32_t ret,i;
    char cmd[4]={'0','0','0','\0'};
    struct tran_ts_controller *pController = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];

    TRAN_FUNC_ENTER();

    TRAN_INFO("gesture test write len:%zu",len);
    TRAN_INFO("tran_ts_controller:%p",pController);

    if(!pController || !pController->controller_init_state){
        TRAN_ERROR("sub device init faile do nothing !");
        TRAN_FUNC_EXIT();
        return -EPERM;
    }

    if(len < 3) {
        TRAN_ERROR("gesture test write len error");
        return -EPERM;
    }

    ret = copy_from_user(cmd, buf, 3);
    if(ret){
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    for(i = 0; i < 3; i++) {
        TRAN_INFO("gesture test write buf[%d]:%c",i,cmd[i]);
    }

    /* 解析命令类型 */
    if (!strncmp(cmd, "cc0", 3)) {
        g_gtp_gesture = GESTURE_DC;
    } else if (!strncmp(cmd, "be0", 3)) {
        g_gtp_gesture = GESTURE_OC;
    } else if (!strncmp(cmd, "aa0", 3)) {
        g_gtp_gesture = GESTURE_RT;
    } else if (!strncmp(cmd, "bb0", 3)) {
        g_gtp_gesture = GESTURE_LF;
    } else if (!strncmp(cmd, "ab0", 3)) {
        g_gtp_gesture = GESTURE_down;
    } else if (!strncmp(cmd, "ba0", 3)) {
        g_gtp_gesture = GESTURE_up;
    } else if (!strncmp(cmd, "w00", 3)) {
        g_gtp_gesture = GESTURE_w;
    } else if (!strncmp(cmd, "z00", 3)) {
        g_gtp_gesture = GESTURE_z;
    } else if (!strncmp(cmd, "c00", 3)) {
        g_gtp_gesture = GESTURE_c;
    } else if (!strncmp(cmd, "v00", 3)) {
        g_gtp_gesture = GESTURE_v;
    } else if (!strncmp(cmd, "e00", 3)) {
        g_gtp_gesture = GESTURE_e;
    } else if (!strncmp(cmd, "s00", 3)) {
        g_gtp_gesture = GESTURE_s;
    } else if (!strncmp(cmd, "m00", 3)) {
        g_gtp_gesture = GESTURE_m;
    } else if (!strncmp(cmd, "o00", 3)) {
        g_gtp_gesture = GESTURE_o;
    } else if(!strncmp(cmd, "z00", 3)) {
        g_gtp_gesture = GESTURE_z;
    } else if(!strncmp(cmd, "t01", 3)) {
        gestureclick_enabled = 1;
        gesturechar_enabled = 1;
        gesturemusic_enabled = 1;
        gesturedouble_enabled = 1;
        for(i = 0 ; i < ARRAY_SIZE(gesture_maps) ; i++)
        {
            gesture_maps[i].enable = 1;
        }
    } else if(!strncmp(cmd, "t00", 3)) {
        gestureclick_enabled = 0;
        gesturechar_enabled = 0;
        gesturemusic_enabled = 0;
        gesturedouble_enabled = 0;
        for(i = 0 ; i < ARRAY_SIZE(gesture_maps) ; i++)
        {
            gesture_maps[i].enable = 0;
        }
    }

    TRAN_INFO("gesture test write cmd:0x%x",g_gtp_gesture);
    GestureHandle(pController);

    return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations gesture_test_ops = {
    .read = gesture_test_read,
    .write = gesture_test_write,
};
#else
static const struct proc_ops gesture_test_ops = {
    .proc_read = gesture_test_read,
    .proc_write = gesture_test_write,
};
#endif

struct tran_ts_proc_node {
    char *name;
    umode_t mode;
    struct proc_dir_entry *proc_dir;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
    const struct file_operations *proc_fops;
#else
    const struct proc_ops *proc_fops;
#endif
};

struct tran_ts_proc_node tran_gesture_proc_node[] = {
    /* main controller function */
    {"main_gesture_function", 0666, NULL, &gesture_node_ops},
    {"main_gesture_state", 0666, NULL, &gesture_state_ops},
    {"main_gesture_flag", 0666, NULL, &gesture_fops},

    /* sub controller function */
    {"gesture_function", 0666, NULL, &gesture_node_ops},
    {"gesture_state", 0666, NULL, &gesture_state_ops},
    {"gesture_flag", 0666, NULL, &sub_gesture_fops},
    {"touch_gesture", 0666, NULL, &gesture_test_ops},
};

static void tran_gesture_proc_node_create(struct tran_ts_core *pdata)
{
    struct proc_dir_entry *proc_entry = NULL;
    int i, num_nodes = sizeof(tran_gesture_proc_node) / sizeof(struct tran_ts_proc_node);

    TRAN_FUNC_ENTER();

    for (i = 0; i < num_nodes; i++) {
        proc_entry = NULL;
        proc_entry = proc_create(tran_gesture_proc_node[i].name, tran_gesture_proc_node[i].mode, \
                        tran_gesture_proc_node[i].proc_dir, tran_gesture_proc_node[i].proc_fops);
        if (!proc_entry) {
            TRAN_ERROR("Failed to create proc entry for %s\n", tran_gesture_proc_node[i].name);
            continue;
        }
    }

    TRAN_FUNC_EXIT();
}

void gesture_init(struct tran_ts_core *pdata)
{
    int ret;

    TRAN_FUNC_ENTER();
    mutex_init(&gesture_data_mutex);
    memset(gestures_flag, 0, sizeof(gestures_flag));//for debug   0-->1
    memset((u8 *) & gesture_data, 0, sizeof(st_gesture_data));

    /* init gesture ops */
    pdata->tpd_ops->gesture_id_report = GestureIdTranslate;
    pdata->tpd_ops->gesture_state_get = SetDozeStatus;
    //BSP: add goodix for TP bringup by hai.tang5 20260302 start
    pdata->tpd_ops->gesture_switch_status = gesture_switch_status;
    //BSP: add goodix for TP bringup by hai.tang5 20260302 end

    /* create proc node */
    tran_gesture_proc_node_create(pdata);

    /* create sys node */
    ret = sysfs_create_group(&pdata->pdev->dev.kobj, &gesture_attribute_group);
    if (ret) {
        TRAN_ERROR("sysfs_create_group() gesture failed!!");
        sysfs_remove_group(&pdata->pdev->dev.kobj, &gesture_attribute_group);
        return;
    } else {
        TRAN_INFO("sysfs_create_group() gesture succeeded!!");
    }

    TRAN_FUNC_EXIT();
}

