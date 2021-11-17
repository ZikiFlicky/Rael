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
        bool does_reference_ast;
        RaelValue reference_string;
    };
};

RaelValue string_get(RaelValue string, size_t idx);

RaelValue string_slice(RaelValue string, size_t start, size_t end);

RaelValue string_plus_string(RaelValue string, RaelValue string2);

size_t string_get_length(RaelValue string);

#endif /* RAEL_STRING_H */
