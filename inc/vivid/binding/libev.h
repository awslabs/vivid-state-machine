// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#ifndef VIVID_BINDING_LIBEV_H
#define VIVID_BINDING_LIBEV_H

#include <ev.h>
#include <vivid/binding.h>

#ifdef __cplusplus
extern "C" {
#endif

vivid_binding_t *vivid_binding_libev_create(struct ev_loop *loop VIVID_LOG_ARGS(, vivid_binding_log_callback_t log_callback, void *logger));

void vivid_binding_libev_destroy(vivid_binding_t *binding);

#ifdef __cplusplus
}
#endif

#endif
