#include "rael.h"

RaelValue *module_math_cos(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *number;
    (void)interpreter;
    assert(arguments_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    return number_newf(cos(number_to_float((RaelNumberValue*)number)));
}

RaelValue *module_math_sin(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *number;
    (void)interpreter;
    assert(arguments_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    return number_newf(sin(number_to_float((RaelNumberValue*)number)));
}

RaelValue *module_math_tan(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *number;
    (void)interpreter;
    assert(arguments_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    return number_newf(tan(number_to_float((RaelNumberValue*)number)));
}

RaelValue *module_math_acos(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *number;
    (void)interpreter;
    assert(arguments_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    return number_newf(acos(number_to_float((RaelNumberValue*)number)));
}

RaelValue *module_math_asin(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *number;
    (void)interpreter;
    assert(arguments_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    return number_newf(asin(number_to_float((RaelNumberValue*)number)));
}

RaelValue *module_math_atan(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *number;
    (void)interpreter;
    assert(arguments_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    return number_newf(atan(number_to_float((RaelNumberValue*)number)));
}

RaelValue *module_math_log10(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *number;
    (void)interpreter;
    assert(arguments_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    return number_newf(log10(number_to_float((RaelNumberValue*)number)));
}

RaelValue *module_math_log2(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *number;
    (void)interpreter;
    assert(arguments_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    return number_newf(log2(number_to_float((RaelNumberValue*)number)));
}

RaelValue *module_math_ceil(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *number;
    (void)interpreter;
    assert(arguments_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    return (RaelValue*)number_ceil((RaelNumberValue*)number);
}

RaelValue *module_math_floor(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *number;
    (void)interpreter;
    assert(arguments_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    return (RaelValue*)number_floor((RaelNumberValue*)number);
}

RaelValue *module_math_abs(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *number;
    (void)interpreter;
    assert(arguments_amount(args) == 1);
    number = arguments_get(args, 0);
    if (number->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    return (RaelValue*)number_abs((RaelNumberValue*)number);
}

RaelValue *module_math_sqrt(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *number;
    RaelFloat n;

    (void)interpreter;
    assert(arguments_amount(args) == 1);
    number = arguments_get(args, 0);

    if (number->type != &RaelNumberType)
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));

    n = number_to_float((RaelNumberValue*)number);
    if (n < 0)
        return BLAME_NEW_CSTR_ST("Sqrt of a negative number is not possible", *arguments_state(args, 0));

    return number_newf(sqrt(n));
}

RaelValue *module_math_pow(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *base;
    RaelValue *power;

    (void)interpreter;
    assert(arguments_amount(args) == 2);
    base = arguments_get(args, 0);
    power = arguments_get(args, 1);
    if (base->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    } else if (power->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 1));
    } else {
        RaelFloat n_base = number_to_float((RaelNumberValue*)base),
                  n_power = number_to_float((RaelNumberValue*)power);
        RaelFloat result = pow(n_base, n_power);

        return number_newf(result);
    }
}

RaelValue *module_math_new(RaelInterpreter *interpreter) {
    RaelModuleValue *m;

    (void)interpreter;
    // create module value
    m = (RaelModuleValue*)module_new(RAEL_HEAPSTR("Math"));
    // set all keys
    module_set_key(m, RAEL_HEAPSTR("Cos"), cfunc_new(RAEL_HEAPSTR("Cos"), module_math_cos, 1));
    module_set_key(m, RAEL_HEAPSTR("Sin"), cfunc_new(RAEL_HEAPSTR("Sin"), module_math_sin, 1));
    module_set_key(m, RAEL_HEAPSTR("Tan"), cfunc_new(RAEL_HEAPSTR("Tan"), module_math_tan, 1));
    module_set_key(m, RAEL_HEAPSTR("ACos"), cfunc_new(RAEL_HEAPSTR("ACos"), module_math_acos, 1));
    module_set_key(m, RAEL_HEAPSTR("ASin"), cfunc_new(RAEL_HEAPSTR("ASin"), module_math_asin, 1));
    module_set_key(m, RAEL_HEAPSTR("ATan"), cfunc_new(RAEL_HEAPSTR("ATan"), module_math_atan, 1));
    module_set_key(m, RAEL_HEAPSTR("Log10"), cfunc_new(RAEL_HEAPSTR("Log10"), module_math_log10, 1));
    module_set_key(m, RAEL_HEAPSTR("Log2"), cfunc_new(RAEL_HEAPSTR("Log2"), module_math_log2, 1));
    module_set_key(m, RAEL_HEAPSTR("Floor"), cfunc_new(RAEL_HEAPSTR("Floor"), module_math_floor, 1));
    module_set_key(m, RAEL_HEAPSTR("Ceil"), cfunc_new(RAEL_HEAPSTR("Ceil"), module_math_ceil, 1));
    module_set_key(m, RAEL_HEAPSTR("Sqrt"), cfunc_new(RAEL_HEAPSTR("Sqrt"), module_math_sqrt, 1));
    module_set_key(m, RAEL_HEAPSTR("Pow"), cfunc_new(RAEL_HEAPSTR("Pow"), module_math_pow, 2));
    module_set_key(m, RAEL_HEAPSTR("Abs"), cfunc_new(RAEL_HEAPSTR("Abs"), module_math_abs, 1));
    module_set_key(m, RAEL_HEAPSTR("PI"), number_newf(RAEL_CONSTANT_PI));
    module_set_key(m, RAEL_HEAPSTR("2PI"), number_newf(RAEL_CONSTANT_2PI));
    module_set_key(m, RAEL_HEAPSTR("E"), number_newf(RAEL_CONSTANT_E));

    return (RaelValue*)m;
}