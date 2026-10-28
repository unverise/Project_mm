// Creator: Transsion Display Team
// Creat Time: 2023/11/21
/* Note: This file is created by Transsion Display Team.
 *       This file is used to add lcm debug note to sysfs.
 *       path: /sys/kernel/tran_display/
 */
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_encoder.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/component.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <video/mipi_display.h>
#include <video/videomode.h>
#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif
#include <linux/ratelimit.h>
#include <soc/mediatek/smi.h>
#include <linux/leds-mtk.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_helper.h"
#include "mtk_mipi_tx.h"
#include "mtk_dump.h"
#include "mtk_log.h"
#include "mtk_drm_lowpower.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_arr.h"
#include "mtk_panel_ext.h"
#include "mtk_disp_notify.h"
#include "tran_display.h"
//USD: add pcd check by jin.zhao AR-202507-001417 20250805 start
#include "mtk_disp_bdg.h"
//USD: add pcd check by jin.zhao AR-202507-001417 20250805 end

// USD:add lcm pcd by diao.lu 20241231 start
extern void mipi_dsi_dcs_write_gce2(struct mtk_dsi *dsi, struct cmdq_pkt *dummy,
					const void *data, size_t len);
// USD:add lcm pcd by diao.lu 20241231 end
static struct mtk_dsi *g_tran_dsi[DDP_DSI_MAX] = {NULL};
struct lcm_node lcm_node_state = {0};

/**
 * tran_get_bl_opt_support - Check if backlight optimization is supported
 *
 * This function checks whether the backlight optimization feature is enabled
 * for the display panel. The optimization is designed to address the
 * backlight logic confusion issue observed in X6895AEE-5207. When enabled,
 it implements three optimization strategies:
 * 1. Lower PQ priority
 * 2. Increase set_backlight priority
 * 3. Extend VBP write instruction interval
 *
 * Returns:
 * true  - Backlight optimization is supported/enabled
 * false - Backlight optimization is not supported
 */
bool tran_get_bl_opt_support(void)
{
	if (g_tran_dsi[DDP_DSI0] && g_tran_dsi[DDP_DSI0]->ext && g_tran_dsi[DDP_DSI0]->ext->params &&
		g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params &&
		g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params->tran_lcd_bl_opt_support)
	{
		return true;
	}
	return false;
}
EXPORT_SYMBOL_GPL(tran_get_bl_opt_support);


void tran_dsi_init(struct mtk_dsi *dsi)
{
	if (dsi->ddp_comp.id == DDP_COMPONENT_DSI0) {
		g_tran_dsi[DDP_DSI0] = dsi;
	} else if (dsi->ddp_comp.id == DDP_COMPONENT_DSI1)
	{
		g_tran_dsi[DDP_DSI1] = dsi;
	} else {
		DDPPR_ERR("%s, dsi error\n", __func__);
	}

	return;
}
struct mtk_dsi *tran_get_dsi(enum DDP_DSI_NUM index)
{
	if (index >= DDP_DSI_MAX) {
		DDPPR_ERR("%s index %d err!\n", __func__, index);
		return NULL;
	}
	if (g_tran_dsi[index] == NULL) {
		DDPPR_ERR("%s dsi[%d] is null!\n", __func__, index);
		return NULL;
	}

	return g_tran_dsi[index];
}

//USD: add pcd check by jin.zhao AR-202507-001417 20250805 start
static struct drm_device *tran_drm_dev;
void tran_disp_init(struct drm_device *dev)
{
	if (IS_ERR_OR_NULL(dev))
		DDPMSG("%s, tran disp init with invalid dev\n", __func__);
	else
		DDPMSG("%s, tran disp init\n", __func__);
	tran_drm_dev = dev;

	return ;
}
EXPORT_SYMBOL_GPL(tran_disp_init);


#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
static void pcd_read_cmdq_timeout_cb(struct cmdq_cb_data data)
{
	struct drm_crtc *crtc = data.data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp = NULL;

	if (!crtc) {
		DDPMSG("%s find crtc fail\n", __func__);
		return;
	}

	DDPINFO("[error]%s cmdq timeout out\n", __func__);

	if (is_bdg_supported()) {
		if (mtk_crtc) {
			output_comp = mtk_ddp_comp_request_output(mtk_crtc);
			if (output_comp) {
				mtk_dump_analysis(output_comp);
				mtk_dump_reg(output_comp);
			}
		}
		bdg_dsi_dump_reg(DISP_BDG_DSI0);
	} else {
		mtk_drm_crtc_analysis(crtc);
		mtk_drm_crtc_dump(crtc);
	}
}

static int pcd_check_read(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp = NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	struct cmdq_pkt *cmdq_handle = NULL;
	int ret = 0;

	DDPINFO("[PCD%u] PCD read panel\n", drm_crtc_index(crtc));


	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s:invalid output comp\n", __func__);
		return -EINVAL;
	}

	if (mtk_drm_is_idle(crtc) && mtk_dsi_is_cmd_mode(output_comp))
		return 0;

	mtk_ddp_comp_io_cmd(output_comp, NULL, REQ_PANEL_EXT, &panel_ext);
	if (unlikely(!(panel_ext && panel_ext->params))) {
		DDPPR_ERR("%s:can't find panel_ext handle\n", __func__);
		return -EINVAL;
	}

	cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
	cmdq_handle->err_cb.cb = pcd_read_cmdq_timeout_cb;
	cmdq_handle->err_cb.data = crtc;

	if (mtk_dsi_is_cmd_mode(output_comp)) {
		if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
						 DDP_SECOND_PATH, 0);
		else
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
						 DDP_FIRST_PATH, 0);

		cmdq_pkt_clear_event(cmdq_handle,
				     mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);

		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, PCD_CHECK_READ,
				    (void *)mtk_crtc);

		cmdq_pkt_set_event(cmdq_handle,
				   mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
	//USD add pcd check v2 by long.chen 20250829 start
	} else if (mtk_dsi_cmd_version() == DSI_CMD_V2) { /* VDO mode v2 */
		if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_SECOND_PATH, 1);
		else
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 1);

		mtk_use_cabc_event(cmdq_handle, mtk_crtc, WAIT_AND_CLEAR_OPT, __LINE__);
		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, PCD_CHECK_READ,
				    (void *)mtk_crtc);
		mtk_use_cabc_event(cmdq_handle, mtk_crtc, SET_OPT, __LINE__);
	//USD add pcd check v2 by long.chen 20250829 end
	} else { /* VDO mode */
		if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
						 DDP_SECOND_PATH, 1);
		else
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
						 DDP_FIRST_PATH, 1);

		if (mtk_crtc->msync2.msync_on) {
			u32 vfp_early_stop = 1;
			mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_VFP_EARLYSTOP,
							&vfp_early_stop);
		}

		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_STOP_VDO_MODE,
				    NULL);

		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, PCD_CHECK_READ,
				    (void *)mtk_crtc);

		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle,
				    DSI_START_VDO_MODE, NULL);

		mtk_disp_mutex_trigger(mtk_crtc->mutex[0], cmdq_handle);
		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, COMP_REG_START,
				    NULL);
	}

	cmdq_pkt_flush(cmdq_handle);
	mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_READ_EPILOG,
				    NULL);
	ret = mtk_ddp_comp_io_cmd(output_comp, NULL, PCD_CHECK_CMP,
				  (void *)mtk_crtc);
	cmdq_pkt_destroy(cmdq_handle);
	return ret;
}

static void pcd_set_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;
	DDPINFO("%s\n", __func__);
	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

//USD: add pcd check by jin.zhao AR-202507-001417 20260119 start
static int mtk_drm_set_pcd_with_gce3(struct drm_crtc *crtc, bool state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	int ret = 0;

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!(mtk_crtc->enabled)) {
		DDPINFO("Sleep State set pcd tracking stop --crtc not ebable\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		return 0;
	}

	if (!(comp && mtk_ddp_comp_get_type(comp->id) == MTK_DSI)) {
		DDPINFO("%s no output comp\n", __func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		return 0;
	}

	mtk_drm_idlemgr_kick(__func__, crtc, 0);
	if (comp->funcs && comp->funcs->io_cmd)
		comp->funcs->io_cmd(comp, NULL, DSI_SET_PCD, &state);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	return ret;
}
//USD: add pcd check by jin.zhao AR-202507-001417 20260119 end

static int mtk_drm_set_pcd(struct drm_crtc *crtc, bool state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle;
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct mtk_cmdq_cb_data *cb_data;
	struct cmdq_client *client;
	bool is_frame_mode;
	int ret = 0;

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!(mtk_crtc->enabled)) {
		DDPINFO("Sleep State set pcd tracking stop --crtc not ebable\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		return 0;
	}

	if (!(comp && mtk_ddp_comp_get_type(comp->id) == MTK_DSI)) {
		DDPINFO("%s no output comp\n", __func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		return 0;
	}

	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		DDPPR_ERR("cb data creation failed\n");
		return -EINVAL;
	}

	is_frame_mode = mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base);

	/* set pcd would use VM CMD in  DSI VDO mode only */
	client = (is_frame_mode) ? mtk_crtc->gce_obj.client[CLIENT_CFG] :
					mtk_crtc->gce_obj.client[CLIENT_DSI_CFG];
	cmdq_handle = cmdq_pkt_create(client);

	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_SECOND_PATH, 0);
	else
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_FIRST_PATH, is_bdg_supported() ? 1: 0);

	if (is_frame_mode) {
		cmdq_pkt_clear_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);

	}
	mtk_drm_idlemgr_kick(__func__, crtc, 0);
	/* set pcd */
	if (comp->funcs && comp->funcs->io_cmd) {
		if (is_bdg_supported()) {
			comp->funcs->io_cmd(comp, cmdq_handle, DSI_STOP_VDO_MODE, NULL);
			comp->funcs->io_cmd(comp, cmdq_handle, DSI_SET_PCD, &state);
			comp->funcs->io_cmd(comp, cmdq_handle, DSI_START_VDO_MODE, NULL);
			mtk_disp_mutex_trigger(mtk_crtc->mutex[0], cmdq_handle);
			comp->funcs->io_cmd(comp, cmdq_handle, COMP_REG_START, NULL);
			DDPINFO("%s bdg set pcd tracking\n", __func__);
		} else
			comp->funcs->io_cmd(comp, cmdq_handle, DSI_SET_PCD, &state);
	}

	if (is_frame_mode) {
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	}

	cb_data->crtc = crtc;
	cb_data->cmdq_handle = cmdq_handle;
	if (cmdq_pkt_flush_threaded(cmdq_handle, pcd_set_cmdq_cb, cb_data) < 0) {
		DDPPR_ERR("failed to flush bl_cmdq_cb\n");
		ret = -EINVAL;
	}

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return ret;
}
#endif
//USD: add pcd check by jin.zhao AR-202507-001417 20250804 end

static ssize_t lcm_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", g_tran_dsi[DDP_DSI0]->panel->dev->driver->name);
}
static ssize_t lcm_sub_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", g_tran_dsi[DDP_DSI1]->panel->dev->driver->name);
}

static ssize_t tran_lcm_hbm_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	bool panel_hbm_state = false;
	if (g_tran_dsi[DDP_DSI0] && g_tran_dsi[DDP_DSI0]->ext && g_tran_dsi[DDP_DSI0]->ext->params &&
		g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params)
	{
		panel_hbm_state = g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params->panel_hbm_state;
	}

	return scnprintf(buf, PAGE_SIZE, "0xFA\n");
}
static ssize_t tran_lcm_hbm_store(struct kobject *kobj,
								  struct kobj_attribute *attr, const char *buf, size_t size)
{
	unsigned long value = 0;
	ssize_t ret = 0;

	ret = kstrtoul(buf, 10, &value);
	if (ret)
		return ret;

	pr_info("%s tran_lcm_hbm_store value= %ld \n", __func__, value);

	if (value == 1) {
		tran_lcm_hbm_set(true);
	} else if (value == 0) {
		tran_lcm_hbm_set(false);
	}
	ret = size;
	return ret;
}

static ssize_t tran_lcm_lcm_dimming_state_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned int dimming_status = 0;
	if (g_tran_dsi[DDP_DSI0] && g_tran_dsi[DDP_DSI0]->ext && g_tran_dsi[DDP_DSI0]->ext->params &&
		g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params)
	{
		dimming_status = g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params->dimming_status;
	}
	return sprintf(buf, "dimming status:%d", dimming_status);
}
static ssize_t tran_lcm_lcm_dimming_state_store(struct kobject *kobj,
												struct kobj_attribute *attr, const char *buf, size_t size)
{
	unsigned int dimming_status = 0;
	unsigned long value = 0;
	ssize_t ret = 0;

	ret = kstrtoul(buf, 10, &value);
	if (ret)
		return ret;
	pr_err("%s value= %ld \n", __func__, value);

	switch (value) {
	case 0:
	case 1024:
		dimming_status = 0;
		break;
	case 1:
	case 2048:
		dimming_status = 1;
		break;
	default:
		ret = -EFAULT;
		goto end;
	}

	if (g_tran_dsi[DDP_DSI0] && g_tran_dsi[DDP_DSI0]->ext &&
		g_tran_dsi[DDP_DSI0]->ext->params && g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params)
	{
		int tran_dimming_data = dimming_status == 0 ? TRAN_DISP_BLANK_ENTER_DIMMING_OFF_NOTIFY : TRAN_DISP_BLANK_ENTER_DIMMING_ON_NOTIFY;
		g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params->dimming_status = dimming_status;
		mtk_disp_notifier_call_chain(MTK_DISP_EVENT_BLANK, &tran_dimming_data);
		pr_err("%s dimming_status = %d notify tran_dimming_data = %d\n", __func__, dimming_status, tran_dimming_data);
	} else {
		pr_err("%s panel dimming_status not init ! %ld \n", __func__, value);
	}
	ret = size;
end:
	return ret;
}

#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
static ssize_t lcm_doze_backlight_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	if (g_tran_dsi[DDP_DSI0] && g_tran_dsi[DDP_DSI0]->ext && g_tran_dsi[DDP_DSI0]->ext->params &&
		g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params &&
		g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params->lcm_doze_backlight)
	{
		return sprintf(buf, "lcm_doze_backlight:%d;%d;%d;%d;", g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params->lcm_doze_backlight->doze_backlight_num,
					g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params->lcm_doze_backlight->doze_backlight_level1,
					g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params->lcm_doze_backlight->doze_backlight_level2,
					g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params->lcm_doze_backlight->doze_backlight_level3);
	} else {
		return sprintf(buf, "lcm_doze_backlight not init !\n");
	}
}
#endif

static void tran_lcm_node_func(const char *opt)
{
	int ret;
	if (strncmp(opt, "tran_backlight:", 15) == 0) {
		unsigned int level;

		ret = sscanf(opt, "tran_backlight:%u\n", &level);
		if (ret != 1) {
			DDPPR_ERR("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}
		lcm_node_state.tran_backlight = level;
		tran_lcm_backlight_set(level);
		if (g_tran_dsi[DDP_DSI0]) {
			update_tran_brightness(g_tran_dsi[DDP_DSI0]->conn.base.id, (int)level);
		}
	} else if (strncmp(opt, "tran_backlight_type:", 20) == 0) {
		unsigned int type;

		ret = sscanf(opt, "tran_backlight_type:%u\n", &type);
		if (ret != 1) {
			DDPPR_ERR("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}
		if (type > TRAN_LCM_BACKLIGHT_TYPE_FACTORY) {
			DDPPR_ERR("%s invalid backlight type %u\n", __func__, type);
			return;
		}
		lcm_node_state.tran_backlight_type = (enum TRAN_LCM_BACKLIGHT_TYPE)type;
	}
}

enum TRAN_LCM_BACKLIGHT_TYPE tran_lcm_backlight_get_type(void)
{
	return lcm_node_state.tran_backlight_type;
}

static ssize_t tran_lcm_node_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = 0;

	tran_lcm_node_func(buf);
	ret = size;
	return ret;
}

static ssize_t tran_lcm_node_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "tran_backlight:%d, tran_backlight_type:%d;",
			lcm_node_state.tran_backlight, lcm_node_state.tran_backlight_type);
}

#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
static ssize_t lcm_pcd_check_by_read_lcm_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned int g_lcm_pcd_check_by_read_lcm = 0;
	//USD: add pcd check by jin.zhao AR-202507-001417 20250805 start
	struct drm_crtc *crtc;

	if (g_tran_dsi[DDP_DSI0]->ext->params->lcm_pcd_check_table[0].cmd != 0) {
		if (IS_ERR_OR_NULL(tran_drm_dev)) {
			DDPPR_ERR("%s, invalid drm dev\n", __func__);
			return -EINVAL;
		}

		crtc = list_first_entry(&(tran_drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
		if (IS_ERR_OR_NULL(crtc)) {
			DDPPR_ERR("%s failed to find crtc\n", __func__);
			return -EINVAL;
		}
		//USD: add pcd check by jin.zhao AR-202507-001417 20260119 start
		if (g_tran_dsi[DDP_DSI0]->ext->params->set_pcd_params_with_gec3 == 1)
			mtk_drm_set_pcd_with_gce3(crtc, true);
		else
			mtk_drm_set_pcd(crtc, true);
		g_lcm_pcd_check_by_read_lcm = pcd_check_read(crtc);
		if (g_tran_dsi[DDP_DSI0]->ext->params->set_pcd_params_with_gec3 == 1)
			mtk_drm_set_pcd_with_gce3(crtc, false);
		else
			mtk_drm_set_pcd(crtc, false);
		//USD: add pcd check by jin.zhao AR-202507-001417 20260119 end
	}
	//USD: add pcd check by jin.zhao AR-202507-001417 20250805 end
	return sprintf(buf, "lcm_pcd_check_by_read_lcm:%d\n", g_lcm_pcd_check_by_read_lcm);
}
#endif

static ssize_t tran_lcm_low_backlight_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned int lcm_low_backlight = 0;
	if (g_tran_dsi[DDP_DSI0] && g_tran_dsi[DDP_DSI0]->ext && g_tran_dsi[DDP_DSI0]->ext->params &&
		g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params)
	{
		lcm_low_backlight = g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params->lcm_low_backlight;
	}
	return sprintf(buf, "lcm_low_backlight:%d", lcm_low_backlight);
}

static ssize_t tran_lcm_low_backlight_store(struct kobject *kobj,
											struct kobj_attribute *attr, const char *buf, size_t size)
{
	unsigned int value = 0;
	int status = 0;

	if (sscanf(buf, "%d", &value) < 0) {
		status = -EFAULT;
	}
	pr_err("%s value= %d \n", __func__, value);

	if (g_tran_dsi[DDP_DSI0] && g_tran_dsi[DDP_DSI0]->ext && g_tran_dsi[DDP_DSI0]->ext->params &&
		g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params)
	{
		g_tran_dsi[DDP_DSI0]->ext->params->tran_panel_params->lcm_low_backlight = value;
	} else {
		pr_err("%s panel lcm_low_backlight not init ! %d \n", __func__, value);
	}

	return size;
}

#define tran_attr(_name, _mode, _show, _store)    \
	static struct kobj_attribute _name##_attr = { \
		.attr = {                                 \
			.name = __stringify(_name),           \
			.mode = _mode,                        \
		},                                        \
		.show = _show,                            \
		.store = _store,                          \
	}
tran_attr(lcm_name, 0444, lcm_name_show, NULL);
tran_attr(lcm_sub_name, 0444, lcm_sub_name_show, NULL);
tran_attr(lcm_hbm_state, 0664, tran_lcm_hbm_show, tran_lcm_hbm_store);
tran_attr(lcm_dimming_state, 0664, tran_lcm_lcm_dimming_state_show, tran_lcm_lcm_dimming_state_store);
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
tran_attr(lcm_doze_backlight, 0444, lcm_doze_backlight_show, NULL);
#endif
#if IS_ENABLED(CONFIG_TRAN_LCM_PCD_CHECK)
tran_attr(lcm_pcd_check_by_read_lcm, 0444, lcm_pcd_check_by_read_lcm_show, NULL);
#endif
tran_attr(lcm_low_backlight, 0444, tran_lcm_low_backlight_show, tran_lcm_low_backlight_store);
tran_attr(lcm_node, 0664, tran_lcm_node_show, tran_lcm_node_store);

static struct attribute *lcm[] = {
	&lcm_name_attr.attr,
	&lcm_hbm_state_attr.attr,
	&lcm_dimming_state_attr.attr,
#if IS_ENABLED(CONFIG_TRANSSION_DOZE_BRIGHTNESS_SUPPORT)
	&lcm_doze_backlight_attr.attr,
#endif
	&lcm_low_backlight_attr.attr,
	&lcm_node_attr.attr,
	NULL,
};

static struct attribute_group lcm_name_attr_group = {
	.attrs = lcm,
};
static struct attribute *lcm_sub[] = {
	&lcm_sub_name_attr.attr,
	NULL,
};

static struct attribute * lcm_pcd_check[] = {
	&lcm_pcd_check_by_read_lcm_attr.attr,
	NULL,
};

static struct attribute_group lcm_pcd_check_by_read_lcm_attr_group = {
	.attrs = lcm_pcd_check,
};

static struct attribute_group lcm_sub_name_attr_group = {
	.attrs = lcm_sub,
};

static struct kobject *mtkfb_sys_kobj = NULL;
int mtkfb_register_kernelfs(struct device *dev)
{
	int status = 0;
	if (mtkfb_sys_kobj == NULL)
	{
		mtkfb_sys_kobj = kobject_create_and_add("tran_display", kernel_kobj);
		if (!mtkfb_sys_kobj)
		{
			DDPPR_ERR("create mtkfb sysfs fail\n");
			return -1;
		}
	}

	status = sysfs_create_group(mtkfb_sys_kobj, &lcm_name_attr_group);
	if (status)
	{
		DDPPR_ERR("create mtkfb sysfs files fail\n");
		return -1;
	}

	if (g_tran_dsi[DDP_DSI0]->ext->params->lcm_pcd_check_table[0].cmd != 0) {
		status = sysfs_create_group(mtkfb_sys_kobj, &lcm_pcd_check_by_read_lcm_attr_group);
		if (status) {
			DDPINFO("create lcm_pcd_check_by_read_lcm node fail\n");
			return -1;
		}
	}

	return 0;
}
EXPORT_SYMBOL(mtkfb_register_kernelfs);

int mtkfb_register_sub_kernelfs(struct device *dev)
{
	int status = 0;
	if (mtkfb_sys_kobj == NULL)
	{
		mtkfb_sys_kobj = kobject_create_and_add("tran_display", kernel_kobj);
		if (!mtkfb_sys_kobj)
		{
			DDPPR_ERR("create mtkfb sysfs fail\n");
			return -1;
		}
	}

	status = sysfs_create_group(mtkfb_sys_kobj, &lcm_sub_name_attr_group);
	if (status)
	{
		DDPPR_ERR("create mtkfb sub sysfs files fail\n");
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(mtkfb_register_sub_kernelfs);
