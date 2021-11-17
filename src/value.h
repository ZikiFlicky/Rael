#ifndef RAEL_VALUE_H
#define RAEL_VALUE_H

#include "common.h"
#include "number.h"
#include "string.h"
#include "stack.h"

#include <stddef.h>

struct RaelValue;

typedef struct RaelValue* RaelValue;

enum ValueType {
    ValueTypeVoid,
    ValueTypeNumber,
    ValueTypeString,
    ValueTypeRoutine,
    ValueTypeStack,
    ValueTypeRange,
    ValueTypeBlame,
    ValueTypeType
};

struct RaelRangeValue {
    int start, end;
};

struct RaelBlameValue {
    RaelValue value;
    struct State original_place;
};

struct RaelRoutineValue {
    struct Scope *scope;
    char **parameters;
    size_t amount_parameters;
    struct Instruction **block;
};

// the dynamic value abstraction layer
struct RaelValue {
    enum ValueType type;
    size_t reference_count;
    union {
        struct RaelNumberValue as_number;
        struct RaelStringValue as_string;
        struct RaelRoutineValue as_routine;
        struct RaelStackValue as_stack;
        struct RaelRangeValue as_range;
        struct RaelBlameValue as_blame;
        enum ValueType as_type;
    };
};

RaelValue value_create(enum ValueType type);

void value_deref(RaelValue value);

void value_ref(RaelValue value);

void value_repr(RaelValue value);

void value_log(RaelValue value);

bool value_as_bool(const RaelValue value);

bool values_equal(const RaelValue value, const RaelValue value2);

bool value_is_iterable(RaelValue value);

size_t value_get_length(RaelValue value);

RaelValue value_get(RaelValue value, size_t idx);

RaelValue range_get(RaelValue range, size_t idx);

char *value_type_to_string(enum ValueType type);

#endif
