// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#include <stdbool.h>
#include <string.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <vivid/binding/freertos.h>
#include <vivid/util/log.h>

#if VIVID_LOCKFREE
#include <stdatomic.h>
#define TRIG_TYPE_QUALIFIER _Atomic
#else
#define TRIG_TYPE_QUALIFIER
#endif

struct vivid_binding_data {
    TaskHandle_t task_handle;
    uint32_t notify_mask;
    vivid_binding_event_t *events;
#if !VIVID_LOCKFREE
    vivid_binding_mutex_t *mutex;
#endif
};

struct vivid_binding_event {
    vivid_binding_t *binding;
    vivid_binding_callback_t callback;
    void *data;
    TRIG_TYPE_QUALIFIER bool trig;
    vivid_binding_event_t *next;
};

struct vivid_binding_timer {
    vivid_binding_t *binding;
    vivid_binding_callback_t callback;
    void *data;
    TimerHandle_t handle;
};

#if !VIVID_LOCKFREE
struct vivid_binding_mutex {
    vivid_binding_t *binding;
    BaseType_t interrupt_status;
};
#endif

#if !VIVID_BINDING_FREERTOS_YIELD_AFTER_NOTIFY
static BaseType_t m_yield_required;
#endif

static void *calloc_mem(vivid_binding_t *me, size_t num, size_t size)
{
    (void)me;
    size_t total_size = num * size;
    if (total_size == 0U) {
        return (void *)-1;
    }
    void *mem = pvPortMalloc(total_size);
    if (mem == NULL) {
        vivid_log_error(me, "could not allocate memory");
        if (me->error_hook != NULL) {
            me->error_hook(me->app, VIVID_ERROR_CALLOC);
        }
        return NULL;
    }
    memset(mem, 0, total_size);
    return mem;
}

static void free_mem(void *mem)
{
    if (mem == (void *)-1) {
        return;
    }
    vPortFree(mem);
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
#if VIVID_LOCKFREE
    atomic_store(&event->trig, true);
#else
    (void)me->lock_mutex(me->data->mutex);
    event->trig = true;
    me->unlock_mutex(me->data->mutex);
#endif

    if (vivid_binding_freertos_is_in_isr() != pdFALSE) {
#if VIVID_BINDING_FREERTOS_YIELD_AFTER_NOTIFY
        BaseType_t yield_required = pdFALSE;
        (void)xTaskNotifyFromISR(me->data->task_handle, me->data->notify_mask, eSetBits, &yield_required);
        portYIELD_FROM_ISR(yield_required);
#else
        (void)xTaskNotifyFromISR(me->data->task_handle, me->data->notify_mask, eSetBits, &m_yield_required);
#endif
    } else {
        (void)xTaskNotify(me->data->task_handle, me->data->notify_mask, eSetBits);
    }
}

static void destroy_timer(vivid_binding_timer_t *timer)
{
    if (timer == NULL) {
        return;
    }
    vivid_binding_t *me = timer->binding;
    if (timer->handle != NULL) {
        if (xTimerDelete(timer->handle, 0) == pdFAIL) {
            vivid_log_error(me, "could not delete timer");
        }
    }
    me->free(timer);
}

static void timer_callback(TimerHandle_t handle)
{
    vivid_binding_timer_t *timer = (vivid_binding_timer_t *)pvTimerGetTimerID(handle);
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
    timer->handle = xTimerCreate(NULL, portMAX_DELAY, pdFALSE, timer, timer_callback);
    if (timer->handle == NULL) {
        vivid_log_error(me, "could not create timer");
        destroy_timer(timer);
        return NULL;
    }
    return timer;
}

static void start_timer(vivid_binding_timer_t *timer, vivid_time_t timeout)
{
    vivid_binding_t *me = timer->binding;
    if (xTimerChangePeriod(timer->handle, timeout, 0) == pdFAIL) {
        vivid_log_error(me, "could not start timer");
        if (me->error_hook != NULL) {
            me->error_hook(me->app, VIVID_ERROR_START_TIMER);
        }
    }
}

static void stop_timer(vivid_binding_timer_t *timer)
{
    vivid_binding_t *me = timer->binding;
    if (xTimerStop(timer->handle, 0) == pdFAIL) {
        vivid_log_error(me, "could not stop timer");
        if (me->error_hook != NULL) {
            me->error_hook(me->app, VIVID_ERROR_STOP_TIMER);
        }
    }
}

static vivid_time_t get_time(vivid_binding_t *me)
{
    (void)me;
    return xTaskGetTickCount();
}

static void sleep_time(vivid_binding_t *me, vivid_time_t time)
{
    (void)me;
    vTaskDelay(time);
}

#if !VIVID_LOCKFREE
static void destroy_mutex(vivid_binding_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }
    vivid_binding_t *me = mutex->binding;
    mutex->binding->free(mutex);
}

static vivid_binding_mutex_t *create_mutex(vivid_binding_t *me)
{
    vivid_binding_mutex_t *mutex = (vivid_binding_mutex_t *)me->calloc(me, 1U, sizeof(*mutex));
    if (mutex == NULL) {
        return NULL;
    }
    mutex->binding = me;
    return mutex;
}

static bool lock_mutex(vivid_binding_mutex_t *mutex)
{
    mutex->interrupt_status = taskENTER_CRITICAL_FROM_ISR();
    return true;
}

static void unlock_mutex(vivid_binding_mutex_t *mutex)
{
    taskEXIT_CRITICAL_FROM_ISR(mutex->interrupt_status);
}
#endif

vivid_binding_t *vivid_binding_freertos_create(TaskHandle_t task_handle, uint32_t notify_mask VIVID_LOG_ARGS(, vivid_binding_log_callback_t log_callback, void *logger))
{
#if !VIVID_BINDING_FREERTOS_YIELD_AFTER_NOTIFY
    m_yield_required = pdFALSE;
#endif
    vivid_binding_t *me = (vivid_binding_t *)pvPortMalloc(sizeof(*me));
    if (me == NULL) {
#if VIVID_LOG
        log_callback(logger, VIVID_LOG_LEVEL_ERROR, "could not allocate memory");
#endif
        return NULL;
    }
    memset(me, 0, sizeof(*me));
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
    me->data->task_handle = task_handle;
    me->data->notify_mask = notify_mask;
#if !VIVID_LOCKFREE
    me->data->mutex = me->create_mutex(me);
    if (me->data->mutex == NULL) {
        goto error;
    }
#endif
    return me;
error:
    vivid_binding_freertos_destroy(me);
    return NULL;
}

void vivid_binding_freertos_destroy(vivid_binding_t *me)
{
    if (me == NULL) {
        return;
    }
#if !VIVID_LOCKFREE
    if (me->data != NULL) {
        me->destroy_mutex(me->data->mutex);
    }
#endif
    me->free(me->data);
    me->free(me);
}

void vivid_binding_freertos_handle_event(vivid_binding_t *me)
{
    if (xTaskGetCurrentTaskHandle() != me->data->task_handle) {
        vivid_log_error(me, "event handled on incorrect task");
        return;
    }
    vivid_binding_event_t *event = me->data->events;
    while (event != NULL) {
        bool trig;
#if VIVID_LOCKFREE
        trig = true;
        (void)atomic_compare_exchange_strong(&event->trig, &trig, false);
#else
        (void)me->lock_mutex(me->data->mutex);
        trig = event->trig;
        event->trig = false;
        me->unlock_mutex(me->data->mutex);
#endif
        if (trig) {
            event->callback(event->data);
        }
        event = event->next;
    }
}

#if !VIVID_BINDING_FREERTOS_YIELD_AFTER_NOTIFY
BaseType_t vivid_binding_freertos_is_yield_required(void)
{
    BaseType_t yield_required = m_yield_required;
    m_yield_required = pdFALSE;
    return yield_required;
}
#endif
