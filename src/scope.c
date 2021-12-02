#include "scope.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

void scope_construct(struct Scope* const scope, struct Scope* const parent_scope) {
    scope->parent = parent_scope;
    varmap_new(&scope->variables);
}

void scope_dealloc(struct Scope* const scope) {
    varmap_delete(&scope->variables);
}

void scope_set(struct Scope* const scope, char *key, RaelValue value, bool dealloc_key_on_free) {
    for (struct Scope *sc = scope; sc; sc = sc->parent) {
        if (varmap_set(&sc->variables, key, value, false, dealloc_key_on_free))
            return;
    }
    varmap_set(&scope->variables, key, value, true, dealloc_key_on_free);
}

void scope_set_local(struct Scope *scope, char* const key, RaelValue value, bool dealloc_key_on_free) {
    varmap_set(&scope->variables, key, value, true, dealloc_key_on_free);
}

RaelValue scope_get(struct Scope *scope, char* const key, const bool warn_undefined) {
    for (; scope; scope = scope->parent) {
        RaelValue value = varmap_get(&scope->variables, key);
        if (value)
            return value;
    }

    if (warn_undefined)
        fprintf(stderr, "WARNING: getting undefined variable ':%s'\n", key);

    return value_create(ValueTypeVoid);
}
