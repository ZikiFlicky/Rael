#ifndef RAEL_STACK_H
#define RAEL_STACK_H

#include "common.h"
#include "value.h"

#include <stdbool.h>
#include <stddef.h>

/* This is an implementation of the stack value type in Rael,
   not the execution stack's implementation */

typedef struct RaelStackValue {
    RAEL_VALUE_BASE;
    RaelValue **values;
    size_t length, allocated;
} RaelStackValue;

RaelStackValue *stack_new(size_t overhead);

void stack_delete(RaelStackValue *stack); // FIXME: should this dereference the value too?

size_t stack_length(RaelStackValue *stack);

RaelValue **stack_get_ptr(RaelStackValue *stack, size_t idx);

RaelValue *stack_get(RaelStackValue *stack, size_t idx);

RaelStackValue *stack_slice(RaelStackValue *stack, size_t start, size_t end);

bool stack_set(RaelStackValue *stack, size_t idx, RaelValue *value);

void stack_push(RaelStackValue *stack, RaelValue *value);

void stack_repr(RaelStackValue *stack);

bool stack_eq(RaelStackValue *stack, RaelStackValue *stack2);

bool stack_as_bool(RaelStackValue *stack);

#endif /* RAEL_STACK_H */
