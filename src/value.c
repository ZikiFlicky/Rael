#include "value.h"

#include <stdlib.h>

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
