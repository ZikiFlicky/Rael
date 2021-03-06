#ifndef RAEL_COMMON_H
#define RAEL_COMMON_H

#include "stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <float.h>

/* declare constants for Rael */
#define RAEL_CONSTANT_PI 3.141592653589793
#define RAEL_CONSTANT_2PI 6.283185307179586
#define RAEL_CONSTANT_E 2.718281828459045

#ifndef NDEBUG /* if we show assetions */
#define RAEL_UNREACHABLE()                                                               \
    do {                                                                                 \
        fprintf(stderr, "Unreachable code block reached (%s:%d)\n", __FILE__, __LINE__); \
        abort();                                                                         \
    } while(0)
#else /* if we ignore assertions */
#define RAEL_UNREACHABLE() do {} while(0)
#endif

typedef long RaelInt;
typedef double RaelFloat;
typedef struct RaelValue RaelValue;

#define RAELINT_MAX LONG_MAX
#define RAELFLOAT_MAX DBL_MAX

struct RaelHybridNumber {
    bool is_float;
    union {
        RaelInt as_int;
        RaelFloat as_float;
    };
};

struct State {
    RaelStreamPtr stream_pos;
    size_t line, column;
};

typedef struct RaelArgument {
    struct State state;
    RaelValue *value;
} RaelArgument;

typedef struct RaelArgumentList {
    size_t amount_arguments, amount_allocated;
    RaelArgument *arguments;
} RaelArgumentList;

char *rael_cstr_duplicate(char *cstr);

RaelInt rael_int_abs(RaelInt i);

RaelFloat rael_float_abs(RaelFloat f);

void rael_show_error_tag(char* const filename, struct State state);

void rael_show_line_state(struct State state);

void rael_show_error_message(char* const filename, struct State state, const char* const error_message, va_list va);

void rael_show_warning_key(char *key);

char *rael_allocate_cstr(char *string, size_t length);

bool rael_int_in_range_of_char(RaelInt number);

/*
    takes a raw string (not from a variable) and allocates it on the stack with a NUL.
    E.g:
    Valid:
    char *heaped = RAEL_HEAPSTR("String");
    Invalid:
    char *str = "String";
    char *heaped = RAEL_HEAPSTR(str);

    To allocate a non-raw string, you must know its length
    and call rael_allocate_cstr with that length.
*/
#define RAEL_HEAPSTR(str) (rael_allocate_cstr(str, sizeof(str)/sizeof(char)-1))

/* create a RaelArguments */
void arguments_new(RaelArgumentList *out, size_t overhead);

/* add an argument */
void arguments_add(RaelArgumentList *args, RaelValue *value, struct State state);

/* returns the argument at the requested index, or NULL if the index is invalid */
RaelValue *arguments_get(RaelArgumentList *args, size_t idx);

/* returns the state of the argument at the requested index, or NULL if the index is invalid */
struct State *arguments_state(RaelArgumentList *args, size_t idx);

/* this function returns the amount of arguments */
size_t arguments_amount(RaelArgumentList *args);

/* this function deallocates the arguments */
void arguments_delete(RaelArgumentList *args);

/* this function shrinks the size of the argument buffer */
void arguments_finalize(RaelArgumentList *args);

#endif // RAEL_COMMON_H
