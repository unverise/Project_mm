// SPDX-License-Identifier: GPL-2.0-or-later

#define KMSG_COMPONENT "WKDM"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/kernel.h>
#include <linux/slab.h>
#include "WKdm.h"
#include "backend_wkdm.h"

static void wkdm_destroy_ctx(struct zcomp_ctx *ctx)
{
    struct wkdm_ctx *zctx = ctx->context;

	if (!zctx)
		return;

    kfree(zctx->dictionary);
    kfree(zctx->tempTagsArray);
    kfree(zctx->tempQPosArray);
    kfree(zctx->tempLowBitsArray);
    kfree(zctx);
}

static int wkdm_create_ctx(struct zcomp_params *params, struct zcomp_ctx *ctx)
{
    struct wkdm_ctx *zctx;

    zctx = kzalloc(sizeof(*zctx), GFP_KERNEL);
	if (!zctx)
		return -ENOMEM;

	ctx->context = zctx;
    zctx->dictionary = kzalloc(DICTIONARY_SIZE * sizeof(DictionaryElement), GFP_KERNEL);
	if (!zctx->dictionary)
		goto err;
    zctx->tempTagsArray = kzalloc(TAGS_ARRAY_SIZE * sizeof(WK_word), GFP_KERNEL);
	if (!zctx->tempTagsArray)
		goto err;
    zctx->tempQPosArray = kzalloc(QPOS_ARRAY_SIZE * sizeof(WK_word), GFP_KERNEL);
	if (!zctx->tempQPosArray)
		goto err;
    zctx->tempLowBitsArray = kzalloc(LOW_BITS_ARRAY_SIZE * sizeof(WK_word), GFP_KERNEL);
	if (!zctx->tempLowBitsArray)
		goto err;

     return 0;

err:
    wkdm_destroy_ctx(ctx);
    return -ENOMEM;
}

static int wkdm_compress(struct zcomp_params *params, struct zcomp_ctx *ctx, struct zcomp_req *req)
{
	int ret;
    struct wkdm_ctx *zctx = ctx->context;

    ret = WKdm_compress(req->src, req->src_len, req->dst, zctx);
	if (ret <= 0) { 
		pr_err("Compress fail: src=%p, dst=%p, slen=%d, dlen=%d, ret=%d\n", req->src, req->dst, (int)req->src_len, (int)req->dst_len, ret);
		return -EINVAL;
	}
	req->dst_len = ret;
#ifdef WK_DEBUG
    pr_info("Compress success: src=%p, dst=%p, slen=%d, dlen=%d\n", req->src, req->dst, (int)req->src_len, (int)req->dst_len);
#endif

    return 0;
}

static int wkdm_decompress(struct zcomp_params *params, struct zcomp_ctx *ctx, struct zcomp_req *req)
{
    int ret;
    struct wkdm_ctx *zctx = ctx->context;

    ret = WKdm_decompress(req->src, req->src_len, req->dst, zctx);
    if (ret != req->dst_len) {
        pr_err("Decompress fail: src=%p, dst=%p, slen=%d, dlen=%d, ret=%d\n", req->src, req->dst, (int)req->src_len, (int)req->dst_len, ret);
        return -EINVAL;
    }
#ifdef WK_DEBUG
    pr_info("Decompress success: src=%p, dst=%p, slen=%d, dlen=%d\n", req->src, req->dst, (int)req->src_len, (int)req->dst_len);
#endif

    return 0;
}

static int wkdm_setup_params(struct zcomp_params *params)
{
    return 0;
}

static void wkdm_release_params(struct zcomp_params *params)
{
}

const struct zcomp_ops backend_wkdm = {
    .compress = wkdm_compress,
    .decompress = wkdm_decompress,
    .create_ctx = wkdm_create_ctx,
    .destroy_ctx = wkdm_destroy_ctx,
    .setup_params = wkdm_setup_params,
    .release_params = wkdm_release_params,
    .name = "wkdm",
};
