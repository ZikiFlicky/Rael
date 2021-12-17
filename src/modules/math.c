#include "../module.h"
#include "../value.h"
#include "../common.h"
#include "../string.h"

#include <math.h>
#include <assert.h>

RaelValue *module_math_cos(RaelArguments *args) {
    RaelValue *number;
    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);
    assert(number->type == ValueTypeNumber);
    return (RaelValue*)number_newf(cos(number_to_float((RaelNumberValue*)number)));
}

RaelValue *module_math_sin(RaelArguments *args) {
    RaelValue *number;
    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != ValueTypeNumber) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected a number");
    }
    return (RaelValue*)number_newf(sin(number_to_float((RaelNumberValue*)number)));
}

RaelValue *module_math_tan(RaelArguments *args) {
    RaelValue *number;
    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != ValueTypeNumber) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected a number");
    }
    return (RaelValue*)number_newf(tan(number_to_float((RaelNumberValue*)number)));
}

RaelValue *module_math_ceil(RaelArguments *args) {
    RaelValue *number;
    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != ValueTypeNumber) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected a number");
    }
    return (RaelValue*)number_ceil((RaelNumberValue*)number);
}

RaelValue *module_math_floor(RaelArguments *args) {
    RaelValue *number;
    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != ValueTypeNumber) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected a number");
    }
    return (RaelValue*)number_floor((RaelNumberValue*)number);
}

RaelValue *module_math_abs(RaelArguments *args) {
    RaelValue *number;
    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != ValueTypeNumber) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected a number");
    }
    return (RaelValue*)number_abs((RaelNumberValue*)number);
}

RaelValue *module_math_sqrt(RaelArguments *args) {
    RaelValue *number;
    RaelFloat n;

    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);

    if (number->type != ValueTypeNumber)
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected a number");

    n = number_to_float((RaelNumberValue*)number);
    if (n < 0)
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("sqrt of a negative number");

    return (RaelValue*)number_newf(sqrt(n));
}

RaelValue *module_math_pow(RaelArguments *args) {
    RaelValue *base;
    RaelValue *power;

    assert(arguments_get_amount(args) == 2);
    base = arguments_get(args, 0);
    power = arguments_get(args, 1);
    if (base->type != ValueTypeNumber) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected the base to be a number");
    } else if (power->type != ValueTypeNumber) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected the power to be a number");
    } else {
        RaelFloat n_base = number_to_float((RaelNumberValue*)base),
                  n_power = number_to_float((RaelNumberValue*)power),
                  result = pow(n_base, n_power);

        return (RaelValue*)number_newf(result);
    }
}

RaelModuleValue *module_math_new(void) {
    RaelModuleValue *m;

    // create module value
    m = module_new(RAEL_HEAPSTR("Math"));
    // set all keys
    module_set_key(m, RAEL_HEAPSTR("Cos"), (RaelValue*)cfunc_new(RAEL_HEAPSTR("Cos"), module_math_cos, 1));
    module_set_key(m, RAEL_HEAPSTR("Sin"), (RaelValue*)cfunc_new(RAEL_HEAPSTR("Sin"), module_math_sin, 1));
    module_set_key(m, RAEL_HEAPSTR("Tan"), (RaelValue*)cfunc_new(RAEL_HEAPSTR("Tan"), module_math_tan, 1));
    module_set_key(m, RAEL_HEAPSTR("Floor"), (RaelValue*)cfunc_new(RAEL_HEAPSTR("Floor"), module_math_floor, 1));
    module_set_key(m, RAEL_HEAPSTR("Ceil"), (RaelValue*)cfunc_new(RAEL_HEAPSTR("Ceil"), module_math_ceil, 1));
    module_set_key(m, RAEL_HEAPSTR("Sqrt"), (RaelValue*)cfunc_new(RAEL_HEAPSTR("Sqrt"), module_math_sqrt, 1));
    module_set_key(m, RAEL_HEAPSTR("Pow"), (RaelValue*)cfunc_new(RAEL_HEAPSTR("Pow"), module_math_pow, 2));
    module_set_key(m, RAEL_HEAPSTR("Abs"), (RaelValue*)cfunc_new(RAEL_HEAPSTR("Abs"), module_math_abs, 1));
    module_set_key(m, RAEL_HEAPSTR("PI"), (RaelValue*)number_newf(RAEL_CONSTANT_PI));
    module_set_key(m, RAEL_HEAPSTR("2PI"), (RaelValue*)number_newf(RAEL_CONSTANT_2PI));
    module_set_key(m, RAEL_HEAPSTR("E"), (RaelValue*)number_newf(RAEL_CONSTANT_E));

    return m;
}
