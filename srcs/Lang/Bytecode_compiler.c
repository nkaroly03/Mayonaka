#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Allocator/Arena.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Str_view.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Cmp.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

#include "../../hdrs/Lang/Builtin_fn.h"
#include "../../hdrs/Lang/Bytecode_compiler.h"
#include "../../hdrs/Lang/IR_compiler.h"
#include "../../hdrs/Lang/Type_info.h"

// ------------------------------------------------------------------------------------------------

static int is_newline(int c){
    return c == '\n';
}
static int is_colon(int c){
    return c == ':';
}
static int is_rbracket(int c){
    return c == ']';
}

typedef struct Bytecode_compiler_state{
    Allocator alloc;
    Vec_base instruction_views;
    Vec_base bytecode;
} Bytecode_compiler_state;

static const Bytecode_compile_result OOM_ERROR = {.error = COMPILE_ERROR_OOM};

static Bytecode_compile_result bytecode_compiler_syntax_error(Bytecode_compiler_state *self, const char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    Str_base_result error_info = str_base_init_fmt_va_list(self->alloc, fmt, args);
    va_end(args);
    
    return (error_info.success) ? (Bytecode_compile_result){.error_info = error_info.result, .error = COMPILE_ERROR_SYNTAX} : OOM_ERROR;
}
#define syntax_error(...) bytecode_compiler_syntax_error(self, "On line <" USIZE_PFMT ">: " __VA_ARGS__)

static Bytecode_compile_result bytecode_compiler_state_compile(Bytecode_compiler_state *self){
    usize instruction_count = self->instruction_views.m_size;
    
    fprintf(stderr, "\n================================================================================================\n");

    for (usize i = 0; i < instruction_count; ++i){
        Str_view sv = *(Str_view*)vec_base_at(&self->instruction_views, i);
        if (sv.m_size > 0){
            if (str_view_starts_with(sv, LABEL_SYMBOL)){
                sv = str_view_trim_left_while_not(sv, is_colon);
                if (sv.m_size == 0)
                    return syntax_error("Label must end with <:>", i + 1);
                sv = str_view_trim_left_while(str_view_trim_left(sv, 1), isspace);
                if (sv.m_size > 0 && sv.m_str[0] != ';')
                    return syntax_error("Label must not be followed by any statement besides a comment", i + 1);
            }
            else{
                enum Op_code op_code;
                const char *op_code_str;
                Str_view op_code_sv;

                Str_view lhs = str_view_trim_right(sv, str_view_trim_left_while_not(sv, isspace).m_size);
                #define op_code_match(op_code_val) \
                    ( \
                        op_code = (op_code_val), \
                        op_code_str = op_code_to_str(op_code), \
                        op_code_sv = str_view_init(op_code_str), \
                        cmp_eq_Str_view(&op_code_sv, &lhs) \
                    )

                Str_view rhs = str_view_trim_left_while(str_view_trim_left(sv, lhs.m_size), isspace);

                if (op_code_match(OP_CODE_PUSH)){
                    if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)op_code}))
                        return OOM_ERROR;

                    enum Type_info_tag type_info_tag;
                    const char *rhs_starts_with;

                    bool bool_val;

                    if (str_view_starts_with(rhs, SP_SYMBOL)){
                        union{
                            usize sp_offset_as_usize;
                            u8 sp_offset_as_u8s[sizeof(usize)];
                        } sp_offset;

                        if (sscanf(rhs.m_str, SP_SYMBOL " [ - " USIZE_SFMT " ]", &sp_offset.sp_offset_as_usize) != 1)
                            return syntax_error("<" SP_SYMBOL "> must be followed by <[-<val>]>, where <val> is an integer", i + 1);
                        if (errno != 0)
                            return syntax_error("<" SP_SYMBOL "> offset is not representable by <usize>", i + 1);

                        rhs = str_view_trim_left_while(str_view_trim_left(str_view_trim_left_while_not(rhs, is_rbracket), 1), isspace);
                        if (rhs.m_size > 0 && rhs.m_str[0] != ';')
                            return syntax_error("Op code <%s> only supports 1 argument", i + 1, op_code_str);

                        if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)TYPE_INFO_TAG_NONE}))
                            return OOM_ERROR;

                        for (usize j = 0; j < array_size(sp_offset.sp_offset_as_u8s); ++j)
                            if (!vec_base_push_back(&self->bytecode, self->alloc, &sp_offset.sp_offset_as_u8s[j]))
                                return OOM_ERROR;
                    }
                    else if (
                        (type_info_tag = (enum Type_info_tag)-1, rhs_starts_with = "argv", str_view_starts_with(rhs, rhs_starts_with)) ||
                        (type_info_tag = TYPE_INFO_TAG_VOID,     rhs_starts_with = "[]",   str_view_starts_with(rhs, rhs_starts_with))
                    ){
                        rhs = str_view_trim_left_while(str_view_trim_prefix(rhs, rhs_starts_with), isspace);
                        if (rhs.m_size > 0 && rhs.m_str[0] != ';')
                            return syntax_error("Op code <%s> only supports 1 argument", i + 1, op_code_str);

                        if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)type_info_tag}))
                            return OOM_ERROR;
                    }
                    else if (
                        (bool_val = false, rhs_starts_with = "false", str_view_starts_with(rhs, rhs_starts_with)) ||
                        (bool_val = true,  rhs_starts_with = "true",  str_view_starts_with(rhs, rhs_starts_with))
                    ){
                        rhs = str_view_trim_prefix(rhs, rhs_starts_with);
                        if (rhs.m_size > 0 && !isspace(rhs.m_str[0]) && rhs.m_str[0] != ';')
                            return syntax_error("Op code <%s> only supports 1 argument", i + 1, op_code_str);

                        if (
                            !vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)TYPE_INFO_TAG_BOOL}) ||
                            !vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)bool_val})
                        )
                            return OOM_ERROR;
                    }
                    else if (str_view_starts_with(rhs, "'") || str_view_starts_with(rhs, "\"")){
                        char quote = rhs.m_str[0];
                        
                        usize quote_end_pos = 0;
                        while (++quote_end_pos < rhs.m_size && rhs.m_str[quote_end_pos] != quote)
                            quote_end_pos += (rhs.m_str[quote_end_pos] == '\\');

                        if (quote_end_pos >= rhs.m_size)
                            return syntax_error("Unclosed <%s> literal", i + 1, (quote == '\'') ? "char" : "str");

                        Str_view unquoted_sv = str_view_trim_left(str_view_trim_right(rhs, rhs.m_size - quote_end_pos), 1);

                        Str_base_unescape_result unescaped = str_base_unescape_str_view(self->alloc, unquoted_sv);
                        switch (unescaped.error){
                            case STR_UNESCAPE_ERROR_NONE:                break;
                            case STR_UNESCAPE_ERROR_OOM:                 return OOM_ERROR;
                            case STR_UNESCAPE_ERROR_BAD_ESCAPE_SEQUENCE: return syntax_error("<%s> is escaped incorrectly", i + 1, (quote == '\'') ? "char" : "str");
                        }
                        
                        if (quote == '\''){
                            if (unquoted_sv.m_size == 0 || str_base_size(&unescaped.result) > 1)
                                return syntax_error("<char> literal must represent 1 character", i + 1);
                            
                            if (
                                !vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)TYPE_INFO_TAG_CHAR}) ||
                                !vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)str_base_data(&unescaped.result)[0]})
                            )
                                return OOM_ERROR;
                        }
                        else{
                            if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)TYPE_INFO_TAG_STR}))
                                return OOM_ERROR;
                            for (char *it = str_base_data(&unescaped.result); *it; ++it)
                                if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)*it}))
                                    return OOM_ERROR;
                            if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)'\0'}))
                                return OOM_ERROR;
                        }
                    }
                    else{
                        // -10
                        // 3.14
                    }
                }
                else if (op_code_match(OP_CODE_CALL)){

                }
                else if (op_code_match(OP_CODE_JMP)){

                }
                else if (op_code_match(OP_CODE_JMPZ)){

                }
                else if (op_code_match(OP_CODE_MOV)){

                }

                else if (
                    op_code_match(OP_CODE_POP      ) ||
                    op_code_match(OP_CODE_RET      ) ||
                    op_code_match(OP_CODE_TO_BOOL  ) ||
                    op_code_match(OP_CODE_TO_CHAR  ) ||
                    op_code_match(OP_CODE_TO_INT   ) ||
                    op_code_match(OP_CODE_TO_FLOAT ) ||
                    op_code_match(OP_CODE_TO_STR   ) ||
                    op_code_match(OP_CODE_NEG      ) ||
                    op_code_match(OP_CODE_BNEG     ) ||
                    op_code_match(OP_CODE_MOV_DEREF) ||
                    op_code_match(OP_CODE_DEREF    ) ||
                    op_code_match(OP_CODE_CMP_EQ   ) ||
                    op_code_match(OP_CODE_CMP_NEQ  ) ||
                    op_code_match(OP_CODE_CMP_LE   ) ||
                    op_code_match(OP_CODE_CMP_LEQ  ) ||
                    op_code_match(OP_CODE_CMP_GE   ) ||
                    op_code_match(OP_CODE_CMP_GEQ  ) ||
                    op_code_match(OP_CODE_ADD      ) ||
                    op_code_match(OP_CODE_SUB      ) ||
                    op_code_match(OP_CODE_MUL      ) ||
                    op_code_match(OP_CODE_DIV      ) ||
                    op_code_match(OP_CODE_MOD      ) ||
                    op_code_match(OP_CODE_POW      ) ||
                    op_code_match(OP_CODE_SHL      ) ||
                    op_code_match(OP_CODE_SHR      ) ||
                    op_code_match(OP_CODE_BAND     ) ||
                    op_code_match(OP_CODE_BOR      ) ||
                    op_code_match(OP_CODE_XOR      )
                ){
                    if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)op_code}))
                        return OOM_ERROR;
                    rhs = str_view_trim_left_while(str_view_trim_left_while(rhs, isalpha), isspace);
                    if (rhs.m_size > 0 && rhs.m_str[0] != ';')
                        return syntax_error("Op code <%s> supports no arguments", i + 1, op_code_str);
                }

                else{
                    fprintf(stderr, "Unknown op code\n");
                    abort();
                }
            }
        }
    }

    return (Bytecode_compile_result){.bytecode = self->bytecode, .error = COMPILE_ERROR_NONE};
}

// ------------------------------------------------------------------------------------------------

Bytecode_compile_result bytecode_compile(Arena *arena, const Str_base *IR){
    assert(arena && "<arena> is not nullable");
    assert(IR && "<IR> is not nullable");

    Bytecode_compiler_state state = {
        .alloc             = arena_allocator(arena),
        .instruction_views = vec_base_init(Str_view),
        .bytecode          = vec_base_init(u8)
    };

    for (Str_view sv = str_base_to_str_view(IR), temp; sv.m_size > 0; sv = str_view_trim_left(sv, temp.m_size + 1)){
        temp = str_view_trim_right(sv, str_view_trim_left_while_not(sv, is_newline).m_size);
        Str_view trimmed = str_view_trim_right_while(str_view_trim_left_while(temp, isspace), isspace);
        if (!vec_base_push_back(&state.instruction_views, state.alloc, &trimmed))
            return (Bytecode_compile_result){.error = COMPILE_ERROR_OOM};
    }

    return bytecode_compiler_state_compile(&state);
}
