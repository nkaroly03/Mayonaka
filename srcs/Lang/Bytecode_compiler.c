#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
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
static int is_semicolon(int c){
    return c == ';';
}
static int is_rbracket(int c){
    return c == ']';
}

typedef struct Bytecode_compiler_state{
    Allocator alloc;
    Vec_base instruction_views;
    usize instruction_idx;
    Vec_base bytecode;
} Bytecode_compiler_state;

static const Bytecode_compile_result OOM_ERROR = {.error = COMPILE_ERROR_OOM};

static Bytecode_compile_result bytecode_compiler_syntax_error(Bytecode_compiler_state *self, const char *fmt, ...){
    Str_base_result error_info = str_base_init_fmt(self->alloc, "On line <" USIZE_PFMT ">: ", self->instruction_idx + 1);

    if (error_info.success){
        va_list args;
        va_start(args, fmt);
        bool append_result = str_base_append_fmt_va_list(&error_info.result, self->alloc, fmt, args);
        va_end(args);
        error_info.success = append_result;
    }
    
    return (error_info.success) ? (Bytecode_compile_result){.error_info = error_info.result, .error = COMPILE_ERROR_SYNTAX} : OOM_ERROR;
}
#define syntax_error(...) bytecode_compiler_syntax_error(self, __VA_ARGS__)

static Bytecode_compile_result bytecode_compiler_state_compile(Bytecode_compiler_state *self){
    usize instruction_count = self->instruction_views.m_size;
    
    fprintf(stderr, "\n================================================================================================\n");

    for (; self->instruction_idx < instruction_count; ++self->instruction_idx){
        Str_view sv = *(Str_view*)vec_base_at(&self->instruction_views, self->instruction_idx);
        if (sv.m_size > 0 && sv.m_str[0] != ';'){
            if (str_view_starts_with(sv, ".")){
                sv = str_view_trim_left_while(str_view_trim_left_while(str_view_trim_prefix(sv, "."), isalnum), isspace);
                if (sv.m_size == 0 || sv.m_str[0] != ':')
                    return syntax_error("Label must end with <:>");
                sv = str_view_trim_left_while(str_view_trim_prefix(sv, ":"), isspace);
                if (sv.m_size > 0 && sv.m_str[0] != ';')
                    return syntax_error("Label must not be followed by any statement besides a comment");
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

                if (lhs.m_size > 0 && !isalpha(lhs.m_str[0]) && lhs.m_str[0] != '_')
                    return syntax_error("Op code starting with %c", lhs.m_str[0]);

                Str_view rhs = str_view_trim_left_while(str_view_trim_left(sv, lhs.m_size), isspace);

                if (op_code_match(OP_CODE_PUSH)){
                    if (rhs.m_size == 0 || rhs.m_str[0] == ';')
                        return syntax_error("Op code <%s> takes 1 argument", op_code_str);

                    if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)op_code}))
                        return OOM_ERROR;

                    enum Op_code_push_tag push_tag;
                    const char *rhs_starts_with;

                    bool bool_val;

                    if (str_view_starts_with(rhs, SP_SYMBOL)){
                        union{
                            usize as_usize;
                            u8 as_u8s[sizeof(usize)];
                        } sp_offset;

                        errno = 0;
                        if (sscanf(rhs.m_str, SP_SYMBOL " [ - " USIZE_SFMT " ]", &sp_offset.as_usize) != 1)
                            return syntax_error("<" SP_SYMBOL "> must be followed by <[-<val>]>, where <val> is an integer");
                        if (errno != 0)
                            return syntax_error("<" SP_SYMBOL "> offset is not representable by <usize>");

                        rhs = str_view_trim_left_while(str_view_trim_left(str_view_trim_left_while_not(rhs, is_rbracket), 1), isspace);
                        if (rhs.m_size > 0 && rhs.m_str[0] != ';')
                            return syntax_error("Invalid or more than 1 argument");

                        if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)OP_CODE_PUSH_TAG_SP}))
                            return OOM_ERROR;

                        for (usize j = 0; j < array_size(sp_offset.as_u8s); ++j)
                            if (!vec_base_push_back(&self->bytecode, self->alloc, &sp_offset.as_u8s[j]))
                                return OOM_ERROR;
                    }
                    else if (
                        (push_tag = OP_CODE_PUSH_TAG_ARGV, rhs_starts_with = "argv", str_view_starts_with(rhs, rhs_starts_with)) ||
                        (push_tag = OP_CODE_PUSH_TAG_LIST, rhs_starts_with = "[]",   str_view_starts_with(rhs, rhs_starts_with))
                    ){
                        rhs = str_view_trim_left_while(str_view_trim_prefix(rhs, rhs_starts_with), isspace);
                        if (rhs.m_size > 0 && rhs.m_str[0] != ';')
                            return syntax_error("Invalid or more than 1 argument");

                        if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)push_tag}))
                            return OOM_ERROR;
                    }
                    else if (
                        (bool_val = false, rhs_starts_with = "false", str_view_starts_with(rhs, rhs_starts_with)) ||
                        (bool_val = true,  rhs_starts_with = "true",  str_view_starts_with(rhs, rhs_starts_with))
                    ){
                        rhs = str_view_trim_left_while(str_view_trim_prefix(rhs, rhs_starts_with), isspace);
                        if (rhs.m_size > 0 && rhs.m_str[0] != ';')
                            return syntax_error("Invalid or more than 1 argument");

                        if (
                            !vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)OP_CODE_PUSH_TAG_BOOL}) ||
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
                            return syntax_error("Unclosed <%s> literal", (quote == '\'') ? "char" : "str");

                        Str_view unquoted_sv = str_view_trim_left(str_view_trim_right(rhs, rhs.m_size - quote_end_pos), 1);

                        rhs = str_view_trim_left_while(str_view_trim_left(rhs, unquoted_sv.m_size + 2), isspace);
                        if (rhs.m_size > 0 && rhs.m_str[0] != ';')
                            return syntax_error("Invalid or more than 1 argument");

                        Str_base_unescape_result unescaped = str_base_unescape_str_view(self->alloc, unquoted_sv);
                        switch (unescaped.error){
                            case STR_UNESCAPE_ERROR_NONE:                break;
                            case STR_UNESCAPE_ERROR_OOM:                 return OOM_ERROR;
                            case STR_UNESCAPE_ERROR_BAD_ESCAPE_SEQUENCE: return syntax_error("<%s> is escaped incorrectly", (quote == '\'') ? "char" : "str");
                        }
                        
                        if (quote == '\''){
                            if (unquoted_sv.m_size == 0 || str_base_size(&unescaped.result) > 1)
                                return syntax_error("<char> literal must represent 1 character");
                            
                            if (
                                !vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)OP_CODE_PUSH_TAG_CHAR}) ||
                                !vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)str_base_data(&unescaped.result)[0]})
                            )
                                return OOM_ERROR;
                        }
                        else{
                            if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)OP_CODE_PUSH_TAG_STR}))
                                return OOM_ERROR;
                            for (char *it = str_base_data(&unescaped.result); *it; ++it)
                                if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)*it}))
                                    return OOM_ERROR;
                            if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)'\0'}))
                                return OOM_ERROR;
                        }
                    }
                    else{
                        rhs = str_view_trim_right_while(str_view_trim_right(rhs, str_view_trim_left_while_not(rhs, is_semicolon).m_size), isspace);

                        Str_view rhs_temp = rhs;
                        if (rhs.m_str[0] == '+' || rhs.m_str[0] == '-')
                            rhs_temp = str_view_trim_left(rhs, 1);
                        else if (!isdigit(rhs.m_str[0]))
                            return syntax_error("Numeric literal must start with a digit or unary <+/->");

                        if (str_view_all_of(rhs_temp, isdigit)){
                            union{
                                i64 as_i64;
                                u8 as_u8s[sizeof(i64)];
                            } int_literal = {.as_i64 = (errno = 0, strtoll(rhs.m_str, NULL, 10))};
                            if (errno != 0)
                                return syntax_error("<int> literal out of range");

                            if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)OP_CODE_PUSH_TAG_INT}))
                                return OOM_ERROR;
                            for (usize j = 0; j < array_size(int_literal.as_u8s); ++j)
                                if (!vec_base_push_back(&self->bytecode, self->alloc, &int_literal.as_u8s[j]))
                                    return OOM_ERROR;
                        }
                        else if (str_view_none_of(rhs_temp, isspace)){
                            usize dot_count = 0;
                            for (usize j = 0; j < rhs_temp.m_size; ++j){
                                char c = rhs_temp.m_str[j];
                                if (!isdigit(c) && c != '.')
                                    return syntax_error("<float> literal containing non-digit characters");
                                dot_count += (c == '.');
                                if (dot_count > 1)
                                    return syntax_error("<float> literal containing more than 1 <.>");
                            }

                            union{
                                f64 as_f64;
                                u8 as_u8s[sizeof(f64)];
                            } float_literal = {.as_f64 = (errno = 0, strtod(rhs.m_str, NULL))};
                            if (errno != 0)
                                return syntax_error("<float> literal out of range");

                            if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)OP_CODE_PUSH_TAG_FLOAT}))
                                return OOM_ERROR;

                            for (usize j = 0; j < array_size(float_literal.as_u8s); ++j)
                                if (!vec_base_push_back(&self->bytecode, self->alloc, &float_literal.as_u8s[j]))
                                    return OOM_ERROR;
                        }
                        else
                            return syntax_error("Numeric literal containing spaces");
                    }
                }
                else if (op_code_match(OP_CODE_MOV)){

                }
                else if (op_code_match(OP_CODE_CALL)){

                }
                else if (op_code_match(OP_CODE_JMP) || op_code_match(OP_CODE_JMPZ)){

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
                    rhs = str_view_trim_left_while(str_view_trim_left_while(rhs, isalpha), isspace);
                    if (rhs.m_size > 0 && rhs.m_str[0] != ';')
                        return syntax_error("Op code <%s> takes no arguments", op_code_str);

                    if (!vec_base_push_back(&self->bytecode, self->alloc, &(u8){(u8)op_code}))
                        return OOM_ERROR;
                }

                else
                    return syntax_error("Unknown op code <%.*s>", (int)lhs.m_size, lhs.m_str);
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
        .instruction_idx   = 0,
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
