#ifndef RAEL_INTERPRETER_H
#define RAEL_INTERPRETER_H

#include "common.h"
#include "parser.h"

typedef struct RaelInterpreter RaelInterpreter;
typedef struct RaelInstance RaelInstance;
typedef RaelValue* (*RaelNewModuleFunc)(RaelInterpreter *interpreter);
typedef void (*RaelInstructionRunFunc)(RaelInterpreter*, RaelInstruction*);
struct Expr;

enum ProgramInterrupt {
    ProgramInterruptNone,
    ProgramInterruptBreak,
    ProgramInterruptReturn,
    ProgramInterruptSkip
};

struct RaelInstance {
    /* Store previous instance */
    RaelInstance *prev;

    RaelStream *stream;
    RaelInstruction **instructions;
    size_t idx;

    /* If true, the scope is inherited from a different instance */
    bool inherit_scope;
    struct Scope *scope;
    enum ProgramInterrupt interrupt;
    RaelValue *returned_value;
    bool inherit_modules;
    RaelValue **module_cache;
};

struct RaelInterpreter {
    char *exec_path;
    char **argv;
    size_t argc;

    RaelStream *main_stream;
    RaelInstance *instance;
    unsigned int seed;

    // warnings
    bool warn_undefined;
};

typedef struct RaelModuleDecl {
    char *name;
    RaelNewModuleFunc module_creator;
} RaelModuleDecl;

extern RaelModuleDecl rael_module_declarations[];

/* standard modules */
RaelValue *module_math_new(RaelInterpreter *interpreter);
RaelValue *module_types_new(RaelInterpreter *interpreter);
RaelValue *module_time_new(RaelInterpreter *interpreter);
RaelValue *module_random_new(RaelInterpreter *interpreter);
RaelValue *module_system_new(RaelInterpreter *interpreter);
RaelValue *module_file_new(RaelInterpreter *interpreter);
RaelValue *module_functional_new(RaelInterpreter *interpreter);
RaelValue *module_bin_new(RaelInterpreter *interpreter);
RaelValue *module_graphics_new(RaelInterpreter *interpreter);

/* interpreter functions */
void interpreter_construct(RaelInterpreter *out, RaelInstruction **instructions, RaelStream *stream,
                        char* const exec_path, char **arguments, size_t amount_arguments,
                        const bool warn_undefined);

void interpreter_destruct(RaelInterpreter* const interpreter);


void interpreter_interpret(RaelInterpreter *interpreter);

void interpreter_push_instance(RaelInterpreter* const interpreter, RaelInstance *instance);

void interpreter_pop_instance(RaelInterpreter* const interpreter);

void interpreter_new_instance(RaelInterpreter* const interpreter, RaelStream *stream,
                            RaelInstruction **instructions, bool inherit_scope, bool inherit_modules);

void interpreter_delete_instance(RaelInterpreter* const interpreter);

void interpreter_push_scope(RaelInterpreter* const interpreter);

void interpreter_pop_scope(RaelInterpreter* const interpreter);

RaelValue *expr_eval(RaelInterpreter* const interpreter, struct Expr* const expr, const bool can_explode);

void block_run(RaelInterpreter* const interpreter, RaelInstruction **block, bool create_new_scope);

/* instance functions */
RaelInstance *instance_new(RaelInstance *previous_instance, RaelStream *stream,
                        RaelInstruction **instructions, struct Scope *scope, RaelValue **module_cache);

void instance_delete(RaelInstance *instance);

#endif /* RAEL_INTERPRETER_H */
