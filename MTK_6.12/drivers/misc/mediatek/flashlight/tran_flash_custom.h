/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Transsion Inc.
 *
 */

//ITD: Modify Flashlight Drv By Jesse 220405 Start
//project para struct: static, config by developer
struct project_current_config_struct {
    int sysui_torch[2];//sysui torch, [0]low1 level current, [1]low2 level current
    int fac_flash;//factory mode current
    int faceid_torch;//face id current
    int torch_360;//torch_360 current
    int phonecall;//phonecall reminder current
    int torch_duty_range[2];//slide control torch current, [0]min, [1]max
};

struct project_current_config_struct g_project_current_config_default[3] = {
    {
        {124,210},
        600,
        22,
        180,
        22,
        {110,227},
    },
    {
        {182,182},
        235,
        182,
        182,
        182,
        {120,220}
    },
    {
        {15,30},
        600,
        22,
        180,
        22,
        {110,227},
    },
};

struct project_current_config_struct g_project_current_config_single[3] = {
    {
        {115,120},
        600,
        22,
        120,
        22,
        {110,227},
    },
    {
        {115,120},
        235,
        182,
        120,
        182,
        {120,220}
    },
    {
        {15,30},
        600,
        22,
        180,
        22,
        {110,227},
    },

};

struct project_current_config_struct g_project_current_config[3] = {0};

int flash_config_init(struct device *dev)
{
    struct device_node *np = dev->of_node;
    u32 project_id = 0;
    
    if (!np) {
        pr_info("No DTS node, use g_project_current_config_default config\n");
        memcpy(g_project_current_config, g_project_current_config_default, sizeof(g_project_current_config_default));
        return 0;
    }

    if (of_property_read_u32(np, "current-config-id", &project_id)) {
        project_id = 0;
        pr_info("No current-config-id property, use g_project_current_config_default \n");
    }
    
    pr_info("Flash config current-config-id: %d\n", project_id);
    
    switch (project_id) {
    case 1:
        memcpy(g_project_current_config, g_project_current_config_single, sizeof(g_project_current_config_single));
        pr_info("Use g_project_current_config_single config\n");
        break;
        
    default:
        memcpy(g_project_current_config, g_project_current_config_default, sizeof(g_project_current_config_default));
        dev_warn(dev, "Invalid current-config-id %d, use project default config\n", project_id);
        break;
    }

    return 0;
}

//ITD: Modify Flashlight Drv By Jesse 220405 End
