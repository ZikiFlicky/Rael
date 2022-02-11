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
    bool have_max;
    size_t min_args, max_args;
} RaelCFuncValue;

typedef struct RaelCFuncMethodValue {
    RAEL_VALUE_BASE;
    RaelValue *method_self;
    RaelMethodFunc func;
    char *name;
    bool limit_arguments;
    // if limit_arguments is true the following members are going to be accessed
    size_t minimum_arguments, maximum_arguments; // includes maximum_arguments when checking
} RaelCFuncMethodValue;

/* Returns a new CFunc value that takes between min_args and max_args */
RaelValue *cfunc_ranged_new(char *name, RaelRawCFunc func, size_t min_args, size_t max_args);

/* Returns a new CFunc value that takes just one amount of args */
RaelValue *cfunc_new(char *name, RaelRawCFunc func, size_t amount_params);

/* Returns a new CFunc value that takes min_args or more arguments (most functions) */
RaelValue *cfunc_unlimited_new(char *name, RaelRawCFunc func, size_t min_args);

/* construct a RaelValue of type MethodFunc */
RaelValue *method_cfunc_new(RaelValue *method_self, MethodDecl *decl);

#endif /* RAEL_CFUNCS_H */
