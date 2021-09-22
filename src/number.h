#ifndef RAEL_NUMBER_H
#define RAEL_NUMBER_H

#include "lexer.h"

#include <stdbool.h>

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

struct NumberExpr number_div(struct State state, struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_mod(struct State state, struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_neg(struct NumberExpr n);

struct NumberExpr number_eq(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_smaller(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_bigger(struct NumberExpr a, struct NumberExpr b);

#endif // RAEL_NUMBER_H