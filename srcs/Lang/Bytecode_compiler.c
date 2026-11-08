#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Allocator/Arena.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Str_view.h"
#include "../../hdrs/Data_structure/Umap_base.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Cmp.h"
#include "../../hdrs/Utils/Num.h"

#include "../../hdrs/Lang/Builtin_fn.h"
#include "../../hdrs/Lang/Bytecode_compiler.h"
#include "../../hdrs/Lang/IR_compiler.h"
#include "../../hdrs/Lang/Lexer.h"

// ------------------------------------------------------------------------------------------------

static int is_newline(int c){
    return c == '\n';
}
static int is_colon(int c){
    return c == ':';
}
static int is_semicolon(int c){
    return c == ';';
}
static int is_rbracket(int c){
    return c == ']';
}
static int is_alpha_or_underscore(int c){
    return c == '_' || isalpha(c);
}
static int is_alnum_or_underscore(int c){
    return c == '_' || isalnum(c);
}
static int is_plus_or_minus(int c){
    return c == '+' || c == '-';
}

typedef struct Jmp_info{
    Str_base label;
    usize bytecode_offset;
    usize instruction_idx;
} Jmp_info;

typedef struct Bytecode_compiler_state_compile_result{
    union{
        Vec_base bytecode;
        Str_base error_info;
    };
    enum Compile_error error;
} Bytecode_compiler_state_compile_result;

typedef struct Bytecode_compiler_state{
    Allocator alloc;
    Vec_base jmp_infos;
    Umap_base label_offset_map;
    Vec_base instruction_views;
    usize instruction_idx;
    Bytecode_compiler_state_compile_result result;
} Bytecode_compiler_state;

static bool bytecode_compiler_state_oom_error(Bytecode_compiler_state *self){
    self->result.error = COMPILE_ERROR_OOM;
    return false;
}
#define oom_error() bytecode_compiler_state_oom_error(self)

static bool bytecode_compiler_state_syntax_error(Bytecode_compiler_state *self, const char *fmt, ...){
    Str_base_result error_info = str_base_init_fmt(self->alloc, "On line <" USIZE_PFMT ">: ", self->instruction_idx + 1);

    if (error_info.success){
        va_list args;
        va_start(args, fmt);
        bool success = str_base_append_fmt_va_list(&error_info.result, self->alloc, fmt, args);
        va_end(args);
        if (success){
            self->result = (Bytecode_compiler_state_compile_result){.error_info = error_info.result, .error = COMPILE_ERROR_SYNTAX};
            return false;
        }
    }
    
    return oom_error();
}
#define syntax_error(...) bytecode_compiler_state_syntax_error(self, __VA_ARGS__)

static bool bytecode_compiler_state_arg_bp_or_sp(Bytecode_compiler_state *self, Str_view rhs, bool (*bp_or_sp_fn)(Bytecode_compiler_state*, Str_view, usize*)){
    usize offset;

    if (!bp_or_sp_fn(self, rhs, &offset))
        return false;

    rhs = str_view_trim_left_while(str_view_trim_left(str_view_trim_left_while_not(rhs, is_rbracket), 1), isspace);
    if (rhs.m_size > 0 && rhs.m_str[0] != ';')
        return syntax_error("Invalid or more than 1 argument");

    for (usize i = 0; i < sizeof(offset); ++i)
        if (!vec_base_push_back(&self->result.bytecode, self->alloc, (u8*)&offset + i))
            return oom_error();

    return true;
}

static bool bytecode_compiler_state_arg_bp_fn(Bytecode_compiler_state *self, Str_view rhs, usize *offset){
    if (errno = 0, sscanf(rhs.m_str, BP_SYMBOL " [" USIZE_SFMT " ]", offset) != 1)
        return syntax_error("<" BP_SYMBOL "> must be followed by [<val>], where <val> >= 0");
    if (errno != 0)
        return syntax_error("<" BP_SYMBOL "> offset is not representable by <usize>");
    return true;
}
static bool bytecode_compiler_state_arg_sp_fn(Bytecode_compiler_state *self, Str_view rhs, usize *offset){
    if (errno = 0, sscanf(rhs.m_str, SP_SYMBOL " [ - " USIZE_SFMT " ]", offset) != 1)
        return syntax_error("<" SP_SYMBOL "> must be followed by [-<val>], where <val> > 0");
    if (errno != 0)
        return syntax_error("<" SP_SYMBOL "> offset is not representable by <usize>");
    return true;
}

static bool bytecode_compiler_state_label_to_str(Bytecode_compiler_state *self, bool is_label_decl, Str_view label_sv, Str_base *out_label_str){
    label_sv = str_view_trim_right_while(
        str_view_trim_left_while(
            str_view_trim_right(label_sv, str_view_trim_left_while_not(label_sv, is_semicolon).m_size),
            isspace
        ),
        isspace
    );

    Str_view label_sv_cpy = label_sv;

    label_sv = str_view_trim_left_while(str_view_trim_prefix(label_sv, LOCAL_LABEL_PREFIX_SYMBOL), isspace);

    if (label_sv.m_size == 0 || !is_alpha_or_underscore(label_sv.m_str[0]))
        return syntax_error("Labels must start with an ascii character or <_>");

    label_sv = str_view_trim_left_while(str_view_trim_left_while(label_sv, is_alnum_or_underscore), isspace);

    if (is_label_decl){
        if (label_sv.m_size == 0 || label_sv.m_str[0] != ':')
            return syntax_error("Label declaration must end with <:>");

        label_sv_cpy = str_view_trim_right(label_sv_cpy, str_view_trim_left_while_not(label_sv_cpy, is_colon).m_size);

        label_sv = str_view_trim_left_while(str_view_trim_left(label_sv, 1), isspace);
    }

    if (label_sv.m_size > 0)
        return syntax_error("Labels can only be followed by comments");

    Str_base label_str = {0};

    for (usize i = 0; i < label_sv_cpy.m_size; ++i)
        if (!(isspace(label_sv_cpy.m_str[i]) || str_base_push_back(&label_str, self->alloc, label_sv_cpy.m_str[i])))
            return oom_error();

    *out_label_str = label_str;

    return true;
}
#define label_to_str(is_label_decl, label_sv, out_label_str) bytecode_compiler_state_label_to_str(self, (is_label_decl), (label_sv), (out_label_str))

static bool bytecode_compiler_state_add_label(Bytecode_compiler_state *self, const Str_base *label_str){
    Umap_insert_result ires = umap_base_insert(&self->label_offset_map, self->alloc, label_str, &(usize){self->result.bytecode.m_size});
    switch (ires.error){
        case UMAP_INSERT_ERROR_NONE:             break;
        case UMAP_INSERT_ERROR_OOM:              return oom_error();
        case UMAP_INSERT_ERROR_ALREADY_INSERTED: return syntax_error("Label <%s> is already in use", str_base_data_const(label_str));
    }
    return true;
}
#define add_label(label_str) bytecode_compiler_state_add_label(self, (label_str))

static bool bytecode_compiler_state_compile(Bytecode_compiler_state *self){
    for (usize instruction_count = self->instruction_views.m_size; self->instruction_idx < instruction_count; ++self->instruction_idx){
        Str_view sv = *(Str_view*)vec_base_at(&self->instruction_views, self->instruction_idx);

        if (sv.m_size > 0 && sv.m_str[0] != ';'){
            if (!(str_view_starts_with(sv, LOCAL_LABEL_PREFIX_SYMBOL) || is_alpha_or_underscore(sv.m_str[0])))
                return syntax_error("Op code starting with <%c>", sv.m_str[0]);

            Str_view lhs = str_view_trim_right(sv, str_view_trim_left_while(sv, is_alnum_or_underscore).m_size);
            Str_view rhs = str_view_trim_left_while(str_view_trim_left(sv, lhs.m_size), isspace);

            enum Op_code op_code;
            const char *op_code_str;
            Str_view op_code_sv;

            #define match_op_code(op_code_val) \
                ( \
                    op_code = (op_code_val), \
                    op_code_str = op_code_to_str(op_code), \
                    op_code_sv = str_view_init(op_code_str), \
                    cmp_eq_Str_view(&op_code_sv, &lhs) \
                )

            if (str_view_starts_with(sv, LOCAL_LABEL_PREFIX_SYMBOL) || (rhs.m_size > 0 && rhs.m_str[0] == ':')){
                Str_base label_str;

                if (!(label_to_str(true, sv, &label_str) && add_label(&label_str)))
                    return false;

                const char *label_str_data = str_base_data(&label_str);

                if (builtin_fn_tag_init(label_str_data) != BUILTIN_FN_TAG_NONE)
                    return syntax_error("Label <%s> is reserved for a builtin function", label_str_data);
            }
            else{
                enum Op_code_arg_tag arg_tag;

                if (match_op_code(OP_CODE_PUSH)){
                    if (rhs.m_size == 0 || rhs.m_str[0] == ';')
                        return syntax_error("Op code <%s> takes 1 argument", op_code_str);

                    if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)op_code}))
                        return oom_error();

                    bool bool_val;
                    bool (*arg_fn)(Bytecode_compiler_state*, Str_view, usize*);

                    if (rhs.m_str[0] == '\'' || rhs.m_str[0] == '"'){
                        char quote = rhs.m_str[0];
                        bool is_single_quote = (quote == '\'');
                        const char *quoted_lit_type_str = token_type_to_str((is_single_quote) ? TOKEN_TYPE_CHAR : TOKEN_TYPE_STR);
                        
                        usize quote_end_pos = 0;
                        while (++quote_end_pos < rhs.m_size && rhs.m_str[quote_end_pos] != quote)
                            quote_end_pos += (rhs.m_str[quote_end_pos] == '\\');
                        if (quote_end_pos >= rhs.m_size)
                            return syntax_error("Unclosed <%s> literal", quoted_lit_type_str);

                        Str_view unquoted_sv = str_view_trim_left(str_view_trim_right(rhs, rhs.m_size - quote_end_pos), 1);

                        rhs = str_view_trim_left_while(str_view_trim_left(rhs, unquoted_sv.m_size + 2), isspace);
                        if (rhs.m_size > 0 && rhs.m_str[0] != ';')
                            return syntax_error("Invalid or more than 1 argument");

                        Str_base_unescape_result unescaped = str_base_unescape_str_view(self->alloc, unquoted_sv);
                        switch (unescaped.error){
                            case STR_UNESCAPE_ERROR_NONE:                break;
                            case STR_UNESCAPE_ERROR_OOM:                 return oom_error();
                            case STR_UNESCAPE_ERROR_BAD_ESCAPE_SEQUENCE: return syntax_error("<%s> is escaped incorrectly", quoted_lit_type_str);
                        }
                        
                        if (is_single_quote){
                            if (unquoted_sv.m_size == 0 || str_base_size(&unescaped.result) > 1)
                                return syntax_error("<char> literal must represent 1 character");
                            
                            if (!(
                                vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)OP_CODE_ARG_TAG_CHAR}) &&
                                vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)str_base_data(&unescaped.result)[0]})
                            ))
                                return oom_error();
                        }
                        else{
                            if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)OP_CODE_ARG_TAG_STR}))
                                return oom_error();
                            for (char *it = str_base_data(&unescaped.result); *it; ++it)
                                if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)*it}))
                                    return oom_error();
                            if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){'\0'}))
                                return oom_error();
                        }
                    }
                    else if (is_plus_or_minus(rhs.m_str[0]) || isdigit(rhs.m_str[0])){
                        Str_view rhs_temp = str_view_trim_left(
                            str_view_trim_right_while(
                                str_view_trim_right(rhs, str_view_trim_left_while_not(rhs, is_semicolon).m_size),
                                isspace
                            ),
                            (usize)is_plus_or_minus(rhs.m_str[0])
                        );
                        
                        if (rhs_temp.m_size == 0 || str_view_any_of(rhs_temp, isspace))
                            return syntax_error("Invalid numeric literal");

                        if (str_view_all_of(rhs_temp, isdigit)){
                            i64 int_literal = (errno = 0, (i64)strtoll(rhs.m_str, NULL, 10));
                            if (errno != 0)
                                return syntax_error("<int> literal out of range");

                            if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)OP_CODE_ARG_TAG_INT}))
                                return oom_error();
                            for (usize i = 0; i < sizeof(int_literal); ++i)
                                if (!vec_base_push_back(&self->result.bytecode, self->alloc, (u8*)&int_literal + i))
                                    return oom_error();
                        }
                        else{
                            usize dot_count = 0;
                            for (usize i = 0; i < rhs_temp.m_size; ++i){
                                char c = rhs_temp.m_str[i];
                                if ((!isdigit(c) && (c != '.' || i == rhs_temp.m_size - 1)) || ((dot_count += (c == '.'))) > 1)
                                    return syntax_error("Invalid <float> literal");
                            }

                            f64 float_literal = (errno = 0, (f64)strtod(rhs.m_str, NULL));
                            if (errno != 0)
                                return syntax_error("<float> literal out of range");

                            if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)OP_CODE_ARG_TAG_FLOAT}))
                                return oom_error();
                            for (usize i = 0; i < sizeof(float_literal); ++i)
                                if (!vec_base_push_back(&self->result.bytecode, self->alloc, (u8*)&float_literal + i))
                                    return oom_error();
                        }
                    }
                    else if (str_view_trim_prefix_in_place(&rhs, "[") && (rhs = str_view_trim_left_while(rhs, isspace), str_view_trim_prefix_in_place(&rhs, "]"))){
                        rhs = str_view_trim_left_while(rhs, isspace);
                        if (rhs.m_size > 0 && rhs.m_str[0] != ';')
                            return syntax_error("Invalid or more than 1 argument");

                        if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)OP_CODE_ARG_TAG_LIST}))
                            return oom_error();
                    }
                    else if (
                        (arg_tag = OP_CODE_ARG_TAG_BP, arg_fn = bytecode_compiler_state_arg_bp_fn, str_view_starts_with(rhs, BP_SYMBOL)) ||
                        (arg_tag = OP_CODE_ARG_TAG_SP, arg_fn = bytecode_compiler_state_arg_sp_fn, str_view_starts_with(rhs, SP_SYMBOL))
                    ){
                        if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)arg_tag}))
                            return oom_error();

                        if (!bytecode_compiler_state_arg_bp_or_sp(self, rhs, arg_fn))
                            return false;
                    }
                    else if (
                        (bool_val = false, str_view_trim_prefix_in_place(&rhs, token_type_to_str(TOKEN_TYPE_FALSE))) ||
                        (bool_val = true,  str_view_trim_prefix_in_place(&rhs, token_type_to_str(TOKEN_TYPE_TRUE )))
                    ){
                        rhs = str_view_trim_left_while(rhs, isspace);
                        if (rhs.m_size > 0 && rhs.m_str[0] != ';')
                            return syntax_error("Invalid or more than 1 argument");

                        if (!(
                            vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)OP_CODE_ARG_TAG_BOOL}) &&
                            vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)bool_val})
                        ))
                            return oom_error();
                    }
                    else
                        return syntax_error("Invalid argument <%.*s>", (int)rhs.m_size, rhs.m_str);
                }
                else if (match_op_code(OP_CODE_MOV)){
                    if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)op_code}))
                        return oom_error();

                    bool (*arg_fn)(Bytecode_compiler_state*, Str_view, usize*);

                    if (!(
                        (arg_tag = OP_CODE_ARG_TAG_BP, arg_fn = bytecode_compiler_state_arg_bp_fn, str_view_starts_with(rhs, BP_SYMBOL)) ||
                        (arg_tag = OP_CODE_ARG_TAG_SP, arg_fn = bytecode_compiler_state_arg_sp_fn, str_view_starts_with(rhs, SP_SYMBOL))
                    ))
                        return syntax_error("Op code <%s> must be followed by <" BP_SYMBOL "> or <" SP_SYMBOL ">", op_code_str);

                    if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)arg_tag}))
                        return oom_error();

                    if (!bytecode_compiler_state_arg_bp_or_sp(self, rhs, arg_fn))
                        return false;
                }
                else if (match_op_code(OP_CODE_CALL)){
                    if (rhs.m_size == 0 || rhs.m_str[0] == ';')
                        return syntax_error("Op code <%s> takes in a label", op_code_str);

                    Str_base label_str;
                    if (!label_to_str(false, rhs, &label_str))
                        return false;

                    if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)op_code}))
                        return oom_error();

                    enum Builtin_fn_tag bfn_tag = builtin_fn_tag_init(str_base_data(&label_str));

                    if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)bfn_tag}))
                        return oom_error();

                    if (bfn_tag == BUILTIN_FN_TAG_NONE){
                        if (!vec_base_push_back(
                            &self->jmp_infos,
                            self->alloc,
                            &(Jmp_info){.label = label_str, .bytecode_offset = self->result.bytecode.m_size, .instruction_idx = self->instruction_idx}
                        ))
                            return oom_error();

                        usize label_bytecode_offset = 0;

                        for (usize i = 0; i < sizeof(label_bytecode_offset); ++i)
                            if (!vec_base_push_back(&self->result.bytecode, self->alloc, (u8*)&label_bytecode_offset + i))
                                return oom_error();
                    }
                }
                else if (match_op_code(OP_CODE_POP) || match_op_code(OP_CODE_RET) || match_op_code(OP_CODE_RETV)){
                    rhs = str_view_trim_right_while(str_view_trim_right(rhs, str_view_trim_left_while_not(rhs, is_semicolon).m_size), isspace);
                    if (rhs.m_size == 0 || !str_view_all_of(rhs, isdigit))
                        return syntax_error("Op code <%s> takes in a positive integer literal", op_code_str);

                    if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)op_code}))
                        return oom_error();

                    usize pop_count = (errno = 0, (usize)strtoull(rhs.m_str, NULL, 10));
                    if (errno != 0)
                        return syntax_error("<int> literal out of range");

                    for (usize i = 0; i < sizeof(pop_count); ++i)
                        if (!vec_base_push_back(&self->result.bytecode, self->alloc, (u8*)&pop_count + i))
                            return oom_error();
                }
                else if (match_op_code(OP_CODE_JMP) || match_op_code(OP_CODE_JMPZ)){
                    if (rhs.m_size == 0 || rhs.m_str[0] == ';')
                        return syntax_error("Op code <%s> takes in a label", op_code_str);

                    Str_base label_str;
                    if (!label_to_str(false, rhs, &label_str))
                        return false;

                    if (!(
                        vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)op_code}) &&
                        vec_base_push_back(
                            &self->jmp_infos,
                            self->alloc,
                            &(Jmp_info){.label = label_str, .bytecode_offset = self->result.bytecode.m_size, .instruction_idx = self->instruction_idx}
                        )
                    ))
                        return oom_error();
                    
                    usize label_bytecode_offset = 0;

                    for (usize i = 0; i < sizeof(label_bytecode_offset); ++i)
                        if (!vec_base_push_back(&self->result.bytecode, self->alloc, (u8*)&label_bytecode_offset + i))
                            return oom_error();
                }
                else if (
                    match_op_code(OP_CODE_TO_BOOL  ) ||
                    match_op_code(OP_CODE_TO_CHAR  ) ||
                    match_op_code(OP_CODE_TO_INT   ) ||
                    match_op_code(OP_CODE_TO_FLOAT ) ||
                    match_op_code(OP_CODE_TO_STR   ) ||
                    match_op_code(OP_CODE_NEG      ) ||
                    match_op_code(OP_CODE_BNEG     ) ||
                    match_op_code(OP_CODE_DEREF    ) ||
                    match_op_code(OP_CODE_POW      ) ||
                    match_op_code(OP_CODE_MUL      ) ||
                    match_op_code(OP_CODE_DIV      ) ||
                    match_op_code(OP_CODE_REM      ) ||
                    match_op_code(OP_CODE_ADD      ) ||
                    match_op_code(OP_CODE_SUB      ) ||
                    match_op_code(OP_CODE_SHL      ) ||
                    match_op_code(OP_CODE_SHR      ) ||
                    match_op_code(OP_CODE_CMP_LT   ) ||
                    match_op_code(OP_CODE_CMP_LE   ) ||
                    match_op_code(OP_CODE_CMP_GT   ) ||
                    match_op_code(OP_CODE_CMP_GE   ) ||
                    match_op_code(OP_CODE_CMP_EQ   ) ||
                    match_op_code(OP_CODE_CMP_NEQ  ) ||
                    match_op_code(OP_CODE_BAND     ) ||
                    match_op_code(OP_CODE_XOR      ) ||
                    match_op_code(OP_CODE_BOR      ) ||
                    match_op_code(OP_CODE_MOV_DEREF)
                ){
                    if (rhs.m_size > 0 && rhs.m_str[0] != ';')
                        return syntax_error("Op code <%s> takes no arguments", op_code_str);

                    if (!vec_base_push_back(&self->result.bytecode, self->alloc, &(u8){(u8)op_code}))
                        return oom_error();
                }
                else
                    return syntax_error("Unknown op code <%.*s>", (int)lhs.m_size, lhs.m_str);
            }
        }
    }

    vec_base_for_each(self->jmp_infos, it){
        Jmp_info *jmp_info = it;

        usize *label_offset = umap_base_at_key(&self->label_offset_map, &jmp_info->label).m_value;
        if (!label_offset){
            self->instruction_idx = jmp_info->instruction_idx;
            return syntax_error("Use of undeclared label <%s>", str_base_data(&jmp_info->label));
        }

        memcpy(vec_base_at(&self->result.bytecode, jmp_info->bytecode_offset), label_offset, sizeof(*label_offset));
    }

    return true;
}

// ------------------------------------------------------------------------------------------------

Bytecode_compiler_compile_result bytecode_compiler_compile(Arena *arena, const Str_base *IR){
    assert(arena && "<arena> is not nullable");
    assert(IR && "<IR> is not nullable");

    Bytecode_compiler_state state = {
        .alloc             = arena_allocator(arena),
        .jmp_infos         = vec_base_init(Jmp_info),
        .label_offset_map  = umap_base_init(Str_base, usize),
        .instruction_views = vec_base_init(Str_view),
        .instruction_idx   = 0,
        .result            = {.bytecode = vec_base_init(u8), .error = COMPILE_ERROR_NONE}
    };

    for (Str_view sv = str_base_to_str_view(IR), temp; sv.m_size > 0; sv = str_view_trim_left(sv, temp.m_size + 1)){
        temp = str_view_trim_right(sv, str_view_trim_left_while_not(sv, is_newline).m_size);
        Str_view trimmed = str_view_trim_right_while(str_view_trim_left_while(temp, isspace), isspace);
        if (!vec_base_push_back(&state.instruction_views, state.alloc, &trimmed))
            return (Bytecode_compiler_compile_result){.error = COMPILE_ERROR_OOM};
    }

    return (bytecode_compiler_state_compile(&state))
        ? (Bytecode_compiler_compile_result){.bytecode = {.m_size = state.result.bytecode.m_size, .m_data = state.result.bytecode.m_data}, .error = COMPILE_ERROR_NONE}
        : (Bytecode_compiler_compile_result){.error_info = state.result.error_info, .error = state.result.error}
    ;
}
