// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <vivid/binding/linux.h>
#include <vivid/util/log.h>

#if VIVID_LOCKFREE
#include <stdatomic.h>
#define TRIG_TYPE_QUALIFIER _Atomic
#else
#define TRIG_TYPE_QUALIFIER
#endif

struct vivid_binding_data {
    vivid_binding_event_t *events;
    int efd;
    int event_fd;
    int quit_fd;
    pthread_t timer_thread_id;
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
    int timer_fd;
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
#if VIVID_LOCKFREE
    atomic_store(&event->trig, true);
#else
    (void)me->lock_mutex(me->data->mutex);
    event->trig = true;
    me->unlock_mutex(me->data->mutex);
#endif
    uint64_t val = 1U;
    if (write(me->data->event_fd, &val, sizeof(val)) < sizeof(val)) {
        vivid_log_error(me, "could not set event fd");
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
    if (epoll_ctl(me->data->efd, EPOLL_CTL_DEL, timer->timer_fd, NULL) < 0) {
        vivid_log_error(me, "could not remove timer fd");
    }
    (void)close(timer->timer_fd);
    me->free(timer);
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
    timer->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timer->timer_fd < 0) {
        vivid_log_error(me, "could not create timer");
        goto error;
    }
    struct epoll_event ev = { 0 };
    ev.events = EPOLLIN;
    ev.data.ptr = timer;
    if (epoll_ctl(me->data->efd, EPOLL_CTL_ADD, timer->timer_fd, &ev) < 0) {
        vivid_log_error(me, "could not add timer fd");
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
    struct itimerspec its = { 0 };
    its.it_value.tv_sec = (time_t)timeout;
    its.it_value.tv_nsec = (time_t)(fmod(timeout, 1.0) * 1000000000.0);
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;
    if (timerfd_settime(timer->timer_fd, 0, &its, NULL) < 0) {
        vivid_log_error(me, "could not set timer fd");
        if (me->error_hook != NULL) {
            me->error_hook(me->app, (timeout > 0.0) ? VIVID_ERROR_START_TIMER : VIVID_ERROR_STOP_TIMER);
        }
    }
}

static void stop_timer(vivid_binding_timer_t *timer)
{
    start_timer(timer, 0.0);
}

static vivid_time_t get_time(vivid_binding_t *me)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        vivid_log_error(me, "could not get time");
        if (me->error_hook != NULL) {
            me->error_hook(me->app, VIVID_ERROR_GET_TIME);
        }
        return 0.0;
    }
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}

static void sleep_time(vivid_binding_t *me, vivid_time_t time)
{
    (void)me;
    (void)usleep((useconds_t)(time * 1000000.0));
}

static void *timer_thread(void *arg)
{
    vivid_binding_t *me = (vivid_binding_t *)arg;
    for (;;) {
        struct epoll_event ev;
        int num_events = epoll_wait(me->data->efd, &ev, 1U, -1);
        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            vivid_log_error(me, "could not wait on efd");
            break;
        }
        vivid_binding_timer_t *timer = (vivid_binding_timer_t *)ev.data.ptr;
        if (timer == NULL) { // quit
            return NULL;
        }
        uint64_t res;
        if (read(timer->timer_fd, &res, sizeof(res)) < sizeof(res)) {
            vivid_log_error(me, "could not read from timer fd");
            break;
        }
        timer->callback(timer->data);
    }
    if (me->error_hook != NULL) {
        me->error_hook(me->app, VIVID_ERROR_TIMER);
    }
    return NULL;
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
        if (me->error_hook != NULL) {
            me->error_hook(me->app, VIVID_ERROR_LOCK_MUTEX);
        }
        return false;
    }
    return true;
}

static void unlock_mutex(vivid_binding_mutex_t *mutex)
{
    vivid_binding_t *me = mutex->binding;
    if (pthread_mutex_unlock(&mutex->mutex) != 0) {
        vivid_log_error(me, "could not unlock mutex");
        if (me->error_hook != NULL) {
            me->error_hook(me->app, VIVID_ERROR_UNLOCK_MUTEX);
        }
    }
}
#endif

vivid_binding_t *vivid_binding_linux_create(int *fd VIVID_LOG_ARGS(, vivid_binding_log_callback_t log_callback, void *logger))
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
#if !VIVID_LOCKFREE
    me->data->mutex = me->create_mutex(me);
    if (me->data->mutex == NULL) {
        goto error;
    }
#endif
    me->data->event_fd = eventfd(0U, EFD_NONBLOCK);
    me->data->quit_fd = eventfd(0U, EFD_NONBLOCK);
    if ((me->data->event_fd < 0) || (me->data->quit_fd < 0)) {
        vivid_log_error(me, "could not create event fd");
        goto error;
    }
    *fd = me->data->event_fd;
    me->data->efd = epoll_create1(0);
    if (me->data->efd < 0) {
        vivid_log_error(me, "could not create efd");
        goto error;
    }
    struct epoll_event ev = { 0 };
    ev.events = EPOLLIN;
    ev.data.ptr = NULL;
    if (epoll_ctl(me->data->efd, EPOLL_CTL_ADD, me->data->quit_fd, &ev) < 0) {
        vivid_log_error(me, "could add fd");
        goto error;
    }
    if (pthread_create(&me->data->timer_thread_id, NULL, timer_thread, me) < 0) {
        vivid_log_error(me, "could not start timer thread");
        goto error;
    }
    return me;
error:
    vivid_binding_linux_destroy(me);
    return NULL;
}

void vivid_binding_linux_destroy(vivid_binding_t *me)
{
    if (me == NULL) {
        return;
    }
    if (me->data != NULL) {
        if (me->data->timer_thread_id != 0U) {
            uint64_t val = 1U;
            if (write(me->data->quit_fd, &val, sizeof(val)) < sizeof(val)) {
                vivid_log_error(me, "could set event fd");
            }
            if (pthread_join(me->data->timer_thread_id, NULL) < 0) {
                vivid_log_error(me, "could not join timer thread");
            }
        }
        (void)close(me->data->efd);
        (void)close(me->data->event_fd);
        (void)close(me->data->quit_fd);
#if !VIVID_LOCKFREE
        me->destroy_mutex(me->data->mutex);
#endif
        me->free(me->data);
    }
    me->free(me);
}

void vivid_binding_linux_handle_event(vivid_binding_t *me)
{
    uint64_t val;
    if (read(me->data->event_fd, &val, sizeof(val)) < sizeof(val)) {
        vivid_log_error(me, "could not read event fd");
        if (me->error_hook != NULL) {
            me->error_hook(me->app, VIVID_ERROR_EVENT);
        }
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
