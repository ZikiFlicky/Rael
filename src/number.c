#include "number.h"
#include "interpreter.h"

#include <math.h>
#include <stdlib.h>

struct NumberExpr number_add(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float && b.is_float) {
        res.is_float = true;
        res.as._float = a.as._float + b.as._float;
    } else if (a.is_float && !b.is_float) {
        res.is_float = true;
        res.as._float = a.as._float + (double)b.as._int;
    } else if (!a.is_float && b.is_float) {
        res.is_float = true;
        res.as._float = (double)a.as._int + b.as._float;
    } else {
        res.is_float = false;
        res.as._int = a.as._int + b.as._int;
    }
    return res;
}

struct NumberExpr number_sub(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float && b.is_float) {
        res.is_float = true;
        res.as._float = a.as._float - b.as._float;
    } else if (a.is_float && !b.is_float) {
        res.is_float = true;
        res.as._float = a.as._float - (double)b.as._int;
    } else if (!a.is_float && b.is_float) {
        res.is_float = true;
        res.as._float = (double)a.as._int - b.as._float;
    } else {
        res.is_float = false;
        res.as._int = a.as._int - b.as._int;
    }
    return res;
}

struct NumberExpr number_mul(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float && b.is_float) {
        res.is_float = true;
        res.as._float = a.as._float * b.as._float;
    } else if (a.is_float && !b.is_float) {
        res.is_float = true;
        res.as._float = a.as._float * (double)b.as._int;
    } else if (!a.is_float && b.is_float) {
        res.is_float = true;
        res.as._float = (double)a.as._int * b.as._float;
    } else {
        res.is_float = false;
        res.as._int = a.as._int * b.as._int;
    }
    return res;
}

struct NumberExpr number_div(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float && b.is_float) {
        res.is_float = true;
        if (b.as._float == 0.f)
            runtime_error("Division by zero");
        res.as._float = a.as._float / b.as._float;
    } else if (a.is_float && !b.is_float) {
        res.is_float = true;
        if (b.as._int == 0)
            runtime_error("Division by zero");
        res.as._float = a.as._float / (double)b.as._int;
    } else if (!a.is_float && b.is_float) {
        res.is_float = true;
        if (b.as._float == 0.f)
            runtime_error("Division by zero");
        res.as._float = (double)a.as._int / b.as._float;
    } else {
        div_t division;
        if (b.as._int == 0)
            runtime_error("Division by zero");
        division = div(a.as._int, b.as._int);
        if (division.rem == 0) {
            res.is_float = false;
            res.as._int = division.quot;
        } else {
            res.is_float = true;
            res.as._float = a.as._int / b.as._int;
        }
    }
    return res;
}

struct NumberExpr number_eq(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    res.is_float = false;
    if (a.is_float && b.is_float) {
        res.as._float = a.as._float == b.as._float;
    } else if (a.is_float && !b.is_float) {
        res.as._float = a.as._float == (double)b.as._int;
    } else if (!a.is_float && b.is_float) {
        res.as._float = (double)a.as._int == b.as._float;
    } else {
        res.as._int = a.as._int == b.as._int;
    }
    return res;
}

struct NumberExpr number_smaller(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    res.is_float = false;
    if (a.is_float && b.is_float) {
        res.as._float = a.as._float < b.as._float;
    } else if (a.is_float && !b.is_float) {
        res.as._float = a.as._float < (double)b.as._int;
    } else if (!a.is_float && b.is_float) {
        res.as._float = (double)a.as._int < b.as._float;
    } else {
        res.as._int = a.as._int < b.as._int;
    }
    return res;
}

struct NumberExpr number_bigger(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    res.is_float = false;
    if (a.is_float && b.is_float) {
        res.as._float = a.as._float > b.as._float;
    } else if (a.is_float && !b.is_float) {
        res.as._float = a.as._float > (double)b.as._int;
    } else if (!a.is_float && b.is_float) {
        res.as._float = (double)a.as._int > b.as._float;
    } else {
        res.as._int = a.as._int > b.as._int;
    }
    return res;
}
