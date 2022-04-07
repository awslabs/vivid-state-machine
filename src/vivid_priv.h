// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#ifndef VIVID_PRIV_H
#define VIVID_PRIV_H

#include "vivid_map.h"
#include "vivid_uml.h"
#include <vivid/sm.h>
#include <vivid/util/log.h>
#include <vivid/util/queue.h>

#if VIVID_LOCKFREE
#include <stdatomic.h>
#define STATE_TYPE_QUALIFIER _Atomic
#else
#define STATE_TYPE_QUALIFIER
#endif

struct vivid_node {
    vivid_sm_t *vsm;
    vivid_state_t fn;
    vivid_node_t *parent;
    vivid_node_t *children;
    vivid_node_t *siblings;
    vivid_node_t *default_child;
    vivid_node_t *STATE_TYPE_QUALIFIER state;
    size_t depth;
    const vivid_queue_entry_t *current_event;
#if VIVID_PARAM
    const vivid_queue_entry_t *last_event;
#endif
#if VIVID_LOG
    const char *name;
#endif
#if VIVID_UML
    const char *json_props;
#endif
    vivid_node_type_t type;
    bool parallel_children;
};

struct vivid_sm {
    vivid_binding_t *binding;
    vivid_binding_event_t *binding_event;
#if !VIVID_LOCKFREE
    vivid_binding_mutex_t *binding_mutex;
#endif
#if VIVID_LOG
    vivid_log_t *log;
    const char *name;
#endif
#if VIVID_UML
    vivid_uml_t *uml;
#endif
#if VIVID_PARAM_STATIC
    size_t max_param_size;
#endif
    void *app;
    vivid_node_t *root_node;
    vivid_map_t *node_map;
    vivid_map_t *timer_map;
    vivid_queue_t *event_queue;
    vivid_state_change_callback_t state_change_callback;
    struct {
        vivid_node_t *target;
        vivid_node_t *ancestor;
        bool reenter_ancestor;
        bool state_change;
    } transition;
    bool init;
    bool init_error;
    bool jump;
    bool event_handled;
};

void vivid_call_node(vivid_node_t *node, const char *event_string);

#endif
