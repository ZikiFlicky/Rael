#include "value.h"
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

RaelValue value_create(enum ValueType type) {
    RaelValue value = malloc(sizeof(struct RaelValue));
    value->type = type;
    value->reference_count = 1;
    return value;
}

void value_deref(RaelValue value) {
    --value->reference_count;
    if (value->reference_count == 0) {
        switch (value->type) {
        case ValueTypeRoutine:
            break;
        case ValueTypeStack:
            for (size_t i = 0; i < value->as_stack.length; ++i) {
                value_deref(value->as_stack.values[i]);
            }
            free(value->as_stack.values);
            break;
        case ValueTypeString:
            switch (value->as_string.type) {
            case StringTypePure:
                if (!value->as_string.does_reference_ast && value->as_string.length)
                    free(value->as_string.value);
                break;
            case StringTypeSub:
                value_deref(value->as_string.reference_string);
                break;
            default:
                RAEL_UNREACHABLE();
            }
            break;
        case ValueTypeBlame:
            if (value->as_blame.value)
                value_deref(value->as_blame.value);
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

static char *value_type_to_string(enum ValueType type) {
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

void value_log_as_original(RaelValue value) {
    switch (value->type) {
    case ValueTypeNumber:
        if (value->as_number.is_float)
            printf("%.17g", value->as_number.as_float);
        else
            printf("%d", value->as_number.as_int);
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
    case ValueTypeType:
        printf("%s", value_type_to_string(value->as_type));
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
    case ValueTypeStack:
        return value->as_stack.length != 0;
    default:
        return true;
    }
}


bool values_equal(const RaelValue lhs, const RaelValue rhs) {
    bool res;
    // if they have the same pointer they must be equal
    if (lhs == rhs) {
        res = true;
    } else if (lhs->type != rhs->type) {
        // if they don't share the same type they are not equal
        res = false;
    } else {
        enum ValueType type = lhs->type;

        switch (type) {
        case ValueTypeNumber:
            res = number_eq(lhs->as_number, rhs->as_number).as_int;
            break;
        case ValueTypeString:
            if (lhs->as_string.length == rhs->as_string.length) {
                if (lhs->as_string.value == rhs->as_string.value)
                    res = true;
                else
                    res = strncmp(lhs->as_string.value, rhs->as_string.value, lhs->as_string.length) == 0;
            } else {
                res = false;
            }
            break;
        case ValueTypeType:
            res = lhs->as_type == rhs->as_type;
            break;
        case ValueTypeVoid:
            res = true;
            break;
        case ValueTypeStack:
            if (lhs->as_stack.length == rhs->as_stack.length) {
                res = true;
                for (size_t i = 0; i < lhs->as_stack.length; ++i) {
                    if (!values_equal(lhs->as_stack.values[i], rhs->as_stack.values[i])) {
                        res = false;
                        break;
                    }
                }
            } else {
                res = false;
            }
            break;
        case ValueTypeRange:
            res = lhs->as_range.start == rhs->as_range.start && lhs->as_range.end == rhs->as_range.end;
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

size_t value_get_length(RaelValue value) {
    size_t length;
    assert(value_is_iterable(value));
    switch (value->type) {
    case ValueTypeString:
        length = value->as_string.length;
        break;
    case ValueTypeStack:
        length = value->as_stack.length;
        break;
    case ValueTypeRange:
        length = (size_t)abs(value->as_range.end - value->as_range.start);
        break;
    default:
        RAEL_UNREACHABLE();
    }
    return length;
}

RaelValue value_at_idx(RaelValue value, size_t idx) {
    RaelValue out;

    assert(value_is_iterable(value));
    assert(idx < value_get_length(value));

    switch (value->type) {
    case ValueTypeStack:
        out = value->as_stack.values[idx];
        value_ref(out);
        break;
    case ValueTypeString:
        out = string_substr(value, idx, idx + 1);
        break;
    case ValueTypeRange:
        out = value_create(ValueTypeNumber);
        out->as_number.is_float = false;

        if (value->as_range.end > value->as_range.start)
            out->as_number.as_int = value->as_range.start + idx;
        else
            out->as_number.as_int = value->as_range.start - idx;
        break;
    default:
        RAEL_UNREACHABLE();
    }

    return out;
}

RaelValue string_substr(RaelValue value, size_t start, size_t end) {
    struct RaelStringValue substr;
    RaelValue new_string;

    assert(value->type == ValueTypeString);
    assert(end >= start);
    assert(start <= value->as_string.length && end <= value->as_string.length);

    substr.type = StringTypeSub;
    substr.value = value->as_string.value + start;
    substr.length = end - start;

    switch (value->as_string.type) {
    case StringTypePure: substr.reference_string = value; break;
    case StringTypeSub: substr.reference_string = value->as_string.reference_string; break;
    default: RAEL_UNREACHABLE();
    }

    value_ref(substr.reference_string);

    new_string = value_create(ValueTypeString);
    new_string->as_string = substr;

    return new_string;
}

RaelValue string_plus_string(RaelValue lhs, RaelValue rhs) {
    RaelValue string;

    assert(lhs->type == ValueTypeString);
    assert(rhs->type == ValueTypeString);

    string = value_create(ValueTypeString);
    string->as_string.type = StringTypePure;
    string->as_string.length = lhs->as_string.length + rhs->as_string.length;
    string->as_string.value = malloc((string->as_string.length) * sizeof(char));

    // copy other strings' contents
    strncpy(string->as_string.value, lhs->as_string.value, lhs->as_string.length);
    strncpy(string->as_string.value + lhs->as_string.length, rhs->as_string.value, rhs->as_string.length);

    return string;
}
