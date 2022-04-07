// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#ifndef VIVID_LOG_H
#define VIVID_LOG_H

#include <vivid/binding.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !VIVID_LOG
#define VIVID_LOG_ERROR(me, format, ...)
#define VIVID_LOG_WARN(me, format, ...)
#define VIVID_LOG_INFO(me, format, ...)
#define VIVID_LOG_DEBUG(me, format, ...)
#define vivid_log_destroy(me)
#define vivid_log_error(binding, message)
#else
#define VIVID_LOG_ERROR(me, format, ...) vivid_log_formatted(me, VIVID_LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define VIVID_LOG_WARN(me, format, ...) vivid_log_formatted(me, VIVID_LOG_LEVEL_WARN, format, ##__VA_ARGS__)
#define VIVID_LOG_INFO(me, format, ...) vivid_log_formatted(me, VIVID_LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define VIVID_LOG_DEBUG(me, format, ...) vivid_log_formatted(me, VIVID_LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)

typedef struct vivid_log vivid_log_t;

vivid_log_t *vivid_log_create(vivid_binding_t *binding, size_t buffer_size);

void vivid_log_destroy(vivid_log_t *me);

void vivid_log_formatted(vivid_log_t *me, vivid_log_level_t level, const char *format, ...);

void vivid_log_error(vivid_binding_t *binding, const char *message);
#endif

#ifdef __cplusplus
}
#endif

#endif
