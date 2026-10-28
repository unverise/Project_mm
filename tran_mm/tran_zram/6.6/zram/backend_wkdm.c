// SPDX-License-Identifier: GPL-2.0-or-later

#define KMSG_COMPONENT "WKDM"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <crypto/algapi.h>
#include <crypto/internal/scompress.h>
#include "WKdm.h"
#include "backend_wkdm.h"

static void __wkdm_cra_exit(struct wkdm_ctx *ctx)
{
	kfree(ctx->dictionary);
	kfree(ctx->tempTagsArray);
	kfree(ctx->tempQPosArray);
	kfree(ctx->tempLowBitsArray);
}

static void wkdm_free_ctx(struct crypto_scomp *tfm, void *ctx)
{
	__wkdm_cra_exit(ctx);
	kfree(ctx);
}

static void wkdm_cra_exit(struct crypto_tfm *tfm)
{
    struct wkdm_ctx *ctx = crypto_tfm_ctx(tfm);

    __wkdm_cra_exit(ctx);
}

static int __wkdm_cra_init(struct wkdm_ctx *ctx)
{
	if (!ctx)
		return -EINVAL;

	ctx->dictionary = kzalloc(DICTIONARY_SIZE * sizeof(DictionaryElement), GFP_KERNEL);
	if (!ctx->dictionary)
		goto err;

	ctx->tempTagsArray = kzalloc(TAGS_ARRAY_SIZE * sizeof(WK_word), GFP_KERNEL);
	if (!ctx->tempTagsArray)
		goto err;

	ctx->tempQPosArray = kzalloc(QPOS_ARRAY_SIZE * sizeof(WK_word), GFP_KERNEL);
	if (!ctx->tempQPosArray)
		goto err;

	ctx->tempLowBitsArray = kzalloc(LOW_BITS_ARRAY_SIZE * sizeof(WK_word), GFP_KERNEL);
	if (!ctx->tempLowBitsArray)
		goto err;

	return 0;

err:
	wkdm_free_ctx(NULL, ctx);
	return -ENOMEM;
}

static void *wkdm_alloc_ctx(struct crypto_scomp *tfm)
{
	int ret;
	struct wkdm_ctx *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ret = __wkdm_cra_init(ctx);
	if (ret) {
		kfree(ctx);
		return ERR_PTR(ret);
	}

	return ctx;
}

static int wkdm_cra_init(struct crypto_tfm *tfm)
{
	struct wkdm_ctx *ctx = crypto_tfm_ctx(tfm);

	return __wkdm_cra_init(ctx);
}

static int __wkdm_compress_crypto(const u8 *src, unsigned int slen, u8 *dst, unsigned int *dlen, void *ctx)
{
	int ret;

	ret = WKdm_compress(src, slen, dst, ctx);
	if (ret <= 0) {
		pr_err("Compress failed: src=%p, dst=%p, slen=%d, dlen=%d, ret=%d\n", src, dst, slen, *dlen, ret);
		return -EINVAL;
	}
	*dlen = ret;

#ifdef WK_DEBUG
	pr_info("Compress success: src=%p, dst=%p, slen=%d, dlen=%d\n", src, dst, slen, *dlen);
#endif

	return 0;
}

static int wkdm_compress_crypto(struct crypto_tfm *tfm, const u8 *src,
				unsigned int slen, u8 *dst, unsigned int *dlen)
{
	struct wkdm_ctx *ctx = crypto_tfm_ctx(tfm);

	return __wkdm_compress_crypto(src, slen, dst, dlen, ctx);
}

static int wkdm_scompress(struct crypto_scomp *tfm, const u8 *src,
			 unsigned int slen, u8 *dst, unsigned int *dlen,  void *ctx)
{
	return __wkdm_compress_crypto(src, slen, dst, dlen, ctx);
}

static int __wkdm_decompress_crypto(const u8 *src, unsigned int slen,
				   u8 *dst, unsigned int *dlen, void *ctx)
{
	int ret;

	ret = WKdm_decompress(src, slen, dst, ctx);
	if (ret != *dlen) {
		pr_err("Decompress failed: src=%p, dst=%p, slen=%d, dlen=%d, ret=%d\n", src, dst, slen, *dlen, ret);
		return -EINVAL;
	}

#ifdef WK_DEBUG
	pr_info("Decompress success: src=%p, dst=%p, slen=%d, dlen=%d\n", src, dst, slen, *dlen);
#endif

	return 0;
}

static int wkdm_decompress_crypto(struct crypto_tfm *tfm, const u8 *src,
				unsigned int slen, u8 *dst, unsigned int *dlen)
{
	struct wkdm_ctx *ctx = crypto_tfm_ctx(tfm);

	return __wkdm_decompress_crypto(src, slen, dst, dlen, ctx);
}

static int wkdm_sdecompress(struct crypto_scomp *tfm, const u8 *src,
			unsigned int slen, u8 *dst, unsigned int *dlen, void *ctx)
{
	return __wkdm_decompress_crypto(src, slen, dst, dlen, ctx);
}

static struct crypto_alg alg_wkdm = {
	.cra_name               = "wkdm",
	.cra_driver_name        = "wkdm-generic",
	.cra_flags              = CRYPTO_ALG_TYPE_COMPRESS,
	.cra_ctxsize            = sizeof(struct wkdm_ctx),
	.cra_module             = THIS_MODULE,
	.cra_init               = wkdm_cra_init,
	.cra_exit               = wkdm_cra_exit,
	.cra_u                  = { .compress = {
	.coa_compress           = wkdm_compress_crypto,
	.coa_decompress         = wkdm_decompress_crypto } }
};

static struct scomp_alg scomp = {
	.alloc_ctx		= wkdm_alloc_ctx,
	.free_ctx		= wkdm_free_ctx,
	.compress		= wkdm_scompress,
	.decompress		= wkdm_sdecompress,
	.base			= {
		.cra_name	= "wkdm",
		.cra_driver_name = "wkdm-scomp",
		.cra_module	 = THIS_MODULE,
	}
};

int __init wkdm_init(void)
{
	int ret;

	ret = crypto_register_alg(&alg_wkdm);
	if (ret)
		return ret;

	ret = crypto_register_scomp(&scomp);
	if (ret) {
		crypto_unregister_alg(&alg_wkdm);
		return ret;
	}

	return ret;
}

void __exit wkdm_exit(void)
{
	crypto_unregister_alg(&alg_wkdm);
	crypto_unregister_scomp(&scomp);
}
