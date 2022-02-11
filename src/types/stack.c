#include "rael.h"
#include "value.h"

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

    value_ref(value);
    values[length++] = value;

    // update the stack
    self->values = values;
    self->allocated = allocated;
    self->length = length;
}

// stack << value
RaelValue *stack_red(RaelStackValue *self, RaelValue *value) {
    stack_push(self, value);
    value_ref((RaelValue*)self);
    return (RaelValue*)self;
}

void stack_delete(RaelStackValue *self) {
    for (size_t i = 0; i < self->length; ++i) {
        value_deref(self->values[i]);
    }
    if (self->allocated > 0) {
        free(self->values);
    }
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

void stack_maybe_shrink(RaelStackValue *self) {
    if (self->allocated - self->length >= 8) {
        self->values = realloc(self->values, (self->allocated -= 8) * sizeof(RaelValue*));
    }
}

/*
 * Remove a value from a specified place, return it, and shift the rest of the values in the stack back.
 * :a ?= { 1, 2, 3 }
 * :a:pop(1) = 2
 * :a = { 1, 3 }
 */
static RaelValue *stack_method_pop(RaelStackValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    size_t pop_index;
    size_t length = stack_length(self);
    RaelValue *popped;

    (void)interpreter;

    switch (arguments_amount(args)) {
    case 0:
        if (length > 0) {
            pop_index = length - 1;
        } else {
            return BLAME_NEW_CSTR("Can't pop from an empty stack");
        }
        break;
    case 1: {
        RaelValue *arg1 = arguments_get(args, 0);
        RaelNumberValue *number;

        if (arg1->type != &RaelNumberType) {
            return BLAME_NEW_CSTR_ST("Expected number", *arguments_state(args, 0));
        }
        number = (RaelNumberValue*)arg1;
        if (!number_is_whole(number) || !number_positive(number)) {
            return BLAME_NEW_CSTR_ST("Expected a positive whole number", *arguments_state(args, 0));
        }

        pop_index = (size_t)number_to_int(number);

        // verify the pop index is valid
        if (pop_index >= length) {
            return BLAME_NEW_CSTR_ST("Index too big", *arguments_state(args, 0));
        }
        break;
    }
    default:
        RAEL_UNREACHABLE();
    }

    // get the popped value
    popped = stack_get(self, pop_index);

    // move everything back
    for (size_t i = pop_index; i < length - 1; ++i) {
        stack_set(self, i, stack_get(self, i + 1));
    }

    // dec length
    --self->length;
    stack_maybe_shrink(self);

    // no need to deref/ref because we take the value from the stack and use it once
    return popped;
}

/*
 * Get the index of the first occurance of the argument given.
 * :a ?= { 1, 2, 3 }
 * :a:findIndexOf(3) = 2
 * :a:findIndexOf(87) = -1
 * The second parameter is optional and if it is truthy (for example 1),
 * it searches for a matching pointer instead of a matching value.
 * :a:findIndexOf(2, 1) = -1
 * :a:findIndexOf(:a at 2, 1) = 2
 */
static RaelValue *stack_method_findIndexOf(RaelStackValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    size_t length = stack_length(self);
    RaelInt index = -1;
    RaelValue *compared;
    bool ptr_comparison;

    (void)interpreter;

    switch (arguments_amount(args)) {
    case 1:
        ptr_comparison = false;
        break;
    case 2:
        ptr_comparison = value_truthy(arguments_get(args, 1));
        break;
    default:
        return BLAME_NEW_CSTR("Expected 1 or 2 arguments");
    }

    // get value to compare to
    compared = arguments_get(args, 0);

    // loop and find a matching value
    for (size_t i = 0; index == -1 && i < length; ++i) {
        if (ptr_comparison) {
            if (stack_get(self, i) == compared) {
                index = (RaelInt)i;
            }
        } else {
            if (values_eq(stack_get(self, i), compared)) {
                index = (RaelInt)i;
            }
        }
    }

    return number_newi(index);
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

    .callable_info = NULL,
    .constructor_info = NULL,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = (RaelAsBoolFunc)stack_as_bool,
    .deallocator = (RaelSingleFunc)stack_delete,
    .repr = (RaelSingleFunc)stack_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = (RaelGetFunc)stack_get,
    .at_range = (RaelSliceFunc)stack_slice,

    .length = (RaelLengthFunc)stack_length,

    .methods = (MethodDecl[]) {
        RAEL_CMETHOD("pop", stack_method_pop, 0, 1),
        RAEL_CMETHOD("findIndexOf", stack_method_findIndexOf, 1, 2),
        RAEL_CMETHOD_TERMINATOR
    }
};
