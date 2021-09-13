#ifndef RAEL_PARSER_H
#define RAEL_PARSER_H

#include "lexer.h"
#include "number.h"
#include "string.h"

#include <stdbool.h>

struct Expr;

enum ValueType {
    ValueTypeVoid,
    ValueTypeNumber,
    ValueTypeString,
    ValueTypeRoutine,
    ValueTypeStack
};

struct ASTStackValue {
    size_t length, allocated;
    struct Expr **entries;
};

struct RaelRoutineValue {
    char **parameters;
    size_t amount_parameters;
    struct Node **block;
};

struct ASTValue {
    enum ValueType type;
    union {
        struct RaelStringValue as_string;
        struct NumberExpr as_number;
        struct RaelRoutineValue as_routine;
        struct ASTStackValue as_stack;
    };
};

enum ExprType {
    ExprTypeValue = 1,
    ExprTypeRoutineCall,
    ExprTypeKey,
    ExprTypeAdd,
    ExprTypeSub,
    ExprTypeMul,
    ExprTypeDiv,
    ExprTypeEquals,
    ExprTypeSmallerThen,
    ExprTypeBiggerThen,
    ExprTypeAt
};

struct RoutineCallExpr {
    char *routine_name;
    struct Expr **arguments;
    size_t amount_arguments;
};

struct Expr {
    enum ExprType type;
    struct State state;
    union {
        struct {
            struct Expr *lhs, *rhs;
        };
        struct ASTValue *as_value;
        char *as_key;
        struct RoutineCallExpr as_call;
    };
};

enum NodeType {
    NodeTypeLog = 1,
    NodeTypeSet,
    NodeTypeIf,
    NodeTypeLoop,
    NodeTypePureExpr,
    NodeTypeReturn,
};

struct IfStatementNode {
    struct Expr *condition;
    struct Node **block;
    struct Node **else_block;
};

struct LoopNode {
    struct Expr *condition;
    struct Node **block;
};

struct Node {
    enum NodeType type;
    struct State state;
    union {
        struct Expr **log_values;
        struct {
            enum {
                SetTypeAtExpr = 1,
                SetTypeKey
            } set_type;
            union {
                struct Expr *as_at_stat;
                char *as_key;
            };
            struct Expr *expr;
        } set;
        struct IfStatementNode if_stat;
        struct LoopNode loop;
        struct Expr *pure;
        struct Expr *return_value;
    };
};

struct Parser {
    struct Lexer lexer;
    struct Node** nodes;
    size_t idx, allocated;
};

struct Node **parse(char* const stream);

#endif // RAEL_PARSER_H
