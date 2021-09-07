#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void interpret(struct Node **instructions);

static void print_help(void) {
    printf("./ech --file [file]\n");
    printf("./ech --string \"string of code\"\n");
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
    bool do_free = false;
    --argc; ++argv;
    if (argc == 0) {
        print_help();
        return 1;
    }
    if (strcmp(argv[0], "--string") == 0 || strcmp(argv[0], "-s") == 0) {
        if (argc == 1)
            goto not_enough_arguments;
        if (!(stream_base = argv[1]))
            return 0;
    } else if (strcmp(argv[0], "--file") == 0 || strcmp(argv[0], "-f") == 0) {
        if (argc == 1)
            goto not_enough_arguments;
        if (!(stream_base = load_file(argv[1]))) {
            perror(argv[1]);
            return 1;
        }
        do_free = true;
    } else {
        print_help();
        return 1;
    }
    interpret(parse(stream_base));
    if (do_free)
        free(stream_base);
    return 0;
not_enough_arguments:
    fprintf(stderr, "Expected an input string after '%s' flag\n", argv[0]);
    return 1;
}
