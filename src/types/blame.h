#ifndef RAEL_BLAME_H
#define RAEL_BLAME_H

#include "value.h"

/* return a RaelValue of type blame, storing a RaelValue of type string with the value of `string` */
#define BLAME_NEW_CSTR(string) ((RaelValue*)blame_new((RaelValue*)RAEL_STRING_FROM_CSTR((string)), NULL))
/* return a RaelValue of type blame, storing a RaelValue of type string with the value of `string` and the state `state` */
#define BLAME_NEW_CSTR_ST(string, state) ((RaelValue*)blame_new((RaelValue*)RAEL_STRING_FROM_CSTR((string)), &(state)))

extern RaelTypeValue RaelBlameType;

typedef struct RaelBlameValue {
    RAEL_VALUE_BASE;
    bool state_defined;
    RaelValue *message;
    struct State original_place;
} RaelBlameValue;

/* create a new blame from a message and a state  */
RaelValue *blame_new(RaelValue *message, struct State *state);

/* if there isn't a state defined in the blame, set it to the new state */
void blame_set_state(RaelBlameValue *value, struct State state);

/* check if the value is a blame */
bool blame_validate(RaelValue *value);

/* get the message of the blame */
RaelValue *blame_get_message(RaelBlameValue *blame);

#endif /* RAEL_BLAME_H */
