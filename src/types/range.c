#include "rael.h"

#include "range.h"

RaelValue *range_new(RaelInt start, RaelInt end) {
    RaelRangeValue *range = RAEL_VALUE_NEW(RaelRangeType, RaelRangeValue);
    range->start = start;
    range->end = end;
    return (RaelValue*)range;
}

RaelInt range_at(RaelRangeValue *self, size_t idx) {
    assert(idx <= range_length(self));
    // get the number, depending on the direction
    if (self->end > self->start)
        return self->start + (RaelInt)idx;
    else
        return self->start - (RaelInt)idx;

}

RaelValue *range_slice(RaelRangeValue *self, size_t start, size_t end) {
    if (end > range_length(self)) {
        return BLAME_NEW_CSTR("Range slicing out of bounds");
    }
    return range_new(range_at(self, start),
                     range_at(self, end));
}

size_t range_length(RaelRangeValue *range) {
    return (size_t)rael_int_abs(range->end - range->start);
}

bool range_as_bool(RaelRangeValue *self) {
    return range_length(self) != 0;
}

RaelValue *range_get(RaelRangeValue *self, size_t idx) {
    if (idx >= range_length(self))
        return NULL;

    // return the number
    return number_newi(range_at(self, idx));
}

bool range_eq(RaelRangeValue *self, RaelRangeValue *value) {
    return self->start == value->start && self->end == value->end;
}

void range_repr(RaelRangeValue *self) {
    printf("%ld to %ld", self->start, self->end);
}

RaelValue *range_construct(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelInt start, end;

    (void)interpreter;

    switch (arguments_amount(args)) {
    case 1: {
        RaelValue *arg1 = arguments_get(args, 0);

        // verify the argument is a whole number
        if (arg1->type != &RaelNumberType) {
            return BLAME_NEW_CSTR_ST("Expected number", *arguments_state(args, 0));
        }
        if (!number_is_whole((RaelNumberValue*)arg1)) {
            return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 0));
        }
        start = 0;
        end = number_to_int((RaelNumberValue*)arg1);
        break;
    }
    case 2: {
        RaelValue *arg1 = arguments_get(args, 0),
                  *arg2 = arguments_get(args, 1);

        // verify the two arguments are whole numbers
        if (arg1->type != &RaelNumberType) {
            return BLAME_NEW_CSTR_ST("Expected a number",
                                     *arguments_state(args, 0));
        }
        if (!number_is_whole((RaelNumberValue*)arg1)) {
            return BLAME_NEW_CSTR_ST("Expected a whole number",
                                     *arguments_state(args, 0));
        }
        if (arg2->type != &RaelNumberType) {
            return BLAME_NEW_CSTR_ST("Expected a number",
                                     *arguments_state(args, 1));
        }
        if (!number_is_whole((RaelNumberValue*)arg2)) {
            return BLAME_NEW_CSTR_ST("Expected a whole number",
                                     *arguments_state(args, 1));
        }
        start = number_to_int((RaelNumberValue*)arg1);
        end = number_to_int((RaelNumberValue*)arg2);
        break;
    }
    default:
        RAEL_UNREACHABLE();
    }
    return range_new(start, end);
}

static RaelConstructorInfo range_constructor_info = {
    (RaelConstructorFunc)range_construct,
    true,
    1,
    2
};

RaelTypeValue RaelRangeType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Range",
    .op_add = NULL,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = NULL,
    .op_eq = (RaelBinCmpFunc)range_eq,
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    .callable_info = NULL,
    .constructor_info = &range_constructor_info,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = (RaelAsBoolFunc)range_as_bool,
    .deallocator = NULL,
    .repr = (RaelSingleFunc)range_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = (RaelGetFunc)range_get,
    .at_range = (RaelSliceFunc)range_slice,

    .length = (RaelLengthFunc)range_length,

    .methods = NULL
};
