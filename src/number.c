#include "number.h"
#include "value.h"

#include <math.h>
#include <stdlib.h>
#include <ctype.h>

/* number to float */
double number_to_float(struct RaelNumberValue n) {
    return n.is_float ? n.as_float : (double)n.as_int;
}

int number_to_int(struct RaelNumberValue number) {
    if (number.is_float)
        return (int)number.as_float;
    else
        return number.as_int;
}

bool number_is_whole(struct RaelNumberValue number) {
    if (number.is_float) {
        if (fmod(number.as_float, 1)) {
            return false;
        } else {
            return true;
        }
    } else {
        return true;
    }
}

/* create a RaelNumberValue from an int */
struct RaelNumberValue numbervalue_newi(int i) {
    return (struct RaelNumberValue) {
        .is_float = false,
        .as_int = i
    };
}

/* create a RaelNumberValue from a float */
struct RaelNumberValue numbervalue_newf(double f) {
    return (struct RaelNumberValue) {
        .is_float = true,
        .as_float = f
    };
}

/* create a RaelValue from a struct RaelNumberValue */
RaelValue number_new(struct RaelNumberValue n) {
    RaelValue number = value_create(ValueTypeNumber);
    number->as_number = n;
    return number;
}

/* create a RaelValue from an int */
RaelValue number_newi(int i) {
    return number_new(numbervalue_newi(i));
}

/* create a RaelValue from a float */
RaelValue number_newf(double f) {
    return number_new(numbervalue_newf(f));
}

struct RaelNumberValue number_add(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return numbervalue_newf(number_to_float(a) + number_to_float(b));
    else
        return numbervalue_newi(a.as_int + b.as_int);
}

struct RaelNumberValue number_sub(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return numbervalue_newf(number_to_float(a) - number_to_float(b));
    else
        return numbervalue_newi(a.as_int - b.as_int);
}

struct RaelNumberValue number_mul(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return numbervalue_newf(number_to_float(a) * number_to_float(b));
    else
        return numbervalue_newi(a.as_int * b.as_int);
}

/* returns false on division by zero */
bool number_div(struct RaelNumberValue a, struct RaelNumberValue b, struct RaelNumberValue *out) {
    if (a.is_float || b.is_float) {
        if (number_to_float(b) == 0.f)
            return false;
        *out = numbervalue_newf(number_to_float(a) / number_to_float(b));
    } else {
        div_t division;

        if (b.as_int == 0)
            return false;
        division = div(a.as_int, b.as_int);
        if (division.rem == 0) {
            *out = numbervalue_newi(division.quot);
        } else {
            *out = numbervalue_newf(number_to_float(a) / number_to_float(b));
        }
    }
    return true;
}

/* returns false on mod of zero */
bool number_mod(struct RaelNumberValue a, struct RaelNumberValue b, struct RaelNumberValue *out) {
    if (a.is_float || b.is_float) {
        if (number_to_float(b) == 0.f)
            return false;
        *out = numbervalue_newf(fmod(number_to_float(a), number_to_float(b)));
    } else {
        if (b.as_int == 0)
            return false;
        *out = numbervalue_newi(a.as_int % b.as_int);
    }
    return true;
}

struct RaelNumberValue number_neg(struct RaelNumberValue n) {
    if (n.is_float)
        return numbervalue_newf(-n.as_float);
    else
        return numbervalue_newi(-n.as_int);
}

struct RaelNumberValue number_eq(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return numbervalue_newi(number_to_float(a) == number_to_float(b));
    else
        return numbervalue_newi(a.as_int == b.as_int);
}

struct RaelNumberValue number_smaller(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return numbervalue_newi(number_to_float(a) < number_to_float(b));
    else
        return numbervalue_newi(a.as_int < b.as_int);
}

struct RaelNumberValue number_bigger(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return numbervalue_newi(number_to_float(a) > number_to_float(b));
    else
        return numbervalue_newi(a.as_int > b.as_int);
}

struct RaelNumberValue number_smaller_eq(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return numbervalue_newi(number_to_float(a) <= number_to_float(b));
    else
        return numbervalue_newi(a.as_int <= b.as_int);
}

struct RaelNumberValue number_bigger_eq(struct RaelNumberValue a, struct RaelNumberValue b) {
    if (a.is_float || b.is_float)
        return numbervalue_newi(number_to_float(a) >= number_to_float(b));
    else
        return numbervalue_newi(a.as_int >= b.as_int);
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
        *out_number = numbervalue_newf((double)decimal + fractional);
    } else {
        *out_number = numbervalue_newi(decimal);
    }

    return true;
}

struct RaelNumberValue number_abs(struct RaelNumberValue number) {
    if (number.is_float)
        return numbervalue_newf(number.as_float > 0 ? number.as_float : -number.as_float);
    else
        return numbervalue_newi(number.as_int > 0 ? number.as_int : -number.as_int);
}

struct RaelNumberValue number_floor(struct RaelNumberValue number) {
    return numbervalue_newi(number_to_int(number));
}

struct RaelNumberValue number_ceil(struct RaelNumberValue number) {
    int n = number_to_int(number);
    // if the number is not an integer, round up (add 1)
    if (!number_is_whole(number))
        ++n;
    return numbervalue_newi(n);
}

void number_repr(struct RaelNumberValue number) {
    if (number.is_float)
        printf("%.17g", number.as_float);
    else
        printf("%d", number.as_int);
}
