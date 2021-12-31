#include "value.h"
#include "stack.h"
#include "module.h"
#include "string.h"
#include "number.h"
#include "common.h"
#include "scope.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

struct Instruction;

void block_run(struct Interpreter* const interpreter, struct Instruction **block);
void interpreter_push_scope(struct Interpreter* const interpreter, struct Scope *scope_addr);
void interpreter_pop_scope(struct Interpreter* const interpreter);

static bool type_validate(RaelValue *type) {
    return type->type == &RaelTypeType;
}

bool type_eq(RaelTypeValue *self, RaelTypeValue *value) {
    // types are constant values that are only created, statically, once
    return self == value;
}

void type_repr(RaelTypeValue *self) {
    printf("%s", self->name);
}

RaelValue *type_cast(RaelTypeValue *self, RaelTypeValue *type) {
    if (type == &RaelTypeType) {
        return string_new_pure(self->name, strlen(self->name), false);
    } else {
        return NULL;
    }
}

RaelTypeValue RaelTypeType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Type",
    .op_add = NULL,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = NULL,
    .op_eq = NULL, /* pointer comparison */
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    .op_call = NULL,

    .as_bool = NULL,
    .deallocator = NULL,
    .repr = (RaelSingleFunc)type_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = (RaelCastFunc)type_cast,

    .at_index = NULL,
    .at_range = NULL,
    .at_key = NULL,

    .length = NULL
};

RaelValue *void_cast(RaelValue *self, RaelTypeValue *type) {
    if (type == &RaelStringType) {
        return RAEL_STRING_FROM_CSTR("Void");
    } else if (type == &RaelNumberType) {
        return number_newi(0);
    } else {
        return NULL;
    }
}

bool void_as_bool(RaelValue *self) {
    return false;
}

void void_repr(RaelValue *self) {
    printf("Void");
}

RaelValue *void_new(void) {
    value_ref(&RaelVoid);
    return &RaelVoid;
}

RaelTypeValue RaelVoidType = {
    RAEL_TYPE_DEF_INIT,
    .name = "VoidType",
    .op_add = NULL,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = NULL,
    .op_eq = NULL,
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    .op_call = NULL,

    .as_bool = (RaelAsBoolFunc)void_as_bool,
    .deallocator = NULL,
    .repr = (RaelSingleFunc)void_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = void_cast,

    .at_index = NULL,
    .at_range = NULL,
    .at_key = NULL,

    .length = NULL
};

RaelValue RaelVoid = (RaelValue) {
    .type = &RaelVoidType,
    .reference_count = 1
};

static inline bool routine_validate(RaelValue *routine) {
    return routine->type == &RaelRoutineType;
}

RaelValue *routine_call(RaelRoutineValue *self, RaelArguments *arguments, struct Interpreter *interpreter) {
    size_t amount_args, amount_params;
    struct Scope *prev_scope, routine_scope;

    amount_args = arguments_amount(arguments);
    amount_params = self->amount_parameters;

    if (amount_args < amount_params) {
        return BLAME_NEW_CSTR("Too few arguments");
    } else if (amount_args > amount_params) {
        return BLAME_NEW_CSTR("Too many arguments");
    }

    // store last scopes
    prev_scope = interpreter->scope;
    // create new "scope chain"
    interpreter->scope = self->scope;
    interpreter_push_scope(interpreter, &routine_scope);

    for (size_t i = 0; i < amount_params; ++i) {
        RaelValue *value = arguments_get(arguments, i);
        assert(value); // you must get a value
        // set the parameter
        scope_set_local(interpreter->scope, self->parameters[i], value, false);
    }

    // run the block of code
    block_run(interpreter, self->block);

    if (interpreter->interrupt == ProgramInterruptReturn) {
        // if had a return statement
        ;
    } else {
        // if you get to the end of the function without returning anything, return a Void
        interpreter->returned_value = void_new();
    }

    // clear interrupt
    interpreter->interrupt = ProgramInterruptNone;
    // remove routine scope and restore previous scope
    interpreter_pop_scope(interpreter);
    interpreter->scope = prev_scope;
    return interpreter->returned_value;
}

void routine_repr(RaelRoutineValue *self) {
    printf("routine(");
    if (self->amount_parameters > 0) {
        printf(":%s", self->parameters[0]);
        for (size_t i = 1; i < self->amount_parameters; ++i)
            printf(", :%s", self->parameters[i]);
    }
    printf(")");
}

RaelTypeValue RaelRoutineType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Routine",
    .op_add = NULL,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = NULL,
    .op_eq = NULL,
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    .op_call = (RaelCallerFunc)routine_call,

    .as_bool = NULL,
    .deallocator = NULL,
    .repr = (RaelSingleFunc)routine_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,
    .at_key = NULL,

    .length = NULL
};

RaelValue *range_new(RaelInt start, RaelInt end) {
    RaelRangeValue *range = RAEL_VALUE_NEW(RaelRangeType, RaelRangeValue);
    range->start = start;
    range->end = end;
    return (RaelValue*)range;
}

static inline bool range_validate(RaelValue *range) {
    return range->type == &RaelRangeType;
}

size_t range_length(RaelRangeValue *range) {
    return (size_t)rael_int_abs(range->end - range->start);
}

bool range_as_bool(RaelRangeValue *self) {
    return range_length(self) != 0;
}

RaelValue *range_get(RaelRangeValue *self, size_t idx) {
    RaelInt number;

    if (idx >= range_length(self))
        return NULL;

    // get the number, depending on the direction
    if (self->end > self->start)
        number = self->start + idx;
    else
        number = self->start - idx;

    // return the number
    return number_newi(number);
}

bool range_eq(RaelRangeValue *self, RaelRangeValue *value) {
    return self->start == value->start && self->end == value->end;
}

void range_repr(RaelRangeValue *self) {
    printf("%ld to %ld", self->start, self->end);
}

RaelTypeValue RaelRangeType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Range",
    .op_add = NULL,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = NULL,
    .op_eq = (RaelBinCmpFunc)range_eq,
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    .op_call = NULL,

    .as_bool = (RaelAsBoolFunc)range_as_bool,
    .deallocator = NULL,
    .repr = (RaelSingleFunc)range_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = (RaelGetFunc)range_get,
    .at_range = NULL,
    .at_key = NULL,

    .length = (RaelLengthFunc)range_length
};

bool blame_validate(RaelValue *value) {
    return value->type == &RaelBlameType;
}

RaelValue *blame_new(RaelValue *message, struct State *state) {
    RaelBlameValue *blame = RAEL_VALUE_NEW(RaelBlameType, RaelBlameValue);
    if (state) {
        blame->state_defined = true;    
        blame->original_place = *state;
    } else {
        blame->state_defined = false;
    }
    
    blame->message = message;
    return (RaelValue*)blame;
}

RaelValue *blame_get_message(RaelBlameValue *blame) {
    return blame->message;
}

void blame_delete(RaelBlameValue *blame) {
    RaelValue *message = blame_get_message(blame);
    // if there is a blame message, dereference it
    if (message)
        value_deref(message);
}

/* if there was no state on the blame beforehand, add a state */
void blame_set_state(RaelBlameValue *self, struct State state) {
    // define a new state only if there isn't one yet
    if (!self->state_defined) {
        self->state_defined = true;
        self->original_place = state;
    }
}

RaelTypeValue RaelBlameType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Blame",
    .op_add = NULL,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = NULL,
    .op_eq = NULL,
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    .op_call = NULL,

    .as_bool = NULL,
    .deallocator = (RaelSingleFunc)blame_delete,
    .repr = NULL,
    .logger = NULL,

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,
    .at_key = NULL,

    .length = NULL
};

/* create a new RaelValue with RaelTypeValue and size `size` */
RaelValue *value_new(RaelTypeValue *type, size_t size) {
    RaelValue *value;
    assert(size >= sizeof(RaelValue));

    value = malloc(size);
    value->type = type;
    value->reference_count = 1;
    return value;
}

void value_ref(RaelValue *value) {
    ++value->reference_count;
}

void value_deref(RaelValue *value) {
    --value->reference_count;
    if (value->reference_count == 0) {
        RaelSingleFunc possible_deallocator = value->type->deallocator;

        // if there is a deallocator defined for the type, call it
        if (possible_deallocator) {
            possible_deallocator(value);
        }

        free(value);
    }
}

void value_repr(RaelValue *value) {
    RaelSingleFunc possible_repr = value->type->repr;

    // if there is a repr function defined for the type
    if (possible_repr) {
        possible_repr(value);
    } else {
        printf("[%s at %p]", value->type->name, value);
    }
}

void value_log(RaelValue *value) {
    RaelSingleFunc possible_complex_repr = value->type->logger;

    if (possible_complex_repr) {
        possible_complex_repr(value);
    } else {
        // fallback to regular repr if there isn't a complex_repr
        value_repr(value);
    }
}

/* is the value booleanly true? like Python's bool() operator */
bool value_as_bool(RaelValue *const value) {
    RaelAsBoolFunc possible_as_bool = value->type->as_bool;

    if (possible_as_bool) {
        return possible_as_bool(value);
    } else {
        // if there is no as_bool function just return a true
        return true;
    }
}

bool values_eq(RaelValue* const value, RaelValue* const value2) {
    bool res;
    // if they have the same pointer they must be equal
    if (value == value2) {
        res = true;
    } else if (value->type != value2->type) {
        // if they don't share the same type they are not equal
        res = false;
    } else {
        RaelBinCmpFunc possible_eq = value->type->op_eq;

        // if there is a '=' operator defined, call it, else return false
        if (possible_eq) {
            res = possible_eq(value, value2);
        } else {
            res = false;
        }
    }

    return res;
}

size_t value_length(RaelValue *self) {
    RaelLengthFunc possible_length_func;
    assert(value_is_iterable(self));

    possible_length_func = self->type->length;
    // every iterable must have a length function
    assert(possible_length_func != NULL);

    return possible_length_func(self);
}

/* :Value at :number */
RaelValue *value_get(RaelValue *self, size_t idx) {
    RaelGetFunc possible_at_idx;

    if (!value_is_iterable(self)) {
        return NULL;
    }
    possible_at_idx = self->type->at_index;
    if (possible_at_idx) {
        RaelValue *value = possible_at_idx(self, idx);
        if (value) {
            return value;
        } else {
            return BLAME_NEW_CSTR("Index too big");
        }
    } else {
        return NULL;
    }
}

/* :Value:Key */
RaelValue *value_get_key(RaelValue *self, char *key) {
    RaelAtKeyFunc possible_get_key = self->type->at_key;

    if (possible_get_key) {
        RaelValue *value = possible_get_key(self, key);
        // the function must return a value
        assert(value);
        return value;
    } else {
        return NULL;
    }
}

RaelValue *value_slice(RaelValue *self, size_t start, size_t end) {
    RaelSliceFunc possible_at_range;
    size_t value_len = value_length(self);

    if (start > end || start > value_len  || end > value_len) {
        return BLAME_NEW_CSTR("Invalid slicing");
    }
    possible_at_range = self->type->at_range;
    if (possible_at_range) {
        return possible_at_range(self, start, end);
    } else {
        return NULL; // can't slice value
    }
}

/* lhs + rhs */
RaelValue *values_add(RaelValue *value, RaelValue *value2) {
    RaelBinExprFunc possible_add = value->type->op_add;

    if (possible_add) {
        return possible_add(value, value2);
    } else {
        return NULL;
    }
}

/* lhs - rhs */
RaelValue *values_sub(RaelValue *value, RaelValue *value2) {
    RaelBinExprFunc possible_sub = value->type->op_sub;

    if (possible_sub) {
        return possible_sub(value, value2);
    } else {
        return NULL;
    }
}

/* lhs * rhs */
RaelValue *values_mul(RaelValue *value, RaelValue *value2) {
    RaelBinExprFunc possible_mul = value->type->op_mul;

    if (possible_mul) {
        return possible_mul(value, value2);
    } else {
        return NULL;
    }
}

/* lhs / rhs */
RaelValue *values_div(RaelValue *value, RaelValue *value2) {
    RaelBinExprFunc possible_div = value->type->op_div;

    if (possible_div) {
        return possible_div(value, value2);
    } else {
        return NULL;
    }
}

/* lhs % rhs */
RaelValue *values_mod(RaelValue *value, RaelValue *value2) {
    RaelBinExprFunc possible_mod = value->type->op_mod;

    if (possible_mod) {
        return possible_mod(value, value2);
    } else {
        return NULL;
    }
}

/* lhs << rhs (redirect operator) */
RaelValue *values_red(RaelValue *value, RaelValue *value2) {
    RaelBinExprFunc possible_red = value->type->op_red;

    if (possible_red) {
        return possible_red(value, value2);
    } else {
        return NULL;
    }
}

/* lhs < rhs */
RaelValue *values_smaller(RaelValue *value, RaelValue *value2) {
    RaelBinCmpFunc possible_smaller = value->type->op_smaller;

    if (value->type != value2->type) {
        return BLAME_NEW_CSTR("Comparison operation expects equal types of values");
    }
    if (possible_smaller) {
        return number_newi(possible_smaller(value, value2));
    } else {
        return NULL;
    }
}

/* lhs > rhs */
RaelValue *values_bigger(RaelValue *value, RaelValue *value2) {
    RaelBinCmpFunc possible_bigger = value->type->op_bigger;

    if (value->type != value2->type) {
        return BLAME_NEW_CSTR("Comparison operation expects equal types of values");
    }
    if (possible_bigger) {
        return number_newi(possible_bigger(value, value2));
    } else {
        return NULL;
    }
}

/* lhs <= rhs */
RaelValue *values_smaller_eq(RaelValue *value, RaelValue *value2) {
    RaelBinCmpFunc possible_smaller_eq = value->type->op_smaller_eq;

    if (value->type != value2->type) {
        return BLAME_NEW_CSTR("Comparison operation expects equal types of values");
    }
    if (possible_smaller_eq) {
        return number_newi(possible_smaller_eq(value, value2));
    } else {
        return NULL;
    }
}

/* lhs >= rhs */
RaelValue *values_bigger_eq(RaelValue *value, RaelValue *value2) {
    RaelBinCmpFunc possible_bigger_eq = value->type->op_bigger_eq;

    if (value->type != value2->type) {
        return BLAME_NEW_CSTR("Comparison operation expects equal types of values");
    }
    if (possible_bigger_eq) {
        return number_newi(possible_bigger_eq(value, value2));
    } else {
        return NULL;
    }
}

RaelValue *value_cast(RaelValue *value, RaelTypeValue *type) {
    RaelCastFunc possible_cast;

    if (value->type == type) {
        value_ref(value);
        return value;
    }

    possible_cast = value->type->cast;
    if (possible_cast) {
        return possible_cast(value, type);
    } else {
        return NULL; // cast not possible between types
    }
}

RaelValue *value_call(RaelValue *value, RaelArguments *args, struct Interpreter *interpreter) {
    RaelCallerFunc possible_call = value->type->op_call;

    if (possible_call) {
        RaelValue *return_value = possible_call(value, args, interpreter);
        // if there was no return value, return a Void
        if (!return_value) {
            return_value = void_new();
        }
        return return_value;
    } else {
        return NULL; // non-callable
    }
}

RaelValue *value_neg(RaelValue *value) {
    RaelNegFunc possible_neg = value->type->op_neg;

    if (possible_neg) {
        return possible_neg(value);
    } else {
        return NULL;
    }
}

bool value_is_iterable(RaelValue *value) {
    return value->type->at_index != NULL &&
           value->type->length != NULL;
}

bool value_is_callable(RaelValue *value) {
    return value->type->op_call != NULL;
}
