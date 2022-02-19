#include "rael.h"

static void print_help(void) {
    puts("Welcome to the Rael programming language!");
    puts("usage: rael [--help | -h] | [[--string | -s] string | file] [--warn-undefined]");
    puts("  --string or -s:   interprets a string of code");
    puts("  --help or -h:     shows this help message");
    puts("  --warn-undefined: shows warning when getting an undefined variable");
}

int main(int argc, char **argv) {
    RaelStream stream;
    char **program_argv;
    size_t program_argc;
    bool warn_undefined = false, stream_defined = false;
    struct Instruction **parsed;

    if (argc <= 1) {
        print_help();
        return 1;
    }

    for (int i = 1; !stream_defined && i < argc; ++i) {
        char *arg = argv[i];
        if (strcmp(arg, "--warn-undefined") == 0) {
            warn_undefined = true;
        } else if (strcmp(arg, "--string") == 0 || strcmp(arg, "-s") == 0) {
            if (++i == argc) {
                fprintf(stderr, "Expected an input string after '%s' flag\n", arg);
                return 1;
            }
            stream.base = argv[i];
            stream.cur = argv[i];
            stream.length = strlen(argv[i]);
            stream.name = NULL;
            stream.on_heap = false;
            // skip string when setting argv
            ++i;
            program_argv = &argv[i];
            program_argc = (size_t)(argc - i);
            stream_defined = true;
        } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            // after you've defined a code stream, you can't run with option --help
            print_help();
            return 0;
        } else {
            // try to set the stream
            if (!load_file(arg, &stream)) {
                perror(arg);
                return 1;
            }
            // skip filename when setting argv
            ++i;
            program_argv = argv + i;
            program_argc = (size_t)(argc - i);
            stream_defined = true;
        }
    }

    if (!stream_defined) {
        fprintf(stderr, "No code stream defined, run with '--help' to learn more\n");
        return 1;
    }

    parsed = rael_parse(stream);
    rael_interpret(parsed, stream, argv[0], program_argv, program_argc, warn_undefined);

    return 0;
}
