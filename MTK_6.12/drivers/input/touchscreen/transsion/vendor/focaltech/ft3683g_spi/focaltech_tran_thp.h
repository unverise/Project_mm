#ifndef __LINUX_FOCALTECH_TRAN_THP_H__
#define __LINUX_FOCALTECH_TRAN_THP_H__

#define THP_DATA_HEAD_SIZE                          (63)
#define THP_CRC_CHECK_OFFSET                        (19)
#define THP_TX_NUM                                  (18)
#define THP_RX_NUM                                  (40)
#define THP_DEBUG_BUF_SIZE                          (48)
#define THP_REPORT_BUF_SIZE                         (12*21)//252
#define THP_RESERVES_BIG_BUF_SIZE                   (0)//1512
#define TRAN_THP_FRAME_DATA_LEN \
        (THP_DATA_HEAD_SIZE + THP_DEBUG_BUF_SIZE + \
        THP_REPORT_BUF_SIZE + THP_RESERVES_BIG_BUF_SIZE + \
        (THP_TX_NUM * THP_RX_NUM + 2 * (THP_TX_NUM + THP_RX_NUM)) * 2 + 1) //sizeof(struct frame_data)


#define THP_CRC_LEN                             (TRAN_THP_FRAME_DATA_LEN - THP_CRC_CHECK_OFFSET - 1 - THP_REPORT_BUF_SIZE)
#define THP_TOUCH_EVENT_OFFSET                  (TRAN_THP_FRAME_DATA_LEN - THP_REPORT_BUF_SIZE)

#define THP_INPUT_DOWN                          0x01
#define THP_INPUT_UP                            0x00
#define THP_INPUT_CONTACT                       0x02
#define THP_EVENT_DOWN(flag)                    ((THP_INPUT_DOWN == flag) || (THP_INPUT_CONTACT == flag))
#define THP_EVENT_UP(flag)                      (THP_INPUT_UP == flag)


#define THP_GET_PROPERTY(np, prop_name, dest, default_val) \
    do { \
        ret = of_property_read_u32(np, prop_name, &(dest)); \
        if (ret < 0) { \
            FTS_ERROR("Unable to get " #prop_name ", using default: %d", default_val); \
            (dest) = default_val; \
        } \
    } while (0)

/* global variable */
extern struct tran_thp_scene g_thp_scene;


#pragma pack(1)
struct frame_data {
    //uint8_t point_buf[FTS_TOUCH_DATA_LEN_V2];  //84
    uint8_t protocol_version;                  //1
    uint16_t head_cnt;                         //2
    int32_t crc;                               //4
    int32_t crc_len;                           //4
    int32_t n_crc;                             //4
    int32_t n_crc_len;                         //4
    uint8_t data_state;                        //1
    uint8_t data_type;                         //1
    uint8_t event_info;                        //1
    uint8_t tp_noise_m;                        //1
    uint8_t scan_mode;                          //1
    uint8_t scan_rate_index;                    //1
    uint16_t scan_freq;                         //2
    uint16_t frame_cnt;                        //2
    uint16_t drop_frame_no;                    //2
    uint16_t noise_r0;                         //2
    uint16_t noise_r1;                         //2
    uint16_t noise_r2;                          //2
    uint16_t noise_r3;                          //2
    uint16_t noise_r4;                          //2
    uint16_t noise_r5;                          //2
    uint16_t cur_frame_len;                     //2
    uint16_t next_frame_len;                    //2
    uint8_t numCol;                             //1
    uint8_t numRow;                             //1
    uint16_t ic_ms_time;                        //2
    uint16_t report_rate;                       //2
    uint8_t scan_freq_index;                    //1
    uint8_t  write_cmd_cnt;                     //1
    uint8_t framedata_type;                     //1
    uint16_t flg_buf;                            //2
    uint8_t debug_buf_size;                      //1
    uint16_t reserved_big_buf_size;             //2
    uint16_t write_cmd;                         //2
    uint16_t mc_data[THP_TX_NUM * THP_RX_NUM];          // 1292
    uint16_t scap_raw[THP_TX_NUM + THP_RX_NUM];         // 110
    uint16_t scap2_raw[THP_TX_NUM + THP_RX_NUM];        // 110
    uint8_t debug_buf[THP_DEBUG_BUF_SIZE];          // 48
    uint8_t reserved_big_buf[THP_RESERVES_BIG_BUF_SIZE];// 1512
    uint8_t touch_num;                        // 1
    uint8_t thp_report_buf[THP_REPORT_BUF_SIZE]; // 252
};
#pragma pack()


#endif
