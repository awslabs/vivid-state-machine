// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#ifndef VIVID_BINDING_FREERTOS_H
#define VIVID_BINDING_FREERTOS_H

#include <FreeRTOS.h>
#include <task.h>
#include <vivid/binding.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
// Options

#ifndef VIVID_BINDING_FREERTOS_YIELD_AFTER_NOTIFY
#define VIVID_BINDING_FREERTOS_YIELD_AFTER_NOTIFY 1
#endif
//--------------------------------------------------------------------------------------------------

vivid_binding_t *vivid_binding_freertos_create(TaskHandle_t task_handle, uint32_t notify_mask VIVID_LOG_ARGS(, vivid_binding_log_callback_t log_callback, void *logger));

void vivid_binding_freertos_handle_event(vivid_binding_t *binding);

void vivid_binding_freertos_destroy(vivid_binding_t *binding);

BaseType_t vivid_binding_freertos_is_in_isr(void);

#if !VIVID_BINDING_FREERTOS_YIELD_AFTER_NOTIFY
BaseType_t vivid_binding_freertos_is_yield_required(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
