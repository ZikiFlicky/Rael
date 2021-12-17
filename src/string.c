#include "common.h"
#include "value.h"
#include "string.h"
#include "number.h"

#include <assert.h>
#include <string.h>

RaelStringValue *string_new_pure(char *source, size_t length, bool can_free) {
    RaelStringValue *string = RAEL_VALUE_NEW(ValueTypeString, RaelStringValue);
    string->type = StringTypePure;
    string->can_be_freed = can_free;
    string->source = source;
    string->length = length;
    return string;
}

/* create a new value from a string pointer and its length */
RaelStringValue *string_new_pure_alloc(char *source, size_t length) {
    char *allocated = malloc(length * sizeof(char));
    memcpy(allocated, source, length * sizeof(char));
    return string_new_pure(allocated, length, true);
}

/* get length of a string */
size_t string_length(RaelStringValue *string) {
    return string->length;
}

/* get the char at idx */
char string_get_char(RaelStringValue *string, size_t idx) {
    size_t str_length;
    str_length = string_length(string);
    if (idx >= str_length)
        return '\0';
    else
        return string->source[idx];
}

/* get a string at idx */
RaelValue *string_get(RaelStringValue *string, size_t idx) {
    RaelStringValue *new_string;
    size_t str_length;

    // get string length
    str_length = string_length(string);
    if (idx > str_length)
        return NULL;

    if (idx == str_length) {
        new_string = string_new_pure(NULL, 0, true);
    } else {
        // allocate a 1-lengthed string and put the character into it
        char *ptr = malloc(1 * sizeof(char));
        ptr[0] = string->source[idx];
        new_string = string_new_pure(ptr, 1, true);
    }
    
    return (RaelValue*)new_string;
}

RaelStringValue *string_slice(RaelStringValue *string, size_t start, size_t end) {
    char *source;
    size_t length;
    RaelStringValue *reference_string, *new_string;

    assert(end >= start);
    assert(start <= string_length(string) && end <= string_length(string));

    source = string->source + start;
    length = end - start;

    switch (string->type) {
    case StringTypePure: reference_string = string; break;
    case StringTypeSub: reference_string = string->reference_string; break;
    default: RAEL_UNREACHABLE();
    }

    // reference the reference string, because it is now used by our string
    value_ref((RaelValue*)reference_string);

    // create a new substring
    new_string = RAEL_VALUE_NEW(ValueTypeString, RaelStringValue);
    new_string->type = StringTypeSub;
    new_string->source = source;
    new_string->length = length;
    new_string->reference_string = reference_string;

    return new_string;
}

RaelStringValue *strings_add(RaelStringValue *string, RaelStringValue *string2) {
    RaelStringValue *new_string;
    char *source;
    size_t length, str1len, str2len;

    // calculate length of strings
    str1len = string_length(string);
    str2len = string_length(string2);

    // calculate size of new string and allocate it
    length = str1len + str2len;
    source = malloc(length * sizeof(char));

    // copy other strings' contents into the new string
    strncpy(source, string->source, str1len);
    strncpy(source + str1len, string2->source, str2len);

    // create the new string object
    new_string = string_new_pure(source, length, true);
    return new_string;
}

void string_delete(RaelStringValue *string) {
    switch (string->type) {
    case StringTypePure:
        if (string->can_be_freed && string->length)
            free(string->source);
        break;
    case StringTypeSub:
        value_deref((RaelValue*)string->reference_string);
        break;
    default:
        RAEL_UNREACHABLE();
    }
}

void string_repr(RaelStringValue *string) {
    putchar('"');
    for (size_t i = 0; i < string->length; ++i) {
        switch (string->source[i]) {
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
            putchar(string->source[i]);
        }
    }
    putchar('"');
}

/* number + string */
RaelValue *string_precede_with_number(RaelNumberValue *number, RaelStringValue *string) {
    RaelInt n;
    char *source;
    size_t str_length;

    if (!number_is_whole(number)) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected the number to be a whole number");
    }
    n = number_to_int(number);
    if (!rael_int_in_range_of_char(n)) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected the number to be in ascii");
    }

    // get length of rhs
    str_length = string_length(string);
    source = malloc((str_length + 1) * sizeof(char));

    source[0] = (char)n;
    strncpy(source + 1, string->source, str_length);
    return (RaelValue*)string_new_pure(source, str_length + 1, true);
}

/* string + number */
RaelValue *string_add_number(RaelStringValue *string, RaelNumberValue *number) {
    RaelInt n;
    char *source;
    size_t str_length;

    if (!number_is_whole(number)) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected the number to be a whole number");
    }

    n = number_to_int(number);
    if (!rael_int_in_range_of_char(n)) {
        return RAEL_BLAME_FROM_RAWSTR_NO_STATE("Expected the number to be in ascii");
    }

    // get length of rhs
    str_length = string_length(string);
    source = malloc((str_length + 1) * sizeof(char));

    strncpy(source, string->source, str_length);
    source[str_length] = (char)n;
    return (RaelValue*)string_new_pure(source, str_length + 1, true);
}

/* is a string equal to another string? */
bool string_eq(RaelStringValue *string, RaelStringValue *string2) {
    size_t str_length, string2_length;

    // get lengths
    str_length = string_length(string);
    string2_length = string_length(string2);
    // if the strings have to have the same length
    if (str_length == string2_length) {
        // if the base value is the same (e.g they inherit from the same constant value or are equal substrings)
        if (string->source == string2->source) {
            return true;
        } else {
            return strncmp(string->source, string2->source, str_length) == 0;
        }
    } else {
        // if the strings differ in length, they are not equal
        return false;
    }
}

bool string_as_bool(RaelStringValue *string) {
    return string_length(string) > 0;
}
