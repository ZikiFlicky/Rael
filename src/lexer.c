#include "rael.h"

struct State lexer_dump_state(struct Lexer* const lexer) {
    struct State state;
    state.column = lexer->column;
    state.line = lexer->line;
    state.stream_pos = lexer->stream;
    return state;
}

void lexer_load_state(struct Lexer* const lexer, struct State state) {
    lexer->column = state.column;
    lexer->line = state.line;
    lexer->stream = state.stream_pos;
}

static void lexer_error(struct Lexer* const lexer, const char* const error_message, ...) {
    va_list va;
    va_start(va, error_message);
    rael_show_error_message(lexer->filename, lexer_dump_state(lexer), error_message, va);
    va_end(va);
    if (lexer->stream_on_heap)
        free(lexer->stream_base);
    exit(1);
}

static inline bool is_identifier_char(const char c) {
    return isalnum(c) || c == '_';
}

static inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t';
}

static bool lexer_clean(struct Lexer* const lexer) {
    bool cleaned = false;
    for (;;) {
        if (is_whitespace(lexer->stream[0])) {
            ++lexer->stream;
            ++lexer->column;
        } else if (lexer->stream[0] == '%' && lexer->stream[1] == '%') { // handle comments :^)
            lexer->stream += 2;
            lexer->column += 2;
            while (lexer->stream[0] != '\n' && lexer->stream[0] != '\0') {
                ++lexer->stream;
                ++lexer->column;
            }
        } else if (lexer->stream[0] == '\\') {
            if (lexer->stream[1] != '\n') {
                lexer_error(lexer, "Expected a newline after '\\'");
            }
            lexer->stream += 2;
            ++lexer->line;
            lexer->column = 1;
        } else {
            break;
        }
        cleaned = true;
    }
    return cleaned;
}

static bool lexer_match_keyword(struct Lexer* const lexer, const char* const keyword,
                                 const size_t length, const enum TokenName name) {
    if (strncmp(lexer->stream, keyword, length) == 0 && !is_identifier_char(lexer->stream[length])) {
        lexer->token.name = name;
        lexer->token.string = lexer->stream;
        lexer->token.length = length;
        lexer->stream += length;
        lexer->column += length;
        return true;
    }
    return false;
}

bool lexer_tokenize(struct Lexer* const lexer) {
    lexer_clean(lexer);
    // if it's an eof, return false
    if (lexer->stream[0] == '\0')
        return false;
    // tokenize a bunch of newlines as one newline
    if (lexer->stream[0] == '\n') {
        lexer->token.name = TokenNameNewline;
        lexer->token.string = lexer->stream;
        lexer->token.length = 1;
        do {
            ++lexer->stream;
            ++lexer->line;
            lexer->column = 1;
            lexer_clean(lexer);
        } while(lexer->stream[0] == '\n');
        // if had newlines and was ended with a \0, count it as an eof
        if (lexer->stream[0] == '\0')
            return false;
        return true;
    }
    if (lexer->stream[0] == ':') {
        ++lexer->stream;
        ++lexer->column;
        lexer->token.string = lexer->stream;
        lexer->token.length = 0;
        lexer->token.name = TokenNameKey;
        if (!is_identifier_char(lexer->stream[0]))
            lexer_error(lexer, "Unexpected character after ':'");
        do {
            ++lexer->stream;
            ++lexer->column;
            ++lexer->token.length;
        } while(is_identifier_char(lexer->stream[0]));
        return true;
    }
    // tokenize number
    if (isdigit(lexer->stream[0])) {
        lexer->token.name = TokenNameNumber;
        lexer->token.string = lexer->stream;
        lexer->token.length = 0;
        do {
            ++lexer->token.length;
            ++lexer->stream;
            ++lexer->column;
        } while(isdigit(lexer->stream[0]));
        if (lexer->stream[0] == '.') {
            if (!isdigit(lexer->stream[1]))
                return true;
            do {
                ++lexer->token.length;
                ++lexer->stream;
                ++lexer->column;
            } while (isdigit(lexer->stream[0]));
        }
        return true;
    }
    // tokenize string
    if (lexer->stream[0] == '"') {
        lexer->token.name = TokenNameString;
        lexer->token.string = ++lexer->stream;
        ++lexer->column;
        lexer->token.length = 0;
        while (lexer->stream[0] != '"') {
            if (lexer->stream[0] == '\\') {
                if (lexer->stream[1] == '\\' || lexer->stream[1] == '"') {
                    ++lexer->token.length;
                    ++lexer->stream;
                    ++lexer->column;
                }
            }
            if (lexer->stream[0] == '\n' || lexer->stream[0] == '\0')
                lexer_error(lexer, "Unexpected end-of-line");
            ++lexer->token.length;
            ++lexer->stream;
            ++lexer->column;
        }
        ++lexer->stream;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == '?' && lexer->stream[1] == '=') {
        lexer->token.name = TokenNameQuestionEquals;
        lexer->token.length = 2;
        lexer->token.string = lexer->stream;
        lexer->stream += 2;
        lexer->column += 2;
        return true;
    }
    if (lexer->stream[0] == '+') {
        lexer->token.length = 1;
        lexer->token.string = lexer->stream;
        ++lexer->stream;
        ++lexer->column;
        if (lexer->stream[0] == '=') {
            lexer->token.name = TokenNamePlusEquals;
            ++lexer->token.length;
            ++lexer->stream;
            ++lexer->column;
        } else {
            lexer->token.name = TokenNameAdd;
        }
        return true;
    }
    if (lexer->stream[0] == '-') {
        lexer->token.length = 1;
        lexer->token.string = lexer->stream;
        ++lexer->stream;
        ++lexer->column;
        if (lexer->stream[0] == '=') {
            lexer->token.name = TokenNameMinusEquals;
            ++lexer->token.length;
            ++lexer->stream;
            ++lexer->column;
        } else {
            lexer->token.name = TokenNameSub;
        }
        return true;
    }
    if (lexer->stream[0] == '*') {
        lexer->token.length = 1;
        lexer->token.string = lexer->stream;
        ++lexer->stream;
        ++lexer->column;
        if (lexer->stream[0] == '=') {
            lexer->token.name = TokenNameStarEquals;
            ++lexer->token.length;
            ++lexer->stream;
            ++lexer->column;
        } else {
            lexer->token.name = TokenNameMul;
        }
        return true;
    }
    if (lexer->stream[0] == '/') {
        lexer->token.length = 1;
        lexer->token.string = lexer->stream;
        ++lexer->stream;
        ++lexer->column;
        if (lexer->stream[0] == '=') {
            lexer->token.name = TokenNameSlashEquals;
            ++lexer->token.length;
            ++lexer->stream;
            ++lexer->column;
        } else {
            lexer->token.name = TokenNameDiv;
        }
        return true;
    }
    if (lexer->stream[0] == '%') {
        lexer->token.length = 1;
        lexer->token.string = lexer->stream;
        ++lexer->stream;
        ++lexer->column;
        if (lexer->stream[0] == '=') {
            lexer->token.name = TokenNamePercentEquals;
            ++lexer->token.length;
            ++lexer->stream;
            ++lexer->column;
        } else {
            lexer->token.name = TokenNameMod;
        }
        return true;
    }
    if (lexer->stream[0] == '(') {
        lexer->token.name = TokenNameLeftParen;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == ')') {
        lexer->token.name = TokenNameRightParen;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == ',') {
        lexer->token.name = TokenNameComma;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == '{') {
        lexer->token.name = TokenNameLeftCur;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == '}') {
        lexer->token.name = TokenNameRightCur;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == '=') {
        lexer->token.name = TokenNameEquals;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == '!') {
        lexer->token.length = 1;
        lexer->token.string = lexer->stream;
        ++lexer->stream;
        ++lexer->column;
        if (lexer->stream[0] == '=') {
            lexer->token.name = TokenNameExclamationMarkEquals;
            ++lexer->token.length;
            ++lexer->stream;
            ++lexer->column;
        } else {
            lexer->token.name = TokenNameExclamationMark;
        }
        return true;
    }
    if (lexer->stream[0] == '<') {
        if (lexer->stream[1] == '<') { // <<
            lexer->token.name = TokenNameRedirect;
            lexer->token.length = 2;
            lexer->token.string = lexer->stream;
            lexer->stream += 2;
            lexer->column += 2;
        } else if (lexer->stream[1] == '=') { // <=
            lexer->token.name = TokenNameSmallerOrEqual;
            lexer->token.length = 2;
            lexer->token.string = lexer->stream;
            lexer->stream += 2;
            lexer->column += 2;
        } else { // <
            lexer->token.name = TokenNameSmallerThan;
            lexer->token.length = 1;
            lexer->token.string = lexer->stream++;
            ++lexer->column;
        }
        return true;
    }
    if (lexer->stream[0] == '>') {
        if (lexer->stream[1] == '=') {
            lexer->token.name = TokenNameBiggerOrEqual;
            lexer->token.length = 2;
            lexer->token.string = lexer->stream;
            lexer->stream += 2;
            lexer->column += 2;
        } else {
            lexer->token.name = TokenNameBiggerThan;
            lexer->token.length = 1;
            lexer->token.string = lexer->stream++;
            ++lexer->column;
        }
        return true;
    }
    if (lexer->stream[0] == '^') {
        lexer->token.name = TokenNameCaret;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == '&') {
        lexer->token.name = TokenNameAmpersand;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == '|') {
        lexer->token.name = TokenNamePipe;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }

#define ADD_KEYWORD(keyword, token_name)                                                         \
        do {                                                                                     \
            if (lexer_match_keyword(lexer, keyword, sizeof(keyword)/sizeof(char)-1, token_name)) \
                return true;                                                                     \
        } while (0)
    // try to lex keywords
    ADD_KEYWORD("log", TokenNameLog);
    ADD_KEYWORD("routine", TokenNameRoutine);
    ADD_KEYWORD("if", TokenNameIf);
    ADD_KEYWORD("else", TokenNameElse);
    ADD_KEYWORD("loop", TokenNameLoop);
    ADD_KEYWORD("Void", TokenNameVoid);
    ADD_KEYWORD("at", TokenNameAt);
    ADD_KEYWORD("sizeof", TokenNameSizeof);
    ADD_KEYWORD("typeof", TokenNameTypeof);
    ADD_KEYWORD("through", TokenNameThrough);
    ADD_KEYWORD("to", TokenNameTo);
    ADD_KEYWORD("blame", TokenNameBlame);
    ADD_KEYWORD("catch", TokenNameCatch);
    ADD_KEYWORD("with", TokenNameWith);
    ADD_KEYWORD("show", TokenNameShow);
    ADD_KEYWORD("getstring", TokenNameGetString);
    ADD_KEYWORD("match", TokenNameMatch);
    ADD_KEYWORD("skip", TokenNameSkip);
    ADD_KEYWORD("break", TokenNameBreak);
    ADD_KEYWORD("load", TokenNameLoad);
#undef ADD_KEYWORD

    // if you couldn't match any token, error
    lexer_error(lexer, "Unrecognized token");
    return false;
}

char *token_allocate_key(struct Token* const token) {
    char *key;
    assert(token->name == TokenNameKey);
    key = malloc((token->length + 1) * sizeof(char));
    strncpy(key, token->string, token->length);
    key[token->length] = '\0';
    return key;
}
