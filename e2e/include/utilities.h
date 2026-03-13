/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdint.h>

#include <zephyr/fatal.h>
#include <zephyr/shell/shell.h>

#define CHECK_HALT(expr, ...)                                                                      \
    if (expr) {                                                                                    \
        LOG_ERR(__VA_ARGS__);                                                                      \
        k_fatal_halt(-1);                                                                          \
    }

#define CHECK_ASTARTE_OK_HALT(expr, ...) CHECK_HALT(expr != ASTARTE_RESULT_OK, __VA_ARGS__)

#define CHECK_RET_1(expr, ...)                                                                     \
    if (expr) {                                                                                    \
        __VA_OPT__(LOG_ERR(__VA_ARGS__));                                                          \
        return 1;                                                                                  \
    }

#define CHECK_ASTARTE_OK_RET_1(expr, ...) CHECK_RET_1(expr != ASTARTE_RESULT_OK, __VA_ARGS__)

#define CHECK_GOTO(expr, label, ...)                                                               \
    if (expr) {                                                                                    \
        LOG_ERR(__VA_ARGS__);                                                                      \
        goto label;                                                                                \
    }

#define CHECK_ASTARTE_OK_GOTO(expr, label, ...)                                                    \
    CHECK_GOTO(expr != ASTARTE_RESULT_OK, label, __VA_ARGS__)

#endif /* UTILITIES_H */
