#ifndef RAEL_NUMBER_H
#define RAEL_NUMBER_H

#include "lexer.h"

#include <stdbool.h>

struct Interpreter;
struct Scope;

struct NumberExpr {
    bool is_float;
    union {
        int as_int;
        double as_float;
    };
};

struct NumberExpr number_add(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_sub(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_mul(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_div(struct Interpreter* const interpreter, struct State state,
                             struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_mod(struct Interpreter* const interpreter, struct State state,
                             struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_neg(struct NumberExpr n);

struct NumberExpr number_eq(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_smaller(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_bigger(struct NumberExpr a, struct NumberExpr b);

bool number_from_string(char *string, size_t length, struct NumberExpr *out_number);

#endif // RAEL_NUMBER_H