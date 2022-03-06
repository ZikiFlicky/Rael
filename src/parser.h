#ifndef RAEL_PARSER_H
#define RAEL_PARSER_H

#include "common.h"
#include "lexer.h"

#include <stdbool.h>

#define RAEL_INSTRUCTION_HEADER RaelInstruction _base

#define RAEL_INSTRUCTION_NEW(instruction_type, c_type) ((c_type*)instruction_new(&(instruction_type), sizeof(c_type)))

struct Expr;
typedef struct RaelInstruction RaelInstruction;
typedef struct RaelInterpreter RaelInterpreter;
typedef void (*RaelInstructionDeleteFunc)(RaelInstruction *instruction);

typedef struct RaelExprList {
    size_t amount_exprs;
    struct RaelExprListEntry {
        struct Expr *expr;
        struct State start_state;
    } *exprs;
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
    RaelInstruction **block;
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
        RaelExprList match_exprs;
        RaelInstruction **case_block;
    } *match_cases;
    RaelInstruction **else_block;
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

typedef struct RaelInstructionType {
    void (*run)(RaelInterpreter*, RaelInstruction*);
    void (*delete)(RaelInstruction*);
} RaelInstructionType;

/* declare all of the instruction types */
extern RaelInstructionType RaelInstructionTypeLog;
extern RaelInstructionType RaelInstructionTypeIf;
extern RaelInstructionType RaelInstructionTypeLoop;
extern RaelInstructionType RaelInstructionTypePureExpr;
extern RaelInstructionType RaelInstructionTypeReturn;
extern RaelInstructionType RaelInstructionTypeBreak;
extern RaelInstructionType RaelInstructionTypeSkip;
extern RaelInstructionType RaelInstructionTypeCatch;
extern RaelInstructionType RaelInstructionTypeShow;
extern RaelInstructionType RaelInstructionTypeLoad;

struct RaelInstruction {
    size_t refcount;
    RaelInstructionType *type;
    struct State state;
};

typedef struct RaelIfInstruction {
    RAEL_INSTRUCTION_HEADER;
    struct IfInstructionInfo {
        enum {
            IfTypeBlock,
            IfTypeInstruction
        } if_type;
        struct Expr *condition;
        union {
            RaelInstruction **if_block;
            RaelInstruction *if_instruction;
        };
        enum {
            ElseTypeNone,
            ElseTypeBlock,
            ElseTypeInstruction
        } else_type;
        union {
            RaelInstruction **else_block;
            RaelInstruction *else_instruction;
        };
    } info;
} RaelIfInstruction;

typedef struct RaelLoopInstruction {
    RAEL_INSTRUCTION_HEADER;
    struct LoopInstructionInfo {
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
                struct Expr *secondary_condition;
            } iterate;
        };
        RaelInstruction **block;
    } info;
} RaelLoopInstruction;

typedef struct RaelCatchInstruction {
    RAEL_INSTRUCTION_HEADER;
    struct Expr *catch_expr;
    RaelInstruction **handle_block;
    RaelInstruction **else_block;
    char *value_key;
} RaelCatchInstruction;

typedef struct RaelLoadInstruction {
    RAEL_INSTRUCTION_HEADER;
    char *module_name;
} RaelLoadInstruction;

typedef struct RaelCsvInstruction {
    RAEL_INSTRUCTION_HEADER;
    RaelExprList csv;
} RaelCsvInstruction;

typedef struct RaelPureInstruction {
    RAEL_INSTRUCTION_HEADER;
    struct Expr *expr;
} RaelPureInstruction;

typedef struct RaelReturnInstruction {
    RAEL_INSTRUCTION_HEADER;
    struct Expr *return_expr;
} RaelReturnInstruction;

typedef struct RaelParser {
    RaelLexer lexer;
    RaelInstruction** instructions;
    size_t idx, allocated;
    bool in_loop;
    bool can_return;
} RaelParser;

RaelInstruction **rael_parse(RaelStream *stream);

struct Expr *rael_parse_expr(RaelStream *stream);

void instruction_ref(RaelInstruction *instruction);

void instruction_deref(RaelInstruction* const instruction);

void expr_delete(struct Expr* const expr);

/* go through insturctions in block and instruction_deref each one of them */
void block_deref(RaelInstruction **block);

/* block_deref then free the block itself */
void block_delete(RaelInstruction **block);

#endif // RAEL_PARSER_H
