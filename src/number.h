#ifndef RAEL_NUMBER_H
#define RAEL_NUMBER_H

#include "lexer.h"

#include <stdbool.h>

struct RaelNumberValue {
    bool is_float;
    union {
        int as_int;
        double as_float;
    };
};

struct RaelNumberValue number_newi(int i);

struct RaelNumberValue number_newf(double f);

struct RaelNumberValue number_add(struct RaelNumberValue a, struct RaelNumberValue b);

struct RaelNumberValue number_sub(struct RaelNumberValue a, struct RaelNumberValue b);

struct RaelNumberValue number_mul(struct RaelNumberValue a, struct RaelNumberValue b);

bool number_div(struct RaelNumberValue a, struct RaelNumberValue b, struct RaelNumberValue *out);

bool number_mod(struct RaelNumberValue a, struct RaelNumberValue b, struct RaelNumberValue *out);

struct RaelNumberValue number_neg(struct RaelNumberValue n);

struct RaelNumberValue number_eq(struct RaelNumberValue a, struct RaelNumberValue b);

struct RaelNumberValue number_smaller(struct RaelNumberValue a, struct RaelNumberValue b);

struct RaelNumberValue number_bigger(struct RaelNumberValue a, struct RaelNumberValue b);

struct RaelNumberValue number_smaller_eq(struct RaelNumberValue a, struct RaelNumberValue b);

struct RaelNumberValue number_bigger_eq(struct RaelNumberValue a, struct RaelNumberValue b);

bool number_as_bool(struct RaelNumberValue n);

bool number_from_string(char *string, size_t length, struct RaelNumberValue *out_number);

#endif // RAEL_NUMBER_H