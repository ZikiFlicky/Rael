#include "rael.h"

/*
 * This is the source code for Rael's :System module,
 * which gives access to system related functions.
 */

RaelValue *expr_eval(RaelInterpreter* const interpreter, struct Expr* const expr, const bool can_explode);
void block_run(RaelInterpreter* const interpreter, struct Instruction **block, bool create_new_scope);
void block_delete(struct Instruction **block);

RaelValue *module_system_RunShellCommand(RaelArgumentList *args, RaelInterpreter *interpreter) {
    char *command_cstr;
    RaelValue *arg1;
    int exit_code;

    (void)interpreter;
    assert(arguments_amount(args) == 1);

    arg1 = arguments_get(args, 0);

    if (arg1->type != &RaelStringType) {
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));
    }

    // get the string as a heap-allocated cstr
    command_cstr = string_to_cstr((RaelStringValue*)arg1);

    // run the command
    exit_code = system(command_cstr);
    // free heap-allocated string
    free(command_cstr);

    if (exit_code == -1)
        return BLAME_NEW_CSTR("Unable to create child process");

    // shift to get only the exit code
    return number_newi((RaelInt)(exit_code >> 8));
}

RaelValue *module_system_GetShellOutput(RaelArgumentList *args, RaelInterpreter *interpreter) {
    char *command_cstr;
    RaelValue *arg1;
    FILE *shell_fd;
    size_t length = 0, allocated = 0;
    char *output = NULL;
    size_t amount_read;

    (void)interpreter;
    assert(arguments_amount(args) == 1);

    arg1 = arguments_get(args, 0);

    if (arg1->type != &RaelStringType) {
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));
    }

    // get the string as a heap-allocated cstr
    command_cstr = string_to_cstr((RaelStringValue*)arg1);

    // run the command
    shell_fd = popen(command_cstr, "r");
    // free heap-allocated command cstring
    free(command_cstr);

    do {
        if (allocated == 0) {
            output = malloc((allocated = 128) * sizeof(char));
        } else if (length + 64 >= allocated) {
            output = realloc(output, (allocated += 64) * sizeof(char));
        }
        amount_read = fread(&output[length], sizeof(char), 64, shell_fd);
        length += amount_read;
    } while (amount_read == 64);

    pclose(shell_fd);

    return string_new_pure(output, length, true);
}

RaelValue *module_system_Exit(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelInt exit_code;

    switch (arguments_amount(args)) {
    case 0:
        exit_code = 1;
        break;
    case 1: {
        RaelValue *arg = arguments_get(args, 0);
        RaelNumberValue *number;

        if (arg->type != &RaelNumberType)
            return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
        number = (RaelNumberValue*)arg;
        if (!number_is_whole(number))
            return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 0));

        exit_code = number_to_int(number);
        break;
    }
    default:
        RAEL_UNREACHABLE();
    }

    interpreter_destroy_all(interpreter);
    exit(exit_code);
    return void_new();
}

RaelValue *module_system_Run(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg;
    RaelStringValue *string;
    RaelStream *stream;
    char *code;
    struct Instruction **instructions;
    bool new_scope;

    assert(arguments_amount(args) >= 1 && arguments_amount(args) <= 2);

    // get first argument
    arg = arguments_get(args, 0);
    if (arg->type != &RaelStringType)
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));
    string = (RaelStringValue*)arg;
    code = string_to_cstr(string);


    // get the rest of the arguments
    switch (arguments_amount(args)) {
    case 1:
        new_scope = false;
        break;
    case 2:
        arg = arguments_get(args, 1);
        new_scope = value_truthy(arg);
        break;
    default:
        RAEL_UNREACHABLE();
    }

    stream = stream_new(code, string_length(string), true, NULL);

    // parse the string
    instructions = rael_parse(stream);
    // create a new instance that inherits our current scope
    interpreter_new_instance(interpreter, stream, instructions, !new_scope);
    // run
    interpreter_interpret(interpreter);
    // remove the new instance and return to the previous one
    interpreter_delete_instance(interpreter);

    return void_new();
}

RaelValue *module_system_Eval(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg;
    RaelStringValue *string;
    RaelStream *stream;
    char *code;
    struct Expr *expr;
    RaelValue *evaluated;
    bool new_scope;

    assert(arguments_amount(args) >= 1 && arguments_amount(args) <= 2);

    // get first argument
    arg = arguments_get(args, 0);
    if (arg->type != &RaelStringType)
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));
    string = (RaelStringValue*)arg;
    code = string_to_cstr(string);

    // get the rest of the arguments
    switch (arguments_amount(args)) {
    case 1:
        new_scope = false;
        break;
    case 2:
        arg = arguments_get(args, 1);
        new_scope = value_truthy(arg);
        break;
    default:
        RAEL_UNREACHABLE();
    }

    stream = stream_new(code, string_length(string), true, NULL);

    interpreter_new_instance(interpreter, stream, NULL, !new_scope);
    if (!(expr = rael_parse_expr(stream)))
        return BLAME_NEW_CSTR_ST("Cannot parse string", *arguments_state(args, 0));

    // evaluate the expression
    evaluated = expr_eval(interpreter, expr, true);
    // remove the expression
    expr_delete(expr);
    // remove the last instance
    interpreter_delete_instance(interpreter);

    return evaluated;
}

static RaelValue *program_argv_stack_new(RaelInterpreter *interpreter) {
    RaelStackValue *stack = (RaelStackValue*)stack_new(interpreter->argc);
    for (size_t i = 0; i < interpreter->argc; ++i) {
        RaelValue *arg = string_new_pure(interpreter->argv[i], strlen(interpreter->argv[i]), false);
        stack_push(stack, arg);
        value_deref(arg);
    }
    return (RaelValue*)stack;
}

static RaelValue *program_filename_string_new(RaelInterpreter *interpreter) {
    char *name = interpreter->main_stream->name;
    if (name) {
        return string_new_pure(name, strlen(name), false);
    } else {
        return void_new();
    }
}

static RaelValue *raelpath_string_new(RaelInterpreter *interpreter) {
    return string_new_pure(interpreter->exec_path, strlen(interpreter->exec_path), false);
}

RaelValue *module_system_new(RaelInterpreter *interpreter) {
    RaelModuleValue *m = (RaelModuleValue*)module_new(RAEL_HEAPSTR("System"));

    module_set_key(m, RAEL_HEAPSTR("RaelPath"), raelpath_string_new(interpreter));
    module_set_key(m, RAEL_HEAPSTR("ProgramArgv"), program_argv_stack_new(interpreter));
    module_set_key(m, RAEL_HEAPSTR("ProgramFilename"), program_filename_string_new(interpreter));
    module_set_key(m, RAEL_HEAPSTR("Run"), cfunc_ranged_new(RAEL_HEAPSTR("Run"), (RaelRawCFunc)module_system_Run, 1, 2));
    module_set_key(m, RAEL_HEAPSTR("Eval"), cfunc_ranged_new(RAEL_HEAPSTR("Eval"), (RaelRawCFunc)module_system_Eval, 1, 2));
    module_set_key(m, RAEL_HEAPSTR("RunShellCommand"), cfunc_new(RAEL_HEAPSTR("RunShellCommand"), (RaelRawCFunc)module_system_RunShellCommand, 1));
    module_set_key(m, RAEL_HEAPSTR("GetShellOutput"), cfunc_new(RAEL_HEAPSTR("GetShellOutput"), (RaelRawCFunc)module_system_GetShellOutput, 1));
    module_set_key(m, RAEL_HEAPSTR("Exit"), cfunc_ranged_new(RAEL_HEAPSTR("Exit"), (RaelRawCFunc)module_system_Exit, 0, 1));

    return (RaelValue*)m;
}
