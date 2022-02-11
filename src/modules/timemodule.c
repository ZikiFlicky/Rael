#include "rael.h"

RaelFloat get_epoch(void) {
    struct timespec tm;
    RaelFloat time, millis;

    clock_gettime(CLOCK_REALTIME, &tm);
    // get seconds
    time = (RaelFloat)tm.tv_sec;

    // get millis and calculate the part after the dot
    millis = (RaelFloat)tm.tv_nsec / 1000000000;
    time += millis;

    return time;
}

RaelValue *module_time_GetEpoch(RaelArgumentList *args, RaelInterpreter *interpreter) {
    (void)interpreter;
    assert(arguments_amount(args) == 0);
    return number_newf(get_epoch());
}

RaelValue *module_time_Since(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelFloat old_time, new_time = get_epoch();

    (void)interpreter;
    assert(arguments_amount(args) == 1);
    arg1 = arguments_get(args, 0);

    if (arg1->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }

    old_time = number_to_float((RaelNumberValue*)arg1);
    if (old_time < 0 || old_time > new_time) {
        return BLAME_NEW_CSTR_ST("Invalid time", *arguments_state(args, 0));
    }

    return number_newf(new_time - old_time);
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
    module_set_key(m, RAEL_HEAPSTR("Since"), cfunc_new(RAEL_HEAPSTR("TimeSince"), module_time_Since, 1));

    return (RaelValue*)m;
}
