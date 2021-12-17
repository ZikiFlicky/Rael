#include "number.h"
#include "value.h"

#include <math.h>
#include <stdlib.h>
#include <ctype.h>

/* number to float */
RaelFloat number_to_float(RaelNumberValue *number) {
    return number->is_float ? number->as_float : (RaelFloat)number->as_int;
}

RaelInt number_to_int(RaelNumberValue *number) {
    if (number->is_float)
        return (RaelInt)number->as_float;
    else
        return number->as_int;
}

bool number_is_whole(RaelNumberValue *number) {
    if (number->is_float) {
        if (fmod(number->as_float, 1)) {
            return false;
        } else {
            return true;
        }
    } else { // an int is obviously a whole number; it's defined as a whole number
        return true;
    }
}

/* create a RaelValue from an int */
RaelNumberValue *number_newi(RaelInt i) {
    RaelNumberValue *number = RAEL_VALUE_NEW(ValueTypeNumber, RaelNumberValue);
    number->is_float = false;
    number->as_int = i;
    return number;
}

/* create a RaelValue from a float */
RaelNumberValue *number_newf(RaelFloat f) {
    RaelNumberValue *number = RAEL_VALUE_NEW(ValueTypeNumber, RaelNumberValue);
    number->is_float = true;
    number->as_float = f;
    return number;
}

RaelNumberValue *number_add(RaelNumberValue *a, RaelNumberValue *b) {
    if (a->is_float || b->is_float)
        return number_newf(number_to_float(a) + number_to_float(b));
    else
        return number_newi(a->as_int + b->as_int);
}

RaelNumberValue *number_sub(RaelNumberValue *a, RaelNumberValue *b) {
    if (a->is_float || b->is_float)
        return number_newf(number_to_float(a) - number_to_float(b));
    else
        return number_newi(a->as_int - b->as_int);
}

RaelNumberValue *number_mul(RaelNumberValue *a, RaelNumberValue *b) {
    if (a->is_float || b->is_float)
        return number_newf(number_to_float(a) * number_to_float(b));
    else
        return number_newi(a->as_int * b->as_int);
}

/* returns NULL on division by zero */
RaelNumberValue *number_div(RaelNumberValue *a, RaelNumberValue *b) {
    if (a->is_float || b->is_float) {
        if (number_to_float(b) == 0.0)
            return NULL;
        return number_newf(number_to_float(a) / number_to_float(b));
    } else {
        ldiv_t division;

        if (b->as_int == 0)
            return NULL;
        division = ldiv(a->as_int, b->as_int);
        if (division.rem == 0) {
            return number_newi(division.quot);
        } else {
            return number_newf(number_to_float(a) / number_to_float(b));
        }
    }
}

/* returns NULL on mod of zero */
RaelNumberValue *number_mod(RaelNumberValue *a, RaelNumberValue *b) {
    if (a->is_float || b->is_float) {
        if (number_to_float(b) == 0.0)
            return NULL;
        return number_newf(fmod(number_to_float(a), number_to_float(b)));
    } else {
        if (b->as_int == 0)
            return NULL;
        return number_newi(a->as_int % b->as_int);
    }
}

RaelNumberValue *number_neg(RaelNumberValue *n) {
    if (n->is_float)
        return number_newf(-n->as_float);
    else
        return number_newi(-n->as_int);
}

bool number_eq(RaelNumberValue *a, RaelNumberValue *b) {
    if (a->is_float || b->is_float)
        return number_to_float(a) == number_to_float(b);
    else
        return a->as_int == b->as_int;
}

// TODO: take all of those boolean-returning operations (>=, >, <, <=, =, !=) and make them actualy return booleans
RaelNumberValue *number_smaller(RaelNumberValue *a, RaelNumberValue *b) {
    if (a->is_float || b->is_float)
        return number_newi(number_to_float(a) < number_to_float(b));
    else
        return number_newi(a->as_int < b->as_int);
}

RaelNumberValue *number_bigger(RaelNumberValue *a, RaelNumberValue *b) {
    if (a->is_float || b->is_float)
        return number_newi(number_to_float(a) > number_to_float(b));
    else
        return number_newi(a->as_int > b->as_int);
}

RaelNumberValue *number_smaller_eq(RaelNumberValue *a, RaelNumberValue *b) {
    if (a->is_float || b->is_float)
        return number_newi(number_to_float(a) <= number_to_float(b));
    else
        return number_newi(a->as_int <= b->as_int);
}

RaelNumberValue *number_bigger_eq(RaelNumberValue *a, RaelNumberValue *b) {
    if (a->is_float || b->is_float)
        return number_newi(number_to_float(a) >= number_to_float(b));
    else
        return number_newi(a->as_int >= b->as_int);
}

bool number_as_bool(RaelNumberValue *n) {
    if (n->is_float)
        return n->as_float != 0.0;
    else
        return n->as_int != 0;
}

/* given a string + length, the value returns a RaelNumberValue parsing the string as a number.
   the value returns NULL on failure. */
RaelNumberValue *number_from_string(char *string, size_t length) {
    bool is_float = false;
    RaelInt decimal = 0;
    RaelFloat fractional;
    size_t since_dot;

    if (length == 0)
        return NULL;

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
            RaelFloat digit;

            if (!isdigit(string[i]))
                return NULL;
            digit = string[i] - '0';
            ++since_dot;
            for (size_t i = 0; i < since_dot; ++i)
                digit /= 10;
            fractional += digit;
        } else {
            decimal *= 10;
            if (!isdigit(string[i]))
                return NULL;
            decimal += string[i] - '0';
        }
    }

    if (is_float) {
        return number_newf((RaelFloat)decimal + fractional);
    } else {
        return number_newi(decimal);
    }
}

RaelNumberValue *number_abs(RaelNumberValue *number) {
    if (number->is_float)
        return number_newf(number->as_float > 0.0 ? number->as_float : -number->as_float);
    else
        return number_newi(number->as_int > 0 ? number->as_int : -number->as_int);
}

RaelNumberValue *number_floor(RaelNumberValue *number) {
    return number_newi(number_to_int(number));
}

RaelNumberValue *number_ceil(RaelNumberValue *number) {
    RaelInt n = number_to_int(number);
    // if the number is not an integer, round up (add 1)
    if (!number_is_whole(number))
        ++n;
    return number_newi(n);
}

void number_repr(RaelNumberValue *number) {
    if (number->is_float)
        printf("%.17g", number->as_float);
    else
        printf("%ld", number->as_int);
}
