#include "ztar.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ztar, CONFIG_ZTAR_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

// Gets the header checksum.
static ztar_result_t get_chksum(const ztar_header_t *header, uint32_t *chksum);
// Validates that the 6-byte magic indicator is strictly POSIX USTAR.
static ztar_result_t validate_magic(const ztar_header_t *header);
// Validates that the 2-byte version string is strictly POSIX USTAR.
static ztar_result_t validate_version(const ztar_header_t *header);
// Validates the magic string, version, and the header checksum.
static ztar_result_t validate_header(const ztar_header_t *header);
// Process a complete or partial header from the stream.
static ztar_result_t stream_process_header(
    ztar_unpack_t *stream, const uint8_t *data, size_t size, size_t *bytes_consumed);
// Process a complete or partial file data block from the stream.
static ztar_result_t stream_process_data(
    ztar_unpack_t *stream, const uint8_t *data, size_t size, size_t *bytes_consumed);
// Process a complete or partial padding block from the stream.
static ztar_result_t stream_process_padding(
    ztar_unpack_t *stream, const uint8_t *data, size_t size, size_t *bytes_consumed);
// Process a complete or partial trailer block from the stream.
static ztar_result_t stream_process_trailer(
    ztar_unpack_t *stream, const uint8_t *data, size_t size, size_t *bytes_consumed);
// Helper function for packing octals safely into headers
static int pack_format_octal(uint64_t value, char *out, size_t out_size);
// Handles getting the next file metadata for packing
static ztar_result_t pack_handle_next_file(ztar_pack_t *stream);
// Handles formatting and outputting the tar header
static ztar_result_t pack_handle_header(
    ztar_pack_t *stream, uint8_t *out, size_t remaining, size_t *written);
// Handles reading and outputting the file data payload
static ztar_result_t pack_handle_data(
    ztar_pack_t *stream, uint8_t *out, size_t remaining, size_t *written);
// Handles writing zero-padding to reach 512-byte boundaries
static ztar_result_t pack_handle_padding(
    ztar_pack_t *stream, uint8_t *out, size_t remaining, size_t *written);
// Handles writing the end-of-archive zero blocks
static ztar_result_t pack_handle_trailer(
    ztar_pack_t *stream, uint8_t *out, size_t remaining, size_t *written);

/************************************************
 *         Global functions definition          *
 ***********************************************/

ztar_result_t ztar_unpack_init(
    ztar_unpack_t *stream, ztar_unpack_callbacks_t callbacks, void *user_data)
{
    if (!stream) {
        LOG_ERR("Called ztar_unpack_init with null stream pointer");
        return ZTAR_RESULT_INVALID_ARGS;
    }
    if (!callbacks.on_file_start || !callbacks.on_file_data || !callbacks.on_file_end) {
        LOG_ERR("Missing callback for unpacking context");
        return ZTAR_RESULT_INVALID_ARGS;
    }
    LOG_DBG("Initializing ztar unpacking stream");

    memset(stream, 0, sizeof(ztar_unpack_t));
    stream->initialized = true;
    stream->state = ZTAR_UNPACK_STATE_HEADER;
    stream->callbacks = callbacks;
    stream->user_data = user_data;

    return ZTAR_RESULT_OK;
}

bool ztar_unpack_is_initialized(const ztar_unpack_t *stream)
{
    if (!stream) {
        LOG_ERR("Called ztar_unpack_is_initialized with null stream pointer");
        return false;
    }
    return stream->initialized;
}

ztar_result_t ztar_unpack_process(ztar_unpack_t *stream, const uint8_t *data, size_t size)
{
    ztar_result_t zres = ZTAR_RESULT_OK;
    if (!stream || !data) {
        LOG_ERR("Called ztar_unpack_process with null stream pointer or data pointer");
        return ZTAR_RESULT_INVALID_ARGS;
    }
    LOG_DBG("Processing ztar stream data of size %zu", size);

    // Loop until we've consumed all input data
    size_t offset = 0;
    while (offset < size) {
        size_t bytes_consumed = 0;
        switch (stream->state) {
            case ZTAR_UNPACK_STATE_HEADER:
                zres = stream_process_header(stream, data + offset, size - offset, &bytes_consumed);
                break;
            case ZTAR_UNPACK_STATE_DATA:
                zres = stream_process_data(stream, data + offset, size - offset, &bytes_consumed);
                break;
            case ZTAR_UNPACK_STATE_PADDING:
                zres
                    = stream_process_padding(stream, data + offset, size - offset, &bytes_consumed);
                break;
            case ZTAR_UNPACK_STATE_TRAILER:
                zres
                    = stream_process_trailer(stream, data + offset, size - offset, &bytes_consumed);
                break;
            default:
                zres = ZTAR_RESULT_INVALID_ARCHIVE;
                break;
        }
        if (zres == ZTAR_RESULT_ARCHIVE_EXAHUSTED) {
            LOG_DBG("Completed processing ztar stream");
            return zres;
        }
        if (zres != ZTAR_RESULT_OK) {
            LOG_ERR("State machine error processing ztar stream: %d", zres);
            return zres;
        }
        offset += bytes_consumed;
    }

    return ZTAR_RESULT_OK;
}

ztar_result_t ztar_unpack_get_file_name(const ztar_header_t *header, char buffer[static 257])
{
    if (!header || !buffer) {
        LOG_ERR("Called ztar_unpack_get_file_name with null header pointer or buffer pointer");
        return ZTAR_RESULT_INVALID_ARGS;
    }
    LOG_DBG("Getting file name for header");

    if (header->prefix[0] != '\0') {
        snprintf(buffer, 257, "%.155s/%.100s", header->prefix, header->name);
    } else {
        snprintf(buffer, 257, "%.100s", header->name);
    }

    return ZTAR_RESULT_OK;
}

ztar_result_t ztar_unpack_get_file_size(const ztar_header_t *header, size_t *file_size)
{
    if (!header || !file_size) {
        LOG_ERR("Called ztar_unpack_get_file_size with null header pointer or file_size pointer");
        return ZTAR_RESULT_INVALID_ARGS;
    }

    // Size buf is null terminated
    char size_buf[13] = { 0 };
    memcpy(size_buf, header->size, 12);

    // Size is an octal string; we pass base 8.
    char *endptr = NULL;
    unsigned long long parsed_size = strtoull(size_buf, &endptr, 8);

    // Validate that the conversion consumed at least one digit
    if (endptr == size_buf) {
        return ZTAR_RESULT_INVALID_ARCHIVE;
    }

    *file_size = (size_t) parsed_size;
    return ZTAR_RESULT_OK;
}

ztar_result_t ztar_unpack_get_file_type(const ztar_header_t *header, ztar_filetype_t *file_type)
{
    if (!header || !file_type) {
        LOG_ERR("Called ztar_unpack_get_file_type with null header pointer or file_type pointer");
        return ZTAR_RESULT_INVALID_ARGS;
    }

    // In standard tar, both '0' and '\0' denote a regular file
    if (header->typeflag == '0' || header->typeflag == '\0') {
        *file_type = ZTAR_REGULAR_FILE;
    }
    // '5' denotes a directory
    else if (header->typeflag == '5') {
        *file_type = ZTAR_DIRECTORY;
    }
    // Anything else (including PAX 'x'/'g' headers) is unsupported
    else {
        LOG_ERR("Found unsupported file type with typeflag '%c'", header->typeflag);
        *file_type = ZTAR_UNSUPPORTED;
    }

    return ZTAR_RESULT_OK;
}

ztar_result_t ztar_pack_init(ztar_pack_t *stream, ztar_pack_callbacks_t callbacks, void *user_data)
{
    if (!stream) {
        LOG_ERR("Called ztar_pack_init with null pointer");
        return ZTAR_RESULT_INVALID_ARGS;
    }
    if (!callbacks.get_next_file || !callbacks.read_file_data) {
        LOG_ERR("Missing callback for packing context");
        return ZTAR_RESULT_INVALID_ARGS;
    }
    LOG_DBG("Initializing ztar packing stream");

    memset(stream, 0, sizeof(ztar_pack_t));
    stream->initialized = true;
    stream->state = ZTAR_PACK_STATE_NEXT_FILE;
    stream->callbacks = callbacks;
    stream->user_data = user_data;

    return ZTAR_RESULT_OK;
}

bool ztar_pack_is_initialized(const ztar_pack_t *stream)
{
    if (!stream) {
        LOG_ERR("Called ztar_pack_is_initialized with null pointer");
        return false;
    }
    return stream->initialized;
}

ztar_result_t ztar_pack_read_stream(
    ztar_pack_t *stream, uint8_t *out_buffer, size_t out_size, size_t *bytes_written)
{
    if (!stream || !out_buffer || !bytes_written) {
        return ZTAR_RESULT_INVALID_ARGS;
    }
    if (!stream->initialized) {
        return ZTAR_RESULT_INVALID_ARGS;
    }
    if (out_size < sizeof(ztar_header_t)) {
        return ZTAR_RESULT_INSUFFICIENT_BUFF;
    }

    *bytes_written = 0;
    ztar_result_t res = ZTAR_RESULT_OK;

    // Loop to fill the buffer as much as possible
    while (*bytes_written < out_size && stream->state != ZTAR_PACK_STATE_DONE) {
        size_t remaining_out = out_size - *bytes_written;
        uint8_t *current_out = out_buffer + *bytes_written;
        size_t step_written = 0;

        switch (stream->state) {
            case ZTAR_PACK_STATE_NEXT_FILE:
                res = pack_handle_next_file(stream);
                break;
            case ZTAR_PACK_STATE_HEADER:
                // If we don't have exactly 512 bytes left in the buffer, pause and wait for the
                // next call so we don't have to manage split headers across buffer boundaries.
                if (remaining_out < sizeof(ztar_header_t)) {
                    return ZTAR_RESULT_OK;
                }
                res = pack_handle_header(stream, current_out, remaining_out, &step_written);
                break;
            case ZTAR_PACK_STATE_DATA:
                res = pack_handle_data(stream, current_out, remaining_out, &step_written);
                break;
            case ZTAR_PACK_STATE_PADDING:
                res = pack_handle_padding(stream, current_out, remaining_out, &step_written);
                break;
            case ZTAR_PACK_STATE_TRAILER:
                res = pack_handle_trailer(stream, current_out, remaining_out, &step_written);
                break;
            case ZTAR_PACK_STATE_DONE:
                return ZTAR_RESULT_ARCHIVE_EXAHUSTED;
            default:
                return ZTAR_RESULT_INVALID_ARCHIVE;
        }

        if (res != ZTAR_RESULT_OK) {
            return res;
        }

        *bytes_written += step_written;
    }

    if (stream->state == ZTAR_PACK_STATE_DONE && *bytes_written == 0) {
        return ZTAR_RESULT_ARCHIVE_EXAHUSTED;
    }

    return ZTAR_RESULT_OK;
}

/************************************************
 *         Static functions definition          *
 ***********************************************/

static ztar_result_t get_chksum(const ztar_header_t *header, uint32_t *chksum)
{
    if (!header || !chksum) {
        return ZTAR_RESULT_INVALID_ARGS;
    }

    // Checksum buf is null terminated
    char chksum_buf[9] = { 0 };
    memcpy(chksum_buf, header->chksum, 8);

    // Checksum is an 8-byte octal string
    *chksum = (uint32_t) strtoul(chksum_buf, NULL, 8);
    return ZTAR_RESULT_OK;
}

static ztar_result_t validate_magic(const ztar_header_t *header)
{
    if (!header) {
        return ZTAR_RESULT_INVALID_ARGS;
    }

    // Strict POSIX USTAR uses "ustar\0".
    // This safely rejects GNU tar's "ustar " (space padded).
    if (memcmp(header->magic, "ustar\0", 6) != 0) {
        return ZTAR_RESULT_INVALID_MAGIC;
    }

    return ZTAR_RESULT_OK;
}

static ztar_result_t validate_version(const ztar_header_t *header)
{
    if (!header) {
        return ZTAR_RESULT_INVALID_ARGS;
    }

    // Strict POSIX USTAR uses "00".
    // This safely rejects GNU tar's " \0" (space and null).
    if (memcmp(header->version, "00", 2) != 0) {
        return ZTAR_RESULT_INVALID_VERSION;
    }

    return ZTAR_RESULT_OK;
}

static ztar_result_t validate_header(const ztar_header_t *header)
{
    ztar_result_t zres = ZTAR_RESULT_OK;
    if (!header) {
        return ZTAR_RESULT_INVALID_ARGS;
    }

    // Validate the magic indicator
    zres = validate_magic(header);
    if (zres != ZTAR_RESULT_OK) {
        return zres;
    }

    // Validate the version string
    zres = validate_version(header);
    if (zres != ZTAR_RESULT_OK) {
        return zres;
    }

    // Validate the checksum
    uint32_t parsed_chksum = 0;
    zres = get_chksum(header, &parsed_chksum);
    if (zres != ZTAR_RESULT_OK) {
        return zres;
    }

    // Calculate the sum of all bytes in the header block (512 bytes)
    uint32_t expected_chksum = 0;
    uint8_t *raw_header = (uint8_t *) header;
    for (size_t i = 0; i < sizeof(ztar_header_t); i++) {
        expected_chksum += raw_header[i];
    }

    // Subtract the actual bytes in the chksum field and replace them with spaces (' ')
    for (size_t i = 0; i < sizeof(header->chksum); i++) {
        expected_chksum -= (uint8_t) header->chksum[i];
        expected_chksum += ' ';
    }

    if (expected_chksum != parsed_chksum) {
        return ZTAR_RESULT_INVALID_CHECKSUM;
    }

    return ZTAR_RESULT_OK;
}

static ztar_result_t stream_process_header(
    ztar_unpack_t *stream, const uint8_t *data, size_t size, size_t *bytes_consumed)
{
    // Calculate how many bytes we still need to complete the header and copy as much as we can
    size_t header_bytes_needed = sizeof(ztar_header_t) - stream->bytes_processed_in_header;
    size_t bytes_to_copy = MIN(size, header_bytes_needed);
    memcpy((uint8_t *) &stream->current_header + stream->bytes_processed_in_header, data,
        bytes_to_copy);
    stream->bytes_processed_in_header += bytes_to_copy;

    // If we've completed the header, validate it and trigger the file start callback
    if (stream->bytes_processed_in_header == sizeof(ztar_header_t)) {
        // Check if this is the end of the archive
        uint8_t *raw_header = (uint8_t *) &stream->current_header;
        if ((raw_header[0] == 0)
            && (memcmp(raw_header, raw_header + 1, sizeof(ztar_header_t) - 1) == 0)) {
            stream->bytes_processed_in_trailer = 512;
            stream->state = ZTAR_UNPACK_STATE_TRAILER;
        } else {
            ztar_result_t zres = validate_header(&stream->current_header);
            if (zres != ZTAR_RESULT_OK) {
                return zres;
            }
            stream->bytes_processed_in_file = 0;
            stream->state = ZTAR_UNPACK_STATE_DATA;

            // Invoke the file start callback with the parsed header
            if (stream->callbacks.on_file_start) {
                int cbk_res
                    = stream->callbacks.on_file_start(&stream->current_header, stream->user_data);
                if (cbk_res != 0) {
                    LOG_ERR("File start callback returned error code %d", cbk_res);
                    return ZTAR_RESULT_USER_CBK_ERROR;
                }
            }
        }
    }

    // Output how many bytes we've consumed from the input data
    *bytes_consumed = bytes_to_copy;
    return ZTAR_RESULT_OK;
}

static ztar_result_t stream_process_data(
    ztar_unpack_t *stream, const uint8_t *data, size_t size, size_t *bytes_consumed)
{
    ztar_result_t zres = ZTAR_RESULT_OK;

    size_t file_size = 0;
    zres = ztar_unpack_get_file_size(&stream->current_header, &file_size);
    if (zres != ZTAR_RESULT_OK) {
        return zres;
    }

    // Calculate how many bytes we still need to complete the file and parse as much as we can
    size_t file_bytes_needed = file_size - stream->bytes_processed_in_file;
    size_t bytes_to_process = MIN(size, file_bytes_needed);

    // Invoke the file data callback with the parsed chunk
    if ((stream->callbacks.on_file_data) && (bytes_to_process != 0)) {
        int cbk_res = stream->callbacks.on_file_data(
            &stream->current_header, data, bytes_to_process, stream->user_data);
        if (cbk_res != 0) {
            LOG_ERR("File data callback returned error code %d", cbk_res);
            return ZTAR_RESULT_USER_CBK_ERROR;
        }
    }

    // Update how many bytes we've processed for the current file
    stream->bytes_processed_in_file += bytes_to_process;

    // If we've completed the file, trigger the file end callback and transition to the next state
    if (stream->bytes_processed_in_file == file_size) {
        if (stream->callbacks.on_file_end) {
            int cbk_res = stream->callbacks.on_file_end(&stream->current_header, stream->user_data);
            if (cbk_res != 0) {
                LOG_ERR("File end callback returned error code %d", cbk_res);
                return ZTAR_RESULT_USER_CBK_ERROR;
            }
        }

        // Calculate padding to next 512-byte boundary
        size_t required_padding = (512 - (file_size % 512)) % 512;
        if (required_padding == 0) {
            stream->bytes_processed_in_header = 0;
            stream->state = ZTAR_UNPACK_STATE_HEADER;
        } else {
            stream->bytes_processed_in_padding = 0;
            stream->state = ZTAR_UNPACK_STATE_PADDING;
        }
    }

    *bytes_consumed = bytes_to_process;
    return ZTAR_RESULT_OK;
}

static ztar_result_t stream_process_padding(
    ztar_unpack_t *stream, const uint8_t *data, size_t size, size_t *bytes_consumed)
{
    ztar_result_t zres = ZTAR_RESULT_OK;

    size_t file_size = 0;
    zres = ztar_unpack_get_file_size(&stream->current_header, &file_size);
    if (zres != ZTAR_RESULT_OK) {
        return zres;
    }

    // Calculate how many bytes we still need to complete the padding and parse as much as we can
    size_t required_padding = (512 - (file_size % 512)) % 512;
    size_t file_bytes_needed = required_padding - stream->bytes_processed_in_padding;
    size_t bytes_to_process = MIN(size, file_bytes_needed);

    // Padding bytes must be zero
    if ((data[0] != 0) || (memcmp(data, data + 1, bytes_to_process - 1) != 0)) {
        return ZTAR_RESULT_INVALID_ARCHIVE;
    }

    // Update how many bytes we've processed for the current padding
    stream->bytes_processed_in_padding += bytes_to_process;

    // If we've completed the padding, transition to the next state
    if (stream->bytes_processed_in_padding == required_padding) {
        stream->bytes_processed_in_header = 0;
        stream->state = ZTAR_UNPACK_STATE_HEADER;
    }

    *bytes_consumed = bytes_to_process;
    return ZTAR_RESULT_OK;
}

static ztar_result_t stream_process_trailer(
    ztar_unpack_t *stream, const uint8_t *data, size_t size, size_t *bytes_consumed)
{
    // Safety check to avoid processing more data if we've already consumed the entire trailer
    if (stream->bytes_processed_in_trailer == 1024) {
        return ZTAR_RESULT_ARCHIVE_EXAHUSTED;
    }

    // We are in the trailer state, we just need to consume 1024 bytes of zeros
    size_t trailer_bytes_needed = 1024 - stream->bytes_processed_in_trailer;
    size_t bytes_to_consume = MIN(size, trailer_bytes_needed);

    if ((data[0] != 0) || (memcmp(data, data + 1, bytes_to_consume - 1) != 0)) {
        return ZTAR_RESULT_INVALID_ARCHIVE;
    }

    stream->bytes_processed_in_trailer += bytes_to_consume;

    // If we've consumed the entire trailer, we can consider the archive fully processed
    if (stream->bytes_processed_in_trailer == 1024) {
        return ZTAR_RESULT_ARCHIVE_EXAHUSTED;
    }

    *bytes_consumed = bytes_to_consume;
    return ZTAR_RESULT_OK;
}

static int pack_format_octal(uint64_t value, char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return -1;
    }

    // This will produce a zero-padded octal string.
    // %0* : Pad with zeros '0', width provided by the first argument '*'
    // PRIo64 : Cross-platform macro for formatting uint64_t as octal
    int written = snprintf(out, out_size, "%0*" PRIo64, (int) (out_size - 1), value);
    if (written < 0 || (size_t) written >= out_size) {
        return -1;
    }
    return 0;
}

static ztar_result_t pack_handle_next_file(ztar_pack_t *stream)
{
    bool has_next = false;
    int res = stream->callbacks.get_next_file(
        &has_next, stream->current_file_name, &stream->expected_file_size, stream->user_data);
    if (res < 0) {
        LOG_ERR("User callback returned error getting next file");
        return ZTAR_RESULT_USER_CBK_ERROR;
    }

    if (has_next) {
        stream->bytes_written_for_file = 0;
        stream->state = ZTAR_PACK_STATE_HEADER;
    } else {
        stream->bytes_processed_in_trailer = 0;
        stream->state = ZTAR_PACK_STATE_TRAILER;
    }
    return ZTAR_RESULT_OK;
}

static ztar_result_t pack_handle_header(
    ztar_pack_t *stream, uint8_t *out, size_t remaining, size_t *written)
{
    *written = 0;

    ztar_header_t header;
    memset(&header, 0, sizeof(ztar_header_t));

    size_t name_len = strlen(stream->current_file_name);
    if (name_len <= 100) {
        strncpy(header.name, stream->current_file_name, sizeof(header.name));
    } else {
        // Find an appropriate '/' in the file name so we can split into prefix and name.
        size_t prefix_len = 0;
        size_t suffix_len = 0;
        const char *split_ptr = strchr(stream->current_file_name, '/');
        while (split_ptr) {
            prefix_len = split_ptr - stream->current_file_name;
            suffix_len = name_len - prefix_len - 1;
            if (prefix_len <= 155 && suffix_len <= 100) {
                break;
            }
            split_ptr = strchr(split_ptr + 1, '/');
        }

        // Early exit when we can't find a suitable split.
        if (!split_ptr) {
            LOG_ERR("File name too long and cannot be split into USTAR prefix/name: %s",
                stream->current_file_name);
            return ZTAR_RESULT_INVALID_ARGS;
        }

        // Copy prefix and name into their respective fields in the header
        memcpy(header.prefix, stream->current_file_name, prefix_len);
        strncpy(header.name, split_ptr + 1, sizeof(header.name));
    }

    if (pack_format_octal(0644, header.mode, sizeof(header.mode)) < 0) {
        return ZTAR_RESULT_INTERNAL_ERROR;
    }
    if (pack_format_octal(0, header.uid, sizeof(header.uid)) < 0) {
        return ZTAR_RESULT_INTERNAL_ERROR;
    }
    if (pack_format_octal(0, header.gid, sizeof(header.gid)) < 0) {
        return ZTAR_RESULT_INTERNAL_ERROR;
    }
    if (pack_format_octal(stream->expected_file_size, header.size, sizeof(header.size)) < 0) {
        return ZTAR_RESULT_INTERNAL_ERROR;
    }
    if (pack_format_octal(0, header.mtime, sizeof(header.mtime)) < 0) {
        return ZTAR_RESULT_INTERNAL_ERROR;
    }

    memcpy(header.magic, "ustar\0", 6);
    memcpy(header.version, "00", 2);
    header.typeflag = '0'; // Regular file, no directory support
    memset(header.chksum, ' ', sizeof(header.chksum));

    uint32_t chksum = 0;
    uint8_t *raw_header = (uint8_t *) &header;
    for (size_t i = 0; i < sizeof(ztar_header_t); i++) {
        chksum += raw_header[i];
    }

    snprintf(header.chksum, sizeof(header.chksum), "%06o", chksum);
    header.chksum[6] = '\0';
    header.chksum[7] = ' ';

    memcpy(out, &header, sizeof(ztar_header_t));
    *written = sizeof(ztar_header_t);

    if (stream->expected_file_size == 0) {
        stream->state = ZTAR_PACK_STATE_NEXT_FILE;
    } else {
        stream->state = ZTAR_PACK_STATE_DATA;
    }
    return ZTAR_RESULT_OK;
}

static ztar_result_t pack_handle_data(
    ztar_pack_t *stream, uint8_t *out, size_t remaining, size_t *written)
{
    *written = 0;
    size_t bytes_to_read
        = MIN(remaining, stream->expected_file_size - stream->bytes_written_for_file);

    size_t bytes_read = 0;
    int read_res
        = stream->callbacks.read_file_data(out, bytes_to_read, stream->user_data, &bytes_read);
    if (read_res < 0) {
        LOG_ERR("User callback returned error reading file data");
        return ZTAR_RESULT_USER_CBK_ERROR;
    }

    // Check if user callback failed to provide requested data prematurely
    if (bytes_read == 0 && bytes_to_read > 0) {
        LOG_ERR("User callback read 0 bytes but %zu were requested", bytes_to_read);
        return ZTAR_RESULT_TRUNCATED_FILE;
    }

    stream->bytes_written_for_file += bytes_read;
    *written = bytes_read;

    if (stream->bytes_written_for_file == stream->expected_file_size) {
        size_t required_padding = (512 - (stream->expected_file_size % 512)) % 512;
        if (required_padding > 0) {
            stream->bytes_processed_in_padding = 0;
            stream->state = ZTAR_PACK_STATE_PADDING;
        } else {
            stream->state = ZTAR_PACK_STATE_NEXT_FILE;
        }
    }
    return ZTAR_RESULT_OK;
}

static ztar_result_t pack_handle_padding(
    ztar_pack_t *stream, uint8_t *out, size_t remaining, size_t *written)
{
    size_t total_padding = (512 - (stream->expected_file_size % 512)) % 512;
    size_t padding_remaining = total_padding - stream->bytes_processed_in_padding;
    size_t write_padding = MIN(remaining, padding_remaining);

    memset(out, 0, write_padding);
    stream->bytes_processed_in_padding += write_padding;
    *written = write_padding;

    if (stream->bytes_processed_in_padding == total_padding) {
        stream->state = ZTAR_PACK_STATE_NEXT_FILE;
    }
    return ZTAR_RESULT_OK;
}

static ztar_result_t pack_handle_trailer(
    ztar_pack_t *stream, uint8_t *out, size_t remaining, size_t *written)
{
    size_t trailer_remaining = 1024 - stream->bytes_processed_in_trailer;
    size_t write_trailer = MIN(remaining, trailer_remaining);

    memset(out, 0, write_trailer);
    stream->bytes_processed_in_trailer += write_trailer;
    *written = write_trailer;

    if (stream->bytes_processed_in_trailer == 1024) {
        stream->state = ZTAR_PACK_STATE_DONE;
    }
    return ZTAR_RESULT_OK;
}
