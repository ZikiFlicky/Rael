#include "stack.h"
#include "value.h"

#include <assert.h>
#include <stdbool.h>

static inline bool stack_validate(RaelValue *value) {
    return value->type == &RaelStackType;
}

RaelValue *stack_new(size_t overhead) {
    RaelStackValue *stack = RAEL_VALUE_NEW(RaelStackType, RaelStackValue);
    stack->allocated = overhead;
    stack->length = 0;
    stack->values = overhead == 0 ? NULL : malloc(overhead * sizeof(RaelValue*));
    return (RaelValue*)stack;
}

size_t stack_length(RaelStackValue *stack) {
    return stack->length;
}

RaelValue **stack_get_ptr(RaelStackValue *self, size_t idx) {
    // if out of range
    if (idx >= stack_length(self))
        return NULL;
    return &self->values[idx];
}

RaelValue *stack_get(RaelStackValue *self, size_t idx) {
    RaelValue **value_ptr = stack_get_ptr(self, idx);

    if (!value_ptr)
        return NULL;
    value_ref(*value_ptr);
    return *value_ptr;
}

RaelValue *stack_slice(RaelStackValue *self, size_t start, size_t end) {
    RaelStackValue *new_stack;
    RaelValue **new_dump;
    size_t new_len;
    const size_t len = stack_length(self);

    // out of range
    if (end < start || start > len || end > len)
        return NULL;

    // allocate a new dump of pointers
    new_dump = malloc((new_len = end - start) * sizeof(RaelValue*));

    for (size_t i = 0; i < new_len; ++i) {
        RaelValue *value = self->values[start + i];
        value_ref(value);
        new_dump[i] = value;
    }

    new_stack = RAEL_VALUE_NEW(RaelStackType, RaelStackValue);
    new_stack->allocated = new_len;
    new_stack->length = new_len;
    new_stack->values = new_dump;

    return (RaelValue*)new_stack;
}

bool stack_set(RaelStackValue *self, size_t idx, RaelValue *value) {
    RaelValue **value_address = stack_get_ptr(self, idx);
    // index out of range
    if (!value_address)
        return NULL;
    value_deref(*value_address);
    *value_address = value;
    return true;
}

void stack_push(RaelStackValue *self, RaelValue *value) {
    RaelValue **values;
    size_t allocated, length;

    values = self->values;
    allocated = self->allocated;
    length = stack_length(self);

    // allocate additional space for stack if there isn't enough
    if (allocated == 0)
        values = malloc((allocated = 16) * sizeof(RaelValue*));
    else if (length >= allocated)
        values = realloc(values, (allocated += 16) * sizeof(RaelValue*));
    values[length++] = value;

    // update the stack
    self->values = values;
    self->allocated = allocated;
    self->length = length;
}

// stack << value
RaelValue *stack_red(RaelStackValue *self, RaelValue *value) {
    value_ref(value);
    stack_push(self, value);
    value_ref((RaelValue*)self);
    return (RaelValue*)self;
}

void stack_delete(RaelStackValue *self) {
    for (size_t i = 0; i < self->length; ++i) {
        value_deref(self->values[i]);
    }
    free(self->values);
}

void stack_repr(RaelStackValue *self) {
    printf("{ ");
    for (size_t i = 0; i < self->length; ++i) {
        if (i > 0)
            printf(", ");
        value_repr(self->values[i]);
    }
    printf(" }");
}

bool stack_eq(RaelStackValue *self, RaelStackValue *value) {
    bool are_equal;
    size_t len1 = stack_length(self),
           len2 = stack_length(value);

    if (len1 == len2) {
        are_equal = true;
        for (size_t i = 0; are_equal && i < len1; ++i) {
            if (!values_eq(self->values[i], value->values[i])) {
                are_equal = false;
            }
        }
    } else { // if lengths are not equal, the stacks can't be equal
        are_equal = false;
    }

    return are_equal;
}

bool stack_as_bool(RaelStackValue *self) {
    return stack_length(self) > 0;
}

RaelTypeValue RaelStackType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Stack",
    .op_add = NULL,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = (RaelBinExprFunc)stack_red,
    .op_eq = (RaelBinCmpFunc)stack_eq,
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    .op_call = NULL,
    .op_construct = NULL,

    .as_bool = (RaelAsBoolFunc)stack_as_bool,
    .deallocator = (RaelSingleFunc)stack_delete,
    .repr = (RaelSingleFunc)stack_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = (RaelGetFunc)stack_get,
    .at_range = (RaelSliceFunc)stack_slice,

    .length = (RaelLengthFunc)stack_length
};
