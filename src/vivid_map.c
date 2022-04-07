// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#include "vivid_map.h"

typedef enum {
    DIR_LEFT,
    DIR_RIGHT,
    DIR_SIZE
} dir_t;

typedef enum {
    COLOR_RED,
    COLOR_BLACK
} color_t;

typedef struct node {
    size_t key;
    void *value;
    struct node *parent;
    struct node *children[DIR_SIZE];
    color_t color;
} node_t;

struct vivid_map {
    vivid_binding_t *binding;
    node_t *root;
};

vivid_map_t *vivid_map_create(vivid_binding_t *binding)
{
    vivid_map_t *me = (vivid_map_t *)binding->calloc(binding, 1U, sizeof(*me));
    if (me == NULL) {
        return NULL;
    }
    me->binding = binding;
    return me;
}

static void destroy(vivid_binding_t *binding, node_t *node)
{
    if (node == NULL) {
        return;
    }
    destroy(binding, node->children[DIR_LEFT]);
    destroy(binding, node->children[DIR_RIGHT]);
    binding->free(node);
}

void vivid_map_destroy(vivid_map_t *me)
{
    if (me == NULL) {
        return;
    }
    destroy(me->binding, me->root);
    me->binding->free(me);
}

static void rotate(vivid_map_t *me, node_t *node, dir_t dir)
{
    node_t *temp_node = node->children[!dir];
    node->children[!dir] = temp_node->children[dir];
    if (temp_node->children[dir] != NULL) {
        temp_node->children[dir]->parent = node;
    }
    temp_node->parent = node->parent;
    if (node->parent == NULL) {
        me->root = temp_node;
    } else if (node == node->parent->children[dir]) {
        node->parent->children[dir] = temp_node;
    } else {
        node->parent->children[!dir] = temp_node;
    }
    temp_node->children[dir] = node;
    node->parent = temp_node;
}

static void fixup(vivid_map_t *me, node_t *node)
{
    while ((node != me->root) && (node->parent->color == COLOR_RED)) {
        for (int dir = 0; dir < DIR_SIZE; dir++) {
            if (node->parent == node->parent->parent->children[dir]) {
                node_t *uncle = node->parent->parent->children[!dir];
                if ((uncle != NULL) && uncle->color == COLOR_RED) {
                    node->parent->color = COLOR_BLACK;
                    uncle->color = COLOR_BLACK;
                    node->parent->parent->color = COLOR_RED;
                    node = node->parent->parent;
                } else {
                    if (node == node->parent->children[!dir]) {
                        node = node->parent;
                        rotate(me, node, dir);
                    }
                    node->parent->color = COLOR_BLACK;
                    node->parent->parent->color = COLOR_RED;
                    rotate(me, node->parent->parent, !dir);
                }
                break;
            }
        }
    }
    me->root->color = COLOR_BLACK;
}

void **vivid_map_set(vivid_map_t *me, size_t key)
{
    node_t *node = me->root;
    node_t *parent = NULL;
    while ((node != NULL) && (node->key != key)) {
        parent = node;
        node = node->children[node->key < key];
    }
    if (node != NULL) {
        return &node->value;
    }
    node = (node_t *)me->binding->calloc(me->binding, 1U, sizeof(*node));
    if (node == NULL) {
        return NULL;
    }
    node->key = key;
    node->parent = parent;
    if (parent == NULL) {
        me->root = node;
    } else {
        parent->children[parent->key < key] = node;
    }
    fixup(me, node);
    return &node->value;
}

void *vivid_map_get(const vivid_map_t *me, size_t key)
{
    node_t *node = me->root;
    while ((node != NULL) && (node->key != key)) {
        node = node->children[node->key < key];
    }
    if (node == NULL) {
        return NULL;
    }
    return node->value;
}

static void iterate(node_t *node, vivid_map_iterate_callback_t callback, void *app)
{
    if (node == NULL) {
        return;
    }
    iterate(node->children[DIR_LEFT], callback, app);
    callback(app, node->key, node->value);
    iterate(node->children[DIR_RIGHT], callback, app);
}

void vivid_map_iterate(const vivid_map_t *me, vivid_map_iterate_callback_t callback, void *app)
{
    if (me == NULL) {
        return;
    }
    iterate(me->root, callback, app);
}
