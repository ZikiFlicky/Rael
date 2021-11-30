#ifndef RAEL_STRING_H
#define RAEL_STRING_H

#include <stdbool.h>
#include <stddef.h>

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

RaelValue string_get(RaelValue string, size_t idx);

RaelValue string_slice(RaelValue string, size_t start, size_t end);

RaelValue string_plus_string(RaelValue string, RaelValue string2);

size_t string_get_length(RaelValue string);

void stringvalue_delete(struct RaelStringValue *string);

void stringvalue_repr(struct RaelStringValue *string);

RaelValue string_new_pure_alloc(char *strptr, size_t length);

#define RAEL_STRING_FROM_RAW(string) string_new_pure_alloc(string, sizeof(string) / sizeof(char) - 1)

#endif /* RAEL_STRING_H */
