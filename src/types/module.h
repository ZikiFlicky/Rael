#ifndef RAEL_MODULE_H
#define RAEL_MODULE_H

#include "value.h"
#include "varmap.h"

/* declare module type */
extern RaelTypeValue RaelModuleType;

typedef struct RaelModuleValue {
    RAEL_VALUE_BASE;
    char *name;
} RaelModuleValue;

/* create a RaelValue with the type of MethodFunc */
RaelValue *method_cfunc_new(RaelValue *method_self, char *name, RaelMethodFunc func);

/* return a new initialized RaelModuleValue */
RaelValue *module_new(char *name);

/* set a key inside of a module value */
void module_set_key(RaelModuleValue *self, char *varname, RaelValue *value);

#endif /* RAEL_MODULE_H */
