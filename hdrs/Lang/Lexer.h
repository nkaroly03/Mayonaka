#ifndef LANG_LEXER_H
#define LANG_LEXER_H

#ifdef __cplusplus
extern "C"{
#endif

#include "../Allocator/Arena.h"
#include "../Data_structure/Str_base.h"
#include "../Utils/Num.h"

enum Token_type{
    TOKEN_TYPE_ID,
    TOKEN_TYPE_ARGV,
    TOKEN_TYPE_FALSE,
    TOKEN_TYPE_TRUE,
    TOKEN_TYPE_CHAR_LIT,
    TOKEN_TYPE_INT_LIT,
    TOKEN_TYPE_FLOAT_LIT,
    TOKEN_TYPE_STR_LIT,
    TOKEN_TYPE_LIST_LIT,

    TOKEN_TYPE_COMMA,
    TOKEN_TYPE_COLON,
    TOKEN_TYPE_SEMICOLON,
    TOKEN_TYPE_LPAREN,
    TOKEN_TYPE_RPAREN,
    TOKEN_TYPE_LBRACKET,
    TOKEN_TYPE_RBRACKET,
    TOKEN_TYPE_LBRACE,
    TOKEN_TYPE_RBRACE,

    TOKEN_TYPE_DOT2,
    TOKEN_TYPE_EQUALS2,
    TOKEN_TYPE_NOT_EQUALS,
    TOKEN_TYPE_LESS_THAN1,
    TOKEN_TYPE_LESS_THAN1_EQUALS,
    TOKEN_TYPE_GREATER_THAN1,
    TOKEN_TYPE_GREATER_THAN1_EQUALS,
    TOKEN_TYPE_PLUS,
    TOKEN_TYPE_MINUS,
    TOKEN_TYPE_ASTERISK1,
    TOKEN_TYPE_ASTERISK2,
    TOKEN_TYPE_SLASH,
    TOKEN_TYPE_PERCENT,
    TOKEN_TYPE_LESS_THAN2,
    TOKEN_TYPE_GREATER_THAN2,
    TOKEN_TYPE_AMPERSAND,
    TOKEN_TYPE_PIPE,
    TOKEN_TYPE_CARET,
    TOKEN_TYPE_TILDE,
    TOKEN_TYPE_AND,
    TOKEN_TYPE_OR,
    TOKEN_TYPE_NOT,
    TOKEN_TYPE_EQUALS1,
    
    TOKEN_TYPE_FN,
    TOKEN_TYPE_LET,

    TOKEN_TYPE_BOOL,
    TOKEN_TYPE_CHAR,
    TOKEN_TYPE_INT,
    TOKEN_TYPE_FLOAT,
    TOKEN_TYPE_STR,

    // TOKEN_TYPE_PRINT,
    // TOKEN_TYPE_SCAN,
    // 
    // TOKEN_TYPE_ARRAY_SIZE,
    // TOKEN_TYPE_RAND,
    // TOKEN_TYPE_POLL_CHAR,

    TOKEN_TYPE_IF,
    TOKEN_TYPE_ELSE,

    TOKEN_TYPE_WHILE,
    TOKEN_TYPE_FOR,

    TOKEN_TYPE_RETURN
};

typedef struct Token{
    enum Token_type m_type;
    Str_base m_id;
    usize m_line_number;
} Token;

typedef struct Token_slice{
    usize m_size;
    const Token *m_data;
} Token_slice;

void token_slice_print(Token_slice tokens_slice);

enum Lex_error{
    LEX_ERROR_NONE,
    LEX_ERROR_OOM,
    LEX_ERROR_SYNTAX,
    LEX_ERROR_FILE
};

typedef struct Lex_result{
    union{
        Token_slice tokens;
        Str_base error_info;
    };
    enum Lex_error error;
} Lex_result;

Lex_result lex(Arena *arena, const char *path);

#ifdef __cplusplus
}
#endif

#endif // LANG_LEXER_H
