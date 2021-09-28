#ifndef RAEL_VALUE_H
#define RAEL_VALUE_H

#include "parser.h"
#include "number.h"
#include "string.h"

#include <stddef.h>

struct RaelValue;

typedef struct RaelValue* RaelValue;

struct RaelRangeValue {
    int start, end;
};

struct RaelStackValue {
    RaelValue *values;
    size_t length, allocated;
};

struct RaelBlameValue {
    RaelValue value;
    struct State original_place;
};

// the dynamic value abstraction layer
struct RaelValue {
    enum ValueType type;
    size_t reference_count;
    union {
        struct NumberExpr as_number;
        struct RaelStringValue as_string;
        struct RaelRoutineValue as_routine;
        struct RaelStackValue as_stack;
        struct RaelRangeValue as_range;
        struct RaelBlameValue as_blame;
    };
};

RaelValue value_create(enum ValueType type);

void value_dereference(RaelValue value);

#endif
