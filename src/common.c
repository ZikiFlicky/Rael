#include "common.h"

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

void rael_show_error_message(struct State state, const char* const error_message) {
    // advance all whitespace
    while (state.stream_pos[0] == ' ' || state.stream_pos[0] == '\t') {
        ++state.column;
        ++state.stream_pos;
    }
    printf("Error [%zu:%zu]: %s\n", state.line, state.column, error_message);
    rael_show_line_state(state);
}

void rael_error(struct State state, const char* const error_message) {
    rael_show_error_message(state, error_message);
    exit(1);
}
