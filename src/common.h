#ifndef RAEL_COMMON_H
#define RAEL_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#define RAEL_CONSTANT_PI 3.141592653589793
#define RAEL_CONSTANT_2PI 6.283185307179586
#define RAEL_CONSTANT_E 2.718281828459045

#define RAEL_UNREACHABLE()                                                               \
    do {                                                                                 \
        fprintf(stderr, "Unreachable code block reached (%s:%d)\n", __FILE__, __LINE__); \
        abort();                                                                         \
    } while(0)

struct State {
    char *stream_pos;
    size_t line, column;
};

void rael_show_error_tag(char* const filename, struct State state);

void rael_show_line_state(struct State state);

void rael_show_error_message(char* const filename, struct State state, const char* const error_message, va_list va);

char *rael_allocate_cstr(char *string, size_t length);

bool rael_int_in_range_of_char(int number);

/*
    takes a raw string (not from a variable) and allocates it on the stack with a NUL.
    E.g:
    Valid:
    char *heaped = RAEL_HEAPSTR("String");
    Invalid:
    char *str = "String";
    char *heaped = RAEL_HEAPSTR(str);

    To allocate an already "variable'd" string, you must know its size
    and call rael_allocate_cstr.
*/
#define RAEL_HEAPSTR(str) (rael_allocate_cstr(str, sizeof(str)/sizeof(char)-1))

#endif // RAEL_COMMON_H
