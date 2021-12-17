#ifndef RAEL_MODULE_H
#define RAEL_MODULE_H

#include "common.h"
#include "value.h"
#include "varmap.h"

typedef struct RaelArguments {
    size_t amount_arguments, amount_allocated;
    RaelValue **arguments;
} RaelArguments;

/* define custom C function pointers as RaelRawCFunc */
typedef RaelValue* (*RaelRawCFunc)(RaelArguments*);

typedef struct RaelExternalCFuncValue {
    RAEL_VALUE_BASE;
    char *name;
    RaelRawCFunc func;
    size_t amount_params;
} RaelExternalCFuncValue;

typedef struct RaelModuleValue {
    RAEL_VALUE_BASE;
    char *name;
    struct VariableMap vars;
} RaelModuleValue;

/* create a RaelValue with the type of CFunc */
RaelExternalCFuncValue *cfunc_new(char *name, RaelRawCFunc func, size_t amount_params);

/* call a cfunc value */
RaelValue *cfunc_call(RaelExternalCFuncValue *cfunc, RaelArguments *args, struct State error_place);

/* dealloc a cfunc value */
void cfunc_delete(RaelExternalCFuncValue *cfunc);

/* print a cfunc */
void cfunc_repr(RaelExternalCFuncValue *cfunc);

/* return a new initialized RaelModuleValue */
RaelModuleValue *module_new(char *name);

/* deallocate a module value */
void module_delete(RaelModuleValue *module);

/* set a key inside of a module value */
void module_set_key(RaelModuleValue *module, char *varname, RaelValue *value);

/* get a key from a module value */
RaelValue *module_get_key(RaelModuleValue *module, char *varname);

/* print a module */
void module_repr(RaelModuleValue *module);

/* get module value by name */
RaelModuleValue *rael_get_module_by_name(char *module_name);

/* create a RaelArguments */
void arguments_new(RaelArguments *out);

/* add an argument */
void arguments_add(RaelArguments *args, RaelValue *value);

/* returns the argument at the requested index, or NULL if the index is invalid */
RaelValue *arguments_get(RaelArguments *args, size_t idx);

/* this function returns the amount of arguments */
size_t arguments_get_amount(RaelArguments *args);

/* this function deallocates the arguments */
void arguments_delete(RaelArguments *args);

/* this function shrinks the size of the argument buffer */
void arguments_finalize(RaelArguments *args);

#endif /* RAEL_MODULE_H */
