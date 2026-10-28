/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 TRANSSION Inc. 2025
 * Author xiaobo.fan
 * Transsion power key to display on monitor
 */
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/syscalls.h>
#include <linux/mfd/mt6363/core.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/reboot.h>
#include <linux/suspend.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/string.h>
#include <linux/input.h>
#include <linux/time64.h>
#define MODULE_NAME 		"tran_key_bigdata"
#define TNE_LOG_DERCRIPTION	"power key display on time out"
#define TNE_ID 				"0xffffff3d"
#define TIMOUE_MIN			(1000)
#define LOG_LEVEL_INFO    KERN_INFO
#define LOG_LEVEL_WARN    KERN_WARNING
#define LOG_LEVEL_ERR     KERN_ERR
#define print_log(level, fmt, ...)    printk(level MODULE_NAME ": " fmt, ##__VA_ARGS__)
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
#include "tranlog.h"
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
#include "tnek.h"
//USD: add for bigdata by junyu.chen2 20250804 start
__attribute__((unused)) const static int64_t FLAG_TNE_USER_LOG_MAIN_SYS = 1 << 1; //mobile log:android and system log
__attribute__((unused)) const static int64_t FLAG_TNE_USER_LOG_EVENT = 1 << 2;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_LOG_RADIO = 1 << 3;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_LOG_CRASH = 1 << 4;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_LOG_KERNEL = 1 << 5;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_ANR_TRACE = 1 << 6; //ams dumptrace ,begin with anr_
__attribute__((unused)) const static int64_t FLAG_TNE_USER_BINDER = 1 << 7;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_MAPS = 1 << 8;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_SIGNAL_TRACE = 1 << 9; //signal catcher trace, signal 3 begin with trace_
__attribute__((unused)) const static int64_t FLAG_TNE_USER_NATIVE_TRACE = 1<< 10; //debuggerd for native trace
__attribute__((unused)) const static int64_t FLAG_TNE_USER_LAST_KMSG = 1<< 11; //
__attribute__((unused)) const static int64_t FLAG_TNE_USER_PS = 1<<12;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_PROC_MEMINFO = 1<<13;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_ZONE_INFO = 1<<14;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_BLOCKIO = 1<<15;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_MMSTAT = 1<<16;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_RAM_CONSOLE = 1<<17; //contain Internal error: Oops or Kernel panic
__attribute__((unused)) const static int64_t FLAG_TNE_USER_PROPERTY = 1<<18;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_DF = 1<<19;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_MOUNT = 1<<20;
__attribute__((unused)) const static int64_t FLAG_TNE_MINIDUMP = 1<<21;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_WINDOW = 1<<22;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_PROC_BASIC = 1<<23;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_BASIC = 1<<24;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_CPU_INFO = 1<<25;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_SYS_MEM = 1<<26;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_BINDER_CALLS_STATS = 1 << 27;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_LOOPER_STATS = 1 << 28;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_SYS_MEM_EXT = 1 << 29;
__attribute__((unused)) const static int64_t FLAG_TNE_USER_APP_INFO = 1 << 30;
//USD: add for bigdata by junyu.chen2 20250804 end
//android_main_log system_log dumpsys_activity dumpsys_input dumpsys_sufaceflinger dumpsys_window proc_meminfo sys_prop kernel_log
#define TNE_NEED_LOG_TYPE	(FLAG_TNE_USER_LOG_MAIN_SYS |FLAG_TNE_USER_WINDOW|FLAG_TNE_USER_SYS_MEM|FLAG_TNE_USER_PROPERTY| FLAG_TNE_USER_LOG_KERNEL)
#endif
static char* g_cloud_id = "1008000016";
static long long g_bigdata_tid = 486360000008;
extern int register_cloudengine_notifier(const char* id, const char* ver, struct notifier_block *nb);
extern int unregister_cloudengine_notifier(const char* id, struct notifier_block *nb);
//extern void cloudengine_feed_back(const char* id, int state);
extern char* cloudengine_getConfig(const char* id);
//extern char* cloudengine_getStringByContent(char* content, const char* key);
//extern int cloudengine_getIntByContent(char* content, const char* key);
//extern char* cloudengine_getStringByID(const char* id, const char* key);
//extern int cloudengine_getIntByID(const char* id, const char* key);
static char* g_cloud_key = "pwrdis_monitor";
static bool  is_cloud_enabled = false;
static int g_tran_key_brightness_level = 0;
static int g_monitor_timer_out_ms = 1000 * 5;
static char* g_timeout_key = "pwrdis_timeout";
static bool is_backlight_on_before = false;
static atomic_t g_is_powerkey_pressed = ATOMIC_INIT(0);
static struct work_struct tran_custom_key_work;
struct delayed_work key_report_work;
static bool driver_loaded = false;
static bool is_key_report_running = false;
extern struct athena_msg* alloc_msg(void);
extern void free_msg(struct athena_msg **athena);
extern void submit_msg(struct athena_msg **athena, int version, long long tid, char *tag);
extern void msg_put_string(struct athena_msg **athena, char *key, char *value);
extern void msg_put_int(struct athena_msg **athena, char *key, int value);
extern void msg_put_long(struct athena_msg **athena, char *key, long value);
static struct work_struct report_key_times_work;
static struct timer_list hourly_timer;
#if IS_ENABLED(CONFIG_TRAN_FREEZE_AEGIS_SUPPORT)
extern int startAegisK(const char *code, const char *pids, const char *extra);
#endif
//report tne log
//key times
//USD: add for keytimes by junyu.chen2 20250825 start
struct device_key_times {
    struct timespec64 tm;
    unsigned int pwk;
    unsigned int vup;
    unsigned int vdown;
    unsigned int aik;
    unsigned int shoulder;
    unsigned int side;
    unsigned int press;
    unsigned int other;
    unsigned int other1;
    spinlock_t count_lock;
} g_device_key_stats;

static long long g_keytimes_tid = 184561000011;
//USD: add for keytimes by junyu.chen2 20250825 end
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT) && IS_ENABLED(CONFIG_TRAN_FREEZE_AEGIS_SUPPORT)
void report_display_timeout_tnelog(void)
{
	print_log(LOG_LEVEL_INFO, "report pwrkey display timeout tnelog in\n");
    	//tnek_caller_api(TNE_ID, TNE_NEED_LOG_TYPE, 0, TNE_LOG_DERCRIPTION);
        startAegisK("PWK_Timeout","","");
}
#endif
void updata_monitor_timeout(const char *config, const char *key) {
    const char *field;
    int value = 1000 * 5;
	print_log(LOG_LEVEL_INFO, "updata_monitor_timeout in.\n");
    if (config == NULL || key == NULL) {
        print_log(LOG_LEVEL_INFO, "Input parameters cannot be NULL.\n");
        goto end;
    }
    field = strstr(config, key);
    if (field != NULL) {
        field += strlen(key) + 1;
        value = simple_strtol(field, NULL, 10);
    } else {
        print_log(LOG_LEVEL_INFO, "Key not found in config.\n");
    }
end:
	if(value < TIMOUE_MIN){
		value = TIMOUE_MIN;
	}
    print_log(LOG_LEVEL_INFO, "updata_monitor_timeout = %d ms\n", value);
    g_monitor_timer_out_ms = value;
}
void update_cloud_config(char *config, const char *key) {
    char buffer[256] = {0};
	bool tmp = false;
	char * tmp_config = NULL;
	tmp_config = config;
	print_log(LOG_LEVEL_INFO, "update_cloud_config in\n");
	if(tmp_config == NULL){
		print_log(LOG_LEVEL_INFO, "cloud config is null try to get\n");
		tmp_config = cloudengine_getConfig(g_cloud_id);
		if(tmp_config == NULL){
			print_log(LOG_LEVEL_ERR, "cloud config back is null update_cloud_config failed\n");
			return;
		}
	}
	print_log(LOG_LEVEL_ERR, "config= %s\n", tmp_config);
	snprintf(buffer, sizeof(buffer), "%s+true", key);
	if(strstr(tmp_config, buffer) != NULL){
		tmp = true;
	} else {
		tmp = false;
	}
	updata_monitor_timeout(tmp_config, g_timeout_key);
	print_log(LOG_LEVEL_ERR, "update_cloud_config = %s\n", tmp?"true":"false");
	is_cloud_enabled = tmp;
}
static void report_bigdata(void)
{
	print_log(LOG_LEVEL_INFO, "report_bigdata in\n");
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
	if(is_tid_enable(g_bigdata_tid)){
		struct athena_msg *msg = NULL;
		msg = alloc_msg();
		if(msg == NULL){
			print_log(LOG_LEVEL_ERR, "alloc msg failed\n");
			return;
		}
		msg_put_string(&msg, "type", "freeze");
		msg_put_string(&msg, "reason", "powerdisplay");
		msg_put_int(&msg, "count", 1);
		submit_msg(&msg, 1, g_bigdata_tid, "systemstability");
		free_msg(&msg);
		print_log(LOG_LEVEL_INFO, "bigdata report tid =%lld \n", g_bigdata_tid);
	} else {
		print_log(LOG_LEVEL_ERR, "bigdata tid is disbaled ! tid:%lld\n", g_bigdata_tid);
	}
#else
	print_log(LOG_LEVEL_INFO, "report_bigdata ng mirco = false\n");
#endif
}

//USD: add for keytimes by junyu.chen2 20250825 start
static void report_key_times(struct work_struct *work)
{
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
    //struct device_key_times key_times = g_device_key_stats;
    if(is_tid_enable(g_keytimes_tid)) {
        struct athena_msg *msg = NULL;
        struct timespec64 ts;
        msg = alloc_msg();
        if(msg == NULL) {
            print_log(LOG_LEVEL_ERR, "alloc msg failed\n");
            return;
        }
        
        ktime_get_real_ts64(&ts);  
        //print_log(LOG_LEVEL_INFO, "new time is =%lld \n", ts.tv_sec);
        msg_put_long(&msg, "tm", ts.tv_sec);  
        msg_put_int(&msg, "pwk", -1);
        msg_put_int(&msg, "vup", g_device_key_stats.vup);
        msg_put_int(&msg, "vdown", g_device_key_stats.vdown);
        msg_put_int(&msg, "aik", -1);
        msg_put_int(&msg, "shoulder", -1);
        msg_put_int(&msg, "side", -1);
        msg_put_int(&msg, "press", -1);
        msg_put_int(&msg, "other", -1);
        msg_put_int(&msg, "other1", -1);
        submit_msg(&msg, 1, g_keytimes_tid, "device_key_times");
        free_msg(&msg);

        //spin_lock(&g_device_key_stats.count_lock);
        memset(&g_device_key_stats, 0, sizeof(struct device_key_times));
        //spin_unlock(&g_device_key_stats.count_lock);
        print_log(LOG_LEVEL_INFO, "key stat report tid =%lld \n", g_keytimes_tid);
    } else {
        print_log(LOG_LEVEL_ERR, "keytimes tid is disabled! tid:%lld\n", g_keytimes_tid);
    }
#else
    print_log(LOG_LEVEL_INFO, "report_key_stat not supported\n");
#endif
}
//USD: add for keytimes by junyu.chen2 20250825 start
static void check_hourly_report(struct timer_list *timer)
{ 
    schedule_work(&report_key_times_work);
    mod_timer(timer, jiffies + msecs_to_jiffies(3600 * 1000));
}
//delaywork callback function for report bigdata and tnelog
void key_report_work_func(struct work_struct *work)
{
	//display is on do nothong
    if (g_tran_key_brightness_level > 0 || is_backlight_on_before) {
        print_log(LOG_LEVEL_INFO, "Brightness level is: %d or screen on yet .do onthing\n", g_tran_key_brightness_level);
		is_key_report_running = false;
		return;
    }
	print_log(LOG_LEVEL_INFO, "Brightness level is: %d report bigdata & tnelog\n", g_tran_key_brightness_level);
	report_bigdata();
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
	report_display_timeout_tnelog();
	is_key_report_running = false;
	return;
#endif
	print_log(LOG_LEVEL_INFO, "CONFIG_TRAN_TNEK_SUPPORT/CONFIG_TRAN_FREEZE_DETECT_SUPPORT disabled\n");
	is_key_report_running = false;
}
//work for key event
__attribute__((unused)) static void tran_custom_key_work_func(struct work_struct *work)
{
	if (is_cloud_enabled) {
        print_log(LOG_LEVEL_INFO,  "cloudengine enabled\n");
    } else {
        print_log(LOG_LEVEL_INFO,  "cloudengine disabled, do nothing\n");
		return;
    }
	if(0 == g_tran_key_brightness_level && driver_loaded ){
		if(is_key_report_running){
			print_log(LOG_LEVEL_INFO, "delaywork is running do nothing\n");
			return;
		}
		schedule_delayed_work(&key_report_work, msecs_to_jiffies(g_monitor_timer_out_ms));
		is_backlight_on_before = false;
		is_key_report_running = true;
        print_log(LOG_LEVEL_INFO, "delaywork started for %d ms\n", g_monitor_timer_out_ms);
	} else if(0 < g_tran_key_brightness_level && driver_loaded){
		print_log(LOG_LEVEL_INFO, "back light is %d do not start monitor\n", g_tran_key_brightness_level);
		if(is_key_report_running){
			cancel_delayed_work(&key_report_work);
			is_key_report_running = false;
		}
	}
}
//custom key event insert to key event
void tran_custom_key_bigdata(unsigned int hw_irq)
{
	if(!driver_loaded){
		return;
	}
    switch (hw_irq) {
    case 1:
        print_log(LOG_LEVEL_INFO, "powerkey pressed\n");
        atomic_set(&g_is_powerkey_pressed, 1);
        if (!work_busy(&tran_custom_key_work)) {
            schedule_work(&tran_custom_key_work);
        }
        break;
    case 0:
        print_log(LOG_LEVEL_INFO, "powerkey released\n");
        atomic_set(&g_is_powerkey_pressed, 0);
        break;
    default:
        break;
    }
}
//insert to where backlight changed
void lcm_updata_backlight_to_key(int brightness_level)
{
	//print_log(LOG_LEVEL_INFO, "brightness_level = %d\n", brightness_level);
	if(driver_loaded){
		g_tran_key_brightness_level = brightness_level;
		if(g_tran_key_brightness_level > 0){//lcm on
			is_backlight_on_before = true;
		}
	}
}
EXPORT_SYMBOL(lcm_updata_backlight_to_key);
static ssize_t tran_key_bigdata_proc_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
    int32_t ret;
    int32_t tmp;
    char buff[2]= {'0','\0'};
	print_log(LOG_LEVEL_INFO, "tran_key_bigdata_proc_write in\n");
	if(!driver_loaded){
		return count;
	}
    if (count == 0 || count > 2) {
        print_log(LOG_LEVEL_ERR, "Invalid value! count = %zu", count);
        ret = -EINVAL;
        goto out;
    }
    ret = copy_from_user(buff,buf,2);
    if(ret) {
        print_log(LOG_LEVEL_ERR, "copy_from_user fail.");
        return -EPERM;
    }
    ret = sscanf(buff, "%d", &tmp);
    if (ret != 1) {
        print_log(LOG_LEVEL_ERR, "Invalid value! ret = %d", ret);
        ret = -EINVAL;
        goto out;
    }
    if (tmp > 6) {
        print_log(LOG_LEVEL_ERR, "Invalid value! tmp = %d", tmp);
        ret = -EINVAL;
        goto out;
    }
	print_log(LOG_LEVEL_INFO, "wtite val = %d\n", tmp);
	if(tmp == 1){//test pwr down
		tran_custom_key_bigdata(1);
	}
	if(tmp == 2){//test pwr up
		tran_custom_key_bigdata(2);
	}
	if(tmp == 3){//test cloud function
		update_cloud_config(NULL, g_cloud_key);
		if (is_cloud_enabled) {
			print_log(LOG_LEVEL_INFO,  "cloude status = true\n");
		} else {
			print_log(LOG_LEVEL_INFO,  "cloude status = false\n");
		}
		return count;
	}
	if(tmp == 4){//test report bigdata
		report_bigdata();
	}
	if(tmp == 5){//test report tne log
#if IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
		report_display_timeout_tnelog();
		return count;
#endif
		print_log(LOG_LEVEL_ERR, "tne function macro is disabled\n");
		return count;
	}
	if(tmp == 6){//test report bigdata
		is_backlight_on_before = true;
		print_log(LOG_LEVEL_INFO, "change targ lcm has on\n");
	}
out:
    return count;
}
static ssize_t tran_key_bigdata_proc_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	print_log(LOG_LEVEL_INFO, "tran_key_bigdata_proc_read\n");
    return count;
}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0))
static const struct file_operations tran_key_bigdata_fops = {
    .owner = THIS_MODULE,
    .read   = tran_key_bigdata_proc_read,
    .write  = tran_key_bigdata_proc_write,
};
#else
__attribute__((unused)) static const struct proc_ops tran_key_bigdata_fops = {
    .proc_read   = tran_key_bigdata_proc_read,
    .proc_write  = tran_key_bigdata_proc_write,
};
#endif
static int cloudengine_notifier_block_change(struct notifier_block *nb, unsigned long action, void* data)
{
    struct cloud_engine_config* cec;
    char* id;
    char* config;
    if(NULL == data)
    {
        return -1;
    }
    cec = (struct cloud_engine_config*)data;
    id = cec->id;
    if(strcmp(g_cloud_id, id) == 0)
    {
        config = cec->config;
        update_cloud_config(config, g_cloud_key);
    }
    return 0;
}
struct notifier_block cloudengine_notifier_block = {
    .notifier_call = cloudengine_notifier_block_change,
};
static void disp_timeout_key(unsigned int code, int value)
{
	if (value) {
		if (code == KEY_POWER) {
			tran_custom_key_bigdata(1);
		}
	}else {
		if (code == KEY_POWER) {
			tran_custom_key_bigdata(0);
		}
	}
}
static void disp_timeout_event(struct input_handle *handle,
	unsigned int type, unsigned int code, int value)
{
	if (type == EV_KEY && code != BTN_TOUCH)
		disp_timeout_key(code, value);
}

//USD: add for keytimes by junyu.chen2 20250825 start
static void key_times_event(struct input_handle *handle,
    unsigned int type, unsigned int code, int value) {
	//struct device_key_times key_times = g_device_key_stats;
    if (type == EV_KEY && value) { 
        switch(code) {
            case KEY_VOLUMEUP:
		spin_lock(&g_device_key_stats.count_lock);
		g_device_key_stats.vup++;
		spin_unlock(&g_device_key_stats.count_lock);
                printk(KERN_INFO "bigdata Volume Up key pressed\n");
                break;
            case KEY_VOLUMEDOWN:
                spin_lock(&g_device_key_stats.count_lock);
		g_device_key_stats.vdown++;
		spin_unlock(&g_device_key_stats.count_lock);	
                printk(KERN_INFO "bigdata Volume Down key pressed\n");
                break;
            default:
                break;
        }
    }
}
//USD: add for keytimes by junyu.chen2 20250825 end

static int disp_timeout_connect(struct input_handler *handler,
			 struct input_dev *dev,
			 const struct input_device_id *id)
{
	struct input_handle *disp_timeout_handle;
	int error = 0;
	disp_timeout_handle = (struct input_handle *)kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!disp_timeout_handle)
		return -ENOMEM;
	disp_timeout_handle->dev = dev;
	disp_timeout_handle->handler = handler;
	disp_timeout_handle->name = "disp_timeout";
	error = input_register_handle(disp_timeout_handle);
	if (error) {
		print_log(LOG_LEVEL_ERR, "Failed to register input sysrq handler, error %d\n",error);
		goto err_free;
	}
	error = input_open_device(disp_timeout_handle);
	if (error) {
		print_log(LOG_LEVEL_ERR, "Failed to open input device, error %d\n", error);
		goto err_unregister;
	}
	return 0;
 err_unregister:
	input_unregister_handle(disp_timeout_handle);
 err_free:
	kfree(disp_timeout_handle);
	return error;
}
 static void disp_timeout_disconnect(struct input_handle *handle)
 {
	 input_close_device(handle);
	 input_unregister_handle(handle);
 }
static const struct input_device_id sysdump_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{},
};
__attribute__((unused)) static struct input_handler disp_timeout_handler = {
	.event = disp_timeout_event,
	.connect	= disp_timeout_connect,
	.disconnect	= disp_timeout_disconnect,
	.name = "disp_timeout",
	.id_table	= sysdump_ids,
};
//USD: add for keytimes by junyu.chen2 20250825 start
__attribute__((unused)) static struct input_handler key_times_handler = {
    .event = key_times_event,
    .connect = disp_timeout_connect, 
    .disconnect = disp_timeout_disconnect, 
    .name = "key_times",
    .id_table = sysdump_ids, 
};
//USD: add for keytimes by junyu.chen2 20250825 end
#endif
static int __init current_module_init(void)
{
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
	struct proc_dir_entry *proc_entry = NULL;
	register_cloudengine_notifier(g_cloud_id, "v1.0", &cloudengine_notifier_block);
	INIT_DELAYED_WORK(&key_report_work, key_report_work_func);
	INIT_WORK(&tran_custom_key_work, tran_custom_key_work_func);
	proc_entry = proc_create(MODULE_NAME, 0666, NULL, &tran_key_bigdata_fops);
	if (proc_entry == NULL) {
	print_log(LOG_LEVEL_ERR, "create border_suppression test proc entry failed");
	}
	update_cloud_config(NULL, g_cloud_key);
	print_log(LOG_LEVEL_INFO, "tran key bigdata init ok\n");
	if (input_register_handler(&disp_timeout_handler)){
		print_log(LOG_LEVEL_ERR, "regist sysdump_handler failed.\n");
	}
	//USD: add for keytimes by junyu.chen2 20250825 start
	if (input_register_handler(&key_times_handler)) {
            print_log(LOG_LEVEL_ERR, "Failed to register key_times_handler\n");
        }
	INIT_WORK(&report_key_times_work, report_key_times);
        timer_setup(&hourly_timer, check_hourly_report, 0);
        if (mod_timer(&hourly_timer, jiffies + msecs_to_jiffies(3600 * 1000)) != 0) {
    	   print_log(LOG_LEVEL_ERR, "Failed to setup hourly timer\n");
	}
	spin_lock_init(&g_device_key_stats.count_lock);
	//USD: add for keytimes by junyu.chen2 20250825 end
	driver_loaded = true;
#else
	print_log(LOG_LEVEL_INFO, "CONFIG_TRAN_FREEZE_DETECT_SUPPORT=false load empty ko\n");
#endif
	return 0;
}
static void __exit current_module_exit(void)
{
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
	driver_loaded = false;
	del_timer_sync(&hourly_timer);
        cancel_work_sync(&report_key_times_work);
	input_unregister_handler(&disp_timeout_handler);
	input_unregister_handler(&key_times_handler);
	cancel_work_sync(&tran_custom_key_work);
	cancel_delayed_work(&key_report_work);
	remove_proc_entry(MODULE_NAME, NULL);
	unregister_cloudengine_notifier(g_cloud_id, &cloudengine_notifier_block);
	print_log(LOG_LEVEL_INFO, "tran key bigdata exit ok\n");
#endif
}
module_init(current_module_init);
module_exit(current_module_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("transsion");
MODULE_DESCRIPTION("transsion key bigdata");
