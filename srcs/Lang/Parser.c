#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../hdrs/Allocator/Arena.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Num.h"

#include "../../hdrs/Lang/Lexer.h"
#include "../../hdrs/Lang/Parser.h"

// ------------------------------------------------------------------------------------------------

static bool token_type_is_atom(enum Token_type token_type){
    return token_type >= TOKEN_TYPE_ID && token_type <= TOKEN_TYPE_STR_LIT;
}
static bool token_type_is_operation(enum Token_type token_type){
    return token_type >= TOKEN_TYPE_EQUALS2 && token_type <= TOKEN_TYPE_EQUALS1;
}

typedef struct Binding_powers{
    u8 lhs, rhs;
} Binding_powers;

#define bps_init(lhs_bp, rhs_bp) (Binding_powers){.lhs = lhs_bp, .rhs = rhs_bp}

static const Binding_powers BINDING_POWERS_UNARY = {.lhs = 111, .rhs = 110};
static Binding_powers token_type_binding_powers(enum Token_type token_type){
    switch (token_type){
        case TOKEN_TYPE_ASTERISK2:            return bps_init(121, 120);

        case TOKEN_TYPE_NOT:
        case TOKEN_TYPE_TILDE:                return BINDING_POWERS_UNARY;

        case TOKEN_TYPE_ASTERISK1:
        case TOKEN_TYPE_SLASH:
        case TOKEN_TYPE_PERCENT:              return bps_init(100, 101);

        case TOKEN_TYPE_PLUS:
        case TOKEN_TYPE_MINUS:                return bps_init(90, 91);

        case TOKEN_TYPE_LESS_THAN2:
        case TOKEN_TYPE_GREATER_THAN2:        return bps_init(80, 81);

        case TOKEN_TYPE_EQUALS2:
        case TOKEN_TYPE_NOT_EQUALS:
        case TOKEN_TYPE_LESS_THAN1:
        case TOKEN_TYPE_LESS_THAN1_EQUALS:
        case TOKEN_TYPE_GREATER_THAN1:
        case TOKEN_TYPE_GREATER_THAN1_EQUALS: return bps_init(70, 71);

        case TOKEN_TYPE_AMPERSAND:            return bps_init(60, 61);
        case TOKEN_TYPE_CARET:                return bps_init(50, 51);
        case TOKEN_TYPE_PIPE:                 return bps_init(40, 41);
        case TOKEN_TYPE_AND:                  return bps_init(30, 31);
        case TOKEN_TYPE_OR:                   return bps_init(20, 21);
        case TOKEN_TYPE_EQUALS1:              return bps_init(11, 10);

        default:
            fprintf(stderr, "Not implemented");
            abort();
    }
}

typedef struct Parser_state{
    Allocator alloc;
    Token_slice tokens;
    usize token_idx;
    Vec_base ast_node_ptrs;
} Parser_state;

typedef struct Parser_state_parse_result{
    union{
        AST_node *ast_node_ptr;
        const char *error_info;
    };
    enum Parse_error error;
} Parser_state_parse_result;

static const Parser_state_parse_result OOM_ERROR = {.error = PARSE_ERROR_OOM};
static Parser_state_parse_result parser_state_syntax_error(Parser_state *self, const char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    Str_base_result error_info = str_base_init_fmt_va_list(self->alloc, fmt, args);
    va_end(args);

    return (error_info.success) ? (Parser_state_parse_result){.error_info = str_base_data(&error_info.result), .error = PARSE_ERROR_SYNTAX} : OOM_ERROR;
}

static AST_node* parser_state_ast_node_alloc(Parser_state *self, const Token *tok){
    AST_node *ast_node = allocator_alloc(self->alloc, AST_node, 1);

    if (ast_node)
        *ast_node = (AST_node){.m_parent = NULL, .m_sub_nodes = {.m_size = 0, .m_data = NULL}, .m_token = tok};

    return ast_node;
}

#define syntax_error(...) parser_state_syntax_error(self, __VA_ARGS__)
static Parser_state_parse_result parser_state_parse_arithm_expr(Parser_state *self, usize prev_rhs_bp){
    if (self->token_idx >= self->tokens.m_size)
        return syntax_error("On line <" USIZE_PFMT ">: No tokens are available", self->tokens.m_data[self->tokens.m_size - 1].m_line_number);

    AST_node *lhs;

    const Token *tok = &self->tokens.m_data[self->token_idx++];

    if (token_type_is_atom(tok->m_type)){
        lhs = parser_state_ast_node_alloc(self, tok);
        if (!lhs)
            return OOM_ERROR;
    }
    else if (tok->m_type == TOKEN_TYPE_LPAREN){
        Parser_state_parse_result temp = parser_state_parse_arithm_expr(self, 0);
        if (temp.error != PARSE_ERROR_NONE)
            return temp;

        lhs = temp.ast_node_ptr;

        tok = &self->tokens.m_data[self->token_idx++];
        if (tok->m_type != TOKEN_TYPE_RPAREN)
            return syntax_error("On line <" USIZE_PFMT ">: Expected <)>", tok->m_line_number);
    }
    else{
        switch (tok->m_type){
            case TOKEN_TYPE_PLUS:
            case TOKEN_TYPE_MINUS:
            case TOKEN_TYPE_TILDE:
            case TOKEN_TYPE_NOT:
                {
                    Parser_state_parse_result unary_rhs = parser_state_parse_arithm_expr(self, BINDING_POWERS_UNARY.rhs);
                    if (unary_rhs.error != PARSE_ERROR_NONE)
                        return unary_rhs;

                    Vec_base sub_nodes = vec_base_init(AST_node*);
                    lhs = parser_state_ast_node_alloc(self, tok);
                    if (!lhs || !vec_base_push_back(&sub_nodes, self->alloc, &unary_rhs.ast_node_ptr))
                        return OOM_ERROR;

                    lhs->m_sub_nodes = (AST_node_ptr_slice){.m_size = sub_nodes.m_size, .m_data = sub_nodes.m_data};
                    unary_rhs.ast_node_ptr->m_parent = lhs;
                }
                break;
            default:
                return syntax_error("On line <" USIZE_PFMT ">: Found invalid token <%s>", tok->m_line_number, str_base_data_const(&tok->m_id));
        }
    }

    while (true){
        if (self->token_idx >= self->tokens.m_size)
            return syntax_error("On line <" USIZE_PFMT ">: No tokens are available", self->tokens.m_data[self->tokens.m_size - 1].m_line_number);

        Parser_state_parse_result rhs;
        AST_node *new_lhs;

        Vec_base sub_nodes = vec_base_init(AST_node*);

        const Token *op = &self->tokens.m_data[self->token_idx];
        switch (op->m_type){
            case TOKEN_TYPE_COMMA:
            case TOKEN_TYPE_SEMICOLON:
            case TOKEN_TYPE_RPAREN:
            case TOKEN_TYPE_RBRACKET:
            case TOKEN_TYPE_DOT2:
                goto end;
            case TOKEN_TYPE_LBRACKET:
                ++self->token_idx;

                rhs = parser_state_parse_arithm_expr(self, 0);
                if (rhs.error != PARSE_ERROR_NONE)
                    return rhs;

                new_lhs = parser_state_ast_node_alloc(self, op);
                if (!new_lhs || !vec_base_push_back(&sub_nodes, self->alloc, &lhs) || !vec_base_push_back(&sub_nodes, self->alloc, &rhs.ast_node_ptr))
                    return OOM_ERROR;

                op = &self->tokens.m_data[self->token_idx++];
                if (op->m_type != TOKEN_TYPE_RBRACKET)
                    return syntax_error("On line <" USIZE_PFMT ">: <[> must be closed by <]>", op->m_line_number);

                rhs.ast_node_ptr->m_parent = new_lhs;
                break;
            case TOKEN_TYPE_LPAREN:
                new_lhs = parser_state_ast_node_alloc(self, op);
                if (!new_lhs || !vec_base_push_back(&sub_nodes, self->alloc, &lhs))
                    return OOM_ERROR;
                
                lhs->m_parent = new_lhs;

                while (self->tokens.m_data[++self->token_idx].m_type != TOKEN_TYPE_RPAREN){
                    rhs = parser_state_parse_arithm_expr(self, 0);
                    if (rhs.error != PARSE_ERROR_NONE)
                        return rhs;

                    if (!vec_base_push_back(&sub_nodes, self->alloc, &rhs.ast_node_ptr))
                        return OOM_ERROR;

                    rhs.ast_node_ptr->m_parent = new_lhs;

                    self->token_idx -= (self->tokens.m_data[self->token_idx].m_type != TOKEN_TYPE_COMMA);
                }
                ++self->token_idx;
                break;
            default:
                if (!token_type_is_operation(op->m_type))
                    return syntax_error("On line <" USIZE_PFMT ">: Found invalid token <%s>", op->m_line_number, str_base_data_const(&op->m_id));

                Binding_powers bps = token_type_binding_powers(op->m_type);
                if (bps.lhs < prev_rhs_bp)
                    goto end;

                ++self->token_idx;

                rhs = parser_state_parse_arithm_expr(self, bps.rhs);
                if (rhs.error != PARSE_ERROR_NONE)
                    return rhs;

                new_lhs = parser_state_ast_node_alloc(self, op);
                if (!new_lhs || !vec_base_push_back(&sub_nodes, self->alloc, &lhs) || !vec_base_push_back(&sub_nodes, self->alloc, &rhs.ast_node_ptr))
                    return OOM_ERROR;

                rhs.ast_node_ptr->m_parent = new_lhs;
                break;
        }

        lhs->m_parent = new_lhs;
        lhs = new_lhs;
        lhs->m_sub_nodes = (AST_node_ptr_slice){.m_size = sub_nodes.m_size, .m_data = sub_nodes.m_data};
    }

end:
    return (Parser_state_parse_result){.ast_node_ptr = lhs, .error = PARSE_ERROR_NONE};
}

static Parser_state_parse_result parser_state_parse_expr(Parser_state *self){
    const Token *tok = &self->tokens.m_data[self->token_idx++], *tok_temp;

    Parser_state_parse_result arithm_expr_result;
    AST_node *node, *node_temp;
    Vec_base sub_nodes = vec_base_init(AST_node*);

    switch (tok->m_type){
        case TOKEN_TYPE_ID: break;
        case TOKEN_TYPE_ARGV: break;
        case TOKEN_TYPE_FALSE: break;
        case TOKEN_TYPE_TRUE: break;
        case TOKEN_TYPE_CHAR_LIT: break;
        case TOKEN_TYPE_INT_LIT: break;
        case TOKEN_TYPE_FLOAT_LIT: break;
        case TOKEN_TYPE_STR_LIT: break;

        case TOKEN_TYPE_COMMA: break;
        case TOKEN_TYPE_COLON: break;
        case TOKEN_TYPE_SEMICOLON: break;
        case TOKEN_TYPE_LPAREN: break;
        case TOKEN_TYPE_RPAREN: break;
        case TOKEN_TYPE_LBRACKET: break;
        case TOKEN_TYPE_RBRACKET: break;
        case TOKEN_TYPE_LBRACE: break;
        case TOKEN_TYPE_RBRACE: break;

        case TOKEN_TYPE_DOT2: break;
        case TOKEN_TYPE_EQUALS2: break;
        case TOKEN_TYPE_NOT_EQUALS: break;
        case TOKEN_TYPE_LESS_THAN1: break;
        case TOKEN_TYPE_LESS_THAN1_EQUALS: break;
        case TOKEN_TYPE_GREATER_THAN1: break;
        case TOKEN_TYPE_GREATER_THAN1_EQUALS: break;
        case TOKEN_TYPE_PLUS: break;
        case TOKEN_TYPE_MINUS: break;
        case TOKEN_TYPE_ASTERISK1: break;
        case TOKEN_TYPE_ASTERISK2: break;
        case TOKEN_TYPE_SLASH: break;
        case TOKEN_TYPE_PERCENT: break;
        case TOKEN_TYPE_LESS_THAN2: break;
        case TOKEN_TYPE_GREATER_THAN2: break;
        case TOKEN_TYPE_AMPERSAND: break;
        case TOKEN_TYPE_PIPE: break;
        case TOKEN_TYPE_CARET: break;
        case TOKEN_TYPE_TILDE: break;
        case TOKEN_TYPE_AND: break;
        case TOKEN_TYPE_OR: break;
        case TOKEN_TYPE_NOT: break;
        case TOKEN_TYPE_EQUALS1: break;
        
        case TOKEN_TYPE_FN: break;
        case TOKEN_TYPE_LET:
            node = parser_state_ast_node_alloc(self, tok);
            if (!node)
                return OOM_ERROR;

            if (self->token_idx >= self->tokens.m_size || (tok_temp = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_ID)
                return syntax_error("On line <" USIZE_PFMT ">: <let> must be followed by an identifier", tok->m_line_number);
            tok = tok_temp;

            node_temp = parser_state_ast_node_alloc(self, tok);
            if (!node_temp || !vec_base_push_back(&sub_nodes, self->alloc, &node_temp))
                return OOM_ERROR;

            node_temp->m_parent = node;

            if (self->token_idx >= self->tokens.m_size || (tok_temp = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_COLON)
                return syntax_error("On line <" USIZE_PFMT ">: <%s> must be followed by <:>", tok->m_line_number, str_base_data_const(&tok->m_id));
            tok = tok_temp;

            if (self->token_idx >= self->tokens.m_size)
                return syntax_error("On line <" USIZE_PFMT ">: <:> must be followed by a type", tok->m_line_number);

            tok = &self->tokens.m_data[self->token_idx++];
            switch (tok->m_type){
                case TOKEN_TYPE_LBRACKET:
                    if (self->token_idx >= self->tokens.m_size || (tok_temp = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_RBRACKET)
                        return syntax_error("On line <" USIZE_PFMT ">: After <:> <[> must be followed by <]>", tok->m_line_number);

                    node_temp = parser_state_ast_node_alloc(self, tok);
                    if (!node_temp || !vec_base_push_back(&sub_nodes, self->alloc, &node_temp))
                        return OOM_ERROR;

                    node_temp->m_parent = node;

                    if (self->token_idx >= self->tokens.m_size)
                        return syntax_error("On line <" USIZE_PFMT ">: <[]> must be followed by a type", tok_temp->m_line_number);

                    tok = &self->tokens.m_data[self->token_idx++];
                    switch (tok->m_type){
                        case TOKEN_TYPE_BOOL:
                        case TOKEN_TYPE_CHAR:
                        case TOKEN_TYPE_INT:
                        case TOKEN_TYPE_FLOAT:
                        case TOKEN_TYPE_STR:
                            {
                                Vec_base list_type_sub_nodes = vec_base_init(AST_node*);
                                AST_node *list_type_node = parser_state_ast_node_alloc(self, tok);
                                if (!list_type_node || !vec_base_push_back(&list_type_sub_nodes, self->alloc, &list_type_node))
                                    return OOM_ERROR;

                                if (self->token_idx >= self->tokens.m_size || (tok_temp = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_EQUALS1)
                                    return syntax_error("On line <" USIZE_PFMT ">: <[]%s> must be followed by <=>", tok->m_line_number, str_base_data_const(&tok->m_id));
                                tok = tok_temp;

                                if (
                                    self->token_idx >= self->tokens.m_size || (tok_temp = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_LBRACKET ||
                                    self->token_idx >= self->tokens.m_size || (tok_temp = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_RBRACKET
                                )
                                    return syntax_error("On line <" USIZE_PFMT ">: <=> must be followed by <[]>", tok->m_line_number, str_base_data_const(&tok->m_id));

                                list_type_node->m_parent = node_temp;
                                node_temp->m_sub_nodes = (AST_node_ptr_slice){.m_size = list_type_sub_nodes.m_size, .m_data = list_type_sub_nodes.m_data};
                            }
                            break;
                        
                        default:
                            return syntax_error("On line <" USIZE_PFMT ">: <[]> must be followed by a type", tok->m_line_number);
                    }
                    break;
                case TOKEN_TYPE_BOOL:
                case TOKEN_TYPE_CHAR:
                case TOKEN_TYPE_INT:
                case TOKEN_TYPE_FLOAT:
                case TOKEN_TYPE_STR:
                    node_temp = parser_state_ast_node_alloc(self, tok);
                    if (!node_temp || !vec_base_push_back(&sub_nodes, self->alloc, &node_temp))
                        return OOM_ERROR;

                    node_temp->m_parent = node;

                    if (self->token_idx >= self->tokens.m_size || (tok_temp = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_EQUALS1)
                        return syntax_error("On line <" USIZE_PFMT ">: <%s> must be followed by <=>", tok->m_line_number, str_base_data_const(&tok->m_id));
                    tok = tok_temp;

                    arithm_expr_result = parser_state_parse_arithm_expr(self, 0);
                    if (arithm_expr_result.error != PARSE_ERROR_NONE)
                        return arithm_expr_result;

                    if (!vec_base_push_back(&sub_nodes, self->alloc, &arithm_expr_result.ast_node_ptr))
                        return OOM_ERROR;

                    arithm_expr_result.ast_node_ptr->m_parent = node;
                    break;

                default:
                    return syntax_error("On line <" USIZE_PFMT ">: <:> must be followed by a type", tok->m_line_number);
            }

            if (self->token_idx >= self->tokens.m_size || self->tokens.m_data[self->token_idx++].m_type != TOKEN_TYPE_SEMICOLON)
                return syntax_error("On line <" USIZE_PFMT ">: <let> statement must be closed by <;>", tok->m_line_number);

            node->m_sub_nodes = (AST_node_ptr_slice){.m_size = sub_nodes.m_size, .m_data = sub_nodes.m_data};
            break;

        case TOKEN_TYPE_BOOL: break;
        case TOKEN_TYPE_CHAR: break;
        case TOKEN_TYPE_INT: break;
        case TOKEN_TYPE_FLOAT: break;
        case TOKEN_TYPE_STR: break;

        case TOKEN_TYPE_IF: break;
        case TOKEN_TYPE_ELSE: break;

        case TOKEN_TYPE_WHILE: break;
        case TOKEN_TYPE_FOR: break;

        case TOKEN_TYPE_RETURN: break;

        default: break;
    }

    return (Parser_state_parse_result){.ast_node_ptr = node, .error = PARSE_ERROR_NONE};
}

static void ast_node_print(const AST_node *self, usize indent){
    for (usize i = 0; i < indent; ++i)
        putchar(' ');
    // printf("%s\n", str_base_data_const(&self->m_token->m_id));
    printf("%s parent: %s\n", str_base_data_const(&self->m_token->m_id), (self->m_parent) ? str_base_data_const(&self->m_parent->m_token->m_id) : "null");
    for (usize i = 0; i < self->m_sub_nodes.m_size; ++i)
        ast_node_print(self->m_sub_nodes.m_data[i], indent + 4);
}

// ------------------------------------------------------------------------------------------------

void ast_node_ptr_slice_print(AST_node_ptr_slice ast_node_ptr_slice){
    for (usize i = 0; i < ast_node_ptr_slice.m_size; ++i)
        ast_node_print(ast_node_ptr_slice.m_data[i], 0);
}

Parse_result parse(Arena *arena, Token_slice tokens){
    assert(arena && "<arena> is not nullable");

    Parser_state state = {
        .alloc         = arena_allocator(arena),
        .tokens        = tokens,
        .token_idx     = 0,
        .ast_node_ptrs = vec_base_init(AST_node*)
    };

    while (state.token_idx < state.tokens.m_size){
        const Token *tok = &state.tokens.m_data[state.token_idx];
        if (tok->m_type != TOKEN_TYPE_SEMICOLON){
            // Parser_state_parse_result ast_node = parser_state_parse_arithm_expr(&state, 0);
            Parser_state_parse_result ast_node = parser_state_parse_expr(&state);
            switch (ast_node.error){
                case PARSE_ERROR_NONE:
                    break;
                case PARSE_ERROR_OOM:
                case PARSE_ERROR_SYNTAX:
                    fprintf(stderr, "\x1b[38;2;255;0;0m%s\x1b[0m", ast_node.error_info);
                    return (Parse_result){.error_info = ast_node.error_info, .error = ast_node.error};
            }
            if (!vec_base_push_back(&state.ast_node_ptrs, state.alloc, &ast_node.ast_node_ptr))
                return (Parse_result){.error = PARSE_ERROR_OOM};
        }
        else
            ++state.token_idx;
    }

    ast_node_ptr_slice_print((AST_node_ptr_slice){.m_size = state.ast_node_ptrs.m_size, .m_data = state.ast_node_ptrs.m_data});

    if (state.tokens.m_size == 0 || state.tokens.m_data[state.tokens.m_size - 1].m_type != TOKEN_TYPE_RETURN){
        assert(false && "TODO: Add <return 0;> if last statement is not a return");
    }

    return (Parse_result){.ast_nodes = {.m_size = state.ast_node_ptrs.m_size, .m_data = state.ast_node_ptrs.m_data}, .error = PARSE_ERROR_NONE};
}
