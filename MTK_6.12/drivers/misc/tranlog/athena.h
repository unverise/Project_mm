#ifndef __TRANLOG_ATHENA__H__
#define __TRANLOG_ATHENA__H__

#include <asm/ioctl.h>

#define ATHENA_IOC_MAGIC    'F'

struct ioc_tid {
    long long tid;
    int state;
};

#define ATHENA_SET_TID  _IOW(ATHENA_IOC_MAGIC, 0x01, struct ioc_tid)
#define ATHENA_SET_TIDS _IOW(ATHENA_IOC_MAGIC, 0x02, int)
#define ATHENA_CLR_TID  _IO(ATHENA_IOC_MAGIC, 0x03)
#define ATHENA_GET_TID  _IOWR(ATHENA_IOC_MAGIC, 0x04, struct ioc_tid)
#define ATHENA_NFY_TID  _IO(ATHENA_IOC_MAGIC, 0x05)
#define ATHENA_SET_EXP  _IOW(ATHENA_IOC_MAGIC, 0x06, int)

extern void athena_ctl_init(void);
extern void athena_ctl_exit(void);

#endif
