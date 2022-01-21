#include "../rael.h"

RaelValue *module_time_GetEpoch(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelFloat output;
    RaelFloat millis;
    struct timespec tm;

    (void)interpreter;
    assert(arguments_amount(args) == 0);

    // get time into tm
    clock_gettime(CLOCK_REALTIME, &tm);
    // get seconds
    output = (RaelFloat)tm.tv_sec;

    // get millis and calculate the part after the dot
    millis = (RaelFloat)tm.tv_nsec / 1000000000;
    output += millis;

    return number_newf(output);
}

RaelValue *module_time_Sleep(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelFloat sleeptime;
    RaelInt amount_sec, amount_millis;
    struct timespec tm;

    (void)interpreter;
    assert(arguments_amount(args) == 1);

    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected number", *arguments_state(args, 0));
    }
    // get sleeptime
    sleeptime = number_to_float((RaelNumberValue*)arg1);
    if (sleeptime < 0) {
        return BLAME_NEW_CSTR_ST("Expected a positive number", *arguments_state(args, 0));
    }


    // calculate how many seconds and how many milliseconds to sleep
    amount_sec = (RaelInt)sleeptime;
    amount_millis = (RaelInt)(sleeptime * 1000000000) % 1000000000;
    // set the sleep time
    tm.tv_sec = amount_sec;
    tm.tv_nsec = amount_millis;
    nanosleep(&tm, NULL);

    return void_new();
}

RaelValue *module_time_new(RaelInterpreter *interpreter) {
    RaelModuleValue *m;

    (void)interpreter;
    m = (RaelModuleValue*)module_new(RAEL_HEAPSTR("Time"));
    module_set_key(m, RAEL_HEAPSTR("GetEpoch"), cfunc_new(RAEL_HEAPSTR("GetEpoch"), module_time_GetEpoch, 0));
    module_set_key(m, RAEL_HEAPSTR("Sleep"), cfunc_new(RAEL_HEAPSTR("Sleep"), module_time_Sleep, 1));

    return (RaelValue*)m;
}
