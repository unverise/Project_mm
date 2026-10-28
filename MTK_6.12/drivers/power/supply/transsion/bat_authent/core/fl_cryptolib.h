


















#ifndef _FL_CRYPTO_LIB_H_
#define  _FL_CRYPTO_LIB_H_

#include "fl_cryptolib_struct.h"

uint8_t ecc_init_p256k1(struct str_ecc_key *key, struct str_pae *pae_ctx);
uint8_t ecc_precombpoint(struct str_ecc_key *key);
uint8_t ecdsa_verifyhash(struct str_ecc_key *key, uint8_t *hash_buf,
		uint8_t *sig);

uint8_t fl_sha256_init(struct str_sha256_context *ctx);
uint8_t fl_sha256_update(struct str_sha256_context *ctx,
		const uint8_t *in, uint32_t len);
uint8_t fl_sha256_final(struct str_sha256_context *ctx, uint8_t *p_digest);

#endif  /* END _FL_CRYPTO_LIB_H_ */

