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

struct ValueExpr {
    enum ValueType type;
    union {
        struct RaelStringValue as_string;
        struct RaelNumberValue as_number;
        struct RaelRoutineValue as_routine;
        struct RaelExprList as_stack;
        enum ValueType as_type;
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
    ExprTypeMatch,
    ExprTypeGetKey
};

struct CallExpr {
    struct Expr *callable_expr;
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

struct MatchExpr {
    size_t amount_cases;
    struct Expr *match_against;
    struct MatchCase {
        struct Expr *case_value;
        struct Instruction **case_block;
    } *match_cases;
    struct Instruction **else_block;
};

struct GetKeyExpr {
    struct Expr *lhs;
    struct State key_state;
    char *at_key;
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
        struct GetKeyExpr as_getkey;
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
};

struct LoadInstruction {
    char *module_name;
};

struct Instruction {
    enum InstructionType type;
    struct State state;
    union {
        struct RaelExprList csv;
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
