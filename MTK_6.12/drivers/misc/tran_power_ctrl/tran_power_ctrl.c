/*
 * Copyright (c) 2024 Transsion Inc
 * Author jian.ding
 * Add tran_power_ctrl module in kernel
 */
#include "tran_power_info.h"
#include "tran_power_ctrl.h"
//USD:modify by zhaoren.he for [modem sleep]20250528 start
#ifdef CONFIG_TRAN_ODPM_MD_SWITCH
#include "../perf_log/tran_perf_util.h"
#endif
//USD:modify by zhaoren.he for [modem sleep]20250528 end

#define tran_power_attr(_name) static struct device_attribute dev_attr_##_name = __ATTR(_name, 0644, _name##_show, _name##_store)

struct kobject *tran_power_kobj = NULL;
struct platform_device *tran_power_pdev = NULL;

static atomic_t debug_enabled = ATOMIC_INIT(0); // Default to disabled (DEBUG_EVENT_LOG_DISABLE)
static atomic_long_t total_26M_time = ATOMIC_INIT(0); // Total 26M time counter
static atomic_long_t total_suspend_time = ATOMIC_INIT(0); // Total suspend time counter

//atomic_long_add(time_suspend, &total_suspend_time);
//atomic_long_add(time_26M, &total_26M_time);

static struct resource tran_power_resources[] = {
    [0] = {
	.name   = "tran_power_resource",
	.start  = 0,
	.end    = 0,
	.flags  = IORESOURCE_MEM,
    },
};

static void *tran_power_platform_data = NULL;

static struct block_system_info block_system[] = {
    [0] = {"md", ATOMIC_INIT(0)},
    [1] = {"conn", ATOMIC_INIT(0)},
    [2] = {"disp", ATOMIC_INIT(0)},
    [3] = {"scp", ATOMIC_INIT(0)},
    [4] = {"adsp", ATOMIC_INIT(0)},
    [5] = {"ufs", ATOMIC_INIT(0)},
    [6] = {"msdc", ATOMIC_INIT(0)},
    [7] = {"apu", ATOMIC_INIT(0)},
    [8] = {"gce", ATOMIC_INIT(0)},
    [9] = {"uarthub", ATOMIC_INIT(0)},
    [10] = {"pcie", ATOMIC_INIT(0)},
    [11] = {"srclkeni", ATOMIC_INIT(0)},
    [12] = {"spm", ATOMIC_INIT(0)},
    [13] = {"UNKNOWN", ATOMIC_INIT(0)},

};

static struct wakeup_source_info wakeup_source[] = {
    [0] = {" R12_PCM_TIMER", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [1] = {" R12_TWAM_PMSR_DVFSRC_IRQ", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [2] = {" R12_KP_IRQ_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [3] = {" R12_APWDT_EVENT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [4] = {" R12_APXGPT1_EVENT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [5] = {" R12_CONN2AP_SPM_WAKEUP_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [6] = {" R12_EINT_EVENT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [7] = {" R12_CONN_WDT_IRQ_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [8] = {" R12_CCIF0_EVENT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [9] = {" R12_CCIF1_EVENT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [10] = {" R12_SC_SSPM2SPM_WAKEUP_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [11] = {" R12_SC_SCP2SPM_WAKEUP_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [12] = {" R12_RESERVED_BIT", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [13] = {" R12_PCM_WDT_WAKEUP_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [14] = {" R12_USB_CDSC_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [15] = {" R12_USB_POWERDWN_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [16] = {" R12_UART2SPM_IRQ_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [17] = {" R12_SYSTIMER_EVENT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [18] = {" R12_EINT_EVENT_SECURED", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [19] = {" R12_AFE_IRQ_MCU_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [20] = {" R12_THERM_CTRL_EVENT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [21] = {" R12_SYS_CIRQ_IRQ_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [22] = {" R12_MD2AP_PEER_EVENT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [23] = {" R12_CSYSPWREQ_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [24] = {" R12_MD1_WDT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [25] = {" R12_AP2AP_PEER_WAKEUPEVENT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [26] = {" R12_SEJ_EVENT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [27] = {" R12_SPM_CPU_WAKEUPEVENT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [28] = {" R12_APUSYS_WAKE_HOST", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [29] = {" R12_MSDC_WAKEUPEVENT_B", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [30] = {" R12_SYSTIMER", ATOMIC_INIT(0), ATOMIC_INIT(0)},
    [31] = {" UNKNOWN", ATOMIC_INIT(0), ATOMIC_INIT(0)},
};

static struct idle_status_info idle_status[] = {
    [0] = {"cpuoff_l", ATOMIC_INIT(0)},
    [1] = {"cpuoff_m", ATOMIC_INIT(0)},
    [2] = {"cpuoff_b", ATOMIC_INIT(0)},
    [3] = {"cpu_off", ATOMIC_INIT(0)},
    [4] = {"clusteroff_l", ATOMIC_INIT(0)},
    [5] = {"clusteroff_m", ATOMIC_INIT(0)},
    [6] = {"clusteroff_b", ATOMIC_INIT(0)},
    [7] = {"cluster_off", ATOMIC_INIT(0)},
    [8] = {"mcusysoff_l", ATOMIC_INIT(0)},
    [9] = {"mcusysoff_m", ATOMIC_INIT(0)},
    [10] = {"mcusysoff_b", ATOMIC_INIT(0)},
    [11] = {"mcusys_off", ATOMIC_INIT(0)},
    [12] = {"system_mem", ATOMIC_INIT(0)},
    [13] = {"system_pll", ATOMIC_INIT(0)},
    [14] = {"system_bus", ATOMIC_INIT(0)},
    [15] = {"system_vcore", ATOMIC_INIT(0)},
    [16] = {"s2idle", ATOMIC_INIT(0)},
    [17] = {"suspend", ATOMIC_INIT(0)},
    [18] = {"UNKNOWN", ATOMIC_INIT(0)},
};

enum debug_type {
    DEBUG_EVENT_LOG_DISABLE = 0,
    DEBUG_LOG_EVENT_ENABLE = 1,
    DEBUG_LOG_ENABLE = 2,
    DEBUG_EVENT_ENABLE = 3
};

static ssize_t tran_power_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf,"0:disable log&event,1:enable event,2:enable log,3:enable event,now value= %d\n", atomic_read(&debug_enabled));
}
static ssize_t tran_power_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
    int value;
    if (!buf || n == 0 || !*buf || kstrtoint(buf, 10, &value) < 0) {
        pr_err("Invalid value for debug_enabled\n");
        return -EINVAL;
    }

    if (value >= DEBUG_EVENT_LOG_DISABLE && value <= DEBUG_EVENT_ENABLE) {
        atomic_set(&debug_enabled, value);
        pr_info("debug_enabled set to %d\n", value);
        return n;
    }

    pr_err("debug_enabled must be 0 or 1 or 2 or 3\n");
    return -EINVAL;
}
tran_power_attr(tran_power_debug);

static ssize_t tran_26M_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "total_26M_time=%lu\ntotal_suspend_time=%lu\n", atomic_long_read(&total_26M_time), atomic_long_read(&total_suspend_time));
}

static ssize_t tran_26M_time_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
    // Only support reset function for 26M_time and suspend_time counters
    size_t len = n;
    if (len > 0 && buf[len - 1] == '\n')
        len--;

    if (len == strlen("reset") && strncmp(buf, "reset", len) == 0) {
        atomic_long_set(&total_26M_time, 0);
        atomic_long_set(&total_suspend_time, 0);
        pr_info("26M_time and suspend_time reset to 0\n");
        return n;
    } else {
        pr_err("Invalid value for 26M_time, only 'reset' is supported\n");
        return -EINVAL;
    }
}

tran_power_attr(tran_26M_time);

/**
 * tran_suspend_info_increment - Increment the count of the specified wakeup source
 * @idle:  idle status name
 * @wakeup: wakeup source name
 * @time_suspend: suspend time
 * @time_26M: 26M time
 *
 * This function can be called when an external module needs to count the number of times
 * a specific idle status and wakeup source have been triggered.
 * If a matching idle status and wakeup source name is found, its count is incremented by 1
 * and debug information is output.
 * If no match is found, the corresponding UNKNOWN item's count is incremented.
 */
void tran_suspend_info_increment(const char *idle, const char *wakeup, const int time_suspend, const int time_26M)
{

    int i, ret = -1;
    bool wakeup_found = false;
    bool idle_found = false;
    char event_buf[256] = {0};
    char time_26M_buf[64] = {0};
    char time_suspend_buf[64] = {0};
    char *envp[4] = {event_buf, time_26M_buf, time_suspend_buf, NULL};

    if (!idle || !wakeup || time_suspend < 0 || time_26M < 0) {
        pr_err("Invalid parameters: idle=%p, wakeup=%p, time_suspend=%d, time_26M=%d\n", 
               idle, wakeup, time_suspend, time_26M);
        return;
    }

    for (i = 0; i < ARRAY_SIZE(wakeup_source); i++) {
        if (strcmp(wakeup_source[i].name, wakeup) == 0) {
            if (!strcmp(idle, "suspend")) {
                atomic_inc(&wakeup_source[i].suspend_count);
                // Use atomic_long_add_return to avoid race condition
                unsigned long old_suspend_total = atomic_long_read(&total_suspend_time);
                unsigned long new_suspend_total = atomic_long_add_return(time_suspend, &total_suspend_time);
                if (new_suspend_total < old_suspend_total) {
                    pr_warn("total_suspend_time overflow detected, wrapping around\n");
                }
                // Use atomic_long_add_return to avoid race condition
                unsigned long old_26M_total = atomic_long_read(&total_26M_time);
                unsigned long new_26M_total = atomic_long_add_return(time_26M, &total_26M_time);
                if (new_26M_total < old_26M_total) {
                    pr_warn("total_26M_time overflow detected, wrapping around\n");
                }

                if (tran_power_pdev) {
                    snprintf(event_buf, sizeof(event_buf), "wakeup_source=%s", wakeup);
                    snprintf(time_suspend_buf, sizeof(time_suspend_buf), "time_suspend=%d", time_suspend);
                    snprintf(time_26M_buf, sizeof(time_26M_buf), "time_26M=%d", time_26M);
                    if (atomic_read(&debug_enabled) == DEBUG_EVENT_ENABLE || atomic_read(&debug_enabled) == DEBUG_LOG_EVENT_ENABLE) {
                        ret = kobject_uevent_env(&tran_power_pdev->dev.kobj, KOBJ_CHANGE, envp);
                        if (ret < 0)
                            pr_err("Failed to send wakeup source '%s' suspend uevent, ret=%d\n", wakeup, ret);
                    }
                }
            }
            else
                atomic_inc(&wakeup_source[i].count);
            if (atomic_read(&debug_enabled) == DEBUG_LOG_ENABLE || atomic_read(&debug_enabled) == DEBUG_LOG_EVENT_ENABLE)
                pr_info("Wakeup source '%s' %s count incremented\n", wakeup, idle);
            wakeup_found = true;
            break;
        }
    }

    if (!wakeup_found) {
        if (!strcmp(idle, "suspend")) {
            atomic_inc(&wakeup_source[WAKEUP_SOURCE_UNKNOWN_IDX].suspend_count);
            if (tran_power_pdev) {
                snprintf(event_buf, sizeof(event_buf), "wakeup_source=%s", wakeup);
                snprintf(time_suspend_buf, sizeof(time_suspend_buf), "time_suspend=%d", time_suspend);
                snprintf(time_26M_buf, sizeof(time_26M_buf), "time_26M=%d", time_26M);
                if (atomic_read(&debug_enabled) == DEBUG_EVENT_ENABLE || atomic_read(&debug_enabled) == DEBUG_LOG_EVENT_ENABLE) {
                    ret = kobject_uevent_env(&tran_power_pdev->dev.kobj, KOBJ_CHANGE, envp);
                    if (ret < 0)
                        pr_err("Failed to send wakeup source '%s' suspend uevent(UNKNOWN), ret=%d\n", wakeup, ret);
                }
            }
        }
        else {
            atomic_inc(&wakeup_source[WAKEUP_SOURCE_UNKNOWN_IDX].count);
        }
        if (atomic_read(&debug_enabled) == DEBUG_LOG_ENABLE || atomic_read(&debug_enabled) == DEBUG_LOG_EVENT_ENABLE)
            pr_info("Wakeup source '%s' suspend count incremented(UNKNOWN)\n", wakeup);
    }

    for (i = 0; i < ARRAY_SIZE(idle_status); i++) {
        if (strcmp(idle_status[i].name, idle) == 0) {
            atomic_inc(&idle_status[i].count);
            if (atomic_read(&debug_enabled) == DEBUG_LOG_ENABLE || atomic_read(&debug_enabled) == DEBUG_LOG_EVENT_ENABLE)
                pr_info("Idle status '%s' count incremented to %d\n", idle, atomic_read(&idle_status[i].count));
            idle_found = true;
            break;
        }
    }

    if (!idle_found) {
        atomic_inc(&idle_status[IDLE_STATUS_UNKNOWN_IDX].count);
        if (atomic_read(&debug_enabled) == DEBUG_LOG_ENABLE || atomic_read(&debug_enabled) == DEBUG_LOG_EVENT_ENABLE)
            pr_info("Idle status '%s' count incremented(UNKNOWN)\n", idle);
    }

    return;
}
EXPORT_SYMBOL_GPL(tran_suspend_info_increment);

/**
 * tran_block_system_count_increment - Increment the count of the specified block system module
 * @name: block system module name
 *
 * When an external module needs to count the number of times a specific block system module
 * has been triggered, it can call this function.
 * If a matching block system module name is found, its count is incremented by 1 and debug
 * information is output.
 */
void tran_block_system_count_increment(const char *name)
{
    int i, ret = -1;
    char event_buf[128] = {0};
    char *envp[2] = {event_buf, NULL};

    if (!name) {
        pr_err("Invalid parameter: name is NULL\n");
        return;
    }

    for (i = 0; i < ARRAY_SIZE(block_system); i++) {
        if (strcmp(block_system[i].name, name) == 0) {
            atomic_inc(&block_system[i].count);
            if (atomic_read(&debug_enabled) == DEBUG_LOG_ENABLE || atomic_read(&debug_enabled) == DEBUG_LOG_EVENT_ENABLE)
                pr_info("Block system '%s' count incremented to %d\n", name, atomic_read(&block_system[i].count));
            if (tran_power_pdev) {
                snprintf(event_buf, sizeof(event_buf), "block_subsys=%s", name);
                if (atomic_read(&debug_enabled) == DEBUG_LOG_EVENT_ENABLE || atomic_read(&debug_enabled) == DEBUG_EVENT_ENABLE) {
                    ret = kobject_uevent_env(&tran_power_pdev->dev.kobj, KOBJ_CHANGE, envp);
                    if (ret < 0)
                        pr_err("Failed to send block system '%s' uevent, ret=%d\n", name, ret);
                }
            }
            return;
        }
    }

    atomic_inc(&block_system[BLOCK_SYSTEM_UNKNOWN_IDX].count);
    if (atomic_read(&debug_enabled) == DEBUG_LOG_ENABLE || atomic_read(&debug_enabled) == DEBUG_LOG_EVENT_ENABLE)
        pr_info("Block system '%s' count incremented to %d (UNKNOWN)\n", name, atomic_read(&block_system[BLOCK_SYSTEM_UNKNOWN_IDX].count));
    if (tran_power_pdev) {
        snprintf(event_buf, sizeof(event_buf), "block_subsys=%s", name);
        if (atomic_read(&debug_enabled) == DEBUG_EVENT_ENABLE || atomic_read(&debug_enabled) == DEBUG_LOG_EVENT_ENABLE) {
            ret = kobject_uevent_env(&tran_power_pdev->dev.kobj, KOBJ_CHANGE, envp);
            if (ret < 0)
                pr_err("Failed to send block system '%s' uevent(UNKNOWN), ret=%d\n", name, ret);
        }
    }

    return;
}
EXPORT_SYMBOL_GPL(tran_block_system_count_increment);

//USD:modify by zhaoren.he for [modem sleep]20250528 start
#ifdef CONFIG_TRAN_ODPM_MD_SWITCH
static ssize_t tran_modem_sleep_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int i;
    ssize_t len = 0;
    unsigned long long total_sleep_time = 0;

    for (i = 0; i < DAYS_TO_TRACK; i++) {
        total_sleep_time += sleep_history[i].md_sleep_time / 1000000 +
        sleep_history[i].gsm_sleep_time / 1000000 +
        sleep_history[i].wcdma_sleep_time / 1000000 +
        sleep_history[i].lte_sleep_time / 1000000 +
        sleep_history[i].nr_sleep_time / 1000000;
    }
    len += snprintf(buf + len,
    PAGE_SIZE - len, "Total: %llu seconds\n", total_sleep_time);
    for (i = 0; i < DAYS_TO_TRACK; i++) {
        if (sleep_history[i].year == 0)
            continue;

        len += snprintf(buf + len, PAGE_SIZE - len,
            "%02d/%02d: SUM MD/2G/3G/4G/5G_FR1 = %6llu/%6llu/%6llu/%6llu/%6llu seconds\n",
            sleep_history[i].month,
            sleep_history[i].day,
            sleep_history[i].md_sleep_time / 1000000,
            sleep_history[i].gsm_sleep_time / 1000000,
            sleep_history[i].wcdma_sleep_time / 1000000,
            sleep_history[i].lte_sleep_time / 1000000,
            sleep_history[i].nr_sleep_time / 1000000);

        if (len >= PAGE_SIZE - 1)
            break;
    }

    return len;
}

static ssize_t tran_modem_sleep_time_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
    return n;
}

tran_power_attr(tran_modem_sleep_time);
#endif
//USD:modify by zhaoren.he for [modem sleep]20250528 end


static ssize_t tran_suspend_info_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
    if (!buf || n == 0)
        return -EINVAL;
    size_t len = n;
    if (len > 0 && buf[len - 1] == '\n')
        len--;

    if (len == strlen("reset") && strncmp(buf, "reset", len) == 0) {
        int i;
        for (i = 0; i < ARRAY_SIZE(wakeup_source); i++) {
            atomic_set(&wakeup_source[i].count, 0);
            atomic_set(&wakeup_source[i].suspend_count, 0);
        }

        for (i = 0; i < ARRAY_SIZE(idle_status); i++) {
            atomic_set(&idle_status[i].count, 0);
        }
        pr_info("All wakeup source and idle status counters reset successfully.\n");
        return n;
    }
    pr_info("Invalid input. Please enter 'reset' to reset all counters.\n");

    return -EINVAL;
}

static ssize_t tran_suspend_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int i;
    ssize_t len = 0;
    len+=snprintf(buf + len, PAGE_SIZE - len, "\nWakeupSource:\t\tNo_Suspend\tSuspend\n");

    for (i = 0; i < ARRAY_SIZE(wakeup_source); i++) {
        if (atomic_read(&wakeup_source[i].count) == 0 && atomic_read(&wakeup_source[i].suspend_count) == 0)
            continue;

        len += snprintf(buf + len, PAGE_SIZE - len, "%s:\t\t%d\t%d\n", wakeup_source[i].name, atomic_read(&wakeup_source[i].count), atomic_read(&wakeup_source[i].suspend_count));

    }

    len += snprintf(buf + len, PAGE_SIZE - len, "\n\nIdleStatus:\tCount\n");

    sort(idle_status, ARRAY_SIZE(idle_status), sizeof(struct idle_status_info), idle_status_cmp, NULL);

    for (i = 0; i < ARRAY_SIZE(idle_status); i++) {
        if (atomic_read(&idle_status[i].count) == 0)
            continue;

        len += snprintf(buf + len, PAGE_SIZE - len, "%s:\t%d\n", idle_status[i].name, atomic_read(&idle_status[i].count));
    }

    len += snprintf(buf + len, PAGE_SIZE - len, "\nEnter 'reset' to reset all counters.\n");
    return len;
}

tran_power_attr(tran_suspend_info);

static ssize_t tran_block_system_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
    // Remove trailing newline character (if any)
    size_t len = n;
    if (len > 0 && buf[len - 1] == '\n')
        len--;

    // Check if it's a reset command
    if (len == strlen("reset") && strncmp(buf, "reset", len) == 0) {
        // Reset all block system module counts
        int i;
        for (i = 0; i < ARRAY_SIZE(block_system); i++) {
            atomic_set(&block_system[i].count, 0);
        }
        pr_info("All block system counters reset successfully.\n");
        return n; // Return the number of bytes processed
    }

    // Input does not match any block system module name
    pr_info("Invalid input. Please enter 'reset' to reset all counters.\n");

    return -EINVAL; // Return error code
}

static ssize_t tran_block_system_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int i;
    ssize_t len = 0;

    // Sort block_system array by count in descending order
    sort(block_system, ARRAY_SIZE(block_system), sizeof(struct block_system_info), block_system_cmp, NULL);
    len += snprintf(buf + len, PAGE_SIZE - len, "Block System:\tCount\n");

    // Display all block system modules and their counts
    for (i = 0; i < ARRAY_SIZE(block_system); i++) {
        if (atomic_read(&block_system[i].count) == 0)
            continue;
        len += snprintf(buf + len, PAGE_SIZE - len, "%-20s: %d\n", block_system[i].name, atomic_read(&block_system[i].count));
    }
    // Add reset instructions
    len += snprintf(buf + len, PAGE_SIZE - len, "\nEnter 'reset' to reset all counters.\n");

    return len;
}
tran_power_attr(tran_block_system);

static int tran_power_ctrl_probe(struct platform_device *pdev)
{
    int err = 0;

    pr_info("tran_power_device probe\n");
    err = device_create_file(&pdev->dev, &dev_attr_tran_power_debug);
    if (err) {
        pr_err("Failed to create tran_power_debug attribute\n");
        goto cleanup_debug;
    }

#ifdef CONFIG_TRAN_ODPM_MD_SWITCH
    err = device_create_file(&pdev->dev, &dev_attr_tran_modem_sleep_time);
    if (err) {
        pr_err("Failed to create tran_modem_sleep_time attribute\n");
        goto cleanup_modem;
    }
#endif

    err = device_create_file(&pdev->dev, &dev_attr_tran_block_system);
    if (err) {
        pr_err("Failed to create tran_block_system attribute\n");
        goto cleanup_block;
    }

    err = device_create_file(&pdev->dev, &dev_attr_tran_suspend_info);
    if (err) {
        pr_err("Failed to create tran_suspend_info attribute\n");
        goto cleanup_suspend;
    }

    err = device_create_file(&pdev->dev, &dev_attr_tran_26M_time);
    if (err) {
        pr_err("Failed to create tran_26M_time attribute\n");
        goto cleanup_26M_time;
    }

    pr_info("tran_power_device initialized successfully\n");
    return 0;

cleanup_26M_time:
    device_remove_file(&pdev->dev, &dev_attr_tran_26M_time);
cleanup_suspend:
    device_remove_file(&pdev->dev, &dev_attr_tran_suspend_info);
cleanup_block:
    device_remove_file(&pdev->dev, &dev_attr_tran_block_system);
#ifdef CONFIG_TRAN_ODPM_MD_SWITCH
cleanup_modem:
    device_remove_file(&pdev->dev, &dev_attr_tran_modem_sleep_time);
#endif
cleanup_debug:
    device_remove_file(&pdev->dev, &dev_attr_tran_power_debug);
    return err;
}

static void tran_power_ctrl_remove(struct platform_device *pdev)
{
    pr_info("tran_power_device remove\n");

    device_remove_file(&pdev->dev, &dev_attr_tran_suspend_info);
    device_remove_file(&pdev->dev, &dev_attr_tran_block_system);
    device_remove_file(&pdev->dev, &dev_attr_tran_26M_time);
#ifdef CONFIG_TRAN_ODPM_MD_SWITCH
    device_remove_file(&pdev->dev, &dev_attr_tran_modem_sleep_time);
#endif
    device_remove_file(&pdev->dev, &dev_attr_tran_power_debug);
}

static struct platform_driver tran_power_driver = {
    .driver = {
        .name   = "tran_power",
        .owner  = THIS_MODULE,
    },
    .probe  = tran_power_ctrl_probe,
    .remove = tran_power_ctrl_remove,
};

static int __init tran_power_ctrl_init(void)
{
    int err = 0;

    pr_info("tran_power_ctrl_init\n");

    err = platform_driver_register(&tran_power_driver);
    if (err) {
        pr_err("Failed to register platform driver: %d\n", err);
        return err;
    }

    tran_power_pdev = platform_device_alloc("tran_power", -1);
    if (!tran_power_pdev) {
        pr_err("Failed to allocate platform device\n");
        err = -ENOMEM;
        goto unregister_driver;
    }

    err = platform_device_add_resources(tran_power_pdev, tran_power_resources,
        ARRAY_SIZE(tran_power_resources));
    if (err) {
        pr_err("Failed to add resources to platform device: %d\n", err);
        goto put_device;
    }

    platform_device_add_data(tran_power_pdev, tran_power_platform_data,
        sizeof(tran_power_platform_data));

    err = platform_device_add(tran_power_pdev);
    if (err) {
        pr_err("Failed to add platform device: %d\n", err);
        goto put_device;
    }

    pr_info("tran_power platform device and driver registered successfully\n");
    return 0;

put_device:
    platform_device_put(tran_power_pdev);
    tran_power_pdev = NULL;
unregister_driver:
    platform_driver_unregister(&tran_power_driver);
    return err;
}

static void __exit tran_power_ctrl_exit(void)
{
    pr_info("tran_power_ctrl_exit\n");

    if (tran_power_pdev) {
        platform_device_unregister(tran_power_pdev);
        tran_power_pdev = NULL;
    }

    platform_driver_unregister(&tran_power_driver);
}

module_init(tran_power_ctrl_init);
module_exit(tran_power_ctrl_exit);
MODULE_LICENSE("GPL");
