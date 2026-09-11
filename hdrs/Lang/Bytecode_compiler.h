#ifndef LANG_BYTECODE_COMPILER_H
#define LANG_BYTECODE_COMPILER_H

#ifdef __cplusplus
extern "C"{
#endif

#include "../Allocator/Arena.h"
#include "../Data_structure/Str_base.h"
#include "../Utils/Num.h"

#include "IR_compiler.h"

enum Op_code_arg_tag{
    OP_CODE_ARG_TAG_BP,
    OP_CODE_ARG_TAG_SP,
    OP_CODE_ARG_TAG_ARGV,
    OP_CODE_ARG_TAG_BOOL,
    OP_CODE_ARG_TAG_CHAR,
    OP_CODE_ARG_TAG_INT,
    OP_CODE_ARG_TAG_FLOAT,
    OP_CODE_ARG_TAG_STR,
    OP_CODE_ARG_TAG_LIST
};

typedef struct U8_slice{
    usize m_size;
    const u8 *m_data;
} U8_slice;

typedef struct Bytecode_compile_result{
    union{
        U8_slice bytecode;
        Str_base error_info;
    };
    enum Compile_error error;
} Bytecode_compile_result;

Bytecode_compile_result bytecode_compile(Arena *arena, const Str_base *IR);

#ifdef __cplusplus
}
#endif

#endif // LANG_BYTECODE_COMPILER_H
