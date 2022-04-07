#include "../application.h"
#include "callee.h"
#include "caller.h"
#include <stdlib.h>

typedef struct
{
    vivid_binding_t *binding;
    caller_t caller_a;
    caller_t caller_b;
    callee_t callee;
} application_t;

void application_destroy(void *application)
{
    application_t *me = (application_t *)application;
    if (me == NULL) {
        return;
    }
    caller_deinit(&me->caller_a);
    caller_deinit(&me->caller_b);
    callee_deinit(&me->callee);
    me->binding->free(me);
}

void *application_create(vivid_binding_t *binding)
{
    application_t *me = (application_t *)binding->calloc(binding, 1U, sizeof(*me));
    if (me == NULL) {
        return NULL;
    }
    me->binding = binding;
    if (!callee_init(&me->callee, binding)
        || !caller_init(&me->caller_a, binding, "caller_a", &me->callee)
        || !caller_init(&me->caller_b, binding, "caller_b", &me->callee)) {
        application_destroy(me);
        return NULL;
    }
    return me;
}

void application_trigger(void *application, const char *value, size_t len)
{
    application_t *me = (application_t *)application;
    (void)me;
}
