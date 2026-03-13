/*
 * (C) Copyright 2025, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "device_handler.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "utilities.h"

LOG_MODULE_REGISTER(device_handler, CONFIG_DEVICE_HANDLER_LOG_LEVEL);

/************************************************
 *   Constants, static variables and defines    *
 ***********************************************/

#define GENERIC_WAIT_SLEEP_500_MS 500

K_THREAD_STACK_DEFINE(device_thread_stack_area, CONFIG_DEVICE_THREAD_STACK_SIZE);
static struct k_thread device_thread_data;
static atomic_t device_thread_flags;
enum device_thread_flags
{
    DEVICE_THREAD_CONNECTED_FLAG = 0,
    DEVICE_THREAD_TERMINATION_FLAG,
};

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static void device_thread_entry_point(void *unused1, void *unused2, void *unused3);

/************************************************
 *         Global functions definition          *
 ***********************************************/

void setup_device()
{
    LOG_INF("Initializing an Edgehog device.");

    LOG_INF("Spawning a new thread to poll data from the Edgehog device.");
    k_thread_create(&device_thread_data, device_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_thread_stack_area), device_thread_entry_point, NULL, NULL,
        NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);

    LOG_INF("Edgehog device created.");
}

void free_device()
{
    atomic_set_bit(&device_thread_flags, DEVICE_THREAD_TERMINATION_FLAG);

    CHECK_HALT(k_thread_join(&device_thread_data, K_FOREVER) != 0,
        "Failed in waiting for the Edgehog thread to terminate.");

    LOG_INF("Destroing Edgehog device and freeing resources.");
    LOG_INF("Edgehog device destroyed.");
}

void wait_for_device_connection()
{
    while (!atomic_test_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG)) {
        k_sleep(K_MSEC(GENERIC_WAIT_SLEEP_500_MS));
    }
}

void disconnect_device()
{
    atomic_set_bit(&device_thread_flags, DEVICE_THREAD_TERMINATION_FLAG);
}

void wait_for_device_disconnection()
{
    while (atomic_test_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG)) {
        k_sleep(K_MSEC(GENERIC_WAIT_SLEEP_500_MS));
    }
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void device_thread_entry_point(void *unused1, void *unused2, void *unused3)
{
    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);
    ARG_UNUSED(unused3);

    LOG_INF("Started Edgehog device thread.");

    while (!atomic_test_bit(&device_thread_flags, DEVICE_THREAD_TERMINATION_FLAG)) {
        k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(CONFIG_DEVICE_POLL_PERIOD_MS));

        // Managing

        k_sleep(sys_timepoint_timeout(timepoint));
    }

    LOG_INF("Exiting from the polling thread.");
}
