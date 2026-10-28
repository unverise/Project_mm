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
 * @file   crypto_lib.c
 * @date   May, 2021
 * @brief  Implementation of cryptographic library
 */

#include "../helper/interface_helper.h"
#include "../authon_config.h"
#include "../authon_status.h"
#include "crypto_lib.h"
#include "ecc_curve.h"

#define USE_PRECOMP_2P_3P                        (1) /*! Use precomputed points for 2P/3P if set to 1, otherwise calculate on the fly. Memory/Runtime Tradeoff.*/

/** pointers to parameters of base point */
eccpoint_t actual_base_point[1];   /* coordinates of base point */
uint32_t *actual_base_point_order; /* order of the base point */

/** pointers to actual parameters of the elliptic curve */
uint32_t *actual_coeff_a;      /* curve parameter a */
uint32_t *actual_coeff_sqrt_b; /* square root of curve parameter b */

unsigned int actual_degree;
func2_pt actual_gf2n_sum, actual_gf2n_square, actual_dwordvec_copy;
func3_pt actual_gf2n_add, actual_gf2n_mul;

#define dwordvec_copy(A, B) actual_dwordvec_copy(A, B)

/** irreducible polynomial */
uint32_t *actual_irred_polynomial;

/* initialize function pointers dependent on extension degree of finite field */
static void gf2n_init(const unsigned int degree);

/* test if point is on curve */
static int ecc_point_on_curve(const eccpoint_t *p);

#if (USE_LD_COORDS==1)
/* Calculates the Product of a*P + b*Q using "Sharmir's Trick" with a sliding window of W=2. */
static int ecc_multiply_shamir_lopez_w2_sliding_point_x(dwordvec_t point_x, const eccpoint_t *P, const eccpoint_t *Q, dwordvec_t skalar_a, dwordvec_t skalar_b);
#else
/* scalar elliptic curve multiplication with projective result */
static void ecc_mul_projective(dwordvec_t X, dwordvec_t Y, dwordvec_t Z, const eccpoint_t *p, const dwordvec_t scalar);

/* compute affine x-coordinate of sum of two projective points */
static int ecc_add_point_x(dwordvec_t point_x, const dwordvec_t X1, const dwordvec_t Y1, const dwordvec_t Z1, const dwordvec_t X2, const dwordvec_t Y2, const dwordvec_t Z2);
#endif

/*---------------------------------------------------------------------------*/
/** arithmetic in the finite field GF(p) */

/* addition: c = a + b */
static void dwordvec_add(dwordvec_t c, const dwordvec_t a, const dwordvec_t b);

/* subtraction: c = a - b */
static uint32_t dwordvec_sub(dwordvec_t c, const dwordvec_t a, const dwordvec_t b);

/* division: x = a / b */
static void gfp_divide(dwordvec_t x, const dwordvec_t a, const dwordvec_t b);

/*---------------------------------------------------------------------------*/
/* other useful functions */
/* division: t1 = op1 / op2 */
static int gf2n_divide(dwordvec_t t1, const dwordvec_t op1, const dwordvec_t op2);

/* scalar elliptic curve multiplication */
static void mont_ecc_mul(dwordvec_t A, dwordvec_t B, dwordvec_t C, dwordvec_t D, const dwordvec_t point_x, const dwordvec_t scalar);

static void mac_core_80(mac_t a,  mac_t b);
static void auth_mac_algorithm_80(mac_t mac_value, mac_t key_state, const mac_t data, const mac_t session_key);
static void mac_algorithm_80(mac_t mac_value, const mac_t data, const mac_t session_key);

static void AMAC_round(dwordvec_t REG_A, dwordvec_t CHALL);
static void amac_compute_tag(dwordvec_t tag, dwordvec_t mac_key,  dwordvec_t mac_first_input,  dwordvec_t mac_second_input);

/* check if element is zero */
static int dwordvec_iszero(const dwordvec_t el);

/* comparison of two uint32 vectors */
static int dwordvec_cmp(const dwordvec_t a, const dwordvec_t b);

static void Byte2Dword(dwordvec_t data_dword, uint8_t *byte_in, uint8_t din_len);

#if (USE_FAST_MAC==0)
static void SBOX(uint32_t box, uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t *w, uint32_t*x, uint32_t* y,uint32_t *z);
#endif

/** constant field element with value 1*/
static const dwordvec_t one_element = {0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

/** constant field element with value 0*/
static const dwordvec_t zero_element = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

/** constant for the ODC Curve parameters*/
#if(USE_PRECOMP_2P_3P==1)
		static const dwordvec_t P2X = {0x13061ce7, 0xca197c14, 0x5b552d2e, 0x0746c04b, 0xb549cf95, 0x7f9311aa, 0x00};
		static const dwordvec_t P2Y = {0x8597b1e4, 0xa1c16b34, 0x95543b76, 0xe92f12b6, 0x872508bd, 0x2fefafd1, 0x00};
		static const dwordvec_t P3X = {0xc5ebc8e1, 0x0fcb037e, 0x659d115d, 0x38ffc105, 0xa5ee5f7d, 0x7a1644d5, 0x01};
		static const dwordvec_t P3Y = {0x09a3b990, 0x4c9b0c52, 0x17a4924e, 0x7f3be5c7, 0xa89575d9, 0xea116683, 0x01};
#endif
/**
* @brief Return Crypto library version and release date.
* @param version Crypto version value
* @param date Date release date.
*/
uint16_t authon_get_crypto_version(uint16_t* version, uint32_t* date) {
	uint16_t crypto_version = CRYPTO_VERSION;
	uint32_t crypto_date = CRYPTO_DATE;

	if (version == NULL) {
		return FALSE;
	}
	memcpy(version, &crypto_version, sizeof(crypto_version));

	if (date == NULL) {
		return FALSE;
	}
	memcpy(date, &crypto_date, sizeof(crypto_date));

	return TRUE;
}


/** elliptic curve point addition */
static void mont_ecc_add(dwordvec_t E, dwordvec_t F, const dwordvec_t A, const dwordvec_t B, const dwordvec_t C,
                         const dwordvec_t D, const dwordvec_t point_x) {
  dwordvec_t G;

  gf2n_mul(G, A, D);
  gf2n_mul(F, B, C);
  gf2n_mul(E, G, F);
  gf2n_sum(F, G);
  gf2n_square(F, F);
  gf2n_mul(G, point_x, F);
  gf2n_sum(E, G);
}


/** elliptic curve point doubling */
static void mont_ecc_double(dwordvec_t E, dwordvec_t F, const dwordvec_t A, const dwordvec_t B) {
  dwordvec_t C;

  gf2n_square(E, A);
  gf2n_square(F, B);
  gf2n_mul(C, actual_coeff_sqrt_b, F);
  gf2n_mul(F, E, F);
  gf2n_sum(E, C);
  gf2n_square(E, E);
}

/** scalar elliptic curve multiplication
  * \param[out] A, B, C, D projective x-coordinates of scalar*G and (scalar+1)*G
  * \param[in]  point_x affine x-coordinate of the point G
  * \param[in]  scalar scalar
  */
static void mont_ecc_mul(dwordvec_t A, dwordvec_t B, dwordvec_t C, dwordvec_t D, const dwordvec_t point_x, const dwordvec_t scalar) {
  dwordvec_t E, F;
  int i;

  /* initialization of coordinates */
  dwordvec_copy(A, one_element);
  dwordvec_copy(B, zero_element);
  dwordvec_copy(C, point_x);
  dwordvec_copy(D, one_element);
  /* main loop */
  // Find highest bit in skalar
  for (i = actual_degree-1; i >= 0; i--)
    if (scalar[i/32] & 1UL<<i%32) break;

  // double montgomery ladder
  while (i >= 0) {
	  // if bit of skalar is set
    if (scalar[i/32] & 1UL<<i%32) {
      if (i & 0x1) {
        mont_ecc_add(E, F, A, B, C, D, point_x);
        mont_ecc_double(C, D, C, D);
        dwordvec_copy(A, E);
        dwordvec_copy(B, F);
      } else {
        mont_ecc_double(E, F, C, D);
        mont_ecc_add(A, B, A, B, C, D, point_x);
        dwordvec_copy(C, E);
        dwordvec_copy(D, F);
      }
    } else {
      if (i & 0x1) {
        mont_ecc_add(E, F, A, B, C, D, point_x);
        mont_ecc_double(A, B, A, B);
        dwordvec_copy(C, E);
        dwordvec_copy(D, F);
      } else {
        mont_ecc_double(E, F, A, B);
        mont_ecc_add(C, D, A, B, C, D, point_x);
        dwordvec_copy(A, E);
        dwordvec_copy(B, F);
      }
    }
    i--;
  }
}

#if(USE_LD_COORDS == 0)
/** scalar elliptic curve muliplication with projective result
  * \param[out] X x-coordinate of projective point
  * \param[out] Y y-coordinate of projective point
  * \param[out] Z z-coordinate of projective point
  * \param[in] point_x x-coordinate of affine point
  * \param[in] point_y y-coordinate of affine point
  * \param[in] scalar scalar
  */
static void ecc_mul_projective(dwordvec_t X, dwordvec_t Y, dwordvec_t Z, const eccpoint_t *p, const dwordvec_t scalar) {
  dwordvec_t A, B, C, D, t1, t2, t3, t4;

  mont_ecc_mul(A, B, C, D, p->x_coord, scalar);
  if (dwordvec_iszero(B)) {
    /* result is point at infinity */
    dwordvec_copy(X, zero_element);
    dwordvec_copy(Y, one_element);
    dwordvec_copy(Z, zero_element);
    return;
  }
  if (dwordvec_iszero(D)) {
    /* result is -P */
    dwordvec_copy(X, p->x_coord);
    gf2n_add(Y, p->x_coord, p->y_coord);
    dwordvec_copy(Z, one_element);
    return;
  }
  
  //Resolve  the X/Y/Z Coords of a P by resolving P and P+1
  gf2n_mul(t1, B, D);           /* Z1*Z2 */
  gf2n_mul(t2, p->x_coord, B);  /* x*Z1 */
  gf2n_mul(Z, t1, t2);          /* x*Z1^2*Z2 */
  gf2n_mul(t3, p->x_coord, t1); /* x*Z1*Z2 */
  gf2n_mul(X, A, t3);           /* x*X1*Z1*Z2 */
  gf2n_sum(t2, A);              /* X1+x*Z1 */
  gf2n_mul(t3, p->x_coord, D);  /* x*Z2 */
  gf2n_sum(t3, C);              /* X2+x*Z2 */
  gf2n_mul(t3, t3, t2);         /* (X1+x*Z1)*(X2+x*Z2) */
  gf2n_square(t4, p->x_coord);  /* x^2 */
  gf2n_sum(t4, p->y_coord);     /* x^2+y */
  gf2n_mul(t1, t1, t4);         /* (x^2+y)*Z1*Z2 */
  gf2n_sum(t3, t1);             /* (X1+x*Z1)*(X2+x*Z2)+(x^2+y)*Z1*Z2 */
  gf2n_mul(t2, t2, t3);         /* (X1+x*Z1)*((X1+x*Z1)*(X2+x*Z2)+(x^2+y)*Z1*Z2) */
  gf2n_mul(t1, p->y_coord, Z);  /* x*y*Z1^2*Z2 */
  gf2n_add(Y, t1, t2);          /* (X1+x*Z1)*((X1+x*Z1)*(X2+x*Z2)+(x^2+y)*Z1*Z2)+x*y*Z1^2*Z2 */
}

/** \brief compute affine x-coordinate of sum of two projective points
  * \param[out] point_x affine x-coordinate of sum of the points
  * \param[in] X1 projective x-coordinate of first point
  * \param[in] Y1 projective y-coordinate of first point
  * \param[in] Z1 projective z-coordinate of first point
  * \param[in] X2 projective x-coordinate of second point
  * \param[in] Y2 projective y-coordinate of second point
  * \param[in] Z2 projective z-coordinate of second point
  * \return TRUE if sum is point at infinity or an error occurs, FALSE otherwise
  */
static int ecc_add_point_x(dwordvec_t point_x, const dwordvec_t X1, const dwordvec_t Y1, const dwordvec_t Z1,
                    const dwordvec_t X2, const dwordvec_t Y2, const dwordvec_t Z2) {
  dwordvec_t t1, t2, t3, t4, t5;

  /* cases with point at infinity */
  if (dwordvec_iszero(Z1)) {
    if (dwordvec_iszero(Z2)) return TRUE;
    gf2n_divide(point_x, X2, Z2);
    return FALSE;
  }
  if (dwordvec_iszero(Z2)) {
    gf2n_divide(point_x, X1, Z1);
    return FALSE;
  }
  /* compare affine x- and y-coordinates */
  gf2n_mul(t1, Y1, Z2);
  gf2n_mul(t2, Y2, Z1);
  gf2n_sum(t1, t2);     /* Y1*Z2+Y2*Z1 */
  gf2n_mul(t2, X1, Z2);
  gf2n_mul(t3, X2, Z1);
  gf2n_sum(t2, t3);     /* X1*Z2+X2*Z1 */
  if (dwordvec_iszero(t2)) {
    /* x-ccordinates are equal */
    if (!dwordvec_iszero(t1))  return TRUE; /* case P + (-P) */
    /* case doubling point */
    gf2n_square(t2, Z1);
    gf2n_square(t3, X1);
    gf2n_mul(t1, t2, actual_coeff_sqrt_b);
    gf2n_sum(t1, t3);
    gf2n_square(t1, t1);   /* (X1^2+coeff_sqrt_b*Z1^2)^2 */
    gf2n_mul(t2, t2, t3);  /* X1^2*Z1^2 */
    return gf2n_divide(point_x, t1, t2); /* TRUE, when doubling point of order 2 */
  }
  gf2n_add(t3, t1, t2);
  gf2n_mul(t3, t3, t1);             /* (Y1*Z2+Y2*Z1)^2+(Y1*Z2+Y2*Z1)*(X1*Z2+X2*Z1) */
  gf2n_square(t4, t2);              /* (X1*Z2+X2*Z1)^2 */
  gf2n_mul(t2, t2, t4);             /* (X1*Z2+X2*Z1)^3 */
  gf2n_mul(t5, actual_coeff_a, t4); /* a*(X1*Z2+X2*Z1)^2 */
  gf2n_sum(t3, t5);                 /* (Y1*Z2+Y2*Z1)^2+(Y1*Z2+Y2*Z1)*(X1*Z2+X2*Z1)+a*(X1*Z2+X2*Z1)^2 */
  gf2n_mul(t5, Z1, Z2);             /* Z1*Z2 */
  gf2n_mul(t3, t3, t5);             /* Z1*Z2*((Y1*Z2+Y2*Z1)^2+(Y1*Z2+Y2*Z1)*(X1*Z2+X2*Z1)+a*(X1*Z2+X2*Z1)^2) */
  gf2n_sum(t3, t2);                 /* Z1*Z2*((Y1*Z2+Y2*Z1)^2+(Y1*Z2+Y2*Z1)*(X1*Z2+X2*Z1)+a*(X1*Z2+X2*Z1)^2)+(X1*Z2+X2*Z1)^3 */
  gf2n_mul(t4, t5, t4);             /* Z1*Z2*(X1*Z2+X2*Z1)^2 */
  gf2n_divide(point_x, t3, t4);
  return FALSE;
}
#endif

/** \brief check the integrity of an affine point
  * \param[in] p point of an elliptic curve
  * \return TRUE if P is on the elliptic curve, FALSE otherwise
  */
static int ecc_point_on_curve(const eccpoint_t *p) {
  dwordvec_t t1, t2;

  gf2n_square(t1, p->x_coord);              /* x^2 */
  gf2n_add(t2, p->x_coord, actual_coeff_a); /* x+a */
  gf2n_mul(t1, t1, t2);                     /* x^3+a*x^2 */
  gf2n_square(t2, actual_coeff_sqrt_b);     /* b */
  gf2n_sum(t1, t2);                         /* x^3+ax^2+b */
  gf2n_add(t2, p->x_coord, p->y_coord);     /* x*y */
  gf2n_mul(t2, t2, p->y_coord);             /* y^2+xy */
  gf2n_sum(t1, t2);                         /* x^3+ax^2+b+y^2+xy */
  return dwordvec_iszero(t1);
}

/** \brief choose elliptic curve and initialize global variables
  * \param[in] p pointer to structure with parameters
  */
void ecc_init(curve_parameter_t *p) {
  gf2n_init(p->degree);
  actual_coeff_a = p->coeff_a;
  actual_coeff_sqrt_b = p->coeff_sqrt_b;
  dwordvec_copy(actual_base_point->x_coord, p->base_point_x);
  if (p->base_point_y)
    dwordvec_copy(actual_base_point->y_coord, p->base_point_y);
  actual_base_point_order = p->order;
}


static uint8_t ChallengeByteEncode(uint8_t byteIn)
{
// challenge encoding
#define ChanEncodeB7  3
#define ChanEncodeB6  4
#define ChanEncodeB5  6
#define ChanEncodeB4  0
#define ChanEncodeB3  1
#define ChanEncodeB2  5
#define ChanEncodeB1  7
#define ChanEncodeB0  2

	uint8_t ByteEncode = 0;
	uint8_t data;

	// bit 7 after encoding
	data = byteIn;
	data= (data >> ChanEncodeB7) & 0x01;
	ByteEncode |= data << 7;

	// bit 6 after encoding
	data = byteIn;
	data= (data >> ChanEncodeB6) & 0x01;
	ByteEncode |= data << 6;

	// bit 5 after encoding
	data = byteIn;
	data= (data >> ChanEncodeB5) & 0x01;
	ByteEncode |= data << 5;

	// bit 4 after encoding
	data = byteIn;
	data= (data >> ChanEncodeB4) & 0x01;
	ByteEncode |= data << 4;

	// bit 3 after encoding
	data = byteIn;
	data= (data >> ChanEncodeB3) & 0x01;
	ByteEncode |= data << 3;

	// bit 2 after encoding
	data = byteIn;
	data= (data >> ChanEncodeB2) & 0x01;
	ByteEncode |= data << 2;

	// bit 1 after encoding
	data = byteIn;
	data= (data >> ChanEncodeB1) & 0x01;
	ByteEncode |= data << 1;

	// bit 0 after encoding
	data = byteIn;
	data= (data >> ChanEncodeB0) & 0x01;
	ByteEncode |= data;

#undef ChanEncodeB7
#undef ChanEncodeB6
#undef ChanEncodeB5
#undef ChanEncodeB4
#undef ChanEncodeB3
#undef ChanEncodeB2
#undef ChanEncodeB1
#undef ChanEncodeB0  

	return ByteEncode;

}

//Challenge encoding
static void Ecc_EncodeChlg( dwordvec_t Chlg )
{
	uint8_t i;

	Chlg[ARRAY_LEN(actual_degree)-1] &= 0x7;

	for(i=0;i<((actual_degree+7) >> 3);i++){
		*(((uint8_t *)Chlg)+i) = ChallengeByteEncode(*(((uint8_t *)Chlg)+i));
	}
}

//Response decoding
static uint8_t RespByteDecode(uint8_t byteIn)
{

// Response decoding
#define RespEncodeB7  3
#define RespEncodeB6  7
#define RespEncodeB5  4
#define RespEncodeB4  1
#define RespEncodeB3  5
#define RespEncodeB2  2
#define RespEncodeB1  6
#define RespEncodeB0  0

	uint8_t ByteDecode = 0;
	uint8_t data;

	// bit 7 after decoding
	data = byteIn;
	data= (data >> RespEncodeB7) & 0x01;
	ByteDecode |= data << 7;

	// bit 6 after decoding
	data = byteIn;
	data= (data >> RespEncodeB6) & 0x01;
	ByteDecode |= data << 6;

	// bit 5 after decoding
	data = byteIn;
	data= (data >> RespEncodeB5) & 0x01;
	ByteDecode |= data << 5;

	// bit 4 after decoding
	data = byteIn;
	data= (data >> RespEncodeB4) & 0x01;
	ByteDecode |= data << 4;

	// bit 3 after decoding
	data = byteIn;
	data= (data >> RespEncodeB3) & 0x01;
	ByteDecode |= data << 3;

	// bit 2 after decoding
	data = byteIn;
	data= (data >> RespEncodeB2) & 0x01;
	ByteDecode |= data << 2;

	// bit 1 after decoding
	data = byteIn;
	data= (data >> RespEncodeB1) & 0x01;
	ByteDecode |= data << 1;

	// bit 0 after decoding
	data = byteIn;
	data= (data >> RespEncodeB0) & 0x01;
	ByteDecode |= data;

#undef RespEncodeB7
#undef RespEncodeB6
#undef RespEncodeB5
#undef RespEncodeB4
#undef RespEncodeB3
#undef RespEncodeB2
#undef RespEncodeB1
#undef RespEncodeB0

	return ByteDecode;

}

//Decode the response
void process_response( uint8_t *resp, uint16_t len )
{
	uint8_t i;

	for(i=0; i<len; i++){
		resp[i] = RespByteDecode(resp[i]);
	}
}

/* ------------------------------------------------------------------------- */
/** challenge generation
  * \param[out] lambda secret challenge scalar
  * \param[out] xA challenge, which is the affine x-coordinate
  *             of \a A = \a lambda * \a P
  * \param[in] curve parameters of the elliptic curve
  * \return FALSE if everything is ok, TRUE otherwise
  */
int generate_challenge(dwordvec_t xA, dwordvec_t lambda, curve_parameter_t *curve) {
  dwordvec_t A, B, C, D;

  /* initialize finite field and curve parameters */
  ecc_init(curve);

  /* compute challenge */
  if (gf2n_rand(lambda)){ 
    return TRUE;
  }

#if (ENABLE_DEBUG_PRINT>=2)
  PRINT("TRNG: %x %x %x %x %x %x %x\r\n", lambda[0],lambda[1],lambda[2],lambda[3],lambda[4],lambda[5],lambda[6]);
#endif

#if (FIXED_CHALLENGE==1)
  lambda[0]=1;
  lambda[1]=0;
  lambda[2]=0;
  lambda[3]=0;
  lambda[4]=0;
  lambda[5]=0;
  lambda[6]=0;
#endif

  mont_ecc_mul(A, B, C, D, actual_base_point->x_coord, lambda);

    //Encode the challenge
  if(gf2n_divide(xA, A, B)==0){
#if (ENABLE_DEBUG_PRINT>=2)
    PRINT(" CHL: %x %x %x %x %x %x %x\r\n", xA[0],xA[1],xA[2],xA[3],xA[4],xA[5],xA[6]);
#endif    
    //xA[6] = 0;
    Ecc_EncodeChlg( xA );
#if (ENABLE_DEBUG_PRINT>=2)
    PRINT("ECHL: %x %x %x %x %x %x %x\r\n", xA[0],xA[1],xA[2],xA[3],xA[4],xA[5],xA[6]);
#endif    
  }

  /* compute affine result */
  //return gf2n_divide(xA, A, B); //This part has been brought forward to be encoder function above
  return 0;
  
}

/* ------------------------------------------------------------------------- */
/** generate check value
  * \param[out] xC check value for the response, which is the affine x-coordinate
  *             of \a C = \a lambda * \a xT
  * \param[in] lambda secret challenge scalar
  * \param[in] xT public key, which is the affine x-coordinate of \a T = \a xiT * \a P
  * \param[in] curve parameters of the elliptic curve
  * \return FALSE if everything is ok, TRUE otherwise
  */
int generate_check_value(dwordvec_t xC, const dwordvec_t lambda, const dwordvec_t xT,
                         curve_parameter_t *curve) {
  dwordvec_t A, B, C, D;

  /* initialize finite field and curve parameters */
  ecc_init(curve);
  /* compute check value */
  mont_ecc_mul(A, B, C, D, xT, lambda);
  /* compute affine result */
  return gf2n_divide(xC, A, B);
}

/* ------------------------------------------------------------------------- */
/** verify MAC value
  * \param[in] mac_value MAC computed by the tag
  * \param[in] Z projective z-coordinate of the response of the tag
  * \param[in] xC expected response (i.e. check value), which is the affine
  *            x-coordinate of \a C = \a lambda * \a xT
  * \param[in] data data to be auhenticated (i.e. counter value)
  * \param[in] mac_id select appropriate mac algorithm
  * \param[in] curve parameters of the elliptic curve
  * \param[in] group MAC group
  * \return TRUE if mac/response is valid, FALSE otherwise
  */
int verify_ecc_mac(const mac_t mac_value, const dwordvec_t Z, const dwordvec_t xC, mac_t data, curve_parameter_t *curve) {
  dwordvec_t  host_mac_result;
  uint32_t t;

  do_mac(host_mac_result, data, Z, xC,  curve);

  t = host_mac_result[0] ^ mac_value[0];
  t |= host_mac_result[1] ^ mac_value[1];
  t |= host_mac_result[2] ^ mac_value[2];
  if (t) return FALSE;
  return TRUE;
}


/* ------------------------------------------------------------------------- */
/** do MAC on input data
  * \param[out] mac_result MAC result
  * \param[in] data data to be auhenticated (i.e. counter value)
  * \param[in] z projective z-coordinate of the response of the tag
  * \param[in] checkval expected response (i.e. check value), which is the affine
  *            x-coordinate of \a C = \a lambda * \a xT
  * \param[in] mac_id selects the mac algorithm
  * \param[in] curve parameters of the elliptic curve
  * \param[in] group MAC group
  * \return TRUE if mac/response is valid, FALSE otherwise
  */
uint8_t do_mac(mac_t mac_result, const mac_t data, const dwordvec_t z, const dwordvec_t checkval, curve_parameter_t *curve)
{
  dwordvec_t session_key;

  /* initialize finite field */
  gf2n_init(curve->degree);

  if (dwordvec_iszero(z)) {
    return FALSE;
  }
  gf2n_mul(session_key, checkval, z);

  //Compute the actual MAC
  mac_algorithm_80(mac_result, data, session_key);

  return TRUE;
 }

 #if (ENGINEERING_SAMPLE_MAC_KEY==1)
const dwordvec_t HA_MAC_key = {0xABDE6EC6, 0xA83ACD99,0x9646, 0x0, 0x0, 0x0, 0x0};
/*
For engineering sample debugging.
const dwordvec_t HA_MAC_key1 = {0x2505B39B, 0x4BF8FA1B, 0xECD0, 0x0, 0x0, 0x0, 0x0};
*/
#endif


void Get_HA_MAC_key(dwordvec_t ha_mac_key)
{
	memcpy(ha_mac_key, HA_MAC_key, sizeof(HA_MAC_key));
}

uint16_t HA_GenerateTagA(uint8_t *ub_tagA, uint8_t *ub_nonceA, uint8_t *ub_nonceB){
	dwordvec_t actual_key;
	dwordvec_t na,nb,tagA;

	dwordvec_t HA_MAC_key;
	Get_HA_MAC_key(HA_MAC_key);

	ecc_init(ECC_CURVE_163);
	Byte2Dword(na, ub_nonceA, MAC_BYTE_LEN);
	Byte2Dword(nb, ub_nonceB, MAC_BYTE_LEN);

	// Obtain K*
	ha_derive_session_key(actual_key, na, nb, HA_MAC_key);
	// Compute two tags
	amac_compute_tag(tagA, actual_key, nb, na);
	MacData2Byte(tagA, ub_tagA);

	return APP_HA_SUCCESS;
}

uint16_t VerifyTagB(uint8_t *ub_tagB, uint8_t *ub_nonceA, uint8_t *ub_nonceB ) {
	dwordvec_t tagB_in;
	dwordvec_t tagB_host;
	dwordvec_t actual_key;
	dwordvec_t na, nb;
	dwordvec_t HA_MAC_key;
	Get_HA_MAC_key(HA_MAC_key);

	ecc_init(ECC_CURVE_163);

	Byte2Dword(na, ub_nonceA, MAC_BYTE_LEN);
	Byte2Dword(nb, ub_nonceB, MAC_BYTE_LEN);
	Byte2Dword(tagB_in, ub_tagB, MAC_BYTE_LEN);

	// Obtain K*
	ha_derive_session_key(actual_key, na, nb, HA_MAC_key);
	// Compute two tags
	amac_compute_tag(tagB_host, actual_key, na, nb);

	if (dwordvec_cmp(tagB_in, tagB_host) != 0) {
		return APP_HA_E_TAGB;
	} else {		
    return APP_HA_SUCCESS;
	}
}

/** \brief perform ecdsa signature verification
  * \param[in] sig pointer to signature
  * \param[in] hash_data pointer to SHA-256 hash value
  * \param[in] pub_key pointer to public ecdsa key
  * \param[in] curve parameters of the elliptic curve
  * \return TRUE is signature is valid, FALSE otherwise
  */
int ECDSA_verify(const signature_t *sig, const uint8_t *hash_data, const eccpoint_t *pub_key, curve_parameter_t *curve) {
  int i;
  dwordvec_t hash, temp;
  uint32_t t1, t2;
  uint8_t *ptr;

#if (USE_LD_COORDS==0)
    dwordvec_t X1, Y1, Z1, X2, Y2, Z2;
#endif    

  /* initialize finite field and curve parameters */
  ecc_init(curve);
  /* check input parameters */
  if (dwordvec_iszero(sig->r_value)) return FALSE;
  if (dwordvec_cmp(sig->r_value, actual_base_point_order) >= 0) return FALSE;
  if (dwordvec_iszero(sig->s_value)) return FALSE;
  if (dwordvec_cmp(sig->s_value, actual_base_point_order) >= 0) return FALSE;
  if (!ecc_point_on_curve(pub_key)) return FALSE;
  if (!ecc_point_on_curve(actual_base_point)) return FALSE;
  /* convert hash value to dwordvec_t according to ANS X9.62-2005 7.4 Verifying Process */
  ptr = (uint8_t *)hash_data;
  t1 = 0UL;
  for (i = ARRAY_LEN(actual_degree)-1; i >= 0; i--) {
    t2 = BIG_U8TO32(ptr);
    hash[i] = t1<<(actual_degree%32) | t2>>(32-actual_degree%32);
    t1 = t2;
    ptr += 4;
  }
  /* reduce hash mod order */
  while (dwordvec_cmp(hash, actual_base_point_order) >= 0)
    dwordvec_sub(hash, hash, actual_base_point_order);
  /* verify signature */
  gfp_divide(temp, hash, sig->s_value);
  gfp_divide(hash, sig->r_value, sig->s_value); //variable hash is reused here for memory savings!

#if (USE_LD_COORDS==1)
  if(ecc_multiply_shamir_lopez_w2_sliding_point_x(temp, actual_base_point, pub_key, temp, hash)) return FALSE;
#else

  ecc_mul_projective(X1, Y1, Z1, actual_base_point, temp);
  ecc_mul_projective(X2, Y2, Z2, pub_key, hash);
  if (ecc_add_point_x(temp, X1, Y1, Z1, X2, Y2, Z2)) return FALSE;
#endif
  /* reduce temp mod order */
  while (dwordvec_cmp(temp, actual_base_point_order) >= 0)
    dwordvec_sub(temp, temp, actual_base_point_order);
  /* compare result with sig->r */
  gf2n_sum(temp, sig->r_value);
  return dwordvec_iszero(temp);
}

#if (USE_FAST_MAC==0)
#define BIT(A, B) (((A[B>>5]) >> (B&0x1f)) & 1)
#define XOR(A, B, C) ((A[C>>5]) ^= ((B)<<(C&0x1f)))
#endif

#define USE_ALTERNATIVE_SBOX_DESCRIPTION 1

#if (USE_FAST_MAC==0)
static void SBOX(uint32_t box, uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t *w, uint32_t*x, uint32_t* y,uint32_t *z){

	  uint32_t sbox7[] = { 1,13,15, 0,14, 8, 2,11, 7, 4,12,10, 9, 3, 5, 6};
	  uint32_t sbox4[] = { 1,15, 8, 3,12, 0,11, 6, 2, 5, 4,10, 9,14, 7,13 };
	  uint32_t sbox2[] = { 8, 6, 7, 9, 3,12,10,15,13, 1,14, 4, 0,11, 5, 2 };
	  uint32_t sbox5[] = {15, 5, 2,11, 4,10, 9,12, 0, 3,14, 8,13, 6, 7, 1}; // new in Authenticate ON

  uint32_t val = (a & 1) | (b & 1)<<1 | (c & 1)<<2 | (d & 1)<<3;

	  uint32_t res;

	  if (box==7) {
	    res = sbox7[val];
	  }else if (box==4) {
	    res = sbox4[val];
	  }else if (box==2) {
	    res = sbox2[val];
	  }else if (box==5) {
		  res = sbox5[val];
  }else return;

  *w = res &1;
  *x = (res >>1) &1;
  *y = (res >>2) &1;
  *z = (res >>3) &1;

}

static void mac_core_80_generic(mac_t a,  mac_t b, uint32_t overall_rounds, uint32_t phase1, uint32_t phase2, uint8_t sbox_sel){
  uint32_t i, t;
  uint32_t x0, x1, x2, x3, y0, y1, y2, y3, k0, k1;

  for (i = 0; i < overall_rounds ; i++) {

    /* extract key bits */
    k0 = BIT(b, 39);
    k1 = BIT(b, 79);


    //First we extract the highest bit at position 80
    t = (b[2] & 0x8000) >> 15; /*t contains bit 80 (MSB)*/

    //We just shift b2||b1||b0 to the left
    b[2] = (b[1]>>31 | b[2]<<1);
    b[2] = b[2] & 0xFFFF; //remove overflow
    b[1] = (b[0]>>31 | b[1]<<1);
    b[0] = (b[0]<<1);

    //Now we apply the feedback polynomial
    if (i < (phase1) || (i >= phase2)){ /* shift low byte by one and add feedback*/
      b[0] = b[0] ^ ((t)?0x905:0); /* if t=1 add x^80 + x^11 + x^8 + x^2 + 1 else 0*/
    }
    else{
      b[0] = ((b[0]) ^ ((t)?0xC009:0)); /* if t=1 x^80 + x^15 + x^14 + x^3 + 1 else 0*/
    }

    x0 = BIT(a, 38);
    x1 = BIT(a, 68);
    x2 = BIT(a, 73);
    x3 = BIT(a, 77);

    SBOX(sbox_sel, x0,x1,x2,x3,&y0,&y1,&y2,&y3);

    //Use feedback x^80 + x^9 + x^4 + x^2 + 1; thus 0x115 = 100010101
    t = ((a[2] & 0x8000) >> 15);
    a[2] = (a[1]>>31 | a[2]<<1) & 0xffff;
    a[1] = (a[0]>>31 | a[1]<<1);
    a[0] = ((a[0]<<1) ^ ((t)?0x215:0));

    /* non-linear feedback */
    XOR(a, y0, 5);
    XOR(a, y1, 11);
    XOR(a, y2, 13);
    XOR(a, y3, 42);

    //  /* add key bits */
    XOR(a, k0, 13);
    XOR(a, k1, 42);

  }
}
#endif

#if (USE_FAST_MAC==1)
/** MAC algorithm for data authentication
  * \param[out] mac_value result of MAC computation
  * \param[in]  data data to be authenticated
  * \param[in]  session_key key to be used for data authentication
  */
static void mac_core_80_fast(mac_t a,  mac_t b, int overall_rounds, int phase1, int phase2, uint32_t sbox_sel){

  uint32_t i, t;
  uint32_t x0, x1, x2, x3, y0, y1, y2, y3, k0, k1;
  if (overall_rounds != 4160 || phase1 != 1300 || phase2 != 2678 || sbox_sel != 5){
    return;
  }

  for (i = 0; i < 320; i++) {

    k0 = (((b[1] & 0xff)<<5) | (b[0] >> 27)) & 0x1fff;

    k1 = (b[2]>>3) & 0x1fff;
   b[2] = (((b[2]) << 13) | ((b[1] >> 19) & 0x1fff)) & 0xffff;
    b[1] = (b[1] << 13) | ((b[0] >> 19) & 0x1fff);
    b[0] = (b[0] << 13);

    if (i < (100) || (i >= 206)){
      b[0] = b[0] ^ (k1 << 11) ^ (k1 << 8) ^ (k1 << 2) ^ k1;
    }else{
      b[0] = b[0] ^ (k1 << 15) ^ (k1 << 14) ^ (k1 << 3) ^ k1;
    }

    x0 = (a[1]<<19 | a[0]>>13) >> 13;
    x1 = (a[2]<<21 | a[1]>>11) >> 13;
    x2 = (a[2]<<16 | a[1]>>16) >> 13;
    x3 = (a[2]<<12 | a[1]>>20) >> 13;

    y0 =  (x0 & x1) ^ (x0 & x3) ^ (x1 & x3) ^ x1 ^ x2 ^ x3 ^ 0xffffffff;
    y1 =  ((x0 & x1) & x3) ^ (x0 & x1) ^ x0 ^ (x1 & x3) ^ (x2 & x3) ^ x2 ^ x3 ^ 0xffffffff;
    y2 =  ((x0 & x1) & x3) ^ (x0 & x2 & x3) ^ (x0 & x2) ^ (x1 & x2 & x3) ^ x1 ^ (x2 & x3) ^ x3 ^ 0xffffffff;
    y3 =  ((x0 & x1) & x2) ^ (x0 & x2 & x3) ^ (x0 & x3) ^ x0 ^ x1 ^ x2 ^ x3 ^ 0xffffffff;

    y0 = y0 & 0x1fff;
    y1 = y1 & 0x1fff;
    y2 = y2 & 0x1fff;
    y3 = y3 & 0x1fff;

    t = (a[2]>>3) & 0x1fff;
    a[2] = (((a[2]) << 13) | (a[1] >> 19)) & 0xffff;
    a[1] = (a[1] << 13) | ((a[0] >> 19) & 0x1fff);
    a[0] = (a[0] << 13);
    a[0] = a[0] ^ (t << 9) ^ (t << 4) ^ (t << 2) ^ t;

    a[0] ^= y0 << 5 ^ y1 << 11 ^ y2 << 13;
    a[1] ^= y3 << 10;

    a[0] ^= k0 << 13;
    a[1] ^= k1 << 10;

  }
}
#endif

/** MAC data formatting
  * \param[in] a a data
  * \param[out] b data
  * \param[out] group value
  */
static void mac_core_80(mac_t a,  mac_t b) {

      uint32_t overall_rounds;
      uint32_t phase1; //
      uint32_t phase2; //

      overall_rounds =	 3952+2*26+2*26+2*26+2*26;
      phase1 = 1300; //
      phase2 = 2626+2*26; //
#if (USE_FAST_MAC==0)
      mac_core_80_generic(a,  b,  overall_rounds, phase1,  phase2, 5);
#endif
#if (USE_FAST_MAC==1)
      mac_core_80_fast(a,  b,  overall_rounds, phase1,  phase2, 5);
#endif
}

/** MAC data formatting
  * \param[in] mac_data original mac data in mac_t format
  * \param[out] mac_o data in byte format
  */
void MacData2Byte(mac_t mac_data, uint8_t *mac_o){
	uint8_t i;
	for(i = 0;i < MAC_BYTE_LEN;i ++){
		mac_o[i] = (mac_data[i>>2] >> (8 * (i & 0x3))) & 0xff;
	}
}

/** MAC algorithm for data authentication
  * \param[out] mac_value result of MAC computation
  * \param[in]  data data to be authenticated
  * \param[in]  session_key key to be used for data authentication
  */
static void mac_algorithm_80(mac_t mac_value, const mac_t data, const mac_t session_key) {
	mac_t a, b;

	a[0] = data[0];
	a[1] = data[1];
	a[2] = data[2] & 0xffff;
	b[0] = session_key[0];
	b[1] = session_key[1];
	b[2] = session_key[2] & 0xffff;

	mac_core_80(a, b);

	mac_value[0] = a[0] ^ session_key[0];
	mac_value[1] = a[1] ^ session_key[1];
	mac_value[2] = (a[2] ^ session_key[2]) & 0xffff;
}

/** Host authentication feature related function. Disable from authon_config.h if not used.
  */
static void Byte2Dword(dwordvec_t data_dword, uint8_t *byte_in, uint8_t din_len) {
	uint8_t i;
	uint8_t dword_ptr = 0;

	for (i = 0; i < 6; i++) {
		data_dword[i] = 0;
	}
	i = 0;
	do {
		data_dword[dword_ptr] |= (byte_in[i] << (8 * (i & 0x03)));
		i++;
		if ((i & 0x03) == 0) {
			dword_ptr++;
		}
	} while (i < din_len);
}

// Perform the session key derivation of the protocol (Na * k + Nb * K^2) and store in K_star
void ha_derive_session_key(dwordvec_t k_star, const dwordvec_t na, const dwordvec_t nb, const dwordvec_t k) {

	dwordvec_t tmp;

	// Compute K* = N_A * K + N_B * K^2
	gf2n_mul(k_star, na, k);
	gf2n_mul(tmp, nb, k);
	gf2n_mul(tmp, tmp, k);
	gf2n_add(k_star, k_star, tmp);
}

static void auth_mac_algorithm_80(mac_t mac_value, mac_t key_state, const mac_t data, const mac_t session_key) {

	mac_t a, b;
	//uint32_t i, t;
	//uint32_t x0, x1, x2, x3, y0, y1, y2, y3, k0, k1;

	a[0] = data[0];
	a[1] = data[1];
	a[2] = data[2] & 0xffff;
	b[0] = session_key[0];
	b[1] = session_key[1];
	b[2] = session_key[2] & 0xffff;

	mac_core_80(a, b);

	mac_value[0] = a[0];
	mac_value[1] = a[1];
	mac_value[2] = a[2];

	key_state[0] = b[0];
	key_state[1] = b[1];
	key_state[2] = b[2];
}


static void AMAC_round(dwordvec_t REG_A, dwordvec_t CHALL){
  mac_t mac_value, key_state, data, session_key;

  //data = CHALL
  data[0] = CHALL[0];
  data[1] = CHALL[1];
  data[2] = CHALL[2] & 0xFFFF;

  //session_key = REG_A
  session_key[0] = REG_A[0];
  session_key[1] = REG_A[1];
  session_key[2] = REG_A[2] & 0xFFFF;

  auth_mac_algorithm_80(mac_value, key_state, data, session_key);

  CHALL[0] = mac_value[0];
  CHALL[1] = mac_value[1];
  CHALL[2] = mac_value[2] & 0xFFFF;

  REG_A[0] = key_state[0];
  REG_A[1] = key_state[1];
  REG_A[2] = key_state[2] & 0xFFFF;
}


static void amac_compute_tag(dwordvec_t tag, dwordvec_t mac_key,  dwordvec_t mac_first_input,  dwordvec_t mac_second_input){

  dwordvec_t mac_lower; // [0:79]
  dwordvec_t mac_key_lower; // [0:79]
  dwordvec_t mac_higher; //[80:159]

  // Extract the lower part [0:79] of the 160-bit key k*
 mac_key_lower[0] = mac_key[0];
 mac_key_lower[1] = mac_key[1];
 mac_key_lower[2] = (mac_key[2]) & 0xffff;

 // Extract the higher part [80:159] of the 160-bit key k*
 mac_higher[0] = ((mac_key[3]<<(16)) | (mac_key[2] >> (16)))  & 0xffffffff;
 mac_higher[1] = ((mac_key[4]<<(16)) | (mac_key[3] >> (16)))  & 0xffffffff;
 mac_higher[2] = (mac_key[4] >> (16)) & 0x1ffff; //So that we can shift
 mac_higher[3] = 0;

 // Perform some fix to account for a hardware/control issue where higher part is shifted by one to the right
 mac_higher[0] = (((mac_higher[1]) & 1) <<31) | (mac_higher[0] >>1);
 mac_higher[1] = (((mac_higher[2]) & 1) <<31) | (mac_higher[1] >>1);
 mac_higher[2] = (((mac_higher[3]) & 1) <<31) | (mac_higher[2] >>1);
 mac_higher[2] = mac_higher[2] & 0xFFFF; //make sure that nothing is left

 // Add the first 80-bit data input to the lower part of K* and use this as 80-bit mac data register (mac_lower)
 gf2n_add(mac_lower, mac_key_lower, mac_first_input);

 // Do the first AMAC_round function
 AMAC_round(mac_higher, mac_lower);

 // Add the second 80-bit data input to the data register
 gf2n_add(mac_lower, mac_lower, mac_second_input);

 // Do the second AMAC_round function
 AMAC_round(mac_higher, mac_lower);

 // Do the key whitening where we add K*[0:79] to mac_lower to get the final tag
 gf2n_add(tag, mac_key_lower, mac_lower);

}

#define DOUBLE_ARRAY_LEN(A) ((2*(A)+31)/32)

typedef uint32_t double_dwordvec_t[DOUBLE_ARRAY_LEN(MAX_DEGREE)];

/** precomputed lookup table for faster squaring in GF(2^n) */
static const uint16_t square_tab[256] = {
  0x0, 0x1, 0x4, 0x5, 0x10, 0x11, 0x14, 0x15,
  0x40, 0x41, 0x44, 0x45, 0x50, 0x51, 0x54, 0x55,
  0x100, 0x101, 0x104, 0x105, 0x110, 0x111, 0x114, 0x115,
  0x140, 0x141, 0x144, 0x145, 0x150, 0x151, 0x154, 0x155,
  0x400, 0x401, 0x404, 0x405, 0x410, 0x411, 0x414, 0x415,
  0x440, 0x441, 0x444, 0x445, 0x450, 0x451, 0x454, 0x455,
  0x500, 0x501, 0x504, 0x505, 0x510, 0x511, 0x514, 0x515,
  0x540, 0x541, 0x544, 0x545, 0x550, 0x551, 0x554, 0x555,
  0x1000, 0x1001, 0x1004, 0x1005, 0x1010, 0x1011, 0x1014, 0x1015,
  0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051, 0x1054, 0x1055,
  0x1100, 0x1101, 0x1104, 0x1105, 0x1110, 0x1111, 0x1114, 0x1115,
  0x1140, 0x1141, 0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155,
  0x1400, 0x1401, 0x1404, 0x1405, 0x1410, 0x1411, 0x1414, 0x1415,
  0x1440, 0x1441, 0x1444, 0x1445, 0x1450, 0x1451, 0x1454, 0x1455,
  0x1500, 0x1501, 0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515,
  0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551, 0x1554, 0x1555,
  0x4000, 0x4001, 0x4004, 0x4005, 0x4010, 0x4011, 0x4014, 0x4015,
  0x4040, 0x4041, 0x4044, 0x4045, 0x4050, 0x4051, 0x4054, 0x4055,
  0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111, 0x4114, 0x4115,
  0x4140, 0x4141, 0x4144, 0x4145, 0x4150, 0x4151, 0x4154, 0x4155,
  0x4400, 0x4401, 0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415,
  0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451, 0x4454, 0x4455,
  0x4500, 0x4501, 0x4504, 0x4505, 0x4510, 0x4511, 0x4514, 0x4515,
  0x4540, 0x4541, 0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555,
  0x5000, 0x5001, 0x5004, 0x5005, 0x5010, 0x5011, 0x5014, 0x5015,
  0x5040, 0x5041, 0x5044, 0x5045, 0x5050, 0x5051, 0x5054, 0x5055,
  0x5100, 0x5101, 0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115,
  0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151, 0x5154, 0x5155,
  0x5400, 0x5401, 0x5404, 0x5405, 0x5410, 0x5411, 0x5414, 0x5415,
  0x5440, 0x5441, 0x5444, 0x5445, 0x5450, 0x5451, 0x5454, 0x5455,
  0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511, 0x5514, 0x5515,
  0x5540, 0x5541, 0x5544, 0x5545, 0x5550, 0x5551, 0x5554, 0x5555};

/** irreducible polynomials for different field sizes */
static const uint32_t irred_polynomial_163[ARRAY_LEN(GF2_163)] =
  {0xc9, 0x0, 0x0, 0x0, 0x0, 0x8};                              /* x^163+x^7+x^6+x^3+1 */
static const uint32_t irred_polynomial_193[ARRAY_LEN(GF2_193)] =
  {0x8001, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2};                       /* x^193+x^15+1 */

/** pointers to actual arithmetic functions for GF(2^n) */
static func2_pt actual_dwordvec_l_shift;

#define dwordvec_l_shift(A, B) actual_dwordvec_l_shift(A, B)

/* ------------------------------------------------------------------------- */

/** addition in the finite field GF(2^163)
  * \param[out] out output of addition
  * \param[in] op1 first operand
  * \param[in] op2 second operand
  */
static void gf2_163_add(dwordvec_t out, const dwordvec_t op1, const dwordvec_t op2) {
  out[0] = op1[0] ^ op2[0];
  out[1] = op1[1] ^ op2[1];
  out[2] = op1[2] ^ op2[2];
  out[3] = op1[3] ^ op2[3];
  out[4] = op1[4] ^ op2[4];
  out[5] = op1[5] ^ op2[5];
}

/** addition in the finite field GF(2^193)
  * \param[out] out output of addition
  * \param[in] op1 first operand
  * \param[in] op2 second operand
  */
static void gf2_193_add(dwordvec_t out, const dwordvec_t op1, const dwordvec_t op2) {
  out[0] = op1[0] ^ op2[0];
  out[1] = op1[1] ^ op2[1];
  out[2] = op1[2] ^ op2[2];
  out[3] = op1[3] ^ op2[3];
  out[4] = op1[4] ^ op2[4];
  out[5] = op1[5] ^ op2[5];
  out[6] = op1[6] ^ op2[6];
}

/* ------------------------------------------------------------------------- */
/** addition in the finite field GF(2^163)
  * \param[in,out] io output of addition and first operand
  * \param[in] op second operand
  */
static void gf2_163_sum(dwordvec_t io, const dwordvec_t op) {
  io[0] ^= op[0];
  io[1] ^= op[1];
  io[2] ^= op[2];
  io[3] ^= op[3];
  io[4] ^= op[4];
  io[5] ^= op[5];
}

/** addition in the finite field GF(2^193)
  * \param[in,out] io output of addition and first operand
  * \param[in] op second operand
  */
static void gf2_193_sum(dwordvec_t io, const dwordvec_t op) {
  io[0] ^= op[0];
  io[1] ^= op[1];
  io[2] ^= op[2];
  io[3] ^= op[3];
  io[4] ^= op[4];
  io[5] ^= op[5];
  io[6] ^= op[6];
}

/* ------------------------------------------------------------------------- */
/** reduction in the finite field GF(2^163) modulo x^163+x^7+x^6+x^3+1
  * \param[out] out reduced field element
  * \param[in] temp vector of uint32_t
  */
static void gf2_163_reduction(dwordvec_t out, const double_dwordvec_t temp) {
  int i;
  uint32_t t, t1, t2, t3;
  /* reduction modulo x^163+x^7+x^6+x^3+1 */
  /* x^192 = x^36+x^35+x^32+x^29 */

  t1 = t2 = 0UL;
  for (i = 0; i < DOUBLE_ARRAY_LEN(GF2_163)-ARRAY_LEN(GF2_163); i++) {
    t = temp[i+ARRAY_LEN(GF2_163)];
    t1 ^= t<<29; /* x^29 */
    t2 ^= t>>3 ^ t ^ t<<3 ^ t<<4; /* x^29+x^32+x^35+x^36 */
    t3 = t>>29 ^ t>>28; /* x^35+x^36 */
    out[i] = temp[i] ^ t1;
    t1 = t2;
    t2 = t3;
  }
  t2 = temp[ARRAY_LEN(GF2_163)-1] ^ t1;
  t = t2 & 0xfffffff8;
  out[ARRAY_LEN(GF2_163)-1] = t2 & 7UL;
  out[0] ^= t>>3 ^ t ^ t<<3 ^ t<<4; /* 1+x^3+x^6+x^7 */
  out[1] ^= t>>29 ^ t>>28; /* x^6+x^7 */
}

/** reduction in the finite field GF(2^193) modulo x^193+x^15+1
  * \param[out] out reduced field element
  * \param[in] temp vector of uint32_t
  */
static void gf2_193_reduction(dwordvec_t out, const double_dwordvec_t temp) {
  int i;
  uint32_t t, t1, t2, t3;
  /* reduction modulo x^193+x^15+1 */
  /* x^224 = x^46+x^31 */

  t1 = t2 = 0UL;
  for (i = 0; i < DOUBLE_ARRAY_LEN(GF2_193)-ARRAY_LEN(GF2_193); i++) {
    t = temp[i+ARRAY_LEN(GF2_193)];
    t1 ^= t<<31; /* x^31 */
    t2 ^= t>>1 ^ t<<14; /* x^31+x^46 */
    t3 = t>>18; /* x^46 */
    out[i] = temp[i] ^ t1;
    t1 = t2;
    t2 = t3;
  }
  t2 = temp[ARRAY_LEN(GF2_193)-1] ^ t1;
  t = t2 & 0xfffffffe;
  out[ARRAY_LEN(GF2_193)-1] = t2 & 1UL;
  out[0] ^= t>>1 ^ t<<14; /* 1+x^15 */
  out[1] ^= t>>18; /* x^15 */
}

/* ------------------------------------------------------------------------- */
/** multiply an element in GF(2^163) by x without reduction
  * \param[in] in GF(2^n) element
  * \param[out] out GF(2^n) element
  */
static void dwordvec_l_shift_163(dwordvec_t out, const dwordvec_t in) {
  out[5] = in[5]<<1 | in[4]>>31;
  out[4] = in[4]<<1 | in[3]>>31;
  out[3] = in[3]<<1 | in[2]>>31;
  out[2] = in[2]<<1 | in[1]>>31;
  out[1] = in[1]<<1 | in[0]>>31;
  out[0] = in[0]<<1;
}

/** multiply an element in GF(2^193) by x without reduction
  * \param[in] in GF(2^n) element
  * \param[out] out GF(2^n) element
  */
static void dwordvec_l_shift_193(dwordvec_t out, const dwordvec_t in) {
  out[6] = in[6]<<1 | in[5]>>31;
  out[5] = in[5]<<1 | in[4]>>31;
  out[4] = in[4]<<1 | in[3]>>31;
  out[3] = in[3]<<1 | in[2]>>31;
  out[2] = in[2]<<1 | in[1]>>31;
  out[1] = in[1]<<1 | in[0]>>31;
  out[0] = in[0]<<1;
}

/* ------------------------------------------------------------------------- */
/** precomputation for multiplication
  * \param[out] table table of precomputed field elements
  * \param[in] el field element
  */
static void precompute(dwordvec_t table[], const dwordvec_t el) {
  dwordvec_copy(table[0], zero_element);
  dwordvec_copy(table[1], el);
  dwordvec_l_shift(table[2], table[1]);
  dwordvec_l_shift(table[4], table[2]);
  dwordvec_l_shift(table[8], table[4]);
  gf2n_add(table[3], table[2], table[1]);
  gf2n_add(table[5], table[4], table[1]);
  gf2n_add(table[6], table[4], table[2]);
  gf2n_add(table[7], table[4], table[3]);
  gf2n_add(table[9], table[8], table[1]);
  gf2n_add(table[10], table[8], table[2]);
  gf2n_add(table[11], table[8], table[3]);
  gf2n_add(table[12], table[8], table[4]);
  gf2n_add(table[13], table[8], table[5]);
  gf2n_add(table[14], table[8], table[6]);
  gf2n_add(table[15], table[8], table[7]);
}

/* ------------------------------------------------------------------------- */
/** multiplication in the finite field GF(2^163) modulo x^163+x^7+x^6+x^3+1
  * \param[out] out result of multiplication
  * \param[in] op1 first operand
  * \param[in] op2 second operand
  */
static void gf2_163_mul(dwordvec_t out, const dwordvec_t op1, const dwordvec_t op2) {
  dwordvec_t table[16];
  double_dwordvec_t temp;
  int i=0;
  int j=0;
  int t=0;

  /* precomputation */
  precompute(table, op2);
  for (i = 0; i < DOUBLE_ARRAY_LEN(GF2_163); i++){
    temp[i] = 0;
  }

  /* multiplication */
  for (j = 28; j > 0; j -= 4) {
    for (i = 0; i < ARRAY_LEN(GF2_163)-1; i++) {
      t = op1[i]>>j & 0xf;
      gf2_163_sum(temp+i, table[t]);
    }
    for (i = DOUBLE_ARRAY_LEN(GF2_163)-1; i > 0; i--)
      temp[i] = temp[i]<<4 | temp[i-1]>>28;
    temp[0] <<= 4;
  }
  for (i = 0; i < ARRAY_LEN(GF2_163); i++) {
    t = op1[i] & 0xf;
    gf2_163_sum(temp+i, table[t]);
  }
  /* reduction */
  gf2_163_reduction(out, temp);
}

/** multiplication in the finite field GF(2^193) modulo x^193+x^15+1
  * \param[out] out result of multiplication
  * \param[in] op1 first operand
  * \param[in] op2 second operand
  */
static void gf2_193_mul(dwordvec_t out, const dwordvec_t op1, const dwordvec_t op2) {
  dwordvec_t table[16];
  double_dwordvec_t temp;
  
  int i=0; 
  int j=0;
  int t=0;

  /* precomputation */
  precompute(table, op2);

  for (i = 0; i < DOUBLE_ARRAY_LEN(GF2_193); i++){
    temp[i] = 0;
	}
  /* multiplication */
  for (j = 28; j > 0; j -= 4)
  {
    for (i = 0; i < ARRAY_LEN(GF2_193)-1; i++)
	{
      t = op1[i]>>j & 0xf;
      gf2_193_sum(temp+i, table[t]);
    }
    for (i = DOUBLE_ARRAY_LEN(GF2_193)-1; i > 0; i--)
      temp[i] = temp[i]<<4 | temp[i-1]>>28;
    temp[0] <<= 4;
  }
  for (i = 0; i < ARRAY_LEN(GF2_193); i++)
  {
    t = op1[i] & 0xf;
    gf2_193_sum(temp+i, table[t]);
  }
  /* reduction */
  gf2_193_reduction(out, temp);
}

/* ------------------------------------------------------------------------- */
/** squaring in the finite field GF(2^163) modulo x^163+x^7+x^6+x^3+1
  * \param[out] out result of squaring
  * \param[in] op operand
  */
static void gf2_163_square(dwordvec_t out, const dwordvec_t op) {
  uint32_t t;
  double_dwordvec_t temp;

  /* spreading by table lookup*/
  t = op[0];
  temp[0] = ((uint32_t)(square_tab[t & 0xff]) ^ ((uint32_t)(square_tab[t>>8 & 0xff])<<16));
  temp[1] = ((uint32_t)(square_tab[t>>16 & 0xff]) ^ ((uint32_t)(square_tab[t>>24])<<16));
  t = op[1];
  temp[2] = ((uint32_t)(square_tab[t & 0xff]) ^ ((uint32_t)(square_tab[t>>8 & 0xff])<<16));
  temp[3] = ((uint32_t)(square_tab[t>>16 & 0xff]) ^ ((uint32_t)(square_tab[t>>24])<<16));
  t = op[2];
  temp[4] = ((uint32_t)(square_tab[t & 0xff]) ^ ((uint32_t)(square_tab[t>>8 & 0xff])<<16));
  temp[5] = ((uint32_t)(square_tab[t>>16 & 0xff]) ^ ((uint32_t)(square_tab[t>>24])<<16));
  t = op[3];
  temp[6] = ((uint32_t)(square_tab[t & 0xff]) ^ ((uint32_t)(square_tab[t>>8 & 0xff])<<16));
  temp[7] = ((uint32_t)(square_tab[t>>16 & 0xff]) ^ ((uint32_t)(square_tab[t>>24])<<16));
  t = op[4];
  temp[8] = ((uint32_t)(square_tab[t & 0xff]) ^ ((uint32_t)(square_tab[t>>8 & 0xff])<<16));
  temp[9] = ((uint32_t)(square_tab[t>>16 & 0xff]) ^ ((uint32_t)(square_tab[t>>24])<<16));
  t = op[5];
  temp[10] = (uint32_t)(square_tab[t]);
  /* reduction */
  gf2_163_reduction(out, temp);
}

/** squaring in the finite field GF(2^193) modulo x^193+x^15+1
  * \param[out] out result of squaring
  * \param[in] op operand
  */
static void gf2_193_square(dwordvec_t out, const dwordvec_t op) {
  uint32_t t;
  double_dwordvec_t temp;

  /* spreading by table lookup*/
  t = op[0];
  temp[0] = ((uint32_t)(square_tab[t & 0xff]) ^ ((uint32_t)(square_tab[t>>8 & 0xff])<<16));
  temp[1] = ((uint32_t)(square_tab[t>>16 & 0xff]) ^ ((uint32_t)(square_tab[t>>24])<<16));
  t = op[1];
  temp[2] = ((uint32_t)(square_tab[t & 0xff]) ^ ((uint32_t)(square_tab[t>>8 & 0xff])<<16));
  temp[3] = ((uint32_t)(square_tab[t>>16 & 0xff]) ^ ((uint32_t)(square_tab[t>>24])<<16));
  t = op[2];
  temp[4] = ((uint32_t)(square_tab[t & 0xff]) ^ ((uint32_t)(square_tab[t>>8 & 0xff])<<16));
  temp[5] = ((uint32_t)(square_tab[t>>16 & 0xff]) ^ ((uint32_t)(square_tab[t>>24])<<16));
  t = op[3];
  temp[6] = ((uint32_t)(square_tab[t & 0xff]) ^ ((uint32_t)(square_tab[t>>8 & 0xff])<<16));
  temp[7] = ((uint32_t)(square_tab[t>>16 & 0xff]) ^ ((uint32_t)(square_tab[t>>24])<<16));
  t = op[4];
  temp[8] = ((uint32_t)(square_tab[t & 0xff]) ^ ((uint32_t)(square_tab[t>>8 & 0xff])<<16));
  temp[9] = ((uint32_t)(square_tab[t>>16 & 0xff]) ^ ((uint32_t)(square_tab[t>>24])<<16));
  t = op[5];
  temp[10] = ((uint32_t)(square_tab[t & 0xff]) ^ ((uint32_t)(square_tab[t>>8 & 0xff])<<16));
  temp[11] = ((uint32_t)(square_tab[t>>16 & 0xff]) ^ ((uint32_t)(square_tab[t>>24])<<16));
  t = op[6];
  temp[12] = (uint32_t)(square_tab[t]);
  /* reduction */
  gf2_193_reduction(out, temp);
}

/* ------------------------------------------------------------------------- */
/** right shift of a finite field element
  * \param[in,out] el finite field element
  */
static void r_shift(dwordvec_t el) {
  el[0] = el[0]>>1 | el[1]<<31;
  el[1] = el[1]>>1 | el[2]<<31;
  el[2] = el[2]>>1 | el[3]<<31;
  el[3] = el[3]>>1 | el[4]<<31;
  el[4] = el[4]>>1 | el[5]<<31;
  if (actual_degree == GF2_163) {
    el[5] >>= 1;
    return;
  }
  /* actual_degree == GF2_193 */
  el[5] = el[5]>>1 | el[6]<<31;
  el[6] >>= 1;
}

/* ------------------------------------------------------------------------- */
/** swapping of two finite field elements
  * \param[in,out] el1 finite field element
  * \param[in,out] el2 finite field element
  */
static void dwordvec_swap(dwordvec_t el1, dwordvec_t el2) {
  uint32_t t;

  t = el1[0];
  el1[0] = el2[0];
  el2[0] = t;
  t = el1[1];
  el1[1] = el2[1];
  el2[1] = t;
  t = el1[2];
  el1[2] = el2[2];
  el2[2] = t;
  t = el1[3];
  el1[3] = el2[3];
  el2[3] = t;
  t = el1[4];
  el1[4] = el2[4];
  el2[4] = t;
  t = el1[5];
  el1[5] = el2[5];
  el2[5] = t;
  if (actual_degree == GF2_163) return;
  t = el1[6];
  el1[6] = el2[6];
  el2[6] = t;
}

/* ------------------------------------------------------------------------- */
/** \brief comparison of two uint32 vectors
  * \param[in] a uint32 vector
  * \param[in] b uint32 vector
  * \return 1 if \a a > \a b, -1 if \a a < \a b, or 0 if \a a == \a b
  */
static int dwordvec_cmp(const dwordvec_t a, const dwordvec_t b) {
  int i;

  for (i = ARRAY_LEN(actual_degree)-1; i > 0; i--)
    if (a[i] != b[i]) break;
  if (a[i] > b[i]) return 1;
  if (a[i] < b[i]) return -1;
  return 0;
}

/* ------------------------------------------------------------------------- */
/** local function is part of gf2n_divide for GF(2^n)
  * \param[in,out] a element in GF(2^n)
  * \param[in,out] b element in GF(2^n)
  */
static void remove_x_power(dwordvec_t a, dwordvec_t b) {
  while(!(a[0] & 1UL)) {
    r_shift(a);
    if (b[0] & 1UL) gf2n_sum(b, actual_irred_polynomial);
    r_shift(b);
  }
}

/* ------------------------------------------------------------------------- */
/** division in GF(2^n)
  * \param[out] t1 result of division
  * \param[in] op1 nominator GF(2^n) field element
  * \param[in] op2 denominator GF(2^n) field element
  * \return TRUE if op2 is 0, FALSE otherwise
  */
static int gf2n_divide(dwordvec_t t1, const dwordvec_t op1, const dwordvec_t op2) {
  dwordvec_t r0, r1, t0;

  /* check division by zero */
  if (dwordvec_iszero(op2)) return TRUE;
  /* initialization */
  dwordvec_copy(r0, actual_irred_polynomial);
  dwordvec_copy(r1, op2);
  dwordvec_copy(t0, zero_element);
  dwordvec_copy(t1, op1);
  /* main loop */
  remove_x_power(r1, t1);
  while (!dwordvec_iszero(r0)) {
    remove_x_power(r0, t0);
    if (dwordvec_cmp(r1, r0) > 0) {
      dwordvec_swap(r1, r0);
      dwordvec_swap(t1, t0);
    }
    gf2n_sum(r0, r1);
    gf2n_sum(t0, t1);
  }
  return FALSE;
}

/* ------------------------------------------------------------------------- */
/** check if GF(2^n)-variable is zero
  * \param[in] el GF(2^n)-element
  * \return TRUE if el == 0, FALSE otherwise
  */
static int dwordvec_iszero(const dwordvec_t el) {
  uint32_t t;

  t = (el[0] | el[1]);
  t |= el[2];
  t |= el[3];
  t |= el[4];
  t |= el[5];
  if (actual_degree == GF2_193) t |= el[6];
  if (t) return FALSE;
  return TRUE;
}

/* ------------------------------------------------------------------------- */
/** copy an element of GF(2^163)
  * \param[out] copy copy of element
  * \param[in] in element
  */
static void dwordvec_163_copy(dwordvec_t copy, const dwordvec_t in) {
  copy[0] = in[0];
  copy[1] = in[1];
  copy[2] = in[2];
  copy[3] = in[3];
  copy[4] = in[4];
  copy[5] = in[5];
}

/** copy an element of GF(2^193)
  * \param[out] copy copy of element
  * \param[in] in element
  */
static void dwordvec_193_copy(dwordvec_t copy, const dwordvec_t in) {
  copy[0] = in[0];
  copy[1] = in[1];
  copy[2] = in[2];
  copy[3] = in[3];
  copy[4] = in[4];
  copy[5] = in[5];
  copy[6] = in[6];
}

/* ------------------------------------------------------------------------- */
/** choose finite field GF(2^n) and initialize global variables
  *  \param[in] degree degree of the field
  */
static void gf2n_init(const unsigned int degree) {
  if (degree == GF2_163) {
    actual_degree = GF2_163;
    actual_irred_polynomial = (uint32_t *)irred_polynomial_163;
    actual_dwordvec_l_shift = dwordvec_l_shift_163;
    actual_gf2n_sum = gf2_163_sum;
    actual_gf2n_square = gf2_163_square;
    actual_dwordvec_copy = dwordvec_163_copy;
    actual_gf2n_add = gf2_163_add;
    actual_gf2n_mul = gf2_163_mul;
    return;
  }
  actual_degree = GF2_193;
  actual_irred_polynomial = (uint32_t *)irred_polynomial_193;
  actual_dwordvec_l_shift = dwordvec_l_shift_193;
  actual_gf2n_sum = gf2_193_sum;
  actual_gf2n_square = gf2_193_square;
  actual_dwordvec_copy = dwordvec_193_copy;
  actual_gf2n_add = gf2_193_add;
  actual_gf2n_mul = gf2_193_mul;
}

/* ------------------------------------------------------------------------- */
/** \brief addition of two vectors of uint32_t
  * \param[in] a vector of uint32_t
  * \param[in] b vector of uint32_t
  * \param[out] c vector of uint32_t
  * \return none
  */
static void dwordvec_add(dwordvec_t c, const dwordvec_t a, const dwordvec_t b) {
  unsigned int i;
  uint32_t carry, t;

  t = a[0];                        /* safe the least significat limb of a (needed if c == a) */
  c[0] = a[0] + b[0];              /* add least significant limbs */
  carry = c[0] < t;                /* save carry of first addition */
  for (i = 1; i < ARRAY_LEN(actual_degree); i++) {
    t = a[i];                      /* save limb of a (needed if c == a) */
    c[i] = b[i] + carry;           /* add carry of preceeding loop to b */
    carry = c[i] < carry;          /* check carry */
    c[i] += t;                     /* add limb of a */
    carry |= c[i] < t;             /* check carry */
  }
}

/* ------------------------------------------------------------------------- */
/** \brief subtraction of two vectors of uint32_t
  * \param[in] a vector of uint32_t
  * \param[in] b vector of uint32_t
  * \param[out] c vector of uint32_t
  * \return borrow
  */
static uint32_t dwordvec_sub(dwordvec_t c, const dwordvec_t a, const dwordvec_t b) {
  unsigned int i;
  uint32_t borrow, t1, t2;

  t1 = a[0];                       /* safe the least significat limb of a (needed if c == a) */
  c[0] = a[0] - b[0];              /* add least significant limbs */
  borrow = c[0] > t1;              /* save borrow of first subtraction */
  for (i = 1; i < ARRAY_LEN(actual_degree); i++) {
    t1 = a[i];                     /* save limb of a (needed if c == a) */
    t2 = b[i];                     /* save limb of b (needed if c == b) */
    c[i] = a[i] - borrow;          /* subtract borrow of preceeding loop from a */
    borrow = c[i] > t1;            /* check borrow */
    t1 = c[i];
    c[i] -= t2;                    /* subtract limb of a */
    borrow |= c[i] > t1;           /* check borrow */
  }
  return borrow;
}

/* ------------------------------------------------------------------------- */
/** \brief local function is part of gfp_divide for GF(p)
  * \param[in,out] a finite field element
  * \param[in,out] b finite field element
  * \return none
  */
static void remove_2_power(dwordvec_t a, dwordvec_t b) {
  while(!(a[0] & 1UL)) {
    r_shift(a);
    if (b[0] & 1) dwordvec_add(b, b, actual_base_point_order);
    r_shift(b);
  }
}

/* ------------------------------------------------------------------------- */
/** \brief division of two finite field elements in GF(p)
  * \param[in] a finite field element
  * \param[in] b finite field element != 0
  * \param[out] x finite field element a/b mod p
  * \return none
  */
static void gfp_divide(dwordvec_t x, const dwordvec_t a, const dwordvec_t b) {
  uint32_t t;
  dwordvec_t u, v, y;

  /* initialization */
  dwordvec_copy(u, b);
  dwordvec_copy(v, actual_base_point_order);
  dwordvec_copy(x, a);
  dwordvec_copy(y, zero_element);
  /* main loop */
  remove_2_power(u, x); /* make u odd */
  while (!dwordvec_iszero(v)) {
    remove_2_power(v, y); /* make v odd */
    /* sort u and v */
    if (dwordvec_cmp(u, v) > 0) {
      dwordvec_swap(u, v);
      dwordvec_swap(x, y);
    }
    /* subtract v = v-u and y = y-x mod p */
    dwordvec_sub(v, v, u);
    t = dwordvec_sub(y, y, x);
    if (t) dwordvec_add(y, y, actual_base_point_order);
  }
}

#if(USE_LD_COORDS==1)
/**
 * Add two ECC Points in affine coordinate system
 * NOTE: Does not check whether the two points are identical due to performance reasons!
 * @param[out] out_x x-coordinate of result
 * @param[out] out_y y-coordinate of result
 * @param[in] x1 x-coordinate of first affine point
 * @param y1 y-coordinate of first affine point
 * @param x2 x-coordinate of second affine point
 * @param y2 y-coordinate of second affine point
 */
static void ecc_add_affine_coords(dwordvec_t out_x, dwordvec_t out_y,
                          const dwordvec_t x1,
                          const dwordvec_t y1,
                          const dwordvec_t x2,
                          const dwordvec_t y2)
{
    dwordvec_t x3, y3;
    dwordvec_t lambda, t1, t2, t3;
    // CALC X Coord
    //t1 = y1+y2
    gf2n_add(t1, y1, y2);
    //t2 = x1+x2
    gf2n_add(t2, x1, x2);
    gf2n_divide(lambda, t1, t2);

    gf2n_square(t3, lambda);
    gf2n_add(x3, t3, lambda);
    gf2n_add(x3, x3, t2);
    gf2n_add(x3, x3, actual_coeff_a);

    // CALC Y COORD
    // y3 = lambda*(x1+x3)+x3+y1
    gf2n_add(t1, x1, x3);
    gf2n_mul(y3, t1, lambda);
    gf2n_add(y3, y3, x3);
    gf2n_add(y3, y3, y1);

    dwordvec_copy(out_x, x3);
    dwordvec_copy(out_y, y3);
}

/**
 * Add doubles an ECC Point in affine coordinate system
 * @param[out] out_x x-coordinate of result
 * @param[out] out_y y-coordinate of result
 * @param[in] x1 x-coordinate of affine point to be doubled
 * @param[in] y1 y-coordinate of affine point to be doubled
 */
static void ecc_double_affine_coords(dwordvec_t out_x, dwordvec_t out_y,
                             const dwordvec_t x1,
                             const dwordvec_t y1)
{
    dwordvec_t x3, y3;

    dwordvec_t lambda, t1, t2;
    // CALC X Coord
    gf2n_divide(lambda, y1, x1);
    gf2n_add(lambda, lambda, x1);

    gf2n_square(x3, lambda);
    gf2n_add(x3, x3, lambda);
    gf2n_add(x3, x3, actual_coeff_a);

    // Calc Y
    gf2n_square(t1, x1);
    gf2n_mul(t2, lambda, x3);
    gf2n_add(y3, t1, t2);
    gf2n_add(y3, y3, x3);
    dwordvec_copy(out_x, x3);
    dwordvec_copy(out_y, y3);
}


/**
 * Double an ECC point (X1/Y1/Z1) in LD format
 * according to https://eprint.iacr.org/2004/323.pdf
 * @param[out] X3 x-coordinate of result in LD
 * @param[out] Y3 y-coordinate of result in LD
 * @param[in] Z3 z-coordinate of result in LD
 * @param[in] X1 x-coordinate of LD point to be doubled
 * @param[in] Y1 y-coordinate of LD point to be doubled
 * @param[in] Z1 z-coordinate of LD point to be doubled
 */
static void ecc_double_lopez_point(dwordvec_t X3, dwordvec_t Y3, dwordvec_t Z3, const dwordvec_t X1, const dwordvec_t Y1, const dwordvec_t Z1)
{
    dwordvec_t t1, t2;
    dwordvec_t S, U, T;

    gf2n_square(S, X1);
    gf2n_add(U, S, Y1);
    gf2n_mul(T, X1, Z1);
    gf2n_square(Z3, T);
    gf2n_mul(T, U, T);

    gf2n_square(U, U);
    gf2n_mul(X3, actual_coeff_a, Z3);
    gf2n_sum(X3, T);
    gf2n_sum(X3, U);

    gf2n_square(S, S);
    gf2n_mul(t1, S, Z3);
    gf2n_add(t2, Z3, T);
    gf2n_mul(t2, t2, X3);
    gf2n_add(Y3, t1, t2);
}


/**
 * Add an affine point (X2,Y2) to a point in LD coordinates (X1,Y1,Z1)
 * @param[out] X3 x-coordinate of result in LD
 * @param[out] Y3 y-coordinate of result in LD
 * @param[out] Z3 z-coordinate of result in LD
 * @param[in] X1 x-coordinate of first operand in LD
 * @param[in] Y1 y-coordinate of first operand in LD
 * @param[in] Z1 z-coordinate of first operand in LD
 * @param[in] X2 x-coordinate of second operand in affine
 * @param[in] Y2 y-coordinate of second operand in affine
 * @return TRUE if LD point is at infinity, FALSE otherwise
 */
static int ecc_add_lopez_affinite_point(
    dwordvec_t X3,
    dwordvec_t Y3,
    dwordvec_t Z3,
    const dwordvec_t X1,
    const dwordvec_t Y1,
    const dwordvec_t Z1,
    const dwordvec_t X2,
    const dwordvec_t Y2)
{
	dwordvec_t A, B, C, D, t1, t2, t3;
    /* cases with point at infinity */
    if (dwordvec_iszero(Z1) && dwordvec_iszero(Y1)) {
        dwordvec_copy(X3, X2);
        dwordvec_copy(Y3, Y2);
        dwordvec_copy(Z3, one_element);
        return TRUE;
    }

    gf2n_square(t1, Z1);
    gf2n_mul(A, Y2, t1);
    gf2n_sum(A, Y1);

    gf2n_mul(B, X2, Z1);
    gf2n_sum(B, X1);

    gf2n_mul(C, B, Z1);

    gf2n_square(Z3, C);

    gf2n_mul(D, X2, Z3);

    gf2n_mul(t1, actual_coeff_a, C);
    gf2n_square(t2, B);
    gf2n_add(t3, A, t1);
    gf2n_sum(t3, t2);
    gf2n_mul(X3, C, t3);
    gf2n_square(t1, A);
    gf2n_sum(X3, t1);

    gf2n_add(t1, D, X3);
    gf2n_mul(t2, A, C);
    gf2n_sum(t2, Z3);
    gf2n_mul(Y3, t1, t2);

    gf2n_square(t1, Z3);
    gf2n_add(t2, Y2, X2);
    gf2n_mul(t3, t1, t2);
    gf2n_sum(Y3, t3);
    return FALSE;
}

/**
 * Calculates the Product of a*P + b*Q using "Sharmir's Trick" with a sliding window of W=2.
 * This requires quite some memory for precomputation of 16*dwowrdvec_t = e.g. for GF192: 16*7*4bytes = 448 bytes!
 *
 * The actual computations are carried out in LD format and then converted back to affine coords as this is the fasted way.
 * cf. Hankerson, Lange.
 * @param[out] point_x x-coordinate of a*P+b*Q
 * @param[in] P Known Point P
 * @param[in] Q Unknown Point Q
 * @param[in] skalar_a constant to multiply P
 * @param[in] skalar_b constant to multiply Q with
 * @return TRUE if result is at infinity, FALSE if everything went fine.
 */
static int ecc_multiply_shamir_lopez_w2_sliding_point_x(dwordvec_t point_x, const eccpoint_t *P, const eccpoint_t *Q, dwordvec_t skalar_a, dwordvec_t skalar_b)
{

    dwordvec_t RX, RY, RZ;
    int i, j;
    int index = 0, index2 = 0;
    int i2, i3 = 0;
    int offset;
    eccpoint_t PREC[16] = {0};

    dwordvec_copy(RX, one_element);
    dwordvec_copy(RY, zero_element);
    dwordvec_copy(RZ, zero_element);
    
    dwordvec_copy(PREC[4].x_coord, P->x_coord);
    dwordvec_copy(PREC[4].y_coord, P->y_coord);

#if (USE_PRECOMP_2P_3P==1)
    dwordvec_copy(PREC[8].x_coord, P2X);
    dwordvec_copy(PREC[8].y_coord, P2Y);
#else
    ecc_double_affine_coords(PREC[8].x_coord, PREC[8].y_coord, P->x_coord, P->y_coord);
#endif

#if (USE_PRECOMP_2P_3P==1)
    dwordvec_copy(PREC[12].x_coord, P3X);
    dwordvec_copy(PREC[12].y_coord, P3Y);
#else
    ecc_add_affine_coords(PREC[12].x_coord, PREC[12].y_coord, PREC[8].x_coord, PREC[8].y_coord, P->x_coord, P->y_coord);
#endif


    dwordvec_copy(PREC[1].x_coord, Q->x_coord);
    dwordvec_copy(PREC[1].y_coord, Q->y_coord);
    ecc_double_affine_coords(PREC[2].x_coord, PREC[2].y_coord, Q->x_coord, Q->y_coord);
    ecc_add_affine_coords(PREC[3].x_coord, PREC[3].y_coord, PREC[2].x_coord, PREC[2].y_coord, Q->x_coord, Q->y_coord);
    ecc_add_affine_coords(PREC[5].x_coord, PREC[5].y_coord, P->x_coord, P->y_coord, Q->x_coord, Q->y_coord);
    ecc_add_affine_coords(PREC[9].x_coord, PREC[9].y_coord, PREC[8].x_coord, PREC[8].y_coord, Q->x_coord, Q->y_coord);
    ecc_add_affine_coords(PREC[13].x_coord, PREC[13].y_coord, PREC[12].x_coord, PREC[12].y_coord, Q->x_coord, Q->y_coord);
    ecc_add_affine_coords(PREC[6].x_coord, PREC[6].y_coord, P->x_coord, P->y_coord, PREC[2].x_coord, PREC[2].y_coord);
    ecc_add_affine_coords(PREC[14].x_coord, PREC[14].y_coord, PREC[12].x_coord, PREC[12].y_coord, PREC[2].x_coord, PREC[2].y_coord);
    ecc_add_affine_coords(PREC[7].x_coord, PREC[7].y_coord, P->x_coord, P->y_coord, PREC[3].x_coord, PREC[3].y_coord);
    ecc_add_affine_coords(PREC[11].x_coord, PREC[11].y_coord, PREC[8].x_coord, PREC[8].y_coord, PREC[3].x_coord, PREC[3].y_coord);
    ecc_add_affine_coords(PREC[15].x_coord, PREC[15].y_coord, PREC[12].x_coord, PREC[12].y_coord, PREC[3].x_coord, PREC[3].y_coord);

    for (i = actual_degree - 1; i >= 0; i--)
        if (skalar_a[i / 32] & 1UL << i % 32)
            break;

    for (j = actual_degree - 1; j >= 0; j--)
        if (skalar_b[j / 32] & 1UL << j % 32)
            break;

    if (j > i) {
        i = j;
    }


    while (i >= 0) {
        offset = i;

        i2 = (skalar_a[offset / 32] >> offset % 32 & 1UL) << 2;
        i3 = (skalar_b[offset / 32] >> offset % 32 & 1UL);
        index = (i2 | i3);

        ecc_double_lopez_point(RX, RY, RZ, RX, RY, RZ);
        i--;

        if (index == 0) {
            continue;
        }

        offset--;
        if (offset >= 0) {

            i2 = (skalar_a[offset / 32] >> offset % 32 & 1UL) << 2;
            i3 = (skalar_b[offset / 32] >> offset % 32 & 1UL);
            index2 = (i2 | i3);

            if (index2 != 0) {
                ecc_double_lopez_point(RX, RY, RZ, RX, RY, RZ);
                index = index << 1 | index2;
                i--;
            }
        }

        if (index != 0) {
            ecc_add_lopez_affinite_point(RX, RY, RZ, RX, RY, RZ, PREC[index].x_coord, PREC[index].y_coord);
        }
    }

    if (dwordvec_iszero(RY) && dwordvec_iszero(RZ)) return TRUE;

    gf2n_divide(point_x, RX, RZ);

    return FALSE;
}
#endif
