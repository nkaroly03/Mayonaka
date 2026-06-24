#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../hdrs/Allocator/Arena.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Umap_base.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Num.h"

#include "../../hdrs/Lang/Compiler.h"
#include "../../hdrs/Lang/Parser.h"
#include "../../hdrs/Lang/Type_info.h"

// ------------------------------------------------------------------------------------------------

static const char *TYPE_INFO_TAG_SYMBOLS[] = {
    [TYPE_INFO_TAG_NONE]  = "none",
    [TYPE_INFO_TAG_VOID]  = "void",
    [TYPE_INFO_TAG_BOOL]  = "bool",
    [TYPE_INFO_TAG_CHAR]  = "char",
    [TYPE_INFO_TAG_INT]   = "int",
    [TYPE_INFO_TAG_FLOAT] = "float",
    [TYPE_INFO_TAG_STR]   = "str",
};

static const char *OP_CODE_SYMBOLS[] = {
    [OP_CODE_PUSH]      = "push",
    [OP_CODE_POP]       = "pop",

    [OP_CODE_CALL]      = "call",
    [OP_CODE_RET]       = "ret",

    [OP_CODE_JMP]       = "jmp",
    [OP_CODE_JMPZ]      = "jmpz",

    [OP_CODE_TO_BOOL]   = "to_bool",
    [OP_CODE_TO_CHAR]   = "to_char",
    [OP_CODE_TO_INT]    = "to_int",
    [OP_CODE_TO_FLOAT]  = "to_float",
    [OP_CODE_TO_STR]    = "to_str",

    [OP_CODE_NEG]       = "neg",
    [OP_CODE_BNEG]      = "bneg",

    [OP_CODE_DEREF]     = "deref",

    [OP_CODE_MOV]       = "mov",
    [OP_CODE_MOV_DEREF] = "mov_deref",

    [OP_CODE_CMP_EQ]    = "cmp_eq",
    [OP_CODE_CMP_NEQ]   = "cmp_neq",
    [OP_CODE_CMP_LE]    = "cmp_le",
    [OP_CODE_CMP_LEQ]   = "cmp_leq",
    [OP_CODE_CMP_GE]    = "cmp_ge",
    [OP_CODE_CMP_GEQ]   = "cmp_geq",

    [OP_CODE_ADD]       = "add",
    [OP_CODE_SUB]       = "sub",
    [OP_CODE_MUL]       = "mul",
    [OP_CODE_DIV]       = "div",
    [OP_CODE_MOD]       = "mod",
    [OP_CODE_POW]       = "pow",

    [OP_CODE_SHL]       = "shl",
    [OP_CODE_SHR]       = "shr",
    [OP_CODE_BAND]      = "band",
    [OP_CODE_BOR]       = "bor",
    [OP_CODE_XOR]       = "xor"
};

static Str_base_result type_info_to_str_base(Type_info type_info, Allocator alloc){
    Str_base result = {0};

    for (usize i = 0; i < type_info.m_dimensions; ++i)
        if (!str_base_append_raw(&result, alloc, "[]"))
            goto oom_error;

    if (!str_base_append_raw(&result, alloc, TYPE_INFO_TAG_SYMBOLS[type_info.m_tag]))
        goto oom_error;

    return (Str_base_result){.result = result, .success = true};

oom_error:
    str_base_deinit(&result, alloc);
    return (Str_base_result){0};
}

typedef struct Id_info{
    usize stack_idx;
    Type_info type_info;
} Id_info;

typedef struct Compiler_state{
    Allocator alloc;
    Vec_base id_stack;
    Umap_base id_info_map;
    Vec_base let_decl_counts;
    Vec_base type_info_stack;
    Str_base IR;
} Compiler_state;

typedef struct Compiler_state_to_IR_result{
    Str_base error_info;
    enum Compile_error error;
} Compiler_state_to_IR_result;

static const Compiler_state_to_IR_result OOM_ERROR = {.error = COMPILE_ERROR_OOM};

static Compiler_state_to_IR_result compiler_state_syntax_error(Compiler_state *self, const char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    Str_base_result error_info = str_base_init_fmt_va_list(self->alloc, fmt, args);
    va_end(args);

    return (error_info.success)
        ? (Compiler_state_to_IR_result){.error_info = error_info.result, .error = COMPILE_ERROR_SYNTAX}
        : (Compiler_state_to_IR_result){.error = COMPILE_ERROR_OOM}
    ;
}
#define syntax_error(...) compiler_state_syntax_error(self, "On line <" USIZE_PFMT ">: " __VA_ARGS__)

#define INDENT "    "
static bool compiler_state_add_instruction(Compiler_state *self, const char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    bool result = 
        str_base_append_raw(&self->IR, self->alloc, INDENT) &&
        str_base_append_fmt_va_list(&self->IR, self->alloc, fmt, args) &&
#ifndef NDEBUG
        str_base_append_fmt(&self->IR, self->alloc, " ; " USIZE_PFMT, self->type_info_stack.m_size) &&
#endif // NDEBUG
        str_base_push_back(&self->IR, self->alloc, '\n')
    ;
    va_end(args);

    return result;
}
#define add_instruction(...) \
    do{ \
        if (!compiler_state_add_instruction(self, __VA_ARGS__)) \
            return OOM_ERROR; \
    } while (0)

static bool compiler_state_pop_on_discarded_expression(Compiler_state *self, const AST_node *ast_node){
    if (ast_node->m_parent){
        switch (ast_node->m_parent->m_token->m_type){
            case TOKEN_TYPE_LBRACE:
            case TOKEN_TYPE_IF:
            case TOKEN_TYPE_ELSE:
            case TOKEN_TYPE_WHILE:
                break;
            default:
                return true;
        }
    }

    vec_base_pop_back_discard(&self->type_info_stack);

    return compiler_state_add_instruction(self, "%s", OP_CODE_SYMBOLS[OP_CODE_POP]);
}
#define pop_on_discarded_expression(ast_node) \
    do{ \
        if (!compiler_state_pop_on_discarded_expression(self, (ast_node))) \
            return OOM_ERROR; \
    } while (0)

static const char SP_SYMBOL[] = "sp";

static Compiler_state_to_IR_result compiler_state_to_IR(Compiler_state *self, const AST_node *ast_node){
    enum Binary_op bin_op;
    enum Op_code op_code;

    switch (ast_node->m_token->m_type){
        case TOKEN_TYPE_ID:
            {
                Umap_pair pair = umap_base_get_pair(&self->id_info_map, &ast_node->m_token->m_id);
                if (!pair.m_key)
                    return syntax_error("Use of undeclared identifier <%s>", ast_node->m_token->m_line_number, str_base_data_const(&ast_node->m_token->m_id));
                Id_info *id_info = pair.m_value;
                if (!vec_base_push_back(&self->type_info_stack, self->alloc, &id_info->type_info))
                    return OOM_ERROR;
                add_instruction("%s %s[-" USIZE_PFMT "]", OP_CODE_SYMBOLS[OP_CODE_PUSH], SP_SYMBOL, self->type_info_stack.m_size - id_info->stack_idx - 1);
                pop_on_discarded_expression(ast_node);
            }
            break;
        case TOKEN_TYPE_ARGV:
            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &(Type_info){.m_tag = TYPE_INFO_TAG_STR, .m_dimensions = 1}))
                return OOM_ERROR;
            add_instruction("%s %s", OP_CODE_SYMBOLS[OP_CODE_PUSH], str_base_data_const(&ast_node->m_token->m_id));
            pop_on_discarded_expression(ast_node);
            break;
        case TOKEN_TYPE_FALSE:
        case TOKEN_TYPE_TRUE:
            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &(Type_info){.m_tag = TYPE_INFO_TAG_BOOL, .m_dimensions = 0}))
                return OOM_ERROR;
            add_instruction("%s %s", OP_CODE_SYMBOLS[OP_CODE_PUSH], str_base_data_const(&ast_node->m_token->m_id));
            pop_on_discarded_expression(ast_node);
            break;
        case TOKEN_TYPE_CHAR_LIT:
        case TOKEN_TYPE_INT_LIT:
        case TOKEN_TYPE_FLOAT_LIT:
        case TOKEN_TYPE_STR_LIT:
            if (!vec_base_push_back(
                &self->type_info_stack,
                self->alloc,
                &(Type_info){.m_tag = (enum Type_info_tag)(TYPE_INFO_TAG_CHAR + ast_node->m_token->m_type - TOKEN_TYPE_CHAR_LIT), .m_dimensions = 0}
            ))
                return OOM_ERROR;
            add_instruction("%s %s", OP_CODE_SYMBOLS[OP_CODE_PUSH], str_base_data_const(&ast_node->m_token->m_id));
            pop_on_discarded_expression(ast_node);
            break;
        case TOKEN_TYPE_INIT_LIST:
            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &(Type_info){.m_tag = TYPE_INFO_TAG_VOID, .m_dimensions = 1}))
                return OOM_ERROR;
            add_instruction("%s []", OP_CODE_SYMBOLS[OP_CODE_PUSH]);
            if (ast_node->m_sub_nodes.m_size > 0){
                fprintf(stderr, "Initilizer list with elements is not implemented");
                abort();
            }
            pop_on_discarded_expression(ast_node);
            break;

        case TOKEN_TYPE_LPAREN:
            break;
        case TOKEN_TYPE_LBRACE:
            break;

        case TOKEN_TYPE_TILDE:
        case TOKEN_TYPE_NOT:
            {
                enum Unary_op unary_op = (ast_node->m_token->m_type == TOKEN_TYPE_TILDE) ? UNARY_OP_BNEG : UNARY_OP_NOT;

                Compiler_state_to_IR_result to_IR_result = compiler_state_to_IR(self, ast_node->m_sub_nodes.m_data[0]);
                if (to_IR_result.error != COMPILE_ERROR_NONE)
                    return to_IR_result;

                Type_info *last_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                Type_info type_info = unary_op_result_type_info(unary_op, *last_type_info_ptr);
                if (type_info.m_tag == TYPE_INFO_TAG_NONE){
                    Str_base_result type_info_str = type_info_to_str_base(*last_type_info_ptr, self->alloc);
                    if (!type_info_str.success)
                        return OOM_ERROR;
                    return syntax_error(
                        "Invalid unary operation <%s> on <%s>",
                        ast_node->m_token->m_line_number,
                        str_base_data_const(&ast_node->m_token->m_id),
                        str_base_data(&type_info_str.result)
                    );
                }

                *last_type_info_ptr = type_info;

                if (unary_op == UNARY_OP_BNEG)
                    add_instruction("%s", OP_CODE_SYMBOLS[OP_CODE_BNEG]);
                else{
                    add_instruction("%s", OP_CODE_SYMBOLS[OP_CODE_TO_BOOL]);
                    add_instruction("%s", OP_CODE_SYMBOLS[OP_CODE_NEG]);
                }

                pop_on_discarded_expression(ast_node);
            }
            break;

        case TOKEN_TYPE_PLUS:
        case TOKEN_TYPE_MINUS:
            if (ast_node->m_sub_nodes.m_size > 1){
                if (ast_node->m_token->m_type == TOKEN_TYPE_PLUS){
                    bin_op  = BINARY_OP_ADD;
                    op_code = OP_CODE_ADD;
                }
                else{
                    bin_op  = BINARY_OP_SUB;
                    op_code = OP_CODE_SUB;
                }
                goto bin_op_case;
            }
            else{
                enum Unary_op unary_op = (ast_node->m_token->m_type == TOKEN_TYPE_PLUS) ? UNARY_OP_PLUS : UNARY_OP_MINUS;

                Compiler_state_to_IR_result to_IR_result = compiler_state_to_IR(self, ast_node->m_sub_nodes.m_data[0]);
                if (to_IR_result.error != COMPILE_ERROR_NONE)
                    return to_IR_result;

                Type_info *last_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                Type_info type_info = unary_op_result_type_info(unary_op, *last_type_info_ptr);
                if (type_info.m_tag == TYPE_INFO_TAG_NONE){
                    Str_base_result type_info_str = type_info_to_str_base(*last_type_info_ptr, self->alloc);
                    if (!type_info_str.success)
                        return OOM_ERROR;
                    return syntax_error(
                        "Invalid unary operation <%s> on <%s>",
                        ast_node->m_token->m_line_number,
                        str_base_data_const(&ast_node->m_token->m_id),
                        str_base_data(&type_info_str.result)
                    );
                }

                *last_type_info_ptr = type_info;

                if (unary_op == UNARY_OP_MINUS)
                    add_instruction("%s", OP_CODE_SYMBOLS[OP_CODE_NEG]);

                pop_on_discarded_expression(ast_node);
            }
            break;

        case TOKEN_TYPE_EQUALS1:
            break;

        case TOKEN_TYPE_LBRACKET:             bin_op = BINARY_OP_SUBSCRIPT; op_code = OP_CODE_DEREF;   goto bin_op_case;
        case TOKEN_TYPE_EQUALS2:              bin_op = BINARY_OP_EQ;        op_code = OP_CODE_CMP_EQ;  goto bin_op_case;
        case TOKEN_TYPE_NOT_EQUALS:           bin_op = BINARY_OP_NEQ;       op_code = OP_CODE_CMP_NEQ; goto bin_op_case;
        case TOKEN_TYPE_LESS_THAN1:           bin_op = BINARY_OP_LE;        op_code = OP_CODE_CMP_LE;  goto bin_op_case;
        case TOKEN_TYPE_LESS_THAN1_EQUALS:    bin_op = BINARY_OP_LEQ;       op_code = OP_CODE_CMP_LEQ; goto bin_op_case;
        case TOKEN_TYPE_GREATER_THAN1:        bin_op = BINARY_OP_GE;        op_code = OP_CODE_CMP_GE;  goto bin_op_case;
        case TOKEN_TYPE_GREATER_THAN1_EQUALS: bin_op = BINARY_OP_GEQ;       op_code = OP_CODE_CMP_GEQ; goto bin_op_case;
        case TOKEN_TYPE_ASTERISK1:            bin_op = BINARY_OP_MUL;       op_code = OP_CODE_MUL;     goto bin_op_case;
        case TOKEN_TYPE_SLASH:                bin_op = BINARY_OP_DIV;       op_code = OP_CODE_DIV;     goto bin_op_case;
        case TOKEN_TYPE_PERCENT:              bin_op = BINARY_OP_MOD;       op_code = OP_CODE_MOD;     goto bin_op_case;
        case TOKEN_TYPE_ASTERISK2:            bin_op = BINARY_OP_POW;       op_code = OP_CODE_POW;     goto bin_op_case;
        case TOKEN_TYPE_LESS_THAN2:           bin_op = BINARY_OP_SHL;       op_code = OP_CODE_SHL;     goto bin_op_case;
        case TOKEN_TYPE_GREATER_THAN2:        bin_op = BINARY_OP_SHR;       op_code = OP_CODE_SHR;     goto bin_op_case;
        case TOKEN_TYPE_AMPERSAND:            bin_op = BINARY_OP_BAND;      op_code = OP_CODE_BAND;    goto bin_op_case;
        case TOKEN_TYPE_PIPE:                 bin_op = BINARY_OP_BOR;       op_code = OP_CODE_BOR;     goto bin_op_case;
        case TOKEN_TYPE_CARET:                bin_op = BINARY_OP_XOR;       op_code = OP_CODE_XOR;
        bin_op_case:
            {
                const AST_node *lhs_node = ast_node->m_sub_nodes.m_data[0];
                const AST_node *rhs_node = ast_node->m_sub_nodes.m_data[1];

                Compiler_state_to_IR_result to_IR_result;
                if (
                    (to_IR_result = compiler_state_to_IR(self, lhs_node)).error != COMPILE_ERROR_NONE ||
                    (to_IR_result = compiler_state_to_IR(self, rhs_node)).error != COMPILE_ERROR_NONE
                )
                    return to_IR_result;

                Type_info *lhs_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 2);
                Type_info *rhs_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                Type_info bin_op_result = binary_op_result_type_info(bin_op, *lhs_type_info_ptr, *rhs_type_info_ptr);
                if (bin_op_result.m_tag == TYPE_INFO_TAG_NONE){
                    Str_base_result lhs_type_info_str;
                    Str_base_result rhs_type_info_str;
                    if (
                        !(lhs_type_info_str = type_info_to_str_base(*lhs_type_info_ptr, self->alloc)).success ||
                        !(rhs_type_info_str = type_info_to_str_base(*rhs_type_info_ptr, self->alloc)).success
                    )
                        return OOM_ERROR;
                    return syntax_error(
                        "Invalid binary operation <%s> between <%s> and <%s>",
                        ast_node->m_token->m_line_number,
                        str_base_data_const(&ast_node->m_token->m_id),
                        str_base_data_const(&lhs_type_info_str.result),
                        str_base_data_const(&rhs_type_info_str.result)
                    );
                }

                *lhs_type_info_ptr = bin_op_result;
                vec_base_pop_back_discard(&self->type_info_stack);

                add_instruction("%s", OP_CODE_SYMBOLS[op_code]);
                pop_on_discarded_expression(ast_node);
            }
            break;

        case TOKEN_TYPE_AND:
        case TOKEN_TYPE_OR:
            break;
        
        case TOKEN_TYPE_FN:
            fprintf(stderr, "Not implemented\n");
            abort();
        case TOKEN_TYPE_LET:
            {
                const AST_node *id_node   = ast_node->m_sub_nodes.m_data[0];
                const AST_node *type_node = ast_node->m_sub_nodes.m_data[1];
                const AST_node *expr_node = ast_node->m_sub_nodes.m_data[2];

                Type_info let_decl_type_info = {0};

                while (type_node->m_token->m_type == TOKEN_TYPE_LBRACKET){
                    type_node = type_node->m_sub_nodes.m_data[0];
                    ++let_decl_type_info.m_dimensions;
                }

                let_decl_type_info.m_tag = (enum Type_info_tag)(TYPE_INFO_TAG_BOOL + type_node->m_token->m_type - TOKEN_TYPE_BOOL);

                Umap_insert_result ires = umap_base_insert(
                    &self->id_info_map,
                    self->alloc,
                    &id_node->m_token->m_id,
                    &(Id_info){.stack_idx = self->id_stack.m_size, .type_info = let_decl_type_info}
                );

                switch (ires.error){
                    case UMAP_INSERT_ERROR_NONE:
                        break;
                    case UMAP_INSERT_ERROR_OOM:
                        return OOM_ERROR;
                    case UMAP_INSERT_ERROR_ALREADY_INSERTED:
                        return syntax_error("Identifier <%s> is already in use", id_node->m_token->m_line_number, str_base_data_const(&id_node->m_token->m_id));
                }

                Compiler_state_to_IR_result to_IR_result = compiler_state_to_IR(self, expr_node);
                if (to_IR_result.error != COMPILE_ERROR_NONE)
                    return to_IR_result;
                
                if (!vec_base_push_back(&self->id_stack, self->alloc, &id_node->m_token->m_id))
                    return OOM_ERROR;

                Type_info *last_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1), last_type_info = *last_type_info_ptr;
                if (binary_op_result_type_info(BINARY_OP_ASSIGNMENT, let_decl_type_info, last_type_info).m_tag == TYPE_INFO_TAG_NONE){
                    Str_base_result type_str;
                    Str_base_result expr_type_str;
                    if (
                        !(type_str = type_info_to_str_base(let_decl_type_info, self->alloc)).success ||
                        !(expr_type_str = type_info_to_str_base(last_type_info, self->alloc)).success
                    )
                        return OOM_ERROR;
                    return syntax_error(
                        "Expression's type <%s> is incompatible with the type of the destination <%s>",
                        expr_node->m_token->m_line_number,
                        str_base_data(&expr_type_str.result),
                        str_base_data(&type_str.result)
                    );
                }

                *last_type_info_ptr = let_decl_type_info;
                ++*(usize*)vec_base_at(&self->let_decl_counts, self->let_decl_counts.m_size - 1);

                if (last_type_info.m_dimensions == 0)
                    add_instruction("%s", OP_CODE_SYMBOLS[OP_CODE_TO_BOOL + let_decl_type_info.m_tag - TYPE_INFO_TAG_BOOL]);
            }
            break;

        case TOKEN_TYPE_IF:
        case TOKEN_TYPE_ELSE:

        case TOKEN_TYPE_WHILE:

        case TOKEN_TYPE_RETURN:
            break;

        default:
            fprintf(stderr, "Not implemented\n");
            abort();
    }

    return (Compiler_state_to_IR_result){.error = COMPILE_ERROR_NONE};
}

// ------------------------------------------------------------------------------------------------

Compile_to_IR_result compile_to_IR(Arena *arena, AST_node_ptr_slice ast_nodes){
    assert(arena && "<arena> is not nullable");

    Compiler_state state = {
        .alloc           = arena_allocator(arena),
        .id_stack        = vec_base_init(Str_base),
        .id_info_map     = umap_base_init(Str_base, Id_info),
        .let_decl_counts = vec_base_init(usize),
        .type_info_stack = vec_base_init(Type_info),
        .IR              = {0}
    };

    if (!vec_base_push_back(&state.let_decl_counts, state.alloc, &(usize){0}))
        return (Compile_to_IR_result){.error = COMPILE_ERROR_OOM};

    for (usize i = 0; i < ast_nodes.m_size; ++i){
        Compiler_state_to_IR_result to_IR_result = compiler_state_to_IR(&state, ast_nodes.m_data[i]);
        if (to_IR_result.error != COMPILE_ERROR_NONE)
            return (Compile_to_IR_result){.error_info = to_IR_result.error_info, .error = to_IR_result.error};
    }

    return (Compile_to_IR_result){.IR = state.IR, .error = COMPILE_ERROR_NONE};
}
