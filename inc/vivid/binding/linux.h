// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#ifndef VIVID_BINDING_LINUX_H
#define VIVID_BINDING_LINUX_H

#include <vivid/binding.h>

#ifdef __cplusplus
extern "C" {
#endif

vivid_binding_t *vivid_binding_linux_create(int *fd VIVID_LOG_ARGS(, vivid_binding_log_callback_t log_callback, void *logger));

void vivid_binding_linux_destroy(vivid_binding_t *binding);

void vivid_binding_linux_handle_event(vivid_binding_t *binding);

#ifdef __cplusplus
}
#endif

#endif
