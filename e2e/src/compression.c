/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "compression.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(compression, CONFIG_COMPRESSION_LOG_LEVEL);

// Static preferences for the LZ4 compression
static const LZ4F_preferences_t lz4_prefs = {
    .autoFlush = 1,
};

int compression_init(compression_ctx_t *ctx)
{
    if (!ctx || ctx->lz4_cctx) {
        return -1;
    }
    LOG_DBG("Initializing compression context");

    size_t ret = LZ4F_createCompressionContext(&ctx->lz4_cctx, LZ4F_VERSION);
    if (LZ4F_isError(ret)) {
        LOG_ERR("Failed to create LZ4 compression context: %s", LZ4F_getErrorName(ret));
        return -1;
    }

    return 0;
}

bool compression_is_initialized(const compression_ctx_t *ctx)
{
    return ctx && ctx->lz4_cctx != NULL;
}

int compression_begin(compression_ctx_t *ctx, uint8_t *out, size_t out_size, size_t *bytes_written)
{
    if (!ctx || !ctx->lz4_cctx)
        return -1;

    size_t ret = LZ4F_compressBegin(ctx->lz4_cctx, out, out_size, &lz4_prefs);
    if (LZ4F_isError(ret)) {
        LOG_ERR("LZ4 compress begin failed: %s", LZ4F_getErrorName(ret));
        return -1;
    }

    *bytes_written = ret;
    return 0;
}

int compression_update(compression_ctx_t *ctx, const uint8_t *input, size_t input_size,
    uint8_t *out, size_t out_size, size_t *bytes_written)
{
    if (!ctx || !ctx->lz4_cctx)
        return -1;

    size_t ret = LZ4F_compressUpdate(ctx->lz4_cctx, out, out_size, input, input_size, NULL);
    if (LZ4F_isError(ret)) {
        LOG_ERR("LZ4 compression failed: %s", LZ4F_getErrorName(ret));
        return -1;
    }

    *bytes_written = ret;
    return 0;
}

int compression_end(compression_ctx_t *ctx, uint8_t *out, size_t out_size, size_t *bytes_written)
{
    if (!ctx || !ctx->lz4_cctx)
        return -1;

    size_t ret = LZ4F_compressEnd(ctx->lz4_cctx, out, out_size, NULL);
    if (LZ4F_isError(ret)) {
        LOG_ERR("LZ4 footer write failed: %s", LZ4F_getErrorName(ret));
        return -1;
    }

    *bytes_written = ret;
    return 0;
}

void compression_free(compression_ctx_t *ctx)
{
    if (ctx && ctx->lz4_cctx) {
        LOG_DBG("Freeing compression context");
        LZ4F_freeCompressionContext(ctx->lz4_cctx);
        ctx->lz4_cctx = NULL;
    }
}
