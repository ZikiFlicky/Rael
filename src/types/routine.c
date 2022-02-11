#include "rael.h"

#include "routine.h"

void block_run(RaelInterpreter* const interpreter, struct Instruction **block, bool create_new_scope);
void interpreter_push_scope(RaelInterpreter* const interpreter, struct Scope *scope_addr);
void interpreter_pop_scope(RaelInterpreter* const interpreter);

RaelValue *routine_call(RaelRoutineValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    struct Scope *prev_scope, routine_scope;
    size_t amount_params = self->amount_parameters;

    // store last scopes
    prev_scope = interpreter->scope;
    // create new "scope chain"
    interpreter->scope = self->scope;
    interpreter_push_scope(interpreter, &routine_scope);

    for (size_t i = 0; i < amount_params; ++i) {
        RaelValue *value = arguments_get(args, i);
        assert(value); // you must get a value
        // set the parameter
        scope_set_local(interpreter->scope, self->parameters[i], value, false);
    }

    // run the block of code
    block_run(interpreter, self->block, false);

    if (interpreter->interrupt == ProgramInterruptReturn) {
        // if had a return statement
        ;
    } else {
        // if you get to the end of the function without returning anything, return a Void
        interpreter->returned_value = void_new();
    }

    // clear interrupt
    interpreter->interrupt = ProgramInterruptNone;
    // remove routine scope and restore previous scope
    interpreter_pop_scope(interpreter);
    interpreter->scope = prev_scope;
    return interpreter->returned_value;
}

void routine_repr(RaelRoutineValue *self) {
    printf("routine(");
    if (self->amount_parameters > 0) {
        printf(":%s", self->parameters[0]);
        for (size_t i = 1; i < self->amount_parameters; ++i)
            printf(", :%s", self->parameters[i]);
    }
    printf(")");
}

RaelInt routine_validate_args(RaelRoutineValue *self, size_t amount) {
    if (amount < self->amount_parameters) {
        return -1;
    } else if (amount > self->amount_parameters) {
        return 1;
    } else {
        return 0;
    }
}

static RaelCallableInfo routine_callable_info = {
    (RaelCallerFunc)routine_call,
    (RaelCanTakeFunc)routine_validate_args
};

RaelTypeValue RaelRoutineType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Routine",
    .op_add = NULL,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = NULL,
    .op_eq = NULL,
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    .callable_info = &routine_callable_info,
    .constructor_info = NULL,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = NULL,
    .deallocator = NULL,
    .repr = (RaelSingleFunc)routine_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL,

    .methods = NULL
};
