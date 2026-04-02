#ifndef ZTAR_H
#define ZTAR_H

/**
 * @file ztar.h
 * @brief Tar archive extraction and packing stream parser.
 * @details This parser only supports the USTAR format and is designed to work with streaming data,
 * making it suitable for processing large tar archives without needing to load the entire archive
 * into memory. It provides callbacks for file start, file data, and file end events, allowing users
 * to handle extracted files as they are processed from the stream. It also supports packing files
 * into a stream.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Result codes for ztar stream operations. */
typedef enum
{
    /** @brief Operation completed successfully. */
    ZTAR_RESULT_OK = 0,
    /** @brief Internal error, should not occur. */
    ZTAR_RESULT_INTERNAL_ERROR,
    /** @brief Invalid arguments provided to the function. */
    ZTAR_RESULT_INVALID_ARGS,
    /** @brief The archive format is invalid. */
    ZTAR_RESULT_INVALID_ARCHIVE,
    /** @brief The provided buffer is insufficient. */
    ZTAR_RESULT_INSUFFICIENT_BUFF,
    /** @brief The TAR magic indicator is invalid. */
    ZTAR_RESULT_INVALID_MAGIC,
    /** @brief The TAR version is invalid. */
    ZTAR_RESULT_INVALID_VERSION,
    /** @brief The checksum of the header is invalid. */
    ZTAR_RESULT_INVALID_CHECKSUM,
    /** @brief The file within the archive is truncated. */
    ZTAR_RESULT_TRUNCATED_FILE,
    /** @brief The archive itself is truncated. */
    ZTAR_RESULT_TRUNCATED_ARCHIVE,
    /** @brief The archive stream has been fully exhausted. */
    ZTAR_RESULT_ARCHIVE_EXAHUSTED,
    /** @brief The user callback returned an error. */
    ZTAR_RESULT_USER_CBK_ERROR,
} ztar_result_t;

/** @brief Extracted TAR file types. */
typedef enum
{
    /** @brief Standard regular file. */
    ZTAR_REGULAR_FILE,
    /** @brief Directory file type. */
    ZTAR_DIRECTORY,
    /** @brief Handles symlinks, hardlinks, FIFOs, and PAX extension headers. */
    ZTAR_UNSUPPORTED
} ztar_filetype_t;

/**
 * @brief Standard POSIX TAR header structure.
 * @note This structure is exactly 512 bytes, which is the size of a TAR header block. It relies
 * on the compiler not adding any padding between fields, which is generally safe given the
 * use of only char arrays and single char fields, but should be verified if any changes are made.
 */
typedef struct
{
    /** @brief Name of the file. */
    char name[100];
    /** @brief File permissions mode. */
    char mode[8];
    /** @brief Numeric user ID of the file owner. */
    char uid[8];
    /** @brief Numeric group ID of the file owner. */
    char gid[8];
    /** @brief File size in bytes (octal string). */
    char size[12];
    /** @brief Last modification time in numeric Unix time format. */
    char mtime[12];
    /** @brief Checksum for the header block. */
    char chksum[8];
    /** @brief Link indicator / file type flag. */
    char typeflag;
    /** @brief Name of the linked file. */
    char linkname[100];
    /** @brief UStar magic indicator ("ustar"). */
    char magic[6];
    /** @brief UStar version ("00"). */
    char version[2];
    /** @brief Owner user name. */
    char uname[32];
    /** @brief Owner group name. */
    char gname[32];
    /** @brief Device major number. */
    char devmajor[8];
    /** @brief Device minor number. */
    char devminor[8];
    /** @brief Prefix for the file name. */
    char prefix[155];
    /** @brief Padding to fill the 512-byte block. */
    char padding[12];
} ztar_header_t;

/** @brief Callbacks for ztar stream parsing events. */
typedef struct
{
    /**
     * @brief Callback invoked when a new file starts in the archive.
     *
     * @param[in] header Pointer to the parsed TAR header.
     * @param[in,out] user_data User specified data.
     * @return 0 if successful, -1 on error. The stream parser will abort if an error is returned.
     */
    int (*on_file_start)(const ztar_header_t *header, void *user_data);

    /**
     * @brief Callback invoked when file data is read from the stream.
     *
     * @param[in] header Pointer to the parsed TAR header.
     * @param[in] data Pointer to the file data chunk.
     * @param[in] size Size of the file data chunk.
     * @param[in,out] user_data User specified data.
     * @return 0 if successful, -1 on error. The stream parser will abort if an error is returned.
     */
    int (*on_file_data)(
        const ztar_header_t *header, const uint8_t *data, size_t size, void *user_data);

    /**
     * @brief Callback invoked when a file ends in the archive.
     *
     * @param[in] header Pointer to the parsed TAR header.
     * @param[in,out] user_data User specified data.
     * @return 0 if successful, -1 on error. The stream parser will abort if an error is returned.
     */
    int (*on_file_end)(const ztar_header_t *header, void *user_data);
} ztar_unpack_callbacks_t;

/** @brief Internal states of the TAR stream parser. */
typedef enum
{
    /** @brief Parser is currently reading a header block. */
    ZTAR_UNPACK_STATE_HEADER = 0,
    /** @brief Parser is currently reading file data. */
    ZTAR_UNPACK_STATE_DATA,
    /** @brief Parser is currently reading file padding blocks. */
    ZTAR_UNPACK_STATE_PADDING,
    /** @brief Parser is reading the archive end trailer blocks. */
    ZTAR_UNPACK_STATE_TRAILER
} ztar_unpack_state_t;

/** @brief Data struct for a ztar stream parser instance. */
typedef struct
{
    /** @brief Indicates if the stream parser has been initialized. */
    bool initialized;
    /** @brief Current internal state of the stream parser. */
    ztar_unpack_state_t state;
    /** @brief The TAR header currently being processed. */
    ztar_header_t current_header;
    /** @brief Number of bytes processed in the current header. */
    size_t bytes_processed_in_header;
    /** @brief Number of bytes processed in the current file data. */
    size_t bytes_processed_in_file;
    /** @brief Number of bytes processed in the current file's block padding. */
    size_t bytes_processed_in_padding;
    /** @brief Number of bytes processed in the archive trailer. */
    size_t bytes_processed_in_trailer;
    /** @brief Registered callbacks for parsing events. */
    ztar_unpack_callbacks_t callbacks;
    /** @brief User data passed to callback functions. */
    void *user_data;
} ztar_unpack_t;

/**
 * @brief Initialize the ztar stream parser.
 *
 * @param[in,out] stream Pointer to the ztar stream to initialize.
 * @param[in] callbacks Struct containing parser event callbacks.
 * @param[in] user_data User specified data to pass to the callbacks.
 * @return ZTAR_RESULT_OK if successful, otherwise a ztar_result_t error code.
 */
ztar_result_t ztar_unpack_init(
    ztar_unpack_t *stream, ztar_unpack_callbacks_t callbacks, void *user_data);

/**
 * @brief Check if the ztar stream parser is initialized.
 *
 * @param[in] stream Pointer to the ztar stream.
 * @return True if initialized, false if not.
 */
bool ztar_unpack_is_initialized(const ztar_unpack_t *stream);

/**
 * @brief Process a chunk of data through the TAR stream parser.
 *
 * @param[in,out] stream Pointer to the initialized ztar stream.
 * @param[in] data Pointer to the chunk of TAR archive data.
 * @param[in] size Size of the TAR archive data chunk.
 * @return ZTAR_RESULT_OK if successful, otherwise a ztar_result_t error code.
 */
ztar_result_t ztar_unpack_process(ztar_unpack_t *stream, const uint8_t *data, size_t size);

/**
 * @brief Extract the full file name from a parsed TAR header.
 * @details In USTAR, if the prefix is populated, the full path is prefix + '/' + name.
 *
 * @param[in] header Pointer to the parsed TAR header.
 * @param[out] buffer Output buffer for the file name (must be at least 257 bytes).
 * @return ZTAR_RESULT_OK if successful, otherwise a ztar_result_t error code.
 */
ztar_result_t ztar_unpack_get_file_name(const ztar_header_t *header, char buffer[static 257]);

/**
 * @brief Extract the file size from a parsed TAR header.
 *
 * @param[in] header Pointer to the parsed TAR header.
 * @param[out] file_size Output pointer for the extracted file size.
 * @return ZTAR_RESULT_OK if successful, otherwise a ztar_result_t error code.
 */
ztar_result_t ztar_unpack_get_file_size(const ztar_header_t *header, size_t *file_size);

/**
 * @brief Extract the file type from a parsed TAR header.
 * @details Restricts output strictly to regular files and directories, all other types are
 * considered unsupported.
 *
 * @param[in] header Pointer to the parsed TAR header.
 * @param[out] file_type Output pointer for the extracted file type.
 * @return ZTAR_RESULT_OK if successful, otherwise a ztar_result_t error code.
 */
ztar_result_t ztar_unpack_get_file_type(const ztar_header_t *header, ztar_filetype_t *file_type);

/** @brief Internal states of the TAR stream packer. */
typedef enum
{
    ZTAR_PACK_STATE_NEXT_FILE = 0,
    ZTAR_PACK_STATE_HEADER,
    ZTAR_PACK_STATE_DATA,
    ZTAR_PACK_STATE_PADDING,
    ZTAR_PACK_STATE_TRAILER,
    ZTAR_PACK_STATE_DONE
} ztar_pack_state_t;

/** @brief Callbacks for ztar stream packing events. */
typedef struct
{
    /**
     * @brief Called when the packer needs the next file's metadata.
     * @param[out] has_next Pointer to a boolean indicating if there are more files.
     * @param[out] name Buffer to copy the file name into.
     * @param[out] size Pointer to write the file size into.
     * @param[in] user_data User specified data.
     * @return 0 on success, -1 on error.
     */
    int (*get_next_file)(bool *has_next, char name[static 257], size_t *size, void *user_data);

    /**
     * @brief Called when the packer needs raw file data.
     * @param[out] buffer Destination buffer for the file data.
     * @param[in] max_size Maximum number of bytes to read.
     * @param[in] user_data User specified data.
     * @param[out] bytes_read Pointer to write the actual number of bytes read into.
     * @return 0 on success, -1 on error.
     */
    int (*read_file_data)(uint8_t *buffer, size_t max_size, void *user_data, size_t *bytes_read);
} ztar_pack_callbacks_t;

/** @brief Data struct for a ztar stream packer instance. */
typedef struct
{
    /** @brief Indicates if the stream packer has been initialized. */
    bool initialized;
    ztar_pack_state_t state;
    char current_file_name[257];
    /** @brief Expected size of the file currently being packed. */
    size_t expected_file_size;
    /** @brief Number of bytes written for the current file data so far. */
    size_t bytes_written_for_file;
    size_t bytes_processed_in_padding;
    size_t bytes_processed_in_trailer;
    ztar_pack_callbacks_t callbacks;
    void *user_data;
} ztar_pack_t;

ztar_result_t ztar_pack_init(ztar_pack_t *stream, ztar_pack_callbacks_t callbacks, void *user_data);

bool ztar_pack_is_initialized(const ztar_pack_t *stream);

ztar_result_t ztar_pack_read_stream(
    ztar_pack_t *stream, uint8_t *out_buffer, size_t out_size, size_t *bytes_written);

#ifdef __cplusplus
}
#endif

#endif
