#include "rael.h"

RaelValue *cfunc_new(char *name, RaelRawCFunc func, size_t amount_params) {
    RaelCFuncValue *cfunc = RAEL_VALUE_NEW(RaelCFuncType, RaelCFuncValue);
    cfunc->name = name;
    cfunc->func = func;
    cfunc->amount_params = amount_params;
    return (RaelValue*)cfunc;
}

void cfunc_delete(RaelCFuncValue *self) {
    free(self->name);
}

RaelValue *cfunc_call(RaelCFuncValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    size_t amount_args = arguments_amount(args);
    if (amount_args < self->amount_params)
        return BLAME_NEW_CSTR("Not enough arguments");
    else if (amount_args > self->amount_params)
        return BLAME_NEW_CSTR("Too many arguments");

    return self->func(args, interpreter);
}

bool cfunc_eq(RaelCFuncValue *self, RaelCFuncValue *value) {
    return self->func == value->func;
}

void cfunc_repr(RaelCFuncValue *self) {
    printf("[cfunc '%s' for %zu arguments]", self->name, self->amount_params);
}

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

    .op_call = (RaelCallerFunc)cfunc_call,
    .op_construct = NULL,
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

RaelValue *method_cfunc_new(RaelValue *method_self, char *name, RaelMethodFunc func) {
    RaelCFuncMethodValue *method = RAEL_VALUE_NEW(RaelCFuncMethodType, RaelCFuncMethodValue);

    method->method_self = method_self;
    method->name = name;
    method->func = func;

    return (RaelValue*)method;
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

    .op_call = (RaelCallerFunc)method_cfunc_call,
    .op_construct = NULL,
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

RaelValue *module_new(char *name) {
    RaelModuleValue *module = RAEL_VALUE_NEW(RaelModuleType, RaelModuleValue);
    // set the module's name
    module->name = name;
    return (RaelValue*)module;
}

void module_set_key(RaelModuleValue *self, char *varname, RaelValue *value) {
    varmap_set(&((RaelValue*)self)->keys, varname, value, true, true);
    // remove the added reference
    value_deref(value);
}

static void module_delete(RaelModuleValue *self) {
    free(self->name);
}

static void module_repr(RaelModuleValue *self) {
    printf("module(:%s)", self->name);
}

RaelTypeValue RaelModuleType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Module",
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
    .op_construct = NULL,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = NULL, /* will always be true */
    .deallocator = (RaelSingleFunc)module_delete,
    .repr = (RaelSingleFunc)module_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL,

    .methods = NULL
};
