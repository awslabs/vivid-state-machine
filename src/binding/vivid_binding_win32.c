// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#include <stdbool.h>
#include <stdlib.h>
#include <vivid/binding/win32.h>
#include <vivid/util/log.h>
#include <windows.h>

struct vivid_binding_data {
    HANDLE event_handle;
    vivid_binding_event_t *events;
};

struct vivid_binding_event {
    vivid_binding_t *binding;
    vivid_binding_callback_t callback;
    void *data;
    volatile bool trig;
    vivid_binding_event_t *next;
};

struct vivid_binding_timer {
    vivid_binding_t *binding;
    vivid_binding_callback_t callback;
    void *data;
    HANDLE timer_handle;
    HANDLE wait_handle;
};

#if !VIVID_LOCKFREE
struct vivid_binding_mutex {
    vivid_binding_t *binding;
    CRITICAL_SECTION critical_section;
};
#endif

static void *calloc_mem(vivid_binding_t *me, size_t num, size_t size)
{
    void *mem = calloc(num, size);
    if (mem == NULL) {
        vivid_log_error(me, "could not allocate memory");
        if (me->error_hook != NULL) {
            me->error_hook(me->app, VIVID_ERROR_CALLOC);
        }
        return NULL;
    }
    return mem;
}

static void free_mem(void *mem)
{
    free(mem);
}

static void destroy_event(vivid_binding_event_t *event)
{
    if (event == NULL) {
        return;
    }
    vivid_binding_t *me = event->binding;
    me->free(event);
}

static vivid_binding_event_t *create_event(vivid_binding_t *me, vivid_binding_callback_t callback, void *data)
{
    vivid_binding_event_t *event = (vivid_binding_event_t *)me->calloc(me, 1U, sizeof(*event));
    if (event == NULL) {
        return NULL;
    }
    event->binding = me;
    event->callback = callback;
    event->data = data;
    event->next = me->data->events;
    me->data->events = event;
    return event;
}

static void trigger_event(vivid_binding_event_t *event)
{
    vivid_binding_t *me = event->binding;
    event->trig = true;
    MemoryBarrier();
    if (!SetEvent(me->data->event_handle)) {
        vivid_log_error(me, "could not set event");
        if (me->error_hook != NULL) {
            me->error_hook(me->app, VIVID_ERROR_TRIGGER_EVENT);
        }
    }
}

static void destroy_timer(vivid_binding_timer_t *timer)
{
    if (timer == NULL) {
        return;
    }
    vivid_binding_t *me = timer->binding;
    if (timer->wait_handle != NULL) {
        if (!UnregisterWait(timer->wait_handle) && (GetLastError() != ERROR_IO_PENDING)) {
            vivid_log_error(me, "could unregister wait on timer");
        }
    }
    if (timer->timer_handle != NULL) {
        (void)CloseHandle(timer->timer_handle);
    }
    me->free(timer);
}

VOID timer_callback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
    (void)TimerOrWaitFired;
    vivid_binding_timer_t *timer = (vivid_binding_timer_t *)lpParameter;
    timer->callback(timer->data);
}

static vivid_binding_timer_t *create_timer(vivid_binding_t *me, vivid_binding_callback_t callback, void *data)
{
    vivid_binding_timer_t *timer = (vivid_binding_timer_t *)me->calloc(me, 1U, sizeof(*timer));
    if (timer == NULL) {
        return NULL;
    }
    timer->binding = me;
    timer->callback = callback;
    timer->data = data;
    timer->timer_handle = CreateWaitableTimer(NULL, FALSE, NULL);
    if (timer->timer_handle == NULL) {
        vivid_log_error(me, "could not create timer");
        goto error;
    }
    if (!RegisterWaitForSingleObject(&timer->wait_handle, timer->timer_handle, timer_callback, timer, INFINITE, 0U)) {
        vivid_log_error(me, "could not wait on timer");
        goto error;
    }
    return timer;
error:
    destroy_timer(timer);
    return NULL;
}

static void start_timer(vivid_binding_timer_t *timer, vivid_time_t timeout)
{
    vivid_binding_t *me = timer->binding;
    LARGE_INTEGER due_time = { 0 };
    due_time.QuadPart = (LONGLONG)(timeout * -10000000.0); // Negative means relative
    if (!SetWaitableTimer(timer->timer_handle, &due_time, (LONG)(timeout / 0.001), NULL, NULL, 0)) {
        vivid_log_error(me, "could not start timer");
        if (me->error_hook != NULL) {
            me->error_hook(me->app, VIVID_ERROR_START_TIMER);
        }
    }
}

static void stop_timer(vivid_binding_timer_t *timer)
{
    vivid_binding_t *me = timer->binding;
    if (!CancelWaitableTimer(timer->timer_handle)) {
        vivid_log_error(me, "could not stop timer");
        if (me->error_hook != NULL) {
            me->error_hook(me->app, VIVID_ERROR_STOP_TIMER);
        }
    }
}

static vivid_time_t get_time(vivid_binding_t *me)
{
    (void)me;
    SYSTEMTIME st;
    GetSystemTime(&st);
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    return (double)(((LONGLONG)ft.dwHighDateTime << 32) + ft.dwLowDateTime) / 10000000.0;
}

static void sleep_time(vivid_binding_t *me, vivid_time_t time)
{
    (void)me;
    Sleep((DWORD)(time * 1000.0));
}

#if !VIVID_LOCKFREE
static void destroy_mutex(vivid_binding_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }
    vivid_binding_t *me = mutex->binding;
    DeleteCriticalSection(&mutex->critical_section);
    me->free(mutex);
}

static vivid_binding_mutex_t *create_mutex(vivid_binding_t *me)
{
    vivid_binding_mutex_t *mutex = (vivid_binding_mutex_t *)me->calloc(me, 1U, sizeof(*mutex));
    if (mutex == NULL) {
        return NULL;
    }
    mutex->binding = me;
    InitializeCriticalSection(&mutex->critical_section);
    return mutex;
}

static bool lock_mutex(vivid_binding_mutex_t *mutex)
{
    EnterCriticalSection(&mutex->critical_section);
    return true;
}

static void unlock_mutex(vivid_binding_mutex_t *mutex)
{
    LeaveCriticalSection(&mutex->critical_section);
}
#endif

vivid_binding_t *vivid_binding_win32_create(HANDLE *handle VIVID_LOG_ARGS(, vivid_binding_log_callback_t log_callback, void *logger))
{
    vivid_binding_t *me = (vivid_binding_t *)calloc(1U, sizeof(*me));
    if (me == NULL) {
#if VIVID_LOG
        log_callback(logger, VIVID_LOG_LEVEL_ERROR, "could not allocate memory");
#endif
        return NULL;
    }
    me->calloc = calloc_mem;
    me->free = free_mem;
    me->create_event = create_event;
    me->trigger_event = trigger_event;
    me->destroy_event = destroy_event;
    me->create_timer = create_timer;
    me->start_timer = start_timer;
    me->stop_timer = stop_timer;
    me->destroy_timer = destroy_timer;
    me->get_time = get_time;
    me->sleep = sleep_time;
#if !VIVID_LOCKFREE
    me->create_mutex = create_mutex;
    me->lock_mutex = lock_mutex;
    me->unlock_mutex = unlock_mutex;
    me->destroy_mutex = destroy_mutex;
#endif
#if VIVID_LOG
    me->log = log_callback;
    me->logger = logger;
#endif
    me->data = (vivid_binding_data_t *)me->calloc(me, 1U, sizeof(*me->data));
    if (me->data == NULL) {
        goto error;
    }
    me->data->event_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (me->data->event_handle == NULL) {
        vivid_log_error(me, "could not create event");
        goto error;
    }
    *handle = me->data->event_handle;
    return me;
error:
    vivid_binding_win32_destroy(me);
    return NULL;
}

void vivid_binding_win32_destroy(vivid_binding_t *me)
{
    if (me == NULL) {
        return;
    }
    if (me->data != NULL) {
        if (me->data->event_handle != NULL) {
            (void)CloseHandle(me->data->event_handle);
        }
        me->free(me->data);
    }
    me->free(me);
}

void vivid_binding_win32_handle_event(vivid_binding_t *me)
{
    vivid_binding_event_t *event = me->data->events;
    while (event != NULL) {
        if (event->trig) {
            event->trig = false;
            MemoryBarrier();
            event->callback(event->data);
        }
        event = event->next;
    }
}
