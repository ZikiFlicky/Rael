#ifndef RAEL_NUMBER_H
#define RAEL_NUMBER_H

#include <stdbool.h>

struct NumberExpr {
    bool is_float;
    union {
        int _int;
        double _float;
    } as;
};

struct NumberExpr number_add(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_sub(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_mul(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_div(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_eq(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_smaller(struct NumberExpr a, struct NumberExpr b);

struct NumberExpr number_bigger(struct NumberExpr a, struct NumberExpr b);

#endif // RAEL_NUMBER_H