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
    ProgramInterruptReturn,
};

struct Interpreter {
    struct Node **instructions;
    size_t idx;
    struct Scope scope;
    enum ProgramInterrupt interrupt;
    bool can_break;
};

static void interpreter_interpret_node(struct Interpreter* const interpreter, struct Scope *scope,
                                       struct Node* const node, RaelValue *returned_value);
static RaelValue expr_eval(struct Interpreter* const interpreter, struct Scope *scope, struct Expr* const expr, const bool can_explode);
static void value_log(RaelValue value);
static void block_run(struct Interpreter* const interpreter, struct Scope *scope, struct Node **block, RaelValue *returned_value);

static void rael_show_line_state(struct State state) {
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
}

void rael_error(struct State state, const char* const error_message) {
    // advance all whitespace
    while (state.stream_pos[0] == ' ' || state.stream_pos[0] == '\t') {
        ++state.column;
        ++state.stream_pos;
    }
    printf("Error [%zu:%zu]: %s\n", state.line, state.column, error_message);
    rael_show_line_state(state);
    exit(1);
}

static RaelValue value_eval(struct Interpreter* const interpreter, struct Scope *scope, struct ASTValue value) {
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
            out_value->as_stack.values[i] = expr_eval(interpreter, scope, value.as_stack.exprs[i], true);
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

static void stack_set(struct Interpreter* const interpreter, struct Scope *scope, struct Expr *expr, RaelValue value) {
    RaelValue lhs, rhs;

    assert(expr->type == ExprTypeAt);
    lhs = expr_eval(interpreter, scope, expr->lhs, true);
    rhs = expr_eval(interpreter, scope, expr->rhs, true);

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

static RaelValue interpret_value_at(struct Interpreter* const interpreter, struct Scope *scope, struct Expr *expr) {
    RaelValue lhs, rhs, value;

    assert(expr->type == ExprTypeAt);
    lhs = expr_eval(interpreter, scope, expr->lhs, true);
    rhs = expr_eval(interpreter, scope, expr->rhs, true);

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

static RaelValue routine_call_eval(struct Interpreter* const interpreter, struct Scope *scope,
                                   struct RoutineCallExpr call, struct State state) {
    struct Scope routine_scope;
    RaelValue return_value;
    RaelValue maybe_routine = expr_eval(interpreter, scope, call.routine_value, true);
    const bool can_break_old = interpreter->can_break;

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
                        expr_eval(interpreter, scope, call.arguments.exprs[i], true));
    }

    interpreter->can_break = false;
    block_run(interpreter, &routine_scope, maybe_routine->as_routine.block, &return_value);
    if (interpreter->interrupt == ProgramInterruptReturn) {
        interpreter->interrupt = ProgramInterruptNone;
        interpreter->can_break = can_break_old;
        scope_dealloc(&routine_scope);
        return return_value;
    }

    interpreter->can_break = can_break_old;
    scope_dealloc(&routine_scope);
    return value_create(ValueTypeVoid);
}

static RaelValue expr_eval(struct Interpreter* const interpreter, struct Scope *scope,
                           struct Expr* const expr, const bool can_explode) {
    RaelValue lhs, rhs, single, value;

    switch (expr->type) {
    case ExprTypeValue:
        value = value_eval(interpreter, scope, *expr->as_value);
        break;
    case ExprTypeKey:
        value = scope_get(scope, expr->as_key);
        break;
    case ExprTypeAdd:
        lhs = expr_eval(interpreter, scope, expr->lhs, true);
        rhs = expr_eval(interpreter, scope, expr->rhs, true);

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

        break;
    case ExprTypeSub:
        lhs = expr_eval(interpreter, scope, expr->lhs, true);
        rhs = expr_eval(interpreter, scope, expr->rhs, true);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_sub(lhs->as_number, rhs->as_number);
        } else {
            rael_error(expr->state, "Invalid operation (-) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeMul:
        lhs = expr_eval(interpreter, scope, expr->lhs, true);
        rhs = expr_eval(interpreter, scope, expr->rhs, true);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_mul(lhs->as_number, rhs->as_number);
        } else {
            rael_error(expr->state, "Invalid operation (*) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeDiv:
        lhs = expr_eval(interpreter, scope, expr->lhs, true);
        rhs = expr_eval(interpreter, scope, expr->rhs, true);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_div(expr->state, lhs->as_number, rhs->as_number);
        } else {
            rael_error(expr->state, "Invalid operation (/) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeMod:
        lhs = expr_eval(interpreter, scope, expr->lhs, true);
        rhs = expr_eval(interpreter, scope, expr->rhs, true);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_mod(expr->state, lhs->as_number, rhs->as_number);
        } else {
            rael_error(expr->state, "Invalid operation (%) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeEquals:
        lhs = expr_eval(interpreter, scope, expr->lhs, true);
        rhs = expr_eval(interpreter, scope, expr->rhs, true);

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

        break;
    case ExprTypeSmallerThen:
        lhs = expr_eval(interpreter, scope, expr->lhs, true);
        rhs = expr_eval(interpreter, scope, expr->rhs, true);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_smaller(lhs->as_number, rhs->as_number);
        } else {
            rael_error(expr->state, "Invalid operation (<) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeBiggerThen:
        lhs = expr_eval(interpreter, scope, expr->lhs, true);
        rhs = expr_eval(interpreter, scope, expr->rhs, true);

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
            value = value_create(ValueTypeNumber);
            value->as_number = number_bigger(lhs->as_number, rhs->as_number);
        } else {
            rael_error(expr->state, "Invalid operation (>) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);

        break;
    case ExprTypeRoutineCall:
        value = routine_call_eval(interpreter, scope, expr->as_call, expr->state);
        break;
    case ExprTypeAt:
        value = interpret_value_at(interpreter, scope, expr);
        break;
    case ExprTypeTo:
        lhs = expr_eval(interpreter, scope, expr->lhs, true);

        // validate that lhs is an int
        if (lhs->type != ValueTypeNumber)
            rael_error(expr->lhs->state, "Expected number");
        if (lhs->as_number.is_float)
            rael_error(expr->lhs->state, "Float not allowed in range");

        rhs = expr_eval(interpreter, scope, expr->rhs, true);

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

        break;
    case ExprTypeRedirect:
        lhs = expr_eval(interpreter, scope, expr->lhs, true);
        rhs = expr_eval(interpreter, scope, expr->rhs, true);

        if (lhs->type != ValueTypeStack) {
            rael_error(expr->lhs->state, "Expected a stack value");
        }
        if (lhs->as_stack.length == lhs->as_stack.allocated) {
            lhs->as_stack.values = realloc(lhs->as_stack.values, (lhs->as_stack.allocated += 8) * sizeof(RaelValue));
        }

        // no need to dereference a *used* value
        lhs->as_stack.values[lhs->as_stack.length++] = rhs;

        value = lhs;
        break;
    case ExprTypeSizeof: {
        int size;
        single = expr_eval(interpreter, scope, expr->as_single, true);

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

        break;
    }
    case ExprTypeNeg: {
        RaelValue maybe_number = expr_eval(interpreter, scope, expr->as_single, can_explode);

        if (maybe_number->type != ValueTypeNumber) {
            rael_error(expr->as_single->state, "Expected number");
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
            value->as_blame.value = expr_eval(interpreter, scope, expr->as_single, true);
        else
            value->as_blame.value = NULL;
        value->as_blame.original_place = expr->state;
        break;
    case ExprTypeSet:
        switch (expr->as_set.set_type) {
        case SetTypeAtExpr: {
            value = expr_eval(interpreter, scope, expr->as_set.expr, true);
            stack_set(interpreter, scope, expr->as_set.as_at_stat, value);
            break;
        }
        case SetTypeKey:
            value = expr_eval(interpreter, scope, expr->as_set.expr, true);
            scope_set(scope, expr->as_set.as_key, value);
            break;
        default:
            assert(0);
        }
        ++value->reference_count;
        break;
    default:
        assert(0);
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

        // deallocate all of the scopes
        while (scope) {
            struct Scope *next_scope = scope->parent;
            scope_dealloc(scope);
            scope = next_scope;
        }

        // deallocate all of the intstructions
        for (size_t i = 0; interpreter->instructions[i]; ++i)
            node_delete(interpreter->instructions[i]);

        free(interpreter->instructions);

        exit(1);
    }

    return value;
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
            case '\\':
                printf("\\\\");
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

static void block_run(struct Interpreter* const interpreter, struct Scope *scope, struct Node **block, RaelValue *returned_value) {
    for (size_t i = 0; block[i]; ++i) {
        interpreter_interpret_node(interpreter, scope, block[i], returned_value);
        if (interpreter->interrupt != ProgramInterruptNone)
            break;
    }
}

static void interpreter_interpret_node(struct Interpreter* const interpreter,
                                       struct Scope *scope, struct Node* const node,
                                       RaelValue *returned_value) {
    switch (node->type) {
    case NodeTypeLog: {
        RaelValue value;
        value_log((value = expr_eval(interpreter, scope, node->log_values.exprs[0], true)));
        value_dereference(value); // dereference
        for (size_t i = 1; i < node->log_values.amount_exprs; ++i) {
            printf(" ");
            value_log((value = expr_eval(interpreter, scope, node->log_values.exprs[i], true)));
            value_dereference(value); // dereference
        }
        printf("\n");
        break;
    }
    case NodeTypeIf: {
        struct Scope if_scope;
        RaelValue condition;
        scope_construct(&if_scope, scope);

        if (value_as_bool((condition = expr_eval(interpreter, scope, node->if_stat.condition, true)))) {
            value_dereference(condition);
            block_run(interpreter, &if_scope, node->if_stat.block, returned_value);
        } else {
            switch (node->if_stat.else_type) {
            case ElseTypeBlock:
                block_run(interpreter, &if_scope, node->if_stat.else_block, returned_value);
                break;
            case ElseTypeNode:
                interpreter_interpret_node(interpreter, &if_scope, node->if_stat.else_node, returned_value);
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
        const bool can_break_old = interpreter->can_break;
        interpreter->can_break = true;

        scope_construct(&loop_scope, scope);

        switch (node->loop.type) {
        case LoopWhile: {
            RaelValue condition;

            while (value_as_bool((condition = expr_eval(interpreter, &loop_scope, node->loop.while_condition, true)))) {
                value_dereference(condition);
                block_run(interpreter, &loop_scope, node->loop.block, returned_value);

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
            RaelValue iterator = expr_eval(interpreter, scope, node->loop.iterate.expr, true);
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
                block_run(interpreter, &loop_scope, node->loop.block, returned_value);

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
                block_run(interpreter, &loop_scope, node->loop.block, returned_value);

                if (interpreter->interrupt == ProgramInterruptBreak) {
                    interpreter->interrupt = ProgramInterruptNone;
                    break;
                } else if (interpreter->interrupt == ProgramInterruptReturn) {
                    break;
                }
            }
            break;
        default:
            assert(0);
        }

        interpreter->can_break = can_break_old;
        scope_dealloc(&loop_scope);
        break;
    }
    case NodeTypePureExpr:
        // dereference result right after evaluation
        value_dereference(expr_eval(interpreter, scope, node->pure, true));
        break;
    case NodeTypeReturn:
        if (returned_value) {
            // if you can set a return value
            if (node->return_value) {
                *returned_value = expr_eval(interpreter, scope, node->return_value, false);
            } else {
                *returned_value = value_create(ValueTypeVoid);
            }
        } else {
            rael_error(node->state, "'^' outside a routine is not permitted");
        }

        interpreter->interrupt = ProgramInterruptReturn;
        break;
    case NodeTypeBreak:
        if (!interpreter->can_break)
            rael_error(node->state, "';' has to be inside a loop");

        interpreter->interrupt = ProgramInterruptBreak;
        break;
    case NodeTypeCatch: {
        RaelValue caught_value = expr_eval(interpreter, scope, node->catch.catch_expr, false);

        // handle blame
        if (caught_value->type == ValueTypeBlame) {
            struct Scope catch_scope;
            scope_construct(&catch_scope, scope);
            block_run(interpreter, &catch_scope, node->catch.handle_block, returned_value);
        }

        value_dereference(caught_value);

        break;
    }
    default:
        assert(0);
    }
}

void interpret(struct Node **instructions) {
    struct Node *node;
    struct Interpreter interp = {
        .instructions = instructions,
        .interrupt = ProgramInterruptNone,
        .can_break = false,
    };

    scope_construct(&interp.scope, NULL);
    for (interp.idx = 0; (node = interp.instructions[interp.idx]); ++interp.idx) {
        interpreter_interpret_node(&interp, &interp.scope, node, NULL);
    }
    scope_dealloc(&interp.scope);
    for (interp.idx = 0; (node = interp.instructions[interp.idx]); ++interp.idx) {
        node_delete(node);
    }
    free(interp.instructions);
}
