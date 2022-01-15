#include "rael.h"

bool number_validate(RaelValue *self) {
    return self->type == &RaelNumberType;
}

/* number to float */
RaelFloat number_to_float(RaelNumberValue *self) {
    return self->is_float ? self->as_float : (RaelFloat)self->as_int;
}

RaelInt number_to_int(RaelNumberValue *self) {
    if (self->is_float)
        return (RaelInt)self->as_float;
    else
        return self->as_int;
}

bool number_is_whole(RaelNumberValue *self) {
    if (self->is_float) {
        if (fmod(self->as_float, 1)) {
            return false;
        } else {
            return true;
        }
    } else { // an int is obviously a whole number; it's defined as a whole number
        return true;
    }
}

/* create a RaelValue from an int */
RaelValue *number_newi(RaelInt i) {
    RaelNumberValue *number = RAEL_VALUE_NEW(RaelNumberType, RaelNumberValue);
    number->is_float = false;
    number->as_int = i;
    return (RaelValue*)number;
}

/* create a RaelValue from a float */
RaelValue *number_newf(RaelFloat f) {
    RaelNumberValue *number = RAEL_VALUE_NEW(RaelNumberType, RaelNumberValue);
    number->is_float = true;
    number->as_float = f;
    return (RaelValue*)number;
}

/* number + string */
static RaelValue *number_add_string(RaelNumberValue *number, RaelStringValue *string) {
    RaelInt n;
    char *source;
    size_t str_len;

    if (!number_is_whole(number)) {
        return BLAME_NEW_CSTR("Expected the number to be a whole number");
    }
    n = number_to_int(number);
    if (!rael_int_in_range_of_char(n)) {
        return BLAME_NEW_CSTR("Expected the number to be in ascii");
    }

    // get length of rhs
    str_len = string_length(string);
    source = malloc((str_len + 1) * sizeof(char));

    source[0] = (char)n;
    strncpy(source + 1, string->source, str_len);
    return (RaelValue*)string_new_pure(source, str_len + 1, true);
}

RaelValue *number_add(RaelNumberValue *self, RaelValue *value) {
    if (number_validate(value)) {
        RaelNumberValue *number = (RaelNumberValue*)value;
        if (self->is_float || number->is_float)
            return number_newf(number_to_float(self) + number_to_float(number));
        else
            return number_newi(self->as_int + number->as_int);
    } else if (value->type == &RaelStringType) {
        return number_add_string(self, (RaelStringValue*)value);
    } else {
        return NULL;
    }
}

RaelValue *number_sub(RaelNumberValue *self, RaelValue *value) {
    if (number_validate(value)) {
        RaelNumberValue *number = (RaelNumberValue*)value;
        if (self->is_float || self->is_float)
            return number_newf(number_to_float(self) - number_to_float(number));
        else
            return number_newi(self->as_int - number->as_int);
    } else {
        return NULL;
    }
}

RaelValue *number_mul(RaelNumberValue *self, RaelValue *value) {
    // TODO: add number * string
    if (number_validate(value)) {
        RaelNumberValue *number = (RaelNumberValue*)value;
        if (self->is_float || self->is_float)
            return number_newf(number_to_float(self) * number_to_float(number));
        else
            return number_newi(self->as_int * number->as_int);
    } else {
        return NULL;
    }
}

RaelValue *number_div(RaelNumberValue *self, RaelValue *value) {
    if (number_validate(value)) {
        RaelNumberValue *number = (RaelNumberValue*)value;
        if (self->is_float || number->is_float) {
            if (number_to_float(number) == 0.0)
                return BLAME_NEW_CSTR("Division by zero");
            return number_newf(number_to_float(self) / number_to_float(number));
        } else {
            ldiv_t division;

            if (number->as_int == 0)
                return BLAME_NEW_CSTR("Division by zero");
            division = ldiv(self->as_int, number->as_int);
            if (division.rem == 0) {
                return number_newi(division.quot);
            } else {
                return number_newf(number_to_float(self) / number_to_float(number));
            }
        }
    } else {
        return NULL; // invalid operation between types
    }
}

RaelValue *number_mod(RaelNumberValue *self, RaelValue *value) {
    if (number_validate(value)) {
        RaelNumberValue *number = (RaelNumberValue*)value;

        // the double modulo calculations are there to get rid of the negative part
        // in calculations like `-13 % 3`, which should give 2 but gives -1 instead
        if (self->is_float || number->is_float) {
            RaelFloat lhs, rhs, res;
            if (number_to_float(number) == 0.0)
                return BLAME_NEW_CSTR("Division by zero");
            lhs = number_to_float(self);
            rhs = number_to_float(number);
            // calculate the result of the operation
            res = fmod(fmod(lhs, rhs) + rhs, rhs);
            return number_newf(res);
        } else {
            RaelInt lhs, rhs, res;
            if (number->as_int == 0)
                return BLAME_NEW_CSTR("Division by zero");
            lhs = self->as_int;
            rhs = number->as_int;
            // calculate the result of the operation
            res = ((lhs % rhs) + rhs) % rhs;
            return number_newi(res);
        }
    } else {
        return NULL; // invalid operation between types
    }
}

RaelValue *number_neg(RaelNumberValue *self) {
    if (self->is_float)
        return number_newf(-self->as_float);
    else
        return number_newi(-self->as_int);
}

bool number_eq(RaelNumberValue *self, RaelNumberValue *value) {
    if (self->is_float || value->is_float)
        return number_to_float(self) == number_to_float(value);
    else
        return self->as_int == value->as_int;
}

bool number_smaller(RaelNumberValue *self, RaelNumberValue *value) {
    if (self->is_float || value->is_float)
        return number_to_float(self) < number_to_float(value);
    else
        return self->as_int < value->as_int;
}

bool number_bigger(RaelNumberValue *self, RaelNumberValue *value) {
    if (self->is_float || value->is_float)
        return number_to_float(self) > number_to_float(value);
    else
        return self->as_int > value->as_int;
}

bool number_smaller_eq(RaelNumberValue *self, RaelNumberValue *value) {
    if (self->is_float || value->is_float)
        return number_to_float(self) <= number_to_float(value);
    else
        return self->as_int <= value->as_int;
}

bool number_bigger_eq(RaelNumberValue *self, RaelNumberValue *value) {
    if (self->is_float || value->is_float)
        return number_to_float(self) >= number_to_float(value);
    else
        return self->as_int >= value->as_int;
}

bool number_as_bool(RaelNumberValue *self) {
    if (self->is_float)
        return self->as_float != 0.0;
    else
        return self->as_int != 0;
}

/*
 * given a string + length, the function parses a number into `out`.
 * the function returns false on failure and true on success
 */
bool number_from_string(char *string, size_t length, struct RaelHybridNumber *out) {
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
        out->is_float = true;
        out->as_float = (RaelFloat)decimal + fractional;
    } else {
        out->is_float = false;
        out->as_int = decimal;
    }
    return true;
}

RaelValue *number_abs(RaelNumberValue *self) {
    if (self->is_float)
        return number_newf(self->as_float > 0.0 ? self->as_float : -self->as_float);
    else
        return number_newi(self->as_int > 0 ? self->as_int : -self->as_int);
}

RaelValue *number_floor(RaelNumberValue *self) {
    return number_newi(number_to_int(self));
}

RaelValue *number_ceil(RaelNumberValue *self) {
    RaelInt n;

    n = number_to_int(self);
    // if the number is not whole, round up (add 1)
    if (!number_is_whole(self))
        ++n;
    return number_newi(n);
}

bool number_positive(RaelNumberValue *self) {
    if (self->is_float) {
        return self->as_float >= 0;
    } else {
        return self->as_int >= 0;
    }
}

RaelValue *number_cast(RaelNumberValue *self, RaelTypeValue *type) {
    if (type == &RaelStringType) {
        // FIXME: make float conversions more accurate
        bool is_negative;
        RaelInt decimal;
        RaelFloat fractional;
        size_t allocated, idx = 0;
        char *source = malloc((allocated = 10) * sizeof(char));
        size_t middle;

        // check if the number is negative and set a flag
        if (self->is_float) {
            is_negative = self->as_float < 0.0;
            fractional = fmod(rael_float_abs(self->as_float), 1);
        } else {
            is_negative = self->as_int < 0;
            fractional = 0.0;
        }
        // set the decimal value to the absolute whole value of the number
        decimal = rael_int_abs(number_to_int((RaelNumberValue*)self));

        do {
            if (idx >= allocated)
                source = realloc(source, (allocated += 10) * sizeof(char));
            source[idx++] = '0' + (decimal % 10);
            decimal = (decimal - decimal % 10) / 10;
        } while (decimal);

        // minimize size, and add a '-' to be flipped at the end
        if (is_negative) {
            source = realloc(source, (allocated = idx + 1) * sizeof(char));
            source[idx++] = '-';
        } else {
            source = realloc(source, (allocated = idx) * sizeof(char));
        }

        middle = (idx - idx % 2) / 2;

        // flip string
        for (size_t i = 0; i < middle; ++i) {
            char c = source[i];
            source[i] = source[idx - i - 1];
            source[idx - i - 1] = c;
        }

        if (fractional) {
            size_t characters_added = 0;
            source = realloc(source, (allocated += 4) * sizeof(char));
            source[idx++] = '.';
            fractional *= 10;
            do {
                RaelFloat new_fractional = fmod(fractional, 1);
                if (idx >= allocated)
                    source = realloc(source, (allocated += 4) * sizeof(char));
                source[idx++] = '0' + (RaelInt)(fractional - new_fractional);
                fractional = new_fractional;
            } while (++characters_added < 14 && (RaelInt)(fractional *= 10));

            source = realloc(source, (allocated = idx) * sizeof(char));
        }
        return string_new_pure(source, allocated, true);
    } else {
        return NULL;
    }
}

void number_repr(RaelNumberValue *self) {
    if (self->is_float)
        printf("%.17g", self->as_float);
    else
        printf("%ld", self->as_int);
}

/*
 * modulo, but the normal C way of doing it:
 * (-4):signedMod(3) = -1, instead of the much more straightforward
 * -4 % 3 = 2
 */
RaelValue *number_method_signedMod(RaelNumberValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;

    (void)interpreter;

    if (arguments_amount(args) != 1) {
        return BLAME_NEW_CSTR("Method takes only one argument");
    }

    // get the first argument
    arg1 = arguments_get(args, 0);

    if (number_validate(arg1)) {
        RaelNumberValue *number = (RaelNumberValue*)arg1;

        // if any of them is a float, do a float calculation,
        // otherwise do a regular '%' operation
        if (self->is_float || number->is_float) {
            RaelFloat lhs, rhs, res;
            if (number_to_float(number) == 0.0)
                return BLAME_NEW_CSTR_ST("Division by zero", *arguments_state(args, 0));
            lhs = number_to_float(self);
            rhs = number_to_float(number);
            // calculate the result of the operation
            res = fmod(lhs, rhs);
            return number_newf(res);
        } else {
            RaelInt lhs, rhs, res;
            if (number->as_int == 0)
                return BLAME_NEW_CSTR_ST("Division by zero", *arguments_state(args, 0));
            lhs = self->as_int;
            rhs = number->as_int;
            // calculate the result of the operation
            res = lhs % rhs;
            return number_newi(res);
        }
    } else {
        // didn't get a number as the second parameter
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
}

/*
 * The method no arguments and returns a new string with
 * the number as it's only character.
 * For example:
 * 65:toCharString() = "A"
 * 99:toCharString() = "c"
 */
RaelValue *number_method_toCharString(RaelNumberValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    int number;
    char c;

    (void)interpreter;
    if (arguments_amount(args) > 0) {
        return BLAME_NEW_CSTR("Too many arguments");
    }
    if (!number_is_whole(self)) {
        return BLAME_NEW_CSTR("Expected self to be a whole number");
    }
    number = number_to_int(self);
    if (number < CHAR_MIN || number > CHAR_MAX) {
        return BLAME_NEW_CSTR("Number not in range of char");
    }
    // get char and return a new string from that char
    c = (char)number;
    return string_new_pure_cpy(&c, 1);
}

RaelTypeValue RaelNumberType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Number",
    .op_add = (RaelBinExprFunc)number_add,
    .op_sub = (RaelBinExprFunc)number_sub,
    .op_mul = (RaelBinExprFunc)number_mul,
    .op_div = (RaelBinExprFunc)number_div,
    .op_mod = (RaelBinExprFunc)number_mod,
    .op_red = NULL,
    .op_eq = (RaelBinCmpFunc)number_eq,
    .op_smaller = (RaelBinCmpFunc)number_smaller,
    .op_bigger = (RaelBinCmpFunc)number_bigger,
    .op_smaller_eq = (RaelBinCmpFunc)number_smaller_eq,
    .op_bigger_eq = (RaelBinCmpFunc)number_bigger_eq,

    .op_neg = (RaelNegFunc)number_neg,

    .op_call = NULL,
    .op_construct = NULL,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = (RaelAsBoolFunc)number_as_bool,
    .deallocator = NULL,
    .repr = (RaelSingleFunc)number_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = (RaelCastFunc)number_cast,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL,

    .methods = {
        { "signedMod", (RaelMethodFunc)number_method_signedMod },
        { "toCharString", (RaelMethodFunc)number_method_toCharString },
        { NULL, NULL }
    }
};
