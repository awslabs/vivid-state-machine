#include "../application.h"
#include "countdown.h"

typedef struct
{
    vivid_binding_t *binding;
    countdown_t countdown;
} application_t;

void application_destroy(void *application)
{
    application_t *me = (application_t *)application;
    if (me == NULL) {
        return;
    }
    countdown_deinit(&me->countdown);
    me->binding->free(me);
}

void *application_create(vivid_binding_t *binding)
{
    application_t *me = binding->calloc(binding, 1U, sizeof(*me));
    if (me == NULL) {
        return NULL;
    }
    me->binding = binding;
    if (!countdown_init(&me->countdown, binding)) {
        application_destroy(me);
        return NULL;
    }
    return me;
}

void application_trigger(void *application, const char *value, size_t len)
{
    application_t *me = (application_t *)application;
    (void)value;
    (void)len;
    countdown_ev_start(&me->countdown);
}
