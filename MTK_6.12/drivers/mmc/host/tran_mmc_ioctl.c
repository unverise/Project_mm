#include "tran_mmc_ioctl.h"
struct mmc_host *g_mmc;
#define MMC_IOC_FFU_CMD _IOWR(MMC_BLOCK_MAJOR, 3, struct mmc_ffu_ioc_data)
/*
 * There is one mmc_blk_data per slot.
 */
struct mmc_blk_data {
	struct device	*parent;
	struct gendisk	*disk;
	struct mmc_queue queue;
	struct list_head part;
	struct list_head rpmbs;
	unsigned int	flags;
#define MMC_BLK_CMD23	(1 << 0)	/* Can do SET_BLOCK_COUNT for multiblock */
#define MMC_BLK_REL_WR	(1 << 1)	/* MMC Reliable write support */
	unsigned int	usage;
	unsigned int	read_only;
	unsigned int	part_type;
	unsigned int	reset_done;
#define MMC_BLK_READ		BIT(0)
#define MMC_BLK_WRITE		BIT(1)
#define MMC_BLK_DISCARD		BIT(2)
#define MMC_BLK_SECDISCARD	BIT(3)
#define MMC_BLK_CQE_RECOVERY	BIT(4)
	/*
	 * Only set in main mmc_blk_data associated
	 * with mmc_card with dev_set_drvdata, and keeps
	 * track of the current selected device partition.
	 */
	unsigned int	part_curr;
	struct device_attribute force_ro;
	struct device_attribute power_ro_lock;
	int	area_type;
	/* debugfs files (only in main mmc_blk_data) */
	struct dentry *status_dentry;
	struct dentry *ext_csd_dentry;
};
struct mmc_blk_ioc_data {
	struct mmc_ioc_cmd ic;
	unsigned char *buf;
	u64 buf_bytes;
	unsigned int flags;
#define MMC_BLK_IOC_DROP	BIT(0)	/* drop this mrq */
#define MMC_BLK_IOC_SBC	BIT(1)	/* use mrq.sbc */
	struct mmc_rpmb_data *rpmb;
};
static struct mmc_blk_ioc_data *tran_mmc_ioctl_copy_from_user(
	struct mmc_ioc_cmd __user *user)
{
	struct mmc_blk_ioc_data *idata;
	int err;
	idata = kmalloc(sizeof(*idata), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		dev_err(mmc_dev(g_mmc), "%s malloc idata fail!\n", __func__);
		goto out;
	}
	if (copy_from_user(&idata->ic, user, sizeof(idata->ic))) {
		err = -EFAULT;
		dev_err(mmc_dev(g_mmc), "%s copy_from_user fail!\n", __func__);
		goto idata_err;
	}
	idata->buf_bytes = (u64) idata->ic.blksz * idata->ic.blocks;
	if (idata->buf_bytes > MMC_IOC_MAX_BYTES) {
		err = -EOVERFLOW;
		dev_err(mmc_dev(g_mmc), "%s idata->buf_bytes isn't allowd!\n", __func__);
		goto idata_err;
	}
	if (!idata->buf_bytes) {
		idata->buf = NULL;
		dev_err(mmc_dev(g_mmc), "%s idata->buf_bytes is NULL!\n", __func__);
		return idata;
	}
	idata->buf = memdup_user((void __user *)(unsigned long)
				 idata->ic.data_ptr, idata->buf_bytes);
	if (IS_ERR(idata->buf)) {
		err = PTR_ERR(idata->buf);
		dev_err(mmc_dev(g_mmc), "%s memcpy user buffer fail\n", __func__);
		goto idata_err;
	}
	return idata;
idata_err:
	kfree(idata);
out:
	return ERR_PTR(err);
}
static int tran_mmc_ioctl_copy_to_user(struct mmc_ioc_cmd __user *ic_ptr,
				      struct mmc_blk_ioc_data *idata)
{
	struct mmc_ioc_cmd *ic = &idata->ic;
	if (copy_to_user(&(ic_ptr->response), ic->response,
			 sizeof(ic->response))) {
		dev_err(mmc_dev(g_mmc), "%s response copy_to_user fail\n", __func__);
		return -EFAULT;
	}
	if (!idata->ic.write_flag) {
		if (copy_to_user((void __user *)(unsigned long)ic->data_ptr,
				 idata->buf, idata->buf_bytes)) {
			dev_err(mmc_dev(g_mmc), "%s buffer copy_to_user fail\n", __func__);
			return -EFAULT;
		}
	}
	return 0;
}
static int tran_mmc_ioctl_cmd(struct mmc_host *host,
			     struct mmc_ioc_cmd __user *ic_ptr)
{
	struct mmc_blk_data *md;
	struct mmc_blk_ioc_data *idata;
	struct mmc_blk_ioc_data *idatas[1];
	struct mmc_queue *mq;
	struct mmc_card *card = host->card;
	int err = 0, ioc_err = 0;
	struct request *req;
	idata = tran_mmc_ioctl_copy_from_user(ic_ptr);
	if (IS_ERR(idata)) {
		dev_err(mmc_dev(g_mmc), "%s idata is NULL\n", __func__);
		return PTR_ERR(idata);
	}
	if (IS_ERR(card)) {
		dev_err(mmc_dev(g_mmc), "%s card is NULL\n", __func__);
		err = PTR_ERR(card);
		goto cmd_done;
	}
	md = dev_get_drvdata(&card->dev);
	if(IS_ERR(md)) {
		dev_err(mmc_dev(g_mmc), "%s md is NULL\n", __func__);
		err = PTR_ERR(md);
		goto cmd_done;
	}
	mq = &md->queue;
	req = blk_mq_alloc_request(mq->queue,
		idata->ic.write_flag ? REQ_OP_DRV_OUT : REQ_OP_DRV_IN, 0);
	if (IS_ERR(req)) {
		dev_err(mmc_dev(g_mmc), "%s req is NULL\n", __func__);
		err = PTR_ERR(req);
		goto cmd_done;
	}
	idatas[0] = idata;
	req_to_mmc_queue_req(req)->drv_op = MMC_DRV_OP_IOCTL;
	req_to_mmc_queue_req(req)->drv_op_result = -EIO;
	req_to_mmc_queue_req(req)->drv_op_data = idatas;
	req_to_mmc_queue_req(req)->ioc_count = 1;
	blk_execute_rq(req, false);
	ioc_err = req_to_mmc_queue_req(req)->drv_op_result;
	err = tran_mmc_ioctl_copy_to_user(ic_ptr, idata);
	blk_mq_free_request(req);
cmd_done:
	kfree(idata->buf);
	kfree(idata);
	return ioc_err ? ioc_err : err;
}
static int tran_mmc_ioctl_multi_cmd(struct mmc_host *host,
			     struct mmc_ioc_multi_cmd __user *ic_ptr)
{
	struct mmc_blk_data *md;
	struct mmc_blk_ioc_data **idata = NULL;
	struct mmc_ioc_cmd __user *cmds = ic_ptr->cmds;
	struct mmc_card *card = host->card;
	struct mmc_queue *mq;
	int i, err = 0, ioc_err = 0;
	__u64 num_of_cmds;
	struct request *req;
	if (copy_from_user(&num_of_cmds, &ic_ptr->num_of_cmds,
			   sizeof(num_of_cmds))) {
		dev_err(mmc_dev(g_mmc), "%s copy_from_user fail\n", __func__);
		return -EFAULT;
	}
	if (!num_of_cmds)
		return 0;
	if (num_of_cmds > MMC_IOC_MAX_CMDS) {
		dev_err(mmc_dev(g_mmc), "%s num_of_cmds isn't allowd\n", __func__);
		return -EINVAL;
	}
	idata = kcalloc(num_of_cmds, sizeof(*idata), GFP_KERNEL);
	if (!idata)
		return -ENOMEM;
	if (IS_ERR(card)) {
		dev_err(mmc_dev(g_mmc), "%s IS_ERR is NULL\n", __func__);
		err = PTR_ERR(card);
		goto cmd_err;
	}
	for (i = 0; i < num_of_cmds; i++) {
		idata[i] = tran_mmc_ioctl_copy_from_user(&cmds[i]);
		if (IS_ERR(idata[i])) {
			dev_err(mmc_dev(g_mmc), "%s idata[%d] is NULL\n", __func__, i);
			err = PTR_ERR(idata[i]);
			num_of_cmds = i;
			goto cmd_err;
		}
	}
	md = dev_get_drvdata(&card->dev);
	if(IS_ERR(md)) {
		dev_err(mmc_dev(g_mmc), "%s md is NULL\n", __func__);
		err = PTR_ERR(md);
		goto cmd_err;
	}
	/*
	 * Dispatch the ioctl()s into the block request queue.
	 */
	mq = &md->queue;
	req = blk_mq_alloc_request(mq->queue,
		idata[0]->ic.write_flag ? REQ_OP_DRV_OUT : REQ_OP_DRV_IN, 0);
	if (IS_ERR(req)) {
		dev_err(mmc_dev(g_mmc), "%s req is NULL\n", __func__);
		err = PTR_ERR(req);
		goto cmd_err;
	}
	req_to_mmc_queue_req(req)->drv_op = MMC_DRV_OP_IOCTL;
	req_to_mmc_queue_req(req)->drv_op_result = -EIO;
	req_to_mmc_queue_req(req)->drv_op_data = idata;
	req_to_mmc_queue_req(req)->ioc_count = num_of_cmds;
	blk_execute_rq(req, false);
	ioc_err = req_to_mmc_queue_req(req)->drv_op_result;
			/* copy to user if data and response */
	for (i = 0; i < num_of_cmds && !err; i++)
		err = tran_mmc_ioctl_copy_to_user(&cmds[i], idata[i]);
	blk_mq_free_request(req);
cmd_err:
	for (i = 0; i < num_of_cmds; i++) {
		kfree(idata[i]->buf);
		kfree(idata[i]);
	}
	kfree(idata);
	return ioc_err ? ioc_err : err;
}
static struct mmc_ffu_ioc_data *mmc_ffu_ioctl_copy_from_user(
	struct mmc_ioc_cmd __user *user)
{
	struct mmc_ffu_ioc_data *idata;
	int err;
	idata = kmalloc(sizeof(*idata), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}
	if (copy_from_user(&idata->ic, user, sizeof(idata->ic))) {
		err = -EFAULT;
		goto idata_err;
	}
	idata->buf_bytes = (u64)idata->ic.blksz * idata->ic.blocks;
	if (!idata->buf_bytes) {
		err = -EINVAL;
		goto idata_err;
	}
	idata->buf = memdup_user((void __user *)(unsigned long)
				 idata->ic.data_ptr, idata->buf_bytes);
	if (IS_ERR(idata->buf)) {
		err = PTR_ERR(idata->buf);
		goto idata_err;
	}
	return idata;
idata_err:
	kfree(idata);
out:
	return ERR_PTR(err);
}
static int
mmc_send_cxd_native(struct mmc_host *host, u32 arg, u32 *cxd, int opcode)
{
	int err;
	struct mmc_command cmd = {};
	cmd.opcode = opcode;
	cmd.arg = arg;
	cmd.flags = MMC_RSP_R2 | MMC_CMD_AC;
	err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;
	memcpy(cxd, cmd.resp, sizeof(u32) * 4);
	return 0;
}
int mmc_send_cid(struct mmc_host *host, u32 *cid)
{
	return mmc_send_cxd_native(host, 0, cid, MMC_ALL_SEND_CID);
}
static void mmc_ffu_hw_reset(struct mmc_card *card)
{
	u8 raw_prv, prv;
	u32 cid[4];
	int ret;
	ret = mmc_send_cid(g_mmc, cid);
	if (ret) {
		dev_err(mmc_dev(g_mmc), "%s:(FFU) get cid info fail!\n", mmc_hostname(card->host));
		return;
	}
	raw_prv = (card->raw_cid[2] >> 16) & 0xff;
	prv = (cid[2] >> 16) & 0xff;
	dev_err(mmc_dev(g_mmc), "%s:(FFU) raw prv %x, prv %x\n", mmc_hostname(card->host),
		raw_prv, prv);
	if (unlikely((card->csd.mmca_vsn >= 2) && (raw_prv != prv))) {
		dev_err(mmc_dev(g_mmc), "%s:(FFU) raw prv %x, prv %x\n", mmc_hostname(card->host),
			raw_prv, prv);
		card->raw_cid[2] &= 0xff00ffff;
		card->raw_cid[2] |= prv << 16;
		card->cid.prv = prv;
	}
	dev_err(mmc_dev(g_mmc), "%s:(FFU) hw reset\n", mmc_hostname(card->host));
	mmc_hw_reset(card);
	return;
}
static int mmc_ffu_cache_off(struct mmc_card *card)
{
	int err = 0;
	if (card->ext_csd.cache_size > 0 && card->ext_csd.cache_ctrl) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_CACHE_CTRL, 0, 0);
		if (err)
			dev_err(mmc_dev(g_mmc),
			"%s: cache off error %d\n", mmc_hostname(card->host), err);
		else
			card->ext_csd.cache_ctrl = 0;
	}
	return err;
}
static int mmc_ffu_wait_busy(struct mmc_card *card,
	unsigned int timeout_ms)
{
	struct mmc_command cmd = {0};
	int err;
	unsigned long timeout;
	unsigned int udelay = 32, udelay_max = 32768;
	bool expired = false;
	bool busy = false;
	memset(&cmd, 0, sizeof(struct mmc_command));
	cmd.opcode = MMC_SEND_STATUS;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;
	timeout = jiffies + msecs_to_jiffies(timeout_ms) + 1;
	do {
		expired = time_after(jiffies, timeout);
		err = mmc_wait_for_cmd(card->host, &cmd, 0);
		if (err)
			return err;
		/* Timeout if the device still remains busy. */
		busy = !(cmd.resp[0] & R1_READY_FOR_DATA) ||
				(R1_CURRENT_STATE(cmd.resp[0]) == R1_STATE_PRG);
		if (expired && busy) {
			dev_err(mmc_dev(g_mmc), "%s: Card stuck being busy! %s\n",
				mmc_hostname(card->host), __func__);
			return -ETIMEDOUT;
		}
		/* Throttle the polling rate to avoid hogging the CPU. */
		if (busy) {
			usleep_range(udelay, udelay * 2);
			if (udelay < udelay_max)
				udelay *= 2;
		}
	} while (busy);
	return 0;
}
//STD: xiaoxiang.xiong add for avoid abnormal req pointer 20240120 start
#if IS_ENABLED(CONFIG_EMMC_TRANSSION_MMC_MQ_DEFAULT)
#define EMMC_IOC_CMD (1 << 20)
#endif
//STD: xiaoxiang.xiong add for avoid abnormal req pointer 20240120 end
static int mmc_ffu_download(struct mmc_card *card,
	struct mmc_ffu_ioc_data *idata,
	u8 *ext_csd)
{
	const char *name = mmc_hostname(card->host);
	struct mmc_command cmd = {}, stop = {};
	struct mmc_data data = {};
	struct mmc_request mrq = {};
	struct scatterlist sg;
	int err, err2;
	cmd.arg = ext_csd[EXT_CSD_FFU_ARG] | ext_csd[EXT_CSD_FFU_ARG + 1] << 8
				| ext_csd[EXT_CSD_FFU_ARG + 2] << 16
				| ext_csd[EXT_CSD_FFU_ARG + 3] << 24;
	cmd.flags = MMC_CMD_ADTC | MMC_RSP_R1;
	if (idata->buf_bytes) {
		data.sg = &sg;
		data.sg_len = 1;
		data.blksz = idata->ic.blksz;
		data.blocks = idata->ic.blocks;
		data.flags = MMC_DATA_WRITE;
		sg_init_one(data.sg, idata->buf, idata->buf_bytes);
		mmc_set_data_timeout(&data, card);
		if (idata->ic.data_timeout_ns)
			data.timeout_ns = idata->ic.data_timeout_ns;
		if ((cmd.flags & MMC_RSP_R1B) == MMC_RSP_R1B)
			data.timeout_ns = idata->ic.cmd_timeout_ms * 1000000;
		mrq.data = &data;
	}
	mrq.cmd = &cmd;
	mrq.cmd->opcode = MMC_WRITE_MULTIPLE_BLOCK;
	mrq.stop = &stop;
	mrq.stop->opcode = MMC_STOP_TRANSMISSION;
	mrq.stop->arg = 0;
	mrq.stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
	/* set MODE_CONFIG[30] to "FFU Mode" */
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_MODE_CONFIG,
			MMC_FFU_MODE_SET, card->ext_csd.generic_cmd6_time);
	if (err) {
		dev_err(mmc_dev(g_mmc),
			"%s:(FFU) switch to FFU mode failed, err%d\n", name, err);
		goto exit_err;
	}
	dev_err(mmc_dev(g_mmc), "eMMC cache originally %s -> %s\n",
		((card->ext_csd.cache_ctrl) ? "on" : "off"),
		((card->ext_csd.cache_ctrl) ? "turn off" : "keep"));
	if (card->ext_csd.cache_ctrl) {
		(void)mmc_flush_cache(card->host);
		(void)mmc_ffu_cache_off(card);
	}
//STD: xiaoxiang.xiong add for avoid abnormal req pointer 20240120 start
#if IS_ENABLED(CONFIG_EMMC_TRANSSION_MMC_MQ_DEFAULT)
	mrq.cmd->flags |=EMMC_IOC_CMD;
#endif
//STD: xiaoxiang.xiong add for avoid abnormal req pointer 20240120 end
	mmc_wait_for_req(card->host, &mrq);
	err = mmc_ffu_wait_busy(card, 20 * 1000);
	if (err)
		err = -EBUSY;
	else if (cmd.error)
		err = cmd.error;
	else if (mrq.data->error)
		err = mrq.data->error;
	else if (stop.error)
		err = stop.error;
	else if (mrq.data->bytes_xfered != mrq.data->blocks * mrq.data->blksz)
		err = -EIO;
	else
		err = 0;
	if (err) {
		err2 = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_MODE_CONFIG,
				MMC_FFU_MODE_NORMAL, card->ext_csd.generic_cmd6_time);
		if (err2) {
			dev_err(mmc_dev(g_mmc),
				"%s:(FFU) switch to normal mode fail, err%d\n", name, err2);
			mmc_ffu_hw_reset(card);
		}
	}
exit_err:
	return err;
}
static int mmc_ffu_install(struct mmc_card *card, u8 *ext_csd)
{
	const char *name = mmc_hostname(card->host);
	u8 *ext_csd_new = NULL;
	u32 timeout;
	u8 set = 1;
	u8 retry = 10;
	u32 fw_sectors;
	int err, err2;
	if (ext_csd[EXT_CSD_FFU_FEATURES] & MMC_FFU_FEATURES) {
		fw_sectors = ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG]
						| ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 1] << 8
						| ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 2] << 16
						| ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 3] << 24;
		if (!fw_sectors) {
			dev_err(mmc_dev(g_mmc),
				"%s:(FFU) firmware were not download into the device", name);
			err = -EIO;
			goto restore_normal;
		}
		timeout = ext_csd[EXT_CSD_OPERATION_CODE_TIMEOUT];
		if (timeout == 0 || timeout > 0x17) {
			timeout = 0x17;
			dev_err(mmc_dev(g_mmc),
				"%s:(FFU) use default OPERATION_CODES_TIMEOUT value", name);
		}
		timeout = (100 * (1 << timeout)) / 1000 + 1;
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_MODE_OPERATION_CODES, MMC_FFU_INSTALL_SET, timeout);
		if (err) {
			dev_err(mmc_dev(g_mmc),
				"%s:(FFU) install failed", name);
			goto restore_normal;
		}
	} else {
		if ((card->cid.manfid == CID_MANFID_HYNIX) && (card->cid.prv == 0x03))
			set = 0;
		dev_err(mmc_dev(g_mmc),
			"%s:(FFU) Device not support MODE_OPERATION_CODES\n", name);
		err = mmc_switch(card, set, EXT_CSD_MODE_CONFIG,
				MMC_FFU_MODE_NORMAL, card->ext_csd.generic_cmd6_time);
		if (err) {
			dev_err(mmc_dev(g_mmc),
				"%s:(FFU) error %d:exit FFU mode\n", name, err);
			goto restore_normal;
		}
		mmc_ffu_hw_reset(card);
	}
	while (retry--) {
		err = mmc_get_ext_csd(card, &ext_csd_new);
		if (!err)
			break;
		dev_err(mmc_dev(g_mmc),
			"%s:(FFU) read ext_csd_new failed %d times", name, retry);
	}
	if (err) {
		dev_err(mmc_dev(g_mmc),
			"%s:(FFU) read ext_csd_new failed 10 times, reboot!!!", name);
		goto reboot_sys;
	}
	if (!ext_csd_new[EXT_CSD_FFU_STATUS]) {
		dev_err(mmc_dev(g_mmc), "%s:(FFU) install success!!", name);
		if (memcmp(card->ext_csd.fwrev,
					&ext_csd_new[EXT_CSD_FIRMWARE_VERSION], MMC_FIRMWARE_LEN)) {
			memcpy(card->ext_csd.fwrev, &ext_csd_new[EXT_CSD_FIRMWARE_VERSION],
				   MMC_FIRMWARE_LEN);
			dev_err(mmc_dev(g_mmc), "%s:(FFU) update ext_csd fwrev", name);
		}
		goto exit;
	} else {
		dev_err(mmc_dev(g_mmc), "%s:(FFU) install failed!!,%x", name,
			ext_csd_new[EXT_CSD_FFU_STATUS]);
		err = -EIO;
	}
	goto exit;
reboot_sys:
	emergency_restart();
	goto exit;
restore_normal:
	err2 = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_MODE_CONFIG,
			MMC_FFU_MODE_NORMAL, card->ext_csd.generic_cmd6_time);
	if (err2) {
		dev_err(mmc_dev(g_mmc), "%s:(FFU) switch to normal mode failed, err%d\n", name, err2);
		mmc_ffu_hw_reset(card);
	}
exit:
	kfree(ext_csd_new);
	return err;
}
static int tran_mmc_ffu_ioctl_cmd(struct mmc_host *host,
	struct mmc_ioc_cmd __user *ic_ptr)
{
	const char *name = mmc_hostname(host);
	struct mmc_ffu_ioc_data *idata;
	struct mmc_card *card = host->card;
	struct mmc_blk_data *md;
	struct mmc_queue *mq;
	struct sdio_func sf;
	int err = 0, ioc_err = 0;
	u8 *ext_csd = NULL;
	bool cmdq_dis = false;
	int busy = 1000;
	if (!card || mmc_card_removed(card))
		return -ENODEV;
	if ((card->ext_csd.rev < 7) || !card->ext_csd.ffu_capable) {
		dev_err(mmc_dev(g_mmc), "%s:(FFU) ffu operation not permitted, ver%d\n", name,
			card->ext_csd.rev);
		return -EPERM;
	}
	idata = mmc_ffu_ioctl_copy_from_user(ic_ptr);
	if (IS_ERR(idata) || idata->ic.opcode != MMC_FFU_DOWNLOAD_OP) {
		dev_err(mmc_dev(g_mmc), "%s:(FFU) copy from user failed\n", name);
		err = (int)PTR_ERR(idata);
		return err;
	}
	md = dev_get_drvdata(&card->dev);
	mq = &md->queue;
	err = -EBUSY;
	while (busy--) {
		spin_lock_irq(&mq->lock);
		if (mq->recovery_needed || mq->busy) {
			spin_unlock_irq(&mq->lock);
			usleep_range(3000, 5000);
			continue;
		}
		mq->busy = true;
		spin_unlock_irq(&mq->lock);
		err = 0;
		break;
	}
	if (err) {
		dev_err(mmc_dev(g_mmc), "%s:(FFU) mq busy\n", name);
		return err;
	}
	if (host->cqe_enabled || host->hsq_enabled) {
		err = host->cqe_ops->cqe_wait_for_idle(host);
		if (err) {
			dev_err(mmc_dev(g_mmc), "%s:(FFU) wait for idle fail\n", name);
			return err;
		}
	}
	sf.card = card;
	sdio_claim_host(&sf);
	if (card->ext_csd.cmdq_en) {
		err = mmc_cmdq_disable(card);
		if (err) {
			dev_err(mmc_dev(g_mmc), "%s:(FFU) cmdq disable fail,err=%d\n", name, err);
			goto cmd_done;
		}
		cmdq_dis = true;
	}
	err = mmc_get_ext_csd(card, &ext_csd);
	if (err) {
		dev_err(mmc_dev(g_mmc), "%s:(FFU) read ext_csd failed,err=%d\n", name, err);
		goto cmd_done;
	}
	err = mmc_ffu_download(card, idata, ext_csd);
	if (err) {
		dev_err(mmc_dev(g_mmc), "%s:(FFU) download FW failed, err%d\n", name, err);
		goto cmd_done;
	}
	err = mmc_ffu_install(card, ext_csd);
	if (err)
		dev_err(mmc_dev(g_mmc), "%s:(FFU) install FW failed, err%d\n", name, err);
cmd_done:
	if (cmdq_dis) {
		err = mmc_cmdq_enable(card);
		if (err) {
			dev_err(mmc_dev(g_mmc), "%s:(FFU) cmdq enable fail,err=%d\n", name, err);
			mmc_ffu_hw_reset(card);
		}
	}
	spin_lock_irq(&mq->lock);
	mq->busy = false;
	spin_unlock_irq(&mq->lock);
	sdio_release_host(&sf);
	if (!IS_ERR(idata)) {
		kfree(idata->buf);
		kfree(idata);
	}
	kfree(ext_csd);
	dev_err(mmc_dev(g_mmc), "emmc ffu success,reboot!\n");
	emergency_restart();
	return ioc_err ? ioc_err : err;
}
static int tran_mmc_ioctl_check(void)
{
	pr_err("[tran_rhi]:current->comm = %s\n",current->comm);
	if (((!(strstr(current->comm, "storage_feature") ||
		strstr(current->comm, "flashtest") ||
		strstr(current->comm, "memtester"))) &&
		(!capable(CAP_SYS_RAWIO))))
	return -EPERM;
	return 0;
}
static long tran_mmc_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int ret;
	switch (cmd) {
	case MMC_IOC_CMD:
		ret = tran_mmc_ioctl_check();
		if(ret)
			return ret;
		ret = tran_mmc_ioctl_cmd(g_mmc, (struct mmc_ioc_cmd __user *)arg);
		return ret;
	case MMC_IOC_FFU_CMD:
		ret = tran_mmc_ffu_ioctl_cmd(g_mmc, (struct mmc_ioc_cmd __user *)arg);
		return ret;
	case MMC_IOC_MULTI_CMD:
		ret = tran_mmc_ioctl_check();
		if(ret)
			return ret;
		ret = tran_mmc_ioctl_multi_cmd(g_mmc, (struct mmc_ioc_multi_cmd __user *)arg);
		return ret;
	default:
		return -EINVAL;
	}
}
#ifdef CONFIG_COMPAT
static long tran_mmc_compat_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	return tran_mmc_ioctl(NULL, cmd, (unsigned long) compat_ptr(arg));
}
#endif
static int tran_mmc_ioctl_open(struct inode *inode, struct file *file)
{
	return single_open(file, NULL, NULL);
}
static const struct proc_ops tran_proc_mmc_ioctl_fops=
{
	.proc_open = tran_mmc_ioctl_open,
	.proc_ioctl = tran_mmc_ioctl,
#ifdef CONFIG_COMPAT
	.proc_compat_ioctl = tran_mmc_compat_ioctl,
#endif
};
void tran_mmc_ioctl_init_handler(struct msdc_host *host)
{
	struct mmc_host *mmc= mmc_from_priv(host);
	struct mmc_card *card = mmc->card;
	if(!card) {
		dev_info(host->dev, "mmc_card is NULL\n");
		return;
	}
	g_mmc = mmc;
	proc_create("mmc", S_IALLUGO, NULL, &tran_proc_mmc_ioctl_fops);
	return;
}
static void tran_mmc_ioctl_init_work(struct work_struct *work)
{
	struct msdc_host *host = container_of(work, struct msdc_host,
						tran_mmc_ioctl_init_worker.work);
	tran_mmc_ioctl_init_handler(host);
	return;
}
void tran_mmc_ioctl_init(struct msdc_host *host)
{
	if(strncmp(mmc_hostname(mmc_from_priv(host)), "mmc0", strlen("mmc0")) == 0) {
		dev_err(host->dev, "tran_mmc_ioctl_init start");
		INIT_DELAYED_WORK(&host->tran_mmc_ioctl_init_worker, tran_mmc_ioctl_init_work);
		schedule_delayed_work(&host->tran_mmc_ioctl_init_worker, msecs_to_jiffies(3000));
	}
}
void tran_mmc_ioctl_exit(struct msdc_host *host)
{
	cancel_delayed_work_sync(&host->tran_mmc_ioctl_init_worker);
	remove_proc_entry("mmc", NULL);
}

