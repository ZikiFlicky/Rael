#include "value.h"
#include "stack.h"
#include "module.h"
#include "string.h"
#include "number.h"
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

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

RaelRangeValue *range_new(RaelInt start, RaelInt end) {
    RaelRangeValue *range = RAEL_VALUE_NEW(ValueTypeRange, RaelRangeValue);
    range->start = start;
    range->end = end;
    return range;
}

size_t range_length(RaelRangeValue *range) {
    return (size_t)rael_int_abs(range->end - range->start);
}

RaelValue *range_get(RaelRangeValue *range, size_t idx) {
    RaelInt number;

    if (idx >= range_length(range))
        return NULL;

    // get the number, depending on the direction
    if (range->end > range->start)
        number = range->start + idx;
    else
        number = range->start - idx;

    // return the number
    return (RaelValue*)number_newi(number);;
}

bool range_eq(RaelRangeValue *range, RaelRangeValue *range2) {
    return range->start == range2->start && range->end == range2->end;
}

void range_repr(RaelRangeValue *range) {
    printf("%ld to %ld", range->start, range->end);
}

bool value_is_blame(RaelValue *value) {
    return value->type == ValueTypeBlame;
}

bool type_eq(RaelTypeValue *type, RaelTypeValue *type2) {
    return type->type == type2->type;
}

void type_repr(RaelTypeValue *type) {
    printf("%s", value_type_to_string(type->type));
}

// TODO: make this take a `struct State *state` so you could decide not to initialize the blame's state,
// which will let us get rid of `blame_no_state_new`
RaelBlameValue *blame_new(RaelValue *message, struct State state) {
    RaelBlameValue *blame = RAEL_VALUE_NEW(ValueTypeBlame, RaelBlameValue);
    blame->value = message;
    blame->original_place = state;
    return blame;
}

RaelBlameValue *blame_no_state_new(RaelValue *message) {
    RaelBlameValue *blame = RAEL_VALUE_NEW(ValueTypeBlame, RaelBlameValue);
    blame->value = message;
    return blame;
}

void blame_delete(RaelBlameValue *blame) {
    // if there is a value inside of the blame, dereference it
    if (blame->value)
        value_deref(blame->value);
}

void blame_add_state(RaelBlameValue *blame, struct State state) {
    blame->original_place = state;
}

/* create a new RaelValue with type `enum ValueType` and size `size` */
RaelValue *value_new(enum ValueType type, size_t size) {
    RaelValue *value;
    assert(size >= sizeof(RaelValue));

    value = malloc(size);
    value->type = type;
    value->reference_count = 1;
    return value;
}

void value_ref(RaelValue *value) {
    ++value->reference_count;
}

// TODO: optimally, this should call a deconstructor, like value->type->_deallocator
void value_deref(RaelValue *value) {
    --value->reference_count;
    if (value->reference_count == 0) {
        switch (value->type) {
        case ValueTypeStack:
            stack_delete((RaelStackValue*)value);
            break;
        case ValueTypeString:
            string_delete((RaelStringValue*)value);
            break;
        case ValueTypeBlame:
            blame_delete((RaelBlameValue*)value);
            break;
        case ValueTypeCFunc:
            cfunc_delete((RaelExternalCFuncValue*)value);
            break;
        case ValueTypeModule:
            module_delete((RaelModuleValue*)value);
            break;
        default:
            break;
        }
        free(value);
    }
}

// TODO: this should optimally be something like value->type->_logger
void value_repr(RaelValue *value) {
    switch (value->type) {
    case ValueTypeNumber:
        number_repr((RaelNumberValue*)value);
        break;
    case ValueTypeString:
        string_repr((RaelStringValue*)value);
        break;
    case ValueTypeVoid:
        printf("Void");
        break;
    case ValueTypeRoutine:
        routine_repr((RaelRoutineValue*)value);
        break;
    case ValueTypeStack:
        stack_repr((RaelStackValue*)value);
        break;
    case ValueTypeRange:
        range_repr((RaelRangeValue*)value);
        break;
    case ValueTypeType:
        type_repr((RaelTypeValue*)value);
        break;
    case ValueTypeCFunc:
        cfunc_repr((RaelExternalCFuncValue*)value);
        break;
    case ValueTypeModule:
        module_repr((RaelModuleValue*)value);
        break;
    default:
        RAEL_UNREACHABLE();
    }
}

void value_log(RaelValue *value) {
    // only strings are printed differently when `log`ed than inside a stack
    switch (value->type) {
    case ValueTypeString: {
        size_t str_length = string_length((RaelStringValue*)value);
        for (size_t i = 0; i < str_length; ++i)
            putchar(string_get_char((RaelStringValue*)value, i));
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
        return string_as_bool((RaelStringValue*)value);
    case ValueTypeNumber:
        return number_as_bool((RaelNumberValue*)value);
    case ValueTypeStack:
        return stack_as_bool((RaelStackValue*)value);
    default:
        return true;
    }
}

bool values_eq(RaelValue* const value, RaelValue* const value2) {
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
            res = number_eq((RaelNumberValue*)value, (RaelNumberValue*)value2);
            break;
        case ValueTypeString:
            res = string_eq((RaelStringValue*)value, (RaelStringValue*)value2);
            break;
        case ValueTypeType:
            res = type_eq((RaelTypeValue*)value, (RaelTypeValue*)value2);
            break;
        case ValueTypeVoid: // Void = Void will always be true
            res = true;
            break;
        case ValueTypeStack: {
            res = stack_eq((RaelStackValue*)value, (RaelStackValue*)value2);
            break;
        }
        case ValueTypeRange:
            res = range_eq((RaelRangeValue*)value, (RaelRangeValue*)value2);
            break;
        default:
            res = false;
        }
    }

    return res;
}

size_t value_length(RaelValue *value) {
    size_t length;
    assert(value_is_iterable(value));

    switch (value->type) {
    case ValueTypeStack:
        length = stack_length((RaelStackValue*)value);
        break;
    case ValueTypeString:
        length = string_length((RaelStringValue*)value);
        break;
    case ValueTypeRange:
        length = range_length((RaelRangeValue*)value);
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
        out = stack_get((RaelStackValue*)value, idx);
        break;
    case ValueTypeString:
        out = string_get((RaelStringValue*)value, idx);
        break;
    case ValueTypeRange:
        out = range_get((RaelRangeValue*)value, idx);
        break;
    default:
        RAEL_UNREACHABLE();
    }

    return out;
}

/* lhs + rhs */
RaelValue *values_add(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) { // number + number
        return (RaelValue*)number_add((RaelNumberValue*)value, (RaelNumberValue*)value2);
    } else if (value->type == ValueTypeString && value2->type == ValueTypeString) { // string + string
        return (RaelValue*)strings_add((RaelStringValue*)value, (RaelStringValue*)value2);
    } else if (value->type == ValueTypeNumber && value2->type == ValueTypeString) { // number + string
        return (RaelValue*)string_precede_with_number((RaelNumberValue*)value, (RaelStringValue*)value2);
    } else if (value->type == ValueTypeString && value2->type == ValueTypeNumber) { // string + number
        return (RaelValue*)string_add_number((RaelStringValue*)value, (RaelNumberValue*)value2);
    } else {
        return NULL;
    }
}

/* lhs - rhs */
RaelValue *values_sub(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        return (RaelValue*)number_sub((RaelNumberValue*)value, (RaelNumberValue*)value2);
    } else {
        return NULL;
    }
}

/* lhs * rhs */
RaelValue *values_mul(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        return (RaelValue*)number_mul((RaelNumberValue*)value, (RaelNumberValue*)value2);
    } else {
        return NULL;
    }
}

/* lhs / rhs */
RaelValue *values_div(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        RaelNumberValue *out;
        if (!(out = number_div((RaelNumberValue*)value, (RaelNumberValue*)value2))) {
            return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Division by zero");
        }
        return (RaelValue*)out;
    } else {
        return NULL;
    }
}

/* lhs % rhs */
RaelValue *values_mod(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        RaelNumberValue *out;
        // if had a div by zero
        if (!(out = number_mod((RaelNumberValue*)value, (RaelNumberValue*)value2)))
            return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Division by zero");
        return (RaelValue*)out;
    } else {
        return NULL;
    }
}

/* lhs < rhs */
RaelValue *values_smaller(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        return (RaelValue*)number_smaller((RaelNumberValue*)value, (RaelNumberValue*)value2);
    } else {
        return NULL;
    }
}

/* lhs > rhs */
RaelValue *values_bigger(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        return (RaelValue*)number_bigger((RaelNumberValue*)value, (RaelNumberValue*)value2);
    } else {
        return NULL;
    }
}

/* lhs <= rhs */
RaelValue *values_smaller_eq(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        return (RaelValue*)number_smaller_eq((RaelNumberValue*)value, (RaelNumberValue*)value2);
    } else {
        return NULL;
    }
}

/* lhs >= rhs */
RaelValue *values_bigger_eq(RaelValue *value, RaelValue *value2) {
    if (value->type == ValueTypeNumber && value2->type == ValueTypeNumber) {
        return (RaelValue*)number_bigger_eq((RaelNumberValue*)value, (RaelNumberValue*)value2);
    } else {
        return NULL;
    }
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
