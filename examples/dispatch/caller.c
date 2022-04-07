#include "caller.h"
#include <stdio.h>

VIVID_EVENT_PARAM_PRIVATE(caller_t, ev_response, callee_response_t, vsm);

static void request(caller_t *me)
{
    printf("%s | dispatching request with foo=%d\n", me->name, me->foo);
    callee_request_t req;
    req.foo = me->foo;
    req.callback = (callee_callback_t)ev_response;
    req.app = me;
    callee_dispatch(me->callee, me->name, &req);
}

static void process(caller_t *me, const callee_response_t *response)
{
    printf("%s | processing response with bar=%d\n", me->name, response->bar);
    me->foo++;
}

VIVID_DECLARE_STATE(root);
VIVID_DECLARE_STATE(waiting);

VIVID_STATE(caller_t, root)
{
    VIVID_SUB_STATE(waiting);
    VIVID_DEFAULT(waiting, request(me););
}

VIVID_STATE(caller_t, waiting)
{
    VIVID_ON_EVENT_PARAM(ev_response, true, waiting, process(me, param); request(me););
}

bool caller_init(caller_t *me, vivid_binding_t *binding, const char *name, callee_t *callee)
{
    me->name = name;
    me->callee = callee;
    me->vsm = VIVID_CREATE_SM(binding, name, root, 16U, me);
    if (me->vsm == NULL) {
        return false;
    }
    vivid_save_uml(me->vsm, "caller.puml");
    return true;
}

void caller_deinit(caller_t *me)
{
    vivid_destroy_sm(me->vsm);
}
