#ifndef LANG_PARSER_H
#define LANG_PARSER_H

#ifdef __cplusplus
extern "C"{
#endif

#include "../Allocator/Arena.h"
#include "../Utils/Num.h"

#include "Lexer.h"

typedef struct AST_node AST_node;

typedef struct AST_node_ptr_slice{
    usize m_size;
    const AST_node *const *m_data;
} AST_node_ptr_slice;

void ast_node_ptr_slice_print(AST_node_ptr_slice ast_node_ptr_slice);

struct AST_node{
    const AST_node *m_parent;
    AST_node_ptr_slice m_sub_nodes;
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
        const char *error_info;
    };
    enum Parse_error error;
} Parse_result;

Parse_result parse(Arena *arena, Token_slice tokens);

#ifdef __cplusplus
}
#endif

#endif // LANG_PARSER_H
