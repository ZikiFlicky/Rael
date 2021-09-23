#include "scope.h"
#include "number.h"
#include "value.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

enum ProgramInterrupt {
    ProgramInterruptNone,
    ProgramInterruptBreak,
    ProgramInterruptReturn
};

struct Interpreter {
    struct Node **instructions;
    size_t idx;
    struct Scope scope;
};

static enum ProgramInterrupt interpreter_interpret_node(struct Scope *scope, struct Node* const node,
                                                        RaelValue *returned_value, bool can_break);
static RaelValue expr_eval(struct Scope *scope, struct Expr* const expr);
static void value_log(RaelValue value);
static enum ProgramInterrupt block_run(struct Scope *scope, struct Node **block, RaelValue *returned_value, bool can_break);

void rael_error(struct State state, const char* const error_message) {
    // advance all whitespace
    while (state.stream_pos[0] == ' ' || state.stream_pos[0] == '\t') {
        ++state.column;
        ++state.stream_pos;
    }
    printf("Error [%zu:%zu]: %s\n", state.line, state.column, error_message);
    printf("| ");
    for (int i = -state.column + 1; state.stream_pos[i] && state.stream_pos[i] != '\n'; ++i) {
        if (state.stream_pos[i] == '\t') {
            printf("    ");
        } else {
            putchar(state.stream_pos[i]);
        }
    }
    printf("\n| ");
    for (size_t i = 0; i < state.column - 1; ++i) {
        if (state.stream_pos[-state.column + i] == '\t') {
            printf("    ");
        } else {
            putchar(' ');
        }
    }
    printf("^\n");
    exit(1);
}

static RaelValue value_eval(struct Scope *scope, struct ASTValue value) {
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
        out_value->as_routine.scope = scope;
        break;
    case ValueTypeStack:
        out_value->as_stack = (struct RaelStackValue) {
            .length = value.as_stack.amount_exprs,
            .allocated = value.as_stack.amount_exprs,
            .values = malloc(value.as_stack.amount_exprs * sizeof(RaelValue))
        };
        for (size_t i = 0; i < value.as_stack.amount_exprs; ++i) {
            out_value->as_stack.values[i] = expr_eval(scope, value.as_stack.exprs[i]);
        }
        break;
    case ValueTypeVoid:
        break;
    default:
        assert(0);
    }

    return out_value;
}

static void value_verify_is_number_uint(struct State number_state, RaelValue number) {
    if (number->type != ValueTypeNumber)
        rael_error(number_state, "Expected number");
    if (number->as_number.is_float)
        rael_error(number_state, "Float index is not allowed");
    if (number->as_number.as_int < 0)
        rael_error(number_state, "A negative index is not allowed");
}

static void stack_set(struct Scope *scope, struct Expr *expr, RaelValue value) {
    RaelValue lhs, rhs;

    assert(expr->type == ExprTypeAt);
    lhs = expr_eval(scope, expr->lhs);
    rhs = expr_eval(scope, expr->rhs);

    if (lhs->type != ValueTypeStack)
        rael_error(expr->lhs->state, "Expected stack on the left of 'at' when setting value");

    value_verify_is_number_uint(expr->rhs->state, rhs);

    if ((size_t)rhs->as_number.as_int >= lhs->as_stack.length)
        rael_error(expr->rhs->state, "Index too big");

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
        assert(0);
    }

    return value;
}

static RaelValue interpret_value_at(struct Scope *scope, struct Expr *expr) {
    RaelValue lhs, rhs, value;

    assert(expr->type == ExprTypeAt);
    lhs = expr_eval(scope, expr->lhs);
    rhs = expr_eval(scope, expr->rhs);

    if (lhs->type == ValueTypeStack) {
        value_verify_is_number_uint(expr->rhs->state, rhs);
        if ((size_t)rhs->as_number.as_int >= lhs->as_stack.length)
            rael_error(expr->rhs->state, "Index too big");
        value = value_at(lhs, rhs->as_number.as_int);
    } else if (lhs->type == ValueTypeString) {
        value_verify_is_number_uint(expr->rhs->state, rhs);
        if ((size_t)rhs->as_number.as_int >= lhs->as_string.length)
            rael_error(expr->rhs->state, "Index too big");
        value = value_at(lhs, rhs->as_number.as_int);
    } else if (lhs->type == ValueTypeRange) {
        value_verify_is_number_uint(expr->rhs->state, rhs);
        if ((size_t)rhs->as_number.as_int >= abs(lhs->as_range.end - lhs->as_range.start))
            rael_error(expr->rhs->state, "Index too big");
        value = value_at(lhs, rhs->as_number.as_int);
    } else {
        rael_error(expr->lhs->state, "Expected string, stack or range on the left of 'at'");
    }

    value_dereference(lhs);
    value_dereference(rhs);

    return value;
}

static RaelValue routine_call_eval(struct Scope *scope, struct RoutineCallExpr call, struct State state) {
    struct Scope routine_scope;
    RaelValue return_value;
    RaelValue maybe_routine = expr_eval(scope, call.routine_value);

    if (maybe_routine->type != ValueTypeRoutine) {
        value_dereference(maybe_routine);
        rael_error(state, "Call not possible on non-routine");
    }

    scope_construct(&routine_scope, maybe_routine->as_routine.scope);

    if (maybe_routine->as_routine.amount_parameters != call.arguments.amount_exprs)
        rael_error(state, "Arguments don't match parameters");

    // set parameters as variables
    for (size_t i = 0; i < maybe_routine->as_routine.amount_parameters; ++i) {
        scope_set_local(&routine_scope,
                        maybe_routine->as_routine.parameters[i],
                        expr_eval(scope, call.arguments.exprs[i]));
    }

    if (block_run(&routine_scope, maybe_routine->as_routine.block, &return_value, false) == ProgramInterruptReturn) {
        scope_dealloc(&routine_scope);
        return return_value;
    }

    scope_dealloc(&routine_scope);
    return value_create(ValueTypeVoid);
}

static RaelValue expr_eval(struct Scope *scope, struct Expr* const expr) {
    RaelValue lhs, rhs, single, value;

    switch (expr->type) {
    case ExprTypeValue:
        return value_eval(scope, *expr->as_value);
    case ExprTypeKey:
        return scope_get(scope, expr->as_key);
    case ExprTypeAdd:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_add(lhs->as_number, rhs->as_number);
        } else if (lhs->type == ValueTypeString && rhs->type == ValueTypeString) {
            struct RaelStringValue string;

            string.length = lhs->as_string.length + rhs->as_string.length;
            string.value = malloc(string.length * sizeof(char));
            string.does_reference_ast = false;

            strncpy(string.value, lhs->as_string.value, lhs->as_string.length);
            strncpy(string.value + lhs->as_string.length, rhs->as_string.value, rhs->as_string.length);

            value = value_create(ValueTypeString);
            value->as_string = string;
        } else {
            rael_error(expr->state, "Invalid operation (+) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        return value;
    case ExprTypeSub:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_sub(lhs->as_number, rhs->as_number);
        } else {
            rael_error(expr->state, "Invalid operation (-) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        return value;
    case ExprTypeMul:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_mul(lhs->as_number, rhs->as_number);
        } else {
            rael_error(expr->state, "Invalid operation (*) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        return value;
    case ExprTypeDiv:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_div(expr->state, lhs->as_number, rhs->as_number);
        } else {
            rael_error(expr->state, "Invalid operation (/) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        return value;
    case ExprTypeMod:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_mod(expr->state, lhs->as_number, rhs->as_number);
        } else {
            rael_error(expr->state, "Invalid operation (%) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        return value;
    case ExprTypeEquals:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);

        value = value_create(ValueTypeNumber);
        value->as_number.is_float = false;

        if (lhs == rhs) {
            value->as_number.as_int = 1;
        } else if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value->as_number = number_eq(lhs->as_number, rhs->as_number);
        } else if (lhs->type == ValueTypeString && rhs->type == ValueTypeString) {
            // if they have the same pointer, they must be equal
            if (lhs->as_string.value == rhs->as_string.value) {
                value->as_number.as_int = 1;
            } else {
                if (lhs->as_string.length == rhs->as_string.length)
                    value->as_number.as_int = strncmp(lhs->as_string.value,
                                                      rhs->as_string.value,
                                                      lhs->as_string.length) == 0;
                else
                    value->as_number.as_int = 0; // if lengths don't match, the strings don't match
            }
        } else if (lhs->type == ValueTypeVoid && rhs->type == ValueTypeVoid) {
            value->as_number.as_int = 1;
        } else {
            value->as_number.as_int = 0;
        }

        value_dereference(lhs);
        value_dereference(rhs);

        return value;
    case ExprTypeSmallerThen:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_smaller(lhs->as_number, rhs->as_number);
        } else {
            rael_error(expr->state, "Invalid operation (<) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        return value;
    case ExprTypeBiggerThen:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_bigger(lhs->as_number, rhs->as_number);
        } else {
            rael_error(expr->state, "Invalid operation (>) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        return value;
    case ExprTypeRoutineCall:
        return routine_call_eval(scope, expr->as_call, expr->state);
    case ExprTypeAt:
        return interpret_value_at(scope, expr);
    case ExprTypeTo:
        lhs = expr_eval(scope, expr->lhs);

        // validate that lhs is an int
        if (lhs->type != ValueTypeNumber)
            rael_error(expr->lhs->state, "Expected number");
        if (lhs->as_number.is_float)
            rael_error(expr->lhs->state, "Float not allowed in range");

        rhs = expr_eval(scope, expr->rhs);

        // validate that rhs is an int
        if (rhs->type != ValueTypeNumber)
            rael_error(expr->rhs->state, "Expected number");
        if (rhs->as_number.is_float)
            rael_error(expr->rhs->state, "Float not allowed in range");

        value = value_create(ValueTypeRange);

        value->as_range.start = lhs->as_number.as_int;
        value->as_range.end = rhs->as_number.as_int;

        value_dereference(lhs);
        value_dereference(rhs);

        return value;
    case ExprTypeRedirect:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);

        if (lhs->type != ValueTypeStack) {
            rael_error(expr->lhs->state, "Expected a stack value");
        }
        if (lhs->as_stack.length == lhs->as_stack.allocated) {
            lhs->as_stack.values = realloc(lhs->as_stack.values, (lhs->as_stack.allocated += 8) * sizeof(RaelValue));
        }

        // no need to dereference a *used* value
        lhs->as_stack.values[lhs->as_stack.length++] = rhs;

        return lhs;
    case ExprTypeSizeof: {
        int size;
        single = expr_eval(scope, expr->as_single);

        // FIXME: those int conversions aren't really safe
        switch (single->type) {
        case ValueTypeStack:
            size = (int)single->as_stack.length;
            break;
        case ValueTypeString:
            size = (int)single->as_string.length;
            break;
        default:
            rael_error(expr->as_single->state, "Unsupported type for 'sizeof' operation");
        }

        value = value_create(ValueTypeNumber);
        value->as_number = (struct NumberExpr) {
            .is_float = false,
            .as_int = size
        };

        value_dereference(single);

        return value;
    }
    case ExprTypeNeg: {
        RaelValue maybe_number = expr_eval(scope, expr->as_single);

        if (maybe_number->type != ValueTypeNumber) {
            rael_error(expr->as_single->state, "Expected number");
        }

        value = value_create(ValueTypeNumber);
        value->as_number = number_neg(maybe_number->as_number);

        value_dereference(maybe_number);
        return value;
    }
    default:
        assert(0);
    }
}

static void value_log_as_original(RaelValue value) {
    switch (value->type) {
    case ValueTypeNumber:
        if (value->as_number.is_float) {
            printf("%f", value->as_number.as_float);
        } else {
            printf("%d", value->as_number.as_int);
        }
        break;
    case ValueTypeString:
        putchar('"');
        for (size_t i = 0; i < value->as_string.length; ++i) {
            switch (value->as_string.value[i]) {
            case '\n':
                printf("\\n");
                break;
            case '\r':
                printf("\\r");
                break;
            case '\t':
                printf("\\t");
                break;
            case '"':
                printf("\\\"");
                break;
            default:
                putchar(value->as_string.value[i]);
            }
        }
        putchar('"');
        break;
    case ValueTypeVoid:
        printf("Void");
        break;
    case ValueTypeRoutine:
        printf("routine(");
        if (value->as_routine.amount_parameters > 0) {
            printf(":%s", value->as_routine.parameters[0]);
            for (size_t i = 1; i < value->as_routine.amount_parameters; ++i)
                printf(", :%s", value->as_routine.parameters[i]);
        }
        printf(")");
        break;
    case ValueTypeStack:
        printf("{ ");
        for (size_t i = 0; i < value->as_stack.length; ++i) {
            if (i > 0)
                printf(", ");
            value_log_as_original(value->as_stack.values[i]);
        }
        printf(" }");
        break;
    case ValueTypeRange:
        printf("%d to %d", value->as_range.start, value->as_range.end);
        break;
    default:
        assert(0);
    }
}

static void value_log(RaelValue value) {
    // only strings are printed differently when `log`ed then inside a stack
    switch (value->type) {
    case ValueTypeString:
        for (size_t i = 0; i < value->as_string.length; ++i)
            putchar(value->as_string.value[i]);
        break;
    default:
        value_log_as_original(value);
    }
}

/* is the value booleanly true? like Python's bool() operator */
static bool value_as_bool(const RaelValue value) {
    switch (value->type) {
    case ValueTypeVoid:
        return false;
    case ValueTypeString:
        return value->as_string.length != 0;
    case ValueTypeNumber:
        if (value->as_number.is_float)
            return value->as_number.as_float != 0.0f;
        else
            return value->as_number.as_int != 0;
    case ValueTypeRoutine:
        return true;
    case ValueTypeStack:
        return value->as_stack.length != 0;
    case ValueTypeRange:
        return true;
    default:
        assert(0);
    }
}

static enum ProgramInterrupt block_run(struct Scope *scope, struct Node **block, RaelValue *returned_value, bool can_break) {
    enum ProgramInterrupt interrupt = ProgramInterruptNone;

    for (size_t i = 0; block[i]; ++i) {
        if ((interrupt = interpreter_interpret_node(scope, block[i], returned_value, can_break)))
            break;
    }

    return interrupt;
}

/*
    returns true if got a return statement
*/
static enum ProgramInterrupt interpreter_interpret_node(struct Scope *scope, struct Node* const node,
                                                        RaelValue *returned_value, bool can_break) {
    enum ProgramInterrupt interrupt = ProgramInterruptNone;

    switch (node->type) {
    case NodeTypeLog: {
        RaelValue value;
        value_log((value = expr_eval(scope, node->log_values.exprs[0])));
        value_dereference(value); // dereference
        for (size_t i = 1; i < node->log_values.amount_exprs; ++i) {
            printf(" ");
            value_log((value = expr_eval(scope, node->log_values.exprs[i])));
            value_dereference(value); // dereference
        }
        printf("\n");
        break;
    }
    case NodeTypeBlame: {
        RaelValue value;
        printf("Error [%zu:%zu]: ", node->state.line, node->state.column);
        if (node->blame_values.amount_exprs > 0) {
            value_log((value = expr_eval(scope, node->blame_values.exprs[0])));
            value_dereference(value); // dereference
            for (size_t i = 1; i < node->blame_values.amount_exprs; ++i) {
                printf(" ");
                value_log((value = expr_eval(scope, node->blame_values.exprs[i])));
                value_dereference(value); // dereference
            }
        }
        printf("\n");
        printf("| ");
        for (int i = -node->state.column + 1; node->state.stream_pos[i] && node->state.stream_pos[i] != '\n'; ++i) {
            if (node->state.stream_pos[i] == '\t') {
                printf("    ");
            } else {
                putchar(node->state.stream_pos[i]);
            }
        }
        printf("\n| ");
        for (size_t i = 0; i < node->state.column - 1; ++i) {
            if (node->state.stream_pos[-node->state.column + i] == '\t') {
                printf("    ");
            } else {
                putchar(' ');
            }
        }
        printf("^\n");
        exit(1);
    }
    case NodeTypeSet:
        switch (node->set.set_type) {
        case SetTypeAtExpr: {
            stack_set(scope, node->set.as_at_stat, expr_eval(scope, node->set.expr));
            break;
        }
        case SetTypeKey:
            scope_set(scope, node->set.as_key, expr_eval(scope, node->set.expr));
            break;
        default:
            assert(0);
        }
        break;
    case NodeTypeIf: {
        struct Scope if_scope;
        RaelValue condition;
        scope_construct(&if_scope, scope);

        if (value_as_bool((condition = expr_eval(scope, node->if_stat.condition)))) {
            value_dereference(condition);
            interrupt = block_run(&if_scope, node->if_stat.block, returned_value, can_break);
        } else {
            switch (node->if_stat.else_type) {
            case ElseTypeBlock:
                interrupt = block_run(&if_scope, node->if_stat.else_block, returned_value, can_break);
                break;
            case ElseTypeNode:
                interrupt = interpreter_interpret_node(&if_scope, node->if_stat.else_node, returned_value, can_break);
                break;
            case ElseTypeNone:
                break;
            default:
                assert(0);
            }
        }

        scope_dealloc(&if_scope);
        break;
    }
    case NodeTypeLoop: {
        struct Scope loop_scope;
        scope_construct(&loop_scope, scope);

        switch (node->loop.type) {
        case LoopWhile: {
            RaelValue condition;

            while (value_as_bool((condition = expr_eval(&loop_scope, node->loop.while_condition)))) {
                value_dereference(condition);
                interrupt = block_run(&loop_scope, node->loop.block, returned_value, true);

                if (interrupt == ProgramInterruptBreak) {
                    interrupt = ProgramInterruptNone;
                    break;
                } else if (interrupt == ProgramInterruptReturn) {
                    break;
                }
            }

            break;
        }
        case LoopThrough: {
            RaelValue iterator = expr_eval(scope, node->loop.iterate.expr);
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
                rael_error(node->loop.iterate.expr->state, "Expected an iterable");
            }

            for (size_t i = 0; i < length; ++i) {
                scope_set(&loop_scope, node->loop.iterate.key, value_at(iterator, i));
                interrupt = block_run(&loop_scope, node->loop.block, returned_value, true);

                if (interrupt == ProgramInterruptBreak) {
                    interrupt = ProgramInterruptNone;
                    break;
                } else if (interrupt == ProgramInterruptReturn) {
                    break;
                }
            }

            break;
        }
        case LoopForever:
            for (;;) {
                interrupt = block_run(&loop_scope, node->loop.block, returned_value, true);

                if (interrupt == ProgramInterruptBreak) {
                    interrupt = ProgramInterruptNone;
                    break;
                } else if (interrupt == ProgramInterruptReturn) {
                    break;
                }
            }
            break;
        default:
            assert(0);
        }

        scope_dealloc(&loop_scope);
        break;
    }
    case NodeTypePureExpr:
        // dereference result right after evaluation
        value_dereference(expr_eval(scope, node->pure));
        break;
    case NodeTypeReturn:
        if (returned_value) {
            if (node->return_value) {
                *returned_value = expr_eval(scope, node->return_value);
            } else {
                *returned_value = value_create(ValueTypeVoid);
            }
        } else {
            rael_error(node->state, "'^' outside a routine is not permitted");
        }

        interrupt = ProgramInterruptReturn;
        break;
    case NodeTypeBreak:
        if (!can_break) {
            rael_error(node->state, "';' has to be inside a loop");
        }
        interrupt = ProgramInterruptBreak;
        break;
    default:
        assert(0);
    }

    return interrupt;
}

void interpret(struct Node **instructions) {
    struct Node *node;
    struct Interpreter interp = {
        .instructions = instructions
    };

    scope_construct(&interp.scope, NULL);
    for (interp.idx = 0; (node = interp.instructions[interp.idx]); ++interp.idx) {
        interpreter_interpret_node(&interp.scope, node, NULL, false);
    }
    scope_dealloc(&interp.scope);
    for (interp.idx = 0; (node = interp.instructions[interp.idx]); ++interp.idx) {
        node_delete(node);
    }
    free(interp.instructions);
}
