#ifndef RAEL_VALUE_H
#define RAEL_VALUE_H

#include "parser.h"
#include "number.h"
#include "string.h"

#include <stddef.h>

struct RaelStackValue {
    struct RaelValue *values;
    size_t length, allocated;
};

// the dynamic value abstraction layer
struct RaelValue {
    enum ValueType type;
    union {
        struct NumberExpr number;
        struct RaelStringValue string;
        struct RaelRoutineValue routine;
        struct RaelStackValue stack;
    } as;
};

#endif
