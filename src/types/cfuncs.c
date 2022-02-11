#include "rael.h"

#include "cfuncs.h"

/* Returns a new CFunc value that takes between min_args and max_args */
RaelValue *cfunc_ranged_new(char *name, RaelRawCFunc func, size_t min_args, size_t max_args) {
    RaelCFuncValue *cfunc = RAEL_VALUE_NEW(RaelCFuncType, RaelCFuncValue);
    cfunc->name = name;
    cfunc->func = func;
    cfunc->have_max = true;
    cfunc->min_args = min_args;
    cfunc->max_args = max_args;
    return (RaelValue*)cfunc;
}

/* Returns a new CFunc value that takes just one amount of args */
RaelValue *cfunc_new(char *name, RaelRawCFunc func, size_t amount_args) {
    return cfunc_ranged_new(name, func, amount_args, amount_args);
}

/* Returns a new CFunc value that takes min_args or more arguments (most functions) */
RaelValue *cfunc_unlimited_new(char *name, RaelRawCFunc func, size_t min_args) {
    RaelCFuncValue *cfunc = RAEL_VALUE_NEW(RaelCFuncType, RaelCFuncValue);
    cfunc->name = name;
    cfunc->func = func;
    cfunc->have_max = false;
    cfunc->min_args = min_args;
    return (RaelValue*)cfunc;
}

void cfunc_delete(RaelCFuncValue *self) {
    free(self->name);
}

RaelValue *cfunc_call(RaelCFuncValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    return self->func(args, interpreter);
}

bool cfunc_eq(RaelCFuncValue *self, RaelCFuncValue *value) {
    return self->func == value->func;
}

void cfunc_repr(RaelCFuncValue *self) {
    if (self->have_max) {
        if (self->min_args == self->max_args) {
            printf("[cfunc '%s' for %zu arguments]", self->name, self->min_args);
        } else {
            printf("[cfunc '%s' for %zu-%zu arguments]", self->name, self->min_args, self->max_args);
        }
    } else {
        printf("[cfunc '%s' for %zu or more arguments]", self->name, self->min_args);
    }
}

RaelInt cfunc_validate_args(RaelCFuncValue *self, size_t amount) {
    if (amount < self->min_args)
        return -1;
    if (self->have_max) {
        if (amount > self->max_args) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

static RaelCallableInfo cfunc_callable_info = {
    (RaelCallerFunc)cfunc_call,
    (RaelCanTakeFunc)cfunc_validate_args
};

RaelTypeValue RaelCFuncType = {
    RAEL_TYPE_DEF_INIT,
    .name = "CFunc",
    .op_add = NULL,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = NULL,
    .op_eq = (RaelBinCmpFunc)cfunc_eq,
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    .callable_info = &cfunc_callable_info,
    .constructor_info = NULL,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = NULL, // will just always be true
    .deallocator = (RaelSingleFunc)cfunc_delete,
    .repr = (RaelSingleFunc)cfunc_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL,

    .methods = NULL
};

RaelValue *method_cfunc_new(RaelValue *method_self, MethodDecl *decl) {
    RaelCFuncMethodValue *method = RAEL_VALUE_NEW(RaelCFuncMethodType, RaelCFuncMethodValue);

    method->method_self = method_self;
    method->name = decl->name;
    method->func = decl->method;
    if (decl->limits_args) {
        method->limit_arguments = true;
        method->minimum_arguments = decl->minimum_args;
        method->maximum_arguments = decl->maximum_args;
    } else {
        method->limit_arguments = false;
    }

    return (RaelValue*)method;
}

RaelInt method_cfunc_validate_args(RaelCFuncMethodValue *self, size_t amount) {
    if (!self->limit_arguments)
        return 0;
    else if (amount < self->minimum_arguments)
        return -1;
    else if (amount > self->maximum_arguments)
        return 1;
    else
        return 0;
}

RaelValue *method_cfunc_call(RaelCFuncMethodValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    return self->func(self->method_self, args, interpreter);
}

void method_cfunc_repr(RaelCFuncMethodValue *self) {
    printf("[cfunc method '%s' for type '", self->name);
    value_repr((RaelValue*)self->method_self->type);
    printf("']");
}

void method_cfunc_ref(RaelCFuncMethodValue *self) {
    value_ref(self->method_self);
}

void method_cfunc_deref(RaelCFuncMethodValue *self) {
    value_deref(self->method_self);
}

static RaelCallableInfo cfunc_method_callable_info = {
    (RaelCallerFunc)method_cfunc_call,
    (RaelCanTakeFunc)method_cfunc_validate_args
};

RaelTypeValue RaelCFuncMethodType = {
    RAEL_TYPE_DEF_INIT,
    .name = "CFuncMethod",
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

    .callable_info = &cfunc_method_callable_info,
    .constructor_info = NULL,
    .op_ref = (RaelSingleFunc)method_cfunc_ref,
    .op_deref = (RaelSingleFunc)method_cfunc_deref,

    .as_bool = NULL,
    .deallocator = NULL,
    .repr = (RaelSingleFunc)method_cfunc_repr,
    .logger = NULL,

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL,

    .methods = NULL
};
