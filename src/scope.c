#include "scope.h"
#include "interpreter.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static size_t scope_hash(struct Scope* const scope, char* const to_hash) {
    size_t value = 0;
    char *cur_idx;

    for (cur_idx = to_hash; *cur_idx; ++cur_idx) {
        value *= 256;
        value += *cur_idx;
        value %= scope->variables.allocated;
    }

    return value;
}

void scope_construct(struct Scope* const scope, struct Scope* const parent_scope) {
    scope->parent = parent_scope;
    scope->variables.buckets = NULL;
    scope->variables.allocated = 0;
    scope->variables.pairs = 0;
}

/* FIXME: optimise this */
static void node_delete(struct BucketNode *node) {
    if (!node)
        return;
    free(node->key);
    node_delete(node->next);
    free(node);
}

void scope_dealloc(struct Scope* const scope) {
    size_t i;
    for (i = 0; i < scope->variables.allocated; ++i)
        node_delete(scope->variables.buckets[i]);
}

bool scope_set(struct Scope* const scope, char *key, struct Value value) {
    struct BucketNode *node;
    struct BucketNode *previous_node;
    size_t hash_result;

    if (scope->variables.allocated == 0) {
        scope->variables.buckets = calloc(8, sizeof(struct BucketNode *));
        if (!scope->variables.buckets)
            return false;
        scope->variables.allocated = 8;
    }

    hash_result = scope_hash(scope, key);
    previous_node = NULL;
    for (node = scope->variables.buckets[hash_result]; node; node = node->next) {
        if (!node->key || strcmp(node->key, key) == 0) {
            /* deallocate value if already exists at key */
            // value_dealloc(&node->value);
            node->key = key;
            node->value = value;
            return true;
        }
        previous_node = node;

    }
    if (!(node = malloc(sizeof(struct BucketNode))))
        return false;

    node->key = key;
    node->value = value;
    node->next = NULL;

    if (!previous_node)
        scope->variables.buckets[hash_result] = node;
    else
        previous_node->next = node;

    ++scope->variables.pairs;
    return true;
}

struct Value scope_get(struct Scope* const scope, char* const key) {
    struct BucketNode *node;
    struct Scope *search_scope;

    for (search_scope = scope; search_scope; search_scope = search_scope->parent) {
        if (search_scope->variables.allocated == 0)
            continue;
        for (node = scope->variables.buckets[scope_hash(scope, key)]; node; node = node->next) {
            if (strcmp(key, node->key) == 0)
                return node->value;
        }
    }
    /* TODO: I should definitely decide if this is a good idea */
    return (struct Value) {
        .type = ValueTypeVoid
    };
}