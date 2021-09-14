#ifndef RAEL_VALUE_H
#define RAEL_VALUE_H

#include "parser.h"
#include "number.h"
#include "string.h"

#include <stddef.h>

struct RaelStackValue {
    struct RaelValue **values;
    size_t length, allocated;
};

// the dynamic value abstraction layer
struct RaelValue {
    enum ValueType type;
    size_t amount_references;
    union {
        struct NumberExpr as_number;
        struct RaelStringValue as_string;
        struct RaelRoutineValue as_routine;
        struct RaelStackValue as_stack;
    };
};

struct RaelValue *value_create(enum ValueType type);

void value_delete(struct RaelValue *value);

#endif
