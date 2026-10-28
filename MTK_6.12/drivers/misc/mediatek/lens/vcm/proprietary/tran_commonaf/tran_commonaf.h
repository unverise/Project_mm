//读0x00寄存器         寄存器中的值
#define DW9800SV_CHIPID	0xEB
#define AW8601AF_CHIPID	0x01
#define GT9772B_CHIPID	0xF2
#define CN3938HP_CHIPID	0xEB
#define AW86017AF_CHIPID 0x03
//读0x0B寄存器
#define CN3927VLC_CHIPID	0x85
//读0x03              寄存器中的值
#define AK7316_CHIPID   0x1C
#define AK7314C_CHIPID  0X1E

static unsigned int af_i2c_slave_addr[] = {0x18,0x48, 0xFF};
static unsigned int afchip_id_addr_table[] = {0x00,0x03,0x0B,0xFF};

#define I2CCOMM_MAXSIZE 4
/* I2C format */
struct stVCM_I2CFormat {
    /* Register address */
    uint8_t Addr[I2CCOMM_MAXSIZE]; //AF move addr
    uint8_t AddrNum;

    /* Register Data */
    uint8_t CtrlData[I2CCOMM_MAXSIZE];
    uint8_t BitR[I2CCOMM_MAXSIZE];
    uint8_t Mask1[I2CCOMM_MAXSIZE];
    uint8_t BitL[I2CCOMM_MAXSIZE];
    uint8_t Mask2[I2CCOMM_MAXSIZE];
    uint8_t DataNum;
};

#define I2CSEND_MAXSIZE 4

struct VcmDriverConfig {
	int af_id;
	int af_id_sec[2];//add、value,if af_id is same use af_id_sec
	// Init param
	char init_table[16][3];
	char init_table_11bit[16][3];

	// release param
	char release_table[8][3];
	unsigned int origin_focus_pos;
	int move_steps;
	int move_delay_ms;

	uint8_t I2CSendNum;
	struct stVCM_I2CFormat I2Cfmt[I2CSEND_MAXSIZE];
};

static  struct VcmDriverConfig tran_vcm_configs[] = {

	{.af_id=DW9800SV_CHIPID,
	.af_id_sec={0x0B,0xFF},
	 .init_table={{0x01,0x02, 0x01}, {0x01,0x02, 0x00}, {0x01,0xFE, 0xFE},{0x01,0x02, 0x02}, {0x01,0x06, 0x40}, {0x01,0x07, 0x60},{0x01,0xFE, 0xFE}},
	 .init_table_11bit={0},
	 .origin_focus_pos=512,
	 .move_steps = 100,
	 .move_delay_ms = 5,
	 .release_table={{0x01,0x02, 0x01}},
	 .I2CSendNum = 2,
	 .I2Cfmt[0] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x03,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 8,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,
	 },
	 .I2Cfmt[1] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x04,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 0,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,
	 },
	},
	{
	 .af_id=AW8601AF_CHIPID,
	 .af_id_sec={0xFF,0xFF},
	 .init_table={{0x01, 0x02, 0x01},{0x01, 0x02, 0x00},{0x03, 0x01},{0x01, 0x02, 0x02},{0x01, 0x06, 0x40},{0x01, 0x07, 0x08}},
	 .init_table_11bit={0},
	 .origin_focus_pos=512,
	 .move_steps = 100,
	 .move_delay_ms = 5,
	 .release_table={{0x01,0x02, 0x01}},
	 .I2CSendNum = 2,
	 .I2Cfmt[0] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x03,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 8,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,
	 },
	 .I2Cfmt[1] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x04,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 0,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,
	 },
	},
	{.af_id=CN3938HP_CHIPID,
	 .af_id_sec={0x0B,0x00},
	 .init_table={{0x01, 0x02, 0x01},{0x01, 0x02, 0x00},{0x01, 0xFE, 0xFE},{0x01, 0x02, 0x02},{0x01, 0x06, 0x40},{0x01, 0x07, 0x02},{0x01, 0xFE, 0xFE}},
	 .init_table_11bit={0},
	 .origin_focus_pos=512,
	 .move_steps = 100,
	 .move_delay_ms = 5,
	 .release_table={{0x01,0x02, 0x01}},
	 .I2CSendNum = 2,
	 .I2Cfmt[0] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x03,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 8,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,
	 },
	 .I2Cfmt[1] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x04,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 0,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,
	 },
	},
	{.af_id=AW86017AF_CHIPID,
	 .af_id_sec={0xFF,0xFF},
	 .init_table={{0x01, 0xed, 0x0ab},{0x01, 0x02, 0x01},{0x01, 0x02, 0x00},{0x01, 0x06, 0x88},{0x01, 0x07, 0x01}},
	 .init_table_11bit={0},
	 .origin_focus_pos=512,
	 .move_steps = 100,
	 .move_delay_ms = 5,
	 .release_table={{0x01,0x02, 0x01}},
	 .I2CSendNum = 2,
	 .I2Cfmt[0] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x03,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 8,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,
	 },
	 .I2Cfmt[1] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x04,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 0,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,
	 },
	},
	{
	 .af_id=CN3927VLC_CHIPID,
	 .af_id_sec={0xFF,0xFF},
	 .init_table={{0x01,0xec, 0xa3},{0x01, 0xa1, 0x14},{0x01, 0xf2, 0x10},{0x01, 0xdc, 0x51}},
	 .init_table_11bit={0},
	 .origin_focus_pos=0,
	 .move_steps = 100,
	 .move_delay_ms = 5,
	 .release_table={{0x01,0xEC, 0xA3},{0x01, 0xA1, 0x00},{0x01, 0xDC, 0x51},{0x01, 0x00, 0x05}},
	 .I2CSendNum = 1,
	 .I2Cfmt[0] ={
		 .AddrNum = 0,
		 .DataNum = 2,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 4,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,

		 .CtrlData[1] = 0x00,
		 .BitR[1] = 0,
		 .Mask1[1] = 0xFF,
		 .BitL[1] = 4,
		 .Mask2[1] = 0xFF,
	 },
	},
	{.af_id=AK7316_CHIPID,
	 .af_id_sec={0xFF,0xFF},
	 .init_table={{0x01,0x02, 0x00}},
	 .init_table_11bit={0},
	 .origin_focus_pos=512,
	 .move_steps = 100,
	 .move_delay_ms = 5,
	 .release_table={{0x01,0x02, 0x20}},
	 .I2CSendNum = 2,
	 .I2Cfmt[0] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x00,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 2,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,
	 },
	 .I2Cfmt[1] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x01,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 0,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 6,
		 .Mask2[0] = 0xFF,
	 },
	},
	{.af_id=AK7314C_CHIPID,
	 .af_id_sec={0xFF,0xFF},
	 .init_table={{0x01,0x02, 0x00}},
	 .init_table_11bit={0},
	 .origin_focus_pos=512,
	 .move_steps = 100,
	 .move_delay_ms = 5,
	 .release_table={{0x01,0x02, 0x20}},
	 .I2CSendNum = 2,
	 .I2Cfmt[0] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x00,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 2,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,
	 },
	 .I2Cfmt[1] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x01,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 0,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 6,
		 .Mask2[0] = 0xFF,
	 },
	},
	{.af_id=GT9772B_CHIPID,
	 .af_id_sec={0xFF,0xFF},
	 .init_table={{0x01, 0x0B, 0x53},{0x03, 0x05},{0x01, 0x02, 0x02},{0x01, 0x06, 0x00},{0x01, 0x07, 0x07}},
	 .init_table_11bit={0},
	 .origin_focus_pos=0,
	 .move_steps = 100,
	 .move_delay_ms = 5,
	 .release_table={{0x01,0x02, 0x01}},
	 .I2CSendNum = 2,
	 .I2Cfmt[0] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x03,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 8,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,
	 },
	 .I2Cfmt[1] ={
		 .AddrNum = 1,
		 .Addr[0] = 0x04,
		 .DataNum = 1,
		 .CtrlData[0] = 0x00,
		 .BitR[0] = 0,
		 .Mask1[0] = 0xFF,
		 .BitL[0] = 0,
		 .Mask2[0] = 0xFF,
	 },
	},
};
