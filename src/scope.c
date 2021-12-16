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

void scope_set(struct Scope* const scope, char *key, RaelValue *value, bool dealloc_key_on_free) {
    for (struct Scope *sc = scope; sc; sc = sc->parent) {
        if (varmap_set(&sc->variables, key, value, false, dealloc_key_on_free))
            return;
    }
    varmap_set(&scope->variables, key, value, true, dealloc_key_on_free);
}

void scope_set_local(struct Scope *scope, char* const key, RaelValue *value, bool dealloc_key_on_free) {
    varmap_set(&scope->variables, key, value, true, dealloc_key_on_free);
}

RaelValue **scope_get_ptr(struct Scope* const scope, char *key) {
    // loop all scopes and try to find the key inside them
    for (struct Scope *sc = scope; sc; sc = sc->parent) {
        RaelValue **value = varmap_get_ptr(&sc->variables, key);
        // couldn't find the key inside of the varmap
        if (value) {
            return value;
        }
    }
    return NULL;
}

RaelValue *scope_get(struct Scope *scope, char* const key, const bool warn_undefined) {
    RaelValue **value_ptr;
    // try to find the key
    value_ptr = scope_get_ptr(scope, key);
    // if the value is present, return it, and if it isn't return a Void
    if (value_ptr) {
        value_ref(*value_ptr);
        return *value_ptr;
    } else {
        // if defined --warn-undefined
        if (warn_undefined) {
            fprintf(stderr, "WARNING: getting undefined variable ':%s'\n", key);
        }
        return value_new(ValueTypeVoid);
    }
}
