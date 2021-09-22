#ifndef RAEL_LEXER_H
#define RAEL_LEXER_H

#include <stddef.h>
#include <stdbool.h>

enum TokenName {
    TokenNameString = 1,
    TokenNameNumber,
    TokenNameNewline,
    TokenNameLog,
    TokenNameKey,
    TokenNameAdd,
    TokenNameSub,
    TokenNameMul,
    TokenNameDiv,
    TokenNameLeftParen,
    TokenNameRightParen,
    TokenNameRoutine,
    TokenNameComma,
    TokenNameLeftCur,
    TokenNameRightCur,
    TokenNameEquals,
    TokenNameSmallerThan,
    TokenNameBiggerThan,
    TokenNameIf,
    TokenNameElse,
    TokenNameLoop,
    TokenNameVoid,
    TokenNameCaret,
    TokenNameAt,
    TokenNameRedirect,
    TokenNameSizeof,
    TokenNameThrough,
    TokenNameSemicolon,
    TokenNameTo
};

struct Token {
    enum TokenName name;
    char *string;
    size_t length;
};

struct Lexer {
    struct Token token;
    char *stream;
    size_t line;
    size_t column;
};

struct State {
    char *stream_pos;
    size_t line, column;
};

bool lexer_tokenize(struct Lexer* const lexer);

struct State lexer_dump_state(struct Lexer* const lexer);

void lexer_load_state(struct Lexer* const lexer, struct State state);

char *token_allocate_key(struct Token* const token);

#endif // RAEL_LEXER_H
