#include <linux/extcon-provider.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/usb/role.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include "tran_extcon_usb.h"

#include "tc_charger.h"
#include "tc_common_class.h"
#include "tc_misc_intf.h"

#include <linux/iio/consumer.h>
#include <linux/delay.h>
#include "extcon.h"

static const unsigned int usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

static int do_tran_otg_uevent(struct extcon_dev *edev,int state)
{
	char name_buf[120] = {0};
	char state_buf[120] = {0};
	char *prop_buf = NULL;
	char *envp[3] = {NULL};
	int env_offset = 0;
	int length = 0;

	prop_buf = (char *)get_zeroed_page(GFP_ATOMIC);
	if (!prop_buf) {
		dev_err(&edev->dev, "out of memory in extcon_set_state\n");
		return -ENOMEM;
	}
	length = sprintf(prop_buf, "%s\n", edev->name);
	if (length > 0) {
		if (prop_buf[length - 1] == '\n')
			prop_buf[length - 1] = 0;
		snprintf(name_buf, sizeof(name_buf), "SWITCH_NAME=%s", prop_buf);
		envp[env_offset++] = name_buf;
	}

	length = sprintf(prop_buf, "%d\n",state);
	if (length > 0) {
		if (prop_buf[length - 1] == '\n')
			prop_buf[length - 1] = 0;
		snprintf(state_buf, sizeof(state_buf), "SWITCH_STATE=%s", prop_buf);
		envp[env_offset++] = state_buf;
	}
	envp[env_offset] = NULL;
	dev_dbg(&edev->dev,"name_buf:%s \n",name_buf);
	dev_dbg(&edev->dev,"state_buf:%s \n",state_buf);
	kobject_uevent_env(&edev->dev.kobj, KOBJ_CHANGE, envp);
	free_page((unsigned long)prop_buf);

	return 0;
}
static void do_otg_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct tran_extcon_info *extcon =
		container_of(dwork, struct tran_extcon_info, otg_dely_work);
	if(extcon)
	{
		do_tran_otg_uevent(extcon->edev,extcon->otg_event);
	}
}

static int extcon_otg_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct tran_extcon_info *extcon =
		container_of(nb, struct tran_extcon_info, otg_nb);
	extcon->otg_event = event;
	schedule_delayed_work(&extcon->otg_dely_work,0);

	return NOTIFY_DONE;
}

static void tran_usb_extcon_update_role(struct work_struct *work)
{
	struct usb_role_info *role = container_of(to_delayed_work(work),
					struct usb_role_info, dwork);
	struct tran_extcon_info *extcon = role->extcon;
	unsigned int cur_dr, new_dr;

	cur_dr = extcon->c_role;
	new_dr = role->d_role;

	dev_info(extcon->dev, "cur_dr(%d) new_dr(%d)\n", cur_dr, new_dr);

	/* none -> device */
	if (cur_dr == USB_ROLE_NONE &&
			new_dr == USB_ROLE_DEVICE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, true);
	/* none -> host */
	} else if (cur_dr == USB_ROLE_NONE &&
			new_dr == USB_ROLE_HOST) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, true);
	/* device -> none */
	} else if (cur_dr == USB_ROLE_DEVICE &&
			new_dr == USB_ROLE_NONE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, false);
	/* host -> none */
	} else if (cur_dr == USB_ROLE_HOST &&
			new_dr == USB_ROLE_NONE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, false);
	/* device -> host */
	} else if (cur_dr == USB_ROLE_DEVICE &&
			new_dr == USB_ROLE_HOST) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, false);
		extcon_set_state_sync(extcon->edev,	EXTCON_USB_HOST, true);
	/* host -> device */
	} else if (cur_dr == USB_ROLE_HOST &&
			new_dr == USB_ROLE_DEVICE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, false);
		extcon_set_state_sync(extcon->edev,	EXTCON_USB, true);
	}

	/* usb role switch */
	if (extcon->role_sw)
		usb_role_switch_set_role(extcon->role_sw, new_dr);

	extcon->c_role = new_dr;
	kfree(role);
}

static int tran_usb_extcon_set_role(struct tran_extcon_info *extcon,
						unsigned int role)
{
	struct usb_role_info *role_info;

	/* create and prepare worker */
	role_info = kzalloc(sizeof(*role_info), GFP_ATOMIC);
	if (!role_info)
		return -ENOMEM;

	INIT_DELAYED_WORK(&role_info->dwork, tran_usb_extcon_update_role);

	role_info->extcon = extcon;
	role_info->d_role = role;
	/* issue connection work */
	queue_delayed_work(extcon->extcon_wq, &role_info->dwork, 0);

	return 0;
}

static bool usb_is_online(struct tran_extcon_info *extcon)
{
	union power_supply_propval pval;
	union power_supply_propval tval;
	int ret;

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get online prop\n");
		return false;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_USB_TYPE, &tval);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get usb type\n");
		return false;
	}

	dev_info(extcon->dev, "online=%d, type=%d\n", pval.intval, tval.intval);

	if (pval.intval && (tval.intval == POWER_SUPPLY_USB_TYPE_SDP ||
			tval.intval == POWER_SUPPLY_USB_TYPE_CDP))
		return true;
	else
		return false;
}

static void tran_usb_extcon_psy_detector(struct work_struct *work)
{
	struct tran_extcon_info *extcon = container_of(to_delayed_work(work),
		struct tran_extcon_info, wq_psy);

	/* Workaround for PR_SWAP, IF tcpc_dev, then do not switch role. */
	/* Since we will set USB to none when type-c plug out */
	if (usb_is_online(extcon) && extcon->c_role == USB_ROLE_NONE) {
		dev_info(extcon->dev, "Type-C SINK plug in of psy\n");
		tran_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
	}
}

static int tran_usb_extcon_psy_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct tran_extcon_info *extcon = container_of(nb,
					struct tran_extcon_info, psy_nb);

	if (event != PSY_EVENT_PROP_CHANGED || psy != extcon->usb_psy)
		return NOTIFY_DONE;

	queue_delayed_work(system_power_efficient_wq, &extcon->wq_psy, 0);

	return NOTIFY_DONE;
}

static int tran_usb_extcon_psy_init(struct tran_extcon_info *extcon)
{
	int ret = 0;
	struct device *dev = extcon->dev;

	if (!of_property_read_bool(dev->of_node, "charger")) {
		ret = -EINVAL;
		goto fail;
	}

	extcon->usb_psy = devm_power_supply_get_by_phandle(dev, "charger");
	if (IS_ERR_OR_NULL(extcon->usb_psy)) {
		/* try to get by name */
		extcon->usb_psy = power_supply_get_by_name("primary_chg");
		if (IS_ERR_OR_NULL(extcon->usb_psy)) {
			dev_err(dev, "fail to get usb_psy\n");
			ret = -EINVAL;
			goto fail;
		}
	}

	INIT_DELAYED_WORK(&extcon->wq_psy, tran_usb_extcon_psy_detector);

	extcon->psy_nb.notifier_call = tran_usb_extcon_psy_notifier;
	ret = power_supply_reg_notifier(&extcon->psy_nb);
	if (ret)
		dev_err(dev, "fail to register notifer\n");
fail:
	return ret;
}


static int tran_extcon_pd_tcpc_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct tc_tcpc_noti *noti = data;
	struct tran_extcon_info *extcon = (struct tran_extcon_info *)container_of(nb,
		struct tran_extcon_info, tc_tcpc_nb);
	struct device *dev = extcon->dev;
	int count = 0;

	dev_err(dev, "%s event:%lu\n", __func__, event);

	switch (event) {
	case TC_TYPEC_OTG_PLUG_IN:
		dev_info(dev, "Type-C SRC plug in\n");
		tran_usb_extcon_set_role(extcon, USB_ROLE_HOST);
		break;
	case TC_TYPEC_USB_PLUG_IN:
		if(!extcon->bypss_typec_sink){
			dev_info(dev, "Type-C SINK plug in\n");
			tran_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
		}
		break;
	case TC_PD_SRC_TO_SNK:
		dev_info(dev, "Type-C pw_swap Source to Sink!\n");
		break;
	case TC_PD_SNK_TO_SRC:
		dev_info(dev, "Type-C pw_swap Sink to Source!\n");
		break;
	case TC_TYPEC_OTG_PLUG_OUT:
	case TC_TYPEC_USB_PLUG_OUT:
		dev_info(dev, "Type-C plug out\n");
		tran_usb_extcon_set_role(extcon, USB_ROLE_NONE);
		break;
	case TC_PD_DR_SWAP_NEW_ROLE:
DelayforSwap:
		if (noti->pd_dr_swap_role == TC_PD_DR_SWAP_DEVICE &&
				extcon->c_role != USB_ROLE_DEVICE){
			dev_info(dev, "switch role to device\n");
			tran_usb_extcon_set_role(extcon, USB_ROLE_NONE);
			tran_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
		}else if(noti->pd_dr_swap_role == TC_PD_DR_SWAP_HOST &&
				extcon->c_role != USB_ROLE_HOST){
			dev_info(dev, "switch role to host\n");
			tran_usb_extcon_set_role(extcon, USB_ROLE_NONE);
			tran_usb_extcon_set_role(extcon, USB_ROLE_HOST);
		} else {
			count++;
			if(count>3){
				count = 0;
				break;
			}
			dev_info(dev, "Delay for swap...\n");
			msleep(500);
			goto DelayforSwap;
		}
		break;
	};
	return NOTIFY_OK;
}

static int tran_usb_extcon_tc_tcpc_init(struct tran_extcon_info *extcon)
{
	int ret = 0;

	extcon->tc_tcpc_nb.notifier_call = tran_extcon_pd_tcpc_notifier;
	ret = register_tc_tcpc_notifier(&extcon->tc_tcpc_nb);
	if (ret < 0){
		dev_info(extcon->dev, "extcon register tc_tcpc failed!\n");
		return -1;
	}
	return 0;
}

#define PROC_FILE_SMT "mtk_typec"
#define FILE_SMT_U2_CC_MODE "smt_u2_cc_mode"

static int usb_cc_smt_procfs_show(struct seq_file *s, void *unused)
{
	struct tran_device *tc_tcpc_dev = tran_get_by_name("tc_tcpc");
	union com_propval tran_val = {0, };
	uint8_t val = 0;

	if (IS_ERR_OR_NULL(tc_tcpc_dev)) {
		seq_puts(s, "1\n");
		pr_err("tc_tcpc_dev is NULL");
		return -ENODEV;
	}

	tran_dev_get_prop(tc_tcpc_dev, TRAN_PROP_TYPE_EXTCON_CC_SMT, &tran_val);
	val = tran_val.intval;
	seq_printf(s, "%d\n", val);

	return 0;
}

static int usb_cc_smt_procfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, usb_cc_smt_procfs_show, pde_data(inode));
}

static const struct  proc_ops usb_cc_smt_procfs_fops = {
	.proc_open = usb_cc_smt_procfs_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int tran_usb_extcon_procfs_init(struct tran_extcon_info *extcon)
{
	struct proc_dir_entry *file, *root;
	int ret = 0;

	root = proc_mkdir(PROC_FILE_SMT, NULL);
	if (!root) {
		dev_info(extcon->dev, "fail creating proc dir: %s\n",
			PROC_FILE_SMT);
		ret = -ENOMEM;
		goto error;
	}

	file = proc_create_data(FILE_SMT_U2_CC_MODE, 0400, root,
		&usb_cc_smt_procfs_fops, extcon);
	if (!file) {
		dev_info(extcon->dev, "fail creating proc file: %s\n",
			FILE_SMT_U2_CC_MODE);
		ret = -ENOMEM;
		goto error;
	}

	dev_info(extcon->dev, "success creating proc file: %s\n",
		FILE_SMT_U2_CC_MODE);

error:
	dev_info(extcon->dev, "%s ret:%d\n", __func__, ret);
	return ret;
}

static int tran_usb_extcon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tran_extcon_info *extcon;
	const char *tcpc_name;

	int ret;
	pr_info("%s:start!\n",__func__);

	extcon = devm_kzalloc(&pdev->dev, sizeof(*extcon), GFP_KERNEL);
	if (!extcon)
		return -ENOMEM;

	extcon->dev = dev;

	/* extcon */
	extcon->edev = devm_extcon_dev_allocate(dev, usb_extcon_cable);
	if (IS_ERR(extcon->edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(dev, extcon->edev);
	if (ret < 0) {
		dev_info(dev, "failed to register extcon device\n");
		return ret;
	}

	extcon->otg_nb.notifier_call = extcon_otg_notifier;
	devm_extcon_register_notifier(extcon->dev,extcon->edev,EXTCON_USB_HOST,&extcon->otg_nb);
	INIT_DELAYED_WORK(&extcon->otg_dely_work, do_otg_work);

	/* usb role switch */
	extcon->role_sw = usb_role_switch_get(extcon->dev);
	if (IS_ERR(extcon->role_sw)) {
		dev_err(dev, "failed to get usb role\n");
		return PTR_ERR(extcon->role_sw);
	}

	/* initial usb role */
	if (extcon->role_sw)
		extcon->c_role = USB_ROLE_NONE;

	extcon->bypss_typec_sink =
		of_property_read_bool(dev->of_node,
			"mediatek,bypss-typec-sink");

	ret = of_property_read_string(dev->of_node, "tcpc", &tcpc_name);
	if (of_property_read_bool(dev->of_node, "mediatek,u2") && ret == 0
		&& strcmp(tcpc_name, "type_c_port0") == 0) {
		tran_usb_extcon_procfs_init(extcon);
	}

	extcon->extcon_wq = create_singlethread_workqueue("extcon_usb");
	if (!extcon->extcon_wq)
		return -ENOMEM;

	/* power psy */
	ret = tran_usb_extcon_psy_init(extcon);
	if (ret < 0)
		dev_err(dev, "failed to init psy\n");


	ret = tran_usb_extcon_tc_tcpc_init(extcon);
	if (ret < 0)
		dev_err(dev, "failed to init tc_tcpc\n");

	platform_set_drvdata(pdev, extcon);

	return 0;
}

static void tran_usb_extcon_remove(struct platform_device *pdev)
{

}

static void tran_usb_extcon_shutdown(struct platform_device *pdev)
{
	struct tran_extcon_info *extcon = platform_get_drvdata(pdev);

	dev_info(extcon->dev, "shutdown\n");
}

static const struct of_device_id tran_usb_extcon_of_match[] = {
	{ .compatible = "transsion,extcon-usb", },
	{ },
};
MODULE_DEVICE_TABLE(of, tran_usb_extcon_of_match);

static struct platform_driver tran_usb_extcon_driver = {
	.probe		= tran_usb_extcon_probe,
	.remove		= tran_usb_extcon_remove,
	.shutdown	= tran_usb_extcon_shutdown,
	.driver		= {
		.name	= "tran-extcon-usb",
		.of_match_table = tran_usb_extcon_of_match,
	},
};

static int __init tran_usb_extcon_init(void)
{
	return platform_driver_register(&tran_usb_extcon_driver);
}
late_initcall(tran_usb_extcon_init);

static void __exit tran_usb_extcon_exit(void)
{
	platform_driver_unregister(&tran_usb_extcon_driver);
}
module_exit(tran_usb_extcon_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Transsion Extcon USB Driver");
