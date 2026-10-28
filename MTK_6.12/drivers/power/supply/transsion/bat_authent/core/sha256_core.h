


















#ifndef _SHA256_H_
#define _SHA256_H_

#define    ALG_SHA_256          0x06
#define    SHA256_PAD_BYTE      0x80

enum {
	SHA256_MESSAGE_BLOCK_SIZE = 64,
	SHA256_HASHSIZE           = 32,
	SHA256_HASHWORDSIZE       = 8
};
/*
 *  All SHA functions return one of these values.
 */
enum {
	SHA_SUCCESS = 0,
	SHA_NULL,                 /* Null pointer parameter */
	SHA_INTOOLONG,            /* input data too long */
	SHA_STATEERROR,           /* called Input after FinalBits or Result */
	SHA_BADPARAM              /* passed a bad parameter */
};
/* _SHA_enum_ */

/*
 *  This structure will hold context information for the SHA-256
 *  hashing operation.
 */
struct str_sha256_context {
	uint32_t    state[8];               /* Message Digest 8 words */
	uint32_t    length_high;            /* Message length in bits */
	uint32_t    length_low;             /* Message length in bits */
	uint16_t    message_block_index;    /* msg_block array index */
	uint8_t     message_block[64];      /* 512-bit msg blocks = 64 bytes */
	int         alg;                    /* Alg type */
	int         computed;               /* Is the hash computed? */
	int         corrupted;              /* Cumulative corruption code */
};

#endif  /* END _SHA256_H_ */

