#include "number.h"
#include "parser.h"

#include <math.h>
#include <stdlib.h>
#include <ctype.h>

void interpreter_error(struct Interpreter* const interpreter, struct State state, const char* const error_message);

static inline double number_as_float(struct RaelNumberValue n) {
    return n.is_float ? n.as_float : (double)n.as_int;
}

struct RaelNumberValue number_add(struct RaelNumberValue a, struct RaelNumberValue b) {
    struct RaelNumberValue res;
    if (a.is_float || b.is_float) {
        res.is_float = true;
        res.as_float = number_as_float(a) + number_as_float(b);
    } else {
        res.is_float = false;
        res.as_int = a.as_int + b.as_int;
    }
    return res;
}

struct RaelNumberValue number_sub(struct RaelNumberValue a, struct RaelNumberValue b) {
    struct RaelNumberValue res;
    if (a.is_float || b.is_float) {
        res.is_float = true;
        res.as_float = number_as_float(a) - number_as_float(b);
    } else {
        res.is_float = false;
        res.as_int = a.as_int - b.as_int;
    }
    return res;
}

struct RaelNumberValue number_mul(struct RaelNumberValue a, struct RaelNumberValue b) {
    struct RaelNumberValue res;
    if (a.is_float || b.is_float) {
        res.is_float = true;
        res.as_float = number_as_float(a) * number_as_float(b);
    } else {
        res.is_float = false;
        res.as_int = a.as_int * b.as_int;
    }
    return res;
}

struct RaelNumberValue number_div(struct Interpreter* const interpreter,
                             struct State state, struct RaelNumberValue a, struct RaelNumberValue b) {
    struct RaelNumberValue res;
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

struct RaelNumberValue number_mod(struct Interpreter* const interpreter, struct State state, struct RaelNumberValue a, struct RaelNumberValue b) {
    struct RaelNumberValue res;
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

struct RaelNumberValue number_neg(struct RaelNumberValue n) {
    if (n.is_float) {
        n.as_float = -n.as_float;
    } else {
        n.as_int = -n.as_int;
    }
    return n;
}

struct RaelNumberValue number_eq(struct RaelNumberValue a, struct RaelNumberValue b) {
    struct RaelNumberValue res;
    res.is_float = false;
    if (a.is_float || b.is_float) {
        res.as_int = number_as_float(a) == number_as_float(b);
    } else {
        res.as_int = a.as_int == b.as_int;
    }
    return res;
}

struct RaelNumberValue number_smaller(struct RaelNumberValue a, struct RaelNumberValue b) {
    struct RaelNumberValue res;
    res.is_float = false;
    if (a.is_float || b.is_float) {
        res.as_int = number_as_float(a) < number_as_float(b);
    } else {
        res.as_int = a.as_int < b.as_int;
    }
    return res;
}

struct RaelNumberValue number_bigger(struct RaelNumberValue a, struct RaelNumberValue b) {
    struct RaelNumberValue res;
    res.is_float = false;
    if (a.is_float || b.is_float) {
        res.as_int = number_as_float(a) > number_as_float(b);
    } else {
        res.as_int = a.as_int > b.as_int;
    }
    return res;
}

bool number_from_string(char *string, size_t length, struct RaelNumberValue *out_number) {
    bool is_float = false;
    int decimal = 0;
    double fractional;
    size_t since_dot;

    if (length == 0)
        return false;

    for (size_t i = 0; i < length; ++i) {
        if (string[i] == '.') {
            if (!is_float) {
                is_float = true;
                since_dot = 0;
                fractional = 0;
                continue;
            }
        }
        if (is_float) {
            double digit;

            if (!isdigit(string[i]))
                return false;
            digit = string[i] - '0';
            ++since_dot;
            for (size_t i = 0; i < since_dot; ++i)
                digit /= 10;
            fractional += digit;
        } else {
            decimal *= 10;
            if (!isdigit(string[i]))
                return false;
            decimal += string[i] - '0';
        }
    }

    if (is_float) {
        *out_number = (struct RaelNumberValue) {
            .is_float = true,
            .as_float = (double)decimal + fractional
        };
    } else {
        *out_number = (struct RaelNumberValue) {
            .is_float = false,
            .as_int = decimal
        };
    }

    return true;
}
