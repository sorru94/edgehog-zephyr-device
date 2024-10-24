/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HEAP_H
#define HEAP_H

/**
 * @file heap.h
 * @brief Heap allocator functions.
 *
 * @details Depending on configuration options for the device this module will use a dedicated
 * heap or the system heap.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocate @p size bytes of memory from the heap.
 *
 * @details This function will use the system heap of the Edgehog dedicated heap depending on
 * the configuration.
 *
 * @param[in] size The number of bytes to allocate.
 */
void *edgehog_malloc(size_t size);

/**
 * @brief Allocates memory for an array of @p num objects of @p size and initializes all bytes in
 * the allocated storage to zero.
 *
 * @details This function will use the system heap of the Edgehog dedicated heap depending on
 * the configuration.
 *
 * @param[in] num Number of objects to allocate.
 * @param[in] size Size of each object to allocate.
 */
void *edgehog_calloc(size_t num, size_t size);

/**
 * @brief Allocates memory for an array of @p num objects of @p size and initializes all bytes in
 * the allocated storage to zero.
 *
 * @details This function will use the system heap of the Edgehog dedicated heap depending on
 * the configuration.
 *
 * @param[in] num Number of objects to allocate.
 * @param[in] size Size of each object to allocate.
 */
void *edgehog_realloc(void *ptr, size_t new_size);

/**
 * @brief Free previously allocated memory region.
 *
 * @details This function will use the system heap of the Edgehog dedicated heap depending on
 * the configuration.
 *
 * @param[in] ptr The memory to free.
 */
void edgehog_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif // HEAP_H
