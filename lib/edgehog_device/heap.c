/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "heap.h"

#if defined(CONFIG_EDGEHOG_DEVICE_SDK_ADVANCED_ENABLE_HEAP)
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/math_extras.h>
#else
#include <stdlib.h>
#endif

#if defined(CONFIG_EDGEHOG_DEVICE_SDK_ADVANCED_ENABLE_HEAP)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
K_HEAP_DEFINE(k_heap_edgehog, CONFIG_EDGEHOG_DEVICE_SDK_ADVANCED_HEAP_SIZE);
#endif

/************************************************
 *         Global functions definitions         *
 ***********************************************/

void *edgehog_malloc(size_t size)
{
#if defined(CONFIG_EDGEHOG_DEVICE_SDK_ADVANCED_ENABLE_HEAP)
    return k_heap_alloc(&k_heap_edgehog, size, K_NO_WAIT);
#else
    return malloc(size);
#endif
}

void *edgehog_calloc(size_t num, size_t size)
{
#if defined(CONFIG_EDGEHOG_DEVICE_SDK_ADVANCED_ENABLE_HEAP)
    void *ret = NULL;
    size_t bounds = 0U;
    if (size_mul_overflow(num, size, &bounds)) {
        return NULL;
    }
    ret = k_heap_alloc(&k_heap_edgehog, bounds, K_NO_WAIT);
    if (ret != NULL) {
        (void) memset(ret, 0, bounds);
    }
    return ret;
#else
    return calloc(num, size);
#endif
}

void *edgehog_realloc(void *ptr, size_t new_size)
{
#if defined(CONFIG_EDGEHOG_DEVICE_SDK_ADVANCED_ENABLE_HEAP)
    return k_heap_realloc(&k_heap_edgehog, ptr, new_size, K_NO_WAIT);
#else
    return realloc(ptr, new_size);
#endif
}

void edgehog_free(void *ptr)
{
#if defined(CONFIG_EDGEHOG_DEVICE_SDK_ADVANCED_ENABLE_HEAP)
    k_heap_free(&k_heap_edgehog, ptr);
#else
    free(ptr);
#endif
}
