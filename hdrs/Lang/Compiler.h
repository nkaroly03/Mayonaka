#ifndef LANG_COMPILER_H
#define LANG_COMPILER_H

#ifdef __cplusplus
extern "C"{
#endif

#include "../Allocator/Arena.h"
#include "../Data_structure/Str_base.h"

#include "Parser.h"

enum Op_code{
    OP_CODE_PUSH,
    OP_CODE_POP,

    OP_CODE_CALL,

    OP_CODE_RET,

    OP_CODE_JMP,
    OP_CODE_JMPZ,

    OP_CODE_TO_BOOL,
    OP_CODE_TO_CHAR,
    OP_CODE_TO_INT,
    OP_CODE_TO_FLOAT,
    OP_CODE_TO_STR,

    OP_CODE_NEG,
    OP_CODE_BNEG,

    OP_CODE_DEREF,

    OP_CODE_MOV,
    OP_CODE_MOV_DEREF,

    OP_CODE_CMP_EQ,
    OP_CODE_CMP_NEQ,
    OP_CODE_CMP_LE,
    OP_CODE_CMP_LEQ,
    OP_CODE_CMP_GE,
    OP_CODE_CMP_GEQ,

    OP_CODE_ADD,
    OP_CODE_SUB,
    OP_CODE_MUL,
    OP_CODE_DIV,
    OP_CODE_MOD,
    OP_CODE_POW,

    OP_CODE_SHL,
    OP_CODE_SHR,
    OP_CODE_BAND,
    OP_CODE_BOR,
    OP_CODE_XOR
};

const char* op_code_to_str(enum Op_code op_code);

enum Compile_error{
    COMPILE_ERROR_NONE,
    COMPILE_ERROR_OOM,
    COMPILE_ERROR_SYNTAX
};

typedef struct Compile_to_IR_result{
    union{
        Str_base IR;
        Str_base error_info;
    };
    enum Compile_error error;
} Compile_to_IR_result;

Compile_to_IR_result compile_to_IR(Arena *arena, AST_node_ptr_slice ast_nodes);

#ifdef __cplusplus
}
#endif

#endif // LANG_COMPILER_H
