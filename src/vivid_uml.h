// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#ifndef VIVID_UML_H
#define VIVID_UML_H

#include <vivid/sm.h>
#include <vivid/util/log.h>

#if !VIVID_UML
#define vivid_uml_destroy(me)
#define vivid_uml_on_transition(node, transition_type, name, timeout_text, guard_text, target_name, target_fn, action_text, json_props)
#define vivid_uml_on_entry_or_exit(node, dir, action_text)
#else
typedef enum {
    VIVID_TRANSITION_TYPE_DEFAULT,
    VIVID_TRANSITION_TYPE_EVENT,
    VIVID_TRANSITION_TYPE_TIMEOUT,
    VIVID_TRANSITION_TYPE_JUMP
} vivid_transition_type_t;

typedef struct vivid_uml vivid_uml_t;

vivid_uml_t *vivid_uml_create(vivid_binding_t *binding, vivid_log_t *log);

void vivid_uml_destroy(vivid_uml_t *me);

void vivid_uml_on_transition(vivid_node_t *node, vivid_transition_type_t transition_type, const char *name, const char *timeout_text, const char *guard_text, const char *target_name, vivid_state_t target_fn, const char *action_text, const char *json_props);

void vivid_uml_on_entry_or_exit(vivid_node_t *node, const char *dir, const char *action_text);
#endif

#endif
