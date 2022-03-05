#include "rael.h"

static struct Expr *parser_parse_expr(RaelParser* const parser);
static struct Expr *parser_parse_expr_at(RaelParser* const parser);
static struct Instruction *parser_parse_instr(RaelParser* const parser);
static struct Instruction **parser_parse_block(RaelParser* const parser);
static RaelExprList parser_parse_csv(RaelParser* const parser, const bool allow_newlines);
static struct Expr *parser_parse_suffix(RaelParser* const parser);

static void exprlist_delete(RaelExprList *list);
static void match_case_delete(struct MatchCase *match_case);
void block_delete(struct Instruction **block);

static inline char *parser_get_filename(RaelParser* const parser) {
    return parser->lexer.stream.base->name;
}

static inline void internal_parser_state_error(RaelParser* const parser, struct State state,
                                               const char* const error_message, va_list va) {
    rael_show_error_message(parser_get_filename(parser), state, error_message, va);

    // we don't return to the function because we exit, so let's just destroy the va here
    va_end(va);

    // destroy all of the parsed instructions
    for (size_t i = 0; i < parser->idx; ++i)
        instruction_deref(parser->instructions[i]);

    free(parser->instructions);
    lexer_destruct(&parser->lexer);
    exit(1);
}

static void parser_construct(RaelParser *parser, RaelStream *stream) {
    lexer_construct(&parser->lexer, stream);
    parser->idx = 0;
    parser->allocated = 0;
    parser->instructions = NULL;
    parser->can_return = false;
    parser->in_loop = false;
}

static void parser_destruct(RaelParser* parser) {
    lexer_destruct(&parser->lexer);
}

static inline struct State parser_dump_state(RaelParser* const parser) {
    return lexer_dump_state(&parser->lexer);
}

static inline void parser_load_state(RaelParser* const parser, struct State state) {
    lexer_load_state(&parser->lexer, state);
}

static struct Expr *expr_new(enum ExprType type) {
    struct Expr *expr = malloc(sizeof(struct Expr));
    expr->type = type;
    return expr;
}

static struct ValueExpr *value_expr_new(enum ValueExprType type) {
    struct ValueExpr *value = malloc(sizeof(struct ValueExpr));
    value->type = type;
    return value;
}

static struct Instruction *instruction_new(enum InstructionType type) {
    struct Instruction *instruction = malloc(sizeof(struct Instruction));
    instruction->refcount = 1;
    instruction->type = type;
    return instruction;
}

void instruction_ref(struct Instruction *instruction) {
    ++instruction->refcount;
}

static void parser_state_error(RaelParser* const parser, struct State state, const char* const error_message, ...) {
    va_list va;
    va_start(va, error_message);
    internal_parser_state_error(parser, state, error_message, va);
}

static void parser_error(RaelParser* const parser, char* error_message, ...) {
    va_list va;
    va_start(va, error_message);
    internal_parser_state_error(parser, parser_dump_state(parser), error_message, va);
}

static void parser_push(RaelParser* const parser, struct Instruction* const inst) {
    if (parser->allocated == 0) {
        parser->instructions = malloc(((parser->allocated = 64)+1) * sizeof(struct Instruction*));
    } else if (parser->idx == 64) {
        parser->instructions = realloc(parser->instructions, ((parser->allocated += 64)+1) * sizeof(struct Instruction*));
    }
    parser->instructions[parser->idx++] = inst;
}

static bool parser_match(RaelParser* const parser, enum TokenName token) {
    struct State backtrack = parser_dump_state(parser);

    if (!lexer_tokenize(&parser->lexer))
        return false;

    if (parser->lexer.token.name != token) {
        parser_load_state(parser, backtrack);
        return false;
    }

    return true;
}

static void parser_expect_newline(RaelParser* const parser) {
    struct State backtrack = parser_dump_state(parser);
    if (lexer_tokenize(&parser->lexer)) {
        if (parser->lexer.token.name != TokenNameNewline) {
            parser_state_error(parser, backtrack, "Expected newline after expression");
        }
    }
}

static bool parser_maybe_expect_newline(RaelParser* const parser) {
    struct State backtrack = parser_dump_state(parser);
    if (lexer_tokenize(&parser->lexer)) {
        if (parser->lexer.token.name != TokenNameNewline) {
            parser_load_state(parser, backtrack);
            return false;
        }
    }
    return true;
}

static struct ValueExpr *parser_parse_stack(RaelParser* const parser) {
    struct ValueExpr *value;
    RaelExprList stack;
    struct State backtrack = parser_dump_state(parser);

    if (!parser_match(parser, TokenNameLeftCur))
        return NULL;

    stack = parser_parse_csv(parser, true);

    if (!parser_match(parser, TokenNameRightCur)) {
        parser_load_state(parser, backtrack);
        exprlist_delete(&stack);
        return NULL;
    }

    value = value_expr_new(ValueTypeStack);
    value->as_stack.entries = stack;
    return value;
}

static struct ValueExpr *parser_parse_number(RaelParser* const parser) {
    struct ValueExpr *value;
    struct RaelHybridNumber ast_number;

    if (!parser_match(parser, TokenNameNumber))
        return NULL;

    // this should always work
    number_from_string(parser->lexer.token.string, parser->lexer.token.length, &ast_number);

    value = value_expr_new(ValueTypeNumber);
    value->as_number = ast_number;
    return value;
}

static struct ValueExpr *parser_parse_routine(RaelParser* const parser) {
    struct ValueExpr *value;
    struct ASTRoutineValue decl;
    struct State backtrack;
    bool old_can_return;

    if (!parser_match(parser, TokenNameRoutine))
        return NULL;

    old_can_return = parser->can_return;
    parser->can_return = true;
    backtrack = parser_dump_state(parser);

    if (!parser_match(parser, TokenNameLeftParen))
        parser_state_error(parser, backtrack, "Expected '(' after 'routine'");

    backtrack = parser_dump_state(parser);

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
            backtrack = parser_dump_state(parser);

            if (!lexer_tokenize(&parser->lexer))
                parser_error(parser, "Unexpected EOF");

            if (parser->lexer.token.name == TokenNameRightParen)
                break;

            if (parser->lexer.token.name != TokenNameComma) {
                parser_state_error(parser, backtrack, "Expected a Comma");
            }

            backtrack = parser_dump_state(parser);
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

    value = value_expr_new(ValueTypeRoutine);
    value->as_routine = decl;
    parser->can_return = old_can_return;
    return value;
}

static struct ValueExpr *parser_parse_string(RaelParser* const parser) {
    struct ValueExpr *value;
    char *string = NULL;
    size_t allocated = 0, length = 0;

    // expect a string
    if (!parser_match(parser, TokenNameString)) {
        return NULL;
    }

    for (size_t i = 0; i < parser->lexer.token.length; ++i) {
        char c;
        if (parser->lexer.token.string[i] == '\\') {
            // there isn't really a reason to have more than this
            switch (parser->lexer.token.string[++i]) {
            case '\n':
                continue;
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
            default: // this works with \" too
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

    value = value_expr_new(ValueTypeString);
    value->as_string = (struct ASTStringValue) {
        .source = string,
        .length = length
    };

    return value;
}

static struct Expr *parser_parse_literal_expr(RaelParser* const parser) {
    struct Expr *expr;
    struct State backtrack = parser_dump_state(parser);
    struct ValueExpr *value;

    if ((value = parser_parse_routine(parser)) ||
        (value = parser_parse_stack(parser))   ||
        (value = parser_parse_number(parser))  ||
        (value = parser_parse_string(parser))) {
        expr = expr_new(ExprTypeValue);
        expr->as_value = value;
    } else { // if you couldn't parse any other literal expression up until here, try to parse other stuff
        if (!lexer_tokenize(&parser->lexer))
            return NULL;

        switch (parser->lexer.token.name) {
        case TokenNameKey:
            expr = expr_new(ExprTypeKey);
            expr->as_key = token_allocate_key(&parser->lexer.token);
            break;
        case TokenNameVoid:
            expr = expr_new(ExprTypeValue);
            expr->as_value = value_expr_new(ValueTypeVoid);
            break;
        default:
            parser_load_state(parser, backtrack);
            return NULL;
        }
    }

    expr->state = backtrack;
    return expr;
}

static struct Expr *parser_parse_match(RaelParser* const parser) {
    struct Expr *expr; // the final match expression
    struct Expr *match_against; // the value you compare against
    struct MatchCase *match_cases; // an array of all of the with statements
    size_t amount = 0, allocated = 0; // array length and size counters
    struct Instruction **else_block = NULL; // the block of the else case
    struct State backtrack, full_backtrack;
    bool is_matching = true;
    bool old_can_return;

    // make sure it starts with 'match'
    if (!parser_match(parser, TokenNameMatch))
        return NULL;

    old_can_return = parser->can_return;
    parser->can_return = true;

    // parse the expression you compare against
    if (!(match_against = parser_parse_expr(parser)))
        parser_error(parser, "Expected expression");

    parser_maybe_expect_newline(parser);
    full_backtrack = backtrack = parser_dump_state(parser);

    // parse the '{'
    if (!parser_match(parser, TokenNameLeftCur)) {
        expr_delete(match_against);
        parser_load_state(parser, backtrack);
        parser_error(parser, "Expected a '{'");
    }
    parser_maybe_expect_newline(parser);

    // parse everything else
    while (is_matching) {
        backtrack = parser_dump_state(parser);
        if (!lexer_tokenize(&parser->lexer)) {
            expr_delete(match_against);
            for (size_t i = 0; i < amount; ++i)
                match_case_delete(&match_cases[i]);
            parser_error(parser, "Unexpected EOF");
        }

        switch (parser->lexer.token.name) {
        case TokenNameWith: {
            struct Instruction **case_block;
            RaelExprList exprs;

            // parse the part after the 'with'
            if ((exprs = parser_parse_csv(parser, true)).amount_exprs == 0) {
                expr_delete(match_against);
                parser_error(parser, "Expected at least one expression after 'with'");
            }

            parser_maybe_expect_newline(parser);
            // parse the block to execute
            if (!(case_block = parser_parse_block(parser))) {
                expr_delete(match_against);
                for (size_t i = 0; i < amount; ++i)
                    match_case_delete(&match_cases[i]);
                parser_error(parser, "Expected a block");
            }
            parser_maybe_expect_newline(parser);

            // extend case array if needed
            if (allocated == 0)
                match_cases = malloc((allocated = 8) * sizeof(struct MatchCase));
            else if (amount >= allocated)
                match_cases = realloc(match_cases, (allocated += 8) * sizeof(struct MatchCase));

            // add match case
            match_cases[amount++] = (struct MatchCase) {
                .match_exprs = exprs,
                .case_block = case_block
            };
            break;
        }
        case TokenNameElse:
            // parse the part after the 'else'
            if (!(else_block = parser_parse_block(parser))) {
                expr_delete(match_against);
                parser_error(parser, "Block expected");
            }

            // parse the ending '}'
            parser_maybe_expect_newline(parser);
            if (!parser_match(parser, TokenNameRightCur)) {
                expr_delete(match_against);
                block_delete(else_block);
                for (size_t i = 0; i < amount; ++i)
                    match_case_delete(&match_cases[i]);
                parser_error(parser, "Expected a '}'");
            }
            is_matching = false;
            break;
        case TokenNameRightCur:
            is_matching = false;
            break;
        default:
            parser_load_state(parser, backtrack);
            expr_delete(match_against);
            for (size_t i = 0; i < amount; ++i)
                match_case_delete(&match_cases[i]);
            parser_error(parser, "Expected 'with' or 'else'");
        }
    }

    // minimize size
    if (amount > 0)
        match_cases = realloc(match_cases, (allocated = amount) * sizeof(struct MatchCase));
    else
        match_cases = NULL;

    expr = expr_new(ExprTypeMatch);
    expr->as_match = (struct MatchExpr) {
        .match_against = match_against,
        .amount_cases = amount,
        .match_cases = match_cases,
        .else_block = else_block
    };
    parser->can_return = old_can_return;
    expr->state = full_backtrack;
    return expr;
}

static struct Expr *parser_parse_expr_set(RaelParser* const parser) {
    struct SetExpr set_expr;
    enum ExprType type;
    struct Expr *at_set, *full_expr;
    struct State backtrack = parser_dump_state(parser);
    struct State operator_state;

    if ((at_set = parser_parse_expr_at(parser)) && at_set->type == ExprTypeAt) {
        set_expr.set_type = SetTypeAtExpr;
        set_expr.as_at = at_set;
    } else {
        struct Expr *get_member;

        // load state to the previous state, in case we couldn't parse an 'at' statement
        parser_load_state(parser, backtrack);
        if (at_set) {
            expr_delete(at_set);
        }

        if ((get_member = parser_parse_suffix(parser)) && get_member->type == ExprTypeGetMember) {
            set_expr.set_type = SetTypeMember;
            set_expr.as_member = get_member;
        } else {
            // load state to the previous state, in case we couldn't parse an get_member statement
            parser_load_state(parser, backtrack);
            if (get_member) {
                expr_delete(get_member);
            }
            // if the lhs of the '?=' is invalid
            if (!parser_match(parser, TokenNameKey)) {
                return NULL;
            }
            set_expr.set_type = SetTypeKey;
            set_expr.as_key = token_allocate_key(&parser->lexer.token);
        }
    }
    // store the state in which the operator is found
    operator_state = parser_dump_state(parser);
    // try to parse ?=
    if (!lexer_tokenize(&parser->lexer)) {
        goto error;
    }
    // decide, according to the token type, the expression type
    switch (parser->lexer.token.name) {
    case TokenNameQuestionEquals: type = ExprTypeSet; break;
    case TokenNamePlusEquals: type = ExprTypeAddEqual; break;
    case TokenNameMinusEquals: type = ExprTypeSubEqual; break;
    case TokenNameStarEquals: type = ExprTypeMulEqual; break;
    case TokenNameSlashEquals: type = ExprTypeDivEqual; break;
    case TokenNamePercentEquals: type = ExprTypeModEqual; break;
    default: goto error;
    }
    // FIXME: should this error instead?
    // parse rhs of set expression
    if (!(set_expr.expr = parser_parse_expr(parser))) {
        goto error;
    }

    full_expr = expr_new(type);
    full_expr->as_set = set_expr;
    full_expr->state = operator_state;
    return full_expr;
error:
    // if you couldn't parse the expression properly, free used stuff and return a NULL
    switch (set_expr.set_type) {
    case SetTypeAtExpr:
        expr_delete(set_expr.as_at);
        break;
    case SetTypeKey:
        free(set_expr.as_key);
        break;
    case SetTypeMember:
        expr_delete(set_expr.as_member);
        break;
    default:
        RAEL_UNREACHABLE();
    }
    parser_load_state(parser, backtrack);
    return NULL;
}

static struct Expr *parser_parse_paren_expr(RaelParser* const parser) {
    struct State full_backtrack = parser_dump_state(parser);
    struct State backtrack;
    struct Expr *expr;

    if (!parser_match(parser, TokenNameLeftParen)) {
        return NULL;
    }

    backtrack = parser_dump_state(parser);

    // if it couldn't parse an expression after the '('
    if (!(expr = parser_parse_expr(parser))) {
        parser_error(parser, "Expected an expression after left paren");
    }

    // expect a ')' after an expression, but if there isn't, it might be a set expression
    if (!parser_match(parser, TokenNameRightParen)) {
        struct State after_expr_state = parser_dump_state(parser);

        // reset position to after left paren
        parser_load_state(parser, backtrack);
        expr_delete(expr);

        if (!(expr = parser_parse_expr_set(parser)))
            parser_state_error(parser, after_expr_state, "Unmatched '('");

        if (!parser_match(parser, TokenNameRightParen))
            parser_error(parser, "Unmatched '('");
    }

    expr->state = full_backtrack;
    return expr;
}

/* suffix parsing */
static struct Expr *parser_parse_suffix(RaelParser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_paren_expr(parser)) &&
        !(expr = parser_parse_literal_expr(parser))) {
        return NULL;
    }

    for (;;) {
        struct State backtrack, start_backtrack;

        start_backtrack = backtrack = parser_dump_state(parser);

        if (parser_match(parser, TokenNameKey)) {
            struct Expr *new_expr;
            char *key = token_allocate_key(&parser->lexer.token);

            new_expr = expr_new(ExprTypeGetMember);
            new_expr->as_get_member.lhs = expr;
            new_expr->as_get_member.key = key;
            expr = new_expr;
        } else {
            // parse call
            struct CallExpr call;
            // verify there is '(' after the key?
            if (!parser_match(parser, TokenNameLeftParen))
                break;

            call.callable_expr = expr;
            call.args = parser_parse_csv(parser, true);

            backtrack = parser_dump_state(parser);
            if (!parser_match(parser, TokenNameRightParen))
                parser_state_error(parser, backtrack, "Expected a ')'");

            expr = expr_new(ExprTypeCall);
            expr->as_call = call;
        }
        expr->state = start_backtrack;
    }

    return expr;
}

static struct Expr *parser_parse_expr_single(RaelParser* const parser) {
    struct Expr *expr;
    struct State backtrack = parser_dump_state(parser);;

    if (!(expr = parser_parse_suffix(parser)) &&
        !(expr = parser_parse_match(parser))) {
        if (!lexer_tokenize(&parser->lexer))
            return NULL;

        switch (parser->lexer.token.name) {
        case TokenNameSub: { // -value
            struct Expr *to_negative;

            if (!(to_negative = parser_parse_expr_single(parser)))
                parser_error(parser, "Expected an expression after or before '-'");

            expr = expr_new(ExprTypeNeg);
            expr->as_single = to_negative;
            break;
        }
        case TokenNameExclamationMark: {
            struct Expr *to_opposite;

            if (!(to_opposite = parser_parse_expr_single(parser)))
                parser_error(parser, "Expected an expression after '!'");

            expr = expr_new(ExprTypeNot);
            expr->as_single = to_opposite;
            break;
        }
        case TokenNameSizeof: {
            struct Expr *sizeof_value;

            if (!(sizeof_value = parser_parse_expr_single(parser)))
                parser_error(parser, "Expected value after 'sizeof'");

            expr = expr_new(ExprTypeSizeof);
            expr->as_single = sizeof_value;
            break;
        }
        case TokenNameTypeof: {
            struct Expr *typeof_value;

            if (!(typeof_value = parser_parse_expr_single(parser)))
                parser_error(parser, "Expected value after 'typeof'");

            expr = expr_new(ExprTypeTypeof);
            expr->as_single = typeof_value;
            break;
        }
        case TokenNameGetString: {
            struct Expr *prompt_value;

            if (!(prompt_value = parser_parse_expr_single(parser)))
                parser_error(parser, "Expected value after 'getstring'");

            expr = expr_new(ExprTypeGetString);
            expr->as_single = prompt_value;
            break;
        }
        case TokenNameBlame:
            expr = expr_new(ExprTypeBlame);
            expr->as_single = parser_parse_expr_single(parser);
            break;
        default:
            parser_load_state(parser, backtrack);
            return NULL;
        }
        expr->state = backtrack;
    }

    return expr;
}

static struct Expr *parser_parse_expr_product(RaelParser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_single(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = parser_dump_state(parser);

        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameMul:
                new_expr = expr_new(ExprTypeMul);
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_single(parser))) {
                    parser_state_error(parser, backtrack, "Expected a value after '*'");
                }
                break;
            case TokenNameDiv:
                new_expr = expr_new(ExprTypeDiv);
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_single(parser))) {
                    parser_state_error(parser, backtrack, "Expected a value after '/'");
                }
                break;
            case TokenNameMod:
                new_expr = expr_new(ExprTypeMod);
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_single(parser))) {
                    parser_state_error(parser, backtrack, "Expected a value after '%'");
                }
                break;
            default:
                parser_load_state(parser, backtrack);
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

static struct Expr *parser_parse_expr_sum(RaelParser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_product(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = parser_dump_state(parser);

        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameAdd:
                new_expr = expr_new(ExprTypeAdd);
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_product(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '+'");
                break;
            case TokenNameSub:
                new_expr = expr_new(ExprTypeSub);
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_product(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '-'");
                break;
            default:
                parser_load_state(parser, backtrack);
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


static struct Expr *parser_parse_expr_range(RaelParser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_sum(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = parser_dump_state(parser);

        if (lexer_tokenize(&parser->lexer)) {
            if (parser->lexer.token.name == TokenNameTo) {
                new_expr = expr_new(ExprTypeTo);
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_sum(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after 'to'");
            } else {
                parser_load_state(parser, backtrack);
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

static struct Expr *parser_parse_expr_at(RaelParser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_range(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = parser_dump_state(parser);

        if (lexer_tokenize(&parser->lexer)) {
            if (parser->lexer.token.name == TokenNameAt) {
                new_expr = expr_new(ExprTypeAt);
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_range(parser))) {
                    parser_state_error(parser, backtrack, "Expected a value after 'at'");
                }
            } else {
                parser_load_state(parser, backtrack);
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

static struct Expr *parser_parse_expr_comparison(RaelParser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_at(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = parser_dump_state(parser);

        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameEquals: // =
                new_expr = expr_new(ExprTypeEquals);
                if (!(new_expr->rhs = parser_parse_expr_at(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '='");
                break;
            case TokenNameExclamationMarkEquals: // !=
                new_expr = expr_new(ExprTypeNotEqual);
                if (!(new_expr->rhs = parser_parse_expr_at(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '='");
                break;
            case TokenNameSmallerThan: // <
                new_expr = expr_new(ExprTypeSmallerThan);
                if (!(new_expr->rhs = parser_parse_expr_at(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '<'");
                break;
            case TokenNameBiggerThan: // >
                new_expr = expr_new(ExprTypeBiggerThan);
                if (!(new_expr->rhs = parser_parse_expr_at(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '>'");
                break;
            case TokenNameSmallerOrEqual: // <=
                new_expr = expr_new(ExprTypeSmallerOrEqual);
                if (!(new_expr->rhs = parser_parse_expr_at(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '<='");
                break;
            case TokenNameBiggerOrEqual: // >=
                new_expr = expr_new(ExprTypeBiggerOrEqual);
                if (!(new_expr->rhs = parser_parse_expr_at(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '>='");
                break;
            default:
                parser_load_state(parser, backtrack);
                goto loop_end;
            }
            // set the lhs of the new expression to the old lhs
            new_expr->lhs = expr;
        } else {
            break;
        }

        new_expr->state = backtrack;
        expr = new_expr;
    }
loop_end:
    return expr;
}

static struct Expr *parser_parse_expr_and(RaelParser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_comparison(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = parser_dump_state(parser);

        if (lexer_tokenize(&parser->lexer)) {
            if (parser->lexer.token.name == TokenNameAmpersand) {
                new_expr = expr_new(ExprTypeAnd);
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_comparison(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '&'");
            } else {
                parser_load_state(parser, backtrack);
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

static struct Expr *parser_parse_expr_or(RaelParser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_and(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = parser_dump_state(parser);

        if (lexer_tokenize(&parser->lexer)) {
            if (parser->lexer.token.name == TokenNamePipe) {
                new_expr = expr_new(ExprTypeOr);
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_and(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '|'");
            } else {
                parser_load_state(parser, backtrack);
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

static struct Expr *parser_parse_expr(RaelParser* const parser) {
    struct Expr *expr;

    if (!(expr = parser_parse_expr_or(parser)))
        return NULL;

    for (;;) {
        struct Expr *new_expr;
        struct State backtrack = parser_dump_state(parser);

        if (lexer_tokenize(&parser->lexer)) {
            switch (parser->lexer.token.name) {
            case TokenNameRedirect:
                new_expr = expr_new(ExprTypeRedirect);
                new_expr->lhs = expr;
                if (!(new_expr->rhs = parser_parse_expr_or(parser)))
                    parser_state_error(parser, backtrack, "Expected a value after '<<'");
                break;
            default:
                parser_load_state(parser, backtrack);
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

static struct Instruction *parser_parse_instr_pure(RaelParser* const parser) {
    struct Instruction *inst;
    struct Expr *expr;
    struct State backtrack = parser_dump_state(parser);

    if ((expr = parser_parse_expr(parser))) {
        if (parser_maybe_expect_newline(parser))
            goto end;
        expr_delete(expr);
        parser_load_state(parser, backtrack);
    }

    if ((expr = parser_parse_expr_set(parser))) {
        if (parser_maybe_expect_newline(parser))
            goto end;
        expr_delete(expr);
        parser_load_state(parser, backtrack);
    }

    return NULL;
end:
    inst = instruction_new(InstructionTypePureExpr);
    inst->pure = expr;
    return inst;
}

/* parse comma separated expressions (e1, e2, e3, e4, ...) */
static RaelExprList parser_parse_csv(RaelParser* const parser, const bool allow_newlines) {
    struct RaelExprListEntry *entries;
    struct Expr *expr;
    struct State backtrack, full_backtrack = parser_dump_state(parser);
    size_t allocated, idx = 0;

    if (allow_newlines)
        parser_maybe_expect_newline(parser);

    // load backtrack
    backtrack = parser_dump_state(parser);

    // if you couldn't parse anything, backtrace and return an empty RaelExprList
    if (!(expr = parser_parse_expr(parser))) {
        parser_load_state(parser, full_backtrack);
        return (RaelExprList) {
            .amount_exprs = 0,
            .exprs = NULL
        };
    }

    entries = malloc((allocated = 1) * sizeof(struct RaelExprListEntry));
    entries[idx].expr = expr;
    entries[idx].start_state = backtrack;
    ++idx;

    for (;;) {
        // allow a newline before a comma
        if (allow_newlines)
            parser_maybe_expect_newline(parser);

        if (!parser_match(parser, TokenNameComma))
            break;

        backtrack = parser_dump_state(parser);
        // allow a newline after a comma
        if (allow_newlines)
            parser_maybe_expect_newline(parser);

        if ((expr = parser_parse_expr(parser))) {
            // push expression
            if (idx == allocated) {
                entries = realloc(entries, (allocated += 4) * sizeof(struct RaelExprListEntry));
            }
            entries[idx].expr = expr;
            entries[idx].start_state = backtrack;
            ++idx;
        } else {
            parser_state_error(parser, backtrack, "Expected expression after ','");
        }
        if (allow_newlines)
            parser_maybe_expect_newline(parser);
    }

    return (RaelExprList) {
        .amount_exprs = idx,
        .exprs = entries
    };
}

static struct Instruction *parser_parse_instr_catch(RaelParser* const parser) {
    struct Instruction *inst;
    struct CatchInstruction catch;
    struct Token key_token;
    bool store_value;

    if (!parser_match(parser, TokenNameCatch))
        return NULL;

    if (!(catch.catch_expr = parser_parse_expr(parser))) {
        parser_error(parser, "Expected expression");
    }

    parser_maybe_expect_newline(parser);

    if (parser_match(parser, TokenNameWith)) {
        if (!parser_match(parser, TokenNameKey)) {
            expr_delete(catch.catch_expr);
            parser_error(parser, "Expected a key");
        }
        key_token = parser->lexer.token;
        store_value = true;
    } else {
        store_value = false;
    }

    if (!(catch.handle_block = parser_parse_block(parser))) {
        expr_delete(catch.catch_expr);
        if (store_value) {
            parser_error(parser, "Expected block");
        } else {
            parser_error(parser, "Expected a 'with' or a block");
        }
    }

    parser_maybe_expect_newline(parser);

    // if there is an else keyword
    if (parser_match(parser, TokenNameElse)) {
        struct Instruction **else_block;

        // there can be a newline after else keyword
        parser_maybe_expect_newline(parser);

        // get block
        else_block = parser_parse_block(parser);
        if (!else_block) {
            expr_delete(catch.catch_expr);
            block_delete(catch.handle_block);
            // FIXME: this leaks
            parser_error(parser, "Expected block");
        }
        parser_maybe_expect_newline(parser);
        catch.else_block = else_block;
    } else {
        catch.else_block = NULL;
    }

    parser_maybe_expect_newline(parser);

    if (store_value) {
        // add key to catch statement
        catch.value_key = token_allocate_key(&key_token);
    } else {
        catch.value_key = NULL;
    }

    inst = instruction_new(InstructionTypeCatch);
    inst->catch = catch;
    return inst;
}

static struct Instruction *parser_parse_instr_log(RaelParser* const parser) {
    struct Instruction *inst;
    RaelExprList expr_list;

    if (!parser_match(parser, TokenNameLog))
        return NULL;

    expr_list = parser_parse_csv(parser, false);
    parser_expect_newline(parser);

    inst = instruction_new(InstructionTypeLog);
    inst->csv = expr_list;

    return inst;
}

static struct Instruction *parser_parse_instr_show(RaelParser* const parser) {
    struct Instruction *inst;
    RaelExprList expr_list;

    if (!parser_match(parser, TokenNameShow))
        return NULL;

    if ((expr_list = parser_parse_csv(parser, false)).amount_exprs == 0)
        parser_error(parser, "Expected at least one expression after \"show\"");

    parser_expect_newline(parser);

    inst = instruction_new(InstructionTypeShow);
    inst->csv = expr_list;

    return inst;
}

static struct Instruction *parser_parse_instr_return(RaelParser* const parser) {
    struct Instruction *inst;
    struct Expr *expr;
    struct State backtrack = parser_dump_state(parser);

    if (!parser_match(parser, TokenNameCaret))
        return NULL;

    if (parser_maybe_expect_newline(parser)) {
        expr = NULL;
    } else if ((expr = parser_parse_expr(parser))) {
        parser_expect_newline(parser);
    } else {
        parser_error(parser, "Expected an expression or nothing after \"^\"");
    }

    if (!parser->can_return) {
        if (expr)
            expr_delete(expr);
        parser_state_error(parser, backtrack, "'^' is outside of a routine and a match statement");
    }

    inst = instruction_new(InstructionTypeReturn);
    inst->return_value = expr;
    return inst;
}

static struct Instruction *parser_parse_instr_single(RaelParser* const parser) {
    struct Instruction *inst;
    struct State backtrack = parser_dump_state(parser);
    enum InstructionType type;

    if (!lexer_tokenize(&parser->lexer))
        return NULL;

    switch (parser->lexer.token.name) {
    case TokenNameBreak:
        if (!parser->in_loop)
            parser_state_error(parser, backtrack, "'break' is outside of a loop");
        type = InstructionTypeBreak;
        break;
    case TokenNameSkip:
        if (!parser->in_loop)
            parser_state_error(parser, backtrack, "'skip' is outside of a loop");
        type = InstructionTypeSkip;
        break;
    default:
        parser_load_state(parser, backtrack);
        return NULL;
    }

    parser_expect_newline(parser);
    inst = instruction_new(type);
    return inst;
}

static struct Instruction **parser_parse_block(RaelParser* const parser) {
    struct Instruction **block;
    size_t allocated, idx = 0;
    struct State backtrack = parser_dump_state(parser);

    parser_maybe_expect_newline(parser);
    if (!parser_match(parser, TokenNameLeftCur)) {
        parser_load_state(parser, backtrack);
        return NULL;
    }

    parser_maybe_expect_newline(parser);
    block = malloc(((allocated = 32)+1) * sizeof(struct Instruction *));
    for (;;) {
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
        if (idx == allocated)
            block = realloc(block, ((allocated += 32)+1) * sizeof(struct Instruction*));
        block[idx++] = inst;
    }

    block[idx++] = NULL;
    block = realloc(block, idx * sizeof(struct Instruction*));

    return block;
}

static struct Instruction *parser_parse_instr_load(RaelParser* const parser) {
    struct Instruction *instruction;
    char *key;
    struct Token key_token;

    if (!parser_match(parser, TokenNameLoad))
        return NULL;

    if (!parser_match(parser, TokenNameKey))
        parser_error(parser, "Expected a key");

    // store token, make sure there is a newline, and allocate the key
    key_token = parser->lexer.token;
    parser_expect_newline(parser);
    key = token_allocate_key(&key_token);

    instruction = instruction_new(InstructionTypeLoad);
    instruction->load.module_name = key;
    return instruction;
}

static struct Instruction *parser_parse_instr_if(RaelParser* const parser) {
    struct Instruction *inst;
    struct IfInstruction if_stat = {
        .else_type = ElseTypeNone
    };

    if (!parser_match(parser, TokenNameIf))
        return NULL;

    if (!(if_stat.condition = parser_parse_expr(parser)))
        parser_error(parser, "No expression after if keyword");

    // try to parse a block
    if ((if_stat.if_block = parser_parse_block(parser))) {
        if_stat.if_type = IfTypeBlock;
    } else {
        // if you couldn't parse a block, try to parse a single instruction
        if (!parser_maybe_expect_newline(parser)) {
            expr_delete(if_stat.condition);
            parser_error(parser, "No newline or block after 'if' statement");
        }
        if ((if_stat.if_instruction = parser_parse_instr(parser))) {
            if_stat.if_type = IfTypeInstruction;
        } else {
            expr_delete(if_stat.condition);
            parser_error(parser, "No block or instruction after 'if' statement");
        }
    }

    parser_maybe_expect_newline(parser);

    inst = instruction_new(InstructionTypeIf);

    if (!parser_match(parser, TokenNameElse))
        goto end;

    if ((if_stat.else_block = parser_parse_block(parser))) {
        if_stat.else_type = ElseTypeBlock;
    } else {
        parser_maybe_expect_newline(parser);
        if ((if_stat.else_instruction = parser_parse_instr(parser))) {
           if_stat.else_type = ElseTypeInstruction;
        } else {
            expr_delete(if_stat.condition);
            parser_error(parser, "Expected a block or an instruction after 'else' keyword");
        }
    }

    parser_maybe_expect_newline(parser);
end:
    inst->if_stat = if_stat;
    return inst;
}

static struct Instruction *parser_parse_instr_loop(RaelParser* const parser) {
    struct Instruction *inst;
    struct LoopInstruction loop;
    struct State backtrack;
    bool old_in_loop;

    if (!parser_match(parser, TokenNameLoop))
        return NULL;

    old_in_loop = parser->in_loop;
    parser->in_loop = true;

    if ((loop.block = parser_parse_block(parser))) {
        loop.type = LoopForever;
        goto loop_parsing_end;
    }

    backtrack = parser_dump_state(parser);

    if (parser_match(parser, TokenNameKey)) {
        struct Token key_token = parser->lexer.token;

        if (parser_match(parser, TokenNameThrough)) {
            if (!(loop.iterate.expr = parser_parse_expr(parser)))
                parser_error(parser, "Expected an expression after 'through'");

            loop.type = LoopThrough;
            loop.iterate.key = token_allocate_key(&key_token);
            if (parser_match(parser, TokenNameComma)) {
                if (!(loop.iterate.secondary_condition = parser_parse_expr(parser))) {
                    parser_error(parser, "Expected an expression after comma in loop");
                }
            } else {
                loop.iterate.secondary_condition = NULL;
            }
        } else {
            parser_load_state(parser, backtrack);
            // FIXME: is this really true?
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
    inst = instruction_new(InstructionTypeLoop);
    inst->loop = loop;
    parser->in_loop = old_in_loop;
    return inst;
}

static struct Instruction *parser_parse_instr(RaelParser* const parser) {
    struct Instruction *inst;
    struct State prev_state = parser_dump_state(parser);
    if ((inst = parser_parse_instr_pure(parser))   ||
        (inst = parser_parse_instr_log(parser))    ||
        (inst = parser_parse_instr_if(parser))     ||
        (inst = parser_parse_instr_loop(parser))   ||
        (inst = parser_parse_instr_return(parser)) ||
        (inst = parser_parse_instr_single(parser)) ||
        (inst = parser_parse_instr_catch(parser))  ||
        (inst = parser_parse_instr_show(parser))   ||
        (inst = parser_parse_instr_load(parser))) {
        inst->state = prev_state;
        return inst;
    }
    return NULL;
}

struct Instruction **rael_parse(RaelStream *stream) {
    struct State backtrack;
    struct Instruction *inst;
    RaelParser parser;

    parser_construct(&parser, stream);
    parser_maybe_expect_newline(&parser);
    // parse instruction while possible
    while ((inst = parser_parse_instr(&parser)))
        parser_push(&parser, inst);

    backtrack = lexer_dump_state(&parser.lexer);
    if (lexer_tokenize(&parser.lexer)) {
        lexer_load_state(&parser.lexer, backtrack);
        parser_error(&parser, "Syntax Error");
    }

    // push a terminating NULL
    parser_push(&parser, NULL);
    // remove the parser, but not its instructions
    parser_destruct(&parser);
    return parser.instructions;
}

struct Expr *rael_parse_expr(RaelStream *stream) {
    RaelParser parser;
    struct Expr *expr;

    parser_construct(&parser, stream);
    parser_maybe_expect_newline(&parser);
    expr = parser_parse_expr(&parser);
    if (!expr)
        return NULL;
    if (!parser_maybe_expect_newline(&parser)) {
        expr_delete(expr);
        return NULL;
    }
    parser_destruct(&parser);
    return expr;
}

static void match_case_delete(struct MatchCase *match_case) {
    exprlist_delete(&match_case->match_exprs);
    block_delete(match_case->case_block);
}

void block_deref(struct Instruction **block) {
    for (struct Instruction **instr = block; *instr; ++instr)
        instruction_deref(*instr);
}

void block_delete(struct Instruction **block) {
    assert(block);
    block_deref(block);
    free(block);
}

static void exprlist_delete(RaelExprList *list) {
    for (size_t i = 0; i < list->amount_exprs; ++i) {
        struct RaelExprListEntry *entry = &list->exprs[i];
        expr_delete(entry->expr);
    }
    free(list->exprs);
}

static void value_expr_delete(struct ValueExpr* value) {
    switch (value->type) {
    case ValueTypeVoid:
    case ValueTypeNumber:
        break;
    case ValueTypeString:
        if (value->as_string.length > 0)
            free(value->as_string.source);
        break;
    case ValueTypeRoutine:
        for (size_t i = 0; i < value->as_routine.amount_parameters; ++i)
            free(value->as_routine.parameters[i]);
        free(value->as_routine.parameters);
        block_delete(value->as_routine.block);
        break;
    case ValueTypeStack:
        exprlist_delete(&value->as_stack.entries);
        break;
    default:
        RAEL_UNREACHABLE();
    }
    free(value);
}

void expr_delete(struct Expr* const expr) {
    switch (expr->type) {
    case ExprTypeValue:
        value_expr_delete(expr->as_value);
        break;
    case ExprTypeCall: {
        struct CallExpr call = expr->as_call;
        expr_delete(call.callable_expr);
        exprlist_delete(&call.args);
        break;
    }
    case ExprTypeKey:
        free(expr->as_key);
        break;
    case ExprTypeAdd:
    case ExprTypeSub:
    case ExprTypeMul:
    case ExprTypeDiv:
    case ExprTypeMod:
    case ExprTypeEquals:
    case ExprTypeNotEqual:
    case ExprTypeSmallerThan:
    case ExprTypeBiggerThan:
    case ExprTypeSmallerOrEqual:
    case ExprTypeBiggerOrEqual:
    case ExprTypeAt:
    case ExprTypeRedirect:
    case ExprTypeTo:
    case ExprTypeAnd:
    case ExprTypeOr:
        expr_delete(expr->lhs);
        expr_delete(expr->rhs);
        break;
    case ExprTypeSizeof:
    case ExprTypeTypeof:
    case ExprTypeGetString:
    case ExprTypeNeg:
    case ExprTypeNot:
        expr_delete(expr->as_single);
        break;
    case ExprTypeBlame:
        if (expr->as_single)
            expr_delete(expr->as_single);
        break;
    case ExprTypeSet:
    case ExprTypeAddEqual:
    case ExprTypeSubEqual:
    case ExprTypeMulEqual:
    case ExprTypeDivEqual:
    case ExprTypeModEqual:
        switch (expr->as_set.set_type) {
        case SetTypeKey:
            free(expr->as_set.as_key);
            break;
        case SetTypeAtExpr:
            expr_delete(expr->as_set.as_at);
            break;
        case SetTypeMember:
            expr_delete(expr->as_set.as_member);
            break;
        default:
            RAEL_UNREACHABLE();
        }
        expr_delete(expr->as_set.expr);
        break;
    case ExprTypeMatch: {
        struct MatchExpr *match = &expr->as_match;

        expr_delete(match->match_against);
        for (size_t i = 0; i < match->amount_cases; ++i)
            match_case_delete(&match->match_cases[i]);

        free(match->match_cases);
        if (match->else_block)
            block_delete(match->else_block);
        break;
    }
    case ExprTypeGetMember:
        expr_delete(expr->as_get_member.lhs);
        free(expr->as_get_member.key);
        break;
    default:
        RAEL_UNREACHABLE();
    }
    free(expr);
}

void instruction_deref(struct Instruction* const inst) {
    --inst->refcount;
    if (inst->refcount == 0) {
        switch (inst->type) {
        case InstructionTypeIf:
            expr_delete(inst->if_stat.condition);
            // remove the if statement part
            switch (inst->if_stat.if_type) {
            case IfTypeBlock:
                block_delete(inst->if_stat.if_block);
                break;
            case IfTypeInstruction:
                instruction_deref(inst->if_stat.if_instruction);
                break;
            default:
                RAEL_UNREACHABLE();
            }
            // remove the else statement part
            switch (inst->if_stat.else_type) {
            case ElseTypeBlock:
                block_delete(inst->if_stat.else_block);
                break;
            case ElseTypeInstruction:
                instruction_deref(inst->if_stat.else_instruction);
                break;
            case ElseTypeNone:
                break;
            default:
                RAEL_UNREACHABLE();
            }
            break;
        case InstructionTypeLog:
        case InstructionTypeShow:
            exprlist_delete(&inst->csv);
            break;
        case InstructionTypeLoop: {
            struct LoopInstruction *loop = &inst->loop;
            switch (loop->type) {
            case LoopWhile:
                expr_delete(loop->while_condition);
                break;
            case LoopThrough:
                free(loop->iterate.key);
                expr_delete(loop->iterate.expr);
                if (loop->iterate.secondary_condition) {
                    expr_delete(loop->iterate.secondary_condition);
                }
                break;
            case LoopForever:
                break;
            default:
                RAEL_UNREACHABLE();
            }

            block_delete(loop->block);
            break;
        }
        case InstructionTypePureExpr:
            expr_delete(inst->pure);
            break;
        case InstructionTypeReturn:
            if (inst->return_value) {
                expr_delete(inst->return_value);
            }
            break;
        case InstructionTypeBreak:
        case InstructionTypeSkip:
            break;
        case InstructionTypeCatch:
            expr_delete(inst->catch.catch_expr);
            block_delete(inst->catch.handle_block);

            // if there is an else part to the catch statement
            if (inst->catch.else_block) {
                block_delete(inst->catch.else_block);
            }
            // only if you tell it to catch, catch
            if (inst->catch.value_key)
                free(inst->catch.value_key);
            break;
        case InstructionTypeLoad:
            free(inst->load.module_name);
            break;
        default:
            RAEL_UNREACHABLE();
        }
        free(inst);
    }
}
