// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0.

#include "vivid_priv.h"

#if VIVID_UML
#include <stdio.h>
#include <string.h>

#define JSMN_STATIC
#define JSMN_PARENT_LINKS
#define JSMN_STRICT
#include <jsmn.h>

#define MAX_JSON_TOKENS 16U
#define ROOT_JSON_TOKEN_INDEX 0
#define INDENT_WIDTH 2U

#define NO_ACTION_STRING(x) x #x

struct vivid_uml {
    vivid_binding_t *binding;
    vivid_log_t *log;
    jsmn_parser json_parser;
    jsmntok_t json_tokens[MAX_JSON_TOKENS];
    FILE *fp;
    int indent;
};

static const char *const m_uml_string = "uml";
static const char *const m_uml_default_string = "uml_default";
static const char *const m_true_string = "true";
static const char *const m_dir_string = "dir";
static const char *const m_note_string = "note";
static const char *const m_pos_string = "pos";
static const char *const m_sep_string = "sep";
static const char *const m_no_action_string = NO_ACTION_STRING(VIVID_NO_ACTION);

static const char *get_json_prop(vivid_uml_t *me, const char *json, const char *prop_name, int *value_len)
{
    *value_len = 0;
    if (json == NULL) {
        return NULL;
    }
    jsmn_init(&me->json_parser);
    int num_json_tokens = jsmn_parse(&me->json_parser, json, strlen(json), me->json_tokens, MAX_JSON_TOKENS);
    if (num_json_tokens == JSMN_ERROR_NOMEM) {
        VIVID_LOG_ERROR(me->log, "too many json tokens: %s", json);
        return NULL;
    }
    if (num_json_tokens < 0) {
        VIVID_LOG_ERROR(me->log, "invalid json: %s", json);
        return NULL;
    }
    if ((num_json_tokens < 2) || (me->json_tokens[ROOT_JSON_TOKEN_INDEX].type != JSMN_OBJECT)) {
        return NULL;
    }
    int index = ROOT_JSON_TOKEN_INDEX + 1;
    int prop_len = (int)strlen(prop_name);
    for (int i = 0U; (i < me->json_tokens[ROOT_JSON_TOKEN_INDEX].size) && (index < (num_json_tokens - 1)); i++) {
        const jsmntok_t *token = &me->json_tokens[index];
        const char *member_name = &json[token->start];
        int member_len = token->end - token->start;
        if ((member_len == prop_len) && (strncmp(member_name, prop_name, member_len) == 0)) {
            token = &me->json_tokens[index + 1];
            if (token->type != JSMN_STRING) {
                return NULL;
            }
            *value_len = token->end - token->start;
            return &json[token->start];
        }
        do {
            index++;
        } while ((index < (num_json_tokens - 1)) && (me->json_tokens[index].parent > ROOT_JSON_TOKEN_INDEX));
    }
    return NULL;
}

vivid_uml_t *vivid_uml_create(vivid_binding_t *binding, vivid_log_t *log)
{
    vivid_uml_t *me = (vivid_uml_t *)binding->calloc(binding, 1U, sizeof(*me));
    if (me == NULL) {
        return NULL;
    }
    me->binding = binding;
    me->log = log;
    return me;
}

void vivid_uml_destroy(vivid_uml_t *me)
{
    if (me == NULL) {
        return;
    }
    me->binding->free(me);
}

static bool is_action(const char *action_text)
{
    return (*action_text != '\0') && (strcmp(action_text, m_no_action_string) != 0);
}

static void add_code(FILE *fp, const char *code, bool add_newlines)
{
    bool in_escape = false;
    bool in_string = false;
    bool in_char = false;
    while (*code != '\0') {
        // Search for one of these characters: \;"'
        const char *ch = strpbrk(code, "\\;\"'");
        if (ch == NULL) {
            fputs(code, fp);
            break;
        }
        fwrite(code, sizeof(char), ch - code, fp);
        if (ch != code) {
            in_escape = false;
        }
        code = ch + 1;
        if (*ch == '\\') {
            // Escape backslashes:
            fputs("\\\\", fp);
            in_escape = !in_escape;
        } else {
            fputc(*ch, fp);
            switch (*ch) {
            case ';':
                // Add newlines after semicolons, except when in a string or character literal:
                if (add_newlines && !in_string && !in_char) {
                    if (*code != '\0') {
                        fputs("\\n", fp);
                    }
                    if (*code == ' ') {
                        code++;
                    }
                }
                break;
            case '"':
                if (!in_char && (!in_string || !in_escape)) {
                    in_string = !in_string;
                }
                break;
            default: // '\''
                if (!in_string && (!in_char || !in_escape)) {
                    in_char = !in_char;
                }
                break;
            }
            in_escape = false;
        }
    }
}

void vivid_uml_on_entry_or_exit(vivid_node_t *node, const char *dir, const char *action_text)
{
    if ((node->current_event->name != m_uml_string) || !is_action(action_text)) {
        return;
    }
    FILE *fp = node->vsm->uml->fp;
    fprintf(fp, "%*s%s : %s/", node->vsm->uml->indent * INDENT_WIDTH, "", node->name, dir);
    add_code(fp, action_text, false);
    fputc('\n', fp);
}

static void add_stereotype(vivid_node_t *node)
{
    const char *stereotype;
    if (node->type == VIVID_NODE_TYPE_CONDITION) {
        stereotype = "choice";
    } else if ((node->type == VIVID_NODE_TYPE_JUNCTION) || (node->type == VIVID_NODE_TYPE_STATE_FINAL)) {
        stereotype = "end";
    } else {
        return;
    }
    fprintf(node->vsm->uml->fp, "%*sstate %s <<%s>>\n", node->vsm->uml->indent * INDENT_WIDTH, "", node->name, stereotype);
}

void vivid_uml_on_transition(vivid_node_t *node, vivid_transition_type_t type, const char *name, const char *timeout_text, const char *guard_text, const char *target_name, vivid_state_t target_fn, const char *action_text, const char *json_props)
{
    if ((type == VIVID_TRANSITION_TYPE_DEFAULT)
            ? (node->current_event->name != m_uml_default_string)
            : (node->current_event->name != m_uml_string)) {
        return;
    }
    vivid_uml_t *me = node->vsm->uml;
    FILE *fp = me->fp;
    vivid_node_t *target_node = NULL;
    if (target_fn != NULL) {
        target_node = (vivid_node_t *)vivid_map_get(node->vsm->node_map, (size_t)target_fn);
    }
    add_stereotype(node);
    if (target_node == NULL) {
        fprintf(fp, "%*s%s", me->indent * INDENT_WIDTH, "", node->name);
    } else {
        add_stereotype(target_node);
        int dir_len;
        const char *dir = get_json_prop(me, json_props, m_dir_string, &dir_len);
        fprintf(fp, "%*s%s -%.*s-> %s", me->indent * INDENT_WIDTH, "", (type == VIVID_TRANSITION_TYPE_DEFAULT) ? "[*]" : node->name, dir_len, dir, target_node->name);
    }
    bool is_guard_true = strcmp(guard_text, m_true_string) == 0;
    bool is_condition = node->type == VIVID_NODE_TYPE_CONDITION;
    switch (type) {
    case VIVID_TRANSITION_TYPE_DEFAULT:
    case VIVID_TRANSITION_TYPE_JUMP:
        if (is_condition || !is_guard_true || is_action(action_text)) {
            fputs(" : ", fp);
        }
        break;
    case VIVID_TRANSITION_TYPE_EVENT:
        fprintf(fp, " : %s", name);
        break;
    case VIVID_TRANSITION_TYPE_TIMEOUT:
        fprintf(fp, " : %s(", name);
        add_code(fp, timeout_text, false);
        fputc(')', fp);
        break;
    default:
        VIVID_LOG_ERROR(node->vsm->log, "%s | unknown uml transition type: %d", node->vsm->name, type);
        break;
    }
    if (is_condition || !is_guard_true) {
        fputc('[', fp);
        add_code(fp, (is_condition && is_guard_true) ? "else" : guard_text, false);
        fputc(']', fp);
    }
    if (is_action(action_text)) {
        fputc('/', fp);
        if ((type == VIVID_TRANSITION_TYPE_EVENT) || (type == VIVID_TRANSITION_TYPE_TIMEOUT) || is_condition || !is_guard_true) {
            fputs("\\n", fp);
        }
        add_code(fp, action_text, true);
    }
    fputc('\n', fp);
    int note_len;
    const char *note = get_json_prop(me, json_props, m_note_string, &note_len);
    if (note != NULL) {
        int pos_len;
        const char *pos = get_json_prop(me, json_props, m_pos_string, &pos_len);
        fprintf(fp, "note %.*s on link\n%.*s\nend note\n", pos_len, pos, note_len, note);
    }
}

static void walk_uml(vivid_node_t *node, int indent)
{
    vivid_uml_t *me = node->vsm->uml;
    FILE *fp = me->fp;
    int note_len;
    const char *note = get_json_prop(me, node->json_props, m_note_string, &note_len);
    if (note != NULL) {
        int pos_len;
        const char *pos = get_json_prop(me, node->json_props, m_pos_string, &pos_len);
        if (pos != NULL) {
            fprintf(fp, "note %.*s of %s\n%.*s\nend note\n", pos_len, pos, node->name, note_len, note);
        } else {
            fprintf(fp, "note as note_%s\n%.*s\nend note\n", node->name, note_len, note);
        }
    }
    me->indent = indent;
    vivid_call_node(node, m_uml_string);
    if (node->siblings != NULL) {
        walk_uml(node->siblings, indent);
    }
    if (node->children != NULL) {
        bool is_root = node->parent == NULL;
        bool parallel_siblings = !is_root && node->parent->parallel_children;
        if (parallel_siblings) {
            if (node->siblings != NULL) {
                int sep_len;
                const char *sep = get_json_prop(me, node->parent->json_props, m_sep_string, &sep_len);
                if (sep == NULL) {
                    sep = "||";
                    sep_len = 2;
                }
                fprintf(fp, "%*s%*s\n", indent * INDENT_WIDTH, "", sep_len, sep);
            }
        } else if (!is_root) {
            fprintf(fp, "%*sstate %s {\n", indent * INDENT_WIDTH, "", node->name);
        }
        int child_indent = indent + ((parallel_siblings || is_root) ? 0U : 1U);
        me->indent = child_indent;
        vivid_call_node(node, m_uml_default_string);
        walk_uml(node->children, child_indent);
        if (!parallel_siblings && !is_root) {
            fprintf(fp, "%*s}\n", indent * INDENT_WIDTH, "");
        }
    }
}

void vivid_save_uml(vivid_sm_t *me, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        VIVID_LOG_ERROR(me->log, "%s | could not open %s for writing", me->name, filename);
        return;
    }
    me->uml->fp = fp;
    VIVID_LOG_INFO(me->log, "%s | writing to %s...", me->name, filename);
    fputs("@startuml\n", fp);
    walk_uml(me->root_node, 0U);
    fputs("@enduml\n", fp);
    fclose(fp);
}
#endif
