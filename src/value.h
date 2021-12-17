#ifndef RAEL_VALUE_H
#define RAEL_VALUE_H

#include "common.h"

#include <stddef.h>

typedef struct RaelValue RaelValue;

/* return a RaelValue of type blame, storing a RaelValue of type string with the value of `string` */
#define RAEL_BLAME_FROM_RAWSTR_NO_STATE(string) ((RaelValue*)blame_no_state_new((RaelValue*)RAEL_STRING_FROM_RAWSTR((string))))
/* return a RaelValue of type blame, storing a RaelValue of type string with the value of `string` and the state `state` */
#define RAEL_BLAME_FROM_RAWSTR_STATE(string, state) ((RaelValue*)blame_new((RaelValue*)RAEL_STRING_FROM_RAWSTR((string)), (state)))
/* create a value from an `enum ValueType` and a C type that inherits from RaelValue */
#define RAEL_VALUE_NEW(value_type, c_type) ((c_type*)value_new(value_type, sizeof(c_type)))
/* header to put on top of custom rael runtime values which lets the values inherit from RaelValue */
#define RAEL_VALUE_BASE RaelValue _base

enum ValueType {
    ValueTypeVoid,
    ValueTypeNumber,
    ValueTypeString,
    ValueTypeRoutine,
    ValueTypeStack,
    ValueTypeRange,
    ValueTypeBlame,
    ValueTypeType,
    ValueTypeCFunc,
    ValueTypeModule
};

/* the dynamic value abstraction layer */
typedef struct RaelValue {
    enum ValueType type;
    size_t reference_count;
} RaelValue;

typedef struct RaelRangeValue {
    RAEL_VALUE_BASE;
    RaelInt start, end;
} RaelRangeValue;

typedef struct RaelBlameValue {
    RAEL_VALUE_BASE;
    RaelValue *value;
    struct State original_place;
} RaelBlameValue;

typedef struct RaelRoutineValue {
    RAEL_VALUE_BASE;
    struct Scope *scope;
    char **parameters;
    size_t amount_parameters;
    struct Instruction **block;
} RaelRoutineValue;

typedef struct RaelTypeValue {
    RAEL_VALUE_BASE;
    enum ValueType type;
} RaelTypeValue;

RaelRangeValue *range_new(RaelInt start, RaelInt end);

RaelValue *range_get(RaelRangeValue *range, size_t idx);

size_t range_length(RaelRangeValue *range);

RaelBlameValue *blame_new(RaelValue *message, struct State state);

RaelBlameValue *blame_no_state_new(RaelValue *message);

bool value_is_blame(RaelValue *value);

void blame_add_state(RaelBlameValue *value, struct State state);

void blame_delete(RaelBlameValue *blame);

RaelValue *value_new(enum ValueType type, size_t size);

void value_deref(RaelValue *value);

void value_ref(RaelValue *value);

void value_repr(RaelValue *value);

void value_log(RaelValue *value);

bool value_as_bool(RaelValue *value);

bool values_eq(RaelValue *value, RaelValue *value2);

bool value_is_iterable(RaelValue *value);

size_t value_length(RaelValue *value);

RaelValue **value_get_ptr(RaelValue *value, size_t idx);

RaelValue *value_get(RaelValue *value, size_t idx);

char *value_type_to_string(enum ValueType type);

/* lhs + rhs */
RaelValue *values_add(RaelValue *value, RaelValue *value2);

/* lhs - rhs */
RaelValue *values_sub(RaelValue *value, RaelValue *value2);

/* lhs * rhs */
RaelValue *values_mul(RaelValue *value, RaelValue *value2);

/* lhs / rhs */
RaelValue *values_div(RaelValue *value, RaelValue *value2);

/* lhs % rhs */
RaelValue *values_mod(RaelValue *value, RaelValue *value2);

/* lhs < rhs */
RaelValue *values_smaller(RaelValue *value, RaelValue *value2);

/* lhs > rhs */
RaelValue *values_bigger(RaelValue *value, RaelValue *value2);

/* lhs <= rhs */
RaelValue *values_smaller_eq(RaelValue *value, RaelValue *value2);

/* lhs >= rhs */
RaelValue *values_bigger_eq(RaelValue *value, RaelValue *value2);

#endif /* RAEL_VALUE_H */
