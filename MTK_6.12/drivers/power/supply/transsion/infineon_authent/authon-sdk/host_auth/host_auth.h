// SPDX-FileCopyrightText: 2024 Infineon Technologies AG
//
// SPDX-License-Identifier: MIT

/**
 * @file   host_auth.h
 * @date   January, 2021
 * @brief  Implementation of host authentication routine
 */
#ifndef __HOST_AUTH_H__
#define __HOST_AUTH_H__

#include "../authon_api.h"

uint16_t get_random_number(struct authon_context *ctx, uint8_t *nonce_a);
uint16_t authon_exe_host_authentication(struct authon_context *ctx);

#endif /* __HOST_AUTH_H__ */
