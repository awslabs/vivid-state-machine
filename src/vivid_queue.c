// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#include <string.h>
#include <vivid/util/log.h>
#include <vivid/util/queue.h>

#if VIVID_LOCKFREE
#include <stdatomic.h>
#define INDEX_TYPE_QUALIFIER _Atomic
#else
#define INDEX_TYPE_QUALIFIER
#endif

struct vivid_queue {
    vivid_binding_t *binding;
    vivid_queue_entry_t *entry_buffer;
    size_t size;
    INDEX_TYPE_QUALIFIER size_t read;
    INDEX_TYPE_QUALIFIER size_t write;
#if VIVID_LOCKFREE
    _Atomic size_t pending;
    _Atomic bool *ready_buffer;
#else
    vivid_binding_mutex_t *binding_mutex;
#endif
#if VIVID_PARAM_STATIC
    size_t max_param_size;
    char *param_buffer;
#endif
};

vivid_queue_t *vivid_queue_create(vivid_binding_t *binding, size_t size VIVID_PARAM_STATIC_ARGS(, size_t max_param_size))
{
    vivid_queue_t *me = (vivid_queue_t *)binding->calloc(binding, 1U, sizeof(*me));
    if (me == NULL) {
        return NULL;
    }
    me->binding = binding;
    me->size = size + 1U;
    me->entry_buffer = (vivid_queue_entry_t *)binding->calloc(binding, me->size, sizeof(*me->entry_buffer));
    if (me->entry_buffer == NULL) {
        goto error;
    }
#if VIVID_LOCKFREE
    me->ready_buffer = (atomic_bool *)binding->calloc(binding, me->size, sizeof(*me->ready_buffer));
    if (me->ready_buffer == NULL) {
        goto error;
    }
#else
    me->binding_mutex = binding->create_mutex(binding);
    if (me->binding_mutex == NULL) {
        goto error;
    }
#endif
#if VIVID_PARAM_STATIC
    me->max_param_size = max_param_size;
    me->param_buffer = (char *)binding->calloc(binding, me->size, max_param_size);
    if (me->param_buffer == NULL) {
        goto error;
    }
    for (size_t i = 0U; i < me->size; i++) {
        me->entry_buffer[i].param = &me->param_buffer[i * max_param_size];
    }
#endif
    return me;
error:
    vivid_queue_destroy(me);
    return NULL;
}

void vivid_queue_destroy(vivid_queue_t *me)
{
    if (me == NULL) {
        return;
    }
#if VIVID_PARAM_STATIC
    me->binding->free(me->param_buffer);
#endif
#if VIVID_LOCKFREE
    me->binding->free(me->ready_buffer);
#else
    me->binding->destroy_mutex(me->binding_mutex);
#endif
    me->binding->free(me->entry_buffer);
    me->binding->free(me);
}

static size_t inc_index(const vivid_queue_t *me, size_t index)
{
    index++;
    if (index >= me->size) {
        index = 0U;
    }
    return index;
}

bool vivid_queue_push(vivid_queue_t *me, const char *name VIVID_PARAM_ARGS(, VIVID_PARAM_STATIC_ARGS(const) void *param, VIVID_PARAM_STATIC_ARGS(size_t param_size) VIVID_PARAM_DYNAMIC_ARGS(vivid_param_destructor_t param_destructor)))
{
#if VIVID_PARAM_STATIC
    if (param_size > me->max_param_size) {
        vivid_log_error(me->binding, "queue param size too large");
        return false;
    }
#endif

    size_t index;
#if VIVID_LOCKFREE
    // Find the next available entry:
    for (;;) {
        index = atomic_load(&me->pending);
        size_t new_pending = inc_index(me, index);
        // If all entries are full:
        if (new_pending == atomic_load(&me->read)) {
            vivid_log_error(me->binding, "queue full");
            return false;
        }
        // Try to set the new pending index:
        if (atomic_compare_exchange_strong(&me->pending, &index, new_pending)) {
            break; // Success
        }
    }
#else
    // Lock the mutex and get the next write index:
    if (!me->binding->lock_mutex(me->binding_mutex)) {
        return false;
    }
    index = me->write;
    size_t new_write = inc_index(me, index);
    // If all entries are full:
    if (new_write == me->read) {
        me->binding->unlock_mutex(me->binding_mutex);
        vivid_log_error(me->binding, "queue full");
        return false;
    }
#endif

    // Fill the entry:
    vivid_queue_entry_t *entry = &me->entry_buffer[index];
    entry->name = name;
#if VIVID_PARAM
#if VIVID_PARAM_DYNAMIC
    entry->param = param;
    entry->param_destructor = param_destructor;
#else
    memcpy(entry->param, param, param_size);
    entry->param_size = param_size;
#endif
#endif

#if VIVID_LOCKFREE
    // Set the ready flag:
    atomic_bool *ready = &me->ready_buffer[index];
    bool ready_value = true;
    atomic_store(ready, ready_value);
    // Increment the write index for each 'ready' flag that can be changed from true to false:
    index = atomic_load(&me->write);
    for (;;) {
        ready = &me->ready_buffer[index];
        if (!atomic_compare_exchange_strong(ready, &ready_value, false)) {
            break; // Either the entry is not ready, or another thread has beaten us to it
        }
        index = inc_index(me, index);
        atomic_store(&me->write, index);
    }
#else
    // Update the write index and unlock the mutex:
    me->write = new_write;
    me->binding->unlock_mutex(me->binding_mutex);
#endif
    return true;
}

bool vivid_queue_empty(vivid_queue_t *me)
{
#if VIVID_LOCKFREE
    return atomic_load(&me->read) == atomic_load(&me->write);
#else
    (void)me->binding->lock_mutex(me->binding_mutex);
    bool empty = me->read == me->write;
    me->binding->unlock_mutex(me->binding_mutex);
    return empty;
#endif
}

const vivid_queue_entry_t *vivid_queue_front(const vivid_queue_t *me)
{
#if VIVID_LOCKFREE
    return &me->entry_buffer[atomic_load(&me->read)];
#else
    return &me->entry_buffer[me->read];
#endif
}

void vivid_queue_pop(vivid_queue_t *me)
{
#if VIVID_PARAM_DYNAMIC
    const vivid_queue_entry_t *entry = vivid_queue_front(me);
    if (entry->param_destructor != NULL) {
        entry->param_destructor(entry->param);
    }
#endif
#if VIVID_LOCKFREE
    atomic_store(&me->read, inc_index(me, atomic_load(&me->read)));
#else
    (void)me->binding->lock_mutex(me->binding_mutex);
    me->read = inc_index(me, me->read);
    me->binding->unlock_mutex(me->binding_mutex);
#endif
}
