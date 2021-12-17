#include "stack.h"
#include "value.h"

#include <stdbool.h>

RaelStackValue *stack_new(size_t overhead) {
    RaelStackValue *stack = RAEL_VALUE_NEW(ValueTypeStack, RaelStackValue);
    stack->allocated = overhead;
    stack->length = 0;
    stack->values = overhead == 0 ? NULL : malloc(overhead * sizeof(RaelValue*));
    return stack;
}

size_t stack_length(RaelStackValue *stack) {
    return stack->length;
}

RaelValue **stack_get_ptr(RaelStackValue *stack, size_t idx) {
    if (idx >= stack_length(stack))
        return NULL;
    return &stack->values[idx];
}

RaelValue *stack_get(RaelStackValue *stack, size_t idx) {
    RaelValue *value;
    if (idx >= stack_length(stack))
        return NULL;
    value = *stack_get_ptr(stack, idx);
    value_ref(value);
    return value;
}

RaelStackValue *stack_slice(RaelStackValue *stack, size_t start, size_t end) {
    RaelStackValue *new_stack;
    RaelValue **new_dump;
    size_t new_len;
    const size_t len = stack_length(stack);

    if (end < start || start > len || end > len)
        return NULL;

    new_dump = malloc((new_len = end - start) * sizeof(RaelValue*));

    for (size_t i = 0; i < new_len; ++i) {
        RaelValue *value = stack->values[start + i];
        value_ref(value);
        new_dump[i] = value;
    }

    new_stack = RAEL_VALUE_NEW(ValueTypeStack, RaelStackValue);
    new_stack->allocated = new_len;
    new_stack->length = new_len;
    new_stack->values = new_dump;

    return new_stack;
}

bool stack_set(RaelStackValue *stack, size_t idx, RaelValue *value) {
    RaelValue **value_address;

    if (idx >= stack_length(stack))
        return false;

    value_address = stack_get_ptr(stack, idx);
    value_deref(*value_address);
    *value_address = value;
    return true;
}

void stack_push(RaelStackValue *stack, RaelValue *value) {
    RaelValue **values;
    size_t allocated, length;

    values = stack->values;
    allocated = stack->allocated;
    length = stack_length(stack);

    // allocate additional space for stack if there isn't enough
    if (allocated == 0)
        values = malloc((allocated = 16) * sizeof(RaelValue*));
    else if (length >= allocated)
        values = realloc(values, (allocated += 16) * sizeof(RaelValue*));
    values[length++] = value;

    // update the stack
    stack->values = values;
    stack->allocated = allocated;
    stack->length = length;
}

void stack_delete(RaelStackValue *stack) {
    for (size_t i = 0; i < stack->length; ++i) {
        value_deref(stack->values[i]);
    }
    free(stack->values);
}

void stack_repr(RaelStackValue *stack) {
    printf("{ ");
    for (size_t i = 0; i < stack->length; ++i) {
        if (i > 0)
            printf(", ");
        value_repr(stack->values[i]);
    }
    printf(" }");
}

bool stack_eq(RaelStackValue *stack, RaelStackValue *stack2) {
    size_t len1 = stack_length(stack),
           len2 = stack_length(stack2);
    bool are_equal;
    if (len1 == len2) {
        are_equal = true;
        for (size_t i = 0; are_equal && i < len1; ++i) {
            if (!values_eq(stack->values[i], stack2->values[i])) {
                are_equal = false;
            }
        }
    } else { // if lengths are not equal, the stacks can't be equal
        are_equal = false;
    }
    return are_equal;
}

bool stack_as_bool(RaelStackValue *stack) {
    return stack_length(stack) > 0;
}
