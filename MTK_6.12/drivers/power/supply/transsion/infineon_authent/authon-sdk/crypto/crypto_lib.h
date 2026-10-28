#ifndef CRYPTO_LIB_H_
#define CRYPTO_LIB_H_
/***************************************************************************************
 * Copyright 2021 Infineon Technologies AG ( www.infineon.com ).                       *
 * All rights reserved.                                                                *
 *                                                                                     *
 * Licensed  Material-Property of Infineon Technologies AG.                            *
 * This software is made available solely pursuant to the terms of Infineon            *
 * Technologies AG agreement which governs its use. This code and the information      *
 * contained in it are proprietary and confidential to Infineon Technologies AG.       *
 * No person is allowed to copy, reprint, reproduce or publish any part of this code,  *
 * nor disclose its contents to others, nor make any use of it, nor allow or assist    *
 * others to make any use of it - unless by prior Written express authorization of     *
 * Infineon Technologies AG and then only to the extent authorized.                    *
 *                                                                                     *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,            *
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,           *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE DISCLAIMED.  IN NO       *
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,     *
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,                 *
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;         *
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY             *
 * WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR            *
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF              *
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                          *
 *                                                                                     *
 ***************************************************************************************/
/**
 * @file   crypto_lib.h
 * @date   August, 2021
 * @brief  Implementation of Crypto library routine
 */


#include "../authon_config.h"
#include "../platform/authon_platform.h"

/**
@brief Crypto library version.
*/
#define CRYPTO_VERSION   (0x1007u)

/**
@brief Crypto library release date.
*/
#define CRYPTO_DATE      (0x20221220u)

#define FIXED_CHALLENGE  (0)  //Set "1" for debugging purposes, generates an ECC fixed challenge. Set "0" to disable.
#define USE_FAST_MAC     (1)  //Set "1" for Fast MAC computation. Set "0" to use SBOX MAC.

typedef uint32_t mac_t[3];

/** sizes of finite fields GF(2^n) */
#define GF2_163 (163)
#define GF2_193 (193)

#define MAX_DEGREE (193)

#define ARRAY_LEN(A) (((A)+31)/32)

/** data type for elements in GF(2^n) and GF(p) */
typedef uint32_t dwordvec_t[ARRAY_LEN(MAX_DEGREE)];

typedef void (*func2_pt)(uint32_t *, const uint32_t *);
typedef void (*func3_pt)(uint32_t *, const uint32_t *, const uint32_t *);
/*---------------------------------------------------------------------------*/
/** arithmetic in the finite field GF(2^n) */

/* addition: out = op1 + op2 */
#define gf2n_add(A, B, C) actual_gf2n_add(A, B, C)

/* addition: out += op */
#define gf2n_sum(A, B) actual_gf2n_sum(A, B)

/* multiplication: out = op1 * op2 */
#define gf2n_mul(A, B, C) actual_gf2n_mul(A, B, C)

/* squaring: out = op^2 */
#define gf2n_square(A, B) actual_gf2n_square(A, B)

/** @brief data type for points on elliptic curve */
typedef struct {
    dwordvec_t x_coord;
    dwordvec_t y_coord;
} eccpoint_t;

/** @brief data type for elliptic curve parameters */
typedef struct {
unsigned int degree;    /* extension degree n of the finite field GF(2^n) */
uint32_t *coeff_a;      /* parameter a of the elliptic curve y^2+xy = x^3+ax^2+b */
uint32_t *coeff_sqrt_b; /* square root of parameter b of the elliptic curve y^2+xy = x^3+ax^2+b */
uint32_t *base_point_x; /* x-coordinate of the base point */
uint32_t *base_point_y; /* y-coordinate of the base point */
uint32_t *order;        /* prime order of the base point */
} curve_parameter_t;

#define  MAC_BYTE_LEN 			(10)

/** @brief data type for signature */
typedef struct {
	/** r value of signature */
	dwordvec_t r_value;
	/** s value of signature */
	dwordvec_t s_value;
} signature_t;


  

#define ONE8 0xFFU
#define T8(x) ((x) & ONE8)

  /** conversion character string to word big endian */
#define BIG_U8TO32(c) (((uint32_t)T8(*(c))<<24) | ((uint32_t)T8(*((c)+1))<<16) | \
                       ((uint32_t)T8(*((c)+2))<<8) | ((uint32_t)T8(*((c)+3))))

  /** conversion word to character string big endian */
#define BIG_U32TO8(c, v) do {            \
    uint32_t common_h_tmp_x = (v);       \
    uint8_t *common_h_tmp_d = (c);                               \
    common_h_tmp_d[0] = (uint8_t)T8(common_h_tmp_x >> 24);       \
    common_h_tmp_d[1] = (uint8_t)T8(common_h_tmp_x >> 16);       \
    common_h_tmp_d[2] = (uint8_t)T8(common_h_tmp_x >> 8);        \
    common_h_tmp_d[3] = (uint8_t)T8(common_h_tmp_x);             \
  } while (0)


/* Crypto library version */
uint16_t authon_get_crypto_version(uint16_t* version, uint32_t* date);

/* Hash functions */
void sha256(uint8_t *hash_value, const uint8_t *input_data, const uint32_t input_length);

/* ECC crypto API */
void ecc_init(curve_parameter_t *p);
int generate_challenge(dwordvec_t xA, dwordvec_t lambda, curve_parameter_t *curve);
int generate_check_value(dwordvec_t xC, const dwordvec_t lambda, const dwordvec_t xT, curve_parameter_t *curve);
void process_response( uint8_t *resp, uint16_t len );
int ECDSA_verify(const signature_t *sig, const uint8_t *hash_data, const eccpoint_t *pub_key, curve_parameter_t *curve);
int verify_ecc_mac(const mac_t mac_value, const dwordvec_t Z, const dwordvec_t xC, mac_t data, curve_parameter_t *curve);
uint8_t do_mac(mac_t mac_result, const mac_t data, const dwordvec_t z, const dwordvec_t checkval, curve_parameter_t *curve);

/* Host Authentication crypto API */
uint16_t HA_GenerateTagA(uint8_t *ub_tagA, uint8_t *ub_nonceA, uint8_t *ub_nonceB);
uint16_t VerifyTagB(uint8_t *ub_tagB, uint8_t *ub_nonceA, uint8_t *ub_nonceB);
void ha_derive_session_key(dwordvec_t k_star, const dwordvec_t na, const dwordvec_t nb, const dwordvec_t k);

/* MAC crypto API */
void MacData2Byte(mac_t mac_data, uint8_t *mac_o);
void Get_HA_MAC_key(dwordvec_t ha_mac_key);
void Get_AUTHKILL_PASSWORD(uint32_t * password);
void Get_Reset_Password(uint32_t *reset_password);
#endif /* CRYPTO_LIB_H_ */
