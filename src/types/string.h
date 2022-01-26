#ifndef RAEL_STRING_H
#define RAEL_STRING_H

#include "common.h"
#include "value.h"
#include "number.h"

#include <stdbool.h>
#include <stddef.h>

#define RAEL_STRING_FROM_CSTR(string) (string_new_pure_cpy(string, sizeof(string) / sizeof(char) - 1))

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

extern RaelTypeValue RaelStringType;

RaelValue *string_new_pure(char *source, size_t length, bool can_free);

RaelValue *string_new_pure_cpy(char *source, size_t length);

RaelValue *string_new_substr(char *source, size_t length, RaelStringValue *reference_string);

void string_delete(RaelStringValue *self);

char *string_to_cstr(RaelStringValue *self);

size_t string_length(RaelStringValue *self);

RaelValue *string_get(RaelStringValue *self, size_t idx);

char string_get_char(RaelStringValue *self, size_t idx);

RaelValue *string_slice(RaelStringValue *self, size_t start, size_t end);

RaelValue *string_add(RaelStringValue *self, RaelValue *string2);

bool string_eq(RaelStringValue *string, RaelStringValue *string2);

void string_repr(RaelStringValue *self);

bool string_as_bool(RaelStringValue *self);

#endif /* RAEL_STRING_H */
