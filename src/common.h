#ifndef RAEL_COMMON_H
#define RAEL_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

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

#endif // RAEL_COMMON_H
