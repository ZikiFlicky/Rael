#ifndef RAEL_CFUNCS_H
#define RAEL_CFUNCS_H

#include "value.h"

/*
 * Defines two CFunc types:
 * the CFuncMethod type, which allows running C-defined methods,
 * and the regular CFunc type which allows running C-defined functions
 */

extern RaelTypeValue RaelCFuncMethodType;
extern RaelTypeValue RaelCFuncType;

/* define custom C function pointers as RaelRawCFunc */
typedef RaelValue* (*RaelRawCFunc)(RaelArgumentList*, RaelInterpreter*);

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

/* create a RaelValue with the type of CFunc */
RaelValue *cfunc_new(char *name, RaelRawCFunc func, size_t amount_params);

#endif /* RAEL_CFUNCS_H */