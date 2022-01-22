#ifndef RAEL_RANGE_H
#define RAEL_RANGE_H

#include "value.h"

extern RaelTypeValue RaelRangeType;

typedef struct RaelRangeValue {
    RAEL_VALUE_BASE;
    RaelInt start, end;
} RaelRangeValue;

/* create a new range from two whole numbers */
RaelValue *range_new(RaelInt start, RaelInt end);

/* get the length of the range */
size_t range_length(RaelRangeValue *range);

/*
 * Get the value of the range at the index given.
 * Notice that the function asserts that the index isn't too big,
 * so you'll have to check the index and the length before calling.
 */
RaelInt range_at(RaelRangeValue *self, size_t idx);

#endif /* RAEL_RANGE_H */
