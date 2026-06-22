#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../hdrs/Allocator/Arena.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Umap_base.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Cmp.h"
#include "../../hdrs/Utils/Hash.h"
#include "../../hdrs/Utils/Num.h"

#include "../../hdrs/Lang/Compiler.h"
#include "../../hdrs/Lang/Parser.h"
#include "../../hdrs/Lang/Type_info.h"

// ------------------------------------------------------------------------------------------------

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

#define INDENT "    "

static const char SP_SYMBOL[] = "sp";

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

static bool compiler_state_pop_on_discarded_expression(Compiler_state *self, const AST_node *ast_node){
    if (!ast_node->m_parent)
        goto pop;
    else{
        switch (ast_node->m_parent->m_token->m_type){
            case TOKEN_TYPE_LBRACE:
            case TOKEN_TYPE_IF:
            case TOKEN_TYPE_ELSE:
            case TOKEN_TYPE_WHILE:
                goto pop;
            default:
                break;
        }
    }

    return true;

pop:
    vec_base_pop_back_discard(&self->type_info_stack);
    return str_base_append_fmt(&self->IR, self->alloc, INDENT "%s\n", op_code_to_str(OP_CODE_POP));
}

static Compiler_state_to_IR_result compiler_state_to_IR(Compiler_state *self, const AST_node *ast_node){
    #define add_instruction(...) \
        do{ \
            if (!str_base_append_fmt(&self->IR, self->alloc, INDENT __VA_ARGS__) || !str_base_push_back(&self->IR, self->alloc, '\n')) \
                return OOM_ERROR; \
        } while (0)

    switch (ast_node->m_token->m_type){
        case TOKEN_TYPE_ID:
            break;
        case TOKEN_TYPE_ARGV:
            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &(Type_info){.m_tag = TYPE_INFO_TAG_STR, .m_dimensions = 1}))
                return OOM_ERROR;
            add_instruction("%s %s", op_code_to_str(OP_CODE_PUSH), str_base_data_const(&ast_node->m_token->m_id));
            if (!compiler_state_pop_on_discarded_expression(self, ast_node))
                return OOM_ERROR;
            break;
        case TOKEN_TYPE_FALSE:
        case TOKEN_TYPE_TRUE:
            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &(Type_info){.m_tag = TYPE_INFO_TAG_BOOL, .m_dimensions = 0}))
                return OOM_ERROR;
            add_instruction("%s %s", op_code_to_str(OP_CODE_PUSH), str_base_data_const(&ast_node->m_token->m_id));
            if (!compiler_state_pop_on_discarded_expression(self, ast_node))
                return OOM_ERROR;
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
            add_instruction("%s %s", op_code_to_str(OP_CODE_PUSH), str_base_data_const(&ast_node->m_token->m_id));
            if (!compiler_state_pop_on_discarded_expression(self, ast_node))
                return OOM_ERROR;
            break;
        case TOKEN_TYPE_INIT_LIST:
            add_instruction("%s []", op_code_to_str(OP_CODE_PUSH));
            if (ast_node->m_sub_nodes.m_size > 0){
                fprintf(stderr, "Initilizer list with elements is not implemented");
                abort();
            }
            else if (!vec_base_push_back(&self->type_info_stack, self->alloc, &(Type_info){.m_tag = TYPE_INFO_TAG_VOID, .m_dimensions = 1}))
                return OOM_ERROR;
            if (!compiler_state_pop_on_discarded_expression(self, ast_node))
                return OOM_ERROR;
            break;

        case TOKEN_TYPE_COMMA:
        case TOKEN_TYPE_COLON:
        case TOKEN_TYPE_SEMICOLON:
        case TOKEN_TYPE_LPAREN:
        case TOKEN_TYPE_RPAREN:
        case TOKEN_TYPE_LBRACKET:
        case TOKEN_TYPE_RBRACKET:
        case TOKEN_TYPE_LBRACE:
        case TOKEN_TYPE_RBRACE:

        case TOKEN_TYPE_DOT2:
        case TOKEN_TYPE_EQUALS2:
        case TOKEN_TYPE_NOT_EQUALS:
        case TOKEN_TYPE_LESS_THAN1:
        case TOKEN_TYPE_LESS_THAN1_EQUALS:
        case TOKEN_TYPE_GREATER_THAN1:
        case TOKEN_TYPE_GREATER_THAN1_EQUALS:
        case TOKEN_TYPE_PLUS:
        case TOKEN_TYPE_MINUS:
        case TOKEN_TYPE_ASTERISK1:
        case TOKEN_TYPE_ASTERISK2:
        case TOKEN_TYPE_SLASH:
        case TOKEN_TYPE_PERCENT:
        case TOKEN_TYPE_LESS_THAN2:
        case TOKEN_TYPE_GREATER_THAN2:
        case TOKEN_TYPE_AMPERSAND:
        case TOKEN_TYPE_PIPE:
        case TOKEN_TYPE_CARET:
        case TOKEN_TYPE_TILDE:
        case TOKEN_TYPE_AND:
        case TOKEN_TYPE_OR:
        case TOKEN_TYPE_NOT:
        case TOKEN_TYPE_EQUALS1:
        
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
                if (binary_op_result_type_info(BINARY_OP_ASSIGNMENT, let_decl_type_info, last_type_info).m_tag == TYPE_INFO_TAG_NONE)
                    return syntax_error("Expression's type is incompatible with the type of the destination", expr_node->m_token->m_line_number);

                *last_type_info_ptr = let_decl_type_info;
                ++*(usize*)vec_base_at(&self->let_decl_counts, self->let_decl_counts.m_size - 1);

                if (last_type_info.m_tag >= TYPE_INFO_TAG_BOOL && last_type_info.m_tag <= TYPE_INFO_TAG_STR)
                    add_instruction("%s", op_code_to_str((enum Op_code)(OP_CODE_TO_BOOL + let_decl_type_info.m_tag - TYPE_INFO_TAG_BOOL)));
            }
            break;

        case TOKEN_TYPE_IF:
        case TOKEN_TYPE_ELSE:

        case TOKEN_TYPE_WHILE:
            break;

        case TOKEN_TYPE_RETURN:
            break;

        default:
            fprintf(stderr, "Not implemented\n");
            abort();
    }

    return (Compiler_state_to_IR_result){.error = COMPILE_ERROR_NONE};
}

// ------------------------------------------------------------------------------------------------

const char* op_code_to_str(enum Op_code op_code){
    switch (op_code){
        case OP_CODE_PUSH: return "push";
        case OP_CODE_POP:  return "pop";

        case OP_CODE_CALL: return "call";

        case OP_CODE_RET: return "ret";

        case OP_CODE_JMP:  return "jmp";
        case OP_CODE_JMPZ: return "jmpz";

        case OP_CODE_TO_BOOL:  return "to_bool";
        case OP_CODE_TO_CHAR:  return "to_char";
        case OP_CODE_TO_INT:   return "to_int";
        case OP_CODE_TO_FLOAT: return "to_float";
        case OP_CODE_TO_STR:   return "to_str";

        case OP_CODE_NEG:  return "neg";
        case OP_CODE_BNEG: return "bneg";

        case OP_CODE_DEREF: return "deref";

        case OP_CODE_MOV:       return "mov";
        case OP_CODE_MOV_DEREF: return "mov_deref";

        case OP_CODE_CMP_EQ:  return "cmp_eq";
        case OP_CODE_CMP_NEQ: return "cmp_neq";
        case OP_CODE_CMP_LE:  return "cmp_le";
        case OP_CODE_CMP_LEQ: return "cmp_leq";
        case OP_CODE_CMP_GE:  return "cmp_ge";
        case OP_CODE_CMP_GEQ: return "cmp_geq";

        case OP_CODE_ADD: return "add";
        case OP_CODE_SUB: return "sub";
        case OP_CODE_MUL: return "mul";
        case OP_CODE_DIV: return "div";
        case OP_CODE_MOD: return "mod";
        case OP_CODE_POW: return "pow";

        case OP_CODE_SHL:  return "shl";
        case OP_CODE_SHR:  return "shr";
        case OP_CODE_BAND: return "band";
        case OP_CODE_BOR:  return "bor";
        case OP_CODE_XOR:  return "xor";

        default: return NULL;
    }
}

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
