/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "runner.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/fatal.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/sys/bitarray.h>
#include <zephyr/sys/util.h>

#include "compression.h"
#include "decompression.h"
#include "device_handler.h"
#include "shell_handlers.h"
#include "shell_macros.h"

#include "http.h"
#include "ztar.h"

LOG_MODULE_REGISTER(runner, CONFIG_RUNNER_LOG_LEVEL);

typedef struct
{
    ztar_unpack_t ztar_stream;
    decompression_ctx_t decomp_ctx;
} download_cbk_user_data_t;

typedef struct
{
    ztar_pack_t pack_ctx;
    uint8_t tar_buffer[512]; // Min allowed size for the buffer
    uint8_t lz4_buffer[640]; // Slightly above the worst case compression for 512 bytes
    compression_ctx_t comp_ctx;
    bool tar_exhausted;
    bool lz4_footer_written;
    struct pack_cbk_user_data_s
    {
        char **files;
        char **file_names;
        size_t num_files;
        size_t current_file_idx;
        size_t data_offset_in_current_file;
    } pack_cbk_user_data;
} upload_cbk_user_data_t;

char *foo_files[] = {
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Mauris auctor ullamcorper viverra. "
    "Praesent nec viverra arcu, vitae lacinia odio. Vivamus sollicitudin feugiat mattis. Maecenas "
    "id eros eget ante efficitur molestie. Praesent non tortor at nisi luctus commodo quis a "
    "massa. Phasellus vestibulum pellentesque ullamcorper. Praesent in auctor dolor. Nam ut dui et "
    "diam efficitur interdum luctus a est. Proin rutrum sodales aliquam. Nunc sit amet euismod "
    "sem, at feugiat enim. \n Fusce mollis quam lorem, sit amet luctus nunc auctor sed. Etiam "
    "maximus justo arcu, varius congue urna porttitor eget. In quis libero et mauris tincidunt "
    "venenatis. Proin est felis, vehicula a ex eu, euismod semper enim. Etiam elementum sed sem "
    "quis mattis. Phasellus id ante non turpis faucibus bibendum nec quis enim. Sed pharetra "
    "hendrerit pretium. Ut vitae ex ac nunc fringilla dictum a ut urna. Morbi varius lacus sit "
    "amet purus tincidunt aliquet. In hac habitasse platea dictumst. Praesent eros dui, maximus eu "
    "ipsum at, suscipit molestie tortor. Ut mauris tortor, venenatis ut elementum ac, bibendum "
    "vitae nunc. Nullam mollis ornare vehicula. Maecenas ornare tincidunt felis, in rhoncus leo "
    "pharetra dignissim. Duis consequat bibendum magna in condimentum. Vestibulum rhoncus accumsan "
    "cursus. Integer convallis vel velit ac sodales. Nunc at justo sit amet lacus consequat "
    "pharetra. In et interdum enim. Morbi ipsum erat, gravida et tellus ut, sagittis efficitur "
    "diam. Etiam ullamcorper nisl tellus, in vehicula dolor ornare et. Donec feugiat ac quam eget "
    "varius. \nMauris placerat pretium varius. Nulla bibendum nulla massa, sit amet fringilla "
    "libero cursus nec. Nulla libero sapien, auctor quis semper quis, iaculis at lorem. In rutrum "
    "placerat elit, egestas consectetur nisl. Vestibulum maximus elementum viverra. Nam dignissim, "
    "sem id venenatis condimentum, nisl diam lacinia tellus, ac tincidunt elit quam at tellus. "
    "Pellentesque tincidunt lectus eget mauris egestas efficitur. Nam sed maximus lorem. Etiam "
    "sollicitudin sed eros eu pellentesque. Donec ac ultricies odio, id aliquet est. Proin a lorem "
    "et nisl dignissim scelerisque. In mollis vehicula felis, eget vestibulum sapien tempus ut. "
    "Praesent pulvinar lorem vitae viverra vestibulum. Aliquam varius dictum sapien, nec luctus "
    "massa lobortis sit amet. In eget velit eget metus fermentum eleifend et eu magna. Fusce ut "
    "tempus metus, sed laoreet est. Aenean ultrices massa feugiat orci varius iaculis. In id felis "
    "eleifend, aliquet urna nec, elementum ipsum. Morbi et lacus eleifend, fermentum magna quis, "
    "gravida felis. Suspendisse in erat non quam sodales feugiat. Proin porttitor elit sed nunc "
    "viverra, sed tempor lorem posuere. Aliquam ultricies et lectus quis auctor. Fusce leo nunc, "
    "facilisis vestibulum faucibus et, rutrum eget tellus. In tristique nulla vitae diam "
    "ultricies, ac lobortis ante tristique. Phasellus ut lorem id diam pellentesque ultricies et "
    "non lorem. Donec viverra, libero eu ullamcorper pharetra, justo ex luctus magna, vitae varius "
    "massa lectus ut arcu. Donec et quam eros. Aliquam vel ipsum ut elit convallis feugiat. "
    "Integer rhoncus augue at augue ornare euismod. Sed at felis luctus, lobortis odio non, "
    "porttitor neque. Etiam ac rutrum nisl. Duis posuere augue nec tincidunt dictum. Cras "
    "dignissim tortor sed sagittis elementum. Pellentesque quis sagittis sem. ",
    "second file", "last file"
};
char *foo_files_names[] = { "first.txt", "second.txt", "last.txt" };

/************************************************
 *              Callback functions              *
 ***********************************************/

int unpacking_on_file_start_cbk(const ztar_header_t *header, void *user_data)
{
    char file_name[257] = { 0 };
    ztar_result_t zres = ztar_unpack_get_file_name(header, file_name);
    if (zres != ZTAR_RESULT_OK) {
        LOG_ERR("TAR - Failed reading file name %d", zres);
        return -1;
    }
    LOG_INF("TAR - File start, name: %s", file_name);

    ztar_filetype_t file_type;
    zres = ztar_unpack_get_file_type(header, &file_type);
    if (zres != ZTAR_RESULT_OK) {
        LOG_ERR("TAR - Failed reading file type %d", zres);
        return -1;
    }

    size_t file_size = 0;
    switch (file_type) {
        case ZTAR_REGULAR_FILE:
            LOG_INF("TAR - File type: regular file");
            zres = ztar_unpack_get_file_size(header, &file_size);
            if (zres != ZTAR_RESULT_OK) {
                LOG_ERR("TAR - Failed reading file size %d", zres);
                return -1;
            }
            LOG_INF("TAR - File size: %zu", file_size);
            break;
        case ZTAR_DIRECTORY:
            LOG_INF("TAR - File type: directory");
            break;
        default:
            LOG_INF("TAR - File type: unsupported");
            break;
    }
    return 0;
}

int unpacking_on_file_data_cbk(
    const ztar_header_t *header, const uint8_t *data, size_t size, void *user_data)
{
    LOG_INF("TAR - Received %zu bytes.", size);
    LOG_INF("TAR - Bytes content (as char): %.*s", size, data);
    return 0;
}

int unpacking_on_file_end_cbk(const ztar_header_t *header, void *user_data)
{
    char file_name[257] = { 0 };
    ztar_result_t zres = ztar_unpack_get_file_name(header, file_name);
    if (zres != ZTAR_RESULT_OK) {
        LOG_ERR("TAR - Failed reading file name %d", zres);
        return -1;
    }
    LOG_INF("TAR - File end, name: %s", file_name);
    return 0;
}

/* Callback to write decompressed LZ4 chunks into the tar file buffer */
static int decompression_cbk(const uint8_t *data, size_t size, void *user_data)
{
    ztar_result_t zres = ZTAR_RESULT_OK;
    ztar_unpack_t *ztar_stream = (ztar_unpack_t *) user_data;

    if (!ztar_unpack_is_initialized(ztar_stream)) {
        ztar_unpack_callbacks_t callbacks = { .on_file_start = unpacking_on_file_start_cbk,
            .on_file_data = unpacking_on_file_data_cbk,
            .on_file_end = unpacking_on_file_end_cbk };

        zres = ztar_unpack_init(ztar_stream, callbacks, NULL);
        if (zres != ZTAR_RESULT_OK) {
            LOG_ERR("Failed to initialize ztar stream");
            return -1;
        }
    }

    zres = ztar_unpack_process(ztar_stream, data, size);
    if ((zres != ZTAR_RESULT_OK) && (zres != ZTAR_RESULT_ARCHIVE_EXAHUSTED)) {
        LOG_ERR("Failed to proces decompressed chunk in ztar stream");
        return -1;
    }

    return 0;
}

edgehog_result_t my_download_cbk(edgehog_http_response_chunk_t *response_chunk, void *user_data)
{
    int ret = 0;
    download_cbk_user_data_t *download_cbk_user_data = (download_cbk_user_data_t *) user_data;
    decompression_ctx_t *decomp_ctx = &download_cbk_user_data->decomp_ctx;
    ztar_unpack_t *ztar_stream = &download_cbk_user_data->ztar_stream;

    // Initialize the decompression context on the very first chunk
    if (!decompression_is_initialized(decomp_ctx)) {
        ret = decompression_init(decomp_ctx, decompression_cbk, ztar_stream);
        if (ret != 0) {
            return EDGEHOG_RESULT_INTERNAL_ERROR;
        }
        LOG_INF("LZ4 decompression context created.");
    }

    // Feed the incoming chunk to the decompressor in a loop
    size_t src_size = response_chunk->chunk_size;
    if (src_size != 0) {
        const uint8_t *src_ptr = (const uint8_t *) response_chunk->chunk_start_addr;
        ret = decompression_process_chunk(decomp_ctx, src_ptr, src_size);
        if (ret != 0) {
            decompression_free(decomp_ctx);
            return EDGEHOG_RESULT_INTERNAL_ERROR;
        }
    }

    // Clean up once the download finishes
    if (response_chunk->last_chunk) {
        LOG_INF("Download complete. Freeing decompression context.");
        decompression_free(decomp_ctx);
    }

    return EDGEHOG_RESULT_OK;
}

static int pack_get_next_file_cbk(
    bool *has_next, char name[static 257], size_t *size, void *user_data)
{
    struct pack_cbk_user_data_s *ctx = (struct pack_cbk_user_data_s *) user_data;

    if (ctx->current_file_idx >= ctx->num_files) {
        *has_next = false;
        return 0;
    }

    strncpy(name, ctx->file_names[ctx->current_file_idx], 256);
    name[256] = '\0';
    *size = strlen(ctx->files[ctx->current_file_idx]);

    // Reset our internal tracker for file data reading
    ctx->data_offset_in_current_file = 0;

    *has_next = true;
    return 0;
}

static int pack_read_file_data_cbk(
    uint8_t *buffer, size_t max_size, void *user_data, size_t *bytes_read)
{
    struct pack_cbk_user_data_s *ctx = (struct pack_cbk_user_data_s *) user_data;

    const char *file_content = ctx->files[ctx->current_file_idx];
    size_t file_len = strlen(file_content);
    size_t remaining_in_file = file_len - ctx->data_offset_in_current_file;

    size_t to_copy = MIN(max_size, remaining_in_file);
    memcpy(buffer, file_content + ctx->data_offset_in_current_file, to_copy);

    ctx->data_offset_in_current_file += to_copy;

    // If we've read everything, bump the file index for the next cycle
    if (ctx->data_offset_in_current_file == file_len) {
        ctx->current_file_idx++;
    }

    *bytes_read = to_copy;
    return 0;
}

static edgehog_result_t my_upload_cbk(edgehog_http_payload_chunk_t *payload_chunk, void *user_data)
{
    upload_cbk_user_data_t *ctx = (upload_cbk_user_data_t *) user_data;
    size_t lz4_bytes_written = 0;
    int comp_res = 0;

    if (!ztar_pack_is_initialized(&ctx->pack_ctx)) {
        ztar_pack_callbacks_t callbacks = { .get_next_file = pack_get_next_file_cbk,
            .read_file_data = pack_read_file_data_cbk };
        ztar_pack_init(&ctx->pack_ctx, callbacks, &ctx->pack_cbk_user_data);
        LOG_INF("TAR pack context initialized.");
    }

    if (!compression_is_initialized(&ctx->comp_ctx)) {
        comp_res = compression_init(&ctx->comp_ctx);
        if (comp_res != 0) {
            LOG_ERR("Compression context initialization failed.");
            return EDGEHOG_RESULT_INTERNAL_ERROR;
        }

        comp_res = compression_begin(
            &ctx->comp_ctx, ctx->lz4_buffer, sizeof(ctx->lz4_buffer), &lz4_bytes_written);
        if (comp_res != 0) {
            LOG_ERR("Failed to begin compression.");
            return EDGEHOG_RESULT_INTERNAL_ERROR;
        }
    } else {
        // Read from tar and compress until we have some output bytes or are completely done
        while (lz4_bytes_written == 0 && !ctx->lz4_footer_written) {
            if (!ctx->tar_exhausted) {
                size_t tar_bytes_read = 0;
                ztar_result_t zres = ztar_pack_read_stream(
                    &ctx->pack_ctx, ctx->tar_buffer, sizeof(ctx->tar_buffer), &tar_bytes_read);
                if (zres == ZTAR_RESULT_ARCHIVE_EXAHUSTED) {
                    LOG_DBG("TAR archive exhausted.");
                    ctx->tar_exhausted = true;
                } else if (zres != ZTAR_RESULT_OK) {
                    LOG_ERR("TAR stream packing failed: %d", zres);
                    return EDGEHOG_RESULT_INTERNAL_ERROR;
                }

                if (tar_bytes_read > 0) {
                    size_t chunk_written = 0;
                    comp_res = compression_update(&ctx->comp_ctx, ctx->tar_buffer, tar_bytes_read,
                        ctx->lz4_buffer + lz4_bytes_written,
                        sizeof(ctx->lz4_buffer) - lz4_bytes_written, &chunk_written);
                    if (comp_res != 0) {
                        LOG_ERR("Failed to update compression.");
                        return EDGEHOG_RESULT_INTERNAL_ERROR;
                    }
                    lz4_bytes_written += chunk_written;
                }
            }

            // Once the TAR is completely processed, finalize the LZ4 frame
            if (ctx->tar_exhausted && !ctx->lz4_footer_written) {
                size_t chunk_written = 0;
                comp_res = compression_end(&ctx->comp_ctx, ctx->lz4_buffer + lz4_bytes_written,
                    sizeof(ctx->lz4_buffer) - lz4_bytes_written, &chunk_written);
                if (comp_res != 0) {
                    LOG_ERR("Failed to terminate the compression.");
                    return EDGEHOG_RESULT_INTERNAL_ERROR;
                }
                lz4_bytes_written += chunk_written;
                ctx->lz4_footer_written = true;
            }
        }
    }

    // Set the chunk variables for the HTTP uploader
    payload_chunk->chunk_start_addr = ctx->lz4_buffer;
    payload_chunk->chunk_size = lz4_bytes_written;
    payload_chunk->last_chunk = ctx->lz4_footer_written;

    // Clean up the context when the operation concludes
    if (ctx->lz4_footer_written) {
        compression_free(&ctx->comp_ctx);
    }

    return EDGEHOG_RESULT_OK;
}

/************************************************
 *         Global functions definition          *
 ***********************************************/

void run_end_to_end_test()
{
    LOG_INF("End to end test runner");

#if CONFIG_E2E_LOG_ONLY
    LOG_WRN("Running with device callbacks in log only mode");
    LOG_WRN("Data received will NOT be checked against expected data");
#endif

    LOG_INF("Starting the device");
    setup_device();

    // Wait for the device connection
    LOG_INF("Waiting for the device to be connected");
    wait_for_device_connection();

    // Wait for a second to allow the Edgehog device to perform the initial publish
    k_sleep(K_SECONDS(1));

    // We are ready to send and receive data
    const struct shell *uart_shell = shell_backend_uart_get_ptr();
    shell_start(uart_shell);
    k_sleep(K_MSEC(100));

    // Pytest detects the readyness of the shell through this string
    shell_print(uart_shell, SHELL_IS_READY);

    download_cbk_user_data_t download_cbk_user_data = { 0 };

    edgehog_http_get_data_t download_data
        = { .url = "https://192.0.2.2:8443/compressed_archive.tar.lz4",
              .header_fields = (const char *[]){ NULL },
              .timeout_ms = 5000,
              .response_cbk = my_download_cbk,
              .user_data = (void *) &download_cbk_user_data };
    edgehog_result_t result = edgehog_http_get(&download_data);
    if (result == EDGEHOG_RESULT_OK) {
        LOG_WRN("HTTP GET request succeeded.");
    } else {
        LOG_WRN("HTTP GET request failed with error code: %d", result);
    }

    upload_cbk_user_data_t upload_cbk_user_data = { .pack_cbk_user_data.files = foo_files,
        .pack_cbk_user_data.file_names = foo_files_names,
        .pack_cbk_user_data.num_files = sizeof(foo_files) / sizeof(foo_files[0]),
        .pack_cbk_user_data.current_file_idx = 0 };

    edgehog_http_put_data_t upload_data = { .url = "https://192.0.2.2:8443/archive.tar.lz4",
        .header_fields = (const char *[]){ NULL },
        .timeout_ms = 5000,
        .payload_cbk = my_upload_cbk,
        .user_data = &upload_cbk_user_data };
    result = edgehog_http_put(&upload_data);
    if (result == EDGEHOG_RESULT_OK) {
        LOG_WRN("HTTP PUT request succeeded.");
    } else {
        LOG_WRN("HTTP PUT request failed with error code: %d", result);
    }

    // Wait untill a shell command disconnects the device
    wait_for_device_disconnection();

    // Pytest detects the a termination of the test through this string
    shell_print(uart_shell, SHELL_IS_CLOSING);
    shell_stop(uart_shell);

    // Free the device and epxected data structures after `shell_stop`
    free_device();

#if CONFIG_E2E_LOG_ONLY
    LOG_WRN("Test has been run with device callbacks in log only mode");
    LOG_WRN("Data received didn't get checked against expected data");
#endif
}
