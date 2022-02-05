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

/* return a new initialized RaelModuleValue */
RaelValue *module_new(char *name);

/* set a key inside of a module value */
void module_set_key(RaelModuleValue *self, char *varname, RaelValue *value);

#endif /* RAEL_MODULE_H */
