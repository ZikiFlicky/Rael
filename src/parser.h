#ifndef RAEL_PARSER_H
#define RAEL_PARSER_H

#include "common.h"
#include "lexer.h"
#include "value.h"
#include "number.h"
#include "string.h"

#include <stdbool.h>

struct Expr;

typedef struct RaelExprList {
    size_t amount_exprs;
    struct Expr **exprs;
} RaelExprList;

enum ValueExprType {
    ValueTypeVoid,
    ValueTypeNumber,
    ValueTypeString,
    ValueTypeRoutine,
    ValueTypeStack
};

struct ASTStringValue {
    char *source;
    size_t length;
};

struct ASTRoutineValue {
    char **parameters;
    size_t amount_parameters;
    struct Instruction **block;
};

struct ASTStackValue {
    RaelExprList entries;
};

struct ValueExpr {
    enum ValueExprType type;
    union {
        struct RaelHybridNumber as_number;
        struct ASTStringValue as_string;
        struct ASTRoutineValue as_routine;
        struct ASTStackValue as_stack;
        RaelTypeValue *as_type;
    };
};

enum ExprType {
    ExprTypeValue = 1,
    ExprTypeCall,
    ExprTypeKey,
    ExprTypeAdd,
    ExprTypeSub,
    ExprTypeMul,
    ExprTypeDiv,
    ExprTypeMod,
    ExprTypeNeg,
    ExprTypeEquals,
    ExprTypeNotEqual,
    ExprTypeSmallerThan,
    ExprTypeBiggerThan,
    ExprTypeSmallerOrEqual,
    ExprTypeBiggerOrEqual,
    ExprTypeAt,
    ExprTypeRedirect,
    ExprTypeSizeof,
    ExprTypeTypeof,
    ExprTypeGetString,
    ExprTypeTo,
    ExprTypeBlame,
    ExprTypeSet,
    ExprTypeAddEqual,
    ExprTypeSubEqual,
    ExprTypeMulEqual,
    ExprTypeDivEqual,
    ExprTypeModEqual,
    ExprTypeAnd,
    ExprTypeOr,
    ExprTypeNot,
    ExprTypeMatch,
    ExprTypeGetMember
};

struct CallExpr {
    struct Expr *callable_expr;
    RaelExprList args;
};

struct SetExpr {
    enum {
        SetTypeAtExpr = 1,
        SetTypeKey,
        SetTypeMember
    } set_type;
    union {
        struct Expr *as_at;
        struct Expr *as_member;
        char *as_key;
    };
    struct Expr *expr;
};

struct MatchExpr {
    size_t amount_cases;
    struct Expr *match_against;
    struct MatchCase {
        struct Expr *case_value;
        struct Instruction **case_block;
    } *match_cases;
    struct Instruction **else_block;
};

struct GetMemberExpr {
    struct Expr *lhs;
    char *key;
};

struct Expr {
    enum ExprType type;
    struct State state;
    union {
        struct {
            struct Expr *lhs, *rhs;
        };
        struct Expr *as_single;
        struct ValueExpr *as_value;
        char *as_key;
        struct CallExpr as_call;
        struct SetExpr as_set;
        struct MatchExpr as_match;
        struct GetMemberExpr as_get_member;
    };
};

enum InstructionType {
    InstructionTypeLog = 1,
    InstructionTypeIf,
    InstructionTypeLoop,
    InstructionTypePureExpr,
    InstructionTypeReturn,
    InstructionTypeBreak,
    InstructionTypeSkip,
    InstructionTypeCatch,
    InstructionTypeShow,
    InstructionTypeLoad
};

struct IfInstruction {
    enum {
        IfTypeBlock,
        IfTypeInstruction
    } if_type;
    struct Expr *condition;
    union {
        struct Instruction **if_block;
        struct Instruction *if_instruction;
    };
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
    char *value_key;
};

struct LoadInstruction {
    char *module_name;
};

struct Instruction {
    enum InstructionType type;
    struct State state;
    union {
        RaelExprList csv;
        struct IfInstruction if_stat;
        struct LoopInstruction loop;
        struct Expr *pure;
        struct Expr *return_value;
        struct CatchInstruction catch;
        struct LoadInstruction load;
    };
};

struct Parser {
    struct Lexer lexer;
    struct Instruction** instructions;
    size_t idx, allocated;
    bool in_loop;
    bool can_return;
};

struct Instruction **rael_parse(char* const filename, char* const stream, bool stream_on_heap);

void instruction_delete(struct Instruction* const instruction);

#endif // RAEL_PARSER_H
