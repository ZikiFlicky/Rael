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

enum ProgramInterrupt {
    ProgramInterruptNone,
    ProgramInterruptBreak,
    ProgramInterruptReturn,
};

struct Interpreter {
    char *stream_base;
    const bool stream_on_heap;
    struct Instruction **instructions;
    size_t idx;
    struct Scope *scope;
    enum ProgramInterrupt interrupt;
    bool in_loop;
    bool can_return;
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
    rael_show_error_message(state, error_message, va);
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

static RaelValue value_eval(struct Interpreter* const interpreter, struct ASTValue value) {
    RaelValue out_value = value_create(value.type);

    switch (value.type) {
    case ValueTypeNumber:
        out_value->as_number = value.as_number;
        break;
    case ValueTypeString:
        // it's okay because strings are immutable
        out_value->as_string = value.as_string;
        out_value->as_string.type = StringTypePure;
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
    case ValueTypeType:
        out_value->as_type = value.as_type;
        break;
    default:
        RAEL_UNREACHABLE();
    }

    return out_value;
}

static void value_verify_uint(struct Interpreter* const interpreter,
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

static RaelValue stack_slice(RaelValue stack, size_t start, size_t end) {
    RaelValue new_stack;
    RaelValue *new_dump;
    size_t length;

    assert(stack->type == ValueTypeStack);
    assert(end >= start);
    assert(start <= stack->as_stack.length && end <= stack->as_stack.length);

    new_dump = malloc((length = end - start) * sizeof(RaelValue));

    for (size_t i = 0; i < length; ++i) {
        RaelValue value = stack->as_stack.values[start + i];
        ++value->reference_count;
        new_dump[i] = value;
    }

    new_stack = value_create(ValueTypeStack);
    new_stack->as_stack.allocated = length;
    new_stack->as_stack.length = length;
    new_stack->as_stack.values = new_dump;

    return new_stack;
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

    value_verify_uint(interpreter, expr->rhs->state, rhs);

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

static void stack_push(RaelValue stack, RaelValue value) {
    RaelValue *values;
    size_t allocated, length;
    assert(stack->type == ValueTypeStack);
    values = stack->as_stack.values;
    allocated = stack->as_stack.allocated;
    length = stack->as_stack.length;

    // allocate additional space for stack if there isn't enough
    if (allocated == 0)
        values = malloc((allocated = 16) * sizeof(RaelValue));
    else if (length >= allocated)
        values = realloc(values, (allocated += 16) * sizeof(RaelValue));
    values[length++] = value;

    stack->as_stack = (struct RaelStackValue) {
        .values = values,
        .allocated = allocated,
        .length = length
    };
}

static RaelValue stack_at(struct Interpreter* const interpreter, RaelValue stack, struct Expr *at_expr) {
    RaelValue at_value, out_value;

    assert(stack->type == ValueTypeStack);
    at_value = expr_eval(interpreter, at_expr->rhs, true);

    switch (at_value->type) {
    case ValueTypeNumber:
        if (at_value->as_number.is_float) {
            value_dereference(stack);
            value_dereference(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Float index is not allowed");
        }
        if (at_value->as_number.as_int < 0) {
            value_dereference(stack);
            value_dereference(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "A negative index is not allowed");
        }
        if ((size_t)at_value->as_number.as_int >= stack->as_stack.length) {
            value_dereference(stack);
            value_dereference(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Index too big");
        }

        out_value = value_at_idx(stack, at_value->as_number.as_int);
        break;
    case ValueTypeRange: {
        int start = at_value->as_range.start,
            end = at_value->as_range.end;
        size_t length = stack->as_stack.length;

        // make sure range numbers are positive, -2 to 3 is not allowed
        if (start < 0 || end < 0) {
            value_dereference(stack);
            value_dereference(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Negative range numbers for stack slicing are not allowed");
        }

        // make sure range direction is not negative
        if (end < start) {
            value_dereference(stack);
            value_dereference(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Range start mustn't be smaller than its end when slicing a stack");
        }

        // make sure you are inside of the stack's boundaries
        if ((size_t)start > length || (size_t)end > length) {
            value_dereference(stack);
            value_dereference(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Stack slicing out of range");
        }

        out_value = stack_slice(stack, (size_t)start, (size_t)end);
        break;
    }
    default:
        interpreter_error(interpreter, at_expr->rhs->state, "Expected number or range");
    }

    value_dereference(stack); // lhs
    value_dereference(at_value); // rhs
    return out_value;
}

static RaelValue string_at(struct Interpreter* const interpreter, RaelValue string, struct Expr *at_expr) {
    RaelValue at_value, out_value;

    assert(string->type == ValueTypeString);
    at_value = expr_eval(interpreter, at_expr->rhs, true);

    switch (at_value->type) {
    case ValueTypeRange:
        // make sure range numbers are positive, -2 to -3 is not allowed
        if (at_value->as_range.start < 0 || at_value->as_range.end < 0) {
            value_dereference(string);
            value_dereference(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Expected non-negative range numbers");
        }
        // make sure range direction is not negative, e.g 4 to 2
        if (at_value->as_range.end < at_value->as_range.start) {
            value_dereference(string);
            value_dereference(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Negative range direction for substrings is not allowed");
        }
        // make sure you are inside of the string's boundaries
        if ((size_t)at_value->as_range.start > string->as_string.length ||
            (size_t)at_value->as_range.end > string->as_string.length) {
            value_dereference(string);
            value_dereference(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Substring out of range");
        }

        out_value = string_substr(string, (size_t)at_value->as_range.start, (size_t)at_value->as_range.end);
        break;
    case ValueTypeNumber:
        value_verify_uint(interpreter, at_expr->rhs->state, at_value);
        if ((size_t)at_value->as_number.as_int >= string->as_string.length) {
            value_dereference(string);
            value_dereference(at_value);
            interpreter_error(interpreter, at_expr->rhs->state, "Index too big");
        }

        out_value = value_at_idx(string, at_value->as_number.as_int);
        break;
    default:
        interpreter_error(interpreter, at_expr->rhs->state, "Expected range or number");
    }

    value_dereference(string);   // lhs
    value_dereference(at_value); // rhs
    return out_value;
}

static RaelValue range_at(struct Interpreter* const interpreter, RaelValue range, struct Expr *at_expr) {
    RaelValue at_value, out_value;

    assert(range->type == ValueTypeRange);
    at_value = expr_eval(interpreter, at_expr->rhs, true);

    // evaluate index
    at_value = expr_eval(interpreter, at_expr->rhs, true);
    value_verify_uint(interpreter, at_expr->rhs->state, at_value);

    if ((size_t)at_value->as_number.as_int >= abs(range->as_range.end - range->as_range.start)) {
        value_dereference(range);
        value_dereference(at_value);
        interpreter_error(interpreter, at_expr->rhs->state, "Index too big");
    }

    out_value = value_at_idx(range, at_value->as_number.as_int);

    value_dereference(range);    // lhs
    value_dereference(at_value); // rhs
    return out_value;
}

static RaelValue interpret_value_at(struct Interpreter* const interpreter, struct Expr *expr) {
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
        value_dereference(lhs);
        interpreter_error(interpreter, state, "Expected string, stack or range on the left of 'at'");
    }
    }

    return value;
}

static RaelValue routine_call_eval(struct Interpreter* const interpreter,
                                   struct RoutineCallExpr call, struct State state) {
    struct Scope routine_scope;
    RaelValue maybe_routine = expr_eval(interpreter, call.routine_value, true);
    const bool in_loop_old = interpreter->in_loop;
    const bool can_return_old = interpreter->can_return;
    struct Scope *prev_scope;

    interpreter->in_loop = false;
    interpreter->can_return = true;

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
    interpreter->can_return = can_return_old;
    // deallocate routine scope and switch scope to previous scope
    scope_dealloc(&routine_scope);
    interpreter->scope = prev_scope;

    return interpreter->returned_value;
}

static char *type_to_string(enum ValueType type) {
    switch (type) {
    case ValueTypeVoid: return "VoidType";
    case ValueTypeNumber: return "Number";
    case ValueTypeString: return "String";
    case ValueTypeRoutine: return "Routine";
    case ValueTypeStack: return "Stack";
    case ValueTypeRange: return "Range";
    case ValueTypeType: return "Type";
    default: RAEL_UNREACHABLE();
    }
}

static RaelValue value_cast(struct Interpreter* const interpreter, RaelValue value, enum ValueType cast_type, struct State value_state) {
    RaelValue casted;
    enum ValueType value_type = value->type;

    if (value_type == cast_type) {
        // because you reference the returned value as a new value
        // and deallocate the old one, even if it has the same address
        ++value->reference_count;
        return value;
    }

    switch (cast_type) {
    case ValueTypeString:
        casted = value_create(ValueTypeString);
        switch (value->type) {
        case ValueTypeVoid:
            casted->as_string = (struct RaelStringValue) {
                .type = StringTypePure,
                .does_reference_ast = false,
                .length = 4,
                .value = malloc(4 * sizeof(char))
            };
            // to avoid the warnings when strncpying
            memcpy(casted->as_string.value, "Void", 4 * sizeof(char));
            break;
        case ValueTypeNumber: {
            // FIXME: make float conversions more accurate
            bool is_negative;
            int decimal;
            double fractional;
            size_t allocated, idx = 0;
            char *string = malloc((allocated = 10) * sizeof(char));
            size_t middle;

            if (value->as_number.is_float) {
                is_negative = value->as_number.as_float < 0;
                decimal = abs((int)value->as_number.as_float);
                fractional = fmod(fabs(value->as_number.as_float), 1);
            } else {
                is_negative = value->as_number.as_int < 0;
                decimal = abs(value->as_number.as_int);
                fractional = 0.f;
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
                    double new_fractional = fmod(fractional, 1);
                    if (idx >= allocated)
                        string = realloc(string, (allocated += 4) * sizeof(char));
                    string[idx++] = '0' + (int)(fractional - new_fractional);
                    fractional = new_fractional;
                } while (++characters_added < 14 && (int)(fractional *= 10));

                string = realloc(string, (allocated = idx) * sizeof(char));
            }

            casted = value_create(ValueTypeString);
            casted->as_string = (struct RaelStringValue) {
                .type = StringTypePure,
                .does_reference_ast = false,
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
            casted = value_create(ValueTypeNumber);
            casted->as_number = (struct RaelNumberValue) {
                .as_int = 0
            };
            break;
        case ValueTypeString: { // from string
            bool is_negative = false;
            casted = value_create(ValueTypeNumber);
            if (value->as_string.length > 0)
                is_negative = value->as_string.value[0] == '-';

            // try to convert to an int
            if (value->as_string.length == (is_negative?1:0) ||
                !number_from_string(value->as_string.value + is_negative, value->as_string.length - is_negative, &casted->as_number)) {
                value_dereference(casted);
                interpreter_error(interpreter, value_state, "The string '%.*s' can't be parsed as a number",
                                  (int)value->as_string.length, value->as_string.value);
            }
            if (is_negative)
                casted->as_number = number_mul(casted->as_number, (struct RaelNumberValue) { .as_int = -1 });
            break;
        }
        default:
            goto invalid_cast;
        }
        break;
    case ValueTypeStack:
        switch (value->type) {
        case ValueTypeString:
            casted = value_create(ValueTypeStack);
            casted->as_stack = (struct RaelStackValue) {};
            for (size_t i = 0; i < value->as_string.length; ++i) {
                RaelValue char_value = value_create(ValueTypeNumber);
                char_value->as_number = (struct RaelNumberValue) {
                    .as_int = (int)value->as_string.value[i]
                };
                stack_push(casted, char_value);
            }
            break;
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
                      type_to_string(value_type), type_to_string(cast_type));
    RAEL_UNREACHABLE();
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
            } else if (rhs->type == ValueTypeString) {
                int number;
                struct RaelStringValue string, new_string;

                if (lhs->as_number.is_float) {
                    value_dereference(lhs);
                    value_dereference(rhs);
                    interpreter_error(interpreter, expr->lhs->state, "Expected a whole number");
                }

                number = lhs->as_number.as_int;
                if (number < CHAR_MIN || number > CHAR_MAX) {
                    value_dereference(lhs);
                    value_dereference(rhs);
                    interpreter_error(interpreter, expr->lhs->state, "Number %d is not in ascii", number);
                }

                string = rhs->as_string;
                new_string = (struct RaelStringValue) {
                    .length = 1,
                    .value = malloc((rhs->as_string.length + 1) * sizeof(char))
                };
                new_string.value[0] = (char)number;
                strncpy(new_string.value + 1, string.value, string.length);
                new_string.length += string.length;

                value = value_create(ValueTypeString);
                value->as_string = new_string;
            } else {
                value_dereference(rhs);
                goto invalid_types_add;
            }
        } else if (lhs->type == ValueTypeString) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            if (rhs->type == ValueTypeString) {
                value = string_plus_string(lhs, rhs);
            } else if (rhs->type == ValueTypeNumber) {
                int number;
                struct RaelStringValue string, new_string;

                if (rhs->as_number.is_float) {
                    value_dereference(lhs);
                    value_dereference(rhs);
                    interpreter_error(interpreter, expr->rhs->state, "Expected a whole number");
                }

                number = rhs->as_number.as_int;
                if (number < CHAR_MIN || number > CHAR_MAX) {
                    value_dereference(lhs);
                    value_dereference(rhs);
                    interpreter_error(interpreter, expr->rhs->state, "Number %d is not in ascii", number);
                }

                string = lhs->as_string;
                new_string = (struct RaelStringValue) {
                    .length = string.length,
                    .value = malloc((string.length + 1) * sizeof(char))
                };

                strncpy(new_string.value, string.value, string.length);
                new_string.value[new_string.length++] = (char)number;

                value = value_create(ValueTypeString);
                value->as_string = new_string;
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
                if (!number_div(lhs->as_number, rhs->as_number, &value->as_number)) {
                    interpreter_error(interpreter, expr->state, "Division by zero");
                }
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
                if (!number_mod(lhs->as_number, rhs->as_number, &value->as_number)) {
                    interpreter_error(interpreter, expr->state, "Division by zero");
                }
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
        value->as_number.as_int = values_equal(lhs, rhs) ? 1 : 0;

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
            interpreter_error(interpreter, expr->state, "Invalid operation (<) on types");
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
            interpreter_error(interpreter, expr->state, "Invalid operation (>) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);
        break;
    case ExprTypeSmallerOrEqual:
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (lhs->type == ValueTypeNumber) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            if (rhs->type == ValueTypeNumber) {
                value = value_create(ValueTypeNumber);
                value->as_number = number_smaller_eq(lhs->as_number, rhs->as_number);
            } else {
                value_dereference(rhs);
                goto invalid_types_smaller_eq;
            }
        } else {
        invalid_types_smaller_eq:
            value_dereference(lhs);
            interpreter_error(interpreter, expr->state, "Invalid operation (<=) on types");
        }

        value_dereference(lhs);
        value_dereference(rhs);
        break;
    case ExprTypeBiggerOrEqual:
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (lhs->type == ValueTypeNumber) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            if (rhs->type == ValueTypeNumber) {
                value = value_create(ValueTypeNumber);
                value->as_number = number_bigger_eq(lhs->as_number, rhs->as_number);
            } else {
                value_dereference(rhs);
                goto invalid_types_bigger_eq;
            }
        } else {
        invalid_types_bigger_eq:
            value_dereference(lhs);
            interpreter_error(interpreter, expr->state, "Invalid operation (>=) on types");
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
        rhs = expr_eval(interpreter, expr->rhs, true);

        switch (rhs->type) {
        // range
        case ValueTypeNumber:
            if (rhs->as_number.is_float) {
                value_dereference(lhs);
                value_dereference(rhs);
                interpreter_error(interpreter, expr->rhs->state, "Float not allowed in range");
            }

            // validate that lhs is an int
            if (lhs->type != ValueTypeNumber) {
                value_dereference(lhs);
                interpreter_error(interpreter, expr->lhs->state, "Expected number");
            }
            if (lhs->as_number.is_float){
                value_dereference(lhs);
                interpreter_error(interpreter, expr->lhs->state, "Float not allowed in range");
            }

            value = value_create(ValueTypeRange);
            value->as_range.start = lhs->as_number.as_int;
            value->as_range.end = rhs->as_number.as_int;
            break;
        // cast
        case ValueTypeType:
            value = value_cast(interpreter, lhs, rhs->as_type, expr->lhs->state);
            break;
        default:
            value_dereference(lhs);
            value_dereference(rhs);
            interpreter_error(interpreter, expr->rhs->state, "Expected number or type");
        }

        value_dereference(lhs);
        value_dereference(rhs);
        break;
    case ExprTypeRedirect:
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (lhs->type != ValueTypeStack) {
            value_dereference(lhs);
            interpreter_error(interpreter, expr->lhs->state, "Expected a stack value");
        }

        // eval the rhs, push it and set the stack as the return value
        rhs = expr_eval(interpreter, expr->rhs, true);
        stack_push(lhs, rhs);
        value = lhs;
        break;
    case ExprTypeSizeof:
        single = expr_eval(interpreter, expr->as_single, true);

        if (!value_is_iterable(single)) {
            value_dereference(single);
            interpreter_error(interpreter, expr->as_single->state, "Unsupported type for 'sizeof' operation");
        }

        value = value_create(ValueTypeNumber);
        value->as_number = (struct RaelNumberValue) {
            .is_float = false,
            .as_int = (int)value_get_length(single)
        };

        value_dereference(single);
        break;
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
    case ExprTypeAnd: {
        int result = 0;
        lhs = expr_eval(interpreter, expr->lhs, true);

        if (value_as_bool(lhs) == true) {
            rhs = expr_eval(interpreter, expr->rhs, true);
            result = value_as_bool(rhs) == true ? 1 : 0;
            value_dereference(rhs);
        }
        value_dereference(lhs);

        value = value_create(ValueTypeNumber);
        value->as_number.is_float = false;
        value->as_number.as_int = result;
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
            value_dereference(rhs);
        }
        value_dereference(lhs);

        value = value_create(ValueTypeNumber);
        value->as_number.is_float = false;
        value->as_number.as_int = result;
        break;
    }
    case ExprTypeTypeof:
        single = expr_eval(interpreter, expr->as_single, true);

        value = value_create(ValueTypeType);
        value->as_type = single->type;
        value_dereference(single);
        break;
    case ExprTypeGetString: {
        struct RaelStringValue string;
        single = expr_eval(interpreter, expr->as_single, true);
        value_log(single);
        value_dereference(single);
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
        bool old_can_return = interpreter->can_return;
        interpreter->can_return = true;

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
            value_dereference(with_value);
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
        value_dereference(match_against);
        // reload last can_return state
        interpreter->can_return = old_can_return;
        break;
    }
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
            value_dereference(value);
            for (size_t i = 1; i < instruction->csv.amount_exprs; ++i) {
                printf(" ");
                value_log((value = expr_eval(interpreter, instruction->csv.exprs[i], true)));
                value_dereference(value);
            }
        }
        printf("\n");
        break;
    }
    case InstructionTypeShow: {
        for (size_t i = 0; i < instruction->csv.amount_exprs; ++i) {
            RaelValue value = expr_eval(interpreter, instruction->csv.exprs[i], true);
            value_log(value);
            value_dereference(value);
        }
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

            if (!value_is_iterable(iterator)) {
                value_dereference(iterator);
                interpreter_error(interpreter, instruction->loop.iterate.expr->state, "Expected an iterable");
            }

            // calculate length every time because stacks can always grow
            for (size_t i = 0; i < value_get_length(iterator); ++i) {
                scope_set(interpreter->scope, instruction->loop.iterate.key, value_at_idx(iterator, i));
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
        if (interpreter->can_return) {
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

void rael_interpret(struct Instruction **instructions, char *stream_base, const bool stream_on_heap, const bool warn_undefined) {
    struct Instruction *instruction;
    struct Scope bottom_scope;
    struct Interpreter interp = {
        .instructions = instructions,
        .interrupt = ProgramInterruptNone,
        .in_loop = false,
        .can_return = false,
        .returned_value = NULL,
        .stream_base = stream_base,
        .warn_undefined = warn_undefined,
        .stream_on_heap = stream_on_heap
    };

    scope_construct(&bottom_scope, NULL);
    interp.scope = &bottom_scope;
    for (interp.idx = 0; (instruction = interp.instructions[interp.idx]); ++interp.idx) {
        interpreter_interpret_inst(&interp, instruction);
    }
    interpreter_destroy_all(&interp);
}
