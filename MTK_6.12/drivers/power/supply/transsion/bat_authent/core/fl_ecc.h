


















#ifndef _FL_ECC_H_
#define _FL_ECC_H_

uint8_t ecc_pointadd(struct str_ecc_key *key,
		struct str_affpoint *in1, struct str_affpoint *in2,
		struct str_affpoint *out);
uint8_t ecc_precombpoint(struct str_ecc_key *key);
uint8_t ecc_pointmult_comb(struct str_ecc_key *key, uint32_t *p_comb_table,
		uint8_t *k, struct str_affpoint *out);

#endif /* END  _FL_ECC_H_ */

