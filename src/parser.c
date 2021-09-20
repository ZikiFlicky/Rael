#include "parser.h"
#include "lexer.h"
#include "number.h"
#include "value.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static struct Expr *parser_parse_expr(struct Parser* const parser);
static struct Node *parser_parse_node(struct Parser* const parser);
static struct ASTValue *parser_parse_node_routine(struct Parser* const parser);

void rael_error(struct State state, const char* const error_message);

static inline void parser_error(struct Parser* const parser, const char* const error_message) {
    rael_error(lexer_dump_state(&parser->lexer), error_message);
}

static void parser_push(struct Parser* const parser, struct Node* const node) {
    if (parser->allocated == 0) {
        parser->nodes = malloc(((parser->allocated = 32)+1) * sizeof(struct Node*));
    } else if (parser->idx == 32) {
        parser->nodes = realloc(parser->nodes, (parser->allocated += 32)+1 * sizeof(struct Node*));
    }
    parser->nodes[parser->idx++] = node;
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
    struct State backtrack = lexer_dump_state(&parser->lexer);
    struct ASTStackValue stack = {
        .allocated = 0,
        .length = 0,
        .entries = NULL
    };
    if (!lexer_tokenize(&parser->lexer)) {
        return NULL;
    }
    if (parser->lexer.token.name != TokenNameLeftCur) {
        lexer_load_state(&parser->lexer, backtrack);
        return NULL;
    }
    parser_maybe_expect_newline(parser);
    for (;;) {
        struct Expr *expr = parser_parse_expr(parser);
        parser_maybe_expect_newline(parser);
        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameRightCur:
                if (expr) {
                    if (stack.allocated == 0) {
                        stack.entries = malloc((stack.allocated = 8) * sizeof(struct Expr*));
                    } else {
                        stack.entries = realloc(stack.entries, (stack.allocated += 3) * sizeof(struct Expr*));
                    }
                    stack.entries[stack.length++] = expr;
                }
                goto loop_end;
            case TokenNameComma:
                break;
            default:
                goto backtrack;
            }
        } else {
            parser_error(parser, "Encountered an unexpected end-of-file while parsing a stack");
        }
        if (stack.allocated == 0) {
            stack.entries = malloc((stack.allocated = 8) * sizeof(struct Expr*));
        } else {
            stack.entries = realloc(stack.entries, (stack.allocated += 3) * sizeof(struct Expr*));
        }
        stack.entries[stack.length++] = expr;
    }
loop_end:
    value = malloc(sizeof(struct ASTValue));
    value->type = ValueTypeStack;
    value->as_stack = stack;
    return value;
backtrack:
    lexer_load_state(&parser->lexer, backtrack);
    return NULL;
}

static struct ASTValue *parser_parse_number(struct Parser* const parser) {
    struct State backtrack = lexer_dump_state(&parser->lexer);
    struct ASTValue *value;
    struct NumberExpr number;
    double as_float = 0.f;
    int as_int = 0;

    number.is_float = false;

    if (!lexer_tokenize(&parser->lexer) || parser->lexer.token.name != TokenNameNumber) {
        lexer_load_state(&parser->lexer, backtrack);
        return NULL;
    }

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

    if ((value = parser_parse_node_routine(parser)) ||
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
        char *string;
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

static struct Expr *parser_parse_expr_single(struct Parser* const parser) {
    struct Expr *expr;
    struct State backtrack = lexer_dump_state(&parser->lexer);;

    if (!(expr = parser_parse_literal_expr(parser))) {
        if (!lexer_tokenize(&parser->lexer))
            return NULL;

        if (parser->lexer.token.name == TokenNameSub) {
            struct Expr *to_negative;

            if (!(to_negative = parser_parse_expr_single(parser)))
                parser_error(parser, "Expected an expression after or before '-'");

            expr = malloc(sizeof(struct Expr));
            expr->type = ExprTypeNeg;
            expr->as_single = to_negative;
        } else if (parser->lexer.token.name == TokenNameSizeof) {
            struct Expr *sizeof_value;

            if (!(sizeof_value = parser_parse_expr_single(parser))) {
                parser_error(parser, "Expected value after 'sizeof'");
            }

            expr = malloc(sizeof(struct Expr));
            expr->type = ExprTypeSizeof;
            expr->as_single = sizeof_value;
        } else if (parser->lexer.token.name == TokenNameLeftParen) {
            if (!(expr = parser_parse_expr(parser))) {
                parser_error(parser, "Expected an expression after left paren");
            }
            if (!lexer_tokenize(&parser->lexer) || parser->lexer.token.name != TokenNameRightParen) {
                parser_error(parser, "Unmatched '('");
            }
        } else {
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
        struct Expr *argument;

        start_backtrack = backtrack = lexer_dump_state(&parser->lexer);

        // verify there is '(' after the key?
        if (!lexer_tokenize(&parser->lexer) || parser->lexer.token.name != TokenNameLeftParen) {
            lexer_load_state(&parser->lexer, backtrack);
            break;
        }

        backtrack = lexer_dump_state(&parser->lexer);

        call.routine_value = expr;

        if ((argument = parser_parse_expr(parser))) {
            size_t allocated;
            call.arguments = malloc((allocated = 4) * sizeof(struct Expr*));
            call.amount_arguments = 0;
            call.arguments[call.amount_arguments++] = argument;
            for (;;) {
                // verify you can tokenize
                if (!lexer_tokenize(&parser->lexer))
                    parser_error(parser, "Unexpected EOF");
                // if the token is ')', you can exit the loop
                if (parser->lexer.token.name == TokenNameRightParen)
                    break;
                // if there is no comma, error
                if (parser->lexer.token.name != TokenNameComma)
                    parser_error(parser, "Expected Comma");
                // parse expression
                if (!(argument = parser_parse_expr(parser)))
                    parser_error(parser, "Expected expression after ','");
                // reallocate if needed
                if (call.amount_arguments == allocated) {
                    call.arguments = realloc(call.arguments, (allocated += 3)*sizeof(struct Expr*));
                }
                // push argument
                call.arguments[call.amount_arguments++] = argument;
            }
        } else if (lexer_tokenize(&parser->lexer)) {
            if (parser->lexer.token.name == TokenNameRightParen) {
                call.amount_arguments = 0;
                call.arguments = NULL;
            } else {
                rael_error(backtrack, "Expected a ')'");
            }
        } else {
            parser_error(parser, "Unexpected EOF");
        }

        expr = malloc(sizeof(struct Expr));
        expr->type = ExprTypeRoutineCall;
        expr->as_call = call;
        expr->state = start_backtrack;
    }

    return expr;
}

static struct Expr *parser_parse_expr_product(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_routine_call(parser))) {
        return NULL;
    }

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
                    rael_error(backtrack, "Expected a value after '*'");
                }
                break;
            case TokenNameDiv:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeDiv;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_routine_call(parser))) {
                    rael_error(backtrack, "Expected a value after '/'");
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
                    rael_error(backtrack, "Expected a value after '+'");
                break;
            case TokenNameSub:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeSub;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_product(parser)))
                    rael_error(backtrack, "Expected a value after '-'");
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

static struct Expr *parser_parse_expr_comparison(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_sum(parser)))
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
                if (!(new_expr->rhs = parser_parse_expr_sum(parser)))
                    rael_error(backtrack, "Expected a value after '='");
                break;
            case TokenNameSmallerThan:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeSmallerThen;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_sum(parser)))
                    rael_error(backtrack, "Expected a value after '<'");
                break;
            case TokenNameBiggerThan:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeBiggerThen;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_sum(parser)))
                    rael_error(backtrack, "Expected a value after '>'");
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

    if (!(expr = parser_parse_expr_comparison(parser))) {
        return NULL;
    }

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = lexer_dump_state(&parser->lexer);

        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameAt:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeAt;
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_comparison(parser))) {
                    rael_error(backtrack, "Expected a value after 'at'");
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

static struct Expr *parser_parse_expr(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_at(parser)))
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
                if (!(new_expr->rhs = parser_parse_expr_at(parser)))
                    rael_error(backtrack, "Expected a value after '<<'");
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

static struct Node *parser_parse_node_set(struct Parser* const parser) {
    struct State backtrack = lexer_dump_state(&parser->lexer);
    struct Node *node;
    struct Expr *expr, *at_set;

    if ((at_set = parser_parse_expr_at(parser)) && at_set->type == ExprTypeAt) {
        // parse rhs
        if (!(expr = parser_parse_expr(parser))) {
            lexer_load_state(&parser->lexer, backtrack);
            return NULL;
        }
        parser_expect_newline(parser);
        node = malloc(sizeof(struct Node));
        node->type = NodeTypeSet;
        node->set.set_type = SetTypeAtExpr;
        node->set.as_at_stat = at_set;
        node->set.expr = expr;
    } else {
        struct Token key_token;
        lexer_load_state(&parser->lexer, backtrack);
        // is there nothing?
        if (!lexer_tokenize(&parser->lexer))
            return NULL;
        // doesn't start with "log"
        if (parser->lexer.token.name != TokenNameKey) {
            lexer_load_state(&parser->lexer, backtrack);
            return NULL;
        }
        key_token = parser->lexer.token;
        // expect an expression after key
        if (!(expr = parser_parse_expr(parser))) {
            lexer_load_state(&parser->lexer, backtrack);
            return NULL;
        }
        parser_expect_newline(parser);
        node = malloc(sizeof(struct Node));
        node->type = NodeTypeSet;
        node->set.set_type = SetTypeKey;
        node->set.as_key = token_allocate_key(&key_token);
        node->set.expr = expr;
    }
    node->state = backtrack;
    return node;
}

static struct Node *parser_parse_node_pure(struct Parser* const parser) {
    struct Node *node;
    struct Expr *expr;
    struct State backtrack = lexer_dump_state(&parser->lexer);

    if (!(expr = parser_parse_expr(parser)))
        return NULL;

    if (!parser_maybe_expect_newline(parser)) {
        lexer_load_state(&parser->lexer, backtrack);
        return NULL;
    }

    node = malloc(sizeof(struct Node));
    node->type = NodeTypePureExpr;
    node->pure = expr;
    return node;
}

static struct Node *parser_parse_node_log(struct Parser* const parser) {
    struct State backtrack = lexer_dump_state(&parser->lexer);
    struct Node *node;
    struct Expr *expr;
    struct Expr **exprs_ary;
    size_t allocated, idx = 0;

    if (!lexer_tokenize(&parser->lexer))
        return NULL;

    if (parser->lexer.token.name != TokenNameLog) {
        lexer_load_state(&parser->lexer, backtrack);
        return NULL;
    }

    if (!(expr = parser_parse_expr(parser)))
        parser_error(parser, "Expected at least one expression after \"log\"");

    exprs_ary = malloc(((allocated = 1) + 1) * sizeof(struct Expr*));
    exprs_ary[idx++] = expr;

    for (;;) {
        if (!lexer_tokenize(&parser->lexer))
            break;
        switch (parser->lexer.token.name) {
        case TokenNameComma:
            if ((expr = parser_parse_expr(parser))) {
                if (idx == allocated) {
                    exprs_ary = realloc(exprs_ary, ((allocated += 4) + 1) * sizeof(struct Expr *));
                }
                exprs_ary[idx++] = expr;
            } else {
                parser_error(parser, "Expected expression after comma in log node");
            }
            break;
        case TokenNameNewline:
            goto loop_end;
        default:
            parser_error(parser, "Expected a comma or newline after expression in log");
        }
    }
loop_end:
    exprs_ary[idx] = NULL;

    node = malloc(sizeof(struct Node));
    node->type = NodeTypeLog;
    node->log_values = exprs_ary;

    return node;
}

static struct Node *parser_parse_node_return(struct Parser* const parser) {
    struct State backtrack = lexer_dump_state(&parser->lexer);
    struct Node *node;
    struct Expr *expr;

    if (!lexer_tokenize(&parser->lexer))
        return NULL;

    if (parser->lexer.token.name != TokenNameCaret) {
        lexer_load_state(&parser->lexer, backtrack);
        return NULL;
    }

    if (parser_maybe_expect_newline(parser)) {
        expr = NULL;
    } else if ((expr = parser_parse_expr(parser))) {
        parser_expect_newline(parser);
    } else {
        parser_error(parser, "Expected an expression or nothing after \"^\"");
    }

    node = malloc(sizeof(struct Node));
    node->type = NodeTypeReturn;
    node->return_value = expr;
    return node;
}

static struct Node **parser_parse_block(struct Parser* const parser) {
    struct State backtrack = lexer_dump_state(&parser->lexer);
    struct Node **nodes;
    size_t node_amount, node_idx = 0;
    // is there something?
    if (!lexer_tokenize(&parser->lexer))
        return NULL;
    // verify it starts with '{'
    if (parser->lexer.token.name != TokenNameLeftCur) {
        lexer_load_state(&parser->lexer, backtrack);
        return NULL;
    }
    parser_maybe_expect_newline(parser);
    nodes = malloc(((node_amount = 32)+1) * sizeof(struct Node *));
    while (true) {
        struct Node *node;
        if (!(node = parser_parse_node(parser))) {
            if (!lexer_tokenize(&parser->lexer))
                parser_error(parser, "Unmatched '}'");
            if (parser->lexer.token.name == TokenNameRightCur) {
                break;
            } else {
                parser_error(parser, "Unexpected token");
            }
        }
        if (node_idx == node_amount) {
            nodes = realloc(nodes, ((node_amount += 32)+1) * sizeof(struct Node *));
        }
        nodes[node_idx++] = node;
    }

    nodes[node_idx] = NULL;
    return nodes;
}

static struct ASTValue *parser_parse_node_routine(struct Parser* const parser) {
    struct ASTValue *value;
    struct RaelRoutineValue decl;
    struct State backtrack = lexer_dump_state(&parser->lexer);

    if (!lexer_tokenize(&parser->lexer))
        return NULL;

    if (parser->lexer.token.name != TokenNameRoutine) {
        lexer_load_state(&parser->lexer, backtrack);
        return NULL;
    }

    backtrack = lexer_dump_state(&parser->lexer);
    if (!lexer_tokenize(&parser->lexer) || parser->lexer.token.name != TokenNameLeftParen) {
        rael_error(backtrack, "Expected '(' after 'routine'");
    }

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
                rael_error(backtrack, "Expected a Comma");
            }

            backtrack = lexer_dump_state(&parser->lexer);
            if (!lexer_tokenize(&parser->lexer))
                parser_error(parser, "Unexpected EOF");

            if (parser->lexer.token.name != TokenNameKey) {
                rael_error(backtrack, "Expected key");
            }

            if (decl.amount_parameters == allocated)
                decl.parameters = realloc(decl.parameters, (allocated += 3) * sizeof(struct Expr*));

            // verify there are no duplicate parameters
            for (size_t parameter = 0; parameter < decl.amount_parameters; ++parameter) {
                if (strncmp(parser->lexer.token.string, decl.parameters[parameter], parser->lexer.token.length) == 0) {
                    rael_error(backtrack, "Duplicate parameter on routine decleration");
                }
            }
            decl.parameters[decl.amount_parameters++] = token_allocate_key(&parser->lexer.token);
        }
        break;
    }
    default:
        rael_error(backtrack, "Expected key");
    }

    if (!(decl.block = parser_parse_block(parser))) {
        parser_error(parser, "Expected block after routine decleration");
    }

    value = malloc(sizeof(struct ASTValue));
    value->type = ValueTypeRoutine;
    value->as_routine = decl;
    return value;
}

static struct Node *parser_parse_if_statement(struct Parser* const parser) {
    struct Node *node;
    struct State backtrack = lexer_dump_state(&parser->lexer);
    struct IfStatementNode if_stat = {
        .block = NULL,
        .else_block = NULL,
        .condition = NULL
    };
    // is there something?
    if (!lexer_tokenize(&parser->lexer))
        return NULL;
    // verify it starts with 'if'
    if (parser->lexer.token.name != TokenNameIf) {
        lexer_load_state(&parser->lexer, backtrack);
        return NULL;
    }
    if (!(if_stat.condition = parser_parse_expr(parser))) {
        parser_error(parser, "Expected an expression after if keyword");
    }
    if (!(if_stat.block = parser_parse_block(parser))) {
        parser_error(parser, "Expected block after if (expr)");
    }
    parser_maybe_expect_newline(parser);
    node = malloc(sizeof(struct Node));
    node->type = NodeTypeIf;
    backtrack = lexer_dump_state(&parser->lexer);
    if (!lexer_tokenize(&parser->lexer)) {
        goto end;
    }
    if (parser->lexer.token.name != TokenNameElse) {
        lexer_load_state(&parser->lexer, backtrack);
        goto end;
    }
    if (!(if_stat.else_block = parser_parse_block(parser))) {
        parser_error(parser, "Expected block after Else keyword");
    }
    parser_maybe_expect_newline(parser);
end:
    node->if_stat = if_stat;
    return node;
}

static struct Node *parser_parse_loop(struct Parser* const parser) {
    struct Node *node;
    struct State backtrack = lexer_dump_state(&parser->lexer);
    struct LoopNode loop;

    if (!lexer_tokenize(&parser->lexer))
        return NULL;

    if (parser->lexer.token.name != TokenNameLoop) {
        lexer_load_state(&parser->lexer, backtrack);
        return NULL;
    }

    backtrack = lexer_dump_state(&parser->lexer);

    if ((loop.block = parser_parse_block(parser))) {
        loop.type = LoopForever;
        goto loop_parsing_end;
    }

    if (!lexer_tokenize(&parser->lexer))
        parser_error(parser, "Expected expression after 'loop'");

    if (parser->lexer.token.name == TokenNameKey) {
        struct Token key_token = parser->lexer.token;

        if (lexer_tokenize(&parser->lexer) && parser->lexer.token.name == TokenNameThrough) {
            if (!(loop.iterate.expr = parser_parse_expr(parser)))
                parser_error(parser, "Expected an expression after 'through'");

            loop.type = LoopThrough;
            loop.iterate.key = token_allocate_key(&key_token);
        } else {
            lexer_load_state(&parser->lexer, backtrack);
            // we already know there is a key
            assert(loop.while_condition = parser_parse_expr(parser));
            loop.type = LoopWhile;
        }
    } else {
        lexer_load_state(&parser->lexer, backtrack);

        if (!(loop.while_condition = parser_parse_expr(parser)))
            parser_error(parser, "Expected expression after loop");

        loop.type = LoopWhile;
    }

    if (!(loop.block = parser_parse_block(parser)))
        parser_error(parser, "Expected block after loop");

loop_parsing_end:
    parser_maybe_expect_newline(parser);
    node = malloc(sizeof(struct Node));
    node->type = NodeTypeLoop;
    node->loop = loop;
    return node;
}

static struct Node *parser_parse_node(struct Parser* const parser) {
    struct Node *node;
    struct State prev_state = lexer_dump_state(&parser->lexer);
    if ((node = parser_parse_node_pure(parser))    ||
        (node = parser_parse_node_set(parser))     ||
        (node = parser_parse_node_log(parser))     ||
        (node = parser_parse_if_statement(parser)) ||
        (node = parser_parse_loop(parser))         ||
        (node = parser_parse_node_return(parser))) {
        node->state = prev_state;
        return node;
    }
    return NULL;
}

struct Node **parse(char* const stream) {
    struct Node *node;
    struct Parser parser = {
        .allocated = 0,
        .idx = 0,
        .nodes = NULL,
        .lexer = {
            .line = 1,
            .column = 1,
            .stream = stream
        }
    };
    parser_maybe_expect_newline(&parser);
    while ((node = parser_parse_node(&parser))) {
        parser_push(&parser, node);
    }
    parser_push(&parser, NULL);
    if (lexer_tokenize(&parser.lexer)) {
        parser_error(&parser, "Syntax Error");
    }
    return parser.nodes;
}

static void expr_delete(struct Expr* const expr);

void node_delete(struct Node* const node);

static void astvalue_delete(struct ASTValue* value) {
    switch (value->type) {
    case ValueTypeVoid:
    case ValueTypeNumber:
        break;
    case ValueTypeString:
        free(value->as_string.value);
        break;
    case ValueTypeRoutine:
        for (size_t i = 0; i < value->as_routine.amount_parameters; ++i) {
            free(value->as_routine.parameters[i]);
        }

        free(value->as_routine.parameters);

        for (size_t i = 0; value->as_routine.block[i]; ++i) {
            node_delete(value->as_routine.block[i]);
        }

        free(value->as_routine.block);

        break;
    case ValueTypeStack:
        for (size_t i = 0; i < value->as_stack.length; ++i) {
            expr_delete(value->as_stack.entries[i]);
        }

        free(value->as_stack.entries);
        break;
    default:
        assert(0);
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
        for (size_t i = 0; i < expr->as_call.amount_arguments; ++i) {
            expr_delete(expr->as_call.arguments[i]);
        }
        free(expr->as_call.arguments);
        break;
    case ExprTypeKey:
        free(expr->as_key);
        break;
    case ExprTypeAdd:
    case ExprTypeSub:
    case ExprTypeMul:
    case ExprTypeDiv:
    case ExprTypeEquals:
    case ExprTypeSmallerThen:
    case ExprTypeBiggerThen:
    case ExprTypeAt:
    case ExprTypeRedirect:
        expr_delete(expr->lhs);
        expr_delete(expr->rhs);
        break;
    case ExprTypeSizeof:
    case ExprTypeNeg:
        expr_delete(expr->as_single);
        break;
    default:
        assert(0);
    }
    free(expr);
}

void node_delete(struct Node* const node) {
    switch (node->type) {
    case NodeTypeIf:
        expr_delete(node->if_stat.condition);

        for (size_t i = 0; node->if_stat.block[i]; ++i)
            node_delete(node->if_stat.block[i]);

        free(node->if_stat.block);

        if (node->if_stat.else_block) {
            for (size_t i = 0; node->if_stat.else_block[i]; ++i)
                node_delete(node->if_stat.else_block[i]);

            free(node->if_stat.else_block);
        }
        break;
    case NodeTypeLog:
        for (size_t i = 0; node->log_values[i]; ++i) {
            expr_delete(node->log_values[i]);
        }

        free(node->log_values);
        break;
    case NodeTypeLoop:
        switch (node->loop.type) {
        case LoopWhile:
            expr_delete(node->loop.while_condition);
            break;
        case LoopThrough:
            free(node->loop.iterate.key);
            expr_delete(node->loop.iterate.expr);
            break;
        case LoopForever:
            break;
        default:
            assert(0);
        }

        for (size_t i = 0; node->loop.block[i]; ++i) {
            node_delete(node->loop.block[i]);
        }

        free(node->loop.block);
        break;
    case NodeTypePureExpr:
        expr_delete(node->pure);
        break;
    case NodeTypeReturn:
        if (node->return_value) {
            expr_delete(node->return_value);
        }
        break;
    case NodeTypeSet:
        if (node->set.set_type == SetTypeKey) {
            free(node->set.as_key);
        } else {
            expr_delete(node->set.as_at_stat);
        }
        expr_delete(node->set.expr);
        break;
    default:
        assert(0);
    }
    free(node);
}
