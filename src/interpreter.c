#include "interpreter.h"
#include "varmap.h"
#include "number.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>


static void interpreter_interpret_node(struct Scope *scope, struct Node* const node);

void runtime_error(const char* const error_message) {
    printf("RuntimeError: %s.\n", error_message);
    exit(1);
}

static struct Value expr_eval(struct Scope *scope, struct Expr* const expr) {
    struct Value lhs, rhs, value;
    switch (expr->type) {
    case ExprTypeValue:
        return *expr->as.value;
    case ExprTypeKey:
        return scope_get(scope, expr->as.key);
    case ExprTypeAdd:
        lhs = expr_eval(scope, expr->as.binary.lhs);
        rhs = expr_eval(scope, expr->as.binary.rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            value.as.number = number_add(lhs.as.number, rhs.as.number);
        } else if (lhs.type == ValueTypeString && rhs.type == ValueTypeString) {
            // FIXME: optimise this
            char *new_string;
            size_t s_lhs = strlen(lhs.as.string), s_rhs = strlen(rhs.as.string);
            new_string = malloc((s_lhs + s_rhs + 1) * sizeof(char));
            strcpy(new_string, lhs.as.string);
            strcpy(new_string + s_lhs, rhs.as.string);
            new_string[s_lhs + s_rhs] = '\0';
            value.type = ValueTypeString;
            value.as.string = new_string;
        } else {
            runtime_error("Invalid operation (+) on types");
        }
        return value;
    case ExprTypeSub:
        lhs = expr_eval(scope, expr->as.binary.lhs);
        rhs = expr_eval(scope, expr->as.binary.rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            value.as.number = number_sub(lhs.as.number, rhs.as.number);
        } else {
            runtime_error("Invalid operation (-) on types");
        }
        return value;
    case ExprTypeMul:
        lhs = expr_eval(scope, expr->as.binary.lhs);
        rhs = expr_eval(scope, expr->as.binary.rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            value.as.number = number_mul(lhs.as.number, rhs.as.number);
        } else {
            runtime_error("Invalid operation (*) on types");
        }
        return value;
    case ExprTypeDiv:
        lhs = expr_eval(scope, expr->as.binary.lhs);
        rhs = expr_eval(scope, expr->as.binary.rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            if (rhs.as.number.as._float == 0.f) {
                runtime_error("Haha! no. you are not dividing by zero. Have a great day!");
            }
            value.as.number = number_div(lhs.as.number, rhs.as.number);
        } else {
            runtime_error("Invalid operation (/) on types");
        }
        return value;
    case ExprTypeEquals:
        lhs = expr_eval(scope, expr->as.binary.lhs);
        rhs = expr_eval(scope, expr->as.binary.rhs);
        value.type = ValueTypeNumber;
        value.as.number.is_float = false;
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.as.number = number_eq(lhs.as.number, rhs.as.number);
        } else if (lhs.type == ValueTypeString && rhs.type == ValueTypeString) {
            // if they have the same pointer, they must be equal
            if (lhs.as.string == rhs.as.string) {
                value.as.number.as._int = 1;
            } else {
                value.as.number.as._int = strcmp(lhs.as.string, rhs.as.string) == 0;
            }
        } else {
            value.as.number.as._int = 0;
        }
        return value;
    case ExprTypeSmallerThen:
        lhs = expr_eval(scope, expr->as.binary.lhs);
        rhs = expr_eval(scope, expr->as.binary.rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            value.as.number = number_smaller(lhs.as.number, rhs.as.number);
        } else {
            runtime_error("Invalid operation (<) on types");
        }
        return value;
    case ExprTypeBiggerThen:
        lhs = expr_eval(scope, expr->as.binary.lhs);
        rhs = expr_eval(scope, expr->as.binary.rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            value.as.number = number_bigger(lhs.as.number, rhs.as.number);
        } else {
            runtime_error("Invalid operation (>) on types");
        }
        return value;
    case ExprTypeRoutineCall: {
        struct Value maybe_routine = scope_get(scope, expr->as.call.routine_name);
        struct Scope routine_scope;
        size_t i;
        if (maybe_routine.type != ValueTypeRoutine) {
            runtime_error("Call not possible on non-routine");
        }
        scope_construct(&routine_scope, scope);
        // verify the amount of arguments equal the amount of parameters
        if (maybe_routine.as.routine.amount_parameters != expr->as.call.amount_arguments)
            runtime_error("Arguments don't match parameters");
        // set parameters
        for (i = 0; i < maybe_routine.as.routine.amount_parameters; ++i) {
            scope_set(&routine_scope,
                    maybe_routine.as.routine.parameters[i],
                    expr_eval(scope, expr->as.call.arguments[i]));
        }
        for (struct Node **node = maybe_routine.as.routine.block; *node; ++node) {
            interpreter_interpret_node(&routine_scope, *node);
        }
        value.type = ValueTypeString;
        value.as.string = expr->as.call.routine_name;
        return value;
    }
    default:
        assert(0);
    }
}

static void value_log(struct Value value) {
    switch (value.type) {
    case ValueTypeNumber:
        if (value.as.number.is_float) {
            printf("%f", value.as.number.as._float);
        } else {
            printf("%d", value.as.number.as._int);
        }
        break;
    case ValueTypeString:
        printf("%s", value.as.string);
        break;
    case ValueTypeVoid:
        printf("Void");
        break;
    case ValueTypeRoutine:
        printf("routine(");
        if (value.as.routine.amount_parameters > 0) {
            printf(":%s", value.as.routine.parameters[0]);
            for (size_t i = 1; i < value.as.routine.amount_parameters; ++i)
                printf(", :%s", value.as.routine.parameters[i]);
        }
        printf(")");
        break;
    default:
        assert(0);
    }
    printf("\n");
}

/* is the value booleanly true? like Python's bool */
static bool value_as_bool(struct Value const value) {
    switch (value.type) {
    case ValueTypeVoid:
        return false;
    case ValueTypeString:
        return value.as.string[0] != '\0';
    case ValueTypeNumber:
        if (value.as.number.is_float)
            return value.as.number.as._float != 0.0f;
        else
            return value.as.number.as._int != 0;
    case ValueTypeRoutine:
        return true;
    default:
        assert(0);
    }
}

static void interpreter_interpret_node(struct Scope *scope, struct Node* const node) {
    switch (node->type) {
    case NodeTypeLog:
        value_log(expr_eval(scope, node->value.log_value));
        break;
    case NodeTypeSet: {
        scope_set(scope, node->value.set.key, expr_eval(scope, node->value.set.expr));
        break;
    }
    case NodeTypeIf: {
        struct Scope if_scope;
        struct Value val;
        scope_construct(&if_scope, scope);
        if (value_as_bool((val = expr_eval(scope, node->value.if_stat.condition)))) {
            for (size_t i = 0; node->value.if_stat.block[i]; ++i) {
                interpreter_interpret_node(&if_scope, node->value.if_stat.block[i]);
            }
        } else {
            if (node->value.if_stat.else_block) {
                for (size_t i = 0; node->value.if_stat.else_block[i]; ++i) {
                    interpreter_interpret_node(&if_scope, node->value.if_stat.else_block[i]);
                }
            }
        }
        scope_dealloc(&if_scope);
        break;
    }
    case NodeTypeLoop: {
        struct Scope loop_scope;
        scope_construct(&loop_scope, scope);
        while (value_as_bool(expr_eval(scope, node->value.if_stat.condition))) {
            for (size_t i = 0; node->value.if_stat.block[i]; ++i) {
                interpreter_interpret_node(scope, node->value.if_stat.block[i]);
            }
        }
        scope_dealloc(&loop_scope);
        break;
    }
    default:
        assert(0);
    }
}

void interpret(struct Node **instructions) {
    struct Node *node;
    struct Interpreter interp = {
        .instructions = instructions
    };
    scope_construct(&interp.scope, NULL);
    for (interp.idx = 0; (node = interp.instructions[interp.idx]); ++interp.idx) {
        interpreter_interpret_node(&interp.scope, node);
    }
}
