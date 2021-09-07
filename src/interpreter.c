#include "interpreter.h"
#include "scope.h"
#include "number.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


static void interpreter_interpret_node(struct Scope *scope, struct Node* const node);
static struct RaelValue expr_eval(struct Scope *scope, struct Expr* const expr);

void runtime_error(const char* const error_message) {
    printf("RuntimeError: %s.\n", error_message);
    exit(1);
}

static struct RaelValue value_eval(struct Scope *scope, struct ASTValue value) {
    struct RaelValue out_value = {
        .type = value.type
    };
    switch (value.type) {
    case ValueTypeNumber:
        out_value.as.number = value.as.number;
        break;
    case ValueTypeString:
        // FIXME: optimise this (already parse as a RaelString)
        out_value.as.string.value = value.as.string;
        out_value.as.string.length = strlen(value.as.string);
        break;
    case ValueTypeRoutine:
        out_value.as.routine = value.as.routine;
        break;
    case ValueTypeStack:
        out_value.as.stack = (struct RaelStackValue) {
            .length = value.as.stack.length,
            .allocated = value.as.stack.length,
            .values = malloc(value.as.stack.length * sizeof(struct RaelValue))
        };
        for (size_t i = 0; i < value.as.stack.length; ++i) {
            out_value.as.stack.values[i] = expr_eval(scope, value.as.stack.entries[i]);
        }
        break;
    case ValueTypeVoid:
        break;
    default:
        assert(0);
    }
    return out_value;
}

static struct RaelValue expr_eval(struct Scope *scope, struct Expr* const expr) {
    struct RaelValue lhs, rhs, value;
    switch (expr->type) {
    case ExprTypeValue:
        return value_eval(scope, *expr->as.value);
    case ExprTypeKey:
        return scope_get(scope, expr->as.key);
    case ExprTypeAdd:
        lhs = expr_eval(scope, expr->as.binary.lhs);
        rhs = expr_eval(scope, expr->as.binary.rhs);
        if (lhs.type == ValueTypeNumber && rhs.type == ValueTypeNumber) {
            value.type = ValueTypeNumber;
            value.as.number = number_add(lhs.as.number, rhs.as.number);
        } else if (lhs.type == ValueTypeString && rhs.type == ValueTypeString) {
            struct RaelStringValue string;
            // set length to the sum of added strings' lengths
            string.length = lhs.as.string.length + rhs.as.string.length;
            // allocate the length + 1 for null termination and copy strings into it
            string.value = malloc((string.length + 1) * sizeof(char));
            strcpy(string.value, lhs.as.string.value);
            strcpy(string.value + lhs.as.string.length, rhs.as.string.value);
            string.value[string.length] = '\0';
            value.type = ValueTypeString;
            value.as.string = string;
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
            if (lhs.as.string.value == rhs.as.string.value) {
                value.as.number.as._int = 1;
            } else {
                if (lhs.as.string.length == rhs.as.string.length)
                    value.as.number.as._int = strcmp(lhs.as.string.value, rhs.as.string.value) == 0;
                else
                    value.as.number.as._int = 0; // if lengths don't match, the strings don't match
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
        struct RaelValue maybe_routine = scope_get(scope, expr->as.call.routine_name);
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
        // TODO: add return statement and put the return value here
        value.type = ValueTypeVoid;
        return value;
    }
    default:
        assert(0);
    }
}

static void value_log_as_original(struct RaelValue value) {
    switch (value.type) {
    case ValueTypeNumber:
        if (value.as.number.is_float) {
            printf("%f", value.as.number.as._float);
        } else {
            printf("%d", value.as.number.as._int);
        }
        break;
    case ValueTypeString:
        // %.*s gives some warning when using size_t (it expects ints)
        putchar('"');
        for (size_t i = 0; i < value.as.string.length; ++i)
            putchar(value.as.string.value[i]);
        putchar('"');
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
    case ValueTypeStack:
        printf("{ ");
        for (size_t i = 0; i < value.as.stack.length; ++i) {
            if (i > 0)
                printf(", ");
            value_log_as_original(value.as.stack.values[i]);
        }
        printf(" }");
        break;
    default:
        assert(0);
    }
}

static void value_log(struct RaelValue value) {
    // only strings are printed differently when `log`ed then inside an array
    switch (value.type) {
    case ValueTypeString:
        // %.*s gives some warning when using size_t (it expects ints)
        for (size_t i = 0; i < value.as.string.length; ++i)
            putchar(value.as.string.value[i]);
        break;
    case ValueTypeRoutine:
    case ValueTypeNumber:
    case ValueTypeVoid:
    case ValueTypeStack:
        value_log_as_original(value);
        break;
    default:
        assert(0);
    }
    printf("\n");
}

/* is the value booleanly true? like Python's bool() operator */
static bool value_as_bool(struct RaelValue const value) {
    switch (value.type) {
    case ValueTypeVoid:
        return false;
    case ValueTypeString:
        return value.as.string.length != 0;
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
        struct RaelValue val;
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
    case NodeTypePureExpr:
        expr_eval(scope, node->value.pure);
        break;
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
