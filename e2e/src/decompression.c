/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "decompression.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(decompression, CONFIG_DECOMPRESSION_LOG_LEVEL);

#define DECOMP_BUF_SIZE 4096

int decompression_init(
    decompression_ctx_t *ctx, decompression_write_data_cbk_t write_data_cbk, void *user_data)
{
    if (!ctx || ctx->lz4_dctx) {
        return -1;
    }
    if (!write_data_cbk) {
        LOG_ERR("No write callback provided for decompression context");
        return -1;
    }
    LOG_DBG("Initializing decompression context");

    LZ4F_dctx *lz4_dctx = NULL;
    uint8_t *decomp_buf = NULL;

    decomp_buf = malloc(DECOMP_BUF_SIZE);
    if (!decomp_buf) {
        LOG_ERR("Failed to allocate decompression buffer");
        goto failure;
    }

    size_t ret = LZ4F_createDecompressionContext(&lz4_dctx, LZ4F_VERSION);
    if (LZ4F_isError(ret)) {
        LOG_ERR("Failed to create LZ4 context: %s", LZ4F_getErrorName(ret));
        goto failure;
    }

    ctx->lz4_dctx = lz4_dctx;
    ctx->decomp_buf = decomp_buf;
    ctx->write_data_cbk = write_data_cbk;
    ctx->user_data = user_data;
    return 0;

failure:
    free(decomp_buf);
    if (lz4_dctx) {
        LZ4F_freeDecompressionContext(lz4_dctx);
    }
    return -1;
}

bool decompression_is_initialized(const decompression_ctx_t *ctx)
{
    return ctx && ctx->lz4_dctx != NULL;
}

int decompression_process_chunk(decompression_ctx_t *ctx, const uint8_t *src, size_t src_size)
{
    if (!ctx || !ctx->lz4_dctx) {
        return -1;
    }
    LOG_DBG("Processing chunk of size %zu", src_size);

    const uint8_t *src_cursor = src;
    size_t src_remaining = src_size;
    bool flush_needed = false;

    while (src_remaining > 0 || flush_needed) {
        size_t dst_consumed = DECOMP_BUF_SIZE;
        size_t src_consumed = src_remaining;

        size_t ret = LZ4F_decompress(
            ctx->lz4_dctx, ctx->decomp_buf, &dst_consumed, src_cursor, &src_consumed, NULL);
        if (LZ4F_isError(ret)) {
            LOG_ERR("Decompression error: %s", LZ4F_getErrorName(ret));
            return -1;
        }

        if (dst_consumed > 0) {
            LOG_DBG("Extracted %zu uncompressed bytes", dst_consumed);

            int write_ret = ctx->write_data_cbk(ctx->decomp_buf, dst_consumed, ctx->user_data);
            if (write_ret < 0) {
                LOG_ERR("Failed to write decompressed data");
                return -1;
            }
        }

        src_cursor += src_consumed;
        src_remaining -= src_consumed;

        // If LZ4 completely filled the destination buffer, it likely has more data
        // buffered internally. We must loop again to flush it, even if src_remaining == 0.
        flush_needed = (dst_consumed == DECOMP_BUF_SIZE);
    }

    return 0;
}

void decompression_free(decompression_ctx_t *ctx)
{
    if (ctx) {
        LOG_DBG("Freeing decompression context");
        free(ctx->decomp_buf);
        ctx->decomp_buf = NULL;
        if (ctx->lz4_dctx) {
            LZ4F_freeDecompressionContext(ctx->lz4_dctx);
            ctx->lz4_dctx = NULL;
        }
    }
}
