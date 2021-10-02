#ifndef RAEL_COMMON_H
#define RAEL_COMMON_H

#include <stdio.h>
#include <stdlib.h>

#define RAEL_UNREACHABLE()                                                             \
    do {                                                                               \
        fprintf(stderr, "unreachable code block reached (%s:%d)", __FILE__, __LINE__); \
        abort();                                                                       \
    } while(0)

struct State {
    char *stream_pos;
    size_t line, column;
};

void rael_show_line_state(struct State state);

void rael_show_error_message(struct State state, const char* const error_message);

void rael_error(struct State state, const char* const error_message);

#endif // RAEL_COMMON_H
