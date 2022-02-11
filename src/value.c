#include "rael.h"

bool type_eq(RaelTypeValue *self, RaelTypeValue *value) {
    // types are constant values that are only created, statically, once
    return self == value;
}

void type_repr(RaelTypeValue *self) {
    printf("%s", self->name);
}

RaelValue *type_cast(RaelTypeValue *self, RaelTypeValue *type) {
    if (type == &RaelStringType) {
        return string_new_pure(self->name, strlen(self->name), false);
    } else {
        return NULL;
    }
}

RaelInt type_validate_args(RaelTypeValue *self, size_t amount) {
    assert(self->constructor_info);

    if (!self->constructor_info->limits_args)
        return 0;
    else if (amount < self->constructor_info->min_args)
        return -1;
    else if (amount > self->constructor_info->max_args)
        return 1;
    else
        return 0;
}

/* construct a new value from the type, if possible */
RaelValue *type_call(RaelTypeValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    // if there is a constructor to the type, call it
    if (self->constructor_info) {
        RaelInt validation_id = type_validate_args(self, arguments_amount(args));
        if (validation_id != 0)
            return callable_blame_from_validation(validation_id);
        return self->constructor_info->op_construct(args, interpreter);
    } else {
        return BLAME_NEW_CSTR("Tried to construct a non-constructable type");
    }
}

static RaelCallableInfo type_callable_info = {
    (RaelCallerFunc)type_call,
    /* The validate_args check is performed in type_call, so we don't define it here */
    NULL
};

RaelTypeValue RaelTypeType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Type",
    .op_add = NULL,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = NULL,
    .op_eq = NULL, /* pointer comparison of types */
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    /* Defines the type as a callable and the callable-specific functions */
    .callable_info = &type_callable_info,
    .constructor_info = NULL,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = NULL,
    .deallocator = NULL,
    .repr = (RaelSingleFunc)type_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = (RaelCastFunc)type_cast,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL,

    .methods = NULL
};

RaelValue *void_cast(RaelValue *self, RaelTypeValue *type) {
    (void)self;
    if (type == &RaelStringType) {
        return RAEL_STRING_FROM_CSTR("Void");
    } else if (type == &RaelNumberType) {
        return number_newi(0);
    } else {
        return NULL;
    }
}

bool void_as_bool(RaelValue *self) {
    (void)self;
    return false;
}

void void_repr(RaelValue *self) {
    (void)self;
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

    .callable_info = NULL,
    .constructor_info = NULL,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = (RaelAsBoolFunc)void_as_bool,
    .deallocator = NULL,
    .repr = (RaelSingleFunc)void_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = void_cast,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL,

    .methods = NULL
};

RaelValue RaelVoid = (RaelValue) {
    .type = &RaelVoidType,
    .reference_count = 1
};

/* create a new RaelValue with RaelTypeValue and size `size` */
RaelValue *value_new(RaelTypeValue *type, size_t size) {
    RaelValue *value;
    assert(size >= sizeof(RaelValue));

    // re-reference the type because it's going to be used for the type
    value_ref((RaelValue*)type);
    value = malloc(size);
    value->type = type;
    value->reference_count = 1;
    // initialize members
    varmap_new(&value->keys);

    // if there are methods defines, add them
    if (type->methods) {
        for (MethodDecl *m = type->methods; m->method; ++m) {
            RaelValue *method = method_cfunc_new(value, m);
            varmap_set(&value->keys, m->name, method, true, false);
            value_deref(method);
        }
    }

    return value;
}

void value_ref(RaelValue *value) {
    RaelSingleFunc maybe_ref = value->type->op_ref;
    ++value->reference_count;
    // call the reference function if there is one defined
    if (maybe_ref) {
        maybe_ref(value);
    }
}

void value_deref(RaelValue *value) {
    RaelSingleFunc maybe_deref = value->type->op_deref;

    --value->reference_count;
    if (value->reference_count == 0) {
        RaelSingleFunc possible_deallocator = value->type->deallocator;

        // if there is a deallocator defined for the type, call it
        if (possible_deallocator) {
            possible_deallocator(value);
        }

        // delete set keys
        varmap_delete(&value->keys);
        // dereference the type
        value_deref((RaelValue*)value->type);
        // remove the allocated space of the dynamic value in memory
        free(value);
    } else if (maybe_deref) {
        maybe_deref(value);
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
bool value_truthy(RaelValue *const value) {
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
RaelValue *value_get_key(RaelValue *self, char *key, RaelInterpreter *interpreter) {
    // get value at that key
    RaelValue *value = varmap_get(&self->keys, key);

    // if the key is found, return it, otherwise return a Void
    if (value) {
        return value;
    }

    if (interpreter->warn_undefined) {
        rael_show_warning_key(key);
    }

    return void_new();
}

void value_set_key(RaelValue *self, char *key, RaelValue *value, bool deallocate_key_on_free) {
    varmap_set(&self->keys, key, value, true, deallocate_key_on_free);
}

RaelValue *value_slice(RaelValue *self, size_t start, size_t end) {
    RaelSliceFunc possible_at_range = self->type->at_range;

    if (possible_at_range) {
        size_t value_len = value_length(self);

        if (start > end || start > value_len  || end > value_len) {
            return BLAME_NEW_CSTR("Invalid slicing");
        }
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

RaelValue *value_call(RaelValue *value, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelCallerFunc possible_call;
    RaelValue *return_value;
    RaelInt validation_id;

    if (!value_is_callable(value))
        return NULL;

    possible_call = value->type->callable_info->op_call;
    assert(possible_call);
    // verify the callable can take that many arguments
    validation_id = callable_validate_args(value, arguments_amount(args));
    if (validation_id != 0)
        return callable_blame_from_validation(validation_id);

    return_value = possible_call(value, args, interpreter);

    // if a NULL was returned, make it a Void
    if (!return_value)
        return_value = void_new();

    return return_value;
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
    return value->type->callable_info != NULL;
}

/*
 * This function returns an integer that represents whether we can take `amount` arguments,
 * and if we don't, why can't we take that many arguments:
 * -1 = not enough arguments
 * 0 = can take that
 * 1 = too many arguments
 */
RaelInt callable_validate_args(RaelValue *callable, size_t amount) {
    RaelCanTakeFunc validate_args;

    assert(value_is_callable(callable));
    validate_args = callable->type->callable_info->op_validate_args;
    // if the function is not defined
    if (!validate_args)
        return 0;
    return validate_args(callable, amount);
}

/* return a matching blame to the given RaelInt */
RaelValue *callable_blame_from_validation(RaelInt validation) {
    if (validation == -1)
        return BLAME_NEW_CSTR("Not enough arguments");
    else if (validation == 1) {
        return BLAME_NEW_CSTR("Too many arguments");
    } else {
        RAEL_UNREACHABLE();
    }
}
