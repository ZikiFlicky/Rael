#include "rael.h"

RaelValue *module_encodings_Base64Encode(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelStringValue *string;
    size_t source_length, encoded_length;
    char *source, *encoded;

    (void)interpreter;
    assert(arguments_amount(args) == 1);

    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelStringType)
        return BLAME_NEW_CSTR_ST("Expected string", *arguments_state(args, 0));

    string = (RaelStringValue*)arg1;
    source = string->source;
    source_length = string_length(string);
    if (source_length % 3 != 0)
        return BLAME_NEW_CSTR_ST("Expected string length to be divisible by 3",
                                *arguments_state(args, 0));

    encoded_length = string_length(string) / 3 * 4;
    encoded = malloc(encoded_length * sizeof(char));

    for (size_t i = 0; i < encoded_length; ++i) {
        size_t byte1_idx = i * 3 / 4; // index of first chunk
        size_t byte2_idx = (i + 1) * 3 / 4; // index of second chunk
        size_t byte1_bits = (8 - (i * 6) % 8) % 8; // number of bits from source[byte1_idx]
        size_t byte2_bits = 6 - byte1_bits; // number of bits from source[byte2_idx]
        char byte = ((source[byte1_idx] & ((1 << byte1_bits) - 1)) << (6 - byte1_bits))
                + (byte2_bits == 0 ? 0 : source[byte2_idx] >> (8 - byte2_bits));
        char c;

        if (byte < 26)
            c = 'A' + byte;
        else if (byte < 52)
            c = 'a' + (byte - 26);
        else if (byte < 62)
            c = '0' + (byte - 52);
        else if (byte == 62)
            c = '+';
        else
            c = '/';
        encoded[i] = c;
    }

    return string_new_pure(encoded, encoded_length, true);
}

RaelValue *module_encodings_Base64Decode(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelStringValue *string;
    size_t source_length, decoded_length;
    char *source, *decoded;

    (void)interpreter;
    assert(arguments_amount(args) == 1);

    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelStringType)
        return BLAME_NEW_CSTR_ST("Expected string", *arguments_state(args, 0));

    string = (RaelStringValue*)arg1;
    source = string->source;
    source_length = string_length(string);
    if (source_length % 4 != 0)
        return BLAME_NEW_CSTR_ST("Invalid base64 string",
                                *arguments_state(args, 0));

    decoded_length = string_length(string) / 4 * 3;
    decoded = malloc(decoded_length * sizeof(char));

    for (size_t i = 0; i < source_length; ++i) {
        int byte = source[i];
        int c;
        if (byte >= 'A' && byte <= 'Z') {
            c = byte - 'A';
        } else if (byte >= 'a' && byte <= 'z') {
            c = byte - 'a' + 26;
        } else if (byte >= '0' && byte <= '9') {
            c = byte - '0' + 52;
        } else if (byte == '+') {
            c = 62;
        } else if (byte == '/') {
            c = 63;
        } else {
            free(decoded);
            return BLAME_NEW_CSTR_ST("Unexpected characters in base64 string",
                                    *arguments_state(args, 0));
        }
        size_t byte1_idx = i * 3 / 4;
        size_t byte2_idx = (i + 1) * 3 / 4;
        size_t byte1_bits = (8 - (i * 6) % 8) % 8;
        size_t byte2_bits = 6 - byte1_bits;

        decoded[byte1_idx] = (decoded[byte1_idx] >> byte1_bits << byte1_bits) + (c >> (6 - byte1_bits));
        decoded[byte2_idx] = (c & ((1 << byte2_bits) - 1)) << (8 - byte2_bits);
    }

    return string_new_pure(decoded, decoded_length, true);
}

RaelValue *module_encodings_HexEncode(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg;
    RaelStringValue *string;
    char *source, *encoded;
    size_t source_length, encoded_length;
    bool use_uppercase;

    (void)interpreter;
    assert(arguments_amount(args) >= 1 && arguments_amount(args) <= 2);

    arg = arguments_get(args, 0);
    if (arg->type != &RaelStringType)
        return BLAME_NEW_CSTR_ST("Expected string", *arguments_state(args, 0));
    string = (RaelStringValue*)arg;

    if (arguments_amount(args) == 2) {
        arg = arguments_get(args, 1);
        use_uppercase = value_truthy(arg);
    } else {
        use_uppercase = false;
    }

    source = string->source;
    source_length = string_length(string);

    encoded_length = source_length * 2;
    encoded = malloc(encoded_length * sizeof(char));

    for (size_t i = 0; i < source_length; ++i) {
        char c = source[i];
        for (size_t j = 0; j < 2; ++j) {
            char computed = (c & (0xf << ((1 - j) * 4))) >> ((1 - j) * 4);
            char repr;

            if (computed < 10)
                repr = '0' + computed;
            else
                repr = (use_uppercase ? 'A' : 'a') + (computed - 10);
            encoded[i * 2 + j] = repr;
        }
    }

    return string_new_pure(encoded, encoded_length, true);
}

RaelValue *module_encodings_HexDecode(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelStringValue *string;
    char *source, *decoded;
    size_t source_length, decoded_length;

    (void)interpreter;
    assert(arguments_amount(args) == 1);

    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelStringType)
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));

    string = (RaelStringValue*)arg1;
    source = string->source;
    source_length = string_length(string);
    if (source_length % 2 != 0)
        return BLAME_NEW_CSTR_ST("Invalid hex string",
                                *arguments_state(args, 0));

    decoded_length = source_length / 2;
    decoded = malloc(decoded_length * sizeof(char));

    for (size_t i = 0; i < decoded_length; ++i) {
        char c1 = tolower(source[i * 2]), c2 = tolower(source[i * 2 + 1]);
        char left, right;
        if (!isxdigit(c1) || !isxdigit(c2)) {
            free(decoded);
            return BLAME_NEW_CSTR_ST("Invalid hex string",
                                    *arguments_state(args, 0));
        }
        if (c1 >= 'a')
            left = c1 - 'a' + 10;
        else
            left = c1 - '0';

        if (c2 >= 'a')
            right = c2 - 'a' + 10;
        else
            right = c2 - '0';

        decoded[i] = (left << 4) + right;
    }

    return string_new_pure(decoded, decoded_length, true);
}

RaelValue *module_encodings_new(RaelInterpreter *interpreter) {
    RaelModuleValue *m;

    (void)interpreter;
    m = (RaelModuleValue*)module_new(RAEL_HEAPSTR("Encodings"));

    module_set_key(m, RAEL_HEAPSTR("Base64Encode"), cfunc_new(RAEL_HEAPSTR("Base64Encode"), module_encodings_Base64Encode, 1));
    module_set_key(m, RAEL_HEAPSTR("Base64Decode"), cfunc_new(RAEL_HEAPSTR("Base64Decode"), module_encodings_Base64Decode, 1));
    module_set_key(m, RAEL_HEAPSTR("HexEncode"), cfunc_new(RAEL_HEAPSTR("Base64Encode"), module_encodings_HexEncode, 1));
    module_set_key(m, RAEL_HEAPSTR("HexDecode"), cfunc_ranged_new(RAEL_HEAPSTR("Base64Decode"), module_encodings_HexDecode, 1, 2));

    return (RaelValue*)m;
}
