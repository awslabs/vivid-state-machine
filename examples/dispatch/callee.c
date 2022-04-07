#include "callee.h"
#include <stdio.h>
#include <string.h>

static void process(callee_t *me)
{
    const vivid_queue_entry_t *entry = vivid_queue_front(me->dispatch_queue);
    const callee_request_t *request = (const callee_request_t *)entry->param;
    printf("callee | processing request from %s with argument foo=%d\n", entry->name, request->foo);
    callee_response_t response;
    response.bar = request->foo * 2;
    request->callback(request->app, &response);
    vivid_queue_pop(me->dispatch_queue);
}

static bool is_empty(callee_t *me)
{
    return vivid_queue_empty(me->dispatch_queue);
}

VIVID_DECLARE_STATE(root);
VIVID_DECLARE_STATE(idle);
VIVID_DECLARE_STATE(cond_empty);
VIVID_DECLARE_STATE(processing);

VIVID_EVENT_PRIVATE(callee_t, ev_dispatch, vsm);

VIVID_STATE(callee_t, root)
{
    VIVID_SUB_STATE(idle);
    VIVID_DEFAULT(idle, VIVID_NO_ACTION);
    VIVID_SUB_STATE(processing);
    VIVID_SUB_CONDITION(cond_empty);
}

VIVID_STATE(callee_t, idle)
{
    VIVID_ON_EVENT(ev_dispatch, true, cond_empty, VIVID_NO_ACTION);
}

VIVID_STATE(callee_t, cond_empty)
{
    VIVID_JUMP(is_empty(me), idle, VIVID_NO_ACTION);
    VIVID_JUMP(true, processing, process(me););
}

VIVID_STATE(callee_t, processing)
{
    VIVID_ON_TIMEOUT(tm_process, 1.0, true, cond_empty, VIVID_NO_ACTION);
}

bool callee_init(callee_t *me, vivid_binding_t *binding)
{
    me->binding = binding;
    me->dispatch_queue = vivid_queue_create(binding, 2U VIVID_PARAM_STATIC_ARGS(, sizeof(callee_request_t)));
    me->vsm = VIVID_CREATE_SM(binding, "callee", root, 16U, me);
    if ((me->dispatch_queue == NULL) || (me->vsm == NULL)) {
        return false;
    }
    vivid_save_uml(me->vsm, "callee.puml");
    return true;
}

void callee_deinit(callee_t *me)
{
    vivid_destroy_sm(me->vsm);
    vivid_queue_destroy(me->dispatch_queue);
}

void callee_dispatch(callee_t *me, const char *caller_name, const callee_request_t *request)
{
#if VIVID_PARAM_DYNAMIC
    void *new_request = me->binding->calloc(me->binding, 1U, sizeof(*request));
    if (new_request == NULL) {
        return;
    }
    memcpy(new_request, request, sizeof(*request));
    vivid_queue_push(me->dispatch_queue, caller_name, new_request, me->binding->free);
#else
    vivid_queue_push(me->dispatch_queue, caller_name, request, sizeof(*request));
#endif
    ev_dispatch(me);
}
