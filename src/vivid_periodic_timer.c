// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#include <stdbool.h>
#include <vivid/util/periodic_timer.h>

struct vivid_periodic_timer {
    vivid_binding_t *binding;
    vivid_binding_event_t *event;
    vivid_binding_timer_t *timer;
    vivid_binding_callback_t callback;
    void *data;
    vivid_time_t due_time;
    bool active;
};

static void timer_callback(void *data)
{
    vivid_periodic_timer_t *me = (vivid_periodic_timer_t *)data;
    me->binding->trigger_event(me->event);
}

static void event_callback(void *data)
{
    vivid_periodic_timer_t *me = (vivid_periodic_timer_t *)data;
    if (!me->active || (me->due_time > me->binding->get_time(me->binding))) {
        return;
    }
    me->callback(me->data);
}

vivid_periodic_timer_t *vivid_periodic_timer_create(vivid_binding_t *binding, vivid_binding_callback_t callback, void *data)
{
    vivid_periodic_timer_t *me = (vivid_periodic_timer_t *)binding->calloc(binding, 1U, sizeof(*me));
    if (me == NULL) {
        return NULL;
    }
    me->binding = binding;
    me->callback = callback;
    me->data = data;
    me->timer = binding->create_timer(binding, timer_callback, me);
    me->event = binding->create_event(binding, event_callback, me);
    if ((me->timer == NULL) || (me->event == NULL)) {
        vivid_periodic_timer_destroy(me);
        return NULL;
    }
    return me;
}

void vivid_periodic_timer_destroy(vivid_periodic_timer_t *me)
{
    if (me == NULL) {
        return;
    }
    me->binding->destroy_event(me->event);
    me->binding->destroy_timer(me->timer);
    me->binding->free(me);
}

void vivid_periodic_timer_start(vivid_periodic_timer_t *me, vivid_time_t timeout)
{
    me->due_time = me->binding->get_time(me->binding) + timeout;
    me->binding->start_timer(me->timer, timeout);
    me->active = true;
}

void vivid_periodic_timer_stop(vivid_periodic_timer_t *me)
{
    me->binding->stop_timer(me->timer);
    me->active = false;
}
