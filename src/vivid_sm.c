// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#include "vivid_priv.h"

#if VIVID_LOG
#include <string.h>
#endif

//--------------------------------------------------------------------------------------------------
// Options

#ifndef VIVID_LOG_BUFFER_SIZE
#define VIVID_LOG_BUFFER_SIZE 256U
#endif
//--------------------------------------------------------------------------------------------------

typedef struct {
    vivid_sm_t *vsm;
    const char *name;
    vivid_binding_timer_t *binding_timer;
    vivid_time_t due_time;
    bool active;
} vivid_sm_timer_t;

static const char *const m_init_string = "init";
static const char *const m_entry_string = "entry";
static const char *const m_exit_string = "exit";
static const char *const m_jump_string = "jump";
#if VIVID_LOG
static const char *const m_true_string = "true";
static const char *const m_false_string = "false";
#endif

void vivid_call_node(vivid_node_t *node, const char *event_string)
{
    vivid_queue_entry_t event = { 0 };
    event.name = event_string;
    node->current_event = &event;
    node->fn(node, node->vsm->app);
    node->current_event = NULL;
}

static vivid_node_t *walk_init(vivid_sm_t *me, vivid_state_t fn, size_t depth VIVID_LOG_ARGS(, const char *name))
{
    vivid_node_t **value = (vivid_node_t **)vivid_map_set(me->node_map, (size_t)fn);
    if (value == NULL) {
        return NULL;
    }
    if (*value != NULL) {
        VIVID_LOG_ERROR(me->log, "sub-state defined more than once: %s", name);
        return NULL;
    }
    vivid_node_t *node = (vivid_node_t *)me->binding->calloc(me->binding, 1U, sizeof(*node));
    if (node == NULL) {
        return NULL;
    }
    *value = node;
    node->vsm = me;
#if VIVID_LOG
    node->name = name;
#endif
    node->fn = fn;
    node->depth = depth;
    vivid_call_node(node, m_init_string); // Note: this function is recursive via this call
    if (me->init_error) {
        return NULL;
    }
    if ((node->children != NULL) && !node->parallel_children && (node->default_child == NULL)) {
        VIVID_LOG_ERROR(me->log, "%s | %s | undefined default sub-state", me->name, name);
        return NULL;
    }
    return node;
}

static void set_state(vivid_node_t *node, vivid_node_t *value)
{
    vivid_sm_t *me = node->vsm;
    // Ignore pseudo-state changes:
    if ((value != NULL) && ((value->type == VIVID_NODE_TYPE_STATE) || (value->type == VIVID_NODE_TYPE_STATE_FINAL))) {
        me->transition.state_change = true;
    }
#if VIVID_LOCKFREE
    atomic_store(&node->state, value);
#else
    (void)me->binding->lock_mutex(me->binding_mutex);
    node->state = value;
    me->binding->unlock_mutex(me->binding_mutex);
#endif
}

static vivid_node_t *get_state(vivid_node_t *node)
{
#if VIVID_LOCKFREE
    return atomic_load(&node->state);
#else
    vivid_sm_t *me = node->vsm;
    (void)me->binding->lock_mutex(me->binding_mutex);
    vivid_node_t *value = node->state;
    me->binding->unlock_mutex(me->binding_mutex);
    return value;
#endif
}

static void walk_entry_down(vivid_node_t *node, const vivid_node_t *except)
{
    if (node != except) {
        vivid_call_node(node, m_entry_string);
        if (node->default_child != NULL) {
            set_state(node, node->default_child);
            walk_entry_down(node->default_child, NULL);
        }
        if (node->parallel_children) {
            walk_entry_down(node->children, NULL);
        }
    }
    if ((node->parent != NULL) && node->parent->parallel_children && (node->siblings != NULL)) {
        walk_entry_down(node->siblings, except);
    }
}

static void walk_entry_up(vivid_node_t *node, const vivid_node_t *ancestor, vivid_node_t *branch, bool enter_ancestor)
{
    if ((node->parent != NULL) && (node != ancestor)) {
        walk_entry_up(node->parent, ancestor, node, enter_ancestor);
    }
    if (branch != NULL) {
        set_state(node, branch);
        if ((node != ancestor) || enter_ancestor) {
            vivid_call_node(node, m_entry_string);
        }
        if (node->parallel_children) {
            walk_entry_down(node->children, branch);
        }
        return;
    }
    walk_entry_down(node, NULL);
}

static void walk_exit_down(vivid_node_t *node, const vivid_node_t *except)
{
    if (node != except) {
        vivid_node_t *state = get_state(node);
        if (state != NULL) {
            walk_exit_down(state, NULL);
            set_state(node, NULL);
        }
        if (node->parallel_children) {
            walk_exit_down(node->children, NULL);
        }
        vivid_call_node(node, m_exit_string);
    }
    if ((node->parent != NULL) && node->parent->parallel_children && (node->siblings != NULL)) {
        walk_exit_down(node->siblings, except);
    }
}

static void walk_exit_up(vivid_node_t *node, const vivid_node_t *ancestor, const vivid_node_t *branch, bool exit_ancestor)
{
    vivid_node_t *state = get_state(node);
    if ((branch == NULL) && (state != NULL)) {
        walk_exit_down(state, NULL);
    }
    if (node->parallel_children) {
        walk_exit_down(node->children, branch);
    }
    set_state(node, NULL);
    if ((node != ancestor) || exit_ancestor) {
        vivid_call_node(node, m_exit_string);
    }
    if ((node->parent != NULL) && (node != ancestor)) {
        walk_exit_up(node->parent, ancestor, node, exit_ancestor);
    }
}

static void walk_event(vivid_node_t *node, const vivid_queue_entry_t *current_event VIVID_PARAM_ARGS(, const vivid_queue_entry_t *last_event))
{
    vivid_sm_t *me = node->vsm;
    node->current_event = current_event;
#if VIVID_PARAM
    node->last_event = last_event;
#endif
    node->fn(node, me->app);
    node->current_event = NULL;
#if VIVID_PARAM
    node->last_event = NULL;
#endif
    if (me->transition.target != NULL) {
        walk_entry_up(me->transition.target, me->transition.ancestor, NULL, me->transition.reenter_ancestor);
        me->transition.target = NULL;
        if ((me->state_change_callback != NULL) && me->transition.state_change) {
            me->state_change_callback(me->app);
        }
        return;
    }
    if (node->parallel_children) {
        walk_event(node->children, current_event VIVID_PARAM_ARGS(, last_event));
    }
    if ((node->parent != NULL) && node->parent->parallel_children && (node->siblings != NULL)) {
        walk_event(node->siblings, current_event VIVID_PARAM_ARGS(, last_event));
    }
    vivid_node_t *state = get_state(node);
    if (state != NULL) {
        walk_event(state, current_event VIVID_PARAM_ARGS(, last_event));
    }
}

static void jump(vivid_sm_t *me VIVID_PARAM_ARGS(, const vivid_queue_entry_t *last_event))
{
    vivid_queue_entry_t event = { 0 };
    event.name = m_jump_string;
    while (me->jump) {
        me->jump = false;
        walk_event(me->root_node, &event VIVID_PARAM_ARGS(, last_event));
    }
}

static void event_callback(void *data)
{
    vivid_sm_t *me = (vivid_sm_t *)data;
    if (me->init) {
        me->init = false;
        walk_entry_down(me->root_node, NULL);
        jump(me VIVID_PARAM_ARGS(, NULL));
    }
    if (vivid_queue_empty(me->event_queue)) {
        return;
    }
    const vivid_queue_entry_t *event = vivid_queue_front(me->event_queue);
    me->event_handled = false;
    walk_event(me->root_node, event, NULL);
    if (!me->event_handled) {
        VIVID_LOG_DEBUG(me->log, "%s | event | %s (unhandled)", me->name, event->name);
    }
    jump(me VIVID_PARAM_ARGS(, event));
    vivid_queue_pop(me->event_queue);
    if (!vivid_queue_empty(me->event_queue)) {
        me->binding->trigger_event(me->binding_event);
    }
}

vivid_sm_t *vivid_create_sm(vivid_binding_t *binding, vivid_state_t root_fn, size_t event_queue_size, void *app VIVID_LOG_ARGS(, const char *name, const char *root_name))
{
    vivid_sm_t *me = (vivid_sm_t *)binding->calloc(binding, 1U, sizeof(*me));
    if (me == NULL) {
        return NULL;
    }
    me->binding = binding;
    me->app = app;
    me->binding_event = binding->create_event(binding, event_callback, me);
    me->node_map = vivid_map_create(binding);
    me->timer_map = vivid_map_create(binding);
    if ((me->binding_event == NULL) || (me->node_map == NULL) || (me->timer_map == NULL)) {
        goto error;
    }
#if VIVID_LOG
    me->name = name;
    me->log = vivid_log_create(binding, VIVID_LOG_BUFFER_SIZE);
    if (me->log == NULL) {
        goto error;
    }
#endif
#if VIVID_UML
    me->uml = vivid_uml_create(binding, me->log);
    if (me->uml == NULL) {
        goto error;
    }
#endif
#if !VIVID_LOCKFREE
    me->binding_mutex = binding->create_mutex(binding);
    if (me->binding_mutex == NULL) {
        goto error;
    }
#endif

    me->root_node = walk_init(me, root_fn, 0U VIVID_LOG_ARGS(, root_name));
    if (me->root_node == NULL) {
        goto error;
    }

    me->event_queue = vivid_queue_create(binding, event_queue_size VIVID_PARAM_STATIC_ARGS(, me->max_param_size));
    if ((me->event_queue == NULL)) {
        goto error;
    }

    me->init = true;
    binding->trigger_event(me->binding_event);
    return me;

error:
    vivid_destroy_sm(me);
    return NULL;
}

static void destroy_timer(void *app, size_t key, void *value)
{
    (void)key;
    vivid_sm_t *me = (vivid_sm_t *)app;
    vivid_sm_timer_t *timer = (vivid_sm_timer_t *)value;
    me->binding->destroy_timer(timer->binding_timer);
    me->binding->free(timer);
}

static void destroy_node(void *app, size_t key, void *value)
{
    (void)key;
    vivid_sm_t *me = (vivid_sm_t *)app;
    vivid_node_t *node = (vivid_node_t *)value;
    me->binding->free(node);
}

void vivid_destroy_sm(vivid_sm_t *me)
{
    if (me == NULL) {
        return;
    }
    vivid_queue_destroy(me->event_queue);
    vivid_map_iterate(me->timer_map, destroy_timer, me);
    vivid_map_destroy(me->timer_map);
    vivid_map_iterate(me->node_map, destroy_node, me);
    vivid_map_destroy(me->node_map);
#if !VIVID_LOCKFREE
    me->binding->destroy_mutex(me->binding_mutex);
#endif
    vivid_uml_destroy(me->uml);
    vivid_log_destroy(me->log);
    me->binding->destroy_event(me->binding_event);
    me->binding->free(me);
}

vivid_binding_t *vivid_get_binding(vivid_sm_t *me)
{
    return me->binding;
}

void vivid_set_state_change_callback(vivid_sm_t *me, vivid_state_change_callback_t callback)
{
    me->state_change_callback = callback;
}

#if VIVID_LOG
static const char *get_node_type_string(vivid_node_type_t type)
{
    // clang-format off
    switch (type) {
    case VIVID_NODE_TYPE_ROOT:           return "root";
    case VIVID_NODE_TYPE_STATE:          return "state";
    case VIVID_NODE_TYPE_STATE_FINAL:    return "final state";
    case VIVID_NODE_TYPE_STATE_PARALLEL: return "parallel state";
    case VIVID_NODE_TYPE_CONDITION:      return "condition";
    case VIVID_NODE_TYPE_JUNCTION:       return "junction";
    default:                             return "unknown";
    }
    // clang-format on
}
#endif

void vivid_sub_node(vivid_node_t *node, vivid_state_t fn, vivid_node_type_t type VIVID_LOG_ARGS(, const char *name VIVID_UML_ARGS(, const char *json_props)))
{
    if (node->current_event->name != m_init_string) {
        return;
    }
    vivid_sm_t *me = node->vsm;
    VIVID_LOG_DEBUG(me->log, "%s | init | %s | sub-%s: %s", me->name, node->name, get_node_type_string(type), name);
    if ((type == VIVID_NODE_TYPE_STATE_PARALLEL)
            ? ((node->children != NULL) && !node->parallel_children)
            : node->parallel_children) {
        VIVID_LOG_ERROR(me->log, "%s | init | %s | parallel and non-parallel sub-states", me->name, node->name);
        me->init_error = true;
        return;
    }
    vivid_node_t *sub_node = walk_init(me, fn, node->depth + 1U VIVID_LOG_ARGS(, name));
    if (sub_node == NULL) {
        me->init_error = true;
        return;
    }
    sub_node->type = type;
    sub_node->parent = node;
    sub_node->siblings = node->children;
#if VIVID_UML
    sub_node->json_props = json_props;
#endif
    node->children = sub_node;
    node->parallel_children = type == VIVID_NODE_TYPE_STATE_PARALLEL;
}

bool vivid_default(vivid_node_t *node, vivid_state_t fn VIVID_LOG_ARGS(, const char *name VIVID_UML_ARGS(, const char *action_text, const char *json_props)))
{
    vivid_uml_on_transition(node, VIVID_TRANSITION_TYPE_DEFAULT, "", "", m_true_string, name, fn, action_text, json_props);
    vivid_sm_t *me = node->vsm;
    if (node->current_event->name == m_init_string) {
        if (node->default_child != NULL) {
            VIVID_LOG_ERROR(me->log, "%s | init | %s | default already defined as %s", me->name, node->name, node->default_child->name);
            me->init_error = true;
            return false;
        }
        vivid_node_t *sub_node = (vivid_node_t *)vivid_map_get(me->node_map, (size_t)fn);
        if (sub_node == NULL) {
            VIVID_LOG_ERROR(me->log, "%s | init | %s | sub-state %s not yet defined", me->name, node->name, name);
            me->init_error = true;
            return false;
        }
        VIVID_LOG_DEBUG(me->log, "%s | init | %s | default: %s", me->name, node->name, name);
        node->default_child = sub_node;
        return false;
    }
    if (node->current_event->name != m_entry_string) {
        return false;
    }
    VIVID_LOG_DEBUG(me->log, "%s | default | %s | %s", me->name, node->name, name);
    return true;
}

bool vivid_on_entry(vivid_node_t *node VIVID_UML_ARGS(, const char *action_text))
{
    vivid_uml_on_entry_or_exit(node, m_entry_string, action_text);
    if (node->current_event->name != m_entry_string) {
        return false;
    }
    VIVID_LOG_DEBUG(node->vsm->log, "%s | entry | %s", node->vsm->name, node->name);
    return true;
}

bool vivid_on_exit(vivid_node_t *node VIVID_UML_ARGS(, const char *action_text))
{
    vivid_uml_on_entry_or_exit(node, m_exit_string, action_text);
    if (node->current_event->name != m_exit_string) {
        return false;
    }
    VIVID_LOG_DEBUG(node->vsm->log, "%s | exit | %s", node->vsm->name, node->name);
    return true;
}

bool vivid_on_event(vivid_node_t *node, const char *name VIVID_PARAM_ARGS(, const void **param VIVID_PARAM_STATIC_ARGS(, size_t param_size)) VIVID_UML_ARGS(, vivid_state_t target_fn, const char *guard_text, const char *target_name, const char *action_text, const char *json_props))
{
    vivid_uml_on_transition(node, VIVID_TRANSITION_TYPE_EVENT, name, "", guard_text, target_name, target_fn, action_text, json_props);
    vivid_sm_t *me = node->vsm;
#if VIVID_PARAM_STATIC
    if (node->current_event->name == m_init_string) {
        if (param_size > me->max_param_size) {
            me->max_param_size = param_size;
        }
        return false;
    }
#endif
    if (node->current_event->name != name) {
        return false;
    }
#if VIVID_PARAM
    if (param != NULL) {
        *param = node->current_event->param;
    }
#endif
    me->event_handled = true;
    return true;
}

static void timer_callback(void *data)
{
    vivid_sm_timer_t *timer = (vivid_sm_timer_t *)data;
    vivid_queue_event(timer->vsm, timer->name VIVID_PARAM_ARGS(, NULL, VIVID_PARAM_STATIC_ARGS(0U) VIVID_PARAM_DYNAMIC_ARGS(NULL)));
}

bool vivid_on_timeout(vivid_node_t *node, const char *name, vivid_time_t timeout VIVID_UML_ARGS(, vivid_state_t target_fn, const char *timeout_text, const char *guard_text, const char *target_name, const char *action_text, const char *json_props))
{
    vivid_uml_on_transition(node, VIVID_TRANSITION_TYPE_TIMEOUT, name, timeout_text, guard_text, target_name, target_fn, action_text, json_props);
    vivid_sm_t *me = node->vsm;
    if (node->current_event->name == m_init_string) {
        vivid_sm_timer_t **value = (vivid_sm_timer_t **)vivid_map_set(me->timer_map, (size_t)name);
        if (value == NULL) {
            me->init_error = true;
            return false;
        }
        if (*value != NULL) {
            VIVID_LOG_ERROR(me->log, "%s | timer %s not unique", me->name, name);
            me->init_error = true;
            return false;
        }
        vivid_sm_timer_t *timer = (vivid_sm_timer_t *)me->binding->calloc(me->binding, 1U, sizeof(*timer));
        if (timer == NULL) {
            me->init_error = true;
            return false;
        }
        *value = timer;
        timer->vsm = me;
        timer->name = name;
        timer->binding_timer = me->binding->create_timer(me->binding, timer_callback, timer);
        if (timer->binding_timer == NULL) {
            me->init_error = true;
            return false;
        }
        return false;
    }
    if ((node->current_event->name != m_entry_string) && (node->current_event->name != m_exit_string) && (node->current_event->name != name)) {
        return false;
    }
    vivid_sm_timer_t *timer = (vivid_sm_timer_t *)vivid_map_get(me->timer_map, (size_t)name);
    if (timer == NULL) {
        VIVID_LOG_ERROR(me->log, "%s | could not find timer %s", me->name, name);
        return false;
    }
    vivid_time_t current_time = me->binding->get_time(me->binding);
    if (node->current_event->name == m_entry_string) {
        timer->due_time = current_time + timeout;
        me->binding->start_timer(timer->binding_timer, timeout);
        timer->active = true;
        return false;
    }
    if (node->current_event->name == m_exit_string) {
        me->binding->stop_timer(timer->binding_timer);
        timer->active = false;
        return false;
    }
    if (!timer->active || (timer->due_time > current_time)) {
        return false; // Ignore late arriving timeouts
    }
    me->event_handled = true;
    timer->active = false;
    return true;
}

bool vivid_jump(vivid_node_t *node VIVID_PARAM_ARGS(, const void **param, const char *param_event_name) VIVID_UML_ARGS(, vivid_state_t target_fn, const char *guard_text, const char *target_name, const char *action_text, const char *json_props))
{
    vivid_uml_on_transition(node, VIVID_TRANSITION_TYPE_JUMP, "", "", guard_text, target_name, target_fn, action_text, json_props);
    vivid_sm_t *me = node->vsm;
    if (node->current_event->name == m_entry_string) {
        me->jump = true;
        return false;
    }
    if (node->current_event->name != m_jump_string) {
        return false;
    }
#if VIVID_PARAM
    if (param != NULL) {
        if (node->last_event == NULL) {
            VIVID_LOG_ERROR(me->log, "%s | %s | no jump param available: %s", me->name, node->name, param_event_name);
            return false;
        }
        if (node->last_event->name != param_event_name) {
            VIVID_LOG_ERROR(me->log, "%s | %s | incorrect jump param: %s vs %s", me->name, node->name, param_event_name, node->last_event->name);
            return false;
        }
        *param = node->last_event->param;
    }
#endif
    return true;
}

bool vivid_transit(vivid_node_t *node, bool guard, vivid_state_t target_fn VIVID_LOG_ARGS(, const char *type, const char *name, const char *guard_text, const char *target_name))
{
    vivid_sm_t *me = node->vsm;
#if VIVID_LOG
    if (!guard || (strcmp(guard_text, m_true_string) != 0)) {
        VIVID_LOG_DEBUG(me->log, "%s | %-5s | %s%s%s: \"%s\" is %s", me->name, type, name, (*name != '\0') ? ": " : "", node->name, guard_text, guard ? m_true_string : m_false_string);
    } else if (node->type == VIVID_NODE_TYPE_CONDITION) {
        VIVID_LOG_DEBUG(me->log, "%s | %-5s | %s%s%s: [else]", me->name, type, name, (*name != '\0') ? ": " : "", node->name);
    }
#endif
    if (!guard) {
        return false;
    }
    VIVID_LOG_INFO(me->log, "%s | %-5s | %s%s%s%s%s", me->name, type, name, (*name != '\0') ? ": " : "", node->name, (target_fn != NULL) ? " -> " : "", (target_fn != NULL) ? target_name : "");
    if (target_fn == NULL) {
        return true;
    }
    vivid_node_t *target_node = vivid_map_get(me->node_map, (size_t)target_fn);
    if (target_node == NULL) {
        VIVID_LOG_ERROR(me->log, "%s | %s | node not found: %s", me->name, node->name, target_name);
        return false;
    }
    vivid_node_t *ancestor_src = node;
    while (ancestor_src->depth > target_node->depth) {
        ancestor_src = ancestor_src->parent;
    }
    vivid_node_t *ancestor_dest = target_node;
    while (ancestor_dest->depth > node->depth) {
        ancestor_dest = ancestor_dest->parent;
    }
    while (ancestor_src != ancestor_dest) {
        ancestor_src = ancestor_src->parent;
        ancestor_dest = ancestor_dest->parent;
    }
    bool reenter_ancestor = (node == ancestor_src) || (target_node == ancestor_src);
    walk_exit_up(node, ancestor_src, NULL, reenter_ancestor);
    me->transition.target = target_node;
    me->transition.ancestor = ancestor_src;
    me->transition.reenter_ancestor = reenter_ancestor;
    me->transition.state_change = false;
    return true;
}

void vivid_queue_event(vivid_sm_t *me, const char *name VIVID_PARAM_ARGS(, VIVID_PARAM_STATIC_ARGS(const) void *param, VIVID_PARAM_STATIC_ARGS(size_t param_size) VIVID_PARAM_DYNAMIC_ARGS(vivid_param_destructor_t param_destructor)))
{
    if (!vivid_queue_push(me->event_queue, name VIVID_PARAM_ARGS(, param VIVID_PARAM_STATIC_ARGS(, param_size) VIVID_PARAM_DYNAMIC_ARGS(, param_destructor)))) {
        vivid_log_error(me->binding, "queue event error - vsm name and event name to follow");
        vivid_log_error(me->binding, me->name);
        vivid_log_error(me->binding, name);
        if (me->binding->error_hook != NULL) {
            me->binding->error_hook(me->binding->app, VIVID_ERROR_QUEUE_EVENT);
        }
        return;
    }
    me->binding->trigger_event(me->binding_event);
}

bool vivid_is_in(vivid_sm_t *me, vivid_state_t state)
{
    vivid_node_t *node = (vivid_node_t *)vivid_map_get(me->node_map, (size_t)state);
    return (node != NULL) && ((node->parent == NULL) || (get_state(node->parent) == node));
}

vivid_state_t vivid_get_state(vivid_sm_t *me, vivid_state_t parent_state VIVID_LOG_ARGS(, const char **name))
{
    vivid_node_t *parent_node = (vivid_node_t *)vivid_map_get(me->node_map, (size_t)parent_state);
    if (parent_node == NULL) {
        goto undefined_state;
    }
    const vivid_node_t *node = get_state(parent_node);
    if (node == NULL) {
        goto undefined_state;
    }
#if VIVID_LOG
    if (name != NULL) {
        *name = node->name;
    }
#endif
    return node->fn;
undefined_state:
#if VIVID_LOG
    if (name != NULL) {
        *name = "(undefined)";
    }
#endif
    return NULL;
}
