#include "lexer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

void rael_error(struct State state, const char* const error_message);

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

static inline void lexer_error(struct Lexer* const lexer, const char* const error_message) {
    rael_error(lexer_dump_state(lexer), error_message);
}

static inline bool is_identifier_char(const char c) {
    return isalnum(c) || c == '_';
}

static bool lexer_clean(struct Lexer* const lexer) {
    bool cleaned = false;
    while (true) {
        if (lexer->stream[0] == ' ' || lexer->stream[0] == '\t') {
            cleaned = true;
            ++lexer->stream;
            ++lexer->column;
        } else if (lexer->stream[0] == '%' && lexer->stream[1] == '%') { // handle comments :^)
            cleaned = true;
            lexer->stream += 2;
            lexer->column += 2;
            while (lexer->stream[0] != '\n' && lexer->stream[0] != '\0') {
                ++lexer->stream;
                ++lexer->column;
            }
        } else {
            break;
        }
    }
    return cleaned;
}

static bool lexer_match_keyword(struct Lexer* const lexer, const char* const keyword,
                                        const size_t length, const enum TokenName name) {
    if (strncmp(lexer->stream, keyword, length) == 0) {
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
    if (lexer->stream[0] == '+') {
        lexer->token.name = TokenNameAdd;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == '-') {
        lexer->token.name = TokenNameSub;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == '*') {
        lexer->token.name = TokenNameMul;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == '/') {
        lexer->token.name = TokenNameDiv;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == '%') {
        lexer->token.name = TokenNameMod;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
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
    if (lexer->stream[0] == '<') {
        if (lexer->stream[1] == '<') {
            lexer->token.name = TokenNameRedirect;
            lexer->token.length = 2;
            lexer->token.string = lexer->stream;
            lexer->stream += 2;
            lexer->column += 2;
        } else {
            lexer->token.name = TokenNameSmallerThan;
            lexer->token.length = 1;
            lexer->token.string = lexer->stream++;
            ++lexer->column;
        }
        return true;
    }
    if (lexer->stream[0] == '>') {
        lexer->token.name = TokenNameBiggerThan;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == '^') {
        lexer->token.name = TokenNameCaret;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    if (lexer->stream[0] == ';') {
        lexer->token.name = TokenNameSemicolon;
        lexer->token.length = 1;
        lexer->token.string = lexer->stream++;
        ++lexer->column;
        return true;
    }
    // try to tokenize `log`
    if (lexer_match_keyword(lexer, "log", 3, TokenNameLog))
        return true;
    // try to tokenize `routine`
    if (lexer_match_keyword(lexer, "routine", 7, TokenNameRoutine))
        return true;
    // try to tokenize `if`
    if (lexer_match_keyword(lexer, "if", 2, TokenNameIf))
        return true;
    // try to tokenize `else`
    if (lexer_match_keyword(lexer, "else", 4, TokenNameElse))
        return true;
    // try to tokenize `loop`
    if (lexer_match_keyword(lexer, "loop", 4, TokenNameLoop))
        return true;
    // try to tokenize `token`
    if (lexer_match_keyword(lexer, "Void", 4, TokenNameVoid))
        return true;
    // try to tokenize `at`
    if (lexer_match_keyword(lexer, "at", 2, TokenNameAt))
        return true;
    // try to tokenize `sizeof`
    if (lexer_match_keyword(lexer, "sizeof", 6, TokenNameSizeof))
        return true;
    // try to tokenize `through`
    if (lexer_match_keyword(lexer, "through", 7, TokenNameThrough))
        return true;
    // try to tokenize `to`
    if (lexer_match_keyword(lexer, "to", 2, TokenNameTo))
        return true;
    // try to tokenize `blame`
    if (lexer_match_keyword(lexer, "blame", 5, TokenNameBlame))
        return true;
    // try to tokenize `catch`
    if (lexer_match_keyword(lexer, "catch", 5, TokenNameCatch))
        return true;
    // try to tokenize `with`
    if (lexer_match_keyword(lexer, "with", 4, TokenNameWith))
        return true;
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
