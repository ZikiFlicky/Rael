#include "string.h"
#include "value.h"

#include <assert.h>
#include <string.h>

/* get length of a string */
size_t string_get_length(RaelValue string) {
    assert(string->type == ValueTypeString);
    return string->as_string.length;
}

/* get char of a string at idx */
RaelValue string_get(RaelValue string, size_t idx) {
    RaelValue new_string;
    char *string_ptr;
    size_t string_length;
    assert(string->type == ValueTypeString);
    string_length = string_get_length(string);
    if (idx >= string_length)
        return NULL;

    // allocate a 1-lengthed string and put the character into it
    string_ptr = malloc(1 * sizeof(char));
    string_ptr[0] = string->as_string.value[idx];
    new_string = value_create(ValueTypeString);
    new_string->as_string = (struct RaelStringValue) {
        .type = StringTypePure,
        .does_reference_ast = false,
        .value = string_ptr,
        .length = 1
    };
    return new_string;
}

RaelValue string_slice(RaelValue value, size_t start, size_t end) {
    struct RaelStringValue substr;
    RaelValue new_string;

    assert(value->type == ValueTypeString);
    assert(end >= start);
    assert(start <= value->as_string.length && end <= value->as_string.length);

    substr.type = StringTypeSub;
    substr.value = value->as_string.value + start;
    substr.length = end - start;

    switch (value->as_string.type) {
    case StringTypePure: substr.reference_string = value; break;
    case StringTypeSub: substr.reference_string = value->as_string.reference_string; break;
    default: RAEL_UNREACHABLE();
    }

    value_ref(substr.reference_string);

    new_string = value_create(ValueTypeString);
    new_string->as_string = substr;

    return new_string;
}

RaelValue string_plus_string(RaelValue lhs, RaelValue rhs) {
    RaelValue string;

    assert(lhs->type == ValueTypeString);
    assert(rhs->type == ValueTypeString);

    string = value_create(ValueTypeString);
    string->as_string.type = StringTypePure;
    string->as_string.length = lhs->as_string.length + rhs->as_string.length;
    string->as_string.value = malloc((string->as_string.length) * sizeof(char));

    // copy other strings' contents
    strncpy(string->as_string.value, lhs->as_string.value, lhs->as_string.length);
    strncpy(string->as_string.value + lhs->as_string.length, rhs->as_string.value, rhs->as_string.length);

    return string;
}
