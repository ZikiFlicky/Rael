#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void interpret(struct Instruction **instructions, char *base_stream, const bool warn_undefined);

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
    char *stream_base;
    bool do_free = false, undefined_errors = false, defined_stream = false;

    --argc; ++argv;
    if (argc == 0) {
        print_help();
        return 1;
    }

    while (argc) {
        if (strcmp(argv[0], "--warn-undefined") == 0) {
            --argc; ++argv;
            undefined_errors = true;
        } else if (strcmp(argv[0], "--string") == 0 || strcmp(argv[0], "-s") == 0) {
            --argc; ++argv;

            if (argc == 0) {
                fprintf(stderr, "Expected an input string after '%s' flag\n", argv[0]);
                return 1;
            }

            if (defined_stream) {
                fprintf(stderr, "Can't load string because a code stream was already loaded\n");
                return 1;
            }

            if (!(stream_base = argv[0]))
                return 0;

            --argc; ++argv;
            defined_stream = true;
        } else if (strcmp(argv[0], "--help") == 0 || strcmp(argv[0], "-h") == 0) {
            // after you've defined a code stream, you can't run with option --help
            if (!defined_stream) {
                print_help();
                return 0;
            }
        } else {
            if (defined_stream) {
                fprintf(stderr, "Can't load file '%s' because a code stream was already loaded\n", argv[0]);
                return 1;
            }

            if (!(stream_base = load_file(argv[0]))) {
                perror(argv[0]);
                return 1;
            }

            --argc; ++argv;
            do_free = true;
            defined_stream = true;
        }
    }

    if (!defined_stream) {
        fprintf(stderr, "No stream defined, run with '--help' to learn more\n");
        return 1;
    }

    interpret(parse(stream_base), stream_base, undefined_errors);

    if (do_free)
        free(stream_base);

    return 0;
}
