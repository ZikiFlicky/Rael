#include "common.h"

#include <stdarg.h>

void rael_show_error_tag(char* const filename, struct State state) {
    printf("Error [%s:%zu:%zu]: ", filename, state.line, state.column);
}

void rael_show_line_state(struct State state) {
    printf("| ");
    for (int i = -state.column + 1; state.stream_pos[i] && state.stream_pos[i] != '\n'; ++i) {
        if (state.stream_pos[i] == '\t') {
            printf("    ");
        } else {
            putchar(state.stream_pos[i]);
        }
    }
    printf("\n| ");
    for (size_t i = 0; i < state.column - 1; ++i) {
        if (state.stream_pos[-state.column + i] == '\t') {
            printf("    ");
        } else {
            putchar(' ');
        }
    }
    printf("^\n");
}

void rael_show_error_message(char* const filename, struct State state, const char* const error_message, va_list va) {
    // advance all whitespace
    while (state.stream_pos[0] == ' ' || state.stream_pos[0] == '\t') {
        ++state.column;
        ++state.stream_pos;
    }
    rael_show_error_tag(filename, state);
    vprintf(error_message, va);
    printf("\n");
    rael_show_line_state(state);
}
