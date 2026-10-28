#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include "transsion_core.h"

#define SELFTEST_DATA_MAX_SIZE    (1024*8*5)

extern struct tran_ts_controller *get_active_controller(void);
/*USD: add for pocket anti accidental touch by longtao.ran 20240605 start*/
static ssize_t tp_pocket_state_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    uint8_t tran_pocket_state;
    char buff[2]= {'0','\0'};
    struct tran_ts_controller *ctr = NULL;

    if (count == 0 || count > 2) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff, buf, 1);
    if (ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        TRAN_ERROR("Invalid value! ret = %d", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp != 0 && tmp != 1) {
        TRAN_ERROR("Invalid value! ret = %d, tran_pocket_state val = %d", ret, tmp);
        return -EINVAL;
        goto out;
    }

    tran_pocket_state = tmp;
    TRAN_INFO("tran_pocket_state = %d", tran_pocket_state);

    g_tran_ts.tran_pocket_state = tran_pocket_state;

    ctr = get_active_controller();
    if (ctr == NULL) {
        TRAN_ERROR("controller is null");
        return -EINVAL;
    }

    if (TRAN_SUSPEND_MODE == ctr->controller_state) {
        TRAN_ERROR("TP already suspended do not tran_pocket_state");
    } else {
        ctr->ops->set_ts_mode(SET_POCKET_MODE, tran_pocket_state);
    }
    ret = count;

out:
    return ret;
}
static ssize_t tp_pocket_state_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
    static int finished = 0;
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;

    /*
    * We return 0 to indicate end of file, that we have
    * no more information. Otherwise, processes will
    * continue to read from us in an endless loop.
    */
    if (finished) {
        TRAN_INFO("read END");
        finished = 0;
        return 0;
    }
    finished = 1;

    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    if (IS_ERR_OR_NULL(page)) {
        return -ENOMEM;
    }

    ptr = page;
    ptr += sprintf(page, "%d\n", g_tran_ts.tran_pocket_state);
    len = ptr -page;
    if (*offp >= len) {
        kfree(page);
        return 0;
    }
    if (copy_to_user(buff,(char *)page,len))
    {
        kfree(page);
        return len;
    }
    *offp += len;
    kfree(page);
    TRAN_INFO("--");
    return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations tp_pocket_state_fops = {
    .owner  = THIS_MODULE,
    .read   = tp_pocket_state_read,
    .write  = tp_pocket_state_write,
};
#else
static const struct proc_ops tp_pocket_state_fops = {
    .proc_read   = tp_pocket_state_read,
    .proc_write  = tp_pocket_state_write,
};
#endif
/*USD: add for pocket anti accidental touch by longtao.ran 20240605 end*/

/*BSP: add for esports feature by yun.luo 20230914 start*/
static ssize_t tp_transient_report_rate_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    uint8_t tran_transient_report_rate;
    char buff[2]= {'0','\0'};
    struct tran_ts_controller *ctr = NULL;

    if (count == 0 || count > 2) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff, buf, 1);
    if (ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        TRAN_ERROR("Invalid value! ret = %d", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp != 0 && tmp != 1) {
        TRAN_ERROR("Invalid value! ret = %d, tran_transient_report_rate val = %d", ret, tmp);
        return -EINVAL;
        goto out;
    }

    tran_transient_report_rate = tmp;
    TRAN_INFO("tran_transient_report_rate = %d", tran_transient_report_rate);

    g_tran_ts.g_tran_transient_report_rate = tran_transient_report_rate;

    ctr = get_active_controller();
    if (ctr == NULL) {
        TRAN_ERROR("no controller running");
        return -EINVAL;
    }
    if (TRAN_SUSPEND_MODE == ctr->controller_state) {
        TRAN_ERROR("TP already suspended do not tran_transient_report_rate");
    } else {
        ctr->ops->set_ts_mode(TRANSIENT_SAMPLING_RATE_SWITCH, tran_transient_report_rate);
    }
    ret = count;

out:
    return ret;
}
static ssize_t tp_transient_report_rate_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
    static int finished = 0;
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;

    /*
    * We return 0 to indicate end of file, that we have
    * no more information. Otherwise, processes will
    * continue to read from us in an endless loop.
    */
    if (finished) {
        TRAN_INFO("read END");
        finished = 0;
        return 0;
    }
    finished = 1;

    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    if (!page) {
        return -ENOMEM;
    }

    ptr = page;
    ptr += sprintf(page, "%d\n", g_tran_ts.g_tran_transient_report_rate);
    len = ptr -page;
    if (*offp >= len) {
        kfree(page);
        return 0;
    }
    if (copy_to_user(buff,(char *)page,len))
    {
        kfree(page);
        return len;
    }
    *offp += len;
    kfree(page);
    TRAN_INFO("--");
    return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations tp_transient_report_rate_fops = {
    .owner  = THIS_MODULE,
    .read   = tp_transient_report_rate_read,
    .write  = tp_transient_report_rate_write,
};
#else
static const struct proc_ops tp_transient_report_rate_fops = {
    .proc_read   = tp_transient_report_rate_read,
    .proc_write  = tp_transient_report_rate_write,
};
#endif

static ssize_t tp_report_rate_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    uint8_t tran_report_rate;
    char buff[2]= {'0','\0'};
    struct tran_ts_controller *ctr = NULL;

    if (count == 0 || count > 2) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff, buf, 1);
    if (ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        TRAN_ERROR("Invalid value! ret = %d", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp != 0 && tmp != 1 && tmp != 2 && tmp != 3 && tmp != 4 && tmp != 5) {
        TRAN_ERROR("Invalid value! ret = %d, tran_report_rate val = %d", ret, tmp);
        return -EINVAL;
        goto out;
    }

    tran_report_rate = tmp;
    TRAN_INFO("tran_report_rate = %d", tran_report_rate);

    g_tran_ts.g_tran_report_rate = tran_report_rate;

    ctr = get_active_controller();
    if (ctr == NULL) {
        TRAN_ERROR("no controller running");
        return -EINVAL;
    }
    if (TRAN_SUSPEND_MODE == ctr->controller_state) {
        TRAN_ERROR("TP already suspended do not set report_rate");
    } else {
        ctr->ops->set_ts_mode(REPORT_RATE_SWITCH, tran_report_rate);
    }
    ret = count;

out:
    return ret;
}
static ssize_t tp_report_rate_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
    static int finished = 0;
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;

    /*
    * We return 0 to indicate end of file, that we have
    * no more information. Otherwise, processes will
    * continue to read from us in an endless loop.
    */
    if (finished) {
        TRAN_INFO("read END");
        finished = 0;
        return 0;
    }
    finished = 1;

    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    if (!page) {
        return -ENOMEM;
    }

    ptr = page;
    ptr += sprintf(page, "%d\n", g_tran_ts.g_tran_report_rate);
    len = ptr -page;
    if (*offp >= len) {
        kfree(page);
        return 0;
    }
    if (copy_to_user(buff,(char *)page,len))
    {
        kfree(page);
        return len;
    }
    *offp += len;
    kfree(page);
    TRAN_INFO("--");
    return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations tp_report_rate_fops = {
    .owner  = THIS_MODULE,
    .read   = tp_report_rate_read,
    .write  = tp_report_rate_write,
};
#else
static const struct proc_ops tp_report_rate_fops = {
    .proc_read   = tp_report_rate_read,
    .proc_write  = tp_report_rate_write,
};
#endif

static ssize_t tp_game_mode_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    uint8_t tran_game_mode;
    char buff[2]= {'0','\0'};
    struct tran_ts_controller *ctr = NULL;

    if (count == 0 || count > 2) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff, buf, 1);
    if (ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        TRAN_ERROR("Invalid value! ret = %d", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp != 0 && tmp != 1 && tmp != 2 && tmp != 3 && tmp != 4 && tmp != 5) {
        TRAN_ERROR("Invalid value! ret = %d, tran_report_rate val = %d", ret, tmp);
        return -EINVAL;
        goto out;
    }

    tran_game_mode = tmp;
    TRAN_INFO("tran_report_rate = %d", tran_game_mode);

    g_tran_ts.g_tran_game_mode = tran_game_mode;

    ctr = get_active_controller();
    if (ctr == NULL) {
        TRAN_ERROR("no controller running");
        return -EINVAL;
    }
    if (TRAN_SUSPEND_MODE == ctr->controller_state) {
        TRAN_ERROR("TP already suspended do not set game_mode");
    } else {
        ctr->ops->set_ts_mode(LOCK_POINT_SWITCH, tran_game_mode);
    }
    ret = count;

out:
    return ret;
}
static ssize_t tp_game_mode_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
    static int finished = 0;
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;
    u8 lock_point_distance = 0;
    struct tran_ts_controller *ctr = NULL;

    /*
    * We return 0 to indicate end of file, that we have
    * no more information. Otherwise, processes will
    * continue to read from us in an endless loop.
    */
    if (finished) {
        TRAN_INFO("read END");
        finished = 0;
        return 0;
    }
    finished = 1;

    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    if (!page) {
        return -ENOMEM;
    }

    ctr = get_active_controller();
    if (ctr == NULL) {
        TRAN_ERROR("no controller running");
        return -EINVAL;
    }

    if(ctr->ops->get_lock_point_distance){
        ctr->ops->get_lock_point_distance(&lock_point_distance);
    }
    ptr = page;
    ptr += sprintf(page, "Gear position:%d\nLock point distance:%d\n", g_tran_ts.g_tran_game_mode,lock_point_distance);

    len = ptr -page;
    if (*offp >= len) {
        kfree(page);
        return 0;
    }
    if (copy_to_user(buff,(char *)page,len))
    {
        kfree(page);
        return len;
    }
    *offp += len;
    kfree(page);
    TRAN_INFO("--");
    return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations tp_game_mode_fops = {
    .owner  = THIS_MODULE,
    .read   = tp_game_mode_read,
    .write  = tp_game_mode_write,
};
#else
static const struct proc_ops tp_game_mode_fops = {
    .proc_read   = tp_game_mode_read,
    .proc_write  = tp_game_mode_write,
};
#endif
/*BSP: add for esports feature by yun.luo 20230914 end*/

//BSP: add for custom mistouch by linsheng.liu 20231019 start
int chartohex(char b,char c)
{
    int a;
    //first number
    if(b>='0'&& b<='9')
    {
        a=(b-48)*16;
    }else if(b=='a'||b=='A'){
        a=10*16;
    }else if(b=='b'||b=='B'){
        a=11*16;
    }else if(b=='c'||b=='C'){
        a=12*16;
    }else if(b=='d'||b=='D'){
        a=13*16;
    }else if(b=='e'||b=='E'){
        a=14*16;
    }else if(b=='f'||b=='F'){
        a=15*16;
    }else{
        a=0;
    }
    //second number
    if(c>='0'&& c<='9')
    {
        a=a+c-48;
    }else if(c=='a'||c=='A'){
        a=a+10;
    }else if(c=='b'||c=='B'){
        a=a+11;
    }else if(c=='c'||c=='C'){
        a=a+12;
    }else if(c=='d'||c=='D'){
        a=a+13;
    }else if(c=='e'||c=='E'){
        a=a+14;
    }else if(c=='f'||c=='F'){
        a=a+15;
    }
    return a;
}

/*
* close:
* 0x01 0x03 0x00 0x00
*
* open:
* 0x06 0x03 0x00 0x01 0x0b 0x01 0x01 0x0 0x0 0x0 0x0 0x0 0x0 0x0 0x0 0x0b 0x01 0x01 0x0 0x0 0x0 0x0 0x0 0x0 0x0 0x0 0x0b 0x01 0x01 0x0 0x0 0x0 0x0 0x0 0x0 0x0 0x0 0x0b 0x01 0x01 * 0x04 0x37 0x09 0x83 0x03 0x70 0x08 0xbc  0x0b 0x01 0x01 00x04 0x37 0x0 0x0 0x03 0x70 0x0 0xc8
*
*
* ((100,100)  (700,1000))     // 0x0 0x64 0x0 0x64 0x02 0xbc 0x03 0xe8
*
* ((100,1200)  (500,1500))    // 0x0 0x64 0x4 0xb0 0x1 0xf4 0x05 0xdc
*
* ((200,2000)  (900,2300))    // 0x0 0xc8 0x7 0xd0 0x03 0x84 0x08 0xfc
*/
static ssize_t custom_mistouch_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t i=0;
    int32_t j=0;
    char str[400];
    int a[60]={0};
    struct tran_ts_controller *ctr = NULL;

    ctr = get_active_controller();
    if (ctr == NULL) {
        TRAN_ERROR("no controller running");
        return -EINVAL;
    }

    if (count == 0 || count > 400) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(str, buf, count);
    if (ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }
   str[count-1]='\0';
   for(i=0;str[i]!='\0';i++)
   {
        if((str[i]=='x'|| str[i]=='X')&&(j<60))
        {
            if(str[i+2]==' '||str[i+2]=='\0')
            {
                a[j++]=chartohex('N',str[i+1]);
            }else{
                a[j++]=chartohex(str[i+1],str[i+2]);
            }
        }
   }
    if (TRAN_SUSPEND_MODE == ctr->controller_state) {
        TRAN_ERROR("TP already suspended do not set game_mode");
    } else {
    switch(a[0]) {
    case 1:
    for(i=0;i<5;i++)
    {
         g_tran_ts.mistouch[i].LeftStartX_H =0;
         g_tran_ts.mistouch[i].LeftStartX_L =0;
         g_tran_ts.mistouch[i].LeftStartY_H =0;
         g_tran_ts.mistouch[i].LeftStartY_L =0;
         g_tran_ts.mistouch[i].RightStartX_H =0;
         g_tran_ts.mistouch[i].RightStartX_L =0;
         g_tran_ts.mistouch[i].RightStartY_H =0;
         g_tran_ts.mistouch[i].RightStartY_L =0;
    }
         ctr->ops->set_ts_mode(SET_CUSTOM_MISTOUCH_MODE,1);
        break;
    case 6:
    for(i=0;i<5;i++)
    {
         g_tran_ts.mistouch[i].LeftStartX_H =a[7+11*i];
         g_tran_ts.mistouch[i].LeftStartX_L =a[8+11*i];
         g_tran_ts.mistouch[i].LeftStartY_H =a[9+11*i];
         g_tran_ts.mistouch[i].LeftStartY_L =a[10+11*i];
         g_tran_ts.mistouch[i].RightStartX_H =a[11+11*i];
         g_tran_ts.mistouch[i].RightStartX_L =a[12+11*i];
         g_tran_ts.mistouch[i].RightStartY_H =a[13+11*i];
         g_tran_ts.mistouch[i].RightStartY_L =a[14+11*i];
    }
         ctr->ops->set_ts_mode(SET_CUSTOM_MISTOUCH_MODE,1);
        break;
    default:
        TRAN_INFO("can't find cmd");
        break;
        }
    }
    ret = count;

out:
    return ret;
}
static ssize_t custom_mistouch_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
    static int finished = 0;
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;

    /*
    * We return 0 to indicate end of file, that we have
    * no more information. Otherwise, processes will
    * continue to read from us in an endless loop.
    */
    if (finished) {
        TRAN_INFO("read END");
        finished = 0;
        return 0;
    }
    finished = 1;

    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    if (!page) {
        return -ENOMEM;
    }

    ptr = page;
    ptr += sprintf(page, "First region(0x%x%x 0x%x%x) (0x%x%x 0x%x%x)\nSecond region(0x%x%x 0x%x%x) (0x%x%x 0x%x%x)\nThird region(0x%x%x 0x%x%x) (0x%x%x 0x%x%x)\nfour region(0x%x%x 0x%x%x) (0x%x%x 0x%x%x)\nfive region(0x%x%x 0x%x%x) (0x%x%x 0x%x%x)\n",
         g_tran_ts.mistouch[0].LeftStartX_H ,
         g_tran_ts.mistouch[0].LeftStartX_L ,
         g_tran_ts.mistouch[0].LeftStartY_H ,
         g_tran_ts.mistouch[0].LeftStartY_L ,
         g_tran_ts.mistouch[0].RightStartX_H ,
         g_tran_ts.mistouch[0].RightStartX_L ,
         g_tran_ts.mistouch[0].RightStartY_H ,
         g_tran_ts.mistouch[0].RightStartY_L ,
         g_tran_ts.mistouch[1].LeftStartX_H ,
         g_tran_ts.mistouch[1].LeftStartX_L ,
         g_tran_ts.mistouch[1].LeftStartY_H ,
         g_tran_ts.mistouch[1].LeftStartY_L ,
         g_tran_ts.mistouch[1].RightStartX_H ,
         g_tran_ts.mistouch[1].RightStartX_L ,
         g_tran_ts.mistouch[1].RightStartY_H ,
         g_tran_ts.mistouch[1].RightStartY_L ,
         g_tran_ts.mistouch[2].LeftStartX_H ,
         g_tran_ts.mistouch[2].LeftStartX_L ,
         g_tran_ts.mistouch[2].LeftStartY_H ,
         g_tran_ts.mistouch[2].LeftStartY_L ,
         g_tran_ts.mistouch[2].RightStartX_H ,
         g_tran_ts.mistouch[2].RightStartX_L ,
         g_tran_ts.mistouch[2].RightStartY_H ,
         g_tran_ts.mistouch[2].RightStartY_L,
         g_tran_ts.mistouch[3].LeftStartX_H ,
         g_tran_ts.mistouch[3].LeftStartX_L ,
         g_tran_ts.mistouch[3].LeftStartY_H ,
         g_tran_ts.mistouch[3].LeftStartY_L ,
         g_tran_ts.mistouch[3].RightStartX_H ,
         g_tran_ts.mistouch[3].RightStartX_L ,
         g_tran_ts.mistouch[3].RightStartY_H ,
         g_tran_ts.mistouch[3].RightStartY_L,
         g_tran_ts.mistouch[4].LeftStartX_H ,
         g_tran_ts.mistouch[4].LeftStartX_L ,
         g_tran_ts.mistouch[4].LeftStartY_H ,
         g_tran_ts.mistouch[4].LeftStartY_L ,
         g_tran_ts.mistouch[4].RightStartX_H ,
         g_tran_ts.mistouch[4].RightStartX_L ,
         g_tran_ts.mistouch[4].RightStartY_H ,
         g_tran_ts.mistouch[4].RightStartY_L
         );
    len = ptr -page;
    if (*offp >= len) {
        kfree(page);
        return 0;
    }
    if (copy_to_user(buff,(char *)page,len))
    {
        kfree(page);
        return len;
    }
    *offp += len;
    kfree(page);
    TRAN_INFO("--");
    return len;
}



#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations custom_mistouch_fops = {
    .owner  = THIS_MODULE,
    .read   = custom_mistouch_read,
    .write  = custom_mistouch_write,
};
#else
static const struct proc_ops custom_mistouch_fops = {
    .proc_read   = custom_mistouch_read,
    .proc_write  = custom_mistouch_write,
};
#endif
//BSP: add for custom mistouch by linsheng.liu 20231019 end
/* BSP: add for esports feature by longtao.ran 20230818 start */
static ssize_t tp_game_state_write(
    struct file *filp, const char __user *buf, unsigned long count, loff_t *off)
{
    int32_t ret;
    int32_t tmp;
    uint8_t g_tran_game_state = 0;
    char buff[2]= {'0','\0'};
    struct tran_ts_controller *ctr = NULL;

    ret = copy_from_user(buff, buf, 1);
    if(ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        TRAN_ERROR("Invalid value! ret = %d", ret);
        ret = -EINVAL;
    }
    if ( tmp > 1) {
        TRAN_ERROR("Invalid value! tmp = %d", tmp);
        ret = -EINVAL;
    }

    g_tran_game_state = tmp;
    g_tran_ts.g_tran_game_state = g_tran_game_state;
    TRAN_INFO("tp_game_state_write val = %d", g_tran_game_state);

    ctr = get_active_controller();
    if (ctr == NULL) {
        TRAN_ERROR("no controller running");
        return -EINVAL;
    }
    ctr->ops->set_ts_mode(SET_GAME_MODE, g_tran_game_state);

    ret = count;

    return ret;
}

static ssize_t tp_game_state_read(struct file *filp, char *buf, size_t count, loff_t *off)
{
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;
    uint8_t g_tran_game_state = g_tran_ts.g_tran_game_state;

    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    if (!page) {
        return -ENOMEM;
    }

    ptr = page;
    ptr += sprintf(page, "%d\n", g_tran_game_state);
    len = ptr -page;
    if (*off >= len) {
        kfree(page);
        return 0;
    }
    if (copy_to_user(buf, (char *)page, len)) {
        kfree(page);
        return len;
    }
    *off += len;
    kfree(page);

    return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations tp_game_state_fops = {
    .owner  = THIS_MODULE,
    .read   = tp_game_state_read,
    .write  = tp_game_state_write,
};
#else
static const struct proc_ops tp_game_state_fops = {
    .proc_read   = tp_game_state_read,
    .proc_write  = tp_game_state_write,
};
#endif
/* BSP: add for esports feature by longtao.ran 20230818 end */

/* BSP: add for tp_micro_control_accuracy by yu.zhang8 start */
static ssize_t tp_micro_control_accuracy_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    uint8_t tp_micro_control_accuracy;
    char buff[2]= {'0','\0'};
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];

    if (ctr == NULL) {
        TRAN_ERROR("controller is null");
        return -EINVAL;
    }
    if (count == 0 || count > 2) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff, buf, 1);
    if (ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        TRAN_ERROR("Invalid value! ret = %d", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp != 0 && tmp != 1 && tmp != 2 && tmp != 3 && tmp != 4 && tmp != 5) {
        TRAN_ERROR("Invalid value! ret = %d, tp_micro_control_accuracy val = %d", ret, tmp);
        return -EINVAL;
        goto out;
    }

    tp_micro_control_accuracy = tmp;
    TRAN_INFO("tp_micro_control_accuracy = %d", tp_micro_control_accuracy);

    g_tran_ts.g_tran_micro_control_accuracy = tp_micro_control_accuracy;
    if (TRAN_SUSPEND_MODE == ctr->controller_state) {
        TRAN_ERROR("TP already suspended do not set game_mode");
    } else {
        ctr->ops->set_ts_mode(MICRO_CONTROL_ACCURACY, tp_micro_control_accuracy);
    }
    ret = count;

out:
    return ret;
}
static ssize_t tp_micro_control_accuracy_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
    static int finished = 0;
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;

    /*
    * We return 0 to indicate end of file, that we have
    * no more information. Otherwise, processes will
    * continue to read from us in an endless loop.
    */
    if (finished) {
        TRAN_INFO("read END");
        finished = 0;
        return 0;
    }
    finished = 1;

    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    if (!page) {
        return -ENOMEM;
    }

    ptr = page;
    ptr += sprintf(page, "%d\n", g_tran_ts.g_tran_micro_control_accuracy);
    len = ptr -page;
    if (*offp >= len) {
        kfree(page);
        return 0;
    }
    if (copy_to_user(buff,(char *)page,len))
    {
        kfree(page);
        return len;
    }
    *offp += len;
    kfree(page);
    TRAN_INFO("--");
    return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations tp_micro_control_accuracy = {
    .owner  = THIS_MODULE,
    .read   = tp_micro_control_accuracy_read,
    .write  = tp_micro_control_accuracy_write,
};
#else
static const struct proc_ops tp_micro_control_accuracy = {
    .proc_read   = tp_micro_control_accuracy_read,
    .proc_write  = tp_micro_control_accuracy_write,
};
#endif
/* BSP: add for tp_micro_control_accuracy by yu.zhang8 end */

//BSP: add for edge leve by hongwu.zhan 20230908 start
static ssize_t tran_edge_level_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    char buff[2]= {'0','\0'};
    uint8_t tran_edge_level;
    struct tran_ts_controller *ctr = NULL;

    if (count == 0 || count > 2) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        return -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff, buf, 1);
    if(ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
        goto out;
    }
    ret = sscanf(buff, "%d", &tmp);
    if (tmp != 0 && tmp != 2 && tmp != 3) {
        TRAN_ERROR("Invalid value! ret = %d, tran_edge_level_write val = %d", ret, tmp);
        return -EINVAL;
        goto out;
    }
    tran_edge_level = tmp;
    TRAN_INFO("tran_edge_level = %d", tran_edge_level);

    g_tran_ts.g_tran_edge_level = tran_edge_level;

    ctr = get_active_controller();
    if (ctr == NULL) {
        TRAN_ERROR("no controller running");
        return -EINVAL;
    }

    if (ctr->controller_state != TRAN_ACTIVE_MODE) {
        TRAN_ERROR("TP already suspended do not set edge level");
    } else {
        ctr->ops->set_ts_mode(SET_EDGE_LEVER_MODE, tran_edge_level);
    }
    ret = count;

out:
    return ret;
}

static ssize_t tran_edge_level_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
    static int finished = 0;
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;

    /*
    * We return 0 to indicate end of file, that we have
    * no more information. Otherwise, processes will
    * continue to read from us in an endless loop.
    */
    if (finished) {
        TRAN_INFO("read END");
        finished = 0;
        return 0;
    }
    finished = 1;

    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    if (!page) {
        return -ENOMEM;
    }

    ptr = page;
    ptr += sprintf(page, "%d\n", g_tran_ts.g_tran_edge_level);
    len = ptr -page;
    if (*offp >= len) {
        kfree(page);
        return 0;
    }
    if (copy_to_user(buff, (char *)page, len)) {
        kfree(page);
        return len;
    }
    *offp += len;
    kfree(page);
    TRAN_INFO("--");

    return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations tran_edge_level_fops = {
    .owner  = THIS_MODULE,
    .read   = tran_edge_level_read,
    .write  = tran_edge_level_write,
};
#else
static const struct proc_ops tran_edge_level_fops = {
    .proc_read   = tran_edge_level_read,
    .proc_write  = tran_edge_level_write,
};
#endif
//BSP: add for edge leve by hongwu.zhan 20230908 start

static ssize_t tran_tp_scene_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    char buff[2]= {'0','\0'};
    uint8_t tran_tp_scenee;
    struct tran_ts_controller *ctr = NULL;

    if (count == 0 || count > 2) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        return -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff, buf, 1);
    if(ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
        goto out;
    }
    ret = sscanf(buff, "%d", &tmp);
    if (tmp < 0 || tmp > 4) {
        TRAN_ERROR("Invalid value! ret = %d, tran_tp_scene_write val = %d", ret, tmp);
        return -EINVAL;
        goto out;
    }
    tran_tp_scenee = (uint8_t)tmp;
    TRAN_INFO(" tran_tp_scenee = %d", tran_tp_scenee);
    g_tran_ts.g_tran_tp_scene = tran_tp_scenee;

        ctr = get_active_controller();
    if (ctr == NULL) {
        TRAN_ERROR("controller is null");
        return -EINVAL;
    }

    if (TRAN_SUSPEND_MODE == ctr->controller_state) {
        TRAN_ERROR("TP already suspended do not set edge level");
    } else {
        ctr->ops->set_ts_mode(GET_SCENE_STATUS, tran_tp_scenee);
    }
    ret = count;
out:
    return ret;
}
static ssize_t tran_tp_scene_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
    static int finished = 0;
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;
    /*
    * We return 0 to indicate end of file, that we have
    * no more information. Otherwise, processes will
    * continue to read from us in an endless loop.
    */
    if (finished) {
        TRAN_INFO("read END");
        finished = 0;
        return 0;
    }
    finished = 1;
    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    if (!page) {
        return -ENOMEM;
    }
    ptr = page;
    ptr += sprintf(page, "%d\n", g_tran_ts.g_tran_tp_scene);
    len = ptr -page;
    if (*offp >= len) {
        kfree(page);
        return 0;
    }
    if (copy_to_user(buff, (char *)page, len)) {
        kfree(page);
        return len;
    }
    *offp += len;
    kfree(page);
    TRAN_INFO("--");
    return len;
}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations tran_tp_scene_fops = {
    .owner  = THIS_MODULE,
    .read   = tran_tp_scene_read,
    .write  = tran_tp_scene_write,
};
#else
static const struct proc_ops tran_tp_scene_fops = {
    .proc_read   = tran_tp_scene_read,
    .proc_write  = tran_tp_scene_write,
};
#endif


static int32_t tran_selftest_close(struct inode *inode, struct file *file)
{
    TRAN_INFO("tran_selftest_close");
    return 0;
}

static int32_t tran_selftest_open(struct inode *inode, struct file *file)
{
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];
    if(ctr && ctr->controller_init_state){
        TRAN_INFO("tran_selftest_open");
        ctr->ops->tran_selftest_open(inode,file);
    }
    return 0;
}

static int32_t sub_tran_selftest_open(struct inode *inode, struct file *file)
{
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_SUB_CONTROLLER];
    if(ctr && ctr->controller_init_state){
        TRAN_INFO("tran_selftest_open");
        ctr->ops->tran_selftest_open(inode,file);
    }
    return 0;
}

static ssize_t tran_selftest_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
    int ret = 0;
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];

    char s_test_pass[] = "Short Test PASS";
    char o_test_pass[] = "Open Test PASS";
    char s_test_fail[] = "Short Test FAIL";
    char o_test_fail[] = "Open Test FAIL";
    int len = -1;
    char *page = NULL;
    char *ptr = NULL;
    int selftest_result;

    if (*offp || !ctr || !ctr->controller_init_state) {
        return 0;
    }

    selftest_result = ctr->ops->tran_selftest();
    TRAN_INFO("selftest_result:%d.", selftest_result);
    page = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!page) {
        kfree(page);
        return -ENOMEM;
    }
    ptr = page;
    if (1 == selftest_result) {
        ptr += sprintf(ptr, "%s\n%s\n", s_test_pass, o_test_pass);
    } else {
        ptr += sprintf(ptr, "%s\n%s\n", s_test_fail, o_test_fail);
    }
    len = ptr - page;
    if(*offp >= len) {
        kfree(page);
        return 0;
    }
    ret = copy_to_user(buff,(char *)page,len);
    *offp += len;
    if(ret) {
        kfree(page);
        return ret;
    }
    kfree(page);
    return len;
}

static ssize_t sub_tran_selftest_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
    int ret = 0;
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_SUB_CONTROLLER];

    char s_test_pass[] = "Short Test PASS";
    char o_test_pass[] = "Open Test PASS";
    char s_test_fail[] = "Short Test FAIL";
    char o_test_fail[] = "Open Test FAIL";
    int len = -1;
    char *page = NULL;
    char *ptr = NULL;
    int selftest_result;

    if (*offp || !ctr || !ctr->controller_init_state) {
        return 0;
    }

    selftest_result = ctr->ops->tran_selftest();
    TRAN_INFO("selftest_result:%d.", selftest_result);
    page = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!page) {
        kfree(page);
        return -ENOMEM;
    }
    ptr = page;
    if (1 == selftest_result) {
        ptr += sprintf(ptr, "%s\n%s\n", s_test_pass, o_test_pass);
    } else {
        ptr += sprintf(ptr, "%s\n%s\n", s_test_fail, o_test_fail);
    }
    len = ptr - page;
    if(*offp >= len) {
        kfree(page);
        return 0;
    }
    ret = copy_to_user(buff,(char *)page,len);
    *offp += len;
    if(ret) {
        kfree(page);
        return ret;
    }
    kfree(page);
    return len;
}

static ssize_t tran_selftest_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    char buff[2]= {'0','\0'};
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];

    if (count == 0 || count > 2) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        return -EINVAL;
    }
    ret = copy_from_user(buff, buf, 1);
    if(ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }
    ret = sscanf(buff, "%d", &tmp);
    if (tmp == 2) {
        TRAN_INFO("enter fixture test mode !!");
        ctr->tran_ts_data->fixture_test_mode = true;
    } else {
        TRAN_INFO("exit fixture test mode !!");
        ctr->tran_ts_data->fixture_test_mode = false;
    }

    ret = count;
    return ret;
}


#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations tran_flash_fops = {
    .open = tran_selftest_open,
    .release = tran_selftest_close,
    .read = tran_selftest_read,
    .write  = tran_selftest_write,
};
#else
static const struct proc_ops tran_flash_fops = {
    .proc_open = tran_selftest_open,
    .proc_release = tran_selftest_close,
    .proc_read = tran_selftest_read,
    .proc_write  = tran_selftest_write,
};
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations sub_tran_flash_fops = {
    .open = sub_tran_selftest_open,
    .release = tran_selftest_close,
    .read = sub_tran_selftest_read,
};
#else
static const struct proc_ops sub_tran_flash_fops = {
    .proc_open = sub_tran_selftest_open,
    .proc_release = tran_selftest_close,
    .proc_read = sub_tran_selftest_read,
};
#endif


static ssize_t edge_reject_switch_proc_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    uint8_t edge_reject_switch;
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_MAIN_CONTROLLER];
    char buff[2]= {'0','\0'};

    TRAN_FUNC_ENTER();
    if (ctr == NULL || (count == 0 || count > 2) || !ctr->controller_init_state) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff,buf,2);
    if(ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        TRAN_ERROR("Invalid value! ret = %d", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp > 4) {
        TRAN_ERROR("Invalid value! tmp = %d", tmp);
        ret = -EINVAL;
        goto out;
    }

    edge_reject_switch = (uint8_t)tmp;
    g_tran_ts.switch_mode = edge_reject_switch;

    if(ctr->controller_state == TRAN_SUSPEND_MODE) {
        TRAN_INFO("tp in sleep mode cancel this opration");
        return ret;
    }

    ctr->ops->set_ts_mode(SET_EDGE_REJECT_MODE, edge_reject_switch);

out:
    return ret;
}

static ssize_t sub_edge_reject_switch_proc_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    uint8_t edge_reject_switch;
    struct tran_ts_controller *ctr = g_tran_ts.controller[TRAN_SUB_CONTROLLER];
    char buff[2]= {'0','\0'};

    TRAN_FUNC_ENTER();
    if(!ctr || !ctr->controller_init_state){
        TRAN_ERROR("vendor init faild do nothing!");
        ret = -EINVAL;
        goto out;
    }
    if (ctr == NULL && (count == 0 || count > 2)) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff,buf,2);
    if(ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        TRAN_ERROR("Invalid value! ret = %d", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp > 4) {
        TRAN_ERROR("Invalid value! tmp = %d", tmp);
        ret = -EINVAL;
        goto out;
    }

    edge_reject_switch = (uint8_t)tmp;
    g_tran_ts.switch_mode = edge_reject_switch;

    if(ctr->controller_state == TRAN_SUSPEND_MODE) {
        TRAN_INFO("tp in sleep mode cancel this opration");
        return ret;
    }

    ctr->ops->set_ts_mode(SET_EDGE_REJECT_MODE, edge_reject_switch);

out:
    return ret;
}

static ssize_t edge_reject_switch_proc_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
    static int finished = 0;
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;

    TRAN_INFO("++");
    /*
    * We return 0 to indicate end of file, that we have
    * no more information. Otherwise, processes will
    * continue to read from us in an endless loop.
    */
    if (finished) {
        TRAN_INFO("read END");
        finished = 0;
        return 0;
    }
    finished = 1;

    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    if (!page) {
        return -ENOMEM;
    }

    ptr = page;
    ptr += sprintf(page, "%d\n", g_tran_ts.switch_mode);
    len = ptr -page;
    if(*offp >= len) {
        kfree(page);
        return 0;
    }
    if(copy_to_user(buff,(char *)page,len))
    {
        kfree(page);
        return len;
    }
    *offp += len;
    kfree(page);
    TRAN_INFO("--");
    return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations edge_reject_switch_fops = {
    .owner = THIS_MODULE,
    .read   = edge_reject_switch_proc_read,
    .write  = edge_reject_switch_proc_write,
};
#else
static const struct proc_ops edge_reject_switch_fops = {
    .proc_read   = edge_reject_switch_proc_read,
    .proc_write  = edge_reject_switch_proc_write,
};
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations sub_edge_reject_switch_fops = {
    .owner = THIS_MODULE,
    .read   = edge_reject_switch_proc_read,
    .write  = sub_edge_reject_switch_proc_write,
};
#else
static const struct proc_ops sub_edge_reject_switch_fops = {
    .proc_read   = edge_reject_switch_proc_read,
    .proc_write  = sub_edge_reject_switch_proc_write,
};
#endif



static int seq_file_demo_show(struct seq_file *seq, void *v)
{
    int data_size = 0;
    char *selftest_data = (char *)seq->private;
    struct tran_ts_controller *ctr = g_tran_ts.controller[0];

    if(selftest_data == NULL) {
        TRAN_INFO("selftest data is NULL;");
        return 0;
    }

    data_size = strlen(selftest_data);
    seq_printf(seq, "%s\n", selftest_data);
    if(seq->size >= data_size) {
        TRAN_INFO("cp fac test data end free test buf");
        vfree(selftest_data);
        ctr->selftest_data = NULL;
    }
    return 0;
}

static int32_t tran_selftest_data_open(struct inode *inode, struct file *file)
{
    struct tran_ts_controller *ctr = g_tran_ts.controller[0];
    char *selftest_data = NULL;

    selftest_data = vmalloc(SELFTEST_DATA_MAX_SIZE);
    if (!selftest_data) {
        TRAN_ERROR("selftest_data vmalloc fail");
        return 0;
    }
    ctr->selftest_data = selftest_data;
    memset(selftest_data, 0, SELFTEST_DATA_MAX_SIZE);
    ctr->ops->get_selftest_data(selftest_data);
    return single_open(file, &seq_file_demo_show, selftest_data);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations tran_fac_selftest_fops = {
    .owner = THIS_MODULE,
    .open = tran_selftest_data_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};
#else
static const struct proc_ops tran_fac_selftest_fops = {
    .proc_open = tran_selftest_data_open,
    .proc_release = single_release,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
};
#endif

//USD: add tne log by hongwu.zhan 20240710 start
static ssize_t tp_tne_test_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    char buff[2]= {'0','\0'};

    if (count == 0 || count > 2) {
        TRAN_ERROR("Invalid value! count = %zu", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff, buf, 1);
    if (ret) {
        TRAN_ERROR("copy_from_user fail.");
        return -EPERM;
    }

    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        TRAN_ERROR("Invalid value! ret = %d", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp != 0 && tmp != 1) {
        TRAN_ERROR("Invalid value! ret = %d, g_tran_tne_test_enable val = %d", ret, tmp);
        return -EINVAL;
        goto out;
    }

    g_tran_ts.g_tran_tne_test_enable = tmp;
    TRAN_INFO("g_tran_tne_test_enable = %d", g_tran_ts.g_tran_tne_test_enable);

    ret = count;

out:
    return ret;
}
static ssize_t tp_tne_test_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
    static int finished = 0;
    int32_t len = 0;
    char *page = NULL;
    char *ptr = NULL;

    /*
    * We return 0 to indicate end of file, that we have
    * no more information. Otherwise, processes will
    * continue to read from us in an endless loop.
    */
    if (finished) {
        TRAN_INFO("read END");
        finished = 0;
        return 0;
    }
    finished = 1;

    page = kmalloc(PAGE_SIZE,GFP_KERNEL);
    if (IS_ERR_OR_NULL(page)) {
        return -ENOMEM;
    }
    ptr = page;
    ptr += sprintf(page, "g_tran_tne_test_enable:%d\n", g_tran_ts.g_tran_tne_test_enable);
    len = ptr -page;
    if (*offp >= len) {
        kfree(page);
        return 0;
    }
    if (copy_to_user(buff,(char *)page,len))
    {
        kfree(page);
        return len;
    }
    *offp += len;
    kfree(page);
    TRAN_INFO("--");
    return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations tp_tne_test_fops = {
    .owner  = THIS_MODULE,
    .read   = tp_tne_test_read,
    .write  = tp_tne_test_write,
};
#else
static const struct proc_ops tp_tne_test_fops = {
    .proc_read   = tp_tne_test_read,
    .proc_write  = tp_tne_test_write,
};
#endif
//USD: add tne log by hongwu.zhan 20240710 end

struct tran_ts_proc_node {
    char *name;
    umode_t mode;
    struct proc_dir_entry *proc_dir;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
    const struct file_operations *proc_fops;
#else
    const struct proc_ops *proc_fops;
#endif
    enum feature_type type;
};

struct tran_ts_proc_node tran_double_panel_node[] = {
    /* main controller function */
    {"main_selftest", 0666, NULL, &tran_flash_fops, FEATURE_BASE_FUNCTION},
    {"main_tran_selftest", 0666, NULL, &tran_fac_selftest_fops, FEATURE_BASE_FUNCTION},
    {"main_border_suppression", 0666, NULL, &edge_reject_switch_fops, FEATURE_BASE_FUNCTION},
    /* sub controller function */
    {"nvt_selftest",0666, NULL, &sub_tran_flash_fops, FEATURE_BASE_FUNCTION},
    {"tran_selftest", 0666, NULL, &tran_fac_selftest_fops, FEATURE_BASE_FUNCTION},
    {"border_suppression", 0666, NULL, &sub_edge_reject_switch_fops, FEATURE_BASE_FUNCTION},
    {"game_mode", 0666, NULL, &tp_game_mode_fops, FEATURE_ESPORT},
    /* edge function */
    {"custom_mistouch", 0666, NULL, &custom_mistouch_fops, FEATURE_GAME_AMT},
    {"tran_edge_level", 0666, NULL, &tran_edge_level_fops, FEATURE_EDGE_AMT},
    /* esports function */
    {"game_state", 0666, NULL, &tp_game_state_fops, FEATURE_ESPORT},
    {"tp_transient_report_rate_switch", 0666, NULL, &tp_transient_report_rate_fops, FEATURE_ESPORT},
    {"tp_report_rate_switch", 0666, NULL, &tp_report_rate_fops, FEATURE_DYNAMIC_REPORT_RATE},
    {"tp_micro_control_accuracy", 0666, NULL, &tp_micro_control_accuracy, FEATURE_ESPORT},
    /* tne log */
    {"tp_tne_test", 0666, NULL, &tp_tne_test_fops, FEATURE_BASE_FUNCTION},
};

struct tran_ts_proc_node tran_single_panel_node[] = {
    /* main controller function */
    {"nvt_selftest", 0666, NULL, &tran_flash_fops, FEATURE_BASE_FUNCTION},
    {"tran_selftest", 0666, NULL, &tran_fac_selftest_fops, FEATURE_BASE_FUNCTION},
    {"tp_panel_test", 0666, NULL, &tran_flash_fops, FEATURE_BASE_FUNCTION},
    {"border_suppression", 0666, NULL, &edge_reject_switch_fops, FEATURE_BASE_FUNCTION},
    {"game_mode", 0666, NULL, &tp_game_mode_fops, FEATURE_ESPORT},
    /* edge function */
    {"custom_mistouch", 0666, NULL, &custom_mistouch_fops, FEATURE_GAME_AMT},
    {"tran_edge_level", 0666, NULL, &tran_edge_level_fops, FEATURE_EDGE_AMT},
    /* esports function */
    {"game_state", 0666, NULL, &tp_game_state_fops, FEATURE_ESPORT},
    {"tp_transient_report_rate_switch", 0666, NULL, &tp_transient_report_rate_fops, FEATURE_ESPORT},
    {"tp_report_rate_switch", 0666, NULL, &tp_report_rate_fops, FEATURE_DYNAMIC_REPORT_RATE},
    {"tp_large_area_mistouch_switch", 0666, NULL, &tp_pocket_state_fops, FEATURE_POCKET_AMT},
    {"tp_micro_control_accuracy", 0666, NULL, &tp_micro_control_accuracy, FEATURE_ESPORT},
    /* tne log */
    {"tp_tne_test", 0666, NULL, &tp_tne_test_fops, FEATURE_BASE_FUNCTION},
    {"tp_scene", 0666, NULL, &tran_tp_scene_fops, FEATURE_TP_SCENE},
};

static void tran_proc_node_create(struct tran_ts_core *pdata)
{
    struct proc_dir_entry *proc_entry = NULL;
    struct tran_ts_proc_node *tran_proc_node = NULL;
    struct touchscreen_feature *features = &pdata->features;
    int i, num_nodes;
    
    TRAN_FUNC_ENTER();
    
    if(features->fold_touch == true) {
        tran_proc_node = tran_double_panel_node;
        num_nodes = sizeof(tran_double_panel_node) / sizeof(struct tran_ts_proc_node);
    } else {
        tran_proc_node = tran_single_panel_node;
        num_nodes = sizeof(tran_single_panel_node) / sizeof(struct tran_ts_proc_node);
    }

    for (i = 0; i < num_nodes; i++) {
        proc_entry = NULL;
        if(!is_support_feature(tran_proc_node[i].type)) {
            TRAN_INFO("function not support, skip %s create", tran_proc_node[i].name);
            continue;
        }

        proc_entry = proc_create(tran_proc_node[i].name, tran_proc_node[i].mode, tran_proc_node[i].proc_dir, tran_proc_node[i].proc_fops);
        if (!proc_entry) {
            TRAN_ERROR("Failed to create proc entry for %s\n", tran_proc_node[i].name);
            continue;
        }
    }

    TRAN_FUNC_EXIT();
}

extern int tp_poll_drv_init(void);
static void tran_sys_node_create(struct tran_ts_core *pdata)
{
    TRAN_FUNC_ENTER();
    tp_poll_drv_init();
    TRAN_FUNC_EXIT();
    return;
}

void tran_node_init(struct tran_ts_core *pdata)
{
    struct tran_ts_platform *plat = pdata->platform_data;
    TRAN_FUNC_ENTER();

    if(plat == NULL) {
        TRAN_ERROR("tran_platform_t is NULL!");
        return ;
    }

    tran_proc_node_create(pdata);
    tran_sys_node_create(pdata);
}
