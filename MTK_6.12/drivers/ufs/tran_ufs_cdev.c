// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <ufs/ufshcd.h>
#include <ufs/unipro.h>
#include "../core/ufshcd-priv.h"
#include "tran_ufs_cdev.h"
#include "ufs-mediatek.h"
static void tran_ufs_cdev_class_rls(struct device *dev) {}
static struct tran_ufs_cdev_data ufs_cdev;
static struct class ufs_cdev_class = {
	.name = "ufs_cdev",
	.dev_release = tran_ufs_cdev_class_rls,
};

//STD: xiaoxiang.xiong add atomic storage 3.0 20241030 start
#if IS_ENABLED(CONFIG_TRANSSION_BLK)
extern int tran_scsi_execute_cmd(struct scsi_device *sdev, const unsigned char *cmd,
		     blk_opf_t opf, void *buffer, unsigned int bufflen,
		     int timeout, int ml_retries,
		     const struct scsi_exec_args *args);
extern struct request *tran_scsi_alloc_request(struct request_queue *q, blk_opf_t opf,
				   blk_mq_req_flags_t flags);
extern blk_status_t (*__tran_blk_execute_rq_hook)(struct request *rq,
						  bool at_head);
extern void (*__tran_blk_mq_free_request_hook)(struct request *req);
#endif
//STD: xiaoxiang.xiong add atomic storage 3.0 20241030 end

static int tran_ufs_cdev_open(struct inode *inode, struct file *filp)
{
	filp->private_data = ufs_cdev.hba;
	return nonseekable_open(inode, filp);
}
static int tran_ufs_cdev_release(struct inode *inode, struct file *filp)
{
	return 0;
}
static int tran_ufs_ffu_send_cmd(struct scsi_device *dev, struct ufs_ioctl_ffu_data *idata)
{
	struct ufs_hba *hba;
	unsigned char cmd[10];
	struct scsi_sense_hdr sshdr;
	unsigned long flags;
	int ret;
	int size_to_write, written;
	struct scsi_exec_args exec_args = {
		.sense = NULL,
		.sshdr = &sshdr,
		.req_flags = BLK_MQ_REQ_PM,
		.resid = NULL,
		};
	if (dev)
		hba = shost_priv(dev->host);
	else
		return -ENODEV;
	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(dev);
	if (!ret && !scsi_device_online(dev)) {
		ret = -ENODEV;
		scsi_device_put(dev);
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (ret)
		return ret;
	for (written = 0; written < idata->buf_byte;
		 written += size_to_write) {
		if ((idata->buf_byte - written) > MAX_WRITE_BUFFER_SIZE)
			size_to_write = MAX_WRITE_BUFFER_SIZE;
		else
			size_to_write = (idata->buf_byte - written);
		/*
		 * If scsi commands fail, the scsi mid-layer schedules scsi
		 * error-handling, which would wait for host to be resumed.
		 * Since we know we are functional while we are here, skip
		 * host resume in error handling context.
		 */
		hba->host->eh_noresume = 1;
		cmd[0] = WRITE_BUFFER;					 /* Opcode */
		/* 0xE: Download firmware */
		cmd[1] = 0xE;
		cmd[2] = 0; 							 /* Buffer ID = 0 */
		/* Buffer Offset[23:16] = 0 */
		cmd[3] = (unsigned char)((written >> 16) & 0xff);
		/* Buffer Offset[15:08] = 0 */
		cmd[4] = (unsigned char)((written >> 8) & 0xff);
		/* Buffer Offset[07:00] = 0 */
		cmd[5] = (unsigned char)(written & 0xff);
		cmd[6] = (size_to_write >> 16) & 0xff;	 /* Length[23:16] */
		cmd[7] = (size_to_write >> 8) & 0xff;	 /* Length[15:08] */
		cmd[8] = (size_to_write) & 0xff;		 /* Length[07:00] */
		cmd[9] = 0x0;							 /* Control = 0 */
		/*
		 * Current function would be generally called from the power
		 * management callbacks hence set the RQF_PM flag so that it
		 * doesn't resume the already suspended children.
		 */
//STD: xiaoxiang.xiong add atomic storage 3.0 20241030 start
#if IS_ENABLED(CONFIG_TRANSSION_BLK)
		ret = tran_scsi_execute_cmd(dev, cmd, REQ_OP_DRV_OUT,
							idata->buf_ptr + written,
							size_to_write,
							msecs_to_jiffies(1000), 0, &exec_args);
#else
		ret = scsi_execute_cmd(dev, cmd, REQ_OP_DRV_OUT,
							idata->buf_ptr + written,
							size_to_write,
							msecs_to_jiffies(1000), 0, &exec_args);
#endif
//STD: xiaoxiang.xiong add atomic storage 3.0 20241030 end
		if (ret) {
			sdev_printk(KERN_ERR, dev,
			"WRITE BUFFER failed for firmware upgrade sense key:0x%x;asc:0x%x;ascq:0x%x",
			(int)exec_args.sshdr->sense_key, (int)exec_args.sshdr->asc,
			(int)exec_args.sshdr->ascq);
		}
	}
	scsi_device_put(dev);
	hba->host->eh_noresume = 0;
	return ret;
}
//STD: add scsi dev resum by fenglin.yang 20250930 start
void tran_ufs_device_resume(struct ufs_hba *hba)
{
	struct scsi_device *scsi_d;
	int i;
	struct scsi_device *sdev_rpmb = scsi_device_lookup(hba->host, 0, 0,
		((UFS_UPIU_RPMB_WLUN & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE));

	for (i = 0; i < UFS_UPIU_MAX_GENERAL_LUN; i++) {
		scsi_d = scsi_device_lookup(hba->host, 0, 0, i);
		if (scsi_d)
			scsi_device_resume(scsi_d);
	}
	scsi_d = scsi_device_lookup(hba->host, 0, 0,
		((UFS_UPIU_BOOT_WLUN & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE));
	if (scsi_d)
		scsi_device_resume(scsi_d);
	if (hba->ufs_device_wlun)
		scsi_device_resume(hba->ufs_device_wlun);
	if (sdev_rpmb)
		scsi_device_resume(sdev_rpmb);
}
//STD: add scsi dev resum by fenglin.yang 20250930 end
void tran_ufs_device_quiesce(struct ufs_hba *hba)
{
	struct scsi_device *scsi_d;
	int i;
	struct scsi_device *sdev_rpmb = scsi_device_lookup(hba->host, 0, 0,
		 ((UFS_UPIU_RPMB_WLUN & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE));
	/*
	 * Wait all cmds done & block user issue cmds to
	 * general LUs, wlun device, wlun rpmb and wlun boot.
	 * To avoid new cmds coming after device has been
	 * stopped by SSU cmd in ufshcd_suspend().
	 */
	for (i = 0; i < UFS_UPIU_MAX_GENERAL_LUN; i++) {
		scsi_d = scsi_device_lookup(hba->host, 0, 0, i);
		if (scsi_d)
			scsi_device_quiesce(scsi_d);
	}
	scsi_d = scsi_device_lookup(hba->host, 0, 0,
		((UFS_UPIU_BOOT_WLUN & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE));
	if (scsi_d)
		scsi_device_quiesce(scsi_d);
	if (hba->ufs_device_wlun)
		scsi_device_quiesce(hba->ufs_device_wlun);
	if (sdev_rpmb)
		scsi_device_quiesce(sdev_rpmb);
}
static int tran_ufs_ioctl_ffu(struct scsi_device *dev, void __user *buf_user)
{
	struct ufs_hba *hba = shost_priv(dev->host);
	struct ufs_ioctl_ffu_data *idata = NULL;
	struct ufs_ioctl_ffu_data *idata_user = NULL;
	int err = 0;
	u32 attr = 0;
	idata = kzalloc(sizeof(struct ufs_ioctl_ffu_data), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}
	idata_user = kzalloc(sizeof(struct ufs_ioctl_ffu_data), GFP_KERNEL);
	if (!idata_user) {
		kfree(idata);
		err = -ENOMEM;
		goto out;
	}
	/* extract struct idata from user buffer */
	err = copy_from_user(idata_user, buf_user,
		sizeof(struct ufs_ioctl_ffu_data));
	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}
	memcpy(idata, idata_user, sizeof(struct ufs_ioctl_ffu_data));
	/* extract firmware from user buffer */
	if (idata->buf_byte > (u32)UFS_IOCTL_FFU_MAX_FW_SIZE_BYTES) {
		dev_err(hba->dev, "%s: idata->buf_byte:0x%x > max 0x%x bytes\n",
				__func__, idata->buf_byte,
				(u32)UFS_IOCTL_FFU_MAX_FW_SIZE_BYTES);
		err = -ENOMEM;
		goto out_release_mem;
	}
	idata->buf_ptr = vzalloc(idata->buf_byte);
	if (!idata->buf_ptr) {
		err = -ENOMEM;
		goto out_release_mem;
	}
	if (copy_from_user(idata->buf_ptr,
		(void __user *)idata_user->buf_ptr, idata->buf_byte)) {
		err = -EFAULT;
		goto out_release_idata_buf_ptr;
	}
	tran_ufs_device_quiesce(hba);
	/* do FFU */
	err = tran_ufs_ffu_send_cmd(dev, idata);
	//STD: add scsi dev resum by fenglin.yang 20250930 start
	tran_ufs_device_resume(hba);
	//STD: add scsi dev resum by fenglin.yang 20250930 end
	if (err) {
		dev_err(hba->dev, "%s: ffu failed, err %d\n", __func__, err);
		//ufs_mtk_device_resume(hba);
		goto out_release_idata_buf_ptr;
	} else {
		dev_info(hba->dev, "%s: ffu ok\n", __func__);
	}
	/*
	 * Check bDeviceFFUStatus attribute
	 *
	 * For reference only since UFS spec. said the status is valid after
	 * device power cycle.
	 */
	err = ufshcd_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
		QUERY_ATTR_IDN_FFU_STATUS, 0, 0, &attr);
	if (err) {
		dev_err(hba->dev, "%s: query bDeviceFFUStatus failed, err %d\n",
			__func__, err);
		goto out_release_idata_buf_ptr;
	}
	if (attr > UFS_FFU_STATUS_OK)
		dev_err(hba->dev, "%s: bDeviceFFUStatus shows fail %d (ref only)\n",
			__func__, attr);
out_release_idata_buf_ptr:
	vfree(idata->buf_ptr);
out_release_mem:
	kfree(idata);
	kfree(idata_user);
out:
	return err;
}
static int tran_ufs_ioctl_get_dev_info(struct scsi_device *dev, void __user *buf_user)
{
	struct ufs_hba *hba;
	struct ufs_ioctl_query_device_info *idata = NULL;
	int err = 0;
	if (!dev)
		return -ENODEV;
	hba = shost_priv(dev->host);
	if (!dev->rev) {
		dev_err(hba->dev, "%s: scsi_device or rev is NULL\n", __func__);
		err = -ENOENT;
		goto out;
	}
	idata = kzalloc(sizeof(struct ufs_ioctl_query_device_info), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}
	err = copy_from_user(idata, buf_user, sizeof(struct ufs_ioctl_query_device_info));
	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}
	memcpy(idata->vendor, dev->vendor, UFS_IOCTL_FFU_MAX_VENDOR_BYTES);
	memcpy(idata->model, dev->model, UFS_IOCTL_FFU_MAX_MODEL_BYTES);
	memcpy(idata->fw_rev, dev->rev, UFS_IOCTL_FFU_MAX_FW_VER_BYTES);
	idata->manid = hba->dev_info.wmanufacturerid;
	idata->max_hw_sectors_size = (dev->request_queue->limits.max_hw_sectors << 9);
	err = copy_to_user(buf_user, idata, sizeof(struct ufs_ioctl_query_device_info));
	if (err)
		dev_err(hba->dev, "%s: err %d copying to user.\n", __func__, err);
out_release_mem:
	kfree(idata);
out:
	return err;
}
//MD:add XLLSHLSSP-17 by xiaoxiang.xiong for ffu start
/**
 * sprd_ufs_ioctl_query - perform user read queries
 * @hba: per-adapter instance
 * @lun: used for lun specific queries
 * @buffer: user space buffer for reading and submitting query data and params
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_query_data.
 * It will read the opcode, idn and buf_length parameters, and, put the
 * response in the buffer field while updating the used size in buf_length.
 */
int tran_ufs_ioctl_query(struct ufs_hba *hba, void __user *buf_user)
{
	struct tran_ufs_ioctl_query_data *idata;
	void __user *user_buf_ptr;
	int err = 0;
	int length = 0;
	void *data_ptr;
	bool flag;
	u32 att = 0;
	u8 *desc = NULL;
	u8 read_desc, read_attr, write_attr, read_flag;
	idata = kzalloc(sizeof(struct tran_ufs_ioctl_query_data), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}
	/* extract params from user buffer */
	err = copy_from_user(idata, buf_user,
			sizeof(struct tran_ufs_ioctl_query_data));
	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}
	user_buf_ptr = idata->buf_ptr;
	/*
	 * save idata->idn to specific local variable to avoid
	 * confusion by comapring idata->idn with different enums below.
	 */
	switch (idata->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		read_desc = idata->idn;
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		read_attr = idata->idn;
		break;
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		write_attr = idata->idn;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		read_flag = idata->idn;
		break;
	default:
		goto out_einval;
	}
	/* verify legal parameters & send query */
	switch (idata->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		switch (read_desc) {
		case QUERY_DESC_IDN_DEVICE:
//BSP:add XLELHLES-7 by shentao.yang for ufs healthinfo get 20211214 start
		case QUERY_DESC_IDN_HEALTH:
//BSP:add XLELHLES-7 by shentao.yang for ufs healthinfo get 20211214 end
		case QUERY_DESC_IDN_STRING:
			break;
		default:
			goto out_einval;
		}
		length = min_t(int, QUERY_DESC_MAX_SIZE,
				idata->buf_byte);
		desc = kzalloc(length, GFP_KERNEL);
		if (!desc) {
			err = -ENOMEM;
			goto out_release_mem;
		}
		err = ufshcd_query_descriptor_retry(hba, idata->opcode,
				read_desc, idata->idx, 0, desc, &length);
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		switch (read_attr) {
		case QUERY_ATTR_IDN_BOOT_LUN_EN:
			break;
		case QUERY_ATTR_IDN_FFU_STATUS:
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_attr(hba, idata->opcode,
					read_attr, idata->idx, 0, &att);
		break;
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		switch (write_attr) {
		case QUERY_ATTR_IDN_BOOT_LUN_EN:
			break;
		default:
			goto out_einval;
		}
		length = min_t(int, sizeof(int), idata->buf_byte);
		if (copy_from_user(&att, (void __user *)(unsigned long)
					idata->buf_ptr, length)) {
			err = -EFAULT;
			goto out_release_mem;
		}
		err = ufshcd_query_attr(hba, idata->opcode,
					write_attr, idata->idx, 0, &att);
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		switch (read_flag) {
		case QUERY_FLAG_IDN_PERMANENTLY_DISABLE_FW_UPDATE:
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_flag(hba, idata->opcode,
					read_flag, 0, &flag);
		break;
	default:
		goto out_einval;
	}
	if (err) {
		dev_err(hba->dev, "%s: query for idn %d failed\n", __func__,
					idata->idn);
		goto out_release_mem;
	}
	/*
	 * copy response data
	 * As we might end up reading less data then what is specified in
	 * "ioct_data->buf_byte". So we are updating "ioct_data->
	 * buf_byte" to what exactly we have read.
	 */
	switch (idata->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		idata->buf_byte = min_t(int, idata->buf_byte, length);
		data_ptr = desc;
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		idata->buf_byte = sizeof(att);
		data_ptr = &att;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		idata->buf_byte = 1;
		data_ptr = &flag;
		break;
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		/* write attribute does not require coping response data */
		goto out_release_mem;
	default:
		goto out_einval;
	}
	/* copy to user */
	err = copy_to_user(buf_user, idata,
			sizeof(struct tran_ufs_ioctl_query_data));
	if (err)
		dev_err(hba->dev, "%s: failed copying back to user.\n",
			__func__);
	err = copy_to_user(user_buf_ptr, data_ptr, idata->buf_byte);
	if (err)
		dev_err(hba->dev, "%s: err %d copying back to user.\n",
				__func__, err);
	goto out_release_mem;
out_einval:
	dev_err(hba->dev,
		"%s: illegal ufs query ioctl data, opcode 0x%x, idn 0x%x\n",
		__func__, idata->opcode, (unsigned int)idata->idn);
	err = -EINVAL;
out_release_mem:
	kfree(idata);
	kfree(desc);
out:
	return err;
}
/**
 * sprd_ufs_ioctl_get_fw_ver - perform user request: query fw ver
 * @hba: per-adapter instance
 * @buffer: user space buffer for ffu ioctl data
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_ffu_data.
 * It will read the buffer information of new firmware.
 */
int tran_ufs_ioctl_get_fw_ver(struct scsi_device *dev, void __user *buf_user)
{
	struct ufs_hba *hba;
	struct ufs_ioctl_query_fw_ver_data *idata = NULL;
	int err;
	if (dev)
		hba = shost_priv(dev->host);
	else
		return -ENODEV;
	/* check scsi device instance */
	if (!dev->rev) {
		dev_err(hba->dev, "%s: scsi_device or rev is NULL\n", __func__);
		err = -ENOENT;
		goto out;
	}
	idata = kzalloc(sizeof(struct ufs_ioctl_query_fw_ver_data), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}
	/* extract params from user buffer */
	err = copy_from_user(idata, buf_user,
			sizeof(struct ufs_ioctl_query_fw_ver_data));
	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}
	idata->buf_byte = min_t(int, UFS_IOCTL_FFU_MAX_FW_VER_BYTES,
				idata->buf_byte);
	/*
	 * Copy firmware version string to user buffer
	 *
	 * We get firmware version from scsi_device->rev,
	 * which is ready in scsi_add_lun()
	 * during SCSI device probe process.
	 *
	 * If probe failed, rev will be NULL.
	 * We checked it in the beginning of this function.
	 */
	err = copy_to_user(idata->buf_ptr, dev->rev, idata->buf_byte);
	if (err) {
		dev_err(hba->dev, "%s: err %d copying back to user.\n",
				__func__, err);
		goto out_release_mem;
	}
out_release_mem:
	kfree(idata);
out:
	return 0;
}
//MD:add XLLSHLSSP-17 by xiaoxiang.xiong for ffu end
//STD: add proc tran_healthinfo by xujie.chu 20250603 start
#include <scsi/sg.h>
#include <linux/blkdev.h>
#define UFS_MAJOR 8  // /dev/block/sdc
#define UFS_MIJOR 32 // /dev/block/sdc
static int get_uhr_sg_io(struct scsi_device *sdev, void __user *buf_user)
{
	struct request_queue *q;
	struct sg_io_hdr *hdr;
	unsigned long start_time;
	ssize_t ret = 0;
	int writing = 0;
	int at_head = 0;
	struct request *rq;
	struct scsi_cmnd *scmd;
	struct bio *bio;
	q = sdev->request_queue;
	if(!q) {
		pr_err("tran_rhi:q NULL\n");
		return -EFAULT;
	}
	hdr = kzalloc(sizeof(struct sg_io_hdr), GFP_KERNEL);
	if(!hdr) {
		return -EFAULT;
	}
	if (copy_from_user(hdr, buf_user, sizeof(struct sg_io_hdr))){
		kfree(hdr);
		return -EFAULT;
	}
	if (hdr->dxfer_len)
		switch (hdr->dxfer_direction) {
		default:
			kfree(hdr);
			return -EINVAL;
		case SG_DXFER_TO_DEV:
			writing = 1;
			break;
		case SG_DXFER_TO_FROM_DEV:
		case SG_DXFER_FROM_DEV:
			break;
		}
	if (hdr->flags & SG_FLAG_Q_AT_HEAD)
		at_head = 1;
	ret = -ENOMEM;
//STD: xiaoxiang.xiong add atomic storage 3.0 20241030 start
#if IS_ENABLED(CONFIG_TRANSSION_BLK)
	rq = tran_scsi_alloc_request(q, writing ? REQ_OP_DRV_OUT : REQ_OP_DRV_IN, 0);
#else
	rq = scsi_alloc_request(q, writing ? REQ_OP_DRV_OUT : REQ_OP_DRV_IN, 0);
#endif
//STD: xiaoxiang.xiong add atomic storage 3.0 20241030 end
	if (IS_ERR(rq)) {
		kfree(hdr);
		return PTR_ERR(rq);
	}
	scmd = blk_mq_rq_to_pdu(rq);
	if (hdr->cmd_len > sizeof(scmd->cmnd)) {
		ret = -EINVAL;
		goto out_put_request;
	}
	//start ret = blk_fill_sghdr_rq(q, rq, hdr, mode);
	if (copy_from_user(scmd->cmnd, hdr->cmdp, hdr->cmd_len)) {
		pr_err("tran_rhi:copy_from_user hdr->cmdp fail\n");
		goto out_put_request;
	}
	/*
	 * fill in request structure
	 */
	scmd->cmd_len = hdr->cmd_len;
	rq->timeout = msecs_to_jiffies(hdr->timeout);
	if (!rq->timeout)
		rq->timeout = sdev->sg_timeout;
	if (!rq->timeout)
		rq->timeout = BLK_DEFAULT_SG_TIMEOUT;
	if (rq->timeout < BLK_MIN_SG_TIMEOUT)
		rq->timeout = BLK_MIN_SG_TIMEOUT;
	//end ret = blk_fill_sghdr_rq(q, rq, hdr, mode);
	ret = 0;
	ret = blk_rq_map_user_io(rq, NULL, hdr->dxferp, hdr->dxfer_len,
			GFP_KERNEL, hdr->iovec_count && hdr->dxfer_len,
			hdr->iovec_count, 0, rq_data_dir(rq));
	if (ret) {
		pr_err("tran_rhi:blk_rq_map_user fail\n");
		goto out_put_request;
	}
	bio = rq->bio;
	scmd->allowed = 0;
	start_time = jiffies;
	/* ignore return value. All information is passed back to caller
	 * (if he doesn't check that is his problem).
	 * N.B. a non-zero SCSI status is _not_ necessarily an error.
	 */
//STD: xiaoxiang.xiong add atomic storage 3.0 20241030 start
#if IS_ENABLED(CONFIG_TRANSSION_BLK)
	if(__tran_blk_execute_rq_hook)
		__tran_blk_execute_rq_hook(rq, at_head);
	else
		blk_execute_rq(rq, at_head);
#else
	blk_execute_rq(rq, at_head);
#endif
//STD: xiaoxiang.xiong add atomic storage 3.0 20241030 end
	hdr->duration = jiffies_to_msecs(jiffies - start_time);
	//ret = blk_complete_sghdr_rq(rq, hdr, bio);
	ret = blk_rq_unmap_user(bio);
	if (copy_to_user(buf_user, hdr, sizeof(struct sg_io_hdr))) {
		pr_err("tran_rhi:copy_to_user hdr fail\n");
	}
out_put_request:
//STD: xiaoxiang.xiong add atomic storage 3.0 20241030 start
#if IS_ENABLED(CONFIG_TRANSSION_BLK)
	if(__tran_blk_mq_free_request_hook)
		__tran_blk_mq_free_request_hook(rq);
	else
		blk_mq_free_request(rq);
#else
	blk_mq_free_request(rq);
#endif
//STD: xiaoxiang.xiong add atomic storage 3.0 20241030 end

	kfree(hdr);
	pr_err("tran_rhi:get_uhr_sg_io leave(ret=%zd)\n",ret);
	return ret;
}
//STD: add proc tran_healthinfo by xujie.chu 20250603 end
static long tran_ufs_cdevt_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct ufs_hba *hba = fp->private_data;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct scsi_device *sdev;

	if (!host)
		return false;
	sdev = scsi_device_lookup(hba->host, 0, 0, 0);
	if(!sdev)
		sdev = host->hba->ufs_device_wlun;
	switch (cmd) {
	case UFS_IOCTL_FFU:
		ufshcd_rpm_get_sync(hba);
		err = tran_ufs_ioctl_ffu(sdev, (void __user *)arg);
		ufshcd_rpm_put_sync(hba);
		break;
	case UFS_IOCTL_QUERY:
//STD add by yong.niu for fix ufs null pointer issue at 20251111 start
		ufshcd_rpm_get_sync(hba);
		err = tran_ufs_ioctl_query(hba, (void __user *)arg);
		ufshcd_rpm_put_sync(hba);
//STD add by yong.niu for fix ufs null pointer issue at 20251111 end
		break;
	case UFS_IOCTL_GET_FW_VER:
		err = tran_ufs_ioctl_get_fw_ver(sdev, (void __user *)arg);
		break;
//STD: add proc tran_healthinfo by xujie.chu 20250603 start
	case UFS_IOCTL_SG_IO:
		err = get_uhr_sg_io(sdev, (void __user *)arg);
		break;
//STD: add proc tran_healthinfo by xujie.chu 20250603 end
	case UFS_IOCTL_GET_DEVICE_INFO:
		err = tran_ufs_ioctl_get_dev_info(sdev, (void __user *)arg);
		break;
	default:
		err = -ENOIOCTLCMD;
        	break;
	}
	return err;
}
static const struct file_operations tran_ufs_cdev_fops = {
	.release = tran_ufs_cdev_release,
	.open = tran_ufs_cdev_open,
	.owner = THIS_MODULE,
	.llseek = noop_llseek,
	.unlocked_ioctl = tran_ufs_cdevt_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};
/*
 * cdevfs interfaces:
 * /dev/ufs_cdev [rw] [userspace]
 */
int tran_ufs_cdev_init(struct ufs_hba *hba)
{
	int ret;
	ufs_cdev.hba = hba;
	ret = alloc_chrdev_region(&ufs_cdev.devt, 0, UFS_MAX_DEVICES, "ufs_cdev");
	if (ret < 0) {
		dev_err(hba->dev, "unable to allocate char dev region.\n");
		return ret;
	}
	ufs_cdev.chrdev.owner = THIS_MODULE;
	cdev_init(&ufs_cdev.chrdev, &tran_ufs_cdev_fops);
	ret = cdev_add(&ufs_cdev.chrdev, ufs_cdev.devt, 1);
	if (ret) {
		dev_err(hba->dev, "ufs could not add character device.\n");
		goto unregister_chrdev;
	}
	ret = class_register(&ufs_cdev_class);
	if (ret) {
		dev_err(hba->dev, "failed to register ufs_cdev_class!\n");
		goto del_cdev;
	}
	ufs_cdev.ufs_dev = device_create(&ufs_cdev_class, NULL, MKDEV(MAJOR(ufs_cdev.devt), 0),
					 NULL, "ufs_cdev");
	if (ret)
		goto class_destroy;
	dev_err(hba->dev, "tran ufs cdev create chardev(%d:1)\n", MAJOR(ufs_cdev.devt));
	return 0;
class_destroy:
	class_unregister(&ufs_cdev_class);
del_cdev:
	cdev_del(&ufs_cdev.chrdev);
unregister_chrdev:
	unregister_chrdev_region(ufs_cdev.devt, UFS_MAX_DEVICES);
	return ret;
}
EXPORT_SYMBOL(tran_ufs_cdev_init);

void tran_ufs_cdev_exit(void)
{
	class_unregister(&ufs_cdev_class);
	cdev_del(&ufs_cdev.chrdev);
	unregister_chrdev_region(ufs_cdev.devt, UFS_MAX_DEVICES);
}
EXPORT_SYMBOL(tran_ufs_cdev_exit);
