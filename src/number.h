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

RaelNumberValue *number_newi(RaelInt i);

RaelNumberValue *number_newf(RaelFloat f);

RaelFloat number_to_float(RaelNumberValue *n);

RaelInt number_to_int(RaelNumberValue *number);

bool number_is_whole(RaelNumberValue *number);

RaelNumberValue *number_add(RaelNumberValue *a, RaelNumberValue *b);

RaelNumberValue *number_sub(RaelNumberValue *a, RaelNumberValue *b);

RaelNumberValue *number_mul(RaelNumberValue *a, RaelNumberValue *b);

RaelNumberValue *number_div(RaelNumberValue *a, RaelNumberValue *b);

RaelNumberValue *number_mod(RaelNumberValue *a, RaelNumberValue *b);

RaelNumberValue *number_neg(RaelNumberValue *n);

bool number_eq(RaelNumberValue *a, RaelNumberValue *b);

RaelNumberValue *number_smaller(RaelNumberValue *a, RaelNumberValue *b);

RaelNumberValue *number_bigger(RaelNumberValue *a, RaelNumberValue *b);

RaelNumberValue *number_smaller_eq(RaelNumberValue *a, RaelNumberValue *b);

RaelNumberValue *number_bigger_eq(RaelNumberValue *a, RaelNumberValue *b);

RaelNumberValue *number_abs(RaelNumberValue *number);

RaelNumberValue *number_floor(RaelNumberValue *number);

RaelNumberValue *number_ceil(RaelNumberValue *number);

bool number_as_bool(RaelNumberValue *n);

RaelNumberValue *number_from_string(char *string, size_t length);

void number_repr(RaelNumberValue *number);

#endif // RAEL_NUMBER_H