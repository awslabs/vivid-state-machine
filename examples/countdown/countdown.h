#ifndef COUNTDOWN_H
#define COUNTDOWN_H

#include <stdbool.h>
#include <vivid/sm.h>

typedef struct {
    vivid_sm_t *vsm;
    int counter;
} countdown_t;

bool countdown_init(countdown_t *me, vivid_binding_t *binding);

void countdown_deinit(countdown_t *me);

VIVID_DECLARE_EVENT_PUBLIC(countdown_t, countdown, ev_start);

#endif
