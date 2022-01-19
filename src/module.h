#ifndef RAEL_MODULE_H
#define RAEL_MODULE_H

#include "value.h"
#include "varmap.h"

/* define custom C function pointers as RaelRawCFunc */
typedef RaelValue* (*RaelRawCFunc)(RaelArgumentList*);

typedef struct RaelCFuncValue {
    RAEL_VALUE_BASE;
    char *name;
    RaelRawCFunc func;
    size_t amount_params;
} RaelCFuncValue;

typedef struct RaelCFuncMethodValue {
    RAEL_VALUE_BASE;
    RaelValue *method_self;
    RaelMethodFunc func;
    char *name;
} RaelCFuncMethodValue;

typedef struct RaelModuleValue {
    RAEL_VALUE_BASE;
    char *name;
} RaelModuleValue;

/* declare module-related types */
extern RaelTypeValue RaelModuleType;
extern RaelTypeValue RaelCFuncType;
extern RaelTypeValue RaelCFuncMethodType;

/* create a RaelValue with the type of CFunc */
RaelValue *cfunc_new(char *name, RaelRawCFunc func, size_t amount_params);

/* create a RaelValue with the type of MethodFunc */
RaelValue *method_cfunc_new(RaelValue *method_self, char *name, RaelMethodFunc func);

/* return a new initialized RaelModuleValue */
RaelValue *module_new(char *name);

/* set a key inside of a module value */
void module_set_key(RaelModuleValue *self, char *varname, RaelValue *value);

#endif /* RAEL_MODULE_H */
