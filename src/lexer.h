#ifndef RAEL_LEXER_H
#define RAEL_LEXER_H

#include "common.h"

#include <stddef.h>
#include <stdbool.h>

enum TokenName {
    TokenNameString = 1,
    TokenNameNumber,
    TokenNameNewline,
    TokenNameLog,
    TokenNameShow,
    TokenNameKey,
    TokenNameAdd,
    TokenNameSub,
    TokenNameMul,
    TokenNameDiv,
    TokenNameMod,
    TokenNameLeftParen,
    TokenNameRightParen,
    TokenNameRoutine,
    TokenNameComma,
    TokenNameLeftCur,
    TokenNameRightCur,
    TokenNameEquals,
    TokenNameSmallerThan,
    TokenNameBiggerThan,
    TokenNameSmallerOrEqual,
    TokenNameBiggerOrEqual,
    TokenNameIf,
    TokenNameElse,
    TokenNameLoop,
    TokenNameVoid,
    TokenNameCaret,
    TokenNameAt,
    TokenNameRedirect,
    TokenNameSizeof,
    TokenNameThrough,
    TokenNameBreak,
    TokenNameSkip,
    TokenNameTo,
    TokenNameBlame,
    TokenNameCatch,
    TokenNameWith,
    TokenNameAmpersand,
    TokenNamePipe,
    TokenNameExclamationMark,
    TokenNameTypeof,
    TokenNameGetString,
    TokenNameMatch,
    TokenNameLoad,
    TokenNameQuestionEquals,
    TokenNamePlusEquals,
    TokenNameMinusEquals,
    TokenNameStarEquals,
    TokenNameSlashEquals,
    TokenNamePercentEquals,
    TokenNameExclamationMarkEquals
};

struct Token {
    enum TokenName name;
    char *string;
    size_t length;
};

struct Lexer {
    struct Token token;
    RaelStream stream;
    size_t line;
    size_t column;
};

/* constructs the lexer pointer */
void lexer_construct(struct Lexer *lexer, RaelStream stream);

bool lexer_tokenize(struct Lexer* const lexer);

struct State lexer_dump_state(struct Lexer* const lexer);

void lexer_load_state(struct Lexer* const lexer, struct State state);

char *token_allocate_key(struct Token* const token);

#endif // RAEL_LEXER_H
