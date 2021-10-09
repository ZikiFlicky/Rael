#include "scope.h"
#include "number.h"
#include "value.h"
#include "common.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

enum ProgramInterrupt {
    ProgramInterruptNone,
    ProgramInterruptBreak,
    ProgramInterruptReturn,
};

struct Interpreter {
    char *stream_base;
    struct Instruction **instructions;
    size_t idx;
    struct Scope *scope;
    enum ProgramInterrupt interrupt;
    bool in_loop;
    bool in_routine;
    RaelValue returned_value;

    // warnings
    bool warn_undefined;
};

static void interpreter_interpret_inst(struct Interpreter* const interpreter, struct Instruction* const instruction);
static RaelValue expr_eval(struct Interpreter* const interpreter, struct Expr* const expr, const bool can_explode);
static void block_run(struct Interpreter* const interpreter, struct Instruction **block);

static void interpreter_push_scope(struct Interpreter* const interpreter, struct Scope *scope_addr) {
    scope_construct(scope_addr, interpreter->scope);
    interpreter->scope = scope_addr;
}

static void interpreter_pop_scope(struct Interpreter* const interpreter) {
    if (interpreter->scope) {
        struct Scope *parent = interpreter->scope->parent;
        scope_dealloc(interpreter->scope);
        interpreter->scope = parent;
    }
}

static void interpreter_destroy_all(struct Interpreter* const interpreter) {
    // deallocate all of the scopes
    while (interpreter->scope)
        interpreter_pop_scope(interpreter);

    // deallocate all of the intstructions
    for (size_t i = 0; interpreter->instructions[i]; ++i)
        instruction_delete(interpreter->instructions[i]);

    free(interpreter->instructions);
}

void interpreter_error(struct Interpreter* const interpreter, struct State state, const char* const error_message) {
    rael_show_error_message(state, error_message);
    interpreter_destroy_all(interpreter);
    free(interpreter->stream_base);
    exit(1);
}

static RaelValue value_eval(struct Interpreter* const interpreter, struct ASTValue value) {
    RaelValue out_value = value_create(value.type);

    switch (value.type) {
    case ValueTypeNumber:
        out_value->as_number = value.as_number;
        break;
    case ValueTypeString:
        // it's okay because strings are immutable
        out_value->as_string = value.as_string;
        // flags not to deallocate string, there is still a reference in the ast
        out_value->as_string.does_reference_ast = true;
        break;
    case ValueTypeRoutine:
        out_value->as_routine = value.as_routine;
        out_value->as_routine.scope = interpreter->scope;
        break;
    case ValueTypeStack:
        out_value->as_stack = (struct RaelStackValue) {
            .length = value.as_stack.amount_exprs,
            .allocated = value.as_stack.amount_exprs,
            .values = malloc(value.as_stack.amount_exprs * sizeof(RaelValue))
        };
        for (size_t i = 0; i < value.as_stack.amount_exprs; ++i) {
            out_value->as_stack.values[i] = expr_eval(interpreter, value.as_stack.exprs[i], true);
        }
        break;
    case ValueTypeVoid:
        break;
    default:
        RAEL_UNREACHABLE();
    }

    return out_value;
}

static void value_verify_is_number_uint(struct Interpreter* const interpreter,
                                        struct State number_state, RaelValue number) {
    if (number->type != ValueTypeNumber) {
        value_dereference(number);
        interpreter_error(interpreter, number_state, "Expected number");
    }
    if (number->as_number.is_float) {
        value_dereference(number);
        interpreter_error(interpreter, number_state, "Float index is not allowed");
    }
    if (number->as_number.as_int < 0) {
        value_dereference(number);
        interpreter_error(interpreter, number_state, "A negative index is not allowed");
    }
}

static void stack_set(struct Interpreter* const interpreter, struct Expr *expr, RaelValue value) {
    RaelValue lhs, rhs;

    assert(expr->type == ExprTypeAt);
    lhs = expr_eval(interpreter, expr->lhs, true);

    if (lhs->type != ValueTypeStack) {
        value_dereference(lhs);
        interpreter_error(interpreter, expr->lhs->state, "Expected stack on the left of 'at' when setting value");
    }

    rhs = expr_eval(interpreter, expr->rhs, true);

    value_verify_is_number_uint(interpreter, expr->rhs->state, rhs);

    if ((size_t)rhs->as_number.as_int >= lhs->as_stack.length) {
        value_dereference(lhs);
        value_dereference(rhs);
        interpreter_error(interpreter, expr->rhs->state, "Index too big");
    }

    value_dereference(lhs->as_stack.values[rhs->as_number.as_int]);
    lhs->as_stack.values[rhs->as_number.as_int] = value;
    value_dereference(lhs);
    value_dereference(rhs);
}

static RaelValue value_at(RaelValue iterable, size_t idx) {
    RaelValue value;

    if (iterable->type == ValueTypeStack) {
        value = iterable->as_stack.values[idx];
        ++value->reference_count;
    } else if (iterable->type == ValueTypeString) {
        value = value_create(ValueTypeString);
        value->as_string.length = 1;
        value->as_string.value = malloc(1 * sizeof(char));
        value->as_string.value[0] = iterable->as_string.value[idx];
        value->as_string.does_reference_ast = false;
    } else if (iterable->type == ValueTypeRange) {
        value = value_create(ValueTypeNumber);
        value->as_number.is_float = false;

        if (iterable->as_range.end > iterable->as_range.start)
            value->as_number.as_int = iterable->as_range.start + idx;
        else
            value->as_number.as_int = iterable->as_range.start - idx;
    } else {
        RAEL_UNREACHABLE();
    }

    return value;
}

static RaelValue interpret_value_at(struct Interpreter* const interpreter, struct Expr *expr) {
    RaelValue lhs, rhs, value;

    assert(expr->type == ExprTypeAt);
    // evaluate the lhs of the at expression
    lhs = expr_eval(interpreter, expr->lhs, true);

    if (lhs->type == ValueTypeStack) {
        // evaluate index
        rhs = expr_eval(interpreter, expr->rhs, true);
        value_verify_is_number_uint(interpreter, expr->rhs->state, rhs);

        if ((size_t)rhs->as_number.as_int >= lhs->as_stack.length) {
            value_dereference(lhs);
            value_dereference(rhs);
            interpreter_error(interpreter, expr->rhs->state, "Index too big");
        }

        value = value_at(lhs, rhs->as_number.as_int);
    } else if (lhs->type == ValueTypeString) {
        // evaluate index
        rhs = expr_eval(interpreter, expr->rhs, true);
        value_verify_is_number_uint(interpreter, expr->rhs->state, rhs);

        if ((size_t)rhs->as_number.as_int >= lhs->as_string.length) {
            value_dereference(lhs);
            value_dereference(rhs);
            interpreter_error(interpreter, expr->rhs->state, "Index too big");
        }

        value = value_at(lhs, rhs->as_number.as_int);
    } else if (lhs->type == ValueTypeRange) {
        // evaluate index
        rhs = expr_eval(interpreter, expr->rhs, true);
        value_verify_is_number_uint(interpreter, expr->rhs->state, rhs);

        if ((size_t)rhs->as_number.as_int >= abs(lhs->as_range.end - lhs->as_range.start)) {
            value_dereference(lhs);
            value_dereference(rhs);
            interpreter_error(interpreter, expr->rhs->state, "Index too big");
        }

        value = value_at(lhs, rhs->as_number.as_int);
    } else {
        // save state, dereference and error
        struct State state = expr->lhs->state;
        value_dereference(lhs);
        interpreter_error(interpreter, state, "Expected string, stack or range on the left of 'at'");
    }

    value_dereference(lhs);
    value_dereference(rhs);

    return value;
}

static RaelValue routine_call_eval(struct Interpreter* const interpreter,
                                   struct RoutineCallExpr call, struct State state) {
    struct Scope routine_scope;
    RaelValue maybe_routine = expr_eval(interpreter, call.routine_value, true);
    const bool in_loop_old = interpreter->in_loop;
    const bool in_routine_old = interpreter->in_routine;
    struct Scope *prev_scope;

    interpreter->in_loop = false;
    interpreter->in_routine = true;

    if (maybe_routine->type != ValueTypeRoutine) {
        value_dereference(maybe_routine);
        interpreter_error(interpreter, state, "Call not possible on non-routine");
    }

    if (maybe_routine->as_routine.amount_parameters != call.arguments.amount_exprs) {
        value_dereference(maybe_routine);
        interpreter_error(interpreter, state, "Arguments don't match parameters");
    }

    // create the routine scope
    scope_construct(&routine_scope, maybe_routine->as_routine.scope);

    // set parameters as variables
    for (size_t i = 0; i < maybe_routine->as_routine.amount_parameters; ++i) {
        scope_set_local(&routine_scope,
                        maybe_routine->as_routine.parameters[i],
                        expr_eval(interpreter, call.arguments.exprs[i], true));
    }

    // store current scope and set it to the routine scope
    prev_scope = interpreter->scope;
    interpreter->scope = &routine_scope;
    // run the routine
    block_run(interpreter, maybe_routine->as_routine.block);
    if (interpreter->interrupt == ProgramInterruptReturn) {
        ;
    } else {
        // just got to the end of the function
        interpreter->returned_value = value_create(ValueTypeVoid);
    }

    interpreter->interrupt = ProgramInterruptNone;
    interpreter->in_loop = in_loop_old;
    interpreter->in_routine = in_routine_old;
    // deallocate routine scope and switch scope to previous scope
    scope_dealloc(&routine_scope);
    interpreter->scope = prev_scope;

    return interpreter->returned_value;
}

static RaelValue expr_eval(struct Interpreter* const interpreter, struct Expr* const expr, const bool can_explode) {
    RaelValue lhs, rhs, single, value;

    switch (expr->type) {
    case ExprTypeValue:
        value = value_eval(interpreter, *expr->as_value);
        break;
    case ExprTypeKey:
        value = scope_get(interpreter->scope, expr->as_key, interpreter->warn_undefined);
        break;
    case ExprTypeAdd:
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (lhs->type == ValueTypeNumber) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            if (rhs->type == ValueTypeNumber) {
                value = value_create(ValueTypeNumber);
                value->as_number = number_add(lhs->as_number, rhs->as_number);
            } else {
                value_dereference(rhs);
                goto invalid_types_add;
            }
        } else if (lhs->type == ValueTypeString) {
            struct RaelStringValue string;

            rhs = expr_eval(interpreter, expr->rhs, true);
            if (rhs->type == ValueTypeString) {
                string.length = lhs->as_string.length + rhs->as_string.length;
                string.value = malloc(string.length * sizeof(char));
                string.does_reference_ast = false;

                strncpy(string.value, lhs->as_string.value, lhs->as_string.length);
                strncpy(string.value + lhs->as_string.length, rhs->as_string.value, rhs->as_string.length);

                value = value_create(ValueTypeString);
                value->as_string = string;
            } else {
                value_dereference(rhs);
                goto invalid_types_add;
            }
        } else {
        invalid_types_add:
            value_dereference(lhs);
            interpreter_error(interpreter, expr->state, "Invalid operation (+) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeSub:
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (lhs->type == ValueTypeNumber) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            if (rhs->type == ValueTypeNumber) {
                value = value_create(ValueTypeNumber);
                value->as_number = number_sub(lhs->as_number, rhs->as_number);
            } else {
                value_dereference(rhs);
                goto invalid_types_sub;
            }
        } else {
        invalid_types_sub:
            value_dereference(lhs);
            interpreter_error(interpreter, expr->state, "Invalid operation (-) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeMul:
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (lhs->type == ValueTypeNumber) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            if (rhs->type == ValueTypeNumber) {
                value = value_create(ValueTypeNumber);
                value->as_number = number_mul(lhs->as_number, rhs->as_number);
            } else {
                value_dereference(rhs);
                goto invalid_types_mul;
            }
        } else {
        invalid_types_mul:
            value_dereference(lhs);
            interpreter_error(interpreter, expr->state, "Invalid operation (*) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeDiv:
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (lhs->type == ValueTypeNumber) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            if (rhs->type == ValueTypeNumber) {
                value = value_create(ValueTypeNumber);
                value->as_number = number_div(interpreter, expr->state, lhs->as_number, rhs->as_number);
            } else {
                value_dereference(rhs);
                goto invalid_types_div;
            }
        } else {
        invalid_types_div:
            value_dereference(lhs);
            interpreter_error(interpreter, expr->state, "Invalid operation (/) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeMod:
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (lhs->type == ValueTypeNumber) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            if (rhs->type == ValueTypeNumber) {
                value = value_create(ValueTypeNumber);
                value->as_number = number_mod(interpreter, expr->state, lhs->as_number, rhs->as_number);
            } else {
                value_dereference(rhs);
                goto invalid_types_mod;
            }
        } else {
        invalid_types_mod:
            value_dereference(lhs);
            interpreter_error(interpreter, expr->state, "Invalid operation (%) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeEquals:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        value = value_create(ValueTypeNumber);
        value->as_number.is_float = false;

        // same pointer same thing
        if (lhs == rhs) {
            value->as_number.as_int = 1;
        } else if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value->as_number = number_eq(lhs->as_number, rhs->as_number);
        } else if (lhs->type == ValueTypeString && rhs->type == ValueTypeString) {
            // if they have the same string pointer, they must be equal
            if (lhs->as_string.value == rhs->as_string.value) {
                value->as_number.as_int = 1;
            } else {
                if (lhs->as_string.length == rhs->as_string.length) {
                    value->as_number.as_int = strncmp(lhs->as_string.value,
                                                      rhs->as_string.value,
                                                      lhs->as_string.length) == 0;
                } else {
                    // if lengths don't match, the strings don't match
                    value->as_number.as_int = 0;
                }
            }
        } else if (lhs->type == ValueTypeVoid && rhs->type == ValueTypeVoid) {
            value->as_number.as_int = 1;
        } else {
            value->as_number.as_int = 0;
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeSmallerThen:
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (lhs->type == ValueTypeNumber) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            if (rhs->type == ValueTypeNumber) {
                value = value_create(ValueTypeNumber);
                value->as_number = number_smaller(lhs->as_number, rhs->as_number);
            } else {
                value_dereference(rhs);
                goto invalid_types_smaller;
            }
        } else {
        invalid_types_smaller:
            value_dereference(lhs);
            interpreter_error(interpreter, expr->state, "Invalid operation (-) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeBiggerThen:
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (lhs->type == ValueTypeNumber) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            if (rhs->type == ValueTypeNumber) {
                value = value_create(ValueTypeNumber);
                value->as_number = number_bigger(lhs->as_number, rhs->as_number);
            } else {
                value_dereference(rhs);
                goto invalid_types_bigger;
            }
        } else {
        invalid_types_bigger:
            value_dereference(lhs);
            interpreter_error(interpreter, expr->state, "Invalid operation (-) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeRoutineCall:
        value = routine_call_eval(interpreter, expr->as_call, expr->state);
        break;
    case ExprTypeAt:
        value = interpret_value_at(interpreter, expr);
        break;
    case ExprTypeTo:
        lhs = expr_eval(interpreter, expr->lhs, true);

        // validate that lhs is an int
        if (lhs->type != ValueTypeNumber) {
            value_dereference(lhs);
            interpreter_error(interpreter, expr->lhs->state, "Expected number");
        }
        if (lhs->as_number.is_float){
            value_dereference(lhs);
            interpreter_error(interpreter, expr->lhs->state, "Float not allowed in range");
        }

        // evaluate rhs
        rhs = expr_eval(interpreter, expr->rhs, true);

        // validate that rhs is an int
        if (rhs->type != ValueTypeNumber) {
            value_dereference(lhs);
            value_dereference(rhs);
            interpreter_error(interpreter, expr->rhs->state, "Expected number");
        }
        if (rhs->as_number.is_float) {
            value_dereference(lhs);
            value_dereference(rhs);
            interpreter_error(interpreter, expr->rhs->state, "Float not allowed in range");
        }

        value = value_create(ValueTypeRange);

        value->as_range.start = lhs->as_number.as_int;
        value->as_range.end = rhs->as_number.as_int;

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeRedirect:
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (lhs->type != ValueTypeStack) {
            value_dereference(lhs);
            interpreter_error(interpreter, expr->lhs->state, "Expected a stack value");
        }

        if (lhs->as_stack.length == lhs->as_stack.allocated) {
            lhs->as_stack.values = realloc(lhs->as_stack.values, (lhs->as_stack.allocated += 8) * sizeof(RaelValue));
        }

        rhs = expr_eval(interpreter, expr->rhs, true);

        // no need to dereference a *used* value
        lhs->as_stack.values[lhs->as_stack.length++] = rhs;

        value = lhs;
        break;
    case ExprTypeSizeof: {
        int size;
        single = expr_eval(interpreter, expr->as_single, true);

        // FIXME: those int conversions aren't really safe
        switch (single->type) {
        case ValueTypeStack:
            size = (int)single->as_stack.length;
            break;
        case ValueTypeString:
            size = (int)single->as_string.length;
            break;
        default:
            interpreter_error(interpreter, expr->as_single->state, "Unsupported type for 'sizeof' operation");
        }

        value = value_create(ValueTypeNumber);
        value->as_number = (struct NumberExpr) {
            .is_float = false,
            .as_int = size
        };

        value_dereference(single);

        break;
    }
    case ExprTypeNeg: {
        RaelValue maybe_number = expr_eval(interpreter, expr->as_single, can_explode);

        if (maybe_number->type != ValueTypeNumber) {
            value_dereference(maybe_number);
            interpreter_error(interpreter, expr->as_single->state, "Expected number");
        }

        value = value_create(ValueTypeNumber);
        value->as_number = number_neg(maybe_number->as_number);

        value_dereference(maybe_number);
        break;
    }
    case ExprTypeBlame:
        value = value_create(ValueTypeBlame);
        // if there is something after `blame`
        if (expr->as_single)
            value->as_blame.value = expr_eval(interpreter, expr->as_single, true);
        else
            value->as_blame.value = NULL;
        value->as_blame.original_place = expr->state;
        break;
    case ExprTypeSet:
        switch (expr->as_set.set_type) {
        case SetTypeAtExpr: {
            value = expr_eval(interpreter, expr->as_set.expr, true);
            stack_set(interpreter, expr->as_set.as_at_stat, value);
            break;
        }
        case SetTypeKey:
            value = expr_eval(interpreter, expr->as_set.expr, true);
            scope_set(interpreter->scope, expr->as_set.as_key, value);
            break;
        default:
            RAEL_UNREACHABLE();
        }
        ++value->reference_count;
        break;
    default:
        RAEL_UNREACHABLE();
    }

    if (can_explode && value->type == ValueTypeBlame) {
        struct State state = value->as_blame.original_place;

        // advance all whitespace
        while (state.stream_pos[0] == ' ' || state.stream_pos[0] == '\t') {
            ++state.column;
            ++state.stream_pos;
        }

        printf("Error [%zu:%zu]: ", state.line, state.column);
        if (value->as_blame.value) {
            value_log(value->as_blame.value);
            value_dereference(value->as_blame.value); // dereference
        }
        printf("\n");

        rael_show_line_state(state);

        // dereference the blame value
        value_dereference(value);

        interpreter_destroy_all(interpreter);

        free(interpreter->stream_base);

        exit(1);
    }

    return value;
}

static void block_run(struct Interpreter* const interpreter, struct Instruction **block) {
    for (size_t i = 0; block[i]; ++i) {
        interpreter_interpret_inst(interpreter, block[i]);
        if (interpreter->interrupt != ProgramInterruptNone)
            break;
    }
}

static void interpreter_interpret_inst(struct Interpreter* const interpreter, struct Instruction* const instruction) {
    switch (instruction->type) {
    case InstructionTypeLog: {
        RaelValue value;
        value_log((value = expr_eval(interpreter, instruction->log_values.exprs[0], true)));
        value_dereference(value); // dereference
        for (size_t i = 1; i < instruction->log_values.amount_exprs; ++i) {
            printf(" ");
            value_log((value = expr_eval(interpreter, instruction->log_values.exprs[i], true)));
            value_dereference(value); // dereference
        }
        printf("\n");
        break;
    }
    case InstructionTypeIf: {
        struct Scope if_scope;
        RaelValue condition;

        interpreter_push_scope(interpreter, &if_scope);

        if (value_as_bool((condition = expr_eval(interpreter, instruction->if_stat.condition, true)))) {
            value_dereference(condition);
            block_run(interpreter, instruction->if_stat.block);
        } else {
            switch (instruction->if_stat.else_type) {
            case ElseTypeBlock:
                block_run(interpreter, instruction->if_stat.else_block);
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

        interpreter_pop_scope(interpreter);
        break;
    }
    case InstructionTypeLoop: {
        struct Scope loop_scope;
        const bool in_loop_old = interpreter->in_loop;
        interpreter->in_loop = true;
        interpreter_push_scope(interpreter, &loop_scope);

        switch (instruction->loop.type) {
        case LoopWhile: {
            RaelValue condition;

            while (value_as_bool((condition = expr_eval(interpreter, instruction->loop.while_condition, true)))) {
                value_dereference(condition);
                block_run(interpreter, instruction->loop.block);

                if (interpreter->interrupt == ProgramInterruptBreak) {
                    interpreter->interrupt = ProgramInterruptNone;
                    break;
                } else if (interpreter->interrupt == ProgramInterruptReturn) {
                    break;
                }
            }

            break;
        }
        case LoopThrough: {
            RaelValue iterator = expr_eval(interpreter, instruction->loop.iterate.expr, true);
            size_t length;

            switch (iterator->type) {
            case ValueTypeString:
                length = iterator->as_string.length;
                break;
            case ValueTypeStack:
                length = iterator->as_stack.length;
                break;
            case ValueTypeRange:
                length = abs(iterator->as_range.end - iterator->as_range.start);
                break;
            default:
                value_dereference(iterator);
                interpreter_error(interpreter, instruction->loop.iterate.expr->state, "Expected an iterable");
            }

            for (size_t i = 0; i < length; ++i) {
                scope_set(interpreter->scope, instruction->loop.iterate.key, value_at(iterator, i));
                block_run(interpreter, instruction->loop.block);

                if (interpreter->interrupt == ProgramInterruptBreak) {
                    interpreter->interrupt = ProgramInterruptNone;
                    break;
                } else if (interpreter->interrupt == ProgramInterruptReturn) {
                    break;
                }
            }

            break;
        }
        case LoopForever:
            for (;;) {
                block_run(interpreter, instruction->loop.block);

                if (interpreter->interrupt == ProgramInterruptBreak) {
                    interpreter->interrupt = ProgramInterruptNone;
                    break;
                } else if (interpreter->interrupt == ProgramInterruptReturn) {
                    break;
                }
            }
            break;
        default:
            RAEL_UNREACHABLE();
        }

        interpreter->in_loop = in_loop_old;
        interpreter_pop_scope(interpreter);
        break;
    }
    case InstructionTypePureExpr:
        // dereference result right after evaluation
        value_dereference(expr_eval(interpreter, instruction->pure, true));
        break;
    case InstructionTypeReturn: {
        if (interpreter->in_routine) {
            // if you can set a return value
            if (instruction->return_value) {
                interpreter->returned_value = expr_eval(interpreter, instruction->return_value, false);
            } else {
                interpreter->returned_value = value_create(ValueTypeVoid);
            }
        } else {
            interpreter_error(interpreter, instruction->state, "'^' outside a routine is not permitted");
        }

        interpreter->interrupt = ProgramInterruptReturn;
        break;
    }
    case InstructionTypeBreak:
        if (!interpreter->in_loop)
            interpreter_error(interpreter, instruction->state, "';' has to be inside a loop");

        interpreter->interrupt = ProgramInterruptBreak;
        break;
    case InstructionTypeCatch: {
        RaelValue caught_value = expr_eval(interpreter, instruction->catch.catch_expr, false);

        // handle blame
        if (caught_value->type == ValueTypeBlame) {
            struct Scope catch_scope;
            interpreter_push_scope(interpreter, &catch_scope);
            block_run(interpreter, instruction->catch.handle_block);
            interpreter_pop_scope(interpreter);
        }

        value_dereference(caught_value);

        break;
    }
    default:
        RAEL_UNREACHABLE();
    }
}

void interpret(struct Instruction **instructions, char *stream_base, const bool warn_undefined) {
    struct Instruction *instruction;
    struct Scope bottom_scope;
    struct Interpreter interp = {
        .instructions = instructions,
        .interrupt = ProgramInterruptNone,
        .in_loop = false,
        .in_routine = false,
        .returned_value = NULL,
        .stream_base = stream_base,
        .warn_undefined = warn_undefined
    };

    scope_construct(&bottom_scope, NULL);
    interp.scope = &bottom_scope;
    for (interp.idx = 0; (instruction = interp.instructions[interp.idx]); ++interp.idx) {
        interpreter_interpret_inst(&interp, instruction);
    }
    interpreter_destroy_all(&interp);
}
