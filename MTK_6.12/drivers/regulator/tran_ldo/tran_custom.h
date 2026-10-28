#ifndef _TRAN_CUSTOM_H_
#define _TRAN_CUSTOM_H_

#include "tran_regulator.h"

//customize the config for each chip here
#define MAX_NUM_OF_SUPPORT_REGULATOR 10// 10 is the max number of regulator driver
#define ET5904_ID	0x00
#define SGM38121_ID	0x80
#define DIO8016_ID	0x04
#define WL28661D_ID	0x33
static unsigned int chip_id_addr_table[] = {0x00, 0xff};
static unsigned int tran_regulator_list[MAX_NUM_OF_SUPPORT_REGULATOR] = {ET5904_ID, SGM38121_ID,DIO8016_ID,WL28661D_ID};

// Define the register map for each chip here
static const struct tranldo_register et5904_registers = {
	.rdis = 0x02,
	.ldo1vout = 0x03,
	.ldo2vout = 0x04,
	.ldo3vout = 0x05,
	.ldo4vout = 0x06,
	.seq1 = 0x0A,
	.seq2 = 0x0B,
	.enable = 0x0E,
};
// Add more linear ranges for each chip here as needed
static const struct linear_range et5904_linear_ranges[] = {
	REGULATOR_LINEAR_RANGE(600000,  0x00, 0xff, 6000),
	REGULATOR_LINEAR_RANGE(1200000, 0x00, 0xff, 12500),
};
static const struct linear_range sgm38121_linear_ranges[] = {
	REGULATOR_LINEAR_RANGE(528000,  0x03, 0x7d, 8000),
	REGULATOR_LINEAR_RANGE(1504000, 0x0f, 0xff, 8000),
};

static const struct tran_ldo_config tran_ldo_configs[] = {
	/*
	struct tran_ldo_config {
		int id;
		int n_linear_ranges;
		int n_voltages;
		int enable_time;
		int vsel_mask;
		const struct linear_range *linear_ranges;
		const struct  tranldo_register *reg;
	};
	*/
	{ET5904_ID,   1, 0xff, 150, 0xff,&et5904_linear_ranges[0],&et5904_registers},    // Configuration for LDOD
	{ET5904_ID,   1, 0xff, 150, 0xff,&et5904_linear_ranges[1],&et5904_registers},  // Configuration for LDOA
	{DIO8016_ID,  1, 0xff, 150, 0xff,&et5904_linear_ranges[0],&et5904_registers},    // Configuration for LDOD
	{DIO8016_ID,  1, 0xff, 150, 0xff,&et5904_linear_ranges[1],&et5904_registers},  // Configuration for LDOA
	{WL28661D_ID, 1, 0xff, 150, 0xff,&et5904_linear_ranges[0],&et5904_registers},    // Configuration for LDOD
	{WL28661D_ID, 1, 0xff, 150, 0xff,&et5904_linear_ranges[1],&et5904_registers},  // Configuration for LDOA
	{SGM38121_ID, 1, 0x7a, 150, 0xff,&sgm38121_linear_ranges[0],&et5904_registers},  // Configuration for LDOD  
	{SGM38121_ID, 1, 0xf0, 150, 0xff,&sgm38121_linear_ranges[1],&et5904_registers},// Configuration for LDOA
    // Add more configurations as needed
};
#endif