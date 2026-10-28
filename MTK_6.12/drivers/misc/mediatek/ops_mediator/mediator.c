#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#define TRAN_INF_DEBUG(fmt, args...) do { \
    printk("[OPS_MEDIATOR]%s %d:"fmt"\n", __func__, __LINE__, ##args); \
} while (0)

struct tran_lcm_ops {
    int (*dsp_register_client)(struct notifier_block *nb);
    int (*dsp_unregister_client)(struct notifier_block *nb);
    int (*dsp_notifier_call_chain)(unsigned long val, int v);
	void (*dsp_disable_vdd)(void);
};

struct tran_tp_ops {
    u32 (*tp_cmd_handle)(u32 cmd, void *pdata);
};

enum tp_func_type{
    CTP_CMD_HANDLE = 0,
	LCM_VDD_DISABLE_CMD_HANDLE,
    CTP_TPYE_MAX
};

struct tran_module_interface{
	struct tran_lcm_ops lcm_ops;
	struct tran_tp_ops tp_ops;
};

static struct tran_module_interface tran_module_intf;


/* dispaly */
int tran_dsp_register_client (struct notifier_block *nb);
int tran_dsp_unregister_client(struct notifier_block *nb);
int tran_dsp_notifier_call_chain(unsigned long val, int v);

/* touchscreen */
//USD add for fts lcm esd by xiaobo.fan start
int tran_tp_need_lcd_esdrecovery = 0;
EXPORT_SYMBOL(tran_tp_need_lcd_esdrecovery);
//USD add for fts lcm esd by xiaobo.fan end

/* lcm module common func interface*/
int tran_dsp_register_client (struct notifier_block *nb)
{
/*
	if (tran_module_intf->lcm_ops == NULL \
        || tran_module_intf->lcm_ops->dsp_register_client == NULL)
		return -1;
	return tran_module_intf->lcm_ops->dsp_register_client(nb);
*/
	return 0;
}
EXPORT_SYMBOL(tran_dsp_register_client);
int tran_dsp_unregister_client (struct notifier_block *nb)
{
/*

	if (tran_module_intf->lcm_ops == NULL \
        || tran_module_intf->lcm_ops->dsp_unregister_client == NULL)
		return -1;
	return tran_module_intf->lcm_ops->dsp_unregister_client(nb);
*/
    return 0;
}
EXPORT_SYMBOL(tran_dsp_unregister_client);
int tran_dsp_notifier_call_chain (unsigned long val, int v)
{
/*

	if (tran_module_intf->lcm_ops == NULL \
        || tran_module_intf->lcm_ops->dsp_notifier_call_chain == NULL)
		return -1;
	return tran_module_intf->lcm_ops->dsp_notifier_call_chain(val, v);
*/
    return 0;
}
EXPORT_SYMBOL(tran_dsp_notifier_call_chain);


/* for tp call lcm func to disable vdd 3.3v */
void tran_call_lcm_disable_vdd (void)
{
	//func exist ?
	if(tran_module_intf.lcm_ops.dsp_disable_vdd != NULL){
		tran_module_intf.lcm_ops.dsp_disable_vdd();
	}
}
EXPORT_SYMBOL(tran_call_lcm_disable_vdd);


/* tp module common func interface*/
u32 tran_call_tp_cmd_handle(u32 cmd, void *pdata)
{
	if (tran_module_intf.tp_ops.tp_cmd_handle == NULL)
		return -1;
	return tran_module_intf.tp_ops.tp_cmd_handle(cmd, pdata);
}
EXPORT_SYMBOL(tran_call_tp_cmd_handle);


/* register function */
void tran_register_tp_func(void* func, int type){
    if(type < CTP_TPYE_MAX){
        switch(type) {
            case CTP_CMD_HANDLE:
                tran_module_intf.tp_ops.tp_cmd_handle = func;
                break;
			case LCM_VDD_DISABLE_CMD_HANDLE:
                tran_module_intf.lcm_ops.dsp_disable_vdd = func;
                break;
            default:
                TRAN_INF_DEBUG("unknown tp function type!!");
        }
    }
}
EXPORT_SYMBOL_GPL(tran_register_tp_func);


/* module init */
static int __init tran_module_init(void)
{
    TRAN_INF_DEBUG("tran interface module init");
    return 0;
}
static void __exit tran_module_exit(void)
{
    TRAN_INF_DEBUG("tran interface module exit");
}
module_init(tran_module_init);
module_exit(tran_module_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Transsion touch panel driver");
MODULE_AUTHOR("<Transsion_BSP@transsion.com>");
