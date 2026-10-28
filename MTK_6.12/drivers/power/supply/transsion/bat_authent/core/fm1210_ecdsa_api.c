


















#include <linux/string.h>
#include <linux/stddef.h>
//#include <stdlib.h>
#include <linux/slab.h>
#include "fl_cryptolib.h"

//替换为kernel空间的malloc和free
#define malloc(s)        kmalloc(s, GFP_KERNEL)
#define free(p)          kfree(p)

/*
 * u8 �ַ�������ת����32-bit����,���ģʽ
 */
static void comm_get_u32_be(uint32_t *pt_n, uint8_t *b, uint32_t offset)
{
	*pt_n = (((uint32_t) b[offset]) << 24) |
			(((uint32_t) b[offset + 1]) << 16) |
			(((uint32_t) b[offset + 2]) <<  8) |
			(((uint32_t) b[offset + 3]));
}

int fm1210_ecdsa_precombpoint(const unsigned char *combtable, size_t tablen,
		const unsigned char *pa, size_t palen)
{
	int ret = 0;
	int i;
	struct str_pae *pae_ctx1 = NULL;
	struct str_ecc_key key;
	uint32_t pa_buf[0x40];

	pae_ctx1 = (struct str_pae *)malloc(sizeof(struct str_pae));
	if (pae_ctx1 == NULL)
		return 1;
	ecc_init_p256k1(&key, pae_ctx1);
	key.q = (struct str_affpoint *)pa_buf;
	for (i = 0; i < 0x10; i++)
		comm_get_u32_be(&pa_buf[i], (unsigned char *)pa, i * 4);
	key.comb_table_q = (uint32_t *)combtable;
	ret = ecc_precombpoint(&key);
	free(pae_ctx1);
	return ret;
}

int fm1210_ecdsa_verify(const unsigned char *combtable, size_t tablen,
		const unsigned char *pa, size_t palen,
		const unsigned char *hash, size_t hlen,
		const unsigned char *sig, size_t slen)
{
	int ret = 0;
	int i;
	uint32_t pa_buf[0x40];
	struct str_pae *pae_ctx1;
	struct str_ecc_key key;

	pae_ctx1 = (struct str_pae *)malloc(sizeof(struct str_pae));
	if (pae_ctx1 == NULL)
		return 1;

	ecc_init_p256k1(&key, pae_ctx1);
	key.q = (struct str_affpoint *)pa_buf;
	for (i = 0; i < 0x10; i++)
		comm_get_u32_be(&pa_buf[i], (unsigned char *)pa, i * 4);
	key.comb_table_q = (uint32_t *)combtable;
	ret = ecdsa_verifyhash(&key, (unsigned char *)hash, (unsigned char *)sig);
	free(pae_ctx1);
	return (ret == RESULT_OK ? 0 : 1);
}

int fm1210_ecdsa_sha256(const unsigned char *msg, size_t msglen,
		unsigned char *outhash)
{
	int ret = 0;
	struct str_sha256_context ctx;

	fl_sha256_init(&ctx);
	fl_sha256_update(&ctx, msg, msglen);
	fl_sha256_final(&ctx, outhash);
	return ret;
}

