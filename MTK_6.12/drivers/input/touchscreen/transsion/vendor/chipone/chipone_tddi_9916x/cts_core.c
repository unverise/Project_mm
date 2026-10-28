#define LOG_TAG         "Core"
#include <linux/vmalloc.h>
//#include "cts_config.h"
#include "cts_platform.h"
#include "cts_sfctrl.h"
#include "cts_flash.h"
#include "cts_sysfs.h"
#include "cts_firmware.h"
#include "cts_charger_detect.h"
#include "cts_earjack_detect.h"
#include "cts_tcs.h"
#include "cts_transfer.h"
#include "cts_core.h"


u16 cts_crc16(const u8 *data, size_t len)
{
    u16 crc = 0;

    const static u16 crc16_table[] = {
        0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
        0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
        0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
        0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
        0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
        0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
        0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
        0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
        0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
        0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
        0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
        0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
        0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
        0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
        0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
        0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
        0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
        0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
        0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
        0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
        0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
        0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
        0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
        0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
        0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
        0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
        0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
        0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
        0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
        0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
        0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
        0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202,
    };

    while (len) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ *data) & 0xFF];
        data++;
        len--;
    }

    return crc;
}

u32 cts_crc32(const u8 *data, size_t len)
{
    const static u32 crc32_table[] = {
        0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC,
        0x17C56B6B,
        0x1A864DB2, 0x1E475005, 0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6,
        0x2B4BCB61,
        0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD, 0x4C11DB70,
        0x48D0C6C7,
        0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2,
        0x52568B75,
        0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014,
        0x7DDC5DA3,
        0x709F7B7A, 0x745E66CD, 0x9823B6E0, 0x9CE2AB57, 0x91A18D8E,
        0x95609039,
        0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5, 0xBE2B5B58,
        0xBAEA46EF,
        0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA,
        0xA06C0B5D,
        0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C,
        0xC3F706FB,
        0xCEB42022, 0xCA753D95, 0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46,
        0xFF79A6F1,
        0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D, 0x34867077,
        0x30476DC0,
        0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5,
        0x2AC12072,
        0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x018AEB13,
        0x054BF6A4,
        0x0808D07D, 0x0CC9CDCA, 0x7897AB07, 0x7C56B6B0, 0x71159069,
        0x75D48DDE,
        0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02, 0x5E9F46BF,
        0x5A5E5B08,
        0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D,
        0x40D816BA,
        0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B,
        0xBB60ADFC,
        0xB6238B25, 0xB2E29692, 0x8AAD2B2F, 0x8E6C3698, 0x832F1041,
        0x87EE0DF6,
        0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A, 0xE0B41DE7,
        0xE4750050,
        0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055,
        0xFEF34DE2,
        0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683,
        0xD1799B34,
        0xDC3ABDED, 0xD8FBA05A, 0x690CE0EE, 0x6DCDFD59, 0x608EDB80,
        0x644FC637,
        0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB, 0x4F040D56,
        0x4BC510E1,
        0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4,
        0x51435D53,
        0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42,
        0x32D850F5,
        0x3F9B762C, 0x3B5A6B9B, 0x0315D626, 0x07D4CB91, 0x0A97ED48,
        0x0E56F0FF,
        0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623, 0xF12F560E,
        0xF5EE4BB9,
        0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC,
        0xEF68060B,
        0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A,
        0xC0E2D0DD,
        0xCDA1F604, 0xC960EBB3, 0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610,
        0xB07DABA7,
        0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B, 0x9B3660C6,
        0x9FF77D71,
        0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74,
        0x857130C3,
        0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645,
        0x4A4FFBF2,
        0x470CDD2B, 0x43CDC09C, 0x7B827D21, 0x7F436096, 0x7200464F,
        0x76C15BF8,
        0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24, 0x119B4BE9,
        0x155A565E,
        0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B,
        0x0FDC1BEC,
        0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D,
        0x2056CD3A,
        0x2D15EBE3, 0x29D4F654, 0xC5A92679, 0xC1683BCE, 0xCC2B1D17,
        0xC8EA00A0,
        0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C, 0xE3A1CBC1,
        0xE760D676,
        0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673,
        0xFDE69BC4,
        0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5,
        0x9E7D9662,
        0x933EB0BB, 0x97FFAD0C, 0xAFB010B1, 0xAB710D06, 0xA6322BDF,
        0xA2F33668,
        0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4
    };

    u32 crc = 0;

    while (len) {
        crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *data) & 0xFF];

        data++;
        len--;
    }

    return crc;
}

static int icnl9916_set_access_ddi_reg(struct cts_device *cts_dev, bool enable)
{
    int ret;
    u8 access_flag;
    u8 val[2] = {0, 0};

    cts_info("ICNL9916 %s access ddi reg", enable ? "enable" : "disable");

    ret = cts_read_hw_reg(cts_dev,
            cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_DDI_REG_CTRL,
            &access_flag, 1, false);
    if (ret) {
        cts_err("Read HW_REG_DDI_REG_CTRL failed %d", ret);
        return ret;
    }

    access_flag = enable ? (access_flag | 0x01) : (access_flag & (~0x01));
    ret = cts_write_ddi_reg(cts_dev,
            cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_DDI_REG_CTRL,
            &access_flag, sizeof(access_flag), false);
    if (ret) {
        cts_err("Write HW_REG_DDI_REG_CTRL %02x failed %d", access_flag, ret);
        return ret;
    }

    val[0] = enable ? 1 : 0;
    ret = cts_write_ddi_reg(cts_dev,
            cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_HPPROG,
            &val[0], 1, false);
    if (ret) {
        cts_err("Write CTS_DEV_HW_REG_HPPROG failed %d", ret);
        return ret;
    }

    if (enable) {
        val[0] = 0x59;
        val[1] = 0x5A;
    } else {
        val[0] = 0x5A;
        val[1] = 0x5A;
    }
    ret = cts_write_ddi_reg(cts_dev, 0x3DFF0, val, 2, false);
    if (ret) {
        cts_err("Write password to F0 failed %d", ret);
        return ret;
    }

    if(enable) {
        val[0] = 0xA6;
        val[1] = 0xA5;
    } else {
        val[0] = 0x5A;
        val[1] = 0x5A;
    }
    ret = cts_write_ddi_reg(cts_dev, 0x3DFF4, val, 2, false);
    if (ret) {
        cts_err("Write password to F1 failed %d", ret);
        return ret;
    }

    return 0;
}

static struct cts_sfctrl icnl9916_sfctrl = {
    .xchg_sram_base = 96 * 1024,
    .ops = &cts_sfctrlv2_ops
};

static struct cts_sfctrl icnl9916c_sfctrl = {
    .xchg_sram_base = 96 * 1024,
    .ops = &cts_sfctrlv2_ops
};

static struct cts_sfctrl icnl9922_sfctrl = {
    .xchg_sram_base = 96 * 1024,
    .ops = &cts_sfctrlv2_ops
};

static struct cts_sfctrl icnl9922c_sfctrl = {
    .xchg_sram_base = 0x24000,
    .ops = &cts_sfctrlv2_ops
};

static struct cts_sfctrl icnl9951_sfctrl = {
    .xchg_sram_base = 160 * 1024,
    .ops = &cts_sfctrlv2_ops
};

static struct cts_sfctrl icnl9951c_sfctrl = {
    .xchg_sram_base = 0x027010,
    .ops = &cts_sfctrlv2_ops
};

const static struct cts_device_hwdata cts_device_hwdatas[] = {
    {
        .name = "ICNL9911C",
        .hwid = CTS_DEV_HWID_ICNL9911C,
        .fwid = CTS_DEV_FWID_ICNL9911C,
        .support_cascade = false,
        .num_row = 32,
        .num_col = 18,
        .regdef_base = 0x30000,
        .sfctl_base = 0x34000,
        .i2ccfg_base = 0x37000,
        .spi_base = 0x38000,
        .flash_ddi_para_addr = 0x19000,
        .flash_ddi_init_en_addr = 0x1B000,
        .spi_protocol = CTS_SPI_PRW,
        .sfctrl = &icnl9916_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },
    {
        .name = "ICNL9911D",
        .hwid = CTS_DEV_HWID_ICNL9911D,
        .fwid = CTS_DEV_FWID_ICNL9911D,
        .support_cascade = false,
        .num_row = 32,
        .num_col = 18,
        .regdef_base = 0x30000,
        .sfctl_base = 0x34000,
        .i2ccfg_base = 0x37000,
        .spi_base = 0x38000,
        .flash_ddi_para_addr = 0x19000,
        .flash_ddi_init_en_addr = 0x1B000,
        .spi_protocol = CTS_SPI_PRW,
        .sfctrl = &icnl9916_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },
    {
        .name = "ICNL9916",
        .hwid = CTS_DEV_HWID_ICNL9916,
        .fwid = CTS_DEV_FWID_ICNL9916,
        .support_cascade = false,
        .num_row = 32,
        .num_col = 18,
        .regdef_base = 0x30000,
        .sfctl_base = 0x34000,
        .i2ccfg_base = 0x37000,
        .spi_base = 0x38000,
        .flash_ddi_para_addr = 0x19000,
        .flash_ddi_init_en_addr = 0x1B000,
        .spi_protocol = CTS_SPI_PRW,
        .sfctrl = &icnl9916_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },
    {
        .name = "ICNL9916C",
        .hwid = CTS_DEV_HWID_ICNL9916C,
        .fwid = CTS_DEV_FWID_ICNL9916C,
        .support_cascade = false,
        .num_row = 32,
        .num_col = 18,
        .regdef_base = 0x30000,
        .sfctl_base = 0x34000,
        .i2ccfg_base = 0x37000,
        .spi_base = 0x38000,
        .flash_ddi_para_addr = 0x19000,
        .flash_ddi_init_en_addr = 0x1B000,
        .spi_protocol = CTS_SPI_PRW,
        .sfctrl = &icnl9916c_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },
    {
        .name = "ICNL9916X",
        .hwid = CTS_DEV_HWID_ICNL9916X,
        .fwid = CTS_DEV_FWID_ICNL9916X,
        .support_cascade = false,
        .num_row = 32,
        .num_col = 18,
        .has_flash = false,
        .regdef_base = 0x30000,
        .sfctl_base = 0x34000,
        .i2ccfg_base = 0x37000,
        .spi_base = 0x38000,
        .flash_ddi_para_addr = 0x19000,
        .flash_ddi_init_en_addr = 0x1B000,
        .spi_protocol = CTS_SPI_DRW,
        .spi_addr_prog = 0x60,
        .spi_addr_norm = 0xf0,
        .sfctrl = &icnl9916c_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },
    {
        .name = "ICNL9905",
        .hwid = CTS_DEV_HWID_ICNL9905,
        .fwid = CTS_DEV_FWID_ICNL9905,
        .support_cascade = false,
        .num_row = 32,
        .num_col = 15,
        .regdef_base = 0x30000,
        .sfctl_base = 0x34000,
        .i2ccfg_base = 0x37000,
        .spi_base = 0x38000,
        .spi_protocol = CTS_SPI_DRW,
        .spi_addr_prog = 0x60,
        .spi_addr_norm = 0xf0,
        .sfctrl = &icnl9916c_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },
    {
        .name = "ICNL9922",
        .hwid = CTS_DEV_HWID_ICNL9922,
        .fwid = CTS_DEV_FWID_ICNL9922,
        .support_cascade = false,
        .num_row = 36,
        .num_col = 18,
        .regdef_base = 0x30000,
        .sfctl_base = 0x34000,
        .i2ccfg_base = 0x37000,
        .spi_base = 0x38000,
        .flash_ddi_para_addr = 0x19000,
        .flash_ddi_init_en_addr = 0x1B000,
        .spi_protocol = CTS_SPI_PRW,
        .sfctrl = &icnl9922_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },

    {
        .name = "ICNL9922C",
        .hwid = CTS_DEV_HWID_ICNL9922C,
        .fwid = CTS_DEV_FWID_ICNL9922C,
        .support_cascade = false,
        .num_row = 36,
        .num_col = 18,
        .regdef_base = 0x70000,
        .sfctl_base = 0x74000,
        .i2ccfg_base = 0x77000,
        .spi_base = 0x78000,
        .flash_ddi_para_addr = 0x19000,
        .flash_ddi_init_en_addr = 0x1B000,
        .spi_protocol = CTS_SPI_DRW,
        .sfctrl = &icnl9922c_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },

    {
        .name = "ICNL9951",
        .hwid = CTS_DEV_HWID_ICNL9951,
        .fwid = CTS_DEV_FWID_ICNL9951,
        .support_cascade = true,
        .num_row = 50,
        .num_col = 16,
        .regdef_base = 0x70000,
        .sfctl_base = 0x74000,
        .i2ccfg_base = 0x77000,
        .spi_base = 0x78000,
        .gpio_base = 0x73000,
        .has_flash = true,
        .flash_ddi_para_addr = 0x31000,
        .flash_ddi_init_en_addr = 0x33000,
        .spi_protocol = CTS_SPI_PRW,
        .i2c_protocol = CTS_I2C_PRW,
        .sfctrl = &icnl9951_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },

    {
        .name = "ICNL9951R",
        .hwid = CTS_DEV_HWID_ICNL9951R,
        .fwid = CTS_DEV_FWID_ICNL9951R,
        .support_cascade = true,
        .num_row = 50,
        .num_col = 16,
        .regdef_base = 0x70000,
        .sfctl_base = 0x74000,
        .i2ccfg_base = 0x77000,
        .i2c_addr_norm = 0x48,
        .spi_base = 0x78000,
        .gpio_base = 0x73000,
//        .has_flash = true,
        .has_flash = false,
        .flash_ddi_para_addr = 0x31000,
        .flash_ddi_init_en_addr = 0x33000,
        .spi_protocol = CTS_SPI_PRW,
        .i2c_protocol = CTS_I2C_PRW,
        .sfctrl = &icnl9951_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },

    {
        .name = "ICNL9951C",
        .hwid = CTS_DEV_HWID_ICNL9951C,
        .fwid = CTS_DEV_FWID_ICNL9951C,
        .support_cascade = true,
        .num_row = 50,
        .num_col = 32,
        .regdef_base = 0x70000,
        .sfctl_base = 0x74000,
        .i2ccfg_base = 0x77000,
        .spi_base = 0x78000,
        .gpio_base = 0x73000,
        .krang_base = 0x7b000,
        .has_flash = false,
        .flash_ddi_para_addr = 0x31000,
        .flash_ddi_init_en_addr = 0x33000,
        .spi_protocol = CTS_SPI_DRW,
        .i2c_protocol = CTS_I2C_DRW,
        .sfctrl = &icnl9951c_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    }
};

static int cts_init_hwdata(struct cts_device *cts_dev,
        u32 hwid, u16 fwid)
{
    int i;

    cts_info("Init hardware data hwid: %06x fwid: %04x", hwid, fwid);

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (hwid == cts_device_hwdatas[i].hwid ||
        fwid == cts_device_hwdatas[i].fwid) {
            cts_dev->hwdata = &cts_device_hwdatas[i];
            return 0;
        }
    }

    return -EINVAL;
}

static void cts_deinit_hwdata(struct cts_device *cts_dev)
{
    cts_info("Deinit hwdata\n");

    cts_dev->hwdata = NULL;
}

void cts_lock_device(const struct cts_device *cts_dev)
{
    cts_dbg("*** Lock ***");

    mutex_lock(&cts_dev->pdata->dev_lock);
}

void cts_unlock_device(const struct cts_device *cts_dev)
{
    cts_dbg("### Un-Lock ###");

    mutex_unlock(&cts_dev->pdata->dev_lock);
}

static int cts_get_touchinfo(struct cts_device *cts_dev,
        void *touch_info)
{
    cts_dbg("Get touch info");

    if (cts_dev->rtdata.boot_mode == CTS_BOOT_MODE_PROGRAM) {
        cts_warn("Get touch info in program mode");
        return -ENODEV;
    }

    return cts_tcs_get_touchinfo(cts_dev, touch_info);
}

static int cts_get_dev_boot_mode(const struct cts_device *cts_dev,
        u8 *dev_boot_mode)
{
    int ret;
    bool force_drw = false;

    if (!cts_dev->hwdata) {
        cts_err("hwdata is not initialized\n");
        return -EINVAL;
    }

    if (cts_dev->hwdata->i2c_protocol == CTS_I2C_DRW ||
        cts_dev->hwdata->spi_protocol == CTS_SPI_DRW)
        force_drw = true;
    ret = cts_read_hw_reg(cts_dev,
            cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_CURRENT_MODE,
            dev_boot_mode, sizeof(u8), force_drw);
    if (ret) {
        cts_err("%s: get boot mode failed\n", __func__);
    }

    *dev_boot_mode &= CTS_DEV_BOOT_MODE_MASK;

    if(!is_valid_boot_mode(*dev_boot_mode))
        ret = -EINVAL;

    cts_info("Curr dev boot mode: %u(%s)", *dev_boot_mode,
        cts_dev_boot_mode2str(cts_dev, *dev_boot_mode));

    return ret;
}

static int cts_set_dev_boot_mode(struct cts_device *cts_dev, u8 boot_mode)
{
    int force_drw = false;
    int ret;

    if (cts_dev->hwdata) {
        cts_info("Set dev boot mode to %u(%s)", boot_mode,
        cts_dev_boot_mode2str(cts_dev, boot_mode));
        if (cts_dev->hwdata->i2c_protocol == CTS_I2C_DRW ||
            cts_dev->hwdata->spi_protocol == CTS_SPI_DRW)
            force_drw = true;
        else
            cts_enter_program_mode(cts_dev);

        ret = cts_write_hw_reg(cts_dev,
                cts_dev->hwdata->regdef_base + CTS_DEV_HW_REG_BOOT_MODE,
                &boot_mode, 1, force_drw);
        if (ret)
            cts_err("Write hw register BOOT_MODE failed %d", ret);
    } else {
        cts_info("hwdata is not initialized, return directly!\n");
        ret = -EINVAL;
    }

    return ret;
}

static int cts_init_fwdata(struct cts_device *cts_dev)
{
    struct cts_device_fwdata *fwdata = &cts_dev->fwdata;
    int ret;

    cts_info("Init firmware data");

    if (cts_dev->rtdata.boot_mode == CTS_BOOT_MODE_PROGRAM) {
        cts_warn("Init firmware data while in program mode");
        ret = cts_enter_normal_mode(cts_dev);
        if (ret)
            return -EINVAL;
    }

    ret = cts_tcs_get_fw_ver(cts_dev, &fwdata->version);
    if (ret < 0) {
        cts_err("get_fw_ver failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_module_id(cts_dev, &fwdata->module_id);
    if (ret < 0) {
        cts_err("get_module_id failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_lib_ver(cts_dev, &fwdata->lib_version);
    if (ret < 0) {
        cts_err("get_lib_ver failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_ddi_ver(cts_dev, &fwdata->ddi_version);
    if (ret < 0) {
        cts_err("get_ddi_ver failed");
        return -EINVAL;
    }

    if (cts_dev->hwdata->support_cascade) {
        ret = cts_tcs_get_ic_channel_info(cts_dev, &fwdata->channel_info);
        if (!!ret)
            cts_err("get touch_xy_info failed, ret:%d\n", ret);

        ret = cts_tcs_get_panel_info(cts_dev, &fwdata->panel_info);
        if (!!ret) {
            cts_err("get touch_xy_info failed, ret:%d\n", ret);
        } else {
            if (fwdata->panel_info.res_x != cts_dev->pdata->res_x ||
                fwdata->panel_info.res_y != cts_dev->pdata->res_y)
                cts_warn("resolution mismatch between dts and fw"
                    "fw res_x:%d, dts res_x:%d, fw res_y:%d, dts res_y:%d",
                    fwdata->panel_info.res_x, cts_dev->pdata->res_x,
                    fwdata->panel_info.res_y, cts_dev->pdata->res_y);
        }
    } else {
        ret = cts_tcs_get_touch_xy_info(cts_dev, &fwdata->xy_info);
        if (!!ret) {
            cts_err("get touch_xy_info failed, ret:%d\n", ret);
        } else {
            if (fwdata->xy_info.res_x != cts_dev->pdata->res_x ||
                fwdata->xy_info.res_y != cts_dev->pdata->res_y)
                cts_warn("resolution mismatch between dts and fw"
                    "fw res_x:%d, dts res_x:%d, fw res_y:%d, dts res_y:%d",
                    fwdata->xy_info.res_x, cts_dev->pdata->res_x,
                    fwdata->xy_info.res_y, cts_dev->pdata->res_y);
        }
    }

    ret = cts_tcs_get_flip_x(cts_dev, &fwdata->flip_x);
    if (ret < 0) {
        cts_err("get_flip_x failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_flip_y(cts_dev, &fwdata->flip_y);
    if (ret < 0) {
        cts_err("get_flip_y failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_swap_axes(cts_dev, &fwdata->swap_axes);
    if (ret < 0) {
        cts_err("get_swap_axes failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_int_mode(cts_dev, &fwdata->int_mode);
    if (ret < 0) {
        cts_err("get_int_mode failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_int_keep_time(cts_dev, &fwdata->int_keep_time);
    if (ret < 0) {
        cts_err("get_int_keep_time failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_rawdata_target(cts_dev, &fwdata->rawdata_target);
    if (ret < 0) {
        cts_err("get_rawdata_target failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_esd_method(cts_dev, &fwdata->esd_method);
    if (ret < 0) {
        cts_err("get_esd_method failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_has_int_data(cts_dev,
            &fwdata->has_int_data);
    if (ret < 0) {
        cts_err("get_has_int_data failed: %d", ret);
        return -EINVAL;
    }

    if (fwdata->has_int_data) {
        ret = cts_tcs_get_int_data_method(cts_dev,
                &fwdata->int_data_method);
        if (ret < 0) {
            cts_err("get_int_data_method failed: %d", ret);
            return -EINVAL;
        }
        if (fwdata->int_data_method >= INT_DATA_METHOD_CNT) {
            cts_err("Invalid int data method: %d",
                fwdata->int_data_method);
            return -EINVAL;
        }

        ret = cts_tcs_get_int_data_types(cts_dev,
                &fwdata->int_data_types);
        if (ret < 0) {
            cts_err("get_int_data_types failed: %d", ret);
            return -EINVAL;
        }
        fwdata->int_data_types &= INT_DATA_TYPE_MASK;

        cts_tcs_calc_int_data_size(cts_dev);
    }

    cts_info("fwver: %04x", fwdata->version);
    cts_info("module_id: %04x", fwdata->module_id);
    cts_info("libver: %04x", fwdata->lib_version);
    cts_info("ddi_version: %02x", fwdata->ddi_version);
    if (cts_dev->hwdata->support_cascade) {
        cts_info("panel_type: %d", fwdata->panel_info.panel_type);
        cts_info("panel_scal: %d", fwdata->panel_info.panel_scal);
        cts_info("panel_size: %d", fwdata->panel_info.panel_size);
        cts_info("res_x: %d", fwdata->panel_info.res_x);
        cts_info("res_y: %d", fwdata->panel_info.res_y);
        cts_info("fs_cols: %d", fwdata->panel_info.fs_cols);
        cts_info("ic_fs_cols: %d", fwdata->channel_info.ic_fs_cols);
        cts_info("fs_rows: %d", fwdata->panel_info.fs_rows);
        cts_info("ic_fs_rows: %d", fwdata->channel_info.ic_fs_rows);
        cts_info("gs_cols: %d", fwdata->panel_info.gs_cols);
        cts_info("ic_gs_cols: %d", fwdata->channel_info.ic_gs_cols);
        cts_info("gs_rows: %d", fwdata->panel_info.gs_rows);
        cts_info("ic_gs_rows: %d", fwdata->channel_info.ic_gs_rows);
        cts_info("pc_cols: %d", fwdata->panel_info.pc_cols);
        cts_info("ic_pc_cols: %d", fwdata->channel_info.ic_pc_cols);
        cts_info("pc_rows: %d", fwdata->panel_info.pc_rows);
        cts_info("ic_pc_rows: %d", fwdata->channel_info.ic_pc_rows);
        cts_info("pr_cols: %d", fwdata->panel_info.pr_cols);
        cts_info("ic_pr_cols: %d", fwdata->channel_info.ic_pr_cols);
        cts_info("pr_rows: %d", fwdata->panel_info.pr_rows);
        cts_info("ic_pr_rows: %d", fwdata->channel_info.ic_pr_rows);
    } else {
        cts_info("res_x: %d", fwdata->xy_info.res_x);
        cts_info("res_y: %d", fwdata->xy_info.res_y);
        cts_info("rows: %d", fwdata->xy_info.rows);
        cts_info("cols: %d", fwdata->xy_info.cols);
        cts_info("lp_rows: %d", fwdata->xy_info.lp_rows);
        cts_info("lp_cols: %d", fwdata->xy_info.lp_cols);
    }
    cts_info("flip_x: %d", fwdata->flip_x);
    cts_info("flip_y: %d", fwdata->flip_y);
    cts_info("swap_axes: %d", fwdata->swap_axes);
    cts_info("int_mode: %d", fwdata->int_mode);
    cts_info("int_keep_time: %d", fwdata->int_keep_time);
    cts_info("rawdata_target: %d", fwdata->rawdata_target);
    cts_info("esd_method: %d", fwdata->esd_method);
    cts_info("has_int_data: %d", fwdata->has_int_data);
    cts_info("int_data_method: %d", fwdata->int_data_method);
    cts_info("int_data_types: %d", fwdata->int_data_types);
    cts_info("int_data_size: %ld", fwdata->int_data_size);

    return ret;
}

#ifdef CONFIG_CTS_TP_PROXIMITY
static bool cts_is_proximity_enable(struct cts_device *cts_dev)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    return !!status->proximity;
}

static int cts_enable_proximity_mode(struct cts_device *cts_dev)
{
    return cts_tcs_set_proximity_mode(cts_dev, 1);
}

static int cts_exit_proximity_mode(struct cts_device *cts_dev)
{
    return cts_tcs_set_proximity_mode(cts_dev, 0);
}

static void cts_handle_proximity_event(bool status)
{
    cts_info("Handler proximity '%s' event", status ? "Near" : "Far");
}
#endif

int cts_irq_handler(struct cts_device *cts_dev)
{
    void *touch_info;
    int ret;

    cts_dbg("Enter IRQ handler");

    if (!cts_is_sram_mode(cts_dev)) {
        cts_err("IRQ triggered in program mode");
        return -EINVAL;
    }

    /*bsp:add for deep sleep gesture by guoxin.zhu 20230908 start*/
#if CTS_PATCH_COMERR_PM
    if ((cts_dev->rtdata.suspended)) {
        if(cts_dev->bus_type == CTS_BUS_I2C)
			pm_wakeup_event(&cts_dev->pdata->i2c_client->dev, 2000);
		else if (cts_dev->bus_type == CTS_BUS_SPI)
			pm_wakeup_event(&cts_dev->pdata->spi_client->dev, 2000);
    }

    if(cts_dev->rtdata.pm_suspend) {
        ret = wait_for_completion_timeout(
                  &cts_dev->rtdata.pm_completion,
                  msecs_to_jiffies(CTS_TIMEOUT_COMERR_PM));
        if (!ret) {
            cts_err("Bus don't resume from pm(deep),timeout,skip irq");
            return IRQ_HANDLED;
        }
    }
#endif
    /*bsp:add for deep sleep by guoxin.zhu 20230908 end*/

    touch_info = cts_dev->rtdata.touch_info;
    ret = cts_get_touchinfo(cts_dev, touch_info);
    if (ret) {
        cts_err("Get touch info failed %d", ret);
        return ret;
    }

    ret = cts_plat_process_touch_info(cts_dev, touch_info);
    if (ret)  //TODO: dump the bus buffer when continous err frames reported
        cts_err("process touch info failed, ret:%d\n", ret);

    return ret;
}

bool cts_is_device_suspended(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.suspended;
}

int cts_suspend_device(struct cts_device *cts_dev)
{
    u8 pwr_mode;
    struct cts_firmware *firmware = NULL;
    int ret;
//#STD:add for gesture Decoupling by kunrui.li 20251016 start
    struct tran_core_ops *ops = NULL;
//#STD:add for gesture Decoupling by kunrui.li 20251016 end

    cts_info("Suspend device");
/* Disable this check for sleep/gesture switch */

//#STD:add for gesture Decoupling by kunrui.li 20251016 start
   if(get_tpd_ops(&chipone_controller, &ops)) {
        cts_dev->rtdata.gesture_wakeup_enabled = ops->gesture_state_get(&chipone_controller);
         cts_info("gesture_wakeup_enabled = %d\n" , cts_dev->rtdata.gesture_wakeup_enabled);
    }
//#STD:add for gesture Decoupling by kunrui.li 20251016 end

    if (cts_dev->rtdata.suspended) {
        cts_warn("Suspend device while already suspended");
        return 0;
    }

    if (cts_dev->rtdata.boot_mode == CTS_BOOT_MODE_PROGRAM) {
        cts_info("Quit programming mode before suspend");
        ret = cts_enter_normal_mode(cts_dev);
        if (ret) {
            cts_err("Failed to exit program mode before suspend:%d", ret);
            return ret;
        }
    }

    cts_info("Set suspend mode:%s",
        cts_dev->rtdata.gesture_wakeup_enabled ? "gesture" : "sleep");

    if (cts_dev->fwdata.cts_fw->is_split_bins && !cts_dev->hwdata->has_flash &&
        cts_dev->rtdata.gesture_wakeup_enabled) {
        ret = cts_request_firmware(cts_dev, &firmware, cts_dev->hwdata->hwid,
                cts_dev->hwdata->fwid, 0);
        if (!ret) {
            ret = cts_update_firmware(cts_dev, firmware, CTS_SPLIT_BIN_GESTURE);
            if (ret) {
                cts_err("Update gesture bin failed %d", ret);
                return ret;
            }
        }
    }

    pwr_mode = cts_dev->rtdata.gesture_wakeup_enabled ?
            PWR_MODE_GSTR_WAKEUP : PWR_MODE_HIBERNATE;

    ret = cts_tcs_set_pwr_mode(cts_dev, pwr_mode);
    if (ret) {
        cts_err("Suspend device failed %d", ret);
        return ret;
    }

    cts_info("Device suspended ...");
    cts_dev->rtdata.suspended = true;

    //BSP:add for pocket mode by xihao.ding 20251129 start
    if(cts_dev->pdata->key_status_pocket && !IS_ERR_OR_NULL(cts_dev->pdata->ts_input_dev)) {//relese key
        input_report_key(cts_dev->pdata->ts_input_dev, KEY_LARGE_AREA, 0);
        input_sync(cts_dev->pdata->ts_input_dev);
        cts_dev->pdata->key_status_pocket = false;
        cts_info("pocket key report 251 0\n");
    }
    //BSP:add for pocket mode by xihao.ding 20251129 end

    return 0;
}

int cts_resume_device(struct cts_device *cts_dev)
{
    int retries = 3;
    int ret = 0;
    struct cts_firmware *firmware = NULL;

    cts_info("Resume device");
    /* reset to rebuild base for IC, and the rebuild process is done inside IC */
    cts_reset_device(cts_dev);

    /* Check whether device is in normal mode */
    while (--retries >= 0) {
        if (cts_plat_is_normal_mode(cts_dev->pdata))
            break;
        cts_reset_device(cts_dev);
    }

    if (retries < 0 ||
        (cts_dev->fwdata.cts_fw->is_split_bins &&
        !cts_dev->hwdata->has_flash &&
        cts_dev->rtdata.gesture_wakeup_enabled)) {
        cts_info("Need update firmware when resume");

        ret = cts_request_firmware(cts_dev, &firmware, cts_dev->hwdata->hwid,
            cts_dev->hwdata->fwid, 0);
        if (ret) {
            cts_err("Request default firmware failed %d, "
                "please update manually!!", ret);
            goto err_set_program_mode;
        }
        /* in both split bin and single bin case, normal bin is at bin[0] */
        ret = cts_update_firmware(cts_dev, firmware, CTS_SPLIT_BIN_NORMAL);
        cts_release_firmware(cts_dev, firmware);
        if (ret) {
            cts_err("Update default firmware failed %d", ret);
            goto err_set_program_mode;
        }
    }

#ifdef CONFIG_CTS_CHARGER_DETECT
    if (cts_is_charger_exist(cts_dev)) {
        int r = cts_set_dev_charger_attached(cts_dev, true);
        if (r)
            cts_err("Set dev charger attached failed %d", r);
    }
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    if (cts_is_earjack_exist(cts_dev)) {
        int r = cts_set_dev_earjack_attached(cts_dev, true);
        if (r) {
            cts_err("Set dev earjack attached failed %d", r);
        }
    }
#endif

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev))
        cts_enter_glove_mode(cts_dev);
#endif

#ifdef CONFIG_CTS_TP_PROXIMITY
    if (cts_is_proximity_enable(cts_dev)) {
        cts_enable_proximity_mode(cts_dev);
        cts_info("after update firmware, enable proximity mode.");
    }
#endif

    cts_dev->rtdata.suspended = false;
    return 0;

err_set_program_mode:
    cts_enter_program_mode(cts_dev);

    return ret;
}

bool cts_is_program_mode(const struct cts_device *cts_dev)
{
    return (cts_dev->rtdata.boot_mode == CTS_BOOT_MODE_PROGRAM);
}

bool cts_is_sram_mode(const struct cts_device *cts_dev)
{
    return (cts_dev->rtdata.boot_mode == CTS_BOOT_MODE_SRAM);
}

static inline void cts_init_rtdata(struct cts_device_rtdata *rtdata)
{
    memset(rtdata, 0, sizeof(struct cts_device_rtdata));
    rtdata->touch_info = vzalloc(CFG_CTS_INT_DATA_MAX_SIZE);
    if (!rtdata->touch_info)
        cts_err("allocate memory for touch_info failed!!\n");

    rtdata->suspended = false;
    rtdata->updating = false;
    rtdata->testing = false;
    rtdata->fw_log_redirect_enabled = false;
    rtdata->glove_mode_enabled = false;
    rtdata->esd_count = 0;
    rtdata->boot_mode = CTS_BOOT_MODE_UNKOWN;
#ifdef CONFIG_CTS_TP_PROXIMITY
    rtdata->proximity_num = 0;
    rtdata->proximity_status = false;
#endif
#if CTS_PATCH_COMERR_PM
    /*bsp:add for deep sleep by guoxin.zhu 20230908 start*/
    init_completion(&rtdata->pm_completion);
    /*bsp:add for deep sleep by guoxin.zhu 20230908 end*/
#endif
}

static inline void cts_deinit_rtdata(struct cts_device_rtdata *rtdata)
{
    cts_info("Deinit rtdata!!\n");

    if (rtdata->touch_info) {
        vfree(rtdata->touch_info);
        rtdata->touch_info = NULL;
    }
}

#ifdef FIX_DEFAULT_ENABLE_DRW
extern int cts_spi_send_recv(struct cts_platform_data *pdata, u8 *tx_buffer,
        size_t len, u8 *rx_buffer);
static int cts_disable_drw(struct cts_device *cts_dev)
{
    int ret;

    u8 txbuf[] = {
        0x60,
        0x07, 0x80, 0x34,
        0x00, 0x00, 0x04,
        0xdd, 0xe8,
        0x00, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00,
        0xdf, 0xfc,
        0x00, 0x00, 0x00
    };
    u8 rxbuf[] = {
        0x60,
        0x07, 0x80, 0x34,
        0x00, 0x00, 0x04,
        0xdd, 0xe8,
        0x00, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00,
        0xdf, 0xfc,
        0x00, 0x00, 0x00
    };

    ret = cts_spi_send_recv(cts_dev->pdata, txbuf, sizeof(txbuf), rxbuf);
    if (ret)
        cts_err("disable drw ret=%d", ret);
    return ret;
}
#endif

bool is_dev_bootmode_program_mode(const struct cts_device *cts_dev, u8 dev_boot_mode)
{
    const struct cts_device_hwdata *hwdata = cts_dev->hwdata;
    bool ret = false;

    if(!hwdata) {
        cts_warn("hwdata is not initialized, boot mode is unkown\n");
            ret = false;
    } else {
        if (strncmp(hwdata->name, "ICNL9911", strlen("ICNL9911"))) {
            if (CTS_DEV_BOOT_MODE_TCH_PROG == dev_boot_mode)
                ret = true;
            else
                ret = false;
        } else {
            if(cts_dev->bus_type == CTS_BUS_I2C) {
                if (CTS_DEV_BOOT_MODE_TCH_PROG == dev_boot_mode)
                    ret = true;
                else
                    ret = false;
            } else if (cts_dev->bus_type == CTS_BUS_SPI) {
                if (CTS_DEV_BOOT_MODE_SPI_PRG_9911 == dev_boot_mode)
                    ret = true;
                else
                    ret = false;
            }
        }
    }
    return ret;
}

int cts_is_casc_enabled(const struct cts_device *cts_dev)
{
    u8 casc_state;
    int force_drw = false;
    int ret;

    if (cts_dev->hwdata->i2c_protocol == CTS_I2C_DRW ||
        cts_dev->hwdata->spi_protocol == CTS_SPI_DRW)
        force_drw = true;

    ret = cts_read_hw_reg(cts_dev,
            cts_dev->hwdata->gpio_base + CTS_DEV_HW_REG_CASC_STATE,
            &casc_state, sizeof(casc_state), force_drw);
    if (ret) {
        cts_err("read casc_state failed, ret:%d\n", ret);
        return -EINVAL;
    }

    return !(casc_state & CTS_CASC_EN_MASK);
}

int cts_plat_is_casc_master(const struct cts_device *cts_dev)
{
    u8 casc_state;
    int force_drw = false;
    int ret;

    if (cts_dev->hwdata->i2c_protocol == CTS_I2C_DRW ||
        cts_dev->hwdata->spi_protocol == CTS_SPI_DRW)
        force_drw = true;

    ret = cts_read_hw_reg(cts_dev,
            cts_dev->hwdata->gpio_base + CTS_DEV_HW_REG_CASC_STATE,
            &casc_state, sizeof(casc_state), force_drw);
    if (ret) {
        cts_err("read casc_state failed, ret:%d\n", ret);
        return -EINVAL;
    }

    return casc_state & CTS_CASC_MASTER_MASK;
}

bool is_sram_mode(const struct cts_device *cts_dev, u8 dev_boot_mode)
{
    if (CTS_DEV_BOOT_MODE_SRAM == dev_boot_mode)
        return true;
    else
        return false;
}

int cts_stop_krang(struct cts_device *cts_dev)
{
    int i;
    int ret = -EINVAL;

    for (i = 0; i < CTS_RETRY; i++) {
        ret = cts_tcs_set_krang_stop(cts_dev);
        if (ret) {
            cts_err("send stop krang cmd failed ret:%d", ret);
            mdelay(CTS_M_DELAY);
            continue;
        }

        mdelay(CTS_M_DELAY);

        if (cts_tcs_is_krang_stopped(cts_dev))
            break;

        ret = -EINVAL;
    }

    return ret;
}

int cts_enter_program_mode(struct cts_device *cts_dev)
{
    u8 program_mode_cmd[ENTER_PROG_MODE_CMD_LEN] = ENTER_PROG_MODE_CMD;
    int ret = 0;

    cts_info("Enter program mode");

    if (cts_dev->rtdata.boot_mode == CTS_BOOT_MODE_PROGRAM) {
        cts_warn("Enter program mode while already in");
    }

    if (cts_is_sram_mode(cts_dev) && cts_dev->pdata->is_cascade_enabled) {
        ret = cts_stop_krang(cts_dev);
        if (ret) {
            cts_warn("stop krang failed when enter program mode");
//            return ret;
        }
    }

    if (cts_dev->bus_type == CTS_BUS_SPI) {
        ret = cts_spi_send_recv(cts_dev->pdata, program_mode_cmd,
                ENTER_PROG_MODE_CMD_LEN, NULL);
    } else if (cts_dev->bus_type == CTS_BUS_I2C) {
        ret = cts_plat_i2c_transfer(cts_dev->pdata,
                CTS_I2C_PROG_ADDR_DEFAULT, program_mode_cmd,
                ENTER_PROG_MODE_CMD_LEN, NULL, 0, 5, 10);
    }
    if (ret) {
        cts_err("Write magic number to i2c_dev: 0x%02x failed %d",
            CTS_I2C_PROG_ADDR_DEFAULT, ret);
        return ret;
    }
	cts_dev->rtdata.boot_mode = CTS_BOOT_MODE_PROGRAM;
	mdelay(5);
	
    return ret;
}

char *cts_dev_boot_mode2str(const struct cts_device *cts_dev, u8 dev_boot_mode)
{
    switch (dev_boot_mode) {
    case CTS_DEV_BOOT_MODE_IDLE:
        return "IDLE-BOOT";
    case CTS_DEV_BOOT_MODE_FLASH:
        return "FLASH-BOOT";
    case CTS_DEV_BOOT_MODE_SRAM:
        return "SRAM-BOOT";
    case CTS_DEV_BOOT_MODE_TCH_PROG:
        if (cts_dev->hwdata &&
            !strncmp(cts_dev->hwdata->name, "ICNL9911", strlen("ICNL9911")))
            return "I2C-PROGRAM-BOOT";
        else
            return "TCH-PROG-BOOT";
    case CTS_DEV_BOOT_MODE_DDI_PROG:
        return "DDI-PROGRAM-BOOT";
    case CTS_DEV_BOOT_MODE_SPI_PRG_9911:
        if (cts_dev->hwdata &&
            !strncmp(cts_dev->hwdata->name, "ICNL9911", strlen("ICNL9911")))
            return "SPI-PROGRAM-BOOT";
        else
            return "UNKNOWN-BOOT";
    default:
        cts_info("Unknown dev boot mode %u", dev_boot_mode);
        return "INVALID";
    }
}

bool is_valid_boot_mode(u8 dev_boot_mode)
{
    if(dev_boot_mode >= CTS_DEV_BOOT_MODE_IDLE &&
        dev_boot_mode < CTS_DEV_BOOT_MODE_UNKOWN)
        return true;
    else
        return false;
}

int cts_enter_normal_mode(struct cts_device *cts_dev)
{
    int ret = 0;
    u8 dev_boot_mode;
    int retries;
    u16 fwid = CTS_DEV_FWID_INVALID;
    u8 auto_boot = 0;
    u8 first_boot = 1;

    cts_info("Enter normal mode");

    ret = cts_get_dev_boot_mode(cts_dev, &dev_boot_mode);
    if (ret)
        cts_err("Get BOOT_MODE failed %d", ret);
    if (is_sram_mode(cts_dev, dev_boot_mode)) {
        cts_dev->rtdata.boot_mode = CTS_BOOT_MODE_SRAM;
        cts_info("already in normal mode, return directly\n");
        return 0;
    } else {
        cts_dev->rtdata.boot_mode = CTS_BOOT_MODE_UNKOWN;
    }

    if (cts_dev->hwdata->has_flash)
        auto_boot = 1;

#ifdef CFG_CTS_UPDATE_CRCCHECK
    auto_boot = 1;
#endif
    for (retries = 5; retries >= 0; retries--) {
        if (first_boot == 1 || auto_boot == 0) {
            ret = cts_set_dev_boot_mode(cts_dev, CTS_DEV_BOOT_MODE_SRAM);
            if (ret) {
                cts_err("Set BOOT_MODE to SRAM failed %d,"
                    "try to reset device", ret);
            }
            mdelay(30);
        }
        first_boot = 0;

        ret = cts_get_dev_boot_mode(cts_dev, &dev_boot_mode);
        if (ret) {
            cts_err("Get BOOT_MODE failed %d", ret);
        } else if (is_sram_mode(cts_dev, dev_boot_mode)) {
            cts_info("%s:%d,Curr boot mode %u(%s)", __func__, __LINE__,
                dev_boot_mode, cts_dev_boot_mode2str(cts_dev, dev_boot_mode));
            cts_dev->rtdata.boot_mode = CTS_BOOT_MODE_SRAM;
            break;
        }

        ret = cts_tcs_get_fw_id(cts_dev, &fwid);
        if (ret) {
            cts_err("Get firmware id failed %d, retries %d", ret, retries);
        } else {
            cts_info("Get firmware id: 0x%04x", fwid);
            cts_dev->rtdata.boot_mode = CTS_BOOT_MODE_SRAM;
            break;
        }

    cts_reset_device(cts_dev);
    mdelay(80);
    }

    if (retries >= 0)
        return 0;

    cts_dev->rtdata.boot_mode = CTS_BOOT_MODE_UNKOWN;
    return ret;
}

bool cts_is_device_enabled(const struct cts_device *cts_dev)
{
    return cts_dev->enabled;
}

int cts_start_device(struct cts_device *cts_dev)
{
#if defined(CONFIG_CTS_ESD_PROTECTION) || \
    defined(CONFIG_CTS_CHARGER_DETECT) || \
    defined(CONFIG_CTS_EARJACK_DETECT)
    struct chipone_ts_data *cts_data =
    container_of(cts_dev, struct chipone_ts_data, cts_dev);
#endif
    int ret;

    ret = cts_init_fwdata(cts_dev);
    if (ret) {
        cts_err("Device init firmware data failed %d", ret);
        return ret;
    }

    if (cts_is_device_enabled(cts_dev)) {
        cts_warn("Start device while already started");
        return 0;
    }

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    cts_enable_heartbeat_mechanism(cts_data);
#endif

#ifdef CONFIG_CTS_ESD_PROTECTION
    cts_enable_esd_protection(cts_data);
#endif

#ifdef CONFIG_CTS_CHARGER_DETECT
    cts_start_charger_detect(cts_data);
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    cts_start_earjack_detect(cts_data);
#endif

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev))
        cts_enter_glove_mode(cts_dev);
#endif

#ifdef CONFIG_CTS_TP_PROXIMITY
    if (cts_is_proximity_enable(cts_dev)) {
        cts_enable_proximity_mode(cts_dev);
        cts_info("after update firmware, enable proximity mode.");
    }
#endif

    ret = cts_plat_enable_irq(cts_dev->pdata);
    if (ret < 0) {
        cts_err("Enable IRQ failed %d", ret);
        return ret;
    }

    cts_dev->enabled = true;
    cts_info("Start device successfully");

    return 0;
}

int cts_stop_device(struct cts_device *cts_dev)
{
    struct chipone_ts_data *cts_data =
        container_of(cts_dev, struct chipone_ts_data, cts_dev);
    int ret;

    cts_info("Stop device...");

    if (!cts_is_device_enabled(cts_dev)) {
        cts_warn("Stop device while halted");
        return 0;
    }

    if (cts_is_firmware_updating(cts_dev)) {
        cts_warn("Stop device while firmware updating, please try again");
        return -EAGAIN;
    }

    ret = cts_plat_disable_irq(cts_dev->pdata);
    if (ret < 0) {
        cts_err("Disable IRQ failed %d", ret);
        return ret;
    }

    cts_dev->enabled = false;

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    cts_disable_heartbeat_mechanism(cts_data);
#endif

#ifdef CONFIG_CTS_ESD_PROTECTION
    cts_disable_esd_protection(cts_data);
#endif

#ifdef CONFIG_CTS_CHARGER_DETECT
    cts_stop_charger_detect(cts_data);
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    cts_stop_earjack_detect(cts_data);
#endif

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev))
        cts_exit_glove_mode(cts_dev);
#endif

#ifdef CONFIG_CTS_TP_PROXIMITY
    if (cts_is_proximity_enable(cts_dev)) {
        cts_exit_proximity_mode(cts_dev);
        cts_info("stop device, disable proximity mode.");
    }
#endif

#ifndef CONFIG_GENERIC_HARDIRQS
    if (work_pending(&cts_data->pdata->ts_irq_work)) {
        cts_warn("IRQ work is pending .... flush it");
        flush_work(&cts_data->pdata->ts_irq_work);
    } else
        cts_info("None IRQ work is pending");
#endif
    cts_info("Flush workqueue...");
    if (!cts_dev->rtdata.update_work_run)
        flush_workqueue(cts_data->workqueue);

    ret = cts_plat_release_all_touch(cts_dev->pdata);
    if (ret) {
        cts_err("Release all touch failed %d", ret);
        return ret;
    }
#ifdef CONFIG_CTS_VIRTUALKEY
    ret = cts_plat_release_all_vkey(cts_dev->pdata);
    if (ret) {
        cts_err("Release all vkey failed %d", ret);
        return ret;
    }
#endif

    return 0;
}

#ifdef CONFIG_CTS_ESD_PROTECTION
int cts_start_device_esdrecover(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Start device...");

    if (cts_is_device_enabled(cts_dev)) {
        cts_warn("Start device while already started");
        return 0;
    }

    ret = cts_plat_enable_irq(cts_dev->pdata);
    if (ret < 0) {
        cts_err("Enable IRQ failed %d", ret);
        return ret;
    }

    cts_dev->enabled = true;

    cts_info("Start device successfully");

    return 0;
}

int cts_stop_device_esdrecover(struct cts_device *cts_dev)
{
    struct chipone_ts_data *cts_data =
        container_of(cts_dev, struct chipone_ts_data, cts_dev);
    int ret;

    cts_info("Stop device...");

    if (!cts_is_device_enabled(cts_dev)) {
        cts_warn("Stop device while halted");
        return 0;
    }

    if (cts_is_firmware_updating(cts_dev)) {
        cts_warn("Stop device while firmware updating, please try again");
        return -EAGAIN;
    }

    ret = cts_plat_disable_irq(cts_dev->pdata);
    if (ret < 0) {
        cts_err("Disable IRQ failed %d", ret);
        return ret;
    }

    cts_dev->enabled = false;

    flush_workqueue(cts_data->workqueue);

    ret = cts_plat_release_all_touch(cts_dev->pdata);
    if (ret) {
        cts_err("Release all touch failed %d", ret);
        return ret;
    }
#ifdef CONFIG_CTS_VIRTUALKEY
    ret = cts_plat_release_all_vkey(cts_dev->pdata);
    if (ret) {
        cts_err("Release all vkey failed %d", ret);
        return ret;
    }
#endif /* CONFIG_CTS_VIRTUALKEY */

    return 0;
}
#endif

bool cts_is_fwid_valid(u16 fwid)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (cts_device_hwdatas[i].fwid == fwid)
            return true;
    }

    return false;
}

static bool cts_is_hwid_valid(u32 hwid)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (cts_device_hwdatas[i].hwid == hwid)
            return true;
    }

    return false;
}

int cts_get_hwid(struct cts_device *cts_dev, u32 *hwid)
{
    int ret, i;
    u32 regdef_base[2] = {CTS_REGDEF_BASE0, CTS_REGDEF_BASE1};

    cts_info("Get device hardware id");

    if (cts_dev->hwdata) {
        *hwid = cts_dev->hwdata->hwid;
        return 0;
    }

    cts_info("Device hardware data not initialized, try to read from register");

    if (cts_dev->rtdata.boot_mode != CTS_BOOT_MODE_PROGRAM) {
        ret = cts_enter_program_mode(cts_dev);
        if (ret) {
            cts_err("Enter program mode failed %d", ret);
            goto err_out;
        }
    }

    for(i = 0; i < ARRAY_SIZE(regdef_base); i++) {
        ret = cts_read_try_protocols(cts_dev,
                regdef_base[i] + CTS_DEV_HW_REG_HARDWARE_ID,
                (u8 *)hwid, 4);
        if (ret)
            continue;

        *hwid = le32_to_cpu(*hwid);
        *hwid &= 0XFFFFFF00;
        if (!cts_is_hwid_valid(*hwid)) {
            ret = -EINVAL;
            continue;
        }
        break;
    }

    cts_info("Device hardware id: %04x", *hwid);
    return ret;

err_out:
    *hwid = CTS_DEV_HWID_INVALID;
    return ret;
}

int cts_probe_device(struct cts_device *cts_dev)
{
    int ret, i = 0;
    u16 fwid = CTS_DEV_FWID_INVALID;
    u32 hwid = CTS_DEV_HWID_INVALID;

    cts_dev->fwdata.version = 0;

    cts_info("Probe device");

    cts_init_rtdata(&cts_dev->rtdata);

    cts_reset_device(cts_dev);
    mdelay(80);

    if (!cts_plat_is_normal_mode(cts_dev->pdata)) {
        cts_warn("device is not in sram-boot!!\n");
    } else {
        ret = cts_tcs_get_fw_id(cts_dev, &fwid);
        if (ret) {
            cts_err("Get firmware id failed %d", ret);
        } else {
            cts_info("Firmware id: 0x%04x", fwid);
            ret = cts_tcs_get_fw_ver(cts_dev, &cts_dev->fwdata.version);
            if (ret) {
                cts_err("Read firmware version failed %d", ret);
                cts_dev->fwdata.version = 0;
            } else {
                cts_info("Firmware version: 0x%04x", cts_dev->fwdata.version);
            }
            goto init_hwdata;
        }
    }

    for(i = 0; i < 3; i++) {
        ret = cts_get_hwid(cts_dev, &hwid);
        if (ret || hwid == CTS_DEV_HWID_INVALID) {
            cts_err("Get hardware id failed, ret: %d", ret);
            if(i == 2) {
                cts_deinit_rtdata(&cts_dev->rtdata);
                return ret;
            }
            cts_reset_device(cts_dev);
        } else {
                break;
        }
    }

init_hwdata:
    ret = cts_init_hwdata(cts_dev, hwid, fwid);
    if (ret) {
        cts_err("Device hwid: %06x fwid: %04x not found", hwid, fwid);
        cts_deinit_rtdata(&cts_dev->rtdata);
        return -ENODEV;
    }
    ret = cts_pre_init_fwdata(cts_dev, &cts_dev->fwdata);
    if (ret) {
        cts_err("Device hwid: %06x fwid: %04x not found", hwid, fwid);
        cts_deinit_rtdata(&cts_dev->rtdata);
        cts_deinit_hwdata(cts_dev);
        return -ENODEV;
    }

    if (cts_dev->hwdata->has_flash) {
        ret = probe_flash(cts_dev);
        if (ret)
            cts_err("flash is configured but probe failed!!!\n");
    }

    return ret;
}

void cts_deinit_device(struct cts_device *cts_dev)
{
    cts_deinit_rtdata(&cts_dev->rtdata);
    cts_deinit_hwdata(cts_dev);
    cts_deinit_fwdata(cts_dev);
}

#ifdef CFG_CTS_GESTURE
void cts_enable_gesture_wakeup(struct cts_device *cts_dev)
{
    cts_info("Enable gesture wakeup");
    cts_dev->rtdata.gesture_wakeup_enabled = true;
}

void cts_disable_gesture_wakeup(struct cts_device *cts_dev)
{
    cts_info("Disable gesture wakeup");
    cts_dev->rtdata.gesture_wakeup_enabled = false;
}

bool cts_is_gesture_wakeup_enabled(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.gesture_wakeup_enabled;
}

int cts_get_gesture_info(struct cts_device *cts_dev, void *gesture_info)
{
    int ret;

    cts_info("Get gesture info");

    if (cts_dev->rtdata.boot_mode == CTS_BOOT_MODE_PROGRAM) {
        cts_warn("Get gesture info in program mode");
        return -ENODEV;
    }

    if (!cts_dev->rtdata.suspended) {
        cts_warn("Get gesture info while not suspended");
        return -ENODEV;
    }

    if (!cts_dev->rtdata.gesture_wakeup_enabled) {
        cts_warn("Get gesture info while gesture wakeup not enabled");
        return -ENODEV;
    }

    ret = cts_tcs_get_gestureinfo(cts_dev, gesture_info);
    if (ret) {
        cts_err("Read gesture info header failed %d", ret);
        return ret;
    }

    return 0;
}
#endif /* CFG_CTS_GESTURE */

int cts_set_int_data_types(struct cts_device *cts_dev, u16 types)
{
    int ret;
    u16 realtypes = types & INT_DATA_TYPE_MASK;

    cts_info("Set int data types: %#06x, mask to %#06x", types, realtypes);

    ret = cts_tcs_set_int_data_types(cts_dev, realtypes);
    if (ret) {
        cts_err("Set int data type failed: %d", ret);
        return -EIO;
    }
    cts_dev->fwdata.int_data_types = realtypes;

    cts_tcs_calc_int_data_size(cts_dev);

    return ret;
}

int cts_set_int_data_method(struct cts_device *cts_dev, u8 method)
{
    int ret = 0;

    cts_info("Set int data method: %d", method);

    if (method >= INT_DATA_METHOD_CNT) {
        cts_err("Invalid int data method");
        return -EINVAL;
    }

    ret = cts_tcs_set_int_data_method(cts_dev, method);
    if (ret) {
        cts_err("Set int data method failed: %d", ret);
        return -EIO;
    }
    cts_dev->fwdata.int_data_method = method;

    cts_tcs_calc_int_data_size(cts_dev);

    return ret;
}

#ifdef CONFIG_CTS_ESD_PROTECTION
static void cts_esd_protection_work(struct work_struct *work)
{
    struct chipone_ts_data *cts_data;
    struct cts_firmware *firmware = NULL;
    struct cts_device *cts_dev;
    int ret = 0;

    cts_dbg("ESD protection work");
    cts_data = container_of(work, struct chipone_ts_data, esd_work.work);
    cts_dev = &cts_data->cts_dev;

    cts_lock_device(cts_dev);
    if (!cts_plat_is_normal_mode(cts_data->pdata)) {
        cts_data->esd_check_fail_cnt++;
        /*reset chip next time */
        if ((cts_data->esd_check_fail_cnt % 2) == 0) {
            cts_err("ESD protection read normal mode failed, reset chip!");
            ret = cts_reset_device(cts_dev);
            if (ret)
                cts_err("ESD protection reset chip failed %d", ret);
        }
    } else {
        cts_data->esd_check_fail_cnt = 0;
    }

    if (cts_data->esd_check_fail_cnt >= CFG_CTS_ESD_FAILED_CONFIRM_CNT) {
        cts_warn("ESD protection check failed, update firmware!!!");
        cts_stop_device_esdrecover(cts_dev);
        ret = cts_request_firmware(cts_dev, &firmware,
            cts_data->cts_dev.hwdata->hwid, cts_data->cts_dev.hwdata->fwid, 0);
        if (firmware) {
            ret = cts_update_firmware(cts_dev, firmware, CTS_SPLIT_BIN_NORMAL);
            cts_release_firmware(cts_dev, firmware);

            if (ret)
                cts_err("Update default firmware failed %d", ret);
        } else
            cts_err("Request default firmware failed %d, "
                "please update manually!!", ret);

        cts_start_device_esdrecover(cts_dev);
        cts_data->esd_check_fail_cnt = 0;
    }
    queue_delayed_work(cts_data->esd_workqueue, &cts_data->esd_work,
        CFG_CTS_ESD_PROTECTION_CHECK_PERIOD);

    cts_unlock_device(cts_dev);
}

void cts_enable_esd_protection(struct chipone_ts_data *cts_data)
{
    if (cts_data->esd_workqueue && !cts_data->esd_enabled) {
        cts_info("ESD protection enable");

        cts_data->esd_enabled = true;
        cts_data->esd_check_fail_cnt = 0;
        queue_delayed_work(cts_data->esd_workqueue,
                &cts_data->esd_work,
                CFG_CTS_ESD_PROTECTION_CHECK_PERIOD);
    }
}

void cts_disable_esd_protection(struct chipone_ts_data *cts_data)
{
    if (cts_data->esd_workqueue && cts_data->esd_enabled) {
        cts_info("ESD protection disable");

        cts_data->esd_enabled = false;
        cancel_delayed_work(&cts_data->esd_work);
        flush_workqueue(cts_data->esd_workqueue);
    }
}

void cts_init_esd_protection(struct chipone_ts_data *cts_data)
{
    cts_info("Init ESD protection");

    INIT_DELAYED_WORK(&cts_data->esd_work, cts_esd_protection_work);

    cts_data->esd_enabled = false;
    cts_data->esd_check_fail_cnt = 0;
}

void cts_deinit_esd_protection(struct chipone_ts_data *cts_data)
{
    cts_info("De-Init ESD protection");

    if (cts_data->esd_workqueue && cts_data->esd_enabled) {
        cts_data->esd_enabled = false;
        cancel_delayed_work(&cts_data->esd_work);
    }
}
#endif /* CONFIG_CTS_ESD_PROTECTION */


#ifdef CFG_CTS_HEARTBEAT_MECHANISM
void cts_show_touch_debug_msg(void *debug)
{
    struct cts_touch_debug_msg *msg = (struct cts_touch_debug_msg *)debug;
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &msg->firmware_status;

    cts_info("FW_Ver      : 0x%02X", msg->fw_version);
    cts_info("Reset_Flag  : 0x%08X", msg->reset_flag);

    cts_info("Charger     : %d", status->charger);
    cts_info("Proxmty     : %d", status->proximity);
    cts_info("Earjack     : %d", status->earjack);
    cts_info("Knuckle     : %d", status->knuckle);
    cts_info("Glove       : %d", status->glove);
    cts_info("Pocket      : %d", status->pocket);
    cts_info("Game        : %d", status->game);

    cts_info("High_Temp   : %d", status->high_temperature);
    cts_info("Low_Temp    : %d", status->low_temperature);
    cts_info("Bend        : %d", status->bend_mode);
    cts_info("Palm        : %d", status->palm_status);
    cts_info("Noise       : %d", status->noise_mode);
    cts_info("Base_Trace  : %d", status->base_trace);
    cts_info("Water       : %d", status->water_mode);
    cts_info("Ground      : %d", status->ground);

    cts_info("Proxmity_Sts: 0x%02x", msg->proximity);

    cts_info("Work_Mode   : %d", msg->work_mode);
    cts_info("Power_Mode  : %d", msg->power_mode);
    cts_info("Freq        : %d", msg->curr_freq);
    cts_info("Esd_0a      : %d", msg->esd_0a_status);
    cts_info("Fw_Esd      : %d", msg->fw_esd_status);
    cts_info("Landscape   : %d", msg->landscape_mode);

    cts_info("Freq_Noise  : %d", msg->curr_freq_noise);
    cts_info("Max_Diff    : %d", msg->max_diff);
    cts_info("Row         : %d", msg->max_diff_row);
    cts_info("Col         : %d", msg->max_diff_col);
    cts_info("Max_Neg     : -%d", msg->max_neg_diff);
    cts_info("Neg_Row     : %d", msg->max_neg_diff_row);
    cts_info("Neg_Col     : %d", msg->max_neg_diff_col);
}

void cts_heartbeat_mechanism_work(struct work_struct *work)
{
    struct chipone_ts_data *cts_data;
    struct cts_device *cts_dev;
    struct cts_platform_data *pdata;
    struct cts_touch_info_legacy info;
    struct cts_firmware *firmware = NULL;
    u8 curr_status;
    int ret = 0;

    cts_data = container_of(work, struct chipone_ts_data, heart_work.work);
    cts_dev = &cts_data->cts_dev;
    pdata = cts_data->pdata;

    cts_dbg("heartbeat mechanism work");

    cts_lock_device(cts_dev);
    ret = cts_tcs_get_touch_status(cts_dev);
    if (ret) {
        cts_err("get touch status failed");
        cts_reset_device(cts_dev);

        if (++cts_dev->rtdata.esd_count < 3)
            goto end;

        ret = cts_request_firmware(cts_dev, &firmware, cts_dev->hwdata->hwid,
                cts_dev->hwdata->fwid, 0);
        if (firmware) {
            ret = cts_update_firmware(&cts_data->cts_dev, firmware, CTS_SPLIT_BIN_NORMAL);
            if (ret)
                cts_err("Update default firmware failed %d", ret);
            cts_release_firmware(cts_dev, firmware);
        }
        cts_tcs_reinit_fw_status(cts_dev);
        goto end;
    }

    memcpy(&info, pdata->int_data, sizeof(info));

    curr_status = info.debug_msg.firmware_status;

    cts_dev->rtdata.esd_count = 0;

    /* 9922C reset_flag:0xFFFFFF; 9922 9916 9916C reset_flag:0xFFFF */
    if ((info.debug_msg.reset_flag & 0xFFFFFF) != 0xFFFFFF
        || (info.debug_msg.reset_flag & 0xFFFF) != 0xFFFF) {
        cts_err("heartbeat reset flag:0x%08x error", info.debug_msg.reset_flag);
        cts_show_touch_debug_msg(&info.debug_msg);
    }

    if (curr_status != cts_dev->rtdata.firmware_status) {
        cts_err("firmware status error");
        cts_show_touch_debug_msg(&info.debug_msg);
        cts_tcs_reinit_fw_status(cts_dev);
    }

end:
    queue_delayed_work(cts_data->heart_workqueue,
            &cts_data->heart_work, msecs_to_jiffies(2000));
    cts_unlock_device(cts_dev);
}
void cts_enable_heartbeat_mechanism(struct chipone_ts_data *cts_data)
{
    queue_delayed_work(cts_data->heart_workqueue,
            &cts_data->heart_work, msecs_to_jiffies(2000));
}
void cts_disable_heartbeat_mechanism(struct chipone_ts_data *cts_data)
{
    cancel_delayed_work_sync(&cts_data->heart_work);
}
#endif

#ifdef CONFIG_CTS_GLOVE
int cts_enter_glove_mode(const struct cts_device *cts_dev)
{
    int ret;

    cts_info("Enter glove mode");

    ret = cts_tcs_set_glove_mode(cts_dev, 1);
    if (ret)
        cts_err("Enable Glove mode err");
    else
        cts_dev->rtdata.glove_mode_enabled = true;

    return ret;
}

int cts_exit_glove_mode(const struct cts_device *cts_dev)
{
    cts_info("Exit glove mode");

    ret = cts_tcs_set_glove_mode(cts_dev, 0);
    if (ret)
        cts_err("Exit Glove mode err");
    else
        cts_dev->rtdata.glove_mode_enabled = false;

    return ret;
}

int cts_is_glove_enabled(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.glove_mode_enabled;
}

#endif /* CONFIG_CTS_GLOVE */

#ifdef CONFIG_CTS_CHARGER_DETECT
bool cts_is_charger_exist(struct cts_device *cts_dev)
{
    struct chipone_ts_data *cts_data;
    bool attached = false;
    int ret;

    cts_data = container_of(cts_dev, struct chipone_ts_data, cts_dev);

    ret = cts_is_charger_attached(cts_data, &attached);
    if (ret)
        cts_err("Get charger state failed %d", ret);

    cts_dev->rtdata.charger_exist = attached;

    return attached;
}

int cts_set_dev_charger_attached(struct cts_device *cts_dev, bool attached)
{
    int ret;
    u8 buf;

    cts_info("Set dev charger %s", attached ? "ATTACHED" : "DETATCHED");
    buf = attached ? 1 : 0;

    ret = cts_tcs_set_charger_plug(cts_dev, buf);
    if (ret)
        cts_err("Set failed %d", ret);

    return ret;
}
#endif /* CONFIG_CTS_CHARGER_DETECT */

#ifdef CONFIG_CTS_CHARGER_DETECT_FOR_TRANSSION
int cts_set_dev_charger_attached_for_transsion(struct cts_device *cts_dev, bool attached)
{
    int ret;
    u8 buf;

    cts_info("Set dev charger %s", attached ? "ATTACHED" : "DETATCHED");
    buf = attached ? 1 : 0;

    ret = cts_tcs_set_charger_plug(cts_dev, buf);
    if (ret)
        cts_err("Set failed %d", ret);

    return ret;
}
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
bool cts_is_earjack_exist(struct cts_device *cts_dev)
{
    struct chipone_ts_data *cts_data;
    bool attached = false;
    int  ret;

    cts_data = container_of(cts_dev, struct chipone_ts_data, cts_dev);

    ret = cts_is_earjack_attached(cts_data, &attached);
    if (ret) {
        cts_err("Get earjack state failed %d", ret);
    }

    return attached;
}

int cts_set_dev_earjack_attached(struct cts_device *cts_dev, bool attached)
{
    int ret;
    u8 buf;

    cts_info("Set dev earjack %s", attached ? "ATTACHED" : "DETATCHED");
    buf = attached ? 1 : 0;
    ret = cts_tcs_set_earjack_plug(cts_dev, buf);
    if (ret)
        cts_err("Set failed %d", ret);

    return ret;
}
#endif /* CONFIG_CTS_EARJACK_DETECT */


void cts_firmware_upgrade_work(struct work_struct *work)
{
    struct chipone_ts_data *cts_data;
    struct cts_device *cts_dev;
    struct cts_firmware *firmware = NULL;
    int retries;
    int ret = 0;

    cts_info("Firmware upgrade work");

    cts_data = container_of(work, struct chipone_ts_data, fw_upgrade_work.work);
    cts_dev = &cts_data->cts_dev;
    cts_lock_device(cts_dev);
    cts_dev->rtdata.update_work_run = true;

    ret = cts_request_firmware(cts_dev, &firmware, cts_dev->hwdata->hwid,
            CTS_DEV_FWID_ANY, cts_dev->fwdata.version);
    if (ret) {
        if (cts_dev->hwdata->i2c_protocol == CTS_I2C_PRW ||
            cts_dev->hwdata->spi_protocol == CTS_SPI_PRW)
            cts_enter_program_mode(cts_dev);

        /* With flash, do not update. */
        ret = cts_enter_normal_mode(cts_dev);
        goto end;
    }

    retries = 0;
    do {
        ret = cts_update_firmware(cts_dev, firmware, CTS_SPLIT_BIN_NORMAL);
        if (ret) {
            cts_err("Update firmware failed times: %d", retries);
            cts_reset_device(cts_dev);
        } else
            break;
    } while (++retries < 3);

    cts_release_firmware(cts_dev, firmware);

end:
    if (ret == 0)
        cts_start_device(cts_dev);

    cts_dev->rtdata.update_work_run = false;
    cts_unlock_device(cts_dev);
}

int cts_reset_device(struct cts_device *cts_dev)
{
#ifdef CFG_CTS_HAS_RESET_PIN
    cts_info("reset by pull HW pin\n");
    return cts_plat_reset_device(cts_dev->pdata);
#else
    cts_info("reset by sending cmd\n");
    if (!cts_dev->hwdata)
        return cts_plat_reset_device(cts_dev->pdata);
    return cts_tcs_reset_device(cts_dev);
#endif

}

static struct file *cts_log_filp;
static int cts_log_to_file_level;
extern int cts_write_file(struct file *filp, const void *data, size_t size);

static char *cts_log_buffer;
static int cts_log_buf_size;
static int cts_log_buf_wr_size;

static bool cts_log_redirect;

extern int cts_mkdir_for_file(const char *filepath, umode_t mode);

int cts_start_driver_log_redirect(const char *filepath, bool append_to_file,
    char *log_buffer, int log_buf_size, int log_level)
{
#define START_BANNER \
    ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"

    int ret = 0;

    cts_info("Start driver log redirect");

    cts_log_to_file_level = log_level;

    if (log_buffer && log_buf_size) {
        cts_info(" - Start driver log to buffer: %p size: %d level: %d",
            log_buffer, log_buf_size, log_level);
        cts_log_buffer = log_buffer;
        cts_log_buf_size = log_buf_size;
        cts_log_buf_wr_size = 0;
    }

    if (filepath && filepath[0]) {
        cts_info(" - Start driver log to file  : '%s' level: %d",
            filepath, log_level);
#ifdef CFG_CTS_FOR_GKI
        cts_info("%s(): filp_open is forbiddon with GKI Version!", __func__);
#else
        cts_log_filp = filp_open(filepath,
            O_WRONLY | O_CREAT | (append_to_file ? O_APPEND : O_TRUNC),
            S_IRUGO | S_IWUGO);
        if (IS_ERR(cts_log_filp)) {
            ret = PTR_ERR(cts_log_filp);
            cts_log_filp = NULL;
            cts_err("Open file '%s' for driver log failed %d",
                filepath, ret);
        } else {
            cts_write_file(cts_log_filp, START_BANNER, strlen(START_BANNER));
        }
#endif
    }

    cts_log_redirect = true;

    return ret;
#undef START_BANNER
}

void cts_stop_driver_log_redirect(void)
{
#define END_BANNER \
    "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
#ifndef CFG_CTS_FOR_GKI
    int ret;
#endif

    cts_log_redirect = false;

    cts_info("Stop driver log redirect");

    if (cts_log_filp) {
        cts_info(" - Stop driver log to file");

        cts_write_file(cts_log_filp, END_BANNER, strlen(END_BANNER));
#ifndef CFG_CTS_FOR_GKI
        ret = filp_close(cts_log_filp, NULL);
        if (ret) {
            cts_err("Close driver log file failed %d", ret);
        }
#endif
        cts_log_filp = NULL;
    }

    if (cts_log_buffer) {
        cts_info(" - Stop driver log to buffer");

        cts_log_buffer = NULL;
        cts_log_buf_size = 0;
        cts_log_buf_wr_size = 0;
    }

#undef END_BANNER
}

int cts_get_driver_log_redirect_size(void)
{
    if (cts_log_redirect && cts_log_buffer && cts_log_buf_wr_size) {
        return cts_log_buf_wr_size;
    } else {
        return 0;
    }
}

void cts_log(int level, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    if (cts_log_redirect) {
        if (cts_log_buffer &&
            cts_log_buf_wr_size < cts_log_buf_size &&
            level <= cts_log_to_file_level) {
            cts_log_buf_wr_size += vscnprintf(cts_log_buffer + cts_log_buf_wr_size,
                cts_log_buf_size - cts_log_buf_wr_size, fmt, args);
        }

        if (cts_log_filp != NULL && level <= cts_log_to_file_level) {
            char buf[512];
            int count = vscnprintf(buf, sizeof(buf), fmt, args);

            cts_write_file(cts_log_filp, buf, count);
        }
    }

    if (level < CTS_DRIVER_LOG_DEBUG || cts_show_debug_log) {
        vprintk(fmt, args);
    }

    va_end(args);
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
/* From lib/kstrtox.c */
/**
 * kstrtobool - convert common user inputs into boolean values
 * @s: input string
 * @res: result
 *
 * This routine returns 0 iff the first character is one of 'Yy1Nn0', or
 * [oO][NnFf] for "on" and "off". Otherwise it will return -EINVAL.  Value
 * pointed to by res is updated upon finding a match.
 */
int kstrtobool(const char *s, bool *res)
{
    if (!s)
        return -EINVAL;

    switch (s[0]) {
    case 'y':
    case 'Y':
    case '1':
        *res = true;
        return 0;
    case 'n':
    case 'N':
    case '0':
        *res = false;
        return 0;
    case 'o':
    case 'O':
        switch (s[1]) {
        case 'n':
        case 'N':
            *res = true;
            return 0;
        case 'f':
        case 'F':
            *res = false;
            return 0;
        default:
            break;
        }
    default:
        break;
    }

    return -EINVAL;
}
#endif
