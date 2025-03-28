/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage_usage.h"

#include "edgehog_private.h"
#include "nvs.h"
#include "system_time.h"

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/result.h>

#include "generated_interfaces.h"
#include "log.h"
EDGEHOG_LOG_MODULE_REGISTER(storage_usage, CONFIG_EDGEHOG_DEVICE_STORAGE_USAGE_LOG_LEVEL);

void publish_storage_usage(edgehog_device_handle_t edgehog_device)
{

    size_t free_space = 0;
    edgehog_result_t res = edgehog_nvs_get_free_space(&free_space);

    if (res == EDGEHOG_RESULT_OK) {
        astarte_object_entry_t object_entries[]
            = { { .path = "totalBytes", .data = astarte_data_from_longinteger(NVS_PARTITION_SIZE) },
                  { .path = "freeBytes", .data = astarte_data_from_longinteger(free_space) } };

        int64_t timestamp_ms = 0;
        system_time_current_ms(&timestamp_ms);

        astarte_result_t res = astarte_device_send_object(edgehog_device->astarte_device,
            io_edgehog_devicemanager_StorageUsage.name, "/" NVS_PARTITION_LABEL, object_entries,
            ARRAY_SIZE(object_entries), &timestamp_ms);
        if (res != ASTARTE_RESULT_OK) {
            EDGEHOG_LOG_ERR("Unable to send syste_status"); // NOLINT
        }
    }
}
