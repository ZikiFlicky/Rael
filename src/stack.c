#include "stack.h"
#include "value.h"

#include <assert.h>
#include <stdbool.h>

RaelValue *stack_new(size_t overhead) {
    RaelValue *stack = value_new(ValueTypeStack);
    stack->as_stack.allocated = overhead;
    stack->as_stack.length = 0;
    stack->as_stack.values = overhead == 0 ? NULL : malloc(overhead * sizeof(RaelValue*));
    return stack;
}

size_t stack_get_length(RaelValue *stack) {
    assert(stack->type == ValueTypeStack);
    return stack->as_stack.length;
}

RaelValue **stack_get_ptr(RaelValue *stack, size_t idx) {
    assert(stack->type == ValueTypeStack);
    if (idx >= stack_get_length(stack))
        return NULL;
    return &stack->as_stack.values[idx];
}

RaelValue *stack_get(RaelValue *stack, size_t idx) {
    RaelValue *value;
    assert(stack->type == ValueTypeStack);
    if (idx >= stack_get_length(stack))
        return NULL;
    value = *stack_get_ptr(stack, idx);
    value_ref(value);
    return value;
}

RaelValue *stack_slice(RaelValue *stack, size_t start, size_t end) {
    RaelValue *new_stack;
    RaelValue **new_dump;
    size_t length, stack_length;

    assert(stack->type == ValueTypeStack);
    stack_length = stack_get_length(stack);
    if (end < start || start > stack_length || end > stack_length)
        return NULL;

    new_dump = malloc((length = end - start) * sizeof(RaelValue*));

    for (size_t i = 0; i < length; ++i) {
        RaelValue *value = stack->as_stack.values[start + i];
        value_ref(value);
        new_dump[i] = value;
    }

    new_stack = value_new(ValueTypeStack);
    new_stack->as_stack.allocated = length;
    new_stack->as_stack.length = length;
    new_stack->as_stack.values = new_dump;

    return new_stack;
}

bool stack_set(RaelValue *stack, size_t idx, RaelValue *value) {
    RaelValue **value_address;
    assert(stack->type == ValueTypeStack);

    if (idx >= stack_get_length(stack))
        return false;

    value_address = stack_get_ptr(stack, idx);
    value_deref(*value_address);
    *value_address = value;
    return true;
}

void stack_push(RaelValue *stack, RaelValue *value) {
    RaelValue **values;
    size_t allocated, length;
    assert(stack->type == ValueTypeStack);

    values = stack->as_stack.values;
    allocated = stack->as_stack.allocated;
    length = stack_get_length(stack);

    // allocate additional space for stack if there isn't enough
    if (allocated == 0)
        values = malloc((allocated = 16) * sizeof(RaelValue*));
    else if (length >= allocated)
        values = realloc(values, (allocated += 16) * sizeof(RaelValue*));
    values[length++] = value;

    stack->as_stack = (RaelStackValue) {
        .values = values,
        .allocated = allocated,
        .length = length
    };
}

void stackvalue_delete(RaelStackValue *stack) {
    for (size_t i = 0; i < stack->length; ++i) {
        value_deref(stack->values[i]);
    }
    free(stack->values);
}

void stackvalue_repr(RaelStackValue *stack) {
    printf("{ ");
    for (size_t i = 0; i < stack->length; ++i) {
        if (i > 0)
            printf(", ");
        value_repr(stack->values[i]);
    }
    printf(" }");
}

bool stack_equals_stack(RaelValue *stack, RaelValue *stack2) {
    size_t len1 = stack_get_length(stack),
           len2 = stack_get_length(stack2);
    bool are_equal;
    if (len1 == len2) {
        are_equal = true;
        for (size_t i = 0; are_equal && i < len1; ++i) {
            if (!values_equal(stack->as_stack.values[i], stack2->as_stack.values[i])) {
                are_equal = false;
            }
        }
    } else {
        are_equal = false;
    }
    return are_equal;
}
