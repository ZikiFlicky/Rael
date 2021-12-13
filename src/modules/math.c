#include "../module.h"
#include "../value.h"
#include "../common.h"
#include "../string.h"

#include <math.h>
#include <assert.h>

RaelValue module_math_cos(RaelArguments *args) {
    RaelValue number, return_value;
    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);
    assert(number->type == ValueTypeNumber);
    return_value = number_newf(cos(number_to_float(number->as_number)));
    return return_value;
}

RaelValue module_math_sin(RaelArguments *args) {
    RaelValue number, return_value;
    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != ValueTypeNumber) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected a number");
    }
    return_value = number_newf(sin(number_to_float(number->as_number)));
    return return_value;
}

RaelValue module_math_tan(RaelArguments *args) {
    RaelValue number, return_value;
    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != ValueTypeNumber) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected a number");
    }
    return_value = number_newf(tan(number_to_float(number->as_number)));
    return return_value;
}

RaelValue module_math_ceil(RaelArguments *args) {
    RaelValue number, return_value;
    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != ValueTypeNumber) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected a number");
    }
    return_value = number_new(number_ceil(number->as_number));
    return return_value;
}

RaelValue module_math_floor(RaelArguments *args) {
    RaelValue number, return_value;
    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != ValueTypeNumber) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected a number");
    }
    return_value = number_new(number_floor(number->as_number));
    return return_value;
}

RaelValue module_math_abs(RaelArguments *args) {
    RaelValue number, return_value;
    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != ValueTypeNumber) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected a number");
    }
    return_value = number_new(number_abs(number->as_number));
    return return_value;
}

RaelValue module_math_sqrt(RaelArguments *args) {
    RaelValue number, return_value;
    double n;

    assert(arguments_get_amount(args) == 1);
    number = arguments_get(args, 0);

    if (number->type != ValueTypeNumber)
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected a number");

    n = number_to_float(number->as_number);
    if (n < 0)
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("sqrt of a negative number");

    return_value = number_newf(sqrt(n));
    return return_value;
}

RaelValue module_math_pow(RaelArguments *args) {
    RaelValue base, power, return_value;

    assert(arguments_get_amount(args) == 2);
    base = arguments_get(args, 0);
    power = arguments_get(args, 1);
    if (base->type != ValueTypeNumber) {
        return_value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected the base to be a number");
    } else if (power->type != ValueTypeNumber) {
        return_value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected the power to be a number");
    } else {
        double n_base = number_to_float(base->as_number);
        double n_power = number_to_float(power->as_number);
        double result = pow(n_base, n_power);    

        return_value = number_newf(result);
    }
    return return_value;
}

RaelValue module_math_new(void) {
    RaelValue module;
    struct RaelModuleValue m;

    // create module value
    module_new(&m, RAEL_HEAPSTR("Math"));
    // set all keys
    module_set_key(&m, RAEL_HEAPSTR("Cos"), cfunc_new(RAEL_HEAPSTR("Cos"), module_math_cos, 1));
    module_set_key(&m, RAEL_HEAPSTR("Sin"), cfunc_new(RAEL_HEAPSTR("Sin"), module_math_sin, 1));
    module_set_key(&m, RAEL_HEAPSTR("Tan"), cfunc_new(RAEL_HEAPSTR("Tan"), module_math_tan, 1));
    module_set_key(&m, RAEL_HEAPSTR("Floor"), cfunc_new(RAEL_HEAPSTR("Floor"), module_math_floor, 1));
    module_set_key(&m, RAEL_HEAPSTR("Ceil"), cfunc_new(RAEL_HEAPSTR("Ceil"), module_math_ceil, 1));
    module_set_key(&m, RAEL_HEAPSTR("Sqrt"), cfunc_new(RAEL_HEAPSTR("Sqrt"), module_math_sqrt, 1));
    module_set_key(&m, RAEL_HEAPSTR("Pow"), cfunc_new(RAEL_HEAPSTR("Pow"), module_math_pow, 2));
    module_set_key(&m, RAEL_HEAPSTR("Abs"), cfunc_new(RAEL_HEAPSTR("Abs"), module_math_abs, 1));
    module_set_key(&m, RAEL_HEAPSTR("PI"), number_newf(RAEL_CONSTANT_PI));
    module_set_key(&m, RAEL_HEAPSTR("2PI"), number_newf(RAEL_CONSTANT_2PI));
    module_set_key(&m, RAEL_HEAPSTR("E"), number_newf(RAEL_CONSTANT_E));
    module = value_create(ValueTypeModule);
    module->as_module = m;

    return module;
}
