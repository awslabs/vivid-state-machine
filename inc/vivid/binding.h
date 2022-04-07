// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#ifndef VIVID_BINDING_H
#define VIVID_BINDING_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
// Options

#ifndef VIVID_TIME_TYPE
#define VIVID_TIME_TYPE double
#endif

#ifndef VIVID_CONVERT_TIME
#define VIVID_CONVERT_TIME(x) x
#endif

#ifndef VIVID_LOCKFREE
#define VIVID_LOCKFREE 0
#endif

#ifndef VIVID_LOG
#define VIVID_LOG 1
#endif
//--------------------------------------------------------------------------------------------------

#if VIVID_LOG
#define VIVID_LOG_ARGS(...) __VA_ARGS__
#else
#define VIVID_LOG_ARGS(...)
#endif

#define VIVID_SLEEP(binding, time) binding->sleep(binding, VIVID_CONVERT_TIME(time))

typedef struct vivid_binding vivid_binding_t;
typedef struct vivid_binding_data vivid_binding_data_t;
typedef struct vivid_binding_event vivid_binding_event_t;
typedef struct vivid_binding_timer vivid_binding_timer_t;
typedef struct vivid_binding_mutex vivid_binding_mutex_t;
typedef VIVID_TIME_TYPE vivid_time_t;

typedef enum {
    VIVID_LOG_LEVEL_NONE,
    VIVID_LOG_LEVEL_ERROR,
    VIVID_LOG_LEVEL_WARN,
    VIVID_LOG_LEVEL_INFO,
    VIVID_LOG_LEVEL_DEBUG
} vivid_log_level_t;

typedef enum {
    VIVID_ERROR_NONE,
    VIVID_ERROR_CALLOC,
    VIVID_ERROR_EVENT,
    VIVID_ERROR_QUEUE_EVENT,
    VIVID_ERROR_TRIGGER_EVENT,
    VIVID_ERROR_TIMER,
    VIVID_ERROR_START_TIMER,
    VIVID_ERROR_STOP_TIMER,
    VIVID_ERROR_GET_TIME,
    VIVID_ERROR_SLEEP,
    VIVID_ERROR_LOCK_MUTEX,
    VIVID_ERROR_UNLOCK_MUTEX
} vivid_error_t;

typedef void (*vivid_binding_callback_t)(void *data);
typedef void (*vivid_binding_log_callback_t)(void *logger, vivid_log_level_t level, const char *message);

// clang-format off
struct vivid_binding {
    vivid_binding_data_t *data;

    void                  *(*calloc       )(vivid_binding_t *me, size_t num, size_t size);
    void                   (*free         )(void *mem);
    vivid_binding_event_t *(*create_event )(vivid_binding_t *me, vivid_binding_callback_t callback, void *data);
    void                   (*trigger_event)(vivid_binding_event_t *event);
    void                   (*destroy_event)(vivid_binding_event_t *event);
    vivid_binding_timer_t *(*create_timer )(vivid_binding_t *me, vivid_binding_callback_t callback, void *data);
    void                   (*start_timer  )(vivid_binding_timer_t *timer, vivid_time_t timeout);
    void                   (*stop_timer   )(vivid_binding_timer_t *timer);
    void                   (*destroy_timer)(vivid_binding_timer_t *timer);
    vivid_time_t           (*get_time     )(vivid_binding_t *me);
    void                   (*sleep        )(vivid_binding_t *me, vivid_time_t time);

#if !VIVID_LOCKFREE
    vivid_binding_mutex_t *(*create_mutex )(vivid_binding_t *me);
    bool                   (*lock_mutex   )(vivid_binding_mutex_t *mutex);
    void                   (*unlock_mutex )(vivid_binding_mutex_t *mutex);
    void                   (*destroy_mutex)(vivid_binding_mutex_t *mutex);
#endif

#if VIVID_LOG
    void                   (*log          )(void *logger, vivid_log_level_t level, const char *message);
    void *logger;
#endif

    void                   (*error_hook   )(void *app, vivid_error_t error);
    void *app;
};
// clang-format on

#ifdef __cplusplus
}
#endif

#endif
