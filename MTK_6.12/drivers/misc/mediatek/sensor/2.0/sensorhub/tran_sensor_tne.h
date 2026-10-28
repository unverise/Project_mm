#if IS_ENABLED(CONFIG_TRAN_FREEZE_DETECT_SUPPORT)
#include <tk/tnek.h>

#define FLAG_TNE_USER_LOG_MAIN_SYS (1 << 1)
#define FLAG_TNE_LOG (1 << 9)
#define FLAG_TNE_USER_PROPERTY (1 << 18)

extern int sensorhub_cloud_engine_init(void);
extern void sensorhub_cloud_engine_exit(void);
#endif