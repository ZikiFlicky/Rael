#include "number.h"

#include <math.h>
#include <stdlib.h>
#include <ctype.h>

/* number to float */
static inline double number_to_float(struct RaelNumberValue n) {
    return n.is_float ? n.as_float : (double)n.as_int;
}

/* create a new int number value with the value i */
struct RaelNumberValue number_newi(int i) {
    return (struct RaelNumberValue) {
        .is_float = false,
        .as_int = i
    };
}

/* create a new float number value with the value f */
struct RaelNumberValue number_newf(double f) {
    return (struct RaelNumberValue) {
        .is_float = true,
        .as_float = f
    };
}

struct RaelNumberValue number_add(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return number_newf(number_to_float(a) + number_to_float(b));
    else
        return number_newi(a.as_int + b.as_int);
}

struct RaelNumberValue number_sub(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return number_newf(number_to_float(a) - number_to_float(b));
    else
        return number_newi(a.as_int - b.as_int);
}

struct RaelNumberValue number_mul(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return number_newf(number_to_float(a) * number_to_float(b));
    else
        return number_newi(a.as_int * b.as_int);
}

bool number_div(struct RaelNumberValue a, struct RaelNumberValue b, struct RaelNumberValue *out) {
    if (a.is_float || b.is_float) {
        if (number_to_float(b) == 0.f)
            return false;
        *out = number_newf(number_to_float(a) / number_to_float(b));
    } else {
        div_t division;

        if (b.as_int == 0)
            return false;
        division = div(a.as_int, b.as_int);
        if (division.rem == 0) {
            *out = number_newi(division.quot);
        } else {
            *out = number_newf(number_to_float(a) / number_to_float(b));
        }
    }
    return true;
}

bool number_mod(struct RaelNumberValue a, struct RaelNumberValue b, struct RaelNumberValue *out) {
    if (a.is_float || b.is_float) {
        if (number_to_float(b) == 0.f)
            return false;
        *out = number_newf(fmod(number_to_float(a), number_to_float(b)));
    } else {
        if (b.as_int == 0)
            return false;
        *out = number_newi(a.as_int % b.as_int);
    }
    return true;
}

struct RaelNumberValue number_neg(struct RaelNumberValue n) {
    if (n.is_float)
        return number_newf(-n.as_float);
    else
        return number_newi(-n.as_int);
}

struct RaelNumberValue number_eq(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return number_newi(number_to_float(a) == number_to_float(b));
    else
        return number_newi(a.as_int == b.as_int);
}

struct RaelNumberValue number_smaller(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return number_newi(number_to_float(a) < number_to_float(b));
    else
        return number_newi(a.as_int < b.as_int);
}

struct RaelNumberValue number_bigger(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return number_newi(number_to_float(a) > number_to_float(b));
    else
        return number_newi(a.as_int > b.as_int);
}

struct RaelNumberValue number_smaller_eq(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return number_newi(number_to_float(a) <= number_to_float(b));
    else
        return number_newi(a.as_int <= b.as_int);
}

struct RaelNumberValue number_bigger_eq(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return number_newi(number_to_float(a) >= number_to_float(b));
    else
        return number_newi(a.as_int >= b.as_int);
}

bool number_as_bool(struct RaelNumberValue n) {
    if (n.is_float)
        return n.as_float != 0.f;
    else
        return n.as_int != 0;
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
        *out_number = number_newf((double)decimal + fractional);
    } else {
        *out_number = number_newi(decimal);
    }

    return true;
}
