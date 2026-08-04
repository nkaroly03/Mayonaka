#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../hdrs/Allocator/Arena.h"
#include "../../hdrs/Data_structure/Ordered_umap_base.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Umap_base.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

#include "../../hdrs/Lang/Builtin_fn.h"
#include "../../hdrs/Lang/IR_compiler.h"
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

static Type_info ast_node_to_type_info(const AST_node *type_node){
    Type_info result = {0};
    while (type_node->m_token->m_type == TOKEN_TYPE_LBRACKET){
        type_node = type_node->m_sub_nodes.m_data[0];
        ++result.m_dimensions;
    }
    result.m_tag = (enum Type_info_tag)(TYPE_INFO_TAG_VOID + (type_node->m_token->m_type - TOKEN_TYPE_VOID));
    return result;
}

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

typedef struct Id_count{
    usize fn_id_count;
    usize var_id_count;
} Id_count;

typedef struct Fn_id_info{
    Str_base id_mangled;
    Type_info_slice arg_type_infos;
    Type_info return_type_info;
} Fn_id_info;

typedef struct Var_id_info{
    usize stack_idx;
    Type_info type_info;
    bool is_global;
} Var_id_info;

typedef struct While_label_info{
    const char *break_label_str, *continue_label_str;
    usize id_count_stack_idx;
} While_label_info;

typedef struct IR_compiler_state{
    Allocator alloc;
    Vec_base id_count_stack;
    Ordered_umap_base *fn_ids_ptr;
    Ordered_umap_base var_ids;
    Vec_base type_info_stack;
    usize label_counter;
    Vec_base while_label_info_stack;
    Str_base IR;
    Str_base fn_IRs;
} IR_compiler_state;

static bool IR_compiler_state_init_in_place(IR_compiler_state *self, Allocator arena_alloc, usize label_counter_start, Ordered_umap_base *fn_ids_ptr){
    *self = (IR_compiler_state){
        .alloc                  = arena_alloc,
        .id_count_stack         = vec_base_init(Id_count),
        .fn_ids_ptr             = fn_ids_ptr,
        .var_ids                = ordered_umap_base_init(Str_base, Var_id_info),
        .type_info_stack        = vec_base_init(Type_info),
        .label_counter          = label_counter_start,
        .while_label_info_stack = vec_base_init(While_label_info),
        .IR                     = {0},
        .fn_IRs                 = {0}
    };
    return vec_base_push_back(&self->id_count_stack, self->alloc, &(Id_count){0}) != NULL;
}

typedef struct IR_compiler_state_compile_result{
    Str_base error_info;
    enum Compile_error error;
} IR_compiler_state_compile_result;

static const IR_compiler_state_compile_result OOM_ERROR = {.error = COMPILE_ERROR_OOM};

static IR_compiler_state_compile_result IR_compiler_state_syntax_error(IR_compiler_state *self, const char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    Str_base_result error_info = str_base_init_fmt_va_list(self->alloc, fmt, args);
    va_end(args);

    return (error_info.success) ? (IR_compiler_state_compile_result){.error_info = error_info.result, .error = COMPILE_ERROR_SYNTAX} : OOM_ERROR;
}
#define syntax_error(...) IR_compiler_state_syntax_error(self, "On line <" USIZE_PFMT ">: " __VA_ARGS__)

static IR_compiler_state_compile_result IR_compiler_state_unary_op_error(IR_compiler_state *self, const AST_node *un_op_node, Type_info type_info){
    Str_base_result type_info_str = type_info_to_str_base(type_info, self->alloc);
    if (!type_info_str.success)
        return OOM_ERROR;
    return syntax_error(
        "Invalid unary operation <%s> on <%s>",
        un_op_node->m_token->m_line_number,
        str_base_data_const(&un_op_node->m_token->m_id),
        str_base_data(&type_info_str.result)
    );
}
static IR_compiler_state_compile_result IR_compiler_state_binary_op_error(IR_compiler_state *self, const AST_node *bin_op_node, Type_info lhs_type_info, Type_info rhs_type_info){
    Str_base_result lhs_type_info_str;
    Str_base_result rhs_type_info_str;
    if (
        !(lhs_type_info_str = type_info_to_str_base(lhs_type_info, self->alloc)).success ||
        !(rhs_type_info_str = type_info_to_str_base(rhs_type_info, self->alloc)).success
    )
        return OOM_ERROR;
    return syntax_error(
        "Invalid binary operation <%s> between <%s> and <%s>",
        bin_op_node->m_token->m_line_number,
        str_base_data_const(&bin_op_node->m_token->m_id),
        str_base_data(&lhs_type_info_str.result),
        str_base_data(&rhs_type_info_str.result)
    );
}
static IR_compiler_state_compile_result IR_compiler_state_type_conversion_error(
    IR_compiler_state *self,
    const AST_node *ast_node,
    Type_info dest_type_info,
    Type_info src_type_info
){
    Str_base_result dest_type_info_str;
    Str_base_result src_type_info_str;
    if (
        !(dest_type_info_str = type_info_to_str_base(dest_type_info, self->alloc)).success ||
        !( src_type_info_str = type_info_to_str_base( src_type_info, self->alloc)).success
    )
        return OOM_ERROR;
    return syntax_error(
        "Expression with type <%s> is not convertible to <%s>",
        ast_node->m_token->m_line_number,
        str_base_data(&src_type_info_str.result),
        str_base_data(&dest_type_info_str.result)
    );
}

#define INDENT "    "
static bool IR_compiler_state_add_instruction(IR_compiler_state *self, const char *fmt, ...){
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
        if (!IR_compiler_state_add_instruction(self, __VA_ARGS__)) \
            return OOM_ERROR; \
    } while (0)

static bool IR_compiler_state_add_type_conversion_instruction(IR_compiler_state *self, Type_info dest_type_info){
    return (dest_type_info.m_dimensions == 0)
        ? IR_compiler_state_add_instruction(self, "%s", op_code_to_str((enum Op_code)(OP_CODE_TO_BOOL + (dest_type_info.m_tag - TYPE_INFO_TAG_BOOL))))
        : true
    ;
}
#define add_type_conversion_instruction(dest_type_info) \
    do{ \
        if (!IR_compiler_state_add_type_conversion_instruction(self, (dest_type_info))) \
            return OOM_ERROR; \
    } while (0)

static bool IR_compiler_state_pop_on_discarded_expression(IR_compiler_state *self, const AST_node *ast_node){
    if (ast_node->m_parent){
        const AST_node *parent = ast_node->m_parent;
        switch (parent->m_token->m_type){
            case TOKEN_TYPE_COLON:
            case TOKEN_TYPE_LBRACE:
            case TOKEN_TYPE_ELSE:
                break;
            case TOKEN_TYPE_IF:
            case TOKEN_TYPE_WHILE:
                if (parent->m_sub_nodes.m_data[0] != ast_node)
                    break;
                FALLTHROUGH;
            default:
                return true;
        }
    }

    vec_base_pop_back_discard(&self->type_info_stack);

    return IR_compiler_state_add_instruction(self, "%s", op_code_to_str(OP_CODE_POP));
}
#define pop_on_discarded_expression(ast_node) \
    do{ \
        if (!IR_compiler_state_pop_on_discarded_expression(self, (ast_node))) \
            return OOM_ERROR; \
    } while (0)

static IR_compiler_state_compile_result IR_compiler_state_push_back_var_id(IR_compiler_state *self, const AST_node *id_node, Type_info id_type_info){
    assert(self->var_ids.m_keys.m_size == self->type_info_stack.m_size - 1);

    enum Umap_insert_error insert_error = ordered_umap_base_push_back(
        &self->var_ids,
        self->alloc,
        &id_node->m_token->m_id,
        &(Var_id_info){
            .stack_idx = self->var_ids.m_keys.m_size,
            .type_info = id_type_info,
            .is_global = (id_node->m_parent->m_token->m_type != TOKEN_TYPE_FN && id_node->m_parent->m_parent == NULL)
        }
    ).error;

    switch (insert_error){
        case UMAP_INSERT_ERROR_NONE:
            break;
        case UMAP_INSERT_ERROR_OOM:
            return OOM_ERROR;
        case UMAP_INSERT_ERROR_ALREADY_INSERTED:
            return syntax_error("Identifier <%s> is already in use", id_node->m_token->m_line_number, str_base_data_const(&id_node->m_token->m_id));
    }

    ++((Id_count*)vec_base_at(&self->id_count_stack, self->id_count_stack.m_size - 1))->var_id_count;

    return (IR_compiler_state_compile_result){0};
}
static bool IR_compiler_state_pop_ids_in_current_scope(IR_compiler_state *self){
    Id_count id_count;
    vec_base_pop_back_to(&self->id_count_stack, &id_count);
    while (id_count.fn_id_count-- > 0)
        ordered_umap_base_pop_back_discard(self->fn_ids_ptr, self->alloc);
    while (id_count.var_id_count-- > 0){
        ordered_umap_base_pop_back_discard(&self->var_ids, self->alloc);
        vec_base_pop_back_discard(&self->type_info_stack);
        if (!IR_compiler_state_add_instruction(self, "%s", op_code_to_str(OP_CODE_POP)))
            return false;
    }
    return true;
}
#define pop_ids_in_current_scope() \
    do{ \
        if (!IR_compiler_state_pop_ids_in_current_scope(self)) \
            return OOM_ERROR; \
    } while (0)

#define JMP_LABEL_SYMBOL "L"
#define JMP_LABEL_FMT LOCAL_LABEL_PREFIX_SYMBOL JMP_LABEL_SYMBOL USIZE_PFMT
#define JMP_LABEL_BUFSIZE array_size(LOCAL_LABEL_PREFIX_SYMBOL JMP_LABEL_SYMBOL "18446744073709551615")

static IR_compiler_state_compile_result IR_compiler_state_compile(IR_compiler_state *self, const AST_node *ast_node){
    enum Binary_op bin_op;
    enum Op_code bin_op_code;

    switch (ast_node->m_token->m_type){
        case TOKEN_TYPE_ID:{
            Var_id_info *var_id_info_ptr = ordered_umap_base_at_key(&self->var_ids, &ast_node->m_token->m_id).m_value;
            if (!var_id_info_ptr)
                return syntax_error("Use of undeclared identifier <%s>", ast_node->m_token->m_line_number, str_base_data_const(&ast_node->m_token->m_id));
            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &var_id_info_ptr->type_info))
                return OOM_ERROR;
            if (var_id_info_ptr->is_global)
                add_instruction("%s " BP_SYMBOL "[" USIZE_PFMT "]", op_code_to_str(OP_CODE_PUSH), var_id_info_ptr->stack_idx);
            else
                add_instruction("%s " SP_SYMBOL "[-" USIZE_PFMT "]", op_code_to_str(OP_CODE_PUSH), self->type_info_stack.m_size - var_id_info_ptr->stack_idx - 1);
            pop_on_discarded_expression(ast_node);
            break;
        }
        case TOKEN_TYPE_ARGV:
            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &(Type_info){.m_tag = TYPE_INFO_TAG_STR, .m_dimensions = 1}))
                return OOM_ERROR;
            add_instruction("%s %s", op_code_to_str(OP_CODE_PUSH), str_base_data_const(&ast_node->m_token->m_id));
            pop_on_discarded_expression(ast_node);
            break;
        case TOKEN_TYPE_FALSE:
        case TOKEN_TYPE_TRUE:
            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &(Type_info){.m_tag = TYPE_INFO_TAG_BOOL, .m_dimensions = 0}))
                return OOM_ERROR;
            add_instruction("%s %s", op_code_to_str(OP_CODE_PUSH), str_base_data_const(&ast_node->m_token->m_id));
            pop_on_discarded_expression(ast_node);
            break;
        case TOKEN_TYPE_CHAR_LIT:
        case TOKEN_TYPE_INT_LIT:
        case TOKEN_TYPE_FLOAT_LIT:
        case TOKEN_TYPE_STR_LIT:
            if (!vec_base_push_back(
                &self->type_info_stack,
                self->alloc,
                &(Type_info){.m_tag = (enum Type_info_tag)(TYPE_INFO_TAG_CHAR + (ast_node->m_token->m_type - TOKEN_TYPE_CHAR_LIT)), .m_dimensions = 0}
            ))
                return OOM_ERROR;
            add_instruction("%s %s", op_code_to_str(OP_CODE_PUSH), str_base_data_const(&ast_node->m_token->m_id));
            pop_on_discarded_expression(ast_node);
            break;
        case TOKEN_TYPE_INIT_LIST:
            if (!ast_node->m_parent || ast_node->m_parent->m_token->m_type != TOKEN_TYPE_LET)
                return syntax_error("Initilizer list is only allowed in <let> statement", ast_node->m_token->m_line_number);
            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &(Type_info){.m_tag = TYPE_INFO_TAG_VOID, .m_dimensions = 1}))
                return OOM_ERROR;
            add_instruction("%s []", op_code_to_str(OP_CODE_PUSH));
            if (ast_node->m_sub_nodes.m_size > 0){
                fprintf(stderr, "Initilizer list with elements is not implemented");
                abort();
            }
            // pop_on_discarded_expression(ast_node);
            break;

        case TOKEN_TYPE_LPAREN:{
            const char *fn_id = str_base_data_const(&ast_node->m_sub_nodes.m_data[0]->m_token->m_id);
            AST_node_ptr_slice fn_arg_nodes = {.m_size = ast_node->m_sub_nodes.m_size - 1, .m_data = &ast_node->m_sub_nodes.m_data[1]};

            const char *fn_id_mangled = fn_id;
            Type_info return_type_info;

            enum Builtin_fn_tag bfn_tag = builtin_fn_tag_init(fn_id);
            if (bfn_tag != BUILTIN_FN_TAG_NONE){
                Builtin_fn_tag_call_result bfn_call_result;

                if (fn_arg_nodes.m_size > 0){
                    for (usize i = 0; i < fn_arg_nodes.m_size; ++i){
                        IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, fn_arg_nodes.m_data[i]);
                        if (compile_result.error != COMPILE_ERROR_NONE)
                            return compile_result;

                        Type_info last_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);
                        add_type_conversion_instruction(last_type_info);
                    }

                    Type_info_slice arg_type_infos = {
                        .m_size = fn_arg_nodes.m_size,
                        .m_data = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - fn_arg_nodes.m_size)
                    };
                    bfn_call_result = builtin_fn_tag_call(bfn_tag, arg_type_infos);
                    if (!bfn_call_result.m_is_callable){
                        Str_base type_info_list_str = {0};
                        for (usize i = 0; i < arg_type_infos.m_size; ++i){
                            Str_base_result type_info_str = type_info_to_str_base(arg_type_infos.m_data[i], self->alloc);
                            if (!type_info_str.success || !str_base_append_fmt(&type_info_list_str, self->alloc, "%s, ", str_base_data(&type_info_str.result)))
                                return OOM_ERROR;
                        }
                        str_base_pop_back(&type_info_list_str);
                        str_base_pop_back(&type_info_list_str);

                        return syntax_error(
                            "Builtin function <%s> is not callable with types <%s>",
                            ast_node->m_token->m_line_number,
                            fn_id,
                            str_base_data(&type_info_list_str)
                        );
                    }
                }
                else if (!(bfn_call_result = builtin_fn_tag_call(bfn_tag, (Type_info_slice){0})).m_is_callable)
                    return syntax_error("Builtin function <%s> is not callable without arguments", ast_node->m_token->m_line_number, fn_id);

                return_type_info = bfn_call_result.m_return_type_info;
            }
            else{
                Fn_id_info *fn_id_info_ptr = ordered_umap_base_at_key(self->fn_ids_ptr, &ast_node->m_sub_nodes.m_data[0]->m_token->m_id).m_value;
                if (!fn_id_info_ptr)
                    return syntax_error("Use of undeclared function <%s>", ast_node->m_token->m_line_number, fn_id);

                fn_id_mangled = str_base_data_const(&fn_id_info_ptr->id_mangled);

                for (usize i = 0; i < fn_arg_nodes.m_size; ++i){
                    if (i >= fn_id_info_ptr->arg_type_infos.m_size)
                        return syntax_error("Function <%s> called with wrong number of arguments", ast_node->m_token->m_line_number, fn_id);

                    IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, fn_arg_nodes.m_data[i]);
                    if (compile_result.error != COMPILE_ERROR_NONE)
                        return compile_result;

                    Type_info last_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                    if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, fn_id_info_ptr->arg_type_infos.m_data[i], last_type_info).m_tag == TYPE_INFO_TAG_NONE)
                        return IR_compiler_state_type_conversion_error(self, fn_arg_nodes.m_data[i], fn_id_info_ptr->arg_type_infos.m_data[i], last_type_info);

                    add_type_conversion_instruction(fn_id_info_ptr->arg_type_infos.m_data[i]);
                }

                return_type_info = fn_id_info_ptr->return_type_info;
            }

            for (usize i = 0; i < fn_arg_nodes.m_size; ++i)
                vec_base_pop_back_discard(&self->type_info_stack);

            if (return_type_info.m_tag != TYPE_INFO_TAG_VOID){
                if (!vec_base_push_back(&self->type_info_stack, self->alloc, &return_type_info))
                    return OOM_ERROR;
            }
            else if (ast_node->m_parent){
                const AST_node *parent = ast_node->m_parent;
                switch (parent->m_token->m_type){
                    case TOKEN_TYPE_COLON:
                    case TOKEN_TYPE_LBRACE:
                    case TOKEN_TYPE_ELSE:
                        break;
                    case TOKEN_TYPE_IF:
                    case TOKEN_TYPE_WHILE:
                        if (parent->m_sub_nodes.m_data[0] != ast_node)
                            break;
                        FALLTHROUGH;
                    default:
                        return syntax_error("Function <%s> returning type <void> is used in an expression", ast_node->m_token->m_line_number, fn_id);
                }
            }

            add_instruction("%s %s", op_code_to_str(OP_CODE_CALL), fn_id_mangled);

            if (return_type_info.m_tag != TYPE_INFO_TAG_VOID)
                pop_on_discarded_expression(ast_node);
            break;
        }

        case TOKEN_TYPE_LBRACE:
            if (!vec_base_push_back(&self->id_count_stack, self->alloc, &(Id_count){0}))
                return OOM_ERROR;
            for (usize i = 0; i < ast_node->m_sub_nodes.m_size; ++i){
                IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[i]);
                if (compile_result.error != COMPILE_ERROR_NONE)
                    return compile_result;
            }
            pop_ids_in_current_scope();
            break;

        case TOKEN_TYPE_TILDE:
        case TOKEN_TYPE_NOT:{
            enum Unary_op un_op = (ast_node->m_token->m_type == TOKEN_TYPE_TILDE) ? UNARY_OP_BNEG : UNARY_OP_NOT;

            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[0]);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info *last_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

            Type_info un_op_result = unary_op_type_info_result(un_op, *last_type_info_ptr);
            if (un_op_result.m_tag == TYPE_INFO_TAG_NONE)
                return IR_compiler_state_unary_op_error(self, ast_node, *last_type_info_ptr);

            *last_type_info_ptr = un_op_result;

            if (un_op == UNARY_OP_BNEG)
                add_instruction("%s", op_code_to_str(OP_CODE_BNEG));
            else{
                add_instruction("%s", op_code_to_str(OP_CODE_TO_BOOL));
                add_instruction("%s", op_code_to_str(OP_CODE_NEG));
            }

            pop_on_discarded_expression(ast_node);
            break;
        }

        case TOKEN_TYPE_PLUS:
        case TOKEN_TYPE_MINUS:
            if (ast_node->m_sub_nodes.m_size > 1){
                if (ast_node->m_token->m_type == TOKEN_TYPE_PLUS){
                    bin_op      = BINARY_OP_ADD;
                    bin_op_code = OP_CODE_ADD;
                }
                else{
                    bin_op      = BINARY_OP_SUB;
                    bin_op_code = OP_CODE_SUB;
                }
                goto bin_op_case;
            }
            else{
                enum Unary_op un_op = (ast_node->m_token->m_type == TOKEN_TYPE_PLUS) ? UNARY_OP_PLUS : UNARY_OP_MINUS;

                IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[0]);
                if (compile_result.error != COMPILE_ERROR_NONE)
                    return compile_result;

                Type_info *last_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                Type_info un_op_result = unary_op_type_info_result(un_op, *last_type_info_ptr);
                if (un_op_result.m_tag == TYPE_INFO_TAG_NONE)
                    return IR_compiler_state_unary_op_error(self, ast_node, *last_type_info_ptr);

                *last_type_info_ptr = un_op_result;

                if (un_op == UNARY_OP_MINUS)
                    add_instruction("%s", op_code_to_str(OP_CODE_NEG));

                pop_on_discarded_expression(ast_node);
            }
            break;

        case TOKEN_TYPE_EQUALS1:{
            const AST_node *lhs_node = ast_node->m_sub_nodes.m_data[0];
            const AST_node *rhs_node = ast_node->m_sub_nodes.m_data[1];

            bool push_back_after_assignment = false;
            if (ast_node->m_parent){
                switch (ast_node->m_parent->m_token->m_type){
                    case TOKEN_TYPE_IF:
                    case TOKEN_TYPE_WHILE:
                        if (ast_node->m_parent->m_sub_nodes.m_data[0] != ast_node)
                            break;
                        FALLTHROUGH;
                    case TOKEN_TYPE_LPAREN:
                    case TOKEN_TYPE_TILDE:
                    case TOKEN_TYPE_NOT:
                    case TOKEN_TYPE_PLUS:
                    case TOKEN_TYPE_MINUS:
                    case TOKEN_TYPE_EQUALS1:
                    case TOKEN_TYPE_LBRACKET:
                    case TOKEN_TYPE_EQUALS2:
                    case TOKEN_TYPE_NOT_EQUALS:
                    case TOKEN_TYPE_LESS_THAN1:
                    case TOKEN_TYPE_LESS_THAN1_EQUALS:
                    case TOKEN_TYPE_GREATER_THAN1:
                    case TOKEN_TYPE_GREATER_THAN1_EQUALS:
                    case TOKEN_TYPE_ASTERISK1:
                    case TOKEN_TYPE_SLASH:
                    case TOKEN_TYPE_PERCENT:
                    case TOKEN_TYPE_ASTERISK2:
                    case TOKEN_TYPE_LESS_THAN2:
                    case TOKEN_TYPE_GREATER_THAN2:
                    case TOKEN_TYPE_AMPERSAND:
                    case TOKEN_TYPE_PIPE:
                    case TOKEN_TYPE_CARET:
                    case TOKEN_TYPE_AND:
                    case TOKEN_TYPE_OR:
                    case TOKEN_TYPE_LET:
                    case TOKEN_TYPE_RETURN:
                        push_back_after_assignment = true;
                        break;
                    default:
                        break;
                }
            }

            if (lhs_node->m_token->m_type != TOKEN_TYPE_LBRACKET){
                // TODO?: change argv to be mutable
                if (lhs_node->m_token->m_type == TOKEN_TYPE_ARGV)
                    return syntax_error("<argv> is immutable", lhs_node->m_token->m_line_number);
                if (lhs_node->m_token->m_type != TOKEN_TYPE_ID)
                    return syntax_error("Trying to assign to rvalue", lhs_node->m_token->m_line_number);

                Var_id_info *var_id_info_ptr = ordered_umap_base_at_key(&self->var_ids, &lhs_node->m_token->m_id).m_value;
                if (!var_id_info_ptr)
                    return syntax_error("Use of undeclared identifier <%s>", lhs_node->m_token->m_line_number, str_base_data_const(&lhs_node->m_token->m_id));

                IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, rhs_node);
                if (compile_result.error != COMPILE_ERROR_NONE)
                    return compile_result;

                Type_info lhs_type_info = var_id_info_ptr->type_info;
                Type_info rhs_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                Type_info bin_op_result = binary_op_type_info_result(BINARY_OP_ASSIGNMENT, lhs_type_info, rhs_type_info);
                if (bin_op_result.m_tag == TYPE_INFO_TAG_NONE)
                    return IR_compiler_state_binary_op_error(self, ast_node, lhs_type_info, rhs_type_info);

                vec_base_pop_back_discard(&self->type_info_stack);

                if (var_id_info_ptr->is_global)
                    add_instruction("%s " BP_SYMBOL "[" USIZE_PFMT "]", op_code_to_str(OP_CODE_MOV), var_id_info_ptr->stack_idx);
                else
                    add_instruction("%s " SP_SYMBOL "[-" USIZE_PFMT "]", op_code_to_str(OP_CODE_MOV), self->type_info_stack.m_size - var_id_info_ptr->stack_idx + 1);

                if (push_back_after_assignment && (compile_result = IR_compiler_state_compile(self, lhs_node)).error != COMPILE_ERROR_NONE)
                    return compile_result;
            }
            else{
                for (
                    const AST_node *lhs_sub_node = lhs_node->m_sub_nodes.m_data[0];
                    lhs_sub_node->m_token->m_type != TOKEN_TYPE_ID;
                    lhs_sub_node = lhs_sub_node->m_sub_nodes.m_data[0]
                ){
                    enum Token_type token_type = lhs_sub_node->m_token->m_type;
                    if (token_type == TOKEN_TYPE_ARGV)
                        return syntax_error("<argv> is immutable", lhs_sub_node->m_token->m_line_number);
                    if (token_type != TOKEN_TYPE_EQUALS1 && token_type != TOKEN_TYPE_LBRACKET)
                        return syntax_error("Trying to assign to rvalue", lhs_sub_node->m_token->m_line_number);
                }

                const AST_node *subscript_lhs_node = lhs_node->m_sub_nodes.m_data[0];
                const AST_node *subscript_rhs_node = lhs_node->m_sub_nodes.m_data[1];

                IR_compiler_state_compile_result compile_result;
                if (
                    (compile_result = IR_compiler_state_compile(self, subscript_lhs_node)).error != COMPILE_ERROR_NONE ||
                    (compile_result = IR_compiler_state_compile(self, subscript_rhs_node)).error != COMPILE_ERROR_NONE
                )
                    return compile_result;

                Type_info subscript_lhs_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 2);
                Type_info subscript_rhs_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                Type_info subscript_bin_op_result = binary_op_type_info_result(BINARY_OP_SUBSCRIPT, subscript_lhs_type_info, subscript_rhs_type_info);
                if (subscript_bin_op_result.m_tag == TYPE_INFO_TAG_NONE)
                    return IR_compiler_state_binary_op_error(self, lhs_node, subscript_lhs_type_info, subscript_rhs_type_info);

                if (push_back_after_assignment){
                    if (!vec_base_push_back(&self->type_info_stack, self->alloc, &subscript_lhs_type_info))
                        return OOM_ERROR;
                    add_instruction("%s " SP_SYMBOL "[-2]", op_code_to_str(OP_CODE_PUSH));
                    if (!vec_base_push_back(&self->type_info_stack, self->alloc, &subscript_rhs_type_info))
                        return OOM_ERROR;
                    add_instruction("%s " SP_SYMBOL "[-2]", op_code_to_str(OP_CODE_PUSH));
                }

                compile_result = IR_compiler_state_compile(self, rhs_node);
                if (compile_result.error != COMPILE_ERROR_NONE)
                    return compile_result;

                Type_info rhs_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                Type_info bin_op_result = binary_op_type_info_result(BINARY_OP_ASSIGNMENT, subscript_bin_op_result, rhs_type_info);
                if (bin_op_result.m_tag == TYPE_INFO_TAG_NONE)
                    return IR_compiler_state_binary_op_error(self, ast_node, subscript_bin_op_result, rhs_type_info);

                vec_base_pop_back_discard(&self->type_info_stack);
                vec_base_pop_back_discard(&self->type_info_stack);
                vec_base_pop_back_discard(&self->type_info_stack);

                add_instruction("%s", op_code_to_str(OP_CODE_MOV_DEREF));

                if (push_back_after_assignment){
                    vec_base_pop_back_discard(&self->type_info_stack);
                    *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1) = subscript_bin_op_result;
                    add_instruction("%s", op_code_to_str(OP_CODE_DEREF));
                }
            }
            break;
        }

        case TOKEN_TYPE_LBRACKET:             bin_op = BINARY_OP_SUBSCRIPT; bin_op_code = OP_CODE_DEREF;   goto bin_op_case;
        case TOKEN_TYPE_EQUALS2:              bin_op = BINARY_OP_CMP_EQ;    bin_op_code = OP_CODE_CMP_EQ;  goto bin_op_case;
        case TOKEN_TYPE_NOT_EQUALS:           bin_op = BINARY_OP_CMP_NEQ;   bin_op_code = OP_CODE_CMP_NEQ; goto bin_op_case;
        case TOKEN_TYPE_LESS_THAN1:           bin_op = BINARY_OP_CMP_LE;    bin_op_code = OP_CODE_CMP_LE;  goto bin_op_case;
        case TOKEN_TYPE_LESS_THAN1_EQUALS:    bin_op = BINARY_OP_CMP_LEQ;   bin_op_code = OP_CODE_CMP_LEQ; goto bin_op_case;
        case TOKEN_TYPE_GREATER_THAN1:        bin_op = BINARY_OP_CMP_GE;    bin_op_code = OP_CODE_CMP_GE;  goto bin_op_case;
        case TOKEN_TYPE_GREATER_THAN1_EQUALS: bin_op = BINARY_OP_CMP_GEQ;   bin_op_code = OP_CODE_CMP_GEQ; goto bin_op_case;
        case TOKEN_TYPE_ASTERISK1:            bin_op = BINARY_OP_MUL;       bin_op_code = OP_CODE_MUL;     goto bin_op_case;
        case TOKEN_TYPE_SLASH:                bin_op = BINARY_OP_DIV;       bin_op_code = OP_CODE_DIV;     goto bin_op_case;
        case TOKEN_TYPE_PERCENT:              bin_op = BINARY_OP_MOD;       bin_op_code = OP_CODE_MOD;     goto bin_op_case;
        case TOKEN_TYPE_ASTERISK2:            bin_op = BINARY_OP_POW;       bin_op_code = OP_CODE_POW;     goto bin_op_case;
        case TOKEN_TYPE_LESS_THAN2:           bin_op = BINARY_OP_SHL;       bin_op_code = OP_CODE_SHL;     goto bin_op_case;
        case TOKEN_TYPE_GREATER_THAN2:        bin_op = BINARY_OP_SHR;       bin_op_code = OP_CODE_SHR;     goto bin_op_case;
        case TOKEN_TYPE_AMPERSAND:            bin_op = BINARY_OP_BAND;      bin_op_code = OP_CODE_BAND;    goto bin_op_case;
        case TOKEN_TYPE_PIPE:                 bin_op = BINARY_OP_BOR;       bin_op_code = OP_CODE_BOR;     goto bin_op_case;
        case TOKEN_TYPE_CARET:                bin_op = BINARY_OP_XOR;       bin_op_code = OP_CODE_XOR;
        bin_op_case:{
            const AST_node *lhs_node = ast_node->m_sub_nodes.m_data[0];
            const AST_node *rhs_node = ast_node->m_sub_nodes.m_data[1];

            IR_compiler_state_compile_result compile_result;
            if (
                (compile_result = IR_compiler_state_compile(self, lhs_node)).error != COMPILE_ERROR_NONE ||
                (compile_result = IR_compiler_state_compile(self, rhs_node)).error != COMPILE_ERROR_NONE
            )
                return compile_result;

            Type_info *lhs_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 2);
            Type_info *rhs_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

            Type_info bin_op_result = binary_op_type_info_result(bin_op, *lhs_type_info_ptr, *rhs_type_info_ptr);
            if (bin_op_result.m_tag == TYPE_INFO_TAG_NONE)
                return IR_compiler_state_binary_op_error(self, ast_node, *lhs_type_info_ptr, *rhs_type_info_ptr);

            *lhs_type_info_ptr = bin_op_result;
            vec_base_pop_back_discard(&self->type_info_stack);

            add_instruction("%s", op_code_to_str(bin_op_code));
            pop_on_discarded_expression(ast_node);
            break;
        }

        case TOKEN_TYPE_AND:
        case TOKEN_TYPE_OR:{
            const AST_node *lhs_node = ast_node->m_sub_nodes.m_data[0];
            const AST_node *rhs_node = ast_node->m_sub_nodes.m_data[1];

            char and_or_label_str_buf[JMP_LABEL_BUFSIZE];
            sprintf(and_or_label_str_buf, JMP_LABEL_FMT, self->label_counter++);

            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, lhs_node);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info bool_type_info = {.m_tag = TYPE_INFO_TAG_BOOL, .m_dimensions = 0};

            Type_info lhs_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);
            if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, bool_type_info, lhs_type_info).m_tag == TYPE_INFO_TAG_NONE)
                return IR_compiler_state_type_conversion_error(self, ast_node, bool_type_info, lhs_type_info);
            add_instruction("%s", op_code_to_str(OP_CODE_TO_BOOL));

            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &lhs_type_info))
                return OOM_ERROR;
            add_instruction("%s " SP_SYMBOL "[-1]", op_code_to_str(OP_CODE_PUSH));
            if (ast_node->m_token->m_type == TOKEN_TYPE_OR)
                add_instruction("%s", op_code_to_str(OP_CODE_NEG));
            vec_base_pop_back_discard(&self->type_info_stack);
            add_instruction("%s %s", op_code_to_str(OP_CODE_JMPZ), and_or_label_str_buf);

            vec_base_pop_back_discard(&self->type_info_stack);
            add_instruction("%s", op_code_to_str(OP_CODE_POP));

            compile_result = IR_compiler_state_compile(self, rhs_node);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info rhs_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);
            if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, bool_type_info, rhs_type_info).m_tag == TYPE_INFO_TAG_NONE)
                return IR_compiler_state_type_conversion_error(self, ast_node, bool_type_info, rhs_type_info);
            add_instruction("%s", op_code_to_str(OP_CODE_TO_BOOL));

            if (binary_op_type_info_result(BINARY_OP_AND, lhs_type_info, rhs_type_info).m_tag == TYPE_INFO_TAG_NONE)
                return IR_compiler_state_binary_op_error(self, ast_node, lhs_type_info, rhs_type_info);

            if (!str_base_append_fmt(&self->IR, self->alloc, "%s:\n", and_or_label_str_buf))
                return OOM_ERROR;

            pop_on_discarded_expression(ast_node);
            break;
        }
        
        case TOKEN_TYPE_FN:{
            const AST_node *fn_id_node          = ast_node->m_sub_nodes.m_data[0];
            AST_node_ptr_slice fn_arg_nodes     = {.m_size = ast_node->m_sub_nodes.m_size - 3, .m_data = &ast_node->m_sub_nodes.m_data[1]};
            const AST_node *fn_return_type_node = ast_node->m_sub_nodes.m_data[ast_node->m_sub_nodes.m_size - 2];
            const AST_node *fn_body_node        = ast_node->m_sub_nodes.m_data[ast_node->m_sub_nodes.m_size - 1];

            const char *fn_id = str_base_data_const(&fn_id_node->m_token->m_id);

            Str_base id_mangled = {0};
            if (ast_node->m_parent){
                if (!str_base_assign_fmt(&id_mangled, self->alloc, LOCAL_LABEL_PREFIX_SYMBOL "%s" USIZE_PFMT, fn_id, self->label_counter++))
                    return OOM_ERROR;
            }
            else if (!str_base_assign_raw(&id_mangled, self->alloc, fn_id))
                return OOM_ERROR;

            Vec_base fn_arg_type_infos = vec_base_init(Type_info);
            if (!vec_base_reserve(&fn_arg_type_infos, self->alloc, fn_arg_nodes.m_size))
                return OOM_ERROR;

            Fn_id_info fn_id_info = {
                .id_mangled       = id_mangled,
                .arg_type_infos   = {.m_size = fn_arg_nodes.m_size, .m_data = fn_arg_type_infos.m_data},
                .return_type_info = ast_node_to_type_info(fn_return_type_node)
            };

            switch (ordered_umap_base_push_back(self->fn_ids_ptr, self->alloc, &fn_id_node->m_token->m_id, &fn_id_info).error){
                case UMAP_INSERT_ERROR_NONE:
                    if (builtin_fn_tag_init(fn_id) != BUILTIN_FN_TAG_NONE){
                case UMAP_INSERT_ERROR_ALREADY_INSERTED:
                        return syntax_error("Function identifier <%s> is already in use", fn_id_node->m_token->m_line_number, fn_id);
                    }
                    break;
                case UMAP_INSERT_ERROR_OOM:
                    return OOM_ERROR;
            }

            ++((Id_count*)vec_base_at(&self->id_count_stack, self->id_count_stack.m_size - 1))->fn_id_count;

            IR_compiler_state fn_IR_compiler_state;

            if (
                !IR_compiler_state_init_in_place(&fn_IR_compiler_state, self->alloc, self->label_counter, self->fn_ids_ptr) ||
                !str_base_append_fmt(&fn_IR_compiler_state.IR, fn_IR_compiler_state.alloc, "%s:\n", str_base_data_const(&fn_id_info.id_mangled))
            )
                return OOM_ERROR;

            Id_count *id_count_ptr = vec_base_at(&fn_IR_compiler_state.id_count_stack, 0);
            for (usize i = 0; i < ((Id_count*)vec_base_at(&self->id_count_stack, 0))->var_id_count; ++i){
                Umap_pair pair = ordered_umap_base_at_idx(&self->var_ids, i);
                if (
                    !vec_base_push_back(&fn_IR_compiler_state.type_info_stack, fn_IR_compiler_state.alloc, &((Var_id_info*)pair.m_value)->type_info) ||
                    ordered_umap_base_push_back(&fn_IR_compiler_state.var_ids, fn_IR_compiler_state.alloc, pair.m_key, pair.m_value).error != UMAP_INSERT_ERROR_NONE
                )
                    return OOM_ERROR;
                ++id_count_ptr->var_id_count;
            }
            if (!vec_base_push_back(&fn_IR_compiler_state.id_count_stack, fn_IR_compiler_state.alloc, &(Id_count){0}))
                return OOM_ERROR;

            for (usize i = 0; i < fn_arg_nodes.m_size; ++i){
                const AST_node *arg_id_node = fn_arg_nodes.m_data[i];
                Type_info arg_type_info = ast_node_to_type_info(arg_id_node->m_sub_nodes.m_data[0]);

                if (!vec_base_push_back(&fn_IR_compiler_state.type_info_stack, fn_IR_compiler_state.alloc, &arg_type_info))
                    return OOM_ERROR;

                IR_compiler_state_compile_result insert_var_id_result = IR_compiler_state_push_back_var_id(&fn_IR_compiler_state, arg_id_node, arg_type_info);
                if (insert_var_id_result.error != COMPILE_ERROR_NONE)
                    return insert_var_id_result;

                (void)vec_base_push_back(&fn_arg_type_infos, self->alloc, &arg_type_info);
            }

            assert(fn_arg_type_infos.m_size == fn_id_info.arg_type_infos.m_size);
            assert(fn_arg_type_infos.m_size == fn_arg_type_infos.m_capacity);

            const AST_node *fn_body_last_node = fn_body_node;
            if (fn_id_info.return_type_info.m_tag != TYPE_INFO_TAG_VOID){
                if (
                    fn_body_node->m_sub_nodes.m_size == 0 ||
                    (fn_body_last_node = fn_body_node->m_sub_nodes.m_data[fn_body_node->m_sub_nodes.m_size - 1])->m_token->m_type != TOKEN_TYPE_RETURN ||
                    fn_body_last_node->m_sub_nodes.m_size == 0
                ){
                    return syntax_error(
                        "Function returning non-void must end with a <return> statement that contains an expression",
                        fn_body_last_node->m_token->m_line_number
                    );
                }
            }
            else if (
                fn_body_node->m_sub_nodes.m_size > 0 &&
                (fn_body_last_node = fn_body_node->m_sub_nodes.m_data[fn_body_node->m_sub_nodes.m_size - 1])->m_token->m_type == TOKEN_TYPE_RETURN &&
                fn_body_last_node->m_sub_nodes.m_size > 0
            )
                return syntax_error("Function with return type <void> returning non-void", fn_body_last_node->m_token->m_line_number);

            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(&fn_IR_compiler_state, fn_body_node);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            if (
                fn_id_info.return_type_info.m_tag == TYPE_INFO_TAG_VOID && (
                    fn_body_node->m_sub_nodes.m_size == 0 ||
                    fn_body_node->m_sub_nodes.m_data[fn_body_node->m_sub_nodes.m_size - 1]->m_token->m_type != TOKEN_TYPE_RETURN
                )
            ){
                usize fn_IR_compiler_state_type_info_stack_size = fn_IR_compiler_state.type_info_stack.m_size;
                fn_IR_compiler_state.type_info_stack.m_size = ((Id_count*)vec_base_at(&fn_IR_compiler_state.id_count_stack, 0))->var_id_count;
                if (!IR_compiler_state_add_instruction(
                    &fn_IR_compiler_state,
                    "%s " USIZE_PFMT,
                    op_code_to_str(OP_CODE_RETV),
                    fn_IR_compiler_state_type_info_stack_size - fn_IR_compiler_state.type_info_stack.m_size
                ))
                    return OOM_ERROR;
                fn_IR_compiler_state.type_info_stack.m_size = fn_IR_compiler_state_type_info_stack_size;
            }

            if (
                !str_base_append_str_base(&fn_IR_compiler_state.IR, self->alloc, &fn_IR_compiler_state.fn_IRs) ||
                !str_base_append_str_base(&self->fn_IRs, self->alloc, &fn_IR_compiler_state.IR)
            )
                return OOM_ERROR;

            self->label_counter = fn_IR_compiler_state.label_counter;
            break;
        }
        case TOKEN_TYPE_LET:{
            const AST_node *id_node   = ast_node->m_sub_nodes.m_data[0];
            const AST_node *type_node = ast_node->m_sub_nodes.m_data[1];
            const AST_node *expr_node = ast_node->m_sub_nodes.m_data[2];

            Type_info id_type_info = ast_node_to_type_info(type_node);

            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, expr_node);
            if (compile_result.error != COMPILE_ERROR_NONE || (compile_result = IR_compiler_state_push_back_var_id(self, id_node, id_type_info)).error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info *last_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1), last_type_info = *last_type_info_ptr;
            if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, id_type_info, last_type_info).m_tag == TYPE_INFO_TAG_NONE){
                Str_base_result type_str;
                Str_base_result expr_type_str;
                if (
                    !(type_str = type_info_to_str_base(id_type_info, self->alloc)).success ||
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

            *last_type_info_ptr = id_type_info;

            add_type_conversion_instruction(id_type_info);
            break;
        }

        case TOKEN_TYPE_IF:{
            char if_end_label_str_buf[JMP_LABEL_BUFSIZE];
            sprintf(if_end_label_str_buf, JMP_LABEL_FMT, self->label_counter++);

            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[0]);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info last_type_info;
            vec_base_pop_back_to(&self->type_info_stack, &last_type_info);

            Type_info bool_type_info = {.m_tag = TYPE_INFO_TAG_BOOL, .m_dimensions = 0};
            if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, bool_type_info, last_type_info).m_tag == TYPE_INFO_TAG_NONE)
                return IR_compiler_state_type_conversion_error(self, ast_node, (Type_info){.m_tag = TYPE_INFO_TAG_BOOL, .m_dimensions = 0}, last_type_info);

            add_instruction("%s %s", op_code_to_str(OP_CODE_JMPZ), if_end_label_str_buf);

            if (ast_node->m_sub_nodes.m_size > 1){
                bool has_body = (ast_node->m_sub_nodes.m_data[1]->m_token->m_type != TOKEN_TYPE_ELSE);
                bool has_else = (ast_node->m_sub_nodes.m_data[ast_node->m_sub_nodes.m_size - 1]->m_token->m_type == TOKEN_TYPE_ELSE);
                
                if (has_body){
                    if (!vec_base_push_back(&self->id_count_stack, self->alloc, &(Id_count){0}))
                        return OOM_ERROR;
                    compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[1]);
                    if (compile_result.error != COMPILE_ERROR_NONE)
                        return compile_result;
                    pop_ids_in_current_scope();
                    if (!has_else && !str_base_append_fmt(&self->IR, self->alloc, "%s:\n", if_end_label_str_buf))
                        return OOM_ERROR;
                }

                if (has_else){
                    char else_end_label_str_buf[JMP_LABEL_BUFSIZE];
                    sprintf(else_end_label_str_buf, JMP_LABEL_FMT, self->label_counter++);

                    add_instruction("%s %s", op_code_to_str(OP_CODE_JMP), else_end_label_str_buf);

                    if (!str_base_append_fmt(&self->IR, self->alloc, "%s:\n", if_end_label_str_buf))
                        return OOM_ERROR;

                    AST_node_ptr_slice else_node_sub_nodes = ast_node->m_sub_nodes.m_data[ast_node->m_sub_nodes.m_size - 1]->m_sub_nodes;
                    if (else_node_sub_nodes.m_size > 0){
                        if (!vec_base_push_back(&self->id_count_stack, self->alloc, &(Id_count){0}))
                            return OOM_ERROR;
                        compile_result = IR_compiler_state_compile(self, else_node_sub_nodes.m_data[0]);
                        if (compile_result.error != COMPILE_ERROR_NONE)
                            return compile_result;
                        pop_ids_in_current_scope();
                    }

                    if (!str_base_append_fmt(&self->IR, self->alloc, "%s:\n", else_end_label_str_buf))
                        return OOM_ERROR;
                }
            }
            else if (!str_base_append_fmt(&self->IR, self->alloc, "%s:\n", if_end_label_str_buf))
                return OOM_ERROR;
            break;
        }

        case TOKEN_TYPE_WHILE:{
            char    start_label_str_buf[JMP_LABEL_BUFSIZE];
            char    break_label_str_buf[JMP_LABEL_BUFSIZE];
            char continue_label_str_buf[JMP_LABEL_BUFSIZE];

            sprintf(   start_label_str_buf, JMP_LABEL_FMT, self->label_counter++);
            sprintf(   break_label_str_buf, JMP_LABEL_FMT, self->label_counter++);
            sprintf(continue_label_str_buf, JMP_LABEL_FMT, self->label_counter++);

            if (!str_base_append_fmt(&self->IR, self->alloc, "%s:\n", start_label_str_buf))
                return OOM_ERROR;

            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[0]);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info last_type_info;
            vec_base_pop_back_to(&self->type_info_stack, &last_type_info);

            Type_info bool_type_info = {.m_tag = TYPE_INFO_TAG_BOOL, .m_dimensions = 0};
            if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, bool_type_info, last_type_info).m_tag == TYPE_INFO_TAG_NONE)
                return IR_compiler_state_type_conversion_error(self, ast_node, (Type_info){.m_tag = TYPE_INFO_TAG_BOOL, .m_dimensions = 0}, last_type_info);

            add_instruction("%s %s", op_code_to_str(OP_CODE_JMPZ), break_label_str_buf);

            bool has_continue_expr = false;
            if (ast_node->m_sub_nodes.m_size > 1){
                has_continue_expr = (ast_node->m_sub_nodes.m_data[1]->m_token->m_type == TOKEN_TYPE_COLON);
                bool has_body = (ast_node->m_sub_nodes.m_data[ast_node->m_sub_nodes.m_size - 1]->m_token->m_type != TOKEN_TYPE_COLON);

                if (has_body){
                    if (
                        !vec_base_push_back(
                            &self->while_label_info_stack,
                            self->alloc,
                            &(While_label_info){
                                .break_label_str    = break_label_str_buf,
                                .continue_label_str = continue_label_str_buf,
                                .id_count_stack_idx = self->id_count_stack.m_size
                            }
                        ) ||
                        !vec_base_push_back(&self->id_count_stack, self->alloc, &(Id_count){0})
                    )
                        return OOM_ERROR;
                    compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[ast_node->m_sub_nodes.m_size - 1]);
                    if (compile_result.error != COMPILE_ERROR_NONE)
                        return compile_result;
                    pop_ids_in_current_scope();
                    vec_base_pop_back_discard(&self->while_label_info_stack);
                }
            }

            if (!str_base_append_fmt(&self->IR, self->alloc, "%s:\n", continue_label_str_buf))
                return OOM_ERROR;
            if (has_continue_expr && (compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[1]->m_sub_nodes.m_data[0])).error != COMPILE_ERROR_NONE)
                return compile_result;

            add_instruction("%s %s", op_code_to_str(OP_CODE_JMP), start_label_str_buf);

            if (!str_base_append_fmt(&self->IR, self->alloc, "%s:\n", break_label_str_buf))
                return OOM_ERROR;
            break;
        }

        case TOKEN_TYPE_BREAK:
        case TOKEN_TYPE_CONTINUE:{
            if (self->while_label_info_stack.m_size == 0)
                return syntax_error("<%s> must be used inside a loop", ast_node->m_token->m_line_number, str_base_data_const(&ast_node->m_token->m_id));
            While_label_info while_label_info = *(While_label_info*)vec_base_at(&self->while_label_info_stack, self->while_label_info_stack.m_size - 1);
            usize type_info_stack_size = self->type_info_stack.m_size;
            for (usize i = self->id_count_stack.m_size; i-- > while_label_info.id_count_stack_idx;){
                for (usize var_id_count = ((Id_count*)vec_base_at(&self->id_count_stack, i))->var_id_count; var_id_count-- > 0;){
                    --self->type_info_stack.m_size;
                    add_instruction("%s", op_code_to_str(OP_CODE_POP));
                }
            }
            add_instruction(
                "%s %s",
                op_code_to_str(OP_CODE_JMP),
                (ast_node->m_token->m_type == TOKEN_TYPE_BREAK) ? while_label_info.break_label_str : while_label_info.continue_label_str
            );
            self->type_info_stack.m_size = type_info_stack_size;
            break;
        }
        case TOKEN_TYPE_RETURN:{
            const AST_node *fn_node = ast_node->m_parent;
            while (fn_node && fn_node->m_token->m_type != TOKEN_TYPE_FN)
                fn_node = fn_node->m_parent;

            if (!fn_node){
                if (ast_node->m_sub_nodes.m_size != 1)
                    return syntax_error("The program must return a non-void value on exit", ast_node->m_token->m_line_number);

                IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[0]);
                if (compile_result.error != COMPILE_ERROR_NONE)
                    return compile_result;

                vec_base_pop_back_discard(&self->type_info_stack);
                add_instruction("%s %s", op_code_to_str(OP_CODE_CALL), builtin_fn_tag_to_str(BUILTIN_FN_TAG_EXIT));
            }
            else{
                enum Op_code ret_op_code = OP_CODE_RETV;

                Fn_id_info *fn_id_info_ptr = ordered_umap_base_at_key(self->fn_ids_ptr, &fn_node->m_sub_nodes.m_data[0]->m_token->m_id).m_value;
                if (fn_id_info_ptr->return_type_info.m_tag != TYPE_INFO_TAG_VOID){
                    if (ast_node->m_sub_nodes.m_size != 1)
                        return syntax_error("Function returning non-void must end with a <return> statement that contains an expression", ast_node->m_token->m_line_number);

                    ret_op_code = OP_CODE_RET;

                    IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[0]);
                    if (compile_result.error != COMPILE_ERROR_NONE)
                        return compile_result;

                    Type_info *last_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                    if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, fn_id_info_ptr->return_type_info, *last_type_info_ptr).m_tag == TYPE_INFO_TAG_NONE)
                        return IR_compiler_state_type_conversion_error(self, ast_node, fn_id_info_ptr->return_type_info, *last_type_info_ptr);

                    add_type_conversion_instruction(fn_id_info_ptr->return_type_info);

                    vec_base_pop_back_discard(&self->type_info_stack);
                }
                else if (ast_node->m_sub_nodes.m_size > 0)
                    return syntax_error("Function with return type <void> returning non-void", ast_node->m_token->m_line_number);

                usize type_info_stack_size = self->type_info_stack.m_size;
                self->type_info_stack.m_size = ((Id_count*)vec_base_at(&self->id_count_stack, 0))->var_id_count;
                add_instruction("%s " USIZE_PFMT, op_code_to_str(ret_op_code), type_info_stack_size - self->type_info_stack.m_size);
                self->type_info_stack.m_size = type_info_stack_size;
            }
            break;
        }

        default:
            fprintf(stderr, "Not implemented\n");
            abort();
    }

    return (IR_compiler_state_compile_result){.error = COMPILE_ERROR_NONE};
}

// ------------------------------------------------------------------------------------------------

const char* op_code_to_str(enum Op_code op_code){
    switch (op_code){
        case OP_CODE_PUSH:      return "push";
        case OP_CODE_POP:       return "pop";

        case OP_CODE_CALL:      return "call";
        case OP_CODE_RET:       return "ret";
        case OP_CODE_RETV:      return "retv";

        case OP_CODE_JMP:       return "jmp";
        case OP_CODE_JMPZ:      return "jmpz";

        case OP_CODE_TO_BOOL:   return "to_bool";
        case OP_CODE_TO_CHAR:   return "to_char";
        case OP_CODE_TO_INT:    return "to_int";
        case OP_CODE_TO_FLOAT:  return "to_float";
        case OP_CODE_TO_STR:    return "to_str";

        case OP_CODE_NEG:       return "neg";
        case OP_CODE_BNEG:      return "bneg";

        case OP_CODE_DEREF:     return "deref";

        case OP_CODE_MOV:       return "mov";
        case OP_CODE_MOV_DEREF: return "mov_deref";

        case OP_CODE_CMP_EQ:    return "cmp_eq";
        case OP_CODE_CMP_NEQ:   return "cmp_neq";
        case OP_CODE_CMP_LE:    return "cmp_le";
        case OP_CODE_CMP_LEQ:   return "cmp_leq";
        case OP_CODE_CMP_GE:    return "cmp_ge";
        case OP_CODE_CMP_GEQ:   return "cmp_geq";

        case OP_CODE_ADD:       return "add";
        case OP_CODE_SUB:       return "sub";
        case OP_CODE_MUL:       return "mul";
        case OP_CODE_DIV:       return "div";
        case OP_CODE_MOD:       return "mod";
        case OP_CODE_POW:       return "pow";

        case OP_CODE_SHL:       return "shl";
        case OP_CODE_SHR:       return "shr";
        case OP_CODE_BAND:      return "band";
        case OP_CODE_BOR:       return "bor";
        case OP_CODE_XOR:       return "xor";

        default:                return NULL;
    };
}

IR_compile_result IR_compile(Arena *arena, AST_node_ptr_slice ast_nodes){
    assert(arena && "<arena> is not nullable");

    Allocator alloc = arena_allocator(arena);
    IR_compiler_state state;
    Ordered_umap_base *fn_ids_ptr = allocator_alloc(alloc, Ordered_umap_base, 1);

    if (!fn_ids_ptr || !IR_compiler_state_init_in_place(&state, alloc, 0, fn_ids_ptr))
        goto oom_error;

    *fn_ids_ptr = ordered_umap_base_init(Str_base, Fn_id_info);

    for (usize i = 0; i < ast_nodes.m_size; ++i){
        IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(&state, ast_nodes.m_data[i]);
        if (compile_result.error != COMPILE_ERROR_NONE)
            return (IR_compile_result){.error_info = compile_result.error_info, .error = compile_result.error};
    }

    if (
        !IR_compiler_state_pop_ids_in_current_scope(&state) || (
            (ast_nodes.m_size == 0 || ast_nodes.m_data[ast_nodes.m_size - 1]->m_token->m_type != TOKEN_TYPE_RETURN) && (
                !vec_base_push_back(&state.type_info_stack, state.alloc, &(Type_info){.m_tag = TYPE_INFO_TAG_INT, .m_dimensions = 0}) ||
                !IR_compiler_state_add_instruction(&state, "%s 0", op_code_to_str(OP_CODE_PUSH)) || (
                    vec_base_pop_back_discard(&state.type_info_stack),
                    !IR_compiler_state_add_instruction(&state, "%s %s", op_code_to_str(OP_CODE_CALL), builtin_fn_tag_to_str(BUILTIN_FN_TAG_EXIT))
                )
            )
        ) ||
        !str_base_append_str_base(&state.IR, state.alloc, &state.fn_IRs)
    )
        goto oom_error;

    return (IR_compile_result){.IR = state.IR, .error = COMPILE_ERROR_NONE};

oom_error:
    return (IR_compile_result){.error = COMPILE_ERROR_OOM};
}
