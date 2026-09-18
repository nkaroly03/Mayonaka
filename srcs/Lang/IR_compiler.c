#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../hdrs/Allocator/Arena.h"
#include "../../hdrs/Data_structure/Ordered_umap_base.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

#include "../../hdrs/Lang/Builtin_fn.h"
#include "../../hdrs/Lang/IR_compiler.h"
#include "../../hdrs/Lang/Lexer.h"
#include "../../hdrs/Lang/Parser.h"
#include "../../hdrs/Lang/Type_info.h"

// ------------------------------------------------------------------------------------------------

typedef struct Id_count{
    usize fn_id_count;
    usize var_id_count;
    usize type_id_count;
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

typedef struct Type_id_info_maps{
    Ordered_umap_base str_id_map;
    Ordered_umap_base type_info_tag_as_i32_id_map;
    i32 type_id_counter;
} Type_id_info_maps;

typedef struct Type_id_info{
    Str_base str_id;
    enum Type_info_tag type_info_tag_id;
    Ordered_umap_base field_infos;
} Type_id_info;

typedef struct Type_id_info_field_info{
    usize field_idx;
    Type_info field_type_info;
} Type_id_info_field_info;

typedef struct IR_compiler_state{
    Allocator alloc;
    Vec_base id_count_stack;
    Ordered_umap_base *fn_ids_ptr;
    Ordered_umap_base var_ids;
    Type_id_info_maps *type_id_info_maps_ptr;
    i32 type_id_counter;
    Vec_base type_info_stack;
    usize *label_counter_ptr;
    Ordered_umap_base while_labels;
    Str_base IR;
    Str_base fn_IRs;
} IR_compiler_state;

static const AST_node* ast_node_find_fn_node(const AST_node *ast_node){
    while (ast_node && ast_node->m_type != AST_NODE_TYPE_DECL_FN)
        ast_node = ast_node->m_parent;
    return ast_node;
}

static const AST_node* IR_compiler_state_ast_node_to_type_info(IR_compiler_state *self, const AST_node *type_node, Type_info *out_type_info){
    Type_info result = {.m_tag = TYPE_INFO_TAG_NONE};
    while (type_node->m_type == AST_NODE_TYPE_TYPE_LIST){
        type_node = type_node->m_sub_nodes.m_data[0];
        ++result.m_dimensions;
    }
    if (type_node->m_type != AST_NODE_TYPE_TYPE_ID)
        result.m_tag = (enum Type_info_tag)(TYPE_INFO_TAG_VOID + (type_node->m_type - AST_NODE_TYPE_TYPE_VOID));
    else{
        Type_id_info *type_id_info_ptr = ordered_umap_base_at_key(&self->type_id_info_maps_ptr->str_id_map, &type_node->m_token->m_id).m_value;
        if (!type_id_info_ptr)
            return type_node;
        result.m_tag = type_id_info_ptr->type_info_tag_id;
    }
    *out_type_info = result;
    return NULL;
}
#define ast_node_to_type_info(type_node, out_type_info) IR_compiler_state_ast_node_to_type_info(self, (type_node), (out_type_info))

static Str_base_result IR_compiler_state_type_info_to_str_base(IR_compiler_state *self, Type_info type_info, Allocator alloc){
    Str_base result = {0};

    for (usize i = 0; i < type_info.m_dimensions; ++i)
        if (!str_base_append_raw(&result, alloc, "[]"))
            goto oom_error;

    const char *type_info_tag_str;
    switch (type_info.m_tag){
        case TYPE_INFO_TAG_NONE:  unreachable();
        case TYPE_INFO_TAG_VOID:  type_info_tag_str = "void";  break;
        case TYPE_INFO_TAG_BOOL:  type_info_tag_str = "bool";  break;
        case TYPE_INFO_TAG_CHAR:  type_info_tag_str = "char";  break;
        case TYPE_INFO_TAG_INT:   type_info_tag_str = "int";   break;
        case TYPE_INFO_TAG_FLOAT: type_info_tag_str = "float"; break;
        case TYPE_INFO_TAG_STR:   type_info_tag_str = "str";   break;
        default:
            type_info_tag_str = str_base_data(
                &((Type_id_info*)ordered_umap_base_at_key(&self->type_id_info_maps_ptr->type_info_tag_as_i32_id_map, &(i32){(i32)type_info.m_tag}).m_value)->str_id
            );
            break;
    }

    if (!str_base_append_raw(&result, alloc, type_info_tag_str))
        goto oom_error;

    return (Str_base_result){.result = result, .success = true};

oom_error:
    str_base_deinit(&result, alloc);
    return (Str_base_result){0};
}
#define type_info_to_str_base(type_info, alloc) IR_compiler_state_type_info_to_str_base(self, (type_info), (alloc))

static Str_base_result IR_compiler_state_type_info_slice_to_str_base(IR_compiler_state *self, Type_info_slice type_info_slice, Allocator alloc){
    Str_base result = {0};
    for (usize i = 0; i < type_info_slice.m_size; ++i){
        Str_base_result type_info_str = type_info_to_str_base(type_info_slice.m_data[i], alloc);
        if (!type_info_str.success || !str_base_append_fmt(&result, alloc, "%s, ", str_base_data(&type_info_str.result)))
            return (Str_base_result){0};
    }
    str_base_pop_back(&result);
    str_base_pop_back(&result);
    return (Str_base_result){.result = result, .success = true};
}
#define type_info_slice_to_str_base(type_info_slice, alloc) IR_compiler_state_type_info_slice_to_str_base(self, (type_info_slice), (alloc))

static bool IR_compiler_state_init_in_place(
    IR_compiler_state *self,
	Allocator arena_alloc,
	Ordered_umap_base *fn_ids_ptr,
	Type_id_info_maps *type_id_info_maps_ptr,
	usize *label_counter_ptr
){
    *self = (IR_compiler_state){
        .alloc                 = arena_alloc,
        .id_count_stack        = vec_base_init(Id_count),
        .fn_ids_ptr            = fn_ids_ptr,
        .var_ids               = ordered_umap_base_init(Str_base, Var_id_info),
        .type_id_info_maps_ptr = type_id_info_maps_ptr,
        .type_info_stack       = vec_base_init(Type_info),
        .label_counter_ptr     = label_counter_ptr,
        .while_labels          = ordered_umap_base_init(Str_base, While_label_info),
        .IR                    = {0},
        .fn_IRs                = {0}
    };
    return vec_base_push_back(&self->id_count_stack, self->alloc, &(Id_count){0}) != NULL;
}

typedef struct IR_compiler_state_compile_result{
    Str_base error_info;
    enum Compile_error error;
} IR_compiler_state_compile_result;

static const IR_compiler_state_compile_result  NO_ERROR = {.error = COMPILE_ERROR_NONE};
static const IR_compiler_state_compile_result OOM_ERROR = {.error = COMPILE_ERROR_OOM};

static IR_compiler_state_compile_result IR_compiler_state_syntax_error(IR_compiler_state *self, const AST_node *ast_node, const char *fmt, ...){
    Token_positiion_info pos = ast_node->m_token->m_pos;

    Str_base_result error_info = str_base_init_fmt(self->alloc, "<" USIZE_PFMT ":" USIZE_PFMT ">: ", pos.m_line, pos.m_column);
    if (error_info.success){
        va_list args;
        va_start(args, fmt);
        error_info.success = str_base_append_fmt_va_list(&error_info.result, self->alloc, fmt, args);
        va_end(args);
    }

    return (error_info.success) ? (IR_compiler_state_compile_result){.error_info = error_info.result, .error = COMPILE_ERROR_SYNTAX} : OOM_ERROR;
}
#define syntax_error(ast_node_val, ...) IR_compiler_state_syntax_error(self, (ast_node_val), __VA_ARGS__)

static IR_compiler_state_compile_result IR_compiler_state_undeclared_type_id_error(IR_compiler_state *self, const AST_node *type_id_node){
    return syntax_error(type_id_node, "Use of undeclared type identifier <%s>", str_base_data_const(&type_id_node->m_token->m_id));
}
#define undeclared_type_id_error(type_id_node) IR_compiler_state_undeclared_type_id_error(self, (type_id_node))

static IR_compiler_state_compile_result IR_compiler_state_unary_op_error(IR_compiler_state *self, const AST_node *un_op_node, Type_info type_info){
    Str_base_result type_info_str = type_info_to_str_base(type_info, self->alloc);
    if (!type_info_str.success)
        return OOM_ERROR;
    return syntax_error(un_op_node, "Invalid unary operation <%s> on <%s>", str_base_data_const(&un_op_node->m_token->m_id), str_base_data(&type_info_str.result));
}
#define unary_op_error(un_op_node, type_info) IR_compiler_state_unary_op_error(self, (un_op_node), (type_info))

static IR_compiler_state_compile_result IR_compiler_state_binary_op_error(IR_compiler_state *self, const AST_node *bin_op_node, Type_info lhs_type_info, Type_info rhs_type_info){
    Str_base_result lhs_type_info_str;
    Str_base_result rhs_type_info_str;
    if (
        !(lhs_type_info_str = type_info_to_str_base(lhs_type_info, self->alloc)).success ||
        !(rhs_type_info_str = type_info_to_str_base(rhs_type_info, self->alloc)).success
    )
        return OOM_ERROR;
    return syntax_error(
        bin_op_node,
        "Invalid binary operation <%s> between <%s> and <%s>",
        str_base_data_const(&bin_op_node->m_token->m_id),
        str_base_data(&lhs_type_info_str.result),
        str_base_data(&rhs_type_info_str.result)
    );
}
#define binary_op_error(bin_op_node, lhs_type_info, rhs_type_info) IR_compiler_state_binary_op_error(self, (bin_op_node), (lhs_type_info), (rhs_type_info))

static IR_compiler_state_compile_result IR_compiler_state_type_conversion_error(
    IR_compiler_state *self,
    const AST_node *ast_node,
    Type_info dest_type_info,
    Type_info src_type_info
){
    Str_base_result dest_type_info_str;
    Str_base_result  src_type_info_str;
    if (
        !(dest_type_info_str = type_info_to_str_base(dest_type_info, self->alloc)).success ||
        !( src_type_info_str = type_info_to_str_base( src_type_info, self->alloc)).success
    )
        return OOM_ERROR;
    return syntax_error(ast_node, "Expression with type <%s> is not convertible to <%s>", str_base_data(&src_type_info_str.result), str_base_data(&dest_type_info_str.result));
}
#define type_conversion_error(ast_node, dest_type_info, src_type_info) IR_compiler_state_type_conversion_error(self, (ast_node), (dest_type_info), (src_type_info))

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
#define add_instruction(...) IR_compiler_state_add_instruction(self, __VA_ARGS__)

static bool IR_compiler_state_add_type_conversion_instruction(IR_compiler_state *self, Type_info dest_type_info){
    return (dest_type_info.m_dimensions == 0 && dest_type_info.m_tag >= TYPE_INFO_TAG_BOOL && dest_type_info.m_tag <= TYPE_INFO_TAG_STR)
        ? add_instruction("%s", op_code_to_str((enum Op_code)(OP_CODE_TO_BOOL + (dest_type_info.m_tag - TYPE_INFO_TAG_BOOL))))
        : true
    ;
}
#define add_type_conversion_instruction(dest_type_info) IR_compiler_state_add_type_conversion_instruction(self, (dest_type_info))

static bool IR_compiler_state_pop_on_discarded_expression(IR_compiler_state *self, const AST_node *ast_node){
    if (ast_node->m_parent){
        const AST_node *parent = ast_node->m_parent;
        enum AST_node_type parent_token_type = parent->m_type;
        switch (parent_token_type){
            case AST_NODE_TYPE_STATEMENT_BLOCK:
                break;
            case AST_NODE_TYPE_STATEMENT_IF:
            case AST_NODE_TYPE_STATEMENT_WHILE:
                if (parent->m_sub_nodes.m_data[parent_token_type == AST_NODE_TYPE_STATEMENT_WHILE] != ast_node)
                    break;
                FALLTHROUGH;
            default:
                return true;
        }
    }

    vec_base_pop_back_discard(&self->type_info_stack);

    return add_instruction("%s 1", op_code_to_str(OP_CODE_POP));
}
#define pop_on_discarded_expression(ast_node) IR_compiler_state_pop_on_discarded_expression(self, (ast_node))

static IR_compiler_state_compile_result IR_compiler_state_push_back_var_id(IR_compiler_state *self, const AST_node *id_node, Type_info id_type_info){
    assert(id_node->m_parent->m_type == AST_NODE_TYPE_DECL_FN || id_node->m_parent->m_type == AST_NODE_TYPE_DECL_VAR);
    assert(self->var_ids.m_keys.m_size == self->type_info_stack.m_size - 1);

    enum Umap_insert_error insert_error = ordered_umap_base_push_back(
        &self->var_ids,
        self->alloc,
        &id_node->m_token->m_id,
        &(Var_id_info){
            .stack_idx = self->var_ids.m_keys.m_size,
            .type_info = id_type_info,
            .is_global = (id_node->m_parent->m_type == AST_NODE_TYPE_DECL_VAR && !id_node->m_parent->m_parent)
        }
    ).error;

    switch (insert_error){
        case UMAP_INSERT_ERROR_NONE:
            break;
        case UMAP_INSERT_ERROR_OOM:
            return OOM_ERROR;
        case UMAP_INSERT_ERROR_ALREADY_INSERTED:
            return syntax_error(id_node, "Identifier <%s> is already in use", str_base_data_const(&id_node->m_token->m_id));
    }

    ++((Id_count*)vec_base_at(&self->id_count_stack, self->id_count_stack.m_size - 1))->var_id_count;

    return NO_ERROR;
}
#define push_back_var_id(id_node, id_type_info) IR_compiler_state_push_back_var_id(self, (id_node), (id_type_info))

static bool IR_compiler_state_pop_ids_in_current_scope(IR_compiler_state *self){
    Id_count id_count;
    vec_base_pop_back_to(&self->id_count_stack, &id_count);

    Allocator alloc = self->alloc;

    Type_id_info_maps *type_id_info_maps_ptr = self->type_id_info_maps_ptr;

    Ordered_umap_base *type_info_tag_as_i32_id_map_ptr = &type_id_info_maps_ptr->type_info_tag_as_i32_id_map;
    Ordered_umap_base *str_id_map_ptr                  = &type_id_info_maps_ptr->str_id_map;
    i32 *type_id_counter_ptr                           = &type_id_info_maps_ptr->type_id_counter;

    Ordered_umap_base *fn_ids_ptr = self->fn_ids_ptr;
    Ordered_umap_base *var_ids_ptr = &self->var_ids;
    Vec_base *type_info_stack_ptr = &self->type_info_stack;

    while (id_count.type_id_count-- > 0){
        ordered_umap_base_pop_back_discard(type_info_tag_as_i32_id_map_ptr, alloc);
        ordered_umap_base_pop_back_discard(str_id_map_ptr, alloc);
        --*type_id_counter_ptr;
    }
    while (id_count.fn_id_count-- > 0)
        ordered_umap_base_pop_back_discard(fn_ids_ptr, alloc);
    for (usize i = id_count.var_id_count; i-- > 0;){
        ordered_umap_base_pop_back_discard(var_ids_ptr, alloc);
        vec_base_pop_back_discard(type_info_stack_ptr);
    }

    return (id_count.var_id_count > 0) ? add_instruction("%s " USIZE_PFMT, op_code_to_str(OP_CODE_POP), id_count.var_id_count) : true;
}
#define pop_ids_in_current_scope() IR_compiler_state_pop_ids_in_current_scope(self)

static IR_compiler_state_compile_result IR_compiler_state_init_list_type_info_from_context(
    IR_compiler_state *self,
    const AST_node *init_list_node,
    Type_info *out_init_list_type_info
){
    if (!init_list_node->m_parent)
        goto init_list_context_error;

    const AST_node *parent = init_list_node->m_parent;

    switch (parent->m_type){
        case AST_NODE_TYPE_ATOM_INIT_LIST:{
            IR_compiler_state_compile_result from_context_result = IR_compiler_state_init_list_type_info_from_context(self, parent, out_init_list_type_info);
            if (from_context_result.error != COMPILE_ERROR_NONE)
                return from_context_result;
            if (--out_init_list_type_info->m_dimensions == 0)
                return syntax_error(init_list_node, "Initializer list has an incorrect number of dimensions");
            break;
        }
        case AST_NODE_TYPE_BINARY_OP_AS:{
            const AST_node *type_id_node = ast_node_to_type_info(parent->m_sub_nodes.m_data[1], out_init_list_type_info);
            if (type_id_node)
                return undeclared_type_id_error(type_id_node);
            if (out_init_list_type_info->m_dimensions == 0)
                return syntax_error(init_list_node, "Casting initializer list to non-list type in <as> expression");
            break;
        }
        case AST_NODE_TYPE_BINARY_OP_ASSIGNMENT:{
            usize i = 0;
            const AST_node *id_node = parent->m_sub_nodes.m_data[0];
            while (id_node->m_type != AST_NODE_TYPE_ATOM_ID){
                i += (id_node->m_type == AST_NODE_TYPE_BINARY_OP_SUBSCRIPT);
                id_node = id_node->m_sub_nodes.m_data[0];
            }
            *out_init_list_type_info = ((Var_id_info*)ordered_umap_base_at_key(&self->var_ids, &id_node->m_token->m_id).m_value)->type_info;
            out_init_list_type_info->m_dimensions -= i;
            break;
        }
        case AST_NODE_TYPE_FN_CALL:{
            usize i = 1;
            while (parent->m_sub_nodes.m_data[i] != init_list_node)
                ++i;
            const Str_base *fn_id = &parent->m_sub_nodes.m_data[0]->m_token->m_id;
            if (builtin_fn_tag_init(str_base_data_const(fn_id)) != BUILTIN_FN_TAG_NONE)
                return syntax_error(init_list_node, "Using an initializer list as a parameter to a function is only allowed in user-defined functions");
            *out_init_list_type_info = ((Fn_id_info*)ordered_umap_base_at_key(self->fn_ids_ptr, fn_id).m_value)->arg_type_infos.m_data[i - 1];
            if (out_init_list_type_info->m_dimensions == 0)
                return syntax_error(init_list_node, "Passing initializer list to function where a non-list type was expected");
            break;
        }
        case AST_NODE_TYPE_DECL_VAR:
            (void)ast_node_to_type_info(parent->m_sub_nodes.m_data[1], out_init_list_type_info);
            if (out_init_list_type_info->m_dimensions == 0)
                return syntax_error(init_list_node, "Initializing non-list type with initializer list");
            break;
        case AST_NODE_TYPE_STATEMENT_RETURN:{
            const AST_node *fn_node = ast_node_find_fn_node(parent->m_parent);
            if (!fn_node)
                return syntax_error(init_list_node, "Returning an initializer list is only allowed inside a user-defined function");
            *out_init_list_type_info = ((Fn_id_info*)ordered_umap_base_at_key(self->fn_ids_ptr, &fn_node->m_sub_nodes.m_data[0]->m_token->m_id).m_value)->return_type_info;
            if (out_init_list_type_info->m_dimensions == 0)
                return syntax_error(init_list_node, "Returning an initializer list from function where a non-list type was expected");
            break;
        }
        default:{
            const AST_node *obj_init_node = parent->m_parent;
            if (obj_init_node && obj_init_node->m_type == AST_NODE_TYPE_ATOM_OBJ_INIT){
                *out_init_list_type_info = (
                    (Type_id_info_field_info*)ordered_umap_base_at_key(
                        &((Type_id_info*)ordered_umap_base_at_key(&self->type_id_info_maps_ptr->str_id_map, &obj_init_node->m_token->m_id).m_value)->field_infos,
                        &parent->m_token->m_id
                    ).m_value
                )->field_type_info;
                if (out_init_list_type_info->m_dimensions == 0)
                    return syntax_error(init_list_node, "Initializing field in object initializer where a non-list type was expected");
                break;
            }
        }
        init_list_context_error:
            return syntax_error(init_list_node, "Initializer list's type is contextually unknown");
    }

    return NO_ERROR;
}
#define init_list_type_info_from_context(init_list_node, out_init_list_type_info) \
    IR_compiler_state_init_list_type_info_from_context(self, (init_list_node), (out_init_list_type_info))

#define JMP_LABEL_SYMBOL "L"
#define JMP_LABEL_FMT LOCAL_LABEL_PREFIX_SYMBOL JMP_LABEL_SYMBOL USIZE_PFMT
#define JMP_LABEL_BUFSIZE array_size(LOCAL_LABEL_PREFIX_SYMBOL JMP_LABEL_SYMBOL "18446744073709551615")

static IR_compiler_state_compile_result IR_compiler_state_compile(IR_compiler_state *self, const AST_node *ast_node){
    enum Unary_op un_op;
    enum Binary_op bin_op;
    enum Op_code bin_op_code;

    switch (ast_node->m_type){
        case AST_NODE_TYPE_ATOM_ID:{
            Var_id_info *var_id_info_ptr = ordered_umap_base_at_key(&self->var_ids, &ast_node->m_token->m_id).m_value;
            if (!var_id_info_ptr)
                return syntax_error(ast_node, "Use of undeclared identifier <%s>", str_base_data_const(&ast_node->m_token->m_id));
            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &var_id_info_ptr->type_info))
                return OOM_ERROR;
            if (var_id_info_ptr->is_global){
                if (!add_instruction("%s " BP_SYMBOL "[" USIZE_PFMT "]", op_code_to_str(OP_CODE_PUSH), var_id_info_ptr->stack_idx))
                    return OOM_ERROR;
            }
            else if (!add_instruction("%s " SP_SYMBOL "[-" USIZE_PFMT "]", op_code_to_str(OP_CODE_PUSH), self->type_info_stack.m_size - var_id_info_ptr->stack_idx - 1))
                return OOM_ERROR;
            if (!pop_on_discarded_expression(ast_node))
                return OOM_ERROR;
            break;
        }
        case AST_NODE_TYPE_ATOM_FALSE:
        case AST_NODE_TYPE_ATOM_TRUE:
            if (
                !vec_base_push_back(&self->type_info_stack, self->alloc, &(Type_info){.m_tag = TYPE_INFO_TAG_BOOL, .m_dimensions = 0}) ||
                !add_instruction("%s %s", op_code_to_str(OP_CODE_PUSH), str_base_data_const(&ast_node->m_token->m_id)) ||
                !pop_on_discarded_expression(ast_node)
            )
                return OOM_ERROR;
            break;
        case AST_NODE_TYPE_ATOM_CHAR_LIT:
        case AST_NODE_TYPE_ATOM_INT_LIT:
        case AST_NODE_TYPE_ATOM_FLOAT_LIT:
        case AST_NODE_TYPE_ATOM_STR_LIT:
            if (
                !vec_base_push_back(
                    &self->type_info_stack,
                    self->alloc,
                    &(Type_info){.m_tag = (enum Type_info_tag)(TYPE_INFO_TAG_CHAR + (ast_node->m_type - AST_NODE_TYPE_ATOM_CHAR_LIT)), .m_dimensions = 0}
                ) ||
                !add_instruction("%s %s", op_code_to_str(OP_CODE_PUSH), str_base_data_const(&ast_node->m_token->m_id)) ||
                !pop_on_discarded_expression(ast_node)
            )
                return OOM_ERROR;
            break;
        case AST_NODE_TYPE_ATOM_OBJ_INIT:{
            Type_id_info *type_id_info_ptr = ordered_umap_base_at_key(&self->type_id_info_maps_ptr->str_id_map, &ast_node->m_token->m_id).m_value;
            if (!type_id_info_ptr)
                return syntax_error(ast_node, "Use of undeclared type identifier <%s>", str_base_data_const(&ast_node->m_token->m_id));

            if (ast_node->m_sub_nodes.m_size != type_id_info_ptr->field_infos.m_keys.m_size)
                return syntax_error(ast_node, "Must initialize every field exactly once");
            
            Type_info obj_type_info = {.m_tag = type_id_info_ptr->type_info_tag_id, .m_dimensions = 0};
            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &obj_type_info) || !add_instruction("%s []", op_code_to_str(OP_CODE_PUSH)))
                return OOM_ERROR;

            const char *push_back_str = builtin_fn_tag_to_str(BUILTIN_FN_TAG_PUSH_BACK);

            for (usize i = 0; i < ast_node->m_sub_nodes.m_size; ++i){
                const AST_node *field_id_node = ast_node->m_sub_nodes.m_data[i];
                const AST_node *field_expr_node = field_id_node->m_sub_nodes.m_data[0];

                Type_id_info_field_info *field_info_ptr = ordered_umap_base_at_key(&type_id_info_ptr->field_infos, &field_id_node->m_token->m_id).m_value;
                if (!field_info_ptr){
                    return syntax_error(
                        field_id_node,
                        "<%s> doesn't contain a field with the name <%s>",
                        str_base_data_const(&type_id_info_ptr->str_id),
                        str_base_data_const(&field_id_node->m_token->m_id)
                    );
                }

                if (i != field_info_ptr->field_idx)
                    return syntax_error(field_id_node, "Fields must be initialized in declaration order");

                Type_info field_type_info = field_info_ptr->field_type_info;
                if (!vec_base_push_back(&self->type_info_stack, self->alloc, &field_type_info) || !add_instruction("%s " SP_SYMBOL "[-1]", op_code_to_str(OP_CODE_PUSH)))
                    return OOM_ERROR;
                
                IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, field_expr_node);
                if (compile_result.error != COMPILE_ERROR_NONE)
                    return compile_result;

                Type_info last_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, field_type_info, last_type_info).m_tag == TYPE_INFO_TAG_NONE)
                    return binary_op_error(field_expr_node, field_type_info, last_type_info);

                if (!add_type_conversion_instruction(field_type_info))
                    return OOM_ERROR;

                vec_base_pop_back_discard(&self->type_info_stack);
                vec_base_pop_back_discard(&self->type_info_stack);
                if (!add_instruction("%s %s", op_code_to_str(OP_CODE_CALL), push_back_str))
                    return OOM_ERROR;
            }

            if (!pop_on_discarded_expression(ast_node))
                return OOM_ERROR;
            break;
        }
        case AST_NODE_TYPE_ATOM_INIT_LIST:{
            Type_info init_list_type_info;
            IR_compiler_state_compile_result compile_result = init_list_type_info_from_context(ast_node, &init_list_type_info);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            if (!vec_base_push_back(&self->type_info_stack, self->alloc, &init_list_type_info) || !add_instruction("%s []", op_code_to_str(OP_CODE_PUSH)))
                return OOM_ERROR;

            const char *push_back_str = builtin_fn_tag_to_str(BUILTIN_FN_TAG_PUSH_BACK);

            for (usize i = 0; i < ast_node->m_sub_nodes.m_size; ++i){
                if (!vec_base_push_back(&self->type_info_stack, self->alloc, &init_list_type_info) || !add_instruction("%s " SP_SYMBOL "[-1]", op_code_to_str(OP_CODE_PUSH)))
                    return OOM_ERROR;

                compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[i]);
                if (compile_result.error != COMPILE_ERROR_NONE)
                    return compile_result;

                if (
                    !builtin_fn_tag_call(
                        BUILTIN_FN_TAG_PUSH_BACK,
                        (Type_info_slice){.m_size = 2, .m_data = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 2)}
                    ).m_is_callable
                ){
                    --init_list_type_info.m_dimensions;
                    Str_base_result type_info_str = type_info_to_str_base(init_list_type_info, self->alloc);
                    return (type_info_str.success)
                        ? syntax_error(ast_node, "Initializer list must only contain elements of type <%s>", str_base_data(&type_info_str.result))
                        : OOM_ERROR
                    ;
                }

                if (!add_type_conversion_instruction(*(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1)))
                    return OOM_ERROR;

                vec_base_pop_back_discard(&self->type_info_stack);
                vec_base_pop_back_discard(&self->type_info_stack);
                if (!add_instruction("%s %s", op_code_to_str(OP_CODE_CALL), push_back_str))
                    return OOM_ERROR;
            }
            break;
        }

        case AST_NODE_TYPE_UNARY_OP_PLUS:  un_op = UNARY_OP_PLUS;  goto un_op_case;
        case AST_NODE_TYPE_UNARY_OP_MINUS: un_op = UNARY_OP_MINUS; goto un_op_case;
        case AST_NODE_TYPE_UNARY_OP_BNEG:  un_op = UNARY_OP_BNEG;  goto un_op_case;
        case AST_NODE_TYPE_UNARY_OP_NOT:   un_op = UNARY_OP_NOT;
        un_op_case:{
            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[0]);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info *last_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

            Type_info un_op_result = unary_op_type_info_result(un_op, *last_type_info_ptr);
            if (un_op_result.m_tag == TYPE_INFO_TAG_NONE)
                return unary_op_error(ast_node, *last_type_info_ptr);

            *last_type_info_ptr = un_op_result;

            if (un_op == UNARY_OP_BNEG){
                if (!add_instruction("%s", op_code_to_str(OP_CODE_BNEG)))
                    return OOM_ERROR;
            }
            else if (
                un_op != UNARY_OP_PLUS && (
                    (un_op == UNARY_OP_NOT && !add_instruction("%s", op_code_to_str(OP_CODE_TO_BOOL))) ||
                    !add_instruction("%s", op_code_to_str(OP_CODE_NEG))
                )
            )
                return OOM_ERROR;

            if (!pop_on_discarded_expression(ast_node))
                return OOM_ERROR;
            break;
        }

        case AST_NODE_TYPE_BINARY_OP_MEMBER_ACCESS:
            fprintf(stderr, __FILE__ ":" tok_to_str(__LINE__) ": Not implemented\n");
            abort();

        case AST_NODE_TYPE_BINARY_OP_SUBSCRIPT: bin_op = BINARY_OP_SUBSCRIPT; bin_op_code = OP_CODE_DEREF;   goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_POW:       bin_op = BINARY_OP_POW;       bin_op_code = OP_CODE_POW;     goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_MUL:       bin_op = BINARY_OP_MUL;       bin_op_code = OP_CODE_MUL;     goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_DIV:       bin_op = BINARY_OP_DIV;       bin_op_code = OP_CODE_DIV;     goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_REM:       bin_op = BINARY_OP_REM;       bin_op_code = OP_CODE_REM;     goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_ADD:       bin_op = BINARY_OP_ADD;       bin_op_code = OP_CODE_ADD;     goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_SUB:       bin_op = BINARY_OP_SUB;       bin_op_code = OP_CODE_SUB;     goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_SHL:       bin_op = BINARY_OP_SHL;       bin_op_code = OP_CODE_SHL;     goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_SHR:       bin_op = BINARY_OP_SHR;       bin_op_code = OP_CODE_SHR;     goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_CMP_LE:    bin_op = BINARY_OP_CMP_LE;    bin_op_code = OP_CODE_CMP_LE;  goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_CMP_LEQ:   bin_op = BINARY_OP_CMP_LEQ;   bin_op_code = OP_CODE_CMP_LEQ; goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_CMP_GE:    bin_op = BINARY_OP_CMP_GE;    bin_op_code = OP_CODE_CMP_GE;  goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_CMP_GEQ:   bin_op = BINARY_OP_CMP_GEQ;   bin_op_code = OP_CODE_CMP_GEQ; goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_CMP_EQ:    bin_op = BINARY_OP_CMP_EQ;    bin_op_code = OP_CODE_CMP_EQ;  goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_CMP_NEQ:   bin_op = BINARY_OP_CMP_NEQ;   bin_op_code = OP_CODE_CMP_NEQ; goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_BAND:      bin_op = BINARY_OP_BAND;      bin_op_code = OP_CODE_BAND;    goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_XOR:       bin_op = BINARY_OP_XOR;       bin_op_code = OP_CODE_XOR;     goto bin_op_case;
        case AST_NODE_TYPE_BINARY_OP_BOR:       bin_op = BINARY_OP_BOR;       bin_op_code = OP_CODE_BOR;
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
                return binary_op_error(ast_node, *lhs_type_info_ptr, *rhs_type_info_ptr);

            *lhs_type_info_ptr = bin_op_result;

            vec_base_pop_back_discard(&self->type_info_stack);
            if (!add_instruction("%s", op_code_to_str(bin_op_code)) || !pop_on_discarded_expression(ast_node))
                return OOM_ERROR;
            break;
        }

        case AST_NODE_TYPE_BINARY_OP_AS:{
            const AST_node *lhs_node = ast_node->m_sub_nodes.m_data[0];
            const AST_node *rhs_node = ast_node->m_sub_nodes.m_data[1];

            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, lhs_node);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info *last_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

            Type_info as_type_info;
            const AST_node *type_id_node = ast_node_to_type_info(rhs_node, &as_type_info);
            if (type_id_node)
                return undeclared_type_id_error(type_id_node);

            if (binary_op_type_info_result(BINARY_OP_AS, *last_type_info_ptr, as_type_info).m_tag == TYPE_INFO_TAG_NONE)
                return binary_op_error(ast_node, *last_type_info_ptr, as_type_info);

            *last_type_info_ptr = as_type_info;
            if (!add_type_conversion_instruction(as_type_info) || !pop_on_discarded_expression(ast_node))
                return OOM_ERROR;
            break;
        }

        case AST_NODE_TYPE_BINARY_OP_AND:
        case AST_NODE_TYPE_BINARY_OP_OR:{
            const AST_node *lhs_node = ast_node->m_sub_nodes.m_data[0];
            const AST_node *rhs_node = ast_node->m_sub_nodes.m_data[1];

            char and_or_label_str_buf[JMP_LABEL_BUFSIZE];
            sprintf(and_or_label_str_buf, JMP_LABEL_FMT, (*self->label_counter_ptr)++);

            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, lhs_node);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info lhs_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);
            if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, TYPE_INFO_BOOL, lhs_type_info).m_tag == TYPE_INFO_TAG_NONE)
                return type_conversion_error(ast_node, TYPE_INFO_BOOL, lhs_type_info);

            if (
                !add_instruction("%s", op_code_to_str(OP_CODE_TO_BOOL)) ||
                !vec_base_push_back(&self->type_info_stack, self->alloc, &lhs_type_info) ||
                !add_instruction("%s " SP_SYMBOL "[-1]", op_code_to_str(OP_CODE_PUSH)) || (
                    ast_node->m_type == AST_NODE_TYPE_BINARY_OP_OR &&
                    !add_instruction("%s", op_code_to_str(OP_CODE_NEG))
                )
            )
                return OOM_ERROR;
            vec_base_pop_back_discard(&self->type_info_stack);
            if (!add_instruction("%s %s", op_code_to_str(OP_CODE_JMPZ), and_or_label_str_buf))
                return OOM_ERROR;

            vec_base_pop_back_discard(&self->type_info_stack);
            if (!add_instruction("%s 1", op_code_to_str(OP_CODE_POP)))
                return OOM_ERROR;

            compile_result = IR_compiler_state_compile(self, rhs_node);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info rhs_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);
            if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, TYPE_INFO_BOOL, rhs_type_info).m_tag == TYPE_INFO_TAG_NONE)
                return type_conversion_error(ast_node, TYPE_INFO_BOOL, rhs_type_info);

            if (!add_instruction("%s", op_code_to_str(OP_CODE_TO_BOOL)))
                return OOM_ERROR;

            if (binary_op_type_info_result(BINARY_OP_AND, lhs_type_info, rhs_type_info).m_tag == TYPE_INFO_TAG_NONE)
                return binary_op_error(ast_node, lhs_type_info, rhs_type_info);

            if (!str_base_append_fmt(&self->IR, self->alloc, "%s:\n", and_or_label_str_buf) || !pop_on_discarded_expression(ast_node))
                return OOM_ERROR;
            break;
        }

        case AST_NODE_TYPE_BINARY_OP_ASSIGNMENT:{
            const AST_node *lhs_node = ast_node->m_sub_nodes.m_data[0];
            const AST_node *rhs_node = ast_node->m_sub_nodes.m_data[1];

            bool push_back_after_assignment = false;
            if (ast_node->m_parent){
                const AST_node *parent = ast_node->m_parent;
                enum AST_node_type parent_token_type = parent->m_type;
                switch (parent_token_type){
                    // TODO: for objs
                    case AST_NODE_TYPE_STATEMENT_IF:
                    case AST_NODE_TYPE_STATEMENT_WHILE:
                        if (parent->m_sub_nodes.m_data[parent_token_type == AST_NODE_TYPE_STATEMENT_WHILE] != ast_node)
                            break;
                        FALLTHROUGH;
                    case AST_NODE_TYPE_ATOM_OBJ_INIT:
                    case AST_NODE_TYPE_ATOM_INIT_LIST:
                    case AST_NODE_TYPE_UNARY_OP_PLUS:
                    case AST_NODE_TYPE_UNARY_OP_MINUS:
                    case AST_NODE_TYPE_UNARY_OP_BNEG:
                    case AST_NODE_TYPE_UNARY_OP_NOT:
                    case AST_NODE_TYPE_BINARY_OP_MEMBER_ACCESS:
                    case AST_NODE_TYPE_BINARY_OP_SUBSCRIPT:
                    case AST_NODE_TYPE_BINARY_OP_POW:
                    case AST_NODE_TYPE_BINARY_OP_AS:
                    case AST_NODE_TYPE_BINARY_OP_MUL:
                    case AST_NODE_TYPE_BINARY_OP_DIV:
                    case AST_NODE_TYPE_BINARY_OP_REM:
                    case AST_NODE_TYPE_BINARY_OP_ADD:
                    case AST_NODE_TYPE_BINARY_OP_SUB:
                    case AST_NODE_TYPE_BINARY_OP_SHL:
                    case AST_NODE_TYPE_BINARY_OP_SHR:
                    case AST_NODE_TYPE_BINARY_OP_CMP_LE:
                    case AST_NODE_TYPE_BINARY_OP_CMP_LEQ:
                    case AST_NODE_TYPE_BINARY_OP_CMP_GE:
                    case AST_NODE_TYPE_BINARY_OP_CMP_GEQ:
                    case AST_NODE_TYPE_BINARY_OP_CMP_EQ:
                    case AST_NODE_TYPE_BINARY_OP_CMP_NEQ:
                    case AST_NODE_TYPE_BINARY_OP_BAND:
                    case AST_NODE_TYPE_BINARY_OP_XOR:
                    case AST_NODE_TYPE_BINARY_OP_BOR:
                    case AST_NODE_TYPE_BINARY_OP_AND:
                    case AST_NODE_TYPE_BINARY_OP_OR:
                    case AST_NODE_TYPE_BINARY_OP_ASSIGNMENT:
                    case AST_NODE_TYPE_FN_CALL:
                    case AST_NODE_TYPE_DECL_VAR:
                    case AST_NODE_TYPE_STATEMENT_RETURN:
                        push_back_after_assignment = true;
                        break;
                    default:
                        break;
                }
            }

            if (lhs_node->m_type != AST_NODE_TYPE_BINARY_OP_SUBSCRIPT){
                if (lhs_node->m_type != AST_NODE_TYPE_ATOM_ID)
                    return syntax_error(lhs_node, "Trying to assign to rvalue");

                Var_id_info *var_id_info_ptr = ordered_umap_base_at_key(&self->var_ids, &lhs_node->m_token->m_id).m_value;
                if (!var_id_info_ptr)
                    return syntax_error(lhs_node, "Use of undeclared identifier <%s>", str_base_data_const(&lhs_node->m_token->m_id));

                IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, rhs_node);
                if (compile_result.error != COMPILE_ERROR_NONE)
                    return compile_result;

                Type_info lhs_type_info = var_id_info_ptr->type_info;
                Type_info rhs_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, lhs_type_info, rhs_type_info).m_tag == TYPE_INFO_TAG_NONE)
                    return binary_op_error(ast_node, lhs_type_info, rhs_type_info);

                vec_base_pop_back_discard(&self->type_info_stack);
                if (var_id_info_ptr->is_global){
                    if (!add_instruction("%s " BP_SYMBOL "[" USIZE_PFMT "]", op_code_to_str(OP_CODE_MOV), var_id_info_ptr->stack_idx))
                        return OOM_ERROR;
                }
                else if (!add_instruction("%s " SP_SYMBOL "[-" USIZE_PFMT "]", op_code_to_str(OP_CODE_MOV), self->type_info_stack.m_size - var_id_info_ptr->stack_idx + 1))
                    return OOM_ERROR;

                if (push_back_after_assignment && (compile_result = IR_compiler_state_compile(self, lhs_node)).error != COMPILE_ERROR_NONE)
                    return compile_result;
            }
            else{
                for (
                    const AST_node *lhs_sub_node = lhs_node->m_sub_nodes.m_data[0];
                    lhs_sub_node->m_type != AST_NODE_TYPE_ATOM_ID;
                    lhs_sub_node = lhs_sub_node->m_sub_nodes.m_data[0]
                ){
                    enum AST_node_type ast_node_type = lhs_sub_node->m_type;
                    if (ast_node_type != AST_NODE_TYPE_BINARY_OP_ASSIGNMENT && ast_node_type != AST_NODE_TYPE_BINARY_OP_SUBSCRIPT && ast_node_type != AST_NODE_TYPE_BINARY_OP_AS)
                        return syntax_error(lhs_node, "Trying to assign to rvalue");
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
                    return binary_op_error(lhs_node, subscript_lhs_type_info, subscript_rhs_type_info);

                if (
                    push_back_after_assignment && (
                        !vec_base_push_back(&self->type_info_stack, self->alloc, &subscript_lhs_type_info) ||
                        !add_instruction("%s " SP_SYMBOL "[-2]", op_code_to_str(OP_CODE_PUSH)) ||
                        !vec_base_push_back(&self->type_info_stack, self->alloc, &subscript_rhs_type_info) ||
                        !add_instruction("%s " SP_SYMBOL "[-2]", op_code_to_str(OP_CODE_PUSH))
                    )
                )
                    return OOM_ERROR;

                compile_result = IR_compiler_state_compile(self, rhs_node);
                if (compile_result.error != COMPILE_ERROR_NONE)
                    return compile_result;

                Type_info rhs_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, subscript_bin_op_result, rhs_type_info).m_tag == TYPE_INFO_TAG_NONE)
                    return binary_op_error(ast_node, subscript_bin_op_result, rhs_type_info);

                vec_base_pop_back_discard(&self->type_info_stack);
                vec_base_pop_back_discard(&self->type_info_stack);
                vec_base_pop_back_discard(&self->type_info_stack);
                if (!add_instruction("%s", op_code_to_str(OP_CODE_MOV_DEREF)))
                    return OOM_ERROR;

                if (push_back_after_assignment){
                    vec_base_pop_back_discard(&self->type_info_stack);
                    *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1) = subscript_bin_op_result;
                    if (!add_instruction("%s", op_code_to_str(OP_CODE_DEREF)))
                        return OOM_ERROR;
                }
            }
            break;
        }

        case AST_NODE_TYPE_FN_CALL:{
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
                        if (!add_type_conversion_instruction(*(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1)))
                            return OOM_ERROR;
                    }

                    Type_info_slice arg_type_infos = {
                        .m_size = fn_arg_nodes.m_size,
                        .m_data = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - fn_arg_nodes.m_size)
                    };
                    bfn_call_result = builtin_fn_tag_call(bfn_tag, arg_type_infos);
                    if (!bfn_call_result.m_is_callable){
                        Str_base_result type_info_list_str = type_info_slice_to_str_base(arg_type_infos, self->alloc);
                        return (type_info_list_str.success)
                            ? syntax_error(ast_node, "Builtin function <%s> is not callable with types <%s>", fn_id, str_base_data(&type_info_list_str.result))
                            : OOM_ERROR
                        ;
                    }
                }
                else if (!(bfn_call_result = builtin_fn_tag_call(bfn_tag, (Type_info_slice){0})).m_is_callable)
                    return syntax_error(ast_node, "Builtin function <%s> is not callable without arguments", fn_id);

                return_type_info = bfn_call_result.m_return_type_info;
            }
            else{
                Fn_id_info *fn_id_info_ptr = ordered_umap_base_at_key(self->fn_ids_ptr, &ast_node->m_sub_nodes.m_data[0]->m_token->m_id).m_value;
                if (!fn_id_info_ptr)
                    return syntax_error(ast_node, "Use of undeclared function <%s>", fn_id);

                fn_id_mangled = str_base_data_const(&fn_id_info_ptr->id_mangled);

                for (usize i = 0; i < fn_arg_nodes.m_size; ++i){
                    if (i >= fn_id_info_ptr->arg_type_infos.m_size)
                        return syntax_error(ast_node, "Function <%s> called with wrong number of arguments", fn_id);

                    IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, fn_arg_nodes.m_data[i]);
                    if (compile_result.error != COMPILE_ERROR_NONE)
                        return compile_result;

                    Type_info  arg_type_info = fn_id_info_ptr->arg_type_infos.m_data[i];
                    Type_info last_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                    if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, arg_type_info, last_type_info).m_tag == TYPE_INFO_TAG_NONE)
                        return type_conversion_error(fn_arg_nodes.m_data[i], arg_type_info, last_type_info);

                    if (!add_type_conversion_instruction(arg_type_info))
                        return OOM_ERROR;
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
                enum AST_node_type parent_token_type = parent->m_type;
                switch (parent_token_type){
                    case AST_NODE_TYPE_STATEMENT_BLOCK:
                        break;
                    case AST_NODE_TYPE_STATEMENT_IF:
                    case AST_NODE_TYPE_STATEMENT_WHILE:
                        if (parent->m_sub_nodes.m_data[parent_token_type == AST_NODE_TYPE_STATEMENT_WHILE] != ast_node)
                            break;
                        FALLTHROUGH;
                    default:
                        return syntax_error(ast_node, "Function <%s> returning type <void> is used in an expression", fn_id);
                }
            }

            if (!add_instruction("%s %s", op_code_to_str(OP_CODE_CALL), fn_id_mangled) || (return_type_info.m_tag != TYPE_INFO_TAG_VOID && !pop_on_discarded_expression(ast_node)))
                return OOM_ERROR;
            break;
        }

        case AST_NODE_TYPE_DECL_FN:{
            const AST_node *fn_id_node          = ast_node->m_sub_nodes.m_data[0];
            AST_node_ptr_slice fn_arg_nodes     = {.m_size = ast_node->m_sub_nodes.m_size - 3, .m_data = &ast_node->m_sub_nodes.m_data[1]};
            const AST_node *fn_return_type_node = ast_node->m_sub_nodes.m_data[ast_node->m_sub_nodes.m_size - 2];
            const AST_node *fn_body_node        = ast_node->m_sub_nodes.m_data[ast_node->m_sub_nodes.m_size - 1];

            const char *fn_id = str_base_data_const(&fn_id_node->m_token->m_id);

            Str_base id_mangled = {0};
            if (ast_node->m_parent){
                if (!str_base_assign_fmt(&id_mangled, self->alloc, LOCAL_LABEL_PREFIX_SYMBOL "%s" USIZE_PFMT, fn_id, (*self->label_counter_ptr)++))
                    return OOM_ERROR;
            }
            else if (!str_base_assign_raw(&id_mangled, self->alloc, fn_id))
                return OOM_ERROR;

            Vec_base fn_arg_type_infos = vec_base_init(Type_info);
            if (!vec_base_reserve(&fn_arg_type_infos, self->alloc, fn_arg_nodes.m_size))
                return OOM_ERROR;

            Type_info return_type_info;
            const AST_node *type_id_node = ast_node_to_type_info(fn_return_type_node, &return_type_info);
            if (type_id_node)
                return undeclared_type_id_error(type_id_node);

            Fn_id_info fn_id_info = {
                .id_mangled       = id_mangled,
                .arg_type_infos   = {.m_size = fn_arg_nodes.m_size, .m_data = fn_arg_type_infos.m_data},
                .return_type_info = return_type_info
            };

            switch (ordered_umap_base_push_back(self->fn_ids_ptr, self->alloc, &fn_id_node->m_token->m_id, &fn_id_info).error){
                case UMAP_INSERT_ERROR_NONE:
                    if (builtin_fn_tag_init(fn_id) != BUILTIN_FN_TAG_NONE){
                case UMAP_INSERT_ERROR_ALREADY_INSERTED:
                        return syntax_error(fn_id_node, "Function identifier <%s> is already in use", fn_id);
                    }
                    break;
                case UMAP_INSERT_ERROR_OOM:
                    return OOM_ERROR;
            }

            ++((Id_count*)vec_base_at(&self->id_count_stack, self->id_count_stack.m_size - 1))->fn_id_count;

            IR_compiler_state fn_IR_compiler_state;

            if (
                !IR_compiler_state_init_in_place(&fn_IR_compiler_state, self->alloc, self->fn_ids_ptr, self->type_id_info_maps_ptr, self->label_counter_ptr) ||
                !str_base_append_fmt(&fn_IR_compiler_state.IR, fn_IR_compiler_state.alloc, "%s:\n", str_base_data_const(&fn_id_info.id_mangled))
            )
                return OOM_ERROR;

            usize *fn_IR_compiler_state_global_var_id_count_ptr = &((Id_count*)vec_base_at(&fn_IR_compiler_state.id_count_stack, 0))->var_id_count;
            for (usize i = 0, global_var_id_count = ((Id_count*)vec_base_at(&self->id_count_stack, 0))->var_id_count; i < global_var_id_count; ++i){
                Umap_pair pair = ordered_umap_base_at_idx(&self->var_ids, i);
                if (
                    !vec_base_push_back(&fn_IR_compiler_state.type_info_stack, fn_IR_compiler_state.alloc, &((Var_id_info*)pair.m_value)->type_info) ||
                    ordered_umap_base_push_back(&fn_IR_compiler_state.var_ids, fn_IR_compiler_state.alloc, pair.m_key, pair.m_value).error != UMAP_INSERT_ERROR_NONE
                )
                    return OOM_ERROR;
                ++*fn_IR_compiler_state_global_var_id_count_ptr;
            }
            if (!vec_base_push_back(&fn_IR_compiler_state.id_count_stack, fn_IR_compiler_state.alloc, &(Id_count){0}))
                return OOM_ERROR;

            for (usize i = 0; i < fn_arg_nodes.m_size; ++i){
                const AST_node *arg_id_node = fn_arg_nodes.m_data[i];

                Type_info arg_type_info;
                type_id_node = ast_node_to_type_info(arg_id_node->m_sub_nodes.m_data[0], &arg_type_info);
                if (type_id_node)
                    return undeclared_type_id_error(type_id_node);

                if (!vec_base_push_back(&fn_IR_compiler_state.type_info_stack, fn_IR_compiler_state.alloc, &arg_type_info))
                    return OOM_ERROR;

                IR_compiler_state_compile_result push_back_var_id_result = IR_compiler_state_push_back_var_id(&fn_IR_compiler_state, arg_id_node, arg_type_info);
                if (push_back_var_id_result.error != COMPILE_ERROR_NONE)
                    return push_back_var_id_result;

                (void)vec_base_push_back(&fn_arg_type_infos, self->alloc, &arg_type_info);
            }

            assert(fn_arg_type_infos.m_size == fn_id_info.arg_type_infos.m_size);
            assert(fn_arg_type_infos.m_size == fn_arg_type_infos.m_capacity);

            const AST_node *fn_body_last_node = fn_body_node;
            if (fn_id_info.return_type_info.m_tag != TYPE_INFO_TAG_VOID){
                if (
                    fn_body_node->m_sub_nodes.m_size == 0 ||
                    (fn_body_last_node = fn_body_node->m_sub_nodes.m_data[fn_body_node->m_sub_nodes.m_size - 1])->m_type != AST_NODE_TYPE_STATEMENT_RETURN ||
                    fn_body_last_node->m_sub_nodes.m_size == 0
                )
                    return syntax_error(fn_body_last_node, "Function returning non-void must end with a <return> statement that contains an expression");
            }
            else if (
                fn_body_node->m_sub_nodes.m_size > 0 &&
                (fn_body_last_node = fn_body_node->m_sub_nodes.m_data[fn_body_node->m_sub_nodes.m_size - 1])->m_type == AST_NODE_TYPE_STATEMENT_RETURN &&
                fn_body_last_node->m_sub_nodes.m_size > 0
            )
                return syntax_error(fn_body_last_node, "Function with return type <void> returning non-void");

            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(&fn_IR_compiler_state, fn_body_node);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            if (
                fn_id_info.return_type_info.m_tag == TYPE_INFO_TAG_VOID && (
                    fn_body_node->m_sub_nodes.m_size == 0 ||
                    fn_body_node->m_sub_nodes.m_data[fn_body_node->m_sub_nodes.m_size - 1]->m_type != AST_NODE_TYPE_STATEMENT_RETURN
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
            break;
        }

        case AST_NODE_TYPE_DECL_VAR:{
            const AST_node *id_node   = ast_node->m_sub_nodes.m_data[0];
            const AST_node *type_node = ast_node->m_sub_nodes.m_data[1];
            const AST_node *expr_node = ast_node->m_sub_nodes.m_data[2];

            Type_info id_type_info;
            const AST_node *type_id_node = ast_node_to_type_info(type_node, &id_type_info);
            if (type_id_node)
                return undeclared_type_id_error(type_id_node);

            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, expr_node);
            if (compile_result.error != COMPILE_ERROR_NONE || (compile_result = push_back_var_id(id_node, id_type_info)).error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info *last_type_info_ptr = vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);
            if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, id_type_info, *last_type_info_ptr).m_tag == TYPE_INFO_TAG_NONE){
                Str_base_result type_str;
                Str_base_result expr_type_str;
                if (
                    !(type_str = type_info_to_str_base(id_type_info, self->alloc)).success ||
                    !(expr_type_str = type_info_to_str_base(*last_type_info_ptr, self->alloc)).success
                )
                    return OOM_ERROR;
                return syntax_error(
                    expr_node,
                    "Expression's type <%s> is incompatible with the type of the destination <%s>",
                    str_base_data(&expr_type_str.result),
                    str_base_data(&type_str.result)
                );
            }

            *last_type_info_ptr = id_type_info;

            if (!add_type_conversion_instruction(id_type_info))
                return OOM_ERROR;
            break;
        }

        case AST_NODE_TYPE_DECL_TYPE:{
            const AST_node *type_id_node = ast_node->m_sub_nodes.m_data[0];
            
            Umap_insert_result type_insert_result = ordered_umap_base_push_back(
                &self->type_id_info_maps_ptr->str_id_map,
                self->alloc,
                &type_id_node->m_token->m_id,
                &(Type_id_info){
                    .str_id           = type_id_node->m_token->m_id,
                    .type_info_tag_id = (enum Type_info_tag)self->type_id_info_maps_ptr->type_id_counter,
                    .field_infos      = ordered_umap_base_init(Str_base, Type_id_info_field_info)
                }
            );
            switch (type_insert_result.error){
                case UMAP_INSERT_ERROR_NONE:
                    break;
                case UMAP_INSERT_ERROR_OOM:
                    return OOM_ERROR;
                case UMAP_INSERT_ERROR_ALREADY_INSERTED:
                    return syntax_error(type_id_node, "Type identifier <%s> is already in use", str_base_data_const(&type_id_node->m_token->m_id));
            }
            if (
                ordered_umap_base_push_back(
                    &self->type_id_info_maps_ptr->type_info_tag_as_i32_id_map,
                    self->alloc,
                    &self->type_id_info_maps_ptr->type_id_counter,
                    type_insert_result.result.m_value
                ).error != UMAP_INSERT_ERROR_NONE
            )
                return OOM_ERROR;

            enum Type_info_tag type_id_type_info_tag = self->type_id_info_maps_ptr->type_id_counter++;

            for (usize i = 1; i < ast_node->m_sub_nodes.m_size; ++i){
                const AST_node *field_id_node = ast_node->m_sub_nodes.m_data[i];
                const AST_node *field_type_node = field_id_node->m_sub_nodes.m_data[0];

                Type_info field_type_info;
                const AST_node *field_type_id_node = ast_node_to_type_info(field_type_node, &field_type_info);
                if (field_type_id_node)
                    return undeclared_type_id_error(field_type_id_node);

                if (field_type_info.m_tag == type_id_type_info_tag && field_type_info.m_dimensions == 0)
                    return syntax_error(field_type_node, "Type containing itself directly");

                enum Umap_insert_error field_insert_error = ordered_umap_base_push_back(
                    &((Type_id_info*)type_insert_result.result.m_value)->field_infos,
                    self->alloc,
                    &field_id_node->m_token->m_id,
                    &(Type_id_info_field_info){.field_idx = i - 1, .field_type_info = field_type_info}
                ).error;
                switch (field_insert_error){
                    case UMAP_INSERT_ERROR_NONE:
                        break;
                    case UMAP_INSERT_ERROR_OOM:
                        return OOM_ERROR;
                    case UMAP_INSERT_ERROR_ALREADY_INSERTED:
                        return syntax_error(field_id_node, "Field identifier <%s> is already in use", str_base_data_const(&field_id_node->m_token->m_id));
                }
            }
            break;
        }

        case AST_NODE_TYPE_STATEMENT_BLOCK:
            if (!vec_base_push_back(&self->id_count_stack, self->alloc, &(Id_count){0}))
                return OOM_ERROR;
            for (usize i = 0; i < ast_node->m_sub_nodes.m_size; ++i){
                IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[i]);
                if (compile_result.error != COMPILE_ERROR_NONE)
                    return compile_result;
            }
            if (!pop_ids_in_current_scope())
                return OOM_ERROR;
            break;

        case AST_NODE_TYPE_STATEMENT_IF:{
            const AST_node *if_cond_node   = ast_node->m_sub_nodes.m_data[0];
            const AST_node *if_body_node   = ast_node->m_sub_nodes.m_data[1];
            const AST_node *else_body_node = ast_node->m_sub_nodes.m_data[2];

            char   if_end_label_str_buf[JMP_LABEL_BUFSIZE];
            char else_end_label_str_buf[JMP_LABEL_BUFSIZE];

            sprintf(if_end_label_str_buf, JMP_LABEL_FMT, (*self->label_counter_ptr)++);

            if (else_body_node)
                sprintf(else_end_label_str_buf, JMP_LABEL_FMT, (*self->label_counter_ptr)++);

            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, if_cond_node);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info last_type_info;
            vec_base_pop_back_to(&self->type_info_stack, &last_type_info);

            if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, TYPE_INFO_BOOL, last_type_info).m_tag == TYPE_INFO_TAG_NONE)
                return type_conversion_error(ast_node, TYPE_INFO_BOOL, last_type_info);

            if (!add_instruction("%s %s", op_code_to_str(OP_CODE_JMPZ), if_end_label_str_buf))
                return OOM_ERROR;

            if (if_body_node || else_body_node){
                if (if_body_node){
                    if (!vec_base_push_back(&self->id_count_stack, self->alloc, &(Id_count){0}))
                        return OOM_ERROR;
                    compile_result = IR_compiler_state_compile(self, if_body_node);
                    if (compile_result.error != COMPILE_ERROR_NONE)
                        return compile_result;
                    if (!pop_ids_in_current_scope() || (!else_body_node && !str_base_append_fmt(&self->IR, self->alloc, "%s:\n", if_end_label_str_buf)))
                        return OOM_ERROR;
                }
                if (else_body_node){
                    if (
                        !add_instruction("%s %s", op_code_to_str(OP_CODE_JMP), else_end_label_str_buf) || 
                        !str_base_append_fmt(&self->IR, self->alloc, "%s:\n", if_end_label_str_buf) ||
                        !vec_base_push_back(&self->id_count_stack, self->alloc, &(Id_count){0})
                    )
                        return OOM_ERROR;
                    compile_result = IR_compiler_state_compile(self, else_body_node);
                    if (compile_result.error != COMPILE_ERROR_NONE)
                        return compile_result;
                    if (!pop_ids_in_current_scope() || !str_base_append_fmt(&self->IR, self->alloc, "%s:\n", else_end_label_str_buf))
                        return OOM_ERROR;
                }
            }
            else if (!str_base_append_fmt(&self->IR, self->alloc, "%s:\n", if_end_label_str_buf))
                return OOM_ERROR;
            break;
        }

        case AST_NODE_TYPE_STATEMENT_WHILE:{
            const AST_node *while_label_node         = ast_node->m_sub_nodes.m_data[0];
            const AST_node *while_cond_node          = ast_node->m_sub_nodes.m_data[1];
            const AST_node *while_continue_expr_node = ast_node->m_sub_nodes.m_data[2];
            const AST_node *while_body_node          = ast_node->m_sub_nodes.m_data[3];

            char     cond_label_str_buf[JMP_LABEL_BUFSIZE];
            char    break_label_str_buf[JMP_LABEL_BUFSIZE];
            char continue_label_str_buf[JMP_LABEL_BUFSIZE];

            sprintf(    cond_label_str_buf, JMP_LABEL_FMT, (*self->label_counter_ptr)++);
            sprintf(   break_label_str_buf, JMP_LABEL_FMT, (*self->label_counter_ptr)++);
            sprintf(continue_label_str_buf, JMP_LABEL_FMT, (*self->label_counter_ptr)++);

            if (!str_base_append_fmt(&self->IR, self->alloc, "%s:\n", cond_label_str_buf))
                return OOM_ERROR;

            IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, while_cond_node);
            if (compile_result.error != COMPILE_ERROR_NONE)
                return compile_result;

            Type_info last_type_info;
            vec_base_pop_back_to(&self->type_info_stack, &last_type_info);

            if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, TYPE_INFO_BOOL, last_type_info).m_tag == TYPE_INFO_TAG_NONE)
                return type_conversion_error(ast_node, TYPE_INFO_BOOL, last_type_info);

            if (!add_instruction("%s %s", op_code_to_str(OP_CODE_JMPZ), break_label_str_buf))
                return OOM_ERROR;

            Str_base while_label_id_str = {0};
            if (while_label_node)
                while_label_id_str = while_label_node->m_token->m_id;
            else if (!str_base_assign_fmt(&while_label_id_str, self->alloc, USIZE_PFMT, *self->label_counter_ptr))
                return OOM_ERROR;
            enum Umap_insert_error insert_error = ordered_umap_base_push_back(
                &self->while_labels,
                self->alloc,
                &while_label_id_str,
                &(While_label_info){
                    .break_label_str    = break_label_str_buf,
                    .continue_label_str = continue_label_str_buf,
                    .id_count_stack_idx = self->id_count_stack.m_size
                }
            ).error;
            switch (insert_error){
                case UMAP_INSERT_ERROR_NONE:
                    break;
                case UMAP_INSERT_ERROR_OOM:
                    return OOM_ERROR;
                case UMAP_INSERT_ERROR_ALREADY_INSERTED:
                    return syntax_error(while_label_node, "Label identifier <%s> is already in use", str_base_data_const(&while_label_id_str));
            }
            if (while_body_node){
                if (!vec_base_push_back(&self->id_count_stack, self->alloc, &(Id_count){0}))
                    return OOM_ERROR;
                compile_result = IR_compiler_state_compile(self, while_body_node);
                if (compile_result.error != COMPILE_ERROR_NONE)
                    return compile_result;
                if (!pop_ids_in_current_scope())
                    return OOM_ERROR;
            }
            ordered_umap_base_pop_back_discard(&self->while_labels, self->alloc);

            if (!str_base_append_fmt(&self->IR, self->alloc, "%s:\n", continue_label_str_buf))
                return OOM_ERROR;
            if (while_continue_expr_node && (compile_result = IR_compiler_state_compile(self, while_continue_expr_node)).error != COMPILE_ERROR_NONE)
                return compile_result;

            if (!add_instruction("%s %s", op_code_to_str(OP_CODE_JMP), cond_label_str_buf) || !str_base_append_fmt(&self->IR, self->alloc, "%s:\n", break_label_str_buf))
                return OOM_ERROR;
            break;
        }

        case AST_NODE_TYPE_STATEMENT_BREAK:
        case AST_NODE_TYPE_STATEMENT_CONTINUE:{
            if (self->while_labels.m_keys.m_size == 0)
                return syntax_error(ast_node, "<%s> must be used inside a loop", str_base_data_const(&ast_node->m_token->m_id));
            While_label_info *while_label_info_ptr = ((ast_node->m_sub_nodes.m_size > 0)
                ? ordered_umap_base_at_key(&self->while_labels, &ast_node->m_sub_nodes.m_data[0]->m_token->m_id)
                : ordered_umap_base_at_idx(&self->while_labels, self->while_labels.m_keys.m_size - 1)
            ).m_value;
            if (!while_label_info_ptr){
                const AST_node *label_id_node = ast_node->m_sub_nodes.m_data[0];
                return syntax_error(label_id_node, "Use of undeclared identifier <%s>", str_base_data_const(&label_id_node->m_token->m_id));
            }
            usize type_info_stack_size = self->type_info_stack.m_size;
            for (usize i = self->id_count_stack.m_size; i-- > while_label_info_ptr->id_count_stack_idx;)
                for (usize var_id_count = ((Id_count*)vec_base_at(&self->id_count_stack, i))->var_id_count; var_id_count-- > 0;)
                    --self->type_info_stack.m_size;
            if (
                self->type_info_stack.m_size < type_info_stack_size &&
                !add_instruction("%s " USIZE_PFMT, op_code_to_str(OP_CODE_POP), type_info_stack_size - self->type_info_stack.m_size)
            )
                return OOM_ERROR;
            if (!add_instruction(
                "%s %s",
                op_code_to_str(OP_CODE_JMP),
                (ast_node->m_type == AST_NODE_TYPE_STATEMENT_BREAK) ? while_label_info_ptr->break_label_str : while_label_info_ptr->continue_label_str
            ))
                return OOM_ERROR;
            self->type_info_stack.m_size = type_info_stack_size;
            break;
        }
        case AST_NODE_TYPE_STATEMENT_RETURN:{
            const AST_node *fn_node = ast_node_find_fn_node(ast_node->m_parent);
            if (!fn_node){
                if (ast_node->m_sub_nodes.m_size != 1)
                    return syntax_error(ast_node, "The program must return a non-void value on exit");

                IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[0]);
                if (compile_result.error != COMPILE_ERROR_NONE)
                    return compile_result;

                vec_base_pop_back_discard(&self->type_info_stack);
                if (!add_instruction("%s %s", op_code_to_str(OP_CODE_CALL), builtin_fn_tag_to_str(BUILTIN_FN_TAG_EXIT)))
                    return OOM_ERROR;
            }
            else{
                enum Op_code ret_op_code = OP_CODE_RETV;

                Fn_id_info *fn_id_info_ptr = ordered_umap_base_at_key(self->fn_ids_ptr, &fn_node->m_sub_nodes.m_data[0]->m_token->m_id).m_value;
                if (fn_id_info_ptr->return_type_info.m_tag != TYPE_INFO_TAG_VOID){
                    if (ast_node->m_sub_nodes.m_size != 1)
                        return syntax_error(ast_node, "Function returning non-void must end with a <return> statement that contains an expression");

                    ret_op_code = OP_CODE_RET;

                    IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(self, ast_node->m_sub_nodes.m_data[0]);
                    if (compile_result.error != COMPILE_ERROR_NONE)
                        return compile_result;

                    Type_info last_type_info = *(Type_info*)vec_base_at(&self->type_info_stack, self->type_info_stack.m_size - 1);

                    if (binary_op_type_info_result(BINARY_OP_ASSIGNMENT, fn_id_info_ptr->return_type_info, last_type_info).m_tag == TYPE_INFO_TAG_NONE)
                        return type_conversion_error(ast_node, fn_id_info_ptr->return_type_info, last_type_info);

                    if (!add_type_conversion_instruction(fn_id_info_ptr->return_type_info))
                        return OOM_ERROR;

                    vec_base_pop_back_discard(&self->type_info_stack);
                }
                else if (ast_node->m_sub_nodes.m_size > 0)
                    return syntax_error(ast_node, "Function with return type <void> returning non-void");

                usize type_info_stack_size = self->type_info_stack.m_size;
                self->type_info_stack.m_size = ((Id_count*)vec_base_at(&self->id_count_stack, 0))->var_id_count;
                if (!add_instruction("%s " USIZE_PFMT, op_code_to_str(ret_op_code), type_info_stack_size - self->type_info_stack.m_size))
                    return OOM_ERROR;
                self->type_info_stack.m_size = type_info_stack_size;
            }
            break;
        }

        default:
            fprintf(stderr, __FILE__ ":" tok_to_str(__LINE__) ": Not implemented\n");
            abort();
    }

    return NO_ERROR;
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
        case OP_CODE_REM:       return "rem";
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
    Ordered_umap_base fn_ids = ordered_umap_base_init(Str_base, Fn_id_info);
    Type_id_info_maps type_id_info_maps = {
        .str_id_map                  = ordered_umap_base_init(Str_base, Type_id_info),
        .type_info_tag_as_i32_id_map = ordered_umap_base_init(i32, Type_id_info),
        .type_id_counter             = TYPE_INFO_TAG_STR + 1
    };
    usize label_counter = 0;

    IR_compiler_state state;
    if (!IR_compiler_state_init_in_place(&state, alloc, &fn_ids, &type_id_info_maps, &label_counter))
        goto oom_error;

    for (usize i = 0; i < ast_nodes.m_size; ++i){
        IR_compiler_state_compile_result compile_result = IR_compiler_state_compile(&state, ast_nodes.m_data[i]);
        if (compile_result.error != COMPILE_ERROR_NONE)
            return (IR_compile_result){.error_info = compile_result.error_info, .error = compile_result.error};
    }

    if (
        !IR_compiler_state_pop_ids_in_current_scope(&state) || (
            (ast_nodes.m_size == 0 || ast_nodes.m_data[ast_nodes.m_size - 1]->m_type != AST_NODE_TYPE_STATEMENT_RETURN) && (
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
