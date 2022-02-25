#include "rael.h"

static RaelInt get_random(void) {
    const size_t times_generate = sizeof(RaelInt) / sizeof(RAND_MAX);
    RaelInt generated = 0;

    for (size_t i = 0; i < times_generate; ++i) {
        int rand_chunk = rand();
        generated += (RaelInt)rand_chunk << (RaelInt)(i * (sizeof(RAND_MAX) * 8));
    }

    return generated;
}

RaelValue *module_random_RandomRange(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelRangeValue *range;
    size_t length, idx;
    RaelInt generated_number;

    (void)interpreter;
    assert(arguments_amount(args) == 1);

    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelRangeType) {
        return BLAME_NEW_CSTR_ST("Expected a range", *arguments_state(args, 0));
    }
    range = (RaelRangeValue*)arg1;

    length = range_length(range);
    if (length == 0) {
        return BLAME_NEW_CSTR_ST("Range length is 0", *arguments_state(args, 0));
    }

    // get the index of the range
    idx = (size_t)((get_random() % (RaelInt)length) + (RaelInt)length) % length;
    generated_number = range_at(range, idx);

    return number_newi(generated_number);
}

RaelValue *module_random_RandomFloat(RaelArgumentList *args, RaelInterpreter *interpreter) {
    (void)interpreter;
    assert(arguments_amount(args) == 0);
    return number_newf((RaelFloat)get_random() / (RaelFloat)RAELINT_MAX);
}

RaelValue *module_random_new(RaelInterpreter *interpreter) {
    RaelModuleValue *m;

    (void)interpreter;
    // create module value
    m = (RaelModuleValue*)module_new(RAEL_HEAPSTR("Random"));

    // set all keys
    module_set_key(m, RAEL_HEAPSTR("RandomRange"), cfunc_new(RAEL_HEAPSTR("RandomRange"), module_random_RandomRange, 1));
    module_set_key(m, RAEL_HEAPSTR("RandomFloat"), cfunc_new(RAEL_HEAPSTR("RandomFloat"), module_random_RandomFloat, 0));

    return (RaelValue*)m;
}
