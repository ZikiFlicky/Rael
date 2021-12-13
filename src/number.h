#ifndef RAEL_NUMBER_H
#define RAEL_NUMBER_H

#include "lexer.h"

#include <stdbool.h>

typedef struct RaelValue* RaelValue;

struct RaelNumberValue {
    bool is_float;
    union {
        int as_int;
        double as_float;
    };
};

struct RaelNumberValue numbervalue_newi(int i);

struct RaelNumberValue numbervalue_newf(double f);

RaelValue number_new(struct RaelNumberValue n);

RaelValue number_newi(int i);

RaelValue number_newf(double f);

double number_to_float(struct RaelNumberValue n);

int number_to_int(struct RaelNumberValue number);

bool number_is_whole(struct RaelNumberValue number);

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

struct RaelNumberValue number_abs(struct RaelNumberValue number);

struct RaelNumberValue number_floor(struct RaelNumberValue number);

struct RaelNumberValue number_ceil(struct RaelNumberValue number);

bool number_as_bool(struct RaelNumberValue n);

bool number_from_string(char *string, size_t length, struct RaelNumberValue *out_number);

void number_repr(struct RaelNumberValue number);

#endif // RAEL_NUMBER_H