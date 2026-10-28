#define LOG_TAG         "Oem"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_oem.h"
#include "cts_test.h"
#include "cts_tcs.h"
#include "cts_firmware.h"

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#define OEM_TEST_RESET_PIN_MASK   BIT(0)
#define OEM_TEST_INT_PIN_MASK     BIT(1)
#define OEM_TEST_RAWDATA_MASK     BIT(2)
#define OEM_TEST_NOISE_MASK       BIT(3)
#define OEM_TEST_OPEN_MASK        BIT(4)
#define OEM_TEST_SHORT_MASK       BIT(5)
#define OEM_TEST_COMP_CAP_MASK    BIT(6)
#define OEM_TEST_FULL_TEST        0xFFFF

/* Default settings if not configured in device tree */
#define OEM_OF_DEF_RAWDATA_FRAMES   1//2
#define OEM_OF_DEF_RAWDATA_MIN      1050//1200
#define OEM_OF_DEF_RAWDATA_MAX      3100//2800
#define OEM_OF_DEF_NOISE_FRAMES     16//10
#define OEM_OF_DEF_NOISE_MAX        110//80
#define OEM_OF_DEF_OPEN_MIN         1200//1980
#define OEM_OF_DEF_SHORT_MIN        1600//500
#define OEM_OF_DEF_COMP_CAP_MIN     1
#define OEM_OF_DEF_COMP_CAP_MAX     127


#define OEM_OF_PROPNAME_PREFIX              "chipone," //modify

#define OEM_OF_PROPNAME_TEST_RESET_PIN      OEM_OF_PROPNAME_PREFIX"test-reset-pin"
#define OEM_OF_PROPNAME_TEST_INT_PIN        OEM_OF_PROPNAME_PREFIX"test-int-pin"
#define OEM_OF_PROPNAME_TEST_RAWDATA        OEM_OF_PROPNAME_PREFIX"test-rawdata"
#define OEM_OF_PROPNAME_RAWDATA_FRAMES      OEM_OF_PROPNAME_PREFIX"test-rawdata-frames"
#define OEM_OF_PROPNAME_RAWDATA_MIN         OEM_OF_PROPNAME_PREFIX"rawdata-min"
#define OEM_OF_PROPNAME_RAWDATA_MAX         OEM_OF_PROPNAME_PREFIX"rawdata-max"
#define OEM_OF_PROPNAME_TEST_NOISE          OEM_OF_PROPNAME_PREFIX"test-noise"
#define OEM_OF_PROPNAME_NOISE_FRAMES        OEM_OF_PROPNAME_PREFIX"test-noise-frames"
#define OEM_OF_PROPNAME_NOISE_MAX           OEM_OF_PROPNAME_PREFIX"noise-max"
#define OEM_OF_PROPNAME_TEST_OPEN           OEM_OF_PROPNAME_PREFIX"test-open"
#define OEM_OF_PROPNAME_OPEN_MIN            OEM_OF_PROPNAME_PREFIX"open-min"
#define OEM_OF_PROPNAME_TEST_SHORT          OEM_OF_PROPNAME_PREFIX"test-short"
#define OEM_OF_PROPNAME_SHORT_MIN           OEM_OF_PROPNAME_PREFIX"short-min"
#define OEM_OF_PROPNAME_TEST_COMP_CAP       OEM_OF_PROPNAME_PREFIX"test-compensate-cap"
#define OEM_OF_PROPNAME_COMP_CAP_MIN        OEM_OF_PROPNAME_PREFIX"compensate-cap-min"
#define OEM_OF_PROPNAME_COMP_CAP_MAX        OEM_OF_PROPNAME_PREFIX"compensate-cap-max"

#define OEM_SELFTEST_PROC_FILENAME          "cts_selftest"
#define OEM_LIMIT_PROC_FILENAME             "cts_limit"
#define OEM_FACTORY_TEST_PROC_FILENAME      "cts_factory_test"
#define OEM_RAWDATA_PROC_FILENAME           "cts_rawdata"
#define OEM_MANUAL_PROC_FILENAME            "cts_manual"
#define OEM_DIFFDATA_PROC_FILENAME          "cts_diffdata"

//BSP: modify for midtest by jiangzhu.yang 20230719 start
static int32_t TestResult_RESET = 0;
static int32_t TestResult_INT = 0;
static int32_t TestResult_FW_Rawdata = 0;
static int32_t TestResult_Noise = 0;
static int32_t TestResult_Open = 0;
static int32_t TestResult_Short = 0;
static int32_t TestResult_CC = 0;
//BSP: modify for midtest by jiangzhu.yang 20230719 end

#pragma pack(1)
struct cts_limit {
    u8 header[7];
    u8 version;
    struct {
        unsigned int int_item : 1;
        unsigned int reset_item : 1;
        unsigned int normal_rawdata_item : 1;
        unsigned int normal_noise_item : 1;
        unsigned int open_item : 1;
        unsigned int short_item : 1;
        unsigned int comp_cap_item : 1;
        unsigned int gstr_rawdata_item : 1;
        unsigned int gstrlp_rawdata_item : 1;
        unsigned int gstr_noise_item : 1;
        unsigned int gstrlp_noise_item : 1;
        unsigned int type : 5;
    };
    int normal_rawdata_frames;
    int normal_rawdata_min;
    int normal_rawdata_max;
    int normal_noise_frames;
    int normal_noise_max;
    int normal_open_min;
    int normal_short_min;
    int normal_compcap_min;
    int normal_compcap_max;
    int gstr_rawdata_frames;
    int gstr_rawdata_min;
    int gstr_rawdata_max;
    int gstrlp_rawdata_frames;
    int gstrlp_rawdata_min;
    int gstrlp_rawdata_max;
    int gstr_noise_frames;
    int gstr_noise_max;
    int gstrlp_noise_frames;
    int gstrlp_noise_max;
};
#pragma pack()

struct cts_test_case {
    int test_result;
    u32 sample_frames;
    u16 *test_data_buf;
    int data_buff_size;
    int data_size;
    int threshold_max;
    int threshold_min;
    char data_file_path[PATH_MAX];
};

struct cts_oem_data {
    struct proc_dir_entry *selftest_proc_entry;
    struct proc_dir_entry *limit_entry;
    struct proc_dir_entry *factory_test_entry;
    struct proc_dir_entry *manual_proc_entry;
    struct proc_dir_entry *diffdata_proc_entry;
    struct proc_dir_entry *rawdata_proc_entry;

    bool need_parse_test_config_from_dt;
    int test_cases;

    /* Test configuration from device tree or limit.bin */
    struct cts_test_case test_reset_pin;
    struct cts_test_case test_int_pin;
    struct cts_test_case test_rawdata;
    struct cts_test_case test_noise;
    struct cts_test_case test_open;
    struct cts_test_case test_short;
    struct cts_test_case test_comp_cap;

    struct chipone_ts_data *cts_data;
};

struct cts_oem_data cts_oem_data;

struct proc_dir_entry *cts_tp_work_proc;

#define ALLOC_TEST_DATA_MEM(type, size, mask) \
    do { \
        if (oem_data->test_cases & mask) { \
            if (oem_data->test_##type.test_data_buf == NULL) { \
                cts_info(" - Alloc " #type " test data mem size %d", size); \
                oem_data->test_##type.test_data_buf = vmalloc(size); \
                if (oem_data->test_##type.test_data_buf == NULL) { \
                    cts_err("Alloc " #type " test data mem failed"); \
                    return -ENOMEM; \
                } \
                oem_data->test_##type.data_buff_size = size; \
            } \
            memset(oem_data->test_##type.test_data_buf, 0, size); \
        } \
    } while (0)

/* NOTE: Any test data mem alloc failed will NOT clean other mem */
static int alloc_sleftest_data_mem(struct cts_oem_data *oem_data, int nodes)
{
    cts_info("Alloc selftest data");

    ALLOC_TEST_DATA_MEM(rawdata,
                        nodes * 2 * oem_data->test_rawdata.sample_frames, OEM_TEST_RAWDATA_MASK);
    ALLOC_TEST_DATA_MEM(noise,
                        nodes * 2 * (oem_data->test_noise.sample_frames + 3), OEM_TEST_NOISE_MASK);
    ALLOC_TEST_DATA_MEM(open, nodes * 2, OEM_TEST_OPEN_MASK);
    ALLOC_TEST_DATA_MEM(short, nodes * 2 * 10, OEM_TEST_SHORT_MASK);
    ALLOC_TEST_DATA_MEM(comp_cap, nodes, OEM_TEST_COMP_CAP_MASK);

    return 0;
}
#undef ALLOC_TEST_DATA_MEM

#define FREE_TEST_DATA_MEM(type) \
    do { \
        if (oem_data->test_##type.test_data_buf) { \
            cts_info("- Free " #type " test data mem"); \
            vfree(oem_data->test_##type.test_data_buf); \
            oem_data->test_##type.test_data_buf = NULL; \
            oem_data->test_##type.data_buff_size = 0; \
        } \
    } while(0)

static void free_selftest_data_mem(struct cts_oem_data *oem_data)
{
    cts_info("Free selftest data");

    FREE_TEST_DATA_MEM(rawdata);
    FREE_TEST_DATA_MEM(noise);
    FREE_TEST_DATA_MEM(open);
    FREE_TEST_DATA_MEM(short);
    FREE_TEST_DATA_MEM(comp_cap);
}
#undef FREE_TEST_DATA_MEM

static void parse_selftest_dt(struct cts_oem_data *oem_data,
                             struct device_node *np)
{
    cts_info("Parse selftest dt");

    if (of_property_read_bool(np, OEM_OF_PROPNAME_TEST_RESET_PIN))
        oem_data->test_cases |= OEM_TEST_RESET_PIN_MASK;

    if (of_property_read_bool(np, OEM_OF_PROPNAME_TEST_INT_PIN))
        oem_data->test_cases |= OEM_TEST_INT_PIN_MASK;

    oem_data->test_rawdata.sample_frames = OEM_OF_DEF_RAWDATA_FRAMES;
    oem_data->test_rawdata.threshold_min = OEM_OF_DEF_RAWDATA_MIN;
    oem_data->test_rawdata.threshold_max = OEM_OF_DEF_RAWDATA_MAX;
    strncpy(oem_data->test_rawdata.data_file_path,
            CTS_RAWDATA_TEST_DATA_FILEPATH, strlen(CTS_RAWDATA_TEST_DATA_FILEPATH));
    if (of_property_read_bool(np, OEM_OF_PROPNAME_TEST_RAWDATA)) {
         oem_data->test_cases |= OEM_TEST_RAWDATA_MASK;

    if (of_property_read_u32(np, OEM_OF_PROPNAME_RAWDATA_FRAMES,
          (u32 *)&oem_data->test_rawdata.sample_frames))

    cts_warn("Parse '"OEM_OF_PROPNAME_RAWDATA_FRAMES"' failed");
    if (of_property_read_u32(np, OEM_OF_PROPNAME_RAWDATA_MIN,
               (u32 *)&oem_data->test_rawdata.threshold_min))
        cts_warn("Parse '"OEM_OF_PROPNAME_RAWDATA_MIN"' failed");

    if (of_property_read_u32(np, OEM_OF_PROPNAME_RAWDATA_MAX,
         (u32 *)&oem_data->test_rawdata.threshold_max))
         cts_warn("Parse '"OEM_OF_PROPNAME_RAWDATA_MAX"' failed");
    }

     oem_data->test_noise.sample_frames = OEM_OF_DEF_NOISE_FRAMES;
      oem_data->test_noise.threshold_max = OEM_OF_DEF_NOISE_MAX;
      strncpy(oem_data->test_noise.data_file_path,
           CTS_NOISE_TEST_DATA_FILEPATH, strlen(CTS_NOISE_TEST_DATA_FILEPATH));
     if (of_property_read_bool(np, OEM_OF_PROPNAME_TEST_NOISE)) {
           oem_data->test_cases |= OEM_TEST_NOISE_MASK;
          if (of_property_read_u32(np, OEM_OF_PROPNAME_NOISE_FRAMES,
                (u32 *)&oem_data->test_noise.sample_frames))
              cts_warn("Parse '"OEM_OF_PROPNAME_NOISE_FRAMES"' failed");

          if (of_property_read_u32(np, OEM_OF_PROPNAME_NOISE_MAX,
                (u32 *)&oem_data->test_noise.threshold_max))
              cts_warn("Parse '"OEM_OF_PROPNAME_NOISE_MAX"' failed");
     }

     oem_data->test_open.threshold_min = OEM_OF_DEF_OPEN_MIN;
     strncpy(oem_data->test_open.data_file_path,
            CTS_OPEN_TEST_DATA_FILEPATH, strlen(CTS_OPEN_TEST_DATA_FILEPATH));
      if (of_property_read_bool(np, OEM_OF_PROPNAME_TEST_OPEN)) {
          oem_data->test_cases |=  OEM_TEST_OPEN_MASK;
          if (of_property_read_u32(np, OEM_OF_PROPNAME_OPEN_MIN,
               (u32 *)&oem_data->test_open.threshold_min))
               cts_warn("Parse '"OEM_OF_PROPNAME_OPEN_MIN"' failed");
      }

     oem_data->test_short.threshold_min = OEM_OF_DEF_SHORT_MIN;
     strncpy(oem_data->test_short.data_file_path,
      CTS_SHORT_TEST_DATA_FILEPATH, strlen(CTS_SHORT_TEST_DATA_FILEPATH));
     if (of_property_read_bool(np, OEM_OF_PROPNAME_TEST_SHORT)) {
         oem_data->test_cases |= OEM_TEST_SHORT_MASK;
         if (of_property_read_u32(np, OEM_OF_PROPNAME_SHORT_MIN,
             (u32 *)&oem_data->test_short.threshold_min))
           cts_warn("Parse '"OEM_OF_PROPNAME_SHORT_MIN"' failed");
          }

           oem_data->test_comp_cap.threshold_min = OEM_OF_DEF_COMP_CAP_MIN;
           oem_data->test_comp_cap.threshold_max = OEM_OF_DEF_COMP_CAP_MAX;
           strncpy(oem_data->test_comp_cap.data_file_path,
                CTS_COMP_CAP_TEST_DATA_FILEPATH, strlen(CTS_COMP_CAP_TEST_DATA_FILEPATH));
           if (of_property_read_bool(np, OEM_OF_PROPNAME_TEST_COMP_CAP)) {
                oem_data->test_cases |= OEM_TEST_COMP_CAP_MASK;
           if (of_property_read_u32(np, OEM_OF_PROPNAME_COMP_CAP_MIN,
                (u32 *)&oem_data->test_comp_cap.threshold_min))
               cts_warn("Parse '"OEM_OF_PROPNAME_COMP_CAP_MIN"' failed");

           if (of_property_read_u32(np, OEM_OF_PROPNAME_COMP_CAP_MAX,
                (u32 *)&oem_data->test_comp_cap.threshold_max))
                cts_warn("Parse '"OEM_OF_PROPNAME_COMP_CAP_MAX"' failed");
      }

      if(!oem_data->test_cases)
      oem_data->test_cases |= OEM_TEST_FULL_TEST;

      oem_data->need_parse_test_config_from_dt = false;

}

static void print_selftest_config(const struct cts_oem_data *oem_data)
{
    cts_info("Seltest configuration:");

    /** reset pin **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_RESET_PIN,
             oem_data->test_cases & OEM_TEST_RESET_PIN_MASK ? 'Y' : 'N');

    /** int pin **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_INT_PIN,
             oem_data->test_cases & OEM_TEST_INT_PIN_MASK ? 'Y' : 'N');

    /** rawdata **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_RAWDATA,
             oem_data->test_cases & OEM_TEST_RAWDATA_MASK ? 'Y' : 'N');
    if (oem_data->test_cases & OEM_TEST_RAWDATA_MASK) {

        cts_info(" - %-32s = %u", OEM_OF_PROPNAME_RAWDATA_FRAMES,
                 oem_data->test_rawdata.sample_frames);
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_RAWDATA_MIN,
                 oem_data->test_rawdata.threshold_min);
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_RAWDATA_MAX,
                 oem_data->test_rawdata.threshold_max);
    }

    /** noise **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_NOISE,
         oem_data->test_cases & OEM_TEST_NOISE_MASK ? 'Y' : 'N');
    if (oem_data->test_cases & OEM_TEST_NOISE_MASK) {
        cts_info(" - %-32s = %u", OEM_OF_PROPNAME_NOISE_FRAMES,
                 oem_data->test_noise.sample_frames);
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_NOISE_MAX,
                 oem_data->test_noise.threshold_max);
    }

    /** open **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_OPEN,
         oem_data->test_cases & OEM_TEST_OPEN_MASK ? 'Y' : 'N');
    if (oem_data->test_cases & OEM_TEST_OPEN_MASK) {
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_OPEN_MIN,
                 oem_data->test_open.threshold_min);
    }

    /** short **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_SHORT,
         oem_data->test_cases & OEM_TEST_SHORT_MASK ? 'Y' : 'N');
    if (oem_data->test_cases & OEM_TEST_SHORT_MASK) {
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_SHORT_MIN,
                 oem_data->test_short.threshold_min);
    }

    /** comp cap **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_COMP_CAP,
         oem_data->test_cases & OEM_TEST_COMP_CAP_MASK ? 'Y' : 'N');
         if (oem_data->test_cases & OEM_TEST_COMP_CAP_MASK) {
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_COMP_CAP_MIN,
                 oem_data->test_comp_cap.threshold_min);
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_COMP_CAP_MAX,
                 oem_data->test_comp_cap.threshold_max);
    }
}

void convert_oem_data_to_test_param(struct cts_oem_data *oem_data,
    struct cts_test_param *test_param, int test_item)
{
    memset(test_param, 0, sizeof(struct cts_test_param));

    switch (test_item) {
        case CTS_TEST_RESET_PIN:
            test_param->test_item = CTS_TEST_RESET_PIN;
            break;
        case CTS_TEST_INT_PIN:
            test_param->test_item = CTS_TEST_INT_PIN;
            break;
        case CTS_TEST_RAWDATA:
            test_param->test_item = CTS_TEST_RAWDATA;
            test_param->flags = CTS_TEST_RAWDATA_FLAG;
            test_param->min = &oem_data->test_rawdata.threshold_min;
            test_param->max = &oem_data->test_rawdata.threshold_max;
            test_param->test_data_buf = oem_data->test_rawdata.test_data_buf;
            test_param->test_data_buf_size = oem_data->test_rawdata.data_buff_size;
            test_param->test_data_size = &oem_data->test_rawdata.data_size;
            test_param->sample_frames = oem_data->test_rawdata.sample_frames;
            oem_data->test_rawdata.data_size = 0;
            break;
        case CTS_TEST_NOISE:
            test_param->test_item = CTS_TEST_NOISE;
            test_param->flags = CTS_TEST_NOISE_FLAG;
            test_param->max = &oem_data->test_noise.threshold_max;
            test_param->test_data_buf = oem_data->test_noise.test_data_buf;
            test_param->test_data_buf_size = oem_data->test_noise.data_buff_size;
            test_param->test_data_size = &oem_data->test_noise.data_size;
            test_param->sample_frames = oem_data->test_noise.sample_frames;
            oem_data->test_noise.data_size = 0;
            break;
        case CTS_TEST_OPEN:
            test_param->test_item = CTS_TEST_OPEN;
            test_param->flags = CTS_TEST_OPEN_SHORT_FLAG;
            test_param->min = &oem_data->test_open.threshold_min;
            test_param->test_data_buf = oem_data->test_open.test_data_buf;
            test_param->test_data_buf_size = oem_data->test_open.data_buff_size;
            test_param->test_data_size = &oem_data->test_open.data_size;
            oem_data->test_open.data_size = 0;
            break;
        case CTS_TEST_SHORT:
            test_param->test_item = CTS_TEST_SHORT;
            test_param->flags = CTS_TEST_OPEN_SHORT_FLAG;
            test_param->min = &oem_data->test_short.threshold_min;
            test_param->test_data_buf = oem_data->test_short.test_data_buf;
            test_param->test_data_buf_size = oem_data->test_short.data_buff_size;
            test_param->test_data_size = &oem_data->test_short.data_size;
            oem_data->test_short.data_size = 0;
            break;
        case CTS_TEST_COMPENSATE_CAP:
            test_param->test_item = CTS_TEST_COMPENSATE_CAP;
            test_param->flags = CTS_TEST_CNEG_FLAG;
            test_param->min = &oem_data->test_comp_cap.threshold_min;
            test_param->max = &oem_data->test_comp_cap.threshold_max;
            test_param->test_result = &oem_data->test_comp_cap.test_result;
            test_param->test_data_buf = oem_data->test_comp_cap.test_data_buf;
            test_param->test_data_buf_size = oem_data->test_comp_cap.data_buff_size;
            test_param->test_data_size = &oem_data->test_comp_cap.data_size;
            oem_data->test_comp_cap.data_size = 0;
            break;
        default:
            cts_warn("test item(%d) not supported\n", test_item);
     }
 }


static void _do_tests(struct cts_oem_data *oem_data)
{
    struct cts_test_param test_param;

    cts_info("Do tests");
	TestResult_RESET = 0;
	TestResult_INT = 0;
	TestResult_FW_Rawdata = 0;
	TestResult_Noise = 0;
	TestResult_Open = 0;
	TestResult_Short = 0;
	TestResult_CC = 0;

    /** reset pin test **/
    if (oem_data->test_cases & OEM_TEST_RESET_PIN_MASK) {
        convert_oem_data_to_test_param(oem_data, &test_param, CTS_TEST_RESET_PIN);
        oem_data->test_reset_pin.test_result =
            cts_test_reset_pin(&oem_data->cts_data->cts_dev);
        if (oem_data->test_reset_pin.test_result)
            cts_err("Test reset pin failed %d", oem_data->test_reset_pin.test_result);
		TestResult_RESET = oem_data->test_reset_pin.test_result;
    }

    /** int pin test **/
    if (oem_data->test_cases & OEM_TEST_INT_PIN_MASK) {
        convert_oem_data_to_test_param(oem_data, &test_param, CTS_TEST_INT_PIN);
        oem_data->test_int_pin.test_result =
            cts_test_int_pin(&oem_data->cts_data->cts_dev);
        if (oem_data->test_rawdata.test_result)
            cts_err("Test int pin failed %d", oem_data->test_rawdata.test_result);
		TestResult_INT = oem_data->test_int_pin.test_result;
    }

    /** rawdata test **/
    if (oem_data->test_cases & OEM_TEST_RAWDATA_MASK) {
        convert_oem_data_to_test_param(oem_data, &test_param, CTS_TEST_RAWDATA);
        oem_data->test_rawdata.test_result =
            cts_test_rawdata(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->test_rawdata.test_result)
            cts_err("Test rawdata failed %d", oem_data->test_rawdata.test_result);
		TestResult_FW_Rawdata = oem_data->test_rawdata.test_result;
    }

    /** noise test **/
    if (oem_data->test_cases & OEM_TEST_NOISE_MASK) {
        convert_oem_data_to_test_param(oem_data, &test_param, CTS_TEST_NOISE);
        oem_data->test_noise.test_result =
            cts_test_noise(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->test_noise.test_result)
            cts_err("Test noise failed %d", oem_data->test_noise.test_result);
		TestResult_Noise = oem_data->test_noise.test_result;
    }

    /** open test **/
    if (oem_data->test_cases & OEM_TEST_OPEN_MASK) {
        convert_oem_data_to_test_param(oem_data, &test_param, CTS_TEST_OPEN);
        oem_data->test_open.test_result =
            cts_test_open(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->test_open.test_result)
            cts_err("Test open failed %d", oem_data->test_open.test_result);
		TestResult_Open = oem_data->test_open.test_result;
    }

    /** short test **/
    if (oem_data->test_cases & OEM_TEST_SHORT_MASK) {
        convert_oem_data_to_test_param(oem_data, &test_param, CTS_TEST_SHORT);
        oem_data->test_short.test_result =
            cts_test_short(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->test_short.test_result)
           cts_err("Test short failed %d", oem_data->test_short.test_result);
		TestResult_Short = oem_data->test_short.test_result;
    }

    /** comp cap test **/
    if (oem_data->test_cases & OEM_TEST_COMP_CAP_MASK) {
        convert_oem_data_to_test_param(oem_data, &test_param, CTS_TEST_COMPENSATE_CAP);
        oem_data->test_comp_cap.test_result =
            cts_test_compensate_cap(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->test_comp_cap.test_result)
            cts_err("Test compensate cap failed %d",
                   oem_data->test_comp_cap.test_result);
		TestResult_CC = oem_data->test_comp_cap.test_result;		
    }
}

static void do_selftest(struct cts_device *cts_dev, struct cts_oem_data *oem_data) {
    prepare_product_test(cts_dev);
    _do_tests(oem_data);
    post_product_test(cts_dev);
}

static int dump_tsdata_row_to_buffer(char *buf, size_t size, const u16 *data,
                                     int cols, const char *prefix, const char *suffix, char seperator)
{
    int c, count = 0;

    if (prefix) {
        count += scnprintf(buf, size, "%s", prefix);
    }

    for (c = 0; c < cols; c++) {
        count += scnprintf(buf + count, size - count,
                           "%4u%c ", data[c], seperator);
    }

    if (suffix) {
        count += scnprintf(buf + count, size - count, "%s", suffix);
    }

    return count;
}

static int dump_tsdata_to_csv_file(const char *filepath,
                                   int flags, const u16 *data, int frames, int rows, int cols, int hw_cols)
{
#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
    return -EPERM;
#else
    loff_t pos = 0;
    int i, r, ret;
    struct file *file;

    cts_info("Dump tsdata to csv file: '%s' flags: 0x%x data: %p frames: %d row: %d col: %d hw_col: %d",
             filepath, flags, data, frames, rows, cols, hw_cols);

    file = filp_open(filepath, flags, 0666);
    if (IS_ERR(file)) {
        cts_err("Open file '%s' failed %ld", filepath, PTR_ERR(file));
        return PTR_ERR(file);
    }

    for (i = 0; i < frames; i++) {
        for (r = 0; r < rows; r++) {
            char linebuf[256];
            int len;

            len = dump_tsdata_row_to_buffer(linebuf, sizeof(linebuf),
                                            data, cols, NULL, "\n", ',');
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
            ret = kernel_write(file, linebuf, len, &pos);
#else
            ret = kernel_write(file, linebuf, len, pos);
            pos += len;
#endif
            if (ret != len) {
                cts_err("Write to file '%s' failed %d", filepath, ret);
                goto close_file;
            }
            data += hw_cols;
        }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
        ret = kernel_write(file, "\n", 1, &pos);
#else
        ret = kernel_write(file, "\n", 1, pos);
        pos ++;
#endif
        if (ret != 1) {
            cts_err("Write newline to file '%s' failed %d", filepath, ret);
            goto close_file;
        }
    }

close_file: {
        int r = filp_close(file, NULL);
        if (r) {
            cts_err("Close file '%s' failed %d", filepath, r);
        }
    }

    return ret;
#endif
}

static void dump_tsdata_to_seq_file(struct seq_file *m,
                                    const u16 *data, int rows, int cols, int hw_cols)
{
    int r;

    for (r = 0; r < rows; r++) {
        char linebuf[256];

        dump_tsdata_row_to_buffer(linebuf, sizeof(linebuf),
                                        data, cols, NULL, "\n", ',');
        seq_puts(m, linebuf);

        data += hw_cols;
    }
}

static int dump_comp_cap_row_to_buffer(char *buf, size_t size, const u16 *cap,
                                       int cols, const char *prefix, const char *suffix, char seperator)
{
    int c, count = 0;

    if (prefix) {
        count += scnprintf(buf, size, "%s", prefix);
    }

    for (c = 0; c < cols; c++) {
        count += scnprintf(buf + count, size - count,
                           "%3u%c ", cap[c], seperator);
    }

    if (suffix) {
        count += scnprintf(buf + count, size - count, "%s", suffix);
    }

    return count;
}

static int dump_comp_cap_to_csv_file(const char *filepath,
                                     int flags, const u16 *cap, int rows, int cols, int hw_cols)
{
#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
    return -EPERM;
#else
    struct file *file;
    int r, ret = 0;
    loff_t pos = 0;

    cts_info("Dump compensate cap to csv file: '%s' flags: 0x%x row: %d col: %d",
             filepath, flags, rows, cols);

    file = filp_open(filepath, flags, 0666);
    if (IS_ERR(file)) {
        cts_err("Open file '%s' failed %ld", filepath, PTR_ERR(file));
        return PTR_ERR(file);
    }

    for (r = 0; r < rows; r++) {
        char linebuf[256];
        int len;

        len = dump_comp_cap_row_to_buffer(linebuf, sizeof(linebuf),
                                          cap, cols, NULL, "\n", ',');
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
        ret = kernel_write(file, linebuf, len, &pos);
#else
        ret = kernel_write(file, linebuf, len, pos);
        pos += len;
#endif
        if (ret != len) {
            cts_err("Write to file '%s' failed %d", filepath, ret);
            goto close_file;
        }

        cap += hw_cols;
    }

close_file: {
        int r = filp_close(file, NULL);
        if (r) {
            cts_err("Close file '%s' failed %d", filepath, ret);
        }
    }

    return ret;
#endif
}

static void dump_comp_cap_to_seq_file(struct seq_file *m,
                                      const u16 *data, int rows, int cols, int hw_cols)
{
    int r;

    for (r = 0; r < rows; r++) {
        char linebuf[256];

        dump_comp_cap_row_to_buffer(linebuf, sizeof(linebuf),
                                          data, cols, NULL, "\n", ',');
        seq_puts(m, linebuf);

        data += hw_cols;
    }
}

static int save_selftest_data_to_file(struct cts_oem_data *oem_data)
{
    int rows, cols, hw_cols;
    int ret;

    cts_info("Save selftest data to file");

    rows = oem_data->cts_data->cts_dev.fwdata.rows;
    cols = oem_data->cts_data->cts_dev.fwdata.cols;
    hw_cols = oem_data->cts_data->cts_dev.hwdata->num_col;

    if (oem_data->test_cases & OEM_TEST_RAWDATA_MASK) {
        ret = dump_tsdata_to_csv_file(oem_data->test_rawdata.data_file_path,
                O_RDWR | O_CREAT | O_TRUNC, oem_data->test_rawdata.test_data_buf,
                oem_data->test_rawdata.sample_frames, rows, cols, hw_cols);
        if (ret < 0) {
            cts_err("Dump rawdata test data to file failed");
            return ret;
        }
    }

    if (oem_data->test_cases & OEM_TEST_NOISE_MASK) {
        ret = dump_tsdata_to_csv_file(oem_data->test_noise.data_file_path,
            O_RDWR | O_CREAT | O_TRUNC, oem_data->test_noise.test_data_buf,
            (oem_data->test_noise.sample_frames + 3), rows, cols, hw_cols);
        if (ret < 0) {
            cts_err("Dump noise test data to file failed");
            return ret;
        }
    }

    if (oem_data->test_cases & OEM_TEST_OPEN_MASK) {
        ret = dump_tsdata_to_csv_file(oem_data->test_open.data_file_path,
            O_RDWR | O_CREAT | O_TRUNC, oem_data->test_open.test_data_buf,
            1, rows, cols, hw_cols);
        if (ret < 0) {
            cts_err("Dump open test data to file failed");
            return ret;
        }
    }

     if (oem_data->test_cases & OEM_TEST_SHORT_MASK) {
         ret = dump_tsdata_to_csv_file(oem_data->test_short.data_file_path,
             O_RDWR | O_CREAT | O_TRUNC, oem_data->test_short.test_data_buf,
             10, rows, cols, hw_cols);
        if (ret < 0) {
            cts_err("Dump short test data to file failed");
            return ret;
        }
    }

     if (oem_data->test_cases & OEM_TEST_COMP_CAP_MASK) {
         ret = dump_comp_cap_to_csv_file(oem_data->test_comp_cap.data_file_path,
             O_RDWR | O_CREAT | O_TRUNC,
             oem_data->test_comp_cap.test_data_buf, rows, cols, hw_cols);
        if (ret < 0) {
            cts_err("Dump compensate cap test data to file failed");
            return ret;
        }
    }

    return 0;
}

static void *selftest_seq_start(struct seq_file *m, loff_t *pos)
{
    return *pos < 1 ? (void *)1 : NULL;
}

static void *selftest_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
    ++*pos;
    return NULL;
}

static void selftest_seq_stop(struct seq_file *m, void *v)
{
    return;
}

static int selftest_seq_show(struct seq_file *m, void *v)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)m->private;
    struct cts_oem_data *oem_data = NULL;
    int i, rows, cols, hw_rows, hw_cols;

    cts_info("Show seq selftest");

    if (cts_data == NULL) {
        cts_err("Selftest seq file private data = NULL");
        return -EFAULT;
    }

    oem_data = cts_data->oem_data;
    rows    = cts_data->cts_dev.fwdata.rows;
    cols    = cts_data->cts_dev.fwdata.cols;
    hw_rows    = cts_data->cts_dev.hwdata->num_row;
    hw_cols    = cts_data->cts_dev.hwdata->num_col;

    seq_printf(m, "FW Version %04x!\n\n", cts_data->cts_dev.fwdata.version);

    if (oem_data->test_cases & OEM_TEST_RESET_PIN_MASK) {
        seq_printf(m, "Reset-Pin Test %s!\n\n",
            oem_data->test_reset_pin.test_result == 0 ? "PASS" : "FAIL");
    }
    if (oem_data->test_cases & OEM_TEST_INT_PIN_MASK) {
        seq_printf(m, "Int-Pin Test %s!\n\n",
            oem_data->test_int_pin.test_result == 0 ? "PASS" : "FAIL");
    }
    if (oem_data->test_cases & OEM_TEST_RAWDATA_MASK) {
        seq_printf(m, "FW Rawdata Test");
        if (oem_data->test_rawdata.test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->test_rawdata.test_result > 0) {
            seq_printf(m, " FAIL!\n");
            for(i = 0; i < oem_data->test_rawdata.sample_frames; i++) {
                dump_tsdata_to_seq_file(m, oem_data->test_rawdata.test_data_buf +
                    i * hw_rows * hw_cols, rows, cols, hw_cols);
                seq_putc(m, '\n');
            }
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->test_rawdata.test_result);
        }
    }
    if (oem_data->test_cases & OEM_TEST_NOISE_MASK) {
        seq_printf(m, "Noise Test");
        if (oem_data->test_noise.test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->test_noise.test_result > 0) {
            seq_printf(m, " FAIL!\n");
           for(i = 0; i < oem_data->test_noise.sample_frames; i++) {
                dump_tsdata_to_seq_file(m, oem_data->test_noise.test_data_buf +
                        i * hw_rows * hw_cols, rows, cols, hw_cols);
                seq_putc(m, '\n');
            }
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->test_noise.test_result);
        }
    }
    if (oem_data->test_cases & OEM_TEST_OPEN_MASK) {
        seq_printf(m, "Open Test");
        if (oem_data->test_open.test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->test_open.test_result > 0) {
            seq_printf(m, " FAIL!\n");
            dump_tsdata_to_seq_file(m, oem_data->test_open.test_data_buf,
                rows, cols, hw_cols);
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->test_open.test_result);
        }
    }
    if (oem_data->test_cases & OEM_TEST_SHORT_MASK) {
        seq_printf(m, "Short Test");
        if (oem_data->test_short.test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->test_short.test_result > 0) {
            seq_printf(m, " FAIL!\n");
            for (i = 0; i < 10; i++) {
                dump_tsdata_to_seq_file(m, oem_data->test_short.test_data_buf +
                    i * hw_rows * hw_cols, rows, cols, hw_cols);
                seq_putc(m, '\n');
            }
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->test_short.test_result);
        }
    }
    if (oem_data->test_cases & OEM_TEST_COMP_CAP_MASK) {
        seq_printf(m, "Compensate-Cap Test");
        if (oem_data->test_comp_cap.test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->test_comp_cap.test_result > 0) {
            seq_printf(m, " FAIL!\n");
            dump_comp_cap_to_seq_file(m, oem_data->test_comp_cap.test_data_buf,
                rows, cols, hw_cols);
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->test_comp_cap.test_result);
        }
    }

    return 0;
}


const struct seq_operations selftest_seq_ops = {
    .start  = selftest_seq_start,
    .next   = selftest_seq_next,
    .stop   = selftest_seq_stop,
    .show   = selftest_seq_show,
};

int32_t selftest_proc_open(struct inode *inode, struct file *file)
{
    /*BSP: modify for midtest by jiangzhu.yang 20230719 start*/
    //struct chipone_ts_data *cts_data = PDE_DATA(inode);
    struct chipone_ts_data *cts_data = g_cts_data;
    /*BSP: modify for midtest by jiangzhu.yang 20230719 end*/
/*
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
     struct chipone_ts_data *cts_data = PDE_DATA(inode);
#else
    struct chipone_ts_data *cts_data = pde_data(inode);
#endif
*/
    struct cts_oem_data *oem_data = NULL;
    int ret;

    if (cts_data == NULL) {
        cts_err("Open selftest proc with cts_data = NULL");
        return -EFAULT;
    }

    oem_data = cts_data->oem_data;
    if (oem_data == NULL) {
        cts_err("Open selftest proc with oem_data = NULL");
        return -EFAULT;
    }

    cts_info("Open '/proc/" OEM_SELFTEST_PROC_FILENAME "'");

    if (oem_data->need_parse_test_config_from_dt) {
#ifndef CONFIG_CTS_I2C_HOST
        parse_selftest_dt(oem_data, cts_data->pdata->spi_client->dev.of_node);
#else
        parse_selftest_dt(oem_data, cts_data->device->of_node);
#endif
    }

    print_selftest_config(oem_data);

    ret = alloc_sleftest_data_mem(oem_data,
                                  cts_data->cts_dev.hwdata->num_row * cts_data->cts_dev.hwdata->num_col);
    if (ret) {
        cts_err("Alloc test data mem failed");
        return ret;
    }

    do_selftest(&cts_data->cts_dev, oem_data);

    ret = save_selftest_data_to_file(oem_data);
    if (ret) {
        cts_err("Save selftest data to file failed %d", ret);
    }

    ret = seq_open(file, &selftest_seq_ops);
    if (ret) {
        cts_err("Open selftest seq file failed %d", ret);
        return ret;
    }

    ((struct seq_file *)file->private_data)->private = cts_data;

    return 0;
}

static int32_t selftest_seq_release(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
    struct chipone_ts_data *cts_data = PDE_DATA(inode);
#else
    struct chipone_ts_data *cts_data = pde_data(inode);
#endif

    struct cts_oem_data *oem_data = NULL;
    if (cts_data == NULL) {
        cts_err("Open selftest proc with cts_data = NULL");
        return -EFAULT;
    }

    oem_data = cts_data->oem_data;
    if (oem_data == NULL) {
        cts_err("Open selftest proc with oem_data = NULL");
        return -EFAULT;
    }

    free_selftest_data_mem(oem_data);
    return seq_release(inode, file);
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static const struct file_operations selftest_proc_fops = {
    .owner   = THIS_MODULE,
    .open    = selftest_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = selftest_seq_release,
};
#else
static const struct proc_ops selftest_proc_fops = {
    .proc_open    = selftest_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = selftest_seq_release,
};
#endif

static int cts_rawdata_show(struct seq_file *m, void *v)
{
    struct chipone_ts_data *cts_data = m->private;
    struct cts_device *cts_dev;
    u8 *rawdata;
    s16 *data;
    u8 rows, cols;
    u8 i, j;

    cts_dev = &cts_data->cts_dev;
    rows = cts_dev->hwdata->num_row;
    cols = cts_dev->hwdata->num_col;
    rawdata = kzalloc(rows * cols * 2, GFP_KERNEL);
    if (rawdata == NULL) {
        cts_err("Allocate rawdata failed");
        return -ENOMEM;
    }

    cts_lock_device(cts_dev);
    cts_tcs_top_get_rawdata(cts_dev, rawdata, rows * cols * 2);
    cts_unlock_device(cts_dev);

    data = (s16 *)rawdata;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            seq_printf(m, "%5d", data[i * cols + j]);
        }
        seq_printf(m, "\n");
    }

    kfree(rawdata);
    return 0;
}
static int cts_rawdata_open(struct inode *inode, struct file *file)
{
    return single_open(file, cts_rawdata_show, pde_data(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static const struct file_operations cts_rawdata_ops = {
    .owner = THIS_MODULE,
    .open = cts_rawdata_open,
    .read = seq_read,
};
#else
static const struct proc_ops cts_rawdata_ops = {
    .proc_open = cts_rawdata_open,
    .proc_read = seq_read,
};
#endif

static int cts_diffdata_show(struct seq_file *m, void *v)
{
    struct chipone_ts_data *cts_data = m->private;
    struct cts_device *cts_dev;
    u8 *rawdata;
    s16 *data;
    u8 rows, cols;
    u8 i, j;

    cts_dev = &cts_data->cts_dev;
    rows = cts_dev->hwdata->num_row;
    cols = cts_dev->hwdata->num_col;
    rawdata = kzalloc(rows * cols * 2, GFP_KERNEL);
    if (rawdata == NULL) {
        cts_err("Allocate rawdata failed");
        return -ENOMEM;
    }

    cts_lock_device(cts_dev);
    cts_tcs_top_get_real_diff(cts_dev, rawdata, rows * cols * 2);
    cts_unlock_device(cts_dev);

    data = (s16 *)rawdata;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            seq_printf(m, "%5d", data[i * cols + j]);
        }
        seq_printf(m, "\n");
    }

    kfree(rawdata);
    return 0;
}
static int cts_diffdata_open(struct inode *inode, struct file *file)
{
    return single_open(file, cts_diffdata_show, pde_data(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static const struct file_operations cts_diffdata_ops = {
    .owner = THIS_MODULE,
    .open = cts_diffdata_open,
    .read = seq_read,
};
#else
static const struct proc_ops cts_diffdata_ops = {
    .proc_open = cts_diffdata_open,
    .proc_read = seq_read,
};
#endif

static int cts_manual_show(struct seq_file *m, void *v)
{
    struct chipone_ts_data *cts_data = m->private;
    struct cts_device *cts_dev;
    u8 *rawdata;
    s16 *data;
    u8 rows, cols;
    u8 i, j;

    cts_dev = &cts_data->cts_dev;
    rows = cts_dev->hwdata->num_row;
    cols = cts_dev->hwdata->num_col;
    rawdata = kzalloc(rows * cols * 2, GFP_KERNEL);
    if (rawdata == NULL) {
        cts_err("Allocate rawdata failed");
        return -ENOMEM;
    }

    cts_lock_device(cts_dev);
    cts_tcs_top_get_manual_diff(cts_dev, rawdata, rows * cols * 2);
    cts_unlock_device(cts_dev);

    data = (s16 *)rawdata;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            seq_printf(m, "%5d", data[i * cols + j]);
        }
        seq_printf(m, "\n");
    }

    kfree(rawdata);
    return 0;
}
static int cts_manual_open(struct inode *inode, struct file *file)
{
    return single_open(file, cts_manual_show, pde_data(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static const struct file_operations cts_manual_ops = {
    .owner = THIS_MODULE,
    .open = cts_manual_open,
    .read = seq_read,
};
#else
static const struct proc_ops cts_manual_ops = {
    .proc_open = cts_manual_open,
    .proc_read = seq_read,
};
#endif

/* Use limit bin */
static int cts_limit_show(struct seq_file *m, void *v)
{
    struct chipone_ts_data *cts_data = m->private;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    const struct firmware *limit_fw = NULL;
    struct cts_limit limit;
    char header[7] = {'C', 'h', 'i', 'p', 'o', 'n', 'e'};
    int ret;

    ret = request_firmware(&limit_fw, CFG_CTS_FACTORY_LIMIT_FILENAME,
                           &cts_dev->pdata->ts_input_dev->dev);
    if (ret) {
        seq_printf(m, "Request %s failed!\n", CFG_CTS_FACTORY_LIMIT_FILENAME);
        return 0;
    }

    ret = memcmp(header, limit_fw->data, sizeof(header));
    if (ret) {
        seq_printf(m, "LIMIT file was not matched\n");
        release_firmware(limit_fw);
        return 0;
    } else {
        cts_info("Matched limit file!");
    }

    memcpy(&limit, limit_fw->data, limit_fw->size);
    release_firmware(limit_fw);

    seq_printf(m, "Limit version: %x, type:%d\n", limit.version, limit.type);
    seq_printf(m, "\n");
    seq_printf(m, "Switch status:\n");
    seq_printf(m, " int_item              : %s\n", limit.int_item ? "Y" : "N");
    seq_printf(m, " reset_item            : %s\n", limit.reset_item ? "Y" : "N");
    seq_printf(m, " normal_rawdata_item   : %s\n", limit.normal_rawdata_item ? "Y" : "N");
    seq_printf(m, " normal_noise_item     : %s\n", limit.normal_noise_item ? "Y" : "N");
    seq_printf(m, " open_item             : %s\n", limit.open_item ? "Y" : "N");
    seq_printf(m, " short_item            : %s\n", limit.short_item ? "Y" : "N");
    seq_printf(m, " comp_cap_item         : %s\n", limit.comp_cap_item ? "Y" : "N");
    seq_printf(m, " gstr_rawdata_item     : %s\n", limit.gstr_rawdata_item ? "Y" : "N");
    seq_printf(m, " gstrlp_rawdata_item   : %s\n", limit.gstrlp_rawdata_item ? "Y" : "N");
    seq_printf(m, " gstr_noise_item       : %s\n", limit.gstr_noise_item ? "Y" : "N");
    seq_printf(m, " gstrlp_noise_item     : %s\n", limit.gstrlp_noise_item ? "Y" : "N");
    seq_printf(m, "\n");
    seq_printf(m, "Threshold:\n");
    seq_printf(m, " normal_rawdata_frames : %d\n", limit.normal_rawdata_frames);
    seq_printf(m, " normal_rawdata_min    : %d\n", limit.normal_rawdata_min);
    seq_printf(m, " normal_rawdata_max    : %d\n", limit.normal_rawdata_max);
    seq_printf(m, " normal_noise_frames   : %d\n", limit.normal_noise_frames);
    seq_printf(m, " normal_noise_max      : %d\n", limit.normal_noise_max);
    seq_printf(m, " normal_open_min       : %d\n", limit.normal_open_min);
    seq_printf(m, " normal_short_min      : %d\n", limit.normal_short_min);
    seq_printf(m, " normal_compcap_min    : %d\n", limit.normal_compcap_min);
    seq_printf(m, " normal_compcap_max    : %d\n", limit.normal_compcap_max);
    seq_printf(m, " gstr_rawdata_frames   : %d\n", limit.gstr_rawdata_frames);
    seq_printf(m, " gstr_rawdata_min      : %d\n", limit.gstr_rawdata_min);
    seq_printf(m, " gstr_rawdata_max      : %d\n", limit.gstr_rawdata_max);
    seq_printf(m, " gstrlp_rawdata_frames : %d\n", limit.gstrlp_rawdata_frames);
    seq_printf(m, " gstrlp_rawdata_min    : %d\n", limit.gstrlp_rawdata_min);
    seq_printf(m, " gstrlp_rawdata_max    : %d\n", limit.gstrlp_rawdata_max);
    seq_printf(m, " gstr_noise_frames     : %d\n", limit.gstr_noise_frames);
    seq_printf(m, " gstr_noise_max        : %d\n", limit.gstr_noise_max);
    seq_printf(m, " gstrlp_noise_frames   : %d\n", limit.gstrlp_noise_frames);
    seq_printf(m, " gstrlp_noise_max      : %d\n", limit.gstrlp_noise_max);

    return 0;
}
static int cts_limit_open(struct inode *inode, struct file *file)
{
    return single_open(file, cts_limit_show, pde_data(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static const struct file_operations cts_limit_ops = {
    .owner = THIS_MODULE,
    .open = cts_limit_open,
    .read = seq_read,
};
#else
static const struct proc_ops cts_limit_ops = {
    .proc_open = cts_limit_open,
    .proc_read = seq_read,
};
#endif

static int cts_factory_test_show(struct seq_file *m, void *v)
{
#define TOUCH_DATA_DIRECTORY_PREFIX           "/sdcard/chipone-tddi/test"
#define RAWDATA_TEST_DATA_FILENAME            "rawdata-test-data.txt"
#define NOISE_TEST_DATA_FILENAME              "noise-test-data.txt"
#define OPEN_TEST_DATA_FILENAME               "open-test-data.txt"
#define SHORT_TEST_DATA_FILENAME              "short-test-data.txt"
#define COMP_CAP_TEST_DATA_FILENAME           "comp-cap-test-data.txt"
    struct chipone_ts_data *cts_data = m->private;
    struct cts_device *cts_dev = &cts_data->cts_dev;

    /*s64 reset_pin_test_elapsed_time = 0;*/
    /*struct cts_test_param reset_pin_test_param = {
        .test_item = CTS_TEST_RESET_PIN,
        .flags = 0,
        .elapsed_time_ms = &reset_pin_test_elapsed_time,
    };*/
    /*s64 int_pin_test_elapsed_time = 0;*/
    /*struct cts_test_param int_pin_test_param = {
        .test_item = CTS_TEST_INT_PIN,
        .flags = 0,
        .elapsed_time_ms = &int_pin_test_elapsed_time,
    };*/

    s64 rawdata_test_elapsed_time = 0;
    struct cts_test_param rawdata_test_param = {
        .test_item = CTS_TEST_RAWDATA,
        .flags = CTS_TEST_RAWDATA_FLAG,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .elapsed_time_ms = &rawdata_test_elapsed_time,
        .sample_frames = 50,
    };
    s64 noise_test_elapsed_time = 0;
    struct cts_test_param noise_test_param = {
        .test_item = CTS_TEST_NOISE,
        .flags = CTS_TEST_NOISE_FLAG,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .elapsed_time_ms = &noise_test_elapsed_time,
        .sample_frames = 50,
    };
    s64 open_test_elapsed_time = 0;
    struct cts_test_param open_test_param = {
        .test_item = CTS_TEST_OPEN,
        .flags = CTS_TEST_OPEN_SHORT_FLAG,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .elapsed_time_ms = &open_test_elapsed_time,
    };
    s64 short_test_elapsed_time = 0;
    struct cts_test_param short_test_param = {
        .test_item = CTS_TEST_SHORT,
        .flags = CTS_TEST_OPEN_SHORT_FLAG,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .elapsed_time_ms = &short_test_elapsed_time,
    };
    s64 comp_cap_test_elapsed_time = 0;
    struct cts_test_param comp_cap_test_param = {
        .test_item = CTS_TEST_COMPENSATE_CAP,
        .flags = CTS_TEST_CNEG_FLAG,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .elapsed_time_ms = &comp_cap_test_elapsed_time,
    };
    const struct firmware *limit_fw = NULL;
    struct cts_limit limit;
    char header[7] = {'C', 'h', 'i', 'p', 'o', 'n', 'e'};
    int int_pin_test_result;
    int reset_pin_test_result;
    int rawdata_test_result;
    int noise_test_result;
    int open_test_result;
    int short_test_result;
    int comp_cap_test_result;
    char touch_data_filepath[256];
    ktime_t start_time, end_time, delta_time;
    u16 fw_version;
    int ret;

    ret = request_firmware(&limit_fw, CFG_CTS_FACTORY_LIMIT_FILENAME,
                           &cts_dev->pdata->ts_input_dev->dev);
    if (ret) {
        cts_err("Request %s failed!", CFG_CTS_FACTORY_LIMIT_FILENAME);
        return 0;
    }

    ret = memcmp(header, limit_fw->data, sizeof(header));
    if (ret) {
        cts_err("LIMIT file was not matched");
        release_firmware(limit_fw);
        return 0;
    } else {
        cts_info("Matched limit file!");
    }

    memcpy(&limit, limit_fw->data, limit_fw->size);
    release_firmware(limit_fw);

    cts_info("limit version         : %x", limit.version);
    cts_info("int_item              : %s", limit.int_item ? "Y" : "N");
    cts_info("reset_item            : %s", limit.reset_item ? "Y" : "N");
    cts_info("normal_rawdata_item   : %s", limit.normal_rawdata_item ? "Y" : "N");
    cts_info("normal_noise_item     : %s", limit.normal_noise_item ? "Y" : "N");
    cts_info("open_item             : %s", limit.open_item ? "Y" : "N");
    cts_info("short_item            : %s", limit.short_item ? "Y" : "N");
    cts_info("comp_cap_item         : %s", limit.comp_cap_item ? "Y" : "N");
    cts_info("gstr_rawdata_item     : %s", limit.gstr_rawdata_item ? "Y" : "N");
    cts_info("gstrlp_rawdata_item   : %s", limit.gstrlp_rawdata_item ? "Y" : "N");
    cts_info("gstr_noise_item       : %s", limit.gstr_noise_item ? "Y" : "N");
    cts_info("gstrlp_noise_item     : %s", limit.gstrlp_noise_item ? "Y" : "N");
    cts_info("type                  : %d", limit.type);
    cts_info("----------------------------");
    //cts_info("normal_rawdata_frames : %d", limit.normal_rawdata_frames);
    cts_info("normal_rawdata_min    : %d", limit.normal_rawdata_min);
    cts_info("normal_rawdata_max    : %d", limit.normal_rawdata_max);
    //cts_info("normal_noise_frames   : %d", limit.normal_noise_frames);
    cts_info("normal_noise_max      : %d", limit.normal_noise_max);
    cts_info("normal_open_min       : %d", limit.normal_open_min);
    cts_info("normal_short_min      : %d", limit.normal_short_min);
    cts_info("normal_compcap_min    : %d", limit.normal_compcap_min);
    cts_info("normal_compcap_max    : %d", limit.normal_compcap_max);
    cts_info("gstr_rawdata_frames   : %d", limit.gstr_rawdata_frames);
    cts_info("gstr_rawdata_min      : %d", limit.gstr_rawdata_min);
    cts_info("gstr_rawdata_max      : %d", limit.gstr_rawdata_max);
    cts_info("gstrlp_rawdata_frames : %d", limit.gstrlp_rawdata_frames);
    cts_info("gstrlp_rawdata_min    : %d", limit.gstrlp_rawdata_min);
    cts_info("gstrlp_rawdata_max    : %d", limit.gstrlp_rawdata_max);
    cts_info("gstr_noise_frames     : %d", limit.gstr_noise_frames);
    cts_info("gstr_noise_max        : %d", limit.gstr_noise_max);
    cts_info("gstrlp_noise_frames   : %d", limit.gstrlp_noise_frames);
    cts_info("gstrlp_noise_max      : %d", limit.gstrlp_noise_max);

    rawdata_test_param.min = &limit.normal_rawdata_min;
    rawdata_test_param.max = &limit.normal_rawdata_max;
    noise_test_param.max = &limit.normal_noise_max;
    open_test_param.min = &limit.normal_open_min;
    short_test_param.min = &limit.normal_short_min;
    comp_cap_test_param.min = &limit.normal_compcap_min;
    comp_cap_test_param.max = &limit.normal_compcap_max;

    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
             TOUCH_DATA_DIRECTORY_PREFIX"/"RAWDATA_TEST_DATA_FILENAME);
    rawdata_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
             TOUCH_DATA_DIRECTORY_PREFIX"/"NOISE_TEST_DATA_FILENAME);
    noise_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
             TOUCH_DATA_DIRECTORY_PREFIX"/"OPEN_TEST_DATA_FILENAME);
    open_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
             TOUCH_DATA_DIRECTORY_PREFIX"/"SHORT_TEST_DATA_FILENAME);
    short_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
             TOUCH_DATA_DIRECTORY_PREFIX"/"COMP_CAP_TEST_DATA_FILENAME);
    comp_cap_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);

    start_time = ktime_get();

    ret = cts_tcs_get_fw_ver(cts_dev, &fw_version);
    if (ret) {
        seq_printf(m, "Factory test get firmware version failed %d\n", ret);
        fw_version = 0;
    }

    if (limit.int_item) {
        int_pin_test_result = cts_test_int_pin(cts_dev);
    }
    if (limit.reset_item) {
        reset_pin_test_result = cts_test_reset_pin(cts_dev);
    }
    if (limit.normal_rawdata_item) {
        rawdata_test_result = cts_test_rawdata(cts_dev, &rawdata_test_param);
    }
    if (limit.normal_noise_item) {
        noise_test_result = cts_test_noise(cts_dev, &noise_test_param);
    }
    if (limit.open_item) {
        open_test_result = cts_test_open(cts_dev, &open_test_param);
    }
    if (limit.short_item) {
        short_test_result = cts_test_short(cts_dev, &short_test_param);
    }
    if (limit.comp_cap_item) {
        comp_cap_test_result =
            cts_test_compensate_cap(cts_dev, &comp_cap_test_param);
    }

    if (rawdata_test_param.test_data_filepath) {
        kfree(rawdata_test_param.test_data_filepath);
    }
    if (noise_test_param.test_data_filepath) {
        kfree(noise_test_param.test_data_filepath);
    }
    if (open_test_param.test_data_filepath) {
        kfree(open_test_param.test_data_filepath);
    }
    if (short_test_param.test_data_filepath) {
        kfree(short_test_param.test_data_filepath);
    }
    if (comp_cap_test_param.test_data_filepath) {
        kfree(comp_cap_test_param.test_data_filepath);
    }

    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);

    seq_printf(m, "Factory test, total ELAPSED TIME: %lldms\n",
               ktime_to_ms(delta_time));

    seq_printf(m, "Firmware Version: 0x%04X\n", fw_version);

    if (limit.int_item) {
        seq_printf(m, "Int-Pin  : %s,\n", int_pin_test_result ? "Failed" : "Pass");
    }
    if (limit.reset_item) {
        seq_printf(m, "Reset-Pin: %s\n", reset_pin_test_result ? "Failed" : "Pass");
    }
    if (limit.normal_rawdata_item) {
        seq_printf(m, "Rawdata  : %s\n", rawdata_test_result ? "Failed" : "Pass");
    }
    if (limit.normal_noise_item) {
        seq_printf(m, "Noise    : %s\n", noise_test_result ? "Failed" : "Pass");
    }
    if (limit.open_item) {
        seq_printf(m, "Open     : %s\n", open_test_result ? "Failed" : "Pass");
    }
    if (limit.short_item) {
        seq_printf(m, "Short    : %s\n", short_test_result ? "Failed" : "Pass");
    }
    if (limit.comp_cap_item) {
        seq_printf(m, "Comp-Cap : %s\n", comp_cap_test_result ? "Failed" : "Pass");
    }

    return 0;

#undef TOUCH_DATA_DIRECTORY_PREFIX
#undef RAWDATA_TEST_DATA_FILENAME
#undef NOISE_TEST_DATA_FILENAME
#undef OPEN_TEST_DATA_FILENAME
#undef SHORT_TEST_DATA_FILENAME
#undef COMP_CAP_TEST_DATA_FILENAME
}
static int cts_factory_test_open(struct inode *inode, struct file *file)
{
    return single_open(file, cts_factory_test_show, pde_data(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static const struct file_operations cts_factory_test_ops = {
    .owner = THIS_MODULE,
    .open = cts_factory_test_open,
    .read = seq_read,
};
#else
static const struct proc_ops cts_factory_test_ops = {
    .proc_open = cts_factory_test_open,
    .proc_read = seq_read,
};
#endif

int cts_oem_init(struct chipone_ts_data *cts_data)
{
    struct cts_oem_data *oem_data = &cts_oem_data;
    int ret = 0;
	 


    if (cts_data == NULL) {
        cts_err("Init with cts_data = NULL");
        return -EINVAL;
    }

    cts_info("Init");

    cts_data->oem_data = NULL;

    cts_info(" - Create '/proc/"OEM_SELFTEST_PROC_FILENAME"'");
    oem_data->selftest_proc_entry =
        proc_create_data(OEM_SELFTEST_PROC_FILENAME,
                         S_IRUGO, NULL, &selftest_proc_fops, cts_data);
    if (oem_data->selftest_proc_entry == NULL) {
        cts_err("Create '/proc/"OEM_SELFTEST_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto create_selftest_err;
    }

    oem_data->limit_entry =
        proc_create_data(OEM_LIMIT_PROC_FILENAME,
                         S_IRUGO, NULL, &cts_limit_ops, cts_data);
    if (oem_data->limit_entry == NULL) {
        cts_err("Create '/proc/"OEM_LIMIT_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto create_limit_err;
    }

    oem_data->factory_test_entry =
        proc_create_data(OEM_FACTORY_TEST_PROC_FILENAME,
                         S_IRUGO, NULL, &cts_factory_test_ops, cts_data);
    if (oem_data->factory_test_entry == NULL) {
        cts_err("Create '/proc/"OEM_FACTORY_TEST_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto create_factory_test_err;
    }

    oem_data->rawdata_proc_entry = proc_create_data(OEM_RAWDATA_PROC_FILENAME,
                                   S_IRUGO, NULL, &cts_rawdata_ops, cts_data);
    if (oem_data->rawdata_proc_entry == NULL) {
        cts_err("Create '/proc/"OEM_RAWDATA_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto create_rawdata_err;
    }

    oem_data->manual_proc_entry = proc_create_data(OEM_MANUAL_PROC_FILENAME,
                                  S_IRUGO, NULL, &cts_manual_ops, cts_data);
    if (oem_data->manual_proc_entry == NULL) {
        cts_err("Create '/proc/"OEM_MANUAL_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto create_manual_err;
    }

    oem_data->diffdata_proc_entry = proc_create_data(OEM_DIFFDATA_PROC_FILENAME,
                                    S_IRUGO, NULL, &cts_diffdata_ops, cts_data);
    if (oem_data->diffdata_proc_entry == NULL) {
        cts_err("Create '/proc/"OEM_DIFFDATA_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto create_diffdata_err;
    }

    oem_data->need_parse_test_config_from_dt = true;
    oem_data->test_cases = 0x0;

    cts_data->oem_data = oem_data;
    oem_data->cts_data = cts_data;

    return ret;

create_diffdata_err:
    remove_proc_entry(OEM_MANUAL_PROC_FILENAME, NULL);
create_manual_err:
    remove_proc_entry(OEM_RAWDATA_PROC_FILENAME, NULL);
create_rawdata_err:
    remove_proc_entry(OEM_FACTORY_TEST_PROC_FILENAME, NULL);
create_factory_test_err:
    remove_proc_entry(OEM_LIMIT_PROC_FILENAME, NULL);
create_limit_err:
    remove_proc_entry(OEM_SELFTEST_PROC_FILENAME, NULL);
create_selftest_err:
	 return ret;
}

int cts_oem_deinit(struct chipone_ts_data *cts_data)
{
    struct cts_oem_data *oem_data = NULL;

    if (cts_data == NULL) {
        cts_err("Deinit with cts_data = NULL");
        return -EINVAL;
    }

    if (cts_data->oem_data == NULL) {
        cts_warn("Deinit with oem_data = NULL");
        return 0;
    }

    cts_info("Deinit");

    oem_data = cts_data->oem_data;

    if (oem_data->selftest_proc_entry) {
        cts_info("  Remove '/proc/"OEM_SELFTEST_PROC_FILENAME"'");
        remove_proc_entry(OEM_SELFTEST_PROC_FILENAME, NULL);
    }

    if (oem_data->limit_entry) {
        cts_info("  Remove '/proc/"OEM_LIMIT_PROC_FILENAME"'");
        remove_proc_entry(OEM_LIMIT_PROC_FILENAME, NULL);
    }
    if (oem_data->factory_test_entry) {
        cts_info("  Remove '/proc/"OEM_FACTORY_TEST_PROC_FILENAME"'");
        remove_proc_entry(OEM_FACTORY_TEST_PROC_FILENAME, NULL);
    }

    if (oem_data->rawdata_proc_entry) {
        cts_info("  Remove '/proc/"OEM_RAWDATA_PROC_FILENAME"'");
        remove_proc_entry(OEM_RAWDATA_PROC_FILENAME, NULL);
    }

    if (oem_data->manual_proc_entry) {
        cts_info("  Remove '/proc/"OEM_MANUAL_PROC_FILENAME"'");
        remove_proc_entry(OEM_MANUAL_PROC_FILENAME, NULL);
    }

    if (oem_data->diffdata_proc_entry) {
        cts_info("  Remove '/proc/"OEM_DIFFDATA_PROC_FILENAME"'");
        remove_proc_entry(OEM_DIFFDATA_PROC_FILENAME, NULL);
    }

    cts_data->oem_data = NULL;

    return 0;
}

/*BSP: modify for midtest by jiangzhu.yang 20230719 start*/
int tran_get_SelftestResult(void)
{
    int selftestresult;

    if ((TestResult_RESET == 0) && (TestResult_INT == 0)&& (TestResult_FW_Rawdata == 0) && (TestResult_Noise == 0) && (TestResult_Open == 0) && (TestResult_Short == 0) && (TestResult_CC == 0)) {
        selftestresult = 1;
    } else {
        selftestresult = 0;
    }

    return selftestresult;
}
/*BSP: modify for midtest by jiangzhu.yang 20230719 end*/
