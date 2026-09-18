#ifndef LANG_PARSER_H
#define LANG_PARSER_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>

#include "../Allocator/Arena.h"
#include "../Data_structure/Str_base.h"
#include "../Utils/Num.h"

#include "Lexer.h"

typedef struct AST_node AST_node;

typedef struct AST_node_ptr_slice{
    usize m_size;
    const AST_node *const *m_data;
} AST_node_ptr_slice;

i64 ast_node_ptr_slice_print(AST_node_ptr_slice ast_node_ptr_slice, FILE *file);

enum AST_node_type{
    AST_NODE_TYPE_ATOM_ID,
    AST_NODE_TYPE_ATOM_FALSE,
    AST_NODE_TYPE_ATOM_TRUE,
    AST_NODE_TYPE_ATOM_CHAR_LIT,
    AST_NODE_TYPE_ATOM_INT_LIT,
    AST_NODE_TYPE_ATOM_FLOAT_LIT,
    AST_NODE_TYPE_ATOM_STR_LIT,
    AST_NODE_TYPE_ATOM_OBJ_INIT,
    AST_NODE_TYPE_ATOM_INIT_LIST,

    AST_NODE_TYPE_TYPE_ID,
    AST_NODE_TYPE_TYPE_VOID,
    AST_NODE_TYPE_TYPE_BOOL,
    AST_NODE_TYPE_TYPE_CHAR,
    AST_NODE_TYPE_TYPE_INT,
    AST_NODE_TYPE_TYPE_FLOAT,
    AST_NODE_TYPE_TYPE_STR,
    AST_NODE_TYPE_TYPE_LIST,

    AST_NODE_TYPE_UNARY_OP_PLUS,
    AST_NODE_TYPE_UNARY_OP_MINUS,
    AST_NODE_TYPE_UNARY_OP_BNEG,
    AST_NODE_TYPE_UNARY_OP_NOT,

    AST_NODE_TYPE_BINARY_OP_MEMBER_ACCESS,
    AST_NODE_TYPE_BINARY_OP_SUBSCRIPT,
    AST_NODE_TYPE_BINARY_OP_POW,
    AST_NODE_TYPE_BINARY_OP_AS,
    AST_NODE_TYPE_BINARY_OP_MUL,
    AST_NODE_TYPE_BINARY_OP_DIV,
    AST_NODE_TYPE_BINARY_OP_REM,
    AST_NODE_TYPE_BINARY_OP_ADD,
    AST_NODE_TYPE_BINARY_OP_SUB,
    AST_NODE_TYPE_BINARY_OP_SHL,
    AST_NODE_TYPE_BINARY_OP_SHR,
    AST_NODE_TYPE_BINARY_OP_CMP_LE,
    AST_NODE_TYPE_BINARY_OP_CMP_LEQ,
    AST_NODE_TYPE_BINARY_OP_CMP_GE,
    AST_NODE_TYPE_BINARY_OP_CMP_GEQ,
    AST_NODE_TYPE_BINARY_OP_CMP_EQ,
    AST_NODE_TYPE_BINARY_OP_CMP_NEQ,
    AST_NODE_TYPE_BINARY_OP_BAND,
    AST_NODE_TYPE_BINARY_OP_XOR,
    AST_NODE_TYPE_BINARY_OP_BOR,
    AST_NODE_TYPE_BINARY_OP_AND,
    AST_NODE_TYPE_BINARY_OP_OR,
    AST_NODE_TYPE_BINARY_OP_ASSIGNMENT,

    AST_NODE_TYPE_FN_CALL,

    AST_NODE_TYPE_DECL_FN,
    AST_NODE_TYPE_DECL_VAR,
    AST_NODE_TYPE_DECL_TYPE,

    AST_NODE_TYPE_STATEMENT_BLOCK,
    AST_NODE_TYPE_STATEMENT_IF,
    AST_NODE_TYPE_STATEMENT_WHILE,
    AST_NODE_TYPE_STATEMENT_BREAK,
    AST_NODE_TYPE_STATEMENT_CONTINUE,
    AST_NODE_TYPE_STATEMENT_RETURN
};

struct AST_node{
    const AST_node *m_parent;
    AST_node_ptr_slice m_sub_nodes;
    enum AST_node_type m_type;
    const Token *m_token;
};

enum Parse_error{
    PARSE_ERROR_NONE,
    PARSE_ERROR_OOM,
    PARSE_ERROR_SYNTAX
};

typedef struct Parse_result{
    union{
        AST_node_ptr_slice ast_nodes;
        Str_base error_info;
    };
    enum Parse_error error;
} Parse_result;

Parse_result parse(Arena *arena, Token_slice tokens);

#ifdef __cplusplus
}
#endif

#endif // LANG_PARSER_H
