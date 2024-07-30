/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ota.h"

#include "edgehog_device/device.h"
#include "edgehog_device/ota_event.h"
#include "edgehog_private.h"
#include "generated_interfaces.h"
#include "http.h"
#include "settings.h"

#include <stdlib.h>

#include <time.h>

#include <zephyr/device.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>

#include "log.h"
EDGEHOG_LOG_MODULE_REGISTER(ota, CONFIG_EDGEHOG_DEVICE_OTA_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define OTA_REQ_TIMEOUT_MS (60 * 1000)
#define MAX_OTA_RETRY 5
#define OTA_PROGRESS_PERC 100
#define OTA_PROGRESS_PERC_ROUNDING_STEP 10
#define OTA_ATTEMPS_DELAY_MS 2000

#define SLOT0_LABEL slot0_partition
#define SLOT1_LABEL slot1_partition

/* FIXED_PARTITION_ID() values used below are auto-generated by DT */
#define FLASH_AREA_IMAGE_PRIMARY FIXED_PARTITION_ID(SLOT0_LABEL)
#define FLASH_AREA_IMAGE_SECONDARY FIXED_PARTITION_ID(SLOT1_LABEL)

#define OTA_KEY "ota"
#define OTA_STATE_KEY "state"
#define OTA_REQUEST_ID_KEY "req_id"

#define THREAD_STACK_SIZE 8192
#define OTA_STATE_RUN_BIT (1)

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
K_THREAD_STACK_DEFINE(ota_thread_stack, THREAD_STACK_SIZE);
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef CONFIG_EDGEHOG_DEVICE_ZBUS_OTA_EVENT
ZBUS_CHAN_DEFINE(edgehog_ota_chan, edgehog_ota_chan_event_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.event = EDGEHOG_OTA_INVALID_EVENT));
#endif
/**
 * @brief OTA machine states.
 *
 * @details Defines the internal OTA machine states.
 */
typedef enum
{
    /** @brief The OTA machine is in idle State. */
    OTA_STATE_IDLE = 1,
    /** @brief The OTA machine is in Progress state. */
    OTA_STATE_IN_PROGRESS = 2,
    /** @brief The OTA machine is in Reboot state. */
    OTA_STATE_REBOOT = 3,
} ota_state_t;

/**
 * @brief Edgehog OTA Event codes.
 *
 * @details Defines an OTA event to stream OTA status to the server.
 */
typedef enum
{
    /** @brief The device received an OTA Request. */
    OTA_EVENT_ACKNOWLEDGED = 1,
    /** @brief OTA update is in the process of downloading. */
    OTA_EVENT_DOWNLOADING = 2,
    /** @brief OTA update is in the process of deploying. */
    OTA_EVENT_DEPLOYING = 3,
    /** @brief OTA update is deployed on the device. */
    OTA_EVENT_DEPLOYED = 4,
    /** @brief The device is in the process of rebooting. */
    OTA_EVENT_REBOOTING = 5,
    /** @brief OTA update succeeded. This is a final status of OTA Update*/
    OTA_EVENT_SUCCESS = 6,
    /** @brief An error happened during the OTA update. */
    OTA_EVENT_ERROR = 7,
    /** @brief An OTA update failed. This is a final status of OTA Update*/
    OTA_EVENT_FAILURE = 8
} ota_event_t;

/**
 * @brief OTA settings data.
 *
 * @details Defines the OTA data used by Edgehog settings.
 */
typedef struct
{
    /** @brief OTA request UUID. */
    char uuid[ASTARTE_UUID_STR_LEN + 1];
    /** @brief OTA state. */
    uint8_t ota_state;
} ota_settings_t;

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Convert BOOT SWAP TYPE to string.
 */
static const char *swap_type_str(int swap_type);

/**
 * @brief OTA thread entry function.
 */
static void ota_thread_entry_point(void *edgehog_device, void *ptr2, void *ptr3);

/**
 * @brief Callback used when download data is received from the server.
 */
static edgehog_result_t http_download_payload_cbk(
    int sock_id, http_download_chunk_t *download_chunk, void *user_data);
/**
 * @brief Publish an OTA update event to Astarte.
 *
 * @param[in] astarte_device Handle to the astarte device instance.
 * @param[in] request_uuid Uuid for the OTA request.
 * @param[in] event Event to publish.
 * @param[in] status_progress Percentage of progress for the operation.
 * @param[in] error Possible errors from edgehog generated during the OTA operation.
 * @param[in] message Additional message to append to the OTA update event.
 */
static void pub_ota_event(astarte_device_handle_t astarte_device, const char *request_uuid,
    ota_event_t event, int32_t status_progress, edgehog_result_t error, const char *message);

/**
 * @brief Handle an OTA update operation event.
 *
 * @param[in] edgehog_dev Handle to the edgehog device instance.
 * @param[in] ota_request OTA data request.
 */
static edgehog_result_t edgehog_ota_event_update(
    edgehog_device_handle_t edgehog_dev, ota_request_t *ota_request);

static edgehog_result_t perform_ota(edgehog_device_handle_t edgehog_device);
static edgehog_result_t perform_ota_attempt(edgehog_device_handle_t edgehog_device);

/**
 * @brief Handle an OTA cancel operation event.
 *
 * @param[in] edgehog_dev Handle to the edgehog device instance..
 * @param[in] request_uuid OTA UUID request.
 */
static edgehog_result_t edgehog_ota_event_cancel(
    edgehog_device_handle_t edgehog_dev, const char *request_uuid);

/**
 * @brief Handle ota settings loading.
 *
 * @param[in] key the name with skipped part that was used as name in handler registration.
 * @param[in] len the size of the data found in the backend.
 * @param[in] read_cb function provided to read the data from the backend.
 * @param[inout] cb_arg arguments for the read function provided by the backend.
 * @param[inout] param parameter given to the settings_load_subtree_direct function.
 *
 * @return When nonzero value is returned, further subtree searching is stopped.
 */
static int ota_settings_loader(
    const char *key, size_t len, settings_read_cb read_cb, void *cb_arg, void *param);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

void edgehog_ota_init(edgehog_device_handle_t edgehog_dev)
{
    if (!edgehog_dev) {
        EDGEHOG_LOG_ERR("Unable to init ota edgehog_device undefined");
        return;
    }

    memset(&edgehog_dev->ota_thread, 0, sizeof(ota_thread_t));

    // Step 1 check if an UUID is present in Edgehog settings. If not there is no need to continue
    // as there is no pending OTA update.

    ota_settings_t ota_settings = { 0 };
    edgehog_result_t res = edgehog_settings_load("ota", ota_settings_loader, &ota_settings);
    if (res != EDGEHOG_RESULT_OK) {
        EDGEHOG_LOG_ERR("Edgehog Settings load failed");
        return;
    }

    if (strlen(ota_settings.uuid) != ASTARTE_UUID_STR_LEN) {
        EDGEHOG_LOG_INF("No OTA update request UUID found from Edgehog Settings");
        goto end;
    }

    // Step 3 check if the OTA update state is reboot. If not notify astarte of the error.

    if (ota_settings.ota_state != OTA_STATE_REBOOT) {
        EDGEHOG_LOG_ERR("Unable to fetch the OTA state from Edgehog settings");
        pub_ota_event(edgehog_dev->astarte_device, ota_settings.uuid, OTA_EVENT_FAILURE, 0,
            EDGEHOG_RESULT_OTA_INTERNAL_ERROR, "");
        goto end;
    }

    int swap_type = mcuboot_swap_type();
    if (swap_type != BOOT_SWAP_TYPE_NONE) {
        EDGEHOG_LOG_ERR(
            "Unable to swap the contents to slot 1. Swap type:%s", swap_type_str(swap_type));
        pub_ota_event(edgehog_dev->astarte_device, ota_settings.uuid, OTA_EVENT_FAILURE, 0,
            EDGEHOG_RESULT_OTA_SWAP_FAIL, "");
        goto end;
    }

    if (boot_is_img_confirmed()) {
        EDGEHOG_LOG_ERR("Boot Image is alredy confirmed, it is not an OTA update process");
        pub_ota_event(edgehog_dev->astarte_device, ota_settings.uuid, OTA_EVENT_FAILURE, 0,
            EDGEHOG_RESULT_OTA_SWAP_FAIL, "");
        goto end;
    }

    int ret = boot_write_img_confirmed();
    if (ret < 0) {
        EDGEHOG_LOG_ERR("Couldn't confirm this image: %d", ret);
        pub_ota_event(edgehog_dev->astarte_device, ota_settings.uuid, OTA_EVENT_FAILURE, 0,
            EDGEHOG_RESULT_OTA_INTERNAL_ERROR, "");
        goto end;
    }

    EDGEHOG_LOG_INF("Marked image as OK");

    pub_ota_event(edgehog_dev->astarte_device, ota_settings.uuid, OTA_EVENT_SUCCESS, 0,
        EDGEHOG_RESULT_OK, "");

end:
    edgehog_settings_delete(OTA_KEY, OTA_REQUEST_ID_KEY);
    ota_settings.ota_state = OTA_STATE_IDLE;
    edgehog_settings_save(
        OTA_KEY, OTA_STATE_KEY, &ota_settings.ota_state, sizeof(ota_settings.ota_state));
}

edgehog_result_t edgehog_ota_event(
    edgehog_device_handle_t edgehog_dev, astarte_device_datastream_object_event_t *object_event)
{
    if (!object_event) {
        EDGEHOG_LOG_ERR("Unable to handle event, object event undefined");
        return EDGEHOG_RESULT_OTA_INVALID_REQUEST;
    }

    astarte_object_entry_t *rx_values = object_event->entries;
    size_t rx_values_length = object_event->entries_len;

    char *req_uuid = NULL;
    char *ota_url = NULL;
    char *ota_operation = NULL;

    for (size_t i = 0; i < rx_values_length; i++) {
        const char *path = rx_values[i].path;
        astarte_individual_t rx_value = rx_values[i].individual;

        if (strcmp(path, "uuid") == 0) {
            req_uuid = (char *) rx_value.data.string;
            EDGEHOG_LOG_INF("uuid: %s", rx_value.data.string);
        } else if (strcmp(path, "url") == 0) {
            ota_url = (char *) rx_value.data.string;
            EDGEHOG_LOG_INF("url: %s", rx_value.data.string);
        } else if (strcmp(path, "operation") == 0) {
            ota_operation = (char *) rx_value.data.string;
            EDGEHOG_LOG_INF("operation: %s", rx_value.data.string);
        }
    }

    if (!req_uuid || !ota_operation) {
        EDGEHOG_LOG_ERR("Unable to extract data from request");
        return EDGEHOG_RESULT_OTA_INVALID_REQUEST;
    }

    edgehog_result_t res = EDGEHOG_RESULT_OK;

    if (strcmp("Update", ota_operation) == 0) {
        if (!ota_url) {
            EDGEHOG_LOG_ERR("Unable to extract data from request");
            return EDGEHOG_RESULT_OTA_INVALID_REQUEST;
        }

        ota_request_t ota_request = {
            .download_url = ota_url,
            .uuid = req_uuid,
        };

        res = edgehog_ota_event_update(edgehog_dev, &ota_request);

    } else if (strcmp("Cancel", ota_operation) == 0) {
        res = edgehog_ota_event_cancel(edgehog_dev, req_uuid);
    } else {
        pub_ota_event(edgehog_dev->astarte_device, req_uuid, OTA_EVENT_FAILURE, 0,
            EDGEHOG_RESULT_OTA_INVALID_REQUEST, "");
        res = EDGEHOG_RESULT_OTA_INVALID_REQUEST;
    }

    return res;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/
static edgehog_result_t edgehog_ota_event_update(
    edgehog_device_handle_t edgehog_device, ota_request_t *ota_request)
{
    if (atomic_test_bit(
            &edgehog_device->ota_thread.ota_thread_data.ota_run_state, OTA_STATE_RUN_BIT)) {
        pub_ota_event(edgehog_device->astarte_device, ota_request->uuid, OTA_EVENT_FAILURE, 0,
            EDGEHOG_RESULT_OTA_ALREADY_IN_PROGRESS, "");
        return EDGEHOG_RESULT_OTA_ALREADY_IN_PROGRESS;
    }

    ota_thread_data_t *ota_thread_data = &edgehog_device->ota_thread.ota_thread_data;
    ota_thread_data->ota_request.uuid = NULL;
    ota_thread_data->ota_request.download_url = NULL;

    edgehog_result_t edgehog_result = EDGEHOG_RESULT_OK;
    memset(ota_thread_data, 0, sizeof(ota_thread_data_t));

    size_t req_uuid_len = strlen(ota_request->uuid);
    ota_thread_data->ota_request.uuid = (char *) calloc((req_uuid_len + 1), sizeof(char));
    if (!ota_thread_data->ota_request.uuid) {
        EDGEHOG_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        edgehog_result = EDGEHOG_RESULT_OUT_OF_MEMORY;
        goto fail;
    }
    strncpy(ota_thread_data->ota_request.uuid, ota_request->uuid, req_uuid_len + 1);

    size_t ota_url_len = strlen(ota_request->download_url);
    ota_thread_data->ota_request.download_url = (char *) calloc((ota_url_len + 1), sizeof(char));
    if (!ota_thread_data->ota_request.download_url) {
        EDGEHOG_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        edgehog_result = EDGEHOG_RESULT_OUT_OF_MEMORY;
        goto fail;
    }
    strncpy(ota_thread_data->ota_request.download_url, ota_request->download_url, ota_url_len + 1);

    if (atomic_test_and_set_bit(&ota_thread_data->ota_run_state, OTA_STATE_RUN_BIT)) {
        EDGEHOG_LOG_ERR("Unable to set OTA RUN BIT");
        edgehog_result = EDGEHOG_RESULT_OUT_OF_MEMORY;
        goto fail;
    }

    struct k_thread *thread_handle = &edgehog_device->ota_thread.ota_thread_handle;
    memset(thread_handle, 0, sizeof(struct k_thread));

    k_tid_t thread_id = k_thread_create(thread_handle, ota_thread_stack, THREAD_STACK_SIZE,
        ota_thread_entry_point, edgehog_device, NULL, NULL, K_HIGHEST_THREAD_PRIO, 0, K_NO_WAIT);

    if (!thread_id) {
        EDGEHOG_LOG_ERR("OTA update thread creation failed.");
        pub_ota_event(edgehog_device->astarte_device, ota_request->uuid, OTA_EVENT_FAILURE, 0,
            EDGEHOG_RESULT_OTA_INTERNAL_ERROR, "");
        edgehog_result = EDGEHOG_RESULT_THREAD_CREATE_ERROR;
        goto fail;
    }

    return edgehog_result;

fail:
    free(ota_thread_data->ota_request.download_url);
    free(ota_thread_data->ota_request.uuid);

    return edgehog_result;
}

static void ota_thread_entry_point(void *edgehog_device, void *ptr2, void *ptr3)
{
    if (!edgehog_device) {
        EDGEHOG_LOG_ERR("Unable to handle ota_thread, edgehog_device is undefined.");
        return;
    }

    ARG_UNUSED(ptr2);
    ARG_UNUSED(ptr3);

    edgehog_device_handle_t edgehog_dev = (edgehog_device_handle_t) edgehog_device;
    ota_thread_data_t *ota_thread_data = &edgehog_dev->ota_thread.ota_thread_data;
    const char *req_uuid = ota_thread_data->ota_request.uuid;

    // Step 1 acknowledge the valid update request and notify the start of the download
    // operation.

    pub_ota_event(
        edgehog_dev->astarte_device, req_uuid, OTA_EVENT_ACKNOWLEDGED, 0, EDGEHOG_RESULT_OK, "");

    // Step 2 Init Edgehog settings for the OTA update

    EDGEHOG_LOG_INF("OTA INIT");

    edgehog_result_t edgehog_result = edgehog_settings_init();
    if (edgehog_result != EDGEHOG_RESULT_OK) {
        EDGEHOG_LOG_ERR("Edgehog Settings Init failed");
        EDGEHOG_LOG_WRN("OTA FAILED");
        pub_ota_event(edgehog_dev->astarte_device, req_uuid, OTA_EVENT_FAILURE, 0,
            EDGEHOG_RESULT_SETTINGS_INIT_FAIL, "");
        goto selfdestruct;
    }

    // Step 3 perform the OTA update

    EDGEHOG_LOG_INF("DOWNLOAD_AND_DEPLOY");
    uint8_t ota_state = OTA_STATE_IN_PROGRESS;
    edgehog_settings_save(OTA_KEY, OTA_STATE_KEY, &ota_state, sizeof(uint8_t));

    edgehog_result = perform_ota(edgehog_dev);
    if (edgehog_result == EDGEHOG_RESULT_OK) {
        pub_ota_event(
            edgehog_dev->astarte_device, req_uuid, OTA_EVENT_DEPLOYING, 0, EDGEHOG_RESULT_OK, "");
        EDGEHOG_LOG_INF("OTA PREPARE REBOOT");
        ota_state = OTA_STATE_REBOOT;
        edgehog_settings_save(OTA_KEY, OTA_STATE_KEY, &ota_state, sizeof(uint8_t));

        struct mcuboot_img_header hdr;
        memset(&hdr, 0, sizeof(struct mcuboot_img_header));

        int err = boot_read_bank_header(FLASH_AREA_IMAGE_SECONDARY, &hdr, sizeof(hdr));
        if (err) {
            EDGEHOG_LOG_ERR(
                "Failed to read sec area (%u) header: %d", FLASH_AREA_IMAGE_SECONDARY, err);
            pub_ota_event(edgehog_dev->astarte_device, req_uuid, OTA_EVENT_FAILURE, 0,
                EDGEHOG_RESULT_OTA_INTERNAL_ERROR, "");
            goto selfdestruct;
        }

        err = boot_request_upgrade(BOOT_UPGRADE_TEST);
        if (err) {
            EDGEHOG_LOG_ERR("Failed to mark the image in slot 1 as pending %d", err);
            pub_ota_event(edgehog_dev->astarte_device, req_uuid, OTA_EVENT_FAILURE, 0,
                EDGEHOG_RESULT_OTA_INTERNAL_ERROR, "");
            goto selfdestruct;
        }

        pub_ota_event(
            edgehog_dev->astarte_device, req_uuid, OTA_EVENT_DEPLOYED, 0, EDGEHOG_RESULT_OK, "");
        pub_ota_event(
            edgehog_dev->astarte_device, req_uuid, OTA_EVENT_REBOOTING, 0, EDGEHOG_RESULT_OK, "");
        EDGEHOG_LOG_INF("Device restart in 5 seconds");
        k_sleep(K_SECONDS(5));
        EDGEHOG_LOG_INF("Device restart now");
        sys_reboot(SYS_REBOOT_WARM);

    } else {
        EDGEHOG_LOG_WRN("OTA FAILED");
        pub_ota_event(
            edgehog_dev->astarte_device, req_uuid, OTA_EVENT_FAILURE, 0, edgehog_result, "");
        ota_state = OTA_STATE_IDLE;
        edgehog_settings_save(OTA_KEY, OTA_STATE_KEY, &ota_state, sizeof(uint8_t));
    }

selfdestruct:
    atomic_clear_bit(&edgehog_dev->ota_thread.ota_thread_data.ota_run_state, OTA_STATE_RUN_BIT);

    free(ota_thread_data->ota_request.uuid);
    free(ota_thread_data->ota_request.download_url);
    edgehog_settings_delete(OTA_KEY, OTA_REQUEST_ID_KEY);
    ota_state = OTA_STATE_IDLE;
    edgehog_settings_save(OTA_KEY, OTA_STATE_KEY, &ota_state, sizeof(uint8_t));
}

static edgehog_result_t perform_ota(edgehog_device_handle_t edgehog_device)
{
    edgehog_result_t edgehog_result = EDGEHOG_RESULT_OK;

    astarte_device_handle_t astarte_device = edgehog_device->astarte_device;
    ota_thread_data_t *thread_data = &edgehog_device->ota_thread.ota_thread_data;

    int err = boot_erase_img_bank(FIXED_PARTITION_ID(SLOT1_LABEL));
    if (err) {
        EDGEHOG_LOG_ERR("Failed to erase second slot: %d", err);
        return EDGEHOG_RESULT_OTA_ERASE_SECOND_SLOT_ERROR;
    }

    err = flash_img_init(&thread_data->flash_ctx);
    if (err) {
        EDGEHOG_LOG_ERR("Unable to init flash area: %d", err);
        return EDGEHOG_RESULT_OTA_INIT_FLASH_ERROR;
    }

    // Step 1 set the request ID to the received uuid in Settings
    edgehog_result = edgehog_settings_save(
        OTA_KEY, OTA_REQUEST_ID_KEY, thread_data->ota_request.uuid, ASTARTE_UUID_STR_LEN + 1);
    if (edgehog_result != EDGEHOG_RESULT_OK) {
        EDGEHOG_LOG_ERR("Unable to write OTA req_uuid into Edgehog Settings, OTA canceled");
        return edgehog_result;
    }
    // Step 2 attempt OTA operation for MAX_OTA_RETRY tries

    for (uint8_t update_attempts = 0; update_attempts < MAX_OTA_RETRY; update_attempts++) {
        pub_ota_event(astarte_device, thread_data->ota_request.uuid, OTA_EVENT_DOWNLOADING, 0,
            EDGEHOG_RESULT_OK, "");

        edgehog_result = perform_ota_attempt(edgehog_device);

        if (edgehog_result == EDGEHOG_RESULT_OK || edgehog_result == EDGEHOG_RESULT_OTA_CANCELED) {
            break;
        }

        k_msleep(update_attempts * OTA_ATTEMPS_DELAY_MS);
        pub_ota_event(
            astarte_device, thread_data->ota_request.uuid, OTA_EVENT_ERROR, 0, edgehog_result, "");
        EDGEHOG_LOG_WRN("! OTA FAILED, ATTEMPT #%d !", update_attempts);
    }

    return edgehog_result;
}

static edgehog_result_t perform_ota_attempt(edgehog_device_handle_t edgehog_device)
{
    ota_thread_data_t *thread_data = &edgehog_device->ota_thread.ota_thread_data;

    const char *header_fields[] = { 0 };

    http_download_t http_download;
    http_download.user_data = edgehog_device;
    http_download.download_cbk = http_download_payload_cbk;
    edgehog_result_t edgehog_result = edgehog_http_download(
        thread_data->ota_request.download_url, header_fields, OTA_REQ_TIMEOUT_MS, &http_download);

    if (!atomic_test_bit(&thread_data->ota_run_state, OTA_STATE_RUN_BIT)) {
        EDGEHOG_LOG_DBG("OTA canceled");
        return EDGEHOG_RESULT_OTA_CANCELED;
    }

    if (edgehog_result != EDGEHOG_RESULT_OK) {
        return edgehog_result;
    }

    thread_data->download_size = flash_img_bytes_written(&thread_data->flash_ctx);

    if (thread_data->download_size <= 0 || thread_data->download_size != thread_data->image_size) {
        return EDGEHOG_RESULT_NETWORK_ERROR;
    }

    return EDGEHOG_RESULT_OK;
}

static edgehog_result_t http_download_payload_cbk(
    int sock_id, http_download_chunk_t *download_chunk, void *user_data)
{
    if (!download_chunk) {
        EDGEHOG_LOG_ERR("Unable to read chunk, It is empty");
        return EDGEHOG_RESULT_HTTP_REQUEST_ERROR;
    }

    if (!user_data) {
        EDGEHOG_LOG_ERR("Unable to read user data context");
        return EDGEHOG_RESULT_INTERNAL_ERROR;
    }

    edgehog_device_handle_t edgehog_device = (edgehog_device_handle_t) user_data;
    ota_thread_data_t *ota_thread_data = &edgehog_device->ota_thread.ota_thread_data;
    if (!atomic_test_bit(&ota_thread_data->ota_run_state, OTA_STATE_RUN_BIT)) {
        edgehog_http_download_abort(sock_id);
        return EDGEHOG_RESULT_OK;
    }

    int ret = flash_img_buffered_write(&ota_thread_data->flash_ctx,
        download_chunk->chunk_start_addr, download_chunk->chunk_size, download_chunk->last_chunk);
    if (ret < 0) {
        EDGEHOG_LOG_ERR("Flash write error: %d", ret);
        EDGEHOG_LOG_ERR("Errno: %s\n", strerror(errno));
        edgehog_http_download_abort(sock_id);
        return EDGEHOG_RESULT_OTA_WRITE_FLASH_ERROR;
    }

    ota_thread_data->image_size = download_chunk->download_size;
    ota_thread_data->download_size = flash_img_bytes_written(&ota_thread_data->flash_ctx);
    int read_perc = (int) (OTA_PROGRESS_PERC * ota_thread_data->download_size
        / download_chunk->download_size);
    int read_perc_rounded = read_perc - (read_perc % OTA_PROGRESS_PERC_ROUNDING_STEP);

    if (read_perc_rounded != ota_thread_data->last_perc_sent) {
        pub_ota_event(edgehog_device->astarte_device, ota_thread_data->ota_request.uuid,
            OTA_EVENT_DOWNLOADING, read_perc_rounded, EDGEHOG_RESULT_OK, "");
        EDGEHOG_LOG_DBG("Downloading %d%% chunk %d written %d size %d \n", read_perc_rounded,
            download_chunk->chunk_size, ota_thread_data->download_size,
            download_chunk->download_size);
        ota_thread_data->last_perc_sent = read_perc_rounded;
    }

    return EDGEHOG_RESULT_OK;
}

static edgehog_result_t edgehog_ota_event_cancel(
    edgehog_device_handle_t edgehog_dev, const char *request_uuid)
{
    if (!atomic_test_bit(
            &edgehog_dev->ota_thread.ota_thread_data.ota_run_state, OTA_STATE_RUN_BIT)) {
        pub_ota_event(edgehog_dev->astarte_device, request_uuid, OTA_EVENT_FAILURE, 0,
            EDGEHOG_RESULT_OTA_INVALID_REQUEST,
            "Unable to cancel OTA update request, no OTA update running.");
        return EDGEHOG_RESULT_OTA_INVALID_REQUEST;
    }

    edgehog_result_t res = edgehog_settings_init();
    if (res != EDGEHOG_RESULT_OK) {
        EDGEHOG_LOG_ERR("Edgehog Settings Init failed");
        pub_ota_event(edgehog_dev->astarte_device, request_uuid, OTA_EVENT_FAILURE, 0,
            EDGEHOG_RESULT_OTA_INTERNAL_ERROR,
            "Unable to cancel OTA update request, Edgeghog Settings init error.");
        return EDGEHOG_RESULT_OTA_INTERNAL_ERROR;
    }

    ota_settings_t ota_settings = { 0 };
    res = edgehog_settings_load("ota", ota_settings_loader, &ota_settings);
    if (res != EDGEHOG_RESULT_OK) {
        EDGEHOG_LOG_ERR("Edgehog Settings load failed");
        pub_ota_event(edgehog_dev->astarte_device, request_uuid, OTA_EVENT_FAILURE, 0,
            EDGEHOG_RESULT_OTA_INTERNAL_ERROR,
            "Unable to cancel OTA update request, Edgeghog Settings load error.");
        return EDGEHOG_RESULT_OTA_INTERNAL_ERROR;
    }

    if (strlen(ota_settings.uuid) != ASTARTE_UUID_STR_LEN) { /* item was found, show it */
        EDGEHOG_LOG_ERR("Error fetching the OTA update request UUID from Edgehog Settings");
        pub_ota_event(edgehog_dev->astarte_device, request_uuid, OTA_EVENT_FAILURE, 0,
            EDGEHOG_RESULT_OTA_INTERNAL_ERROR,
            "Unable to cancel OTA update request, Edgehog Settings error.");
        return EDGEHOG_RESULT_OTA_INTERNAL_ERROR;
    }

    if (!atomic_test_and_clear_bit(
            &edgehog_dev->ota_thread.ota_thread_data.ota_run_state, OTA_STATE_RUN_BIT)) {
        EDGEHOG_LOG_ERR("OTA_STATE_RUN_BIT was already cleared");
    }

    return EDGEHOG_RESULT_OK;
}

static const char *swap_type_str(int swap_type)
{
    switch (swap_type) {
        case BOOT_SWAP_TYPE_NONE:
            return "none";
        case BOOT_SWAP_TYPE_TEST:
            return "test";
        case BOOT_SWAP_TYPE_PERM:
            return "perm";
        case BOOT_SWAP_TYPE_REVERT:
            return "revert";
        case BOOT_SWAP_TYPE_FAIL:
            return "fail";
        default:
            return "unknown";
    }
}

static void pub_ota_event(astarte_device_handle_t astarte_device, const char *request_uuid,
    ota_event_t event, int32_t status_progress, edgehog_result_t error, const char *message)
{
    char *status = NULL;
    char *status_code = NULL;
#ifdef EDGEHOG_DEVICE_ZBUS_OTA_EVENT
    edgehog_ota_chan_event_t ota_chan_event = { .event = EDGEHOG_OTA_INVALID_EVENT };
#endif
    switch (event) {
        case OTA_EVENT_ACKNOWLEDGED:
            status = "Acknowledged";
#ifdef EDGEHOG_DEVICE_ZBUS_OTA_EVENT
            ota_chan_event.event = EDGEHOG_OTA_INIT_EVENT;
#endif
            break;
        case OTA_EVENT_DOWNLOADING:
            status = "Downloading";
            break;
        case OTA_EVENT_DEPLOYING:
            status = "Deploying";
            break;
        case OTA_EVENT_DEPLOYED:
            status = "Deployed";
            break;
        case OTA_EVENT_REBOOTING:
            status = "Rebooting";
            break;
        case OTA_EVENT_SUCCESS:
            status = "Success";
#ifdef EDGEHOG_DEVICE_ZBUS_OTA_EVENT
            ota_chan_event.event = EDGEHOG_OTA_SUCCESS_EVENT;
#endif
            break;
        case OTA_EVENT_FAILURE:
            status = "Failure";
#ifdef EDGEHOG_DEVICE_ZBUS_OTA_EVENT
            ota_chan_event.event = EDGEHOG_OTA_FAILED_EVENT;
#endif
            break;
        default: // OTA_EVENT_ERROR
            status = "Error";
#ifdef EDGEHOG_DEVICE_ZBUS_OTA_EVENT
            ota_chan_event.event = EDGEHOG_OTA_FAILED_EVENT;
#endif
            break;
    }

    switch (error) {
        case EDGEHOG_RESULT_OK:
            status_code = "";
            break;
        case EDGEHOG_RESULT_OTA_INVALID_REQUEST:
            status_code = "InvalidRequest";
            break;
        case EDGEHOG_RESULT_OTA_ALREADY_IN_PROGRESS:
            status_code = "UpdateAlreadyInProgress";
            break;
        case EDGEHOG_RESULT_NETWORK_ERROR:
            status_code = "ErrorNetwork";
            break;
        case EDGEHOG_RESULT_SETTINGS_INIT_FAIL:
        case EDGEHOG_RESULT_SETTINGS_SAVE_FAIL:
        case EDGEHOG_RESULT_SETTINGS_LOAD_FAIL:
        case EDGEHOG_RESULT_SETTINGS_DELETE_FAIL:
            status_code = "IOError";
            break;
        case EDGEHOG_RESULT_OTA_INVALID_IMAGE:
            status_code = "InvalidBaseImage";
            break;
        case EDGEHOG_RESULT_OTA_SYSTEM_ROLLBACK:
            status_code = "SystemRollback";
            break;
        case EDGEHOG_RESULT_OTA_CANCELED:
            status_code = "Canceled";
            break;
        default: // EDGEHOG_ERR_OTA_INTERNAL
            status_code = "InternalError";
            break;
    }

#ifdef CONFIG_EDGEHOG_DEVICE_ZBUS_OTA_EVENT
    if (ota_chan_event.event != EDGEHOG_OTA_INVALID_EVENT) {
        zbus_chan_pub(&edgehog_ota_chan, &ota_chan_event, K_SECONDS(1));
    }
#endif

    astarte_object_entry_t object_entries[] = {
        { .path = "requestUUID", .individual = astarte_individual_from_string(request_uuid) },
        { .path = "status", .individual = astarte_individual_from_string(status) },
        { .path = "statusProgress",
            .individual = astarte_individual_from_integer(status_progress) },
        { .path = "statusCode", .individual = astarte_individual_from_string(status_code) },
        { .path = "message", .individual = astarte_individual_from_string(message) },
    };

    const int64_t timestamp = (int64_t) time(NULL);

    astarte_result_t res
        = astarte_device_stream_aggregated(astarte_device, io_edgehog_devicemanager_OTAEvent.name,
            "/event", object_entries, ARRAY_SIZE(object_entries), &timestamp);
    if (res != ASTARTE_RESULT_OK) {
        EDGEHOG_LOG_ERR("Unable to send ota_event"); // NOLINT
    }
}

static int ota_settings_loader(
    const char *key, size_t len, settings_read_cb read_cb, void *cb_arg, void *param)
{
    ARG_UNUSED(len);

    const char *next = NULL;
    ota_settings_t *dest = (ota_settings_t *) param;

    size_t key_len = settings_name_next(key, &next);
    if (!next) {

        if (strncmp(key, OTA_STATE_KEY, key_len) == 0) {
            int res = read_cb(cb_arg, &(dest->ota_state), sizeof(dest->ota_state));
            if (res < 0) {
                EDGEHOG_LOG_ERR("Unable to read ota state from settings: %d", res);
                return res;
            }

            return 0;
        }

        if (strncmp(key, OTA_REQUEST_ID_KEY, key_len) == 0) {
            int res = read_cb(cb_arg, &(dest->uuid), sizeof(dest->uuid));
            if (res < 0) {
                EDGEHOG_LOG_ERR("Unable to read ota request uuid from settings: %d", res);
                return res;
            }

            return 0;
        }
    }

    return -ENOENT;
}
