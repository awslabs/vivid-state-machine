#ifndef CALLER_H
#define CALLER_H

#include "callee.h"
#include <stdbool.h>
#include <vivid/sm.h>

typedef struct {
    vivid_sm_t *vsm;
    const char *name;
    callee_t *callee;
    int foo;
} caller_t;

bool caller_init(caller_t *me, vivid_binding_t *binding, const char *name, callee_t *callee);

void caller_deinit(caller_t *me);

#endif
