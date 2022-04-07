// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#include <vivid/util/log.h>

#if VIVID_LOG
#include <stdarg.h>
#include <stdio.h>

struct vivid_log {
    vivid_binding_t *binding;
    char *buffer;
    size_t buffer_size;
};

vivid_log_t *vivid_log_create(vivid_binding_t *binding, size_t buffer_size)
{
    vivid_log_t *me = (vivid_log_t *)binding->calloc(binding, 1U, sizeof(*me));
    if (me == NULL) {
        return NULL;
    }
    me->binding = binding;
    me->buffer_size = buffer_size;
    me->buffer = binding->calloc(binding, 1U, buffer_size);
    if (me->buffer == NULL) {
        vivid_log_destroy(me);
        return NULL;
    }
    return me;
}

void vivid_log_destroy(vivid_log_t *me)
{
    if (me == NULL) {
        return;
    }
    me->binding->free(me->buffer);
    me->binding->free(me);
}

void vivid_log_formatted(vivid_log_t *me, vivid_log_level_t level, const char *format, ...)
{
    if ((me == NULL) || (me->binding->log == NULL)) {
        return;
    }
    va_list args;
    va_start(args, format);
    int res = vsnprintf(me->buffer, me->buffer_size, format, args);
    if (res < 0) {
        vivid_log_error(me->binding, "log format error");
        return;
    }
    if ((unsigned)res >= me->buffer_size) {
        vivid_log_error(me->binding, "log buffer too small");
        return;
    }
    me->binding->log(me->binding->logger, level, me->buffer);
}

void vivid_log_error(vivid_binding_t *binding, const char *message)
{
    if ((binding == NULL) || (binding->log == NULL)) {
        return;
    }
    binding->log(binding->logger, VIVID_LOG_LEVEL_ERROR, message);
}
#endif
