#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void rael_interpret(struct Instruction **instructions, char *stream_base, const bool stream_on_heap,
                    char* const exec_path, char *filename, char **argv, size_t argc, const bool warn_undefined);

static void print_help(void) {
    puts("Welcome to the Rael programming language!");
    puts("usage: rael [--help | -h] | [[--string | -s] string | file] [--warn-undefined]");
    puts("  --string or -s:   interprets a string of code");
    puts("  --help or -h:     shows this help message");
    puts("  --warn-undefined: shows warning when getting an undefined variable");
}

static char *load_file(const char* const filename) {
    FILE *file;
    char *allocated;
    size_t size;
    if (!(file = fopen(filename, "r")))
        return NULL;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (!(allocated = malloc((size+1) * sizeof(char))))
        return NULL;
    fread(allocated, size, 1, file);
    allocated[size] = '\0';
    fclose(file);
    return allocated;
}

int main(int argc, char **argv) {
    char *stream = NULL;
    char *filename;
    char **program_argv;
    size_t program_argc;
    bool warn_undefined = false, on_heap = false;
    struct Instruction **parsed;

    if (argc <= 1) {
        print_help();
        return 1;
    }

    for (int i = 1; stream == NULL && i < argc; ++i) {
        char *arg = argv[i];
        if (strcmp(arg, "--warn-undefined") == 0) {
            warn_undefined = true;
        } else if (strcmp(arg, "--string") == 0 || strcmp(arg, "-s") == 0) {
            if (++i == argc) {
                fprintf(stderr, "Expected an input string after '%s' flag\n", arg);
                return 1;
            }
            // set the stream
            stream = argv[i];
            on_heap = false;
            // set metadata
            filename = NULL;
            // skip filename when setting argv
            ++i;
            program_argv = argv + i;
            program_argc = (size_t)(argc - i);
        } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            // after you've defined a code stream, you can't run with option --help
            print_help();
            return 0;
        } else {
            // try to set the stream
            if (!(stream = load_file(arg))) {
                perror(arg);
                return 1;
            }
            on_heap = true;
            // set metadata
            filename = arg;
            // skip filename when setting argv
            ++i;
            program_argv = argv + i;
            program_argc = (size_t)(argc - i);
        }
    }

    if (!stream) {
        fprintf(stderr, "No code stream defined, run with '--help' to learn more\n");
        return 1;
    }

    parsed = rael_parse(filename, stream, on_heap);
    rael_interpret(parsed, stream, on_heap, argv[0], filename, program_argv, program_argc, warn_undefined);

    return 0;
}
