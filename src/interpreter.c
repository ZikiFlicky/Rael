#include "scope.h"
#include "number.h"
#include "value.h"
#include "common.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>

typedef RaelValue (*RaelBinaryOperationFunction)(RaelValue, RaelValue);

enum ProgramInterrupt {
    ProgramInterruptNone,
    ProgramInterruptBreak,
    ProgramInterruptReturn,
    ProgramInterruptSkip
};

struct Interpreter {
    char *stream_base;
    char* const filename;
    const bool stream_on_heap;
    struct Instruction **instructions;
    size_t idx;
    struct Scope *scope;
    enum ProgramInterrupt interrupt;
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

static struct RaelStringValue rael_readline(struct Interpreter* const interpreter, struct State state) {
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

    return (struct RaelStringValue) {
        .value = string,
        .length = allocated
    };
}

static RaelValue value_eval(struct Interpreter* const interpreter, struct ValueExpr value) {
    RaelValue out_value;

    switch (value.type) {
    case ValueTypeNumber:
        out_value = number_new(value.as_number);
        break;
    case ValueTypeString:
        out_value = value_create(ValueTypeString);
        // it's okay because strings are immutable
        out_value->as_string = value.as_string;
        out_value->as_string.type = StringTypePure;
        // flags not to deallocate string, because there is still a reference in the ast
        out_value->as_string.can_be_freed = false;
        break;
    case ValueTypeRoutine:
        out_value = value_create(ValueTypeRoutine);
        out_value->as_routine = value.as_routine;
        out_value->as_routine.scope = interpreter->scope;
        break;
    case ValueTypeStack: {
        size_t length = value.as_stack.amount_exprs;
        out_value = stack_new(length);
        for (size_t i = 0; i < length; ++i) {
            stack_push(out_value, expr_eval(interpreter, value.as_stack.exprs[i], true));
        }
        break;
    }
    case ValueTypeVoid:
        out_value = value_create(ValueTypeVoid);
        break;
    case ValueTypeType:
        out_value = value_create(ValueTypeType);
        out_value->as_type = value.as_type;
        break;
    default:
        RAEL_UNREACHABLE();
    }

    return out_value;
}

/* return a specific error if the RaelValue is not an int, and a NULL if it is an int */
static RaelValue value_verify_int(RaelValue number, struct State number_state) {
    RaelValue blame;
    if (number->type != ValueTypeNumber) {
        blame = RAEL_BLAME_FROM_RAWSTR_STATE("Expected a number", number_state);
    } else if (!number_is_whole(number->as_number)) {
        blame = RAEL_BLAME_FROM_RAWSTR_STATE("Float index is not allowed", number_state);
    } else {
        blame = NULL;
    }
    return blame;
}

/* return a specific error if the RaelValue is not an uint, and a NULL if it is a uint */
static RaelValue value_verify_uint(RaelValue number, struct State number_state) {
    // the checks for uint follow the same checks as an int does
    RaelValue blame = value_verify_int(number, number_state);
    // if there was an error, leave it as it is
    if (blame) {
        ;
    } else if (number_to_int(number->as_number) < 0) {
        blame = RAEL_BLAME_FROM_RAWSTR_STATE("A negative index is not allowed", number_state);
    } else {
        blame = NULL;
    }
    return blame;
}

static RaelValue eval_stack_set(struct Interpreter* const interpreter, struct Expr *at_expr, struct Expr *value_expr) {
    RaelValue stack, idx, value;

    assert(at_expr->type == ExprTypeAt);
    stack = expr_eval(interpreter, at_expr->lhs, true);
    if (stack->type != ValueTypeStack) {
        value_deref(stack);
        return RAEL_BLAME_FROM_RAWSTR_STATE("Expected stack on the left of 'at' when setting value", at_expr->lhs->state);
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
    if (!stack_set(stack, (size_t)number_to_int(idx->as_number), value)) {
        value_deref(stack);
        value_deref(idx);
        return RAEL_BLAME_FROM_RAWSTR_STATE("Index too big", at_expr->rhs->state);
    }
    value_deref(stack);
    value_deref(idx);
    value_ref(value);
    return value;
}

static RaelValue eval_at_operation_set(struct Interpreter* const interpreter, struct Expr *at_expr, struct Expr *value_expr,
                                    RaelBinaryOperationFunction operation, struct State operator_state) {
    RaelValue stack, idx, *value_ptr, result, value;

    assert(at_expr->type == ExprTypeAt);
    stack = expr_eval(interpreter, at_expr->lhs, true);
    if (stack->type != ValueTypeStack) {
        value_deref(stack);
        return RAEL_BLAME_FROM_RAWSTR_STATE("Expected a stack", at_expr->lhs->state);
    }

    idx = expr_eval(interpreter, at_expr->rhs, true);
    if ((result = value_verify_uint(idx, at_expr->rhs->state))) {
        value_deref(stack);
        value_deref(idx);
        return result;
    }
    value_ptr = stack_get_ptr(stack, (size_t)number_to_int(idx->as_number));

    value = expr_eval(interpreter, value_expr, true);
    // if the value exists
    if (value_ptr) {
        result = operation(*value_ptr, value);
        // if the operation failed, set the return value to be an error
        if (!result) {
            result = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Invalid operation between values (types don't match)");
        }
    } else {
        result = RAEL_BLAME_FROM_RAWSTR_STATE("Index too big", at_expr->rhs->state);
    }
    if (value_is_blame(result)) {
        blame_add_state(result, operator_state);
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

static RaelValue stack_at(struct Interpreter* const interpreter, RaelValue stack, struct Expr *at_expr) {
    RaelValue idx, out_value;

    assert(stack->type == ValueTypeStack);
    idx = expr_eval(interpreter, at_expr->rhs, true);

    switch (idx->type) {
    case ValueTypeNumber:
        // if idx is not a uint, return the error
        if ((out_value = value_verify_uint(idx, at_expr->rhs->state))) {
            value_deref(stack);
            value_deref(idx);
            return out_value;
        }
        out_value = stack_get(stack, (size_t)idx->as_number.as_int);
        // if is out of range
        if (!out_value) {
            value_deref(stack);
            value_deref(idx);
            interpreter_error(interpreter, at_expr->rhs->state, "Index too big");
        }
        break;
    case ValueTypeRange: {
        RaelInt start = idx->as_range.start,
                end = idx->as_range.end;
        size_t length = stack_get_length(stack);

        // make sure range numbers are positive, e.g -2 to 3 is not allowed
        if (start < 0 || end < 0) {
            value_deref(stack);
            value_deref(idx);
            interpreter_error(interpreter, at_expr->rhs->state, "Negative range numbers for stack slicing are not allowed");
        }

        // make sure range direction is not negative
        if (end < start) {
            value_deref(stack);
            value_deref(idx);
            interpreter_error(interpreter, at_expr->rhs->state, "Range end is smaller than its start when slicing a stack");
        }

        // make sure you are inside of the stack's boundaries
        if ((size_t)start > length || (size_t)end > length) {
            value_deref(stack);
            value_deref(idx);
            interpreter_error(interpreter, at_expr->rhs->state, "Stack slicing out of range");
        }

        out_value = stack_slice(stack, (size_t)start, (size_t)end);
        assert(out_value);
        break;
    }
    default:
        value_deref(stack);
        value_deref(idx);
        interpreter_error(interpreter, at_expr->rhs->state, "Expected number or range");
    }

    value_deref(stack); // lhs
    value_deref(idx); // rhs
    return out_value;
}

static RaelValue string_at(struct Interpreter* const interpreter, RaelValue string, struct Expr *at_expr) {
    RaelValue at_value, out_value;

    assert(string->type == ValueTypeString);
    at_value = expr_eval(interpreter, at_expr->rhs, true);

    switch (at_value->type) {
    case ValueTypeRange: {
        size_t string_length = string_get_length(string);
        // make sure range numbers are positive, -2 to -3 is not allowed
        if (at_value->as_range.start < 0 || at_value->as_range.end < 0) {
            value_deref(string);
            value_deref(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Expected non-negative range numbers");
        }
        // make sure range direction is not negative, e.g 4 to 2
        if (at_value->as_range.end < at_value->as_range.start) {
            value_deref(string);
            value_deref(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Negative range direction for substrings is not allowed");
        }
        // make sure you are inside of the string's boundaries
        if ((size_t)at_value->as_range.start > string_length ||
            (size_t)at_value->as_range.end > string_length) {
            value_deref(string);
            value_deref(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Substring out of range");
        }

        out_value = string_slice(string, (size_t)at_value->as_range.start, (size_t)at_value->as_range.end);
        break;
    }
    case ValueTypeNumber:
        // verify the value is a uint
        if (!(out_value = value_verify_uint(at_value, at_expr->rhs->state))) {
            out_value = value_get(string, (size_t)at_value->as_number.as_int);
            if (!out_value) {
                value_deref(string);
                value_deref(at_value);
                interpreter_error(interpreter, at_expr->rhs->state, "Index too big");
            }
        }
        break;
    default:
        interpreter_error(interpreter, at_expr->rhs->state, "Expected range or number");
    }

    value_deref(string);   // lhs
    value_deref(at_value); // rhs
    return out_value;
}

static RaelValue range_at(struct Interpreter* const interpreter, RaelValue range, struct Expr *at_expr) {
    RaelValue idx, out_value;

    assert(range->type == ValueTypeRange);
    // evaluate index
    idx = expr_eval(interpreter, at_expr->rhs, true);
    // if the value was not a uint, return an error
    if ((out_value = value_verify_uint(idx, at_expr->rhs->state))) {
        value_deref(idx);
        return out_value;
    }

    if (idx->as_number.as_int >= rael_int_abs(range->as_range.end - range->as_range.start)) {
        value_deref(range);
        value_deref(idx);
        interpreter_error(interpreter, at_expr->rhs->state, "Index too big");
    }

    out_value = value_get(range, idx->as_number.as_int);

    value_deref(range); // dereference lhs
    value_deref(idx);   // dereference rhs
    return out_value;
}

static RaelValue value_at(struct Interpreter* const interpreter, struct Expr *expr) {
    RaelValue lhs, value;

    assert(expr->type == ExprTypeAt);
    // evaluate the lhs of the at expression
    lhs = expr_eval(interpreter, expr->lhs, true);

    switch (lhs->type) {
    case ValueTypeStack:
        value = stack_at(interpreter, lhs, expr);
        break;
    case ValueTypeString:
        value = string_at(interpreter, lhs, expr);
        break;
    case ValueTypeRange:
        value = range_at(interpreter, lhs, expr);
        break;
    default: {
        // save state, dereference and error
        struct State state = expr->lhs->state;
        value_deref(lhs);
        interpreter_error(interpreter, state, "Expected string, stack or range on the left of 'at'");
    }
    }

    return value;
}

/*
    return value:
    0 -> okay
    1 -> too few arguments
    2 -> too many arguments
*/
static int routine_call_expr_eval(struct Interpreter *interpreter, RaelValue routine, struct CallExpr *call) {
    size_t amount_args, amount_params;
    struct Scope routine_scope, *prev_scope;
    assert(routine->type == ValueTypeRoutine);
    amount_params = routine->as_routine.amount_parameters;
    amount_args = call->arguments.amount_exprs;

    // set flags if the amount of arguments isn't equal to the amount of parameters
    if (amount_args < amount_params) {
        return 1;
    } else if (amount_args > amount_params) {
        return 2;
    }

    // create the routine scope
    scope_construct(&routine_scope, routine->as_routine.scope);

    // set parameters as variables
    for (size_t i = 0; i < amount_params; ++i) {
        scope_set_local(&routine_scope,
                        routine->as_routine.parameters[i],
                        expr_eval(interpreter, call->arguments.exprs[i], true),
                        false);
    }

    // store current scope and set it to the routine scope
    prev_scope = interpreter->scope;
    interpreter->scope = &routine_scope;
    // run the routine
    block_run(interpreter, routine->as_routine.block);
    if (interpreter->interrupt == ProgramInterruptReturn) {
        // had a return statement
        ;
    } else {
        // just got to the end of the function (which means no return value)
        interpreter->returned_value = value_create(ValueTypeVoid);
    }

    interpreter->interrupt = ProgramInterruptNone;
    // deallocate routine scope and switch scope to previous scope
    scope_dealloc(&routine_scope);
    interpreter->scope = prev_scope;
    return 0;
}

static int cfunc_call_expr_eval(struct Interpreter *interpreter, RaelValue cfunc, struct CallExpr *call, struct State state) {
    RaelArguments arguments;
    size_t amount_args, amount_params;
    assert(cfunc->type == ValueTypeCFunc);
    amount_params = cfunc->as_cfunc.amount_params;
    amount_args = call->arguments.amount_exprs;

    // set flags if the amount of arguments isn't equal to the amount of parameters
    if (amount_args < amount_params) {
        return 1;
    } else if (amount_args > amount_params) {
        return 2;
    }

    // initialize argument list and add arguments
    arguments_new(&arguments);
    for (size_t i = 0; i < amount_args; ++i) {
        arguments_add(&arguments, expr_eval(interpreter, call->arguments.exprs[i], true));
    }
    // minimize size of argument list (realloc to minimum size)
    arguments_finalize(&arguments);
    // set the interpreter return value to the return value of the function after execution
    interpreter->returned_value = cfunc_call(&cfunc->as_cfunc, &arguments, state);
    // verify the return value is not invalid
    assert(interpreter->returned_value != NULL);
    // delete argument list
    arguments_delete(&arguments);
    return 0;
}

static RaelValue call_expr_eval(struct Interpreter* const interpreter,
                                struct CallExpr call, struct State state) {
    RaelValue callable = expr_eval(interpreter, call.callable_expr, true);
    int err_state = 0;

    switch (callable->type) {
    case ValueTypeRoutine:
        err_state = routine_call_expr_eval(interpreter, callable, &call);
        break;
    case ValueTypeCFunc:
        err_state = cfunc_call_expr_eval(interpreter, callable, &call, state);
        break;
    default:
        value_deref(callable);
        interpreter_error(interpreter, state, "Call not possible on a non-callable");
    }
    // dereference the temporary callable
    value_deref(callable);
    // handle posible errors when calling the callable
    switch (err_state) {
    case 0:
        break;
    case 1:
        interpreter_error(interpreter, state, "Too few arguments");
        break;
    case 2:
        interpreter_error(interpreter, state, "Too many arguments");
        break;
    default:
        RAEL_UNREACHABLE();
    }
    return interpreter->returned_value;
}

static RaelValue value_cast(struct Interpreter* const interpreter, RaelValue value, enum ValueType cast_type, struct State value_state) {
    RaelValue casted;
    enum ValueType value_type = value->type;

    if (value_type == cast_type) {
        // add reference because it is returned
        value_ref(value);
        return value;
    }

    switch (cast_type) {
    case ValueTypeString:
        casted = value_create(ValueTypeString);
        switch (value->type) {
        case ValueTypeVoid:
            casted->as_string = (struct RaelStringValue) {
                .type = StringTypePure,
                .can_be_freed = true,
                .length = 4,
                .value = malloc(4 * sizeof(char))
            };
            // to avoid the warnings when strncpying
            memcpy(casted->as_string.value, "Void", 4 * sizeof(char));
            break;
        case ValueTypeNumber: {
            // FIXME: make float conversions more accurate
            bool is_negative;
            RaelInt decimal;
            RaelFloat fractional;
            size_t allocated, idx = 0;
            char *string = malloc((allocated = 10) * sizeof(char));
            size_t middle;

            if (value->as_number.is_float) {
                is_negative = value->as_number.as_float < 0;
                decimal = rael_int_abs(number_to_int(value->as_number));
                fractional = fmod(rael_float_abs(value->as_number.as_float), 1);
            } else {
                is_negative = value->as_number.as_int < 0;
                decimal = rael_int_abs(value->as_number.as_int);
                fractional = 0.0;
            }

            do {
                if (idx >= allocated)
                    string = realloc(string, (allocated += 10) * sizeof(char));
                string[idx++] = '0' + (decimal % 10);
                decimal = (decimal - decimal % 10) / 10;
            } while (decimal);

            // minimize size, and add a '-' to be flipped at the end
            if (is_negative) {
                string = realloc(string, (allocated = idx + 1) * sizeof(char));
                string[idx++] = '-';
            } else {
                string = realloc(string, (allocated = idx) * sizeof(char));
            }

            middle = (idx - idx % 2) / 2;

            // flip string
            for (size_t i = 0; i < middle; ++i) {
                char c = string[i];
                string[i] = string[idx - i - 1];
                string[idx - i - 1] = c;
            }

            if (fractional) {
                size_t characters_added = 0;
                string = realloc(string, (allocated += 4) * sizeof(char));
                string[idx++] = '.';
                fractional *= 10;
                do {
                    RaelFloat new_fractional = fmod(fractional, 1);
                    if (idx >= allocated)
                        string = realloc(string, (allocated += 4) * sizeof(char));
                    string[idx++] = '0' + (RaelInt)(fractional - new_fractional);
                    fractional = new_fractional;
                } while (++characters_added < 14 && (RaelInt)(fractional *= 10));

                string = realloc(string, (allocated = idx) * sizeof(char));
            }

            casted = value_create(ValueTypeString);
            casted->as_string = (struct RaelStringValue) {
                .type = StringTypePure,
                .can_be_freed = true,
                .length = allocated,
                .value = string
            };
            break;
        }
        default:
            goto invalid_cast;
        }
        break;
    case ValueTypeNumber: // convert to number
        switch (value->type) {
        case ValueTypeVoid: // from void
            casted = number_newi(0);
            break;
        case ValueTypeString: { // from string
            bool is_negative = false;
            size_t string_length = string_get_length(value);
            casted = value_create(ValueTypeNumber);
            if (string_length > 0)
                is_negative = value->as_string.value[0] == '-';

            // try to convert to an int
            if (string_length == (is_negative?1:0) ||
                !number_from_string(value->as_string.value + is_negative, string_length - is_negative, &casted->as_number)) {
                value_deref(casted);
                interpreter_error(interpreter, value_state, "The string '%.*s' can't be parsed as a number",
                                  (RaelInt)string_length, value->as_string.value);
            }
            if (is_negative)
                casted->as_number = number_mul(casted->as_number, numbervalue_newi(-1));
            break;
        }
        default:
            goto invalid_cast;
        }
        break;
    case ValueTypeStack:
        switch (value->type) {
        case ValueTypeString: {
            size_t string_length = string_get_length(value);
            casted = stack_new(string_length);
            for (size_t i = 0; i < string_length; ++i) {
                // create a new RaelNumberValue from the char
                RaelValue char_value = number_newi((RaelInt)string_get_char(value, i));
                stack_push(casted, char_value);
            }
            break;
        }
        default:
            goto invalid_cast;
        }
        break;
    default:
        goto invalid_cast;
    }

    return casted;
invalid_cast:
    interpreter_error(interpreter, value_state, "Cannot cast value of type '%s' to a value of type '%s'",
                      value_type_to_string(value_type), value_type_to_string(cast_type));
    RAEL_UNREACHABLE();
}

static RaelValue eval_key_operation_set(struct Interpreter *interpreter, char *key, struct Expr *value_expr,
                                        RaelBinaryOperationFunction operation, struct State expr_state) {
    RaelValue value, *lhs_ptr;
    // try to get the pointer to the value you modify
    lhs_ptr = scope_get_ptr(interpreter->scope, key);
    // if there is a value at that key, get its value and do addition
    if (lhs_ptr) {
        RaelValue rhs = expr_eval(interpreter, value_expr, true);
        // do the operation on the two values
        value = operation(*lhs_ptr, rhs);
        // dereference the temporary rhs
        value_deref(rhs);
        // if it couldn't add the values
        if (!value) {
            value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Invalid operation between values");
        }
    } else {
        value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Key has not been assigned yet");
    }
    // if a blame was created (the blame was returned from the function or the operation was unsuccessful),
    // add a state to the blame value
    if (value_is_blame(value)) {
        blame_add_state(value, expr_state);
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

static RaelValue eval_set_operation_expr(struct Interpreter *interpreter, struct Expr *set_expr,
                                         RaelBinaryOperationFunction operation) {
    switch (set_expr->as_set.set_type) {
    case SetTypeAtExpr: {
        return eval_at_operation_set(interpreter, set_expr->as_set.as_at_stat,
                                     set_expr->as_set.expr, operation, set_expr->state);
    }
    case SetTypeKey:
        return eval_key_operation_set(interpreter, set_expr->as_set.as_key, set_expr->as_set.expr,
                                      operation, set_expr->state);
    default:
        RAEL_UNREACHABLE();
    }
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
        // eval lhs and rhs in lhs + rhs
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);
        // try to add the values
        value = values_add(lhs, rhs);
        if (!value) {
            value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Invalid operation (+) on types");
        }
        if (value_is_blame(value)) {
            blame_add_state(value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeSub:
        // eval lhs and rhs in lhs - rhs
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);
        // try to add the values
        value = values_sub(lhs, rhs);
        if (!value) {
            value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Invalid operation (-) on types");
        }
        if (value_is_blame(value)) {
            blame_add_state(value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeMul:
        // eval lhs and rhs in lhs * rhs
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);
        // try to add the values
        value = values_mul(lhs, rhs);
        if (!value) {
            value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Invalid operation (*) on types");
        }
        if (value_is_blame(value)) {
            blame_add_state(value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeDiv:
        // eval lhs and rhs in lhs / rhs
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);
        // try to add the values
        value = values_div(lhs, rhs);
        if (!value) {
            value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Invalid operation (/) on types");
        }
        if (value_is_blame(value)) {
            blame_add_state(value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeMod:
        // eval lhs and rhs in lhs % rhs
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);
        // try to add the values
        value = values_mod(lhs, rhs);
        if (!value) {
            value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Invalid operation (%) on types");
        }
        if (value_is_blame(value)) {
            blame_add_state(value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeEquals:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        // create an integer from the boolean result of the comparison
        value = number_newi(values_equal(lhs, rhs) ? 1 : 0);

        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeSmallerThan:
        // eval lhs and rhs in lhs < rhs
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);
        // try to add the values
        value = values_smaller(lhs, rhs);
        if (!value) {
            value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Invalid operation (<) on types");
        }
        if (value_is_blame(value)) {
            blame_add_state(value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeBiggerThan:
        // eval lhs and rhs in lhs > rhs
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);
        // try to add the values
        value = values_bigger(lhs, rhs);
        if (!value) {
            value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Invalid operation (>) on types");
        }
        if (value_is_blame(value)) {
            blame_add_state(value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeSmallerOrEqual:
        // eval lhs and rhs in lhs <= rhs
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);
        // try to add the values
        value = values_smaller_eq(lhs, rhs);
        if (!value) {
            value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Invalid operation (<=) on types");
        }
        if (value_is_blame(value)) {
            blame_add_state(value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeBiggerOrEqual:
        // eval lhs and rhs in lhs >= rhs
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);
        // try to add the values
        value = values_bigger_eq(lhs, rhs);
        if (!value) {
            value = RAEL_BLAME_FROM_RAWSTR_NO_STATE("Invalid operation (>=) on types");
        }
        if (value_is_blame(value)) {
            blame_add_state(value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeCall:
        value = call_expr_eval(interpreter, expr->as_call, expr->state);
        break;
    case ExprTypeAt:
        value = value_at(interpreter, expr);
        break;
    case ExprTypeTo:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        switch (rhs->type) {
        case ValueTypeNumber: // range: 'number to number'
            // if there was an error in verifying the values are whole numbers, return the errors
            if ((value = value_verify_int(lhs, expr->lhs->state)) ||
                (value = value_verify_int(rhs, expr->rhs->state))) {
                value_deref(lhs);
                value_deref(rhs);
                return value;
            }
            value = value_create(ValueTypeRange);
            value->as_range.start = number_to_int(lhs->as_number);
            value->as_range.end = number_to_int(rhs->as_number);
            break;
        // cast
        case ValueTypeType:
            value = value_cast(interpreter, lhs, rhs->as_type, expr->lhs->state);
            break;
        default:
            value_deref(lhs);
            value_deref(rhs);
            interpreter_error(interpreter, expr->rhs->state, "Expected number or type");
        }

        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeRedirect:
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (lhs->type != ValueTypeStack) {
            value_deref(lhs);
            interpreter_error(interpreter, expr->lhs->state, "Expected a stack value");
        }

        // eval the rhs, push it and set the stack as the return value
        rhs = expr_eval(interpreter, expr->rhs, true);
        stack_push(lhs, rhs);
        value = lhs;
        break;
    case ExprTypeSizeof:
        single = expr_eval(interpreter, expr->as_single, true);

        if (value_is_iterable(single)) {
            value = value_create(ValueTypeNumber);
            value->as_number = numbervalue_newi((RaelInt)value_get_length(single));
        } else if (single->type == ValueTypeVoid) {
            value = value_create(ValueTypeNumber);
            value->as_number = numbervalue_newi(0);
        } else {
            value_deref(single);
            interpreter_error(interpreter, expr->as_single->state, "Unsupported type for 'sizeof' operation");
        }

        value_deref(single);
        break;
    case ExprTypeNeg: {
        RaelValue maybe_number = expr_eval(interpreter, expr->as_single, can_explode);

        if (maybe_number->type != ValueTypeNumber) {
            value_deref(maybe_number);
            interpreter_error(interpreter, expr->as_single->state, "Expected number");
        }

        value = value_create(ValueTypeNumber);
        value->as_number = number_neg(maybe_number->as_number);

        value_deref(maybe_number);
        break;
    }
    case ExprTypeBlame: {
        RaelValue message;
        // if there is something after `blame`
        if (expr->as_single) {
            message = expr_eval(interpreter, expr->as_single, true);
        } else {
            message = NULL;
        }
        value = blame_new(message, expr->state);
        break;
    }
    case ExprTypeSet:
        switch (expr->as_set.set_type) {
        case SetTypeAtExpr: {
            value = eval_stack_set(interpreter, expr->as_set.as_at_stat, expr->as_set.expr);
            break;
        }
        case SetTypeKey: {
            value = expr_eval(interpreter, expr->as_set.expr, true);
            scope_set(interpreter->scope, expr->as_set.as_key, value, false);
            // reference again because the value is returned
            value_ref(value);
            break;
        }
        default:
            RAEL_UNREACHABLE();
        }
        break;
    case ExprTypeAddEqual:
        value = eval_set_operation_expr(interpreter, expr, values_add);
        break;
    case ExprTypeSubEqual:
        value = eval_set_operation_expr(interpreter, expr, values_sub);
        break;
    case ExprTypeMulEqual:
        value = eval_set_operation_expr(interpreter, expr, values_mul);
        break;
    case ExprTypeDivEqual:
        value = eval_set_operation_expr(interpreter, expr, values_div);
        break;
    case ExprTypeModEqual:
        value = eval_set_operation_expr(interpreter, expr, values_mod);
        break;
    case ExprTypeAnd: {
        int result = 0;
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (value_as_bool(lhs) == true) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            result = value_as_bool(rhs) == true ? 1 : 0;
            value_deref(rhs);
        }
        value_deref(lhs);

        value = number_newi(result);
        break;
    }
    case ExprTypeOr: {
        int result = 0;
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (value_as_bool(lhs) == true) {
            result = 1;
        } else {
            rhs = expr_eval(interpreter, expr->rhs, true);
            result = value_as_bool(rhs) == true ? 1 : 0;
            value_deref(rhs);
        }
        value_deref(lhs);

        value = number_newi(result);
        break;
    }
    case ExprTypeTypeof:
        single = expr_eval(interpreter, expr->as_single, true);

        value = value_create(ValueTypeType);
        value->as_type = single->type;
        value_deref(single);
        break;
    case ExprTypeGetString: {
        struct RaelStringValue string;
        single = expr_eval(interpreter, expr->as_single, true);
        value_log(single);
        value_deref(single);
        // this is a possible exit point so don't allocate
        // the value on the heap yet because it could leak
        string = rael_readline(interpreter, expr->state);
        value = value_create(ValueTypeString);
        value->as_string = string;
        break;
    }
    case ExprTypeMatch: {
        RaelValue match_against = expr_eval(interpreter, expr->as_match.match_against, true);
        bool matched = false;

        // loop all match cases while there's no match
        for (size_t i = 0; i < expr->as_match.amount_cases && !matched; ++i) {
            struct MatchCase match_case = expr->as_match.match_cases[i];
            // evaluate the value to compare with
            RaelValue with_value = expr_eval(interpreter, match_case.case_value, true);

            // check if the case matches, and if it does,
            // run its block and stop the match's execution
            if (values_equal(match_against, with_value)) {
                block_run(interpreter, match_case.case_block);
                matched = true;
            }
            value_deref(with_value);
        }

        // if you've matched nothing and there's an else case, run the else block
        if (!matched && expr->as_match.else_block)
            block_run(interpreter, expr->as_match.else_block);

        // if there was a return, set the match's return value to the return
        // value and clear the interrupt
        if (interpreter->interrupt == ProgramInterruptReturn) {
            // set return value
            value = interpreter->returned_value;
            interpreter->interrupt = ProgramInterruptNone;
        } else {
            // if there was no interrupt, set the return value to Void
            value = value_create(ValueTypeVoid);
        }

        // deallocate the value it matched against because it has no use anymore
        value_deref(match_against);
        break;
    }
    case ExprTypeGetKey:
        lhs = expr_eval(interpreter, expr->as_getkey.lhs, true);
        if (lhs->type != ValueTypeModule) {
            value_deref(lhs);
            interpreter_error(interpreter, expr->state, "Expected lhs of key indexing expression to be a module");
        }
        value = module_get_key(&lhs->as_module, expr->as_getkey.at_key);
        assert(value);
        value_deref(lhs);
        break;
    default:
        RAEL_UNREACHABLE();
    }

    if (can_explode && value_is_blame(value)) {
        // explode (error)
        struct State state = value->as_blame.original_place;

        // advance all whitespace
        while (state.stream_pos[0] == ' ' || state.stream_pos[0] == '\t') {
            ++state.column;
            ++state.stream_pos;
        }

        rael_show_error_tag(interpreter->filename, state);
        if (value->as_blame.value) {
            value_log(value->as_blame.value);
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
            RaelValue value = expr_eval(interpreter, instruction->csv.exprs[i], true);
            value_log(value);
            value_deref(value);
        }
        break;
    }
    case InstructionTypeIf: {
        struct Scope if_scope;
        RaelValue condition;
        bool is_true;

        interpreter_push_scope(interpreter, &if_scope);

        // evaluate condition and see check it is true then dereference the condition
        condition = expr_eval(interpreter, instruction->if_stat.condition, true);
        is_true = value_as_bool(condition);
        value_deref(condition);

        if (is_true) {
            switch(instruction->if_stat.if_type) {
            case IfTypeBlock:
                block_run(interpreter, instruction->if_stat.if_block);
                break;
            case IfTypeInstruction:
                interpreter_interpret_inst(interpreter, instruction->if_stat.if_instruction);
                break;
            default:
                RAEL_UNREACHABLE();
            }
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
        interpreter_push_scope(interpreter, &loop_scope);

        switch (instruction->loop.type) {
        case LoopWhile: {
            RaelValue condition;

            while (value_as_bool((condition = expr_eval(interpreter, instruction->loop.while_condition, true)))) {
                value_deref(condition);
                block_run(interpreter, instruction->loop.block);

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
        }
        case LoopThrough: {
            RaelValue iterator = expr_eval(interpreter, instruction->loop.iterate.expr, true);

            if (!value_is_iterable(iterator)) {
                value_deref(iterator);
                interpreter_error(interpreter, instruction->loop.iterate.expr->state, "Expected an iterable");
            }

            // calculate length every time because stacks can always grow
            for (size_t i = 0; i < value_get_length(iterator); ++i) {
                scope_set(interpreter->scope, instruction->loop.iterate.key, value_get(iterator, i), false);
                block_run(interpreter, instruction->loop.block);

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
        }
        case LoopForever:
            for (;;) {
                block_run(interpreter, instruction->loop.block);

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

        interpreter_pop_scope(interpreter);
        break;
    }
    case InstructionTypePureExpr:
        // dereference result right after evaluation
        value_deref(expr_eval(interpreter, instruction->pure, true));
        break;
    case InstructionTypeReturn: {
        // if there is a return value, return it, and if there isn't, return a Void
        if (instruction->return_value) {
            interpreter->returned_value = expr_eval(interpreter, instruction->return_value, false);
        } else {
            interpreter->returned_value = value_create(ValueTypeVoid);
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
        RaelValue caught_value = expr_eval(interpreter, instruction->catch.catch_expr, false);

        // handle blame
        if (value_is_blame(caught_value)) {
            struct Scope catch_scope;
            interpreter_push_scope(interpreter, &catch_scope);
            block_run(interpreter, instruction->catch.handle_block);
            interpreter_pop_scope(interpreter);
        }

        value_deref(caught_value);
        break;
    }
    case InstructionTypeLoad: {
        // try to load module
        RaelValue module = rael_get_module_by_name(instruction->load.module_name);
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
    RaelValue argv_stack = stack_new(argc);
    for (size_t i = 0; i < argc; ++i) {
        RaelValue arg = string_new_pure(argv[i], strlen(argv[i]), false);
        stack_push(argv_stack, arg);
    }
    scope_set_local(interpreter->scope, RAEL_HEAPSTR("_Argv"), argv_stack, true);
}

static void interpreter_set_filename(struct Interpreter *interpreter, char *filename) {
    RaelValue value;
    if (filename) {
        value = string_new_pure(filename, strlen(filename), false);
    } else {
        value = value_create(ValueTypeVoid);
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
