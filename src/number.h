#ifndef RAEL_NUMBER_H
#define RAEL_NUMBER_H

#include "common.h"
#include "lexer.h"

#include <stdbool.h>

typedef struct RaelNumberValue {
    bool is_float;
    union {
        RaelInt as_int;
        RaelFloat as_float;
    };
} RaelNumberValue;

RaelNumberValue numbervalue_newi(RaelInt i);

RaelNumberValue numbervalue_newf(RaelFloat f);

RaelValue *number_new(RaelNumberValue n);

RaelValue *number_newi(RaelInt i);

RaelValue *number_newf(RaelFloat f);

RaelFloat number_to_float(RaelNumberValue n);

RaelInt number_to_int(RaelNumberValue number);

bool number_is_whole(RaelNumberValue number);

RaelNumberValue number_add(RaelNumberValue a, RaelNumberValue b);

RaelNumberValue number_sub(RaelNumberValue a, RaelNumberValue b);

RaelNumberValue number_mul(RaelNumberValue a, RaelNumberValue b);

bool number_div(RaelNumberValue a, RaelNumberValue b, RaelNumberValue *out);

bool number_mod(RaelNumberValue a, RaelNumberValue b, RaelNumberValue *out);

RaelNumberValue number_neg(RaelNumberValue n);

RaelNumberValue number_eq(RaelNumberValue a, RaelNumberValue b);

RaelNumberValue number_smaller(RaelNumberValue a, RaelNumberValue b);

RaelNumberValue number_bigger(RaelNumberValue a, RaelNumberValue b);

RaelNumberValue number_smaller_eq(RaelNumberValue a, RaelNumberValue b);

RaelNumberValue number_bigger_eq(RaelNumberValue a, RaelNumberValue b);

RaelNumberValue number_abs(RaelNumberValue number);

RaelNumberValue number_floor(RaelNumberValue number);

RaelNumberValue number_ceil(RaelNumberValue number);

bool number_as_bool(RaelNumberValue n);

bool number_from_string(char *string, size_t length, RaelNumberValue *out_number);

void number_repr(RaelNumberValue number);

#endif // RAEL_NUMBER_H