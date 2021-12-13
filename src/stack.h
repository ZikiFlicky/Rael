#ifndef RAEL_STACK_H
#define RAEL_STACK_H

#include <stdbool.h>
#include <stddef.h>

typedef struct RaelValue* RaelValue;

struct RaelStackValue {
    RaelValue *values;
    size_t length, allocated;
};

RaelValue stack_new(size_t overhead);

void stackvalue_delete(struct RaelStackValue *stack);

size_t stack_get_length(RaelValue stack);

RaelValue *stack_get_ptr(RaelValue stack, size_t idx);

RaelValue stack_get(RaelValue stack, size_t idx);

bool stack_set(RaelValue stack, size_t idx, RaelValue value);

void stack_push(RaelValue stack, RaelValue value);

RaelValue stack_slice(RaelValue stack, size_t start, size_t end);

void stackvalue_repr(struct RaelStackValue *stack);

bool stack_equals_stack(RaelValue stack, RaelValue stack2);

#endif /* RAEL_STACK_H */
