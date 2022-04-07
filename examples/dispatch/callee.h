#ifndef CALLEE_H
#define CALLEE_H

#include <stdbool.h>
#include <vivid/sm.h>
#include <vivid/util/queue.h>

typedef struct {
    int bar;
} callee_response_t;

typedef void (*callee_callback_t)(void *app, const callee_response_t *response);

typedef struct {
    int foo;
    callee_callback_t callback;
    void *app;
} callee_request_t;

typedef struct {
    vivid_sm_t *vsm;
    vivid_queue_t *dispatch_queue;
    vivid_binding_t *binding;
} callee_t;

bool callee_init(callee_t *me, vivid_binding_t *binding);

void callee_deinit(callee_t *me);

void callee_dispatch(callee_t *me, const char *caller_name, const callee_request_t *request);

#endif
