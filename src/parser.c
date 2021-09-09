#include "parser.h"
#include "lexer.h"
#include "number.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

static struct Expr *parser_parse_expr(struct Parser* const parser);
static struct Node *parser_parse_node(struct Parser* const parser);
static struct ASTValue *parser_parse_node_routine(struct Parser* const parser);

static inline bool parser_error(const struct Parser* const parser, const char* const error_message) {
    fprintf(stderr, "ParserError: %s. on line: %ld, column: %ld\n",
            error_message, parser->lexer.line, parser->lexer.column);
    exit(1);
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
    struct Lexer last_state = parser->lexer;
    if (lexer_tokenize(&parser->lexer)) {
        if (parser->lexer.token.name != TokenNameNewline) {
            parser->lexer = last_state;
            return false;
        }
    }
    return true;
}

static struct ASTValue *parser_parse_stack(struct Parser* const parser) {
    struct ASTValue *value;
    struct Lexer old_state = parser->lexer;
    struct ASTStackValue stack = {
        .allocated = 0,
        .length = 0,
        .entries = NULL
    };
    if (!lexer_tokenize(&parser->lexer)) {
        return NULL;
    }
    if (parser->lexer.token.name != TokenNameLeftCur) {
        parser->lexer = old_state;
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
    value->as.stack = stack;
    return value;
backtrack:
    parser->lexer = old_state;
    return NULL;
}

static struct Expr *parser_parse_literal_expr(struct Parser* const parser) {
    struct Expr *expr;
    struct Lexer old_state = parser->lexer;
    struct ASTValue *value;
    if ((value = parser_parse_node_routine(parser)) ||
        (value = parser_parse_stack(parser))) {
        expr = malloc(sizeof(struct Expr));
        expr->type = ExprTypeValue;
        expr->as.value = value;
        return expr;
    }
    if (!lexer_tokenize(&parser->lexer))
        return NULL;
    switch (parser->lexer.token.name) {
    case TokenNameNumber: {
        struct NumberExpr number = {
            .is_float = false,
        };
        double as_float = 0.f;
        int as_int = 0;
        expr = malloc(sizeof(struct Expr));
        for (size_t i = 0; i < parser->lexer.token.length; ++i) {
            if (parser->lexer.token.string[i] == '.') {
                if (!number.is_float) {
                    number.is_float = true;
                    continue;
                } else {
                    parser_error(parser, "Unexpected '.' after float literal");
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
            number.as._float = as_float;
        else
            number.as._int = as_int;
        expr->type = ExprTypeValue;
        expr->as.value = malloc(sizeof(struct ASTValue));
        expr->as.value->type = ValueTypeNumber;
        expr->as.value->as.number = number;
        return expr;
    }
    case TokenNameString: {
        char *string;
        expr = malloc(sizeof(struct Expr));
        string = malloc((parser->lexer.token.length + 1) * sizeof(char));
        strncpy(string, parser->lexer.token.string, parser->lexer.token.length);
        string[parser->lexer.token.length] = '\0';
        expr->type = ExprTypeValue;
        expr->as.value = malloc(sizeof(struct ASTValue));
        expr->as.value->type = ValueTypeString;
        expr->as.value->as.string = string;
        return expr;
    }
    case TokenNameKey: {
        char *key;
        expr = malloc(sizeof(struct Expr));
        key = token_allocate_key(&parser->lexer.token);
        expr->as.key = key;
        expr->type = ExprTypeKey;
        return expr;
    }
    case TokenNameVoid:
        expr = malloc(sizeof(struct Expr));
        expr->type = ExprTypeValue;
        expr->as.value = malloc(sizeof(struct ASTValue));
        expr->as.value->type = ValueTypeVoid;
        return expr;
    default:
        parser->lexer = old_state;
        return NULL;
    }
}

/** routine call is:
 *    :key(arguments, seperated, by, a, comma)
 *  example:
 *    :add(5, 8)
 **/
static struct Expr *parser_parse_routine_call(struct Parser* const parser) {
    struct Expr *expr;
    struct RoutineCallExpr call;
    struct Lexer old_state = parser->lexer;
    struct Token key_token;
    struct Expr *argument;
    // is there something?
    if (!lexer_tokenize(&parser->lexer))
        return NULL;
    // verify it starts with a key
    if (parser->lexer.token.name != TokenNameKey) {
        parser->lexer = old_state;
        return NULL;
    }
    key_token = parser->lexer.token;
    // verify there is '(' after the key?
    if (!lexer_tokenize(&parser->lexer) || parser->lexer.token.name != TokenNameLeftParen) {
        parser->lexer = old_state;
        return NULL;
    }
    if ((argument = parser_parse_expr(parser))) {
        size_t allocated;
        call.arguments = malloc((allocated = 4) * sizeof(struct Expr*));
        call.amount_arguments = 0;
        call.arguments[call.amount_arguments++] = argument;
        while (true) {
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
            parser_error(parser, "Unexpected token");
        }
    } else {
        parser_error(parser, "Unexpected EOF");
    }
    call.routine_name = token_allocate_key(&key_token);
    expr = malloc(sizeof(struct Expr));
    expr->type = ExprTypeRoutineCall;
    expr->as.call = call;
    return expr;
}

static struct Expr *parser_parse_expr_single(struct Parser* const parser) {
    struct Expr *expr;
    struct Lexer backtrack;

    if ((expr = parser_parse_routine_call(parser)) ||
        (expr = parser_parse_literal_expr(parser))) {
        return expr;
    }
    backtrack = parser->lexer;
    if (!lexer_tokenize(&parser->lexer))
        return NULL;
    if (parser->lexer.token.name != TokenNameLeftParen) {
        parser->lexer = backtrack;
        return NULL;
    }
    expr = parser_parse_expr(parser);
    if (!lexer_tokenize(&parser->lexer) || parser->lexer.token.name != TokenNameRightParen) {
        parser->lexer = backtrack;
        return NULL;
    }
    return expr;
}

static struct Expr *parser_parse_expr_product(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_single(parser))) {
        return NULL;
    }

    for (;;) {
        struct Expr *new_expr;
        struct Lexer backtrack = parser->lexer;

        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameMul:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeMul;
                new_expr->as.binary.lhs = expr;
                if (!(new_expr->as.binary.rhs = parser_parse_expr_single(parser))) {
                    parser_error(parser, "Expected a value after '*'");
                }
                break;
            case TokenNameDiv:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeDiv;
                new_expr->as.binary.lhs = expr;
                if (!(new_expr->as.binary.rhs = parser_parse_expr_single(parser))) {
                    parser_error(parser, "Expected a value after '/'");
                }
                break;
            default:
                parser->lexer = backtrack;
                goto loop_end;
            } 
        } else {
            break;
        }

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
        struct Lexer backtrack = parser->lexer;

        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameAdd:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeAdd;
                new_expr->as.binary.lhs = expr;
                if (!(new_expr->as.binary.rhs = parser_parse_expr_product(parser)))
                    parser_error(parser, "Expected a value after '+'");
                break;
            case TokenNameSub:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeSub;
                new_expr->as.binary.lhs = expr;
                if (!(new_expr->as.binary.rhs = parser_parse_expr_product(parser)))
                    parser_error(parser, "Expected a value after '-'");
                break;
            default:
                parser->lexer = backtrack;
                goto loop_end;
            }
        } else {
            break;
        }

        expr = new_expr;
    }
loop_end:
    return expr;
}

static struct Expr *parser_parse_expr(struct Parser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_sum(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct Lexer backtrack = parser->lexer;

        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameEquals:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeEquals;
                new_expr->as.binary.lhs = expr;
                if (!(new_expr->as.binary.rhs = parser_parse_expr_sum(parser)))
                    parser_error(parser, "Expected a value after '='");
                break;
            case TokenNameSmallerThan:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeSmallerThen;
                new_expr->as.binary.lhs = expr;
                if (!(new_expr->as.binary.rhs = parser_parse_expr_sum(parser)))
                    parser_error(parser, "Expected a value after '<'");
                break;
            case TokenNameBiggerThan:
                new_expr = malloc(sizeof(struct Expr));
                new_expr->type = ExprTypeBiggerThen;
                new_expr->as.binary.lhs = expr;
                if (!(new_expr->as.binary.rhs = parser_parse_expr_sum(parser)))
                    parser_error(parser, "Expected a value after '>'");
                break;
            default:
                parser->lexer = backtrack;
                goto loop_end;
            }
        } else {
            break;
        }

        expr = new_expr;
    }
loop_end:
    return expr;
}

static struct Node *parser_parse_node_set(struct Parser* const parser) {
    struct Lexer old_state = parser->lexer;
    struct Node *node;
    struct Expr *expr;
    struct Token key_token;
    // is there nothing?
    if (!lexer_tokenize(&parser->lexer))
        return NULL;
    // doesn't start with "log"
    if (parser->lexer.token.name != TokenNameKey) {
        parser->lexer = old_state;
        return NULL;
    }
    key_token = parser->lexer.token;
    // expect an expression after "log"
    if (!(expr = parser_parse_expr(parser))) {
        parser->lexer = old_state;
        return NULL;
    }
    parser_expect_newline(parser);
    node = malloc(sizeof(struct Node));
    node->type = NodeTypeSet;
    node->value.set.key = token_allocate_key(&key_token);
    node->value.set.expr = expr;
    return node;
}

static struct Node *parser_parse_node_pure(struct Parser* const parser) {
    struct Node *node;
    struct Expr *expr;

    if (!(expr = parser_parse_expr(parser)))
        return NULL;

    parser_expect_newline(parser);
    node = malloc(sizeof(struct Node));
    node->type = NodeTypePureExpr;
    node->value.pure = expr;
    return node;
}

static struct Node *parser_parse_node_log(struct Parser* const parser) {
    struct Lexer old_state = parser->lexer;
    struct Node *node;
    struct Expr *expr;
    struct Expr **exprs_ary;
    size_t allocated, idx = 0;

    if (!lexer_tokenize(&parser->lexer))
        return NULL;

    if (parser->lexer.token.name != TokenNameLog) {
        parser->lexer = old_state;
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
    node->value.log_values = exprs_ary;

    return node;
}

static struct Node *parser_parse_node_return(struct Parser* const parser) {
    struct Lexer old_state = parser->lexer;
    struct Node *node;
    struct Expr *expr;

    if (!lexer_tokenize(&parser->lexer))
        return NULL;

    if (parser->lexer.token.name != TokenNameCaret) {
        parser->lexer = old_state;
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
    node->value.return_value = expr;
    return node;
}

static struct Node **parser_parse_block(struct Parser* const parser) {
    struct Lexer old_state = parser->lexer;
    struct Node **nodes;
    size_t node_amount, node_idx = 0;
    // is there something?
    if (!lexer_tokenize(&parser->lexer))
        return NULL;
    // verify it starts with '{'
    if (parser->lexer.token.name != TokenNameLeftCur) {
        parser->lexer = old_state;
        return NULL;
    }
    parser_maybe_expect_newline(parser);
    nodes = malloc(((node_amount = 32)+1) * sizeof(struct Node *));
    while (true) {
        struct Node *node;
        if (!(node = parser_parse_node(parser))) {
            if (!lexer_tokenize(&parser->lexer))
                parser_error(parser, "Expected '}'");
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
    struct Lexer old_state = parser->lexer;
    // is there something?
    if (!lexer_tokenize(&parser->lexer))
        return NULL;
    // verify it starts with 'routine'
    if (parser->lexer.token.name != TokenNameRoutine) {
        parser->lexer = old_state;
        return NULL;
    }
    // verify there is '(' after 'routine'?
    if (!lexer_tokenize(&parser->lexer) || parser->lexer.token.name != TokenNameLeftParen) {
        parser->lexer = old_state;
        parser_error(parser, "Expected '(' after 'routine'");
    }
    // verify you can lex
    if (!lexer_tokenize(&parser->lexer))
        parser_error(parser, "Unexpected EOF");
    // if there is a ')', it is the end
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
        while (true) {
            // tokenize
            if (!lexer_tokenize(&parser->lexer))
                parser_error(parser, "Unexpected EOF");
            // if the token is ')', you can exit the loop
            if (parser->lexer.token.name == TokenNameRightParen)
                break;
            // if there is no comma, error
            if (parser->lexer.token.name != TokenNameComma)
                parser_error(parser, "Expected Comma");
            // tokenize
            if (!lexer_tokenize(&parser->lexer))
                parser_error(parser, "Unexpected EOF");
            // reallocate if needed
            if (decl.amount_parameters == allocated) {
                decl.parameters = realloc(decl.parameters, (allocated += 3)*sizeof(struct Expr*));
            }
            // parse expression
            decl.parameters[decl.amount_parameters++] = token_allocate_key(&parser->lexer.token);
        }
        break;
    }
    default:
        parser_error(parser, "Unexpected token");
    }
    if (!(decl.block = parser_parse_block(parser))) {
        parser_error(parser, "Expected block after routine decleration");
    }
    value = malloc(sizeof(struct ASTValue));
    value->type = ValueTypeRoutine;
    value->as.routine = decl;
    return value;
}

static struct Node *parser_parse_if_statement(struct Parser* const parser) {
    struct Node *node;
    struct Lexer old_state = parser->lexer;
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
        parser->lexer = old_state;
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
    old_state = parser->lexer;
    if (!lexer_tokenize(&parser->lexer)) {
        goto end;
    }
    if (parser->lexer.token.name != TokenNameElse) {
        parser->lexer = old_state;
        goto end;
    }
    if (!(if_stat.else_block = parser_parse_block(parser))) {
        parser_error(parser, "Expected block after Else keyword");
    }
    parser_maybe_expect_newline(parser);
end:
    node->value.if_stat = if_stat;
    return node;
}

static struct Node *parser_parse_loop(struct Parser* const parser) {
    struct Node *node;
    struct Lexer old_state = parser->lexer;
    struct LoopNode loop = {
        .block = NULL,
        .condition = NULL
    };
    // is there something?
    if (!lexer_tokenize(&parser->lexer))
        return NULL;
    // verify it starts with 'loop'
    if (parser->lexer.token.name != TokenNameLoop) {
        parser->lexer = old_state;
        return NULL;
    }
    if (!(loop.condition = parser_parse_expr(parser))) {
        parser_error(parser, "Expected an expression after if keyword");
    }
    if (!(loop.block = parser_parse_block(parser))) {
        parser_error(parser, "Expected block after expression");
    }
    parser_maybe_expect_newline(parser);
    node = malloc(sizeof(struct Node));
    node->type = NodeTypeLoop;
    node->value.loop = loop;
    return node;
}

static struct Node *parser_parse_node(struct Parser* const parser) {
    struct Node *node;
    if ((node = parser_parse_node_set(parser))     ||
        (node = parser_parse_node_pure(parser))    ||
        (node = parser_parse_node_log(parser))     ||
        (node = parser_parse_if_statement(parser)) ||
        (node = parser_parse_loop(parser))         ||
        (node = parser_parse_node_return(parser))) {
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
