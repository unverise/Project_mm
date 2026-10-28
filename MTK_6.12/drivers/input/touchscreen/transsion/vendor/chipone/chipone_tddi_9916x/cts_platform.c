#define LOG_TAG         "Plat"

#include <linux/vmalloc.h>
#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sysfs.h"
#include "cts_tcs.h"
#include "cts_transfer.h"

#ifndef KEY_F26
#define KEY_F26 198
#endif

int cts_plat_i2c_transfer(struct cts_platform_data *pdata, u8 i2c_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay)
{
    int num_msg, ret = 0, retries = 0;

    struct i2c_msg msgs[2] = {
        {
            .addr = i2c_addr,
            .flags = 0,
            .buf = (u8 *) wbuf,
            .len = wlen,
        },
        {
            .addr = i2c_addr,
            .flags = I2C_M_RD,
            .buf = (u8 *) rbuf,
            .len = rlen,
        }
    };

    if (rbuf == NULL || rlen == 0)
        num_msg = 1;
    else
        num_msg = 2;

    do {
        ret = i2c_transfer(pdata->i2c_client->adapter,
                msgs, num_msg);

        if (ret != num_msg) {
            if (ret >= 0)
                ret = -EIO;

            if (delay)
                mdelay(delay);
            continue;
        } else {
            return 0;
        }

    } while (++retries < retry);

    return ret;
}

void dump_spi_common(const char *prefix, u8 *data, size_t datalen)
{
    u8 str[1024];
    int offset = 0;
    int i;

    offset += snprintf(str + offset, sizeof(str) - offset, "%s", prefix);
    for (i = 0; i < datalen; i++) {
        offset += snprintf(str + offset, sizeof(str) - offset, " %02x", data[i]);
    }
    cts_err("%s", str);
}

#ifdef CFG_CTS_MANUAL_CS
int cts_plat_set_cs(struct cts_platform_data *pdata, int val)
{
    if (val)
        gpio_set_value(pdata->cs_gpio, 1);
    else
        gpio_set_value(pdata->cs_gpio, 0);

    return 0;
}
#endif

int cts_spi_send_recv(struct cts_platform_data *pdata, u8 *tx_buffer,
        size_t len, u8 *rx_buffer)
{
    struct chipone_ts_data *cts_data;
    struct spi_message msg;
    struct spi_transfer cmd = {
        /*.delay_usecs = 0,*/
        .speed_hz = pdata->spi_speed * 1000u,
        .tx_buf = tx_buffer,
        .rx_buf = rx_buffer,
        .len = len,
        /* .tx_dma = 0, */
        /* .rx_dma = 0, */
    };
    int ret = 0;

    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
    cts_dbg("%s:%d, transfer length:%d\n", __func__, __LINE__, len);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(pdata, 0);
#endif
    spi_message_init(&msg);
    spi_message_add_tail(&cmd, &msg);
    ret = spi_sync(cts_data->spi_client, &msg);
    if (ret)
        cts_err("spi sync failed %d", ret);

    udelay(100);

#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(pdata, 1);
#endif
    return ret;
}

int cts_plat_is_normal_mode(struct cts_platform_data *pdata)
{
    u16 fwid = 0;
    int ret;

    ret = cts_tcs_get_fw_id(pdata->cts_dev, &fwid);
    if (ret || !cts_is_fwid_valid(fwid)) {
        pdata->cts_dev->rtdata.boot_mode = CTS_BOOT_MODE_UNKOWN;
        return false;
    }

    pdata->cts_dev->rtdata.boot_mode = CTS_BOOT_MODE_SRAM;
    return true;
}

bool g_int_pin_test_value = true;
static void cts_plat_handle_test_irq(struct cts_platform_data *pdata)
{
    cts_info("Handle test IRQ");
    g_int_pin_test_value = !CTS_INT_DEFAULT_TEST_VAL;
}

static void cts_plat_handle_irq(struct cts_platform_data *pdata)
{
    int ret;

    cts_dbg("Handle IRQ");

    //cts_lock_device(pdata->cts_dev);
    ret = cts_irq_handler(pdata->cts_dev);
    if (ret)
        cts_err("Device handle IRQ failed %d", ret);
    //cts_unlock_device(pdata->cts_dev);
}

static irqreturn_t cts_plat_test_irq_handler(int irq, void *dev_id)
{
    struct cts_platform_data *pdata;
#ifndef CONFIG_GENERIC_HARDIRQS
    struct chipone_ts_data *cts_data;
#endif

    cts_info("IRQ test handler");

    pdata = (struct cts_platform_data *)dev_id;
    if (pdata == NULL) {
        cts_err("IRQ test handler with NULL dev_id");
        return IRQ_NONE;
    }
#ifdef CONFIG_GENERIC_HARDIRQS
    cts_plat_handle_test_irq(pdata);
#else
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

    if (queue_work(cts_data->workqueue, &pdata->ts_test_irq_work)) {
        cts_info("IRQ test queue work");
        cts_plat_disable_irq(pdata);
    } else
        cts_warn("IRQ test handler queue work failed as already on the queue");
#endif /* CONFIG_GENERIC_HARDIRQS */

    return IRQ_HANDLED;
}

static irqreturn_t cts_plat_irq_handler(int irq, void *dev_id)
{
    struct cts_platform_data *pdata;
#ifndef CONFIG_GENERIC_HARDIRQS
    struct chipone_ts_data *cts_data;
#endif

    cts_dbg("IRQ handler");
    pdata = (struct cts_platform_data *)dev_id;
    if (pdata == NULL) {
        cts_err("IRQ handler with NULL dev_id");
        return IRQ_NONE;
    }

#ifdef CONFIG_GENERIC_HARDIRQS
    cts_plat_handle_irq(pdata);
#else
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

    if (queue_work(cts_data->workqueue, &pdata->ts_irq_work)) {
        cts_dbg("IRQ queue work");
        cts_plat_disable_irq(pdata);
    } else
        cts_warn("IRQ handler queue work failed as already on the queue");
#endif /* CONFIG_GENERIC_HARDIRQS */

    return IRQ_HANDLED;
}

#ifndef CONFIG_GENERIC_HARDIRQS
static void cts_plat_touch_dev_test_irq_work(struct work_struct *work)
{
    struct cts_platform_data *pdata =
        container_of(work, struct cts_platform_data, ts_test_irq_work);

    cts_dbg("IRQ work");

    cts_plat_handle_test_irq(pdata);

    cts_plat_enable_irq(pdata);
}

static void cts_plat_touch_dev_irq_work(struct work_struct *work)
{
    struct cts_platform_data *pdata =
        container_of(work, struct cts_platform_data, ts_irq_work);

    cts_dbg("IRQ work");

    cts_plat_handle_irq(pdata);

    cts_plat_enable_irq(pdata);
}
#endif /* CONFIG_GENERIC_HARDIRQS */

#ifdef CONFIG_CTS_OF
static int cts_plat_parse_dt(struct cts_platform_data *pdata,
        struct device_node *dev_node)
{
    int ret = 0;

    cts_info("Parse device tree");

    pdata->int_gpio = of_get_named_gpio(dev_node, CFG_CTS_OF_INT_GPIO_NAME, 0);
    if (!gpio_is_valid(pdata->int_gpio)) {
        cts_err("Parse INT GPIO from dt failed %d", pdata->int_gpio);
        pdata->int_gpio = -1;
    }
    cts_info("  %-12s: %d", "int gpio", pdata->int_gpio);

    pdata->irq = gpio_to_irq(pdata->int_gpio);
    if (pdata->irq < 0) {
        cts_err("Parse irq failed %d", ret);
        return pdata->irq;
    }
    cts_info("  %-12s: %d", "irq num", pdata->irq);

#ifdef CFG_CTS_HAS_RESET_PIN
    pdata->rst_gpio = of_get_named_gpio(dev_node, CFG_CTS_OF_RST_GPIO_NAME, 0);
    if (!gpio_is_valid(pdata->rst_gpio)) {
        cts_err("Parse RST GPIO from dt failed %d", pdata->rst_gpio);
        pdata->rst_gpio = -1;
    }
    cts_info("  %-12s: %d", "rst gpio", pdata->rst_gpio);
#endif /* CFG_CTS_HAS_RESET_PIN */

#ifdef CFG_CTS_MANUAL_CS
    pdata->cs_gpio = of_get_named_gpio(dev_node, CFG_CTS_OF_CS_GPIO_NAME, 0);
    if (!gpio_is_valid(pdata->cs_gpio)) {
        cts_err("Parse CS GPIO from dt failed %d", pdata->cs_gpio);
        pdata->cs_gpio = -1;
    }
    cts_info("  %-12s: %d", "cs gpio", pdata->cs_gpio);
#endif

    ret = of_property_read_u32(dev_node, CFG_CTS_OF_X_RESOLUTION_NAME,
            &pdata->res_x);
    if (ret)
        cts_warn("Parse X resolution from dt failed %d", ret);

    cts_info("  %-12s: %d", "X resolution", pdata->res_x);

    ret = of_property_read_u32(dev_node, CFG_CTS_OF_Y_RESOLUTION_NAME,
            &pdata->res_y);
    if (ret)
        cts_warn("Parse Y resolution from dt failed %d", ret);

    cts_info("  %-12s: %d", "Y resolution", pdata->res_y);

    if (of_property_read_u32(dev_node, "chipone,def-build-id", &pdata->build_id)) {
        pdata->build_id = 0;
        cts_info("chipone,build_id undefined.");
    } else
        cts_info("chipone,build_id=0x%04X", pdata->build_id);

    if (of_property_read_u32(dev_node, "chipone,def-config-id", &pdata->config_id)) {
        pdata->config_id = 0;
        cts_info("chipone,config_id undefined.");
    } else
        cts_info("chipone,config_id=0x%04X", pdata->config_id);

    //BSP:add for pocket mode by xihao.ding 20251129 start
    pdata->pocket_function_support = of_property_read_bool(dev_node, "chipone,pocket-function-support");
    cts_info("pocket_function_support is %d", pdata->pocket_function_support);
    //BSP:add for pocket mode by xihao.ding 20251129 end

#ifdef CFG_CTS_FW_UPDATE_SYS
    ret = of_property_read_string(dev_node, CFG_CTS_OF_PANEL_SUPPLIER,
            &pdata->panel_supplier);
    if (ret) {
        pdata->panel_supplier = NULL;
        cts_warn("read panel supplier failed, ret=%d", ret);
    } else
        cts_info("panel supplier=%s", (char *)pdata->panel_supplier);
#endif

    return 0;
}
#endif /* CONFIG_CTS_OF */

#ifdef CFG_CTS_FORCE_UP
static void cts_plat_touch_event_timeout_work(struct work_struct *work)
{
    struct cts_platform_data *pdata = container_of(work,
            struct cts_platform_data, touch_event_timeout_work.work);

    cts_warn("Touch event timeout work");

    cts_plat_release_all_touch(pdata);
}
#endif

int cts_plat_spi_setup(struct cts_platform_data *pdata)
{
    int ret;

//TODO: SPI setting is configured by hardcode!!!
    //pdata->spi_client->chip_select = 1;
    pdata->spi_client->mode = SPI_MODE_0;
    pdata->spi_client->bits_per_word = 8;
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    pdata->spi_client->cs_setup.value = 1; //delay 1
    pdata->spi_client->cs_setup.unit = 0;  //delay us
	#endif

    //cts_info("chip_select  :%d", pdata->spi_client->chip_select);
    cts_info("spi_mode     :%d", pdata->spi_client->mode);
    cts_info("bits_per_word:%d", pdata->spi_client->bits_per_word);

    ret = spi_setup(pdata->spi_client);
    if (ret)
        cts_err("spi_setup err!");
    return 0;
}

static int _cts_init_platform_data(struct cts_platform_data *pdata,
        enum cts_bus_type bus_type)
{
    struct input_dev *input_dev;
#ifdef CFG_CTS_GESTURE
    u8 gesture_keymap[CFG_CTS_NUM_GESTURE][2] = CFG_CTS_GESTURE_KEYMAP;
#endif
    int ret;

    cts_info("_cts_init_platform_data Init");

    mutex_init(&pdata->dev_lock);
    spin_lock_init(&pdata->irq_lock);

    input_dev = input_allocate_device();
    if (input_dev == NULL) {
        cts_err("Failed to allocate input device.");
        return -ENOMEM;
    }

    /** - Init input device */
    input_dev->name = CFG_CTS_DEVICE_NAME;
    input_dev->name = CFG_CTS_DEVICE_NAME;

    if (bus_type == CTS_BUS_SPI) {
        input_dev->id.bustype = BUS_SPI;
        input_dev->dev.parent = &pdata->spi_client->dev;
        pdata->spi_tx_buf = vzalloc(CTS_MAX_SPI_BUF_SIZE);
        pdata->spi_rx_buf = vzalloc(CTS_MAX_SPI_BUF_SIZE);
    } else if (bus_type == CTS_BUS_I2C) {
        input_dev->id.bustype = BUS_I2C;
        input_dev->dev.parent = &pdata->i2c_client->dev;
        pdata->i2c_tx_buf = vzalloc(CTS_MAX_I2C_BUF_SIZE);
        pdata->i2c_rx_buf = vzalloc(CTS_MAX_I2C_BUF_SIZE);
    } else if (bus_type == CTS_BUS_I3C) {
        cts_err("%s(%d): i3c is not supported\n", __func__, __LINE__);
        return -EINVAL;
    }

    pdata->int_data = vzalloc(CFG_CTS_INT_DATA_MAX_SIZE);

    input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
#ifdef CFG_CTS_PALM_DETECT
    set_bit(CFG_CTS_PALM_EVENT, input_dev->keybit);
#endif

#ifdef CFG_CTS_SWAP_XY
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->res_y, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->res_x, 0, 0);
#else /* CFG_CTS_SWAP_XY */
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->res_x, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->res_y, 0, 0);
#endif /* CFG_CTS_SWAP_XY */

    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,
            CFG_CTS_MAX_TOUCH_NUM * 2, 0, 0);
    /*bsp: add for bringup by guoxin.zhu 20230608 start*/
    input_set_capability(input_dev, EV_KEY, KEY_F26);
    input_set_capability(input_dev, EV_KEY, KEY_F20);
    /*bsp: add for bringup by guoxin.zhu 20230608 end*/
    //BSP:add for pocket mode by xihao.ding 20251129 start
    input_set_capability(input_dev, EV_KEY, KEY_LARGE_AREA);
    //BSP:add for pocket mode by xihao.ding 20251129 end

    input_set_capability(input_dev, EV_KEY, BTN_TOUCH);

#ifdef CONFIG_CTS_SLOTPROTOCOL
    input_mt_init_slots(input_dev, CFG_CTS_MAX_TOUCH_NUM, 0);
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
    __set_bit(EV_ABS, input_dev->evbit);
    input_set_drvdata(input_dev, pdata);
    ret = input_register_device(input_dev);
    if (ret) {
        cts_err("Failed to register input device");
        goto free_mem;
    }

    pdata->ts_input_dev = input_dev;
    /*bsp: add for bringup by guoxin.zhu 20230608 start*/
    chipone_controller.input_dev = input_dev;
    /*bsp: add for bringup by guoxin.zhu 20230608 end*/

#if !defined(CONFIG_GENERIC_HARDIRQS)
    INIT_WORK(&pdata->ts_irq_work, cts_plat_touch_dev_irq_work);
    INIT_WORK(&pdata->ts_test_irq_work, cts_plat_touch_dev_test_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */

#ifdef CONFIG_CTS_VIRTUALKEY
    u8 vkey_keymap[CFG_CTS_NUM_VKEY] = CFG_CTS_VKEY_KEYCODES;

    memcpy(pdata->vkey_keycodes, vkey_keymap, sizeof(vkey_keymap));
    pdata->vkey_num = CFG_CTS_NUM_VKEY;
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CFG_CTS_GESTURE
    memcpy(pdata->gesture_keymap, gesture_keymap, sizeof(gesture_keymap));
    pdata->gesture_num = CFG_CTS_NUM_GESTURE;
#endif /* CFG_CTS_GESTURE */

#ifdef CFG_CTS_FORCE_UP
    INIT_DELAYED_WORK(&pdata->touch_event_timeout_work,
            cts_plat_touch_event_timeout_work);
#endif

    cts_info("_cts_init_platform_data init success\n!");
    return ret;

free_mem:
    if (bus_type == CTS_BUS_SPI) {
        vfree(pdata->spi_tx_buf);
        pdata->spi_tx_buf = NULL;
        vfree(pdata->spi_rx_buf);
        pdata->spi_rx_buf = NULL;
    } else if (bus_type == CTS_BUS_I2C) {
        vfree(pdata->i2c_tx_buf);
        pdata->i2c_tx_buf = NULL;
        vfree(pdata->i2c_rx_buf);
        pdata->i2c_rx_buf = NULL;
    }

    if (pdata->int_data) {
        vfree(pdata->int_data);
        pdata->int_data = NULL;
    }

    return ret;
}


int cts_i2c_init_platform_data(struct cts_platform_data *pdata,
        struct i2c_client *i2c_client)
{
    int ret;

#ifdef CONFIG_CTS_OF
    struct device *dev;
    dev = &i2c_client->dev;
    ret = cts_plat_parse_dt(pdata, dev->of_node);
    if (ret) {
        cts_err("Parse dt failed %d", ret);
        return ret;
    }
#endif /* CONFIG_CTS_OF */
    pdata->i2c_client = i2c_client;
    pdata->i2c_client->irq = pdata->irq;
    ret = _cts_init_platform_data(pdata, CTS_BUS_I2C);
    if (ret)
        cts_err("platform data init failed, ret = %d\n", ret);

    return ret;
}

int cts_spi_init_platform_data(struct cts_platform_data *pdata,
        struct spi_device *spi)
{
    int ret = 0;

#ifdef CONFIG_CTS_OF
    struct device *dev;
    dev = &spi->dev;
    ret = cts_plat_parse_dt(pdata, dev->of_node);
    if (ret) {
        cts_err("Parse dt failed %d", ret);
        return ret;
    }
#endif /* CONFIG_CTS_OF */

    pdata->spi_client = spi;
    pdata->spi_client->irq = pdata->irq;

    pdata->spi_speed = CFG_CTS_SPI_SPEED_KHZ;
    cts_plat_spi_setup(pdata);

    ret = _cts_init_platform_data(pdata, CTS_BUS_SPI);
    if (ret)
        cts_err("platform data init failed, ret = %d\n", ret);

    return ret;
}

void cts_post_init_platform_data(struct cts_platform_data *pdata)
{
    int ret;

    pdata->is_cascade_enabled = false;

    ret = cts_is_casc_enabled(pdata->cts_dev);
    if (ret < 0)
        cts_err("get casc state failed, ret:%d\n", ret);
    else
        pdata->is_cascade_enabled = !!ret;
    cts_info("%s, cascade %s", __func__, pdata->is_cascade_enabled ?
        "enabled" : "disabled");
}

int cts_deinit_platform_data(struct cts_platform_data *pdata)
{
    struct cts_device *cts_dev = pdata->cts_dev;

    cts_info("De-Init platform_data");
    input_unregister_device(pdata->ts_input_dev);
    if (cts_dev->bus_type == CTS_BUS_SPI) {
        if (pdata->spi_tx_buf) {
            vfree(pdata->spi_tx_buf);
            pdata->spi_tx_buf = NULL;
        }
        if (pdata->spi_rx_buf) {
            vfree(pdata->spi_rx_buf);
            pdata->spi_rx_buf = NULL;
        }
    } else if (cts_dev->bus_type == CTS_BUS_I2C) {
        if (pdata->i2c_tx_buf) {
            vfree(pdata->i2c_tx_buf);
            pdata->i2c_tx_buf = NULL;
        }
        if (pdata->i2c_rx_buf) {
            vfree(pdata->i2c_rx_buf);
            pdata->i2c_rx_buf = NULL;
        }
    }

    if (pdata->int_data) {
        vfree(pdata->int_data);
        pdata->int_data = NULL;
    }

    return 0;
}

int cts_plat_request_resource(struct cts_platform_data *pdata)
{
    int ret;

    cts_info("Request resource");

    ret = gpio_request_one(pdata->int_gpio, GPIOF_IN,
            CFG_CTS_DEVICE_NAME "-int");
    if (ret) {
        cts_err("Request INT gpio (%d) failed %d", pdata->int_gpio, ret);
        goto err_out;
    }
#ifdef CFG_CTS_HAS_RESET_PIN
    ret = gpio_request_one(pdata->rst_gpio, GPIOF_OUT_INIT_HIGH,
            CFG_CTS_DEVICE_NAME "-rst");
    if (ret) {
        cts_err("Request RST gpio (%d) failed %d", pdata->rst_gpio, ret);
        goto err_free_int;
    }
#endif /* CFG_CTS_HAS_RESET_PIN */

#ifdef CFG_CTS_MANUAL_CS
    ret = gpio_request_one(pdata->cs_gpio, GPIOF_OUT_INIT_HIGH,
            CFG_CTS_DEVICE_NAME "-cs");
    if (ret) {
        cts_err("Request CS gpio (%d) failed %d", pdata->cs_gpio, ret);
        goto err_request_cs_gpio;
    }
#endif

    return 0;

#ifdef CFG_CTS_MANUAL_CS
err_request_cs_gpio:
#endif

#ifdef CONFIG_CTS_REGULATOR
err_free_rst:
#endif /* CONFIG_CTS_REGULATOR */
#ifdef CFG_CTS_HAS_RESET_PIN
    gpio_free(pdata->rst_gpio);
err_free_int:
#endif /* CFG_CTS_HAS_RESET_PIN */
    gpio_free(pdata->int_gpio);
err_out:
    return ret;
}

void cts_plat_free_resource(struct cts_platform_data *pdata)
{
    cts_info("Free resource");

    if (gpio_is_valid(pdata->int_gpio))
        gpio_free(pdata->int_gpio);

#ifdef CFG_CTS_HAS_RESET_PIN
    if (gpio_is_valid(pdata->rst_gpio))
        gpio_free(pdata->rst_gpio);
#endif /* CFG_CTS_HAS_RESET_PIN */

#ifdef CFG_CTS_MANUAL_CS
    if (gpio_is_valid(pdata->cs_gpio))
        gpio_free(pdata->cs_gpio);
#endif
}

int cts_plat_request_test_irq(struct cts_platform_data *pdata)
{
    int ret;

    cts_info("Request test IRQ");

#ifdef CONFIG_GENERIC_HARDIRQS
    ret = request_threaded_irq(pdata->irq, NULL, cts_plat_test_irq_handler,
            IRQF_TRIGGER_FALLING | IRQF_ONESHOT, CFG_CTS_DRIVER_NAME, pdata);
#else /* CONFIG_GENERIC_HARDIRQS */
    ret = request_irq(pdata->irq, cts_plat_test_irq_handler,
            IRQF_TRIGGER_FALLING | IRQF_ONESHOT, CFG_CTS_DRIVER_NAME, pdata);
#endif /* CONFIG_GENERIC_HARDIRQS */
    if (ret) {
        cts_err("Request test IRQ failed %d", ret);
        return ret;
    }

    cts_plat_disable_irq(pdata);

    return 0;
}

int cts_plat_request_irq(struct cts_platform_data *pdata)
{
    int ret;

    cts_info("Request IRQ");

#ifdef CONFIG_GENERIC_HARDIRQS
    ret = request_threaded_irq(pdata->irq, NULL, cts_plat_irq_handler,
            IRQF_TRIGGER_FALLING | IRQF_ONESHOT, CFG_CTS_DRIVER_NAME, pdata);
#else /* CONFIG_GENERIC_HARDIRQS */
    ret = request_irq(pdata->irq, cts_plat_irq_handler,
            IRQF_TRIGGER_FALLING | IRQF_ONESHOT, CFG_CTS_DRIVER_NAME, pdata);
#endif /* CONFIG_GENERIC_HARDIRQS */
    if (ret) {
        cts_err("Request IRQ failed %d", ret);
        return ret;
    }

    cts_plat_disable_irq(pdata);

    return 0;
}

int cts_switch_to_test_irq(struct cts_platform_data *pdata)
{
    int ret;

#ifndef CONFIG_GENERIC_HARDIRQS
    if (work_pending(&pdata->ts_irq_work))
        cancel_work_sync(&pdata->ts_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */
    cts_plat_free_irq(pdata);

    ret = cts_plat_request_test_irq(pdata);
    if (ret) {
        cts_err("request rest irq failed");
        goto restore_irq;
    }

    ret = cts_plat_enable_irq(pdata);
    if (ret) {
        cts_err("enable test irq failed");
        goto enable_irq_err;
    }
    return ret;

enable_irq_err:
    cts_plat_free_irq(pdata);
restore_irq:
    if (cts_plat_request_irq(pdata))
        cts_err("restore normal irq handler failed");

    return ret;
}

int restore_normal_irq(struct cts_platform_data *pdata)
{
    int ret;

#ifndef CONFIG_GENERIC_HARDIRQS
    if (work_pending(&pdata->ts_test_irq_work))
        cancel_work_sync(&pdata->ts_test_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */
    cts_plat_free_irq(pdata);

    ret = cts_plat_request_irq(pdata);
    if (ret)
        cts_err("restore normal irq handler failed");

    return ret;
}

void cts_plat_free_irq(struct cts_platform_data *pdata)
{
    free_irq(pdata->irq, pdata);
    /* clear the flag to prevent unbanlanced irq dis/enable */
    pdata->irq_is_disable = false;
}

int cts_plat_enable_irq(struct cts_platform_data *pdata)
{
    unsigned long irqflags;

    cts_dbg("Enable IRQ");

    if (pdata->irq > 0) {
        spin_lock_irqsave(&pdata->irq_lock, irqflags);
        if (pdata->irq_is_disable) {/* && !cts_is_device_suspended(pdata->chip)) */
            cts_dbg("Real enable IRQ");
            enable_irq(pdata->irq);
            pdata->irq_is_disable = false;
        }
        spin_unlock_irqrestore(&pdata->irq_lock, irqflags);

        return 0;
    }

    return -ENODEV;
}

int cts_plat_disable_irq(struct cts_platform_data *pdata)
{
    unsigned long irqflags;

    cts_dbg("Disable IRQ");

    if (pdata->irq > 0) {
        spin_lock_irqsave(&pdata->irq_lock, irqflags);
        if (!pdata->irq_is_disable) {
            cts_dbg("Real disable IRQ");
            disable_irq_nosync(pdata->irq);
            pdata->irq_is_disable = true;
        }
        spin_unlock_irqrestore(&pdata->irq_lock, irqflags);

        return 0;
    }

    return -ENODEV;
}

#ifdef CFG_CTS_HAS_RESET_PIN
int cts_plat_reset_device(struct cts_platform_data *pdata)
{
    /* !!!can not be modified */
    /* !!!can not be modified */
    /* !!!can not be modified */

    if(!pdata) {
        cts_info("%s:%d, invalid input: pdata\n", __func__, __LINE__);
        return -EINVAL;
    }

    gpio_set_value(pdata->rst_gpio, 1);
    mdelay(1);
    gpio_set_value(pdata->rst_gpio, 0);
    mdelay(20);
    gpio_set_value(pdata->rst_gpio, 1);
    mdelay(40);

    return 0;
}

int cts_plat_set_reset(struct cts_platform_data *pdata, int val)
{
    cts_info("Set Reset to %s", val ? "HIGH" : "LOW");
    if (val)
        gpio_set_value(pdata->rst_gpio, 1);
    else
        gpio_set_value(pdata->rst_gpio, 0);

    return 0;
}
#endif /* CFG_CTS_HAS_RESET_PIN */

int cts_plat_get_int_pin(struct cts_platform_data *pdata)
{
    return gpio_get_value(pdata->int_gpio);
}

int cts_plat_power_up_device(struct cts_platform_data *pdata)
{
    cts_info("Power up device");

    return 0;
}

int cts_plat_power_down_device(struct cts_platform_data *pdata)
{
    cts_info("Power down device");

    return 0;
}

int cts_plat_init_touch_device(struct cts_platform_data *pdata)
{
    cts_info("Init touch device");

    return 0;
}

void cts_plat_deinit_touch_device(struct cts_platform_data *pdata)
{
    cts_info("De-init touch device");

#ifndef CONFIG_GENERIC_HARDIRQS
    if (work_pending(&pdata->ts_irq_work))
        cancel_work_sync(&pdata->ts_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */
}

#ifdef CFG_CTS_PALM_DETECT
void cts_report_palm_event(struct cts_platform_data *pdata)
{
    input_report_key(pdata->ts_input_dev, CFG_CTS_PALM_EVENT, 1);
    input_sync(pdata->ts_input_dev);
    msleep(100);
    input_report_key(pdata->ts_input_dev, CFG_CTS_PALM_EVENT, 0);
    input_sync(pdata->ts_input_dev);
}
#endif


int cts_plat_process_finger_msg(struct cts_platform_data *pdata,
    void *touch_info)
{
#if defined CFG_CTS_FORCE_UP || defined CFG_CTS_HEARTBEAT_MECHANISM
    struct chipone_ts_data *cts_data = NULL;
#endif
    struct input_dev *input_dev = pdata->ts_input_dev;
    struct cts_touchinfo_finger *finger_info;
    struct cts_touch_msg_finger *msgs;
    int finger_num;
    int i;
    int contact = 0;
#ifdef CONFIG_CTS_SLOTPROTOCOL
    static unsigned char finger_last[CFG_CTS_MAX_TOUCH_NUM] = { 0 };
#endif
    unsigned char finger_current[CFG_CTS_MAX_TOUCH_NUM] = { 0 };


    finger_info = (struct cts_touchinfo_finger *)touch_info;
    msgs = finger_info->msg;
    finger_num = finger_info->header.proto_info.finger_num;

    cts_dbg("Process touch %d msgs", finger_num);
#if defined CFG_CTS_FORCE_UP || defined CFG_CTS_HEARTBEAT_MECHANISM
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
#endif

    if (finger_num == 0 || finger_num > CFG_CTS_MAX_TOUCH_NUM)
        return 0;

    for (i = 0; i < finger_num; i++) {
        u16 x, y;

        x = le16_to_cpu(msgs[i].x_l | (msgs[i].xy_h.x_h << 8));
        y = le16_to_cpu(msgs[i].y_l | (msgs[i].xy_h.y_h << 8));
#ifdef CFG_CTS_SWAP_XY
        swap(x, y);
#endif /* CFG_CTS_SWAP_XY */
#ifdef CFG_CTS_WRAP_X
        x = wrap(pdata->res_x, x);
#endif /* CFG_CTS_WRAP_X */
#ifdef CFG_CTS_WRAP_Y
        y = wrap(pdata->res_y, y);
#endif /* CFG_CTS_WRAP_Y */
        cts_dbg("  Process touch msg[%d]: id[%u] ev=%u x=%u y=%u p=%u",
            i, msgs[i].id_info.id, msgs[i].id_info.event, x, y, msgs[i].pressure);
        if (msgs[i].id_info.event == CTS_TOUCH_EVENT_DOWN
        || msgs[i].id_info.event == CTS_TOUCH_EVENT_MOVE
        || msgs[i].id_info.event == CTS_TOUCH_EVENT_STAY) {
            if (msgs[i].id_info.id < CFG_CTS_MAX_TOUCH_NUM)
                finger_current[msgs[i].id_info.id] = 1;
        }
#ifdef CONFIG_CTS_SLOTPROTOCOL
        /* input_mt_slot(input_dev, msgs[i].id_info.id); */
        switch (msgs[i].id_info.event) {
        case CTS_TOUCH_EVENT_DOWN:

#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
            TPD_DEBUG_SET_TIME;
            TPD_EM_PRINT(x, y, x, y, msgs[i].id_info.id, 1);
            tpd_history_x = x;
            tpd_history_y = y;
#ifdef CONFIG_MTK_BOOT
            if (tpd_dts_data.use_tpd_button) {
                if (FACTORY_BOOT == get_boot_mode() ||
                    RECOVERY_BOOT == get_boot_mode())
                    tpd_button(x, y, 1);
            }
#endif /* CONFIG_MTK_BOOT */
#endif /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */

        case CTS_TOUCH_EVENT_MOVE:
        case CTS_TOUCH_EVENT_STAY:
            contact++;
            input_mt_slot(input_dev, msgs[i].id_info.id);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
            input_report_abs(input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, msgs[i].pressure);
            input_report_abs(input_dev, ABS_MT_PRESSURE, msgs[i].pressure);
            break;

        case CTS_TOUCH_EVENT_UP:
#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
            TPD_DEBUG_SET_TIME;
            TPD_EM_PRINT(tpd_history_x, tpd_history_y, tpd_history_x, tpd_history_y, msgs[i].id_info.id, 0);
            tpd_history_x = 0;
            tpd_history_y = 0;
#ifdef CONFIG_MTK_BOOT
            if (tpd_dts_data.use_tpd_button) {
                if (FACTORY_BOOT == get_boot_mode() ||
                    RECOVERY_BOOT == get_boot_mode())
                    tpd_button(0, 0, 0);
            }
#endif /* CONFIG_MTK_BOOT */
            //input_report_key(input_dev, BTN_TOUCH, 0);
            //input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
#endif /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */
            break;

        default:
            cts_warn("Process touch msg with unknwon event %u id %u",
                    msgs[i].id_info.event, msgs[i].id_info.id);
            break;
        }
#else /* CONFIG_CTS_SLOTPROTOCOL */
    /**
    * If the driver reports one of BTN_TOUCH or ABS_PRESSURE
    * in addition to the ABS_MT events, the last SYN_MT_REPORT event
    * may be omitted. Otherwise, the last SYN_REPORT will be dropped
    * by the input core, resulting in no zero-contact event
    * reaching userland.
    */
        switch (msgs[i].id_info.event) {
        case CTS_TOUCH_EVENT_DOWN:
        case CTS_TOUCH_EVENT_MOVE:
        case CTS_TOUCH_EVENT_STAY:
            contact++;
            input_report_abs(input_dev, ABS_MT_PRESSURE, msgs[i].pressure);
//            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, msgs[i].pressure);
            input_report_key(input_dev, BTN_TOUCH, 1);
            input_report_abs(input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
            input_mt_sync(input_dev);
            break;

        case CTS_TOUCH_EVENT_UP:
            break;
        default:
            cts_warn("Process touch msg with unknwon event %u id %u",
                    msgs[i].id_info.event, msgs[i].id_info.id);
            break;
        }
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    }

#ifdef CONFIG_CTS_SLOTPROTOCOL
    for (i = 0; i < CFG_CTS_MAX_TOUCH_NUM; i++) {
        if (finger_last[i] != 0 && finger_current[i] == 0) {
            input_mt_slot(input_dev, i);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
        }
        finger_last[i] = finger_current[i];
    }
    input_report_key(input_dev, BTN_TOUCH, contact > 0);
#else /* CONFIG_CTS_SLOTPROTOCOL */
    if (contact == 0) {
        input_report_key(input_dev, BTN_TOUCH, 0);
        input_mt_sync(input_dev);
    }
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    input_sync(input_dev);

#ifdef CFG_CTS_FORCE_UP
    if (contact) {
        if (delayed_work_pending(&pdata->touch_event_timeout_work)) {
            mod_delayed_work(cts_data->workqueue,
                    &pdata->touch_event_timeout_work, msecs_to_jiffies(100));
        } else {
            queue_delayed_work(cts_data->workqueue,
                    &pdata->touch_event_timeout_work, msecs_to_jiffies(100));
        }
    } else {
        cancel_delayed_work_sync(&pdata->touch_event_timeout_work);
    }
#endif

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    if (contact) {
        if (delayed_work_pending(&cts_data->heart_work)) {
            mod_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        } else {
            queue_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        }
    }
#endif

    return 0;
}

int cts_plat_process_stylus_msg(struct cts_platform_data *pdata,
        void *touch_info)
{
#if defined CFG_CTS_FORCE_UP || defined CFG_CTS_HEARTBEAT_MECHANISM
    struct chipone_ts_data *cts_data;
#endif
    struct input_dev *pen_dev = pdata->pen_input_dev;
    struct cts_touchinfo_stylus *stylus_info;
    struct cts_touch_msg_stylus *msgs;
    int stylus_num;
    int i;
    int contact = 0;

    stylus_info = (struct cts_touchinfo_stylus *)touch_info;
    stylus_num = stylus_info->stylus_info.num;
    msgs = stylus_info->msg;
#if defined CFG_CTS_FORCE_UP || defined CFG_CTS_HEARTBEAT_MECHANISM
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
#endif
    cts_dbg("Process stylus %d msgs", stylus_num);
    if (stylus_num == 0 || stylus_num > CFG_CTS_MAX_STYLUS_NUM)
        return 0;

    for (i = 0; i < stylus_num; i++) {
        u16 x, y, p;

        x = le16_to_cpu(msgs[i].x_l | (msgs[i].xy_h.x_h << 8));
        y = le16_to_cpu(msgs[i].y_l | (msgs[i].xy_h.y_h << 8));
        p = le16_to_cpu(msgs[i].pressure_l | (msgs[i].key_info.pressure_h << 8));

        cts_dbg("  Process stylus msg[%d]: id[%u] ev=%u x=%u y=%u p=%u"
            " tx=%d ty=%d btn0=%d btn1=%d btn2=%d",
            i, msgs[i].id_info.id, msgs[i].id_info.event, x, y, p, msgs[i].tilt_x, msgs[i].tilt_y,
            msgs[i].key_info.key0, msgs[i].key_info.key1, msgs[i].key_info.key2);

        /* Report stylus button */
        input_report_key(pen_dev, BTN_STYLUS, msgs[i].key_info.key0);
        input_report_key(pen_dev, BTN_STYLUS2, msgs[i].key_info.key1);

        switch (msgs[i].id_info.event) {
        case CTS_TOUCH_EVENT_DOWN:
        case CTS_TOUCH_EVENT_MOVE:
        case CTS_TOUCH_EVENT_STAY:
            contact++;
            input_report_abs(pen_dev, ABS_X, x);
            input_report_abs(pen_dev, ABS_Y, y);
            input_report_abs(pen_dev, ABS_PRESSURE, p);
            input_report_abs(pen_dev, ABS_TILT_X, msgs[i].tilt_x);
            input_report_abs(pen_dev, ABS_TILT_Y, msgs[i].tilt_y);
            break;

        case CTS_TOUCH_EVENT_UP:
            break;
        default:
            cts_warn("Process stylus msg with unknwon event %u id %u",
                    msgs[i].id_info.event, msgs[i].id_info.id);
            break;
        }
    }

    input_report_key(pen_dev, BTN_TOUCH, contact > 0);
    input_report_key(pen_dev, BTN_TOOL_PEN, contact > 0);
    input_sync(pen_dev);

#ifdef CFG_CTS_FORCE_UP
    if (contact) {
        if (delayed_work_pending(&pdata->touch_event_timeout_work)) {
            mod_delayed_work(cts_data->workqueue,
                    &pdata->touch_event_timeout_work, msecs_to_jiffies(100));
        } else {
            queue_delayed_work(cts_data->workqueue,
                    &pdata->touch_event_timeout_work, msecs_to_jiffies(100));
        }
    } else {
        cancel_delayed_work_sync(&pdata->touch_event_timeout_work);
    }
#endif

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    if (contact) {
        if (delayed_work_pending(&cts_data->heart_work)) {
            mod_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        } else {
            queue_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        }
    }
#endif

    return 0;
}

int cts_plat_process_finger_stylus(struct cts_platform_data *pdata,
        void *touch_info)
{
#if defined CFG_CTS_FORCE_UP || defined CFG_CTS_HEARTBEAT_MECHANISM
    struct chipone_ts_data *cts_data;
#endif
    struct input_dev *input_dev = pdata->ts_input_dev;
    struct input_dev *pen_dev = pdata->pen_input_dev;
    struct cts_touchinfo_finger_stylus *finger_stylus_info;
    struct cts_touch_msg_finger *finger_msg;
    struct cts_touch_msg_stylus *stylus_msg;
    int stylus_num;
    int finger_num;
    int i;
    int contact = 0;
#ifdef CONFIG_CTS_SLOTPROTOCOL
    static unsigned char finger_last[CFG_CTS_MAX_TOUCH_NUM] = { 0 };
#endif
    unsigned char finger_current[CFG_CTS_MAX_TOUCH_NUM] = { 0 };

    finger_stylus_info = (struct cts_touchinfo_finger_stylus *)touch_info;
    finger_msg = finger_stylus_info->finger_msg;
    finger_num = finger_stylus_info->header.proto_info.finger_num;
    stylus_msg = finger_stylus_info->stylus_msg;
    stylus_num = finger_stylus_info->stylus_info.num;

    cts_dbg("Process touch %d msgs, stylus %d msgs", finger_num, stylus_num);
#if defined CFG_CTS_FORCE_UP || defined CFG_CTS_HEARTBEAT_MECHANISM
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
#endif

    if (finger_num == 0 || finger_num > CFG_CTS_MAX_TOUCH_NUM) {
        goto process_stylus;
    }

    for (i = 0; i < finger_num; i++) {
        u16 x, y;

        x = le16_to_cpu(finger_msg[i].x_l | (finger_msg[i].xy_h.x_h << 8));
        y = le16_to_cpu(finger_msg[i].y_l | (finger_msg[i].xy_h.y_h << 8));

#ifdef CFG_CTS_SWAP_XY
        swap(x, y);
#endif /* CFG_CTS_SWAP_XY */
#ifdef CFG_CTS_WRAP_X
        x = wrap(pdata->res_x, x);
#endif /* CFG_CTS_WRAP_X */
#ifdef CFG_CTS_WRAP_Y
        y = wrap(pdata->res_y, y);
#endif /* CFG_CTS_WRAP_Y */

        cts_dbg("  Process touch finger_msg[%d]: id[%u] ev=%u x=%u y=%u p=%u",
            i, finger_msg[i].id_info.id, finger_msg[i].id_info.event, x, y, finger_msg[i].pressure);
        if (finger_msg[i].id_info.event == CTS_TOUCH_EVENT_DOWN
        || finger_msg[i].id_info.event == CTS_TOUCH_EVENT_MOVE
        || finger_msg[i].id_info.event == CTS_TOUCH_EVENT_STAY) {
            if (finger_msg[i].id_info.id < CFG_CTS_MAX_TOUCH_NUM)
                finger_current[finger_msg[i].id_info.id] = 1;
        }
#ifdef CONFIG_CTS_SLOTPROTOCOL
        /* input_mt_slot(input_dev, finger_msg[i].id_info.id); */
        switch (finger_msg[i].id_info.event) {
        case CTS_TOUCH_EVENT_DOWN:
#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
            TPD_DEBUG_SET_TIME;
            TPD_EM_PRINT(x, y, x, y, finger_msg[i].id_info.id, 1);
            tpd_history_x = x;
            tpd_history_y = y;
#ifdef CONFIG_MTK_BOOT
            if (tpd_dts_data.use_tpd_button) {
                if (FACTORY_BOOT == get_boot_mode() ||
                    RECOVERY_BOOT == get_boot_mode())
                    tpd_button(x, y, 1);
            }
#endif /* CONFIG_MTK_BOOT */
#endif /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */
        case CTS_TOUCH_EVENT_MOVE:
        case CTS_TOUCH_EVENT_STAY:
            contact++;
            input_mt_slot(input_dev, finger_msg[i].id_info.id);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
            input_report_abs(input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, finger_msg[i].pressure);
            input_report_abs(input_dev, ABS_MT_PRESSURE, finger_msg[i].pressure);
            break;

        case CTS_TOUCH_EVENT_UP:
#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
            TPD_DEBUG_SET_TIME;
            TPD_EM_PRINT(tpd_history_x, tpd_history_y, tpd_history_x, tpd_history_y, finger_msg[i].id_info.id, 0);
            tpd_history_x = 0;
            tpd_history_y = 0;
#ifdef CONFIG_MTK_BOOT
            if (tpd_dts_data.use_tpd_button) {
                if (FACTORY_BOOT == get_boot_mode() ||
                    RECOVERY_BOOT == get_boot_mode())
                    tpd_button(0, 0, 0);
            }
#endif /* CONFIG_MTK_BOOT */
#endif /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */
            break;
        default:
            cts_warn("Process touch msg with unknwon event %u id %u",
                    finger_msg[i].id_info.event, finger_msg[i].id_info.id);
            break;
        }
#else /* CONFIG_CTS_SLOTPROTOCOL */
    /**
    * If the driver reports one of BTN_TOUCH or ABS_PRESSURE
    * in addition to the ABS_MT events, the last SYN_MT_REPORT event
    * may be omitted. Otherwise, the last SYN_REPORT will be dropped
    * by the input core, resulting in no zero-contact event
    * reaching userland.
    */
        switch (finger_msg[i].id_info.event) {
        case CTS_TOUCH_EVENT_DOWN:
        case CTS_TOUCH_EVENT_MOVE:
        case CTS_TOUCH_EVENT_STAY:
            contact++;
            input_report_abs(input_dev, ABS_MT_PRESSURE, finger_msg[i].pressure);
//            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, finger_msg[i].pressure); //no major data reported
            input_report_key(input_dev, BTN_TOUCH, 1);
            input_report_abs(input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
            input_mt_sync(input_dev);
            break;

        case CTS_TOUCH_EVENT_UP:
            break;
        default:
            cts_warn("Process touch msg with unknwon event %u id %u",
                    finger_msg[i].id_info.event, finger_msg[i].id_info.id);
            break;
        }
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    }

#ifdef CONFIG_CTS_SLOTPROTOCOL
    for (i = 0; i < CFG_CTS_MAX_TOUCH_NUM; i++) {
        if (finger_last[i] != 0 && finger_current[i] == 0) {
            input_mt_slot(input_dev, i);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
        }
        finger_last[i] = finger_current[i];
    }
    input_report_key(input_dev, BTN_TOUCH, contact > 0);
#else
    if (contact == 0) {
        input_report_key(input_dev, BTN_TOUCH, 0);
        input_mt_sync(input_dev);
    }
#endif
    input_sync(input_dev);

process_stylus:
    if (stylus_num == 0 || stylus_num > CFG_CTS_MAX_STYLUS_NUM) {
        return 0;
    }

    for (i = 0; i < stylus_num; i++) {
        u16 x, y, p;

        x = le16_to_cpu((stylus_msg[i].x_l) | (stylus_msg[i].xy_h.x_h << 8));
        y = le16_to_cpu((stylus_msg[i].y_l) | (stylus_msg[i].xy_h.y_h << 8));
        p = le16_to_cpu(stylus_msg[i].pressure_l |
                (stylus_msg[i].key_info.pressure_h << 8));

        cts_dbg("  Process stylus msg[%d]: id[%u] ev=%u x=%u y=%u p=%u"
            " tx=%d ty=%d btn0=%d btn1=%d btn2=%d",
            i, stylus_msg[i].id_info.id, stylus_msg[i].id_info.event, x, y, p, stylus_msg[i].tilt_x,
            stylus_msg[i].tilt_y, stylus_msg[i].key_info.key0,
            stylus_msg[i].key_info.key1, stylus_msg[i].key_info.key2);

        /* Report stylus button */
        input_report_key(pen_dev, BTN_STYLUS, stylus_msg[i].key_info.key0);
        input_report_key(pen_dev, BTN_STYLUS2, stylus_msg[i].key_info.key1);

        switch (stylus_msg[i].id_info.event) {
        case CTS_TOUCH_EVENT_DOWN:
        case CTS_TOUCH_EVENT_MOVE:
        case CTS_TOUCH_EVENT_STAY:
            contact++;
            input_report_abs(pen_dev, ABS_X, x);
            input_report_abs(pen_dev, ABS_Y, y);
            input_report_abs(pen_dev, ABS_PRESSURE, p);
            input_report_abs(pen_dev, ABS_TILT_X, stylus_msg[i].tilt_x);
            input_report_abs(pen_dev, ABS_TILT_Y, stylus_msg[i].tilt_y);
            break;

        case CTS_TOUCH_EVENT_UP:
            break;
        default:
            cts_warn("Process stylus msg with unknwon event %u id %u",
                    stylus_msg[i].id_info.event, stylus_msg[i].id_info.id);
            break;
        }
    }

    input_report_key(pen_dev, BTN_TOUCH, contact > 0);
    input_report_key(pen_dev, BTN_TOOL_PEN, contact > 0);
    input_sync(pen_dev);


#ifdef CFG_CTS_FORCE_UP
    if (contact) {
        if (delayed_work_pending(&pdata->touch_event_timeout_work)) {
            mod_delayed_work(cts_data->workqueue,
                    &pdata->touch_event_timeout_work, msecs_to_jiffies(100));
        } else {
            queue_delayed_work(cts_data->workqueue,
                    &pdata->touch_event_timeout_work, msecs_to_jiffies(100));
        }
    } else {
        cancel_delayed_work_sync(&pdata->touch_event_timeout_work);
    }
#endif

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    if (contact) {
        if (delayed_work_pending(&cts_data->heart_work)) {
            mod_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        } else {
            queue_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        }
    }
#endif

    return 0;
}

int cts_plat_process_touch_msg_legacy(struct cts_platform_data *pdata,
        void *touch_info)
{
#if defined CFG_CTS_FORCE_UP || defined CFG_CTS_HEARTBEAT_MECHANISM
    struct chipone_ts_data *cts_data;
#endif
    struct input_dev *input_dev = pdata->ts_input_dev;
    struct cts_touch_info_legacy *info;
    int finger_num;
    int i;
    int contact = 0;
#ifdef CONFIG_CTS_SLOTPROTOCOL
    static unsigned char finger_last[CFG_CTS_MAX_TOUCH_NUM] = { 0 };
#endif
    unsigned char finger_current[CFG_CTS_MAX_TOUCH_NUM] = { 0 };

#if defined CFG_CTS_FORCE_UP || defined CFG_CTS_HEARTBEAT_MECHANISM
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
#endif
    info = (struct cts_touch_info_legacy *)touch_info;
    finger_num = info->header.proto_info.finger_num;

    cts_dbg("legacy:Process touch %d msgs", finger_num);
    if (finger_num == 0 || finger_num > CFG_CTS_MAX_TOUCH_NUM)
        return 0;

    for (i = 0; i < finger_num; i++) {
        u16 x, y;

        x = le16_to_cpu(info->msgs[i].x);
        y = le16_to_cpu(info->msgs[i].y);

#ifdef CFG_CTS_SWAP_XY
        swap(x, y);
#endif /* CFG_CTS_SWAP_XY */
#ifdef CFG_CTS_WRAP_X
        x = wrap(pdata->res_x, x);
#endif /* CFG_CTS_WRAP_X */
#ifdef CFG_CTS_WRAP_Y
        y = wrap(pdata->res_y, y);
#endif /* CFG_CTS_WRAP_Y */
        cts_dbg("  Process touch msg[%d]: id[%u] ev=%u x=%u y=%u p=%u",
            i, info->msgs[i].id_info.id, info->msgs[i].event, x, y, info->msgs[i].pressure);
        if (info->msgs[i].event == CTS_TOUCH_EVENT_DOWN
        || info->msgs[i].event == CTS_TOUCH_EVENT_MOVE
        || info->msgs[i].event == CTS_TOUCH_EVENT_STAY) {
            if (info->msgs[i].id_info.id < CFG_CTS_MAX_TOUCH_NUM)
                finger_current[info->msgs[i].id_info.id] = 1;
        }
#ifdef CONFIG_CTS_SLOTPROTOCOL
        /* input_mt_slot(input_dev, info->msgs[i].id); */
        switch (info->msgs[i].event) {
        case CTS_TOUCH_EVENT_DOWN:
        case CTS_TOUCH_EVENT_MOVE:
        case CTS_TOUCH_EVENT_STAY:
            contact++;
            input_mt_slot(input_dev, info->msgs[i].id);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
            input_report_abs(input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, info->msgs[i].pressure);
            input_report_abs(input_dev, ABS_MT_PRESSURE, info->msgs[i].pressure);
            break;

        case CTS_TOUCH_EVENT_UP:
            /* input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false); */
            break;

        default:
            cts_warn("Process touch msg with unknwon event %u id %u",
                    info->msgs[i].event, info->msgs[i].id);
            break;
        }
#else /* CONFIG_CTS_SLOTPROTOCOL */
    /**
    * If the driver reports one of BTN_TOUCH or ABS_PRESSURE
    * in addition to the ABS_MT events, the last SYN_MT_REPORT event
    * may be omitted. Otherwise, the last SYN_REPORT will be dropped
    * by the input core, resulting in no zero-contact event
    * reaching userland.
    */
        switch (info->msgs[i].event) {
        case CTS_TOUCH_EVENT_DOWN:
        case CTS_TOUCH_EVENT_MOVE:
        case CTS_TOUCH_EVENT_STAY:
            contact++;
            input_report_abs(input_dev, ABS_MT_PRESSURE, info->msgs[i].pressure);
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, info->msgs[i].pressure);
            input_report_key(input_dev, BTN_TOUCH, 1);
            input_report_abs(input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
            input_mt_sync(input_dev);
            break;

        case CTS_TOUCH_EVENT_UP:
            break;
        default:
            cts_warn("Process touch msg with unknwon event %u id %u",
                    info->msgs[i].event, info->msgs[i].id);
            break;
        }
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    }

#ifdef CONFIG_CTS_SLOTPROTOCOL
    for (i = 0; i < CFG_CTS_MAX_TOUCH_NUM; i++) {
        if (finger_last[i] != 0 && finger_current[i] == 0) {
            input_mt_slot(input_dev, i);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
        }
        finger_last[i] = finger_current[i];
    }
    input_report_key(input_dev, BTN_TOUCH, contact > 0);
#else
    if (contact == 0) {
        input_report_key(input_dev, BTN_TOUCH, 0);
        input_mt_sync(input_dev);
    }
#endif
    input_sync(input_dev);
#ifdef CFG_CTS_FORCE_UP
    if (contact) {
        if (delayed_work_pending(&pdata->touch_event_timeout_work)) {
            mod_delayed_work(cts_data->workqueue,
                    &pdata->touch_event_timeout_work, msecs_to_jiffies(100));
        } else {
            queue_delayed_work(cts_data->workqueue,
                    &pdata->touch_event_timeout_work, msecs_to_jiffies(100));
        }
    } else {
        cancel_delayed_work_sync(&pdata->touch_event_timeout_work);
    }
#endif

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    if (contact) {
        if (delayed_work_pending(&cts_data->heart_work)) {
            mod_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        } else {
            queue_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        }
    }
#endif

    return 0;
}

int cts_plat_release_all_touch(struct cts_platform_data *pdata)
{
    struct input_dev *input_dev = pdata->ts_input_dev;

#if defined(CONFIG_CTS_SLOTPROTOCOL)
    int id;
#endif /* CONFIG_CTS_SLOTPROTOCOL */

    cts_info("Release all touch");

#ifdef CONFIG_CTS_SLOTPROTOCOL
    for (id = 0; id < CFG_CTS_MAX_TOUCH_NUM; id++) {
        input_mt_slot(input_dev, id);
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
    }
    input_report_key(input_dev, BTN_TOUCH, 0);
#else
    input_report_key(input_dev, BTN_TOUCH, 0);
    input_mt_sync(input_dev);
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    input_sync(input_dev);

    return 0;
}

#ifdef CONFIG_CTS_VIRTUALKEY
int cts_plat_init_vkey_device(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Init VKey");

    pdata->vkey_state = 0;

    for (i = 0; i < pdata->vkey_num; i++) {
        input_set_capability(pdata->ts_input_dev,
                EV_KEY, pdata->vkey_keycodes[i]);
    }

    return 0;
}

void cts_plat_deinit_vkey_device(struct cts_platform_data *pdata)
{
    cts_info("De-init VKey");

    pdata->vkey_state = 0;
}

int cts_plat_process_vkey(struct cts_platform_data *pdata, u8 vkey_state)
{
    u8 event;
    int i;

    event = pdata->vkey_state ^ vkey_state;

    cts_dbg("Process vkey state=0x%02x, event=0x%02x", vkey_state, event);

    for (i = 0; i < pdata->vkey_num; i++) {
        input_report_key(pdata->ts_input_dev, pdata->vkey_keycodes[i],
                vkey_state & BIT(i) ? 1 : 0);
    }

    pdata->vkey_state = vkey_state;

    return 0;
}

int cts_plat_release_all_vkey(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Release all vkeys");

    for (i = 0; i < pdata->vkey_num; i++) {
        if (pdata->vkey_state & BIT(i)) {
            input_report_key(pdata->ts_input_dev, pdata->vkey_keycodes[i], 0);
        }
    }

    pdata->vkey_state = 0;

    return 0;
}
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CFG_CTS_GESTURE
int cts_plat_enable_irq_wake(struct cts_platform_data *pdata)
{
    int ret;

    cts_dbg("Enable IRQ wake");

    if (pdata->irq > 0) {
        ret = enable_irq_wake(pdata->irq);
        if (ret < 0) {
            cts_err("Enable irq wake failed");
            return -EINVAL;
        }
        pdata->irq_wake_enabled = true;
        return 0;
    }

    cts_warn("Enable irq wake while irq invalid %d", pdata->irq);
    return -ENODEV;
}

int cts_plat_disable_irq_wake(struct cts_platform_data *pdata)
{
    int ret;

    cts_dbg("Disable IRQ wake");

    if (pdata->irq > 0) {
        ret = disable_irq_wake(pdata->irq);
        if (ret < 0) {
            cts_warn("Disable irq wake while already disabled");
            return -EINVAL;
        }
        pdata->irq_wake_enabled = false;
        return 0;
    }

    cts_warn("Disable irq wake while irq invalid %d", pdata->irq);
    return -ENODEV;
}

int cts_plat_init_gesture(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Init gesture");

    /* TODO: If system will issure enable/disable command, comment following line. */
    /* cts_enable_gesture_wakeup(pdata->cts_dev); */

    for (i = 0; i < pdata->gesture_num; i++) {
        input_set_capability(pdata->ts_input_dev, EV_KEY,
                pdata->gesture_keymap[i][1]);
    }

    return 0;
}

void cts_plat_deinit_gesture(struct cts_platform_data *pdata)
{
    cts_info("De-init gesture");
}

int cts_plat_process_gesture_info(struct cts_platform_data *pdata,
        struct cts_touch_info_header *touch_header)
{

    /*BSP: modify for transsion core by guoxin.zhu 20230613 start*/
    struct tran_core_ops *ops = NULL;

    cts_info("Process gesture, id=0x%02x", touch_header->gesture_info.gesture_id);

    if(get_tpd_ops(&chipone_controller, &ops)) {
        if (0 > ops->gesture_id_report(touch_header->gesture_info.gesture_id,&chipone_controller)) {
            cts_err("[GESTURE]gesture touch data report fail");
            return EINVAL;
        }
    }

    return 0;
	
#if 0
    int i;

    cts_dbg("Process gesture, id=0x%02x", touch_header->gesture_info.gesture_id);

#if defined(CFG_CTS_GESTURE_REPORT_KEY)
    for (i = 0; i < CFG_CTS_NUM_GESTURE; i++) {
        if (touch_header->gesture_info.gesture_id == pdata->gesture_keymap[i][0]) {
            cts_info("Report key[%u]", pdata->gesture_keymap[i][1]);
            input_report_key(pdata->ts_input_dev, pdata->gesture_keymap[i][1], 1);
            input_sync(pdata->ts_input_dev);

            input_report_key(pdata->ts_input_dev, pdata->gesture_keymap[i][1], 0);
            input_sync(pdata->ts_input_dev);

            return 0;
        }
    }
#endif /* CFG_CTS_GESTURE_REPORT_KEY */

    cts_warn("Process unrecognized gesture id=%u", touch_header->gesture_info.gesture_id);

    return -EINVAL;
#endif
    /*BSP: modify for transsion core by guoxin.zhu 20230613 end*/
}
#endif /* CFG_CTS_GESTURE */


int cts_plat_process_touch_info(struct cts_device *cts_dev, void *touch_info)
{
    struct cts_touch_info_header *touch_header;
    struct cts_platform_data *pdata = cts_dev->pdata;
    //u8 has_ps_info = 0;
    enum cts_tp_protocols tp_protocol;
    int ret = -EINVAL;

    touch_header = (struct cts_touch_info_header *) touch_info;
    tp_protocol = touch_header->proto_info.protocol;

    switch (tp_protocol) {
        case CTS_TP_LEGACY:
            //has_ps_info = touch_header->gesture_info.has_ps_info;
            pdata->process_touch_msg = cts_plat_process_touch_msg_legacy;
            break;
        case CTS_TP_FINGER_ONLY:
            pdata->process_touch_msg = cts_plat_process_finger_msg;
            break;
        case CTS_TP_STYLUS_ONLY:
            pdata->process_touch_msg = cts_plat_process_stylus_msg;
            break;
        case CTS_TP_FINGER_STYLUS:
            pdata->process_touch_msg = cts_plat_process_finger_stylus;
            break;
        default:
            cts_err("unkown protocol:%d!\n", tp_protocol);
            return ret;
    }

    if (unlikely(cts_dev->rtdata.suspended)) {
#ifdef CFG_CTS_GESTURE
        if (cts_dev->rtdata.gesture_wakeup_enabled) {

            ret = cts_plat_process_gesture_info(pdata, touch_header);

            if (cts_dev->fwdata.int_data_method != INT_DATA_METHOD_HOST) {
                if (ret)
                    cts_err("Process gesture info failed %d", ret);

                ret = cts_tcs_set_pwr_mode(cts_dev, PWR_MODE_GSTR_WAKEUP);
                if (ret)
                    cts_warn("Reenter suspend with gesture wakeup failed %d", ret);

                if (cts_tcs_clr_gstr_ready_flag(cts_dev))
                    cts_err("Clear gesture ready flag failed");
            }
        } else {
            cts_warn("IRQ triggered while device suspended "
                "without gesture wakeup enable");
        }
#endif /* CFG_CTS_GESTURE */
    } else {

#ifdef CFG_CTS_PALM_DETECT
        if (CFG_CTS_PALM_ID == touch_header->vkey_state) {
            cts_report_palm_event(pdata);
            cts_plat_release_all_touch(pdata);
            return 0;
        }
#endif

    //BSP:add for pocket mode by xihao.ding 20251129 start
    if (cts_dev->pdata->pocket_function_support && cts_dev->is_pocket_in) {
        cts_dbg("enter touch palm");
        if(CTS_CMD_PALM_DETECTED == touch_header->vkey_state && !cts_dev->pdata->key_status_pocket){
           input_report_key(cts_dev->pdata->ts_input_dev, KEY_LARGE_AREA, 1);
           input_sync(cts_dev->pdata->ts_input_dev);
           cts_dev->pdata->key_status_pocket = true;
           cts_info("pocket key report 251 1\n");
        } else if(CTS_CMD_PALM_DETECTED != touch_header->vkey_state && cts_dev->pdata->key_status_pocket) {
           input_report_key(cts_dev->pdata->ts_input_dev, KEY_LARGE_AREA, 0);
           input_sync(cts_dev->pdata->ts_input_dev);
           cts_dev->pdata->key_status_pocket = false;
           cts_info("pocket key report 251 0\n");
       }
    }
    //BSP:add for pocket mode by xihao.ding 20251129 end

#ifdef CONFIG_CTS_TP_PROXIMITY
        if (cts_is_proximity_enable(cts_dev)) {
            cts_dbg("proximity status:%d", cts_dev->rtdata.proximity_status);
            if (!cts_dev->rtdata.proximity_status
            && touch_header->vkey_state == CTS_CMD_PROXIMITY_STATUS) {
                cts_dev->rtdata.proximity_num++;
                if (cts_dev->rtdata.proximity_num == 4) {
                    cts_dev->rtdata.proximity_num = 0;
                    cts_dev->rtdata.proximity_status = true;
                    cts_handle_proximity_event(true);
                    return 0;
                }
            } else if (cts_dev->rtdata.proximity_status
            && (touch_header->vkey_state == 0)) {
                cts_dev->rtdata.proximity_num = 0;
                cts_dev->rtdata.proximity_status = false;
                cts_handle_proximity_event(false);
                cts_plat_release_all_touch(pdata);
                return 0;
            }
        }
#endif

        ret = pdata->process_touch_msg(pdata, touch_info);
        if (ret) {
            cts_err("Process touch msg failed %d", ret);
            return ret;
        }
#ifdef CONFIG_CTS_VIRTUALKEY
        cts_dbg("Touch info: vkey_state %x, finger_num %u",
            touch_header->vkey_state, touch_header->proto_info.finger_num);
        ret = cts_plat_process_vkey(pdata, touch_header->vkey_state);
        if (ret) {
            cts_err("Process vkey failed %d", ret);
            return ret;
        }
#endif
    }

    return ret;
}
