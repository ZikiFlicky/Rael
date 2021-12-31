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

extern RaelTypeValue RaelStackType;

RaelValue *stack_new(size_t overhead);

void stack_delete(RaelStackValue *self);

size_t stack_length(RaelStackValue *self);

RaelValue **stack_get_ptr(RaelStackValue *self, size_t idx);

RaelValue *stack_get(RaelStackValue *self, size_t idx);

RaelValue *stack_slice(RaelStackValue *self, size_t start, size_t end);

bool stack_set(RaelStackValue *self, size_t idx, RaelValue *value);

void stack_push(RaelStackValue *self, RaelValue *value);

void stack_repr(RaelStackValue *self);

bool stack_eq(RaelStackValue *self, RaelStackValue *value);

bool stack_as_bool(RaelStackValue *self);

#endif /* RAEL_STACK_H */
