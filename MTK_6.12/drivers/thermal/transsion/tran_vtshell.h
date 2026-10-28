#ifndef __TRAN_VTSHELL_H__
#define __TRAN_VTSHELL_H__

#define MAX_VTSHELL_REF_NUM 10

enum vtshell_operation {
    OPER_COEF,
    OPER_MAX,
    OPER_NUM,
};

struct vtshell_coef {
    char sensor_name[THERMAL_NAME_LENGTH + 1];
    long long sensor_coef;
};

struct vtshell_tz_param {
    char tz_name[THERMAL_NAME_LENGTH + 1];
    enum vtshell_operation operation;
    unsigned int ref_num;
    struct vtshell_coef vtshell_ref[MAX_VTSHELL_REF_NUM];
    struct thermal_zone_device *tzd[MAX_VTSHELL_REF_NUM];
};

struct vtshell_data {
    struct device *dev;
    struct mutex lock;
    int num_sensor;
    struct vtshell_tz_param *params;
};

struct vtshell_temp_tz {
    unsigned int id;
    struct vtshell_data *shell_data;
    struct vtshell_tz_param *shell_param;
};

extern struct vtshell_data *plat_vtshell_info;

#endif
