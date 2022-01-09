#include "rael.h"

typedef RaelValue *(*RaelBinaryOperationFunction)(RaelValue *, RaelValue *);

// TODO: add interpreter_set_variable function

static void interpreter_interpret_inst(struct Interpreter* const interpreter, struct Instruction* const instruction);
static RaelValue *expr_eval(struct Interpreter* const interpreter, struct Expr* const expr, const bool can_explode);
void block_run(struct Interpreter* const interpreter, struct Instruction **block, bool create_new_scope);

void interpreter_push_scope(struct Interpreter* const interpreter, struct Scope *scope_addr) {
    scope_construct(scope_addr, interpreter->scope);
    interpreter->scope = scope_addr;
}

void interpreter_pop_scope(struct Interpreter* const interpreter) {
    if (interpreter->scope) {
        struct Scope *parent = interpreter->scope->parent;
        scope_dealloc(interpreter->scope);
        interpreter->scope = parent;
    }
}

/* make the interpreter deallocate everything it stores */
static void interpreter_destroy_all(struct Interpreter* const interpreter) {
    // deallocate all of the scopes
    while (interpreter->scope) {
        interpreter_pop_scope(interpreter);
    }

    // deallocate all of the intstructions
    for (size_t i = 0; interpreter->instructions[i]; ++i)
        instruction_delete(interpreter->instructions[i]);

    free(interpreter->instructions);

    if (interpreter->stream_on_heap)
        free(interpreter->stream_base);
}

void interpreter_error(struct Interpreter* const interpreter, struct State state, const char* const error_message, ...) {
    va_list va;
    va_start(va, error_message);
    rael_show_error_message(interpreter->filename, state, error_message, va);
    va_end(va);
    interpreter_destroy_all(interpreter);
    exit(1);
}

static RaelValue *rael_readline(struct Interpreter* const interpreter, struct State state) {
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

static RaelValue *value_eval(struct Interpreter* const interpreter, struct ValueExpr *value) {
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

        new_routine->block = ast_routine.block;
        new_routine->parameters = ast_routine.parameters;
        new_routine->amount_parameters = ast_routine.amount_parameters;
        new_routine->scope = interpreter->scope;

        out_value = (RaelValue*)new_routine;
        break;
    }
    case ValueTypeStack: {
        size_t overhead = value->as_stack.entries.amount_exprs;
        // create a new stack with the exact amount of overhead you need for putting all of the values
        RaelValue *stack = stack_new(overhead);

        // push all of the values
        for (size_t i = 0; i < overhead; ++i) {
            stack_push((RaelStackValue*)stack, expr_eval(interpreter, value->as_stack.entries.exprs[i], true));
        }

        out_value = stack;
        break;
    }
    case ValueTypeVoid:
        out_value = void_new();
        break;
    case ValueTypeType: {
        out_value = (RaelValue*)value->as_type;
        // this is a new instance of the type, add a reference
        value_ref(out_value);
        break;
    }
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

static RaelValue *eval_stack_set(struct Interpreter* const interpreter, struct Expr *at_expr, struct Expr *value_expr) {
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

static RaelValue *eval_at_operation_set(struct Interpreter* const interpreter, struct Expr *at_expr, struct Expr *value_expr,
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

static RaelValue *eval_key_operation_set(struct Interpreter *interpreter, char *key, struct Expr *value_expr,
                                         RaelBinExprFunc operation, struct State expr_state) {
    RaelValue *value;
    RaelValue **lhs_ptr;

    // try to get the pointer to the value you need to modify
    lhs_ptr = scope_get_ptr(interpreter->scope, key);
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
static RaelValue *eval_get_member_operation_set(struct Interpreter *interpreter, struct GetMemberExpr *get_member, struct Expr *value_expr,
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

static RaelValue *eval_set_operation_expr(struct Interpreter *interpreter, struct SetExpr *set_expr, struct State state,
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
static RaelValue *expr_match_eval(struct Interpreter *interpreter, struct MatchExpr *match){
    RaelValue *match_against = expr_eval(interpreter, match->match_against, true);
    RaelValue *return_value;
    bool matched = false;

    // loop all match cases while there's no match
    for (size_t i = 0; i < match->amount_cases && !matched; ++i) {
        struct MatchCase match_case = match->match_cases[i];
        // evaluate the value to compare with
        RaelValue *with_value = expr_eval(interpreter, match_case.case_value, true);

        // check if the case matches, and if it does,
        // run its block and stop the match's execution
        if (values_eq(match_against, with_value)) {
            block_run(interpreter, match_case.case_block, false);
            matched = true;
        }
        value_deref(with_value);
    }

    // if you've matched nothing and there's an else case, run the else block
    if (!matched && match->else_block) {
        block_run(interpreter, match->else_block, false);
    }

    // if there was a return, set the match's return value to the return
    // value and clear the interrupt
    if (interpreter->interrupt == ProgramInterruptReturn) {
        // set return value
        return_value = interpreter->returned_value;
        interpreter->interrupt = ProgramInterruptNone;
    } else {
        // if there was no interrupt, set the return value to Void
        return_value = void_new();
    }

    // deallocate the value it matched against because it has no use anymore
    value_deref(match_against);
    return return_value;
}

static RaelValue *expr_eval(struct Interpreter* const interpreter, struct Expr* const expr, const bool can_explode) {
    RaelValue *lhs;
    RaelValue *rhs;
    RaelValue *single;
    RaelValue *value;

    switch (expr->type) {
    case ExprTypeValue:
        value = value_eval(interpreter, expr->as_value);
        break;
    case ExprTypeKey:
        value = scope_get(interpreter->scope, expr->as_key, interpreter->warn_undefined);
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
            arguments_new(&args); // initialize args
            for (size_t i = 0; i < call.args.amount_exprs; ++i) {
                struct Expr *arg_expr = call.args.exprs[i];
                RaelValue *arg_value = expr_eval(interpreter, arg_expr, true);
                struct State arg_state = arg_expr->state;
                // add the argument
                arguments_add(&args, arg_value, arg_state);
            }
            arguments_finalize(&args); // finish

            // call and remove arguments immediately afterwards
            value = value_call(callable, &args, interpreter);
            assert(value);
            arguments_delete(&args);
        } else {
            value = BLAME_NEW_CSTR("Tried to call a non-callable");
        }

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
            scope_set(interpreter->scope, set.as_key, value, false);
            // reference again because the value set is also the return value of the expression
            value_ref(value);
            break;
        case SetTypeMember: {
            struct GetMemberExpr get_member = set.as_member->as_get_member;

            // get the value to put the member in
            lhs = expr_eval(interpreter, get_member.lhs, true);
            // get the member's value
            value = expr_eval(interpreter, set.expr, true);
            // set the member
            varmap_set(&lhs->keys, get_member.key, value, true, false);

            value_deref(lhs);
            // reference again because the member's value is also the expression's return value
            value_ref(value);
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
        value = value_get_key(lhs, expr->as_get_member.key);
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
        while (state.stream_pos[0] == ' ' || state.stream_pos[0] == '\t') {
            ++state.column;
            ++state.stream_pos;
        }

        rael_show_error_tag(interpreter->filename, state);
        if (blame->message) {
            value_log(blame->message);
        }
        printf("\n");
        rael_show_line_state(state);
        // dereference the blame value
        value_deref(value);
        interpreter_destroy_all(interpreter);
        exit(1);
    }

    return value;
}

void block_run(struct Interpreter* const interpreter, struct Instruction **block, bool create_new_scope) {
    struct Scope block_scope;
    if (create_new_scope)
        interpreter_push_scope(interpreter, &block_scope);
    for (size_t i = 0; block[i]; ++i) {
        interpreter_interpret_inst(interpreter, block[i]);
        if (interpreter->interrupt != ProgramInterruptNone)
            break;
    }
    if (create_new_scope)
        interpreter_pop_scope(interpreter);
}

static void interpreter_interpret_loop(struct Interpreter *interpreter, struct LoopInstruction *loop) {
    struct Scope loop_scope;
    switch (loop->type) {
    case LoopWhile: {
        bool continue_loop;
        do {
            RaelValue *condition;
            interpreter_push_scope(interpreter, &loop_scope);

            condition = expr_eval(interpreter, loop->while_condition, true);
            continue_loop = value_truthy(condition);
            value_deref(condition);
            // if you can loop, run the block
            if (continue_loop) {
                block_run(interpreter, loop->block, false);
                if (interpreter->interrupt == ProgramInterruptBreak) {
                    interpreter->interrupt = ProgramInterruptNone;
                    continue_loop = false;
                } else if (interpreter->interrupt == ProgramInterruptReturn) {
                    continue_loop = false;
                } else if (interpreter->interrupt == ProgramInterruptSkip) {
                    interpreter->interrupt = ProgramInterruptNone;
                }
            }
            interpreter_pop_scope(interpreter);
        } while (continue_loop);
        break;
    }
    case LoopThrough: {
        bool continue_loop = true;
        RaelValue *iterator = expr_eval(interpreter, loop->iterate.expr, true);

        if (!value_is_iterable(iterator)) {
            value_deref(iterator);
            interpreter_error(interpreter, loop->iterate.expr->state, "Expected an iterable");
        }

        // calculate length every time because stacks can always grow
        for (size_t i = 0; continue_loop && i < value_length(iterator); ++i) {
            interpreter_push_scope(interpreter, &loop_scope);
            scope_set_local(interpreter->scope, loop->iterate.key, value_get(iterator, i), false);
            block_run(interpreter, loop->block, false);

            if (interpreter->interrupt == ProgramInterruptBreak) {
                interpreter->interrupt = ProgramInterruptNone;
                continue_loop = false;
            } else if (interpreter->interrupt == ProgramInterruptReturn) {
                continue_loop = false;
            } else if (interpreter->interrupt == ProgramInterruptSkip) {
                interpreter->interrupt = ProgramInterruptNone;
            }
            interpreter_pop_scope(interpreter);
        }
        value_deref(iterator);
        break;
    }
    case LoopForever:
        for (;;) {
            block_run(interpreter, loop->block, true);
            if (interpreter->interrupt == ProgramInterruptBreak) {
                interpreter->interrupt = ProgramInterruptNone;
                break;
            } else if (interpreter->interrupt == ProgramInterruptReturn) {
                break;
            } else if (interpreter->interrupt == ProgramInterruptSkip) {
                interpreter->interrupt = ProgramInterruptNone;
            }
        }
        break;
    default:
        RAEL_UNREACHABLE();
    }
}

static void interpreter_interpret_inst(struct Interpreter* const interpreter, struct Instruction* const instruction) {
    switch (instruction->type) {
    case InstructionTypeLog: {
        RaelValue *value;
        if (instruction->csv.amount_exprs > 0) {
            value_log((value = expr_eval(interpreter, instruction->csv.exprs[0], true)));
            value_deref(value);
            for (size_t i = 1; i < instruction->csv.amount_exprs; ++i) {
                printf(" ");
                value_log((value = expr_eval(interpreter, instruction->csv.exprs[i], true)));
                value_deref(value);
            }
        }
        printf("\n");
        break;
    }
    case InstructionTypeShow: {
        for (size_t i = 0; i < instruction->csv.amount_exprs; ++i) {
            RaelValue *value = expr_eval(interpreter, instruction->csv.exprs[i], true);
            value_log(value);
            value_deref(value);
        }
        break;
    }
    case InstructionTypeIf: {
        struct Scope if_scope;
        RaelValue *condition;
        bool is_true;

        switch (instruction->if_stat.if_type) {
        case IfTypeBlock:
            interpreter_push_scope(interpreter, &if_scope);
            // evaluate condition and check if it is true then dereference the condition
            condition = expr_eval(interpreter, instruction->if_stat.condition, true);
            is_true = value_truthy(condition);
            value_deref(condition);
            if (is_true) {
                block_run(interpreter, instruction->if_stat.if_block, false);
            }
            interpreter_pop_scope(interpreter);
            break;
        case IfTypeInstruction:
            condition = expr_eval(interpreter, instruction->if_stat.condition, true);
            is_true = value_truthy(condition);
            value_deref(condition);
            if (is_true) {
                interpreter_interpret_inst(interpreter, instruction->if_stat.if_instruction);
            }
            break;
        default:
            RAEL_UNREACHABLE();
        }
        if (!is_true) {
            switch (instruction->if_stat.else_type) {
            case ElseTypeBlock:
                block_run(interpreter, instruction->if_stat.else_block, true);
                break;
            case ElseTypeInstruction:
                interpreter_interpret_inst(interpreter, instruction->if_stat.else_instruction);
                break;
            case ElseTypeNone:
                break;
            default:
                RAEL_UNREACHABLE();
            }
        }
        break;
    }
    case InstructionTypeLoop:
        interpreter_interpret_loop(interpreter, &instruction->loop);
        break;
    case InstructionTypePureExpr:
        // dereference result right after evaluation
        value_deref(expr_eval(interpreter, instruction->pure, true));
        break;
    case InstructionTypeReturn: {
        // if there is a return value, return it, and if there isn't, return a Void
        if (instruction->return_value) {
            interpreter->returned_value = expr_eval(interpreter, instruction->return_value, false);
        } else {
            interpreter->returned_value = void_new();
        }
        interpreter->interrupt = ProgramInterruptReturn;
        break;
    }
    case InstructionTypeBreak:
        interpreter->interrupt = ProgramInterruptBreak;
        break;
    case InstructionTypeSkip:
        interpreter->interrupt = ProgramInterruptSkip;
        break;
    case InstructionTypeCatch: {
        RaelValue *caught_value = expr_eval(interpreter, instruction->catch.catch_expr, false);

        // handle blame
        if (blame_validate(caught_value)) {
            block_run(interpreter, instruction->catch.handle_block, true);
        }

        value_deref(caught_value);
        break;
    }
    case InstructionTypeLoad: {
        // try to load module
        RaelValue *module = rael_get_module_by_name(instruction->load.module_name);
        // if you couldn't load the module, error
        if (!module)
            interpreter_error(interpreter, instruction->state, "Unknown module name");
        // set the module
        scope_set(interpreter->scope, instruction->load.module_name, module, false);
        break;
    }
    default:
        RAEL_UNREACHABLE();
    }
}

static void interpreter_set_argv(struct Interpreter *interpreter, char **argv, size_t argc) {
    RaelValue *argv_stack = stack_new(argc);
    for (size_t i = 0; i < argc; ++i) {
        RaelValue *arg = string_new_pure(argv[i], strlen(argv[i]), false);
        stack_push((RaelStackValue*)argv_stack, arg);
    }
    scope_set_local(interpreter->scope, RAEL_HEAPSTR("_Argv"), argv_stack, true);
}

static void interpreter_set_filename(struct Interpreter *interpreter, char *filename) {
    RaelValue *value;
    if (filename) {
        value = string_new_pure(filename, strlen(filename), false);
    } else {
        value = void_new();
    }
    scope_set_local(interpreter->scope, RAEL_HEAPSTR("_Filename"), value, true);
}

void rael_interpret(struct Instruction **instructions, char *stream_base, char *filename, char **argv, size_t argc,
                    const bool stream_on_heap, const bool warn_undefined) {
    struct Instruction *instruction;
    struct Scope bottom_scope;
    struct Interpreter interp = {
        .instructions = instructions,
        .interrupt = ProgramInterruptNone,
        .returned_value = NULL,
        .stream_base = stream_base,
        .warn_undefined = warn_undefined,
        .stream_on_heap = stream_on_heap,
        .filename = filename
    };

    scope_construct(&bottom_scope, NULL);
    interp.scope = &bottom_scope;
    // set _Argv
    interpreter_set_argv(&interp, argv, argc);
    // set _Filename
    interpreter_set_filename(&interp, filename);
    for (interp.idx = 0; (instruction = interp.instructions[interp.idx]); ++interp.idx) {
        interpreter_interpret_inst(&interp, instruction);
    }
    interpreter_destroy_all(&interp);
}
