#ifndef RAEL_NUMBER_H
#define RAEL_NUMBER_H

#include "common.h"
#include "value.h"
#include "lexer.h"

#include <stdbool.h>

typedef struct RaelNumberValue {
    RAEL_VALUE_BASE;
    bool is_float;
    union {
        RaelInt as_int;
        RaelFloat as_float;
    };
} RaelNumberValue;

extern RaelTypeValue RaelNumberType;

RaelValue *number_newi(RaelInt i);

RaelValue *number_newf(RaelFloat f);

RaelFloat number_to_float(RaelNumberValue *self);

RaelInt number_to_int(RaelNumberValue *self);

bool number_is_whole(RaelNumberValue *self);

/* is number bigger than or equal to 0? */
bool number_positive(RaelNumberValue *self);

RaelValue *number_abs(RaelNumberValue *self);

RaelValue *number_floor(RaelNumberValue *self);

RaelValue *number_ceil(RaelNumberValue *self);

bool number_as_bool(RaelNumberValue *self);

bool number_from_string(char *string, size_t length, struct RaelHybridNumber *out);

void number_repr(RaelNumberValue *self);

#endif // RAEL_NUMBER_H