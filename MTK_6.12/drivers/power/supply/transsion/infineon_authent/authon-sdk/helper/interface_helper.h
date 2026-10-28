// SPDX-FileCopyrightText: 2024 Infineon Technologies AG
//
// SPDX-License-Identifier: MIT

/**
 * @file   helper.h
 * @date   January, 2021
 * @brief  Definition of platform functions
 */
#ifndef __HELPER_H__
#define __HELPER_H__

#include "../authon_api.h"

/* CRC functions */
uint16_t crc16_gen(uint8_t *data, uint16_t len);
void rand_byte(uint8_t *ub_rand, uint8_t ub_rand_len);
int gf2n_rand(uint32_t *  erg);

#endif /* __HELPER_H__ */
