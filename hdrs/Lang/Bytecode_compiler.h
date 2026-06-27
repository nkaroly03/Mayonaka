#ifndef LANG_BYTECODE_COMPILER_H
#define LANG_BYTECODE_COMPILER_H

#ifdef __cplusplus
extern "C"{
#endif

#include "../Allocator/Arena.h"
#include "../Data_structure/Str_base.h"
#include "../Data_structure/Vec_base.h"

#include "IR_compiler.h"

enum Op_code_push_tag{
    OP_CODE_PUSH_TAG_SP,
    OP_CODE_PUSH_TAG_ARGV,
    OP_CODE_PUSH_TAG_BOOL,
    OP_CODE_PUSH_TAG_CHAR,
    OP_CODE_PUSH_TAG_INT,
    OP_CODE_PUSH_TAG_FLOAT,
    OP_CODE_PUSH_TAG_STR,
    OP_CODE_PUSH_TAG_LIST
};

typedef struct Bytecode_compile_result{
    union{
        Vec_base bytecode;
        Str_base error_info;
    };
    enum Compile_error error;
} Bytecode_compile_result;

Bytecode_compile_result bytecode_compile(Arena *arena, const Str_base *IR);

#ifdef __cplusplus
}
#endif

#endif // LANG_BYTECODE_COMPILER_H
