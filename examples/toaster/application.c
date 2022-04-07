#include "../application.h"
#include "toaster.h"

typedef struct
{
    vivid_binding_t *binding;
    toaster_t toaster;
} application_t;

void application_destroy(void *application)
{
    application_t *me = (application_t *)application;
    if (me == NULL) {
        return;
    }
    toaster_deinit(&me->toaster);
    me->binding->free(me);
}

void *application_create(vivid_binding_t *binding)
{
    application_t *me = binding->calloc(binding, 1U, sizeof(*me));
    if (me == NULL) {
        return NULL;
    }
    me->binding = binding;
    if (!toaster_init(&me->toaster, binding)) {
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
    toaster_ev_button_press(&me->toaster);
}
