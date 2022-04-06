#ifndef RAEL_SCOPE_H
#define RAEL_SCOPE_H

#include "varmap.h"
#include "value.h"
#include "parser.h"

#include <stddef.h>
#include <stdbool.h>

struct Scope {
    size_t refcount;
    struct VariableMap variables;
    struct Scope *parent;
};

struct Scope *scope_new(struct Scope* const parent);

void scope_set_local(struct Scope *scope, char *key, RaelValue *value, bool dealloc_key_on_free);

void scope_set(struct Scope *scope, char *key, RaelValue *value, bool dealloc_key_on_free);

void scope_ref(struct Scope* const scope);

void scope_deref(struct Scope* const scope);

RaelValue **scope_get_ptr(struct Scope* const scope, char *key);

RaelValue *scope_get(struct Scope* const scope, char *key, const bool warn_undefined);

#endif /* RAEL_SCOPE_H */
