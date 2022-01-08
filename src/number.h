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

RaelValue *number_add(RaelNumberValue *self, RaelValue *value);

RaelValue *number_sub(RaelNumberValue *self, RaelValue *value);

RaelValue *number_mul(RaelNumberValue *self, RaelValue *value);

RaelValue *number_div(RaelNumberValue *self, RaelValue *value);

RaelValue *number_mod(RaelNumberValue *self, RaelValue *value);

RaelValue *number_neg(RaelNumberValue *self);

bool number_eq(RaelNumberValue *self, RaelNumberValue *value);

bool number_smaller(RaelNumberValue *self, RaelNumberValue *value);

bool number_bigger(RaelNumberValue *self, RaelNumberValue *value);

bool number_smaller_eq(RaelNumberValue *self, RaelNumberValue *value);

bool number_bigger_eq(RaelNumberValue *self, RaelNumberValue *value);

RaelValue *number_abs(RaelNumberValue *self);

RaelValue *number_floor(RaelNumberValue *self);

RaelValue *number_ceil(RaelNumberValue *self);

bool number_as_bool(RaelNumberValue *self);

bool number_from_string(char *string, size_t length, struct RaelHybridNumber *out);

void number_repr(RaelNumberValue *self);

#endif // RAEL_NUMBER_H