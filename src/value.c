#include "value.h"
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

RaelValue *value_new(enum ValueType type) {
    RaelValue *value = malloc(sizeof(RaelValue));
    value->type = type;
    value->reference_count = 1;
    return value;
}

void blamevalue_delete(RaelBlameValue *blame) {
    if (blame->value)
        value_deref(blame->value);
}

void value_deref(RaelValue *value) {
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

void value_ref(RaelValue *value) {
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

void routine_repr(RaelRoutineValue *routine) {
    printf("routine(");
    if (routine->amount_parameters > 0) {
        printf(":%s", routine->parameters[0]);
        for (size_t i = 1; i < routine->amount_parameters; ++i)
            printf(", :%s", routine->parameters[i]);
    }
    printf(")");
}

void value_repr(RaelValue *value) {
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
        printf("%ld to %ld", value->as_range.start, value->as_range.end);
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

void value_log(RaelValue *value) {
    // only strings are printed differently when `log`ed than inside a stack
    switch (value->type) {
    case ValueTypeString: {
        size_t string_length = string_get_length(value);
        for (size_t i = 0; i < string_length; ++i)
            putchar(string_get_char(value, i));
        break;
    }
    default:
        value_repr(value);
    }
}

/* is the value booleanly true? like Python's bool() operator */
bool value_as_bool(RaelValue *const value) {
    switch (value->type) {
    case ValueTypeVoid:
        return false;
    case ValueTypeString:
        return value->as_string.length != 0;
    case ValueTypeNumber:
        return !number_as_bool(number_eq(value->as_number, numbervalue_newi(0)));
    case ValueTypeStack:
        return stack_get_length(value) != 0;
    default:
        return true;
    }
}


bool values_equal(RaelValue *const value, RaelValue *const value2) {
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
            res = string_eq(value, value2);
            break;
        case ValueTypeType:
            res = value->as_type == value2->as_type;
            break;
        case ValueTypeVoid:
            res = true;
            break;
        case ValueTypeStack: {
            res = stack_equals_stack(value, value2);
            break;
        }
        case ValueTypeRange:
            res = value->as_range.start == value2->as_range.start && value->as_range.end == value2->as_range.end;
            break;
        default:
            res = false;
        }
    }

    return res;
}

bool value_is_iterable(RaelValue *value) {
    switch (value->type) {
    case ValueTypeString:
    case ValueTypeStack:
    case ValueTypeRange:
        return true;
    default:
        return false;
    }
}

size_t range_get_length(RaelValue *range) {
    assert(range->type == ValueTypeRange);
    return (size_t)rael_int_abs(range->as_range.end - range->as_range.start);
}

RaelValue *range_get(RaelValue *range, size_t idx) {
    RaelValue *value;
    RaelInt number;
    assert(range->type == ValueTypeRange);
    if (idx >= range_get_length(range))
        return NULL;
    value = value_new(ValueTypeNumber);

    if (range->as_range.end > range->as_range.start)
        number = range->as_range.start + idx;
    else
        number = range->as_range.start - idx;

    value->as_number = numbervalue_newi(number);
    return value;
}

RaelValue *blame_no_state_new(RaelValue *value) {
    RaelValue *blame = value_new(ValueTypeBlame);
    blame->as_blame.value = value;
    return blame;
}

bool value_is_blame(RaelValue *value) {
    return value->type == ValueTypeBlame;
}

void blame_add_state(RaelValue *value, struct State state) {
    assert(value_is_blame(value));
    value->as_blame.original_place = state;
}

RaelValue *blame_new(RaelValue *message, struct State state) {
    RaelValue *blame = value_new(ValueTypeBlame);
    blame->as_blame.value = message;
    blame->as_blame.original_place = state;
    return blame;
}

size_t value_get_length(RaelValue *value) {
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

RaelValue *value_get(RaelValue *value, size_t idx) {
    RaelValue *out;
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

/* lhs + rhs */
RaelValue *values_add(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) { // number + number
        return number_new(number_add(value->as_number, value2->as_number));
    } else if (value->type == ValueTypeString && value2->type == ValueTypeString) { // string + string
        return strings_add(value, value2);
    } else if (value->type == ValueTypeNumber && value2->type == ValueTypeString) { // number + string
        return string_precede_with_number(value, value2);
    } else if (value->type == ValueTypeString && value2->type == ValueTypeNumber) { // string + number
        return string_add_number(value, value2);
    } else {
        return NULL;
    }
}

/* lhs - rhs */
RaelValue *values_sub(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        return number_new(number_sub(value->as_number, value2->as_number));
    } else {
        return NULL;
    }
}

/* lhs * rhs */
RaelValue *values_mul(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        return number_new(number_mul(value->as_number, value2->as_number));
    } else {
        return NULL;
    }
}

/* lhs / rhs */
RaelValue *values_div(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        RaelNumberValue out;
        if (!number_div(value->as_number, value2->as_number, &out))
            return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Division by zero");
        return number_new(out);
    } else {
        return NULL;
    }
}

/* lhs % rhs */
RaelValue *values_mod(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        RaelNumberValue out;
        if (!number_mod(value->as_number, value2->as_number, &out))
            return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Division by zero");
        return number_new(out);
    } else {
        return NULL;
    }
}

/* lhs < rhs */
RaelValue *values_smaller(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        return number_new(number_smaller(value->as_number, value2->as_number));
    } else {
        return NULL;
    }
}

/* lhs > rhs */
RaelValue *values_bigger(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        return number_new(number_bigger(value->as_number, value2->as_number));
    } else {
        return NULL;
    }
}

/* lhs <= rhs */
RaelValue *values_smaller_eq(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        return number_new(number_smaller_eq(value->as_number, value2->as_number));
    } else {
        return NULL;
    }
}

/* lhs >= rhs */
RaelValue *values_bigger_eq(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        return number_new(number_bigger_eq(value->as_number, value2->as_number));
    } else {
        return NULL;
    }
}
