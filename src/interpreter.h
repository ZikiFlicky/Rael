#ifndef RAEL_INTERPRETER_H
#define RAEL_INTERPRETER_H

#include "parser.h"
#include "scope.h"

struct Interpreter {
    struct Node **instructions;
    size_t idx;
    struct Scope scope;
};

void runtime_error(const char* const error_message);

void interpret(struct Node **instructions);

#endif // RAEL_INTERPRETER_H
