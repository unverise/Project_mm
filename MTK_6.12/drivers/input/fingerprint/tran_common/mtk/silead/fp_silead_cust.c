#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
//#include <teei_fp.h>

#define CONFIG_SPI_MT65XX_KERNEL510 y

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef CONFIG_MTK_CLKMGR
#include "mach/mt_clkmgr.h"
#else
#include <linux/clk.h>
#endif

#include <net/sock.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

/* MTK header */
#ifndef CONFIG_SPI_MT65XX_KERNEL510
#include "mt_spi.h"
#include "mt_spi_hal.h"
#else
#include "tran_spi.h"
#endif

/* there is no this file on standardized GPIO platform */
#ifdef CONFIG_MTK_GPIO
#include "mtk_gpio.h"
#include "mach/gpio_const.h"
#endif

//#include <mt-plat/sync_write.h>
#include <linux/of_address.h>

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifdef CONFIG_SPI_MT65XX_KERNEL510
#include <linux/platform_data/spi-mt65xx.h>
#endif
#endif

#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include "silead_fp.h"

#define FP_DEV_VERSION "v0.3.0"
#define SPI_SYNC_RTY_CNT 5

struct fp_dev_init_t silfp_dev_init_d = {
    .mode = 0,
    .bits = 8,
    .speed = 8*1000*1000,
    .delay = 100,
    .dev = DEVICE,
    .nl_id = SIFP_NETLINK_ROUTE,
    .dev_id = 0,
    .ta = { 0x51, 0x1E, 0xAD, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    .reg = 0x1100a000,
};

char vendor_name[PROC_VND_ID_LEN];
fp_debug_level_t sil_debug_level = DBG_LOG;

typedef struct _key_map {
    int key_orig;
    int key_new;
} nav_keymap_t;

static nav_keymap_t keymap[] = {
    { NAV_KEY_UP,       KEY_UP,         }, /* KEY_RESERVED, ignore this key */
    { NAV_KEY_DOWN,     KEY_DOWN,       },
    { NAV_KEY_RIGHT,    KEY_RIGHT,      },
    { NAV_KEY_LEFT,     KEY_LEFT,       },
    { NAV_KEY_CLICK,    KEY_F28,   },
    { NAV_KEY_DCLICK,   KEY_F28,   },
    { NAV_KEY_LONGPRESS,KEY_F28,   },
};

int sif_spi_transfer(unsigned char *chip_id_str);
extern struct tran_fp_data * g_tran_fp_datap;

extern int tran_fp_spi_transfer(void *chip_conf, int cfg_len, const char *txbuf, char *rxbuf, int len, const char *txbuf1, char *rxbuf1, int len1);
extern void tran_netlink_send(struct tran_fp_data *tran_fp_datap, const int cmd);

static int silfp_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%s\n", vendor_name);
    return 0;
}

static int silfp_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, silfp_proc_show, NULL);
}

static const struct proc_ops silfp_proc_fops = {
    .proc_read  = seq_read,
    .proc_open  = silfp_proc_open,
    .proc_release = single_release,
};

#ifdef CONFIG_TRAN_FP_DRV_MODE_SPI
#ifndef CONFIG_SPI_MT65XX_KERNEL510
extern struct mt_chip_conf tran_chip_config;
#else
extern struct mtk_chip_config tran_chip_config;
#endif
#endif

static int slf_spi_read_bytes_ree(struct tran_fp_data *slf_dev, u16 addr, u32 data_len, u8 *rx_buf);

int slf_write_reg(void *chip_conf, uint32_t reg, uint32_t val)
{
    int i = 0;
    int ret;
    struct spi_message msg;
    uint8_t tx[] = {
        (uint8_t)(reg & 0xFF), 0xFF,
        (uint8_t)( val & 0xFF),
        (uint8_t)((val >> 8 ) & 0xFF),
        (uint8_t)((val >> 16) & 0xFF),
        (uint8_t)((val >> 24) & 0xFF)
    };

    struct spi_transfer tr = {
        .tx_buf = tx,
        .rx_buf = NULL, //rx,
        .len = ARRAY_SIZE(tx),
        //.delay_usecs = spi.delay,
        .speed_hz = 5*1000*1000,
        .bits_per_word = 8,
    };
	g_tran_fp_datap->tran_fp_dev->controller_data = chip_conf;
    spi_message_init(&msg);
    spi_message_add_tail(&tr, &msg);

    for (i = 0; i < SPI_SYNC_RTY_CNT; i++) {
        ret = spi_sync(g_tran_fp_datap->tran_fp_dev, &msg);
        if (ret < 0) {            
            TRAN_FP_ERROR("%s, spi_sync fail, retry=%d,result=%d\n", __func__, i, ret);
            continue;
        } else {
            TRAN_FP_ERROR("%s, spi_sync pass, retry=%d,result=%d\n", __func__, i, ret);
            return ARRAY_SIZE(tx);
        }
    }
    return ret;
}


int sif_spi_transfer(unsigned char *chip_id_str)
{
	int status = -EFAULT;
	(void) status;
	unsigned char rx_test[7] = {0};
	g_tran_fp_datap->reset(g_tran_fp_datap, 5000, 3000);
	status = slf_spi_read_bytes_ree(g_tran_fp_datap, 0x00FC, 4, rx_test);
    	TRAN_FP_INFO("%s rx_test fp id:%#2x %#2x %#2x %#2x \n", __func__,rx_test[0],rx_test[1],rx_test[2],rx_test[3]);

	memcpy(chip_id_str, rx_test, 4);
	return 0;
}

static int slf_spi_read_bytes_ree(struct tran_fp_data *slf_dev, u16 addr, u32 data_len, u8 *rx_buf)
{
	 int cfg_len;
    	 uint8_t tx[] = {addr & 0xFF, 0, 0, 0, 0, 0, 0};
 	 cfg_len = sizeof(struct mt_chip_conf);
	 tran_fp_spi_transfer(&tran_chip_config, cfg_len, tx, tx, ARRAY_SIZE(tx), NULL, NULL, 0);
	 memcpy(rx_buf, tx + 2, data_len);
	 return 0;
}

irqreturn_t silfp_irq_handler(int irq, void *dev_id)
{
    struct tran_fp_data *tran_fp_datap = (struct tran_fp_data *)dev_id;

    if (tran_fp_datap->irq_ignore) {
        return IRQ_HANDLED;
    }

#ifdef CONFIG_PM_SLEEP
    __pm_wakeup_event(g_tran_fp_datap->tran_wakelock, msecs_to_jiffies(10000));
#else
    wake_lock_timeout(&tran_fp_datap->tran_wakelock, 10*HZ); /* set a little long for a poor MCU */
#endif
    queue_work(tran_fp_datap->wqueue, &tran_fp_datap->work_queue);
    complete(&tran_fp_datap->done);

    return IRQ_HANDLED;
}

void silfp_work_func(struct work_struct *work)
{
    struct tran_fp_data *tran_fp_datap = container_of(work, struct tran_fp_data, work_queue);

#ifdef FINGERPRINT_INTERRUPT_LOG
    TRAN_FP_DEBUG("[%s] running\n", __func__);
#endif
    tran_netlink_send(tran_fp_datap, SIFP_NETLINK_IRQ);
}

void silfp_pwdn(struct tran_fp_data *tran_fp_datap, u8 flag_avdd)
{
    TRAN_FP_DEBUG("[%s] enter, port=%d\n", __func__, tran_fp_datap->reset_gpio);

    if (SIFP_PWDN_FLASH == flag_avdd) {
	tran_fp_datap->power_onoff(tran_fp_datap, false);
	tran_fp_datap->power_init(tran_fp_datap, false);
        mdelay(200);
	tran_fp_datap->power_init(tran_fp_datap, true);
	tran_fp_datap->power_onoff(tran_fp_datap, true);
    }
    pinctrl_select_state(tran_fp_datap->tran_pinctrl, tran_fp_datap->pins_reset_low);
    if (tran_fp_datap->irq_no_use) {
        pinctrl_select_state(tran_fp_datap->tran_pinctrl, tran_fp_datap->pins_reset_high);
    }

    if (SIFP_PWDN_POWEROFF == flag_avdd) {
	tran_fp_datap->power_onoff(tran_fp_datap, false);
	tran_fp_datap->power_init(tran_fp_datap, false);
    }
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void sif_early_suspend(struct early_suspend *es)
{
    struct tran_fp_data *tran_fp_datap = container_of(es, struct tran_fp_data, early_suspend);

    TRAN_FP_DEBUG("[%s] enter\n", __func__);
    tran_fp_datap->scr_off = 1;
    tran_netlink_send(tran_fp_datap, SIFP_NETLINK_SCR_OFF);
}

void sif_late_resume(struct early_suspend *es)
{
    struct tran_fp_data *tran_fp_datap = container_of(es, struct tran_fp_data, early_suspend);

    TRAN_FP_DEBUG("[%s] enter\n", __func__);
    tran_fp_datap->scr_off = 0;
    tran_netlink_send(tran_fp_datap, SIFP_NETLINK_SCR_ON);
}
#else
#if IS_ENABLED(CONFIG_TRAN_HBM_NOTIFY_FINGERPRINT) || IS_ENABLED(CONFIG_TRAN_LHBM_NOTIFY_FINGERPRINT)
static void tran_fasync_for_lcm_full_HBM(void)
{
        struct tran_fp_data *tran_fp_datap = g_tran_fp_datap;
        char* env_ext[2] = {"TRAN_FULL_HBM_EVENT=FULL_HBM_SET", NULL};
        if(tran_fp_datap->tran_fp_dev!=NULL){
                TRAN_FP_ERROR("lcm start setting HBM and prepare to notify fp HAL by uevent");
                kobject_uevent_env(&tran_fp_datap->tran_fp_dev->dev.kobj, KOBJ_CHANGE, env_ext);
        }else{
                TRAN_FP_ERROR("tran_fp_datap->tran_fp_dev is null");
        }
}
#endif
int sif_fb_notifier_callback(struct notifier_block *notif,
                             unsigned long event, void *data)
{
    struct tran_fp_data *tran_fp_datap = container_of(notif, struct tran_fp_data, notifier);
    //struct fb_event *evdata = data;
    //unsigned int blank;
    struct tran_fp_data *slf_dev = NULL;
    int retval = 0;
    int *blank=(int *)data;
    TRAN_FUNC_ENTRY();
    /* If we aren't interested in this event, skip it immediately ... */
    if (event != MTK_DISP_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
        return 0;
    }

    //blank = *(int *)evdata->data;
    slf_dev = container_of(notif, struct tran_fp_data, notifier);

    TRAN_FP_DEBUG("[%s] enter, blank=0x%x\n", __func__, *blank);

    switch (*blank) {
	case MTK_DISP_BLANK_UNBLANK:
		TRAN_FP_INFO(" lcd on notify\n");
        tran_fp_datap->scr_off = 0;
		tran_netlink_send(slf_dev, TRAN_NETLINK_SCREEN_ON);
		break;

	case MTK_DISP_BLANK_POWERDOWN:
		TRAN_FP_INFO(" lcd off notify\n");
        tran_fp_datap->scr_off = 1;
		tran_netlink_send(slf_dev, TRAN_NETLINK_SCREEN_OFF);
		break;
#if IS_ENABLED(CONFIG_TRAN_HBM_NOTIFY_FINGERPRINT) || IS_ENABLED(CONFIG_TRAN_LHBM_NOTIFY_FINGERPRINT)
		case TRAN_DISP_BLANK_NOTIFY_FINGERPRINT:
		TRAN_FP_INFO(" TRAN_DISP_BLANK_NOTIFY_FINGERPRINT\n");
		tran_fasync_for_lcm_full_HBM();
		break;
#endif
    case TRAN_DISP_BLANK_AOD_NOTIFY:
        TRAN_FP_INFO("[%s] LCD ON\n", __func__);
        tran_fp_datap->scr_off = 0;
        tran_netlink_send(tran_fp_datap, SIFP_NETLINK_SCR_ON);
        break;

    default:
        TRAN_FP_INFO("[%s] Unknown notifier\n", __func__);
        break;
    }
    return retval;
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

int silfp_keyevent(struct tran_fp_data	*tran_fp_datap, struct fp_dev_key_t *pkey)
{
    int ret = -EFAULT;
    int i;
//BSP:add for disable side button slide P673LCXE-1782 by jili.chen 20241205 start
    struct device_node *node = NULL;
    node = of_find_compatible_node(NULL, NULL, "tran_fp");
//BSP:add for disable side button slide P673LCXE-1782 by jili.chen 20241205 end

    if (!tran_fp_datap->input) {
        TRAN_FP_ERROR("[%s] invalid input device\n",__func__);
        return -1;
    }
    if ( IS_KEY_VALID(pkey->value) ) {
        /* Translate Click Down/Up key to Click key. */
        switch( pkey->value ) {
        case NAV_KEY_CLICK_DOWN:
            pkey->value = NAV_KEY_CLICK;
            pkey->flag = NAV_KEY_FLAG_DOWN;
            break;
        case NAV_KEY_CLICK_UP:
            pkey->value = NAV_KEY_CLICK;
            pkey->flag = NAV_KEY_FLAG_UP;
            break;
        default:
            break;
        }

        /* Check the custom define keymap */
        if ( tran_fp_datap->k[pkey->value - NAV_KEY_START] ) {
            TRAN_FP_INFO("[%s] custom-key %d\n", __func__,tran_fp_datap->k[pkey->value - NAV_KEY_START]);
            if ( KEY_RESERVED != tran_fp_datap->k[pkey->value - NAV_KEY_START] ) {
                if ( NAV_KEY_FLAG_CLICK == pkey->flag ) {
                    input_report_key(tran_fp_datap->input, tran_fp_datap->k[pkey->value - NAV_KEY_START], NAV_KEY_FLAG_DOWN);
                    input_sync(tran_fp_datap->input);
                    input_report_key(tran_fp_datap->input, tran_fp_datap->k[pkey->value - NAV_KEY_START], NAV_KEY_FLAG_UP);
                    input_sync(tran_fp_datap->input);
                } else {
                    input_report_key(tran_fp_datap->input, tran_fp_datap->k[pkey->value - NAV_KEY_START], pkey->flag);
                    input_sync(tran_fp_datap->input);
                }
            } else {
                // Here means this key is not set, simply ignore it.
            }
            ret = 0;
        }
    }
    
    for ( i = 0; ret && i < ARRAY_SIZE(keymap); i++ ) {
        if ( keymap[i].key_orig == pkey->value ) {
            TRAN_FP_INFO("[%s] key %d\n", __func__,keymap[i].key_new);
//BSP:add for disable side button slide P673LCXE-1782 by jili.chen 20241205 start
            if ( of_property_read_bool(node,"disable-side-button-slide") ) {
                switch ( keymap[i].key_new ) {
                case KEY_UP:
                case KEY_DOWN:
                case KEY_RIGHT:
                case KEY_LEFT:
                case KEY_F28:
                    TRAN_FP_INFO("[%s] disable side button slide, change key to %d\n", __func__,KEY_F28);
                    if ( NAV_KEY_FLAG_CLICK == pkey->flag ) {
                        input_report_key(tran_fp_datap->input, KEY_F28, NAV_KEY_FLAG_DOWN);
                        input_sync(tran_fp_datap->input);
                        input_report_key(tran_fp_datap->input, KEY_F28, NAV_KEY_FLAG_UP);
                        input_sync(tran_fp_datap->input);
                    } else {
                        input_report_key(tran_fp_datap->input, KEY_F28, pkey->flag);
                        input_sync(tran_fp_datap->input);
                    }
                    break;
                default:
                    TRAN_FP_INFO("[%s] this key is not set\n", __func__);
                    break;
                }
            } else if ( KEY_RESERVED != keymap[i].key_new ) {
//BSP:add for disable side button slide P673LCXE-1782 by jili.chen 20241205 end
                       if ( NAV_KEY_FLAG_CLICK == pkey->flag ) {
                           input_report_key(tran_fp_datap->input, keymap[i].key_new, NAV_KEY_FLAG_DOWN);
                           input_sync(tran_fp_datap->input);
                           input_report_key(tran_fp_datap->input, keymap[i].key_new, NAV_KEY_FLAG_UP);
                           input_sync(tran_fp_datap->input);
                       } else {
                           input_report_key(tran_fp_datap->input, keymap[i].key_new, pkey->flag);
                           input_sync(tran_fp_datap->input);
                       }
            } else {
               // Here means this key is not set, simply ignore it.
            }         
            ret = 0;
        }
    }

    if ( ret ) {
        TRAN_FP_INFO("[%s] unregister custom-key %d\n", __func__,pkey->value);
        input_report_key(tran_fp_datap->input, pkey->value, pkey->flag);
        input_sync(tran_fp_datap->input);
        ret = 0;
    }
    return ret;
}

int sif_proc_init(struct tran_fp_data *tran_fp_dev)
{
    if (tran_fp_dev) {
        tran_fp_dev->proc_entry = NULL;
    }
    return 0;

}

void sif_proc_deinit(struct tran_fp_data *tran_fp_dev)
{
    if (tran_fp_dev) {
        if (tran_fp_dev->proc_entry) {
            remove_proc_entry(PROC_NODE, NULL);
        tran_fp_dev->proc_entry = NULL;
        }
    }
}

void silfp_wakelock_ctl(struct tran_fp_data *tran_fp_dev, unsigned char lock)
{
    if (lock) {
#ifdef CONFIG_PM_SLEEP
    __pm_wakeup_event(tran_fp_dev->wakelock_hal, msecs_to_jiffies(10000));
#else
        wake_lock_timeout(&tran_fp_dev->wakelock_hal, 10*HZ);
#endif
    } else {
#ifdef CONFIG_PM_SLEEP
        __pm_relax(tran_fp_dev->wakelock_hal);
#else
        wake_unlock(&tran_fp_dev->wakelock_hal);
#endif
    }
}

int silfp_proc_create_node(struct tran_fp_data *tran_fp_dev)
{
    if (tran_fp_dev->proc_entry == NULL) {
        tran_fp_dev->proc_entry = proc_create(PROC_NODE, 0666, NULL, &silfp_proc_fops);
        if (tran_fp_dev->proc_entry == NULL) {
            TRAN_FP_ERROR("Create entry %s under /proc/ error!\n", PROC_NODE);
            goto err_out1;
        }
    }
    return 0;
err_out1:
    return -ENOMEM;
}

int sif_fp_irq_init(void)
{
    return 0;
}
