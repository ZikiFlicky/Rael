#include "module.h"
#include "value.h"
#include "varmap.h"

#include <stddef.h>
#include <string.h>
#include <math.h>
#include <assert.h>

RaelModuleValue *module_math_new(void);

RaelExternalCFuncValue *cfunc_new(char *name, RaelValue* (*func)(RaelArguments *), size_t amount_params) {
    RaelExternalCFuncValue *cfunc = RAEL_VALUE_NEW(ValueTypeCFunc, RaelExternalCFuncValue);
    cfunc->name = name;
    cfunc->func = func;
    cfunc->amount_params = amount_params;
    return cfunc;
}

RaelValue *cfunc_call(RaelExternalCFuncValue *cfunc, RaelArguments *args, struct State error_place) {
    RaelValue *return_value;
    if (arguments_get_amount(args) != cfunc->amount_params)
        return NULL;
    return_value = cfunc->func(args);
    // if you received an error, add a state to it because it wasn't set beforehand
    if (value_is_blame(return_value)) {
        blame_add_state((RaelBlameValue*)return_value, error_place);
    }
    return return_value;
}

void cfunc_delete(RaelExternalCFuncValue *cfunc) {
    free(cfunc->name);
}

void cfunc_repr(RaelExternalCFuncValue *cfunc) {
    printf("cfunc(:%s, %zu)", cfunc->name, cfunc->amount_params);
}

RaelModuleValue *module_new(char *name) {
    RaelModuleValue *module = RAEL_VALUE_NEW(ValueTypeModule, RaelModuleValue);
    // initialize the module's varmap
    varmap_new(&module->vars);
    // set the module's name
    module->name = name;
    return module;
}

void module_set_key(RaelModuleValue *module, char *varname, RaelValue *value) {
    varmap_set(&module->vars, varname, value, true, true);
}

void module_delete(RaelModuleValue *module) {
    free(module->name);
    varmap_delete(&module->vars);
}

void module_repr(RaelModuleValue *module) {
    printf("module(:%s)", module->name);
}

RaelValue *module_get_key(RaelModuleValue *module, char *varname) {
    RaelValue *value = varmap_get(&module->vars, varname);
    // if you couldn't find the key, return a Void
    if (!value)
        value = RAEL_VALUE_NEW(ValueTypeVoid, RaelValue);
    return value;
}

RaelModuleValue *rael_get_module_by_name(char *module_name) {
    if (strcmp(module_name, "Math") == 0) {
        return module_math_new();
    } else {
        return NULL;
    }
}

void arguments_new(RaelArguments *out) {
    out->amount_allocated = 0;
    out->amount_arguments = 0;
    out->arguments = NULL;
}

void arguments_add(RaelArguments *args, RaelValue *value) {
    if (args->amount_allocated == 0)
        args->arguments = malloc((args->amount_allocated = 4) * sizeof(RaelValue*));
    else if (args->amount_arguments >= args->amount_allocated)
        args->arguments = realloc(args->arguments, (args->amount_allocated += 4) * sizeof(RaelValue*));
    args->arguments[args->amount_arguments++] = value;
}

RaelValue *arguments_get(RaelArguments *args, size_t idx) {
    if (idx >= args->amount_arguments)
        return NULL;
    return args->arguments[idx];
}

size_t arguments_get_amount(RaelArguments *args) {
    return args->amount_arguments;
}

void arguments_delete(RaelArguments *args) {
    for (size_t i = 0; i < args->amount_arguments; ++i) {
        value_deref(arguments_get(args, i));
    }
    free(args->arguments);
}

/* this function shrinks the size of the argument buffer */
void arguments_finalize(RaelArguments *args) {
    args->arguments = realloc(args->arguments,
                    (args->amount_allocated = args->amount_arguments) * sizeof(RaelValue*));
}
