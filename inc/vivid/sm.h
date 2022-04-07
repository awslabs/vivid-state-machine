// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#ifndef VIVID_SM_H
#define VIVID_SM_H

#include <vivid/binding.h>
#include <vivid/util/queue.h>
#if VIVID_PARAM_DYNAMIC
#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
// Options

#if !VIVID_LOG
#undef VIVID_UML
#define VIVID_UML 0
#elif !defined(VIVID_UML)
#define VIVID_UML 1
#endif
//--------------------------------------------------------------------------------------------------

#if VIVID_UML
#define VIVID_UML_ARGS(...) __VA_ARGS__
#else
#define VIVID_UML_ARGS(...)
#endif

#define VIVID_CREATE_SM(binding, name, root, event_queue_size, app) \
    vivid_create_sm(binding, state_##root, event_queue_size, app VIVID_LOG_ARGS(, name, #root))

#define VIVID_DECLARE_STATE(name) \
    static void state_##name(vivid_node_t *node, void *app)

#define VIVID_DECLARE_STATE_CPP(name)           \
    void state_##name##_fn(vivid_node_t *node); \
    static void state_##name(vivid_node_t *node, void *app)

#define VIVID_STATE(type, name)                                  \
    static void state_##name##_fn(vivid_node_t *node, type *me); \
    static void state_##name(vivid_node_t *node, void *app)      \
    {                                                            \
        state_##name##_fn(node, (type *)app);                    \
    }                                                            \
    static void state_##name##_fn(vivid_node_t *node, type *me)

#define VIVID_STATE_CPP(module, name)                        \
    void module::state_##name(vivid_node_t *node, void *app) \
    {                                                        \
        ((module *)app)->state_##name##_fn(node);            \
    }                                                        \
    void module::state_##name##_fn(vivid_node_t *node)

#define VIVID_SUB_STATE(name, /* json_props */...)                                                                        \
    vivid_sub_node(node, /* Use VIVID_DECLARE_STATE() or Use VIVID_DECLARE_STATE_CPP() before this macro */ state_##name, \
        VIVID_NODE_TYPE_STATE VIVID_LOG_ARGS(, #name VIVID_UML_ARGS(, "" #__VA_ARGS__)))

#define VIVID_SUB_STATE_PARALLEL(name, /* json_props */...)                                                               \
    vivid_sub_node(node, /* Use VIVID_DECLARE_STATE() or Use VIVID_DECLARE_STATE_CPP() before this macro */ state_##name, \
        VIVID_NODE_TYPE_STATE_PARALLEL VIVID_LOG_ARGS(, #name VIVID_UML_ARGS(, "" #__VA_ARGS__)))

#define VIVID_SUB_STATE_FINAL(name, /* json_props */...)                                                                  \
    vivid_sub_node(node, /* Use VIVID_DECLARE_STATE() or Use VIVID_DECLARE_STATE_CPP() before this macro */ state_##name, \
        VIVID_NODE_TYPE_STATE_FINAL VIVID_LOG_ARGS(, #name VIVID_UML_ARGS(, "" #__VA_ARGS__)))

#define VIVID_SUB_CONDITION(name, /* json_props */...)                                                                    \
    vivid_sub_node(node, /* Use VIVID_DECLARE_STATE() or Use VIVID_DECLARE_STATE_CPP() before this macro */ state_##name, \
        VIVID_NODE_TYPE_CONDITION VIVID_LOG_ARGS(, #name VIVID_UML_ARGS(, "" #__VA_ARGS__)))

#define VIVID_SUB_JUNCTION(name, /* json_props */...)                                                                     \
    vivid_sub_node(node, /* Use VIVID_DECLARE_STATE() or Use VIVID_DECLARE_STATE_CPP() before this macro */ state_##name, \
        VIVID_NODE_TYPE_JUNCTION VIVID_LOG_ARGS(, #name VIVID_UML_ARGS(, "" #__VA_ARGS__)))

#define VIVID_DEFAULT(name, action, /* json_props */...)                                                                                                                                           \
    if (vivid_default(node, /* Use VIVID_DECLARE_STATE() or Use VIVID_DECLARE_STATE_CPP() before this macro */ state_##name VIVID_LOG_ARGS(, #name VIVID_UML_ARGS(, #action, "" #__VA_ARGS__)))) { \
        action                                                                                                                                                                                     \
    }

#define VIVID_ON_ENTRY(action)                            \
    if (vivid_on_entry(node VIVID_UML_ARGS(, #action))) { \
        action                                            \
    }

#define VIVID_ON_EXIT(action)                            \
    if (vivid_on_exit(node VIVID_UML_ARGS(, #action))) { \
        action                                           \
    }

#define VIVID_DECLARE_EVENT_PUBLIC(type, module, name) void module##_##name(type *me)

#define VIVID_DECLARE_EVENT_CPP(name) void name()

#define VIVID_EVENT_PUBLIC(type, module, name, vsm_member)                                                                                         \
    static const char *const m_##name##_string = #name;                                                                                            \
    void module##_##name(type *me)                                                                                                                 \
    {                                                                                                                                              \
        vivid_queue_event(me->vsm_member, m_##name##_string VIVID_PARAM_ARGS(, NULL, VIVID_PARAM_STATIC_ARGS(0U) VIVID_PARAM_DYNAMIC_ARGS(NULL))); \
    }

#define VIVID_EVENT_PRIVATE(type, name, vsm_member)                                                                                                \
    static const char *const m_##name##_string = #name;                                                                                            \
    static void name(type *me)                                                                                                                     \
    {                                                                                                                                              \
        vivid_queue_event(me->vsm_member, m_##name##_string VIVID_PARAM_ARGS(, NULL, VIVID_PARAM_STATIC_ARGS(0U) VIVID_PARAM_DYNAMIC_ARGS(NULL))); \
    }

#define VIVID_EVENT_CPP(module, name, vsm_member)                                                                                                    \
    static const char *const m_##name##_string = #name;                                                                                              \
    void module::name()                                                                                                                              \
    {                                                                                                                                                \
        vivid_queue_event(this->vsm_member, m_##name##_string VIVID_PARAM_ARGS(, NULL, VIVID_PARAM_STATIC_ARGS(0U) VIVID_PARAM_DYNAMIC_ARGS(NULL))); \
    }

#define VIVID_ON_EVENT(name, guard, target_state, action, /* json_props */...)                                                                                                   \
    if (vivid_on_event(node, /* Use VIVID_EVENT_PUBLIC(), VIVID_EVENT_PRIVATE() or VIVID_EVENT_CPP() before this macro */                                                        \
            m_##name##_string VIVID_PARAM_ARGS(, NULL VIVID_PARAM_STATIC_ARGS(, 0U)) VIVID_UML_ARGS(, state_##target_state, #guard, #target_state, #action, "" #__VA_ARGS__))) { \
        if (vivid_transit(node, guard, state_##target_state VIVID_LOG_ARGS(, "event", m_##name##_string, #guard, #target_state))) {                                              \
            action return;                                                                                                                                                       \
        }                                                                                                                                                                        \
    }

#define VIVID_ON_TIMEOUT(name, timeout, guard, target_state, action, /* json_props */...)                                                                               \
    if (vivid_on_timeout(node, #name, VIVID_CONVERT_TIME(timeout) VIVID_UML_ARGS(, state_##target_state, #timeout, #guard, #target_state, #action, "" #__VA_ARGS__))) { \
        if (vivid_transit(node, guard, state_##target_state VIVID_LOG_ARGS(, "timer", #name, #guard, #target_state))) {                                                 \
            action return;                                                                                                                                              \
        }                                                                                                                                                               \
    }

#define VIVID_JUMP(guard, target_state, action, /* json_props */...)                                                                               \
    if (vivid_jump(node VIVID_PARAM_ARGS(, NULL, NULL) VIVID_UML_ARGS(, state_##target_state, #guard, #target_state, #action, "" #__VA_ARGS__))) { \
        if (vivid_transit(node, guard, state_##target_state VIVID_LOG_ARGS(, "jump", "", #guard, #target_state))) {                                \
            action return;                                                                                                                         \
        }                                                                                                                                          \
    }

#if VIVID_PARAM
#define VIVID_DECLARE_EVENT_PARAM_PUBLIC(type, module, name, param_type) void module##_##name(type *me, const param_type *param)

#define VIVID_DECLARE_EVENT_PARAM_CPP(name, param_type) void name(const param_type &param)

#if VIVID_PARAM_DYNAMIC
#define VIVID_EVENT_PARAM_PUBLIC(type, module, name, param_type, vsm_member)            \
    typedef param_type name##_param_type_t;                                             \
    static const char *const m_##name##_string = #name;                                 \
    void module##_##name(type *me, const param_type *param)                             \
    {                                                                                   \
        vivid_binding_t *binding = vivid_get_binding(me->vsm_member);                   \
        void *new_param = binding->calloc(binding, 1U, sizeof(*param));                 \
        if (new_param == NULL) {                                                        \
            return;                                                                     \
        }                                                                               \
        memcpy(new_param, param, sizeof(*param));                                       \
        vivid_queue_event(me->vsm_member, m_##name##_string, new_param, binding->free); \
    }

#define VIVID_EVENT_PARAM_PRIVATE(type, name, param_type, vsm_member)                   \
    typedef param_type name##_param_type_t;                                             \
    static const char *const m_##name##_string = #name;                                 \
    static void name(type *me, param_type *param)                                       \
    {                                                                                   \
        vivid_binding_t *binding = vivid_get_binding(me->vsm_member);                   \
        void *new_param = binding->calloc(binding, 1U, sizeof(*param));                 \
        if (new_param == NULL) {                                                        \
            return;                                                                     \
        }                                                                               \
        memcpy(new_param, param, sizeof(*param));                                       \
        vivid_queue_event(me->vsm_member, m_##name##_string, new_param, binding->free); \
    }

#define VIVID_EVENT_PARAM_CPP(module, name, param_type, vsm_member)                              \
    typedef param_type name##_param_type_t;                                                      \
    static const char *const m_##name##_string = #name;                                          \
    static void destroy_param_##name(void *param)                                                \
    {                                                                                            \
        param_type *old_param = (param_type *)param;                                             \
        delete old_param;                                                                        \
    }                                                                                            \
    void module::name(const param_type &param)                                                   \
    {                                                                                            \
        void *new_param = new param_type(param);                                                 \
        vivid_queue_event(this->vsm_member, m_##name##_string, new_param, destroy_param_##name); \
    }
#else
#define VIVID_EVENT_PARAM_PUBLIC(type, module, name, param_type, vsm_member)             \
    typedef param_type name##_param_type_t;                                              \
    static const char *const m_##name##_string = #name;                                  \
    void module##_##name(type *me, const param_type *param)                              \
    {                                                                                    \
        vivid_queue_event(me->vsm_member, m_##name##_string, param, sizeof(param_type)); \
    }

#define VIVID_EVENT_PARAM_PRIVATE(type, name, param_type, vsm_member)                    \
    typedef param_type name##_param_type_t;                                              \
    static const char *const m_##name##_string = #name;                                  \
    static void name(type *me, const param_type *param)                                  \
    {                                                                                    \
        vivid_queue_event(me->vsm_member, m_##name##_string, param, sizeof(param_type)); \
    }

#define VIVID_EVENT_PARAM_CPP(module, name, param_type, vsm_member)                         \
    typedef param_type name##_param_type_t;                                                 \
    static const char *const m_##name##_string = #name;                                     \
    void module::name(const param_type &param)                                              \
    {                                                                                       \
        vivid_queue_event(this->vsm_member, m_##name##_string, &param, sizeof(param_type)); \
    }
#endif

#define VIVID_ON_EVENT_PARAM(name, guard, target_state, action, /* json_props */...)                                                                                                                                         \
    {                                                                                                                                                                                                                        \
        /* Use VIVID_EVENT_PARAM_PUBLIC(), VIVID_EVENT_PARAM_PRIVATE() or VIVID_EVENT_PARAM_CPP() before this macro */ const name##_param_type_t *param;                                                                     \
        if (vivid_on_event(node, m_##name##_string, (const void **)&param VIVID_PARAM_STATIC_ARGS(, sizeof(name##_param_type_t)) VIVID_UML_ARGS(, state_##target_state, #guard, #target_state, #action, "" #__VA_ARGS__))) { \
            if (vivid_transit(node, guard, state_##target_state VIVID_LOG_ARGS(, "event", m_##name##_string, #guard, #target_state))) {                                                                                      \
                action return;                                                                                                                                                                                               \
            }                                                                                                                                                                                                                \
        }                                                                                                                                                                                                                    \
    }

#define VIVID_JUMP_PARAM(event_name, guard, target_state, action, /* json_props */...)                                                                                  \
    {                                                                                                                                                                   \
        /* Use VIVID_EVENT_PARAM_PUBLIC(), VIVID_EVENT_PARAM_PRIVATE() or VIVID_EVENT_PARAM_CPP() before this macro */ const event_name##_param_type_t *param;          \
        if (vivid_jump(node, (const void **)&param, m_##event_name##_string VIVID_UML_ARGS(, state_##target_state, #guard, #target_state, #action, "" #__VA_ARGS__))) { \
            if (vivid_transit(node, guard, state_##target_state VIVID_LOG_ARGS(, "jump", "", #guard, #target_state))) {                                                 \
                action return;                                                                                                                                          \
            }                                                                                                                                                           \
        }                                                                                                                                                               \
    }
#endif

#define IS_IN(vsm, state) vivid_is_in(vsm, /* Use VIVID_DECLARE_STATE() or VIVID_DECLARE_STATE_CPP() before this macro */ state_##state)

#define VIVID_DECLARE_IS_IN_PUBLIC(type, module, state) bool module##_is_in_##state(type *me)

#define VIVID_DECLARE_IS_IN_CPP(state) bool is_in_##state()

#define VIVID_IS_IN_PUBLIC(type, module, state, vsm_member) \
    bool module##_is_in_##state(type *me) { return IS_IN(me->vsm_member, state); }

#define VIVID_IS_IN_CPP(module, state, vsm_member) \
    bool module::is_in_##state() { return IS_IN(this->vsm_member, state); }

#define state_NULL NULL

#define VIVID_NO_ACTION

typedef struct vivid_sm vivid_sm_t;
typedef struct vivid_node vivid_node_t;

typedef enum {
    VIVID_NODE_TYPE_ROOT,
    VIVID_NODE_TYPE_STATE,
    VIVID_NODE_TYPE_STATE_FINAL,
    VIVID_NODE_TYPE_STATE_PARALLEL,
    VIVID_NODE_TYPE_CONDITION,
    VIVID_NODE_TYPE_JUNCTION
} vivid_node_type_t;

typedef void (*vivid_state_t)(vivid_node_t *node, void *app);
typedef void (*vivid_state_change_callback_t)(void *app);

vivid_sm_t *vivid_create_sm(vivid_binding_t *binding, vivid_state_t root_fn, size_t event_queue_size, void *app VIVID_LOG_ARGS(, const char *name, const char *root_name));

void vivid_destroy_sm(vivid_sm_t *me);

vivid_binding_t *vivid_get_binding(vivid_sm_t *me);

void vivid_set_state_change_callback(vivid_sm_t *me, vivid_state_change_callback_t callback);

void vivid_sub_node(vivid_node_t *node, vivid_state_t fn, vivid_node_type_t type VIVID_LOG_ARGS(, const char *name VIVID_UML_ARGS(, const char *json_props)));

bool vivid_default(vivid_node_t *node, vivid_state_t fn VIVID_LOG_ARGS(, const char *name VIVID_UML_ARGS(, const char *action_text, const char *json_props)));

bool vivid_on_entry(vivid_node_t *node VIVID_UML_ARGS(, const char *action_text));

bool vivid_on_exit(vivid_node_t *node VIVID_UML_ARGS(, const char *action_text));

bool vivid_on_event(vivid_node_t *node, const char *name VIVID_PARAM_ARGS(, const void **param VIVID_PARAM_STATIC_ARGS(, size_t param_size)) VIVID_UML_ARGS(, vivid_state_t target_fn, const char *guard_text, const char *target_name, const char *action_text, const char *json_props));

bool vivid_on_timeout(vivid_node_t *node, const char *name, vivid_time_t timeout VIVID_UML_ARGS(, vivid_state_t target_fn, const char *timeout_text, const char *guard_text, const char *target_name, const char *action_text, const char *json_props));

bool vivid_jump(vivid_node_t *node VIVID_PARAM_ARGS(, const void **param, const char *param_event_name) VIVID_UML_ARGS(, vivid_state_t target_fn, const char *guard_text, const char *target_name, const char *action_text, const char *json_props));

bool vivid_transit(vivid_node_t *node, bool guard, vivid_state_t target_fn VIVID_LOG_ARGS(, const char *type, const char *name, const char *guard_text, const char *target_name));

void vivid_queue_event(vivid_sm_t *me, const char *name VIVID_PARAM_ARGS(, VIVID_PARAM_STATIC_ARGS(const) void *param, VIVID_PARAM_STATIC_ARGS(size_t param_size) VIVID_PARAM_DYNAMIC_ARGS(vivid_param_destructor_t param_destructor)));

bool vivid_is_in(vivid_sm_t *me, vivid_state_t state);

vivid_state_t vivid_get_state(vivid_sm_t *me, vivid_state_t parent_state VIVID_LOG_ARGS(, const char **name));

#if !VIVID_UML
#define vivid_save_uml(me, filename)
#else
void vivid_save_uml(vivid_sm_t *me, const char *filename);
#endif

#ifdef __cplusplus
}
#endif

#endif
