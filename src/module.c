#include "rael.h"

RaelValue *module_math_new(void);
RaelValue *module_types_new(void);

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
    (void)interpreter;
    if (arguments_amount(args) != self->amount_params)
        return NULL;

    return self->func(args);
}

bool cfunc_eq(RaelCFuncValue *self, RaelCFuncValue *value) {
    return self->func == value->func;
}

void cfunc_repr(RaelCFuncValue *self) {
    printf("cfunc(:%s, %zu)", self->name, self->amount_params);
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

    .as_bool = NULL, // will just always be true
    .deallocator = (RaelSingleFunc)cfunc_delete,
    .repr = (RaelSingleFunc)cfunc_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL
};

RaelValue *module_new(char *name) {
    RaelModuleValue *module = RAEL_VALUE_NEW(RaelModuleType, RaelModuleValue);
    // set the module's name
    module->name = name;
    return (RaelValue*)module;
}

void module_set_key(RaelModuleValue *self, char *varname, RaelValue *value) {
    varmap_set(&((RaelValue*)self)->keys, varname, value, true, true);
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

    .as_bool = NULL, /* will always be true */
    .deallocator = (RaelSingleFunc)module_delete,
    .repr = (RaelSingleFunc)module_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL
};

RaelValue *rael_get_module_by_name(char *module_name) {
    if (strcmp(module_name, "Math") == 0) {
        return module_math_new();
    } else if (strcmp(module_name, "Types") == 0) {
        return module_types_new();
    } else {
        return NULL;
    }
}
