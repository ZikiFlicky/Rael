#include "rael.h"

RaelValue *module_functional_map(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *callable, *iterable;
    size_t length;
    RaelStackValue *mapped_stack;
    assert(arguments_amount(args) == 2);

    callable = arguments_get(args, 0);
    if (!value_is_callable(callable)) {
        return BLAME_NEW_CSTR_ST("Expected a callable", *arguments_state(args, 0));
    }
    if (callable_validate_args(callable, 1) != 0) {
        return BLAME_NEW_CSTR_ST("Expected the callable to take 1 argument",
                                 *arguments_state(args, 0));
    }

    iterable = arguments_get(args, 1);
    if (!value_is_iterable(iterable)) {
        return BLAME_NEW_CSTR_ST("Expected an iterable", *arguments_state(args, 1));
    }

    length = value_length(iterable);
    mapped_stack = (RaelStackValue*)stack_new(length);

    for (size_t i = 0; i < length; ++i) {
        RaelArgumentList callable_args;
        RaelValue *entry = value_get(iterable, i);
        RaelValue *result;

        // create argument list
        arguments_new(&callable_args, 1);
        arguments_add(&callable_args, entry, *arguments_state(args, 1));
        value_deref(entry);
        // call and push the result
        result = value_call(callable, &callable_args, interpreter);
        if (blame_validate(result)) {
            // TODO: write a more detailed blame (also in other functions)
            // deref blame and return a new less detailed one
            value_deref(result);
            stack_delete(mapped_stack);
            arguments_delete(&callable_args);
            return BLAME_NEW_CSTR("Unhandled blame while running :Map");
        }
        stack_push(mapped_stack, result);
        value_deref(result);

        // delete arguments
        arguments_delete(&callable_args);
    }

    return (RaelValue*)mapped_stack;
}

RaelValue *module_functional_filter(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *callable, *iterable;
    RaelStackValue *filtered_stack;
    size_t length;

    (void)interpreter;
    callable = arguments_get(args, 0);
    if (!value_is_callable(callable)) {
        return BLAME_NEW_CSTR_ST("Expected a callable", *arguments_state(args, 0));
    }
    if (callable_validate_args(callable, 1) != 0) {
        return BLAME_NEW_CSTR_ST("Expected the callable to take 1 argument",
                                 *arguments_state(args, 0));
    }

    iterable = arguments_get(args, 1);
    if (!value_is_iterable(iterable)) {
        return BLAME_NEW_CSTR_ST("Expected an iterable", *arguments_state(args, 1));
    }

    filtered_stack = (RaelStackValue *)stack_new(0);
    length = value_length(iterable);

    for (size_t i = 0; i < length; ++i) {
        RaelArgumentList callable_args;
        RaelValue *entry = value_get(iterable, i);
        RaelValue *result;

        // create argument list
        arguments_new(&callable_args, 1);
        arguments_add(&callable_args, entry, *arguments_state(args, 1));
        value_deref(entry);
        // call and check if truthy
        result = value_call(callable, &callable_args, interpreter);
        if (blame_validate(result)) {
            // deref blame and return a new less detailed one
            value_deref(result);
            stack_delete(filtered_stack);
            arguments_delete(&callable_args);
            return BLAME_NEW_CSTR("Unhandled blame while running :Filter");
        }
        // if truthy, push the entry
        if (value_truthy(result))
            stack_push(filtered_stack, entry);
        value_deref(result);

        // delete arguments
        arguments_delete(&callable_args);
    }

    return (RaelValue*)filtered_stack;
}

RaelValue *module_functional_reduce(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *callable, *iterable;
    RaelValue *reduced;
    size_t length;

    (void)interpreter;
    callable = arguments_get(args, 0);
    if (!value_is_callable(callable)) {
        return BLAME_NEW_CSTR_ST("Expected a callable", *arguments_state(args, 0));
    }
    if (callable_validate_args(callable, 2) != 0) {
        return BLAME_NEW_CSTR_ST("Expected the callable to take 2 argument",
                                 *arguments_state(args, 0));
    }

    iterable = arguments_get(args, 1);
    if (!value_is_iterable(iterable)) {
        return BLAME_NEW_CSTR_ST("Expected an iterable", *arguments_state(args, 1));
    }
    if (value_length(iterable) == 0) {
        return BLAME_NEW_CSTR_ST("Expected iterable of size bigger than 0", *arguments_state(args, 1));
    }
    reduced = value_get(iterable, 0);
    length = value_length(iterable);

    for (size_t i = 1; i < length; ++i) {
        RaelArgumentList callable_args;
        RaelValue *entry = value_get(iterable, i);
        RaelValue *result;

        // create argument list
        arguments_new(&callable_args, 2);
        arguments_add(&callable_args, reduced, *arguments_state(args, 1));
        arguments_add(&callable_args, entry, *arguments_state(args, 1));
        value_deref(entry);
        // call and check if truthy
        result = value_call(callable, &callable_args, interpreter);
        if (blame_validate(result)) {
            // deref blame and return a new less detailed one
            value_deref(result);
            value_deref(reduced);
            arguments_delete(&callable_args);
            return BLAME_NEW_CSTR("Unhandled blame while running :Reduce");
        }
        value_deref(reduced);
        reduced = result;

        // delete arguments
        arguments_delete(&callable_args);
    }

    return reduced;
}

RaelValue *module_functional_new(RaelInterpreter *interpreter) {
    RaelModuleValue *m;

    (void)interpreter;
    m = (RaelModuleValue*)module_new(RAEL_HEAPSTR("Functional"));
    module_set_key(m, RAEL_HEAPSTR("Map"), cfunc_new(RAEL_HEAPSTR("Map"), module_functional_map, 2));
    module_set_key(m, RAEL_HEAPSTR("Filter"), cfunc_new(RAEL_HEAPSTR("Filter"), module_functional_filter, 2));
    module_set_key(m, RAEL_HEAPSTR("Reduce"), cfunc_new(RAEL_HEAPSTR("Reduce"), module_functional_reduce, 2));

    return (RaelValue*)m;
}
