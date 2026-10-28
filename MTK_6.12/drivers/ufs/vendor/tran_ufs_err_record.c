#include "tran_ufs_err_record.h"
#include <linux/namei.h>
#include <linux/fs.h>
#include <scsi/scsi_cmnd.h>
#include "tnek.h"
#include "tran_ufs_err_record_cloudctl.h"


const static int64_t FLAG_TNE_USER_LOG_MAIN_SYS = 1 << 1; //mobile log:android and system log
const static int64_t FLAG_TNE_USER_LOG_EVENT = 1 << 2;
const static int64_t FLAG_TNE_USER_LOG_RADIO = 1 << 3;
const static int64_t FLAG_TNE_USER_LOG_CRASH = 1 << 4;
const static int64_t FLAG_TNE_USER_LOG_KERNEL = 1 << 5;
const static int64_t FLAG_TNE_USER_ANR_TRACE = 1 << 6; //ams dumptrace ,begin with anr_
const static int64_t FLAG_TNE_USER_BINDER = 1 << 7;
const static int64_t FLAG_TNE_USER_MAPS = 1 << 8;
const static int64_t FLAG_TNE_USER_SIGNAL_TRACE = 1 << 9; //signal catcher trace, signal 3 begin with trace_
const static int64_t FLAG_TNE_USER_NATIVE_TRACE = 1<< 10; //debuggerd for native trace
const static int64_t FLAG_TNE_USER_LAST_KMSG = 1<< 11; //
const static int64_t FLAG_TNE_USER_PS = 1<<12;
const static int64_t FLAG_TNE_USER_PROC_MEMINFO = 1<<13;
const static int64_t FLAG_TNE_USER_ZONE_INFO = 1<<14;
const static int64_t FLAG_TNE_USER_BLOCKIO = 1<<15;
const static int64_t FLAG_TNE_USER_MMSTAT = 1<<16;
const static int64_t FLAG_TNE_USER_RAM_CONSOLE = 1<<17; //contain Internal error: Oops or Kernel panic
const static int64_t FLAG_TNE_USER_PROPERTY = 1<<18;
const static int64_t FLAG_TNE_USER_DF = 1<<19;
const static int64_t FLAG_TNE_USER_MOUNT = 1<<20;
const static int64_t FLAG_TNE_MINIDUMP = 1<<21;
const static int64_t FLAG_TNE_USER_WINDOW = 1<<22;

#define USER_TNE_FLAG	FLAG_TNE_USER_LOG_KERNEL | FLAG_TNE_USER_LOG_MAIN_SYS | FLAG_TNE_USER_PROPERTY

struct proc_dir_entry *tran_ufs_err_record_entry;

#define TRAN_FLASH_ERR_RECORD_DIR	"tran_flash_error_record"
#define TRAN_UFS_ERR_REOCRD_SCHEDULE_TIME	1000
#define TRAN_UER_MAX_DESC_LEN	1000
static struct ufs_hba *g_hba;
#ifdef CONFIG_TRAN_UFS_EXCEPTION_UPLOAD
static struct work_struct tran_uer_work;
static struct timer_list timer;
#endif


void tran_record_uic_err(struct ufs_hba *hba, enum ufs_event_type evt, u32 
data);
void ufshcd_count_data(struct scsi_cmnd *cmd, struct ufs_hba *hba)
{
	unsigned char opcode;
	unsigned int bytes;
	unsigned long long total_write = 0;
	unsigned long long total_read = 0;
	struct ufs_mtk_host *host = NULL;

	if (cmd == NULL || hba == NULL) {
		printk("ufshcd_count_data cmd or hba is null!\n");
		return;
	}

	host = ufshcd_get_variant(hba);
	bytes = cmd->sdb.length;
	opcode = cmd->cmnd[0];
	if (opcode == WRITE_6 || opcode == WRITE_10 || opcode == WRITE_16)
		total_write = max_t(u64, total_write + bytes, total_write);

	if (opcode == READ_6 || opcode == READ_10 || opcode == READ_16)
		total_read = max_t(u64, total_read + bytes, total_read);
	host->total_count = max_t(u64, host->total_count + total_read + total_write, host->total_count);
}

static int proc_tran_ufs_err_record_show(struct seq_file *m, void *v)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(g_hba);

	seq_printf(m, "pa_total_err_cnt :%d\n", host->tran_ufs_err.pa_total_err_cnt);
	seq_printf(m, "pa_lane0_err_cnt :%d\n", host->tran_ufs_err.pa_lane0_err_cnt);
	seq_printf(m, "pa_lane1_err_cnt :%d\n", host->tran_ufs_err.pa_lane1_err_cnt);
	seq_printf(m, "pa_lane2_err_cnt:%d\n", host->tran_ufs_err.pa_lane2_err_cnt);
	seq_printf(m, "pa_lane3_err_cnt:%d\n", host->tran_ufs_err.pa_lane3_err_cnt);
	seq_printf(m, "pa_line_reset_cnt:%d\n", host->tran_ufs_err.pa_line_reset_cnt);
	seq_printf(m, "dl_total_err_cnt:%d\n", host->tran_ufs_err.dl_total_err_cnt);
	seq_printf(m, "dl_nac_received_cnt:%d\n", host->tran_ufs_err.dl_nac_received_cnt);
	seq_printf(m, "dl_tcx_reply_timer_expired_cnt:%d\n", host->tran_ufs_err.dl_tcx_reply_timer_expired_cnt);
	seq_printf(m, "dl_afcx_quest_timer_expired_cnt:%d\n", host->tran_ufs_err.dl_afcx_quest_timer_expired_cnt);
	seq_printf(m, "dl_fcx_protection_timer_expired_cnt:%d\n", 
	host->tran_ufs_err.dl_fcx_protection_timer_expired_cnt);
	seq_printf(m, "dl_crc_err_cnt:%d\n", host->tran_ufs_err.dl_crc_err_cnt);
	seq_printf(m, "dl_rx_buffer_overflow_cnt:%d\n", host->tran_ufs_err.dl_rx_buffer_overflow_cnt);
	seq_printf(m, "dl_max_frame_length_exce_cnt:%d\n", host->tran_ufs_err.dl_max_frame_length_exce_cnt);
	seq_printf(m, "dl_wrong_sequence_number_cnt:%d\n", host->tran_ufs_err.dl_wrong_sequence_number_cnt);
	seq_printf(m, "dl_afc_frame_syntax_err_cnt:%d\n", host->tran_ufs_err.dl_afc_frame_syntax_err_cnt);
	seq_printf(m, "dl_nac_frame_syntax_err_cnt:%d\n", host->tran_ufs_err.dl_nac_frame_syntax_err_cnt);
	seq_printf(m, "dl_eof_syntax_err_cnt:%d\n", host->tran_ufs_err.dl_eof_syntax_err_cnt);
	seq_printf(m, "dl_frame_syntax_err_cnt:%d\n", host->tran_ufs_err.dl_frame_syntax_err_cnt);
	seq_printf(m, "dl_bad_ctrl_symbol_type_cnt:%d\n", host->tran_ufs_err.dl_bad_ctrl_symbol_type_cnt);
	seq_printf(m, "dl_pa_init_cnt:%d\n", host->tran_ufs_err.dl_pa_init_cnt);
	seq_printf(m, "dl_pa_error_ind_received_cnt:%d\n", host->tran_ufs_err.dl_pa_error_ind_received_cnt);
	seq_printf(m, "nl_err_cnt:%d\n", host->tran_ufs_err.nl_err_cnt);
	seq_printf(m, "tl_err_cnt:%d\n", host->tran_ufs_err.tl_err_cnt);
	seq_printf(m, "dme_err_cnt:%d\n", host->tran_ufs_err.dme_err_cnt);
	seq_printf(m, "auto_h8_err_cnt:%d\n", host->tran_ufs_err.auto_h8_err_cnt);
	seq_printf(m, "ufs_fatal_err_cnt:%d\n", host->tran_ufs_err.ufs_fatal_err_cnt);
	seq_printf(m, "ufs_device_err_cnt:%d\n", host->tran_ufs_err.ufs_device_err_cnt);
	seq_printf(m, "ufs_host_err_cnt: %d\n", host->tran_ufs_err.ufs_host_err_cnt);
	seq_printf(m, "ufs_bus_err_cnt:%d\n", host->tran_ufs_err.ufs_bus_err_cnt);
	seq_printf(m, "ufs_crypto_err_cnt:%d\n", host->tran_ufs_err.ufs_crypto_err_cnt);
	seq_printf(m, "link_startup_fail_cnt:%d\n", host->tran_ufs_err.link_startup_fail_cnt);
	seq_printf(m, "resume_err_cnt:%d\n", host->tran_ufs_err.resume_err_cnt);
	seq_printf(m, "suspend_err_cnt:%d\n", host->tran_ufs_err.suspend_err_cnt);
	seq_printf(m, "dev_reset_cnt:%d\n", host->tran_ufs_err.dev_reset_cnt);
	seq_printf(m, "host_reset_cnt:%d\n", host->tran_ufs_err.host_reset_cnt);
	seq_printf(m, "abort_cnt:%d\n", host->tran_ufs_err.abort_cnt);

	return 0;
}

static int proc_tran_ufs_err_record_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, proc_tran_ufs_err_record_show, NULL);
}

const struct proc_ops proc_tran_ufs_err_record_fops=
{
	.proc_open=proc_tran_ufs_err_record_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};

static int proc_tran_ufs_err_upload_show(struct seq_file *m, void *v)
{
	struct ufs_hba *hba= g_hba;
	struct ufs_event_hist *e = hba->ufs_stats.event;
	int i = 0;

	for(i = 0; i < UFS_EVT_CNT - 1 - 1; i++) {
		seq_printf(m, "%llu,", e[i].cnt);
	}
	seq_printf(m, "%llu", e[UFS_EVT_CNT - 1].cnt);

	return 0;
}

static int proc_tran_ufs_err_upload_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, proc_tran_ufs_err_upload_show, NULL);
}


const struct proc_ops proc_tran_ufs_err_upload_fops=
{
	.proc_open=proc_tran_ufs_err_upload_open,
	.proc_read=seq_read,
	.proc_release = single_release,
};


#ifdef CONFIG_TRAN_UFS_ERR_RECORD_DEBUG
static ssize_t proc_tran_ufs_err_record_write(struct file *file, const char *buf,
				 size_t count, loff_t *data)
{
	char *tmp = kzalloc((count+1), GFP_KERNEL);
	char *buf_tmp;
	int i = 0;
	int evt_type;
	int reg;

	if (copy_from_user(tmp,buf,count))
	{
		dev_err(g_hba->dev, "ufs_error_record_debug, copy_from_user fail!\n");
		return -EFAULT;
	}

	while ((buf_tmp = strsep(&tmp, ",")) != NULL) {
		if (i == 0 && kstrtoint(buf_tmp, 0, &evt_type)) {
			kfree(tmp);
			return -EINVAL;
		}
		if (i == 1 && kstrtoint(buf_tmp, 0, &reg)) {
			kfree(tmp);
			return -EINVAL;
		}
		i++;
	}
	dev_err(g_hba->dev, "ufs_error_record_debug, evt_type:%d!\n", evt_type);
	dev_err(g_hba->dev, "ufs_error_record_debug, reg:0x%x!\n", reg);
	tran_record_uic_err(g_hba, evt_type, reg);

	kfree(tmp);
	return count;
}

const struct proc_ops proc_tran_ufs_err_record_debug_fops=
{
	.proc_open=proc_tran_ufs_err_record_open,
	.proc_read=seq_read,
	.proc_write=proc_tran_ufs_err_record_write,
	.proc_release = single_release,
};
#endif


static int tran_ufs_err_record_create_procfs(struct ufs_hba *hba)
{
	tran_ufs_err_record_entry = proc_mkdir(TRAN_FLASH_ERR_RECORD_DIR, NULL);
	if (tran_ufs_err_record_entry == NULL)
	{
		dev_err(hba->dev, "[tran_err_record] Error: Could not initialize %s\n",
			TRAN_FLASH_ERR_RECORD_DIR);
		return -ENODEV;
	}

	proc_create("tran_ufs_err_record", S_IRUGO, tran_ufs_err_record_entry,
		&proc_tran_ufs_err_record_fops);
	proc_create("tran_ufs_error_upload", S_IRUGO, tran_ufs_err_record_entry,
		&proc_tran_ufs_err_upload_fops);
#ifdef CONFIG_TRAN_UFS_ERR_RECORD_DEBUG
	proc_create("tran_ufs_err_record_debug", 0644, tran_ufs_err_record_entry,
		&proc_tran_ufs_err_record_debug_fops);
#endif

	return 0;
}

#ifdef CONFIG_TRAN_UFS_EXCEPTION_UPLOAD
static int uic_history_count = 0;

static LIST_HEAD(uic_history_list);

int list_length(void)
{
	return uic_history_count;
}

void add_node(unsigned long uic_count, unsigned long long transfer_bytes, ktime_t time)
{
	struct ufs_uic_err_history *new_node;

	if (list_length() >= 10) {
		struct ufs_uic_err_history *first_node =
		list_first_entry(&uic_history_list, struct ufs_uic_err_history, list);
		list_del(&first_node->list);
		kfree(first_node);
		uic_history_count--;
	}

	new_node = kmalloc(sizeof(struct ufs_uic_err_history), GFP_KERNEL);
	if (!new_node) {
		dev_err(g_hba->dev, "[TRAN_UFS_UER]Memory allocation failed\n");
		return;
	}
	new_node->uic_err = uic_count;
	new_node->transfered_bytes = transfer_bytes;
	new_node->time = time;
	list_add_tail(&new_node->list, &uic_history_list);
	uic_history_count++;
}

void clear_list(void)
{
	struct ufs_uic_err_history *node, *tmp;
	list_for_each_entry_safe(node, tmp, &uic_history_list, list) {
		list_del(&node->list);
		kfree(node);
	}
	uic_history_count = 0;
}

static void ufshcd_print_evt(struct ufs_hba *hba, u32 id,
			     char *err_name)
{
	int i;
	bool found = false;
	struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];

	for (i = 0; i < UFS_EVENT_HIST_LENGTH; i++) {
		int p = (i + e->pos) % UFS_EVENT_HIST_LENGTH;

		if (e->tstamp[p] == 0)
			continue;
		dev_err(hba->dev, "%s[%d] = 0x%x at %lld us\n", err_name, p,
			e->val[p], ktime_to_us(e->tstamp[p]));
		found = true;
	}

	if (!found)
		dev_err(hba->dev, "No record of %s\n", err_name);
}


static void print_uic_err_history(struct ufs_hba *hba)
{
	struct ufs_uic_err_history *node;
	list_for_each_entry(node, &uic_history_list, list) {
		dev_err(hba->dev, "uic_err_cnt :%lu, transfered_bytes :%llu\n", 
		node->uic_err, node->transfered_bytes);
	}

	ufshcd_print_evt(hba, UFS_EVT_PA_ERR, "pa_err");
	ufshcd_print_evt(hba, UFS_EVT_DL_ERR, "dl_err");
	ufshcd_print_evt(hba, UFS_EVT_NL_ERR, "nl_err");
	ufshcd_print_evt(hba, UFS_EVT_TL_ERR, "tl_err");
}

static void tran_upload_ufs_err_record_tne(void)
{
#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT) && IS_ENABLED(CONFIG_TRAN_TNEK_SUPPORT)
#ifdef CONFIG_TRAN_UFS_EXCEPTION_USE_TNE
	tnek_caller_api("0xffffff28", USER_TNE_FLAG, 0, NULL);
#endif
#endif
}

static void tran_ufs_uic_err_need_report(struct ufs_mtk_host *host)
{
	struct ufs_hba *hba = host->hba;
	struct ufs_uic_err_history *third_last, *second_last, *last;

	//add new uic_err info to list
	add_node(host->uic_err_cnt, host->total_count, ktime_get());
	//Get the latest delta uic err cnt && transfer bytes
	if(list_length() < 3)
		return;

	last = list_last_entry(&uic_history_list, struct ufs_uic_err_history, list);
	if (&last->list == uic_history_list.next) {
		dev_err(hba->dev, "Not enough nodes in history list.\n");
		return;
	}
	second_last = list_prev_entry(last, list);
	if (&second_last->list == uic_history_list.next) {
		dev_err(hba->dev, "Not enough nodes in history list.\n");
		return;
	}
	third_last = list_prev_entry(second_last, list);

	/*and adjacent uic errors exceed the 1/10^10 threshold.upload tne*/
	if (((last->uic_err - second_last->uic_err) * UFS_SIZE_1GB >
		last->transfered_bytes - second_last->transfered_bytes) &&
		((second_last->uic_err - third_last->uic_err) * UFS_SIZE_1GB >
		second_last->transfered_bytes - third_last->transfered_bytes)) {
		//Print the uic error information stored in the linked list
		dev_err(hba->dev, "%s:uic_err_cnt= %lu, hba->total_count = %llu\n",
				__func__,
				host->uic_err_cnt,
				host->total_count);
		dev_err(hba->dev, "Comparing last nodes: uic_cnt %lu, transfer_bytes:%llu, time:%lld ms\n",
			last->uic_err, last->transfered_bytes, ktime_to_ms(last->time));
		dev_err(hba->dev, "Comparing second_last nodes: uic_cnt %lu, transfer_bytes:%llu, time:%lld ms\n",
			second_last->uic_err, second_last->transfered_bytes, ktime_to_ms(second_last->time));
		dev_err(hba->dev, "Comparing third_last nodes: uic_cnt %lu, transfer_bytes:%llu, time:%lld ms\n",
			third_last->uic_err, third_last->transfered_bytes, ktime_to_ms(third_last->time));
		tran_upload_ufs_err_record_tne();
		print_uic_err_history(hba);
		return;
	}

	dev_err(hba->dev, "%s: UIC error cnt below threshold, do not report!\n", __func__);
	return;
}

extern bool uer_cloudctl_statu;
void tran_ufs_driver_expection_upload(void)
{
	struct ufs_hba *hba= g_hba;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if(uer_cloudctl_statu == false)
		return;

//uic error upload
	host->uic_err_cnt = host->tran_ufs_err.pa_total_err_cnt +
		host->tran_ufs_err.dl_total_err_cnt +
		host->tran_ufs_err.nl_err_cnt +
		host->tran_ufs_err.tl_err_cnt;
	if(host->uic_err_cnt && host->uic_err_cnt != host->last_uic_err_cnt) {
		host->last_uic_err_cnt = host->uic_err_cnt;
		tran_ufs_uic_err_need_report(host);
	}
//dme error upload
	if(host->tran_ufs_err.dme_err_cnt > 0 &&
		host->tran_ufs_err.dme_err_cnt != 
		host->last_ufs_err_record[UFS_EVT_DME_ERR]) {
		host->last_ufs_err_record[UFS_EVT_DME_ERR] = host->tran_ufs_err.dme_err_cnt;
		tran_upload_ufs_err_record_tne();
	}
//autoh8 error upload
	if(host->tran_ufs_err.auto_h8_err_cnt > 0 &&
		host->tran_ufs_err.auto_h8_err_cnt != 
		host->last_ufs_err_record[UFS_EVT_AUTO_HIBERN8_ERR]) {
		host->last_ufs_err_record[UFS_EVT_AUTO_HIBERN8_ERR] =
			host->tran_ufs_err.auto_h8_err_cnt;
		tran_upload_ufs_err_record_tne();
	}
//fatal error upload
	if(host->tran_ufs_err.ufs_fatal_err_cnt > 0 &&
		host->tran_ufs_err.ufs_fatal_err_cnt != 
		host->last_ufs_err_record[UFS_EVT_FATAL_ERR]) {
		host->last_ufs_err_record[UFS_EVT_FATAL_ERR] = host->tran_ufs_err.ufs_fatal_err_cnt;
		tran_upload_ufs_err_record_tne();
	}
//linstartup fail upload
	if(host->tran_ufs_err.link_startup_fail_cnt > 0 &&
		host->tran_ufs_err.link_startup_fail_cnt != 
		host->last_ufs_err_record[UFS_EVT_LINK_STARTUP_FAIL]) {
		host->last_ufs_err_record[UFS_EVT_LINK_STARTUP_FAIL] = host->tran_ufs_err.link_startup_fail_cnt;
		tran_upload_ufs_err_record_tne();
	}
//resume error upload
	if(host->tran_ufs_err.resume_err_cnt > 0 &&
		host->tran_ufs_err.resume_err_cnt != 
		host->last_ufs_err_record[UFS_EVT_RESUME_ERR]) {
		host->last_ufs_err_record[UFS_EVT_RESUME_ERR] = host->tran_ufs_err.resume_err_cnt;
		tran_upload_ufs_err_record_tne();
	}
//suspend error upload
	if(host->tran_ufs_err.suspend_err_cnt > 0 &&
		host->tran_ufs_err.suspend_err_cnt != 
		host->last_ufs_err_record[UFS_EVT_SUSPEND_ERR]) {
		host->last_ufs_err_record[UFS_EVT_SUSPEND_ERR] = host->tran_ufs_err.suspend_err_cnt;
		tran_upload_ufs_err_record_tne();
	}
}

void tran_ufs_err_record_handler(struct work_struct *work)
{
	tran_ufs_driver_expection_upload();
}

void timer_func(struct timer_list *t)
{
	schedule_work(&tran_uer_work);
	mod_timer(&timer, jiffies + msecs_to_jiffies(TRAN_UFS_ERR_REOCRD_SCHEDULE_TIME));
}

void tran_ufs_err_reocord_timer_init(struct ufs_hba *hba)
{
	timer_setup(&timer, timer_func, 0);
	timer.expires = jiffies + msecs_to_jiffies(TRAN_UFS_ERR_REOCRD_SCHEDULE_TIME);
	add_timer(&timer);
}
#endif

void tran_record_uic_err(struct ufs_hba *hba, enum ufs_event_type evt, u32 data)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	unsigned long err_bits;
	int ec;

	dev_err(hba->dev, "%s current event type:%d, reg_val:%d\n", __func__, evt, data);
	switch (evt)
	{
		case UFS_EVT_PA_ERR:
			err_bits = data & UIC_PHY_ADAPTER_LAYER_ERROR_CODE_MASK;
			for_each_set_bit(ec, &err_bits, UNIPRO_PA_ERR_MAX) {
				host->tran_ufs_err.pa_total_err_cnt++;
			}
			if(data & TRAN_UIC_PHY_ADAPTER_LAYER_LANE0_ERROR)
				host->tran_ufs_err.pa_lane0_err_cnt++;
			if(data & TRAN_UIC_PHY_ADAPTER_LAYER_LANE1_ERROR)
				host->tran_ufs_err.pa_lane1_err_cnt++;
			if(data & TRAN_UIC_PHY_ADAPTER_LAYER_LANE2_ERROR)
				host->tran_ufs_err.pa_lane2_err_cnt++;
			if(data & TRAN_UIC_PHY_ADAPTER_LAYER_LANE3_ERROR)
				host->tran_ufs_err.pa_lane3_err_cnt++;
			if(data & TRAN_UIC_PHY_ADAPTER_LAYER_GENERIC_ERROR)
				host->tran_ufs_err.pa_line_reset_cnt++;
			break;
		case UFS_EVT_DL_ERR:
			err_bits = data & UIC_DATA_LINK_LAYER_ERROR_CODE_MASK;
			for_each_set_bit(ec, &err_bits, UNIPRO_DL_ERR_MAX) {
				host->tran_ufs_err.dl_total_err_cnt++;
			}
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_NAC_RECEIVED)
				host->tran_ufs_err.dl_nac_received_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_TCX_REP_TIMER_EXP)
				host->tran_ufs_err.dl_tcx_reply_timer_expired_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_AFCX_REQ_TIMER_EXP)
				host->tran_ufs_err.dl_afcx_quest_timer_expired_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_FCX_PRO_TIMER_EXP)
				host->tran_ufs_err.dl_fcx_protection_timer_expired_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_CRC_ERR)
				host->tran_ufs_err.dl_crc_err_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_NAC_RECEIVED)
				host->tran_ufs_err.dl_nac_received_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_RX_BUF_OF)
				host->tran_ufs_err.dl_rx_buffer_overflow_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_MAX_FRAME_LENGTH_EXCE)
				host->tran_ufs_err.dl_max_frame_length_exce_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_WRONG_SEQ_NUM)
				host->tran_ufs_err.dl_wrong_sequence_number_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_AFC_FRAME_SYNTAX_ERR)
				host->tran_ufs_err.dl_afc_frame_syntax_err_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_NAC_FRAME_SYNTAX_ERR)
				host->tran_ufs_err.dl_nac_frame_syntax_err_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_EOF_SYNTAX_ERR)
				host->tran_ufs_err.dl_eof_syntax_err_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_FRAME_SYNTAX_ERR)
				host->tran_ufs_err.dl_frame_syntax_err_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_BAD_CTRL_SYMBOL_TYPE)
				host->tran_ufs_err.dl_bad_ctrl_symbol_type_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_PA_INIT)
				host->tran_ufs_err.dl_pa_init_cnt++;
			if(data & TRAN_UIC_DATA_LINK_LAYER_ERROR_PA_ERR_IND_RECEIVED)
				host->tran_ufs_err.dl_pa_error_ind_received_cnt++;
			break;
		case UFS_EVT_NL_ERR:
			err_bits = data & UIC_NETWORK_LAYER_ERROR_CODE_MASK;
			for_each_set_bit(ec, &err_bits, UNIPRO_NL_ERR_MAX) {
				host->tran_ufs_err.nl_err_cnt++;
			}
			break;
		case UFS_EVT_TL_ERR:
			err_bits = data & UIC_TRANSPORT_LAYER_ERROR_CODE_MASK;
			for_each_set_bit(ec, &err_bits, UNIPRO_TL_ERR_MAX) {
				host->tran_ufs_err.tl_err_cnt++;
			}
			break;
		case UFS_EVT_DME_ERR:
			err_bits = data & UIC_DME_ERROR_CODE_MASK;
			for_each_set_bit(ec, &err_bits, UNIPRO_DME_ERR_MAX) {
				host->tran_ufs_err.dme_err_cnt++;
			}
			break;
		case UFS_EVT_AUTO_HIBERN8_ERR:
			host->tran_ufs_err.auto_h8_err_cnt++;
			break;
		case UFS_EVT_FATAL_ERR:
			if (DEVICE_FATAL_ERROR & data)
				host->tran_ufs_err.ufs_device_err_cnt++;
			if (CONTROLLER_FATAL_ERROR & data)
				host->tran_ufs_err.ufs_host_err_cnt++;
			if (SYSTEM_BUS_FATAL_ERROR & data)
				host->tran_ufs_err.ufs_bus_err_cnt++;
			if (CRYPTO_ENGINE_FATAL_ERROR & data)
				host->tran_ufs_err.ufs_crypto_err_cnt++;
			host->tran_ufs_err.ufs_fatal_err_cnt = 
				host->tran_ufs_err.ufs_device_err_cnt +
				host->tran_ufs_err.ufs_host_err_cnt +
				host->tran_ufs_err.ufs_bus_err_cnt+
				host->tran_ufs_err.ufs_crypto_err_cnt;
			break;
		case UFS_EVT_LINK_STARTUP_FAIL:
			host->tran_ufs_err.link_startup_fail_cnt++;
			break;
		case UFS_EVT_RESUME_ERR:
			host->tran_ufs_err.resume_err_cnt++;
			break;
		case UFS_EVT_SUSPEND_ERR:
			host->tran_ufs_err.suspend_err_cnt++;
			break;
		case UFS_EVT_DEV_RESET:
			host->tran_ufs_err.dev_reset_cnt++;
			break;
		case UFS_EVT_HOST_RESET:
			host->tran_ufs_err.host_reset_cnt++;
			break;
		case UFS_EVT_ABORT:
			host->tran_ufs_err.abort_cnt++;
			break;
		default:
			break;
	}
}


void tran_ufs_err_record_init(struct ufs_hba *hba)
{
	int ret = 0;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_event_hist *e = hba->ufs_stats.event;
	int i = 0;

	g_hba = hba;
	//init ufs driver except count;
	for(i = 0; i < UFS_EVT_CNT; i++) {
		host->last_ufs_err_record[i] = e[i].cnt;
	}
	//create proc node to record ufs driver except info
	ret = tran_ufs_err_record_create_procfs(hba);
	if (ret)
		dev_err(hba->dev, "create ufs_err_record fail!\n");

#ifdef CONFIG_TRAN_UFS_EXCEPTION_UPLOAD
	//init ufs driver except update workqueue
	INIT_WORK(&tran_uer_work, tran_ufs_err_record_handler);

	//create timer to get ufs driver except
	tran_ufs_err_reocord_timer_init(hba);
	host->last_uic_err_cnt = 0;
#endif

	return;
}EXPORT_SYMBOL(tran_ufs_err_record_init);

void tran_ufs_err_record_exit(void)
{
#ifdef CONFIG_TRAN_UFS_EXCEPTION_UPLOAD
	cancel_work_sync(&tran_uer_work);
#endif
	remove_proc_entry("tran_ufs_err_record", tran_ufs_err_record_entry);
	remove_proc_entry("tran_ufs_error_upload", tran_ufs_err_record_entry);
#ifdef CONFIG_TRAN_UFS_ERR_RECORD_DEBUG
	remove_proc_entry("tran_ufs_err_record_debug", tran_ufs_err_record_entry);
#endif
#ifdef CONFIG_TRAN_UFS_EXCEPTION_UPLOAD
	del_timer(&timer);
	clear_list();
#endif
}
