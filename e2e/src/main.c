/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys_clock.h>

#if defined(CONFIG_ARCH_POSIX)
#include <nsi_main.h>
#endif

#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP)                                   \
    || !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT)
#define NEEDS_TLS
#endif

#if defined(NEEDS_TLS)

// Enable mbed tls debug
#define MBEDTLS_DEBUG_C

#if defined(CONFIG_TLS_CERTIFICATE_PATH)
#include "ca_certificate_inc.h"
#else
#error TLS enabled but no generated certificate found: check the CONFIG_TLS_CERTIFICATE_PATH option
#endif

#include <mbedtls/debug.h>
#include <zephyr/net/tls_credentials.h>

#endif

#include "eth.h"

#include "runner.h"
#include "utilities.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

/************************************************
 *   Constants, static variables and defines    *
 ***********************************************/

#define ETH_THREAD_STACK_SIZE 4096
K_THREAD_STACK_DEFINE(eth_thread_stack_area, ETH_THREAD_STACK_SIZE);
static struct k_thread eth_thread_data;
enum eth_thread_flags
{
    ETH_THREAD_TERMINATION_FLAG = 0,
};
static atomic_t eth_thread_flags;

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static void eth_thread_entry_point(void *unused1, void *unused2, void *unused3);

/************************************************
 *         Global functions definition          *
 ***********************************************/

int main(void)
{
    LOG_INF("Edgehog device end to end test");

    // Initialize Ethernet driver
    LOG_INF("Initializing Ethernet driver.");
    if (eth_connect() != 0) {
        LOG_ERR("Connectivity intialization failed!");
        return -1;
    }

#if defined(NEEDS_TLS)

    // Add TLS certificate
    tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_HTTPS_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
        ca_certificate_root, ARRAY_SIZE(ca_certificate_root));

    // Enable mbedtls logging
    mbedtls_debug_set_threshold(1);

#endif

    LOG_INF("Spawning a new thread to poll the eth interface and check connectivity.");
    k_thread_create(&eth_thread_data, eth_thread_stack_area,
        K_THREAD_STACK_SIZEOF(eth_thread_stack_area), eth_thread_entry_point, NULL, NULL, NULL,
        CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);

    LOG_INF("Running the end to end test.");
    run_end_to_end_test();

    atomic_set_bit(&eth_thread_flags, ETH_THREAD_TERMINATION_FLAG);
    CHECK_HALT(k_thread_join(&eth_thread_data, K_FOREVER) != 0,
        "Failed in waiting for the eth polling thread to terminate.");

    LOG_INF("Returning from the end to end test.");

#if defined(CONFIG_ARCH_POSIX)
    // Required to terminate when on posix
    nsi_exit(0);
#endif
    return 0;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void eth_thread_entry_point(void *unused1, void *unused2, void *unused3)
{
    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);
    ARG_UNUSED(unused3);

    LOG_INF("Started the eth polling thread");

    while (!atomic_test_bit(&eth_thread_flags, ETH_THREAD_TERMINATION_FLAG)) {
        k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(CONFIG_ETH_POLL_PERIOD_MS));

        if (eth_poll() != 0) {
            LOG_ERR("Failed polling Ethernet."); // NOLINT
        }

        k_sleep(sys_timepoint_timeout(timepoint));
    }
}
