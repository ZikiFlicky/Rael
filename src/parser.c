#include "parser.h"
#include "lexer.h"
#include "number.h"
#include "value.h"
#include "common.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static struct Expr *parser_parse_expr(struct Parser* const parser);
static struct Instruction *parser_parse_instr(struct Parser* const parser);
static struct ASTValue *parser_parse_instr_routine(struct Parser* const parser);
static struct Instruction **parser_parse_block(struct Parser* const parser);
static struct RaelExprList parser_parse_csv(struct Parser* const parser, const bool allow_newlines);
static struct Expr *parser_parse_expr_at(struct Parser* const parser);
static void expr_delete(struct Expr* const expr);

static void parser_state_error(struct Parser* const parser, struct State state, const char* const error_message) {
    rael_show_error_message(state, error_message);

    // destroy all of the parsed instructions
    for (size_t i = 0; i < parser->idx; ++i)
        instruction_delete(parser->instructions[i]);

    free(parser->instructions);
    if (parser->lexer.stream_on_heap)
        free(parser->lexer.stream_base);

    exit(1);
}

static inline void parser_error(struct Parser* const parser, const char* const error_message) {
    parser_state_error(parser, lexer_dump_state(&parser->lexer), error_message);
}

static void parser_push(struct Parser* const parser, struct Instruction* const inst) {
    if (parser->allocated == 0) {
        parser->instructions = malloc(((parser->allocated = 32)+1) * sizeof(struct Instruction*));
    } else if (parser->idx == 32) {
        parser->instructions = realloc(parser->instructions, (parser->allocated += 32)+1 * sizeof(struct Instruction*));
    }
    parser->instructions[parser->idx++] = inst;
}

static bool parser_match(struct Parser* const parser, enum TokenName token) {
    struct State backtrack = lexer_dump_state(&parser->lexer);

    if (!lexer_tokenize(&parser->lexer))
        return false;

    if (parser->lexer.token.name != token) {
        lexer_load_state(&parser->lexer, backtrack);
        return false;
    }

    return true;
}

static void parser_expect_newline(struct Parser* const parser) {
    if (lexer_tokenize(&parser->lexer)) {
        if (parser->lexer.token.name != TokenNameNewline) {
            parser_error(parser, "Expected newline after expression");
        }
    }
}

static bool parser_maybe_expect_newline(struct Parser* const parser) {
    struct State backtrack = lexer_dump_state(&parser->lexer);
    if (lexer_tokenize(&parser->lexer)) {
        if (parser->lexer.token.name != TokenNameNewline) {
            lexer_load_state(&parser->lexer, backtrack);
            return false;
        }
    }
    return true;
}

static struct ASTValue *parser_parse_stack(struct Parser* const parser) {
    struct ASTValue *value;
    struct RaelExprList stack;
    struct State backtrack = lexer_dump_state(&parser->lexer);

    if (!parser_match(parser, TokenNameLeftCur))
        return NULL;

    stack = parser_parse_csv(parser, true);

    if (!parser_match(parser, TokenNameRightCur)) {
        lexer_load_state(&parser->lexer, backtrack);
        for (size_t i = 0; i < stack.amount_exprs; ++i)
            expr_delete(stack.exprs[i]);
        return NULL;
    }

    value = malloc(sizeof(struct ASTValue));
    value->type = ValueTypeStack;
    value->as_stack = stack;
    return value;
}

static struct ASTValue *parser_parse_number(struct Parser* const parser) {
    struct ASTValue *value;
    struct NumberExpr number;
    double as_float = 0.f;
    int as_int = 0;

    number.is_float = false;

    if (!parser_match(parser, TokenNameNumber))
        return NULL;

    value = malloc(sizeof(struct ASTValue));
    for (size_t i = 0; i < parser->lexer.token.length; ++i) {
        if (parser->lexer.token.string[i] == '.') {
            if (!number.is_float) {
                number.is_float = true;
                continue;
            }
        }
        if (!number.is_float) {
            as_int *= 10;
            as_int += parser->lexer.token.string[i] - '0';
        }
        as_float *= 10;
        as_float += parser->lexer.token.string[i] - '0';
        if (number.is_float)
            as_float /= 10;
    }

    if (number.is_float)
        number.as_float = as_float;
    else
        number.as_int = as_int;

    value->type = ValueTypeNumber;
    value->as_number = number;

    return value;
}

static struct Expr *parser_parse_literal_expr(struct Parser* const parser) {
    struct Expr *expr;
    struct State backtrack = lexer_dump_state(&parser->lexer);
    struct ASTValue *value;

    if ((value = parser_parse_instr_routine(parser)) ||
        (value = parser_parse_stack(parser))        ||
        (value = parser_parse_number(parser))) {
        expr = malloc(sizeof(struct Expr));
        expr->type = ExprTypeValue;
        expr->as_value = value;
        return expr;
    }

    if (!lexer_tokenize(&parser->lexer))
        return NULL;

    switch (parser->lexer.token.name) {
    case TokenNameString: {
        char *string = NULL;
        size_t allocated = 0, length = 0;

        for (size_t i = 0; i < parser->lexer.token.length; ++i) {
            char c;
            if (parser->lexer.token.string[i] == '\\') {
                // there isn't really a reason to have more than this
                switch (parser->lexer.token.string[++i]) {
                case 'n':
                    c = '\n';
                    break;
                case 'r':
                    c = '\r';
                    break;
                case 't':
                    c = '\t';
                    break;
                case '\\':
                    c = '\\';
                    break;
                default:
                    c = parser->lexer.token.string[i];
                }
            } else {
                c = parser->lexer.token.string[i];
            }

            if (allocated == 0) {
                string = malloc((allocated = 16) * sizeof(char));
            } else if (length == allocated) {
                string = realloc(string, (allocated += 16) * sizeof(char));
            }

            string[length++] = c;
        }

        // shrink size
        if (allocated > 0)
            string = realloc(string, length * sizeof(char));

        expr = malloc(sizeof(struct Expr));
        expr->type = ExprTypeValue;
        expr->as_value = malloc(sizeof(struct ASTValue));
        expr->as_value->type = ValueTypeString;
        expr->as_value->as_string = (struct RaelStringValue) {
            .value = string,
            .length = length
        };

        return expr;
    }
    case TokenNameKey: {
        char *key;
        expr = malloc(sizeof(struct Expr));
        key = token_allocate_key(&parser->lexer.token);
        expr->as_key = key;
        expr->type = ExprTypeKey;
        return expr;
    }
    case TokenNameVoid:
        expr = malloc(sizeof(struct Expr));
        expr->type = ExprTypeValue;
        expr->as_value = malloc(sizeof(struct ASTValue));
        expr->as_value->type = ValueTypeVoid;
        return expr;
    default:
        lexer_load_state(&parser->lexer, backtrack);
        return NULL;
    }
}

static struct Expr *parser_parse_expr_set(struct Parser* const parser) {
    struct Expr *expr, *at_set, *full_expression;
    struct State backtrack = lexer_dump_state(&parser->lexer);

    if ((at_set = parser_parse_expr_at(parser)) && at_set->type == ExprTypeAt) {
        // parse rhs
        if (!(expr = parser_parse_expr(parser))) {
            expr_delete(at_set);
            lexer_load_state(&parser->lexer, backtrack);
            return NULL;
        }

        full_expression = malloc(sizeof(struct Expr));
        *full_expression = (struct Expr) {
            .type = ExprTypeSet,
            .as_set = {
                .set_type = SetTypeAtExpr,
                .as_at_stat = at_set,
                .expr = expr
            }
        };
    } else {
        struct Token key_token;
        lexer_load_state(&parser->lexer, backtrack);

        if (at_set)
            expr_delete(at_set);

        if (!parser_match(parser, TokenNameKey))
            return NULL;
        key_token = parser->lexer.token;
        // expect an expression after key
        if (!(expr = parser_parse_expr(parser))) {
            lexer_load_state(&parser->lexer, backtrack);
            return NULL;
        }

        full_expression = malloc(sizeof(struct Expr));
        *full_expression = (struct Expr) {
            .type = ExprTypeSet,
            .as_set = {
                .set_type = SetTypeKey,
                .as_key = token_allocate_key(&key_token),
                .expr = expr
            }
        };
    }

    return full_expression;
}

static struct Expr *parser_parse_expr_single(struct Parser* const parser) {
    struct Expr *expr;
    struct State backtrack = lexer_dump_state(&parser->lexer);;

    if (!(expr = parser_parse_literal_expr(parser))) {
        if (!lexer_tokenize(&parser->lexer))
            return NULL;

        switch (parser->lexer.token.name) {
        case TokenNameSub: {
            struct Expr *to_negative;

            if (!(to_negative = parser_parse_expr_single(parser)))
                parser_error(parser, "Expected an expression after or before '-'");

            expr = malloc(sizeof(struct Expr));
            expr->type = ExprTypeNeg;
            expr->as_single = to_negative;
            break;
        }
        case TokenNameSizeof: {
            struct Expr *sizeof_value;

            if (!(sizeof_value = parser_parse_expr_single(parser))) {
                parser_error(parser, "Expected value after 'sizeof'");
            }

            expr = malloc(sizeof(struct Expr));
            expr->type = ExprTypeSizeof;
            expr->as_single = sizeof_value;
            break;
        }
        case TokenNameLeftParen: {
            struct State last_state = lexer_dump_state(&parser->lexer);

            if (!(expr = parser_parse_expr(parser)))
                parser_error(parser, "Expected an expression after left paren");

            // expect a ')' after an expression, but if there isn't, it might be a set expression
            if (!parser_match(parser, TokenNameRightParen)) {
                struct State after_expr_state = lexer_dump_state(&parser->lexer);

                // reset position to after left paren
                lexer_load_state(&parser->lexer, last_state);
                expr_delete(expr);

                if (!(expr = parser_parse_expr_set(parser)))
                    parser_state_error(parser, after_expr_state, "Unmatched '('");

                if (!parser_match(parser, TokenNameRightParen))
                    parser_error(parser, "Unmatched '('");
            }

            break;
        }
        case TokenNameBlame:
            expr = malloc(sizeof(struct Expr));
            expr->type = ExprTypeBlame;
            expr->as_single = parser_parse_expr_single(parser);
            break;
        default:
            lexer_load_state(&parser->lexer, backtrack);
            return NULL;
        }
    }

    expr->state = backtrack;
    return expr;
}


/** routine call is:
 *    :key(arguments, seperated, by, a, comma)
 *  example:
 *    :add(5, 8)
 **/
static struct Expr *parser_parse_routine_call(struct Parser* const parser) {
    // FIXME: find a more elegant solution
    struct Expr *expr;

    if (!(expr = parser_parse_expr_single(parser)))
        return NULL;

    for (;;) {
        struct RoutineCallExpr call;
        struct State backtrack, start_backtrack;

        start_backtrack = backtrack = lexer_dump_state(&parser->lexer);

        // verify there is '(' after the key?
        if (!parser_match(parser, TokenNameLeftParen))
            break;

        call.routine_value = expr;
        call.arguments = parser_parse_csv(parser, true);

        backtrack = lexer_dump_state(&parser->lexer);
        if (!parser_match(parser, TokenNameRightParen))
            parser_state_error(parser, backtrack, "Expected a ')'");

        expr = malloc(sizeof(struct Expr));
        expr->type = ExprTypeRoutineCall;
        expr->as_call = call;
        expr->state = start_backtrack;
    }

    return expr;
}

static struct Expr *parser_parse_expr_product(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_routine_call(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = lexer_dump_state(&parser->lexer);

        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameMul:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeMul;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_routine_call(parser))) {
                    parser_state_error(parser, backtrack, "Expected a value after '*'");
                }
                break;
            case TokenNameDiv:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeDiv;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_routine_call(parser))) {
                    parser_state_error(parser, backtrack, "Expected a value after '/'");
                }
                break;
            case TokenNameMod:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeMod;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_routine_call(parser))) {
                    parser_state_error(parser, backtrack, "Expected a value after '%'");
                }
                break;
            default:
                lexer_load_state(&parser->lexer, backtrack);
                goto loop_end;
            } 
        } else {
            break;
        }

        new_expr->state = backtrack;
        expr = new_expr;
    }
loop_end:
    return expr;
}

static struct Expr *parser_parse_expr_sum(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_product(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = lexer_dump_state(&parser->lexer);

        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameAdd:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeAdd;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_product(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '+'");
                break;
            case TokenNameSub:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeSub;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_product(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '-'");
                break;
            default:
                lexer_load_state(&parser->lexer, backtrack);
                goto loop_end;
            }
        } else {
            break;
        }

        new_expr->state = backtrack;
        expr = new_expr;
    }
loop_end:
    return expr;
}


static struct Expr *parser_parse_expr_range(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_sum(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = lexer_dump_state(&parser->lexer);

        if (lexer_tokenize(&parser->lexer)) {
            if (parser->lexer.token.name == TokenNameTo) {
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeTo;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_sum(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after 'to'");
            } else {
                lexer_load_state(&parser->lexer, backtrack);
                break;
            }
        } else {
            break;
        }

        new_expr->state = backtrack;
        expr = new_expr;
    }
    return expr;
}

static struct Expr *parser_parse_expr_comparison(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_range(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = lexer_dump_state(&parser->lexer);

        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameEquals:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeEquals;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_range(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '='");
                break;
            case TokenNameSmallerThan:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeSmallerThen;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_range(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '<'");
                break;
            case TokenNameBiggerThan:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeBiggerThen;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_range(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '>'");
                break;
            default:
                lexer_load_state(&parser->lexer, backtrack);
                goto loop_end;
            }
        } else {
            break;
        }

        new_expr->state = backtrack;
        expr = new_expr;
    }
loop_end:
    return expr;
}

static struct Expr *parser_parse_expr_at(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_comparison(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = lexer_dump_state(&parser->lexer);

        if (lexer_tokenize(&parser->lexer)) {
            if (parser->lexer.token.name == TokenNameAt) {
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeAt;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_comparison(parser))) {
                    parser_state_error(parser, backtrack, "Expected a value after 'at'");
                }
            } else {
                lexer_load_state(&parser->lexer, backtrack);
                break;
            }
        } else {
            break;
        }

        new_expr->state = backtrack;
        expr = new_expr;
    }
    return expr;
}

static struct Expr *parser_parse_expr_and(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_at(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = lexer_dump_state(&parser->lexer);

        if (lexer_tokenize(&parser->lexer)) {
            if (parser->lexer.token.name == TokenNameAmpersand) {
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeAnd;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_at(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '&'");
            } else {
                lexer_load_state(&parser->lexer, backtrack);
                break;
            }
        } else {
            break;
        }

        new_expr->state = backtrack;
        expr = new_expr;
    }
    return expr;
}

static struct Expr *parser_parse_expr_or(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_and(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = lexer_dump_state(&parser->lexer);

        if (lexer_tokenize(&parser->lexer)) {
            if (parser->lexer.token.name == TokenNamePipe) {
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeOr;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_and(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '|'");
            } else {
                lexer_load_state(&parser->lexer, backtrack);
                break;
            }
        } else {
            break;
        }

        new_expr->state = backtrack;
        expr = new_expr;
    }
    return expr;
}

static struct Expr *parser_parse_expr(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_or(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = lexer_dump_state(&parser->lexer);

        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameRedirect:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeRedirect;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_or(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '<<'");
                break;
            default:
                lexer_load_state(&parser->lexer, backtrack);
                goto loop_end;
            }
        } else {
            break;
        }

        new_expr->state = backtrack;
        expr = new_expr;
    }
loop_end:
    return expr;
}

static struct Instruction *parser_parse_instr_pure(struct Parser* const parser) {
    struct Instruction *inst;
    struct Expr *expr;
    struct State backtrack = lexer_dump_state(&parser->lexer);

    if ((expr = parser_parse_expr(parser))) {
        if (parser_maybe_expect_newline(parser))
            goto end;
        expr_delete(expr);
        lexer_load_state(&parser->lexer, backtrack);
    }

    if ((expr = parser_parse_expr_set(parser))) {
        if (parser_maybe_expect_newline(parser))
            goto end;
        expr_delete(expr);
        lexer_load_state(&parser->lexer, backtrack);
    }

    return NULL;
end:
    inst = malloc(sizeof(struct Instruction));
    inst->type = InstructionTypePureExpr;
    inst->pure = expr;
    return inst;
}

/*
  parse comma seperated expressions
*/
static struct RaelExprList parser_parse_csv(struct Parser* const parser, const bool allow_newlines) {
    struct Expr **exprs_ary, *expr;
    size_t allocated, idx = 0;

    if (allow_newlines)
        parser_maybe_expect_newline(parser);

    if (!(expr = parser_parse_expr(parser))) {
        return (struct RaelExprList) {
            .amount_exprs = 0,
            .exprs = NULL
        };
    }

    exprs_ary = malloc((allocated = 1) * sizeof(struct Expr*));
    exprs_ary[idx++] = expr;

    for (;;) {
        if (allow_newlines)
            parser_maybe_expect_newline(parser);

        if (!parser_match(parser, TokenNameComma))
            break;

        if (allow_newlines)
            parser_maybe_expect_newline(parser);
        if ((expr = parser_parse_expr(parser))) {
            if (idx == allocated) {
                exprs_ary = realloc(exprs_ary, (allocated += 4) * sizeof(struct Expr *));
            }
            exprs_ary[idx++] = expr;
        } else {
            parser_error(parser, "Expected expression");
        }
        if (allow_newlines)
            parser_maybe_expect_newline(parser);
    }

    return (struct RaelExprList) {
        .amount_exprs = idx,
        .exprs = exprs_ary
    };
}

static struct Instruction *parser_parse_instr_catch(struct Parser* const parser) {
    struct State backtrack = lexer_dump_state(&parser->lexer);
    struct Instruction *inst;
    struct CatchInstruction catch;

    if (!parser_match(parser, TokenNameCatch))
        return NULL;

    if (!(catch.catch_expr = parser_parse_expr(parser))) {
        parser_error(parser, "Expected expression");
    }

    parser_maybe_expect_newline(parser);

    if (!parser_match(parser, TokenNameWith))
        parser_state_error(parser, backtrack, "Expected 'with'");

    parser_maybe_expect_newline(parser);

    if (!(catch.handle_block = parser_parse_block(parser))) {
        parser_error(parser, "Expected block");
    }

    parser_maybe_expect_newline(parser);

    inst = malloc(sizeof(struct Instruction));
    inst->type = InstructionTypeCatch;
    inst->catch = catch;

    return inst;
}

static struct Instruction *parser_parse_instr_log(struct Parser* const parser) {
    struct Instruction *inst;
    struct RaelExprList expr_list;

    if (!parser_match(parser, TokenNameLog))
        return NULL;

    if ((expr_list = parser_parse_csv(parser, false)).amount_exprs == 0)
        parser_error(parser, "Expected at least one expression after \"log\"");

    parser_expect_newline(parser);

    inst = malloc(sizeof(struct Instruction));
    inst->type = InstructionTypeLog;
    inst->csv = expr_list;

    return inst;
}

static struct Instruction *parser_parse_instr_show(struct Parser* const parser) {
    struct Instruction *inst;
    struct RaelExprList expr_list;

    if (!parser_match(parser, TokenNameShow))
        return NULL;

    if ((expr_list = parser_parse_csv(parser, false)).amount_exprs == 0)
        parser_error(parser, "Expected at least one expression after \"show\"");

    parser_expect_newline(parser);

    inst = malloc(sizeof(struct Instruction));
    inst->type = InstructionTypeShow;
    inst->csv = expr_list;

    return inst;
}

static struct Instruction *parser_parse_instr_return(struct Parser* const parser) {
    struct Instruction *inst;
    struct Expr *expr;

    if (!parser_match(parser, TokenNameCaret))
        return NULL;

    if (parser_maybe_expect_newline(parser)) {
        expr = NULL;
    } else if ((expr = parser_parse_expr(parser))) {
        parser_expect_newline(parser);
    } else {
        parser_error(parser, "Expected an expression or nothing after \"^\"");
    }

    inst = malloc(sizeof(struct Instruction));
    inst->type = InstructionTypeReturn;
    inst->return_value = expr;
    return inst;
}

static struct Instruction *parser_parse_instr_break(struct Parser* const parser) {
    struct Instruction *inst;

    if (!parser_match(parser, TokenNameSemicolon))
        return NULL;

    parser_expect_newline(parser);

    inst = malloc(sizeof(struct Instruction));
    inst->type = InstructionTypeBreak;
    return inst;
}

static struct Instruction **parser_parse_block(struct Parser* const parser) {
    struct Instruction **block;
    size_t allocated, idx = 0;

    if (!parser_match(parser, TokenNameLeftCur))
        return NULL;

    parser_maybe_expect_newline(parser);
    block = malloc(((allocated = 32)+1) * sizeof(struct Instruction *));
    while (true) {
        struct Instruction *inst;
        if (!(inst = parser_parse_instr(parser))) {
            if (!lexer_tokenize(&parser->lexer))
                parser_error(parser, "Unmatched '}'");
            if (parser->lexer.token.name == TokenNameRightCur) {
                break;
            } else {
                parser_error(parser, "Unexpected token");
            }
        }
        if (idx == allocated) {
            block = realloc(block, ((allocated += 32)+1) * sizeof(struct Instruction *));
        }
        block[idx++] = inst;
    }

    block[idx++] = NULL;
    block = realloc(block, idx * sizeof(struct Instruction *));

    return block;
}

static struct ASTValue *parser_parse_instr_routine(struct Parser* const parser) {
    struct ASTValue *value;
    struct RaelRoutineValue decl;
    struct State backtrack;

    if (!parser_match(parser, TokenNameRoutine))
        return NULL;

    if (!parser_match(parser, TokenNameLeftParen))
        parser_state_error(parser, backtrack, "Expected '(' after 'routine'");

    backtrack = lexer_dump_state(&parser->lexer);

    if (!lexer_tokenize(&parser->lexer))
        parser_error(parser, "Unexpected EOF");

    switch (parser->lexer.token.name) {
    case TokenNameRightParen:
        decl.amount_parameters = 0;
        decl.parameters = NULL;
        break;
    case TokenNameKey: {
        size_t allocated;

        decl.parameters = malloc((allocated = 4) * sizeof(struct Expr*));
        decl.amount_parameters = 0;
        decl.parameters[decl.amount_parameters++] = token_allocate_key(&parser->lexer.token);

        for (;;) {
            backtrack = lexer_dump_state(&parser->lexer);

            if (!lexer_tokenize(&parser->lexer))
                parser_error(parser, "Unexpected EOF");

            if (parser->lexer.token.name == TokenNameRightParen)
                break;

            if (parser->lexer.token.name != TokenNameComma) {
                parser_state_error(parser, backtrack, "Expected a Comma");
            }

            backtrack = lexer_dump_state(&parser->lexer);
            if (!parser_match(parser, TokenNameKey))
                parser_state_error(parser, backtrack, "Expected key");

            if (decl.amount_parameters == allocated)
                decl.parameters = realloc(decl.parameters, (allocated += 3) * sizeof(struct Expr*));

            // verify there are no duplicate parameters
            for (size_t parameter = 0; parameter < decl.amount_parameters; ++parameter) {
                if (strncmp(parser->lexer.token.string, decl.parameters[parameter], parser->lexer.token.length) == 0) {
                    parser_state_error(parser, backtrack, "Duplicate parameter on routine decleration");
                }
            }
            decl.parameters[decl.amount_parameters++] = token_allocate_key(&parser->lexer.token);
        }
        break;
    }
    default:
        parser_state_error(parser, backtrack, "Expected key");
    }

    if (!(decl.block = parser_parse_block(parser))) {
        parser_error(parser, "Expected block after routine decleration");
    }

    value = malloc(sizeof(struct ASTValue));
    value->type = ValueTypeRoutine;
    value->as_routine = decl;
    return value;
}

static struct Instruction *parser_parse_if_statement(struct Parser* const parser) {
    struct Instruction *inst;
    struct IfInstruction if_stat = {
        .block = NULL,
        .else_type = ElseTypeNone,
        .condition = NULL
    };

    if (!parser_match(parser, TokenNameIf))
        return NULL;

    if (!(if_stat.condition = parser_parse_expr(parser)))
        parser_error(parser, "Expected an expression after if keyword");

    if (!(if_stat.block = parser_parse_block(parser)))
        parser_error(parser, "Expected block after if (expr)");

    parser_maybe_expect_newline(parser);

    inst = malloc(sizeof(struct Instruction));
    inst->type = InstructionTypeIf;

    if (!parser_match(parser, TokenNameElse))
        goto end;

    parser_maybe_expect_newline(parser);

    if ((if_stat.else_block = parser_parse_block(parser))) {
        if_stat.else_type = ElseTypeBlock;
    } else if ((if_stat.else_instruction = parser_parse_instr(parser))) {
        if_stat.else_type = ElseTypeInstruction;
    } else {
        parser_error(parser, "Expected a block or an instruction after 'else' keyword");
    }

    parser_maybe_expect_newline(parser);
end:
    inst->if_stat = if_stat;
    return inst;
}

static struct Instruction *parser_parse_loop(struct Parser* const parser) {
    struct Instruction *inst;
    struct LoopInstruction loop;
    struct State backtrack;

    if (!parser_match(parser, TokenNameLoop))
        return NULL;

    if ((loop.block = parser_parse_block(parser))) {
        loop.type = LoopForever;
        goto loop_parsing_end;
    }

    backtrack = lexer_dump_state(&parser->lexer);

    if (parser_match(parser, TokenNameKey)) {
        struct Token key_token = parser->lexer.token;

        if (parser_match(parser, TokenNameThrough)) {
            if (!(loop.iterate.expr = parser_parse_expr(parser)))
                parser_error(parser, "Expected an expression after 'through'");

            loop.type = LoopThrough;
            loop.iterate.key = token_allocate_key(&key_token);
        } else {
            lexer_load_state(&parser->lexer, backtrack);
            // we already know there is a key, it must parse at least a key,
            // if not a full expression
            loop.while_condition = parser_parse_expr(parser);
            assert(loop.while_condition);
            loop.type = LoopWhile;
        }
    } else {
        if (!(loop.while_condition = parser_parse_expr(parser)))
            parser_error(parser, "Expected expression after loop");

        loop.type = LoopWhile;
    }

    if (!(loop.block = parser_parse_block(parser)))
        parser_error(parser, "Expected block after loop");

loop_parsing_end:
    parser_maybe_expect_newline(parser);
    inst = malloc(sizeof(struct Instruction));
    inst->type = InstructionTypeLoop;
    inst->loop = loop;
    return inst;
}

static struct Instruction *parser_parse_instr(struct Parser* const parser) {
    struct Instruction *inst;
    struct State prev_state = lexer_dump_state(&parser->lexer);
    if ((inst = parser_parse_instr_pure(parser))    ||
        (inst = parser_parse_instr_log(parser))     ||
        (inst = parser_parse_if_statement(parser))  ||
        (inst = parser_parse_loop(parser))          ||
        (inst = parser_parse_instr_return(parser))  ||
        (inst = parser_parse_instr_break(parser))   ||
        (inst = parser_parse_instr_catch(parser))   ||
        (inst = parser_parse_instr_show(parser))) {
        inst->state = prev_state;
        return inst;
    }
    return NULL;
}

struct Instruction **rael_parse(char* const stream, bool stream_on_heap) {
    struct Instruction *inst;
    struct Parser parser = {
        .lexer = {
            .line = 1,
            .column = 1,
            .stream = stream,
            .stream_base = stream,
            .stream_on_heap = stream_on_heap
        }
    };
    parser_maybe_expect_newline(&parser);
    while ((inst = parser_parse_instr(&parser))) {
        parser_push(&parser, inst);
    }
    parser_push(&parser, NULL);

    if (lexer_tokenize(&parser.lexer))
        parser_error(&parser, "Syntax Error");
    return parser.instructions;
}

static void expr_delete(struct Expr* const expr);

static void astvalue_delete(struct ASTValue* value) {
    switch (value->type) {
    case ValueTypeVoid:
    case ValueTypeNumber:
        break;
    case ValueTypeString:
        if (value->as_string.length > 0)
            free(value->as_string.value);
        break;
    case ValueTypeRoutine:
        for (size_t i = 0; i < value->as_routine.amount_parameters; ++i) {
            free(value->as_routine.parameters[i]);
        }

        free(value->as_routine.parameters);

        for (size_t i = 0; value->as_routine.block[i]; ++i) {
            instruction_delete(value->as_routine.block[i]);
        }

        free(value->as_routine.block);

        break;
    case ValueTypeStack:
        for (size_t i = 0; i < value->as_stack.amount_exprs; ++i) {
            expr_delete(value->as_stack.exprs[i]);
        }

        free(value->as_stack.exprs);
        break;
    default:
        RAEL_UNREACHABLE();
    }
    free(value);
}

static void expr_delete(struct Expr* const expr) {
    switch (expr->type) {
    case ExprTypeValue:
        astvalue_delete(expr->as_value);
        break;
    case ExprTypeRoutineCall:
        expr_delete(expr->as_call.routine_value);
        for (size_t i = 0; i < expr->as_call.arguments.amount_exprs; ++i) {
            expr_delete(expr->as_call.arguments.exprs[i]);
        }
        free(expr->as_call.arguments.exprs);
        break;
    case ExprTypeKey:
        free(expr->as_key);
        break;
    case ExprTypeAdd:
    case ExprTypeSub:
    case ExprTypeMul:
    case ExprTypeDiv:
    case ExprTypeMod:
    case ExprTypeEquals:
    case ExprTypeSmallerThen:
    case ExprTypeBiggerThen:
    case ExprTypeAt:
    case ExprTypeRedirect:
    case ExprTypeTo:
    case ExprTypeAnd:
    case ExprTypeOr:
        expr_delete(expr->lhs);
        expr_delete(expr->rhs);
        break;
    case ExprTypeSizeof:
    case ExprTypeNeg:
        expr_delete(expr->as_single);
        break;
    case ExprTypeBlame:
        if (expr->as_single)
            expr_delete(expr->as_single);
        break;
    case ExprTypeSet:
        switch (expr->as_set.set_type) {
        case SetTypeKey:
            free(expr->as_set.as_key);
            break;
        case SetTypeAtExpr:
            expr_delete(expr->as_set.as_at_stat);
            break;
        default:
            RAEL_UNREACHABLE();
        }
        expr_delete(expr->as_set.expr);
        break;
    default:
        RAEL_UNREACHABLE();
    }
    free(expr);
}

void instruction_delete(struct Instruction* const inst) {
    switch (inst->type) {
    case InstructionTypeIf:
        expr_delete(inst->if_stat.condition);

        for (size_t i = 0; inst->if_stat.block[i]; ++i)
            instruction_delete(inst->if_stat.block[i]);

        free(inst->if_stat.block);

        switch (inst->if_stat.else_type) {
        case ElseTypeBlock:
            for (size_t i = 0; inst->if_stat.else_block[i]; ++i)
                instruction_delete(inst->if_stat.else_block[i]);
            free(inst->if_stat.else_block);
            break;
        case ElseTypeInstruction:
            instruction_delete(inst->if_stat.else_instruction);
            break;
        case ElseTypeNone:
            break;
        default:
            RAEL_UNREACHABLE();
        }

        break;
    case InstructionTypeLog:
    case InstructionTypeShow:
        for (size_t i = 0; i < inst->csv.amount_exprs; ++i) {
            expr_delete(inst->csv.exprs[i]);
        }

        free(inst->csv.exprs);
        break;
    case InstructionTypeLoop:
        switch (inst->loop.type) {
        case LoopWhile:
            expr_delete(inst->loop.while_condition);
            break;
        case LoopThrough:
            free(inst->loop.iterate.key);
            expr_delete(inst->loop.iterate.expr);
            break;
        case LoopForever:
            break;
        default:
            RAEL_UNREACHABLE();
        }

        for (size_t i = 0; inst->loop.block[i]; ++i) {
            instruction_delete(inst->loop.block[i]);
        }

        free(inst->loop.block);
        break;
    case InstructionTypePureExpr:
        expr_delete(inst->pure);
        break;
    case InstructionTypeReturn:
        if (inst->return_value) {
            expr_delete(inst->return_value);
        }
        break;
    case InstructionTypeBreak:
        break;
    case InstructionTypeCatch:
        expr_delete(inst->catch.catch_expr);
        if (inst->catch.handle_block) {
            for (size_t i = 0; inst->catch.handle_block[i]; ++i)
                instruction_delete(inst->catch.handle_block[i]);
        }
        break;
    default:
        RAEL_UNREACHABLE();
    }
    free(inst);
}
