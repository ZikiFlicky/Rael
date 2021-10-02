#include "value.h"
#include "common.h"

#include <stdlib.h>
#include <stdio.h>

RaelValue value_create(enum ValueType type) {
    RaelValue value = malloc(sizeof(struct RaelValue));
    value->type = type;
    value->reference_count = 1;
    return value;
}

void value_dereference(RaelValue value) {
    --value->reference_count;
    if (value->reference_count == 0) {
        switch (value->type) {
        case ValueTypeRoutine:
            break;
        case ValueTypeStack:
            for (size_t i = 0; i < value->as_stack.length; ++i) {
                value_dereference(value->as_stack.values[i]);
            }
            free(value->as_stack.values);
            break;
        case ValueTypeString:
            if (!value->as_string.does_reference_ast && value->as_string.length)
                free(value->as_string.value);
            break;
        case ValueTypeBlame:
            if (value->as_blame.value)
                value_dereference(value->as_blame.value);
            break;
        default:
            break;
        }
        free(value);
    }
}

void value_log_as_original(RaelValue value) {
    switch (value->type) {
    case ValueTypeNumber:
        if (value->as_number.is_float) {
            printf("%f", value->as_number.as_float);
        } else {
            printf("%d", value->as_number.as_int);
        }
        break;
    case ValueTypeString:
        putchar('"');
        for (size_t i = 0; i < value->as_string.length; ++i) {
            switch (value->as_string.value[i]) {
            case '\n':
                printf("\\n");
                break;
            case '\r':
                printf("\\r");
                break;
            case '\t':
                printf("\\t");
                break;
            case '"':
                printf("\\\"");
                break;
            case '\\':
                printf("\\\\");
                break;
            default:
                putchar(value->as_string.value[i]);
            }
        }
        putchar('"');
        break;
    case ValueTypeVoid:
        printf("Void");
        break;
    case ValueTypeRoutine:
        printf("routine(");
        if (value->as_routine.amount_parameters > 0) {
            printf(":%s", value->as_routine.parameters[0]);
            for (size_t i = 1; i < value->as_routine.amount_parameters; ++i)
                printf(", :%s", value->as_routine.parameters[i]);
        }
        printf(")");
        break;
    case ValueTypeStack:
        printf("{ ");
        for (size_t i = 0; i < value->as_stack.length; ++i) {
            if (i > 0)
                printf(", ");
            value_log_as_original(value->as_stack.values[i]);
        }
        printf(" }");
        break;
    case ValueTypeRange:
        printf("%d to %d", value->as_range.start, value->as_range.end);
        break;
    default:
        RAEL_UNREACHABLE();
    }
}

void value_log(RaelValue value) {
    // only strings are printed differently when `log`ed then inside a stack
    switch (value->type) {
    case ValueTypeString:
        for (size_t i = 0; i < value->as_string.length; ++i)
            putchar(value->as_string.value[i]);
        break;
    default:
        value_log_as_original(value);
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
        if (value->as_number.is_float)
            return value->as_number.as_float != 0.0f;
        else
            return value->as_number.as_int != 0;
    case ValueTypeRoutine:
        return true;
    case ValueTypeStack:
        return value->as_stack.length != 0;
    case ValueTypeRange:
        return true;
    default:
        RAEL_UNREACHABLE();
    }
}
