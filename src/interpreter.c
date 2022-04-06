#include "rael.h"

typedef RaelValue *(*RaelBinaryOperationFunction)(RaelValue *, RaelValue *);

// TODO: add interpreter_set_variable function

static void interpreter_interpret_inst(RaelInterpreter* const interpreter, RaelInstruction* const instruction);

RaelModuleDecl rael_module_declarations[] = {
    { "Types", module_types_new },
    { "Math", module_math_new },
    { "Time", module_time_new },
    { "Random", module_random_new },
    { "System", module_system_new },
    { "File", module_file_new },
    { "Functional", module_functional_new },
    { "Bin", module_bin_new },
    { "Graphics", module_graphics_new },
    { "Encodings", module_encodings_new }
};

void interpreter_push_scope(RaelInterpreter* const interpreter) {
    struct Scope *new_scope = scope_new(interpreter->instance->scope);
    interpreter->instance->scope = new_scope;
}

void interpreter_pop_scope(RaelInterpreter* const interpreter) {
    if (interpreter->instance->scope) {
        struct Scope *parent = interpreter->instance->scope->parent;
        scope_deref(interpreter->instance->scope);
        interpreter->instance->scope = parent;
    }
}

static RaelValue *interpreter_get_module_by_name(RaelInterpreter *interpreter, char *module_name) {
    const size_t amount_modules = sizeof(rael_module_declarations) / sizeof(RaelModuleDecl);
    RaelInstance *instance = interpreter->instance;

    for (size_t i = 0; i < amount_modules; ++i) {
        RaelModuleDecl *decl = &rael_module_declarations[i];
        if (strcmp(decl->name, module_name) == 0) {
            RaelValue *module;
            // if the module was already loaded
            if (instance->module_cache[i]) {
                module = instance->module_cache[i];
            } else {
                // construct the module
                module = decl->module_creator(interpreter);
                instance->module_cache[i] = module;
            }
            value_ref(module);
            return module;
        }
    }

    // nothing found
    return NULL;
}

static void instance_remove_modules(RaelInstance *instance) {
    const size_t amount_modules = sizeof(rael_module_declarations) / sizeof(RaelModuleDecl);
    for (size_t i = 0; i < amount_modules; ++i) {
        RaelValue *module;
        // if the module was loaded at least once
        if ((module = instance->module_cache[i]))
            value_deref(module);
    }
    free(instance->module_cache);
}

RaelInstance *instance_new(RaelInstance *previous_instance, RaelStream *stream,
                        RaelInstruction **instructions, struct Scope *scope, RaelValue **module_cache) {
    RaelInstance *instance = malloc(sizeof(RaelInstance));

    instance->prev = previous_instance;
    instance->idx = 0;
    instance->instructions = instructions;
    instance->interrupt = ProgramInterruptNone;
    instance->returned_value = NULL;
    instance->inherit_scope = scope ? true : false;
    instance->scope = scope ? scope : scope_new(NULL);
    instance->stream = stream;
    if (module_cache) {
        instance->module_cache = module_cache;
        instance->inherit_modules = true;
    } else {
        instance->module_cache = calloc(sizeof(rael_module_declarations) / sizeof(RaelModuleDecl), sizeof(RaelValue*));
        instance->inherit_modules = false;
    }

    return instance;
}

void instance_delete(RaelInstance *instance) {
    if (!instance->inherit_modules)
        instance_remove_modules(instance);
    if (!instance->inherit_scope) {
        struct Scope *parent;
        for (struct Scope *scope = instance->scope; scope; scope = parent) {
            parent = scope->parent;
            scope_deref(scope);
        }
    }
    if (instance->instructions)
        block_delete(instance->instructions);
    if (instance->stream)
        stream_deref(instance->stream);
    free(instance);
}

/* Add an existing instance to the top of the interpreter */
void interpreter_push_instance(RaelInterpreter* const interpreter, RaelInstance *instance) {
    instance->prev = interpreter->instance;
    interpreter->instance = instance;
}

/* Remove an instance from the top of the interpreter, without deleting it */
void interpreter_pop_instance(RaelInterpreter* const interpreter) {
    interpreter->instance = interpreter->instance->prev;
}

/* Add a new instance to the interpreter */
void interpreter_new_instance(RaelInterpreter* const interpreter, RaelStream *stream,
                            RaelInstruction **instructions, bool inherit_scope, bool inherit_modules) {
    interpreter->instance = instance_new(interpreter->instance,
                                        stream, instructions,
                                        inherit_scope ? interpreter->instance->scope : NULL,
                                        inherit_modules ? interpreter->instance->module_cache : NULL);
}

/* Delete the instance at the top of the interpreter, and load the previous instance */
void interpreter_delete_instance(RaelInterpreter* const interpreter) {
    RaelInstance *instance = interpreter->instance;
    interpreter->instance = instance->prev;
    instance_delete(instance);
}

static unsigned int generate_seed(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (unsigned int)ts.tv_sec % (unsigned int)ts.tv_nsec;
}

void interpreter_construct(RaelInterpreter *out, RaelInstruction **instructions, RaelStream *stream,
                        char* const exec_path, char **arguments, size_t amount_arguments,
                        const bool warn_undefined) {
    unsigned int seed = generate_seed();

    out->exec_path = exec_path;
    out->argv = arguments;
    out->argc = amount_arguments;

    out->main_stream = stream;
    out->instance = NULL;

    // seed the random number generator
    srand(seed);
    out->seed = seed;

    out->warn_undefined = warn_undefined;

    // create a new instance
    interpreter_new_instance(out, stream, instructions, false, false);
}

/* make the interpreter deallocate everything it stores */
void interpreter_destruct(RaelInterpreter* const interpreter) {
    // deallocate all of the scopes
    while (interpreter->instance)
        interpreter_delete_instance(interpreter);
}

/* make the interpreter run its instructions */
void interpreter_interpret(RaelInterpreter *interpreter) {
    RaelInstance *instance = interpreter->instance;
    RaelInstruction *instruction;

    for (instance->idx = 0; (instruction = instance->instructions[instance->idx]); ++instance->idx) {
        interpreter_interpret_inst(interpreter, instruction);
        assert(instance->interrupt == ProgramInterruptNone);
    }
    // reset instance index
    instance->idx = 0;
}

void interpreter_error(RaelInterpreter* const interpreter, struct State state, const char* const error_message, ...) {
    va_list va;
    va_start(va, error_message);
    rael_show_error_message(interpreter->instance->stream->name, state, error_message, va);
    va_end(va);
    interpreter_destruct(interpreter);
    exit(1);
}

static RaelValue *rael_readline(RaelInterpreter* const interpreter, struct State state) {
    size_t allocated, idx = 0;
    char *string = malloc((allocated = 32) * sizeof(char));
    char c;

    while (!feof(stdin) && (c = getchar()) != '\r' && c != '\n') {
        if (idx == allocated)
            string = realloc(string, (allocated += 32) * sizeof(char));
        string[idx++] = c;
    }
    if (feof(stdin)) {
        printf("\n");
        free(string);
        interpreter_error(interpreter, state, "EOF reached while reading stdin");
    }

    string = realloc(string, (allocated = idx) * sizeof(char));

    return string_new_pure(string, allocated, true);
}

static RaelValue *value_eval(RaelInterpreter* const interpreter, struct ValueExpr *value) {
    RaelValue *out_value;

    switch (value->type) {
    case ValueTypeNumber:
        if (value->as_number.is_float) {
            out_value = number_newf(value->as_number.as_float);
        } else {
            out_value = number_newi(value->as_number.as_int);
        }
        break;
    case ValueTypeString:
        out_value = string_new_pure(
            value->as_string.source,
            value->as_string.length,
            false // flags not to deallocate string, because there is still a reference in the ast
        );
        break;
    case ValueTypeRoutine: {
        const struct ASTRoutineValue ast_routine = value->as_routine;
        RaelRoutineValue *new_routine = RAEL_VALUE_NEW(RaelRoutineType, RaelRoutineValue);

        // reference the block
        for (size_t i = 0; ast_routine.block[i]; ++i)
            instruction_ref(ast_routine.block[i]);

        // copy parameters
        new_routine->parameters = malloc(ast_routine.amount_parameters * sizeof(char*));
        for (size_t i = 0; i < ast_routine.amount_parameters; ++i)
            new_routine->parameters[i] = rael_cstr_duplicate(ast_routine.parameters[i]);

        new_routine->block = ast_routine.block;
        new_routine->amount_parameters = ast_routine.amount_parameters;
        new_routine->scope = interpreter->instance->scope;

        scope_ref(new_routine->scope);

        out_value = (RaelValue*)new_routine;
        break;
    }
    case ValueTypeStack: {
        struct ASTStackValue *stack_expr = &value->as_stack;
        RaelExprList *exprlist = &stack_expr->entries;
        size_t overhead = exprlist->amount_exprs;
        RaelValue *stack;

        // create a new stack with the exact amount of overhead you need for putting all of the values
        stack = stack_new(overhead);

        // push all of the values
        for (size_t i = 0; i < overhead; ++i) {
            RaelValue *entry = expr_eval(interpreter, exprlist->exprs[i].expr, true);
            stack_push((RaelStackValue*)stack, entry);
            // remove static reference
            value_deref(entry);
        }

        out_value = stack;
        break;
    }
    case ValueTypeVoid:
        out_value = void_new();
        break;
    default:
        RAEL_UNREACHABLE();
    }

    return out_value;
}

/* return a specific error if the RaelValue is not an int, and a NULL if it is an int */
static RaelValue *value_verify_int(RaelValue *number, struct State number_state) {
    RaelValue *blame;
    if (number->type != &RaelNumberType) {
        blame = BLAME_NEW_CSTR_ST("Expected a number", number_state);
    } else if (!number_is_whole((RaelNumberValue*)number)) {
        blame = BLAME_NEW_CSTR_ST("Float index is not allowed", number_state);
    } else {
        blame = NULL;
    }
    return blame;
}

/* return a specific error if the RaelValue is not an uint, and a NULL if it is a uint */
static RaelValue *value_verify_uint(RaelValue *number, struct State number_state) {
    // the checks for uint follow the same checks as an int does
    RaelValue *blame = value_verify_int(number, number_state);
    // if there was an error, leave it as it is
    if (blame) {
        ;
    } else if (number_to_int((RaelNumberValue*)number) < 0) {
        blame = BLAME_NEW_CSTR_ST("A negative index is not allowed", number_state);
    } else {
        blame = NULL;
    }
    return blame;
}

static RaelValue *eval_stack_set(RaelInterpreter* const interpreter, struct Expr *at_expr, struct Expr *value_expr) {
    RaelValue *stack;
    RaelValue *idx;
    RaelValue *value;

    assert(at_expr->type == ExprTypeAt);
    stack = expr_eval(interpreter, at_expr->lhs, true);
    if (stack->type != &RaelStackType) {
        value_deref(stack);
        return BLAME_NEW_CSTR_ST("Expected stack on the left of 'at' when setting value", at_expr->lhs->state);
    }

    idx = expr_eval(interpreter, at_expr->rhs, true);
    // if the value was not a uint, return an error
    if ((value = value_verify_uint(idx, at_expr->rhs->state))) {
        value_deref(stack);
        value_deref(idx);
        return value;
    }
    // evaluate the rhs of the setting expression
    value = expr_eval(interpreter, value_expr, true);

    // if you were out of range
    if (!stack_set((RaelStackValue*)stack, (size_t)number_to_int((RaelNumberValue*)idx), value)) {
        value_deref(stack);
        value_deref(idx);
        return BLAME_NEW_CSTR_ST("Index too big", at_expr->rhs->state);
    }
    value_deref(stack);
    value_deref(idx);
    value_ref(value);
    return value;
}

static RaelValue *eval_at_operation_set(RaelInterpreter* const interpreter, struct Expr *at_expr, struct Expr *value_expr,
                                        RaelBinaryOperationFunction operation, struct State operator_state) {
    RaelValue *stack;
    RaelValue *idx;
    RaelValue **value_ptr;
    RaelValue *result;
    RaelValue *value;

    assert(at_expr->type == ExprTypeAt);
    stack = expr_eval(interpreter, at_expr->lhs, true);
    if (stack->type != &RaelStackType) {
        value_deref(stack);
        return BLAME_NEW_CSTR_ST("Expected a stack", at_expr->lhs->state);
    }

    idx = expr_eval(interpreter, at_expr->rhs, true);
    if ((result = value_verify_uint(idx, at_expr->rhs->state))) {
        value_deref(stack);
        value_deref(idx);
        return result;
    }
    value_ptr = stack_get_ptr((RaelStackValue*)stack, (size_t)number_to_int((RaelNumberValue*)idx));

    value = expr_eval(interpreter, value_expr, true);

    // if in range of stack
    if (value_ptr) {
        result = operation(*value_ptr, value);
        // if the operation failed, set the return value to be an error
        if (!result) {
            result = BLAME_NEW_CSTR("Invalid operation between values (types don't match)");
        }
    } else {
        result = BLAME_NEW_CSTR_ST("Index too big", at_expr->rhs->state);
    }

    // if got and error, add a state (supposing there isn't one)
    if (blame_validate(result)) {
        blame_set_state((RaelBlameValue*)result, operator_state);
    } else {
        // besides the calculated value being returned from the expression, it is also set in the stack,
        // so its refcount should be incremented
        value_ref(result);
        // dereference the old value in the stack
        value_deref(*value_ptr);
        // set the new value
        *value_ptr = result;
    }

    value_deref(stack);
    value_deref(idx);
    value_deref(value);
    return result;
}

static RaelValue *eval_key_operation_set(RaelInterpreter *interpreter, char *key, struct Expr *value_expr,
                                         RaelBinExprFunc operation, struct State expr_state) {
    RaelValue *value;
    RaelValue **lhs_ptr;

    // try to get the pointer to the value you need to modify
    lhs_ptr = scope_get_ptr(interpreter->instance->scope, key);
    // if there is a value at that key, get its value and do the operation
    if (lhs_ptr) {
        RaelValue *rhs = expr_eval(interpreter, value_expr, true);
        // do the operation on the two values
        value = operation(*lhs_ptr, rhs);
        // dereference the temporary rhs
        value_deref(rhs);
        // if it couldn't add the values
        if (!value) {
            value = BLAME_NEW_CSTR("Invalid operation between values");
        }
    } else {
        value = BLAME_NEW_CSTR("Can't perform such operation on an undefined member");
    }
    // if a blame was created (the blame was returned from the function or the operation was unsuccessful),
    // add a state to it
    if (blame_validate(value)) {
        blame_set_state((RaelBlameValue*)value, expr_state);
    } else {
        // dereference the old value
        value_deref(*lhs_ptr);
        // set the new value
        *lhs_ptr = value;
        // reference the value again because it is returned from the set expression
        value_ref(value);
    }
    return value;
}

/*
 * given an operation a get_member expression, and an expression, sets the key in
 * the lhs of the get_member expression to be the result of the operation on it
 * and the result of the evaluation of the other expression
 */
static RaelValue *eval_get_member_operation_set(RaelInterpreter *interpreter, struct GetMemberExpr *get_member, struct Expr *value_expr,
                                         RaelBinExprFunc operation, struct State expr_state) {
    RaelValue *value;
    RaelValue *get_member_lhs, **lhs_ptr;

    get_member_lhs = expr_eval(interpreter, get_member->lhs, true);
    // try to get the pointer to the value you modify
    lhs_ptr = varmap_get_ptr(&get_member_lhs->keys, get_member->key);
    value_deref(get_member_lhs);
    // if there is a value at that key, get its value and do the operation
    if (lhs_ptr) {
        RaelValue *rhs = expr_eval(interpreter, value_expr, true);
        // do the operation on the two values
        value = operation(*lhs_ptr, rhs);
        // dereference the temporary rhs
        value_deref(rhs);

        // if it couldn't add the values, return a blame
        if (!value) {
            value = BLAME_NEW_CSTR("Invalid operation between values");
        }
    } else {
        value = BLAME_NEW_CSTR("Can't perform such operation on an undefined key");
    }
    // if a blame was created (the blame was returned from the function or the operation was unsuccessful),
    // add a state to it
    if (blame_validate(value)) {
        blame_set_state((RaelBlameValue*)value, expr_state);
    } else {
        // dereference the old value
        value_deref(*lhs_ptr);
        // set the new value
        *lhs_ptr = value;
        // reference the value again because it is returned from the set expression
        value_ref(value);
    }
    return value;
}

static RaelValue *eval_set_operation_expr(RaelInterpreter *interpreter, struct SetExpr *set_expr, struct State state,
                                          RaelBinExprFunc operation) {
    switch (set_expr->set_type) {
    case SetTypeAtExpr:
        return eval_at_operation_set(interpreter, set_expr->as_at,
                                     set_expr->expr, operation, state);
    case SetTypeKey:
        return eval_key_operation_set(interpreter, set_expr->as_key, set_expr->expr,
                                      operation, state);
    case SetTypeMember:
        assert(set_expr->as_member->type == ExprTypeGetMember);
        return eval_get_member_operation_set(interpreter, &set_expr->as_member->as_get_member,
                                         set_expr->expr, operation, state);
    default:
        RAEL_UNREACHABLE();
    }
}

/* a match statement is basically a more complex switch statement */
static RaelValue *expr_match_eval(RaelInterpreter *interpreter, struct MatchExpr *match){
    RaelValue *match_against = expr_eval(interpreter, match->match_against, true);
    RaelValue *return_value;
    bool matched = false;

    // loop all match cases while there's no match
    for (size_t i = 0; i < match->amount_cases && !matched; ++i) {
        struct MatchCase match_case = match->match_cases[i];
        // loop through all exprs in the specific with statement
        for (size_t expr_idx = 0; !matched && expr_idx < match_case.match_exprs.amount_exprs; ++expr_idx) {
            // evaluate the value to compare with
            RaelValue *with_value = expr_eval(interpreter, match_case.match_exprs.exprs[expr_idx].expr, true);

            // check if the case matches, and if it does,
            // run its block and stop the match's execution
            if (values_eq(match_against, with_value)) {
                block_run(interpreter, match_case.case_block, false);
                matched = true;
            }
            value_deref(with_value);
        }
    }

    // if you've matched nothing and there's an else case, run the else block
    if (!matched && match->else_block) {
        block_run(interpreter, match->else_block, false);
    }

    // if there was a return, set the match's return value to the return
    // value and clear the interrupt
    if (interpreter->instance->interrupt == ProgramInterruptReturn) {
        // set return value
        return_value = interpreter->instance->returned_value;
        interpreter->instance->interrupt = ProgramInterruptNone;
    } else {
        // if there was no interrupt, set the return value to Void
        return_value = void_new();
    }

    // deallocate the value it matched against because it has no use anymore
    value_deref(match_against);
    return return_value;
}

RaelValue *expr_eval(RaelInterpreter* const interpreter, struct Expr* const expr, const bool can_explode) {
    RaelValue *lhs;
    RaelValue *rhs;
    RaelValue *single;
    RaelValue *value;

    switch (expr->type) {
    case ExprTypeValue:
        value = value_eval(interpreter, expr->as_value);
        break;
    case ExprTypeKey:
        value = scope_get(interpreter->instance->scope, expr->as_key, interpreter->warn_undefined);
        break;
    case ExprTypeAdd:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        // try to add the values
        value = values_add(lhs, rhs);
        if (!value) {
            value = BLAME_NEW_CSTR("Invalid operation (+) on types");
        }
        if (blame_validate(value)) {
            blame_set_state((RaelBlameValue*)value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeSub:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);
        // try to add the values
        value = values_sub(lhs, rhs);
        if (!value) {
            value = BLAME_NEW_CSTR("Invalid operation (-) on types");
        }
        if (blame_validate(value)) {
            blame_set_state((RaelBlameValue*)value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeMul:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);
        // try to multiply the values
        value = values_mul(lhs, rhs);
        if (!value) {
            value = BLAME_NEW_CSTR("Invalid operation (*) on types");
        }
        if (blame_validate(value)) {
            blame_set_state((RaelBlameValue*)value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeDiv:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        // try to divide the values
        value = values_div(lhs, rhs);
        if (!value) {
            value = BLAME_NEW_CSTR("Invalid operation (/) on types");
        }
        if (blame_validate(value)) {
            blame_set_state((RaelBlameValue*)value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeMod:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        // try to modulo the values
        value = values_mod(lhs, rhs);
        if (!value) {
            value = BLAME_NEW_CSTR("Invalid operation (%) on types");
        }
        if (blame_validate(value)) {
            blame_set_state((RaelBlameValue*)value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeEquals:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        // create an integer from the boolean result of the comparison
        value = number_newi(values_eq(lhs, rhs));

        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeNotEqual:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        // create an integer from the boolean result of the comparison
        value = number_newi(!values_eq(lhs, rhs));

        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeSmallerThan:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        // try to compare
        value = values_smaller(lhs, rhs);
        if (!value) {
            value = BLAME_NEW_CSTR("Invalid operation (<) on types");
        }
        if (blame_validate(value)) {
            blame_set_state((RaelBlameValue*)value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeBiggerThan:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        // try to compare
        value = values_bigger(lhs, rhs);
        if (!value) {
            value = BLAME_NEW_CSTR("Invalid operation (>) on types");
        }
        if (blame_validate(value)) {
            blame_set_state((RaelBlameValue*)value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeSmallerOrEqual:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        // try to compare
        value = values_smaller_eq(lhs, rhs);
        if (!value) {
            value = BLAME_NEW_CSTR("Invalid operation (<=) on types");
        }
        if (blame_validate(value)) {
            blame_set_state((RaelBlameValue*)value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeBiggerOrEqual:
        // eval lhs and rhs in lhs >= rhs
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);
        // try to compare
        value = values_bigger_eq(lhs, rhs);
        if (!value) {
            value = BLAME_NEW_CSTR("Invalid operation (>=) on types");
        }
        if (blame_validate(value)) {
            blame_set_state((RaelBlameValue*)value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeCall: {
        struct CallExpr call = expr->as_call;
        RaelValue *callable = expr_eval(interpreter, call.callable_expr, true);
        RaelArgumentList args;

        if (value_is_callable(callable)) {
            RaelExprList *exprlist = &call.args;

            // initialize args with a good overhead
            arguments_new(&args, exprlist->amount_exprs);

            for (size_t i = 0; i < exprlist->amount_exprs; ++i) {
                struct RaelExprListEntry *entry = &exprlist->exprs[i];
                RaelValue *arg_value = expr_eval(interpreter, entry->expr, true);
                struct State arg_state = entry->start_state;

                // add the argument
                arguments_add(&args, arg_value, arg_state);
                value_deref(arg_value);
            }
            arguments_finalize(&args); // finish

            // call and remove arguments immediately afterwards
            value = value_call(callable, &args, interpreter);
            assert(value);
            arguments_delete(&args);
        } else {
            value = BLAME_NEW_CSTR("Tried to call a non-callable");
        }

        // if the value is a blame, try to add a state to it
        if (blame_validate(value)) {
            blame_set_state((RaelBlameValue*)value, expr->state);
        }
        value_deref(callable);
        break;
    }
    case ExprTypeAt:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        if (rhs->type == &RaelNumberType) {
            value = value_verify_uint(rhs, expr->rhs->state);
            // if there was no problem with the number
            if (!value) {
                size_t index = (size_t)number_to_int((RaelNumberValue*)rhs);
                value = value_get(lhs, index);
                if (!value) {
                    value = BLAME_NEW_CSTR_ST("Can't get numeric index of a value of such type", expr->lhs->state);
                } else if (blame_validate(value)) {
                    blame_set_state((RaelBlameValue*)value, expr->state);
                }
            }
        } else if (rhs->type == &RaelRangeType) {
            RaelRangeValue *range = (RaelRangeValue*)rhs;

            if (range->start < 0 || range->end < 0 || range->start > range->end) {
                value = BLAME_NEW_CSTR_ST("Invalid range for slicing", expr->rhs->state);
            } else {
                value = value_slice(lhs, (size_t)range->start, (size_t)range->end);
                // type is unsliceable
                if (!value) {
                    value = BLAME_NEW_CSTR_ST("Value of such type cannot be sliced", expr->lhs->state);
                } if (blame_validate(value)) {
                    blame_set_state((RaelBlameValue*)value, expr->state);
                }
            }
        } else {
            value = BLAME_NEW_CSTR_ST("Expected Number or Range", expr->rhs->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeTo:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        if (rhs->type == &RaelNumberType) { // range: 'number to number'
            // if there was no problem with the numbers
            if (!(value = value_verify_int(lhs, expr->lhs->state)) &&
                !(value = value_verify_int(rhs, expr->rhs->state))) {
                value = range_new(number_to_int((RaelNumberValue*)lhs),
                                  number_to_int((RaelNumberValue*)rhs));
            }
        } else if (rhs->type == &RaelTypeType) {
            RaelValue *casted = value_cast(lhs, (RaelTypeValue*)rhs);
            if (casted) {
                // if you were returned an error, add a state to it
                if (blame_validate(casted)) {
                    blame_set_state((RaelBlameValue*)casted, expr->state);
                }
                value = casted;
            } else {
                value = BLAME_NEW_CSTR_ST("Can't cast value to such type", expr->rhs->state);
            }
        } else {
            value = BLAME_NEW_CSTR_ST("Expected number or type", expr->rhs->state);
        }

        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeRedirect:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        value = values_red(lhs, rhs); // lhs << rhs
        if (!value) {
            value = BLAME_NEW_CSTR("Invalid operation (<<) on types");
        }
        if (blame_validate(value)) {
            blame_set_state((RaelBlameValue*)value, expr->state);
        }

        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeSizeof:
        single = expr_eval(interpreter, expr->as_single, true);

        if (value_is_iterable(single)) {
            value = number_newi((RaelInt)value_length(single));
        } else if (single->type == &RaelVoidType) {
            value = number_newi(0);
        } else {
            value = BLAME_NEW_CSTR_ST("Unsupported type for 'sizeof' operation", expr->as_single->state);
        }

        value_deref(single);
        break;
    case ExprTypeNeg: {
        single = expr_eval(interpreter, expr->as_single, can_explode);
        value = value_neg(single);
        if (!value) {
            value = BLAME_NEW_CSTR("Unsupported type for '-' prefix operator");
        }
        if (blame_validate(value)) {
            blame_set_state((RaelBlameValue*)value, expr->state);
        }
        value_deref(single);
        break;
    }
    case ExprTypeBlame: {
        RaelValue *message;
        // if there is something after `blame`
        if (expr->as_single) {
            message = expr_eval(interpreter, expr->as_single, true);
        } else {
            message = NULL;
        }
        value = blame_new(message, &expr->state);
        break;
    }
    case ExprTypeSet: {
        struct SetExpr set = expr->as_set;

        switch (expr->as_set.set_type) {
        case SetTypeAtExpr: {
            value = eval_stack_set(interpreter, set.as_at, set.expr);
            break;
        }
        case SetTypeKey:
            value = expr_eval(interpreter, set.expr, true);
            // this also adds a new reference
            scope_set(interpreter->instance->scope, rael_cstr_duplicate(set.as_key), value, true);
            break;
        case SetTypeMember: {
            struct GetMemberExpr get_member = set.as_member->as_get_member;

            // get the value to put the member in
            lhs = expr_eval(interpreter, get_member.lhs, true);
            // get the member's value
            value = expr_eval(interpreter, set.expr, true);
            // set the member
            value_set_key(lhs, rael_cstr_duplicate(get_member.key), value);

            value_deref(lhs);
            break;
        }
        default:
            RAEL_UNREACHABLE();
        }
        break;
    }
    case ExprTypeAddEqual:
        value = eval_set_operation_expr(interpreter, &expr->as_set, expr->state, values_add);
        break;
    case ExprTypeSubEqual:
        value = eval_set_operation_expr(interpreter, &expr->as_set, expr->state, values_sub);
        break;
    case ExprTypeMulEqual:
        value = eval_set_operation_expr(interpreter, &expr->as_set, expr->state, values_mul);
        break;
    case ExprTypeDivEqual:
        value = eval_set_operation_expr(interpreter, &expr->as_set, expr->state, values_div);
        break;
    case ExprTypeModEqual:
        value = eval_set_operation_expr(interpreter, &expr->as_set, expr->state, values_mod);
        break;
    case ExprTypeAnd: {
        int result = 0;
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (value_truthy(lhs) == true) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            result = value_truthy(rhs) == true;
            value_deref(rhs);
        }
        value_deref(lhs);

        value = number_newi(result);
        break;
    }
    case ExprTypeOr: {
        int result = 0;
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (value_truthy(lhs) == true) {
            result = 1;
        } else {
            rhs = expr_eval(interpreter, expr->rhs, true);
            result = value_truthy(rhs) == true;
            value_deref(rhs);
        }
        value_deref(lhs);

        value = number_newi(result);
        break;
    }
    case ExprTypeNot: {
        bool negative;
        // eval the inside of the '!' expr
        single = expr_eval(interpreter, expr->lhs, true);
        // take the opposite of the value's boolean representation
        negative = !value_truthy(single);
        // deallocate the now unused inside value
        value_deref(single);
        // create a new number value from the opposite of `single`
        value = number_newi(negative);
        break;
    }
    case ExprTypeTypeof: {
        RaelValue *val = expr_eval(interpreter, expr->as_single, true);
        value = (RaelValue*)val->type;
        value_ref(value);
        value_deref(val);
        break;
    }
    case ExprTypeGetString:
        single = expr_eval(interpreter, expr->as_single, true);
        value_log(single);
        value_deref(single);
        value = rael_readline(interpreter, expr->state);
        break;
    case ExprTypeMatch:
        value = expr_match_eval(interpreter, &expr->as_match);
        break;
    case ExprTypeGetMember:
        lhs = expr_eval(interpreter, expr->as_get_member.lhs, true);
        // get key from value
        value = value_get_key(lhs, expr->as_get_member.key, interpreter);
        value_deref(lhs);
        break;
    default:
        RAEL_UNREACHABLE();
    }

    if (can_explode && blame_validate(value)) {
        // explode (error)
        RaelBlameValue *blame = (RaelBlameValue*)value;
        struct State state;
        assert(blame->state_defined);
        state = blame->original_place;

        // remove preceding whitespace
        while (state.stream_pos.cur[0] == ' ' || state.stream_pos.cur[0] == '\t') {
            ++state.column;
            ++state.stream_pos.cur;
        }

        rael_show_error_tag(interpreter->instance->stream->name, state);
        if (blame->message)
            value_log(blame->message);
        printf("\n");
        rael_show_line_state(state);
        // dereference the blame value
        value_deref(value);
        interpreter_destruct(interpreter);
        exit(1);
    }

    return value;
}

void block_run(RaelInterpreter* const interpreter, RaelInstruction **block, bool create_new_scope) {
    if (create_new_scope)
        interpreter_push_scope(interpreter);
    for (size_t i = 0; block[i]; ++i) {
        interpreter_interpret_inst(interpreter, block[i]);
        if (interpreter->instance->interrupt != ProgramInterruptNone)
            break;
    }
    if (create_new_scope)
        interpreter_pop_scope(interpreter);
}

void interpreter_interpret_inst_loop(RaelInterpreter *interpreter, RaelLoopInstruction *inst) {
    switch (inst->info.type) {
    case LoopWhile: {
        bool continue_loop;
        do {
            RaelValue *condition;
            interpreter_push_scope(interpreter);

            condition = expr_eval(interpreter, inst->info.while_condition, true);
            continue_loop = value_truthy(condition);
            value_deref(condition);
            // if you can inst, run the block
            if (continue_loop) {
                block_run(interpreter, inst->info.block, false);
                if (interpreter->instance->interrupt == ProgramInterruptBreak) {
                    interpreter->instance->interrupt = ProgramInterruptNone;
                    continue_loop = false;
                } else if (interpreter->instance->interrupt == ProgramInterruptReturn) {
                    continue_loop = false;
                } else if (interpreter->instance->interrupt == ProgramInterruptSkip) {
                    interpreter->instance->interrupt = ProgramInterruptNone;
                }
            }
            interpreter_pop_scope(interpreter);
        } while (continue_loop);
        break;
    }
    case LoopThrough: {
        bool continue_loop = true;
        RaelValue *iterator = expr_eval(interpreter, inst->info.iterate.expr, true);
        struct Expr *secondary_condition = inst->info.iterate.secondary_condition;

        if (!value_is_iterable(iterator)) {
            value_deref(iterator);
            interpreter_error(interpreter, inst->info.iterate.expr->state, "Expected an iterable");
        }

        // calculate length every time because values can always shrink/grow
        for (size_t i = 0; continue_loop && i < value_length(iterator); ++i) {
            RaelValue *iteration_value;

            // if there is a secondary loop condition, exit the loop
            if (secondary_condition) {
                RaelValue *secondary = expr_eval(interpreter, secondary_condition, true);
                bool is_truthy = value_truthy(secondary);
                value_deref(secondary);
                if (!is_truthy)
                    break;
            }

            // push the new scope and calculate the iterated value
            interpreter_push_scope(interpreter);
            iteration_value = value_get(iterator, i);

            // set the iteration value and deref, because the value is already referenced in scope_set_local
            scope_set_local(interpreter->instance->scope, rael_cstr_duplicate(inst->info.iterate.key), iteration_value, true);
            value_deref(iteration_value);

            // run the block of code
            block_run(interpreter, inst->info.block, false);

            // check for program interrupts
            if (interpreter->instance->interrupt == ProgramInterruptBreak) {
                interpreter->instance->interrupt = ProgramInterruptNone;
                continue_loop = false;
            } else if (interpreter->instance->interrupt == ProgramInterruptReturn) {
                continue_loop = false;
            } else if (interpreter->instance->interrupt == ProgramInterruptSkip) {
                interpreter->instance->interrupt = ProgramInterruptNone;
            }
            interpreter_pop_scope(interpreter);
        }
        value_deref(iterator);
        break;
    }
    case LoopForever:
        for (;;) {
            block_run(interpreter, inst->info.block, true);
            if (interpreter->instance->interrupt == ProgramInterruptBreak) {
                interpreter->instance->interrupt = ProgramInterruptNone;
                break;
            } else if (interpreter->instance->interrupt == ProgramInterruptReturn) {
                break;
            } else if (interpreter->instance->interrupt == ProgramInterruptSkip) {
                interpreter->instance->interrupt = ProgramInterruptNone;
            }
        }
        break;
    default:
        RAEL_UNREACHABLE();
    }
}

void interpreter_interpret_inst_log(RaelInterpreter *interpreter, RaelCsvInstruction *inst) {
    if (inst->csv.amount_exprs > 0) {
        RaelValue *value;
        value_log((value = expr_eval(interpreter, inst->csv.exprs[0].expr, true)));
        value_deref(value);
        for (size_t i = 1; i < inst->csv.amount_exprs; ++i) {
            printf(" ");
            value_log((value = expr_eval(interpreter, inst->csv.exprs[i].expr, true)));
            value_deref(value);
        }
    }
    printf("\n");
}

void interpreter_interpret_inst_show(RaelInterpreter *interpreter, RaelCsvInstruction *inst) {
    for (size_t i = 0; i < inst->csv.amount_exprs; ++i) {
        RaelValue *value = expr_eval(interpreter, inst->csv.exprs[i].expr, true);
        value_log(value);
        value_deref(value);
    }
}

void interpreter_interpret_inst_if(RaelInterpreter *interpreter, RaelIfInstruction *inst) {
    RaelValue *condition;
    bool is_true;

    switch (inst->info.if_type) {
    case IfTypeBlock:
        interpreter_push_scope(interpreter);
        // evaluate condition and check if it is true then dereference the condition
        condition = expr_eval(interpreter, inst->info.condition, true);
        is_true = value_truthy(condition);
        value_deref(condition);
        if (is_true) {
            block_run(interpreter, inst->info.if_block, false);
        }
        interpreter_pop_scope(interpreter);
        break;
    case IfTypeInstruction:
        condition = expr_eval(interpreter, inst->info.condition, true);
        is_true = value_truthy(condition);
        value_deref(condition);
        if (is_true) {
            interpreter_interpret_inst(interpreter, inst->info.if_instruction);
        }
        break;
    default:
        RAEL_UNREACHABLE();
    }
    if (!is_true) {
        switch (inst->info.else_type) {
        case ElseTypeBlock:
            block_run(interpreter, inst->info.else_block, true);
            break;
        case ElseTypeInstruction:
            interpreter_interpret_inst(interpreter, inst->info.else_instruction);
            break;
        case ElseTypeNone:
            break;
        default:
            RAEL_UNREACHABLE();
        }
    }
}

void interpreter_interpret_inst_pure(RaelInterpreter *interpreter, RaelPureInstruction *inst) {
    // dereference result right after evaluation
    value_deref(expr_eval(interpreter, inst->expr, true));
}

void interpreter_interpret_inst_return(RaelInterpreter *interpreter, RaelReturnInstruction *inst) {
    // if there is a return value, return it, and if there isn't, return a Void
    if (inst->return_expr) {
        interpreter->instance->returned_value = expr_eval(interpreter, inst->return_expr, false);
    } else {
        interpreter->instance->returned_value = void_new();
    }
    interpreter->instance->interrupt = ProgramInterruptReturn;
}

void interpreter_interpret_inst_break(RaelInterpreter *interpreter, RaelInstruction *inst) {
    (void)inst;
    interpreter->instance->interrupt = ProgramInterruptBreak;
}

void interpreter_interpret_inst_skip(RaelInterpreter *interpreter, RaelInstruction *inst) {
    (void)inst;
    interpreter->instance->interrupt = ProgramInterruptSkip;
}

void interpreter_interpret_inst_catch(RaelInterpreter *interpreter, RaelCatchInstruction *inst) {
    RaelValue *caught_value = expr_eval(interpreter, inst->catch_expr, false);

    // handle blame
    if (blame_validate(caught_value)) {
        // if it is a catch with, set the message of the blame
        if (inst->value_key) {
            RaelValue *message = ((RaelBlameValue*)caught_value)->message;

            if (message) {
                value_ref(message);
            } else {
                // if there is no message, set a void
                message = void_new();
            }
            // set the message as the key
            scope_set(interpreter->instance->scope, rael_cstr_duplicate(inst->value_key), message, true);
            // dereference the value because it's already being referenced in scope_set
            value_deref(message);
        }
        block_run(interpreter, inst->handle_block, true);
    } else if (inst->else_block) {
        if (inst->value_key) {
            scope_set(interpreter->instance->scope, rael_cstr_duplicate(inst->value_key), caught_value, true);
        }
        block_run(interpreter, inst->else_block, true);
    }

    value_deref(caught_value);
}

void interpreter_interpret_inst_load(RaelInterpreter *interpreter, RaelLoadInstruction *inst) {
    // try to load module
    RaelValue *module = interpreter_get_module_by_name(interpreter, inst->module_name);
    // if you couldn't load the module, error
    if (!module)
        interpreter_error(interpreter, ((RaelInstruction*)inst)->state, "Unknown module name");
    // set the module
    scope_set(interpreter->instance->scope, rael_cstr_duplicate(inst->module_name), module, true);
    // deref because the value is referenced when set
    value_deref(module);
}

static void interpreter_interpret_inst(RaelInterpreter* const interpreter, RaelInstruction* const instruction) {
    RaelInstructionRunFunc func = instruction->type->run;
    assert(func);
    func(interpreter, instruction);
}
