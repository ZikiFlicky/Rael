#include "../rael.h"

RaelValue *module_types_new(void) {
    RaelModuleValue *m;

    // create module value
    m = (RaelModuleValue*)module_new(RAEL_HEAPSTR("Types"));

    //reference all types
    value_ref((RaelValue*)&RaelTypeType);
    value_ref((RaelValue*)&RaelVoidType);
    value_ref((RaelValue*)&RaelNumberType);
    value_ref((RaelValue*)&RaelStringType);
    value_ref((RaelValue*)&RaelStackType);
    value_ref((RaelValue*)&RaelRoutineType);
    value_ref((RaelValue*)&RaelCFuncType);
    value_ref((RaelValue*)&RaelBlameType);
    value_ref((RaelValue*)&RaelRangeType);
    value_ref((RaelValue*)&RaelModuleType);

    // set all keys
    module_set_key(m, RAEL_HEAPSTR("Type"), (RaelValue*)&RaelTypeType);
    module_set_key(m, RAEL_HEAPSTR("VoidType"), (RaelValue*)&RaelVoidType);
    module_set_key(m, RAEL_HEAPSTR("Number"), (RaelValue*)&RaelNumberType);
    module_set_key(m, RAEL_HEAPSTR("String"), (RaelValue*)&RaelStringType);
    module_set_key(m, RAEL_HEAPSTR("Stack"), (RaelValue*)&RaelStackType);
    module_set_key(m, RAEL_HEAPSTR("Routine"), (RaelValue*)&RaelRoutineType);
    module_set_key(m, RAEL_HEAPSTR("CFunc"), (RaelValue*)&RaelCFuncType);
    module_set_key(m, RAEL_HEAPSTR("Blame"), (RaelValue*)&RaelBlameType);
    module_set_key(m, RAEL_HEAPSTR("Range"), (RaelValue*)&RaelRangeType);
    module_set_key(m, RAEL_HEAPSTR("Module"), (RaelValue*)&RaelModuleType);

    return (RaelValue*)m;
}
