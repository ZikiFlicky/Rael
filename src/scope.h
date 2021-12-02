#ifndef RAEL_SCOPE_H
#define RAEL_SCOPE_H

#include "varmap.h"
#include "value.h"
#include "parser.h"

#include <stddef.h>
#include <stdbool.h>

struct Scope {
    struct VariableMap variables;
    struct Scope *parent;
};

void scope_construct(struct Scope* const scope, struct Scope* const parent_scope); 

void scope_set_local(struct Scope *scope, char *key, RaelValue value, bool dealloc_key_on_free);

void scope_set(struct Scope *scope, char *key, RaelValue value, bool dealloc_key_on_free);

void scope_dealloc(struct Scope* const scope);

RaelValue scope_get(struct Scope* const scope, char *key, const bool warn_undefined);

#endif /* RAEL_SCOPE_H */
