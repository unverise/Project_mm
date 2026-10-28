#ifndef _PS_CORE_H_
#define _PS_CORE_H_

#if IS_ENABLED(CONFIG_TP_VIRTUAL_PROXIMITY)
extern  int tp_ps_init(void);
extern void __exit ps_exit(void);
#endif

#endif