#ifndef RAEL_STRING_H
#define RAEL_STRING_H

#include <stddef.h>

struct RaelStringValue {
    char *value;
    size_t length;
    bool does_reference_ast;
};

#endif // RAEL_STRING_H
