#ifndef RAEL_VALUE_H
#define RAEL_VALUE_H

#include "varmap.h"
#include "common.h"
#include "interpreter.h"

#include <stddef.h>

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

/* Special type operation functions */
typedef RaelValue* (*RaelBinExprFunc)(RaelValue*, RaelValue*);
typedef bool (*RaelBinCmpFunc)(RaelValue*, RaelValue*);
typedef bool (*RaelAsBoolFunc)(RaelValue*);
typedef void (*RaelSingleFunc)(RaelValue*);
typedef RaelValue* (*RaelCallerFunc)(RaelValue*, RaelArgumentList*, RaelInterpreter*);
typedef RaelValue* (*RaelConstructorFunc)(RaelArgumentList*, RaelInterpreter*);
typedef RaelValue* (*RaelGetFunc)(RaelValue*, size_t);
typedef RaelValue* (*RaelSliceFunc)(RaelValue*, size_t, size_t);
typedef RaelValue* (*RaelCastFunc)(RaelValue*, RaelTypeValue*);
typedef RaelValue* (*RaelAtKeyFunc)(RaelValue*, char*);
typedef RaelValue* (*RaelNegFunc)(RaelValue*);
typedef size_t (*RaelLengthFunc)(RaelValue*);
typedef RaelValue* (*RaelMethodFunc)(RaelValue*, RaelArgumentList*, RaelInterpreter*);
typedef RaelInt (*RaelCanTakeFunc)(RaelValue*, size_t);

/* the dynamic value abstraction layer */
typedef struct RaelValue {
    RaelTypeValue *type;
    size_t reference_count;
    struct VariableMap keys;
} RaelValue;

typedef struct RaelCallableInfo {
    /* Must be defined */
    RaelCallerFunc op_call;
    /* If is NULL, we infer that we can just call the function */
    RaelCanTakeFunc op_validate_args;
} RaelCallableInfo;

typedef struct RaelConstructorInfo {
    /* Returns true if you can construct */
    RaelConstructorFunc op_construct;
    bool limits_args;
    size_t min_args, max_args;
} RaelConstructorInfo;

typedef struct MethodDecl {
    char *name;
    RaelMethodFunc method;
    bool limits_args;
    size_t minimum_args, maximum_args;
} MethodDecl;

#define RAEL_CMETHOD(name, func, min_args, max_args) { (name), (RaelMethodFunc)(func), true, (min_args), (max_args) }
#define RAEL_CMETHOD_UNRESTRICTED(name, func) { (name), (RaelMethodFunc)(func), false, 0, 0 }
#define RAEL_CMETHOD_TERMINATOR { NULL, NULL, false, 0, 0 }

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

    /* Information about the callability of the value. NULL if the value isn't callable */
    RaelCallableInfo *callable_info;
    /* Information about the constructor. NULL if there is no constructor */
    RaelConstructorInfo *constructor_info;
    /* Special operation to do on reference */
    RaelSingleFunc op_ref;
    /* Special operation to do on dereference if there are still references to the value */
    RaelSingleFunc op_deref;

    /* Convert a value to its boolean representation */
    RaelAsBoolFunc as_bool; 
    /* Free all of the internally allocated values. Called by the interpreter */
    RaelSingleFunc deallocator;
    /*
     * Log (print) the value, as its raw representation (For example: "String").
     * If this is uninitialized, return a default representation for the value
     */
    RaelSingleFunc repr;
    /*
     * Log a more complex representation of the value (For example: String).
     * If this is undefined (NULL), the interpreter will fallback and
     * automatically run the normal repr function instead. defined without a following newline
     */
    RaelSingleFunc logger;

    /* Cast the value to a new type */
    RaelCastFunc cast;

    /* Indexing operations */
    RaelGetFunc at_index;
    RaelSliceFunc at_range;

    /* Get length */
    RaelLengthFunc length;

    /* Methods */
    MethodDecl *methods;
} RaelTypeValue;

/* declare some builtin types */
extern RaelTypeValue RaelTypeType;
extern RaelTypeValue RaelVoidType;

/* declare constants */
extern RaelValue RaelVoid; /* Rael's Void */

/* return RaelVoid pointer and add a reference to it */
RaelValue *void_new(void);

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
bool value_truthy(RaelValue *value);

/* returns a boolean saying if the values are equal */
bool values_eq(RaelValue *value, RaelValue *value2);

/* returns a boolean saying if the value is an iterable */
bool value_is_iterable(RaelValue *value);

/* returns a boolean saying if the value is callable */
bool value_is_callable(RaelValue *value);

/* returns -1 if you haven't entered enough arguments, 1 if you've entered too many, and 0 otherwise */
RaelInt callable_validate_args(RaelValue *callable, size_t amount);

/* returns a matching blame to whatever is returned from callable_validate_args */
RaelValue *callable_blame_from_validation(RaelInt validation);

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
RaelValue *value_get_key(RaelValue *self, char *key, RaelInterpreter *interpreter);

/* value:key ?= value. notice that key will be freed, so it mustn't be a shared key (like an ast one) */
void value_set_key(RaelValue *self, char *key, RaelValue *value);

/* value:key ?= i. same rules as value_set_key are applied here */
void value_set_int(RaelValue *value, char *key, RaelInt i);

/* value at range */
RaelValue *value_slice(RaelValue *self, size_t start, size_t end);

/* value(arg1, arg2, ...) */
RaelValue *value_call(RaelValue *value, RaelArgumentList *args, RaelInterpreter *interpreter);

#endif /* RAEL_VALUE_H */
