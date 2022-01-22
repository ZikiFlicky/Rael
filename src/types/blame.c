#include "rael.h"

#include "blame.h"

RaelValue *blame_new(RaelValue *message, struct State *state) {
    RaelBlameValue *blame = RAEL_VALUE_NEW(RaelBlameType, RaelBlameValue);
    if (state) {
        blame->state_defined = true;    
        blame->original_place = *state;
    } else {
        blame->state_defined = false;
    }
    
    blame->message = message;
    return (RaelValue*)blame;
}

/* return true if the value is a blame */
bool blame_validate(RaelValue *value) {
    return value->type == &RaelBlameType;
}

RaelValue *blame_get_message(RaelBlameValue *blame) {
    return blame->message;
}

void blame_delete(RaelBlameValue *blame) {
    RaelValue *message = blame_get_message(blame);
    // if there is a blame message, dereference it
    if (message)
        value_deref(message);
}

/* if there was no state on the blame beforehand, add a state */
void blame_set_state(RaelBlameValue *self, struct State state) {
    // define a new state only if there isn't one yet
    if (!self->state_defined) {
        self->state_defined = true;
        self->original_place = state;
    }
}

RaelTypeValue RaelBlameType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Blame",
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

    .op_call = NULL,
    .op_construct = NULL,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = NULL,
    .deallocator = (RaelSingleFunc)blame_delete,
    .repr = NULL,
    .logger = NULL,

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL,

    .methods = NULL
};
