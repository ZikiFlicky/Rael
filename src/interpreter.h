#include "varmap.h"
#include "parser.h"

struct Interpreter {
    struct Node **instructions;
    size_t idx;
    struct Scope scope;
};

void runtime_error(const char* const error_message);

void interpret(struct Node **instructions);
