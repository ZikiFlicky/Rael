#include "rael.h"

/*
 * This is the source code for the binary operation library in Rael,
 * called :Bin.
 */

typedef RaelInt (*BinaryBinOp)(RaelInt, RaelInt);

static RaelInt bin_and(RaelInt a, RaelInt b) {
    return a & b;
}

static RaelInt bin_or(RaelInt a, RaelInt b) {
    return a | b;
}

static RaelInt bin_xor(RaelInt a, RaelInt b) {
    return a ^ b;
}

static RaelInt bin_nand(RaelInt a, RaelInt b) {
    return ~(a & b);
}

static RaelInt bin_nor(RaelInt a, RaelInt b) {
    return ~(a | b);
}

static RaelInt bin_xnor(RaelInt a, RaelInt b) {
    return ~(a ^ b);
}

/*
 * This function is called by bin operations (xor, or, nor) and takes 2 or more arguments.
 * The function is also given an function pointer for the function that is run on the two numbers,
 * like bin_or, bin_nand etc...
 * If more than 2 arguments are given, the result of every operation is fed back into a new expression.
 * For example:
 * :Bin:Or(1, 2, 4) is like `1 | 2 | 4` in C
 */
static RaelValue *run_bin_op(RaelArgumentList *args, RaelInterpreter *interpreter, BinaryBinOp op) {
    RaelValue *value;
    RaelNumberValue *number;
    RaelInt n;

    assert(arguments_amount(args) >= 2);
    (void)args;
    (void)interpreter;

    value = arguments_get(args, 0);
    if (value->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    number = (RaelNumberValue*)value;
    if (!number_is_whole(number)) {
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 0));
    }
    n = number_to_int((RaelNumberValue*)value);
    for (size_t i = 1; i < arguments_amount(args); ++i) {
        value = arguments_get(args, i);
        if (value->type != &RaelNumberType) {
            return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, i));
        }
        number = (RaelNumberValue*)value;
        if (!number_is_whole(number)) {
            return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, i));
        }
        n = op(n, number_to_int(number));
    }
    return number_newi(n);
}


RaelValue *module_bin_and(RaelArgumentList *args, RaelInterpreter *interpreter) {
    return run_bin_op(args, interpreter, bin_and);
}

RaelValue *module_bin_or(RaelArgumentList *args, RaelInterpreter *interpreter) {
    return run_bin_op(args, interpreter, bin_or);
}

RaelValue *module_bin_xor(RaelArgumentList *args, RaelInterpreter *interpreter) {
    return run_bin_op(args, interpreter, bin_xor);
}

RaelValue *module_bin_nand(RaelArgumentList *args, RaelInterpreter *interpreter) {
    return run_bin_op(args, interpreter, bin_nand);
}

RaelValue *module_bin_nor(RaelArgumentList *args, RaelInterpreter *interpreter) {
    return run_bin_op(args, interpreter, bin_nor);
}

RaelValue *module_bin_xnor(RaelArgumentList *args, RaelInterpreter *interpreter) {
    return run_bin_op(args, interpreter, bin_xnor);
}

RaelValue *module_bin_not(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelNumberValue *number;

    assert(arguments_amount(args) == 1);
    (void)interpreter;
    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    number = (RaelNumberValue*)arg1;
    if (!number_is_whole(number)) {
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 0));
    }

    return number_newi(~number_to_int(number));
}

RaelValue *module_bin_new(RaelInterpreter *interpreter) {
    RaelModuleValue *m;

    (void)interpreter;
    m = (RaelModuleValue*)module_new(RAEL_HEAPSTR("Bin"));

    module_set_key(m, RAEL_HEAPSTR("And"), cfunc_unlimited_new(RAEL_HEAPSTR("BinAnd"), module_bin_and, 2));
    module_set_key(m, RAEL_HEAPSTR("Or"), cfunc_unlimited_new(RAEL_HEAPSTR("BinOr"), module_bin_or, 2));
    module_set_key(m, RAEL_HEAPSTR("Xor"), cfunc_unlimited_new(RAEL_HEAPSTR("BinXor"), module_bin_xor, 2));
    module_set_key(m, RAEL_HEAPSTR("Nand"), cfunc_unlimited_new(RAEL_HEAPSTR("BinNand"), module_bin_nand, 2));
    module_set_key(m, RAEL_HEAPSTR("Nor"), cfunc_unlimited_new(RAEL_HEAPSTR("BinNor"), module_bin_nor, 2));
    module_set_key(m, RAEL_HEAPSTR("Xnor"), cfunc_unlimited_new(RAEL_HEAPSTR("BinXnor"), module_bin_xnor, 2));
    module_set_key(m, RAEL_HEAPSTR("Not"), cfunc_new(RAEL_HEAPSTR("BinNot"), module_bin_not, 1));

    return (RaelValue*)m;
}
