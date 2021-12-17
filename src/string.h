#ifndef RAEL_STRING_H
#define RAEL_STRING_H

#include "common.h"
#include "value.h"
#include "number.h"

#include <stdbool.h>
#include <stddef.h>

#define RAEL_STRING_FROM_RAWSTR(string) (string_new_pure_alloc(string, sizeof(string) / sizeof(char) - 1))

struct RaelStringValue;

typedef struct RaelStringValue RaelStringValue;

typedef struct RaelStringValue {
    RAEL_VALUE_BASE;
    enum {
        StringTypePure,
        StringTypeSub
    } type;
    char *source;
    size_t length;
    union {
        bool can_be_freed;
        RaelStringValue *reference_string;
    };
} RaelStringValue;

RaelStringValue *string_new_pure(char *source, size_t length, bool can_free);

RaelStringValue *string_new_pure_alloc(char *source, size_t length);

void string_delete(RaelStringValue *string);

size_t string_length(RaelStringValue *string);

RaelValue *string_get(RaelStringValue *string, size_t idx);

char string_get_char(RaelStringValue *string, size_t idx);

RaelStringValue *string_slice(RaelStringValue *string, size_t start, size_t end);

RaelStringValue *strings_add(RaelStringValue *string, RaelStringValue *string2);

/* returns a RaelValue because it can also return a blame */
RaelValue *string_precede_with_number(RaelNumberValue *number, RaelStringValue *string);

/* returns a RaelValue because it can also return a blame */
RaelValue *string_add_number(RaelStringValue *string, RaelNumberValue *number);

bool string_eq(RaelStringValue *string, RaelStringValue *string2);

void string_repr(RaelStringValue *string);

bool string_as_bool(RaelStringValue *string);

#endif /* RAEL_STRING_H */
