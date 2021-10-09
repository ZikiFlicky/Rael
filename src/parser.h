#ifndef RAEL_PARSER_H
#define RAEL_PARSER_H

#include "lexer.h"
#include "number.h"
#include "common.h"
#include "value.h"

#include <stdbool.h>

struct Expr;

struct RaelExprList {
    size_t amount_exprs;
    struct Expr **exprs;
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
    ExprTypeTo,
    ExprTypeBlame,
    ExprTypeSet
};

struct RoutineCallExpr {
    struct Expr *routine_value;
    struct RaelExprList arguments;
};

struct SetExpr {
    enum {
        SetTypeAtExpr = 1,
        SetTypeKey
    } set_type;
    union {
        struct Expr *as_at_stat;
        char *as_key;
    };
    struct Expr *expr;
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
        struct SetExpr as_set;
    };
};

enum InstructionType {
    InstructionTypeLog = 1,
    InstructionTypeIf,
    InstructionTypeLoop,
    InstructionTypePureExpr,
    InstructionTypeReturn,
    InstructionTypeBreak,
    InstructionTypeCatch
};

struct IfInstruction {
    struct Expr *condition;
    struct Instruction **block;
    enum {
        ElseTypeNone,
        ElseTypeBlock,
        ElseTypeInstruction
    } else_type;
    union {
        struct Instruction **else_block;
        struct Instruction *else_instruction;
    };
};

struct LoopInstruction {
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
    struct Instruction **block;
};

struct CatchInstruction {
    struct Expr *catch_expr;
    struct Instruction **handle_block;
};

struct Instruction {
    enum InstructionType type;
    struct State state;
    union {
        struct RaelExprList log_values;
        struct IfInstruction if_stat;
        struct LoopInstruction loop;
        struct Expr *pure;
        struct Expr *return_value;
        struct CatchInstruction catch;
    };
};

struct Parser {
    struct Lexer lexer;
    struct Instruction** instructions;
    size_t idx, allocated;
};

struct Instruction **parse(char* const stream);

void instruction_delete(struct Instruction* const instruction);

#endif // RAEL_PARSER_H
