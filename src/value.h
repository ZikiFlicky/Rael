#ifndef RAEL_VALUE_H
#define RAEL_VALUE_H

#include "common.h"
#include "varmap.h"

#include <stddef.h>

typedef struct RaelValue RaelValue;

/* return a RaelValue of type blame, storing a RaelValue of type string with the value of `string` */
#define BLAME_NEW_CSTR(string) ((RaelValue*)blame_new((RaelValue*)RAEL_STRING_FROM_CSTR((string)), NULL))
/* return a RaelValue of type blame, storing a RaelValue of type string with the value of `string` and the state `state` */
#define BLAME_NEW_CSTR_ST(string, state) ((RaelValue*)blame_new((RaelValue*)RAEL_STRING_FROM_CSTR((string)), &(state)))
/* create a value from a `RaelTypeValue` and a C type that inherits from RaelValue */
#define RAEL_VALUE_NEW(value_type, c_type) ((c_type*)value_new(&value_type, sizeof(c_type)))
/* header to put on top of custom rael runtime values which lets the values inherit from RaelValue */
#define RAEL_VALUE_BASE RaelValue _base
/* header for type definitions */
#define RAEL_TYPE_DEF_INIT ._base = { \
    .reference_count = 1,             \
    .type = &RaelTypeType             \
}

struct RaelTypeValue;
typedef struct RaelTypeValue RaelTypeValue;

/* the dynamic value abstraction layer */
typedef struct RaelValue {
    RaelTypeValue *type;
    size_t reference_count;
    struct VariableMap keys;
} RaelValue;

typedef struct RaelRangeValue {
    RAEL_VALUE_BASE;
    RaelInt start, end;
} RaelRangeValue;

typedef struct RaelBlameValue {
    RAEL_VALUE_BASE;
    bool state_defined;
    RaelValue *message;
    struct State original_place;
} RaelBlameValue;

typedef struct RaelRoutineValue {
    RAEL_VALUE_BASE;
    struct Scope *scope;
    char **parameters;
    size_t amount_parameters;
    struct Instruction **block;
} RaelRoutineValue;

/* Special type operation functions */
typedef RaelValue* (*RaelBinExprFunc)(RaelValue*, RaelValue*);
typedef bool (*RaelBinCmpFunc)(RaelValue*, RaelValue*);
typedef bool (*RaelAsBoolFunc)(RaelValue*);
typedef void (*RaelSingleFunc)(RaelValue*);
typedef RaelValue* (*RaelCallerFunc)(RaelValue*, RaelArguments*, struct Interpreter*);
typedef RaelValue* (*RaelGetFunc)(RaelValue*, size_t);
typedef RaelValue* (*RaelSliceFunc)(RaelValue*, size_t, size_t);
typedef RaelValue* (*RaelCastFunc)(RaelValue*, RaelTypeValue*);
typedef RaelValue* (*RaelAtKeyFunc)(RaelValue*, char*);
typedef RaelValue* (*RaelNegFunc)(RaelValue*);
typedef size_t (*RaelLengthFunc)(RaelValue*);

/* The Type value type */
typedef struct RaelTypeValue {
    RAEL_VALUE_BASE;
    char *name;

    /* Different binary operations */
    RaelBinExprFunc op_add; /* + */
    RaelBinExprFunc op_sub; /* - */
    RaelBinExprFunc op_mul; /* * */
    RaelBinExprFunc op_div; /* / */
    RaelBinExprFunc op_mod; /* % */
    RaelBinExprFunc op_red; /* << */
    RaelBinCmpFunc op_eq; /* = */
    RaelBinCmpFunc op_smaller; /* < */
    RaelBinCmpFunc op_bigger; /* > */
    RaelBinCmpFunc op_smaller_eq; /* <= */
    RaelBinCmpFunc op_bigger_eq; /* >= */

    RaelNegFunc op_neg; /* -value */

    /* non-binary operations */
    RaelCallerFunc op_call; /* Call the value */

    RaelAsBoolFunc as_bool; /* Convert a value to its boolean representation */
    RaelSingleFunc deallocator; /* Free all of the internally allocated values. Called by the interpreter */
    RaelSingleFunc repr; /* Log (print) the value, as its raw representation (For example: "String").
                            If this is uninitialized, return a default representation for the value */
    RaelSingleFunc logger; /* Log a more complex representation of the value (For example: String).
                              If this is undefined (NULL), the interpreter will fallback and
                              automatically run the normal repr function instead. defined without a following newline */

    /* cast the value to a new type */
    RaelCastFunc cast;

    /* indexing operations */
    RaelGetFunc at_index;
    RaelSliceFunc at_range;

    /* length */
    RaelLengthFunc length;
} RaelTypeValue;

/* declare builtin types */
extern RaelTypeValue RaelTypeType;
extern RaelTypeValue RaelVoidType;
extern RaelTypeValue RaelRangeType;
extern RaelTypeValue RaelRoutineType;
extern RaelTypeValue RaelBlameType;

/* declare constants */
extern RaelValue RaelVoid; /* Rael's Void */

/* return RaelVoid pointer and add a reference to it */
RaelValue *void_new(void);

/* create a new range from two whole numbers */
RaelValue *range_new(RaelInt start, RaelInt end);

/* create a new blame from a message and a state  */
RaelValue *blame_new(RaelValue *message, struct State *state);

/* if there isn't a state defined in the blame, set it to the new state */
void blame_set_state(RaelBlameValue *value, struct State state);

/* check if the value is a blame */
bool blame_validate(RaelValue *value);

/* create a new uninitialized value from a RaelTypeValue and a size */
RaelValue *value_new(RaelTypeValue *type, size_t size);

/* dereference a value, by calling its deconstructor */
void value_deref(RaelValue *value);

/* add a reference to a value */
void value_ref(RaelValue *value);

/* log a simple representation of a value, for example: 123 or "a string" */
void value_repr(RaelValue *value);

/* logs a more complex representation of a value, for example: a string */
void value_log(RaelValue *value);

/* return the value's boolean representation, calls the value's `as_bool` method */
bool value_as_bool(RaelValue *value);

/* returns a boolean saying if the values are equal */
bool values_eq(RaelValue *value, RaelValue *value2);

/* returns a boolean saying if the value is an iterable */
bool value_is_iterable(RaelValue *value);

/* returns a boolean saying if the value is callable */
bool value_is_callable(RaelValue *value);

/* get the length of the value. calls the value's `length` method */
size_t value_length(RaelValue *value);

/* get a pointer to a value at an index */
RaelValue **value_get_ptr(RaelValue *value, size_t idx);

/* get the value's value at an index. returns NULL if you can't `get` from the value */
RaelValue *value_get(RaelValue *value, size_t idx);

/* lhs + rhs */
RaelValue *values_add(RaelValue *value, RaelValue *value2);

/* lhs - rhs */
RaelValue *values_sub(RaelValue *value, RaelValue *value2);

/* lhs * rhs */
RaelValue *values_mul(RaelValue *value, RaelValue *value2);

/* lhs / rhs */
RaelValue *values_div(RaelValue *value, RaelValue *value2);

/* lhs << rhs */
RaelValue *values_red(RaelValue *value, RaelValue *value2);

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

/* -value */
RaelValue *value_neg(RaelValue *value);

/* value to type */
RaelValue *value_cast(RaelValue *value, RaelTypeValue *type);

/* value:key */
RaelValue *value_get_key(RaelValue *self, char *key);

/* value:key ?= */
void value_set_key(RaelValue *self, char *key, RaelValue *value, bool deallocate_key_on_free);

/* value at range */
RaelValue *value_slice(RaelValue *self, size_t start, size_t end);

/* value(arg1, arg2, ...) */
RaelValue *value_call(RaelValue *value, RaelArguments *args, struct Interpreter *interpreter);

#endif /* RAEL_VALUE_H */
