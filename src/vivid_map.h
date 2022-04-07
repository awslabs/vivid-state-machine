// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#ifndef VIVID_MAP_H
#define VIVID_MAP_H

#include <vivid/binding.h>

typedef struct vivid_map vivid_map_t;

typedef void (*vivid_map_iterate_callback_t)(void *app, size_t key, void *value);

vivid_map_t *vivid_map_create(vivid_binding_t *binding);

void vivid_map_destroy(vivid_map_t *me);

void **vivid_map_set(vivid_map_t *me, size_t key);

void *vivid_map_get(const vivid_map_t *me, size_t key);

void vivid_map_iterate(const vivid_map_t *me, vivid_map_iterate_callback_t callback, void *app);

#endif
