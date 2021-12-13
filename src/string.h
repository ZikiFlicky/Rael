#ifndef RAEL_STRING_H
#define RAEL_STRING_H

#include <stdbool.h>
#include <stddef.h>

#define RAEL_STRING_FROM_RAWSTR(string) (string_new_pure_alloc(string, sizeof(string) / sizeof(char) - 1))

typedef struct RaelValue* RaelValue;

struct RaelStringValue {
    enum {
        StringTypePure,
        StringTypeSub
    } type;
    char *value;
    size_t length;
    union {
        bool can_be_freed;
        RaelValue reference_string;
    };
};

RaelValue string_new_pure(char *strptr, size_t length, bool can_free);

RaelValue string_new_pure_alloc(char *strptr, size_t length);

void stringvalue_delete(struct RaelStringValue *string);

size_t string_get_length(RaelValue string);

RaelValue string_get(RaelValue string, size_t idx);

char string_get_char(RaelValue string, size_t idx);

RaelValue string_slice(RaelValue string, size_t start, size_t end);

RaelValue strings_add(RaelValue string, RaelValue string2);

RaelValue string_precede_with_number(RaelValue number, RaelValue string);

RaelValue string_add_number(RaelValue string, RaelValue number);

bool string_eq(RaelValue string, RaelValue string2);

void stringvalue_repr(struct RaelStringValue *string);

#endif /* RAEL_STRING_H */
