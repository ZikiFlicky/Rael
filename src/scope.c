#include "scope.h"

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

void scope_dealloc(struct Scope* const scope) {
    for (size_t i = 0; i < scope->variables.allocated; ++i) {
        struct BucketNode *next_node;
        for (struct BucketNode *node = scope->variables.buckets[i]; node; node = next_node) {
            next_node = node->next;
            value_dereference(node->value);
            free(node);
        }
    }
    free(scope->variables.buckets);
}



void scope_set(struct Scope* const scope, char *key, RaelValue value) {
    struct BucketNode *node;
    struct BucketNode *last_top_scope_node;
    size_t hash_result;

    last_top_scope_node = NULL;

    for (struct Scope *sc = scope; sc; sc = sc->parent) {
        if (sc->variables.allocated > 0) {
            hash_result = scope_hash(sc, key);
            for (node = sc->variables.buckets[hash_result]; node; node = node->next) {
                if (strcmp(node->key, key) == 0) {
                    // dereference current value at position if already exists at key
                    value_dereference(node->value);
                    node->key = key;
                    node->value = value;
                    return;
                }

                if (sc == scope)
                    last_top_scope_node = node;
            }
        }
    }

    if (scope->variables.allocated == 0) {
        scope->variables.buckets = calloc((scope->variables.allocated = 8), sizeof(struct BucketNode *));
    }

    node = malloc(sizeof(struct BucketNode));

    // set node
    node->key = key;
    node->value = value;
    node->next = NULL;

    // add node
    if (!last_top_scope_node)
        scope->variables.buckets[scope_hash(scope, key)] = node;
    else
        last_top_scope_node->next = node;

    // increment counter (not used yet)
    ++scope->variables.pairs;
}

void scope_set_local(struct Scope *scope, char* const key, RaelValue value) {
    struct Scope *parent;

    parent = scope->parent;
    scope->parent = NULL;
    scope_set(scope, key, value);
    scope->parent = parent;
}

RaelValue scope_get(struct Scope *scope, char* const key) {
    for (; scope; scope = scope->parent) {
        if (scope->variables.allocated == 0)
            continue;
        // iterate bucket nodes
        for (struct BucketNode *node = scope->variables.buckets[scope_hash(scope, key)]; node; node = node->next) {
            if (strcmp(key, node->key) == 0) {
                ++node->value->reference_count;
                return node->value;
            }
        }
    }

    return value_create(ValueTypeVoid);
}
