#ifndef TOASTER_H
#define TOASTER_H

#include <stdbool.h>
#include <vivid/sm.h>

typedef struct {
    vivid_sm_t *vsm;
} toaster_t;

bool toaster_init(toaster_t *me, vivid_binding_t *binding);

void toaster_deinit(toaster_t *me);

VIVID_DECLARE_EVENT_PUBLIC(toaster_t, toaster, ev_button_press);

#endif
