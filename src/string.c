#include "string.h"
#include "value.h"

#include <assert.h>
#include <string.h>

RaelValue string_new_pure(char *strptr, size_t length, bool can_free) {
    RaelValue string = value_create(ValueTypeString);
    string->as_string = (struct RaelStringValue) {
        .type = StringTypePure,
        .can_be_freed = can_free,
        .value = strptr,
        .length = length
    };
    return string;
}

RaelValue string_new_pure_alloc(char *strptr, size_t length) {
    char *allocated = malloc(length * sizeof(char));
    memcpy(allocated, strptr, length * sizeof(char));
    return string_new_pure(allocated, length, true);
}

/* get length of a string */
size_t string_get_length(RaelValue string) {
    assert(string->type == ValueTypeString);
    return string->as_string.length;
}

/* get the char at idx */
char string_get_char(RaelValue string, size_t idx) {
    size_t string_length;
    assert(string->type == ValueTypeString);
    string_length = string_get_length(string);
    if (idx >= string_length)
        return '\0';
    else
        return string->as_string.value[idx];
}

/* get a string at idx */
RaelValue string_get(RaelValue string, size_t idx) {
    RaelValue new_string;
    size_t string_length;
    assert(string->type == ValueTypeString);
    string_length = string_get_length(string);
    if (idx > string_length)
        return NULL;

    if (idx == string_length) {
        new_string = string_new_pure(NULL, 0, true);
    } else {
        // allocate a 1-lengthed string and put the character into it
        char *ptr = malloc(1 * sizeof(char));
        ptr[0] = string->as_string.value[idx];
        new_string = string_new_pure(ptr, 1, true);
    }
    
    return new_string;
}

RaelValue string_slice(RaelValue string, size_t start, size_t end) {
    struct RaelStringValue substr;
    RaelValue new_string;

    assert(string->type == ValueTypeString);
    assert(end >= start);
    assert(start <= string_get_length(string) && end <= string_get_length(string));

    substr.type = StringTypeSub;
    substr.value = string->as_string.value + start;
    substr.length = end - start;

    switch (string->as_string.type) {
    case StringTypePure: substr.reference_string = string; break;
    case StringTypeSub: substr.reference_string = string->as_string.reference_string; break;
    default: RAEL_UNREACHABLE();
    }

    value_ref(substr.reference_string);

    new_string = value_create(ValueTypeString);
    new_string->as_string = substr;

    return new_string;
}

RaelValue strings_add(RaelValue string, RaelValue string2) {
    RaelValue new_string;
    char *strptr;
    size_t length, str1len, str2len;

    assert(string->type == ValueTypeString);
    assert(string2->type == ValueTypeString);

    // calculate length of strings
    str1len = string_get_length(string);
    str2len = string_get_length(string2);

    // calculate size of new string and allocate it
    length = str1len + str2len;
    strptr = malloc(length * sizeof(char));

    // copy other strings' contents into the new string
    strncpy(strptr, string->as_string.value, str1len);
    strncpy(strptr + str1len, string2->as_string.value, str2len);

    // create the new string object
    new_string = string_new_pure(strptr, length, true);
    return new_string;
}

void stringvalue_delete(struct RaelStringValue *string) {
    switch (string->type) {
    case StringTypePure:
        if (string->can_be_freed && string->length)
            free(string->value);
        break;
    case StringTypeSub:
        value_deref(string->reference_string);
        break;
    default:
        RAEL_UNREACHABLE();
    }
}

void stringvalue_repr(struct RaelStringValue *string) {
    putchar('"');
    for (size_t i = 0; i < string->length; ++i) {
        switch (string->value[i]) {
        case '\n':
            printf("\\n");
            break;
        case '\r':
            printf("\\r");
            break;
        case '\t':
            printf("\\t");
            break;
        case '"':
            printf("\\\"");
            break;
        case '\\':
            printf("\\\\");
            break;
        default:
            putchar(string->value[i]);
        }
    }
    putchar('"');
}

/* number + string */
RaelValue string_precede_with_number(RaelValue number, RaelValue string) {
    RaelInt n;
    char *strptr;
    size_t string_length;

    assert(number->type == ValueTypeNumber);
    assert(string->type == ValueTypeString);

    if (!number_is_whole(number->as_number)) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected the number to be a whole number");
    }
    n = number_to_int(number->as_number);
    if (!rael_int_in_range_of_char(n)) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected the number to be in ascii");
    }

    // get length of rhs
    string_length = string_get_length(string);
    strptr = malloc((string_length + 1) * sizeof(char));

    strptr[0] = (char)n;
    strncpy(strptr+1, string->as_string.value, string_length);
    return string_new_pure(strptr, string_length + 1, true);
}

/* string + number */
RaelValue string_add_number(RaelValue string, RaelValue number) {
    RaelInt n;
    char *strptr;
    size_t string_length;

    assert(string->type == ValueTypeString);
    assert(number->type == ValueTypeNumber);

    if (!number_is_whole(number->as_number)) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected the number to be a whole number");
    }

    n = number_to_int(number->as_number);
    if (!rael_int_in_range_of_char(n)) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected the number to be in ascii");
    }

    // get length of rhs
    string_length = string_get_length(string);
    strptr = malloc((string_length + 1) * sizeof(char));

    strncpy(strptr, string->as_string.value, string_length);
    strptr[string_length] = (char)n;
    return string_new_pure(strptr, string_length + 1, true);
}

/* is a string equal to another string? */
bool string_eq(RaelValue string, RaelValue string2) {
    size_t string_length, string2_length;
    assert(string->type == ValueTypeString);
    assert(string2->type == ValueTypeString);

    // get lengths
    string_length = string_get_length(string);
    string2_length = string_get_length(string2);
    // if the strings have to have the same length
    if (string_length == string2_length) {
        // if the base value is the same (e.g they inherit from the same constant value or are equal substrings)
        if (string->as_string.value == string2->as_string.value) {
            return true;
        } else {
            return strncmp(string->as_string.value, string2->as_string.value, string_length) == 0;
        }
    } else {
        // if the strings differ in length, they are not equal
        return false;
    }
}
