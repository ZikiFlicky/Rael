#include "scope.h"
#include "number.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct Interpreter {
    struct Node **instructions;
    size_t idx;
    struct Scope scope;
};

static bool interpreter_interpret_node(struct Scope *scope, struct Node* const node, struct RaelValue *returned_value);
static struct RaelValue expr_eval(struct Scope *scope, struct Expr* const expr);
static void value_log(struct RaelValue value);

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

static struct RaelValue value_eval(struct Scope *scope, struct ASTValue value) {
    struct RaelValue out_value = {
        .type = value.type
    };
    switch (value.type) {
    case ValueTypeNumber:
        out_value.as_number = value.as_number;
        break;
    case ValueTypeString:
        out_value.as_string = value.as_string;
        break;
    case ValueTypeRoutine:
        out_value.as_routine = value.as_routine;
        break;
    case ValueTypeStack:
        out_value.as_stack = malloc(sizeof(struct RaelStackValue));
        *out_value.as_stack = (struct RaelStackValue) {
            .length = value.as_stack.length,
            .allocated = value.as_stack.length,
            .values = malloc(value.as_stack.length * sizeof(struct RaelValue))
        };
        for (size_t i = 0; i < value.as_stack.length; ++i) {
            out_value.as_stack->values[i] = expr_eval(scope, value.as_stack.entries[i]);
        }
        break;
    case ValueTypeVoid:
        break;
    default:
        assert(0);
    }
    return out_value;
}

static struct RaelValue *stack_value_at(struct Scope *scope, struct Expr *expr) {
    struct RaelValue lhs, rhs;

    assert(expr->type == ExprTypeAt);
    lhs = expr_eval(scope, expr->lhs);
    rhs = expr_eval(scope, expr->rhs);

    if (lhs.type != ValueTypeStack) {
        rael_error(expr->lhs->state, "Expected stack on the left of 'at'");
    }
    if (rhs.type != ValueTypeNumber) {
        rael_error(expr->rhs->state, "Expected number");
    }
    if (rhs.as_number.is_float) {
        rael_error(expr->rhs->state, "Float index is not allowed");
    }
    if (rhs.as_number.as_int < 0) {
        rael_error(expr->rhs->state, "A negative index is not allowed");
    }
    if ((size_t)rhs.as_number.as_int >= lhs.as_stack->length) {
        rael_error(expr->rhs->state, "Index too big");
    }
    return &lhs.as_stack->values[rhs.as_number.as_int];
}

static struct RaelValue expr_eval(struct Scope *scope, struct Expr* const expr) {
    struct RaelValue lhs, rhs, single, value;

    switch (expr->type) {
    case ExprTypeValue:
        return value_eval(scope, *expr->as_value);
    case ExprTypeKey:
        return scope_get(scope, expr->as_key);
    case ExprTypeAdd:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            value.as_number = number_add(lhs.as_number, rhs.as_number);
        } else if (lhs.type == ValueTypeString && rhs.type == ValueTypeString) {
            struct RaelStringValue string;
            string.length = lhs.as_string.length + rhs.as_string.length;
            string.value = malloc(string.length * sizeof(char));
            strncpy(string.value, lhs.as_string.value, lhs.as_string.length);
            strncpy(string.value + lhs.as_string.length, rhs.as_string.value, rhs.as_string.length);
            value.type = ValueTypeString;
            value.as_string = string;
        } else {
            rael_error(expr->state, "Invalid operation (+) on types");
        }
        return value;
    case ExprTypeSub:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            value.as_number = number_sub(lhs.as_number, rhs.as_number);
        } else {
            rael_error(expr->state, "Invalid operation (-) on types");
        }
        return value;
    case ExprTypeMul:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            value.as_number = number_mul(lhs.as_number, rhs.as_number);
        } else {
            rael_error(expr->state, "Invalid operation (*) on types");
        }
        return value;
    case ExprTypeDiv:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            value.as_number = number_div(expr->state, lhs.as_number, rhs.as_number);
        } else {
            rael_error(expr->state, "Invalid operation (/) on types");
        }
        return value;
    case ExprTypeEquals:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);
        value.type = ValueTypeNumber;
        value.as_number.is_float = false;
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.as_number = number_eq(lhs.as_number, rhs.as_number);
        } else if (lhs.type == ValueTypeString && rhs.type == ValueTypeString) {
            // if they have the same pointer, they must be equal
            if (lhs.as_string.value == rhs.as_string.value) {
                value.as_number.as_int = 1;
            } else {
                if (lhs.as_string.length == rhs.as_string.length)
                    value.as_number.as_int = strcmp(lhs.as_string.value, rhs.as_string.value) == 0;
                else
                    value.as_number.as_int = 0; // if lengths don't match, the strings don't match
            }
        } else {
            value.as_number.as_int = 0;
        }
        return value;
    case ExprTypeSmallerThen:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            value.as_number = number_smaller(lhs.as_number, rhs.as_number);
        } else {
            rael_error(expr->state, "Invalid operation (<) on types");
        }
        return value;
    case ExprTypeBiggerThen:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            value.as_number = number_bigger(lhs.as_number, rhs.as_number);
        } else {
            rael_error(expr->state, "Invalid operation (>) on types");
        }
        return value;
    case ExprTypeRoutineCall: {
        struct RaelValue maybe_routine = scope_get(scope, expr->as_call.routine_name);
        struct Scope routine_scope;

        value.type = ValueTypeVoid;

        // FIXME: this feels very hackish
        scope_construct(&routine_scope, scope_get_key_scope(scope, expr->as_call.routine_name));

        maybe_routine = scope_get(&routine_scope, expr->as_call.routine_name);

        if (maybe_routine.type != ValueTypeRoutine)
            rael_error(expr->state, "Call not possible on non-routine");

        // verify the amount of arguments equals the amount of parameters
        if (maybe_routine.as_routine.amount_parameters != expr->as_call.amount_arguments)
            rael_error(expr->state, "Arguments don't match parameters");

        // set parameters as variables
        for (size_t i = 0; i < maybe_routine.as_routine.amount_parameters; ++i) {
            scope_set(&routine_scope,
                    maybe_routine.as_routine.parameters[i],
                    expr_eval(scope, expr->as_call.arguments[i]));
        }

        for (struct Node **node = maybe_routine.as_routine.block; *node; ++node) {
            // if got a return statement, break
            if (interpreter_interpret_node(&routine_scope, *node, &value))
                break;
        }

        scope_dealloc(&routine_scope);
        return value;
    }
    case ExprTypeAt:
        return *stack_value_at(scope, expr);
    case ExprTypeRedirect:
        lhs = expr_eval(scope, expr->lhs);
        rhs = expr_eval(scope, expr->rhs);
        if (lhs.type != ValueTypeStack) {
            rael_error(expr->lhs->state, "Expected a stack value");
        }
        if (lhs.as_stack->length == lhs.as_stack->allocated) {
            lhs.as_stack->values = realloc(lhs.as_stack->values, (lhs.as_stack->allocated += 8) * sizeof(struct RaelValue));
        }
        lhs.as_stack->values[lhs.as_stack->length++] = rhs;
        return rhs;
    case ExprTypeSizeof:
        value.type = ValueTypeNumber;
        value.as_number.is_float = false;
        single = expr_eval(scope, expr->as_single);
        switch (single.type) {
        case ValueTypeStack:
            // FIXME: not exactly safe
            value.as_number.as_int = (int)single.as_stack->length;
            break;
        case ValueTypeString:
            // FIXME: also unsafe
            value.as_number.as_int = (int)single.as_string.length;
            break;
        default:
            rael_error(expr->as_single->state, "Unsupported type for 'sizeof' operation");
            break;
        }
        return value;
    default:
        assert(0);
    }
}

static void value_log_as_original(struct RaelValue value) {
    switch (value.type) {
    case ValueTypeNumber:
        if (value.as_number.is_float) {
            printf("%f", value.as_number.as_float);
        } else {
            printf("%d", value.as_number.as_int);
        }
        break;
    case ValueTypeString:
        // %.*s gives some warning when using size_t (it expects ints)
        putchar('"');
        for (size_t i = 0; i < value.as_string.length; ++i)
            putchar(value.as_string.value[i]);
        putchar('"');
        break;
    case ValueTypeVoid:
        printf("Void");
        break;
    case ValueTypeRoutine:
        printf("routine(");
        if (value.as_routine.amount_parameters > 0) {
            printf(":%s", value.as_routine.parameters[0]);
            for (size_t i = 1; i < value.as_routine.amount_parameters; ++i)
                printf(", :%s", value.as_routine.parameters[i]);
        }
        printf(")");
        break;
    case ValueTypeStack:
        printf("{ ");
        for (size_t i = 0; i < value.as_stack->length; ++i) {
            if (i > 0)
                printf(", ");
            value_log_as_original(value.as_stack->values[i]);
        }
        printf(" }");
        break;
    default:
        assert(0);
    }
}

static void value_log(struct RaelValue value) {
    // only strings are printed differently when `log`ed then inside a stack
    switch (value.type) {
    case ValueTypeString:
        for (size_t i = 0; i < value.as_string.length; ++i)
            putchar(value.as_string.value[i]);
        break;
    default:
        value_log_as_original(value);
    }
}

/* is the value booleanly true? like Python's bool() operator */
static bool value_as_bool(struct RaelValue const value) {
    switch (value.type) {
    case ValueTypeVoid:
        return false;
    case ValueTypeString:
        return value.as_string.length != 0;
    case ValueTypeNumber:
        if (value.as_number.is_float)
            return value.as_number.as_float != 0.0f;
        else
            return value.as_number.as_int != 0;
    case ValueTypeRoutine:
        return true;
    default:
        assert(0);
    }
}

/*
    returns true if got a return statement
*/
static bool interpreter_interpret_node(struct Scope *scope, struct Node* const node, struct RaelValue *returned_value) {
    bool had_return = false;

    switch (node->type) {
    case NodeTypeLog:
        value_log(expr_eval(scope, node->log_values[0]));
        for (size_t i = 1; node->log_values[i]; ++i) {
            printf(" ");
            value_log(expr_eval(scope, node->log_values[i]));
        }
        printf("\n");
        break;
    case NodeTypeSet:
        switch (node->set.set_type) {
        case SetTypeAtExpr: {
            *stack_value_at(scope, node->set.as_at_stat) = expr_eval(scope, node->set.expr);
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
        struct RaelValue val;

        scope_construct(&if_scope, scope);
        if (value_as_bool((val = expr_eval(scope, node->if_stat.condition)))) {
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
        scope_construct(&loop_scope, scope);

        while (value_as_bool(expr_eval(scope, node->if_stat.condition))) {
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
        expr_eval(scope, node->pure);
        break;
    case NodeTypeReturn:
        if (returned_value) {
            if (node->return_value) {
                *returned_value = expr_eval(scope, node->return_value);
            } else {
                returned_value->type = ValueTypeVoid;
            }
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
        if (interpreter_interpret_node(&interp.scope, node, NULL))
            rael_error(node->state, "'^' outside a routine is not permitted");
    }
}
