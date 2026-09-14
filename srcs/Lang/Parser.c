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

static bool token_type_unary_op_to_ast_node_type(enum Token_type token_type, enum AST_node_type *out_ast_node_type){
    switch (token_type){
        case TOKEN_TYPE_PLUS:  *out_ast_node_type = AST_NODE_TYPE_UNARY_OP_PLUS;  break;
        case TOKEN_TYPE_MINUS: *out_ast_node_type = AST_NODE_TYPE_UNARY_OP_MINUS; break;
        case TOKEN_TYPE_TILDE: *out_ast_node_type = AST_NODE_TYPE_UNARY_OP_BNEG;  break;
        case TOKEN_TYPE_NOT:   *out_ast_node_type = AST_NODE_TYPE_UNARY_OP_NOT;   break;
        default:               return false;
    }
    return true;
}

typedef struct Binding_powers{
    u8 lhs, rhs;
} Binding_powers;

static const u8 UNARY_BINDING_POWER = 120;

static bool token_type_binary_op_to_ast_node_type(enum Token_type token_type, enum AST_node_type *out_ast_node_type){
    switch (token_type){
        case TOKEN_TYPE_LBRACKET:              *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_SUBSCRIPT; break;
        case TOKEN_TYPE_ASTERISK2:             *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_POW;       break;
        case TOKEN_TYPE_AS:                    *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_AS;        break;
        case TOKEN_TYPE_ASTERISK1:             *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_MUL;       break;
        case TOKEN_TYPE_SLASH:                 *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_DIV;       break;
        case TOKEN_TYPE_PERCENT:               *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_REM;       break;
        case TOKEN_TYPE_PLUS:                  *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_ADD;       break;
        case TOKEN_TYPE_MINUS:                 *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_SUB;       break;
        case TOKEN_TYPE_LESS_THAN2:            *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_SHL;       break;
        case TOKEN_TYPE_GREATER_THAN2:         *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_SHR;       break;
        case TOKEN_TYPE_LESS_THAN1:            *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_CMP_LE;    break;
        case TOKEN_TYPE_LESS_THAN1_EQUALS1:    *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_CMP_LEQ;   break;
        case TOKEN_TYPE_GREATER_THAN1:         *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_CMP_GE;    break;
        case TOKEN_TYPE_GREATER_THAN1_EQUALS1: *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_CMP_GEQ;   break;
        case TOKEN_TYPE_EQUALS2:               *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_CMP_EQ;    break;
        case TOKEN_TYPE_NOT_EQUALS1:           *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_CMP_NEQ;   break;
        case TOKEN_TYPE_AMPERSAND:             *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_BAND;      break;
        case TOKEN_TYPE_CARET:                 *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_XOR;       break;
        case TOKEN_TYPE_PIPE:                  *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_BOR;       break;
        case TOKEN_TYPE_AND:                   *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_AND;       break;
        case TOKEN_TYPE_OR:                    *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_OR;        break;
        case TOKEN_TYPE_EQUALS1:               *out_ast_node_type = AST_NODE_TYPE_BINARY_OP_ASSIGN;    break;
        default:                               return false;
    }
    return true;
}

static Binding_powers token_type_binding_powers(enum AST_node_type ast_node_type){
    #define bps_init(lhs_bp, rhs_bp) (Binding_powers){.lhs = lhs_bp, .rhs = rhs_bp}

    switch (ast_node_type){
        case AST_NODE_TYPE_BINARY_OP_SUBSCRIPT: return bps_init(140, 141); 

        case AST_NODE_TYPE_BINARY_OP_POW:       return bps_init(131, 130); 

        case AST_NODE_TYPE_BINARY_OP_AS:        return bps_init(UNARY_BINDING_POWER + 1, UNARY_BINDING_POWER); 

        case AST_NODE_TYPE_BINARY_OP_MUL:
        case AST_NODE_TYPE_BINARY_OP_DIV:
        case AST_NODE_TYPE_BINARY_OP_REM:       return bps_init(110, 111);

        case AST_NODE_TYPE_BINARY_OP_ADD:
        case AST_NODE_TYPE_BINARY_OP_SUB:       return bps_init(100, 101);

        case AST_NODE_TYPE_BINARY_OP_SHL:
        case AST_NODE_TYPE_BINARY_OP_SHR:       return bps_init(90, 91);

        case AST_NODE_TYPE_BINARY_OP_CMP_LE:
        case AST_NODE_TYPE_BINARY_OP_CMP_LEQ:
        case AST_NODE_TYPE_BINARY_OP_CMP_GE:
        case AST_NODE_TYPE_BINARY_OP_CMP_GEQ:   return bps_init(80, 81);

        case AST_NODE_TYPE_BINARY_OP_CMP_EQ:
        case AST_NODE_TYPE_BINARY_OP_CMP_NEQ:   return bps_init(70, 71);

        case AST_NODE_TYPE_BINARY_OP_BAND:      return bps_init(60, 61);
        case AST_NODE_TYPE_BINARY_OP_XOR:       return bps_init(50, 51);
        case AST_NODE_TYPE_BINARY_OP_BOR:       return bps_init(40, 41);
        case AST_NODE_TYPE_BINARY_OP_AND:       return bps_init(30, 31);
        case AST_NODE_TYPE_BINARY_OP_OR:        return bps_init(20, 21);
        case AST_NODE_TYPE_BINARY_OP_ASSIGN:    return bps_init(11, 10);

        default:                                unreachable();
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
        Str_base error_info;
    };
    enum Parse_error error;
} Parser_state_parse_result;

static const Parser_state_parse_result OOM_ERROR = {.error = PARSE_ERROR_OOM};

static Parser_state_parse_result parser_state_syntax_error(Parser_state *self, const char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    Str_base_result error_info = str_base_init_fmt_va_list(self->alloc, fmt, args);
    va_end(args);

    return (error_info.success) ? (Parser_state_parse_result){.error_info = error_info.result, .error = PARSE_ERROR_SYNTAX} : OOM_ERROR;
}

static AST_node* parser_state_ast_node_alloc(Parser_state *self, const Token *tok){
    AST_node *ast_node = allocator_alloc(self->alloc, AST_node, 1);
    if (ast_node)
        *ast_node = (AST_node){.m_parent = NULL, .m_sub_nodes = {.m_size = 0, .m_data = NULL}, .m_token = tok};
    return ast_node;
}
#define syntax_error(...) parser_state_syntax_error(self, "On line <" USIZE_PFMT ">: " __VA_ARGS__)

static bool parser_state_for_to_while_tokens_push_back(Parser_state *self, Vec_base *for_to_while_tokens, const char *id, enum Token_type token_type, usize line_number){
    Str_base_result token_id = str_base_init_raw(self->alloc, id);
    return token_id.success && vec_base_push_back(for_to_while_tokens, self->alloc, &(Token){.m_type = token_type, .m_id = token_id.result, .m_line_number = line_number});
}


static Parser_state_parse_result parser_state_parse_type(Parser_state *self, bool void_is_allowed);
static Parser_state_parse_result parser_state_parse_arithm_expr(Parser_state *self, u8 prev_rhs_bp);
static Parser_state_parse_result parser_state_parse(Parser_state *self);

static Parser_state_parse_result parser_state_parse_and_add_ast_sub_node_type(Parser_state *self, AST_node *parent, Vec_base *parent_sub_nodes, bool void_is_allowed);
static Parser_state_parse_result parser_state_parse_and_add_ast_sub_node_arithm_expr(Parser_state *self, AST_node *parent, Vec_base *parent_sub_nodes, u8 prev_rhs_bp);
static Parser_state_parse_result parser_state_parse_and_add_ast_sub_node_arithm_enclosing(
    Parser_state *self,
    AST_node *parent,
    Vec_base *parent_sub_nodes,
    enum Token_type end_token
);
static Parser_state_parse_result parser_state_parse_and_add_ast_sub_node_expr(Parser_state *self, AST_node *parent, Vec_base *parent_sub_nodes);
static Parser_state_parse_result parser_state_parse_and_add_ast_node_body(Parser_state *self, AST_node *parent, Vec_base *parent_sub_nodes);

Parser_state_parse_result parser_state_parse_type(Parser_state *self, bool void_is_allowed){
    if (self->token_idx >= self->tokens.m_size)
        return syntax_error("No tokens are available", self->tokens.m_data[self->tokens.m_size - 1].m_line_number);

    const Token *tok = &self->tokens.m_data[self->token_idx++];

    AST_node *type_node = parser_state_ast_node_alloc(self, tok);
    Vec_base type_node_sub_nodes = vec_base_init(AST_node*);
    if (!type_node)
        return OOM_ERROR;

    switch (tok->m_type){
        case TOKEN_TYPE_LBRACKET:{
            if (self->token_idx >= self->tokens.m_size || (tok = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_RBRACKET)
                return syntax_error("<[> must be closed by <]>", tok->m_line_number);

            Parser_state_parse_result parse_result = parser_state_parse_and_add_ast_sub_node_type(self, type_node, &type_node_sub_nodes, false);
            if (parse_result.error != PARSE_ERROR_NONE)
                return parse_result;
            
            type_node->m_type = AST_NODE_TYPE_TYPE_LIST;
            break;
        }
        case TOKEN_TYPE_VOID:
            if (!void_is_allowed)
                return syntax_error("<void> is not allowed as a type in the current context", tok->m_line_number);
            FALLTHROUGH;
        case TOKEN_TYPE_BOOL:
        case TOKEN_TYPE_CHAR:
        case TOKEN_TYPE_INT:
        case TOKEN_TYPE_FLOAT:
        case TOKEN_TYPE_STR:
            type_node->m_type = (enum AST_node_type)(AST_NODE_TYPE_TYPE_VOID + (tok->m_type - TOKEN_TYPE_VOID));
            break;
        default:
            return syntax_error("Found unknown or contextually invalid token <%s>", tok->m_line_number, str_base_data_const(&tok->m_id));
    }

    type_node->m_sub_nodes = (AST_node_ptr_slice){.m_size = type_node_sub_nodes.m_size, .m_data = type_node_sub_nodes.m_data};

    return (Parser_state_parse_result){.ast_node_ptr = type_node, .error = PARSE_ERROR_NONE};
}

Parser_state_parse_result parser_state_parse_arithm_expr(Parser_state *self, u8 prev_rhs_bp){
    if (self->token_idx >= self->tokens.m_size)
        return syntax_error("No tokens are available", self->tokens.m_data[self->tokens.m_size - 1].m_line_number);

    const Token *tok = &self->tokens.m_data[self->token_idx++];

    AST_node *lhs = parser_state_ast_node_alloc(self, tok);
    Vec_base lhs_sub_nodes = vec_base_init(AST_node*);
    if (!lhs)
        return OOM_ERROR;
    lhs->m_type = (enum AST_node_type)(AST_NODE_TYPE_ATOM_ID + (tok->m_type - TOKEN_TYPE_ID));

    if (tok->m_type == TOKEN_TYPE_LBRACKET){
        --self->token_idx;
        Parser_state_parse_result init_list_result = parser_state_parse_and_add_ast_sub_node_arithm_enclosing(self, lhs, &lhs_sub_nodes, TOKEN_TYPE_RBRACKET);
        if (init_list_result.error != PARSE_ERROR_NONE)
            return init_list_result;
        *lhs = (AST_node){
            .m_parent    = lhs->m_parent,
            .m_sub_nodes = {.m_size = lhs_sub_nodes.m_size, .m_data = lhs_sub_nodes.m_data},
            .m_type      = AST_NODE_TYPE_ATOM_INIT_LIST,
            .m_token     = lhs->m_token
        };
    }
    else if (tok->m_type == TOKEN_TYPE_LPAREN){
        Parser_state_parse_result temp = parser_state_parse_arithm_expr(self, 0);
        if (temp.error != PARSE_ERROR_NONE)
            return temp;
        lhs = temp.ast_node_ptr;

        tok = &self->tokens.m_data[self->token_idx++];
        if (tok->m_type != TOKEN_TYPE_RPAREN)
            return syntax_error("Expected <)>", tok->m_line_number);
    }
    else if (!token_type_is_atom(tok->m_type)){
        enum AST_node_type unary_op_node_type;
        if (!token_type_unary_op_to_ast_node_type(tok->m_type, &unary_op_node_type))
            return syntax_error("Found invalid token <%s>", tok->m_line_number, str_base_data_const(&tok->m_id));

        Parser_state_parse_result unary_result = parser_state_parse_and_add_ast_sub_node_arithm_expr(self, lhs, &lhs_sub_nodes, UNARY_BINDING_POWER);
        if (unary_result.error != PARSE_ERROR_NONE)
            return unary_result;

        lhs->m_sub_nodes = (AST_node_ptr_slice){.m_size = lhs_sub_nodes.m_size, .m_data = lhs_sub_nodes.m_data};
        lhs->m_type = unary_op_node_type;
    }

    while (true){
        if (self->token_idx >= self->tokens.m_size)
            return syntax_error("No tokens are available", self->tokens.m_data[self->tokens.m_size - 1].m_line_number);

        Parser_state_parse_result rhs_result;

        const Token *op_tok = &self->tokens.m_data[self->token_idx];

        AST_node *op_node = parser_state_ast_node_alloc(self, op_tok);
        Vec_base op_node_sub_nodes = vec_base_init(AST_node*);
        if (!op_node)
            return OOM_ERROR;

        enum AST_node_type op_node_type;

        switch (op_tok->m_type){
            case TOKEN_TYPE_COMMA:
            case TOKEN_TYPE_SEMICOLON:
            case TOKEN_TYPE_RPAREN:
            case TOKEN_TYPE_RBRACKET:
            case TOKEN_TYPE_DOT2:
                goto end;

            case TOKEN_TYPE_LBRACKET:{
                Token *subscript_token = allocator_alloc(self->alloc, Token, 1);
                Str_base_result subscript_token_id;
                if (!subscript_token || !(subscript_token_id = str_base_init_raw(self->alloc, "[]")).success)
                    return OOM_ERROR;
                *subscript_token = (Token){.m_type = op_tok->m_type, .m_id = subscript_token_id.result, .m_line_number = op_tok->m_line_number};

                ++self->token_idx;

                rhs_result = parser_state_parse_arithm_expr(self, 0);

                const Token *temp = &self->tokens.m_data[self->token_idx++];
                if (temp->m_type != TOKEN_TYPE_RBRACKET)
                    return syntax_error("<[> must be closed by <]>", temp->m_line_number);

                op_node_type = AST_NODE_TYPE_BINARY_OP_SUBSCRIPT;
                op_node->m_token = subscript_token;
                break;
            }

            case TOKEN_TYPE_LPAREN:
                if (!vec_base_push_back(&op_node_sub_nodes, self->alloc, &lhs))
                    return OOM_ERROR;
                rhs_result = parser_state_parse_and_add_ast_sub_node_arithm_enclosing(self, op_node, &op_node_sub_nodes, TOKEN_TYPE_RPAREN);
                if (rhs_result.error != PARSE_ERROR_NONE)
                    return rhs_result;
                op_node_type = AST_NODE_TYPE_FN_CALL;
                break;

            default:{
                if (!token_type_binary_op_to_ast_node_type(op_tok->m_type, &op_node_type))
                    return syntax_error("Found invalid token <%s>", op_tok->m_line_number, str_base_data_const(&op_tok->m_id));

                Binding_powers bps = token_type_binding_powers(op_node_type);
                if (bps.lhs < prev_rhs_bp)
                    goto end;

                ++self->token_idx;

                rhs_result = (op_tok->m_type != TOKEN_TYPE_AS) ? parser_state_parse_arithm_expr(self, bps.rhs) : parser_state_parse_type(self, false);
                break;
            }
        }

        if (op_tok->m_type != TOKEN_TYPE_LPAREN){
            if (rhs_result.error != PARSE_ERROR_NONE)
                return rhs_result;
            if (!vec_base_push_back(&op_node_sub_nodes, self->alloc, &lhs) || !vec_base_push_back(&op_node_sub_nodes, self->alloc, &rhs_result.ast_node_ptr))
                return OOM_ERROR;
            rhs_result.ast_node_ptr->m_parent = op_node;
        }

        lhs->m_parent = op_node;
        lhs = op_node;
        lhs->m_sub_nodes = (AST_node_ptr_slice){.m_size = op_node_sub_nodes.m_size, .m_data = op_node_sub_nodes.m_data};
        lhs->m_type = op_node_type;
    }

end:
    return (Parser_state_parse_result){.ast_node_ptr = lhs, .error = PARSE_ERROR_NONE};
}

Parser_state_parse_result parser_state_parse_and_add_ast_sub_node_type(Parser_state *self, AST_node *parent, Vec_base *parent_sub_nodes, bool void_is_allowed){
    Parser_state_parse_result result = parser_state_parse_type(self, void_is_allowed);
    if (result.error == PARSE_ERROR_NONE){
        if (!vec_base_push_back(parent_sub_nodes, self->alloc, &result.ast_node_ptr))
            return OOM_ERROR;
        result.ast_node_ptr->m_parent = parent;
    }
    return result;
}
Parser_state_parse_result parser_state_parse_and_add_ast_sub_node_arithm_expr(Parser_state *self, AST_node *parent, Vec_base *parent_sub_nodes, u8 prev_rhs_bp){
    Parser_state_parse_result result = parser_state_parse_arithm_expr(self, prev_rhs_bp);
    if (result.error == PARSE_ERROR_NONE){
        if (!vec_base_push_back(parent_sub_nodes, self->alloc, &result.ast_node_ptr))
            return OOM_ERROR;
        result.ast_node_ptr->m_parent = parent;
    }
    return result;
}
Parser_state_parse_result parser_state_parse_and_add_ast_sub_node_arithm_enclosing(
    Parser_state *self,
    AST_node *parent,
    Vec_base *parent_sub_nodes,
    enum Token_type end_token
){
    while (self->tokens.m_data[++self->token_idx].m_type != end_token){
        Parser_state_parse_result rhs_result = parser_state_parse_and_add_ast_sub_node_arithm_expr(self, parent, parent_sub_nodes, 0);
        if (rhs_result.error != PARSE_ERROR_NONE)
            return rhs_result;
        self->token_idx -= (self->tokens.m_data[self->token_idx].m_type != TOKEN_TYPE_COMMA);
    }
    ++self->token_idx;
    return (Parser_state_parse_result){.error = PARSE_ERROR_NONE};
}
Parser_state_parse_result parser_state_parse_and_add_ast_sub_node_expr(Parser_state *self, AST_node *parent, Vec_base *parent_sub_nodes){
    Parser_state_parse_result result = parser_state_parse(self);
    if (result.error == PARSE_ERROR_NONE){
        if (!vec_base_push_back(parent_sub_nodes, self->alloc, &result.ast_node_ptr))
            return OOM_ERROR;
        result.ast_node_ptr->m_parent = parent;
    }
    return result;
}
Parser_state_parse_result parser_state_parse_and_add_ast_node_body(Parser_state *self, AST_node *parent, Vec_base *parent_sub_nodes){
    Parser_state_parse_result result = {.error = PARSE_ERROR_NONE};
    if (self->tokens.m_data[self->token_idx].m_type == TOKEN_TYPE_SEMICOLON){
        if (!vec_base_push_back(parent_sub_nodes, self->alloc, &(AST_node*){NULL}))
            return OOM_ERROR;
        ++self->token_idx;
    }
    else
        result = parser_state_parse_and_add_ast_sub_node_expr(self, parent, parent_sub_nodes);
    return result;
}

Parser_state_parse_result parser_state_parse(Parser_state *self){
    Parser_state_parse_result parse_result;

    const Token *loop_label_tok_ptr = NULL;

    const Token *tok = &self->tokens.m_data[self->token_idx++];
    
    AST_node *node = parser_state_ast_node_alloc(self, tok);
    Vec_base node_sub_nodes = vec_base_init(AST_node*);
    if (!node)
        return OOM_ERROR;

    switch (tok->m_type){
        case TOKEN_TYPE_ID:{
            if (self->token_idx >= self->tokens.m_size)
                return syntax_error("No tokens are available", tok->m_line_number);

            const Token *temp = &self->tokens.m_data[self->token_idx];
            if (temp->m_type == TOKEN_TYPE_COLON){
                loop_label_tok_ptr = tok;

                if (++self->token_idx >= self->tokens.m_size || ((temp = &self->tokens.m_data[self->token_idx])->m_type != TOKEN_TYPE_WHILE && temp->m_type != TOKEN_TYPE_FOR))
                    return syntax_error("<:> must be followed by <while> or <for>", temp->m_line_number);

                node->m_token = temp;

                tok = &self->tokens.m_data[self->token_idx++];
                if (tok->m_type == TOKEN_TYPE_WHILE)
                    goto if_while_case;
                goto for_case;
            }

            FALLTHROUGH;
        }
        case TOKEN_TYPE_ARGV:
        case TOKEN_TYPE_FALSE:
        case TOKEN_TYPE_TRUE:
        case TOKEN_TYPE_CHAR_LIT:
        case TOKEN_TYPE_INT_LIT:
        case TOKEN_TYPE_FLOAT_LIT:
        case TOKEN_TYPE_STR_LIT:
        case TOKEN_TYPE_LPAREN:
        case TOKEN_TYPE_LBRACKET:
        case TOKEN_TYPE_PLUS:
        case TOKEN_TYPE_MINUS:
        case TOKEN_TYPE_TILDE:
        case TOKEN_TYPE_NOT:
            --self->token_idx;

            parse_result = parser_state_parse_arithm_expr(self, 0);
            if (parse_result.error != PARSE_ERROR_NONE)
                return parse_result;
            node = parse_result.ast_node_ptr;

            if (self->token_idx >= self->tokens.m_size || (tok = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_SEMICOLON)
                return syntax_error("Statement must end with <;>", tok->m_line_number);
            break;

        case TOKEN_TYPE_LBRACE:
            while (self->tokens.m_data[self->token_idx].m_type != TOKEN_TYPE_RBRACE){
                parse_result = parser_state_parse_and_add_ast_sub_node_expr(self, node, &node_sub_nodes);
                if (parse_result.error != PARSE_ERROR_NONE)
                    return parse_result;
            }
            ++self->token_idx;
            node->m_sub_nodes = (AST_node_ptr_slice){.m_size = node_sub_nodes.m_size, .m_data = node_sub_nodes.m_data};
            node->m_type = AST_NODE_TYPE_STATEMENT_BLOCK;
            break;
        
        case TOKEN_TYPE_FN:{
            if (self->token_idx >= self->tokens.m_size || (tok = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_ID)
                return syntax_error("<fn> must be followed by an identifier in function definition", tok->m_line_number);

            AST_node *id_node = parser_state_ast_node_alloc(self, tok);
            if (!id_node || !vec_base_push_back(&node_sub_nodes, self->alloc, &id_node))
                return OOM_ERROR;
            id_node->m_parent = node;
            id_node->m_type = AST_NODE_TYPE_ATOM_ID;

            if (self->token_idx >= self->tokens.m_size || (tok = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_LPAREN)
                return syntax_error("<%s> must be followed by <(> in function definition", tok->m_line_number, str_base_data_const(&id_node->m_token->m_id));

            for (const Token *fn_arg_tok_it; (fn_arg_tok_it = &self->tokens.m_data[self->token_idx])->m_type != TOKEN_TYPE_RPAREN;){
                if (fn_arg_tok_it->m_type != TOKEN_TYPE_ID)
                    return syntax_error("<%s> must be followed by an identifier in function definition", fn_arg_tok_it->m_line_number, str_base_data_const(&tok->m_id));

                tok = fn_arg_tok_it;

                id_node = parser_state_ast_node_alloc(self, tok);
                Vec_base id_node_sub_nodes = vec_base_init(AST_node*);
                if (!id_node || !vec_base_push_back(&node_sub_nodes, self->alloc, &id_node))
                    return OOM_ERROR;

                fn_arg_tok_it = &self->tokens.m_data[++self->token_idx];
                if (fn_arg_tok_it->m_type != TOKEN_TYPE_COLON)
                    return syntax_error("<%s> must be followed by <:> in function definition", fn_arg_tok_it->m_line_number, str_base_data_const(&tok->m_id));

                ++self->token_idx;

                parse_result = parser_state_parse_and_add_ast_sub_node_type(self, id_node, &id_node_sub_nodes, false);
                if (parse_result.error != PARSE_ERROR_NONE)
                    return parse_result;

                tok = &self->tokens.m_data[self->token_idx];
                if (tok->m_type != TOKEN_TYPE_COMMA && tok->m_type != TOKEN_TYPE_RPAREN)
                    return syntax_error("<,> must be used as a separator in the arguments of a function in function definition", tok->m_line_number);

                *id_node = (AST_node){
                    .m_parent    = node,
                    .m_sub_nodes = (AST_node_ptr_slice){.m_size = id_node_sub_nodes.m_size, .m_data = id_node_sub_nodes.m_data},
                    .m_type      = AST_NODE_TYPE_ATOM_ID,
                    .m_token     = id_node->m_token,
                };

                self->token_idx += (tok->m_type == TOKEN_TYPE_COMMA);
            }
            ++self->token_idx;

            parse_result = parser_state_parse_and_add_ast_sub_node_type(self, node, &node_sub_nodes, true);
            if (parse_result.error != PARSE_ERROR_NONE)
                return parse_result;

            tok = &self->tokens.m_data[self->tokens.m_size - 1];
            if (self->token_idx >= self->tokens.m_size || (tok = &self->tokens.m_data[self->token_idx])->m_type != TOKEN_TYPE_LBRACE)
                return syntax_error("Return type must be followed by <{> in function definition", tok->m_line_number);

            parse_result = parser_state_parse_and_add_ast_sub_node_expr(self, node, &node_sub_nodes);
            if (parse_result.error != PARSE_ERROR_NONE)
                return parse_result;

            node->m_sub_nodes = (AST_node_ptr_slice){.m_size = node_sub_nodes.m_size, .m_data = node_sub_nodes.m_data};
            node->m_type = AST_NODE_TYPE_DECL_FN;
            break;
        }

        case TOKEN_TYPE_LET:{
            if (self->token_idx >= self->tokens.m_size || (tok = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_ID)
                return syntax_error("<let> must be followed by an identifier", tok->m_line_number);

            AST_node *id_node = parser_state_ast_node_alloc(self, tok);
            if (!id_node || !vec_base_push_back(&node_sub_nodes, self->alloc, &id_node))
                return OOM_ERROR;
            id_node->m_parent = node;
            id_node->m_type = AST_NODE_TYPE_ATOM_ID;

            const Token *temp;
            if (self->token_idx >= self->tokens.m_size || (temp = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_COLON)
                return syntax_error("<%s> must be followed by <:>", tok->m_line_number, str_base_data_const(&tok->m_id));
            tok = temp;

            parse_result = parser_state_parse_and_add_ast_sub_node_type(self, node, &node_sub_nodes, false);
            if (parse_result.error != PARSE_ERROR_NONE)
                return parse_result;

            tok = &self->tokens.m_data[self->tokens.m_size - 1];
            if (self->token_idx >= self->tokens.m_size || (tok = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_EQUALS1)
                return syntax_error("Type must be followed by <=>", tok->m_line_number);

            parse_result = parser_state_parse_and_add_ast_sub_node_arithm_expr(self, node, &node_sub_nodes, 0);
            if (parse_result.error != PARSE_ERROR_NONE)
                return parse_result;

            tok = &self->tokens.m_data[self->tokens.m_size - 1];
            if (self->token_idx >= self->tokens.m_size || (tok = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_SEMICOLON)
                return syntax_error("<let> statement must end with <;>", tok->m_line_number);

            node->m_sub_nodes = (AST_node_ptr_slice){.m_size = node_sub_nodes.m_size, .m_data = node_sub_nodes.m_data};
            node->m_type = AST_NODE_TYPE_DECL_VAR;
            break;
        }

        case TOKEN_TYPE_IF:
        case TOKEN_TYPE_WHILE:
        if_while_case:{
            if (self->token_idx >= self->tokens.m_size || self->tokens.m_data[self->token_idx++].m_type != TOKEN_TYPE_LPAREN)
                return syntax_error("<%s> must be followed by <(>", tok->m_line_number, str_base_data_const(&tok->m_id));

            if (tok->m_type == TOKEN_TYPE_WHILE){
                if (loop_label_tok_ptr){
                    AST_node *loop_label_id_node = parser_state_ast_node_alloc(self, loop_label_tok_ptr);
                    if (!loop_label_id_node || !vec_base_push_back(&node_sub_nodes, self->alloc, &loop_label_id_node))
                        return OOM_ERROR;
                    loop_label_id_node->m_parent = node;
                    loop_label_id_node->m_type = AST_NODE_TYPE_ATOM_ID;
                }
                else if (!vec_base_push_back(&node_sub_nodes, self->alloc, &(AST_node*){NULL}))
                    return OOM_ERROR;
            }

            parse_result = parser_state_parse_and_add_ast_sub_node_arithm_expr(self, node, &node_sub_nodes, 0);
            if (parse_result.error != PARSE_ERROR_NONE)
                return parse_result;

            const Token *temp = &self->tokens.m_data[self->token_idx++];
            if (temp->m_type != TOKEN_TYPE_RPAREN)
                return syntax_error("<%s> statement's conditional expression must be closed by <)>", temp->m_line_number, str_base_data_const(&tok->m_id));

            if (self->token_idx >= self->tokens.m_size){
                if (tok->m_type == TOKEN_TYPE_WHILE)
                    return syntax_error("<while> statement is missing body or continue expression", temp->m_line_number);
                return syntax_error("<if> statement is missing body", temp->m_line_number);
            }

            if (tok->m_type == TOKEN_TYPE_WHILE){
                temp = &self->tokens.m_data[self->token_idx];
                if (temp->m_type == TOKEN_TYPE_COLON){
                    if (++self->token_idx >= self->tokens.m_size || (temp = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_LPAREN)
                        return syntax_error("<:> must be followed by <(> in continue expression", temp->m_line_number);

                    parse_result = (self->tokens.m_data[self->token_idx].m_type == TOKEN_TYPE_LBRACE) ? parser_state_parse(self): parser_state_parse_arithm_expr(self, 0);
                    if (parse_result.error != PARSE_ERROR_NONE)
                        return parse_result;
                    if (!vec_base_push_back(&node_sub_nodes, self->alloc, &parse_result.ast_node_ptr))
                        return OOM_ERROR;
                    parse_result.ast_node_ptr->m_parent = node;

                    temp = &self->tokens.m_data[self->token_idx++];
                    if (temp->m_type != TOKEN_TYPE_RPAREN)
                        return syntax_error("<while> loop's continue expression must be closed by <)>", temp->m_line_number);
                    if (self->token_idx >= self->tokens.m_size)
                        return syntax_error("<while> statement is missing body", temp->m_line_number);
                }
                else if (!vec_base_push_back(&node_sub_nodes, self->alloc, &(AST_node*){NULL}))
                    return OOM_ERROR;
            }

            parse_result = parser_state_parse_and_add_ast_node_body(self, node, &node_sub_nodes);
            if (parse_result.error != PARSE_ERROR_NONE)
                return parse_result;

            if (tok->m_type != TOKEN_TYPE_WHILE){
                if (self->token_idx < self->tokens.m_size && (temp = &self->tokens.m_data[self->token_idx])->m_type == TOKEN_TYPE_ELSE){
                    if (++self->token_idx >= self->tokens.m_size)
                        return syntax_error("<else> statement is missing body", temp->m_line_number);
                    parse_result = parser_state_parse_and_add_ast_node_body(self, node, &node_sub_nodes);
                    if (parse_result.error != PARSE_ERROR_NONE)
                        return parse_result;
                }
                else if (!vec_base_push_back(&node_sub_nodes, self->alloc, &(AST_node*){NULL}))
                    return OOM_ERROR;
            }

            node->m_sub_nodes = (AST_node_ptr_slice){.m_size = node_sub_nodes.m_size, .m_data = node_sub_nodes.m_data};
            node->m_type = (tok->m_type == TOKEN_TYPE_IF) ? AST_NODE_TYPE_STATEMENT_IF : AST_NODE_TYPE_STATEMENT_WHILE;
            break;
        }

        case TOKEN_TYPE_FOR:
        for_case:{
            if (self->token_idx >= self->tokens.m_size || self->tokens.m_data[self->token_idx++].m_type != TOKEN_TYPE_LPAREN)
                return syntax_error("<for> must be followed by <(>", tok->m_line_number);

            usize for_start_expr_start_pos = self->token_idx;

            Parser_state_parse_result for_start_expr_node = parser_state_parse_arithm_expr(self, 0);
            if (for_start_expr_node.error != PARSE_ERROR_NONE)
                return for_start_expr_node;

            tok = &self->tokens.m_data[self->token_idx];
            if (tok->m_type != TOKEN_TYPE_DOT2)
                return syntax_error("<for> statement's start expression must be followed by <..>", tok->m_line_number);

            usize dot2_pos = self->token_idx++;

            Parser_state_parse_result for_end_expr_node = parser_state_parse_arithm_expr(self, 0);
            if (for_end_expr_node.error != PARSE_ERROR_NONE)
                return for_end_expr_node;

            tok = &self->tokens.m_data[self->token_idx];
            if (tok->m_type != TOKEN_TYPE_RPAREN)
                return syntax_error("<for> statement's end expression must be followed by <)>", tok->m_line_number);
            
            usize for_end_expr_end_pos = self->token_idx++;

            const Token *for_capture_tok = tok;

            if (
                self->token_idx >= self->tokens.m_size || (tok = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_PIPE ||
                self->token_idx >= self->tokens.m_size || (tok = for_capture_tok = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_ID ||
                self->token_idx >= self->tokens.m_size || (tok = &self->tokens.m_data[self->token_idx++])->m_type != TOKEN_TYPE_PIPE
            )
                return syntax_error("<for> statement's range expression must be followed by a capture expression <|identifier|>", tok->m_line_number);

            if (self->token_idx >= self->tokens.m_size)
                return syntax_error("<for> statement is missing body", tok->m_line_number);

            usize for_body_start_pos = self->token_idx;

            bool for_body_empty = (self->tokens.m_data[for_body_start_pos].m_type == TOKEN_TYPE_SEMICOLON);
            if (!for_body_empty && (parse_result = parser_state_parse(self)).error != PARSE_ERROR_NONE)
                return parse_result;

            self->token_idx += for_body_empty;

            const char *capture_id = str_base_data_const(&for_capture_tok->m_id);

            Str_base_result start_var_id = str_base_init_fmt(self->alloc, "$%s_start", capture_id);
            if (!start_var_id.success)
                return OOM_ERROR;
            char *start_var = str_base_data(&start_var_id.result);

            Str_base_result end_var_id = str_base_init_fmt(self->alloc, "$%s_end", capture_id);
            if (!end_var_id.success)
                return OOM_ERROR;
            char *end_var = str_base_data(&end_var_id.result);

            usize for_capture_node_line_number = for_capture_tok->m_line_number;

            Vec_base for_to_while_tokens = vec_base_init(Token);
            #define for_to_while_push_back(id, token_type) \
                do{ \
                    if (!parser_state_for_to_while_tokens_push_back(self, &for_to_while_tokens, (id), (token_type), for_capture_node_line_number)) \
                        return OOM_ERROR; \
                } while (0)

            for_to_while_push_back("{", TOKEN_TYPE_LBRACE);
            for_to_while_push_back("let", TOKEN_TYPE_LET);
            for_to_while_push_back(start_var, TOKEN_TYPE_ID);
            for_to_while_push_back(":", TOKEN_TYPE_COLON);
            for_to_while_push_back("int", TOKEN_TYPE_INT);
            for_to_while_push_back("=", TOKEN_TYPE_EQUALS1);
            for (usize i = for_start_expr_start_pos; i < dot2_pos; ++i)
                if (!vec_base_push_back(&for_to_while_tokens, self->alloc, &self->tokens.m_data[i]))
                    return OOM_ERROR;
            for_to_while_push_back(";", TOKEN_TYPE_SEMICOLON);
            for_to_while_push_back("let", TOKEN_TYPE_LET);
            for_to_while_push_back(end_var, TOKEN_TYPE_ID);
            for_to_while_push_back(":", TOKEN_TYPE_COLON);
            for_to_while_push_back("int", TOKEN_TYPE_INT);
            for_to_while_push_back("=", TOKEN_TYPE_EQUALS1);
            for (usize i = dot2_pos + 1; i < for_end_expr_end_pos; ++i)
                if (!vec_base_push_back(&for_to_while_tokens, self->alloc, &self->tokens.m_data[i]))
                    return OOM_ERROR;
            for_to_while_push_back(";", TOKEN_TYPE_SEMICOLON);
            if (loop_label_tok_ptr){
                for_to_while_push_back(str_base_data_const(&loop_label_tok_ptr->m_id), TOKEN_TYPE_ID);
                for_to_while_push_back(":", TOKEN_TYPE_COLON);
            }
            for_to_while_push_back("while", TOKEN_TYPE_WHILE);
            for_to_while_push_back("(", TOKEN_TYPE_LPAREN);
            for_to_while_push_back(start_var, TOKEN_TYPE_ID);
            for_to_while_push_back("<", TOKEN_TYPE_LESS_THAN1);
            for_to_while_push_back(end_var, TOKEN_TYPE_ID);
            for_to_while_push_back(")", TOKEN_TYPE_RPAREN);
            for_to_while_push_back("{", TOKEN_TYPE_LBRACE);
            for_to_while_push_back("let", TOKEN_TYPE_LET);
            for_to_while_push_back(capture_id, TOKEN_TYPE_ID);
            for_to_while_push_back(":", TOKEN_TYPE_COLON);
            for_to_while_push_back("int", TOKEN_TYPE_INT);
            for_to_while_push_back("=", TOKEN_TYPE_EQUALS1);
            for_to_while_push_back(start_var, TOKEN_TYPE_ID);
            for_to_while_push_back(";", TOKEN_TYPE_SEMICOLON);
            for_to_while_push_back(start_var, TOKEN_TYPE_ID);
            for_to_while_push_back("=", TOKEN_TYPE_EQUALS1);
            for_to_while_push_back(start_var, TOKEN_TYPE_ID);
            for_to_while_push_back("+", TOKEN_TYPE_PLUS);
            for_to_while_push_back("1", TOKEN_TYPE_INT_LIT);
            for_to_while_push_back(";", TOKEN_TYPE_SEMICOLON);
            if (!for_body_empty)
                for (usize i = for_body_start_pos; i < self->token_idx; ++i)
                    if (!vec_base_push_back(&for_to_while_tokens, self->alloc, &self->tokens.m_data[i]))
                        return OOM_ERROR;
            for_to_while_push_back("}", TOKEN_TYPE_RBRACE);
            for_to_while_push_back("}", TOKEN_TYPE_RBRACE);

            usize current_token_idx = self->token_idx;
            Token_slice current_token_slice = self->tokens;

            self->token_idx = 0;
            self->tokens = (Token_slice){.m_size = for_to_while_tokens.m_size, .m_data = for_to_while_tokens.m_data};

            parse_result = parser_state_parse(self);
            if (parse_result.error != PARSE_ERROR_NONE)
                return parse_result;

            self->token_idx = current_token_idx;
            self->tokens = current_token_slice;

            node = parse_result.ast_node_ptr;
            break;
        }

        case TOKEN_TYPE_BREAK:
        case TOKEN_TYPE_CONTINUE:{
            if (self->token_idx >= self->tokens.m_size)
                return syntax_error("No tokens are available", tok->m_line_number);

            const Token *temp = &self->tokens.m_data[self->token_idx++];
            if (temp->m_type != TOKEN_TYPE_SEMICOLON){
                if (temp->m_type != TOKEN_TYPE_ID)
                    return syntax_error("<%s> must be followed by an identifier", tok->m_line_number, str_base_data_const(&tok->m_id));
                if (self->token_idx >= self->tokens.m_size || self->tokens.m_data[self->token_idx++].m_type != TOKEN_TYPE_SEMICOLON)
                    return syntax_error("<%s> must be followed by <;>", temp->m_line_number, str_base_data_const(&temp->m_id));

                AST_node *break_continue_id_node = parser_state_ast_node_alloc(self, temp);
                if (!break_continue_id_node || !vec_base_push_back(&node_sub_nodes, self->alloc, &break_continue_id_node))
                    return OOM_ERROR;
                break_continue_id_node->m_parent = node;
                break_continue_id_node->m_type = AST_NODE_TYPE_ATOM_ID;
            }

            node->m_sub_nodes = (AST_node_ptr_slice){.m_size = node_sub_nodes.m_size, .m_data = node_sub_nodes.m_data};
            node->m_type = (tok->m_type == TOKEN_TYPE_BREAK) ? AST_NODE_TYPE_STATEMENT_BREAK : AST_NODE_TYPE_STATEMENT_CONTINUE;
            break;
        }

        case TOKEN_TYPE_RETURN:
            if (self->token_idx >= self->tokens.m_size)
                return syntax_error("<return> must be followed by a <;> or an arithmetic expression", tok->m_line_number);

            if (
                self->tokens.m_data[self->token_idx].m_type != TOKEN_TYPE_SEMICOLON &&
                (parse_result = parser_state_parse_and_add_ast_sub_node_arithm_expr(self, node, &node_sub_nodes, 0)).error != PARSE_ERROR_NONE
            )
                return parse_result;

            if (self->token_idx >= self->tokens.m_size || self->tokens.m_data[self->token_idx++].m_type != TOKEN_TYPE_SEMICOLON)
                return syntax_error("<return> statement must end with <;>", tok->m_line_number);

            node->m_sub_nodes = (AST_node_ptr_slice){.m_size = node_sub_nodes.m_size, .m_data = node_sub_nodes.m_data};
            node->m_type = AST_NODE_TYPE_STATEMENT_RETURN;
            break;

        default:
            return syntax_error("Found unknown or contextually invalid token <%s>", tok->m_line_number, str_base_data_const(&tok->m_id));
    }

    return (Parser_state_parse_result){.ast_node_ptr = node, .error = PARSE_ERROR_NONE};
}

static const char* ast_node_type_to_str(enum AST_node_type ast_node_type){
    #define generate_case(ast_node_type_id) case ast_node_type_id: return #ast_node_type_id;
    switch (ast_node_type){
        generate_case(AST_NODE_TYPE_ATOM_ID)
        generate_case(AST_NODE_TYPE_ATOM_ARGV)
        generate_case(AST_NODE_TYPE_ATOM_FALSE)
        generate_case(AST_NODE_TYPE_ATOM_TRUE)
        generate_case(AST_NODE_TYPE_ATOM_CHAR_LIT)
        generate_case(AST_NODE_TYPE_ATOM_INT_LIT)
        generate_case(AST_NODE_TYPE_ATOM_FLOAT_LIT)
        generate_case(AST_NODE_TYPE_ATOM_STR_LIT)
        generate_case(AST_NODE_TYPE_ATOM_INIT_LIST)
        generate_case(AST_NODE_TYPE_TYPE_VOID)
        generate_case(AST_NODE_TYPE_TYPE_BOOL)
        generate_case(AST_NODE_TYPE_TYPE_CHAR)
        generate_case(AST_NODE_TYPE_TYPE_INT)
        generate_case(AST_NODE_TYPE_TYPE_FLOAT)
        generate_case(AST_NODE_TYPE_TYPE_STR)
        generate_case(AST_NODE_TYPE_TYPE_LIST)
        generate_case(AST_NODE_TYPE_UNARY_OP_PLUS)
        generate_case(AST_NODE_TYPE_UNARY_OP_MINUS)
        generate_case(AST_NODE_TYPE_UNARY_OP_BNEG)
        generate_case(AST_NODE_TYPE_UNARY_OP_NOT)
        generate_case(AST_NODE_TYPE_BINARY_OP_SUBSCRIPT)
        generate_case(AST_NODE_TYPE_BINARY_OP_POW)
        generate_case(AST_NODE_TYPE_BINARY_OP_AS)
        generate_case(AST_NODE_TYPE_BINARY_OP_MUL)
        generate_case(AST_NODE_TYPE_BINARY_OP_DIV)
        generate_case(AST_NODE_TYPE_BINARY_OP_REM)
        generate_case(AST_NODE_TYPE_BINARY_OP_ADD)
        generate_case(AST_NODE_TYPE_BINARY_OP_SUB)
        generate_case(AST_NODE_TYPE_BINARY_OP_SHL)
        generate_case(AST_NODE_TYPE_BINARY_OP_SHR)
        generate_case(AST_NODE_TYPE_BINARY_OP_CMP_LE)
        generate_case(AST_NODE_TYPE_BINARY_OP_CMP_LEQ)
        generate_case(AST_NODE_TYPE_BINARY_OP_CMP_GE)
        generate_case(AST_NODE_TYPE_BINARY_OP_CMP_GEQ)
        generate_case(AST_NODE_TYPE_BINARY_OP_CMP_EQ)
        generate_case(AST_NODE_TYPE_BINARY_OP_CMP_NEQ)
        generate_case(AST_NODE_TYPE_BINARY_OP_BAND)
        generate_case(AST_NODE_TYPE_BINARY_OP_XOR)
        generate_case(AST_NODE_TYPE_BINARY_OP_BOR)
        generate_case(AST_NODE_TYPE_BINARY_OP_AND)
        generate_case(AST_NODE_TYPE_BINARY_OP_OR)
        generate_case(AST_NODE_TYPE_BINARY_OP_ASSIGN)
        generate_case(AST_NODE_TYPE_FN_CALL)
        generate_case(AST_NODE_TYPE_DECL_FN)
        generate_case(AST_NODE_TYPE_DECL_VAR)
        generate_case(AST_NODE_TYPE_STATEMENT_BLOCK)
        generate_case(AST_NODE_TYPE_STATEMENT_IF)
        generate_case(AST_NODE_TYPE_STATEMENT_WHILE)
        generate_case(AST_NODE_TYPE_STATEMENT_BREAK)
        generate_case(AST_NODE_TYPE_STATEMENT_CONTINUE)
        generate_case(AST_NODE_TYPE_STATEMENT_RETURN)
    }
    unreachable();
}

static i64 ast_node_print(const AST_node *self, FILE *file, int indent){
    i64 chars_written = 0, temp;

    temp = fprintf(file, "%*s", indent, "");
    if (temp < 0)
        return temp;
    chars_written += temp;

    if (self){
        const char *ast_node_type_str = ast_node_type_to_str(self->m_type);

        temp = fprintf(
            file,
            "<%s>\n"
            "%*s<id>%s</id>\n"
            "%*s<line_number>" USIZE_PFMT "</line_number>\n",
            ast_node_type_str,
            indent + 2, "", str_base_data_const(&self->m_token->m_id),
            indent + 2, "", self->m_token->m_line_number
        );
        if (temp < 0)
            return temp;
        chars_written += temp;

        if (self->m_sub_nodes.m_size > 0){
            temp = fprintf(file, "%*s<sub_nodes>\n", indent + 2, "");
            if (temp < 0)
                return temp;
            chars_written += temp;

            for (usize i = 0; i < self->m_sub_nodes.m_size; ++i){
                temp = ast_node_print(self->m_sub_nodes.m_data[i], file, indent + 4);
                if (temp < 0)
                    return temp;
            }

            temp = fprintf(file, "%*s</sub_nodes>\n", indent + 2, "");
            if (temp < 0)
                return temp;
            chars_written += temp;
        }

        temp = fprintf(file, "%*s</%s>\n", indent, "", ast_node_type_str);
        if (temp < 0)
            return temp;
        chars_written += temp;
    }
    else{
        temp = fprintf(file, "null\n");
        if (temp < 0)
            return temp;
        chars_written += temp;
    }
    
    return chars_written;
}

#ifndef NDEBUG
static bool token_slice_is_valid(Token_slice tokens){
    usize lparen_count   = 0, rparen_count   = 0;
    usize lbracket_count = 0, rbracket_count = 0;
    usize lbrace_count   = 0, rbrace_count   = 0;

    for (usize i = 0; i < tokens.m_size; ++i){
        enum Token_type token_type = tokens.m_data[i].m_type;
        lparen_count   += (token_type == TOKEN_TYPE_LPAREN  );
        rparen_count   += (token_type == TOKEN_TYPE_RPAREN  );
        lbracket_count += (token_type == TOKEN_TYPE_LBRACKET);
        rbracket_count += (token_type == TOKEN_TYPE_RBRACKET);
        lbrace_count   += (token_type == TOKEN_TYPE_LBRACE  );
        rbrace_count   += (token_type == TOKEN_TYPE_RBRACE  );
    }

    return lparen_count == rparen_count && lbracket_count == rbracket_count && lbrace_count == rbrace_count;
}
#endif // NDEBUG

// ------------------------------------------------------------------------------------------------

i64 ast_node_ptr_slice_print(AST_node_ptr_slice ast_node_ptr_slice, FILE *file){
    assert(file && "<file> is not nullable");

    i64 result = 0;

    for (usize i = 0; i < ast_node_ptr_slice.m_size; ++i){
        i64 temp = ast_node_print(ast_node_ptr_slice.m_data[i], file, 0);
        if (temp < 0)
            return temp;
        result += temp;
    }

    return result;
}

Parse_result parse(Arena *arena, Token_slice tokens){
    assert(arena && "<arena> is not nullable");
    assert(token_slice_is_valid(tokens));

    Parser_state state = {
        .alloc         = arena_allocator(arena),
        .tokens        = tokens,
        .token_idx     = 0,
        .ast_node_ptrs = vec_base_init(AST_node*)
    };

    while (state.token_idx < state.tokens.m_size){
        const Token *tok = &state.tokens.m_data[state.token_idx];
        if (tok->m_type != TOKEN_TYPE_SEMICOLON){
            Parser_state_parse_result ast_node = parser_state_parse(&state);
            if (ast_node.error != PARSE_ERROR_NONE)
                return (Parse_result){.error_info = ast_node.error_info, .error = ast_node.error};
            if (!vec_base_push_back(&state.ast_node_ptrs, state.alloc, &ast_node.ast_node_ptr))
                return (Parse_result){.error = PARSE_ERROR_OOM};
        }
        else
            ++state.token_idx;
    }

    return (Parse_result){.ast_nodes = {.m_size = state.ast_node_ptrs.m_size, .m_data = state.ast_node_ptrs.m_data}, .error = PARSE_ERROR_NONE};
}
