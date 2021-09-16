#include "number.h"
#include "parser.h"

#include <math.h>
#include <stdlib.h>

void rael_error(struct State state, const char* const error_message);

struct NumberExpr number_add(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float && b.is_float) {
        res.is_float = true;
        res.as_float = a.as_float + b.as_float;
    } else if (a.is_float && !b.is_float) {
        res.is_float = true;
        res.as_float = a.as_float + (double)b.as_int;
    } else if (!a.is_float && b.is_float) {
        res.is_float = true;
        res.as_float = (double)a.as_int + b.as_float;
    } else {
        res.is_float = false;
        res.as_int = a.as_int + b.as_int;
    }
    return res;
}

struct NumberExpr number_sub(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float && b.is_float) {
        res.is_float = true;
        res.as_float = a.as_float - b.as_float;
    } else if (a.is_float && !b.is_float) {
        res.is_float = true;
        res.as_float = a.as_float - (double)b.as_int;
    } else if (!a.is_float && b.is_float) {
        res.is_float = true;
        res.as_float = (double)a.as_int - b.as_float;
    } else {
        res.is_float = false;
        res.as_int = a.as_int - b.as_int;
    }
    return res;
}

struct NumberExpr number_mul(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float && b.is_float) {
        res.is_float = true;
        res.as_float = a.as_float * b.as_float;
    } else if (a.is_float && !b.is_float) {
        res.is_float = true;
        res.as_float = a.as_float * (double)b.as_int;
    } else if (!a.is_float && b.is_float) {
        res.is_float = true;
        res.as_float = (double)a.as_int * b.as_float;
    } else {
        res.is_float = false;
        res.as_int = a.as_int * b.as_int;
    }
    return res;
}

struct NumberExpr number_div(struct State state, struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float && b.is_float) {
        res.is_float = true;
        if (b.as_float == 0.f)
            rael_error(state, "Division by zero");
        res.as_float = a.as_float / b.as_float;
    } else if (a.is_float && !b.is_float) {
        res.is_float = true;
        if (b.as_int == 0)
            rael_error(state, "Division by zero");
        res.as_float = a.as_float / (double)b.as_int;
    } else if (!a.is_float && b.is_float) {
        res.is_float = true;
        if (b.as_float == 0.f)
            rael_error(state, "Division by zero");
        res.as_float = (double)a.as_int / b.as_float;
    } else {
        div_t division;
        if (b.as_int == 0)
            rael_error(state, "Division by zero");
        division = div(a.as_int, b.as_int);
        if (division.rem == 0) {
            res.is_float = false;
            res.as_int = division.quot;
        } else {
            res.is_float = true;
            res.as_float = a.as_int / b.as_int;
        }
    }
    return res;
}

struct NumberExpr number_neg(struct NumberExpr n) {
    struct NumberExpr res;
    if (n.is_float) {
        res.is_float = true;
        res.as_float = -n.as_float;
    } else {
        res.is_float = false;
        res.as_int = -n.as_int;
    }
    return res;
}

struct NumberExpr number_eq(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    res.is_float = false;
    if (a.is_float && b.is_float) {
        res.as_int = a.as_float == b.as_float;
    } else if (a.is_float && !b.is_float) {
        res.as_int = a.as_float == (double)b.as_int;
    } else if (!a.is_float && b.is_float) {
        res.as_int = (double)a.as_int == b.as_float;
    } else {
        res.as_int = a.as_int == b.as_int;
    }
    return res;
}

struct NumberExpr number_smaller(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    res.is_float = false;
    if (a.is_float && b.is_float) {
        res.as_int = a.as_float < b.as_float;
    } else if (a.is_float && !b.is_float) {
        res.as_int = a.as_float < (double)b.as_int;
    } else if (!a.is_float && b.is_float) {
        res.as_int = (double)a.as_int < b.as_float;
    } else {
        res.as_int = a.as_int < b.as_int;
    }
    return res;
}

struct NumberExpr number_bigger(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    res.is_float = false;
    if (a.is_float && b.is_float) {
        res.as_int = a.as_float > b.as_float;
    } else if (a.is_float && !b.is_float) {
        res.as_int = a.as_float > (double)b.as_int;
    } else if (!a.is_float && b.is_float) {
        res.as_int = (double)a.as_int > b.as_float;
    } else {
        res.as_int = a.as_int > b.as_int;
    }
    return res;
}
