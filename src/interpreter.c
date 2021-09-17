#include "scope.h"
#include "number.h"
#include "value.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct Interpreter {
    struct Node **instructions;
    size_t idx;
    struct Scope scope;
};

static bool interpreter_interpret_node(struct Scope *scope, struct Node* const node, RaelValue *returned_value);
static RaelValue expr_eval(struct Scope *scope, struct Expr* const expr);
static void value_log(RaelValue value);

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

RaelValue value_create(enum ValueType type) {
    RaelValue value = malloc(sizeof(struct RaelValue));
    value->type = type;
    value->reference_count = 1;
    return value;
}

void value_dereference(RaelValue value) {
    --value->reference_count;
    if (value->reference_count == 0) {
        switch (value->type) {
        case ValueTypeRoutine:
            break;
        case ValueTypeStack:
            for (size_t i = 0; i < value->as_stack.length; ++i) {
                value_dereference(value->as_stack.values[i]);
            }
            free(value->as_stack.values);
            break;
        case ValueTypeString:
            if (value->as_string.length)
                free(value->as_string.value);
            break;
        default:
            break;
        }
        free(value);
    }
}

static RaelValue value_eval(struct Scope *scope, struct ASTValue value) {
    RaelValue out_value = value_create(value.type);

    switch (value.type) {
    case ValueTypeNumber:
        out_value->as_number = value.as_number;
        break;
    case ValueTypeString:
        // duplicate string
        out_value->as_string.length = value.as_string.length;
        out_value->as_string.value = malloc(value.as_string.length * sizeof(char));
        strncpy(out_value->as_string.value, value.as_string.value, value.as_string.length);
        break;
    case ValueTypeRoutine:
        out_value->as_routine = value.as_routine;
        out_value->as_routine.scope = scope;
        break;
    case ValueTypeStack:
        out_value->as_stack = (struct RaelStackValue) {
            .length = value.as_stack.length,
            .allocated = value.as_stack.length,
            .values = malloc(value.as_stack.length * sizeof(struct RaelValue*))
        };
        for (size_t i = 0; i < value.as_stack.length; ++i) {
            out_value->as_stack.values[i] = expr_eval(scope, value.as_stack.entries[i]);
        }
        break;
    case ValueTypeVoid:
        break;
    default:
        assert(0);
    }

    ++out_value->reference_count;
    return out_value;
}

static void value_verify_is_number_int(struct State number_state, RaelValue number) {
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

    value_verify_is_number_int(expr->rhs->state, rhs);

    if ((size_t)rhs->as_number.as_int >= lhs->as_stack.length)
        rael_error(expr->rhs->state, "Index too big");

    value_dereference(lhs->as_stack.values[rhs->as_number.as_int]);
    lhs->as_stack.values[rhs->as_number.as_int] = value;
    value_dereference(lhs);
    value_dereference(rhs);
}

static RaelValue value_at(struct Scope *scope, struct Expr *expr) {
    RaelValue lhs, rhs, value;

    assert(expr->type == ExprTypeAt);
    lhs = expr_eval(scope, expr->lhs);
    rhs = expr_eval(scope, expr->rhs);

    if (lhs->type == ValueTypeStack) {
        value_verify_is_number_int(expr->rhs->state, rhs);
        if ((size_t)rhs->as_number.as_int >= lhs->as_stack.length)
            rael_error(expr->rhs->state, "Index too big");
        value = lhs->as_stack.values[rhs->as_number.as_int];
        ++value->reference_count;
    } else if (lhs->type == ValueTypeString) {
        value_verify_is_number_int(expr->rhs->state, rhs);
        if ((size_t)rhs->as_number.as_int >= lhs->as_string.length)
            rael_error(expr->rhs->state, "Index too big");
        value = value_create(ValueTypeString);
        value->as_string.length = 1;
        value->as_string.value = malloc(1 * sizeof(char));
        value->as_string.value[0] = lhs->as_string.value[rhs->as_number.as_int];
    } else {
        rael_error(expr->lhs->state, "Expected string or stack on the left of 'at'");
    }

    value_dereference(lhs);
    value_dereference(rhs);

    return value;
}

static RaelValue routine_call_eval(struct Scope *scope, struct RoutineCallExpr call, struct State state) {
    struct Scope routine_scope;
    RaelValue maybe_routine = scope_get(scope, call.routine_name);

    if (maybe_routine->type != ValueTypeRoutine) {
        value_dereference(maybe_routine);
        rael_error(state, "Call not possible on non-routine");
    }

    scope_construct(&routine_scope, maybe_routine->as_routine.scope);

    if (maybe_routine->as_routine.amount_parameters != call.amount_arguments)
        rael_error(state, "Arguments don't match parameters");

    // set parameters as variables
    for (size_t i = 0; i < maybe_routine->as_routine.amount_parameters; ++i) {
        scope_set(&routine_scope,
                maybe_routine->as_routine.parameters[i],
                expr_eval(scope, call.arguments[i]));
    }

    for (struct Node **node = maybe_routine->as_routine.block; *node; ++node) {
        RaelValue return_value;
        // if got a return statement, break
        if ((interpreter_interpret_node(&routine_scope, *node, &return_value))) {
            scope_dealloc(&routine_scope);
            return return_value;
        }
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
    case ExprTypeEquals:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);

        value = value_create(ValueTypeNumber);
        value->as_number.is_float = false;

        if (lhs->type == ValueTypeNumber && rhs->type == ValueTypeNumber) {
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
    case ExprTypeRoutineCall: {
        return routine_call_eval(scope, expr->as_call, expr->state);
    }
    case ExprTypeAt:
        return value_at(scope, expr);
    case ExprTypeRedirect:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);

        if (lhs->type != ValueTypeStack) {
            rael_error(expr->lhs->state, "Expected a stack value");
        }
        if (lhs->as_stack.length == lhs->as_stack.allocated) {
            lhs->as_stack.values = realloc(lhs->as_stack.values, (lhs->as_stack.allocated += 8) * sizeof(struct RaelValue));
        }

        // no need to dereference a *used* value
        lhs->as_stack.values[lhs->as_stack.length++] = rhs;
        value_dereference(lhs);

        // because you return a reference to the pushed value
        ++rhs->reference_count;

        return rhs;
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
        // %.*s gives some warning when using size_t (it expects ints)
        putchar('"');
        for (size_t i = 0; i < value->as_string.length; ++i)
            putchar(value->as_string.value[i]);
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
    default:
        printf("%d\n", value->type);
        exit(0);
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
    default:
        assert(0);
    }
}

/*
    returns true if got a return statement
*/
static bool interpreter_interpret_node(struct Scope *scope, struct Node* const node, RaelValue *returned_value) {
    bool had_return = false;

    switch (node->type) {
    case NodeTypeLog: {
        RaelValue value;
        value_log((value = expr_eval(scope, node->log_values[0])));
        value_dereference(value); // dereference
        for (size_t i = 1; node->log_values[i]; ++i) {
            printf(" ");
            value_log((value = expr_eval(scope, node->log_values[i])));
            value_dereference(value); // dereference
        }
        printf("\n");
        break;
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
            for (size_t i = 0; node->if_stat.block[i]; ++i) {
                if (interpreter_interpret_node(&if_scope, node->if_stat.block[i], returned_value)) {
                    had_return = true;
                    break;
                }
            }
        } else {
            if (node->if_stat.else_block) {
                for (size_t i = 0; node->if_stat.else_block[i]; ++i) {
                    if (interpreter_interpret_node(&if_scope, node->if_stat.else_block[i], returned_value)) {
                        had_return = true;
                        break;
                    }
                }
            }
        }
        scope_dealloc(&if_scope);
        break;
    }
    case NodeTypeLoop: {
        struct Scope loop_scope;
        RaelValue condition;
        scope_construct(&loop_scope, scope);

        while (value_as_bool((condition = expr_eval(scope, node->if_stat.condition)))) {
            value_dereference(condition);
            for (size_t i = 0; node->if_stat.block[i]; ++i) {
                if (interpreter_interpret_node(scope, node->if_stat.block[i], returned_value)) {
                    had_return = true;
                    break;
                }
            }
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
        had_return = true;
        break;
    default:
        assert(0);
    }

    return had_return;
}

void interpret(struct Node **instructions) {
    struct Node *node;
    struct Interpreter interp = {
        .instructions = instructions
    };

    scope_construct(&interp.scope, NULL);
    for (interp.idx = 0; (node = interp.instructions[interp.idx]); ++interp.idx) {
        interpreter_interpret_node(&interp.scope, node, NULL);
    }
    scope_dealloc(&interp.scope);
    // for (interp.idx = 0; (node = interp.instructions[interp.idx]); ++interp.idx) {
    //     node_delete(node);
    // }
    // free(interp.instructions);
}
