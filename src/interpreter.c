#include "common.h"
#include "scope.h"
#include "value.h"
#include "number.h"
#include "string.h"
#include "module.h"
#include "stack.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>

typedef RaelValue *(*RaelBinaryOperationFunction)(RaelValue *, RaelValue *);

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
    RaelValue *returned_value;

    // warnings
    bool warn_undefined;
};

static void interpreter_interpret_inst(struct Interpreter* const interpreter, struct Instruction* const instruction);
static RaelValue *expr_eval(struct Interpreter* const interpreter, struct Expr* const expr, const bool can_explode);
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

static RaelStringValue *rael_readline(struct Interpreter* const interpreter, struct State state) {
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

static RaelValue *value_eval(struct Interpreter* const interpreter, struct ValueExpr value) {
    RaelValue *out_value;

    switch (value.type) {
    case ValueTypeNumber: {
        RaelNumberValue *number;
        if (value.as_number.is_float) {
            number = number_newf(value.as_number.as_float);
        } else {
            number = number_newi(value.as_number.as_int);
        }
        out_value = (RaelValue*)number;
        break;
    }
    case ValueTypeString:
        out_value = (RaelValue*)string_new_pure(
            value.as_string.source,
            value.as_string.length,
            false // flags not to deallocate string, because there is still a reference in the ast
        );
        break;
    case ValueTypeRoutine: {
        const RaelRoutineValue ast_routine = value.as_routine;
        RaelRoutineValue *new_routine = RAEL_VALUE_NEW(ValueTypeRoutine, RaelRoutineValue);

        new_routine->block = ast_routine.block;
        new_routine->parameters = ast_routine.parameters;
        new_routine->amount_parameters = ast_routine.amount_parameters;
        new_routine->scope = interpreter->scope;

        out_value = (RaelValue*)new_routine;
        break;
    }
    case ValueTypeStack: {
        size_t overhead = value.as_stack.amount_exprs;
        // create a new stack with the exact amount of overhead you need for putting all of the values
        RaelStackValue *stack = stack_new(overhead);

        // push all of the values
        for (size_t i = 0; i < overhead; ++i) {
            stack_push(stack, expr_eval(interpreter, value.as_stack.exprs[i], true));
        }
        out_value = (RaelValue*)stack;
        break;
    }
    case ValueTypeVoid: {
        out_value = RAEL_VALUE_NEW(ValueTypeVoid, RaelValue);
        break;
    }
    case ValueTypeType: {
        RaelTypeValue *type = RAEL_VALUE_NEW(ValueTypeType, RaelTypeValue);
        type->type = value.as_type;
        out_value = (RaelValue*)type;
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
    if (number->type != ValueTypeNumber) {
        blame = RAEL_BLAME_FROM_RAWSTR_STATE("Expected a number", number_state);
    } else if (!number_is_whole((RaelNumberValue*)number)) {
        blame = RAEL_BLAME_FROM_RAWSTR_STATE("Float index is not allowed", number_state);
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
        blame = RAEL_BLAME_FROM_RAWSTR_STATE("A negative index is not allowed", number_state);
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
    if (!stack_set((RaelStackValue*)stack, (size_t)number_to_int((RaelNumberValue*)idx), value)) {
        value_deref(stack);
        value_deref(idx);
        return RAEL_BLAME_FROM_RAWSTR_STATE("Index too big", at_expr->rhs->state);
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
    value_ptr = stack_get_ptr((RaelStackValue*)stack, (size_t)number_to_int((RaelNumberValue*)idx));

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
        blame_add_state((RaelBlameValue*)result, operator_state);
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

static RaelValue *stack_at(struct Interpreter* const interpreter, RaelStackValue *stack, struct Expr *at_expr) {
    RaelValue *idx;
    RaelValue *out_value;

    idx = expr_eval(interpreter, at_expr->rhs, true);

    switch (idx->type) {
    case ValueTypeNumber:
        // if idx is not a uint, return the error
        if ((out_value = value_verify_uint(idx, at_expr->rhs->state))) {
            value_deref((RaelValue*)stack);
            value_deref(idx);
            return out_value;
        }
        out_value = stack_get((RaelStackValue*)stack, (size_t)((RaelNumberValue*)idx)->as_int);
        // if is out of range
        if (!out_value) {
            value_deref((RaelValue*)stack);
            value_deref(idx);
            interpreter_error(interpreter, at_expr->rhs->state, "Index too big");
        }
        break;
    case ValueTypeRange: {
        RaelInt start = ((RaelRangeValue*)idx)->start,
                end = ((RaelRangeValue*)idx)->end;
        size_t length = stack_length(stack);

        // make sure range numbers are positive, e.g -2 to 3 is not allowed
        if (start < 0 || end < 0) {
            value_deref((RaelValue*)stack);
            value_deref(idx);
            interpreter_error(interpreter, at_expr->rhs->state, "Negative range numbers for stack slicing are not allowed");
        }

        // make sure range direction is not negative
        if (end < start) {
            value_deref((RaelValue*)stack);
            value_deref(idx);
            interpreter_error(interpreter, at_expr->rhs->state, "Range end is smaller than its start when slicing a stack");
        }

        // make sure you are inside of the stack's boundaries
        if ((size_t)start > length || (size_t)end > length) {
            value_deref((RaelValue*)stack);
            value_deref(idx);
            interpreter_error(interpreter, at_expr->rhs->state, "Stack slicing out of range");
        }

        out_value = (RaelValue*)stack_slice(stack, (size_t)start, (size_t)end);
        assert(out_value);
        break;
    }
    default:
        value_deref((RaelValue*)stack);
        value_deref(idx);
        interpreter_error(interpreter, at_expr->rhs->state, "Expected number or range");
    }

    value_deref((RaelValue*)stack); // lhs
    value_deref(idx); // rhs
    return out_value;
}

static RaelValue *string_at(struct Interpreter* const interpreter, RaelStringValue *string, struct Expr *at_expr) {
    RaelValue *at_value;
    RaelValue *out_value;

    at_value = expr_eval(interpreter, at_expr->rhs, true);

    switch (at_value->type) {
    case ValueTypeRange: {
        const RaelRangeValue *range = (RaelRangeValue*)at_value;
        const size_t str_length = string_length(string);

        // make sure range numbers are positive, -2 to -3 is not allowed
        if (range->start < 0 || range->end < 0) {
            value_deref((RaelValue*)string);
            value_deref(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Expected non-negative range numbers");
        }

        // make sure range direction is not negative, e.g 4 to 2
        if (range->end < range->start) {
            value_deref((RaelValue*)string);
            value_deref(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Negative range direction for substrings is not allowed");
        }

        // make sure you are inside of the string's boundaries
        if ((size_t)range->start > str_length ||
            (size_t)range->end > str_length) {
            value_deref((RaelValue*)string);
            value_deref(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Substring out of range");
        }

        out_value = (RaelValue*)string_slice((RaelStringValue*)string, (size_t)range->start, (size_t)range->end);
        break;
    }
    case ValueTypeNumber:
        // verify the value is a uint. if it is, you've already got the error, so just do nothing
        if (!(out_value = value_verify_uint(at_value, at_expr->rhs->state))) {
            const RaelNumberValue *number = (RaelNumberValue*)at_value;
            out_value = string_get((RaelStringValue*)string, (size_t)number->as_int);

            // if index was too big
            if (!out_value) {
                value_deref((RaelValue*)string);
                value_deref(at_value);
                interpreter_error(interpreter, at_expr->rhs->state, "Index too big");
            }
        }
        break;
    default:
        interpreter_error(interpreter, at_expr->rhs->state, "Expected range or number");
    }

    value_deref((RaelValue*)string);   // lhs
    value_deref(at_value); // rhs
    return out_value;
}

static RaelValue *range_at(struct Interpreter* const interpreter, RaelRangeValue *range, struct Expr *at_expr) {
    RaelValue *idx;
    RaelValue *out_value;
    size_t idx_number;

    // evaluate index
    idx = expr_eval(interpreter, at_expr->rhs, true);
    // if the value was not a uint, return an error
    if ((out_value = value_verify_uint(idx, at_expr->rhs->state))) {
        value_deref(idx);
        return out_value;
    }

    // get the integer index
    idx_number = (size_t)((RaelNumberValue*)idx)->as_int;

    // if the index 
    if (idx_number >= (size_t)rael_int_abs(range->end - range->start)) {
        value_deref((RaelValue*)range);
        value_deref(idx);
        interpreter_error(interpreter, at_expr->rhs->state, "Index too big");
    }

    out_value = range_get(range, idx_number);

    value_deref((RaelValue*)range); // dereference lhs
    value_deref(idx);   // dereference rhs
    return out_value;
}

static RaelValue *value_at(struct Interpreter* const interpreter, struct Expr *expr) {
    RaelValue *lhs;
    RaelValue *value;

    assert(expr->type == ExprTypeAt);
    // evaluate the lhs of the at expression
    lhs = expr_eval(interpreter, expr->lhs, true);

    switch (lhs->type) {
    case ValueTypeStack:
        value = stack_at(interpreter, (RaelStackValue*)lhs, expr);
        break;
    case ValueTypeString:
        value = string_at(interpreter, (RaelStringValue*)lhs, expr);
        break;
    case ValueTypeRange:
        value = range_at(interpreter, (RaelRangeValue*)lhs, expr);
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
static int routine_call_expr_eval(struct Interpreter *interpreter, RaelRoutineValue *routine, struct CallExpr *call) {
    size_t amount_args, amount_params;
    struct Scope routine_scope, *prev_scope;

    amount_params = routine->amount_parameters;
    amount_args = call->arguments.amount_exprs;

    // set flags if the amount of arguments isn't equal to the amount of parameters
    if (amount_args < amount_params) {
        return 1;
    } else if (amount_args > amount_params) {
        return 2;
    }

    // create the routine scope
    scope_construct(&routine_scope, routine->scope);

    // set parameters as variables
    for (size_t i = 0; i < amount_params; ++i) {
        scope_set_local(&routine_scope,
                        routine->parameters[i],
                        expr_eval(interpreter, call->arguments.exprs[i], true),
                        false);
    }

    // store current scope and set it to the routine scope
    prev_scope = interpreter->scope;
    interpreter->scope = &routine_scope;
    // run the routine
    block_run(interpreter, routine->block);
    if (interpreter->interrupt == ProgramInterruptReturn) {
        // had a return statement
        ;
    } else {
        // just got to the end of the function (which means no return value)
        interpreter->returned_value = RAEL_VALUE_NEW(ValueTypeVoid, RaelValue);
    }

    interpreter->interrupt = ProgramInterruptNone;
    // deallocate routine scope and switch scope to previous scope
    scope_dealloc(&routine_scope);
    interpreter->scope = prev_scope;
    return 0;
}

static int cfunc_call_expr_eval(struct Interpreter *interpreter, RaelExternalCFuncValue *cfunc, struct CallExpr *call, struct State state) {
    RaelArguments arguments;
    size_t amount_args, amount_params;

    amount_params = cfunc->amount_params;
    amount_args = call->arguments.amount_exprs;

    if (amount_args < amount_params) {
        return 1; // too few arguments
    } else if (amount_args > amount_params) {
        return 2; // too many arguments
    }

    // initialize argument list and add arguments
    arguments_new(&arguments);
    for (size_t i = 0; i < amount_args; ++i) {
        arguments_add(&arguments, expr_eval(interpreter, call->arguments.exprs[i], true));
    }
    // minimize size of argument list (realloc to minimum size)
    arguments_finalize(&arguments);
    // set the interpreter return value to the return value of the function after execution
    interpreter->returned_value = cfunc_call(cfunc, &arguments, state);
    // verify the return value is not invalid
    assert(interpreter->returned_value != NULL);
    // delete argument list
    arguments_delete(&arguments);
    return 0;
}

static RaelValue *call_expr_eval(struct Interpreter* const interpreter,
                                         struct CallExpr call, struct State state) {
    RaelValue *callable = expr_eval(interpreter, call.callable_expr, true);
    int err_state = 0;

    switch (callable->type) {
    case ValueTypeRoutine:
        err_state = routine_call_expr_eval(interpreter, (RaelRoutineValue*)callable, &call);
        break;
    case ValueTypeCFunc:
        err_state = cfunc_call_expr_eval(interpreter, (RaelExternalCFuncValue*)callable, &call, state);
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

static RaelValue *
value_cast(struct Interpreter* const interpreter, RaelValue *value, enum ValueType cast_type, struct State value_state) {
    RaelValue *casted;
    enum ValueType value_type = value->type;

    if (value_type == cast_type) {
        // add reference because it is returned
        value_ref(value);
        return value;
    }

    switch (cast_type) {
    case ValueTypeString:
        switch (value->type) {
        case ValueTypeVoid:
            casted = (RaelValue*)RAEL_STRING_FROM_RAWSTR("Void");
            break;
        case ValueTypeNumber: {
            // FIXME: make float conversions more accurate
            bool is_negative;
            RaelInt decimal;
            RaelFloat fractional;
            size_t allocated, idx = 0;
            char *source = malloc((allocated = 10) * sizeof(char));
            size_t middle;
            const RaelNumberValue *number = (RaelNumberValue*)value;

            // check if the number is negative and set a flag
            if (number->is_float) {
                is_negative = number->as_float < 0.0;
                fractional = fmod(rael_float_abs(number->as_float), 1);
            } else {
                is_negative = number->as_int < 0;
                fractional = 0.0;
            }
            // set the decimal value to the absolute whole value of the number
            decimal = rael_int_abs(number_to_int((RaelNumberValue*)number));

            do {
                if (idx >= allocated)
                    source = realloc(source, (allocated += 10) * sizeof(char));
                source[idx++] = '0' + (decimal % 10);
                decimal = (decimal - decimal % 10) / 10;
            } while (decimal);

            // minimize size, and add a '-' to be flipped at the end
            if (is_negative) {
                source = realloc(source, (allocated = idx + 1) * sizeof(char));
                source[idx++] = '-';
            } else {
                source = realloc(source, (allocated = idx) * sizeof(char));
            }

            middle = (idx - idx % 2) / 2;

            // flip string
            for (size_t i = 0; i < middle; ++i) {
                char c = source[i];
                source[i] = source[idx - i - 1];
                source[idx - i - 1] = c;
            }

            if (fractional) {
                size_t characters_added = 0;
                source = realloc(source, (allocated += 4) * sizeof(char));
                source[idx++] = '.';
                fractional *= 10;
                do {
                    RaelFloat new_fractional = fmod(fractional, 1);
                    if (idx >= allocated)
                        source = realloc(source, (allocated += 4) * sizeof(char));
                    source[idx++] = '0' + (RaelInt)(fractional - new_fractional);
                    fractional = new_fractional;
                } while (++characters_added < 14 && (RaelInt)(fractional *= 10));

                source = realloc(source, (allocated = idx) * sizeof(char));
            }

            casted = (RaelValue*)string_new_pure(source, allocated, true);
            break;
        }
        default:
            goto invalid_cast;
        }
        break;
    case ValueTypeNumber: // convert to number
        switch (value->type) {
        case ValueTypeVoid: // from void
            casted = (RaelValue*)number_newi(0);
            break;
        case ValueTypeString: { // from string
            bool is_negative = false;
            RaelStringValue *string = (RaelStringValue*)value;
            size_t str_length = string_length(string);
            RaelNumberValue *as_number;

            if (str_length > 0)
                is_negative = string->source[0] == '-';

            // try to convert to an int
            if (str_length == (is_negative?1:0) ||
                !(as_number = number_from_string(string->source + is_negative, str_length - is_negative))) {
                value_deref((RaelValue*)as_number);
                interpreter_error(interpreter, value_state, "The string '%.*s' can't be parsed as a number",
                                  (RaelInt)str_length, ((RaelStringValue*)value)->source);
            }
            if (is_negative) {
                // if the number is negative, set the value to it's opposite
                if (as_number->is_float) {
                    as_number->as_float *= -1;
                } else {
                    as_number->as_int *= -1;
                }
            }
            casted = (RaelValue*)as_number;
            break;
        }
        default:
            goto invalid_cast;
        }
        break;
    case ValueTypeStack:
        switch (value->type) {
        case ValueTypeString: {
            RaelStringValue *string = (RaelStringValue*)value;
            size_t str_length = string_length(string);
            RaelStackValue *stack = stack_new(str_length);

            for (size_t i = 0; i < str_length; ++i) {
                // create a new RaelNumberValue from the char
                RaelNumberValue *char_value = number_newi((RaelInt)string_get_char(string, i));
                stack_push(stack, (RaelValue*)char_value);
            }

            casted = (RaelValue*)stack;
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

static RaelValue *eval_key_operation_set(struct Interpreter *interpreter, char *key, struct Expr *value_expr,
                                                 RaelBinaryOperationFunction operation, struct State expr_state) {
    RaelValue *value;
    RaelValue **lhs_ptr;
    // try to get the pointer to the value you modify
    lhs_ptr = scope_get_ptr(interpreter->scope, key);
    // if there is a value at that key, get its value and do addition
    if (lhs_ptr) {
        RaelValue *rhs = expr_eval(interpreter, value_expr, true);
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
        blame_add_state((RaelBlameValue*)value, expr_state);
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

static RaelValue *eval_set_operation_expr(struct Interpreter *interpreter, struct Expr *set_expr,
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

static RaelValue *expr_eval(struct Interpreter* const interpreter, struct Expr* const expr, const bool can_explode) {
    RaelValue *lhs;
    RaelValue *rhs;
    RaelValue *single;
    RaelValue *value;

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
            blame_add_state((RaelBlameValue*)value, expr->state);
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
            blame_add_state((RaelBlameValue*)value, expr->state);
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
            blame_add_state((RaelBlameValue*)value, expr->state);
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
            blame_add_state((RaelBlameValue*)value, expr->state);
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
            blame_add_state((RaelBlameValue*)value, expr->state);
        }
        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeEquals:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        // create an integer from the boolean result of the comparison
        value = (RaelValue*)number_newi(values_eq(lhs, rhs) ? 1 : 0);

        value_deref(lhs);
        value_deref(rhs);
        break;
    case ExprTypeNotEqual:
        lhs = expr_eval(interpreter, expr->lhs, true);
        rhs = expr_eval(interpreter, expr->rhs, true);

        // create an integer from the boolean result of the comparison
        value = (RaelValue*)number_newi(values_eq(lhs, rhs) ? 0 : 1);

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
            blame_add_state((RaelBlameValue*)value, expr->state);
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
            blame_add_state((RaelBlameValue*)value, expr->state);
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
            blame_add_state((RaelBlameValue*)value, expr->state);
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
            blame_add_state((RaelBlameValue*)value, expr->state);
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
        case ValueTypeNumber: { // range: 'number to number'
            RaelRangeValue *range;

            // if there was an error in verifying the values are whole numbers, return the errors
            if ((value = value_verify_int(lhs, expr->lhs->state)) ||
                (value = value_verify_int(rhs, expr->rhs->state))) {
                value_deref(lhs);
                value_deref(rhs);
                return value;
            }

            range = range_new(number_to_int((RaelNumberValue*)lhs),
                              number_to_int((RaelNumberValue*)rhs));
            value = (RaelValue*)range;
            break;
        }
        // cast
        case ValueTypeType:
            value = value_cast(interpreter, lhs, ((RaelTypeValue*)rhs)->type, expr->lhs->state);
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
        stack_push((RaelStackValue*)lhs, rhs);
        value = lhs;
        break;
    case ExprTypeSizeof:
        single = expr_eval(interpreter, expr->as_single, true);

        if (value_is_iterable(single)) {
            value = (RaelValue*)number_newi((RaelInt)value_length(single));
        } else if (single->type == ValueTypeVoid) {
            value = (RaelValue*)number_newi(0);
        } else {
            value_deref(single);
            interpreter_error(interpreter, expr->as_single->state, "Unsupported type for 'sizeof' operation");
        }

        value_deref(single);
        break;
    case ExprTypeNeg: {
        RaelValue *maybe_number = expr_eval(interpreter, expr->as_single, can_explode);

        if (maybe_number->type != ValueTypeNumber) {
            value_deref(maybe_number);
            interpreter_error(interpreter, expr->as_single->state, "Expected number");
        }

        // get the opposite of the number
        value = (RaelValue*)number_neg((RaelNumberValue*)maybe_number);

        value_deref(maybe_number);
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
        value = (RaelValue*)blame_new(message, expr->state);
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

        value = (RaelValue*)number_newi(result);
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

        value = (RaelValue*)number_newi(result);
        break;
    }
    case ExprTypeNot: {
        bool negative;
        // eval the inside of the '!' expr
        single = expr_eval(interpreter, expr->lhs, true);
        // take the opposite of the value's boolean representation
        negative = !value_as_bool(single);
        // deallocate the now unused inside value
        value_deref(single);
        // create a new number value from the opposite of `single`
        value = (RaelValue*)number_newi(negative ? 1 : 0);
        break;
    }
    case ExprTypeTypeof: {
        RaelTypeValue *type_value;
        enum ValueType type;

        single = expr_eval(interpreter, expr->as_single, true);
        type = single->type; // get type of single
        // deallocate single because we've already got the type
        value_deref(single);
        // create a RaelTypeValue value with single's type
        type_value = RAEL_VALUE_NEW(ValueTypeType, RaelTypeValue);
        type_value->type = type;
        value = (RaelValue*)type_value;
        break;
    }
    case ExprTypeGetString:
        single = expr_eval(interpreter, expr->as_single, true);
        value_log(single);
        value_deref(single);
        value = (RaelValue*)rael_readline(interpreter, expr->state);
        break;
    case ExprTypeMatch: { // a more complex switch statement
        RaelValue *match_against = expr_eval(interpreter, expr->as_match.match_against, true);
        bool matched = false;

        // loop all match cases while there's no match
        for (size_t i = 0; i < expr->as_match.amount_cases && !matched; ++i) {
            struct MatchCase match_case = expr->as_match.match_cases[i];
            // evaluate the value to compare with
            RaelValue *with_value = expr_eval(interpreter, match_case.case_value, true);

            // check if the case matches, and if it does,
            // run its block and stop the match's execution
            if (values_eq(match_against, with_value)) {
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
            value = RAEL_VALUE_NEW(ValueTypeVoid, RaelValue);
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
        value = module_get_key((RaelModuleValue*)lhs, expr->as_getkey.at_key);
        assert(value);
        value_deref(lhs);
        break;
    default:
        RAEL_UNREACHABLE();
    }

    if (can_explode && value_is_blame(value)) {
        // explode (error)
        RaelBlameValue *blame = (RaelBlameValue*)value;
        struct State state = blame->original_place;

        // advance all whitespace
        while (state.stream_pos[0] == ' ' || state.stream_pos[0] == '\t') {
            ++state.column;
            ++state.stream_pos;
        }

        rael_show_error_tag(interpreter->filename, state);
        if (blame->value) {
            value_log(blame->value);
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
            RaelValue *condition;

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
            RaelValue *iterator = expr_eval(interpreter, instruction->loop.iterate.expr, true);

            if (!value_is_iterable(iterator)) {
                value_deref(iterator);
                interpreter_error(interpreter, instruction->loop.iterate.expr->state, "Expected an iterable");
            }

            // calculate length every time because stacks can always grow
            for (size_t i = 0; i < value_length(iterator); ++i) {
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
            interpreter->returned_value = RAEL_VALUE_NEW(ValueTypeVoid, RaelValue);
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
        RaelModuleValue *module = rael_get_module_by_name(instruction->load.module_name);
        // if you couldn't load the module, error
        if (!module)
            interpreter_error(interpreter, instruction->state, "Unknown module name");
        // set the module
        scope_set(interpreter->scope, instruction->load.module_name, (RaelValue*)module, false);
        break;
    }
    default:
        RAEL_UNREACHABLE();
    }
}

static void interpreter_set_argv(struct Interpreter *interpreter, char **argv, size_t argc) {
    RaelStackValue *argv_stack = stack_new(argc);
    for (size_t i = 0; i < argc; ++i) {
        RaelStringValue *arg = string_new_pure(argv[i], strlen(argv[i]), false);
        stack_push(argv_stack, (RaelValue*)arg);
    }
    scope_set_local(interpreter->scope, RAEL_HEAPSTR("_Argv"), (RaelValue*)argv_stack, true);
}

static void interpreter_set_filename(struct Interpreter *interpreter, char *filename) {
    RaelValue *value;
    if (filename) {
        value = (RaelValue*)string_new_pure(filename, strlen(filename), false);
    } else {
        value = RAEL_VALUE_NEW(ValueTypeVoid, RaelValue);
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
