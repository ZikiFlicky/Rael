#ifndef RAEL_PARSER_H
#define RAEL_PARSER_H

#include "lexer.h"
#include "number.h"

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
        char *string;
        struct NumberExpr number;
        struct RaelRoutineValue routine;
        struct ASTStackValue stack;
    } as;
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
    ExprTypeBiggerThen
};

struct RoutineCallExpr {
    char *routine_name;
    struct Expr **arguments;
    size_t amount_arguments;
};

struct Expr {
    enum ExprType type;
    union {
        struct {
            struct Expr *lhs, *rhs;
        } binary;
        struct ASTValue *value;
        char *key;
        struct RoutineCallExpr call;
    } as;
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
    union {
        struct Expr **log_values;
        struct {
            char *key;
            struct Expr *expr;    
        } set;
        struct IfStatementNode if_stat;
        struct LoopNode loop;
        struct Expr *pure;
        struct Expr *return_value;
    } value;
};

struct Parser {
    struct Lexer lexer;
    struct Node** nodes;
    size_t idx, allocated;
};

struct Node **parse(char* const stream);

#endif // RAEL_PARSER_H
