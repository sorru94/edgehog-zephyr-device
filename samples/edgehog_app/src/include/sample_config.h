/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SAMPLE_CONFIG_H
#define SAMPLE_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#include <astarte_device_sdk/device.h>

#if defined(CONFIG_WIFI)
#define SAMPLE_CONFIG_WIFI_MAX_STRINGS 255
#endif

struct sample_config
{
    char device_id[ASTARTE_DEVICE_ID_LEN + 1];
    char credential_secret[ASTARTE_PAIRING_CRED_SECR_LEN + 1];
#if defined(CONFIG_WIFI)
    char wifi_ssid[SAMPLE_CONFIG_WIFI_MAX_STRINGS];
    char wifi_pwd[SAMPLE_CONFIG_WIFI_MAX_STRINGS];
#endif
};

int sample_config_get(struct sample_config *cfg);

#endif /* SAMPLE_CONFIG_H */
