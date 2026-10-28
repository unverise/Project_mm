#ifndef __CLOUD_ENGINE__H__
#define __CLOUD_ENGINE__H__

#include <asm/ioctl.h>

#define CLOUD_ENGINE_ID_MAX      32
#define CLOUD_ENGINE_BUF_SIZE    2048

struct kernel_cloud_config {
    int id_len;
    char *id;
    int config_len;
    char *config;
};

#define CLOUD_ENGINE_IOC_MAGIC    'F'
#define CLOUD_ENGINE_UPDATE_CONFIG  _IOW(CLOUD_ENGINE_IOC_MAGIC, 0x01, struct kernel_cloud_config)
#define CLOUD_ENGINE_NOTIDY_REG     _IO(CLOUD_ENGINE_IOC_MAGIC, 0x02)

extern void cloudengine_ctl_init(void);
extern void cloudengine_ctl_exit(void);
#endif

