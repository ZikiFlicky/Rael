#include "rael.h"

#include "module.h"

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

    .callable_info = NULL,
    .constructor_info = NULL,
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
