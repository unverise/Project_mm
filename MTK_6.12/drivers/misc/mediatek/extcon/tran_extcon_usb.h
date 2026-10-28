#ifndef __LINUX_TRAN_EXTCON_USB_H__
#define __LINUX_TRAN_EXTCON_USB_H__

struct tran_extcon_info {
	struct device *dev;
	struct extcon_dev *edev;
	struct usb_role_switch *role_sw;
	unsigned int c_role; /* current data role */
	struct workqueue_struct *extcon_wq;
	bool vbus_on;
	struct power_supply *usb_psy;
	struct notifier_block psy_nb;
	struct delayed_work wq_psy;
	struct notifier_block tc_tcpc_nb;
	bool bypss_typec_sink;
	struct delayed_work wq_detcable;
	struct notifier_block otg_nb;
	struct delayed_work otg_dely_work;
	unsigned int otg_event;
};

struct usb_role_info {
	struct tran_extcon_info *extcon;
	struct delayed_work dwork;
	unsigned int d_role; /* desire data role */
};

enum {
	DUAL_PROP_MODE_UFP = 0,
	DUAL_PROP_MODE_DFP,
	DUAL_PROP_MODE_NONE,
};

enum {
	DUAL_PROP_PR_SRC = 0,
	DUAL_PROP_PR_SNK,
	DUAL_PROP_PR_NONE,
};

#endif//__LINUX_TRAN_EXTCON_USB_H__