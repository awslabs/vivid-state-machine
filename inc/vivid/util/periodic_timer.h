// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#ifndef VIVID_PERIODIC_TIMER_H
#define VIVID_PERIODIC_TIMER_H

#include <vivid/binding.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vivid_periodic_timer vivid_periodic_timer_t;

vivid_periodic_timer_t *vivid_periodic_timer_create(vivid_binding_t *binding, vivid_binding_callback_t callback, void *data);

void vivid_periodic_timer_destroy(vivid_periodic_timer_t *me);

void vivid_periodic_timer_start(vivid_periodic_timer_t *me, vivid_time_t timeout);

void vivid_periodic_timer_stop(vivid_periodic_timer_t *me);

#ifdef __cplusplus
}
#endif

#endif
