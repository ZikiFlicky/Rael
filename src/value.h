#ifndef RAEL_VALUE_H
#define RAEL_VALUE_H

#include "common.h"
#include "number.h"
#include "string.h"
#include "stack.h"
#include "module.h"

#include <stddef.h>

/* return a RaelValue of type blame, storing a RaelValue of type string with the value of `string` */
#define RAEL_BLAME_FROM_RAWSTR_NO_STATE(string) (blame_no_state_new(RAEL_STRING_FROM_RAWSTR((string))))

/* return a RaelValue of type blame, storing a RaelValue of type string with the value of `string` and the state `state` */
#define RAEL_BLAME_FROM_RAWSTR_STATE(string, state) (blame_new(RAEL_STRING_FROM_RAWSTR((string)), (state)))

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
    ValueTypeType,
    ValueTypeCFunc,
    ValueTypeModule
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
        struct RaelExternalCFuncValue as_cfunc;
        struct RaelModuleValue as_module;
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

RaelValue *value_get_ptr(RaelValue value, size_t idx);

RaelValue value_get(RaelValue value, size_t idx);

RaelValue range_get(RaelValue range, size_t idx);

size_t range_get_length(RaelValue range);

RaelValue blame_new(RaelValue message, struct State state);

RaelValue blame_no_state_new(RaelValue value);

bool value_is_blame(RaelValue value);

void blame_add_state(RaelValue value, struct State state);

void blamevalue_delete(struct RaelBlameValue *blame);

char *value_type_to_string(enum ValueType type);

/* lhs + rhs */
RaelValue values_add(RaelValue value, RaelValue value2);

/* lhs - rhs */
RaelValue values_sub(RaelValue value, RaelValue value2);

/* lhs * rhs */
RaelValue values_mul(RaelValue value, RaelValue value2);

/* lhs / rhs */
RaelValue values_div(RaelValue value, RaelValue value2);

/* lhs % rhs */
RaelValue values_mod(RaelValue value, RaelValue value2);

/* lhs < rhs */
RaelValue values_smaller(RaelValue value, RaelValue value2);

/* lhs > rhs */
RaelValue values_bigger(RaelValue value, RaelValue value2);

/* lhs <= rhs */
RaelValue values_smaller_eq(RaelValue value, RaelValue value2);

/* lhs >= rhs */
RaelValue values_bigger_eq(RaelValue value, RaelValue value2);

#endif /* RAEL_VALUE_H */
