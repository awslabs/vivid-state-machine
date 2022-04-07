// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#include <stdlib.h>
#include <unistd.h>
#include <vivid/binding/libev.h>
#include <vivid/util/log.h>

#if !VIVID_LOCKFREE
#include <pthread.h>
#endif

struct vivid_binding_data {
    struct ev_loop *loop;
};

struct vivid_binding_event {
    vivid_binding_t *binding;
    ev_async watcher;
    vivid_binding_callback_t callback;
    void *data;
};

struct vivid_binding_timer {
    vivid_binding_t *binding;
    ev_timer watcher;
    vivid_binding_callback_t callback;
    void *data;
};

#if !VIVID_LOCKFREE
struct vivid_binding_mutex {
    vivid_binding_t *binding;
    pthread_mutex_t mutex;
};
#endif

static void *calloc_mem(vivid_binding_t *me, size_t num, size_t size)
{
    void *mem = calloc(num, size);
    if (mem == NULL) {
        vivid_log_error(me, "could not allocate memory");
        return NULL;
    }
    return mem;
}

static void free_mem(void *mem)
{
    free(mem);
}

static void async_callback(struct ev_loop *loop, ev_async *w, int revents)
{
    (void)loop;
    (void)revents;
    vivid_binding_event_t *event = (vivid_binding_event_t *)w->data;
    event->callback(event->data);
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
    event->watcher.data = event;
    ev_async_init(&event->watcher, async_callback);
    ev_async_start(me->data->loop, &event->watcher);
    return event;
}

static void trigger_event(vivid_binding_event_t *event)
{
    vivid_binding_t *me = event->binding;
    ev_async_send(me->data->loop, &event->watcher);
}

static void destroy_event(vivid_binding_event_t *event)
{
    if (event == NULL) {
        return;
    }
    vivid_binding_t *me = event->binding;
    me->free(event);
}

static void timer_callback(struct ev_loop *loop, ev_timer *w, int revents)
{
    (void)loop;
    (void)revents;
    vivid_binding_timer_t *timer = (vivid_binding_timer_t *)w->data;
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
    timer->watcher.data = timer;
    ev_timer_init(&timer->watcher, timer_callback, 0.0, 0.0);
    return timer;
}

static void start_timer(vivid_binding_timer_t *timer, vivid_time_t timeout)
{
    vivid_binding_t *me = timer->binding;
    ev_timer_set(&timer->watcher, timeout + (ev_time() - ev_now(me->data->loop)), timeout);
    ev_timer_start(me->data->loop, &timer->watcher);
}

static void stop_timer(vivid_binding_timer_t *timer)
{
    vivid_binding_t *me = timer->binding;
    ev_timer_stop(me->data->loop, &timer->watcher);
}

static vivid_time_t get_time(vivid_binding_t *me)
{
    (void)me;
    return ev_time();
}

static void sleep_time(vivid_binding_t *me, vivid_time_t time)
{
    (void)me;
    (void)usleep((useconds_t)(time * 1000000.0));
}

static void destroy_timer(vivid_binding_timer_t *timer)
{
    if (timer == NULL) {
        return;
    }
    vivid_binding_t *me = timer->binding;
    me->free(timer);
}

#if !VIVID_LOCKFREE
static void destroy_mutex(vivid_binding_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }
    vivid_binding_t *me = mutex->binding;
    if (pthread_mutex_destroy(&mutex->mutex) != 0) {
        vivid_log_error(me, "could not destroy mutex");
    }
    me->free(mutex);
}

static vivid_binding_mutex_t *create_mutex(vivid_binding_t *me)
{
    vivid_binding_mutex_t *mutex = (vivid_binding_mutex_t *)me->calloc(me, 1U, sizeof(*mutex));
    if (mutex == NULL) {
        return NULL;
    }
    mutex->binding = me;
    if (pthread_mutex_init(&mutex->mutex, NULL) != 0) {
        vivid_log_error(me, "could not create mutex");
        destroy_mutex(mutex);
        return NULL;
    }
    return mutex;
}

static bool lock_mutex(vivid_binding_mutex_t *mutex)
{
    vivid_binding_t *me = mutex->binding;
    if (pthread_mutex_lock(&mutex->mutex) != 0) {
        vivid_log_error(me, "could not lock mutex");
        return false;
    }
    return true;
}

static void unlock_mutex(vivid_binding_mutex_t *mutex)
{
    vivid_binding_t *me = mutex->binding;
    if (pthread_mutex_unlock(&mutex->mutex) != 0) {
        vivid_log_error(me, "could not unlock mutex");
    }
}
#endif

vivid_binding_t *vivid_binding_libev_create(struct ev_loop *loop VIVID_LOG_ARGS(, vivid_binding_log_callback_t log_callback, void *logger))
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
        vivid_binding_libev_destroy(me);
        return NULL;
    }
    me->data->loop = loop;
    return me;
}

void vivid_binding_libev_destroy(vivid_binding_t *me)
{
    if (me == NULL) {
        return;
    }
    me->free(me->data);
    me->free(me);
}
