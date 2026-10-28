//add by bacon for fp_wake_lock
#define silead_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0664,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

silead_attr(fp_wake_lock);
silead_attr(fp_wake_unlock);
silead_attr(fp_ext_timer);

static struct attribute * g[] = {
	&fp_wake_lock_attr.attr,
	&fp_wake_unlock_attr.attr,
	&fp_ext_timer_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

struct wakelock
{
	char			*name;
	struct wakeup_source	ws;
};
static struct wakelock * g_wakelock_list[10] = {0};

static DEFINE_MUTEX(wakelocks_lock);
static ssize_t fp_wake_lock_show(struct kobject *kobj,struct kobj_attribute *attr,char *buf)
{
	//return pm_show_wakelocks(buf, true);

	int i;
	char *str = buf;
	char *end = buf + PAGE_SIZE;

	mutex_lock(&wakelocks_lock);
    
	for (i = 0; i < 10; i++) {
		if (g_wakelock_list[i] != NULL) {
	        str += scnprintf(str, end - str, "%s ", g_wakelock_list[i]->name);
        }
	}
	if (str > buf)
		str--;

	str += scnprintf(str, end - str, "\n");

	mutex_unlock(&wakelocks_lock);
	return (str - buf);
}

static ssize_t fp_wake_lock_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t n)
{
//	int error = pm_wake_lock(buf);
//	return error ? error : n;

	int i, j;
	int ret = -1;
    char * wl_name;
    struct wakelock *wl;

	wl_name = kstrndup(buf, n, GFP_KERNEL);
	if (!wl_name) {
		return -ENOMEM;
	}

	mutex_lock(&wakelocks_lock);
	for (j = 0; j < 10; j++) {
		if (g_wakelock_list[j] != NULL) {
			if (strcmp(g_wakelock_list[j]->name,buf) == 0) {
                wl = g_wakelock_list[j];
				ret = n;
				break;
			}
		}
	}

    if (j == 10) {
		wl = kzalloc(sizeof(*wl), GFP_KERNEL);
		if (!wl)
			return -ENOMEM;

		wl->name = wl_name;
		wl->ws.name = wl_name;
		wakeup_source_add(&wl->ws);

       	for (i = 0; i < 10; i++) {
			if (g_wakelock_list[i] == NULL) {
				g_wakelock_list[i] = wl;
				ret = n;
				break;
			}
		}
    }

    __pm_stay_awake(&wl->ws);
	mutex_unlock(&wakelocks_lock);

    TRAN_KDBG("fp_wake_lock_store ret = %d\n", ret);
	return ret;
}

static ssize_t fp_wake_unlock_show(struct kobject *kobj,struct kobj_attribute *attr,char *buf)
{
	//return pm_show_wakelocks(buf, fasle);
	int i;
	char *str = buf;
	char *end = buf + PAGE_SIZE;

	mutex_lock(&wakelocks_lock);
    
	for (i = 0; i < 10; i++) {
		if (g_wakelock_list[i] != NULL) {
			str += scnprintf(str, end - str, "%s ", g_wakelock_list[i]->name);
		}
	}
	if (str > buf)
		str--;

	str += scnprintf(str, end - str, "\n");

	mutex_unlock(&wakelocks_lock);
	return (str - buf);
}

static ssize_t fp_wake_unlock_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t n)
{
//	int error = pm_wake_unlock(buf);
//	return error ? error : n;

	struct wakelock *wl;
	int ret = -1;
	int i;
	mutex_lock(&wakelocks_lock);

	for (i = 0; i < 10; i++) {
		if (g_wakelock_list[i] != NULL) {
			if (strcmp(g_wakelock_list[i]->name,buf) == 0) {
				wl = g_wakelock_list[i];
                __pm_relax(&wl->ws);
				wakeup_source_remove(&wl->ws);
				kfree(wl->name);
				kfree(wl);
				g_wakelock_list[i] = NULL;
				ret = n;
				break;
			}
		}
	}

	mutex_unlock(&wakelocks_lock);
    TRAN_KDBG("fp_wake_unlock_store ret = %d\n", ret);
	return ret;
}
//add by bacon for fp_wake_unlock

//add by matthew
static char g_ext_timer_stamp[64 + 1] = "";
static ssize_t fp_ext_timer_show(struct kobject *kobj,struct kobj_attribute *attr,char *buf)
{
	int n = strlen(g_ext_timer_stamp);
	char *str = buf;
    
	scnprintf(str, PAGE_SIZE - 2, "%s", g_ext_timer_stamp);
	str[PAGE_SIZE - 2] = '\n';
	str[PAGE_SIZE - 1] = '\n';

	return n;
}

static ssize_t fp_ext_timer_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t n)
{
	struct spidev_data*		spidev = fp_spidev;

	int ret = -1;
	int t_n = n;

	if (t_n > 64) {
		t_n = 64;
	}
	scnprintf(g_ext_timer_stamp, t_n, "%s", buf);
	g_ext_timer_stamp[t_n] = 0;

	if (strncmp("deadic_chktimer",buf, 15) == 0) {
		char*	env_ext_forged[2] = {"SILEAD_FP_EVENT=IRQ_FORGED", NULL};
		kobject_uevent_env(&spidev->spi->dev.kobj, KOBJ_CHANGE, env_ext_forged); 	
		ret = n;
		//for user space's poll/select,
		//g_poll_event_mask |= POLLERR;
		//wake_up_interruptible(&sileadfp_pollerr_waitq);
	}

    TRAN_KDBG("fp_ext_timer_store ret = %d\n", ret);
	return ret;
}

int slfp_ext_timer_fire(const char *cmdstr)
{
	return fp_ext_timer_store(NULL,NULL,cmdstr, strlen(cmdstr));
}


#ifndef SLPT
static int gsl_fp_rdinit(struct spidev_data *spidev, unsigned char reg)
{
    uint8_t tx[] = {
        reg, SL_READ,
    };
    unsigned rx[ARRAY_SIZE(tx)] = {0};
    struct spi_message	m;
    struct spi_transfer	t = {
        .rx_buf		= rx,
        .tx_buf		= tx,
        .len		= ARRAY_SIZE(tx),
        .bits_per_word = SPI_BITS,
        .delay_usecs = SPI_DELAY,
        .speed_hz = SPI_SPEED,
    };

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return spidev_sync(spidev, &m);
}
#endif

#ifdef LSB_TO_MSB
static const unsigned char reverse_table256[] = {
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,   
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,   
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,   
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,   
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,   
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,  
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,   
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,  
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,  
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,   
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,  
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,  
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,   
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,  
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,   
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF  
};  

static void Modifybuf(u8 *buf, size_t len)
{
    int i;

    for (i = 0; i < len; i++) {
        buf[i] = reverse_table256[buf[i]];
    }
}
#endif

#ifndef SLPT
static void spidev_complete(void *arg)
{
    complete(arg);
}
#endif

#ifndef SLPT
ssize_t spidev_sync(struct spidev_data *spidev, struct spi_message *message)
{
    DECLARE_COMPLETION_ONSTACK(done);
    int status;

#ifdef LSB_TO_MSB
    struct list_head *p;
    struct spi_transfer *t;

    list_for_each(p, &message->transfers)
    {
        //pr_err("%s:%d\n", __func__, __LINE__);
        t = list_entry(p, struct spi_transfer, transfer_list);
        if (t->tx_buf) {
            Modifybuf((u8*)t->tx_buf, t->len);
        }
    }
#endif

    message->complete = spidev_complete;
    message->context = &done;

    if (spidev->spi == NULL) {
        status = -ESHUTDOWN;
    } else {
        spin_lock_irq(&spidev->spi_lock);
        status = spi_async(spidev->spi, message);
        spin_unlock_irq(&spidev->spi_lock);
		
    }
  
    if (status == 0) {
        //wait_for_completion(&done);------> deadlock
        /*unsigned long ret = */wait_for_completion_timeout(&done, msecs_to_jiffies(3000)); 
        status = message->status;
		
        if (status == 0)
            status = message->actual_length;

#ifdef LSB_TO_MSB
        list_for_each(p, &message->transfers)
        {
            //pr_err("%s:%d\n", __func__, __LINE__);
            t = list_entry(p, struct spi_transfer, transfer_list);
            if (t->rx_buf) {
                Modifybuf((u8*)t->rx_buf, t->len);
            }
        }
#endif
    }

    return status;
}
#endif

#ifndef SLPT
static union
{
    unsigned char temp_char[4];
    unsigned int get_reg_data;
} temp_data;

unsigned int spidev_read_reg(struct spidev_data *spidev, unsigned char reg)
{
    struct spi_message	m;
	/* Modified by alan for DMA transfer 20171127 begin */
	unsigned char* rx = NULL;
	unsigned char* tx = NULL;
	/* Modified by alan for DMA transfer 20171127 end */
	unsigned char tx_t[] = {
        reg, SL_READ, 0x00, 0x00, 0x00, 0x00
    };

	struct spi_transfer	t = {
		.len		= ARRAY_SIZE(tx_t),
        .bits_per_word = SPI_BITS,
        .delay_usecs = SPI_DELAY,
        .speed_hz = SPI_SPEED,
    };

	/* Modified by alan for DMA transfer 20171127 begin */
	rx = kmalloc(6 * sizeof(unsigned char), GFP_KERNEL);
	if  (!rx) {
		dev_err(&spidev->spi->dev, "alloc rx failed [%s] [%d]\n", __FILE__, __LINE__);
        return -ENOMEM;
	}
	tx = kmalloc(6 * sizeof(unsigned char), GFP_KERNEL);
	if  (!tx) {
		dev_err(&spidev->spi->dev, "alloc tx failed [%s] [%d]\n", __FILE__, __LINE__);
		kfree(rx);
        return -ENOMEM;
	}
	memcpy(tx, tx_t, 6);
	t.rx_buf = rx;
	t.tx_buf = tx;
	/* Modified by alan for DMA transfer 20171127 end */

    if (!(reg > 0x80 && reg < 0x100)) {
        gsl_fp_rdinit(spidev, reg);
    }
    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    spidev_sync(spidev, &m);
    memcpy(temp_data.temp_char, (rx + 2), 4);
	
	/* Modified by alan for DMA transfer 20171127 begin */
	kfree(rx);
	kfree(tx);
	/* Modified by alan for DMA transfer 20171127 end */

    return temp_data.get_reg_data;
}

int spidev_write_reg(struct spidev_data *spidev, unsigned int data, unsigned char reg)
{
    struct spi_message	m;
	int retval = 0;
	/* Modified by alan for DMA transfer 20171127 begin */
	unsigned char* rx = NULL;
	unsigned char* tx = NULL;
	/* Modified by alan for DMA transfer 20171127 end */
	uint8_t rx_t[6] = {0};
	uint8_t tx_t[] = {
        reg, SL_WRITE,
        (data >> 0) & 0xFF,
        (data >> 8) & 0xFF,
        (data >> 16) & 0xFF,
        (data >> 24) & 0xFF,
    };
    struct spi_transfer	t = {
        .len		= ARRAY_SIZE(tx_t),
        .bits_per_word = SPI_BITS,
        .delay_usecs = SPI_DELAY,
        .speed_hz = SPI_SPEED,
    };

	/* Modified by alan for DMA transfer 20171127 begin */
	rx = kmalloc(6 * sizeof(unsigned char), GFP_KERNEL);
	if (!rx) {
		dev_err(&spidev->spi->dev, "alloc rx failed [%s] [%d]\n", __FILE__, __LINE__);
        return -ENOMEM;
	}
	tx = kmalloc(6 * sizeof(unsigned char), GFP_KERNEL);
	if (!tx) {
		dev_err(&spidev->spi->dev, "alloc tx failed [%s] [%d]\n", __FILE__, __LINE__);
		kfree(rx);
        return -ENOMEM;
	}
	memcpy(rx, rx_t, 6);
	memcpy(tx, tx_t, 6);
	t.rx_buf = rx;
	t.tx_buf = tx;
	/* Modified by alan for DMA transfer 20171127 end */

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
	retval = spidev_sync(spidev, &m);
	/* Modified by alan for DMA transfer 20171127 begin */
	kfree(rx);
	kfree(tx);
	/* Modified by alan for DMA transfer 20171127 end */

    return retval;
}

static ssize_t
spidev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct spidev_data	*spidev;
    struct spi_message  m;
    struct spi_transfer t ;	
    ssize_t			status = 0;
    spidev = filp->private_data;
    t.rx_buf         = spidev->buffer;
    t.len            = count;
    /* chipselect only toggles at start or end of operation */
    if (count > bufsiz)
        return -EMSGSIZE;


    mutex_lock(&spidev->buf_lock);
    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    status = spidev_sync(spidev, &m);

    if (status > 0) {
        unsigned long	missing;

        missing = copy_to_user(buf, spidev->buffer, status);
        if (missing == status)
            status = -EFAULT;
        else
            status = status - missing;
    }
    mutex_unlock(&spidev->buf_lock);

    return status;
}

/* Write-only message with current device setup */
static ssize_t
spidev_write(struct file *filp, const char __user *buf,
             size_t count, loff_t *f_pos)
{
    struct spidev_data	*spidev;
    ssize_t			status = 0;
    unsigned long		missing;
  
    /* chipselect only toggles at start or end of operation */
    if (count > bufsiz)
        return -EMSGSIZE;

    spidev = filp->private_data;

    mutex_lock(&spidev->buf_lock);
    missing = copy_from_user(spidev->buffer, buf, count);
    if (missing == 0) {
	struct spi_transfer t = {
        .tx_buf         = spidev->buffer,
        .len            = count,
    	};
    struct spi_message  m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    status = spidev_sync(spidev, &m);

    } else
        status = -EFAULT;
    mutex_unlock(&spidev->buf_lock);

    return status;
}

static int spidev_message(struct spidev_data *spidev,
                          struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
    struct spi_message	msg;
    struct spi_transfer	*k_xfers;
    struct spi_transfer	*k_tmp;
    struct spi_ioc_transfer *u_tmp;
    unsigned		n, total;
    u8			*buf;
    int			status = -EFAULT;
   
    spi_message_init(&msg);
    k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
    if (k_xfers == NULL)
        return -ENOMEM;
	
    buf = spidev->buffer;
    total = 0;
    for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers; n; n--, k_tmp++, u_tmp++) {
        k_tmp->len = u_tmp->len;
        total += k_tmp->len;
        if (total > bufsiz) {
            status = -EMSGSIZE;
            goto done;
        }
        if (u_tmp->rx_buf) {
            k_tmp->rx_buf = buf;
            if (!access_ok(VERIFY_WRITE, (u8 __user *)
                           (uintptr_t) u_tmp->rx_buf,
                           u_tmp->len))
				goto done;
        }
        if (u_tmp->tx_buf) {
            k_tmp->tx_buf = buf;
            if (copy_from_user(buf, (const u8 __user *)(uintptr_t)u_tmp->tx_buf,u_tmp->len))
				goto done;
        }
        buf += k_tmp->len;
        k_tmp->cs_change = !!u_tmp->cs_change;
        k_tmp->bits_per_word = u_tmp->bits_per_word;
        k_tmp->delay_usecs = u_tmp->delay_usecs;
        k_tmp->speed_hz = u_tmp->speed_hz;
#if 0
        TRAN_KDBG("  xfer len %d %s%s%s%dbits %u usec %uHz\n",
                u_tmp->len,
                u_tmp->rx_buf ? "rx " : "",
                u_tmp->tx_buf ? "tx " : "",
                u_tmp->cs_change ? "cs " : "",
                u_tmp->bits_per_word ? : spidev->spi->bits_per_word,
                u_tmp->delay_usecs,
                u_tmp->speed_hz ? : spidev->spi->max_speed_hz);
#endif
        spi_message_add_tail(k_tmp, &msg);
    }
    status = spidev_sync(spidev, &msg);
    if (status < 0) {
        dev_err(&spidev->spi->dev, "spidev sync failed %d\n", status);
        goto done;
    }
    /* copy any rx data out of bounce buffer */
    buf = spidev->buffer;
    for (n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++) {
        if (u_tmp->rx_buf) {
            if (__copy_to_user((u8 __user *)(uintptr_t) u_tmp->rx_buf, buf,u_tmp->len)) {
                status = -EFAULT;
                goto done;
            }
        }
        buf += u_tmp->len;
    }
    status = total;
done:
    kfree(k_xfers);
    return status;
}

void init_frame(struct spidev_data *spidev)
{
    unsigned int ret = 0;
    unsigned long timeout;
// init page point
    spidev_write_reg(spidev, 0x00, 0xBF);
// start scanning
    spidev_write_reg(spidev, (0xFF080024 >> 7), 0xF0);
    spidev_write_reg(spidev, 0x2007FFFF, (0xFF080024 % 0x80));
    /* Wait  2 seconds for scanning done */
    timeout = jiffies + 2 * HZ;
    while (time_before(jiffies, timeout)) {
        ret = spidev_read_reg(spidev, 0xBF);
        dev_dbg(&spidev->spi->dev, "0xBF=0x%02x\n", ret);
        if (ret != 0) {
            break;
        }
        udelay(100);
    }
    dev_dbg(&spidev->spi->dev, "last ret 0xBF=0x%02x\n", ret);
    spidev_write_reg(spidev, 0x00, 0xF0);
    gsl_fp_rdinit(spidev, 0);
}
#endif