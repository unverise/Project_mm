#ifndef _CTS_TCS_H_
#define _CTS_TCS_H_


#define TCS_WR_ADDR                         (0xF0)
#define TCS_RD_ADDR                         (0xF1)

#define TCS_READ_BIT                        (14)
#define TCS_WRITE_BIT                       (13)
/* raw touch info without data */
#define TOUCH_INFO_SIZE                      (112)
#define TOUCH_DATA_SIZE                      (112)
#define INT_DATA_INFO_SIZE                   (64)
#define INT_DATA_TYPE_LEN_SIZE               (4)
#define INT_DATA_VALID_INDEX                (TOUCH_INFO_SIZE + 62)

#define TCS_I2C_BATCH_MSG_MAX   1

#define CTS_INT_DATA_STYLUS     BIT(14)

#pragma pack(1)
typedef struct {
    __le16 cmd;
    __le16 datlen;
    u8 check_l;
    u8 check_h;
} tcs_i2c_tx_head;

typedef struct {
    u8 addr;
    __le16 cmd;
    __le16 datlen;
    __le16 crc16;
} tcs_spi_tx_head;
typedef struct {
    u8 ecode;
    __le16 cmd;
    __le16 crc16;
} tcs_rx_tail;

struct cts_tcs_cmd {
    uint16_t cmd_id : 8;
    uint16_t class_id : 5;
    uint16_t is_write : 1;
    uint16_t is_read : 1;
    uint16_t base_flag : 1;
};
#pragma pack()


enum tcs_read_data_type {
        TCS_READ_TYPE_RESERVED = 0,
        TCS_READ_DATA_TYPE_REG_RAM = 1,
        TCS_READ_DATA_TYPE_ABS_IN_GUESTURE = 9,
        TCS_READ_DATA_TYPE_MAX,
};

int cts_tcs_read(const struct cts_device *cts_dev,
        u16 cmd, u8 *buf, size_t len);
int cts_tcs_write(const struct cts_device *cts_dev,
        u16 cmd, u8 *buf, size_t len);
int cts_tcs_read_data(const struct cts_device *cts_dev, u32 addr,
        u8 *data, u32 size);
int cts_tcs_read_ddi_reg(const struct cts_device *cts_dev, u32 addr,
        u8 *regbuf, u32 size);
int cts_tcs_write_tp_data(const struct cts_device *cts_dev, u32 addr,
        const u8 *data, u32 size);
int cts_tcs_write_reg(const struct cts_device *cts_dev, u32 addr,
        u8 *buf, u32 size);
int cts_tcs_write_ddi_reg(const struct cts_device *cts_dev, u32 addr,
        const u8 *data, u32 size);
int cts_tcs_spi_xtrans(const struct cts_device *cts_dev, u8 *tx,
        size_t txlen, u8 *rx, size_t rxlen);
int cts_tcs_get_fw_ver(const struct cts_device *cts_dev, u16 *fwver);
int cts_tcs_get_lib_ver(const struct cts_device *cts_dev, u16 *libver);
int cts_tcs_get_ddi_ver(const struct cts_device *cts_dev, u8 *ddiver);
int cts_tcs_get_touch_xy_info(const struct cts_device *cts_dev,
    struct cts_touch_xy_info *xy_info);
int cts_tcs_get_ic_channel_info(const struct cts_device *cts_dev,
    struct cts_ic_channel_info *ic_channel_info);
int cts_tcs_get_panel_info(const struct cts_device *cts_dev,
    struct cts_panel_info *panel_info);
int cts_tcs_get_flip_x(const struct cts_device *cts_dev, bool *flip_x);
int cts_tcs_get_flip_y(const struct cts_device *cts_dev, bool *flip_y);
int cts_tcs_get_swap_axes(const struct cts_device *cts_dev, bool *swap_axes);
int cts_tcs_clr_gstr_ready_flag(const struct cts_device *cts_dev);

int cts_tcs_get_int_mode(const struct cts_device *cts_dev, u8 *int_mode);
int cts_tcs_get_int_keep_time(const struct cts_device *cts_dev,
        u16 *int_keep_time);
int cts_tcs_get_rawdata_target(const struct cts_device *cts_dev,
        u16 *rawdata_target);
int cts_tcs_get_esd_method(const struct cts_device *cts_dev, u8 *esd_method);

int cts_tcs_get_touchinfo(struct cts_device *cts_dev, void *touch_info);
int cts_tcs_get_gestureinfo(struct cts_device *cts_dev,
        struct cts_touch_info_legacy *gesture_info);
int cts_tcs_get_touch_status(struct cts_device *cts_dev);
int cts_tcs_get_esd_protection(const struct cts_device *cts_dev,
        u8 *esd_protection);

int cts_tcs_set_krang_stop(struct cts_device *cts_dev);
bool cts_tcs_is_krang_stopped(struct cts_device *cts_dev);
int cts_tcs_get_fw_id(const struct cts_device *cts_dev, u16 *fwid);
int cts_tcs_get_workmode(const struct cts_device *cts_dev, u8 *workmode);
int cts_tcs_set_workmode(const struct cts_device *cts_dev, u8 workmode);
int cts_tcs_set_openshort_mode(const struct cts_device *cts_dev, u8 mode);
int cts_tcs_get_curr_mode(const struct cts_device *cts_dev, u8 *currmode);
int cts_tcs_set_tx_vol(const struct cts_device *cts_dev, u8 txvol);

int cts_tcs_set_short_test_type(const struct cts_device *cts_dev, u8 short_type);
int cts_tcs_set_openshort_enable(const struct cts_device *cts_dev, u8 enable);
int cts_tcs_is_openshort_enabled(const struct cts_device *cts_dev, u8 *enabled);

int cts_tcs_set_esd_enable(const struct cts_device *cts_dev, u8 enable);
int cts_tcs_set_cneg_enable(const struct cts_device *cts_dev, u8 enable);
int cts_tcs_set_mnt_enable(const struct cts_device *cts_dev, u8 enable);
int cts_tcs_is_display_on(const struct cts_device *cts_dev, u8 *display_on);
int cts_tcs_set_display_on(const struct cts_device *cts_dev, u8 display_on);
int cts_tcs_is_cneg_enabled(const struct cts_device *cts_dev, u8 *enabled);
int cts_tcs_is_mnt_enabled(const struct cts_device *cts_dev, u8 *enabled);
int cts_tcs_set_pwr_mode(const struct cts_device *cts_dev, u8 pwr_mode);
int cts_tcs_get_has_int_data(const struct cts_device *cts_dev, bool *has);
int cts_tcs_get_int_data_types(const struct cts_device *cts_dev, u16 *type);
int cts_tcs_set_int_data_types(const struct cts_device *cts_dev, u16 type);
int cts_tcs_get_int_data_method(const struct cts_device *cts_dev, u8 *method);
int cts_tcs_set_int_data_method(const struct cts_device *cts_dev, u8 method);
void cts_tcs_calc_int_data_size(struct cts_device *cts_dev);

void cts_tcs_drop_frames(struct cts_device *cts_dev, int frame_count);
int cts_tcs_polling_data(struct cts_device *cts_dev,u8 *buf, size_t size);
int cts_tcs_polling_test_data(struct cts_device *cts_dev,
        u8 *buf, size_t size);
int cts_tcs_top_get_rawdata(struct cts_device *cts_dev, u8 *buf, size_t size);
int cts_tcs_top_get_manual_diff(struct cts_device *cts_dev, u8 *buf,
        size_t size);
int cts_tcs_top_get_real_diff(struct cts_device *cts_dev, u8 *buf, size_t size);
int cts_tcs_top_get_noise_diff(struct cts_device *cts_dev, u8 *buf,size_t size);
int cts_tcs_top_get_basedata(struct cts_device *cts_dev, u8 *buf, size_t size);
int cts_tcs_top_get_cnegdata(struct cts_device *cts_dev, u8 *buf, size_t size);
int cts_tcs_reset_device(const struct cts_device *cts_dev);
int cts_tcs_set_int_test(const struct cts_device *cts_dev, u8 enable);
int cts_tcs_set_int_pin(const struct cts_device *cts_dev, u8 high);
int cts_tcs_get_module_id(const struct cts_device *cts_dev, u32 *modId);

int cts_tcs_set_charger_plug(struct cts_device *cts_dev, u8 set);
int cts_tcs_get_charger_plug(const struct cts_device *cts_dev, u8 *isset);
int cts_tcs_set_earjack_plug(struct cts_device *cts_dev, u8 set);
int cts_tcs_get_earjack_plug(const struct cts_device *cts_dev, u8 *isset);
int cts_tcs_set_panel_direction(const struct cts_device *cts_dev, u8 direction);
int cts_tcs_get_panel_direction(const struct cts_device *cts_dev, u8 *direction);
//USD: add for esports feature by jiangzhu.yang 20240826 start
int cts_tcs_game_mode_select(const struct cts_device *cts_dev, uint8_t cmd);
//USD: add for esports feature by jiangzhu.yang 20240826 end
int cts_tcs_set_game_mode(struct cts_device *cts_dev, u8 enable);
int cts_tcs_get_game_mode(const struct cts_device *cts_dev, u8 *enabled);
int cts_tcs_set_proximity_mode(struct cts_device *cts_dev, u8 enable);
void cts_tcs_reinit_fw_status(struct cts_device *cts_dev);

int cts_tcs_set_product_en(struct cts_device *cts_dev, u8 enable);
//USD:modify for lcd aod by yun.luo 20240527 start
int cts_tcs_set_aod_pwr_mode(const struct cts_device *cts_dev, u8 pwr_mode);
//USD:modify for lcd aod by yun.luo 20240527 end

//BSP:add for pocket mode by xihao.ding 20251129 start
int cts_tcs_set_pocket_mode(const struct cts_device *cts_dev, u8 mode);
//BSP:add for pocket mode by xihao.ding 20251129 end
#endif /* _CTS_TCS_H_ */
