


















#ifndef FM1210_ECSDA_API_H
#define FM1210_ECSDA_API_H
int fm1210_ecdsa_precombpoint(const unsigned char *combtable, size_t tablen,
		const unsigned char *pa, size_t palen);

int fm1210_ecdsa_verify(const unsigned char *combtable, size_t tablen,
		const unsigned char *pa, size_t palen, const unsigned char *hash,
		size_t hlen, const unsigned char *sig, size_t slen);

int fm1210_ecdsa_sha256(const unsigned char *msg, size_t msglen,
		unsigned char *outhash);

#endif /* END FM1210_ECSDA_API_H */

