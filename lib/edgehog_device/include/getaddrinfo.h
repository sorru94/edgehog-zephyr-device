/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GETADDRINFO_H
#define GETADDRINFO_H

#include <zephyr/net/socket.h>

/**
 * @file getaddrinfo.h
 * @brief Modified implementation of getaddrinfo functions to use the Astarte heap for dynamic
 * allocation.
 *
 * @details Except for the allocation this is a 1:1 copy of the `getaddrinfo` implementation
 * from the zephyr networking.
 */

#ifdef __cplusplus
extern "C" {
#endif

// NOLINTBEGIN: This function signature should be kept as similar as possible to the zephyr one.
/**
 * @brief Get address info implementation.
 *
 * @details This is almost an 1:1 copy of the implementation present in the zephyr networking.
 * The only difference is that this module uses the Edgehog heap for dynamic allocation.
 * For more info see: https://docs.zephyrproject.org/apidoc/latest/group__bsd__sockets.html
 *
 */
int edgehog_getaddrinfo(const char *host, const char *service, const struct zsock_addrinfo *hints,
    struct zsock_addrinfo **res);
// NOLINTEND

// NOLINTBEGIN: This function signature should be kept as similar as possible to the zephyr one.
/**
 * @brief Free address info.
 *
 * @details This is almost an 1:1 copy of the implementation present in the zephyr networking.
 * The only difference is that this module uses the Edgehog heap for dynamic allocation.
 * For more info see: https://docs.zephyrproject.org/apidoc/latest/group__bsd__sockets.html
 *
 */
void edgehog_freeaddrinfo(struct zsock_addrinfo *ai);
// NOLINTEND

#ifdef __cplusplus
}
#endif

#endif // GETADDRINFO_H
