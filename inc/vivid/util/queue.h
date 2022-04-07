// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#ifndef VIVID_QUEUE_H
#define VIVID_QUEUE_H

#include <vivid/binding.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
// Options

#ifndef VIVID_PARAM
#define VIVID_PARAM 1
#endif

#if !VIVID_PARAM
#define VIVID_PARAM_DYNAMIC 0
#elif !defined(VIVID_PARAM_DYNAMIC)
#define VIVID_PARAM_DYNAMIC 0
#endif

#define VIVID_PARAM_STATIC (VIVID_PARAM && !VIVID_PARAM_DYNAMIC)
//--------------------------------------------------------------------------------------------------

#if VIVID_PARAM
#define VIVID_PARAM_ARGS(...) __VA_ARGS__
#else
#define VIVID_PARAM_ARGS(...)
#endif

#if VIVID_PARAM_STATIC
#define VIVID_PARAM_STATIC_ARGS(...) __VA_ARGS__
#else
#define VIVID_PARAM_STATIC_ARGS(...)
#endif

#if VIVID_PARAM_DYNAMIC
#define VIVID_PARAM_DYNAMIC_ARGS(...) __VA_ARGS__
#else
#define VIVID_PARAM_DYNAMIC_ARGS(...)
#endif

typedef struct vivid_queue vivid_queue_t;
#if VIVID_PARAM_DYNAMIC
typedef void (*vivid_param_destructor_t)(void *param);
#endif

typedef struct {
    const char *name;
#if VIVID_PARAM
    void *param;
#if VIVID_PARAM_DYNAMIC
    vivid_param_destructor_t param_destructor;
#else
    size_t param_size;
#endif
#endif
} vivid_queue_entry_t;

vivid_queue_t *vivid_queue_create(vivid_binding_t *binding, size_t size VIVID_PARAM_STATIC_ARGS(, size_t max_param_size));

void vivid_queue_destroy(vivid_queue_t *me);

bool vivid_queue_push(vivid_queue_t *me, const char *name VIVID_PARAM_ARGS(, VIVID_PARAM_STATIC_ARGS(const) void *param, VIVID_PARAM_STATIC_ARGS(size_t param_size) VIVID_PARAM_DYNAMIC_ARGS(vivid_param_destructor_t param_destructor)));

bool vivid_queue_empty(vivid_queue_t *me);

const vivid_queue_entry_t *vivid_queue_front(const vivid_queue_t *me);

void vivid_queue_pop(vivid_queue_t *me);

#ifdef __cplusplus
}
#endif

#endif
