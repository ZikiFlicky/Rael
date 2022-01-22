#include "rael.h"

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
    value_ref((RaelValue*)reference_string);
    string->reference_string = reference_string;
    return (RaelValue*)string;
}

/* create a new value from a string pointer and its length */
RaelValue *string_new_pure_cpy(char *source, size_t length) {
    char *allocated;

    // don't even try to malloc(0), just get set a NULL
    if (length == 0) {
        allocated = NULL;
    } else {
        allocated = malloc(length * sizeof(char));
        memcpy(allocated, source, length * sizeof(char));
    }
    return string_new_pure(allocated, length, true);
}


static RaelValue *string_new_empty(void) {
    return string_new_pure(NULL, 0, true);
}

/* get length of a string */
size_t string_length(RaelStringValue *self) {
    return self->length;
}

/* get the char at idx */
char string_get_char(RaelStringValue *self, size_t idx) {
    size_t str_len = string_length(self);
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
            value_deref(char_value);
        }

        return (RaelValue*)stack;
    } else {
        return NULL;
    }
}

bool string_as_bool(RaelStringValue *self) {
    return string_length(self) > 0;
}

/*
 * Given a string self and a search string, the function returns the index of the first occurance of the
 * search string in the self.
 * The third parameter, start_index, mentions where to start searching, but in most cases
 * is just 0.
 * The result is a RaelInt and if the function doesn't find an occurance of search_string in it,
 * it returns -1.
 */
static RaelInt string_find(RaelStringValue *self, RaelStringValue *search_string, size_t start_index) {
    size_t length = string_length(self);
    size_t search_length = string_length(search_string);
    RaelInt index = -1; // not found yet

    // you can only search the string when the search string is shorter or the same length as the searched string
    if (search_length <= length) {
        bool found = false;
        for (size_t i = start_index; !found && i < length - search_length + 1; ++i) {
            bool matching = true;
            for (size_t j = 0; matching && j < search_length; ++j) {
                if (self->source[i + j] != search_string->source[j]) {
                    matching = false;
                }
            }
            if (matching) {
                index = i;
                found = true;
            }
        }
    }
    return index;
}

/*
 * Proabably temporary function until I have a stringbuilder.
 * Copies a string to the end of self.
 */
static void string_extend(RaelStringValue *self, char *source, size_t length) {
    size_t self_length = string_length(self);

    assert(self->type != StringTypeSub);
    assert(self->can_be_freed);
    // allocate more/new space
    if (self_length == 0) {
        if (length > 0) {
            self->source = malloc(length * sizeof(char));
        }
    } else {
        self->source = realloc(self->source, (self_length + length) * sizeof(char));
    }
    self->length += length;
    // copy into self
    if (length > 0)
        strncpy(&self->source[self_length], source, length);
}

/*
 * Returns a new lowercase string created from self.
 * "Rael":toLower() = "rael"
 */
RaelValue *string_method_toLower(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    size_t length;
    char *source;

    (void)interpreter;

    if (arguments_amount(args) > 0) {
        return BLAME_NEW_CSTR_ST("Too many arguments", *arguments_state(args, 0));
    }

    length = string_length(self);
    source = malloc(length * sizeof(char));

    for (size_t i = 0; i < length; ++i) {
        source[i] = tolower(self->source[i]);
    }
    return string_new_pure(source, length, true);
}

/*
 * Returns a new uppercase string created from self.
 * "Rael":toLower() = "RAEL"
 */
RaelValue *string_method_toUpper(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    size_t length;
    char *source;

    (void)interpreter;

    if (arguments_amount(args) > 0) {
        return BLAME_NEW_CSTR("Too many arguments");
    }

    length = string_length(self);
    source = malloc(length * sizeof(char));

    for (size_t i = 0; i < length; ++i) {
        source[i] = toupper(self->source[i]);
    }
    return string_new_pure(source, length, true);
}

/*
 * Returns a stack with numbers representing the chars in the string.
 * "Rael":toCharStack() = { 82, 97, 101, 108 }
 */
RaelValue *string_method_toCharStack(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    size_t length;
    RaelStackValue *stack;

    (void)interpreter;

    if (arguments_amount(args) > 0) {
        return BLAME_NEW_CSTR("Too many arguments");
    }

    length = string_length(self);
    stack = (RaelStackValue*)stack_new(length);

    for (size_t i = 0; i < length; ++i) {
        RaelValue *char_value = number_newi((RaelInt)self->source[i]);
        stack_push(stack, char_value);
        value_deref(char_value);
    }
    return (RaelValue*)stack;
}

/*
 * Returns a number with the char value of the string at the index given as parameter
 * for example:
 * log "Abcdefg":charAt(0) = 65
 */
RaelValue *string_method_charAt(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    size_t length, idx;
    RaelValue *arg1;
    char c;

    (void)interpreter;

    if (arguments_amount(args) != 1) {
        return BLAME_NEW_CSTR("Expected one argument");
    }

    arg1 = arguments_get(args, 0);

    if (arg1->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    if (!number_is_whole((RaelNumberValue*)arg1)) {
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 0));
    }
    if (!number_positive((RaelNumberValue*)arg1)) {
        return BLAME_NEW_CSTR_ST("Expected a positive number", *arguments_state(args, 0));
    }

    // get length of string
    length = string_length(self);
    // get index
    idx = (size_t)number_to_int((RaelNumberValue*)arg1);

    // if index is bigger than the length of the string, return an error
    if (idx >= length) {
        return BLAME_NEW_CSTR_ST("Index out of range", *arguments_state(args, 0));
    }

    // get character's ascii representation
    c = self->source[idx];

    return number_newi((RaelInt)c);
}

/*
 * Returns 1 if the string is lowercase, and 0 if it's not
 */
RaelValue *string_method_isLower(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    bool has_lower = false, has_non_lower = false;
    size_t length;

    (void)interpreter;

    if (arguments_amount(args) > 0) {
        return BLAME_NEW_CSTR("Too many arguments");
    }

    length = string_length(self);

    for (size_t i = 0; !has_non_lower && i < length; ++i) {
        if (islower(self->source[i])) {
            has_lower = true;
        } else {
            has_non_lower = true;
        }
    }

    return number_newi(has_lower && !has_non_lower);
}

/*
 * Returns 1 if the string is upper, and 0 if it's not
 */
RaelValue *string_method_isUpper(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    bool has_upper = false, has_non_upper = false;
    size_t length;

    (void)interpreter;

    if (arguments_amount(args) > 0) {
        return BLAME_NEW_CSTR("Too many arguments");
    }

    length = string_length(self);

    for (size_t i = 0; !has_non_upper && i < length; ++i) {
        if (isupper(self->source[i])) {
            has_upper = true;
        } else {
            has_non_upper = true;
        }
    }

    return number_newi(has_upper && !has_non_upper);
}

/*
 * Returns 1 if the string is full of digit, and 0 if it's not
 */
RaelValue *string_method_isDigit(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    bool has_digit = false, has_non_digit = false;
    size_t length;

    (void)interpreter;

    if (arguments_amount(args) > 0) {
        return BLAME_NEW_CSTR("Too many arguments");
    }

    length = string_length(self);

    for (size_t i = 0; !has_non_digit && i < length; ++i) {
        if (isdigit(self->source[i])) {
            has_digit = true;
        } else {
            has_non_digit = true;
        }
    }

    return number_newi(has_digit && !has_non_digit);
}

/*
 * Split into substrings of a size given as a parameter.
 * "abcdefgd":chunkSplit(2) = { "ab", "cd", "ef", "gd" }
 */
RaelValue *string_method_chunkSplit(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    int chunk_size;
    size_t length, amount_full_chunks;
    size_t additional_chunk_length;
    RaelStackValue *stack;

    (void)interpreter;

    if (arguments_amount(args) != 1) {
        return BLAME_NEW_CSTR("Expected 1 argument");
    }

    arg1 = arguments_get(args, 0);
    
    if (arg1->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    if (!number_is_whole((RaelNumberValue*)arg1)) {
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 0));
    }
    chunk_size = number_to_int((RaelNumberValue*)arg1);
    if (chunk_size < 1) {
        return BLAME_NEW_CSTR_ST("Expected a number bigger than 0", *arguments_state(args, 0));
    }

    length = string_length(self);
    // calculate the amount of chunks needed
    amount_full_chunks = length / (size_t)chunk_size;
    additional_chunk_length = length % (size_t)chunk_size;

    // create a new stack
    stack = (RaelStackValue*)stack_new(amount_full_chunks + (additional_chunk_length > 0));

    for (size_t i = 0; i < amount_full_chunks; ++i) {
        RaelValue *substr = string_new_substr(&self->source[i * (size_t)chunk_size], (size_t)chunk_size, self);
        stack_push(stack, substr);
        value_deref(substr);
    }

    // if there was an additional chunk add it
    if (additional_chunk_length > 0) {
        RaelValue *substr = string_new_substr(&self->source[amount_full_chunks * (size_t)chunk_size], additional_chunk_length, self);
        stack_push(stack, substr);
        value_deref(substr);
    }
    return (RaelValue*)stack;
}

/*
 * Given a splitter string, self is split into a stack of the substrings between
 * the each splitter occurance to the next.
 * "Apple, Banana, Orange":split(", ") = { "Apple", "Banana", "Orange" }
 */
RaelValue *string_method_split(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelStackValue *stack;
    RaelValue *arg1, *rest;
    RaelStringValue *splitter;
    size_t length, splitter_length;
    size_t last_index = 0;

    (void)interpreter;

    if (arguments_amount(args) != 1) {
        return BLAME_NEW_CSTR("Expected 1 argument");
    }

    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelStringType) {
        return BLAME_NEW_CSTR("Expected a string as argument");
    }

    splitter = (RaelStringValue*)arg1;

    // get length of string and splitter
    length = string_length(self);
    splitter_length = string_length(splitter);

    if (splitter_length == 0) {
        return BLAME_NEW_CSTR_ST("Empty seperator not allowed", *arguments_state(args, 0));
    }

    stack = (RaelStackValue*)stack_new(0);

    // if the length of the splitter is bigger, it wouldn't be able to split the string
    if (splitter_length <= length) {
        while (last_index < length - splitter_length + 1) {
            RaelInt match = string_find(self, splitter, last_index);

            // if the seperator is not found again, break
            if (match == -1) {
                break;
            } else {
                RaelValue *substr;
                size_t substr_length = (size_t)match - last_index;

                // get the substring and push it
                substr = string_new_substr(&self->source[last_index], substr_length, self);
                stack_push(stack, substr);
                value_deref(substr);

                last_index = (size_t)match + splitter_length;
            }
        }
    }

    // push the rest
    rest = string_new_substr(&self->source[last_index], length - last_index, self);
    stack_push(stack, rest);
    value_deref(rest);

    return (RaelValue*)stack;
}

/*
 * Returns the index of the first occurance of the string argument in self.
 * "Banana":findIndexOf("na") = 2
 */
RaelValue *string_method_findIndexOf(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelStringValue *search_string;
    RaelInt index;

    (void)interpreter;

    if (arguments_amount(args) != 1) {
        return BLAME_NEW_CSTR("Expected 1 argument");
    }

    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelStringType) {
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));
    }

    search_string = (RaelStringValue*)arg1;
    index = string_find(self, search_string, 0);

    return number_newi(index);
}

/*
 * Returns 1 if the string argument can be found inside of self and 0 otherise.
 * "abcdef":contains("def") = 1
 * "abcdef":contains("Def") = 0
 * "abcdef":contains("abcdef") = 1
 */
RaelValue *string_method_contains(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelStringValue *search_string;
    bool contains;

    (void)interpreter;

    if (arguments_amount(args) != 1) {
        return BLAME_NEW_CSTR("Expected 1 argument");
    }

    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelStringType) {
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));
    }

    search_string = (RaelStringValue*)arg1;
    if (string_find(self, search_string, 0) != -1) {
        contains = true;
    } else {
        contains = false;
    }
    return number_newi(contains);
}

/*
 * Returns a new string replacing one occurance of a string with the second string.
 * "This string is a string full of characters":replace("string", "str") = "This str is a string full of characters"
 */
RaelValue *string_method_replace(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1, *arg2;
    RaelStringValue *search_string, *replace_string, *new_string;
    size_t length, search_length, replace_length, last_index = 0;
    
    (void)interpreter;

    if (arguments_amount(args) != 2) {
        return BLAME_NEW_CSTR("Expected 2 arguments");
    }

    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelStringType) {
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));
    }
    arg2 = arguments_get(args, 1);
    if (arg2->type != &RaelStringType) {
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 1));
    }

    // get the string to replace and the string to replace it with
    search_string = (RaelStringValue*)arg1;
    replace_string = (RaelStringValue*)arg2;
    // initialize an empty string
    new_string = (RaelStringValue*)string_new_empty();

    // get lengths
    length = string_length(self);
    search_length = string_length(search_string);
    replace_length = string_length(replace_string);

    while (last_index < length - search_length + 1) {
        // try to find the next occurance to replace
        RaelInt index = string_find(self, search_string, last_index);

        // if there is no future occurance
        if (index == -1) {
            break;
        } else {
            // extand string with things up until now
            string_extend(new_string, &self->source[last_index], (size_t)index - last_index);

            // add replacement
            string_extend(new_string, replace_string->source, replace_length);

            // calculate new index
            last_index = (size_t)index + search_length;
        }
    }

    // add the rest
    string_extend(new_string, &self->source[last_index], length - last_index);
    return (RaelValue*)new_string;
}

/*
 * Returns the amount of times a string argument can be found inside of self.
 * "a string is a string":timesContains("string") = 2
 */
RaelValue *string_method_timesContains(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelStringValue *search_string;
    size_t length, search_length, search_index = 0;
    RaelInt times_contained = 0;

    (void)interpreter;

    if (arguments_amount(args) != 1) {
        return BLAME_NEW_CSTR("Expected 1 argument");
    }

    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelStringType) {
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));
    }

    // get the search string
    search_string = (RaelStringValue*)arg1;
    // get lengths
    length = string_length(self);
    search_length = string_length(search_string);

    while (search_index < length - search_length + 1) {
        RaelInt next_occurance = string_find(self, search_string, search_index);

        // no next occurance
        if (next_occurance == -1) {
            break;
        } else {
            ++times_contained;
            search_index = (size_t)next_occurance + (search_length == 0 ? 1 : search_length);
        }
    }
    return number_newi(times_contained);
}

/*
 * Create a new string from a string iterator, seperating it with self.
 * ", ":seperate({ "Banana", "Apple", "Beans" }) = "Banana, Apple, Beans"
 */
RaelValue *string_method_seperate(RaelStringValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelStackValue *strings;
    size_t amount_strings, length;
    RaelStringValue *new_string;

    (void)interpreter;

    if (arguments_amount(args) != 1) {
        return BLAME_NEW_CSTR("Expected 1 argument");
    }

    arg1 = arguments_get(args, 0);

    if (!value_is_iterable(arg1)) {
        return BLAME_NEW_CSTR_ST("Expected an iterable", *arguments_state(args, 0));
    }

    // initialize strings array
    strings = (RaelStackValue*)stack_new(0);

    for (size_t i = 0; i < value_length(arg1); ++i) {
        RaelValue *maybe_string = value_get(arg1, i);
        // verify the number is a string
        if (maybe_string->type != &RaelStringType) {
            value_deref(maybe_string);
            value_deref((RaelValue*)strings);
            return BLAME_NEW_CSTR_ST("Iterable does not produce strings", *arguments_state(args, 0));
        }
        // push the string to the strings array
        stack_push(strings, maybe_string);
        value_deref(maybe_string);
    }

    amount_strings = stack_length(strings);
    // get length of seperator
    length = string_length(self);
    // create the string that will be filled
    new_string = (RaelStringValue*)string_new_empty();

    for (size_t i = 0; i < amount_strings; ++i) {
        RaelStringValue *sep = (RaelStringValue*)stack_get(strings, i);
        size_t sep_length = string_length(sep);

        if (i > 0) {
            string_extend(new_string, self->source, length);
        }
        string_extend(new_string, sep->source, sep_length);
        value_deref((RaelValue*)sep);
    }

    // remove the temp stack and return the new string
    value_deref((RaelValue*)strings);
    return (RaelValue*)new_string;
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

    .methods = (MethodDecl[]) {
        { "toLower", (RaelMethodFunc)string_method_toLower },
        { "toUpper", (RaelMethodFunc)string_method_toUpper },
        { "toCharStack", (RaelMethodFunc)string_method_toCharStack },
        { "charAt", (RaelMethodFunc)string_method_charAt },
        { "isLower", (RaelMethodFunc)string_method_isLower },
        { "isUpper", (RaelMethodFunc)string_method_isUpper },
        { "isDigit", (RaelMethodFunc)string_method_isDigit },
        { "chunkSplit", (RaelMethodFunc)string_method_chunkSplit },
        { "split", (RaelMethodFunc)string_method_split },
        { "findIndexOf", (RaelMethodFunc)string_method_findIndexOf },
        { "contains", (RaelMethodFunc)string_method_contains },
        { "replace", (RaelMethodFunc)string_method_replace },
        { "timesContains", (RaelMethodFunc)string_method_timesContains },
        { "seperate", (RaelMethodFunc)string_method_seperate },
        { NULL, NULL }
    }
};
