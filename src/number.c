#include "number.h"
#include "parser.h"

#include <math.h>
#include <stdlib.h>

void interpreter_error(struct Interpreter* const interpreter, struct State state, const char* const error_message);

static inline double number_as_float(struct NumberExpr n) {
    return n.is_float ? n.as_float : (double)n.as_int;
}

struct NumberExpr number_add(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float || b.is_float) {
        res.is_float = true;
        res.as_float = number_as_float(a) + number_as_float(b);
    } else {
        res.is_float = false;
        res.as_int = a.as_int + b.as_int;
    }
    return res;
}

struct NumberExpr number_sub(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float || b.is_float) {
        res.is_float = true;
        res.as_float = number_as_float(a) - number_as_float(b);
    } else {
        res.is_float = false;
        res.as_int = a.as_int - b.as_int;
    }
    return res;
}

struct NumberExpr number_mul(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float || b.is_float) {
        res.is_float = true;
        res.as_float = number_as_float(a) * number_as_float(b);
    } else {
        res.is_float = false;
        res.as_int = a.as_int * b.as_int;
    }
    return res;
}

struct NumberExpr number_div(struct Interpreter* const interpreter,
                             struct State state, struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float || b.is_float) {
        res.is_float = true;
        if (number_as_float(b) == 0.f)
            interpreter_error(interpreter, state, "Division by zero");
        res.as_float = number_as_float(a) / number_as_float(b);
    } else {
        div_t division;

        if (b.as_int == 0)
            interpreter_error(interpreter, state, "Division by zero");
        division = div(a.as_int, b.as_int);
        if (division.rem == 0) {
            res.is_float = false;
            res.as_int = division.quot;
        } else {
            res.is_float = true;
            res.as_float = (double)a.as_int / (double)b.as_int;
        }
    }
    return res;
}

struct NumberExpr number_mod(struct Interpreter* const interpreter, struct State state, struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    if (a.is_float || b.is_float) {
        res.is_float = true;
        if (number_as_float(b) == 0.f)
            interpreter_error(interpreter, state, "Division by zero");
        res.as_float = fmod(number_as_float(a), number_as_float(b));
    } else {
        res.is_float = false;
        if (b.as_int == 0)
            interpreter_error(interpreter, state, "Division by zero");
        res.as_int = a.as_int % b.as_int;
    }
    return res;
}

struct NumberExpr number_neg(struct NumberExpr n) {
    if (n.is_float) {
        n.as_float = -n.as_float;
    } else {
        n.as_int = -n.as_int;
    }
    return n;
}

struct NumberExpr number_eq(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    res.is_float = false;
    if (a.is_float || b.is_float) {
        res.as_int = number_as_float(a) == number_as_float(b);
    } else {
        res.as_int = a.as_int == b.as_int;
    }
    return res;
}

struct NumberExpr number_smaller(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    res.is_float = false;
    if (a.is_float || b.is_float) {
        res.as_int = number_as_float(a) < number_as_float(b);
    } else {
        res.as_int = a.as_int < b.as_int;
    }
    return res;
}

struct NumberExpr number_bigger(struct NumberExpr a, struct NumberExpr b) {
    struct NumberExpr res;
    res.is_float = false;
    if (a.is_float || b.is_float) {
        res.as_int = number_as_float(a) > number_as_float(b);
    } else {
        res.as_int = a.as_int > b.as_int;
    }
    return res;
}
