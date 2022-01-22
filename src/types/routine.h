#ifndef RAEL_ROUTINE_H
#define RAEL_ROUTINE_H

#include "value.h"

extern RaelTypeValue RaelRoutineType;

typedef struct RaelRoutineValue {
    RAEL_VALUE_BASE;
    struct Scope *scope;
    char **parameters;
    size_t amount_parameters;
    struct Instruction **block;
} RaelRoutineValue;

#endif /* RAEL_ROUTINE_H */