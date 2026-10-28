static u8 icnl9916_driver_builtin_firmware[] = {
};

static u8 icnl9916c_driver_builtin_firmware[] = {
};

static struct cts_firmware cts_driver_builtin_firmwares[] = {
    {
        .name = "OEM-Project",    /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9916,
        .fwid = CTS_DEV_FWID_ICNL9916,
        .bins = {
            {
                .flash_fw_info_addr = 0x30000,
                .flash_fw_addr = CTS_FLASH_FW_START_ADDR_DEFAULT,
                .bin = icnl9916_driver_builtin_firmware,
            }
        },
    },
    {
        .name = "OEM-Project",    /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9916C,
        .fwid = CTS_DEV_FWID_ICNL9916C,
        .bins = {
            {
                .flash_fw_info_addr = 0x30000,
                .flash_fw_addr = CTS_FLASH_FW_START_ADDR_DEFAULT,
                .bin = icnl9916c_driver_builtin_firmware,
            }
        },
    },

};
