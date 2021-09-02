#ifndef RAEL_SCOPE_H
#define RAEL_SCOPE_H

#include "parser.h"

#include <stddef.h>
# include <stdbool.h>

struct VariableMap {
    struct BucketNode {
        struct BucketNode *next;
        char *key;
        struct Value value;
    } **buckets;
    size_t allocated, pairs;
};

struct Scope {
    struct VariableMap variables;
    struct Scope *parent;
};

void scope_construct(struct Scope* const scope, struct Scope* const parent_scope); 

bool scope_set(struct Scope* const scope, char *key, struct Value value);

void scope_dealloc(struct Scope* const scope);

struct Value scope_get(struct Scope* const scope, char *key);

#endif /* RAEL_SCOPE_H */
