#include "value.h"
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

RaelValue value_create(enum ValueType type) {
    RaelValue value = malloc(sizeof(struct RaelValue));
    value->type = type;
    value->reference_count = 1;
    return value;
}

void blamevalue_delete(struct RaelBlameValue *blame) {
    if (blame->value)
        value_deref(blame->value);
}

void value_deref(RaelValue value) {
    --value->reference_count;
    if (value->reference_count == 0) {
        switch (value->type) {
        case ValueTypeRoutine:
            break;
        case ValueTypeStack:
            stackvalue_delete(&value->as_stack);
            break;
        case ValueTypeString:
            stringvalue_delete(&value->as_string);
            break;
        case ValueTypeBlame:
            blamevalue_delete(&value->as_blame);
            break;
        case ValueTypeCFunc:
            cfunc_delete(&value->as_cfunc);
            break;
        case ValueTypeModule:
            module_delete(&value->as_module);
            break;
        default:
            break;
        }
        free(value);
    }
}

void value_ref(RaelValue value) {
    ++value->reference_count;
}

char *value_type_to_string(enum ValueType type) {
    switch (type) {
    case ValueTypeVoid: return "VoidType";
    case ValueTypeNumber: return "Number";
    case ValueTypeString: return "String";
    case ValueTypeRoutine: return "Routine";
    case ValueTypeStack: return "Stack";
    case ValueTypeRange: return "Range";
    case ValueTypeBlame: return "Blame";
    case ValueTypeType: return "Type";
    default: RAEL_UNREACHABLE();
    }
}

void routine_repr(struct RaelRoutineValue *routine) {
    printf("routine(");
    if (routine->amount_parameters > 0) {
        printf(":%s", routine->parameters[0]);
        for (size_t i = 1; i < routine->amount_parameters; ++i)
            printf(", :%s", routine->parameters[i]);
    }
    printf(")");
}

void value_repr(RaelValue value) {
    switch (value->type) {
    case ValueTypeNumber:
        number_repr(value->as_number);
        break;
    case ValueTypeString:
        stringvalue_repr(&value->as_string);
        break;
    case ValueTypeVoid:
        printf("Void");
        break;
    case ValueTypeRoutine:
        routine_repr(&value->as_routine);
        break;
    case ValueTypeStack:
        stackvalue_repr(&value->as_stack);
        break;
    case ValueTypeRange:
        printf("%d to %d", value->as_range.start, value->as_range.end);
        break;
    case ValueTypeType:
        printf("%s", value_type_to_string(value->as_type));
        break;
    case ValueTypeCFunc:
        cfunc_repr(&value->as_cfunc);
        break;
    case ValueTypeModule:
        module_repr(&value->as_module);
        break;
    default:
        RAEL_UNREACHABLE();
    }
}

void value_log(RaelValue value) {
    // only strings are printed differently when `log`ed than inside a stack
    switch (value->type) {
    case ValueTypeString:
        for (size_t i = 0; i < value->as_string.length; ++i)
            putchar(value->as_string.value[i]);
        break;
    default:
        value_repr(value);
    }
}

/* is the value booleanly true? like Python's bool() operator */
bool value_as_bool(const RaelValue value) {
    switch (value->type) {
    case ValueTypeVoid:
        return false;
    case ValueTypeString:
        return value->as_string.length != 0;
    case ValueTypeNumber:
        return !number_as_bool(number_eq(value->as_number, numbervalue_newi(0)));
    case ValueTypeStack:
        return value->as_stack.length != 0;
    default:
        return true;
    }
}


bool values_equal(const RaelValue value, const RaelValue value2) {
    bool res;
    // if they have the same pointer they must be equal
    if (value == value2) {
        res = true;
    } else if (value->type != value2->type) {
        // if they don't share the same type they are not equal
        res = false;
    } else {
        enum ValueType type = value->type;

        switch (type) {
        case ValueTypeNumber:
            res = number_eq(value->as_number, value2->as_number).as_int;
            break;
        case ValueTypeString:
            if (value->as_string.length == value2->as_string.length) {
                if (value->as_string.value == value2->as_string.value)
                    res = true;
                else
                    res = strncmp(value->as_string.value, value2->as_string.value, value->as_string.length) == 0;
            } else {
                res = false;
            }
            break;
        case ValueTypeType:
            res = value->as_type == value2->as_type;
            break;
        case ValueTypeVoid:
            res = true;
            break;
        case ValueTypeStack:
            if (value->as_stack.length == value2->as_stack.length) {
                res = true;
                for (size_t i = 0; i < value->as_stack.length; ++i) {
                    if (!values_equal(value->as_stack.values[i], value2->as_stack.values[i])) {
                        res = false;
                        break;
                    }
                }
            } else {
                res = false;
            }
            break;
        case ValueTypeRange:
            res = value->as_range.start == value2->as_range.start && value->as_range.end == value2->as_range.end;
            break;
        default:
            res = false;
        }
    }

    return res;
}

bool value_is_iterable(RaelValue value) {
    switch (value->type) {
    case ValueTypeString:
    case ValueTypeStack:
    case ValueTypeRange:
        return true;
    default:
        return false;
    }
}

size_t range_get_length(RaelValue range) {
    assert(range->type == ValueTypeRange);
    return (size_t)abs(range->as_range.end - range->as_range.start);
}

RaelValue range_get(RaelValue range, size_t idx) {
    RaelValue value;
    int number;
    assert(range->type == ValueTypeRange);
    if (idx >= range_get_length(range))
        return NULL;
    value = value_create(ValueTypeNumber);

    if (range->as_range.end > range->as_range.start)
        number = range->as_range.start + idx;
    else
        number = range->as_range.start - idx;

    value->as_number = numbervalue_newi(number);
    return value;
}

RaelValue blame_no_state_new(RaelValue value) {
    RaelValue blame = value_create(ValueTypeBlame);
    blame->as_blame.value = value;
    return blame;
}

size_t value_get_length(RaelValue value) {
    size_t length;
    assert(value_is_iterable(value));

    switch (value->type) {
    case ValueTypeString:
        length = string_get_length(value);
        break;
    case ValueTypeStack:
        length = stack_get_length(value);
        break;
    case ValueTypeRange:
        length = range_get_length(value);
        break;
    default:
        RAEL_UNREACHABLE();
    }
    return length;
}

RaelValue value_get(RaelValue value, size_t idx) {
    RaelValue out;
    assert(value_is_iterable(value));

    switch (value->type) {
    case ValueTypeStack:
        out = stack_get(value, idx);
        break;
    case ValueTypeString:
        out = string_get(value, idx);
        break;
    case ValueTypeRange:
        out = range_get(value, idx);
        break;
    default:
        RAEL_UNREACHABLE();
    }

    return out;
}
