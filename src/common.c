#include "rael.h"

void instance_delete(RaelInstance *instance) {
    if (!instance->inherit_scope) {
        struct Scope *parent;
        for (struct Scope *scope = instance->scope; scope; scope = parent) {
            parent = scope->parent;
            scope_delete(scope);
        }
    }
    if (instance->instructions)
        block_delete(instance->instructions);
    stream_deref(instance->stream);
    free(instance);
}

RaelInt rael_int_abs(RaelInt i) {
    if (i < 0) {
        return -i;
    } else {
        return i;
    }
}

RaelFloat rael_float_abs(RaelFloat f) {
    if (f < 0.0) {
        return -f;
    } else {
        return f;
    }
}

void rael_show_error_tag(char* const filename, struct State state) {
    if (filename)
        printf("Error [%s:%zu:%zu]: ", filename, state.line, state.column);
    else
        printf("Error [%zu:%zu]: ", state.line, state.column);
}

void rael_show_line_state(struct State state) {
    size_t line_length = state.column - 1;
    char *line_start = state.stream_pos.cur - line_length;

    // show the line
    printf("| ");
    for (size_t i = 0; line_start[i] && line_start[i] != '\n'; ++i) {
        if (line_start[i] == '\t') {
            printf("    ");
        } else {
            putchar(line_start[i]);
        }
    }
    // show the pointer/cursor
    printf("\n| ");
    for (size_t i = 0; i < line_length; ++i) {
        if (line_start[i] == '\t') {
            printf("    ");
        } else {
            putchar(' ');
        }
    }
    printf("^\n");
}

void rael_show_error_message(char* const filename, struct State state, const char* const error_message, va_list va) {
    // advance all whitespace
    while (state.stream_pos.cur[0] == ' ' || state.stream_pos.cur[0] == '\t') {
        ++state.column;
        ++state.stream_pos.cur;
    }
    rael_show_error_tag(filename, state);
    vprintf(error_message, va);
    printf("\n");
    rael_show_line_state(state);
}

void rael_show_warning_key(char *key) {
    fprintf(stderr, "WARNING: getting undefined key ':%s'\n", key);
}

char *rael_allocate_cstr(char *string, size_t length) {
    char *new_string = malloc((length + 1) * sizeof(char));
    strncpy(new_string, string, length);
    new_string[length] = '\0';
    return new_string;
}

bool rael_int_in_range_of_char(RaelInt number) {
    if (number >= CHAR_MIN && number <= CHAR_MAX) {
        return true;
    } else {
        return false;
    }
}

void arguments_new(RaelArgumentList *out, size_t overhead) {
    if (overhead == 0) {
        out->arguments = NULL;
    } else {
        out->arguments = malloc(overhead * sizeof(RaelArgument));
    }
    out->amount_allocated = overhead;
    out->amount_arguments = 0;
}

void arguments_add(RaelArgumentList *args, RaelValue *value, struct State state) {
    RaelArgument arg;

    if (args->amount_allocated == 0)
        args->arguments = malloc((args->amount_allocated = 4) * sizeof(RaelArgument));
    else if (args->amount_arguments >= args->amount_allocated)
        args->arguments = realloc(args->arguments, (args->amount_allocated += 4) * sizeof(RaelArgument));

    // push argument to argument list
    value_ref(value);
    arg.value = value;
    arg.state = state;
    args->arguments[args->amount_arguments++] = arg;
}

RaelValue *arguments_get(RaelArgumentList *args, size_t idx) {
    if (idx >= args->amount_arguments)
        return NULL;
    return args->arguments[idx].value;
}

struct State *arguments_state(RaelArgumentList *args, size_t idx) {
    if (idx >= args->amount_arguments)
        return NULL;
    return &args->arguments[idx].state;
}

size_t arguments_amount(RaelArgumentList *args) {
    return args->amount_arguments;
}

void arguments_delete(RaelArgumentList *args) {
    for (size_t i = 0; i < args->amount_arguments; ++i) {
        value_deref(arguments_get(args, i));
    }
    free(args->arguments);
}

/* this function shrinks the size of the argument buffer */
void arguments_finalize(RaelArgumentList *args) {
    if (args->amount_allocated > 0) {
        args->arguments = realloc(args->arguments,
                          (args->amount_allocated = args->amount_arguments) * sizeof(RaelArgument));
    }
}
