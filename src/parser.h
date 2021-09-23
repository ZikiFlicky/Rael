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
    ValueTypeStack,
    ValueTypeRange
};

struct RaelExprList {
    size_t amount_exprs;
    struct Expr **exprs;
};

struct RaelRoutineValue {
    struct Scope *scope;
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
        struct RaelExprList as_stack;
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
    ExprTypeMod,
    ExprTypeNeg,
    ExprTypeEquals,
    ExprTypeSmallerThen,
    ExprTypeBiggerThen,
    ExprTypeAt,
    ExprTypeRedirect,
    ExprTypeSizeof,
    ExprTypeTo
};

struct RoutineCallExpr {
    struct Expr *routine_value;
    struct RaelExprList arguments;
};

struct Expr {
    enum ExprType type;
    struct State state;
    union {
        struct {
            struct Expr *lhs, *rhs;
        };
        struct Expr *as_single;
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
    NodeTypeBreak,
    NodeTypeBlame,
};

struct IfStatementNode {
    struct Expr *condition;
    struct Node **block;
    enum {
        ElseTypeNone,
        ElseTypeBlock,
        ElseTypeNode
    } else_type;
    union {
        struct Node **else_block;
        struct Node *else_node;
    };
};

struct LoopNode {
    enum {
        LoopWhile,
        LoopThrough, // iterate
        LoopForever
    } type;
    union {
        struct Expr *while_condition;
        struct {
            char *key;
            struct Expr *expr;
        } iterate;
    };
    struct Node **block;
};

struct Node {
    enum NodeType type;
    struct State state;
    union {
        struct RaelExprList log_values;
        struct RaelExprList blame_values;
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

void node_delete(struct Node* const node);

#endif // RAEL_PARSER_H
