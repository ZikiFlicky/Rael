#include "rael.h"

static inline bool string_validate(RaelValue *value) {
    return value->type == &RaelStringType;
}

RaelValue *string_new_pure(char *source, size_t length, bool can_free) {
    RaelStringValue *string = RAEL_VALUE_NEW(RaelStringType, RaelStringValue);
    string->type = StringTypePure;
    string->can_be_freed = can_free;
    string->source = source;
    string->length = length;
    return (RaelValue*)string;
}

RaelValue *string_new_substr(char *source, size_t length, RaelStringValue *reference_string) {
    RaelStringValue *string = RAEL_VALUE_NEW(RaelStringType, RaelStringValue);
    string->type = StringTypeSub;
    string->source = source;
    string->length = length;
    string->reference_string = reference_string;
    return (RaelValue*)string;
}

/* create a new value from a string pointer and its length */
RaelValue *string_new_pure_cpy(char *source, size_t length) {
    char *allocated = malloc(length * sizeof(char));
    memcpy(allocated, source, length * sizeof(char));
    return string_new_pure(allocated, length, true);
}

/* get length of a string */
size_t string_length(RaelStringValue *self) {
    return self->length;
}

/* get the char at idx */
char string_get_char(RaelStringValue *self, size_t idx) {
    size_t str_len;
    str_len = string_length(self);
    assert(idx < str_len);
    return self->source[idx];
}

/* get a string at idx */
RaelValue *string_get(RaelStringValue *self, size_t idx) {
    RaelValue *new_string;
    size_t str_len;

    // get string length
    str_len = string_length(self);
    if (idx > str_len)
        return NULL;

    if (idx == str_len) { // if you indexed the last character, return an empty string
        new_string = string_new_pure(NULL, 0, true);
    } else {
        // allocate a 1-lengthed string and put the character into it
        char *ptr = malloc(1 * sizeof(char));
        ptr[0] = self->source[idx];
        new_string = string_new_pure(ptr, 1, true);
    }
    
    return new_string;
}

RaelValue *string_slice(RaelStringValue *self, size_t start, size_t end) {
    char *source;
    size_t length;
    RaelStringValue *reference_string;
    RaelValue *new_string;

    assert(end >= start);
    assert(start <= string_length(self) && end <= string_length(self));

    source = self->source + start;
    length = end - start;

    switch (self->type) {
    case StringTypePure: reference_string = self; break;
    case StringTypeSub: reference_string = self->reference_string; break;
    default: RAEL_UNREACHABLE();
    }

    // reference the reference string, because it is now used by our string
    value_ref((RaelValue*)reference_string);

    // create a new substring
    new_string = string_new_substr(source, length, reference_string);

    return new_string;
}

static RaelValue *string_add_string(RaelStringValue *string, RaelStringValue *string2) {
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

    // return the new string object
    return string_new_pure(source, length, true);
}

/* string + number */
static RaelValue *string_add_number(RaelStringValue *string, RaelNumberValue *number) {
    RaelInt n;
    char *source;
    size_t str_len;

    if (!number_is_whole(number)) {
        return BLAME_NEW_CSTR("Expected the number to be a whole number");
    }

    n = number_to_int(number);
    if (!rael_int_in_range_of_char(n)) {
        return BLAME_NEW_CSTR("Expected the number to be in ascii");
    }

    // get length of rhs
    str_len = string_length(string);
    // allocate the string
    source = malloc((str_len + 1) * sizeof(char));

    // copy the string
    strncpy(source, string->source, str_len);
    // add the ascii character
    source[str_len] = (char)n;

    return string_new_pure(source, str_len + 1, true);
}

RaelValue *string_add(RaelStringValue *self, RaelValue *value) {
    if (value->type == &RaelStringType) {
        return string_add_string(self, (RaelStringValue*)value);
    } else if (value->type == &RaelNumberType) {
        return string_add_number(self, (RaelNumberValue*)value);
    } else {
        return NULL;
    }
}

void string_delete(RaelStringValue *self) {
    switch (self->type) {
    case StringTypePure:
        if (self->can_be_freed && self->length)
            free(self->source);
        break;
    case StringTypeSub:
        value_deref((RaelValue*)self->reference_string);
        break;
    default:
        RAEL_UNREACHABLE();
    }
}

void string_repr(RaelStringValue *self) {
    putchar('"');
    for (size_t i = 0; i < self->length; ++i) {
        // escape string if needed
        switch (self->source[i]) {
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
            putchar(self->source[i]);
        }
    }
    putchar('"');
}

void string_complex_repr(RaelStringValue *self) {
    size_t str_len = string_length(self);
    for (size_t i = 0; i < str_len; ++i)
        putchar(string_get_char(self, i));
}

/* equal to another string? */
bool string_eq(RaelStringValue *self, RaelStringValue *value) {
    size_t str1_len, str2_len;

    // get lengths
    str1_len = string_length(self);
    str2_len = string_length(value);

    // if the strings have to have the same length
    if (str1_len == str2_len) {
        // if the base value is the same (e.g they inherit from the same constant value or are equal substrings)
        if (self->source == value->source) {
            return true;
        } else {
            return strncmp(self->source, value->source, str1_len) == 0;
        }
    } else {
        // if the strings differ in length, they are not equal
        return false;
    }
}

RaelValue *string_cast(RaelStringValue *self, RaelTypeValue *type) {
    if (type == &RaelNumberType) {
        struct RaelHybridNumber hybrid;
        
        if (!number_from_string(self->source, self->length, &hybrid)) {
            return BLAME_NEW_CSTR("String could not be parsed as an int");
        }

        if (hybrid.is_float) {
            return number_newf(hybrid.as_float);
        } else {
            return number_newi(hybrid.as_int);
        }
    } else if (type == &RaelStackType) {
        size_t str_len = string_length(self);
        RaelStackValue *stack = (RaelStackValue*)stack_new(str_len);

        for (size_t i = 0; i < str_len; ++i) {
            // create a new RaelNumberValue from the char
            RaelValue *char_value = number_newi((RaelInt)string_get_char(self, i));
            stack_push(stack, char_value);
        }

        return (RaelValue*)stack;
    } else {
        return NULL;
    }
}

bool string_as_bool(RaelStringValue *self) {
    return string_length(self) > 0;
}

RaelTypeValue RaelStringType = {
    RAEL_TYPE_DEF_INIT,
    .name = "String",
    .op_add = (RaelBinExprFunc)string_add,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = NULL,
    .op_eq = (RaelBinCmpFunc)string_eq,
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    .op_call = NULL,
    .op_construct = NULL,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = (RaelAsBoolFunc)string_as_bool,
    .deallocator = (RaelSingleFunc)string_delete,
    .repr = (RaelSingleFunc)string_repr,
    .logger = (RaelSingleFunc)string_complex_repr, /* fallbacks */

    .cast = (RaelCastFunc)string_cast,

    .at_index = (RaelGetFunc)string_get,
    .at_range = (RaelSliceFunc)string_slice,

    .length = (RaelLengthFunc)string_length,

    .methods = NULL
};
