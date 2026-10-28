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
 * @file   ecc_curve.h
 * @date   January, 2021
 * @brief  Implementation of cryptographic ECC and ODC curve parameters
 */

#ifndef LIBS_AUTHON_SDK_CRYPTO_ECC_CURVE_H_
#define LIBS_AUTHON_SDK_CRYPTO_ECC_CURVE_H_

#include "../authon_config.h"
#include "../platform/authon_platform.h"


  /** curve parameters for ECC authentication protocol: */
  /** curve parameter a */
  static const uint32_t ecc_coeff_a_163[ARRAY_LEN(GF2_163)] =
  {0x0u, 0x0u, 0x0u, 0x0u, 0x0u, 0x0u};
  /** square root of curve parameter b */
  static const uint32_t ecc_coeff_sqrt_b_163[ARRAY_LEN(GF2_163)] =
  { 0x2834d5d2,  0xcb41edb6,  0xbe7418c9,  0x0b308003,  0x2f323f25,  0x0};
  /** affine x-coordinate of the base point */
  static const uint32_t ecc_xP_163[ARRAY_LEN(GF2_163)] =
   //{ 0x1a8b7f78u, 0x502959d8u, 0x2b894868u, 0x6c0356a7u, 0x8cdb7ffu, 0x7u };
  {0x9ce5acd7u, 0x879dc045u, 0xebc22ab8u, 0x15222b4du, 0x71c1f1b7u, 0x4u};
  /** affine y-coordinate of the base point */
  static const uint32_t ecc_yP_163[ARRAY_LEN(GF2_163)] =
  {0x76c3987au, 0xf0a0c7e0u, 0x75786b92u, 0x755aa34du, 0x4abc8133u, 0x0u};
  /* order of the base point 2923003274661805836407367994200320671150421777303 */
  static const uint32_t ecc_order_163[ARRAY_LEN(GF2_163)] =
  {0x30fb1b97u, 0x51856d95u, 0xfffe9e1au, 0xffffffffu, 0xffffffffu, 0x1u};

  /* Trace of Frobenius: -2138543718956608924763003 */


  curve_parameter_t ECC_CURVE_163[1] = {{
      /* degree       */ 	GF2_163,
      /* coeff_a      */ 	(uint32_t *)ecc_coeff_a_163,
      /* coeff_sqrt_b */	(uint32_t *)ecc_coeff_sqrt_b_163,
      /* base_point_x */  (uint32_t *)ecc_xP_163,
      /* base_point_y */  (uint32_t *)ecc_yP_163,
      /* order        */ 	(uint32_t *)ecc_order_163}};


  /** curve parameters for verification of ECDSA signed certificates: */
  /** curve parameter a */
  static const uint32_t certificate_coeff_a_193[ARRAY_LEN(GF2_193)] =
    {0x11df7b01, 0x98ac8a9, 0x7b4087de, 0x69e171f7, 0x7a989751, 0x17858feb, 0x0};
  /** square root of curve parameter b */
  static const uint32_t certificate_coeff_sqrt_b_193[ARRAY_LEN(GF2_193)] =
    {0x52fdfb06, 0xd43f8be7, 0xd24e42e9, 0x139483af, 0xddee67cd, 0xde5fb3d7, 0x1};
  /** affine x-coordinate of the base point */
  static const uint32_t certificate_xP_193[ARRAY_LEN(GF2_193)] =
    {0xd8c0c5e1, 0x79625372, 0xdef4bf61, 0xad6cdf6f, 0xff84a74, 0xf481bc5f, 0x1};
  /** affine y-coordinate of the base point */
  static const uint32_t certificate_yP_193[ARRAY_LEN(GF2_193)] =
    {0xf7ce1b05, 0xb3201b6a, 0x1ad17fb0, 0xf3ea9e3a, 0x903712cc, 0x25e399f2, 0x0};
  /** order of base point 6277101735386680763835789423269548053691575186051040197193 */
  static const uint32_t certificate_order_193[ARRAY_LEN(GF2_193)] =
    {0x920eba49, 0x8f443acc, 0xc7f34a77, 0x0, 0x0, 0x0, 0x1};

  curve_parameter_t certificate_193[1] = {{
      /* degree       */ GF2_193,
      /* coeff_a      */ (uint32_t *)certificate_coeff_a_193,
      /* coeff_sqrt_b */ (uint32_t *)certificate_coeff_sqrt_b_193,
      /* base_point_x */ (uint32_t *)certificate_xP_193,
      /* base_point_y */ (uint32_t *)certificate_yP_193,
      /* order        */ (uint32_t *)certificate_order_193}};

  /* ODC 193-bit ES public key */
  eccpoint_t ODC_193_ES_PubKeyXY =
  {
#if(ENGINEERING_SAMPLE_ODC==1)
    {0x18cd8e6b,0xaa470435,0x341a8cb5,0xdadc80ee,0x12837924,0x7725a8f0,0x00000000},
  	{0x8a91cfcb,0x073756ee,0xa447ef80,0x8e71ee59,0xdf682c4e,0x2497137f,0x00000000}
#else//For compilation only
    {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}
#endif
  };

#define PRODUCTIVE_ODC    0 
  /* ODC 193-bit Productive public key */
  eccpoint_t ODC_193_Productive_PubKeyXY =
  {
#if(PRODUCTIVE_ODC==1)
    {0xb24c1893, 0x35cdf079, 0x2ca9f30d, 0x43a76f8a, 0x97982d40, 0x7420fb0f, 0x00000001},
    {0x98fa2c25, 0x23de54e2, 0x98795bbf, 0x14c1bb2f, 0x95d09592, 0xa85342b5, 0x00000000}
#else//For compilation only
    {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}
#endif
  };

#endif /* LIBS_AUTHON_SDK_CRYPTO_ECC_CURVE_H_ */
