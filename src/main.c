#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void rael_interpret(struct Instruction **instructions, char *stream_base, const bool stream_on_heap, const bool warn_undefined);

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
    bool warn_undefined = false, on_heap = false;

    if (argc <= 1) {
        print_help();
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--warn-undefined") == 0) {
            warn_undefined = true;
        } else if (strcmp(argv[i], "--string") == 0 || strcmp(argv[i], "-s") == 0) {
            if (++i == argc) {
                fprintf(stderr, "Expected an input string after '%s' flag\n", argv[i]);
                return 1;
            }
            if (stream) {
                fprintf(stderr, "Can't load string because a code stream was already loaded\n");
                return 1;
            }
            if (!(stream = argv[i]))
                return 0;
            on_heap = false;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            // after you've defined a code stream, you can't run with option --help
            if (!stream) {
                print_help();
                return 0;
            }
        } else {
            if (stream) {
                fprintf(stderr, "Can't load file '%s' because a code stream was already loaded\n", argv[i]);
                return 1;
            }
            if (!(stream = load_file(argv[i]))) {
                perror(argv[i]);
                return 1;
            }
            on_heap = true;
        }
    }

    if (!stream) {
        fprintf(stderr, "No stream defined, run with '--help' to learn more\n");
        return 1;
    }

    rael_interpret(rael_parse(stream, on_heap), stream, on_heap, warn_undefined);

    return 0;
}
