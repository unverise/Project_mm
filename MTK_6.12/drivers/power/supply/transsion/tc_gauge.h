
#ifndef __TC_GAUGE_H
#define __TC_GAUGE_H
#define DEFAULT_BATT_VOLTAGE	3999
#define DEFAULT_BATT_TEMP	25
#define DEFAULT_UISOC	50
#define DEFAULT_Q_MAX 4000

enum {
	GAUGE_NON_BATT_MODE = 0,
	GAUGE_SINGLE_MASTER_BATT_MODE,
	GAUGE_SINGLE_SLAVE_BATT_MODE,
	GAUGE_DUAL_BATT_MODE,
};

enum {
	GAUGE_DUAL_BATT_ONLINE = 0,
	GAUGE_SINGLE_MASTER_BATT_ONLINE,
	GAUGE_SINGLE_SLAVE_BATT_ONLINE,
	GAUGE_NONE_BATT_ONLINE,
};

static const char * const BATTERY_ONLINE_STATUS[] = {
	[GAUGE_DUAL_BATT_ONLINE]            = "dual_battery",
	[GAUGE_SINGLE_MASTER_BATT_ONLINE]   = "single_master",
	[GAUGE_SINGLE_SLAVE_BATT_ONLINE]    = "single_slave",
	[GAUGE_NONE_BATT_ONLINE]            = "none_batt",

};

struct tc_gauge {
	struct device *dev;
	struct platform_device *pdev;
	struct tran_device *gauge_dev;
	struct tran_device *ambient_dev;
	struct tran_properties gauge_props;
	struct tran_device *bat_dev;
	struct power_supply_desc gauge_psy_desc;
	struct power_supply_config gauge_psy_cfg;
	struct power_supply *gauge_psy;
	struct power_supply *batt_psy;
	struct power_supply *chg_psy;
	struct charger_device *chg1_dev;
	struct tran_device *charge_transfer_dev;
	struct mutex tc_gauge_mutex;
	struct class *bat_cali_class;
	struct cdev *bat_cali_cdev;
	struct notifier_block gauge_notifier;
	int batt_status;
	int batt_cycle;
	int bat_cali_major;
	dev_t bat_cali_devno;
	int fake_batt_cycle;
	int batt_cycle_ratio;
	int fixed_bat_tmp;
};

#endif
