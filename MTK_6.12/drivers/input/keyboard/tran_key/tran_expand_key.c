/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 TRANSSION Inc.
 * Author mingzhu.wang
 * Transsion device stability message manager on 20250428
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
//USD: [stability] add for manual trigger hang by mingzhu.wang 20250509 start
#define HANG_TRIGGER_TIME	10
static struct hrtimer manual_hang_timer;
static volatile bool timer_started = false;
atomic_t volup_pressed = ATOMIC_INIT(0);
atomic_t voldown_pressed = ATOMIC_INIT(0);
static bool double_key_10s_done = false;
static bool double_key_realse_under_6s = false;
static unsigned long after_double_key_start,after_double_key_end;
static unsigned long up_press_times;
extern bool Mannual_hang;
extern struct wait_queue_head manual_two_key_hang;
static inline void update_key_state(void)
{
       if (!manual_hang_timer.function)
               return;

	pr_info("%s:%d keys state: volup_pressed(%d), voldown_pressed(%d)\n",
			__func__,__LINE__,atomic_read(&volup_pressed), atomic_read(&voldown_pressed));
	if (double_key_10s_done) {
		if (!atomic_read(&volup_pressed) && !atomic_read(&voldown_pressed)) {
			//duration_double_key = jiffies-after_double_key_start;
			//pr_info("keys state: duration_double_key %d s\n",duration_double_key / HZ);
			if (time_before_eq(jiffies,after_double_key_start+HZ*6)) {//you have to released under 6s
				double_key_realse_under_6s = true;
				after_double_key_end = jiffies;
			}
			double_key_10s_done=0;
		}
	}
		if (double_key_realse_under_6s) {
			if (atomic_read(&volup_pressed) && !atomic_read(&voldown_pressed)) {
				up_press_times++;
			}
			pr_info("keys state: (jiffies -after_double_key_end )/HZ = %lu ,up_press_times = (%lu) \n ",(jiffies -after_double_key_end )/HZ,up_press_times);
			if (up_press_times == 3 &&(time_before_eq(jiffies,after_double_key_end+5*HZ))) {
				//wakeup_result=wake_up_process(mhd_thread);
				Mannual_hang = true;
				wake_up_interruptible(&manual_two_key_hang);
				pr_info("keys state: success\n");
				double_key_realse_under_6s = false;
				up_press_times= 0;
			}
			else if (atomic_read(&voldown_pressed) || time_after(jiffies,after_double_key_end+5*HZ)) {
				double_key_realse_under_6s = false;
				up_press_times=0;
				pr_info("keys state: overtime or down pressed\n");
			}
		}
	if (atomic_read(&volup_pressed) && atomic_read(&voldown_pressed)) {
		timer_started = true;
		hrtimer_start(&manual_hang_timer, ktime_set(HANG_TRIGGER_TIME, 0), HRTIMER_MODE_REL);
		pr_info("manual_hang_timer started\n");
	} else {
		if (timer_started) {
			hrtimer_cancel(&manual_hang_timer);
			timer_started = false;
			pr_info("manual_hang_timer canceled\n");
		}
	}
}
void manual_hang_process(u32 keycode, u16 pressed)
{
#if 0
	extern char *saved_command_line;
	if ((get_boot_mode() == RECOVERY_BOOT) || (strstr(saved_command_line, "tran_ota=1"))) {
		return;
	}
#endif
	printk("%s:%d define KEY_VOLUMEUP=%d,KEY_VOLUMEDOWN=%d\n",__func__,__LINE__,KEY_VOLUMEUP,KEY_VOLUMEDOWN);
	printk("%s:%d hang %d -%d\n ",__func__,__LINE__,pressed,keycode);
	if (pressed) {
		if (keycode == KEY_VOLUMEUP) {
			atomic_set(&volup_pressed, 1);
		}
		else if (keycode == KEY_VOLUMEDOWN)
			atomic_set(&voldown_pressed, 1);
		else
			return;
	} else {
		if (keycode == KEY_VOLUMEUP)
			atomic_set(&volup_pressed, 0);
		else if (keycode == KEY_VOLUMEDOWN)
			atomic_set(&voldown_pressed, 0);
		else
			return;
	  }
	update_key_state();
}
EXPORT_SYMBOL_GPL(manual_hang_process);
void manual_hang_process_volumedown_mt6xxx(unsigned int irq)
{
	printk("sean down %d",irq);

	switch(irq) {
	case MT6363_IRQ_HOMEKEY:
		atomic_set(&voldown_pressed, 1);
		break;
	case MT6363_IRQ_HOMEKEY_R:
		atomic_set(&voldown_pressed, 0);
		break;
	/*6858 volup*/
	case MT6363_IRQ_HOMEKEY_2:
		atomic_set(&volup_pressed, 1);
		break;
	case MT6363_IRQ_HOMEKEY_2_R:
		atomic_set(&volup_pressed, 0);
		break;
	default:
		return;
	}

	update_key_state();
}
EXPORT_SYMBOL_GPL(manual_hang_process_volumedown_mt6xxx);
static enum hrtimer_restart manual_hang_timer_func(struct hrtimer *timer)
{
	//struct task_struct *task,*p;
	pr_info("[sean]transsion manual dump, volup & voldown pressed !\n");
	pr_info("Hang_Detect trigger_hangdb by developer\n");
	double_key_10s_done = true;
	after_double_key_start = jiffies;
	return HRTIMER_NORESTART;//not reset hrtimer
}

static int __init tran_manual_hang_init(void)
{
       pr_info("%s start\n", __func__);
       hrtimer_init(&manual_hang_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
       manual_hang_timer.function = manual_hang_timer_func;
       return 0;
}

static void __exit tran_manual_hang_exit(void)
{
       pr_info("%s exit\n", __func__);
       hrtimer_cancel(&manual_hang_timer);
       manual_hang_timer.function = NULL;
}

module_init(tran_manual_hang_init);
module_exit(tran_manual_hang_exit);

MODULE_LICENSE("GPL");
//USD: [stability] add for manual trigger hang by mingzhu.wang 20250509 end
